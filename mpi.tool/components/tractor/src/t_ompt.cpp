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

#include "t_class_shim.h" /* the core through t_shared: built into the core (OMPT=on) or as libtractor.ompt.so */
static const bool t_ompt_core_ok = T_CLASS_CORE_CHECK("OpenMP (OMPT)", register_class);

#define ompt_data_value(x) x == NULL ? -1 : x->value

T_CLASS_STATIC msl_event stored_section_events;
static int begins = 0;
static long long uniq_id = 0;
#if 0 /* explicit tasks take their ids from uniq_implicit_task_id (one id space for task_schedule) */
static long long uniq_task_id = 0;
#endif
static long long uniq_implicit_task_id = 0;
// static ompt_get_record_ompt ompt_get_record;
static msl_event *record_ompt_event(short func0, short func1, const void *pcode)
{
    __DEBUG_PROLOG_OMPT_LAYER__
    msl_event *t_event = NULL;
#ifndef _TRACTOR_PASSTHROUGH_
    long long tsc_entry;
    pid_t tid_in;
    int tid;
    const PEAT_MPI_Func_ID_Through fidc = (PEAT_MPI_Func_ID_Through)func1;
    PEAT_Proc core_id;
    if (T_CORE(tractor_initialized) < 1)
        return NULL;
    tsc_entry = peat_get_time_tid_core(&tid_in, &core_id);
    tid = T_CORE_GET_TID(tid_in);
    /* an OMPT record is a point in time: it takes the level of the call it happens in, without opening one; none inside
       a call at the deepest level (PEAT_TRACTOR_MAX_NESTING_LEVELS): there is no level below it */
    const int level = T_CORE_LEVEL(tid);
    if (level >= T_CORE(t_max_nesting_levels))
        return NULL;
    PEAT_Addr *t_addrs;
    T_CORE_ALLOCATE(tid, level, &t_event, &t_addrs);
    if (T_CORE(full_ompt_unwind)) {
        stack_unwind(T_CORE(max_ret_level), t_addrs, T_CORE(ret_levels));
    } else {
        stack_unwind0(T_CORE(max_ret_level), t_addrs, T_CORE(ret_levels));
    }
    t_event->func_id = func1;
    t_event->tscs[0] = tsc_entry;
    t_event->tscs[2] = tid;
    t_event->ompt.pcode = (PEAT_Addr)pcode;
    t_event->tscs[1] = peat_get_time_core(&core_id);
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
        parallel_data->value = __atomic_fetch_add(&uniq_id, 1, __ATOMIC_RELAXED); // nested regions start on any thread
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
        overall_durations[OMPT_PARALLEL_ID_THROUGH] += event->tscs[0] - stored_section_events.tscs[1];
        overall_entries[OMPT_PARALLEL_ID_THROUGH]++;
        overall_tool_overhead[OMPT_PARALLEL_ID_THROUGH] += event->tscs[1] + stored_section_events.tscs[1] - event->tscs[0] - stored_section_events.tscs[0];
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
        overall_tool_overhead[OMPT_THREAD_BEGIN_ID_THROUGH] += event->tscs[1] - event->tscs[0] + event->tscs[3] - event->tscs[2];
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
        overall_tool_overhead[OMPT_THREAD_END_ID_THROUGH] += event->tscs[1] - event->tscs[0] + event->tscs[3] - event->tscs[2];
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
        overall_tool_overhead[OMPT_TARGET_DATA_OP_EMI_ID_THROUGH] += event->tscs[1] - event->tscs[0] + event->tscs[3] - event->tscs[2];
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
        overall_tool_overhead[OMPT_TARGET_DATA_OP_ID_THROUGH] += event->tscs[1] - event->tscs[0] + event->tscs[3] - event->tscs[2];
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
        overall_tool_overhead[OMPT_TARGET_EMI_ID_THROUGH] += event->tscs[1] - event->tscs[0] + event->tscs[3] - event->tscs[2];
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
        overall_tool_overhead[OMPT_TARGET_ID_THROUGH] += event->tscs[1] - event->tscs[0] + event->tscs[3] - event->tscs[2];
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
        overall_tool_overhead[OMPT_TARGET_MAP_EMI_ID_THROUGH] += event->tscs[1] - event->tscs[0] + event->tscs[3] - event->tscs[2];
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
        overall_tool_overhead[OMPT_TARGET_MAP_ID_THROUGH] += event->tscs[1] - event->tscs[0] + event->tscs[3] - event->tscs[2];
#endif
    }
    __DEBUG_EPILOG_OMPT_LAYER__
}

