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

#include "communicatorsdialog.h"
#include "ui_communicatorsdialog.h"
#include <QApplication>
#include <QHeaderView>
#include <QMessageBox>
#include <QStringList>
#include <QTableWidgetItem>

CommunicatorsDialog::CommunicatorsDialog(QWidget *parent)
    : QDialog(parent), ui(new Ui::CommunicatorsDialog)
{
    ui->setupUi(this);
    QHeaderView *header = ui->tableWidget->horizontalHeader();
    header->setSectionResizeMode(QHeaderView::ResizeToContents);
    header->setSectionResizeMode(3, QHeaderView::Stretch);
    header->setSectionResizeMode(4, QHeaderView::Stretch);
}

CommunicatorsDialog::~CommunicatorsDialog()
{
    delete ui;
}

// Contiguous ascending runs are collapsed (0,1,2,3 -> 0-3); the order of ranks is preserved
// because the position of a rank in the list is its local rank in the communicator.
QString CommunicatorsDialog::format_ranks(const std::vector<PEAT_Rank> &ranks)
{
    QStringList parts;
    size_t i = 0;
    while (i < ranks.size()) {
        size_t j = i;
        while (j + 1 < ranks.size() && ranks[j + 1] == ranks[j] + 1)
            ++j;
        if (j > i + 1) {
            parts << QString("%1-%2").arg(ranks[i]).arg(ranks[j]);
        } else {
            for (size_t k = i; k <= j; ++k)
                parts << QString::number(ranks[k]);
        }
        i = j + 1;
    }
    return parts.join(", ");
}

CommunicatorsData::CommunicatorsData(const Trace_Info &info) : communicators(glob)
{
    glob.input_path = info.path.toStdString();
    glob.trace_files = info.trace_files.toStdString();
    glob.com_ext = info.com_ext.toStdString();
    glob.app_ext = info.app_ext.toStdString();
    model_id.initialize(glob);
    std::map<PEAT_Rank, std::string> com_names = get_file_names(glob.input_path, glob.trace_files, glob.com_ext, true);
    std::map<PEAT_Rank, std::string> app_names;
    app_names[0] = glob.input_path + DIR_DELIMITER + glob.trace_files + glob.app_ext;
    communicators.initialize(com_names, app_names, &model_id);
}

bool CommunicatorsDialog::load(QWidget *parent, const Trace_Info &info, std::shared_ptr<CommunicatorsData> &data)
{
    if (data)
        return true;
    if (info.path.isEmpty() || info.trace_files.isEmpty()) {
        QMessageBox::information(parent, tr("Communicators"), tr("Please, load a trace first."));
        return false;
    }
    QApplication::setOverrideCursor(Qt::WaitCursor);
    try {
        data = std::make_shared<CommunicatorsData>(info);
    } catch (const std::exception &ex) {
        QApplication::restoreOverrideCursor();
        QMessageBox::critical(parent, tr("Communicators"), tr("Can't read communicators: %1").arg(ex.what()));
        return false;
    }
    QApplication::restoreOverrideCursor();
    return true;
}

// Groups are reported per observing rank (an inter communicator {0} and {1,2,3} has group 1 = {0}, group 2 = {1,2,3}
// for rank 0 and the other way round for ranks 1-3). Both groups are taken from one rank so they stay consistent:
// the first rank that reports a remote group, otherwise the first rank's local group (intra communicator).
// Traces without group info fall back to the communicator ranks.
void CommunicatorsDialog::get_groups(const Communicators::Communicator_Info &info, std::vector<PEAT_Rank> &group1, std::vector<PEAT_Rank> &group2)
{
    group1 = info.ranks;
    group2.clear();
    if (!info.groups[1].empty()) {
        const PEAT_Rank rank = info.groups[1].begin()->first;
        group2 = info.groups[1].begin()->second;
        const auto local = info.groups[0].find(rank);
        if (local != info.groups[0].end())
            group1 = local->second;
    } else if (!info.groups[0].empty()) {
        group1 = info.groups[0].begin()->second;
    }
}

void CommunicatorsDialog::set_communicators(const Communicators &communicators, const std::vector<PEAT_MPI_Comm> *only)
{
    QTableWidget *table = ui->tableWidget;
    table->setRowCount(0);
    int inter = 0;
    auto add_row = [&](const QString &id, bool is_inter, const QString &size, const QString &g1, const QString &g2) {
        const int row = table->rowCount();
        table->insertRow(row);
        QTableWidgetItem *id_item = new QTableWidgetItem(id);
        id_item->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        QTableWidgetItem *size_item = new QTableWidgetItem(size);
        size_item->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        table->setItem(row, 0, id_item);
        table->setItem(row, 1, new QTableWidgetItem(is_inter ? tr("Inter") : tr("Intra")));
        table->setItem(row, 2, size_item);
        table->setItem(row, 3, new QTableWidgetItem(g1));
        table->setItem(row, 4, new QTableWidgetItem(g2));
        inter += is_inter;
    };
    auto add_info = [&](const Communicators::Communicator_Info &info) {
        std::vector<PEAT_Rank> group1, group2;
        get_groups(info, group1, group2);
        add_row(QString::number(info.comm_id, 16), !group2.empty(), QString::number(info.ranks.size()), format_ranks(group1), format_ranks(group2));
    };
    // predefined communicators are not listed in the trace: 1 is MPI_COMM_SELF, 2 is MPI_COMM_WORLD
    auto add_predefined = [&](PEAT_MPI_Comm id) {
        std::vector<PEAT_Rank> ranks;
        QString name = QString::number(id, 16);
        QString size = tr("N/A"), group = tr("N/A");
        if (id == 2) {
            for (PEAT_Rank r = 0; r < communicators.get_comm_world_rank_count(); ++r)
                ranks.push_back(r);
            name += tr(" (WORLD)");
            size = QString::number(ranks.size());
            group = format_ranks(ranks);
        } else if (id == 1) { // differs per rank: each rank is alone in its own SELF
            name += tr(" (SELF)");
            size = "1";
            group = tr("each rank itself");
        }
        add_row(name, false, size, group, QString());
    };
    if (!only) {
        add_predefined(1);
        add_predefined(2);
        for (const auto &entry : communicators.comms)
            add_info(entry.second);
    } else {
        for (PEAT_MPI_Comm id : *only) {
            auto it = communicators.comms.find(id);
            if (it != communicators.comms.end())
                add_info(it->second);
            else
                add_predefined(id);
        }
    }
    table->resizeRowsToContents();
    ui->labelSummary->setText(tr("%1 communicators (%2 inter)").arg(table->rowCount()).arg(inter));
}
