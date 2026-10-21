/*
 * This file is part of PEAT (Performance Estimation & Analysing Toolkit).
 * Copyright (C) 2026 by Alex Knyazev
 *
 * PEAT is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * PEAT is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with PEAT.  If not, see <https://www.gnu.org/licenses/>.
 */

#include <QApplication>
#include <QDir>
#include <QFile>
#include <QPainter>
#include <QtDebug>
#include <QtGlobal>

#include <QHeaderView>
#include <QProgressDialog>
#include <algorithm>
#include <sstream>
#include <stdexcept>

#include "../common/common_types.h"
#include "../common/model_id.h"
#include "elements.h"
#include "globals.h"
#ifdef _OPENMP
#include <omp.h>
#endif

#define heightPercent 1.0
#define showElementsThreshold 1 // 0.3

// P2P and request functions called by the MPI library itself (PEAT_PROP_MPI_INTERNAL) have their own filter flags
static bool mpi_func_shown(const TV_TraceParts &tp, const TV_Event *e, TV_MPI_Func_Group user_group)
{
    return tp.mpi_functions[user_group + global_func_id.check_property(e->func_id, PEAT_PROP_MPI_INTERNAL)];
}
#define MAX_SIZE 100
Elements::Elements(QFileInfo &name, TV_TraceParts *swn) : trace_info_file(name), shown(swn), ptp(NULL), model_id(NULL), user_regions(NULL)
#ifdef _OPENMP
                                                          ,
                                                          locks(NULL)
#endif
{
    model_time_stop = 0;
    model_time_start = LLONG_MAX;
    zoom_time_start = 0.0f;
    zoom_time_stop = 0.0f;
    no_draw_elems = 0;
    candidates_it = candidates.end();
    current_candidate = 0;
#ifdef _OPENMP
    trace_line_MPIs = new String_Split[omp_get_max_threads() + 1];
    trace_line_HWCs = new String_Split[omp_get_max_threads() + 1];
    trace_line_binds = new String_Split[omp_get_max_threads() + 1];
    locks = NULL;
#endif
}

Elements::~Elements()
{
    clear();
    if (user_regions)
        delete user_regions;
    if (ptp)
        delete ptp;
    if (model_id)
        delete model_id;
#ifdef _OPENMP
    if (locks)
        delete[] locks;
    delete[] trace_line_MPIs;
    delete[] trace_line_HWCs;
    delete[] trace_line_binds;
#endif
}
bool Elements::find_collective(Collective &entry)
{
    return false;
}

bool Elements::get_message(const PEAT_Rank rank, File_Storage_GZ *input_bind, TV_TraceParts &tp)
{
#ifdef _OPENMP
    String_Split &trace_line_bind = trace_line_binds[omp_get_thread_num()];
#endif
    trace_line_bind.clear();
    PEAT_Bool isReady = input_bind->read_lines(trace_line_bind, rank, ':', ';');
    if (isReady) {
        int line_size = trace_line_bind.size();
        if (line_size > 3) {
            PEAT_Record_Type type = PEAT_RT_Remark;
            extract(type, trace_line_bind.at(0));
            switch (type) {
            case PEAT_RT_Bind_Collective: {
                PEAT_MPI_Func_ID func_id = TMPI_FUNC_ID_NONE;
                extract(func_id, trace_line_bind.at(1), 10, false);
                if (func_id == 0) {
                    func_id = global_func_id.idx_by_name(trace_line_bind.at(1));
                }
                Elements::Collective tmp;
                tmp.func_id = func_id;
                PEAT_Rank rank_num = 0;
                /*  Following 2 events will be holding conditionaly sender and receiver parts of bytes in case they are different */
                for (int i = 2; i < line_size;) { // rank:ids pairs, ids are the trace's id columns
                    PEAT_Rank rank = -1;
                    extract(rank, trace_line_bind.at(i));
                    rank_num++;
                    PEAT_Event_IDs coll_ids;
                    i = bind_ids(trace_line_bind, i + 1, rank, coll_ids);
                    TV_Coll_Event *evt_tmp = static_cast<TV_Coll_Event *>(find_event(rank, coll_ids));

                    if (evt_tmp) {
                        if (evt_tmp->get_event_type() == TV_EVENT_TYPE_COLLECTIVE) { // ignore COMM collective for now
                            TV_Collective_Event *evt = static_cast<TV_Collective_Event *>(evt_tmp);
                            TV_Collective_Event *master_evt1 = NULL;
                            TV_Collective_Event *master_evt2 = NULL;
                            tmp.events[rank] = evt;
                            if (master_evt1) {
                                if (master_evt1->bytes == evt->bytes) {
                                    evt->msg_bytes = &master_evt1->bytes;
                                    evt->bytes.clear();
                                } else {
                                    if (master_evt2) {
                                        if (master_evt1->bytes == evt->bytes) {
                                            evt->msg_bytes = &master_evt2->bytes;
                                            evt->bytes.clear();
                                        }
                                    } else {
                                        master_evt2 = evt;
                                    }
                                }
                            } else {
                                master_evt1 = evt;
                            }
                        }
                    }
                }
                size_t index = -1;
                if (tmp.events.size() > 1) {
                    if (!find_collective(tmp)) {
                        coll[rank].push_back(tmp);
                        index = coll[rank].size() - 1;
                    }
                    for (std::map<PEAT_Rank, TV_Event *>::iterator to_it = tmp.events.begin(), from_it = to_it++; to_it != tmp.events.end(); ++to_it, ++from_it) {
                        add_message(from_it->second, to_it->second, TV_PEN_MESSAGE_TYPE_COLLECTIVE, tp, rank, index);
                    }
                }
            } break;
            case PEAT_RT_Bind_Peer: { // func_A:rank_A:idsA:func_B:rank_B:idsB:size - both sides at phase[1] (the post); see P2PAL::print_P2PF
                PEAT_Rank from_rank = -1;
                extract(from_rank, trace_line_bind.at(2));
                PEAT_Event_IDs from_ids;
                int pos = bind_ids(trace_line_bind, 3, from_rank, from_ids);
                PEAT_Rank to_rank = -1;
                extract(to_rank, trace_line_bind.at(pos + 1));
                PEAT_Event_IDs to_ids;
                pos = bind_ids(trace_line_bind, pos + 2, to_rank, to_ids);
                // draw to the receiver's completion (Wait/Test/Request_Release/...), not its post, when one was
                // recorded: mat writes to_rank's completion line for its own post before this line, in the same
                // file, whenever this Bind_Peer line is itself in to_rank's file (mat's default: written to the
                // receiver's file) - so it's already in bind_completion_map by the time we get here.
                if (to_rank == rank) {
                    std::map<PEAT_Event_IDs, PEAT_Event_IDs>::const_iterator comp_it = bind_completion_map[rank].find(to_ids);
                    if (comp_it != bind_completion_map[rank].end())
                        to_ids = comp_it->second;
                }
                TV_Event *from = find_event(from_rank, from_ids);
                TV_Event *to = find_event(to_rank, to_ids);
                if (from && to) {
                    add_message(from, to, TV_PEN_MESSAGE_TYPE_NORMAL, tp);
                }
            } break;
            case PEAT_RT_Bind_Completion: { // type:func:ids:func:ids (both records of the file's rank)
                PEAT_Rank to_rank = rank;
                PEAT_Rank from_rank = rank;
                PEAT_Event_IDs from_ids, to_ids;
                int pos = bind_ids(trace_line_bind, 2, from_rank, from_ids);
                bind_ids(trace_line_bind, pos + 1, to_rank, to_ids);
                bind_completion_map[rank][from_ids] = to_ids; // post -> completion, for PEAT_RT_Bind_Peer to chase
                TV_Event *from = find_event(from_rank, from_ids);
                TV_Event *to = find_event(to_rank, to_ids);
                if (from && to) {
                    switch (from->func_id) {
                    case ZECOMMANDLISTAPPENDMEMORYCOPY_ID:
                    case ZECOMMANDLISTAPPENDMEMORYFILL_ID:
                    case ZECOMMANDLISTAPPENDMEMORYCOPYREGION_ID:
                    case ZECOMMANDLISTAPPENDMEMORYCOPYFROMCONTEXT_ID:
                    case ZECOMMANDLISTAPPENDIMAGECOPY_ID:
                    case ZECOMMANDLISTAPPENDIMAGECOPYREGION_ID:
                    case ZECOMMANDLISTAPPENDIMAGECOPYTOMEMORY_ID:
                    case ZECOMMANDLISTAPPENDIMAGECOPYFROMMEMORY_ID:
                    case ZECOMMANDLISTAPPENDMEMORYPREFETCH_ID:
                    case ZECOMMANDLISTAPPENDMEMADVISE_ID:
                        add_message(from, to, TV_PEN_MESSAGE_TYPE_GPU_HD_TRANSFER, tp);
                        break;
                    case ZECOMMANDLISTAPPENDLAUNCHKERNEL_ID:
                        add_message(from, to, TV_PEN_MESSAGE_TYPE_GPU_EXEC, tp);
                        break;

                    default:
                        add_message(from, to, TV_PEN_MESSAGE_TYPE_COMPLETION, tp);
                        break;
                    }
                }
            } break;
            case PEAT_RT_Bind_Initiation: { // type:func:ids:func:ids
                PEAT_Rank to_rank = rank;
                PEAT_Rank from_rank = rank;
                PEAT_Event_IDs from_ids, to_ids;
                int pos = bind_ids(trace_line_bind, 2, from_rank, from_ids);
                bind_ids(trace_line_bind, pos + 1, to_rank, to_ids);
                TV_ToR_Event *from = static_cast<TV_ToR_Event *>(find_event(from_rank, from_ids));
                TV_Start_Event *to = static_cast<TV_Start_Event *>(find_event(to_rank, to_ids));
                if (from && to) {
                    to->t_events.push_back(from);
                    add_message(from, to, TV_PEN_MESSAGE_TYPE_INITIATION, tp);
                }
            } break;
            default:
                break;
            }
        }
        return true;
    } else {
        return false;
    }
}

void Elements::compact_events(int nthreads)
{
#pragma omp parallel for schedule(dynamic) num_threads(nthreads)
    for (PEAT_Rank rank = 0; rank < eventsV.size(); ++rank) {
        //    for (std::vector<std::vector<TV_Event*>>::iterator rank_it = eventsV.begin(); rank_it != eventsV.end(); ++rank_it, ++r) {
        PEAT_UID index = 0;
        for (std::vector<TV_Event *>::iterator event_it = eventsV.at(rank).begin(); event_it != eventsV.at(rank).end(); ++event_it) {
            TV_Event *evt = *event_it;
            if (evt) {
                eventsV.at(rank)[index] = evt;
#ifdef QTV_TR_VECTOR
                events_tr.at(rank)[evt->ids[0]] = index;
#else
                events_tr.at(rank)[evt->ids[0]] = evt;
#endif
                ++index;
            }
        }
        eventsV.at(rank).resize(index);
    }
}

