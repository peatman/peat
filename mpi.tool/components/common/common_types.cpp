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

#include <algorithm>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>

#include <cstdio>

#include "assert.h"
#include "common_types.h"
#include "math.h"
#include "model_id.h"

#define PEATCERR \
    if (verbose) \
    std::cerr
#define PEATDBG(x)    \
    if (verbose >= x) \
    std::cerr
static const char *protocol[]{
    "Reliable", "Unreliable"};
static const char *onoff[]{
    "Off", "On"};
static const char *engines[]{
    "MPI Blocked", "MPI All", "Full"};
void Global_Parameters::check()
{
    if (matching_only) {
        if (target_freq != 0) {
            fprintf(stderr, "--matching-only is specified together with target frequency. Target frequency is ignored\n");
            target_freq = 0;
        }
        if (target_ppn != 0) {
            fprintf(stderr, "--matching-only is specified together with target PPN. Target PPN is ignored\n");
            target_ppn = 0;
        }
        if (target_numa_pp != 0) {
            fprintf(stderr, "--matching-only is specified together with target Numa PPN. Target Numa PPN is ignored\n");
            target_numa_pp = 0;
        }
        if (user_regions_CPI_ratio != 1.0) {
            fprintf(stderr, "--matching-only is specified together with User Regions Scale. User Regions Scale is ignored\n");
            user_regions_CPI_ratio = 1.0;
        }
        if (MPI_CPI_ratio != 1.0) {
            fprintf(stderr, "--matching-only is specified together with MPI Regions Scale. MPI Regions Scale is ignored\n");
            MPI_CPI_ratio = 1.0;
        }
    }
}

void Global_Parameters::print()
{
    printf("Global Parameters used:\n");
    printf("Trace path %s.\n", input_path.c_str());
    printf("Output path %s.\n", output_path.c_str());
    printf("User Regions File: %s.\n", user_region_file.c_str());
    printf("PEAT_Transport File: %s.\n", transport_file.c_str());
    printf("Trace Files: %s<%s|%s|%s>.<rank>[.gz] %s%s.<rank>[.gz], %s%s[.gz].\n", trace_files.c_str(), mpi_files.c_str(), hwc_files.c_str(), fab_files.c_str(), trace_files.c_str(), com_ext.c_str(), trace_files.c_str(), app_ext.c_str());
    if (mpi_time_file.length() > 0)
        printf("MPI Time File: %s.\n", mpi_time_file.c_str());
    if (map_file.length() > 0)
        printf("Map File: %s.\n", map_file.c_str());
    if (output_files.length() > 0)
        printf("Output Files : %s<%s|%s>.<rank>%s[.gz].\n", trace_files.c_str(), mpi_files.c_str(), hwc_files.c_str(), output_files.c_str());
    if (bind_files.length() > 0)
        printf("Bind Files: %s%s.<rank>[.gz].\n", trace_files.c_str(), bind_files.c_str());
    if (functions_split_file.length() > 0)
        printf("Function Splitting Template File: %s.\n", functions_split_file.c_str());
    printf("Protocol: %s.\n", protocol[use_up]);
    printf("Parallel Progress Engine: %s.\n", engines[progress_engine]);
    if (target_freq == 0) {
        printf("Target CPU Frequency: as input.\n");
    } else {
        printf("Target CPU Frequency: %lld.\n", target_freq);
    }
    printf("Eager Thresholds: intranode %lld, Internode:%lld\n", smp_eager_threshold, nic_eager_threshold);
    if (target_ppn == 0) {
        printf("Target PPN: as input,");
    } else {
        printf("Target PPN: %d,", target_ppn);
    }
    if (target_numa_pp == 0) {
        printf("Numa PPN: as input.\n");
    } else {
        printf("Numa PPN: %d.\n", target_numa_pp);
    }
    printf("Target User Regions CPI Ratio: %lf\n", user_regions_CPI_ratio);
    printf("Target MPI CPI Ratio: %lf.\n", MPI_CPI_ratio);
    if (matching_only) {
        printf("Matching only mode.\n");
    }
    printf("\n\n");
}
void User_Region::add(const Elems &el)
{
    Elems t_el = el;
    t_el.cpi = (PEAT_Ratio)t_el.clocks / (PEAT_Ratio)t_el.instr;
    elements.push_back(t_el);
}

