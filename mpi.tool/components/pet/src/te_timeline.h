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

#ifndef __TE_TIMELINE_H__
#define __TE_TIMELINE_H__
#include <iomanip>
#include <iostream>
#include <map>
#include <mutex>
#include <sstream>
#include <stdexcept>

#include "common_types.h"
#include "ptp.h"
#include "te_event.h"
#include "transport.h"
#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <stdlib.h>

#ifdef PEAT_MPI
#include <mpi.h>
#endif
/* Defines below are temporal solution*/

#define PEAT_NIC_PACKET_LENGTH 64
#define PEAT_NIC_UNIT_LENGTH 1492
#define MAX_NIC_CHAINS 1024

#define PEAT_QPI_PACKET_LENGTH 64
#define PEAT_QPI_UNIT_LENGTH 64
#define MAX_QPI_CHAINS LLONG_MAX

#define PEAT_MEM_PACKET_LENGTH 64
#define PEAT_MEM_UNIT_LENGTH 64
#define MAX_MEM_CHAINS LLONG_MAX

#define PEAT_DDR_PACKET_LENGTH 64
#define PEAT_DDR_UNIT_LENGTH 64
#define MAX_DDR_CHAINS LLONG_MAX

struct TE_Interval {
    PEAT_Clock start, finish;
    PEAT_LNum t_units;
    TE_Event *evt;
};
class TE_Timeline
{
    PEAT_LNum max_chains;

  protected:
    static TE_Event empty_event;
    static PEAT_UID global_order_id;
    static TE_Event last_deleted_event;
    TE_Event last_evt;
    std::map<PEAT_UID, TE_Event> write_requests;
    std::map<PEAT_UID, TE_Event> read_requests;
    PEAT_UID order_id, completed_id;
    PEAT_LNum unit_length;
    PEAT_LNum packet_length;
    PEAT_Float last_read_clock;
    PEAT_Bool estimate_read;
    PEAT_LNum get_unit_length()
    {
        return unit_length;
    }

    TE_Event tuw_event, tur_event;
    PEAT_Mutex *event_mutex;

  public:
    static FILE *fp_debug;
    TE_Timeline(PEAT_LNum max_c, PEAT_LNum l, PEAT_LNum p) : max_chains(max_c), order_id(0), completed_id(0), unit_length(l), packet_length(p), last_read_clock(0), estimate_read(false)
    {
        event_mutex = new PEAT_Mutex;
    };
    virtual ~TE_Timeline()
    {
        delete event_mutex;
    }

    void set_estimate_read(PEAT_Bool r) { estimate_read = r; }
    PEAT_Bool remove_event(TE_Event *evt);
    virtual TE_Event *update_completed_event(bool isread);
    PEAT_BW get_bw(TE_Event *evt);
    PEAT_Clock get_lat(TE_Event *evt);
    PEAT_Clock get_multiplexing_lat(TE_Event *evt);
    PEAT_Num get_max_chain_num() { return max_chains; };

    virtual TE_Event *insert(TE_Event *evt, bool isperm);
    // TE_Event *get_event(PEAT_UID uniq_id, PET_Event *owner, TE_Queue queue);
    virtual TE_Event *update_write(PEAT_Clock clk, PEAT_Bool write) = 0;
    virtual TE_Event *update_read(PEAT_Clock clk, PEAT_Bool write) = 0;
    virtual TE_Event *complete(TE_Event *);
    std::map<PEAT_UID, TE_Event> &get_read_requests() { return read_requests; };
    virtual void clear_cache(bool isread) {}
};
class SEQ_Timeline : public TE_Timeline
{
    TE_Event tw_event, tr_event;
    std::list<TE_Event> active_events;
    std::list<TE_Event>::iterator active_events_it;

  public:
    SEQ_Timeline(PEAT_Num max_c = MAX_NIC_CHAINS, PEAT_LNum l = PEAT_NIC_UNIT_LENGTH, PEAT_LNum p = PEAT_NIC_PACKET_LENGTH) : TE_Timeline(max_c, l, p)
    {
        active_events_it = active_events.begin();
    };
    PEAT_LNum get_unit_length(PEAT_LNum message_size)
    {
        if (message_size < 3 * unit_length / 4)
            return packet_length;
        return unit_length;
    }

    virtual TE_Event *update_completed_event(bool isread);
    virtual TE_Event *update_write(PEAT_Clock clk, PEAT_Bool write);
    virtual TE_Event *update_read(PEAT_Clock clk, PEAT_Bool write);
    TE_Event *re_arrange_chunks(PEAT_Clock clk, bool write);

    void check_and_correct_write(PEAT_Clock clk);
    void check_and_correct_read(PEAT_Clock clk);
    void mark_reqs_orphan(TE_Event *evt);
    TE_Event *insert_req_chunk(TE_Event *evt, PEAT_Clock last_start, bool write);
    /*void clear_tmp_read_requests(PEAT_Clock valid_till) {
        for(std::map <PEAT_UID, TE_Event>::iterator evt_it = temp_read_requests.lower_bound(valid_till); evt_it != temp_read_requests.end();) {
            temp_read_requests.erase(evt_it);
        }
    }*/
    virtual TE_Event *insert(TE_Event *evt, bool isperm);
    virtual void clear_cache(bool isread)
    {
        if (isread)
            tr_event = empty_event;
        else
            tw_event = empty_event;
    }
};
class PAR_Timeline : public TE_Timeline
{
  public:
    PAR_Timeline(PEAT_Num max_c = MAX_MEM_CHAINS, PEAT_LNum l = PEAT_MEM_UNIT_LENGTH, PEAT_LNum p = PEAT_MEM_PACKET_LENGTH) : TE_Timeline(max_c, l, p) {};
    // PAR_Timeline(PEAT_LNum max_c = MAX_MEM_CHAINS) : TE_Timeline(max_buffers, PEAT_MEM_UNIT_LENGTH) {};
    virtual TE_Event *update_write(PEAT_Clock clk, PEAT_Bool write);
    virtual TE_Event *update_read(PEAT_Clock clk, PEAT_Bool write);
    virtual TE_Event *insert(TE_Event *evt, bool isperm);
};

#if 0
class NIC_Timeline : public SEQ_Timeline {
	virtual PEAT_LNum get_unit_length() { return PEAT_NIC_UNIT_LENGTH; };
public:
	NIC_Timeline() : SEQ_Timeline(MAX_NIC_BUFFERS) {};
};

class QPI_Timeline : public SEQ_Timeline {
	virtual PEAT_LNum get_unit_length() { return PEAT_QPI_UNIT_LENGTH; };
public:
	QPI_Timeline() : SEQ_Timeline(MAX_QPI_BUFFERS) {};
};

class DDR_Timeline : public PAR_Timeline {
	virtual PEAT_LNum get_unit_length() { return PEAT_DDR_UNIT_LENGTH; };
public:
	DDR_Timeline() : PAR_Timeline(MAX_DDR_BUFFERS) {};
};
#endif

#endif // __TE_TIMELINE_H__