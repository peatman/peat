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
PET_Coll_Event::~PET_Coll_Event()
{
    // t_events is nested_events: the records are deleted by PET_Event::~PET_Event
}

PEAT_Bool PET_Coll_Event::is_assembled() const
{
    std::list<Coll_Storage>::const_iterator collective;
    if (get_core()->get_coll_const(this, collective)) {
        return (collective->assembled);
    }
    return false;
}
PEAT_Bool PET_Coll_Event::is_bound() const
{
    const std::vector<PET_Event *> &kids = child_iteration_list(); // includes nested records
    int size = (int)kids.size();
    for (int i = 0; i < size; i++) {
        if (kids[i] == NULL || !kids[i]->is_bound()) {
            return false;
        }
    }
    return true;
}
PEAT_Bool PET_Coll_Event::safe_to_delete()
{
    PEAT_Bool is_safe = PET_Local_Event::safe_to_delete();
    std::list<Coll_Storage>::const_iterator collective;
    if (get_core()->get_coll_const(this, collective)) {
        for (std::map<PEAT_Rank, PET_Coll_Event *>::const_iterator coll_it = collective->events.cbegin(); coll_it != collective->events.cend(); ++coll_it) {
            is_safe = is_safe && coll_it->second->is_written;
        }
    } else {
        return is_safe; // this collective has already been destroyed
    }
    return is_safe;
}
PEAT_Bool PET_Coll_Event::can_be_destroyed()
{
    const std::vector<PET_Event *> &kids = child_iteration_list();
    PEAT_Num size = kids.size();
    for (PEAT_Num i = 0; i < size; i++) {
        if (kids[i] && kids[i]->can_be_destroyed() == false) {
            return false;
        }
    }
    return true;
}

PEAT_Bool PET_Coll_Event::bind()
{
    // if (is_bound() && is_assembled()) return true;
    const std::vector<PET_Event *> &kids = child_iteration_list();
    PEAT_Num size = kids.size();
    for (PEAT_Num i = 0; i < size; i++) {
        if (kids[i] == NULL || !kids[i]->bind()) {
            return false;
        }
    }
    return is_bound() && is_assembled();
}

PEAT_Bool PET_Coll_Event::ready_to_estimate()
{
    if (!is_assembled())
        return false;
    if (!is_bound())
        return false;
    if (!PET_Local_Event::ready_to_estimate())
        return false;
    std::list<Coll_Storage>::iterator collective;
    if (get_core()->get_coll(this, collective)) {
        // check other members
        for (std::map<PEAT_Rank, PET_Coll_Event *>::iterator coll_it = collective->events.begin(); coll_it != collective->events.end(); ++coll_it) {
            if (!coll_it->second->is_bound()) {
                if (!coll_it->second->bind()) {
                    return false; // binding
                }
            }
            //			if (!coll_it->second->PET_Event::ready_to_estimate()) return false; //1st in line why?
        }
    }
    const std::vector<PET_Event *> &kids = child_iteration_list();
    PEAT_Num size = kids.size();
    for (PEAT_Num i = 0; i < size; i++) {
        if (kids[i] == NULL || !kids[i]->ready_to_estimate())
            return false;
    }
    return true;
}

PEAT_Bool PET_Coll_Event::is_estimated() const
{
    return PET_Local_Event::is_estimated();
}

void PET_Coll_Event::scale_event()
{
    PET_Event::scale_event();
    get_core()->handle_coll(this);
}

PET_Event *PET_Coll_Event::write_event(File_Storage_GZ *gz_MPI, File_Storage_GZ *gz_HWC, File_Storage_GZ *gz_bind, const char *mpi_eol, const char *hwc_eol)
{
    write_nested_records(gz_MPI, gz_HWC, gz_bind, mpi_eol, hwc_eol); // trace order, nested records included
    return this;
}

/* child iteration walks every colt record (nested ones included) in trace order */
PET_Event *PET_Coll_Event::get_first_child_event()
{
    const std::vector<PET_Event *> &kids = child_iteration_list();
    if (kids.empty())
        return this;
    int_event_index = 0;
    if (int_event_index >= kids.size() || kids.at(int_event_index) == NULL)
        return this;
    return kids.at(int_event_index);
}

PET_Event *PET_Coll_Event::get_last_child_event()
{
    const std::vector<PET_Event *> &kids = child_iteration_list();
    if (kids.empty())
        return this;
    int_event_index = kids.size() - 1;
    if (int_event_index >= kids.size() || kids.at(int_event_index) == NULL)
        return this;
    return kids.at(int_event_index);
}

