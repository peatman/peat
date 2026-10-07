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

#include "tv_event.h"

Func_ID global_func_id;

TV_Event::TV_Event() : ids(2, -1), addrs(NULL), func_id(TMPI_FUNC_ID_NONE)
{
}

TV_Event::~TV_Event()
{
    if (addrs)
        delete addrs;
}

PEAT_Bool TV_Event::read_event(PEAT_Rank r_rank, const String_Split &mpi_string, const String_Split &hwc_string, const PEAT_Time &evt_time, const PEAT_Num num_addrs, const PEAT_Record_Header &hdr)
{
    // basic info
    rank = r_rank;
    const int b = hdr.hwc_payload; // first tsc field on the .hwc line
    assert(hwc_string.size() >= static_cast<size_t>(b + 4));
    if (hwc_string.size() < static_cast<size_t>(b + 4))
        return false;
    assert(hdr.same_ids(hwc_string)); // the .hwc twin carries the same ids and thread
    // TSCs block
    if (evt_time.func.out > 0) { // read and corrected in get_event
        e_time = evt_time;
    }
    else { // hasn't read and corrected in get_event
        extract(e_time.func.in, hwc_string.at(b), 10);
        extract(e_time.func.out, hwc_string.at(b + 1), 10);
        extract(e_time.pe.in, hwc_string.at(b + 2), 10);
        extract(e_time.pe.out, hwc_string.at(b + 3), 10);

    }
    addrs = new Event_Address_Info(num_addrs);
    for (int i = 0; i < (int)num_addrs; i++) {
        PEAT_UID addr;
        extract(addr, hwc_string.at(b + 4 + i));
        addrs->addr[i] = addr;
    }
    // sanity check (and if pe information was not written)
    if (e_time.pe.in < e_time.func.in)
        e_time.pe.in = e_time.func.in;
    if (e_time.pe.out < e_time.func.in || e_time.pe.out > e_time.func.out)
        e_time.pe.out = e_time.func.out;
    return true;
}
PEAT_Bool TV_Local_Event::read_event(PEAT_Rank r_rank, const String_Split &mpi_string, const String_Split &hwc_string, const PEAT_Time &evt_time, const PEAT_Num num_addrs, const PEAT_Record_Header &hdr)
{
    rank = r_rank;
    ids = hdr.ids; // the whole path, the bind files refer to it
    thread = hdr.thread_id;
    int funcid;
    extract(funcid, mpi_string.at(hdr.payload), 10, false);
    if (funcid == 0) {
        func_id = global_func_id.idx_by_name(mpi_string.at(hdr.payload));
    } /*else {
        func_id = get_core()->model_id.modelFunctionTags.at((PEAT_MPI_Func_ID)funcid);
    }*/
    TV_Event::read_event(r_rank, mpi_string, hwc_string, evt_time, num_addrs, hdr);
    if (func_id == MPI_PCONTROL_ID) {
        extract(p_control_id, mpi_string.at(hdr.payload), 10, false);
    }
    // update_pending();
    return true;
}
PEAT_Bool TV_Local_Buf_Event::read_event(PEAT_Rank r_rank, const String_Split &mpi_string, const String_Split &hwc_string, const PEAT_Time &evt_time, const PEAT_Num num_addrs, const PEAT_Record_Header &hdr)
{
    TV_Local_Event::read_event(r_rank, mpi_string, hwc_string, evt_time, num_addrs, hdr);
    extract(buf_count, mpi_string.at(hdr.payload + 1));
    extract(buf_size, mpi_string.at(hdr.payload + 2));
    return true;
}
TV_Coll_Event::~TV_Coll_Event()
{
    /*
    PEAT_Num size = t_events.size();
    if (size > 0) {
        for (PEAT_Num i = 0; i < size; i++) {
            delete t_events[i];
        }
    }
    */
}

PEAT_Bool TV_Collective_Event::read_event(PEAT_Rank r_rank, const String_Split &mpi_string, const String_Split &hwc_string, const PEAT_Time &evt_time, const PEAT_Num num_addrs, const PEAT_Record_Header &hdr)
{
    TV_Local_Event::read_event(r_rank, mpi_string, hwc_string, evt_time, num_addrs, hdr);
    extract(comm, mpi_string.at(hdr.payload + 1), 16);
    extract(root, mpi_string.at(hdr.payload + 2));
    extract(request, mpi_string.at(hdr.payload + 3), 16);
    PEAT_Num srb[2];
    extract(srb[0], mpi_string.at(hdr.payload + 4));
    extract(srb[1], mpi_string.at(hdr.payload + 5));
    const int k = hdr.nested ? 0 : 1; // legacy records keep num_colts here; nested records carry num_children in the header

    PEAT_Message_Size sizeMsgCollective = -1;
    if (srb[0] > 0) {
        if (srb[0] == 1) {
            extract(sizeMsgCollective, mpi_string.at(hdr.payload + 6 + k));
            bytes.tx_bytes.set_scalar(sizeMsgCollective);
        } else {
            for (int i = 0; i < srb[0]; i++) {
                extract(sizeMsgCollective, mpi_string.at(hdr.payload + 6 + k + i));
                bytes.tx_bytes.set_vector(sizeMsgCollective, i);
            }
        }
    }
    if (srb[1] > 0) {
        if (srb[1] == 1) {
            extract(sizeMsgCollective, mpi_string.at(hdr.payload + 6 + k + bytes.tx_bytes.size()));
            bytes.rx_bytes.set_scalar(sizeMsgCollective);
        } else {
            for (int i = 0; i < srb[0]; i++) {
                extract(sizeMsgCollective, mpi_string.at(hdr.payload + 6 + k + i + bytes.tx_bytes.size()));
                bytes.rx_bytes.set_vector(sizeMsgCollective, i);
            }
        }
    }
    return true;
}

