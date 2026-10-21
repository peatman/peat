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

#ifndef __DRAWOPTIONSDIALOG_H__
#define __DRAWOPTIONSDIALOG_H__

#include "globals.h"
#include <QCheckBox>
#include <QDialog>
#include <QSpinBox>

// Drawing options of the timeline view (Options -> Drawing...)
class DrawOptionsDialog : public QDialog
{
  public:
    explicit DrawOptionsDialog(QWidget *parent = nullptr);
    void set_properties(const TV_TraceParts &shown);
    void get_properties(TV_TraceParts &shown) const;

  private:
    QSpinBox *spin_func_min_width;
    QCheckBox *check_offscreen_messages;
};

#endif // __DRAWOPTIONSDIALOG_H__
