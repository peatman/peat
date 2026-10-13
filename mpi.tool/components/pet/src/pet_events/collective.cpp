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
#include "pet_rank.h"
#include "stat.h"
#include "utils.h"

#ifdef PEAT_MPI
#include <mpi.h>
#endif

PEAT_Bool PET_Collective_Event::read_event(PEAT_Rank r_rank, const String_Split &mpi_string, const String_Split &hwc_string, PET_Event *host, const PEAT_Record_Header &hdr)
{
    PET_Local_Event::read_event(r_rank, mpi_string, hwc_string, host, hdr);
    extract(comm, mpi_string.at(hdr.payload + 1), 16);
    extract(root, mpi_string.at(hdr.payload + 2));
    extract(request, mpi_string.at(hdr.payload + 3), 16);
    PEAT_Num srb[2];
    extract(srb[0], mpi_string.at(hdr.payload + 4));
    extract(srb[1], mpi_string.at(hdr.payload + 5));

    ranks = get_core()->get_comm()->get_ranks_map_from_comm(comm);
    PEAT_Message_Size size_msg_collective = -1;
    if (srb[0] > 0) {
        if (srb[0] == 1) {
            extract(size_msg_collective, mpi_string.at(hdr.payload + 6 ));
            tx_bytes.set_scalar(size_msg_collective);
        } else {
            int i = 0;
            // this code is technically not valid (for neighbor *Neighbor* type one - extra processing is requered) but prevents crashing 
#if 0
            for(std::map<PEAT_Rank, PEAT_Rank>::const_iterator it = ranks.begin(); it != ranks.end(); ++it) {
                extract(size_msg_collective, mpi_string.at(hdr.payload + 6 + i));
                tx_bytes.set_vector(size_msg_collective, it->first);
                ++i;
            }
        }
#else
            for(i = 0; i < srb[0]; i++) {
                extract(size_msg_collective, mpi_string.at(hdr.payload + 6 + i));
                tx_bytes.set_vector(size_msg_collective, i);
            }
#endif
        }
    }
    if (srb[1] > 0) {
        if (srb[1] == 1) {
            extract(size_msg_collective, mpi_string.at(hdr.payload + 6 + tx_bytes.size()));
            rx_bytes.set_scalar(size_msg_collective);
        } else {
            int i = 0;
            // this code is technically not valid (for neighbor *Neighbor* type one - extra processing is requered) but prevents crashing 
#if 0
            for (std::map<PEAT_Rank, PEAT_Rank>::const_iterator it = ranks.begin(); it != ranks.end(); ++it) {
                extract(size_msg_collective, mpi_string.at(hdr.payload + 6 + tx_bytes.size() + i));
                rx_bytes.set_vector(size_msg_collective, it->first);
                ++i;
            }
#else
            for(i = 0; i < srb[1];i++) {
                extract(size_msg_collective, mpi_string.at(hdr.payload + 6 + tx_bytes.size() + i));
                rx_bytes.set_vector(size_msg_collective, i);
            }
#endif
        }
    }
    return true;
}
PEAT_Bool PET_Collective_Event::write_bind(File_Storage_GZ *gz_bind)
{
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
    gz_MPI->printf(rank, ":%x:%d:%llx:%d:%d",
                   comm, root, request, tx_bytes.size(), rx_bytes.size());
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
static PEAT_Clock record_finish(PET_Event *evt)
{
    // a transfer ends when its message (or ack) is through; if that is not modelled yet fall back to the record end
    PEAT_Clock finish = evt->e_time.func.out;
    if (evt->get_event_type() == PET_EVENT_TYPE_TOR || evt->get_event_type() == PET_EVENT_TYPE_TAR) {
        PET_ToR_Event *tor = static_cast<PET_ToR_Event *>(evt);
        PEAT_Clock msg = tor->get_ack_finish(PEAT_DIRECTION_AUTO_SELECT);
        if (msg > 0)
            finish = __max__(finish, msg);
    }
    return finish;
}

PEAT_Bool PET_Collective_Event::progress_events_estimated() const
{
    // note: a completion that re-evaluates a record clears its estimated flag for good (override_children),
    // finish_calculations is the reliable "this record is done" mark
    PEAT_Num size = progress_events.size();
    for (PEAT_Num i = 0; i < size; i++) {
        if (!progress_events[i]->finish_calculations && !progress_events[i]->is_estimated())
            return false;
    }
    return true;
}

PEAT_Clock PET_Collective_Event::get_records_finish() const
{
    // note: the completion (MPI_Wait) clears the estimated flag of this collective while it re-evaluates it,
    // so only the progress records are required to be done here
    if (!progress_events_estimated())
        return -1;
    PEAT_Clock finish = e_time.mpi.out + event_parts.epilog;
    const std::vector<PET_Event *> &kids = child_iteration_list();
    for (PEAT_Num i = 0; i < kids.size(); i++) {
        if (kids[i])
            finish = __max__(finish, record_finish(kids[i]));
    }
    for (PEAT_Num i = 0; i < progress_events.size(); i++) {
        finish = __max__(finish, record_finish(progress_events[i]));
    }
    return finish;
}

PEAT_Clock PET_Collective_Event::get_message_finish() const
{
    if (progress_events.empty())
        return e_time.mpi.out + event_parts.epilog;
    // a nonblocking collective driven from other calls ends with its last record, wherever that ran
    return get_records_finish();
}
PEAT_Clock PET_Collective_Event::get_ack_finish() const
{
    return get_message_finish();
}

PEAT_Bool PET_Collective_Event::uses_tag(PEAT_MPI_Comm c, PEAT_MPI_Tag t) const
{
    if (c != comm)
        return false;
    const std::vector<PET_Event *> &kids = child_iteration_list();
    for (PEAT_Num i = 0; i < kids.size(); i++) {
        PET_Event *kid = kids[i];
        if (kid == NULL)
            continue;
        if (kid->get_event_type() == PET_EVENT_TYPE_TOR || kid->get_event_type() == PET_EVENT_TYPE_TAR) {
            if (kid->get_send_tag() == t || kid->get_recv_tag() == t)
                return true;
        }
    }
    for (PEAT_Num i = 0; i < progress_events.size(); i++) {
        PET_Event *evt = progress_events[i];
        if (evt->get_event_type() == PET_EVENT_TYPE_TOR || evt->get_event_type() == PET_EVENT_TYPE_TAR) {
            if (evt->get_send_tag() == t || evt->get_recv_tag() == t)
                return true;
        }
    }
    return false;
}

void PET_Collective_Event::add_progress_event(PET_Event *event)
{
    event->progress_owner = this;
    progress_events.push_back(event);
}

void PET_Collective_Event::notify_before_destroy(PET_Event *event)
{
    PET_Coll_Event::notify_before_destroy(event);
    for (std::vector<PET_Event *>::iterator it = progress_events.begin(); it != progress_events.end(); ++it) {
        if (*it == event) {
            progress_events.erase(it);
            break;
        }
    }
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
    if (is_nonblocking_collective()) {
        get_rank()->register_nb_collective(this); // later calls may progress it, see PET_Core::attribute_progress_records()
    }
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
    for (PEAT_Num i = 0; i < progress_events.size(); i++) {
        progress_events[i]->progress_owner = NULL;
    }
    if (is_nonblocking_collective() && get_rank())
        get_rank()->unregister_nb_collective(this);
}
