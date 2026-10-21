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

#include "filterdialog.h"
#include "ui_filterdialog.h"
#include <QHeaderView>
#include <QTableWidgetItem>

FilterDialog::FilterDialog(QWidget *parent)
    : QDialog(parent), ui(new Ui::FilterDialog), comms_filled(false), redraw(false)
{
    ui->setupUi(this);
    ui->tabWidget->setTabEnabled(ui->tabWidget->indexOf(ui->tabCommunicators), false);
    QHeaderView *header = ui->tableComms->horizontalHeader();
    header->setSectionResizeMode(QHeaderView::ResizeToContents);
    header->setSectionResizeMode(4, QHeaderView::Stretch);
    header->setSectionResizeMode(5, QHeaderView::Stretch);
}

FilterDialog::~FilterDialog()
{
    delete ui;
}

void FilterDialog::set_properties(const TV_TraceParts &shown)
{
    ui->checkLocalsFunc->setChecked(shown.mpi_functions[TV_MPI_FUNC_GROUP_LOCALS]);
    ui->checkP2PFunc->setChecked(shown.mpi_functions[TV_MPI_FUNC_GROUP_P2P]);
    ui->checkCollectivesFunc->setChecked(shown.mpi_functions[TV_MPI_FUNC_GROUP_COLLECTIVES]);
    ui->checkRequestsFunc->setChecked(shown.mpi_functions[TV_MPI_FUNC_GROUP_REQUESTS]);
    ui->checkP2PInternalFunc->setChecked(shown.mpi_functions[TV_MPI_FUNC_GROUP_P2P_INTERNAL]);
    ui->checkCollectivesInternalFunc->setChecked(shown.mpi_functions[TV_MPI_FUNC_GROUP_COLLECTIVES_INTERNAL]);
    ui->checkRequestsInternalFunc->setChecked(shown.mpi_functions[TV_MPI_FUNC_GROUP_REQUESTS_INTERNAL]);
    ui->checkP2PMessages->setChecked(shown.mpi_messages[0]);
    ui->checkCollectivesMessages->setChecked(shown.mpi_messages[1]);
    ui->checkInitiationMessages->setChecked(shown.mpi_messages[2]);
    ui->checkCompletionsMessages->setChecked(shown.mpi_messages[3]);
}

void FilterDialog::get_properties(TV_TraceParts &shown) const
{
    shown.mpi_functions[TV_MPI_FUNC_GROUP_LOCALS] = ui->checkLocalsFunc->isChecked();
    shown.mpi_functions[TV_MPI_FUNC_GROUP_P2P] = ui->checkP2PFunc->isChecked();
    shown.mpi_functions[TV_MPI_FUNC_GROUP_COLLECTIVES] = ui->checkCollectivesFunc->isChecked();
    shown.mpi_functions[TV_MPI_FUNC_GROUP_REQUESTS] = ui->checkRequestsFunc->isChecked();
    shown.mpi_functions[TV_MPI_FUNC_GROUP_P2P_INTERNAL] = ui->checkP2PInternalFunc->isChecked();
    shown.mpi_functions[TV_MPI_FUNC_GROUP_COLLECTIVES_INTERNAL] = ui->checkCollectivesInternalFunc->isChecked();
    shown.mpi_functions[TV_MPI_FUNC_GROUP_REQUESTS_INTERNAL] = ui->checkRequestsInternalFunc->isChecked();
    shown.mpi_messages[0] = ui->checkP2PMessages->isChecked();
    shown.mpi_messages[1] = ui->checkCollectivesMessages->isChecked();
    shown.mpi_messages[2] = ui->checkInitiationMessages->isChecked();
    shown.mpi_messages[3] = ui->checkCompletionsMessages->isChecked();
    if (comms_filled) {
        shown.hidden_comms.clear();
        for (int row = 0; row < ui->tableComms->rowCount(); ++row) {
            const QTableWidgetItem *item = ui->tableComms->item(row, 0);
            if (item->checkState() != Qt::Checked)
                shown.hidden_comms.insert(item->data(Qt::UserRole).toInt());
        }
    }
}

void FilterDialog::set_communicators(const Communicators *communicators, const std::set<PEAT_MPI_Comm> &hidden)
{
    if (!communicators)
        return;
    QTableWidget *table = ui->tableComms;
    table->setRowCount(0);
    auto add_row = [&](PEAT_MPI_Comm id, const QString &name, bool is_inter, const QString &origin, size_t size, const QString &group1, const QString &group2) {
        const int row = table->rowCount();
        table->insertRow(row);
        QTableWidgetItem *id_item = new QTableWidgetItem(name);
        id_item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsUserCheckable);
        id_item->setCheckState(hidden.count(id) ? Qt::Unchecked : Qt::Checked);
        id_item->setData(Qt::UserRole, id);
        QTableWidgetItem *size_item = new QTableWidgetItem(QString::number(size));
        size_item->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        table->setItem(row, 0, id_item);
        table->setItem(row, 1, new QTableWidgetItem(is_inter ? tr("Inter") : tr("Intra")));
        table->setItem(row, 2, new QTableWidgetItem(origin));
        table->setItem(row, 3, size_item);
        table->setItem(row, 4, new QTableWidgetItem(group1));
        table->setItem(row, 5, new QTableWidgetItem(group2));
    };
    // predefined communicators are not listed in the trace: 1 is MPI_COMM_SELF, 2 is MPI_COMM_WORLD
    std::vector<PEAT_Rank> world_ranks;
    for (PEAT_Rank r = 0; r < communicators->get_comm_world_rank_count(); ++r)
        world_ranks.push_back(r);
    add_row(1, tr("1 (comm_self)"), false, tr("Predefined"), 1, tr("each rank itself"), QString()); // differs per rank
    add_row(2, tr("2 (comm_world)"), false, tr("Predefined"), world_ranks.size(), CommunicatorsDialog::format_ranks(world_ranks), QString());
    for (const auto &entry : communicators->comms) {
        const Communicators::Communicator_Info &info = entry.second;
        std::vector<PEAT_Rank> group1, group2;
        CommunicatorsDialog::get_groups(info, group1, group2);
        add_row(info.comm_id, QString::number(info.comm_id, 16), !group2.empty(), info.is_internal ? tr("MPI internal") : tr("User"), info.ranks.size(), CommunicatorsDialog::format_ranks(group1), CommunicatorsDialog::format_ranks(group2));
    }
    table->resizeRowsToContents();
    comms_filled = true;
    ui->tabWidget->setTabEnabled(ui->tabWidget->indexOf(ui->tabCommunicators), true);
}

void FilterDialog::set_all_comms(Qt::CheckState state)
{
    for (int row = 0; row < ui->tableComms->rowCount(); ++row)
        ui->tableComms->item(row, 0)->setCheckState(state);
}

void FilterDialog::on_buttonBox_clicked(QAbstractButton *button)
{
    if (ui->buttonBox->buttonRole(button) == QDialogButtonBox::ApplyRole) { // Apply: like OK, but redraw
        redraw = true;
        accept();
    }
}

void FilterDialog::on_buttonCommsAll_clicked()
{
    set_all_comms(Qt::Checked);
}

void FilterDialog::on_buttonCommsNone_clicked()
{
    set_all_comms(Qt::Unchecked);
}