void User_Region::process(PEAT_Ratio window)
{
    num = elements.size();
    std::sort(elements.begin(), elements.end());
    PEAT_LNum first_idx = (PEAT_LNum)((PEAT_Ratio)num * window / 2);
    PEAT_LNum last_idx = num - first_idx;
    PEAT_LNum num_q = last_idx - first_idx;
    PEAT_LNum idx = 0;
    for (std::vector<Elems>::iterator el_it = elements.begin(); el_it != elements.end(); ++el_it, ++idx) {
        if (el_it->clocks < min_clock)
            min_clock = el_it->clocks;
        if (el_it->clocks > max_clock)
            max_clock = el_it->clocks;
        sum_clock += el_it->clocks;
        if (el_it->instr < min_instr)
            min_instr = el_it->instr;
        if (el_it->instr > max_instr)
            max_instr = el_it->instr;
        sum_instr += el_it->instr;
        if (el_it->cpi < min_CPI)
            min_CPI = el_it->cpi;
        if (el_it->cpi > max_CPI)
            max_CPI = el_it->cpi;
        sum_CPI += el_it->cpi;
        if (idx >= first_idx && idx < last_idx) {
            sum_m_CPI += el_it->cpi;
        }
    }

    median_clock = (PEAT_Ratio)sum_clock / (PEAT_Ratio)num;
    median_instr = (PEAT_Ratio)sum_instr / (PEAT_Ratio)num;
    median_CPI = sum_CPI / (PEAT_Ratio)num;
    median_m_CPI = sum_m_CPI / (PEAT_Ratio)num_q;
    true_median_CPI = (PEAT_Ratio)sum_clock / (PEAT_Ratio)sum_instr;
    idx = 0;
    for (std::vector<Elems>::iterator el_it = elements.begin(); el_it != elements.end(); ++el_it, ++idx) {
        u_st_dev_clock += (el_it->clocks - median_clock) * (el_it->clocks - median_clock);
        u_st_dev_instr += (el_it->instr - median_instr) * (el_it->instr - median_instr);
        u_st_dev_CPI += (el_it->cpi - median_CPI) * (el_it->cpi - median_CPI);
        true_u_st_dev_CPI += (el_it->cpi - true_median_CPI) * (el_it->cpi - true_median_CPI);
        if (idx >= first_idx && idx < last_idx) {
            u_st_dev_m_CPI += (el_it->cpi - median_m_CPI) * (el_it->cpi - median_m_CPI);
        }
    }
    u_st_dev_clock = sqrt(u_st_dev_clock / num);
    u_st_dev_instr = sqrt(u_st_dev_instr / num);
    u_st_dev_CPI = sqrt(u_st_dev_CPI / num);
    u_st_dev_m_CPI = sqrt(u_st_dev_m_CPI / num_q);
    true_u_st_dev_CPI = sqrt(true_u_st_dev_CPI / num);
}

#define USER_REGION_VALUE_WIDTH 18
#define ADDRESS_VALUE_WIDTH 12
#define ID_VALUE_WIDTH 6
#define PERCENTAGE_WIDTH 6
#define PATH_VALUE_WIDTH 256

