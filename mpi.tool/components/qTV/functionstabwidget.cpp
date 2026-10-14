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

#include "functionstabwidget.h"
#include "ui_functionstabwidget.h"

FunctionsTabWidget::FunctionsTabWidget(QWidget *parent)
    : QTabWidget(parent), ui(new Ui::FunctionsTabWidget)
{
    ui->setupUi(this);
}

FunctionsTabWidget::~FunctionsTabWidget()
{
    delete ui;
}
QTableWidget *FunctionsTabWidget::get_mpi_table() { return ui->mpi; }
QTableWidget *FunctionsTabWidget::get_omp_table() { return ui->omp; }
QTableWidget *FunctionsTabWidget::get_gpu_table() { return ui->gpu; }
QTableWidget *FunctionsTabWidget::get_transport_table() { return ui->transport; }
