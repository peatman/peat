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

#include "transport.h"
#include "file_io.h"
#include "func_id.h"
#include "mpi_func_ids.h"
#include <cassert>
#include <iomanip>
#include <sstream>

#define PRTDBG(x)     \
    if (verbose >= x) \
    std::cerr
#define PCOUT                                        \
    if (globals.verbose && (0 == globals.comm_rank)) \
    std::cout
#define MEGABYTE (1000 * 1000)

// 1 sec = 2 700 000 000 TSC (if freq == 2.7GHz)
PEAT_Transport::PEAT_Transport(const Global_Parameters &glob, const PEAT_FREQ target_freq) : verbose(glob.verbose)
{
    std::string file_name;
    std::string file_dir;
    globals = glob;
    file_dir = globals.input_path;
    file_name = globals.transport_file;
    tgt_freq = target_freq;
    if (file_name.find(DIR_DELIMITER) != std::string::npos) {
        file_dir.erase();
    } else {
        file_dir += DIR_DELIMITER;
    }
    if (verbose) {
        printf("Reading Transport Profile from %s %s\n", file_dir.c_str(), file_name.c_str());
    }
    /*try*/ {
        read_trasport_profile(file_dir, file_name);
    } /*catch(const std::runtime_error &e) {
        std::ostringstream str;
        str << "Error while parsing communication profile file. Error: " << e.what() << "\n";
        std::logic_error(str.str());
    }*/
}

