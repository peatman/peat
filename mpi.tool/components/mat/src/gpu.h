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

#ifndef __GPU_H__
#define __GPU_H__
#include "common_types.h"
#include "communicators.h"
#include "func_id.h"
#include "ptp.h"
#include "records.h"
#include "tevent.h"

#include <assert.h>
#include <list>
#include <map>
#include <string>
#include <vector>

struct GPU_Event { // tsc_u_time is for debugging purposes only!
    GPU_Event() : ids(2, -1), request(0) {}
    PEAT_Event_IDs ids;
    PEAT_Collective_Bytes send_bytes, recv_bytes;
    // std::vector<PEAT_Addr> addrs;
    std::vector<PEAT_UID> addrs;
    PEAT_Time tsc_time, tsc_u_time;
    PEAT_MPI_Request request;
    Phase completion;
};

struct PEAT_GPU_Record {
    PEAT_MPI_Func_ID func_id;
    PEAT_Collective_Bytes send_bytes, recv_bytes;
    std::map<PEAT_Rank, std::string> comp_funcs; // map<source_rank, destination_properties>
    PEAT_Rank root, size;
    PEAT_MPI_Comm comm;
    PEAT_GPU_Record() : root(-1), size(-1), comm(0)
    {
        func_id = TMPI_FUNC_ID_NONE;
    };
    bool operator<(const PEAT_GPU_Record &right) const
    {
        if (size < right.size)
            return true;
        if (size > right.size)
            return false;
        if (root < right.root)
            return true;
        if (root > right.root)
            return false;
        if (func_id < right.func_id)
            return true;
        if (func_id > right.func_id)
            return false;
        if (send_bytes < right.send_bytes)
            return true;
        if (right.send_bytes < send_bytes)
            return false;
        if (recv_bytes < right.recv_bytes)
            return true;
        if (right.recv_bytes < recv_bytes)
            return false;
        if (comp_funcs < right.comp_funcs)
            return true;
        if (right.comp_funcs < comp_funcs)
            return false;
        return false;
    }
};

enum Where_Located {
    WL_HOST,
    WL_DEVICE,
    WL_SHARED
};

struct Memory_Region {
    Where_Located loc;
    PEAT_Handle handle;
    PEAT_Message_Size size;
};
struct PEAT_Reduced_Event_Info {
    PEAT_Time time;
    PEAT_MPI_Func_ID func_id;
    PEAT_Event_IDs ids;
    PEAT_UID thread_id;
};
struct Memory_Transfer {
    PEAT_Handle handle;
    Memory_Region from, to;
    PEAT_Message_Size size;
    PEAT_Reduced_Event_Info init, completion;
};

struct Region_Exec {
    PEAT_Handle handle;
    PEAT_Reduced_Event_Info init, completion;
};

class GPU : public Outputs
{
    bool parse_gpu;
    Memory_Transfer *cur_transfer;
    Region_Exec *cur_exec;

  public:
    GPU(PTP *p, Communicators *c,
        /*
        #ifdef PEAT_MPI
                const std::vector<PEAT_Rank>& wR, const std::vector<PEAT_Rank>& mRs, const MPI_Comm mat_cc, const PEAT_Num mat_ccs, const PEAT_Rank m_r,
        #endif
         */
        int verbose = false);
    virtual ~GPU();
    // cur_transfer/cur_exec cache raw pointers into the transfers/executions maps; copying would alias/invalidate them.
    GPU(const GPU &) = delete;
    GPU &operator=(const GPU &) = delete;
    void parse_GPU(const PEAT_Rank rank, const Tractor_Event &event);

    bool find_handle(PEAT_Rank rank, PEAT_MPI_Request request);
    bool update_handle(PEAT_Rank rank, PEAT_MPI_Request request, const Phase &phase);

  private:
    void print_GPU_out(const Tractor_Event &event);
    void print_GPU_bind(const Tractor_Event &event);
    void create_memory_region(Memory_Region &region, PEAT_Rank rank, Where_Located loc, PEAT_Handle handle, PEAT_Message_Size size, PEAT_Addr addr);

    bool check_completion();
    PTP *ptp;
    Communicators *com_store;
    int verbose;
    std::map<PEAT_Rank, std::map<PEAT_Handle, PEAT_Bool>> GPU_contexts;      // for checking purposes only
    std::map<PEAT_Rank, std::map<PEAT_Handle, PEAT_Bool>> GPU_list_contexts; // for checking purposes only
    std::map<PEAT_Rank, std::map<PEAT_Addr, Memory_Region>> memory_regions;
    std::map<PEAT_Rank, std::map<PEAT_Handle, Memory_Transfer>> transfers;
    std::map<PEAT_Rank, std::map<PEAT_Handle, Region_Exec>> executions;

  public:
    std::map<int, std::map<PEAT_GPUT_Record, PEAT_Single_Event_Clock>> printed_gpu_transfers;
    std::map<int, std::map<PEAT_GPUE_Record, PEAT_Single_Event_Clock>> printed_gpu_executions;
};

#endif // __GPU_H__
