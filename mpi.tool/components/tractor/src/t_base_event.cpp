/*
 * This file is part of PEAT (Performance Estimation & Analyzing Toolkit).
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

#include "mpi.h"
#include <assert.h>
#include <dlfcn.h>
#include <libgen.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>
#ifdef _OPENMP
#include <omp.h>
#endif
#include "hwloc.h"
#include <iomanip>
#include <iostream>
#include <mutex>
#include <sstream>
#include <stdexcept>
#include <string>

#include "mpi_func_ids_through.h"
#include "t_base_event.h"

#if defined(__CPLUSPLUS__) || defined(__cplusplus)
extern "C" {
#endif

t_base_event::t_base_event()
{
    __DEBUG_PROLOG_TRACTOR_LAYER__
    debug_printf("Base Event::t_base_event %llx.\n", this);
    initialized = 0;
    calls_limit = (unsigned long long)(-1);
    num_allocated = 0;
    use_realtime_dump = 0;
    compress_events = 0;
    t_pid = getpid();
#ifdef _MEMPROFILE_STORE_
    t_memory_event_size = sizeof(t_memory_event);
#endif

    onetimealloc = defonetimealloc;
    t_max_threads = 6;
    t_tid_counter = 0;
    t_max_nesting_levels = 4;
    t_achieved_nesting_levels = 1;
    nesting_level = NULL;
    t_max_tid_num = MAX_TID_NUM;
    // tid2t_tid = NULL;

    extras = 0;
    events_start_chain = events_chain = NULL;
    extras_start_chain = extras_chain = NULL;

    event_index = NULL;
    extras_index = NULL;

    extras_array = NULL;
    events_array = NULL;
#ifdef _HWCOUNTERS_ENABLED_
    hwc_array = NULL;
#endif
#ifdef _MEMPROFILE_STORE_
    mems_array = NULL;
#endif

    addrs_array = NULL;
    uniq_index = NULL;
    last_returned_event = NULL;
    __DEBUG_EPILOG_TRACTOR_LAYER__
};

t_base_event::~t_base_event()
{
    __DEBUG_PROLOG_TRACTOR_LAYER__
    debug_printf("Base Event::~t_base_event %llx %d.\n", this, t_max_threads);
    t_base_event::end();
    __DEBUG_EPILOG_TRACTOR_LAYER__
}

std::mutex thread_mutex;
int t_base_event::get_tid(pid_t tid_in)
{
    int t_tid = 0;
    pid_t tid = tid_in - t_pid;

    debug_printf("Base Event::get_tid. %d %d %p.\n", tid_in, t_tid, tid2t_tid);
    // if (tid2t_tid==NULL) return 0; // happens if t_transport_profiler won't count any events
    if (counting_permit) {
        if (tid >= t_max_tid_num) {
            fprintf(stderr, "Rank %d. Thread ID=%d. is out of range (0..%d).\n", t_mpi_rank, tid, t_max_tid_num);
            PMPI_Abort(MPI_COMM_WORLD, 911);
        }
        if (tid < 0) {
            fprintf(stderr, "Rank %d. Thread ID=%d. is negative.\n", t_mpi_rank, tid, t_max_tid_num);
            PMPI_Abort(MPI_COMM_WORLD, 911);
        }
        t_tid = tid2t_tid[tid];
        if (t_tid >= t_max_threads) {
            fprintf(stderr, "Rank %d. Thread ID=%d. Tractor Thread ID=%d is out of range (0..%d).\n", t_mpi_rank, (int)(tid), t_tid, t_max_threads);
            PMPI_Abort(MPI_COMM_WORLD, 911);
        }
        if (t_tid < 0) {
            thread_mutex.lock();
            t_tid = t_tid_counter++;
            tid2t_tid[tid] = t_tid;
            thread_mutex.unlock();
            if (t_tid >= t_max_threads) {
                fprintf(stderr, "Rank %d. Thread ID=%d. Tractor Thread ID=%d is out of range (0..%d).\n", t_mpi_rank, (int)(tid), t_tid, t_max_threads);
                PMPI_Abort(MPI_COMM_WORLD, 911);
            }
            init_events_storage(t_tid);
            debug_printf("Base Event::get_tid. New TID received. Chain initialized %d %d %d %p.\n", tid_in, tid, t_tid, tid2t_tid);

            /*
            for (int i=0;i<t_max_tid_num;i++) {
                debug_printf(" Current State of Translation %d->%d.\n", i, tid2t_tid[i]);
            }
            */
        }
    }
    return (t_tid);
}


