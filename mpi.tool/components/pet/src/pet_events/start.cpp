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

PET_Start_Event::~PET_Start_Event()
{
    //	if (!is_colt()) { // for collective transfers those functions are deleted by "head" collective destructor
    shutdown_initiated = true;
    // p_events holds every triggered event; the request-keyed maps may hold fewer of them since MPICH reuses
    // the handle of immediately completed requests (all MPIC_Isend of an alltoall share one handle)
    std::set<PET_Event *> done;
    for (std::vector<PET_Event *>::iterator event_it = p_events.begin(); event_it != p_events.end(); ++event_it) {
        PET_Event *evt = *event_it;
        if (evt == NULL || done.count(evt))
            continue;
        done.insert(evt);
        if (evt->host_event == NULL) {
            delete evt; // standalone MPI_Ixxx: owned by this completion
        } else {
            evt->notify_before_destroy(this); // collective transfer: owned by its container
        }
    }
    for (std::map<PEAT_MPI_Request, PET_ToR_Event *>::iterator event_it = tor_events.begin(); event_it != tor_events.end(); ++event_it) {
        if (done.count(event_it->second))
            continue;
        if (event_it->second->host_event == NULL) {
            delete event_it->second;
        } else {
            event_it->second->notify_before_destroy(this);
        }
    }
    for (std::map<PEAT_MPI_Request, PET_Collective_Event *>::iterator event_it = collective_events.begin(); event_it != collective_events.end(); ++event_it) {
        if (done.count(event_it->second))
            continue;
        if (event_it->second->host_event == NULL) {
            delete event_it->second;
        } else {
            event_it->second->notify_before_destroy(this);
        }
    }
}

void PET_Start_Event::set_times()
{
    PEAT_Signed_Num index = 0;
    for (std::map<PEAT_MPI_Request, PET_ToR_Event *>::iterator event_it = tor_events.begin(); event_it != tor_events.end(); ++event_it) {
        PET_ToR_Event *evt = event_it->second;
        evt->e_time = e_time;
        evt->t_time = t_time;
        evt->o_time = o_time;
        evt->ut_time = ut_time;
        evt->set_sub_index(index++);
    }
    for (std::map<PEAT_MPI_Request, PET_Collective_Event *>::iterator event_it = collective_events.begin(); event_it != collective_events.end(); ++event_it) {
        PET_Collective_Event *evt = event_it->second;
        evt->e_time = e_time;
        evt->t_time = t_time;
        evt->o_time = o_time;
        evt->ut_time = ut_time;
        evt->set_sub_index(index++);
    }
}
void PET_Start_Event::scale_event()
{
    PET_Local_Event::scale_event();
}

PET_Event *PET_Start_Event::write_event(File_Storage_GZ *gz_MPI, File_Storage_GZ *gz_HWC, File_Storage_GZ *gz_bind, const char *mpi_eol, const char *hwc_eol)
{
    PET_Local_Event::write_event(gz_MPI, gz_HWC, gz_bind, "", hwc_eol);
    gz_MPI->printf(rank, ":%d:%d:%d", input_request_num, triggered_request_num, flag);
    gz_MPI->printf(rank, ":%llx", addr);
    for (int i = 0; i < input_request_num; i++) {
        gz_MPI->printf(rank, ":%llx", input_requests[i]);
    }
    for (std::map<PEAT_MPI_Request, PET_ToR_Event *>::iterator event_it = tor_events.begin(); event_it != tor_events.end(); ++event_it) {
        PET_ToR_Event *evt = event_it->second;
        evt->write_bind(gz_bind);
    }
    for (std::map<PEAT_MPI_Request, PET_Collective_Event *>::iterator event_it = collective_events.begin(); event_it != collective_events.end(); ++event_it) {
        PET_Collective_Event *evt = event_it->second;
        evt->write_bind(gz_bind);
    }
    gz_MPI->printf(rank, "%s", mpi_eol);
    write_nested_records(gz_MPI, gz_HWC, gz_bind, mpi_eol, hwc_eol); // progress engine records issued inside this call
    return this;
}

