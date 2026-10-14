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

#include "tvmainwindow.h"
#include "./ui_tvmainwindow.h"
#include "cutoffdialog.h"
#include "globals.h"
#include "legenddialog.h"
#include "textviewer.h"
#include "tracedialog.h"
#include "trace_info.h"
#include <QMessageBox>
#include <QPalette>

// Loads the SVG icon for the given graphics-editor tool/action ("presentation",
// "curve", "line", "rectangle", "ellipse" or "arrow") from the icons.qrc
// resource, picking the light or dark variant based on the application's
// current palette (window background lightness).
QIcon TVMainWindow::make_presentation_tool_icon(const QString &kind)
{
    const bool dark_mode = QApplication::palette().color(QPalette::Window).lightness() < 128;
    const QString path = QString(":/icons/%1/%2.svg").arg(dark_mode ? "dark" : "light", kind);
    return QIcon(path);
}

TVMainWindow::TVMainWindow(QWidget *parent)
    : QMainWindow(parent), ui(new Ui::TVMainWindow)
{
    get_cpu_freq();
    ui->setupUi(this);
    // sa = new QScrollArea(this);
    sa = ui->scrollArea;
    // pb = new QProgressBar;
    pb = ui->progressBar;
    // sr = new QSlider(Qt::Horizontal);
    // sr = new QScrollBar(Qt::Horizontal, statusBar());
    sr = ui->horizontalScrollBar;
    pb->hide();
    sr->hide();
    statusBar()->addWidget(pb, 1);
    statusBar()->addWidget(sr, 1);
    tv = new TV(sa, statusBar(), pb, sr, &shown, this);
    sa->setWidget(tv);
    sa->setWidgetResizable(true);
    sa->setBackgroundRole(QPalette::Dark);
    setCentralWidget(sa);
    label_current_time = new QLabel(outputDouble(0.0));
    label_current_time->setMargin(3);
    label_start_time = new QLabel(outputDouble(0.0));
    label_start_time->setMargin(3);
    label_end_time = new QLabel(outputDouble(0.0));
    label_end_time->setMargin(3);
    label_hidden_elements = new QLabel("0");
    label_hidden_elements->setStyleSheet("QLabel { color : red; }");
    label_hidden_elements->setMargin(3);
    label_ctrl = new QLabel("Ctrl");
    label_ctrl->setMargin(3);
    label_alt = new QLabel("Alt");
    label_alt->setMargin(3);
    label_shift = new QLabel("Shift");
    label_shift->setMargin(3);
    label_candidate = new QLabel("0");
    label_candidate->setStyleSheet("QLabel { color : green; }");
    label_candidate->setMargin(3);
    ui->toolBar->addWidget(label_current_time);
    ui->toolBar->addSeparator();
    ui->toolBar->addWidget(label_start_time);
    ui->toolBar->addSeparator();
    ui->toolBar->addWidget(label_end_time);
    ui->toolBar->addSeparator();
    QAction *hidden_elements_action = ui->toolBar->addWidget(label_hidden_elements);
    Q_UNUSED(hidden_elements_action);
    ui->statusBar->addPermanentWidget(label_candidate);
    ui->statusBar->addPermanentWidget(label_ctrl);
    ui->statusBar->addPermanentWidget(label_alt);
    ui->statusBar->addPermanentWidget(label_shift);

    exit_action = new QAction(tr("E&xit"), this);
    exit_action->setShortcuts(QKeySequence::Quit);
    exit_action->setStatusTip(tr("Exit the application"));
    connect(exit_action, &QAction::triggered, this, &QWidget::close);

    // Standard Qt icons for the existing (non-presentation) toolbar actions.
    ui->actionLoad->setIcon(style()->standardIcon(QStyle::SP_DialogOpenButton));
    ui->actionReLoad->setIcon(style()->standardIcon(QStyle::SP_BrowserReload));
    ui->actionSnapshot->setIcon(style()->standardIcon(QStyle::SP_DialogSaveButton));
    ui->actionReset->setIcon(style()->standardIcon(QStyle::SP_DialogResetButton));
    ui->actionReDraw->setIcon(style()->standardIcon(QStyle::SP_BrowserReload));
    ui->actionZoomIn->setIcon(make_presentation_tool_icon("zoom_in"));
    ui->actionZoomOut->setIcon(make_presentation_tool_icon("zoom_out"));
    ui->actionFind->setIcon(style()->standardIcon(QStyle::SP_FileDialogContentsView));
    ui->actionLegend->setIcon(style()->standardIcon(QStyle::SP_FileDialogDetailedView));

    connect(tv, SIGNAL(show_current_time(QString)), label_current_time, SLOT(setText(QString)));
    connect(tv, SIGNAL(show_start_time(QString)), label_start_time, SLOT(setText(QString)));
    connect(tv, SIGNAL(show_end_time(QString)), label_end_time, SLOT(setText(QString)));
    connect(tv, SIGNAL(show_invisible_items_number(QString)), label_hidden_elements, SLOT(setText(QString)));
    connect(tv, SIGNAL(show_candidate_number(QString)), label_candidate, SLOT(setText(QString)));
    connect(tv, SIGNAL(show_ctrl_key(bool)), label_ctrl, SLOT(setDisabled(bool)));
    connect(tv, SIGNAL(show_alt_key(bool)), label_alt, SLOT(setDisabled(bool)));
    connect(tv, SIGNAL(show_shift_key(bool)), label_shift, SLOT(setDisabled(bool)));

    for (int i = 0; i < max_recent_files; ++i) {
        recent_file_actions[i] = new QAction(this);
        recent_file_actions[i]->setVisible(false);
        connect(recent_file_actions[i], &QAction::triggered, this, &TVMainWindow::open_recent_file);
    }
    clear_recent_files_action = new QAction(tr("Clear Recent Files"), this);
    clear_recent_files_action->setStatusTip(tr("Clear the recent files list"));
    connect(clear_recent_files_action, &QAction::triggered, this, &TVMainWindow::clear_recent_files);

    about_action = new QAction(tr("&About"), this);
    about_action->setStatusTip(tr("About this application"));
    connect(about_action, &QAction::triggered, this, &TVMainWindow::about_dialog);

    presentation_action = new QAction(tr("&Presentation"), this);
    presentation_action->setCheckable(true);
    presentation_action->setChecked(false);
    presentation_action->setIcon(make_presentation_tool_icon("presentation"));
    presentation_action->setStatusTip(tr("Toggle presentation (annotation) mode: draw curves, lines and rectangles over the picture"));
    presentation_action->setToolTip(tr("Presentation mode: draw over the current picture with the graphics editor tools"));
    connect(presentation_action, &QAction::toggled, this, &TVMainWindow::toggle_presentation_mode);
    // Presentation mode requires a loaded trace; disabled until one is loaded.
    presentation_action->setEnabled(false);
    // Presentation has to be placed right after the Find action/separator.
    QAction *insert_before_presentation = nullptr;
    {
        const QList<QAction *> actions = ui->toolBar->actions();
        const int find_index = actions.indexOf(ui->actionFind);
        if (find_index >= 0 && find_index + 1 < actions.size()) {
            insert_before_presentation = actions.at(find_index + 1);
        }
    }
    ui->toolBar->insertAction(insert_before_presentation, presentation_action);
    QAction *presentation_separator = ui->toolBar->insertSeparator(insert_before_presentation);

    // Graphics-editor toolbar actions: only visible/enabled while in presentation mode.
    {
        QSettings settings;
        presentation_current_color = settings.value("presentation/color", QColor(Qt::red)).value<QColor>();
        if (!presentation_current_color.isValid()) {
            presentation_current_color = Qt::red;
        }
    }
    presentation_color_action = new QAction(tr("Color"), this);
    presentation_color_action->setStatusTip(tr("Pick the drawing color"));
    connect(presentation_color_action, &QAction::triggered, this, &TVMainWindow::select_presentation_color);

    presentation_curve_action = new QAction(tr("Curve"), this);
    presentation_curve_action->setCheckable(true);
    presentation_curve_action->setChecked(true);
    presentation_curve_action->setIcon(make_presentation_tool_icon("curve"));
    presentation_curve_action->setStatusTip(tr("Free-hand curve tool"));
    connect(presentation_curve_action, &QAction::triggered, this, &TVMainWindow::select_presentation_curve_tool);

    presentation_line_action = new QAction(tr("Line"), this);
    presentation_line_action->setCheckable(true);
    presentation_line_action->setIcon(make_presentation_tool_icon("line"));
    presentation_line_action->setStatusTip(tr("Line tool"));
    connect(presentation_line_action, &QAction::triggered, this, &TVMainWindow::select_presentation_line_tool);

    presentation_rectangle_action = new QAction(tr("Rectangle"), this);
    presentation_rectangle_action->setCheckable(true);
    presentation_rectangle_action->setIcon(make_presentation_tool_icon("rectangle"));
    presentation_rectangle_action->setStatusTip(tr("Rectangle tool"));
    connect(presentation_rectangle_action, &QAction::triggered, this, &TVMainWindow::select_presentation_rectangle_tool);

    presentation_ellipse_action = new QAction(tr("Ellipse"), this);
    presentation_ellipse_action->setCheckable(true);
    presentation_ellipse_action->setIcon(make_presentation_tool_icon("ellipse"));
    presentation_ellipse_action->setStatusTip(tr("Ellipse/circle tool"));
    connect(presentation_ellipse_action, &QAction::triggered, this, &TVMainWindow::select_presentation_ellipse_tool);

    presentation_arrow_action = new QAction(tr("Arrow"), this);
    presentation_arrow_action->setCheckable(true);
    presentation_arrow_action->setIcon(make_presentation_tool_icon("arrow"));
    presentation_arrow_action->setStatusTip(tr("Arrow tool"));
    connect(presentation_arrow_action, &QAction::triggered, this, &TVMainWindow::select_presentation_arrow_tool);

    presentation_tool_group = new QActionGroup(this);
    presentation_tool_group->addAction(presentation_curve_action);
    presentation_tool_group->addAction(presentation_line_action);
    presentation_tool_group->addAction(presentation_rectangle_action);
    presentation_tool_group->addAction(presentation_ellipse_action);
    presentation_tool_group->addAction(presentation_arrow_action);

    presentation_save_action = new QAction(tr("Save"), this);
    presentation_save_action->setIcon(style()->standardIcon(QStyle::SP_DialogSaveButton));
    presentation_save_action->setStatusTip(tr("Save the current presentation picture to a file"));
    connect(presentation_save_action, &QAction::triggered, this, &TVMainWindow::save_presentation_picture);

    presentation_undo_action = new QAction(tr("Undo"), this);
    presentation_undo_action->setIcon(make_presentation_tool_icon("undo"));
    presentation_undo_action->setStatusTip(tr("Undo the last drawing action"));
    presentation_undo_action->setShortcut(QKeySequence::Undo);
    presentation_undo_action->setEnabled(false);
    connect(presentation_undo_action, &QAction::triggered, this, &TVMainWindow::undo_presentation_action);

    presentation_redo_action = new QAction(tr("Redo"), this);
    presentation_redo_action->setIcon(make_presentation_tool_icon("redo"));
    presentation_redo_action->setStatusTip(tr("Redo the last undone drawing action"));
    presentation_redo_action->setShortcut(QKeySequence::Redo);
    presentation_redo_action->setEnabled(false);
    connect(presentation_redo_action, &QAction::triggered, this, &TVMainWindow::redo_presentation_action);

    connect(tv, &TV::presentation_undo_redo_changed, this, &TVMainWindow::update_presentation_undo_redo);

    ui->toolBar->insertAction(presentation_separator, presentation_color_action);
    ui->toolBar->insertAction(presentation_separator, presentation_curve_action);
    ui->toolBar->insertAction(presentation_separator, presentation_line_action);
    ui->toolBar->insertAction(presentation_separator, presentation_rectangle_action);
    ui->toolBar->insertAction(presentation_separator, presentation_ellipse_action);
    ui->toolBar->insertAction(presentation_separator, presentation_arrow_action);
    ui->toolBar->insertAction(presentation_separator, presentation_undo_action);
    ui->toolBar->insertAction(presentation_separator, presentation_redo_action);
    ui->toolBar->insertAction(presentation_separator, presentation_save_action);

    presentation_color_action->setVisible(false);
    presentation_curve_action->setVisible(false);
    presentation_line_action->setVisible(false);
    presentation_rectangle_action->setVisible(false);
    presentation_ellipse_action->setVisible(false);
    presentation_arrow_action->setVisible(false);
    presentation_undo_action->setVisible(false);
    presentation_redo_action->setVisible(false);
    presentation_save_action->setVisible(false);

    update_presentation_color_icon();

    load_recent_files();
    update_recent_file_actions();
    QMenu *fileMenu = ui->menuFile;
    for (int i = 0; i < max_recent_files; ++i) {
        ui->menuFile->addAction(recent_file_actions[i]);
    }

    fileMenu->addAction(clear_recent_files_action);
    fileMenu->addSeparator();
    fileMenu->addAction(exit_action);
    QMenu *helpMenu = menuBar()->addMenu(tr("&Help"));
    helpMenu->addAction(about_action);
}

