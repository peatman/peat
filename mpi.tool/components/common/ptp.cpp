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

#include <assert.h>
#include <iostream>
#include <sstream>
#include <stdexcept>

#include "ptp.h"
#include "utils.h"

#define PTPCERR          \
    if (globals.verbose) \
    std::cerr
#define PTPDBG(x)             \
    if (globals.verbose >= x) \
    std::cerr
#define rankANDnode(x) "rank " << x << " on node " << getHostName(x)

PTP::PTP(const Global_Parameters &glob) : globals(glob), cvt_to_ns(false)
{
    global_start = 0;
    master_node = 0;
    InpFreq = 0LL;
}

void PTP::sync_points_to_PTPD_records()
{
    master_node = 0; // since we got here there is no PTP Daemon started
    if (!start_sync.empty()) {
        std::map<PEAT_Rank, PEAT_Clock> TSync;
        PEAT_Rank ranks = (PEAT_Rank)start_sync.size();
        for (PEAT_Rank i = 0; i < ranks; ++i) {
            PEAT_Clock tsc = start_sync.at(i);
            PEAT_Rank master_rank = i; // SMP_to_master_src.at(i);
            if (TSync.count(master_rank) < 1)
                TSync[master_rank] = tsc;
            if (TSync.at(master_rank) > tsc)
                TSync[master_rank] = tsc;
        }
        PEAT_Clock tsc0 = TSync.at(0);
        for (std::map<PEAT_Rank, PEAT_Clock>::iterator it = TSync.begin(); it != TSync.end(); ++it) {
            Times_Sync ptpd_times;
            ptpd_times.t1 = tsc0;
            ptpd_times.t2 = it->second;
            ptpd_times.t3 = it->second;
            ptpd_times.t4 = tsc0;
            parse_PTPD_record(it->first, ptpd_times);
        }
    }
    if (!end_sync.empty()) {
        std::map<PEAT_Rank, PEAT_Clock> TSync;
        PEAT_Rank ranks = (PEAT_Rank)end_sync.size();
        for (PEAT_Rank i = 0; i < ranks; ++i) {
            PEAT_Clock tsc = end_sync.at(i);
            PEAT_Rank master_rank = i; // SMP_to_master_src.at(i);
            if (TSync.count(master_rank) < 1)
                TSync[master_rank] = tsc;
            if (TSync.at(master_rank) < tsc)
                TSync[master_rank] = tsc;
        }
        PEAT_Clock tsc0 = TSync.at(0);
        for (std::map<PEAT_Rank, PEAT_Clock>::iterator it = TSync.begin(); it != TSync.end(); ++it) {
            Times_Sync ptpd_times;
            ptpd_times.t1 = tsc0;
            ptpd_times.t2 = it->second;
            ptpd_times.t3 = it->second;
            ptpd_times.t4 = tsc0;
            parse_PTPD_record(it->first, ptpd_times);
        }
    }
}
void PTP::parse(const String_Split &line, PEAT_Rank rank)
{
    if (line.size() < 2) {
        return;
    }
    PEAT_Record_Type event = PEAT_RT_Remark;
    extract(event, line.at(0));
    event = m_id->translate_record_type(event);
    switch (event) {
    // MPM Events:
    case PEAT_RT_MPM_Master_Ranks: {
        PEAT_Rank ranks = 0;
        extract(ranks, line.at(2));
        // PTPCERR << "Adding Master GRP record. Ranks in Group " << ranks << endl;
        for (PEAT_Num i = 2; i < line.size(); ++i) { // skip MSE,id,tid,ranks
            PEAT_Rank grpRank = 0;
            extract(grpRank, line.at((int)i));
            MPMMasterGrp.push_back(grpRank);
        }
    } break;
    case PEAT_RT_CPU_Freq: {
        PEAT_FREQ rankFreq = 0;
        extract(rankFreq, line.at(2));
        PTPD_proc_freq[rank] = rankFreq;
    } break;
    case PEAT_RT_MPM_Nodes_Freq: {
        if (line.size() != 7) {
            PTPCERR << "MPM misc format changed. Expected 7 columns in app file, but " << line.size() << " exists\n";
        }
        PEAT_Rank masterRank = 0;
        extract(masterRank, line.at(1)); // rank in master comm
        PEAT_Rank rank = 0;
        extract(rank, line.at(2)); // rank in comm world
        PEAT_FREQ rankFreq = 0;
        extract(rankFreq, line.at(3)); // frequency
                                       // old variant    PEAT_FREQ rankFreq = 0; extract(rankFreq, line.at(5)); //frequency
        MPM_host_names[rank] = line.at(6);
        MPM_proc_freq[rank] = rankFreq;
        if (MPMMaster2CW.size() == masterRank) {
            MPMMaster2CW.push_back(rank);
        } else {
            throw std::logic_error("Master Rank mismatch.");
        }
    } break;
    case PEAT_RT_MPM_Ranks_And_PPN: {
        PEAT_Rank nodePPN = 0;
        extract(nodePPN, line.at(3));
        PEAT_Rank nodeID = 0;
        extract(nodeID, line.at(4));
        nodes_PPN[nodeID] = nodePPN;

        std::string ranksLine = line.at(5);
        if (!ranksLine.empty()) {
            PEAT_Rank masterRank = 0;
            extract(masterRank, ranksLine.c_str());
            std::size_t linePos = 0;
            master_to_node_src[masterRank] = nodeID;
            // PTPCERR << "Adding SMP GRP record for " << rankANDnode(rank) << " Ranks in Group " << ranks << endl;
            for (int i = 0; i < nodePPN; ++i) {
                PEAT_Rank smpRank = 0;
                extract(smpRank, ranksLine.c_str() + linePos);
                MPM_SMP_grp[masterRank].push_back(smpRank);
                SMP_to_master_src[smpRank] = masterRank;
                if (MPM_host_names.count(masterRank)) {
                    MPM_host_names[smpRank] = MPM_host_names.at(masterRank);
                } else {
                    throw std::runtime_error("Have no MPM hostnames in PTP::parseMPM_SMP_grpRecord");
                }
                linePos = ranksLine.find_first_of(":,", linePos);
                if (std::string::npos != linePos) {
                    ++linePos;
                } else {
                    break;
                }
            }
        }
    } break;
    case PEAT_RT_PTP_Ranks: {
        PEAT_Rank ranks = 0;
        extract(ranks, line.at(2));
        // PTPCERR << "Adding Master GRP record. Ranks in Group " << ranks << endl;
        for (PEAT_Num i = 3 - master_node; i < line.size(); ++i) { // skip MSE,id,tid,ranks and master node if present
            PEAT_Rank grpRank = 0;
            extract(grpRank, line.at((int)i));
            PTPDMasterGrp.push_back(grpRank + master_node);
        }
    } break;
    case PEAT_RT_MPM_All_Ranks_Exec_Time:
        PTPDBG(2) << "PTP:parse. Tag is deprecated: " << event << " line " << line << std::endl;
        break;
    case PEAT_RT_Sync_Point: {
        if (rank < 0) {
            if (start_sync.empty()) {
                PEAT_Rank ranks = 0;
                extract(ranks, line.at(1));
                for (PEAT_Rank i = 0; i < ranks; ++i) {
                    PEAT_Clock tsc = 0;
                    extract(tsc, line.at(i + 2));
                    start_sync[i] = tsc;
                }
                break;
            }
            if (end_sync.empty()) {
                PEAT_Rank ranks = 0;
                extract(ranks, line.at(1));
                for (PEAT_Rank i = 0; i < ranks; ++i) {
                    PEAT_Clock tsc = 0;
                    extract(tsc, line.at(i + 2));
                    end_sync[i] = tsc;
                }
                break;
            }
        } 
    } break;
    case PEAT_RT_MPM_Rank_Time_Stamp: {
        Times_Sync times;
        extract(times.t1, line.at(1));
        extract(times.t2, line.at(2));
        extract(times.t3, line.at(3));
        extract(times.t4, line.at(4));
        sync_times[rank] = times;
    } break;
    case PEAT_RT_PTP_Main_Record: {
        PEAT_Rank rank = 0;
        extract(rank, line.at(1));
        Times_Sync ptpd_times;
        extract(ptpd_times.t1, line.at(2));
        extract(ptpd_times.t2, line.at(3));
        extract(ptpd_times.t3, line.at(4));
        extract(ptpd_times.t4, line.at(5));
        parse_PTPD_record(rank, ptpd_times);
        if (global_start == 0 && master_node != 0) {
            global_start = ptpd_times.t1;
        }
        break;
    }
    case PEAT_RT_PTP_Mode:
        extract(master_node, line.at(1));
        master_node = -master_node;
        break;
    case PEAT_RT_PTP_Nodes_Freq: {
        if (line.size() != 5) {
            PTPCERR << "PTPD misc format changed. Expected 5 columns in ptp file, but " << line.size() << " exists\n";
        }
        PEAT_Rank rank = 0;
        extract(rank, line.at(1)); // rank in demon comm world
        PEAT_FREQ rankFreq = 0;
        extract(rankFreq, line.at(2)); // frequency
                                       //     PEAT_FREQ rankFreq = 0; extract(rankFreq, line.at(3)); //frequency
        PTPD_host_names[rank + master_node] = line.at(4);
        PTPD_proc_freq[rank + master_node] = rankFreq;
    } break;
    case PEAT_RT_HWLOC_Node_Record: {
        PEAT_Num num_packages = line.size() - 1;
        PEAT_Node node_id = get_src_node(rank);
        for (PEAT_Num i = 0; i < num_packages; i++) {
            const char *package = line.at(i + 1);
            char *endptr;
            PEAT_Proc package_id = strtol(package, &endptr, 10);
            while (*endptr++ == ',') {
                PEAT_Proc core_id = strtol(endptr, &endptr, 10);
                while (*endptr++ == '-') {
                    PEAT_Proc pu_id = strtol(endptr, &endptr, 10);
                    PU_Info pu_info(core_id, package_id);
                    node_info_src[node_id].proc_units[pu_id] = pu_info;
                }
                endptr--;
            }
        }
    } break;
    case PEAT_RT_HWLOC_Proc_Record: {
        PEAT_Proc proc_id;
        extract(proc_id, line.at(1));
        rank_to_proc_src[rank] = proc_id;
    } break;
    case PEAT_RT_Non_Existent:
        std::cerr << "Tag is ignored: " << event << " line " << line << std::endl;
        break;
    default:
        PTPDBG(2) << "PTP:parse. Tag is ignored: " << event << " line " << line << std::endl;
        break;
    }
    return;
}

