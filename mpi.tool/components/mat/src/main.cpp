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
#include <stdlib.h>
#include <string>

#include <time.h>

#include "mat.h"
#include "utils.h"

#ifdef PEAT_OMP
#include "omp.h"
#endif

#define TRACEFILEMASK "tractor_t"
#define ADDRFILE "0.profile_user.in"
#define EAGERTHREASHOLD 262144 // 256k bytes

#ifdef PEAT_MPI
MPI_Comm mat_COMPUTE_COMM = MPI_COMM_WORLD;
#endif

Func_ID global_func_id;
#include "model_id.h"

std::string make_out_file_name(std::string path, std::string str_in, std::string out_file, std::string ext)
{
    //    string::size_type found_pos = cur_file.find_last_of(".");
    std::string str_out = path;
    std::string str_i = str_in;
    size_t found_dir_d = str_in.rfind(DIR_DELIMITER);
    if (found_dir_d != std::string::npos) {
        str_i = str_in.substr(found_dir_d);
    } else {
        str_i = str_in;
    }
    if (!str_out.ends_with(DIR_DELIMITER))
        str_out.append(DIR_DELIMITER_CSTR);
    size_t found_ext = str_i.find(ext);
    if (found_ext != std::string::npos && ext.length() > 0) {
        str_out += str_i.substr(0, found_ext);
        str_out += out_file + ext;
    } else {
        str_out += str_i + out_file;
    }
    return str_out;
}
void help(int rank)
{
    if (rank == 0) {
        std::string tstring("Matching and Statistic Module(mat). Built date: ");
        tstring += __DATE__;
        printf("%s\n", tstring.c_str());
        printf("--help (-h) print this help\n");
        printf("--input=<mask> (-i) select input file mask (default tractor_t)\n");
        printf("--output=<dir> (-o) select output directory (default ./out)\n");
        printf("--wdir=<dir> (-w) select working (input) directory (default current one)\n");
        printf("--matching-only (-m) only write .bind files, don't rewrite .mpi/.hwc, don't open .hwc input, and skip negative-time correction/imbalance calculation (default off)\n");
        printf("--verbose (-v) print additional info\n");
        printf("\n");
    }
    exit(0);
}

int main(int argc, char **argv)
{
    Global_Parameters glob_param;
    std::string trace_info_file;

#ifdef PEAT_MPI
    int provided = 0;
    MPI_Init_thread(&argc, &argv, MPI_THREAD_MULTIPLE, &provided);
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
    glob_param.user_region_file = ADDRFILE;
    glob_param.com_ext = ".com";
    glob_param.output_files = ".out";
    glob_param.trace_files = TRACEFILEMASK;
    glob_param.ptpd_ext = ".ptp";
    glob_param.mpi_files = ".mpi";
    glob_param.hwc_files = ".hwc";
    glob_param.app_ext = ".app";
    glob_param.bind_files = ".bind";
    glob_param.input_path = ".";

    struct option long_options[] = {
        {"input", required_argument, 0, 'i'},
        {"output", required_argument, 0, 'o'},
        {"wdir", required_argument, 0, 'w'},
        {"skip", no_argument, 0, 's'},
        {"pc_levels", required_argument, 0, 'l'},
        {"matching-only", no_argument, 0, 'm'},
        {"print_on_send", no_argument, 0, 'p'},
        {"verbose", no_argument, 0, 'v'},
        {"help", no_argument, 0, 'h'},
        {0, 0, 0, 0},
    };
    do {
        res = getopt_long(argc, argv, "i:w:o:l:c:phvsm", long_options, &option_index);
        switch (res) {
        case 'i':
            glob_param.trace_files = optarg;
            break;
        case 'w':
            glob_param.input_path = optarg;
            break;
        case 'm':
            glob_param.matching_only = 1;
            break;
        case 'o':
            glob_param.output_path = optarg;
            break;
        case 'l':
            glob_param.pc_levels = optarg;
            break;
        case 'v':
            glob_param.verbose = 1;
            break;
        case 's':
            glob_param.skip_processing = 1;
            break;
        case 'p':
            glob_param.print_location = 1;
            break;
        case 'c':
#ifdef PEAT_MPI
        {
            char *endptr;
            long long tmp = strtoll(optarg, &endptr, 10);
            if (endptr == NULL || strlen(endptr) == 0)
                glob_param.cycle_events = (unsigned long long)tmp;
        }
#endif
        break;
        case 'h':
            help(glob_param.comm_rank);
            break;
        }
    } while (res != -1);
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
        if (glob_param.output_path.empty()) {
            glob_param.output_path = glob_param.input_path + DIR_DELIMITER + "out";
        }
        model_id_storage.initialize(glob_param);

        std::map<PEAT_Rank, std::string> MPI_files = get_file_names(glob_param.input_path, glob_param.trace_files, glob_param.mpi_files, true);
        std::map<PEAT_Rank, std::string> HWC_files;
        if (!glob_param.matching_only) {
            HWC_files = get_file_names(glob_param.input_path, glob_param.trace_files, glob_param.hwc_files, true);
        }
        glob_param.trace_comm_size = MPI_files.size();
        peat_mkdir(glob_param.output_path, glob_param.verbose);
        trace_info_file = make_out_file_name(glob_param.output_path, glob_param.trace_files, ".trace.info", "");
        glob_param.whole_ranks.resize(glob_param.trace_comm_size);
        glob_param.my_ranks.reserve(glob_param.trace_comm_size);
        for (PEAT_Rank rank = 0; rank < glob_param.trace_comm_size; rank++) {
            glob_param.whole_ranks[rank] = rank % glob_param.comm_size;
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
        for (std::vector<PEAT_Rank>::const_iterator it = glob_param.my_ranks.cbegin(); it != glob_param.my_ranks.cend(); ++it) {
            {
                glob_param.pe_MPI_files_in_rank[*it] = MPI_files.at(*it);
                std::string mpi_out_name = make_out_file_name(glob_param.output_path, MPI_files.at(*it), glob_param.output_files, ".gz");
                std::string bind_out_name = mpi_out_name;
                bind_out_name.replace(bind_out_name.rfind(glob_param.mpi_files), glob_param.mpi_files.length(), glob_param.bind_files);
                glob_param.pe_bind_files_in_rank_out[*it] = bind_out_name;
                if (!glob_param.matching_only) {
                    glob_param.pe_HWC_files_in_rank[*it] = HWC_files.at(*it);
                    glob_param.pe_HWC_files_in_rank_out[*it] = make_out_file_name(glob_param.output_path, HWC_files.at(*it), glob_param.output_files, ".gz");
                    glob_param.pe_MPI_files_in_rank_out[*it] = mpi_out_name;
                }
            }
        }
        Mat mat(glob_param, model_id_storage);
        if (0 == glob_param.comm_rank) {
            std::cout << "Start processing." << std::endl;
        }
        mat.process();
        if (0 == glob_param.comm_rank) {
            std::cout << "End processing." << std::endl;
        }
        mat.write_trace_info(trace_info_file);
    }

#ifdef PEAT_MPI
    MPI_Barrier(MPI_COMM_WORLD);
    MPI_Finalize();
#endif

    return 0;
}
