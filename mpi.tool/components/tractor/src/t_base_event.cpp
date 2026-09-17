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
    t_max_tid_num = MAX_TID_NUM;
    // tid2t_tid = NULL;

    extras = 0;
    events_start_chain = events_chain = NULL;
    extras_start_chain = extras_chain = NULL;
    c_extras_start_chain = c_extras_chain = NULL;

    event_index = NULL;
    extras_index = NULL;
    c_extras_index = NULL;

    extras_array = NULL;
    c_extras_array = NULL;
    events_array = NULL;

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
    // if (tid2t_tid==NULL) return 0; // happens if t_fab_profiler won't count any events
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

int t_base_event::reuse_event(int tid, msl_event **event, msl_extras_counters *counters, short func_id)
{
    __DEBUG_PROLOG_TRACTOR_LAYER__
    if ((compress_events & 1) == 0) {
        __DEBUG_EPILOG_TRACTOR_LAYER__
        return 0;
    }
    if (counting_permit && num_allocated <= calls_limit) {
        if (last_returned_event[tid]) {
            if (last_returned_event[tid]->func_id[1] == func_id) {
                if (last_returned_event[tid]->func_id[1] >= MPI_TEST_START_THROUGH && last_returned_event[tid]->func_id[1] <= MPI_TEST_END_THROUGH) {
                    if (last_returned_event[tid]->requests.flag != 0) {
                        __DEBUG_EPILOG_TRACTOR_LAYER__
                        return 0; // previous test was not empty
                    }
                }
                if (last_returned_event[tid]->func_id[1] >= MPI_IPROBE_ID_THROUGH && last_returned_event[tid]->func_id[1] <= MPI_IMPROBE_ID_THROUGH) {
                    if (last_returned_event[tid]->p2p_pr_pb.flag != 0) {
                        __DEBUG_EPILOG_TRACTOR_LAYER__
                        return 0; // previous test was not empty
                    }
                }
                *event = last_returned_event[tid];
                counters->addrs = &addrs_array[tid][(event_index[tid] - 1) * num_addrs];
#ifdef _HWCOUNTERS_ENABLED_
                counters->hwcs = &hwc_array[tid][(event_index[tid] - 1) * num_hwcs * 2];
#endif
#ifdef _MEMPROFILE_STORE_
                counters->mems = &mems_array[tid][(event_index[tid] - 1) * 2];
#endif
                __DEBUG_EPILOG_TRACTOR_LAYER__
                return 1;
            }
        }
    }
    __DEBUG_EPILOG_TRACTOR_LAYER__
    return 0;
}

