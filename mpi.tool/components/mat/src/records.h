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

#ifndef __RECORDS_H__
#define __RECORDS_H__
#include "common_types.h"
#include "func_id.h"
#include <list>
#include <map>
#include <string>
#include <vector>
struct Tractor_Event;
struct Phase {
    Phase() : func_id(TMPI_FUNC_ID_NONE), thread_id(-1) {}
    ~Phase()
    {
        addrs.clear();
    }
    Phase(Tractor_Event& t);
    PEAT_MPI_Func_ID func_id;
    PEAT_Event_IDs ids; // id path as written in the trace: ids[0] is the top-level id (2 entries in the legacy format)
    PEAT_Thread thread_id;
    // std::vector<PEAT_Addr> addrs;
    std::vector<PEAT_UID> addrs;
    PEAT_Time tsc_u_time, tsc_time;
    PEAT_Num pack_size() const
    { // Not object size. Just size for data serialization.
        return tsc_u_time.pack_size() +
               tsc_time.pack_size() +
               sizeof(func_id) +
               pack_pod_size(ids) +
               sizeof(thread_id) +
               pack_pod_size(addrs);
    }
    PEAT_Num pack(char *ptr) const
    {
        PEAT_Num ssize = 0, ptr_it = 0;
        ptr_it = tsc_u_time.pack(ptr);
        ptr_it += tsc_time.pack(ptr + ptr_it);
        ssize = sizeof(func_id);
        memcpy(ptr + ptr_it, &func_id, ssize);
        ptr_it += ssize;
        ptr_it += pack_pod(ptr + ptr_it, ids);
        ssize = sizeof(thread_id);
        memcpy(ptr + ptr_it, &thread_id, ssize);
        ptr_it += ssize;
        ptr_it += pack_pod(ptr + ptr_it, addrs);
        return ptr_it;
    }
    PEAT_Num unpack(const char *ptr)
    {
        PEAT_Num ssize = 0, ptr_it = 0;
        ptr_it = tsc_u_time.unpack(ptr);
        ptr_it += tsc_time.unpack(ptr + ptr_it);
        ssize = sizeof(func_id);
        memcpy(&func_id, ptr + ptr_it, ssize);
        ptr_it += ssize;
        ptr_it += unpack_pod(ptr + ptr_it, ids);
        ssize = sizeof(thread_id);
        memcpy(&thread_id, ptr + ptr_it, ssize);
        ptr_it += ssize;
        ptr_it += unpack_pod(ptr + ptr_it, addrs);
        return ptr_it;
    }
    friend std::ostream &operator<<(std::ostream &dbg, const Phase &ph);
};

struct MAT_Record {
    MAT_Record() : rank(0), pc_level(0) {}
    MAT_Record(const MAT_Record& s) : rank(s.rank), pc_level(s.pc_level) {}
    PEAT_Rank rank;
    int pc_level;
    virtual int record_type() = 0;
    virtual Phase& get_ph(int idx) = 0;
};

struct MAT_Comm_Record : public MAT_Record {

    PEAT_MPI_Comm comm, new_comm, remote_comm;
	PEAT_MPI_Request request;
    MAT_Comm_Record() : MAT_Record(),comm(0),new_comm(0),remote_comm(0),request(0) {}
    int record_type() { return 3; }
	Phase ph; // doesn't need an array, because only one phase is recorded for comm events
    virtual Phase& get_ph(int idx) { return ph; }
};

struct MAT_FIO_Record : public MAT_Record {

    PEAT_MPI_Comm comm;
    PEAT_MPI_Request request;
    PEAT_MPI_File  fh;
    MAT_FIO_Record() : MAT_Record(), comm(0), request(0), fh(0) {}
    MAT_FIO_Record(const Tractor_Event& s);
    int record_type() { return 4; }
    Phase ph; // doesn't need an array, because only one phase is recorded for comm events
    virtual Phase& get_ph(int idx) { return ph; }
};

