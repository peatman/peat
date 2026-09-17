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
struct Phase {
    Phase() : func_id(TMPI_FUNC_ID_NONE), primary_id(-1LL), secondary_id(-1LL), thread_id(-1) {}
    ~Phase()
    {
        addrs.clear();
    }
    PEAT_MPI_Func_ID func_id;
    PEAT_UID primary_id, secondary_id;
    PEAT_UID thread_id;
    // std::vector<PEAT_Addr> addrs;
    std::vector<PEAT_UID> addrs;
    PEAT_Time tsc_u_time, tsc_time;
    PEAT_Num pack_size() const
    { // Not object size. Just size for data serialization.
        return tsc_u_time.pack_size() +
               tsc_time.pack_size() +
               sizeof(func_id) +
               sizeof(primary_id) +
               sizeof(secondary_id) +
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
        ssize = sizeof(primary_id);
        memcpy(ptr + ptr_it, &primary_id, ssize);
        ptr_it += ssize;
        ssize = sizeof(secondary_id);
        memcpy(ptr + ptr_it, &secondary_id, ssize);
        ptr_it += ssize;
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
        ssize = sizeof(primary_id);
        memcpy(&primary_id, ptr + ptr_it, ssize);
        ptr_it += ssize;
        ssize = sizeof(secondary_id);
        memcpy(&secondary_id, ptr + ptr_it, ssize);
        ptr_it += ssize;
        ssize = sizeof(thread_id);
        memcpy(&thread_id, ptr + ptr_it, ssize);
        ptr_it += ssize;
        ptr_it += unpack_pod(ptr + ptr_it, addrs);
        return ptr_it;
    }
    friend std::ostream &operator<<(std::ostream &dbg, const Phase &ph);
};

struct Record {
    Record() : rank(0), size(0), tag(0), comm(0) {}
    PEAT_Rank rank;
    PEAT_Message_Size size;
    PEAT_MPI_Tag tag;
    PEAT_MPI_Comm comm;
    Phase ph[3];
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
    bool operator<(const PEAT_P2P_Record &right) const
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
    bool operator<(const PEAT_GPUT_Record &right) const
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
    bool operator<(const PEAT_GPUE_Record &right) const
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