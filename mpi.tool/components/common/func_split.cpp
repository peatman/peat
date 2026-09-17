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

#include "func_split.h"
#include "file_io.h"
#include "func_id.h"
#include "mpi_func_ids.h"
#include <cassert>
#include <iomanip>

#define PRTDBG(x)     \
    if (verbose >= x) \
    std::cerr
#define PCOUT                                        \
    if (globals.verbose && (0 == globals.comm_rank)) \
    std::cout
#define MEGABYTE (1000 * 1000)

// 1 sec = 2 700 000 000 TSC (if freq == 2.7GHz)
PEAT_Function_Splitter::PEAT_Function_Splitter(const Global_Parameters &glob) : verbose(glob.verbose)
{
    file_dir = glob.input_path;
    file_name = glob.functions_split_file;
    globals = glob;
    if (file_name.find(DIR_DELIMITER) != std::string::npos) {
        file_dir.erase();
    } else {
        file_dir += DIR_DELIMITER;
    }
    if (verbose) {
        printf("Reading Function Split Profile From  %s %s\n", file_dir.c_str(), file_name.c_str());
    }

    try {
        read_func_split_profile(file_dir, file_name);
    } catch (const std::runtime_error &e) {
        PCOUT << "No Function profile file. Error: " << e.what() << "\n";
    }
}

PEAT_Function_Splitter::~PEAT_Function_Splitter()
{
}

void PEAT_Function_Splitter::read_func_split_profile(const std::string &dir, const std::string &name)
{
    if (name.empty())
        return;
    File_Storage_GZ tst_file(dir + name, PEAT_FIO_FLAG_READ | PEAT_FIO_FLAG_LAZY | PEAT_FIO_FLAG_ICLOSE, NULL, verbose);
    String_Split lines;
    PEAT_Bool hasMoreLines = true;
    if (tst_file.is_any_file_good()) {
        while (hasMoreLines) {
            hasMoreLines = tst_file.read_lines(lines, 0, ':');
            parse(lines, 0);
        }
    } else {
        std::ostringstream errTxt;
        errTxt << "Can't open file " << dir + name;
        throw std::runtime_error(errTxt.str());
    }
}

void PEAT_Function_Splitter::parse(const String_Split &line, PEAT_Rank rank)
{
    if (line.size() < 2) {
        return;
    }
    PEAT_Record_Type event = PEAT_RT_Remark;
    extract(event, line.at(0));
    switch (event) {
    case PEAT_RT_Func_Split: {

        // fprintf( stderr, "%d:%d:%lld:%s:%s", PEAT_RT_Func_Split, t, ptp->get_trace_freq( 0 ), global_func_id.name_by_idx( func_id ), global_func_id.name_by_idx( parent_func_id ) );
        // fprintf( stderr, ":%lld", message_size );
        // fprintf( stderr, ":%.0llf:%.0llf", ( PEAT_LAT ) min_prolog, ( PEAT_LAT ) min_epilog );
        // fprintf( stderr, ":%.0llf:%.0llf", ( PEAT_LAT ) avg_prolog, ( PEAT_LAT ) avg_epilog );
        // fprintf( stderr, ":%.0llf:%.0llf", ( PEAT_LAT ) median_prolog, ( PEAT_LAT ) median_epilog );
        // fprintf( stderr, ":%.0llf:%.0llf", ( PEAT_LAT ) max_prolog, ( PEAT_LAT ) max_epilog );
        PEAT_Num line_size = line.size();
        PEAT_Ext_Time_Parts tmp;
        int t_tmp;
        PEAT_Transports t;
        extract(t_tmp, line.at(1));
        t = (PEAT_Transports)t_tmp;
        extract(tmp.frequency, line.at(2));
        PEAT_MPI_Func_ID func_id, parent_func_id;
        extract(func_id, line.at(3), 10, false);
        extract(parent_func_id, line.at(4), 10, false);
        if (func_id == 0) {
            func_id = global_func_id.idx_by_name(line.at(3));
        }
        if (parent_func_id == 0) {
            parent_func_id = global_func_id.idx_by_name(line.at(4));
        }
        PEAT_Rank num_ranks;
        extract(num_ranks, line.at(5), 10);
        PEAT_Func_ID_Pair fid_pair(func_id, parent_func_id);
        PEAT_Num add = 0;
        if (global_func_id.check_rx(func_id))
            add = 1;
        for (PEAT_Num i = 6; i < line_size; i += 10) {
            extract(tmp.message_size, line.at(i + add));
            extract(tmp.time_parts[0].prolog, line.at(i + 1));
            extract(tmp.time_parts[0].epilog, line.at(i + 2));
            extract(tmp.time_parts[1].prolog, line.at(i + 3));
            extract(tmp.time_parts[1].epilog, line.at(i + 4));
            extract(tmp.time_parts[2].prolog, line.at(i + 5));
            extract(tmp.time_parts[2].epilog, line.at(i + 6));
            extract(tmp.time_parts[3].prolog, line.at(i + 7));
            extract(tmp.time_parts[3].epilog, line.at(i + 8));
            func_split_env[t][fid_pair][num_ranks].push_back(tmp);
        }
    } break;
    default:
        PRTDBG(2) << "PEAT_Function_Splitter::parse. Tag is ignored: " << event << " line " << line << std::endl;
        break;
    }
    return;
}

PEAT_Time_Parts *PEAT_Function_Splitter::get_func_split(const PEAT_Transports t, const PEAT_Func_ID_Pair &func_id_pair, PEAT_Rank num_ranks, PEAT_Message_Size size)
{
    return NULL;
}