struct MAT_P2P_Record : public MAT_Record {
    MAT_P2P_Record() : MAT_Record(), comm(0), size(0), tag(0), message(0), peer_rank(-1), request(0), peer_assigned(false), is_send(false) {}
    MAT_P2P_Record(const Tractor_Event& s);
    PEAT_MPI_Comm comm;
    PEAT_Message_Size size;
    PEAT_MPI_Tag tag;
    PEAT_MPI_Message message;
    PEAT_Rank peer_rank;
    PEAT_MPI_Request request;
    PEAT_Bool peer_assigned;
    PEAT_Bool is_send;
    Phase ph[3];
    virtual int record_type() {return 1;}
    virtual Phase& get_ph(int idx) { return ph[idx]; }
};
struct Collective_Event;
struct MAT_Collectives_Record : public MAT_Record {
    MAT_Collectives_Record() : MAT_Record(), coll_event(NULL) {}
    MAT_Collectives_Record(const MAT_Collectives_Record& s);
    MAT_Collectives_Record(const Tractor_Event& s);
    ~MAT_Collectives_Record();
    Collective_Event *coll_event;
    virtual int record_type() { return 2; };
    virtual Phase& get_ph(int idx);
};

struct MAT_Request_Record {
    MAT_Request_Record() : request(0), peer(-1), ids(2, -1), pc_level(0) {}
    PEAT_MPI_Request request;
    PEAT_Rank peer;
    PEAT_Event_IDs ids;
    int pc_level;
};
struct PEAT_P2P_Record {
    PEAT_MPI_Func_ID func_id[3];
    PEAT_LNum bytes;
    PEAT_Rank rank, peer;
    PEAT_P2P_Record() : bytes(0), rank(-1), peer(-1)
    {
        func_id[0] = TMPI_FUNC_ID_NONE;
        func_id[1] = TMPI_FUNC_ID_NONE;
        func_id[2] = TMPI_FUNC_ID_NONE;
    };
    PEAT_Bool operator<(const PEAT_P2P_Record &right) const
    {
        if (rank < right.rank)
            return true;
        if (rank > right.rank)
            return false;
        if (peer < right.peer)
            return true;
        if (peer > right.peer)
            return false;
        if (bytes < right.bytes)
            return true;
        if (bytes > right.bytes)
            return false;
        if (func_id[0] < right.func_id[0])
            return true;
        if (func_id[0] > right.func_id[0])
            return false;
        if (func_id[1] < right.func_id[1])
            return true;
        if (func_id[1] > right.func_id[1])
            return false;
        if (func_id[2] < right.func_id[2])
            return true;
        if (func_id[2] > right.func_id[2])
            return false;
        return false;
    }
};
struct PEAT_GPUT_Record {
    PEAT_MPI_Func_ID func_id[2];
    PEAT_LNum bytes;
    PEAT_Rank rank;
    PEAT_GPUT_Record() : bytes(0), rank(-1)
    {
        func_id[0] = TMPI_FUNC_ID_NONE;
        func_id[1] = TMPI_FUNC_ID_NONE;
    };
    PEAT_Bool operator<(const PEAT_GPUT_Record &right) const
    {
        if (rank < right.rank)
            return true;
        if (rank > right.rank)
            return false;
        if (bytes < right.bytes)
            return true;
        if (bytes > right.bytes)
            return false;
        if (func_id[0] < right.func_id[0])
            return true;
        if (func_id[0] > right.func_id[0])
            return false;
        if (func_id[1] < right.func_id[1])
            return true;
        if (func_id[1] > right.func_id[1])
            return false;
        return false;
    }
};

struct PEAT_GPUE_Record {
    PEAT_MPI_Func_ID func_id[2];
    PEAT_Rank rank;
    PEAT_Handle handle;
    PEAT_GPUE_Record() : rank(-1), handle(0)
    {
        func_id[0] = TMPI_FUNC_ID_NONE;
        func_id[1] = TMPI_FUNC_ID_NONE;
    };
    PEAT_Bool operator<(const PEAT_GPUE_Record &right) const
    {
        if (rank < right.rank)
            return true;
        if (rank > right.rank)
            return false;
        if (handle < right.handle)
            return true;
        if (handle > right.handle)
            return false;
        if (func_id[0] < right.func_id[0])
            return true;
        if (func_id[0] > right.func_id[0])
            return false;
        if (func_id[1] < right.func_id[1])
            return true;
        if (func_id[1] > right.func_id[1])
            return false;
        return false;
    }
};

#endif //__RECORDS_H__