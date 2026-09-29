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
#include "utils.h"

#ifdef PEAT_MPI
#include <mpi.h>
#endif

/* A colt record issued by the progress engine while another MPI call was spinning belongs (in the trace) to that
   other call, so the peer of a colt transfer may sit in any call of the peer rank: scan them all (nested included). */
PET_Event *PET_Core::find_colt_peer_anywhere(PET_Event *event, PEAT_Rank peer, PEAT_Bool want_receiver)
{
    if (peer < 0)
        return NULL;
    PEAT_Node node = ptp->get_tgt_node(peer);
    if (node < 0 || node >= (PEAT_Node)nodes.size() || nodes[node].ranks.count(peer) == 0)
        return NULL;
    PET_Rank &pet_rank = nodes[node].ranks.at(peer);
    for (std::vector<PET_Thread *>::iterator thread_it = pet_rank.threads.begin(); thread_it != pet_rank.threads.end(); ++thread_it) {
        for (std::list<PET_Event *>::iterator event_it = (*thread_it)->active_events.begin(); event_it != (*thread_it)->active_events.end(); ++event_it) {
            PET_Event *main_event = *event_it;
            if (main_event->is_written)
                continue;
            for (PET_Event *cand = main_event->get_first_child_event(); cand != main_event; cand = main_event->get_next_child_event()) {
                if (want_receiver) {
                    if (is_wild_card(cand))
                        continue;
                    if (check_peer(event, cand) && cand->ready_to_bind())
                        return cand;
                } else {
                    if (check_peer(cand, event) && cand->ready_to_bind())
                        return cand;
                }
            }
        }
    }
    return NULL;
}

/* The progress engine, spun by a blocking call, executes the schedules of outstanding nonblocking collectives:
   their transfers show up as colt records of the spinning call. Match them by communicator and tag to the
   collective that owns them (its own records carry the tag); internal completions follow the records they complete. */
void PET_Core::attribute_progress_records(PET_Event *host)
{
    PET_Rank *pet_rank = host->get_rank();
    if (pet_rank == NULL)
        return;
    const std::vector<PET_Event *> &kids = host->child_iteration_list();
    for (PEAT_Num i = 0; i < kids.size(); i++) {
        PET_Event *evt = kids[i];
        if (evt == NULL || evt->is_foreign_progress())
            continue;
        PET_Collective_Event *owner = NULL;
        PET_Event_Type type = evt->get_event_type();
        if (type == PET_EVENT_TYPE_TOR || type == PET_EVENT_TYPE_TAR) {
            PEAT_MPI_Tag tag = evt->check_tx() ? evt->get_send_tag() : evt->get_recv_tag();
            owner = pet_rank->find_nb_collective(evt->get_comm(), tag, host);
        } else if (type == PET_EVENT_TYPE_COMPLETION) {
            // an internal wait/release belongs to whoever owns the records it completes
            PET_Completion_Event *cevt = static_cast<PET_Completion_Event *>(evt);
            for (std::list<PET_Request>::iterator req_it = cevt->triggered_requests.begin(); req_it != cevt->triggered_requests.end(); ++req_it) {
                PET_Event *target = req_it->event;
                if (target == NULL)
                    continue;
                PET_Collective_Event *o = target->progress_owner;
                if (o == NULL && target->host_event && target->host_event != host && target->host_event->get_event_type() == PET_EVENT_TYPE_COLLECTIVE) {
                    PET_Collective_Event *h = static_cast<PET_Collective_Event *>(target->host_event);
                    if (h->is_nonblocking_collective())
                        o = h; // completes an own record of that collective (e.g. release of its irecv)
                }
                if (o == NULL || (owner && owner != o)) {
                    owner = NULL;
                    break;
                }
                owner = o;
            }
        }
        if (owner) {
            owner->add_progress_event(evt);
            if (globals.verbose) {
                fprintf(stderr, "Rank %d %s(%lld): %s(%s) progressed %s(%lld) tag %d\n", host->rank, global_func_id.name_by_idx(host->func_id), (long long)host->ids[0],
                        global_func_id.name_by_idx(evt->func_id), evt->ids_str().c_str(), global_func_id.name_by_idx(owner->func_id), (long long)owner->ids[0],
                        (int)(evt->check_tx() ? evt->get_send_tag() : evt->get_recv_tag()));
            }
        }
    }
}

