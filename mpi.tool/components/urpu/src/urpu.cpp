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

#include "urpu.h"
#include <string>
#ifdef PEAT_MPI
#include <mpi.h>
#endif

#define urpu_out          \
    if (globals.verbose) \
    std::cout
#define urpu_dbg(x)           \
    if (globals.verbose > x) \
    std::cerr
PEAT_Bool urpu::parse_single_event(const int rank)
{
    String_Split line_hwc;
    if (!input_HWC->is_file_good(rank)) {
        return false;
    }
    input_HWC->read_lines(line_hwc, rank, ':');
    // urpu_dbg(0) << "P" << rank << std::endl;
    if (line_hwc.size() < 2) {
        return false;
    }
    Tractor_Event tLine(rank, line_hwc, model_id.num_tscs, model_id.num_hwcs, model_id.num_addrs, user_region_file);
    if (tLine.level > 0) // nested record (issued inside another traced call): no user code in front of it
        return false;
    if (last_idx[rank] == tLine.uniq_id) {
        last_idx[rank] = tLine.uniq_id;
        return false;
    }
    last_idx[rank] = tLine.uniq_id;
    if (tLine.uniq_id <= 1) {
        last_key[rank] = tLine.addr_key;
        return false;
    }
    User_Address_Key prev_ur(last_key[rank]);
    User_Address_Key ur(tLine.addr_key);
    ur = prev_ur + ur;
    User_Region_Store::Totals evt;

    User_Region::Elems elem;
    evt.uc_instr = tLine.hwc_values.at(0);
    evt.uc_clock = tLine.hwc_values.at(1);
    evt.mpi_instr = tLine.hwc_values.at(2);
    evt.mpi_clock = tLine.hwc_values.at(3);

    elem.instr = evt.uc_instr;
    elem.clocks = evt.uc_clock;
    uc_store.user_regions[rank][ur].add(elem);
    uc_store.totals[rank] = uc_store.totals[rank] + evt;
    last_key[rank] = tLine.addr_key;
    return true;
}
urpu::urpu(Global_Parameters &glob, Model_ID &model_id_storage) : globals(glob), model_id(model_id_storage)
{
    input_HWC = 0;
#ifdef PEAT_MPI
    total_completed_ranks = 0;
#endif
    // Open *com* files
    std::map<PEAT_Rank, std::string> file_input_com_names = get_file_names(globals.input_path, globals.trace_files, globals.com_ext, true);
    std::string file_name = glob.input_path + DIR_DELIMITER + glob.trace_files + glob.app_ext;
    std::map<PEAT_Rank, std::string> file_input_app_names;
    file_input_app_names[0] = file_name;

    PEAT_Rank trace_comm_size = (PEAT_Rank)file_input_com_names.size();

    input_HWC = new File_Storage_GZ(globals.pe_HWC_files_in_rank, PEAT_FIO_FLAG_READ | PEAT_FIO_FLAG_LAZY | PEAT_FIO_FLAG_ICLOSE, NULL, globals.verbose);
    user_region_file = new Address_File_Storage(globals, PEAT_FIO_FLAG_WRITE, &model_id_storage);

#ifdef PEAT_MPI
    whole_ranks = globals.whole_ranks;
    completed_ranks.resize(trace_comm_size);
    my_ranks = globals.my_ranks;
#endif
}
urpu::~urpu()
{
    if (input_HWC)
        delete input_HWC;
    if (user_region_file)
        delete user_region_file;
}
void urpu::process()
{
    const double total_event_num = (double)(input_HWC->get_all_files_size());
    const double bytes_step = total_event_num * 0.01;
    long cur_percent = 1;
    PEAT_Ratio start_estimation = Wtime();
    PEAT_Ratio time_since_last_active = Wtime();
    String_Split trace_line_MPI, trace_line_HWC;

    PEAT_Num completions = 0;
    if (model_id.num_hwcs == 2) {
        while ((input_HWC->is_any_file_good() || completions > 0)) {
            for (std::map<PEAT_Rank, std::string>::const_iterator rank = globals.pe_HWC_files_in_rank.begin(); rank != globals.pe_HWC_files_in_rank.end(); ++rank) {
                parse_single_event(rank->first);
            }
        }
#if 1
        for (std::map<PEAT_Rank, std::map<User_Address_Key, User_Region>>::iterator urs_it = uc_store.user_regions.begin(); urs_it != uc_store.user_regions.end(); ++urs_it) {
            for (std::map<User_Address_Key, User_Region>::iterator ur_it = urs_it->second.begin(); ur_it != urs_it->second.end(); ++ur_it) {
                ur_it->second.process(0.8);
            }
        }
        user_region_file->put_records(uc_store);
#endif
    } else {
        std::cerr << "Hardware counters mismatch...exiting" << std::endl;
    }
    return;
}
