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

#include "tracedialog.h"
#include "ui_tracedialog.h"

TraceDialog::TraceDialog(QWidget *parent)
    : QDialog(parent), ui(new Ui::Dialog)
{
    ui->setupUi(this);
    ui->spinBoxObjects->setEnabled(false);
}

TraceDialog::~TraceDialog()
{
    delete ui;
}

void TraceDialog::reselect_ranks()
{
    ui->listWidgetRanks->clearSelection();
    int ranks = ui->spinBoxObjects->value();
    int rank = 0;
    for (std::map<PEAT_Clock, PEAT_Rank>::iterator rank_it = mpi_times_sorted.begin(); rank < ranks && rank_it != mpi_times_sorted.end(); ++rank_it, ++rank) {
        ui->listWidgetRanks->setCurrentRow(rank_it->second, QItemSelectionModel::Select);
    }
    rank = 0;
    for (std::map<PEAT_Clock, PEAT_Rank>::reverse_iterator rank_it = mpi_times_sorted.rbegin(); rank < ranks && rank_it != mpi_times_sorted.rend(); ++rank_it, ++rank) {
        ui->listWidgetRanks->setCurrentRow(rank_it->second, QItemSelectionModel::Select);
    }
    ui->spinBoxLoadThreads->setValue(std::min(ranks * 2, ui->spinBoxLoadThreads->value()));
}

void TraceDialog::reselect_nodes()
{
    ui->listWidgetNodes->clearSelection();
    int nodes = ui->spinBoxObjects->value();
    int node = 0;
    for (std::map<PEAT_Clock, PEAT_Node_Desc>::iterator node_it = mpi_times_node_sorted.begin(); node < nodes && node_it != mpi_times_node_sorted.end(); ++node_it, ++node) {
        ui->listWidgetNodes->setCurrentRow(node_it->second.number, QItemSelectionModel::Select);
    }
    node = 0;
    for (std::map<PEAT_Clock, PEAT_Node_Desc>::reverse_iterator node_it = mpi_times_node_sorted.rbegin(); node < nodes && node_it != mpi_times_node_sorted.rend(); ++node_it, ++node) {
        ui->listWidgetNodes->setCurrentRow(node_it->second.number, QItemSelectionModel::Select);
    }
}

void TraceDialog::set_propertiers(std::vector<PEAT_MPI_Time> &times, std::vector<PEAT_Node_Desc> &nodes, int tmin, int tmax, int load_threads, int render_threads, bool has_fab_files)
{
    ui->checkBoxLoadFabEvents->setEnabled(has_fab_files);
    if (!has_fab_files) {
        ui->checkBoxLoadFabEvents->setChecked(false);
    }
    mpi_times = times;
    for (int i = 0; i < mpi_times.size(); i++) {
        char str[1024];
        sprintf(str, "Rank %d - %lld %s", i, mpi_times[i].clocks, mpi_times[i].percentage.c_str());
        ui->listWidgetRanks->addItem(str);
        while (mpi_times_sorted.count(mpi_times[i].clocks) == 1)
            mpi_times[i].clocks++; // avoid overwriting
        mpi_times_sorted[mpi_times[i].clocks] = i;
    }
    mpi_node_times = nodes;
    ui->spinBoxLoadThreads->setMaximum(load_threads);
    ui->spinBoxRenderThreads->setMaximum(render_threads);
    ui->spinBoxLoadThreads->setValue(load_threads);
    for (std::vector<PEAT_Node_Desc>::iterator node_it = mpi_node_times.begin(); node_it != mpi_node_times.end(); ++node_it) {
        node_it->clocks = 0;
        for (std::vector<PEAT_Rank>::iterator rank_it = node_it->ranks.begin(); rank_it != node_it->ranks.end(); ++rank_it) {
            node_it->clocks += mpi_times.at(*rank_it).clocks;
        }
    }
    for (int i = 0; i < mpi_node_times.size(); i++) {
        char str[1024];
        sprintf(str, "Node %s - %lld", mpi_node_times[i].name.c_str(), mpi_node_times[i].clocks);
        ui->listWidgetNodes->addItem(str);
        while (mpi_times_node_sorted.count(mpi_node_times[i].clocks) == 1)
            mpi_node_times[i].clocks++; // avoid overwriting
        mpi_times_node_sorted[mpi_node_times[i].clocks] = mpi_node_times[i];
    }
    ui->horizontalSliderMin->setValue(tmin);
    ui->horizontalSliderMax->setValue(tmax);
    if (tmin != 0 && tmax != 100) {
        ui->horizontalSliderMin->setEnabled(false);
        ui->horizontalSliderMax->setEnabled(false);
    }
    ui->listWidgetGPUFunctions->setEnabled(false);

    ui->listWidgetRanks->selectAll();
    ui->listWidgetNodes->selectAll();
    ui->listWidgetNodes->hide();
    ui->listWidgetRanks->show();
}
PEAT_Rank get_rank_from_text(QString &str)
{
    QString str1 = str;
    str1.remove(QRegularExpression("Rank "));
    str1.remove(QRegularExpression("-.*"));
    return str1.toLongLong();
}
void TraceDialog::get_propertiers(std::set<PEAT_Rank> &ranks, int &tmin, int &tmax, int &load_threads, int &render_threads, TV_TraceParts &tp)
{
    ranks.clear();
    if (ui->radioButtonRanks->isChecked()) {
        QList<QListWidgetItem *> list = ui->listWidgetRanks->selectedItems();
        for (QList<QListWidgetItem *>::iterator list_it = list.begin(); list_it != list.end(); ++list_it) {
            int rank = ui->listWidgetRanks->row(*list_it);
            // QString str=(*list_it)->text();
            // ranks[get_rank_from_text(str)]=true;
            ranks.insert(rank);
        }
    } else {
        QList<QListWidgetItem *> list = ui->listWidgetNodes->selectedItems();
        for (QList<QListWidgetItem *>::iterator list_it = list.begin(); list_it != list.end(); ++list_it) {
            int node = ui->listWidgetNodes->row(*list_it);
            for (std::vector<PEAT_Rank>::iterator rank_it = mpi_node_times[node].ranks.begin(); rank_it != mpi_node_times[node].ranks.end(); ++rank_it) {
                ranks.insert(*rank_it);
            }
        }
    }
    tmin = ui->horizontalSliderMin->value();
    tmax = ui->horizontalSliderMax->value();
    load_threads = ui->spinBoxLoadThreads->value();
    render_threads = ui->spinBoxRenderThreads->value();

    int f;
    int c = ui->listWidgetMPIFunctions->count();

    for (f = 0; f < c; f++) {
        tp.mpi_functions[f] = ui->listWidgetMPIFunctions->item(f)->checkState();
    }

    c = ui->listWidgetMPIMessages->count();
    for (f = 0; f < c; f++) {
        tp.mpi_messages[f] = ui->listWidgetMPIMessages->item(f)->checkState();
    }

    c = ui->listWidgetOMPFunctions->count();
    for (f = 0; f < c; f++) {
        tp.omp_functions[f] = ui->listWidgetOMPFunctions->item(f)->checkState();
    }

    c = ui->listWidgetGPUFunctions->count();
    for (f = 0; f < c; f++) {
        tp.gpu_functions[f] = ui->listWidgetGPUFunctions->item(f)->checkState();
        ;
    }

    tp.fab_functions = ui->checkBoxLoadFabEvents->isChecked();
}

