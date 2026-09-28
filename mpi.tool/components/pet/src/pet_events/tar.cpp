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

PEAT_Bool PET_TaR_Event::read_event(PEAT_Rank r_rank, const String_Split &mpi_string, const String_Split &hwc_string, PET_Event *host, const PEAT_Record_Header &hdr)
{
    if (!process_second_line) {
        PET_Local_Event::read_event(r_rank, mpi_string, hwc_string, host, hdr);
        extract(comm, mpi_string.at(hdr.payload + 1), 16);
        extract(peer_rank, mpi_string.at(hdr.payload + 2)); // COMMWORLD based No need to translate
                                              /*        PEAT_MPI_Request request;
                                              extract(request, mpi_string.at(hdr.payload + 2),16);
                                              extract(message, mpi_string.at(hdr.payload + 3),16);*/
        extract(tag, mpi_string.at(hdr.payload + 5));
        extract(t_bytes, mpi_string.at(hdr.payload + 6));
        process_second_line = true;
    } else {
        extract(peer_rank_2, mpi_string.at(hdr.payload + 2)); // COMMWORLD based No need to translate
        extract(tag_2, mpi_string.at(hdr.payload + 5));
        extract(t_bytes_2, mpi_string.at(hdr.payload + 6));
    }
    return true;
}
PEAT_Bool PET_TaR_Event::write_bind(File_Storage_GZ *gz_bind)
{
    gz_bind->printf(rank, "%d:%s:%d:%s:%s:%d:%s:%lld\n", PEAT_RT_Bind_Peer,
                    global_func_id.name_by_idx(func_id),
                    rank, ids_str().c_str(),
                    global_func_id.name_by_idx(peer_completion_func_id_2),
                    peer_rank, peat_ids_string(peer_completion_ids_2).c_str(), t_bytes);

    /*  //don't write receive part
        gz_bind->printf(rank, "%d:%s:%s:%d:%lld:%s:%s:%d:%lld\n", PEAT_RT_Bind_Peer,
            global_func_id.name_by_idx(peer_completion_func_id_2),
            global_func_id.name_by_idx(peer_func_id_2),
            peer_rank_2, peat_ids_string(peer_ids_2).c_str(),
            global_func_id.name_by_idx(parent_func_id),
            global_func_id.name_by_idx(func_id),
            rank, ids_str().c_str());
    */
    get_core()->messages_bound[rank]++;
    return true;
}

PET_Event *PET_TaR_Event::write_event(File_Storage_GZ *gz_MPI, File_Storage_GZ *gz_HWC, File_Storage_GZ *gz_bind, const char *mpi_eol, const char *hwc_eol)
{
    is_written = true;
    if (host_event && !host_event->force_write)
        return this;
    PET_Local_Event::write_event(gz_MPI, gz_HWC, gz_bind, "", hwc_eol);
    gz_MPI->printf(rank, ":%d:%d:%llx:%llx:%d:%lld%s",
                   comm, peer_rank, request, message, tag, t_bytes, mpi_eol);
    header_second_line = true; // the second line repeats the ids with num_children = 0
    PET_Local_Event::write_event(gz_MPI, gz_HWC, gz_bind, "", hwc_eol);
    header_second_line = false;
    gz_MPI->printf(rank, ":%d:%d:%llx:%llx:%d:%lld%s",
                   comm, peer_rank_2, request, message, tag_2, t_bytes_2, mpi_eol);

    write_bind(gz_bind);
    write_nested_records(gz_MPI, gz_HWC, gz_bind, mpi_eol, hwc_eol); // progress engine records issued inside this call
    // TODO: check send/recv pointers
    return this;
}

