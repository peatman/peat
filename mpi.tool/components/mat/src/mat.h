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

#ifndef __MAT_H__
#define __MAT_H__

#include "ptp.h"

#include "collectives.h"
#include "comm.h"
#include "osc.h"
#include "communicators.h"
#include "gpu.h"
#include "model_id.h"
#include "p2pal.h"
#include "requests.h"
#include <memory>
#include <vector>

/* mat.omp (PEAT_OMP) splits the trace ranks over its threads (rank % threads), each walking its own ranks in time
   order. They share the matching structures: everything from the dispatch of a parsed event into the stores on runs
   under match_lock, while reading, parsing and time conversion of events, and the compression of the output files
   (deferred writing, see File_Storage_GZ::set_deferred), run in parallel. */
#if defined(PEAT_OMP) && defined(PEAT_MPI)
#error "pmat with OpenMP (hybrid MPI+OpenMP mat) is not supported yet"
#endif
#ifdef PEAT_OMP
#include <atomic>
#include <mutex>
#include <thread>
/* The lock every event of every thread passes through, held only briefly each time: a sleeping mutex would make
   each hand-over wait for a futex wake-up. Recursive: a send-receive record parses its second line under it. */
class Mat_Match_Lock
{
    std::atomic<int> owner{-1};
    int depth = 0;

  public:
    void lock()
    {
        const int me = omp_get_thread_num();
        if (owner.load(std::memory_order_relaxed) == me) {
            depth++;
            return;
        }
        for (unsigned spins = 0;; spins++) {
            int free_lock = -1;
            if (owner.load(std::memory_order_relaxed) == -1 && owner.compare_exchange_weak(free_lock, me, std::memory_order_acquire))
                break;
            if (spins > 1000)
                std::this_thread::yield();
        }
        depth = 1;
    }
    void unlock()
    {
        if (--depth == 0)
            owner.store(-1, std::memory_order_release);
    }
};
#endif

/* State of one walk over a set of trace ranks: the whole trace in mat, a subset in each mat.omp thread */
struct Mat_Worker {
    std::vector<PEAT_Rank> ranks;
    std::map<PEAT_Clock, PEAT_Rank> timeline; // next event of each rank, by time
    String_Split line_mpi, line_hwc;
    PEAT_Bool second_line = false; // between the two lines of a send-receive record
    Tractor_Event send_line, recv_line;
};

struct Mat_Time {
    PEAT_Clock durations, occurences;
    Mat_Time() : durations(0), occurences(0) {}
};
class Mat
{
  public:
    std::vector<PEAT_Rank> whole_ranks;
    std::vector<PEAT_Rank> my_ranks;
    Communicators *comm_store;
    PTP *ptp;
    Collectives *coll_func_store;
    P2PAL *p2p_func_store;
    GPU *gpu_func_store;
    Comm* comm_func_store;
    OSC* osc_func_store;
    Requests *request_func_store;
    Request_Store* req_store;
    File_Storage_GZ *input_MPI;
    File_Storage_GZ *input_HWC;
    File_Storage_GZ *output_MPI;
    File_Storage_GZ *output_HWC;
    File_Storage_GZ *output_bind;
    std::vector<PEAT_LNum> rank_events_processed;
    PEAT_LNum events_processed;

    // File_Storage_GZ *output_model;
    // File_Storage_GZ *output_model_message;
    Address_File_Storage *user_region_store;
    const Global_Parameters globals;
    const Model_ID &model_id;
    std::vector<PEAT_Clock> tsc_corrector, tsc_predictor;
    std::map<int, int> p_control_in_use;
    std::map<int, int> pc_levels;
    PEAT_Rank get_soonest_event(Mat_Worker &w, PEAT_Clock &tsc);
    std::map<PEAT_Num, std::map<PEAT_Property, Mat_Time>> timings;
    Mat(Global_Parameters &file_names, Model_ID &model_id_storage);
    ~Mat();
    Mat(const Mat &) = delete;
    Mat &operator=(const Mat &) = delete;
    void write_trace_info(const std::string &name);
    PEAT_Bool parse_single_event(const int rank);
#ifdef PEAT_MPI
    PEAT_Num handle_incoming_messages();
    std::list<PEAT_Event_Delivery> active_deliveries; // every work message sent to a pmat rank, until its send completes
    PEAT_Num process_outgoing_messages();
    // work messages (events, requests, bind strings) this pmat rank sent / received, for termination detection
    PEAT_LNum deliveries_completed = 0, messages_received = 0;
    PEAT_LNum messages_sent() const { return deliveries_completed + (PEAT_LNum)active_deliveries.size(); }
    void wait_for_global_quiescence();
    void print_remote_string(int size, int tag, int src);
    PEAT_Signed_Num check_unbound_events()
    {
        PEAT_Signed_Num p2p_ue = p2p_func_store->check_unbound_events();
        PEAT_Signed_Num coll_ue = coll_func_store->check_unbound_events();
        return p2p_ue + coll_ue;
    }
#endif
    void process();
    void process_unbound_events(PEAT_Rank my_rank)
    {
        p2p_func_store->process_unbound_events(my_rank);
        coll_func_store->process_unbound_events(my_rank);
    }

  private:
    Mat_Worker &worker();
    Mat_Worker serial_worker;
#ifdef PEAT_OMP
    std::vector<std::unique_ptr<Mat_Worker>> workers; // one per thread, indexed by omp_get_thread_num()
    Mat_Match_Lock match_lock;
    void process_omp_ranks();
#endif
};

#endif
