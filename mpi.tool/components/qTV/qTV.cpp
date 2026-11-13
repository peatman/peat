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

#include "qTV.h"
#include "../common/common_plain_types.h"
#include "elements.h"
#include "textviewer.h"
#include "statsdialog.h"
#include "tableexport.h"
#include "tableitems.h"
#include "candidatesdialog.h"
#include "finddialog.h"
#include <QCompleter>
#include <QInputDialog>
#include <QMessageBox>
#include <QFontMetrics>
#include <QGuiApplication>
#include <QHeaderView>
#include <QScreen>
#include <algorithm>
#include <cmath>
#include <functional>

// #include <QtGui>

#define heightRankPercent 0.6
#define MIN_MOUSE_MOVE 3
#define ZOOM_PORTION 0.1
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

TV::TV(QScrollArea *a, QStatusBar *s, QProgressBar *p, QScrollBar *sl, TV_TraceParts *swn, QWidget *parent) : QWidget(parent), ctrl_pressed(false), alt_pressed(false), shift_pressed(false), pb(p), sb(s), sa(a), sr(sl), shown(swn)
{
    ptp = 0;
    elements = 0;
    thread_images = 0;
    main_image = 0;
    main_image_first = new QImage();
    back_to_full_view = false;
    drag = false;
    lfocus = false;
    rfocus = false;
    draw_point_start = QPoint(0, 0);
    draw_point_stop = QPoint(0, 0);
    focus_point_start = QPoint(0, 0);
    focus_point_stop = QPoint(0, 0);
    filter_rank = -1;
    render_threads = 1;
    setMouseTracking(true);
    // TODO contextMenu

    QAction *act_info = new QAction(QObject::tr("Info"), this);
    QAction *act_func = new QAction(QObject::tr("Functions"), this);
    QAction *act_msg = new QAction(QObject::tr("Messages"), this);
    QAction *act_addr = new QAction(QObject::tr("User regions"), this);

    addAction(act_info);
    addAction(act_func);
    addAction(act_msg);
    addAction(act_addr);
    connect(act_info, SIGNAL(triggered()), this, SLOT(look_for_info()));
    connect(act_func, SIGNAL(triggered()), this, SLOT(look_for_functions()));
    connect(act_msg, SIGNAL(triggered()), this, SLOT(look_for_messages()));
    connect(act_addr, SIGNAL(triggered()), this, SLOT(look_for_user_regions()));
    QAction *act_stat = new QAction(QObject::tr("Statistics"), this);
    addAction(act_stat);
    connect(act_stat, &QAction::triggered, this, [this]() { show_statistics(true); });
    QAction *act_peers = new QAction(QObject::tr("Show selected with its peers"), this);
    addAction(act_peers);
    connect(act_peers, &QAction::triggered, this, &TV::show_selection_with_peers);
    image_creating = false;
    connect(this, SIGNAL(start_image_creation()), this, SLOT(draw_main_image()));
    // Qt::ActionsContextMenu triggers on mouse *press* by default on X11/Wayland
    // (incl. WSL) but on *release* on Windows, so on Linux the right button press
    // is consumed before mousePressEvent() ever sees it. Qt::PreventContextMenu
    // guarantees mousePressEvent()/mouseReleaseEvent() always get right-button
    // events on every platform; the actions menu is shown manually on release.
    setContextMenuPolicy(Qt::PreventContextMenu);
    // allow keyboard working
    setFocusPolicy(Qt::StrongFocus);

    // Presentation (annotation) mode initialization
    presentation_mode = false;
    presentation_image = 0;
    annotation_image = 0;
    presentation_stroke_base = 0;
    presentation_tool = PresentationTool::Curve;
    presentation_color = Qt::red;
    presentation_drawing = false;
    presentation_button = Qt::NoButton;
    presentation_has_changes = false;

    view_history_pos = -1;
    view_history_navigating = false;
    view_history_top_is_pan = false;

    find_dialog = 0;
    find_anchor = 0;
    find_anchor_valid = false;
    loading = false;
    time_units = 0;
    presentation_width = 2;
    selected_event = 0;
}

// Remembers the view that has just been rendered. Called from draw_main_image(),
// which every way of changing the view (zoom, pan, find, reset, ...) ends in.
void TV::record_view(qreal left, qreal right)
{
    const qreal length = right - left;
    if (!(length > 0)) {
        return;
    }
    const qreal eps = length * 1e-9;
    bool is_pan = false;
    if (view_history_pos >= 0) {
        const View_Bounds &cur = view_history.at(view_history_pos);
        if (qAbs(cur.left - left) <= eps && qAbs(cur.right - right) <= eps) {
            return; // same view rendered again (redraw, filter change, window resize)
        }
        is_pan = qAbs((cur.right - cur.left) - length) <= eps;
    }
    // a new view after going back drops the views that were ahead
    while (view_history.size() > view_history_pos + 1) {
        view_history.removeLast();
    }
    // a series of quick pans (a held arrow key) is kept as a single entry
    const bool merge = is_pan && view_history_top_is_pan && view_history_timer.isValid() && view_history_timer.elapsed() < 1000;
    if (merge) {
        view_history.last() = View_Bounds{left, right};
    } else {
        view_history.append(View_Bounds{left, right});
        if (view_history.size() > max_view_history) {
            view_history.removeFirst();
        }
    }
    view_history_pos = view_history.size() - 1;
    view_history_top_is_pan = is_pan;
    view_history_timer.start();
    emit view_history_changed(can_go_view_back(), can_go_view_forward());
}

void TV::go_view_history(int step)
{
    if (!elements || !main_image || image_creating || presentation_mode || view_history_pos < 0 || step == 0) {
        return;
    }
    step = step < 0 ? -1 : 1;
    // A reload may have changed the loaded time window: the stored view is cut to
    // what is loaded now, and views completely outside of it are dropped.
    const qreal model_left = elements->get_model_left_bound();
    const qreal model_right = elements->get_model_right_bound();
    int target = view_history_pos + step;
    qreal left = 0, right = 0;
    bool found = false;
    while (target >= 0 && target < view_history.size()) {
        left = qMax(view_history.at(target).left, model_left);
        right = qMin(view_history.at(target).right, model_right);
        if (right > left) {
            found = true;
            break;
        }
        view_history.removeAt(target);
        if (step < 0) {
            --view_history_pos;
            --target;
        }
    }
    if (!found) {
        emit view_history_changed(can_go_view_back(), can_go_view_forward());
        return;
    }
    view_history_pos = target;
    view_history_top_is_pan = false;
    emit view_history_changed(can_go_view_back(), can_go_view_forward());
    elements->update_zoom(left, right);
    view_history_navigating = true;
    render_current_zoom();
    view_history_navigating = false;
    setFocus();
}

void TV::render_current_zoom()
{
    draw_point_start = QPoint(0, 0);
    draw_point_stop = QPoint(width(), height());
    delete (main_image);
    main_image = 0;
    emit start_image_creation();
    updateBars();
    update();
}

void TV::show_view(qreal left, qreal right)
{
    if (!elements || !main_image || image_creating || presentation_mode) {
        return;
    }
    const qreal model_left = elements->get_model_left_bound();
    const qreal model_right = elements->get_model_right_bound();
    const qreal length = right - left;
    if (!(length > 0) || !(model_right > model_left)) {
        return;
    }
    if (length >= model_right - model_left) {
        left = model_left;
        right = model_right;
    } else if (left < model_left) { // shifted into the loaded trace, the length is kept
        left = model_left;
        right = model_left + length;
    } else if (right > model_right) {
        right = model_right;
        left = model_right - length;
    }
    elements->update_zoom(left, right);
    render_current_zoom();
}

bool TV::get_view_info(qreal &left, qreal &right, qreal &model_left, qreal &model_right, qreal &freq) const
{
    if (!elements || !main_image || image_creating) {
        return false;
    }
    left = elements->get_left_bound();
    right = elements->get_right_bound();
    model_left = elements->get_model_left_bound();
    model_right = elements->get_model_right_bound();
    freq = elements->get_frequency();
    return true;
}

QImage TV::current_picture() const
{
    if (!main_image) {
        return QImage();
    }
    QImage picture(*main_image);
    if (presentation_mode && annotation_image) {
        QPainter p(&picture);
        p.drawImage(QPoint(0, 0), *annotation_image);
    }
    return picture;
}