PEAT_Bool PET_Start_Event::bind()
{
    if (!is_all_bound) {
        std::map<PEAT_MPI_Request, PET_ToR_Event *>::iterator tor_event;
        PEAT_Bool r = get_first_tor_event(tor_event);
        PEAT_Bool peers_required = false;
        while (r) {
            if (!tor_event->second->is_bound()) {
                tor_event->second->bind();
                peers_required = peers_required || !tor_event->second->is_bound();
            }
            r = get_next_tor_event(tor_event);
        };
        if (peers_required) {
            is_all_bound = false;
        } else {
            is_all_bound = true;
        }
    }
    return is_all_bound;
}

void PET_Start_Event::handle_event(Estimation_Event *event)
{
    switch (event->type) {
    case EET_FUNC_START:
        if (!estimation_started) {
            // set_times();
            estimation_started = true;
            if (get_core()->get_mo()) {
                // concatenate_event();
                finish_calculations = true;
                submit(e_time.func.out, EET_FUNC_END, this, this, NULL);
            } else {
                split_event();
                for (std::map<PEAT_MPI_Request, PET_ToR_Event *>::iterator event_it = tor_events.begin(); event_it != tor_events.end(); ++event_it) {
                    PET_ToR_Event *evt = event_it->second;
                    submit(event->start, EET_FUNC_START, evt, this, NULL);
                }
                for (std::map<PEAT_MPI_Request, PET_Collective_Event *>::iterator event_it = collective_events.begin(); event_it != collective_events.end(); ++event_it) {
                    PET_Collective_Event *evt = event_it->second;
                    submit(event->start, EET_FUNC_START, evt, this, NULL);
                }
            }
        }
        break;
    case EET_FUNC_END:
        // assert(estimation_started);
        if (!estimation_started) {
            pet_throw_error(pet_core, this, event, "Function is not estimated.\n");
        }
        set_estimated();
        put_transport_event_stat();
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
    case EET_CTS_RECV:
        pet_throw_error(pet_core, this, event, "Inappropriate  Code.\n");
        // assert(0);
        break;
    case EET_CTS_RECV_NOTIFY:
        pet_throw_error(pet_core, this, event, "Inappropriate  Code.\n");
        // assert(0);
        break;
    case EET_MSG_TX:
        pet_throw_error(pet_core, this, event, "Inappropriate  Code.\n");
        // assert(0);
        break;
    case EET_MSG_RECV:
        pet_throw_error(pet_core, this, event, "Inappropriate  Code.\n");
        // assert(0);
        break;
    case EET_MSG_RECV_NOTIFY:
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
        break;
        break;
    case EET_NOTIFY:
        break;
    default:
        pet_throw_error(pet_core, this, event, "Inappropriate  Code.\n");
        // assert(0);
        break;
    }
    get_core()->delete_ee_entry(event);
}

PEAT_Bool PET_Start_Event::ready_to_estimate()
{
    for (std::map<PEAT_MPI_Request, PET_ToR_Event *>::iterator event_it = tor_events.begin(); event_it != tor_events.end(); ++event_it) {
        PET_ToR_Event *evt = event_it->second;
        if (evt->ready_to_estimate() == false)
            return false;
    }
    for (std::map<PEAT_MPI_Request, PET_Collective_Event *>::iterator event_it = collective_events.begin(); event_it != collective_events.end(); ++event_it) {
        PET_Collective_Event *evt = event_it->second;
        if (evt->ready_to_estimate() == false)
            return false;
    }
    return true;
    // return PET_Local_Event::ready_to_estimate();
}
PEAT_Bool PET_Start_Event::remove_event(PEAT_MPI_Request req)
{
    // assert(tor_events.count(req) + collective_events.count(req) == 1);
    if (tor_events.count(req) + collective_events.count(req) != 1) {
        pet_throw_error(pet_core, this, NULL, "Multiple or no event found for single request.\n");
    }
    if (tor_events.count(req) == 1)
        tor_events.erase(req);
    if (collective_events.count(req) == 1)
        collective_events.erase(req);
    return true;
}

