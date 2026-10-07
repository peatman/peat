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

#ifndef __FUNCTIONSTABWIDGET_H__
#define __FUNCTIONSTABWIDGET_H__

#include <QTabWidget>
#include <QTableWidget>

namespace Ui
{
class FunctionsTabWidget;
}

class FunctionsTabWidget : public QTabWidget
{
    Q_OBJECT

  public:
    explicit FunctionsTabWidget(QWidget *parent = nullptr);
    ~FunctionsTabWidget();
    QTableWidget *get_mpi_table();
    QTableWidget *get_omp_table();
    QTableWidget *get_gpu_table();

  private:
    Ui::FunctionsTabWidget *ui;
};

#endif // __FUNCTIONSTABWIDGET_H__
