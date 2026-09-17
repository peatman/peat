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
#include <list>

#ifdef PEAT_MPI
#include <mpi.h>
#endif
#define MIN_COMPLETION_TIME 3000

PEAT_Bool PET_Completion_Event::get_bounds()
{
    for (std::list<PET_Fabric_Event>::const_iterator fab_event_it = fabric_events.cbegin(); fab_event_it != fabric_events.cend(); ++fab_event_it) {
        PEAT_Clock d2 = ut_time.mpi.out - fab_event_it->ut_time.mpi.out;
        if (d2 < 0)
            d2 = 0;
        if (t_time.pe.in <= 0) {
            PEAT_Clock d = fab_event_it->ut_time.mpi.in - ut_time.mpi.in;
            if (d < 0)
                d = 0;
            t_time.pe.in = t_time.func.in + d;
            e_time.pe.in = e_time.func.in + d;
        }
        t_time.pe.out = t_time.func.out - d2;
        e_time.pe.out = e_time.func.out - d2;
    }
#ifdef _DEBUG
    PEAT_Clock dur = e_time.pe.duration();
#endif
    return true;
}

PEAT_Bool PET_Completion_Event::override_children()
{
    if (!is_children_overridden) {
        for (std::list<PET_Request>::iterator req_it = triggered_requests.begin(); req_it != triggered_requests.end(); ++req_it) {
            PET_Event *event = req_it->event;
            if (!event->is_written && event->host_event == NULL)
                return false;
        }

        // PEAT_Signed_Num index = 0;
        for (std::list<PET_Request>::iterator req_it = triggered_requests.begin(); req_it != triggered_requests.end(); ++req_it) {
            PET_Event *event = req_it->event;
            if (event) {
                // PET_Request& pet_req = *req_it;
                // event->event_parts = 0;
                switch (event->get_event_type()) {
                case PET_EVENT_TYPE_TOR: {
                    PET_ToR_Event *evt = static_cast<PET_ToR_Event *>(event);
                    tor_events[req_it->request] = evt;
                    evt->set_block(true); // evt->set_verify();
                    evt->clear_estimated();
                    evt->ut_time = ut_time;
                    // evt->fabric_events = fabric_events;
                    p_events.push_back(evt);
                    if (evt->get_parent_event() == NULL) {
                        if (evt->get_host_event() == NULL) {
                            get_core()->remove_entry(evt); // standalone mpi_i_xxx
                            // evt->secondary_id = index++;
                        }
                    } else {
                        PET_Start_Event *event = static_cast<PET_Start_Event *>(evt->get_parent_event());
                        event->remove_event(req_it->request);
                    }
                    evt->set_parent_event(this);
                } break;
                case PET_EVENT_TYPE_COLLECTIVE: {
                    PET_Collective_Event *evt = static_cast<PET_Collective_Event *>(event);
                    collective_events[req_it->request] = evt;
                    evt->set_block(true);
                    evt->clear_estimated();
                    evt->ut_time = ut_time;
                    // evt->fabric_events = fabric_events;
                    p_events.push_back(evt);
                    if (evt->get_parent_event() == NULL) {
                        if (evt->get_host_event() == NULL) {
                            get_core()->remove_entry(evt);
                            // evt->secondary_id = index++;
                        }
                    } else {
                        PET_Start_Event *event = static_cast<PET_Start_Event *>(evt->get_parent_event());
                        event->remove_event(req_it->request);
                    }
                    evt->set_parent_event(this);
                } break;
                default:
                    pet_throw_error(pet_core, this, NULL, "Unknown Function Type.\n");
                    // assert(0);
                    break;
                }
            } else {
                pet_throw_error(pet_core, this, NULL, "Event not found.\n");
                // assert(0);
            }
        }
        is_children_overridden = true;
    }
    return is_children_overridden;
}

