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
#include "communicatorsdialog.h"
#include "cutoffdialog.h"
#include "drawoptionsdialog.h"
#include "filterdialog.h"
#include "globals.h"
#include "helpviewer.h"
#include "highlightdialog.h"
#include "legenddialog.h"
#include "textviewer.h"
#include "tracedialog.h"
#include "trace_info.h"
#include <QClipboard>
#include <QInputDialog>
#include <QMessageBox>
#include <QMimeData>
#include <QPdfWriter>
#include <QPalette>
#include <QTimer>

// TV_TraceParts flags are kept in the settings as a string of 0/1
static QString flags_to_string(const bool *flags, int count)
{
    QString text;
    for (int i = 0; i < count; ++i)
        text += flags[i] ? QLatin1Char('1') : QLatin1Char('0');
    return text;
}
static void flags_from_string(const QString &text, bool *flags, int count)
{
    if (text.size() != count)
        return; // nothing saved yet, or saved by a version with another set of flags
    for (int i = 0; i < count; ++i)
        flags[i] = text.at(i) == QLatin1Char('1');
}

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
    {
        QSettings settings;
        shown.func_min_width = settings.value("Drawing/func_min_width", 2).toInt();
        shown.draw_offscreen_messages = settings.value("Drawing/offscreen_messages", false).toBool();
        shown.row_height_percent = qBound(25, settings.value("Drawing/row_height", 100).toInt(), 400);
        // "Drawing/color_by_function" is the flag this mode has replaced
        shown.color_mode = settings.contains("Drawing/color_mode") ? qBound(0, settings.value("Drawing/color_mode").toInt(), TV_COLOR_MODE_MAX - 1)
                                                                   : (settings.value("Drawing/color_by_function", false).toBool() ? TV_COLOR_BY_FUNCTION : TV_COLOR_BY_KIND);
        // the hidden communicators are not kept: their ids belong to one trace
        flags_from_string(settings.value("Filter/mpi_functions").toString(), shown.mpi_functions, TV_MPI_FUNC_GROUP_MAX);
        flags_from_string(settings.value("Filter/mpi_messages").toString(), shown.mpi_messages, 7);
    }
    tv = new TV(sa, statusBar(), pb, sr, &shown, this);
    sa->setWidget(tv);
    sa->setWidgetResizable(true);
    sa->setBackgroundRole(QPalette::Dark);
    ruler = new TimeRuler(this);
    {
        QWidget *central = new QWidget(this);
        QVBoxLayout *central_layout = new QVBoxLayout(central);
        central_layout->setContentsMargins(0, 0, 0, 0);
        central_layout->setSpacing(0);
        central_layout->addWidget(ruler);
        central_layout->addWidget(sa); // moved out of the designer's central widget before that one is replaced
        setCentralWidget(central);
    }
    connect(tv, &TV::view_changed, this, [this](double left, double right, double model_left, double model_right, double clock_freq, int width) {
        ruler->set_view(left, right, model_left, model_right, clock_freq, sa->viewport()->x() + tv->x(), width);
    });
    connect(tv, &TV::cursor_moved, ruler, &TimeRuler::set_cursor);
    connect(ruler, &TimeRuler::jump_requested, this, [this](double fraction) { tv->draw_view(fraction); });
    setAcceptDrops(true);
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
    label_current_time->setToolTip(tr("Time under the mouse cursor: clock ticks (seconds)"));
    label_start_time->setToolTip(tr("Left bound of the view: clock ticks (seconds)"));
    label_end_time->setToolTip(tr("Right bound and length of the view: clock ticks (seconds)"));
    label_hidden_elements->setToolTip(tr("Elements not drawn because they are narrower than the minimal width.\nZoom in, or lower the width in Options > Drawing."));
    label_candidate->setToolTip(tr("Number of the shown view in the list found by the last Analyse action (Ctrl+Left/Right steps through it)"));
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

    // View history: Back/Forward over the previously shown views, placed right
    // after Zoom Out in the toolbar and before Reset in the View menu.
    view_back_action = new QAction(tr("&Back"), this);
    view_back_action->setIcon(style()->standardIcon(QStyle::SP_ArrowBack));
    view_back_action->setShortcuts(QKeySequence::Back);
    view_back_action->setStatusTip(tr("Go back to the previous view"));
    view_back_action->setToolTip(tr("Previous view (%1)").arg(view_back_action->shortcut().toString(QKeySequence::NativeText)));
    view_back_action->setEnabled(false);
    connect(view_back_action, &QAction::triggered, this, [this]() { tv->go_view_history(-1); });
    view_forward_action = new QAction(tr("For&ward"), this);
    view_forward_action->setIcon(style()->standardIcon(QStyle::SP_ArrowForward));
    view_forward_action->setShortcuts(QKeySequence::Forward);
    view_forward_action->setStatusTip(tr("Go forward to the next view"));
    view_forward_action->setToolTip(tr("Next view (%1)").arg(view_forward_action->shortcut().toString(QKeySequence::NativeText)));
    view_forward_action->setEnabled(false);
    connect(view_forward_action, &QAction::triggered, this, [this]() { tv->go_view_history(1); });
    connect(tv, &TV::view_history_changed, this, [this](bool can_back, bool can_forward) {
        view_back_action->setEnabled(can_back);
        view_forward_action->setEnabled(can_forward);
    });
    {
        const QList<QAction *> actions = ui->toolBar->actions();
        const int zoom_out_index = actions.indexOf(ui->actionZoomOut);
        QAction *before = (zoom_out_index >= 0 && zoom_out_index + 1 < actions.size()) ? actions.at(zoom_out_index + 1) : nullptr;
        ui->toolBar->insertAction(before, view_back_action);
        ui->toolBar->insertAction(before, view_forward_action);
        ui->menuView->insertAction(ui->actionReset, view_back_action);
        ui->menuView->insertAction(ui->actionReset, view_forward_action);
    }

    QAction *copy_picture_action = new QAction(tr("&Copy picture"), this);
    copy_picture_action->setShortcut(QKeySequence(tr("Ctrl+Shift+C")));
    copy_picture_action->setStatusTip(tr("Copy the current picture with its time axis to the clipboard"));
    connect(copy_picture_action, &QAction::triggered, this, [this]() {
        const QImage picture = compose_picture();
        if (!picture.isNull())
            QApplication::clipboard()->setImage(picture);
    });
    ui->menuFile->insertAction(ui->menuFile->actions().value(3), copy_picture_action); // right after Snapshot

    QAction *go_to_time_action = new QAction(tr("&Go to time..."), this);
    go_to_time_action->setShortcut(QKeySequence(tr("Ctrl+G")));
    go_to_time_action->setStatusTip(tr("Center the view at a given time"));
    connect(go_to_time_action, &QAction::triggered, this, &TVMainWindow::go_to_time);
    QAction *find_function_action = new QAction(tr("Find f&unction..."), this);
    find_function_action->setShortcut(QKeySequence(tr("Ctrl+Shift+F")));
    find_function_action->setStatusTip(tr("Find the next or previous call of a function by its name"));
    connect(find_function_action, &QAction::triggered, this, [this]() { tv->open_find_function_by_name(); });
    ui->menuView->insertAction(ui->actionFilter, go_to_time_action);
    ui->menuView->insertAction(ui->actionFilter, ui->actionFind);
    ui->menuView->insertAction(ui->actionFilter, find_function_action);
    ui->menuView->insertSeparator(ui->actionFilter);

    QAction *taller_rows_action = new QAction(tr("&Taller rows"), this);
    taller_rows_action->setShortcut(QKeySequence(tr("Ctrl+Up")));
    taller_rows_action->setStatusTip(tr("Make the thread rows taller"));
    connect(taller_rows_action, &QAction::triggered, this, [this]() { change_row_height(1); });
    QAction *shorter_rows_action = new QAction(tr("&Shorter rows"), this);
    shorter_rows_action->setShortcut(QKeySequence(tr("Ctrl+Down")));
    shorter_rows_action->setStatusTip(tr("Make the thread rows shorter, to see more ranks at once"));
    connect(shorter_rows_action, &QAction::triggered, this, [this]() { change_row_height(-1); });
    QAction *legend_window_action = new QAction(tr("&Legend window"), this);
    legend_window_action->setStatusTip(tr("Show the colours and their meanings in a window beside the trace"));
    connect(legend_window_action, &QAction::triggered, this, &TVMainWindow::show_legend_window);
    ui->menuView->insertAction(ui->actionFilter, taller_rows_action);
    ui->menuView->insertAction(ui->actionFilter, shorter_rows_action);
    ui->menuView->insertAction(ui->actionFilter, legend_window_action);
    QAction *highlight_action = new QAction(tr("&Highlight..."), this);
    highlight_action->setStatusTip(tr("Dim all events but those of a function, a communicator or a message size"));
    connect(highlight_action, &QAction::triggered, this, &TVMainWindow::show_highlight_dialog);
    ui->menuView->insertAction(ui->actionFilter, highlight_action);
    {
        // what the toolbar shows for the cursor and the bounds of the view
        QMenu *units_menu = new QMenu(tr("Time &units"), this);
        QActionGroup *units_group = new QActionGroup(this);
        QSettings settings;
        const int units = settings.value("View/time_units", 0).toInt() == 1 ? 1 : 0;
        const QString names[2] = {tr("&Clock ticks and seconds"), tr("&Time from the start of the loaded trace")};
        for (int i = 0; i < 2; ++i) {
            QAction *action = units_menu->addAction(names[i]);
            action->setCheckable(true);
            action->setChecked(i == units);
            units_group->addAction(action);
            connect(action, &QAction::triggered, this, [this, i]() {
                QSettings s;
                s.setValue("View/time_units", i);
                tv->set_time_units(i);
            });
        }
        tv->set_time_units(units);
        ui->menuView->insertMenu(ui->actionFilter, units_menu);
    }
    ui->menuView->insertSeparator(ui->actionFilter);

    QAction *late_sender_action = new QAction(tr("Late &sender"), this);
    late_sender_action->setStatusTip(tr("Messages whose receiver waited for the send to start, the longest wait first"));
    connect(late_sender_action, &QAction::triggered, this, [this]() { tv->find_P2P_late(true); });
    QAction *late_receiver_action = new QAction(tr("Late &receiver"), this);
    late_receiver_action->setStatusTip(tr("Messages whose sender waited for the receive to start, the longest wait first"));
    connect(late_receiver_action, &QAction::triggered, this, [this]() { tv->find_P2P_late(false); });
    ui->menuPoint_to_Point->addAction(late_sender_action);
    ui->menuPoint_to_Point->addAction(late_receiver_action);

    QAction *statistics_action = new QAction(tr("&Statistics of the view"), this);
    statistics_action->setStatusTip(tr("Time per function, MPI time per rank and messages between ranks in the shown window"));
    connect(statistics_action, &QAction::triggered, this, [this]() { tv->show_statistics(false); });
    ui->menuAnalyse->addSeparator();
    ui->menuAnalyse->addAction(statistics_action);

    bookmarks_menu = new QMenu(tr("&Bookmarks"), this);
    menuBar()->insertMenu(ui->menuLegend->menuAction(), bookmarks_menu);
    rebuild_bookmarks_menu();

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

    presentation_text_action = new QAction(tr("Text"), this);
    presentation_text_action->setCheckable(true);
    presentation_text_action->setStatusTip(tr("Text tool: click where the text has to be"));
    connect(presentation_text_action, &QAction::triggered, this, [this]() { tv->set_presentation_tool(TV::PresentationTool::Text); });

    presentation_eraser_action = new QAction(tr("Eraser"), this);
    presentation_eraser_action->setCheckable(true);
    presentation_eraser_action->setStatusTip(tr("Eraser: wipes the drawings under the cursor"));
    connect(presentation_eraser_action, &QAction::triggered, this, [this]() { tv->set_presentation_tool(TV::PresentationTool::Eraser); });

    presentation_tool_group = new QActionGroup(this);
    presentation_tool_group->addAction(presentation_text_action);
    presentation_tool_group->addAction(presentation_eraser_action);
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
    ui->toolBar->insertAction(presentation_separator, presentation_text_action);
    ui->toolBar->insertAction(presentation_separator, presentation_eraser_action);
    {
        QSpinBox *width_box = new QSpinBox(this);
        width_box->setRange(1, 12);
        width_box->setValue(2);
        width_box->setSuffix(tr(" px"));
        width_box->setToolTip(tr("Width of the drawing pen"));
        width_box->setFocusPolicy(Qt::ClickFocus);
        connect(width_box, &QSpinBox::valueChanged, this, [this](int width) { tv->set_presentation_width(width); });
        presentation_width_action = ui->toolBar->insertWidget(presentation_separator, width_box);
    }
    ui->toolBar->insertAction(presentation_separator, presentation_undo_action);
    ui->toolBar->insertAction(presentation_separator, presentation_redo_action);
    ui->toolBar->insertAction(presentation_separator, presentation_save_action);

    presentation_color_action->setVisible(false);
    presentation_curve_action->setVisible(false);
    presentation_line_action->setVisible(false);
    presentation_rectangle_action->setVisible(false);
    presentation_ellipse_action->setVisible(false);
    presentation_arrow_action->setVisible(false);
    presentation_text_action->setVisible(false);
    presentation_eraser_action->setVisible(false);
    presentation_width_action->setVisible(false);
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
    QAction *contents_action = new QAction(tr("&Contents"), this);
    contents_action->setShortcut(QKeySequence::HelpContents);
    contents_action->setStatusTip(tr("Help of qTV"));
    connect(contents_action, &QAction::triggered, this, [this]() { show_help("index.html"); });
    helpMenu->addAction(contents_action);
    QAction *controls_action = new QAction(tr("C&ontrols"), this);
    controls_action->setStatusTip(tr("Mouse and keyboard controls of the trace picture"));
    connect(controls_action, &QAction::triggered, this, [this]() { show_help("controls.html"); });
    helpMenu->addAction(controls_action);
    helpMenu->addAction(about_action);

    {
        QSettings settings;
        restoreGeometry(settings.value("Window/geometry").toByteArray());
    }
}

