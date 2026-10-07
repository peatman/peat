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

int rv;
copy_comm;
PEAT_Clock tsc_entry, tsc_exit, tsc_mpi_entry, tsc_mpi_exit, local_tool_overhead;
int tid;
int level = -1;
long long children_base = 0;
pid_t tid_in;
msl_extras_counters counters;
msl_event *event;
bool l_skip_parse = mpi_skip_parse;

#ifdef _HWCOUNTERS_ENABLED_
int num_hwcs = gt_info(num_hwcs);
long long l_hwcs[num_hwcs];
#endif
#ifdef _MEMPROFILE_STORE_
t_memory_event l_mem;
#endif

tsc_entry = peat_get_time_tid(&tid_in);
//    if (__builtin_expect((counting_permission[fidc]>0 && !l_skip_parse),1)) {
last_coll_comm = MPI_COMM_WORLD;
counting_permit += 2 * counting_permission_pcontrol[fidc];
tid = __msl_get_tid(tid_in);
level = __msl_enter_call(tid);
children_base = __msl_child_base(tid, level);
__msl_allocate_event(tid, level, 0, &event, &counters);
#ifdef _HWCOUNTERS_ENABLED_
gt_info(hwc).get_all_counters(l_hwcs);
#endif
#ifdef _MEMPROFILE_STORE_
gt_info(mprof).get_mem_status(&l_mem);
#endif
tsc_mpi_entry = peat_get_time();
//    } // if (__builtin_expect((counting_permission[fidc]>0 && !l_skip_parse),1))
