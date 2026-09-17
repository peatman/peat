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

PEAT_Bool is_wild_card(PET_Event *evt)
{
    return (evt->get_recv_tag() == evt->get_core()->get_comm()->get_MPI_ANY_TAG() || evt->get_src() == evt->get_core()->get_comm()->get_MPI_ANY_SOURCE()) && !evt->is_persistent_init(); // exclude already estimated (persistent inits) since they're locals and don't affect binding
}
PEAT_Bool check_peer(PET_Event *send, PET_Event *recv)
{
    if (!send->check_real_tx())
        return false;
    if (!recv->check_real_rx())
        return false;
    if (send->get_tx_paired())
        return false; // already paired
    if (recv->get_rx_paired())
        return false; // already paired
    if (send->is_mpic() != recv->is_mpic())
        return false;
    if (send->get_comm() != recv->get_comm())
        return false;
    if (send->get_send_tag() != recv->get_recv_tag())
        return false;
    if (send->get_dest() != recv->rank)
        return false;
    if (send->rank != recv->get_src())
        return false;
    if (send->get_mpi_message() != recv->get_mpi_message())
        return false;
    /*
    if ( send->get_peer_for_send() != recv && send->get_peer_for_send() != NULL ) {
        assert(0);
        return false; //already paired with someone else
    }
    if ( recv->get_peer_for_recv() != send && recv->get_peer_for_recv() != NULL ) {
        assert(0);
        return false; //already paired with someone else
    }*/
    return true;
}

PET_Event::PET_Event(PET_Core *p) : PET_Time_Event(), pet_core(p), pet_node(NULL), pet_rank(NULL), pet_thread(NULL),
                                    func_id(TMPI_FUNC_ID_NONE), parent_func_id(TMPI_FUNC_ID_NONE),
                                    primary_id(-1), secondary_id(-1), rank(-1), node(-1), thread_id(-1),
                                    is_user_code_applied(false), estimation_started(false), is_written(false), is_initiated(false),
                                    pending_reason(pr_event_not_estimated), addrs(p->model_id.num_addrs),
                                    parent_event(NULL), host_event(NULL), parallel_section_begin(NULL), prev_event(NULL), next_event(NULL),
                                    sub_t_applied(false), sub_t_ae(0), sub_t_t(0), sub_t_ae_t(0), sub_t_t_t(0),
                                    uc_length(0), uco_length(0), finish_calculations(false), remaining_ee_events(0)
{
}

PET_Event *PET_Event::get_current_event()
{
    return get_thread()->event_being_estimated;
}

PET_Event::~PET_Event()
{
#ifdef _DEBUG_DESTROYED_EVENT
    PET_Event_Definition qple(rank, primary_id, secondary_id, thread_id);
    if (get_core()->deleted_events.count(qple) > 0 && primary_id >= 0) {
        char str[1024];
        sprintf(str, "Duplicated delete attempt. This func %s,previous func %s\n",
                global_func_id.name_by_idx(func_id), global_func_id.name_by_idx(get_core()->deleted_events.at(qple)));
        pet_throw_error(get_core(), this, NULL, str);
    } else {
        if (primary_id >= 0)
            get_core()->deleted_events[qple] = func_id;
    }
#endif // _DEBUG
}
void PET_Event::assign_cache_pointers()
{
    pet_node = get_core()->get_pet_node(this); // also impicitly sets node
    pet_rank = get_core()->get_pet_rank(this);
    pet_thread = get_core()->get_pet_thread(this);
}