// Address_File_Storage::Address_File_Storage(const std::string &path, const std::string &user_region_file_name, const std::string &file_name, const std::string &com_file_ext, const std::string &app_file_ext, const std::set <PEAT_Rank>& rank_set, int flags, const Model_ID* id)
//	:  num_addrs(-1), m_id(id)
Address_File_Storage::Address_File_Storage(const Global_Parameters &globals, int flags, const Model_ID *id)
    : num_addrs(-1), m_id(id)
{
    //	user_region_store = new Address_File_Storage(globals.input_path, globals.user_region_file, globals.trace_files, globals.com_ext, globals.app_ext, globals.rank_set, PEAT_FIO_FLAG_READ | PEAT_FIO_FLAG_LAZY | PEAT_FIO_FLAG_ICLOSE, &modelIdStorage);

    std::map<PEAT_Rank, std::string> user_region_file = get_file_names(globals.input_path, globals.trace_files, globals.com_ext, globals.rank_set, true);
    File_Storage_GZ com_ext(user_region_file, PEAT_FIO_FLAG_READ | PEAT_FIO_FLAG_LAZY | PEAT_FIO_FLAG_ICLOSE, NULL, globals.verbose);
    if (!com_ext.is_any_file_good()) {
        std::ostringstream errTxt;
        errTxt << "Trace is not usable. Can't open file " << globals.user_region_file + globals.com_ext;
        throw std::runtime_error(errTxt.str());
    }
    lines.clear();
    PEAT_Num app_rank_num = com_ext.get_number_of_files();
    // last_event_idx.resize(app_rank_num, 0);
    // last_thread_idx.resize(app_rank_num, 0);
    if (globals.rank_set.empty()) {
        for (PEAT_Num i = 0; i < app_rank_num; i++) {
            PEAT_Bool hasMoreLines = true;
            while (hasMoreLines) {
                hasMoreLines = com_ext.read_lines(lines, (PEAT_Rank)i, ':');
                parse(lines, (PEAT_Rank)i);
            }
        }
    } else {
        for (std::set<PEAT_Rank>::const_iterator rank_it = globals.rank_set.cbegin(); rank_it != globals.rank_set.cend(); ++rank_it) {
            PEAT_Bool hasMoreLines = true;
            while (hasMoreLines) {
                hasMoreLines = com_ext.read_lines(lines, *rank_it, ':');
                parse(lines, *rank_it);
            }
        }
    }
    {
        std::map<PEAT_Rank, std::string> file_names;
        file_names[0] = globals.input_path + DIR_DELIMITER + globals.trace_files + globals.app_ext;
        File_Storage_GZ app_File(file_names, PEAT_FIO_FLAG_READ | PEAT_FIO_FLAG_LAZY | PEAT_FIO_FLAG_ICLOSE, NULL, globals.verbose);
        if (!app_File.is_file_good(0)) {
            std::ostringstream tmp;
            tmp << "Can't open Application file  " << file_names[0];
            throw std::runtime_error(tmp.str());
        } else {
            PEAT_Bool hasMoreLines = true;
            while (hasMoreLines) {
                hasMoreLines = app_File.read_lines(lines, -1, ':');
                parse(lines, -1);
            }
        }
    }
    {
        if (flags & PEAT_FIO_FLAG_READ) {
            std::map<PEAT_Rank, std::string> file_names;
            if (globals.user_region_file.length() > 0) {
                file_names[0] = globals.input_path + DIR_DELIMITER + globals.user_region_file;
                UR_File = new File_Storage_GZ(file_names, flags, NULL, globals.verbose);
                if (!UR_File->is_file_good(0)) {
                    std::cerr << "Can't open User Regions file  " << file_names[0] << "\n";
                } else {
                    PEAT_Bool hasMoreLines = true;
                    while (hasMoreLines) {
                        hasMoreLines = UR_File->read_lines(lines, -1, ':');
                        parse(lines, -1);
                    }
                }
            } else {
                UR_File = NULL;
            }
        } else {
            std::map<PEAT_Rank, std::string> file_names = user_region_file;
            if (globals.user_region_file.length() > 0) {
                char t_buf[32];
                for (std::map<PEAT_Rank, std::string>::iterator file_it = file_names.begin(); file_it != file_names.end(); ++file_it) {
                    sprintf(t_buf, ".%d", file_it->first);
                    file_it->second = globals.input_path + DIR_DELIMITER + globals.user_region_file + t_buf;
                }
                UR_File = new File_Storage_GZ(file_names, flags, NULL, globals.verbose);
            } else {
                UR_File = NULL;
            }
        }
    }
}

void Address_File_Storage::parse(const String_Split &lines, const PEAT_Rank rank)
{
    if (lines.size() < 2) {
        return;
    }
    if (isdigit(*lines.at(0))) {
        PEAT_Record_Type event = PEAT_RT_Non_Existent;
        extract(event, lines.at(0));
        if (m_id)
            event = m_id->translate_record_type(event);
        switch (event) {
        case PEAT_RT_User_Regions:
            process_user_region_table(lines);
            break;
        case PEAT_RT_Modules_Files_Definition:
            process_modules_table(lines, rank);
            break;
        case PEAT_RT_User_Addresses:
            process_addresses_table(lines, rank);
            break;
        case PEAT_RT_Number_Of_Events: {
            PEAT_Clock last_event_id = 0;
            extract(last_event_id, lines.at(2));
            last_event_idx[rank] = last_event_id;
            PEAT_Thread last_thread_Id = 0;
            if (lines.size() > 3)
                extract(last_thread_Id, lines.at(3));
            last_thread_idx[rank] = last_thread_Id;
            if (lines.size() > 4)
                extract(last_event_id, lines.at(4));
            last_fab_event_idx[rank] = last_event_id;
            break;
        }
        case PEAT_RT_User_Regions_Total: {
            PEAT_Ratio weighted = 0.0;
            extract(weighted, lines.at(4));
            if (weighted != weighted) { // check for NaN
                weighted = 0.0;
            }
            std::cerr << "Tag is deprecated: " << event << " line " << lines << std::endl;
            // metricsWeighted[rank] = weighted;
            break;
        }
        case PEAT_RT_Var_Block_Info: {
            extract(num_addrs, lines.at(2));
        } break;

        default:
            // cerr << "Tag is ignored: " << event << " line " << line << endl;
            break;
        }
    }
}

