/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 *
 *  (C) 2001 by Argonne National Laboratory.
 *      See COPYRIGHT in top-level directory.
 */
/* Copyright (c) 2001-2020, The Ohio State University. All rights
 * reserved.
 *
 * This file is part of the MVAPICH2 software package developed by the
 * team members of The Ohio State University's Network-Based Computing
 * Laboratory (NBCL), headed by Professor Dhabaleswar K. (DK) Panda.
 *
 * For detailed copyright and licensing information, please refer to the
 * copyright file COPYRIGHT in the top level MVAPICH2 directory.
 *
 */

#include "helper_fns.h"

/* These functions are used in the implementation of collective
   operations. They are wrappers around MPID send/recv functions. They do
   sends/receives by setting the context offset to
   MPID_CONTEXT_INTRA_COLL or MPID_CONTEXT_INTER_COLL. */

#if defined(HAVE_PRAGMA_WEAK)
#pragma weak MPIC_Probe = PMPIC_Probe
#elif defined(HAVE_PRAGMA_HP_SEC_DEF)
#pragma _HP_SECONDARY_DEF PMPIC_Probe  MPIC_Probe
#elif defined(HAVE_PRAGMA_CRI_DUP)
#pragma _CRI duplicate MPIC_Probe as PMPIC_Probe
#elif defined(HAVE_WEAK_ATTRIBUTE)
int MPIC_Probe(int source, int tag, MPI_Comm comm, MPI_Status *status)
                  __attribute__((weak,alias("PMPIC_Probe")));
#endif
/* -- End Profiling Symbol Block */

/* Define MPICH_MPI_FROM_PMPI if weak symbols are not supported to build
   the MPI routines */
#ifndef MPICH_MPI_FROM_PMPI
#undef MPIC_Probe
#define MPIC_Probe PMPIC_Probe

#undef FUNCNAME
#define FUNCNAME MPIC_Probe
#undef FCNAME
#define FCNAME MPL_QUOTE(FUNCNAME)
int MPIC_Probe(int source, int tag, MPI_Comm comm, MPI_Status *status)
{
    int mpi_errno = MPI_SUCCESS;
    int context_id;
    MPID_Comm *comm_ptr;

    MPID_Comm_get_ptr( comm, comm_ptr );

    context_id = (comm_ptr->comm_kind == MPID_INTRACOMM) ?
        MPID_CONTEXT_INTRA_COLL : MPID_CONTEXT_INTER_COLL;
    
    mpi_errno = MPID_Probe(source, tag, comm_ptr, context_id, status);
    if (mpi_errno != MPI_SUCCESS) goto fn_fail;

 fn_exit:
    return mpi_errno;
 fn_fail:
    goto fn_exit;
}

#endif


#if defined(HAVE_PRAGMA_WEAK)
#pragma weak MPIC_Wait = PMPIC_Wait
#elif defined(HAVE_PRAGMA_HP_SEC_DEF)
#pragma _HP_SECONDARY_DEF PMPIC_Wait  MPIC_Wait
#elif defined(HAVE_PRAGMA_CRI_DUP)
#pragma _CRI duplicate MPIC_Wait as PMPIC_Wait
#elif defined(HAVE_WEAK_ATTRIBUTE)
int MPIC_Wait(MPID_Request * request_ptr, MPIR_Errflag_t *errflag)
                  __attribute__((weak,alias("PMPIC_Wait")));
#endif
/* -- End Profiling Symbol Block */

/* Define MPICH_MPI_FROM_PMPI if weak symbols are not supported to build
   the MPI routines */
#ifndef MPICH_MPI_FROM_PMPI
#undef MPIC_Wait
#define MPIC_Wait PMPIC_Wait


/* FIXME: For the brief-global and finer-grain control, we must ensure that
   the global lock is *not* held when this routine is called. (unless we change
   progress_start/end to grab the lock, in which case we must *still* make
   sure that the lock is not held when this routine is called). */
#undef FUNCNAME
#define FUNCNAME MPIC_Wait
#undef FCNAME
#define FCNAME MPL_QUOTE(FUNCNAME)
/* FIXME: For the brief-global and finer-grain control, we must ensure that
   the global lock is *not* held when this routine is called. (unless we change
   progress_start/end to grab the lock, in which case we must *still* make
   sure that the lock is not held when this routine is called). */
int MPIC_Wait(MPID_Request * request_ptr, MPIR_Errflag_t *errflag)		
{
    int mpi_errno = MPI_SUCCESS;
    MPIDI_STATE_DECL(MPID_STATE_MPIC_WAIT);

    MPIDI_PT2PT_FUNC_ENTER(MPID_STATE_MPIC_WAIT);

    MPIU_DBG_MSG_S(PT2PT, TYPICAL, "IN: errflag = %s", *errflag?"TRUE":"FALSE");

    if (request_ptr->kind == MPID_REQUEST_SEND)
        request_ptr->status.MPI_TAG = 0;

    if (!MPID_Request_is_complete(request_ptr))
    {
	MPID_Progress_state progress_state;
	
	MPID_Progress_start(&progress_state);
        while (!MPID_Request_is_complete(request_ptr))
	{
	    mpi_errno = MPID_Progress_wait(&progress_state);
	    if (mpi_errno) { MPIR_ERR_POP(mpi_errno); }
	}
	MPID_Progress_end(&progress_state);
    }

    if (request_ptr->kind == MPID_REQUEST_RECV)
        MPIR_Process_status(&request_ptr->status, errflag);

    MPIR_TAG_CLEAR_ERROR_BITS(request_ptr->status.MPI_TAG);

 fn_exit:
    MPIU_DBG_MSG_D(PT2PT, TYPICAL, "OUT: errflag = %d", *errflag);
    MPIDI_PT2PT_FUNC_EXIT(MPID_STATE_MPIC_WAIT);
    return mpi_errno;
 fn_fail:
    /* --BEGIN ERROR HANDLING-- */
    goto fn_exit;
    /* --END ERROR HANDLING-- */
}

