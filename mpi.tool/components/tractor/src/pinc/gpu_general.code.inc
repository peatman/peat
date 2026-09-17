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
    __DEBUG_PROLOG_GPU_LAYER__
#ifndef _TRACTOR_PASSTHROUGH_
    int i,temp_index=0;
    $declare
    long long tsc_entry,tsc_exit,tsc_mpi_entry,tsc_mpi_exit,local_tool_overhead;
    const PEAT_MPI_Func_ID_Through fidc=$funcid_through;
#ifdef USE_NANO_TIMER
    long long nsec_entry;
#endif
    pid_t tid_in;
    int tid;
    msl_extras_counters counters;
    msl_event *t_event;
    bool l_skip_parse=skip_parse;

#ifdef _HWCOUNTERS_ENABLED_
    int num_hwcs=gt_info(num_hwcs);
    long long l_hwcs[num_hwcs];
#endif
#ifdef _MEMPROFILE_STORE_
    t_memory_event l_mem;
#endif

    $rtdl // if necessary initialize pointer
    if(__builtin_expect((counting_permission[$funcid_through]>0 && !l_skip_parse),1)) {
#ifdef USE_NANO_TIMER
        nsec_entry=peat_get_nano_sec();
#endif
        tsc_entry = rdtscta(&tid_in);
        counting_permit+=2*counting_permission_pcontrol[$funcid_through];
        tid=__msl_get_tid(tid_in);
        first_progress_entry[tid]=last_progress_exit[tid]=0;
#ifdef _HWCOUNTERS_ENABLED_
        gt_info(hwc).get_all_counters(l_hwcs);
#endif
#ifdef _MEMPROFILE_STORE_
        gt_info(mprof).get_mem_status(&l_mem);
#endif
        t_event->gpu.num_arguments=$num_params;
        t_event->gpu.num_out_arguments=$num_out_params;
        t_event->gpu.num_extra_arguments=$num_extra_params;
        $record_params
        $record_extra_params
        tsc_mpi_entry=rdtsca();
    } //if(__builtin_expect((counting_permission[$funcid_through]>0 && !l_skip_parse),1))
#else
    int rv;
#endif
#ifdef _TRACTOR_PCALL_
    $call t_$c_call;
#endif
#ifndef _TRACTOR_PASSTHROUGH_
    if(__builtin_expect((counting_permission[$funcid_through]>0 && !l_skip_parse),1)) {
        tsc_mpi_exit = rdtsca();
        __msl_allocate_event(tid,0,&t_event,&counters);
        $record_out_params
        stack_unwind(gt_info(max_ret_level),counters.addrs,gt_info(ret_levels));
        t_event->func_id[0] = $funcid;
        t_event->func_id[1] = $funcid_through;
#ifdef USE_NANO_TIMER
        t_event->tscs[0]=nsec_entry;
#else
        t_event->tscs[0]=tsc_entry;
#endif
        t_event->tscs[1]=tsc_mpi_entry-tsc_entry;
        t_event->tscs[2]=tsc_mpi_exit-tsc_entry;
#ifdef _HWCOUNTERS_ENABLED_
        memcpy(counters.hwcs,l_hwcs,num_hwcs*sizeof(l_hwcs[0]));
        gt_info(hwc).get_all_counters(&counters.hwcs[num_hwcs]);
#endif
#ifdef _MEMPROFILE_STORE_
        counters.mems[0]=l_mem;
        gt_info(mprof).get_mem_status(&counters.mems[1]);
#endif
        // counting_permit-=2*counting_permission_pcontrol[$funcid_through];
        tsc_exit = rdtsca();
        t_event->tscs[3]=tsc_exit-tsc_entry;
        __msl_check_chain(tid);
        local_tool_overhead=tsc_mpi_entry-tsc_entry+tsc_exit-tsc_mpi_exit;
        global_gpu_time+=tsc_mpi_exit-tsc_mpi_entry;
        global_tool_overhead += local_tool_overhead;
#ifdef _TRACTOR_COLLECT_STAT_
        overall_durations[$funcid_through]+=tsc_mpi_exit-tsc_mpi_entry;
        overall_entries[$funcid_through]++;
        overall_tool_overhead[$funcid_through] += local_tool_overhead;
#endif
    } //if(__builtin_expect((counting_permission[$funcid_through]>0 && !l_skip_parse),1))
#endif
    __DEBUG_EPILOG_GPU_LAYER__
    $return
}

EOF
