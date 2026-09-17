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

/*
 * ompcalls.c
 *
 * OMPT tool that reproduces the statistics collected by the hand written
 * "#ifdef KMP_MICRO_TIMING ... __osl_record_event(...)" instrumentation
 * scattered through kmp_runtime.c, kmp_csupport.c, kmp_tasking.c and
 * kmp_stat.c.
 *
 * Deliberately targets the current OpenMP 5.x OMPT interface
 * (<omp-tools.h>: ompt_data_t, ompt_callback_sync_region with a "kind"
 * argument, ompt_callback_mutex_acquire/acquired/released, ompt_start_tool(),
 * ...) rather than the older "Technical Report" API that ships in this
 * tree today (ompt-general.c / ompt-specific.c / ompt-internal.h, event
 * set gated by ompt-event-specific.h). That older API can't express every
 * legacy event (no barrier kind, no team-relative thread index, no
 * critical-section acquire/wait callbacks are wired up); OMPT 5.x can.
 * To actually run this file, either pair it with a modern OMPT-capable
 * OpenMP runtime, or upgrade ompt-general.c/ompt-specific.c in this tree
 * to a real OMPT 5.x implementation and load this file as its
 * OMP_TOOL_LIBRARIES tool (or link it directly and export ompt_start_tool).
 *
 * Legacy event                            -> OMPT 5.x callback used here
 * -------------------------------------------------------------------
 * OSL_APPLICATION_STARTS/ENDS             -> tool initialize()/finalize()
 * OSL_CALIBRATION_STARTS                  -> ompt_callback_thread_end (worker)
 * OSL_FORK_CALL                           -> ompt_callback_parallel_begin
 * OSL_SERIALIZED_MICROTASK_STARTS/ENDS    -> ompt_callback_implicit_task, actual_parallelism==1
 * OSL_SERIALIZED_TEAM_MICROTASK_STARTS/ENDS -> folded into OSL_SERIALIZED_MICROTASK_* (see note)
 * OSL_MICROTASK_STARTS/ENDS               -> ompt_callback_implicit_task, actual_parallelism>1
 * OSL_FORK_BARRIER_STARTS/ENDS            -> ompt_callback_sync_region_wait(ompt_sync_region_barrier_implicit_parallel)
 *                                            (ompt_callback_idle was removed from the OMPT spec/
 *                                            omp-tools.h; the wait sub-phase of the parallel-region-
 *                                            exit barrier is used instead, same worker-idle window)
 * OSL_JOIN_BARRIER_STARTS/ENDS            -> ompt_callback_sync_region(ompt_sync_region_barrier_implicit)
 * OSL_PLAIN_BARRIER_STARTS/ENDS           -> ompt_callback_sync_region(ompt_sync_region_barrier_explicit)
 * OSL_SINGLE_STARTS/ENDS                  -> ompt_callback_work(single_executor/single_other)
 *                                            and ompt_callback_masked (master)
 * OSL_CRITICAL_SECTION_STARTS             -> ompt_callback_mutex_acquire  (kind==ompt_mutex_critical)
 * OSL_CRITICAL_BARRIER_STARTS             -> ompt_callback_mutex_acquire  (wait phase, same call-back)
 * OSL_CRITICAL_BARRIER_ENDS               -> ompt_callback_mutex_acquired (lock obtained)
 * OSL_CRITICAL_SECTION_ENDS               -> ompt_callback_mutex_released (lock released)
 * OSL_TASK_STARTS/ENDS                    -> ompt_callback_task_create + ompt_callback_task_schedule
 * OSL_TASKWAIT_STARTS/ENDS                -> ompt_callback_sync_region(ompt_sync_region_taskwait)
 * OSL_START_ONDEMAND/OSL_STOP_ONDEMAND    -> ompt_callback_control_tool (omp_control_tool() API)
 * OSL_TASKFORK_CALL                       -> not recorded (unused/dead in the legacy code too)
 *
 * Unlike the older TR API, OMPT 5.x resolves every gap the TR version had:
 *   - ompt_callback_sync_region's "kind" argument tells barrier_implicit
 *     (join) apart from barrier_explicit (plain), so both OSL_JOIN_BARRIER_*
 *     and OSL_PLAIN_BARRIER_* can be produced instead of being folded together.
 *   - ompt_callback_implicit_task's "index" argument is the team-relative
 *     thread number (what __kmp_tid_from_gtid() returns internally), so
 *     "tid" no longer has to be approximated with the global thread id.
 *   - ompt_callback_mutex_acquire/acquired/released are real, always-fireable
 *     callbacks in OMPT 5.x, so the full critical-section acquire-wait/
 *     acquired/release cycle is recoverable, not just the release.
 * The one event this API still can't separate is
 * OSL_SERIALIZED_TEAM_MICROTASK_STARTS/ENDS: it identified a narrow "teams
 * construct nested serialize" edge case in kmp_runtime.c that has no
 * distinct OMPT signal of its own, so it folds into OSL_SERIALIZED_MICROTASK_*.
 *
 * gtid/tid/gtmid convention:
 *   - "gtid" reproduces the original global thread id: assigned once per
 *     OS thread, in ompt_callback_thread_begin.
 *   - "tid" reproduces the original team-relative id: the "index" argument
 *     ompt_callback_implicit_task passes, cached per OS thread and reused
 *     by every callback that fires while that thread is executing inside
 *     the team (barriers, single, critical, tasks, taskwait) since those
 *     callbacks don't all carry an index of their own.
 *   - "gtmid" reproduces the original "team id" grouping field: a unique
 *     id assigned to parallel_data->value the first time a region is seen
 *     (ompt_callback_parallel_begin), cached per OS thread the same way as
 *     "tid" for callbacks that don't receive parallel_data directly
 *     (e.g. the mutex callbacks).
 *
 * This file is compiled like the other "*.c" files of this project (see
 * rules.mk): with the C++ compiler, hence thread_local/std::atomic and
 * extern "C" linkage are freely usable.
 */

