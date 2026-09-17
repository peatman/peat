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
// #include "funcprop.h"
#include "t_stat.h"
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>

#include "tractor.h"

#if defined(__CPLUSPLUS__) || defined(__cplusplus)
extern "C" {
#endif

#ifdef _MEM_HOOKS_
#ifdef _MEM_MUTEX_
pthread_mutex_t malloc_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t calloc_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t realloc_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t free_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t memalign_mutex = PTHREAD_MUTEX_INITIALIZER;
#endif
void *malloc(size_t len)
{
    void *ret = 0;
    if (!t_memory_profiler::malloc_orig) {                                                                           // called before ctor completed. No instance constructed yet
        t_memory_profiler::malloc_orig = reinterpret_cast<void *(*)(size_t)>(dlsym(RTLD_NEXT, MEMPROF_MALLOC_NAME)); // also allocate memory
        t_memory_profiler::acc_before_ctor.count += 1;
        t_memory_profiler::acc_before_ctor.size += len;
    }
    assert(t_memory_profiler::malloc_orig);
    ret = t_memory_profiler::malloc_orig(len);

    if (t_memory_profiler::can_collect) {
#ifdef _MEM_MUTEX_
        pthread_mutex_lock(&malloc_mutex);
#endif
        PEAT_Addr ret_addr = reinterpret_cast<PEAT_Addr>(__builtin_return_address(0));
        if (ret_addr >= gt_info(lowtrac) && ret_addr <= gt_info(hightrac))
            bytes_allocated += len;
        t_memory_profiler::can_collect = false;
        t_memory_profiler_event evt;
        evt.size = len;
        evt.time = rdtsca();
        evt.type = mpe_malloc;
        evt.addr_obj = ret_addr;
        evt.addr_ptr_new = reinterpret_cast<PEAT_Addr>(ret);
#ifndef _SKIP_MEM_EVENTS_
        gt_info(mprof).events.push_back(evt);
#endif
        t_memory_profiler::total_events += 1;
        t_memory_profiler::can_collect = true;
#ifdef _MEM_MUTEX_
        pthread_mutex_unlock(&malloc_mutex);
#endif
    }
    return ret;
}

#define dumb_area_size 65536
char dumb_arr[dumb_area_size];

void clear_area(void *ptr, size_t len)
{
    char *t_ptr = (char *)ptr;
    for (size_t i = 0; i < len; i++)
        t_ptr[i] = 0;
}

void *calloc(size_t count, size_t len)
{
    void *ret = 0;
    if (!t_memory_profiler::calloc_orig) { // called before ctor completed. No instance constructed yet
        // dlsym calls calloc
        // t_memory_profiler::calloc_orig = reinterpret_cast<void *(*)(size_t, size_t)>(dlsym(RTLD_NEXT, MEMPROF_CALLOC_NAME));
        // assert(t_memory_profiler::calloc_orig);
        t_memory_profiler::acc_before_ctor.count += 1;
        ret = &dumb_arr[t_memory_profiler::acc_before_ctor.size]; // mmap(0, len * count, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
        t_memory_profiler::acc_before_ctor.size += len * count;
        // ret = malloc(count*len);
        assert(t_memory_profiler::acc_before_ctor.size + count * len < dumb_area_size);
        clear_area(ret, count * len);
    } else {
        ret = t_memory_profiler::calloc_orig(count, len);
        if (t_memory_profiler::can_collect) {
#ifdef _MEM_MUTEX_
            pthread_mutex_lock(&calloc_mutex);
#endif
            t_memory_profiler::can_collect = false;
            PEAT_Addr ret_addr = reinterpret_cast<PEAT_Addr>(__builtin_return_address(0));
            if (ret_addr >= gt_info(lowtrac) && ret_addr <= gt_info(hightrac))
                bytes_allocated += len * count;
            t_memory_profiler_event evt;
            evt.size = len * count;
            evt.time = rdtsca();
            evt.type = mpe_calloc;
            evt.addr_obj = ret_addr;
            evt.addr_ptr_new = reinterpret_cast<PEAT_Addr>(ret);
#ifndef _SKIP_MEM_EVENTS_
            gt_info(mprof).events.push_back(evt);
#endif
            t_memory_profiler::total_events += 1;
            t_memory_profiler::can_collect = true;
#ifdef _MEM_MUTEX_
            pthread_mutex_unlock(&calloc_mutex);
#endif
        }
    }
    return ret;
}
void *memalign(size_t alignment, size_t size)
{

    void *ret = 0;
    if (!t_memory_profiler::memalign_orig) { // called before ctor completed. No instance constructed yet
        t_memory_profiler::memalign_orig = reinterpret_cast<void *(*)(size_t, size_t)>(dlsym(RTLD_NEXT, MEMPROF_MEMALIGN_NAME));
        t_memory_profiler::acc_before_ctor.count += 1;
        t_memory_profiler::acc_before_ctor.size += size;
    }

    assert(t_memory_profiler::memalign_orig);
    ret = t_memory_profiler::memalign_orig(alignment, size);

    if (t_memory_profiler::can_collect) {
#ifdef _MEM_MUTEX_
        pthread_mutex_lock(&memalign_mutex);
#endif
        t_memory_profiler::can_collect = false;
        PEAT_Addr ret_addr = reinterpret_cast<PEAT_Addr>(__builtin_return_address(0));
        if (ret_addr >= gt_info(lowtrac) && ret_addr <= gt_info(hightrac))
            bytes_allocated += size;
        t_memory_profiler_event evt;
        evt.size = size;
        evt.time = rdtsca();
        evt.type = mpe_memalign;
        evt.addr_obj = ret_addr;
        evt.addr_ptr_new = reinterpret_cast<PEAT_Addr>(ret);
#ifndef _SKIP_MEM_EVENTS_
        gt_info(mprof).events.push_back(evt);
#endif
        t_memory_profiler::total_events += 1;
        t_memory_profiler::can_collect = true;
#ifdef _MEM_MUTEX_
        pthread_mutex_unlock(&memalign_mutex);
#endif
    }
    return ret;
}

void *realloc(void *ptr, size_t size)
{
    void *ret = 0;
    if (!t_memory_profiler::realloc_orig) { // called before ctor completed. No instance constructed yet
        t_memory_profiler::realloc_orig = reinterpret_cast<void *(*)(void *, size_t)>(dlsym(RTLD_NEXT, MEMPROF_REALLOC_NAME));
        t_memory_profiler::acc_before_ctor.count += 1;
        t_memory_profiler::acc_before_ctor.size += size;
    }

    assert(t_memory_profiler::realloc_orig);
    ret = t_memory_profiler::realloc_orig(ptr, size);

    if (t_memory_profiler::can_collect) {
#ifdef _MEM_MUTEX_
        pthread_mutex_lock(&realloc_mutex);
#endif
        t_memory_profiler::can_collect = false;
        PEAT_Addr ret_addr = reinterpret_cast<PEAT_Addr>(__builtin_return_address(0));
        if (ret_addr >= gt_info(lowtrac) && ret_addr <= gt_info(hightrac))
            bytes_allocated += size;
        t_memory_profiler_event evt;
        evt.size = size;
        evt.time = rdtsca();
        evt.type = mpe_realloc;
        evt.addr_obj = ret_addr;
        evt.addr_ptr_new = reinterpret_cast<PEAT_Addr>(ret);
        evt.addr_ptr_free = reinterpret_cast<PEAT_Addr>(ptr);
#ifndef _SKIP_MEM_EVENTS_
        gt_info(mprof).events.push_back(evt);
#endif
        t_memory_profiler::total_events += 1;
        t_memory_profiler::can_collect = true;
#ifdef _MEM_MUTEX_
        pthread_mutex_unlock(&realloc_mutex);
#endif
    }
    return ret;
}

void free(void *ptr)
{
#if 1
    if (ptr >= dumb_arr && ptr < dumb_arr + dumb_area_size) {
        return;
    }
#endif
    if (!t_memory_profiler::free_orig) { // called before ctor completed. No instance constructed yet
        t_memory_profiler::free_orig = reinterpret_cast<void (*)(void *)>(dlsym(RTLD_NEXT, MEMPROF_FREE_NAME));
        t_memory_profiler::acc_before_ctor.count += 1;
    }

    assert(t_memory_profiler::free_orig);
    t_memory_profiler::free_orig(ptr);

    if (t_memory_profiler::can_collect) {
#ifdef _MEM_MUTEX_
        pthread_mutex_lock(&free_mutex);
#endif
        t_memory_profiler::can_collect = false;
        PEAT_Addr ret_addr = reinterpret_cast<PEAT_Addr>(__builtin_return_address(0));
        t_memory_profiler_event evt;
        evt.time = rdtsca();
        evt.type = mpe_free;
        evt.addr_obj = ret_addr;
        evt.addr_ptr_free = reinterpret_cast<PEAT_Addr>(ptr);
#ifndef _SKIP_MEM_EVENTS_
        gt_info(mprof).events.push_back(evt);
#endif
        t_memory_profiler::total_events += 1;
        t_memory_profiler::can_collect = true;
#ifdef _MEM_MUTEX_
        pthread_mutex_unlock(&free_mutex);
#endif
    }
    return;
}
#endif

#if defined(__CPLUSPLUS__) || defined(__cplusplus)
}
#endif