void PTP::initialize(const std::map<PEAT_Rank, std::string> &commonfile_names, const std::map<PEAT_Rank, std::string> &ptpfile_names, const std::map<PEAT_Rank, std::string> appfile_names, const Model_ID *id)
{
    m_id = id;
    try {

        File_Storage_GZ app_files(appfile_names, PEAT_FIO_FLAG_READ | PEAT_FIO_FLAG_LAZY | PEAT_FIO_FLAG_ICLOSE, NULL, globals.verbose);
        String_Split lines;
        PEAT_Rank app_num = (PEAT_Rank)app_files.get_number_of_files();
        if (app_num != 1)
            peat_throw_error("Incorrect Numbers of .app File\n");
        PEAT_Bool hasMoreLines = true;
        while (hasMoreLines) {
            hasMoreLines = app_files.read_lines(lines, 0, ':');
            parse(lines, -1);
        }
        File_Storage_GZ com_ext(commonfile_names, PEAT_FIO_FLAG_READ | PEAT_FIO_FLAG_LAZY | PEAT_FIO_FLAG_ICLOSE, NULL, globals.verbose);
        if (!com_ext.is_any_file_good()) {
            std::ostringstream errTxt;
            errTxt << "ptp::initialize() Trace is not usable. Can't open files " << commonfile_names << std::endl;
            throw std::runtime_error(errTxt.str());
        }
        PEAT_Rank app_rank_num = (PEAT_Rank)com_ext.get_number_of_files();

        if (globals.rank_set.empty()) {
            for (PEAT_Rank i = 0; i < app_rank_num; i++) {
                PEAT_Bool hasMoreLines = true;
                while (hasMoreLines) {
                    hasMoreLines = com_ext.read_lines(lines, i, ':');
                    parse(lines, i);
                }
            }
        } else {
            for (std::set<PEAT_Rank>::const_iterator rank_it = globals.rank_set.cbegin(); rank_it != globals.rank_set.cend(); ++rank_it) {
                PEAT_Bool hasMoreLines = true;
                while (hasMoreLines) {
                    hasMoreLines = com_ext.read_lines(lines, *rank_it, ':');
                    parse(lines, *rank_it);
                }
            }
        }

        File_Storage_GZ ptpd_files(ptpfile_names, PEAT_FIO_FLAG_READ | PEAT_FIO_FLAG_LAZY | PEAT_FIO_FLAG_ICLOSE, NULL, globals.verbose);

        PEAT_Rank ptpd_num = (PEAT_Rank)ptpd_files.get_number_of_files();
        if (ptpd_num > 0) {
            resetPTPDRecords();
        }
        for (PEAT_Rank i = 0; i < ptpd_num; i++) {
            PEAT_Bool hasMoreLines = true;
            while (hasMoreLines) {
                hasMoreLines = ptpd_files.read_lines(lines, i, ':');
                parse(lines, i);
            }
        }
    } catch (const std::runtime_error &e) {
        std::cerr << "Exceptions raised while PTP initialization:" << e.what() << "\n";
    }
    if (1 < MPM_SMP_grp.size()) { // if multinode
        if (MPM_SMP_grp.size() != PTPD_host_names.size() && PTPD_host_names.size() > 0) {
            std::cerr << "ERROR. Diffrent number of nodes in PTPD and trace. MPM_SMP_grp.size()=" << MPM_SMP_grp.size() << " PTPD_host_names.size()=" << PTPD_host_names.size() << "\n";
        }
    }
    make_initial_adjustments();
    set_boundary_adjustments();
    consistency_check();
    propagate();
}

