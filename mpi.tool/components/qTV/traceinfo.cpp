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

#include "traceinfo.h"
#include "../common/utils.h"
#include <QDir>

TraceInfo::TraceInfo() {}

QString TraceInfo::make_absolute_path(const QString &file_path, const QString &relative_dir)
{
    QFileInfo file_info(file_path);
    QDir base_dir = file_info.absoluteDir();
    return QDir::cleanPath(base_dir.absoluteFilePath(relative_dir));
}

bool TraceInfo::parse_trace_info_file(QFileInfo &tracefile)
{
    QFileInfo user_file_info(tracefile);
    if (!user_file_info.exists()) {
        return false;
    }
    trace_info_file = tracefile.absoluteFilePath();
    std::string filename = trace_info_file.toStdString();
    File_Storage_GZ trace(filename, PEAT_FIO_FLAG_READ | PEAT_FIO_FLAG_ICLOSE, NULL, false);
    String_Split lines;
    trace.read_lines(lines, 0, '|', ';');
    std::string key = lines.at(0);
    if (key != "PEATTRACEINFO")
        return false;
    do {
        trace.read_lines(lines, 0, '|', ';');
        if (lines.size() < 2)
            continue;
        PEAT_Record_Type type = PEAT_RT_Non_Existent;
        extract(type, lines.at(0));
        switch (type) {
        case PEAT_RT_Paths:
            path = lines.at(1);
            path = make_absolute_path(trace_info_file, path);
            trace_files = lines.at(2);
            mpi_files = lines.at(3);
            hwc_files = lines.at(4);
            bind_files = lines.at(5);
            fab_files = lines.at(6);
            com_ext = lines.at(7);
            app_ext = lines.at(8);
            // mpi_files.remove( '.' );
            // hwc_files.remove( '.' );
            // bind_files.remove( '.' );
            // fab_files.remove( '.' );
            // com_ext.remove( '.' );
            // app_ext.remove( '.' );
            break;
        case PEAT_RT_CPU_Freq:
            extract(freq, lines.at(1));
            break;
        case PEAT_RT_Var_Block_Info:
            extract(num_tscs, lines.at(1));
            extract(num_hwcs, lines.at(2));
            extract(num_addrs, lines.at(3));
            break;
        case PEAT_RT_Bind_Collective:
            extract(num_colls, lines.at(1));
            // extract( tmps_num, lines.at( 1 ) );
            // coll.resize( tmps_num );
            break;
        case PEAT_RT_Bind_Peer:
            messages_in_rank.resize(lines.size() - 1);
            for (int i = 1; i < lines.size(); i++) {
                extract(messages_in_rank[i - 1], lines.at(i));
            }
            break;
        case PEAT_RT_Number_Of_Events:
            events_in_rank.resize(lines.size() - 1);
            for (int i = 1; i < lines.size(); i++) {
                extract(events_in_rank[i - 1], lines.at(i));
            }
            break;
        case PEAT_RT_MPI_Time:
            mpi_times.resize(events_in_rank.size());
            for (int i = 1; i < lines.size(); i += 3) {
                PEAT_Rank rank;
                extract(rank, lines.at(i));
                extract(mpi_times[rank].clocks, lines.at(i + 1));
                mpi_times[rank].percentage = lines.at(i + 2);
            }
            break;
        case PEAT_RT_MPM_Rank_Time_Stamp: {
            for (int i = 1; i < lines.size(); i += 5) {
                PEAT_Rank rank;
                Times_Sync t;
                extract(rank, lines.at(i));
                extract(t.t1, lines.at(i + 1));
                extract(t.t2, lines.at(i + 2));
                extract(t.t3, lines.at(i + 3));
                extract(t.t4, lines.at(i + 4));
                rank_times[rank] = t;
            }
        } break;
        default:
            break;
        }
    } while (trace.is_file_good(0));

    std::string path_std = path.toStdString();
    std::string trace_files_std = trace_files.toStdString();
    std::string app_ext_std = app_ext.toStdString();

    std::string file_name = path_std + DIR_DELIMITER + trace_files_std + app_ext_std;

    File_Storage_GZ appfile(file_name, PEAT_FIO_FLAG_READ | PEAT_FIO_FLAG_ICLOSE, NULL, false);
    do {
        appfile.read_lines(lines, 0, ':', ';');
        if (lines.size() < 2)
            continue;
        PEAT_Record_Type type = PEAT_RT_Non_Existent;
        extract(type, lines.at(0));
        switch (type) {
        case PEAT_RT_MPM_Ranks_And_PPN: {
            PEAT_FREQ node_freq = 0;
            extract(node_freq, lines.at(2));
            PEAT_Rank node_ppn = 0;
            extract(node_ppn, lines.at(3));
            PEAT_Node node_id = 0;
            extract(node_id, lines.at(4));
            PEAT_Node_Desc tmp_desc;
            tmp_desc.name = lines.at(1);
            tmp_desc.number = node_id;
            std::string ranks_line = lines.at(5);
            std::size_t line_pos = 0;
            if (!ranks_line.empty()) {
                PEAT_Rank master_rank = 0;
                extract(master_rank, ranks_line.c_str());
                for (int i = 0; i < node_ppn; ++i) {
                    PEAT_Rank smp_rank = 0;
                    extract(smp_rank, ranks_line.c_str() + line_pos);
                    tmp_desc.ranks.push_back(smp_rank);
                    line_pos = ranks_line.find_first_of(":,", line_pos);
                    if (std::string::npos != line_pos) {
                        ++line_pos;
                    } else {
                        break;
                    }
                }
                nodes.push_back(tmp_desc);
            }
        } break;
        default:
            break;
        }
    } while (appfile.is_file_good(0));

    filename += ".user";
    File_Storage_GZ user_settings(filename, PEAT_FIO_FLAG_READ | PEAT_FIO_FLAG_ICLOSE, NULL, false);
    while (user_settings.is_file_good(0)) {
        user_settings.read_lines(lines, 0, '|');
        if (lines.size() < 2)
            continue;
        QString key = lines.at(0);
        QString value = lines.at(1);
        if (key == "searchdir") {
            src_dirs.append(value);
        }
    }

    return true;
}
void TraceInfo::update_src_dirs()
{
    std::string filename = trace_info_file.toStdString();
    filename += ".user";
    File_Storage_GZ user_settings(filename, PEAT_FIO_FLAG_WRITE, NULL, false);
    for (QString dir : src_dirs) {
        std::string line = "searchdir|" + dir.toStdString() + "\n";
        user_settings.puts(0, line);
    }
}
