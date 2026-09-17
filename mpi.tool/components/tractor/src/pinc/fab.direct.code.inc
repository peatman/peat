#
# This file is part of PEAT (Performance Estimation & Analysing Toolkit).
# Copyright (C) 2026 by Alex Knyazev
#
# PEAT is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# PEAT is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with PEAT.  If not, see <https://www.gnu.org/licenses/>.
#

print <<EOF
$t_func_pointer
$prototype {
    __DEBUG_PROLOG_FAB_LAYER__
#ifndef _TRACTOR_PASSTHROUGH_
    $declare
    int i,temp_index=0;
    long long tsc_entry,tsc_exit,tsc_mpi_entry,tsc_mpi_exit,local_tool_overhead;
    const PEAT_MPI_Func_ID_Through fidc=$funcid_through;
#ifdef USE_NANO_TIMER
    long long nsec_entry;
#endif
    pid_t tid_in;
    int tid;
    msl_event *f_event;
    bool l_skip_parse=(gt_info(t_permit_collect_fab)==0);
    $rtdl // if necessary initialize pointer
    // init_fab_hooks();
    if(__builtin_expect((counting_permission[$funcid_through]>0 && !l_skip_parse),1)) {
#ifdef USE_NANO_TIMER
        nsec_entry=peat_get_nano_sec();
#endif
        tsc_entry = rdtscta(&tid_in);
        tid=__msl_get_tid(tid_in);
        __msl_fab_allocate_event(tid,&f_event,$funcid_through);
        tsc_mpi_entry=rdtsca();
    } // if(__builtin_expect((counting_permission[$funcid_through]>0 && !l_skip_parse),1))
#else
    $declare
#endif
#ifdef _TRACTOR_PCALL_
    ${call}$funcname($funcpars);
#endif
#ifndef _TRACTOR_PASSTHROUGH_
    if(__builtin_expect((counting_permission[$funcid_through]>0 && !l_skip_parse),1)) {
        tsc_mpi_exit = rdtsca();
        f_event->func_id[0] = $funcid;
        f_event->func_id[1] = $funcid_through;
#ifdef USE_NANO_TIMER
        f_event->tscs[0]=nsec_entry;
#else
        f_event->tscs[0]=tsc_entry;
#endif
        f_event->tscs[1]=tsc_mpi_entry-tsc_entry;
        f_event->fab.num_calls=1;
        f_event->fab.total_clocks_bitween_calls=0;
        tsc_exit = rdtsca();
        f_event->tscs[2]=tsc_exit - tsc_mpi_exit;
        f_event->tscs[3]=tsc_exit - tsc_entry;
        f_event->fab.num_calls=1;
        f_event->fab.total_clocks_bitween_calls=0;
        local_tool_overhead=tsc_mpi_entry-tsc_entry+tsc_exit-tsc_mpi_exit;
        global_tool_overhead += local_tool_overhead;
#ifdef _TRACTOR_COLLECT_STAT_
        overall_durations[$funcid_through]+=tsc_mpi_exit-tsc_mpi_entry;
        overall_entries[$funcid_through]++;
        overall_tool_overhead[$funcid_through] += local_tool_overhead;
#endif
    } // if(__builtin_expect((counting_permission[$funcid_through]>0 && !l_skip_parse),1))
#endif
    __DEBUG_EPILOG_FAB_LAYER__
    $return
}

EOF
