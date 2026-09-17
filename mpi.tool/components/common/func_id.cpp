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

#include <cassert>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <stdio.h>
#include <string.h>

#include "common_types.h"
#include "func_id.h"
#include "utils.h"

#define _MVAPICH_
#define _MPICH_ 3

#include "func_id_def.h"

#define FIDCERR  \
    if (verbose) \
    std::cerr
const char *Func_ID::noname = "NoName";

Func_ID::Func_ID(int v) : verbose(v), frame_marker(0)
{
    for (PEAT_UID i = 0; mpi_funcs_static[i].id > TMPI_FUNC_ID_NONE && mpi_funcs_static[i].id != MPI_END_OF_LIST; ++i) {
        PEAT_MPI_Func_ID curfunc_id = mpi_funcs_static[i].id;
        std::pair<std::map<PEAT_MPI_Func_ID, const MPIFuncs *>::iterator, PEAT_Bool> ret;
        ret = funcStore.insert(std::pair<PEAT_MPI_Func_ID, const MPIFuncs *>(curfunc_id, &mpi_funcs_static[i]));
        func_idStore[mpi_funcs_static[i].name] = mpi_funcs_static[i].id;
        char tmps[128];
        strcpy(tmps, mpi_funcs_static[i].name);
        peat_strupr(tmps);
        func_idStoreUC[tmps] = mpi_funcs_static[i].id;
        if (ret.second == false) {
            FIDCERR << "element " << mpi_funcs_static[i].id << " already existed with a value of " << ret.first->second->name << "\n";
        }
    }
    /*
    func_idStore["ompt_parallel"] = OMPT_PARALLEL_ID;
    func_idStoreUC["OMPT_PARALLEL"] = OMPT_PARALLEL_ID;
    func_idStore["ompt_nested_parallel_begin"] = OMPT_NESTED_PARALLEL_BEGIN_ID;
    func_idStoreUC["OMPT_NESTED_PARALLEL_BEGIN"] = OMPT_NESTED_PARALLEL_BEGIN_ID;
    func_idStore["ompt_nested_parallel_end"] = OMPT_NESTED_PARALLEL_END_ID;
    func_idStoreUC["OMPT_NESTED_PARALLE_END"] = OMPT_NESTED_PARALLEL_END_ID;
    */
    return;
}

const char *Func_ID::name_by_idx(PEAT_MPI_Func_ID id) const
{
    if (!funcStore.empty()) {
        try {
            const MPIFuncs *fStore = funcStore.at(id);
            assert(fStore);
            return fStore->name;
        } catch (const std::out_of_range &e) {
            std::ostringstream tmp;
            tmp << "func_id::name_by_idx. Have no function name for id " << id << " location: " << __FILE__ << ":" << __LINE__ << " orig exeption " << e.what() << "\n";
            // throw std::out_of_range(tmp.str());
            perror(tmp.str().c_str());
        }
    }
    return noname;
}

PEAT_MPI_Func_ID Func_ID::idx_by_name(const char *name) const
{
    if (func_idStore.count(name) > 0)
        return func_idStore.at(name);
    if (func_idStoreUC.count(name) > 0)
        return func_idStoreUC.at(name);
    return TMPI_FUNC_ID_NONE;
}

/*
T_property Func_ID::event_dataByIdx(const PEAT_MPI_Func_ID id) const
{
    if(!funcStore.empty()) {
        try {
            const MPIFuncs *fStore = funcStore.at(id);
            assert(fStore);
            return fStore->prop[1];
        } catch(const out_of_range &e) {
            ostringstream tmp;
            tmp << "func_id::event_dataByIdx. Have no function name for id " << id << " location: " << __FILE__ << ":" << __LINE__ << " orig exeption " << e.what() << "\n";
            throw out_of_range(tmp.str());
        }
    }
    return T_PROPERTY_NONE;
}*/
PEAT_Bool Func_ID::check_property(const PEAT_MPI_Func_ID id, T_property property_id) const
{
    if (id == TMPI_FUNC_ID_NONE)
        return false;
    if (!funcStore.empty()) {
        try {
            const MPIFuncs *fStore = funcStore.at(id);
            assert(fStore);
            for (int i = 0; i < sizeof(fStore->prop) / sizeof(fStore->prop[0]); i++) {
                if (fStore->prop[i] == property_id)
                    return true;
            }
        } catch (const std::out_of_range &e) {
            std::ostringstream tmp;
            tmp << "func_id::event_dataByIdx. Have no function name for id " << id << " location: " << __FILE__ << ":" << __LINE__ << " orig exeption " << e.what() << "\n";
            throw std::out_of_range(tmp.str());
        }
    }
    return false;
}

std::ostream &operator<<(std::ostream &dbg, const Func_ID *me)
{
    if (!me->funcStore.empty()) {
        for (std::map<PEAT_MPI_Func_ID, const Func_ID::MPIFuncs *>::const_iterator it = me->funcStore.begin(); it != me->funcStore.end(); ++it) {
            const Func_ID::MPIFuncs *fStore = it->second;
            assert(fStore);
            dbg << it->first << ':' << fStore->name << ':' << fStore->prop << "\n";
        }
        dbg << "Total " << me->funcStore.size() << " MPI function definition." << std::endl;
    } else {
        dbg << "Have no MPI function definitions." << std::endl;
    }
    return dbg;
}

std::ostream &operator<<(std::ostream &dbg, const T_property &data)
{
    switch (data) {
    case PEAT_PROP_DATA_TX:
        dbg << "TX";
        break;
    case PEAT_PROP_DATA_RX:
        dbg << "RX";
        break;
    case PEAT_PROP_DATA_COLLECTIVE:
        dbg << "COLLECTIVE";
        break;
    case PEAT_PROP_DATA_LOCAL:
        dbg << "LOCAL";
        break;
    case T_PROPERTY_NONE:
        dbg << "NONE";
        break;
    default:
        dbg << "Reserved";
        break;
    }
    return dbg;
}
