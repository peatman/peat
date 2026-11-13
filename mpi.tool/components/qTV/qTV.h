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

#ifndef __QTV_H__
#define __QTV_H__

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QPointer>
#include <QDialog>
#include <QDialogButtonBox>
#include <QElapsedTimer>
#include <QFileDialog>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
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
#include <QToolTip>
#include <QHelpEvent>
#include <QVBoxLayout>
#include <QtDebug>
#include <QtGui>

// #include "../common/func_id.h"
#include "globals.h"
// #include "messagesdialog.h"
#include "elements.h"
#include "communicatorsdialog.h"
#include "functionstabwidget.h"
#include "messagestabwidget.h"
class FindFunctionDialog;
class TV : public QWidget
{
    Q_OBJECT
  public:
    explicit TV(QScrollArea *a, QStatusBar *s, QProgressBar *p, QScrollBar *sl, TV_TraceParts *swn, QWidget *parent = 0);
    ~TV();
    bool load_model(QFileInfo &trace_file, const std::set<PEAT_Rank> &ranks_to_load, int tmin, int tmax, int load_threads, int render_threads, TV_TraceParts &tp);
    QPoint get_mouse_point() const
    {
        return cursor_point;
    }
    void draw_time_marker(QPainter &, long) const;
    void draw_focus(QPainter &, long) const;
    bool save(const QString &file_name, const char *format = 0, const int quality = -1)
    {
        if (main_image) {
            return main_image->save(file_name, format, quality);
        }
        return false;
    }
    // bool getRateCurve(QMap<qreal, qreal> &, bool) const;
    void zoom(int delta);
    void zoom_in();
    void zoom_out();
    void find_coll_l_bw(PEAT_Message_Size cutoff);
    void find_coll_m_ib();
    void find_P2P_l_bw(PEAT_Message_Size cutoff);
    void find_P2P_m_ib();
    void find_P2P_late(bool sender); // late sender (true) or late receiver (false)
    bool get_current_minmax(int &tmin, int &tmax);
    void reset_zoom()
    {
        if (elements)
            elements->reset_zoom();
    }

    inline qreal get_left_bound() const
    {
        if (elements)
            return elements->get_left_bound();
        return 0;
    }
    inline qreal get_right_bound() const
    {
        if (elements)
            return elements->get_right_bound();
        return 0;
    }
    inline void update_zoom(qreal lb, qreal rb) const
    {
        if (elements) {
            elements->update_zoom(lb, rb);
        }
    }

    // ---- Presentation (annotation) mode ----
    // While presentation mode is active, the Left and Middle mouse buttons draw
    // free-hand curves, lines or rectangles (as selected via the toolbar) using
    // a user-picked color, instead of their usual behaviour (select/drag). The
    // Right mouse button keeps its normal behaviour (context menu). The
    // underlying picture is preserved and restored once presentation mode is left.
    enum class PresentationTool {
        Curve,
        Line,
        Rectangle,
        Ellipse,
        Arrow,
        Text,  // a click asks for a text and puts it at the clicked place
        Eraser // wipes the drawings under the cursor
    };
    void set_presentation_width(int width)
    {
        presentation_width = width;
    }
    bool is_presentation_mode() const
    {
        return presentation_mode;
    }
    void set_presentation_mode(bool enabled);
    void set_presentation_tool(PresentationTool tool)
    {
        presentation_tool = tool;
    }
    void set_presentation_color(const QColor &color)
    {
        presentation_color = color;
    }
    bool has_presentation_annotations() const
    {
        return presentation_has_changes;
    }
    // Saves the currently visible picture (including annotations, if any) to a file.
    bool save_current_view(const QString &file_name, const char *format = 0, const int quality = -1);

    // ---- Presentation undo/redo ----
    // Every finished stroke (curve/line/rectangle/ellipse/arrow) is pushed onto
    // the undo stack before it is applied, so it can be erased with undo_presentation()
    // and reapplied with redo_presentation().
    bool can_undo_presentation() const
    {
        return !presentation_undo_stack.isEmpty();
    }
    bool can_redo_presentation() const
    {
        return !presentation_redo_stack.isEmpty();
    }
    void undo_presentation();
    void redo_presentation();
    std::shared_ptr<Communicators_Data> &get_communicators_data() { return communicators_data; }

    // ---- View history (Back/Forward) ----
    // Every rendered view is remembered as its two time bounds only (no image and
    // no trace data), so the history costs nothing however big the trace is.
    // Going back or forward re-renders the stored bounds from the loaded events.
    bool can_go_view_back() const
    {
        return view_history_pos > 0;
    }
    bool can_go_view_forward() const
    {
        return view_history_pos >= 0 && view_history_pos + 1 < view_history.size();
    }
    void go_view_history(int step); // -1 is back, +1 is forward
    void clear_view_history();      // to be called when another trace is loaded (not on reload)
    void close_detail_views();      // closes the Info / Functions / Messages / User regions windows
    bool cancel_running();          // Esc: stops the loading or the rendering in progress, false if there is none
    // What the toolbar shows: 0 - clock ticks with seconds, 1 - time from the start of the loaded trace
    void set_time_units(int units);