void PET_Event::insert()
{
    assign_parallel_section_begin(get_thread()->get_parallel_section());
    get_core()->insert_entry(this, false);
};
void PET_Event::concatenate_event()
{
    e_time.func.in = e_time.mpi.in;
    e_time.mpi.out = e_time.func.out = e_time.mpi.in + event_parts.prolog +
                                       event_parts.meat + event_parts.epilog;
    e_time.pe.in = e_time.mpi.in + event_parts.prolog;
    e_time.pe.out = e_time.mpi.out - event_parts.epilog;
    sub_t_ae = o_event_parts.duration() - event_parts.duration();
    sub_t_ae_t += sub_t_ae;

    if (parent_event == NULL && host_event == NULL && func_id != OMPT_PARALLEL_END_ID) {
        get_core()->update_active_event_chain(sub_t_ae, this);
    }
}
PEAT_Bool PET_Event::read_event(PEAT_Rank r_rank, const String_Split &mpi_string, const String_Split &hwc_string, PET_Coll_Event *host)
{
    // basic info
    rank = r_rank;
    if (hwc_string.size() < 2)
        return false;
    extract(primary_id, hwc_string.at(0), 10);
    extract(secondary_id, hwc_string.at(1), 10);
    extract(thread_id, hwc_string.at(2), 10);
    // TSCs block
    extract(e_time.func.in, hwc_string.at(3), 10);
    extract(e_time.mpi.in, hwc_string.at(4), 10);
    extract(e_time.mpi.out, hwc_string.at(5), 10);
    extract(e_time.func.out, hwc_string.at(6), 10);
    /* clutch for ompt */
    if (e_time.mpi.in < 0) {
        e_time.mpi.in = 0;
    }
    if (e_time.mpi.out < e_time.mpi.in) {
        e_time.mpi.out = e_time.mpi.in;
    }
    if (e_time.func.out < e_time.mpi.out) {
        e_time.func.out = e_time.mpi.out;
    }

    if (get_core()->model_id.num_tscs == 6) {
        extract(e_time.pe.in, hwc_string.at(7), 10);
        extract(e_time.pe.out, hwc_string.at(8), 10);
    }
    for (int i = 0; i < (int)get_core()->model_id.num_addrs; i++) {
        PEAT_UID addr;
        extract(addr, hwc_string.at(3 + get_core()->model_id.num_tscs + get_core()->model_id.num_hwcs) + i);
#ifdef TRANSLATE_UIDS
        addrs.addr[i] = get_core()->get_addr_by_id(rank, addr);
#else
        addrs.addr[i] = addr;
#endif
    }
    assign_cache_pointers();
    return true;
}

PET_Event *PET_Event::write_event(File_Storage_GZ *gz_MPI, File_Storage_GZ *gz_HWC, File_Storage_GZ *gz_bind, const char *mpi_eol, const char *hwc_eol)
{
#ifdef REMOVE_T_OVERHEAD
    gz_HWC->printf(rank, "%lld:%lld:%d:%lld:%lld:%lld:%lld", primary_id, secondary_id, thread_id,
                   e_time.mpi.in,
                   e_time.mpi.out,
                   e_time.pe.in,
                   e_time.pe.out);
#else
    gz_HWC->printf(rank, "%lld:%lld:%d:%lld:%lld:%lld:%lld:%lld:%lld", primary_id, secondary_id, thread_id,
                   e_time.func.in,
                   e_time.mpi.in,
                   e_time.mpi.out,
                   e_time.func.out,
                   e_time.pe.in,
                   e_time.pe.out);
#endif
    for (int i = 0; i < (int)get_core()->model_id.num_addrs; i++) {
        gz_HWC->printf(rank, ":%lx", addrs.addr[i]);
    }
#if 1 // def _DEBUG
    /*
    PEAT_Clock total_uc_o = get_core()->stat.get_stat(Event_Set_Stat::tst_orig)[rank].uncovered_UR_length;
    PEAT_Clock total_uc_m = get_core()->stat.get_stat(Event_Set_Stat::tst_post)[rank].uncovered_UR_length;
    gzprintf(gz_HWC, ";  \t ef=%lld:of=%lld\t st=%lld:se=%lld:stt=%lld:ste=%lld:uo=%lld:ue=%lld:tuo=%lld:tue=%lld%s", event_parts.meat, o_event_parts.meat, sub_t_t, sub_t_ae, sub_t_t_t, sub_t_ae_t,uco_length, uc_length,
             total_uc_o, total_uc_m,hwc_eol);
    */
    PEAT_Clock head = t_time.func.in - e_time.func.in;
    PEAT_Clock tail = t_time.func.out - e_time.func.out;
    PEAT_Ratio r = (PEAT_Ratio)o_event_parts.meat / (PEAT_Ratio)event_parts.meat;
    PEAT_Ratio r_t = (PEAT_Ratio)o_event_parts.duration() / (PEAT_Ratio)event_parts.duration();
    gz_HWC->printf(rank, ";  \t ef=%lld:of=%lld:r=%lf:%lf\t st=%lld:se=%lld(%lld):head=%lld:tail=%lld", event_parts.meat, o_event_parts.meat, r, r_t, sub_t_t, sub_t_ae, sub_t_ae_t, head, tail);
    gz_HWC->printf(rank, " \t %s", global_func_id.name_by_idx(func_id));
    std::map<std::string, PEAT_LNum> fabric_funcs;
    for (std::list<PET_Fabric_Event>::const_iterator fab_event_it = fabric_events.cbegin(); fab_event_it != fabric_events.cend(); ++fab_event_it) {
        fabric_funcs[global_func_id.name_by_idx(fab_event_it->func_id)] += fab_event_it->l_num_calls;
    }
    for (std::map<std::string, PEAT_LNum>::const_iterator fab_func_it = fabric_funcs.cbegin(); fab_func_it != fabric_funcs.cend(); ++fab_func_it) {
        gz_HWC->printf(rank, " %s(%lld)", fab_func_it->first.c_str(), fab_func_it->second);
    }
    gz_HWC->printf(rank, "%s", hwc_eol);

    if (tail - head != sub_t_ae) {
        pet_throw_error(pet_core, this, NULL, "Moving length mismatch\n");
    }
#else
    gz_HWC->printf(rank, "%s", hwc_eol);
#endif
    get_core()->events_processed[rank]++;
    is_written = true;
    return this;
}