void PEAT_Transport::read_trasport_profile(const std::string &dir, const std::string &name)
{
    if (name.empty())
        return;
    File_Storage_GZ tst_file(dir + name, PEAT_FIO_FLAG_READ | PEAT_FIO_FLAG_LAZY | PEAT_FIO_FLAG_ICLOSE, NULL, verbose);
    String_Split lines;
    PEAT_Bool hasMoreLines = true;
    while (hasMoreLines) {
        hasMoreLines = tst_file.read_lines(lines, 0, ':');
        parse(lines, 0);
    }
    convert_profiles();
}
void PEAT_Transport::convert_profiles()
{
    PEAT_Ratio cpi_ratio = 1.0 / globals.MPI_CPI_ratio;
    for (int i = 0; i < PEAT_TRANSPORT_MAX; i++) {
        PEAT_Rank target_numa_pp = globals.target_numa_pp;
        PEAT_Ext_Trasport_Environment &cte_orig = te_orig[i];
        Trasport_Corection_Environment &cte_mod = te_mod[i];
        if (cte_orig.initialized) {
            PEAT_Ratio freq_ratio = (PEAT_Float)tgt_freq / (PEAT_Float)cte_orig.frequency;
            PEAT_Func_ID_Pair fid_pair;
            if (te_corr_orig[i].corrections.empty()) {
                cte_mod.corrections[fid_pair].resize(1);
                cte_mod.corrections[fid_pair][0].message_size = 0;
                cte_mod.corrections[fid_pair][0].general_bw = (PEAT_BW)cte_orig.general_bw / freq_ratio;
                cte_mod.corrections[fid_pair][0].general_lat = (PEAT_LAT)(cte_orig.general_lat * freq_ratio);
                assert(cte_mod.corrections[fid_pair][0].general_bw > 0);
                assert(cte_mod.corrections[fid_pair][0].general_lat > 0);
                cte_mod.corrections[fid_pair][0].right_ratio = cte_orig.right_ratio;
                cte_mod.corrections[fid_pair][0].left_ratio = cte_orig.left_ratio;
                cte_mod.corrections[fid_pair][0].right_ratio_num = cte_orig.right_ratio_num;
                cte_mod.corrections[fid_pair][0].right_ratio = cte_orig.right_ratio;

                cte_mod.corrections[fid_pair][0].cross_packet_delay = (PEAT_Float)(cte_orig.cross_packet_delay * freq_ratio);
                cte_mod.corrections[fid_pair][0].frequency = tgt_freq;
                cte_mod.corrections[fid_pair][0].min_pre_delay = (PEAT_LAT)(cte_orig.min_pre_delay * cpi_ratio);
                cte_mod.corrections[fid_pair][0].min_post_delay = (PEAT_LAT)(cte_orig.min_post_delay * cpi_ratio);
                cte_mod.corrections[fid_pair][0].avg_pre_delay = (PEAT_LAT)(cte_orig.avg_pre_delay * cpi_ratio);
                cte_mod.corrections[fid_pair][0].avg_post_delay = (PEAT_LAT)(cte_orig.avg_post_delay * cpi_ratio);
                cte_mod.corrections[fid_pair][0].median_pre_delay = (PEAT_LAT)(cte_orig.median_pre_delay * cpi_ratio);
                cte_mod.corrections[fid_pair][0].median_post_delay = (PEAT_LAT)(cte_orig.median_post_delay * cpi_ratio);
                cte_mod.corrections[fid_pair][0].max_pre_delay = (PEAT_LAT)(cte_orig.max_pre_delay * cpi_ratio);
                cte_mod.corrections[fid_pair][0].max_post_delay = (PEAT_LAT)(cte_orig.max_post_delay * cpi_ratio);
                cte_mod.corrections[fid_pair][0].t_id = cte_orig.t_id;
                cte_mod.corrections[fid_pair][0].initialized = true;
            } else {
                for (std::map<PEAT_Func_ID_Pair, std::vector<PEAT_Ext_Trasport_Environment>>::const_iterator corr_func_it = te_corr_orig[i].corrections.cbegin(); corr_func_it != te_corr_orig[i].corrections.cend(); ++corr_func_it) {
                    cte_mod.corrections[corr_func_it->first].reserve(te_corr_orig[i].corrections.size());
                    for (std::vector<PEAT_Ext_Trasport_Environment>::const_iterator size_it = corr_func_it->second.cbegin(); size_it != corr_func_it->second.cend(); ++size_it) {
                        PEAT_Ext_Trasport_Environment tmp;
                        //             ratio bitween target profile frequency and correction frequency
                        PEAT_Ratio freq_s_ratio = (PEAT_Float)tgt_freq / (PEAT_Float)size_it->frequency;
                        tmp.message_size = size_it->message_size;
                        tmp.general_bw = (PEAT_BW)cte_orig.general_bw * (size_it->general_bw) / freq_s_ratio;
                        tmp.general_lat = (PEAT_LAT)(cte_orig.general_lat * (size_it->mult_lat) * freq_s_ratio);
                        tmp.cross_packet_delay = (PEAT_Float)(cte_orig.cross_packet_delay * cpi_ratio);
                        tmp.min_pre_delay = (PEAT_LAT)(size_it->min_pre_delay * cpi_ratio);
                        tmp.min_post_delay = (PEAT_LAT)(size_it->min_post_delay * cpi_ratio);
                        tmp.avg_pre_delay = (PEAT_LAT)(size_it->avg_pre_delay * cpi_ratio);
                        tmp.avg_post_delay = (PEAT_LAT)(size_it->avg_post_delay * cpi_ratio);
                        tmp.median_pre_delay = (PEAT_LAT)(size_it->median_pre_delay * cpi_ratio);
                        tmp.median_post_delay = (PEAT_LAT)(size_it->median_post_delay * cpi_ratio);
                        tmp.max_pre_delay = (PEAT_LAT)(size_it->max_pre_delay * cpi_ratio);
                        tmp.max_post_delay = (PEAT_LAT)(size_it->max_post_delay * cpi_ratio);
                        tmp.left_ratio_num = cte_orig.left_ratio_num;
                        tmp.right_ratio_num = cte_orig.right_ratio_num;
                        tmp.left_ratio = cte_orig.left_ratio;
                        tmp.right_ratio = cte_orig.right_ratio;
                        tmp.frequency = tgt_freq;
                        tmp.t_id = cte_orig.t_id;
                        tmp.initialized = true;
                        assert(tmp.general_bw > 0);
                        assert(tmp.general_lat > 0);
                        cte_mod.corrections[corr_func_it->first].push_back(tmp);
                    }
                }
            }
        }
    }
}
PEAT_Trasport_Environment PEAT_Transport::cvt_env(const PEAT_Ext_Trasport_Environment &evt_in, PET_Transport_Selector ts)
{
    PEAT_Trasport_Environment evt_out = evt_in;
    switch (ts) {
    case PET_Transport_Selector_MIN:
        evt_out.pre_delay = evt_in.min_pre_delay;
        evt_out.post_delay = evt_in.min_post_delay;
        break;
    case PET_Transport_Selector_AVG:
        evt_out.pre_delay = evt_in.avg_pre_delay;
        evt_out.post_delay = evt_in.avg_post_delay;
        break;
    case PET_Transport_Selector_MEDIAN:
        evt_out.pre_delay = evt_in.median_pre_delay;
        evt_out.post_delay = evt_in.median_post_delay;
        break;
    case PET_Transport_Selector_MAX:
        evt_out.pre_delay = evt_in.max_pre_delay;
        evt_out.post_delay = evt_in.max_post_delay;
        break;
    }
    return evt_out;
}

