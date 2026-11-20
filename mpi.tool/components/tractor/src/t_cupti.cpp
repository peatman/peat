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

// CUDA through CUPTI (built with CUPTI_PARSE=on, recorded with PEAT_TRACTOR_PARSE_CUDA=1), the CLASS_CUDA entries of
// mpi_func.inc:
//  - host CUDA runtime calls, from the callback API: the ENTER and EXIT callbacks of one call make one record, nested
//    like the generated wrappers (llama_process > ggml_backend_sched_graph_compute_async > cudaLaunchKernel);
//  - GPU activity (kernels, copies, memsets, synchronizations), from the activity API: one record per activity, top level,
//    on a thread lane of its own per context and stream. A kernel record is written with the kernel's name (see
//    t_cupti_kernel_name()) instead of cuda_kernel. Lanes are per context, the synchronization records have no device id.
// CUPTI takes its timestamps from tractor's clock (cuptiActivityRegisterTimestampCallback), so the GPU records are on the
// same time line as the host ones without any conversion.

#ifndef USE_NANO_TIMER
#error "t_cupti.cpp needs USE_NANO_TIMER: CUPTI timestamps are nanoseconds"
#endif

#include "mpi_func_ids.h"
#include "t_defines.h"
#include "t_extradef.h"
#include "tractor.h"
#include <cupti.h>
#include <cxxabi.h>
#include <mutex>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <unordered_map>
#include <vector>

extern int counting_permit;
extern int cuda_skip_parse;
extern PEAT_Clock global_gpu_time;
extern PEAT_Clock global_tool_overhead;
PEAT_Clock global_cuda_time = 0; /* CUDA records were taken: tractor::end() writes the trace of a non-MPI process */

