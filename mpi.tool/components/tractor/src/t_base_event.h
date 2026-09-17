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

#ifndef __T_BASE_EVENT_H__
#define __T_BASE_EVENT_H__
#include "mpi.h"
#include <assert.h>

#ifdef _HWCOUNTERS_ENABLED_
#include "hwcounter.h"
#endif

#ifdef _MEMPROFILE_
#include "memprofile.h"
#endif
#include "t_alloc.h"
#include "t_defines.h"

#define t_max(a, b) ((a) > (b)) ? (a) : (b)
#define t_min(a, b) ((a) < (b)) ? (a) : (b)

#if defined(__CPLUSPLUS__) || defined(__cplusplus)
extern "C" {
#endif
extern int counting_permit;
const long defonetimealloc = 1000000;

struct msl_base_chain {
    msl_base_chain *next;
    void *events_array;
    int num_events;
    long long start_index;
    int events_size;

    msl_base_chain(msl_base_chain *prev_chain, int s, long long _start_index_ = 0)
    {
        next = NULL;
        start_index = _start_index_;
        num_events = 0;
        events_size = s;
        if (events_size > 0) {
            events_array = t_malloc((onetimealloc + 1) * events_size, 128);
        } else {
            events_array = NULL;
        }
#ifdef _TRACTOR_ZERO_ON_ALLOCATE_
        if (events_size > 0)
            memset(events_array, 0, (onetimealloc + 1) * events_size);
#endif
        if (prev_chain != NULL)
            prev_chain->next = this;
        //        debug_printf("msl_base_chain %llx %d\n",array,size);
    }

    virtual ~msl_base_chain()
    {
        //        debug_printf("~msl_base_chain %llx %llx\n",this,array);
        if (events_array) {
            t_free(events_array, (onetimealloc + 1) * events_size);
        }
    }
};

struct msl_adv_chain : msl_base_chain {

    void *addr_array;
    int addr_size;
#ifdef _HWCOUNTERS_ENABLED_
    void *hwc_array;
    int hwc_size;
#endif
#ifdef _MEMPROFILE_STORE_
    void *mem_array;
    int mem_size;
#endif
    msl_adv_chain(msl_base_chain *prev_chain, int s, int addr_num,
#ifdef _HWCOUNTERS_ENABLED_
                  int hwc_num,
#endif
#ifdef _MEMPROFILE_STORE_
                  int mprof_size,
#endif
                  long long _start_index_ = 0) : msl_base_chain(prev_chain, s, _start_index_)
    {
        addr_size = sizeof(void *) * addr_num;
        if (addr_size > 0) {
            addr_array = t_malloc((onetimealloc + 1) * sizeof(void *) * addr_num, 128);
        } else {
            addr_array = NULL;
        }
#ifdef _HWCOUNTERS_ENABLED_
        hwc_size = sizeof(long long) * hwc_num;
        if (hwc_size > 0) {
            hwc_array = t_malloc((onetimealloc + 1) * sizeof(long long) * hwc_num, 128);
        } else {
            hwc_array = NULL;
        }
#endif
#ifdef _MEMPROFILE_STORE_
        mem_size = mprof_size;
        if (mem_size > 0) {
            mem_array = t_malloc((onetimealloc + 1) * mprof_size, 128);
        } else {
            mem_array = NULL
        };
#endif

#ifdef _TRACTOR_ZERO_ON_ALLOCATE_
        if (addr_size > 0)
            memset(addr_array, 0, (onetimealloc + 1) * sizeof(void *) * addr_num);
#ifdef _HWCOUNTERS_ENABLED_
        if (hwc_size > 0)
            memset(hwc_array, 0, (onetimealloc + 1) * sizeof(long long) * hwc_num);
#endif
#ifdef _MEMPROFILE_STORE_
        if (mem_size > 0)
            memset(mem_array, 0, (onetimealloc + 1) * mprof_size);
#endif
#endif
    }

    virtual ~msl_adv_chain()
    {
        // debug_printf("~msl_adv_chain %llx %llx %llx %llx\n",this,array,array2,array3);
        if (addr_array) {
            t_free(addr_array, (onetimealloc + 1) * addr_size);
        }
#ifdef _HWCOUNTERS_ENABLED_
        if (hwc_array) {
            t_free(hwc_array, (onetimealloc + 1) * hwc_size);
        }
#endif
#ifdef _MEMPROFILE_STORE_
        if (mem_array) {
            t_free(mem_array, (onetimealloc + 1) * mem_size);
        }
#endif
    }
};

static int destroy_whole_chain(msl_base_chain *chain)
{
    msl_base_chain *next;
    while (chain != NULL) {
        next = chain->next;
        delete chain;
        chain = next;
    }
    return (0);
}

static int destroy_shift_start(msl_base_chain **start)
{
    msl_base_chain *next;
    next = (*start)->next;
    delete *start;
    *start = next;
    return (0);
}

struct t_base_event {

    int initialized;

    int t_max_threads;
    int t_max_tid_num;
    int t_tid_counter;
    static pid_t *tid2t_tid;
    pid_t t_pid;

    void set_tsc_adjust(int tid);

    msl_adv_chain **events_start_chain, **events_chain;
    msl_base_chain **extras_start_chain, **extras_chain;
    msl_base_chain **c_extras_start_chain, **c_extras_chain;
    int extras;
    int *event_index;
    int *extras_index;
    int *c_extras_index;
    int get_tid(pid_t tid_in);
    int compress_events;

    msl_extra_info **extras_array;
    msl_extra_info **c_extras_array;
    msl_event **events_array;

    void ***addrs_array;
    long long *uniq_index;

    int num_addrs;
    unsigned long long calls_limit, num_allocated;
#ifdef _HWCOUNTERS_ENABLED_
    long long **hwc_array;
    int num_hwcs;
    HW_counter hwc;
#endif

#ifdef _MEMPROFILE_STORE_
    int t_memory_event_size;
    t_memory_event **mems_array;
#endif
    int use_realtime_dump;

    msl_event fake_event;
    void *fake_addrs[16];
    msl_event **last_returned_event;

    int reuse_event(int tid, msl_event **event, msl_extras_counters *counters, short func_id);
    void allocate_event(int tid, int skip_dump, msl_event **event, msl_extras_counters *counters);
    int reuse_event(int tid, msl_event **event, short func_id);
    void allocate_event(int tid, msl_event **event, short func_id);
    int discard_event(int tid, msl_event **event);
    void check_chain(pid_t tid);
    void fini();
    void init(int max_threads);
    void init_events_storage(int tid);
    void end();
    virtual void dump_extras(pid_t tid, msl_base_chain **chain, int is_colt) = 0;
    virtual void dump_events(pid_t tid, msl_adv_chain **chain) = 0;
    t_base_event();
    ~t_base_event();

#undef __current_extras__
#undef __extras_index__
#undef __extras_current__
#undef __extras_start__

#undef __record_ints__
#undef __record_int__
#undef __record_longs__
#undef __record_low_longs__
#undef __record_long__
#undef __is_colt__

#define __current_extras__ extras_array[tid]
#define __extras_index__ extras_index[tid]
#define __extras_current__ extras_chain[tid]
#define __extras_start__ extras_start_chain[tid]

#define __record_ints__ record_ints
#define __record_int__ record_int
#define __record_longs__ record_longs
#define __record_low_longs__ record_low_longs
#define __record_long__ record_long
#define __is_colt__ 0

#include "records.h"

#undef __current_extras__
#undef __extras_index__
#undef __extras_current__
#undef __extras_start__

#undef __record_ints__
#undef __record_int__
#undef __record_longs__
#undef __record_low_longs__
#undef __record_long__
#undef __is_colt__

#define __current_extras__ c_extras_array[tid]
#define __extras_index__ c_extras_index[tid]
#define __extras_current__ c_extras_chain[tid]
#define __extras_start__ c_extras_start_chain[tid]

#define __record_ints__ record_ints_colt
#define __record_int__ record_int_colt
#define __record_longs__ record_longs_colt
#define __record_low_longs__ record_low_longs_colt
#define __record_long__ record_long_colt
#define __is_colt__ 1

#include "records.h"
};

#if defined(__CPLUSPLUS__) || defined(__cplusplus)
}
#endif
#endif // __T_BASE_EVENT_H__
