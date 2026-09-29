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

PEAT_Num PET_ToR_Event::check_for_outstanding_progress_messages(PEAT_Clock clk)
{
    PEAT_Signed_Num pe_type = get_core()->get_pe_type();
    if (!is_non_block() || pe_type > 0)
        return handle_progress_messages(clk, true);
    return 0;
}

PEAT_Bool PET_ToR_Event::is_rdv(PEAT_DIRECTION direction) const
{
    // assert( !(check_rx() && !check_tx()) ); //mpi_recv must not call this
    PEAT_Bool r;
    if (direction == PEAT_DIRECTION_TX && check_tx()) {
        r = (t_bytes >= get_core()->get_eager_threshold(is_smp(direction), global_func_id.check_property(func_id, PEAT_PROP_NON_BLOCK))) || global_func_id.check_property(func_id, PEAT_PROP_DATA_SYNC);
        return r;
    }
    if (direction == PEAT_DIRECTION_RX && check_rx()) {
        r = (t_bytes >= get_core()->get_eager_threshold(is_smp(direction), global_func_id.check_property(peer_func_id, PEAT_PROP_NON_BLOCK))) || global_func_id.check_property(peer_func_id, PEAT_PROP_DATA_SYNC);
        return r;
    }
    // warning(pet_core, this, NULL, "Wrong rdv direction selector.\n");
    return false;
}
PEAT_Bool PET_ToR_Event::is_smp(PEAT_DIRECTION direction) const
{
    return get_core()->get_ptp()->is_SMP_tgt(rank, peer_rank);
};

PEAT_Bool PET_ToR_Event::get_up_status(PEAT_DIRECTION direction) const
{
    if (is_smp(direction))
        return is_rdv(direction); // smp transfers are acked
    return get_core()->get_up_status();
}
PEAT_Bool PET_ToR_Event::check_up_status() const
{
    if (check_tx()) {
        if (is_smp(PEAT_DIRECTION_TX))
            return is_rdv(PEAT_DIRECTION_TX); // smp transfers are acked
        for (std::list<PET_Fabric_Event>::const_iterator fab_event_it = fabric_events.cbegin(); fab_event_it != fabric_events.cend(); ++fab_event_it) {
            if (fab_event_it->is_any_rx() && !fab_event_it->is_peek() && !fab_event_it->is_poll())
                return true;
        }
    }
    if (check_rx()) {
        if (is_smp(PEAT_DIRECTION_RX))
            return is_rdv(PEAT_DIRECTION_RX); // smp transfers are acked
        for (std::list<PET_Fabric_Event>::const_iterator fab_event_it = fabric_events.cbegin(); fab_event_it != fabric_events.cend(); ++fab_event_it) {
            if (fab_event_it->is_any_tx() && !fab_event_it->is_peek() && !fab_event_it->is_poll())
                return true;
        }
    }
    return false;
}

PEAT_Bool PET_ToR_Event::is_short(PEAT_Transports t) const
{
    PEAT_Bool r = t_bytes < get_core()->get_short_threshold(t);
    return r;
}

PEAT_Bool PET_ToR_Event::read_event(PEAT_Rank r_rank, const String_Split &mpi_string, const String_Split &hwc_string, PET_Event *host, const PEAT_Record_Header &hdr)
{
    PET_Local_Event::read_event(r_rank, mpi_string, hwc_string, host, hdr);
    extract(comm, mpi_string.at(hdr.payload + 1), 16);
    extract(peer_rank, mpi_string.at(hdr.payload + 2));
    extract(request, mpi_string.at(hdr.payload + 3), 16);
    extract(message, mpi_string.at(hdr.payload + 4), 16);
    extract(tag, mpi_string.at(hdr.payload + 5));
    extract(t_bytes, mpi_string.at(hdr.payload + 6));
    if (global_func_id.check_property(func_id, PEAT_PROP_PERSISTENT_REQUEST_INIT)) {
        get_core()->add_persistent_request(*this);
    }
    return true;
}
PEAT_Bool PET_ToR_Event::write_bind(File_Storage_GZ *gz_bind)
{
    if (check_tx()) {
        gz_bind->printf(rank, "%d:%s:%d:%s:%s:%d:%s:%lld\n", PEAT_RT_Bind_Peer,
                        global_func_id.name_by_idx(func_id),
                        rank, ids_str().c_str(),
                        global_func_id.name_by_idx(peer_completion_func_id),
                        peer_rank, peat_ids_string(peer_completion_ids).c_str(), t_bytes);
        get_core()->messages_bound[rank]++;

    } /*else {
        gz_bind->printf(rank, "%d:%s:%s:%d:%lld:%lld:%s:%s:%d:%lld:%lld\n", PEAT_RT_Bind_Peer,
            global_func_id.name_by_idx(peer_completion_func_id),
            global_func_id.name_by_idx(peer_func_id),
            peer_rank, peat_ids_string(peer_ids).c_str(),
            global_func_id.name_by_idx(request > 0 ? get_completion()->func_id : func_id),
            global_func_id.name_by_idx(func_id),
            rank, ids_str().c_str());
    }*/
    // if (global_func_id.check_property(func_id, PEAT_PROP_DATA_TX)) get_peer()->src_written();
    // if (global_func_id.check_property(func_id, PEAT_PROP_DATA_RX)) get_peer()->dst_written();
    if (request > 0 && get_completion() != NULL) { // check for orphanity
        gz_bind->printf(rank, "%d:%s:%s:%s:%s\n", PEAT_RT_Bind_Completion,
                        global_func_id.name_by_idx(func_id),
                        ids_str().c_str(),
                        global_func_id.name_by_idx(get_completion()->func_id),
                        get_completion()->ids_str().c_str());
        get_core()->messages_bound[rank]++;
    }
    return true;
}
PET_Event *PET_ToR_Event::write_event(File_Storage_GZ *gz_MPI, File_Storage_GZ *gz_HWC, File_Storage_GZ *gz_bind, const char *mpi_eol, const char *hwc_eol)
{
    is_written = true;
    if (host_event && !host_event->force_write)
        return this;
    PET_Local_Event::write_event(gz_MPI, gz_HWC, gz_bind, "", "");
    gz_MPI->printf(rank, ":%d:%d:%llx:%llx:%d:%lld%s",
                   comm, peer_rank, request, message, tag, t_bytes, mpi_eol);
    gz_HWC->printf(rank, "(%lld)%s", t_bytes, hwc_eol);
    if (!is_persistent_init()) {
        write_bind(gz_bind);
        //		get_peer()->remote_written(check_tx());
    }
    write_nested_records(gz_MPI, gz_HWC, gz_bind, mpi_eol, hwc_eol); // progress engine records issued inside this call
    return this;
}