void Address_File_Storage::put_records(const User_Region_Store &user_regions)
{
    if (user_regions.user_regions.empty())
        return;
    for (std::map<PEAT_Rank, std::map<User_Address_Key, User_Region>>::const_iterator rank_it = user_regions.user_regions.cbegin(); rank_it != user_regions.user_regions.cend(); ++rank_it) {
        if (rank_it->second.empty())
            continue;
        gzFile out = UR_File->get_file_ptr(rank_it->first);
        std::map<User_Address_Key, User_Region>::const_iterator ur_it = rank_it->second.cbegin();
        const User_Address_Key &addr = ur_it->first;
        const std::vector<PEAT_UID> &addrs = addr.addrs;
        // header
        const int sep_length = (int)((USER_REGION_VALUE_WIDTH + 1) * 17 +
                                     (ID_VALUE_WIDTH + 1) * (addrs.size() + 1) +
                                     (ADDRESS_VALUE_WIDTH + 1) * addrs.size() * 2 +
                                     (PERCENTAGE_WIDTH + 1) * 2);
        SEPARATORGZ(UR_File, rank_it->first, sep_length);
        UR_File->printf(0, "%d", PEAT_RT_Remark);
        UR_File->printf(0, ":%*s%*s", PRINT_PARAMETERS(ID_VALUE_WIDTH, " "));
        UR_File->printf(0, ":%*s%*s", PRINT_PARAMETERS(USER_REGION_VALUE_WIDTH, " "));
        UR_File->printf(0, ":%*s%*s", PRINT_PARAMETERS(USER_REGION_VALUE_WIDTH * 2 + 1, "Approximation quality"));
        UR_File->printf(0, ":%*s%*s", PRINT_PARAMETERS(USER_REGION_VALUE_WIDTH * 5 + PERCENTAGE_WIDTH + 5, "Intructions"));
        UR_File->printf(0, ":%*s%*s", PRINT_PARAMETERS(USER_REGION_VALUE_WIDTH * 5 + PERCENTAGE_WIDTH + 5, "Clockticks"));
        UR_File->printf(0, ":%*s%*s", PRINT_PARAMETERS(USER_REGION_VALUE_WIDTH * 4 + 3, "CPI"));
        UR_File->printf(0, ":%*s%*s", PRINT_PARAMETERS(ADDRESS_VALUE_WIDTH * addrs.size() + ID_VALUE_WIDTH * addrs.size() + addrs.size(), "Object id + offset"));
        UR_File->printf(0, ":%*s%*s", PRINT_PARAMETERS(ADDRESS_VALUE_WIDTH * addrs.size() + addrs.size() - 1, "Addresses"));
        UR_File->printf(0, "\n");

        SEPARATORGZ(UR_File, rank_it->first, sep_length);
        UR_File->printf(0, "%d", PEAT_RT_Remark);
        UR_File->printf(0, ":%*s%*s", PRINT_PARAMETERS(ID_VALUE_WIDTH, "ID"));
        UR_File->printf(0, ":%*s%*s", PRINT_PARAMETERS(USER_REGION_VALUE_WIDTH, "Count"));

        UR_File->printf(0, ":%*s%*s", PRINT_PARAMETERS(USER_REGION_VALUE_WIDTH, "MCPI"));
        UR_File->printf(0, ":%*s%*s", PRINT_PARAMETERS(USER_REGION_VALUE_WIDTH, "uStDev"));
        UR_File->printf(0, ":%*s%*s", PRINT_PARAMETERS(USER_REGION_VALUE_WIDTH, "Total"));
        UR_File->printf(0, ":%*s%*s", PRINT_PARAMETERS(PERCENTAGE_WIDTH, "%"));
        UR_File->printf(0, ":%*s%*s", PRINT_PARAMETERS(USER_REGION_VALUE_WIDTH, "Min"));
        UR_File->printf(0, ":%*s%*s", PRINT_PARAMETERS(USER_REGION_VALUE_WIDTH, "Max"));
        UR_File->printf(0, ":%*s%*s", PRINT_PARAMETERS(USER_REGION_VALUE_WIDTH, "Avg"));
        UR_File->printf(0, ":%*s%*s", PRINT_PARAMETERS(USER_REGION_VALUE_WIDTH, "uStDev"));
        UR_File->printf(0, ":%*s%*s", PRINT_PARAMETERS(USER_REGION_VALUE_WIDTH, "Total"));
        UR_File->printf(0, ":%*s%*s", PRINT_PARAMETERS(PERCENTAGE_WIDTH, "%"));
        UR_File->printf(0, ":%*s%*s", PRINT_PARAMETERS(USER_REGION_VALUE_WIDTH, "Min"));
        UR_File->printf(0, ":%*s%*s", PRINT_PARAMETERS(USER_REGION_VALUE_WIDTH, "Max"));
        UR_File->printf(0, ":%*s%*s", PRINT_PARAMETERS(USER_REGION_VALUE_WIDTH, "Avg"));
        UR_File->printf(0, ":%*s%*s", PRINT_PARAMETERS(USER_REGION_VALUE_WIDTH, "uStDev"));
        UR_File->printf(0, ":%*s%*s", PRINT_PARAMETERS(USER_REGION_VALUE_WIDTH, "Min"));
        UR_File->printf(0, ":%*s%*s", PRINT_PARAMETERS(USER_REGION_VALUE_WIDTH, "Max"));
        UR_File->printf(0, ":%*s%*s", PRINT_PARAMETERS(USER_REGION_VALUE_WIDTH, "Avg"));
        UR_File->printf(0, ":%*s%*s", PRINT_PARAMETERS(USER_REGION_VALUE_WIDTH, "uStDev"));
        int num = 0;
        for (std::vector<PEAT_UID>::const_iterator addr_it = addrs.cbegin(); addr_it != addrs.cend(); ++addr_it, ++num) {
            UR_File->printf(0, ":%*s%*s", PRINT_PARAMETERS(ADDRESS_VALUE_WIDTH, "Offset"));
            UR_File->printf(0, ":%*s%*s", PRINT_PARAMETERS(ID_VALUE_WIDTH, "ID"));
        }
        num = 0;
        for (std::vector<PEAT_UID>::const_iterator addr_it = addrs.cbegin(); addr_it != addrs.cend(); ++addr_it) {
            char t_buf[1024];
            sprintf(t_buf, "%d", num++);
            UR_File->printf(0, ":%*s%*s", PRINT_PARAMETERS(ADDRESS_VALUE_WIDTH, t_buf));
        }
        UR_File->printf(0, "\n");
        SEPARATORGZ(UR_File, rank_it->first, sep_length);
        // data
        for (long rid = 0; ur_it != rank_it->second.cend(); ++ur_it, rid++) {
            const User_Address_Key &key = ur_it->first;
            const User_Region &ur = ur_it->second;
            // ur.process();
            const std::vector<PEAT_UID> &addrs = key.addrs;
            UR_File->printf(0, "%d", PEAT_RT_User_Regions);
            UR_File->printf(0, ":%*lld%", ID_VALUE_WIDTH, rid);
            UR_File->printf(0, ":%*lld", USER_REGION_VALUE_WIDTH, ur.num);
            UR_File->printf(0, ":%*.2lf", USER_REGION_VALUE_WIDTH, ur.median_m_CPI);
            UR_File->printf(0, ":%*.2lf", USER_REGION_VALUE_WIDTH, ur.u_st_dev_m_CPI);

            UR_File->printf(0, ":%*lld", USER_REGION_VALUE_WIDTH, ur.sum_instr);
            UR_File->printf(0, ":%*.2lf", PERCENTAGE_WIDTH, ur.sum_instr / user_regions.totals.at(rank_it->first).uc_instr);
            UR_File->printf(0, ":%*lld", USER_REGION_VALUE_WIDTH, ur.min_instr);
            UR_File->printf(0, ":%*lld", USER_REGION_VALUE_WIDTH, ur.max_instr);
            UR_File->printf(0, ":%*.2lf", USER_REGION_VALUE_WIDTH, ur.median_instr);
            UR_File->printf(0, ":%*.2lf", USER_REGION_VALUE_WIDTH, ur.u_st_dev_instr);

            UR_File->printf(0, ":%*lld", USER_REGION_VALUE_WIDTH, ur.sum_clock);
            UR_File->printf(0, ":%*.2lf", PERCENTAGE_WIDTH, ur.sum_clock / user_regions.totals.at(rank_it->first).uc_clock);
            UR_File->printf(0, ":%*lld", USER_REGION_VALUE_WIDTH, ur.min_clock);
            UR_File->printf(0, ":%*lld", USER_REGION_VALUE_WIDTH, ur.max_clock);
            UR_File->printf(0, ":%*.2lf", USER_REGION_VALUE_WIDTH, ur.median_clock);
            UR_File->printf(0, ":%*.2lf", USER_REGION_VALUE_WIDTH, ur.u_st_dev_clock);

            UR_File->printf(0, ":%*.2lf", USER_REGION_VALUE_WIDTH, ur.min_CPI);
            UR_File->printf(0, ":%*.2lf", USER_REGION_VALUE_WIDTH, ur.max_CPI);
            UR_File->printf(0, ":%*.2lf", USER_REGION_VALUE_WIDTH, ur.median_CPI);
            UR_File->printf(0, ":%*.2lf", USER_REGION_VALUE_WIDTH, ur.u_st_dev_CPI);
            for (std::vector<PEAT_UID>::const_iterator addr_it = addrs.cbegin(); addr_it != addrs.cend(); ++addr_it) {
                User_Address_Info *addr_info = get_addr_by_id(rank_it->first, *addr_it);
                UR_File->printf(0, ":%*llx", ADDRESS_VALUE_WIDTH, addr_info->addr_offset);
                UR_File->printf(0, ":%*lld", ID_VALUE_WIDTH, addr_info->obj_id);
            }
            for (std::vector<PEAT_UID>::const_iterator addr_it = addrs.cbegin(); addr_it != addrs.cend(); ++addr_it) {
                const User_Address_Info *ua_info = get_addr_by_id(rank_it->first, *addr_it);
                UR_File->printf(0, ":%*llx", ADDRESS_VALUE_WIDTH, ua_info->addr);
            }

            UR_File->printf(0, "\n");
        }
        SEPARATORGZ(UR_File, rank_it->first, sep_length);
        // 0: ID:      Base:      Load:      End:    Virtual:    Physical: Offset:  File:  Mem: Align:      Type: Flags:Object path
        const int sep_length2 = (int)((ID_VALUE_WIDTH + 1) +
                                      (ADDRESS_VALUE_WIDTH + 1) * 3 +
                                      (PATH_VALUE_WIDTH + 1));
        SEPARATORGZ(UR_File, rank_it->first, sep_length2);
        UR_File->printf(0, "%d", PEAT_RT_Remark);
        UR_File->printf(0, ":%*s%*s", PRINT_PARAMETERS(ID_VALUE_WIDTH, "ID"));
        UR_File->printf(0, ":%*s%*s", PRINT_PARAMETERS(ADDRESS_VALUE_WIDTH, "Base"));
        UR_File->printf(0, ":%*s%*s", PRINT_PARAMETERS(ADDRESS_VALUE_WIDTH, "Load"));
        UR_File->printf(0, ":%*s%*s", PRINT_PARAMETERS(ADDRESS_VALUE_WIDTH, "End"));
        UR_File->printf(0, ":%*s%*s", PRINT_PARAMETERS(PATH_VALUE_WIDTH, "Path"));
        UR_File->printf(0, "\n");
        SEPARATORGZ(UR_File, rank_it->first, sep_length2);

        for (std::map<PEAT_UID, Object_File_Definition>::const_iterator obj_it = modules.at(rank_it->first).cbegin(); obj_it != modules.at(rank_it->first).cend(); ++obj_it) {
            UR_File->printf(0, "%d", PEAT_RT_Modules_Files_Definition);
            UR_File->printf(0, ":%*lld", ID_VALUE_WIDTH, obj_it->second.obj_id);
            UR_File->printf(0, ":%*llx", ADDRESS_VALUE_WIDTH, obj_it->second.base_addr);
            UR_File->printf(0, ":%*llx", ADDRESS_VALUE_WIDTH, obj_it->second.load_addr);
            UR_File->printf(0, ":%*llx", ADDRESS_VALUE_WIDTH, obj_it->second.end_addr);
            UR_File->printf(0, ":%*s%*s", PRINT_PARAMETERS(PATH_VALUE_WIDTH, obj_it->second.full_path.c_str()));
            UR_File->printf(0, "\n");
        }
        SEPARATORGZ(UR_File, rank_it->first, sep_length2);
    }
}
void Address_File_Storage::put_string(const char *str, PEAT_Rank rank)
{
    UR_File->puts(rank, str);
    std::cerr << "Rank " << rank << std::endl
              << str << std::endl;
}