void TV::show_statistics(bool selected_region)
{
    if (!elements || !main_image || image_creating) {
        return;
    }
    qreal left = elements->get_left_bound();
    qreal right = elements->get_right_bound();
    if (selected_region && qAbs(focus_point_stop.x() - focus_point_start.x()) > MIN_MOUSE_MOVE) {
        left = elements->convert_mouse_to_ticks(qMin(focus_point_start.x(), focus_point_stop.x()), 0, main_image->width());
        right = elements->convert_mouse_to_ticks(qMax(focus_point_start.x(), focus_point_stop.x()), 0, main_image->width());
    }
    const qreal freq = elements->get_frequency();
    if (!(right > left) || !(freq > 0)) {
        return;
    }
    Elements::Statistics stats;
    QApplication::setOverrideCursor(Qt::WaitCursor);
    elements->get_statistics(PEAT_Clock(left), PEAT_Clock(right), stats);
    QApplication::restoreOverrideCursor();
    const qreal model_left = elements->get_model_left_bound();
    const QString text = tr("Window from %1 to %2 after the start of the loaded trace, length %3, %4 rank(s) loaded.\nCalls nested into other calls have rows of their own and are not added to the time of a rank; a call crossing the window bounds adds its part inside the window to the totals.")
                             .arg(format_seconds((left - model_left) / freq, 6), format_seconds((right - model_left) / freq, 6), format_seconds((right - left) / freq, 6))
                             .arg(stats.ranks.size());
    StatsDialog *dialog = new StatsDialog(window(), stats, (right - left) / freq, freq, text);
    dialog->show();
}

void TV::open_find_function_by_name()
{
    if (!elements || !main_image || image_creating) {
        return;
    }
    if (!find_dialog) { // the dialog itself is in finddialog.ui
        find_dialog = new FindFunctionDialog(window());
        connect(find_dialog, &FindFunctionDialog::find_requested, this, &TV::find_function_step);
        // another name or rank starts the search over from the current view
        connect(find_dialog, &FindFunctionDialog::search_changed, this, [this]() { find_anchor_valid = false; });
        connect(find_dialog, &FindFunctionDialog::dim_changed, this, [this]() {
            const int func = find_dialog->dim_others() ? int(global_func_id.idx_by_name(find_dialog->function_name().toLatin1().constData())) : 0;
            if (shown->highlight_func != func) {
                shown->highlight_func = func;
                redraw_view();
            }
        });
    }
    find_dialog->set_ranks(elements->get_loaded_ranks()); // the loaded ranks may differ after a (re)load
    find_anchor_valid = false;
    find_dialog->open_for_search();
}

void TV::find_function_step(int direction)
{
    if (!elements || !main_image || image_creating || presentation_mode || !find_dialog) {
        return;
    }
    const QString name = find_dialog->function_name();
    const PEAT_MPI_Func_ID func_id = global_func_id.idx_by_name(name.toLatin1().constData());
    if (name.isEmpty() || func_id == TMPI_FUNC_ID_NONE) {
        find_dialog->set_status(tr("Unknown function name."));
        return;
    }
    if (!find_anchor_valid) { // start from the edge of the view, so that the calls inside it are found first
        find_anchor = direction > 0 ? PEAT_Clock(elements->get_left_bound()) - 1 : PEAT_Clock(elements->get_right_bound()) + 1;
    }
    QApplication::setOverrideCursor(Qt::WaitCursor);
    const TV_Event *found = elements->find_function(func_id, find_dialog->rank(), direction, find_anchor);
    QApplication::restoreOverrideCursor();
    if (!found) {
        find_dialog->set_status(tr("No more calls of %1 in this direction.").arg(name));
        return;
    }
    find_anchor = found->e_time.func.in;
    find_anchor_valid = true;
    if (find_dialog->dim_others()) {
        shown->highlight_func = int(func_id); // drawn by the show_view() below
    }
    const qreal freq = elements->get_frequency();
    const qreal in = found->e_time.func.in, out = found->e_time.func.out;
    const qreal margin = 2 * qMax<qreal>(out - in, 1); // the call takes the middle fifth of the view
    find_dialog->set_status(tr("Rank %1, IDs %2%3, at %4, duration %5")
                                .arg(found->rank)
                                .arg(QString::fromStdString(peat_ids_string(found->ids)), found->is_nested() ? tr(" (nested)") : QString(),
                                     format_seconds((in - elements->get_model_left_bound()) / freq, 9), format_seconds((out - in) / freq)));
    show_view(in - margin, out + margin);
}

void TV::show_candidates(const QString &title)
{
    if (candidates_view) {
        delete candidates_view.data();
    }
    if (!elements) {
        return;
    }
    // the dialog itself is in candidatesdialog.ui
    CandidatesDialog *dialog = new CandidatesDialog(window(), title, elements->get_candidates(), elements->get_frequency(), elements->get_model_left_bound());
    connect(dialog, &CandidatesDialog::candidate_selected, this, [this](int index) {
        if (elements && main_image && !image_creating && !presentation_mode && elements->select_candidate(index)) {
            render_current_zoom();
        }
    });
    candidates_view = dialog;
    dialog->show();
}

void TV::clear_view_history()
{
    view_history.clear();
    view_history_pos = -1;
    view_history_top_is_pan = false;
    emit view_history_changed(false, false);
}

TV::~TV()
{
    if (elements)
        delete (elements);
    if (ptp)
        delete (ptp);
    if (main_image)
        delete (main_image);
    if (main_image_first)
        delete (main_image_first);
    close_detail_views();
    if (presentation_image)
        delete (presentation_image);
    if (annotation_image)
        delete (annotation_image);
    if (presentation_stroke_base)
        delete (presentation_stroke_base);
    // if (threadImages) delete [] (threadImages);
}
void TV::draw_main_image()
{
    if (elements) {
        int ranks = elements->get_draw_rank_count();
        elements->paint.clear(ranks);
        // a row holds two text lines by default; it is never lower than the message lines need (see y_indent in draw_elements)
        elements->paint.thread_height = qMax(8, fontMetrics().height() * 2 * shown->row_height_percent / 100);
        int mpi_ulc = 0, transport_ulc, omp_ulc, gpu_ulc;
        for (int a = 0; a < ranks; a++) {
            elements->paint.mpi_ulc[a] = mpi_ulc;
            int mpi_threads = elements->get_mpi_threads(a);
            int transport_threads = elements->get_transport_threads(a);
            int omp_threads = elements->get_omp_threads(a);
            int gpu_threads = elements->get_gpu_threads(a);
            transport_ulc = (mpi_ulc + mpi_threads * elements->paint.thread_height);
            omp_ulc = (transport_ulc + transport_threads * elements->paint.thread_height);
            gpu_ulc = (omp_ulc + omp_threads * elements->paint.thread_height);
            mpi_ulc = (gpu_ulc + gpu_threads * elements->paint.thread_height);
            elements->paint.transport_ulc[a] = transport_ulc;
            elements->paint.omp_ulc[a] = omp_ulc;
            elements->paint.gpu_ulc[a] = gpu_ulc;
        }
        elements->paint.height = mpi_ulc;
        elements->paint.width = width();

        image_creating = true; // block
        // find minimum height for the picture to show text on rectangles
        // qDebug() << "Minimum H " << minimumHeight();
        setGeometry(QRect(0, 0, 0, 0));
        // construct the image of the scene
        if (back_to_full_view && elements->paint.width == main_image_first->width() && elements->paint.height == main_image_first->height()) {
            // qDebug() << "Use initial view";
            main_image = new QImage(*main_image_first);
            back_to_full_view = false;
            sr->setMaximum(0);
        } else {
            // qDebug() << "construct Image W:" << width() << " H:" << scrH << "start" << pointStart.x() << "stop" << pointStop.x() << " INT_MAX " << INT_MAX;
            main_image = new QImage(elements->paint.width, elements->paint.height, QImage::Format_ARGB32); // QImage::Format_RGB16 or QImage::Format_RGB32 Format_ARGB32_Premultiplied
            // mainImage = new QImage(width(), scrH, QImage::Format_Indexed8); //QImage::Format_RGB16 or QImage::Format_RGB32 Format_ARGB32_Premultiplied
            Q_ASSERT(main_image);
            setMinimumHeight(main_image->height()); // to ask QScrollArea do its work
            main_image->fill(tv_canvas_line_color() == QColor(Qt::black) ? Qt::white : Qt::black); // base under the rows, light or dark as they are
            if (render_threads >= 1) {
                thread_images = new QImage *[render_threads];
                for (int a = 0; a < render_threads; a++) {
                    thread_images[a] = new QImage(elements->paint.width, elements->paint.height, QImage::Format_ARGB32);
                }
            }
            elements->draw_elements(pb, sr, main_image, thread_images, render_threads, draw_point_start.x(), draw_point_stop.x());
            if (render_threads >= 1) {
                for (int a = 0; a < render_threads; a++) {
                    delete thread_images[a];
                }
                delete[] thread_images;
                thread_images = NULL;
            }
            if (main_image_first->isNull()) { // initial creation of the "first look"
                // qDebug() << "copy to initial view";
                *main_image_first = *main_image;
            }
            if (back_to_full_view) { // recreating "first look" if window size changed
                // qDebug() << "recreating initial view";
                delete main_image_first;
                main_image_first = new QImage(*main_image);
                back_to_full_view = false;
            }
        }
        if (!view_history_navigating) {
            record_view(elements->get_left_bound(), elements->get_right_bound());
        }
        emit view_changed(elements->get_left_bound(), elements->get_right_bound(), elements->get_model_left_bound(), elements->get_model_right_bound(), elements->get_frequency(), elements->paint.width);
        sb->show();
        image_creating = false;
    }
}
void TV::paintEvent(QPaintEvent *event)
{
    if (main_image && !image_creating) {
        QPainter main_p(this);
        main_p.drawImage(QPoint(0, 0), *main_image);
        if (presentation_mode && annotation_image) {
            main_p.drawImage(QPoint(0, 0), *annotation_image);
        }
        if (!presentation_mode) {
            if (selected_event) {
                draw_selection(main_p);
            }
            draw_time_marker(main_p, main_image->height());
            draw_focus(main_p, main_image->height());
        }
        if (elements && (elements->drawing_cancelled() || elements->loading_cancelled())) {
            const QString text = elements->loading_cancelled() ? tr("Loading was cancelled: only what had been read by then is shown (ReLoad reads the trace again)")
                                                               : tr("Rendering was cancelled: the picture is incomplete (Ctrl+D redraws it)");
            QRect box = main_p.fontMetrics().boundingRect(text).adjusted(-5, -3, 5, 3);
            box.moveTopLeft(QPoint(60, visibleRegion().boundingRect().top() + 4));
            main_p.setPen(Qt::red);
            main_p.setBrush(QColor(255, 255, 220));
            main_p.drawRect(box);
            main_p.drawText(box, Qt::AlignCenter, text);
        }
    }
    QWidget::paintEvent(event);
}