PEAT_Clock PET_Event::get_user_code_length(PET_Event *prev_event)
{
    if (prev_event) {
        PEAT_Clock uc_len = e_time.func.in - prev_event->e_time.func.out;
        // assert(uc_len >= 0);
        if (uc_len < 0)
            uc_len = 0;
        return uc_len;
    }
    return 0;
}

PEAT_Bool PET_Event::query_release()
{
    update_pending();
    return pending_reason == pr_none;
}
PEAT_Bool PET_Event::release()
{
    return query_release();
}

PEAT_Bool PET_Event::update_pending()
{
    if (parent_event == NULL && host_event == NULL) {
        // if (prev_event == NULL) prev_event = get_core()->get_prev_event(this, false);
        PET_Event *p = get_thread()->get_first_event();
        if (p == this) {
            clear_reason(pr_not_on_top);
            return true;
        }
        set_reason(pr_not_on_top);
    }
    return false;
}

PEAT_Bool PET_Event::ready_to_estimate()
{
    if (parent_event)
        return true;
    PET_Event *prev_event;
    if (host_event) {
        prev_event = host_event->get_prev_child_event(this);
    } else {
        prev_event = get_prev_event(false);
    }
    PEAT_Bool response = true;
    if (prev_event) {
        if (prev_event->thread_id == thread_id) {
            response = prev_event->is_estimated(); // prev_event check
        } else {
            assert(prev_event->thread_id == 0);
            response = prev_event->ready_to_estimate() || prev_event->is_estimated(); // prev_event check if from thread 0
        }
    }

    response = response && is_bound();
    if (parallel_section_begin) {
        response = response && (get_parallel_section_end() != NULL);
    }
    return response;
}
PEAT_Bool PET_Event::initiate()
{
    if (is_initiated || is_estimated()) {
        return true;
    }
    // place func start into estmation queue
    submit(e_time.mpi.in, EET_FUNC_START, this, this, NULL);
    is_initiated = true;
    return true;
}

PEAT_Bool PET_Event::estimate(Estimation_Event *event)
{
    // if ( event == NULL ) return;
    switch (event->type) {
    case EET_FUNC_START:
        // assert(is_estimated() == false);
        if (is_estimated()) {
            pet_throw_error(pet_core, this, event, "Repeated Estimation Attempt.\n");
        }
        estimation_started = true;
        if (!get_core()->get_mo()) {
            split_event();
            concatenate_event();
        }
        submit(e_time.func.out, EET_FUNC_END, this, this, NULL);
        break;
    case EET_FUNC_END:
        if (!estimation_started) {
            pet_throw_error(pet_core, this, event, "Function is not estimated.\n");
        }
        // assert(estimation_started);
        set_estimated();
        put_transport_event_stat();
        break;
    case EET_PROGRESS_NOTIFY:
        break;
    case EET_NOTIFY:
        break;
    default:
        pet_throw_error(pet_core, this, event, "Inappropriate  Code.\n");
        // assert(0);
        break;
    }
    get_core()->delete_ee_entry(event);
    return true;
}

