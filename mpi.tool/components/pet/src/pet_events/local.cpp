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
#include <assert.h>
#include <errno.h>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits.h>
#include <numeric> //std::accumulate
#include <set>
#include <sstream>
#include <stdexcept>
#include <stdlib.h>

#include "pet_core.h"
#include "stat.h"
#include "utils.h"

#ifdef PEAT_MPI
#include <mpi.h>
#endif

PEAT_Bool PET_Local_Event::read_event(PEAT_Rank r_rank, const String_Split &mpi_string, const String_Split &hwc_string, PET_Event *host, const PEAT_Record_Header &hdr)
{
    rank = r_rank;
    node = get_core()->get_ptp()->get_tgt_node(rank);
    ids = hdr.ids;
    thread_id = hdr.thread_id;
    int funcid;
    extract(funcid, mpi_string.at(hdr.payload), 10, false);
    if (funcid == 0) {
        func_id = global_func_id.idx_by_name(mpi_string.at(hdr.payload));
    } else {
        func_id = get_core()->model_id.model_function_tags.at((PEAT_MPI_Func_ID)funcid);
    }
    parent_func_id = func_id;
    PET_Event::read_event(r_rank, mpi_string, hwc_string, host, hdr);
    if (func_id == MPI_PCONTROL_ID) {
        extract(p_control_id, mpi_string.at(hdr.payload + 1), 10, false);
    }
    // update_pending();
    return true;
}

PET_Event *PET_Local_Event::write_event(File_Storage_GZ *gz_MPI, File_Storage_GZ *gz_HWC, File_Storage_GZ *gz_bind, const char *mpi_eol, const char *hwc_eol)
{
    PET_Event::write_event(gz_MPI, gz_HWC, gz_bind, mpi_eol, hwc_eol);
    gz_MPI->printf(rank, "%s:%s%s", header_str().c_str(), global_func_id.name_by_idx(func_id), mpi_eol);
    is_written = true;
    return this;
}
