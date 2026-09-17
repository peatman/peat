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

#include <iomanip>
#include <list>
#include <sstream>
#include <stdexcept>

#include <assert.h>
#include <limits.h>

#include "func_id.h"
#include "pet_core.h"
#include "pet_event.h"
#include "utils.h"

#define P2PDELIM '|'

#define event_dbg(x)  \
    if (verbose > x) \
    cerr

PET_Node::PET_Node()
{
    verbose = 1;
}
void PET_Node::create_transenvs(const PEAT_Transport *transports)
{
    for (int i = 0; i < PEAT_TRANSPORT_MAX; i++) {
        if (transports->te_orig[i].initialized) {
            if (transports->te_orig[i].is_parallel) {
                abstract_transenvs[i] = new PAR_Timeline(transports->te_orig[i].chain_num, transports->te_orig[i].unit_size, transports->te_orig[i].packet_size);
            } else {
                abstract_transenvs[i] = new SEQ_Timeline(transports->te_orig[i].chain_num, transports->te_orig[i].unit_size, transports->te_orig[i].packet_size);
            }
            abstract_transenvs[i]->set_estimate_read(transports->globals.estimate_read);
        } else {
            abstract_transenvs[i] = NULL;
        }
    }
}
PET_Node::~PET_Node()
{
    for (int i = 0; i < PEAT_TRANSPORT_MAX; i++) {
        if (abstract_transenvs[i]) {
            delete abstract_transenvs[i];
            abstract_transenvs[i] = NULL;
        }
    }
}

void PET_Node::collect_garbage()
{
    for (std::map<PEAT_Rank, PET_Rank>::iterator rank_it = ranks.begin(); rank_it != ranks.end(); ++rank_it) {

        rank_it->second.collect_garbage();
    }
}
#define compare(e_tmp, e)                                                                              \
    if (e_tmp) {                                                                                       \
        if (e_tmp->t_finish <= 0) {                                                                    \
            pet_throw_error(e_tmp->owner->get_core(), e_tmp->owner, e_tmp, "Empty Event received.\n"); \
        }                                                                                              \
        if (e) {                                                                                       \
            if (e_tmp->t_finish < e->t_finish) {                                                       \
                {                                                                                      \
                    e = e_tmp;                                                                         \
                }                                                                                      \
            }                                                                                          \
        } else {                                                                                       \
            {                                                                                          \
                e = e_tmp;                                                                             \
            }                                                                                          \
        }                                                                                              \
    }

TE_Event *PET_Node::update_te_timelines_write(PEAT_Clock clk)
{
    TE_Event *e = NULL;
    TE_Event *e_tmp = NULL;
    for (int i = 0; i < PEAT_TRANSPORT_MAX; i++) {
        if (abstract_transenvs[i]) {
            e_tmp = abstract_transenvs[i]->update_write(clk, false);
            compare(e_tmp, e);
        }
    }
    return e;
}
TE_Event *PET_Node::update_te_timelines_read(PEAT_Clock clk)
{
    TE_Event *e = NULL;
    TE_Event *e_tmp = NULL;
    for (int i = 0; i < PEAT_TRANSPORT_MAX; i++) {
        if (abstract_transenvs[i]) {
            e_tmp = abstract_transenvs[i]->update_read(clk, false);
            compare(e_tmp, e);
        }
    }
    return e;
}