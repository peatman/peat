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

#ifndef __T_MPIDEF_H__
#define __T_MPIDEF_H__

#include <mpi.h>

#if defined(__CPLUSPLUS__) || defined(__cplusplus)
extern "C" {
#endif

#ifdef _STATUS_LO_HI_
#define MPI_STATUS_GET_COUNT_P(x) ((long long)((((long long)(((unsigned int)x->count_hi_and_cancelled) >> 1)) << BITS_IN_INT) + (unsigned int)x->count_lo))
#define MPI_STATUS_GET_COUNT_R(x) ((long long)((((long long)(((unsigned int)x.count_hi_and_cancelled) >> 1)) << BITS_IN_INT) + (unsigned int)x.count_lo))
static int __msl_record_request(const int tid, const MPI_Request value) { return (gt_info(record_int(tid, (int)value))); }
static int __msl_record_requests(const int tid, const MPI_Request *values, const int num) { return (gt_info(record_ints(tid, (int *)values, num))); }
#else
#ifdef OPEN_MPI
#define MPI_STATUS_GET_COUNT_P(x) (((long long)(x)->_ucount))
#define MPI_STATUS_GET_COUNT_R(x) (((long long)(x)._ucount))
static int __msl_record_request(const int tid, const MPI_Request value) { return (gt_info(record_long(tid, (long long)value))); }
static int __msl_record_requests(const int tid, const MPI_Request *values, const int num) { return (gt_info(record_longs(tid, (long long *)values, num))); }
#else
#define MPI_STATUS_GET_COUNT_P(x) (((long long)(x)->count))
#define MPI_STATUS_GET_COUNT_R(x) (((long long)(x).count))
static int __msl_record_request(const int tid, const MPI_Request value) { return (gt_info(record_int(tid, (int)value))); }
static int __msl_record_requests(const int tid, const MPI_Request *values, const int num) { return (gt_info(record_ints(tid, (int *)values, num))); }
#endif
#endif

#ifdef _USE_PROTORYPES_WITH_CONST_
#define MTIconst const
#ifdef OPEN_MPI
#define MTIOMPIDconst
#else
#define MTIOMPIDconst const
#endif
#else
#define MTIconst
#define MTIOMPIDconst
#endif

#ifdef _TRACTOR_PASSTHROUGH_
#define ptractor_status status
#endif

#define get_rank_from_req(x, y) (y)
// #define get_rank_from_req_colt(x,y) mpi_internal_request_translate_rank(x,y)
#define get_rank_from_req_colt(x, y) mpi_internal_comm_translate_rank(x, y)
#define get_rank_from_req_colt_v(x, y) mpi_internal_comm_translate_rank_verbose(x, y)
// #define get_rank_from_req_colt(x,y) (y)
#define is_empty(status) (status->MPI_TAG == MPI_ANY_TAG || status->MPI_SOURCE == MPI_ANY_SOURCE)

inline int is_all_empty(int inum, MPI_Status *status)
{
    for (int i = 0; i < inum; i++) {
        if (status[i].MPI_TAG != MPI_ANY_TAG || status[i].MPI_SOURCE != MPI_ANY_SOURCE)
            return 0;
    }
    return 1;
}
#define __msl_dup_requests(out, in, num) memcpy(out, in, num * sizeof(MPI_Request))

int T_Type_size(MPI_Datatype datatype, MPI_Aint *size)
{
    debug_printf("T_Type_size called with %x %p\n", (int) datatype,size);

    if (datatype == MPI_DATATYPE_NULL)
        return 0;
    return PMPI_Type_size(datatype, (int *) size);
}

/* Number of entries in the per-peer arrays (counts, displacements, types) of a
   collective. Used by the generated collective wrappers instead of a plain
   PMPI_Comm_size, because the array length is not the communicator size for
   every collective:
   - neighbourhood collectives (MPI_Neighbor_*): send arrays have one entry per
     outgoing neighbour, receive arrays one per incoming neighbour. Cartesian:
     2*ndims each (MPI_PROC_NULL neighbours at non-periodic edges keep their
     slot), graph: MPI_Graph_neighbors_count for both, distributed graph:
     MPI_Dist_graph_neighbors_count in/out degree. Same as MPICH's
     MPIR_Topo_canon_nhb_count.
   - other collectives on an inter-communicator: the arrays describe the remote
     group, so both sizes are MPI_Comm_remote_size.
   - MPI_Reduce_scatter's recvcounts describes the local group even on an
     inter-communicator (T_COMM_SIZE_LOCAL).
   - everything else: MPI_Comm_size. */
enum t_comm_size_kind {
    T_COMM_SIZE_PEERS = 0,     /* per-rank arrays: local size, remote size on an inter-communicator */
    T_COMM_SIZE_NEIGHBORS = 1, /* neighbourhood collective: out-degree (send), in-degree (recv) */
    T_COMM_SIZE_LOCAL = 2      /* per-rank arrays that always cover the local group */
};

static inline void determine_comm_size(MPI_Comm comm, int kind, int *send_size, int *recv_size)
{
    int flag = 0;

    if (kind == T_COMM_SIZE_NEIGHBORS) {
        int topo = MPI_UNDEFINED, ndims = 0, rank = 0, weighted = 0;
        PMPI_Topo_test(comm, &topo);
        switch (topo) {
        case MPI_CART:
            PMPI_Cartdim_get(comm, &ndims);
            *send_size = *recv_size = 2 * ndims;
            return;
        case MPI_GRAPH:
            PMPI_Comm_rank(comm, &rank);
            PMPI_Graph_neighbors_count(comm, rank, recv_size);
            *send_size = *recv_size;
            return;
        case MPI_DIST_GRAPH:
            PMPI_Dist_graph_neighbors_count(comm, recv_size, send_size, &weighted);
            return;
        default: /* no topology: the MPI call itself is erroneous, record nothing */
            debug_printf("determine_comm_size: neighbourhood collective on a communicator without topology (%x)\n", (int) comm);
            *send_size = *recv_size = 0;
            return;
        }
    }
    if (kind == T_COMM_SIZE_PEERS) {
        PMPI_Comm_test_inter(comm, &flag);
        if (flag) {
            PMPI_Comm_remote_size(comm, send_size);
            *recv_size = *send_size;
            return;
        }
    }
    PMPI_Comm_size(comm, send_size);
    *recv_size = *send_size;
}

#ifdef _TRACTOR_PARSE_COLT_

struct MPID_Comm;
struct MPID_Request;
int mpi_internal_comm_translate_rank(MPID_Comm *comm, int rank);
int mpi_internal_comm_translate_rank_verbose(MPID_Comm *comm, int rank);
int mpi_internal_request_translate_rank(MPID_Request *request, int rank);
MPI_Request mpi_internal_get_request(const MPID_Request *request);
int mpi_internal_get_requests(int inum, const MPID_Request **requests_in, MPI_Request *request_out);
MPID_Comm *mpi_internal_get_comm(MPID_Request *request);
int mpi_internal_get_comms(int inum, MPID_Request **requests_in, MPID_Comm **comms_out);
MPI_Comm mpi_internal_get_mpicomm_from_mpidcomm(MPID_Comm *comm);
int mpi_internal_print_comm_tree(MPID_Comm *comm, int depth);

static MPI_Request __msl_get_drequest(const MPID_Request *value)
{
    MPI_Request mpi_req = mpi_internal_get_request(value);
    return (mpi_req);
}

static void __msl_cvt_drequests(MPI_Request *out, const MPID_Request **values, const int num)
{
    mpi_internal_get_requests(num, values, out);
}

static MPI_Request __msl_record_request_colt(const int tid, const MPI_Request value)
{
    __msl_record_int_colt(tid, value);
    return (value);
}

static int __msl_record_requests_colt(const int tid, const MPI_Request *values, const int num)
{
    return (__msl_record_ints_colt(tid, values, num));
}

static MPI_Request __msl_record_drequest_colt(const int tid, const MPID_Request *value)
{
    MPI_Request mpi_req = mpi_internal_get_request(value);
    __msl_record_int_colt(tid, mpi_req);
    return (mpi_req);
}

static int __msl_record_drequests_colt(const int tid, const MPID_Request **values, const int num)
{
    MPI_Request local_reqs[num];
    mpi_internal_get_requests(num, values, local_reqs);
    return (__msl_record_ints_colt(tid, local_reqs, num));
}

void PT_MPID_Request_release(MPID_Request *request);
int PT_MPID_Request_complete(MPID_Request *request);

void T_MPID_Request_release(MPID_Request *request)
{
    __DEBUG_PROLOG_MPI_LAYER__
#ifndef _TRACTOR_PASSTHROUGH_
    int rv, i, temp_index = 0;
    long long tsc_entry, tsc_exit, tsc_mpi_entry, tsc_mpi_exit, local_tool_overhead;
    pid_t tid_in;
    int tid, flag = 0;
    msl_extras_counters counters;
    msl_event *event;
    bool l_skip_parse = skip_parse;
    MPI_Status status;
    MPI_Request mpi_req;

    if (__builtin_expect((counting_permission[MPID_REQUEST_RELEASE_ID_THROUGH] > 0 && !l_skip_parse), 1)) {
        tsc_entry = peat_get_time_tid(&tid_in);
#ifndef USE_NANO_TIMER
        tsc_entry = (rdtsca() + tsc_entry * 3) / 4;
#endif
        tid = __msl_get_tid(tid_in);
        __msl_allocate_event(tid, 1, &event, &counters);
        first_progress_entry_colt[tid] = last_progress_exit_colt[tid] = 0;
        num_colts[tid]++;
        mpi_req = __msl_record_drequest_colt(tid, (const MPID_Request *)request);

#ifdef _HWCOUNTERS_ENABLED_
        gt_info(hwc).get_all_counters(&counters.hwcs[0]);
#endif
#ifdef _MEMPROFILE_STORE_
        gt_info(mprof).get_mem_status(&counters.mems[0]);
#endif
        in_mpi_call[tid] = 1;
        tsc_mpi_entry = peat_get_time();
        // PMPI_Request_get_status(mpi_req,&flag,&status);
    } // if(__builtin_expect((counting_permission[MPID_REQUEST_RELEASE_ID_THROUGH]>0 && !l_skip_parse),1))
#else
    int rv;

#endif
#ifdef _TRACTOR_PCALL_
    PT_MPID_Request_release(request);
#endif
#ifndef _TRACTOR_PASSTHROUGH_
    if (__builtin_expect((counting_permission[MPID_REQUEST_RELEASE_ID_THROUGH] > 0 && !l_skip_parse), 1)) {
        tsc_mpi_exit = peat_get_time();
        in_mpi_call[tid] = 0;
        stack_unwind_colt(gt_info(max_ret_level), counters.addrs, gt_info(ret_levels));
        event->func_id[0] = MPID_REQUEST_RELEASE_ID;
        event->func_id[1] = MPID_REQUEST_RELEASE_ID_THROUGH;
        event->requests.input_request_num = 1;
        // event->requests.triggered_request_num=1;
        event->requests.request_addr = (PEAT_Addr)request;

        event->requests.num_colts = 0;
        event->tscs[0] = tsc_entry;
        event->tscs[1] = tsc_mpi_entry - tsc_entry;
        event->tscs[2] = tsc_mpi_exit - tsc_entry;
#ifdef _HWCOUNTERS_ENABLED_
        gt_info(hwc).get_all_counters(&counters.hwcs[gt_info(num_hwcs)]);
#endif
#ifdef _MEMPROFILE_STORE_
        gt_info(mprof).get_mem_status(&counters.mems[1]);
#endif

        event->requests.flag = 1;

        /*
            if (event->requests.flag) {
                long long TSC[4];
                    TSC[0]=0;
                    TSC[1]=status.MPI_TAG;
                    TSC[2]=get_rank_from_req_colt(request,status.MPI_SOURCE);
                    TSC[3]=MPI_STATUS_GET_COUNT_R(status);
                __msl_record_longs_colt (tid,TSC,4);
            }
        */
        tsc_exit = peat_get_time();
        event->tscs[3] = tsc_exit - tsc_entry;
        local_tool_overhead = tsc_mpi_entry - tsc_entry + tsc_exit - tsc_mpi_exit;
        global_mpi_time += tsc_mpi_exit - tsc_mpi_entry;
        global_tool_overhead += local_tool_overhead;
#ifdef _TRACTOR_COLLECT_STAT_
        overall_durations[MPID_REQUEST_RELEASE_ID_THROUGH] += tsc_mpi_exit - tsc_mpi_entry;
        overall_entries[MPID_REQUEST_RELEASE_ID_THROUGH]++;
        overall_tool_overhead[MPID_REQUEST_RELEASE_ID_THROUGH] += local_tool_overhead;
#endif
    } // if(__builtin_expect((counting_permission[MPID_REQUEST_RELEASE_ID_THROUGH]>0 && !l_skip_parse),1))
#endif

    __DEBUG_EPILOG_MPI_LAYER__
    return;
}

#if 0

int T_MPID_Request_complete(MPID_Request *request) {
    __DEBUG_PROLOG_MPI_LAYER__
#ifndef _TRACTOR_PASSTHROUGH_
    int rv, i,temp_index=0;
    long long tsc_entry,tsc_exit,tsc_mpi_entry,tsc_mpi_exit,local_tool_overhead;
#ifdef USE_NANO_TIMER
    long long nsec_entry;
#endif
    pid_t tid_in;
    int tid,flag=0;
    msl_extras_counters counters;
    msl_event *event;
    bool l_skip_parse=skip_parse;
    MPI_Status status;
    MPI_Request mpi_req;
    
    if(__builtin_expect((counting_permission[MPID_REQUEST_COMPLETE_ID_THROUGH]>0 && !l_skip_parse),1)) {
        tsc_entry = rdtscta(&tid_in);
#ifdef USE_NANO_TIMER
        nsec_entry=peat_get_nano_sec();
#endif
        tsc_entry = (rdtsca() + tsc_entry*3)/4;
        tid=__msl_get_tid(tid_in);
        __msl_allocate_event(tid,1,&event,&counters);
        first_progress_entry_colt [tid] = last_progress_exit_colt [tid] = 0;
        num_colts[tid]++;
        mpi_req=__msl_record_drequest_colt(tid,(const MPID_Request *) request);

#ifdef _HWCOUNTERS_ENABLED_
        gt_info(hwc).get_all_counters(&counters.hwcs[0]);
#endif
#ifdef _MEMPROFILE_STORE_
        gt_info(mprof).get_mem_status(&counters.mems[0]);
#endif
        in_mpi_call[tid]=1;
        tsc_mpi_entry=rdtsca();
        // PMPI_Request_get_status(mpi_req,&flag,&status);
    }//if(__builtin_expect((counting_permission[MPID_REQUEST_COMPLETE_ID_THROUGH]>0 && !l_skip_parse),1))
#else
    int rv;

#endif
#ifdef _TRACTOR_PCALL_
    rv=PT_MPID_Request_complete(request);
#endif
#ifndef _TRACTOR_PASSTHROUGH_
    if(__builtin_expect((counting_permission[MPID_REQUEST_COMPLETE_ID_THROUGH]>0 && !l_skip_parse),1)) {
        tsc_mpi_exit = rdtsca();
        in_mpi_call[tid]=0;
        stack_unwind_colt(gt_info(max_ret_level),counters.addrs,gt_info(ret_levels));
        event->func_id[0] = MPID_REQUEST_COMPLETE_ID;
        event->func_id[1] = MPID_REQUEST_COMPLETE_ID_THROUGH;
        event->requests.input_request_num=1;
        event->requests.request_addr=(PEAT_Addr)request;
        
        
        event->requests.num_colts=0;
#ifdef USE_NANO_TIMER
        event->tscs[0]=nsec_entry;
#else
        event->tscs[0]=tsc_entry;
#endif
        event->tscs[1]=tsc_mpi_entry-tsc_entry;
        event->tscs[2]=tsc_mpi_exit-tsc_entry;
#ifdef _HWCOUNTERS_ENABLED_
        gt_info(hwc).get_all_counters(&counters.hwcs[gt_info(num_hwcs)]);
#endif
#ifdef _MEMPROFILE_STORE_
        gt_info(mprof).get_mem_status(&counters.mems[1]);
#endif

        event->requests.flag=1;
        
/*        
    if (event->requests.flag) { 
        long long TSC[4];
            TSC[0]=0;
            TSC[1]=status.MPI_TAG;
            TSC[2]=get_rank_from_req_colt(request,status.MPI_SOURCE);
            TSC[3]=MPI_STATUS_GET_COUNT_R(status);
        __msl_record_longs_colt (tid,TSC,4);
    }
*/
        tsc_exit = rdtsca();
        event->tscs[3]=tsc_exit-tsc_entry;
        local_tool_overhead=tsc_mpi_entry-tsc_entry+tsc_exit-tsc_mpi_exit;
        global_mpi_time+=tsc_mpi_exit-tsc_mpi_entry;
        global_tool_overhead += local_tool_overhead;
#ifdef _TRACTOR_COLLECT_STAT_
        overall_durations[MPID_REQUEST_COMPLETE_ID_THROUGH]+=tsc_mpi_exit-tsc_mpi_entry;
        overall_entries[MPID_REQUEST_COMPLETE_ID_THROUGH]++;
        overall_tool_overhead[MPID_REQUEST_COMPLETE_ID_THROUGH] += local_tool_overhead;
#endif
}//if(__builtin_expect((counting_permission[MPID_REQUEST_COMPLETE_ID_THROUGH]>0 && !l_skip_parse),1))
#endif
    
    __DEBUG_EPILOG_MPI_LAYER__
    return rv;
}
#endif

#ifdef _MPICH_

#if (_MPICH_==3) //Intel MPI,mvapich2, mpich3
int PMPIC_Send(const void *buf, MPI_Aint count, MPI_Datatype datatype, int dest, int tag, MPID_Comm *comm, int *errflag);
int PMPIC_Recv(void *buf, MPI_Aint count, MPI_Datatype datatype, int source, int tag, MPID_Comm *comm, MPI_Status *status, int *errflag);
int PMPIC_Ssend(const void *buf, MPI_Aint count, MPI_Datatype datatype, int dest, int tag, MPID_Comm *comm, int *errflag);
int PMPIC_Sendrecv(const void *sendbuf, MPI_Aint sendcount, MPI_Datatype sendtype, int dest, int sendtag, void *recvbuf, MPI_Aint recvcount, MPI_Datatype recvtype, int source, int recvtag, MPID_Comm *comm, MPI_Status *status, int *errflag);
int PMPIC_Sendrecv_replace(void *buf, MPI_Aint count, MPI_Datatype datatype, int dest, int sendtag, int source, int recvtag, MPID_Comm *comm, MPI_Status *status, int *errflag);
int PMPIC_Isend(const void *buf, MPI_Aint count, MPI_Datatype datatype, int dest, int tag, MPID_Comm *comm, MPID_Request **request, int *errflag);
int PMPIC_Irecv(void *buf, MPI_Aint count, MPI_Datatype datatype, int source, int tag, MPID_Comm *comm, MPID_Request **request);

int PMPIC_Probe(int source, int tag, MPI_Comm comm, MPI_Status *status); /* takes the MPI_Comm handle in every MPICH/MVAPICH, unlike the other MPIC_* helpers */
int PMPIC_Wait(MPID_Request *request_ptr);
int PMPIC_Waitall(int numreq, MPID_Request *requests[], MPI_Status statuses[], int *errflag);
int PMPIC_Waitany(int numreq, MPID_Request *requests[], int *index, MPI_Status *status, int *errflag);
#endif
#if (_MPICH_>=4)  //Intel mpich4, mvapich4, mpich5 
/* -- Begin Profiling Symbol Block for routine MPI_Allreduce */
int PMPIC_Send(const void *buf, MPI_Aint count, MPI_Datatype datatype, int dest, int tag, MPID_Comm *comm, int errflag);
int PMPIC_Recv(void *buf, MPI_Aint count, MPI_Datatype datatype, int source, int tag, MPID_Comm *comm, MPI_Status *status);
int PMPIC_Sendrecv(const void *sendbuf, MPI_Aint sendcount, MPI_Datatype sendtype, int dest, int sendtag, void *recvbuf, MPI_Aint recvcount, MPI_Datatype recvtype, int source, int recvtag, MPID_Comm *comm, MPI_Status *status, int errflag);
int PMPIC_Sendrecv_replace(void *buf, MPI_Aint count, MPI_Datatype datatype, int dest, int sendtag, int source, int recvtag, MPID_Comm *comm, MPI_Status *status, int errflag);
int PMPIC_Isend(const void *buf, MPI_Aint count, MPI_Datatype datatype, int dest, int tag, MPID_Comm *comm, MPID_Request **request, int errflag);
int PMPIC_Irecv(void *buf, MPI_Aint count, MPI_Datatype datatype, int source, int tag, MPID_Comm *comm, MPID_Request **request);

int PMPIC_Probe(int source, int tag, MPI_Comm comm, MPI_Status *status); /* takes the MPI_Comm handle in every MPICH/MVAPICH, unlike the other MPIC_* helpers */
int PMPIC_Wait(MPID_Request *request_ptr);
int PMPIC_Waitall(int numreq, MPID_Request *requests[], MPI_Status *statuses);
int PMPIC_Waitany(int numreq, MPID_Request *requests[], int *index, MPI_Status *status);
#endif
#endif
#endif

#if defined(__CPLUSPLUS__) || defined(__cplusplus)
}
#endif

#ifdef _MPICH_
#if  (_MPICH_>=5)  

#define MPIO_Request MPI_Request // mpich5 doesn't have separate MPIO_Request

int mpi_internal_type_size(MPI_Datatype datatype, MPI_Aint * size); // mpich5 does have separate internal datatypes

int T_Type_size_colt(MPI_Datatype datatype, MPI_Aint *size)
{
    debug_printf("T_Type_size_colt (5) called with %x %p\n", (int) datatype, size);

    return mpi_internal_type_size(datatype, size);
}
#else

int T_Type_size_colt(MPI_Datatype datatype, MPI_Aint *size)
{
    debug_printf("T_Type_size_colt (4) called with %x %p\n", (int) datatype, size);

    return PMPI_Type_size(datatype, (int *) size);
}

#endif
#endif

#endif //__T_MPIDEF_H__
