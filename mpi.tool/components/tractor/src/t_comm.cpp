/*
 * This file is part of PEAT (Performance Estimation & Analyzing Toolkit).
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

#include <cassert>
#include <cstring>

#include <mpi.h>
#include <stdlib.h>

#include "t_comm.h"
#include "tractor.h"

size_t t_comm::pack_size() const
{
    return sizeof(idx) + sizeof(gid) + sizeof(kind) + sizeof(comm) +
           sizeof(begin_time) + sizeof(end_time) +
           sizeof(int) * l_comm2world_ranks.size() + sizeof(size_t) + // (size of size)
           sizeof(int) * r_comm2world_ranks.size() + sizeof(size_t);
}

size_t t_comm::pack(char *ptr) const
{
    __DEBUG_PROLOG_COMM_LAYER__
    size_t ssize = 0, ptr_it = 0;
    ssize = sizeof(idx);
    memcpy(ptr + ptr_it, &idx, ssize);
    ptr_it += ssize;
    ssize = sizeof(gid);
    memcpy(ptr + ptr_it, &gid, ssize);
    ptr_it += ssize;
    ssize = sizeof(kind);
    memcpy(ptr + ptr_it, &kind, ssize);
    ptr_it += ssize;
    ssize = sizeof(comm);
    memcpy(ptr + ptr_it, &comm, ssize);
    ptr_it += ssize;
    ssize = sizeof(begin_time);
    memcpy(ptr + ptr_it, &begin_time, ssize);
    ptr_it += ssize;
    ssize = sizeof(end_time);
    memcpy(ptr + ptr_it, &end_time, ssize);
    ptr_it += ssize;

    /* local ranks */
    size_t l_c2wr = l_comm2world_ranks.size();
    // debug_printf("pack:comm %x,size %d\n",comm,l_c2wr);
    ssize = sizeof(l_c2wr);
    memcpy(ptr + ptr_it, &l_c2wr, ssize);
    ptr_it += ssize;
    ssize = sizeof(int) * (l_comm2world_ranks.size());
    memcpy(ptr + ptr_it, l_comm2world_ranks.data(), ssize);
    ptr_it += ssize;

    /* remote ranks */
    size_t r_c2wr = r_comm2world_ranks.size();
    // debug_printf("pack:comm %x,size %d\n",comm,r_c2wr);
    ssize = sizeof(r_c2wr);
    memcpy(ptr + ptr_it, &r_c2wr, ssize);
    ptr_it += ssize;
    ssize = sizeof(int) * (r_comm2world_ranks.size());
    memcpy(ptr + ptr_it, r_comm2world_ranks.data(), ssize);
    ptr_it += ssize;

    // for (size_t i=0;i<l_c2wr;i++) debug_printf(",%d",l_comm2world_ranks.at(i));
    // debug_printf("\n ");
    // for (size_t i=0;i<r_c2wr;i++) debug_printf(",%d",r_comm2world_ranks.at(i));
    // debug_printf("\n ");
    __DEBUG_EPILOG_COMM_LAYER__
    return ptr_it;
}
size_t t_comm::unpack(char *ptr)
{
    __DEBUG_PROLOG_COMM_LAYER__
    size_t ssize = 0, ptr_it = 0;
    ssize = sizeof(idx);
    memcpy(&idx, ptr + ptr_it, ssize);
    ptr_it += ssize;
    ssize = sizeof(gid);
    memcpy(&gid, ptr + ptr_it, ssize);
    ptr_it += ssize;
    ssize = sizeof(kind);
    memcpy(&kind, ptr + ptr_it, ssize);
    ptr_it += ssize;
    ssize = sizeof(comm);
    memcpy(&comm, ptr + ptr_it, ssize);
    ptr_it += ssize;
    ssize = sizeof(begin_time);
    memcpy(&begin_time, ptr + ptr_it, ssize);
    ptr_it += ssize;
    ssize = sizeof(end_time);
    memcpy(&end_time, ptr + ptr_it, ssize);
    ptr_it += ssize;

    /* local ranks */
    size_t l_c2wr;
    ssize = sizeof(l_c2wr);
    memcpy(&l_c2wr, ptr + ptr_it, ssize);
    ptr_it += ssize;
    // debug_printf("unpack:comm %x,size %d\n",comm,l_c2wr);
    l_comm2world_ranks.resize(l_c2wr);
    ssize = sizeof(int) * l_c2wr;
    memcpy(l_comm2world_ranks.data(), ptr + ptr_it, ssize);
    ptr_it += ssize;

    /* remote ranks */
    size_t r_c2wr;
    ssize = sizeof(r_c2wr);
    memcpy(&r_c2wr, ptr + ptr_it, ssize);
    ptr_it += ssize;
    // debug_printf("unpack:comm %x,size %d\n",comm,r_c2wr);
    r_comm2world_ranks.resize(r_c2wr);
    ssize = sizeof(int) * r_c2wr;
    memcpy(r_comm2world_ranks.data(), ptr + ptr_it, ssize);
    ptr_it += ssize;

    // for (size_t i=0;i<l_c2wr;i++)debug_printf(",%d",l_comm2world_ranks.at(i));
    // debug_printf("\n ");
    // for (size_t i=0;i<r_c2wr;i++)debug_printf(",%d",r_comm2world_ranks.at(i));
    // debug_printf("\n ");
    __DEBUG_EPILOG_COMM_LAYER__

    return ptr_it;
}