#endif

#if defined(HAVE_PRAGMA_WEAK)
#pragma weak MPIC_Send = PMPIC_Send
#elif defined(HAVE_PRAGMA_HP_SEC_DEF)
#pragma _HP_SECONDARY_DEF PMPIC_Send  MPIC_Send
#elif defined(HAVE_PRAGMA_CRI_DUP)
#pragma _CRI duplicate MPIC_Send as PMPIC_Send
#elif defined(HAVE_WEAK_ATTRIBUTE)
int MPIC_Send(const void *buf, MPI_Aint count, MPI_Datatype datatype, int dest, int tag,
                 MPID_Comm *comm_ptr, MPIR_Errflag_t *errflag)
                  __attribute__((weak,alias("PMPIC_Send")));
#endif
/* -- End Profiling Symbol Block */

/* Define MPICH_MPI_FROM_PMPI if weak symbols are not supported to build
   the MPI routines */
#ifndef MPICH_MPI_FROM_PMPI
#undef MPIC_Send
#define MPIC_Send PMPIC_Send

/* Fault-tolerance versions.  When a process fails, collectives will
   still complete, however the result may be invalid.  Processes
   directly communicating with the failed process can detect the
   failure, however another mechanism is needed to commuinicate the
   failure to other processes receiving the invalid data.  To do this
   we introduce the _ft versions of the MPIC_ helper functions.  These
   functions take a pointer to an error flag.  When this is set to
   TRUE, the send functions will communicate the failure to the
   receiver.  If a function detects a failure, either by getting a
   failure in the communication operation, or by receiving an error
   indicator from a remote process, it sets the error flag to TRUE.

   In this implementation, we indicate an error to a remote process by
   sending an empty message instead of the requested buffer.  When a
   process receives an empty message, it knows to set the error flag.
   We count on the fact that collectives that exchange data (as
   opposed to barrier) will never send an empty message.  The barrier
   collective will not communicate failure information this way, but
   this is OK since there is no data that can be received corrupted. */

#undef FUNCNAME
#define FUNCNAME MPIC_Send
#undef FCNAME
#define FCNAME MPL_QUOTE(FUNCNAME)
int MPIC_Send(const void *buf, MPI_Aint count, MPI_Datatype datatype, int dest, int tag,
                 MPID_Comm *comm_ptr, MPIR_Errflag_t *errflag)
{
    int mpi_errno = MPI_SUCCESS;
    int context_id;
    MPID_Request *request_ptr = NULL;
    MPIDI_STATE_DECL(MPID_STATE_MPIC_SEND);

    MPIDI_FUNC_ENTER(MPID_STATE_MPIC_SEND);

    MPIU_DBG_MSG_D(PT2PT, TYPICAL, "IN: errflag = %d", *errflag);

    MPIR_ERR_CHKANDJUMP1((count < 0), mpi_errno, MPI_ERR_COUNT,
                         "**countneg", "**countneg %d", count);

    switch(*errflag) {
        case MPIR_ERR_NONE:
            break;
        case MPIR_ERR_PROC_FAILED:
            MPIR_TAG_SET_PROC_FAILURE_BIT(tag);
        default:
            MPIR_TAG_SET_ERROR_BIT(tag);
    }

    context_id = (comm_ptr->comm_kind == MPID_INTRACOMM) ?
        MPID_CONTEXT_INTRA_COLL : MPID_CONTEXT_INTER_COLL;

    mpi_errno = MPID_Send(buf, count, datatype, dest, tag, comm_ptr,
                          context_id, &request_ptr);
    if (mpi_errno) MPIR_ERR_POP(mpi_errno);
    if (request_ptr) {
        mpi_errno = PMPIC_Wait(request_ptr, errflag);
        if (mpi_errno) MPIR_ERR_POP(mpi_errno);
        MPID_Request_release(request_ptr);
    }

 fn_exit:
    MPIU_DBG_MSG_D(PT2PT, TYPICAL, "OUT: errflag = %d", *errflag);
    MPIDI_FUNC_EXIT(MPID_STATE_MPIC_SEND);
    return mpi_errno;
 fn_fail:
    /* --BEGIN ERROR HANDLING-- */
    if (request_ptr) MPID_Request_release(request_ptr);
    if (mpi_errno && !*errflag) {
        if (MPIX_ERR_PROC_FAILED == MPIR_ERR_GET_CLASS(mpi_errno)) {
            *errflag = MPIR_ERR_PROC_FAILED;
        } else {
            *errflag = MPIR_ERR_OTHER;
        }
    }
    goto fn_exit;
    /* --END ERROR HANDLING-- */
}

#endif

#if defined(HAVE_PRAGMA_WEAK)
#pragma weak MPIC_Recv = PMPIC_Recv
#elif defined(HAVE_PRAGMA_HP_SEC_DEF)
#pragma _HP_SECONDARY_DEF PMPIC_Recv  MPIC_Recv
#elif defined(HAVE_PRAGMA_CRI_DUP)
#pragma _CRI duplicate MPIC_Recv as PMPIC_Recv
#elif defined(HAVE_WEAK_ATTRIBUTE)
int MPIC_Recv(void *buf, MPI_Aint count, MPI_Datatype datatype, int source, int tag,
                 MPID_Comm *comm_ptr, MPI_Status *status, MPIR_Errflag_t *errflag)
                  __attribute__((weak,alias("PMPIC_Recv")));
#endif
/* -- End Profiling Symbol Block */

/* Define MPICH_MPI_FROM_PMPI if weak symbols are not supported to build
   the MPI routines */