PEAT_Bool PET_Start_Event::safe_to_delete()
{
    PEAT_Bool is_safe = is_written && tor_events.empty() && collective_events.empty() && PET_Coll_Event::safe_to_delete();
    return is_safe;
}

PEAT_Bool PET_Start_Event::can_be_destroyed()
{
    for (std::map<PEAT_MPI_Request, PET_ToR_Event *>::iterator event_it = tor_events.begin(); event_it != tor_events.end(); ++event_it) {
        PET_ToR_Event *evt = event_it->second;
        if (evt->can_be_destroyed() == false)
            return false;
    }
    for (std::map<PEAT_MPI_Request, PET_Collective_Event *>::iterator event_it = collective_events.begin(); event_it != collective_events.end(); ++event_it) {
        PET_Collective_Event *evt = event_it->second;
        if (evt->can_be_destroyed() == false)
            return false;
    }
    return PET_Coll_Event::can_be_destroyed();
}

PET_Event *PET_Start_Event::get_real_event(PET_Event *evt)
{
    PEAT_MPI_Request req = evt->get_mpi_request();
    if (tor_events.count(req) == 1) {
        return tor_events.at(req);
    }
    return NULL;
}
PEAT_Bool PET_Start_Event::is_assembled() const
{
    return true;
}

PEAT_Rank PET_Start_Event::get_rank_read_suggestion()
{
    std::map<PEAT_MPI_Request, PET_ToR_Event *>::iterator tor_event;
    PEAT_Bool r = get_first_tor_event(tor_event);
    while (r) {
        if (!tor_event->second->is_bound()) {
            return tor_event->second->get_rank_read_suggestion();
        }
        r = get_next_tor_event(tor_event);
    };
    return rank;
}
void PET_Start_Event::concatenate_event()
{
#if 1
    PET_Local_Event::concatenate_event();
#else
    if (func_id == MPI_REQUEST_FREE_ID) {
        PET_Local_Event::concatenate_event();
    } else {
        e_time.func.in = e_time.mpi.in;
        e_time.mpi.out = e_time.func.out = e_time.mpi.in + event_parts.prolog +
                                           event_parts.meat + event_parts.epilog;
        e_time.pe.in = e_time.mpi.in + event_parts.prolog;
        e_time.pe.out = e_time.mpi.out - event_parts.epilog;

        PEAT_Clock sub_t = o_event_parts.prolog - event_parts.prolog +
                           o_event_parts.meat - event_parts.meat +
                           o_event_parts.epilog - event_parts.epilog;
        sub_t_ae = sub_t;
        sub_t_ae_t += sub_t_ae;
        if (parent_event == NULL && host_event == NULL) {
            get_core()->update_active_event_chain(sub_t_ae, this);
        }
    }
#endif
    return;
}
PET_Event *PET_Start_Event::get_first_child_event()
{
    if (p_events.empty())
        return this;
    int_event_index = 0;
    if (int_event_index >= p_events.size() || p_events.at(int_event_index) == NULL)
        return this;
    return p_events.at(int_event_index);
};
PET_Event *PET_Start_Event::get_last_child_event()
{
    if (p_events.empty())
        return this;
    int_event_index = p_events.size() - 1;
    if (int_event_index >= p_events.size() || p_events.at(int_event_index) == NULL)
        return this;
    return p_events.at(int_event_index);
};
PET_Event *PET_Start_Event::get_next_child_event()
{
    if (p_events.empty())
        return this;
    int_event_index++;
    if (int_event_index >= p_events.size() || p_events.at(int_event_index) == NULL)
        return this;
    return p_events.at(int_event_index);
};
PET_Event *PET_Start_Event::get_prev_child_event()
{
    int_event_index--;
    if (int_event_index >= p_events.size() || p_events.at(int_event_index) == NULL)
        return this;
    return p_events.at(int_event_index);
};

