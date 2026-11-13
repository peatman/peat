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
#include <set>
#include "../common/common_plain_types.h"
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
class QString;
// a duration or a time offset in seconds as text with a fitting unit (s, ms, us, ns)
QString format_seconds(double seconds, int digits = 4);
// index in TV_TraceParts::mpi_functions; the internal (PEAT_PROP_MPI_INTERNAL) group follows its user group
enum TV_MPI_Func_Group {
    TV_MPI_FUNC_GROUP_LOCALS,
    TV_MPI_FUNC_GROUP_P2P,
    TV_MPI_FUNC_GROUP_P2P_INTERNAL,
    TV_MPI_FUNC_GROUP_COLLECTIVES,
    TV_MPI_FUNC_GROUP_REQUESTS,
    TV_MPI_FUNC_GROUP_REQUESTS_INTERNAL,
    TV_MPI_FUNC_GROUP_MAX
};
// what the colour of an event shows (TV_TraceParts::color_mode)
enum TV_Color_Mode {
    TV_COLOR_BY_KIND,     // kind of the event, the colours of the legend
    TV_COLOR_BY_FUNCTION, // one colour per function name
    TV_COLOR_BY_COMM,     // one colour per communicator, grey for events without one
    TV_COLOR_BY_DURATION, // from blue (1 us and less) to red (1 s and more)
    TV_COLOR_BY_SIZE,     // message size: from blue (1 byte) to red (1 GB and more), grey for events without data
    TV_COLOR_MODE_MAX
};
struct TV_TraceParts {
    bool mpi_functions[TV_MPI_FUNC_GROUP_MAX] = {true, true, true, true, true, true};
    // p2p, blocking collectives, initiation, completion, collective init, collective start, collective completion
    bool mpi_messages[7] = {true, true, true, true, true, true, true};
    bool omp_functions[5] = {true, true, true, true, true};
    bool gpu_functions[3] = {true, true, true};
    bool transport_functions = true;
    int func_min_width = 2;                // functions narrower than this (px) are not drawn, unless over a user region; 1 draws all
    bool draw_offscreen_messages = false;  // draw messages even when both ends are outside the window
    int row_height_percent = 100;          // height of a thread row, in percent of the default (two text lines)
    int color_mode = TV_COLOR_BY_KIND;     // TV_Color_Mode
    // Highlighting: the events that match every condition set here are drawn bright, the others are dimmed
    int highlight_func = 0;                // PEAT_MPI_Func_ID, 0 for any function
    PEAT_MPI_Comm highlight_comm = 0;      // 0 (MPI_COMM_NULL) for any communicator
    long long highlight_min_bytes = 0;     // least message size of the event, 0 for any
    bool highlighting() const { return highlight_func != 0 || highlight_comm != 0 || highlight_min_bytes > 0; }
    std::set<PEAT_MPI_Comm> hidden_comms; // elements of these communicators are not shown
    bool comm_visible(PEAT_MPI_Comm comm) const { return hidden_comms.empty() || hidden_comms.count(comm) == 0; }
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
    TV_BRUSH_EVENT_TYPE_TRANSPORT,
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
    // TV_PEN_MESSAGE_TYPE_COLLECTIVE is the blocking collective; the following ones are appended (saved colours are keyed by index)
    TV_PEN_MESSAGE_TYPE_COLLECTIVE_START,      // starts of a nonblocking (MPI_Ibcast) or persistent (MPI_Start*) collective
    TV_PEN_MESSAGE_TYPE_COLLECTIVE_COMPLETION, // completions (Wait/Test/...) of a nonblocking/persistent collective
    TV_PEN_MESSAGE_TYPE_COLLECTIVE_INIT,       // inits of a persistent collective (MPI_Bcast_init)
    TV_PEN_MESSAGE_TYPE_MAX
};

// index in TV_TraceParts::mpi_messages that shows/hides a collective message type, -1 for other types
inline int collective_message_flag(TV_Pen_Message_Type type)
{
    switch (type) {
    case TV_PEN_MESSAGE_TYPE_COLLECTIVE:
        return 1;
    case TV_PEN_MESSAGE_TYPE_COLLECTIVE_INIT:
        return 4;
    case TV_PEN_MESSAGE_TYPE_COLLECTIVE_START:
        return 5;
    case TV_PEN_MESSAGE_TYPE_COLLECTIVE_COMPLETION:
        return 6;
    default:
        return -1;
    }
}

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
// Colour for the lines drawn over the rows (time marker, rank separators, selection of a range):
// dark on a light "User Code" colour, light on a dark one.
QColor tv_canvas_line_color();
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
