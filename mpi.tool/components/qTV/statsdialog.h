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

#ifndef __STATSDIALOG_H__
#define __STATSDIALOG_H__

#include "elements.h"
#include <QDialog>

namespace Ui
{
class StatsDialog;
}

// Aggregated numbers of a time window of the loaded trace: time per function,
// MPI time per rank and the point to point messages between pairs of ranks.
// The layout and the columns of the tables are in statsdialog.ui.
class StatsDialog : public QDialog
{
    Q_OBJECT

  public:
    StatsDialog(QWidget *parent, const Elements::Statistics &stats, double window_seconds, double freq, const QString &window_text);
    ~StatsDialog();

  private:
    Ui::StatsDialog *ui;
    void fill_functions(const Elements::Statistics &stats, double window_seconds, double freq);
    void fill_ranks(const Elements::Statistics &stats, double window_seconds, double freq);
    void fill_messages(const Elements::Statistics &stats);
};

#endif // __STATSDIALOG_H__
