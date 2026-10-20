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

#ifndef __FILTERDIALOG_H__
#define __FILTERDIALOG_H__

#include "communicatorsdialog.h"
#include "globals.h"
#include <QAbstractButton>
#include <QDialog>

namespace Ui
{
class FilterDialog;
}

// Tabbed dialog selecting which trace elements are shown. Each tab is defined in filterdialog.ui.
class FilterDialog : public QDialog
{
    Q_OBJECT

  public:
    explicit FilterDialog(QWidget *parent = nullptr);
    ~FilterDialog();
    void set_properties(const TV_TraceParts &shown);
    void get_properties(TV_TraceParts &shown) const;
    // Fills the Communicators tab; without it (no trace loaded) the tab is disabled.
    void set_communicators(const Communicators *communicators, const std::set<PEAT_MPI_Comm> &hidden);

    bool redraw_requested() const { return redraw; } // true if the dialog was closed with Apply

  private slots:
    void on_buttonBox_clicked(QAbstractButton *button); // auto-connected by name
    void on_buttonCommsAll_clicked();  // auto-connected by name
    void on_buttonCommsNone_clicked(); // auto-connected by name

  private:
    void set_all_comms(Qt::CheckState state);
    bool comms_filled;
    bool redraw;
    Ui::FilterDialog *ui;
};

#endif // __FILTERDIALOG_H__