PET_Event *PET_Completion_Event::write_event(File_Storage_GZ *gz_MPI, File_Storage_GZ *gz_HWC, File_Storage_GZ *gz_bind, const char *mpi_eol, const char *hwc_eol)
{
    Completion_Entry entry(input_request_num, addr);
    if (global_func_id.check_property(func_id, PEAT_PROP_NON_DETERMINISTIC) && !get_core()->get_mo()) {
        PET_Event *chain_locker = get_core()->get_lock(rank, entry);
        if (chain_locker == NULL) {
            return NULL;
        }
        if (chain_locker != this) {
            is_written = true;
            return NULL; // scheduled for discard
        }
    }
    PET_Local_Event::write_event(gz_MPI, gz_HWC, gz_bind, "", hwc_eol);
    gz_MPI->printf(rank, ":%d:%d:%d:%d:%llx", input_request_num, triggered_requests.size(), flag, t_events.size(), addr);
    for (int i = 0; i < input_request_num; i++) {
        gz_MPI->printf(rank, ":%llx", input_requests[i]);
    }
    // 4 - tuples(index, tag, source, transferred_bytes)
    for (std::list<PET_Request>::const_iterator req_it = triggered_requests.cbegin(); req_it != triggered_requests.cend(); ++req_it) {
        gz_MPI->printf(rank, ":%d:%d:%d:%lld", req_it->index,
                       req_it->tag,
                       req_it->src,
                       req_it->t_bytes);
    }
    gz_MPI->printf(rank, "%s", mpi_eol);
    /*
    if (func_id == MPI_REQUEST_FREE_ID) {
        assert(0);//TODO: write handle
    }*/
    PET_Coll_Event::write_event(gz_MPI, gz_HWC, gz_bind, mpi_eol, hwc_eol);
    return this;
}
PEAT_Bool PET_Completion_Event::bind()
{
    return PET_Coll_Event::bind();
}
PEAT_Bool PET_Completion_Event::is_assembled() const
{
    return true;
}

