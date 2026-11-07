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

#include "mat.h"
#include <string>
#ifdef PEAT_MPI
#include <mpi.h>
#endif
#ifdef _WIN32
#include <synchapi.h>
#else
#include <unistd.h>
#endif

#define matout          \
    if (globals.verbose) \
    std::cout
#define matdbg(x)           \
    if (globals.verbose > x) \
    std::cerr

PEAT_Bool Mat::parse_single_event(const int rank)
{
    Mat_Worker &w = worker();
    w.line_mpi.clear();
    w.line_hwc.clear();
    if (!input_MPI->is_file_good(rank) || (!globals.matching_only && !input_HWC->is_file_good(rank))) {
        return false;
    }
    input_MPI->read_lines(w.line_mpi, rank, ':');
    if (!globals.matching_only) {
        input_HWC->read_lines(w.line_hwc, rank, ':');
    }
    // matdbg(0) << "P" << rank << std::endl;
    if (w.line_mpi.size() < 2) {
        return false;
    }
    Tractor_Event t_event(rank, w.line_mpi, w.line_hwc, model_id.num_hwcs, model_id.num_addrs, user_region_store, comm_store, model_id);
    PEAT_MPI_Func_ID mpifunc = t_event.func_id;
    const bool nested_record = t_event.level > 0;
    PEAT_Clock mpi_time = 0, uc_time = 0, t_time = 0, occ_inc = 0;
    if (!globals.matching_only) { // tsc_time is all sentinel (-1) without hwc data, so these are meaningless
        if (!w.second_line) {
            t_event.tsc_time.func.in += tsc_predictor.at(rank);
            tsc_predictor[rank] = t_event.tsc_time.func.in;
            if(!nested_record) {
                mpi_time = t_event.tsc_time.mpi.out - t_event.tsc_time.mpi.in;
                if(t_event.func_id != MPI_INIT_ID && t_event.func_id != MPI_INIT_THREAD_ID && !is_omp(t_event.func_class)) {
                    uc_time = t_event.tsc_time.func.in - tsc_corrector[rank];
                } else {
                    t_time = 0;
                }
                if(t_event.thread_id == 0) {
                    tsc_corrector[rank] = t_event.tsc_time.func.in + t_event.tsc_time.func.out;
                }
                occ_inc = 1;
            }
            if(t_event.func_id != MPI_INIT_ID && t_event.func_id != MPI_INIT_THREAD_ID && t_event.func_id != MPI_FINALIZE_ID && t_event.thread_id == 0) {
                if(is_omp(t_event.func_class)) { // temporal clutch until fixed in tractor
                    if(t_event.func_id == OMPT_SYNC_REGION_ID || t_event.func_id == OMPT_SYNC_REGION_WAIT_ID) {
                        t_time = t_event.tsc_time.mpi.in + t_event.tsc_time.func.out - t_event.tsc_time.mpi.out;
                    } else {
                        t_time = t_event.tsc_time.mpi.in;
                    }
                } else {
                    t_time = t_event.tsc_time.mpi.in + t_event.tsc_time.func.out - t_event.tsc_time.mpi.out;
                }
            } else {
                t_time = 0;
            }
        }
        t_event.tsc_time.mpi.in += t_event.tsc_time.func.in;
        t_event.tsc_time.mpi.out += t_event.tsc_time.func.in;
        t_event.tsc_time.func.out += t_event.tsc_time.func.in;
        if (t_event.tsc_time.pe.in > 0)
            t_event.tsc_time.pe.in += t_event.tsc_time.func.in;
        if (t_event.tsc_time.pe.out > 0)
            t_event.tsc_time.pe.out += t_event.tsc_time.func.in;
        t_event.tsc_u_time = t_event.tsc_time; // update unmodified time

        /* update time according to ptpd or similar method */
        t_event.tsc_time.func.in = ptp->get_real_time(t_event.tsc_time.func.in, rank);
        t_event.tsc_time.func.out = ptp->get_real_time(t_event.tsc_time.func.out, rank);
        t_event.tsc_time.mpi.in = ptp->get_real_time(t_event.tsc_time.mpi.in, rank);
        t_event.tsc_time.mpi.out = ptp->get_real_time(t_event.tsc_time.mpi.out, rank);
        if (t_event.tsc_time.pe.in > 0)
            t_event.tsc_time.pe.in = ptp->get_real_time(t_event.tsc_time.pe.in, rank);
        if (t_event.tsc_time.pe.out > 0)
            t_event.tsc_time.pe.out = ptp->get_real_time(t_event.tsc_time.pe.out, rank);
    }
/* this section is not working as designed if --matching-only is enabled so some imbalance may occur */
    PEAT_Clock tsc_t = t_event.tsc_time.func.in;
    if (tsc_t < 0) {
        if (w.timeline.empty()) {
            tsc_t = 0;
        } else {
            std::map<PEAT_Clock, PEAT_Rank> ::const_reverse_iterator rit = w.timeline.crbegin();
            tsc_t = rit->first + 1;
        }
    }
    while (w.timeline.count(tsc_t) > 0)
        tsc_t++;
    w.timeline[tsc_t] = rank;
#ifdef PEAT_OMP
    // from here on the event goes into the stores every thread shares
    std::lock_guard<Mat_Match_Lock> matching(match_lock);
#endif

    if (mpifunc == MPI_PCONTROL_ID) {
        int ctl = t_event.PControl.ctl;
        int abs_ctl = ctl > 0 ? ctl : -ctl;
        if (pc_levels.count(abs_ctl) > 0) {
            p_control_in_use[rank] = ctl;
        } else {
            p_control_in_use[rank] = 0;
        }
    }
    t_event.PControl.ctl = p_control_in_use[rank];

    if (t_event.func_class == PEAT_PROP_CLASS_COLLECTIVE) { // collective
#ifdef PEAT_MPI
        coll_func_store->send_collective(rank, t_event);
#else
        coll_func_store->parse_collective(rank, t_event, false);
#endif
    }
    if (t_event.func_class == PEAT_PROP_CLASS_P2P) { // P2P
        if (global_func_id.check_property(mpifunc, PEAT_PROP_DATA_TXRX)) {
            if (w.second_line) {
                w.recv_line = t_event;
                w.recv_line.tsc_time = w.send_line.tsc_time;
                w.recv_line.tsc_u_time = w.send_line.tsc_u_time;
#ifdef PEAT_MPI
                p2p_func_store->send_P2PTx(rank, w.send_line);
#else
                p2p_func_store->parse_P2PTx(rank, w.send_line, true);
#endif
                p2p_func_store->parse_P2PRx(rank, w.recv_line, false);
                w.second_line = false;
            } else {
                w.send_line.PControl.ctl = p_control_in_use[rank];
                w.send_line = t_event;
                // p2p_func_store->parse_P2PTx(rank, w.send_line, true); // ???
                w.second_line = true;
                parse_single_event(rank);
            }
        } else {
            if (global_func_id.check_property(mpifunc, PEAT_PROP_DATA_TX)) { // send f.i
#ifdef PEAT_MPI
                p2p_func_store->send_P2PTx(rank, t_event);
#else
                p2p_func_store->parse_P2PTx(rank, t_event, true);
#endif
            } else {
                if (global_func_id.check_property(mpifunc, PEAT_PROP_DATA_RX)) { // recv f.i TODO must be the first in switch/case
                    p2p_func_store->parse_P2PRx(rank, t_event);
                }
            }
        }
    }
    if (t_event.func_class == PEAT_PROP_CLASS_PROBE) {
        p2p_func_store->parse_P2PPb(rank, t_event);
    }
    if (t_event.func_class == PEAT_PROP_CLASS_REQUEST) {
        request_func_store->parse_cn(rank, t_event, false);
    }

    if (t_event.func_class == PEAT_PROP_CLASS_FIO) {
#ifdef PEAT_MPI
        coll_func_store->send_collective(rank, t_event);
#else
        coll_func_store->parse_collective(rank, t_event, false);
#endif
    }

    if (t_event.func_class == PEAT_PROP_CLASS_BUF_LOCAL) {
        p2p_func_store->parse_Local(rank, t_event);
    }
    if (t_event.func_class == PEAT_PROP_CLASS_COMM) {
        if (global_func_id.check_property(mpifunc, PEAT_PROP_DATA_COLLECTIVE)) {
#ifdef PEAT_MPI
            coll_func_store->send_collective(rank, t_event);
#else
            coll_func_store->parse_collective(rank, t_event, false);
#endif
        } else {
            comm_func_store->parse_comm(rank, t_event);
        }
    }
    if (t_event.func_class == PEAT_PROP_CLASS_OSC) {
        osc_func_store->parse_osc(rank, t_event);   
    }

    if (t_event.func_class == PEAT_PROP_CLASS_GPU) {
        gpu_func_store->parse_GPU(rank, t_event);
    }
    if (t_event.func_class == PEAT_PROP_CLASS_LOCAL || t_event.func_class == PEAT_PROP_CLASS_OMPT) {
        p2p_func_store->parse_Local(rank, t_event);
    }
    if (p_control_in_use[rank] > 0 && occ_inc) {
        Mat_Time t = timings[p_control_in_use[rank]][t_event.func_class];
        t.durations += mpi_time;
        t.occurences += (mpi_time != 0);
        timings[p_control_in_use[rank]][t_event.func_class] = t;

        t = timings[p_control_in_use[rank]][PEAT_PROP_CODE_ALL];
        t.durations += mpi_time;
        t.occurences += (mpi_time != 0);
        timings[p_control_in_use[rank]][PEAT_PROP_CODE_ALL] = t;

        t = timings[p_control_in_use[rank]][PEAT_PROP_CODE_USER];
        t.durations += uc_time;
        t.occurences += (uc_time != 0);
        timings[p_control_in_use[rank]][PEAT_PROP_CODE_USER] = t;

        t = timings[p_control_in_use[rank]][PEAT_PROP_CODE_TRACTOR_OVERHEAD];
        t.durations += t_time;
        t.occurences += (t_time != 0);
        timings[p_control_in_use[rank]][PEAT_PROP_CODE_TRACTOR_OVERHEAD] = t;

    } else if (occ_inc) {
        Mat_Time t = timings[0][t_event.func_class];
        t.durations += mpi_time;
        t.occurences += (mpi_time != 0);
        timings[0][t_event.func_class] = t;

        t = timings[0][PEAT_PROP_CODE_ALL];
        t.durations += mpi_time;
        t.occurences += (mpi_time != 0);
        timings[0][PEAT_PROP_CODE_ALL] = t;

        t = timings[0][PEAT_PROP_CODE_USER];
        t.durations += uc_time;
        t.occurences += (uc_time != 0);
        timings[0][PEAT_PROP_CODE_USER] = t;

        t = timings[0][PEAT_PROP_CODE_TRACTOR_OVERHEAD];
        t.durations += t_time;
        t.occurences += (t_time != 0);
        timings[0][PEAT_PROP_CODE_TRACTOR_OVERHEAD] = t;
    }
    if (!w.second_line && (t_event.ids.size()<2 || t_event.ids[1] < 0)) { //count only 'user' events
        rank_events_processed[rank]++;
#pragma omp atomic
        events_processed++;
    }
    return true;
}
#ifdef PEAT_MPI
void Mat::print_remote_string(int size, int tag, int src)
{
#if defined(_WIN32) && !defined(__MINGW32__)
    char *buffer = (char *)_malloca(size);
#else
    char buffer[size];
#endif
    MPI_Status status;
    MPI_Recv(buffer, size, MPI_CHAR, src, tag, globals.comm_compute, &status);
    int *ibuf = (int *)buffer;
    int rank = *ibuf++;
    char *w_buffer = (char *)ibuf;
    if (tag == TRACTOR_P2P_PRINT_STRING_TAG || tag == TRACTOR_REQ_PRINT_STRING_TAG) {
        output_bind->puts(rank, w_buffer);
        p2p_func_store->messages_bound[rank]++;
    }
    if (tag == TRACTOR_COLL_PRINT_STRING_TAG) {
        coll_func_store->print_collective_bind_str(rank, w_buffer);
    }
}
PEAT_Num Mat::handle_incoming_messages()
{
    int flag = 0;
    MPI_Status status;
    do {
        MPI_Iprobe(MPI_ANY_SOURCE, MPI_ANY_TAG, globals.comm_compute, &flag, &status);
        if (flag) {
            switch (status.MPI_TAG) {
            case TRACTOR_P2P_EVENT_TAG:
            case TRACTOR_COLLECTIVE_EVENT_TAG:
            case TRACTOR_P2P_PRINT_STRING_TAG:
            case TRACTOR_REQ_PRINT_STRING_TAG:
            case TRACTOR_COLL_PRINT_STRING_TAG:
            case TRACTOR_REQUEST_TAG:
                messages_received++;
                break;
            default:
                // not a work message (PROFILE_WRITE_PERMISSION from a rank that already terminated): left for the
                // explicit receive waiting for it - probing on would find the same message forever
                return 0;
            }
            switch (status.MPI_TAG) {
            case RTS_TAG:
                assert(0);
                break;
            case CTS_TAG:
                assert(0);
                break;
            case TRACTOR_ADDRESS_TAG:
                assert(0);
                break;
            case TRACTOR_POINTER_TAG:
                assert(0);
                break;
            case TRACTOR_P2P_EVENT_TAG: {
                int size;
                MPI_Get_count(&status, MPI_CHAR, &size);
                p2p_func_store->process_remote_event(size, status.MPI_SOURCE);
            } break;
            case TRACTOR_COLLECTIVE_EVENT_TAG: {
                int size;
                MPI_Get_count(&status, MPI_CHAR, &size);
                coll_func_store->process_remote_collective(size, status.MPI_SOURCE);
            } break;
            case TRACTOR_P2P_PRINT_STRING_TAG: {
            case TRACTOR_REQ_PRINT_STRING_TAG:
            case TRACTOR_COLL_PRINT_STRING_TAG:
                int size;
                MPI_Get_count(&status, MPI_CHAR, &size);
                print_remote_string(size, status.MPI_TAG, status.MPI_SOURCE);
            } break;
            case TRACTOR_REQUEST_TAG: {
                int size;
                MPI_Get_count(&status, MPI_CHAR, &size);
                request_func_store->process_remote_request(size, status.MPI_SOURCE);
            } break;
            }
        }
    } while (flag > 0);
    return flag;
}
PEAT_Num Mat::process_outgoing_messages()
{
    if (!active_deliveries.empty()) {
        for (std::list<PEAT_Event_Delivery>::iterator it = active_deliveries.begin(); it != active_deliveries.end();) {
            MPI_Status status;
            int flag = 0;
            MPI_Test(&it->request, &flag, &status);
            if (flag > 0) { // request completed
                assert(it->request == MPI_REQUEST_NULL);
                peat_free(it->buffer);
                it = active_deliveries.erase(it);
                deliveries_completed++;
                continue;
            }
            ++it;
        }
        return active_deliveries.size();
    }
    return 0;
    // return p2p_func_store->check_completions() + coll_func_store->check_completions() + request_func_store->check_completions();
}
/*
   Called when this rank has parsed all its trace events: keep handling messages until every work message any pmat rank
   sent has been received. Handling a message can send more (a completion forwarded to the rank matching the collective
   is answered with the bind record for the trace rank's owner), and MPI_Iprobe can't see messages still in transit, so a
   rank being idle locally proves nothing. Instead every rank counts the work messages it sent and received, and
   rounds of MPI_Iallreduce sum them up while messages keep being handled. Termination: two consecutive rounds with
   sent == received and the same totals (Mattern's four-counter method) - then nothing was in transit between the two
   rounds and nothing was sent, and with all trace events parsed only a received message can cause a send. Every rank
   sees the same sums, so all stop in the same round.
*/
void Mat::wait_for_global_quiescence()
{
    PEAT_LNum previous[2] = {-1, -1};
    for (;;) {
        handle_incoming_messages();
        process_outgoing_messages();
        PEAT_LNum local[2] = {messages_sent(), messages_received}, global[2] = {0, 0};
        MPI_Request round;
        MPI_Iallreduce(local, global, 2, MPI_LONG_LONG, MPI_SUM, globals.comm_compute, &round);
        int done = 0;
        while (!done) {
            handle_incoming_messages();
            process_outgoing_messages();
            MPI_Test(&round, &done, MPI_STATUS_IGNORE);
        }
        if (global[0] == global[1] && global[0] == previous[0] && global[1] == previous[1])
            break;
        previous[0] = global[0];
        previous[1] = global[1];
    }
    // every message has been received, so the remaining sends complete without further receives
    while (process_outgoing_messages() > 0) {
    }
}
#endif
Mat::Mat(Global_Parameters &glob, Model_ID &model_id_storage) : ptp(NULL), input_MPI(NULL), input_HWC(NULL), events_processed(0), globals(glob), model_id(model_id_storage)
{
    // output_model = 0;
    // output_model_message = 0;
    const char *start = glob.pc_levels.c_str();
    char *endptr;
    int level = strtol(start, &endptr, 10);
    if (level > 0)
        pc_levels[level] = 1;
    while (strlen(endptr++) > 0) {
        level = strtol(endptr, &endptr, 10);
        if (level > 0)
            pc_levels[level] = 1;
    };
    // Open *com* files
    std::map<PEAT_Rank, std::string> file_input_com_names = get_file_names(globals.input_path, globals.trace_files, globals.com_ext, true);
    std::string file_name = glob.input_path + DIR_DELIMITER + glob.trace_files + glob.app_ext;
    std::map<PEAT_Rank, std::string> file_input_app_names;
    file_input_app_names[0] = file_name;

    file_name = glob.input_path + DIR_DELIMITER + glob.trace_files + glob.ptpd_ext;
    std::map<PEAT_Rank, std::string> file_input_ptp_names;
    file_input_ptp_names[0] = file_name;
    comm_store = new Communicators(globals);
    comm_store->initialize(file_input_com_names, file_input_app_names, &model_id_storage);

    PEAT_Rank trace_comm_size = comm_store->get_app_rank_count();
    tsc_corrector.resize(trace_comm_size, 0);
    tsc_predictor.resize(trace_comm_size, 0);
    rank_events_processed.resize(trace_comm_size, 0);

    for (std::map<PEAT_Rank, PEAT_Clock>::iterator it = comm_store->frame_tsc_start.begin(); it != comm_store->frame_tsc_start.end(); ++it) {
        tsc_corrector.at(it->first) = it->second;
    }
    ptp = new PTP(globals); // No need to set FREQ first
    ptp->initialize(file_input_com_names, file_input_ptp_names, file_input_app_names, &model_id_storage);

    // user_region_store = new Address_File_Storage(globals.path, globals.user_region_file + ".orig", globals.trace_files, globals.com_ext, globals.app_ext, PEAT_FIO_FLAG_READ | PEAT_FIO_FLAG_LAZY | PEAT_FIO_FLAG_ICLOSE, globals.verbose);
    user_region_store = new Address_File_Storage(globals, PEAT_FIO_FLAG_READ | PEAT_FIO_FLAG_LAZY | PEAT_FIO_FLAG_ICLOSE, &model_id_storage);

    input_MPI = new File_Storage_GZ(globals.pe_MPI_files_in_rank, PEAT_FIO_FLAG_READ | PEAT_FIO_FLAG_LAZY | PEAT_FIO_FLAG_ICLOSE, NULL, globals.verbose);
    input_HWC = new File_Storage_GZ(globals.pe_HWC_files_in_rank, PEAT_FIO_FLAG_READ | PEAT_FIO_FLAG_LAZY | PEAT_FIO_FLAG_ICLOSE, NULL, globals.verbose);
#ifdef MAT_PROCESS_HWC_MPI_OUTPUT
    output_MPI = new File_Storage_GZ(globals.pe_MPI_files_in_rank_out, PEAT_FIO_FLAG_WRITE, "w9", globals.verbose);
    output_HWC = new File_Storage_GZ(globals.pe_HWC_files_in_rank_out, PEAT_FIO_FLAG_WRITE, "w9", globals.verbose);
#else
    output_MPI = NULL;
    output_HWC = NULL;
#endif
    output_bind = new File_Storage_GZ(globals.pe_bind_files_in_rank_out, PEAT_FIO_FLAG_WRITE, "w9", globals.verbose);

#ifdef PEAT_MPI
    whole_ranks = globals.whole_ranks;
    my_ranks = globals.my_ranks;
#endif
	req_store = new Request_Store(ptp, globals.verbose);

    comm_func_store = new Comm();
    osc_func_store = new OSC();
    gpu_func_store = new GPU();

	p2p_func_store = new P2PAL(input_MPI, globals.skip_processing, globals.print_location
#ifdef PEAT_MPI
							   ,
							   &active_deliveries, whole_ranks, my_ranks, globals.comm_compute, globals.comm_rank
#endif
	);
	coll_func_store = new Collectives(globals.print_location
#ifdef PEAT_MPI
									  ,
									  &active_deliveries, whole_ranks, my_ranks, globals.comm_compute, globals.comm_size, globals.comm_rank
#endif
	);
	request_func_store = new Requests(globals.skip_processing
#ifdef PEAT_MPI
									 ,
									 &active_deliveries, whole_ranks, my_ranks, globals.comm_compute, globals.comm_rank
#endif
	);
    comm_func_store->initialize(ptp, comm_store, req_store, request_func_store, output_MPI, output_HWC, output_bind);
    osc_func_store->initialize(ptp, comm_store, req_store, request_func_store, output_MPI, output_HWC, output_bind);
    gpu_func_store->initialize(ptp, comm_store, output_MPI, output_HWC, output_bind);
	coll_func_store->initialize(ptp, comm_store, req_store, comm_func_store, request_func_store, output_MPI, output_HWC, output_bind);
	p2p_func_store->initialize(ptp, comm_store, req_store, request_func_store, gpu_func_store, output_MPI, output_HWC, output_bind);
	request_func_store->initialize(ptp, comm_store, req_store, p2p_func_store, comm_func_store, osc_func_store, coll_func_store, output_MPI, output_HWC, output_bind);
}
Mat::~Mat()
{
    delete input_MPI;
    delete input_HWC;
#ifdef MAT_PROCESS_HWC_MPI_OUTPUT
    delete output_MPI;
    delete output_HWC;
#endif
    delete output_bind;
    delete user_region_store;
    delete gpu_func_store;
    delete request_func_store;
    delete coll_func_store;
    delete p2p_func_store;
    delete comm_store;
    delete ptp;
}
Mat_Worker &Mat::worker()
{
#ifdef PEAT_OMP
    if (!workers.empty())
        return *workers[omp_get_thread_num()];
#endif
    return serial_worker;
}
PEAT_Rank Mat::get_soonest_event(Mat_Worker &w, PEAT_Clock &tsc)
{
    if (w.timeline.empty())
        return -1;
    std::map<PEAT_Clock, PEAT_Rank>::iterator it_rank = w.timeline.begin();
    PEAT_Rank rank = it_rank->second;
    tsc = it_rank->first;
    w.timeline.erase(it_rank);
    return rank;
}
#ifdef PEAT_OMP
#define MAT_OMP_FLUSH_SIZE (256 * 1024) // compress a rank's pending output once it holds this much text
/* The threads' walks over their ranks (rank % threads). Every output file is written deferred meanwhile: the text goes
   to memory under match_lock, and the thread owning the rank compresses it outside the lock (after each event of the
   rank, once enough has piled up). Whatever is left - bind records other threads wrote into this thread's files after
   it finished - is flushed when deferred writing is switched off at the end. */