TVMainWindow::~TVMainWindow()
{
    save_presentation_color();
    delete ui;
}
void TVMainWindow::snap_shot()
{
    const QString file_name = QFileDialog::getSaveFileName(this, tr("Save PNG image"), "untitled.png", tr("Image Files (*.png);;All files (*.*)"));
    bool r = tv->save(file_name, "PNG", -1);
}

bool TVMainWindow::offer_save_presentation()
{
    if (!tv || !tv->has_presentation_annotations()) {
        return true;
    }
    QMessageBox::StandardButton answer = QMessageBox::question(
        this, tr("Save Presentation"),
        tr("Do you want to save the current presentation picture before exiting presentation mode?"),
        QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel, QMessageBox::Yes);
    if (answer == QMessageBox::Cancel) {
        return false;
    }
    if (answer == QMessageBox::Yes) {
        const QString file_name = QFileDialog::getSaveFileName(this, tr("Save PNG image"), "presentation.png", tr("Image Files (*.png);;All files (*.*)"));
        if (!file_name.isEmpty()) {
            tv->save_current_view(file_name, "PNG", -1);
        }
    }
    return true;
}

void TVMainWindow::toggle_presentation_mode(bool checked)
{
    if (!tv) {
        return;
    }
    if (checked) {
        // Remove every toolbar action/widget that is not related to the
        // graphics editor, remembering them (in order, along with whether
        // they used to sit before or after the presentation toggle button)
        // so they can be restored exactly as they were once presentation
        // mode is left.
        ui->menuBar->setDisabled(true);
        presentation_saved_actions.clear();
        bool before_presentation = true;
        for (QAction *action : ui->toolBar->actions()) {
            if (action == presentation_action) {
                before_presentation = false;
                continue;
            }
            if (action != presentation_color_action && action != presentation_curve_action && action != presentation_line_action
                && action != presentation_rectangle_action && action != presentation_ellipse_action && action != presentation_arrow_action
                && action != presentation_undo_action && action != presentation_redo_action && action != presentation_save_action) {
                action->setProperty("presentation_before", before_presentation);
                presentation_saved_actions.append(action);
                ui->toolBar->removeAction(action);
            }
        }
        presentation_color_action->setVisible(true);
        presentation_curve_action->setVisible(true);
        presentation_line_action->setVisible(true);
        presentation_rectangle_action->setVisible(true);
        presentation_ellipse_action->setVisible(true);
        presentation_arrow_action->setVisible(true);
        presentation_undo_action->setVisible(true);
        presentation_redo_action->setVisible(true);
        presentation_undo_action->setEnabled(false);
        presentation_redo_action->setEnabled(false);
        presentation_save_action->setVisible(true);
        presentation_curve_action->setChecked(true);
        tv->set_presentation_tool(TV::PresentationTool::Curve);
        tv->set_presentation_color(presentation_current_color);
        tv->set_presentation_mode(true);
    } else {
        if (!offer_save_presentation()) {
            // user cancelled: stay in presentation mode
            presentation_action->blockSignals(true);
            presentation_action->setChecked(true);
            presentation_action->blockSignals(false);
            return;
        }
        tv->set_presentation_mode(false);
        ui->menuBar->setDisabled(false);
        presentation_color_action->setVisible(false);
        presentation_curve_action->setVisible(false);
        presentation_line_action->setVisible(false);
        presentation_rectangle_action->setVisible(false);
        presentation_ellipse_action->setVisible(false);
        presentation_arrow_action->setVisible(false);
        presentation_undo_action->setVisible(false);
        presentation_redo_action->setVisible(false);
        presentation_save_action->setVisible(false);
        // Persist the drawing color so it is restored the next time
        // presentation mode (or the application) is entered/started.
        save_presentation_color();
        // Restore the previously removed actions/widgets in their original
        // order and original side (before/after) of the presentation button,
        // so the presentation button always stays directly to the left of
        // the number-of-events widget.
        QAction *before_anchor = presentation_action;
        QAction *after_anchor = nullptr;
        for (QAction *action : presentation_saved_actions) {
            if (action->property("presentation_before").toBool()) {
                ui->toolBar->insertAction(before_anchor, action);
            } else {
                ui->toolBar->insertAction(after_anchor, action);
            }
        }
        presentation_saved_actions.clear();
    }
}

