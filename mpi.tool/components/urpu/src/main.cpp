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

#ifdef PEAT_MPI
#include "mpi.h"
#endif

#include "getopt.h"
#include <iostream>

#include <time.h>

#include "urpu.h"
#include "utils.h"

#define TRACEFILEMASK "tractor_t"
#define ADDRFILE "tractor_t.urpu"

#ifdef PEAT_MPI
MPI_Comm urpu_COMPUTE_COMM = MPI_COMM_WORLD;
#endif

Func_ID global_func_id;
#include "model_id.h"

int main(int argc, char **argv)
{
    Global_Parameters glob_param;

#ifdef PEAT_MPI
    MPI_Init(&argc, &argv);
    glob_param.comm_compute = MPI_COMM_WORLD;
    MPI_Comm_size(glob_param.comm_compute, &glob_param.comm_size);
    MPI_Comm_rank(glob_param.comm_compute, &glob_param.comm_rank);
#else
    glob_param.comm_size = 1;
    glob_param.comm_rank = 0;
#endif

    std::string final_string = "Error.";
    std::string tstring("User Regions Processing Utility (urpu). Built date: ");
    tstring += __DATE__;
    time_t tbegin = time(0);
    int res = -1, option_index = 0;
    int func_id_print = 0;
    glob_param.user_region_file = ADDRFILE;
    glob_param.com_ext = ".com";
    glob_param.output_files = ".out";
    glob_param.trace_files = TRACEFILEMASK;
    glob_param.ptpd_ext = ".ptp";
    glob_param.app_ext = ".app";
    glob_param.bind_files = ".bind";
    glob_param.input_path = ".";
    struct option long_options[] = {
        {"input", required_argument, 0, 'i'},
        {"wdir", required_argument, 0, 'w'},
        {"skip", no_argument, 0, 's'},
        {"pc_levels", required_argument, 0, 'p'},
        {"verbose", required_argument, 0, 'v'},
        {0, 0, 0, 0},
    };
    do {
        res = getopt_long(argc, argv, "i:w:p:v:s ", long_options, &option_index);
        switch (res) {
        case 'i':
            glob_param.trace_files = optarg;
            break;
        case 'w':
            glob_param.input_path = optarg;
            break;
        case 'p':
            glob_param.pc_levels = optarg;
            break;
        case 'v':
            glob_param.verbose = atoi(optarg);
            break;
        case 's':
            glob_param.skip_processing = 1;
            break;
        }
    } while (res != -1);
    if (optind < argc) {
        std::cout << "Unknown options:";
        while (optind < argc) {
            std::cout << " " << argv[optind++];
        }
        std::cout << std::endl;
    }

    if (func_id_print) {
        if (0 == glob_param.comm_rank) {
            // func_id *func_id = new func_id(glob_param.verbose);
            std::cout << &global_func_id << std::endl;
            // delete func_id;
        }
    } else {
        Model_ID model_id_storage;
        model_id_storage.initialize(glob_param);

        std::map<PEAT_Rank, std::string> HWC_files = get_file_names(glob_param.input_path, glob_param.trace_files, ".hwc", true);
        glob_param.trace_comm_size = HWC_files.size();
        std::string outpath = glob_param.input_path;
        peat_mkdir(outpath, glob_param.verbose);
#ifdef PEAT_MPI
        glob_param.whole_ranks.resize(glob_param.trace_comm_size);
        glob_param.my_ranks.reserve(glob_param.trace_comm_size);
        for (PEAT_Rank rank = 0; rank < glob_param.trace_comm_size; rank++) {
            glob_param.whole_ranks[rank] = rank % glob_param.comm_size;
            if (rank % glob_param.comm_size == glob_param.comm_rank) {
                glob_param.my_ranks.push_back(rank);
            }
        }
#endif

        for (std::map<PEAT_Rank, std::string>::const_iterator it = HWC_files.begin(); it != HWC_files.end(); ++it) {
#ifdef PEAT_MPI
            if (glob_param.whole_ranks[it->first] == glob_param.comm_rank)
#endif
            {
                glob_param.pe_HWC_files_in_rank[it->first] = it->second;
            }
        }
        urpu urpu(glob_param, model_id_storage);
        if (0 == glob_param.comm_rank) {
            std::cout << "Start processing." << std::endl;
        }
        urpu.process();
        if (0 == glob_param.comm_rank) {
            std::cout << "End processing." << std::endl;
        }
    }
#ifdef PEAT_MPI
    MPI_Barrier(MPI_COMM_WORLD);
    MPI_Finalize();
#endif

    return 0;
}
