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

#include "model_id.h"
#include "file_io.h"
#include "func_id.h"
#include <cassert>

Model_ID::Model_ID()
{
    frame_marker = -1;
    num_tscs = -1;
    num_addrs = -1;
    num_hwcs = -1;
}

void Model_ID::initialize(const Global_Parameters &glob)
{
    std::ostringstream errTxt;
    const std::string file_name = glob.input_path + DIR_DELIMITER + glob.trace_files + glob.app_ext;
    std::map<PEAT_Rank, std::string> fileSet;
    fileSet[0] = file_name;
    File_Storage_GZ appFile(fileSet, PEAT_FIO_FLAG_READ | PEAT_FIO_FLAG_LAZY | PEAT_FIO_FLAG_ICLOSE, NULL, glob.verbose);
    if (!appFile.is_any_file_good()) {
        errTxt << "Trace is not usable. Can't open file " << file_name << " for reading" << std::endl;
        throw std::runtime_error(errTxt.str());
    }
    String_Split line;
    PEAT_Bool definitionsEnd = false;
    PEAT_Bool hasMoreLines = true;
    while (hasMoreLines) {
        hasMoreLines = appFile.read_lines(line, 0, ':');
        if (line.size() < 2) {
            continue;
        }
        PEAT_Record_Type fr_type = PEAT_RT_Non_Existent;
        try {
            extract(fr_type, line.at(0));
        } catch (const std::invalid_argument &ex) {
            std::cerr << "Main model ID file " << file_name << " shouldn't contain lines with non-numeric prefix(\"" << line.at(0) << "\"). Error: " << ex.what() << "\n";
            continue;
        }
        assert(-1 != fr_type);
        if (PEAT_RT_Definitions == fr_type && !definitionsEnd) {
            const char *modelMainIdName = line.at(2);
            PEAT_Record_Type fr_type_TagId = PEAT_RT_Non_Existent;
            extract(fr_type_TagId, line.at(1));
            assert(modelMainIdName);
            PEAT_Record_Type currentVersionTagId = find_estimator_main_id(modelMainIdName);
            if (currentVersionTagId >= PEAT_RT_Remark) {
                model_main_tags[fr_type_TagId] = currentVersionTagId;
                if (PEAT_RT_End_Of_Definitions == currentVersionTagId) {
                    definitionsEnd = true;
                }
            }
        } else {
            PEAT_Record_Type estimatorId = PEAT_RT_Non_Existent;
            try {
                estimatorId = model_main_tags.at(fr_type);
            } catch (const std::out_of_range &ex) {
                errTxt << "Undefined tag (" << fr_type << ") provided in Model_ID::ctor(). Error: " << ex.what() << "\n";
                throw std::runtime_error(errTxt.str());
            }
            switch (estimatorId) {
            case PEAT_RT_Frame_Marker: {
                extract(frame_marker, line.at(1)); // never used ?
            } break;
            case PEAT_RT_MPM_Master_Ranks: {
                // extract(frame_marker, lines.at(1));
            } break;
            case PEAT_RT_MPM_Nodes_Freq: {
                if (7 != line.size()) {
                    errTxt << "MPM misc format changed. Expected 6 columns in app file, but " << line.size() << " exists\n";
                    throw std::runtime_error(errTxt.str());
                }
                PEAT_Rank nodeId = 0;
                extract(nodeId, line.at(1)); // rank in master comm
                PEAT_Rank rank = 0;
                extract(rank, line.at(2)); // rank in comm world
                PEAT_FREQ rankFreq = 0;
                extract(rankFreq, line.at(3)); // frequency
                node_info &node = nodes[nodeId];
                node.name = line.at(5);
                node.freq = rankFreq;
            } break;
            case PEAT_RT_MPM_Ranks_And_PPN: {
                // new format
                PEAT_Rank nodePPN = 0;
                extract(nodePPN, line.at(3));
                PEAT_Rank nodeID = 0;
                extract(nodeID, line.at(4));
                // PEAT_Rank masterRank = 0;  extract(masterRank, line.at(3));

                std::pair<std::map<PEAT_Rank, node_info>::iterator, PEAT_Bool> ret;
                ret = nodes.insert(std::pair<PEAT_Rank, node_info>(nodeID, node_info()));

                node_info &node = ret.first->second;
                node.name = line.at(1);
                extract(node.freq, line.at(2));
                std::string ranksLine = line.at(5);
                if (!ranksLine.empty()) {
                    node.ranks.resize(nodePPN, -1);
                    std::size_t linePos = 0;
                    for (int i = 0; i < nodePPN; ++i) {
                        PEAT_Rank smpRank = 0;
                        extract(smpRank, ranksLine.c_str() + linePos);
                        node.ranks[i] = smpRank;
                        ranks[smpRank] = ret.first; // populate ranks storage with node pointers
                        linePos = ranksLine.find_first_of(":,", linePos);
                        if (std::string::npos != linePos) {
                            ++linePos;
                        } else {
                            break;
                        }
                    }
                }
            } break;
            case PEAT_RT_Var_Block_Info: {
                num_tscs = 4;
                extract(num_addrs, line.at(1));
                extract(num_hwcs, line.at(2));
            } break;
            case PEAT_RT_MPM_All_Ranks_Exec_Time:
                /* skip it*/
                break;
            case PEAT_RT_MPI_Function_Name_ID: {
                // cerr << lines << endl;
                PEAT_MPI_Func_ID modelTagId = TMPI_FUNC_ID_NONE;
                extract(modelTagId, line.at(1));
                if (modelTagId > 0) {
                    const char *modelFunctionIdName = line.at(2);
                    assert(modelFunctionIdName);
                    model_function_tags[modelTagId] = find_estimator_function_id(modelFunctionIdName);
                }
            } break;
            case PEAT_RT_Sync_Point:
                break;

            case PEAT_RT_Global_Communicators_Report:
                break; // to avoid printing
            case PEAT_RT_Remark: {
                // extract(frame_marker, lines.at(1));
            } break;

            default: {
                std::cerr << "Unhandled tag(" << estimatorId << " - " << print_record_type(estimatorId) << ") in Model_ID::ctor()\n";
            } break;
            }
        }
    }
}
PEAT_Record_Type inc_frt(PEAT_Record_Type r)
{
    int i = (int)r;
    ++i;
    return (PEAT_Record_Type)i;
}
PEAT_Record_Type Model_ID::find_estimator_main_id(const char *modelNameId) const
{
    assert(modelNameId);
    // size_t modelStringSize = strlen(modelNameId);
    for (PEAT_Record_Type i = PEAT_RT_Remark; i <= PEAT_RT_End_Of_Definitions; i = inc_frt(i)) {
        const char *estimatorMainIdName = print_record_type(i);
        if (estimatorMainIdName) {
            // size_t estimatorStringSize = strlen(estimatorMainIdName);
            if (0 == strcasecmp(modelNameId, estimatorMainIdName)) {
                // cerr << "MATCHED TO " << estimatorMainIdName << " return " << i << endl;
                return i;
            }
        }
    }
    std::cerr << "Model contains unsupported tag \"" << modelNameId << "\". Errors while processing might occur\n";
    //    ostringstream errTxt;
    //    errTxt << "Model contains unsupported tag \"" << modelNameId << "\" and can't be processed\n";
    //    throw runtime_error(errTxt.str());
    return PEAT_RT_Non_Existent;
}

