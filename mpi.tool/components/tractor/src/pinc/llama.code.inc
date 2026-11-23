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
# llama.cpp / ggml pipeline stage wrapper (CLASS_LLAMA, see make_class_llama in mpi_layer.pl).
# Same nesting/timing as gpu_general.code.inc, but no parameters are recorded (plain record), it has its own
# switch ($skip_flag: llama_skip_parse/PEAT_TRACTOR_PARSE_LLAMA, or for the ggml CPU kernels (CLASS_GGML_CPU)
# ggml_cpu_skip_parse/PEAT_TRACTOR_PARSE_GGML_CPU) and its time is not counted as GPU time.
# Calls after the tractor has written its files (llama_free/llama_model_free from static destructors)
# are passed through untraced.

print <<EOF
$t_func_pointer
$prototype {
    __DEBUG_PROLOG_LLAMA_LAYER__
    $declare
    $rtdl
#ifndef _TRACTOR_PASSTHROUGH_
    int level=-1;              /* nesting level of this call, -1 if not traced */
    long long children_base=0; /* records of the next level existing at entry */
    long long tsc_entry,tsc_exit,tsc_mpi_entry,tsc_mpi_exit,local_tool_overhead;
    const PEAT_MPI_Func_ID_Through fidc=$funcid_through;
    pid_t tid_in;
    int tid=0;
    msl_extras_counters counters;
    msl_event *t_event=NULL;
    const bool l_record=(counting_permission[fidc]>0) && !$skip_flag && !tractor_finished;

T_COUNTERS_DECL

    if(__builtin_expect(l_record,1)) {
        tsc_entry = peat_get_time_tid(&tid_in);
        counting_permit+=2*counting_permission_pcontrol[fidc];
        tid=__msl_get_tid(tid_in);
        level=__msl_enter_call(tid);
        children_base=__msl_child_base(tid,level);
        __msl_allocate_event(tid,level,0,&t_event,&counters);
        t_event->gpu.num_arguments=0; /* the cuBLAS records are written in the GPU layout (CUDA group), with no arguments */
        t_event->gpu.num_out_arguments=0;
        t_event->gpu.num_extra_arguments=0;
T_COUNTERS_ENTRY
        tsc_mpi_entry=peat_get_time();
    }
#endif
    $call t_$funcname($args);
#ifndef _TRACTOR_PASSTHROUGH_
    if (level>=0) __msl_leave_call(tid);
    if(__builtin_expect(l_record,1)) {
        tsc_mpi_exit = peat_get_time();
        stack_unwind(gt_info(max_ret_level),counters.addrs,gt_info(ret_levels));
        t_event->func_id = fidc;
        t_event->num_children += (short)(__msl_child_base(tid,level) - children_base);
        t_event->tscs[0] = tsc_entry;
        t_event->tscs[1] = tsc_mpi_entry;
T_COUNTERS_EXIT(counters)
        tsc_exit = peat_get_time();
        t_event->tscs[2] = tsc_mpi_exit;
        t_event->tscs[3] = tsc_exit;
        __msl_check_chain(tid,level);
        local_tool_overhead=tsc_mpi_entry-tsc_entry+tsc_exit-tsc_mpi_exit;
        global_llama_time+=tsc_mpi_exit-tsc_mpi_entry; /* also makes tractor::end() write the trace of a non-MPI process */
        global_tool_overhead += local_tool_overhead;
#ifdef _TRACTOR_COLLECT_STAT_
        overall_durations[fidc]+=tsc_mpi_exit-tsc_mpi_entry;
        overall_entries[fidc]++;
        overall_tool_overhead[fidc] += local_tool_overhead;
#endif
    }
#endif
    __DEBUG_EPILOG_LLAMA_LAYER__
    $return
}

EOF