void TVMainWindow::update_presentation_color_icon()
{
    QPixmap pixmap(16, 16);
    pixmap.fill(presentation_current_color);
    presentation_color_action->setIcon(QIcon(pixmap));
}

void TVMainWindow::undo_presentation_action()
{
    if (tv) {
        tv->undo_presentation();
    }
}

void TVMainWindow::redo_presentation_action()
{
    if (tv) {
        tv->redo_presentation();
    }
}

void TVMainWindow::update_presentation_undo_redo(bool can_undo, bool can_redo)
{
    presentation_undo_action->setEnabled(can_undo);
    presentation_redo_action->setEnabled(can_redo);
}

void TVMainWindow::save_presentation_color()
{
    QSettings settings;
    settings.setValue("presentation/color", presentation_current_color);
    settings.sync();
}

void TVMainWindow::select_presentation_color()
{
    QColor color = QColorDialog::getColor(presentation_current_color, this, tr("Pick drawing color"));
    if (color.isValid()) {
        presentation_current_color = color;
        update_presentation_color_icon();
        save_presentation_color();
        if (tv) {
            tv->set_presentation_color(presentation_current_color);
        }
    }
}

void TVMainWindow::select_presentation_curve_tool()
{
    if (tv) {
        tv->set_presentation_tool(TV::PresentationTool::Curve);
    }
}

