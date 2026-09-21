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

#ifndef __TEVENT_H__
#define __TEVENT_H__
#include "common_types.h"
#include "communicators.h"
#include "records.h"
#include "utils.h"
#include <map>
#include <string>
#include <vector>
#ifndef _DEBUG
#define USE_LOCAL_PARSE
#endif

struct PEAT_MPI_R4Status {
    PEAT_Signed_Num index = 0;
    PEAT_MPI_Tag tag = 0;
    PEAT_Rank src = 0;
    PEAT_Message_Size bytes = 0;
    PEAT_MPI_R4Status() : index(0), tag(0), src(0), bytes(0) {}
};
struct PEAT_MPI_Memory_Buffer {
    PEAT_Addr addr = NULL;
    PEAT_Message_Size size = 0;
};
struct PEAT_Single_Event_Clock {
    PEAT_Clock total_c, min_c, max_c;
    PEAT_LNum occ;
    PEAT_Single_Event_Clock() : total_c(0), min_c(LLONG_MAX), max_c(0), occ(0) {};

    PEAT_Single_Event_Clock(const PEAT_Single_Event_Clock &right)
    {
        total_c = right.total_c;
        min_c = right.min_c;
        max_c = right.max_c;
        occ = right.occ;
    };

    PEAT_Single_Event_Clock &operator=(const PEAT_Single_Event_Clock &right)
    {
        total_c = right.total_c;
        min_c = right.min_c;
        max_c = right.max_c;
        occ = right.occ;
        return *this;
    };

    PEAT_Single_Event_Clock &operator+=(const PEAT_Single_Event_Clock &right)
    {
        total_c += right.total_c;
        min_c = right.min_c < min_c ? right.min_c : min_c;
        max_c = right.max_c > max_c ? right.max_c : max_c;
        occ += right.occ;
        return *this;
    };
};

struct Tractor_Event : public Phase {
    PEAT_Property func_class;
    PEAT_Rank rank;
    PEAT_Signed_Num num_colts;
    std::vector<PEAT_ULNum> params, out_params, extra_params;
    std::vector<PEAT_MPI_Request> requests;
    std::vector<PEAT_MPI_R4Status> statuses;
    std::vector<PEAT_UID> addrs;
    std::vector<PEAT_MPI_Memory_Buffer> buffers;

    Tractor_Event() : func_class(PEAT_PROPERTY_NONE), rank(-1), num_colts(0) {}
    Tractor_Event(const PEAT_Rank r, const String_Split &line_mpi, const String_Split &line_hwc, const PEAT_Num num_hwcs, const PEAT_Num num_addrs, Address_File_Storage *user_region_file, const Communicators *com_store);
    void get_extra_txrx(const String_Split &line, const Communicators *com_store);
    ~Tractor_Event()
    {
        params.clear();
        out_params.clear();
        extra_params.clear();
        requests.clear();
        statuses.clear();
        addrs.clear();
        buffers.clear();
        num_colts = -1;
    }

    // class specific values
    struct {
        PEAT_Message_Size transfered_bytes = 0;
        PEAT_MPI_Tag tag = 0;
        PEAT_Rank peer = 0;
        PEAT_MPI_Comm comm = 0;
        PEAT_MPI_Request request = 0;
        PEAT_MPI_Extra flag = 0;
        PEAT_MPI_Message message = 0;
    } P2P;
    struct {
        PEAT_Signed_Num inbuf_size = -1, outbuf_size = -1;
    } buf;
    struct {
        PEAT_Signed_Num input_request_num = 0;
        PEAT_Signed_Num triggered_request_num = 0;
        PEAT_Signed_Num flag = 0;
        PEAT_Addr buf;
    } Requests;

    struct {                            // 8 + 8 + 4 + 4 + 4
        PEAT_Signed_Num rw_fields0 = 0; // read/write counts for collectives
        PEAT_Signed_Num rw_fields1 = 0; // read/write counts for collectives
        PEAT_Rank root = 0;
        PEAT_MPI_Comm comm = 0;
        PEAT_MPI_Request request = 0; /* actual request or "0" if no request */
        PEAT_Message_Size bytes = 0;
    } Collective;
    std::vector<PEAT_Message_Size> send_bytes, recv_bytes;

    struct {
        PEAT_MPI_Comm comm = 0;
        PEAT_MPI_Comm newcomm = 0;
        PEAT_MPI_Comm remote_comm = 0;
        PEAT_MPI_Comm inter_comm = 0;
        PEAT_MPI_Request request = 0;
    } Comms;
    // std::vector<Tractor_Event*> t_events;

    struct {
        PEAT_Message_Size count = 0, size = 0;
    } buf_local;
    struct {
        PEAT_MPI_Comm comm = 0;
    } Local;
    struct {
        int ctl = 0;
    } PControl;