#include <atomic>
#include <cstdint>
#include <cstdio>
#include <unistd.h>

#include "omp-tools.h"
#include "omp.h"

#include "kmp_stat.h"

/* ---------------------------------------------------------------------- */
/* Per OS thread state                                                    */
/* ---------------------------------------------------------------------- */

static std::atomic<int64_t> g_next_gtid(0);
static std::atomic<int64_t> g_next_gtmid(0);

static thread_local int64_t tls_gtid = -1;    /* assigned in on_thread_begin */
static thread_local int64_t tls_tid = 0;      /* assigned in on_implicit_task, team-relative index */
static thread_local int64_t tls_gtmid = 0;    /* assigned in on_implicit_task, from parallel_data->value */
static thread_local void *tls_task_fn = NULL; /* codeptr_ra cached from task_create for task_schedule */

static inline int current_gtid(void) { return (int)tls_gtid; }
static inline int current_tid(void) { return (int)tls_tid; }
static inline int current_gtmid(void) { return (int)tls_gtmid; }

/* uniq_id correlation between a START and the matching END, one slot per
   construct kind since a given OS thread cannot be in two instances of
   the same construct at once. */
static thread_local long long tls_uniq_barrier = -1;
static thread_local long long tls_uniq_single = -1;
static thread_local long long tls_uniq_task = -1;
static thread_local long long tls_uniq_taskwait = -1;
static thread_local long long tls_uniq_microtask = -1;
static thread_local long long tls_uniq_fork_barrier = -1;
static thread_local long long tls_uniq_critical = -1;

/* ---------------------------------------------------------------------- */
/* Runtime entry points looked up during initialize()                     */
/* ---------------------------------------------------------------------- */

static ompt_set_callback_t ompcalls_set_callback = NULL;

/* ---------------------------------------------------------------------- */
/* Thread life cycle -> OSL_CALIBRATION_STARTS                            */
/* ---------------------------------------------------------------------- */

static void
on_thread_begin(ompt_thread_t thread_type, ompt_data_t *thread_data)
{
    (void)thread_type;
    thread_data->value = tls_gtid = g_next_gtid.fetch_add(1);
}

static void
on_thread_end(ompt_data_t *thread_data)
{
    //    __osl_calibrate_counters((int) thread_data->value);
}

/* ---------------------------------------------------------------------- */
/* Parallel region fork -> OSL_FORK_CALL                                   */
/* ---------------------------------------------------------------------- */

static void
on_parallel_begin(ompt_data_t *encountering_task_data,
                  const ompt_frame_t *encountering_task_frame,
                  ompt_data_t *parallel_data,
                  unsigned int requested_parallelism,
                  int flags,
                  const void *codeptr_ra)
{
    (void)encountering_task_data;
    (void)encountering_task_frame;
    (void)requested_parallelism;
    (void)flags;

    parallel_data->value = g_next_gtmid.fetch_add(1);
    __osl_record_event(OSL_FORK_CALL, current_tid(), current_gtid(), (int)parallel_data->value,
                       (void *)codeptr_ra);
}

/* ---------------------------------------------------------------------- */
/* Implicit task -> OSL_MICROTASK_* / OSL_SERIALIZED_MICROTASK_*           */
/* ---------------------------------------------------------------------- */

