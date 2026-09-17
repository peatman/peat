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

#ifndef __MEMPROFILE_H__
#define __MEMPROFILE_H__

#include <dlfcn.h>
#include <list>
#include <string>
#include <zlib.h>

// #include <linux/limits.h> //LLONG_MAX
#include <climits> //LLONG_MAX

#include "common_plain_types.h"
#include "hwcounter.h"

#define MEMPROF_MALLOC_NAME "malloc"
#define MEMPROF_CALLOC_NAME "calloc"
#define MEMPROF_REALLOC_NAME "realloc"
#define MEMPROF_MEMALIGN_NAME "memalign"
#define MEMPROF_FREE_NAME "free"

enum t_memory_profiler_event_type {
    mpe_none,
    mpe_malloc,
    mpe_calloc,
    mpe_realloc,
    mpe_memalign,
    mpe_free,
    mpe_size
};

struct t_memory_profiler_event {
    t_memory_profiler_event() : size(0), time(0), addr_obj(0), addr_ptr_new(0), addr_ptr_free(0), type(mpe_none) {}
    PEAT_Num size;
    PEAT_Clock time;
    PEAT_Addr addr_obj;
    PEAT_Addr addr_ptr_new;
    PEAT_Addr addr_ptr_free; // realloc parameter
    t_memory_profiler_event_type type;
};

struct t_memory_profiler_accumulator { // used to accumulate memory allocation calls before class instance initialized
    t_memory_profiler_accumulator() {} //: size(0), count(0) {}
    PEAT_MPI_Message_Size size;
    PEAT_Num count;
};

struct t_memory_profiler_object_addresses { // used to translate addresses into object IDs
    t_memory_profiler_object_addresses() : id(-1), high(0), low((PEAT_Addr)LLONG_MAX) {}
    PEAT_Signed_Num id;
    PEAT_Addr high;
    PEAT_Addr low;
    std::string obj_name;
};

struct t_memory_profiler_common_stat {
    t_memory_profiler_common_stat() : count(0), current_size(0), max_size(0) {}
    long long count;
    long long current_size;
    long long max_size;
};

#include "t_stat.h"

struct t_memory_profiler {
    t_memory_profiler();
    ~t_memory_profiler();
    PEAT_Num get_max_memory_RSS_in_KB() const;
    void get_mem_status(t_memory_event *evt);

    PEAT_Addr system_page_size;
    int statm_file; // /proc/self/statm opened file descriptor
#ifdef _MEM_HOOKS_
    void enable_memory_profiling()
    {
        can_collect = true;
    }
    void disable_memory_profiling()
    {
        can_collect = false;
    }
    PEAT_Clock get_total_events() const
    {
        return total_events;
    }
    void initialize_object_addresses(const std::map<PEAT_Num, shared_object_info> &obj);
    void print_memory_profile_report(gzFile file, const PEAT_Record_Type line_prefix1, const PEAT_Record_Type line_prefix2) const;
    void set_add_vars(void *lh, void *hh, void *ls, void *hs)
    {
        lowheap = lh;
        highheap = hh;
        lowstack = ls;
        highstack = hs;
    };
    PEAT_Clock fill_common_statistic(const std::string &name, const std::string &mode);
#if 1
    static __thread bool can_collect; // enable\disable event collection
//    static __thread PEAT_Clock total_events;//total event collected
//    static __thread PEAT_LNum mem_used_by_tractor;
#else
    static bool can_collect; // enable\disable event collection
#endif
    static PEAT_Clock total_events; // total event collected

    static t_memory_profiler_accumulator acc_before_ctor; // accumulate information before Ctor called

    static __thread void *(*malloc_orig)(size_t);           // original function handler. Initialized in Ctor
    static __thread void *(*calloc_orig)(size_t, size_t);   // original function handler. Initialized in Ctor
    static __thread void *(*realloc_orig)(void *, size_t);  // original function handler. Initialized in Ctor
    static __thread void *(*memalign_orig)(size_t, size_t); // original function handler. Initialized in Ctor
    static __thread void (*free_orig)(void *);              // original function handler. Initialized in Ctor

    // Addresses storage to translate into shared object ID
#ifdef _USE_T_ALLOCATOR_
    std::map<PEAT_Addr, t_memory_profiler_object_addresses, std::less<PEAT_Addr>, T_Allocator<PEAT_Addr, std::map<PEAT_Addr, t_memory_profiler_object_addresses>>> obj_id; // map<low address, t_memory_profiler_object_addresses>
    std::list<t_memory_profiler_event, T_Allocator<t_memory_profiler_event, std::list<t_memory_profiler_event>>> events;
#else
    std::map<PEAT_Addr, t_memory_profiler_object_addresses> obj_id; // map<low address, t_memory_profiler_object_addresses>
    std::list<t_memory_profiler_event> events;
#endif
    PEAT_Signed_Num get_obj_id(const PEAT_Addr addr) const;
    std::string get_obj_name(const PEAT_Signed_Num id) const;

    // Common statistic
    std::map<PEAT_Signed_Num, t_memory_profiler_common_stat> mp_common_stat; // map<obj_id, memory_profiler_common_stat>
    PEAT_Clock max_water_level;                                              // maximum memory allocated in all objects

    void *highheap;
    void *lowheap;
    void *highstack;
    void *lowstack;
#endif
};

#endif //__MEMPROFILE_H__