void PET_Event::scale_event()
{
    PEAT_Ratio global_freq_ratio, global_cpi_ratio;
    get_core()->get_cpu_diff(global_freq_ratio, global_cpi_ratio);
    scale_cpu_part(global_cpi_ratio);
    /*if (parent_event == NULL && host_event == NULL) {
        get_core()->update_active_event_chain(sub_t_ae, this);
        sub_t_applied = false;//clear this flag
    }*/
};

PET_Event *PET_Event::get_next_event()
{
    if (next_event)
        return next_event;
    PET_Event *evt = get_core()->get_next_event(this);
    return evt;
};

PET_Event *PET_Event::get_prev_event(PEAT_Bool reverse)
{
    if (prev_event)
        return prev_event;
    PET_Event *evt = get_core()->get_prev_event(this, thread_id, reverse, true);
#if 1 // def ASSUME_DIFFERENT_THREAD
    if (thread_id > 0) {
        PET_Event *evt_t0 = get_core()->get_prev_event(this, 0, reverse, true);
        if (evt == NULL) {
            prev_event = NULL;
            return evt_t0;
        }
        if (evt_t0) {
            if (func_id != OMPT_PARALLEL_END_ID && // not closiing parallel section
                get_parallel_section_begin() == NULL &&
                //(get_parallel_section_begin() == NULL || evt->func_id == OMPT_PARALLEL_END_ID) &&
                evt_t0->primary_id > evt->primary_id) {
                prev_event = NULL;
                return evt_t0;
            }
        }
    }
    if (evt)
        if (evt->thread_id == thread_id) {
            prev_event = evt;
            prev_event->next_event = this;
        }
#else
    prev_event = evt;
    if (evt) {
        prev_event->next_event = this;
    }
#endif
    return evt;
};

PEAT_Bool PET_Event::can_be_destroyed()
{
    const PET_Event *last_event = get_core()->get_last_event(this);
    if (last_event == this && !get_core()->is_rank_finished(rank))
        return false;
    if (remaining_ee_events > 0)
        return false;
    return true;
}

PEAT_Clock PET_Event::remove_tractor_overhead(PEAT_Clock *prolog_overhead)
{
    PEAT_Clock prolog = e_time.mpi.in - e_time.func.in, epilog = e_time.func.out - e_time.mpi.out;
    e_time.mpi.move(prolog);
    if (e_time.pe > 0)
        e_time.pe.move(prolog);
    if (prolog_overhead)
        *prolog_overhead = prolog;
    e_time.func.adjust(prolog + epilog);
    sub_t_t = prolog + epilog;
    sub_t_t_t = get_core()->get_tractor_overhead(rank);
    t_time = e_time;
    return prolog + epilog;
}
PEAT_Bool PET_Event::safe_to_delete()
{
    PET_Event *next_event = get_next_event();
    if (next_event) {
        return is_written && next_event->is_written;
    } else {
        return is_written && get_core()->is_rank_finished(rank);
    }
};

PEAT_Bool PET_Event::set_estimated()
{
    clear_reason(pr_event_not_estimated);
#if 0	
	if (parent_event == NULL && host_event == NULL && get_event_type() != PET_EVENT_TYPE_OMPT) {
		get_core()->fill_next_event(this);
	}
#endif
    finish_calculations = true;
    return true;
}
PEAT_Bool PET_Event::clear_estimated()
{
    set_reason(pr_event_not_estimated);
    //	finish_calculations = false;
    return true;
}

PEAT_Bool PET_Event::submit(PEAT_Clock clk_start, Estimation_Event *event)
{
    if (event) {
        event->start = clk_start;
        event->submiter = this;
    }
    get_core()->insert_ee_entry(event);
    return true;
}

PEAT_Bool PET_Event::submit(PEAT_Clock clk_start, Estimation_Event_Type t, PET_Event *owner, PET_Event *submiter, TE_Event *info)
{
    Estimation_Event evt;
    evt.start = clk_start;
    evt.owner = owner;
    evt.type = t;
    evt.submiter = submiter;
    if (info) {
        evt.info = *info;
        evt.info.owner = this; // ugly hack
    }
    get_core()->insert_ee_entry(&evt);
    return true;
}