TVMainWindow::~TVMainWindow()
{
    save_presentation_color();
    delete ui;
}
void TVMainWindow::snap_shot()
{
    save_picture_as("untitled.png");
}

// View > Highlight: which events stay bright
void TVMainWindow::show_highlight_dialog()
{
    HighlightDialog d(this);
    const Communicators *communicators = nullptr;
    if (!info.path.isEmpty() && !info.trace_files.isEmpty() && CommunicatorsDialog::load(this, info, tv->get_communicators_data()))
        communicators = &tv->get_communicators_data()->communicators;
    d.set_properties(shown, communicators);
    if (d.exec() == QDialog::Accepted) {
        d.get_properties(shown);
        tv->redraw_view();
    }
}

// Asks for a file and saves the picture as it is on the screen (with the presentation drawings, if any)
// together with its time axis.
void TVMainWindow::save_picture_as(const QString &suggested_name)
{
    const QImage picture = compose_picture();
    if (picture.isNull()) {
        return; // nothing is shown yet
    }
    const QString file_name = QFileDialog::getSaveFileName(this, tr("Save the picture"), suggested_name, tr("PNG image (*.png);;JPEG image (*.jpg);;PDF document (*.pdf);;All files (*.*)"));
    if (file_name.isEmpty()) {
        return;
    }
    // the format follows the extension, PNG if there is none
    const QString suffix = QFileInfo(file_name).suffix().toLower();
    bool saved = false;
    if (suffix == "pdf") { // one page of the size of the picture; the picture inside is a bitmap, not vector graphics
        QPdfWriter writer(file_name);
        writer.setResolution(96);
        writer.setPageSize(QPageSize(QSizeF(picture.width() * 72.0 / 96, picture.height() * 72.0 / 96), QPageSize::Point));
        writer.setPageMargins(QMarginsF(0, 0, 0, 0));
        QPainter p(&writer);
        saved = p.isActive();
        if (saved) {
            p.drawImage(0, 0, picture);
        }
    } else {
        saved = suffix.isEmpty() ? picture.save(file_name, "PNG") : picture.save(file_name);
    }
    if (!saved) {
        QMessageBox::warning(this, tr("Snapshot"), tr("Cannot save the picture to %1").arg(file_name));
    }
}

