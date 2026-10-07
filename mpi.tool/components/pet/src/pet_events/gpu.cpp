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

PET_GPU_Event::PET_GPU_Event(PET_Core *p) : PET_Local_Event(p)
{
}

PET_GPU_Event::~PET_GPU_Event()
{
}

PEAT_Bool PET_GPU_Event::read_event(PEAT_Rank r_rank, const String_Split &mpi_string, const String_Split &hwc_string, PET_Event *host, const PEAT_Record_Header &hdr)
{
    size_t num_params = 0, num_out_params = 0, num_extra_params = 0;
    rank = r_rank;
    node = get_core()->get_ptp()->get_tgt_node(rank);
    PET_Local_Event::read_event(r_rank, mpi_string, hwc_string, host, hdr);
    extract(num_params, mpi_string.at(hdr.payload + 1));
    params.resize(num_params);
    extract(num_out_params, mpi_string.at(hdr.payload + 2));
    out_params.resize(num_out_params);
    extract(num_extra_params, mpi_string.at(hdr.payload + 3));
    extra_params.resize(num_extra_params);
    for (size_t p = 0; p < num_params; p++) {
        extract(params[p], mpi_string.at(hdr.payload + 4 + p), 16);
    }
    for (size_t p = 0; p < num_out_params; p++) {
        extract(out_params[p], mpi_string.at(hdr.payload + 4 + num_params + p), 16);
    }
    for (size_t p = 0; p < num_extra_params; p++) {
        extract(extra_params[p], mpi_string.at(hdr.payload + 4 + num_params + num_out_params + p), 16);
    }
    return true;
}

PET_Event *PET_GPU_Event::write_event(File_Storage_GZ *gz_MPI, File_Storage_GZ *gz_HWC, File_Storage_GZ *gz_bind, const char *mpi_eol, const char *hwc_eol)
{
    size_t num_params = params.size();
    size_t num_out_params = out_params.size();
    size_t num_extra_params = extra_params.size();
    PET_Local_Event::write_event(gz_MPI, gz_HWC, gz_bind, "", hwc_eol);
    gz_MPI->printf(rank, ":%zu", num_params);
    gz_MPI->printf(rank, ":%zu", num_out_params);
    gz_MPI->printf(rank, ":%zu", num_extra_params);
    for (size_t p = 0; p < num_params; p++) {
        gz_MPI->printf(rank, ":%llx", params[p]);
    }
    for (size_t p = 0; p < num_out_params; p++) {
        gz_MPI->printf(rank, ":%llx", out_params[p]);
    }
    for (size_t p = 0; p < num_extra_params; p++) {
        gz_MPI->printf(rank, ":%llx", extra_params[p]);
    }
    gz_MPI->printf(rank, ":%s", mpi_eol);
    is_written = true;
    return this;
}