#ifndef MPICH_MPI_FROM_PMPI
#undef MPIC_Recv
#define MPIC_Recv PMPIC_Recv

#undef FUNCNAME
#define FUNCNAME MPIC_Recv
#undef FCNAME
#define FCNAME MPL_QUOTE(FUNCNAME)
int MPIC_Recv(void *buf, MPI_Aint count, MPI_Datatype datatype, int source, int tag,
                 MPID_Comm *comm_ptr, MPI_Status *status, MPIR_Errflag_t *errflag)
{
    int mpi_errno = MPI_SUCCESS;
    int context_id;
    MPI_Status mystatus;
    MPID_Request *request_ptr = NULL;
    MPIDI_STATE_DECL(MPID_STATE_MPIC_RECV);

    MPIDI_FUNC_ENTER(MPID_STATE_MPIC_RECV);

    MPIU_DBG_MSG_D(PT2PT, TYPICAL, "IN: errflag = %d", *errflag);

    MPIR_ERR_CHKANDJUMP1((count < 0), mpi_errno, MPI_ERR_COUNT,
                         "**countneg", "**countneg %d", count);

    context_id = (comm_ptr->comm_kind == MPID_INTRACOMM) ?
        MPID_CONTEXT_INTRA_COLL : MPID_CONTEXT_INTER_COLL;

    if (status == MPI_STATUS_IGNORE)
        status = &mystatus;

    mpi_errno = MPID_Recv(buf, count, datatype, source, tag, comm_ptr,
                          context_id, status, &request_ptr);
    if (mpi_errno) MPIR_ERR_POP(mpi_errno);
    if (request_ptr) {
        mpi_errno = PMPIC_Wait(request_ptr, errflag);
        if (mpi_errno != MPI_SUCCESS)
            MPIR_ERR_POP(mpi_errno);

        *status = request_ptr->status;
        mpi_errno = status->MPI_ERROR;
        MPID_Request_release(request_ptr);
    } else {
        MPIR_Process_status(status, errflag);

        MPIR_TAG_CLEAR_ERROR_BITS(status->MPI_TAG);
    }

    if (MPI_SUCCESS == MPIR_ERR_GET_CLASS(status->MPI_ERROR)) {
        MPIU_Assert(status->MPI_TAG == tag);
    }

 fn_exit:
    MPIU_DBG_MSG_D(PT2PT, TYPICAL, "OUT: errflag = %d", *errflag);
    MPIDI_FUNC_EXIT(MPID_STATE_MPIC_RECV);
    return mpi_errno;
 fn_fail:
    /* --BEGIN ERROR HANDLING-- */
    if (request_ptr) MPID_Request_release(request_ptr);
    goto fn_exit;
    /* --END ERROR HANDLING-- */
}
#endif

#if defined(HAVE_PRAGMA_WEAK)
#pragma weak MPIC_Ssend = PMPIC_Ssend
#elif defined(HAVE_PRAGMA_HP_SEC_DEF)
#pragma _HP_SECONDARY_DEF PMPIC_Ssend  MPIC_Ssend
#elif defined(HAVE_PRAGMA_CRI_DUP)
#pragma _CRI duplicate MPIC_Ssend as PMPIC_Ssend
#elif defined(HAVE_WEAK_ATTRIBUTE)
int MPIC_Ssend(const void *buf, MPI_Aint count, MPI_Datatype datatype, int dest, int tag,
                  MPID_Comm *comm_ptr, MPIR_Errflag_t *errflag)
                  __attribute__((weak,alias("PMPIC_Ssend")));
#endif
/* -- End Profiling Symbol Block */

/* Define MPICH_MPI_FROM_PMPI if weak symbols are not supported to build
   the MPI routines */
#ifndef MPICH_MPI_FROM_PMPI
#undef MPIC_Ssend
#define MPIC_Ssend PMPIC_Ssend

#undef FUNCNAME
#define FUNCNAME MPIC_Ssend
#undef FCNAME
#define FCNAME MPL_QUOTE(FUNCNAME)
int MPIC_Ssend(const void *buf, MPI_Aint count, MPI_Datatype datatype, int dest, int tag,
                  MPID_Comm *comm_ptr, MPIR_Errflag_t *errflag)
{
    int mpi_errno = MPI_SUCCESS;
    int context_id;
    MPID_Request *request_ptr = NULL;
    MPIDI_STATE_DECL(MPID_STATE_MPIC_SSEND);

    MPIDI_FUNC_ENTER(MPID_STATE_MPIC_SSEND);

    MPIU_DBG_MSG_D(PT2PT, TYPICAL, "IN: errflag = %d", *errflag);

    MPIR_ERR_CHKANDJUMP1((count < 0), mpi_errno, MPI_ERR_COUNT,
            "**countneg", "**countneg %d", count);

    context_id = (comm_ptr->comm_kind == MPID_INTRACOMM) ?
        MPID_CONTEXT_INTRA_COLL : MPID_CONTEXT_INTER_COLL;

    switch(*errflag) {
        case MPIR_ERR_NONE:
            break;
        case MPIR_ERR_PROC_FAILED:
            MPIR_TAG_SET_PROC_FAILURE_BIT(tag);
        default:
            MPIR_TAG_SET_ERROR_BIT(tag);
    }

    mpi_errno = MPID_Ssend(buf, count, datatype, dest, tag, comm_ptr,
                           context_id, &request_ptr);
    if (mpi_errno) MPIR_ERR_POP(mpi_errno);
    if (request_ptr) {
        mpi_errno = PMPIC_Wait(request_ptr, errflag);
        if (mpi_errno) MPIR_ERR_POP(mpi_errno);
        MPID_Request_release(request_ptr);
    }

 fn_exit:
    MPIU_DBG_MSG_D(PT2PT, TYPICAL, "OUT: errflag = %d", *errflag);
    MPIDI_FUNC_EXIT(MPID_STATE_MPIC_SSEND);
    return mpi_errno;
 fn_fail:
    /* --BEGIN ERROR HANDLING-- */
    if (request_ptr) MPID_Request_release(request_ptr);
    if (mpi_errno && !*errflag) {
        if (MPIX_ERR_PROC_FAILED == MPIR_ERR_GET_CLASS(mpi_errno)) {
            *errflag = MPIR_ERR_PROC_FAILED;
        } else {
            *errflag = MPIR_ERR_OTHER;
        }
    }
    goto fn_exit;
    /* --END ERROR HANDLING-- */
}