size_t t_local_comms::pack_size()
{
    __DEBUG_PROLOG_COMM_LAYER__
    size_t total_size = 0;
    for (std::map<MPI_Comm, std::list<t_comm>>::const_iterator comm_store_it = comm_store.begin(); comm_store_it != comm_store.end(); ++comm_store_it) {
        for (std::list<t_comm>::const_iterator t_comm_it = comm_store_it->second.begin(); t_comm_it != comm_store_it->second.end(); ++t_comm_it) {
            total_size += t_comm_it->pack_size();
        }
    }
    __DEBUG_EPILOG_COMM_LAYER__
    return total_size;
}

size_t t_local_comms::pack_local(char *ptr, size_t total_size)
{
    __DEBUG_PROLOG_COMM_LAYER__
    size_t ptr_it = 0;
    for (int i = 3; i < index; i++) {
        std::list<t_comm>::const_iterator result;
        bool bFound = find_by_idx(i, result);
        if (bFound) {
            ptr_it += result->pack(ptr + ptr_it);
            // debug_printf("Comm=%x(%d) packed, (%ld,%ld),%d(%d)\n",result->comm,result->idx,result->begin_time,result->end_time,total_size,ptr_it);
        }
    }
    if (ptr_it != total_size) {
        debug_printf("Sizes mismatch: %d!=%d.Index=%d\n", ptr_it, total_size, index);
    }
    assert(ptr_it == total_size);
    __DEBUG_EPILOG_COMM_LAYER__
    return ptr_it;
}
size_t t_local_comms::unpack_global(char *ptr, size_t total_size)
{
    __DEBUG_PROLOG_COMM_LAYER__
    size_t ssize, ptr_it = 0;
    assert(total_size % sizeof(r_comm) == 0);
    for (r_comm *r_comm_ptr = (r_comm *)ptr; (char *)r_comm_ptr < ptr + total_size; r_comm_ptr++) {
        // find suitable comm and isert gid into it;
        std::list<t_comm>::iterator result;
        // debug_printf("Unpacking global comm=%x(%d), (%ld,%ld),%d(%d) at rank %d\n",r_comm_ptr->comm,r_comm_ptr->gid,r_comm_ptr->begin_time,r_comm_ptr->end_time,total_size,(char *)r_comm_ptr-ptr,t_mpi_rank);

        bool bFound = get_local_comm_id(r_comm_ptr->comm, r_comm_ptr->begin_time + 1, result);
        if (bFound) {
            if (result->begin_time == r_comm_ptr->begin_time && result->end_time == r_comm_ptr->end_time) {
                assert(result->gid == r_comm_ptr->gid || result->gid == -1);
                result->gid = r_comm_ptr->gid;
                ptr_it += sizeof(r_comm); // sanity check
            }
        } else {
            assert(0);
        }
    }
    __DEBUG_EPILOG_COMM_LAYER__
    return ptr_it;
}

t_local_comms::~t_local_comms()
{
    __DEBUG_PROLOG_COMM_LAYER__
    t_free(comm_world_ranks, sizeof(int) * comm_world_size);
    __DEBUG_EPILOG_COMM_LAYER__
}