PEAT_Bool PET_Completion_Event::self_discard()
{
    PEAT_Clock sub_t = e_time.func.duration();
    PET_Event *next_evt = get_next_event();
    if (next_evt) {
        if (next_evt->uc_length <= get_core()->get_blank()) {
            sub_t += next_evt->uc_length;
            next_evt->uc_length = uc_length;
        } else {
            next_evt->uc_length += uc_length;
        }
    }
    get_core()->update_active_event_chain(sub_t, this);
    get_core()->remove_entry(this);
    discarded = true;
    return true;
}
PEAT_Bool PET_Completion_Event::patch_nd_event(Completion_Chain *chain)
{
    PEAT_Num size = chain->entry.size();
    if (size > 1) {
        func_id = MPI_WAITALL_ID;
    } else {
        func_id = MPI_WAIT_ID;
    }
    input_request_num = triggered_request_num = size;
    input_requests.resize(size);
    for (PEAT_Num i = 0; i < size; i++) {
        input_requests[i] = chain->entry[i].request;

        if (find_triggered_request(input_requests[i]) == NULL) {
            // extract request information from chain
            PEAT_Bool bfound = false;
            for (std::list<PET_Completion_Event *>::iterator event_it = chain->entry[i].events.begin(); event_it != chain->entry[i].events.end(); ++event_it) {
                PET_Completion_Event *event = *event_it;
                PET_Request *req = event->find_triggered_request(input_requests[i]);
                if (req) {
                    triggered_requests.push_back(*req);
                    // triggered_requests[input_requests[i]] = event->triggered_requests.at(input_requests[i]);
                    bfound = true;
                    break;
                }
            }
            if (!bfound) {
                pet_throw_error(pet_core, this, NULL, "Request not found.\n");
            }
            // assert(bfound);
        }
    }
    return true;
}
void PET_Completion_Event::handle_event(Estimation_Event *event)
{
    switch (event->type) {
    case EET_FUNC_START:
        estimation_started = true;
        split_event();
        end_submitted = false;
        handle_progress_messages(__max__(e_time.mpi.in + event_parts.prolog, event->start), false);
        PET_Coll_Event::estimate(event);
        break;
    case EET_FUNC_END:
        // assert(estimation_started);
        finish_calculations = true;
        if (!estimation_started) {
            pet_throw_error(pet_core, this, event, "Function is not estimated.\n");
        }
        set_estimated();
        put_transport_event_stat();
        if (host_event) {
            submit(event->start, EET_CHILD_COMPLETED, host_event, this, NULL);
        }
        get_core()->delete_ee_entry(event);
        break;
    case EET_RTS:
        pet_throw_error(pet_core, this, event, "Inappropriate  Code.\n");
        // assert(0);
        break;
    case EET_RTS_RECV_NOTIFY:
        pet_throw_error(pet_core, this, event, "Inappropriate  Code.\n");
        // assert(0);
        break;
    case EET_RTS_RECV:
        pet_throw_error(pet_core, this, event, "Inappropriate  Code.\n");
        // assert(0);
        break;
    case EET_CTS:
        pet_throw_error(pet_core, this, event, "Inappropriate  Code.\n");
        // assert(0);
        break;
    case EET_CTS_RECV_NOTIFY:
        pet_throw_error(pet_core, this, event, "Inappropriate  Code.\n");
        // assert(0);
        break;
    case EET_CTS_RECV:
        pet_throw_error(pet_core, this, event, "Inappropriate  Code.\n");
        // assert(0);
        break;
    case EET_MSG_TX:
        pet_throw_error(pet_core, this, event, "Inappropriate  Code.\n");
        // assert(0);
        break;
    case EET_MSG_RECV_NOTIFY:
        pet_throw_error(pet_core, this, event, "Inappropriate  Code.\n");
        // assert(0);
        break;
    case EET_MSG_RECV:
        pet_throw_error(pet_core, this, event, "Inappropriate  Code.\n");
        // assert(0);
        break;
    case EET_ACK_RECV_NOTIFY:
        pet_throw_error(pet_core, this, event, "Inappropriate  Code.\n");
        // assert(0);
        break;
    case EET_ACK_RECV:
        pet_throw_error(pet_core, this, event, "Inappropriate  Code.\n");
        // assert(0);
        break;
    case EET_PROGRESS_NOTIFY:
        if (!finish_calculations && estimation_started) {
            handle_progress_messages(__max__(e_time.mpi.in + event_parts.prolog, event->start), false);
        }
        PET_Coll_Event::estimate(event);
        break;
    case EET_NOTIFY:
        if (!finish_calculations && estimation_started) {
            handle_progress_messages(__max__(e_time.mpi.in + event_parts.prolog, event->start), false);
        } else {
            PET_Event *current_event = get_current_event();
            if (current_event != NULL && current_event != this) {
                submit(event->start, EET_NOTIFY, current_event, this, NULL);
            }
        }
        get_core()->delete_ee_entry(event);
        break;
    case EET_CHILD_COMPLETED:
        // notification from child received
        if (!finish_calculations && estimation_started) {
            handle_progress_messages(__max__(e_time.mpi.in + event_parts.prolog, event->start), false);
        }
        PET_Coll_Event::estimate(event);
        break;
    default:
        pet_throw_error(pet_core, this, event, "Inappropriate  Code.\n");
        // assert(0);
        break;
    }
}

