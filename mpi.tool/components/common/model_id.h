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

#ifndef __MODEL_ID_H__
#define __MODEL_ID_H__

#include "common_types.h"
#include <vector>

struct node_info {
    node_info() : freq(-1) {}
    PEAT_Clock freq;
    std::vector<PEAT_Rank> ranks;
    std::string name;
};

struct Model_ID {
    Model_ID();
    void initialize(const Global_Parameters &glob); // function to initialize the object with data instead doing it in ctor. Usefull for parallel environment
    PEAT_Record_Type find_estimator_main_id(const char *modelNameId) const;
    PEAT_MPI_Func_ID find_estimator_function_id(const char *modelFunctionId) const;
    PEAT_Record_Type translate_record_type(PEAT_Record_Type tag) const;

    PEAT_Bool is_smp(const PEAT_Rank rank1, const PEAT_Rank rank2) const;

    std::map<PEAT_Record_Type, PEAT_Record_Type> model_main_tags;     // Key is Model_ID, Value is EstimatorID. Model->Estimator IDs translation
    std::map<PEAT_MPI_Func_ID, PEAT_MPI_Func_ID> model_function_tags; // Key is Model_ID, Value is EstimatorID. Model->Estimator IDs translation
    PEAT_UID frame_marker;
    PEAT_Num num_tscs;
    PEAT_Num num_addrs;
    PEAT_Num num_hwcs;
    std::map<PEAT_Rank, node_info> nodes;                                // map<NodeID, node_info>
    std::map<PEAT_Rank, std::map<PEAT_Rank, node_info>::iterator> ranks; // map<realRank, iteratorToNodes>
};

#endif // __MODEL_ID_H__