PET_Event *PET_Core::find_peer(PET_Event *event, PET_Event *host_evt, PEAT_Bool recv_part)
{
    PEAT_Bool r;
    std::list<Coll_Storage>::const_iterator collective;
    const PET_Coll_Event *coll_host = dynamic_cast<const PET_Coll_Event *>(host_evt); // only collective calls have peers to match against
    if (coll_host && get_coll_const(coll_host, collective)) {
        if (global_func_id.check_property(event->func_id, PEAT_PROP_DATA_TX)) {
        send_part:
            PEAT_Rank dest = event->get_dest();
            if (dest < 0)
                return NULL;
            if (collective->events.count(dest) < 1) return NULL; // receiver is not yet partsed
            PET_Event *main_recv_event = collective->events.at(dest);
            PET_Event *recv_event = main_recv_event->get_first_child_event();
            while (recv_event != main_recv_event) {
                if (is_wild_card(recv_event)) {
                    return NULL; // we cannot resolve until we know for sure (until completion is received)
                }
                if (check_peer(event, recv_event) && recv_event->ready_to_bind()) {
                    return recv_event;
                }
                recv_event = main_recv_event->get_next_child_event();
            }
            return find_colt_peer_anywhere(event, dest, true); // the receive may have been issued from progress inside another call
        }
        if (global_func_id.check_property(event->func_id, PEAT_PROP_DATA_RX)) {
        recv_part:
            // if (chain_has_unpaired_event(event, false)) return NULL;
            if (is_wild_card(event))
                return NULL; // we cannot resolve until we know for sure (until completion is received)

            PEAT_Rank src = event->get_src();
            if (collective->events.count(src) < 1)
                return NULL; // sender is not yet partsed
            PET_Event *main_send_event = collective->events.at(src);
            PET_Event *send_event = main_send_event->get_first_child_event();
            while (send_event != main_send_event) {
                if (check_peer(send_event, event) && send_event->ready_to_bind()) {
                    return send_event;
                }
                send_event = main_send_event->get_next_child_event();
            }
            return find_colt_peer_anywhere(event, src, false); // the send may have been issued from progress inside another call
        }
        if (global_func_id.check_property(event->func_id, PEAT_PROP_DATA_TXRX)) {
            if (recv_part)
                goto recv_part;
            goto send_part;
        }
        return NULL;
    } else {
        pet_throw_error(this,event,NULL,"Host event is not found in the list of collectives.\n");
    }
    return NULL;
}

PET_Event *PET_Core::find_peer(PET_Event *event, PEAT_Bool recv_part)
{
    std::list<PET_Event *>::iterator event_it;
    PEAT_Bool r;
    if (event->get_host_event()) {
        return find_peer(event, event->get_host_event(), recv_part);
    }
    if (global_func_id.check_property(event->func_id, PEAT_PROP_DATA_TX)) {
    send_part:
        PEAT_Rank dest = event->get_dest();
        if (dest < 0)
            return NULL;
        PEAT_Node node = ptp->get_tgt_node(dest);
        // if (chain_has_unpaired_event(event, true)) return NULL;
        r = nodes[node].ranks[dest].get_head_event(event_it);
        while (r) {
            PET_Event *main_recv_event = *event_it;
            if (is_wild_card(main_recv_event)) {
                return NULL; // we cannot resolve until we know for sure (until completion is received)
            }
            if (check_peer(event, main_recv_event) && main_recv_event->ready_to_bind()) {
                return main_recv_event;
            }
            PET_Event *recv_event = main_recv_event->get_first_child_event();
            while (recv_event != main_recv_event) {
                if (is_wild_card(recv_event)) {
                    return NULL; // we cannot resolve until we know for sure (until completion is received)
                }
                if (check_peer(event, recv_event) && recv_event->ready_to_bind()) {
                    return recv_event;
                }
                recv_event = main_recv_event->get_next_child_event();
            }
            r = nodes[node].ranks[dest].get_next_event(event_it);
        }
        return NULL;
    }
    if (global_func_id.check_property(event->func_id, PEAT_PROP_DATA_RX)) {
    recv_part:
        // if (chain_has_unpaired_event(event, false)) return NULL;
        PEAT_Rank src = event->get_src();
        PEAT_Node node = ptp->get_tgt_node(src);
        if (is_wild_card(event))
            return NULL; // we cannot resolve until we know for sure (until completion is received)
        r = nodes[node].ranks[src].get_head_event(event_it);
        while (r) {
            PET_Event *main_send_event = *event_it;
            if (is_wild_card(main_send_event)) {
                return NULL; // we cannot resolve until we know for sure (until completion is received)
            }
            if (check_peer(main_send_event, event) && main_send_event->ready_to_bind()) {
                return main_send_event;
            }
            PET_Event *send_event = main_send_event->get_first_child_event();
            while (send_event != main_send_event) {
                if (check_peer(send_event, event) && send_event->ready_to_bind()) {
                    return send_event;
                }
                send_event = main_send_event->get_next_child_event();
            }
            r = nodes[node].ranks[src].get_next_event(event_it);
        }
        return NULL;
    }
    if (global_func_id.check_property(event->func_id, PEAT_PROP_DATA_TXRX)) {
        if (recv_part)
            goto recv_part;
        goto send_part;
    }
    return NULL;
}

