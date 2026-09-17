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

#ifndef _TRACTOR_PASSTHROUGH_
//    if (__builtin_expect((counting_permission[fidc]>0 && !l_skip_parse),1)) {
tsc_mpi_exit = rdtsca();
in_mpi_call[tid] = 0;
event->func_id[0] = fid;
event->func_id[1] = fidc;
event->comms.comm = lcomm;
event->comms.num_colts = num_colts[tid];
memcpy(counters.addrs, p_entrys, sizeof(void *) * gt_info(num_addrs));
//    } // if (__builtin_expect((counting_permission[fidc]>0 && !l_skip_parse),1))
#ifdef USE_NANO_TIMER
gt_info(local_comms)->comm_end(lcomm, (PEAT_Clock)(((double)nsec_entry) * freq_ratio) + tsc_mpi_exit - tsc_entry);
#else
gt_info(local_comms)->comm_end(lcomm, tsc_mpi_exit);
#endif

#ifdef _HWCOUNTERS_ENABLED_
memcpy(counters.hwcs, l_hwcs, num_hwcs * sizeof(l_hwcs[0]));
gt_info(hwc).get_all_counters(&counters.hwcs[num_hwcs]);
#endif
#ifdef _MEMPROFILE_STORE_
counters.mems[0] = l_mem;
gt_info(mprof).get_mem_status(&counters.mems[1]);
#endif
#ifdef USE_NANO_TIMER
event->tscs[0] = nsec_entry;
#else
event->tscs[0] = tsc_entry;
#endif
event->tscs[1] = tsc_mpi_entry - tsc_entry;
event->tscs[2] = tsc_mpi_exit - tsc_entry;
tsc_exit = rdtsca();
event->tscs[3] = tsc_exit - tsc_entry;
__msl_check_chain(tid);
local_tool_overhead = tsc_mpi_entry - tsc_entry + tsc_exit - tsc_mpi_exit;
global_tool_overhead += local_tool_overhead;
global_mpi_time += tsc_mpi_exit - tsc_mpi_entry;
#ifdef _TRACTOR_COLLECT_STAT_
//    if (__builtin_expect((counting_permission[fidc]>0 && !l_skip_parse),1)) {
overall_durations[fidc] += tsc_mpi_exit - tsc_mpi_entry;
overall_entries[fidc]++;
overall_tool_overhead[fidc] += local_tool_overhead;
//    } // if (__builtin_expect((counting_permission[fidc]>0 && !l_skip_parse),1))
#endif
#endif
