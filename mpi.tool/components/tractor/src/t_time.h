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

#ifndef __T_TIME_H__
#define __T_TIME_H__

#include "common_plain_types.h"

#if defined(__CPLUSPLUS__) || defined(__cplusplus)
extern "C" {
#endif

#ifdef _T_USE_GETTID_INT_
#include <sys/syscall.h>
static inline pid_t gettid(void) { return syscall(SYS_gettid); }
#endif

extern long long proc_freq[2];
extern PEAT_Clock t_tsc_init;

#define PEAT_BILLION 1E9
static __inline__ PEAT_Clock peat_get_time_nano_sec(clockid_t clk_id)
{
    timespec ts;
    int result = clock_gettime(clk_id, &ts);
    return ts.tv_sec * PEAT_BILLION + ts.tv_nsec;
}

static __inline__ void rdtsc_init(void)
{
    unsigned long long high, low;
    __asm__ __volatile__("rdtsc" : "=a"(low), "=d"(high));
    t_tsc_init = (high << 32) + low;
}

static __inline__ PEAT_Clock rdtsc(void)
{
    unsigned long long high, low;
    __asm__ __volatile__("rdtsc" : "=a"(low), "=d"(high));
    return (high << 32) + low - t_tsc_init;
}

static __inline__ PEAT_Clock rdtscp(PEAT_Proc *core_id)
{
    unsigned long long high, low;
    unsigned long c;
    __asm__ __volatile__("rdtscp" : "=a"(low), "=d"(high), "=c"(c));
    *core_id = c & 0xfff;
    return (high << 32) + low - t_tsc_init;
}

static __inline__ PEAT_Clock rdtscpt(pid_t *tid, PEAT_Proc *core_id)
{
    *tid = gettid();
    unsigned long long high, low, tsc;
    unsigned long c;
    __asm__ __volatile__("rdtscp" : "=a"(low), "=d"(high), "=c"(c));
    *core_id = c & 0xfff;
    tsc = (high << 32) + low - t_tsc_init;
    return tsc;
}

static __inline__ PEAT_Clock rdtsct(pid_t *tid)
{
    *tid = gettid();
    unsigned long long high, low, tsc;
    __asm__ __volatile__("rdtsc" : "=a"(low), "=d"(high));
    tsc = (high << 32) + low - t_tsc_init;
    return tsc;
}

#ifdef USE_NANO_TIMER

extern PEAT_Clock t_nano_init;

extern clockid_t default_timer;

static __inline__ PEAT_Clock peat_get_nano_sec()
{
    return peat_get_time_nano_sec(default_timer) - t_nano_init;
}

static __inline__ void peat_init_nano_sec()
{
    t_nano_init = peat_get_time_nano_sec(default_timer);
}

static __inline__ PEAT_Clock rdtsca(void)
{
    unsigned long long high, low, tsc;
    unsigned long c;
    PEAT_Proc core_id;
    __asm__ __volatile__("rdtscp" : "=a"(low), "=d"(high), "=c"(c));
    core_id = c & 0xfff;
    tsc = (high << 32) + low;
    return tsc + t_tsc_init;
}

static __inline__ PEAT_Clock rdtscca(PEAT_Proc *core_id)
{
    unsigned long long high, low, tsc;
    unsigned long c;
    __asm__ __volatile__("rdtscp" : "=a"(low), "=d"(high), "=c"(c));
    *core_id = c & 0xfff;
    tsc = (high << 32) + low;
    return tsc - t_tsc_init;
}

static __inline__ PEAT_Clock rdtscta(pid_t *tid)
{
    *tid = gettid();
    unsigned long long high, low, tsc;
    unsigned long c;
    PEAT_Proc core_id;
    __asm__ __volatile__("rdtscp" : "=a"(low), "=d"(high), "=c"(c));
    core_id = c & 0xfff;
    tsc = (high << 32) + low;
    return tsc - t_tsc_init;
}

static __inline__ PEAT_Clock rdtscptca(pid_t *tid, PEAT_Proc *core_id)
{
    *tid = gettid();
    unsigned long long high, low, tsc;
    unsigned long c;
    __asm__ __volatile__("rdtscp" : "=a"(low), "=d"(high), "=c"(c));
    *core_id = c & 0xfff;
    tsc = (high << 32) + low;
    return tsc - t_tsc_init;
}
#else
#define rdtsca rdtsc
#define rdtscca rdtscp
#define rdtscta rdtsct
#define rdtscptca rdtscpt
#endif

// Node-wide, core-invariant timer: under USE_NANO_TIMER, backed by clock_gettime
// (comparable across cores/threads); otherwise falls back to the calibrated TSC.
// Callers get the value in whatever unit the rest of the pipeline expects at
// capture time (raw nanoseconds under USE_NANO_TIMER, clockticks otherwise) -
// conversion to clockticks happens once, at trace finalization.
static __inline__ PEAT_Clock peat_get_time(void)
{
#ifdef USE_NANO_TIMER
    return peat_get_nano_sec();
#else
    return rdtsca();
#endif
}

static __inline__ PEAT_Clock peat_get_time_tid(pid_t *tid)
{
#ifdef USE_NANO_TIMER
    *tid = gettid();
    return peat_get_nano_sec();
#else
    return rdtscta(tid);
#endif
}

// core_id is only meaningful to the TSC-calibration path; under USE_NANO_TIMER
// it is unused (clock_gettime is core-invariant by construction) and set to 0.
static __inline__ PEAT_Clock peat_get_time_core(PEAT_Proc *core_id)
{
#ifdef USE_NANO_TIMER
    *core_id = 0;
    return peat_get_nano_sec();
#else
    return rdtscca(core_id);
#endif
}

static __inline__ PEAT_Clock peat_get_time_tid_core(pid_t *tid, PEAT_Proc *core_id)
{
#ifdef USE_NANO_TIMER
    *tid = gettid();
    *core_id = 0;
    return peat_get_nano_sec();
#else
    return rdtscptca(tid, core_id);
#endif
}


static __inline__ PEAT_Proc get_core_id()
{
    unsigned long long a, d;
    unsigned long c;
    __asm__ __volatile__("rdtscp" : "=a"(a), "=d"(d), "=c"(c));
    return c & 0xfff;
}

static __inline__ PEAT_Clock rdpmc_intrin(const unsigned long c)
{
    unsigned long a = 0, d = 0;
    __asm__ __volatile__("rdpmc" : "=a"(a), "=d"(d) : "c"(c));
    return (d << 32) + a;
}

#if defined(__CPLUSPLUS__) || defined(__cplusplus)
}
#endif

#endif //__T_TIME_H__
