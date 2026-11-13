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
    spin_row_height = new QSpinBox(this);
    spin_row_height->setRange(25, 400);
    spin_row_height->setSingleStep(25);
    spin_row_height->setSuffix(tr(" %"));
    spin_row_height->setToolTip(tr("Height of a thread row; rows lower than a text line show no function names (Ctrl+Up / Ctrl+Down change it too)"));
    QHBoxLayout *height_layout = new QHBoxLayout;
    height_layout->addWidget(new QLabel(tr("Height of a row:"), this));
    height_layout->addWidget(spin_row_height);
    combo_color_mode = new QComboBox(this); // in the order of TV_Color_Mode
    combo_color_mode->addItem(tr("Kind of the event (the colours of the legend)"));
    combo_color_mode->addItem(tr("Function name"));
    combo_color_mode->addItem(tr("Communicator (grey: none)"));
    combo_color_mode->addItem(tr("Duration: blue 1 us ... red 1 s"));
    combo_color_mode->addItem(tr("Message size: blue 1 byte ... red 1 GB (grey: no data)"));
    QHBoxLayout *color_layout = new QHBoxLayout;
    color_layout->addWidget(new QLabel(tr("The colour of an event shows:"), this));
    color_layout->addWidget(combo_color_mode, 1);
    QDialogButtonBox *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    layout->addLayout(width_layout);
    layout->addLayout(height_layout);
    layout->addWidget(check_offscreen_messages);
    layout->addLayout(color_layout);
    layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

void DrawOptionsDialog::set_properties(const TV_TraceParts &shown)
{
    spin_func_min_width->setValue(shown.func_min_width);
    check_offscreen_messages->setChecked(shown.draw_offscreen_messages);
    spin_row_height->setValue(shown.row_height_percent);
    combo_color_mode->setCurrentIndex(qBound(0, shown.color_mode, TV_COLOR_MODE_MAX - 1));
}

void DrawOptionsDialog::get_properties(TV_TraceParts &shown) const
{
    shown.func_min_width = spin_func_min_width->value();
    shown.draw_offscreen_messages = check_offscreen_messages->isChecked();
    shown.row_height_percent = spin_row_height->value();
    shown.color_mode = combo_color_mode->currentIndex();
}
