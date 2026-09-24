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
$prototype {
    __DEBUG_PROLOG_MPI_LAYER__
#ifndef _TRACTOR_PASSTHROUGH_
    int rv, i,temp_index=0;
    long long tsc_entry,tsc_exit,tsc_mpi_entry,tsc_mpi_exit,local_tool_overhead;
    const PEAT_MPI_Func_ID_Through fidc=$funcid_through;
    pid_t tid_in;
    int tid;
    msl_extras_counters counters;
    msl_event *event;
    bool l_skip_parse=mpi_skip_parse;

#ifdef _HWCOUNTERS_ENABLED_
    int num_hwcs=gt_info(num_hwcs);
    long long l_hwcs[num_hwcs];
#endif
#ifdef _MEMPROFILE_STORE_
    t_memory_event l_mem;
#endif

    if(__builtin_expect((counting_permission[$funcid_through]>0 && !l_skip_parse),1)) {
        tsc_entry = peat_get_time_tid(&tid_in);
        counting_permit+=2*counting_permission_pcontrol[$funcid_through];
        tid=__msl_get_tid(tid_in);
        first_progress_entry[tid]=last_progress_exit[tid]=0;
#ifdef _HWCOUNTERS_ENABLED_
        gt_info(hwc).get_all_counters(l_hwcs);
#endif
#ifdef _MEMPROFILE_STORE_
        gt_info(mprof).get_mem_status(&l_mem);
#endif
        in_mpi_call[tid]=1;
        tsc_mpi_entry=peat_get_time();
    } //if(__builtin_expect((counting_permission[$funcid_through]>0 && !l_skip_parse),1))
#else
    int rv;
#endif
#ifdef _TRACTOR_PCALL_
    $Preturn=P$c_call;
#endif
#ifndef _TRACTOR_PASSTHROUGH_
    if(__builtin_expect((counting_permission[$funcid_through]>0 && !l_skip_parse),1)) {
        tsc_mpi_exit = peat_get_time();
        in_mpi_call[tid]=0;
        __msl_allocate_event(tid,0,&event,&counters);
        stack_unwind(gt_info(max_ret_level),counters.addrs,gt_info(ret_levels));
        event->func_id[0] = $funcid;
        event->func_id[1] = $funcid_through;
        event->tscs[0]=tsc_entry;
        event->tscs[1]=tsc_mpi_entry-tsc_entry;
        event->tscs[2]=tsc_mpi_exit-tsc_entry;
#ifdef _HWCOUNTERS_ENABLED_
        memcpy(counters.hwcs,l_hwcs,num_hwcs*sizeof(l_hwcs[0]));
        gt_info(hwc).get_all_counters(&counters.hwcs[num_hwcs]);
#endif
#ifdef _MEMPROFILE_STORE_
        counters.mems[0]=l_mem;
        gt_info(mprof).get_mem_status(&counters.mems[1]);
#endif
        // counting_permit-=2*counting_permission_pcontrol[$funcid_through];
        tsc_exit = peat_get_time();
        event->tscs[3]=tsc_exit-tsc_entry;
        __msl_check_chain(tid);
        local_tool_overhead=tsc_mpi_entry-tsc_entry+tsc_exit-tsc_mpi_exit;
        global_mpi_time+=tsc_mpi_exit-tsc_mpi_entry;
        global_tool_overhead += local_tool_overhead;
#ifdef _TRACTOR_COLLECT_STAT_
        overall_durations[$funcid_through]+=tsc_mpi_exit-tsc_mpi_entry;
        overall_entries[$funcid_through]++;
        overall_tool_overhead[$funcid_through] += local_tool_overhead;
#endif
    } //if(__builtin_expect((counting_permission[$funcid_through]>0 && !l_skip_parse),1))
#endif
    __DEBUG_EPILOG_MPI_LAYER__
    $return
}

EOF