PEAT_Bool PET_Start_Event::read_event(PEAT_Rank r_rank, const String_Split &mpi_string, const String_Split &hwc_string, PET_Event *host, const PEAT_Record_Header &hdr)
{
    PET_Local_Event::read_event(r_rank, mpi_string, hwc_string, host, hdr);
    extract(input_request_num, mpi_string.at(hdr.payload + 1));
    extract(triggered_request_num, mpi_string.at(hdr.payload + 2));
    extract(flag, mpi_string.at(hdr.payload + 3));
    extract(addr, mpi_string.at(hdr.payload + 4), 16);
    input_requests.resize(input_request_num);
    for (int i = 0; i < input_request_num; i++) {
        PEAT_MPI_Request req;
        extract(req, mpi_string.at(hdr.payload + 5 + i), 16);
        input_requests[i] = req;
    }
    for (int i = 0; i < input_request_num; i++) {
        PEAT_MPI_Request req = input_requests[i];
        PET_Event *event = get_core()->find_event_by_pers_request(rank, req);
#ifdef _DEBUG
        if (event == NULL) {
            event = get_core()->find_event_by_pers_request(rank, req);
        }
#endif
        if (event) {
            switch (event->get_event_type()) {
            case PET_EVENT_TYPE_TOR: {
                PET_ToR_Event *evt_t = static_cast<PET_ToR_Event *>(event);
                PET_ToR_Event *evt = new PET_ToR_Event(*evt_t);
                //*evt = *evt_t;
                tor_events[req] = evt;
                // evt->parent_func_id = parent_func_id;
                switch (evt->func_id) {
                case MPI_SEND_INIT_ID:
                    evt->func_id = MPI_ISEND_ID;
                    break;
                case MPI_RSEND_INIT_ID:
                    evt->func_id = MPI_IRSEND_ID;
                    break;
                case MPI_BSEND_INIT_ID:
                    evt->func_id = MPI_IBSEND_ID;
                    break;
                case MPI_SSEND_INIT_ID:
                    evt->func_id = MPI_ISSEND_ID;
                    break;
                case MPI_RECV_INIT_ID:
                    evt->func_id = MPI_IRECV_ID;
                    break;
                default:
                    pet_throw_error(pet_core, this, NULL, "Wrong Function.\n");
                    // assert(0);
                    break;
                }
                evt->set_parent_event(this);
                // evt->event_parts=event_parts;
                // evt->o_event_parts = o_event_parts;
                evt->ids = ids;
                evt->set_sub_index(i);
                evt->is_written = true;
                p_events.push_back(evt);
            } break;
            case PET_EVENT_TYPE_COLLECTIVE: {
                PET_Collective_Event *evt_t = static_cast<PET_Collective_Event *>(event);
                PET_Collective_Event *evt = new PET_Collective_Event(*evt_t);
                collective_events[req] = evt;
                // evt->parent_func_id = parent_func_id;
                evt->event_parts = event_parts;
                evt->o_event_parts = o_event_parts;
                switch (evt->func_id) {
                // persistent collective are not in standart yet
                default:
                    assert(0);
                    break;
                }
                evt->set_parent_event(this);
                evt->ids = ids;
                evt->set_sub_index(i);
                p_events.push_back(evt);
            } break;
            default:
                assert(0);
                break;
            }
        }
    }
    return true;
}

