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
    int rv;
#ifndef _TRACTOR_PASSTHROUGH_
    int i;
    int ranks;
    int nranks;
    MPI_Group cwg,group;
    int level=-1;              /* nesting level of this call, -1 if not traced */
    long long children_base=0; /* records of the next level existing at entry */
    long long tsc_entry,tsc_exit,tsc_mpi_entry,tsc_mpi_exit,local_tool_overhead;
    const PEAT_MPI_Func_ID_Through fidc=$funcid_through;
    pid_t tid_in;
    int tid;
    msl_extras_counters counters;
    msl_event *event;
    bool l_skip_parse=mpi_skip_parse;


    if(__builtin_expect((counting_permission[$funcid_through]>0 && !l_skip_parse),1)) {
        tsc_entry = peat_get_time_tid(&tid_in);
        counting_permit+=2*counting_permission_pcontrol[$funcid_through];
        tid=__msl_get_tid(tid_in);
        level=__msl_enter_call(tid);
        children_base=__msl_child_base(tid,level);
        __msl_allocate_event(tid,level,0,&event,&counters);
        tsc_mpi_entry=peat_get_time();
    } //if(__builtin_expect((counting_permission[$funcid_through]>0 && !l_skip_parse),1))
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
        event->$basic_struct.comm=$comm;
        $size
        $event_handle
        event->tscs[0] = tsc_entry;
        event->tscs[1] = tsc_mpi_entry;
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