#endif

#if defined(HAVE_PRAGMA_WEAK)
#pragma weak MPIC_Sendrecv = PMPIC_Sendrecv
#elif defined(HAVE_PRAGMA_HP_SEC_DEF)
#pragma _HP_SECONDARY_DEF PMPIC_Sendrecv  MPIC_Sendrecv
#elif defined(HAVE_PRAGMA_CRI_DUP)
#pragma _CRI duplicate MPIC_Sendrecv as PMPIC_Sendrecv
#elif defined(HAVE_WEAK_ATTRIBUTE)
int MPIC_Sendrecv(const void *sendbuf, MPI_Aint sendcount, MPI_Datatype sendtype,
                     int dest, int sendtag, void *recvbuf, MPI_Aint recvcount,
                     MPI_Datatype recvtype, int source, int recvtag,
                     MPID_Comm *comm_ptr, MPI_Status *status, MPIR_Errflag_t *errflag)
                  __attribute__((weak,alias("PMPIC_Sendrecv")));
#endif
/* -- End Profiling Symbol Block */

/* Define MPICH_MPI_FROM_PMPI if weak symbols are not supported to build
   the MPI routines */
#ifndef MPICH_MPI_FROM_PMPI
#undef MPIC_Sendrecv
#define MPIC_Sendrecv PMPIC_Sendrecv

#undef FUNCNAME
#define FUNCNAME MPIC_Sendrecv
#undef FCNAME
#define FCNAME MPL_QUOTE(FUNCNAME)
int MPIC_Sendrecv(const void *sendbuf, MPI_Aint sendcount, MPI_Datatype sendtype,
                     int dest, int sendtag, void *recvbuf, MPI_Aint recvcount,
                     MPI_Datatype recvtype, int source, int recvtag,
                     MPID_Comm *comm_ptr, MPI_Status *status, MPIR_Errflag_t *errflag)
{
    int mpi_errno = MPI_SUCCESS;
    int context_id;
    MPI_Status mystatus;
    MPID_Request *recv_req_ptr = NULL, *send_req_ptr = NULL;
    MPIDI_STATE_DECL(MPID_STATE_MPIC_SENDRECV);

    MPIDI_FUNC_ENTER(MPID_STATE_MPIC_SENDRECV);

    MPIU_DBG_MSG_S(PT2PT, TYPICAL, "IN: errflag = %s", *errflag?"TRUE":"FALSE");

    MPIR_ERR_CHKANDJUMP1((sendcount < 0), mpi_errno, MPI_ERR_COUNT,
                         "**countneg", "**countneg %d", sendcount);
    MPIR_ERR_CHKANDJUMP1((recvcount < 0), mpi_errno, MPI_ERR_COUNT,
                         "**countneg", "**countneg %d", recvcount);

    context_id = (comm_ptr->comm_kind == MPID_INTRACOMM) ?
        MPID_CONTEXT_INTRA_COLL : MPID_CONTEXT_INTER_COLL;

    if (status == MPI_STATUS_IGNORE) status = &mystatus;
    switch(*errflag) {
        case MPIR_ERR_NONE:
            break;
        case MPIR_ERR_PROC_FAILED:
            MPIR_TAG_SET_PROC_FAILURE_BIT(sendtag);
        default:
            MPIR_TAG_SET_ERROR_BIT(sendtag);
    }

    mpi_errno = MPID_Irecv(recvbuf, recvcount, recvtype, source, recvtag,
                           comm_ptr, context_id, &recv_req_ptr);
    if (mpi_errno) MPIR_ERR_POP(mpi_errno);
    mpi_errno = MPID_Isend(sendbuf, sendcount, sendtype, dest, sendtag,
                           comm_ptr, context_id, &send_req_ptr);
    if (mpi_errno) MPIR_ERR_POP(mpi_errno);

    mpi_errno = PMPIC_Wait(send_req_ptr, errflag);
    if (mpi_errno) MPIR_ERR_POP(mpi_errno);
    mpi_errno = PMPIC_Wait(recv_req_ptr, errflag);
    if (mpi_errno) MPIR_ERR_POPFATAL(mpi_errno);

    *status = recv_req_ptr->status;

    if (mpi_errno == MPI_SUCCESS) {
        mpi_errno = recv_req_ptr->status.MPI_ERROR;

        if (mpi_errno == MPI_SUCCESS) {
            mpi_errno = send_req_ptr->status.MPI_ERROR;
        }
    }

    MPID_Request_release(send_req_ptr);
    MPID_Request_release(recv_req_ptr);

 fn_exit:
    MPIU_DBG_MSG_D(PT2PT, TYPICAL, "OUT: errflag = %d", *errflag);

    MPIDI_FUNC_EXIT(MPID_STATE_MPIC_SENDRECV);
    return mpi_errno;
 fn_fail:
    if (send_req_ptr)
        MPID_Request_release(send_req_ptr);
    if (recv_req_ptr)
        MPID_Request_release(recv_req_ptr);
    goto fn_exit;
}

#endif

