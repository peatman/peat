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

#include "tevent.h"
#include "communicators.h"
#include "utils.h"

#include <assert.h>
#include <sstream>
#include <stdexcept>

void Tractor_Event::get_extra_txrx(const String_Split &line, const Communicators *com_store)
{

    /*
    const int block_main_event_info_length = 5;
    const int block_tsc_start = block_main_event_info_length;
    const int block_hw_start = block_tsc_start + com_store->frames.at(rank).size_tsc_block;
    const int block_auxiliary_start = block_hw_start + com_store->frames.at(rank).size_hw_block;
    PEAT_Signed_Num aux_fields_num = 0; extract(aux_fields_num, line.at(block_auxiliary_start));
    assert(aux_fields_num == 3);
    extract(P2P.peer, line.at(block_auxiliary_start + 1), 16);
    extract(P2P.tag, line.at(block_auxiliary_start+2), 16);
    extract(P2P.bytes, line.at(block_auxiliary_start+3), 16);
    */
}
Tractor_Event::Tractor_Event(const PEAT_Rank r, const String_Split &line_mpi, const String_Split &line_hwc, const PEAT_Num num_hwcs, const PEAT_Num num_addrs, Address_File_Storage *user_region_file, const Communicators *com_store) : rank(r), num_colts(0)
{
    // no need to check line.size() due line.at() throw an exception
    //  First read fixed part
    extract(func_id, line_mpi.at(3), 10, false);
    if (func_id == TMPI_FUNC_ID_NONE) {
        func_id = global_func_id.idx_by_name(line_mpi.at(3));
    }
    extract(primary_id, line_mpi.at(0));
    extract(secondary_id, line_mpi.at(1));
    extract(thread_id, line_mpi.at(2));
    PEAT_UID l_primary_id, l_secondary_id;
    PEAT_Thread l_thread_id;
    extract(l_primary_id, line_hwc.at(0));
    extract(l_secondary_id, line_hwc.at(1));
    extract(l_thread_id, line_hwc.at(2));
    assert(l_primary_id == primary_id);
    assert(l_secondary_id == secondary_id);
    assert(l_thread_id == thread_id);
    extract(tsc_time.func.in, line_hwc.at(3), 10);
    extract(tsc_time.mpi.in, line_hwc.at(4), 10);
    extract(tsc_time.mpi.out, line_hwc.at(5), 10);
    extract(tsc_time.func.out, line_hwc.at(6), 10);
    for (int i = 0; i < (int)num_addrs; i++) {
        PEAT_UID addr;
        extract(addr, line_hwc.at(7 + i));
        addrs.push_back(addr);
        // addrs.push_back(user_region_file->get_addr_by_id(rank, addr)->addr);
    }
    //  Hardware counters are following to addrs (num_hwcs)
    // memory info is 8 cells, since not widely used  skip for now
    PEAT_Num hwc_size = (PEAT_Num)line_hwc.size();
    for (PEAT_Num i = 7 + num_hwcs + num_addrs; i < hwc_size; i += 2) {
        PEAT_MPI_Memory_Buffer buf;
        if (strlen(line_hwc.at(i)) > 0) { // bug in soe versions of tractor
            extract(buf.addr, line_hwc.at(i), 16);
            extract(buf.size, line_hwc.at(i + 1));
        }
        buffers.push_back(buf);
    }

#if 0
    assert(tsc_time.func.in >= 0);
    assert(tsc_time.mpi.in >= 0);
    assert(tsc_time.mpi.out >= 0);
    assert(tsc_time.func.out >= 0);
#else // correction instead of assert
    assert(tsc_time.func.in >= 0);
    assert(tsc_time.mpi.in >= 0);
    if (tsc_time.mpi.out < tsc_time.mpi.in)
        tsc_time.mpi.out = tsc_time.mpi.in;
    if (tsc_time.func.out < tsc_time.mpi.out)
        tsc_time.func.out = tsc_time.mpi.out;
#endif
    if (func_id == MPI_PCONTROL_ID) {
        extract(PControl.ctl, line_mpi.at(4));
    }

    func_class = PEAT_PROP_CLASS_LOCAL; /* (default decision) */
    // class dependent part
    if (func_id >= MPI_START_FILE_GROUP && func_id < MPI_END_FILE_GROUP) {
        func_class = PEAT_PROP_CLASS_FIO;
    }

    if (func_id >= MPI_START_BUFFERED_LOCAL_GROUP && func_id < MPI_END_BUFFERED_LOCAL_GROUP) {
        func_class = PEAT_PROP_CLASS_BUF_LOCAL;
        extract(buf.inbuf_size, line_mpi.at(4), 16);
        extract(buf.outbuf_size, line_mpi.at(5), 16);
    }
    if (((func_id >= MPI_START_COMM_COLLECTIVE_GROUP) && (func_id < MPI_END_COMM_COLLECTIVE_GROUP)) || (func_id == MPI_COMM_FREE_ID)) {
        func_class = PEAT_PROP_CLASS_COMM;
        extract(Comms.comm, line_mpi.at(4), 16);
        extract(Comms.newcomm, line_mpi.at(5), 16);
        extract(Comms.remote_comm, line_mpi.at(6), 16);
        extract(Comms.inter_comm, line_mpi.at(7), 16);
        extract(Comms.request, line_mpi.at(8), 16);
        int num_colts;
        extract(num_colts, line_mpi.at(9)); // number of collective transfers followed by
        // t_events.resize(num_colts, NULL);
    }
    if (((func_id >= MPI_P2P_TRANSFER_START) && (func_id < MPI_P2P_TRANSFER_END)) ||
        ((func_id >= MPI_P2P_PERSISTENT_INIT_START) && (func_id < MPI_P2P_PERSISTENT_INIT_END)) ||
        ((func_id >= MPI_PROBE_START) && (func_id < MPI_PROBE_END))) {
        extract(P2P.comm, line_mpi.at(4), 16);
        extract(P2P.peer, line_mpi.at(5)); // COMMWORLD based No need to translate
        extract(P2P.request, line_mpi.at(6), 16);
        extract(P2P.message, line_mpi.at(7), 16);
        extract(P2P.tag, line_mpi.at(8));
        extract(P2P.transfered_bytes, line_mpi.at(9));
        /*
        if(global_func_id.check_property(func_id, PEAT_PROP_PERSISTENT_REQUEST)) {
            pet_core->add_persistent_request(this);
        }*/
        func_class = PEAT_PROP_CLASS_P2P;
        if ((func_id >= MPI_PROBE_START) && (func_id < MPI_PROBE_END)) {
            func_class = PEAT_PROP_CLASS_PROBE;
        }
    }
    if (func_id >= MPI_START_OSC_GROUP && func_id < MPI_END_OSC_GROUP) {
        func_class = PEAT_PROP_CLASS_OSC;
    }

    if (func_id >= MPI_REQUEST_START && func_id < MPI_REQUEST_END) {

        Requests.flag = false;

        extract(Requests.input_request_num, line_mpi.at(4));
        extract(Requests.triggered_request_num, line_mpi.at(5));
        extract(Requests.flag, line_mpi.at(6));
        extract(num_colts, line_mpi.at(7));
        extract(Requests.buf, line_mpi.at(8), 16);
        // p2p_events.resize(num_colts);
        if (func_id == MPI_STARTALL_ID || func_id == MPI_START_ID || func_id == MPI_REQUEST_FREE_ID || func_id == MPI_CANCEL_ID) {
            for (int i = 0; i < (int)Requests.input_request_num; i++) {
                PEAT_MPI_Request req;
                extract(req, line_mpi.at(9 + i), 16);
                requests.push_back(req);
            }

        } else {
            for (int i = 0; i < (int)Requests.input_request_num; i++) {
                PEAT_MPI_Request req;
                extract(req, line_mpi.at(9 + i), 16);
                requests.push_back(req);
            }

            // 4 - tuples(index, tag, source, bytes)
            for (int i = 0; i < (int)Requests.triggered_request_num; i++) {
                PEAT_MPI_R4Status status;
                extract(status.index, line_mpi.at(9 + Requests.input_request_num + i * 4 + 0));
                extract(status.tag, line_mpi.at(9 + Requests.input_request_num + i * 4 + 1));
                extract(status.src, line_mpi.at(9 + Requests.input_request_num + i * 4 + 2));
                extract(status.bytes, line_mpi.at(9 + Requests.input_request_num + i * 4 + 3));
                statuses.push_back(status);
            }
        }
        func_class = PEAT_PROP_CLASS_REQUEST;
    }
    if (func_id >= MPI_START_GENERAL_COLLECTIVE_GROUP && func_id < MPI_END_GENERAL_COLLECTIVE_GROUP) {
        // collectives      5  6  7  8  9  10 11
        extract(Collective.comm, line_mpi.at(4), 16);
        extract(Collective.root, line_mpi.at(5));
        extract(Collective.request, line_mpi.at(6), 16);
        extract(Collective.rw_fields0, line_mpi.at(7));
        extract(Collective.rw_fields1, line_mpi.at(8));
        extract(num_colts, line_mpi.at(9)); // number of collective transfers followed by
        // t_events.resize(num_colts);

        // PEAT_Message_Size size_msg_collective = -1;

        send_bytes.reserve(Collective.rw_fields0);
        recv_bytes.reserve(Collective.rw_fields1);

        for (PEAT_Signed_Num j = 0; j < Collective.rw_fields0; j++) {
            PEAT_Message_Size bytes;
            extract(bytes, line_mpi.at(10 + j));
            send_bytes.push_back(bytes);
        }
        for (PEAT_Signed_Num j = 0; j < Collective.rw_fields1; j++) {
            PEAT_Message_Size bytes;
            extract(bytes, line_mpi.at(10 + Collective.rw_fields0 + j));
            recv_bytes.push_back(bytes);
        }

        func_class = PEAT_PROP_CLASS_COLLECTIVE;
    }
    if (((func_id >= CL_START_GROUP) && (func_id < CL_END_GROUP)) || ((func_id >= ZE_START_GROUP) && (func_id < ZE_END_GROUP))) {
        func_class = PEAT_PROP_CLASS_GPU;
        size_t num_params = 0, num_out_params = 0, num_extra_params = 0;
        extract(num_params, line_mpi.at(4));
        params.resize(num_params);
        extract(num_out_params, line_mpi.at(5));
        out_params.resize(num_out_params);
        extract(num_extra_params, line_mpi.at(6));
        extra_params.resize(num_extra_params);
        for (size_t p = 0; p < num_params; p++) {
            extract(params[p], line_mpi.at(7 + p), 16);
        }
        for (size_t p = 0; p < num_out_params; p++) {
            extract(out_params[p], line_mpi.at(7 + num_params + p), 16);
        }
        for (size_t p = 0; p < num_extra_params; p++) {
            extract(extra_params[p], line_mpi.at(7 + num_params + num_out_params + p), 16);
        }
    }
    if (func_id >= OMPT_START_GROUP && func_id < OMPT_END_GROUP) {
        func_class = PEAT_PROP_CLASS_OMPT;
    }
}
