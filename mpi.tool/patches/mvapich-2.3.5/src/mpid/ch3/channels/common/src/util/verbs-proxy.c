
#include <infiniband/verbs.h>
#define VERBS_PROXY_H /* avoid accidental include */
#include "mpiimpl.h"


void mv2_proxy_ibv_ack_cq_events(struct ibv_cq *cq, unsigned int nevents);
#if defined(HAVE_PRAGMA_WEAK)
#pragma weak mv2_proxy_ibv_ack_cq_events = t_mv2_proxy_ibv_ack_cq_events
#elif defined(HAVE_PRAGMA_HP_SEC_DEF)
#pragma _HP_SECONDARY_DEF t_mv2_proxy_ibv_ack_cq_events  mv2_proxy_ibv_ack_cq_events
#elif defined(HAVE_PRAGMA_CRI_DUP)
#pragma _CRI duplicate mv2_proxy_ibv_ack_cq_events as t_mv2_proxy_ibv_ack_cq_events
#elif defined(HAVE_WEAK_ATTRIBUTE)
void mv2_proxy_ibv_ack_cq_events(struct ibv_cq *cq, unsigned int nevents)
                  __attribute__((weak,alias("t_mv2_proxy_ibv_ack_cq_events")));
#endif
/* -- End Profiling Symbol Block */

/* Define MPICH_MPI_FROM_PMPI if weak symbols are not supported to build
 the MPI routines */
#ifndef MPICH_MPI_FROM_PMPI
#undef mv2_proxy_ibv_ack_cq_events
#define mv2_proxy_ibv_ack_cq_events t_mv2_proxy_ibv_ack_cq_events

#undef FUNCNAME
#define FUNCNAME mv2_proxy_ibv_ack_cq_events
#undef FCNAME
#define FCNAME MPL_QUOTE(FUNCNAME)

void mv2_proxy_ibv_ack_cq_events(struct ibv_cq *cq, unsigned int nevents)
{
    ibv_ack_cq_events(cq,nevents);
    return;
}

#endif

void mv2_proxy_ibv_ack_async_event(struct ibv_async_event *event);
#if defined(HAVE_PRAGMA_WEAK)
#pragma weak mv2_proxy_ibv_ack_async_event = t_mv2_proxy_ibv_ack_async_event
#elif defined(HAVE_PRAGMA_HP_SEC_DEF)
#pragma _HP_SECONDARY_DEF t_mv2_proxy_ibv_ack_async_event  mv2_proxy_ibv_ack_async_event
#elif defined(HAVE_PRAGMA_CRI_DUP)
#pragma _CRI duplicate mv2_proxy_ibv_ack_async_event as t_mv2_proxy_ibv_ack_async_event
#elif defined(HAVE_WEAK_ATTRIBUTE)
void mv2_proxy_ibv_ack_async_event(struct ibv_async_event *event)
                  __attribute__((weak,alias("t_mv2_proxy_ibv_ack_async_event")));
#endif
/* -- End Profiling Symbol Block */

/* Define MPICH_MPI_FROM_PMPI if weak symbols are not supported to build
 the MPI routines */
#ifndef MPICH_MPI_FROM_PMPI
#undef mv2_proxy_ibv_ack_async_event
#define mv2_proxy_ibv_ack_async_event t_mv2_proxy_ibv_ack_async_event

#undef FUNCNAME
#define FUNCNAME mv2_proxy_ibv_ack_async_event
#undef FCNAME
#define FCNAME MPL_QUOTE(FUNCNAME)

void mv2_proxy_ibv_ack_async_event(struct ibv_async_event *event)
{
    ibv_ack_async_event(event);
    return;
}

#endif

struct ibv_pd* mv2_proxy_ibv_alloc_pd(struct ibv_context *context);
#if defined(HAVE_PRAGMA_WEAK)
#pragma weak mv2_proxy_ibv_alloc_pd = t_mv2_proxy_ibv_alloc_pd
#elif defined(HAVE_PRAGMA_HP_SEC_DEF)
#pragma _HP_SECONDARY_DEF t_mv2_proxy_ibv_alloc_pd  mv2_proxy_ibv_alloc_pd
#elif defined(HAVE_PRAGMA_CRI_DUP)
#pragma _CRI duplicate mv2_proxy_ibv_alloc_pd as t_mv2_proxy_ibv_alloc_pd
#elif defined(HAVE_WEAK_ATTRIBUTE)
struct ibv_pd* mv2_proxy_ibv_alloc_pd(struct ibv_context *context)
                  __attribute__((weak,alias("t_mv2_proxy_ibv_alloc_pd")));
#endif
/* -- End Profiling Symbol Block */

/* Define MPICH_MPI_FROM_PMPI if weak symbols are not supported to build
 the MPI routines */
#ifndef MPICH_MPI_FROM_PMPI
#undef mv2_proxy_ibv_alloc_pd
#define mv2_proxy_ibv_alloc_pd t_mv2_proxy_ibv_alloc_pd

#undef FUNCNAME
#define FUNCNAME mv2_proxy_ibv_alloc_pd
#undef FCNAME
#define FCNAME MPL_QUOTE(FUNCNAME)

struct ibv_pd* mv2_proxy_ibv_alloc_pd(struct ibv_context *context)
{
    struct ibv_pd* rv = ibv_alloc_pd(context);
    return rv;
}

#endif

int mv2_proxy_ibv_attach_mcast(struct ibv_qp *qp, const union ibv_gid *gid, uint16_t lid);
#if defined(HAVE_PRAGMA_WEAK)
#pragma weak mv2_proxy_ibv_attach_mcast = t_mv2_proxy_ibv_attach_mcast
#elif defined(HAVE_PRAGMA_HP_SEC_DEF)
#pragma _HP_SECONDARY_DEF t_mv2_proxy_ibv_attach_mcast  mv2_proxy_ibv_attach_mcast
#elif defined(HAVE_PRAGMA_CRI_DUP)
#pragma _CRI duplicate mv2_proxy_ibv_attach_mcast as t_mv2_proxy_ibv_attach_mcast
#elif defined(HAVE_WEAK_ATTRIBUTE)
int mv2_proxy_ibv_attach_mcast(struct ibv_qp *qp, const union ibv_gid *gid, uint16_t lid)
                  __attribute__((weak,alias("t_mv2_proxy_ibv_attach_mcast")));
#endif
/* -- End Profiling Symbol Block */

/* Define MPICH_MPI_FROM_PMPI if weak symbols are not supported to build
 the MPI routines */
#ifndef MPICH_MPI_FROM_PMPI
#undef mv2_proxy_ibv_attach_mcast
#define mv2_proxy_ibv_attach_mcast t_mv2_proxy_ibv_attach_mcast

#undef FUNCNAME
#define FUNCNAME mv2_proxy_ibv_attach_mcast
#undef FCNAME
#define FCNAME MPL_QUOTE(FUNCNAME)

int mv2_proxy_ibv_attach_mcast(struct ibv_qp *qp, const union ibv_gid *gid, uint16_t lid)
{
    int rv = ibv_attach_mcast(qp,gid,lid);
    return rv;
}

#endif

int mv2_proxy_ibv_close_device(struct ibv_context *context);
#if defined(HAVE_PRAGMA_WEAK)
#pragma weak mv2_proxy_ibv_close_device = t_mv2_proxy_ibv_close_device
#elif defined(HAVE_PRAGMA_HP_SEC_DEF)
#pragma _HP_SECONDARY_DEF t_mv2_proxy_ibv_close_device  mv2_proxy_ibv_close_device
#elif defined(HAVE_PRAGMA_CRI_DUP)
#pragma _CRI duplicate mv2_proxy_ibv_close_device as t_mv2_proxy_ibv_close_device
#elif defined(HAVE_WEAK_ATTRIBUTE)
int mv2_proxy_ibv_close_device(struct ibv_context *context)
                  __attribute__((weak,alias("t_mv2_proxy_ibv_close_device")));
#endif
/* -- End Profiling Symbol Block */

