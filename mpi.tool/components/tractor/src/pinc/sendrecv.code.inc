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
    int rv, i, comm_rank;
    long long s_count, r_count;
    MPI_Aint s_data_size = 0, r_data_size = 0;
    int level=-1;              /* nesting level of this call, -1 if not traced */
    long long children_base=0; /* records of the next level existing at entry */
    long long tsc_entry,tsc_exit,tsc_mpi_entry,tsc_mpi_exit,local_tool_overhead;
    const PEAT_MPI_Func_ID_Through fidc=$funcid_through;
    pid_t tid_in;
    int tid;
    msl_extras_counters counters[2];
    msl_event* event[2];
    bool l_skip_parse=mpi_skip_parse;

#ifdef _HWCOUNTERS_ENABLED_
    int num_hwcs=gt_info(num_hwcs);
    long long l_hwcs[num_hwcs];
#endif
#ifdef _MEMPROFILE_STORE_
    t_memory_event l_mem;
#endif

    $declare_status
    if(__builtin_expect((counting_permission[$funcid_through]>0 && !l_skip_parse),1)) {
        tsc_entry = peat_get_time_tid(&tid_in);
        $datasize;
        $fix_status
        counting_permit+=4*counting_permission_pcontrol[$funcid_through];
        tid=__msl_get_tid(tid_in);
        level=__msl_enter_call(tid);
        children_base=__msl_child_base(tid,level);
        __msl_allocate_event(tid,level,1,&event[0],&counters[0]);
        __msl_allocate_event(tid,level,1,&event[1],&counters[1]);
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
        stack_unwind$colt(gt_info(max_ret_level),counters[0].addrs,gt_info(ret_levels));

        event[0]->func_id = $funcid_through;
        event[0]->num_children += (short)(__msl_child_base(tid,level) - children_base);
        event[0]->p2p_pr_pb.comm=$comm;
        event[0]->p2p_pr_pb.tag=$stag;
        event[0]->p2p_pr_pb.peer=$dest;
        event[0]->p2p_pr_pb.transfer_bytes=(s_count*s_data_size);
        __msl_uncount_event(tid,level); /* the pair is one call: one child of the enclosing record */
        memcpy(counters[1].addrs,counters[0].addrs,gt_info(num_addrs)*sizeof(void *));
        memcpy((void *)event[1],(const void *) event[0],sizeof(msl_event));
        event[1]->num_children=0; /* the children belong to the first record */
        event[1]->p2p_pr_pb.tag=$recv_tag;
        event[1]->p2p_pr_pb.peer=$recv_peer;
        event[1]->p2p_pr_pb.transfer_bytes=$recv_bytes;
        $event_request
#ifdef _TRACTOR_EXTRA_FIELDS_
        $event_buf
#endif
        $internal_comm_tracking
        event[0]->tscs[0] = tsc_entry;
        event[0]->tscs[1] = tsc_mpi_entry;

#ifdef _HWCOUNTERS_ENABLED_
        memcpy(counters[0].hwcs,l_hwcs,num_hwcs*sizeof(l_hwcs[0]));
        gt_info(hwc).get_all_counters(&counters[0].hwcs[num_hwcs]);
        memcpy(counters[1].hwcs,counters[0].hwcs,gt_info(num_hwcs)*sizeof(long long)*2);
#endif
#ifdef _MEMPROFILE_STORE_
        counters[0].mems[0]=l_mem;
        gt_info(mprof).get_mem_status(&counters[0].mems[1]);
        memcpy(counters[1].mems,counters[0].mems,sizeof(t_memory_event)*2);
#endif
        // counting_permit-=4*counting_permission_pcontrol[$funcid_through];
        tsc_exit = peat_get_time();
        event[0]->tscs[2] = tsc_mpi_exit;
        event[0]->tscs[3] = tsc_exit;
        event[1]->tscs[0] = event[0]->tscs[0];
        event[1]->tscs[1] = event[0]->tscs[1];
        event[1]->tscs[2] = event[0]->tscs[2];
        event[1]->tscs[3] = event[0]->tscs[3];
        // event[1]->tscs[1] = event[1]->tscs[2] = event[1]->tscs[3]  = 0;
        __msl_check_chain(tid,level);
        local_tool_overhead=tsc_mpi_entry-tsc_entry+tsc_exit-tsc_mpi_exit;
        $global_mpi_time
        global_tool_overhead += local_tool_overhead;
#ifdef _TRACTOR_COLLECT_STAT_
        overall_durations[$funcid_through]+=tsc_mpi_exit-tsc_mpi_entry;
        overall_entries[$funcid_through]++;
        overall_tool_overhead[$funcid_through] += local_tool_overhead;
#endif
    } //if(__builtin_expect((counting_permission[$funcid_through]>0 && !l_skip_parse),1))
#endif
    __DEBUG_EPILOG_MPI_LAYER__
    $return;
}

EOF