void t_base_event::allocate_event(int tid, int skip_dump, msl_event **event, msl_extras_counters *counters)
{
    __DEBUG_PROLOG_TRACTOR_LAYER__

    if (counting_permit && ++num_allocated <= calls_limit) {
        if (event_index[tid] >= onetimealloc) {
            msl_adv_chain *temp = events_chain[tid];
            events_chain[tid] = new msl_adv_chain(events_chain[tid], sizeof(msl_event), num_addrs,
#ifdef _HWCOUNTERS_ENABLED_
                                                  num_hwcs * 2,
#endif
#ifdef _MEMPROFILE_STORE_
                                                  t_memory_event_size * 2,
#endif
                                                  uniq_index[tid] + 1);
            temp->num_events = event_index[tid];
            events_array[tid] = (msl_event *)events_chain[tid]->events_array;
            addrs_array[tid] = (void **)events_chain[tid]->addr_array;
#ifdef _HWCOUNTERS_ENABLED_
            hwc_array[tid] = (long long *)events_chain[tid]->hwc_array;
#endif
#ifdef _MEMPROFILE_STORE_
            mems_array[tid] = (t_memory_event *)events_chain[tid]->mem_array;
#endif
            event_index[tid] = 0;
            if (use_realtime_dump && !skip_dump)
                dump_events(tid, &events_start_chain[tid]);
        }

        msl_event *store_event = &events_array[tid][event_index[tid]];
        counters->addrs = &addrs_array[tid][event_index[tid] * num_addrs];
#ifdef _HWCOUNTERS_ENABLED_
        counters->hwcs = &hwc_array[tid][event_index[tid] * num_hwcs * 2];
#endif
#ifdef _MEMPROFILE_STORE_
        counters->mems = &mems_array[tid][event_index[tid] * 2];
#endif
#ifndef _TRACTOR_ZERO_ON_ALLOCATE_
        memset(store_event, 0, sizeof(msl_event));
#endif
        event_index[tid]++;

#ifdef _TRACTOR_STORE_UNIQ_INDEX_
        store_event->uniq_index = uniq_index[tid];
#endif
        uniq_index[tid]++;
        *event = last_returned_event[tid] = store_event;
        if (counting_permit > 1)
            counting_permit -= 2;
    } else {
        *event = &fake_event;
        counters->addrs = fake_addrs;
#ifdef _HWCOUNTERS_ENABLED_
        counters->hwcs = fake_hwcs;
#endif
#ifdef _MEMPROFILE_STORE_
        counters->mems = fake_mems;
#endif
    }
    __DEBUG_EPILOG_TRACTOR_LAYER__
    return;
}
int t_base_event::discard_event(int tid, msl_event **event)
{
    __DEBUG_PROLOG_TRACTOR_LAYER__
    if (last_returned_event[tid] == *event && event_index[tid] > 0) {
        --uniq_index[tid];
        --event_index[tid];
        last_returned_event[tid] = &events_array[tid][event_index[tid]];
        *event = NULL;
        __DEBUG_EPILOG_TRACTOR_LAYER__
        return 1;
    }
    __DEBUG_EPILOG_TRACTOR_LAYER__
    return 0;
}

void t_base_event::check_chain(pid_t tid)
{
    __DEBUG_PROLOG_TRACTOR_LAYER__
    if (use_realtime_dump) {
        if (events_start_chain[tid] != events_chain[tid])
            dump_events(tid, &events_start_chain[tid]);
    }
    __DEBUG_EPILOG_TRACTOR_LAYER__
}