PEAT_Bool TV_Comm_Event::read_event(PEAT_Rank r_rank, const String_Split &mpi_string, const String_Split &hwc_string, const PEAT_Time &evt_time, const PEAT_Num num_addrs, const PEAT_Record_Header &hdr)
{
    TV_Local_Event::read_event(r_rank, mpi_string, hwc_string, evt_time, num_addrs, hdr);
    extract(comm, mpi_string.at(hdr.payload + 1), 16);
    extract(newcomm, mpi_string.at(hdr.payload + 2), 16);
    extract(remotecomm, mpi_string.at(hdr.payload + 3), 16);
    extract(intercomm, mpi_string.at(hdr.payload + 4), 16);
    extract(request, mpi_string.at(hdr.payload + 5), 16);
    return true;
}

TV_Start_Event::~TV_Start_Event()
{
    for (std::vector<TV_Event *>::iterator event_it = p_events.begin(); event_it != p_events.end(); ++event_it) {
        delete *event_it;
    }
}
PEAT_Bool TV_Start_Event::read_event(PEAT_Rank r_rank, const String_Split &mpi_string, const String_Split &hwc_string, const PEAT_Time &evt_time, const PEAT_Num num_addrs, const PEAT_Record_Header &hdr)
{
    TV_Local_Event::read_event(r_rank, mpi_string, hwc_string, evt_time, num_addrs, hdr);
    extract(input_request_num, mpi_string.at(hdr.payload + 1));
    extract(triggered_request_num, mpi_string.at(hdr.payload + 2));
    extract(flag, mpi_string.at(hdr.payload + 3));
    const int k = hdr.nested ? 0 : 1; // legacy records keep num_colts here
    extract(addr, mpi_string.at(hdr.payload + 4 + k), 16);
    input_requests.resize(input_request_num);
    for (int i = 0; i < input_request_num; i++) {
        PEAT_MPI_Request req;
        extract(req, mpi_string.at(hdr.payload + 5 + k + i), 16);
        input_requests[i] = req;
    }
    for (int i = 0; i < input_request_num; i++) {
        PEAT_MPI_Request req = input_requests[i];
        TV_Event *event = NULL; // get_core()->find_event_by_pers_request(rank, req); //TODO fill this
        if (event) {
            switch (event->get_event_type()) {
            case TV_EVENT_TYPE_TOR: {
                TV_ToR_Event *evt_t = static_cast<TV_ToR_Event *>(event);
                TV_ToR_Event *evt = new TV_ToR_Event(*evt_t);
                tor_events[req] = evt;
                switch (evt->func_id) {
                case MPI_SEND_INIT_ID:
                    evt->func_id = MPI_ISEND_ID;
                    break;
                case MPI_RSEND_INIT_ID:
                    evt->func_id = MPI_IRSEND_ID;
                    break;
                case MPI_BSEND_INIT_ID:
                    evt->func_id = MPI_IBSEND_ID;
                    break;
                case MPI_SSEND_INIT_ID:
                    evt->func_id = MPI_ISSEND_ID;
                    break;
                case MPI_RECV_INIT_ID:
                    evt->func_id = MPI_IRECV_ID;
                    break;
                default:
                    assert(0);
                    break;
                }
                evt->ids = ids;
                p_events.push_back(evt);
            } break;
            case TV_EVENT_TYPE_COLLECTIVE: {
                TV_Collective_Event *evt_t = static_cast<TV_Collective_Event *>(event);
                TV_Collective_Event *evt = new TV_Collective_Event(*evt_t);
                collective_events[req] = evt;
                switch (evt->func_id) {
                    // persistent collective are not in standart yet
                default:
                    assert(0);
                    break;
                }
                evt->ids = ids;
                p_events.push_back(evt);
            } break;
            default:
                assert(0);
                break;
            }
        }
    }
    return true;
}
PEAT_Bool TV_Completion_Event::read_event(PEAT_Rank r_rank, const String_Split &mpi_string, const String_Split &hwc_string, const PEAT_Time &evt_time, const PEAT_Num num_addrs, const PEAT_Record_Header &hdr)
{
    TV_Local_Event::read_event(r_rank, mpi_string, hwc_string, evt_time, num_addrs, hdr);
    extract(input_request_num, mpi_string.at(hdr.payload + 1));
    extract(triggered_request_num, mpi_string.at(hdr.payload + 2));
    extract(flag, mpi_string.at(hdr.payload + 3));
    const int k = hdr.nested ? 0 : 1; // legacy records keep num_colts here
    extract(addr, mpi_string.at(hdr.payload + 4 + k), 16);
    input_requests.resize(input_request_num);
    for (int i = 0; i < input_request_num; i++) {
        PEAT_MPI_Request req;
        extract(req, mpi_string.at(hdr.payload + 5 + k + i), 16);
        input_requests[i] = req;
        if (func_id != MPID_REQUEST_RELEASE_ID) {
            TV_Event *event = NULL; // get_core()->find_event_by_request(rank, req); //TODO fill this
            // if (event == NULL) event = find_event_by_request(req, host);//TODO and this
            if (event) {
                if (event->get_event_type() == TV_EVENT_TYPE_START) {
                    event = NULL; //  event->get_event_from_req(req); //and this
                    assert(event);
                }
            }
        }
    }
    // 4 - tuples(index, tag, source, transferred_bytes)
    for (int i = 0; i < triggered_request_num; i++) {
        TV_Request req;
        int index;
        extract(index, mpi_string.at(hdr.payload + 5 + k + input_request_num + i * 4 + 0));
        extract(req.tag, mpi_string.at(hdr.payload + 5 + k + input_request_num + i * 4 + 1));
        PEAT_Rank src;
        extract(src, mpi_string.at(hdr.payload + 5 + k + input_request_num + i * 4 + 2));
        req.src = src;
        extract(req.t_bytes, mpi_string.at(hdr.payload + 5 + k + input_request_num + i * 4 + 3));
        req.index = index;
        // if (input_requests.at(index) == get_core()->get_comm()->get_MPI_REQUEST_NULL()) continue;
        req.request = input_requests.at(index);
        triggered_requests.push_back(req);
    }
    return true;
}