void TV::set_presentation_mode(bool enabled)
{
    if (enabled == presentation_mode) {
        return;
    }
    if (enabled) {
        // Enter presentation mode: remember the current picture and prepare a
        // transparent overlay to draw the annotations onto.
        if (main_image) {
            if (presentation_image) {
                delete presentation_image;
            }
            presentation_image = new QImage(*main_image);
            if (annotation_image) {
                delete annotation_image;
            }
            annotation_image = new QImage(main_image->size(), QImage::Format_ARGB32);
            annotation_image->fill(Qt::transparent);
        }
        presentation_mode = true;
        presentation_drawing = false;
        presentation_button = Qt::NoButton;
        presentation_undo_stack.clear();
        presentation_redo_stack.clear();
        presentation_has_changes = false;
        emit presentation_undo_redo_changed(false, false);
    } else {
        // Leave presentation mode: restore the picture as it was before entering.
        presentation_mode = false;
        presentation_drawing = false;
        presentation_button = Qt::NoButton;
        if (presentation_image) {
            if (main_image) {
                delete main_image;
            }
            main_image = new QImage(*presentation_image);
            delete presentation_image;
            presentation_image = 0;
        }
        if (annotation_image) {
            delete annotation_image;
            annotation_image = 0;
        }
        if (presentation_stroke_base) {
            delete presentation_stroke_base;
            presentation_stroke_base = 0;
        }
        presentation_undo_stack.clear();
        presentation_redo_stack.clear();
        presentation_has_changes = false;
    }
    update();
}

void TV::push_presentation_undo_state()
{
    if (!annotation_image) {
        return;
    }
    presentation_undo_stack.append(*annotation_image);
    presentation_redo_stack.clear();
    presentation_has_changes = true;
    emit presentation_undo_redo_changed(can_undo_presentation(), can_redo_presentation());
}

void TV::undo_presentation()
{
    if (!annotation_image || presentation_undo_stack.isEmpty()) {
        return;
    }
    presentation_redo_stack.append(*annotation_image);
    *annotation_image = presentation_undo_stack.takeLast();
    presentation_has_changes = !presentation_undo_stack.isEmpty();
    emit presentation_undo_redo_changed(can_undo_presentation(), can_redo_presentation());
    update();
}

void TV::redo_presentation()
{
    if (!annotation_image || presentation_redo_stack.isEmpty()) {
        return;
    }
    presentation_undo_stack.append(*annotation_image);
    *annotation_image = presentation_redo_stack.takeLast();
    presentation_has_changes = true;
    emit presentation_undo_redo_changed(can_undo_presentation(), can_redo_presentation());
    update();
}

bool TV::save_current_view(const QString &file_name, const char *format, const int quality)
{
    if (!main_image) {
        return false;
    }
    if (presentation_mode && annotation_image) {
        QImage combined(*main_image);
        QPainter p(&combined);
        p.drawImage(QPoint(0, 0), *annotation_image);
        p.end();
        return combined.save(file_name, format, quality);
    }
    return main_image->save(file_name, format, quality);
}

void TV::setup_presentation_painter(QPainter &p) const
{
    QPen pen(presentation_color);
    pen.setWidth(presentation_width);
    pen.setCapStyle(Qt::RoundCap);
    if (presentation_tool == PresentationTool::Eraser) { // wipes the drawings, the trace picture below stays
        p.setCompositionMode(QPainter::CompositionMode_Clear);
        pen.setWidth(presentation_width * 6);
    }
    p.setPen(pen);
}

void TV::draw_presentation_arrow(QPainter &p, const QPoint &from, const QPoint &to)
{
    p.drawLine(from, to);
    QLineF line(from, to);
    if (qFuzzyCompare(line.length() + 1.0, 1.0)) {
        return; // zero-length line, no arrowhead to draw
    }
    const qreal arrow_size = 12.0;
    const double angle = std::atan2(-line.dy(), line.dx());
    QPointF arrow_p1 = to + QPointF(std::sin(angle - M_PI / 3) * arrow_size, std::cos(angle - M_PI / 3) * arrow_size);
    QPointF arrow_p2 = to + QPointF(std::sin(angle - M_PI + M_PI / 3) * arrow_size, std::cos(angle - M_PI + M_PI / 3) * arrow_size);

    QPolygonF arrow_head;
    arrow_head << QPointF(to) << arrow_p1 << arrow_p2;
    QBrush old_brush = p.brush();
    p.setBrush(QBrush(p.pen().color()));
    p.drawPolygon(arrow_head);
    p.setBrush(old_brush);
}

void TV::contextMenuEvent(QContextMenuEvent *event)
{
    // Never reached in practice: contextMenuPolicy is Qt::PreventContextMenu,
    // so QWidget::event() ignores QEvent::ContextMenu before it gets here.
    // The actions menu is shown explicitly from mouseReleaseEvent() instead.
    if (presentation_mode) {
        event->ignore();
        return;
    }
    QWidget::contextMenuEvent(event);
}

void TV::mousePressEvent(QMouseEvent *event)
{
    if (presentation_mode) {
        // Only the Left button draws in presentation mode. Middle and Right
        // buttons fall through to the base implementation and do nothing
        // (the actions menu is only shown from mouseReleaseEvent() outside
        // presentation mode).
        if (event->button() != Qt::LeftButton) {
            QWidget::mousePressEvent(event);
            return;
        }
        if (main_image && !image_creating && annotation_image) {
            if (presentation_tool == PresentationTool::Text) { // a click asks for the text and puts it there
                bool ok = false;
                const QString text = QInputDialog::getText(this, tr("Text"), tr("Text to put at the clicked place:"), QLineEdit::Normal, QString(), &ok);
                if (ok && !text.isEmpty()) {
                    push_presentation_undo_state();
                    QPainter p(annotation_image);
                    p.setRenderHint(QPainter::Antialiasing);
                    p.setPen(presentation_color);
                    QFont text_font = font();
                    text_font.setPointSize(qMax(8, text_font.pointSize()) + 2 * presentation_width);
                    text_font.setBold(true);
                    p.setFont(text_font);
                    p.drawText(event->pos(), text);
                    update();
                }
                return;
            }
            presentation_drawing = true;
            presentation_button = event->button();
            presentation_start_point = event->pos();
            presentation_last_point = event->pos();
            push_presentation_undo_state();
            if (presentation_stroke_base) {
                delete presentation_stroke_base;
            }
            presentation_stroke_base = new QImage(*annotation_image);
            if (presentation_freehand()) {
                QPainter p(annotation_image);
                p.setRenderHint(QPainter::Antialiasing);
                setup_presentation_painter(p);
                p.drawPoint(presentation_last_point);
            }
            update();
        }
        return;
    }
    if (main_image && !image_creating) {
        Q_ASSERT(elements);
        switch (event->button()) {
        case Qt::LeftButton:
            if (event->pos().x() <= 0) {
                draw_point_start = QPoint(0, 0);
            } else if (event->pos().x() >= width()) {
                draw_point_start = QPoint(width(), height());
            } else {
                draw_point_start = event->pos();
            }
            lfocus = true;
            rfocus = false;
            break;
        case Qt::MiddleButton:
            lfocus = true;
            rfocus = false;
            drag = true;
            draw_point_start = event->pos();
            break;
        case Qt::RightButton:
            lfocus = false;
            rfocus = true;
            focus_point_start = event->pos();
            break;
        case Qt::BackButton:
            go_view_history(-1);
            break;
        case Qt::ForwardButton:
            go_view_history(1);
            break;
        default:
            break;
        }
    }
    // QWidget::mousePressEvent(event);
}

