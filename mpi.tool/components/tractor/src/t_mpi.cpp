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

/* The MPI layer's side of t_mpi_iface.h: everything the core needs of MPI - the launcher's rank, the collectives of
 * nodes_info, PTPD, the communicator tracking (t_comm.cpp) and the parameters of the MPI records (msl_mpi_event). */
#include <mpi.h>
#include <stdint.h>
#include <string.h>
#include <type_traits>
#include <zlib.h>

#include "mpi_func_ids.h"
#include "mpi_func_ids_through.h"
/* this build's function names, under a name of its own (the core has its own mpi_function_names) */
#define mpi_function_names t_mpi_function_names
#include "mpi_func_names.h"
#undef mpi_function_names
#include "t_comm.h"
#include "t_mpi_event.h"

/* ---------------- handles <-> 64 bits ---------------- */

/* an MPICH-family handle (an int) zero-extended, an Open MPI handle (a pointer) as it is */
template <typename T>
static inline typename std::enable_if<std::is_pointer<T>::value, long long>::type t_h64(T h)
{
    return (long long)(uintptr_t)h;
}
template <typename T>
static inline typename std::enable_if<!std::is_pointer<T>::value, long long>::type t_h64(T h)
{
    return (long long)(unsigned long long)(typename std::make_unsigned<T>::type)h;
}
template <typename T>
static inline typename std::enable_if<std::is_pointer<T>::value, T>::type t_from64(long long v)
{
    return (T)(uintptr_t)v;
}
template <typename T>
static inline typename std::enable_if<!std::is_pointer<T>::value, T>::type t_from64(long long v)
{
    return (T)v;
}