void PTP::make_initial_adjustments(void)
{
    if (PTPD_master_adjustments.empty()) {
        if (!MPM_proc_freq.empty()) {
            if (PTPD_proc_freq.empty())
                PTPD_proc_freq = MPM_proc_freq;
            sync_points_to_PTPD_records();
        } else {
            throw std::runtime_error("Have no MPM ProcFreq in PTP::makeInitialAdjustments.");
        }
    }
    const std::map<PEAT_Num, Times_Sync> &record = PTPD_master_adjustments.begin()->second;
    global_sync_point[master_node] = record.at(0).t1;
    local_sync_point[master_node] = global_sync_point[master_node];
}

PEAT_Bool PTP::is_SMP_src(const PEAT_Rank rank1, const PEAT_Rank rank2) const
{
    PEAT_Bool ret = false;
    if (!SMP_to_master_src.empty()) {
        if (SMP_to_master_src.count(rank1) && SMP_to_master_src.count(rank2)) {
            ret = (SMP_to_master_src.at(rank1) == SMP_to_master_src.at(rank2));
        } else {
            PTPCERR << "PTP::is_SMP_src. No requested ranks available rank1: "
                    << rank1 << "(" << SMP_to_master_src.count(rank1) << ") rank2: "
                    << rank2 << "(" << SMP_to_master_src.count(rank2) << ")\n";
        }
    } else {
        ret = true; // PTPCERR << "No data in SMP_to_master available. SMP comminucation assumed.\n";
    }
    return ret;
}
PEAT_Bool PTP::is_SMP_tgt(const PEAT_Rank rank1, const PEAT_Rank rank2) const
{
    PEAT_Bool ret = false;
    if (!SMP_to_master_tgt.empty()) {
        if (SMP_to_master_tgt.count(rank1) && SMP_to_master_tgt.count(rank2)) {
            ret = (SMP_to_master_tgt.at(rank1) == SMP_to_master_tgt.at(rank2));
        } else {
            PTPCERR << "PTP::is_SMP_tgt. No requested ranks available rank1: "
                    << rank1 << "(" << SMP_to_master_tgt.count(rank1) << ") rank2: "
                    << rank2 << "(" << SMP_to_master_tgt.count(rank2) << ")\n";
        }
    } else {
        ret = true; // PTPCERR << "No data in SMP_to_master available. SMP comminucation assumed.\n";
    }
    return ret;
}