bool t_local_comms::get_local_comm_id(const MPI_Comm comm, const PEAT_Clock cur_time, std::list<t_comm>::iterator &result)
{
    __DEBUG_PROLOG_COMM_LAYER__
    // debug_printf("Obtaining global communicator for %llx after TSC time %lld at rank %d (commstore %p)\n", (PEAT_MPI_Long_Comm) comm, cur_time, t_mpi_rank,(void *) &comm_store);
    // debug_printf("Obtaining global communicator for %llx after TSC time %lld at rank %d (commstore size is %d)\n", (PEAT_MPI_Long_Comm) comm, cur_time, t_mpi_rank,comm_store.size());
    if (comm != 0 && comm != MPI_COMM_NULL && comm != MPI_COMM_SELF && comm != MPI_COMM_WORLD) {
        std::map<MPI_Comm, std::list<t_comm>>::const_iterator comm_list;
        try {
            comm_list = comm_store.find(comm); // don't want an exception here by .at()
        } catch (...) {
            std::cerr << "An exeption was raised while obtaining global communicator " << std::hex << comm << " after TSC time " << std::dec << cur_time << " at rank " << t_mpi_rank << std::endl;
            // debug_printf("An exeption was raised while obtaining global communicator %llx after TSC time %lld at rank %d (commstore size is %d)\n", (PEAT_MPI_Long_Comm) comm, cur_time, t_mpi_rank,comm_store.size());
        }
        if (comm_store.end() == comm_list) {
            std::cerr << "Error in communicator get_local_comm_id function. Can't find comm id " << std::hex << comm << std::dec << " at rank " << t_mpi_rank << "\n";
        } else {
            std::list<t_comm> &comm_list = comm_store.at(comm);
            for (std::list<t_comm>::iterator it_comm = comm_list.begin(); it_comm != comm_list.end(); ++it_comm) {
                // const t_comm &item = *it_comm;
                if (cur_time >= it_comm->begin_time && (-1 == it_comm->end_time || it_comm->end_time >= cur_time)) {
                    result = it_comm;
                    return true;
                }
            }
            std::cerr << "Error in communicator t_comm::get_local_comm_id function. Can't find communicator " << std::hex << comm << " after TSC time " << std::dec << cur_time << " at rank " << t_mpi_rank << "\n";
            // debug_printf("Error in communicator t_comm::get_local_comm_id function. Can't find communicator %llx after TSC time %lld at rank %d\n", (PEAT_MPI_Long_Comm) comm, cur_time, t_mpi_rank);
            // assert(!"Can't find communicator");
        }
    }
    __DEBUG_EPILOG_COMM_LAYER__
    return false;
}

bool t_local_comms::find_by_idx(const int idx, std::list<t_comm>::const_iterator &result)
{
    __DEBUG_PROLOG_COMM_LAYER__
    for (std::map<MPI_Comm, std::list<t_comm>>::const_iterator comm_list = comm_store.begin(); comm_list != comm_store.end(); ++comm_list) {
        for (std::list<t_comm>::const_iterator it_comm = comm_list->second.begin(); it_comm != comm_list->second.end(); ++it_comm) {
            if (it_comm->idx == idx) {
                result = it_comm;
                return true;
            }
        }
    }
    debug_printf("Can't find comm with index=%d\n", idx);
    __DEBUG_EPILOG_COMM_LAYER__
    return false;
}

void t_local_comms::comm_begin(const MPI_Comm newcomm, const PEAT_Clock begin, bool force_add)
{
    __DEBUG_PROLOG_COMM_LAYER__
    if (MPI_COMM_NULL != newcomm || force_add) {
        int is_inter;
        PMPI_Comm_test_inter(newcomm, &is_inter);
        if (is_inter) {
            inter_comm_begin(newcomm, begin, force_add);
        } else {
            intra_comm_begin(newcomm, begin, force_add);
        }
    }
    __DEBUG_EPILOG_COMM_LAYER__
}

void t_local_comms::intra_comm_begin(const MPI_Comm newcomm, const PEAT_Clock begin, bool force_add)
{
    __DEBUG_PROLOG_COMM_LAYER__
    if (MPI_COMM_NULL != newcomm || force_add) {
        t_comm comm_elem;
        comm_elem.comm = newcomm;
        comm_elem.begin_time = begin;
        comm_elem.idx = index;
        comm_elem.kind = T_MPI_INTRA_COMM;

        if (MPI_COMM_NULL != newcomm) {
            PEAT_Rank newcomm_size;
            MPI_Group newcomm_group;
            PMPI_Comm_size(newcomm, &newcomm_size);
            PMPI_Comm_group(newcomm, &newcomm_group);
            PEAT_Rank newcomm_ranks[comm_world_size];
            PMPI_Group_translate_ranks(newcomm_group, newcomm_size, comm_world_ranks, comm_world_group, newcomm_ranks);
            comm_elem.l_comm2world_ranks.resize(newcomm_size, -1);
            memcpy(comm_elem.l_comm2world_ranks.data(), newcomm_ranks, newcomm_size * sizeof(PEAT_Rank));
            // move these two lines inside (don't store empty comms)
            newcomm_lock.lock();
            comm_store[newcomm].push_back(comm_elem);
            newcomm_lock.unlock();
            debug_printf("New communicator (%x) arrived:size=%d,index=%d at rank %d, starting from %lld.\n", newcomm, newcomm_size, index, t_mpi_rank, begin);
            PMPI_Group_free(&newcomm_group);
            ++index;
        }
    }
    __DEBUG_EPILOG_COMM_LAYER__
}