PEAT_Bool PET_TaR_Event::bind()
{
    if (is_bound())
        return true;
    if (ready_to_bind()) {
        PET_ToR_Event *recv_event = static_cast<PET_ToR_Event *>(get_peer_for_send());
        if (recv_event == NULL) {
            recv_event = static_cast<PET_ToR_Event *>(get_core()->find_peer(this, false));
            if (recv_event) {
                set_peer_for_send(recv_event);
                recv_event->set_peer_for_recv(this);
            }
        }
        PET_ToR_Event *send_event = static_cast<PET_ToR_Event *>(get_peer_for_recv());
        if (send_event == NULL) {
            send_event = static_cast<PET_ToR_Event *>(get_core()->find_peer(this, true));
            if (send_event) {
                set_peer_for_recv(send_event);
                send_event->set_peer_for_send(this);
            }
        }
        if (recv_event && send_event) {
            return true;
        } else {
            return false;
        }
    }
    return false;
}

PEAT_Bool PET_TaR_Event::is_rdv(PEAT_DIRECTION direction) const
{
    PEAT_Bool r = false;
    if (direction == PEAT_DIRECTION_TX) {
        r = (t_bytes >= get_core()->get_eager_threshold(is_smp(PEAT_DIRECTION_TX), false)) || global_func_id.check_property(func_id, PEAT_PROP_DATA_SYNC);
    }
    if (direction == PEAT_DIRECTION_RX) {
        r = (t_bytes_2 >= get_core()->get_eager_threshold(is_smp(PEAT_DIRECTION_RX), global_func_id.check_property(peer_func_id_2, PEAT_PROP_NON_BLOCK))) || global_func_id.check_property(peer_func_id_2, PEAT_PROP_DATA_SYNC);
    }
    return r;
}
PEAT_Bool PET_TaR_Event::is_smp(PEAT_DIRECTION direction) const
{
    PEAT_Bool r = false;
    if (direction == PEAT_DIRECTION_TX) {
        return get_core()->get_ptp()->is_SMP_tgt(rank, peer_rank);
    }
    if (direction == PEAT_DIRECTION_RX) {
        return get_core()->get_ptp()->is_SMP_tgt(rank, peer_rank_2);
    }
    return r;
}

PEAT_Bool PET_TaR_Event::estimate(Estimation_Event *event)
{
    // if ( event ) {
    handle_event(event);
    // }
    if (is_estimated() || finish_calculations)
        return true;
    if (has_nested() && !nested_all_estimated())
        return true; // wait for the records issued from progress inside this call
    if (estimation_started) {
        PEAT_Clock sub_t = 0;
        PEAT_Clock f_in = e_time.mpi.in + event_parts.prolog;
        PEAT_Clock f_out = f_in + event_parts.meat;
        PEAT_Clock m_t_in = get_message_start(PEAT_DIRECTION_TX);
        PEAT_Clock m_t_out = get_ack_finish(PEAT_DIRECTION_TX);
        PEAT_Clock m_r_in = get_message_start(PEAT_DIRECTION_RX);
        PEAT_Clock m_r_out = get_message_finish(PEAT_DIRECTION_RX);
        if (m_t_out > 0 && m_r_out > 0) {
            // event_parts.stat.fabric = __max__( m_t_out - m_t_in, 0 ) + (m_r_out - m_r_in);
            if (f_in < m_t_in) {
                event_parts.stat.wait = m_t_in - f_in;
            }
            if (m_r_out > f_in) {
                // set idle time here
                if (f_in < m_r_in) {
                    event_parts.stat.wait = __max__(m_r_in - f_in, event_parts.stat.wait);
                }
            }
            // if ( tx_message.is_blocking || m_t_out > 0 ) {
            sub_t = f_out - __max__(m_t_out, m_r_out);
            //} else {
            //	sub_t = f_out - m_r_out;
            //}
            sub_t = clamp_sub_t_to_nested(sub_t, f_out);
            if (sub_t > event_parts.meat)
                sub_t = event_parts.meat;
            event_parts.adjust(0, sub_t, 0);
            event_parts.stat.overlap = get_overlap();
            event_parts.stat.fabric = get_fabric();
            concatenate_event();
            finish_calculations = true;
            submit(e_time.func.out, EET_FUNC_END, this, this, NULL);
            return true;
        }
    }
    return true;
}