PEAT_Node PTP::get_src_node(PEAT_Rank rank) const
{
    PEAT_Node ret = 0;
    if (SMP_to_master_src.count(rank)) {
        ret = master_to_node_src.at(SMP_to_master_src.at(rank));
    } else {
        PTPCERR << "PTP::get_src_node. No requested ranks available, rank "
                << rank << "(" << SMP_to_master_src.count(rank) << ")\n";
    }
    return ret;
}
PEAT_Node PTP::get_tgt_node(PEAT_Rank rank) const
{
    PEAT_Node ret = 0;
    if (SMP_to_master_tgt.count(rank)) {
        ret = master_to_node_tgt.at(SMP_to_master_tgt.at(rank));
    } else {
        PTPCERR << "PTP::get_tgt_node. No requested ranks available, rank "
                << rank << "(" << SMP_to_master_tgt.count(rank) << ")\n";
    }
    return ret;
}

PEAT_Proc PTP::get_src_package(PEAT_Rank rank)
{
    PEAT_Node node_id = get_src_node(rank);
    PEAT_Proc proc_id;
    if (rank_to_proc_src.count(rank)) {
        proc_id = rank_to_proc_src.at(rank);
        if (node_info_src[node_id].proc_units.count(proc_id)) {
            PEAT_Proc package_id = node_info_src[node_id].proc_units.at(proc_id).package_id;
            return package_id;
        }
    } else {
        PTPCERR << "PTP::get_src_package. No requested ranks available, rank "
                << rank << "(" << rank_to_proc_src.count(rank) << ")\n";
    }
    return -1;
}
PEAT_Proc PTP::get_tgt_package(PEAT_Rank rank)
{
    PEAT_Node node_id = get_tgt_node(rank);
    PEAT_Proc proc_id;
    if (rank_to_proc_tgt.count(rank)) {
        proc_id = rank_to_proc_tgt.at(rank);
        if (node_info_tgt[node_id].proc_units.count(proc_id)) {
            PEAT_Proc package_id = node_info_tgt[node_id].proc_units.at(proc_id).package_id;
            return package_id;
        }
    } else {
        PTPCERR << "PTP::get_tgt_package. No requested ranks available, rank "
                << rank << "(" << rank_to_proc_tgt.count(rank) << ")\n";
    }
    return -1;
}