/* ---------------- nesting ---------------- */

int t_base_event::enter_call(int tid)
{
    int level = nesting_level[tid];
    if (level >= t_max_nesting_levels) {
        fprintf(stderr, "Rank %d. Thread %d: nesting level %d exceeds the maximum %d (PEAT_TRACTOR_MAX_NESTING_LEVELS).\n", t_mpi_rank, tid, level, t_max_nesting_levels);
        PMPI_Abort(MPI_COMM_WORLD, 912);
    }
    nesting_level[tid] = level + 1;
    if (level + 1 > t_achieved_nesting_levels)
        t_achieved_nesting_levels = level + 1;
    return level;
}

void t_base_event::leave_call(int tid)
{
    if (nesting_level[tid] > 0)
        nesting_level[tid]--;
}

long long t_base_event::child_base(int tid, int level)
{
    if (level + 1 >= t_max_nesting_levels)
        return 0;
    return uniq_index[tid][level + 1];
}

/* ---------------- storage ---------------- */

static inline msl_adv_chain *t_new_adv_chain(t_base_event *b, msl_adv_chain *prev, long long start_index)
{
    return new msl_adv_chain(prev, sizeof(msl_event), b->num_addrs,
#ifdef _HWCOUNTERS_ENABLED_
                             b->num_hwcs * 2,
#endif
#ifdef _MEMPROFILE_STORE_
                             b->t_memory_event_size * 2,
#endif
                             start_index);
}

void t_base_event::init_level_storage(int t_tid, int level)
{
    if (events_chain[t_tid][level] == NULL) {
        events_chain[t_tid][level] = t_new_adv_chain(this, NULL, uniq_index[t_tid][level]);
        events_start_chain[t_tid][level] = events_chain[t_tid][level];
        events_array[t_tid][level] = (msl_event *)events_chain[t_tid][level]->events_array;
        addrs_array[t_tid][level] = (void **)events_chain[t_tid][level]->addr_array;
#ifdef _HWCOUNTERS_ENABLED_
        hwc_array[t_tid][level] = (long long *)events_chain[t_tid][level]->hwc_array;
#endif
#ifdef _MEMPROFILE_STORE_
        mems_array[t_tid][level] = (t_memory_event *)events_chain[t_tid][level]->mem_array;
#endif
        memset(&events_array[t_tid][level][0], 0, sizeof(msl_event));
        event_index[t_tid][level] = 0;
    }
    if (extras && extras_chain[t_tid][level] == NULL) {
        extras_chain[t_tid][level] = new msl_base_chain(NULL, sizeof(msl_extra_info));
        extras_start_chain[t_tid][level] = extras_chain[t_tid][level];
        extras_array[t_tid][level] = (msl_extra_info *)extras_start_chain[t_tid][level]->events_array;
        extras_index[t_tid][level] = 0;
    }
}

void t_base_event::init_events_storage(int t_tid)
{
    init_level_storage(t_tid, 0);
}

int t_base_event::reuse_event(int tid, int level, msl_event **event, msl_extras_counters *counters, short func_id)
{
    __DEBUG_PROLOG_TRACTOR_LAYER__
    if ((compress_events & 1) == 0) {
        __DEBUG_EPILOG_TRACTOR_LAYER__
        return 0;
    }
    if (counting_permit && num_allocated <= calls_limit) {
        msl_event *last = last_returned_event[tid][level];
        if (last && last->func_id == func_id) {
            if (func_id >= MPI_TEST_START_THROUGH && func_id <= MPI_TEST_END_THROUGH) {
                if (last->requests.flag != 0) {
                    __DEBUG_EPILOG_TRACTOR_LAYER__
                    return 0; // previous test was not empty
                }
            }
            if (func_id >= MPI_IPROBE_ID_THROUGH && func_id <= MPI_IMPROBE_ID_THROUGH) {
                if (last->p2p_pr_pb.flag != 0) {
                    __DEBUG_EPILOG_TRACTOR_LAYER__
                    return 0; // previous probe was not empty
                }
            }
            *event = last;
            int idx = event_index[tid][level] - 1;
            counters->addrs = &addrs_array[tid][level][idx * num_addrs];
#ifdef _HWCOUNTERS_ENABLED_
            counters->hwcs = &hwc_array[tid][level][idx * num_hwcs * 2];
#endif
#ifdef _MEMPROFILE_STORE_
            counters->mems = &mems_array[tid][level][idx * 2];
#endif
            __DEBUG_EPILOG_TRACTOR_LAYER__
            return 1;
        }
    }
    __DEBUG_EPILOG_TRACTOR_LAYER__
    return 0;
}