void t_local_comms::inter_comm_begin(const MPI_Comm newcomm, const PEAT_Clock begin, bool force_add)
{
    __DEBUG_PROLOG_COMM_LAYER__
    t_comm comm_elem;
    if (MPI_COMM_NULL != newcomm || force_add) {
        comm_elem.comm = newcomm;
        comm_elem.begin_time = begin;
        comm_elem.idx = index;
        comm_elem.kind = T_MPI_INTER_COMM;
        if (MPI_COMM_NULL != newcomm) {
            PEAT_Rank newcomm_l_size;
            MPI_Group newcomm_l_group;
            PEAT_Rank newcomm_r_size;
            MPI_Group newcomm_r_group;
            PMPI_Comm_size(newcomm, &newcomm_l_size);
            PMPI_Comm_group(newcomm, &newcomm_l_group);
            PMPI_Comm_remote_size(newcomm, &newcomm_r_size);
            PMPI_Comm_remote_group(newcomm, &newcomm_r_group);
            PEAT_Rank newcomm_l_ranks[comm_world_size];
            PEAT_Rank newcomm_r_ranks[comm_world_size];
            PMPI_Group_translate_ranks(newcomm_l_group, newcomm_l_size, comm_world_ranks, comm_world_group, newcomm_l_ranks);
            PMPI_Group_translate_ranks(newcomm_r_group, newcomm_r_size, comm_world_ranks, comm_world_group, newcomm_r_ranks);
            comm_elem.l_comm2world_ranks.resize(newcomm_l_size, -1);
            comm_elem.r_comm2world_ranks.resize(newcomm_r_size, -1);
            memcpy(comm_elem.l_comm2world_ranks.data(), newcomm_l_ranks, newcomm_l_size * sizeof(PEAT_Rank));
            memcpy(comm_elem.r_comm2world_ranks.data(), newcomm_r_ranks, newcomm_r_size * sizeof(PEAT_Rank));
            // move these two lines inside (don't store empty comms)
            newcomm_lock.lock();
            comm_store[newcomm].push_back(comm_elem);
            newcomm_lock.unlock();
            debug_printf("New Inter communicator (%x) arrived:size=%d,%d ,index=%d at rank %d, starting from %lld.\n", newcomm, newcomm_l_size, newcomm_r_size, index, t_mpi_rank, begin);
            PMPI_Group_free(&newcomm_l_group);
            PMPI_Group_free(&newcomm_r_group);
            ++index;
        }
    }
    __DEBUG_EPILOG_COMM_LAYER__
}

void t_local_comms::comm_end(const MPI_Comm comm, const PEAT_Clock end, bool complete_any_way)
{
    __DEBUG_PROLOG_COMM_LAYER__
    std::list<t_comm> &comm_list = comm_store[comm];
    if (comm_list.empty()) {
        std::cerr << "Error. Can't find communicator " << std::hex << comm << " to set completion time.\n";
    } else {
        t_comm &cur_comm = comm_list.back();               // get last element because ID reusage
        if (-1 != cur_comm.end_time && !complete_any_way) { // Communicators must be new for completion because ID reusage.
            std::cerr << "communicator_end: Wrong comminucator(" << std::hex << comm << std::dec << ") found for end time\n";
        }
        cur_comm.end_time = end;
        debug_printf("Communicator (%x) ended: at rank %d, from %lld.\n", comm, t_mpi_rank, end);
    }
    __DEBUG_EPILOG_COMM_LAYER__
}

#define PRINT_PARAMETERS(width, string) int(width + strlen(string)) / 2, string, int(width - strlen(string)) / 2, ""
#define SEPARATOR(width)                   \
    gzprintf(file, "%d:", PEAT_RT_Remark); \
    for (int i = 0; i < width; ++i) {      \
        gzprintf(file, "%c", '-');         \
    }                                      \
    gzprintf(file, "\n")

