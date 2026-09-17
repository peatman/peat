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

#ifndef __URPU_H__
#define __URPU_H__

#include "ptp.h"

#include "communicators.h"
#include "model_id.h"
#include "tevent.h"
#include <vector>
struct urpu_Time {
    PEAT_Clock durations, occurences;
    urpu_Time() : durations(0), occurences(0) {}
};
class urpu
{
  public:
#ifdef PEAT_MPI
    //    MPI_Comm urpu_COMPUTE_COMM;
    //    PEAT_Rank urpu_COMPUTE_COMM_SIZE;
    PEAT_Num all_finished, total_completed_ranks;
    std::vector<PEAT_Rank> whole_ranks;
    std::vector<PEAT_Rank> my_ranks;
    std::vector<PEAT_Rank> completed_ranks;
#endif
    Func_ID *fid;
    File_Storage_GZ *input_HWC;
    Address_File_Storage *user_region_file;
    const Global_Parameters globals;
    const Model_ID &model_id;
    std::map<PEAT_Rank, PEAT_UID> last_idx;
    std::map<PEAT_Rank, User_Address_Key> last_key;
    User_Region_Store uc_store;
    urpu(Global_Parameters &file_names, Model_ID &model_id_storage);
    ~urpu();
    bool parse_single_event(const int rank);
    void process();
};

#endif