// fill_next_event
PET_Event *PET_Core::get_next_event(PET_Event *event)
{
    PET_Thread *pet_thread = event->get_thread();
    std::list<PET_Event *>::iterator event_it = pet_thread->find_event(event, true);
    if (event_it != pet_thread->active_events.end()) {
        ++event_it;
        if (event_it != pet_thread->active_events.end()) {
            return *event_it;
        }
    }
    return NULL;
}
PET_Event *PET_Core::get_prev_event(PET_Event *event, PEAT_Bool reverse)
{
    PET_Thread *pet_thread = event->get_thread();
    if (reverse) {
        std::list<PET_Event *>::reverse_iterator event_it;
        event_it = pet_thread->find_revent(event);
        //	if (event_it == pet_thread->active_events.end() || event_it == pet_thread->active_events.begin()) return NULL;// special case with collectives when they are not in the quque
        if (event_it == pet_thread->active_events.rend() || pet_thread->active_events.empty())
            return NULL; // special case with collectives when they are not in the quque
        ++event_it;
        return *event_it;
    } else {
        std::list<PET_Event *>::iterator event_it;
        event_it = pet_thread->find_event(event, false);
        //	if (event_it == pet_thread->active_events.end() || event_it == pet_thread->active_events.begin()) return NULL;// special case with collectives when they are not in the quque
        if (event_it == pet_thread->active_events.begin() || pet_thread->active_events.empty())
            return NULL; // special case with collectives when they are not in the quque
        --event_it;
        return *event_it;
    }
}
#if 0
PET_Event* PET_Core::get_prev_event(PET_Event* event, PEAT_Thread thread_id, PEAT_Bool reverse, PEAT_Bool use_cache)
{
	PET_Thread* pet_thread = event->get_thread();
	if (reverse) {
		std::list <PET_Event*>::reverse_iterator event_it;
		event_it = pet_thread->find_rpevent(event->e_time.func.in);
		if (event_it == pet_thread->active_events.rend() || pet_thread->active_events.empty()) {
			return NULL;// special case with collectives when they are not in the quque
		}
		//++event_it; // no need in this case 
		return *event_it;
	} else {
		std::list <PET_Event*>::iterator event_it;
		event_it = pet_thread->find_event(event->e_time.func.in, use_cache);
		if (event_it == pet_thread->active_events.end() || pet_thread->active_events.empty() || event_it == pet_thread->active_events.begin()) {
			return NULL;// special case with collectives when they are not in the quque
		}
		--event_it;
		return *event_it;
	}
}
#endif
PET_Event *PET_Core::get_last_event(PET_Event *event)
{
    PET_Thread *pet_thread = event->get_thread();
    std::list<PET_Event *>::reverse_iterator event_it = pet_thread->active_events.rbegin();
    if (event_it != pet_thread->active_events.rend()) {
        return *event_it;
    }
    return NULL;
}
PET_Event *PET_Core::get_last_event_long(PET_Event *event)
{
    PET_Thread *pet_thread = nodes.at(event->node).ranks.at(event->rank).threads.at(event->thread_id);
    std::list<PET_Event *>::reverse_iterator event_it = pet_thread->active_events.rbegin();
    if (event_it != pet_thread->active_events.rend()) {
        return *event_it;
    }
    return NULL;
}
PEAT_Bool PET_Core::remove_entry(PET_Event *event)
{
    PET_Thread *pet_thread = event->get_thread();
#ifdef PEAT_OMP
//	ins_rem_mutexes[event->rank].lock();
#endif

    std::list<PET_Event *>::iterator event_it = pet_thread->find_event(event, false);
    // update cache chain
    PET_Event *evt = *event_it;
    PET_Event *prev_evt = evt->prev_event;
    PET_Event *next_evt = evt->next_event;
    if (prev_evt) {
        if (next_evt) {
            prev_evt->next_event = (next_evt->thread_id == prev_evt->thread_id) ? evt->next_event : NULL;
        } else {
            prev_evt->next_event = NULL;
        }
    }
    if (next_evt) {
        if (prev_evt) {
            next_evt->prev_event = (next_evt->thread_id == prev_evt->thread_id) ? evt->prev_event : NULL;
        } else {
            next_evt->prev_event = NULL;
        }
    }
    pet_thread->active_events.erase(event_it);
#ifdef PEAT_OMP
// 	ins_rem_mutexes[event->rank].unlock();
#endif
    return false;
}
PEAT_Bool PET_Core::insert_entry(PET_Event *event, PEAT_Bool check_place)
{
    PET_Thread *pet_thread = event->get_thread();
    PET_Rank *pet_rank = event->get_rank();
#ifdef PEAT_OMP
//	ins_rem_mutexes[event->rank].lock();
#endif
    pet_thread->insert_ael_entry(event, check_place);
    pet_rank->insert_pre_entry(event);
#ifdef PEAT_OMP
//	ins_rem_mutexes[event->rank].unlock();
#endif
    return true;
}

HANDLE_EVENT_CODE PET_Core::handle_event(PET_Event *event)
{
    PEAT_MPI_Func_ID func_id = event->func_id;
    if (event->bind()) {
        if (event->ready_to_estimate()) {
            if (event->initiate()) {
                if (event->is_estimated()) {
                    // event->update_pending();
                    write_event_and_update_stat(event);
                    return EVENT_OK;
                }
                return EVENT_INITIATED;
            } else {
                return EVENT_NOT_ESTIMATED;
            }
        } else {
            return EVENT_NOT_READY;
        }
    } else {
        return EVENT_NOT_BOUND;
    };
}

void PET_Core::fill_next_event(PET_Event *event)
{
    PET_Rank *pet_rank = get_pet_rank(event);
    PET_Event *next_event = event->get_next_event();
    if (next_event) {
        HANDLE_EVENT_CODE handle_err = handle_event(event); // for write
        handle_err = handle_event(next_event);
        if (handle_err == EVENT_INITIATED) {
            ++pet_rank->threads[event->thread_id]->start_event_it;
            if (pet_rank->threads[event->thread_id]->start_event_it == pet_rank->threads[event->thread_id]->active_events.end()) {
                pet_throw_error(this, event, NULL, "Wrong Iterator. Something is broken inside pipeline.\n");
            } /*else {
                if ((*pet_rank->threads[event->thread_id]->event_being_estimated_it) != next_event) {
                    pet_throw_error(this, event, NULL, "Iterator Mismatch. Something is broken inside pipeline.\n");
                }
            }*/
            pet_rank->threads[event->thread_id]->event_being_estimated = next_event;
            return;
        }
        // assert(handle_err == EVENT_NOT_BOUND);
        if (handle_err != EVENT_NOT_BOUND && handle_err != EVENT_NOT_READY && handle_err != EVENT_OK) {
            pet_throw_error(this, event, NULL, "Wrong return code. Something is broken inside pipeline.\n");
        }
    } /*else {
        pet_rank->threads[event->thread_id]->start_event_it = pet_rank->threads[event->thread_id]->active_events.begin();
    }*/
    re_fill_queue = true;
}