gzFile Address_File_Storage::get_file(PEAT_Rank rank)
{
    return UR_File->get_file_ptr(rank);
}

void Address_File_Storage::get_records()
{
}
PEAT_UID Address_File_Storage::get_num_events(const std::vector<PEAT_Rank> &ranks)
{
    PEAT_UID num_events = 0;
    if (ranks.empty()) {
        for (std::map<PEAT_Rank, PEAT_UID>::const_iterator idx_id = last_event_idx.cbegin(); idx_id != last_event_idx.cend(); ++idx_id) {
            num_events += idx_id->second;
        }
    } else {
        for (std::vector<PEAT_Rank>::const_iterator idx_id = ranks.cbegin(); idx_id != ranks.cend(); ++idx_id) {
            num_events += last_event_idx.at(*idx_id);
        }
    }

    return num_events;
}
PEAT_Bool Address_File_Storage::get_length(const PEAT_Rank rank, const User_Address_Key &key, PEAT_Clock &retLength, PEAT_Ratio &retRatio, const PEAT_UID &idxStart, const PEAT_UID &idxStop, const int subDirStat)
{

    return false;
}

void Address_File_Storage::put_value(const PEAT_Rank rank, const User_Address_Key &key, const PEAT_Clock &len, const PEAT_UID &altLIdx)
{
}