// Ctrl+Up / Ctrl+Down: taller or shorter thread rows (the same value as in Options > Drawing).
void TVMainWindow::change_row_height(int direction)
{
    if (tv->is_presentation_mode()) {
        return;
    }
    const int height = qBound(25, direction > 0 ? shown.row_height_percent * 5 / 4 : shown.row_height_percent * 4 / 5, 400);
    if (height != shown.row_height_percent) {
        shown.row_height_percent = height;
        save_drawing_settings();
        tv->redraw_view();
    }
}

void TVMainWindow::save_drawing_settings()
{
    QSettings settings;
    settings.setValue("Drawing/func_min_width", shown.func_min_width);
    settings.setValue("Drawing/offscreen_messages", shown.draw_offscreen_messages);
    settings.setValue("Drawing/row_height", shown.row_height_percent);
    settings.setValue("Drawing/color_mode", shown.color_mode);
}

void TVMainWindow::show_legend_window()
{
    QPoint place;
    const bool had_window = legend_window;
    if (had_window) { // recreated to show the current colours, at the same place
        place = legend_window->pos();
        delete legend_window.data();
    }
    legend_window = LegendDialog::create_window(this, tv_colors);
    if (had_window) {
        legend_window->move(place);
    }
    legend_window->show();
}

// The picture as it is on the screen with the time axis on top of it.
QImage TVMainWindow::compose_picture() const
{
    const QImage picture = tv->current_picture();
    if (picture.isNull()) {
        return picture;
    }
    const QPixmap axis = ruler->grab();
    const int axis_height = ruler->height();
    QImage result(picture.width(), picture.height() + axis_height, QImage::Format_ARGB32);
    result.fill(palette().color(QPalette::Window));
    QPainter p(&result);
    p.drawPixmap(-ruler->picture_offset(), 0, axis);
    p.drawImage(0, axis_height, picture);
    return result;
}

