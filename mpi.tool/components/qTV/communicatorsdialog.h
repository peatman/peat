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

#ifndef __COMMUNICATORSDIALOG_H__
#define __COMMUNICATORSDIALOG_H__

#include "../common/communicators.h"
#include "trace_info.h"
#include <QDialog>
#include <memory>
#include <vector>

namespace Ui
{
class CommunicatorsDialog;
}

// Communicators of a trace read from its app and com files. Throws std::exception on failure.
struct Communicators_Data {
    explicit Communicators_Data(const Trace_Info &info);
    Global_Parameters glob;
    Model_ID model_id;
    Communicators communicators;
};

// Shows all communicators of a trace: ID, type (intra/inter), size and rank groups.
class CommunicatorsDialog : public QDialog
{
    Q_OBJECT

  public:
    explicit CommunicatorsDialog(QWidget *parent = nullptr);
    ~CommunicatorsDialog();
    // Shows every communicator, or only those listed in `only` (e.g. the communicator of an event).
    void set_communicators(const Communicators &communicators, const std::vector<PEAT_MPI_Comm> *only = nullptr);
    // Reads the communicators of the trace once (cached in `data`); returns false after reporting an error.
    static bool load(QWidget *parent, const Trace_Info &info, std::shared_ptr<Communicators_Data> &data);

    static QString format_ranks(const std::vector<PEAT_Rank> &ranks);
    // Group 1 (local) and group 2 (remote, empty for an intra communicator) of a communicator.
    static void get_groups(const Communicators::Communicator_Info &info, std::vector<PEAT_Rank> &group1, std::vector<PEAT_Rank> &group2);

  private:
    Ui::CommunicatorsDialog *ui;
};

#endif // __COMMUNICATORSDIALOG_H__
