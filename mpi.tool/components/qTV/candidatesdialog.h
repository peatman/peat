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

#ifndef __CANDIDATESDIALOG_H__
#define __CANDIDATESDIALOG_H__

#include "elements.h"
#include <QDialog>

namespace Ui
{
class CandidatesDialog;
}

// List of the views found by an Analyse action, the worst one first. A click on a row asks
// TV to show that view; the rows are in the order of Elements::get_candidates().
class CandidatesDialog : public QDialog
{
    Q_OBJECT

  public:
    CandidatesDialog(QWidget *parent, const QString &title, const std::map<double, PEAT_Viewport> &candidates, double freq, double model_left);
    ~CandidatesDialog();

  signals:
    void candidate_selected(int index);

  private:
    Ui::CandidatesDialog *ui;
};

#endif // __CANDIDATESDIALOG_H__