void t_local_comms::print_comms_report(gzFile file, const PEAT_Record_Type line_prefix) const
{
    __DEBUG_PROLOG_COMM_LAYER__
    const int name_id_len = 4;
    const int field_len = 14;
    const int field_len2 = 24;
    const int separator_length = 2 * name_id_len + 1 + field_len + 1 + 3 * field_len2 + 3;
    SEPARATOR(separator_length);
    gzprintf(file, "%d", PEAT_RT_Remark);
    gzprintf(file, ":%*s%*s", PRINT_PARAMETERS(name_id_len, "ID"));
    gzprintf(file, ":%*s%*s", PRINT_PARAMETERS(field_len, "Communicator"));
    gzprintf(file, ":%*s%*s", PRINT_PARAMETERS(field_len2, "Begin time"));
    gzprintf(file, ":%*s%*s", PRINT_PARAMETERS(field_len2, "End time"));
    gzprintf(file, ":%*s%*s", PRINT_PARAMETERS(name_id_len, "Size (local)"));
    gzprintf(file, ":%*s%*s", PRINT_PARAMETERS(name_id_len, "Size (Remote)"));
    gzprintf(file, ":%*s%*s", PRINT_PARAMETERS(field_len2, "Ranks (MPI_COMM_WORLD)"));
    gzprintf(file, "\n");
    SEPARATOR(separator_length);
    for (std::map<MPI_Comm, std::list<t_comm>>::const_iterator it_comm_uniq = comm_store.begin(); it_comm_uniq != comm_store.end(); ++it_comm_uniq) {
        for (std::list<t_comm>::const_iterator it = it_comm_uniq->second.begin(); it != it_comm_uniq->second.end(); ++it) {
            const t_comm &com = *it;
            gzprintf(file, "%d", line_prefix);
            gzprintf(file, ":%*x", name_id_len, com.idx);
            gzprintf(file, ":%*x", field_len, com.comm);
            gzprintf(file, ":%*lld", field_len2, com.begin_time);
            gzprintf(file, ":%*lld", field_len2, com.end_time);
            gzprintf(file, ":%*ld", name_id_len, it->l_comm2world_ranks.size());
            gzprintf(file, ":%*ld", name_id_len, it->r_comm2world_ranks.size());
            char rank_delim = ':';
            for (std::vector<PEAT_Rank>::const_iterator it_rank = it->l_comm2world_ranks.begin(); it_rank != it->l_comm2world_ranks.end(); ++it_rank) {
                gzprintf(file, "%c%d", rank_delim, *it_rank);
                if (':' == rank_delim) {
                    rank_delim = ',';
                }
            }
            rank_delim = ':';
            for (std::vector<PEAT_Rank>::const_iterator it_rank = it->r_comm2world_ranks.begin(); it_rank != it->r_comm2world_ranks.end(); ++it_rank) {
                gzprintf(file, "%c%d", rank_delim, *it_rank);
                if (':' == rank_delim) {
                    rank_delim = ',';
                }
            }
            gzprintf(file, "\n");
        }
    }
    SEPARATOR(separator_length);
    __DEBUG_EPILOG_COMM_LAYER__
}

