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

#include <algorithm>
#include <assert.h>
#include <errno.h>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits.h>
#include <numeric> //std::accumulate
#include <set>
#include <sstream>
#include <stdexcept>
#include <stdlib.h>

#include "pet_core.h"
#include "stat.h"
#include "transport.h"
#include "utils.h"

#ifdef PEAT_MPI
#include <mpi.h>
#endif

#define PCCOUT           \
    if (globals.verbose) \
    std::cout
#define PCDBG(x)              \
    if (globals.verbose >= x) \
    std::cerr
#define PCDELIM '|'
#define SEC_OUTPUT(x) fixed << std::setprecision(0) << x << " ( " << std::setprecision(2) << x / freq << " sec)"

PET_Core::PET_Core(const Global_Parameters &glob, const Model_ID &model_id_storage) : globals(glob), model_id(model_id_storage), e_timeline_state_changed(false), useless_iterations(0), useful_iterations(0), collectives_bound(0), event_processed(0), event_processed_delta(0)
{
    ptp = 0;
    user_region_file = 0;
    func_split = 0;
    input_MPI = 0;
    input_HWC = 0;
    input_fab = 0;
    loop_counter = 0;
    freq = 1000000000; // default value, will be fixed by data file information.

    // Open *com* files
    std::map<PEAT_Rank, std::string> file_input_com_names = get_file_names(globals.input_path, globals.trace_files, globals.com_ext, true);
    std::string file_name = glob.input_path + DIR_DELIMITER + glob.trace_files + glob.app_ext;
    std::map<PEAT_Rank, std::string> file_input_app_names;
    file_input_app_names[0] = file_name;
    file_name = glob.input_path + DIR_DELIMITER + glob.trace_files + glob.ptpd_ext;
    std::map<PEAT_Rank, std::string> file_input_ptp_names;
    file_input_ptp_names[0] = file_name;
    com_store = new Communicators(globals);
    com_store->initialize(file_input_com_names, file_input_app_names, &model_id_storage);

    ptp = new PTP(globals); // No need to set FREQ first
    ptp->initialize(file_input_com_names, file_input_ptp_names, file_input_app_names, &model_id_storage);
    freq = ptp->get_trace_freq(0);

    PEAT_Rank trace_comm_size = com_store->get_app_rank_count();
    coll_store.resize(com_store->get_comms_num() + 3); // 0,1,2 - are predefined MPI_COMM_NULL,SELF and WORLD
    tsc_corrector.resize(trace_comm_size, 0);
    fabric_tsc_corrector.resize(trace_comm_size, 0);
#ifdef PEAT_OMP
    read_mutexes = new PEAT_Mutex[trace_comm_size];
    // match_mutexes = new PEAT_Mutex[trace_comm_size];
    //  ins_rem_mutexes = new PEAT_Mutex[trace_comm_size];
    trace_line_MPIs = new String_Split[omp_get_max_threads() + 1];
    trace_line_HWCs = new String_Split[omp_get_max_threads() + 1];
#endif
    for (std::map<PEAT_Rank, PEAT_Clock>::iterator it = com_store->frame_tsc_start.begin(); it != com_store->frame_tsc_start.end(); ++it) {
        tsc_corrector.at(it->first) = it->second;
        fabric_tsc_corrector.at(it->first) = it->second;
    }

    input_MPI = new File_Storage_GZ(globals.pe_MPI_files_in_rank, PEAT_FIO_FLAG_READ | PEAT_FIO_FLAG_LAZY | PEAT_FIO_FLAG_ICLOSE, NULL, globals.verbose);
    input_HWC = new File_Storage_GZ(globals.pe_HWC_files_in_rank, PEAT_FIO_FLAG_READ | PEAT_FIO_FLAG_LAZY | PEAT_FIO_FLAG_ICLOSE, NULL, globals.verbose);
    input_fab = new File_Storage_GZ(globals.pe_fab_files_in_rank, PEAT_FIO_FLAG_READ | PEAT_FIO_FLAG_LAZY | PEAT_FIO_FLAG_ICLOSE, NULL, globals.verbose);
    output_MPI = new File_Storage_GZ(globals.pe_MPI_files_in_rank_out, PEAT_FIO_FLAG_WRITE, "w9", globals.verbose);
    output_HWC = new File_Storage_GZ(globals.pe_HWC_files_in_rank_out, PEAT_FIO_FLAG_WRITE, "w9", globals.verbose);
    output_bind = new File_Storage_GZ(globals.pe_bind_files_in_rank_out, PEAT_FIO_FLAG_WRITE, "w9", globals.verbose);

    user_region_file = new Address_File_Storage(globals, PEAT_FIO_FLAG_READ | PEAT_FIO_FLAG_LAZY | PEAT_FIO_FLAG_ICLOSE, &model_id_storage);
    {
        // mat's peer/completion/collective bindings, computed once offline over the whole trace (see PET_Bind_Store);
        // mat writes them next to the raw trace by default when run with -m/--matching-only.
        std::vector<PEAT_Rank> all_ranks;
        for (PEAT_Rank r = 0; r < ptp->get_rank_num(); r++)
            all_ranks.push_back(r);
        bind_store.load(globals, *user_region_file, all_ranks, globals.verbose);
        if (globals.verbose)
            bind_store.print_stats(stderr);
    }
    func_split = new PEAT_Function_Splitter(globals);
    transport = new PEAT_Transport(glob, get_target_freq());
    nodes.resize(ptp->get_tgt_node_num());
    messages_bound.resize(ptp->get_rank_num());
    events_processed.resize(ptp->get_rank_num());
    for (std::vector<PET_Node>::iterator node_it = nodes.begin(); node_it != nodes.end(); ++node_it) {
        node_it->create_transenvs(transport);
    }
    if (0 == globals.comm_rank) {
        PCCOUT << "CPU frequency is set to " << get_trace_freq() << "\n";
        PCCOUT << "Target CPU frequency is set to " << get_target_freq() << "\n";
        PCCOUT << "Eager/Rendevous thresholds are set to " << globals.smp_eager_threshold << "," << globals.nic_eager_threshold << "\n";
        if (globals.target_ppn > 0) {
            PCCOUT << "Target PPN is set to " << globals.target_ppn << "\n";
        } else {
            PCCOUT << "Target PPN is set according to input trace\n";
        }
    }
    if (glob.onetime_pass_per_rank > 0) {
        event_processed_delta = glob.onetime_pass_per_rank * trace_comm_size;
    } else {
        event_processed_delta = user_region_file->get_num_events(glob.my_ranks);
    }

    return;
}

PET_Core::~PET_Core()
{
    delete ptp;         // ptp = 0;
    delete transport;   // prof_hw = 0;
    delete com_store;   // com_store = 0;
    delete input_MPI;   // input_MPI = 0;
    delete input_HWC;   // input_HWCpor = 0;
    delete input_fab;   // input_HWCpor = 0;
    delete output_MPI;  // output_MPI = 0;
    delete output_HWC;  // output_HWC = 0;
    delete output_bind; // output_bind = 0;
#ifdef PEAT_OMP
    delete[] read_mutexes;
    delete[] trace_line_MPIs;
    delete[] trace_line_HWCs;
    // delete[] ins_rem_mutexes;
//	delete[] match_mutexes;
#endif
#if 0
    if(user_region_file && (0 == globals.comm_rank)) {
        user_region_file->put_records();
	}
#endif
    delete user_region_file; // user_region_file_orig = 0;
    delete func_split;
}

PEAT_Bool PET_Core::find_ucr(const Event_Address_Info &c_addr, const Event_Address_Info &p_addr, PEAT_Ratio &r)
{
    return false;
}
PEAT_Ratio PET_Core::get_user_code_ratio(PET_Event *prev_event, PET_Event *this_event)
{
    const Event_Address_Info &c_addr = this_event->addrs;
    const Event_Address_Info &p_addr = prev_event->addrs;
    PEAT_Clock uc_len = this_event->get_user_code_length(prev_event);
    PEAT_Ratio r = 1.0 / globals.user_regions_CPI_ratio;
    if (find_ucr(c_addr, p_addr, r)) {
        stat.add_uc_event_stat(Event_Set_Stat::tst_orig, this_event->rank, uc_len, true);
        stat.add_uc_event_stat(Event_Set_Stat::tst_trc, this_event->rank, uc_len, true);
        stat.add_uc_event_stat(Event_Set_Stat::tst_post, this_event->rank, (PEAT_Clock)(uc_len * r), true);
    } else {
        stat.add_uc_event_stat(Event_Set_Stat::tst_orig, this_event->rank, uc_len, false);
        stat.add_uc_event_stat(Event_Set_Stat::tst_trc, this_event->rank, uc_len, false);
        stat.add_uc_event_stat(Event_Set_Stat::tst_post, this_event->rank, uc_len, false);
    }
    return r;
}

PEAT_Bool PET_Core::new_coll(PET_Coll_Event *event)
{
    Coll_Storage collective;
    collective.expected_ranks = com_store->get_ranks_map_from_comm(event->comm);
    collective.size = collective.expected_ranks.size();
    // collective.events[event->rank] = event;
    collective.comm = event->comm;
    collective.func_id = event->func_id;
    coll_store[event->comm].push_back(collective);
    return true;
}

PEAT_Bool PET_Core::get_coll(const PET_Coll_Event *event, std::list<Coll_Storage>::iterator &collective)
{
    if (!coll_store.empty()) {
        for (std::list<Coll_Storage>::iterator coll_it = coll_store[event->comm].begin(); coll_it != coll_store[event->comm].end(); ++coll_it) {
            if (coll_it->events.count(event->rank) == 1) {
                if (coll_it->events.at(event->rank)->ids[0] == event->ids[0]) {
                    collective = coll_it;
                    return true;
                }
            }
        }
    }
    return false;
}

PEAT_Bool PET_Core::get_coll_const(const PET_Coll_Event *event, std::list<Coll_Storage>::const_iterator &collective) const
{
    if (!coll_store.empty()) {
        for (std::list<Coll_Storage>::const_iterator coll_it = coll_store[event->comm].cbegin(); coll_it != coll_store[event->comm].cend(); ++coll_it) {
            if (coll_it->events.count(event->rank) == 1) {
                if (coll_it->events.at(event->rank)->ids[0] == event->ids[0]) {
                    collective = coll_it;
                    return true;
                }
            }
        }
    }
    return false;
}

PEAT_Bool PET_Core::find_coll(PET_Coll_Event *event, std::list<Coll_Storage>::iterator &collective)
{
    if (!coll_store.empty()) {
        for (std::list<Coll_Storage>::iterator coll_it = coll_store[event->comm].begin(); coll_it != coll_store[event->comm].end(); ++coll_it) {
            if (coll_it->check_coll(event)) {
                collective = coll_it;
                goto fn_exit;
            }
        }
    }
    {
        new_coll(event);
        collective = --coll_store[event->comm].end();
    }
fn_exit:
    return true;
}

