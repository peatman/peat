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

#ifndef __CTT_H__
#define __CTT_H__

#include "ptp.h"

#include "communicators.h"
#include "model_id.h"
#include "tevent.h"
#include <vector>
struct CTT_Event {
    PEAT_Clock duration, occurences;
    PEAT_Rank rank;
    T_property func_class;
    CTT_Event() : duration(0), occurences(0), rank(-1), func_class(T_PROPERTY_NONE) {}
    std::vector<PEAT_UID> addrs;
    struct p2p_properties {
        PEAT_Message_Size size;
        bool operator!=(const p2p_properties &s)
        {
            return false;
        }
        p2p_properties() : size(-1) {};
    } p2p;
    struct coll_properties {
        PEAT_Message_Size aggr_send_size, aggr_recv_size;
        PEAT_MPI_Comm comm_id;
        bool operator!=(const coll_properties &s)
        {
            if (comm_id != s.comm_id)
                return true;
            if (aggr_send_size != s.aggr_send_size)
                return true;
            if (aggr_recv_size != s.aggr_recv_size)
                return true;
            return false;
        }
        coll_properties() : comm_id(-1), aggr_send_size(-1), aggr_recv_size(-1) {};
    } coll;
    struct req_properties {
        std::vector<PEAT_MPI_Request> reqs;
        bool operator!=(const req_properties &s)
        {
            if (reqs != s.reqs)
                return true;
            return false;
        }
    } req;
    bool operator==(const CTT_Event &s)
    {
        if (rank != s.rank)
            return false;
        if (func_class != s.func_class)
            return false;
        if (p2p != s.p2p)
            return false;
        if (coll != s.coll)
            return false;
        if (addrs != s.addrs)
            return false;
        return true;
    }
    void add_call(PEAT_Clock time)
    {
        duration += time;
        occurences++;
    }
    bool operator<(const CTT_Event &s)
    {
        if (duration > s.duration)
            return true; // I need reverse order
        return false;
    }
};

class CTT
{
  public:
    std::vector<PEAT_Rank> whole_ranks;
    std::vector<PEAT_Rank> my_ranks;
    Func_ID *fid;
    File_Storage_GZ *input_HWC;
    File_Storage_GZ *input_MPI;
    Address_File_Storage *user_region_store;
    Communicators *com_store;
    PTP *ptp;

    const Global_Parameters globals;
    const Model_ID &model_id;
    std::map<PEAT_Rank, PEAT_UID> last_idx;
    std::map<PEAT_Rank, User_Address_Key> last_key;
    CTT(Global_Parameters &file_names, Model_ID &model_id_storage);
    ~CTT();
    bool parse_single_event(const int rank, String_Split &line_mpi, String_Split &line_hwc);
    void process();
    bool second_line;

  private:
    PEAT_LNum events_processed;
    PEAT_Message_Size get_aggr_size(const std::vector<PEAT_Message_Size> bytes) const
    {
        PEAT_Message_Size size = 0;
        for (std::vector<PEAT_Message_Size>::const_iterator byte_it = bytes.cbegin(); byte_it != bytes.cend(); ++byte_it) {
            size += *byte_it;
        }
        return size;
    }
    void parse_common(const PEAT_Rank rank, const Tractor_Event &t_event, CTT_Event &f_event)
    {
        f_event.rank = rank;
        f_event.addrs = t_event.addrs;
        f_event.duration = t_event.tsc_time.mpi.duration(); // duration of event
        f_event.occurences = 1;
        f_event.func_class = t_event.func_class;
    };
    void parse_P2PTx(const PEAT_Rank rank, const Tractor_Event &t_event, CTT_Event &f_event)
    {
        parse_common(rank, t_event, f_event);
        f_event.p2p.size = t_event.P2P.transfered_bytes;
    };
    void parse_P2PRx(const PEAT_Rank rank, const Tractor_Event &t_event, CTT_Event &f_event)
    {
        parse_common(rank, t_event, f_event);
        f_event.p2p.size = t_event.P2P.transfered_bytes;
    };
    void parse_P2PPb(const PEAT_Rank rank, const Tractor_Event &t_event, CTT_Event &f_event)
    {
        parse_common(rank, t_event, f_event);
        f_event.p2p.size = t_event.P2P.transfered_bytes;
    };
    void parse_collective(const PEAT_Rank rank, const Tractor_Event &t_event, CTT_Event &f_event)
    {
        parse_common(rank, t_event, f_event);
        f_event.coll.comm_id = t_event.Collective.comm;
        f_event.coll.aggr_send_size = get_aggr_size(t_event.send_bytes);
        f_event.coll.aggr_recv_size = get_aggr_size(t_event.recv_bytes);
    };
    void parse_cn(const PEAT_Rank rank, const Tractor_Event &t_event, CTT_Event &f_event)
    {
        parse_common(rank, t_event, f_event);
        f_event.req.reqs = t_event.requests;
    };
    void parse_local(const PEAT_Rank rank, const Tractor_Event &t_event, CTT_Event &f_event) { parse_common(rank, t_event, f_event); };
    void parse_comm(const PEAT_Rank rank, const Tractor_Event &t_event, CTT_Event &f_event) { parse_common(rank, t_event, f_event); };
    void parse_GPU(const PEAT_Rank rank, const Tractor_Event &t_event, CTT_Event &f_event) { parse_common(rank, t_event, f_event); };
    std::map<PEAT_Rank, std::map<std::string, std::list<CTT_Event>>> funcs_out;
    //    std::map <std::string, std::list<CTT_Event>> funcs_out;
};

#endif