#define T_CUPTI_CHECK(call)                                                                    \
    do {                                                                                       \
        CUptiResult _status = call;                                                            \
        if (_status != CUPTI_SUCCESS) {                                                        \
            const char *_err = NULL;                                                           \
            cuptiGetResultString(_status, &_err);                                              \
            fprintf(stderr, "tractor: %s failed: %s, CUDA is not traced\n", #call, _err ? _err : "?"); \
            return;                                                                            \
        }                                                                                      \
    } while (0)

static CUpti_SubscriberHandle t_cupti_subscriber;
static bool t_cupti_active = false;  /* subscribed and activity enabled */
static bool t_cupti_tried = false;   /* t_cupti_init() ran (once: a CUPTI that fails to start is reported only once) */
static bool t_cupti_flushed = false; /* the last flush is done: CUDA may be torn down after it */

/* ---------------------------------------------------------------------------------------------------------------- */
/* kernel names                                                                                                      */

/* built on first use and never destroyed: t_cupti_init() runs from the constructor of tractor's global object in
   tractor.cpp, possibly before the namespace-scope objects of this file are constructed, and the kernel names are
   needed by its destructor, which writes the trace after the function-local statics are destroyed */
static std::mutex &t_kernel_names_mutex() { static std::mutex *m = new std::mutex; return *m; }
static std::unordered_map<std::string, int> &t_kernel_name_index() { static std::unordered_map<std::string, int> *m = new std::unordered_map<std::string, int>; return *m; }
static std::vector<std::string> &t_kernel_names() { static std::vector<std::string> *v = new std::vector<std::string>; return *v; }

/* "void mul_mat_vec_q<(ggml_type)2, 1, false>(void const*, ...)" -> "mul_mat_vec_q<(ggml_type)2, 1, false>":
   no return type and parameter list, "::" (and any ':', the field separator of the trace) written as '.' */
static std::string t_kernel_short_name(const char *mangled)
{
    int status = -1;
    char *demangled = abi::__cxa_demangle(mangled, NULL, NULL, &status);
    std::string s = (status == 0 && demangled) ? demangled : mangled;
    free(demangled);
    if (s.compare(0, 5, "void ") == 0)
        s.erase(0, 5);
    int angle = 0;
    for (size_t i = 0; i < s.size(); i++) {
        const char c = s[i];
        if (c == '<') {
            angle++;
        } else if (c == '>') {
            angle--;
        } else if (c == '(' && angle == 0 && i > 0) {
            s.erase(i);
            break;
        }
    }
    std::string out;
    out.reserve(s.size());
    for (size_t i = 0; i < s.size(); i++) {
        if (s[i] == ':') {
            out += '.';
            if (i + 1 < s.size() && s[i + 1] == ':')
                i++;
        } else if (s[i] == '\n' || s[i] == '\r') {
            out += ' ';
        } else {
            out += s[i];
        }
    }
    return out.empty() ? std::string("cuda_kernel") : out;
}

static int t_kernel_name_id(const char *mangled)
{
    std::lock_guard<std::mutex> lock(t_kernel_names_mutex());
    std::unordered_map<std::string, int>::iterator it = t_kernel_name_index().find(mangled ? mangled : "");
    if (it != t_kernel_name_index().end())
        return it->second;
    const int idx = (int)t_kernel_names().size();
    t_kernel_names().push_back(t_kernel_short_name(mangled ? mangled : "cuda_kernel"));
    t_kernel_name_index()[mangled ? mangled : ""] = idx;
    return idx;
}

extern "C" const char *t_cupti_kernel_name(int idx)
{
    std::lock_guard<std::mutex> lock(t_kernel_names_mutex());
    if (idx < 0 || idx >= (int)t_kernel_names().size())
        return "cuda_kernel";
    return t_kernel_names()[idx].c_str();
}

/* ---------------------------------------------------------------------------------------------------------------- */
/* host CUDA runtime calls (callback API)                                                                            */

/* API name -> through id of the CLASS_CUDA entries */
static std::unordered_map<std::string, short> &t_runtime_ids() { static std::unordered_map<std::string, short> *m = new std::unordered_map<std::string, short>; return *m; }

struct t_cupti_frame { /* an open runtime call, between its ENTER and EXIT callbacks */
    msl_event *event;
    msl_extras_counters counters;
    long long tsc_entry, tsc_mpi_entry, children_base;
    int tid, level;
    short fidc;
};
#define T_CUPTI_MAX_OPEN 32
static __thread t_cupti_frame t_cupti_open[T_CUPTI_MAX_OPEN];
static __thread int t_cupti_open_num = 0;

/* "cudaMemcpyAsync_ptsz" / "cudaLaunchKernel_v7000" -> "cudaMemcpyAsync" / "cudaLaunchKernel" */
static short t_runtime_id(const char *name)
{
    if (name == NULL)
        return -1;
    std::unordered_map<std::string, short>::const_iterator it = t_runtime_ids().find(name);
    if (it != t_runtime_ids().end())
        return it->second;
    std::string s(name);
    const size_t us = s.rfind('_');
    if (us != std::string::npos) {
        const std::string suffix = s.substr(us + 1);
        if (suffix == "ptsz" || suffix == "ptds" || (suffix.size() > 1 && suffix[0] == 'v' && isdigit((unsigned char)suffix[1]))) {
            it = t_runtime_ids().find(s.substr(0, us));
            if (it != t_runtime_ids().end())
                return it->second;
        }
    }
    return -1;
}

static void t_cupti_api_enter(const CUpti_CallbackData *cb, short fidc)
{
    __DEBUG_PROLOG_GPU_LAYER__
    if (t_cupti_open_num >= T_CUPTI_MAX_OPEN) {
        *cb->correlationData = 0;
        __DEBUG_EPILOG_GPU_LAYER__
        return;
    }
    t_cupti_frame &f = t_cupti_open[t_cupti_open_num];
    pid_t tid_in;
    f.tsc_entry = peat_get_time_tid(&tid_in);
    f.fidc = fidc;
    counting_permit += 2 * counting_permission_pcontrol[fidc];
    f.tid = __msl_get_tid(tid_in);
    f.level = __msl_enter_call(f.tid);
    f.children_base = __msl_child_base(f.tid, f.level);
    __msl_allocate_event(f.tid, f.level, 0, &f.event, &f.counters);
    /* GPU layout: one argument, the correlation id of the call (the same one as the GPU records it caused) */
    f.event->cuda.num_arguments = 1;
    f.event->cuda.num_out_arguments = 0;
    f.event->cuda.num_extra_arguments = 0;
    long long correlation = cb->correlationId;
    __msl_write_records(f.tid, f.level, &correlation, 1);
    f.tsc_mpi_entry = peat_get_time();
    *cb->correlationData = ++t_cupti_open_num; /* the frame of this call for its EXIT callback */
    __DEBUG_EPILOG_GPU_LAYER__
}

static void t_cupti_api_exit(const CUpti_CallbackData *cb)
{
    __DEBUG_PROLOG_GPU_LAYER__
    const int idx = (int)*cb->correlationData - 1;
    if (idx < 0 || idx != t_cupti_open_num - 1) { /* not opened (no free frame) or not the innermost open call */
        __DEBUG_EPILOG_GPU_LAYER__
        return;
    }
    t_cupti_frame &f = t_cupti_open[idx];
    t_cupti_open_num--;
    const long long tsc_mpi_exit = peat_get_time();
    __msl_leave_call(f.tid);
    const short fidc = f.fidc;
    /* no call site: from a CUPTI callback the return addresses are libcupti's, and the frames between it and the caller
       (libcudart, libcupti) have no frame pointers to walk. The record's nesting tells where the call was made. */
    for (int i = 0; i < gt_info(num_addrs); i++)
        f.counters.addrs[i] = 0;
    f.event->func_id = fidc;
    f.event->num_children += (short)(__msl_child_base(f.tid, f.level) - f.children_base);
    f.event->tscs[0] = f.tsc_entry;
    f.event->tscs[1] = f.tsc_mpi_entry;
    const long long tsc_exit = peat_get_time();
    f.event->tscs[2] = tsc_mpi_exit;
    f.event->tscs[3] = tsc_exit;
    __msl_check_chain(f.tid, f.level);
    const long long overhead = f.tsc_mpi_entry - f.tsc_entry + tsc_exit - tsc_mpi_exit;
    global_tool_overhead += overhead;
    global_cuda_time += tsc_mpi_exit - f.tsc_mpi_entry;
#ifdef _TRACTOR_COLLECT_STAT_
    overall_durations[fidc] += tsc_mpi_exit - f.tsc_mpi_entry;
    overall_entries[fidc]++;
    overall_tool_overhead[fidc] += overhead;
#endif
    __DEBUG_EPILOG_GPU_LAYER__
}

static void t_cupti_flush_at_exit(void);

static void CUPTIAPI t_cupti_callback(void *userdata, CUpti_CallbackDomain domain, CUpti_CallbackId cbid, const void *data)
{
    if (domain != CUPTI_CB_DOMAIN_RUNTIME_API || cuda_skip_parse || tractor_finished || gt_info(tractor_initialized) < 1)
        return;
    static bool atexit_registered = false;
    if (!atexit_registered) { /* after the CUDA runtime's own exit handlers: ours runs first, while CUDA is still there */
        atexit_registered = true;
        atexit(t_cupti_flush_at_exit);
    }
    const CUpti_CallbackData *cb = (const CUpti_CallbackData *)data;
    if (cb->callbackSite == CUPTI_API_ENTER) {
        const short fidc = t_runtime_id(cb->functionName);
        if (fidc < 0 || counting_permission[fidc] == 0) {
            *cb->correlationData = 0;
            return;
        }
        t_cupti_api_enter(cb, fidc);
    } else {
        t_cupti_api_exit(cb);
    }
}

/* ---------------------------------------------------------------------------------------------------------------- */
/* GPU activity (activity API)                                                                                       */

/* (context, stream) -> tractor thread of its records */
static std::unordered_map<unsigned long long, int> &t_lanes() { static std::unordered_map<unsigned long long, int> *m = new std::unordered_map<unsigned long long, int>; return *m; }
static std::mutex &t_lanes_mutex() { static std::mutex *m = new std::mutex; return *m; }

/* A lane is a tractor thread no system thread has: the thread index is the offset of the thread id from the process
   id, the lanes take the indices from the top of that range down. */
static int t_lane_tid(unsigned int context, unsigned int stream)
{
    const unsigned long long key = ((unsigned long long)context << 32) | stream;
    std::lock_guard<std::mutex> lock(t_lanes_mutex());
    std::unordered_map<unsigned long long, int>::iterator it = t_lanes().find(key);
    if (it != t_lanes().end())
        return it->second;
    const pid_t fake = gt_info(t_pid) + gt_info(t_max_tid_num) - 1 - (pid_t)t_lanes().size();
    const int tid = __msl_get_tid(fake);
    t_lanes()[key] = tid;
    return tid;
}

static void t_cupti_device_record(short fidc, unsigned int context, unsigned int stream, uint64_t start, uint64_t end,
                                  int name_index, bool gpu_time, const long long *args, int num_args)
{
    if (counting_permission[fidc] == 0 || start == 0 || end < start)
        return;
    const int tid = t_lane_tid(context, stream);
    msl_event *ev = NULL;
    msl_extras_counters counters;
    const int level = __msl_enter_call(tid);
    __msl_allocate_event(tid, level, 0, &ev, &counters);
    for (int i = 0; i < gt_info(num_addrs); i++)
        counters.addrs[i] = 0; /* no host call site */
    ev->func_id = fidc;
    ev->num_children = 0;
    /* GPU layout: correlation id (the host call that caused it), stream, and what the kind has (see the callers) */
    ev->cuda.num_arguments = num_args;
    ev->cuda.num_out_arguments = 0;
    ev->cuda.num_extra_arguments = 0;
    ev->cuda.name_index = name_index;
    __msl_write_records(tid, level, args, num_args);
    ev->tscs[0] = ev->tscs[1] = (PEAT_Clock)start;
    ev->tscs[2] = ev->tscs[3] = (PEAT_Clock)end;
    __msl_leave_call(tid);
    __msl_check_chain(tid, level);
    if (gpu_time)
        global_gpu_time += end - start;
    global_cuda_time += end - start;
#ifdef _TRACTOR_COLLECT_STAT_
    overall_durations[fidc] += end - start;
    overall_entries[fidc]++;
#endif
}

static short t_memcpy_id(uint8_t kind)
{
    switch (kind) {
    case CUPTI_ACTIVITY_MEMCPY_KIND_HTOD:
        return CUDA_MEMCPY_HTOD_ID_THROUGH;
    case CUPTI_ACTIVITY_MEMCPY_KIND_DTOH:
        return CUDA_MEMCPY_DTOH_ID_THROUGH;
    case CUPTI_ACTIVITY_MEMCPY_KIND_DTOD:
        return CUDA_MEMCPY_DTOD_ID_THROUGH;
    case CUPTI_ACTIVITY_MEMCPY_KIND_PTOP:
        return CUDA_MEMCPY_PTOP_ID_THROUGH;
    case CUPTI_ACTIVITY_MEMCPY_KIND_HTOH:
        return CUDA_MEMCPY_HTOH_ID_THROUGH;
    default: /* array copies: as the direction they have */
        return (kind == CUPTI_ACTIVITY_MEMCPY_KIND_HTOA) ? CUDA_MEMCPY_HTOD_ID_THROUGH
             : (kind == CUPTI_ACTIVITY_MEMCPY_KIND_ATOH) ? CUDA_MEMCPY_DTOH_ID_THROUGH
                                                         : CUDA_MEMCPY_DTOD_ID_THROUGH;
    }
}

static short t_sync_id(CUpti_ActivitySynchronizationType type)
{
    switch (type) {
    case CUPTI_ACTIVITY_SYNCHRONIZATION_TYPE_CONTEXT_SYNCHRONIZE:
        return CUDA_SYNC_CONTEXT_ID_THROUGH;
    case CUPTI_ACTIVITY_SYNCHRONIZATION_TYPE_STREAM_SYNCHRONIZE:
        return CUDA_SYNC_STREAM_ID_THROUGH;
    case CUPTI_ACTIVITY_SYNCHRONIZATION_TYPE_EVENT_SYNCHRONIZE:
        return CUDA_SYNC_EVENT_ID_THROUGH;
    case CUPTI_ACTIVITY_SYNCHRONIZATION_TYPE_STREAM_WAIT_EVENT:
        return CUDA_SYNC_STREAM_WAIT_EVENT_ID_THROUGH;
    default:
        return -1;
    }
}

#define T_CUPTI_BUFFER_SIZE (8 * 1024 * 1024)
#define T_CUPTI_NO_STREAM 0xffffffffu /* lane of the device-wide records (context synchronization) */

static void CUPTIAPI t_cupti_buffer_requested(uint8_t **buffer, size_t *size, size_t *max_records)
{
    *buffer = (uint8_t *)aligned_alloc(8, T_CUPTI_BUFFER_SIZE);
    *size = *buffer ? T_CUPTI_BUFFER_SIZE : 0;
    *max_records = 0;
}

static void CUPTIAPI t_cupti_buffer_completed(CUcontext ctx, uint32_t stream_id, uint8_t *buffer, size_t size, size_t valid_size)
{
    const bool record = !cuda_skip_parse && !tractor_finished && gt_info(tractor_initialized) > 0;
    CUpti_Activity *rec = NULL;
    /* Memcpy5, Synchronization and GraphTrace instead of Memcpy6/Synchronization2/GraphTrace2: the newer records only add
       fields at the end, so the older layouts read them too, and those exist in every CUPTI from CUDA 11.8 (API 18, the
       Debian/Ubuntu packages) to 13.x (cupti_activity_deprecated.h). Kernel9 and Memset4 are already in API 18. */
    while (record && cuptiActivityGetNextRecord(buffer, valid_size, &rec) == CUPTI_SUCCESS) {
        switch (rec->kind) {
        case CUPTI_ACTIVITY_KIND_KERNEL:
        case CUPTI_ACTIVITY_KIND_CONCURRENT_KERNEL: {
            const CUpti_ActivityKernel9 *k = (const CUpti_ActivityKernel9 *)rec;
            /* correlation, stream, grid x y z, block x y z, shared memory bytes (static + dynamic) */
            const long long args[] = {k->correlationId, k->streamId, k->gridX, k->gridY, k->gridZ, k->blockX, k->blockY, k->blockZ,
                                      (long long)k->staticSharedMemory + k->dynamicSharedMemory};
            t_cupti_device_record(CUDA_KERNEL_ID_THROUGH, k->contextId, k->streamId, k->start, k->end, t_kernel_name_id(k->name), true,
                                  args, 9);
        } break;
        case CUPTI_ACTIVITY_KIND_MEMCPY: {
            const CUpti_ActivityMemcpy5 *m = (const CUpti_ActivityMemcpy5 *)rec;
            const long long args[] = {m->correlationId, m->streamId, (long long)m->bytes}; /* correlation, stream, bytes */
            t_cupti_device_record(t_memcpy_id(m->copyKind), m->contextId, m->streamId, m->start, m->end, -1, true, args, 3);
        } break;
        case CUPTI_ACTIVITY_KIND_MEMSET: {
            const CUpti_ActivityMemset4 *m = (const CUpti_ActivityMemset4 *)rec;
            const long long args[] = {m->correlationId, m->streamId, (long long)m->bytes}; /* correlation, stream, bytes */
            t_cupti_device_record(CUDA_MEMSET_ID_THROUGH, m->contextId, m->streamId, m->start, m->end, -1, true, args, 3);
        } break;
        case CUPTI_ACTIVITY_KIND_SYNCHRONIZATION: {
            const CUpti_ActivitySynchronization *s = (const CUpti_ActivitySynchronization *)rec;
            const short id = t_sync_id(s->type);
            if (id >= 0) {
                const unsigned int stream = (s->streamId == CUPTI_SYNCHRONIZATION_INVALID_VALUE) ? T_CUPTI_NO_STREAM : s->streamId;
                const long long args[] = {s->correlationId, stream}; /* correlation, stream (ffffffff: device wide) */
                t_cupti_device_record(id, s->contextId, stream, s->start, s->end, -1, false, args, 2);
            }
        } break;
        case CUPTI_ACTIVITY_KIND_GRAPH_TRACE: {
            const CUpti_ActivityGraphTrace *g = (const CUpti_ActivityGraphTrace *)rec;
            const long long args[] = {g->correlationId, g->streamId, g->graphId}; /* correlation, stream, graph id */
            t_cupti_device_record(CUDA_GRAPH_ID_THROUGH, g->contextId, g->streamId, g->start, g->end, -1, true, args, 3);
        } break;
        default:
            break;
        }
    }
    size_t dropped = 0;
    if (cuptiActivityGetNumDroppedRecords(ctx, stream_id, &dropped) == CUPTI_SUCCESS && dropped > 0)
        fprintf(stderr, "tractor: CUPTI dropped %zu activity records\n", dropped);
    free(buffer);
}

static uint64_t CUPTIAPI t_cupti_timestamp(void)
{
    return (uint64_t)peat_get_nano_sec(); /* tractor's own clock (USE_NANO_TIMER) */
}

/* ---------------------------------------------------------------------------------------------------------------- */

static void t_cupti_flush(void)
{
    if (!t_cupti_active || t_cupti_flushed)
        return;
    cuptiActivityFlushAll(CUPTI_ACTIVITY_FLAG_FLUSH_FORCED);
    t_cupti_flushed = true;
}

static void t_cupti_flush_at_exit(void)
{
    t_cupti_flush();
}

/* Called from tractor::init() when PEAT_TRACTOR_PARSE_CUDA=1, before the application initializes CUDA. */
extern "C" void t_cupti_init(void)
{
    if (t_cupti_tried)
        return;
    t_cupti_tried = true;
    for (int i = CUDA_START_GROUP_THROUGH + 1; i < CUDA_END_GROUP_THROUGH; i++) {
        if (mpi_function_names[i] && strncmp(mpi_function_names[i], "cuda", 4) == 0 && strncmp(mpi_function_names[i], "cuda_", 5) != 0)
            t_runtime_ids()[mpi_function_names[i]] = (short)i;
    }
    T_CUPTI_CHECK(cuptiActivityRegisterTimestampCallback(t_cupti_timestamp));
    T_CUPTI_CHECK(cuptiActivityRegisterCallbacks(t_cupti_buffer_requested, t_cupti_buffer_completed));
    T_CUPTI_CHECK(cuptiSubscribe(&t_cupti_subscriber, (CUpti_CallbackFunc)t_cupti_callback, NULL));
    T_CUPTI_CHECK(cuptiEnableDomain(1, t_cupti_subscriber, CUPTI_CB_DOMAIN_RUNTIME_API));
    const char *graphs = getenv("PEAT_TRACTOR_CUDA_GRAPH_TRACE"); /* whole graphs instead of their kernels */
    if (graphs && strcmp(graphs, "1") == 0) {
        T_CUPTI_CHECK(cuptiActivityEnable(CUPTI_ACTIVITY_KIND_GRAPH_TRACE));
    }
    T_CUPTI_CHECK(cuptiActivityEnable(CUPTI_ACTIVITY_KIND_CONCURRENT_KERNEL));
    T_CUPTI_CHECK(cuptiActivityEnable(CUPTI_ACTIVITY_KIND_MEMCPY));
    T_CUPTI_CHECK(cuptiActivityEnable(CUPTI_ACTIVITY_KIND_MEMSET));
    T_CUPTI_CHECK(cuptiActivityEnable(CUPTI_ACTIVITY_KIND_SYNCHRONIZATION));
    t_cupti_active = true;
}

/* Called from the tractor destructor before the trace is written: the GPU records still buffered by CUPTI. */
extern "C" void t_cupti_finalize(void)
{
    t_cupti_flush();
}

/* For the modules remark of the com file: whether CUPTI started (no: not asked for, or failed, e.g. without a driver). */
extern "C" int t_cupti_recording(void)
{
    return t_cupti_active;
}

/* For the modules remark of the com file: the CUDA toolkit the library was built with, e.g. "CUDA 12.9/CUPTI 28". */
extern "C" const char *t_cupti_build_info(void)
{
    static char info[64];
    snprintf(info, sizeof(info), "CUDA %d.%d/CUPTI %d", CUDA_VERSION / 1000, (CUDA_VERSION % 1000) / 10, (int)CUPTI_API_VERSION);
    return info;
}
