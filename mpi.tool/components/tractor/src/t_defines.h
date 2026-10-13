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

#ifndef __T_DEFINES_H__
#define __T_DEFINES_H__
#include <stdio.h>
#include <sys/types.h>
// #include <linux/unistd.h>
#include <unistd.h>

#include <mpi.h>
#include <time.h>
#ifdef _T_OMPT_
#include <omp-tools.h>
#endif

#ifndef MAX_PATH
#define MAX_PATH 2048
#endif
#ifndef __max__
#define __max__(x, y) (x) > (y) ? (x) : (y)
#endif
#ifndef __min__
#define __min__(x, y) (x) < (y) ? (x) : (y)
#endif

#ifndef MAX_HOSTNAME
#define MAX_HOSTNAME 256
#endif

#ifndef MAX_TID_NUM
#define MAX_TID_NUM 262144
#endif

#ifdef _TRACTOR_DIAG_
#define gt_info(x) t_info_debug.t_info->x
#else
#define gt_info(x) t_info.x
#endif
/* debug/profiling switches. */
#ifdef _TRACTOR_DIAG_
#if defined(__CPLUSPLUS__) || defined(__cplusplus)
extern "C" {
#endif

extern FILE *debug_fp;

#if defined(__CPLUSPLUS__) || defined(__cplusplus)
}
#endif
// #define debug_printf(args...) if (debug_fp) { fprintf(debug_fp, ##args); fprintf(debug_fp, "%p\n",debug_fp); fflush(debug_fp);} else { fprintf(stderr, ##args); fprintf(stderr, "%p\n",debug_fp); fflush(stderr);}
#define debug_printf(args...)      \
    if (debug_fp) {                \
        fprintf(debug_fp, ##args); \
        fflush(debug_fp);          \
    } else {                       \
        fprintf(stderr, ##args);   \
        fflush(stderr);            \
    }
#else
static void debug_printf(const char *format, ...) {};
#endif

#ifdef _TRACTOR_DEBUG_TRACTOR_LAYER_ENTRY_EXIT_
#define _TRACTOR_DEBUG_ENTRY_EXIT_
#define __DEBUG_PROLOG_TRACTOR_LAYER__ __DEBUG_PROLOG__
#define __DEBUG_EPILOG_TRACTOR_LAYER__ __DEBUG_EPILOG__
#else
#define __DEBUG_PROLOG_TRACTOR_LAYER__
#define __DEBUG_EPILOG_TRACTOR_LAYER__
#endif

#ifdef _TRACTOR_DEBUG_MPI_LAYER_ENTRY_EXIT_
#define _TRACTOR_DEBUG_ENTRY_EXIT_
#define __DEBUG_PROLOG_MPI_LAYER__ __DEBUG_PROLOG__
#define __DEBUG_EPILOG_MPI_LAYER__ __DEBUG_EPILOG__
#else
#define __DEBUG_PROLOG_MPI_LAYER__
#define __DEBUG_EPILOG_MPI_LAYER__
#endif

#ifdef _TRACTOR_DEBUG_GPU_LAYER_ENTRY_EXIT_
#define _TRACTOR_DEBUG_ENTRY_EXIT_
#define __DEBUG_PROLOG_GPU_LAYER__ __DEBUG_PROLOG__
#define __DEBUG_EPILOG_GPU_LAYER__ __DEBUG_EPILOG__
#else
#define __DEBUG_PROLOG_GPU_LAYER__
#define __DEBUG_EPILOG_GPU_LAYER__
#endif

#ifdef _TRACTOR_DEBUG_COMM_ENTRY_EXIT_
#define _TRACTOR_DEBUG_ENTRY_EXIT_
#define __DEBUG_PROLOG_COMM_LAYER__ __DEBUG_PROLOG__
#define __DEBUG_EPILOG_COMM_LAYER__ __DEBUG_EPILOG__
#else
#define __DEBUG_PROLOG_COMM_LAYER__
#define __DEBUG_EPILOG_COMM_LAYER__
#endif

#ifdef _TRACTOR_DEBUG_OMPT_ENTRY_EXIT_
#define _TRACTOR_DEBUG_ENTRY_EXIT_
#define __DEBUG_PROLOG_OMPT_LAYER__ __DEBUG_PROLOG__
#define __DEBUG_EPILOG_OMPT_LAYER__ __DEBUG_EPILOG__
#else
#define __DEBUG_PROLOG_OMPT_LAYER__
#define __DEBUG_EPILOG_OMPT_LAYER__
#endif

#ifdef _TRACTOR_DEBUG_ENTRY_EXIT_
#define __DEBUG_PROLOG__ debug_printf("Rank %d: Entering %s at %d in %s\n", t_mpi_rank, __FUNCTION__, __LINE__, __FILE__);
#define __DEBUG_EPILOG__ debug_printf("Rank %d: Leaving  %s at %d in %s\n", t_mpi_rank, __FUNCTION__, __LINE__, __FILE__);
#else
#define __DEBUG_PROLOG__
#define __DEBUG_EPILOG__
#endif

#include "common_plain_types.h"

#include "t_time.h"

#if 1
/* Frames inside the MPI Fortran wrapper library(ies) are skipped so the recorded call site is the user's Fortran code */
#ifdef _OPENMPI_
#if defined(__CPLUSPLUS__) || defined(__cplusplus)
extern "C" {
#endif
int t_addr_in_fortran_lib(const void *addr); /* tractor.cpp: any of libmpi_mpifh, libmpi_usempif08, libmpi_usempi* */
#if defined(__CPLUSPLUS__) || defined(__cplusplus)
}
#endif
#define T_IN_FORTRAN_LIB(a) t_addr_in_fortran_lib(a)
#else
#define T_IN_FORTRAN_LIB(a) ((a) > gt_info(lowfort) && (a) < gt_info(highfort)) /* libmpifort */
#endif

#define stack_unwind_x(lev_num, store_addr, levels)                                                            \
    {                                                                                                         \
        void **t_frame;                                                                                       \
        asm("movq %%rbp, %0;" : "=r"(t_frame) :);                                                             \
        void *ret_addr = t_frame[1];                                                                          \
        while (T_IN_FORTRAN_LIB(ret_addr)) {                                                                 \
            t_frame = (void **)t_frame[0];                                                                    \
            ret_addr = t_frame[1];                                                                            \
        }                                                                                                     \
        for (int i = 0, j = 0; i <= lev_num; i++) {                                                           \
            ret_addr = t_frame[1];                                                                            \
            if (levels[j] == i)                                                                               \
                store_addr[j++] = reinterpret_cast<void *>(reinterpret_cast<unsigned long>(ret_addr) /*-5*/); \
            if ((ret_addr > gt_info(lowval) && ret_addr < gt_info(highval)) || ret_addr < gt_info(binval))    \
                t_frame = (void **)t_frame[0];                                                                \
        }                                                                                                     \
    }

#define stack_unwind0(lev_num, store_addr, levels)                                                            \
    {                                                                                                         \
        void **t_frame;                                                                                       \
        asm("movq %%rbp, %0;" : "=r"(t_frame) :);                                                             \
        void *ret_addr = t_frame[1];                                                                          \
        while (T_IN_FORTRAN_LIB(ret_addr)) {                                                                 \
            t_frame = (void **)t_frame[0];                                                                    \
            ret_addr = t_frame[1];                                                                            \
        }                                                                                                     \
        for (int i = 0, j = 0; i <= lev_num; i++) {                                                           \
            ret_addr = t_frame[1];                                                                            \
            if (levels[j] == i)                                                                               \
                store_addr[j++] = reinterpret_cast<void *>(reinterpret_cast<unsigned long>(ret_addr) /*-5*/); \
        }                                                                                                     \
    }

#define stack_unwind(lev_num, store_addr, levels)  \
    if (fullstack_permission[fidc]) {              \
        stack_unwind_x(lev_num, store_addr, levels) \
    } else {                                       \
        stack_unwind0(lev_num, store_addr, levels) \
    }

/*
#define stack_unwind(lev_num,store_addr,levels) {                                                                                    \
    int  start_frame=0;                                                                                                              \
    void *ret_addr=__builtin_return_address(0);                                                                                      \
    while (ret_addr>gt_info(lowfort) && ret_addr<gt_info(highfort)) {start_frame++;ret_addr=__builtin_return_address(start_frame);}  \
    for (int i=0,j=0;i<=lev_num;i++) {                                                                                               \
        if (levels[j]==i) store_addr[j++] = __builtin_return_address(j+start_frame);                                                 \
    }                                                                                                                                \
}
*/
#define mpi_internal_stack_unwind(lev_num, store_addr, levels) stack_unwind0(lev_num, store_addr, levels)

#else
#define stack_unwind(lev_num, store_addr, levels)
#define mpi_internal_stack_unwind(lev_num, store_addr, levels)
#endif

struct msl_extra_info {
    union {
        long long value;
        int values[2];
    };
};

struct t_memory_event {
    t_memory_event()
    {
        vm_size = vm_RSS = ru_maxrss = mem_used_by_tractor = 0;
    }
    PEAT_Num vm_size;
    PEAT_Num vm_RSS;
    PEAT_Num ru_maxrss;
    PEAT_LNum mem_used_by_tractor;
};

struct msl_extras_counters {
    PEAT_Addr *addrs;
#ifdef _HWCOUNTERS_ENABLED_
    long long *hwcs;
#endif
#ifdef _MEMPROFILE_STORE_
    t_memory_event *mems;
#endif
};

#include "addr_entry.h"
struct msl_event {
    PEAT_Clock tscs[4];
    short func_id;      /* through id; the base id comes from mpi_func_ids_trans[] when writing */
    short num_children; /* records of the next nesting level created while this call was open */
#ifdef _TRACTOR_STORE_UNIQ_INDEX_
    long long uniq_index;
#endif
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
        struct {
            int num_arguments;
            int num_out_arguments;
            int num_extra_arguments;
        } __attribute__((packed)) __attribute__((aligned(4))) gpu;
#ifdef _T_OMPT_
        struct {
            long long parallel_data;
            long long task_data;
            PEAT_Addr pcode;
            short kind;
            short endpoint;
        } __attribute__((packed)) __attribute__((aligned(4))) ompt;
#endif
    };
};

struct t_timeline {
    PEAT_Clock tsc;
    int tid; // we definitely need this to disambigue the same tsc(s) coming from different threads
    t_timeline(PEAT_Clock _tsc, int _tid) : tsc(_tsc), tid(_tid) {};
    bool operator<(const t_timeline &rhs) const
    {
        if (tsc < rhs.tsc) {
            return true;
        }
        if (tsc > rhs.tsc) {
            return false;
        }
        if (tid < rhs.tid) {
            return true;
        }
        if (tid > rhs.tid) {
            return false;
        }
        return false;
    }
};

/*#ifdef _T_OMPT_
extern  ompt_finalize_tool_t ompt_finalize_tool;
#endif*/

#define __msl_allocate_event(tid_in, level, skip_write, event, counters) gt_info(allocate_event(tid_in, level, skip_write, event, counters))
#define __msl_reuse_event(tid_in, level, event, counters, funcid) gt_info(reuse_event(tid_in, level, event, counters, funcid))
#define __msl_discard_event(tid_in, level, event, children_base) gt_info(discard_event(tid_in, level, event, children_base))

#define __msl_get_tid(tid_in) gt_info(get_tid(tid_in))
#define __msl_check_chain(tid, level) gt_info(t_check_chain(tid, level))

/* nesting: a traced call issued while another traced call of the thread is open is one level deeper */
#define __msl_enter_call(tid) gt_info(enter_call(tid))
#define __msl_leave_call(tid) gt_info(leave_call(tid))
#define __msl_child_base(tid, level) gt_info(child_base(tid, level))
#define __msl_current_level(tid) gt_info(current_level(tid))
#define __msl_uncount_event(tid, level) gt_info(uncount_event(tid, level))

#define __msl_record_int(tid, level, value) gt_info(record_int(tid, level, value))
#define __msl_record_long(tid, level, value) gt_info(record_long(tid, level, value))
#define __msl_record_ints(tid, level, value, nums) gt_info(record_ints(tid, level, value, nums))
#define __msl_record_longs(tid, level, value, nums) gt_info(record_longs(tid, level, value, nums))
#define __msl_record_low_longs(tid, level, value, nums) gt_info(record_low_longs(tid, level, value, nums))

#ifdef _TRACTOR_VIRT2PHYS_
bool __msl_virt2phys(int pmfile, int pffile, addr_entry *entry);
int __msl_open_pfile(char *ppath);
void __msl_close_pfile(int pfile);
#endif //_TRACTOR_VIRT2PHYS_

#endif //__T_DEFINES_H__