#define MAX_SYNC_REGIONS 11
T_CLASS_STATIC msl_event **stored_sync_events[MAX_SYNC_REGIONS];
#if 0 /* sync_region_wait is not recorded, see t_ompt_init */
T_CLASS_STATIC msl_event **stored_sync_events_wait[MAX_SYNC_REGIONS];
#endif

static void store_event(msl_event **stored_events[MAX_SYNC_REGIONS], ompt_sync_region_t kind, ompt_scope_endpoint_t endpoint, msl_event *event)
{
    int tid = event->tscs[2];
    if (kind < 0 || kind >= MAX_SYNC_REGIONS || tid < 0 || tid >= T_CORE(t_max_threads) || stored_events[kind] == NULL)
        return; /* a kind this table does not know (newer OMPT), or no table yet */
    debug_printf("Storing event %p of kind %d endpoint %d for tid %d\n", event, kind, endpoint, tid);
    if (stored_events[kind][tid]) {
        debug_printf("Unretrived event %p of kind %d endpoint %d and tid %d found.\n", stored_events[kind][tid], endpoint, kind, tid);
    }
    stored_events[kind][tid] = event;
}

static msl_event *retrieve_event(msl_event **stored_events[MAX_SYNC_REGIONS], ompt_sync_region_t kind, ompt_scope_endpoint_t endpoint, int tid)
{
    if (kind < 0 || kind >= MAX_SYNC_REGIONS || tid < 0 || tid >= T_CORE(t_max_threads) || stored_events[kind] == NULL)
        return NULL;
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
    long long tsc_entry = peat_get_time_tid_core(&tid_in, &core_id);
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
        if (event) {
            event->ompt.kind = kind;
            event->ompt.endpoint = endpoint;
        }
        if (event && endpoint == ompt_scope_beginend) {
            event->ompt.task_data = ompt_data_value(task_data);
            event->ompt.parallel_data = ompt_data_value(parallel_data);
            event->tscs[2] = tsc_entry;
            event->tscs[3] = peat_get_time_core(&core_id);
#ifdef _TRACTOR_COLLECT_STAT_
            overall_durations[OMPT_SYNC_REGION_ID_THROUGH] += event->tscs[2] - event->tscs[1];
            overall_entries[OMPT_SYNC_REGION_ID_THROUGH]++;
            overall_tool_overhead[OMPT_SYNC_REGION_ID_THROUGH] += event->tscs[1] - event->tscs[0] + event->tscs[3] - event->tscs[2];
#endif
        }
    }
    if (endpoint == ompt_scope_begin) {
        if (event) {
            store_event(stored_sync_events, kind, endpoint, event);
        }
    }
    if (endpoint == ompt_scope_end) {
        int tid = (int)T_CORE_GET_TID(tid_in);
        event = retrieve_event(stored_sync_events, kind, endpoint, tid);
        if (event) {
            event->ompt.task_data = ompt_data_value(task_data);
            event->ompt.parallel_data = ompt_data_value(parallel_data);
            event->tscs[2] = tsc_entry;
            event->tscs[3] = peat_get_time_core(&core_id);
#ifdef _TRACTOR_COLLECT_STAT_
            overall_durations[OMPT_SYNC_REGION_ID_THROUGH] += event->tscs[2] - event->tscs[1];
            overall_entries[OMPT_SYNC_REGION_ID_THROUGH]++;
            overall_tool_overhead[OMPT_SYNC_REGION_ID_THROUGH] += event->tscs[1] - event->tscs[0] + event->tscs[3] - event->tscs[2];
#endif
        } else {
            debug_printf("Null Event returned for kind %d and tid %d\n", kind, tid);
        }
    }
#endif
    __DEBUG_EPILOG_OMPT_LAYER__
}