void TV::mouseReleaseEvent(QMouseEvent *event)
{
    if (presentation_mode) {
        if (event->button() != Qt::LeftButton) {
            QWidget::mouseReleaseEvent(event);
            return;
        }
        if (presentation_drawing && event->button() == presentation_button) {
            if (annotation_image && presentation_stroke_base && !presentation_freehand()) {
                // finalize the shape onto the annotation image
                *annotation_image = *presentation_stroke_base;
                QPainter p(annotation_image);
                p.setRenderHint(QPainter::Antialiasing);
                setup_presentation_painter(p);
                if (presentation_tool == PresentationTool::Line) {
                    p.drawLine(presentation_start_point, event->pos());
                } else if (presentation_tool == PresentationTool::Rectangle) {
                    p.drawRect(QRect(presentation_start_point, event->pos()));
                } else if (presentation_tool == PresentationTool::Ellipse) {
                    p.drawEllipse(QRect(presentation_start_point, event->pos()));
                } else if (presentation_tool == PresentationTool::Arrow) {
                    draw_presentation_arrow(p, presentation_start_point, event->pos());
                }
            }
            if (presentation_stroke_base) {
                delete presentation_stroke_base;
                presentation_stroke_base = 0;
            }
            presentation_drawing = false;
            presentation_button = Qt::NoButton;
            update();
        }
        return;
    }
    if (main_image && !image_creating) {
        Q_ASSERT(elements);
        switch (event->button()) {
        case Qt::LeftButton: {
            if ((event->pos() - draw_point_start).manhattanLength() > MIN_MOUSE_MOVE) {
                if (event->pos().x() >= this->width()) {
                    draw_point_stop = QPoint(width(), height());
                } else if (event->pos().x() <= 0) {
                    draw_point_stop = QPoint(0, 0);
                } else {
                    draw_point_stop = event->pos();
                }
                delete (main_image);
                main_image = 0;
                emit start_image_creation();
            } else {
                select_event_at(event->pos(), event->globalPosition().toPoint());
            }
            lfocus = false;
        } break;
        case Qt::MiddleButton: {
            if (drag) {
                QPoint delta = draw_point_start - event->pos();
                draw_point_start.setX(delta.x());
                draw_point_stop.setX(width() + delta.x());
                delete (main_image);
                main_image = 0;
                emit start_image_creation();
            }
            // cursorPoint = event->pos(); //draw vertical line
            lfocus = false;
            updateBars();
            update();
            drag = false;
        } break;
        case Qt::RightButton: {
            focus_point_stop = event->pos();
            rfocus = false;
            QList<QAction *> menu_actions = actions();
            if (!menu_actions.isEmpty()) {
                QMenu::exec(menu_actions, event->globalPosition().toPoint(), nullptr, this);
            }
        } break;
        default:
            break;
        }
        update();
    } else {
        drag = false;
    }
    lfocus = false;
    rfocus = false;
    // QWidget::mouseReleaseEvent(event);
}

void TV::mouseMoveEvent(QMouseEvent *event)
{
    if (presentation_mode) {
        if (presentation_drawing && annotation_image) {
            if (presentation_freehand()) {
                QPainter p(annotation_image);
                p.setRenderHint(QPainter::Antialiasing);
                setup_presentation_painter(p);
                p.drawLine(presentation_last_point, event->pos());
                presentation_last_point = event->pos();
            } else if (presentation_stroke_base) {
                // live preview of line/rectangle without committing it
                *annotation_image = *presentation_stroke_base;
                QPainter p(annotation_image);
                p.setRenderHint(QPainter::Antialiasing);
                setup_presentation_painter(p);
                if (presentation_tool == PresentationTool::Line) {
                    p.drawLine(presentation_start_point, event->pos());
                } else if (presentation_tool == PresentationTool::Rectangle) {
                    p.drawRect(QRect(presentation_start_point, event->pos()));
                } else if (presentation_tool == PresentationTool::Ellipse) {
                    p.drawEllipse(QRect(presentation_start_point, event->pos()));
                } else if (presentation_tool == PresentationTool::Arrow) {
                    draw_presentation_arrow(p, presentation_start_point, event->pos());
                }
            }
            update();
        }
        return;
    }
    if (main_image && !image_creating) {
        Q_ASSERT(elements);
        cursor_point = event->pos(); // draw vertical line
        emit cursor_moved(cursor_point.x());
        updateBars();
        update();
    }
    // QWidget::mouseMoveEvent(event);
}


void TV::updateBars() const
{
    Q_ASSERT(elements);
    if (!main_image) {
        return;
    }
    double freq = elements->get_frequency();
    double curr = elements->convert_mouse_to_ticks(cursor_point.x(), 0, main_image->width());
    double left = elements->get_left_bound();
    double right = elements->get_right_bound();
    long hidElem = elements->get_no_draw_elems();
    int cur_candidate = elements->get_current_candidate();
    QString csec(" (" + QString::number(curr / freq, 'g', 3) + ")s");
    QString lsec(" (" + QString::number(left / freq, 'g', 3) + ")s");
    QString rsec(" (" + QString::number(right / freq, 'g', 3) + ")s");
    // set values in tool bar
    if (time_units == 1 && freq > 0) { // time from the start of the loaded trace, as on the time axis
        const double origin = elements->get_model_left_bound();
        emit show_current_time("Current: " + format_seconds((curr - origin) / freq, 9));
        emit show_start_time("From: " + format_seconds((left - origin) / freq, 9));
        emit show_end_time("To: " + format_seconds((right - origin) / freq, 9) + " Length: " + format_seconds((right - left) / freq, 6));
    } else {
        emit show_current_time("Current: " + outputDouble(curr) + csec);
        emit show_start_time("From: " + outputDouble(left) + lsec);
        emit show_end_time("To: " + outputDouble(right) + rsec + " Length: " + outputDouble(right - left) + (" (" + QString::number((right - left) / freq, 'g', 3) + ")s"));
    }
    emit show_invisible_items_number("H:" + QString::number(hidElem));    // number of hidden elements on screen Qt::TextFormat
    emit show_candidate_number(" C: " + QString::number(cur_candidate)); // number of hidden elements on screen Qt::TextFormat
    emit show_ctrl_key(!ctrl_pressed);
    emit show_alt_key(!alt_pressed);
    emit show_shift_key(!shift_pressed);
}

void TV::wheelEvent(QWheelEvent *e)
{
    if (main_image && !image_creating && !presentation_mode && (e->modifiers() & Qt::ShiftModifier) && !(e->modifiers() & Qt::ControlModifier)) {
        // Shift+wheel moves the view in time (some platforms report it as a horizontal wheel)
        const int delta = e->angleDelta().y() != 0 ? e->angleDelta().y() : e->angleDelta().x();
        if (delta != 0) {
            Q_ASSERT(elements);
            const qreal shift = width() * ZOOM_PORTION * (delta > 0 ? -1 : 1);
            draw_point_start.setX(shift);
            draw_point_stop.setX(width() + shift);
            delete (main_image);
            main_image = 0;
            emit start_image_creation();
            updateBars();
            update();
        }
        e->accept();
        setFocus();
        return;
    }
    if (main_image && !image_creating) {
        if (ctrl_pressed) {
            Q_ASSERT(elements);
            qreal zoomIn = width() * ZOOM_PORTION;
            qreal offset = e->position().x() - width() * 0.5;
            qreal newXleft = 0;
            qreal newXright = 0;
            // shift
            newXleft = (offset);
            newXright = (width() + offset);
            if (e->angleDelta().y() > 0) { // apply zooming
                newXleft += zoomIn;
                newXright -= zoomIn;
                // shift back
                newXleft -= offset * (1 - ZOOM_PORTION * 2);
                newXright -= offset * (1 - ZOOM_PORTION * 2);
            } else { // apply zooming
                newXleft -= zoomIn;
                newXright += zoomIn;
                // shift back
                newXleft -= offset * (1 + ZOOM_PORTION * 2);
                newXright -= offset * (1 + ZOOM_PORTION * 2);
            }
            draw_point_start.setX(newXleft);
            draw_point_stop.setX(newXright);
            delete (main_image);
            main_image = 0;
            emit start_image_creation();
            updateBars();
            update();
            // QWidget::wheelEvent(e);
            setFocus();
            return;
        }
    }
    setFocus();
    QWidget::wheelEvent(e);
}