PEAT_Bool PET_Core::handle_coll(PET_Coll_Event *event)
{
    std::list<Coll_Storage>::iterator collective;
#ifdef PEAT_OMP
    coll_mutex.lock();
#endif
    if (get_coll(event, collective)) {
#ifdef PEAT_OMP
        coll_mutex.unlock();
#endif
        return true;
    }
    if (find_coll(event, collective)) {
        collective->events[event->rank] = event;
        collective->coll_uids[event->rank] = event->ids;
        {
            collective->expected_ranks.erase(event->rank);
        }
        if (collective->expected_ranks.empty()) {
            collective->assembled = true;
        }
        if (collective->events.size() == collective->size) {
            collective->assembled = true;
        }
#ifdef PEAT_OMP
        coll_mutex.unlock();
#endif
        return (collective->update_release());
    }
#ifdef PEAT_OMP
    coll_mutex.unlock();
#endif
    pet_throw_error(this, event, NULL, "Collective not bound.\n");
    // assert(0);
    return false;
}

void PET_Core::process_exhaust_events()
{
#pragma omp parallel
    {
#pragma omp for schedule(dynamic)
        for (std::vector<PET_Node>::iterator node_it = nodes.begin(); node_it != nodes.end(); ++node_it) {
            node_it->collect_garbage();
        }
    }
    // and then collective queue
    for (std::vector<std::list<Coll_Storage>>::iterator com_it = coll_store.begin(); com_it != coll_store.end(); ++com_it) {
        for (std::list<Coll_Storage>::iterator coll_it = com_it->begin(); coll_it != com_it->end(); ++coll_it) {
            if (coll_it->assembled && coll_it->events.empty()) {
                com_it->erase(coll_it);
                if (com_it->empty())
                    break;
                coll_it = com_it->begin();
            }
        }
    }
}
bool PET_Core::remove_orphans(PEAT_Rank rank)
{
    // scan read rank for orphan events
    PEAT_Node node = ptp->get_tgt_node(rank);
    assert(ranks_read[rank]);
    if (nodes.at(node).ranks.at(rank).orphans_processed == true)
        return false;
    for (std::list<PET_Event *>::iterator event_it = nodes.at(node).ranks.at(rank).pairing_rank_events.begin(); event_it != nodes.at(node).ranks.at(rank).pairing_rank_events.end(); ++event_it) {
        PET_Event *evt = *event_it;
        if (evt->get_event_type() == PET_EVENT_TYPE_TOR) {
            PET_ToR_Event *evtt = static_cast<PET_ToR_Event *>(evt);
            if (!evtt->completion_received()) {
#ifdef PEAT_OMP
//				mutexes[rank].lock();
#endif
                evtt->set_treat_as_local();
#ifdef PEAT_OMP
//				mutexes[rank].unlock();
#endif
            }
        }
    }
    nodes.at(node).ranks.at(rank).orphans_processed = true;
    return true;
}
bool PET_Core::process_events()
{
    PEAT_Bool enough_events;
    Estimation_Event *min_event;
#ifdef _DEBUG_CURRENT_EVENTS_
    std::list<PET_Event *> broken_events;
#endif
    PEAT_UID previous_processed_events = event_processed;
    do {
        e_timeline_state_changed = false;
        ;
#ifdef _DEBUG_CURRENT_EVENTS_
        broken_events.clear();
        dbg_event_being_estimated.clear();
        dbg_event_being_estimated_clock.clear();
#endif
        enough_events = true;
        min_event = NULL;
#pragma omp declare reduction(min : Estimation_Event * : omp_out = min_ee_event(omp_out, omp_in)) initializer(omp_priv = NULL)
#pragma omp parallel reduction(&& : enough_events) reduction(min : min_event)
        {
#ifdef PEAT_OMP
            String_Split &trace_line_MPI = trace_line_MPIs[omp_get_thread_num()];
            String_Split &trace_line_HWC = trace_line_HWCs[omp_get_thread_num()];
#endif
            trace_line_MPI.clear();
            trace_line_HWC.clear();
#pragma omp for schedule(dynamic)
            for (std::vector<PET_Node>::iterator node_it = nodes.begin(); node_it != nodes.end(); ++node_it) {
                PEAT_Clock clk;
                for (std::map<PEAT_Rank, PET_Rank>::iterator rank_it = node_it->ranks.begin(); rank_it != node_it->ranks.end(); ++rank_it) {
                    for (std::vector<PET_Thread *>::iterator thread_it = rank_it->second.threads.begin(); thread_it != rank_it->second.threads.end(); ++thread_it) {
                    continue_thread:
                        if ((*thread_it)->event_being_estimated == NULL || (*thread_it)->start_event_it == (*thread_it)->active_events.end()) {
                            (*thread_it)->start_event_it = (*thread_it)->active_events.begin();
                        }
                        PET_Event *evt = NULL;
                        HANDLE_EVENT_CODE handle_err = EVENT_OK;
                        for (std::list<PET_Event *>::iterator event_it = (*thread_it)->start_event_it; handle_err == EVENT_OK; ++event_it) {
                            if (event_it == (*thread_it)->active_events.end()) {
                                if ((*thread_it)->thread_num == 0) { // emergency read events if current queue become processed
                                    if (!is_rank_read(rank_it->first)) {
                                        PEAT_Num local_reads = 0;
                                        PET_Event *read_event = NULL;
                                        do {
                                            read_event = get_event(rank_it->first, trace_line_MPI, trace_line_HWC, NULL);
                                            if (read_event == NULL) {
                                                break;
                                            }
                                            ++local_reads;
                                        } while (local_reads < globals.onetime_pass_per_rank);
                                        if (local_reads > 0)
                                            goto continue_thread;
                                    } else {
                                        break;
                                    }
                                } else {
                                    break;
                                }
                            }
                            handle_err = EVENT_OK;
                            if ((*thread_it)->event_being_estimated != NULL) {
                                if ((*thread_it)->event_being_estimated->is_estimated()) {
                                    (*thread_it)->event_being_estimated = NULL;
                                } else {
#ifdef _DEBUG_CURRENT_EVENTS_
                                    dbg_event_being_estimated[(*event_it)->rank][(*event_it)->thread_id] = *event_it;
#endif
                                    break;
                                }
                            }
                            clk = (*event_it)->e_time.func.in;
                            evt = *event_it;
                            if (evt->is_written)
                                continue;
                            (*thread_it)->start_event_it = (*thread_it)->cached_event_it = event_it;
                            // (*thread_it)->event_being_estimated = evt;
                        handle_again:
                            handle_err = handle_event(evt);
                            if (handle_err == EVENT_NOT_BOUND) { //|| handle_err == EVENT_NOT_READY
                                PEAT_Rank r = evt->get_rank_read_suggestion();
                                if (r >= 0) {
#ifdef PEAT_OMP
                                    if (r != evt->rank)
                                        read_mutexes[r].lock();
#endif
                                    PEAT_Num local_reads = 0;
                                    PEAT_MPI_Func_ID missing = evt->missing_event();
                                    PET_Event *read_event = NULL;
                                    do {
                                        read_event = get_event(r, trace_line_MPI, trace_line_HWC, NULL);
                                        // assert(event);
                                        if (read_event == NULL) {
                                            if (remove_orphans(r)) {
                                                if (r == rank_it->first)
                                                    goto handle_again;
                                                break;
                                            };
                                            pet_throw_error(this, NULL, NULL, "No more events but they are...required. Is the trace valid?\n");
                                            break;
                                        }
                                        ++local_reads;
                                    } while ((missing == TMPI_FUNC_ID_NONE && local_reads < globals.onetime_pass_per_rank) || (missing != TMPI_FUNC_ID_NONE && read_event->func_id != missing));
                                    if (!is_rank_read(r)) {
#ifdef PEAT_OMP
                                        if (r != evt->rank)
                                            read_mutexes[r].unlock();
#endif
                                        // if (r != evt->rank)
                                        goto handle_again; // speed up reading peer
                                    }
                                    // goto handle_again;
#ifdef PEAT_OMP
                                    if (r != evt->rank)
                                        read_mutexes[r].unlock();
#endif
                                }
#ifdef _DEBUG_CURRENT_EVENTS_
                                broken_events.push_back(evt);
#endif
                                break; // can't continue on current thread
                            }
                            if (handle_err == EVENT_INITIATED) {
                                (*thread_it)->event_being_estimated = evt;
#ifdef _DEBUG_CURRENT_EVENTS_
                                // #pragma omp critical
                                {
                                    dbg_event_being_estimated[evt->rank][evt->thread_id] = evt;
                                    dbg_event_being_estimated_clock[evt->e_time.func.in] = evt;
                                }
#endif
                                break;
                            }
                            // if (handle_err != EVENT_OK) break; //next event(s) cannot be estimated anyhow
                            // if (handle_err != EVENT_NOT_READY) break; //next event(s) cannot be estimated anyhow
                        }
#ifdef _DEBUG_CURRENT_EVENTS_
                        // #pragma omp critical
                        {
                            if ((*thread_it)->event_being_estimated) {
                                PET_Event *estimated_event = (*thread_it)->get_currently_estimating_event();
                                dbg_event_being_estimated[estimated_event->rank][estimated_event->thread_id] = estimated_event;
                                dbg_event_being_estimated_clock[estimated_event->e_time.func.in] = estimated_event;
                            }
                        }
#endif
                        if ((*thread_it)->event_being_estimated == NULL) { //&& (*thread_it)->thread_num==0
                            if ((!is_rank_read(rank_it->first))) {         // || handle_err == EVENT_NOT_READY && (*thread_it)->thread_num == 0
#if 0
								if (handle_err == EVENT_NOT_BOUND) {
									PET_Event* read_event = NULL;
#ifdef PEAT_OMP
									read_mutexes[rank_it->first].lock();
#endif
									PEAT_MPI_Func_ID missing = evt->missing_event();
									PEAT_Num local_reads = 0;
									do {
										read_event = get_event(rank_it->first, trace_line_MPI, trace_line_HWC, NULL);
										//assert(read_event);
										/*if (read_event == NULL && is_rank_read(rank_it->first)) { // && (*thread_it)->thread_num==0
											pet_throw_error(this, NULL, NULL, "No more events but they are...required. Is the trace valid?\n");
										}*/
										if (read_event == NULL)  break;
										++local_reads;
									} while ((missing == TMPI_FUNC_ID_NONE && local_reads < globals.onetime_pass_per_rank) || (missing != TMPI_FUNC_ID_NONE && read_event->func_id != missing));
#ifdef PEAT_OMP
									read_mutexes[rank_it->first].unlock();
#endif
								}
								// if ((*thread_it)->thread_num == 0) // is it valid assumption?
								if (is_rank_read(rank_it->first)) { //break only if rank isn't fully read  
									if (!remove_orphans(rank_it->first)) {
										pet_throw_error(this, NULL, NULL, "No more events but they are...required. Is the trace valid?\n");
									}
								}
#endif
                            }
                        }
                        if (handle_err != EVENT_OK && handle_err != EVENT_INITIATED) {           // just in case the're no events in current thread ->  evt is undefined
                            if (evt->func_id != OMPT_PARALLEL_BEGIN_ID || evt->thread_id == 0) { // avoid being stuck on parallel section from auxiliary threads
                                enough_events = false;
                            }
                        }
                        if (!enough_events)
                            break; // some events are missing...skip
                    }
                    /*
                    if (is_rank_read(rank_it->first)) { //break only if rank isn't fully read
                        if (!remove_orphans(rank_it->first) && !enough_events) {
                            pet_throw_error(this, NULL, NULL, "No more events but they are...required. Is the trace valid?\n");
                        }
                    }*/
                }
                min_event = min_ee_event(min_event, node_it->e_timeline.get_min_event());
            }
        }
#ifdef _DEBUG_CURRENT_EVENTS_
        check_progress(enough_events, broken_events); // to set debug point there
#endif
        if (enough_events) {
            /*
            Estimation_Event *min_tmp_event = get_min_ee_entry();
            if (min_event != min_tmp_event) {
                if (min_tmp_event->start != min_event->start) {
                    min_event = min_tmp_event;
                }
            }*/
            TE_Event *te_evt;
            re_fill_queue = false;
            if (min_event) {
                Estimation_Event *min_event_save = min_event;
                do {
                    te_evt = update_te_timelines(min_event->start);
                    if (te_evt) {
                        if (check_for_outstanding_progress_messages(te_evt->owner->node, te_evt->owner->rank, te_evt->t_finish) == 0) {
                            te_evt = complete_te_event(te_evt);
                        } else {
                            re_fill_queue = true;
                        }
                    }
                    if (re_fill_queue) {
                        break;
                    }
                    min_event = get_min_ee_entry();
                } while (te_evt);
                if (!re_fill_queue) {
                    // min_event = get_min_ee_entry();
                    // assert(global_min_event.start <= min_event->start);
                    if (global_min_event.start > min_event->start) {
                        pet_throw_error(this, NULL, min_event, "global_min_event.start > min_event->start.\n");
                    }

                    global_min_event = *min_event;

                    if (!min_event->handle()) {
                        pet_throw_error(this, NULL, min_event, "Estimation Event not handled.\n");
                        // assert(0);
                    }
                    min_event = NULL;
                }
            } else {
                te_evt = update_te_timelines(LLONG_MAX); // one step ahead
                if (te_evt) {
                    if (te_evt->get_evt_tr_type() == TR_TYPE_TRANSMIT) {
                        if (check_for_outstanding_progress_messages(te_evt->n_src, te_evt->src, te_evt->t_finish) == 0) {
                            te_evt = complete_te_event(te_evt);
                        } else {
                            re_fill_queue = true;
                        }
                    }
                    if (te_evt->get_evt_tr_type() == TR_TYPE_RECEIVE) {
                        if (check_for_outstanding_progress_messages(te_evt->n_dst, te_evt->dst, te_evt->t_finish) == 0) {
                            te_evt = complete_te_event(te_evt);
                        } else {
                            re_fill_queue = true;
                        }
                    }

                } else {
                    /* for ( std::vector<PET_Node>::iterator node_it = nodes.begin(); node_it != nodes.end(); ++node_it ) {
                        for ( std::map<PEAT_Rank, PET_Rank>::iterator rank_it = node_it->ranks.begin(); rank_it != node_it->ranks.end(); ++rank_it ) {
                            if ( rank_it->second.event_being_estimated ) {
                                rank_it->second.event_being_estimated->check_estimated();
                            }
                        }
                        min_event = min_ee_event(min_event, node_it->e_timeline.get_min_event());
                    } */
                    if (min_event == NULL) {
                        no_more_events(true); // to set debug point here
                        return true;
                    }
                }
                if (re_fill_queue)
                    break;
            }
        }
    } while (e_timeline_state_changed && previous_processed_events + event_processed_delta > event_processed);
    return false;
}
void PET_Core::write_ompt_regions()
{
#pragma omp parallel
    {
#pragma omp for schedule(dynamic)
        for (std::vector<PET_Node>::iterator node_it = nodes.begin(); node_it != nodes.end(); ++node_it) {
            for (std::map<PEAT_Rank, PET_Rank>::iterator rank_it = node_it->ranks.begin(); rank_it != node_it->ranks.end(); ++rank_it) {
                for (std::list<PET_Event *>::iterator event_it = rank_it->second.omp_parallel_sections.begin(); event_it != rank_it->second.omp_parallel_sections.end(); ++event_it) {
                    PET_Event *event = *event_it;
                    PEAT_Clock head = event->t_time.func.in - event->e_time.func.in;
                    PEAT_Clock tail = event->t_time.func.out - event->e_time.func.out;
                    event->sub_t_ae = tail - head; // to eliminate error thrown during write;
                    event->write_event(output_MPI, output_HWC, output_bind);
                }
            }
        }
    }
}
TE_Event *min_te_event(TE_Event *e, TE_Event *e_tmp)
{
    if (e == NULL) {
        return e_tmp;
    }
    if (e_tmp == NULL) {
        return e;
    }
    if (e_tmp->t_finish < e->t_finish)
        return e_tmp;
    return e;
}