/* Define MPICH_MPI_FROM_PMPI if weak symbols are not supported to build
 the MPI routines */
#ifndef MPICH_MPI_FROM_PMPI
#undef mv2_proxy_ibv_close_device
#define mv2_proxy_ibv_close_device t_mv2_proxy_ibv_close_device

#undef FUNCNAME
#define FUNCNAME mv2_proxy_ibv_close_device
#undef FCNAME
#define FCNAME MPL_QUOTE(FUNCNAME)

int mv2_proxy_ibv_close_device(struct ibv_context *context)
{
    int rv = ibv_close_device(context);
    return rv;
}

#endif

struct ibv_ah* mv2_proxy_ibv_create_ah(struct ibv_pd *pd, struct ibv_ah_attr *attr);
#if defined(HAVE_PRAGMA_WEAK)
#pragma weak mv2_proxy_ibv_create_ah = t_mv2_proxy_ibv_create_ah
#elif defined(HAVE_PRAGMA_HP_SEC_DEF)
#pragma _HP_SECONDARY_DEF t_mv2_proxy_ibv_create_ah  mv2_proxy_ibv_create_ah
#elif defined(HAVE_PRAGMA_CRI_DUP)
#pragma _CRI duplicate mv2_proxy_ibv_create_ah as t_mv2_proxy_ibv_create_ah
#elif defined(HAVE_WEAK_ATTRIBUTE)
struct ibv_ah* mv2_proxy_ibv_create_ah(struct ibv_pd *pd, struct ibv_ah_attr *attr)
                  __attribute__((weak,alias("t_mv2_proxy_ibv_create_ah")));
#endif
/* -- End Profiling Symbol Block */

/* Define MPICH_MPI_FROM_PMPI if weak symbols are not supported to build
 the MPI routines */
#ifndef MPICH_MPI_FROM_PMPI
#undef mv2_proxy_ibv_create_ah
#define mv2_proxy_ibv_create_ah t_mv2_proxy_ibv_create_ah

#undef FUNCNAME
#define FUNCNAME mv2_proxy_ibv_create_ah
#undef FCNAME
#define FCNAME MPL_QUOTE(FUNCNAME)

struct ibv_ah* mv2_proxy_ibv_create_ah(struct ibv_pd *pd, struct ibv_ah_attr *attr)
{
    struct ibv_ah* rv = ibv_create_ah(pd,attr);
    return rv;
}

#endif

struct ibv_comp_channel* mv2_proxy_ibv_create_comp_channel(struct ibv_context *context);
#if defined(HAVE_PRAGMA_WEAK)
#pragma weak mv2_proxy_ibv_create_comp_channel = t_mv2_proxy_ibv_create_comp_channel
#elif defined(HAVE_PRAGMA_HP_SEC_DEF)
#pragma _HP_SECONDARY_DEF t_mv2_proxy_ibv_create_comp_channel  mv2_proxy_ibv_create_comp_channel
#elif defined(HAVE_PRAGMA_CRI_DUP)
#pragma _CRI duplicate mv2_proxy_ibv_create_comp_channel as t_mv2_proxy_ibv_create_comp_channel
#elif defined(HAVE_WEAK_ATTRIBUTE)
struct ibv_comp_channel* mv2_proxy_ibv_create_comp_channel(struct ibv_context *context)
                  __attribute__((weak,alias("t_mv2_proxy_ibv_create_comp_channel")));
#endif
/* -- End Profiling Symbol Block */

/* Define MPICH_MPI_FROM_PMPI if weak symbols are not supported to build
 the MPI routines */
#ifndef MPICH_MPI_FROM_PMPI
#undef mv2_proxy_ibv_create_comp_channel
#define mv2_proxy_ibv_create_comp_channel t_mv2_proxy_ibv_create_comp_channel

#undef FUNCNAME
#define FUNCNAME mv2_proxy_ibv_create_comp_channel
#undef FCNAME
#define FCNAME MPL_QUOTE(FUNCNAME)

struct ibv_comp_channel* mv2_proxy_ibv_create_comp_channel(struct ibv_context *context)
{
    struct ibv_comp_channel* rv = ibv_create_comp_channel(context);
    return rv;
}

#endif

struct ibv_cq* mv2_proxy_ibv_create_cq(struct ibv_context *context, int cqe,void *cq_context,struct ibv_comp_channel *channel,int comp_vector);
#if defined(HAVE_PRAGMA_WEAK)
#pragma weak mv2_proxy_ibv_create_cq = t_mv2_proxy_ibv_create_cq
#elif defined(HAVE_PRAGMA_HP_SEC_DEF)
#pragma _HP_SECONDARY_DEF t_mv2_proxy_ibv_create_cq  mv2_proxy_ibv_create_cq
#elif defined(HAVE_PRAGMA_CRI_DUP)
#pragma _CRI duplicate mv2_proxy_ibv_create_cq as t_mv2_proxy_ibv_create_cq
#elif defined(HAVE_WEAK_ATTRIBUTE)
struct ibv_cq* mv2_proxy_ibv_create_cq(struct ibv_context *context, int cqe,void *cq_context,struct ibv_comp_channel *channel,int comp_vector)
                  __attribute__((weak,alias("t_mv2_proxy_ibv_create_cq")));
#endif
/* -- End Profiling Symbol Block */

/* Define MPICH_MPI_FROM_PMPI if weak symbols are not supported to build
 the MPI routines */
#ifndef MPICH_MPI_FROM_PMPI
#undef mv2_proxy_ibv_create_cq
#define mv2_proxy_ibv_create_cq t_mv2_proxy_ibv_create_cq

#undef FUNCNAME
#define FUNCNAME mv2_proxy_ibv_create_cq
#undef FCNAME
#define FCNAME MPL_QUOTE(FUNCNAME)

struct ibv_cq* mv2_proxy_ibv_create_cq(struct ibv_context *context, int cqe,void *cq_context,struct ibv_comp_channel *channel,int comp_vector)
{
    struct ibv_cq* rv = ibv_create_cq(context,cqe,cq_context,channel,comp_vector);
    return rv;
}

#endif

struct ibv_srq* mv2_proxy_ibv_create_srq(struct ibv_pd *pd,struct ibv_srq_init_attr *srq_init_attr);
#if defined(HAVE_PRAGMA_WEAK)
#pragma weak mv2_proxy_ibv_create_srq = t_mv2_proxy_ibv_create_srq
#elif defined(HAVE_PRAGMA_HP_SEC_DEF)
#pragma _HP_SECONDARY_DEF t_mv2_proxy_ibv_create_srq  mv2_proxy_ibv_create_srq
#elif defined(HAVE_PRAGMA_CRI_DUP)
#pragma _CRI duplicate mv2_proxy_ibv_create_srq as t_mv2_proxy_ibv_create_srq
#elif defined(HAVE_WEAK_ATTRIBUTE)
struct ibv_srq* mv2_proxy_ibv_create_srq(struct ibv_pd *pd,struct ibv_srq_init_attr *srq_init_attr)
                  __attribute__((weak,alias("t_mv2_proxy_ibv_create_srq")));
#endif
/* -- End Profiling Symbol Block */

/* Define MPICH_MPI_FROM_PMPI if weak symbols are not supported to build
 the MPI routines */
#ifndef MPICH_MPI_FROM_PMPI
#undef mv2_proxy_ibv_create_srq
#define mv2_proxy_ibv_create_srq t_mv2_proxy_ibv_create_srq

#undef FUNCNAME
#define FUNCNAME mv2_proxy_ibv_create_srq
#undef FCNAME
#define FCNAME MPL_QUOTE(FUNCNAME)

struct ibv_srq* mv2_proxy_ibv_create_srq(struct ibv_pd *pd,struct ibv_srq_init_attr *srq_init_attr)
{
    struct ibv_srq* rv = ibv_create_srq(pd,srq_init_attr);
    return rv;
}

#endif

