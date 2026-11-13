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

#include "tableexport.h"
#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QFile>
#include <QFileDialog>
#include <QHeaderView>
#include <QList>
#include <QMessageBox>
#include <QSet>
#include <QStringList>
#include <QTableWidget>
#include <QTextStream>
#include <QTreeWidget>
#include <functional>

namespace
{
typedef QList<QStringList> Rows;

Rows table_rows(const QTableWidget *table, bool selected_only)
{
    Rows rows;
    QStringList header;
    for (int c = 0; c < table->columnCount(); ++c) {
        const QTableWidgetItem *h = table->horizontalHeaderItem(c);
        header << (h ? h->text() : QString::number(c + 1));
    }
    rows << header;
    QSet<int> selected;
    if (selected_only) {
        const QList<QTableWidgetItem *> items = table->selectedItems();
        for (const QTableWidgetItem *item : items)
            selected.insert(item->row());
    }
    for (int r = 0; r < table->rowCount(); ++r) {
        if (selected_only && !selected.contains(r))
            continue;
        QStringList row;
        for (int c = 0; c < table->columnCount(); ++c) {
            const QTableWidgetItem *item = table->item(r, c);
            row << (item ? item->text() : QString());
        }
        rows << row;
    }
    return rows;
}

Rows tree_rows(const QTreeWidget *tree, bool selected_only)
{
    Rows rows;
    QStringList header;
    for (int c = 0; c < tree->columnCount(); ++c)
        header << tree->headerItem()->text(c);
    rows << header;
    std::function<void(const QTreeWidgetItem *, int)> add = [&](const QTreeWidgetItem *item, int depth) {
        if (!selected_only || item->isSelected()) {
            QStringList row;
            for (int c = 0; c < tree->columnCount(); ++c)
                row << (c == 0 ? QString(depth * 2, QLatin1Char(' ')) + item->text(c) : item->text(c)); // nesting is kept as an indent
            rows << row;
        }
        for (int i = 0; i < item->childCount(); ++i)
            add(item->child(i), depth + 1);
    };
    for (int i = 0; i < tree->topLevelItemCount(); ++i)
        add(tree->topLevelItem(i), 0);
    return rows;
}

void to_clipboard(const Rows &rows)
{
    QStringList lines;
    for (const QStringList &row : rows) {
        QStringList cells;
        for (QString cell : row)
            cells << cell.replace(QLatin1Char('\t'), QLatin1Char(' ')).replace(QLatin1Char('\n'), QLatin1Char(' '));
        lines << cells.join(QLatin1Char('\t'));
    }
    QApplication::clipboard()->setText(lines.join(QLatin1Char('\n')));
}

void to_csv(QWidget *parent, const Rows &rows)
{
    const QString file_name = QFileDialog::getSaveFileName(parent, QObject::tr("Export to CSV"), "untitled.csv", QObject::tr("CSV files (*.csv);;All files (*)"));
    if (file_name.isEmpty())
        return;
    QFile file(file_name);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(parent, QObject::tr("Export to CSV"), QObject::tr("Cannot write %1:\n%2").arg(file_name, file.errorString()));
        return;
    }
    QTextStream out(&file);
    for (const QStringList &row : rows) {
        QStringList cells;
        for (QString cell : row) {
            if (cell.contains(QLatin1Char(',')) || cell.contains(QLatin1Char('"')) || cell.contains(QLatin1Char('\n'))) {
                cell.replace(QLatin1Char('"'), QLatin1String("\"\""));
                cell = QLatin1Char('"') + cell + QLatin1Char('"');
            }
            cells << cell;
        }
        out << cells.join(QLatin1Char(',')) << '\n';
    }
}

template <typename W, typename F>
void install_actions(W *widget, F get_rows)
{
    QAction *copy = new QAction(QObject::tr("Copy selected rows"), widget);
    copy->setShortcut(QKeySequence::Copy);
    copy->setShortcutContext(Qt::WidgetShortcut);
    QObject::connect(copy, &QAction::triggered, widget, [widget, get_rows]() { to_clipboard(get_rows(widget, true)); });
    QAction *copy_all = new QAction(QObject::tr("Copy all"), widget);
    QObject::connect(copy_all, &QAction::triggered, widget, [widget, get_rows]() { to_clipboard(get_rows(widget, false)); });
    QAction *csv = new QAction(QObject::tr("Export to CSV..."), widget);
    QObject::connect(csv, &QAction::triggered, widget, [widget, get_rows]() { to_csv(widget, get_rows(widget, false)); });
    widget->addAction(copy);
    widget->addAction(copy_all);
    widget->addAction(csv);
    widget->setContextMenuPolicy(Qt::ActionsContextMenu);
}
} // namespace

void Table_Export::install(QTableWidget *table)
{
    if (table)
        install_actions(table, table_rows);
}

void Table_Export::install(QTreeWidget *tree)
{
    if (tree)
        install_actions(tree, tree_rows);
}
