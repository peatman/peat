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
    $sbuf_bottom
    $rbuf_bottom
#ifndef _TRACTOR_PASSTHROUGH_
    int rv, i;
    MPI_Aint data_size=0;
    int level=-1;              /* nesting level of this call, -1 if not traced */
    long long children_base=0; /* records of the next level existing at entry */
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
        level=__msl_enter_call(tid);
        children_base=__msl_child_base(tid,level);
        __msl_allocate_event(tid,level,0,&event,&counters);
#ifdef _HWCOUNTERS_ENABLED_
        gt_info(hwc).get_all_counters(l_hwcs);
#endif
#ifdef _MEMPROFILE_STORE_
        gt_info(mprof).get_mem_status(&l_mem);
#endif
        tsc_mpi_entry=peat_get_time();
    } //if(__builtin_expect((counting_permission[$funcid_through]>0 && !l_skip_parse),1))
#else
    int rv;
#endif
#ifdef _TRACTOR_PCALL_
    $Preturn=P$c_call;
#endif
#ifndef _TRACTOR_PASSTHROUGH_
    if (level>=0) __msl_leave_call(tid); /* also when the record was discarded */
    if(__builtin_expect((counting_permission[$funcid_through]>0 && !l_skip_parse),1)) {
        tsc_mpi_exit = peat_get_time();
        stack_unwind(gt_info(max_ret_level),counters.addrs,gt_info(ret_levels));
        event->func_id = $funcid_through;
        event->num_children += (short)(__msl_child_base(tid,level) - children_base);
        $type
        $count
        $size
#ifdef _TRACTOR_EXTRA_FIELDS_
        $event_sbuf
        $event_rbuf
#endif
        event->tscs[0] = tsc_entry;
        event->tscs[1] = tsc_mpi_entry;
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
        event->tscs[2] = tsc_mpi_exit;
        event->tscs[3] = tsc_exit;
        __msl_check_chain(tid,level);
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