bool Elements::load_model_data(QProgressBar *pb, QScrollBar *sr,
                               std::map<PEAT_Rank, std::string> &mpi_file_names,
                               std::map<PEAT_Rank, std::string> &hwc_file_names,
                               std::map<PEAT_Rank, std::string> &bind_file_names,
                               const std::set<PEAT_Rank> &ranks_to_load,
                               int tmin, int tmax, int load_threads, TV_TraceParts &tp)
{
    const QString prog_bar_text = pb->format();
    if (!parse_trace_info_file(trace_info_file, mpi_file_names, hwc_file_names, bind_file_names, ranks_to_load)) {
        // TODO add messageBox?;
        return false;
    };

    File_Storage_GZ mpi_files(mpi_file_names, PEAT_FIO_FLAG_READ | PEAT_FIO_FLAG_LAZY | PEAT_FIO_FLAG_ICLOSE);
    File_Storage_GZ hwc_files(hwc_file_names, PEAT_FIO_FLAG_READ | PEAT_FIO_FLAG_LAZY | PEAT_FIO_FLAG_ICLOSE);
    File_Storage_GZ bind_files(bind_file_names, PEAT_FIO_FLAG_READ | PEAT_FIO_FLAG_LAZY | PEAT_FIO_FLAG_ICLOSE);
    ranks_to_load_v.clear();
    ranks_to_load_v_rev.clear();
    ranks_to_load_v_rev.resize(bind_file_names.size());
    max_mpi_tid.clear();
    max_mpi_tid.resize(bind_file_names.size());
    max_omp_tid.clear();
    max_omp_tid.resize(bind_file_names.size());
    max_gpu_tid.clear();
    max_gpu_tid.resize(bind_file_names.size());
    PEAT_Rank r = 0;
    for (std::set<PEAT_Rank>::const_iterator rank_it = ranks_to_load.cbegin(); rank_it != ranks_to_load.cend(); ++rank_it, ++r) {
        ranks_to_load_v.push_back(*rank_it);
        ranks_to_load_v_rev[*rank_it] = r;
    }
    sr->hide();
    pb->setMinimum(0);
    pb->setMaximum(get_draw_rank_count() + get_total_rank_count());
    pb->setValue(0);
    pb->show();
    // pb->repaint();
    QApplication::processEvents();

    String_Split line;
    long long prevclk = 0;
    // start load messages
    psection.resize(bind_file_names.size());
    try {
        // std::string myPath = traceinfo.absolutePath().toStdString() + "/";

        pb->setFormat("Reading mpi & hwc files " + prog_bar_text + " done");
        pb->repaint();
        int rank_completed = 0;
        PEAT_Rank m_r = mpi_file_names.size();
#pragma omp parallel for schedule(dynamic) num_threads(load_threads)
        for (PEAT_Rank r = 0; r < m_r; ++r) {
            // for (std::map<PEAT_Rank, std::string>::const_iterator mpiFileIt = mpiFileNames.cbegin(); mpiFileIt != mpiFileNames.cend(); ++mpiFileIt) {
            PEAT_Bool rc = false;
            PEAT_Clock min_clock = (trace_info.rank_times[ranks_to_load_v[r]].t3 - trace_info.rank_times[ranks_to_load_v[r]].t2) / 100 * tmin + trace_info.rank_times[ranks_to_load_v[r]].t1;
            PEAT_Clock max_clock = (trace_info.rank_times[ranks_to_load_v[r]].t3 - trace_info.rank_times[ranks_to_load_v[r]].t2) / 100 * (tmax + 1) + trace_info.rank_times[ranks_to_load_v[r]].t2;
            PEAT_UID max_id = 0;
            if (tmin <= 0)
                min_clock = LLONG_MIN;
            if (tmax >= 99)
                max_clock = LLONG_MAX;
            do {
                TV_Event *event = NULL;
                rc = get_event(ranks_to_load_v[r], &mpi_files, &hwc_files, min_clock, max_clock, &event, ptp);

#ifdef _OPENMP
                if (omp_get_thread_num() == 0)
#endif
                {
                    if (rank_completed != pb->value())
                        pb->setValue(rank_completed);
                    progress(prevclk, QApplication::processEvents);
                }
                if (event) {
                    if (event->e_time.func.out < min_clock && psection[r].empty()) { // saturation issue
                        delete event;
                        continue;
                    }
                    if (event->e_time.func.in > max_clock && psection[r].empty()) {
                        // cut_events(ranks_to_load_v[r],event->ids[0]);
                        max_id = event->ids[0];
                        delete event;
                        break;
                    }
                    if (!add_event(event, tp)) {
                        delete event; // event rejected
                    } else {
                        max_id = event->ids[0] + 1;
                    }
                }
            } while (rc);
            cut_events(ranks_to_load_v[r], max_id);
            mpi_files.close(r);
            hwc_files.close(r);
#pragma omp atomic
            rank_completed++;
        }
        pb->setFormat("Compacting Events " + prog_bar_text + " done");
        QApplication::processEvents();
        compact_events(load_threads);
        pb->setFormat("Reading bind files " + prog_bar_text + " done");
        QApplication::processEvents();
        m_r = bind_file_names.size();
        bind_completion_map.clear();
        bind_completion_map.resize(m_r); // sized single-threaded: each rank below only ever touches its own index
        if (tp.mpi_messages[0] || tp.mpi_messages[1] || tp.mpi_messages[2] || tp.mpi_messages[3]) {
#pragma omp parallel for schedule(dynamic) num_threads(load_threads)
            for (PEAT_Rank r = 0; r < m_r; ++r) {
                // for (std::map<PEAT_Rank, std::string>::const_iterator bindFileIt = bindFileNames.cbegin(); bindFileIt != bindFileNames.cend(); ++bindFileIt) {
                PEAT_Bool rc = true;
                do {
                    rc = get_message(r, &bind_files, tp);
#ifdef _OPENMP
                    if (omp_get_thread_num() == 0)
#endif
                    {
                        if (rank_completed != pb->value())
                            pb->setValue(rank_completed);
                        progress(prevclk, QApplication::processEvents);
                    }
                } while (rc);
                bind_files.close(r);
#pragma omp atomic
                rank_completed++;
            }
        }
    } catch (const std::runtime_error &e) {
        qDebug() << "Couldn't open message file. Error: " << e.what() << "\n";
    }

    pb->hide();
    pb->setFormat(prog_bar_text); // revert original text
    QApplication::processEvents();
    psection.clear();
    return true;
}

bool Elements::parse_trace_info_file(
    QFileInfo &trace_info_file,
    std::map<PEAT_Rank, std::string> &mpi_file_names,
    std::map<PEAT_Rank, std::string> &hwc_file_names,
    std::map<PEAT_Rank, std::string> &bind_file_names,
    const std::set<PEAT_Rank> &ranks_to_load)
{
    if (!trace_info.parse_trace_info_file(trace_info_file))
        return false;

    QString abs_path = trace_info_file.absolutePath() + "/";
    QString file_tmpl;
    file_tmpl = trace_info.trace_files;

    // get the list of files objects by template
    QDir directory(abs_path);
    mpi_file_names.clear();
    hwc_file_names.clear();
    bind_file_names.clear();
    QFileInfoList all_files_found = directory.entryInfoList(QStringList(file_tmpl + "*"), QDir::Files);
    for (QFileInfoList::iterator it = all_files_found.begin(); it != all_files_found.end(); ++it) {
        QString file = it->fileName();
        // qDebug() << file;
        QStringList file_splitted = file.split('.');
        if (file_splitted.size() < 3) {
            continue; // ignore non-trace files like tractor_t.ee
        }
        PEAT_Rank rank = file_splitted.at(2).toInt();
        QString type = "." + file_splitted.at(1);

#ifndef TV_IGNORE_PROCESSED_OUTPUT
        if (type == trace_info.mpi_files && ranks_to_load.count(rank) == 1) {
            mpi_file_names[rank] = it->absoluteFilePath().toStdString();
        }
        if (type == trace_info.hwc_files && ranks_to_load.count(rank) == 1) {
            hwc_file_names[rank] = it->absoluteFilePath().toStdString();
        }
#endif
        if (type == trace_info.bind_files) { // bind files all to be read
            bind_file_names[rank] = it->absoluteFilePath().toStdString();
        }
    }

    // messagesV.resize(trace_info.messages_in_rank.size());
    //  for ( int i=0; i< trace_info.messages_in_rank.size(); ++i ){
    //     messagesV[i].resize( trace_info.messages_in_rank[i] + trace_info.num_colls);// coll.size() );
    // }
    std::set<PEAT_Rank> rank_set;
    if (user_regions)
        delete user_regions;
    Global_Parameters glob;
    glob.input_path = trace_info.path.toStdString();
    glob.trace_files = trace_info.trace_files.toStdString();
    glob.com_ext = trace_info.com_ext.toStdString();
    glob.app_ext = trace_info.app_ext.toStdString();
    glob.rank_set = rank_set;
    user_regions = new Address_File_Storage(glob, PEAT_FIO_FLAG_READ | PEAT_FIO_FLAG_LAZY | PEAT_FIO_FLAG_ICLOSE, NULL);

    // user_regions = new Address_File_Storage(trace_info.path.toStdString(), "", trace_info.trace_files.toStdString(), trace_info.com_ext.toStdString(), trace_info.app_ext.toStdString(), rank_set, PEAT_FIO_FLAG_READ | PEAT_FIO_FLAG_LAZY | PEAT_FIO_FLAG_ICLOSE,NULL);

    if (ptp) {
        delete ptp;
        ptp = NULL;
    }
    if (model_id) {
        delete model_id;
        model_id = NULL;
    }
    if (mpi_file_names.empty() && hwc_file_names.empty() && !ranks_to_load.empty()) {
        // Either mat was run with --matching-only (no processed .mpi/.hwc files exist in the
        // output directory, only .bind), or TV_IGNORE_PROCESSED_OUTPUT is defined (default) so
        // processed .mpi/.hwc files are never read even when present. Either way, fall back to the
        // original raw trace files and do our own PTP real-time correction (mat always writes .bind
        // + trace.info, but only writes the per-rank timing sync boundaries into trace.info, not
        // per-event corrected timestamps).
        glob.mpi_files = trace_info.mpi_files.toStdString();
        glob.hwc_files = trace_info.hwc_files.toStdString();
        glob.ptpd_ext = ".ptp";
        std::map<PEAT_Rank, std::string> raw_mpi_names = get_file_names(glob.input_path, glob.trace_files, glob.mpi_files, true);
        std::map<PEAT_Rank, std::string> raw_hwc_names = get_file_names(glob.input_path, glob.trace_files, glob.hwc_files, true);
        for (std::map<PEAT_Rank, std::string>::const_iterator it = raw_mpi_names.begin(); it != raw_mpi_names.end(); ++it) {
            if (ranks_to_load.count(it->first) == 1)
                mpi_file_names[it->first] = it->second;
        }
        for (std::map<PEAT_Rank, std::string>::const_iterator it = raw_hwc_names.begin(); it != raw_hwc_names.end(); ++it) {
            if (ranks_to_load.count(it->first) == 1)
                hwc_file_names[it->first] = it->second;
        }

        model_id = new Model_ID();
        model_id->initialize(glob);
        std::map<PEAT_Rank, std::string> file_input_com_names = get_file_names(glob.input_path, glob.trace_files, glob.com_ext, true);
        std::map<PEAT_Rank, std::string> file_input_app_names;
        file_input_app_names[0] = glob.input_path + DIR_DELIMITER + glob.trace_files + glob.app_ext;
        std::map<PEAT_Rank, std::string> file_input_ptp_names;
        file_input_ptp_names[0] = glob.input_path + DIR_DELIMITER + glob.trace_files + glob.ptpd_ext;
        ptp = new PTP(glob);
        ptp->initialize(file_input_com_names, file_input_ptp_names, file_input_app_names, model_id);

        tsc_predictor.assign(ranks_to_load.empty() ? 0 : (*ranks_to_load.rbegin() + 1), 0);
    }
    eventsV.resize(user_regions->last_event_idx.size());
    events_tr.resize(user_regions->last_event_idx.size());
    // PEAT_Rank_Pair p(user_regions->last_event_idx.size());
    for (int i = 0; i < eventsV.size(); i++) {
        if (ranks_to_load.count(i) == 1) {
            eventsV[i].resize(user_regions->last_event_idx.at(i) + 1);
#ifdef QTV_TR_VECTOR
            events_tr[i].resize(user_regions->last_event_idx.at(i) + 1);
            std::fill(events_tr[i].begin(), events_tr[i].end(), -1);
#endif
        }
    }
    // messages.resize(p.max());
    messages.resize(user_regions->last_event_idx.size());
    coll.resize(user_regions->last_event_idx.size());
#ifdef _OPENMP
    locks = new std::mutex[user_regions->last_event_idx.size()];
#endif

    return true;
}

