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
#include <QHeaderView>
#include <QStyle>

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

void MessagesTabWidget::fit_to_contents(const QSize &max_size)
{
    int width = 0;
    int height = 0;
    QTableWidget *tables[] = {ui->initiation, ui->completion, ui->p2p, ui->gpu};
    for (QTableWidget *tbl : tables) {
        tbl->resizeColumnsToContents();
        tbl->resizeRowsToContents();
        int cols = tbl->verticalHeader()->sizeHint().width() + 2 * tbl->frameWidth();
        for (int i = 0; i < tbl->columnCount(); ++i)
            cols += tbl->columnWidth(i);
        int rows = tbl->horizontalHeader()->sizeHint().height() + 2 * tbl->frameWidth();
        for (int i = 0; i < tbl->rowCount(); ++i)
            rows += tbl->rowHeight(i);
        width = qMax(width, cols);
        height = qMax(height, rows);
    }
    QTreeWidget *tree = ui->collectives;
    int tree_cols = 2 * tree->frameWidth();
    for (int i = 0; i < tree->columnCount(); ++i) {
        tree->resizeColumnToContents(i);
        tree_cols += tree->columnWidth(i);
    }
    width = qMax(width, tree_cols);
    // room for the vertical scroll bar, tab bar and frame around the pages
    const int frame = style()->pixelMetric(QStyle::PM_DefaultFrameWidth); // QTabWidget is not a QFrame: no frameWidth()
    const int extra_w = style()->pixelMetric(QStyle::PM_ScrollBarExtent) + 2 * frame + 16;
    const int extra_h = tabBar()->sizeHint().height() + 2 * frame + 16;
    resize(qMin(width + extra_w, max_size.width()), qMin(height + extra_h, max_size.height()));
}
