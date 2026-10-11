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

#include "te_timeline.h"
#include "pet_core.h"
#include "pet_event.h"
#include <algorithm>

PEAT_UID TE_Timeline::global_order_id = 0;
TE_Event TE_Timeline::last_deleted_event;

TE_Event TE_Timeline::empty_event;
FILE *TE_Timeline::fp_debug = NULL;

void TE_Event::convert_event(TE_Event *src, PEAT_Clock last_start)
{
    *this = *src;
    PEAT_Clock diff = last_start - src->start;
    if (diff < 0)
        diff = 0;
    start = src->start + diff + src->tr_env.general_lat;
    uniq_id = start;
    stage_finish = src->stage_finish + diff + src->tr_env.general_lat;
    ideal_finish = src->ideal_finish + diff + src->tr_env.general_lat;
    switch (src->type) {
    case TE_Event_RTS:
        type = TE_Event_RTS_R;
        owner = src->owner->get_peer_for_send();
        break;
    case TE_Event_Message_TX:
        type = TE_Event_Message_RX;
        owner = src->owner->get_peer_for_send();
        break;
    case TE_Event_CTS:
        type = TE_Event_CTS_R;
        owner = src->owner->get_peer_for_recv();
        break;
    case TE_Event_ACK:
        type = TE_Event_ACK_R;
        owner = src->owner->get_peer_for_recv();
        break;
    default:
        pet_throw_error(src->owner->pet_core, src->owner, src, "Wrong Message Type.\n");
        break;
    }
    assert(owner);
}

PEAT_BW TE_Timeline::get_bw(TE_Event *evt)
{
    assert(evt);
    return evt->tr_env.general_bw;
}
PEAT_Clock TE_Timeline::get_lat(TE_Event *evt)
{
    assert(evt);
    return (PEAT_Clock)evt->tr_env.general_lat;
}
PEAT_Clock TE_Timeline::get_multiplexing_lat(TE_Event *evt)
{
    assert(evt);
    return (PEAT_Clock)evt->tr_env.cross_packet_delay;
}

PEAT_Bool TE_Timeline::remove_event(TE_Event *evt)
{
    PEAT_Bool deleted = false;
    TE_Queue queue = TE_None;
    switch (evt->type) {
    case TE_Event_RTS:
    case TE_Event_Message_TX:
    case TE_Event_CTS:
    case TE_Event_ACK:
        queue = TE_Write;
        break;
    case TE_Event_RTS_R:
    case TE_Event_CTS_R:
    case TE_Event_Message_RX:
    case TE_Event_ACK_R:
        queue = TE_Read;
        break;
    default:
        assert(0);
        break;
    }
    if (queue == TE_Read) {
        if (read_requests.count(evt->uniq_id) > 0) {
            if (read_requests.at(evt->uniq_id).owner == evt->owner) {
                read_requests.erase(evt->uniq_id);
                deleted = true;
            } else {
                assert(0);
            }
        }
    }
    if (queue == TE_Write) {
        if (write_requests.count(evt->uniq_id) > 0) {
            if (write_requests.at(evt->uniq_id).owner == evt->owner) {
                write_requests.erase(evt->uniq_id);
                deleted = true;
            } else {
                assert(0);
            }
        }
    }
    return deleted;
}