void Elements::draw_elements(QProgressBar *pb, QScrollBar *sr, QImage *finalImage, QImage *threadImages[], int render_threads, qint32 start, qint32 stop) // start,stop are the mouse coordinates
{
    if (!finalImage) {
        return;
    }
    double screen_w = double(finalImage->width());
    // Convert mouse coordinates into trace time
    double zoom_length_time = zoom_time_stop - zoom_time_start; // previous length in TSC
    double frame_shift = zoom_time_start;
    zoom_time_start = frame_shift + zoom_length_time * double(qMin(start, stop)) / screen_w; // new length in TSC based on mouse click
    zoom_time_stop = frame_shift + zoom_length_time * double(qMax(start, stop)) / screen_w;
    double showed_duration = zoom_time_stop - zoom_time_start;
    double model_duration = model_time_stop - model_time_start;
    int scale = (int)((model_duration / showed_duration));
    sr->setMaximum(scale - 1);
    int starting_position = (int)((zoom_time_start - model_time_start) / (model_duration)*scale);
    sr->setValue(starting_position);
    for (int a = 0; a < render_threads; a++)
        threadImages[a]->fill(Qt::transparent);
    draw_elements(pb, sr, finalImage, threadImages, render_threads, zoom_time_start, zoom_time_stop);
}
void Elements::draw_elements(QProgressBar *pb, QScrollBar *sr, QImage *finalImage, QImage *threadImages[], int render_threads, const qreal start_time, const qreal stop_time)
{
    const QString prog_bar_text = pb->format();
    std::vector<QPainter *> p;
    p.resize(render_threads);
    for (int a = 0; a < render_threads; a++) {
        p[a] = new QPainter(threadImages[a]);
        // p[a]->begin(threadImages[a]);
        p[a]->setRenderHint(QPainter::Antialiasing, true);
    }
    const qreal current_opacity = 0.5;
    int w_min_name_size = p[0]->fontMetrics().maxWidth() >> 1;
    w_min_name_size *= strlen("MPI_ISEND");
    no_draw_elems = 0;

    sr->hide();
    pb->setFormat("Rendering trace..." + prog_bar_text + " done");
    pb->setMinimum(0);
    pb->setMaximum(get_draw_rank_count() + get_total_rank_count());
    pb->setValue(0);
    pb->show();
    QApplication::processEvents();
    // Draw functions (events)
    PEAT_Rank r = 0;
    long long prevclk = 0;
    int pb_value = 0;
    long s_width = paint.width;
    long thread_height = paint.thread_height;
    double pix_in_tic = s_width / (stop_time - start_time);
    double w_min_name_h = (p[0]->fontMetrics().height()) * 0.8;           // to prevent crossing MPI event rect and text. MPI Event will not be shown in this case.
#pragma omp parallel for num_threads(render_threads) schedule(dynamic) // firstprivate (rankPtext,funcPline,pePline,funcPtext,linePrank)
                                                                       //   for (std::vector<std::vector<TV_Event*>>::const_iterator it_rank = eventsV.begin(); it_rank != eventsV.end(); ++it_rank, ++r) {
    for (r = 0; r < ranks_to_load_v.size(); ++r) {
#ifdef _OPENMP
        int tid = omp_get_thread_num();
#else
        int tid = 0;
#endif
        assert(tid < render_threads);
        if (tid == 0)
            progress(prevclk, QApplication::processEvents);
        std::vector<TV_Event *> &it_rank = eventsV.at(ranks_to_load_v.at(r));
        QPen line_rank(Qt::black, 0, Qt::DashDotLine);
        // QPen   linePrank(Qt::black, 0, Qt::SolidLine);

        // p[tid]->setOpacity(current_opacity);
        p[tid]->setOpacity(1.0);
        // draw user code rect
        p[tid]->setPen(Qt::NoPen);
        p[tid]->setBrush(tv_colors.event_brushes[TV_BRUSH_EVENT_TYPE_USER_CODE]);
        long y_start = paint.mpi_ulc.at(r);
        long pix_in_rank = (get_mpi_threads(r) + get_omp_threads(r) + get_gpu_threads(r)) * paint.thread_height;
        p[tid]->drawRect(0, y_start, s_width, pix_in_rank);
        if (r && (r < get_draw_rank_count())) {
            p[tid]->setPen(line_rank);
            p[tid]->drawLine(0, r * pix_in_rank, s_width, r * pix_in_rank);
        }
        QColor user_region_color = threadImages[tid]->pixel(0, y_start);
        // p[tid]->setOpacity(current_opacity);

        if (it_rank.empty())
            continue;
#pragma omp atomic
        pb_value++;
        if (tid == 0) {
            if (pb->value() != pb_value)
                pb->setValue(pb_value);
        }
        EventRankTypeConstIter init_it = lower_bound(it_rank, start_time);
        if (init_it != it_rank.begin()) {
            --init_it;
        }
        // long xEndOfPreviousMPIEvent = 0;
        for (EventRankTypeConstIter it = init_it; it != it_rank.end(); ++it) {
            // long height = thread_height;
            const TV_Event *cur_func = *it;
            if (cur_func == NULL) {
                continue;
            }
            if (cur_func->e_time.func.out <= start_time) {
                continue;
            }
            if (cur_func->e_time.func.in >= stop_time) {
                break;
            }
            if (tid == 0)
                progress(prevclk, QApplication::processEvents);
            std::vector<const TV_Event *> subtree; // the call and its nested records at every level
            cur_func->collect_subtree(subtree);
            for (size_t f = 0; f < subtree.size(); f++) {
                const TV_Event *cur_func_ptr = subtree[f];
                long f_width = double(cur_func_ptr->e_time.func.duration()) * pix_in_tic;
                long pe_width = double(cur_func_ptr->e_time.pe.duration()) * pix_in_tic;
                if (pe_width == f_width)
                    pe_width = 0;
                if (f_width < 1) {
                    f_width = 1;
                }
                long x_start = (cur_func_ptr->e_time.func.in - start_time) * pix_in_tic;
                if (x_start < 0) {
                    f_width += x_start;
                    x_start = 0;
                }
                long x_pe_start = (cur_func_ptr->e_time.pe.in - start_time) * pix_in_tic;
                if (x_pe_start < 0) {
                    if (pe_width > 0)
                        pe_width += x_pe_start;
                    x_pe_start = 0;
                }
                // long y_start = ranks_to_load_v_rev[cur_func_ptr->rank] * pixInRank + (pixInRank - (heightPercent * pixInRank)) * 0.5; //base
                assert(ranks_to_load_v_rev[cur_func_ptr->rank] == r);
                PEAT_Rank cur_func_rank = ranks_to_load_v_rev[cur_func_ptr->rank];
                long y_start = thread_height * cur_func_ptr->thread;
                if (cur_func->is_mpi()) {
                    y_start += paint.mpi_ulc.at(cur_func_rank);
                } else {
                    if (cur_func->is_omp()) {
                        y_start += paint.omp_ulc.at(cur_func_rank);
                    } else {
                        if (cur_func->is_gpu()) {
                            y_start += paint.gpu_ulc.at(cur_func_rank);
                        }
                    }
                }
                p[tid]->setOpacity(1.0);
                QColor cc = threadImages[tid]->pixel(x_start, y_start);
                if (cc != user_region_color && (f_width < shown->func_min_width)) {
                    no_draw_elems++;
                    continue;
                }
                // draw MPI event rect
                //  QFont font = p[tid]->font();
                //  qreal ps = font.pointSize();

                long height = thread_height;
                PEAT_MPI_Comm event_comm = 0;
                if (cur_func_ptr->get_comm(event_comm) && !shown->comm_visible(event_comm))
                    continue;
                switch (cur_func_ptr->get_event_type()) {
                case TV_EVENT_TYPE_COLLECTIVE:
                    if (!mpi_func_shown(*shown, cur_func_ptr, TV_MPI_FUNC_GROUP_COLLECTIVES))
                        continue;
                case TV_EVENT_TYPE_COMM:
                    p[tid]->setBrush(tv_colors.event_brushes[TV_BRUSH_EVENT_TYPE_COLLECTIVE]);
                    break;
                case TV_EVENT_TYPE_TOR:
                    if (!mpi_func_shown(*shown, cur_func_ptr, TV_MPI_FUNC_GROUP_P2P))
                        continue;
                    if (cur_func_ptr->check_tx()) {
                        p[tid]->setBrush(tv_colors.event_brushes[TV_BRUSH_EVENT_TYPE_SOURCE]);
                    }
                    if (cur_func_ptr->check_rx()) {
                        p[tid]->setBrush(tv_colors.event_brushes[TV_BRUSH_EVENT_TYPE_DESTINATION]);
                    }
                    break;
                case TV_EVENT_TYPE_TAR:
                    if (!mpi_func_shown(*shown, cur_func_ptr, TV_MPI_FUNC_GROUP_P2P))
                        continue;
                    p[tid]->setBrush(tv_colors.event_brushes[TV_BRUSH_EVENT_TYPE_SAND]);
                    break;
                case TV_EVENT_TYPE_START:
                    if (!mpi_func_shown(*shown, cur_func_ptr, TV_MPI_FUNC_GROUP_REQUESTS))
                        continue;
                    p[tid]->setBrush(tv_colors.event_brushes[TV_BRUSH_EVENT_TYPE_START]);
                    break;
                case TV_EVENT_TYPE_COMPLETION:
                    if (!mpi_func_shown(*shown, cur_func_ptr, TV_MPI_FUNC_GROUP_REQUESTS))
                        continue;
                    p[tid]->setBrush(tv_colors.event_brushes[TV_BRUSH_EVENT_TYPE_COMPLETION]);
                    break;
                case TV_EVENT_TYPE_GPU_TRANSFER:
                    if (!shown->gpu_functions[1])
                        continue;
                    p[tid]->setBrush(tv_colors.event_brushes[TV_BRUSH_EVENT_TYPE_GPU]);
                    break;
                case TV_EVENT_TYPE_GPU_LOCAL:
                    if (!shown->gpu_functions[0])
                        continue;
                    p[tid]->setBrush(tv_colors.event_brushes[TV_BRUSH_EVENT_TYPE_GPU]);
                    break;
                case TV_EVENT_TYPE_OMPT_TARGET_DATA:
                    if (!shown->omp_functions[3])
                        continue;
                    // p[tid]->setFont(font);
                    p[tid]->setBrush(tv_colors.event_brushes[TV_BRUSH_EVENT_TYPE_OMPT_LOCAL]);
                    break;
                case TV_EVENT_TYPE_OMPT_LOCAL:
                    if (!shown->omp_functions[0])
                        continue;
                    // p[tid]->setFont(font);
                    p[tid]->setBrush(tv_colors.event_brushes[TV_BRUSH_EVENT_TYPE_OMPT_LOCAL]);
                    break;
                case TV_EVENT_TYPE_OMPT_PARALLEL:
                    if (!shown->omp_functions[1])
                        continue;
                    p[tid]->setBrush(tv_colors.event_brushes[TV_BRUSH_EVENT_TYPE_OMPT_PARALLEL]);
                    height = thread_height * get_omp_threads(r);
                    break;
                default:
                    p[tid]->setBrush(tv_colors.event_brushes[TV_BRUSH_EVENT_TYPE_OTHER]);
                    break;
                }
                // font.setPixelSize(height);
                QRect rfrom(x_start, y_start, f_width, height);
                QRect rpefrom(x_pe_start, y_start, pe_width, height);
                p[tid]->setPen(tv_colors.func_line);
                p[tid]->setOpacity(current_opacity);
                p[tid]->drawRect(rfrom);
                if (pe_width > 1) {
                    p[tid]->setBrush(tv_colors.event_brushes[TV_BRUSH_EVENT_TYPE_PE]);
                    p[tid]->setPen(tv_colors.pe_line);
                    p[tid]->drawRect(rpefrom);
                }

                // draw MPI event text
                if (f_width > w_min_name_size) {
                    p[tid]->setOpacity(1.0);
                    p[tid]->setPen(tv_colors.func_text);
                    p[tid]->drawText(rfrom, Qt::AlignLeft | Qt::AlignVCenter, QString(global_func_id.name_by_idx(cur_func_ptr->func_id)) + QString("\nidx:") + QString().setNum(cur_func_ptr->ids[0])); // Slow!
                }
                /*
                font.setPointSize(ps);
                p[tid]->setFont(font);

                //draw user region function name
                const long startFunctionNameBefore = qMax(long(0), xEndOfPreviousMPIEvent);
                const long widthFunctionNameBefore = x_start - startFunctionNameBefore;
                if (widthFunctionNameBefore > w_min_name_size) {
                    QString address1Name;
                    const PEAT_Addr address1 = cur_func_ptr->addrs[0].addr;
                    if (address1) {
                        address1Name.asprintf("%p", address1);
                    }
                    p[tid]->setPen(funcPtext);
                    p[tid]->setOpacity(1.0);
                    QRect userFunctionBefore(startFunctionNameBefore, y_start, widthFunctionNameBefore, thread_height);
                    p[tid]->drawText(userFunctionBefore, Qt::AlignRight | Qt::AlignVCenter, address1Name); //Slow!
                    p[tid]->setOpacity(current_opacity);//restore opacity
                }
                */
                // save previous MPI function pixel end
                //  if (cur_func_ptr == cur_func) xEndOfPreviousMPIEvent = x_start + width;
            }
        }
        p[tid]->setOpacity(1.0);
        p[tid]->setPen(tv_colors.rank_text);
        QString rstr = "R" + QString::number(ranks_to_load_v[r]);
        p[tid]->drawText(5, paint.mpi_ulc.at(r) + w_min_name_h, rstr);
        p[tid]->setOpacity(current_opacity);
    }
    QPainter f(finalImage);
    for (int i = 0; i < render_threads; ++i) {
        f.drawImage(0, 0, *threadImages[i]);
    }
    for (int a = 0; a < render_threads; a++)
        threadImages[a]->fill(Qt::transparent);
    // Draw messages
    std::vector<qint32> y_offsets;
#define y_indent 5
#define y_inc 4
    PEAT_Rank num_ranks = get_total_rank_count();
    y_offsets.resize(num_ranks);
    PEAT_Rank cur_rank = 0;
#pragma omp parallel for num_threads(render_threads) schedule(dynamic)
    for (cur_rank = 0; cur_rank < messages.size(); ++cur_rank) {
        // for (std::vector<std::map<PEAT_Clock, TV_Message> >::const_iterator it_rank = messages.begin(); it_rank != messages.end(); ++it_rank, ++cur_rank) {
#ifdef _OPENMP
        int tid = omp_get_thread_num();
#pragma omp atomic
        pb_value++;
#else
        int tid = 0;
#endif
        assert(tid < render_threads);
        if (tid == 0) {
            if (pb->value() != pb_value)
                pb->setValue(pb_value);
        }
        std::map<PEAT_Clock, TV_Message> &it_rank = messages.at(cur_rank);
        if (tid == 0)
            progress(prevclk, QApplication::processEvents);
        if (it_rank.empty())
            continue;
        // size_t size = it_rank.size();
        PEAT_LNum mess_num = 0;
        std::map<PEAT_Clock, TV_Message>::const_iterator init_it = it_rank.lower_bound(start_time);
        for (std::map<PEAT_Clock, TV_Message>::const_iterator it_msg = init_it; it_msg != it_rank.end(); ++it_msg) {
            // long height = rank_height;
            const TV_Event *from = it_msg->second.from;
            const TV_Event *to = it_msg->second.to;
            long from_thread_height = thread_height;
            long to_thread_height = thread_height;
            mess_num++;
            if (to->e_time.func.out <= start_time) {
                continue;
            }
            if (from->e_time.func.in >= stop_time && it_msg->second.msg_type != TV_PEN_MESSAGE_TYPE_REVERSED) {
                break;
            }
            PEAT_MPI_Comm message_comm = 0;
            if (from->get_comm(message_comm) && !shown->comm_visible(message_comm))
                continue;
            if (tid == 0)
                progress(prevclk, QApplication::processEvents);
            // long wdt_from= qAbs(from.timeOut-from.timeIn)*pixInTic;
            qint32 y_offset = y_offsets[ranks_to_load_v_rev[from->rank]];
            y_offsets[ranks_to_load_v_rev[from->rank]] += y_inc;
            y_offset %= (thread_height - y_indent);
            y_offset += y_indent;
            long wdt_from = from->e_time.pe.duration() * pix_in_tic;
            long x_from = (from->e_time.pe.in - start_time) * pix_in_tic;
            long y_from = y_offset + from_thread_height * from->thread;
            long wdt_to = to->e_time.pe.duration() * pix_in_tic;
            long x_to = (to->e_time.pe.in - start_time) * pix_in_tic;
            long y_to = y_offset + to_thread_height * to->thread;
            // by default a line is drawn only if one of its ends is inside the view; optionally also when it just crosses the view
            // (e.g. init long before, start long after)
            if ((x_from > 0 && x_from < s_width) || (x_to + wdt_to > 0 && x_to + wdt_to < s_width) || (shown->draw_offscreen_messages && qMin(x_from, x_to + wdt_to) < s_width && qMax(x_from, x_to + wdt_to) > 0)) {
                // Do not draw a line with 1 pix width
                p[tid]->setPen(tv_colors.message_pens[it_msg->second.msg_type]);
                switch (it_msg->second.msg_type) {
                case TV_PEN_MESSAGE_TYPE_NORMAL:
                    p[tid]->setPen(tv_colors.message_pens[TV_PEN_MESSAGE_TYPE_NORMAL]);
                    goto cont_noraml;
                case TV_PEN_MESSAGE_TYPE_REVERSED:
                    p[tid]->setPen(tv_colors.message_pens[TV_PEN_MESSAGE_TYPE_REVERSED]);
                cont_noraml:
                    if (!mpi_func_shown(*shown, from, TV_MPI_FUNC_GROUP_P2P) || !shown->mpi_messages[0])
                        continue;
                    if (qAbs(x_to + wdt_to - x_from) <= showElementsThreshold) {
                        no_draw_elems++;
                        continue;
                    }
                    y_from += paint.mpi_ulc.at(ranks_to_load_v_rev[from->rank]);
                    y_to += paint.mpi_ulc.at(ranks_to_load_v_rev[to->rank]);
                    y_from -= y_offset; // always start from center
                    y_to -= y_offset;   // always start from center
                    p[tid]->drawLine(x_from, y_from + from_thread_height * 0.5, x_to + wdt_to, y_to + to_thread_height * 0.5);
                    // Point at the end of the line
                    p[tid]->drawPoint(x_from, y_from + from_thread_height * 0.5);
                    p[tid]->drawPoint(x_to + wdt_to, y_to + to_thread_height * 0.5);
                    break;
                case TV_PEN_MESSAGE_TYPE_GPU_HD_TRANSFER:
                case TV_PEN_MESSAGE_TYPE_GPU_EXEC:
                    y_from += paint.gpu_ulc.at(ranks_to_load_v_rev[from->rank]);
                    y_to += paint.gpu_ulc.at(ranks_to_load_v_rev[to->rank]);
                    p[tid]->drawLine(x_from + wdt_from * 0.5, y_from, x_to + wdt_to * 0.5, y_to);
                    // p[tid]->setPen(message_pens[TV_PEN_MESSAGE_TYPE_NONE]);
                    p[tid]->drawPoint(x_from + wdt_from * 0.5, y_from);
                    p[tid]->drawPoint(x_to + wdt_to * 0.5, y_to);
                    break;
                case TV_PEN_MESSAGE_TYPE_INITIATION:
                case TV_PEN_MESSAGE_TYPE_COMPLETION:
                    if ((!shown->mpi_messages[2] || !shown->mpi_functions[TV_MPI_FUNC_GROUP_REQUESTS]) && it_msg->second.msg_type == TV_PEN_MESSAGE_TYPE_INITIATION)
                        continue;
                    if ((!shown->mpi_messages[3] || !shown->mpi_functions[TV_MPI_FUNC_GROUP_REQUESTS]) && it_msg->second.msg_type == TV_PEN_MESSAGE_TYPE_COMPLETION)
                        continue;
                    if (qAbs(x_to + wdt_to - x_from) <= showElementsThreshold) {
                        no_draw_elems++;
                        continue;
                    }
                    y_from += paint.mpi_ulc.at(ranks_to_load_v_rev[from->rank]);
                    y_to += paint.mpi_ulc.at(ranks_to_load_v_rev[to->rank]);
                    p[tid]->drawLine(x_from + wdt_from * 0.5, y_from, x_to + wdt_to * 0.5, y_to);
                    // p[tid]->setPen(message_pens[TV_PEN_MESSAGE_TYPE_NONE]);
                    p[tid]->drawPoint(x_from + wdt_from * 0.5, y_from);
                    p[tid]->drawPoint(x_to + wdt_to * 0.5, y_to);
                    break;
                case TV_PEN_MESSAGE_TYPE_COLLECTIVE:
                    if (!shown->mpi_messages[1] || !mpi_func_shown(*shown, from, TV_MPI_FUNC_GROUP_COLLECTIVES))
                        continue;
                    y_from += paint.mpi_ulc.at(ranks_to_load_v_rev[from->rank]);
                    y_to += paint.mpi_ulc.at(ranks_to_load_v_rev[to->rank]);
                    y_from -= y_offset; // always start from center
                    y_to -= y_offset;   // always start from center
                    if (qAbs(x_to + wdt_to * 0.5 - x_from + wdt_from * 0.5) <= showElementsThreshold) {
                        no_draw_elems++;
                        continue;
                    }
                    p[tid]->setPen(tv_colors.message_pens[TV_PEN_MESSAGE_TYPE_COLLECTIVE]);
                    p[tid]->drawLine(x_from + wdt_from * 0.5, y_from + from_thread_height * 0.5, x_to + wdt_to * 0.5, y_to + to_thread_height * 0.5);
                    // Point at the end of the line
                    // p[tid]->setPen(message_pens[TV_PEN_MESSAGE_TYPE_NONE]);
                    p[tid]->drawPoint(x_from + wdt_from * 0.5, y_from + from_thread_height * 0.5);
                    p[tid]->drawPoint(x_to + wdt_to * 0.5, y_to + to_thread_height * 0.5);
                    break;
                default:
                    qDebug() << "Unknown Message Type x_from " << x_from << " x_to " << x_to + wdt_to;
                    break;
                }
            }
        }
    }

    for (int a = 0; a < render_threads; a++) {
        // p[a]->end();
        delete p[a];
    }

    for (int i = 0; i < render_threads; ++i) {
        f.drawImage(0, 0, *threadImages[i]);
    }

    pb->hide();
    sr->show();
    pb->setFormat(prog_bar_text); // revert original text
    QApplication::processEvents();
    // qDebug()<< "No drew elements" << no_draw_elems << "from" << fsize() << msize();
}