PET_Event *PET_Core::get_event(const PEAT_Rank rank, String_Split &trace_line_MPI, String_Split &trace_line_HWC, PET_Event *host_evt, PET_Event *container)
{
    PEAT_Bool possible_colt_included = false;
    assert(com_store);
    assert(user_region_file);
    PET_Event *event = NULL;
    PEAT_Bool r_mpi = input_MPI->read_lines(trace_line_MPI, rank, ':');
    PEAT_Bool r_hwc = input_HWC->read_lines(trace_line_HWC, rank, ':');
    if (!(r_mpi && r_hwc))
        return NULL;
    if (trace_line_HWC.size() < 2) {
        if (!input_MPI->is_file_good(rank) || !input_HWC->is_file_good(rank)) {
            if (!ranks_read[rank]) {
                ranks_read[rank] = true;
            }
        }
        return NULL;
    }

    PEAT_Record_Header hdr; // ids, thread and nesting of the record
    user_region_file->parse_record_header(hdr, trace_line_MPI, rank);
    PEAT_MPI_Func_ID func_id = TMPI_FUNC_ID_NONE;
    extract(func_id, trace_line_MPI.at(hdr.payload), 10, false);
    if (func_id == 0) {
        func_id = global_func_id.idx_by_name(trace_line_MPI.at(hdr.payload));
    }

    if (func_id >= MPI_START_FILE_GROUP && func_id < MPI_END_FILE_GROUP) {
        event = new PET_Local_Event(this);
    } else if (func_id >= MPI_START_OSC_GROUP && func_id < MPI_END_OSC_GROUP) {
        event = new PET_Local_Event(this);
    } else if (func_id >= MPI_START_BUFFERED_LOCAL_GROUP && func_id < MPI_END_BUFFERED_LOCAL_GROUP) {
        event = new PET_Local_Buf_Event(this);
    } else if ((func_id >= MPI_START_COMM_COLLECTIVE_GROUP &&
                func_id < MPI_END_COMM_COLLECTIVE_GROUP) ||
               func_id == MPI_COMM_FREE_ID) {
        event = new PET_Comm_Event(this);
        possible_colt_included = true;
    } else if ((func_id >= MPI_P2P_TRANSFER_START && func_id < MPI_P2P_TRANSFER_END) ||
               (func_id >= MPI_P2P_PERSISTENT_INIT_START && func_id < MPI_P2P_PERSISTENT_INIT_END) ||
               (func_id >= MPI_PROBE_START && func_id < MPI_PROBE_END) ||
               (func_id >= MPI_P2P_COLL_TRANSFER_START && func_id < MPI_P2P_COLL_TRANSFER_END) ||
               (func_id >= MPI_P2P_COLL_PROBE_START && func_id < MPI_P2P_COLL_PROBE_END)) {
        if (func_id != MPI_SENDRECV_ID && func_id != MPI_SENDRECV_REPLACE_ID && func_id != MPIC_SENDRECV_ID && func_id != MPIC_SENDRECV_REPLACE_ID) {
            event = new PET_ToR_Event(this);
        } else {
            event = new PET_TaR_Event(this);
        }
    } else if ((func_id >= MPI_REQUEST_START && func_id < MPI_REQUEST_END) ||
               (func_id >= MPI_MISC_REQUEST_START && func_id < MPI_MISC_REQUEST_END) ||
               (func_id >= MPI_P2P_COLL_REQUEST_START && func_id < MPI_P2P_COLL_REQUEST_END)) {
        if (func_id == MPI_STARTALL_ID || func_id == MPI_START_ID) {
            event = new PET_Start_Event(this);
        } else {
            event = new PET_Completion_Event(this);
            possible_colt_included = true;
        }
    } else if (func_id >= MPI_START_GENERAL_COLLECTIVE_GROUP && func_id < MPI_END_GENERAL_COLLECTIVE_GROUP) {
        event = new PET_Collective_Event(this);
        possible_colt_included = true;
    } else if ((func_id >= CL_START_GROUP && func_id < CL_END_GROUP) || (func_id >= ZE_START_GROUP && func_id < ZE_END_GROUP)) {
        event = new PET_GPU_Event(this);
    } else if (func_id >= OMPT_START_GROUP && func_id < OMPT_END_GROUP) {
        event = new PET_OMPT_Event(this);
    } else {
        event = new PET_Local_Event(this);
    }
    if (!event->read_event(rank, trace_line_MPI, trace_line_HWC, host_evt, hdr)) {
        delete event;
        return NULL;
    }
    // event->node = ptp->get_tgt_node(rank);
    if (func_id == MPI_SENDRECV_ID || func_id == MPI_SENDRECV_REPLACE_ID || func_id == MPIC_SENDRECV_ID || func_id == MPIC_SENDRECV_REPLACE_ID) {
        input_MPI->read_lines(trace_line_MPI, rank, ':');
        input_HWC->read_lines(trace_line_HWC, rank, ':');
        event->read_event(rank, trace_line_MPI, trace_line_HWC, host_evt, hdr); // same header, tscs of the receive part
    }
    event->pet_rank->register_event_by_ids(event); // O(1) lookup by id path, any nesting depth: replaces the old child-walk search
    /*if (event->rank == 23 && event->ids[0] == 59) {
        event->rank = 23;
    }*/
    // event->assign_cache_pointers();
    // nested format: every record start is relative to the previous record at any level (the predictor follows every record);
    // legacy format: colt records are relative to their host, which keeps the predictor
    update_time_predictor(tsc_corrector[rank], event, hdr.nested ? NULL : host_evt);
    event->e_time.func.in = ptp->get_real_time(event->e_time.func.in, rank);
    event->e_time.mpi.in = ptp->get_real_time(event->e_time.mpi.in, rank);
    event->e_time.mpi.out = ptp->get_real_time(event->e_time.mpi.out, rank);
    event->e_time.func.out = ptp->get_real_time(event->e_time.func.out, rank);
    if (event->e_time.pe.in > 0)
        event->e_time.pe.in = ptp->get_real_time(event->e_time.pe.in, rank);
    if (event->e_time.pe.out > 0)
        event->e_time.pe.out = ptp->get_real_time(event->e_time.pe.out, rank);

    if (container) { // nested format: the record is a child of the record being read (container) inside the top-level call (host_evt)
        event->container_event = container;
        event->host_event = host_evt;
        container->nested_events.push_back(event);
        host_evt->flat_events.push_back(event);
    }
    if (hdr.nested) { // nested format: the num_children records following this one are its children (pre-order tree)
        PET_Event *top = host_evt ? host_evt : event;
        for (PEAT_Signed_Num i = 0; i < hdr.num_children; i++) {
            String_Split t_trace_line_MPI, t_trace_line_HWC;
            PET_Event *child_event = get_event(rank, t_trace_line_MPI, t_trace_line_HWC, top, event);
            if (child_event == NULL)
                break; // truncated trace
        }
        // some records were issued from progress for another, still outstanding nonblocking collective
        if (host_evt == NULL && event->has_nested())
            attribute_progress_records(event);
    } else if (possible_colt_included) { // legacy format: num_colts flat records follow the call
        PET_Coll_Event *cevent = static_cast<PET_Coll_Event *>(event);
        int size = (int)cevent->t_events.size();
        for (int i = 0; i < size; i++) {
            String_Split t_trace_line_MPI, t_trace_line_HWC;
            PET_Event *child_event = get_event(rank, t_trace_line_MPI, t_trace_line_HWC, cevent);
            if (child_event == NULL)
                break;
            cevent->put_t_event(child_event);
        }
        if (size > 0) {
            // colt records issued from the progress engine are nested in the record that was spinning it
            cevent->build_nesting();
            // ... and some of them were issued for another, still outstanding nonblocking collective
            attribute_progress_records(cevent);
        }
    }

    PET_Node *event_node = get_pet_node(event->node);
    PET_Rank *event_rank = get_pet_rank(event);
    event->o_time = event->e_time;
    if (host_evt == NULL) {
        PEAT_Clock prev_o_out, prev_e_out = 0;
        PET_Event *prev_event = get_last_event_long(event);       // previous event from the same thread
        PET_Event *prev_read_event = event_rank->last_read_event; // previous read event from the rank
        PEAT_Clock o_distance;
        PEAT_Clock e_distance;
        PEAT_Clock difference = 0;
        // event->move_event(event_rank->tractor_overhead_time);
        PEAT_Clock sub_t = event->remove_tractor_overhead();
        if (prev_event) {
            prev_e_out = prev_event->e_time.func.out;
            prev_o_out = prev_event->o_time.func.out;

            o_distance = event->o_time.func.in - prev_o_out;
            e_distance = event->e_time.func.in - prev_e_out;
            difference = e_distance - o_distance;
            event->move_event(difference);
        } else {
            if (prev_read_event) {
                prev_e_out = prev_read_event->e_time.func.out;
                prev_o_out = prev_read_event->o_time.func.out;

                o_distance = event->o_time.func.in - prev_o_out;
                e_distance = event->e_time.func.in - prev_e_out;
                difference = e_distance - o_distance;
                event->move_event(difference);
            }
        }
        if (event->thread_id == 0) {
            event_rank->tractor_overhead_time += sub_t;
            // if (event->func_id!= OMPT_PARALLEL_ID) move_openmp_regions(sub_t, event);
        } // add
        event_rank->threads[event->thread_id]->tractor_overhead_time += sub_t;
        if (prev_event) {
            event_rank->sub_time = difference + event->apply_user_code(get_user_code_ratio(prev_event, event), prev_event);
        }
        read_fabric_events(event);
        event->insert();
        // event->split_event();
        event->post_read_event();
        event->set_sub_t(event_rank->tractor_overhead_time, event_rank->sub_time);
        event_rank->last_read_event = event;
    }
    return event;
}
/*
void PET_Core::move_openmp_regions(PEAT_Clock sub_t, PET_Event* event) {
    PET_Rank* pet_rank = get_pet_rank(event);
    for (std::list<PET_Event*> ::reverse_iterator omp_section_it = pet_rank->omp_parallel_sections.rbegin(); omp_section_it != pet_rank->omp_parallel_sections.rend(); ++omp_section_it) {
        PET_Event* evt = *omp_section_it;
        if (evt->ut_time.func.out < event->ut_time.func.in) break; // this region is before the event -> (early) exit
        if (evt->ut_time.func.in > event->ut_time.func.out) { /// this region is after the event -> move whole region
            evt->move_event(sub_t);
            continue;
        }
        if (evt->ut_time.func.in > event->e_time.func.in) { /// this region begins before but finishes after the event-> shrink/expand region
            evt->adjust_event(sub_t);
            continue;
        }
    }
}
*/

