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

#ifndef __T_ALLOC_H__
#define __T_ALLOC_H__
#include <malloc.h>
#include <new>
#include <stdlib.h>
#include <t_defines.h>

#if defined(__CPLUSPLUS__) || defined(__cplusplus)
extern "C" {
#endif

extern long onetimealloc;
extern int t_mpi_rank;
extern long long bytes_allocated;
const char bad_alloc_err[] = "Memory Allocation Error\n";
#if 1 // ndef OPEN_MPI

#ifdef _TRACTOR_DIAG_
#define t_malloc(s, a)   \
    t_malloc_impl(s, a); \
    debug_printf(" at %s:%d\n", __FILE__, __LINE__)
#define t_free(s, a)   \
    t_free_impl(s, a); \
    debug_printf(" at %s:%d\n", __FILE__, __LINE__)
#else
#define t_malloc(s, a) t_malloc_impl(s, a)
#define t_free(s, a) t_free_impl(s, a)
#endif

static void *t_malloc_impl(const size_t size, const size_t align_size)
{
    // if(size ==0) debug_printf("Alloc:Zero size requested\n");
    void *chunk = 0;
    int result = posix_memalign(&chunk, align_size, size);
    if (result == 0) {
        bytes_allocated += size;
    } else {
        const char *err = bad_alloc_err;
        perror(bad_alloc_err);
        abort();
    }
    debug_printf("Requested %ld bytes,total %lld ", size, bytes_allocated);
    return chunk;
}
static void t_free_impl(void *ptr, const size_t size)
{
    bytes_allocated -= size;
    debug_printf("Freed %ld bytes,total %lld", size, bytes_allocated);
    free(ptr);
}

#else
#define t_malloc(x, y) malloc((x))
#define t_free(x, y) free((x))
#endif

#if defined(__CPLUSPLUS__) || defined(__cplusplus)
}
#endif
#ifdef _USE_T_ALLOCATOR_
#include <memory>

template <class T, class T_Type, class Base_Allocator = std::allocator<T>>
class T_Allocator : public Base_Allocator
{
  public:
    typedef typename Base_Allocator::pointer pointer;
    typedef typename Base_Allocator::size_type size_type;

    T_Allocator() throw() : Base_Allocator() {}
    T_Allocator(const T_Allocator &b) throw() : Base_Allocator(b) {}
    T_Allocator(T_Allocator &&b) throw() : Base_Allocator(b) {}
    template <class T>
    T_Allocator(const typename T_Allocator::template rebind<T>::other &b) throw() : Base_Allocator(b) {}
    ~T_Allocator() {}

    template <class T>
    struct rebind {
        typedef T_Allocator<T, T_Type, typename Base_Allocator::template rebind<T>::other> other;
    };

    pointer allocate(size_type n)
    {
        // debug_printf ("Allocate called:%d\n",n*sizeof(T));
        pointer r = Base_Allocator::allocate(n * sizeof(T));
        bytes_allocated += n * sizeof(T);
        return r;
    }
    pointer allocate(size_type n, pointer h)
    {
        // debug_printf ("Allocate called:%d\n",n*sizeof(T));
        pointer r = Base_Allocator::allocate(n, h);
        bytes_allocated += n * sizeof(T);
        return r;
    }
    void deallocate(pointer p, size_type n) throw()
    {
        // debug_printf ("Deallocate called:%d\n",n*sizeof(T));
        Base_Allocator::deallocate(p, n);
        bytes_allocated -= n * sizeof(T);
    }
};
#endif

#endif
