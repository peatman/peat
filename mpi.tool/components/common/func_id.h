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

#ifndef __FUNC_ID_H__
#define __FUNC_ID_H__

#include "common_plain_types.h"
#include "mpi_func_ids.h"
#include <map>
#include <string>
#include <vector>

class Func_ID
{
  public:
    explicit Func_ID(int verbose = false);
    PEAT_Num size() const
    {
        return funcStore.size();
    }
    PEAT_Bool empty() const
    {
        return funcStore.empty();
    }
    const char *name_by_idx(PEAT_MPI_Func_ID idx) const;
    PEAT_MPI_Func_ID idx_by_name(const char *name) const;
    // const char *func_idOrNameByIdx(PEAT_MPI_Func_ID idx) const;//respects nameInsteadIdx. if nameInsteadIdx=true, returns name. Overwise return ID.
    // PEAT_Property event_dataByIdx(const PEAT_MPI_Func_ID id) const;
    PEAT_Bool check_property(const PEAT_MPI_Func_ID id, PEAT_Property property_id) const;
    PEAT_Bool check_tx(const PEAT_MPI_Func_ID id)
    {
        return check_property(id, PEAT_PROP_DATA_TX) || check_property(id, PEAT_PROP_DATA_TXRX);
    }
    PEAT_Bool check_rx(const PEAT_MPI_Func_ID id)
    {
        return check_property(id, PEAT_PROP_DATA_RX) || check_property(id, PEAT_PROP_DATA_TXRX);
    }

    friend std::ostream &operator<<(std::ostream &, const Func_ID *);
    // Do not use std::string as members of the type. This broke all members memory reallocation if shared object created
    struct MPIFuncs {
        const PEAT_MPI_Func_ID id;
        const char *prefix;
        const char *name;
        const char *parameters;
        const char *suffix;
        const char *extra_out;
        struct {
            const PEAT_Property prop[16];
        };
    };
    static const MPIFuncs mpi_funcs_static[];
    const std::map<PEAT_MPI_Func_ID, const MPIFuncs *> &data() const
    { // used in Collector
        return funcStore;
    }
    int getFrameMarker() const { return frame_marker; }

  private:
    int verbose;
    std::map<PEAT_MPI_Func_ID, const MPIFuncs *> funcStore;
    std::map<std::string, PEAT_MPI_Func_ID> func_idStore;
    std::map<std::string, PEAT_MPI_Func_ID> func_idStoreUC; // to support old traces
    static const char *noname;                              // if no function in the database
    int frame_marker;
};
extern Func_ID global_func_id;

std::ostream &operator<<(std::ostream &dbg, const PEAT_Property &data);

#endif // __FUNC_ID_H__
