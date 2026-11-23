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
/* Interposed allocator. The originals are looked up with dlsym(RTLD_NEXT), and dlsym itself allocates and frees (glibc 2.44
   frees from inside dlsym), so a hook that calls dlsym while its original is still unknown would call itself forever.
   Therefore: all five originals are resolved at once, once per process (not per thread), under a re-entrancy guard; what
   dlsym allocates meanwhile (and what is allocated before the first resolution cannot happen: the first call resolves) comes
   from a small static arena that is never freed or reused, each block with a header that holds its size so that realloc can
   move it out. A free() reaching the guard before free_orig exists is dropped (the block leaks). */
#define dumb_area_size 65536
#define ARENA_HEADER 16
static char dumb_arr[dumb_area_size] __attribute__((aligned(16)));
static size_t dumb_used = 0;
static __thread bool t_resolving = false;

static inline bool in_arena(const void *ptr)
{
    return ptr >= (const void *)dumb_arr && ptr < (const void *)(dumb_arr + dumb_area_size);
}

static void *arena_alloc(size_t len, size_t alignment)
{
    if (alignment < 16)
        alignment = 16;
    const size_t need = len + alignment + ARENA_HEADER;
    const size_t off = __atomic_fetch_add(&dumb_used, need, __ATOMIC_RELAXED);
    if (off + need > dumb_area_size) {
        static const char msg[] = "tractor: the allocation area for the memory hooks is exhausted\n";
        if (write(2, msg, sizeof(msg) - 1) < 0) {
        }
        abort();
    }
    uintptr_t p = ((uintptr_t)(dumb_arr + off) + ARENA_HEADER + alignment - 1) & ~(uintptr_t)(alignment - 1);
    *(size_t *)(p - ARENA_HEADER) = len;
    memset((void *)p, 0, len);
    return (void *)p;
}

/* all five originals, once; called by the first hook that needs them, and by the profiler's constructor */
void t_mem_resolve_originals(void)
{
    if (t_resolving)
        return;
    t_resolving = true;
    if (!t_memory_profiler::malloc_orig)
        t_memory_profiler::malloc_orig = reinterpret_cast<void *(*)(size_t)>(dlsym(RTLD_NEXT, MEMPROF_MALLOC_NAME));
    if (!t_memory_profiler::calloc_orig)
        t_memory_profiler::calloc_orig = reinterpret_cast<void *(*)(size_t, size_t)>(dlsym(RTLD_NEXT, MEMPROF_CALLOC_NAME));
    if (!t_memory_profiler::realloc_orig)
        t_memory_profiler::realloc_orig = reinterpret_cast<void *(*)(void *, size_t)>(dlsym(RTLD_NEXT, MEMPROF_REALLOC_NAME));
    if (!t_memory_profiler::memalign_orig)
        t_memory_profiler::memalign_orig = reinterpret_cast<void *(*)(size_t, size_t)>(dlsym(RTLD_NEXT, MEMPROF_MEMALIGN_NAME));
    if (!t_memory_profiler::free_orig)
        t_memory_profiler::free_orig = reinterpret_cast<void (*)(void *)>(dlsym(RTLD_NEXT, MEMPROF_FREE_NAME));
    t_resolving = false;
}

void *malloc(size_t len)
{
    void *ret = 0;
    if (!t_memory_profiler::malloc_orig) { // before the first resolution, or while dlsym is working
        t_memory_profiler::acc_before_ctor.count += 1;
        t_memory_profiler::acc_before_ctor.size += len;
        if (t_resolving)
            return arena_alloc(len, 16);
        t_mem_resolve_originals();
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

void *calloc(size_t count, size_t len)
{
    void *ret = 0;
    if (!t_memory_profiler::calloc_orig) { // before the first resolution, or while dlsym is working
        t_memory_profiler::acc_before_ctor.count += 1;
        t_memory_profiler::acc_before_ctor.size += len * count;
        if (t_resolving)
            return arena_alloc(len * count, 16);
        t_mem_resolve_originals();
    }
    assert(t_memory_profiler::calloc_orig);
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
    return ret;
}

void *memalign(size_t alignment, size_t size)
{
    void *ret = 0;
    if (!t_memory_profiler::memalign_orig) { // before the first resolution, or while dlsym is working
        t_memory_profiler::acc_before_ctor.count += 1;
        t_memory_profiler::acc_before_ctor.size += size;
        if (t_resolving)
            return arena_alloc(size, alignment);
        t_mem_resolve_originals();
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
    if (in_arena(ptr)) { // a block of the allocation area: a new one is made and the contents copied, the old one stays
        const size_t old_size = *(size_t *)((char *)ptr - ARENA_HEADER);
        void *p = malloc(size);
        if (p)
            memcpy(p, ptr, old_size < size ? old_size : size);
        return p;
    }
    if (!t_memory_profiler::realloc_orig) { // before the first resolution, or while dlsym is working
        t_memory_profiler::acc_before_ctor.count += 1;
        t_memory_profiler::acc_before_ctor.size += size;
        if (t_resolving) {
            void *p = arena_alloc(size, 16); // ptr is not from the area here, and the old size is unknown: only a null ptr can be served
            return p;
        }
        t_mem_resolve_originals();
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
    if (in_arena(ptr))
        return;
    if (!t_memory_profiler::free_orig) { // before the first resolution, or while dlsym is working
        t_memory_profiler::acc_before_ctor.count += 1;
        if (t_resolving)
            return; // the block leaks: there is nothing to free it with yet
        t_mem_resolve_originals();
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