PEAT_Bool PET_ToR_Event::place_rts(PEAT_Clock clk_start)
{
    if (rts.start < 0) {
        TE_Event rts_evt;
        rts_evt.dst = peer_rank;
        rts_evt.src = rank;
        rts_evt.type = TE_Event_RTS;
        rts_evt.owner = this;
        rts_evt.size = -1; // special transport for rts
        get_core()->assign_transport(rts_evt, func_id, parent_func_id);
        rts_evt.size = 0;
        rts_evt.start = clk_start + rts_evt.tr_env.pre_delay;
        rts_evt.is_blocking = guess_blocking();
        rts = *(get_core()->insert_te_event(&rts_evt));
    }
    return false;
}
PEAT_Bool PET_ToR_Event::place_cts(PEAT_Clock clk_start)
{
    if (cts.start < 0) {
        TE_Event cts_evt;
        cts_evt.dst = peer_rank;
        cts_evt.src = rank;
        cts_evt.owner = this;
        cts_evt.size = -1; // special transport for cts
        cts_evt.type = TE_Event_CTS;
        get_core()->assign_transport(cts_evt, func_id, parent_func_id);
        cts_evt.size = 0;
        cts_evt.start = clk_start + cts_evt.tr_env.pre_delay;
        cts_evt.is_blocking = guess_blocking();
        cts = *(get_core()->insert_te_event(&cts_evt));
    }
    return true;
}
PEAT_Bool PET_ToR_Event::place_message(PEAT_Clock clk_start)
{
    if (tx_message.start < 0) {
        TE_Event send_evt;
        send_evt.dst = peer_rank;
        send_evt.src = rank;
        send_evt.size = t_bytes;
        send_evt.owner = this;
        send_evt.type = TE_Event_Message_TX;
        get_core()->assign_transport(send_evt, translate_func_id(), translate_parent_func_id());
        send_evt.start = clk_start + send_evt.tr_env.pre_delay;
        send_evt.is_blocking = guess_blocking();
        tx_message = *(get_core()->insert_te_event(&send_evt));
    }
    return false;
}
PEAT_Bool PET_ToR_Event::place_ack(PEAT_Clock clk_start)
{
    if (ack.start < 0) {
        TE_Event ack_evt;
        ack_evt.dst = peer_rank;
        ack_evt.src = rank;
        ack_evt.size = -1;
        ack_evt.owner = this;
        ack_evt.type = TE_Event_ACK;
        get_core()->assign_transport(ack_evt, func_id, parent_func_id);
        ack_evt.start = clk_start + ack_evt.tr_env.pre_delay;
        ack_evt.is_blocking = guess_blocking();
        ack = *(get_core()->insert_te_event(&ack_evt));
    }
    return false;
}

PEAT_Bool PET_ToR_Event::bind()
{
    if (is_bound())
        return true;
#ifdef _PET_USE_BIND_STORE_
    // mat matched this record against the whole trace already (any nesting depth, any collective); a NULL
    // here means mat itself couldn't resolve it. Not defined by default for exactly that reason - define
    // _PET_USE_BIND_STORE_ to try/continue this path.
    if (!ready_to_bind())
        return false;
    const PET_Bind_Store &bind_store = get_core()->get_bind_store();
    const PET_Bind_Key *peer_key = check_tx() ? bind_store.find_send_peer(rank, ids) : bind_store.find_recv_peer(rank, ids);
    if (!peer_key)
        return false;
    PET_Event *peer_event = get_core()->get_pet_rank(peer_key->rank)->find_event_by_ids(peer_key->ids);
    if (!peer_event)
        return false; // the peer record hasn't been read yet; bind() is retried later as estimation progresses
    PET_ToR_Event *event = static_cast<PET_ToR_Event *>(peer_event);
    if (check_tx()) {
        set_peer_for_send(event);
        event->set_peer_for_recv(this);
    }
    if (check_rx()) {
        set_peer_for_recv(event);
        event->set_peer_for_send(this);
    }
    return true;
#else
    if (ready_to_bind()) {
        PET_ToR_Event *event = static_cast<PET_ToR_Event *>(get_core()->find_peer(this));
        if (event) {
            if (check_tx()) {
                set_peer_for_send(event);
                event->set_peer_for_recv(this);
            }
            if (check_rx()) {
                set_peer_for_recv(event);
                event->set_peer_for_send(this);
            }
            return true;
        } else {
            return false;
        }
    }
    return false;
#endif
}

PEAT_Bool PET_ToR_Event::safe_to_delete()
{
    PEAT_Bool is_safe = is_written;
    is_safe = is_safe || global_func_id.check_property(func_id, PEAT_PROP_PERSISTENT_REQUEST_INIT); // init functions are like locals
    return is_safe && PET_Local_Event::safe_to_delete();
}

PEAT_Bool PET_ToR_Event::can_be_destroyed()
{
    // if (host_event) return false; // sub event... cannot be destroyed
    PET_Event *completion = get_completion();
    if (!get_core()->get_mo()) {
        if (!check_completion()) {
            return false;
        }
    }
    if (completion) {
        return completion->is_written && PET_Local_Event::can_be_destroyed();
    } else {
        return PET_Local_Event::can_be_destroyed();
    }
}

PEAT_Bool PET_ToR_Event::estimate(Estimation_Event *event)
{
    if (treat_as_local)
        return PET_Local_Event::estimate(event);
    // if ( event ) {
    handle_event(event);
    // }
    if (is_estimated() || finish_calculations)
        return true;
    if (is_non_block()) {
        return estimate_nonblocked();
    } else {
        return estimate_blocked();
    }
    return false;
}