PEAT_Bool PET_Start_Event::estimate(Estimation_Event *event)
{

    // if ( event ) {
    handle_event(event);
    // }
    if (is_estimated() || finish_calculations)
        return true;
#if 0
	PEAT_Clock max_finish = 0;
	PEAT_Clock min_start = LLONG_MAX;
	PEAT_Bool is_estimation_finished = true;
	PEAT_Clock fabric = -1;
	PEAT_Bool no_fabric = false;
	if (check_tx()) {
		for (std::map<PEAT_MPI_Request, PET_ToR_Event *>::iterator event_it = tor_events.begin(); event_it != tor_events.end(); ++event_it) {
			PET_ToR_Event* evt = event_it->second;
			PEAT_Clock start = evt->get_txrx_start();
			PEAT_Clock finish = evt->get_txrx_finish(true);
			if (start > 0) {
				min_start = __min__(min_start, start);
			} else {
				is_estimation_finished = false;
			}
			if (finish > 0) {
				max_finish = __max__(max_finish, finish);
			} else {
				finish = evt->get_txrx_finish(false);
				no_fabric = true;
			}
			if (fabric < evt->event_parts.meat) fabric = evt->event_parts.meat;
		}
		for (std::map<PEAT_MPI_Request, PET_Collective_Event *>::iterator event_it = collective_events.begin(); event_it != collective_events.end(); ++event_it) {
			PET_Collective_Event* evt = event_it->second;
			PEAT_Clock start = evt->get_txrx_start();
			PEAT_Clock finish = evt->get_txrx_finish();
			if (start > 0) {
				min_start = __min__(min_start, finish);
			} else {
				is_estimation_finished = false;
			}
			if (finish > 0) {
				max_finish = __max__(max_finish, finish);
			}
			if (fabric < evt->event_parts.meat) fabric = evt->event_parts.meat;
		}
		// all children should created their own rts anyway
		if (is_estimation_finished) {
			if (max_finish > e_time.mpi.in + event_parts.prolog || no_fabric) {
				event_parts.stat.overlap = e_time.mpi.in + event_parts.prolog - min_start;
				if (!no_fabric) event_parts.meat = fabric;
				if (event_parts.meat < 0) {
					pet_throw_error(pet_core, this, "Negative fabric time.\n");
				}
				if (event_parts.stat.overlap < 0) event_parts.stat.overlap = 0;
			} else {
				pet_throw_error(pet_core, this, "Transmission before start???.\n");
			}

			concatenate_event();
			submit(e_time.func.out, EET_FUNC_END, this, this, NULL);
			finish_calculations = true;
		}
	} else {
		concatenate_event();
		submit(e_time.func.out, EET_FUNC_END, this, this, NULL);
		finish_calculations = true;
	}
#endif
    PEAT_Clock max_finish = 1;
    PEAT_Time max_time;
    PEAT_Time_Parts max_parts;
    for (std::map<PEAT_MPI_Request, PET_ToR_Event *>::iterator event_it = tor_events.begin(); event_it != tor_events.end(); ++event_it) {
        PET_ToR_Event *evt = event_it->second;
        if (!evt->is_estimated())
            return true;
        if (evt->e_time.func.out > max_finish) {
            max_time = evt->e_time;
            max_parts = evt->event_parts;
            max_finish = evt->e_time.func.out;
        }
    }
    for (std::map<PEAT_MPI_Request, PET_Collective_Event *>::iterator event_it = collective_events.begin(); event_it != collective_events.end(); ++event_it) {
        PET_Collective_Event *evt = event_it->second;
        if (!evt->is_estimated())
            return true;
        if (evt->e_time.func.out > max_finish) {
            max_time = evt->e_time;
            max_parts = evt->event_parts;
            max_finish = evt->e_time.func.out;
        }
    }
    if (!finish_calculations) {
        // sub_t_ae = e_time.func.out - max_time.func.out;
        // sub_t_ae_t += sub_t_ae;
        // e_time = max_time;
        event_parts = max_parts;
        concatenate_event();
        submit(e_time.func.out, EET_FUNC_END, this, this, NULL);
        finish_calculations = true;
    }
    return true;
}