TE_Event *TE_Timeline::insert(TE_Event *evt, PEAT_Bool isperm)
{
    TE_Event inserted_event;
    event_mutex->lock();
    if (evt->start > 0 && evt->uniq_id < 0) {
        evt->uniq_id = evt->start;
    }
    evt->src_timeline = evt->src_timeline == NULL ? this : evt->src_timeline;
    evt->dst_timeline = evt->dst_timeline == NULL ? evt->owner->get_node()->get_transenv(evt->tr_env.t_id) : evt->dst_timeline;
    evt->stage_finish = evt->stage_finish < 0 ? (PEAT_Float)evt->start : evt->stage_finish;
    assert(evt->uniq_id >= 0);
    // assert(evt->start >= last_evt->finish);
    switch (evt->type) {
    case TE_Event_RTS:
    case TE_Event_Message_TX:
    case TE_Event_CTS:
    case TE_Event_ACK:
        std::map<PEAT_UID, TE_Event> *write_requests_local;
        write_requests_local = &write_requests;
        if (evt->start < last_evt.finish) {
            pet_throw_error(evt->owner->pet_core, evt->owner, evt, "Current (te) event starts before already estimated one.\n");
        }
        if (evt->start < last_deleted_event.finish) {
            pet_throw_error(evt->owner->pet_core, evt->owner, evt, "Current (te) event starts before already estimated one.\n");
        }
        while (write_requests_local->count(evt->uniq_id) > 0)
            evt->uniq_id++;
        evt->order_id = order_id++;
        evt->global_order_id = global_order_id;
#pragma omp atomic
        global_order_id++;
        (*write_requests_local)[evt->uniq_id] = *evt;
        if (fp_debug)
            evt->print(fp_debug, "INSERTED: ");
        event_mutex->unlock();
        return &(write_requests_local->at(evt->uniq_id));
        break;
    case TE_Event_RTS_R:
    case TE_Event_CTS_R:
    case TE_Event_Message_RX:
    case TE_Event_ACK_R:
        std::map<PEAT_UID, TE_Event> *read_requests_local;
        read_requests_local = &evt->dst_timeline->get_read_requests();
        while (read_requests_local->count(evt->uniq_id) > 0) {
            if (read_requests_local->at(evt->uniq_id) == *evt)
                return &empty_event;
            evt->uniq_id++;
        }
        /*if (evt->start < last_evt.finish) {
            pet_throw_error(evt->owner->pet_core, evt->owner, evt, "Current (te) event starts before already estimated one.\n");
        }
        if (evt->start < last_deleted_event.finish) {
            pet_throw_error(evt->owner->pet_core, evt->owner, evt, "Current (te) event starts before already estimated one.\n");
        }*/
        evt->order_id = evt->order_id < 0 ? order_id++ : evt->order_id;
        if (evt->global_order_id < 0) {
            evt->global_order_id = global_order_id;
#pragma omp atomic
            global_order_id++;
        }
        (*read_requests_local)[evt->uniq_id] = *evt;
        if (fp_debug)
            evt->print(fp_debug, "INSERTED: ");
        evt->dst_timeline->clear_cache(true);
        event_mutex->unlock();
        return &read_requests_local->at(evt->uniq_id);
        break;
    default:
        assert(0);
        break;
    }
    event_mutex->unlock();
    return &empty_event;
}

