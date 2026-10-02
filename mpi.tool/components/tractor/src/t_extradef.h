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

#ifndef __T_EXTRADEF_H__
#define __T_EXTRADEF_H__
#if defined(__CPLUSPLUS__) || defined(__cplusplus)
extern "C" {
#endif

#include "mpi_func_ids_through.h"

#ifdef _TRACTOR_COLLECT_STAT_
extern long long overall_tool_overhead[MPI_MAX_FUNCID_THROUGH];
extern long long overall_durations[MPI_MAX_FUNCID_THROUGH];
extern long long overall_entries[MPI_MAX_FUNCID_THROUGH];
extern char *mpi_function_names[MPI_MAX_FUNCID_THROUGH];
#endif
extern int mpi_func_ids_trans[MPI_MAX_FUNCID_THROUGH]; /* through id -> base id */
extern char counting_permission[MPI_MAX_FUNCID_THROUGH];
extern char counting_permission_pcontrol[MPI_MAX_FUNCID_THROUGH];
extern char fullstack_permission[MPI_MAX_FUNCID_THROUGH];
/* set once tractor::end() starts: libfabric's own ELF-level destructors (e.g. vrb_fini) can still
   call our fi_* wrappers after tractor's own global object has already been torn down */
extern bool tractor_finished;

#if defined(__CPLUSPLUS__) || defined(__cplusplus)
}
#endif

#endif //__T_EXTRADEF_H__