#if 0 /* not recorded, see t_ompt_init */
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
    long long tsc_entry = peat_get_time_tid_core(&tid_in, &core_id);
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
        if (event) {
            event->ompt.kind = kind;
            event->ompt.endpoint = endpoint;
        }
        if (event && endpoint == ompt_scope_beginend) {
            event->ompt.task_data = ompt_data_value(task_data);
            event->ompt.parallel_data = ompt_data_value(parallel_data);
            event->tscs[2] = tsc_entry;
            event->tscs[3] = peat_get_time_core(&core_id);
#ifdef _TRACTOR_COLLECT_STAT_
            overall_durations[OMPT_SYNC_REGION_WAIT_ID_THROUGH] += event->tscs[2] - event->tscs[1];
            overall_entries[OMPT_SYNC_REGION_WAIT_ID_THROUGH]++;
            overall_tool_overhead[OMPT_SYNC_REGION_WAIT_ID_THROUGH] += event->tscs[1] - event->tscs[0] + event->tscs[3] - event->tscs[2];
#endif
        }
    }
    if (endpoint == ompt_scope_begin) {
        if (event) {
            store_event(stored_sync_events_wait, kind, endpoint, event);
        }
    }
    if (endpoint == ompt_scope_end) {
        int tid = (int)T_CORE_GET_TID(tid_in);
        event = retrieve_event(stored_sync_events_wait, kind, endpoint, tid);
        if (event) {
            event->ompt.task_data = ompt_data_value(task_data);
            event->ompt.parallel_data = ompt_data_value(parallel_data);
            event->tscs[2] = tsc_entry;
            event->tscs[3] = peat_get_time_core(&core_id);
#ifdef _TRACTOR_COLLECT_STAT_
            overall_durations[OMPT_SYNC_REGION_WAIT_ID_THROUGH] += event->tscs[2] - event->tscs[1];
            overall_entries[OMPT_SYNC_REGION_WAIT_ID_THROUGH]++;
            overall_tool_overhead[OMPT_SYNC_REGION_WAIT_ID_THROUGH] += event->tscs[1] - event->tscs[0] + event->tscs[3] - event->tscs[2];
#endif
        } else {
            debug_printf("Null Event returned for kind %d and tid %d\n", kind, tid);
        }
    }
#endif
    __DEBUG_EPILOG_OMPT_LAYER__
}
#endif

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
    /* one id space with the implicit tasks (task_schedule names both), taken atomically: tasks are created on any thread */
    if (new_task_data)
        new_task_data->value = __atomic_fetch_add(&uniq_implicit_task_id, 1, __ATOMIC_RELAXED);
    if (event) {
        event->tscs[2] = event->tscs[3] = event->tscs[1];
        event->ompt.kind = flags; // ompt_task_explicit, _undeferred, _untied, _final, _mergeable, _merged, _taskwait, ...
        event->ompt.task_data = ompt_data_value(encountering_task_data);
        event->ompt.parallel_data = ompt_data_value(new_task_data);
#ifdef _TRACTOR_COLLECT_STAT_
        overall_durations[OMPT_TASK_CREATE_ID_THROUGH] += event->tscs[2] - event->tscs[1];
        overall_entries[OMPT_TASK_CREATE_ID_THROUGH]++;
        overall_tool_overhead[OMPT_TASK_CREATE_ID_THROUGH] += event->tscs[1] - event->tscs[0] + event->tscs[3] - event->tscs[2];
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
    /* task_data is the new task of task_create, which has given it its id already: not overwritten here */
    if (event) {
        event->tscs[2] = event->tscs[3] = event->tscs[1];
        event->ompt.task_data = ompt_data_value(task_data);
        event->ompt.parallel_data = ompt_data_value(task_data);
#ifdef _TRACTOR_COLLECT_STAT_
        overall_durations[OMPT_DEPENDENCES_ID_THROUGH] += event->tscs[2] - event->tscs[1];
        overall_entries[OMPT_DEPENDENCES_ID_THROUGH]++;
        overall_tool_overhead[OMPT_DEPENDENCES_ID_THROUGH] += event->tscs[1] - event->tscs[0] + event->tscs[3] - event->tscs[2];
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
        overall_tool_overhead[OMPT_TASK_DEPENDENCE_ID_THROUGH] += event->tscs[1] - event->tscs[0] + event->tscs[3] - event->tscs[2];
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
        event->ompt.kind = prior_task_status; // ompt_task_complete = 1 ends the prior task; switch, yield, detach... suspend it
        event->ompt.task_data = ompt_data_value(prior_task_data);
        event->ompt.parallel_data = ompt_data_value(next_task_data);
#ifdef _TRACTOR_COLLECT_STAT_
        overall_durations[OMPT_TASK_SCHEDULE_ID_THROUGH] += event->tscs[2] - event->tscs[1];
        overall_entries[OMPT_TASK_SCHEDULE_ID_THROUGH]++;
        overall_tool_overhead[OMPT_TASK_SCHEDULE_ID_THROUGH] += event->tscs[1] - event->tscs[0] + event->tscs[3] - event->tscs[2];
#endif
    }
    __DEBUG_EPILOG_OMPT_LAYER__
}

