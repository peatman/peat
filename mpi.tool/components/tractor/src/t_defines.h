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

#include <time.h>
#include "t_shim_state.h"
#ifdef _T_OMPT_
#include <omp-tools.h>
#endif
#ifdef _T_ACC_
#include <acc_prof.h>
#endif
/* GPU-style records (parameter lists in the extras) and their transfer/execution statistics: written for OpenCL/Level
   Zero (GPUPARSE), OpenACC and CUDA; always compiled, the records may come from a shim (print_gpu_stat) */
#define _T_GPU_STAT_

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
#define debug_printf(args...)          \
    do {                               \
        if (debug_fp) {                \
            fprintf(debug_fp, ##args); \
            fflush(debug_fp);          \
        } else {                       \
            fprintf(stderr, ##args);   \
            fflush(stderr);            \
        }                              \
    } while (0)
#else
static void debug_printf(const char *format, ...) {};
#endif

#if defined(__cplusplus)
#include <stdint.h>
#include <type_traits>
// Portable printf helpers: MPI handles are ints (MPICH) or pointers (Open MPI), PEAT_Addr is a pointer.
template <typename T>
static inline typename std::enable_if<std::is_pointer<T>::value, int>::type peat_handle_id(T h)
{
    return static_cast<int>(reinterpret_cast<intptr_t>(h));
}
template <typename T>
static inline typename std::enable_if<!std::is_pointer<T>::value, int>::type peat_handle_id(T h)
{
    return static_cast<int>(h);
}
static inline unsigned long long peat_addr_ull(const void *a)
{
    return static_cast<unsigned long long>(reinterpret_cast<uintptr_t>(a));
}
static inline unsigned long long peat_addr_ull(unsigned long long a)
{
    return a;
}
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

#ifdef _TRACTOR_DEBUG_TRANSPORT_LAYER_ENTRY_EXIT_
#define _TRACTOR_DEBUG_ENTRY_EXIT_
#define __DEBUG_PROLOG_TRANSPORT_LAYER__ __DEBUG_PROLOG__
#define __DEBUG_EPILOG_TRANSPORT_LAYER__ __DEBUG_EPILOG__
#else
#define __DEBUG_PROLOG_TRANSPORT_LAYER__
#define __DEBUG_EPILOG_TRANSPORT_LAYER__
#endif

#ifdef _TRACTOR_DEBUG_GPU_LAYER_ENTRY_EXIT_
#define _TRACTOR_DEBUG_ENTRY_EXIT_
#define __DEBUG_PROLOG_GPU_LAYER__ __DEBUG_PROLOG__
#define __DEBUG_EPILOG_GPU_LAYER__ __DEBUG_EPILOG__
#else
#define __DEBUG_PROLOG_GPU_LAYER__
#define __DEBUG_EPILOG_GPU_LAYER__
#endif

#ifdef _TRACTOR_DEBUG_LLAMA_LAYER_ENTRY_EXIT_
#define _TRACTOR_DEBUG_ENTRY_EXIT_
#define __DEBUG_PROLOG_LLAMA_LAYER__ __DEBUG_PROLOG__
#define __DEBUG_EPILOG_LLAMA_LAYER__ __DEBUG_EPILOG__
#else
#define __DEBUG_PROLOG_LLAMA_LAYER__
#define __DEBUG_EPILOG_LLAMA_LAYER__
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

#ifdef _TRACTOR_DEBUG_ACC_ENTRY_EXIT_
#define _TRACTOR_DEBUG_ENTRY_EXIT_
#define __DEBUG_PROLOG_ACC_LAYER__ __DEBUG_PROLOG__
#define __DEBUG_EPILOG_ACC_LAYER__ __DEBUG_EPILOG__
#else
#define __DEBUG_PROLOG_ACC_LAYER__
#define __DEBUG_EPILOG_ACC_LAYER__
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
/* where the unwinding finds the address bounds: the core's own members, a shim redefines it to t_shared.x */
#ifndef T_UNWIND_INFO
#define T_UNWIND_INFO(x) gt_info(x)
#endif
/* Frames inside the MPI Fortran wrapper library(ies) are skipped so the recorded call site is the user's Fortran code.
   The libraries are named by the MPI layer (t_mpi_layer.fortran_libs); lowfort/highfort is the first one. */
#if defined(__CPLUSPLUS__) || defined(__cplusplus)
extern "C" {
#endif
int t_addr_in_fortran_lib(const void *addr); /* tractor.cpp: in any of the MPI Fortran wrapper libraries */
#if defined(__CPLUSPLUS__) || defined(__cplusplus)
}
#endif
#if defined(_OPENMPI_) && !defined(_T_CLASS_SHIM_)
#define T_IN_FORTRAN_LIB(a) t_addr_in_fortran_lib(a) /* Open MPI has several Fortran wrapper libraries */
#else
#define T_IN_FORTRAN_LIB(a) ((a) > T_UNWIND_INFO(lowfort) && (a) < T_UNWIND_INFO(highfort))
#endif

#define stack_unwind_x(lev_num, store_addr, levels)                                                            \
    {                                                                                                         \
        /* the frame of the calling wrapper; a plain asm reading rbp may be scheduled by the compiler before the frame is set up */ \
        void **t_frame = (void **)__builtin_frame_address(0);                                                 \
        void *ret_addr = t_frame[1];                                                                          \
        while (T_IN_FORTRAN_LIB(ret_addr)) {                                                                 \
            t_frame = (void **)t_frame[0];                                                                    \
            ret_addr = t_frame[1];                                                                            \
        }                                                                                                     \
        for (int i = 0, j = 0; i <= lev_num; i++) {                                                           \
            ret_addr = t_frame[1];                                                                            \
            if (levels[j] == i)                                                                               \
                store_addr[j++] = reinterpret_cast<void *>(reinterpret_cast<unsigned long>(ret_addr) /*-5*/); \
            if ((ret_addr > T_UNWIND_INFO(lowval) && ret_addr < T_UNWIND_INFO(highval)) || ret_addr < T_UNWIND_INFO(binval)) \
                t_frame = (void **)t_frame[0];                                                                \
        }                                                                                                     \
    }

#define stack_unwind0(lev_num, store_addr, levels)                                                            \
    {                                                                                                         \
        /* the frame of the calling wrapper; a plain asm reading rbp may be scheduled by the compiler before the frame is set up */ \
        void **t_frame = (void **)__builtin_frame_address(0);                                                 \
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

#define stack_unwind(lev_num, store_addr, levels)      \
    do {                                                \
        if (fullstack_permission[fidc]) {               \
            stack_unwind_x(lev_num, store_addr, levels) \
        } else {                                        \
            stack_unwind0(lev_num, store_addr, levels)  \
        }                                               \
    } while (0)

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
    long long value; // every extra is written as 64 bits (write_record/write_records)
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

/* where the record's return addresses and counter samples go (entry and exit each); hwcs/mems are NULL when the core
   takes no such samples. The same layout everywhere: the MPI layer and the shims fill it through t_shared. */
struct msl_extras_counters {
    PEAT_Addr *addrs;
    long long *hwcs;
    t_memory_event *mems;
};

/* counter samples around a call (t_shim_state.h counters_*): DECL a sample buffer, ENTRY before the call, EXIT(c) into
   the record after it; ENTRY_TO(c) / EXIT_TO(c) when the record exists before the call already; COPY(d, s) for the
   second record of a pair. The event-class shims take no counters (t_class_shim.h). */
#define T_COUNTERS_DECL long long t_csample[t_shared.counters_size > 0 ? (t_shared.counters_size + 7) / 8 : 1];
#define T_COUNTERS_ENTRY \
    if (t_shared.counters_size) \
        t_shared.counters_read(t_csample);
#define T_COUNTERS_EXIT(c) \
    if (t_shared.counters_size) \
        t_shared.counters_store(&(c), t_csample);
#define T_COUNTERS_ENTRY_TO(c) \
    if (t_shared.counters_size) \
        t_shared.counters_entry(&(c));
#define T_COUNTERS_EXIT_TO(c) \
    if (t_shared.counters_size) \
        t_shared.counters_store(&(c), NULL);
#define T_COUNTERS_COPY(d, s) \
    if (t_shared.counters_size) \
        t_shared.counters_copy(&(d), &(s));

#include "addr_entry.h"
/* A record: the header, the same for every kind of record, then the parameters of the call. The core knows the
   parameters of its own classes (transport, GPU, OMPT, CUDA) only; the MPI layer's are msl_mpi_event (t_mpi_event.h, the
   same header), which the core reads through t_mpi_layer.get_view. The records are t_record_size bytes apart
   (t_base_event.cpp): the larger of the two. */
#ifdef _TRACTOR_STORE_UNIQ_INDEX_
#define T_MSL_EVENT_UNIQ_INDEX long long uniq_index;
#else
#define T_MSL_EVENT_UNIQ_INDEX
#endif
#define T_MSL_EVENT_HEADER                                                                                  \
    PEAT_Clock tscs[4];                                                                                     \
    short func_id;      /* through id; the base id comes from mpi_func_ids_trans[] when writing */          \
    short num_children; /* records of the next nesting level created while this call was open */           \
    T_MSL_EVENT_UNIQ_INDEX

struct msl_event {
    T_MSL_EVENT_HEADER
    union {
        struct { /* 8 + 4 */
            PEAT_Clock total_clocks_bitween_calls;
            PEAT_Clock first_call;
            int num_calls;
        } __attribute__((packed)) __attribute__((aligned(4))) transport;
        struct {
            int num_arguments;
            int num_out_arguments;
            int num_extra_arguments;
        } __attribute__((packed)) __attribute__((aligned(4))) gpu;
        struct {
            long long parallel_data;
            long long task_data;
            PEAT_Addr pcode;
            short kind;
            short endpoint;
        } __attribute__((packed)) __attribute__((aligned(4))) ompt;
        struct { /* the GPU layout (same first members as gpu) and the name of a kernel (written by the CUDA class) */
            int num_arguments;
            int num_out_arguments;
            int num_extra_arguments;
            int name_index; /* cuda_kernel records: index of the kernel name (t_shim_state::cuda_kernel_name) */
        } __attribute__((packed)) __attribute__((aligned(4))) cuda;
    };
};

#if defined(__CPLUSPLUS__) || defined(__cplusplus)
extern "C" {
#endif
extern int t_record_size; /* bytes from one record to the next: max(sizeof(msl_event), t_mpi_layer.record_size) */
#if defined(__CPLUSPLUS__) || defined(__cplusplus)
}
#endif
/* record i of an array of records */
#define T_RECORD(array, i) ((msl_event *)((char *)(array) + (size_t)(i) * (size_t)t_record_size))

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

/* event: the address of a msl_event * or, in the MPI layer, of a msl_mpi_event * (the same header) */
#define __msl_allocate_event(tid_in, level, skip_write, event, counters) gt_info(allocate_event(tid_in, level, skip_write, (msl_event **)(event), counters))
#define __msl_reuse_event(tid_in, level, event, counters, funcid) gt_info(reuse_event(tid_in, level, (msl_event **)(event), counters, funcid))
#define __msl_discard_event(tid_in, level, event, children_base) gt_info(discard_event(tid_in, level, (msl_event **)(event), children_base))
/* transport records: through t_shared (t_shim_state.h), the same in the core and in the shims */
#define T_TRANSPORT_PERMIT (t_shared.t_permit_collect_transport)
#define __msl_transport_get_tid(tid_in) t_shared.get_tid(tid_in)
#define __msl_transport_allocate_event(tid_in, event, funcid) t_shared.transport_allocate_event(tid_in, event, funcid)
#define __msl_transport_reuse_event(tid_in, event, funcid) t_shared.transport_reuse_event(tid_in, event, funcid)

#define __msl_get_tid(tid_in) gt_info(get_tid(tid_in))
#define __msl_check_chain(tid, level) gt_info(t_check_chain(tid, level))

/* nesting: a traced call issued while another traced call of the thread is open is one level deeper */
#define __msl_enter_call(tid) gt_info(enter_call(tid))
#define __msl_leave_call(tid) gt_info(leave_call(tid))
#define __msl_child_base(tid, level) gt_info(child_base(tid, level))
#define __msl_current_level(tid) gt_info(current_level(tid))
#define __msl_uncount_event(tid, level) gt_info(uncount_event(tid, level))

#define __msl_write_record(tid, level, value) gt_info(write_record(tid, level, value))
#define __msl_write_records(tid, level, value, nums) gt_info(write_records(tid, level, value, nums))

#ifdef _TRACTOR_VIRT2PHYS_
bool __msl_virt2phys(int pmfile, int pffile, addr_entry *entry);
int __msl_open_pfile(char *ppath);
void __msl_close_pfile(int pfile);
#endif //_TRACTOR_VIRT2PHYS_

#endif //__T_DEFINES_H__