void TraceDialog::on_radioButtonRanks_clicked()
{
    ui->listWidgetNodes->hide();
    ui->listWidgetRanks->show();
}

void TraceDialog::on_radioButtonNodes_clicked()
{
    ui->listWidgetNodes->show();
    ui->listWidgetRanks->hide();
}

void TraceDialog::on_horizontalSliderMin_valueChanged(int value)
{
    if (value > ui->horizontalSliderMax->value())
        ui->horizontalSliderMax->setValue(value);
}

void TraceDialog::on_horizontalSliderMax_valueChanged(int value)
{
    if (value < ui->horizontalSliderMin->value())
        ui->horizontalSliderMin->setValue(value);
}

void TraceDialog::on_pushButtonRestore_pressed()
{
    ui->listWidgetNodes->selectAll();
    /*
    for (QList<QListWidgetItem *>::iterator list_it=list_nodes.begin();list_it!=list_nodes.end();++list_it ) {
        ui->listWidgetNodes->setCurrentItem(*list_it,QItemSelectionModel::ClearAndSelect);
    }
    */
    ui->listWidgetRanks->selectAll();
    /*
    for (QList<QListWidgetItem *>::iterator list_it=list_ranks.begin();list_it!=list_ranks.end();++list_it ) {
        ui->listWidgetRanks->setCurrentItem(*list_it,QItemSelectionModel::ClearAndSelect);
    }
    */
    ui->checkBoxFastSlow->setChecked(false);
    ui->spinBoxObjects->setEnabled(false);
}

void TraceDialog::on_checkBoxFastSlow_clicked(bool checked)
{
    if (ui->checkBoxFastSlow->isChecked()) {
        ui->spinBoxObjects->setEnabled(true);
        if (ui->radioButtonRanks->isChecked()) {
            list_ranks = ui->listWidgetRanks->selectedItems();
            reselect_ranks();
        } else {
            list_nodes = ui->listWidgetNodes->selectedItems();
            reselect_nodes();
        }
    } else {
        ui->spinBoxObjects->setEnabled(false);
        /*
        if (ui->radioButtonRanks->isChecked()) {
            ui->listWidgetNodes->clearSelection();
            for (QList<QListWidgetItem *>::iterator list_it=list_nodes.begin();list_it!=list_nodes.end();++list_it ) {
                ui->listWidgetNodes->setCurrentItem(*list_it,QItemSelectionModel::ClearAndSelect);
            }
        } else {
            ui->listWidgetRanks->clearSelection();
            for (QList<QListWidgetItem *>::iterator list_it=list_ranks.begin();list_it!=list_ranks.end();++list_it ) {
                ui->listWidgetRanks->setCurrentItem(*list_it,QItemSelectionModel::ClearAndSelect);
            }
        }*/
    }
}

void TraceDialog::on_spinBoxObjects_valueChanged(int arg1)
{
    reselect_ranks();
    reselect_nodes();
}