void t_base_event::allocate_event(int tid, int level, int skip_dump, msl_event **event, msl_extras_counters *counters)
{
    __DEBUG_PROLOG_TRACTOR_LAYER__

    if (counting_permit && ++num_allocated <= calls_limit) {
        if (events_chain[tid][level] == NULL)
            init_level_storage(tid, level);
        if (event_index[tid][level] >= onetimealloc) {
            msl_adv_chain *temp = events_chain[tid][level];
            events_chain[tid][level] = t_new_adv_chain(this, temp, uniq_index[tid][level] + 1);
            temp->num_events = event_index[tid][level];
            events_array[tid][level] = (msl_event *)events_chain[tid][level]->events_array;
            addrs_array[tid][level] = (void **)events_chain[tid][level]->addr_array;
#ifdef _HWCOUNTERS_ENABLED_
            hwc_array[tid][level] = (long long *)events_chain[tid][level]->hwc_array;
#endif
#ifdef _MEMPROFILE_STORE_
            mems_array[tid][level] = (t_memory_event *)events_chain[tid][level]->mem_array;
#endif
            event_index[tid][level] = 0;
            if (use_realtime_dump && !skip_dump)
                dump_events(tid, level, &events_start_chain[tid][level]);
        }

        int idx = event_index[tid][level];
        msl_event *store_event = &events_array[tid][level][idx];
        counters->addrs = &addrs_array[tid][level][idx * num_addrs];
#ifdef _HWCOUNTERS_ENABLED_
        counters->hwcs = &hwc_array[tid][level][idx * num_hwcs * 2];
#endif
#ifdef _MEMPROFILE_STORE_
        counters->mems = &mems_array[tid][level][idx * 2];
#endif
#ifndef _TRACTOR_ZERO_ON_ALLOCATE_
        memset(store_event, 0, sizeof(msl_event));
#endif
        event_index[tid][level]++;

#ifdef _TRACTOR_STORE_UNIQ_INDEX_
        store_event->uniq_index = uniq_index[tid][level];
#endif
        uniq_index[tid][level]++;
        *event = last_returned_event[tid][level] = store_event;
        if (counting_permit > 1)
            counting_permit -= 2;
    } else {
        *event = &fake_event;
        counters->addrs = (PEAT_Addr *)fake_addrs;
#ifdef _HWCOUNTERS_ENABLED_
        counters->hwcs = (long long *)fake_addrs;
#endif
#ifdef _MEMPROFILE_STORE_
        counters->mems = (t_memory_event *)fake_addrs;
#endif
    }
    __DEBUG_EPILOG_TRACTOR_LAYER__
    return;
}

int t_base_event::discard_event(int tid, int level, msl_event **event, long long children_base)
{
    __DEBUG_PROLOG_TRACTOR_LAYER__
    if (last_returned_event[tid][level] == *event && event_index[tid][level] > 0) {
        if (child_base(tid, level) != children_base) {
            __DEBUG_EPILOG_TRACTOR_LAYER__
            return 0; // records were issued from progress inside this call: they need their parent
        }
        --uniq_index[tid][level];
        --event_index[tid][level];
        last_returned_event[tid][level] = event_index[tid][level] > 0 ? &events_array[tid][level][event_index[tid][level] - 1] : NULL;
        *event = NULL;
        __DEBUG_EPILOG_TRACTOR_LAYER__
        return 1;
    }
    __DEBUG_EPILOG_TRACTOR_LAYER__
    return 0;
}

void t_base_event::check_chain(pid_t tid, int level)
{
    __DEBUG_PROLOG_TRACTOR_LAYER__
    if (use_realtime_dump) {
        if (events_start_chain[tid][level] && events_start_chain[tid][level] != events_chain[tid][level])
            dump_events(tid, level, &events_start_chain[tid][level]);
    }
    __DEBUG_EPILOG_TRACTOR_LAYER__
}