struct ibv_qp* mv2_proxy_ibv_create_qp(struct ibv_pd *pd,struct ibv_qp_init_attr *qp_init_attr);
#if defined(HAVE_PRAGMA_WEAK)
#pragma weak mv2_proxy_ibv_create_qp = t_mv2_proxy_ibv_create_qp
#elif defined(HAVE_PRAGMA_HP_SEC_DEF)
#pragma _HP_SECONDARY_DEF t_mv2_proxy_ibv_create_qp  mv2_proxy_ibv_create_qp
#elif defined(HAVE_PRAGMA_CRI_DUP)
#pragma _CRI duplicate mv2_proxy_ibv_create_qp as t_mv2_proxy_ibv_create_qp
#elif defined(HAVE_WEAK_ATTRIBUTE)
struct ibv_qp* mv2_proxy_ibv_create_qp(struct ibv_pd *pd,struct ibv_qp_init_attr *qp_init_attr)
                  __attribute__((weak,alias("t_mv2_proxy_ibv_create_qp")));
#endif
/* -- End Profiling Symbol Block */

/* Define MPICH_MPI_FROM_PMPI if weak symbols are not supported to build
 the MPI routines */
#ifndef MPICH_MPI_FROM_PMPI
#undef mv2_proxy_ibv_create_qp
#define mv2_proxy_ibv_create_qp t_mv2_proxy_ibv_create_qp

#undef FUNCNAME
#define FUNCNAME mv2_proxy_ibv_create_qp
#undef FCNAME
#define FCNAME MPL_QUOTE(FUNCNAME)

struct ibv_qp* mv2_proxy_ibv_create_qp(struct ibv_pd *pd,struct ibv_qp_init_attr *qp_init_attr)
{
    struct ibv_qp* rv = ibv_create_qp(pd,qp_init_attr);
    return rv;
}

#endif

int mv2_proxy_ibv_dealloc_pd(struct ibv_pd *pd);
#if defined(HAVE_PRAGMA_WEAK)
#pragma weak mv2_proxy_ibv_dealloc_pd = t_mv2_proxy_ibv_dealloc_pd
#elif defined(HAVE_PRAGMA_HP_SEC_DEF)
#pragma _HP_SECONDARY_DEF t_mv2_proxy_ibv_dealloc_pd  mv2_proxy_ibv_dealloc_pd
#elif defined(HAVE_PRAGMA_CRI_DUP)
#pragma _CRI duplicate mv2_proxy_ibv_dealloc_pd as t_mv2_proxy_ibv_dealloc_pd
#elif defined(HAVE_WEAK_ATTRIBUTE)
int mv2_proxy_ibv_dealloc_pd(struct ibv_pd *pd)
                  __attribute__((weak,alias("t_mv2_proxy_ibv_dealloc_pd")));
#endif
/* -- End Profiling Symbol Block */

/* Define MPICH_MPI_FROM_PMPI if weak symbols are not supported to build
 the MPI routines */
#ifndef MPICH_MPI_FROM_PMPI
#undef mv2_proxy_ibv_dealloc_pd
#define mv2_proxy_ibv_dealloc_pd t_mv2_proxy_ibv_dealloc_pd

#undef FUNCNAME
#define FUNCNAME mv2_proxy_ibv_dealloc_pd
#undef FCNAME
#define FCNAME MPL_QUOTE(FUNCNAME)

int mv2_proxy_ibv_dealloc_pd(struct ibv_pd *pd)
{
    int rv = ibv_dealloc_pd(pd);
    return rv;
}

#endif

int mv2_proxy_ibv_dereg_mr(struct ibv_mr *mr);
#if defined(HAVE_PRAGMA_WEAK)
#pragma weak mv2_proxy_ibv_dereg_mr = t_mv2_proxy_ibv_dereg_mr
#elif defined(HAVE_PRAGMA_HP_SEC_DEF)
#pragma _HP_SECONDARY_DEF t_mv2_proxy_ibv_dereg_mr  mv2_proxy_ibv_dereg_mr
#elif defined(HAVE_PRAGMA_CRI_DUP)
#pragma _CRI duplicate mv2_proxy_ibv_dereg_mr as t_mv2_proxy_ibv_dereg_mr
#elif defined(HAVE_WEAK_ATTRIBUTE)
int mv2_proxy_ibv_dereg_mr(struct ibv_mr *mr)
                  __attribute__((weak,alias("t_mv2_proxy_ibv_dereg_mr")));
#endif
/* -- End Profiling Symbol Block */

/* Define MPICH_MPI_FROM_PMPI if weak symbols are not supported to build
 the MPI routines */
#ifndef MPICH_MPI_FROM_PMPI
#undef mv2_proxy_ibv_dereg_mr
#define mv2_proxy_ibv_dereg_mr t_mv2_proxy_ibv_dereg_mr

#undef FUNCNAME
#define FUNCNAME mv2_proxy_ibv_dereg_mr
#undef FCNAME
#define FCNAME MPL_QUOTE(FUNCNAME)

int mv2_proxy_ibv_dereg_mr(struct ibv_mr *mr)
{
    int rv = ibv_dereg_mr(mr);
    return rv;
}

#endif

int mv2_proxy_ibv_destroy_ah(struct ibv_ah *ah);
#if defined(HAVE_PRAGMA_WEAK)
#pragma weak mv2_proxy_ibv_destroy_ah = t_mv2_proxy_ibv_destroy_ah
#elif defined(HAVE_PRAGMA_HP_SEC_DEF)
#pragma _HP_SECONDARY_DEF t_mv2_proxy_ibv_destroy_ah  mv2_proxy_ibv_destroy_ah
#elif defined(HAVE_PRAGMA_CRI_DUP)
#pragma _CRI duplicate mv2_proxy_ibv_destroy_ah as t_mv2_proxy_ibv_destroy_ah
#elif defined(HAVE_WEAK_ATTRIBUTE)
int mv2_proxy_ibv_destroy_ah(struct ibv_ah *ah)
                  __attribute__((weak,alias("t_mv2_proxy_ibv_destroy_ah")));
#endif
/* -- End Profiling Symbol Block */

/* Define MPICH_MPI_FROM_PMPI if weak symbols are not supported to build
 the MPI routines */
#ifndef MPICH_MPI_FROM_PMPI
#undef mv2_proxy_ibv_destroy_ah
#define mv2_proxy_ibv_destroy_ah t_mv2_proxy_ibv_destroy_ah

#undef FUNCNAME
#define FUNCNAME mv2_proxy_ibv_destroy_ah
#undef FCNAME
#define FCNAME MPL_QUOTE(FUNCNAME)

int mv2_proxy_ibv_destroy_ah(struct ibv_ah *ah)
{
    int rv = ibv_destroy_ah(ah);
    return rv;
}

#endif

int mv2_proxy_ibv_destroy_comp_channel(struct ibv_comp_channel *channel);
#if defined(HAVE_PRAGMA_WEAK)
#pragma weak mv2_proxy_ibv_destroy_comp_channel = t_mv2_proxy_ibv_destroy_comp_channel
#elif defined(HAVE_PRAGMA_HP_SEC_DEF)
#pragma _HP_SECONDARY_DEF t_mv2_proxy_ibv_destroy_comp_channel  mv2_proxy_ibv_destroy_comp_channel
#elif defined(HAVE_PRAGMA_CRI_DUP)
#pragma _CRI duplicate mv2_proxy_ibv_destroy_comp_channel as t_mv2_proxy_ibv_destroy_comp_channel
#elif defined(HAVE_WEAK_ATTRIBUTE)
int mv2_proxy_ibv_destroy_comp_channel(struct ibv_comp_channel *channel)
                  __attribute__((weak,alias("t_mv2_proxy_ibv_destroy_comp_channel")));
#endif
/* -- End Profiling Symbol Block */

/* Define MPICH_MPI_FROM_PMPI if weak symbols are not supported to build
 the MPI routines */
#ifndef MPICH_MPI_FROM_PMPI
#undef mv2_proxy_ibv_destroy_comp_channel
#define mv2_proxy_ibv_destroy_comp_channel t_mv2_proxy_ibv_destroy_comp_channel

