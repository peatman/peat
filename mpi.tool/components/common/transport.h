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

#ifndef __TRANSPORT_H__
#define __TRANSPORT_H__

#include "common_plain_types.h"
#include "model_id.h"
#include "transenv.h"
#include <cmath>
#include <map>
#include <string>

class PEAT_Transport
{
    int verbose;
    Trasport_Corection_Environment te_corr_orig[PEAT_TRANSPORT_MAX];
    Trasport_Corection_Environment te_mod[PEAT_TRANSPORT_MAX];

  public:
    PEAT_Ext_Trasport_Environment te_orig[PEAT_TRANSPORT_MAX];
    Global_Parameters globals;
    std::map<PEAT_Num, std::map<PEAT_MPI_Func_ID, Function_Environment>> func_env; // either statistical or real
    PEAT_Transport(const Global_Parameters &glob, const PEAT_FREQ target_freq = 1000000000);
    virtual ~PEAT_Transport();
    PEAT_Trasport_Environment get_env(int which, PEAT_MPI_Func_ID func_id, PEAT_MPI_Func_ID parent_func_id, PEAT_Message_Size message_size, PEAT_Num numa_ppn, PET_Transport_Selector ts);
    PEAT_Trasport_Environment cvt_env(const PEAT_Ext_Trasport_Environment &evt_in, PET_Transport_Selector ts);

  private:
    void read_trasport_profile(const std::string &dir, const std::string &name);
    void write_func_profile(const std::string &dir, const std::string &name) const;
    void parse(const String_Split &line, PEAT_Rank rank);
    void convert_profiles();
    PEAT_FREQ tgt_freq;
};

#endif // __TRANSPORT_H__