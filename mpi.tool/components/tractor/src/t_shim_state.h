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
 * _T_SHIM_STATE_ selects this interface; without it (-D_T_NO_SHIM_STATE_) the shims use struct tractor directly and
 * check its layout (t_transport_layout(), tractor.h), as before 2026-11-23. Core and shims have to be built alike. */
#ifndef __T_SHIM_STATE_H__
#define __T_SHIM_STATE_H__

#ifndef _T_NO_SHIM_STATE_
#define _T_SHIM_STATE_
#endif

#ifdef _T_SHIM_STATE_
#include <stddef.h>
#include <stdio.h>
#include <sys/types.h>

#define T_SHIM_STATE_VERSION 1

struct msl_event;

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
    return true;
}

#if defined(__CPLUSPLUS__) || defined(__cplusplus)
}
#endif
#endif /* _T_SHIM_STATE_ */
#endif /* __T_SHIM_STATE_H__ */