PEAT_Trasport_Environment PEAT_Transport::get_env(int which, PEAT_MPI_Func_ID func_id, PEAT_MPI_Func_ID parent_func_id, PEAT_Message_Size message_size, PEAT_Num numa_ppn, PET_Transport_Selector ts)
{
    PEAT_Func_ID_Pair fid_pair(func_id, parent_func_id);
    if (te_mod[which].corrections.count(fid_pair) < 1) {
        fid_pair.parent_id = fid_pair.id;
        if (te_mod[which].corrections.count(fid_pair) < 1) {
            fid_pair.parent_id = TMPI_FUNC_ID_NONE;
            if (te_mod[which].corrections.count(fid_pair) < 1) {
                fid_pair.id = TMPI_FUNC_ID_NONE;
            }
            fid_pair.id = TMPI_FUNC_ID_NONE;
        }
    }
    if (te_mod[which].corrections.count(fid_pair) < 1)
        return cvt_env(te_mod[which].corrections.at(fid_pair).at(0), ts);

    if (te_mod[which].corrections.at(fid_pair).size() <= 2)
        return cvt_env(te_mod[which].corrections.at(fid_pair).at(0), ts);
    std::vector<PEAT_Ext_Trasport_Environment> &working_list = te_mod[which].corrections.at(fid_pair);
    if (message_size < 0) { // rts, cts and ack are handled separatelly
        return cvt_env(working_list.at(working_list.size() - 1), ts);
    }
    PEAT_Signed_Num start = 0, stop = (PEAT_Signed_Num)working_list.size() - 2; // size-1 is special for rts/cts/ack
    if (working_list.at(stop).message_size < message_size) {
        start = stop;
    } else if (working_list.at(start).message_size > message_size) {
        stop = start;
    }
    while (stop - start > 1) {
        PEAT_Signed_Num index = (start + stop) / 2;
        if (working_list.at(index).message_size > message_size) {
            stop = index;
            if (stop <= start) {
                stop = start;
            }
            continue;
        }
        if (working_list.at(index).message_size < message_size) {
            start = index;
            if (start >= stop) {
                start = stop;
            }
            continue;
        }
        start = stop = index;
    };
    // here we have stop=start+k(k=0 or 1)
    // let's interpolate
    PEAT_Ext_Trasport_Environment rt = working_list.at(start);
    PEAT_Ext_Trasport_Environment rp = working_list.at(stop);
    PEAT_Message_Size distance = rp.message_size - rt.message_size;
    if (distance > 0) {
        PEAT_Ratio r = 1.0 - ((PEAT_Ratio)message_size - (PEAT_Ratio)rt.message_size) / (PEAT_Ratio)distance;
        rt *= r;
        rp *= (1.0 - r);
        rt += rp;
    }
    switch (ts) {
    case PET_Transport_Selector_MIN:
        rt.pre_delay = rt.min_pre_delay;
        rt.post_delay = rt.min_post_delay;
        break;
    case PET_Transport_Selector_AVG:
        rt.pre_delay = rt.avg_pre_delay;
        rt.post_delay = rt.avg_post_delay;
        break;
    case PET_Transport_Selector_MEDIAN:
        rt.pre_delay = rt.median_pre_delay;
        rt.post_delay = rt.median_post_delay;
        break;
    case PET_Transport_Selector_MAX:
        rt.pre_delay = rt.max_pre_delay;
        rt.post_delay = rt.max_post_delay;
        break;
    }
    PEAT_Ratio rank_ratio = (PEAT_Ratio)(numa_ppn - rt.left_ratio_num) / ((PEAT_Ratio)rt.left_ratio_num + (PEAT_Ratio)rt.right_ratio_num);
    rank_ratio = rt.left_ratio * (1 - rank_ratio) + rt.right_ratio * rank_ratio;
    rt.general_bw *= rank_ratio;
    return rt;
}