TE_Event *PAR_Timeline::insert(TE_Event *evt, PEAT_Bool isperm)
{
    if (evt->size == 0) {
        evt->num_units = 0;
    } else {
        evt->num_units = evt->size / get_unit_length();
        evt->remainder = evt->size % get_unit_length();
        if (evt->remainder == 0) {
            evt->num_units--;
        }
    }
    evt->remainder = get_unit_length();
    PEAT_BW bw = get_bw(evt);
    PEAT_Float time_packet = get_unit_length() / bw;
    PEAT_Float time = (PEAT_Float(evt->num_units + 1)) * time_packet;
    evt->ideal_finish = (PEAT_Clock)(((PEAT_Float)evt->start) + time);
    // evt->is_blocking = true;
    return TE_Timeline::insert(evt, isperm);
}
TE_Event *SEQ_Timeline::insert(TE_Event *evt, PEAT_Bool isperm)
{
    if (evt->size == 0) {
        evt->num_units = 0;
    } else {
        evt->num_units = evt->size / get_unit_length(evt->size);
        evt->remainder = evt->size % get_unit_length(evt->size);
        if (evt->remainder == 0) {
            evt->num_units--;
        }
    }
    evt->remainder = get_unit_length(evt->size);
    PEAT_BW bw = get_bw(evt);
    PEAT_Float time_packet = get_unit_length(evt->size) / bw;
    PEAT_Float time = (PEAT_Float(evt->num_units + 1)) * time_packet;
    evt->ideal_finish = (PEAT_Clock)(((PEAT_Float)evt->start) + time);

    PEAT_Float ideal_finish = (PEAT_Float)evt->start;
    for (PEAT_LNum unit = 0; unit <= evt->num_units; unit++)
        ideal_finish += time_packet;
    if (ideal_finish < evt->ideal_finish) {
        pet_warning(evt->owner->pet_core, evt->owner, "Precision lost during event length idealization\n");
        evt->ideal_finish = (PEAT_Clock)ideal_finish;
    }
    if (evt->get_evt_tr_type() == TR_TYPE_TRANSMIT) {
        if (evt->start < tw_event.t_finish) {
            SEQ_Timeline::clear_cache(false); // tw_event = empty_event; // clear cache
        }
    }
    if (evt->get_evt_tr_type() == TR_TYPE_RECEIVE) {
        if (evt->start < tr_event.t_finish) {
            SEQ_Timeline::clear_cache(true); // tr_event = empty_event; // clear cache
        }
    }
    return TE_Timeline::insert(evt, isperm);
}
void SEQ_Timeline::check_and_correct_write(PEAT_Clock clk)
{
    tw_event.valid_till = clk;
    if (tw_event.t_finish > 0 && tw_event.t_finish < tw_event.ideal_finish) {
        pet_warning(tw_event.owner->pet_core, tw_event.owner, "Actual finish is earlier than ideal(selection): precision lost\n");
        tw_event.t_finish = tw_event.ideal_finish;
    }
}
void SEQ_Timeline::check_and_correct_read(PEAT_Clock clk)
{
    tr_event.valid_till = clk;
    if (tr_event.t_finish > 0 && tr_event.t_finish < tr_event.ideal_finish) {
        pet_warning(tr_event.owner->pet_core, tr_event.owner, "Actual finish is earlier than ideal(selection): precision lost\n");
        tr_event.t_finish = tr_event.ideal_finish;
    }
}
void SEQ_Timeline::mark_reqs_orphan(TE_Event *evt)
{
    for (std::map<PEAT_UID, TE_Event>::iterator evt_it = read_requests.begin(); evt_it != read_requests.end(); ++evt_it) {
        if (evt_it->second.parent_event == evt) {
            evt_it->second.parent_event = NULL;
        }
    }
}

TE_Event *SEQ_Timeline::insert_req_chunk(TE_Event *evt, PEAT_Clock last_start, PEAT_Bool write)
{
    TE_Event tmp = *evt;
    tmp.convert_event(evt, last_start);
#if 1
    TE_Event *inserted_chunk = TE_Timeline::insert(&tmp, write); // skip renumertating
    inserted_chunk->parent_event = evt;
    return inserted_chunk;
#else
    if (write) {
        std::map<PEAT_UID, TE_Event> &target_rr = evt->dst_timeline->get_read_requests();
        while (target_rr.find(tmp.stage_finish) != target_rr.end())
            tmp.stage_finish++;
        target_rr[tmp.stage_finish] = tmp;
        return &target_rr.at(tmp.stage_finish);
    } else {
        std::map<PEAT_UID, TE_Event> &target_rr = evt->dst_timeline->get_temp_read_requests();
        while (target_rr.find(tmp.stage_finish) != target_rr.end())
            tmp.stage_finish++;
        target_rr[tmp.stage_finish] = tmp;
        return &target_rr.at(tmp.stage_finish);
    }
#endif
}