TE_Event *PET_Core::update_te_timelines_parallel(PEAT_Clock clk)
{
    TE_Event *e = NULL;
#pragma omp declare reduction(min : TE_Event * : omp_out = min_te_event(omp_out, omp_in)) initializer(omp_priv = NULL)
#pragma omp parallel shared(clk) reduction(min : e)
    {
#pragma omp for schedule(dynamic)
        for (std::vector<PET_Node>::iterator node_it = nodes.begin(); node_it != nodes.end(); ++node_it) {
            e = min_te_event(e, node_it->update_te_timelines_write(clk));
        }
#pragma omp for schedule(dynamic)
        for (std::vector<PET_Node>::iterator node_it = nodes.begin(); node_it != nodes.end(); ++node_it) {
            e = min_te_event(e, node_it->update_te_timelines_read(clk));
        }
    }
    return e;
}

TE_Event *PET_Core::update_te_timelines(PEAT_Clock clk)
{
    TE_Event *evt = update_te_timelines_parallel(clk);
    return evt;
}

TE_Event *PET_Core::insert_te_event(TE_Event *evt)
{
    return (nodes.at(evt->n_src).insert_te_event(evt));
}

TE_Event *PET_Core::complete_te_event(TE_Event *evt)
{
    // assert(evt);
    // assert(evt->tr_env);
    if (evt == NULL) {
        pet_throw_error(this, NULL, NULL, "Event is NULL.\n");
    }
    if (evt->get_evt_tr_type() == TR_TYPE_TRANSMIT)
        return (nodes.at(evt->n_src).complete_te_event(evt));
    if (evt->get_evt_tr_type() == TR_TYPE_RECEIVE)
        return (nodes.at(evt->n_dst).complete_te_event(evt));
    return NULL;
}
PEAT_Num PET_Core::check_for_outstanding_progress_messages(PEAT_Node node, PEAT_Rank rank, PEAT_Clock time)
{
    return nodes.at(node).ranks.at(rank).check_for_outstanding_progress_messages(time);
}

void PET_Core::cache()
{
    PEAT_Rank ranks = globals.pe_MPI_files_in_rank.size();
    PEAT_Rank start_rank = globals.pe_MPI_files_in_rank.begin()->first;
    PEAT_Rank finish_rank = globals.pe_MPI_files_in_rank.rbegin()->first;
    PEAT_Rank ranks_processed = 0;
#pragma omp parallel for schedule(dynamic) // private(trace_line_MPI, trace_line_HWC)
    for (PEAT_Rank rank = start_rank; rank <= finish_rank; ++rank) {
        // PET_Node &node = nodes[ptp->get_tgt_node(rank->first)];
        PEAT_Ratio start_caching = Wtime();
        PET_Rank *pet_rank = get_pet_rank(rank);
        pet_rank->check_and_allocate_threads(user_region_file->last_thread_idx.at(rank));
#pragma omp critical
        {
            printf("Caching rank %d.\n", rank);
            fflush(stdout);
        }
        PEAT_UID last_event_idx = user_region_file->last_event_idx[rank];
        PEAT_UID events_to_read = last_event_idx * globals.read_ahead_ratio;
        PET_Event *event;
        do {
#ifdef PEAT_OMP
            String_Split &trace_line_MPI = trace_line_MPIs[omp_get_thread_num()];
            String_Split &trace_line_HWC = trace_line_HWCs[omp_get_thread_num()];
#endif
            trace_line_MPI.clear();
            trace_line_HWC.clear();
            event = get_event(rank, trace_line_MPI, trace_line_HWC, NULL);
        } while (event->ids[0] < events_to_read);
        PEAT_Ratio time = Wtime() - start_caching;
        PEAT_Ratio cache_bw = (events_to_read + 1) / time;
#pragma omp critical
        {
            ranks_processed++;
            printf("Done caching rank %d. %lld of %lld events read. Speed is %.2lf events per second. Ranks processed %d of %d.\n", rank, events_to_read + 1, last_event_idx + 1, cache_bw, ranks_processed, ranks);
            fflush(stdout);
        }
    }
}

static void dump_event_line(FILE *fp, const char *indent, PET_Event *evt)
{
    fprintf(fp, "%s%s(%s) t=%d in=%lld out=%lld written=%d initiated=%d started=%d finished=%d estimated=%d bound=%d reason=%d ee=%lld nested=%lld\n",
            indent, global_func_id.name_by_idx(evt->func_id), evt->ids_str().c_str(), (int)evt->thread_id,
            (long long)evt->e_time.func.in, (long long)evt->e_time.func.out,
            (int)evt->is_written, (int)evt->is_initiated, (int)evt->estimation_started, (int)evt->finish_calculations,
            (int)evt->is_estimated(), (int)evt->is_bound(), (int)evt->pending_reason, (long long)evt->remaining_ee_events, (long long)evt->nested_events.size());
}

void pet_dump_event_tree(FILE *fp, int depth, PET_Event *evt)
{
    char indent[64];
    snprintf(indent, sizeof(indent), "%*s", depth * 4, "");
    dump_event_line(fp, indent, evt);
    if (depth < 4) {
        PEAT_Num size = evt->nested_events.size();
        for (PEAT_Num i = 0; i < size; i++) {
            pet_dump_event_tree(fp, depth + 1, evt->nested_events[i]);
        }
    }
}