#if defined(HAVE_PRAGMA_WEAK)
#pragma weak MPIC_Sendrecv_replace = PMPIC_Sendrecv_replace
#elif defined(HAVE_PRAGMA_HP_SEC_DEF)
#pragma _HP_SECONDARY_DEF PMPIC_Sendrecv_replace  MPIC_Sendrecv_replace
#elif defined(HAVE_PRAGMA_CRI_DUP)
#pragma _CRI duplicate MPIC_Sendrecv_replace as PMPIC_Sendrecv_replace
#elif defined(HAVE_WEAK_ATTRIBUTE)
int MPIC_Sendrecv_replace(void *buf, MPI_Aint count, MPI_Datatype datatype,
                             int dest, int sendtag,
                             int source, int recvtag,
                             MPID_Comm *comm_ptr, MPI_Status *status, MPIR_Errflag_t *errflag)
                  __attribute__((weak,alias("PMPIC_Sendrecv_replace")));
#endif
/* -- End Profiling Symbol Block */

/* Define MPICH_MPI_FROM_PMPI if weak symbols are not supported to build
   the MPI routines */
#ifndef MPICH_MPI_FROM_PMPI
#undef MPIC_Sendrecv_replace
#define MPIC_Sendrecv_replace PMPIC_Sendrecv_replace

/* NOTE: for regular collectives (as opposed to irregular collectives) calling
 * this function repeatedly will almost always be slower than performing the
 * equivalent inline because of the overhead of the repeated malloc/free */
#undef FUNCNAME
#define FUNCNAME MPIC_Sendrecv_replace
#undef FCNAME
#define FCNAME MPL_QUOTE(FUNCNAME)
int MPIC_Sendrecv_replace(void *buf, MPI_Aint count, MPI_Datatype datatype,
                             int dest, int sendtag,
                             int source, int recvtag,
                             MPID_Comm *comm_ptr, MPI_Status *status, MPIR_Errflag_t *errflag)
{
    int mpi_errno = MPI_SUCCESS;
    MPI_Status mystatus;
    MPIU_Context_id_t context_id_offset;
    MPID_Request *sreq = NULL;
    MPID_Request *rreq = NULL;
    void *tmpbuf = NULL;
    MPI_Aint tmpbuf_size = 0;
    MPI_Aint tmpbuf_count = 0;
    MPIU_CHKLMEM_DECL(1);
    MPIDI_STATE_DECL(MPID_STATE_MPIC_SENDRECV_REPLACE);
#ifdef MPID_LOG_ARROWS
    /* The logging macros log sendcount and recvcount */
    int sendcount = count, recvcount = count;
#endif

    MPIDI_FUNC_ENTER(MPID_STATE_MPIC_SENDRECV_REPLACE);

    MPIU_DBG_MSG_D(PT2PT, TYPICAL, "IN: errflag = %d", *errflag);

    MPIR_ERR_CHKANDJUMP1((count < 0), mpi_errno, MPI_ERR_COUNT,
                         "**countneg", "**countneg %d", count);

    if (status == MPI_STATUS_IGNORE) status = &mystatus;
    switch(*errflag) {
        case MPIR_ERR_NONE:
            break;
        case MPIR_ERR_PROC_FAILED:
            MPIR_TAG_SET_PROC_FAILURE_BIT(sendtag);
        default:
            MPIR_TAG_SET_ERROR_BIT(sendtag);
    }

    context_id_offset = (comm_ptr->comm_kind == MPID_INTRACOMM) ?
        MPID_CONTEXT_INTRA_COLL : MPID_CONTEXT_INTER_COLL;

    if (count > 0 && dest != MPI_PROC_NULL) {
        MPIR_Pack_size_impl(count, datatype, &tmpbuf_size);
        MPIU_CHKLMEM_MALLOC(tmpbuf, void *, tmpbuf_size, mpi_errno, "temporary send buffer");

        mpi_errno = MPIR_Pack_impl(buf, count, datatype, tmpbuf, tmpbuf_size, &tmpbuf_count);
        if (mpi_errno) MPIR_ERR_POP(mpi_errno);
    }

    mpi_errno = MPID_Irecv(buf, count, datatype, source, recvtag,
                           comm_ptr, context_id_offset, &rreq);
    if (mpi_errno) MPIR_ERR_POP(mpi_errno);

    mpi_errno = MPID_Isend(tmpbuf, tmpbuf_count, MPI_PACKED, dest,
                           sendtag, comm_ptr, context_id_offset, &sreq);
    if (mpi_errno != MPI_SUCCESS) {
        /* --BEGIN ERROR HANDLING-- */
        /* FIXME: should we cancel the pending (possibly completed) receive
         * request or wait for it to complete? */
        MPID_Request_release(rreq);
        MPIR_ERR_POP(mpi_errno);
        /* --END ERROR HANDLING-- */
    }

    mpi_errno = PMPIC_Wait(sreq, errflag);
    if (mpi_errno) MPIR_ERR_POP(mpi_errno);
    mpi_errno = PMPIC_Wait(rreq, errflag);
    if (mpi_errno) MPIR_ERR_POP(mpi_errno);

    *status = rreq->status;

    if (mpi_errno == MPI_SUCCESS) {
        mpi_errno = rreq->status.MPI_ERROR;

        if (mpi_errno == MPI_SUCCESS) {
            mpi_errno = sreq->status.MPI_ERROR;
        }
    }

    MPID_Request_release(sreq);
    MPID_Request_release(rreq);

 fn_exit:
    MPIU_CHKLMEM_FREEALL();
    MPIU_DBG_MSG_D(PT2PT, TYPICAL, "OUT: errflag = %d", *errflag);
    MPIDI_FUNC_EXIT(MPID_STATE_MPIC_SENDRECV_REPLACE);
    return mpi_errno;
 fn_fail:
     if (sreq)
         MPID_Request_release(sreq);
     if (rreq)
         MPID_Request_release(rreq);
    goto fn_exit;
}
#endif