static void
on_implicit_task(ompt_scope_endpoint_t endpoint,
                 ompt_data_t *parallel_data,
                 ompt_data_t *task_data,
                 unsigned int actual_parallelism,
                 unsigned int index,
                 int flags)
{
    (void)task_data;
    (void)flags;

    if (endpoint == ompt_scope_begin) {
        tls_tid = index;
        tls_gtmid = parallel_data ? parallel_data->value : current_gtid();
        tls_uniq_microtask = __osl_record_event(actual_parallelism <= 1 ? OSL_SERIALIZED_MICROTASK_STARTS
                                                                        : OSL_MICROTASK_STARTS,
                                                current_tid(), current_gtid(), current_gtmid(),
                                                NULL);
    } else if (endpoint == ompt_scope_end) {
        __osl_record_event(actual_parallelism <= 1 ? OSL_SERIALIZED_MICROTASK_ENDS : OSL_MICROTASK_ENDS,
                           current_tid(), current_gtid(), current_gtmid(),
                           NULL, tls_uniq_microtask);
    }
}

/* ---------------------------------------------------------------------- */
/* Barriers / taskwait -> OSL_JOIN_BARRIER_*, OSL_PLAIN_BARRIER_*,         */
/*                        OSL_TASKWAIT_*                                  */
/* ---------------------------------------------------------------------- */

static void
on_sync_region(ompt_sync_region_t kind,
               ompt_scope_endpoint_t endpoint,
               ompt_data_t *parallel_data,
               ompt_data_t *task_data,
               const void *codeptr_ra)
{
    (void)task_data;

    int start_event, end_event;
    long long *slot;

    switch (kind) {
    case ompt_sync_region_barrier_implicit:
        start_event = OSL_JOIN_BARRIER_STARTS;
        end_event = OSL_JOIN_BARRIER_ENDS;
        slot = &tls_uniq_barrier;
        break;
    case ompt_sync_region_barrier_explicit:
        start_event = OSL_PLAIN_BARRIER_STARTS;
        end_event = OSL_PLAIN_BARRIER_ENDS;
        slot = &tls_uniq_barrier;
        break;
    case ompt_sync_region_taskwait:
        start_event = OSL_TASKWAIT_STARTS;
        end_event = OSL_TASKWAIT_ENDS;
        slot = &tls_uniq_taskwait;
        break;
    default:
        return; /* taskgroup / reduction / barrier_implementation: not tracked originally */
    }

    int gtmid = parallel_data ? (int)parallel_data->value : current_gtmid();

    if (endpoint == ompt_scope_begin) {
        *slot = __osl_record_event(start_event, current_tid(), current_gtid(), gtmid,
                                   (void *)codeptr_ra);
    } else if (endpoint == ompt_scope_end) {
        __osl_record_event(end_event, current_tid(), current_gtid(), gtmid, NULL, *slot);
    }
}

/* ---------------------------------------------------------------------- */
/* Parallel-region exit barrier wait phase -> OSL_FORK_BARRIER_*          */
/* (worker blocked here until released to join the next team)             */
/* ---------------------------------------------------------------------- */

static void
on_sync_region_wait(ompt_sync_region_t kind,
                    ompt_scope_endpoint_t endpoint,
                    ompt_data_t *parallel_data,
                    ompt_data_t *task_data,
                    const void *codeptr_ra)
{
    (void)task_data;

    if (kind != ompt_sync_region_barrier_implicit_parallel)
        return; /* only the parallel-region-exit barrier maps to the old fork barrier */

    int gtmid = parallel_data ? (int)parallel_data->value : current_gtmid();

    if (endpoint == ompt_scope_begin) {
        tls_uniq_fork_barrier = __osl_record_event(OSL_FORK_BARRIER_STARTS, current_tid(), current_gtid(),
                                                   gtmid, codeptr_ra);
    } else if (endpoint == ompt_scope_end) {
        __osl_record_event(OSL_FORK_BARRIER_ENDS, current_tid(), current_gtid(), gtmid, codeptr_ra, tls_uniq_fork_barrier);
    }
}

/* ---------------------------------------------------------------------- */
/* single / master -> OSL_SINGLE_*                                         */
/* ---------------------------------------------------------------------- */

