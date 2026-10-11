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

#ifndef __TE_EVENT_H__
#define __TE_EVENT_H__
#include "assert.h"
#include "transenv.h"

enum TE_Event_Type {
    TE_Event_None,
    TE_Event_RTS,
    TE_Event_RTS_R,
    TE_Event_CTS,
    TE_Event_CTS_R,
    TE_Event_Message_TX,
    TE_Event_Message_RX,
    TE_Event_ACK,
    TE_Event_ACK_R,
    TE_Event_MAX
};
/*
static TE_Event_Type write2read(TE_Event_Type in) // this routine highly depends on order of enum above, modfify one with caution!!!
{
    int out = (int)in;
    return (TE_Event_Type)(++out);
}
*/
enum TE_Queue {
    TE_None,
    TE_Write,
    TE_Read,
    TE_All,
    TE_Max
};
enum TR_TYPE {
    TR_TYPE_NONE,
    TR_TYPE_TRANSMIT,
    TR_TYPE_RECEIVE,
    TR_TYPE_MAX
};

struct PET_Event;
class TE_Timeline;
struct TE_Event {
    PEAT_UID order_id, global_order_id, completed_id, uniq_id;
    PEAT_Clock start, t_finish, finish, valid_till;
    PEAT_Float stage_finish;
    PEAT_Clock ideal_finish;
    TE_Event_Type type;
    PEAT_Message_Size size;
    PEAT_LNum num_units;
    PEAT_LNum remainder;
    PEAT_Rank src, dst;
    PEAT_Node n_src, n_dst;
    PEAT_Proc p_src, p_dst;
    PEAT_Bool is_blocking;
    TE_Timeline *src_timeline, *dst_timeline;
    PET_Event *owner;
    PEAT_Trasport_Environment tr_env;
    TE_Event *parent_event;
    TE_Event() : order_id(-1), global_order_id(-1), completed_id(-1), uniq_id(-1),
                 start(-1), t_finish(LLONG_MAX), finish(-1), valid_till(-1),
                 stage_finish(-1), ideal_finish(-1),
                 type(TE_Event_None), size(-1), num_units(-1), remainder(-1),
                 src(-2), dst(-2),
                 n_src(-2), n_dst(-2),
                 p_src(-2), p_dst(-2),
                 is_blocking(false),
                 src_timeline(NULL), dst_timeline(NULL),
                 owner(NULL),
                 parent_event(NULL)

    {
    }
    PEAT_Bool operator<(const TE_Event &s)
    {
        if (t_finish < s.t_finish)
            return true;
        return false;
    }
    TR_TYPE get_evt_tr_type()
    {
        switch (type) {
        case TE_Event_RTS:
        case TE_Event_CTS:
        case TE_Event_Message_TX:
        case TE_Event_ACK:
            return TR_TYPE_TRANSMIT;
        case TE_Event_RTS_R:
        case TE_Event_CTS_R:
        case TE_Event_Message_RX:
        case TE_Event_ACK_R:
            return TR_TYPE_RECEIVE;
        default:
            assert(0);
        }
        return TR_TYPE_NONE;
    }
    void convert_event(TE_Event *src, PEAT_Clock last_start);
    /*
    PEAT_Bool operator >(const TE_Event& s) {
        if (t_finish > s.t_finish) return true;
        return false;
    }
    PEAT_Bool operator <=(const TE_Event& s) {
        if (t_finish <= s.t_finish) return true;
        return false;
    }
    PEAT_Bool operator >=(const TE_Event& s) {
        if (t_finish >= s.t_finish) return true;
        return false;
    }*/
    PEAT_Bool operator==(const TE_Event &s)
    {
        if (t_finish != s.t_finish)
            return false;
        if (num_units != s.num_units)
            return false;
        if (src != s.src)
            return false;
        if (dst != s.dst)
            return false;
        if (n_src != s.n_src)
            return false;
        if (n_dst != s.n_dst)
            return false;
        if (num_units != s.num_units)
            return false;
        if (finish != s.finish)
            return false;
        if (start != s.start)
            return false;
        if (owner != s.owner)
            return false;
        if (uniq_id != s.uniq_id)
            return false;
        if (order_id != s.order_id)
            return false;
        if (global_order_id != s.global_order_id)
            return false;
        // if (tr_env != s.tr_env) return false;
        return true;
    }
    PEAT_Bool operator!=(const TE_Event &s)
    {
        return !(*this == s);
    }
    PEAT_Clock get_cpu_times()
    {
        return tr_env.get_cpu_times();
    };
    PEAT_Clock get_overlap() const
    {
        if (finish > 0)
            return finish - ideal_finish;
        return 0;
    };
    PEAT_Clock get_fabric() const
    {
        if (finish > 0)
            return finish - start;
        return 0;
    };
    void print(FILE *fp, const char *str);
};

#endif //__TE_EVENT_H__