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

PEAT_Bool PET_Comm_Event::read_event(PEAT_Rank r_rank, const String_Split &mpi_string, const String_Split &hwc_string, PET_Event *host, const PEAT_Record_Header &hdr)
{
    PET_Local_Event::read_event(r_rank, mpi_string, hwc_string, host, hdr);
    extract(comm, mpi_string.at(hdr.payload + 1), 16);
    extract(newcomm, mpi_string.at(hdr.payload + 2), 16);
    extract(remotecomm, mpi_string.at(hdr.payload + 3), 16);
    extract(intercomm, mpi_string.at(hdr.payload + 4), 16);
    extract(request, mpi_string.at(hdr.payload + 5), 16);
    if (!hdr.nested) { // legacy: number of collective transfers followed by
        int num_colts;
        extract(num_colts, mpi_string.at(hdr.payload + 6));
        t_events.resize(num_colts, NULL);
    }
    return true;
}

PET_Event *PET_Comm_Event::write_event(File_Storage_GZ *gz_MPI, File_Storage_GZ *gz_HWC, File_Storage_GZ *gz_bind, const char *mpi_eol, const char *hwc_eol)
{
    PET_Local_Event::write_event(gz_MPI, gz_HWC, gz_bind, "", hwc_eol);
    gz_MPI->printf(rank, ":%x:%x:%x:%x:%x", comm, newcomm, remotecomm, intercomm, request);
    if (!nested_format())
        gz_MPI->printf(rank, ":%d", host_event ? 0 : nested_count_total()); // legacy num_colts
    gz_MPI->printf(rank, "%s", mpi_eol);
    std::list<Coll_Storage>::iterator collective;
    if (get_core()->get_coll(this, collective)) {
        std::map<PEAT_Rank, PEAT_Event_IDs>::iterator coll_it = collective->coll_uids.begin();
        if (coll_it->first == rank) { // only write collectives once (for 1st rank)
            gz_bind->printf(rank, "%d:%s", PEAT_RT_Bind_Collective, global_func_id.name_by_idx(func_id));
            for (; coll_it != collective->coll_uids.end(); ++coll_it) {
                gz_bind->printf(rank, ":%d:%s", coll_it->first, peat_ids_string(coll_it->second).c_str());
            }
            gz_bind->printf(rank, "\n");
            get_core()->collectives_bound++;
        }
        collective->shutdown_rank(rank);
    } else {
        pet_throw_error(pet_core, this, NULL, "Unknown (comm) collective.\n");
        // assert(0);
    }
    PET_Coll_Event::write_event(gz_MPI, gz_HWC, gz_bind, mpi_eol, hwc_eol);
    return this;
}

void PET_Comm_Event::post_read_event()
{
    PET_Coll_Event::post_read_event();
    get_core()->handle_coll(this);
}

PEAT_Bool PET_Comm_Event::is_assembled() const
{
    // if (func_id == MPI_COMM_FREE_ID) return true; // not collective operation
    return PET_Coll_Event::is_assembled();
}
PEAT_Bool PET_Comm_Event::is_bound() const
{
    if (!ready_to_bind() || !is_assembled())
        return false;
    return PET_Coll_Event::is_bound();
}