bool Elements::find_idx(PEAT_Rank rank, PEAT_UID idx)
{
    TV_Event *event = find_event(rank, idx);
    if (event) {
        zoom_time_start = event->e_time.func.in;
        zoom_time_stop = event->e_time.func.out;
        return true;
    }
    return false;
}
TV_Event *Elements::find_event(PEAT_Rank rank, PEAT_UID top_id) const
{
#ifdef QTV_TR_VECTOR
    if (top_id < 0 || events_tr[rank].size() <= static_cast<size_t>(top_id))
        return NULL;
    if (events_tr[rank][top_id] < 0)
        return NULL;
    if (eventsV[rank].size() <= static_cast<size_t>(events_tr[rank][top_id]))
        return NULL;
    return eventsV[rank][events_tr[rank][top_id]];
#else
    if (events_tr.at(rank).count(top_id) < 1)
        return NULL;
    return events_tr.at(rank).at(top_id);
#endif
}

TV_Event *Elements::find_event(PEAT_Rank rank, const PEAT_Event_IDs &ids) const
{
    if (ids.empty())
        return NULL;
    TV_Event *host = find_event(rank, ids[0]);
    if (host == NULL || !peat_ids_nested(ids))
        return host; // a top-level record
    return host->find_event(ids);
}

int Elements::bind_ids(const String_Split &line, int pos, PEAT_Rank rank, PEAT_Event_IDs &ids) const
{
    return peat_ids_parse(line, pos, user_regions->get_id_columns(rank), ids);
}

TV_Event *Elements::find_event(PEAT_Rank rank, PEAT_Clock clk, int thread, int selector) const
{
    EventRankTypeConstIter it_func = lower_bound(eventsV.at(rank), clk);

    if (it_func != eventsV.at(rank).end()) {
        TV_Event *cur_event = *it_func;
        while (cur_event && cur_event->e_time.func.out < clk) {
            ++it_func;
            if (it_func == eventsV.at(rank).end()) {
                return NULL;
            }
            cur_event = *it_func;
        }
        if (cur_event->e_time.func.in > clk) return NULL;
        while (cur_event) {
            if (thread != cur_event->thread) {
                ++it_func;
                if (it_func == eventsV.at(rank).end())
                    return NULL;
                cur_event = *it_func;
                while (cur_event && cur_event->e_time.func.out < clk && thread != cur_event->thread) {
                    ++it_func;
                    if (it_func == eventsV.at(rank).end()) {
                        return NULL;
                    }
                    cur_event = *it_func;
                }
                if (thread != cur_event->thread)
                    return NULL;
            }
            if (cur_event->e_time.func.in > clk) return NULL;
            // selector match
            if (selector == 0 && (cur_event->is_mpi()))
                return cur_event;
            if (selector == 1 && (cur_event->is_omp()))
                return cur_event;
            if (selector == 2 && (cur_event->is_gpu()))
                return cur_event;
            ++it_func;
            if (it_func == eventsV.at(rank).end()) {
                return NULL;
            }
        }
    }
    return NULL;
}

bool Elements::add_event(TV_Event *event, TV_TraceParts &tp)
{
    if (event->get_event_type() == TV_EVENT_TYPE_LOCAL && !tp.mpi_functions[TV_MPI_FUNC_GROUP_LOCALS])
        return false;
    if (event->get_event_type() == TV_EVENT_TYPE_LOCAL_BUF && !tp.mpi_functions[TV_MPI_FUNC_GROUP_LOCALS])
        return false;
    if (event->get_event_type() == TV_EVENT_TYPE_COLLECTIVE && !mpi_func_shown(tp, event, TV_MPI_FUNC_GROUP_COLLECTIVES))
        return false;
    if (event->get_event_type() == TV_EVENT_TYPE_COMM && !mpi_func_shown(tp, event, TV_MPI_FUNC_GROUP_COLLECTIVES))
        return false;
    if (event->get_event_type() == TV_EVENT_TYPE_TOR && !mpi_func_shown(tp, event, TV_MPI_FUNC_GROUP_P2P))
        return false;
    if (event->get_event_type() == TV_EVENT_TYPE_TAR && !mpi_func_shown(tp, event, TV_MPI_FUNC_GROUP_P2P))
        return false;
    if (event->get_event_type() == TV_EVENT_TYPE_START && !mpi_func_shown(tp, event, TV_MPI_FUNC_GROUP_REQUESTS))
        return false;
    if (event->get_event_type() == TV_EVENT_TYPE_COMPLETION && !mpi_func_shown(tp, event, TV_MPI_FUNC_GROUP_REQUESTS))
        return false;
    if (event->e_time.func.out > model_time_stop) {
        model_time_stop = event->e_time.func.out;
    }
    if (event->e_time.func.in < model_time_start) {
        model_time_start = event->e_time.func.in;
    }

    try {
        // std::pair<std::map<PEAT_Clock, TV_Event*>::iterator, bool> insertStatus;
#if 0
        do {
            auto insertStatus = events[event->rank].insert(events.at(event->rank).end(), std::make_pair(event->e_time.func.in, event));
            if (insertStatus->second != event) {
                event->e_time.func.in++;
            } else {
                break;
            }
        } while (true);
#endif
        std::vector<PEAT_Thread> *max_tid;
        switch (event->get_event_type()) {
        case TV_EVENT_TYPE_OMPT_LOCAL:
        case TV_EVENT_TYPE_OMPT_TARGET_DATA:
        case TV_EVENT_TYPE_OMPT_PARALLEL:
            max_tid = &max_omp_tid;
            break;
        case TV_EVENT_TYPE_GPU_LOCAL:
        case TV_EVENT_TYPE_GPU_TRANSFER:
        case TV_EVENT_TYPE_GPU_EXECUTION:
            max_tid = &max_gpu_tid;
            break;
        default:
            max_tid = &max_mpi_tid;
            break;
        }
        if (!event->is_nested()) {
            eventsV[event->rank][event->ids[0]] = event;
            if (event->thread + 1 > (*max_tid)[event->rank])
                (*max_tid)[event->rank] = event->thread + 1;
        } else { // nested record: attach to its top-level record (any event may host nested records)
            TV_Event *host = eventsV[event->rank][event->ids[0]];
            if (!host || !host->put_event(event))
                return false; // host filtered out or not loaded: reject the nested record too
        }
    } catch (const std::bad_alloc &e) {
        qDebug() << "Elements::addFunction. No memory. Size:" << fsize() << " sizeof " << sizeof(TV_Event) << e.what();
        exit(1);
    }
    return true;
}

