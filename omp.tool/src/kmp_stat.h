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

#ifndef __KMP_STAT_H__
#define __KMP_STAT_H__

#if defined(__CPLUSPLUS__) || defined(__cplusplus)
extern "C" {
#endif

enum __EVENTS__ {
    OSL_EMPTY_EVENT = 0,
    OSL_MICROTASK_STARTS,
    OSL_MICROTASK_ENDS,
    OSL_TASKWAIT_STARTS,
    OSL_TASKWAIT_ENDS,
    OSL_TASK_STARTS,
    OSL_TASK_ENDS,
    OSL_CRITICAL_SECTION_STARTS,
    OSL_CRITICAL_SECTION_ENDS,
    OSL_CRITICAL_BARRIER_STARTS,
    OSL_CRITICAL_BARRIER_ENDS,
    OSL_PLAIN_BARRIER_STARTS,
    OSL_PLAIN_BARRIER_ENDS,
    OSL_FORK_BARRIER_STARTS,
    OSL_FORK_BARRIER_ENDS,
    OSL_JOIN_BARRIER_STARTS,
    OSL_JOIN_BARRIER_ENDS,
    OSL_SINGLE_STARTS,
    OSL_SINGLE_ENDS,
    OSL_APPLICATION_STARTS,
    OSL_APPLICATION_ENDS,
    OSL_CALIBRATION_STARTS,
    OSL_FREQUENCY,
    OSL_EXECUTABLE_FILE,
    OSL_SERIALIZED_MICROTASK_STARTS,
    OSL_SERIALIZED_MICROTASK_ENDS,
    OSL_SERIALIZED_TEAM_MICROTASK_STARTS,
    OSL_SERIALIZED_TEAM_MICROTASK_ENDS,
    OSL_FORK_CALL,
    OSL_TASKFORK_CALL,
    OSL_START_ONDEMAND,
    OSL_STOP_ONDEMAND,
    OSL_EVENT_DESCRIPTION,
    OSL_LIST_END,
};
extern int kmp_sec_ret_addr;
extern void *kmp_highval;
extern void *kmp_binval;
extern void *kmp_lowval;

typedef struct _microtask_event_ {
#ifdef USE_PAPI
    long long int counters[MAX_NUM_CPU_EVENTS];
#endif
    long long tsc, uniq_id;
    void *p_entry;
    int event_type;
    int tid;
    int gtid;
    int gtmid;
} microtask_event;

#if defined(__CPLUSPLUS__) || defined(__cplusplus)
long long __osl_record_event(int event_type, int tid, int gtid, int gtmid, const void *p_entry, long long uniq_id_in = -1, int forced = 0);
#else
long long __osl_record_event(int event_type, int tid, int gtid, int gtmid, const void *p_entry, long long uniq_id_in, int forced);
#endif
void __osl_calibrate_counters(int thread_id);
void __osl_begin_application(int max_threads, int max_cores);
void __osl_end_application(void);
void __osl_start_microtiming(void);
void __osl_stop_microtiming(void);

static __inline__ long long rdtsc(void)
{
    unsigned long long high, low;
    __asm__ __volatile__("rdtsc" : "=a"(low), "=d"(high));
    return (high << 32) + low;
}

#if defined(__CPLUSPLUS__) || defined(__cplusplus)
}
#endif

#endif