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

#include "ctt.h"
#include "utils.h"
#include <set>

#define TRACEFILEMASK "tractor_t"

#ifdef PEAT_MPI
MPI_Comm urpu_COMPUTE_COMM = MPI_COMM_WORLD;
#endif

Func_ID global_func_id;
#include "model_id.h"
static void determine_fs_rank(const Model_ID &model_id_storage, Global_Parameters &glob_param, int num)
{
    std::map<PEAT_Rank, std::string> com_files = get_file_names(glob_param.input_path, glob_param.trace_files, ".com", glob_param.rank_set, true);
    std::string file_name = glob_param.input_path + DIR_DELIMITER + glob_param.trace_files + glob_param.app_ext;
    std::map<PEAT_Rank, std::string> file_input_app_names;
    file_input_app_names[0] = file_name;
    Communicators *com_store = new Communicators(glob_param);
    com_store->initialize(com_files, file_input_app_names, &model_id_storage);
    std::map<PEAT_Clock, PEAT_Rank> mpi_times;
    for (int rank = 0; rank < com_store->get_app_rank_count(); rank++) {
        const PEAT_MPI_Time &time = com_store->get_mpi_rank_time(rank);
        PEAT_Clock clk = time.clocks;
        while (mpi_times.count(clk) > 0)
            clk++;
        mpi_times[clk] = rank;
    }
    int ranks = 0;
    for (std::map<PEAT_Clock, PEAT_Rank>::iterator time_it = mpi_times.begin(); time_it != mpi_times.end(); time_it++, ranks++) {
        if (ranks >= num)
            break;
        glob_param.rank_set.insert(time_it->second);
    }
    ranks = 0;
    for (std::map<PEAT_Clock, PEAT_Rank>::reverse_iterator time_it = mpi_times.rbegin(); time_it != mpi_times.rend(); time_it++, ranks++) {
        if (ranks >= num)
            break;
        glob_param.rank_set.insert(time_it->second);
    }
    if (com_store)
        delete com_store;
}

static void parse_set(const char *set_str, std::set<PEAT_Rank> &set)
{
    const char *temp1, *temp2;
    char temp_fn[256];
    temp1 = set_str;
    while ((temp2 = strchr(temp1, ',')) != NULL) {
        memcpy(temp_fn, temp1, temp2 - temp1);
        temp_fn[temp2 - temp1] = 0;
        // strupr(temp_fn);
        set.insert(atoi(temp_fn));
        temp1 = temp2 + 1;
    };

    if (strlen(temp1) > 0) {
        strcpy(temp_fn, temp1);
        set.insert(atoi(temp_fn));
    }
    return;
}

static void parse_set(const char *set_str, std::set<std::string> &set)
{
    const char *temp1, *temp2;
    char temp_fn[256];
    temp1 = set_str;
    while ((temp2 = strchr(temp1, ',')) != NULL) {
        memcpy(temp_fn, temp1, temp2 - temp1);
        temp_fn[temp2 - temp1] = 0;
        // strupr(temp_fn);
        set.insert(temp_fn);
        temp1 = temp2 + 1;
    };

    if (strlen(temp1) > 0) {
        strcpy(temp_fn, temp1);
        set.insert(temp_fn);
    }
    return;
}