PEAT_Num PTP::get_numa_pp(PEAT_Node node, PEAT_Node numa_node)
{

    PEAT_Numa_Node nn_info(node, numa_node);
    return numa_nodes_PPN.at(nn_info);
}

void PTP::parse_PTPD_record(PEAT_Rank rank, const Times_Sync &ptpd_times)
{
    PEAT_Rank refrank = rank; // SMP_to_master_src.at(rank);
    PEAT_Rank freqfrank = SMP_to_master_src.at(rank);
    // assert(refrank == rank); //they should be the same now
    PTPD_master_adjustments[refrank][parsed_PTPD_records[refrank]] = ptpd_times;
    PEAT_FREQ freq_rank = PTPD_proc_freq.count(freqfrank) == 1 ? PTPD_proc_freq.at(freqfrank) : MPM_proc_freq.count(freqfrank);
    PEAT_FREQ freq_master_node = PTPD_proc_freq.count(master_node) == 1 ? PTPD_proc_freq.at(master_node) : MPM_proc_freq.at(master_node);

    PTPD_deliveries[refrank][parsed_PTPD_records[refrank]] = (ptpd_times.t2 - ptpd_times.t3) + (ptpd_times.t4 - ptpd_times.t1) * freq_rank / freq_master_node;
    //    PTPCERR << "Adding PTPD Master record" << parsed_PTPD_records[refrank] << " for " << rankANDnode(refrank) << ".Delivery  time : " << PTPD_deliveries.at(refrank).at(parsed_PTPD_records.at(refrank)) << " clocks." << endl;
    parsed_PTPD_records[refrank]++;
}