PEAT_Message_Size PET_Start_Event::get_tx_bytes() const
{
    PEAT_Message_Size tx_bytes = -1;
    for (std::map<PEAT_MPI_Request, PET_ToR_Event *>::const_iterator event_it = tor_events.cbegin(); event_it != tor_events.cend(); ++event_it) {
        PET_ToR_Event *evt = event_it->second;
        if (evt->check_real_tx()) {
            if (tx_bytes == -1)
                tx_bytes = 0;
            PEAT_Message_Size size = evt->get_tx_bytes();
            if (size < 0)
                pet_throw_error(pet_core, this, NULL, "Negative number of TX bytes found");
            tx_bytes += size;
        }
    }
    for (std::map<PEAT_MPI_Request, PET_Collective_Event *>::const_iterator event_it = collective_events.cbegin(); event_it != collective_events.cend(); ++event_it) {
        PET_Collective_Event *evt = event_it->second;
        if (evt->check_real_tx()) {
            if (tx_bytes == -1)
                tx_bytes = 0;
            PEAT_Message_Size size = evt->get_tx_bytes();
            if (size < 0)
                pet_throw_error(pet_core, this, NULL, "Negative number of TX bytes found");
            tx_bytes += size;
        }
    }
    return tx_bytes;
}
PEAT_Message_Size PET_Start_Event::get_rx_bytes() const
{
    PEAT_Message_Size rx_bytes = -1;
    for (std::map<PEAT_MPI_Request, PET_ToR_Event *>::const_iterator event_it = tor_events.cbegin(); event_it != tor_events.cend(); ++event_it) {
        PET_ToR_Event *evt = event_it->second;
        if (evt->check_real_rx()) {
            if (rx_bytes == -1)
                rx_bytes = 0;
            PEAT_Message_Size size = evt->get_rx_bytes();
            if (size < 0)
                pet_throw_error(pet_core, this, NULL, "Negative number of RX bytes found");
            rx_bytes += size;
        }
    }
    for (std::map<PEAT_MPI_Request, PET_Collective_Event *>::const_iterator event_it = collective_events.cbegin(); event_it != collective_events.cend(); ++event_it) {
        PET_Collective_Event *evt = event_it->second;
        if (evt->check_real_rx()) {
            if (rx_bytes == -1)
                rx_bytes = 0;
            PEAT_Message_Size size = evt->get_rx_bytes();
            if (size < 0)
                pet_throw_error(pet_core, this, NULL, "Negative number of RX bytes found");
            rx_bytes += size;
        }
    }
    return rx_bytes;
}

PEAT_Message_Size PET_Start_Event::get_real_tx_bytes() const
{
    return get_tx_bytes();
}
PEAT_Message_Size PET_Start_Event::get_real_rx_bytes() const
{
    return get_rx_bytes();
}

PEAT_Bool PET_Start_Event::check_real_tx() const
{
    for (std::map<PEAT_MPI_Request, PET_ToR_Event *>::const_iterator event_it = tor_events.cbegin(); event_it != tor_events.cend(); ++event_it) {
        PET_ToR_Event *evt = event_it->second;
        if (evt->check_real_tx())
            return true;
    }
    for (std::map<PEAT_MPI_Request, PET_Collective_Event *>::const_iterator event_it = collective_events.cbegin(); event_it != collective_events.cend(); ++event_it) {
        PET_Collective_Event *evt = event_it->second;
        if (evt->check_real_tx())
            return true;
    }
    return false;
}
PEAT_Bool PET_Start_Event::check_real_rx() const
{
    for (std::map<PEAT_MPI_Request, PET_ToR_Event *>::const_iterator event_it = tor_events.cbegin(); event_it != tor_events.cend(); ++event_it) {
        PET_ToR_Event *evt = event_it->second;
        if (evt->check_real_rx())
            return true;
    }
    for (std::map<PEAT_MPI_Request, PET_Collective_Event *>::const_iterator event_it = collective_events.cbegin(); event_it != collective_events.cend(); ++event_it) {
        PET_Collective_Event *evt = event_it->second;
        if (evt->check_real_rx())
            return true;
    }
    return false;
}
PEAT_Bool PET_Start_Event::check_tx() const
{
    for (std::map<PEAT_MPI_Request, PET_ToR_Event *>::const_iterator event_it = tor_events.cbegin(); event_it != tor_events.cend(); ++event_it) {
        PET_ToR_Event *evt = event_it->second;
        if (evt->check_tx())
            return true;
    }
    for (std::map<PEAT_MPI_Request, PET_Collective_Event *>::const_iterator event_it = collective_events.cbegin(); event_it != collective_events.cend(); ++event_it) {
        PET_Collective_Event *evt = event_it->second;
        if (evt->check_tx())
            return true;
    }
    return false;
}
PEAT_Bool PET_Start_Event::check_rx() const
{
    for (std::map<PEAT_MPI_Request, PET_ToR_Event *>::const_iterator event_it = tor_events.cbegin(); event_it != tor_events.cend(); ++event_it) {
        PET_ToR_Event *evt = event_it->second;
        if (evt->check_rx())
            return true;
    }
    for (std::map<PEAT_MPI_Request, PET_Collective_Event *>::const_iterator event_it = collective_events.cbegin(); event_it != collective_events.cend(); ++event_it) {
        PET_Collective_Event *evt = event_it->second;
        if (evt->check_rx())
            return true;
    }
    return false;
}
PEAT_Bool PET_Start_Event::get_bounds()
{
    PEAT_Signed_Num pe_type = get_core()->get_pe_type();
    for (std::list<PET_Fabric_Event>::const_iterator fab_event_it = fabric_events.cbegin(); fab_event_it != fabric_events.cend(); ++fab_event_it) {
        if ((fab_event_it->is_any_tx() && (check_tx() || pe_type == 2)) || (fab_event_it->is_any_rx() && (check_rx() || pe_type == 2))) {
            PEAT_Clock d2 = ut_time.mpi.out - fab_event_it->ut_time.mpi.out;
            assert(d2 >= 0);
            if (t_time.pe.in <= 0) {
                PEAT_Clock d = fab_event_it->ut_time.mpi.in - ut_time.mpi.in;
                assert(d >= 0);
                t_time.pe.in = t_time.func.in + d;
                e_time.pe.in = e_time.func.in + d;
            }
            t_time.pe.out = t_time.func.out - d2;
            e_time.pe.out = e_time.func.out - d2;
        }
    }
#ifdef _DEBUG
    PEAT_Clock dur = e_time.pe.duration();
#endif
    return true;
}