PET_Event *PET_Coll_Event::get_next_child_event()
{
    const std::vector<PET_Event *> &kids = child_iteration_list();
    if (kids.empty())
        return this;
    int_event_index++;
    if (int_event_index >= kids.size() || kids.at(int_event_index) == NULL)
        return this;
    return kids.at(int_event_index);
}

PET_Event *PET_Coll_Event::get_prev_child_event()
{
    const std::vector<PET_Event *> &kids = child_iteration_list();
    int_event_index--;
    if (int_event_index >= kids.size() || kids.at(int_event_index) == NULL)
        return this;
    return kids.at(int_event_index);
}
PET_Event *PET_Event::get_prev_child_event(PET_Event *event)
{
    /* kept as it always behaved: the loop never matches (it compares the host, not the child),
       so the predecessor of a nested record is the event before its top-level call */
    PEAT_Signed_Num size = (PEAT_Signed_Num)nested_events.size() - 1;
    for (PEAT_Signed_Num event_index = size; event_index > 0; event_index--) {
        if (nested_events.at(event_index) == this && size > 0)
            return nested_events.at(event_index - 1);
    }
    return get_core()->get_prev_event(this, false);
}

PEAT_Rank PET_Coll_Event::get_rank_read_suggestion()
{
    std::list<Coll_Storage>::const_iterator collective;
    if (get_core()->get_coll_const(this, collective)) {
        if (collective->expected_ranks.empty()) {
            for (std::map<PEAT_Rank, PET_Coll_Event *>::const_iterator event_it = collective->events.cbegin(); event_it != collective->events.cend(); ++event_it) {
                if (!event_it->second->ready_to_bind())
                    return event_it->first;
            }
            const std::vector<PET_Event *> &kids = child_iteration_list();
            PEAT_Num size = (PEAT_Num)kids.size();
            for (PEAT_Num i = 0; i < size; i++) {
                if (kids[i] && !kids[i]->is_bound())
                    return kids[i]->get_rank_read_suggestion();
            }
            pet_warning(get_core(), this, "Unable to select rank to read.\n");
            return -1;
        };
        return (collective->expected_ranks.begin()->first);
    } else {
        const std::vector<PET_Event *> &kids = child_iteration_list();
        PEAT_Num size = (PEAT_Num)kids.size();
        for (PEAT_Num i = 0; i < size; i++) {
            if (kids[i] && !kids[i]->is_bound())
                return kids[i]->get_rank_read_suggestion();
        }
        pet_warning(get_core(), this, "Unable to select rank to read.\n");
        return -1;
    }
}