void Elements::add_message(TV_Event *from, TV_Event *to, TV_Pen_Message_Type type, TV_TraceParts &tp, PEAT_Rank rank, PEAT_UID index)
{
    if (!from || !to) {
        // qDebug() << "Function indeces are out of range. Message hasn't been added.";//SIGSEGV << from->second.idx << to->second.idx << fsize();
        return;
    }
    if (type == TV_PEN_MESSAGE_TYPE_NORMAL && !tp.mpi_messages[0])
        return;
    if (type == TV_PEN_MESSAGE_TYPE_COLLECTIVE && !tp.mpi_messages[1])
        return;
    if (type == TV_PEN_MESSAGE_TYPE_INITIATION && !tp.mpi_messages[2])
        return;
    if (type == TV_PEN_MESSAGE_TYPE_COMPLETION && !tp.mpi_messages[3])
        return;

    if (type == TV_PEN_MESSAGE_TYPE_NORMAL) {
        if (from->e_time.pe.in > to->e_time.pe.out) {
#ifdef _DEBUG
            qDebug() << "Start of" << global_func_id.name_by_idx(from->func_id) << "function, in rank" << from->rank << ", at" << from->e_time.pe.in << "is later than end of" << global_func_id.name_by_idx(to->func_id) << "function, in rank" << to->rank << ", at" << to->e_time.pe.out << "D: " << from->e_time.pe.in - to->e_time.pe.out;
#endif
            type = TV_PEN_MESSAGE_TYPE_REVERSED;
        }
    } else {
        type = type; // to set debug point nhere
    }
    TV_Message msg;
    msg.from = from;
    msg.to = to;
    msg.msg_type = type;
    msg.rank = rank;
    msg.index = index;
    try {
        // std::pair<std::map<PEAT_Clock, TV_Message>::iterator, bool> insertStatus;
        // insertStatus = messages[from->rank].insert(std::pair<PEAT_Clock, TV_Message>(to->e_time.func.out, msg));
        // if (insertStatus.second == false) {
        //     qDebug() << "Message pair exists.\nFrom " << from << "\n  To " << to << "\n";
        // }
        // #pragma omp critical
        {
            PEAT_Clock out_time = to->e_time.func.out;
            // PEAT_Rank_Pair r(from->rank, to->rank, eventsV.size());
            PEAT_Rank r = from->rank;
#ifdef _OPENMP
            locks[r].lock();
#endif
            while (messages[r].count(out_time) >= 1)
                out_time++;
            // messages[r].insert(messages.at(r).end(), std::make_pair(out_time, msg));
            messages[r][out_time] = msg;
#ifdef _OPENMP
            locks[r].unlock();
#endif
        }
    } catch (const std::bad_alloc &e) {
        qDebug() << "Elements::addMessage. No memory. Size:" << msize() << " sizeof " << sizeof(TV_Message) << e.what();
        exit(1);
    }
}

void Elements::clear()
{
    qDebug() << "Clearing" << fsize() << "functions and " << msize() << "messages.";

#if 0
#pragma omp parallel for schedule(dynamic) 
    for (PEAT_Rank rank = 0; rank < eventsV.size(); ++rank) {
        for (std::vector<TV_Event*>::iterator event_it = eventsV.at(rank).begin(); event_it != eventsV.at(rank).end(); ++event_it) {
            if (*event_it != NULL) delete (*event_it); *event_it = NULL;
        }
    }
#else
    for (std::vector<std::vector<TV_Event *>>::reverse_iterator rank_it = eventsV.rbegin(); rank_it != eventsV.rend(); ++rank_it) {
        for (std::vector<TV_Event *>::reverse_iterator event_it = rank_it->rbegin(); event_it != rank_it->rend(); ++event_it) {
            if (*event_it != NULL)
                delete (*event_it);
            *event_it = NULL;
        }
    }
#endif
    eventsV.clear();
    events_tr.clear();
    messages.clear();
    coll.clear();
    // numberOfRanks = 0;
    model_time_stop = 0.0;
    model_time_start = LLONG_MAX;
    // traceInfo; //???
    // zoom_time_start = 0.0;
    // zoom_time_stop = 0.0;
}

