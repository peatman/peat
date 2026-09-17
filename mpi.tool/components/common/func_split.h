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

#ifndef __FUNC_SPLIT_H__
#define __FUNC_SPLIT_H__

#include "common_types.h"
#include "model_id.h"
#include <cmath>
#include <map>
#include <string>

class PEAT_Function_Splitter
{
    int verbose;

  public:
    PEAT_Function_Splitter(const Global_Parameters &glob);
    virtual ~PEAT_Function_Splitter();
    PEAT_Time_Parts *get_func_split(const PEAT_Transports t, const PEAT_Func_ID_Pair &func_id_pair, PEAT_Rank num_ranks, PEAT_Message_Size size);

  private:
    void read_func_split_profile(const std::string &dir, const std::string &name);
    void parse(const String_Split &line, PEAT_Rank rank);
    Global_Parameters globals;
    std::string file_name;
    std::string file_dir;
    std::map<PEAT_Func_ID_Pair, std::map<PEAT_Rank, std::list<PEAT_Ext_Time_Parts>>> func_split_env[PEAT_TRANSPORT_MAX];
};

#endif // __FUNC_SPLIT_H__