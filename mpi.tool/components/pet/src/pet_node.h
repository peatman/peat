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

#ifndef __PET_NODE_H__
#define __PET_NODE_H__

#include "common_types.h"
#include "pet_event.h"
#include "pet_rank.h"
#include "stat.h"
#include "timeline.h"
#include <assert.h>
#include <set>
class PET_Core;
class PET_Node
{
    std::map<PEAT_Rank, PET_Rank> ranks;
    int verbose; // if true, add algo debug output
    TE_Timeline *abstract_transenvs[PEAT_TRANSPORT_MAX];
    Estimation_Timeline e_timeline;

  public:
    TE_Timeline *get_transenv(PEAT_Transports t_id)
    {
        return abstract_transenvs[t_id];
    }
    void set_finished(PEAT_Rank rank)
    {
        ranks.at(rank).set_finished();
    }
    void set_all_node_ranks_finished()
    {
        for (std::map<PEAT_Rank, PET_Rank>::iterator rank_it = ranks.begin(); rank_it != ranks.end(); ++rank_it) {

            rank_it->second.set_finished();
        }
    }

    PEAT_Bool is_all_finished()
    {
        for (std::map<PEAT_Rank, PET_Rank>::iterator rank_it = ranks.begin(); rank_it != ranks.end(); ++rank_it) {
            if (!rank_it->second.is_finished())
                return false;
        }
        return true;
    }
    PEAT_Bool is_rank_finished(PEAT_Rank rank)
    {
        return ranks.at(rank).is_finished();
    }
    TE_Event *update_te_timelines_write(PEAT_Clock clk);
    TE_Event *update_te_timelines_read(PEAT_Clock clk);
    TE_Event *complete_te_event(TE_Event *evt)
    {
        return abstract_transenvs[evt->tr_env.t_id]->complete(evt);
    }
    TE_Event *insert_te_event(TE_Event *evt)
    {
        return abstract_transenvs[evt->tr_env.t_id]->insert(evt, true);
    }
    void collect_garbage();
    void create_transenvs(const PEAT_Transport *transports);
    PET_Node();
    ~PET_Node();
    friend PET_Core;
    friend PET_Event;
};

#endif //__PET_NODE_H__
