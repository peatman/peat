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
#include "utils.h"
#include <map>
#include <string>
#include <vector>

#define USE_LOCAL_PARSE

struct PEAT_MPI_R4Status {
    PEAT_Signed_Num index = 0;
    PEAT_MPI_Tag tag = 0;
    PEAT_Rank src = 0;
    PEAT_Message_Size transfer_bytes = 0;
    PEAT_MPI_R4Status() : index(0), src(0), tag(0), transfer_bytes(0) {}
    /*
    PEAT_MPI_R4Status& operator=(int v) {
        index = v; tag = v; src = v; transfer_bytes = v;
        return *this;
    }*/
};

struct Tractor_Event {
    Tractor_Event(const PEAT_Rank r, const String_Split &line_hwc, const PEAT_Num num_tscs, const PEAT_Num num_hwcs, const PEAT_Num num_addrs, Address_File_Storage *user_region_file);
    PEAT_Rank rank;
    PEAT_Signed_Num num_children; // directly nested records (former num_colts)
    PEAT_UID uniq_id;             // top-level id of the record (nested records share it with their host)
    int level;                    // nesting depth (0: top-level record)
    // class specific values
    User_Address_Key addr_key;
    std::vector<PEAT_HWC_Value> hwc_values;

    PEAT_Num pack_size() const
    { // Not object size. Just size for data serialization.
        return sizeof(num_children) +
               sizeof(level) +
               sizeof(rank) +
               sizeof(uniq_id) +
               pack_pod_size(hwc_values) +
               addr_key.pack_size();
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
        ssize = sizeof(rank);
        memcpy(ptr + ptr_it, &rank, ssize);
        ptr_it += ssize;
        ssize = sizeof(num_children);
        memcpy(ptr + ptr_it, &num_children, ssize);
        ptr_it += ssize;
        ssize = sizeof(level);
        memcpy(ptr + ptr_it, &level, ssize);
        ptr_it += ssize;
        ssize = sizeof(uniq_id);
        memcpy(ptr + ptr_it, &uniq_id, ssize);
        ptr_it += ssize;
        ptr_it += pack_pod(ptr + ptr_it, hwc_values);
        ptr_it += addr_key.pack(ptr + ptr_it);
        return ptr_it;
    }
    PEAT_Num unpack(const char *ptr)
    {
        PEAT_Num ssize = 0, ptr_it = 0;
        ssize = sizeof(rank);
        memcpy(&rank, ptr + ptr_it, ssize);
        ptr_it += ssize;
        ssize = sizeof(num_children);
        memcpy(&num_children, ptr + ptr_it, ssize);
        ptr_it += ssize;
        ssize = sizeof(level);
        memcpy(&level, ptr + ptr_it, ssize);
        ptr_it += ssize;
        ssize = sizeof(uniq_id);
        memcpy(&uniq_id, ptr + ptr_it, ssize);
        ptr_it += ssize;
        ptr_it += unpack_pod(ptr + ptr_it, hwc_values);
        ptr_it += addr_key.unpack(ptr + ptr_it);
        return ptr_it;
    }
};
#endif // __TEVENT_H__
