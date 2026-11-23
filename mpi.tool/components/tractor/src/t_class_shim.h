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

/* How the code of an event class (t_ompt.cpp, t_acc.cpp, t_cupti.cpp and the cuBLAS wrappers) reaches the core. The
 * same source is built into the core library or into a shim (Makefile TARGET=class_layers, -D_T_CLASS_SHIM_) preloaded
 * next to a core built without that class; in both cases it uses t_shared only (t_shim_state.h).
 * Include after tractor.h and t_defines.h, inside the extern "C" block of the file. */
#ifndef __T_CLASS_SHIM_H__
#define __T_CLASS_SHIM_H__

#ifdef _T_CLASS_SHIM_
extern struct t_shim_state t_shared __attribute__((weak)); /* NULL without a core library */

#if defined(_HWCOUNTERS_ENABLED_) || defined(_MEMPROFILE_STORE_) || defined(_TRACTOR_DIAG_)
#error "the event-class shims are built without HWCOUNTERS, MEMPROFILE and DIAG: those are switches of the core, the shims fit any of its variants"
#endif
/* In a shim the core is t_shared: gt_info(x) is t_shared.x, so the code written for the core (t_cupti.cpp, the
   generated wrappers) compiles unchanged, and a use of anything of the core that t_shared does not have fails to
   compile instead of reaching into struct tractor. The __msl_* macros of t_defines.h expand to gt_info(...), so they
   follow; the records are allocated without the counters, which are the core's (the shim gets the addresses only). */
#undef gt_info
#define gt_info(x) t_shared.x
#undef __msl_allocate_event
#define __msl_allocate_event(tid_in, level, skip_write, event, counters) \
    t_shared.allocate_event(tid_in, level, event, (void ***)&(counters)->addrs)
/* no counter samples in the records of a shim (t_shared may be NULL here) */
#undef T_COUNTERS_DECL
#undef T_COUNTERS_ENTRY
#undef T_COUNTERS_EXIT
#undef T_COUNTERS_ENTRY_TO
#undef T_COUNTERS_EXIT_TO
#undef T_COUNTERS_COPY
#define T_COUNTERS_DECL
#define T_COUNTERS_ENTRY
#define T_COUNTERS_EXIT(c)
#define T_COUNTERS_ENTRY_TO(c)
#define T_COUNTERS_EXIT_TO(c)
#define T_COUNTERS_COPY(d, s)
#endif

#define T_CORE(x) t_shared.x
#define T_CORE_GET_TID(t) t_shared.get_tid(t)
#define T_CORE_LEVEL(t) t_shared.current_level(t)
/* a new record at the level and where its return addresses go (void **) */
#define T_CORE_ALLOCATE(tid, level, event, addrs_out) t_shared.allocate_event(tid, level, event, addrs_out)
#define T_CORE_WRITE_RECORDS(tid, level, values, num) t_shared.write_records(tid, level, values, num)
/* cls: a static struct t_shim_class. 0 if a class of that name is registered already: another copy of the class
   (built into the core and preloaded as a shim, or two shims) is set up, this one must stay inert, or every event
   would be recorded twice */
#define T_CORE_REGISTER_CLASS(cls) t_shared.register_class(cls)
/* the inline stack unwinding (t_defines.h) reads the bounds from t_shared */
#undef T_UNWIND_INFO
#define T_UNWIND_INFO(x) t_shared.x
/* nothing of the class file is used by the core directly; and static so that a shim never binds to a copy of the same
   function in another loaded library (a core with the class built in, another shim) */
#define T_CLASS_STATIC static
/* the core is loaded and has t_shared up to the last member the class uses; checked once at load, a class records
   nothing otherwise */
#define T_CLASS_CORE_CHECK(name, last_member) t_shim_state_check(&t_shared, T_SHIM_STATE_NEED(last_member), name)

#endif /* __T_CLASS_SHIM_H__ */
