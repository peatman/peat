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

//

#include "mpi_func_ids.h"
#include "t_defines.h"
#include "t_extradef.h"
#include "tractor.h"
#include <omp-tools.h>
extern "C" {

#define ompt_data_value(x) x == NULL ? -1 : x->value

msl_event stored_section_events;
static int begins = 0;
static long long uniq_id = 0;
static long long uniq_task_id = 0;
static long long uniq_implicit_task_id = 0;
// static ompt_get_record_ompt ompt_get_record;
static msl_event *record_ompt_event(short func0, short func1, const void *pcode)
{
    __DEBUG_PROLOG_OMPT_LAYER__
    msl_event *t_event = NULL;
#ifndef _TRACTOR_PASSTHROUGH_
    long long tsc_entry;
#ifdef USE_NANO_TIMER
    long long nsec_entry;
#endif
    pid_t tid_in;
    int tid;
    const PEAT_MPI_Func_ID_Through fidc = (PEAT_MPI_Func_ID_Through)func1;
    PEAT_Proc core_id;
    msl_extras_counters counters;
    if (gt_info(tractor_initialized) < 1)
        return;
#ifdef USE_NANO_TIMER
    nsec_entry = peat_get_nano_sec();
#endif
    tsc_entry = rdtscptca(&tid_in, &core_id);
    tid = __msl_get_tid(tid_in);
    __msl_allocate_event(tid, 0, &t_event, &counters);
    if (gt_info(full_ompt_unwind)) {
        stack_unwind(gt_info(max_ret_level), counters.addrs, gt_info(ret_levels));
    } else {
        stack_unwind0(gt_info(max_ret_level), counters.addrs, gt_info(ret_levels));
    }
    t_event->func_id[0] = func0;
    t_event->func_id[1] = func1;
#ifdef USE_NANO_TIMER
    t_event->tscs[0] = nsec_entry;
#else
    t_event->tscs[0] = tsc_entry;
#endif
    t_event->tscs[3] = tsc_entry;
    t_event->tscs[2] = tid;
    t_event->ompt.pcode = (PEAT_Addr)pcode;
    t_event->tscs[1] = rdtscca(&core_id) - tsc_entry;
#endif
    __DEBUG_EPILOG_OMPT_LAYER__
    return t_event;
}

static void t_ompt_callback_parallel_begin(ompt_data_t *encountering_task_data,
                                           const ompt_frame_t *encountering_task_frame,
                                           ompt_data_t *parallel_data,
                                           unsigned int requested_parallelism,
                                           int flags,
                                           const void *codeptr_ra)
{
    __DEBUG_PROLOG_OMPT_LAYER__
    msl_event *event = record_ompt_event(OMPT_PARALLEL_BEGIN_ID, OMPT_PARALLEL_BEGIN_ID_THROUGH, codeptr_ra);
    if (event && begins == 0) {
        stored_section_events = *event;
        debug_printf("New parallel section started. Event  %p\n", event);
    }
    if (event) {
        event->tscs[2] = event->tscs[3] = event->tscs[1];
        if (begins > 0)
            debug_printf("New nested parallel section started. Event  %p\n", event);
    }
    ++begins;
    if (parallel_data)
        parallel_data->value = uniq_id++;
    if (event) {
        event->ompt.task_data = ompt_data_value(encountering_task_data);
        event->ompt.parallel_data = ompt_data_value(parallel_data);
    }
    __DEBUG_EPILOG_OMPT_LAYER__
}
static void t_ompt_callback_parallel_end(ompt_data_t *parallel_data,
                                         ompt_data_t *encountering_task_data,
                                         int flags,
                                         const void *codeptr_ra)
{
    __DEBUG_PROLOG_OMPT_LAYER__
    msl_event *event = record_ompt_event(OMPT_PARALLEL_END_ID, OMPT_PARALLEL_END_ID_THROUGH, codeptr_ra);
    --begins;
    if (event && begins == 0) {
        debug_printf("Parallel section ended. Event  %p \n", event);
#ifdef _TRACTOR_COLLECT_STAT_
        overall_durations[OMPT_PARALLEL_ID_THROUGH] += event->tscs[3] - (stored_section_events.tscs[3] + stored_section_events.tscs[1]);
        overall_entries[OMPT_PARALLEL_ID_THROUGH]++;
        overall_tool_overhead[OMPT_PARALLEL_ID_THROUGH] += event->tscs[1] + stored_section_events.tscs[1];
#endif
    }
    if (event) {
        event->tscs[2] = event->tscs[3] = event->tscs[1];
        event->ompt.task_data = ompt_data_value(encountering_task_data);
        event->ompt.parallel_data = ompt_data_value(parallel_data);
        if (begins > 0)
            debug_printf("Nested parallel section ended. Event  %p\n", event);
    }
    __DEBUG_EPILOG_OMPT_LAYER__
}

static void t_ompt_callback_thread_begin(ompt_thread_t thread_type, ompt_data_t *thread_data)
{
    __DEBUG_PROLOG_OMPT_LAYER__
    msl_event *event = record_ompt_event(OMPT_THREAD_BEGIN_ID, OMPT_THREAD_BEGIN_ID_THROUGH, NULL);
    if (event) {
        if (thread_data)
            thread_data->value = event->tscs[2]; // tid
        event->tscs[2] = event->tscs[3] = event->tscs[1];
        event->ompt.task_data = ompt_data_value(thread_data);
#ifdef _TRACTOR_COLLECT_STAT_
        overall_durations[OMPT_THREAD_BEGIN_ID_THROUGH] += event->tscs[2] - event->tscs[1];
        overall_entries[OMPT_THREAD_BEGIN_ID_THROUGH]++;
        overall_tool_overhead[OMPT_THREAD_BEGIN_ID_THROUGH] += event->tscs[1] + event->tscs[3] - event->tscs[2];
#endif
    }
    __DEBUG_EPILOG_OMPT_LAYER__
}

static void t_ompt_callback_thread_end(ompt_data_t *thread_data)
{
    __DEBUG_PROLOG_OMPT_LAYER__
    msl_event *event = record_ompt_event(OMPT_THREAD_END_ID, OMPT_THREAD_END_ID_THROUGH, NULL);
    if (event) {
        event->tscs[2] = event->tscs[3] = event->tscs[1];
        event->ompt.task_data = ompt_data_value(thread_data);
#ifdef _TRACTOR_COLLECT_STAT_
        overall_durations[OMPT_THREAD_END_ID_THROUGH] += event->tscs[2] - event->tscs[1];
        overall_entries[OMPT_THREAD_END_ID_THROUGH]++;
        overall_tool_overhead[OMPT_THREAD_END_ID_THROUGH] += event->tscs[1] + event->tscs[3] - event->tscs[2];
#endif
    }
    __DEBUG_EPILOG_OMPT_LAYER__
}

static void t_ompt_callback_target_data_op_emi(
    ompt_scope_endpoint_t endpoint, ompt_data_t *target_task_data,
    ompt_data_t *target_data, ompt_id_t *host_op_id,
    ompt_target_data_op_t optype, void *src_addr,
    int src_device_num, void *dest_addr,
    int dest_device_num, size_t bytes,
    const void *codeptr_ra)
{
    __DEBUG_PROLOG_OMPT_LAYER__
    msl_event *event = record_ompt_event(OMPT_TARGET_DATA_OP_EMI_ID, OMPT_TARGET_DATA_OP_EMI_ID_THROUGH, codeptr_ra);
    if (event) {
        event->tscs[2] = event->tscs[3] = event->tscs[1];
        event->ompt.endpoint = endpoint;
        event->ompt.task_data = ompt_data_value(target_task_data);
        event->ompt.parallel_data = ompt_data_value(target_data);
#ifdef _TRACTOR_COLLECT_STAT_
        overall_durations[OMPT_TARGET_DATA_OP_EMI_ID_THROUGH] += event->tscs[2] - event->tscs[1];
        overall_entries[OMPT_TARGET_DATA_OP_EMI_ID_THROUGH]++;
        overall_tool_overhead[OMPT_TARGET_DATA_OP_EMI_ID_THROUGH] += event->tscs[1] + event->tscs[3] - event->tscs[2];
#endif
    }
    __DEBUG_EPILOG_OMPT_LAYER__
}

static void t_ompt_callback_target_data_op(
    ompt_id_t target_id, ompt_id_t host_op_id,
    ompt_target_data_op_t optype, void *src_addr,
    int src_device_num, void *dest_addr,
    int dest_device_num, size_t bytes,
    const void *codeptr_ra)
{
    __DEBUG_PROLOG_OMPT_LAYER__
    msl_event *event = record_ompt_event(OMPT_TARGET_DATA_OP_ID, OMPT_TARGET_DATA_OP_ID_THROUGH, codeptr_ra);
    if (event) {
        event->tscs[2] = event->tscs[3] = event->tscs[1];
#ifdef _TRACTOR_COLLECT_STAT_
        overall_durations[OMPT_TARGET_DATA_OP_ID_THROUGH] += event->tscs[2] - event->tscs[1];
        overall_entries[OMPT_TARGET_DATA_OP_ID_THROUGH]++;
        overall_tool_overhead[OMPT_TARGET_DATA_OP_ID_THROUGH] += event->tscs[1] + event->tscs[3] - event->tscs[2];
#endif
    }
    __DEBUG_EPILOG_OMPT_LAYER__
}

static void t_ompt_callback_target_emi(
    ompt_target_t kind, ompt_scope_endpoint_t endpoint,
    int device_num, ompt_data_t *task_data,
    ompt_data_t *target_task_data, ompt_data_t *target_data,
    const void *codeptr_ra)
{
    __DEBUG_PROLOG_OMPT_LAYER__
    msl_event *event = record_ompt_event(OMPT_TARGET_EMI_ID, OMPT_TARGET_EMI_ID_THROUGH, codeptr_ra);
    if (event) {
        event->tscs[2] = event->tscs[3] = event->tscs[1];
        event->ompt.kind = kind;
        event->ompt.endpoint = endpoint;
        event->ompt.task_data = ompt_data_value(target_task_data);
        event->ompt.parallel_data = ompt_data_value(target_data);
#ifdef _TRACTOR_COLLECT_STAT_
        overall_durations[OMPT_TARGET_EMI_ID_THROUGH] += event->tscs[2] - event->tscs[1];
        overall_entries[OMPT_TARGET_EMI_ID_THROUGH]++;
        overall_tool_overhead[OMPT_TARGET_EMI_ID_THROUGH] += event->tscs[1] + event->tscs[3] - event->tscs[2];
#endif
    }
    __DEBUG_EPILOG_OMPT_LAYER__
}

static void t_ompt_callback_target(
    ompt_target_t kind, ompt_scope_endpoint_t endpoint,
    int device_num, ompt_data_t *task_data,
    ompt_id_t target_id, const void *codeptr_ra)
{
    __DEBUG_PROLOG_OMPT_LAYER__
    msl_event *event = record_ompt_event(OMPT_TARGET_ID, OMPT_TARGET_ID_THROUGH, codeptr_ra);
    if (event) {
        event->tscs[2] = event->tscs[3] = event->tscs[1];
        event->ompt.kind = kind;
        event->ompt.endpoint = endpoint;
        event->ompt.task_data = ompt_data_value(task_data);
        event->ompt.parallel_data = ompt_data_value(task_data);
#ifdef _TRACTOR_COLLECT_STAT_
        overall_durations[OMPT_TARGET_ID_THROUGH] += event->tscs[2] - event->tscs[1];
        overall_entries[OMPT_TARGET_ID_THROUGH]++;
        overall_tool_overhead[OMPT_TARGET_ID_THROUGH] += event->tscs[1] + event->tscs[3] - event->tscs[2];
#endif
    }
    __DEBUG_EPILOG_OMPT_LAYER__
}

static void t_ompt_callback_target_map_emi(
    ompt_data_t *target_data, unsigned int nitems,
    void **host_addr, void **device_addr,
    size_t *bytes, unsigned int *mapping_flags,
    const void *codeptr_ra)
{
    __DEBUG_PROLOG_OMPT_LAYER__
    msl_event *event = record_ompt_event(OMPT_TARGET_MAP_EMI_ID, OMPT_TARGET_MAP_EMI_ID_THROUGH, codeptr_ra);
    if (event) {
        event->tscs[2] = event->tscs[3] = event->tscs[1];
        event->ompt.task_data = ompt_data_value(target_data);
        event->ompt.parallel_data = ompt_data_value(target_data);
#ifdef _TRACTOR_COLLECT_STAT_
        overall_durations[OMPT_TARGET_MAP_EMI_ID_THROUGH] += event->tscs[2] - event->tscs[1];
        overall_entries[OMPT_TARGET_MAP_EMI_ID_THROUGH]++;
        overall_tool_overhead[OMPT_TARGET_MAP_EMI_ID_THROUGH] += event->tscs[1] + event->tscs[3] - event->tscs[2];
#endif
    }
    __DEBUG_EPILOG_OMPT_LAYER__
}

static void t_ompt_callback_target_map(
    ompt_id_t target_id, unsigned int nitems,
    void **host_addr, void **device_addr,
    size_t *bytes, unsigned int *mapping_flags,
    const void *codeptr_ra)
{
    __DEBUG_PROLOG_OMPT_LAYER__
    msl_event *event = record_ompt_event(OMPT_TARGET_MAP_ID, OMPT_TARGET_MAP_ID_THROUGH, codeptr_ra);
    if (event) {
        event->tscs[2] = event->tscs[3] = event->tscs[1];
#ifdef _TRACTOR_COLLECT_STAT_
        overall_durations[OMPT_TARGET_MAP_ID_THROUGH] += event->tscs[2] - event->tscs[1];
        overall_entries[OMPT_TARGET_MAP_ID_THROUGH]++;
        overall_tool_overhead[OMPT_TARGET_MAP_ID_THROUGH] += event->tscs[1] + event->tscs[3] - event->tscs[2];
#endif
    }
    __DEBUG_EPILOG_OMPT_LAYER__
}

#define MAX_SYNC_REGIONS 11
msl_event **stored_sync_events[MAX_SYNC_REGIONS];
msl_event **stored_sync_events_wait[MAX_SYNC_REGIONS];

static void store_event(msl_event **stored_events[MAX_SYNC_REGIONS], ompt_sync_region_t kind, ompt_scope_endpoint_t endpoint, msl_event *event)
{
    int tid = event->tscs[2];
    debug_printf("Storing event %p of kind %d endpoint %d for tid %d\n", event, kind, endpoint, tid);
    if (stored_events[kind][tid]) {
        debug_printf("Unretrived event %p of kind %d endpoint %d and tid %d found.\n", stored_events[kind][tid], endpoint, kind, tid);
    }
    stored_events[kind][tid] = event;
}

static msl_event *retrieve_event(msl_event **stored_events[MAX_SYNC_REGIONS], ompt_sync_region_t kind, ompt_scope_endpoint_t endpoint, int tid)
{
    msl_event *event = stored_events[kind][tid];
    stored_events[kind][tid] = NULL;
    debug_printf("Retrieving event %p of kind %d endpoint %d for tid %d\n", event, kind, endpoint, tid);
    return event;
}

static void t_ompt_callback_sync_region(
    ompt_sync_region_t kind,
    ompt_scope_endpoint_t endpoint,
    ompt_data_t *parallel_data,
    ompt_data_t *task_data,
    const void *codeptr_ra)
{
    __DEBUG_PROLOG_OMPT_LAYER__
    msl_event *event = NULL;
    pid_t tid_in;
    PEAT_Proc core_id;
    // if (kind==ompt_sync_region_barrier_implicit_workshare || kind==ompt_sync_region_barrier_implicit_parallel) return; // no need these 'trash' callbacks
    long long tsc_entry = rdtscptca(&tid_in, &core_id);
#if 0
        event=record_ompt_event(OMPT_SYNC_REGION_ID,OMPT_SYNC_REGION_ID_THROUGH,codeptr_ra);
        if (endpoint==ompt_scope_begin) {
            if (event) event->ompt.kind=kind;
        }
        if (endpoint==ompt_scope_end) {
            if (event) event->ompt.kind=-kind;
        }
#else
    if (endpoint == ompt_scope_begin || endpoint == ompt_scope_beginend) {
        event = record_ompt_event(OMPT_SYNC_REGION_ID, OMPT_SYNC_REGION_ID_THROUGH, codeptr_ra);
        if (event)
            event->ompt.kind = kind;
        event->ompt.endpoint = endpoint;
        if (event && endpoint == ompt_scope_beginend) {
            event->ompt.task_data = ompt_data_value(task_data);
            event->ompt.parallel_data = ompt_data_value(parallel_data);
            event->tscs[2] = tsc_entry - event->tscs[3];
            event->tscs[3] = rdtscca(&core_id) - event->tscs[3];
#ifdef _TRACTOR_COLLECT_STAT_
            overall_durations[OMPT_SYNC_REGION_ID_THROUGH] += event->tscs[2] - event->tscs[1];
            overall_entries[OMPT_SYNC_REGION_ID_THROUGH]++;
            overall_tool_overhead[OMPT_SYNC_REGION_ID_THROUGH] += event->tscs[1] + event->tscs[3] - event->tscs[2];
#endif
        }
    }
    if (endpoint == ompt_scope_begin) {
        if (event) {
            store_event(stored_sync_events, kind, endpoint, event);
        }
    }
    if (endpoint == ompt_scope_end) {
        int tid = (int)__msl_get_tid(tid_in);
        event = retrieve_event(stored_sync_events, kind, endpoint, tid);
        if (event) {
            event->ompt.task_data = ompt_data_value(task_data);
            event->ompt.parallel_data = ompt_data_value(parallel_data);
            event->tscs[2] = tsc_entry - event->tscs[3];
            event->tscs[3] = rdtscca(&core_id) - event->tscs[3];
#ifdef _TRACTOR_COLLECT_STAT_
            overall_durations[OMPT_SYNC_REGION_ID_THROUGH] += event->tscs[2] - event->tscs[1];
            overall_entries[OMPT_SYNC_REGION_ID_THROUGH]++;
            overall_tool_overhead[OMPT_SYNC_REGION_ID_THROUGH] += event->tscs[1] + event->tscs[3] - event->tscs[2];
#endif
        } else {
            debug_printf("Null Event returned for kind %d and tid %d\n", kind, tid);
        }
    }
#endif
    __DEBUG_EPILOG_OMPT_LAYER__
}

static void t_ompt_callback_sync_region_wait(
    ompt_sync_region_t kind,
    ompt_scope_endpoint_t endpoint,
    ompt_data_t *parallel_data,
    ompt_data_t *task_data,
    const void *codeptr_ra)
{
    __DEBUG_PROLOG_OMPT_LAYER__
    msl_event *event = NULL;
    pid_t tid_in;
    PEAT_Proc core_id;
    // if (kind==ompt_sync_region_barrier_implicit_workshare || kind==ompt_sync_region_barrier_implicit_parallel) return; // no need these 'trash' callbacks
    long long tsc_entry = rdtscptca(&tid_in, &core_id);
#if 0
        event=record_ompt_event(OMPT_SYNC_REGION_ID,OMPT_SYNC_REGION_ID_THROUGH,codeptr_ra);
        if (endpoint==ompt_scope_begin) {
            if (event) event->ompt.kind=kind;
        }
        if (endpoint==ompt_scope_end) {
            if (event) event->ompt.kind=-kind;
        }
#else
    if (endpoint == ompt_scope_begin || endpoint == ompt_scope_beginend) {
        event = record_ompt_event(OMPT_SYNC_REGION_WAIT_ID, OMPT_SYNC_REGION_WAIT_ID_THROUGH, codeptr_ra);
        if (event)
            event->ompt.kind = kind;
        event->ompt.endpoint = endpoint;
        if (event && endpoint == ompt_scope_beginend) {
            event->ompt.task_data = ompt_data_value(task_data);
            event->ompt.parallel_data = ompt_data_value(parallel_data);
            event->tscs[2] = tsc_entry - event->tscs[3];
            event->tscs[3] = rdtscca(&core_id) - event->tscs[3];
#ifdef _TRACTOR_COLLECT_STAT_
            overall_durations[OMPT_SYNC_REGION_WAIT_ID_THROUGH] += event->tscs[2] - event->tscs[1];
            overall_entries[OMPT_SYNC_REGION_WAIT_ID_THROUGH]++;
            overall_tool_overhead[OMPT_SYNC_REGION_WAIT_ID_THROUGH] += event->tscs[1] + event->tscs[3] - event->tscs[2];
#endif
        }
    }
    if (endpoint == ompt_scope_begin) {
        if (event) {
            store_event(stored_sync_events_wait, kind, endpoint, event);
        }
    }
    if (endpoint == ompt_scope_end) {
        int tid = (int)__msl_get_tid(tid_in);
        event = retrieve_event(stored_sync_events_wait, kind, endpoint, tid);
        if (event) {
            event->ompt.task_data = ompt_data_value(task_data);
            event->ompt.parallel_data = ompt_data_value(parallel_data);
            event->tscs[2] = tsc_entry - event->tscs[3];
            event->tscs[3] = rdtscca(&core_id) - event->tscs[3];
#ifdef _TRACTOR_COLLECT_STAT_
            overall_durations[OMPT_SYNC_REGION_WAIT_ID_THROUGH] += event->tscs[2] - event->tscs[1];
            overall_entries[OMPT_SYNC_REGION_WAIT_ID_THROUGH]++;
            overall_tool_overhead[OMPT_SYNC_REGION_WAIT_ID_THROUGH] += event->tscs[1] + event->tscs[3] - event->tscs[2];
#endif
        } else {
            debug_printf("Null Event returned for kind %d and tid %d\n", kind, tid);
        }
    }
#endif
    __DEBUG_EPILOG_OMPT_LAYER__
}

static void t_ompt_callback_task_create(
    ompt_data_t *encountering_task_data,
    const ompt_frame_t *encountering_task_frame,
    ompt_data_t *new_task_data,
    int flags,
    int has_dependences,
    const void *codeptr_ra)
{
    __DEBUG_PROLOG_OMPT_LAYER__
    msl_event *event = record_ompt_event(OMPT_TASK_CREATE_ID, OMPT_TASK_CREATE_ID_THROUGH, codeptr_ra);
    if (new_task_data)
        new_task_data->value = uniq_task_id++;
    if (event) {
        event->tscs[2] = event->tscs[3] = event->tscs[1];
        event->ompt.task_data = ompt_data_value(encountering_task_data);
        event->ompt.parallel_data = ompt_data_value(new_task_data);
#ifdef _TRACTOR_COLLECT_STAT_
        overall_durations[OMPT_TASK_CREATE_ID_THROUGH] += event->tscs[2] - event->tscs[1];
        overall_entries[OMPT_TASK_CREATE_ID_THROUGH]++;
        overall_tool_overhead[OMPT_TASK_CREATE_ID_THROUGH] += event->tscs[1] + event->tscs[3] - event->tscs[2];
#endif
    }
    __DEBUG_EPILOG_OMPT_LAYER__
}

static void t_ompt_callback_dependences(
    ompt_data_t *task_data,
    const ompt_dependence_t *deps,
    int ndeps)
{
    __DEBUG_PROLOG_OMPT_LAYER__
    msl_event *event = record_ompt_event(OMPT_DEPENDENCES_ID, OMPT_DEPENDENCES_ID_THROUGH, NULL);
    if (task_data)
        task_data->value = uniq_id++;
    if (event) {
        event->tscs[2] = event->tscs[3] = event->tscs[1];
        event->ompt.task_data = ompt_data_value(task_data);
        event->ompt.parallel_data = ompt_data_value(task_data);
#ifdef _TRACTOR_COLLECT_STAT_
        overall_durations[OMPT_DEPENDENCES_ID_THROUGH] += event->tscs[2] - event->tscs[1];
        overall_entries[OMPT_DEPENDENCES_ID_THROUGH]++;
        overall_tool_overhead[OMPT_DEPENDENCES_ID_THROUGH] += event->tscs[1] + event->tscs[3] - event->tscs[2];
#endif
    }
    __DEBUG_EPILOG_OMPT_LAYER__
}

static void t_ompt_callback_task_dependence(
    ompt_data_t *src_task_data,
    ompt_data_t *sink_task_data)

{
    __DEBUG_PROLOG_OMPT_LAYER__
    msl_event *event = record_ompt_event(OMPT_TASK_DEPENDENCE_ID, OMPT_TASK_DEPENDENCE_ID_THROUGH, NULL);
    if (event) {
        event->tscs[2] = event->tscs[3] = event->tscs[1];
        event->ompt.task_data = ompt_data_value(src_task_data);
        event->ompt.parallel_data = ompt_data_value(sink_task_data);
#ifdef _TRACTOR_COLLECT_STAT_
        overall_durations[OMPT_TASK_DEPENDENCE_ID_THROUGH] += event->tscs[2] - event->tscs[1];
        overall_entries[OMPT_TASK_DEPENDENCE_ID_THROUGH]++;
        overall_tool_overhead[OMPT_TASK_DEPENDENCE_ID_THROUGH] += event->tscs[1] + event->tscs[3] - event->tscs[2];
#endif
    }
    __DEBUG_EPILOG_OMPT_LAYER__
}

static void t_ompt_callback_task_schedule(
    ompt_data_t *prior_task_data,
    ompt_task_status_t prior_task_status,
    ompt_data_t *next_task_data)
{
    __DEBUG_PROLOG_OMPT_LAYER__
    msl_event *event = record_ompt_event(OMPT_TASK_SCHEDULE_ID, OMPT_TASK_SCHEDULE_ID_THROUGH, NULL);
    if (event) {
        event->tscs[2] = event->tscs[3] = event->tscs[1];
        event->ompt.task_data = ompt_data_value(prior_task_data);
        event->ompt.parallel_data = ompt_data_value(next_task_data);
#ifdef _TRACTOR_COLLECT_STAT_
        overall_durations[OMPT_TASK_SCHEDULE_ID_THROUGH] += event->tscs[2] - event->tscs[1];
        overall_entries[OMPT_TASK_SCHEDULE_ID_THROUGH]++;
        overall_tool_overhead[OMPT_TASK_SCHEDULE_ID_THROUGH] += event->tscs[1] + event->tscs[3] - event->tscs[2];
#endif
    }
    __DEBUG_EPILOG_OMPT_LAYER__
}

static void t_ompt_callback_implicit_task(
    ompt_scope_endpoint_t endpoint,
    ompt_data_t *parallel_data,
    ompt_data_t *task_data,
    unsigned int actual_parallelism,
    unsigned int index,
    int flags)
{
    __DEBUG_PROLOG_OMPT_LAYER__
    msl_event *event = record_ompt_event(OMPT_IMPLICIT_TASK_ID, OMPT_IMPLICIT_TASK_ID_THROUGH, NULL);
    if (event) {
        event->tscs[2] = event->tscs[3] = event->tscs[1];
        event->ompt.kind = actual_parallelism;
        event->ompt.endpoint = endpoint;
        if (endpoint == ompt_scope_begin || endpoint == ompt_scope_beginend) {
            if (task_data)
                task_data->value = uniq_implicit_task_id++;
        }
        event->ompt.task_data = ompt_data_value(task_data);
        event->ompt.parallel_data = ompt_data_value(parallel_data);
#ifdef _TRACTOR_COLLECT_STAT_
        overall_durations[OMPT_IMPLICIT_TASK_ID_THROUGH] += event->tscs[2] - event->tscs[1];
        overall_entries[OMPT_IMPLICIT_TASK_ID_THROUGH]++;
        overall_tool_overhead[OMPT_IMPLICIT_TASK_ID_THROUGH] += event->tscs[1] + event->tscs[3] - event->tscs[2];
#endif
    }
    __DEBUG_EPILOG_OMPT_LAYER__
}

ompt_initialize_t t_init;
ompt_start_tool_result_t t_ompt_start;
ompt_finalize_tool_t ompt_finalize_tool;

int t_ompt_init(ompt_function_lookup_t lookup, int initial_device_num, ompt_data_t *tool_data)
{
    __DEBUG_PROLOG_OMPT_LAYER__
    int successes = 0;
    debug_printf("ompt_init started...\n");

    ompt_set_callback_t ompt_set_callback = (ompt_set_callback_t)lookup("ompt_set_callback");
    ompt_finalize_tool = (ompt_finalize_tool_t)lookup("ompt_finalize_tool");
    // ompt_get_record = (ompt_get_record_ompt) lookup("ompt_get_record_ompt");
    if (ompt_set_callback(ompt_callback_parallel_begin, (ompt_callback_t)t_ompt_callback_parallel_begin) == ompt_set_never) {
        debug_printf("set_callbalck failed...ompt_callback_parallel_begin\n");
    } else
        successes++;

    if (ompt_set_callback(ompt_callback_parallel_end, (ompt_callback_t)t_ompt_callback_parallel_end) == ompt_set_never) {
        debug_printf("set_callbalck failed...ompt_callback_parallel_end\n");
    } else
        successes++;

    if (ompt_set_callback(ompt_callback_thread_begin, (ompt_callback_t)t_ompt_callback_thread_begin) == ompt_set_never) {
        debug_printf("set_callbalck failed...ompt_callback_thread_begin\n");
    } else
        successes++;

    if (ompt_set_callback(ompt_callback_thread_end, (ompt_callback_t)t_ompt_callback_thread_end) == ompt_set_never) {
        debug_printf("set_callbalck failed...ompt_callback_thread_end\n");
    } else
        successes++;

    if (ompt_set_callback(ompt_callback_target_data_op_emi, (ompt_callback_t)t_ompt_callback_target_data_op_emi) == ompt_set_never) {
        debug_printf("set_callbalck failed...ompt_callback_target_data_op_emi\n");
    } else
        successes++;

    if (ompt_set_callback(ompt_callback_target_data_op, (ompt_callback_t)t_ompt_callback_target_data_op) == ompt_set_never) {
        debug_printf("set_callbalck failed...ompt_callback_target_data_op\n");
    } else
        successes++;
    if (ompt_set_callback(ompt_callback_target_data_op_emi, (ompt_callback_t)t_ompt_callback_target_data_op_emi) == ompt_set_never) {
        debug_printf("set_callbalck failed...ompt_callback_target_data_op_emi\n");
    } else
        successes++;
    if (ompt_set_callback(ompt_callback_target_emi, (ompt_callback_t)t_ompt_callback_target_emi) == ompt_set_never) {
        debug_printf("set_callbalck failed...ompt_callback_target_emi\n");
    } else
        successes++;
    if (ompt_set_callback(ompt_callback_target, (ompt_callback_t)t_ompt_callback_target) == ompt_set_never) {
        debug_printf("set_callbalck failed...ompt_callback_target\n");
    } else
        successes++;
    if (ompt_set_callback(ompt_callback_target_map_emi, (ompt_callback_t)t_ompt_callback_target_map_emi) == ompt_set_never) {
        debug_printf("set_callbalck failed...ompt_callback_target_map_emi\n");
    } else
        successes++;
    if (ompt_set_callback(ompt_callback_target_map, (ompt_callback_t)t_ompt_callback_target_map) == ompt_set_never) {
        debug_printf("set_callbalck failed...ompt_callback_target_map\n");
    } else
        successes++;
    if (ompt_set_callback(ompt_callback_sync_region, (ompt_callback_t)t_ompt_callback_sync_region) == ompt_set_never) {
        debug_printf("set_callbalck failed...ompt_callback_sync_region\n");
    } else
        successes++;
    if (ompt_set_callback(ompt_callback_sync_region_wait, (ompt_callback_t)t_ompt_callback_sync_region_wait) == ompt_set_never) {
        debug_printf("set_callbalck failed...ompt_callback_sync_region_wait\n");
    } else
        successes++;
    if (ompt_set_callback(ompt_callback_task_create, (ompt_callback_t)t_ompt_callback_task_create) == ompt_set_never) {
        debug_printf("set_callbalck failed...ompt_callback_task_create\n");
    } else
        successes++;
    if (ompt_set_callback(ompt_callback_dependences, (ompt_callback_t)t_ompt_callback_dependences) == ompt_set_never) {
        debug_printf("set_callbalck failed...ompt_callback_dependences\n");
    } else
        successes++;
    if (ompt_set_callback(ompt_callback_task_dependence, (ompt_callback_t)t_ompt_callback_task_dependence) == ompt_set_never) {
        debug_printf("set_callbalck failed...ompt_callback_task_dependence\n");
    } else
        successes++;
    if (ompt_set_callback(ompt_callback_task_schedule, (ompt_callback_t)t_ompt_callback_task_schedule) == ompt_set_never) {
        debug_printf("set_callbalck failed...ompt_callback_task_schedule\n");
    } else
        successes++;
#if 0
        if (ompt_set_callback(ompt_callback_implicit_task, (ompt_callback_t)t_ompt_callback_implicit_task) == ompt_set_never) {
            debug_printf("set_callbalck failed...ompt_callback_implicit_task\n");
        } else successes++;
#endif

    if (successes > 0) {
        int max_threads = gt_info(t_max_threads);
        for (int i = 0; i < MAX_SYNC_REGIONS; i++) {
            stored_sync_events[i] = new msl_event *[max_threads];
            stored_sync_events_wait[i] = new msl_event *[max_threads];
            memset(stored_sync_events[i], 0, max_threads * sizeof(msl_event *));
            memset(stored_sync_events_wait[i], 0, max_threads * sizeof(msl_event *));
        }
    }
    debug_printf("ompt_init finished...\n");
    __DEBUG_EPILOG_OMPT_LAYER__
    return successes;
}

void t_ompt_fini(ompt_data_t *tool_data)
{
    __DEBUG_PROLOG_OMPT_LAYER__
    __DEBUG_EPILOG_OMPT_LAYER__
    return;
}
ompt_start_tool_result_t *ompt_start_tool(unsigned int omp_version, const char *runtime_version)
{
    __DEBUG_PROLOG_OMPT_LAYER__
    debug_printf("ompt_start_tool started...\n");
    if (!gt_info(parse_openmp))
        return NULL;
    t_ompt_start.initialize = t_ompt_init;
    t_ompt_start.finalize = t_ompt_fini;
    debug_printf("ompt_start_tool finished...\n");
    __DEBUG_EPILOG_OMPT_LAYER__
    return &t_ompt_start;
}
}
