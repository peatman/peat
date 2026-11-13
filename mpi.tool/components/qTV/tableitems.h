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

#ifndef __TABLEITEMS_H__
#define __TABLEITEMS_H__

#include <QHeaderView>
#include <QLocale>
#include <QTableWidget>
#include <QTreeWidget>

// Cells of the Functions / Messages / User regions views hold numbers as texts formatted for
// reading ("1,234,567"), which sort wrongly as texts. These items sort such cells by value:
// two numbers by their values, a number before a text, two texts alphabetically.
inline bool tv_cell_less(const QString &a, const QString &b)
{
    const QLocale locale; // the cells are formatted with the default locale (%L)
    bool a_number = false, b_number = false;
    const double x = locale.toDouble(a.trimmed(), &a_number);
    const double y = locale.toDouble(b.trimmed(), &b_number);
    if (a_number && b_number)
        return x < y;
    if (a_number != b_number)
        return a_number;
    return a.compare(b, Qt::CaseInsensitive) < 0;
}

class TV_Table_Item : public QTableWidgetItem
{
  public:
    TV_Table_Item() : QTableWidgetItem() {}
    explicit TV_Table_Item(const QString &text) : QTableWidgetItem(text) {}
    bool operator<(const QTableWidgetItem &other) const override
    {
        return tv_cell_less(text(), other.text());
    }
};

class TV_Tree_Item : public QTreeWidgetItem
{
  public:
    TV_Tree_Item(QTreeWidget *tree, const QStringList &strings) : QTreeWidgetItem(tree, strings) {}
    explicit TV_Tree_Item(QTreeWidgetItem *parent) : QTreeWidgetItem(parent) {}
    bool operator<(const QTreeWidgetItem &other) const override
    {
        const int column = treeWidget() ? treeWidget()->sortColumn() : 0;
        return tv_cell_less(text(column), other.text(column));
    }
};

// Sorting by a click on a column header. To be called after the view is filled: a sorted view
// moves a row as soon as one of its cells is set. The rows keep their order until the first click.
inline void tv_enable_sorting(QTableWidget *table)
{
    table->horizontalHeader()->setSortIndicator(-1, Qt::AscendingOrder);
    table->setSortingEnabled(true);
}
inline void tv_enable_sorting(QTreeWidget *tree)
{
    tree->header()->setSortIndicator(-1, Qt::AscendingOrder);
    tree->setSortingEnabled(true);
}

#endif // __TABLEITEMS_H__
