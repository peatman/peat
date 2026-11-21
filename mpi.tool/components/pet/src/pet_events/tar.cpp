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

PEAT_Bool PET_TaR_Event::is_null_peer(PEAT_Rank r) const
{
    return r == static_cast<PEAT_Rank>(get_core()->get_comm()->get_MPI_PROC_NULL());
}

PEAT_Bool PET_TaR_Event::read_event(PEAT_Rank r_rank, const String_Split &mpi_string, const String_Split &trace_string, PET_Event *host, const PEAT_Record_Header &hdr)
{
    if (!process_second_line) {
        PET_Local_Event::read_event(r_rank, mpi_string, trace_string, host, hdr);
        extract(comm, mpi_string.at(hdr.payload + 1), 16);
        extract(peer_rank, mpi_string.at(hdr.payload + 2)); // COMMWORLD based No need to translate
        extract(request_tx, mpi_string.at(hdr.payload + 3), 16);
        extract(tag, mpi_string.at(hdr.payload + 5));
        extract(t_bytes, mpi_string.at(hdr.payload + 6));
        process_second_line = true;
    } else {
        extract(peer_rank_2, mpi_string.at(hdr.payload + 2)); // COMMWORLD based No need to translate
        extract(request, mpi_string.at(hdr.payload + 3), 16); // MPI_Isendrecv*: the request of the whole call (0 when blocking)
        extract(tag_2, mpi_string.at(hdr.payload + 5));
        extract(t_bytes_2, mpi_string.at(hdr.payload + 6));
    }
    return true;
}
PEAT_Bool PET_TaR_Event::write_bind(File_Storage_GZ *gz_bind)
{
    // the send part: its receiver is the send-side peer (peer_*), not the one this call receives from (peer_*_2)
    if (check_tx()) {
        gz_bind->printf(rank, "%d:%s:%d:%s:%s:%d:%s:%lld\n", PEAT_RT_Bind_Peer,
                        global_func_id.name_by_idx(func_id),
                        rank, ids_str().c_str(),
                        global_func_id.name_by_idx(peer_completion_func_id),
                        peer_rank, peat_ids_string(peer_completion_ids).c_str(), t_bytes);
        get_core()->messages_bound[rank]++;
    }
    if (request > 0 && get_completion() != NULL) { // MPI_Isendrecv*
        gz_bind->printf(rank, "%d:%s:%s:%s:%s\n", PEAT_RT_Bind_Completion,
                        global_func_id.name_by_idx(func_id),
                        ids_str().c_str(),
                        global_func_id.name_by_idx(get_completion()->func_id),
                        get_completion()->ids_str().c_str());
        get_core()->messages_bound[rank]++;
    }

    /*  //don't write receive part
        gz_bind->printf(rank, "%d:%s:%s:%d:%lld:%s:%s:%d:%lld\n", PEAT_RT_Bind_Peer,
            global_func_id.name_by_idx(peer_completion_func_id_2),
            global_func_id.name_by_idx(peer_func_id_2),
            peer_rank_2, peat_ids_string(peer_ids_2).c_str(),
            global_func_id.name_by_idx(parent_func_id),
            global_func_id.name_by_idx(func_id),
            rank, ids_str().c_str());
    */
    return true;
}

PET_Event *PET_TaR_Event::write_event(File_Storage_GZ *gz_MPI, File_Storage_GZ *gz_bind)
{
    is_written = true;
    if (host_event && !host_event->force_write)
        return this;
    write_function_head(gz_MPI);
    gz_MPI->printf(rank, ":%d:%d:%llx:%llx:%d:%lld\n",
                   comm, peer_rank, request_tx, message, tag, t_bytes);
    write_trace_line(gz_MPI);
    // the receive part is only a function-specific line, it shares the trace-specific line of the send part
    write_function_head(gz_MPI);
    gz_MPI->printf(rank, ":%d:%d:%llx:%llx:%d:%lld\n",
                   comm, peer_rank_2, request, message, tag_2, t_bytes_2);

    write_bind(gz_bind);
    write_nested_records(gz_MPI, gz_bind); // progress engine records issued inside this call
    // TODO: check send/recv pointers
    return this;
}

