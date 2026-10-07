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

#ifndef __TVMAINWINDOW_H__
#define __TVMAINWINDOW_H__

#include "qTV.h"
#include <QApplication>
#include <QActionGroup>
#include <QCheckBox>
#include <QColor>
#include <QColorDialog>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QMainWindow>
#include <QMenu>
#include <QMenuBar>
#include <QPainter>
#include <QProgressBar>
#include <QProgressDialog>
#include <QPushButton>
#include <QScreen>
#include <QScrollArea>
#include <QScrollBar>
#include <QSpinBox>
#include <QStatusBar>
#include <QToolBar>
#include <QToolButton>
#include <QVBoxLayout>
#include <QtDebug>
#include <QtGui>
#define DIALOG_WIDTH_PERCENT 0.7

QT_BEGIN_NAMESPACE
namespace Ui
{
class TVMainWindow;
}
QT_END_NAMESPACE

class TVMainWindow : public QMainWindow
{
    Q_OBJECT

  public:
    TVMainWindow(QWidget *parent = nullptr);
    ~TVMainWindow();
    TV *tv;
  private slots:
    void load_data();
    void snap_shot();
    void reload_data();
    void open_recent_file();
    void clear_recent_files();
    void about_dialog();
    void toggle_presentation_mode(bool checked);
    void select_presentation_color();
    void select_presentation_curve_tool();
    void select_presentation_line_tool();
    void select_presentation_rectangle_tool();
    void select_presentation_ellipse_tool();
    void select_presentation_arrow_tool();
    void save_presentation_picture();
    void undo_presentation_action();
    void redo_presentation_action();
    void update_presentation_undo_redo(bool can_undo, bool can_redo);

    void on_actionReset_triggered();

    void on_actionCollLeast_BW_triggered();

    void on_actionCollMost_Imbalance_triggered();

    void on_actionP2PLeast_BW_triggered();

    void on_actionP2PMost_Imbalance_triggered();

    void on_actionZoomIn_triggered();

    void on_actionZoomOut_triggered();

    void on_actionReDraw_triggered();

    void on_actionFind_triggered();

    void on_actionLocalsFunc_triggered(bool checked);

    void on_actionP2PFunc_triggered(bool checked);

    void on_actionCollectivesFunc_triggered(bool checked);

    void on_actionRequestsFunc_triggered(bool checked);

    void on_actionCollectivesMessages_triggered(bool checked);

    void on_actionP2PMessages_triggered(bool checked);

    void on_actionInitiationMessages_triggered(bool checked);

    void on_actionCompletionsMessages_triggered(bool checked);

    void on_horizontalScrollBar_sliderReleased();

    void on_actionLegend_triggered();

  protected:
    void keyPressEvent(QKeyEvent *);
    void keyReleaseEvent(QKeyEvent *);
    bool event(QEvent *e);

  private:
    void disable_all_actions();
    void enable_all_actions();
    void update_recent_file_actions();
    void add_to_recent_files(const QString &file_name);
    void load_recent_files();
    void save_recent_files();
    void proceed_load_data();
    bool offer_save_presentation();
    void update_presentation_color_icon();
    void save_presentation_color();
    static QIcon make_presentation_tool_icon(const QString &kind);

    Ui::TVMainWindow *ui;
    QScrollArea *sa;
    QProgressBar *pb;
    QScrollBar *sr;
    QLabel *label_current_time;
    QLabel *label_start_time;
    QLabel *label_end_time;
    QLabel *label_hidden_elements;
    QLabel *label_ctrl;
    QLabel *label_alt;
    QLabel *label_shift;
    QLabel *label_candidate;
    Trace_Info info;
    QFileInfo trace_file;
    TV_TraceParts tp;
    TV_TraceParts shown;
    static constexpr int max_recent_files = 10;
    QAction *recent_file_actions[max_recent_files];
    QAction *clear_recent_files_action;
    QAction *about_action;
    QAction *exit_action;
    QAction *presentation_action;
    QAction *presentation_color_action;
    QAction *presentation_curve_action;
    QAction *presentation_line_action;
    QAction *presentation_rectangle_action;
    QAction *presentation_ellipse_action;
    QAction *presentation_arrow_action;
    QAction *presentation_save_action;
    QAction *presentation_undo_action;
    QAction *presentation_redo_action;
    QActionGroup *presentation_tool_group;
    QColor presentation_current_color;
    QList<QAction *> presentation_saved_actions;

    QStringList recent_files;
};
bool get_file_info(const QString &user_file, QFileInfo &trace_file);

class Model_File_Load_Form : public QDialog
{
    Q_OBJECT
  public:
    explicit Model_File_Load_Form(QWidget *parent);
    // bool get_file_info( const QString& userFile, QFileInfo& traceinfoFile );
    void getNames(QFileInfo &names)
    {
        names = trace_info_file;
    }
  private slots:
    void select_trace_file();
    void accept();

  private:
    // select files line
    QVBoxLayout *v_main_layout;
    QLineEdit *l_trace_file;
    QPushButton *b_open_file;
    // form bottons
    QDialogButtonBox *b_form;
    QFileInfo trace_info_file;
    // Pcontrol
};

#endif // __TVMAINWINDOW_H__