void Address_File_Storage::process_user_region_table(const String_Split &line)
{
    // format PEAT_RT_User_Regions:rank:
    PEAT_Rank rank = -1;
    PEAT_Ratio ratio = 1.0;
    std::vector<PEAT_UID> addrs;
    std::vector<PEAT_UID> obj_ids;
    extract(rank, line.at(1));
    extract(ratio, line.at(2));
    addrs.reserve(num_addrs);
    for (PEAT_Num i = 0; i < num_addrs; i++) {
        PEAT_UID addr;
        extract(addr, line.at(3 + i * 2), 16);
        addrs.push_back(addr);
    }
    User_Address_Key addrKeyValue(addrs); // key with address Values
    mAddresses[rank][addrKeyValue] = ratio;
}
void Address_File_Storage::process_modules_table(const String_Split &line, const PEAT_Rank rank)
{
    Object_File_Definition obj;
    PEAT_UID obj_id;
    extract(obj_id, line.at(1));
    obj.full_path = line.at(2);
    extract(obj.base_addr, line.at(3), 16);
    extract(obj.load_addr, line.at(4), 16);
    if (line.size() > 5) {
        extract(obj.end_addr, line.at(5), 16);
    } else {
        obj.end_addr = (PEAT_Addr)LLONG_MAX;
    }
    obj.obj_id = obj_id;
    modules[rank][obj_id] = obj;
}