void Elements::get_functions_in_region(QTableWidget *mpi_tw, QTableWidget *omp_tw, QTableWidget *gpu_tw, const PEAT_Clock start, const PEAT_Clock stop, const PEAT_Num height) const
{
    Q_ASSERT(mpi_tw);
    Q_ASSERT(omp_tw);
    Q_ASSERT(gpu_tw);
    QTableWidget *all_tables[3] = {mpi_tw, omp_tw, gpu_tw};
    QStringList column_names;
    column_names << "Name" << "Rank" << "IDs" << "Start" << "End" << "Duration(clk)" << "Duration(sec E-06)" << "Before (clk)" << "After (clk)" << "Prev UID" << "Next UID";
    for (QTableWidget *tbl : all_tables) {
        tbl->setColumnCount(column_names.size());
        tbl->setHorizontalHeaderLabels(column_names);
        tbl->setWindowTitle("Functions in region");
    }

    auto add_addr_info = [&](QTableWidgetItem *item, const TV_Event *e) {
        if (!user_regions || !e->addrs)
            return;
        QString tt;
        QString source_file;
        int source_line = -1;
        QVariantMap userData;
        for (int addr_num = 0; addr_num < trace_info.num_addrs; addr_num++) {
            const PEAT_UID addr = e->addrs->addr[addr_num];
            std::ostringstream itemObjectAddress;
            User_Address_Info *addr_info = user_regions->get_addr_by_id(e->rank, addr);
            itemObjectAddress << addr_info->full_path << "+0x" << std::hex << addr_info->addr_offset << "\n"
                              << addr_info->source << ":" << std::dec << addr_info->line << "\n";
            tt = tt + itemObjectAddress.str().c_str();
            source_file = QString::fromStdString(addr_info->source);
            source_line = addr_info->line;
            QString file_key = "src_file" + QString::number(addr_num);
            QString line_key = "src_line" + QString::number(addr_num);
            if (!source_file.isEmpty() && source_line >= 0) {
                userData[file_key] = source_file;
                userData[line_key] = source_line;
            }
        }
        userData["func_name"] = global_func_id.name_by_idx(e->func_id);
        item->setToolTip(tt);
        item->setData(Qt::UserRole, userData);
    };

    auto add_row = [&](QTableWidget *tbl, const TV_Event *cur_func_ptr, const TV_Event *prev_event, const TV_Event *next_event) {
        int row = tbl->rowCount();
        tbl->setRowCount(row + 1);
        // Name
        QTableWidgetItem *name = new QTableWidgetItem(global_func_id.name_by_idx(cur_func_ptr->func_id));
        tbl->setItem(row, 0, name);
        // Rank
        QTableWidgetItem *rank = new QTableWidgetItem();
        rank->setData(Qt::DisplayRole, cur_func_ptr->rank);
        rank->setTextAlignment(Qt::AlignHCenter);
        tbl->setItem(row, 1, rank);
        // IDs
        QTableWidgetItem *primary_item = new QTableWidgetItem();
        primary_item->setData(Qt::DisplayRole, QString::fromStdString(peat_ids_string(cur_func_ptr->ids)));
        primary_item->setTextAlignment(Qt::AlignHCenter);
        tbl->setItem(row, 2, primary_item);
        // Time In
        QTableWidgetItem *time_in = new QTableWidgetItem(outputDouble(cur_func_ptr->e_time.func.in));
        time_in->setTextAlignment(Qt::AlignRight);
        tbl->setItem(row, 3, time_in);
        // Time Out
        QTableWidgetItem *time_out = new QTableWidgetItem(outputDouble(cur_func_ptr->e_time.func.out));
        time_out->setTextAlignment(Qt::AlignRight);
        tbl->setItem(row, 4, time_out);
        // Duration
        QTableWidgetItem *duration = new QTableWidgetItem(outputDouble(cur_func_ptr->e_time.func.duration()));
        duration->setTextAlignment(Qt::AlignRight);
        tbl->setItem(row, 5, duration);
        // Duration micro sec
        QTableWidgetItem *duration_micro = new QTableWidgetItem(outputDouble((PEAT_Ratio(cur_func_ptr->e_time.func.duration()) / trace_info.freq) * 1000000));
        duration_micro->setTextAlignment(Qt::AlignRight);
        tbl->setItem(row, 6, duration_micro);
        // distance Before
        PEAT_Clock prev_out = prev_event ? prev_event->e_time.func.out : cur_func_ptr->e_time.func.in;
        QTableWidgetItem *before = new QTableWidgetItem(outputDouble(cur_func_ptr->e_time.func.in - prev_out));
        before->setTextAlignment(Qt::AlignRight);
        tbl->setItem(row, 7, before);
        // distance After
        PEAT_Clock next_in = next_event ? next_event->e_time.func.in : cur_func_ptr->e_time.func.out;
        QTableWidgetItem *after = new QTableWidgetItem(outputDouble(next_in - cur_func_ptr->e_time.func.out));
        after->setTextAlignment(Qt::AlignRight);
        tbl->setItem(row, 8, after);
        // Before ID
        QTableWidgetItem *before_id = new QTableWidgetItem();
        before_id->setData(Qt::DisplayRole, prev_event ? prev_event->ids[0] : -1);
        before_id->setTextAlignment(Qt::AlignRight);
        tbl->setItem(row, 9, before_id);
        // AfterID
        QTableWidgetItem *after_id = new QTableWidgetItem();
        after_id->setData(Qt::DisplayRole, next_event ? next_event->ids[0] : -1);
        after_id->setTextAlignment(Qt::AlignRight);
        tbl->setItem(row, 10, after_id);

        add_addr_info(primary_item, cur_func_ptr);
        add_addr_info(name, cur_func_ptr); // duplicate for name column
        if (prev_event)
            add_addr_info(before_id, prev_event);
        if (next_event)
            add_addr_info(after_id, next_event);
    };

    for (std::vector<std::vector<TV_Event *>>::const_iterator it_rank = eventsV.begin(); it_rank != eventsV.end(); ++it_rank) {
        if (it_rank->empty())
            continue;
        EventRankTypeConstIter prev_it = lower_bound(*it_rank, start);
        EventRankTypeConstIter next_it = prev_it;
        // ++next_it;
        for (EventRankTypeConstIter it_func = prev_it; it_func != it_rank->cend(); ++it_func) {
            const TV_Event *cur_func_ptr = *it_func;
            if (it_func != it_rank->cbegin()) {
                prev_it = it_func;
                --prev_it;
            }
            if (next_it != it_rank->cend()) {
                ++next_it;
            }
            if (next_it == it_rank->cend()) {
                next_it = it_func;
            }
            if (cur_func_ptr == NULL)
                continue;
            if (cur_func_ptr->e_time.func.in > stop)
                break;
            if (cur_func_ptr->e_time.func.out < start)
                continue;
            QTableWidget *target;
            switch (cur_func_ptr->get_event_group()) {
            case 1:
                target = omp_tw;
                break;
            case 2:
                target = gpu_tw;
                break;
            default: // 0: MPI local/p2p/collective/completion events
                target = mpi_tw;
                break;
            }
            add_row(target, cur_func_ptr, *prev_it, *next_it);
        }
    }

    // resize tableViews
    for (QTableWidget *tbl : all_tables) {
        tbl->resizeRowsToContents();
        tbl->resizeColumnsToContents();
        PEAT_Num sum_of_columns_width = 0;
        for (PEAT_Rank i = 0; i < tbl->columnCount(); ++i) {
            sum_of_columns_width += tbl->columnWidth(i) + 1;
        }
        QRect rect = tbl->geometry();
        rect.setWidth(tbl->verticalHeader()->sizeHint().width() + sum_of_columns_width);
        PEAT_Num sum_of_rows_height = tbl->rowCount() * tbl->rowHeight(0) + tbl->rowCount() + tbl->horizontalHeader()->sizeHint().height(); // assume all rows are the same
        if (sum_of_rows_height > height) {
            sum_of_rows_height = height;
        }
        rect.setHeight(sum_of_rows_height);
        tbl->setGeometry(rect);
        tbl->sortItems(1);
    }
}
void Elements::get_messages_in_region(QTableWidget *initiation, QTableWidget *completion, QTableWidget *p2p, QTreeWidget *collective, QTableWidget *gpu, const PEAT_Clock start, const PEAT_Clock stop, const PEAT_Num height) const
{
    std::set<const Elements::Collective *> coll_set;
    QStringList p2p_column_names;
    p2p_column_names << "From" << "Rank" << "Uniq Idx" << "Time" << "To" << "Rank" << "Uniq Idx" << "Time" << "Duration(clk)" << "Duration(sec E-06)" << "Volume(B)";
    p2p->setColumnCount(p2p_column_names.size());
    p2p->setHorizontalHeaderLabels(p2p_column_names);
    p2p->setWindowTitle("Messages in region");
    QStringList ic_column_names;
    ic_column_names << "From" << "Rank" << "Uniq Idx" << "Time" << "To" << "Rank" << "Uniq Idx" << "Time" << "Duration(clk)" << "Duration(sec E-06)";
    initiation->setColumnCount(ic_column_names.size());
    initiation->setHorizontalHeaderLabels(ic_column_names);
    initiation->setWindowTitle("Messages in region");
    completion->setColumnCount(ic_column_names.size());
    completion->setHorizontalHeaderLabels(ic_column_names);
    completion->setWindowTitle("Messages in region");
    // tw->verticalHeader()->hide();
    QBrush failed_msg(Qt::darkCyan, Qt::SolidPattern);
    // PEAT_Rank num_ranks = getTotalRankCount();
    PEAT_Rank cur_rank = 0;
    for (std::vector<std::map<PEAT_Clock, TV_Message>>::const_iterator it_rank = messages.begin(); it_rank != messages.end(); ++it_rank, ++cur_rank) {
        for (std::map<PEAT_Clock, TV_Message>::const_iterator it_msg = it_rank->begin(); it_msg != it_rank->end(); ++it_msg) {
            const TV_Event *from = it_msg->second.from;
            const TV_Event *to = it_msg->second.to;
            if ((from->get_message_draw_start() <= start && to->get_message_draw_end() <= start) || (from->get_message_draw_start() >= stop && to->get_message_draw_end() >= stop))
                continue;
            //            qDebug() << msgs.at(i);
            if (it_msg->second.msg_type == TV_PEN_MESSAGE_TYPE_COLLECTIVE) {
                const Elements::Collective *evt = &(coll[it_msg->second.rank][it_msg->second.index]);
                if (coll_set.count(evt))
                    continue;
                coll_set.insert(evt);
                QStringList itemList;
                itemList.append(global_func_id.name_by_idx(evt->func_id)); // itemDescriptionString.str().c_str());
                QTreeWidgetItem *func_name = new QTreeWidgetItem((QTreeWidget *)0, itemList);
                QFont font1 = func_name->font(1);
                font1.setBold(true);
                func_name->setFont(1, font1);
                collective->addTopLevelItem(func_name);
                for (std::map<PEAT_Rank, TV_Event *>::const_iterator evt_it = evt->events.cbegin(); evt_it != evt->events.cend(); ++evt_it) {
                    QTreeWidgetItem *func_prop = new QTreeWidgetItem(func_name);
                    func_prop->setText(1, QString("%1").arg(evt_it->second->rank));
                    func_prop->setText(2, QString("%1").arg(evt_it->second->ids[0]));
                    if (user_regions) {
                        QString tt;
                        QString ttf;
                        QString source_file;
                        int source_line = -1;
                        QVariantMap userData;

                        for (int addr_num = 0; addr_num < trace_info.num_addrs; addr_num++) {
                            const PEAT_UID addr = evt_it->second->addrs->addr[addr_num];
                            std::ostringstream item_object_address;
                            User_Address_Info *addr_info = user_regions->get_addr_by_id(evt_it->second->rank, addr);
                            item_object_address << addr_info->full_path << "+0x" << std::hex << addr_info->addr_offset << "\n"
                                              << addr_info->source << ":" << std::dec << addr_info->line << "\n";
                            tt = tt + item_object_address.str().c_str();
                            source_file = QString::fromStdString(addr_info->source);
                            source_line = addr_info->line;
                            QString file_key = "src_file" + QString::number(addr_num);
                            QString line_key = "src_line" + QString::number(addr_num);
                            if (!source_file.isEmpty() && source_line >= 0) {
                                userData[file_key] = source_file;
                                userData[line_key] = source_line;
                            }
                        }
                        userData["func_name"] = global_func_id.name_by_idx(evt_it->second->func_id);
                        func_prop->setData(2, Qt::UserRole, userData);
                        func_prop->setToolTip(2, tt);
                        std::ostringstream coll_prop;
                        coll_prop << "Root: " << evt_it->second->get_root() << "\nSend: ";
                        static_cast<TV_Collective_Event *>(evt_it->second)->print_tx_bytes(coll_prop);
                        coll_prop << "Recv: ";
                        static_cast<TV_Collective_Event *>(evt_it->second)->print_rx_bytes(coll_prop);
                        ttf = coll_prop.str().c_str();
                        // Word-wrap ttf to make tooltip more square
                        {
                            int target_width = (int)sqrt((double)ttf.length());
                            QString wrapped_ttf;
                            int lineLen = 0;
                            for (int ci = 0; ci < ttf.length(); ci++) {
                                wrapped_ttf += ttf[ci];
                                lineLen++;
                                if (lineLen >= target_width && ttf[ci] == ' ') {
                                    wrapped_ttf += '\n';
                                    lineLen = 0;
                                }
                            }
                            func_prop->setToolTip(1, wrapped_ttf);
                        }
                    }
                }
            } else {
                QTableWidget *tw = NULL;
                switch (it_msg->second.msg_type) {
                case TV_PEN_MESSAGE_TYPE_NORMAL:
                case TV_PEN_MESSAGE_TYPE_REVERSED:
                    tw = p2p;
                    break;
                case TV_PEN_MESSAGE_TYPE_INITIATION:
                    tw = initiation;
                    break;
                case TV_PEN_MESSAGE_TYPE_COMPLETION:
                    tw = completion;
                    break;
                case TV_PEN_MESSAGE_TYPE_GPU_HD_TRANSFER:
                case TV_PEN_MESSAGE_TYPE_GPU_EXEC:
                    tw = gpu;
                    break;
                }
                tw->setRowCount(tw->rowCount() + 1);
                // Name (from)
                QTableWidgetItem *i_from_name = new QTableWidgetItem(global_func_id.name_by_idx(from->func_id));
                tw->setItem(tw->rowCount() - 1, 0, i_from_name);
                // Rank (from)
                QTableWidgetItem *i_from_rank = new QTableWidgetItem();
                i_from_rank->setData(Qt::DisplayRole, from->rank);
                i_from_rank->setTextAlignment(Qt::AlignHCenter);
                tw->setItem(tw->rowCount() - 1, 1, i_from_rank);
                // Uniq Idx (from)
                QTableWidgetItem *i_from_idx = new QTableWidgetItem();
                i_from_idx->setData(Qt::DisplayRole, from->ids[0]);
                i_from_idx->setTextAlignment(Qt::AlignHCenter);
                tw->setItem(tw->rowCount() - 1, 2, i_from_idx);
                // Time In (from)
                QTableWidgetItem *i_from_in = new QTableWidgetItem(outputDouble(from->e_time.func.in));
                i_from_in->setTextAlignment(Qt::AlignRight);
                if (it_msg->second.msg_type == TV_PEN_MESSAGE_TYPE_REVERSED) {
                    i_from_in->setBackground(failed_msg);
                }
                tw->setItem(tw->rowCount() - 1, 3, i_from_in);
                // Name (to)
                QTableWidgetItem *i_to_name = new QTableWidgetItem(global_func_id.name_by_idx(to->func_id));
                tw->setItem(tw->rowCount() - 1, 4, i_to_name);
                // Rank (to)
                QTableWidgetItem *i_to_rank = new QTableWidgetItem();
                i_to_rank->setData(Qt::DisplayRole, to->rank);
                i_to_rank->setTextAlignment(Qt::AlignHCenter);
                tw->setItem(tw->rowCount() - 1, 5, i_to_rank);
                // UNiq Idx (to)
                QTableWidgetItem *i_to_idx = new QTableWidgetItem();
                i_to_idx->setData(Qt::DisplayRole, to->ids[0]);
                i_to_idx->setTextAlignment(Qt::AlignHCenter);
                tw->setItem(tw->rowCount() - 1, 6, i_to_idx);
                // Time Out (to)
                QTableWidgetItem *i_to_out = new QTableWidgetItem(outputDouble(to->e_time.func.out));
                i_to_out->setTextAlignment(Qt::AlignRight);
                tw->setItem(tw->rowCount() - 1, 7, i_to_out);
                // Duration
                if (it_msg->second.msg_type == TV_PEN_MESSAGE_TYPE_REVERSED || it_msg->second.msg_type == TV_PEN_MESSAGE_TYPE_NORMAL) {
                    QTableWidgetItem *i_duration = new QTableWidgetItem(outputDouble(to->e_time.func.out - from->e_time.func.in));
                    i_duration->setTextAlignment(Qt::AlignRight);
                    tw->setItem(tw->rowCount() - 1, 8, i_duration);
                    // transfer rate
                    // qDebug() << freq;
                    // QTableWidgetItem *iRate = new QTableWidgetItem(QString().setNum(PEAT_Ratio(fromPtr.size) / PEAT_Ratio(toPtr.tscOut - fromPtr.tscIn) * freq));
                    QTableWidgetItem *i_rate = new QTableWidgetItem(outputDouble((PEAT_Ratio(to->e_time.func.out - from->e_time.func.in) / trace_info.freq) * 1000000));
                    i_rate->setTextAlignment(Qt::AlignRight);
                    tw->setItem(tw->rowCount() - 1, 9, i_rate);
                    // Buf Size (send function buffer based)
                    QTableWidgetItem *i_buf_size = new QTableWidgetItem(outputDouble(std::max(it_msg->second.from->get_tx_bytes(), it_msg->second.to->get_rx_bytes())));
                    i_buf_size->setTextAlignment(Qt::AlignRight);
                    tw->setItem(tw->rowCount() - 1, 10, i_buf_size);
                }
                if (user_regions) {
                    QString tt;
                    QString source_file;
                    int source_line = -1;
                    QVariantMap userData;

                    for (int addr_num = 0; addr_num < trace_info.num_addrs; addr_num++) {
                        const PEAT_UID addr = from->addrs->addr[addr_num];
                        std::ostringstream item_object_address;
                        User_Address_Info *addr_info = user_regions->get_addr_by_id(from->rank, addr);
                        item_object_address << addr_info->full_path << "+0x" << std::hex << addr_info->addr_offset << "\n"
                                            << addr_info->source << ":" << std::dec << addr_info->line << "\n";
                        tt = tt + item_object_address.str().c_str();
                        source_file = QString::fromStdString(addr_info->source);
                        source_line = addr_info->line;
                        QString file_key = "src_file" + QString::number(addr_num);
                        QString line_key = "src_line" + QString::number(addr_num);
                        if (!source_file.isEmpty() && source_line >= 0) {
                            userData[file_key] = source_file;
                            userData[line_key] = source_line;
                        }
                    }
                    userData["func_name"] = global_func_id.name_by_idx(from->func_id);
                    i_from_idx->setData(Qt::UserRole, userData);
                    i_from_idx->setToolTip(tt);
                    tt.clear();
                    userData.clear();
                    for (int addr_num = 0; addr_num < trace_info.num_addrs; addr_num++) {
                        const PEAT_UID addr = to->addrs->addr[addr_num];
                        std::ostringstream item_object_address;
                        User_Address_Info *addr_info = user_regions->get_addr_by_id(to->rank, addr);
                        item_object_address << addr_info->full_path << "+0x" << std::hex << addr_info->addr_offset << "\n"
                                            << addr_info->source << ":" << std::dec << addr_info->line << "\n";
                        tt = tt + item_object_address.str().c_str();
                        source_file = QString::fromStdString(addr_info->source);
                        source_line = addr_info->line;
                        QString file_key = "src_file" + QString::number(addr_num);
                        QString line_key = "src_line" + QString::number(addr_num);
                        if (!source_file.isEmpty() && source_line >= 0) {
                            userData[file_key] = source_file;
                            userData[line_key] = source_line;
                        }
                    }
                    userData["func_name"] = global_func_id.name_by_idx(to->func_id);
                    i_to_idx->setData(Qt::UserRole, userData);
                    i_to_idx->setToolTip(tt);
                }
            }
        }
    }
    // resize tableView
    /*initiation->resizeRowsToContents();
    initiation->resizeColumnsToContents();
    completion->resizeRowsToContents();
    completion->resizeColumnsToContents();
    p2p->resizeRowsToContents();
    p2p->resizeColumnsToContents();
    gpu->resizeRowsToContents();
    gpu->resizeColumnsToContents();
    PEAT_Num sum_of_columns_width = 0;
    for(PEAT_Rank i = 0; i < tw->columnCount(); ++i){
        sum_of_columns_width += tw->columnWidth(i) + 1;
    }
    QRect rect = tw->geometry();
    //qDebug() << "RECT " << rect << " header " << tw->verticalHeader()->sizeHint().width() << " column " << sum_of_columns_width << " height" << height;
    rect.setWidth(tw->verticalHeader()->sizeHint().width() + sum_of_columns_width);
    PEAT_Num sum_of_rows_height = tw->rowCount() * tw->rowHeight(0) + tw->rowCount() + tw->horizontalHeader()->sizeHint().height();//assume all rows are the same
    if(sum_of_rows_height > height) {
        sum_of_rows_height = height;
    }
    rect.setHeight(sum_of_rows_height);
    tw->setGeometry(rect);
    tw->sortItems(1);
    */
    return;
}
void Elements::get_user_addresses_in_region(QTreeWidget *tw, const PEAT_Clock start, const PEAT_Clock stop, const PEAT_Num height) const
{
    // qDebug() << "getUserAddressesAtPoint " << p;
    Q_ASSERT(tw);
    QStringList column_names;
    column_names << "Rank" << "Functions" << "Idx/Source" << "Size/Address";
    tw->setColumnCount(column_names.size());
    tw->setHeaderLabels(column_names);
    // tw->setMaximumHeight(8192);
    // tw->setSortingEnabled(true);
    tw->setWindowTitle("User addresses in region");
    PEAT_Rank r = 0;
#ifdef _DEBUG
    PEAT_UID regions_added = 0;
#endif
    for (std::vector<std::vector<TV_Event *>>::const_iterator it_rank = eventsV.begin(); it_rank != eventsV.end(); ++it_rank, ++r) {
        if (it_rank->empty())
            continue;
        EventRankTypeConstIter it_func_previous = lower_bound(*it_rank, start);
        ;
        if (it_func_previous != it_rank->begin()) {
            --it_func_previous;
        }
        for (EventRankTypeConstIter it_func = it_func_previous; it_func != it_rank->end(); ++it_func) {
            EventRankTypeConstIter it_func_next = it_func;
            if (it_func_next != it_rank->end()) {
                ++it_func_next;
            }
            // qDebug() << "Rank " << it_rank->first << " IDX prev " << it_func_previous->second.idx << " next " << it_func_next->second.idx;
            const TV_Event *cur_func_prev = *it_func;
            const TV_Event *cur_func_next = *it_func_next;
            while (cur_func_prev == NULL && it_func != it_rank->end())
                cur_func_prev = *(--it_func);
            while (cur_func_next == NULL && it_func_next != it_rank->end())
                cur_func_next = *(++it_func_next);
            if (cur_func_prev == NULL || cur_func_next == NULL)
                continue;
            if (cur_func_prev->e_time.func.in > stop)
                break;
            if (cur_func_prev->is_nested())
                cur_func_prev = find_event(r, cur_func_prev->ids[0]);
            if (cur_func_next->is_nested())
                cur_func_next = find_event(r, cur_func_next->ids[0] + 1);
            // const StatisticReader &rankStatLocal = ranksStat.at(it_rank->first);
#ifdef _DEBUG
            regions_added++;
#endif

            QStringList itemList1, itemList2;
            itemList1.append(QString("%1").arg(r));
            itemList1.append(global_func_id.name_by_idx(cur_func_prev->func_id)); // itemDescriptionString.str().c_str());
            itemList1.append(QString("%1").arg(cur_func_prev->ids[0]));
            itemList1.append(QString("%1/%2").arg(cur_func_prev->get_tx_bytes()).arg(cur_func_prev->get_rx_bytes()));
            itemList2.append("");
            itemList2.append(global_func_id.name_by_idx(cur_func_next->func_id)); // itemDescriptionString.str().c_str());
            itemList2.append(QString("%1").arg(cur_func_next->ids[0]));
            itemList2.append(QString("%1/%2").arg(cur_func_next->get_tx_bytes()).arg(cur_func_next->get_rx_bytes()));
            QTreeWidgetItem *func_name_prev = new QTreeWidgetItem((QTreeWidget *)0, itemList1);
            QFont font1 = func_name_prev->font(1);
            font1.setBold(true);
            func_name_prev->setFont(1, font1);
            QTreeWidgetItem *func_name_next = new QTreeWidgetItem((QTreeWidget *)0, itemList2);
            QFont font2 = func_name_next->font(1);
            font2.setBold(true);
            func_name_next->setFont(1, font2);
            tw->addTopLevelItem(func_name_prev);
            if (user_regions) {
                for (int addr_num = 0; addr_num < trace_info.num_addrs; addr_num++) {
                    const PEAT_UID addr = cur_func_prev->addrs->addr[addr_num];
                    std::ostringstream item_object_address, item_source_line;
                    User_Address_Info *addr_info = user_regions->get_addr_by_id(cur_func_prev->rank, addr);
                    item_object_address << addr_info->full_path << "+0x" << std::hex << addr_info->addr_offset;
                    item_source_line << addr_info->source << ":" << std::dec << addr_info->line;
                    QStringList qItemChild1;
                    qItemChild1.append("");
                    qItemChild1.append(addr_info->func.c_str());
                    qItemChild1.append(item_source_line.str().c_str());
                    qItemChild1.append(item_object_address.str().c_str());
                    QTreeWidgetItem *i_child = new QTreeWidgetItem((QTreeWidget *)0, qItemChild1);
                    func_name_prev->addChild(i_child);
                }
                func_name_prev->addChild(func_name_next);
                for (int addr_num = 0; addr_num < trace_info.num_addrs; addr_num++) {
                    const PEAT_UID addr = cur_func_next->addrs->addr[addr_num];
                    std::ostringstream item_object_address, item_source_line;
                    User_Address_Info *addr_info = user_regions->get_addr_by_id(cur_func_next->rank, addr);
                    item_object_address << addr_info->full_path << "+0x" << std::hex << addr_info->addr_offset;
                    item_source_line << addr_info->source << ":" << std::dec << addr_info->line;
                    QStringList qItemChild1;
                    qItemChild1.append("");
                    qItemChild1.append(addr_info->func.c_str());
                    qItemChild1.append(item_source_line.str().c_str());
                    qItemChild1.append(item_object_address.str().c_str());
                    QTreeWidgetItem *i_child = new QTreeWidgetItem((QTreeWidget *)0, qItemChild1);
                    func_name_prev->addChild(i_child);
                }
            }
        }
    }
    // resize
    PEAT_Num sum_of_columns_width = 0;
    int cc = tw->columnCount();
    for (int i = 0; i < cc; ++i) {
        tw->resizeColumnToContents(i);
        tw->header()->setSectionResizeMode(i, QHeaderView::ResizeToContents);
        sum_of_columns_width += tw->columnWidth(i) + 1;
    }
    QRect rect = tw->geometry();
    // qDebug() << "RECT " << rect << " header " << tw->verticalHeader()->sizeHint().width() << " column " << sum_of_columns_width << " height" << height;
    rect.setWidth(sum_of_columns_width);
    tw->setGeometry(rect);
    // tw->sortColumn();
}
TV_Event *Elements::get_event(int x, int y, int width) const
{
    // Implement the logic to generate the tooltip based on the x and y coordinates
    // simple linear search at the moment, can be optimized later
    int rank = -1;
    int thread = -1;
    int selector = -1;
    for (int r = 0; r < ranks_to_load_v.size(); ++r) {
        long rank_top = paint.mpi_ulc.at(r);
        long pix_in_rank = (get_mpi_threads(r) + get_omp_threads(r) + get_gpu_threads(r)) * paint.thread_height;
        if (y >= rank_top && y < rank_top + pix_in_rank) {
            if (y > paint.gpu_ulc.at(r)) {
                selector = 2;
                thread = (y - paint.gpu_ulc.at(r)) / paint.thread_height;
            } else if (y > paint.omp_ulc.at(r)) {
                selector = 1;
                thread = (y - paint.omp_ulc.at(r)) / paint.thread_height;
            } else if (y > paint.mpi_ulc.at(r)) {
                selector = 0;
                thread = (y - paint.mpi_ulc.at(r)) / paint.thread_height;
            }
            rank = ranks_to_load_v.at(r);
            break;
        }
    }
    PEAT_Clock clk = convert_mouse_to_ticks(x, 0, width);
    if (rank < 0)
        return NULL;
    return (find_event(rank, clk, selector, thread));
}