PEAT_MPI_Func_ID Model_ID::find_estimator_function_id(const char *modelFunctionId) const
{
    assert(modelFunctionId);
    // size_t modelStringSize = strlen(modelFunctionId);
    for (std::map<PEAT_MPI_Func_ID, const Func_ID::MPIFuncs *>::const_iterator it = global_func_id.data().begin(); it != global_func_id.data().end(); ++it) {
        const char *estimatorFunctionIdName = it->second->name;
        const PEAT_MPI_Func_ID estimatorFunctionId = it->second->id;
        if (estimatorFunctionIdName) {
            // size_t estimatorStringSize = strlen(estimatorFunctionIdName);
            if (0 == strcasecmp(modelFunctionId, estimatorFunctionIdName)) {
                // cerr << "MATCHED TO " << estimatorFunctionIdName << " return " << estimatorFunctionId << endl;
                return estimatorFunctionId;
            }
        }
    }
    std::ostringstream errTxt;
    errTxt << "Model contains unsupported function name \"" << modelFunctionId << "\" which can't be processed\n";
    // throw std::runtime_error(errTxt.str());
    peat_warning(errTxt.str().c_str());
    return TMPI_FUNC_ID_NONE;
}

PEAT_Bool Model_ID::is_smp(const PEAT_Rank rank1, const PEAT_Rank rank2) const
{
    PEAT_Bool ret = false;
    if (!ranks.empty()) {
        if (ranks.count(rank1) && ranks.count(rank2)) {
            ret = (ranks.at(rank1) == ranks.at(rank2));
        } else {
            std::cerr << "Model_ID::is_smp. No requested ranks available rank1: "
                      << rank1 << "(" << ranks.count(rank1) << ") rank2: "
                      << rank2 << "(" << ranks.count(rank2) << ")\n";
        }
    } else {
        ret = true; // PTPCERR << "No data in SMP_to_master available. SMP comminucation assumed.\n";
    }
    return ret;
}

PEAT_Record_Type Model_ID::translate_record_type(PEAT_Record_Type tag) const
{
    if (model_main_tags.count(tag) == 1)
        return model_main_tags.at(tag);
    return PEAT_RT_Non_Existent;
}
