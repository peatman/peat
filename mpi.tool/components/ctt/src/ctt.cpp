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

#include "ctt.h"
#include <string>
#ifdef PEAT_MPI
#include <mpi.h>
#endif
#ifdef _OPENMP
#include <omp.h>
#endif
#define ctt_out           \
    if (globals.verbose) \
    std::cout
#define ctt_dbg(x)            \
    if (globals.verbose > x) \
    std::cerr
Tractor_Event send_line, recv_line;

bool CTT::parse_single_event(const int rank, String_Split &line_mpi, String_Split &line_hwc)
{
    line_mpi.clear();
    line_hwc.clear();
    if (!input_MPI->is_file_good(rank) || !input_HWC->is_file_good(rank)) {
        return false;
    }
    input_MPI->read_lines(line_mpi, rank, ':');
    input_HWC->read_lines(line_hwc, rank, ':');
    // ctt_dbg(0) << "P" << rank << std::endl;
    if (line_mpi.size() < 2) {
        return false;
    }
    std::string mpi_func = line_mpi.at(3);
    if (!second_line) {
#pragma omp atomic
        events_processed++;
    }
    if (globals.func_set.count(mpi_func) < 1 && (!globals.func_set.empty()))
        return true;
    Tractor_Event t_event(rank, line_mpi, line_hwc, model_id.num_hwcs, model_id.num_addrs, user_region_store, com_store);
    PEAT_MPI_Func_ID mpifunc = t_event.func_id;
    CTT_Event f_event;
    if (t_event.func_class == PEAT_PROP_CLASS_COLLECTIVE) { // collective
        parse_collective(rank, t_event, f_event);
    }

    if (t_event.func_class == PEAT_PROP_CLASS_P2P) { // P2P
        if (global_func_id.check_property(mpifunc, PEAT_PROP_DATA_TXRX)) {
            if (second_line) {
                recv_line = t_event;
                recv_line.tsc_time = send_line.tsc_time;
                recv_line.tsc_u_time = send_line.tsc_u_time;
                parse_P2PTx(rank, send_line, f_event);
                parse_P2PRx(rank, recv_line, f_event);
                second_line = false;
            } else {
                send_line = t_event;
                second_line = true;
                String_Split line_mpi;
                String_Split line_hwc;
                parse_single_event(rank, line_mpi, line_hwc);
            }
        } else {
            if (global_func_id.check_property(mpifunc, PEAT_PROP_DATA_TX)) { // send f.i
                parse_P2PTx(rank, t_event, f_event);
            } else {
                if (global_func_id.check_property(mpifunc, PEAT_PROP_DATA_RX)) { // recv f.i TODO must be the first in switch/case
                    parse_P2PRx(rank, t_event, f_event);
                }
            }
        }
    }
    if (t_event.func_class == PEAT_PROP_CLASS_PROBE) {
        parse_P2PPb(rank, t_event, f_event);
    }
    if (t_event.func_class == PEAT_PROP_CLASS_REQUEST) {
        parse_cn(rank, t_event, f_event);
    }

    if (t_event.func_class == PEAT_PROP_CLASS_FIO) {
        t_event.func_class = PEAT_PROP_CLASS_LOCAL;
    }

    if (t_event.func_class == PEAT_PROP_CLASS_BUF_LOCAL) {
        parse_local(rank, t_event, f_event);
    }
    if (t_event.func_class == PEAT_PROP_CLASS_COMM) {
        parse_comm(rank, t_event, f_event);
    }
    if (t_event.func_class == PEAT_PROP_CLASS_OSC) {
        t_event.func_class = PEAT_PROP_CLASS_LOCAL;
    }

    if (t_event.func_class == PEAT_PROP_CLASS_GPU) {
        parse_GPU(rank, t_event, f_event);
    }
    if (t_event.func_class == PEAT_PROP_CLASS_LOCAL || t_event.func_class == PEAT_PROP_CLASS_OMPT) {
        parse_local(rank, t_event, f_event);
    }
    std::list<CTT_Event> &tmp_list = funcs_out.at(rank)[mpi_func];
    std::list<CTT_Event>::iterator func_it;
    if (globals.aggregate_funcs) {
        for (func_it = tmp_list.begin(); func_it != tmp_list.end(); ++func_it) {
            if (f_event == *func_it) {
                func_it->add_call(f_event.duration);
                break;
            }
        }
        if (func_it == tmp_list.end()) {
            tmp_list.push_back(f_event);
        }
    } else {
        tmp_list.push_back(f_event);
        tmp_list.sort();
        if (tmp_list.size() > globals.max_list_size)
            tmp_list.resize(globals.max_list_size);
    }
    return true;
}
CTT::CTT(Global_Parameters &glob, Model_ID &model_id_storage) : second_line(false), globals(glob), model_id(model_id_storage), events_processed(0), input_HWC(NULL), input_MPI(NULL)
{
    // Open *com* files
    std::map<PEAT_Rank, std::string> file_input_com_names = get_file_names(globals.input_path, globals.trace_files, globals.com_ext, globals.rank_set, true);
    std::string file_name = glob.input_path + DIR_DELIMITER + glob.trace_files + glob.app_ext;
    std::map<PEAT_Rank, std::string> file_input_app_names;
    file_input_app_names[0] = file_name;

    file_name = glob.input_path + DIR_DELIMITER + glob.trace_files + glob.ptpd_ext;
    std::map<PEAT_Rank, std::string> file_input_ptp_names;
    file_input_ptp_names[0] = file_name;
    com_store = new Communicators(globals);
    ptp = new PTP(globals);
#ifdef PEAT_MPI
    whole_ranks = globals.whole_ranks;
    my_ranks = globals.my_ranks;
#endif

    com_store->initialize(file_input_com_names, file_input_app_names, &model_id_storage);
    ptp->initialize(file_input_com_names, file_input_ptp_names, file_input_app_names, &model_id_storage);

    // user_region_store = new Address_File_Storage(globals.path, globals.user_region_file + ".orig", globals.trace_files, globals.com_ext, globals.app_ext, PEAT_FIO_FLAG_READ | PEAT_FIO_FLAG_LAZY | PEAT_FIO_FLAG_ICLOSE, globals.verbose);
    user_region_store = new Address_File_Storage(globals, PEAT_FIO_FLAG_READ | PEAT_FIO_FLAG_LAZY | PEAT_FIO_FLAG_ICLOSE, &model_id_storage);

    input_MPI = new File_Storage_GZ(globals.pe_MPI_files_in_rank, PEAT_FIO_FLAG_READ | PEAT_FIO_FLAG_LAZY | PEAT_FIO_FLAG_ICLOSE, NULL, globals.verbose);
    input_HWC = new File_Storage_GZ(globals.pe_HWC_files_in_rank, PEAT_FIO_FLAG_READ | PEAT_FIO_FLAG_LAZY | PEAT_FIO_FLAG_ICLOSE, NULL, globals.verbose);
}