PEAT_Bool PET_Completion_Event::estimate(Estimation_Event *event)
{
    if (func_id == MPI_REQUEST_FREE_ID || func_id == MPI_CANCEL_ID) {
        return PET_Local_Event::estimate(event);
    }
    PEAT_Clock event_start = e_time.mpi.out;
    // if ( event ) {
    event_start = event->start;
    handle_event(event);
    // }
    if (is_estimated() || finish_calculations || !estimation_started)
        return true;
    PEAT_Bool is_estimation_finished = true;
    if (global_func_id.check_property(func_id, PEAT_PROP_NON_DETERMINISTIC) && !is_chained) {
        Completion_Entry entry(input_request_num, addr);
        for (PEAT_Num i = 0; i < input_request_num; i++) {
            if (input_requests[i] == get_core()->get_comm()->get_MPI_REQUEST_NULL())
                continue;
            PEAT_Bool is_completed = false;
            if (find_triggered_request(input_requests[i]))
                is_completed = true;
            get_core()->add_request_to_chain(rank, entry, i, input_requests[i], this, is_completed);
        }
        is_chained = true;
    }

    Completion_Entry entry(input_request_num, addr);
    if (global_func_id.check_property(func_id, PEAT_PROP_NON_DETERMINISTIC)) {
        assert(event);
        if (get_core()->get_lock(rank, entry) != this) {
            self_discard();
            submit(event_start, EET_FUNC_END, this, this, NULL);
            return true;
        }
    }
    if (override_children()) {
        /*
        TE_Event* max_evt = NULL;
        TE_Event* min_evt = NULL;
        for (std::map<PEAT_MPI_Request, PET_ToR_Event *>::iterator event_it = tor_events.begin(); event_it != tor_events.end(); ++event_it) {
            PET_ToR_Event* evt = event_it->second;
            TE_Event message = evt->get_highest_message_completion();
            if (max_evt == NULL || message.finish > max_evt->finish) max_evt = &message;
            message = evt->get_lowest_message_completion();
            if (min_evt == NULL || (message.finish < min_evt->finish && message.finish>0)) min_evt = &message;
        }
        */
        PEAT_Clock max_finish = 0;
        PEAT_Clock min_start = LLONG_MAX;
        for (std::map<PEAT_MPI_Request, PET_ToR_Event *>::iterator event_it = tor_events.begin(); event_it != tor_events.end(); ++event_it) {
            PET_ToR_Event *evt = event_it->second;
            PEAT_Clock start = evt->get_message_start(PEAT_DIRECTION_AUTO_SELECT);
            PEAT_Clock finish = evt->get_ack_finish(PEAT_DIRECTION_AUTO_SELECT);
            if (start > 0) {
                min_start = __min__(min_start, start);
            }
            if (finish > 0) {
                max_finish = __max__(max_finish, finish);
            } else {
                is_estimation_finished = false;
                break;
            }
        }
        for (std::map<PEAT_MPI_Request, PET_Collective_Event *>::iterator event_it = collective_events.begin(); event_it != collective_events.end(); ++event_it) {
            PET_Collective_Event *evt = event_it->second;
            PEAT_Clock start = evt->get_message_start();
            PEAT_Clock finish = evt->get_message_finish();
            if (start > 0) {
                min_start = __min__(min_start, finish);
            }
            if (finish > 0) {
                max_finish = __max__(max_finish, finish);
            } else {
                is_estimation_finished = false;
                break;
            }
        }
        PEAT_Clock start = get_event_start();
        PEAT_Clock finish = get_event_finish();
        if (start > 0) {
            min_start = __min__(min_start, start);
        }
        if (finish > 0) {
            max_finish = __max__(max_finish, finish);
        } else {
            is_estimation_finished = false;
        }

        // not sure how calculate overlapped time correctly
        if ((is_estimation_finished) && !end_submitted) { //|| event_parts.meat == 0 ???
            Completion_Entry entry(input_request_num, addr);
            Completion_Chain *chain = NULL;
            if (global_func_id.check_property(func_id, PEAT_PROP_NON_DETERMINISTIC)) {
                chain = get_core()->get_chain(rank, entry);
                // assuming now that we are in locker (other branch terminates early)
                /*
                // calculating total amount of user_code excluding blanks
                std::map <PEAT_UID, PET_Completion_Event *> working_set;
                for (std::vector <Extended_Request>::iterator entry_it = chain->entry.begin(); entry_it != chain->entry.end(); ++entry_it) {
                    for (std::list <PET_Completion_Event*>::iterator event_it = entry_it->events.begin(); event_it != entry_it->events.end(); ++event_it) {
                        assert(working_set.count((*event_it)->primary_id) == 0);
                        working_set[(*event_it)->primary_id] = (PET_Completion_Event *)(*event_it);
                    }
                }
                std::map <PEAT_UID, PET_Completion_Event *>::iterator event_it = working_set.begin();
                PEAT_Clock min_start = event_it->second->e_time.func.in, total_uc = 0;
                if (event_it->second->uc_length >= get_core()->get_blank()) total_uc += event_it->second->uc_length;
                if (event_it->second != this) delete (event_it->second);
                for (++event_it; event_it != working_set.end(); ++event_it) {
                    if (event_it->second->uc_length >= get_core()->get_blank()) total_uc += event_it->second->uc_length;
                    if (event_it->second != this) delete (event_it->second);
                }
                total_uc -= uc_length; //subtract since we already
                PET_Event* prev_event = get_core()->get_prev_event();
                PEAT_Clock prev_out = 0;
                if (prev_event) prev_out = prev_event->e_time.func.out;
                e_time.mpi.in = e_time.func.in = __max__(min_start + total_uc, prev_out + uc_length);
                */
            }
            // event_parts.prolog = o_event_parts.prolog;
            if (max_finish > e_time.mpi.in + event_parts.prolog) {
                event_parts.meat = max_finish - e_time.mpi.in - event_parts.prolog;
            } /*else {
                if (event_parts.meat > MIN_COMPLETION_TIME) event_parts.meat = MIN_COMPLETION_TIME;
            }*/
            // event_parts.epilog = o_event_parts.epilog;
            if (global_func_id.check_property(func_id, PEAT_PROP_NON_DETERMINISTIC)) {
                // patch current event to WAIT(ALL)
                patch_nd_event(chain);
                // and remove chain
                get_core()->destroy_chain(rank, entry, this);
            }
            event_parts.stat.overlap = get_overlap();
            event_parts.stat.fabric = get_fabric();
            concatenate_event();
            submit(e_time.func.out, EET_FUNC_END, this, this, NULL);
            end_submitted = true;
        } /* else {
            if ( event == NULL ) {
                PET_Coll_Event::estimate(NULL);
            }
        } */
    }
    return true;
}
PEAT_Bool PET_Completion_Event::ready_to_estimate()
{
    if (is_estimated())
        return true;
    if (func_id == MPI_REQUEST_FREE_ID || func_id == MPI_CANCEL_ID)
        return PET_Local_Event::ready_to_estimate(); // request free is actually local
    PEAT_Bool ready = true;
    return ready && PET_Coll_Event::ready_to_estimate();
}
PEAT_Bool PET_Completion_Event::is_estimated() const
{
    return PET_Coll_Event::is_estimated();
}

