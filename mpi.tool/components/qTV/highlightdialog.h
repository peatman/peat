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

#ifndef __HIGHLIGHTDIALOG_H__
#define __HIGHLIGHTDIALOG_H__

#include "../common/communicators.h"
#include "globals.h"
#include <QDialog>

namespace Ui
{
class HighlightDialog;
}

// View > Highlight: which events are drawn bright while the others are dimmed
// (TV_TraceParts::highlight_func, highlight_comm, highlight_min_bytes).
class HighlightDialog : public QDialog
{
    Q_OBJECT

  public:
    explicit HighlightDialog(QWidget *parent = nullptr);
    ~HighlightDialog();
    // communicators may be NULL when they could not be read: the predefined ones are listed anyway
    void set_properties(const TV_TraceParts &shown, const Communicators *communicators);
    void get_properties(TV_TraceParts &shown) const;
    void accept() override; // refuses an unknown function name

  private:
    Ui::HighlightDialog *ui;
    void reset();
};

#endif // __HIGHLIGHTDIALOG_H__