void Address_File_Storage::process_addresses_table(const String_Split &line, const PEAT_Rank rank)
{
    std::map<PEAT_UID, User_Address_Info> &rankAddr = appAddresses[rank];
    PEAT_UID addrKey = 0;
    extract(addrKey, line.at(1));
    User_Address_Info &addrInfo = rankAddr[addrKey];
    addrInfo.addr_id = addrKey;
    extract(addrInfo.addr, line.at(2), 16);
    addrInfo.full_path = line.at(3);
    addrInfo.func = line.at(4);
    addrInfo.source = line.at(5);
    extract(addrInfo.line, line.at(6));
    const Object_File_Definition *obj = get_object_by_name(rank, addrInfo.full_path);
    addrInfo.base_addr = obj->base_addr;
    addrInfo.addr_offset = (PEAT_LNum)addrInfo.addr - (PEAT_LNum)obj->load_addr;
    addrInfo.obj_id = obj->obj_id;
    // addrInfo.name = trim(line.at(6));
    // addrInfo.path = trim(line.at(7));
}
Object_File_Definition *Address_File_Storage::get_object_by_name(PEAT_Rank rank, std::string full_path)
{
    if (full_path.find("libmpi.so") != std::string::npos) { // special case for mpi
        for (std::map<PEAT_UID, Object_File_Definition>::iterator obj_id = modules.at(rank).begin(); obj_id != modules.at(rank).end(); obj_id++) {
            if (obj_id->second.full_path.find(full_path) != std::string::npos)
                return &obj_id->second;
        }
    } else {
        for (std::map<PEAT_UID, Object_File_Definition>::iterator obj_id = modules.at(rank).begin(); obj_id != modules.at(rank).end(); obj_id++) {
            if (obj_id->second.full_path == full_path)
                return &obj_id->second;
        }
    }
    return &empty_obj;
}
PEAT_UID Address_File_Storage::convert_obj_id_beetween_ranks(const PEAT_UID objId, const PEAT_Rank from, const PEAT_Rank to) const
{

    if (appAddresses.count(from) && appAddresses.count(to)) {
        std::string symbolName;
        const std::map<PEAT_UID, User_Address_Info> &fromRank = appAddresses.at(from);
        for (std::map<PEAT_UID, User_Address_Info>::const_iterator it = fromRank.begin(); it != fromRank.end(); ++it) {
            const User_Address_Info &val = it->second;
            if (val.obj_id == objId) {
                symbolName = val.full_path;
                break;
            }
        }
        // cerr << "DEBUG " << symbolName << endl;
        const std::map<PEAT_UID, User_Address_Info> &toRank = appAddresses.at(to);
        for (std::map<PEAT_UID, User_Address_Info>::const_iterator it = toRank.begin(); it != toRank.end(); ++it) {
            const User_Address_Info &val = it->second;
            if (val.full_path == symbolName) {
                return val.obj_id;
            }
        }
    }
    return -1;
}
User_Address_Info *Address_File_Storage::get_addr_by_id(const PEAT_Rank rank, const PEAT_UID addr_id)
{
    if (0 == addr_id) {
        return &empty_info;
    }
    std::map<PEAT_UID, User_Address_Info> &rankAddr = appAddresses.at(rank);
    for (std::map<PEAT_UID, User_Address_Info>::iterator it = rankAddr.begin(); it != rankAddr.end(); ++it) {
        if (it->second.addr_id == addr_id) {
            return &it->second;
        }
    }
    std::cerr << "Address_File_Storage::get_addr_id_by_addr_value Can't find coresponded address " << addr_id << "\n";
    return &empty_info;
}