void PET_Core::dump_stall_state(FILE *fp)
{
    fprintf(fp, "==== estimation stalled: %lld events processed, %ld useless / %ld useful iterations ====\n", (long long)event_processed, useless_iterations, useful_iterations);
    for (std::vector<PET_Node>::iterator node_it = nodes.begin(); node_it != nodes.end(); ++node_it) {
        Estimation_Event *ee = node_it->e_timeline.get_min_event();
        if (ee) {
            fprintf(fp, "node pending estimation event: type=%d start=%lld owner=%s(%lld) submiter=%s(%lld)\n", (int)ee->type, (long long)ee->start,
                    ee->owner ? global_func_id.name_by_idx(ee->owner->func_id) : "-", ee->owner ? (long long)ee->owner->ids[0] : -1LL,
                    ee->submiter ? global_func_id.name_by_idx(ee->submiter->func_id) : "-", ee->submiter ? (long long)ee->submiter->ids[0] : -1LL);
        }
        for (std::map<PEAT_Rank, PET_Rank>::iterator rank_it = node_it->ranks.begin(); rank_it != node_it->ranks.end(); ++rank_it) {
            fprintf(fp, "rank %d read=%d\n", rank_it->first, (int)is_rank_read(rank_it->first));
            for (std::vector<PET_Thread *>::iterator thread_it = rank_it->second.threads.begin(); thread_it != rank_it->second.threads.end(); ++thread_it) {
                int shown = 0;
                for (std::list<PET_Event *>::iterator event_it = (*thread_it)->active_events.begin(); event_it != (*thread_it)->active_events.end() && shown < 6; ++event_it) {
                    PET_Event *evt = *event_it;
                    if (evt->is_written)
                        continue;
                    pet_dump_event_tree(fp, 1, evt);
                    if (shown == 0 && !evt->estimation_started) { // what handle_event() would decide for the head event
                        PEAT_Bool b = evt->bind();
                        PEAT_Bool r = b ? evt->ready_to_estimate() : false;
                        fprintf(fp, "      head: bind()=%d ready_to_estimate()=%d\n", (int)b, (int)r);
                        std::list<Coll_Storage>::const_iterator collective;
                        PET_Coll_Event *cevt = dynamic_cast<PET_Coll_Event *>(evt);
                        if (cevt && get_coll_const(cevt, collective)) {
                            fprintf(fp, "      collective: assembled=%d members=%lld/%lld expected=%lld\n", (int)collective->assembled, (long long)collective->events.size(), (long long)collective->size, (long long)collective->expected_ranks.size());
                            for (std::map<PEAT_Rank, PET_Coll_Event *>::const_iterator m = collective->events.cbegin(); m != collective->events.cend(); ++m) {
                                fprintf(fp, "      member rank %d: %s(%lld) bound=%d ready_to_bind=%d estimated=%d written=%d\n", m->first, global_func_id.name_by_idx(m->second->func_id), (long long)m->second->ids[0], (int)m->second->is_bound(), (int)m->second->ready_to_bind(), (int)m->second->is_estimated(), (int)m->second->is_written);
                                for (PET_Event *kid = m->second->get_first_child_event(); kid != m->second; kid = m->second->get_next_child_event()) {
                                    fprintf(fp, "         kid %s(%s) bound=%d completion=%d\n", global_func_id.name_by_idx(kid->func_id), kid->ids_str().c_str(), (int)kid->is_bound(), (int)kid->completion_received());
                                }
                            }
                        }
                        const std::vector<PET_Event *> &kids = evt->child_iteration_list();
                        for (PEAT_Num k = 0; k < kids.size(); k++) {
                            if (kids[k])
                                fprintf(fp, "      kid %s(%s) bound=%d ready=%d completion=%d\n", global_func_id.name_by_idx(kids[k]->func_id), kids[k]->ids_str().c_str(), (int)kids[k]->is_bound(), (int)kids[k]->ready_to_estimate(), (int)kids[k]->completion_received());
                        }
                    }
                    shown++;
                }
            }
        }
    }
    fflush(fp);
}

void PET_Core::estimate()
{
    const PEAT_UID total_event_num = user_region_file->get_num_events(globals.my_ranks);
    long long last_tsc = 0;
    int cur_percent = 1;
    // sort();
    // timeline.clear();
    cache();
#ifdef PEAT_OMP
    int thread_count = omp_get_num_procs();
    if (thread_count > ptp->get_tgt_node_num()) {
        omp_set_num_threads(ptp->get_tgt_node_num());
    }
#endif
    PEAT_Ratio start_estimation = Wtime();
    PEAT_Ratio prev_step = start_estimation;
    bool finished = false;

    PEAT_UID prev_event_num = 0;
    PEAT_UID stall_event_num = event_processed;
    PEAT_Ratio stall_start = Wtime();
    do {
        finished = process_events();
        process_exhaust_events();
        if (globals.verbose) { // stall detector: nothing estimated for a while
            if (event_processed != stall_event_num) {
                stall_event_num = event_processed;
                stall_start = Wtime();
            } else if (Wtime() - stall_start > 20.0) {
                dump_stall_state(stderr);
                throw std::runtime_error("Estimation stalled (see the state dump above).");
            }
        }
        // Progress bar
        if (0 == globals.comm_rank) {
            if (event_processed > total_event_num * cur_percent / 100) {
                PEAT_Ratio cur_step = Wtime();
                PEAT_Ratio estimation_bw = ((PEAT_Ratio)(event_processed)) / (cur_step - start_estimation);
                PEAT_Ratio instant_bw = ((PEAT_Ratio)(event_processed - prev_event_num)) / (cur_step - prev_step);
                prev_event_num = event_processed;
                prev_step = cur_step;
                if (globals.verbose) {
                    // print_elems_in_queue();
                }
                printf("%d%% Speed is %.2lf (%.2lf) events per second.\n", cur_percent, instant_bw, estimation_bw);
                fflush(stdout);
                cur_percent++;
            }
        }
    } while (event_processed < total_event_num && !finished);
    if (event_processed < total_event_num) {
        fprintf(stderr, "Warning: estimation ended with %lld of %lld events processed; the remaining ones could not be estimated (output is incomplete).\n",
                (long long)event_processed, (long long)total_event_num);
        if (globals.verbose)
            dump_stall_state(stderr);
    }
    write_ompt_regions();
    std::ostringstream msg;
    msg << "(rank " << globals.comm_rank << ": Loop counter " << loop_counter << " Perf metric: " << ")\n"; // todo insert metric here
    std::cerr << msg.str();
    for (std::vector<PET_Node>::const_iterator it = nodes.begin(); it != nodes.end(); ++it) {
        for (std::map<PEAT_Rank, PET_Rank>::const_iterator r_it = it->ranks.begin(); r_it != it->ranks.end(); ++r_it) {
            const PEAT_Rank rank = r_it->first;
            std::cout << "Speedup on Rank " << rank << " is " << r_it->second.sub_time << " Clocks. Tractor Overhead is "
                      << r_it->second.tractor_overhead_time << "\n";
        }
    }

    return;
}
/*
void PET_Core::update_current_event(PEAT_Clock start, PET_Event *event)
{
    PET_Event *evt = get_entry(event->rank, event->thread_id, start);
    if (evt == event) {
        remove_entry(event, start);
    }
    insert_entry(event);
}
*/
TE_Event *PET_Core::update_te_timelines_serial(PEAT_Clock clk)
{
    TE_Event *e = NULL;
    for (std::vector<PET_Node>::iterator node_it = nodes.begin(); node_it != nodes.end(); ++node_it) {
        e = min_te_event(e, node_it->update_te_timelines_write(clk));
    }
    for (std::vector<PET_Node>::iterator node_it = nodes.begin(); node_it != nodes.end(); ++node_it) {
        e = min_te_event(e, node_it->update_te_timelines_read(clk));
    }
    return e;
}

PEAT_Bool PET_Core::assign_transport(TE_Event &evt, PEAT_MPI_Func_ID func_id, PEAT_MPI_Func_ID parent_id)
{
    PEAT_Rank src = evt.src;
    PEAT_Rank dst = evt.dst;
    PEAT_Node n_src = ptp->get_tgt_node(src);
    PEAT_Node n_dst = ptp->get_tgt_node(dst);
    PEAT_Proc p_src = ptp->get_tgt_package(src);
    PEAT_Proc p_dst = ptp->get_tgt_package(dst);
    evt.n_src = n_src;
    evt.n_dst = n_dst;
    evt.p_src = p_src;
    evt.p_dst = p_dst;
    PEAT_Transports t_num = PEAT_TRANSPORT_MAX;
    if (n_src != n_dst) {
        // internode
        t_num = PEAT_TRANSPORT_NIC;
    } else {
        if (p_src != p_dst) {
            t_num = PEAT_TRANSPORT_QPI;
        } else {
            if (globals.HBM_enabled) {
                t_num = PEAT_TRANSPORT_HBM;
            } else {
                t_num = PEAT_TRANSPORT_DDR;
            }
        }
    }
    // assert(t_num != PEAT_TRANSPORT_MAX);
    if (t_num == PEAT_TRANSPORT_MAX) {
        pet_throw_error(this, evt.owner, &evt, "No aproppriate transport found.\n");
    }
    if (parent_id == TMPI_FUNC_ID_NONE)
        parent_id = func_id;
    PEAT_Num numa_ppn = ptp->get_numa_pp(n_src, p_src);
    evt.tr_env = transport->get_env(t_num, func_id, parent_id, evt.size, numa_ppn, globals.transport_selector);
    evt.tr_env.func_id = func_id;
    return true;
}

