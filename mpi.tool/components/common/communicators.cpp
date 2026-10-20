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

#include <algorithm>
#include <limits>
#include <sstream>
#include <stdexcept>

#include <assert.h>

#include "communicators.h"
#include "utils.h"

Communicators::Communicators(const Global_Parameters &glob) : globals(glob), m_id(NULL)
{
    CMPI_SUCCESS = -1;
    CMPI_UNDEFINED = -1;
    CMPI_COMM_WORLD = -1;
    CMPI_COMM_SELF = -1;
    CMPI_COMM_NULL = -1;
    CMPI_PROC_NULL = -1;
    CMPI_ANY_TAG = -1;
    CMPI_ANY_SOURCE = -1;
    CMPI_REQUEST_NULL = -1;
    frame_tsc_field_size = 0;
    frame_hw_field_size = 0;
    app_rank_num = 1;
    commworld_rank_num = 1;
}

void Communicators::initialize(const std::map<PEAT_Rank, std::string> &commonfile_names, const std::map<PEAT_Rank, std::string> appfile_names, const Model_ID *id)
{
    m_id = id;
    File_Storage_GZ app_files(appfile_names, PEAT_FIO_FLAG_READ | PEAT_FIO_FLAG_LAZY | PEAT_FIO_FLAG_ICLOSE, NULL, globals.verbose);
    if (!app_files.is_any_file_good()) {
        std::ostringstream errTxt;
        errTxt << "Communicators::initialize() Trace is not usable. Can't open files " << commonfile_names << std::endl;
        throw std::runtime_error(errTxt.str());
    }
    lines.clear();

    PEAT_Rank app_num = (PEAT_Rank)app_files.get_number_of_files();
    for (PEAT_Rank i = 0; i < app_num; i++) {
        PEAT_Bool hasMoreLines = true;
        while (hasMoreLines) {
            hasMoreLines = app_files.read_lines(lines, i, ':');
            parse(lines, i, true);
        }
    }

    File_Storage_GZ com_ext(commonfile_names, PEAT_FIO_FLAG_READ | PEAT_FIO_FLAG_LAZY | PEAT_FIO_FLAG_ICLOSE, NULL, globals.verbose);
    if (!com_ext.is_any_file_good()) {
        std::ostringstream errTxt;
        errTxt << "Communicators::initialize() Trace is not usable. Can't open files " << commonfile_names << std::endl;
        throw std::runtime_error(errTxt.str());
    }
    app_rank_num = (PEAT_Rank)com_ext.get_number_of_files();
    if (globals.rank_set.empty()) {
        for (PEAT_Rank i = 0; i < app_rank_num; i++) {
            PEAT_Bool hasMoreLines = true;
            while (hasMoreLines) {
                hasMoreLines = com_ext.read_lines(lines, i, ':');
                parse(lines, i, false);
            }
        }
    } else {
        for (std::set<PEAT_Rank>::const_iterator rank_it = globals.rank_set.cbegin(); rank_it != globals.rank_set.cend(); ++rank_it) {
            PEAT_Bool hasMoreLines = true;
            while (hasMoreLines) {
                hasMoreLines = com_ext.read_lines(lines, *rank_it, ':');
                parse(lines, *rank_it, false);
            }
        }
    }

    // build shift for real time traces
    // PEAT_Clock minimumTime = LLONG_MAX;
    // PEAT_Clock firstAppTSC = LLONG_MAX;
    PEAT_Clock maximumCorrectionTSC = 0;
    for (std::map<PEAT_Rank, Time_Stamps>::const_iterator it = time_stamps.begin(); it != time_stamps.end(); ++it) {
        const Time_Stamps &stamps = it->second;
        // printf("Rank %d after_init time %lld first TSC %lld\n", it->first, stamps.after_init_time, stamps.first_tsc);
        maximumCorrectionTSC = __max__(maximumCorrectionTSC, stamps.after_init_tsc - stamps.first_tsc); // max distance beetween bench mark and the first mark
        // if(minimumTime > stamps.after_init_time) {
        //     minimumTime = stamps.after_init_time;
        //     firstAppTSC = stamp.after_init_tsc;
        // }
        // minimumTime = max(minimumTime, stamps.after_init_time);
        // firstAppTSC = min(firstAppTSC, stamps.first_tsc);
    }
    // printf("Selected time %lld minimum TSC %lld\n", minimumTime, firstAppTSC);
    for (std::map<PEAT_Rank, Time_Stamps>::iterator it = time_stamps.begin(); it != time_stamps.end(); ++it) {
        Time_Stamps &stamps = it->second;
        // stamps.after_init_time_shift = (minimumTime - stamps.after_init_time) * (PEAT_Ratio(stamps.rank_freq) / PEAT_Ratio(1000.0 * 1000.0 * 1000.0));
        // stamps.first_tsc = firstAppTSC;
        // printf("Rank %3d shift time %8lld first_tsc %lld\n", it->first, stamps.after_init_time_shift, stamps.first_tsc);
        stamps.after_init_time_shift = stamps.after_init_tsc - maximumCorrectionTSC;
    }

    std::cerr.precision(std::numeric_limits<double>::digits10);
    // for(map<PEAT_Rank, time_stamps>::const_iterator it = time_stamps.begin(); it != time_stamps.end(); ++it) {
    // const time_stamps &stamp = it->second;
    // const PEAT_Ratio freqRatio = PEAT_Ratio((stamp.after_finalize_time - stamp.after_init_time) * (PEAT_Ratio(stamp.rank_freq) / PEAT_Ratio(1000.0 * 1000.0 * 1000.0))) / PEAT_Ratio(stamp.after_finalize_tsc - stamp.after_init_tsc);
    // cerr << " rank " << it->first << " freq ratio " << fixed << freqRatio << "\n";
    //}
}

