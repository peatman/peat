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

#include <iostream>

#include "getopt.h"
#include <time.h>

#include "pet_core.h"
#include "utils.h"

#ifdef PEAT_MPI
#include <mpi.h>
#endif

#define TRACEFILEMASK "tractor_t"
#define ADDRFILE "tractor_t.urf"
#define EAGERTHREASHOLD 262144 // 256k bytes

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

    size_t found_ext = str_i.find(ext);
    if (found_ext != std::string::npos && ext.length() > 0) {
        str_out += str_i.substr(0, found_ext);
        str_out += out_file + ext;
    } else {
        str_out += str_i + out_file;
    }
    return str_out;
}
int parse_config_file(const char *file, Global_Parameters &glob_param, const char *short_options, const struct option *long_options);

int parse_strings(int argc, char *const *argv, Global_Parameters &glob_param, const char *short_options, const struct option *long_options)
{
    int res = -1, option_index = 0;
    std::string config_file;
    do {
        res = getopt_long(argc, argv, short_options, long_options, &option_index);
        switch (res) {
        case 0:
            if (0 == strcasecmp(long_options[option_index].name, "target-cpu-freq")) {
                if (glob_param.target_freq == 0) {
                    extract(glob_param.target_freq, optarg);
                } else {
                    std::cerr << "Target CPU Frequency is already set. Ignored.\n";
                }
                break;
            }
            if (0 == strcasecmp(long_options[option_index].name, "onetime-pass-per-rank")) {
                extract(glob_param.onetime_pass_per_rank, optarg);
                break;
            }
            if (0 == strcasecmp(long_options[option_index].name, "read-ahead-ratio")) {
                extract(glob_param.read_ahead_ratio, optarg);
                break;
            }
            if (0 == strcasecmp(long_options[option_index].name, "target-ppn")) {
                char *endptr = NULL;
                if (glob_param.target_ppn == 0) {
                    extract(glob_param.target_ppn, optarg, &endptr);
                } else {
                    PEAT_Rank target_ppn;
                    extract(target_ppn, optarg, &endptr);
                    std::cerr << "Target PPN is already set. Ignored.\n";
                }
                if (glob_param.target_numa_pp == 0) {
                    if (endptr && strlen(endptr) > 0)
                        extract(glob_param.target_numa_pp, endptr + 1, &endptr);
                } else {
                    std::cerr << "Target Numa PPN is already set. Ignored.\n";
                }
                break;
            }
            if (0 == strcasecmp(long_options[option_index].name, "pcontrol")) {
                glob_param.pcontrol_mode = 1;
                if (optarg) {
                    extract(glob_param.pcontrol_mode_rank, optarg);
                }
                break;
            }
            if (0 == strcasecmp(long_options[option_index].name, "progress-engine")) {
                if (optarg) {
                    extract(glob_param.progress_engine, optarg);
                }
                break;
            }

            break;
        case 'i':
            glob_param.trace_files = optarg;
            break;
        case 'w':
            glob_param.input_path = optarg;
            if (!glob_param.input_path.empty()) {
                if (*glob_param.input_path.rbegin() != DIR_DELIMITER) {
                    glob_param.input_path.push_back(DIR_DELIMITER);
                }
            }
            break;
        case 'o':
            glob_param.output_path = optarg;
            if (!glob_param.output_path.empty()) {
                if (*glob_param.output_path.rbegin() != DIR_DELIMITER) {
                    glob_param.output_path.push_back(DIR_DELIMITER);
                }
            }
            break;
        case 't':
            glob_param.transport_file = optarg;
            break;
        case 'u':
            glob_param.user_region_file = optarg;
            break;
        case 'g':
            glob_param.gather_transport_stat = 1;
            break;
        case 's':
            if (strcasecmp(optarg, "min") == 0) {
                glob_param.transport_selector = PET_Transport_Selector_MIN;
            }
            if (strcasecmp(optarg, "max") == 0) {
                glob_param.transport_selector = PET_Transport_Selector_MAX;
            }
            if (strcasecmp(optarg, "avg") == 0) {
                glob_param.transport_selector = PET_Transport_Selector_AVG;
            }
            if (strcasecmp(optarg, "median") == 0) {
                glob_param.transport_selector = PET_Transport_Selector_MEDIAN;
            }
            break;
        case 'p':
            if (strcasecmp(optarg, "prolog") == 0) {
                glob_param.part_receiver = PET_Part_Receiver_PROLOG;
            }
            if (strcasecmp(optarg, "meat") == 0) {
                glob_param.part_receiver = PET_Part_Receiver_MEAT;
            }
            if (strcasecmp(optarg, "epilog") == 0) {
                glob_param.part_receiver = PET_Part_Receiver_EPILOG;
            }
            break;
        case 'f':
            glob_param.functions_split_file = optarg;
            break;

        case 'c': {
            config_file = optarg;
        } break;
        case 'a': {
            char *endptr = NULL;
            extract(glob_param.user_regions_CPI_ratio, optarg, &endptr);
            if (endptr && strlen(endptr) > 0)
                extract(glob_param.MPI_CPI_ratio, endptr + 1, &endptr);
            if (glob_param.user_regions_CPI_ratio <= 0)
                glob_param.user_regions_CPI_ratio = 1;
            if (glob_param.MPI_CPI_ratio <= 0)
                glob_param.MPI_CPI_ratio = 1;
        } break;
        case 'r': {
            extract(glob_param.read_ahead_ratio, optarg);
            break;
        }
        case 'd': {
            extract(glob_param.onetime_pass_per_rank, optarg);
            break;
        }

        case 'e': {
            char *endptr = NULL;
            extract(glob_param.smp_eager_threshold, optarg, &endptr);
            if (endptr && strlen(endptr) > 0)
                extract(glob_param.nic_eager_threshold, endptr + 1, &endptr);
        } break;
        case 'v':
            glob_param.verbose = 1;
            break;
        case -1:
            break;
        default:
            std::cout << "unknown option " << res << std::endl;
            break;
        }
    } while (res != -1);
    /*
    if ( optind < argc ) {
        std::cout << "Unknown options:";
        while ( optind < argc ) {
            std::cout << " " << argv[optind++];
        }
        std::cout << std::endl;
    }
    */

    if (!config_file.empty()) {
        parse_config_file(config_file.c_str(), glob_param, short_options, long_options);
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
    return 0;
}
int parse_config_file(const char *file, Global_Parameters &glob_param, const char *short_options, const struct option *long_options)
{
    String_Split line;
    File_Storage_GZ *input_file = new File_Storage_GZ(file, PEAT_FIO_FLAG_READ | PEAT_FIO_FLAG_LAZY | PEAT_FIO_FLAG_ICLOSE, NULL, glob_param.verbose);
    // PEAT_Bool is_orig = false;
    if (input_file && input_file->is_any_file_good()) {
        input_file->read_lines(line, 0, ':');
        if (strcasecmp(line.at(0), "PETCFG") != 0) {
            printf("Config file %s is not in the required format\n", file);
            return 1;
        }
        while ((input_file->is_any_file_good())) {
            input_file->read_lines(line, 0, ' ');
            int argc = line.size();
            char *argv[128];
            argv[0] = (char *)file;
            for (int i = 1; i <= argc; i++) {
                argv[i] = (char *)line.at(i - 1);
            }
            optind = 0;
            if (parse_strings(argc + 1, (char *const *)argv, glob_param, short_options, long_options)) {
                return 1;
            }
        }
        delete input_file;
    } else {
        printf("Config file %s can't be opened\n", file);
        return 1;
    }
    return 0;
}
#ifndef MAX_PATH
#define MAX_PATH 512
#endif
int main(int argc, char **argv)
{
#ifdef PEAT_MPI
    MPI_Init(&argc, &argv);
#endif
    Global_Parameters glob_param;
#ifdef PEAT_MPI
    MPI_Comm_size(MPI_COMM_WORLD, &glob_param.comm_size);
    MPI_Comm_rank(MPI_COMM_WORLD, &glob_param.comm_rank);
#else
    glob_param.comm_size = 1;
    glob_param.comm_rank = 0;
#endif

    std::string final_string = "Error.";
    std::string tstring("Performance Estimation Tool (PET). Built date: ");
    tstring += __DATE__;
    time_t tbegin = time(0);
    int func_id_print = 0;
    // glob_param.eager_threshold = EAGERTHREASHOLD;
    glob_param.user_region_file = ADDRFILE;
    glob_param.com_ext = ".com";
    glob_param.output_files = ".out";
    glob_param.trace_files = TRACEFILEMASK;
    glob_param.ptpd_ext = ".ptp";
    glob_param.app_ext = ".app";
    glob_param.bind_files = ".bind";
    glob_param.mpi_files = ".mpi";
    glob_param.hwc_files = ".hwc";
    glob_param.transport_files = ".transport";
    std::string trace_info_file;
    glob_param.input_path = ".";
    glob_param.transport_file = "pet.transport";

    struct option long_options[] = {
        {"verbose", no_argument, &glob_param.verbose, 1},
        {"func-id-print", no_argument, &func_id_print, 1},
        {"model-output", no_argument, &glob_param.model_output, 1},
        {"gather-transport-stat", no_argument, &glob_param.gather_transport_stat, 1},
        {"progress-engine", required_argument, 0, 0},
        {"no-pe-split", required_argument, 0, 'p'},
        {"use-up", no_argument, &glob_param.use_up, 1},
        {"matching-only", no_argument, &glob_param.matching_only, 1},
        {"pcontrol", optional_argument, 0, 0},
        {"input", required_argument, 0, 'i'},
        {"transport-file", required_argument, 0, 't'},
        {"user-region-file", required_argument, 0, 'u'},
        {"transport-selection", required_argument, 0, 's'},
        {"config-file", required_argument, 0, 'c'},
        {"function-split-file", required_argument, 0, 'f'},
        {"CPI-scale", required_argument, 0, 'a'},
        {"trace-dir", required_argument, 0, 'w'},
        {"trace-out-dir", required_argument, 0, 'o'},
        {"eager-thresholds", required_argument, 0, 'e'},
        {"target-cpu-freq", required_argument, 0, 0},
        {"target-ppn", required_argument, 0, 0},
        {"read-ahead-ratio", required_argument, 0, 0},
        {"onetime-pass-per-rank", required_argument, 0, 0},
        {0, 0, 0, 0},
    };
    const char short_options[] = "gvi:a:w:e:t:f:s:c:u:r:d:p:";
    std::cout << tstring +
                     "\nDisclaimer\n"
                     "THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR IMPLIED\n"
                     "WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED WARRANTIES OF\n"
                     "MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.\n"
                     "Question/comments/bug reports please send to ... \n\n"
              << std::endl;

    if (parse_strings(argc, argv, glob_param, short_options, long_options))
        return 1;
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
        std::map<PEAT_Rank, std::string> HWC_files = get_file_names(glob_param.input_path, glob_param.trace_files, glob_param.hwc_files, true);
        std::map<PEAT_Rank, std::string> transport_files = get_file_names(glob_param.input_path, glob_param.trace_files, glob_param.transport_files, false);
        PEAT_Rank mpi_info_files_num = (PEAT_Rank)MPI_files.size();
        PEAT_Rank chunk = mpi_info_files_num / glob_param.comm_size;
        if (chunk < 1) {
            chunk = 1;
        }
        peat_mkdir(glob_param.output_path, glob_param.verbose);
        const PEAT_Rank model_rank_start = chunk * glob_param.comm_rank;
        const PEAT_Rank model_rank_end = (glob_param.comm_size - 1 == glob_param.comm_rank) ? mpi_info_files_num : chunk * glob_param.comm_rank + chunk;

        for (std::map<PEAT_Rank, std::string>::const_iterator it = MPI_files.begin(); it != MPI_files.end(); ++it) {
            if (it->first >= model_rank_start && it->first < model_rank_end) {
                // std::cout << it->first << " -> " << it->second << std::endl;
                glob_param.pe_MPI_files_in_rank[it->first] = it->second;
                glob_param.pe_MPI_files_in_rank_out[it->first] = make_out_file_name(glob_param.output_path, it->second, glob_param.output_files, ".gz");
                glob_param.pe_bind_files_in_rank_out[it->first] = glob_param.pe_MPI_files_in_rank_out[it->first];
                glob_param.pe_bind_files_in_rank_out[it->first].replace(glob_param.pe_MPI_files_in_rank_out[it->first].rfind(glob_param.mpi_files), glob_param.mpi_files.length(), glob_param.bind_files);
                // std::cout << "MPI_OUT: " << glob_param.pe_MPI_files_in_rank_out[ it->first ] << std::endl;
                // std::cout << "BIND_OUT: " << glob_param.pe_bind_files_in_rank_out[ it->first ] << std::endl;
            }
        }
        for (std::map<PEAT_Rank, std::string>::const_iterator it = transport_files.begin(); it != transport_files.end(); ++it) {
            if (it->first >= model_rank_start && it->first < model_rank_end) {
                glob_param.pe_transport_files_in_rank[it->first] = it->second;
                // std::cout << "TRANSPORT: " << glob_param.pe_transport_files_in_rank[ it->first ] << std::endl;
            }
        }
        for (std::map<PEAT_Rank, std::string>::const_iterator it = HWC_files.begin(); it != HWC_files.end(); ++it) {
            if (it->first >= model_rank_start && it->first < model_rank_end) {
                glob_param.pe_HWC_files_in_rank[it->first] = it->second;
                glob_param.pe_HWC_files_in_rank_out[it->first] = make_out_file_name(glob_param.output_path, it->second, glob_param.output_files, ".gz");
                // std::cout << "HWC: " << glob_param.pe_HWC_files_in_rank[ it->first ] << std::endl;
                // std::cout << "HWC_OUT: " << glob_param.pe_HWC_files_in_rank_out[ it->first ] << std::endl;
            }
        }
        trace_info_file = make_out_file_name(glob_param.output_path + DIR_DELIMITER, glob_param.trace_files, ".trace.info", "");

#ifdef PEAT_MPI
        int group = 0;
        for (PEAT_Rank i = 0; i < mpi_info_files_num; ++i) {
            if (group < (glob_param.comm_size - 1)) {
                group = i / chunk;
            }
        }
        //        ostringstream tmp;
        //        tmp << glob_param.comm_rank << ": Model files " << model_files_num << " chunk " << chunk << " files " << glob_param.pe_files_in_rank_pe << "\nModel2PE " << glob_param.pe_Model_2_PE << "\n";
        //        printf("%s", tmp.str().c_str());
        MPI_Comm_split(MPI_COMM_WORLD, !glob_param.pe_files_in_rank_pe.empty(), glob_param.comm_rank, &glob_param.pe_communicator);
        MPI_Comm_size(glob_param.pe_communicator, &glob_param.comm_size);
        MPI_Comm_rank(glob_param.pe_communicator, &glob_param.comm_rank);

        if (!glob_param.pe_files_in_rank_pe.empty()) {
            PE estimator(glob_param, model_id_storage);
            try {
                if (0 == glob_param.comm_rank) {
                    std::cout << "Start estimation." << endl;
                }
                estimator.estimate();
                if (0 == glob_param.comm_rank) {
                    std::cout << "End estimation." << endl;
                }
                estimator.collect_stat_data();
                estimator.collect_perevent_data();
                estimator.collectm_addresses_data();
                // estimator.collect_app_addresses_data();
                if (0 == glob_param.comm_rank) {
                    // cerr << p2p_msg;
                    double rate = estimator.analyze(glob_param.path + "0.profile");
                    cout << "Estimation done. Ratio (original/estimated): " << rate << endl;
                    //        if(verbose_flag){
                    //            cout << "Test: Example rate for 0 bytes is " << idelizer_msg.get_ideal_message_rate(0) << endl;
                    //            cout << "Test: Example rate for 4 bytes is " << idelizer_msg.get_ideal_message_rate(4) << endl;
                    //            cout << "Test: Example rate for 4194304 bytes is " << idelizer_msg.get_ideal_message_rate(4194304) << endl;
                    //            cout << "Test: Example rate for 1111048575 bytes is " << idelizer_msg.get_ideal_message_rate(1111048575) << endl;
                    //        }
                    final_string = "Finished.";
                }
            } catch (const PE::exit_controller_exception &exc) { // use it to call DTORs in the code
#if 1
                if (0 == glob_param.comm_rank) {
                    ostringstream main_rank_info;
                    estimator.print_elems_in_queue(main_rank_info);
                    main_rank_info << " Perf:" << estimator.get_perf_metric() << '\n';
                    // collect info from other ranks
                    map<PEAT_Rank, string> messages_from_world;
                    messages_from_world[glob_param.comm_rank] = main_rank_info.str();

                    for (PEAT_Rank i = 1; i < glob_param.comm_size; ++i) {
                        MPI_Status status;
                        PEAT_Message_Size msg_size = 0;
                        MPI_Recv(&msg_size, sizeof(PEAT_Message_Size), MPI_CHAR, i, pect_rank_events, glob_param.pe_communicator, &status);
                        char msg_buf[msg_size];
                        MPI_Recv(msg_buf, msg_size, MPI_CHAR, i, pect_rank_events, glob_param.pe_communicator, &status);
                        messages_from_world[i] = msg_buf;
                    }
                    cerr << glob_param.comm_rank << ": We have an Error in the estimator. Exception caught: " << exc.what() << "\n";
                    for (map<PEAT_Rank, string>::iterator it = messages_from_world.begin(); it != messages_from_world.end(); ++it) {
                        cerr << it->second;
                    }

                } else {
                    ostringstream msg_to_main_rank;
                    estimator.print_elems_in_queue(msg_to_main_rank);
                    msg_to_main_rank << " Perf:" << estimator.get_perf_metric() << '\n';
                    PEAT_Message_Size msg_size = msg_to_main_rank.str().size() + 1;
                    MPI_Send(&msg_size, sizeof(msg_size), MPI_CHAR, 0, pect_rank_events, glob_param.pe_communicator);
                    MPI_Send(msg_to_main_rank.str().c_str(), msg_size, MPI_CHAR, 0, pect_rank_events, glob_param.pe_communicator);
                }
#else
                ostringstream msg_to_main_rank;
                estimator.print_elems_in_queue(msg_to_main_rank);
                msg_to_main_rank << " Perf:" << estimator.get_perf_metric() << '\n';
                int msg_size = msg_to_main_rank.str().size() + 1;

                int recv_counts[glob_param.comm_rank];
                MPI_Gather(&msg_size, sizeof(int), MPI_CHAR, recv_counts, sizeof(int), MPI_CHAR, 0, glob_param.pe_communicator);
                if (0 == glob_param.comm_rank)
                    for (PEAT_Rank i = 0; i < glob_param.comm_size; ++i) {
                        cerr << i << "-" << recv_counts[i] << '\n';
                    }
                int displs[glob_param.comm_size];
                displs[0] = 0;
                PEAT_Num common_len = recv_counts[0];
                for (PEAT_Rank i = 1; i < glob_param.comm_size; ++i) {
                    common_len += recv_counts[i];
                    displs[i] = displs[i - 1] + recv_counts[i - 1];
                }
                char recv_buf_msg[common_len];
                cerr << "asdasd " << common_len << endl;
                MPI_Gatherv(msg_to_main_rank.str().c_str(), msg_size, MPI_CHAR, recv_buf_msg, recv_counts, displs, MPI_CHAR, 0, glob_param.pe_communicator);

                if (0 == glob_param.comm_rank) {
                    cerr << glob_param.comm_rank << ": We have an Error in the estimator. Exception caught: " << exc.what() << "\n";
                    for (PEAT_Rank i = 0; i < glob_param.comm_size; ++i) {
                        cerr << recv_buf_msg[displs[i]];
                    }
                }
#endif
            }
            if (0 == glob_param.comm_rank) {
                time_t tend = time(0) - tbegin;
                cout << "\n"
                     << final_string << " Processing took " << tend << " seconds" << endl;
            }
        }
#else
//        #define __LOG_TIMELINES__

#ifdef __LOG_TIMELINES__
        std::string full_ee_name = glob_param.output_path + DIR_DELIMITER + glob_param.trace_files + ".ee";
        std::string full_te_name = glob_param.output_path + DIR_DELIMITER + glob_param.trace_files + ".te";
        Estimation_Timeline::fp_debug = fopen(full_ee_name.c_str(), "w+t");
        TE_Timeline::fp_debug = fopen(full_te_name.c_str(), "w+t");
#endif
        PET_Core estimator(glob_param, model_id_storage);
        glob_param.check();
        if (0 == glob_param.comm_rank) {
            glob_param.print();
            printf("Estimation started.\n");
        }
        estimator.estimate();
#ifdef __LOG_TIMELINES__
        fclose(Estimation_Timeline::fp_debug);
        fclose(TE_Timeline::fp_debug);
#endif

        if (0 == glob_param.comm_rank) {
            printf("Estimation finished.\n");
        }
        double ratio = estimator.analyze(glob_param.input_path + DIR_DELIMITER + "0.profile");
        estimator.write_trace_info(trace_info_file);
        time_t tend = time(0) - tbegin;
        printf("Ratio( original / estimated ) : %.2lf.\nFinished.  Processing took %lld seconds.\n", ratio, tend);

#endif
    }
#ifdef PEAT_MPI
    MPI_Finalize();
#endif
    return 0;
}