static PEAT_Bool find_iterator(std::list<TE_Event> &list, PEAT_UID uniq_id, std::list<TE_Event>::iterator &evt_it_out)
{
    for (std::list<TE_Event>::iterator evt_it = list.begin(); evt_it != list.end(); ++evt_it) {
        if ((*evt_it).uniq_id == uniq_id) {
            evt_it_out = evt_it;
            return true;
        }
    }
    return false;
}
TE_Event *SEQ_Timeline::update_write(PEAT_Clock clk, PEAT_Bool write)
{
    if (!write) {
        if (tw_event.finish > 0 && tw_event.finish <= clk) {
            return &tw_event;
        }
        if (tw_event.valid_till >= clk && tw_event.finish < 0) {
            return NULL;
        }
        SEQ_Timeline::clear_cache(false);
        // tw_event = empty_event;
    }
    std::list<TE_Event> active_events_wrk = active_events;
    std::map<PEAT_UID, TE_Event> write_requests_wrk = write_requests;
    std::map<PEAT_Clock, TE_Event>::iterator r_it = write_requests_wrk.begin();
    PEAT_Float prev_finish = last_evt.stage_finish; // since it's float
    PEAT_Float last_start = prev_finish;
    PEAT_Clock max_allowed_clock = clk;

    std::list<TE_Event>::iterator e_it = active_events_wrk.begin();
    PEAT_Float cur_start = 0;

    if (active_events_it != active_events.end()) {
    reacquire:
        if (!find_iterator(active_events_wrk, (*active_events_it).uniq_id, e_it)) {
            pet_throw_error(NULL, NULL, NULL, "Finding wrk_event_it failed");
        };
    } else {
        if (!active_events.empty()) {
            active_events_it = active_events.begin();
            goto reacquire;
        }
    }
    while (!(write_requests_wrk.empty() && active_events_wrk.empty())) {
        TE_Event *evt = NULL;
        if (!write_requests_wrk.empty()) {
            // write_request is not empty - > get soonest event
            evt = &r_it->second;
            cur_start = (PEAT_Float)evt->start;
        }
        cur_start = __max__(cur_start, prev_finish);
    continue_filling:
        if (!active_events_wrk.empty()) {
            PEAT_Num ae_size = active_events_wrk.size();
            while (cur_start >= last_start || write_requests_wrk.empty()) { // work only with events in active_events list until new one arives
                TE_Event *evt = &(*e_it);
                assert(evt->finish > evt->start || evt->finish < 0);
                PEAT_BW bw = get_bw(evt);
                PEAT_Clock mult_lat = get_multiplexing_lat(evt);
                PEAT_Float time_mtu = ((PEAT_Float)(get_unit_length(evt->size)) / bw);
                if (evt->num_units > 0) {
                    if (estimate_read)
                        insert_req_chunk(evt, last_start, write);
                    last_start += time_mtu + mult_lat * (ae_size > 1);
                    evt->num_units--;
                    evt->stage_finish = last_start;
                    ++e_it;
                    if (last_start > max_allowed_clock) {
                        if (!write) {
                            check_and_correct_write(clk);
                            return NULL;
                        }
                        pet_throw_error(evt->owner->pet_core, evt->owner, evt, "Updating (SEQ/TE) Pipeline Error");
                    };

                } else {
                    insert_req_chunk(evt, last_start, write); // last chunk is always inserted
                    PEAT_Float time_packet = ((PEAT_Float)(evt->remainder) / bw);
                    last_start += time_packet;
                    evt->stage_finish = last_start;
                    evt->finish = evt->t_finish = (PEAT_Clock)ceil(last_start);
                    if (last_start > max_allowed_clock) {
                        if (!write) {
                            check_and_correct_write(clk);
                            return NULL;
                        }
                        pet_throw_error(evt->owner->pet_core, evt->owner, evt, "Updating (SEQ/TE) Pipeline Error");
                    };
                    if (write) {
                        tuw_event = *evt;
                        e_it = active_events_wrk.erase(e_it); // remove current_event and
                        if (e_it == active_events_wrk.end()) {
                            e_it = active_events_wrk.begin();
                        }
                        active_events = active_events_wrk;
                        if (active_events.empty()) {
                            active_events_it = active_events.begin();
                        } else {
                            if (!find_iterator(active_events, (*e_it).uniq_id, active_events_it)) {
                                pet_throw_error(evt->owner->pet_core, evt->owner, evt, "Finding event_it failed");
                            };
                        }
                        write_requests = write_requests_wrk;
                        if (tw_event.uniq_id != tuw_event.uniq_id) {
                            pet_throw_error(tuw_event.owner->pet_core, tuw_event.owner, &tuw_event, "Write/no_write mistmatch");
                        }
                        update_completed_event(false);
                        return &tuw_event;
                    } else {
                        tw_event = *evt;
                        check_and_correct_write(clk);
                        return &tw_event;
                    }
                }
                if (e_it == active_events_wrk.end())
                    e_it = active_events_wrk.begin();
            };
        }
        assert(evt);
        if (evt == NULL) {
            // write_request is not empty - > get soonest event
            pet_throw_error(tuw_event.owner->pet_core, tuw_event.owner, &tuw_event, "Event wasn't read on time");
            evt = &r_it->second;
            cur_start = (PEAT_Float)evt->start;
        }
        cur_start = __max__(cur_start, last_start);
        if (active_events_wrk.size() > get_max_chain_num())
            goto continue_filling;
        last_start = __max__(last_start, cur_start);
        PEAT_BW bw = get_bw(evt);
        PEAT_Float time_mtu = ((PEAT_Float)(get_unit_length(evt->size)) / bw);
        PEAT_BW bw_packet = bw;

        if (evt->num_units > 0) {
            if (estimate_read)
                insert_req_chunk(evt, last_start, write);
            evt->num_units--;
            last_start += time_mtu;
            evt->stage_finish = last_start;
            if (last_start > max_allowed_clock) {
                if (!write) {
                    check_and_correct_write(clk);
                    return NULL;
                }
                pet_throw_error(evt->owner->pet_core, evt->owner, evt, "Updating (SEQ/TE) Pipeline Error");
            };
            active_events_wrk.push_back(*evt);
            if (e_it == active_events_wrk.end())
                e_it = active_events_wrk.begin();
        } else {
            insert_req_chunk(evt, last_start, write); // last chunk is always inserted
            PEAT_Float time_packet = (PEAT_Float)(evt->remainder) / bw_packet;
            last_start += time_packet;
            evt->stage_finish = last_start;
            evt->finish = evt->t_finish = (PEAT_Clock)ceil(last_start);
            if (last_start > max_allowed_clock) {
                if (!write) {
                    check_and_correct_write(clk);
                    return NULL;
                }
                pet_throw_error(evt->owner->pet_core, evt->owner, evt, "Updating (SEQ/TE) Pipeline Error");
            };
            if (write) {
                tuw_event = *evt;
                active_events = active_events_wrk;
                if (!active_events.empty()) { // write active_events related_stuff back
                    if (!find_iterator(active_events, (*e_it).uniq_id, active_events_it)) {
                        pet_throw_error(evt->owner->pet_core, evt->owner, evt, "Finding event_it failed");
                    };
                    if (active_events_it == active_events.end()) {
                        active_events_it = active_events.begin();
                    }
                }
                write_requests = write_requests_wrk; // and write requests as well
                if (tw_event.uniq_id != tuw_event.uniq_id) {
                    pet_throw_error(tuw_event.owner->pet_core, tuw_event.owner, &tuw_event, "Write/no_write mistmatch");
                }
                update_completed_event(false);
                return &tuw_event;
            } else {
                tw_event = *evt;
                check_and_correct_write(clk);
                return &tw_event;
            }
        }
        prev_finish = last_start;
        r_it = write_requests_wrk.erase(r_it);
        assert(r_it == write_requests_wrk.begin());
        // assert(prev_finish <= evt->start);
    }
    if (write)
        pet_throw_error(NULL, NULL, NULL, "Event Stage Finish calculation error.");
    return NULL;
}
TE_Event *SEQ_Timeline::re_arrange_chunks(PEAT_Clock clk, PEAT_Bool write)
{
    PET_Event *prev_evt_owner = NULL;
    if (!write) {
        PEAT_Float cur_clock = last_read_clock;
        // for (std::map <PEAT_UID, TE_Event>::iterator req_it = temp_read_requests.begin(); req_it != temp_read_requests.end(); ++req_it) {
        for (std::map<PEAT_UID, TE_Event>::iterator req_it = read_requests.begin(); req_it != read_requests.end(); ++req_it) {
            TE_Event *evt = &req_it->second;
            PEAT_BW bw = get_bw(evt);
            PEAT_Clock mult_lat = get_multiplexing_lat(evt);
            PEAT_Float time_mtu = ((PEAT_Float)(get_unit_length(evt->size)) / bw);
            if (prev_evt_owner == evt->owner)
                mult_lat = 0; // chunks from the same request
            cur_clock = __max__(cur_clock + time_mtu + mult_lat, evt->stage_finish + time_mtu + mult_lat);
            if (cur_clock > clk)
                return NULL;
            if (evt->num_units == 0) {
                tr_event = *evt;
                tr_event.finish = tr_event.t_finish = (PEAT_Clock)ceil(cur_clock);
                return &tr_event;
            }
            prev_evt_owner = evt->owner;
        }
    } else {
        PEAT_Float cur_clock = last_read_clock;
        /*
        for (std::map <PEAT_UID, TE_Event>::iterator req_it = temp_read_requests.begin(); req_it != temp_read_requests.end();) {
            TE_Event* evt = &req_it->second;
            PEAT_BW bw = get_bw(evt);
            PEAT_Clock mult_lat = get_multiplexing_lat(evt);
            PEAT_Float time_mtu = ((PEAT_Float)(get_unit_length()) / bw);
            cur_clock = __max__(cur_clock + time_mtu, evt->stage_finish + time_mtu);
            if (cur_clock > clk) return NULL;
            if (evt->num_units == 0) {
                temp_read_requests.erase(req_it);
                break;
            } else {
                req_it = temp_read_requests.erase(req_it);
            }
        }
        cur_clock = last_read_clock;*/
        for (std::map<PEAT_UID, TE_Event>::iterator req_it = read_requests.begin(); req_it != read_requests.end();) {
            TE_Event *evt = &req_it->second;
            PEAT_BW bw = get_bw(evt);
            PEAT_Clock mult_lat = get_multiplexing_lat(evt);
            PEAT_Float time_mtu = ((PEAT_Float)(get_unit_length(evt->size)) / bw);
            cur_clock = __max__(cur_clock + time_mtu, evt->stage_finish + time_mtu);
            if (prev_evt_owner == evt->owner)
                mult_lat = 0; // chunks from the same request
            if (cur_clock > clk)
                return NULL;
            prev_evt_owner = evt->owner;
            if (evt->num_units == 0) {
                tur_event = *evt;
                last_read_clock = cur_clock;
                tur_event.finish = tur_event.t_finish = (PEAT_Clock)ceil(cur_clock);
                update_completed_event(true);
                return &tur_event;
            } else {
                req_it = read_requests.erase(req_it);
            }
        }
    }
    return NULL;
}
TE_Event *SEQ_Timeline::update_read(PEAT_Clock clk, PEAT_Bool write)
{
    if (!write) {
        if (tr_event.finish > 0 && tr_event.finish <= clk) {
            return &tr_event;
        }
        if (tr_event.valid_till >= clk && tr_event.finish < 0) {
            return NULL;
        }
        SEQ_Timeline::clear_cache(true); // tr_event = empty_event;
    }
    return re_arrange_chunks(clk, write);
}
TE_Event *TE_Timeline::update_completed_event(PEAT_Bool isread)
{
    if (isread) {
        if (tur_event.finish < 0)
            tur_event.finish = tur_event.t_finish;
        if (fp_debug)
            tur_event.print(fp_debug, "COMPLETED:");
        // assert(evt->finish > evt->start);
        if (tur_event.finish < tur_event.start) {
            pet_throw_error(tur_event.owner->pet_core, tur_event.owner, &tur_event, "Empty Event is being tried to be completed.\n");
        }
        // assert(evt->ideal_finish <= evt->finish);
        if (tur_event.finish < tur_event.ideal_finish) {
            pet_warning(tur_event.owner->pet_core, tur_event.owner, "Actual finish is earlier than ideal: precision lost\n");
            tur_event.finish = tur_event.ideal_finish;
        }
        tur_event.completed_id = completed_id++;
        // assert(evt->finish >= last_evt.finish);
        if (tur_event.finish < last_evt.finish) {
            pet_throw_error(tur_event.owner->pet_core, tur_event.owner, &tur_event, "Current (te)event ends before already estimated one.\n");
        }
        if (tur_event.finish < last_deleted_event.finish) {
            pet_throw_error(tur_event.owner->pet_core, tur_event.owner, &tur_event, "Current (te)event ends before already estimated one.\n");
        }
        tur_event.owner->on_update(&tur_event);
        last_deleted_event = tur_event;
        last_evt = tur_event;
        remove_event(&tur_event);
        return &tur_event;

    } else {
        if (tuw_event.finish < 0)
            tuw_event.finish = tuw_event.t_finish;
        if (fp_debug)
            tuw_event.print(fp_debug, "COMPLETED:");
        // assert(evt->finish > evt->start);
        if (tuw_event.finish < tuw_event.start) {
            pet_throw_error(tuw_event.owner->pet_core, tuw_event.owner, &tuw_event, "Empty Event is being tried to be completed.\n");
        }
        // assert(evt->ideal_finish <= evt->finish);
        if (tuw_event.finish < tuw_event.ideal_finish) {
            pet_warning(tuw_event.owner->pet_core, tuw_event.owner, "Actual finish is earlier than ideal: precision lost\n");
            tuw_event.finish = tuw_event.ideal_finish;
        }
        tuw_event.completed_id = completed_id++;
        // assert(evt->finish >= last_evt.finish);
        if (tuw_event.finish < last_evt.finish) {
            pet_throw_error(tuw_event.owner->pet_core, tuw_event.owner, &tuw_event, "Current (te)event ends before already estimated one.\n");
        }
        if (tuw_event.finish < last_deleted_event.finish) {
            pet_throw_error(tuw_event.owner->pet_core, tuw_event.owner, &tuw_event, "Current (te)event ends before already estimated one.\n");
        }
        tuw_event.owner->on_update(&tuw_event);
        last_deleted_event = tuw_event;
        last_evt = tuw_event;
        remove_event(&tuw_event);
        return &tuw_event;
    }
}