PEAT_Bool TV_ToR_Event::read_event(PEAT_Rank r_rank, const String_Split &mpi_string, const String_Split &hwc_string, const PEAT_Time &evt_time, const PEAT_Num num_addrs, const PEAT_Record_Header &hdr)
{
    TV_Local_Event::read_event(r_rank, mpi_string, hwc_string, evt_time, num_addrs, hdr);
    extract(comm, mpi_string.at(hdr.payload + 1), 16);
    extract(peer_rank, mpi_string.at(hdr.payload + 2));
    extract(request, mpi_string.at(hdr.payload + 3), 16);
    extract(message, mpi_string.at(hdr.payload + 4), 16);
    extract(tag, mpi_string.at(hdr.payload + 5));
    extract(t_bytes, mpi_string.at(hdr.payload + 6));
    if (global_func_id.check_property(func_id, PEAT_PROP_PERSISTENT_REQUEST_INIT)) {
        // get_core()->add_persistent_request(*this);
    }
    return true;
}

PEAT_Bool TV_TaR_Event::read_event(PEAT_Rank r_rank, const String_Split &mpi_string, const String_Split &hwc_string, const PEAT_Time &evt_time, const PEAT_Num num_addrs, const PEAT_Record_Header &hdr)
{
    if (!process_second_line) {
        TV_Local_Event::read_event(r_rank, mpi_string, hwc_string, evt_time, num_addrs, hdr);
        extract(comm, mpi_string.at(hdr.payload + 1), 16);
        extract(peer_rank, mpi_string.at(hdr.payload + 2)); // COMMWORLD based No need to translate
        /*
        PEAT_MPI_Request request;
        extract(request, mpi_string.at(hdr.payload + 3),16);
        extract(message, mpi_string.at(hdr.payload + 4),16);
        */
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

PEAT_Bool TV_OMPT_Event::read_event(PEAT_Rank r_rank, const String_Split &mpi_string, const String_Split &hwc_string, const PEAT_Time &evt_time, const PEAT_Num num_addrs, const PEAT_Record_Header &hdr)
{
    return TV_Local_Event::read_event(r_rank, mpi_string, hwc_string, evt_time, num_addrs, hdr);
}

PEAT_Bool TV_GPU_Event::read_event(PEAT_Rank r_rank, const String_Split &mpi_string, const String_Split &hwc_string, const PEAT_Time &evt_time, const PEAT_Num num_addrs, const PEAT_Record_Header &hdr)
{
    return TV_Local_Event::read_event(r_rank, mpi_string, hwc_string, evt_time, num_addrs, hdr);
}