#if defined(__CPLUSPLUS__) || defined(__cplusplus)
extern "C" {
#endif

MPI_Comm t_mpi_comm_world = MPI_COMM_NULL;
struct t_local_comms *t_local_comms_p = NULL;
static t_global_comms *t_global_comms_p = NULL;
static MPI_Group grp_world = MPI_GROUP_NULL, grp_smp = MPI_GROUP_NULL, grp_master = MPI_GROUP_NULL;
static MPI_Comm comm_smp = MPI_COMM_NULL, comm_master = MPI_COMM_NULL;

/* ---------------- dummy requests ---------------- */

#if defined(OPEN_MPI)
extern "C" struct ompi_request_t ompi_request_empty;
#elif defined(MVAPICH2_VERSION)
extern "C" int *mv2_dummy_request __attribute__((weak)); /* an MPID_Request *: its first member is the handle */
#endif
/* 0 for a dummy request, one that must not be tracked: a single handle shared by any number of calls that completed
   at once. Works with a stock MPI - only predefined handles/objects are compared. */
static MPI_Request t_check_dummy(MPI_Request request)
{
#if defined(OPEN_MPI)
    /* a small send completed inline gets the shared, always complete ompi_request_empty; MPI_REQUEST_NULL is a
       per-process pointer */
    if (request == MPI_REQUEST_NULL || request == &ompi_request_empty) {
        return 0;
    }
#elif defined(MPICH_VERSION)
    /* MPICH family: builtin request handles (handle kind builtin 1 << 30 | object MPIR_REQUEST 0xb << 26) are the
       pre-completed "lightweight" requests MPIR_REQUEST_COMPLETE, _SEND, _RECV, ... (none in MPICH 3.2 based
       MVAPICH2) */
    if ((request & 0xfc000000) == 0x6c000000) {
        return 0;
    }
#if defined(MVAPICH2_VERSION)
    /* MVAPICH2 ch3:mrail: sends to MPI_PROC_NULL share mv2_dummy_request (other devices don't have the symbol);
       its handle is read once, so the object is never touched after MPI_Finalize frees it */
    static int mv2_dummy_handle = 0;
    if (mv2_dummy_handle == 0 && &mv2_dummy_request != NULL && mv2_dummy_request != NULL) {
        mv2_dummy_handle = *mv2_dummy_request;
    }
    if (mv2_dummy_handle != 0 && request == mv2_dummy_handle) {
        return 0;
    }
#endif
#endif
    return request;
}
static long long t_mpi_check_request(long long request)
{
    return t_h64(t_check_dummy(t_from64<MPI_Request>(request)));
}
static inline long long t_req64(MPI_Request request)
{
    return t_h64(t_check_dummy(request));
}
static long long t_mpi_comm_null(void)
{
    return t_h64(MPI_COMM_NULL);
}

/* ---------------- life cycle ---------------- */

static void t_mpi_setup(void)
{
    if (t_local_comms_p == NULL)
        t_local_comms_p = new t_local_comms;
    if (t_global_comms_p == NULL)
        t_global_comms_p = new t_global_comms;
}
static void t_mpi_post_init(void)
{
    PMPI_Comm_dup(MPI_COMM_WORLD, &t_mpi_comm_world);
    t_local_comms_p->init(); /* no-op if already done by an internal comm created during PMPI_Init */
}
static void t_mpi_bind_comms(void)
{
    t_global_comms_p->bind_local_comms();
}
static void t_mpi_release(void)
{
    PMPI_Comm_free(&t_mpi_comm_world);
    if (grp_smp != MPI_GROUP_NULL)
        PMPI_Group_free(&grp_smp);
    if (comm_smp != MPI_COMM_NULL)
        PMPI_Comm_free(&comm_smp);
    if (grp_master != MPI_GROUP_NULL)
        PMPI_Group_free(&grp_master);
    if (comm_master != MPI_COMM_NULL)
        PMPI_Comm_free(&comm_master);
    if (grp_world != MPI_GROUP_NULL)
        PMPI_Group_free(&grp_world);
    t_local_comms_p->finalize();
}
static void t_mpi_end(void)
{
    delete t_local_comms_p;
    t_local_comms_p = NULL;
    delete t_global_comms_p;
    t_global_comms_p = NULL;
}
static void t_mpi_abort(int code)
{
    PMPI_Abort(MPI_COMM_WORLD, code);
}

/* ---------------- collectives over the groups of nodes_info ---------------- */

static MPI_Comm t_group_comm(int group)
{
    switch (group) {
    case T_MPI_WORLD:
        return t_mpi_comm_world;
    case T_MPI_SMP:
        return comm_smp;
    case T_MPI_MASTER:
        return comm_master;
    }
    return MPI_COMM_NULL;
}
static MPI_Datatype t_type(int type)
{
    switch (type) {
    case T_MPI_INT:
        return MPI_INT;
    case T_MPI_LONG_LONG:
        return MPI_LONG_LONG;
    case T_MPI_DOUBLE:
        return MPI_DOUBLE;
    }
    return MPI_BYTE;
}
static int t_mpi_group_rank(int group)
{
    int rank = -1;
    MPI_Comm comm = t_group_comm(group);
    if (comm != MPI_COMM_NULL)
        PMPI_Comm_rank(comm, &rank);
    return rank;
}
static int t_mpi_group_size(int group)
{
    int size = -1;
    MPI_Comm comm = t_group_comm(group);
    if (comm != MPI_COMM_NULL)
        PMPI_Comm_size(comm, &size);
    return size;
}
static void t_mpi_barrier(int group)
{
    PMPI_Barrier(t_group_comm(group));
}
static void t_mpi_gather(int group, const void *send, int count, int type, void *recv, int root)
{
    PMPI_Gather(send, count, t_type(type), recv, count, t_type(type), root, t_group_comm(group));
}
static void t_mpi_gatherv(int group, const void *send, int count, int type, void *recv, const int *counts, const int *displs, int root)
{
    PMPI_Gatherv(send, count, t_type(type), recv, counts, displs, t_type(type), root, t_group_comm(group));
}
static void t_mpi_allgather(int group, const void *send, int count, int type, void *recv)
{
    PMPI_Allgather(send, count, t_type(type), recv, count, t_type(type), t_group_comm(group));
}
static void t_mpi_reduce(int group, const void *send, void *recv, int count, int type, int op, int root)
{
    PMPI_Reduce(send, recv, count, t_type(type), op == T_MPI_MIN ? MPI_MIN : MPI_MAX, root, t_group_comm(group));
}
static void t_mpi_allreduce(int group, const void *send, void *recv, int count, int type, int op)
{
    PMPI_Allreduce(send, recv, count, t_type(type), op == T_MPI_MIN ? MPI_MIN : MPI_MAX, t_group_comm(group));
}
static void t_mpi_create_group(int group, int count, const int *world_ranks)
{
    if (grp_world == MPI_GROUP_NULL)
        PMPI_Comm_group(t_mpi_comm_world, &grp_world);
    MPI_Group *grp = group == T_MPI_SMP ? &grp_smp : &grp_master;
    MPI_Comm *comm = group == T_MPI_SMP ? &comm_smp : &comm_master;
    PMPI_Group_incl(grp_world, count, world_ranks, grp);
    PMPI_Comm_create(t_mpi_comm_world, *grp, comm);
}
static void t_mpi_translate_ranks(int group, int count, int *world_ranks)
{
    int ranks[count > 0 ? count : 1];
    for (int i = 0; i < count; i++)
        ranks[i] = i;
    PMPI_Group_translate_ranks(group == T_MPI_SMP ? grp_smp : grp_master, count, ranks, grp_world, world_ranks);
}

/* ---------------- PTPD ---------------- */

#define TAG_START 10
#define TAG_STOP 11
static MPI_Comm ptpd_comm = MPI_COMM_NULL;

static int t_mpi_ptpd_start(const char *exec, char **argv, int nprocs, const char *hostfile)
{
    MPI_Info ptpd_info;
    int *ptpd_errcodes = (int *)t_malloc((nprocs + 2) * sizeof(int), 128);
    PMPI_Info_create(&ptpd_info);
    PMPI_Info_set(ptpd_info, "hostfile", hostfile);
    //        PMPI_Info_set(ptpd_info,"perhost","1");
    PMPI_Comm_spawn(exec, argv, nprocs, ptpd_info, 0, MPI_COMM_SELF, &ptpd_comm, ptpd_errcodes);
    t_free(ptpd_errcodes, (nprocs + 2) * sizeof(int));
    PMPI_Info_free(&ptpd_info);
    if (ptpd_comm == MPI_COMM_NULL)
        return 0;
    int start_counting = 1;
    PMPI_Send(&start_counting, 1, MPI_INT, 0, TAG_START, ptpd_comm);
    return 1;
}
static void t_mpi_ptpd_stop(void)
{
    if (ptpd_comm == MPI_COMM_NULL)
        return;
    MPI_Status Sstop;
    int stop_counting = 1, rank;
    PMPI_Comm_rank(MPI_COMM_WORLD, &rank); /* tractor's world is released already at MPI_Finalize */
    if (rank == 0) {
        PMPI_Send(&stop_counting, 1, MPI_INT, 0, TAG_STOP, ptpd_comm);         // wait for permission to count if started by MPI_Comm_spawn
        PMPI_Recv(&stop_counting, 1, MPI_INT, 0, TAG_STOP, ptpd_comm, &Sstop); // acknowledge
    }
    PMPI_Comm_free(&ptpd_comm);
}

/* ---------------- communicators and ranks of the records ---------------- */

static int t_mpi_real_rank(long long comm64, int rank_in, long long tsc, int is_internal, int resolve)
{
    __DEBUG_PROLOG_TRACTOR_LAYER__
    if (!resolve) {
        __DEBUG_EPILOG_TRACTOR_LAYER__
        return rank_in;
    }
    const MPI_Comm comm = t_from64<MPI_Comm>(comm64);
    if (rank_in == MPI_ROOT) {
        __DEBUG_EPILOG_TRACTOR_LAYER__
        return t_mpi_rank; // Assume it's correct
    }
    if (rank_in < 0) {
        __DEBUG_EPILOG_TRACTOR_LAYER__
        return rank_in;
    }
    std::list<t_comm>::iterator event_comm_it;
    if (comm == MPI_COMM_WORLD) {
        __DEBUG_EPILOG_TRACTOR_LAYER__
        return rank_in;
    }
    if (comm == MPI_COMM_SELF) {
        __DEBUG_EPILOG_TRACTOR_LAYER__
        return t_mpi_rank;
    }
    if (comm == MPI_COMM_NULL) {
        __DEBUG_EPILOG_TRACTOR_LAYER__
        return rank_in;
    }
    const bool communicator_id_found = t_local_comms_p->get_local_comm_id(comm, tsc, is_internal, event_comm_it);
    if (communicator_id_found) {
        if (event_comm_it->kind == T_MPI_INTER_COMM) {
            if (get_val(event_comm_it->l_comm2world_ranks, t_mpi_rank) >= 0) {
                if (event_comm_it->r_comm2world_ranks.size() > rank_in) {
                    __DEBUG_EPILOG_TRACTOR_LAYER__
                    return event_comm_it->r_comm2world_ranks.at(rank_in);
                } else {
                    debug_printf("Destination rank %d is out of range for rank %d in intercomm %x, remote size %zu\n", rank_in, t_mpi_rank, peat_handle_id(comm), event_comm_it->r_comm2world_ranks.size());
                    __DEBUG_EPILOG_TRACTOR_LAYER__
                    return rank_in;
                }
            }
            debug_printf("Something wrong happen: %d is not in local group in intercomm %x\n", t_mpi_rank, peat_handle_id(comm));
            if (get_val(event_comm_it->r_comm2world_ranks, t_mpi_rank) >= 0) {
                // I suppose this shouldn't happen
                if (event_comm_it->l_comm2world_ranks.size() > rank_in) {
                    __DEBUG_EPILOG_TRACTOR_LAYER__
                    return event_comm_it->l_comm2world_ranks.at(rank_in);
                } else {
                    debug_printf("Destination rank %d is out of range for rank %d in intercomm %x local size %zu\n", rank_in, t_mpi_rank, peat_handle_id(comm), event_comm_it->l_comm2world_ranks.size());
                    __DEBUG_EPILOG_TRACTOR_LAYER__
                    return rank_in;
                }
            }
        } else {
            // T_MPI_INTRA_COMM
            if (event_comm_it->l_comm2world_ranks.size() > rank_in) {
                __DEBUG_EPILOG_TRACTOR_LAYER__
                return event_comm_it->l_comm2world_ranks.at(rank_in);
            } else {
                debug_printf("Destination rank %d is out of range for rank %d in intracomm %x local size %zu\n", rank_in, t_mpi_rank, peat_handle_id(comm), event_comm_it->l_comm2world_ranks.size());
                __DEBUG_EPILOG_TRACTOR_LAYER__
                return rank_in;
            }
        }
    }
    debug_printf("No comm %x has been found in rank %d\n", peat_handle_id(comm), t_mpi_rank);
    __DEBUG_EPILOG_TRACTOR_LAYER__
    return -rank_in; // to signalize error
}

static int t_mpi_global_comm_id(long long comm64, long long tsc, int is_internal, int resolve)
{
    const MPI_Comm comm = t_from64<MPI_Comm>(comm64);
    if (!resolve)
        return (PEAT_MPI_Comm)((PEAT_MPI_Long_Comm)comm);
    if (comm == MPI_COMM_WORLD)
        return 2;
    if (comm == MPI_COMM_SELF)
        return 1;
    if (comm == MPI_COMM_NULL)
        return 0;
    return t_local_comms_p->get_global_comm_id(comm, tsc, is_internal);
}

/* ---------------- com file ---------------- */

static void t_mpi_write_comms_report(void *gz, int line_prefix)
{
    if (t_local_comms_p)
        t_local_comms_p->print_comms_report((gzFile)gz, (PEAT_Record_Type)line_prefix);
}
static void t_mpi_write_global_comms_report(void *gz, int line_prefix)
{
    if (t_global_comms_p)
        t_global_comms_p->print_comms_report((gzFile)gz, (PEAT_Record_Type)line_prefix);
}
static void t_mpi_write_predefined(void *gz, int line_prefix)
{
    gzprintf((gzFile)gz, "%d:%d:%x:%x:%x:%x:%d:%d:%d:%x:%d\n", line_prefix, MPI_SUCCESS, MPI_UNDEFINED, MPI_COMM_WORLD, MPI_COMM_SELF, MPI_COMM_NULL, MPI_PROC_NULL, MPI_ANY_TAG, MPI_ANY_SOURCE, MPI_REQUEST_NULL, MPI_ROOT);
}
static void t_mpi_write_layout(void *gz, int line_prefix, int record_size, int transport_size)
{
    gzprintf((gzFile)gz, "%d:msl_event=%d : pp=%d,req=%d,coll=%d,comm=%d,bl=%d,osc=%d,fio=%d,l=%d,pc=%d,transport=%d\n", line_prefix, record_size,
             (int)sizeof(msl_mpi_event::p2p_pr_pb), (int)sizeof(msl_mpi_event::requests), (int)sizeof(msl_mpi_event::collective),
             (int)sizeof(msl_mpi_event::comms), (int)sizeof(msl_mpi_event::buf_local), (int)sizeof(msl_mpi_event::one_sided_comm), (int)sizeof(msl_mpi_event::mpi_fio),
             (int)sizeof(msl_mpi_event::local), (int)sizeof(msl_mpi_event::pcontrol), transport_size);
}

/* ---------------- the parameters of a record ---------------- */

#define T_IN(x, a, b) ((x) >= (a) && (x) < (b))

static void t_mpi_get_view(const void *rec_in, int func_base, struct t_mpi_view *v)
{
    const msl_mpi_event &rec = *(const msl_mpi_event *)rec_in;
    if (func_base == MPI_PCONTROL_ID) {
        v->ctl = rec.pcontrol.ctl;
    }
    if (T_IN(func_base, MPI_START_FILE_GROUP, MPI_END_FILE_GROUP)) {
        v->fh = t_h64(rec.mpi_fio.fh);
        v->comm = t_h64(rec.mpi_fio.comm);
        v->request = t_req64(rec.mpi_fio.request);
        v->offset = rec.mpi_fio.offset;
        v->transfer_bytes = rec.mpi_fio.transfer_bytes;
#ifdef _TRACTOR_EXTRA_FIELDS_
        v->bufs[v->num_bufs++] = &rec.mpi_fio.buf;
#endif
    }
    if (T_IN(func_base, MPI_START_BUFFERED_LOCAL_GROUP, MPI_END_BUFFERED_LOCAL_GROUP)) {
        v->count = rec.buf_local.count;
        v->size = rec.buf_local.size;
#ifdef _TRACTOR_EXTRA_FIELDS_
        v->bufs[v->num_bufs++] = &rec.buf_local.buf[0];
        v->bufs[v->num_bufs++] = &rec.buf_local.buf[1];
#endif
    }
    if (T_IN(func_base, MPI_START_COMM_COLLECTIVE_GROUP, MPI_END_COMM_COLLECTIVE_GROUP) ||
        func_base == MPI_COMM_FREE_ID || func_base == MPID_COMM_FREE_ID || func_base == MPID_COMM_CREATE_ID) {
        v->comm = t_h64(rec.comms.comm);
        v->newcomm = t_h64(rec.comms.newcomm);
        v->remotecomm = t_h64(rec.comms.remotecomm);
        v->request = t_req64(rec.comms.request);
    }
    const bool p2p = T_IN(func_base, MPI_P2P_TRANSFER_START, MPI_P2P_TRANSFER_END) ||
                     T_IN(func_base, MPI_P2P_PERSISTENT_INIT_START, MPI_P2P_PERSISTENT_INIT_END) ||
                     T_IN(func_base, MPI_PROBE_START, MPI_PROBE_END);
    const bool p2p_coll = T_IN(func_base, MPI_P2P_COLL_TRANSFER_START, MPI_P2P_COLL_TRANSFER_END) ||
                          T_IN(func_base, MPI_P2P_COLL_PROBE_START, MPI_P2P_COLL_PROBE_END);
    if (p2p || p2p_coll) {
        v->comm = t_h64(rec.p2p_pr_pb.comm);
        v->peer = rec.p2p_pr_pb.peer;
        v->request = t_req64(rec.p2p_pr_pb.request);
        v->message = t_h64(rec.p2p_pr_pb.message);
        v->tag = rec.p2p_pr_pb.tag;
        v->transfer_bytes = rec.p2p_pr_pb.transfer_bytes;
#ifdef _TRACTOR_EXTRA_FIELDS_
        if (p2p)
            v->bufs[v->num_bufs++] = &rec.p2p_pr_pb.buf;
#endif
    }
    if (T_IN(func_base, MPI_START_OSC_GROUP, MPI_END_OSC_GROUP)) {
        v->comm = t_h64(rec.one_sided_comm.comm);
        v->peer = rec.one_sided_comm.peer;
        v->request = t_req64(rec.one_sided_comm.request);
        v->window = t_h64(rec.one_sided_comm.window);
        v->size = rec.one_sided_comm.size;
        v->transfer_bytes = rec.one_sided_comm.transfer_bytes;
#ifdef _TRACTOR_EXTRA_FIELDS_
        v->bufs[v->num_bufs++] = &rec.one_sided_comm.buf;
        v->bufs[v->num_bufs++] = &rec.one_sided_comm.rbuf;
        v->bufs[v->num_bufs++] = &rec.one_sided_comm.cbuf;
#endif
    }
    if (T_IN(func_base, MPI_REQUEST_START, MPI_REQUEST_END) ||
        T_IN(func_base, MPI_MISC_REQUEST_START, MPI_MISC_REQUEST_END) ||
        T_IN(func_base, MPI_P2P_COLL_REQUEST_START, MPI_P2P_COLL_REQUEST_END)) {
        v->input_request_num = rec.requests.input_request_num;
        v->triggered_request_num = rec.requests.triggered_request_num;
        v->flag = rec.requests.flag;
        v->request_addr = rec.requests.request_addr;
#ifdef _TRACTOR_EXTRA_FIELDS_
        if (T_IN(func_base, MPI_REQUEST_START, MPI_REQUEST_END)) {
            v->bufs[v->num_bufs++] = &rec.requests.requests;
            v->bufs[v->num_bufs++] = &rec.requests.statuses;
        }
#endif
    }
    if (T_IN(func_base, MPI_START_GENERAL_COLLECTIVE_GROUP, MPI_END_GENERAL_COLLECTIVE_GROUP)) {
        v->comm = t_h64(rec.collective.comm);
        v->root = rec.collective.root;
        v->request = t_req64(rec.collective.request);
        v->sr_bytes[0] = rec.collective.sr_bytes[0];
        v->sr_bytes[1] = rec.collective.sr_bytes[1];
#ifdef _TRACTOR_EXTRA_FIELDS_
        v->bufs[v->num_bufs++] = &rec.collective.buf[0];
        v->bufs[v->num_bufs++] = &rec.collective.buf[1];
#endif
    }
}

static int t_mpi_reusable(const void *rec_in)
{
    const msl_mpi_event *last = (const msl_mpi_event *)rec_in;
    const int func_id = last->func_id;
    if (func_id >= MPI_TEST_START_THROUGH && func_id <= MPI_TEST_END_THROUGH && last->requests.flag != 0)
        return 0; // previous test was not empty
    if (func_id >= MPI_IPROBE_ID_THROUGH && func_id <= MPI_IMPROBE_ID_THROUGH && last->p2p_pr_pb.flag != 0)
        return 0; // previous probe was not empty
    return 1;
}

/* ---------------- the layer ---------------- */

#define T_STR2(x) #x
#define T_STR(x) T_STR2(x)
#ifdef _PARSE_MPI_INTERNAL_
#define T_MPI_INTERNAL_INFO " MPI-internal"
#else
#define T_MPI_INTERNAL_INFO ""
#endif

#if defined(OPEN_MPI)
static const char *const t_rank_env[] = {"OMPI_COMM_WORLD_RANK", NULL};
/* Open MPI splits the Fortran wrappers: libmpi_mpifh (mpif.h / use mpi; also called by the f08 layer),
   libmpi_usempif08 (use mpi_f08), libmpi_usempi_ignore_tkr / libmpi_usempi (use mpi module code). */
static const char *const t_fortran_libs[] = {"libmpi_mpifh", "libmpi_usempif08", "libmpi_usempi_ignore_tkr", "libmpi_usempi", NULL};
#else
static const char *const t_rank_env[] = {"PMI_RANK", NULL};
static const char *const t_fortran_libs[] = {"libmpifort", NULL};
#endif

const struct t_mpi t_mpi_layer = {
    T_MPI_IFACE_VERSION,
    (int)sizeof(struct t_mpi),
    "MPI " T_STR(MPI_VERSION) "." T_STR(MPI_SUBVERSION) "(" _TRACTOR_LIB_SUFFIX_ ")" T_MPI_INTERNAL_INFO,
    t_rank_env,
    t_fortran_libs,
    (int)sizeof(msl_mpi_event),
    t_mpi_setup,
    t_mpi_post_init,
    t_mpi_bind_comms,
    t_mpi_release,
    t_mpi_end,
    t_mpi_abort,
    t_mpi_group_rank,
    t_mpi_group_size,
    t_mpi_barrier,
    t_mpi_gather,
    t_mpi_gatherv,
    t_mpi_allgather,
    t_mpi_reduce,
    t_mpi_allreduce,
    t_mpi_create_group,
    t_mpi_translate_ranks,
    t_mpi_ptpd_start,
    t_mpi_ptpd_stop,
    t_mpi_get_view,
    t_mpi_check_request,
    t_mpi_comm_null,
    t_mpi_global_comm_id,
    t_mpi_real_rank,
    t_mpi_write_comms_report,
    t_mpi_write_predefined,
    t_mpi_write_layout,
    t_mpi_reusable,
    t_mpi_write_global_comms_report,
    t_mpi_function_names,
    MPI_MAX_FUNCID_THROUGH,
    "ptpd" _TRACTOR_BIN_SUFFIX_,
#ifdef I_MPI_BUILD
    1,
#else
    0,
#endif
};

#if defined(__CPLUSPLUS__) || defined(__cplusplus)
}
#endif
