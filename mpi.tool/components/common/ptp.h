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

#ifndef __PTP_H__
#define __PTP_H__

#include "common_types.h"
#include "model_id.h"
#include "utils.h"
#include <map>
#include <string>
#include <vector>
/*
#include "common_types.h"
#include "common_plain_types.h"
#include "pet_event.h"

#include <string>
#include <vector>
#include <map>
*/
struct Times_Sync {
    Times_Sync() : t1(0), t2(0), t3(0), t4(0) {}
    PEAT_Clock t1;
    PEAT_Clock t2;
    PEAT_Clock t3;
    PEAT_Clock t4;
};

struct PU_Info {
    PEAT_Proc core_id;
    PEAT_Proc package_id;
    PU_Info(PEAT_Proc c = -1, PEAT_Proc p = -1) : core_id(c), package_id(p) {};
};

struct Node_Info {
    std::map<PEAT_Proc, PU_Info> proc_units;
};

class PTP
{
    const Model_ID *m_id;

  public:
    PEAT_Bool cvt_to_ns;
    PTP(const Global_Parameters &glob);
    void initialize(const std::map<PEAT_Rank, std::string> &commonfile_names, const std::map<PEAT_Rank, std::string> &ptpfile_names, const std::map<PEAT_Rank, std::string> appfile_names, const Model_ID *id);

    PEAT_Bool is_SMP_src(const PEAT_Rank rank1, const PEAT_Rank rank2) const;
    PEAT_Bool is_SMP_tgt(const PEAT_Rank rank1, const PEAT_Rank rank2) const;
    PEAT_Node get_tgt_node(PEAT_Rank rank) const; // return node ID by MPI rank number
    PEAT_Node get_src_node(PEAT_Rank rank) const; // return node ID by MPI rank number
    PEAT_Proc get_tgt_package(PEAT_Rank rank);    // return proc ID by MPI rank number
    PEAT_Proc get_src_package(PEAT_Rank rank);    // return proc ID by MPI rank number
    PEAT_Num get_numa_pp(PEAT_Node node, PEAT_Node numa_node);

    PEAT_Rank get_rank_num() { return (PEAT_Rank)SMP_to_master_src.size(); }
    PEAT_Rank get_tgt_node_num() { return (PEAT_Rank)master_to_node_tgt.size(); }
    PEAT_Rank get_src_node_num() { return (PEAT_Rank)master_to_node_src.size(); }
    std::string get_host_name(PEAT_Rank rank) const;
    PEAT_Clock get_real_time(const PEAT_Clock tsc_in, const PEAT_Rank rank);
    PEAT_FREQ get_trace_freq(const PEAT_Rank rank) const
    {
        if (rank < 0) {
            return PTPD_proc_freq.at(master_node);
        } else {
            return PTPD_proc_freq.at(rank);
        }
    }
    void add_rank_MPI(const PEAT_Rank rank, PEAT_Clock val)
    {
        if (rank_MPI.count(rank)) {
            rank_MPI.at(rank) += val;
        } else {
            rank_MPI[rank] = val;
        }
    }

    PEAT_Clock get_rank_MPI(const PEAT_Rank rank) const
    {
        if (rank_MPI.count(rank)) {
            return rank_MPI.at(rank);
        } else {
            std::cerr << "get rank mpi. No information for rank=" << rank << " is available." << std::endl;
            return 0;
        }
    }

    std::map<PEAT_Rank, PEAT_Num> nodes_PPN;           //<nodeID, ppn>
    std::map<PEAT_Numa_Node, PEAT_Num> numa_nodes_PPN; //<nodeID, ppn>
  private:
    Global_Parameters globals;
    void parse_PTPD_record(PEAT_Rank rank, const Times_Sync &ptpd_times);

    void make_initial_adjustments();
    void propagate();