PEAT_Bool PET_Coll_Event::split_event()
{
    if (event_parts.duration() > 0)
        return true;
    int size = (int)t_events.size();
    if (size > 0) {
        const std::vector<PET_Event *> &kids = child_iteration_list();
        int all_size = (int)kids.size();
        for (int i = 0; i < all_size; i++) {
            kids[i]->split_event(); // nested records included
        }
        event_parts.prolog = t_events[0]->e_time.func.in - e_time.func.in;
        event_parts.epilog = e_time.func.out - t_events[size - 1]->e_time.func.out;
        // TODO get better approxiamtion
        event_parts.meat = t_events[size - 1]->e_time.func.out - t_events[0]->e_time.func.in;
        o_event_parts = event_parts;
    } else {
        return PET_Local_Event::split_event();
    }
    o_time.pe.in = o_time.mpi.in + o_event_parts.prolog;
    o_time.pe.out = o_time.mpi.out - o_event_parts.epilog;

    return true;
}
void PET_Coll_Event::concatenate_event()
{
    if (!t_events.empty()) {
        int size = (int)t_events.size();
        // e_time.func.in = e_time.mpi.in = entry->e_time.mpi.in;
        e_time.func.out = e_time.mpi.out = t_events[size - 1]->e_time.func.out + event_parts.epilog;
        event_parts.meat = t_events[size - 1]->e_time.func.out - t_events[0]->e_time.func.in;
    }
    PET_Local_Event::concatenate_event();
    sub_t_ae = o_event_parts.duration() - event_parts.duration();
    sub_t_ae_t += sub_t_ae;
    return;
}
PEAT_Clock PET_Coll_Event::scale_cpu_part(PEAT_Ratio ratio)
{
    PEAT_Clock total_adj = PET_Event::scale_cpu_part(ratio);
    PEAT_Clock first_adj = o_event_parts.prolog - event_parts.prolog;
    int size = (int)t_events.size();
    for (int j = 0; j < size; j++) {
        t_events[j]->move_event(first_adj);
    }
    for (int i = 0; i < size; i++) {
        PEAT_Clock cpu_code_bitween_calls = i > 0 ? t_events[i]->e_time.mpi.in - t_events[i - 1]->e_time.mpi.out : 0;
        PEAT_Clock cpu_code_adj = cpu_code_bitween_calls - (PEAT_Clock)(cpu_code_bitween_calls * ratio);
        // t_events[i]->move_event(cpu_code_adj);
        PEAT_Clock adj_t = t_events[i]->scale_cpu_part(ratio);
        adj_t += cpu_code_adj;
        for (int j = i; j < size; j++) {
            t_events[j]->move_event(cpu_code_adj);
        }
        event_parts.meat -= adj_t;
        total_adj += adj_t;
    }
    return total_adj;
};
PEAT_Bool PET_Coll_Event::estimate(Estimation_Event *event)
{
    if (t_events.empty() && get_event_type() != PET_EVENT_TYPE_COMPLETION) {
        return PET_Local_Event::estimate(event);
    } else {
        PEAT_Num size = t_events.size();
        /*if ( event == NULL ) { // just pass it down
            if ( index_event_being_estimated >= 0 ) {
                return t_events[index_event_being_estimated]->estimate(NULL);
            } else {
                return false;
            }
        }*/
        switch (event->type) {
        case EET_FUNC_START:
            if (is_estimated()) {
                pet_throw_error(pet_core, this, event, "Repeated Estimation Attempt.\n");
            }
            estimation_started = true;
            split_event();
            if (size > 0) {
                initiate_nested(); // the records run one after another, see handle_nested_completed()
            } else {
                if (get_core()->get_mo()) {
                    // concatenate_event();
                    finish_calculations = true;
                    submit(e_time.func.out, EET_FUNC_END, this, this, NULL);
                }
            }
            break;
        case EET_FUNC_END:
            if (!estimation_started) {
                pet_throw_error(pet_core, this, event, "Function is not estimated.\n");
            }
            set_estimated();
            put_transport_event_stat();
            break;
        case EET_PROGRESS_NOTIFY:
            if (!finish_calculations && estimation_started) {
                handle_progress_messages(__max__(e_time.mpi.in + event_parts.prolog, event->start), false);
            }
            break;
        case EET_CHILD_COMPLETED:
            if (!estimation_started) {
                pet_throw_error(pet_core, this, event, "Estimation not started.\n");
            }
            // moves the following records and starts the next one; when the last one is done the call ends
            if (handle_nested_completed(event) && nested_index_being_estimated >= (PEAT_Signed_Num)size) {
                if (get_event_type() != PET_EVENT_TYPE_COMPLETION) { // completion events will handle by themselves
                    concatenate_event();
                    submit(e_time.func.out, EET_FUNC_END, this, this, NULL);
                }
            }
            break;
        case EET_NOTIFY:
            break;
        default:
            pet_throw_error(pet_core, this, event, "Inappropriate  Code.\n");
            break;
        }
        PET_Event *current_event = get_current_event();
        if (current_event != NULL && current_event != this && current_event->host_event != this) {
            submit(event->start, EET_NOTIFY, current_event, this, NULL);
        }
        if (host_event && event->type != EET_FUNC_END) {
            submit(event->start, EET_NOTIFY, host_event, this, NULL);
        }
        if (parent_event && parent_event != host_event && parent_event != current_event) {
            submit(event->start, EET_NOTIFY, parent_event, this, NULL);
        }

        get_core()->delete_ee_entry(event);
    }
    return true;
}
PEAT_Clock PET_Coll_Event::get_centre_code(Event_Set_Stat::t_stat_type t)
{
    int size = (int)t_events.size();
    PEAT_Clock centre_code = 0;
    switch (t) {
    case Event_Set_Stat::tst_orig:
        for (int i = 0; i < size - 1; i++) {
            centre_code += t_events[i + 1]->o_time.func.in - t_events[i]->o_time.func.out;
        }
        break;
    case Event_Set_Stat::tst_trc:
        for (int i = 0; i < size - 1; i++) {
            centre_code += t_events[i + 1]->t_time.func.in - t_events[i]->t_time.func.out;
        }
        break;
    case Event_Set_Stat::tst_post:
        for (int i = 0; i < size - 1; i++) {
            centre_code += t_events[i + 1]->e_time.func.in - t_events[i]->e_time.func.out;
        }
        break;
    }
    if (size > 1)
        centre_code /= size;
    return centre_code;
}
void PET_Coll_Event::post_read_event()
{
    // after build_nesting() t_events holds depth 0 records only, so an overlap here is a partial one
    // (neither record contains the other), which a single thread cannot produce: clip and warn
    int size = (int)t_events.size();
    if (size > 1) {
        for (int i = size - 1; i > 0; i--) {
            if (t_events[i - 1]->t_time.func.out > t_events[i]->t_time.func.in) { // correct overlapped events
                pet_warning(get_core(), this, "Partially overlapped collective transfer records detected\n");
                t_events[i - 1]->t_time.func.out = t_events[i]->t_time.func.in;
                t_events[i - 1]->e_time.func.out = t_events[i]->e_time.func.in;
                t_events[i - 1]->t_time.mpi.out = t_events[i]->t_time.mpi.in;
                t_events[i - 1]->e_time.mpi.out = t_events[i]->e_time.mpi.in;
                t_events[i - 1]->ut_time.func.out = t_events[i]->ut_time.func.in;
                // ut_time.mpi.out ?
            }
        }
    }
}
void PET_Coll_Event::notify_before_destroy(PET_Event *event)
{
    PET_Event::notify_before_destroy(event); // drops it from nested_events and flat_events
}

