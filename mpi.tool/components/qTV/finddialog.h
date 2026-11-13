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

#ifndef __FINDDIALOG_H__
#define __FINDDIALOG_H__

#include "../common/common_plain_types.h"
#include <QDialog>
#include <vector>

namespace Ui
{
class FindFunctionDialog;
}

// Non-modal dialog that asks for a function name and a rank and requests the next or the
// previous call of it. The search itself is done by TV (see TV::find_function_step).
class FindFunctionDialog : public QDialog
{
    Q_OBJECT

  public:
    explicit FindFunctionDialog(QWidget *parent = nullptr);
    ~FindFunctionDialog();
    void set_ranks(const std::vector<PEAT_Rank> &ranks); // the loaded ranks; the chosen one is kept if it is still there
    QString function_name() const;
    PEAT_Rank rank() const; // -1 for any loaded rank
    bool dim_others() const;
    void set_status(const QString &text);
    void open_for_search(); // shows the dialog with the name ready to be typed over

  signals:
    void find_requested(int direction); // +1 the next call, -1 the previous one
    void search_changed();              // another name or rank: the search starts over
    void dim_changed();                 // "Dim the other functions" or the function it applies to has changed

  private:
    Ui::FindFunctionDialog *ui;
};

#endif // __FINDDIALOG_H__