void Communicators::parse(const String_Split &lines, const int rank, PEAT_Bool is_app)
{
    if (lines.size() < 2) {
        return;
    }
    // cerr << "rank " << rank << " line " << lines << endl;
    PEAT_Record_Type event = PEAT_RT_Non_Existent;
    extract(event, lines.at(0));
    event = m_id->translate_record_type(event);
    switch (event) {
    case PEAT_RT_Local_Communicators_Report:
        process_local_communicators(rank, lines);
        break;
    case PEAT_RT_Global_Communicators_Report:
        process_global_communicators(lines);
        break;
    case PEAT_RT_MPI_Predefined:
        process_def_comm(lines);
        break;
    case PEAT_RT_Collector_Frame_Info:
        process_frame_info(lines, rank);
        break;
    case PEAT_RT_Host_Name: {
        Time_Stamps &stamps = time_stamps[rank];
        extract(stamps.rank_freq, lines.at(2));
        extract(stamps.after_init_time, lines.at(3));
        extract(stamps.after_finalize_time, lines.at(4));
        extract(stamps.after_init_tsc, lines.at(7));
        extract(stamps.after_finalize_tsc, lines.at(8));
        // printf("PEAT_RT_Host_Name: rank %d, shift: %lld\n", rank, stamps.after_finalize_tsc);
    } break;
    case PEAT_RT_App_Start_Stop_Info: {
        Time_Stamps &stamp = time_stamps[rank];
        extract(stamp.first_tsc, lines.at(1));
        // printf("PEAT_RT_App_Start_Stop_Info: rank %d, first_tsc: %lld\n", rank, stamp.first_tsc);
    } break;
    case PEAT_RT_MPI_Time: {
        PEAT_Clock mpi_rank_time = 0;
        extract(mpi_rank_time, lines.at(1));
        mpi_time[rank].clocks = mpi_rank_time;
        mpi_time[rank].percentage = lines.at(2);
    } break;
    case PEAT_RT_Remark: // same as one above but for old traces
    {
        if (strcmp(lines.at(1), "MPI Time") == 0) {
            PEAT_Clock mpi_rank_time = 0;
            char *endptr;
            extract(mpi_rank_time, lines.at(2), &endptr, 10);
            mpi_time[rank].clocks = mpi_rank_time;
            mpi_time[rank].percentage = endptr;
        }
    } break;
    case PEAT_RT_Sync_Point:
        if (is_app) {
            char *endptr;
            extract(commworld_rank_num, lines.at(1), &endptr, 10);
        }
        break;
    case PEAT_RT_Non_Existent:
        std::cerr << "Tag is ignored: " << event << " line " << lines << std::endl;
        break;
    default:
        // std::cerr << "Tag is ignored: " << event << " line " << lines << std::endl;
        break;
    }
}