PEAT_UID Address_File_Storage::get_obj_id_by_name(const std::string &str, const PEAT_Rank rank)
{
    PEAT_UID obj_id = -1;
    const std::map<PEAT_UID, User_Address_Info> &rankAddr = appAddresses.at(rank);
    for (std::map<PEAT_UID, User_Address_Info>::const_iterator it = rankAddr.begin(); it != rankAddr.end(); ++it) {
        if (it->second.full_path == str) {
            obj_id = it->second.obj_id;
            break;
        }
    }
    if ((PEAT_UID)-1 == obj_id) {
        std::cerr << "Address_File_Storage::get_addr_id_by_addr_value Can't find coresponded binary " << str << "\n";
    }
    return obj_id;
}

std::ostream &operator<<(std::ostream &dbg, const User_Address_Key &addr)
{
    std::string delim("|");
    for (std::vector<PEAT_UID>::const_iterator addr_it = addr.addrs.cbegin(); addr_it != addr.addrs.cend(); ++addr_it) {
        dbg << (*addr_it) << delim << delim;
        ;
    }
    return dbg;
}

std::ostream &operator<<(std::ostream &dbg, const User_Address_Info &addr)
{
    /*
    string delim("|");
    dbg << dec << addr.objectNameID << delim
        << hex << addr.addressOffset << delim
        << dec << addr.name;
    */
    return dbg;
}

void peat_throw_error(const char *str)
{
    fprintf(stderr, str);
}
void peat_warning(const char *str)
{
    fprintf(stderr, str);
}
