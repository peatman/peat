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
#include <QDialog>
#include <QDialogButtonBox>
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
#include "functionstabwidget.h"
#include "messagestabwidget.h"
class TV : public QWidget
{
    Q_OBJECT
  public:
    explicit TV(QScrollArea *a, QStatusBar *s, QProgressBar *p, QScrollBar *sl, TV_TraceParts *swn, QWidget *parent = 0);
    ~TV();
    void load_model(QFileInfo &trace_file, const std::set<PEAT_Rank> &ranks_to_load, int tmin, int tmax, int load_threads, int render_threads, TV_TraceParts &tp);
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
        Arrow
    };
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
    std::map<PEAT_Rank, std::string> fab_file_names;

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
    FunctionsTabWidget *functions_view;
    QTreeWidget *info_view;
    MessagesTabWidget *messages_view;
    // QTableWidget *messages_view;
    QTreeWidget *addresses_view;
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
  signals:
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