#undef FUNCNAME
#define FUNCNAME mv2_proxy_ibv_destroy_comp_channel
#undef FCNAME
#define FCNAME MPL_QUOTE(FUNCNAME)

int mv2_proxy_ibv_destroy_comp_channel(struct ibv_comp_channel *channel)
{
    int rv = ibv_destroy_comp_channel(channel);
    return rv;
}

#endif

int mv2_proxy_ibv_destroy_cq(struct ibv_cq *cq);
#if defined(HAVE_PRAGMA_WEAK)
#pragma weak mv2_proxy_ibv_destroy_cq = t_mv2_proxy_ibv_destroy_cq
#elif defined(HAVE_PRAGMA_HP_SEC_DEF)
#pragma _HP_SECONDARY_DEF t_mv2_proxy_ibv_destroy_cq  mv2_proxy_ibv_destroy_cq
#elif defined(HAVE_PRAGMA_CRI_DUP)
#pragma _CRI duplicate mv2_proxy_ibv_destroy_cq as t_mv2_proxy_ibv_destroy_cq
#elif defined(HAVE_WEAK_ATTRIBUTE)
int mv2_proxy_ibv_destroy_cq(struct ibv_cq *cq)
                  __attribute__((weak,alias("t_mv2_proxy_ibv_destroy_cq")));
#endif
/* -- End Profiling Symbol Block */

/* Define MPICH_MPI_FROM_PMPI if weak symbols are not supported to build
 the MPI routines */
#ifndef MPICH_MPI_FROM_PMPI
#undef mv2_proxy_ibv_destroy_cq
#define mv2_proxy_ibv_destroy_cq t_mv2_proxy_ibv_destroy_cq

#undef FUNCNAME
#define FUNCNAME mv2_proxy_ibv_destroy_cq
#undef FCNAME
#define FCNAME MPL_QUOTE(FUNCNAME)

int mv2_proxy_ibv_destroy_cq(struct ibv_cq *cq)
{
    int rv = ibv_destroy_cq(cq);
    return rv;
}

#endif

int mv2_proxy_ibv_destroy_qp(struct ibv_qp *qp);
#if defined(HAVE_PRAGMA_WEAK)
#pragma weak mv2_proxy_ibv_destroy_qp = t_mv2_proxy_ibv_destroy_qp
#elif defined(HAVE_PRAGMA_HP_SEC_DEF)
#pragma _HP_SECONDARY_DEF t_mv2_proxy_ibv_destroy_qp  mv2_proxy_ibv_destroy_qp
#elif defined(HAVE_PRAGMA_CRI_DUP)
#pragma _CRI duplicate mv2_proxy_ibv_destroy_qp as t_mv2_proxy_ibv_destroy_qp
#elif defined(HAVE_WEAK_ATTRIBUTE)
int mv2_proxy_ibv_destroy_qp(struct ibv_qp *qp)
                  __attribute__((weak,alias("t_mv2_proxy_ibv_destroy_qp")));
#endif
/* -- End Profiling Symbol Block */

/* Define MPICH_MPI_FROM_PMPI if weak symbols are not supported to build
 the MPI routines */
#ifndef MPICH_MPI_FROM_PMPI
#undef mv2_proxy_ibv_destroy_qp
#define mv2_proxy_ibv_destroy_qp t_mv2_proxy_ibv_destroy_qp

#undef FUNCNAME
#define FUNCNAME mv2_proxy_ibv_destroy_qp
#undef FCNAME
#define FCNAME MPL_QUOTE(FUNCNAME)

int mv2_proxy_ibv_destroy_qp(struct ibv_qp *qp)
{
    int rv = ibv_destroy_qp(qp);
    return rv;
}

#endif

int mv2_proxy_ibv_detach_mcast(struct ibv_qp *qp, const union ibv_gid *gid, uint16_t lid);
#if defined(HAVE_PRAGMA_WEAK)
#pragma weak mv2_proxy_ibv_detach_mcast = t_mv2_proxy_ibv_detach_mcast
#elif defined(HAVE_PRAGMA_HP_SEC_DEF)
#pragma _HP_SECONDARY_DEF t_mv2_proxy_ibv_detach_mcast  mv2_proxy_ibv_detach_mcast
#elif defined(HAVE_PRAGMA_CRI_DUP)
#pragma _CRI duplicate mv2_proxy_ibv_detach_mcast as t_mv2_proxy_ibv_detach_mcast
#elif defined(HAVE_WEAK_ATTRIBUTE)
int mv2_proxy_ibv_detach_mcast(struct ibv_qp *qp, const union ibv_gid *gid, uint16_t lid)
                  __attribute__((weak,alias("t_mv2_proxy_ibv_detach_mcast")));
#endif
/* -- End Profiling Symbol Block */

/* Define MPICH_MPI_FROM_PMPI if weak symbols are not supported to build
 the MPI routines */
#ifndef MPICH_MPI_FROM_PMPI
#undef mv2_proxy_ibv_detach_mcast
#define mv2_proxy_ibv_detach_mcast t_mv2_proxy_ibv_detach_mcast

#undef FUNCNAME
#define FUNCNAME mv2_proxy_ibv_detach_mcast
#undef FCNAME
#define FCNAME MPL_QUOTE(FUNCNAME)

int mv2_proxy_ibv_detach_mcast(struct ibv_qp *qp, const union ibv_gid *gid, uint16_t lid)
{
    int rv = ibv_detach_mcast(qp,gid,lid);
    return rv;
}

#endif

const char* mv2_proxy_ibv_event_type_str(enum ibv_event_type event);
#if defined(HAVE_PRAGMA_WEAK)
#pragma weak mv2_proxy_ibv_event_type_str = t_mv2_proxy_ibv_event_type_str
#elif defined(HAVE_PRAGMA_HP_SEC_DEF)
#pragma _HP_SECONDARY_DEF t_mv2_proxy_ibv_event_type_str  mv2_proxy_ibv_event_type_str
#elif defined(HAVE_PRAGMA_CRI_DUP)
#pragma _CRI duplicate mv2_proxy_ibv_event_type_str as t_mv2_proxy_ibv_event_type_str
#elif defined(HAVE_WEAK_ATTRIBUTE)
const char* mv2_proxy_ibv_event_type_str(enum ibv_event_type event)
                  __attribute__((weak,alias("t_mv2_proxy_ibv_event_type_str")));
#endif
/* -- End Profiling Symbol Block */

/* Define MPICH_MPI_FROM_PMPI if weak symbols are not supported to build
 the MPI routines */
#ifndef MPICH_MPI_FROM_PMPI
#undef mv2_proxy_ibv_event_type_str
#define mv2_proxy_ibv_event_type_str t_mv2_proxy_ibv_event_type_str

#undef FUNCNAME
#define FUNCNAME mv2_proxy_ibv_event_type_str
#undef FCNAME
#define FCNAME MPL_QUOTE(FUNCNAME)

const char* mv2_proxy_ibv_event_type_str(enum ibv_event_type event)
{
    const char* rv = ibv_event_type_str(event);
    return rv;
}

#endif

void mv2_proxy_ibv_free_device_list(struct ibv_device **list);
#if defined(HAVE_PRAGMA_WEAK)
#pragma weak mv2_proxy_ibv_free_device_list = t_mv2_proxy_ibv_free_device_list
#elif defined(HAVE_PRAGMA_HP_SEC_DEF)
#pragma _HP_SECONDARY_DEF t_mv2_proxy_ibv_free_device_list  mv2_proxy_ibv_free_device_list
#elif defined(HAVE_PRAGMA_CRI_DUP)
#pragma _CRI duplicate mv2_proxy_ibv_free_device_list as t_mv2_proxy_ibv_free_device_list
#elif defined(HAVE_WEAK_ATTRIBUTE)
void mv2_proxy_ibv_free_device_list(struct ibv_device **list)
                  __attribute__((weak,alias("t_mv2_proxy_ibv_free_device_list")));
#endif
/* -- End Profiling Symbol Block */

/* Define MPICH_MPI_FROM_PMPI if weak symbols are not supported to build
 the MPI routines */