    // Shows the given time window (clock ticks); it is shifted or cut to fit the loaded trace.
    void show_view(qreal left, qreal right);
    // Bounds of the view and of the loaded trace (clock ticks) and the clock frequency; false if nothing is shown.
    bool get_view_info(qreal &left, qreal &right, qreal &model_left, qreal &model_right, qreal &freq) const;
    // The picture as it is on the screen (with the presentation drawings, if any).
    QImage current_picture() const;
    // Aggregated numbers of the view, or of the region selected with the right mouse button.
    void show_statistics(bool selected_region);
    // Opens the dialog that looks for the next/previous call of a function by its name.
    void open_find_function_by_name();

  public slots:
    void reset_view();
    void redraw_view();
    void draw_view(double start);
    void open_find_function();
    // void open_filter();
    void look_for_info();
    void look_for_functions();
    void look_for_messages();
    void look_for_messages_P2P();
    void look_for_messages_coll();
    void look_for_messages_req();
    void look_for_user_regions();
    void draw_main_image();
    void handle_table_item_clicked(QTableWidgetItem *item);
    void handle_tree_item_clicked(QTreeWidgetItem *item);
    void handle_info_item_clicked(QTreeWidgetItem *item, int column);

  protected:
    void paintEvent(QPaintEvent *);
    void mousePressEvent(QMouseEvent *);
    void mouseReleaseEvent(QMouseEvent *);
    void mouseMoveEvent(QMouseEvent *);
    void wheelEvent(QWheelEvent *);
    void keyPressEvent(QKeyEvent *);
    void keyReleaseEvent(QKeyEvent *);
    void contextMenuEvent(QContextMenuEvent *);
    bool event(QEvent *e);

  private:
    void delete_elements(Elements *to_delete);
    static void draw_presentation_arrow(QPainter &p, const QPoint &from, const QPoint &to);

    void updateBars() const;
    PTP *ptp;
    Elements *elements;
    QImage *main_image;
    QImage *main_image_first;
    QImage **thread_images;
    bool ctrl_pressed;
    bool alt_pressed;
    bool shift_pressed;
    std::map<PEAT_Rank, std::string> mpi_file_names;
    std::map<PEAT_Rank, std::string> hwc_file_names;
    std::map<PEAT_Rank, std::string> bind_file_names;
    std::map<PEAT_Rank, std::string> transport_file_names;

    bool back_to_full_view;
    bool drag;
    bool lfocus;
    bool rfocus;
    bool image_creating;
    int render_threads;
    QPoint draw_point_start;
    QPoint draw_point_stop;
    QPoint focus_point_start;
    QPoint focus_point_stop;

    QPoint cursor_point;
    PEAT_Rank filter_rank;
    // Info / Functions / Messages / User regions windows: every request opens a new one,
    // the earlier ones stay until the user closes them (they delete themselves then).
    QList<QPointer<QWidget>> detail_views;
    void show_detail_view(QWidget *view, const QString &title);
    QString region_title(const QString &kind, PEAT_Clock start, PEAT_Clock stop) const;
    std::shared_ptr<Communicators_Data> communicators_data; // read on demand, reset when a trace is loaded
    QProgressBar *pb;
    QStatusBar *sb;
    QScrollArea *sa;
    QScrollBar *sr;
    TV_TraceParts *shown;

    // Presentation (annotation) mode state
    bool presentation_mode;
    QImage *presentation_image; // snapshot of the picture right before entering presentation mode
    QImage *annotation_image;   // transparent overlay holding the free-hand drawings
    PresentationTool presentation_tool;
    QColor presentation_color;
    bool presentation_drawing;
    Qt::MouseButton presentation_button;
    QPoint presentation_start_point; // shape origin (line/rectangle) or last point (curve)
    QPoint presentation_last_point;
    QImage *presentation_stroke_base; // snapshot of annotation_image before the current stroke (for shape preview)
    QList<QImage> presentation_undo_stack;
    QList<QImage> presentation_redo_stack;
    bool presentation_has_changes; // true once at least one drawing action has been made since entering presentation mode
    void push_presentation_undo_state();

    // View history state
    struct View_Bounds {
        qreal left, right;
    };
    static constexpr int max_view_history = 100;
    QList<View_Bounds> view_history;
    int view_history_pos;         // index of the shown view in view_history, -1 when empty
    bool view_history_navigating; // true while a Back/Forward render is in progress (the view is not recorded again)
    bool view_history_top_is_pan; // the last recorded view was a pan of the one before it
    QElapsedTimer view_history_timer; // time since the last recorded view, to merge a series of pans into one entry
    void record_view(qreal left, qreal right);
    void render_current_zoom(); // renders the bounds set in elements