static void
on_work(ompt_work_t wstype,
        ompt_scope_endpoint_t endpoint,
        ompt_data_t *parallel_data,
        ompt_data_t *task_data,
        uint64_t count,
        const void *codeptr_ra)
{
    (void)task_data;
    (void)count;

    if (wstype != ompt_work_single_executor && wstype != ompt_work_single_other)
        return; /* only "single" was instrumented in the legacy code */

    int gtmid = parallel_data ? (int)parallel_data->value : current_gtmid();

    if (endpoint == ompt_scope_begin) {
        tls_uniq_single = __osl_record_event(OSL_SINGLE_STARTS, current_tid(), current_gtid(), gtmid,
                                             (void *)codeptr_ra);
    } else if (endpoint == ompt_scope_end) {
        __osl_record_event(OSL_SINGLE_ENDS, current_tid(), current_gtid(), gtmid, codeptr_ra, tls_uniq_single);
    }
}

static void
on_masked(ompt_scope_endpoint_t endpoint,
          ompt_data_t *parallel_data,
          ompt_data_t *task_data,
          const void *codeptr_ra)
{
    /* Legacy code funneled "master" into the same SINGLE_* events. */
    on_work(ompt_work_single_executor, endpoint, parallel_data, task_data, 0, codeptr_ra);
}

/* ---------------------------------------------------------------------- */
/* critical -> OSL_CRITICAL_SECTION_*, OSL_CRITICAL_BARRIER_*              */
/* ---------------------------------------------------------------------- */

static void
on_mutex_acquire(ompt_mutex_t kind,
                 unsigned int hint,
                 unsigned int impl,
                 ompt_wait_id_t wait_id,
                 const void *codeptr_ra)
{
    (void)hint;
    (void)impl;
    (void)wait_id;

    if (kind != ompt_mutex_critical)
        return; /* only "critical" was instrumented in the legacy code */

    __osl_record_event(OSL_CRITICAL_SECTION_STARTS, current_tid(), current_gtid(), current_gtmid(),
                       (void *)codeptr_ra);
    tls_uniq_critical = __osl_record_event(OSL_CRITICAL_BARRIER_STARTS, current_tid(), current_gtid(),
                                           current_gtmid(), (void *)codeptr_ra);
}

static void
on_mutex_acquired(ompt_mutex_t kind, ompt_wait_id_t wait_id, const void *codeptr_ra)
{
    (void)wait_id;
    (void)codeptr_ra;

    if (kind != ompt_mutex_critical)
        return;

    __osl_record_event(OSL_CRITICAL_BARRIER_ENDS, current_tid(), current_gtid(), current_gtmid(),
                       codeptr_ra, tls_uniq_critical);
}

static void
on_mutex_released(ompt_mutex_t kind, ompt_wait_id_t wait_id, const void *codeptr_ra)
{
    (void)wait_id;
    (void)codeptr_ra;

    if (kind != ompt_mutex_critical)
        return;

    __osl_record_event(OSL_CRITICAL_SECTION_ENDS, current_tid(), current_gtid(), current_gtmid(), codeptr_ra);
}

/* ---------------------------------------------------------------------- */
/* explicit tasks -> OSL_TASK_*                                            */
/* ---------------------------------------------------------------------- */

static void
on_task_create(ompt_data_t *encountering_task_data,
               const ompt_frame_t *encountering_task_frame,
               ompt_data_t *new_task_data,
               int flags,
               int has_dependences,
               const void *codeptr_ra)
{
    (void)encountering_task_data;
    (void)encountering_task_frame;
    (void)has_dependences;

    /* Tag explicit tasks so on_task_schedule() can tell them apart from
       the implicit tasks that also flow through task_schedule(); stash
       codeptr_ra as a stand-in for the legacy task->routine pointer,
       which OMPT 5.x does not hand to the tool directly. */
    new_task_data->ptr = (flags & ompt_task_explicit) ? (void *)codeptr_ra : NULL;
}

static void
on_task_schedule(ompt_data_t *prior_task_data,
                 ompt_task_status_t prior_task_status,
                 ompt_data_t *next_task_data)
{
    (void)prior_task_status;

    if (prior_task_data != NULL && prior_task_data->ptr != NULL) {
        __osl_record_event(OSL_TASK_ENDS, current_tid(), current_gtid(), current_gtmid(),
                           prior_task_data->ptr, tls_uniq_task);
    }
    if (next_task_data != NULL && next_task_data->ptr != NULL) {
        tls_uniq_task = __osl_record_event(OSL_TASK_STARTS, current_tid(), current_gtid(), current_gtmid(),
                                           next_task_data->ptr);
    }
}

/* ---------------------------------------------------------------------- */
/* omp_control_tool() -> OSL_START_ONDEMAND / OSL_STOP_ONDEMAND            */
/* ---------------------------------------------------------------------- */

