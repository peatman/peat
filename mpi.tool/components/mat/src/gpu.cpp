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

GPU::GPU(
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
                  cur_transfer(NULL), cur_exec(NULL), ptp(NULL), comm_store(NULL), verbose(v)
{
}

void GPU::initialize(PTP *p, Communicators *c, File_Storage_GZ *MPI, File_Storage_GZ *HWC, File_Storage_GZ *bind)
{
    assert(c);
    assert(p);
    comm_store = c;
    ptp = p;
    Outputs::initialize(MPI, HWC, bind);
}

GPU::~GPU()
{
    ptp = 0;
    comm_store = 0;
}
void GPU::print_GPU_out(const Tractor_Event &event)
{
#ifdef MAT_PROCESS_HWC_MPI_OUTPUT
    output_MPI->printf(event.rank, "%s:%s", event.header_str().c_str(), global_func_id.name_by_idx(event.func_id));
#if 0
    output_HWC->printf(event.rank, "%s:%lld:%lld:%lld:%lld:%lld:%lld", event.hwc_header_str().c_str(),
        event.tsc_time.func.in,
        event.tsc_time.mpi.in,
        event.tsc_time.mpi.out,
        event.tsc_time.func.out,
        event.tsc_time.pe.in,
        event.tsc_time.pe.out);

#else
    output_HWC->printf(event.rank, "%s:%lld:%lld:%lld:%lld", event.hwc_header_str().c_str(),
                       event.tsc_time.mpi.in,
                       event.tsc_time.mpi.out,
                       event.tsc_time.pe.in,
                       event.tsc_time.pe.out);
#endif
    for (PEAT_Num i = 0; i < event.addrs.size(); i++) {
        output_HWC->printf(event.rank, ":%lld", event.addrs.at(i));
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
#endif
    print_GPU_bind(event);
}

void GPU::print_GPU_bind(const Tractor_Event &event)
{
    if (cur_transfer) {
        output_bind->printf(event.rank, "%d:%s:%s:%s:%s\n", PEAT_RT_Bind_Completion,
                            global_func_id.name_by_idx(cur_transfer->init.func_id),
                            peat_ids_string(cur_transfer->init.ids).c_str(),
                            global_func_id.name_by_idx(cur_transfer->completion.func_id),
                            peat_ids_string(cur_transfer->completion.ids).c_str());
        PEAT_Handle handle = cur_transfer->handle;
        transfers[event.rank].erase(handle);
        cur_transfer = NULL;
    }
    if (cur_exec) {
        output_bind->printf(event.rank, "%d:%s:%s:%s:%s\n", PEAT_RT_Bind_Completion,
                            global_func_id.name_by_idx(cur_exec->init.func_id),
                            peat_ids_string(cur_exec->init.ids).c_str(),
                            global_func_id.name_by_idx(cur_exec->completion.func_id),
                            peat_ids_string(cur_exec->completion.ids).c_str());
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

static void add_clock(PEAT_Single_Event_Clock &clocks, PEAT_Clock cur_clock)
{
    clocks.total_c += cur_clock;
    clocks.min_c = cur_clock < clocks.min_c ? cur_clock : clocks.min_c;
    clocks.max_c = cur_clock > clocks.max_c ? cur_clock : clocks.max_c;
    clocks.occ++;
}

void GPU::add_transfer(const Tractor_Event &event, PEAT_Message_Size bytes)
{
    PEAT_GPUT_Record record;
    record.rank = event.rank;
    record.bytes = bytes;
    record.func_id[0] = record.func_id[1] = event.func_id;
    add_clock(printed_gpu_transfers[event.PControl.ctl][record], event.tsc_time.mpi.out - event.tsc_time.mpi.in);
}

void GPU::add_execution(const Tractor_Event &event, PEAT_Handle handle)
{
    PEAT_GPUE_Record record;
    record.rank = event.rank;
    record.handle = handle;
    record.func_id[0] = record.func_id[1] = event.func_id;
    add_clock(printed_gpu_executions[event.PControl.ctl][record], event.tsc_time.mpi.out - event.tsc_time.mpi.in);
}

// OpenCL commands: an enqueue call returns at once, the work is finished when the event it returned (or an event that
// waits for it) is waited for, or the queue is finished. A blocking call is finished by itself.
void GPU::enqueue_cl(const PEAT_Rank rank, const Tractor_Event &event, bool is_transfer, PEAT_Message_Size bytes, PEAT_Handle kernel, bool blocking)
{
    CL_Command cmd;
    cmd.queue = event.params.empty() ? 0 : event.params.at(0);
    cmd.event = event.out_params.empty() ? 0 : event.out_params.at(0);
    cmd.kernel = kernel;
    cmd.bytes = bytes;
    cmd.is_transfer = is_transfer;
    cmd.waits.assign(event.extra_params.begin(), event.extra_params.end());
    cmd.init.time = event.tsc_time;
    cmd.init.func_id = event.func_id;
    cmd.init.ids = event.ids;
    cmd.init.thread_id = event.thread_id;
    cmd.pc_ctl = event.PControl.ctl;
    std::list<CL_Command> &pending = cl_pending[rank];
    if (cmd.event != 0) { // a handle can be reused once the event is released: the older command was never waited for
        for (std::list<CL_Command>::iterator it = pending.begin(); it != pending.end(); ++it) {
            if (it->event == cmd.event) {
                finish_cl(rank, *it, it->init, NULL);
                pending.erase(it);
                break;
            }
        }
    }
    if (blocking) {
        finish_cl(rank, cmd, cmd.init, NULL);
        return;
    }
    pending.push_back(cmd);
}

// completion: the record whose time and ids end the command; bind it unless the command completed by itself
void GPU::finish_cl(const PEAT_Rank rank, const CL_Command &cmd, const PEAT_Reduced_Event_Info &completion, const Tractor_Event *by)
{
    const PEAT_Clock cur_clock = completion.time.mpi.out - cmd.init.time.mpi.in;
    if (cmd.is_transfer) {
        PEAT_GPUT_Record record;
        record.rank = rank;
        record.bytes = cmd.bytes;
        record.func_id[0] = cmd.init.func_id;
        record.func_id[1] = completion.func_id;
        add_clock(printed_gpu_transfers[by ? by->PControl.ctl : cmd.pc_ctl][record], cur_clock);
    } else {
        PEAT_GPUE_Record record;
        record.rank = rank;
        record.handle = cmd.kernel;
        record.func_id[0] = cmd.init.func_id;
        record.func_id[1] = completion.func_id;
        add_clock(printed_gpu_executions[by ? by->PControl.ctl : cmd.pc_ctl][record], cur_clock);
    }
    if (by) {
        output_bind->printf(rank, "%d:%s:%s:%s:%s\n", PEAT_RT_Bind_Completion,
                            global_func_id.name_by_idx(cmd.init.func_id),
                            peat_ids_string(cmd.init.ids).c_str(),
                            global_func_id.name_by_idx(completion.func_id),
                            peat_ids_string(completion.ids).c_str());
    }
}

// the command that returned this event is finished, and so is everything it waited for
void GPU::complete_cl_event(const PEAT_Rank rank, const Tractor_Event &event, PEAT_Handle handle)
{
    std::list<CL_Command> &pending = cl_pending[rank];
    std::vector<PEAT_Handle> todo(1, handle);
    PEAT_Reduced_Event_Info completion;
    completion.time = event.tsc_time;
    completion.func_id = event.func_id;
    completion.ids = event.ids;
    completion.thread_id = event.thread_id;
    while (!todo.empty()) {
        const PEAT_Handle h = todo.back();
        todo.pop_back();
        if (h == 0)
            continue;
        for (std::list<CL_Command>::iterator it = pending.begin(); it != pending.end(); ++it) {
            if (it->event == h) {
                todo.insert(todo.end(), it->waits.begin(), it->waits.end());
                finish_cl(rank, *it, completion, &event);
                pending.erase(it);
                break;
            }
        }
    }
}

// clFinish: everything enqueued so far on the queue (and what that waited for) is finished
void GPU::complete_cl_queue(const PEAT_Rank rank, const Tractor_Event &event, PEAT_Handle queue)
{
    std::list<CL_Command> &pending = cl_pending[rank];
    PEAT_Reduced_Event_Info completion;
    completion.time = event.tsc_time;
    completion.func_id = event.func_id;
    completion.ids = event.ids;
    completion.thread_id = event.thread_id;
    std::vector<PEAT_Handle> deps;
    for (std::list<CL_Command>::iterator it = pending.begin(); it != pending.end();) {
        if (it->queue == queue) {
            deps.insert(deps.end(), it->waits.begin(), it->waits.end());
            finish_cl(rank, *it, completion, &event);
            it = pending.erase(it);
        } else {
            ++it;
        }
    }
    for (size_t d = 0; d < deps.size(); d++)
        complete_cl_event(rank, event, deps[d]);
}

// commands nobody waited for: counted by the enqueue call alone, as the calls with no completion information are
void GPU::flush_pending()
{
    for (std::map<PEAT_Rank, std::list<CL_Command>>::iterator rank_it = cl_pending.begin(); rank_it != cl_pending.end(); ++rank_it) {
        for (std::list<CL_Command>::iterator it = rank_it->second.begin(); it != rank_it->second.end(); ++it)
            finish_cl(rank_it->first, *it, it->init, NULL);
        rank_it->second.clear();
    }
}

// OpenACC and OpenCL events are complete in themselves: the OpenACC records are start/end pairs merged by tractor, the
// OpenCL ones are the enqueue calls (so for non-blocking calls the time is the submission, as in tractor's statistics).
// OpenACC parameters: 0 device type, 1 parent construct, 2 async queue, 3 bytes, 4 host pointer, 5 device pointer,
// 6 gangs*workers*vector length. OpenCL parameters are the call arguments in order.
enum { ACC_PARAM_ASYNC = 2, ACC_PARAM_BYTES = 3, ACC_PARAMS_NUM = 7 };

void GPU::parse_GPU(const PEAT_Rank rank, const Tractor_Event &event)
{
    const size_t np = event.params.size();
    switch (event.func_id) {
    case ACC_ENQUEUE_UPLOAD_ID:
    case ACC_ENQUEUE_DOWNLOAD_ID:
        if (np >= ACC_PARAMS_NUM)
            add_transfer(event, event.params.at(ACC_PARAM_BYTES));
        break;
    case ACC_ENQUEUE_LAUNCH_ID:
        if (np >= ACC_PARAMS_NUM)
            add_execution(event, event.params.at(ACC_PARAM_ASYNC)); // there is no kernel handle, executions are told apart by queue
        break;
    case CLENQUEUEWRITEBUFFER_ID: // (queue, buffer, blocking, offset, size, ptr, ...)
    case CLENQUEUEREADBUFFER_ID:
        if (np > 4)
            enqueue_cl(rank, event, true, event.params.at(4), 0, event.params.at(2) != 0);
        break;
    case CLENQUEUESVMMEMCPY_ID: // (queue, blocking, dst, src, size, ...)
        if (np > 4)
            enqueue_cl(rank, event, true, event.params.at(4), 0, event.params.at(1) != 0);
        break;
    case CLENQUEUECOPYBUFFER_ID: // (queue, src, dst, src_offset, dst_offset, size, ...)
        if (np > 5)
            enqueue_cl(rank, event, true, event.params.at(5), 0, false);
        break;
    case CLENQUEUENDRANGEKERNEL_ID: // (queue, kernel, ...)
    case CLENQUEUETASK_ID:
    case CLENQUEUENATIVEKERNEL_ID: // (queue, user_func, ...): the host function plays the kernel
        if (np > 1)
            enqueue_cl(rank, event, false, 0, event.params.at(1), false);
        break;
    case CLWAITFOREVENTS_ID: // the event list is the extra parameters
        for (size_t e = 0; e < event.extra_params.size(); e++)
            complete_cl_event(rank, event, event.extra_params.at(e));
        break;
    case CLFINISH_ID: // (queue)
        if (np > 0)
            complete_cl_queue(rank, event, event.params.at(0));
        break;
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
        transfer.init.ids = event.ids;
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
            transfer->completion.ids = event.ids;
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
            exec->completion.ids = event.ids;
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
        exec.init.ids = event.ids;
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

PEAT_Bool GPU::check_completion()
{
    return true;
}

PEAT_Bool GPU::find_handle(PEAT_Rank rank, PEAT_MPI_Request request)
{
    return false;
}
PEAT_Bool GPU::update_handle(PEAT_Rank rank, PEAT_MPI_Request request, const Phase &phase)
{
    return false;
}
