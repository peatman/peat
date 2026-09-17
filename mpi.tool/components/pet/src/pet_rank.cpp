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
#include "utils.h"

#define P2PDELIM '|'

#define event_dbg(x)  \
    if (verbose > x) \
    cerr
PEAT_LNum PET_Rank::total_fabric_events = 0;
PEAT_LNum PET_Rank::total_fabric_events_added = 0;
PEAT_LNum PET_Rank::total_fabric_events_removed = 0;

void PET_Rank::collect_garbage()
{
    PEAT_Clock max_deleted_clock = 0;
    for (std::vector<PET_Thread *>::iterator thread_it = threads.begin(); thread_it != threads.end(); ++thread_it) {
        PEAT_Clock deleted_clock = (*thread_it)->delete_processed_events();
        max_deleted_clock = __max__(deleted_clock, max_deleted_clock);
    }
    remove_fabric_events(max_deleted_clock);
}
void PET_Rank::register_parallel_region(PET_Event *event)
{
    PET_Local_Event *evt = (PET_Local_Event *)event;
    if (event->func_id != MPI_INIT_ID && event->func_id != MPI_INIT_THREAD_ID &&
        event->func_id != MPI_FINALIZE_ID && event->func_id != MPI_PCONTROL_ID)
        return;
    if (evt->p_control_id <= 0) {
        std::list<PET_Region> &region_list = parallel_regions[-evt->p_control_id];
        for (std::list<PET_Region>::iterator region_it = region_list.begin(); region_it != region_list.end(); ++region_it) {
            PET_Region &reg = *region_it;
            if (reg.o_exit.in < 0) { // enough to check any single value
                reg.o_exit = evt->o_time.func;
                reg.t_exit = evt->t_time.func;
                reg.e_exit = evt->e_time.func;
                return; // ok we've found one... exit
            }
        }
        if (evt->p_control_id == 0) {
            PET_Region t_region;
            t_region.o_entry = evt->o_time.func;
            t_region.t_entry = evt->t_time.func;
            t_region.e_entry = evt->e_time.func;
            region_list.push_back(t_region);
        } else {
            pet_throw_error(this, NULL, NULL, "No suitable PControl region found.\n");
        }
    }
    if (evt->p_control_id > 0) {
        std::list<PET_Region> &region_list = parallel_regions[evt->p_control_id];
        PET_Region t_region;
        t_region.o_entry = evt->o_time.func;
        t_region.t_entry = evt->t_time.func;
        t_region.e_entry = evt->e_time.func;
        region_list.push_back(t_region);
    }
}
PEAT_Bool PET_Rank::insert_fabric_event(PET_Fabric_Event &event)
{
    fabric_events.push_back(event);
#pragma omp atomic
    total_fabric_events++;
#pragma omp atomic
    total_fabric_events_added++;
    return true;
}

PEAT_Bool PET_Rank::remove_fabric_events(PEAT_Clock clk)
{

    PET_Fabric_Event evt;
#if 0
	while ( !fabric_events.empty() ) {
		evt = fabric_events.front();
		if ( evt.ut_time.func.out < event->ut_time.func.out ) {
			fabric_events.pop_front();
#pragma omp atomic  
			total_fabric_events--;
#pragma omp atomic  
			total_fabric_events_removed++;
		} else break;
	};
#else
    for (std::list<PET_Fabric_Event>::iterator fab_event_it = fabric_events.begin(); fab_event_it != fabric_events.end();) {
        evt = *fab_event_it;
        if (evt.ut_time.func.in >= clk)
            break;
        if (evt.ut_time.func.out >= clk) {
            ++fab_event_it;
            continue;
        }
        fab_event_it = fabric_events.erase(fab_event_it);
#pragma omp atomic
        total_fabric_events--;
#pragma omp atomic
        total_fabric_events_removed++;
    }
#endif

    return true;
}

PET_Event *PET_Rank::get_start_omp_paralel_section(PET_Event *event) const
{
    for (std::list<PET_Event *>::const_reverse_iterator event_it = omp_parallel_sections.crbegin(); event_it != omp_parallel_sections.crend(); ++event_it) {
        if ((*event_it)->e_time.func.in < event->e_time.func.in)
            return *event_it;
    }
    return NULL;
}

PET_Event *PET_Rank::get_finish_omp_paralel_section(PET_Event *event) const
{
    for (std::list<PET_Event *>::const_iterator event_it = omp_parallel_sections.cbegin(); event_it != omp_parallel_sections.cend(); ++event_it) {
        if ((*event_it)->e_time.func.out > event->e_time.func.in)
            return *event_it;
    }
    return NULL;
}