PEAT_Bool PET_Core::write_event_and_update_stat(PET_Event *event)
{
    if (!event->sub_t_applied && !get_mo()) {
        if (event->is_written)
            return false;
    }
    std::cerr << global_func_id.name_by_idx(event->func_id) << " Rank:" << event->rank << "(" << event->thread_id << ").ID:" << event->ids_str() << ". Done." << std::endl;
    event = event->write_event(output_MPI, output_HWC, output_bind);
    if (!event)
        return false;
    event_processed++;

    if (event) {
        // update stat here
        if (event->check_real_tx()) {
            stat.add_mpi_length_stat(Event_Set_Stat::tst_orig, event->rank, event->func_id, event->get_tx_bytes(), event->o_event_parts.meat);
            stat.add_mpi_length_stat(Event_Set_Stat::tst_trc, event->rank, event->func_id, event->get_tx_bytes(), event->o_event_parts.meat);
            stat.add_mpi_length_stat(Event_Set_Stat::tst_post, event->rank, event->func_id, event->get_tx_bytes(), event->event_parts.meat);
        }

        PEAT_Bool is_rndv = false, is_not_verified = false;
        if (event->get_event_type() == PET_EVENT_TYPE_TOR) {
            PET_ToR_Event *evt = (PET_ToR_Event *)event;
            if (evt->check_tx()) {
                is_rndv = evt->is_rdv(PEAT_DIRECTION_TX);
                is_not_verified = !evt->guess_blocking();
            }
        }
        stat.add_event_stat(Event_Set_Stat::tst_orig, event->get_msg_transport_id(), event->rank, event->func_id, event->parent_func_id, event->get_ranks_num(), event->get_real_tx_bytes(), event->get_real_rx_bytes(),
                            event->o_event_parts.prolog, event->o_event_parts.meat, event->get_centre_code(Event_Set_Stat::tst_orig), event->o_event_parts.epilog,
                            event->o_event_parts.stat.wait, event->o_event_parts.stat.overlap, is_rndv, is_not_verified);
        stat.add_event_stat(Event_Set_Stat::tst_trc, event->get_msg_transport_id(), event->rank, event->func_id, event->parent_func_id, event->get_ranks_num(), event->get_real_tx_bytes(), event->get_real_rx_bytes(),
                            event->o_event_parts.prolog, event->o_event_parts.meat, event->get_centre_code(Event_Set_Stat::tst_trc), event->o_event_parts.epilog,
                            event->o_event_parts.stat.wait, event->o_event_parts.stat.overlap, is_rndv, is_not_verified);
        if (!get_mo()) {
            stat.add_event_stat(Event_Set_Stat::tst_post, event->get_msg_transport_id(), event->rank, event->func_id, event->parent_func_id, event->get_ranks_num(), event->get_real_tx_bytes(), event->get_real_rx_bytes(),
                                event->event_parts.prolog, event->event_parts.stat.fabric, event->get_centre_code(Event_Set_Stat::tst_post), event->event_parts.epilog,
                                event->event_parts.stat.wait, event->event_parts.stat.overlap, is_rndv, is_not_verified);
        }
        if (globals.gather_transport_stat) {
            stat.add_ts_fs_event_stat(Event_Set_Stat::tst_trc, event);
            //		stat.add_ts_fs_event_stat(Event_Set_Stat::tst_post, event);
        }

        // time this call spent progressing other (nonblocking) collectives is not its own: subtract it here, credit the owners below
        PEAT_Clock foreign_orig = event->foreign_progress_time(0), foreign_trc = event->foreign_progress_time(1), foreign_post = event->foreign_progress_time(2);
        stat.add_mpi_event_stat(Event_Set_Stat::tst_orig, event->rank, event->func_id,
                                event->o_time.func.in,
                                event->o_time.func.out,
                                event->o_time.func.duration() - foreign_orig,
                                event->o_event_parts.prolog + event->o_event_parts.epilog +
                                    event->o_time.mpi.in - event->o_time.func.in + event->o_time.func.out - event->o_time.mpi.out);
        stat.add_mpi_event_stat(Event_Set_Stat::tst_trc, event->rank, event->func_id,
                                event->t_time.func.in,
                                event->t_time.func.out,
                                event->t_time.func.duration() - foreign_trc,
                                event->o_event_parts.prolog + event->o_event_parts.epilog);
        if (!get_mo()) {
            stat.add_mpi_event_stat(Event_Set_Stat::tst_post, event->rank, event->func_id,
                                    event->e_time.func.in,
                                    event->e_time.func.out,
                                    event->e_time.func.duration() - foreign_post,
                                    event->event_parts.prolog + event->event_parts.epilog);
        }
        if (event->get_first_child_event() != event) {
            for (PET_Event *evt = event->get_first_child_event(); evt != event; evt = event->get_next_child_event()) {
                PEAT_MPI_Func_ID stat_parent_func = event->parent_func_id;
                if (evt->is_foreign_progress()) { // owned by another collective: its statistics go there
                    stat_parent_func = evt->progress_owner->func_id;
                    if (evt->container_event == NULL || !evt->container_event->is_foreign_progress()) { // count a foreign subtree once
                        stat.add_progress_stat(Event_Set_Stat::tst_orig, evt->rank, stat_parent_func, evt->o_time.func.duration());
                        stat.add_progress_stat(Event_Set_Stat::tst_trc, evt->rank, stat_parent_func, evt->t_time.func.duration());
                        if (!get_mo())
                            stat.add_progress_stat(Event_Set_Stat::tst_post, evt->rank, stat_parent_func, evt->e_time.func.duration());
                    }
                }
                if (evt->check_real_tx()) {
                    stat.add_mpi_length_stat(Event_Set_Stat::tst_orig, evt->rank, evt->func_id, evt->get_real_tx_bytes(), evt->o_event_parts.meat);
                    stat.add_mpi_length_stat(Event_Set_Stat::tst_trc, evt->rank, evt->func_id, evt->get_real_tx_bytes(), evt->o_event_parts.meat);
                    if (!get_mo()) {
                        stat.add_mpi_length_stat(Event_Set_Stat::tst_post, evt->rank, evt->func_id, evt->get_real_tx_bytes(), evt->event_parts.meat);
                    }
                }
                PEAT_Bool is_rndv = false, is_not_verified = false;
                if (event->get_event_type() == PET_EVENT_TYPE_TOR) {
                    PET_ToR_Event *evt_x = (PET_ToR_Event *)evt;
                    if (evt_x->check_tx()) {
                        is_rndv = evt_x->is_rdv(PEAT_DIRECTION_TX);
                        is_not_verified = evt_x->guess_blocking();
                    }
                }
                stat.add_event_stat(Event_Set_Stat::tst_orig, evt->get_msg_transport_id(), evt->rank, evt->func_id, stat_parent_func, evt->get_ranks_num(), evt->get_real_tx_bytes(), evt->get_real_rx_bytes(),
                                    evt->o_event_parts.prolog, evt->o_event_parts.meat, evt->get_centre_code(Event_Set_Stat::tst_orig), evt->o_event_parts.epilog,
                                    evt->o_event_parts.stat.wait, evt->o_event_parts.stat.overlap, is_rndv, is_not_verified);
                stat.add_event_stat(Event_Set_Stat::tst_trc, evt->get_msg_transport_id(), evt->rank, evt->func_id, stat_parent_func, evt->get_ranks_num(), evt->get_real_tx_bytes(), evt->get_real_rx_bytes(),
                                    evt->o_event_parts.prolog, evt->o_event_parts.meat, evt->get_centre_code(Event_Set_Stat::tst_trc), evt->o_event_parts.epilog,
                                    evt->o_event_parts.stat.wait, evt->o_event_parts.stat.overlap, is_rndv, is_not_verified);
                if (!get_mo()) {
                    stat.add_event_stat(Event_Set_Stat::tst_post, evt->get_msg_transport_id(), evt->rank, evt->func_id, stat_parent_func, evt->get_ranks_num(), evt->get_real_tx_bytes(), evt->get_real_rx_bytes(),
                                        evt->event_parts.prolog, evt->event_parts.stat.fabric, evt->get_centre_code(Event_Set_Stat::tst_post), evt->event_parts.epilog,
                                        evt->event_parts.stat.wait, evt->event_parts.stat.overlap, is_rndv, is_not_verified);
                }
                if (globals.gather_transport_stat) {
                    stat.add_ts_fs_event_stat(Event_Set_Stat::tst_trc, evt);
                    //				stat.add_ts_fs_event_stat(Event_Set_Stat::tst_post, evt);
                }
            }
        }
#if 0
		//overall stat
		stat.add_event_stat( Event_Set_Stat::tst_orig, -1, event->func_id, event->get_tx_bytes(), event->get_rx_bytes(),
			event->o_event_parts.prolog + event->o_event_parts.epilog,
			event->o_event_parts.stat.fabric, event->o_event_parts.stat.wait, event->o_event_parts.stat.overlap );
		stat.add_event_stat( Event_Set_Stat::tst_post, -1, event->func_id, event->get_tx_bytes(), event->get_rx_bytes(),
			event->event_parts.prolog + event->event_parts.epilog,
			event->event_parts.stat.fabric, event->event_parts.stat.wait, event->o_event_parts.stat.overlap );
#endif
        register_parallel_region(event);
    }
    return true;
}
PEAT_Ratio PET_Core::print_report(const Event_Set_Stat::t_stat_type type, const PEAT_Record_Type line_prefix)
{
    PEAT_Record_Type line_prefix_total = PEAT_RT_Remark;
    std::string model_type("<unknown>");
    switch (line_prefix) {
    case PEAT_RT_PET_Final_Stat_Orig:
        line_prefix_total = PEAT_RT_PET_Final_Stat_Orig_Total;
        model_type = "Original";
        break;
    case PEAT_RT_PET_Final_Stat_Tractor:
        line_prefix_total = PEAT_RT_PET_Final_Stat_Tractor_Total;
        model_type = "Tractor";
        break;
    case PEAT_RT_PET_Final_Stat_Estim:
        line_prefix_total = PEAT_RT_PET_Final_Stat_Estim_Total;
        model_type = "Estimated";
        break;
    }
    PEAT_Clock total_start = LLONG_MAX, total_init_start = LLONG_MAX, total_fin_start = LLONG_MAX;
    PEAT_Clock total_stop = LLONG_MIN, total_init_stop = LLONG_MIN, total_fin_stop = LLONG_MIN;
    PEAT_Clock all_ranks_mpi_len = 0, all_ranks_user_len = 0, all_ranks_user_len_covered = 0, all_ranks_init_len = 0, all_ranks_fin_len = 0;
    PEAT_Rank worst_covered_rank = -1, min_fabric_len_rank = -1;
    PEAT_Clock worst_covered_rank_covered = 0, worst_covered_rank_non_covered = 0, non_covered_max = 0,
               min_fabric_len = LLONG_MAX, min_fabric_rank_len = 0;
    PEAT_FREQ local_freq = 0; // get_trace_freq();//TODO add option to set it in 1 to show TSC instead second
    if (Event_Set_Stat::tst_post == type) {
        local_freq = get_target_freq();
    } else if (Event_Set_Stat::tst_orig == type || Event_Set_Stat::tst_trc == type) {
        local_freq = get_trace_freq();
    } else {
        assert(!"unknown Event_Set_Stat in PET_Core::print_report");
    }
    std::string pcon_mode;
    if (globals.pcontrol_mode) {
        pcon_mode = "ON";
    } else {
        pcon_mode = "OFF";
    }
    std::ostringstream head_line_str;
    head_line_str << model_type << " model. CPU " << local_freq << "Hz. Pcontrol " << pcon_mode;
    std::string head_line = head_line_str.str();
    std::string bottom_line = model_type + " total";
    std::string bottom_par_line = model_type + " Parallel total";
    assert(local_freq);
#if 0
    const int ratio_len = 5;
    const int clock_len_min = 8;
    const int clock_len_max = 14;
#else
    const int ratio_len = 6;
    const int clock_len_min = 8;
    const int clock_len_max = 14;
#endif
    const int separator_length = 4 * ratio_len + 7 * clock_len_min + 4 * clock_len_max + 15;
    SEPARATOR(separator_length);
    printf("%2d", PEAT_RT_Remark);
    printf(":%-*s", ratio_len + 2 * clock_len_min + 2 * clock_len_max + 4, head_line.c_str());
    printf(":%*s%*s", PRINT_PARAMETERS(ratio_len + 5 * clock_len_min + 5, "MPI length"));
    printf(":%*s%*s", PRINT_PARAMETERS(2 * ratio_len + 2 * clock_len_max + 4, "User regions length"));
    printf("\n");
    SEPARATOR(separator_length);
    printf("%2d", PEAT_RT_Remark);
    printf(":%*s", ratio_len, "");
    printf(":%*s%*s", PRINT_PARAMETERS(2 * clock_len_min + 1, "Events number"));
    printf(":%*s%*s", PRINT_PARAMETERS(2 * clock_len_max + 1, "Model time(sec)"));
    printf(":%*s%*s", PRINT_PARAMETERS(2 * clock_len_min + 1, "Messages(sec)"));
    printf(":%*s%*s", PRINT_PARAMETERS(3 * clock_len_min + ratio_len + 3, "Events (sec)"));
    printf(":%*s%*s", PRINT_PARAMETERS(ratio_len + clock_len_max + 1, "Covered "));
    printf(":%*s%*s", PRINT_PARAMETERS(ratio_len + clock_len_max + 1, "Non-covered "));
    printf("\n");
    SEPARATOR(separator_length);
    printf("%2d", PEAT_RT_Remark);
    printf(":%*s%*s", PRINT_PARAMETERS(ratio_len, "Rank "));
    printf(":%*s%*s", PRINT_PARAMETERS(clock_len_min, "MPI "));
    printf(":%*s%*s", PRINT_PARAMETERS(clock_len_min, "Num URC"));
    printf(":%*s%*s", PRINT_PARAMETERS(clock_len_max, "Start "));
    printf(":%*s%*s", PRINT_PARAMETERS(clock_len_max, "End "));
    printf(":%*s%*s", PRINT_PARAMETERS(clock_len_min, "Min "));
    printf(":%*s%*s", PRINT_PARAMETERS(clock_len_min, "Max "));
    printf(":%*s%*s", PRINT_PARAMETERS(clock_len_min, "fabric%"));
    printf(":%*s%*s", PRINT_PARAMETERS(clock_len_min, "cpu% "));
    printf(":%*s%*s", PRINT_PARAMETERS(clock_len_min, "Total "));
    printf(":%*s%*s", PRINT_PARAMETERS(ratio_len, "%"));
    printf(":%*s%*s", PRINT_PARAMETERS(clock_len_max, "Clocks "));
    printf(":%*s%*s", PRINT_PARAMETERS(ratio_len, "%"));
    printf(":%*s%*s", PRINT_PARAMETERS(clock_len_max, "Clocks "));
    printf(":%*s%*s", PRINT_PARAMETERS(ratio_len, "%"));
    printf("\n");
    SEPARATOR(separator_length);
    const std::map<PEAT_Rank, Stat> &stat_local = stat.get_stat(type);
    for (std::map<PEAT_Rank, Stat>::const_iterator it = stat_local.begin(); it != stat_local.end(); ++it) {
        const Stat &st = it->second;
        PEAT_Ratio rank_len = PEAT_Ratio((st.rank_end - st.rank_start));
        if ((Event_Set_Stat::tst_orig == type || Event_Set_Stat::tst_trc == type) && globals.pcontrol_mode) {
            rank_len -= st.pcontrol_removed_user_code_length;
        }
        total_start = __min__(total_start, st.rank_start);
        total_stop = __max__(total_stop, st.rank_end);
        total_init_start = __min__(total_init_start, st.rank_mpi_init_start);
        total_init_stop = __max__(total_init_stop, st.rank_mpi_init_end);
        total_fin_start = __min__(total_fin_start, st.rank_mpi_finalize_start);
        total_fin_stop = __max__(total_fin_stop, st.rank_mpi_finalize_end);

        all_ranks_mpi_len += (st.cpu_len + st.fabric_len);
        all_ranks_user_len += st.covered_UR_length + st.uncovered_UR_length;
        all_ranks_user_len_covered += st.covered_UR_length;
        all_ranks_init_len += st.rank_mpi_init_end - st.rank_mpi_init_start;
        all_ranks_fin_len += st.rank_mpi_finalize_end - st.rank_mpi_finalize_start;
        if (non_covered_max < st.uncovered_UR_length) {
            non_covered_max = st.uncovered_UR_length;
            worst_covered_rank = it->first;
            worst_covered_rank_covered = st.covered_UR_length;
            worst_covered_rank_non_covered = st.uncovered_UR_length;
        }
        printf("%d", line_prefix);
        printf(":%*d", ratio_len, it->first);
        printf(":%*lld", clock_len_min, st.num_events);
        printf(":%*lld", clock_len_min, st.changed_UR_num);
        printf(":%*.3f", clock_len_max, (PEAT_Ratio)st.rank_start / (PEAT_Ratio)local_freq);
        printf(":%*.3f", clock_len_max, (PEAT_Ratio)st.rank_end / (PEAT_Ratio)local_freq);
        printf(":%*.2e", clock_len_min, (PEAT_Ratio)st.get_min_msg_length() / (PEAT_Ratio)local_freq);
        printf(":%*.2e", clock_len_min, (PEAT_Ratio)st.get_max_msg_length() / (PEAT_Ratio)local_freq);
        //        printf(":%*.2e", clock_len_min, st.getmin_event_length() / local_freq);
        //        printf(":%*.2e", clock_len_min, st.getmax_event_length() / local_freq);
        if (min_fabric_len > st.fabric_len) {
            min_fabric_len = st.fabric_len;
            min_fabric_len_rank = it->first;
            min_fabric_rank_len = (PEAT_Clock)rank_len;
        }
        printf(":%*.2f", clock_len_min, 100 * PEAT_Ratio(st.fabric_len) / rank_len);
        printf(":%*.2f", clock_len_min, 100 * PEAT_Ratio(st.cpu_len) / rank_len);

        printf(":%*.2e", clock_len_min, (st.cpu_len + st.fabric_len) / (PEAT_Ratio)local_freq);
        printf(":%*.2f", ratio_len, 100 * PEAT_Ratio(st.cpu_len + st.fabric_len) / rank_len);

        PEAT_Ratio add = (st.covered_UR_length + st.uncovered_UR_length) == 0;

        printf(":%*lld", clock_len_max, st.covered_UR_length);
        printf(":%*.2f", ratio_len, 100 * PEAT_Ratio(st.covered_UR_length) / PEAT_Ratio(st.covered_UR_length + st.uncovered_UR_length + add)); // rank_len);
        printf(":%*lld", clock_len_max, st.uncovered_UR_length);
        printf(":%*.2f", ratio_len, 100 * PEAT_Ratio(st.uncovered_UR_length) / PEAT_Ratio(st.covered_UR_length + st.uncovered_UR_length + add)); // rank_len);
        printf("\n");
    }
    SEPARATOR(separator_length);
    PEAT_Ratio model_total_length = (PEAT_Ratio)(total_stop - total_start);
    // TOTAL
    PEAT_Ratio add_u = (all_ranks_user_len == 0);
    PEAT_Ratio add_l = (all_ranks_mpi_len + all_ranks_user_len == 0);

    printf("%d", line_prefix_total);
    printf(":%*s", ratio_len + 2 * clock_len_min + 2, bottom_line.c_str());
    printf(":%*.3f", 2 * clock_len_max + 1, model_total_length / local_freq);
    printf(":%*.2f", 5 * clock_len_min + 5 + ratio_len, 100 * PEAT_Ratio(all_ranks_mpi_len) / (all_ranks_mpi_len + all_ranks_user_len + add_l)); // MPI % of the application
    printf(":%*lld", clock_len_max, all_ranks_user_len_covered);
    printf(":%*.1f", ratio_len, 100 * (PEAT_Ratio)(all_ranks_user_len_covered) / (PEAT_Ratio)(all_ranks_user_len + add_u));
    printf(":%*lld", clock_len_max, all_ranks_user_len - all_ranks_user_len_covered);
    printf(":%*.1f", ratio_len, 100 * (PEAT_Ratio)(all_ranks_user_len - all_ranks_user_len_covered) / (PEAT_Ratio)(all_ranks_user_len + add_u));
    printf("\n");
    // Parallel info
    printf("%d", line_prefix_total);
    printf(":%*s", ratio_len + 2 * clock_len_min + 2, bottom_par_line.c_str());
    printf(":%*.3f", 2 * clock_len_max + 1, (PEAT_Ratio)(total_fin_start - total_init_stop) / (PEAT_Ratio)local_freq); // parallel time
    printf("\n");
    // MPI_Init info
    printf("%d", line_prefix_total);
    printf(":%*s", ratio_len + 2 * clock_len_min + 2, "MPI_Init");
    printf(":%*.3f", clock_len_max, (PEAT_Ratio)total_init_start / (PEAT_Ratio)local_freq);
    printf(":%*.3f", clock_len_max, (PEAT_Ratio)total_init_stop / (PEAT_Ratio)local_freq);
    printf(":%*.3f", 5 * clock_len_min + 4, (PEAT_Ratio)all_ranks_init_len / (PEAT_Ratio)local_freq);
    printf(":%*.2f", ratio_len, 100 * PEAT_Ratio(all_ranks_init_len) / (all_ranks_mpi_len + all_ranks_user_len + add_l));
    // printf(":%*lld", ratio_len + 2 * clock_len_max + 2, all_ranks_init_len);
    // printf(":%*.2f", ratio_len, 100 * PEAT_Ratio(all_ranks_init_len) / (all_ranks_mpi_len +  all_ranks_user_len));
    printf("\n");
    // MPI_Finalize info
    printf("%d", line_prefix_total);
    printf(":%*s", ratio_len + 2 * clock_len_min + 2, "MPI_Finalize");
    printf(":%*.3f", clock_len_max, (PEAT_Ratio)total_fin_start / (PEAT_Ratio)local_freq);
    printf(":%*.3f", clock_len_max, (PEAT_Ratio)total_fin_stop / (PEAT_Ratio)local_freq);
    printf(":%*.3f", 5 * clock_len_min + 4, (PEAT_Ratio)all_ranks_fin_len / (PEAT_Ratio)local_freq);
    printf(":%*.2f", ratio_len, 100 * PEAT_Ratio(all_ranks_fin_len) / (PEAT_Ratio)(all_ranks_mpi_len + all_ranks_user_len + add_l));
    // printf(":%*lld", ratio_len + 2 * clock_len_max + 2, all_ranks_fin_len);
    // printf(":%*.2f", ratio_len, 100 * PEAT_Ratio(all_ranks_fin_len) / (all_ranks_mpi_len +  all_ranks_user_len));
    printf("\n");
    SEPARATOR(separator_length);
    if (type == Event_Set_Stat::tst_post) {
        PEAT_Ratio rank_non_covered_percent = worst_covered_rank_non_covered ? 100.0 * PEAT_Ratio(worst_covered_rank_non_covered) / PEAT_Ratio(worst_covered_rank_non_covered + worst_covered_rank_covered) : 0;
        printf("Worst covered rank: %d (%.2f%%)\n", worst_covered_rank, rank_non_covered_percent);
        printf("Minimum fabric time: %lld (%.2f%%) in rank %d\n", min_fabric_len, 100 * PEAT_Ratio(min_fabric_len) / PEAT_Ratio(min_fabric_rank_len), min_fabric_len_rank);
    }
    return model_total_length / local_freq;
}

