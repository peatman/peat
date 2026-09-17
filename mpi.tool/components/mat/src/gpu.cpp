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

#include "gpu.h"
#include "communicators.h"
#include "utils.h"
#include <assert.h>
#include <sstream>

GPU::GPU(PTP *p, Communicators *c,
         /*
         #ifdef PEAT_MPI
             const std::vector<PEAT_Rank>& wR, const std::vector<PEAT_Rank>& mRs, const MPI_Comm mat_cc, const PEAT_Num mat_ccs, const PEAT_Rank m_r,
         #endif
         */
         int v) : /*
                  #ifdef PEAT_MPI
                      whole_ranks(wR), my_ranks(mRs), mat_compute_comm(mat_cc), mat_compute_comm_size(mat_ccs), my_rank(m_r),
                  #endif
                  */
                  cur_transfer(NULL), cur_exec(NULL), ptp(p), com_store(c), verbose(v)
{
    assert(c);
    assert(p);
    // com_store = c;
    // ptp = p;
}

GPU::~GPU()
{
    ptp = 0;
    com_store = 0;
}
void GPU::print_GPU_out(const Tractor_Event &event)
{
    // cerr << "Local. Rank:" << local_record.rank << ". uniq_id:" << local_record.ph[0].uniq_id << endl;
    output_MPI->printf(event.rank, "%lld:%lld:%d:%s", event.primary_id, event.secondary_id, event.thread_id, global_func_id.name_by_idx(event.func_id));
#if 0
    output_HWC->printf(event.rank, "%lld:%lld:%d:%lld:%lld:%lld:%lld:%lld:%lld", event.primary_id, event.secondary_id, event.thread_id,
        event.tsc_time.func.in,
        event.tsc_time.mpi.in,
        event.tsc_time.mpi.out,
        event.tsc_time.func.out,
        event.tsc_time.pe.in,
        event.tsc_time.pe.out);

#else
    output_HWC->printf(event.rank, "%lld:%lld:%d:%lld:%lld:%lld:%lld", event.primary_id, event.secondary_id, event.thread_id,
                       event.tsc_time.mpi.in,
                       event.tsc_time.mpi.out,
                       event.tsc_time.pe.in,
                       event.tsc_time.pe.out);
#endif
    for (PEAT_Num i = 0; i < event.addrs.size(); i++) {
        output_HWC->printf(event.rank, ":%llx", event.addrs.at(i));
    }
    output_HWC->printf(event.rank, "\n");
    size_t num_params = event.params.size();
    size_t num_out_params = event.out_params.size();
    output_MPI->printf(event.rank, ":%zu", num_params);
    output_MPI->printf(event.rank, ":%zu", num_out_params);
    for (size_t p = 0; p < num_params; p++) {
        output_MPI->printf(event.rank, ":%llx", event.params[p]);
    }
    for (size_t p = 0; p < num_out_params; p++) {
        output_MPI->printf(event.rank, ":%llx", event.out_params[p]);
    }
    output_MPI->printf(event.rank, "\n");
    print_GPU_bind(event);
}

void GPU::print_GPU_bind(const Tractor_Event &event)
{
    if (cur_transfer) {
        output_bind->printf(event.rank, "%d:%s:%lld:%lld:%s:%lld:%lld\n", PEAT_RT_Bind_Completion,
                            global_func_id.name_by_idx(cur_transfer->init.func_id),
                            cur_transfer->init.primary_id,
                            cur_transfer->init.secondary_id,
                            global_func_id.name_by_idx(cur_transfer->completion.func_id),
                            cur_transfer->completion.primary_id,
                            cur_transfer->completion.secondary_id);
        PEAT_Handle handle = cur_transfer->handle;
        transfers[event.rank].erase(handle);
        cur_transfer = NULL;
    }
    if (cur_exec) {
        output_bind->printf(event.rank, "%d:%s:%lld:%lld:%s:%lld:%lld\n", PEAT_RT_Bind_Completion,
                            global_func_id.name_by_idx(cur_exec->init.func_id),
                            cur_exec->init.primary_id,
                            cur_exec->init.secondary_id,
                            global_func_id.name_by_idx(cur_exec->completion.func_id),
                            cur_exec->completion.primary_id,
                            cur_exec->completion.secondary_id);
        PEAT_Handle handle = cur_exec->handle;
        executions[event.rank].erase(handle);
        cur_exec = NULL;
    }
}