PEAT_Bool PET_Event::post(PEAT_Clock clk_start, Estimation_Event_Type t, PET_Event *owner, PET_Event *submiter, TE_Event *info)
{
    Estimation_Event evt;
    evt.start = clk_start;
    evt.owner = owner;
    evt.type = t;
    evt.submiter = submiter;
    if (info) {
        evt.info = *info;
        evt.info.owner = this; // ugly hack
    }
    get_core()->insert_progress_ee_entry(&evt);
    return true;
}
PEAT_Bool PET_Event::split_event()
{
    if (event_parts.duration() > 0)
        return true;
    get_core()->build_fabric_events_list(this);
    get_bounds();
    if (e_time.pe.in > 0) {
        if (e_time.pe.in < e_time.mpi.in) {
            char buf[128];
            sprintf(buf, "PE Time (in) is outside function time bounds. (%lld)\n", e_time.mpi.out - e_time.pe.out);
            pet_throw_error(pet_core, this, NULL, buf);
            e_time.pe.in = e_time.mpi.in;
        }
        if (e_time.pe.out > e_time.mpi.out) {
            char buf[128];
            sprintf(buf, "PE Time (out) is outside function time bounds. (%lld)\n", e_time.pe.out - e_time.mpi.out);
            pet_throw_error(pet_core, this, NULL, buf);
            e_time.pe.out = e_time.mpi.out;
        }
        event_parts.prolog = e_time.pe.in - e_time.mpi.in;
        event_parts.epilog = e_time.mpi.out - e_time.pe.out;
        event_parts.meat = e_time.mpi.duration() - event_parts.prolog - event_parts.epilog;
    } else {
        switch (get_core()->globals.part_receiver) {
        case PET_Part_Receiver_PROLOG:
            event_parts.prolog = e_time.mpi.duration();
            event_parts.meat = event_parts.epilog = 0;
            break;
        case PET_Part_Receiver_MEAT:
            event_parts.meat = e_time.mpi.duration();
            event_parts.prolog = event_parts.epilog = 0;
            break;
        case PET_Part_Receiver_EPILOG:
            event_parts.epilog = e_time.mpi.duration();
            event_parts.meat = event_parts.prolog = 0;
            break;
        default:
            pet_throw_error(get_core(), this, NULL, "Wrong Receiver Part.\n");
            break;
        }
    }

    o_event_parts = event_parts;
    o_time.pe.in = o_time.mpi.in + o_event_parts.prolog;
    o_time.pe.out = o_time.mpi.out - o_event_parts.epilog;
    scale_event();
    return true;
}
PEAT_Bool PET_Event::get_bounds()
{
    for (std::list<PET_Fabric_Event>::const_iterator fab_event_it = fabric_events.cbegin(); fab_event_it != fabric_events.cend(); ++fab_event_it) {
        if (fab_event_it->is_any_tx() || fab_event_it->is_any_rx()) {
            PEAT_Clock d = fab_event_it->ut_time.mpi.in - ut_time.mpi.in;
            PEAT_Clock d2 = ut_time.mpi.out - fab_event_it->ut_time.mpi.out;
            if (d >= 0 && d2 >= 0) {
                // assert( d >= 0 );
                // assert( d2 >= 0 );
                if (t_time.pe.in <= 0) {
                    t_time.pe.in = t_time.func.in + d;
                    e_time.pe.in = e_time.func.in + d;
                }
                t_time.pe.out = t_time.func.out - d2;
                e_time.pe.out = e_time.func.out - d2;
            }
        } else {
            pet_warning(pet_core, this, "Unclassified fabric event.\n");
        }
    }
    return true;
}
PEAT_Clock PET_Event::scale_cpu_part(PEAT_Ratio ratio)
{
    PEAT_Clock old_prolog_length = event_parts.prolog;
    PEAT_Clock old_epilog_length = event_parts.epilog;
    PEAT_Clock old_fabric_length = event_parts.meat;

    PEAT_Clock new_prolog_length = (PEAT_Clock)(ratio * old_prolog_length);
    PEAT_Clock new_epilog_length = (PEAT_Clock)(ratio * old_epilog_length);
    PEAT_Clock new_fabric_length = old_fabric_length;
    PEAT_Clock prolog_adjustment = old_prolog_length - new_prolog_length;
    PEAT_Clock fabric_adjustment = old_fabric_length - new_fabric_length;
    PEAT_Clock epilog_adjustment = old_epilog_length - new_epilog_length;
    return event_parts.adjust(prolog_adjustment, fabric_adjustment, epilog_adjustment);
};

