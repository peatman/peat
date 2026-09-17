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

struct t_comm {
    t_comm() : idx(-1), gid(-1), kind(T_MPI_INVALID_COMM), comm(MPI_COMM_NULL), begin_time(-1), end_time(-1)
    {
    }
    int idx;
    int gid;
    int kind;
    MPI_Comm comm;
    PEAT_Clock begin_time;
    PEAT_Clock end_time;
    std::vector<int> l_comm2world_ranks, r_comm2world_ranks; // position is comm_rank, value is COMM_WORLD_rank //map<comm_rank, COMM_WORLD_rank>
    size_t pack_size() const;
    size_t pack(char *ptr) const;
    size_t unpack(char *ptr);
    friend bool compare_ranks(const t_comm &comm1, const t_comm &comm2)
    {
        // debug_printf("comparing comms %x and %x...",comm1.comm,comm2.comm);
        if (comm1.kind != comm2.kind)
            return false;
        bool result = true;
        if (comm1.l_comm2world_ranks.size() != comm2.l_comm2world_ranks.size()) {
            debug_printf("Local sizes mismatch %d != %d.", comm1.l_comm2world_ranks.size(), comm2.l_comm2world_ranks.size());
            result = false;
        }
        if (comm1.r_comm2world_ranks.size() != comm2.r_comm2world_ranks.size()) {
            debug_printf("Remote sizes mismatch %d != %d.", comm1.r_comm2world_ranks.size(), comm2.r_comm2world_ranks.size());
            result = false;
        }
        const std::vector<int> *comm2_local, *comm2_remote;
        comm2_local = &comm2.l_comm2world_ranks;
        comm2_remote = &comm2.r_comm2world_ranks;
        if (comm1.kind == T_MPI_INTER_COMM && result == false) {
            /* try the other way */
            comm2_local = &comm2.r_comm2world_ranks;
            comm2_remote = &comm2.l_comm2world_ranks;
            result = true;
            if (comm1.l_comm2world_ranks.size() != comm2.r_comm2world_ranks.size()) {
                debug_printf("Local/Remote sizes mismatch %d != %d.\n", comm1.l_comm2world_ranks.size(), comm2.l_comm2world_ranks.size());
                return false;
            }
            if (comm1.r_comm2world_ranks.size() != comm2.l_comm2world_ranks.size()) {
                debug_printf("Remote/Local sizes mismatch %d != %d.\n", comm1.r_comm2world_ranks.size(), comm2.r_comm2world_ranks.size());
                return false;
            }
        }
        if (result == true) {
            for (size_t size = comm1.l_comm2world_ranks.size(), it = 0; it < size; ++it) {
                if (comm1.l_comm2world_ranks.at(it) != comm2_local->at(it)) {
                    debug_printf("(local) translation %d mismatch %d != %d.\n", it, comm1.l_comm2world_ranks.at(it), comm2_local->at(it));
                    return false;
                }
            }
            for (size_t size = comm1.r_comm2world_ranks.size(), it = 0; it < size; ++it) {
                if (comm1.r_comm2world_ranks.at(it) != comm2_remote->at(it)) {
                    debug_printf("(remote) translation %d mismatch %d != %d.\n", it, comm1.r_comm2world_ranks.at(it), comm2_remote->at(it));
                    return false;
                }
            }
        } else
            return false;
        // debug_printf("matched.\n");
        return true;
    }
};

struct r_comm {
    PEAT_Clock begin_time;
    PEAT_Clock end_time;
    MPI_Comm comm;
    int gid;
    PEAT_Rank rank;
    int kind;
    r_comm() : gid(-1), rank(-1), comm(MPI_COMM_NULL), begin_time(-1), end_time(-1), kind(T_MPI_INVALID_COMM)
    {
    }
    r_comm(const t_comm &tr_comm) : gid(-1), rank(-1), begin_time(tr_comm.begin_time), end_time(tr_comm.end_time), comm(tr_comm.comm), kind(tr_comm.kind)
    {
    }
    r_comm &operator=(const t_comm &tr_comm)
    {
        begin_time = tr_comm.begin_time;
        end_time = tr_comm.end_time;
        comm = tr_comm.comm;
        kind = tr_comm.kind;
        return *this;
    }
};
struct t_local_comms {
    t_local_comms() : index(3), comm_world_group(MPI_GROUP_EMPTY), comm_world_size(0), comm_world_ranks(NULL)
    {
        __DEBUG_PROLOG_COMM_LAYER__
        __DEBUG_EPILOG_COMM_LAYER__
    }
    ~t_local_comms();
    void init()
    {
        __DEBUG_PROLOG_COMM_LAYER__
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
    void comm_begin(const MPI_Comm newcomm, const PEAT_Clock begin, bool force_add = false);
    void intra_comm_begin(const MPI_Comm newcomm, const PEAT_Clock begin, bool force_add = false);
    void inter_comm_begin(const MPI_Comm newcomm, const PEAT_Clock begin, bool force_add = false);
    void comm_end(const MPI_Comm comm, const PEAT_Clock end, bool complete_any_way = false);
    bool get_local_comm_id(const MPI_Comm comm, const PEAT_Clock cur_time, std::list<t_comm>::iterator &result);
    bool find_by_idx(const int idx, std::list<t_comm>::const_iterator &result);

    void print_comms_report(gzFile file, const PEAT_Record_Type line_prefix) const;
    size_t pack_size();
    size_t pack_local(char *ptr, size_t total_size);
    size_t unpack_global(char *ptr, size_t total_size);
    // PEAT_MPI_Comm get_global_comm_id(const MPI_Comm comm, const PEAT_Clock cur_time) const;
    PEAT_MPI_Comm get_global_comm_id(const MPI_Comm comm, const PEAT_Clock tsc)
    {
        std::list<t_comm>::iterator result;
        bool bFound = get_local_comm_id(comm, tsc, result);
        if (bFound) {
            if (result->gid >= 0)
                return result->gid;
            return (PEAT_MPI_Comm)comm;
        } else {
            return 0;
        }
    }
    int index;
    int comm_world_size;
    int *comm_world_ranks;
    MPI_Group comm_world_group; // MPI_GROUP_EMPTY
    std::map<MPI_Comm, std::list<t_comm>> comm_store;
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
            // debug_printf("Packing global comm=%x(gid=%d),(%ld,%ld), at rank (%d)\n",r_list[i].comm,r_list[i].gid, r_list[i].begin_time,r_list[i].end_time,r_list[i].rank);
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
            // debug_printf("unpacking comm=%x(%d),(%ld,%ld),%d(%d)\n",tmp_comm.comm,tmp_comm.idx,tmp_comm.begin_time,tmp_comm.end_time,total_size,ptr_it);
            t_list.push_back(tmp_comm);
        }
        return ptr_it;
    }
    bool bind_local_comms();
    bool find_local_comm(const t_comm &comm, std::list<t_comm> &t_list, std::list<t_comm>::iterator &result) const
    {
        for (std::list<t_comm>::iterator comm_it = t_list.begin(); comm_it != t_list.end(); ++comm_it) {
            if (compare_ranks(comm, *comm_it)) {
                result = comm_it;
                return true;
            }
        }
        std::cerr << "Error in communicator find_local_comm function. Can't find comm id " << std::hex << comm.comm << std::dec << "\n";
        assert(0);
        return false;
    }
};

#if defined(__CPLUSPLUS__) || defined(__cplusplus)
}
#endif

#endif // __T_COMM_H__