#ifndef MPICH_MPI_FROM_PMPI
#undef mv2_proxy_ibv_free_device_list
#define mv2_proxy_ibv_free_device_list t_mv2_proxy_ibv_free_device_list

#undef FUNCNAME
#define FUNCNAME mv2_proxy_ibv_free_device_list
#undef FCNAME
#define FCNAME MPL_QUOTE(FUNCNAME)

void mv2_proxy_ibv_free_device_list(struct ibv_device **list)
{
    ibv_free_device_list(list);
    return;
}

#endif

int mv2_proxy_ibv_get_async_event(struct ibv_context *context,struct ibv_async_event *event);
#if defined(HAVE_PRAGMA_WEAK)
#pragma weak mv2_proxy_ibv_get_async_event = t_mv2_proxy_ibv_get_async_event
#elif defined(HAVE_PRAGMA_HP_SEC_DEF)
#pragma _HP_SECONDARY_DEF t_mv2_proxy_ibv_get_async_event  mv2_proxy_ibv_get_async_event
#elif defined(HAVE_PRAGMA_CRI_DUP)
#pragma _CRI duplicate mv2_proxy_ibv_get_async_event as t_mv2_proxy_ibv_get_async_event
#elif defined(HAVE_WEAK_ATTRIBUTE)
int mv2_proxy_ibv_get_async_event(struct ibv_context *context,struct ibv_async_event *event)
                  __attribute__((weak,alias("t_mv2_proxy_ibv_get_async_event")));
#endif
/* -- End Profiling Symbol Block */

/* Define MPICH_MPI_FROM_PMPI if weak symbols are not supported to build
 the MPI routines */
#ifndef MPICH_MPI_FROM_PMPI
#undef mv2_proxy_ibv_get_async_event
#define mv2_proxy_ibv_get_async_event t_mv2_proxy_ibv_get_async_event

#undef FUNCNAME
#define FUNCNAME mv2_proxy_ibv_get_async_event
#undef FCNAME
#define FCNAME MPL_QUOTE(FUNCNAME)

int mv2_proxy_ibv_get_async_event(struct ibv_context *context,struct ibv_async_event *event)
{
    int rv = ibv_get_async_event(context,event);
    return rv;
}

#endif

struct ibv_device** mv2_proxy_ibv_get_device_list(int *num_devices);
#if defined(HAVE_PRAGMA_WEAK)
#pragma weak mv2_proxy_ibv_get_device_list = t_mv2_proxy_ibv_get_device_list
#elif defined(HAVE_PRAGMA_HP_SEC_DEF)
#pragma _HP_SECONDARY_DEF t_mv2_proxy_ibv_get_device_list  mv2_proxy_ibv_get_device_list
#elif defined(HAVE_PRAGMA_CRI_DUP)
#pragma _CRI duplicate mv2_proxy_ibv_get_device_list as t_mv2_proxy_ibv_get_device_list
#elif defined(HAVE_WEAK_ATTRIBUTE)
struct ibv_device** mv2_proxy_ibv_get_device_list(int *num_devices)
                  __attribute__((weak,alias("t_mv2_proxy_ibv_get_device_list")));
#endif
/* -- End Profiling Symbol Block */

/* Define MPICH_MPI_FROM_PMPI if weak symbols are not supported to build
 the MPI routines */
#ifndef MPICH_MPI_FROM_PMPI
#undef mv2_proxy_ibv_get_device_list
#define mv2_proxy_ibv_get_device_list t_mv2_proxy_ibv_get_device_list

#undef FUNCNAME
#define FUNCNAME mv2_proxy_ibv_get_device_list
#undef FCNAME
#define FCNAME MPL_QUOTE(FUNCNAME)

struct ibv_device** mv2_proxy_ibv_get_device_list(int *num_devices)
{
    struct ibv_device** rv = ibv_get_device_list(num_devices);
    return rv;
}

#endif

const char* mv2_proxy_ibv_get_device_name(struct ibv_device *device);
#if defined(HAVE_PRAGMA_WEAK)
#pragma weak mv2_proxy_ibv_get_device_name = t_mv2_proxy_ibv_get_device_name
#elif defined(HAVE_PRAGMA_HP_SEC_DEF)
#pragma _HP_SECONDARY_DEF t_mv2_proxy_ibv_get_device_name  mv2_proxy_ibv_get_device_name
#elif defined(HAVE_PRAGMA_CRI_DUP)
#pragma _CRI duplicate mv2_proxy_ibv_get_device_name as t_mv2_proxy_ibv_get_device_name
#elif defined(HAVE_WEAK_ATTRIBUTE)
const char* mv2_proxy_ibv_get_device_name(struct ibv_device *device)
                  __attribute__((weak,alias("t_mv2_proxy_ibv_get_device_name")));
#endif
/* -- End Profiling Symbol Block */

/* Define MPICH_MPI_FROM_PMPI if weak symbols are not supported to build
 the MPI routines */
#ifndef MPICH_MPI_FROM_PMPI
#undef mv2_proxy_ibv_get_device_name
#define mv2_proxy_ibv_get_device_name t_mv2_proxy_ibv_get_device_name

#undef FUNCNAME
#define FUNCNAME mv2_proxy_ibv_get_device_name
#undef FCNAME
#define FCNAME MPL_QUOTE(FUNCNAME)

const char* mv2_proxy_ibv_get_device_name(struct ibv_device *device)
{
    const char* rv = ibv_get_device_name(device);
    return rv;
}

#endif

int mv2_proxy_ibv_get_cq_event(struct ibv_comp_channel *channel,struct ibv_cq **cq, void **cq_context);
#if defined(HAVE_PRAGMA_WEAK)
#pragma weak mv2_proxy_ibv_get_cq_event = t_mv2_proxy_ibv_get_cq_event
#elif defined(HAVE_PRAGMA_HP_SEC_DEF)
#pragma _HP_SECONDARY_DEF t_mv2_proxy_ibv_get_cq_event  mv2_proxy_ibv_get_cq_event
#elif defined(HAVE_PRAGMA_CRI_DUP)
#pragma _CRI duplicate mv2_proxy_ibv_get_cq_event as t_mv2_proxy_ibv_get_cq_event
#elif defined(HAVE_WEAK_ATTRIBUTE)
int mv2_proxy_ibv_get_cq_event(struct ibv_comp_channel *channel,struct ibv_cq **cq, void **cq_context)
                  __attribute__((weak,alias("t_mv2_proxy_ibv_get_cq_event")));
#endif
/* -- End Profiling Symbol Block */

/* Define MPICH_MPI_FROM_PMPI if weak symbols are not supported to build
 the MPI routines */
#ifndef MPICH_MPI_FROM_PMPI
#undef mv2_proxy_ibv_get_cq_event
#define mv2_proxy_ibv_get_cq_event t_mv2_proxy_ibv_get_cq_event

#undef FUNCNAME
#define FUNCNAME mv2_proxy_ibv_get_cq_event
#undef FCNAME
#define FCNAME MPL_QUOTE(FUNCNAME)

int mv2_proxy_ibv_get_cq_event(struct ibv_comp_channel *channel,struct ibv_cq **cq, void **cq_context)
{
    int rv = ibv_get_cq_event(channel,cq,cq_context);
    return rv;
}

#endif

int mv2_proxy_ibv_modify_qp(struct ibv_qp *qp, struct ibv_qp_attr *attr,int attr_mask);
#if defined(HAVE_PRAGMA_WEAK)
#pragma weak mv2_proxy_ibv_modify_qp = t_mv2_proxy_ibv_modify_qp
#elif defined(HAVE_PRAGMA_HP_SEC_DEF)
#pragma _HP_SECONDARY_DEF t_mv2_proxy_ibv_modify_qp  mv2_proxy_ibv_modify_qp
#elif defined(HAVE_PRAGMA_CRI_DUP)
#pragma _CRI duplicate mv2_proxy_ibv_modify_qp as t_mv2_proxy_ibv_modify_qp
#elif defined(HAVE_WEAK_ATTRIBUTE)
int mv2_proxy_ibv_modify_qp(struct ibv_qp *qp, struct ibv_qp_attr *attr,int attr_mask)
                  __attribute__((weak,alias("t_mv2_proxy_ibv_modify_qp")));