/* Implicit tasks that are not recorded (the initial task, a serialized region): task_data->value of their begin is set to
   this, so that the end is skipped too (the runtime hands the same task_data value to the end, also on a worker) */
#define T_OMPT_IMPLICIT_TASK_SKIPPED (-1LL)

static void t_ompt_callback_implicit_task(
    ompt_scope_endpoint_t endpoint,
    ompt_data_t *parallel_data,
    ompt_data_t *task_data,
    unsigned int actual_parallelism,
    unsigned int index,
    int flags)
{
    __DEBUG_PROLOG_OMPT_LAYER__
    /* Only the implicit tasks of a real parallel region are recorded: they bound the parallel section of a thread and,
       for a worker, its fork barrier (see t_ompt_init). The initial task and a serialized region (one thread) are no
       fork and no join: they would only add records, which look like boundaries of a fork barrier on a worker. */
    if (endpoint == ompt_scope_begin || endpoint == ompt_scope_beginend) {
        if ((flags & ompt_task_initial) || actual_parallelism <= 1) {
            if (task_data)
                task_data->value = T_OMPT_IMPLICIT_TASK_SKIPPED;
            __DEBUG_EPILOG_OMPT_LAYER__
            return;
        }
    } else if (task_data == NULL || task_data->value == T_OMPT_IMPLICIT_TASK_SKIPPED) {
        __DEBUG_EPILOG_OMPT_LAYER__
        return;
    }
    msl_event *event = record_ompt_event(OMPT_IMPLICIT_TASK_ID, OMPT_IMPLICIT_TASK_ID_THROUGH, NULL);
    if (event) {
        event->tscs[2] = event->tscs[3] = event->tscs[1];
        event->ompt.kind = actual_parallelism;
        event->ompt.endpoint = endpoint;
        if (endpoint == ompt_scope_begin || endpoint == ompt_scope_beginend) {
            if (task_data)
                task_data->value = __atomic_fetch_add(&uniq_implicit_task_id, 1, __ATOMIC_RELAXED);
        }
        event->ompt.task_data = ompt_data_value(task_data);
        event->ompt.parallel_data = ompt_data_value(parallel_data);
#ifdef _TRACTOR_COLLECT_STAT_
        overall_durations[OMPT_IMPLICIT_TASK_ID_THROUGH] += event->tscs[2] - event->tscs[1];
        overall_entries[OMPT_IMPLICIT_TASK_ID_THROUGH]++;
        overall_tool_overhead[OMPT_IMPLICIT_TASK_ID_THROUGH] += event->tscs[1] - event->tscs[0] + event->tscs[3] - event->tscs[2];
#endif
    } else if (endpoint == ompt_scope_begin || endpoint == ompt_scope_beginend) {
        if (task_data)
            task_data->value = T_OMPT_IMPLICIT_TASK_SKIPPED; /* not recorded: its end is not recorded either */
    }
    __DEBUG_EPILOG_OMPT_LAYER__
}

T_CLASS_STATIC ompt_initialize_t t_init;
T_CLASS_STATIC ompt_start_tool_result_t t_ompt_start;
T_CLASS_STATIC ompt_finalize_tool_t ompt_finalize_tool;
static void t_ompt_finalize_class(void) /* called by the core before it writes the trace */
{
    if (ompt_finalize_tool)
        ompt_finalize_tool();
}