PEAT_Clock Communicators::get_real_time(const PEAT_Clock raw_tsc, const PEAT_Rank rank)
{
    PEAT_Clock result = raw_tsc;
    const Time_Stamps &stamp = time_stamps[rank]; // TODO check out of bounds for rank > stamp.size()
    // printf("in %lld\n", result);
    // result -= stamp.first_tsc;

    // printf("stage1 %lld\n", result);
    result -= stamp.after_init_time_shift;

    // printf("stage2 %lld\n", result);
    const PEAT_Ratio freqRatio = 1.0; // PEAT_Ratio((stamp.after_finalize_time - stamp.after_init_time) * (PEAT_Ratio(stamp.rank_freq) / PEAT_Ratio(1000.0 * 1000.0 * 1000.0))) / PEAT_Ratio(stamp.after_finalize_tsc - stamp.after_init_tsc);

    result = PEAT_Clock(PEAT_Ratio(result) * freqRatio); // + stamp.after_init_tsc;

    // cerr << "ratio " << fixed << freqRatio << " rank " << rank << " rawSTC " << raw_tsc << " result " << result << "\n";
    return result;
}

void Communicators::process_def_comm(const String_Split &line)
{
    // cerr << "Communicators::processDefComm " << line << endl;
    extract(CMPI_SUCCESS, line.at(1));
    extract(CMPI_UNDEFINED, line.at(2), 16);
    extract(CMPI_COMM_WORLD, line.at(3), 16);
    extract(CMPI_COMM_SELF, line.at(4), 16);
    extract(CMPI_COMM_NULL, line.at(5), 16);
    extract(CMPI_PROC_NULL, line.at(6));
    extract(CMPI_ANY_TAG, line.at(7));
    extract(CMPI_ANY_SOURCE, line.at(8));
    extract(CMPI_REQUEST_NULL, line.at(9), 16);
}

// Estimator global comm ID expected in parameters
PEAT_Rank Communicators::get_rank_from_comm(const PEAT_MPI_Comm comm, const PEAT_Rank destinationRank) const
{
    // now this function is remained just for sanity check
    if (0 == comm) { // medians MPI_COMM_NULL
        return 0;
    }
    if (1 == comm) { // medians MPI_COMM_SELF
        return destinationRank;
    }
    if (2 == comm) { // medians MPI_COMM_WORLD
        return destinationRank;
    }
    if (get_MPI_ANY_SOURCE() == destinationRank) {
        return destinationRank;
    }
    return (destinationRank);
}

std::map<PEAT_Rank, PEAT_Rank> Communicators::get_ranks_map_from_comm(const PEAT_MPI_Comm comm) const
{
    std::map<PEAT_Rank, PEAT_Rank> resultRanks;
    if (0 == comm) {
        std::ostringstream errTxt;
        errTxt << "Communicators::get_ranks_from_comm. This is unexpected to get MPI_COMM_NULL as collective function communicator\n";
        throw std::logic_error(errTxt.str());
        return resultRanks;
    }
    if (1 == comm) { // MPI_COMM_SELF
        resultRanks[0] = 0;
        return resultRanks;
    }
    if (2 == comm) { // MPI_COMM_WORLD
        for (PEAT_Num i = 0; i < get_comm_world_rank_count(); ++i) {
            resultRanks[(PEAT_Rank)i] = (PEAT_Rank)i;
        }
        return resultRanks;
    }
    const Communicator_Info &info = comms.at(comm);
    for (PEAT_Num i = 0; i < info.ranks.size(); ++i) {
        resultRanks[info.ranks.at(i)] = (PEAT_Rank)i;
    }
    if (resultRanks.empty()) {
        std::ostringstream tErr;
        tErr << comm << ": is not found in " << resultRanks << "\n";
        throw std::range_error(tErr.str());
    }
    return resultRanks;
}