void PTP::update_boundaries(const PEAT_Rank rank, const std::map<PEAT_Num, Times_Sync> &records, const PEAT_Num index)
{
    const Times_Sync &rec = records.at(index);
    const Times_Sync &recP = records.at(index + 1);
    local_durations[rank] = ((recP.t2 + recP.t3) / 2) - ((rec.t2 + rec.t3) / 2);  //(l_r)
    global_durations[rank] = ((recP.t1 + recP.t4) / 2) - ((rec.t1 + rec.t4) / 2); //(l_0)
    local_sync_point[rank] = ((rec.t2 + rec.t3) / 2);
    global_sync_point[rank] = ((rec.t4 + rec.t1) / 2);
    upper_boundaries[rank] = ((rec.t2 + rec.t3) / 2);
    lower_boundaries_index[rank] = index;
}

void PTP::set_boundary_adjustments(void)
{
    for (std::map<PEAT_Rank, std::map<PEAT_Num, Times_Sync>>::const_iterator it = PTPD_master_adjustments.begin(); it != PTPD_master_adjustments.end(); ++it) {
        const PEAT_Rank rank = it->first;
        const std::map<PEAT_Num, Times_Sync> &records = it->second;
        update_boundaries(rank, records, 0);
    }
}
// #define _NOPTPSYNC_
PEAT_Clock PTP::get_real_time(const PEAT_Clock tsc_in, const PEAT_Rank rank_in)
{
#ifdef _NOPTPSYNC_
    return tsc_in;
#endif
    PEAT_Clock tsc = tsc_in;
    PEAT_Rank rank = rank_in; // SMP_to_master_src.at(rank_in);
    /*
    if(rank == master_node) {
        tsc -= FirstTSC.at(master_node) - ExtraShift;
        if(cvt_to_ns) {
            double tsc_d = tsc * 1000000000.0 / PTPD_proc_freq.at(rank);
            tsc = (PEAT_Clock)tsc_d;
        }
        return tsc;
    }
    if(rank == master_node) {
        tsc -= global_start;
        if(cvt_to_ns) {
            double tsc_d = tsc * 1000000000.0 / MPM_proc_freq.at(SMP_to_master_src.at(rank));
            tsc = (PEAT_Clock)tsc_d;
        }
        return tsc;
    }*/
    while (tsc > upper_boundaries.at(rank) && lower_boundaries_index.at(rank) + 2 < parsed_PTPD_records.at(rank)) {
        // update ptpd values
        const std::map<PEAT_Num, Times_Sync> &records = PTPD_master_adjustments.at(rank); // (SMP_to_master_src.at(rank));
        update_boundaries(rank, records, lower_boundaries_index.at(rank) + 1);
    }
    if (tsc - local_sync_point.at(rank) < 0) {
        //        strcerr << "Reverse move is detected in rank " << rank << "." << endl;
        while (tsc - local_sync_point.at(rank) < 0 && lower_boundaries_index.at(rank) > 0) {
            // update ptpd values
            const std::map<PEAT_Num, Times_Sync> &records = PTPD_master_adjustments.at(rank);
            update_boundaries(rank, records, lower_boundaries_index.at(rank) - 1);
        }
    }
    PEAT_Clock l_r = local_durations.at(rank);
    PEAT_Clock l_0 = global_durations.at(rank);
    PEAT_Clock d_r = tsc - local_sync_point.at(rank);
    PEAT_Clock d_0 = (PEAT_Clock)(((double)d_r / (double)l_r) * (double)l_0);
    tsc = global_sync_point.at(rank) + d_0 - global_start;
    // last thing is conversion to nanoseconds
    if (cvt_to_ns) {
        double tsc_d = tsc * 1000000000.0 / PTPD_proc_freq.at(rank); // (SMP_to_master_src.at(rank));
        tsc = (PEAT_Clock)tsc_d;
    }
    return tsc;
}

std::string PTP::get_host_name(PEAT_Rank rank) const
{
    if (MPM_host_names.count(rank)) {
        return MPM_host_names.at(rank);
    } else {
        return "Unavailable";
    }
}

