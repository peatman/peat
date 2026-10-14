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

#ifndef __TRACEDIALOG_H__
#define __TRACEDIALOG_H__

#include "../common/common_types.h"
#include "globals.h"
#include <QDialog>
#include <QList>
#include <QListWidgetItem>

namespace Ui
{
class Dialog;
}

class TraceDialog : public QDialog
{
    Q_OBJECT

  public:
    explicit TraceDialog(QWidget *parent = nullptr);
    ~TraceDialog();
    void set_propertiers(std::vector<PEAT_MPI_Time> &times, std::vector<PEAT_Node_Desc> &nodes, int tmin, int tmax, int load_threads, int render_threads, bool has_transport_files);
    void get_propertiers(std::set<PEAT_Rank> &ranks, int &tmin, int &tmax, int &load_threads, int &render_threads, TV_TraceParts &tp);
  private slots:

    void on_radioButtonRanks_clicked();

    void on_radioButtonNodes_clicked();

    void on_horizontalSliderMin_valueChanged(int value);

    void on_horizontalSliderMax_valueChanged(int value);

    void on_pushButtonRestore_pressed();

    void on_checkBoxFastSlow_clicked(bool checked);

    void on_spinBoxObjects_valueChanged(int arg1);

  private:
    Ui::Dialog *ui;
    std::vector<PEAT_MPI_Time> mpi_times;
    std::vector<PEAT_Node_Desc> mpi_node_times;
    std::map<PEAT_Clock, PEAT_Rank> mpi_times_sorted;
    std::map<PEAT_Clock, PEAT_Node_Desc> mpi_times_node_sorted;
    QList<QListWidgetItem *> list_ranks;
    QList<QListWidgetItem *> list_nodes;
    void reselect_ranks();
    void reselect_nodes();
};

#endif // __TRACEDIALOG_H__
