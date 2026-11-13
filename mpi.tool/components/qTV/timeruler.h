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

#ifndef __TIMERULER_H__
#define __TIMERULER_H__

#include <QWidget>

// Time axis shown above the trace picture. The ticks are labelled with the time
// from the start of the loaded trace. The thin bar on its top is an overview:
// it stands for the whole loaded trace with the shown window highlighted, a
// click on it centers the view at that place.
class TimeRuler : public QWidget
{
    Q_OBJECT
  public:
    explicit TimeRuler(QWidget *parent = nullptr);
    // left/right are the bounds of the view, model_left/model_right of the loaded trace (clock ticks),
    // offset and width are where the picture is, in the ruler's coordinates
    void set_view(double left, double right, double model_left, double model_right, double freq, int offset, int width);
    void set_cursor(int x); // x of the mouse in the picture, negative to hide
    int picture_offset() const
    {
        return offset;
    }
    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;
  signals:
    void jump_requested(double fraction); // center the view at this fraction (0..1) of the loaded trace

  protected:
    void paintEvent(QPaintEvent *) override;
    void mousePressEvent(QMouseEvent *) override;

  private:
    static constexpr int overview_height = 7;
    double left, right, model_left, model_right, freq;
    int offset, width_px, cursor_x;
};

#endif // __TIMERULER_H__