std::set<PEAT_Rank> Communicators::get_ranks_set_from_comm(const PEAT_MPI_Comm comm) const
{
    std::set<PEAT_Rank> resultRanks;
    if (0 == comm) {
        std::ostringstream errTxt;
        errTxt << "Communicators::get_ranks_from_comm. This is unexpected to get MPI_COMM_NULL as collective function communicator\n";
        throw std::logic_error(errTxt.str());
        return resultRanks;
    }
    if (1 == comm) { // MPI_COMM_SELF
        resultRanks.insert(0);
        return resultRanks;
    }
    if (2 == comm) { // MPI_COMM_WORLD
        for (PEAT_Num i = 0; i < get_comm_world_rank_count(); ++i) {
            resultRanks.insert((PEAT_Rank)i);
        }
        return resultRanks;
    }
    const Communicator_Info &info = comms.at(comm);
    for (PEAT_Num i = 0; i < info.ranks.size(); ++i) {
        resultRanks.insert(info.ranks.at(i));
    }
    if (resultRanks.empty()) {
        std::ostringstream tErr;
        tErr << comm << ": is not found in " << resultRanks << "\n";
        throw std::range_error(tErr.str());
    }
    return resultRanks;
}

void Communicators::process_global_communicators(const String_Split &line) // fills basic comm info
{
    // cerr << "Communicators::processNewComm " << line << " rank " << rank << endl;
    // format: id:is_internal:size:rank_0:...
    PEAT_MPI_Comm comm_id = 0;
    extract(comm_id, line.at(1), 16);
    int is_internal = 0;
    extract(is_internal, line.at(2));
    PEAT_Rank comm_size = 0;
    extract(comm_size, line.at(3));
    Communicator_Info comm;
    comm.comm_id = comm_id;
    comm.is_internal = is_internal != 0;
    if (comm_size > 0) {
        comm.ranks.resize(comm_size, 0);
        for (PEAT_Rank i = 0; i < comm_size; ++i) {
            extract(comm.ranks[i], line.at(4 + i));
        }
    }
    comms[comm_id] = comm;
}

void Communicators::process_local_communicators(const PEAT_Rank rank, const String_Split &line) // extends comm info to groups
{
    // cerr << "Communicators::processNewComm " << line << " rank " << rank << endl;
    PEAT_MPI_Comm comm_id = 0;
    extract(comm_id, line.at(1), 16);
    PEAT_Rank local_group_size = 0;
    extract(local_group_size, line.at(5));
    PEAT_Rank remote_group_size = 0;
    extract(remote_group_size, line.at(6));
    Communicator_Info &comm = comms.at(comm_id); // basic info should be filled while processing app file
    if (local_group_size > 0) {
        comm.groups[0][rank].resize(local_group_size, 0);
        std::string ranks = line.at(7);
        std::vector<std::string> v;
        splt_slow(ranks, v, ',');
        for (PEAT_Rank i = 0; i < local_group_size; ++i) {
            extract(comm.groups[0][rank][i], v.at(i).c_str());
        }
    }
    if (remote_group_size > 0) {
        comm.groups[1][rank].resize(remote_group_size, 0);
        std::string ranks = line.at(8);
        std::vector<std::string> v;
        splt_slow(ranks, v, ',');
        for (PEAT_Rank i = 0; i < remote_group_size; ++i) {
            extract(comm.groups[1][rank][i], v.at(i).c_str());
        }
    }
}

void Communicators::process_frame_info(const String_Split &line, PEAT_Rank rank)
{
    PEAT_Clock startTSC = 0;
    extract(startTSC, line.at(4));
    extract(frame_tsc_field_size, line.at(6));
    extract(frame_hw_field_size, line.at(7));
    frame_tsc_start[rank] = startTSC;
}

std::ostream &operator<<(std::ostream &dbg, const Communicators::Communicator_Info &mp)
{
    dbg << "peCommId:" << mp.comm_id << " ranks: " << mp.ranks << "\n";
    return dbg;
}