/* Fixed values from the OpenMP spec (omp_control_tool_t); some omp.h
   headers implement the OMPT callbacks but don't declare this enum. */
#if 0
#define OMPCALLS_CONTROL_TOOL_START 1
#define OMPCALLS_CONTROL_TOOL_PAUSE 2
#define OMPCALLS_CONTROL_TOOL_FLUSH 3
#define OMPCALLS_CONTROL_TOOL_END 4

static int
on_control_tool(uint64_t command, uint64_t modifier, void *arg, const void *codeptr_ra)
{
    (void) modifier;
    (void) arg;
    (void) codeptr_ra;

    switch (command) {
    case OMPCALLS_CONTROL_TOOL_START:
        __osl_start_microtiming();
        return 0;
    case OMPCALLS_CONTROL_TOOL_PAUSE:
    case OMPCALLS_CONTROL_TOOL_END:
        __osl_stop_microtiming();
        return 0;
    default:
        return -1; /* unsupported command */
    }
}

#else
static int
on_control_tool(omp_control_tool_t command, uint64_t modifier, void *arg, const void *codeptr_ra)
{
    (void)modifier;
    (void)arg;
    (void)codeptr_ra;

    switch (command) {
    case omp_control_tool_start:
        __osl_start_microtiming();
        return 0;
    case omp_control_tool_pause:
    case omp_control_tool_end:
        __osl_stop_microtiming();
        return 0;
    default:
        return -1; /* unsupported command */
    }
}
#endif

/* ---------------------------------------------------------------------- */
/* Tool registration                                                       */
/* ---------------------------------------------------------------------- */

#define OMPCALLS_SET(event, fn)                                                 \
    do {                                                                        \
        int rc = ompcalls_set_callback((event), (ompt_callback_t)(fn));         \
        if (rc == ompt_set_never)                                               \
            fprintf(stderr, "ompcalls: runtime does not support %s\n", #event); \
    } while (0)

static int
ompcalls_initialize(ompt_function_lookup_t lookup, int initial_device_num, ompt_data_t *tool_data)
{
    (void)initial_device_num;
    (void)tool_data;

    ompcalls_set_callback = (ompt_set_callback_t)lookup("ompt_set_callback");
    if (ompcalls_set_callback == NULL)
        return 0; /* runtime is not OMPT capable enough, decline */

    OMPCALLS_SET(ompt_callback_thread_begin, on_thread_begin);
    OMPCALLS_SET(ompt_callback_thread_end, on_thread_end);
    OMPCALLS_SET(ompt_callback_parallel_begin, on_parallel_begin);
    OMPCALLS_SET(ompt_callback_implicit_task, on_implicit_task);
    OMPCALLS_SET(ompt_callback_sync_region, on_sync_region);
    OMPCALLS_SET(ompt_callback_sync_region_wait, on_sync_region_wait);
    OMPCALLS_SET(ompt_callback_work, on_work);
    OMPCALLS_SET(ompt_callback_masked, on_masked);
    OMPCALLS_SET(ompt_callback_mutex_acquire, on_mutex_acquire);
    OMPCALLS_SET(ompt_callback_mutex_acquired, on_mutex_acquired);
    OMPCALLS_SET(ompt_callback_mutex_released, on_mutex_released);
    OMPCALLS_SET(ompt_callback_task_create, on_task_create);
    OMPCALLS_SET(ompt_callback_task_schedule, on_task_schedule);
    OMPCALLS_SET(ompt_callback_control_tool, on_control_tool);

    /* OSL_APPLICATION_STARTS: allocates the per-thread event chains, so it
       has to run before any of the callbacks above can fire. */
    __osl_begin_application(omp_get_max_threads(), (int)sysconf(_SC_NPROCESSORS_ONLN));

    return 1; /* accept the tool */
}

static void
ompcalls_finalize(ompt_data_t *tool_data)
{
    (void)tool_data;
    __osl_end_application(); /* records OSL_APPLICATION_ENDS and flushes logs */
}

#ifdef __cplusplus
extern "C" {
#endif

/* Entry point looked up by the OpenMP runtime (OMP_TOOL_LIBRARIES /
   static linkage) to register this tool. Must keep C linkage: it is
   found through dlsym()/symbol lookup by name. */
ompt_start_tool_result_t *
ompt_start_tool(unsigned int omp_version, const char *runtime_version)
{
    (void)omp_version;
    (void)runtime_version;

    static ompt_start_tool_result_t result = {
        &ompcalls_initialize,
        &ompcalls_finalize,
        {0}};
    return &result;
}

#ifdef __cplusplus
}
#endif