int t_base_event::reuse_event(int tid, msl_event **event, short func_id)
{
    __DEBUG_PROLOG_TRACTOR_LAYER__
    int reused = 0;
    if (counting_permit && num_allocated <= calls_limit) {
        if (last_returned_event[tid]) {
            if (last_returned_event[tid]->func_id[1] == func_id) {
                reused = 1;
                *event = last_returned_event[tid];
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
        if (!events_chain[tid]) {
            init_events_storage(tid);
        }
        if (tid >= t_tid_counter) {
            t_tid_counter = tid + 1;
        }
        if (event_index[tid] >= onetimealloc) {
            msl_adv_chain *temp = events_chain[tid];
            events_chain[tid] = new msl_adv_chain(events_chain[tid], sizeof(msl_event), num_addrs,
                                                  uniq_index[tid] + 1);
            temp->num_events = event_index[tid];
            events_array[tid] = (msl_event *)events_chain[tid]->events_array;
            addrs_array[tid] = (void **)events_chain[tid]->addr_array;
            event_index[tid] = 0;
            if (use_realtime_dump)
                dump_events(tid, &events_start_chain[tid]);
        }

        msl_event *store_event = &events_array[tid][event_index[tid]];
#ifndef _TRACTOR_ZERO_ON_ALLOCATE_
        memset(store_event, 0, sizeof(msl_event));
#endif
        event_index[tid]++;

#ifdef _TRACTOR_STORE_UNIQ_INDEX_
        store_event->uniq_index = uniq_index[tid];
#endif
        uniq_index[tid]++;
        *event = store_event;
        if (counting_permit > 1)
            counting_permit -= 2;
        last_returned_event[tid] = store_event;
    } else {
        *event = &fake_event;
    }
    __DEBUG_EPILOG_TRACTOR_LAYER__
    return;
}

void t_base_event::init_events_storage(int t_tid)
{
    events_chain[t_tid] = new msl_adv_chain(NULL, sizeof(msl_event), num_addrs,
#ifdef _HWCOUNTERS_ENABLED_
                                            num_hwcs * 2,
#endif
#ifdef _MEMPROFILE_STORE_
                                            t_memory_event_size * 2,
#endif
                                            uniq_index[t_tid]);
    events_start_chain[t_tid] = events_chain[t_tid];

    events_array[t_tid] = (msl_event *)events_chain[t_tid]->events_array;
    addrs_array[t_tid] = (void **)events_chain[t_tid]->addr_array;
#ifdef _HWCOUNTERS_ENABLED_
    hwc_array[t_tid] = (long long *)events_chain[t_tid]->hwc_array;
#endif
#ifdef _MEMPROFILE_STORE_
    mems_array[t_tid] = (t_memory_event *)events_chain[t_tid]->mem_array;
#endif
    if (extras) {
        extras_chain[t_tid] = new msl_base_chain(NULL, sizeof(msl_extra_info));
        extras_start_chain[t_tid] = extras_chain[t_tid];
        extras_array[t_tid] = (msl_extra_info *)extras_start_chain[t_tid]->events_array;

        c_extras_chain[t_tid] = new msl_base_chain(NULL, sizeof(msl_extra_info));
        c_extras_start_chain[t_tid] = c_extras_chain[t_tid];
        c_extras_array[t_tid] = (msl_extra_info *)c_extras_start_chain[t_tid]->events_array;
    }
    memset(&events_array[t_tid][0], 0, sizeof(msl_event));
#ifdef _TRACTOR_STORE_UNIQ_INDEX_
    events[t_tid][0].uniq_index = uniq_index[t_tid]++;
#endif
    event_index[t_tid] = 0;
    if (extras) {
        extras_index[t_tid] = 0;
        c_extras_index[t_tid] = 0;
    }
}

void t_base_event::init(int max_threads)
{
    __DEBUG_PROLOG_TRACTOR_LAYER__
    int i, j, retval;
    debug_printf("Base Event::init %llx.\n", this);
    t_max_threads = max_threads;
    debug_printf("Base Events:max_threads=%d. %d %d \n", t_max_threads, onetimealloc, num_addrs);

    events_start_chain = (msl_adv_chain **)t_malloc(t_max_threads * sizeof(msl_adv_chain *), 128);
    memset(events_start_chain, 0, t_max_threads * sizeof(msl_adv_chain *));
    events_chain = (msl_adv_chain **)t_malloc(t_max_threads * sizeof(msl_adv_chain *), 128);
    memset(events_chain, 0, t_max_threads * sizeof(msl_adv_chain *));
    events_array = (msl_event **)t_malloc(t_max_threads * sizeof(msl_event *), 128);
    memset(events_array, 0, t_max_threads * sizeof(msl_event *));
    addrs_array = (void ***)t_malloc(t_max_threads * sizeof(void **), 128);
    memset(addrs_array, 0, t_max_threads * sizeof(void **));
#ifdef _HWCOUNTERS_ENABLED_
    hwc_array = (long long **)t_malloc(t_max_threads * sizeof(long long *), 128);
    memset(hwc_array, 0, t_max_threads * sizeof(long long *));
#endif
#ifdef _MEMPROFILE_STORE_
    mems_array = (t_memory_event **)t_malloc(t_max_threads * sizeof(t_memory_event), 128);
    memset(mems_array, 0, t_max_threads * sizeof(t_memory_event));
#endif

    event_index = (int *)t_malloc(t_max_threads * sizeof(int), 128);
    memset(event_index, 0, t_max_threads * sizeof(int));
    if (extras) {
        extras_start_chain = (msl_base_chain **)t_malloc(t_max_threads * sizeof(msl_base_chain *), 128);
        memset(extras_start_chain, 0, t_max_threads * sizeof(msl_base_chain *));
        extras_chain = (msl_base_chain **)t_malloc(t_max_threads * sizeof(msl_base_chain *), 128);
        memset(extras_chain, 0, t_max_threads * sizeof(msl_base_chain *));
        extras_array = (msl_extra_info **)t_malloc(t_max_threads * sizeof(msl_extra_info *), 128);
        memset(extras_chain, 0, t_max_threads * sizeof(msl_extra_info *));
        extras_index = (int *)t_malloc(t_max_threads * sizeof(int), 128);
        memset(extras_chain, 0, t_max_threads * sizeof(int));

        c_extras_start_chain = (msl_base_chain **)t_malloc(t_max_threads * sizeof(msl_base_chain *), 128);
        memset(c_extras_start_chain, 0, t_max_threads * sizeof(msl_base_chain *));
        c_extras_chain = (msl_base_chain **)t_malloc(t_max_threads * sizeof(msl_base_chain *), 128);
        memset(c_extras_chain, 0, t_max_threads * sizeof(msl_base_chain *));
        c_extras_array = (msl_extra_info **)t_malloc(t_max_threads * sizeof(msl_extra_info *), 128);
        memset(c_extras_array, 0, t_max_threads * sizeof(msl_extra_info *));
        c_extras_index = (int *)t_malloc(t_max_threads * sizeof(int), 128);
        memset(c_extras_index, 0, t_max_threads * sizeof(int));
    }
    uniq_index = (long long *)t_malloc(t_max_threads * sizeof(long long), 128);
    memset(uniq_index, 0, t_max_threads * sizeof(long long));

    last_returned_event = (msl_event **)t_malloc(t_max_threads * sizeof(msl_event **), 128);
    memset(last_returned_event, 0, t_max_threads * sizeof(msl_event **));

    if (tid2t_tid == NULL) {
        tid2t_tid = (int *)t_malloc(t_max_tid_num * sizeof(int), 128);
        memset(tid2t_tid, -1, t_max_tid_num * sizeof(int));
    }

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
    int i;
    if (!initialized)
        return;
    initialized = 0;
    debug_printf("Base Event::end %llx %d.\n", this, t_max_threads);
    fflush(stderr);
    debug_printf("Base Event::destroying chains.\n");
    debug_printf("%llx %llx %llx\n", events_start_chain, extras_start_chain, c_extras_start_chain);
    for (i = 0; i < t_tid_counter; i++) {
        debug_printf("%d %llx.\n", i, events_start_chain[i]);
        destroy_whole_chain(events_start_chain[i]);
        if (extras) {
            debug_printf("%d %llx.\n", i, extras_start_chain[i]);
            debug_printf("%d %llx.\n", i, c_extras_start_chain[i]);
            destroy_whole_chain(extras_start_chain[i]);
            destroy_whole_chain(c_extras_start_chain[i]);
        }
    }

    if (events_start_chain)
        t_free(events_start_chain, t_max_threads * sizeof(msl_adv_chain *));
    if (extras) {
        if (extras_start_chain)
            t_free(extras_start_chain, t_max_threads * sizeof(msl_base_chain *));
        if (c_extras_start_chain)
            t_free(c_extras_start_chain, t_max_threads * sizeof(msl_base_chain *));
    }
    if (uniq_index)
        t_free(uniq_index, t_max_threads * sizeof(long long));
    if (last_returned_event)
        t_free(last_returned_event, t_max_threads * sizeof(msl_event **));

    if (tid2t_tid ) { 
        t_free(tid2t_tid, t_max_tid_num * sizeof(int));
        tid2t_tid = NULL;
    }

    if (events_array)
        t_free(events_array, t_max_threads * sizeof(msl_event *));
    if (addrs_array)
        t_free(addrs_array, t_max_threads * sizeof(void **));
    if (extras) {
        if (extras_array)
            t_free(extras_array, t_max_threads * sizeof(msl_extra_info *));
        if (c_extras_array)
            t_free(c_extras_array, t_max_threads * sizeof(msl_extra_info *));
    }
#ifdef _HWCOUNTERS_ENABLED_
    if (hwc_array)
        t_free(hwc_array, t_max_threads * sizeof(long long *));
#endif
#ifdef _MEMPROFILE_STORE_
    if (mems_array)
        t_free(mems_array, t_max_threads * sizeof(t_memory_event));
#endif
    __DEBUG_EPILOG_TRACTOR_LAYER__
}

#if defined(__CPLUSPLUS__) || defined(__cplusplus)
}
#endif