void GPU::create_memory_region(Memory_Region &region, PEAT_Rank rank, Where_Located loc, PEAT_Handle handle, PEAT_Message_Size size, PEAT_Addr addr)
{
    region.loc = loc;
    region.handle = handle;
    region.size = size;
    memory_regions[rank][addr] = region;
}

void GPU::parse_GPU(const PEAT_Rank rank, const Tractor_Event &event)
{
    switch (event.func_id) {
    case ZECONTEXTDESTROY_ID:
        GPU_contexts[rank][event.params.at(0)] = false;
        break;
    case ZECONTEXTCREATEEX_ID:
    case ZECONTEXTCREATE_ID:
        GPU_contexts[rank][event.out_params.at(0)] = true;
        break;
    case ZEMEMALLOCHOST_ID: {
        Memory_Region region;
        create_memory_region(region, rank, WL_HOST, event.params.at(0), event.params.at(3), (PEAT_Addr)event.out_params.at(0));
        assert(GPU_contexts[rank][event.params.at(0)]);
    } break;
    case ZEMEMALLOCDEVICE_ID: {
        Memory_Region region;
        create_memory_region(region, rank, WL_DEVICE, event.params.at(0), event.params.at(3), (PEAT_Addr)event.out_params.at(0));
        assert(GPU_contexts[rank][event.params.at(0)]);
    } break;
    case ZEMEMALLOCSHARED_ID: {
        Memory_Region region;
        create_memory_region(region, rank, WL_SHARED, event.params.at(0), event.params.at(3), (PEAT_Addr)event.out_params.at(0));
        assert(GPU_contexts[rank][event.params.at(0)]);
    } break;
    case ZEMEMFREE_ID:
        memory_regions[rank].erase((PEAT_Addr)event.params.at(1));
        break;
    case ZECOMMANDLISTAPPENDMEMORYCOPY_ID: {
        PEAT_Handle handle = (PEAT_Handle)event.params.at(4);
        PEAT_Addr dstptr = (PEAT_Addr)event.params.at(1);
        PEAT_Addr srcptr = (PEAT_Addr)event.params.at(2);
        PEAT_Message_Size size = event.params.at(3);
        Memory_Transfer transfer;
        if (memory_regions[rank].count(srcptr) < 1) {
            Memory_Region region;
            create_memory_region(region, rank, WL_HOST, handle, size, srcptr);
        }
        if (memory_regions[rank].count(dstptr) < 1) {
            Memory_Region region;
            create_memory_region(region, rank, WL_HOST, handle, size, dstptr);
        }
        transfer.from = memory_regions[rank].at(srcptr);
        transfer.to = memory_regions[rank].at(dstptr);
        transfer.size = size;
        transfer.init.time = event.tsc_time;
        transfer.init.func_id = event.func_id;
        transfer.init.primary_id = event.primary_id;
        transfer.init.secondary_id = event.secondary_id;
        transfer.init.thread_id = event.thread_id;
        transfer.handle = handle;
        transfers[rank][handle] = transfer;
    } break;
    case ZEEVENTHOSTRESET_ID:
    case ZEEVENTHOSTSYNCHRONIZE_ID: {
        if (transfers[rank].count(event.params.at(0)) > 0) {
            Memory_Transfer *transfer = &transfers[rank][event.params.at(0)];
            transfer->completion.func_id = event.func_id;
            transfer->completion.time = event.tsc_time;
            transfer->completion.func_id = event.func_id;
            transfer->completion.primary_id = event.primary_id;
            transfer->completion.secondary_id = event.secondary_id;
            transfer->completion.thread_id = event.thread_id;
            PEAT_GPUT_Record record;
            record.rank = event.rank;
            record.bytes = transfer->size;
            record.func_id[0] = transfer->init.func_id;
            record.func_id[1] = transfer->completion.func_id;
            PEAT_Single_Event_Clock &clocks = printed_gpu_transfers[event.PControl.ctl][record];
            PEAT_Clock cur_clock = transfer->completion.time.mpi.out - transfer->init.time.mpi.in;
            clocks.total_c += cur_clock;
            clocks.min_c = cur_clock < clocks.min_c ? cur_clock : clocks.min_c;
            clocks.max_c = cur_clock > clocks.max_c ? cur_clock : clocks.max_c;
            clocks.occ++;
            cur_transfer = transfer;
        }
        if (executions[rank].count(event.params.at(0)) > 0) {
            Region_Exec *exec = &executions[rank][event.params.at(0)];
            exec->completion.func_id = event.func_id;
            exec->completion.time = event.tsc_time;
            exec->completion.func_id = event.func_id;
            exec->completion.primary_id = event.primary_id;
            exec->completion.secondary_id = event.secondary_id;
            exec->completion.thread_id = event.thread_id;
            PEAT_GPUE_Record record;
            record.rank = event.rank;
            record.handle = event.params.at(0);
            record.func_id[0] = exec->init.func_id;
            record.func_id[1] = exec->completion.func_id;
            PEAT_Single_Event_Clock &clocks = printed_gpu_executions[event.PControl.ctl][record];
            PEAT_Clock cur_clock = exec->completion.time.mpi.out - exec->init.time.mpi.in;
            clocks.total_c += cur_clock;
            clocks.min_c = cur_clock < clocks.min_c ? cur_clock : clocks.min_c;
            clocks.max_c = cur_clock > clocks.max_c ? cur_clock : clocks.max_c;
            clocks.occ++;
            cur_exec = exec;
        }
    } break;
    case ZECOMMANDLISTAPPENDMEMORYFILL_ID:
        break;
    case ZECOMMANDQUEUECREATE_ID:
        break;
    case ZECOMMANDQUEUEDESTROY_ID:
        break;
    case ZECOMMANDQUEUEEXECUTECOMMANDLISTS_ID:
        break;
    case ZECOMMANDQUEUESYNCHRONIZE_ID:
        break;
    case ZECOMMANDLISTCREATE_ID:
        break;
    case ZECOMMANDLISTCREATEIMMEDIATE_ID:
        break;
    case ZECOMMANDLISTDESTROY_ID:
        break;
    case ZECOMMANDLISTCLOSE_ID:
        break;
    case ZECOMMANDLISTRESET_ID:
        break;
    case ZECOMMANDLISTAPPENDLAUNCHCOOPERATIVEKERNEL_ID:
        break;
    case ZECOMMANDLISTAPPENDLAUNCHKERNEL_ID: {
        PEAT_Handle handle = (PEAT_Handle)event.params.at(3);
        Region_Exec &exec = executions[rank][handle];
        exec.init.func_id = event.func_id;
        exec.init.time = event.tsc_time;
        exec.init.func_id = event.func_id;
        exec.init.primary_id = event.primary_id;
        exec.init.secondary_id = event.secondary_id;
        exec.init.thread_id = event.thread_id;
        exec.handle = handle;
        executions[rank][handle] = exec;
    } break;
    case ZECONTEXTGETSTATUS_ID:
        break;
    default:
        break;
    }
    print_GPU_out(event);
    return;
}

bool GPU::check_completion()
{
    return true;
}

bool GPU::find_handle(PEAT_Rank rank, PEAT_MPI_Request request)
{
    return false;
}
bool GPU::update_handle(PEAT_Rank rank, PEAT_MPI_Request request, const Phase &phase)
{
    return false;
}