void TV::zoom(int delta)
{
    if (main_image && !image_creating) {
        Q_ASSERT(elements);
        qreal zoomIn = width() * ZOOM_PORTION;
        qreal offset = 0; // width() * 0.5;
        qreal newXleft = 0;
        qreal newXright = 0;
        // shift
        newXleft = (offset);
        newXright = (width() + offset);
        if (delta > 0) { // apply zooming
            newXleft += zoomIn;
            newXright -= zoomIn;
            // shift back
            newXleft -= offset * (1 - ZOOM_PORTION * 2);
            newXright -= offset * (1 - ZOOM_PORTION * 2);
        } else { // apply zooming
            newXleft -= zoomIn;
            newXright += zoomIn;
            // shift back
            newXleft -= offset * (1 + ZOOM_PORTION * 2);
            newXright -= offset * (1 + ZOOM_PORTION * 2);
        }
        draw_point_start.setX(newXleft);
        draw_point_stop.setX(newXright);
        delete (main_image);
        main_image = 0;
        emit start_image_creation();
        updateBars();
        update();
    }
    setFocus();
}
void TV::zoom_in()
{
    zoom(1);
}
void TV::zoom_out()
{
    zoom(-1);
}
bool TV::event(QEvent *e)
{
    // Qt turns a plain Alt keypress into a QEvent::ShortcutOverride/menu
    // mnemonic activation before it ever reaches keyPressEvent(). Accepting
    // it here makes Alt behave just like Ctrl/Shift (no menu activation),
    // while still letting keyPressEvent()/keyReleaseEvent() update the
    // status bar indicator as usual.
    if (e->type() == QEvent::ShortcutOverride) {
        QKeyEvent *ke = static_cast<QKeyEvent *>(e);
        if (ke->key() == Qt::Key_Alt) {
            e->accept();
            return true;
        }
    }
    if (e->type() == QEvent::ToolTip && main_image) {
        QHelpEvent *he = static_cast<QHelpEvent *>(e);
        TV_Event *evt = elements->get_event(he->pos().x(), he->pos().y(), main_image->width());
        QString tool_tip;
        if (evt) {
            tool_tip = QString("Function:%1\nRank:%2\n").arg(global_func_id.name_by_idx(evt->func_id)).arg(evt->rank);
            const qreal clock_freq = elements->get_frequency();
            if (clock_freq > 0) {
                tool_tip += QString("Duration:%1 (%L2 clk)\nStart:%3 from the trace start\n")
                                .arg(format_seconds(evt->e_time.func.duration() / clock_freq))
                                .arg(evt->e_time.func.duration())
                                .arg(format_seconds((evt->e_time.func.in - elements->get_model_left_bound()) / clock_freq, 9));
            }
            if (evt->get_tx_bytes() >= 0) {
                tool_tip += QString("Sent bytes:%L1\n").arg(evt->get_tx_bytes());
            }
            if (evt->get_rx_bytes() >= 0) {
                tool_tip += QString("Received bytes:%L1\n").arg(evt->get_rx_bytes());
            }
            PEAT_MPI_Comm comm = 0;
            if (evt->get_comm(comm) && comm != 0) { // 0 is MPI_COMM_NULL
                tool_tip += QString("Comm:%1%2\n").arg(QString::number(comm, 16)).arg(comm == 2 ? QString(" (comm_world)") : comm == 1 ? QString(" (comm_self)") : QString());
            }
            tool_tip += QString("IDs:%1\nThread:%2\n").arg(QString::fromStdString(peat_ids_string(evt->ids))).arg(evt->thread);
            if (evt->is_transport()) {
                const TV_Transport_Event *transport_evt = static_cast<const TV_Transport_Event *>(evt);
                tool_tip += QString("Calls:%1\nClocks between calls:%2\n").arg(transport_evt->num_calls).arg(transport_evt->total_clocks_bitween_calls);
            }
        } else {
            tool_tip = QString("User Code/Thread wait");
        }
        QToolTip::showText(he->globalPos(), tool_tip, this);
        return true;
    }
    return QWidget::event(e);
}

void TV::keyReleaseEvent(QKeyEvent *e)
{
    if (main_image && !image_creating) {
        switch (e->type()) {
        case QEvent::KeyRelease:
            switch (e->key()) {
            case Qt::Key_Control:
                ctrl_pressed = false;
                break;
            case Qt::Key_Alt:
                alt_pressed = false;
                break;
            case Qt::Key_Shift:
                shift_pressed = false;
                break;
            }
            break;
        default:
            break;
        }
        updateBars();
    }
}
void TV::keyPressEvent(QKeyEvent *e)
{
    if (e->key() == Qt::Key_Escape && cancel_running()) {
        return;
    }
    if (main_image && !image_creating) {
        Q_ASSERT(elements);
        // qDebug()<< e->key();
        qreal shift = width() * ZOOM_PORTION;
        switch (e->type()) {
        case QEvent::KeyPress:
            switch (e->key()) {
            case Qt::Key_Right:
                if (ctrl_pressed) {
                    elements->move_viewport(1);
                } else {
                    draw_point_start.setX(shift);
                    draw_point_stop.setX(width() + shift);
                }
                break;
            case Qt::Key_Left:
                if (ctrl_pressed) {
                    elements->move_viewport(-1);
                } else {
                    draw_point_start.setX(-shift);
                    draw_point_stop.setX(width() - shift);
                }
                break;
            case Qt::Key_Escape:
                selected_event = 0;
                selected_peers.clear();
                update();
                break;
            case Qt::Key_Home: // to the start of the loaded trace, the length of the view is kept
                show_view(elements->get_model_left_bound(), elements->get_model_left_bound() + (elements->get_right_bound() - elements->get_left_bound()));
                break;
            case Qt::Key_End: // to the end of the loaded trace
                show_view(elements->get_model_right_bound() - (elements->get_right_bound() - elements->get_left_bound()), elements->get_model_right_bound());
                break;
            case Qt::Key_Up:
            case Qt::Key_Down:
            case Qt::Key_PageUp:
            case Qt::Key_PageDown:
                e->ignore(); // left to the scroll area: scrolls the ranks
                return;
            case Qt::Key_Control:
                ctrl_pressed = true;
                break;
            case Qt::Key_Alt:
                alt_pressed = true;
                break;
            case Qt::Key_Shift:
                shift_pressed = true;
                break;
            }
            if (e->key() == Qt::Key_Left || e->key() == Qt::Key_Right) {
                delete (main_image);
                main_image = 0;
                emit start_image_creation();
                update();
            }
            break;
        default:
            break;
        }
        updateBars();
    }
    // QWidget::keyPressEvent(e);
}

void threaded_delete(Elements *to_delete)
{
    delete to_delete;
}

#if defined(_WIN32) && !defined(__MINGW32__)
#include "windows.h"
#define create_thread(func, elems, id) id = CreateThread(NULL, 0, func, elems, 0, NULL)
DWORD WINAPI proxy_delete(LPVOID lpParam)
{
    threaded_delete((Elements *)lpParam);
    return 0;
}

#else
#include "pthread.h"
#define create_thread(func, elems, id) pthread_create(&id, NULL, func, elems)
void *proxy_delete(void *lpParam)
{
    threaded_delete((Elements *)lpParam);
    return NULL;
}
#endif

void TV::delete_elements(Elements *to_delete)
{
#if defined(_WIN32) && !defined(__MINGW32__)
    HANDLE id;
#else
    pthread_t id;
#endif
    create_thread(proxy_delete, to_delete, id);
}

bool TV::load_model(QFileInfo &trace_file, const std::set<PEAT_Rank> &ranks_to_load, int tmin, int tmax, int load_threads, int render_threads, TV_TraceParts &tp)
{
    TV::render_threads = render_threads;
    if (elements) {
        delete_elements(elements);
        elements = 0;
        draw_point_start = QPoint(0, 0);
        draw_point_stop = QPoint(0, 0);
        back_to_full_view = false;
        delete main_image;
        main_image = 0;
        delete main_image_first;
        main_image_first = 0;
        main_image_first = new QImage();
    }
    communicators_data.reset();
    if (candidates_view) { // the found views belong to the data being replaced
        delete candidates_view.data();
    }
    find_anchor_valid = false;
    selected_event = 0;
    selected_peers.clear();
    elements = new Elements(trace_file, shown);
    loading = true; // Esc is taken by cancel_running() from the event loop the reading runs
    const bool loaded = elements->load_model_data(pb, sr, mpi_file_names, hwc_file_names, bind_file_names, transport_file_names, ranks_to_load, tmin, tmax, load_threads, tp);
    loading = false;
    if (!loaded) {
        qDebug() << "Failed in trace parser.";
    }
    // qDebug() << *elements;
    draw_point_stop = QPoint(width(), height());
    return loaded;
}

// The reading and the rendering loops look at their flags at every step.
bool TV::cancel_running()
{
    if (!elements) {
        return false;
    }
    if (loading) {
        elements->cancel_loading();
        return true;
    }
    if (image_creating) {
        elements->cancel_drawing();
        return true;
    }
    return false;
}

void TV::set_time_units(int units)
{
    time_units = units;
    if (elements && main_image && !image_creating) {
        updateBars();
    }
}

