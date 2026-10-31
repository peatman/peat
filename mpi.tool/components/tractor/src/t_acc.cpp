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

// OpenACC profiling interface hooks. Records are written like the OpenCL/Level Zero ones (GPU class):
// transfers, executions and synchronisations are told apart by the function (see print_gpu_stat()).

#include "mpi_func_ids.h"
#include "t_defines.h"
#include "t_extradef.h"
#include "tractor.h"
#include <acc_prof.h>
#include <dlfcn.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

extern "C" {

struct t_acc_event_desc {
    acc_event_t event;
    short id;
    short through;
    bool is_start; // the *_start half of a pair
    bool is_end;   // the *_end half of a pair; neither: a point in time
    bool is_data;  // acc_data_event_info follows
    bool is_launch; // acc_launch_event_info follows
};

#define ACC_PAIR_START(ev, name, kind) {acc_ev_##ev##_start, ACC_##name##_ID, ACC_##name##_ID_THROUGH, true, false, kind == 1, kind == 2}
#define ACC_PAIR_END(ev, name, kind) {acc_ev_##ev##_end, ACC_##name##_ID, ACC_##name##_ID_THROUGH, false, true, kind == 1, kind == 2}
#define ACC_POINT(ev, name, kind) {acc_ev_##ev, ACC_##name##_ID, ACC_##name##_ID_THROUGH, false, false, kind == 1, kind == 2}

// kind: 0 - acc_other_event_info, 1 - acc_data_event_info, 2 - acc_launch_event_info
static const t_acc_event_desc t_acc_events[] = {
    ACC_PAIR_START(device_init, DEVICE_INIT, 0),
    ACC_PAIR_END(device_init, DEVICE_INIT, 0),
    ACC_PAIR_START(device_shutdown, DEVICE_SHUTDOWN, 0),
    ACC_PAIR_END(device_shutdown, DEVICE_SHUTDOWN, 0),
    ACC_POINT(runtime_shutdown, RUNTIME_SHUTDOWN, 0),
    ACC_POINT(create, CREATE, 1),
    ACC_POINT(delete, DELETE, 1),
    ACC_POINT(alloc, ALLOC, 1),
    ACC_POINT(free, FREE, 1),
    ACC_PAIR_START(enter_data, ENTER_DATA, 0),
    ACC_PAIR_END(enter_data, ENTER_DATA, 0),
    ACC_PAIR_START(exit_data, EXIT_DATA, 0),
    ACC_PAIR_END(exit_data, EXIT_DATA, 0),
    ACC_PAIR_START(update, UPDATE, 0),
    ACC_PAIR_END(update, UPDATE, 0),
    ACC_PAIR_START(compute_construct, COMPUTE_CONSTRUCT, 0),
    ACC_PAIR_END(compute_construct, COMPUTE_CONSTRUCT, 0),
    ACC_PAIR_START(enqueue_launch, ENQUEUE_LAUNCH, 2),
    ACC_PAIR_END(enqueue_launch, ENQUEUE_LAUNCH, 2),
    ACC_PAIR_START(enqueue_upload, ENQUEUE_UPLOAD, 1),
    ACC_PAIR_END(enqueue_upload, ENQUEUE_UPLOAD, 1),
    ACC_PAIR_START(enqueue_download, ENQUEUE_DOWNLOAD, 1),
    ACC_PAIR_END(enqueue_download, ENQUEUE_DOWNLOAD, 1),
    ACC_PAIR_START(wait, WAIT, 0),
    ACC_PAIR_END(wait, WAIT, 0),
};
static const int t_acc_events_num = sizeof(t_acc_events) / sizeof(t_acc_events[0]);

#define ACC_SLOTS (ACC_END_GROUP - ACC_START_GROUP - 1)

// the *_start record waiting for its *_end, one per function and thread
static msl_event **volatile stored_acc_events[ACC_SLOTS];
static acc_prof_reg t_acc_unreg = NULL;

#define ACC_HAS_FIELD(info, type, field) ((info)->valid_bytes >= (int)(offsetof(type, field) + sizeof(((type *)0)->field)))

static const t_acc_event_desc *find_desc(acc_event_t event)
{
    for (int i = 0; i < t_acc_events_num; i++) {
        if (t_acc_events[i].event == event)
            return &t_acc_events[i];
    }
    return NULL;
}

static msl_event **acc_slots(int id)
{
    const int idx = id - ACC_START_GROUP - 1;
    if (stored_acc_events[idx] == NULL) {
        const int max_threads = gt_info(t_max_threads);
        msl_event **fresh = new msl_event *[max_threads];
        memset(fresh, 0, max_threads * sizeof(msl_event *));
        if (!__sync_bool_compare_and_swap(&stored_acc_events[idx], (msl_event **)NULL, fresh))
            delete[] fresh;
    }
    return stored_acc_events[idx];
}

// Parameters of a record, written like the arguments of an OpenCL/Level Zero call (hex list after the counts):
// device type, parent construct, async queue, bytes, host pointer, device pointer, launch size (gangs*workers*vector length).
// The extras are appended in record order, so they have to be recorded together with the record, i.e. at the *_start.
#define ACC_PARAMS_NUM 7

static void fill_acc_params(long long *params, const t_acc_event_desc *desc, acc_prof_info *prof, acc_event_info *info)
{
    for (int i = 0; i < ACC_PARAMS_NUM; i++)
        params[i] = 0;
    params[1] = -1;
    if (prof) {
        if (ACC_HAS_FIELD(prof, acc_prof_info, device_type))
            params[0] = prof->device_type;
        if (ACC_HAS_FIELD(prof, acc_prof_info, async_queue))
            params[2] = prof->async_queue;
    }
    if (info == NULL)
        return;
    if (ACC_HAS_FIELD(&info->other_event, acc_other_event_info, parent_construct))
        params[1] = info->other_event.parent_construct;
    if (desc->is_data) {
        if (ACC_HAS_FIELD(&info->data_event, acc_data_event_info, bytes))
            params[3] = info->data_event.bytes;
        if (ACC_HAS_FIELD(&info->data_event, acc_data_event_info, host_ptr))
            params[4] = (long long)info->data_event.host_ptr;
        if (ACC_HAS_FIELD(&info->data_event, acc_data_event_info, device_ptr))
            params[5] = (long long)info->data_event.device_ptr;
    }
    if (desc->is_launch && ACC_HAS_FIELD(&info->launch_event, acc_launch_event_info, vector_length)) {
        const long long gangs = info->launch_event.num_gangs ? info->launch_event.num_gangs : 1;
        const long long workers = info->launch_event.num_workers ? info->launch_event.num_workers : 1;
        const long long vector = info->launch_event.vector_length ? info->launch_event.vector_length : 1;
        params[6] = gangs * workers * vector;
    }
}

// A point-in-time record like the OMPT ones: it takes the level of the call it happens in, without opening one.
static msl_event *record_acc_event(short func1, const long long *params)
{
    __DEBUG_PROLOG_ACC_LAYER__
    msl_event *t_event = NULL;
#ifndef _TRACTOR_PASSTHROUGH_
    long long tsc_entry;
    pid_t tid_in;
    int tid;
    const PEAT_MPI_Func_ID_Through fidc = (PEAT_MPI_Func_ID_Through)func1; // used by stack_unwind()
    PEAT_Proc core_id;
    msl_extras_counters counters;
    if (gt_info(tractor_initialized) < 1)
        return NULL;
    tsc_entry = peat_get_time_tid_core(&tid_in, &core_id);
    tid = __msl_get_tid(tid_in);
    const int level = __msl_current_level(tid);
    __msl_allocate_event(tid, level, 0, &t_event, &counters);
    t_event->gpu.num_arguments = ACC_PARAMS_NUM;
    t_event->gpu.num_out_arguments = 0;
    t_event->gpu.num_extra_arguments = 0;
    __msl_record_longs(tid, level, params, ACC_PARAMS_NUM);
    if (gt_info(full_ompt_unwind)) {
        stack_unwind(gt_info(max_ret_level), counters.addrs, gt_info(ret_levels));
    } else {
        stack_unwind0(gt_info(max_ret_level), counters.addrs, gt_info(ret_levels));
    }
    t_event->func_id = func1;
    t_event->tscs[0] = tsc_entry;
    t_event->tscs[2] = tid;
    t_event->tscs[1] = peat_get_time_core(&core_id);
#endif
    __DEBUG_EPILOG_ACC_LAYER__
    return t_event;
}

extern PEAT_Clock global_gpu_time;

// only the events that do not contain other ones count into the GPU time, otherwise it would be counted twice
static void count_acc_event(msl_event *event, const t_acc_event_desc *desc)
{
    const short through = desc->through;
    if (desc->id == ACC_ENQUEUE_LAUNCH_ID || desc->id == ACC_ENQUEUE_UPLOAD_ID || desc->id == ACC_ENQUEUE_DOWNLOAD_ID || desc->id == ACC_WAIT_ID)
        global_gpu_time += event->tscs[2] - event->tscs[1];
#ifdef _TRACTOR_COLLECT_STAT_
    overall_durations[through] += event->tscs[2] - event->tscs[1];
    overall_entries[through]++;
    overall_tool_overhead[through] += event->tscs[1] - event->tscs[0] + event->tscs[3] - event->tscs[2];
#endif
}

static void t_acc_callback(acc_prof_info *prof, acc_event_info *info, acc_api_info *api)
{
    __DEBUG_PROLOG_ACC_LAYER__
    const t_acc_event_desc *desc = prof ? find_desc(prof->event_type) : NULL;
    if (desc == NULL) {
        __DEBUG_EPILOG_ACC_LAYER__
        return;
    }
    msl_event *event = NULL;
    if (desc->is_end) {
        pid_t tid_in;
        PEAT_Proc core_id;
        const long long tsc_entry = peat_get_time_tid_core(&tid_in, &core_id);
        const int tid = (int)__msl_get_tid(tid_in);
        if (gt_info(tractor_initialized) < 1 || tid >= gt_info(t_max_threads)) {
            __DEBUG_EPILOG_ACC_LAYER__
            return;
        }
        msl_event **slots = acc_slots(desc->id);
        event = slots[tid];
        slots[tid] = NULL;
        if (event) {
            event->tscs[2] = tsc_entry;
            event->tscs[3] = peat_get_time_core(&core_id);
            count_acc_event(event, desc);
        } else {
            debug_printf("Null Event returned for acc event %d and tid %d\n", (int)desc->event, tid);
        }
    } else {
        long long params[ACC_PARAMS_NUM];
        fill_acc_params(params, desc, prof, info);
        event = record_acc_event(desc->through, params);
        if (event) {
            if (desc->is_start) {
                const int tid = (int)event->tscs[2];
                if (tid < gt_info(t_max_threads))
                    acc_slots(desc->id)[tid] = event;
            } else {
                event->tscs[2] = event->tscs[3] = event->tscs[1];
                count_acc_event(event, desc);
            }
        }
    }
    __DEBUG_EPILOG_ACC_LAYER__
}

static void t_acc_register_all(acc_prof_reg reg, acc_prof_reg unreg)
{
    if (t_acc_unreg != NULL) // already registered, the runtime must not get the callbacks twice
        return;
    t_acc_unreg = unreg;
    for (int i = 0; i < t_acc_events_num; i++) {
        reg(t_acc_events[i].event, t_acc_callback, acc_reg);
    }
}

// OMPT finds its tool by the ompt_start_tool symbol, the OpenACC runtimes (libgomp, NVHPC) only look for
// acc_register_library() in the libraries listed in ACC_PROFLIB. The application's runtime is initialized
// before this preloaded library, so it may already have read that variable:
//  - the runtime is not loaded yet: ACC_PROFLIB gets this library added and the runtime calls acc_register_library();
//  - the runtime is loaded: its acc_prof_register() is used directly.
__attribute__((constructor)) static void t_acc_autoload()
{
    const char *off = getenv("PEAT_TRACTOR_PARSE_OPENACC");
    if (off && strcmp(off, "1") != 0 && strcmp(off, "yes") != 0 && strcmp(off, "on") != 0) // same test as is_true() in tractor.cpp
        return;
    Dl_info self;
    if (dladdr((void *)&acc_register_library, &self) && self.dli_fname != NULL) {
        const char *current = getenv("ACC_PROFLIB");
        if (current == NULL || *current == 0) {
            setenv("ACC_PROFLIB", self.dli_fname, 1);
        } else if (!strstr(current, self.dli_fname)) {
            char *list = new char[strlen(current) + strlen(self.dli_fname) + 2];
            sprintf(list, "%s:%s", current, self.dli_fname);
            setenv("ACC_PROFLIB", list, 1);
            delete[] list;
        }
    }
    acc_prof_reg reg = (acc_prof_reg)dlsym(RTLD_DEFAULT, "acc_prof_register");
    acc_prof_reg unreg = (acc_prof_reg)dlsym(RTLD_DEFAULT, "acc_prof_unregister");
    if (reg && unreg)
        t_acc_register_all(reg, unreg);
}

// Entry point of the profiling interface: called by the OpenACC runtime once, when it initializes.
void acc_register_library(acc_prof_reg reg, acc_prof_reg unreg, acc_prof_lookup_func lookup)
{
    __DEBUG_PROLOG_ACC_LAYER__
    debug_printf("acc_register_library started...\n");
    if (!gt_info(parse_openacc))
        return;
    t_acc_register_all(reg, unreg);
    debug_printf("acc_register_library finished...\n");
    __DEBUG_EPILOG_ACC_LAYER__
}

// Called from the tractor destructor: the runtime may still raise shutdown events after the trace is written.
void t_acc_finalize()
{
    if (t_acc_unreg == NULL)
        return;
    for (int i = 0; i < t_acc_events_num; i++) {
        t_acc_unreg(t_acc_events[i].event, t_acc_callback, acc_reg);
    }
    t_acc_unreg = NULL;
}
}
