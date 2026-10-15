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

#ifndef __MAT_H__
#define __MAT_H__

#include "ptp.h"

#include "collectives.h"
#include "comm.h"
#include "osc.h"
#include "communicators.h"
#include "gpu.h"
#include "model_id.h"
#include "p2pal.h"
#include "requests.h"
#include <vector>
struct Mat_Time {
    PEAT_Clock durations, occurences;
    Mat_Time() : durations(0), occurences(0) {}
};
class Mat
{
  public:
    PEAT_Num all_finished, total_completed_ranks;
    std::vector<PEAT_Rank> whole_ranks;
    std::vector<PEAT_Rank> my_ranks;
    std::vector<PEAT_Rank> completed_ranks;
    Communicators *comm_store;
    PTP *ptp;
    Collectives *coll_func_store;
    P2PAL *p2p_func_store;
    GPU *gpu_func_store;
    Comm* comm_func_store;
    OSC* osc_func_store;
    Requests *request_func_store;
    Request_Store* req_store;
    File_Storage_GZ *input_MPI;
    File_Storage_GZ *input_HWC;
    File_Storage_GZ *output_MPI;
    File_Storage_GZ *output_HWC;
    File_Storage_GZ *output_bind;
    std::vector<PEAT_LNum> rank_events_processed;
    PEAT_LNum events_processed;

    // File_Storage_GZ *output_model;
    // File_Storage_GZ *output_model_message;
    Address_File_Storage *user_region_store;
    const Global_Parameters globals;
    const Model_ID &model_id;
    std::vector<PEAT_Clock> tsc_corrector, tsc_predictor;
    std::map<PEAT_Clock, PEAT_Rank> timeline;
    std::map<int, int> p_control_in_use;
    std::map<int, int> pc_levels;
    PEAT_Rank get_soonest_event(PEAT_Clock &tsc);
    std::map<PEAT_Num, std::map<PEAT_Property, Mat_Time>> timings;
    Mat(Global_Parameters &file_names, Model_ID &model_id_storage);
    ~Mat();
    Mat(const Mat &) = delete;
    Mat &operator=(const Mat &) = delete;
    void write_trace_info(const std::string &name);
    PEAT_Bool parse_single_event(const int rank);
#ifdef PEAT_MPI
    PEAT_Num handle_incoming_messages();
    std::list<PEAT_Event_Delivery> active_deliveries;
    PEAT_Num process_outgoing_messages();
    void print_remote_string(int size, int tag, int src);
    PEAT_Signed_Num check_unbound_events()
    {
        PEAT_Signed_Num p2p_ue = p2p_func_store->check_unbound_events();
        PEAT_Signed_Num coll_ue = coll_func_store->check_unbound_events();
        return p2p_ue + coll_ue;
    }
#endif
    void process();
    void process_unbound_events(PEAT_Rank my_rank)
    {
        p2p_func_store->process_unbound_events(my_rank);
        coll_func_store->process_unbound_events(my_rank);
    }
    PEAT_Bool second_line;

  private:
    String_Split line_mpi;
    String_Split line_hwc;
};

#endif