void Mat::process_omp_ranks()
{
    const int nthreads = omp_get_max_threads();
    workers.clear();
    for (int t = 0; t < nthreads; t++)
        workers.push_back(std::make_unique<Mat_Worker>());
    for (std::map<PEAT_Rank, std::string>::const_iterator rank = globals.pe_MPI_files_in_rank.begin(); rank != globals.pe_MPI_files_in_rank.end(); ++rank) {
        workers[rank->first % nthreads]->ranks.push_back(rank->first);
        fprintf(stderr, "Thread:%d is handling rank %d.\n", rank->first % nthreads, rank->first);
    }
    File_Storage_GZ *const outputs[] = {output_MPI, output_HWC, output_bind};
    for (File_Storage_GZ *out : outputs)
        if (out)
            out->set_deferred(true);
    const PEAT_UID total_event_num = user_region_store->get_num_events(my_ranks);
    const PEAT_Ratio start_estimation = Wtime();
#pragma omp parallel num_threads(nthreads)
    {
        Mat_Worker &w = worker();
        const bool reporter = omp_get_thread_num() == 0;
        PEAT_Ratio prev_step = start_estimation;
        PEAT_LNum prev_event_num = 0;
        int cur_percent = 1;
        for (PEAT_Rank rank : w.ranks)
            parse_single_event(rank);
        for (;;) {
            PEAT_Clock tsc;
            const PEAT_Rank s_rank = get_soonest_event(w, tsc);
            if (s_rank == -1)
                break;
            parse_single_event(s_rank);
            for (File_Storage_GZ *out : outputs)
                if (out)
                    out->flush_pending(s_rank, MAT_OMP_FLUSH_SIZE);
            if (reporter) {
                PEAT_LNum done;
#pragma omp atomic read
                done = events_processed;
                if (done > total_event_num * cur_percent / 100) {
                    PEAT_Ratio cur_step = Wtime();
                    PEAT_Ratio estimation_bw = static_cast<PEAT_Ratio>(done) / (cur_step - start_estimation);
                    PEAT_Ratio instant_bw = static_cast<PEAT_Ratio>(done - prev_event_num) / (cur_step - prev_step);
                    prev_event_num = done;
                    printf("%d:%d%% Speed is %.2lf (%.2lf) events per second. Step Time %.2lf. Total Time %.2lf\n", globals.comm_rank, cur_percent, instant_bw, estimation_bw, cur_step - prev_step, cur_step - start_estimation);
                    if (globals.verbose) {
                        std::lock_guard<Mat_Match_Lock> matching(match_lock);
                        p2p_func_store->print_sizes();
                        coll_func_store->print_sizes();
                    }
                    fflush(stdout);
                    prev_step = cur_step;
                    cur_percent++;
                }
            }
        }
        for (PEAT_Rank rank : w.ranks)
            for (File_Storage_GZ *out : outputs)
                if (out)
                    out->flush_pending(rank);
    }
    for (File_Storage_GZ *out : outputs)
        if (out)
            out->set_deferred(false);
    workers.clear(); // worker() is the serial one again for whatever runs after the walk
}
#endif
#define CYCLE_EVENT 100000
void Mat::process()
{
    const double bytes_step = (double)(input_MPI->get_all_files_size() + input_HWC->get_all_files_size()) * 0.01;
    PEAT_Ratio time_since_last_active = Wtime();
    // timeline.clear();
    p2p_func_store->resize_queues(comm_store->get_comm_world_rank_count());
#ifdef PEAT_OMP
    process_omp_ranks();
#else
    for (std::map<PEAT_Rank, std::string>::const_iterator rank = globals.pe_MPI_files_in_rank.begin(); rank != globals.pe_MPI_files_in_rank.end(); ++rank) {
        parse_single_event(rank->first);
    }
#endif
    PEAT_Num completions = 0;
    const PEAT_UID total_event_num = user_region_store->get_num_events(my_ranks);
    PEAT_Ratio start_estimation = Wtime();
    PEAT_Ratio prev_step = start_estimation;
    PEAT_LNum prev_event_num = 0;
    int cur_percent = 1;
#ifdef PEAT_MPI
    unsigned long long cycle = 0;
#endif
#ifndef PEAT_OMP
    {
        while ((input_MPI->is_any_file_good() || !serial_worker.timeline.empty() || completions > 0)) {
            PEAT_Rank s_rank;
            PEAT_Clock tsc;
            s_rank = get_soonest_event(serial_worker, tsc);
            if (s_rank == -1)
                break;
            parse_single_event(s_rank);
#ifdef PEAT_MPI
            if (++cycle > globals.cycle_events) {
                cycle = 0;
                completions = process_outgoing_messages();
                handle_incoming_messages();
            }
#endif
            if (events_processed > total_event_num * cur_percent / 100) {
                PEAT_Ratio cur_step = Wtime();
                PEAT_Ratio estimation_bw = ((PEAT_Ratio)(events_processed)) / (cur_step - start_estimation);
                PEAT_Ratio instant_bw = ((PEAT_Ratio)(events_processed - prev_event_num)) / (cur_step - prev_step);
                prev_event_num = events_processed;
                printf("%d:%d%% Speed is %.2lf (%.2lf) events per second. Step Time %.2lf. Total Time %.2lf\n", globals.comm_rank, cur_percent, instant_bw, estimation_bw, cur_step - prev_step, cur_step - start_estimation);
                if (globals.verbose) {
                    // print_elems_in_queue();
                    p2p_func_store->print_sizes();
                    coll_func_store->print_sizes();
                }
                fflush(stdout);
                prev_step = cur_step;
                cur_percent++;
            }
        }
    }
#endif
#ifdef PEAT_MPI
    wait_for_global_quiescence();
    {
        PEAT_Ratio cur_step = Wtime();
        PEAT_Ratio estimation_bw = ((PEAT_Ratio)(events_processed)) / (cur_step - start_estimation);
        PEAT_Ratio instant_bw = ((PEAT_Ratio)(events_processed - prev_event_num)) / (cur_step - prev_step);
        printf("%d:%d%% Speed is %.2lf (%.2lf) events per second. Step Time %.2lf. Total Time %.2lf\n", globals.comm_rank, 100, instant_bw, estimation_bw, cur_step - prev_step, cur_step - start_estimation);
        if (globals.verbose) {
            p2p_func_store->print_sizes();
            coll_func_store->print_sizes();
        }
    }
#endif
    process_unbound_events(globals.comm_rank);
#ifdef PEAT_MPI
    // with --print_on_send the p2p pairs matched only now send their bind records to the owner of the sending trace
    // rank: wait until those are received too (every rank enters after its own process_unbound_events, so again only
    // a received message can cause a send). Without it they are written locally and nothing is sent. Every rank has
    // the same options, so all of them take part in this collective or none.
    if (globals.print_location) {
        wait_for_global_quiescence();
    }
    int perm = 0;
    MPI_Status status;
    if (globals.comm_rank > 0)
        MPI_Recv(&perm, 1, MPI_INT, globals.comm_rank - 1, PROFILE_WRITE_PERMISSION, globals.comm_compute, &status);
#endif
    gpu_func_store->flush_pending();
    printf("Statistic block begins on rank:%d\n", globals.comm_rank);
    if (!pc_levels.empty()) {
        for (std::map<int, int>::iterator tm_it = pc_levels.begin(); tm_it != pc_levels.end(); ++tm_it) {
            if (!p2p_func_store->printed_p2p[tm_it->first].empty()) {
                printf("%20s: %7d\n", "PC Region", tm_it->first);
                printf("%17s %10s %10s<=>%10s %14s %14s %14s %20s %14s\n", "func_id", "Bytes", "Rank", "Peer", "Min", "Max", "Total", "Imbalance", "Calls");
                for (std::map<PEAT_P2P_Record, PEAT_Single_Event_Clock>::iterator rIt = p2p_func_store->printed_p2p[tm_it->first].begin(); rIt != p2p_func_store->printed_p2p[tm_it->first].end(); ++rIt) {
                    PEAT_P2P_Record record = rIt->first;
                    PEAT_Single_Event_Clock clock = rIt->second;
                    char imb_buf[32];
                    snprintf(imb_buf, sizeof(imb_buf), "%lld (%3.2lf)", clock.imb_c, (double)clock.imb_c * 100.0 / (double)clock.total_c);
                    printf("%15s %10lld %10d<=>%10d %14lld %14lld %14lld %20s %14lld\n",
                           global_func_id.name_by_idx(record.func_id),
                           record.bytes,
                           record.rank, record.peer,
                           clock.min_c, clock.max_c, clock.total_c, imb_buf, clock.occ);
                }
            }
        }
    }
    if (!p2p_func_store->printed_p2p[0].empty()) {
        printf("%20s:\n", "Out of any PC region");
        printf("%17s %10s %10s<=>%10s %14s %14s %14s %20s %14s\n", "func_id", "Bytes", "Rank", "Peer", "Min", "Max", "Total", "Imbalance", "Calls");
        for (std::map<PEAT_P2P_Record, PEAT_Single_Event_Clock>::iterator rIt = p2p_func_store->printed_p2p[0].begin(); rIt != p2p_func_store->printed_p2p[0].end(); ++rIt) {
            PEAT_P2P_Record record = rIt->first;
            PEAT_Single_Event_Clock clock = rIt->second;
            char imb_buf[32];
            snprintf(imb_buf, sizeof(imb_buf), "%lld (%3.2lf)", clock.imb_c, (double) clock.imb_c * 100.0 / (double) clock.total_c);
            printf("%15s %10lld %10d<=>%10d %14lld %14lld %14lld %20s %14lld\n",
                   global_func_id.name_by_idx(record.func_id),
                   record.bytes,
                   record.rank, record.peer,
                   clock.min_c, clock.max_c, clock.total_c, imb_buf, clock.occ);
        }
    }

    if (!pc_levels.empty()) {
        for (std::map<int, int>::iterator tm_it = pc_levels.begin(); tm_it != pc_levels.end(); ++tm_it) {
            if (!coll_func_store->printed_collective[tm_it->first].empty()) {
                printf("%20s: %7d\n", "PC Region", tm_it->first);
                printf("%15s %10s %10s %10s %10s %14s %14s %14s %20s %14s %10s\n", "Function", "Comm", "Root", "Size", "Rank", "Min", "Max", "Total", "Imbalance", "Calls", "Parameters");
                for (std::map<MAT_Collective_Record, MAT_Collective_Clocks>::iterator cIt = coll_func_store->printed_collective[tm_it->first].begin(); cIt != coll_func_store->printed_collective[tm_it->first].end(); ++cIt) {
                    MAT_Collective_Record record = cIt->first;
                    MAT_Collective_Clocks clocks = cIt->second;
                    printf("%15s %10x %10d %10d \n", global_func_id.name_by_idx(record.func_id), record.comm, record.root, record.size);
                    std::map<PEAT_Rank, PEAT_Rank> coll_ranks = comm_store->get_ranks_map_from_comm(record.comm);
                    for (std::map<PEAT_Rank, PEAT_Rank>::iterator rIt = coll_ranks.begin(); rIt != coll_ranks.end(); ++rIt) {
                        PEAT_Rank o_rank = rIt->first;
                        MAT_Collective_Clock clock = clocks.clocks.at(o_rank);
                        char imb_buf[32];
                        snprintf(imb_buf, sizeof(imb_buf), "%lld(%3.2lf)", clock.imb_c, (double)clock.imb_c * 100.0 / (double)clock.total_c);
                        printf("%15s %10s %10s %10s %10d %14lld %14lld %14lld %20s %14lld %10s\n", record.comp_func.c_str(), " ", " ", " ",
                               o_rank, clock.min_c, clock.max_c, clock.total_c, imb_buf, clock.occ, " ");
                        if (!record.send_bytes.empty()) {
                            printf("Send:");
                            if (record.send_bytes.is_vector) {
                                for (std::map<PEAT_Rank, PEAT_Message_Size>::iterator mIt = record.send_bytes.bytes.begin(); mIt != record.send_bytes.bytes.end(); ++mIt) {
                                    PEAT_Rank d_rank = mIt->first;
                                    PEAT_Message_Size send = mIt->second;
                                    printf("%d:%lld ",
                                           d_rank, send);
                                }
                            } else {
                                PEAT_Message_Size send = record.send_bytes.get(0);
                                printf("%lld ", send);
                            }
                        }
                        if (!record.recv_bytes.empty()) {
                            printf("Recv:");
                            if (record.recv_bytes.is_vector) {
                                for (std::map<PEAT_Rank, PEAT_Message_Size>::iterator mIt = record.recv_bytes.bytes.begin(); mIt != record.recv_bytes.bytes.end(); ++mIt) {
                                    PEAT_Rank d_rank = mIt->first;
                                    PEAT_Message_Size recv = mIt->second;
                                    printf("%d:%lld ", d_rank, recv);
                                }
                            } else {
                                PEAT_Message_Size recv = record.recv_bytes.get(0);
                                printf("%lld ", recv);
                            }
                        }
                        printf("\n");
                    }
                }
            }
        }
    }
    if (!coll_func_store->printed_collective[0].empty()) {
        printf("%20s:\n", "Out of any PC region");
        printf("%15s %10s %10s %10s %10s %14s %14s %14s %20s %14s %10s\n", "Function", "Comm", "Root", "Size", "Rank", "Min", "Max", "Total", "Imbalance", "Calls", "Parameters");
        for (std::map<MAT_Collective_Record, MAT_Collective_Clocks>::iterator cIt = coll_func_store->printed_collective[0].begin(); cIt != coll_func_store->printed_collective[0].end(); ++cIt) {
            MAT_Collective_Record record = cIt->first;
            MAT_Collective_Clocks clocks = cIt->second;
            printf("%15s %10x %10d %10d \n", global_func_id.name_by_idx(record.func_id), record.comm, record.root, record.size);
            std::map<PEAT_Rank, PEAT_Rank> coll_ranks = comm_store->get_ranks_map_from_comm(record.comm);
            for (std::map<PEAT_Rank, PEAT_Rank>::iterator rIt = coll_ranks.begin(); rIt != coll_ranks.end(); ++rIt) {
                PEAT_Rank o_rank = rIt->first;
                MAT_Collective_Clock clock = clocks.clocks.at(o_rank);
                char imb_buf[32];
                snprintf(imb_buf, sizeof(imb_buf), "%lld(%3.2lf)", clock.imb_c, (double) clock.imb_c * 100.0 / (double)clock.total_c);
                printf("%15s %10s %10s %10s %10d %14lld %14lld %14lld %20s %14lld %10s\n", record.comp_func.c_str(), " ", " ", " ",
                       o_rank, clock.min_c, clock.max_c, clock.total_c, imb_buf, clock.occ, " ");
                if (!record.send_bytes.empty()) {
                    printf("Send:");
                    if (record.send_bytes.is_vector) {
                        for (std::map<PEAT_Rank, PEAT_Message_Size>::iterator mIt = record.send_bytes.bytes.begin(); mIt != record.send_bytes.bytes.end(); ++mIt) {
                            PEAT_Rank d_rank = mIt->first;
                            PEAT_Message_Size send = mIt->second;
                            printf("%d:%lld ",
                                   d_rank, send);
                        }
                    } else {
                        PEAT_Message_Size send = record.send_bytes.get(0);
                        printf("%lld ", send);
                    }
                }
                if (!record.recv_bytes.empty()) {
                    printf("Recv:");
                    if (record.recv_bytes.is_vector) {
                        for (std::map<PEAT_Rank, PEAT_Message_Size>::iterator mIt = record.recv_bytes.bytes.begin(); mIt != record.recv_bytes.bytes.end(); ++mIt) {
                            PEAT_Rank d_rank = mIt->first;
                            PEAT_Message_Size recv = mIt->second;
                            printf("%d:%lld ", d_rank, recv);
                        }
                    } else {
                        PEAT_Message_Size recv = record.recv_bytes.get(0);
                        printf("%lld ", recv);
                    }
                }
                // printf("\n");
            }
        }
    }

    if (!pc_levels.empty()) {
        for (std::map<int, int>::iterator tm_it = pc_levels.begin(); tm_it != pc_levels.end(); ++tm_it) {
            printf("%20s: %7d\n", "PC Region", tm_it->first);
            std::map<PEAT_Property, Mat_Time> t_times = timings[tm_it->first];
            for (std::map<PEAT_Property, Mat_Time>::iterator t_it = t_times.begin(); t_it != t_times.end(); ++t_it) {
                printf("%20s: %10lld - %12lld\n", print_record_properties(t_it->first), t_it->second.occurences, t_it->second.durations);
            }
        }
    }
    printf("%20s:\n", "Out of any PC region");
    std::map<PEAT_Property, Mat_Time> t_times = timings[0];
    for (std::map<PEAT_Property, Mat_Time>::iterator t_it = t_times.begin(); t_it != t_times.end(); ++t_it) {
        printf("%20s: %10lld - %12lld\n", print_record_properties(t_it->first), t_it->second.occurences, t_it->second.durations);
    }

    printf("%20s:\n", "Transfers");
    if (!pc_levels.empty()) {
        for (std::map<int, int>::iterator tm_it = pc_levels.begin(); tm_it != pc_levels.end(); ++tm_it) {
            if (!gpu_func_store->printed_gpu_transfers[tm_it->first].empty()) {
                printf("%20s: %7d\n", "PC Region", tm_it->first);
                printf("%65s %10s %10s %14s %14s %14s %14s %14s\n", "func_ids", "Bytes", "Rank", "Min", "Max", "Total", "Imbalance", "Calls");
                for (std::map<PEAT_GPUT_Record, PEAT_Single_Event_Clock>::iterator rIt = gpu_func_store->printed_gpu_transfers[tm_it->first].begin(); rIt != gpu_func_store->printed_gpu_transfers[tm_it->first].end(); ++rIt) {
                    PEAT_GPUT_Record record = rIt->first;
                    PEAT_Single_Event_Clock clock = rIt->second;
                    printf("%32s %32s %10lld %10d %14lld %14lld %14lld %14lld %14lld\n",
                           global_func_id.name_by_idx(record.func_id[0]),
                           global_func_id.name_by_idx(record.func_id[1]),
                           record.bytes,
                           record.rank, clock.min_c, clock.max_c, clock.total_c, clock.imb_c, clock.occ);
                }
            }
        }
    }
    if (!gpu_func_store->printed_gpu_transfers[0].empty()) {
        printf("%20s:\n", "Out of any PC region");
        printf("%65s %10s %10s %14s %14s %14s %14s %14s\n", "func_ids", "Bytes", "Rank", "Min", "Max", "Total", "Imbalance", "Calls");
        for (std::map<PEAT_GPUT_Record, PEAT_Single_Event_Clock>::iterator rIt = gpu_func_store->printed_gpu_transfers[0].begin(); rIt != gpu_func_store->printed_gpu_transfers[0].end(); ++rIt) {
            PEAT_GPUT_Record record = rIt->first;
            PEAT_Single_Event_Clock clock = rIt->second;
            printf("%32s %32s %10lld %10d %14lld %14lld %14lld %14lld %14lld\n",
                   global_func_id.name_by_idx(record.func_id[0]),
                   global_func_id.name_by_idx(record.func_id[1]),
                   record.bytes,
                   record.rank, clock.min_c, clock.max_c, clock.total_c, clock.imb_c, clock.occ);
        }
    }

    printf("%20s:\n", "Executions");
    if (!pc_levels.empty()) {
        for (std::map<int, int>::iterator tm_it = pc_levels.begin(); tm_it != pc_levels.end(); ++tm_it) {
            if (!gpu_func_store->printed_gpu_executions[tm_it->first].empty()) {
                printf("%20s: %7d\n", "PC Region", tm_it->first);
                printf("%65s %10s %10s %14s %14s %14s %14s %14s\n", "func_ids", "Handle", "Rank", "Min", "Max", "Total", "Imbalance", "Calls");
                for (std::map<PEAT_GPUE_Record, PEAT_Single_Event_Clock>::iterator rIt = gpu_func_store->printed_gpu_executions[tm_it->first].begin(); rIt != gpu_func_store->printed_gpu_executions[tm_it->first].end(); ++rIt) {
                    PEAT_GPUE_Record record = rIt->first;
                    PEAT_Single_Event_Clock clock = rIt->second;
                    printf("%32s %32s %10llx %10d %14lld %14lld %14lld %14lld %14lld\n",
                           global_func_id.name_by_idx(record.func_id[0]),
                           global_func_id.name_by_idx(record.func_id[1]),
                           record.handle,
                           record.rank, clock.min_c, clock.max_c, clock.total_c, clock.imb_c, clock.occ);
                }
            }
        }
    }
    if (!gpu_func_store->printed_gpu_executions[0].empty()) {
        printf("%20s:\n", "Out of any PC region");
        printf("%65s %10s %10s %14s %14s %14s %14s %14s\n", "func_ids", "Handle", "Rank", "Min", "Max", "Total", "Imbalance", "Calls");
        for (std::map<PEAT_GPUE_Record, PEAT_Single_Event_Clock>::iterator rIt = gpu_func_store->printed_gpu_executions[0].begin(); rIt != gpu_func_store->printed_gpu_executions[0].end(); ++rIt) {
            PEAT_GPUE_Record record = rIt->first;
            PEAT_Single_Event_Clock clock = rIt->second;
            printf("%32s %32s %10llx %10d %14lld %14lld %14lld %14lld %14lld\n",
                   global_func_id.name_by_idx(record.func_id[0]),
                   global_func_id.name_by_idx(record.func_id[1]),
                   record.handle,
                   record.rank, clock.min_c, clock.max_c, clock.total_c, clock.imb_c, clock.occ);
        }
    }
    printf("Statistic block ends on rank:%d\n", globals.comm_rank);
#ifdef PEAT_MPI
    if (globals.comm_rank < globals.comm_size - 1)
        MPI_Ssend(&perm, 1, MPI_INT, globals.comm_rank + 1, PROFILE_WRITE_PERMISSION, globals.comm_compute);
#endif
    return;
}
void Mat::write_trace_info(const std::string &name)
{
#ifdef PEAT_MPI
    PEAT_LNum collectives_bound = coll_func_store->collectives_bound;
    std::vector<PEAT_LNum> messages_bound = p2p_func_store->messages_bound;
    std::vector<PEAT_UID> ranks_events = rank_events_processed;

    MPI_Reduce(&collectives_bound, &coll_func_store->collectives_bound, 1, MPI_LONG_LONG, MPI_SUM, 0, globals.comm_compute);
    MPI_Reduce(messages_bound.data(), p2p_func_store->messages_bound.data(), (int)p2p_func_store->messages_bound.size(), MPI_LONG_LONG, MPI_SUM, 0, globals.comm_compute);
    MPI_Reduce(ranks_events.data(), rank_events_processed.data(), (int)rank_events_processed.size(), MPI_LONG_LONG, MPI_SUM, 0, globals.comm_compute);
    if (globals.comm_rank == 0) {
#endif
        std::map<PEAT_Rank, std::string> file_name;
        file_name[0] = name;
        File_Storage_GZ traceinfo_file(file_name, PEAT_FIO_FLAG_WRITE, NULL, globals.verbose);
        if (!traceinfo_file.is_file_good(0)) {
            std::ostringstream tmp;
            tmp << "Can't open file (to write) " << name;
            throw std::runtime_error(tmp.str());
        }
        traceinfo_file.printf(0, "PEATTRACEINFO\n");
        traceinfo_file.printf(0, "%d|%s|%s|%s|%s|%s|%s|%s|%s\n", PEAT_RT_Paths, peat_get_relative_path(globals.output_path, globals.input_path).c_str(),
                             globals.trace_files.c_str(),
                             globals.mpi_files.c_str(), globals.hwc_files.c_str(), globals.bind_files.c_str(), globals.transport_files.c_str(), globals.com_ext.c_str(), globals.app_ext.c_str());
        traceinfo_file.printf(0, "%d|%lld\n", PEAT_RT_CPU_Freq, ptp->get_trace_freq(0));
        traceinfo_file.printf(0, "%d|%d|%d|%d\n", PEAT_RT_Var_Block_Info, model_id.num_tscs, model_id.num_hwcs, model_id.num_addrs);
        if (coll_func_store->collectives_bound > 0)
            traceinfo_file.printf(0, "%d|%lld\n", PEAT_RT_Bind_Collective, coll_func_store->collectives_bound);
        if (!p2p_func_store->messages_bound.empty()) {
            traceinfo_file.printf(0, "%d", PEAT_RT_Bind_Peer);
            for (std::vector<PEAT_LNum>::const_iterator messages_it = p2p_func_store->messages_bound.begin(); messages_it != p2p_func_store->messages_bound.end(); ++messages_it) {
                traceinfo_file.printf(0, "|%lld", *messages_it);
            }
            traceinfo_file.printf(0, "\n");
        }
        if (!user_region_store->last_event_idx.empty()) {
            traceinfo_file.printf(0, "%d", PEAT_RT_Number_Of_Events);
            for (std::vector<PEAT_UID>::const_iterator events_it = rank_events_processed.begin(); events_it != rank_events_processed.end(); ++events_it) {
                traceinfo_file.printf(0, "|%lld", *events_it);
            }
            traceinfo_file.printf(0, "\n");
        }
        if (!comm_store->mpi_time.empty()) {
            traceinfo_file.printf(0, "%d", PEAT_RT_MPI_Time);
            for (std::map<PEAT_Rank, PEAT_MPI_Time>::const_iterator time_it = comm_store->mpi_time.begin(); time_it != comm_store->mpi_time.end(); ++time_it) {
                traceinfo_file.printf(0, "|%d|%lld|%s", time_it->first, time_it->second.clocks, time_it->second.percentage.c_str());
            }
            traceinfo_file.printf(0, "\n");
        }
        traceinfo_file.printf(0, "%d", PEAT_RT_MPM_Rank_Time_Stamp);
        for (std::map<PEAT_Rank, Times_Sync>::iterator sync_times_it = ptp->sync_times.begin(); sync_times_it != ptp->sync_times.end(); ++sync_times_it) {
            PEAT_Rank r = sync_times_it->first;
            traceinfo_file.printf(0, "|%d|%lld|%lld|%lld|%lld", r,
                                 sync_times_it->second.t1,
                                 sync_times_it->second.t2,
                                 sync_times_it->second.t3,
                                 sync_times_it->second.t4);
        }
        traceinfo_file.printf(0, "\n");
#ifdef PEAT_MPI
    }
#endif
}