PEAT_Bool Elements::get_event(const PEAT_Rank rank, File_Storage_GZ *input_MPI, File_Storage_GZ *input_HWC, PEAT_Clock min_clock, PEAT_Clock max_clock, TV_Event **event, PTP *ptp)
{
    // PEAT_Bool possible_colt_included = false;
    *event = NULL;
#ifdef _OPENMP
    String_Split &trace_line_MPI = trace_line_MPIs[omp_get_thread_num()];
    String_Split &trace_line_HWC = trace_line_HWCs[omp_get_thread_num()];
#endif
    trace_line_MPI.clear();
    trace_line_HWC.clear();

    PEAT_Bool m = input_MPI->read_lines(trace_line_MPI, rank, ':', ';');
    PEAT_Bool h = input_HWC->read_lines(trace_line_HWC, rank, ':', ';');
    if (!(m || h))
        return false;
    if (trace_line_HWC.size() < 2) {
        char tmp[256];
        sprintf(tmp, "Not enough fields(%lu) on rank:%d\n", trace_line_HWC.size(), rank);
        peat_warning(tmp);
        return false;
    }
    if (trace_line_MPI.size() < 2) {
        char tmp[256];
        sprintf(tmp, "Not enough fields(%lu) on rank:%d\n", trace_line_MPI.size(), rank);
        peat_warning(tmp);
        return false;
    }
    PEAT_Record_Header hdr; // ids, thread and nesting of the record
    user_regions->parse_record_header(hdr, trace_line_MPI, rank);
    const int b = hdr.payload;      // function name on the .mpi line
    const int hb = hdr.hwc_payload; // first tsc on the .hwc line
    PEAT_MPI_Func_ID func_id = TMPI_FUNC_ID_NONE;
    extract(func_id, trace_line_MPI.at(b), 10, false);
    if (func_id == 0) {
        func_id = global_func_id.idx_by_name(trace_line_MPI.at(b));
    }
    PEAT_Time evt_time;
    PEAT_Clock func_start = 0, func_stop = 0;
    if (ptp) {
        extract(evt_time.func.in, trace_line_HWC.at(hb), 10);
        extract(evt_time.mpi.in, trace_line_HWC.at(hb + 1), 10);
        extract(evt_time.mpi.out, trace_line_HWC.at(hb + 2), 10);
        extract(evt_time.func.out, trace_line_HWC.at(hb + 3), 10);
        evt_time.mpi.out = evt_time.func.out - evt_time.mpi.out;

        evt_time.func.in += tsc_predictor.at(rank);
        tsc_predictor[rank] = evt_time.func.in;

        evt_time.mpi.in += evt_time.func.in;
        evt_time.mpi.out += evt_time.func.in;
        evt_time.func.out += evt_time.func.in;
        evt_time.func.in = ptp->get_real_time(evt_time.func.in, rank);
        evt_time.func.out = ptp->get_real_time(evt_time.func.out, rank);
        evt_time.mpi.in = ptp->get_real_time(evt_time.mpi.in, rank);
        evt_time.mpi.out = ptp->get_real_time(evt_time.mpi.out, rank);
        func_start = evt_time.func.in;
        func_stop = evt_time.func.out;
    } else {
        extract(func_start, trace_line_HWC.at(hb), 10, false);
        extract(func_stop, trace_line_HWC.at(hb + 1), 10, false);
    }
    if (func_stop < min_clock)
        return true;
    if (func_start > max_clock)
        return false;

    if (func_id >= MPI_START_FILE_GROUP && func_id < MPI_END_FILE_GROUP) {
        *event = new TV_Local_Event();
    } else if (func_id >= MPI_START_OSC_GROUP && func_id < MPI_END_OSC_GROUP) {
        *event = new TV_OSC_Event();
    } else if (func_id >= MPI_START_BUFFERED_LOCAL_GROUP && func_id < MPI_END_BUFFERED_LOCAL_GROUP) {
        *event = new TV_Local_Buf_Event();
    } else if (func_id >= MPI_START_COMM_COLLECTIVE_GROUP &&
                   func_id < MPI_END_COMM_COLLECTIVE_GROUP ||
               func_id == MPI_COMM_FREE_ID || func_id == MPID_COMM_CREATE_ID || func_id == MPID_COMM_FREE_ID) { // tractor writes the same comm payload for the mpi-internal create/free
        *event = new TV_Comm_Event();
        // possible_colt_included = true;
    } else if (func_id >= MPI_P2P_TRANSFER_START && func_id < MPI_P2P_TRANSFER_END ||
               func_id >= MPI_P2P_PERSISTENT_INIT_START && func_id < MPI_P2P_PERSISTENT_INIT_END ||
               func_id >= MPI_PROBE_START && func_id < MPI_PROBE_END ||
               func_id >= MPI_P2P_COLL_TRANSFER_START && func_id < MPI_P2P_COLL_TRANSFER_END ||
               func_id >= MPI_P2P_COLL_PROBE_START && func_id < MPI_P2P_COLL_PROBE_END) {
        if (func_id != MPI_SENDRECV_ID && func_id != MPI_SENDRECV_REPLACE_ID && func_id != MPIC_SENDRECV_ID && func_id != MPIC_SENDRECV_REPLACE_ID) {
            *event = new TV_ToR_Event();
        } else {
            *event = new TV_TaR_Event();
        }
    } else if (func_id >= MPI_REQUEST_START && func_id < MPI_REQUEST_END ||
               func_id >= MPI_MISC_REQUEST_START && func_id < MPI_MISC_REQUEST_END ||
               func_id >= MPI_P2P_COLL_REQUEST_START && func_id < MPI_P2P_COLL_REQUEST_END) {
        if (func_id == MPI_STARTALL_ID || func_id == MPI_START_ID) {
            *event = new TV_Start_Event();
        } else {
            *event = new TV_Completion_Event();
            // possible_colt_included = true;
        }
    } else if (func_id >= MPI_START_GENERAL_COLLECTIVE_GROUP && func_id < MPI_END_GENERAL_COLLECTIVE_GROUP) {
        *event = new TV_Collective_Event();
        // possible_colt_included = true;
    } else if (func_id >= CL_START_GROUP && func_id < CL_END_GROUP || func_id >= ZE_START_GROUP && func_id < ZE_END_GROUP) {
        *event = new TV_GPU_Event();
    } else if (func_id >= OMPT_START_GROUP && func_id < OMPT_END_GROUP) {
        *event = new TV_OMPT_Event();
    } else {
        *event = new TV_Local_Event();
    }

    if (!(*event)->read_event(rank, trace_line_MPI, trace_line_HWC, evt_time, trace_info.num_addrs, hdr)) {
        delete *event;
        *event = NULL;
        return true;
    }
    if ((*event)->func_id == OMPT_PARALLEL_BEGIN_ID) {
        psection[rank].push_back(*event);
        TV_Event *evt = psection[rank].back();
        evt->func_id = OMPT_PARALLEL_ID;
    }
    if ((*event)->func_id == OMPT_PARALLEL_END_ID) {
        TV_Event *evt = psection[rank].back();
        // assert(evt);

        evt->e_time.func.out = (*event)->e_time.func.in;
#ifndef PEAT_NO_MPI_TIME
        evt->e_time.mpi.out = (*event)->e_time.mpi.in;
#endif
        evt->e_time.pe.out = (*event)->e_time.pe.in;
#if 0
        for (PEAT_UID idx = evt->ids[0]; idx < (*event)->ids[0]; idx++) {
            TV_Event* evt = eventsV[rank][idx];
            if (evt) {
                if ((evt->func_id == OMPT_SYNC_REGION_ID || evt->func_id == OMPT_SYNC_REGION_WAIT_ID) &&
                    (evt->thread == (*event)->thread || (*event)->thread == 0)) {
                    evt->e_time.func.out = (*event)->e_time.func.in;
#ifndef PEAT_NO_MPI_TIME
                    evt->e_time.mpi.out = (*event)->e_time.mpi.in;
#endif
                    evt->e_time.pe.out = (*event)->e_time.func.in;
                }
            }
        }
#endif
        psection[rank].pop_back();
        delete (*event);
        *event = NULL;
        return true;
    }

    if (func_id == MPI_SENDRECV_ID || func_id == MPI_SENDRECV_REPLACE_ID || func_id == MPIC_SENDRECV_ID || func_id == MPIC_SENDRECV_REPLACE_ID) {
        input_MPI->read_lines(trace_line_MPI, rank, ':', ';');
        input_HWC->read_lines(trace_line_HWC, rank, ':', ';');
        (*event)->read_event(rank, trace_line_MPI, trace_line_HWC, evt_time, trace_info.num_addrs, hdr);
    }
    return true;
}