void TVMainWindow::select_presentation_line_tool()
{
    if (tv) {
        tv->set_presentation_tool(TV::PresentationTool::Line);
    }
}

void TVMainWindow::select_presentation_rectangle_tool()
{
    if (tv) {
        tv->set_presentation_tool(TV::PresentationTool::Rectangle);
    }
}

void TVMainWindow::select_presentation_ellipse_tool()
{
    if (tv) {
        tv->set_presentation_tool(TV::PresentationTool::Ellipse);
    }
}

void TVMainWindow::select_presentation_arrow_tool()
{
    if (tv) {
        tv->set_presentation_tool(TV::PresentationTool::Arrow);
    }
}

void TVMainWindow::save_presentation_picture()
{
    if (!tv) {
        return;
    }
    const QString file_name = QFileDialog::getSaveFileName(this, tr("Save PNG image"), "presentation.png", tr("Image Files (*.png);;All files (*.*)"));
    if (!file_name.isEmpty()) {
        tv->save_current_view(file_name, "PNG", -1);
    }
}

void TVMainWindow::load_data()
{
    disable_all_actions();
    Model_File_Load_Form customOpenFiles(this);
    if (customOpenFiles.exec() == QDialog::Accepted) {
        customOpenFiles.getNames(trace_file);
        add_to_recent_files(trace_file.filePath());
        proceed_load_data();
    }
    enable_all_actions();
}