/* transport-API records: level 0, no counters; transport-API calls are never nested in each other */
int t_base_event::reuse_event(int tid, msl_event **event, short func_id)
{
    __DEBUG_PROLOG_TRACTOR_LAYER__
    int reused = 0;
    if (counting_permit && num_allocated <= calls_limit) {
        if (last_returned_event[tid][0]) {
            if (last_returned_event[tid][0]->func_id == func_id) {
                reused = 1;
                *event = last_returned_event[tid][0];
            }
        }
    } else {
        *event = &fake_event;
    }
    __DEBUG_EPILOG_TRACTOR_LAYER__
    return reused;
}
void t_base_event::allocate_event(int tid, msl_event **event, short func_id)
{
    __DEBUG_PROLOG_TRACTOR_LAYER__
    if (counting_permit && ++num_allocated <= calls_limit) {
        if (!events_chain[tid][0]) {
            init_level_storage(tid, 0);
        }
        if (tid >= t_tid_counter) {
            t_tid_counter = tid + 1;
        }
        if (event_index[tid][0] >= onetimealloc) {
            msl_adv_chain *temp = events_chain[tid][0];
            events_chain[tid][0] = t_new_adv_chain(this, temp, uniq_index[tid][0] + 1);
            temp->num_events = event_index[tid][0];
            events_array[tid][0] = (msl_event *)events_chain[tid][0]->events_array;
            addrs_array[tid][0] = (void **)events_chain[tid][0]->addr_array;
            event_index[tid][0] = 0;
            if (use_realtime_dump)
                dump_events(tid, 0, &events_start_chain[tid][0]);
        }

        msl_event *store_event = &events_array[tid][0][event_index[tid][0]];
#ifndef _TRACTOR_ZERO_ON_ALLOCATE_
        memset(store_event, 0, sizeof(msl_event));
#endif
        event_index[tid][0]++;

#ifdef _TRACTOR_STORE_UNIQ_INDEX_
        store_event->uniq_index = uniq_index[tid][0];
#endif
        uniq_index[tid][0]++;
        *event = store_event;
        if (counting_permit > 1)
            counting_permit -= 2;
        last_returned_event[tid][0] = store_event;
    } else {
        *event = &fake_event;
    }
    __DEBUG_EPILOG_TRACTOR_LAYER__
    return;
}

/* [tid][level] tables of pointers or scalars (no templates: this file has C linkage) */
static void **t_alloc_2d_raw(int n_tid, int n_level, size_t elem)
{
    void **arr = (void **)t_malloc(n_tid * sizeof(void *), 128);
    for (int i = 0; i < n_tid; i++) {
        arr[i] = t_malloc(n_level * elem, 128);
        memset(arr[i], 0, n_level * elem);
    }
    return arr;
}
static void t_free_2d_raw(void **arr, int n_tid, int n_level, size_t elem)
{
    if (arr == NULL)
        return;
    for (int i = 0; i < n_tid; i++) {
        if (arr[i])
            t_free(arr[i], n_level * elem);
    }
    t_free(arr, n_tid * sizeof(void *));
}
#define T_ALLOC_2D(T, n_tid, n_level) ((T **)t_alloc_2d_raw(n_tid, n_level, sizeof(T)))
#define T_FREE_2D(arr, n_tid, n_level) t_free_2d_raw((void **)(arr), n_tid, n_level, sizeof(*(arr)[0]))