#if defined(HAVE_PRAGMA_WEAK)
#pragma weak MPIC_Isend = PMPIC_Isend
#elif defined(HAVE_PRAGMA_HP_SEC_DEF)
#pragma _HP_SECONDARY_DEF PMPIC_Isend  MPIC_Isend
#elif defined(HAVE_PRAGMA_CRI_DUP)
#pragma _CRI duplicate MPIC_Isend as PMPIC_Isend
#elif defined(HAVE_WEAK_ATTRIBUTE)
int MPIC_Isend(const void *buf, MPI_Aint count, MPI_Datatype datatype, int dest, int tag,
                  MPID_Comm *comm_ptr, MPID_Request **request_ptr, MPIR_Errflag_t *errflag)
                  __attribute__((weak,alias("PMPIC_Isend")));
#endif
/* -- End Profiling Symbol Block */

/* Define MPICH_MPI_FROM_PMPI if weak symbols are not supported to build
   the MPI routines */
#ifndef MPICH_MPI_FROM_PMPI
#undef MPIC_Isend
#define MPIC_Isend PMPIC_Isend

#undef FUNCNAME
#define FUNCNAME MPIC_Isend
#undef FCNAME
#define FCNAME MPL_QUOTE(FUNCNAME)
int MPIC_Isend(const void *buf, MPI_Aint count, MPI_Datatype datatype, int dest, int tag,
                  MPID_Comm *comm_ptr, MPID_Request **request_ptr, MPIR_Errflag_t *errflag)
{
    int mpi_errno = MPI_SUCCESS;
    int context_id;
    MPIDI_STATE_DECL(MPID_STATE_MPIC_ISEND);

    MPIDI_FUNC_ENTER(MPID_STATE_MPIC_ISEND);

    MPIU_DBG_MSG_D(PT2PT, TYPICAL, "IN: errflag = %d", *errflag);

    MPIR_ERR_CHKANDJUMP1((count < 0), mpi_errno, MPI_ERR_COUNT,
                         "**countneg", "**countneg %d", count);

    switch(*errflag) {
        case MPIR_ERR_NONE:
            break;
        case MPIR_ERR_PROC_FAILED:
            MPIR_TAG_SET_PROC_FAILURE_BIT(tag);
        default:
            MPIR_TAG_SET_ERROR_BIT(tag);
    }

    context_id = (comm_ptr->comm_kind == MPID_INTRACOMM) ?
        MPID_CONTEXT_INTRA_COLL : MPID_CONTEXT_INTER_COLL;

    mpi_errno = MPID_Isend(buf, count, datatype, dest, tag, comm_ptr,
            context_id, request_ptr);
    if (mpi_errno) MPIR_ERR_POP(mpi_errno);

 fn_exit:
    MPIDI_FUNC_EXIT(MPID_STATE_MPIC_ISEND);
    return mpi_errno;
 fn_fail:
    goto fn_exit;
}

#endif

#if defined(HAVE_PRAGMA_WEAK)
#pragma weak MPIC_Issend = PMPIC_Issend
#elif defined(HAVE_PRAGMA_HP_SEC_DEF)
#pragma _HP_SECONDARY_DEF PMPIC_Issend  MPIC_Issend
#elif defined(HAVE_PRAGMA_CRI_DUP)
#pragma _CRI duplicate MPIC_Issend as PMPIC_Issend
#elif defined(HAVE_WEAK_ATTRIBUTE)
int MPIC_Issend(const void *buf, MPI_Aint count, MPI_Datatype datatype, int dest, int tag,
                  MPID_Comm *comm_ptr, MPID_Request **request_ptr, MPIR_Errflag_t *errflag)
                  __attribute__((weak,alias("PMPIC_Issend")));
#endif
/* -- End Profiling Symbol Block */

/* Define MPICH_MPI_FROM_PMPI if weak symbols are not supported to build
   the MPI routines */
#ifndef MPICH_MPI_FROM_PMPI
#undef MPIC_Issend
#define MPIC_Issend PMPIC_Issend

#undef FUNCNAME
#define FUNCNAME MPIC_Issend
#undef FCNAME
#define FCNAME MPL_QUOTE(FUNCNAME)
int MPIC_Issend(const void *buf, MPI_Aint count, MPI_Datatype datatype, int dest, int tag,
                  MPID_Comm *comm_ptr, MPID_Request **request_ptr, MPIR_Errflag_t *errflag)
{
    int mpi_errno = MPI_SUCCESS;
    int context_id;
    MPIDI_STATE_DECL(MPID_STATE_MPIC_ISSEND);

    MPIDI_FUNC_ENTER(MPID_STATE_MPIC_ISSEND);

    MPIU_DBG_MSG_D(PT2PT, TYPICAL, "IN: errflag = %d", *errflag);

    MPIR_ERR_CHKANDJUMP1((count < 0), mpi_errno, MPI_ERR_COUNT,
                         "**countneg", "**countneg %d", count);

    switch(*errflag) {
        case MPIR_ERR_NONE:
            break;
        case MPIR_ERR_PROC_FAILED:
            MPIR_TAG_SET_PROC_FAILURE_BIT(tag);
        default:
            MPIR_TAG_SET_ERROR_BIT(tag);
    }

    context_id = (comm_ptr->comm_kind == MPID_INTRACOMM) ?
        MPID_CONTEXT_INTRA_COLL : MPID_CONTEXT_INTER_COLL;

    mpi_errno = MPID_Issend(buf, count, datatype, dest, tag, comm_ptr,
            context_id, request_ptr);
    if (mpi_errno) MPIR_ERR_POP(mpi_errno);

 fn_exit:
    MPIDI_FUNC_EXIT(MPID_STATE_MPIC_ISSEND);
    return mpi_errno;
 fn_fail:
    goto fn_exit;
}
#endif

