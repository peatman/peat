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

#include <algorithm>
#include <assert.h>
#include <errno.h>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits.h>
#include <numeric> //std::accumulate
#include <set>
#include <sstream>
#include <stdexcept>
#include <stdlib.h>

#include "pet_core.h"
#include "stat.h"
#include "utils.h"

PEAT_Bool Coll_Storage::query_release()
{
    if (!assembled)
        return false;
    for (std::map<PEAT_Rank, PET_Coll_Event *>::iterator coll_it = events.begin(); coll_it != events.end(); ++coll_it) {
        if (!coll_it->second->query_release())
            return false;
    }
    return true;
}

PEAT_Bool Coll_Storage::update_release()
{
    if (!assembled)
        return false;
#ifdef PEAT_OMP
    coll_mutex.lock();
#endif
    for (std::map<PEAT_Rank, PET_Coll_Event *>::iterator evt_it = events.begin(); evt_it != events.end(); ++evt_it) {
        evt_it->second->update_pending();
    }
#ifdef PEAT_OMP
    coll_mutex.unlock();
#endif
    return true;
}

PEAT_Bool Coll_Storage::check_coll(PET_Coll_Event *event)
{
    if (func_id != event->func_id)
        return false;
    if (comm != event->comm)
        return false;
    if (events.count(event->rank) > 0)
        return false;
    if (expected_ranks.count(event->rank) < 1)
        return false;
    return true;
}
