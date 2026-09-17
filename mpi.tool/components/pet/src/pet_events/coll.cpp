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
    PEAT_Num size = t_events.size();
    if (size > 0) {
        for (PEAT_Num i = 0; i < size; i++) {
            delete t_events[i];
        }
    }
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
    int size = (int)t_events.size();
    for (int i = 0; i < size; i++) {
        if (!t_events[i]->is_bound()) {
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
    PEAT_Num size = t_events.size();
    if (size > 0) {
        for (PEAT_Num i = 0; i < size; i++) {
            if (t_events[i]->can_be_destroyed() == false) {
                return false;
            }
        }
    }
    return true;
}

PEAT_Bool PET_Coll_Event::bind()
{
    // if (is_bound() && is_assembled()) return true;
    PEAT_Num size = t_events.size();
    if (size > 0) {
        for (PEAT_Num i = 0; i < size; i++) {
            if (!t_events[i]->bind()) {
                return false;
            }
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
    PEAT_Num size = t_events.size();
    if (size > 0) {
        for (PEAT_Num i = 0; i < size; i++) {
            if (!t_events[i]->ready_to_estimate())
                return false;
        }
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
    int size = (int)t_events.size();
    if (size > 0) {
        force_write = true;
        for (int i = 0; i < size; i++) {
            t_events[i]->write_event(gz_MPI, gz_HWC, gz_bind, mpi_eol, hwc_eol);
            // get_core()->remove_entry(t_events[i]);
        }
        // remove all events and insert pure "collective" back
        // concatenate_event();
        // PET_Local_Event::insert(); // insert as standalone event
        clear_reason(pr_event_not_estimated);
    }
    return this;
}

PET_Event *PET_Coll_Event::get_first_child_event()
{
    if (t_events.empty())
        return this;
    int_event_index = 0;
    if (int_event_index >= t_events.size() || t_events.at(int_event_index) == NULL)
        return this;
    return t_events.at(int_event_index);
}

PET_Event *PET_Coll_Event::get_last_child_event()
{
    if (t_events.empty())
        return this;
    int_event_index = t_events.size() - 1;
    if (int_event_index >= t_events.size() || t_events.at(int_event_index) == NULL)
        return this;
    return t_events.at(int_event_index);
}

PET_Event *PET_Coll_Event::get_next_child_event()
{
    if (t_events.empty())
        return this;
    int_event_index++;
    if (int_event_index >= t_events.size() || t_events.at(int_event_index) == NULL)
        return this;
    return t_events.at(int_event_index);
}

PET_Event *PET_Coll_Event::get_prev_child_event()
{
    int_event_index--;
    if (int_event_index >= t_events.size() || t_events.at(int_event_index) == NULL)
        return this;
    return t_events.at(int_event_index);
}
PET_Event *PET_Coll_Event::get_prev_child_event(PET_Event *event)
{
    PEAT_Signed_Num size = (PEAT_Signed_Num)t_events.size() - 1;
    for (PEAT_Signed_Num event_index = size; event_index > 0; event_index--) {
        if (t_events.at(event_index) == this && size > 0)
            return t_events.at(event_index - 1);
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
            PEAT_Num size = (PEAT_Num)t_events.size();
            if (size > 0) {
                for (PEAT_Num i = 0; i < size; i++) {
                    if (!t_events[i]->is_bound())
                        return t_events[i]->get_rank_read_suggestion();
                }
            }
            pet_warning(get_core(), this, "Unable to select rank to read.\n");
            return -1;
        };
        return (collective->expected_ranks.begin()->first);
    } else {
        PEAT_Num size = (PEAT_Num)t_events.size();
        if (size > 0) {
            for (PEAT_Num i = 0; i < size; i++) {
                if (!t_events[i]->is_bound())
                    return t_events[i]->get_rank_read_suggestion();
            }
        }
        pet_warning(get_core(), this, "Unable to select rank to read.\n");
        return -1;
    }
}

PEAT_Clock PET_Coll_Event::remove_tractor_overhead(PEAT_Clock *prolog_overhead)
{
    PEAT_Clock p_overhead = 0, ct_overhead = 0, host_overhead;
    host_overhead = PET_Local_Event::remove_tractor_overhead(&p_overhead);
    int size = (int)t_events.size();
    for (int i = 0; i < size; i++) {
        PEAT_Clock cp_overhead = 0;
        t_events[i]->move_event(p_overhead);
        PEAT_Clock overhead = t_events[i]->remove_tractor_overhead(&cp_overhead);
        t_events[i]->sub_t_t = overhead;
        for (int j = i + 1; j < size; j++) {
            if (t_events[j]->o_time.func.in > t_events[i]->o_time.func.out) {
                t_events[j]->move_event(overhead); // dont't move overlapped events
            } else {
                PEAT_Clock c_overhead = t_events[j]->remove_tractor_overhead();
                t_events[i]->e_time.mpi.adjust(c_overhead);
                t_events[i]->e_time.func.adjust(c_overhead);
                // e_time.mpi.adjust(c_overhead);
                // e_time.func.adjust(c_overhead);
                t_events[j]->move_event(cp_overhead);
                overhead += c_overhead;
            }
        }
        ct_overhead += overhead;
        t_events[i]->sub_t_t_t = get_core()->get_tractor_overhead(rank) + ct_overhead + p_overhead;
        e_time.mpi.adjust(overhead);
        e_time.func.adjust(overhead);
        if (e_time.pe > 0)
            e_time.pe.adjust(overhead);
    }
    t_time = e_time;
    return host_overhead + ct_overhead;
}

PEAT_Bool PET_Coll_Event::split_event()
{
    if (event_parts.duration() > 0)
        return true;
    int size = (int)t_events.size();
    if (size > 0) {
        for (int i = 0; i < size; i++) {
            t_events[i]->split_event();
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
PEAT_Clock PET_Coll_Event::move_event(PEAT_Clock sub_t)
{
    int size = (int)t_events.size();
    PET_Local_Event::move_event(sub_t);
    for (int i = 0; i < size; i++) {
        t_events[i]->move_event(sub_t);
    }
    return sub_t;
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
                t_events[0]->is_initiated = true;
                index_event_being_estimated = 0;
                submit(t_events[0]->e_time.func.in, EET_FUNC_START, t_events[0], this, NULL);
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
            {
                PEAT_Signed_Num index = find_t_event(event->submiter);
                if (index >= 0) {
                    if (event->submiter->is_estimated()) {
                        if (index < (PEAT_Signed_Num)size - 1) {
                            // move other events
                            move_events(index + 1, event->submiter->sub_t_ae, false);
                            t_events[index + 1]->initiate();
                            index_event_being_estimated = index + 1;
                            // t_events[index + 1]->is_initiated = true;
                            // submit(t_events[index + 1]->e_time.func.in, EET_FUNC_START, t_events[index + 1], this, NULL);
                        } else {
                            if (get_event_type() != PET_EVENT_TYPE_COMPLETION) { // completion events will hadnle by themselves
                                // last event
                                index_event_being_estimated = -1;
                                concatenate_event();
                                submit(e_time.func.out, EET_FUNC_END, this, this, NULL);
                            }
                        }
                    }
                } /* else {
                    pet_throw_error(pet_core, this, event, "Unknown submitter.\n");
                } */
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
    int size = (int)t_events.size();
    if (size > 1) {
        for (int i = size - 1; i > 0; i--) {
            if (t_events[i - 1]->t_time.func.out > t_events[i]->t_time.func.in) { // correct overlapped events
                pet_warning(get_core(), this, "Overlapped events detected\n");
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

    int size = (int)t_events.size();
    for (int i = 0; i < size; i++) {
        if (t_events[i] == event) {
            t_events[i] = NULL;
        }
    }
}
