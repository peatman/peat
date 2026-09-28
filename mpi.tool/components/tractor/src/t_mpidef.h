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
static int __msl_record_request(const int tid, const int level, const MPI_Request value) { return (gt_info(record_int(tid, level, (int)value))); }
static int __msl_record_requests(const int tid, const int level, const MPI_Request *values, const int num) { return (gt_info(record_ints(tid, level, (int *)values, num))); }
#else
#ifdef OPEN_MPI
#define MPI_STATUS_GET_COUNT_P(x) (((long long)(x)->_ucount))
#define MPI_STATUS_GET_COUNT_R(x) (((long long)(x)._ucount))
static int __msl_record_request(const int tid, const int level, const MPI_Request value) { return (gt_info(record_long(tid, level, (long long)value))); }
static int __msl_record_requests(const int tid, const int level, const MPI_Request *values, const int num) { return (gt_info(record_longs(tid, level, (long long *)values, num))); }
#else
#define MPI_STATUS_GET_COUNT_P(x) (((long long)(x)->count))
#define MPI_STATUS_GET_COUNT_R(x) (((long long)(x).count))
static int __msl_record_request(const int tid, const int level, const MPI_Request value) { return (gt_info(record_int(tid, level, (int)value))); }
static int __msl_record_requests(const int tid, const int level, const MPI_Request *values, const int num) { return (gt_info(record_ints(tid, level, (int *)values, num))); }
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

#ifdef _TRACTOR_PARSE_COLT_

struct MPID_Comm;
struct MPID_Request;
/* stub for openmpi */
#ifdef _OPENMPI_ 
int mpi_internal_get_comms(int inum, MPI_Request *requests_in, MPI_Comm *comms_out);
MPI_Comm mpi_internal_get_comm(MPI_Request request);
MPI_Request mpi_internal_get_request(MPI_Request request) {return request;};
MPI_Request __msl_get_drequest(MPI_Request request) {return request;};
// int mpi_internal_get_requests(int inum, const MPI_Request *requests_in, MPI_Request *requests_out) {for (int i=0;i<inum;i++) requests_out[i]=requests_in[i];};
MPI_Comm mpi_internal_get_mpicomm_from_mpidcomm(MPI_Comm comm) {return comm;};
int mpi_internal_comm_translate_rank(MPI_Comm comm, int rank); 
int mpi_internal_comm_translate_rank_verbose(MPI_Comm comm, int rank);
int mpi_internal_request_translate_rank(MPI_Request request, int rank);

static MPI_Request __msl_record_drequest(const int tid, const int level, MPI_Request request)
{
    __msl_record_long(tid, level, (const long long)request);
    return (request);
}

static int __msl_record_drequests(const int tid, const int level, MPI_Request *requests, const int num)
{
    return (__msl_record_longs(tid, level, (const long long *)requests, num));
}

#else // for mpich based mpi there is internal function incorporated to source tree via patch provided in PEAT
int mpi_internal_get_comms(int inum, MPID_Request **requests_in, MPID_Comm **comms_out);
MPID_Comm *mpi_internal_get_comm(MPID_Request *request);
int mpi_internal_get_requests(int inum, const MPID_Request **requests_in, MPI_Request *requests_out);
MPI_Request mpi_internal_get_request(const MPID_Request *request);
MPI_Comm mpi_internal_get_mpicomm_from_mpidcomm(MPID_Comm *comm);
int mpi_internal_comm_translate_rank(MPID_Comm *comm, int rank);
int mpi_internal_comm_translate_rank_verbose(MPID_Comm *comm, int rank);
int mpi_internal_request_translate_rank(MPID_Request *request, int rank);
static MPI_Request __msl_get_drequest(const MPID_Request *value)
{
    MPI_Request mpi_req = mpi_internal_get_request(value);
    return (mpi_req);
}

static MPI_Request __msl_record_drequest(const int tid, const int level, const MPID_Request *value)
{
    MPI_Request mpi_req = mpi_internal_get_request(value);
    __msl_record_int(tid, level, mpi_req);
    return (mpi_req);
}

static int __msl_record_drequests(const int tid, const int level, const MPID_Request **values, const int num)
{
    MPI_Request local_reqs[num];
    mpi_internal_get_requests(num, values, local_reqs);
    return (__msl_record_ints(tid, level, local_reqs, num));
}

static void __msl_cvt_drequests(MPI_Request *out, const MPID_Request **values, const int num)
{
    mpi_internal_get_requests(num, values, out);
}

#endif
int mpi_internal_print_comm_tree(MPID_Comm *comm, int depth);


#ifndef _OPENMPI_ // MPICH (based) specifics

void PT_MPID_Request_release(MPID_Request *request);
int PT_MPID_Request_complete(MPID_Request *request);