PEAT_Bool PET_Start_Event::split_event()
{
    /*
    PEAT_Time_Parts *core_suggestion = get_core()->get_func_split(this);
    if (core_suggestion) {
        event_parts = *core_suggestion;
    } else {
        if (e_time.pe.in > 0) {
            event_parts.prolog = e_time.pe.in - e_time.mpi.in;
            event_parts.epilog = e_time.mpi.out - e_time.pe.out;
        } else {
            PET_Coll_Event::split_event();
        }
    }
    event_parts.meat = e_time.mpi.duration() - event_parts.prolog - event_parts.epilog;
    o_event_parts = event_parts;
    o_event_parts.stat.fabric = o_event_parts.meat;
    //assert(e_time.mpi.duration() == event_parts.duration());
    //assert(event_parts.meat >= 0);
    if (event_parts.meat < 0) {
        pet_throw_error(pet_core,"Fabric Time is negative.\n");
    }
    if (e_time.mpi.duration() != event_parts.duration()) {
        pet_throw_error(pet_core,"Durations don't match.\n");
    }
    */
    if (event_parts.duration() > 0)
        return true;
    PET_Local_Event::split_event();
    int size = (int)p_events.size();
    for (int i = 0; i < size; i++) {
        p_events.at(i)->fabric_events = fabric_events;
        p_events.at(i)->uc_fabric_events = uc_fabric_events;
        p_events.at(i)->e_time = e_time;
        p_events.at(i)->t_time = t_time;
        p_events.at(i)->o_time = o_time;
        p_events.at(i)->ut_time = ut_time;
        p_events.at(i)->split_event();
    }
    return true;
}
void PET_Start_Event::notify_before_destroy(PET_Event *event)
{
    PET_Coll_Event::notify_before_destroy(event);
    if (shutdown_initiated)
        return; // skip all below since this is the curent object who is being destroyed
    for (std::vector<PET_Event *>::iterator p_it = p_events.begin(); p_it != p_events.end();) {
        if (*p_it == event)
            p_it = p_events.erase(p_it); // the destructor walks p_events, so no dangling entries
        else
            ++p_it;
    }
    switch (event->get_event_type()) {
    case PET_EVENT_TYPE_TOR: {
        PET_ToR_Event *tor_evt = static_cast<PET_ToR_Event *>(event);
        tor_events.erase(tor_evt->request);
        break;
    }
    case PET_EVENT_TYPE_COLLECTIVE: {
        PET_Collective_Event *coll_evt = static_cast<PET_Collective_Event *>(event);
        collective_events.erase(coll_evt->request);
        break;
    }
    default:
        pet_throw_error(get_core(), this, NULL, "Unknown event type.\n");
        break;
    }
    PET_Coll_Event::notify_before_destroy(event);
}
