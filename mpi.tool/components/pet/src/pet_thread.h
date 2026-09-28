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

#ifndef __PET_THREAD_H__
#define __PET_THREAD_H__

#include "common_types.h"
#include "pet_event.h"
#include "pet_fabric_event.h"
#include "stat.h"
#include "timeline.h"
#include <assert.h>
#include <set>

class PET_Rank;
class PET_Node;
class PET_Thread
{
    PEAT_Clock tractor_overhead_time; // moving event due to removing tractor overhead
    PEAT_Clock sub_time;              // moving event due to estimation
    PET_Event *event_being_estimated;
    std::list<PET_Event *>::iterator start_event_it, cached_event_it;
    // maps here are only for fast access
    std::list<PET_Event *> active_events;
    int verbose; // if true, add algo debug output
    // Estimation_Timeline progress_engine;
    // PEAT_Clock last_progress_notify;
    PET_Rank *rank;
    PEAT_Thread thread_num;
    PEAT_Num nested_begins, nested_ends;
    PEAT_Num begins, ends;
    std::list<PET_OMPT_Event *> parallel_sections;

  public:
    PET_Thread(PET_Rank *parent, PEAT_Thread t);
    ~PET_Thread()
    {
    }
    PEAT_Bool get_head_event(std::list<PET_Event *>::iterator &evt_it)
    {
        evt_it = active_events.begin();
        if (evt_it != active_events.end())
            return true;
        return false;
    }
    PEAT_Bool get_next_event(std::list<PET_Event *>::iterator &evt_it)
    {
        ++evt_it;
        if (evt_it != active_events.end())
            return true;
        return false;
    }
    PEAT_Bool get_tail_event(std::list<PET_Event *>::reverse_iterator &evt_it)
    {
        evt_it = active_events.rbegin();
        if (evt_it != active_events.rend())
            return true;
        return false;
    }

    std::list<PET_Event *>::iterator &delete_event(std::list<PET_Event *>::iterator &evt_it);
    std::list<PET_Event *>::iterator &update_active_event_chain_ps(PEAT_Clock sub_t, std::list<PET_Event *>::iterator &evt_it);
    std::list<PET_Event *>::iterator &update_active_event_chain_bps(PEAT_Clock sub_t, std::list<PET_Event *>::iterator &evt_it);

    PEAT_Clock delete_processed_events();

    // void collect_garbage();
    friend PET_Core;

