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

#include "timeline.h"
#include "pet_event.h"
PEAT_UID Estimation_Timeline::order_id = 0;
Estimation_Event Estimation_Timeline::last_deleted_event;
FILE *Estimation_Timeline::fp_debug = NULL;
PEAT_Bool Estimation_Event::handle()
{
    assert(owner);
    return owner->estimate(this);
}

PEAT_Bool Estimation_Timeline::insert_event(Estimation_Event *event, PEAT_Bool is_posted)
{
    // assert(event->start >= last_deleted_event.start);
    if (event->start < last_deleted_event.start) {
        pet_throw_error(event->owner->pet_core, event->owner, event, "Current (ee)event starts before already estimated one.\n");
    }
    event_mutex->lock();
    {
        event->order_id = order_id;
        event->owner->increase_ee_events();
        event->submiter->increase_ee_events();
        if (fp_debug && !is_posted)
            event->print(fp_debug, "INSERTED: ");
        if (fp_debug && is_posted)
            event->print(fp_debug, "POSTED: ");
#pragma omp atomic
        order_id++;
        timeline[event->start].push_back(*event);
        set_changed();
    }
    event_mutex->unlock();

    return true;
}

PEAT_Bool Estimation_Timeline::repeat_last_ee_entry(PET_Event *evt, PEAT_Bool log_event)
{
    Estimation_Event event = last_deleted_event;
    event.owner = event.submiter = evt;
    event.type = EET_NOTIFY;
    // assert(event->start >= last_deleted_event.start);
    if (event.start < last_deleted_event.start) {
        pet_throw_error(event.owner->pet_core, event.owner, &event, "Current (ee)event starts before already estimated one.\n");
    }
    event_mutex->lock();
    {
        event.order_id = order_id;
        event.owner->increase_ee_events();
        event.submiter->increase_ee_events();
        if (fp_debug && log_event)
            event.print(fp_debug, "INSERTED: ");
#pragma omp atomic
        order_id++;
        timeline[event.start].push_back(event);
        set_changed();
    }
    event_mutex->unlock();
    return true;
}

PEAT_Bool Estimation_Timeline::delete_event(Estimation_Event *event, PEAT_Bool is_posted)
{
    std::map<PEAT_Clock, std::list<Estimation_Event>>::iterator ee_it = timeline.begin();
    Estimation_Event evt = ee_it->second.front();
    if (last_deleted_event == *event) {
        pet_throw_error(event->owner->pet_core, event->owner, event, "This event has already been deleted.\n");
    }
    if (evt != *event) {
        pet_throw_error(event->owner->pet_core, event->owner, event, "Event deletion mismatch.\n");
    }
    // assert( event->start >= last_deleted_event.start || is_posted);
    if (event->start < last_deleted_event.start && (!is_posted)) {
        pet_throw_error(event->owner->pet_core, event->owner, event, "event->start < last_deleted_event.start.\n");
    }
    event_mutex->lock();
    {
        event->owner->decrease_ee_events();
        event->submiter->decrease_ee_events();
        if (fp_debug && !is_posted)
            event->print(fp_debug, "COMPLETED:");
        if (fp_debug && is_posted)
            event->print(fp_debug, "REMOVED:");
        if (!is_posted) {
            last_deleted_event = *event;
        }
        ee_it->second.pop_front();
        if (ee_it->second.empty())
            timeline.erase(ee_it);
        set_changed();
    }
    event_mutex->unlock();
    return true;
}