void TVMainWindow::go_to_time()
{
    qreal left, right, model_left, model_right, clock_freq;
    if (!tv->get_view_info(left, right, model_left, model_right, clock_freq) || !(clock_freq > 0)) {
        return;
    }
    const double last = (model_right - model_left) / clock_freq;
    bool ok = false;
    const double seconds = QInputDialog::getDouble(this, tr("Go to time"), tr("Seconds from the start of the loaded trace (0 ... %1):").arg(last, 0, 'g', 9),
                                                   ((left + right) / 2 - model_left) / clock_freq, 0, last, 9, &ok);
    if (ok) {
        const qreal center = model_left + seconds * clock_freq;
        tv->show_view(center - (right - left) / 2, center + (right - left) / 2);
        tv->setFocus();
    }
}

// Bookmarks are named views kept in the <trace info file>.user file, next to the source search directories.
void TVMainWindow::rebuild_bookmarks_menu()
{
    bookmarks_menu->clear();
    const QStringList bookmarks = info.read_bookmarks();
    QAction *add_action = bookmarks_menu->addAction(tr("&Add bookmark..."), this, &TVMainWindow::add_bookmark);
    add_action->setShortcut(QKeySequence(tr("Ctrl+B")));
    add_action->setStatusTip(tr("Remember the current view under a name"));
    QAction *remove_action = bookmarks_menu->addAction(tr("&Remove all bookmarks"), this, [this]() {
        if (!info.write_bookmarks(QStringList())) {
            QMessageBox::warning(this, tr("Bookmarks"), tr("Cannot write %1").arg(info.user_file_name()));
        }
        QTimer::singleShot(0, this, &TVMainWindow::rebuild_bookmarks_menu); // not from inside the action being deleted
    });
    remove_action->setEnabled(!bookmarks.isEmpty());
    QMenu *remove_menu = new QMenu(tr("Remove &bookmark"), bookmarks_menu); // one entry per bookmark, a click removes it
    remove_menu->setEnabled(!bookmarks.isEmpty());
    bookmarks_menu->insertMenu(remove_action, remove_menu);
    bookmarks_menu->addSeparator();
    for (const QString &bookmark : std::as_const(bookmarks)) { // left|right|name
        const qreal left = bookmark.section('|', 0, 0).toDouble();
        const qreal right = bookmark.section('|', 1, 1).toDouble();
        const QString name = bookmark.section('|', 2);
        bookmarks_menu->addAction(name, this, [this, left, right]() {
            tv->show_view(left, right);
            tv->setFocus();
        });
        remove_menu->addAction(name, this, [this, bookmark]() { remove_bookmark(bookmark); });
    }
}