static bool colt_before(const PET_Event *a, const PET_Event *b)
{
    // earlier start first; on equal start the longer (containing) record first
    if (a->e_time.func.in != b->e_time.func.in)
        return a->e_time.func.in < b->e_time.func.in;
    return a->e_time.func.out > b->e_time.func.out;
}

void PET_Coll_Event::build_nesting()
{
    /* Colt records come in one flat list per MPI call (trace order), but a blocking colt call spins the
       progress engine, which issues further traced colt calls on the same thread: those lie inside the
       time interval of the spinning call. Re-parent by interval inclusion: a record contained in an earlier
       one becomes a nested record of the tightest such container instead of this call. t_events (this call's
       nested_events) ends up in time order; flat_events keeps the original list for iteration and output. */
    PEAT_Num size = t_events.size();
    if (size == 0)
        return;
    for (PEAT_Num i = 0; i < size; i++) {
        if (t_events[i] == NULL) {
            pet_throw_error(get_core(), this, NULL, "Collective transfer record is missing.\n");
            return;
        }
    }
    flat_events = t_events;
    if (size < 2)
        return;
    std::vector<PET_Event *> order(t_events);
    std::stable_sort(order.begin(), order.end(), colt_before);
    std::vector<PET_Event *> stack, depth0;
    PEAT_Num nested_num = 0;
    for (PEAT_Num i = 0; i < size; i++) {
        PET_Event *evt = order[i];
        while (!stack.empty()) {
            PET_Event *top = stack.back();
            if (evt->e_time.func.in >= top->e_time.func.in && evt->e_time.func.out <= top->e_time.func.out)
                break; // contained in top
            if (evt->e_time.func.in < top->e_time.func.out) {
                pet_warning(get_core(), this, "Partially overlapped collective transfer records, nesting is ambiguous\n");
            }
            stack.pop_back();
        }
        if (stack.empty()) {
            depth0.push_back(evt); // stays with this call (container_event == this)
        } else {
            PET_Event *container = stack.back();
            evt->container_event = container;
            container->nested_events.push_back(evt); // time order thanks to the sort
            nested_num++;
        }
        stack.push_back(evt);
    }
    if (nested_num > 0) {
        t_events = depth0;
    } else {
        t_events = order; // no nesting: still make the chain time ordered (containers-at-exit convention of the tracer)
    }
    if (get_core()->is_verbose() && nested_num > 0) {
        fprintf(stderr, "Rank %d %s(%lld): %lld collective transfer records, %lld of them nested:",
                rank, global_func_id.name_by_idx(func_id), ids[0], (long long)size, (long long)nested_num);
        for (PEAT_Num i = 0; i < t_events.size(); i++) {
            if (t_events[i]->has_nested()) {
                fprintf(stderr, " %s(%s)<-%lld", global_func_id.name_by_idx(t_events[i]->func_id), t_events[i]->ids_str().c_str(), (long long)t_events[i]->nested_count_total());
            }
        }
        fprintf(stderr, "\n");
    }
}