void TVMainWindow::proceed_load_data()
{
    if (info.parse_trace_info_file(trace_file)) {
        TraceDialog trace(this);
#ifdef _OPENMP
        int max_threads = std::min(omp_get_max_threads(), (int)info.mpi_times.size());
        ;
#else
        int max_threads = 1;
#endif
        int load_threads, render_threads;
        trace.set_propertiers(info.mpi_times, info.nodes, 0, 100, max_threads, max_threads, info.has_transport_files());
        if (trace.exec() == QDialog::Accepted) {
            std::set<PEAT_Rank> ranks_to_load;
            int tmin, tmax;
            TV_TraceParts tp;
            trace.get_propertiers(ranks_to_load, tmin, tmax, load_threads, render_threads, tp);
            // sa->setMinimumHeight(0);
            // tv->setMinimumHeight(0);
            // sa->setMinimumHeight(0);
            // setMinimumHeight(0);
            QApplication::processEvents();
            tv->load_model(trace_file, ranks_to_load, tmin, tmax, load_threads, render_threads, tp);
            tv->reset_zoom();
            emit tv->start_image_creation();
            tv->update();
            tv->setFocus();
            setWindowTitle("Trace data " + trace_file.absoluteFilePath());
            ui->actionReLoad->setEnabled(true);
            presentation_action->setEnabled(true);
        }
    }
}