    // Find function by name
    FindFunctionDialog *find_dialog;
    PEAT_Clock find_anchor; // start of the last found call: the next search goes on from it
    bool find_anchor_valid;
    void find_function_step(int direction);
    bool loading;   // a trace is being read
    int time_units; // see set_time_units()

    // Presentation pen
    int presentation_width;
    bool presentation_freehand() const // tools that draw while the mouse moves
    {
        return presentation_tool == PresentationTool::Curve || presentation_tool == PresentationTool::Eraser;
    }
    void setup_presentation_painter(QPainter &p) const;

    // Selection: a click selects the event under the cursor, it is framed together with
    // the events joined with it by messages. The pointers are into elements.
    const TV_Event *selected_event;
    std::vector<const TV_Event *> selected_peers;
    void select_event_at(const QPoint &pos, const QPoint &global_pos);
    void draw_selection(QPainter &p) const;
    void show_selection_with_peers();

    // List of the views found by the last Analyse action
    QPointer<QDialog> candidates_view;
    void show_candidates(const QString &title);
  signals:
    void view_changed(double left, double right, double model_left, double model_right, double freq, int width); // a view has been rendered
    void cursor_moved(int x); // the time marker has moved
    void view_history_changed(bool can_back, bool can_forward); // emitted whenever the view history or the position in it changes
    void presentation_undo_redo_changed(bool can_undo, bool can_redo); // emitted whenever the undo/redo stacks change
    void show_current_time(QString) const;           // Show current position of the mouse in Tool Bar in ClockTicks
    void show_start_time(QString) const;            // Show left bound of the trace picture in Tool Bar in ClockTicks
    void show_end_time(QString) const;              // Show right bound of the trace picture in Tool Bar in ClockTicks
    void show_invisible_items_number(QString) const; // number of hidden elements on screen
    void show_candidate_number(QString) const;      // number of hidden elements on screen
    void show_ctrl_key(bool) const;                 // Key status
    void show_alt_key(bool) const;                  // Key status
    void show_shift_key(bool) const;                // Key status
    void start_image_creation() const;              // start construction of the trace image
};
class Find_IDX_Form : public QDialog
{
    Q_OBJECT
  public:
    Find_IDX_Form(QWidget *parent, std::vector<PEAT_Rank> ranks, quint32 maxIDX) : QDialog(parent)
    {
        setWindowTitle("Enter the rank and the unique index to find");
        // resize(parent->width()*DIALOG_WIDTH_PERCENT, 0);
        loaded_ranks = ranks;
        vL = new QVBoxLayout(this);
        lRank = new QListWidget();
        for (std::vector<PEAT_Rank>::iterator rank_it = ranks.begin(); rank_it != ranks.end(); ++rank_it) {
            char str[1024];
            sprintf(str, "Rank: %d", *rank_it);
            lRank->addItem(str);
        }
        lIDX = new QSpinBox();
        lIDX->setPrefix(tr("Uidx: "));
        lIDX->setMaximum(maxIDX);
        vL->addWidget(lRank);
        vL->addWidget(lIDX);
        bForm = new QDialogButtonBox(this);
        bForm->setStandardButtons(QDialogButtonBox::Cancel | QDialogButtonBox::Ok);
        vL->addWidget(bForm);
        QObject::connect(bForm, SIGNAL(accepted()), this, SLOT(accept()));
        QObject::connect(bForm, SIGNAL(rejected()), this, SLOT(reject()));
    }
    void get_names(PEAT_Rank &rank, PEAT_UID &idx)
    {
        rank = loaded_ranks.at(lRank->currentRow());
        idx = lIDX->value();
    }

  private:
    QVBoxLayout *vL;
    QSpinBox *lIDX;
    QListWidget *lRank;
    QDialogButtonBox *bForm;
    std::vector<PEAT_Rank> loaded_ranks;
};
/*
class Filter_Form : public QDialog {
    Q_OBJECT
public:
    Filter_Form(QWidget* parent, const PEAT_Rank filter_in,const PEAT_Rank max_rank) : QDialog(parent)
    {
        setWindowTitle("Enter the Filter rank");
        vL = new QVBoxLayout(this);
        lRank = new QSpinBox();
        lRank->setPrefix(tr("Rank: "));
        lRank->setMinimum(-1);
        lRank->setMaximum(max_rank);
        lRank->setValue(filter_in);
        vL->addWidget(lRank);
        bForm = new QDialogButtonBox(this);
        bForm->setStandardButtons(QDialogButtonBox::Cancel | QDialogButtonBox::Ok);
        vL->addWidget(bForm);
        QObject::connect(bForm, SIGNAL(accepted()), this, SLOT(accept()));
        QObject::connect(bForm, SIGNAL(rejected()), this, SLOT(reject()));
    }
    PEAT_Rank get_names()
    {
        return lRank->value();
    }

private:
    QVBoxLayout* vL;
    QSpinBox* lRank;
    QDialogButtonBox* bForm;

};
*/
#endif // __QTV_H__
