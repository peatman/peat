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

#include "drawoptionsdialog.h"
#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QVBoxLayout>

DrawOptionsDialog::DrawOptionsDialog(QWidget *parent) : QDialog(parent)
{
    setWindowTitle(tr("Drawing options"));
    QVBoxLayout *layout = new QVBoxLayout(this);
    spin_func_min_width = new QSpinBox(this);
    spin_func_min_width->setRange(1, 1000);
    spin_func_min_width->setSuffix(tr(" px"));
    spin_func_min_width->setToolTip(tr("Functions narrower than this are not drawn (1 draws all of them)"));
    QHBoxLayout *width_layout = new QHBoxLayout;
    width_layout->addWidget(new QLabel(tr("Minimal width of a function to be drawn:"), this));
    width_layout->addWidget(spin_func_min_width);
    check_offscreen_messages = new QCheckBox(tr("Always draw messages, even if their bounds are outside the window boundaries"), this);
    QDialogButtonBox *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    layout->addLayout(width_layout);
    layout->addWidget(check_offscreen_messages);
    layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

void DrawOptionsDialog::set_properties(const TV_TraceParts &shown)
{
    spin_func_min_width->setValue(shown.func_min_width);
    check_offscreen_messages->setChecked(shown.draw_offscreen_messages);
}

void DrawOptionsDialog::get_properties(TV_TraceParts &shown) const
{
    shown.func_min_width = spin_func_min_width->value();
    shown.draw_offscreen_messages = check_offscreen_messages->isChecked();
}