void TV::draw_time_marker(QPainter &p, long height) const
{
    // p.setPen(QPen(Qt::black, 0, Qt::DashDotLine));
    p.setPen(QPen(tv_canvas_line_color(), 0, Qt::SolidLine));
    p.drawLine(cursor_point.x(), 0, cursor_point.x(), height);
}
void TV::draw_focus(QPainter &p, long height) const
{
    p.setBrush(QBrush(tv_canvas_line_color(), Qt::Dense1Pattern));
    if (lfocus) {
        p.drawRect(draw_point_start.x(), 0, cursor_point.x() - draw_point_start.x(), height);
    }
    if (rfocus) {
        p.drawRect(focus_point_start.x(), 0, cursor_point.x() - focus_point_start.x(), height);
    }
    // length of the region being selected, shown at the top of the visible part
    if ((lfocus || rfocus) && elements && main_image && main_image->width() > 0) {
        const int from = lfocus ? draw_point_start.x() : focus_point_start.x();
        const qreal freq = elements->get_frequency();
        if (cursor_point.x() != from && freq > 0) {
            const qreal ticks = (elements->get_right_bound() - elements->get_left_bound()) * qAbs(cursor_point.x() - from) / main_image->width();
            const QString text = format_seconds(ticks / freq) + QString(" (%L1 clk)").arg(ticks, 0, 'f', 0);
            QRect box = p.fontMetrics().boundingRect(text).adjusted(-5, -3, 5, 3);
            box.moveTopLeft(QPoint(qMin(from, cursor_point.x()) + 4, visibleRegion().boundingRect().top() + 4));
            p.setPen(Qt::black); // the box is light whatever the canvas is
            p.setBrush(QColor(255, 255, 220));
            p.drawRect(box);
            p.drawText(box, Qt::AlignCenter, text);
        }
    }
}

void TV::open_find_function()
{
    if (!elements || !main_image || image_creating) {
        return;
    }
    Find_IDX_Form custom_find_IDX_files(this, elements->get_loaded_ranks(), elements->fsize());
    if (custom_find_IDX_files.exec() == QDialog::Accepted) {
        PEAT_Rank rank;
        PEAT_UID idx;
        custom_find_IDX_files.get_names(rank, idx);
        if (elements->find_idx(rank, idx)) {
            draw_point_start.setX(0);
            draw_point_stop.setX(width());
            delete (main_image);
            main_image = 0;
            emit start_image_creation();
            update();
        }
    }
}
void TV::reset_view()
{
    if (!elements || !main_image || image_creating) {
        return;
    }
    back_to_full_view = true;
    delete (main_image);
    main_image = 0;
    elements->reset_zoom();
    draw_point_start = QPoint(0, 0);
    draw_point_stop = QPoint(width(), height());
    emit start_image_creation();
    updateBars();
    update();
}

void TV::redraw_view()
{
    if (!elements || !main_image || image_creating) {
        return;
    }
    delete (main_image);
    main_image = 0;
    draw_point_start = QPoint(0, 0);
    draw_point_stop = QPoint(width(), height());
    emit start_image_creation();
    updateBars();
    update();
}
void TV::draw_view(double start)
{
    if (!elements || !main_image || image_creating) {
        return;
    }
    delete (main_image);
    main_image = 0;
    draw_point_start = QPoint(0, 0);
    draw_point_stop = QPoint(width(), height());

    double zoom_time_start = elements->get_left_bound();
    double zoom_time_stop = elements->get_right_bound();
    double model_time_start = elements->get_model_left_bound();
    double model_time_stop = elements->get_model_right_bound();
    double duration = zoom_time_stop - zoom_time_start;
    double model_duration = model_time_stop - model_time_start;
    double zoom_start = model_time_start + start * model_duration;
    zoom_time_start = zoom_start - duration / 2;
    zoom_time_stop = zoom_start + duration / 2;
    if (zoom_time_stop > model_time_stop) {
        zoom_time_stop = model_time_stop;
        zoom_time_start = zoom_time_stop - duration;
    }
    if (zoom_time_start < model_time_start) {
        zoom_time_start = model_time_start;
        zoom_time_stop = zoom_time_start + duration;
    }
    elements->update_zoom(zoom_time_start, zoom_time_stop);
    emit start_image_creation();
    updateBars();
    update();
}

#define MAX_L_DISTANCE 5

void TV::handle_table_item_clicked(QTableWidgetItem *item)
{
    if (!item)
        return;

    // Check if the item has source file data
    QVariantMap userData = item->data(Qt::UserRole).toMap();
    for (int addr_num = 0; addr_num < elements->get_trace_info().num_addrs; addr_num++) {
        QString file_key = "src_file" + QString::number(addr_num);
        QString line_key = "src_line" + QString::number(addr_num);

        if (userData.contains(file_key) && userData.contains(line_key)) {
            QString sourceFile = userData[file_key].toString();
            int sourceLine = userData[line_key].toInt();
            QString func = userData["func_name"].toString();

            if (!sourceFile.isEmpty()) {
                // Create and show TextViewer with the source file
                TextViewer *viewer = new TextViewer(nullptr);
                if (viewer->load_file(elements->get_trace_info().src_dirs, sourceFile) == 0) {
                    elements->update_src_dirs();
                    if (sourceLine > 0)
                        viewer->go_to_line(sourceLine);
                    int found_line = viewer->search_text(func, false, TextViewer::SearchFromCurrent, false, false);
                    if (found_line > 0 && sourceLine - found_line < MAX_L_DISTANCE) {
                        viewer->go_to_line(found_line);
                    }
                    viewer->show();
                }
            }
        }
    }
}
void TV::handle_tree_item_clicked(QTreeWidgetItem *item)
{
    if (!item)
        return;

    // Check if the item has source file data. The data may be stored in
    // different columns depending on which tree view triggered the click,
    // so search all columns for a non-empty map.
    QVariantMap userData;
    for (int col = 0; col < item->columnCount(); col++) {
        QVariantMap m = item->data(col, Qt::UserRole).toMap();
        if (!m.isEmpty()) {
            userData = m;
            break;
        }
    }
    for (int addr_num = 0; addr_num < elements->get_trace_info().num_addrs; addr_num++) {
        QString file_key = "src_file" + QString::number(addr_num);
        QString line_key = "src_line" + QString::number(addr_num);

        if (userData.contains(file_key) && userData.contains(line_key)) {
            QString sourceFile = userData[file_key].toString();
            int sourceLine = userData[line_key].toInt();
            QString func = userData["func_name"].toString();

            if (!sourceFile.isEmpty()) {
                // Create and show TextViewer with the source file
                TextViewer *viewer = new TextViewer(nullptr);
                if (viewer->load_file(elements->get_trace_info().src_dirs, sourceFile) == 0) {
                    elements->update_src_dirs();
                    if (sourceLine > 0)
                        viewer->go_to_line(sourceLine);
                    int found_line = viewer->search_text(func, false, TextViewer::SearchFromCurrent, false, false);
                    if (found_line > 0 && sourceLine - found_line < MAX_L_DISTANCE) {
                        viewer->go_to_line(found_line);
                    }
                    viewer->show();
                }
            }
        }
    }
}
void TV::handle_info_item_clicked(QTreeWidgetItem *item, int column)
{
    if (!item || !elements)
        return;
    const QVariant comm_data = item->data(column, Qt::UserRole + 1);
    if (!comm_data.isValid()) { // not the communicator column: open the source file as before
        handle_tree_item_clicked(item);
        return;
    }
    QTreeWidget *info_view = item->treeWidget();
    if (!CommunicatorsDialog::load(info_view, elements->get_trace_info(), communicators_data))
        return;
    const std::vector<PEAT_MPI_Comm> only(1, comm_data.toInt());
    CommunicatorsDialog dialog(info_view);
    dialog.setWindowTitle(tr("Communicator %1").arg(QString::number(only[0], 16)));
    dialog.set_communicators(communicators_data->communicators, &only);
    dialog.setModal(true);
    dialog.exec();
}
void TV::show_detail_view(QWidget *view, const QString &title)
{
    view->setAttribute(Qt::WA_DeleteOnClose);
    view->setWindowTitle(title);
    detail_views.removeAll(QPointer<QWidget>()); // forget the windows the user has closed
    detail_views.append(view);
    view->show();
}

void TV::close_detail_views()
{
    const QList<QPointer<QWidget>> views = detail_views;
    detail_views.clear();
    for (const QPointer<QWidget> &view : views) {
        delete view.data();
    }
}

// Title of a window made for a region: tells the windows of different regions apart.
QString TV::region_title(const QString &kind, PEAT_Clock start, PEAT_Clock stop) const
{
    const qreal freq = elements->get_frequency();
    if (!(freq > 0)) {
        return kind;
    }
    const qreal model_left = elements->get_model_left_bound();
    return tr("%1: from %2 to %3 (%4)").arg(kind, format_seconds((start - model_left) / freq, 9), format_seconds((stop - model_left) / freq, 9), format_seconds((stop - start) / freq));
}