void PTP::consistency_check()
{
    if (PTPD_host_names.empty() && MPMMasterGrp.size() > 1) {
        std::cerr << "Multinode run without PTPD." << std::endl;
        //        abort();
    } else {
        for (std::map<PEAT_Rank, std::string>::iterator it = PTPD_host_names.begin(); it != PTPD_host_names.end(); ++it) {
            PEAT_Rank rank = it->first;
            if (rank < 0) {
                continue; // skip dedicated node
            }
            if (MPMMasterGrp.at(rank) != MPMMaster2CW.at(PTPDMasterGrp.at(rank))) {
                // cerr << "DEBUG MPM " << MPMMasterGrp << "\n PTP " << MPMMaster2CW << "\nrank " << rank << "\nPTPDMasterGrp " << PTPDMasterGrp << "\n";
                throw std::logic_error("Consistency check failed. Rank mismatch.");
            }
            if (MPM_host_names.at(MPMMaster2CW.at(rank)) != PTPD_host_names.at((rank))) {
                throw std::logic_error("Consistency check failed. Hostnames mismatch.");
            }
            if (MPM_proc_freq.at(MPMMaster2CW.at(rank)) != PTPD_proc_freq.at((rank))) {
                // cerr << "DEBUG MPM " << MPM_proc_freq << "\n PTP " << PTPD_proc_freq << "\n";
                throw std::logic_error("Consistency check failed. Frequency mismatch.");
            }
        }
        // remove "unhealthy" measurements.
        for (std::map<PEAT_Rank, std::map<PEAT_Num, PEAT_Clock>>::iterator it = PTPD_deliveries.begin(); it != PTPD_deliveries.end(); ++it) {
#ifndef PTPD_DELIVERY_HARD_LIMIT
            // calculate "average" delivery
            PEAT_Clock average = 0;
            for (std::map<PEAT_Num, PEAT_Clock>::iterator mesIt = it->second.begin(); mesIt != it->second.end(); ++mesIt) {
                average += mesIt->second;
            }
            average /= parsed_PTPD_records.at(it->first);
            PTPDBG(2) << "Rank:" << it->first << ". Average Delivery:" << average << " Clocks.\n";
            const double ratio = 1.1;
            const PEAT_Clock upper_bound = (PEAT_Clock)(average * ratio);
#else
            const PEAT_Clock upper_bound = (PEAT_Clock)PTPD_DELIVERY_HARD_LIMIT;
#endif
            PEAT_Num deletedRecords = 0;
            for (std::map<PEAT_Num, PEAT_Clock>::iterator mesIt = it->second.begin(); mesIt != it->second.end(); ++mesIt) {
                if (upper_bound < mesIt->second * ratio) {
                    PTPD_master_adjustments.at(it->first).erase(mesIt->first);
                    parsed_PTPD_records.at(it->first)--;
                    deletedRecords++;
                };
            }
            // compress measurement map
            PEAT_Num totalRec = 0;
            std::map<PEAT_Num, Times_Sync> tempPTPDRecs;
            for (std::map<PEAT_Num, Times_Sync>::iterator mesIt = PTPD_master_adjustments.at(it->first).begin(); mesIt != PTPD_master_adjustments.at(it->first).end(); ++mesIt) {
                tempPTPDRecs[totalRec++] = mesIt->second;
            }
            assert(totalRec == parsed_PTPD_records.at(it->first));
            PTPD_master_adjustments.erase(it->first);
            PTPD_master_adjustments[it->first] = tempPTPDRecs;
            PTPDBG(2) << "Rank:" << it->first << " Deleted " << deletedRecords << " Records(total " << it->second.size() << ") Delivery threshold:" << upper_bound << "\n";
        }
    }
    if (global_start == 0) {
        PEAT_Clock g_start = LLONG_MAX;
        for (std::map<PEAT_Rank, Times_Sync>::const_iterator sync_times_it = sync_times.cbegin(); sync_times_it != sync_times.cend(); ++sync_times_it) {
            PEAT_Rank r = sync_times_it->first;
            PEAT_Clock start = sync_times_it->second.t1;
            start = get_real_time(start, r);
            if (start < g_start)
                g_start = start;
        }
        global_start = g_start;
        // assert(global_start >= 0);
    };
    for (std::map<PEAT_Rank, Times_Sync>::iterator sync_times_it = sync_times.begin(); sync_times_it != sync_times.end(); ++sync_times_it) {
        PEAT_Rank r = sync_times_it->first;
        sync_times_it->second.t1 = get_real_time(sync_times_it->second.t1, r);
        sync_times_it->second.t2 = get_real_time(sync_times_it->second.t2, r);
        sync_times_it->second.t3 = get_real_time(sync_times_it->second.t3, r);
        sync_times_it->second.t4 = get_real_time(sync_times_it->second.t4, r);
    }

    for (std::map<PEAT_Rank, PEAT_Proc>::const_iterator rank_it = rank_to_proc_src.cbegin(); rank_it != rank_to_proc_src.cend(); ++rank_it) {
        PEAT_Numa_Node nn_info;
        PEAT_Rank rank = rank_it->first;
        nn_info.node = get_src_node(rank);
        nn_info.numa_node = get_src_package(rank);
        numa_nodes_PPN[nn_info]++;
    }
    //	if (global_start == 0) { global_start = record.at(0).t1 - MPM_proc_freq.size() * 10 * MPM_proc_freq.at(master_node); };
}
PEAT_Bool PTP::parse_node_map(File_Storage_GZ &input_map)
{
    String_Split line;
    map_host_names.clear();
    PEAT_Node node_counter = 0;
    while ((input_map.is_any_file_good())) {
        input_map.read_lines(line, 0, ':');
        if (line.size() < 2) {
            continue;
        }
        PEAT_Rank rank = 0;
        PEAT_Node node = 0;
        // rank:node:numa_node

        extract(rank, line.at(0));
        std::string tmp_node = line.at(1);
        if (map_host_names.count(tmp_node) == 1) {
            node = map_host_names.at(tmp_node);
            SMP_to_master_tgt[rank] = rank;
        } else {
            node = node_counter++;
            map_host_names[tmp_node] = node;
        }
    }
    return true;
}

