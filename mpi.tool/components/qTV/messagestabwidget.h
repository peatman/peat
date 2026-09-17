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

#ifndef __MESSAGESTABWIDGET_H__
#define __MESSAGESTABWIDGET_H__

#include <QTabWidget>
#include <QTableWidget>
#include <QTreeWidget>

namespace Ui
{
class MessagesTabWidget;
}

class MessagesTabWidget : public QTabWidget
{
    Q_OBJECT

  public:
    explicit MessagesTabWidget(QWidget *parent = nullptr);
    ~MessagesTabWidget();
    QTableWidget *get_init_table();
    QTableWidget *get_comp_table();
    QTableWidget *get_p2p_table();
    QTableWidget *get_gpu_table();
    QTreeWidget *get_collective_tree();

  private:
    Ui::MessagesTabWidget *ui;
};

#endif // __MESSAGESTABWIDGET_H__
