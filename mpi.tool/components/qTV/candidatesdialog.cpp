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

#include "candidatesdialog.h"
#include "tableexport.h"
#include "ui_candidatesdialog.h"

CandidatesDialog::CandidatesDialog(QWidget *parent, const QString &title, const std::map<double, PEAT_Viewport> &candidates, double freq, double model_left)
    : QDialog(parent), ui(new Ui::CandidatesDialog)
{
    ui->setupUi(this);
    setAttribute(Qt::WA_DeleteOnClose);
    setWindowTitle(title);
    QTableWidget *table = ui->tableViews;
    table->setRowCount(int(candidates.size()));
    int row = 0;
    for (const auto &entry : candidates) {
        table->setItem(row, 0, new QTableWidgetItem(QString::number(row)));
        table->setItem(row, 1, new QTableWidgetItem(QString::number(entry.first, 'g', 6)));
        table->setItem(row, 2, new QTableWidgetItem(freq > 0 ? format_seconds((entry.second.start - model_left) / freq, 9) : QString::number(entry.second.start)));
        table->setItem(row, 3, new QTableWidgetItem(freq > 0 ? format_seconds((entry.second.end - entry.second.start) / freq) : QString::number(entry.second.end - entry.second.start)));
        ++row;
    }
    table->resizeColumnsToContents();
    table->selectRow(0);
    Table_Export::install(table);
    // the table is not sortable: the number of a row is the number of the view
    connect(table, &QTableWidget::cellClicked, this, [this](int clicked_row, int) { emit candidate_selected(clicked_row); });
}

CandidatesDialog::~CandidatesDialog()
{
    delete ui;
}