PEAT_Bool Elements::find_coll_most_imbalance()
{
    candidates.clear();
    for (std::vector<std::vector<Collective>>::iterator rank_it = coll.begin(); rank_it != coll.end(); ++rank_it) {
        for (std::vector<Collective>::iterator coll_it = rank_it->begin(); coll_it != rank_it->end(); ++coll_it) {
#if 0
            PEAT_Clock min_start = LLONG_MAX, max_start = 0, min_end = LLONG_MAX, max_end = 0;
            double max_coll_clock = 0;
            for (std::map<PEAT_Rank, TV_Event*>::iterator event_it = coll_it->events.begin(); event_it != coll_it->events.end(); ++event_it) {
                min_start = min_start > event_it->second->e_time.func.in ? event_it->second->e_time.func.in : min_start;
                max_start = max_start < event_it->second->e_time.func.in ? event_it->second->e_time.func.in : max_start;
                min_end = min_end > event_it->second->e_time.func.out ? event_it->second->e_time.func.out : min_end;
                max_end = max_end < event_it->second->e_time.func.out ? event_it->second->e_time.func.out : max_end;
                max_coll_clock = max_coll_clock < event_it->second->e_time.func.duration() ? event_it->second->e_time.func.duration() : max_coll_clock;
            }
            PEAT_Viewport v(min_start, max_end);
            double coll_imb = max_end - min_end + max_start - min_start;
            while (candidates.count(max_coll_clock / coll_imb)) max_coll_clock++;
            candidates[max_coll_clock / coll_imb] = v;
#else
            PEAT_Clock min_start = LLONG_MAX, max_end = 0;
            double max_coll_clock = 0;
            double min_coll_clock = LLONG_MAX;
            for (std::map<PEAT_Rank, TV_Event *>::iterator event_it = coll_it->events.begin(); event_it != coll_it->events.end(); ++event_it) {
                min_start = min_start > event_it->second->e_time.func.in ? event_it->second->e_time.func.in : min_start;
                max_end = max_end < event_it->second->e_time.func.out ? event_it->second->e_time.func.out : max_end;
                min_coll_clock = min_coll_clock > event_it->second->e_time.func.duration() ? event_it->second->e_time.func.duration() : min_coll_clock;
                max_coll_clock = max_coll_clock < event_it->second->e_time.func.duration() ? event_it->second->e_time.func.duration() : max_coll_clock;
            }
            PEAT_Viewport v(min_start, max_end);
            while (candidates.count(min_coll_clock / max_coll_clock))
                max_coll_clock++;
            candidates[min_coll_clock / max_coll_clock] = v;
#endif
            if (candidates.size() > MAX_SIZE)
                candidates.erase(prev(candidates.end()));
        }
    }
    if (!candidates.empty()) {
        candidates_it = candidates.begin();
        zoom_time_start = candidates_it->second.start;
        zoom_time_stop = candidates_it->second.end;
        return true;
    }
    return false;
}
PEAT_Bool Elements::find_coll_least_bw(PEAT_Message_Size cutoff)
{
    candidates.clear();
    for (std::vector<std::vector<Collective>>::iterator rank_it = coll.begin(); rank_it != coll.end(); ++rank_it) {
        for (std::vector<Collective>::iterator coll_it = rank_it->begin(); coll_it != rank_it->end(); ++coll_it) {
            PEAT_Clock min_start = LLONG_MAX, max_start = 0, min_end = LLONG_MAX, max_end = 0;
            PEAT_Clock min_duration = LLONG_MAX;
            PEAT_Message_Size size = 0;
            PEAT_Rank ranks = 0;
            for (std::map<PEAT_Rank, TV_Event *>::iterator event_it = coll_it->events.begin(); event_it != coll_it->events.end(); ++event_it) {
                size += event_it->second->get_tx_bytes(event_it->first) + event_it->second->get_rx_bytes(event_it->first);
                ranks++;
                min_start = min_start > event_it->second->e_time.func.in ? event_it->second->e_time.func.in : min_start;
                max_start = max_start < event_it->second->e_time.func.in ? event_it->second->e_time.func.in : max_start;
                min_end = min_end > event_it->second->e_time.func.out ? event_it->second->e_time.func.out : min_end;
                max_end = max_end < event_it->second->e_time.func.out ? event_it->second->e_time.func.out : max_end;
            }
            if (ranks > 0)
                size /= ranks;
            if (size < cutoff)
                continue;
            double bw = (double)size / (double)(max_end - min_start);
            PEAT_Viewport v(min_start, max_end);
            candidates[bw] = v;
            if (candidates.size() > MAX_SIZE)
                candidates.erase(prev(candidates.end()));
        }
    }
    if (!candidates.empty()) {
        candidates_it = candidates.begin();
        zoom_time_start = candidates_it->second.start;
        zoom_time_stop = candidates_it->second.end;
        return true;
    }
    return false;
}
PEAT_Bool Elements::find_p2p_most_imbalance()
{
    PEAT_Clock mess_clock = 0;
    PEAT_Clock start, end;
    candidates.clear();
    for (std::vector<std::map<PEAT_UID, TV_Message>>::iterator rank_it = messages.begin(); rank_it != messages.end(); ++rank_it) {
        for (std::map<PEAT_UID, TV_Message>::iterator mess_it = rank_it->begin(); mess_it != rank_it->end(); ++mess_it) {
            if (mess_it->second.msg_type != TV_PEN_MESSAGE_TYPE_NORMAL)
                continue;
            PEAT_Clock diff, lstart, lend;

            if (mess_it->second.from->e_time.func.out < mess_it->second.to->e_time.func.in)
                continue; // eager message - no imbalance

            diff = std::abs(mess_it->second.from->e_time.func.in - mess_it->second.to->e_time.func.in);

            lstart = std::min(mess_it->second.from->e_time.func.in, mess_it->second.to->e_time.func.in);
            lend = std::max(mess_it->second.from->e_time.func.out, mess_it->second.to->e_time.func.out);
            PEAT_Viewport v(lstart, lend);
            candidates[1.0 / (double)diff] = v;
            if (candidates.size() > MAX_SIZE)
                candidates.erase(prev(candidates.end()));
        }
    }
    if (!candidates.empty()) {
        candidates_it = candidates.begin();
        zoom_time_start = candidates_it->second.start;
        zoom_time_stop = candidates_it->second.end;
        return true;
    }
    return false;
}
PEAT_Bool Elements::find_p2p_least_bw(PEAT_Message_Size cutoff)
{
    double min_bw = (double)ULLONG_MAX;
    PEAT_Clock start, end;
    candidates.clear();
    for (std::vector<std::map<PEAT_UID, TV_Message>>::iterator rank_it = messages.begin(); rank_it != messages.end(); ++rank_it) {
        for (std::map<PEAT_UID, TV_Message>::iterator mess_it = rank_it->begin(); mess_it != rank_it->end(); ++mess_it) {
            if (mess_it->second.msg_type != TV_PEN_MESSAGE_TYPE_NORMAL)
                continue;
            PEAT_Clock send_duration, recv_duration;
            PEAT_Clock lstart, lend;
            if (mess_it->second.from->get_tx_bytes() < cutoff)
                continue;
            send_duration = mess_it->second.from->e_time.func.out - mess_it->second.from->e_time.func.in;
            recv_duration = mess_it->second.to->e_time.func.out - mess_it->second.to->e_time.func.in;
            double bw = (double)mess_it->second.from->get_tx_bytes() / (double)std::min(mess_it->second.from->e_time.func.in, mess_it->second.to->e_time.func.in);

            lstart = std::min(mess_it->second.from->e_time.func.in, mess_it->second.to->e_time.func.in);
            lend = std::max(mess_it->second.from->e_time.func.out, mess_it->second.to->e_time.func.out);
            PEAT_Viewport v(lstart, lend);
            candidates[bw] = v;
            if (candidates.size() > MAX_SIZE)
                candidates.erase(prev(candidates.end()));
        }
    }
    if (!candidates.empty()) {
        candidates_it = candidates.begin();
        current_candidate = 0;
        zoom_time_start = candidates_it->second.start;
        zoom_time_stop = candidates_it->second.end;
        return true;
    }
    return false;
}
void Elements::move_viewport(int direction)
{
    if (candidates.empty())
        return;
    if (candidates_it == candidates.end())
        return;
    if (direction == 0)
        return;
    if (direction > 0) {
        ++candidates_it;
        ++current_candidate;
        if (candidates_it == candidates.end()) {
            candidates_it = candidates.begin();
            current_candidate = 0;
        }
        zoom_time_start = (qreal)candidates_it->second.start;
        zoom_time_stop = (qreal)candidates_it->second.end;
    }
    if (direction < 0) {
        --candidates_it;
        --current_candidate;
        if (candidates_it == candidates.end()) {
            candidates_it = prev(candidates.end());
            current_candidate = candidates.size();
        }
        zoom_time_start = (qreal)candidates_it->second.start;
        zoom_time_stop = (qreal)candidates_it->second.end;
    }
}

QDebug operator<<(QDebug dbg, const Elements &me)
{
    for (std::vector<std::vector<TV_Event *>>::const_iterator it_rank = me.eventsV.begin(); it_rank != me.eventsV.end(); ++it_rank) {
        for (Elements::EventRankTypeConstIter it = it_rank->begin(); it != it_rank->end(); ++it) {
            const TV_Event *cur_func_ptr = *it;
            dbg << "[" << cur_func_ptr->rank << "] " << "IDX:" << cur_func_ptr->ids[0] << " " << cur_func_ptr << "\n";
        }
    }
    dbg << "\nWe have " << me.fsize() << " messages in " << me.get_total_rank_count() << " ranks. Max time is " << me.model_time_stop;
    return dbg;
}