#if defined(HAVE_PRAGMA_WEAK)
#pragma weak MPIC_Irecv = PMPIC_Irecv
#elif defined(HAVE_PRAGMA_HP_SEC_DEF)
#pragma _HP_SECONDARY_DEF PMPIC_Irecv  MPIC_Irecv
#elif defined(HAVE_PRAGMA_CRI_DUP)
#pragma _CRI duplicate MPIC_Irecv as PMPIC_Irecv
#elif defined(HAVE_WEAK_ATTRIBUTE)
int MPIC_Irecv(void *buf, MPI_Aint count, MPI_Datatype datatype, int source,
                  int tag, MPID_Comm *comm_ptr, MPID_Request **request_ptr)
                  __attribute__((weak,alias("PMPIC_Irecv")));
#endif
/* -- End Profiling Symbol Block */

/* Define MPICH_MPI_FROM_PMPI if weak symbols are not supported to build
   the MPI routines */
#ifndef MPICH_MPI_FROM_PMPI
#undef MPIC_Irecv
#define MPIC_Irecv PMPIC_Irecv

#undef FUNCNAME
#define FUNCNAME MPIC_Irecv
#undef FCNAME
#define FCNAME MPL_QUOTE(FUNCNAME)
int MPIC_Irecv(void *buf, MPI_Aint count, MPI_Datatype datatype, int source,
                  int tag, MPID_Comm *comm_ptr, MPID_Request **request_ptr)
{
    int mpi_errno = MPI_SUCCESS;
    int context_id;
    MPIDI_STATE_DECL(MPID_STATE_MPIC_IRECV);

    MPIDI_FUNC_ENTER(MPID_STATE_MPIC_IRECV);

    MPIR_ERR_CHKANDJUMP1((count < 0), mpi_errno, MPI_ERR_COUNT,
                         "**countneg", "**countneg %d", count);

    context_id = (comm_ptr->comm_kind == MPID_INTRACOMM) ?
        MPID_CONTEXT_INTRA_COLL : MPID_CONTEXT_INTER_COLL;

    mpi_errno = MPID_Irecv(buf, count, datatype, source, tag, comm_ptr,
            context_id, request_ptr);
    if (mpi_errno) MPIR_ERR_POP(mpi_errno);

 fn_exit:
    MPIDI_FUNC_EXIT(MPID_STATE_MPIC_IRECV);
    return mpi_errno;
 fn_fail:
    goto fn_exit;
}
#endif

#if defined(HAVE_PRAGMA_WEAK)
#pragma weak MPIC_Waitall = PMPIC_Waitall
#elif defined(HAVE_PRAGMA_HP_SEC_DEF)
#pragma _HP_SECONDARY_DEF PMPIC_Waitall  MPIC_Waitall
#elif defined(HAVE_PRAGMA_CRI_DUP)
#pragma _CRI duplicate MPIC_Waitall as PMPIC_Waitall
#elif defined(HAVE_WEAK_ATTRIBUTE)
int MPIC_Waitall(int numreq, MPID_Request *requests[], MPI_Status statuses[], MPIR_Errflag_t *errflag)
                  __attribute__((weak,alias("PMPIC_Waitall")));
#endif
/* -- End Profiling Symbol Block */

/* Define MPICH_MPI_FROM_PMPI if weak symbols are not supported to build
   the MPI routines */
#ifndef MPICH_MPI_FROM_PMPI
#undef MPIC_Waitall
#define MPIC_Waitall PMPIC_Waitall


#undef FUNCNAME
#define FUNCNAME MPIC_Waitall
#undef FCNAME
#define FCNAME MPL_QUOTE(FUNCNAME)
int MPIC_Waitall(int numreq, MPID_Request *requests[], MPI_Status statuses[], MPIR_Errflag_t *errflag)
{
    int mpi_errno = MPI_SUCCESS;
    int i;
    MPI_Request request_ptr_array[MPIC_REQUEST_PTR_ARRAY_SIZE];
    MPI_Request *request_ptrs = request_ptr_array;
    MPI_Status status_static_array[MPIC_REQUEST_PTR_ARRAY_SIZE];
    MPI_Status *status_array = statuses;
    MPIDI_STATE_DECL(MPID_STATE_MPIC_WAITALL);
    MPIU_CHKLMEM_DECL(2);

    MPIDI_FUNC_ENTER(MPID_STATE_MPIC_WAITALL);

    MPIU_DBG_MSG_S(PT2PT, TYPICAL, "IN: errflag = %s", *errflag?"TRUE":"FALSE");

    if (statuses == MPI_STATUSES_IGNORE) {
        status_array = status_static_array;
    }

    if (numreq > MPIC_REQUEST_PTR_ARRAY_SIZE) {
        MPIU_CHKLMEM_MALLOC(request_ptrs, MPI_Request *, numreq * sizeof(MPI_Request), mpi_errno, "request pointers");
        if (statuses == MPI_STATUSES_IGNORE) {
            MPIU_CHKLMEM_MALLOC(status_array, MPI_Status *, numreq * sizeof(MPI_Status), mpi_errno, "status objects");
        }
    }

    for (i = 0; i < numreq; ++i) {
        /* The MPI_TAG field is not set for send operations, so if we want
        to check for the error bit in the tag below, we should initialize all
        tag fields here. */
        status_array[i].MPI_TAG = 0;
        status_array[i].MPI_SOURCE = MPI_PROC_NULL;

        /* Convert the MPID_Request objects to MPI_Request objects */
        request_ptrs[i] = requests[i]->handle;
    }

    mpi_errno = MPIR_Waitall_impl(numreq, request_ptrs, status_array);

    /* The errflag value here is for all requests, not just a single one.  If
     * in the future, this function is used for multiple collectives at a
     * single time, we may have to change that. */
    for (i = 0; i < numreq; ++i) {
        MPIR_Process_status(&status_array[i], errflag);

        MPIR_TAG_CLEAR_ERROR_BITS(status_array[i].MPI_TAG);
    }

 fn_exit:
    if (numreq > MPIC_REQUEST_PTR_ARRAY_SIZE)
        MPIU_CHKLMEM_FREEALL();

    MPIU_DBG_MSG_D(PT2PT, TYPICAL, "OUT: errflag = %d", *errflag);
    MPIDI_FUNC_EXIT(MPID_STATE_MPIC_WAITALL);
    return mpi_errno;
 fn_fail:
    goto fn_exit;
}
#endif