void TV::look_for_info()
{
    if (elements && main_image && !image_creating) {
        TV_Event *evt = elements->get_event(focus_point_stop.x(), focus_point_stop.y(), main_image->width());
        if (!evt) {
            return;
        }
        QTreeWidget *info_view = new QTreeWidget;
        info_view->setWordWrap(false);
        info_view->setColumnCount(12);
        info_view->setHeaderLabels(QStringList() << "Name" << "Group" << "Type" << "Rank" << "Comm" << "Thread" << "IDs" << "Start Time" << "End Time" << "Duration" << "Duration (us)" << "Source Location");

        std::function<QTreeWidgetItem *(const TV_Event *)> add_event_item = [&](const TV_Event *e) -> QTreeWidgetItem * {
            QTreeWidgetItem *func_item = new QTreeWidgetItem((QTreeWidget *)0);
            func_item->setText(0, global_func_id.name_by_idx(e->func_id));
            func_item->setText(1, tv_event_group_name(e->get_event_group()));
            func_item->setTextAlignment(1, Qt::AlignHCenter);
            func_item->setText(2, tv_event_type_name(e->get_event_type()));
            func_item->setTextAlignment(2, Qt::AlignHCenter);
            func_item->setData(3, Qt::DisplayRole, e->rank);
            func_item->setTextAlignment(3, Qt::AlignHCenter);
            PEAT_MPI_Comm comm = 0;
            if (e->get_comm(comm) && comm != 0) { // 0 is MPI_COMM_NULL
                func_item->setText(4, QString::number(comm, 16));
                func_item->setData(4, Qt::UserRole + 1, comm);
                func_item->setToolTip(4, "Click to show the communicator");
                func_item->setForeground(4, info_view->palette().brush(QPalette::Link));
                QFont link_font = func_item->font(4);
                link_font.setUnderline(true);
                func_item->setFont(4, link_font);
            } else {
                func_item->setText(4, "N/A");
            }
            func_item->setTextAlignment(4, Qt::AlignHCenter);
            func_item->setData(5, Qt::DisplayRole, e->thread);
            func_item->setTextAlignment(5, Qt::AlignHCenter);
            func_item->setData(6, Qt::DisplayRole, QString::fromStdString(peat_ids_string(e->ids)));
            func_item->setTextAlignment(6, Qt::AlignHCenter);
            func_item->setText(7, outputDouble(e->e_time.func.in));
            func_item->setTextAlignment(7, Qt::AlignRight);
            func_item->setText(8, outputDouble(e->e_time.func.out));
            func_item->setTextAlignment(8, Qt::AlignRight);
            func_item->setText(9, outputDouble(e->e_time.func.duration()));
            func_item->setTextAlignment(9, Qt::AlignRight);
            func_item->setText(10, outputDouble((PEAT_Ratio(e->e_time.func.duration()) / elements->get_trace_info().freq) * 1000000));
            func_item->setTextAlignment(10, Qt::AlignRight);

            if (elements->user_regions && e->addrs) { // transport events carry no address/source info
                QString tt;
                QStringList source_locations;
                QString source_file;
                int source_line = -1;
                QVariantMap userData;
                for (int addr_num = 0; addr_num < elements->get_trace_info().num_addrs; addr_num++) {
                    const PEAT_UID addr = e->addrs->addr[addr_num];
                    std::ostringstream itemObjectAddress;
                    User_Address_Info *addr_info = elements->user_regions->get_addr_by_id(e->rank, addr);
                    itemObjectAddress << addr_info->full_path << "+0x" << std::hex << addr_info->addr_offset << "\n"
                                      << addr_info->source << ":" << std::dec << addr_info->line << "\n";
                    tt = tt + itemObjectAddress.str().c_str() + "\n";
                    source_file = QString::fromStdString(addr_info->source);
                    source_line = addr_info->line;
                    QString file_key = "src_file" + QString::number(addr_num);
                    QString line_key = "src_line" + QString::number(addr_num);
                    if (!source_file.isEmpty() && source_line >= 0) {
                        userData[file_key] = source_file;
                        userData[line_key] = source_line;
                        source_locations << (source_file + ":" + QString::number(source_line));
                    }
                }
                userData["func_name"] = global_func_id.name_by_idx(e->func_id);
                func_item->setText(11, source_locations.join("; "));
                func_item->setToolTip(11, tt);
                func_item->setToolTip(0, tt);
                func_item->setData(0, Qt::UserRole, userData);
                func_item->setData(11, Qt::UserRole, userData);
            } else if (e->is_transport()) {
                const TV_Transport_Event *transport_e = static_cast<const TV_Transport_Event *>(e);
                func_item->setText(11, QString("Calls:%1 Clocks between calls:%2").arg(transport_e->num_calls).arg(transport_e->total_clocks_bitween_calls));
            }

            for (int f = 0; const TV_Event *child = e->get_child(f); f++) {
                QTreeWidgetItem *child_item = add_event_item(child);
                func_item->addChild(child_item);
            }

            return func_item;
        };

        QTreeWidgetItem *root_item = add_event_item(evt);
        info_view->addTopLevelItem(root_item);

        // Connect click handler to open source files
        connect(info_view, &QTreeWidget::itemClicked, this, &TV::handle_info_item_clicked);
        Table_Export::install(info_view);

        // Fit the window to the visible rows; repeated when the tree is expanded or collapsed.
        QTreeWidget *view = info_view;
        auto fit_size = [view]() {
            for (int col = 0; col < view->columnCount(); col++) {
                view->resizeColumnToContents(col);
            }
            int total_width = view->frameWidth() * 2;
            for (int col = 0; col < view->columnCount(); col++) {
                total_width += view->columnWidth(col);
            }
            int rows = 0;
            for (QTreeWidgetItemIterator it(view, QTreeWidgetItemIterator::NotHidden); *it; ++it) {
                bool visible = true; // a row is shown only if all its ancestors are expanded
                for (QTreeWidgetItem *p = (*it)->parent(); p; p = p->parent()) {
                    if (!p->isExpanded()) {
                        visible = false;
                        break;
                    }
                }
                rows += visible;
            }
            int total_height = view->header()->height() + view->frameWidth() * 2 + rows * view->sizeHintForRow(0);
            const QScreen *screen = view->screen();
            if (screen) {
                total_height = qMin(total_height, screen->availableGeometry().height());
            }
            view->resize(total_width, total_height);
        };
        fit_size();
        connect(info_view, &QTreeWidget::itemExpanded, info_view, [fit_size](QTreeWidgetItem *) { fit_size(); });
        connect(info_view, &QTreeWidget::itemCollapsed, info_view, [fit_size](QTreeWidgetItem *) { fit_size(); });

        show_detail_view(info_view, tr("Info: %1, rank %2, IDs %3").arg(global_func_id.name_by_idx(evt->func_id)).arg(evt->rank).arg(QString::fromStdString(peat_ids_string(evt->ids))));
    }
}
void TV::look_for_functions()
{
    if (elements && main_image && !image_creating) {
        PEAT_Clock start_clk = elements->convert_mouse_to_ticks(focus_point_start.x(), 0, main_image->width());
        PEAT_Clock stop_clk = elements->convert_mouse_to_ticks(focus_point_stop.x(), 0, main_image->width());
        if (start_clk > stop_clk) {
            PEAT_Clock t = start_clk;
            start_clk = stop_clk;
            stop_clk = t;
        }
        FunctionsTabWidget *functions_view = new FunctionsTabWidget;
        elements->get_functions_in_region(functions_view->get_mpi_table(), functions_view->get_omp_table(), functions_view->get_gpu_table(), functions_view->get_transport_table(), start_clk, stop_clk, height());

        // Connect double-click handler to open source files, for every tab
        auto connectTableWidget = [this](QTableWidget *table) {
            if (table) {
                connect(table, &QTableWidget::itemClicked, this, &TV::handle_table_item_clicked);
            }
        };
        connectTableWidget(functions_view->get_mpi_table());
        connectTableWidget(functions_view->get_omp_table());
        connectTableWidget(functions_view->get_gpu_table());
        connectTableWidget(functions_view->get_transport_table());
        for (QTableWidget *table : {functions_view->get_mpi_table(), functions_view->get_omp_table(), functions_view->get_gpu_table(), functions_view->get_transport_table()}) {
            Table_Export::install(table);
            tv_enable_sorting(table);
        }

        // connect((const QObject*) functionsView->horizontalHeader(), SIGNAL(sectionClicked(int)), (const QObject*) functionsView, SLOT(sortByColumn(int)));//sort column by header click
        // The tables are sized to their contents, but the tab widget keeps its default size: fit it to the widest/tallest tab.
        QSize tables_size(0, 0);
        for (QTableWidget *table : {functions_view->get_mpi_table(), functions_view->get_omp_table(), functions_view->get_gpu_table(), functions_view->get_transport_table()}) {
            tables_size = tables_size.expandedTo(table->size());
        }
        QSize view_size(tables_size.width() + 8, tables_size.height() + functions_view->tabBar()->sizeHint().height() + 8);
        if (const QScreen *screen = functions_view->screen()) {
            view_size = view_size.boundedTo(screen->availableGeometry().size());
        }
        functions_view->resize(view_size);
        show_detail_view(functions_view, region_title(tr("Functions"), start_clk, stop_clk));
    }
}