void PET_Core::print_parallel_region_report(const Event_Set_Stat::t_stat_type type, const PEAT_Record_Type line_prefix)
{
    assert(ptp);

    std::string model_type("<unknown>");
    switch (type) {
    case Event_Set_Stat::tst_orig:
        model_type = "Original";
        break;
    case Event_Set_Stat::tst_trc:
        model_type = "Tractor";
        break;
    case Event_Set_Stat::tst_post:
        model_type = "Estimated";
        break;
    }

    PEAT_Clock local_freq = 0; // get_trace_freq();//TODO add option to set it in 1 to show TSC instead second
    if (Event_Set_Stat::tst_post == type) {
        local_freq = get_target_freq();
    } else if (Event_Set_Stat::tst_orig == type || Event_Set_Stat::tst_trc == type) {
        local_freq = get_trace_freq();
    } else {
        assert(!"unknow Event_Set_Stat in PET_Core::print_report");
    }
    std::ostringstream head_line_str;
    head_line_str << model_type << " model MPI_Pcontrol sections(sec).";
    if (globals.pcontrol_mode_rank >= 0) {
        head_line_str << " Rank:" << globals.pcontrol_mode_rank;
    } else {
        head_line_str << " Rank:all";
    }
    head_line_str << " CPU:" << local_freq << "Hz. PPN:";
    if (Event_Set_Stat::tst_post == type && globals.target_ppn > 0) {
        head_line_str << globals.target_ppn;
        const PEAT_Rank total_rank_count = (PEAT_Rank)ptp->get_rank_num();
        if (globals.target_ppn < total_rank_count) {
            const PEAT_Rank fully_loaded_nodes = total_rank_count / globals.target_ppn;
            const PEAT_Rank remainder = total_rank_count - (fully_loaded_nodes * globals.target_ppn);
            if (remainder > 0) {
                head_line_str << "," << remainder;
            }
        }
    } else {
        PEAT_Rank previous = -1;
        std::string delimeter_str;
        for (std::map<PEAT_Rank, PEAT_Num>::const_iterator it = ptp->nodes_PPN.begin(); it != ptp->nodes_PPN.end(); ++it) {
            if (it->second != previous) {
                head_line_str << delimeter_str << it->second;
                delimeter_str = ",";
            }
            previous = (PEAT_Rank)it->second;
        }
    }
    std::string head_line = head_line_str.str();
    // string bottom_line = string(model_type) + " total";
    PEAT_Rank rank_count = 0;
    const int ratio_len = 4;
    const int clock_len_min = 14;
    const int separator_length = 2 * ratio_len + 1 + 6 * clock_len_min + 7;
    SEPARATOR(separator_length);
    printf("%2d:%-*s\n", PEAT_RT_Remark, separator_length, head_line.c_str());
    SEPARATOR(separator_length);
    printf("%2d", PEAT_RT_Remark);
    printf(":%*s%*s", PRINT_PARAMETERS(ratio_len, "Rank"));
    printf(":%*s%*s", PRINT_PARAMETERS(ratio_len, "Id"));
    printf(":%*s%*s", PRINT_PARAMETERS(2 * (clock_len_min + 1), "Start Clk,Sec"));
    printf(":%*s%*s", PRINT_PARAMETERS(2 * (clock_len_min + 1), "Stop Clk,Sec"));
    printf(":%*s%*s", PRINT_PARAMETERS(2 * (clock_len_min + 1), "Duration Clk,Sec"));
    printf("\n");
    SEPARATOR(separator_length);
    for (std::vector<PET_Node>::iterator node_it = nodes.begin(); node_it != nodes.end(); ++node_it) {
        // PEAT_Clock clk;
        // PET_Event *evt;
        // HANDLE_EVENT_CODE handle_err = EVENT_OK;
        for (std::map<PEAT_Rank, PET_Rank>::iterator rank_it = node_it->ranks.begin(); rank_it != node_it->ranks.end(); ++rank_it) {
            for (std::map<PEAT_Signed_Num, std::list<PET_Region>>::iterator region_list_it = rank_it->second.parallel_regions.begin(); region_list_it != rank_it->second.parallel_regions.end(); ++region_list_it) {
                PEAT_Clock duration_total = 0;
                for (std::list<PET_Region>::iterator region_it = region_list_it->second.begin(); region_it != region_list_it->second.end(); ++region_it) {
                    const PET_Region &elem = *region_it;
                    printf("%d", line_prefix);
                    printf(":%*d", ratio_len, rank_it->first);
                    printf(":%*d", ratio_len, region_list_it->first);
                    PEAT_Clock start = 0, stop = 0;
                    switch (type) {
                    case Event_Set_Stat::tst_orig:
                        stop = elem.o_exit.in;
                        start = elem.o_entry.out;
                        break;
                    case Event_Set_Stat::tst_trc:
                        stop = elem.t_exit.in;
                        start = elem.t_entry.out;
                        break;
                    case Event_Set_Stat::tst_post:
                        stop = elem.e_exit.in;
                        start = elem.e_entry.out;
                        break;
                    }
                    PEAT_Clock duration = stop - start;
                    PEAT_Float start_sec = (PEAT_Float)(start) / (PEAT_Float)local_freq;
                    PEAT_Float stop_sec = (PEAT_Float)(stop) / (PEAT_Float)local_freq;
                    PEAT_Float duration_sec = (PEAT_Float)(duration) / (PEAT_Float)local_freq;
                    duration_total += duration;
                    printf(":%*lld", clock_len_min, start);
                    printf(",%*f", clock_len_min, start_sec);
                    printf(":%*lld", clock_len_min, stop);
                    printf(",%*f", clock_len_min, stop_sec);
                    printf(":%*lld", clock_len_min, duration);
                    printf(",%*f", clock_len_min, duration_sec);
                    printf("\n");
                }
                printf("%d", line_prefix);
                printf(":%*d", ratio_len, rank_it->first);
                printf(":%*d", ratio_len, region_list_it->first);
                PEAT_Float duration_total_sec = (PEAT_Float)(duration_total) / (PEAT_Float)local_freq;
                printf(":%*s", clock_len_min, "-");
                printf(",%*s", clock_len_min, "-");
                printf(":%*s", clock_len_min, "-");
                printf(",%*s", clock_len_min, "-");
                printf(":%*lld", clock_len_min, duration_total);
                printf(",%*f", clock_len_min, duration_total_sec);
                printf("\n");
            }
        }
    }
    SEPARATOR(separator_length);
}