void t_base_event::init(int max_threads, int max_nesting_levels)
{
    __DEBUG_PROLOG_TRACTOR_LAYER__
    int i, j, retval;
    debug_printf("Base Event::init %llx.\n", this);
    t_max_threads = max_threads;
    t_max_nesting_levels = max_nesting_levels > 0 ? max_nesting_levels : 1;
    t_achieved_nesting_levels = 1;
    debug_printf("Base Events:max_threads=%d nesting_levels=%d. %d %d \n", t_max_threads, t_max_nesting_levels, onetimealloc, num_addrs);

    nesting_level = (int *)t_malloc(t_max_threads * sizeof(int), 128);
    memset(nesting_level, 0, t_max_threads * sizeof(int));

    events_start_chain = T_ALLOC_2D(msl_adv_chain *, t_max_threads, t_max_nesting_levels);
    events_chain = T_ALLOC_2D(msl_adv_chain *, t_max_threads, t_max_nesting_levels);
    events_array = T_ALLOC_2D(msl_event *, t_max_threads, t_max_nesting_levels);
    addrs_array = T_ALLOC_2D(void **, t_max_threads, t_max_nesting_levels);
#ifdef _HWCOUNTERS_ENABLED_
    hwc_array = T_ALLOC_2D(long long *, t_max_threads, t_max_nesting_levels);
#endif
#ifdef _MEMPROFILE_STORE_
    mems_array = T_ALLOC_2D(t_memory_event *, t_max_threads, t_max_nesting_levels);
#endif
    event_index = T_ALLOC_2D(int, t_max_threads, t_max_nesting_levels);
    if (extras) {
        extras_start_chain = T_ALLOC_2D(msl_base_chain *, t_max_threads, t_max_nesting_levels);
        extras_chain = T_ALLOC_2D(msl_base_chain *, t_max_threads, t_max_nesting_levels);
        extras_array = T_ALLOC_2D(msl_extra_info *, t_max_threads, t_max_nesting_levels);
        extras_index = T_ALLOC_2D(int, t_max_threads, t_max_nesting_levels);
    }
    uniq_index = T_ALLOC_2D(long long, t_max_threads, t_max_nesting_levels);
    last_returned_event = T_ALLOC_2D(msl_event *, t_max_threads, t_max_nesting_levels);

    if (tid2t_tid == NULL) {
        tid2t_tid = (int *)t_malloc(t_max_tid_num * sizeof(int), 128);
        for (i = 0; i < t_max_tid_num; i++)
            tid2t_tid[i] = -1;
    }
    memset(&fake_event, 0, sizeof(msl_event));
    initialized = 1;
    __DEBUG_EPILOG_TRACTOR_LAYER__
}

void t_base_event::fini()
{
    __DEBUG_PROLOG_TRACTOR_LAYER__
    __DEBUG_EPILOG_TRACTOR_LAYER__
}

void t_base_event::end()
{
    __DEBUG_PROLOG_TRACTOR_LAYER__
    int i, j;
    if (initialized < 1)
        return;
    initialized = 0;
    debug_printf("Base Event::end %llx %d.\n", this, t_max_threads);
    fflush(stderr);
    debug_printf("Base Event::destroying chains.\n");
    for (i = 0; i < t_tid_counter; i++) {
        for (j = 0; j < t_max_nesting_levels; j++) {
            if (events_start_chain[i][j])
                destroy_whole_chain(events_start_chain[i][j]);
            if (extras && extras_start_chain[i][j])
                destroy_whole_chain(extras_start_chain[i][j]);
        }
    }

    T_FREE_2D(events_start_chain, t_max_threads, t_max_nesting_levels);
    T_FREE_2D(events_chain, t_max_threads, t_max_nesting_levels);
    T_FREE_2D(events_array, t_max_threads, t_max_nesting_levels);
    T_FREE_2D(addrs_array, t_max_threads, t_max_nesting_levels);
#ifdef _HWCOUNTERS_ENABLED_
    T_FREE_2D(hwc_array, t_max_threads, t_max_nesting_levels);
#endif
#ifdef _MEMPROFILE_STORE_
    T_FREE_2D(mems_array, t_max_threads, t_max_nesting_levels);
#endif
    T_FREE_2D(event_index, t_max_threads, t_max_nesting_levels);
    if (extras) {
        T_FREE_2D(extras_start_chain, t_max_threads, t_max_nesting_levels);
        T_FREE_2D(extras_chain, t_max_threads, t_max_nesting_levels);
        T_FREE_2D(extras_array, t_max_threads, t_max_nesting_levels);
        T_FREE_2D(extras_index, t_max_threads, t_max_nesting_levels);
    }
    T_FREE_2D(uniq_index, t_max_threads, t_max_nesting_levels);
    T_FREE_2D(last_returned_event, t_max_threads, t_max_nesting_levels);
    if (nesting_level)
        t_free(nesting_level, t_max_threads * sizeof(int));
    nesting_level = NULL;
    events_start_chain = events_chain = NULL;
    extras_start_chain = extras_chain = NULL;
    events_array = NULL;
    addrs_array = NULL;
    extras_array = NULL;
    event_index = extras_index = NULL;
    uniq_index = NULL;
    last_returned_event = NULL;

    if (tid2t_tid) {
        t_free(tid2t_tid, t_max_tid_num * sizeof(int));
        tid2t_tid = NULL;
    }
    __DEBUG_EPILOG_TRACTOR_LAYER__
}

#include "records.h"

#if defined(__CPLUSPLUS__) || defined(__cplusplus)
}
#endif