void TV::look_for_messages()
{
    if (elements && main_image && !image_creating) {
        PEAT_Clock start_clk = elements->convert_mouse_to_ticks(focus_point_start.x(), 0, main_image->width());
        PEAT_Clock stop_clk = elements->convert_mouse_to_ticks(focus_point_stop.x(), 0, main_image->width());
        if (start_clk > stop_clk) {
            PEAT_Clock t = start_clk;
            start_clk = stop_clk;
            stop_clk = t;
        }
        // messages_view = new QTableWidget;
        MessagesTabWidget *messages_view = new MessagesTabWidget;
        elements->get_messages_in_region(messages_view->get_init_table(), messages_view->get_comp_table(), messages_view->get_p2p_table(), messages_view->get_collective_tree(), messages_view->get_gpu_table(), start_clk, stop_clk, height());

        // Connect to all table widgets in the tab widget
        auto connectTableWidget = [this](QTableWidget *table) {
            if (table) {
                connect(table, &QTableWidget::itemClicked, this, &TV::handle_table_item_clicked);
            }
        };

        connectTableWidget(messages_view->get_init_table());
        connectTableWidget(messages_view->get_comp_table());
        connectTableWidget(messages_view->get_p2p_table());
        connectTableWidget(messages_view->get_gpu_table());

        connect(messages_view->get_collective_tree(), &QTreeWidget::itemClicked, this, &TV::handle_tree_item_clicked);
        for (QTableWidget *table : {messages_view->get_init_table(), messages_view->get_comp_table(), messages_view->get_p2p_table(), messages_view->get_gpu_table()}) {
            Table_Export::install(table);
            tv_enable_sorting(table);
        }
        Table_Export::install(messages_view->get_collective_tree());
        tv_enable_sorting(messages_view->get_collective_tree());

        // connect((const QObject*)messagesView->horizontalHeader(), SIGNAL(sectionClicked(int)), (const QObject*) messagesView, SLOT(sortByColumn(int)));//sort column by header click
        QScreen *screen = QGuiApplication::primaryScreen();
        messages_view->fit_to_contents(screen ? screen->availableGeometry().size() * 0.9 : QSize(1600, 900));
        show_detail_view(messages_view, region_title(tr("Messages"), start_clk, stop_clk));
    }
}

void TV::look_for_user_regions()
{
    if (elements && main_image && !image_creating) {
        PEAT_Clock start_clk = elements->convert_mouse_to_ticks(focus_point_start.x(), 0, main_image->width());
        PEAT_Clock stop_clk = elements->convert_mouse_to_ticks(focus_point_stop.x(), 0, main_image->width());
        if (start_clk > stop_clk) {
            PEAT_Clock t = start_clk;
            start_clk = stop_clk;
            stop_clk = t;
        }
        QTreeWidget *addresses_view = new QTreeWidget;
        elements->get_user_addresses_in_region(addresses_view, start_clk, stop_clk, height());
        Table_Export::install(addresses_view);
        tv_enable_sorting(addresses_view);
        // connect((const QObject*) addressesView->headerItem(), SIGNAL(sectionClicked(int)), addressesView, SLOT(sortByColumn(int)));//sort column by header click
        show_detail_view(addresses_view, region_title(tr("User regions"), start_clk, stop_clk));
    }
}

void TV::look_for_messages_P2P()
{
    if (elements && main_image && !image_creating) {
    }
}
void TV::look_for_messages_coll()
{
    if (elements && main_image && !image_creating) {
    }
}
void TV::look_for_messages_req()
{
    if (elements && main_image && !image_creating) {
    }
}

void TV::select_event_at(const QPoint &pos, const QPoint &global_pos)
{
    selected_peers.clear();
    selected_event = elements->get_event(pos.x(), pos.y(), main_image->width());
    if (selected_event) {
        QApplication::setOverrideCursor(Qt::WaitCursor);
        elements->get_peers(selected_event, selected_peers);
        QApplication::restoreOverrideCursor();
        QToolTip::showText(global_pos,
                           tr("%1 on rank %2 is selected, %3 event(s) are joined with it by messages.\nRight click: Show selected with its peers. Esc drops the selection.")
                               .arg(global_func_id.name_by_idx(selected_event->func_id))
                               .arg(selected_event->rank)
                               .arg(selected_peers.size()),
                           this);
    }
    update();
}

// Frames the selected event and the events joined with it by messages.
void TV::draw_selection(QPainter &p) const
{
    const int width = main_image->width();
    const QRect selected = elements->get_event_rect(selected_event, width);
    if (selected.isNull()) {
        return;
    }
    p.setBrush(Qt::NoBrush);
    p.setPen(QPen(QColor(255, 0, 255), 2));
    const size_t shown_peers = qMin<size_t>(selected_peers.size(), 2000); // a collective of a big run would make every repaint slow
    for (size_t i = 0; i < shown_peers; ++i) {
        const QRect peer = elements->get_event_rect(selected_peers[i], width);
        if (peer.isNull()) {
            continue;
        }
        p.drawRect(peer.adjusted(-1, -1, 1, 1));
        p.drawLine(selected.center(), peer.center());
    }
    p.setPen(QPen(Qt::black, 3));
    p.drawRect(selected.adjusted(-2, -2, 2, 2));
    p.setPen(QPen(Qt::yellow, 1));
    p.drawRect(selected.adjusted(-2, -2, 2, 2));
}

void TV::show_selection_with_peers()
{
    if (!elements || !main_image || image_creating || !selected_event) {
        return;
    }
    qreal left = selected_event->e_time.func.in;
    qreal right = selected_event->e_time.func.out;
    for (const TV_Event *peer : selected_peers) {
        left = qMin<qreal>(left, peer->e_time.func.in);
        right = qMax<qreal>(right, peer->e_time.func.out);
    }
    const qreal margin = qMax<qreal>((right - left) * 0.1, 1);
    show_view(left - margin, right + margin);
}

void TV::find_P2P_late(bool sender)
{
    if (elements && main_image && !image_creating) {
        if (elements->find_p2p_late(sender)) {
            render_current_zoom();
            show_candidates(sender ? tr("Point to point: late sender") : tr("Point to point: late receiver"));
        } else {
            QMessageBox::information(window(), sender ? tr("Late sender") : tr("Late receiver"), tr("No such messages in the loaded trace."));
        }
    }
}

void TV::find_coll_l_bw(PEAT_Message_Size cutoff)
{
    if (elements && main_image && !image_creating) {
        if (elements->find_coll_least_bw(cutoff)) {
            draw_point_start.setX(0);
            draw_point_stop.setX(width());
            delete (main_image);
            main_image = 0;
            emit start_image_creation();
            update();
            show_candidates(tr("Collectives: least bandwidth"));
        }
    }
}
void TV::find_coll_m_ib()
{
    if (elements && main_image && !image_creating) {
        if (elements->find_coll_most_imbalance()) {
            draw_point_start.setX(0);
            draw_point_stop.setX(width());
            delete (main_image);
            main_image = 0;
            emit start_image_creation();
            update();
            show_candidates(tr("Collectives: most imbalance"));
        }
    }
}
void TV::find_P2P_l_bw(PEAT_Message_Size cutoff)
{
    if (elements && main_image && !image_creating) {
        if (elements->find_p2p_least_bw(cutoff)) {
            draw_point_start.setX(0);
            draw_point_stop.setX(width());
            delete (main_image);
            main_image = 0;
            emit start_image_creation();
            update();
            show_candidates(tr("Point to point: least bandwidth"));
        }
    }
}
void TV::find_P2P_m_ib()
{
    if (elements && main_image && !image_creating) {
        if (elements->find_p2p_most_imbalance()) {
            draw_point_start.setX(0);
            draw_point_stop.setX(width());
            delete (main_image);
            main_image = 0;
            emit start_image_creation();
            update();
            show_candidates(tr("Point to point: most imbalance"));
        }
    }
}
bool TV::get_current_minmax(int &tmin, int &tmax)
{
    if (elements && main_image && !image_creating) {
        elements->get_minmax(tmin, tmax);
        return true;
    }
    return false;
}
/*
void TV::open_filter()
{
    if (!elements || !main_image || image_creating) { return; }
    PEAT_Rank max_rank = elements->get_draw_rank_count();
    Filter_Form filter(this, filter_rank, max_rank);
    if (filter.exec() == QDialog::Accepted) {
        filter_rank = filter.get_names();
    }
}
*/