bool t_global_comms::bind_local_comms()
{
    __DEBUG_PROLOG_COMM_LAYER__
    int comm_size, comm_rank;
    PMPI_Comm_size(MPI_COMM_WORLD, &comm_size);
    PMPI_Comm_rank(MPI_COMM_WORLD, &comm_rank);
    MPI_Count local_size = (MPI_Count)gt_info(local_comms)->pack_size();
    // debug_printf("(bind_local_comms) size %d \n",local_size);
    MPI_Count sizes[comm_size];
    MPI_Aint displs[comm_size];
    char send_buf[local_size];
    PMPI_Allgather(&local_size, 1, MPI_COUNT, sizes, 1, MPI_COUNT, gt_info(TRACTOR_COMM_WORLD)); // get all sizes
    size_t total_size = sizes[0];
    int rank_it;
    for (rank_it = 1, displs[0] = 0; rank_it < comm_size; ++rank_it) {
        displs[rank_it] = (MPI_Aint)total_size;
        total_size += sizes[rank_it];
    }
    // debug_printf("(bind_local_comms) total_size %ld \n",total_size);
    char global_recv_buf[total_size];
    // debug_printf("(bind_local_comms) packing local...");
    MPI_Count pack_size = (MPI_Count)gt_info(local_comms)->pack_local(send_buf, local_size);
    // debug_printf("finished\n");
    assert(pack_size == local_size);
    // debug_printf("(bind_local_comms) gathering locals...");
#ifndef _NO_C_VR
    PMPI_Gatherv_c(send_buf, pack_size, MPI_BYTE, global_recv_buf, sizes, displs, MPI_BYTE, 0, gt_info(TRACTOR_COMM_WORLD)); // get comms
#else
    // hand written dumb gatherv
    if (comm_rank == 0) {
        MPI_Request reqs[comm_size];
        for (rank_it = 1; rank_it < comm_size; ++rank_it) {
            PMPI_Irecv(&global_recv_buf[displs[rank_it]], sizes[rank_it], MPI_BYTE, rank_it, 42, gt_info(TRACTOR_COMM_WORLD), &reqs[rank_it]);
        }
        memcpy(global_recv_buf, send_buf, sizes[0]);
        PMPI_Waitall(comm_size - 1, &reqs[1], MPI_STATUSES_IGNORE);
    } else {
        PMPI_Send(send_buf, pack_size, MPI_BYTE, 0, 42, gt_info(TRACTOR_COMM_WORLD));
    }
#endif
    // debug_printf("finished\n");
    if (comm_rank == 0) {
        // actual binding starts here
        g_comm_store.clear();
        g_comm_store.resize(comm_size);
        for (rank_it = 0; rank_it < comm_size; ++rank_it) {
            std::list<t_comm> t_list;
            unpack_local(&global_recv_buf[displs[rank_it]], (size_t)sizes[rank_it], t_list);
            g_comm_store[rank_it] = t_list;
        }
        // now g_comm_store contains all gathered comms across all ranks
        g_comms.clear();
        int g_index = 3; // 0,1 and 2 have been already taken by MPI_COMM_NULL,MPI_COMM_SELF, and MPI_COMM_WORLD respectively
        for (rank_it = 0; rank_it < comm_size; ++rank_it) {
            std::list<t_comm> &t_list = g_comm_store.at(rank_it);
            // if (t_list.empty()) continue;
            //  debug_printf("Processing rank %d, global list size %d\n",rank_it, t_list.size());
            for (std::list<t_comm>::iterator comm_it = t_list.begin(); !t_list.empty(); comm_it = t_list.begin()) {
                if (comm_it->kind == T_MPI_INTRA_COMM) {
                    std::list<r_comm> r_list;
                    for (int i = 0; i < comm_it->l_comm2world_ranks.size(); i++) {
                        // debug_printf("(intra local) iteration %d, comm %x...",i,comm_it->comm);
                        // debug_printf("(intra local) getting rank %d...",comm_it->l_comm2world_ranks.at(i));
                        if (comm_it->l_comm2world_ranks.at(i) != rank_it) {
                            std::list<t_comm> &t_tmp_list = g_comm_store.at(comm_it->l_comm2world_ranks.at(i));
                            // debug_printf("(intra local) list size %d...",t_tmp_list.size());

                            std::list<t_comm>::iterator result;
                            if (find_local_comm(*comm_it, t_tmp_list, result)) {
                                // debug_printf("(intra local) inserting comm %x from rank %d...\n",result->comm, comm_it->l_comm2world_ranks.at(i));
                                r_comm tr_comm(*result);
                                tr_comm.gid = g_index;
                                tr_comm.rank = comm_it->l_comm2world_ranks.at(i);
                                r_list.push_back(tr_comm);
                                t_tmp_list.erase(result);
                                gd_comms[comm_it->l_comm2world_ranks.at(i)].push_back(tr_comm);
                            }
                        } else {
                            // debug_printf("(intra local) inserting original comm...%x\n",comm_it->comm);
                            r_comm tr_comm(*comm_it);
                            tr_comm.gid = g_index;
                            tr_comm.rank = rank_it;
                            r_list.push_back(tr_comm);
                            gd_comms[rank_it].push_back(tr_comm);
                        }
                    }
                    // debug_printf("(intra) Finished matching comm %x at rank %d gid=%d\n",comm_it->comm,rank_it, g_index);
                    t_list.erase(comm_it);
                    g_comms[g_index++] = r_list;
                    continue;
                }
                if (comm_it->kind == T_MPI_INTER_COMM) {
                    std::list<r_comm> r_list;
                    for (int i = 0; i < comm_it->l_comm2world_ranks.size(); i++) {
                        // debug_printf("(inter local) iteration %d, comm %x...",i,comm_it->comm);
                        // debug_printf("(inter local) getting rank %d...",comm_it->l_comm2world_ranks.at(i));
                        if (comm_it->l_comm2world_ranks.at(i) != rank_it) {
                            std::list<t_comm> &t_tmp_list = g_comm_store.at(comm_it->l_comm2world_ranks.at(i));
                            // debug_printf("(inter local) list size %d...",t_tmp_list.size());

                            std::list<t_comm>::iterator result;
                            if (find_local_comm(*comm_it, t_tmp_list, result)) {
                                // debug_printf("(inter local) inserting comm %x from rank %d...\n",result->comm, comm_it->l_comm2world_ranks.at(i));
                                r_comm tr_comm(*result);
                                tr_comm.gid = g_index;
                                tr_comm.rank = comm_it->l_comm2world_ranks.at(i);
                                r_list.push_back(tr_comm);
                                t_tmp_list.erase(result);
                                gd_comms[comm_it->l_comm2world_ranks.at(i)].push_back(tr_comm);
                            }
                        } else {
                            // debug_printf("(inter local) inserting original comm...%x\n",comm_it->comm);
                            r_comm tr_comm(*comm_it);
                            tr_comm.gid = g_index;
                            tr_comm.rank = rank_it;
                            r_list.push_back(tr_comm);
                            gd_comms[rank_it].push_back(tr_comm);
                        }
                    }
                    for (size_t i = 0; i < comm_it->r_comm2world_ranks.size(); i++) {
                        // debug_printf("(inter remote) iteration %d, comm %x...",i,comm_it->comm);
                        // debug_printf("(inter remote) getting rank %d...",comm_it->r_comm2world_ranks.at(i));
                        if (comm_it->r_comm2world_ranks.at(i) != rank_it) {
                            std::list<t_comm> &t_tmp_list = g_comm_store.at(comm_it->r_comm2world_ranks.at(i));
                            // debug_printf("(local) list size %d...",t_tmp_list.size());

                            std::list<t_comm>::iterator result;
                            if (find_local_comm(*comm_it, t_tmp_list, result)) {
                                // debug_printf("(inter local) inserting comm %x from rank %d...\n",result->comm, comm_it->r_comm2world_ranks.at(i));
                                r_comm tr_comm(*result);
                                tr_comm.gid = g_index;
                                tr_comm.rank = comm_it->r_comm2world_ranks.at(i);
                                r_list.push_back(tr_comm);
                                t_tmp_list.erase(result);
                                gd_comms[comm_it->r_comm2world_ranks.at(i)].push_back(tr_comm);
                            }
                        } else {
                            // debug_printf("(inter remote) inserting original comm...%x\n",comm_it->comm);
                            r_comm tr_comm(*comm_it);
                            tr_comm.gid = g_index;
                            tr_comm.rank = rank_it;
                            r_list.push_back(tr_comm);
                            gd_comms[rank_it].push_back(tr_comm);
                        }
                    }
                    // debug_printf("(inter) Finished matching comm %x at rank %d gid=%d\n",comm_it->comm,rank_it, g_index);
                    t_list.erase(comm_it);
                    g_comms[g_index++] = r_list;
                }
            }
        }

        // preparing to distribute
        sizes[0] = gd_comms[0].size() * sizeof(r_comm);
        displs[0] = 0;
        local_size = sizes[0];
        for (rank_it = 1; rank_it < comm_size; ++rank_it) {
            sizes[rank_it] = gd_comms[rank_it].size() * sizeof(r_comm);
            displs[rank_it] = displs[rank_it - 1] + sizes[rank_it - 1];
            local_size += sizes[rank_it];
        }
    }
    PMPI_Bcast(&local_size, 1, MPI_COUNT, 0, gt_info(TRACTOR_COMM_WORLD));
    PMPI_Bcast(sizes, comm_size, MPI_COUNT, 0, gt_info(TRACTOR_COMM_WORLD));
    PMPI_Bcast(displs, comm_size, MPI_AINT, 0, gt_info(TRACTOR_COMM_WORLD));
    char global_send_buf[local_size];
    if (comm_rank == 0) {
        for (rank_it = 0, pack_size = 0; rank_it < comm_size; ++rank_it) {
            pack_size += pack_global(&global_send_buf[displs[rank_it]], gd_comms[rank_it]);
            // debug_printf("Size after packing rank %d (%d comms) is %d(%d)\n",rank_it, gd_comms[rank_it].size(), pack_size,local_size);
        }
        assert(pack_size == local_size);
        if (pack_size != local_size)
            debug_printf("Sizes mismatch %d!=%d\n", pack_size, local_size);
    }
    // distribute comms back to ranks

    char local_recv_buf[sizes[comm_rank]];
#ifndef _NO_C_VR
    PMPI_Scatterv_c(global_send_buf, sizes, displs, MPI_BYTE, local_recv_buf, sizes[comm_rank], MPI_BYTE, 0, gt_info(TRACTOR_COMM_WORLD));
#else
    // hand written dumb scatterv
    if (comm_rank == 0) {
        MPI_Request reqs[comm_size];
        for (rank_it = 1; rank_it < comm_size; ++rank_it) {
            PMPI_Isend(&global_send_buf[displs[rank_it]], sizes[rank_it], MPI_BYTE, rank_it, 42, gt_info(TRACTOR_COMM_WORLD), &reqs[rank_it]);
        }
        memcpy(local_recv_buf, global_send_buf, sizes[0]);
        PMPI_Waitall(comm_size - 1, &reqs[1], MPI_STATUSES_IGNORE);
    } else {
        PMPI_Recv(local_recv_buf, sizes[comm_rank], MPI_BYTE, 0, 42, gt_info(TRACTOR_COMM_WORLD), MPI_STATUS_IGNORE);
    }
#endif
    local_size = gt_info(local_comms)->unpack_global(local_recv_buf, sizes[comm_rank]);
    if (sizes[comm_rank] != local_size)
        debug_printf("Sizes mismatch %d!=%d\n", sizes[comm_rank], local_size);
    assert(sizes[comm_rank] == local_size);
    __DEBUG_EPILOG_COMM_LAYER__
}