void PEAT_Transport::parse(const String_Split &line, PEAT_Rank rank)
{
    if (line.size() < 2) {
        return;
    }
    PEAT_Record_Type event = PEAT_RT_Remark;
    extract(event, line.at(0));
    switch (event) {
    case PEAT_RT_TRANSPORT_Vars:
        // PEAT_RT_TRANSPORT_Vars:id:freq:bw(bytes/clock):latency:pre-delay:cross-packet-delay:post-delay
        {
            int id = -1;
            extract(id, line.at(1));
            PEAT_Ext_Trasport_Environment tmp;
            PEAT_FREQ bwlat_frequency;
            extract(tmp.is_parallel, line.at(2));
            extract(tmp.packet_size, line.at(3));
            extract(tmp.unit_size, line.at(4));
            extract(tmp.chain_num, line.at(5));
            extract(tmp.frequency, line.at(6));
            extract(bwlat_frequency, line.at(7));
            PEAT_Ratio r = (PEAT_Ratio)bwlat_frequency / (PEAT_Ratio)tmp.frequency;
            extract(tmp.general_bw, line.at(8));
            extract(tmp.general_lat, line.at(9));
            tmp.general_bw = (PEAT_BW)(tmp.general_bw * r);
            tmp.general_lat = (PEAT_LAT)(tmp.general_lat * r);
            extract(tmp.left_ratio_num, line.at(10));
            extract(tmp.left_ratio, line.at(11));
            extract(tmp.right_ratio_num, line.at(12));
            extract(tmp.right_ratio, line.at(13));

            extract(tmp.cross_packet_delay, line.at(14));
            extract(tmp.min_pre_delay, line.at(15));
            extract(tmp.min_post_delay, line.at(16));
            extract(tmp.avg_pre_delay, line.at(17));
            extract(tmp.avg_post_delay, line.at(18));
            extract(tmp.median_pre_delay, line.at(19));
            extract(tmp.median_post_delay, line.at(20));
            extract(tmp.max_pre_delay, line.at(21));
            extract(tmp.max_post_delay, line.at(22));
            tmp.initialized = true;
            tmp.t_id = (PEAT_Transports)id;
            te_orig[id] = tmp;
        }
        break;
    case PEAT_RT_TRANSPORT_Correction_Vars:
        // PEAT_RT_TRANSPORT_Vars:id:freq:bw(bytes/clock):latency:pre-delay:cross-packet-delay:post-delay
        {
            int id = -1;
            extract(id, line.at(1));
            PEAT_Signed_Num line_size = line.size();
            PEAT_Ext_Trasport_Environment tmp;
            extract(tmp.frequency, line.at(2));
            PEAT_MPI_Func_ID func_id, parent_func_id;
            extract(func_id, line.at(3), 10, false);
            extract(parent_func_id, line.at(4), 10, false);
            if (func_id == 0) {
                func_id = global_func_id.idx_by_name(line.at(3));
            }
            if (parent_func_id == 0) {
                parent_func_id = global_func_id.idx_by_name(line.at(4));
            }
            PEAT_Func_ID_Pair fid_pair(func_id, parent_func_id);
            for (PEAT_Signed_Num i = 5; i < line_size; i += 11) {
                extract(tmp.message_size, line.at(i));
                extract(tmp.general_bw, line.at(i + 1));
                extract(tmp.mult_lat, line.at(i + 2));
                extract(tmp.min_pre_delay, line.at(i + 3));
                extract(tmp.min_post_delay, line.at(i + 4));
                extract(tmp.avg_pre_delay, line.at(i + 5));
                extract(tmp.avg_post_delay, line.at(i + 6));
                extract(tmp.median_pre_delay, line.at(i + 7));
                extract(tmp.median_post_delay, line.at(i + 8));
                extract(tmp.max_pre_delay, line.at(i + 9));
                extract(tmp.max_post_delay, line.at(i + 10));
                te_corr_orig[id].corrections[fid_pair].push_back(tmp);
            }
        }
        break;
    case PEAT_RT_Func_Dev: {
    } break;
    default:
        PRTDBG(2) << "PEAT_Transport::parse. Tag is ignored: " << event << " line " << line << std::endl;
        break;
    }
    return;
}

void PEAT_Transport::write_func_profile(const std::string &dir, const std::string &name) const
{
    std::string full_name = dir + name;
    std::map<PEAT_Rank, std::string> out;
    out[0] = full_name;
    File_Storage_GZ out_file(out, PEAT_FIO_FLAG_WRITE);
    for (std::map<PEAT_Num, std::map<PEAT_MPI_Func_ID, Function_Environment>>::const_iterator mode_it = func_env.cbegin(); mode_it != func_env.cend(); ++mode_it) {
        for (std::map<PEAT_MPI_Func_ID, Function_Environment>::const_iterator func_env_it = mode_it->second.cbegin(); func_env_it != mode_it->second.cend(); ++func_env_it) {
            char out_line[1024];
            sprintf(out_line, "%d:%d:%d:%s:%lld:%lld:%lld:%lld:%lld:%lld\n", PEAT_RT_Func_Dev, (int)mode_it->first, func_env_it->first,
                    global_func_id.name_by_idx(func_env_it->first),
                    func_env_it->second.entry.min, func_env_it->second.entry.max, func_env_it->second.entry.avg,
                    func_env_it->second.exit.min, func_env_it->second.exit.max, func_env_it->second.exit.avg);
            std::string line = out_line;
            out_file.puts(0, line);
        }
    }
}

PEAT_Transport::~PEAT_Transport()
{
    std::string file_name;
    std::string file_dir;
    file_dir = globals.input_path;
    file_name = globals.transport_file;

    std::string::size_type pos = file_name.rfind(DIR_DELIMITER);
    if (pos != std::string::npos) {
        file_name.erase(0, pos);
    } else {
        file_dir += DIR_DELIMITER;
    }
    write_func_profile(file_dir, file_name + ".stat");
}