#endif
/* -- End Profiling Symbol Block */

/* Define MPICH_MPI_FROM_PMPI if weak symbols are not supported to build
 the MPI routines */
#ifndef MPICH_MPI_FROM_PMPI
#undef mv2_proxy_ibv_modify_qp
#define mv2_proxy_ibv_modify_qp t_mv2_proxy_ibv_modify_qp

#undef FUNCNAME
#define FUNCNAME mv2_proxy_ibv_modify_qp
#undef FCNAME
#define FCNAME MPL_QUOTE(FUNCNAME)

int mv2_proxy_ibv_modify_qp(struct ibv_qp *qp, struct ibv_qp_attr *attr,int attr_mask)
{
    int rv = ibv_modify_qp(qp,attr,attr_mask);
    return rv;
}

#endif

int mv2_proxy_ibv_modify_srq(struct ibv_srq *srq,struct ibv_srq_attr *srq_attr,int srq_attr_mask);
#if defined(HAVE_PRAGMA_WEAK)
#pragma weak mv2_proxy_ibv_modify_srq = t_mv2_proxy_ibv_modify_srq
#elif defined(HAVE_PRAGMA_HP_SEC_DEF)
#pragma _HP_SECONDARY_DEF t_mv2_proxy_ibv_modify_srq  mv2_proxy_ibv_modify_srq
#elif defined(HAVE_PRAGMA_CRI_DUP)
#pragma _CRI duplicate mv2_proxy_ibv_modify_srq as t_mv2_proxy_ibv_modify_srq
#elif defined(HAVE_WEAK_ATTRIBUTE)
int mv2_proxy_ibv_modify_srq(struct ibv_srq *srq,struct ibv_srq_attr *srq_attr,int srq_attr_mask)
                  __attribute__((weak,alias("t_mv2_proxy_ibv_modify_srq")));
#endif
/* -- End Profiling Symbol Block */

/* Define MPICH_MPI_FROM_PMPI if weak symbols are not supported to build
 the MPI routines */
#ifndef MPICH_MPI_FROM_PMPI
#undef mv2_proxy_ibv_modify_srq
#define mv2_proxy_ibv_modify_srq t_mv2_proxy_ibv_modify_srq

#undef FUNCNAME
#define FUNCNAME mv2_proxy_ibv_modify_srq
#undef FCNAME
#define FCNAME MPL_QUOTE(FUNCNAME)

int mv2_proxy_ibv_modify_srq(struct ibv_srq *srq,struct ibv_srq_attr *srq_attr,int srq_attr_mask)
{
    int rv = ibv_modify_srq(srq,srq_attr,srq_attr_mask);
    return rv;
}

#endif

struct ibv_context* mv2_proxy_ibv_open_device(struct ibv_device *device);
#if defined(HAVE_PRAGMA_WEAK)
#pragma weak mv2_proxy_ibv_open_device = t_mv2_proxy_ibv_open_device
#elif defined(HAVE_PRAGMA_HP_SEC_DEF)
#pragma _HP_SECONDARY_DEF t_mv2_proxy_ibv_open_device  mv2_proxy_ibv_open_device
#elif defined(HAVE_PRAGMA_CRI_DUP)
#pragma _CRI duplicate mv2_proxy_ibv_open_device as t_mv2_proxy_ibv_open_device
#elif defined(HAVE_WEAK_ATTRIBUTE)
struct ibv_context* mv2_proxy_ibv_open_device(struct ibv_device *device)
                  __attribute__((weak,alias("t_mv2_proxy_ibv_open_device")));
#endif
/* -- End Profiling Symbol Block */

/* Define MPICH_MPI_FROM_PMPI if weak symbols are not supported to build
 the MPI routines */
#ifndef MPICH_MPI_FROM_PMPI
#undef mv2_proxy_ibv_open_device
#define mv2_proxy_ibv_open_device t_mv2_proxy_ibv_open_device

#undef FUNCNAME
#define FUNCNAME mv2_proxy_ibv_open_device
#undef FCNAME
#define FCNAME MPL_QUOTE(FUNCNAME)

struct ibv_context* mv2_proxy_ibv_open_device(struct ibv_device *device)
{
    struct ibv_context* rv = ibv_open_device(device);
    return rv;
}

#endif

int mv2_proxy_ibv_poll_cq(struct ibv_cq *cq, int num_entries, struct ibv_wc *wc);
#if defined(HAVE_PRAGMA_WEAK)
#pragma weak mv2_proxy_ibv_poll_cq = t_mv2_proxy_ibv_poll_cq
#elif defined(HAVE_PRAGMA_HP_SEC_DEF)
#pragma _HP_SECONDARY_DEF t_mv2_proxy_ibv_poll_cq  mv2_proxy_ibv_poll_cq
#elif defined(HAVE_PRAGMA_CRI_DUP)
#pragma _CRI duplicate mv2_proxy_ibv_poll_cq as t_mv2_proxy_ibv_poll_cq
#elif defined(HAVE_WEAK_ATTRIBUTE)
int mv2_proxy_ibv_poll_cq(struct ibv_cq *cq, int num_entries, struct ibv_wc *wc)
                  __attribute__((weak,alias("t_mv2_proxy_ibv_poll_cq")));
#endif
/* -- End Profiling Symbol Block */

/* Define MPICH_MPI_FROM_PMPI if weak symbols are not supported to build
 the MPI routines */
#ifndef MPICH_MPI_FROM_PMPI
#undef mv2_proxy_ibv_poll_cq
#define mv2_proxy_ibv_poll_cq t_mv2_proxy_ibv_poll_cq

#undef FUNCNAME
#define FUNCNAME mv2_proxy_ibv_poll_cq
#undef FCNAME
#define FCNAME MPL_QUOTE(FUNCNAME)

int mv2_proxy_ibv_poll_cq(struct ibv_cq *cq, int num_entries, struct ibv_wc *wc)
{
    int rv = ibv_poll_cq(cq,num_entries,wc);
    return rv;
}

#endif

int mv2_proxy_ibv_post_send(struct ibv_qp *qp, struct ibv_send_wr *wr, struct ibv_send_wr **bad_wr);
#if defined(HAVE_PRAGMA_WEAK)
#pragma weak mv2_proxy_ibv_post_send = t_mv2_proxy_ibv_post_send
#elif defined(HAVE_PRAGMA_HP_SEC_DEF)
#pragma _HP_SECONDARY_DEF t_mv2_proxy_ibv_post_send  mv2_proxy_ibv_post_send
#elif defined(HAVE_PRAGMA_CRI_DUP)
#pragma _CRI duplicate mv2_proxy_ibv_post_send as t_mv2_proxy_ibv_post_send
#elif defined(HAVE_WEAK_ATTRIBUTE)
int mv2_proxy_ibv_post_send(struct ibv_qp *qp, struct ibv_send_wr *wr, struct ibv_send_wr **bad_wr)
                  __attribute__((weak,alias("t_mv2_proxy_ibv_post_send")));
#endif
/* -- End Profiling Symbol Block */

/* Define MPICH_MPI_FROM_PMPI if weak symbols are not supported to build
 the MPI routines */
#ifndef MPICH_MPI_FROM_PMPI
#undef mv2_proxy_ibv_post_send
#define mv2_proxy_ibv_post_send t_mv2_proxy_ibv_post_send

#undef FUNCNAME
#define FUNCNAME mv2_proxy_ibv_post_send
#undef FCNAME
#define FCNAME MPL_QUOTE(FUNCNAME)

int mv2_proxy_ibv_post_send(struct ibv_qp *qp, struct ibv_send_wr *wr, struct ibv_send_wr **bad_wr)
{
    int rv = ibv_post_send(qp,wr,bad_wr);
    return rv;
}

#endif