void t_global_comms::print_comms_report(gzFile file, const PEAT_Record_Type line_prefix) const
{
    __DEBUG_PROLOG_COMM_LAYER__
    const int name_id_len = 8;
    const int field_len = 14;
    const int field_len2 = 24;
    const int separator_length = 2 * name_id_len + 1 + field_len + 1 + 3 * field_len2 + 3;
    SEPARATOR(separator_length);
    gzprintf(file, "%d ", PEAT_RT_Remark);
    gzprintf(file, ":%*s%*s", PRINT_PARAMETERS(name_id_len, "ID"));
    gzprintf(file, ":%*s%*s", PRINT_PARAMETERS(name_id_len, "size"));
    gzprintf(file, ":%*s%*s", PRINT_PARAMETERS(field_len2, "Ranks"));
    //    gzprintf(file, ":%*s%*s", PRINT_PARAMETERS(field_len, "Communicator"));
    gzprintf(file, "\n");
    SEPARATOR(separator_length);
    for (std::map<int, std::list<r_comm>>::const_iterator it_comm_uniq = g_comms.begin(); it_comm_uniq != g_comms.end(); ++it_comm_uniq) {
        gzprintf(file, "%d", line_prefix);
        gzprintf(file, ":%*x:%*d", name_id_len, it_comm_uniq->first, name_id_len, it_comm_uniq->second.size()); // global id,size
        for (std::list<r_comm>::const_iterator it = it_comm_uniq->second.begin(); it != it_comm_uniq->second.end(); ++it) {
            const r_comm &comm = *it;
            gzprintf(file, ":%d", comm.rank);
        }
        gzprintf(file, "\n");
    }
    SEPARATOR(separator_length);
    gzprintf(file, "%d: Informational comm (for sanity check only)", PEAT_RT_Remark);
    gzprintf(file, "\n");
    SEPARATOR(separator_length);
    gzprintf(file, "%d", PEAT_RT_Remark);
    gzprintf(file, ":%*s%*s", PRINT_PARAMETERS(name_id_len, "ID"));
    gzprintf(file, ":%*s%*s", PRINT_PARAMETERS(name_id_len, "size"));
    gzprintf(file, ":%*s%*s", PRINT_PARAMETERS(name_id_len, "rank"));
    gzprintf(file, ":%*s%*s", PRINT_PARAMETERS(field_len, "Local ID"));
    gzprintf(file, ":%*s%*s", PRINT_PARAMETERS(field_len2, "Begin time"));
    gzprintf(file, ":%*s%*s", PRINT_PARAMETERS(field_len2, "End time"));
    gzprintf(file, "\n");
    for (std::map<int, std::list<r_comm>>::const_iterator it_comm_uniq = g_comms.begin(); it_comm_uniq != g_comms.end(); ++it_comm_uniq) {
        gzprintf(file, "%d", PEAT_RT_Remark);
        gzprintf(file, ":%*x:%*d:", name_id_len, it_comm_uniq->first, name_id_len, it_comm_uniq->second.size()); // global id,size
        gzprintf(file, "\n");
        for (std::list<r_comm>::const_iterator it = it_comm_uniq->second.begin(); it != it_comm_uniq->second.end(); ++it) {
            const r_comm &comm = *it;
            gzprintf(file, "%d", PEAT_RT_Remark);
            gzprintf(file, ":%*s:%*s", name_id_len, " ", name_id_len, " "); // global id,size
            gzprintf(file, ":%*d", name_id_len, comm.rank);
            gzprintf(file, ":%*x", field_len, comm.comm);
            gzprintf(file, ":%*lld", field_len2, comm.begin_time);
            gzprintf(file, ":%*lld", field_len2, comm.end_time);
            gzprintf(file, "\n");
        }
    }
    SEPARATOR(separator_length);

    __DEBUG_EPILOG_COMM_LAYER__
}

#undef SEPARATOR
#undef PRINT_PARAMETERS