T_CLASS_STATIC int t_ompt_init(ompt_function_lookup_t lookup, int initial_device_num, ompt_data_t *tool_data)
{
    __DEBUG_PROLOG_OMPT_LAYER__
    int successes = 0;
    debug_printf("ompt_init started...\n");
    static const struct t_shim_class t_ompt_class = {"OMPT", NULL, t_ompt_finalize_class, NULL};
    if (!T_CORE_REGISTER_CLASS(&t_ompt_class)) /* another copy of OMPT is set up already */
        return 0;

    /* the tables of the open sync regions (begin kept until its end) exist before any callback can fire */
    {
        int max_threads = T_CORE(t_max_threads);
        for (int i = 0; i < MAX_SYNC_REGIONS; i++) {
            stored_sync_events[i] = new msl_event *[max_threads];
            memset(stored_sync_events[i], 0, max_threads * sizeof(msl_event *));
#if 0 /* sync_region_wait is not recorded */
            stored_sync_events_wait[i] = new msl_event *[max_threads];
            memset(stored_sync_events_wait[i], 0, max_threads * sizeof(msl_event *));
#endif
        }
    }

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
    /* The OpenMP marks (one record per mark or interval, per thread; intervals follow each other, never nested):
         parallel section begin/end   implicit_task begin/end (every thread of a real parallel region; the main thread
                                      also has parallel_begin/parallel_end, the fork call and its return)
         plain barrier begin/end      sync_region, one record begin..end, kind != 9 (explicit barrier, the implicit
                                      barrier of single/for/sections = kind 8, ...)
         join barrier begin/end       sync_region, one record begin..end, kind 9 (implicit_parallel)
         fork barrier (worker only)   no callback of its own: from implicit_task end (the runtime sets ompt_state_idle right
                                      after it) to the next implicit_task begin, or to thread_end
         task created                 task_create (kind: flags; parallel_data field: id of the new task)
         task begins/resumes, ends    task_schedule (task_data field: prior task, parallel_data field: next task; kind:
                                      status of the prior task, ompt_task_complete = 1 ends it, other values suspend it)
         task dependences             dependences, task_dependence
       Task ids share one counter with the implicit tasks, so task_schedule names every task uniquely.
       sync_region_wait is not recorded: it lies inside its sync_region and adds only a nested interval. */
#if 0
    if (ompt_set_callback(ompt_callback_sync_region_wait, (ompt_callback_t)t_ompt_callback_sync_region_wait) == ompt_set_never) {
        debug_printf("set_callbalck failed...ompt_callback_sync_region_wait\n");
    } else
        successes++;
#endif /* sync_region_wait */
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
    /* The records of the implicit tasks bound the parallel section of every thread. For a worker the end of its implicit
       task is the start of its fork barrier (idle wait for the next region: the runtime sets ompt_state_idle right after
       it) and the begin of the next one is the end of that barrier; there is no other event for it. */
    if (ompt_set_callback(ompt_callback_implicit_task, (ompt_callback_t)t_ompt_callback_implicit_task) == ompt_set_never) {
        debug_printf("set_callbalck failed...ompt_callback_implicit_task\n");
    } else
        successes++;

    debug_printf("ompt_init finished...\n");
    __DEBUG_EPILOG_OMPT_LAYER__
    return successes;
}

T_CLASS_STATIC void t_ompt_fini(ompt_data_t *tool_data)
{
    __DEBUG_PROLOG_OMPT_LAYER__
    __DEBUG_EPILOG_OMPT_LAYER__
    return;
}
ompt_start_tool_result_t *ompt_start_tool(unsigned int omp_version, const char *runtime_version)
{
    __DEBUG_PROLOG_OMPT_LAYER__
    debug_printf("ompt_start_tool started...\n");
    if (!t_ompt_core_ok || !T_CORE(parse_openmp))
        return NULL;
    t_ompt_start.initialize = t_ompt_init;
    t_ompt_start.finalize = t_ompt_fini;
    debug_printf("ompt_start_tool finished...\n");
    __DEBUG_EPILOG_OMPT_LAYER__
    return &t_ompt_start;
}
}