CTT::~CTT()
{
    if (input_HWC)
        delete input_HWC;
    if (input_MPI)
        delete input_MPI;
    if (user_region_store)
        delete user_region_store;
    if (com_store)
        delete com_store;
    if (ptp)
        delete ptp;
}
bool sort_comp(const CTT_Event &s, const CTT_Event &d)
{
    if (s.duration >= d.duration)
        return true; // I need reverse order
    return false;
}

void CTT::process()
{
    int cur_percent = 1;
    String_Split trace_line_MPI, trace_line_HWC;
    const PEAT_UID total_event_num = user_region_store->get_num_events(my_ranks);
    PEAT_LNum prev_event_num = 0;
    PEAT_Ratio start_estimation = Wtime();
    PEAT_Ratio prev_step = start_estimation;
    std::vector<PEAT_Rank> ranks;
    for (std::map<PEAT_Rank, std::string>::const_iterator rank = globals.pe_HWC_files_in_rank.begin(); rank != globals.pe_HWC_files_in_rank.end(); ++rank) {
        ranks.push_back(rank->first);
        std::map<std::string, std::list<CTT_Event>> temp_funcs_out;
        funcs_out[rank->first] = temp_funcs_out;
        funcs_out[rank->first].clear();
    }
    int rank_size = ranks.size();
#pragma omp parallel for schedule(dynamic)
    for (int i = 0; i < rank_size; i++) {
        String_Split line_mpi;
        String_Split line_hwc;
        PEAT_Rank rank = ranks.at(i);
        while (input_MPI->is_file_good(rank)) {
            parse_single_event(rank, line_mpi, line_hwc);
            if (events_processed > total_event_num * cur_percent / 100) {
#ifdef PEAT_OMP
                if (omp_get_thread_num() == 0)
#endif
                {
                    PEAT_Ratio cur_step = Wtime();
                    PEAT_Ratio estimation_bw = ((PEAT_Ratio)(events_processed)) / (cur_step - start_estimation);
                    PEAT_Ratio instant_bw = ((PEAT_Ratio)(events_processed - prev_event_num)) / (cur_step - prev_step);
                    prev_event_num = events_processed;
                    if (globals.verbose) {
                        // print_elems_in_queue();
                    }
                    fprintf(stderr, "%d%%. %lld of %lld events processed. ", cur_percent, events_processed, total_event_num);
                    fprintf(stderr, "Speed is %.2lf (%.2lf) events per second.", instant_bw, estimation_bw);
                    fprintf(stderr, "Step Time %.2lf. Total Time %.2lf\n", cur_step - prev_step, cur_step - start_estimation);
                    fflush(stderr);
                    prev_step = cur_step;
                    cur_percent++;
                }
            }
        }
#pragma omp critical
        {
            for (std::map<std::string, std::list<CTT_Event>>::iterator funcs_it = funcs_out[rank].begin(); funcs_it != funcs_out[rank].end(); ++funcs_it) {
                printf("Function %s\n", funcs_it->first.c_str());
                // funcs_it->second.sort(&sort_comp);
                funcs_it->second.sort();
                if (funcs_it->second.size() > globals.max_list_size)
                    funcs_it->second.resize(globals.max_list_size);
                for (std::list<CTT_Event>::iterator func_it = funcs_it->second.begin(); func_it != funcs_it->second.end(); ++func_it) {
                    double secs = (double)func_it->duration / (double)ptp->get_trace_freq(func_it->rank);
                    printf("\t Rank:%d Calls %lld Time %lld(clk) %lf(sec) ", func_it->rank, func_it->occurences, func_it->duration, secs);
                    switch (func_it->func_class) {
                    case PEAT_PROP_CLASS_P2P:
                        printf("Message Size:%lld\n", func_it->p2p.size);
                        break;
                    case PEAT_PROP_CLASS_COLLECTIVE: {
                        std::map<PEAT_Rank, PEAT_Rank> comm = com_store->get_ranks_map_from_comm(func_it->coll.comm_id);
                        printf("\t\tComm: ID:%x Ranks:%lld ", func_it->coll.comm_id, comm.size());
                        printf("Message Sizes: %lld %lld\n\t\t", func_it->coll.aggr_send_size, func_it->coll.aggr_recv_size);
                        for (std::map<PEAT_Rank, PEAT_Rank>::const_iterator rank_it = comm.cbegin(); rank_it != comm.cend(); ++rank_it) {
                            printf("%ld:%d ", rank_it->second, rank_it->first);
                        }
                        printf("\n");
                    } break;
                    case PEAT_PROP_CLASS_REQUEST:
                        printf("Requests num:%I64d\n", func_it->req.reqs.size());
                        break;
                    default:
                        printf("\n");
                        break;
                    }
                    for (size_t i = 0; i < func_it->addrs.size(); i++) {
                        User_Address_Info *addr_info = user_region_store->get_addr_by_id(func_it->rank, func_it->addrs[i]);
                        printf("\t\t %s %s %s:%d\n", addr_info->full_path.c_str(), addr_info->func.c_str(), addr_info->source.c_str(), addr_info->line);
                    }
                }
            }
            fflush(stdout);
            funcs_out[rank].clear();
        }
    }
    return;
}
