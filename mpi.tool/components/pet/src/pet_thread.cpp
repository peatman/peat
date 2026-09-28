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

#include <iomanip>
#include <list>
#include <sstream>
#include <stdexcept>

#include <assert.h>
#include <limits.h>

#include "func_id.h"
#include "pet_event.h"
#include "pet_rank.h"
#include "pet_thread.h"
#include "utils.h"

#define P2PDELIM '|'

#define event_dbg(x)  \
    if (verbose > x) \
    cerr

PET_Thread::PET_Thread(PET_Rank *parent, PEAT_Thread t) : tractor_overhead_time(0), sub_time(0), event_being_estimated(NULL), rank(parent), thread_num(t),
                                                          nested_begins(0), nested_ends(0), begins(0), ends(0)
{
    start_event_it = cached_event_it = active_events.begin();
    verbose = 1;
}

PEAT_Clock PET_Thread::delete_processed_events()
{
    std::list<PET_Event *>::iterator evt_it;
    std::list<PET_Event *>::reverse_iterator tail_evt_it;
    get_head_event(evt_it);
    PEAT_Clock latest_clock = 0;
    get_tail_event(tail_evt_it);
    while (evt_it != active_events.end()) {
        PET_Event *event = *evt_it;
        if (event == event_being_estimated)
            break; // definitely no more events to delete
        assert(event);
        if (event->release()) {
            if (event->safe_to_delete() && (rank->rank_finished || *tail_evt_it != event) && event->can_be_destroyed()) {
                latest_clock = event->e_time.func.out;
                evt_it = delete_event(evt_it);
                start_event_it = cached_event_it = evt_it;
                if (evt_it != active_events.end())
                    (*evt_it)->prev_event = NULL; // remove previous event reference
                std::cerr << global_func_id.name_by_idx(event->func_id) << " Rank:" << event->rank << "(" << event->thread_id << ").ID:" << event->ids_str() << ". Destroyed." << std::endl;
                delete event; // destroy object
                continue;
            }
        } else {
            break; // if current event can't be destroyed next ones can't be as well
        }
        /*if (!event->is_estimated()) {
            break;
        }*/
        ++evt_it;
    }
    return latest_clock;
}
void PET_Thread::adjust_nested(PET_OMPT_Event *event)
{
    if (event->func_id != OMPT_PARALLEL_BEGIN_ID && event->func_id != OMPT_PARALLEL_END_ID)
        return;
    /*
    for (std::list <PET_Event*>::reverse_iterator evt_it = active_events.rbegin(); evt_it != active_events.rend(); ++evt_it) {
        PET_Event* evt = *evt_it;
        switch (evt->func_id) {
            case OMPT_PARALLEL_BEGIN_ID:
                begins++;
                break;
            case OMPT_NESTED_PARALLEL_BEGIN_ID:
                nested_begins++;
                break;
            case OMPT_PARALLEL_END_ID:
                ends++;
                break;
            case OMPT_NESTED_PARALLEL_END_ID:
                nested_ends++;
                break;
        }
    }
    */
    if (begins < ends || nested_begins < nested_ends) {
        pet_throw_error(event->get_core(), event, NULL, "Parallel sections numbers mismatch.\n");
        return;
    }

    if (event->func_id == OMPT_PARALLEL_BEGIN_ID) {
        if (begins > ends) {
            event->func_id = OMPT_NESTED_PARALLEL_BEGIN_ID;
            nested_begins++;
            return;
        }
        begins++;
        return;
    }
    if (event->func_id == OMPT_PARALLEL_END_ID) {
        if (nested_begins > nested_ends) {
            event->func_id = OMPT_NESTED_PARALLEL_END_ID;
            nested_ends++;
            return;
        }
        ends++;
        return;
    }
}

void PET_Thread::check_nested(PET_OMPT_Event *event)
{
    if (event->func_id == OMPT_PARALLEL_BEGIN_ID) {
        begins++;
    }
    if (event->func_id == OMPT_PARALLEL_END_ID) {
        ends++;
    }
}

std::list<PET_Event *>::iterator &PET_Thread::update_active_event_chain_ps(PEAT_Clock sub_t, std::list<PET_Event *>::iterator &evt_it)
{
    // Move all event from current event till the end of parallel section, returns iterator to event next from parallel section end
#if 1
    // faster version
    while (evt_it != active_events.end()) {
        PET_Event *evt = *evt_it++;
        evt->move_event(sub_t);
        if (evt->func_id == OMPT_PARALLEL_END_ID)
            break;
    }
    return evt_it;
#else
    PET_Event *event = *evt_it;
    PET_Event *stop_event = event->get_parallel_section_end();
    std::list<PET_Event *>::iterator start_event = evt_it;
    std::list<PET_Event *>::iterator end_event = find_event(stop_event, true);
    if (end_event != active_events.end())
        ++end_event;
    for (std::list<PET_Event *>::iterator event_it = start_event; event_it != end_event;) {
        PET_Event *evt = *event_it++;
        evt->move_event(sub_t);
    }
    return end_event;
#endif
}
std::list<PET_Event *>::iterator &PET_Thread::update_active_event_chain_bps(PEAT_Clock sub_t, std::list<PET_Event *>::iterator &evt_it)
{
    // Move all event between parallel sections, returns iterator to first event of next parallel section (PARALLEL_BEGIN)
#if 1
    // faster version
    while (evt_it != active_events.end()) {
        PET_Event *evt = *evt_it;
        if (evt->func_id == OMPT_PARALLEL_BEGIN_ID)
            break;
        evt->move_event(sub_t);
        ++evt_it;
    }
    return evt_it;
#else
    PET_Event *event = *evt_it;
    std::list<PET_Event *>::iterator start_event = evt_it;
    std::list<PET_Event *>::iterator end_event = active_events.end();
    for (std::list<PET_Event *>::iterator event_it = start_event; event_it != end_event; ++event_it) {
        PET_Event *evt = *event_it;
        if (evt->func_id == OMPT_PARALLEL_BEGIN_ID)
            break;
        evt->move_event(sub_t);
    }
    return end_event;
#endif
}

std::list<PET_Event *>::iterator &PET_Thread::delete_event(std::list<PET_Event *>::iterator &evt_it)
{
    rank->remove_pre_entry(*evt_it);
    evt_it = active_events.erase(evt_it);
    return evt_it;
}