PEAT_Bool PET_Event::guess_blocking() const
{
    for (std::list<PET_Fabric_Event>::const_iterator fab_event_it = fabric_events.cbegin(); fab_event_it != fabric_events.cend(); ++fab_event_it) {
        if (fab_event_it->is_send() || fab_event_it->is_wait() || fab_event_it->is_test() || fab_event_it->is_poll() || fab_event_it->is_notify()) {
            return true;
        }
    }
    return false;
}

PET_Event *PET_Event::get_first_child_event()
{
    return this;
};

PET_Event *PET_Event::get_last_child_event()
{
    return this;
};

PET_Event *PET_Event::get_next_child_event()
{
    return this;
}

PET_Event *PET_Event::get_prev_child_event()
{
    return this;
}

PEAT_Num PET_Event::handle_progress_messages(PEAT_Clock clk, PEAT_Bool just_check)
{
    Estimation_Event *evt;
    PEAT_Num num_event_submitted = 0;
    bool can_handle_progress = !(parent_event ? parent_event->is_estimated() : is_estimated());
    if (can_handle_progress) {
        PEAT_Clock &last_progress_notify = get_rank()->last_progress_notify;
        do {
            evt = get_core()->get_progress_ee_event(this);
            if (evt) {
                Estimation_Event tmp_evt = *evt;
                if (evt->start <= clk) {
                    get_core()->delete_progress_ee_entry(evt);
                    evt = &tmp_evt;
                    submit(clk, evt);
                    num_event_submitted++;
                } else {
                    if (num_event_submitted > 0 || last_progress_notify >= evt->start || just_check)
                        break;
                    tmp_evt.type = EET_PROGRESS_NOTIFY;
                    submit(tmp_evt.start, &tmp_evt);
                    last_progress_notify = tmp_evt.start;
                    break;
                };
            }
        } while (evt);
    }
    return num_event_submitted;
}

PEAT_Num PET_Event::ping_progress(Estimation_Event *evt)
{
    PET_Event *current_event = get_current_event();
    if (current_event == NULL) {
        pet_throw_error(get_core(), this, &evt, "No current Event\n");
    } else {
        if (current_event != this) {
            Estimation_Event tmp_evt = *evt;
            tmp_evt.owner = current_event;
            tmp_evt.submiter = this;
            submit(evt->start, &tmp_evt);
        }
    }
    return 0;
}

PEAT_Bool PET_Event::put_transport_event_stat()
{
    PET_Event_Type event_type = get_event_type();
    PEAT_Clock pe_entry = o_time.pe.in;
    PEAT_Clock pe_exit = o_time.pe.out;
    PEAT_Num inter;
    switch (event_type) {
    case PET_EVENT_TYPE_TOR: {
        PET_ToR_Event *tor_event = static_cast<PET_ToR_Event *>(this);
        PEAT_Node peer_node = tor_event->get_core()->get_ptp()->get_tgt_node(tor_event->peer_rank);
        if (node != peer_node) {
            // internode statistic
            inter = 1;
        } else {
            // intranode statistic
            inter = 0;
        }
    } break;
    case PET_EVENT_TYPE_TAR: {
        PET_TaR_Event *tar_event = static_cast<PET_TaR_Event *>(this);
        PEAT_Node peer_node = tar_event->get_core()->get_ptp()->get_tgt_node(tar_event->get_src());
        if (node != peer_node) {
            // internode statistic
            inter = 1;
        } else {
            // intranode statistic
            inter = 0;
        }
    } break;
    case PET_EVENT_TYPE_COLL:
    case PET_EVENT_TYPE_COLLECTIVE:
    case PET_EVENT_TYPE_COMPLETION:
    case PET_EVENT_TYPE_START:
        inter = 2; // mixed
        break;
    default:
        return false;
    }
    if (pe_entry > 0 && pe_exit > 0) {
        pe_entry -= o_time.mpi.in;
        pe_exit = o_time.mpi.out - pe_exit;
        Function_Environment &func_env_r = get_core()->transport->func_env[inter][func_id];
        func_env_r.entry.fill(pe_entry);
        func_env_r.exit.fill(pe_exit);
    }
    return true;
}

PET_OMPT_Event *PET_Event::get_parallel_section_begin()
{
    return parallel_section_begin;
}

PET_OMPT_Event *PET_Event::get_parallel_section_end()
{
    if (parallel_section_begin == NULL)
        return NULL;
    return parallel_section_begin->get_parallel_section_end();
}