PEAT_Bool PET_Completion_Event::safe_to_delete()
{
    PEAT_Bool is_safe = (is_written && PET_Coll_Event::safe_to_delete()) || discarded; // straight call to coll since we have to skip tor and collective queues check
    return is_safe;
}
PEAT_Bool PET_Completion_Event::can_be_destroyed()
{
    PEAT_Bool is_safe = PET_Start_Event::can_be_destroyed() || discarded; // straight call to coll since we have to skip tor and collective queues check
    return is_safe;
}
PET_ToR_Event *find_event_by_request(PEAT_MPI_Request request, PET_Coll_Event *event)
{
    if (event)
        return static_cast<PET_ToR_Event *>(event->PET_Coll_Event::query_request(request));
    else
        return NULL;
}

PEAT_Bool PET_Completion_Event::read_event(PEAT_Rank r_rank, const String_Split &mpi_string, const String_Split &hwc_string, PET_Coll_Event *host)
{
    PET_Local_Event::read_event(r_rank, mpi_string, hwc_string, host);
    extract(input_request_num, mpi_string.at(4));
    extract(triggered_request_num, mpi_string.at(5));
    extract(flag, mpi_string.at(6));
    int num_colts;
    extract(num_colts, mpi_string.at(7));
    t_events.resize(num_colts, NULL);
    extract(addr, mpi_string.at(8), 16);
    input_requests.resize(input_request_num);
    for (int i = 0; i < input_request_num; i++) {
        PEAT_MPI_Request req;
        extract(req, mpi_string.at(9 + i), 16);
        input_requests[i] = req;
#if 0
		if (req == get_core()->com_store->get_MPI_REQUEST_NULL()) continue;
		if (!get_core()->get_mo()) { // no need to push blank events in matching only mode
			if (func_id != MPID_REQUEST_RELEASE_ID) {
				PET_Event* event = get_core()->find_event_by_request(rank, req);
				if (event == NULL) event = find_event_by_request(req, host);
				if (event) {
					if (event->get_event_type() == PET_EVENT_TYPE_START) {
						event = event->get_event_from_req(req);
						if (event == NULL) {
							pet_throw_error(pet_core, this, NULL, "Event not found.\n");
						}
						//assert(event);
					}
					event->set_blank(this);
#ifdef _DEBUG
				}
				else {
					event = get_core()->find_event_by_request(rank, req);
#endif
				}
			}
		}
#endif
    }
    // 4 - tuples(index, tag, source, transferred_bytes)
    for (int i = 0; i < triggered_request_num; i++) {
        PET_Request req;
        int index;
        extract(index, mpi_string.at(9 + input_request_num + i * 4 + 0));
        extract(req.tag, mpi_string.at(9 + input_request_num + i * 4 + 1));
        PEAT_Rank src;
        extract(src, mpi_string.at(9 + input_request_num + i * 4 + 2));
        req.src = src;
        extract(req.t_bytes, mpi_string.at(9 + input_request_num + i * 4 + 3));
        req.index = index;
        if (input_requests.at(index) == get_core()->get_comm()->get_MPI_REQUEST_NULL())
            continue;
        PET_Event *event = get_core()->find_event_by_request(rank, input_requests.at(index));
        if (event == NULL) {
            event = find_event_by_request(input_requests.at(index), host);
        }
#ifdef _DEBUG
        if (event == NULL) {
            event = get_core()->find_event_by_request(rank, input_requests.at(index));
        }
        if (event == NULL)
            event = find_event_by_request(input_requests.at(index), host);
#endif
        if (event == NULL) {
            pet_throw_error(pet_core, this, NULL, "Event not found.\n");
        }
        assert(event);
        if (event->get_event_type() == PET_EVENT_TYPE_START) {
            event = event->get_event_from_req(input_requests.at(index));
            if (event == NULL) {
                pet_throw_error(pet_core, this, NULL, "Event not found.\n");
            }
            // assert(event);
        }
        req.event = event;
        event->set_recv_tag(req.tag);    // won't be updated if it's send function
        event->set_src(req.src);         // won't be updated if it's send function
        event->set_t_bytes(req.t_bytes); // won't be updated if it's send function
        event->set_completion(this);
        req.request = input_requests.at(index);
        triggered_requests.push_back(req);
        // triggered_requests[input_requests.at( index )] = req;
    }
    if (func_id == MPI_REQUEST_FREE_ID || func_id == MPI_CANCEL_ID) {
        PET_ToR_Event *event = get_core()->find_event_by_request(rank, input_requests.at(0));
        if (event == NULL)
            event = find_event_by_request(input_requests.at(0), host);
        if (event) {
            event->set_completion(this);
            event->set_treat_as_local();
            // func_id = MPI_WAIT_ID;// crutch !!!
            // assert( 0 );
        }
        get_core()->remove_persistent_request(rank, input_requests.at(0));
    }
    if (func_id == MPID_REQUEST_RELEASE_ID) {
        PET_ToR_Event *event = get_core()->find_event_by_request(rank, input_requests.at(0));
        if (event == NULL) {
            event = find_event_by_request(input_requests.at(0), host);
        }
        if (event) {
            event->set_completion(this);
            PET_Request req;
            req.event = event;
            req.index = 0;
            req.request = input_requests.at(0);
            if (event->check_tx()) {
                req.tag = event->get_send_tag();
                req.src = event->get_dest();
                req.t_bytes = get_tx_bytes();
            }
            if (event->check_rx()) {
                req.tag = event->get_recv_tag();
                req.src = event->get_src();
                req.t_bytes = get_rx_bytes();
            }
            triggered_requests.push_back(req);
            // func_id = MPIC_WAIT_ID;// crutch !!!
#ifdef _DEBUG
        } else {
            event = get_core()->find_event_by_request(rank, input_requests.at(0));
            if (event == NULL)
                event = find_event_by_request(input_requests.at(0), host);
#endif
        }
    }
    return true;
}
PEAT_Bool PET_Completion_Event::check_tx() const
{
    for (std::list<PET_Request>::const_iterator req_it = triggered_requests.cbegin(); req_it != triggered_requests.cend(); ++req_it) {
        PET_Event *evt = req_it->event;
        if (evt->check_tx())
            return true;
    }
    return false;
}
PEAT_Bool PET_Completion_Event::check_rx() const
{
    for (std::list<PET_Request>::const_iterator req_it = triggered_requests.cbegin(); req_it != triggered_requests.cend(); ++req_it) {
        PET_Event *evt = req_it->event;
        if (evt->check_rx())
            return true;
    }
    return false;
}
PEAT_Clock PET_Completion_Event::get_overlap() const
{
    PEAT_Clock total_overlap = 0;
    for (std::map<PEAT_MPI_Request, PET_ToR_Event *>::const_iterator event_it = tor_events.cbegin(); event_it != tor_events.cend(); ++event_it) {
        PET_ToR_Event *evt = event_it->second;
        total_overlap += evt->get_overlap();
    }
    for (std::map<PEAT_MPI_Request, PET_Collective_Event *>::const_iterator event_it = collective_events.cbegin(); event_it != collective_events.cend(); ++event_it) {
        PET_Collective_Event *evt = event_it->second;
        total_overlap += evt->get_overlap();
    }
    return total_overlap;
}
PEAT_Clock PET_Completion_Event::get_fabric() const
{
    PEAT_Clock total_fabric = 0;
    for (std::map<PEAT_MPI_Request, PET_ToR_Event *>::const_iterator event_it = tor_events.cbegin(); event_it != tor_events.cend(); ++event_it) {
        PET_ToR_Event *evt = event_it->second;
        total_fabric += evt->get_fabric();
    }
    for (std::map<PEAT_MPI_Request, PET_Collective_Event *>::const_iterator event_it = collective_events.cbegin(); event_it != collective_events.cend(); ++event_it) {
        PET_Collective_Event *evt = event_it->second;
        total_fabric += evt->get_fabric();
    }
    return total_fabric;
}
PEAT_Bool PET_Completion_Event::split_event()
{
    if (event_parts.duration() > 0)
        return true;
    return PET_Coll_Event::split_event();
}
void PET_Completion_Event::concatenate_event()
{
    if (t_events.empty()) {
        PET_Start_Event::concatenate_event();
    } else {
        PET_Coll_Event::concatenate_event();
    }
    return;
}
PEAT_Bool PET_Completion_Event::initiate()
{
    if (is_initiated || is_estimated()) {
        return true;
    }
    if (func_id == MPID_REQUEST_RELEASE_ID) {
        PEAT_Clock max_finish = -1;
        for (std::list<PET_Request>::iterator req_it = triggered_requests.begin(); req_it != triggered_requests.end(); ++req_it) {
            PET_ToR_Event *event = static_cast<PET_ToR_Event *>(req_it->event);
            PEAT_Clock finish = event->get_ack_finish(PEAT_DIRECTION_AUTO_SELECT);
            if (finish > 0) {
                max_finish = __max__(max_finish, finish);
            } else {
                max_finish = -1;
                break;
            }
        }
        if (max_finish > 0 && max_finish < e_time.mpi.in) {
            assert(host_event);
            PET_Event *prev_event = host_event->get_prev_event(false);
            if (prev_event) {
                PEAT_Clock sub_t = e_time.func.in - __max__(max_finish, prev_event->e_time.func.out);
                if (sub_t > 0) {
                    host_event->move_events((PEAT_Signed_Num)secondary_id, sub_t, true);
                }
            }
        }
    }

    return PET_Coll_Event::initiate();
}
PEAT_Rank PET_Completion_Event::get_rank_read_suggestion()
{
    return PET_Coll_Event::get_rank_read_suggestion();
}