Model_File_Load_Form::Model_File_Load_Form(QWidget *parent) : QDialog(parent)
{
    setWindowTitle("Open Trace info file");
    resize(parent->width() * DIALOG_WIDTH_PERCENT, 0);
    v_main_layout = new QVBoxLayout(this);

    QHBoxLayout *hLayout1 = new QHBoxLayout();
    l_trace_file = new QLineEdit();
    b_open_file = new QPushButton("Trace Info File");
    hLayout1->addWidget(l_trace_file);
    hLayout1->addWidget(b_open_file);
    v_main_layout->addLayout(hLayout1);

    b_form = new QDialogButtonBox(this);
    b_form->setStandardButtons(QDialogButtonBox::Cancel | QDialogButtonBox::Ok);
    v_main_layout->addWidget(b_form);

    QObject::connect(b_form, SIGNAL(accepted()), this, SLOT(accept()));
    QObject::connect(b_form, SIGNAL(rejected()), this, SLOT(reject()));
    QObject::connect(b_open_file, SIGNAL(clicked()), this, SLOT(select_trace_file()));
}
void Model_File_Load_Form::accept()
{
    get_file_info(l_trace_file->text(), trace_info_file);
    QDialog::accept();
}

void Model_File_Load_Form::select_trace_file()
{
    QString message_file = QFileDialog::getOpenFileName(this, tr("Please, select Trace file"), "", tr("Trace files(*.trace.info);; All files (*)"));
    if (!message_file.isEmpty()) {
        l_trace_file->setText(message_file);
    }
}

// bool Model_File_Load_Form::get_file_info(const QString& user_file, QFileInfo& trace_file)
bool get_file_info(const QString &user_file, QFileInfo &trace_file)
{
    if (user_file.isEmpty()) {
        return false;
    }

    QString corrected_user_file = user_file;
    // corrected_user_file.replace( '\\', "/" );
    // QFileInfo user_file_info( corrected_user_file );
    for (QString::reverse_iterator char_it = corrected_user_file.rbegin(); char_it != corrected_user_file.rend();) {
        if (*char_it == ' ') {
            std::advance(char_it, 1);
            corrected_user_file.erase(char_it.base());
            continue;
        }
        break;
    }
    QFileInfo user_file_info(corrected_user_file);
    if (!user_file_info.exists()) {
        return false;
    }
    trace_file = user_file_info;
    return true;
}

void TVMainWindow::on_actionReset_triggered()
{
    disable_all_actions();
    tv->reset_view();
    enable_all_actions();
}

