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

#include "highlightdialog.h"
#include "../common/func_id.h"
#include "ui_highlightdialog.h"
#include <QCompleter>
#include <QMessageBox>
#include <QPushButton>

HighlightDialog::HighlightDialog(QWidget *parent)
    : QDialog(parent), ui(new Ui::HighlightDialog)
{
    ui->setupUi(this);
    QStringList names;
    for (const auto &entry : global_func_id.data()) {
        if (entry.second && entry.second->name && *entry.second->name)
            names << entry.second->name;
    }
    QCompleter *completer = new QCompleter(names, this);
    completer->setCaseSensitivity(Qt::CaseInsensitive);
    completer->setFilterMode(Qt::MatchContains);
    ui->editFunction->setCompleter(completer);
    connect(ui->buttonBox, &QDialogButtonBox::accepted, this, &HighlightDialog::accept);
    connect(ui->buttonBox->button(QDialogButtonBox::Reset), &QPushButton::clicked, this, &HighlightDialog::reset);
}

HighlightDialog::~HighlightDialog()
{
    delete ui;
}

void HighlightDialog::set_properties(const TV_TraceParts &shown, const Communicators *communicators)
{
    ui->editFunction->setText(shown.highlight_func != 0 ? QString(global_func_id.name_by_idx(PEAT_MPI_Func_ID(shown.highlight_func))) : QString());
    QComboBox *comms = ui->comboComm;
    comms->clear();
    comms->addItem(tr("any communicator"), 0); // 0 is MPI_COMM_NULL: no event has it
    // predefined communicators are not listed in the trace: 2 is MPI_COMM_WORLD, 1 is MPI_COMM_SELF
    comms->addItem(tr("2 (comm_world)"), 2);
    comms->addItem(tr("1 (comm_self)"), 1);
    if (communicators) {
        for (const auto &entry : communicators->comms) {
            const Communicators::Communicator_Info &info = entry.second;
            comms->addItem(QString::number(info.comm_id, 16) + (info.is_internal ? tr(" (MPI internal)") : QString()), info.comm_id);
        }
    }
    int index = comms->findData(shown.highlight_comm);
    if (index < 0) { // set earlier, but not in the list now (the communicators could not be read)
        comms->addItem(QString::number(shown.highlight_comm, 16), shown.highlight_comm);
        index = comms->count() - 1;
    }
    comms->setCurrentIndex(index);
    // the size is shown in the biggest unit that holds it exactly
    long long bytes = shown.highlight_min_bytes > 0 ? shown.highlight_min_bytes : 0;
    int unit = 0;
    while (unit < 2 && bytes > 0 && bytes % 1024 == 0) {
        bytes /= 1024;
        ++unit;
    }
    ui->spinSize->setValue(int(qMin<long long>(bytes, ui->spinSize->maximum())));
    ui->comboUnit->setCurrentIndex(unit);
}

void HighlightDialog::get_properties(TV_TraceParts &shown) const
{
    const QString name = ui->editFunction->text().trimmed();
    shown.highlight_func = name.isEmpty() ? 0 : int(global_func_id.idx_by_name(name.toLatin1().constData()));
    shown.highlight_comm = ui->comboComm->currentData().toInt();
    long long bytes = ui->spinSize->value();
    for (int unit = 0; unit < ui->comboUnit->currentIndex(); ++unit)
        bytes *= 1024;
    shown.highlight_min_bytes = bytes;
}

void HighlightDialog::accept()
{
    const QString name = ui->editFunction->text().trimmed();
    if (!name.isEmpty() && global_func_id.idx_by_name(name.toLatin1().constData()) == TMPI_FUNC_ID_NONE) {
        QMessageBox::warning(this, windowTitle(), tr("Unknown function name: %1").arg(name));
        return;
    }
    QDialog::accept();
}

void HighlightDialog::reset()
{
    ui->editFunction->clear();
    ui->comboComm->setCurrentIndex(0);
    ui->spinSize->setValue(0);
    ui->comboUnit->setCurrentIndex(0);
}