    /*          One sided communication     */
    struct {
        PEAT_Message_Size bytes = 0;
        PEAT_Message_Size size = 0;
        PEAT_Rank peer = 0;
        PEAT_MPI_Comm comm = 0;
        PEAT_MPI_Window window = 0;
        PEAT_MPI_Request request = 0; /* actual request or "0" if no request */
    } one_sided_comm;
    /*          File IO     */
    struct {
        PEAT_Message_Size bytes = 0;
        PEAT_MPI_Offset offset = 0;
        PEAT_MPI_Comm comm = 0;
        PEAT_MPI_File fh = 0;
        PEAT_MPIO_Request request = 0; /* actual request or "0" if no request */
    } MPIFIO;

    friend std::ostream &operator<<(std::ostream &dbg, const Tractor_Event &evt)
    {
        /*dbg << std::hex
            << "comm:" << evt.comm
            << std::dec
            << " rS:" << evt.req_stat
            << " size:" << evt.bytes
            << " tag:" << evt.tag
            << " peer:" << evt.peer
            << " rw_f0:" << evt.rw_fields0
            << " rw_f1:" << evt.rw_fields1
            << " root:" << evt.root
            << " req:" << evt.requests
            << " send_bytes:" << evt.send_bytes
            << " recv_bytes:" << evt.recv_bytes
            << " phase:" << static_cast<const Phase&>(evt) << "\n";*/
        return dbg;
    }
    PEAT_Num pack_size() const
    { // Not object size. Just size for data serialization.
        return Phase::pack_size() +
               sizeof(func_class) +
               sizeof(rank) +
               sizeof(num_colts) +
               sizeof(P2P) +
               sizeof(Requests) +
               sizeof(Collective) +
               sizeof(one_sided_comm) +
               sizeof(Comms) +
               sizeof(buf_local) +
               sizeof(MPIFIO) +
               sizeof(Local) +
               sizeof(PControl) +
               pack_pod_size(requests) +
               pack_pod_size(statuses) +
               pack_pod_size(addrs)
#if defined PROCESS_WHOLE_EVENT
               + pack_pod_size(send_bytes) + pack_pod_size(recv_bytes)
#endif
            ;
    }
    // How to use
    // Tractor_Event tEvt;
    // char buf[tEvt.size()];
    // err = MPI_Isend(buf, tEvt.pack(buf), MPI_BYTE, rank_id, 0, MPI_COMM_WORLD, &send_request);
    // MPI_Probe(MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
    // err = MPI_Irecv(&buf_recv, status.count, MPI_BYTE, rank_id ? 0 : 1, MPI_ANY_TAG, MPI_COMM_WORLD, &recv_request);
    // tEvt.unpack(buf_recv);
    PEAT_Num pack(char *ptr) const
    {
        PEAT_Num ssize = 0, ptr_it = 0;
        ptr_it = Phase::pack(ptr); // base class first
        ssize = sizeof(func_class);
        memcpy(ptr + ptr_it, &func_class, ssize);
        ptr_it += ssize;
        ssize = sizeof(rank);
        memcpy(ptr + ptr_it, &rank, ssize);
        ptr_it += ssize;
        ssize = sizeof(num_colts);
        memcpy(ptr + ptr_it, &num_colts, ssize);
        ptr_it += ssize;
        ssize = sizeof(P2P);
        memcpy(ptr + ptr_it, &P2P, ssize);
        ptr_it += ssize;
        ssize = sizeof(Requests);
        memcpy(ptr + ptr_it, &Requests, ssize);
        ptr_it += ssize;
        ssize = sizeof(Collective);
        memcpy(ptr + ptr_it, &Collective, ssize);
        ptr_it += ssize;
        ssize = sizeof(one_sided_comm);
        memcpy(ptr + ptr_it, &one_sided_comm, ssize);
        ptr_it += ssize;
        ssize = sizeof(Comms);
        memcpy(ptr + ptr_it, &Comms, ssize);
        ptr_it += ssize;
        ssize = sizeof(buf_local);
        memcpy(ptr + ptr_it, &buf_local, ssize);
        ptr_it += ssize;
        ssize = sizeof(MPIFIO);
        memcpy(ptr + ptr_it, &MPIFIO, ssize);
        ptr_it += ssize;
        ssize = sizeof(Local);
        memcpy(ptr + ptr_it, &Local, ssize);
        ptr_it += ssize;
        ssize = sizeof(PControl);
        memcpy(ptr + ptr_it, &PControl, ssize);
        ptr_it += ssize;
        ptr_it += pack_pod(ptr + ptr_it, requests);
        ptr_it += pack_pod(ptr + ptr_it, statuses);
        ptr_it += pack_pod(ptr + ptr_it, addrs);
#if defined PROCESS_WHOLE_EVENT
        ptr_it += pack_pod(ptr + ptr_it, send_bytes);
        ptr_it += pack_pod(ptr + ptr_it, recv_bytes);
#endif
        return ptr_it;
    }
    PEAT_Num unpack(const char *ptr)
    {
        PEAT_Num ssize = 0, ptr_it = 0;
        ptr_it = Phase::unpack(ptr); // base class first
        ssize = sizeof(func_class);
        memcpy(&func_class, ptr + ptr_it, ssize);
        ptr_it += ssize;
        ssize = sizeof(rank);
        memcpy(&rank, ptr + ptr_it, ssize);
        ptr_it += ssize;
        ssize = sizeof(num_colts);
        memcpy(&num_colts, ptr + ptr_it, ssize);
        ptr_it += ssize;
        ssize = sizeof(P2P);
        memcpy(&P2P, ptr + ptr_it, ssize);
        ptr_it += ssize;
        ssize = sizeof(Requests);
        memcpy(&Requests, ptr + ptr_it, ssize);
        ptr_it += ssize;
        ssize = sizeof(Collective);
        memcpy(&Collective, ptr + ptr_it, ssize);
        ptr_it += ssize;
        ssize = sizeof(one_sided_comm);
        memcpy(&one_sided_comm, ptr + ptr_it, ssize);
        ptr_it += ssize;
        ssize = sizeof(Comms);
        memcpy(&Comms, ptr + ptr_it, ssize);
        ptr_it += ssize;
        ssize = sizeof(buf_local);
        memcpy(&buf_local, ptr + ptr_it, ssize);
        ptr_it += ssize;
        ssize = sizeof(MPIFIO);
        memcpy(&MPIFIO, ptr + ptr_it, ssize);
        ptr_it += ssize;
        ssize = sizeof(Local);
        memcpy(&Local, ptr + ptr_it, ssize);
        ptr_it += ssize;
        ssize = sizeof(PControl);
        memcpy(&PControl, ptr + ptr_it, ssize);
        ptr_it += ssize;
        ptr_it += unpack_pod(ptr + ptr_it, requests);
        ptr_it += unpack_pod(ptr + ptr_it, statuses);
        ptr_it += unpack_pod(ptr + ptr_it, addrs);
#if defined PROCESS_WHOLE_EVENT
        ptr_it += unpack_pod(ptr + ptr_it, send_bytes);
        ptr_it += unpack_pod(ptr + ptr_it, recv_bytes);
#endif
        return ptr_it;
    }
};
struct Tractor_Request_Event : public Phase {
    PEAT_MPI_Request request;
    PEAT_Rank rank;
    PEAT_Rank peer;
    int pc_level;
    Tractor_Request_Event() : request(0), rank(-1), peer(-1), pc_level(-1) {};
    Tractor_Request_Event(Phase &phcn, PEAT_MPI_Request req, PEAT_Rank r, PEAT_Rank p) : Phase(phcn), request(req), rank(r), peer(p), pc_level(-1) {};
    PEAT_Num pack(char *ptr) const
    {
        PEAT_Num ssize = 0, ptr_it = 0;
        ptr_it = Phase::pack(ptr); // base class first
        ssize = sizeof(request);
        memcpy(ptr + ptr_it, &request, ssize);
        ptr_it += ssize;
        ssize = sizeof(rank);
        memcpy(ptr + ptr_it, &rank, ssize);
        ptr_it += ssize;
        ssize = sizeof(peer);
        memcpy(ptr + ptr_it, &peer, ssize);
        ptr_it += ssize;
        ssize = sizeof(pc_level);
        memcpy(ptr + ptr_it, &pc_level, ssize);
        ptr_it += ssize;
        return ptr_it;
    };
    PEAT_Num unpack(char *ptr)
    {
        PEAT_Num ssize = 0, ptr_it = 0;
        ptr_it = Phase::unpack(ptr); // base class first
        ssize = sizeof(request);
        memcpy(&request, ptr + ptr_it, ssize);
        ptr_it += ssize;
        ssize = sizeof(rank);
        memcpy(&rank, ptr + ptr_it, ssize);
        ptr_it += ssize;
        ssize = sizeof(peer);
        memcpy(&peer, ptr + ptr_it, ssize);
        ptr_it += ssize;
        ssize = sizeof(pc_level);
        memcpy(&pc_level, ptr + ptr_it, ssize);
        ptr_it += ssize;
        return ptr_it;
    };
    PEAT_Num pack_size() const
    { // Not object size. Just size for data serialization.
        return Phase::pack_size() +
               sizeof(request) +
               sizeof(rank) +
               sizeof(peer) +
               sizeof(pc_level);
    };
};
#endif // __TEVENT_H__