void TVMainWindow::remove_bookmark(const QString &bookmark)
{
    QStringList bookmarks = info.read_bookmarks();
    bookmarks.removeOne(bookmark); // the whole entry is matched, so equal names with different views are told apart
    if (!info.write_bookmarks(bookmarks)) {
        QMessageBox::warning(this, tr("Bookmarks"), tr("Cannot write %1").arg(info.user_file_name()));
    }
    // the menu is rebuilt once the triggering action has returned: rebuilding deletes that action
    QTimer::singleShot(0, this, &TVMainWindow::rebuild_bookmarks_menu);
}

void TVMainWindow::add_bookmark()
{
    qreal left, right, model_left, model_right, clock_freq;
    if (info.trace_info_file.isEmpty() || !tv->get_view_info(left, right, model_left, model_right, clock_freq)) {
        return;
    }
    const QString suggested = clock_freq > 0 ? tr("at %1, length %2").arg(format_seconds((left - model_left) / clock_freq, 6), format_seconds((right - left) / clock_freq)) : QString();
    bool ok = false;
    const QString name = QInputDialog::getText(this, tr("Add bookmark"), tr("Name of the current view:"), QLineEdit::Normal, suggested, &ok);
    if (!ok || name.isEmpty()) {
        return;
    }
    QStringList bookmarks = info.read_bookmarks();
    bookmarks << QString::number(left, 'g', 17) + '|' + QString::number(right, 'g', 17) + '|' + name;
    if (!info.write_bookmarks(bookmarks)) {
        QMessageBox::warning(this, tr("Bookmarks"), tr("Cannot write %1").arg(info.user_file_name()));
    }
    QTimer::singleShot(0, this, &TVMainWindow::rebuild_bookmarks_menu); // not from inside the action being deleted
}

