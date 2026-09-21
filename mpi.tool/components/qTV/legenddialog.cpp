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

#include "legenddialog.h"
#include "QColorDialog"
#include "ui_legenddialog.h"

LegendDialog::LegendDialog(QWidget *parent)
    : QDialog(parent), ui(new Ui::LegendDialog)
{
    ui->setupUi(this);
    message_items.insert("Other", TV_PEN_MESSAGE_TYPE_NONE);
    message_items.insert("Normal", TV_PEN_MESSAGE_TYPE_NORMAL);
    message_items.insert("Reversed", TV_PEN_MESSAGE_TYPE_REVERSED);
    message_items.insert("Initiation", TV_PEN_MESSAGE_TYPE_INITIATION);
    message_items.insert("Completion", TV_PEN_MESSAGE_TYPE_COMPLETION);
    message_items.insert("Collective", TV_PEN_MESSAGE_TYPE_COLLECTIVE);
    message_items.insert("GPU H/D Transfer", TV_PEN_MESSAGE_TYPE_GPU_HD_TRANSFER);
    message_items.insert("GPU Exec", TV_PEN_MESSAGE_TYPE_GPU_EXEC);

    event_items.insert("User Code", TV_BRUSH_EVENT_TYPE_USER_CODE);
    event_items.insert("Source", TV_BRUSH_EVENT_TYPE_SOURCE);
    event_items.insert("Destionation", TV_BRUSH_EVENT_TYPE_DESTINATION);
    event_items.insert("Source and Destination", TV_BRUSH_EVENT_TYPE_SAND);
    event_items.insert("Collective", TV_BRUSH_EVENT_TYPE_COLLECTIVE);
    event_items.insert("Start", TV_BRUSH_EVENT_TYPE_START);
    event_items.insert("Completion", TV_BRUSH_EVENT_TYPE_COMPLETION);
    event_items.insert("OpenMP Parallel", TV_BRUSH_EVENT_TYPE_OMPT_PARALLEL);
    event_items.insert("OpenMP Local", TV_BRUSH_EVENT_TYPE_OMPT_LOCAL);
    event_items.insert("GPU Transfer", TV_BRUSH_EVENT_TYPE_GPU);
    event_items.insert("GPU Other", TV_BRUSH_EVENT_TYPE_OTHER);
    event_items.insert("PE Part", TV_BRUSH_EVENT_TYPE_PE);
    event_items.insert("Fabric", TV_BRUSH_EVENT_TYPE_FAB);
    connect(this, SIGNAL(show_color_dialog(QListWidget *)), this, SLOT(color_dialog(QListWidget *)));
}

LegendDialog::~LegendDialog()
{
    delete ui;
}

void LegendDialog::eventsListWidget_item(QListWidgetItem *item)
{
    int item_num = event_items.value(item->text());
    QColorDialog d;
    d.setCurrentColor(colors.event_brushes[item_num].color());
    if (d.exec() == QDialog::Accepted) {
        QColor color = d.currentColor();
        colors.event_brushes[item_num].setColor(color);
        qint16 alpha = color.alpha();
        item->setBackground(color);
        QRgba64 rgba = QRgba64::fromRgba64(color.rgba64());
        quint64 rgba64 = rgba;
        rgba64 ^= -1;
        color.setRgba64(rgba.fromRgba64(rgba64));
        color.setAlpha(alpha);
        item->setForeground(color);
    }
    item->setSelected(false);
}
void LegendDialog::messageListWidget_item(QListWidgetItem *item)
{
    int item_num = message_items.value(item->text());
    QColorDialog d;
    d.setCurrentColor(colors.message_pens[item_num].color());
    if (d.exec() == QDialog::Accepted) {
        QColor color = d.currentColor();
        colors.message_pens[item_num].setColor(color);
        qint16 alpha = color.alpha();
        item->setBackground(color);
        QRgba64 rgba = QRgba64::fromRgba64(color.rgba64());
        quint64 rgba64 = rgba;
        rgba64 ^= -1;
        color.setRgba64(rgba.fromRgba64(rgba64));
        color.setAlpha(alpha);
        item->setForeground(color);
    }
    item->setSelected(false);
}

void LegendDialog::set_properties(TV_Colors &c)
{
    colors = c;
    int index;
    for (QMap<QString, int>::iterator item = event_items.begin(); item != event_items.end(); ++item) {
        QListWidgetItem *list_item = new QListWidgetItem;
        list_item->setText(item.key());
        QColor color = colors.event_brushes[item.value()].color();
        qint16 alpha = color.alpha();
        list_item->setBackground(color);
        QRgba64 rgba = QRgba64::fromRgba64(color.rgba64());
        quint64 rgba64 = rgba;
        rgba64 ^= -1;
        color.setRgba64(rgba.fromRgba64(rgba64));
        color.setAlpha(alpha);
        list_item->setForeground(color);
        ui->eventsListWidget->addItem(list_item);
    }
    for (QMap<QString, int>::iterator item = message_items.begin(); item != message_items.end(); ++item) {
        QListWidgetItem *list_item = new QListWidgetItem;
        list_item->setText(item.key());
        QColor color = colors.message_pens[item.value()].color();
        qint16 alpha = color.alpha();
        list_item->setBackground(color);
        QRgba64 rgba = QRgba64::fromRgba64(color.rgba64());
        quint64 rgba64 = rgba;
        rgba64 ^= -1;
        color.setRgba64(rgba.fromRgba64(rgba64));
        color.setAlpha(alpha);
        list_item->setForeground(color);
        ui->messagesListWidget->addItem(list_item);
    }
}
void LegendDialog::get_properties(TV_Colors &c)
{
    c = colors;
}
/*
void LegendDialog::on_eventListWidget_itemClicked(QListWidgetItem *item)
{
    eventListWidget_item(item);
}
*/

void LegendDialog::on_eventsListWidget_itemDoubleClicked(QListWidgetItem *item)
{
    eventsListWidget_item(item);
}

/*
void LegendDialog::on_eventsListWidget_itemPressed(QListWidgetItem *item)
{
    eventsListWidget_item(item);
}
*/
/*
void LegendDialog::on_messagesListWidget_itemClicked(QListWidgetItem *item)
{
    messageListWidget_item(item);
}
*/
void LegendDialog::on_messagesListWidget_itemDoubleClicked(QListWidgetItem *item)
{
    messageListWidget_item(item);
}

/*
void LegendDialog::on_messagesListWidget_itemPressed(QListWidgetItem *item)
{
    messageListWidget_item(item);
}
*/

void LegendDialog::keyPressEvent(QKeyEvent *e)
{
    switch (e->type()) {
    case QEvent::KeyPress:
        switch (e->key()) {
        case Qt::Key_Space:
            break;
        default:
            break;
        }
    }
    QDialog::keyPressEvent(e);
}

void LegendDialog::color_dialog(QListWidget *w)
{
    QListWidgetItem *item = w->currentItem();
    if (w == ui->eventsListWidget) {
        eventsListWidget_item(item);
        return;
    }
    if (w == ui->messagesListWidget) {
        messageListWidget_item(item);
        return;
    }
}