    void set_boundary_adjustments(void);
    // void setFirstTSC(const String_Split &line, const PEAT_Rank rank);
    // void setFirstParTSC(const String_Split &line, const PEAT_Rank rank);
    // void setLastTSC(const String_Split &line, const PEAT_Rank rank);
    // void setLastParTSC(const String_Split &line, const PEAT_Rank rank);
    PEAT_Ratio get_trace_freq_scale(const PEAT_Rank rank) const
    {
        return freq_scale.at(rank);
    }
    void resetPTPDRecords()
    { // if we have ptpd file we need to clear syncpoints defined in app file
        PTPD_master_adjustments.clear();
        PTPD_deliveries.clear();
        parsed_PTPD_records.clear();
    }
    void sync_points_to_PTPD_records();
    void update_boundaries(const PEAT_Rank rank, const std::map<PEAT_Num, Times_Sync> &records, const PEAT_Num index);
    void consistency_check();
    void parse(const String_Split &line, PEAT_Rank rank); // temp
    PEAT_Bool parse_node_map(File_Storage_GZ &input_map);

    std::map<PEAT_Rank, std::string> MPM_host_names;
    std::map<std::string, PEAT_Node> map_host_names;
    std::map<PEAT_Rank, PEAT_FREQ> MPM_proc_freq;
    std::map<PEAT_Rank, std::string> PTPD_host_names; // used in consystency check only
    std::map<PEAT_Rank, PEAT_FREQ> PTPD_proc_freq;
    std::map<PEAT_Rank, PEAT_Clock> rank_MPI;

    std::map<PEAT_Rank, std::vector<PEAT_Rank>> MPM_SMP_grp;
    std::vector<PEAT_Rank> MPMMasterGrp;
    std::vector<PEAT_Rank> MPMMaster2CW;
    std::vector<PEAT_Rank> PTPDMasterGrp; // not really used

    std::map<PEAT_Rank, std::map<PEAT_Num, Times_Sync>> PTPD_master_adjustments; // map<rank, <curNumber, PTP time struct> >
    std::map<PEAT_Rank, PEAT_Num> parsed_PTPD_records;                           // Number of elements in PTPD_deliveries/PTPD_master_adjustments for particular rank. BAD. No initialization. TODO
    std::map<PEAT_Rank, PEAT_Rank> SMP_to_master_src;
    std::map<PEAT_Rank, PEAT_Node> master_to_node_src;
    std::map<PEAT_Rank, PEAT_Proc> rank_to_proc_src;
    std::map<PEAT_Node, Node_Info> node_info_src;

    std::map<PEAT_Rank, PEAT_Rank> SMP_to_master_tgt;
    std::map<PEAT_Rank, PEAT_Node> master_to_node_tgt;
    std::map<PEAT_Rank, PEAT_Proc> rank_to_proc_tgt;
    std::map<PEAT_Node, Node_Info> node_info_tgt;

    std::map<PEAT_Rank, PEAT_Clock> global_sync_point;
    std::map<PEAT_Rank, PEAT_Clock> local_sync_point;
    std::map<PEAT_Rank, PEAT_Clock> global_durations;
    std::map<PEAT_Rank, PEAT_Clock> local_durations;

    std::map<PEAT_Rank, PEAT_Num> lower_boundaries_index;
    std::map<PEAT_Rank, PEAT_Clock> upper_boundaries;
    std::map<PEAT_Rank, PEAT_Clock> start_sync, end_sync;

    std::map<PEAT_Rank, PEAT_Ratio> freq_scale;

    std::map<PEAT_Rank, std::map<PEAT_Num, PEAT_Clock>> PTPD_deliveries; // map<rank, <curNumber, (T2-T3) + (T4-T1) * thatFreq / thisFreq> >
  public:
    std::map<PEAT_Rank, Times_Sync> sync_times;

    PEAT_Clock global_start;
    PEAT_FREQ InpFreq;
    PEAT_Rank master_node;
};

#endif // __PTP_H__
