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

#ifndef __TRACE_INFO_H__
#define __TRACE_INFO_H__
#include "../common/common_types.h"
#include "../common/func_id.h"
#include "../common/ptp.h"
#include <QFileInfo>
#include <QString>

class Trace_Info
{
  public:
    Trace_Info();
    bool parse_trace_info_file(QFileInfo &tracefile);
    bool has_transport_files() const;
    void update_src_dirs();
    // The <trace info file>.user file keeps the user's data of a trace as key|value lines:
    // "searchdir|<dir>" and "bookmark|<left>|<right>|<name>" (a named view, bounds in clock ticks).
    // Writing one kind of lines keeps all the other lines of the file.
    QString user_file_name() const;
    QStringList read_bookmarks() const; // "<left>|<right>|<name>" entries
    bool write_bookmarks(const QStringList &bookmarks) const;
    static QString make_absolute_path(const QString &file_path, const QString &relative_dir);

    PEAT_LNum num_colls;
    qreal freq;
    qint32 num_addrs;
    qint32 num_tscs;
    qint32 num_hwcs;
    QString path, trace_files, mpi_files, hwc_files, bind_files, transport_files, com_ext, app_ext;
    std::vector<PEAT_UID> messages_in_rank;
    std::vector<PEAT_UID> events_in_rank;
    std::vector<PEAT_MPI_Time> mpi_times;
    std::vector<PEAT_Node_Desc> nodes;
    std::map<PEAT_Rank, Times_Sync> rank_times;
    QStringList src_dirs;
    QString trace_info_file;

  private:
    QStringList read_user_values(const QString &key) const;
    bool write_user_values(const QString &key, const QStringList &values) const;
};

#endif // __TRACE_INFO_H__