PEAT_Bool PET_ToR_Event::estimate_blocked()
{
    if (has_nested() && !nested_all_estimated())
        return true; // wait for the records issued from progress inside this call
    if (check_tx()) {
        if (estimation_started) {
            PEAT_Clock sub_t = 0;
            PEAT_Clock f_in = e_time.mpi.in + event_parts.prolog;
            // PEAT_Clock m_in = get_message_start(PEAT_DIRECTION_TX);
            PEAT_Clock f_out = f_in + event_parts.meat;
            PEAT_Clock m_out = get_ack_finish(PEAT_DIRECTION_TX);
            PEAT_Bool is_blocking = guess_blocking(); //! is_short(tx_message.tr_env.t_id);
            if (!(is_blocking || is_rdv(PEAT_DIRECTION_TX))) {
                PEAT_Clock cpu_times = get_pre_transport()->get_cpu_times();
                sub_t = f_out - f_in - cpu_times;
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
            } else {
                if (m_out > 0) { // || !(tx_message.is_blocking || is_rdv(false)) && tx_message.start > 0) {

                    // event_parts.stat.fabric = __max__(m_out - m_in, 0);
                    // set wait stat first
                    /* if (f_in > 0 && f_in < m_in) {
                        event_parts.stat.wait = m_in - f_in;
                    } */
                    // then adjust fabric time
                    sub_t = f_out - m_out;
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
        }
    }
    if (check_rx()) {
        if (estimation_started) {
            PEAT_Clock f_in = e_time.mpi.in + event_parts.prolog;
            PEAT_Clock m_in = get_message_start(PEAT_DIRECTION_RX);
            PEAT_Clock f_out = f_in + event_parts.meat;
            PEAT_Clock m_out = get_up_status(PEAT_DIRECTION_RX) ? get_ack_start() : get_message_finish(PEAT_DIRECTION_RX);
            PEAT_Clock sub_t = f_out - m_out;
            if (m_out > 0) {
                // event_parts.stat.fabric = m_out - m_in;
                if (m_out <= f_in) {
                    // transmission ended before recevider starts, "zero" fabric side
                    sub_t = event_parts.meat;
                } else {
                    // set idle time here
                    if (f_in < m_in && f_in > 0) {
                        event_parts.stat.wait = m_in - f_in;
                    }
                }
                sub_t = clamp_sub_t_to_nested(sub_t, f_out);
                if (sub_t > event_parts.meat) {
                    sub_t = event_parts.meat;
                }
                event_parts.adjust(0, sub_t, 0);
                event_parts.stat.overlap = get_overlap();
                event_parts.stat.fabric = get_fabric();
                concatenate_event();
                finish_calculations = true;
                submit(e_time.func.out, EET_FUNC_END, this, this, NULL);
                return true;
            }
        }
    }
    return true;
}

PEAT_Bool PET_ToR_Event::estimate_nonblocked()
{
    if (has_nested() && !nested_all_estimated())
        return true; // wait for the records issued from progress inside this call
    if (is_persistent_init() && estimation_started && !finish_calculations) {
        concatenate_event();
        submit(e_time.func.out, EET_FUNC_END, this, this, NULL);
        finish_calculations = true;
        return true;
    }
    if (check_tx() && estimation_started && !finish_calculations) {
        PEAT_Clock f_in = e_time.mpi.in + event_parts.prolog;
        PEAT_Clock m_in = is_rdv(PEAT_DIRECTION_TX) ? get_rts_start() : get_message_start(PEAT_DIRECTION_TX);
        PEAT_Clock f_out = f_in + event_parts.meat;
        PEAT_Clock m_out = is_rdv(PEAT_DIRECTION_TX) ? get_rts_finish(PEAT_DIRECTION_TX) : get_message_finish(PEAT_DIRECTION_TX);
        PEAT_Bool is_blocking = guess_blocking(); //  is_rdv( PEAT_DIRECTION_TX ) ? rts.is_blocking : tx_message.is_blocking;
        // PEAT_Clock cpu_times = is_rdv() ? rts.get_cpu_times() : tx_message.get_cpu_times();
        if (m_out > 0 || (!(is_blocking) && m_in > 0)) {
            // warning( pet_core, this, "Abnormal path used...\n" );
            // event_parts.stat.fabric = __max__(m_out - m_in, 0);
            /* if (f_in > 0 && f_in < m_in) {
                event_parts.stat.wait = m_in - f_in;
            } */
            if (is_blocking) {
                PEAT_Clock sub_t = f_out - m_out;
                sub_t = clamp_sub_t_to_nested(sub_t, f_out);
                if (sub_t > event_parts.meat)
                    sub_t = event_parts.meat;
                event_parts.adjust(0, sub_t, 0);
            } else {
                // NIC protocol assumes no waiting (delayed send)
                //  sub_t_ae = 0; // all events here are CPU based event_parts.meat;
            }
            concatenate_event();
            finish_calculations = true;
            submit(e_time.func.out, EET_FUNC_END, this, this, NULL);
            return true;
        } else {
            concatenate_event();
            finish_calculations = true;
            submit(e_time.func.out, EET_FUNC_END, this, this, NULL);
            return true;
        }
    }
    if (check_rx() && estimation_started && !finish_calculations) {
        PEAT_Clock f_in = e_time.mpi.in + event_parts.prolog;
        PEAT_Clock m_in = get_message_start(PEAT_DIRECTION_RX) < 0 ? get_cts_start() : get_message_start(PEAT_DIRECTION_RX);
        PEAT_Clock f_out = f_in + event_parts.meat;
        PEAT_Clock m_out = get_message_finish(PEAT_DIRECTION_RX) < 0 ? get_cts_finish(PEAT_DIRECTION_RX) : get_message_finish(PEAT_DIRECTION_RX);
        PEAT_Clock sub_t = f_out - m_out;
        if (m_out > 0) {
            // event_parts.stat.fabric = m_out - m_in;
            if (m_out <= f_in) {
                // transmission ended before recevider starts, "zero" fabric side
                sub_t = event_parts.meat;
            } else {
                // set idle time here
                if (f_in < m_in && f_in > 0) {
                    event_parts.stat.wait = m_in - f_in;
                }
            }
            if (sub_t < 0) {
                // this medians that the message is actually received LATER than this functions ends
                // zero fabric side, handle it later in TEST/WAIT
                sub_t = f_out - f_in;
                // pet_warning( pet_core, this, "Abnormal path used...\n" );
            }
            sub_t = clamp_sub_t_to_nested(sub_t, f_out);
            if (sub_t > event_parts.meat)
                sub_t = event_parts.meat;
            event_parts.adjust(0, sub_t, 0);
            concatenate_event();
        } else {
            // no actual message received during nonbliocking recv, OK
            // sub_t_ae = 0;
            // sub_t_ae_t += sub_t_ae;
            // event_parts.adjust(0, sub_t_ae, 0);
            // event_parts.stat.wait = event_parts.meat;
            concatenate_event();
        }
        finish_calculations = true;
        submit(e_time.func.out, EET_FUNC_END, this, this, NULL);
        return true;
    }
    return true;
}

PEAT_Bool PET_ToR_Event::ready_to_estimate()
{
    return PET_Local_Event::ready_to_estimate() && completion_received();
}

PEAT_Clock PET_ToR_Event::get_message_finish(PEAT_DIRECTION direction) const
{
    if ((check_tx() && direction == PEAT_DIRECTION_AUTO_SELECT) || direction == PEAT_DIRECTION_TX) {
        if (get_up_status(direction)) {
            if (p_ack.finish > 0) {
                PEAT_Clock post_delay = p_ack.tr_env.post_delay;
                return p_ack.finish + post_delay;
            }
        } else {
            if (tx_message.finish > 0) {
                PEAT_Clock post_delay = tx_message.tr_env.post_delay;
                return tx_message.finish + post_delay;
            }
        }
    }
    if ((check_rx() && direction == PEAT_DIRECTION_AUTO_SELECT) || direction == PEAT_DIRECTION_RX) {
        if (rx_message.finish > 0) {
            PEAT_Clock post_delay = rx_message.tr_env.post_delay;
            return rx_message.finish + post_delay;
        }
    }
    return -1;
}

PEAT_Clock PET_ToR_Event::get_ack_finish(PEAT_DIRECTION direction) const
{
    if (check_tx()) {
        if (get_up_status(direction)) {
            if (p_ack.finish > 0) {
                PEAT_Clock post_delay = p_ack.tr_env.post_delay;
                return p_ack.finish + post_delay;
            }
        } else {
            return get_message_finish(direction);
        }
    } else {
        if (get_up_status(direction)) {
            if (ack.finish > 0) {
                PEAT_Clock post_delay = ack.tr_env.post_delay;
                return ack.finish + post_delay;
            }
        } else {
            return get_message_finish(direction);
        }
    }
    return -1;
}

PEAT_Clock PET_ToR_Event::get_rts_finish(PEAT_DIRECTION direction) const
{
    if ((check_tx() && direction == PEAT_DIRECTION_AUTO_SELECT) || direction == PEAT_DIRECTION_TX) {
        if (rts.finish > 0) {
            return rts.finish + rts.tr_env.post_delay;
        }
    }
    if ((check_rx() && direction == PEAT_DIRECTION_AUTO_SELECT) || direction == PEAT_DIRECTION_RX) {
        if (p_rts.finish > 0) {
            return p_rts.finish + p_rts.tr_env.post_delay;
        }
    }
    return -1;
}

PEAT_Clock PET_ToR_Event::get_cts_finish(PEAT_DIRECTION direction) const
{
    if ((check_rx() && direction == PEAT_DIRECTION_AUTO_SELECT) || direction == PEAT_DIRECTION_RX) {
        if (cts.finish > 0) {
            return cts.finish + cts.tr_env.post_delay;
        }
    }
    if ((check_tx() && direction == PEAT_DIRECTION_AUTO_SELECT) || direction == PEAT_DIRECTION_TX) {
        if (p_cts.finish > 0) {
            return p_cts.finish + p_cts.tr_env.post_delay;
        }
    }
    return -1;
}

PEAT_Clock PET_ToR_Event::get_message_start(PEAT_DIRECTION direction) const
{
    if ((check_tx() && direction == PEAT_DIRECTION_AUTO_SELECT) || direction == PEAT_DIRECTION_TX)
        return tx_message.start;
    if ((check_rx() && direction == PEAT_DIRECTION_AUTO_SELECT) || direction == PEAT_DIRECTION_RX)
        return rx_message.start;
    return -1;
}

PEAT_Clock PET_ToR_Event::get_rts_start() const
{
    if (check_tx())
        return rts.start;
    return -1;
}

PEAT_Clock PET_ToR_Event::get_cts_start() const
{
    if (check_rx())
        return cts.start;
    return -1;
}

PEAT_Clock PET_ToR_Event::get_ack_start() const
{
    if (check_rx())
        return ack.start;
    return -1;
}

PET_Event *PET_ToR_Event::get_real_event(PET_Event *evt)
{
    PET_Event *comp_event = evt->get_completion();
    if (comp_event) {
        return comp_event->get_real_event(evt);
    }
    return evt;
}

PET_Fabric_Event *PET_ToR_Event::get_send_event(PEAT_UID index)
{
    PEAT_UID i = 0;
    PET_Fabric_Event *evt = NULL;
    for (std::list<PET_Fabric_Event>::iterator event_it = fabric_events.begin(); event_it != fabric_events.end(); ++event_it) {
        if (event_it->is_tx()) {
            evt = &(*event_it);
            if (index <= i) { // sometimes index=-1
                return evt;
            } else {
                i++;
            }
        }
    }
    return evt; // last event found
}

PET_Fabric_Event *PET_ToR_Event::get_recv_event(PEAT_UID index)
{
    PEAT_UID i = 0;
    PET_Fabric_Event *evt = NULL;
    for (std::list<PET_Fabric_Event>::iterator event_it = fabric_events.begin(); event_it != fabric_events.end(); ++event_it) {
        if (event_it->is_rx()) {
            evt = &(*event_it);
            if (index <= i) { // sometimes index=-1
                return evt;
            } else {
                i++;
            }
        }
    }
    return evt; // last event found
}

PEAT_MPI_Func_ID PET_ToR_Event::translate_func_id() const
{
    switch (func_id) {
    case MPI_SENDRECV_ID:
    case MPI_SENDRECV_REPLACE_ID:
    case MPI_ISEND_ID:
        return MPI_SEND_ID;
    case MPI_ISSEND_ID:
        return MPI_SSEND_ID;
    case MPI_IRSEND_ID:
        return MPI_RSEND_ID;
    case MPIC_ISEND_ID:
        return MPIC_SEND_ID;
    case MPIC_SSEND_ID:
    case MPIC_SENDRECV_ID:
    case MPIC_SENDRECV_REPLACE_ID:
        return MPIC_SEND_ID;
    default:
        return func_id;
    }
}
PEAT_MPI_Func_ID PET_ToR_Event::translate_parent_func_id() const
{
    switch (parent_func_id) {
    case MPI_SENDRECV_ID:
    case MPI_SENDRECV_REPLACE_ID:
    case MPI_ISEND_ID:
        return MPI_SEND_ID;
    case MPI_ISSEND_ID:
        return MPI_SSEND_ID;
    case MPI_IRSEND_ID:
        return MPI_RSEND_ID;
    case MPIC_ISEND_ID:
        return MPIC_SEND_ID;
    case MPIC_SSEND_ID:
    case MPIC_SENDRECV_ID:
    case MPIC_SENDRECV_REPLACE_ID:
        return MPIC_SEND_ID;
    default:
        return func_id;
    }
}

PEAT_Transports PET_ToR_Event::get_msg_transport_id() const
{
    if (check_real_tx()) {
        if (tx_message.start > 0) {
            return tx_message.tr_env.t_id;
        }
        if (rts.start > 0) {
            return rts.tr_env.t_id;
        }
        return PEAT_TRANSPORT_MAX; // No actual transmit occured yet.
    }
    if (check_real_rx()) {
        if (rx_message.start > 0) {
            return rx_message.tr_env.t_id;
        }
        if (cts.start > 0) {
            return cts.tr_env.t_id;
        }
        return PEAT_TRANSPORT_MAX; // No actual receive occured yet.
    }
    if (!is_persistent_init()) {
        pet_throw_error(pet_core, this, NULL, "No assigned transport (Invalid function:Neither TX nor RX).\n");
    }
    return PEAT_TRANSPORT_MAX;
}
PEAT_Trasport_Environment *PET_ToR_Event::get_pre_transport()
{
    fake_evt.dst = peer_rank;
    fake_evt.src = rank;
    fake_evt.type = TE_Event_None;
    fake_evt.size = t_bytes;
    fake_evt.owner = this;
    get_core()->assign_transport(fake_evt, func_id, parent_func_id);
    return &fake_evt.tr_env;
}
PEAT_Bool PET_ToR_Event::split_event()
{
    if (event_parts.duration() > 0)
        return true;
    if (is_persistent_init() || get_core()->get_mo()) {
        return PET_Local_Event::split_event(); //  persistent request inits are like  locals
    }
    PEAT_Time_Parts *core_suggestion = get_core()->get_func_split(this, get_pre_transport()->t_id, t_bytes);
    if (core_suggestion) {
        event_parts = *core_suggestion;
        event_parts.meat = e_time.mpi.duration() - event_parts.cpu_duration();
        o_time.pe.in = o_time.mpi.in + o_event_parts.prolog;
        o_time.pe.out = o_time.mpi.out - o_event_parts.epilog;
    } else {
        // PEAT_Num num_sends = 0, num_recvs = 0, num_isends = 0, num_irecvs = 0, num_waits = 0;
        PET_Local_Event::split_event();
        /*
        for ( std::list <PET_Fabric_Event>::const_iterator fab_event_it = fabric_events.cbegin(); fab_event_it != fabric_events.cend(); ++fab_event_it ) {
            if ( fab_event_it->is_wait() || fab_event_it->is_test() ) {
                num_waits++;
            }
            if ( fab_event_it->is_isend() && check_tx() ) {
                num_isends++;
            }
            if ( fab_event_it->is_irecv() && check_rx() ) {
                num_irecvs++;
            }
            if ( fab_event_it->is_send() && check_tx() ) {
                num_sends++;
            }
            if ( fab_event_it->is_recv() && check_rx() ) {
                num_recvs++;
            }
        }*/
    }
    // if (num_waits >= num_isends + num_irecvs || num_sends > 0 || num_recvs > 0) is_verified = true;
    return true;
}

PEAT_Bool PET_ToR_Event::get_bounds()
{
    if (fabric_events.empty() && !is_persistent_init() && is_send() && !is_non_block()) {
        pet_warning(pet_core, this, "No fabric events during send Suspicious.\n");
        return PET_Local_Event::get_bounds();
    }
    if (is_persistent_init()) {
        return PET_Local_Event::get_bounds();
    }
    PEAT_Bool up_status = check_up_status();
    PEAT_Ratio r = (PEAT_Ratio)e_time.mpi.duration() / (PEAT_Ratio)ut_time.mpi.duration();
    for (std::list<PET_Fabric_Event>::const_iterator fab_event_it = fabric_events.cbegin(); fab_event_it != fabric_events.cend(); ++fab_event_it) {
        PEAT_Bool is_any_tx = fab_event_it->is_any_tx();
        PEAT_Bool is_any_rx = fab_event_it->is_any_rx();
        PEAT_Bool is_any_tx_rx = is_any_tx || is_any_rx;

        if (((is_any_tx_rx && up_status) || (is_any_tx && check_tx()) || (is_any_rx && check_rx())) && is_TT_match(fab_event_it->get_t_type())) {
            PEAT_Clock d2 = ut_time.mpi.out - (fab_event_it->ut_time.func.out - (fab_event_it->ut_time.func.out - fab_event_it->ut_time.mpi.out) / fab_event_it->l_num_calls);
            if (d2 < 0)
                d2 = 0;
            if (ut_time.pe.in <= 0) {
                PEAT_Clock d = fab_event_it->ut_time.func.in + (fab_event_it->ut_time.mpi.in - fab_event_it->ut_time.func.in) / fab_event_it->l_num_calls - ut_time.mpi.in;
                if (d < 0)
                    d = 0;
                ut_time.pe.in = ut_time.mpi.in + d;
                t_time.pe.in = t_time.func.in + (PEAT_Clock)(d * r);
                e_time.pe.in = e_time.func.in + (PEAT_Clock)(d * r);
            }
            if (ut_time.mpi.out - d2 > ut_time.pe.out) {
                ut_time.pe.out = ut_time.mpi.out - d2;
                t_time.pe.out = t_time.mpi.out - (PEAT_Clock)(d2 * r);
                e_time.pe.out = e_time.mpi.out - (PEAT_Clock)(d2 * r);
            }
            // PEAT_Clock dur = e_time.pe.duration();
            // PEAT_Clock u_dur = ut_time.pe.duration();
            if (e_time.pe.in < e_time.mpi.in) {
                char buf[128];
                sprintf(buf, "PE Time (in) is outside function time bounds. (%lld)\n", e_time.mpi.out - e_time.pe.out);
                pet_throw_error(pet_core, this, NULL, buf);
            }
            if (e_time.pe.out > e_time.mpi.out) {
                char buf[128];
                sprintf(buf, "PE Time (out) is outside function time bounds. (%lld)\n", e_time.pe.out - e_time.mpi.out);
                pet_throw_error(pet_core, this, NULL, buf);
            }
        }
    }
    return true;
}
void PET_ToR_Event::handle_event(Estimation_Event *event)
{
    PEAT_Signed_Num pe_type = get_core()->get_pe_type();

    switch (event->type) {
    case EET_FUNC_START:
        estimation_started = true;
        initiate_nested(); // records issued from progress while this call was spinning run alongside it
        if (get_core()->get_mo()) {
            // concatenate_event();
            finish_calculations = true;
            submit(e_time.func.out, EET_FUNC_END, this, this, NULL);
        } else {
            split_event();
            if (!is_persistent_init()) {
                if (check_tx()) {
                    PEAT_Clock appx = 0; // is_non_block() ? event_parts.meat : 0;
                    if (is_rdv(PEAT_DIRECTION_TX)) {
                        submit(__max__(e_time.mpi.in + event_parts.prolog + appx, event->start), EET_RTS, this, this, NULL);
                    } else {
                        submit(__max__(e_time.mpi.in + event_parts.prolog + appx, event->start), EET_MSG_TX, this, this, NULL);
                    }
                }
                if (check_rx()) {
                    if (p_rts.finish > 0) {
                        PEAT_Clock start = __max__(__max__(e_time.mpi.in + event_parts.prolog, get_rts_finish(PEAT_DIRECTION_RX)), event->start);
                        submit(start, EET_CTS, this, this, NULL);
                    }
                    if (rx_message.finish > 0) {
                        if (get_up_status(PEAT_DIRECTION_RX)) {
                            PEAT_Clock start = __max__(__max__(e_time.mpi.in + event_parts.prolog, get_message_finish(PEAT_DIRECTION_RX)), event->start);
                            submit(start, EET_ACK, this, this, NULL);
                        }
                    }
                }
                if (!is_non_block() || pe_type > 0) {
                    handle_progress_messages(__max__(e_time.mpi.in + event_parts.prolog, event->start), false);
                }
            }
        }
        break;
    case EET_FUNC_END:
        // assert(estimation_started);
        if (!estimation_started) {
            pet_throw_error(pet_core, this, event, "Estimation not started.\n");
        }
        set_estimated();
        if (!is_persistent_init())
            put_transport_event_stat();
        if (get_completion_target()) {
            submit(event->start, EET_CHILD_COMPLETED, get_completion_target(), this, NULL);
        }
        break;
    case EET_CHILD_COMPLETED:
        // a nested record (issued from progress inside this call) has been estimated
        if (!handle_nested_completed(event)) {
            pet_throw_error(pet_core, this, event, "Unknown child completed.\n");
        }
        break;

    case EET_RTS:
        // assert(estimation_started);
        // assert(!is_persistent_init());
        if (!estimation_started) {
            pet_throw_error(pet_core, this, event, "Estimation not started.\n");
        }
        if (is_persistent_init()) {
            pet_throw_error(pet_core, this, event, "Persistent requests inits handled other way.\n");
        }
        {
            PET_Fabric_Event *send = get_send_event(parent_event == NULL ? 0L : sub_index());
            PEAT_Clock start = event->start;
            if (send && send->is_non_block()) {
                PEAT_Clock d = send->ut_time.mpi.out - ut_time.mpi.in;
                start = e_time.mpi.in + d;
            }
            place_rts(__max__(event->start, start));

            if ((!is_non_block() || pe_type > 0) && estimation_started && !finish_calculations) {
                handle_progress_messages(__max__(event->start, start), false);
            }
        }
        break;
    case EET_RTS_RECV_NOTIFY:
        if (is_persistent_init()) {
            pet_throw_error(pet_core, this, event, "Persistent requests inits handled other way.\n");
        }
        if ((!is_non_block() || pe_type > 0) && estimation_started && !finish_calculations) {
            handle_progress_messages(event->start, false);
        }
        break;
    case EET_RTS_RECV:
        // assert(!is_persistent_init());
        if (is_persistent_init()) {
            pet_throw_error(pet_core, this, event, "Persistent requests inits handled other way.\n");
        }
        if ((!is_non_block() || pe_type > 0) && estimation_started && !finish_calculations) {
            handle_progress_messages(event->start, false);
        }
        p_rts = event->info;
        if (estimation_started) {
            PEAT_Clock rts_finish = get_rts_finish(PEAT_DIRECTION_RX);
            PEAT_Clock start = __max__(__max__(e_time.mpi.in + event_parts.prolog, rts_finish), event->start);
            submit(start, EET_CTS, this, this, NULL);
        }
        break;
    case EET_CTS:
        // assert(!is_persistent_init());
        if (is_persistent_init()) {
            pet_throw_error(pet_core, this, event, "Persistent requests inits handled other way.\n");
        }
        {
            PEAT_Clock start = event->start;
#if 0
				PET_Fabric_Event* send = get_send_event(parent_event == NULL ? 0 : sub_index());
				if ( send && send->is_non_block() ) {
					PEAT_Clock d = send->ut_time.mpi.out - ut_time.mpi.in;
					start = e_time.mpi.in + d;
				}
#endif
            place_cts(__max__(event->start, start));
            if ((!is_non_block() || pe_type > 0) && estimation_started && !finish_calculations) {
                handle_progress_messages(__max__(event->start, start), false);
            }
        }
        break;
    case EET_CTS_RECV_NOTIFY:
        if (is_persistent_init()) {
            pet_throw_error(pet_core, this, event, "Persistent requests inits handled other way.\n");
        }
        if ((!is_non_block() || pe_type > 0) && estimation_started && !finish_calculations) {
            handle_progress_messages(event->start, false);
        }
        break;
    case EET_CTS_RECV:
        // assert(!is_persistent_init());
        if (is_persistent_init()) {
            pet_throw_error(pet_core, this, event, "Persistent requests inits handled other way.\n");
        }
        p_cts = event->info;
        {
            PEAT_Clock cts_finish = get_cts_finish(PEAT_DIRECTION_TX);
            PEAT_Clock start = __max__(__max__(e_time.mpi.in + event_parts.prolog, cts_finish), event->start);
            submit(start, EET_MSG_TX, this, this, NULL);
        }
        if ((!is_non_block() || pe_type > 0) && estimation_started && !finish_calculations) {
            handle_progress_messages(event->start, false);
        }
        break;

    case EET_MSG_TX:
        // assert(!is_persistent_init());
        if (is_persistent_init()) {
            pet_throw_error(pet_core, this, event, "Persistent requests inits handled other way.\n");
        }
        {
            PET_Fabric_Event *send = get_send_event(parent_event == NULL ? 0L : sub_index());
            PEAT_Clock start = event->start;
#if 1
            if (send && send->is_non_block()) {
                PEAT_Clock d = send->ut_time.mpi.out - ut_time.mpi.in;
                start = e_time.mpi.in + d;
            }
#endif
            place_message(__max__(event->start, start));
            if ((!is_non_block() || pe_type > 0) && estimation_started && !finish_calculations) {
                handle_progress_messages(__max__(event->start, start), false);
            }
        }
        break;
    case EET_MSG_RECV_NOTIFY:
        if (is_persistent_init()) {
            pet_throw_error(pet_core, this, event, "Persistent requests inits handled other way.\n");
        }
        if ((!is_non_block() || pe_type > 0) && estimation_started && !finish_calculations) {
            handle_progress_messages(event->start, false);
        }
        break;
    case EET_MSG_RECV:
        // assert(!is_persistent_init());
        if (is_persistent_init()) {
            pet_throw_error(pet_core, this, event, "Persistent requests inits handled other way.\n");
        }
        rx_message = event->info;
        if ((!is_non_block() || pe_type > 0) && estimation_started && !finish_calculations) {
            handle_progress_messages(event->start, false);
        }
        if (get_up_status(PEAT_DIRECTION_RX)) {
            PEAT_Clock start = __max__(__max__(e_time.mpi.in + event_parts.prolog, get_message_finish(PEAT_DIRECTION_RX)), event->start);
            submit(start, EET_ACK, this, this, NULL);
        }
        break;

    case EET_ACK:
        // assert(!is_persistent_init());
        if (is_persistent_init()) {
            pet_throw_error(pet_core, this, event, "Persistent requests inits handled other way.\n");
        }
        {
            place_ack(event->start);
            if ((!is_non_block() || pe_type > 0) && estimation_started && !finish_calculations) {
                handle_progress_messages(event->start, false);
            }
        }
        break;
    case EET_ACK_RECV_NOTIFY:
        if (is_persistent_init()) {
            pet_throw_error(pet_core, this, event, "Persistent requests inits handled other way.\n");
        }
        if ((!is_non_block() || pe_type > 0) && estimation_started && !finish_calculations) {
            handle_progress_messages(event->start, false);
        }
        break;

    case EET_ACK_RECV:
        if (is_persistent_init()) {
            pet_throw_error(pet_core, this, event, "Persistent requests inits handled other way.\n");
        }
        p_ack = event->info;
        if ((!is_non_block() || pe_type > 0) && estimation_started && !finish_calculations) {
            handle_progress_messages(event->start, false);
        }
        break;
    case EET_PROGRESS_NOTIFY:
        if ((!is_non_block() || pe_type > 0) && estimation_started && !finish_calculations) {
            handle_progress_messages(event->start, false);
        }
        ping_progress(event);
        break;
    case EET_NOTIFY:
        /*if ( is_persistent_init() ) {
            pet_throw_error( pet_core, this, event, "Persistent requests inits handled other way.\n" );
        }*/
        break;
    default:
        pet_throw_error(pet_core, this, event, "Wrong Code.\n");
        // assert(0);
        break;
    }
    PET_Event *current_event = get_current_event();
    /*
        if (current_event != NULL && current_event != this && current_event->ids[0] <= ids[0] && (PET_Event*)current_event->host_event != this) {
            submit(event->start, EET_NOTIFY, current_event,  this, NULL);
        }
    */
    if (host_event && event->type != EET_FUNC_END) {
        submit(event->start, EET_NOTIFY, host_event, this, NULL);
    }
    if (parent_event && parent_event != host_event && parent_event != current_event) {
        submit(event->start, EET_NOTIFY, parent_event, this, NULL);
    }
    get_core()->delete_ee_entry(event);
}
#if 0
PEAT_Bool PET_ToR_Event::on_update(TE_Event* te_event)
{
	PEAT_Signed_Num pe_type = get_core()->get_pe_type();
	PET_ToR_Event* peer;
	switch(te_event->type) {
		case TE_Event_Message_TX:
			tx_message = *te_event;
			peer = (PET_ToR_Event*)get_peer_for_send();
			get_core()->repeat_last_ee_entry(this);
			submit(te_event->finish, EET_NOTIFY, this, this, te_event);
			peer->rx_message = *te_event;
			if(pe_type == 2) {
				if(peer->estimation_started) {
					submit(te_event->finish + te_event->tr_env.general_lat + te_event->tr_env.post_delay, EET_MSG_RECV, peer, this, te_event);
				}
			} else {
				post(te_event->finish + te_event->tr_env.general_lat + te_event->tr_env.post_delay, EET_MSG_RECV, peer, this, te_event);
				submit(te_event->finish + te_event->tr_env.general_lat + te_event->tr_env.post_delay, EET_PROGRESS_NOTIFY, peer, this, te_event);
			}
			break;
		case TE_Event_RTS:
			rts = *te_event;
			peer = (PET_ToR_Event*)get_peer_for_send();
			peer->p_rts = *te_event;
			submit(te_event->finish, EET_NOTIFY, this, this, te_event);
			if(pe_type == 2) {
				if(peer->estimation_started) {
					submit(te_event->finish + te_event->tr_env.general_lat + te_event->tr_env.post_delay, EET_RTS_RECV, peer, this, te_event);
				}
			} else {
				if(peer->estimation_started) {
					post(te_event->finish + te_event->tr_env.general_lat + te_event->tr_env.post_delay, EET_RTS_RECV, peer, this, te_event);
					submit(te_event->finish + te_event->tr_env.general_lat + te_event->tr_env.post_delay, EET_PROGRESS_NOTIFY, peer, this, te_event);
				}
			}
			break;
		case TE_Event_CTS:
		{
			cts = *te_event;
			peer = (PET_ToR_Event*)get_peer_for_recv();
			// post(te_event->finish, EET_CTS_END, this, this, te_event);
			submit(te_event->finish, EET_NOTIFY, this, this, te_event);
			if(pe_type == 2) {
				submit(te_event->finish + te_event->tr_env.general_lat + te_event->tr_env.post_delay, EET_CTS_RECV, peer, this, te_event);
			} else {
				post(te_event->finish + te_event->tr_env.general_lat + te_event->tr_env.post_delay, EET_CTS_RECV, peer, this, te_event);
				submit(te_event->finish + te_event->tr_env.general_lat + te_event->tr_env.post_delay, EET_PROGRESS_NOTIFY, peer, this, te_event);
			}
		}
		break;
		case TE_Event_ACK:
		{
			ack = *te_event;
			peer = (PET_ToR_Event*)get_peer_for_recv();
			submit(te_event->finish, EET_NOTIFY, this, this, te_event);
			if(pe_type == 2) {
				submit(te_event->finish + te_event->tr_env.general_lat + te_event->tr_env.post_delay, EET_ACK_RECV, peer, this, te_event);
			} else {
				post(te_event->finish + te_event->tr_env.general_lat + te_event->tr_env.post_delay, EET_ACK_RECV, peer, this, te_event);
				submit(te_event->finish + te_event->tr_env.general_lat + te_event->tr_env.post_delay, EET_PROGRESS_NOTIFY, peer, this, te_event);
			}
		}
		break;
		default:
			pet_throw_error(pet_core, this, te_event, "Wrong code.\n");
			//assert(0);
			break;
	}
	return false;
}
#else
PEAT_Bool PET_ToR_Event::on_update(TE_Event *te_event)
{
    PEAT_Signed_Num pe_type = get_core()->get_pe_type();
    PET_ToR_Event *peer;
    switch (te_event->type) {
    case TE_Event_Message_TX:
        tx_message = *te_event;
        peer = (PET_ToR_Event *)get_peer_for_send();
        get_core()->repeat_last_ee_entry(this);
        submit(te_event->finish, EET_NOTIFY, this, this, te_event);
        if (pe_type == 2) {
            if (peer->estimation_started) {
                peer->submit(te_event->finish + te_event->tr_env.general_lat + te_event->tr_env.post_delay, EET_MSG_RECV_NOTIFY, peer, this, te_event);
            }
        } else {
            peer->post(te_event->finish + te_event->tr_env.general_lat + te_event->tr_env.post_delay, EET_MSG_RECV_NOTIFY, peer, this, te_event);
            peer->submit(te_event->finish + te_event->tr_env.general_lat + te_event->tr_env.post_delay, EET_PROGRESS_NOTIFY, peer, this, te_event);
        }
        break;
    case TE_Event_Message_RX:
        rx_message = *te_event;
        get_core()->repeat_last_ee_entry(this);
        if (estimation_started) {
            submit(te_event->finish + te_event->tr_env.post_delay, EET_MSG_RECV, this, this, te_event);
        }
        break;
    case TE_Event_RTS:
        rts = *te_event;
        peer = (PET_ToR_Event *)get_peer_for_send();
        submit(te_event->finish, EET_NOTIFY, this, this, te_event);
        if (pe_type == 2) {
            if (peer->estimation_started) {
                peer->submit(te_event->finish + te_event->tr_env.general_lat + te_event->tr_env.post_delay, EET_RTS_RECV_NOTIFY, peer, this, te_event);
            }
        } else {
            if (peer->estimation_started) {
                peer->post(te_event->finish + te_event->tr_env.general_lat + te_event->tr_env.post_delay, EET_RTS_RECV_NOTIFY, peer, this, te_event);
                peer->submit(te_event->finish + te_event->tr_env.general_lat + te_event->tr_env.post_delay, EET_PROGRESS_NOTIFY, peer, this, te_event);
            }
        }
        break;
    case TE_Event_RTS_R:
        p_rts = *te_event;
        if (estimation_started) {
            submit(te_event->finish + te_event->tr_env.post_delay, EET_RTS_RECV, this, this, te_event);
        }
        break;
    case TE_Event_CTS: {
        cts = *te_event;
        peer = (PET_ToR_Event *)get_peer_for_recv();
        submit(te_event->finish, EET_NOTIFY, this, this, te_event);
        if (pe_type == 2) {
            peer->submit(te_event->finish + te_event->tr_env.general_lat + te_event->tr_env.post_delay, EET_CTS_RECV_NOTIFY, peer, this, te_event);
        } else {
            peer->post(te_event->finish + te_event->tr_env.general_lat + te_event->tr_env.post_delay, EET_CTS_RECV_NOTIFY, peer, this, te_event);
            peer->submit(te_event->finish + te_event->tr_env.general_lat + te_event->tr_env.post_delay, EET_PROGRESS_NOTIFY, peer, this, te_event);
        }
    } break;
    case TE_Event_CTS_R: {
        p_cts = *te_event;
        submit(te_event->finish + te_event->tr_env.post_delay, EET_CTS_RECV, this, this, te_event);
    } break;
    case TE_Event_ACK: {
        ack = *te_event;
        peer = (PET_ToR_Event *)get_peer_for_recv();
        submit(te_event->finish, EET_NOTIFY, this, this, te_event);
        if (pe_type == 2) {
            peer->submit(te_event->finish + te_event->tr_env.general_lat + te_event->tr_env.post_delay, EET_ACK_RECV_NOTIFY, peer, this, te_event);
        } else {
            peer->post(te_event->finish + te_event->tr_env.general_lat + te_event->tr_env.post_delay, EET_ACK_RECV_NOTIFY, peer, this, te_event);
            peer->submit(te_event->finish + te_event->tr_env.general_lat + te_event->tr_env.post_delay, EET_PROGRESS_NOTIFY, peer, this, te_event);
        }
    } break;
    case TE_Event_ACK_R: {
        p_ack = *te_event;
        submit(te_event->finish + te_event->tr_env.post_delay, EET_ACK_RECV, this, this, te_event);
    } break;
    default:
        pet_throw_error(pet_core, this, te_event, "Wrong code.\n");
        // assert(0);
        break;
    }
    return false;
}

#endif
PEAT_Bool PET_ToR_Event::check_completion()
{
    if (check_real_tx()) {
        if (is_rdv(PEAT_DIRECTION_TX) && get_up_status(PEAT_DIRECTION_TX)) {
            if (p_ack.finish < 0)
                return false;
        } else {
            if (tx_message.finish < 0)
                return false;
        }
    }
    if (check_real_rx()) {
        if (is_rdv(PEAT_DIRECTION_RX) && get_up_status(PEAT_DIRECTION_RX)) {
            if (ack.finish < 0)
                return false;
        } else {
            if (rx_message.finish < 0)
                return false;
        }
    }
    return true;
}
PET_ToR_Event::~PET_ToR_Event()
{
    if (parent_event)
        parent_event->notify_before_destroy(this);
}

void PET_ToR_Event::notify_before_destroy(PET_Event *event)
{
    if (parent_event == event) {
        parent_event = NULL;
    }
    PET_Event *completion = get_completion();
    if (completion == event) {
        set_completion(NULL);
    }
}
