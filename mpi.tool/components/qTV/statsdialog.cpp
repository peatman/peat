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

#include "statsdialog.h"
#include "tableexport.h"
#include "ui_statsdialog.h"
#include <QTableWidget>

namespace
{
// a cell that shows a formatted number and sorts by its value
class Number_Item : public QTableWidgetItem
{
  public:
    Number_Item(double v, const QString &text) : QTableWidgetItem(text), value(v)
    {
        setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
    }
    bool operator<(const QTableWidgetItem &other) const override
    {
        const Number_Item *o = dynamic_cast<const Number_Item *>(&other);
        return o ? value < o->value : QTableWidgetItem::operator<(other);
    }

  private:
    double value;
};

Number_Item *count_item(long long v)
{
    return new Number_Item(double(v), QString("%L1").arg(v));
}
Number_Item *real_item(double v, int decimals)
{
    return new Number_Item(v, QString("%L1").arg(v, 0, 'f', decimals));
}

// Sorting is switched on only after a table is filled: a sorted table moves a row
// as soon as one of its cells is set, which breaks filling by row numbers.
void finish_table(QTableWidget *table, int sort_column)
{
    Table_Export::install(table);
    table->setSortingEnabled(true);
    table->sortByColumn(sort_column, Qt::DescendingOrder);
    table->resizeColumnsToContents();
}
} // namespace

StatsDialog::StatsDialog(QWidget *parent, const Elements::Statistics &stats, double window_seconds, double freq, const QString &window_text)
    : QDialog(parent), ui(new Ui::StatsDialog)
{
    ui->setupUi(this);
    setAttribute(Qt::WA_DeleteOnClose);
    ui->labelWindow->setText(window_text);
    fill_functions(stats, window_seconds, freq);
    fill_ranks(stats, window_seconds, freq);
    fill_messages(stats);
}

StatsDialog::~StatsDialog()
{
    delete ui;
}

// time per function
void StatsDialog::fill_functions(const Elements::Statistics &stats, double window_seconds, double freq)
{
    QTableWidget *table = ui->tableFunctions;
    const double rank_seconds = window_seconds * stats.ranks.size(); // time of all loaded ranks in the window
    const double us = freq > 0 ? 1e6 / freq : 0;
    table->setRowCount(int(stats.funcs.size()));
    int row = 0;
    for (const auto &entry : stats.funcs) {
        const Elements::Func_Stat &f = entry.second;
        const double total = freq > 0 ? f.total / freq : 0;
        table->setItem(row, 0, new QTableWidgetItem(global_func_id.name_by_idx(entry.first.first)));
        table->setItem(row, 1, new QTableWidgetItem(QString(tv_event_group_name(f.group)) + (entry.first.second ? tr(" (nested)") : QString())));
        table->setItem(row, 2, count_item(f.calls));
        table->setItem(row, 3, real_item(total, 6));
        table->setItem(row, 4, real_item(rank_seconds > 0 ? total * 100 / rank_seconds : 0, 2));
        table->setItem(row, 5, real_item(f.calls ? double(f.full) * us / f.calls : 0, 3));
        table->setItem(row, 6, real_item(double(f.min) * us, 3));
        table->setItem(row, 7, real_item(double(f.max) * us, 3));
        ++row;
    }
    finish_table(table, 3); // the biggest total time first
}

// MPI time per rank
void StatsDialog::fill_ranks(const Elements::Statistics &stats, double window_seconds, double freq)
{
    QTableWidget *table = ui->tableRanks;
    table->setRowCount(int(stats.ranks.size()));
    int row = 0;
    for (const auto &entry : stats.ranks) {
        const double mpi = freq > 0 ? entry.second.mpi_time / freq : 0;
        table->setItem(row, 0, count_item(entry.first));
        table->setItem(row, 1, count_item(entry.second.mpi_calls));
        table->setItem(row, 2, real_item(mpi, 6));
        table->setItem(row, 3, real_item(window_seconds > 0 ? mpi * 100 / window_seconds : 0, 2));
        ++row;
    }
    finish_table(table, 2); // the biggest MPI time first
}

// point to point messages between pairs of ranks
void StatsDialog::fill_messages(const Elements::Statistics &stats)
{
    QTableWidget *table = ui->tableMessages;
    table->setRowCount(int(stats.pairs.size()));
    int row = 0;
    for (const auto &entry : stats.pairs) {
        const Elements::Pair_Stat &m = entry.second;
        table->setItem(row, 0, count_item(entry.first.first));
        table->setItem(row, 1, count_item(entry.first.second));
        table->setItem(row, 2, count_item(m.messages));
        table->setItem(row, 3, count_item(m.bytes));
        table->setItem(row, 4, real_item(m.messages ? double(m.bytes) / m.messages : 0, 1));
        ++row;
    }
    finish_table(table, 3); // the most bytes first
}