    PET_Event *find_event_by_id(PEAT_UID uidx)
    {
        std::list<PET_Event *>::iterator event_it;
        PEAT_Bool r = get_head_event(event_it);
        // try to search in active events first
        while (r) {
            PET_Event *event = *event_it;
            if (event->ids[0] == uidx) {
                return event;
            }
            r = get_next_event(event_it);
        };
        return NULL;
    }
    PET_Event *find_event_by_request(PEAT_MPI_Request request)
    {
        std::list<PET_Event *>::iterator event_it;
        PEAT_Bool r = get_head_event(event_it);
        // skip persistent event
        while (r) {
            PET_Event *event = *event_it;
            PET_Event *evt = event->query_request(request);
            if (evt) {
                if (!evt->is_persistent_init()) {
                    return evt;
                }
            }
            r = get_next_event(event_it);
        };
        return NULL;
    }
    PEAT_Num check_for_outstanding_progress_messages(PEAT_Clock clk)
    {
        if (event_being_estimated) {
            return event_being_estimated->check_for_outstanding_progress_messages(clk);
        }
        return 0;
    }
    PET_Event *get_currently_estimating_event()
    {
        if (event_being_estimated) {
            return event_being_estimated->get_estimated_event();
        } else {
            return NULL;
        }
    }
    void insert_ael_entry(PET_Event *event, PEAT_Bool check_place)
    {
        if (check_place) {
            std::list<PET_Event *>::iterator event_it = active_events.begin();
            for (; event_it != active_events.end(); ++event_it) {
                if ((*event_it)->e_time.func.in >= event->e_time.func.in) {
                    if (event_it != active_events.begin())
                        --event_it;
                    break;
                }
            }
            active_events.insert(event_it, event);
        } else {
            active_events.push_back(event);
        }
    }
    void insert_parallel_section(PET_OMPT_Event *event)
    {
        parallel_sections.push_back(event);
    }
    void adjust_nested(PET_OMPT_Event *event);
    void check_nested(PET_OMPT_Event *event);
    PET_OMPT_Event *get_parallel_section()
    {
        if (parallel_sections.empty()) {
            return NULL;
        }
        PET_OMPT_Event *event = parallel_sections.back();
        if (event == NULL) {
            return NULL;
        }
        return event;
    }
    PET_OMPT_Event *remove_parallel_section()
    {
        if (parallel_sections.empty()) {
            pet_throw_error(this, NULL, NULL, "Empty parallel section\n");
            return NULL;
        }
        PET_OMPT_Event *event = parallel_sections.back();
        if (event == NULL) {
            pet_throw_error(this, NULL, NULL, "No matching parallel section\n");
            return NULL;
        }
        parallel_sections.pop_back();
        return event;
    }
    std::list<PET_Event *>::iterator find_event(PET_Event *event, PEAT_Bool from_current)
    {
        std::list<PET_Event *>::iterator event_it;
        if (from_current) {
            if (start_event_it != active_events.end()) {
                if ((*start_event_it)->ids[0] <= event->ids[0]) {
                    event_it = start_event_it;
                } else {
                    event_it = active_events.begin();
                }
            } else {
                event_it = active_events.begin();
            }

        } else {
            event_it = active_events.begin();
        }
        for (; event_it != active_events.end(); ++event_it) {
            if (*event_it == event)
                break;
        }
        cached_event_it = event_it;
        return event_it;
    }
    std::list<PET_Event *>::iterator find_event_cached(PET_Event *event)
    {
        std::list<PET_Event *>::iterator event_it;
        if (cached_event_it != active_events.end()) {
            if ((*cached_event_it)->ids[0] <= event->ids[0]) {
                event_it = cached_event_it;
            } else {
                event_it = active_events.begin();
            }
        } else {
            event_it = active_events.begin();
        }

        for (; event_it != active_events.end(); ++event_it) {
            if (*event_it == event)
                break;
        }
        cached_event_it = event_it;
        return event_it;
    }
    std::list<PET_Event *>::iterator find_event(PEAT_Clock clk, PEAT_Bool use_cache)
    {

        std::list<PET_Event *>::iterator event_it;
        if (use_cache) {
            event_it = start_event_it;
            if (event_it == active_events.end())
                event_it = active_events.begin();
        } else {
            event_it = active_events.begin();
        }
        for (; event_it != active_events.end(); ++event_it) {
            if ((*event_it)->e_time.func.in >= clk)
                return event_it;
        }
        pet_throw_error(NULL, NULL, NULL, "Previous Event not found\n");
        return event_it;
    }
    std::list<PET_Event *>::reverse_iterator find_revent(PET_Event *event)
    {
        std::list<PET_Event *>::reverse_iterator event_it = active_events.rbegin();
        for (; event_it != active_events.rend(); ++event_it) {
            if (*event_it == event)
                break;
        }
        return event_it;
    }
    std::list<PET_Event *>::reverse_iterator find_rpevent(PEAT_Clock clk)
    {
        std::list<PET_Event *>::reverse_iterator event_it = active_events.rbegin();
        for (; event_it != active_events.rend(); ++event_it) {
            if ((*event_it)->e_time.func.in < clk)
                break;
        }
        return event_it;
    }
    std::list<PET_Event *>::reverse_iterator get_rfirst_event(PET_Event *event)
    { // gets last event in the thread
        return active_events.rbegin();
    }
    std::list<PET_Event *>::reverse_iterator get_rlast_event(PET_Event *event)
    { // gets first event in the thread which starts later or @ the same time than 'event'
        std::list<PET_Event *>::iterator event_it = active_events.begin();
        for (; event_it != active_events.end(); ++event_it) {
            if ((*event_it)->e_time.func.in >= event->e_time.func.in)
                break;
        }
        return make_reverse_iterator(event_it);
    }
    std::list<PET_Event *>::iterator get_first_event(PET_Event *event)
    { // gets first event in the thread which starts later than 'event'
        std::list<PET_Event *>::iterator event_it = active_events.begin();
        for (; event_it != active_events.end(); ++event_it) {
            if ((*event_it)->e_time.func.in > event->e_time.func.in)
                break;
        }
        return event_it;
    }
    PET_Event *get_first_event()
    { // gets first event in the thread which starts later than 'event'
        std::list<PET_Event *>::iterator first = active_events.begin();
        if (first != active_events.end())
            return *first;
        return NULL;
    }
    std::list<PET_Event *>::iterator get_last_event(PET_Event *event)
    { // gets last event in the thread
        return active_events.end();
    }

    friend PET_Node;
    friend PET_Rank;
    friend PET_Event;
};

#endif //__PET_THREAD_H__
