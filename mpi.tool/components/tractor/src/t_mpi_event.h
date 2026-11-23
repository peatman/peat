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

/* The MPI layer's view of a record (t_mpi_event.h): the header of msl_event (t_defines.h), then the parameters of an MPI
 * call. Used by the MPI layer only (the wrappers, t_mpi.cpp); the core reads the parameters through t_mpi_layer.get_view
 * and allocates t_record_size bytes per record, at least sizeof(msl_mpi_event). */
#ifndef __T_MPI_EVENT_H__
#define __T_MPI_EVENT_H__

#include <mpi.h>
#include "t_defines.h"
#include "t_mpi_iface.h"

struct msl_mpi_event {
    T_MSL_EVENT_HEADER
    union { /* this union should divide p2p,collective,osc,local etc function group */
        /*          P2P Transfer messages, Persistant Request INIT functions and probe  */
        struct {
            PEAT_Message_Size transfer_bytes;
            int tag;
            int peer;
            union {
                int flag;
                MPI_Request request; /* actual request or "0" if no request */
            };
            MPI_Comm comm;
#ifdef _MPI_MESSAGE_DEFINED_
            MPI_Message message; /* actual message handle or "0" if missing */
#else
            int message; /* fake value */
#endif
#ifdef _TRACTOR_EXTRA_FIELDS_
            addr_entry buf;
#endif
        } __attribute__((packed)) __attribute__((aligned(4))) p2p_pr_pb;

        /*          Completion (WAIT/TEST) and persistent request start (START) */
        struct {
            PEAT_Addr request_addr;
            int input_request_num;
            int triggered_request_num;
            int flag;
#ifdef _TRACTOR_EXTRA_FIELDS_
            addr_entry requests;
            addr_entry statuses;
#endif
        } __attribute__((packed)) __attribute__((aligned(4))) requests;

        /*          Collective functions     */
        struct {             // 8 + 8 + 4 + 4 + 4
            int sr_bytes[2]; // send/recv counts for collectives
            int root;
            MPI_Comm comm;
            MPI_Request request; /* actual request or "0" if no request */
#ifdef _TRACTOR_EXTRA_FIELDS_
            addr_entry buf[2];
#endif
        } __attribute__((packed)) __attribute__((aligned(4))) collective;
        /*          Communicators functions     */
        struct {
            MPI_Comm comm;
            MPI_Comm newcomm;
            MPI_Comm remotecomm;
            MPI_Request request;
        } __attribute__((packed)) __attribute__((aligned(4))) comms;

        /*          Buffered Local functions     */
        struct {
            long long count, size;
#ifdef _TRACTOR_EXTRA_FIELDS_
            addr_entry buf[2];
#endif
        } __attribute__((packed)) __attribute__((aligned(4))) buf_local;
        /*          One sided communication     */
        struct { // 8 + 4 + 4 + 4 + 4 + 4
            union {
                long long transfer_bytes;
                long long size;
            };
            int peer;
            MPI_Comm comm;
            MPI_Win window;
            MPI_Request request; /* actual request or "0" if no request */
#ifdef _TRACTOR_EXTRA_FIELDS_
            addr_entry buf;
            addr_entry rbuf;
            addr_entry cbuf;
#endif
        } __attribute__((packed)) __attribute__((aligned(4))) one_sided_comm;
        /*          File IO     */
        struct { // 8 + 8 + 4 + 4
            PEAT_Message_Size transfer_bytes;
            MPI_Offset offset;
            MPI_Comm comm;
            MPI_File fh;
            MPI_Request request; /* actual request or "0" if no request */
#ifdef _TRACTOR_EXTRA_FIELDS_
            addr_entry buf;
#endif
        } __attribute__((packed)) __attribute__((aligned(4))) mpi_fio;

        /*          Other local      */
        struct {
            MPI_Comm comm;
        } __attribute__((packed)) __attribute__((aligned(4))) local;
        struct {
            int ctl;
        } __attribute__((packed)) __attribute__((aligned(4))) pcontrol;
    };
};

/* The MPI layer (-D_T_MPI_LAYER_, libtractor.<mpi>.so) reaches the core through t_shared only (t_shim_state.h), as the
   event-class shims do (t_class_shim.h): gt_info(x) is t_shared.x, so the code written for the core (the generated
   wrappers, mpi_layer.cpp.in) compiles unchanged, and a use of anything of the core that t_shared does not have fails to
   compile instead of reaching into struct tractor, whose layout depends on the core's build switches. Include after
   tractor.h. */
#ifdef _T_MPI_LAYER_
#ifdef _T_CLASS_SHIM_
#error "_T_MPI_LAYER_ and _T_CLASS_SHIM_ are two different libraries"
#endif
#undef gt_info
#define gt_info(x) t_shared.x
#undef __msl_allocate_event
#undef __msl_reuse_event
#undef __msl_discard_event
#undef __msl_uncount_event
#define __msl_allocate_event(tid_in, level, skip_write, event, counters) t_shared.allocate_record(tid_in, level, skip_write, (msl_event **)(event), counters)
#define __msl_reuse_event(tid_in, level, event, counters, funcid) t_shared.reuse_record(tid_in, level, (msl_event **)(event), counters, funcid)
#define __msl_discard_event(tid_in, level, event, children_base) t_shared.discard_record(tid_in, level, (msl_event **)(event), children_base)
#define __msl_uncount_event(tid, level) t_shared.uncount_record(tid, level)
/* VIRT2PHYS builds: the physical pages of the buffers of a finished record (the core looks them up) */
#ifdef _TRACTOR_VIRT2PHYS_
#define T_VIRT2PHYS(event) \
    do { \
        if (t_shared.virt2phys) \
            t_shared.virt2phys((msl_event *)(event)); \
    } while (0)
#else
#define T_VIRT2PHYS(event)
#endif
/* the inline stack unwinding (t_defines.h) reads the bounds from t_shared */
#undef T_UNWIND_INFO
#define T_UNWIND_INFO(x) t_shared.x
#endif
/* the last record of the level (an MPI-internal hook checks it was not reported by its MPI call already) */
#define __msl_last_record(tid, level) ((msl_mpi_event *)t_shared.last_record(tid, level))

#if defined(__CPLUSPLUS__) || defined(__cplusplus)
extern "C" {
#endif
/* the MPI layer's state (t_mpi.cpp) */
extern MPI_Comm t_mpi_comm_world;          /* tractor's own duplicate of MPI_COMM_WORLD, from post_init to release */
extern struct t_local_comms *t_local_comms_p; /* communicators created by this rank (t_comm.h), a rank only */
#if defined(__CPLUSPLUS__) || defined(__cplusplus)
}
#endif
#endif /* __T_MPI_EVENT_H__ */