void T_MPID_Request_release(MPID_Request *request)
{
    __DEBUG_PROLOG_MPI_LAYER__
#ifndef _TRACTOR_PASSTHROUGH_
    int rv, i, temp_index = 0;
    int level = -1;
    long long children_base = 0;
    long long tsc_entry, tsc_exit, tsc_mpi_entry, tsc_mpi_exit, local_tool_overhead;
    pid_t tid_in;
    int tid, flag = 0;
    msl_extras_counters counters;
    msl_event *event;
    bool l_skip_parse = mpi_skip_parse;
    MPI_Status status;
    MPI_Request mpi_req;

    if (__builtin_expect((counting_permission[MPID_REQUEST_RELEASE_ID_THROUGH] > 0 && !l_skip_parse), 1)) {
        tsc_entry = peat_get_time_tid(&tid_in);
#ifndef USE_NANO_TIMER
        tsc_entry = (rdtsca() + tsc_entry * 3) / 4;
#endif
        tid = __msl_get_tid(tid_in);
        level = __msl_enter_call(tid);
        children_base = __msl_child_base(tid, level);
        __msl_allocate_event(tid, level, 1, &event, &counters);
        mpi_req = __msl_record_drequest(tid, level, (const MPID_Request *)request);

#ifdef _HWCOUNTERS_ENABLED_
        gt_info(hwc).get_all_counters(&counters.hwcs[0]);
#endif
#ifdef _MEMPROFILE_STORE_
        gt_info(mprof).get_mem_status(&counters.mems[0]);
#endif
        tsc_mpi_entry = peat_get_time();
    } // if(__builtin_expect((counting_permission[MPID_REQUEST_RELEASE_ID_THROUGH]>0 && !l_skip_parse),1))
#else
    int rv;

#endif
#ifdef _TRACTOR_PCALL_
    PT_MPID_Request_release(request);
#endif
#ifndef _TRACTOR_PASSTHROUGH_
    if (level >= 0)
        __msl_leave_call(tid);
    if (__builtin_expect((counting_permission[MPID_REQUEST_RELEASE_ID_THROUGH] > 0 && !l_skip_parse), 1)) {
        tsc_mpi_exit = peat_get_time();
        stack_unwind_colt(gt_info(max_ret_level), counters.addrs, gt_info(ret_levels));
        event->func_id = MPID_REQUEST_RELEASE_ID_THROUGH;
        event->num_children += (short)(__msl_child_base(tid, level) - children_base);
        event->requests.input_request_num = 1;
        // event->requests.triggered_request_num=1;
        event->requests.request_addr = (PEAT_Addr)request;

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

        tsc_exit = peat_get_time();
        event->tscs[3] = tsc_exit - tsc_entry;
        __msl_check_chain(tid, level);
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
#endif

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
    bool l_skip_parse=mpi_skip_parse;
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

int PMPIC_Probe(int source, int tag, MPID_Comm *comm, MPI_Status *status);
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

int PMPIC_Probe(int source, int tag, MPID_Comm *comm, MPI_Status *status);
int PMPIC_Wait(MPID_Request *request_ptr);
int PMPIC_Waitall(int numreq, MPID_Request *requests[], MPI_Status *statuses);
int PMPIC_Waitany(int numreq, MPID_Request *requests[], int *index, MPI_Status *status);
#endif
#endif
#endif


#ifdef _OPENMPI_
#if (_OPENMPI_==5)  
int Pompi_coll_p2p_send(const void *buf, size_t count, MPI_Datatype datatype, int dst, int tag, int mode, MPI_Comm comm);
int Pompi_coll_p2p_recv(void *buf, size_t count, MPI_Datatype datatype, int src, int tag, MPI_Comm comm, MPI_Status *status);
int Pompi_coll_p2p_isend(const void *buf, size_t count, MPI_Datatype datatype, int dst, int tag, int mode, MPI_Comm comm, MPI_Request *request);
int Pompi_coll_p2p_irecv(void *buf, size_t count, MPI_Datatype datatype, int src, int tag, MPI_Comm comm, MPI_Request *request);
int Pompi_coll_p2p_send_init(const void *buf, size_t count, MPI_Datatype datatype, int dst, int tag, int mode, MPI_Comm comm,MPI_Request *request);
int Pompi_coll_p2p_recv_init(void *buf, size_t count, MPI_Datatype datatype, int src, int tag, MPI_Comm comm, MPI_Request *request);
int Pompi_coll_p2p_start(size_t count, MPI_Request *requests);
int Pompi_coll_p2p_wait(MPI_Request *request, MPI_Status *status);
int Pompi_coll_p2p_wait_all(size_t inum, MPI_Request *requests, MPI_Status *statuses);
int Pompi_coll_p2p_wait_any(size_t inum, MPI_Request *requests, int *index, MPI_Status *status);
int Pompi_coll_p2p_wait_some(size_t inum, MPI_Request *requests, int *outcount, int *indices, MPI_Status *statuses);
int Pompi_coll_p2p_test(MPI_Request *request, int *flag, MPI_Status *status);
int Pompi_coll_p2p_test_any(size_t inum, MPI_Request *requests, int *index, int *completed, MPI_Status *status);
int Pompi_coll_p2p_test_all(size_t inum, MPI_Request *requests, int *flag, MPI_Status *statuses);
int Pompi_coll_p2p_test_some(size_t inum, MPI_Request *requests, int *outcount, int *indices, MPI_Status *statuses);
int Pompi_coll_p2p_wait_completion(MPI_Request request);
int Pompi_coll_p2p_cancel(MPI_Request request);
int Pompi_coll_p2p_free(MPI_Request *request);
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
#else //mpich based but not  mpich 5
int T_Type_size_colt(MPI_Datatype datatype, MPI_Aint *size)
{
    debug_printf("T_Type_size_colt (4) called with %x %p\n", (int) datatype, size);

    return PMPI_Type_size(datatype, (int *) size);
}

#endif
#else // openmpi
int T_Type_size_colt(MPI_Datatype datatype, MPI_Aint *size)
{
    debug_printf("T_Type_size_colt (4) called with %x %p\n", (int) datatype, size);

    return PMPI_Type_size(datatype, (int *) size);
}
#endif

#endif //__T_MPIDEF_H__
