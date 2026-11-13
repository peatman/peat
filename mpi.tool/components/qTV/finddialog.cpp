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

#include "finddialog.h"
#include "../common/func_id.h"
#include "ui_finddialog.h"
#include <QCompleter>

FindFunctionDialog::FindFunctionDialog(QWidget *parent)
    : QDialog(parent), ui(new Ui::FindFunctionDialog)
{
    ui->setupUi(this);
    // the names of all known functions are offered while typing
    QStringList names;
    for (const auto &entry : global_func_id.data()) {
        if (entry.second && entry.second->name && *entry.second->name)
            names << entry.second->name;
    }
    QCompleter *completer = new QCompleter(names, this);
    completer->setCaseSensitivity(Qt::CaseInsensitive);
    completer->setFilterMode(Qt::MatchContains);
    ui->editName->setCompleter(completer);

    connect(ui->editName, &QLineEdit::textChanged, this, &FindFunctionDialog::search_changed);
    connect(ui->comboRank, &QComboBox::currentIndexChanged, this, &FindFunctionDialog::search_changed);
    connect(ui->buttonPrevious, &QPushButton::clicked, this, [this]() { emit find_requested(-1); });
    connect(ui->buttonNext, &QPushButton::clicked, this, [this]() { emit find_requested(1); });
    connect(ui->checkDim, &QCheckBox::toggled, this, &FindFunctionDialog::dim_changed);
    // the dimming belongs to the dialog: it ends when the dialog is closed
    connect(this, &QDialog::finished, this, [this]() { ui->checkDim->setChecked(false); });
}

FindFunctionDialog::~FindFunctionDialog()
{
    delete ui;
}

void FindFunctionDialog::set_ranks(const std::vector<PEAT_Rank> &ranks)
{
    const QVariant old_rank = ui->comboRank->currentData();
    ui->comboRank->clear();
    ui->comboRank->addItem(tr("Any loaded rank"), -1);
    for (PEAT_Rank r : ranks) {
        ui->comboRank->addItem(tr("Rank %1").arg(r), r);
    }
    const int old_index = ui->comboRank->findData(old_rank);
    ui->comboRank->setCurrentIndex(old_index >= 0 ? old_index : 0);
}

QString FindFunctionDialog::function_name() const
{
    return ui->editName->text().trimmed();
}

PEAT_Rank FindFunctionDialog::rank() const
{
    return ui->comboRank->currentData().toInt();
}

bool FindFunctionDialog::dim_others() const
{
    return ui->checkDim->isChecked();
}

void FindFunctionDialog::set_status(const QString &text)
{
    ui->labelStatus->setText(text);
}

void FindFunctionDialog::open_for_search()
{
    show();
    raise();
    activateWindow();
    ui->editName->setFocus();
    ui->editName->selectAll();
}
