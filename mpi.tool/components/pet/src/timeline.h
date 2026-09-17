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

#ifndef __TIMELINE_H__
#define __TIMELINE_H__

#include "common_types.h"
#include "pet_defines.h"
#include "te_event.h"
#include <mutex>

enum Estimation_Event_Type {
    EET_NONE = 0,
    EET_FUNC_START,
    EET_FUNC_END,
    EET_RTS,
    EET_RTS_RECV_NOTIFY,
    EET_RTS_RECV,
    EET_CTS,
    EET_CTS_RECV_NOTIFY,
    EET_CTS_RECV,
    EET_MSG_TX,
    EET_MSG_RECV_NOTIFY,
    EET_MSG_RECV,
    EET_ACK,
    EET_ACK_RECV_NOTIFY,
    EET_ACK_RECV,
    EET_NOTIFY,
    EET_CHILD_COMPLETED,
    EET_PROGRESS_NOTIFY,
    EET_MAX
};
struct PET_Event;

struct Estimation_Event {
    Estimation_Event_Type type;
    PET_Event *owner, *submiter;
    PEAT_Clock start;
    PEAT_UID order_id;
    TE_Event info;
    Estimation_Event() : type(EET_NONE), owner(NULL), submiter(NULL), start(-1), order_id(-1) {};
    PEAT_Bool handle();
#if 0
	friend PEAT_Bool operator==(Estimation_Event& l, Estimation_Event& r) {
		return l.type == r.type && l.owner == r.owner && l.submiter == r.submiter && l.start==r.start;
	}
	friend PEAT_Bool operator!=(Estimation_Event& l, Estimation_Event& r) {
		return l.type != r.type || l.owner != r.owner || l.submiter != r.submiter || l.start != r.start;
	}
#else
    friend PEAT_Bool operator==(Estimation_Event &l, Estimation_Event &r)
    {
        return l.type == r.type && l.owner == r.owner && l.submiter == r.submiter && l.start == r.start && l.order_id == r.order_id;
    }
    friend PEAT_Bool operator!=(Estimation_Event &l, Estimation_Event &r)
    {
        return l.type != r.type || l.owner != r.owner || l.submiter != r.submiter || l.start != r.start || l.order_id != r.order_id;
    }
#endif
    void print(FILE *fp, const char *str);
};

struct Estimation_Timeline {
    PEAT_Bool state_changed;
    static PEAT_UID order_id;
    static Estimation_Event last_deleted_event;
    static FILE *fp_debug;
    std::map<PEAT_Clock, std::list<Estimation_Event>> timeline;
    PEAT_Mutex *event_mutex;
    void set_changed()
    {
        state_changed = true;
    }
    void clear_changed()
    {
        state_changed = false;
    }
    PEAT_Bool get_state()
    {
        return state_changed;
    }

    PEAT_Bool insert_event(Estimation_Event *event, PEAT_Bool is_posted);
    PEAT_Bool repeat_last_ee_entry(PET_Event *event, PEAT_Bool log_event);
    PEAT_Bool delete_event(Estimation_Event *event, PEAT_Bool is_posted);
    Estimation_Event *get_min_event()
    {
        if (!timeline.empty()) {
            std::map<PEAT_Clock, std::list<Estimation_Event>>::iterator ee_it = timeline.begin();
            if (ee_it->second.empty()) {
                pet_throw_error(NULL, NULL, NULL, "min_event is empty\n");
            };
            return &ee_it->second.front();
        }
        return NULL;
    }
    Estimation_Timeline() : state_changed(false)
    {
        event_mutex = new PEAT_Mutex;
    }
    ~Estimation_Timeline()
    {
        delete event_mutex;
    }
};

static Estimation_Event *min_ee_event(Estimation_Event *e, Estimation_Event *e_tmp)
{
    if (e == NULL) {
        return e_tmp;
    }
    if (e_tmp == NULL) {
        return e;
    }
    if (e_tmp->start < e->start)
        return e_tmp;
    return e;
}

#endif