void PET_Core::update_active_event_chain_t0(PEAT_Clock sub_t, PET_Event *event)
{
    // Thread 0 -  move all event till the end AND all parallel regions (with events inside) from other threads
    PET_Thread *thread = event->get_thread();
    std::list<PET_Event *>::iterator evt_it = ++(thread->find_event(event, true));         // get event next to current
    if (event->get_parallel_section_end() && event->get_parallel_section_end() != event) { // inside parallel section but not closing it
        // move all event in current ps first
        evt_it = thread->update_active_event_chain_ps(sub_t, evt_it);
    }
    while (evt_it != thread->active_events.end()) {
        evt_it = thread->update_active_event_chain_bps(sub_t, evt_it);
        if (evt_it != thread->active_events.end()) {
            PET_OMPT_Event *evt = static_cast<PET_OMPT_Event *>(*evt_it);
            for (std::vector<PET_OMPT_Event *>::iterator event_it = evt->sections.begin(); event_it != evt->sections.end(); ++event_it) {
                PET_Event *evt = *event_it;
                if (evt->thread_id == 0) {
                    evt_it = thread->update_active_event_chain_ps(sub_t, evt_it);
                } else {
                    PET_Thread *thread = evt->get_thread();
                    std::list<PET_Event *>::iterator evt_it = thread->find_event_cached(evt);
                    thread->update_active_event_chain_ps(sub_t, evt_it);
                }
            }
        }
    }
}
void PET_Core::update_active_event_chain_tn0_out(PEAT_Clock sub_t, PET_Event *event)
{
    // Thread > 0 -  move all event till the first parallel section
    PET_Thread *thread = event->get_thread();
    std::list<PET_Event *>::iterator evt_it = ++(thread->find_event(event, true));
    thread->update_active_event_chain_bps(sub_t, evt_it);
}
void PET_Core::update_active_event_chain_tn0_in(PEAT_Clock sub_t, PET_Event *event)
{
    // Thread > 0 -  move all event till the end of current parallel section
    PET_Thread *thread = event->get_thread();
    std::list<PET_Event *>::iterator evt_it = ++(thread->find_event(event, true));
    thread->update_active_event_chain_ps(sub_t, evt_it);
}

