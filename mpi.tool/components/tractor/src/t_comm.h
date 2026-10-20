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

#ifndef __T_COMM_H__
#define __T_COMM_H__

#include <iostream>
#include <list>
#include <map>
#include <mutex>
#include <vector>

#include <zlib.h>

#include "common_plain_types.h"
#include "mpi.h"
#include "t_alloc.h"

#include "t_defines.h"

/* return index of value in the vector if one has it. used for rank search.*/
template <typename T>
int get_val(const std::vector<T> &vec, T val)
{
    size_t l = 0, r = vec.size() - 1;
    int res = -1;
    while (l <= r) {
        size_t m = l + (r - l) / 2;
        if (vec[m] >= val) {
            if (vec[m] == val) {
                res = (int)m;
                break;
            }
            r = m - 1;
        } else {
            l = m + 1;
        }
    }
    return res;
}

#if defined(__CPLUSPLUS__) || defined(__cplusplus)
extern "C" {
#endif
#define T_MPI_INVALID_COMM -1
#define T_MPI_INTRA_COMM 0
#define T_MPI_INTER_COMM 1

static bool compare_ranks(const std::vector<int>& comm1_ranks, const std::vector<int>& comm2_ranks) {
    if (comm1_ranks.size() != comm2_ranks.size()) {
        debug_printf("(local) sizes mismatch %d != %d.\n", comm1_ranks.size(), comm2_ranks.size());
        return false;
    }
    for (size_t size = comm1_ranks.size(), it = 0; it < size; ++it) {
        if (comm1_ranks.at(it) != comm2_ranks.at(it)) {
            debug_printf("(local) translation %d mismatch %d != %d.\n", it, comm1_ranks.at(it), comm2_ranks.at(it));
            return false;
        }
    }
    return true;
};

struct t_comm {
    t_comm() : idx(-1), gid(-1), kind(T_MPI_INVALID_COMM), comm(MPI_COMM_NULL), begin_time(-1), end_time(-1), is_internal(0)
    {
    }
    int idx;
    int gid;
    int kind;
    MPI_Comm comm;
    PEAT_Clock begin_time;
    PEAT_Clock end_time;
    int is_internal; // user-created (0) vs mpi-internal (1) comm; binding must never match across the two
    std::vector<int> l_comm2world_ranks, r_comm2world_ranks; // position is comm_rank, value is COMM_WORLD_rank //map<comm_rank, COMM_WORLD_rank>
    size_t pack_size() const;
    size_t pack(char *ptr) const;
    size_t unpack(char *ptr);

    friend bool compare_comms(const t_comm &comm1, const t_comm &comm2)
    {
        debug_printf("comparing comms %x and %x...",comm1.comm,comm2.comm);
        if (comm1.kind != comm2.kind)
            return false;
        if (comm1.is_internal != comm2.is_internal)
            return false;
        bool result = false;
        if (comm1.kind == T_MPI_INTER_COMM) {
            // same pair of groups, seen from the same side or from the opposite side; both groups must match
            result = (compare_ranks(comm1.l_comm2world_ranks, comm2.l_comm2world_ranks) && compare_ranks(comm1.r_comm2world_ranks, comm2.r_comm2world_ranks)) ||
                     (compare_ranks(comm1.l_comm2world_ranks, comm2.r_comm2world_ranks) && compare_ranks(comm1.r_comm2world_ranks, comm2.l_comm2world_ranks));
        } else {
            result = compare_ranks(comm1.l_comm2world_ranks, comm2.l_comm2world_ranks);
        }
        debug_printf("result:. (%d)\n",(int) result);
        return result;
    };
};

struct r_comm {
    PEAT_Clock begin_time;
    PEAT_Clock end_time;
    MPI_Comm comm;
    int gid;
    PEAT_Rank rank;
    int kind;
    int is_internal;
    r_comm() : gid(-1), rank(-1), comm(MPI_COMM_NULL), begin_time(-1), end_time(-1), kind(T_MPI_INVALID_COMM), is_internal(0)
    {
    }
    r_comm(const t_comm &tr_comm) : gid(-1), rank(-1), begin_time(tr_comm.begin_time), end_time(tr_comm.end_time), comm(tr_comm.comm), kind(tr_comm.kind), is_internal(tr_comm.is_internal)
    {
    }
    r_comm &operator=(const t_comm &tr_comm)
    {
        begin_time = tr_comm.begin_time;
        end_time = tr_comm.end_time;
        comm = tr_comm.comm;
        kind = tr_comm.kind;
        is_internal = tr_comm.is_internal;
        return *this;
    }
};
struct t_local_comms {
    t_local_comms() : user_index(0), internal_index(0), comm_world_group(MPI_GROUP_EMPTY), comm_world_size(0), comm_world_ranks(NULL)
    {
        __DEBUG_PROLOG_COMM_LAYER__
        __DEBUG_EPILOG_COMM_LAYER__
    }
    ~t_local_comms();
    /* idempotent: MPICH can create internal (node_comm etc.) sub-communicators of MPI_COMM_WORLD
       while still inside PMPI_Init, before tractor's post_mpi_init() gets a chance to call this */
    void init()
    {
        __DEBUG_PROLOG_COMM_LAYER__
        if (comm_world_ranks) {
            __DEBUG_EPILOG_COMM_LAYER__
            return;
        }
        PMPI_Comm_size(MPI_COMM_WORLD, &comm_world_size);
        PMPI_Comm_group(MPI_COMM_WORLD, &comm_world_group);
        comm_world_ranks = (int *)t_malloc(sizeof(int) * comm_world_size, 128);
        for (int i = 0; i < comm_world_size; ++i) {
            comm_world_ranks[i] = i;
        }
        __DEBUG_EPILOG_COMM_LAYER__
    }
    void finalize()
    {
        __DEBUG_PROLOG_COMM_LAYER__
        PMPI_Group_free(&comm_world_group);
        __DEBUG_EPILOG_COMM_LAYER__
    }
    void comm_begin(const MPI_Comm newcomm, const PEAT_Clock begin, int is_internal);
    void intra_comm_begin(const MPI_Comm newcomm, const PEAT_Clock begin, int is_internal);
    void inter_comm_begin(const MPI_Comm newcomm, const PEAT_Clock begin, int is_internal);
    void comm_end(const MPI_Comm comm, const PEAT_Clock end, int is_internal);
    bool get_local_comm_id(const MPI_Comm comm, const PEAT_Clock cur_time, int is_internal, std::list<t_comm>::iterator &result);
    bool get_local_comm_id_in_store(const MPI_Comm comm, const PEAT_Clock cur_time, int is_internal, std::list<t_comm>::iterator &result);
    bool has_local_comm_id(const MPI_Comm comm, const PEAT_Clock cur_time);
    bool find_by_idx(const int idx, int is_internal, std::list<t_comm>::const_iterator &result);
    void close_stale_internal_entries();

    void print_comms_report(gzFile file, const PEAT_Record_Type line_prefix) const;
    size_t pack_size(int is_internal);
    size_t pack_local(char *ptr, size_t total_size, int is_internal);
    size_t unpack_global(char *ptr, size_t total_size);
    // PEAT_MPI_Comm get_global_comm_id(const MPI_Comm comm, const PEAT_Clock cur_time) const;
    PEAT_MPI_Comm get_global_comm_id(const MPI_Comm comm, const PEAT_Clock tsc, int is_internal)
    {
        __DEBUG_PROLOG_COMM_LAYER__
        std::list<t_comm>::iterator result;
        if (comm==0) return 0;
        bool bFound = get_local_comm_id(comm, tsc, is_internal, result);
        if (bFound) {
            if (result->gid >= 0) {
               __DEBUG_EPILOG_COMM_LAYER__
                return result->gid;
            }
            __DEBUG_EPILOG_COMM_LAYER__
            return (PEAT_MPI_Comm)comm;
        } else {
            __DEBUG_EPILOG_COMM_LAYER__
            return (PEAT_MPI_Comm) comm;
        }
      __DEBUG_EPILOG_COMM_LAYER__
      return (PEAT_MPI_Comm) comm;
    }
    int user_index,internal_index;
    int comm_world_size;
    int *comm_world_ranks;
    MPI_Group comm_world_group; // MPI_GROUP_EMPTY
    std::map<MPI_Comm, std::list<t_comm>> comm_store;
    std::map<MPI_Comm, std::list<t_comm>> mpi_internal_comm_store;
    std::mutex newcomm_lock;
};

struct t_global_comms {
    std::vector<std::list<t_comm>> g_comm_store;
    //          |
    //          ^
    //        ranks
    std::map<int, std::list<r_comm>> g_comms;    // global comm_id -> list of "comms" corresponding to each rank in comm
    std::map<int, std::vector<r_comm>> gd_comms; // global rank -> vector of "comms"
    // std::map<int, int> g_num_comms; //global rank -> number of comms written

    void print_comms_report(gzFile file, const PEAT_Record_Type line_prefix) const;
    size_t pack_global(char *ptr, std::vector<r_comm> &r_list)
    {
        size_t num_comms = r_list.size();
        for (size_t i = 0; i < num_comms; i++) {
            memcpy(ptr + i * sizeof(r_comm), &r_list[i], sizeof(r_comm));
            debug_printf("Packing global comm=%x(gid=%d),(%ld,%ld), at rank (%d)\n",r_list[i].comm,r_list[i].gid, r_list[i].begin_time,r_list[i].end_time,r_list[i].rank);
        }
        return sizeof(r_comm) * num_comms;
    }

    size_t unpack_local(char *ptr, size_t total_size, std::list<t_comm> &t_list)
    {
        size_t ptr_it;
        t_list.clear();
        for (ptr_it = 0; ptr_it < total_size;) {
            t_comm tmp_comm;
            ptr_it += tmp_comm.unpack(ptr + ptr_it);
            debug_printf("unpacking comm=%x(%d),(%ld,%ld),%d(%d)\n",tmp_comm.comm,tmp_comm.idx,tmp_comm.begin_time,tmp_comm.end_time,total_size,ptr_it);
            t_list.push_back(tmp_comm);
        }
        return ptr_it;
    }
    bool bind_local_comms();
    /* binds one store (comm_store or mpi_internal_comm_store) only, assigning global ids from
     * start_index; returns the highest global id assigned (start_index-1 if the store was empty) */
    int bind_local_comm(int is_internal, int start_index);
    bool find_local_comm(const t_comm &comm, std::list<t_comm> &t_list, std::list<t_comm>::iterator &result) const
    {
        for (std::list<t_comm>::iterator comm_it = t_list.begin(); comm_it != t_list.end(); ++comm_it) {
            if (compare_comms(comm, *comm_it)) {
                result = comm_it;
                return true;
            }
        }
        std::cerr << "Error in communicator find_local_comm function. Can't find comm id " << std::hex << comm.comm << std::dec <<" Rank " << t_mpi_rank << ".\n";
        assert(0);
        return false;
    }
};

#if defined(__CPLUSPLUS__) || defined(__cplusplus)
}
#endif

#endif // __T_COMM_H__