void PET_Core::write_event_profile(const std::string &name, const Event_Set_Stat::t_stat_type type)
{
    std::map<PEAT_Rank, std::string> file_name;
    file_name[0] = name;
    File_Storage_GZ profile_file(file_name, PEAT_FIO_FLAG_WRITE, NULL, globals.verbose), *profile_file_ptr = &profile_file;
    if (!profile_file.is_file_good(0)) {
        std::ostringstream tmp;
        tmp << "Can't open file (to write) " << name;
        throw std::runtime_error(tmp.str());
    }
    PEAT_Clock all_ranks_mpi_len = 0, all_ranks_user_len = 0;
    PEAT_Clock max_ranks_mpi_len = 0, max_ranks_user_len = 0;
    const std::map<PEAT_Rank, Stat> &stat_local = stat.get_stat(type);
    for (std::map<PEAT_Rank, Stat>::const_iterator stat_it = stat_local.begin(); stat_it != stat_local.end(); ++stat_it) {
        const Stat &st = stat_it->second;
        PEAT_Clock cur_ranks_mpi_len = st.fabric_len + st.cpu_len, cur_ranks_user_len = st.covered_UR_length + st.uncovered_UR_length;
        all_ranks_mpi_len += cur_ranks_mpi_len;
        all_ranks_user_len += cur_ranks_user_len;
        if (max_ranks_mpi_len + max_ranks_user_len < cur_ranks_mpi_len + cur_ranks_user_len) {
            max_ranks_mpi_len = cur_ranks_mpi_len;
            max_ranks_user_len = cur_ranks_user_len;
        }
    }
    const PEAT_Clock all_ranks_app_len = __max__(all_ranks_mpi_len + all_ranks_user_len, 1);
    const PEAT_Clock max_ranks_app_len = __max__(max_ranks_mpi_len + max_ranks_user_len, 1);
    const int name_len = 30;
    const int len = 16;
    const int len1 = 6;

    // const int separator_length = name_len + 1 + 8 * len + 8 + 3 * len1 + 3;
    const int separator_length = name_len + 1 + 9 * len + 8 + 4 * len1 + 4;
    SEPARATORGZ(profile_file_ptr, 0, separator_length);
    profile_file.printf(0, "%d", PEAT_RT_Remark);
    profile_file.printf(0, ":%*s%*s", PRINT_PARAMETERS(name_len, "Function ID"));
    profile_file.printf(0, ":%*s%*s", PRINT_PARAMETERS(len, "Size"));
    profile_file.printf(0, ":%*s%*s", PRINT_PARAMETERS(len, "Count"));
    profile_file.printf(0, ":%*s%*s", PRINT_PARAMETERS(2 * len + 1, "Volume (Send/Recv)"));
    profile_file.printf(0, ":%*s%*s", PRINT_PARAMETERS(len, "CPU"));
    profile_file.printf(0, ":%*s%*s", PRINT_PARAMETERS(len1, "%"));
    profile_file.printf(0, ":%*s%*s", PRINT_PARAMETERS(len, "Fabric"));
    profile_file.printf(0, ":%*s%*s", PRINT_PARAMETERS(len1, "%"));
    profile_file.printf(0, ":%*s%*s", PRINT_PARAMETERS(len, "Wait"));
    profile_file.printf(0, ":%*s%*s", PRINT_PARAMETERS(len1, "%"));
    profile_file.printf(0, ":%*s%*s", PRINT_PARAMETERS(len, "Overlap"));
    profile_file.printf(0, ":%*s%*s", PRINT_PARAMETERS(len1, "%"));
    profile_file.printf(0, "\n");

    SEPARATORGZ(profile_file_ptr, 0, separator_length);
    PEAT_Message_Size total_send_length_sum = 0,
                      total_recv_length_sum = 0;
    PEAT_Num num_events_sum = 0;

    for (std::map<PEAT_Rank, std::map<PEAT_Func_ID_Pair, std::map<PEAT_Message_Size, MPI_Event_Stat>>>::iterator MPI_Event_Stat_it = stat.get_func_stat(type).begin(); MPI_Event_Stat_it != stat.get_func_stat(type).end(); ++MPI_Event_Stat_it) {
        PEAT_Clock cpu_sum = 0,
                   fabric_sum = 0,
                   wait_sum = 0,
                   overlap_sum = 0;
        std::map<PEAT_Func_ID_Pair, std::map<PEAT_Message_Size, MPI_Event_Stat>> &mpi_event_stat = MPI_Event_Stat_it->second;
        profile_file.printf(0, "%d:Rank %d\n", PEAT_RT_Remark, MPI_Event_Stat_it->first);
        SEPARATORGZ(profile_file_ptr, 0, separator_length);
        for (std::map<PEAT_Func_ID_Pair, std::map<PEAT_Message_Size, MPI_Event_Stat>>::const_iterator func_id_it = mpi_event_stat.begin(); func_id_it != mpi_event_stat.end(); ++func_id_it) {
            std::ostringstream func_name;
            func_name << global_func_id.name_by_idx(func_id_it->first.id) << "(" << func_id_it->first.id << ")" << global_func_id.name_by_idx(func_id_it->first.parent_id) << "(" << func_id_it->first.parent_id << ")";
            for (std::map<PEAT_Message_Size, MPI_Event_Stat>::const_iterator msg_it = func_id_it->second.begin(); msg_it != func_id_it->second.end(); ++msg_it) {
                const MPI_Event_Stat &event_stat = msg_it->second;
                profile_file.printf(0, "%d", PEAT_RT_Event_Stat);
                profile_file.printf(0, ":%-*s", name_len, func_name.str().c_str());
                profile_file.printf(0, ":%*lld", len, msg_it->first);
                profile_file.printf(0, ":%*lld", len, event_stat.num_events);
                num_events_sum += event_stat.num_events;
                profile_file.printf(0, ":%*lld/%*lld", len, event_stat.total_send_length, len, event_stat.total_recv_length);
                total_send_length_sum += event_stat.total_send_length;
                total_recv_length_sum += event_stat.total_recv_length;
                profile_file.printf(0, ":%*lld", len, event_stat.cpu());
                cpu_sum += event_stat.prolog + event_stat.epilog;
                profile_file.printf(0, ":%*.3f", len1, 100 * PEAT_Ratio(event_stat.cpu()) / max_ranks_app_len);
                profile_file.printf(0, ":%*lld", len, event_stat.fabric);
                fabric_sum += event_stat.fabric;
                profile_file.printf(0, ":%*.3f", len1, 100 * PEAT_Ratio(event_stat.fabric) / max_ranks_app_len);
                profile_file.printf(0, ":%*lld", len, event_stat.wait);
                wait_sum += event_stat.wait;
                profile_file.printf(0, ":%*.3f", len1, 100 * PEAT_Ratio(event_stat.wait) / max_ranks_app_len);
                profile_file.printf(0, ":%*lld", len, event_stat.overlap);
                overlap_sum += event_stat.overlap;
                profile_file.printf(0, ":%*.3f", len1, 100 * PEAT_Ratio(event_stat.overlap) / max_ranks_app_len);
                profile_file.printf(0, "\n");
            }
        }
        SEPARATORGZ(profile_file_ptr, 0, separator_length);
        profile_file.printf(0, "%d", PEAT_RT_Event_Stat_Total);
        profile_file.printf(0, ":%*lld", name_len + 2 * len + 2, num_events_sum);
        profile_file.printf(0, ":%*lld/%*lld", len, total_send_length_sum, len, total_recv_length_sum);
        profile_file.printf(0, ":%*lld", len, cpu_sum);
        profile_file.printf(0, ":%*.2f", len1, 100 * PEAT_Ratio(cpu_sum) / max_ranks_app_len);
        // profile_file.printf(0, ":%*lld", 4 * len + 3, active_sum);
        profile_file.printf(0, ":%*lld", len, fabric_sum);
        profile_file.printf(0, ":%*.2f", len1, 100 * PEAT_Ratio(fabric_sum) / max_ranks_app_len);
        profile_file.printf(0, ":%*lld", len, wait_sum);
        profile_file.printf(0, ":%*.2f", len1, 100 * PEAT_Ratio(wait_sum) / max_ranks_app_len);
        profile_file.printf(0, ":%*lld", len, overlap_sum);
        profile_file.printf(0, ":%*.2f", len1, 100 * PEAT_Ratio(overlap_sum) / max_ranks_app_len);
        profile_file.printf(0, "\n");
        SEPARATORGZ(profile_file_ptr, 0, separator_length);
    }
}
#if 0
void PET_Core::write_trace_info(const std::string& name)
{
	std::map<PEAT_Rank, std::string> file_name;
	file_name[0] = name;
	File_Storage_GZ traceinfo_file(file_name, PEAT_FIO_FLAG_WRITE , globals.verbose);
	if (!traceinfo_file.is_file_good(0)) {
		std::ostringstream tmp;
		tmp << "Can't open file (to write) " << name;
		throw std::runtime_error(tmp.str());
	}
	gzFile trace_info_file_ptr = traceinfo_file.get_file_ptr(0);
	gzprintf( trace_info_file_ptr, "PEATTRACEINFO\n" );
	gzprintf( trace_info_file_ptr, "%d|%s|%s|%s|%s|%s|%s|%s|%s\n", PEAT_RT_Paths, globals.input_path.c_str(),
		globals.trace_files.c_str(),
		globals.mpi_files.c_str(), globals.hwc_files.c_str(), globals.bind_files.c_str(), globals.fab_files.c_str(), globals.com_ext.c_str(), globals.app_ext.c_str() );
	gzprintf( trace_info_file_ptr, "%d|%lld\n", PEAT_RT_CPU_Freq, get_target_freq());
	gzprintf( trace_info_file_ptr, "%d|%d|%d|%d\n", PEAT_RT_Var_Block_Info, model_id.num_tscs, model_id.num_hwcs, model_id.num_addrs );
	if ( collectives_bound >0) traceinfo_file.printf(0, "%d|%lld\n", PEAT_RT_Bind_Collective, collectives_bound);
	if ( !messages_bound.empty() ){
		gzprintf( trace_info_file_ptr, "%d", PEAT_RT_Bind_Peer );
		for ( std::vector<PEAT_LNum>::const_iterator messages_it = messages_bound.begin(); messages_it != messages_bound.end(); ++messages_it ){
			gzprintf( trace_info_file_ptr, "|%lld", *messages_it );
		}
		gzprintf( trace_info_file_ptr, "\n" );
	}
	if ( !user_region_file->last_event_idx.empty() ){
		gzprintf( trace_info_file_ptr, "%d", PEAT_RT_Number_Of_Events );
		for ( std::vector<PEAT_UID>::const_iterator events_it = events_processed.begin(); events_it != events_processed.end(); ++events_it ){
			gzprintf( trace_info_file_ptr, "|%lld", *events_it );
		}
		gzprintf( trace_info_file_ptr, "\n" );
	}
}
#else
void PET_Core::write_trace_info(const std::string &name)
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
                             globals.mpi_files.c_str(), globals.hwc_files.c_str(), globals.bind_files.c_str(), globals.fab_files.c_str(), globals.com_ext.c_str(), globals.app_ext.c_str());
        traceinfo_file.printf(0, "%d|%lld\n", PEAT_RT_CPU_Freq, ptp->get_trace_freq(0));
        traceinfo_file.printf(0, "%d|%d|%d|%d\n", PEAT_RT_Var_Block_Info, model_id.num_tscs, model_id.num_hwcs, model_id.num_addrs);
        if (collectives_bound > 0)
            traceinfo_file.printf(0, "%d|%lld\n", PEAT_RT_Bind_Collective, collectives_bound);
        if (!messages_bound.empty()) {
            traceinfo_file.printf(0, "%d", PEAT_RT_Bind_Peer);
            for (std::vector<PEAT_LNum>::const_iterator messages_it = messages_bound.begin(); messages_it != messages_bound.end(); ++messages_it) {
                traceinfo_file.printf(0, "|%lld", *messages_it);
            }
            traceinfo_file.printf(0, "\n");
        }
        if (!user_region_file->last_event_idx.empty()) {
            traceinfo_file.printf(0, "%d", PEAT_RT_Number_Of_Events);
            for (std::vector<PEAT_UID>::const_iterator events_it = events_processed.begin(); events_it != events_processed.end(); ++events_it) {
                traceinfo_file.printf(0, "|%lld", *events_it);
            }
            traceinfo_file.printf(0, "\n");
        }
        if (!com_store->mpi_time.empty()) {
            traceinfo_file.printf(0, "%d", PEAT_RT_MPI_Time);
            for (std::map<PEAT_Rank, PEAT_MPI_Time>::const_iterator time_it = com_store->mpi_time.begin(); time_it != com_store->mpi_time.end(); ++time_it) {
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

#endif
PEAT_Ratio PET_Core::analyze(const std::string &output_file)
{
    assert(0 == globals.comm_rank);
    PEAT_Ratio original = print_report(Event_Set_Stat::tst_orig, PEAT_RT_PET_Final_Stat_Orig);
    PEAT_Ratio tractor = print_report(Event_Set_Stat::tst_trc, PEAT_RT_PET_Final_Stat_Tractor);
    PEAT_Ratio estimated;
    if (!get_mo()) {
        estimated = print_report(Event_Set_Stat::tst_post, PEAT_RT_PET_Final_Stat_Estim);
    } else {
        estimated = tractor;
    }
    print_parallel_region_report(Event_Set_Stat::tst_orig, PEAT_RT_PET_MPI_PControl_Stat_Orig);
    print_parallel_region_report(Event_Set_Stat::tst_trc, PEAT_RT_PET_MPI_PControl_Stat_Tractor);
    if (!get_mo()) {
        print_parallel_region_report(Event_Set_Stat::tst_post, PEAT_RT_PET_MPI_PControl_Stat_Estim);
    }
    // printf( "Approximation quality, average weighted u_st_dev: %f\n", user_region_file->get_overall_weighted() );
    write_event_profile(globals.input_path + DIR_DELIMITER + "0.event_stat.orig.txt", Event_Set_Stat::tst_orig);
    write_event_profile(globals.input_path + DIR_DELIMITER + "0.event_stat.trc.txt", Event_Set_Stat::tst_trc);
    if (!get_mo()) {
        write_event_profile(globals.input_path + DIR_DELIMITER + "0.event_stat.post.txt", Event_Set_Stat::tst_post);
        write_transport_profile("0.transport_stat.trc.txt", Event_Set_Stat::tst_trc);
        write_function_split_profile("0.function_split_stat.trc.txt");
    }
    return tractor / estimated;
}

std::ostream &operator<<(std::ostream &dbg, const PET_Core &me)
{
    long num_messages = 0;
    return dbg;
}

std::ostream &operator<<(std::ostream &dbg, const Coll_Storage &rec)
{
    dbg << "Ranks expected: " << rec.expected_ranks << "\n"
        << "events:{\n"
        << rec.events << "\n}\n";
    return dbg;
}
