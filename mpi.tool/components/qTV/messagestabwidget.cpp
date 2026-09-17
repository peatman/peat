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

#include "messagestabwidget.h"
#include "ui_messagestabwidget.h"

MessagesTabWidget::MessagesTabWidget(QWidget *parent)
    : QTabWidget(parent), ui(new Ui::MessagesTabWidget)
{
    ui->setupUi(this);
}

MessagesTabWidget::~MessagesTabWidget()
{
    delete ui;
}
QTableWidget *MessagesTabWidget::get_init_table() { return ui->initiation; }
QTableWidget *MessagesTabWidget::get_comp_table() { return ui->completion; }
QTableWidget *MessagesTabWidget::get_p2p_table() { return ui->p2p; }
QTableWidget *MessagesTabWidget::get_gpu_table() { return ui->gpu; }
QTreeWidget *MessagesTabWidget::get_collective_tree() { return ui->collectives; }