void PET_Core::update_active_event_chain(PEAT_Clock sub_t, PET_Event *event)
{
    if (event->sub_t_applied) {
        pet_warning(this, event, "Event already moved.\n");
        return;
    }
    event->sub_t_applied = true;
    if (event->parent_event || event->host_event)
        return; // child event...skip
#ifndef _DEBUG
    if (sub_t == 0)
        return;
#endif
    PET_Rank *pet_rank = event->get_rank();
    if (event->thread_id == 0) {
        update_active_event_chain_t0(sub_t, event);
        return;
    }
    if (event->get_parallel_section_end()) {              // inside parallel section
        if (event->get_parallel_section_end() != event) { // and not closing it. otherwise do nothing
            update_active_event_chain_tn0_in(sub_t, event);
        }
    } else {
        update_active_event_chain_tn0_out(sub_t, event);
    }
    return;
}
#if 1
PEAT_Bool PET_Core::read_fabric_events(PET_Event *event)
{
    PEAT_Node node = event->node;
    PEAT_Node rank = event->rank;
    String_Split trace_line_f;

    while (input_fab->read_lines(trace_line_f, rank, ':')) {
        if (trace_line_f.size() < 2)
            continue;
        PET_Fabric_Event evt;
        extract(evt.id, trace_line_f.at(0), 10);
        extract(evt.thread, trace_line_f.at(1), 10);
        extract(evt.func_id, trace_line_f.at(2), 10, false);
        if (evt.func_id == TMPI_FUNC_ID_NONE) {
            evt.func_id = global_func_id.idx_by_name(trace_line_f.at(2));
        }
        /*        event->tscs[0]=tsc_entry;
                event->tscs[1]=tsc_mpi_entry-tsc_entry; //pre tractor overhead
                event->psm2.num_calls=1;
                event->psm2.total_clocks_bitween_calls=0;
                tsc_exit = rdtsc();
                event->tscs[2]=tsc_exit - tsc_mpi_exit; // post tractor overhead
                event->tscs[3]=tsc_exit;

                l_tscs[0]=current_array[tid][record_counter[tid]].tscs[0];
                l_tscs[1]=current_array[tid][record_counter[tid]].tscs[1];
                l_tscs[2]=current_array[tid][record_counter[tid]].tscs[2];
                l_tscs[3]=current_array[tid][record_counter[tid]].tscs[3] - l_tscs[0];

        */

        extract(evt.e_time.func.in, trace_line_f.at(3), 10);
        extract(evt.e_time.mpi.in, trace_line_f.at(4), 10);
        extract(evt.e_time.mpi.out, trace_line_f.at(5), 10);
        extract(evt.e_time.func.out, trace_line_f.at(6), 10);
        evt.e_time.mpi.out = evt.e_time.func.out - evt.e_time.mpi.out;
        extract(evt.total_clocks_bitween_calls, trace_line_f.at(7), 10);
        extract(evt.l_num_calls, trace_line_f.at(8), 10);
        update_time_predictor(fabric_tsc_corrector[rank], &evt, NULL);
        nodes.at(node).ranks.at(rank).insert_fabric_event(evt);
        if (evt.ut_time.func.in >= event->ut_time.func.out)
            break;
    };
    return true;
}
#else
PEAT_Bool PET_Core::read_fabric_events(PET_Event *event)
{
    PEAT_Node node = event->node;
    PEAT_Node rank = event->rank;
    String_Split trace_line_f;

    while (input_fab->read_lines(trace_line_f, rank, ':')) {
        if (trace_line_f.size() < 2)
            continue;
        PET_Fabric_Event evt, tmp_evt;
        extract(evt.id, trace_line_f.at(0), 10);
        extract(evt.func_id, trace_line_f.at(2), 10, false);
        if (evt.func_id == TMPI_FUNC_ID_NONE) {
            evt.func_id = global_func_id.idx_by_name(trace_line_f.at(2));
        }

        /*        event->tscs[0]=tsc_entry;
                event->tscs[1]=tsc_mpi_entry-tsc_entry; //pre tractor overhead
                event->psm2.num_calls=1;
                event->psm2.total_clocks_bitween_calls=0;
                tsc_exit = rdtsc();
                event->tscs[2]=tsc_exit - tsc_mpi_exit; // post tractor overhead
                event->tscs[3]=tsc_exit;

                l_tscs[0]=current_array[tid][record_counter[tid]].tscs[0];
                l_tscs[1]=current_array[tid][record_counter[tid]].tscs[1];
                l_tscs[2]=current_array[tid][record_counter[tid]].tscs[2];
                l_tscs[3]=current_array[tid][record_counter[tid]].tscs[3] - l_tscs[0];

        */

        extract(evt.e_time.func.in, trace_line_f.at(3), 10);
        extract(evt.e_time.mpi.in, trace_line_f.at(4), 10);
        extract(evt.e_time.mpi.out, trace_line_f.at(5), 10);
        extract(evt.e_time.func.out, trace_line_f.at(6), 10);
        extract(evt.total_clocks_bitween_calls, trace_line_f.at(7), 10);
        extract(evt.l_num_calls, trace_line_f.at(8), 10);

        PEAT_Clock pre_tractor_overhead = evt.e_time.mpi.in / evt.l_num_calls;
        PEAT_Clock post_tractor_overhead = evt.e_time.mpi.out / evt.l_num_calls;

        PEAT_Clock clocks_bitween_calls = evt.total_clocks_bitween_calls / evt.l_num_calls;
        evt.e_time.mpi.out = evt.e_time.func.out - evt.e_time.mpi.out;
        PEAT_Clock func_clocks = (evt.e_time.func.out - evt.total_clocks_bitween_calls) / evt.l_num_calls - pre_tractor_overhead - post_tractor_overhead;
        update_time_predictor(fabric_tsc_corrector[rank], &evt);

        for (PEAT_LNum i = 0; i < evt.l_num_calls; i++) {
            tmp_evt = evt;
            tmp_evt.e_time.func.in = evt.e_time.func.in + i * (func_clocks + clocks_bitween_calls + pre_tractor_overhead + post_tractor_overhead);
            tmp_evt.e_time.mpi.in = tmp_evt.e_time.func.in + pre_tractor_overhead;
            tmp_evt.e_time.mpi.out = tmp_evt.e_time.mpi.in + func_clocks;
            tmp_evt.e_time.func.out = i == evt.l_num_calls ? evt.e_time.func.out : tmp_evt.e_time.mpi.out + post_tractor_overhead;
            /*
                        tmp_evt.ut_time.func.in = evt.ut_time.func.in + i * ( func_clocks + clocks_bitween_calls );
                        tmp_evt.ut_time.mpi.in = tmp_evt.ut_time.func.in + pre_tractor_overhead;
                        tmp_evt.ut_time.mpi.out = tmp_evt.ut_time.mpi.in + func_clocks;
                        tmp_evt.ut_time.func.out = tmp_evt.ut_time.mpi.out + post_tractor_overhead;
            */
            tmp_evt.ut_time = tmp_evt.e_time;
            tmp_evt.total_clocks_bitween_calls = 0;
            tmp_evt.l_num_calls = 1;
            nodes.at(node).ranks.at(rank).insert_fabric_event(tmp_evt);
        }
        if (evt.ut_time.func.in >= event->ut_time.func.out)
            break;
    };
    return true;
}
#endif

