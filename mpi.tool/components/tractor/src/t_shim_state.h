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

/* What a shim library (libtractor_transport_<api>.so, built once and preloaded next to any core library) may use of the
 * core: one plain C structure, t_shared, defined and exported by every core library. The shims never see struct tractor,
 * whose layout depends on the build switches of the core (TRANSPORT, HWCOUNTERS, MEMPROFILE, DIAG, GPU, ...); the core
 * reaches its own objects behind the function pointers.
 *
 * Rules, so that a shim and a core built apart keep fitting:
 * - members are only appended, never removed, reordered or changed; a change of the meaning of a member raises
 *   T_SHIM_STATE_VERSION; size is the core's sizeof, a shim needs it to cover the members it uses
 * - no #ifdef'd members, plain C types only (no std:: containers, no class objects)
 * - t_shared is a constant (static) initialization in the core: it is valid as soon as the core is loaded, before any
 *   constructor of the core has run; only the run-time flags change later
 *
 * Shims can be loaded at the same time, also next to a core that has the same class built in: register_class() lets
 * only one copy of a class set itself up. */
#ifndef __T_SHIM_STATE_H__
#define __T_SHIM_STATE_H__

#include <stddef.h>
#include <stdio.h>
#include <sys/types.h>

#define T_SHIM_STATE_VERSION 1

struct msl_event;
struct t_shim_class;

#if defined(__CPLUSPLUS__) || defined(__cplusplus)
extern "C" {
#endif

struct t_shim_state {
    int version; /* T_SHIM_STATE_VERSION of the core */
    int size;    /* sizeof(t_shim_state) of the core */

    /* transport records (TRANSPORT=on cores; the function pointers are NULL in the others) */
    int t_permit_collect_transport;                                       /* set once the core is ready to take them */
    int (*get_tid)(pid_t tid_in);                                         /* tractor's index of the calling thread */
    void (*transport_allocate_event)(int tid, msl_event **event, short func_id); /* a new transport record */
    int (*transport_reuse_event)(int tid, msl_event **event, short func_id);     /* the last one again, if the same call */

    /* --- appended for the event-class shims (OMPT first) --- */
    /* values of the core, copied by it whenever it sets them (t_shim_sync in tractor.cpp); tractor_initialized is
       set last, a shim records nothing before */
    int tractor_initialized;
    int t_max_threads;
    int t_max_nesting_levels;
    int max_ret_level;           /* stack unwinding, see stack_unwind in t_defines.h */
    int *ret_levels;
    int num_addrs;
    int full_ompt_unwind;
    void *lowval, *highval, *binval, *lowfort, *highfort;
    int parse_openmp;            /* PEAT_TRACTOR_PARSE_OPENMP; 1 until the core has read it */
    /* records */
    int (*current_level)(int tid);                                                /* nesting depth of the thread */
    void (*allocate_event)(int tid, int level, msl_event **event, void ***addrs); /* a new record at that level and
                                                                                     where its return addresses go */
    /* a class of events that is being set up (struct t_shim_class, which has to stay valid: static). Returns 0 when a
       class of that name is registered already (the same class built into the core and preloaded as a shim, or two
       shims loaded at once): the caller must then not set itself up. Thread-safe, usable before the core's
       constructor has run. */
    int (*register_class)(const struct t_shim_class *cls);

    /* --- appended for the OpenACC shim --- */
    int (*write_records)(int tid, int level, const long long *values, int num); /* extras of the last record */
    int parse_openacc; /* PEAT_TRACTOR_PARSE_OPENACC; 1 until the core has read it */

    /* --- appended for the CUDA shim (CUPTI, cuBLAS) --- */
    int (*enter_call)(int tid);              /* a call opens a nesting level: its level */
    void (*leave_call)(int tid);
    long long (*child_base)(int tid, int level); /* records of the next level so far (children = growth since entry) */
    void (*t_check_chain)(int tid, int level);   /* after a record is complete */
    int t_pid, t_max_tid_num;                /* thread lanes (CUPTI streams): tractor threads no system thread has */
    const char *(*cuda_kernel_name)(int idx); /* set by the CUDA class: name of a kernel record (msl_event.cuda.name_index) */

    /* --- appended for the OpenCL / Level Zero shims --- */
    int (*write_record)(int tid, int level, long long value); /* one extra of the last record */

    /* --- appended for builds of the core and the shims apart (Makefile, CMake) --- */
    int timer; /* T_SHIM_TIMER of the core: the shims take their timestamps with the same clock (t_time.h) */
};

/* the clock of the timestamps (t_time.h): a shim and the core have to use the same one */
#ifdef USE_NANO_TIMER
#define T_SHIM_TIMER 1 /* clock_gettime nanoseconds (Makefile default, CMake PEAT_TIMER=nano, needed by CUPTI) */
#else
#define T_SHIM_TIMER 0 /* TSC (CMake PEAT_TIMER=tsc) */
#endif

/* An event class (OMPT, OpenACC, CUDA): built into the core or in a shim. */
struct t_shim_class {
    const char *name;          /* the key: one copy per name is set up; also the build information, unless info */
    void (*init)(void);        /* when the core is set up (at once, if it is already), may be NULL */
    void (*finalize)(void);    /* before the trace is written, may be NULL */
    const char *(*info)(void); /* the build information of the trace instead of name, may be NULL */
};

extern struct t_shim_state t_shared; /* defined by the core library (tractor.cpp) */

/* bytes of t_shim_state a shim needs: up to and including the member */
#define T_SHIM_STATE_NEED(member) ((int)(offsetof(struct t_shim_state, member) + sizeof(((struct t_shim_state *)0)->member)))

/* shim side, once at load: state is &t_shared (NULL when no core library is loaded, the shim refers to it weakly);
 * returns false, after a message, when the shim cannot work with that core */
static inline bool t_shim_state_check(const struct t_shim_state *state, int need, const char *shim)
{
    if (!state) {
        fprintf(stderr, "tractor: %s shim disabled - no tractor core library is loaded, or it is older than the shim (preload the core library too)\n", shim);
        return false;
    }
    if (state->version != T_SHIM_STATE_VERSION || state->size < need) {
        fprintf(stderr, "tractor: %s shim disabled - built for another version of the tractor core library (interface %d, %d bytes; the shim needs %d, %d bytes): rebuild both from the same sources\n",
                shim, state->version, state->size, T_SHIM_STATE_VERSION, need);
        return false;
    }
    /* a core of the same sources has the timer: the timestamps of the shim and of the core have to be on one clock */
    if (state->size >= (int)(offsetof(struct t_shim_state, timer) + sizeof(int)) && state->timer != T_SHIM_TIMER) {
        fprintf(stderr, "tractor: %s shim disabled - the core library takes its timestamps with the %s timer, the shim with the %s one: build both alike (CMake PEAT_TIMER, the Makefile uses nano)\n",
                shim, state->timer ? "nano" : "tsc", T_SHIM_TIMER ? "nano" : "tsc");
        return false;
    }
    return true;
}

#if defined(__CPLUSPLUS__) || defined(__cplusplus)
}
#endif
#endif /* __T_SHIM_STATE_H__ */