int mv2_proxy_ibv_post_recv(struct ibv_qp *qp, struct ibv_recv_wr *wr, struct ibv_recv_wr **bad_wr);
#if defined(HAVE_PRAGMA_WEAK)
#pragma weak mv2_proxy_ibv_post_recv = t_mv2_proxy_ibv_post_recv
#elif defined(HAVE_PRAGMA_HP_SEC_DEF)
#pragma _HP_SECONDARY_DEF t_mv2_proxy_ibv_post_recv  mv2_proxy_ibv_post_recv
#elif defined(HAVE_PRAGMA_CRI_DUP)
#pragma _CRI duplicate mv2_proxy_ibv_post_recv as t_mv2_proxy_ibv_post_recv
#elif defined(HAVE_WEAK_ATTRIBUTE)
int mv2_proxy_ibv_post_recv(struct ibv_qp *qp, struct ibv_recv_wr *wr, struct ibv_recv_wr **bad_wr)
                  __attribute__((weak,alias("t_mv2_proxy_ibv_post_recv")));
#endif
/* -- End Profiling Symbol Block */

/* Define MPICH_MPI_FROM_PMPI if weak symbols are not supported to build
 the MPI routines */
#ifndef MPICH_MPI_FROM_PMPI
#undef mv2_proxy_ibv_post_recv
#define mv2_proxy_ibv_post_recv t_mv2_proxy_ibv_post_recv

#undef FUNCNAME
#define FUNCNAME mv2_proxy_ibv_post_recv
#undef FCNAME
#define FCNAME MPL_QUOTE(FUNCNAME)

int mv2_proxy_ibv_post_recv(struct ibv_qp *qp, struct ibv_recv_wr *wr, struct ibv_recv_wr **bad_wr)
{
    int rv = ibv_post_recv(qp,wr,bad_wr);
    return rv;
}

#endif

int mv2_proxy_ibv_post_srq_recv(struct ibv_srq *srq, struct ibv_recv_wr *recv_wr, struct ibv_recv_wr **bad_recv_wr);
#if defined(HAVE_PRAGMA_WEAK)
#pragma weak mv2_proxy_ibv_post_srq_recv = t_mv2_proxy_ibv_post_srq_recv
#elif defined(HAVE_PRAGMA_HP_SEC_DEF)
#pragma _HP_SECONDARY_DEF t_mv2_proxy_ibv_post_srq_recv  mv2_proxy_ibv_post_srq_recv
#elif defined(HAVE_PRAGMA_CRI_DUP)
#pragma _CRI duplicate mv2_proxy_ibv_post_srq_recv as t_mv2_proxy_ibv_post_srq_recv
#elif defined(HAVE_WEAK_ATTRIBUTE)
int mv2_proxy_ibv_post_srq_recv(struct ibv_srq *srq, struct ibv_recv_wr *recv_wr, struct ibv_recv_wr **bad_recv_wr)
                  __attribute__((weak,alias("t_mv2_proxy_ibv_post_srq_recv")));
#endif
/* -- End Profiling Symbol Block */

/* Define MPICH_MPI_FROM_PMPI if weak symbols are not supported to build
 the MPI routines */
#ifndef MPICH_MPI_FROM_PMPI
#undef mv2_proxy_ibv_post_srq_recv
#define mv2_proxy_ibv_post_srq_recv t_mv2_proxy_ibv_post_srq_recv

#undef FUNCNAME
#define FUNCNAME mv2_proxy_ibv_post_srq_recv
#undef FCNAME
#define FCNAME MPL_QUOTE(FUNCNAME)

int mv2_proxy_ibv_post_srq_recv(struct ibv_srq *srq, struct ibv_recv_wr *recv_wr, struct ibv_recv_wr **bad_recv_wr)
{
    int rv = ibv_post_srq_recv(srq,recv_wr,bad_recv_wr);
    return rv;
}

#endif

int mv2_proxy_ibv_query_device(struct ibv_context *context, struct ibv_device_attr *device_attr);
#if defined(HAVE_PRAGMA_WEAK)
#pragma weak mv2_proxy_ibv_query_device = t_mv2_proxy_ibv_query_device
#elif defined(HAVE_PRAGMA_HP_SEC_DEF)
#pragma _HP_SECONDARY_DEF t_mv2_proxy_ibv_query_device  mv2_proxy_ibv_query_device
#elif defined(HAVE_PRAGMA_CRI_DUP)
#pragma _CRI duplicate mv2_proxy_ibv_query_device as t_mv2_proxy_ibv_query_device
#elif defined(HAVE_WEAK_ATTRIBUTE)
int mv2_proxy_ibv_query_device(struct ibv_context *context, struct ibv_device_attr *device_attr)
                  __attribute__((weak,alias("t_mv2_proxy_ibv_query_device")));
#endif
/* -- End Profiling Symbol Block */

/* Define MPICH_MPI_FROM_PMPI if weak symbols are not supported to build
 the MPI routines */
#ifndef MPICH_MPI_FROM_PMPI
#undef mv2_proxy_ibv_query_device
#define mv2_proxy_ibv_query_device t_mv2_proxy_ibv_query_device

#undef FUNCNAME
#define FUNCNAME mv2_proxy_ibv_query_device
#undef FCNAME
#define FCNAME MPL_QUOTE(FUNCNAME)

int mv2_proxy_ibv_query_device(struct ibv_context *context, struct ibv_device_attr *device_attr)
{
    int rv = ibv_query_device(context,device_attr);
    return rv;
}

#endif

int mv2_proxy_ibv_query_gid(struct ibv_context *context, uint8_t port_num, int index, union ibv_gid *gid);
#if defined(HAVE_PRAGMA_WEAK)
#pragma weak mv2_proxy_ibv_query_gid = t_mv2_proxy_ibv_query_gid
#elif defined(HAVE_PRAGMA_HP_SEC_DEF)
#pragma _HP_SECONDARY_DEF t_mv2_proxy_ibv_query_gid  mv2_proxy_ibv_query_gid
#elif defined(HAVE_PRAGMA_CRI_DUP)
#pragma _CRI duplicate mv2_proxy_ibv_query_gid as t_mv2_proxy_ibv_query_gid
#elif defined(HAVE_WEAK_ATTRIBUTE)
int mv2_proxy_ibv_query_gid(struct ibv_context *context, uint8_t port_num, int index, union ibv_gid *gid)
                  __attribute__((weak,alias("t_mv2_proxy_ibv_query_gid")));
#endif
/* -- End Profiling Symbol Block */

/* Define MPICH_MPI_FROM_PMPI if weak symbols are not supported to build
 the MPI routines */
#ifndef MPICH_MPI_FROM_PMPI
#undef mv2_proxy_ibv_query_gid
#define mv2_proxy_ibv_query_gid t_mv2_proxy_ibv_query_gid

#undef FUNCNAME
#define FUNCNAME mv2_proxy_ibv_query_gid
#undef FCNAME
#define FCNAME MPL_QUOTE(FUNCNAME)

int mv2_proxy_ibv_query_gid(struct ibv_context *context, uint8_t port_num, int index, union ibv_gid *gid)
{
    int rv = ibv_query_gid(context,port_num,index,gid);
    return rv;
}

#endif

int mv2_proxy_ibv_query_pkey(struct ibv_context *context, uint8_t port_num, int index, uint16_t *pkey);
#if defined(HAVE_PRAGMA_WEAK)
#pragma weak mv2_proxy_ibv_query_pkey = t_mv2_proxy_ibv_query_pkey
#elif defined(HAVE_PRAGMA_HP_SEC_DEF)
#pragma _HP_SECONDARY_DEF t_mv2_proxy_ibv_query_pkey  mv2_proxy_ibv_query_pkey
#elif defined(HAVE_PRAGMA_CRI_DUP)
#pragma _CRI duplicate mv2_proxy_ibv_query_pkey as t_mv2_proxy_ibv_query_pkey
#elif defined(HAVE_WEAK_ATTRIBUTE)
int mv2_proxy_ibv_query_pkey(struct ibv_context *context, uint8_t port_num, int index, uint16_t *pkey)
                  __attribute__((weak,alias("t_mv2_proxy_ibv_query_pkey")));