TE_Event *SEQ_Timeline::update_completed_event(PEAT_Bool isread)
{
    //  if (isread) tr_event = empty_event; else tw_event = empty_event; // clear cache
    SEQ_Timeline::clear_cache(isread);
    return TE_Timeline::update_completed_event(isread);
}

TE_Event *PAR_Timeline::update_write(PEAT_Clock clk, PEAT_Bool write)
{
    // PEAT_Rank rank = caller_event->owner->rank;
    PEAT_Clock max_allowed_clock = clk;
    // first build working set, ordering it by time
    TE_Event *evt, *ret_evt = NULL;
    for (std::map<PEAT_UID, TE_Event>::iterator r_it = write_requests.begin(); r_it != write_requests.end(); ++r_it) {
        evt = &r_it->second;
        assert(evt->finish > evt->start || evt->finish < 0);
        if (evt->finish > 0) {
            assert(0);
            continue; // already estimated but not removed yet. don't touch
        }
        assert(evt->start > 0);
        PEAT_Clock finish = evt->ideal_finish;
        if (finish < max_allowed_clock) {
            if (ret_evt == NULL) {
                ret_evt = evt;
            } else {
                if (ret_evt->ideal_finish > finish) {
                    ret_evt = evt;
                }
            }
        }
        if (evt->start > max_allowed_clock)
            break;
    }
    if (ret_evt) {
        tuw_event = *ret_evt;
        if (write) {
            tuw_event.finish = tuw_event.ideal_finish;
            TE_Event inserted_event;
            inserted_event.convert_event(ret_evt, 0);
            inserted_event.dst_timeline->insert(&inserted_event, write);
            update_completed_event(false);
            return &tuw_event;
        } else {
            tuw_event.t_finish = tuw_event.ideal_finish;
            return &tuw_event;
        }
    }

    return NULL;
}
TE_Event *PAR_Timeline::update_read(PEAT_Clock clk, PEAT_Bool write)
{
    // PEAT_Rank rank = caller_event->owner->rank;
    PEAT_Clock max_allowed_clock = clk;
    // first build working set, ordering it by time
    TE_Event *evt, *ret_evt = NULL;
    for (std::map<PEAT_UID, TE_Event>::iterator r_it = read_requests.begin(); r_it != read_requests.end(); ++r_it) {
        evt = &r_it->second;
        assert(evt->finish > evt->start || evt->finish < 0);
        if (evt->finish > 0) {
            assert(0);
            continue; // already estimated but not removed yet. don't touch
        }
        assert(evt->start > 0);
        PEAT_Clock finish = evt->ideal_finish;
        if (finish < max_allowed_clock) {
            if (ret_evt == NULL) {
                ret_evt = evt;
            } else {
                if (ret_evt->ideal_finish > finish) {
                    ret_evt = evt;
                }
            }
        }
        if (evt->start > max_allowed_clock)
            break;
    }
    if (ret_evt) {
        tur_event = *ret_evt;
        if (write) {
            tur_event.finish = tur_event.ideal_finish;
            update_completed_event(true);
            return &tur_event;
        } else {
            tur_event.t_finish = tur_event.ideal_finish;
            return &tur_event;
        }
    }

    return NULL;
}
#if 0
TE_Event* PAR_Timeline::update(PEAT_Clock clk, PEAT_Bool write)
{
	//PEAT_Rank rank = caller_event->owner->rank;
	PEAT_Clock max_allowed_clock = clk;
	//first build working set, ordering it by time
	TE_Event* read_evt, * read_ret_evt = NULL;
	TE_Event* write_evt, * write_ret_evt = NULL;
	for(std::map <PEAT_UID, TE_Event>::iterator r_it = write_requests.begin(); r_it != write_requests.end(); ++r_it) {
		write_evt = &r_it->second;
		assert(write_evt->finish > write_evt->start || write_evt->finish < 0);
		if(write_evt->finish > 0) {
			assert(0);
			continue; // already estimated but not removed yet. don't touch
		}
		assert(write_evt->start > 0);
		PEAT_Clock finish = write_evt->ideal_finish;
		if(finish < max_allowed_clock) {
			if(write_ret_evt == NULL) {
				write_ret_evt = write_evt;
			} else {
				if(write_ret_evt->ideal_finish > finish) {
					write_ret_evt = write_evt;
				}
			}
		}
		if(write_evt->start > max_allowed_clock) break;
	}
	for(std::map <PEAT_UID, TE_Event>::iterator r_it = read_requests.begin(); r_it != read_requests.end(); ++r_it) {
		read_evt = &r_it->second;
		assert(read_evt->finish > read_evt->start || read_evt->finish < 0);
		if(read_evt->finish > 0) {
			assert(0);
			continue; // already estimated but not removed yet. don't touch
		}
		assert(read_evt->start > 0);
		PEAT_Clock finish = read_evt->ideal_finish;
		if(finish < max_allowed_clock) {
			if(read_ret_evt == NULL) {
				read_ret_evt = read_evt;
			} else {
				if(read_ret_evt->ideal_finish > finish) {
					read_ret_evt = read_evt;
				}
			}
		}
		if(read_evt->start > max_allowed_clock) break;
	}

	TE_Event* ret_evt = NULL;
	if(read_ret_evt && write_ret_evt) {
		// select minimum
		if(write_ret_evt->ideal_finish < read_ret_evt->ideal_finish) {
			ret_evt = write_ret_evt;
		} else {
			ret_evt = read_ret_evt;
		};
	}
	if(read_ret_evt && !write_ret_evt) {
		ret_evt = read_ret_evt;
	}
	if(!read_ret_evt && write_ret_evt) {
		ret_evt = write_ret_evt;
	}
	if(ret_evt) {
		tu_event = *ret_evt;
		if(write) {
			tu_event.finish = tu_event.ideal_finish;
			if(ret_evt == write_ret_evt) {
				TE_Event inserted_event;
				inserted_event.convert_and_insert_event(ret_evt);
				inserted_event.dst_timeline->insert(&inserted_event);
			}
			update_completed_event();
			return &tu_event;
		} else {
			tu_event.t_finish = tu_event.ideal_finish;
			return &tu_event;
		}
	}

	return NULL;
}
#endif
TE_Event *TE_Timeline::complete(TE_Event *evt)
{
    if (evt->get_evt_tr_type() == TR_TYPE_TRANSMIT)
        return update_write(LLONG_MAX, true);
    if (evt->get_evt_tr_type() == TR_TYPE_RECEIVE)
        return update_read(LLONG_MAX, true);
    return NULL;
}
