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

#ifndef __LEGENDDIALOG_H__
#define __LEGENDDIALOG_H__

#include "../common/common_types.h"
#include "globals.h"
#include <QDialog>
#include <QKeyEvent>
#include <QListWidget>
#include <QListWidgetItem>

namespace Ui
{
class LegendDialog;
}
class LegendDialog : public QDialog
{
    Q_OBJECT

  public:
    explicit LegendDialog(QWidget *parent = nullptr);
    ~LegendDialog();
    void set_properties(TV_Colors &c);
    void get_properties(TV_Colors &c);

  private slots:
    // void on_eventListWidget_itemClicked(QListWidgetItem *item);
    // void on_messagesListWidget_itemClicked(QListWidgetItem *item);
    void on_eventsListWidget_itemDoubleClicked(QListWidgetItem *item);
    // void on_eventsListWidget_itemPressed(QListWidgetItem *item);
    void on_messagesListWidget_itemDoubleClicked(QListWidgetItem *item);
    // void on_messagesListWidget_itemPressed(QListWidgetItem *item);

  public slots:
    void color_dialog(QListWidget *w);

  protected:
    virtual void keyPressEvent(QKeyEvent *e) override;

  private:
    Ui::LegendDialog *ui;
    TV_Colors colors;
    QMap<QString, int> event_items;
    QMap<QString, int> message_items;
    void eventsListWidget_item(QListWidgetItem *item);
    void messageListWidget_item(QListWidgetItem *item);
  signals:
    void show_color_dialog(QListWidget *w);
};

#endif // __LEGENDDIALOG_H__