void TVMainWindow::on_actionCollLeast_BW_triggered()
{
    if (tv) {
        PEAT_Message_Size cutoff = 0;
        CutoffDialog dialog(this);
        if (dialog.exec() == QDialog::Accepted) {
            cutoff = dialog.get_cutoff();
            tv->find_coll_l_bw(cutoff);
        }
    }
}

void TVMainWindow::on_actionCollMost_Imbalance_triggered()
{
    if (tv)
        tv->find_coll_m_ib();
}

void TVMainWindow::on_actionP2PLeast_BW_triggered()
{
    if (tv) {
        PEAT_Message_Size cutoff = 0;
        CutoffDialog dialog(this);
        if (dialog.exec() == QDialog::Accepted) {
            cutoff = dialog.get_cutoff();
            tv->find_P2P_l_bw(cutoff);
        }
    }
}

void TVMainWindow::on_actionP2PMost_Imbalance_triggered()
{
    if (tv)
        tv->find_P2P_m_ib();
}

void TVMainWindow::on_actionZoomIn_triggered()
{
    disable_all_actions();
    if (tv)
        tv->zoom_in();
    enable_all_actions();
}

void TVMainWindow::on_actionZoomOut_triggered()
{
    if (tv)
        tv->zoom_out();
}

void TVMainWindow::on_actionReDraw_triggered()
{
    tv->redraw_view();
}

void TVMainWindow::disable_all_actions()
{
    ui->menuBar->setDisabled(true);
    ui->toolBar->setDisabled(true);
}
void TVMainWindow::enable_all_actions()
{
    ui->menuBar->setDisabled(false);
    ui->toolBar->setDisabled(false);
}

void TVMainWindow::on_actionFind_triggered()
{
    tv->open_find_function();
}

void TVMainWindow::on_actionLocalsFunc_triggered(bool checked)
{
    shown.mpi_functions[0] = checked;
}

void TVMainWindow::on_actionP2PFunc_triggered(bool checked)
{
    shown.mpi_functions[1] = checked;
}

void TVMainWindow::on_actionCollectivesFunc_triggered(bool checked)
{
    shown.mpi_functions[2] = checked;
}

void TVMainWindow::on_actionRequestsFunc_triggered(bool checked)
{
    shown.mpi_functions[3] = checked;
}

void TVMainWindow::on_actionCollectivesMessages_triggered(bool checked)
{
    shown.mpi_messages[1] = checked;
}

void TVMainWindow::on_actionP2PMessages_triggered(bool checked)
{
    shown.mpi_messages[0] = checked;
}

void TVMainWindow::on_actionInitiationMessages_triggered(bool checked)
{
    shown.mpi_messages[2] = checked;
}

void TVMainWindow::on_actionCompletionsMessages_triggered(bool checked)
{
    shown.mpi_messages[3] = checked;
}

void TVMainWindow::reload_data()
{
    if (tv) {
        disable_all_actions();
        int tmin = 0, tmax = 100;
        if (tv->get_current_minmax(tmin, tmax)) {
            TraceDialog trace(this);
#ifdef _OPENMP
            int max_threads = std::min(omp_get_max_threads(), (int)info.mpi_times.size());
            ;
#else
            int max_threads = 1;
#endif
            int load_threads, render_threads;
            trace.set_propertiers(info.mpi_times, info.nodes, tmin, tmax, max_threads, max_threads, info.has_transport_files());
            QApplication::removePostedEvents(NULL);
            QApplication::processEvents();
            if (trace.exec() == QDialog::Accepted) {
                std::set<PEAT_Rank> ranks_to_load;
                trace.get_propertiers(ranks_to_load, tmin, tmax, load_threads, render_threads, tp);
                // sa->setMinimumHeight(0);
                // tv->setMinimumHeight(0);
                // sa->setMinimumHeight(0);
                // setMinimumHeight(0);
                // QApplication::removePostedEvents(NULL);
                // QApplication::processEvents();
                // tv->load_model(trace_file, ranks_to_load, tmin, tmax, load_threads, render_threads, tp, false);
                qreal lb = tv->get_left_bound(), rb = tv->get_right_bound();
                tv->load_model(trace_file, ranks_to_load, tmin, tmax, load_threads, render_threads, tp);
                tv->update_zoom(lb, rb);
                emit tv->start_image_creation();
                tv->update();
                tv->setFocus();
                setWindowTitle("Trace data " + trace_file.absoluteFilePath());
                // QApplication::removePostedEvents(NULL);
            }
        }
        enable_all_actions();
    }
}