// Help > Contents / Controls: the pages are in help/*.html (see HelpViewer).
void TVMainWindow::show_help(const QString &page)
{
    if (!help_viewer) {
        help_viewer = new HelpViewer(this);
    }
    help_viewer->show_page(page);
}

// Opens a trace info file given on the command line or dropped onto the window.
void TVMainWindow::open_trace_file(const QString &file_name)
{
    if (tv->is_presentation_mode()) {
        return;
    }
    QFileInfo file;
    if (!get_file_info(file_name, file)) {
        QMessageBox::warning(this, tr("Load trace"), tr("File not found:\n%1").arg(file_name));
        return;
    }
    disable_all_actions();
    trace_file = file;
    add_to_recent_files(trace_file.filePath());
    proceed_load_data();
    enable_all_actions();
}

void TVMainWindow::dragEnterEvent(QDragEnterEvent *e)
{
    if (e->mimeData()->hasUrls() && !tv->is_presentation_mode()) {
        e->acceptProposedAction();
    }
}

void TVMainWindow::dropEvent(QDropEvent *e)
{
    const QList<QUrl> urls = e->mimeData()->urls();
    if (urls.isEmpty() || !urls.first().isLocalFile()) {
        return;
    }
    e->acceptProposedAction();
    const QString file_name = urls.first().toLocalFile();
    // the load dialog is opened after the drop is over, not inside of it
    QTimer::singleShot(0, this, [this, file_name]() { open_trace_file(file_name); });
}

