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
extern double freq_ratio;
extern double freq_ratio_uncorrected;
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

#define MAX_CORE_ID 4096
struct t_tsc_core_adjust {
    PEAT_Clock tsc = 0;
    PEAT_Clock tsc_diff = 0;
    PEAT_Clock nanosec = 0;
};

extern PEAT_Clock tsc_adjustments[MAX_CORE_ID];
extern PEAT_Byte tsc_adjustments_made[MAX_CORE_ID];
extern t_tsc_core_adjust tsc_core_adjust[MAX_CORE_ID];
extern PEAT_Proc lead_core_id;
extern clockid_t default_timer;

static __inline__ PEAT_Clock peat_get_nano_sec()
{
    return peat_get_time_nano_sec(default_timer) - t_nano_init;
}

static __inline__ void peat_init_nano_sec()
{
    t_nano_init = peat_get_time_nano_sec(default_timer);
}

static __inline__ void make_tsc_adjustment()
{
    PEAT_Proc begin_core, end_core;
    PEAT_Clock before_tsc = rdtscp(&begin_core);
    PEAT_Clock nanosec = peat_get_time_nano_sec(default_timer);
    PEAT_Clock after_tsc = rdtscp(&end_core);

    if (begin_core != end_core)
        return;
    PEAT_Proc cid = begin_core;
    if (lead_core_id == -1)
        lead_core_id = cid;

    tsc_core_adjust[cid].nanosec = nanosec;
    tsc_core_adjust[cid].tsc = (after_tsc + before_tsc) / 2;
    tsc_core_adjust[cid].tsc_diff = after_tsc - before_tsc;
    PEAT_Ratio ratio = (PEAT_Ratio)proc_freq[0] / (PEAT_Ratio)PEAT_BILLION;
    PEAT_Clock nano_difference = tsc_core_adjust[cid].nanosec - tsc_core_adjust[lead_core_id].nanosec;
    PEAT_Clock projected_tsc_difference = PEAT_Clock((PEAT_Ratio)nano_difference * ratio);
    // tsc_adjustments[cid]=tsc_core_adjust[lead_core_id].tsc + projected_tsc_difference - tsc_core_adjust[cid].tsc;

    PEAT_Clock tsc_difference = tsc_core_adjust[cid].tsc - tsc_core_adjust[lead_core_id].tsc;
    tsc_adjustments[cid] = tsc_difference - projected_tsc_difference;

    tsc_adjustments_made[cid] = 1;
}

static __inline__ PEAT_Clock rdtsca(void)
{
    unsigned long long high, low, tsc;
    unsigned long c;
    PEAT_Proc core_id;
    do {
        __asm__ __volatile__("rdtscp" : "=a"(low), "=d"(high), "=c"(c));
        core_id = c & 0xfff;
        if (tsc_adjustments_made[core_id] == 0) {
            make_tsc_adjustment();
        }
    } while (tsc_adjustments_made[core_id] == 0);
    tsc = (high << 32) + low;
    return tsc + tsc_adjustments[core_id] - t_tsc_init;
}

static __inline__ PEAT_Clock rdtscca(PEAT_Proc *core_id)
{
    unsigned long long high, low, tsc;
    unsigned long c;
    do {
        __asm__ __volatile__("rdtscp" : "=a"(low), "=d"(high), "=c"(c));
        *core_id = c & 0xfff;
        if (tsc_adjustments_made[*core_id] == 0) {
            make_tsc_adjustment();
        }
    } while (tsc_adjustments_made[*core_id] == 0);
    tsc = (high << 32) + low;
    return tsc + tsc_adjustments[*core_id] - t_tsc_init;
}

static __inline__ PEAT_Clock rdtscta(pid_t *tid)
{
    *tid = gettid();
    unsigned long long high, low, tsc;
    unsigned long c;
    PEAT_Proc core_id;
    do {
        __asm__ __volatile__("rdtscp" : "=a"(low), "=d"(high), "=c"(c));
        core_id = c & 0xfff;
        if (tsc_adjustments_made[core_id] == 0) {
            make_tsc_adjustment();
        }
    } while (tsc_adjustments_made[core_id] == 0);
    tsc = (high << 32) + low;
    return tsc + tsc_adjustments[core_id] - t_tsc_init;
}

static __inline__ PEAT_Clock rdtscptca(pid_t *tid, PEAT_Proc *core_id)
{
    *tid = gettid();
    unsigned long long high, low, tsc;
    unsigned long c;
    do {
        __asm__ __volatile__("rdtscp" : "=a"(low), "=d"(high), "=c"(c));
        *core_id = c & 0xfff;
        if (!tsc_adjustments_made[*core_id]) {
            make_tsc_adjustment();
        }
    } while (tsc_adjustments_made[*core_id] == 0);
    tsc = (high << 32) + low;
    return tsc + tsc_adjustments[*core_id] - t_tsc_init;
}
#else
#define rdtsca rdtsc
#define rdtscca rdtscp
#define rdtscta rdtsct
#define rdtscptca rdtscpt
#endif

#define IA32_TSC_ADJUST 0x3b

static __inline__ PEAT_Clock rdtsc_adjust(void)
{
    unsigned long long high, low;
    __asm__ __volatile__("rdmsr" : "=a"(low), "=d"(high) : "c"(IA32_TSC_ADJUST));
    return (high << 32) + low;
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