PEAT_Bool PET_TaR_Event::bind()
{
    if (is_bound())
        return true;
#ifdef _PET_USE_BIND_STORE_
    // see the matching #ifdef in PET_ToR_Event::bind() (tor.cpp). A sendrecv's send-half and receive-half
    // share this event's (rank,ids) but are matched independently by mat, so they live in the two different
    // directional maps under the same key (see PET_Bind_Store).
    if (!ready_to_bind())
        return false;
    const PET_Bind_Store &bind_store = get_core()->get_bind_store();
    PET_ToR_Event *recv_event = static_cast<PET_ToR_Event *>(get_peer_for_send());
    if (recv_event == NULL && check_tx()) {
        const PET_Bind_Key *peer_key = bind_store.find_send_peer(rank, ids);
        if (peer_key) {
            PET_Event *peer_event = get_core()->get_pet_rank(peer_key->rank)->find_event_by_ids(peer_key->ids);
            if (peer_event) {
                recv_event = static_cast<PET_ToR_Event *>(peer_event);
                set_peer_for_send(recv_event);
                recv_event->set_peer_for_recv(this);
            }
        }
    }
    PET_ToR_Event *send_event = static_cast<PET_ToR_Event *>(get_peer_for_recv());
    if (send_event == NULL && check_rx()) {
        const PET_Bind_Key *peer_key = bind_store.find_recv_peer(rank, ids);
        if (peer_key) {
            PET_Event *peer_event = get_core()->get_pet_rank(peer_key->rank)->find_event_by_ids(peer_key->ids);
            if (peer_event) {
                send_event = static_cast<PET_ToR_Event *>(peer_event);
                set_peer_for_recv(send_event);
                send_event->set_peer_for_send(this);
            }
        }
    }
    return is_bound();
#else
    if (ready_to_bind()) {
        PET_ToR_Event *recv_event = static_cast<PET_ToR_Event *>(get_peer_for_send());
        if (recv_event == NULL && check_tx()) {
            recv_event = static_cast<PET_ToR_Event *>(get_core()->find_peer(this, false));
            if (recv_event) {
                set_peer_for_send(recv_event);
                recv_event->set_peer_for_recv(this);
            }
        }
        PET_ToR_Event *send_event = static_cast<PET_ToR_Event *>(get_peer_for_recv());
        if (send_event == NULL && check_rx()) {
            send_event = static_cast<PET_ToR_Event *>(get_core()->find_peer(this, true));
            if (send_event) {
                set_peer_for_recv(send_event);
                send_event->set_peer_for_send(this);
            }
        }
        return is_bound();
    }
    return false;
#endif
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
    if (estimation_started && is_non_block()) {
        // MPI_Isendrecv*: the call only posts both directions (handle_event started the send); the transfers
        // are waited for by the record completing its request, which takes this one as its child
        concatenate_event();
        finish_calculations = true;
        submit(e_time.func.out, EET_FUNC_END, this, this, NULL);
        return true;
    }
    if (estimation_started) {
        PEAT_Clock sub_t = 0;
        PEAT_Clock f_in = e_time.mpi.in + event_parts.prolog;
        PEAT_Clock f_out = f_in + event_parts.meat;
        // a direction to/from MPI_PROC_NULL moves nothing: done as soon as the call starts
        PEAT_Clock m_t_in = check_tx() ? get_message_start(PEAT_DIRECTION_TX) : f_in;
        PEAT_Clock m_t_out = check_tx() ? get_ack_finish(PEAT_DIRECTION_TX) : f_in;
        PEAT_Clock m_r_in = check_rx() ? get_message_start(PEAT_DIRECTION_RX) : f_in;
        PEAT_Clock m_r_out = check_rx() ? get_message_finish(PEAT_DIRECTION_RX) : f_in;
        if (m_t_out > 0 && m_r_out > 0) {
            // event_parts.stat.transport = __max__( m_t_out - m_t_in, 0 ) + (m_r_out - m_r_in);
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
            event_parts.stat.transport = get_transport();
            concatenate_event();
            finish_calculations = true;
            submit(e_time.func.out, EET_FUNC_END, this, this, NULL);
            return true;
        }
    }
    return true;
}

/* Finish of one direction: the sender's ends with the receiver's ACK (or its message when not acked), the receiver's
   with its own ACK (or the received message). AUTO_SELECT (a completion waiting for a MPI_Isendrecv*) is the later of
   the two, -1 while either is still running; with both directions on MPI_PROC_NULL, the call's own end. */
PEAT_Clock PET_TaR_Event::get_ack_finish(PEAT_DIRECTION direction) const
{
    PEAT_Clock tx = -1, rx = -1;
    if (check_tx()) {
        if (get_up_status(PEAT_DIRECTION_TX))
            tx = p_ack.finish > 0 ? p_ack.finish + p_ack.tr_env.post_delay : -1;
        else
            tx = get_message_finish(PEAT_DIRECTION_TX);
    }
    if (check_rx()) {
        if (get_up_status(PEAT_DIRECTION_RX))
            rx = ack.finish > 0 ? ack.finish + ack.tr_env.post_delay : -1;
        else
            rx = get_message_finish(PEAT_DIRECTION_RX);
    }
    if (direction == PEAT_DIRECTION_TX)
        return tx;
    if (direction == PEAT_DIRECTION_RX)
        return rx;
    if ((check_tx() && tx < 0) || (check_rx() && rx < 0))
        return -1;
    if (!check_tx() && !check_rx())
        return e_time.mpi.out;
    return __max__(tx, rx);
}
