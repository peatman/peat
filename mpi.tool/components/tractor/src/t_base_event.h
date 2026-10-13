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

    /* Nesting. A traced call issued from inside another traced call of the same thread (the MPI progress
       engine runs the schedules of outstanding nonblocking collectives while a blocking call spins it) is
       recorded one level deeper. Every (thread, level) pair has its own event and extras storage, so
       records within one level never overlap and the slot order equals the extras order again. A record
       stores the number of its children (records of the next level created while it was open). */
    int t_max_nesting_levels;      /* storage depth, PEAT_TRACTOR_MAX_NESTING_LEVELS; deeper calls abort */
    int t_achieved_nesting_levels; /* deepest level used so far + 1, written to the com file for the readers */
    int *nesting_level;            /* [tid] current depth of the thread */

    msl_adv_chain ***events_start_chain, ***events_chain;   /* [tid][level] */
    msl_base_chain ***extras_start_chain, ***extras_chain; /* [tid][level] */
    int extras;
    int **event_index;  /* [tid][level] index within the current chain block */
    int **extras_index; /* [tid][level] */
    int get_tid(pid_t tid_in);
    int compress_events;

    msl_extra_info ***extras_array; /* [tid][level] current block */
    msl_event ***events_array;      /* [tid][level] current block */
    void ****addrs_array;           /* [tid][level] */
    long long **uniq_index;         /* [tid][level] records ever created at that level (monotonic) */

    int num_addrs;
    unsigned long long calls_limit, num_allocated;
#ifdef _HWCOUNTERS_ENABLED_
    long long ***hwc_array; /* [tid][level] */
    int num_hwcs;
    HW_counter hwc;
#endif

#ifdef _MEMPROFILE_STORE_
    int t_memory_event_size;
    t_memory_event ***mems_array; /* [tid][level] */
#endif
    int use_realtime_dump;

    msl_event fake_event;
    void *fake_addrs[16];
    msl_event ***last_returned_event; /* [tid][level] */

    /* nesting bookkeeping for the generated wrappers */
    int enter_call(int tid);                          /* returns the level of the call, increments the depth */
    void leave_call(int tid);                         /* decrements the depth */
    long long child_base(int tid, int level);         /* uniq_index of the next level: children = growth since entry */
    int current_level(int tid) { return nesting_level ? nesting_level[tid] : 0; }
    void uncount_event(int tid, int level) { uniq_index[tid][level]--; } /* second record of a pair (sendrecv) */

    int reuse_event(int tid, int level, msl_event **event, msl_extras_counters *counters, short func_id);
    void allocate_event(int tid, int level, int skip_dump, msl_event **event, msl_extras_counters *counters);
    int reuse_event(int tid, msl_event **event, short func_id);     
    void allocate_event(int tid, msl_event **event, short func_id); 
    int discard_event(int tid, int level, msl_event **event, long long children_base);
    void check_chain(pid_t tid, int level);
    void fini();
    void init(int max_threads, int max_nesting_levels);
    void init_events_storage(int tid);            /* level 0 of a new thread */
    void init_level_storage(int tid, int level);  /* lazily, when a level is first used */
    void end();
    virtual void dump_extras(pid_t tid, int level, msl_base_chain **chain) = 0;
    virtual void dump_events(pid_t tid, int level, msl_adv_chain **chain) = 0;
    t_base_event();
    ~t_base_event();

    int record_ints(const int tid, const int level, const int *values, const int num);
    int record_int(const int tid, const int level, const int value);
    int record_longs(const int tid, const int level, const long long *values, const int num);
    int record_low_longs(const int tid, const int level, const long long *values, const int num);
    int record_long(const int tid, const int level, const long long value);
};

#if defined(__CPLUSPLUS__) || defined(__cplusplus)
}
#endif
#endif // __T_BASE_EVENT_H__
