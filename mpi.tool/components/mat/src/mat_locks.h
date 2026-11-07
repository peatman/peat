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

#ifndef __MAT_LOCKS_H__
#define __MAT_LOCKS_H__

/* Locks of mat.omp (PEAT_OMP): its threads each walk their own trace ranks and share the matching structures.
   - Each rank's request store, held wildcard receives, tsc state etc. are only touched by the thread owning the rank.
   - P2P matching: one lock per trace rank (P2PAL::Rank_Guard): adding to a rank's queues takes that rank's lock,
     pairing a send of s with a receive of r takes both, the lower rank first.
   - Collectives, communicator, one-sided and GPU stores: one lock for all of them, mat_store_lock (MAT_STORE_GUARD).
   Spin locks: they are held briefly and taken for nearly every event, so waiting for a futex wake-up would cost more.
   In serial mat (no PEAT_OMP) none of this exists and MAT_STORE_GUARD is empty. */
#include "common_types.h"

#include <cstdio>
#include <sstream>
#include <string>

/* one stderr line written with a single call: stdio locks the stream for it, while each << of std::cerr is a write of
   its own, so lines of several threads came out interleaved. Usage: MAT_CERR("Rank " << r << " ..." << std::endl); */
#define MAT_CERR(stream_expr)                                                                                      \
    do {                                                                                                           \
        std::ostringstream mat_cerr_line;                                                                          \
        mat_cerr_line << stream_expr;                                                                              \
        fputs(mat_cerr_line.str().c_str(), stderr);                                                               \
    } while (0)

#ifdef PEAT_OMP
#include <atomic>
#include <mutex>
#include <thread>

class Mat_Spin_Lock
{
    std::atomic_flag taken = ATOMIC_FLAG_INIT;

  public:
    void lock()
    {
        for (unsigned spins = 0; taken.test_and_set(std::memory_order_acquire); spins++)
            if (spins > 1000)
                std::this_thread::yield();
    }
    void unlock() { taken.clear(std::memory_order_release); }
};

/* recursive: a collective store call can lead to another one under the same lock */
class Mat_Recursive_Spin_Lock
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

extern Mat_Recursive_Spin_Lock mat_store_lock; // collectives, comm, osc, gpu stores
#define MAT_STORE_GUARD std::lock_guard<Mat_Recursive_Spin_Lock> mat_store_guard(mat_store_lock)
#else
#define MAT_STORE_GUARD
#endif

#endif // __MAT_LOCKS_H__