#endif
/* -- End Profiling Symbol Block */

/* Define MPICH_MPI_FROM_PMPI if weak symbols are not supported to build
 the MPI routines */
#ifndef MPICH_MPI_FROM_PMPI
#undef mv2_proxy_ibv_query_pkey
#define mv2_proxy_ibv_query_pkey t_mv2_proxy_ibv_query_pkey

#undef FUNCNAME
#define FUNCNAME mv2_proxy_ibv_query_pkey
#undef FCNAME
#define FCNAME MPL_QUOTE(FUNCNAME)

int mv2_proxy_ibv_query_pkey(struct ibv_context *context, uint8_t port_num, int index, uint16_t *pkey)
{
    int rv = ibv_query_pkey(context,port_num,index,pkey);
    return rv;
}

#endif

int mv2_proxy_ibv_query_port(struct ibv_context *context, uint8_t port_num, struct ibv_port_attr *port_attr);
#if defined(HAVE_PRAGMA_WEAK)
#pragma weak mv2_proxy_ibv_query_port = t_mv2_proxy_ibv_query_port
#elif defined(HAVE_PRAGMA_HP_SEC_DEF)
#pragma _HP_SECONDARY_DEF t_mv2_proxy_ibv_query_port  mv2_proxy_ibv_query_port
#elif defined(HAVE_PRAGMA_CRI_DUP)
#pragma _CRI duplicate mv2_proxy_ibv_query_port as t_mv2_proxy_ibv_query_port
#elif defined(HAVE_WEAK_ATTRIBUTE)
int mv2_proxy_ibv_query_port(struct ibv_context *context, uint8_t port_num, struct ibv_port_attr *port_attr)
                  __attribute__((weak,alias("t_mv2_proxy_ibv_query_port")));
#endif
/* -- End Profiling Symbol Block */

/* Define MPICH_MPI_FROM_PMPI if weak symbols are not supported to build
 the MPI routines */
#ifndef MPICH_MPI_FROM_PMPI
#undef mv2_proxy_ibv_query_port
#define mv2_proxy_ibv_query_port t_mv2_proxy_ibv_query_port

#undef FUNCNAME
#define FUNCNAME mv2_proxy_ibv_query_port
#undef FCNAME
#define FCNAME MPL_QUOTE(FUNCNAME)

int mv2_proxy_ibv_query_port(struct ibv_context *context, uint8_t port_num, struct ibv_port_attr *port_attr)
{
    int rv = ibv_query_port(context,port_num,port_attr);
    return rv;
}

#endif

int mv2_proxy_ibv_query_qp(struct ibv_qp *qp, struct ibv_qp_attr *attr, int attr_mask, struct ibv_qp_init_attr *init_attr);
#if defined(HAVE_PRAGMA_WEAK)
#pragma weak mv2_proxy_ibv_query_qp = t_mv2_proxy_ibv_query_qp
#elif defined(HAVE_PRAGMA_HP_SEC_DEF)
#pragma _HP_SECONDARY_DEF t_mv2_proxy_ibv_query_qp  mv2_proxy_ibv_query_qp
#elif defined(HAVE_PRAGMA_CRI_DUP)
#pragma _CRI duplicate mv2_proxy_ibv_query_qp as t_mv2_proxy_ibv_query_qp
#elif defined(HAVE_WEAK_ATTRIBUTE)
int mv2_proxy_ibv_query_qp(struct ibv_qp *qp, struct ibv_qp_attr *attr, int attr_mask, struct ibv_qp_init_attr *init_attr)
                  __attribute__((weak,alias("t_mv2_proxy_ibv_query_qp")));
#endif
/* -- End Profiling Symbol Block */

/* Define MPICH_MPI_FROM_PMPI if weak symbols are not supported to build
 the MPI routines */
#ifndef MPICH_MPI_FROM_PMPI
#undef mv2_proxy_ibv_query_qp
#define mv2_proxy_ibv_query_qp t_mv2_proxy_ibv_query_qp

#undef FUNCNAME
#define FUNCNAME mv2_proxy_ibv_query_qp
#undef FCNAME
#define FCNAME MPL_QUOTE(FUNCNAME)

int mv2_proxy_ibv_query_qp(struct ibv_qp *qp, struct ibv_qp_attr *attr, int attr_mask, struct ibv_qp_init_attr *init_attr)
{
    int rv = ibv_query_qp(qp,attr,attr_mask,init_attr);
    return rv;
}

#endif

struct ibv_mr* mv2_proxy_ibv_reg_mr(struct ibv_pd *pd, void *addr, size_t length, int access);
#if defined(HAVE_PRAGMA_WEAK)
#pragma weak mv2_proxy_ibv_reg_mr = t_mv2_proxy_ibv_reg_mr
#elif defined(HAVE_PRAGMA_HP_SEC_DEF)
#pragma _HP_SECONDARY_DEF t_mv2_proxy_ibv_reg_mr  mv2_proxy_ibv_reg_mr
#elif defined(HAVE_PRAGMA_CRI_DUP)
#pragma _CRI duplicate mv2_proxy_ibv_reg_mr as t_mv2_proxy_ibv_reg_mr
#elif defined(HAVE_WEAK_ATTRIBUTE)
struct ibv_mr* mv2_proxy_ibv_reg_mr(struct ibv_pd *pd, void *addr, size_t length, int access)
                  __attribute__((weak,alias("t_mv2_proxy_ibv_reg_mr")));
#endif
/* -- End Profiling Symbol Block */

/* Define MPICH_MPI_FROM_PMPI if weak symbols are not supported to build
 the MPI routines */
#ifndef MPICH_MPI_FROM_PMPI
#undef mv2_proxy_ibv_reg_mr
#define mv2_proxy_ibv_reg_mr t_mv2_proxy_ibv_reg_mr

#undef FUNCNAME
#define FUNCNAME mv2_proxy_ibv_reg_mr
#undef FCNAME
#define FCNAME MPL_QUOTE(FUNCNAME)

struct ibv_mr* mv2_proxy_ibv_reg_mr(struct ibv_pd *pd, void *addr, size_t length, int access)
{
    struct ibv_mr* rv = ibv_reg_mr(pd,addr,length,access);
    return rv;
}

#endif

int mv2_proxy_ibv_req_notify_cq(struct ibv_cq *cq, int solicited_only);
#if defined(HAVE_PRAGMA_WEAK)
#pragma weak mv2_proxy_ibv_req_notify_cq = t_mv2_proxy_ibv_req_notify_cq
#elif defined(HAVE_PRAGMA_HP_SEC_DEF)
#pragma _HP_SECONDARY_DEF t_mv2_proxy_ibv_req_notify_cq  mv2_proxy_ibv_req_notify_cq
#elif defined(HAVE_PRAGMA_CRI_DUP)
#pragma _CRI duplicate mv2_proxy_ibv_req_notify_cq as t_mv2_proxy_ibv_req_notify_cq
#elif defined(HAVE_WEAK_ATTRIBUTE)
int mv2_proxy_ibv_req_notify_cq(struct ibv_cq *cq, int solicited_only)
                  __attribute__((weak,alias("t_mv2_proxy_ibv_req_notify_cq")));
#endif
/* -- End Profiling Symbol Block */

/* Define MPICH_MPI_FROM_PMPI if weak symbols are not supported to build
 the MPI routines */
#ifndef MPICH_MPI_FROM_PMPI
#undef mv2_proxy_ibv_req_notify_cq
#define mv2_proxy_ibv_req_notify_cq t_mv2_proxy_ibv_req_notify_cq

#undef FUNCNAME
#define FUNCNAME mv2_proxy_ibv_req_notify_cq
#undef FCNAME
#define FCNAME MPL_QUOTE(FUNCNAME)

int mv2_proxy_ibv_req_notify_cq(struct ibv_cq *cq, int solicited_only)
{
    int rv = ibv_req_notify_cq(cq,solicited_only);
    return rv;
}

#endif