void PTP ::propagate()
{
    if (globals.target_numa_pp > 0 && globals.target_ppn > 0) {
        int tgt_ppn = globals.target_ppn, tgt_numa_pp = globals.target_numa_pp;
        int node = -1, last_master = 0, last_package = -1;
        // int numa_nodes_per_node = tgt_ppn / tgt_numa_pp;
        for (int rank = 0; rank < get_rank_num(); rank++) {
            if (rank % tgt_ppn == 0) {
                // master_rank
                master_to_node_tgt[rank] = ++node;
                last_master = rank;
                last_package = 0;
            }
            SMP_to_master_tgt[rank] = last_master;
            PEAT_Proc core_id = rank % tgt_numa_pp;

            if (core_id == 0) {
                last_package++;
            }
            PEAT_Proc pu_id = core_id;
            rank_to_proc_tgt[rank] = core_id;
            PU_Info pu_info(core_id, last_package);
            node_info_tgt[node].proc_units[pu_id] = pu_info;
        }
        numa_nodes_PPN.clear();
        for (std::map<PEAT_Rank, PEAT_Proc>::const_iterator rank_it = rank_to_proc_tgt.cbegin(); rank_it != rank_to_proc_tgt.cend(); ++rank_it) {
            PEAT_Numa_Node nn_info;
            PEAT_Rank rank = rank_it->first;
            nn_info.node = get_tgt_node(rank);
            nn_info.numa_node = get_tgt_package(rank);
            numa_nodes_PPN[nn_info]++;
        }
        return;
    }
    if (!globals.map_file.empty()) {
        File_Storage_GZ input_map(globals.input_path + DIR_DELIMITER + globals.map_file, PEAT_FIO_FLAG_READ | PEAT_FIO_FLAG_LAZY | PEAT_FIO_FLAG_ICLOSE, NULL, globals.verbose);
        if (parse_node_map(input_map) == 0)
            return;
    }

    SMP_to_master_tgt = SMP_to_master_src;
    master_to_node_tgt = master_to_node_src;
    rank_to_proc_tgt = rank_to_proc_src;
    node_info_tgt = node_info_src;
}