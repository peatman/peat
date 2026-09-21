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

#ifndef __GLOBALS_H__
#define __GLOBALS_H__
#ifdef USE_JE_MALLOC
#include "jemalloc/jemalloc.h"
#endif
#ifdef USE_MS_CONCURRENCY
#include <ppl.h>
#endif

#include <QBrush>
#include <QPen>
#if !defined(_WIN32) || defined(__MINGW32__)
#include <x86intrin.h>
#endif
extern long long freq;
#define def_progress_threshold 1
#define progress(prev_clk, action)                                  \
    {                                                               \
        long long cur_clk = __rdtsc();                              \
        if (prev_clk + (def_progress_threshold) * freq < cur_clk) { \
            prev_clk = cur_clk;                                     \
            action();                                               \
        }                                                           \
    }
void get_cpu_freq();
struct TV_TraceParts {
    bool mpi_functions[4] = {true, true, true, true};
    bool mpi_messages[4] = {true, true, true, true};
    bool omp_functions[5] = {true, true, true, true, true};
    bool gpu_functions[3] = {true, true, true};
    bool fab_functions = true;
    /*
    bool
        mpi_local_functions=true,
        mpi_p2p_functions=true,
        mpi_collective_functions=true,
        mpi_request_functions=true,
        mpi_p2p_messages=true,
        mpi_collective_messages=true,
        mpi_initiation_messages=true,
        mpi_completion_messages=true;
    bool
        omp_local=true,
        omp_parallel_regions=true,
        omp_sync_region=true,
        omp_sync_region_wait=true;
        omp_target=true;
    bool
        gpu_local=true,
        gpu_transfer=true,
        gpu_execution=true;
    */
};
enum TV_Brush_Event_Type {
    TV_BRUSH_EVENT_TYPE_USER_CODE,
    TV_BRUSH_EVENT_TYPE_SOURCE,
    TV_BRUSH_EVENT_TYPE_DESTINATION,
    TV_BRUSH_EVENT_TYPE_SAND,
    TV_BRUSH_EVENT_TYPE_COLLECTIVE,
    TV_BRUSH_EVENT_TYPE_START,
    TV_BRUSH_EVENT_TYPE_COMPLETION,
    TV_BRUSH_EVENT_TYPE_OMPT_PARALLEL,
    TV_BRUSH_EVENT_TYPE_OMPT_LOCAL,
    TV_BRUSH_EVENT_TYPE_GPU,
    TV_BRUSH_EVENT_TYPE_OTHER,
    TV_BRUSH_EVENT_TYPE_PE,
    TV_BRUSH_EVENT_TYPE_FAB,
    TV_BRUSH_EVENT_TYPE_MAX
};

enum TV_Pen_Message_Type {
    TV_PEN_MESSAGE_TYPE_NONE,
    TV_PEN_MESSAGE_TYPE_NORMAL,
    TV_PEN_MESSAGE_TYPE_REVERSED,
    TV_PEN_MESSAGE_TYPE_INITIATION,
    TV_PEN_MESSAGE_TYPE_COMPLETION,
    TV_PEN_MESSAGE_TYPE_COLLECTIVE,
    TV_PEN_MESSAGE_TYPE_GPU_HD_TRANSFER,
    TV_PEN_MESSAGE_TYPE_GPU_EXEC,
    TV_PEN_MESSAGE_TYPE_MAX
};

struct TV_Colors {
    QPen message_pens[TV_PEN_MESSAGE_TYPE_MAX];
    QBrush event_brushes[TV_BRUSH_EVENT_TYPE_MAX];
    QPen rank_text;
    QPen func_line;
    QPen pe_line;
    QPen func_text;
    QBrush failed_msg;
    TV_Colors();
};
struct TV_Global_Colors : public TV_Colors {
    TV_Global_Colors();
    ~TV_Global_Colors() {}
    void load();
    void save();
};

extern TV_Global_Colors tv_colors;
#if defined(_WIN32) && !defined(__MINGW32__)
// #define USE_MS_CONCURRENCY
#endif
#if defined(USE_MS_CONCURRENCY) && defined(USE_JE_MALLOC)
#error "USE_MS_CONCURRENCY and USE_JE_MALLOC cannot be defined together"
#eilf defined(USE_MS_CONCURRENCY)
#define ALT_ALLOCATOR                      \
    static void *operator new(size_t size) \
    {                                      \
        return concurrency::Alloc(size);   \
    }                                      \
    static void operator delete(void *p)   \
    {                                      \
        concurrency::Free(p);              \
    }
#elif defined(USE_JE_MALLOC)
#define ALT_ALLOCATOR                      \
    static void *operator new(size_t size) \
    {                                      \
        return je_malloc(size);            \
    }                                      \
    static void operator delete(void *p)   \
    {                                      \
        je_free(p);                        \
    }
#else
#define ALT_ALLOCATOR
#endif

#endif // __GLOBALS_H__