#if defined(HAVE_PRAGMA_WEAK)
#pragma weak MPIC_Waitany = PMPIC_Waitany
#elif defined(HAVE_PRAGMA_HP_SEC_DEF)
#pragma _HP_SECONDARY_DEF PMPIC_Waitany  MPIC_Waitany
#elif defined(HAVE_PRAGMA_CRI_DUP)
#pragma _CRI duplicate MPIC_Waitany as PMPIC_Waitany
#elif defined(HAVE_WEAK_ATTRIBUTE)
int MPIC_Waitany(int numreq, MPID_Request *requests[], int *indx, MPI_Status* status, MPIR_Errflag_t *errflag)
                  __attribute__((weak,alias("PMPIC_Waitany")));
#endif
/* -- End Profiling Symbol Block */

/* Define MPICH_MPI_FROM_PMPI if weak symbols are not supported to build
   the MPI routines */
#ifndef MPICH_MPI_FROM_PMPI
#undef MPIC_Waitany
#define MPIC_Waitany PMPIC_Waitany


#undef FUNCNAME
#define FUNCNAME MPIC_Waitany
#undef FCNAME
#define FCNAME MPL_QUOTE(FUNCNAME)
int MPIC_Waitany(int numreq, MPID_Request *requests[], int *indx, MPI_Status* status, MPIR_Errflag_t *errflag)
{
    int mpi_errno = MPI_SUCCESS;
    int i;
    MPID_Progress_state progress_state;
    MPI_Status status_static;
    int n_inactive;
    int active_flag;
    int init_req_array;
    int found_nonnull_req;
    int last_disabled_anysource = -1;

    MPIDI_STATE_DECL(MPID_STATE_MPIC_WAITANY);
    MPIU_CHKLMEM_DECL(2);

    MPIDI_FUNC_ENTER(MPID_STATE_MPIC_WAITANY);

    MPIU_DBG_MSG_S(PT2PT, TYPICAL, "IN: errflag = %s", *errflag?"TRUE":"FALSE");

    if (status == MPI_STATUS_IGNORE) {
        status = &status_static;
    }

    n_inactive = 0;
    init_req_array = TRUE;
    found_nonnull_req = FALSE;

    MPID_Progress_start(&progress_state);
    for(;;)
    {
	for (i = 0; i < numreq; i++)
	{
            if (requests[i] == NULL)  continue;
            if (requests[i]->handle == MPI_REQUEST_NULL) continue;
            /* we found at least one non-null request */
            found_nonnull_req = TRUE;

            if (requests[i]->kind == MPID_UREQUEST && requests[i]->greq_fns->poll_fn != NULL)
	    {
                /* this is a generalized request; make progress on it */
                mpi_errno = (requests[i]->greq_fns->poll_fn)(requests[i]->greq_fns->grequest_extra_state, status);
		if (mpi_errno != MPI_SUCCESS) goto fn_progress_end_fail;
	    }
            if (MPID_Request_is_complete(requests[i]))
	    {
		mpi_errno = MPIR_Request_complete(&requests[i]->handle, 
						  requests[i], status, 
						  &active_flag);
		if (active_flag)
		{
		    *indx = i;
		    goto break_l1;
		}
		else
		{
		    ++n_inactive;
		    requests[i] = NULL;

		    if (n_inactive == numreq)
		    {
			*indx = MPI_UNDEFINED;
			/* status is set to empty by MPIR_Request_complete */
			goto break_l1;
		    }
		}
            } else if (unlikely(MPIR_CVAR_ENABLE_FT &&
                        MPID_Request_is_anysource(requests[i]) &&
                        !MPID_Comm_AS_enabled(requests[i]->comm))) {
                last_disabled_anysource = i;
            }
	}
        init_req_array = FALSE;

        if (!found_nonnull_req)
        {
            /* all requests were NULL */
            *indx = MPI_UNDEFINED;
            if (status != NULL)    /* could be null if numreq=0 */
                MPIR_Status_set_empty(status);
            goto break_l1;
        }

        /* If none of the requests completed, mark the last anysource request
         * as pending failure and break out. */
        if (unlikely(last_disabled_anysource != -1))
        {
            MPIR_ERR_SET(mpi_errno, MPIX_ERR_PROC_FAILED_PENDING, "**failure_pending");
            if (status != MPI_STATUS_IGNORE) status->MPI_ERROR = mpi_errno;
            goto fn_progress_end_fail;
        }

	mpi_errno = MPID_Progress_wait(&progress_state);
	if (mpi_errno != MPI_SUCCESS) goto fn_progress_end_fail;
    }
  break_l1:
    MPID_Progress_end(&progress_state);

 fn_exit:
    MPIU_DBG_MSG_D(PT2PT, TYPICAL, "OUT: errflag = %d", *errflag);
    MPIDI_FUNC_EXIT(MPID_STATE_MPIC_WAITANY);
    return mpi_errno;

  fn_progress_end_fail:
    MPID_Progress_end(&progress_state);

 fn_fail:
    goto fn_exit;
}
#endif