void help(void)
{
    std::string tstring("Calls Tracing Tool (ctt). Built date: ");
    tstring += __DATE__;
    printf("%s\n", tstring.c_str());
    printf("--help (-h) print this help\n");
    printf("--input=<mask> (-i) select input file mask (default tractor_t)\n");
    printf("--wdir=<dir> (-w) select working (input) directory (default current one)\n");
    printf("--max-list-size=<value> (-m) select maximum number of output enries for each function (default all) \n");
    printf("--rank-set=<comma_sepparated_ranks> (-r) select ranks to process (default all) \n");
    printf("--functions-set=<comma_sepparated_functions> (-f) select functions to process (default all) \n");
    printf("--verbose (-v) print additional info\n");
    printf("\n");
    exit(0);
}

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
    int res = -1, option_index = 0;
    int func_id_print = 0;
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
        {"rank-set", required_argument, 0, 'r'},
        {"max-list-size", required_argument, 0, 'm'},
        {"functions-set", required_argument, 0, 'f'},
        {"verbose", required_argument, 0, 'v'},
        {"help", no_argument, 0, 'h'},
        {0, 0, 0, 0},
    };
    PEAT_Bool need_determine_fs = false;
    int num = 1;
    do {
        res = getopt_long(argc, argv, "i:w:r:f:v:m:h", long_options, &option_index);
        switch (res) {
        case 'i':
            glob_param.trace_files = optarg;
            break;
        case 'w':
            glob_param.input_path = optarg;
            break;
        case 'm':
            glob_param.max_list_size = atoi(optarg);
            break;
        case 'r': {
            char *rank_set_str = optarg;
            char *fs_str = strstr(rank_set_str, "fs");
            if (fs_str) {
                if (strlen(fs_str) > 3) {
                    num = atoi(&fs_str[3]);
                }
                need_determine_fs = true;
            } else {
                parse_set(rank_set_str, glob_param.rank_set);
            }
        } break;
        case 'f': {
            char *func_set_str = optarg;
            parse_set(func_set_str, glob_param.func_set);
        } break;
        case 'v':
            glob_param.verbose = atoi(optarg);
            break;
        case 'h':
            help();
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
    {
        char abs_path[4096];
#ifdef WIN32
        if (_fullpath(abs_path, glob_param.input_path.c_str(), sizeof(abs_path)) != nullptr) {
            glob_param.input_path = abs_path;
        }
        if (!glob_param.output_path.empty()) {
            if (_fullpath(abs_path, glob_param.output_path.c_str(), sizeof(abs_path)) != nullptr) {
                glob_param.output_path = abs_path;
            }
        }
#else
        if (realpath(glob_param.input_path.c_str(), abs_path) != nullptr) {
            glob_param.input_path = abs_path;
        }
        if (!glob_param.output_path.empty()) {
            if (realpath(glob_param.output_path.c_str(), abs_path) != nullptr) {
                glob_param.output_path = abs_path;
            }
        }
#endif
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

        if (need_determine_fs)
            determine_fs_rank(model_id_storage, glob_param, num);
        std::map<PEAT_Rank, std::string> HWC_files = get_file_names(glob_param.input_path, glob_param.trace_files, ".hwc", glob_param.rank_set, true);
        std::map<PEAT_Rank, std::string> MPI_files = get_file_names(glob_param.input_path, glob_param.trace_files, ".mpi", glob_param.rank_set, true);
        glob_param.trace_comm_size = HWC_files.size();

        glob_param.whole_ranks.resize(glob_param.trace_comm_size);
        glob_param.my_ranks.reserve(glob_param.trace_comm_size);
        {
            PEAT_Rank rank_num = 0;
            for (std::map<PEAT_Rank, std::string>::const_iterator file_it = MPI_files.cbegin(); file_it != MPI_files.cend(); ++file_it, rank_num++) {
                PEAT_Rank rank = file_it->first;
                glob_param.whole_ranks[rank_num] = rank % glob_param.comm_size;
#ifdef PEAT_MPI
                if (rank % glob_param.comm_size == glob_param.comm_rank)
#endif
                {
                    glob_param.my_ranks.push_back(rank);
#ifdef PEAT_MPI
                    fprintf(stderr, "Rank:%d is handling rank %d.\n", glob_param.comm_rank, rank);
#endif
                }
            }
        }

        for (std::vector<PEAT_Rank>::const_iterator it = glob_param.my_ranks.cbegin(); it != glob_param.my_ranks.cend(); ++it) {
            {
                glob_param.pe_MPI_files_in_rank[*it] = MPI_files.at(*it);
                glob_param.pe_HWC_files_in_rank[*it] = HWC_files.at(*it);
            }
        }
        CTT ctt(glob_param, model_id_storage);
        if (0 == glob_param.comm_rank) {
            std::cout << "Start processing." << std::endl;
        }
        ctt.process();
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