bool TVMainWindow::event(QEvent *e)
{
    // See TV::event() in qTV.cpp: this prevents a plain Alt keypress from
    // being turned into a menu-bar mnemonic activation regardless of which
    // widget currently has keyboard focus.
    if (e->type() == QEvent::ShortcutOverride) {
        QKeyEvent *ke = static_cast<QKeyEvent *>(e);
        if (ke->key() == Qt::Key_Alt) {
            e->accept();
            return true;
        }
    }
    return QMainWindow::event(e);
}

void TVMainWindow::keyReleaseEvent(QKeyEvent *e)
{
    QWidget::keyReleaseEvent(e);
}
void TVMainWindow::keyPressEvent(QKeyEvent *e)
{
    QWidget::keyPressEvent(e);
}

void TVMainWindow::on_horizontalScrollBar_sliderReleased()
{
    double sr_min = (double)sr->minimum();
    double sr_max = (double)sr->maximum();
    double sr_value = (double)sr->value();
    tv->draw_view(sr_value / (sr_max - sr_min));
}

void TVMainWindow::on_actionLegend_triggered()
{
    LegendDialog d;
    d.set_properties(tv_colors);
    if (d.exec() == QDialog::Accepted) {
        d.get_properties(tv_colors);
        tv_colors.save();
    }
}

void TVMainWindow::open_recent_file()
{
    QAction *action = qobject_cast<QAction *>(sender());
    if (action) {
        QString file_name = action->data().toString();
        // TODO asdd open file code
        get_file_info(file_name, trace_file);
        proceed_load_data();
    }
}

void TVMainWindow::clear_recent_files()
{
    recent_files.clear();
    update_recent_file_actions();
    save_recent_files();
}

void TVMainWindow::update_recent_file_actions()
{
    int numRecentFiles = qMin(recent_files.size(), max_recent_files);

    for (int i = 0; i < numRecentFiles; ++i) {
        // QString text = tr("&%1 %2").arg(i + 1).arg(stripped_name(recent_files[i]));
        QString text = tr("&%1 %2").arg(i + 1).arg(recent_files[i]);
        recent_file_actions[i]->setText(text);
        recent_file_actions[i]->setData(recent_files[i]);
        recent_file_actions[i]->setVisible(true);
    }

    for (int i = numRecentFiles; i < max_recent_files; ++i) {
        recent_file_actions[i]->setVisible(false);
    }

    clear_recent_files_action->setEnabled(numRecentFiles > 0);
}

void TVMainWindow::add_to_recent_files(const QString &file_name)
{
    recent_files.removeAll(file_name);
    recent_files.prepend(file_name);
    while (recent_files.size() > max_recent_files) {
        recent_files.removeLast();
    }
    update_recent_file_actions();
    save_recent_files();
}

void TVMainWindow::load_recent_files()
{
    QSettings settings;
    recent_files = settings.value("recentFiles/files").toStringList();

    QStringList existingFiles;
    for (const QString &file : std::as_const(recent_files)) {
        if (QFile::exists(file)) {
            existingFiles.append(file);
        }
    }
    recent_files = existingFiles;
}

void TVMainWindow::save_recent_files()
{
    QSettings settings;
    settings.setValue("recentFiles/files", recent_files);
    settings.sync();
}

void TVMainWindow::about_dialog()
{
    QMessageBox::about(this, tr("About qTV"),
                       tr("A simple PEAT Trace Visualizer built with Qt.\n\n"
                          "Features:\n"
                          "- Visualize whole trace and particular piece of it\n"
                          "- Function/Messages in region\n"
                          "- Find Event\n"
                          "- Analyze imbalance and BW\n"
                          "- Presentation mode\n(emphasize trace details during (remote) presentaion\n"
));
}
