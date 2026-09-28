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

#ifndef __COLLSTORAGE_H__
#define __COLLSTORAGE_H__

#include <climits>
#include <fstream>
#include <set>

#include "pet_event.h"

struct Coll_Storage {
    Coll_Storage() : size(0), comm(0), func_id(TMPI_FUNC_ID_NONE), assembled(false) {}
    std::map<PEAT_Rank, PEAT_Rank> expected_ranks;
    std::map<PEAT_Rank, PET_Coll_Event *> events;
    std::map<PEAT_Rank, PEAT_Event_IDs> coll_uids; // id path of every member record
    std::map<PEAT_Rank, PEAT_Bool> shutdown_ranks;
    PEAT_Num size; // known at the begining
    PEAT_MPI_Comm comm;
    PEAT_MPI_Func_ID func_id;
    PEAT_Bool assembled; // true is all events assembled
    PEAT_Bool query_release();
    PEAT_Bool update_release();
    PEAT_Bool shutdown_rank(PEAT_Rank rank)
    {
        if (events.count(rank) == 1) {
            events.erase(rank);
            shutdown_ranks[rank] = true;
            return true;
        }
        return false;
    }
    // PEAT_Bool estimate();
    PEAT_Bool check_coll(PET_Coll_Event *);
    Coll_Storage(const Coll_Storage &s)
    {
        expected_ranks = s.expected_ranks;
        events = s.events;
        coll_uids = s.coll_uids;
        shutdown_ranks = s.shutdown_ranks;
        size = s.size;
        comm = s.comm;
        func_id = s.func_id;
        assembled = s.assembled;
    }

  private:
#ifdef PEAT_OMP
    PEAT_Mutex coll_mutex;
#endif
};
std::ostream &operator<<(std::ostream &dbg, const Coll_Storage &rec);
#endif