void TVMainWindow::closeEvent(QCloseEvent *e)
{
    QSettings settings;
    settings.setValue("Window/geometry", saveGeometry());
    tv->close_detail_views(); // they are windows of their own and would keep the application running
    QMainWindow::closeEvent(e);
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
        save_picture_as("presentation.png");
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
                && action != presentation_undo_action && action != presentation_redo_action && action != presentation_save_action
                && action != presentation_text_action && action != presentation_eraser_action && action != presentation_width_action) {
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
        presentation_text_action->setVisible(true);
        presentation_eraser_action->setVisible(true);
        presentation_width_action->setVisible(true);
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
    presentation_text_action->setVisible(false);
    presentation_eraser_action->setVisible(false);
    presentation_width_action->setVisible(false);
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
    save_picture_as("presentation.png");
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
            tv->clear_view_history(); // another trace: the old views mean nothing (a reload keeps them)
            tv->close_detail_views(); // and so do the Info / Functions / Messages windows of the previous trace
            if (!tv->load_model(trace_file, ranks_to_load, tmin, tmax, load_threads, render_threads, tp)) {
                QMessageBox::warning(this, tr("Load trace"), tr("The trace files listed in\n%1\ncould not be read.").arg(trace_file.absoluteFilePath()));
            }
            tv->reset_zoom();
            emit tv->start_image_creation();
            tv->update();
            tv->setFocus();
            setWindowTitle("Trace data " + trace_file.absoluteFilePath());
            ui->actionReLoad->setEnabled(true);
            presentation_action->setEnabled(true);
            rebuild_bookmarks_menu();
        }
    } else {
        QMessageBox::warning(this, tr("Load trace"), tr("Cannot read the trace info file\n%1").arg(trace_file.absoluteFilePath()));
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
    // strip trailing spaces (QString::erase(const_iterator) needs Qt >= 6.5; chop works everywhere)
    while (corrected_user_file.endsWith(' ')) corrected_user_file.chop(1);
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

void TVMainWindow::on_actionFilter_triggered()
{
    FilterDialog d(this);
    d.set_properties(shown);
    if (!info.path.isEmpty() && !info.trace_files.isEmpty() && CommunicatorsDialog::load(this, info, tv->get_communicators_data()))
        d.set_communicators(&tv->get_communicators_data()->communicators, shown.hidden_comms);
    if (d.exec() == QDialog::Accepted) { // OK saves, Apply saves and redraws, Cancel drops the changes
        d.get_properties(shown);
        QSettings settings;
        settings.setValue("Filter/mpi_functions", flags_to_string(shown.mpi_functions, TV_MPI_FUNC_GROUP_MAX));
        settings.setValue("Filter/mpi_messages", flags_to_string(shown.mpi_messages, 7));
        if (d.redraw_requested())
            tv->redraw_view();
    }
}

void TVMainWindow::on_actionCommunicators_triggered()
{
    if (!CommunicatorsDialog::load(this, info, tv->get_communicators_data()))
        return;
    CommunicatorsDialog dialog(this);
    dialog.set_communicators(tv->get_communicators_data()->communicators);
    dialog.exec();
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
    // Esc cancels a loading or a rendering also when the trace picture has no keyboard focus
    if (e->key() == Qt::Key_Escape && tv && tv->cancel_running()) {
        return;
    }
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
        if (legend_window) {
            show_legend_window(); // the legend window follows the new colours
        }
    }
}

void TVMainWindow::on_actionDrawOptions_triggered()
{
    DrawOptionsDialog d(this);
    d.set_properties(shown);
    if (d.exec() == QDialog::Accepted) {
        d.get_properties(shown);
        save_drawing_settings();
        tv->redraw_view();
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
                          "- Analyse imbalance and BW\n"
                          "- Presentation mode\n(emphasize trace details during (remote) presentaion\n"
));
}