PEAT_Bool PET_Core::update_time_predictor(PEAT_Clock &predictor, PET_Time_Event *event, PET_Event *host_evt)
{
#if 1 // def _DEBUG
    event->raw_time = event->e_time;
    event->time_predictor = predictor;
#endif
    event->e_time.func.in += predictor;
    if (host_evt == NULL) predictor = event->e_time.func.in; // only update predictor on regular functions
    event->e_time.mpi.in += event->e_time.func.in;
    event->e_time.mpi.out += event->e_time.func.in;
    event->e_time.func.out += event->e_time.func.in;
    if (event->e_time.pe.in > 0)
        event->e_time.pe.in += event->e_time.func.in;
    if (event->e_time.pe.out > 0)
        event->e_time.pe.out += event->e_time.func.in;
    event->ut_time = event->e_time; // update unmodified time
    return true;
}

void PET_Core::build_fabric_events_list(PET_Event *event)
{
    PET_Event *prev_event = NULL;
    PEAT_Clock min_clock = 0;
    PEAT_Clock max_clock = event->ut_time.func.in;
    if (event->host_event) {
        prev_event = event->host_event->get_prev_event(true);
    } else {
        prev_event = event->get_prev_event(true);
    }
    if (prev_event) {
        min_clock = prev_event->ut_time.func.out;
    }
    // build UC events first
    PET_Rank *r = get_pet_rank(event);
    if (event->uc_fabric_events.empty()) { // for children avoid putting events twice
        std::list<PET_Fabric_Event>::iterator fab_event_it = r->fabric_events.begin();

        for (; fab_event_it != r->fabric_events.end(); ++fab_event_it) {
            PET_Fabric_Event &evt = *fab_event_it;
            if (evt.ut_time.func.out < min_clock)
                continue;
            if (evt.ut_time.func.in > max_clock)
                break;
            PEAT_Clock pre_tractor_overhead = (evt.e_time.mpi.in - evt.e_time.func.in) / evt.l_num_calls;
            PEAT_Clock post_tractor_overhead = (evt.e_time.func.out - evt.e_time.mpi.out) / evt.l_num_calls;

            PEAT_Clock clocks_bitween_calls = evt.total_clocks_bitween_calls / evt.l_num_calls;
            PEAT_Clock func_clocks = (evt.e_time.func.out - evt.e_time.func.in - evt.total_clocks_bitween_calls) / evt.l_num_calls - pre_tractor_overhead - post_tractor_overhead;
            PET_Fabric_Event tmp_evt = evt, tmp_evt2;
            PEAT_LNum i, i_s;
            for (i = 0, i_s = 0; i < evt.l_num_calls; i++) {
                tmp_evt.e_time.func.in = evt.e_time.func.in + i_s * (func_clocks + clocks_bitween_calls + pre_tractor_overhead + post_tractor_overhead);
                tmp_evt.e_time.mpi.in = tmp_evt.e_time.func.in + pre_tractor_overhead * (i - i_s + 1);
                tmp_evt.e_time.mpi.out = tmp_evt.e_time.mpi.in + (func_clocks + clocks_bitween_calls) * (i - i_s + 1);
                tmp_evt.e_time.func.out = i == evt.l_num_calls - 1 ? evt.e_time.func.out : tmp_evt.e_time.mpi.out + post_tractor_overhead * (i - i_s + 1);
                tmp_evt.ut_time = tmp_evt.e_time;
                tmp_evt.total_clocks_bitween_calls = clocks_bitween_calls * (i - i_s + 1);
                tmp_evt.l_num_calls = i - i_s + 1;
                if (tmp_evt.ut_time.func.in < min_clock) {
                    i_s = i;
                    continue;
                }
                if (tmp_evt.ut_time.func.out > max_clock) {
                    break;
                }
                tmp_evt2 = tmp_evt;
            }
            if (tmp_evt2.total_clocks_bitween_calls >= 0)
                event->uc_fabric_events.push_back(tmp_evt2);
        }
    }
    min_clock = event->ut_time.func.in;
    max_clock = event->ut_time.func.out;
    if (event->fabric_events.empty()) { // for children avoid putting events twice
        std::list<PET_Fabric_Event>::iterator fab_event_it = r->fabric_events.begin();
        for (; fab_event_it != r->fabric_events.end(); ++fab_event_it) {
            PET_Fabric_Event &evt = *fab_event_it;
            if (evt.ut_time.func.out < min_clock)
                continue;
            if (evt.ut_time.func.in > max_clock)
                break;
            PEAT_Clock pre_tractor_overhead = (evt.e_time.mpi.in - evt.e_time.func.in) / evt.l_num_calls;
            PEAT_Clock post_tractor_overhead = (evt.e_time.func.out - evt.e_time.mpi.out) / evt.l_num_calls;

            PEAT_Clock clocks_bitween_calls = evt.total_clocks_bitween_calls / evt.l_num_calls;
            PEAT_Clock func_clocks = (evt.e_time.func.out - evt.e_time.func.in - evt.total_clocks_bitween_calls) / evt.l_num_calls - pre_tractor_overhead - post_tractor_overhead;
            PET_Fabric_Event tmp_evt = evt, tmp_evt2;
            PEAT_LNum i, i_s;
            for (i = 0, i_s = 0; i < evt.l_num_calls; i++) {
                tmp_evt.e_time.func.in = evt.e_time.func.in + i_s * (func_clocks + clocks_bitween_calls + pre_tractor_overhead + post_tractor_overhead);
                tmp_evt.e_time.mpi.in = tmp_evt.e_time.func.in + pre_tractor_overhead * (i - i_s + 1);
                tmp_evt.e_time.mpi.out = tmp_evt.e_time.mpi.in + (func_clocks + clocks_bitween_calls) * (i - i_s + 1);
                tmp_evt.e_time.func.out = i == evt.l_num_calls - 1 ? evt.e_time.func.out : tmp_evt.e_time.mpi.out + post_tractor_overhead * (i - i_s + 1);
                tmp_evt.ut_time = tmp_evt.e_time;
                tmp_evt.total_clocks_bitween_calls = clocks_bitween_calls * (i - i_s + 1);
                tmp_evt.l_num_calls = i - i_s + 1;
                if (tmp_evt.ut_time.func.in < min_clock) {
                    i_s = i;
                    continue;
                }
                if (tmp_evt.ut_time.func.out > max_clock) {
                    break;
                }
                tmp_evt2 = tmp_evt;
            }
            if (tmp_evt2.total_clocks_bitween_calls >= 0)
                event->fabric_events.push_back(tmp_evt2);
        }
    }
}
