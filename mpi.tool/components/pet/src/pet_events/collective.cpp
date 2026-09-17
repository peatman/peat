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

PEAT_Bool PET_Collective_Event::read_event(PEAT_Rank r_rank, const String_Split &mpi_string, const String_Split &hwc_string, PET_Coll_Event *host)
{
    PET_Local_Event::read_event(r_rank, mpi_string, hwc_string, host);
    extract(comm, mpi_string.at(4), 16);
    extract(root, mpi_string.at(5));
    extract(request, mpi_string.at(6), 16);
    PEAT_Num srb[2];
    extract(srb[0], mpi_string.at(7));
    extract(srb[1], mpi_string.at(8));
    int num_colts;
    extract(num_colts, mpi_string.at(9)); // number of collective transfers followed by
    t_events.resize(num_colts, NULL);

    ranks = get_core()->get_comm()->get_ranks_map_from_comm(comm);
    PEAT_Message_Size size_msg_collective = -1;
    if (srb[0] > 0) {
        if (srb[0] == 1) {
            extract(size_msg_collective, mpi_string.at(10));
            tx_bytes.set_scalar(size_msg_collective);
        } else {
            int i = 0;
            for (std::map<PEAT_Rank, PEAT_Rank>::const_iterator it = ranks.begin(); it != ranks.end(); ++it) {
                extract(size_msg_collective, mpi_string.at(10 + i));
                tx_bytes.set_vector(size_msg_collective, it->first);
                ++i;
            }
        }
    }
    if (srb[1] > 0) {
        if (srb[1] == 1) {
            extract(size_msg_collective, mpi_string.at(10 + tx_bytes.size()));
            rx_bytes.set_scalar(size_msg_collective);
        } else {
            int i = 0;
            for (std::map<PEAT_Rank, PEAT_Rank>::const_iterator it = ranks.begin(); it != ranks.end(); ++it) {
                extract(size_msg_collective, mpi_string.at(10 + tx_bytes.size() + i));
                rx_bytes.set_vector(size_msg_collective, it->first);
                ++i;
            }
        }
    }
    return true;
}
PEAT_Bool PET_Collective_Event::write_bind(File_Storage_GZ *gz_bind)
{
    std::list<Coll_Storage>::iterator collective;
    if (get_core()->get_coll(this, collective)) {
        std::map<PEAT_Rank, PEAT_UID>::iterator coll_it = collective->coll_uids.begin();
        if (coll_it->first == rank) { // only write collectives once (for 1st rank)
            gz_bind->printf(rank, "%d:%s", PEAT_RT_Bind_Collective, global_func_id.name_by_idx(func_id));
            for (; coll_it != collective->coll_uids.end(); ++coll_it) {
                gz_bind->printf(rank, ":%d:%lld", coll_it->first, coll_it->second);
            }
            gz_bind->printf(rank, "\n");
            get_core()->collectives_bound++;
        }
        collective->shutdown_rank(rank);
        is_written = true;
    } else {
        pet_throw_error(pet_core, this, NULL, "Unknown collective.\n");
        // assert(0);
    }
    return true;
}

PET_Event *PET_Collective_Event::write_event(File_Storage_GZ *gz_MPI, File_Storage_GZ *gz_HWC, File_Storage_GZ *gz_bind, const char *mpi_eol, const char *hwc_eol)
{
    PET_Local_Event::write_event(gz_MPI, gz_HWC, gz_bind, "", hwc_eol);
    gz_MPI->printf(rank, ":%x:%d:%llx:%d:%d:%d",
                   comm, root, request, tx_bytes.size(), rx_bytes.size(), t_events.size());
    for (int i = 0; i < (int)tx_bytes.size(); i++) {
        gz_MPI->printf(rank, ":%lld", tx_bytes.get(i));
    }
    for (int i = 0; i < (int)rx_bytes.size(); i++) {
        gz_MPI->printf(rank, ":%lld", rx_bytes.get(i));
    }
    gz_MPI->printf(rank, "%s", mpi_eol);
    if (!is_persistent_init()) {
        write_bind(gz_bind);
    }
    PET_Coll_Event::write_event(gz_MPI, gz_HWC, gz_bind, mpi_eol, hwc_eol);

    return this;
}

PEAT_Clock PET_Collective_Event::get_message_start() const
{
    // assert(0);
    return e_time.mpi.in + event_parts.prolog;
    // pet_throw_error(pet_core, this, NULL, "Function is not implemented.\n");
    // return -1;
}
PEAT_Clock PET_Collective_Event::get_message_finish() const
{
    return e_time.mpi.out + event_parts.epilog;
    // pet_throw_error(pet_core, this, NULL, "Function is not implemented.\n");
    // assert(0);
    // return -1;
}
PEAT_Clock PET_Collective_Event::get_ack_finish() const
{
    return e_time.mpi.out + event_parts.epilog;
    // pet_throw_error(pet_core, this, NULL, "Function is not implemented.\n");
    // assert(0);
    // return -1;
}
PEAT_Message_Size PET_Collective_Event::get_real_tx_bytes() const
{
    return tx_bytes.get_total();
}
PEAT_Message_Size PET_Collective_Event::get_real_rx_bytes() const
{
    return rx_bytes.get_total();
}
PEAT_Num PET_Collective_Event::check_for_outstanding_progress_messages(PEAT_Clock clk)
{
    PET_Event *event = get_estimated_event();
    if (event && event != this) {
        return event->check_for_outstanding_progress_messages(clk);
    } else {
        PEAT_Signed_Num pe_type = get_core()->get_pe_type();
        if (!is_non_block() || pe_type > 0)
            return handle_progress_messages(clk, true);
    }
    return 0;
}

void PET_Collective_Event::post_read_event()
{
    PET_Coll_Event::post_read_event();
    get_core()->handle_coll(this);
}

PEAT_Bool PET_Collective_Event::is_assembled() const
{
    return PET_Coll_Event::is_assembled();
}
PEAT_Bool PET_Collective_Event::is_bound() const
{
    if (!ready_to_bind() || !is_assembled())
        return false;
    return PET_Coll_Event::is_bound();
}

PEAT_Bool PET_Collective_Event::safe_to_delete()
{
    PEAT_Bool is_safe = is_written;
    is_safe = is_safe || global_func_id.check_property(func_id, PEAT_PROP_PERSISTENT_REQUEST_INIT); // init functions are like locals
    return is_safe && PET_Coll_Event::safe_to_delete();
}

PEAT_Bool PET_Collective_Event::can_be_destroyed()
{
    // if (host_event) return false; // sub event... cannot be destroyed
    PET_Event *completion = get_completion();
    if (completion) {
        return completion->is_written && PET_Coll_Event::can_be_destroyed();
    } else {
        return PET_Coll_Event::can_be_destroyed();
    }
}
PET_Collective_Event::~PET_Collective_Event()
{
    if (parent_event)
        parent_event->notify_before_destroy(this);
}
