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

#include "timeruler.h"
#include <QMouseEvent>
#include <QPainter>
#include <cmath>

TimeRuler::TimeRuler(QWidget *parent)
    : QWidget(parent), left(0), right(0), model_left(0), model_right(0), freq(0), offset(0), width_px(0), cursor_x(-1)
{
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    setAutoFillBackground(true);
    setToolTip(tr("Time from the start of the loaded trace.\nThe bar on top is the whole loaded trace, click it to move the view there."));
}

QSize TimeRuler::sizeHint() const
{
    return QSize(200, overview_height + fontMetrics().height() + 10);
}

QSize TimeRuler::minimumSizeHint() const
{
    return QSize(50, overview_height + fontMetrics().height() + 10);
}

void TimeRuler::set_view(double l, double r, double ml, double mr, double f, int off, int w)
{
    left = l;
    right = r;
    model_left = ml;
    model_right = mr;
    freq = f;
    offset = off;
    width_px = w;
    update();
}

void TimeRuler::set_cursor(int x)
{
    if (x != cursor_x) {
        cursor_x = x;
        update();
    }
}

void TimeRuler::mousePressEvent(QMouseEvent *e)
{
    // only clicks move the view: a drag would re-render the trace on every move
    if (e->button() == Qt::LeftButton && e->pos().y() <= overview_height + 2 && width_px > 0 && model_right > model_left) {
        emit jump_requested(qBound(0.0, double(e->pos().x() - offset) / width_px, 1.0));
    }
}

void TimeRuler::paintEvent(QPaintEvent *)
{
    if (width_px <= 0 || !(right > left) || !(freq > 0)) {
        return;
    }
    QPainter p(this);
    // overview: the whole loaded trace with the shown window highlighted
    const double model_len = model_right - model_left;
    p.fillRect(offset, 1, width_px, overview_height - 2, palette().color(QPalette::Mid));
    if (model_len > 0) {
        int x0 = offset + int((left - model_left) / model_len * width_px);
        int x1 = offset + int((right - model_left) / model_len * width_px);
        x0 = qBound(offset, x0, offset + width_px - 2);
        x1 = qBound(x0 + 2, x1, offset + width_px);
        p.fillRect(x0, 0, x1 - x0, overview_height, palette().color(QPalette::Highlight));
    }
    // ticks: a step of 1, 2 or 5 * 10^k seconds, at least min_label_px apart
    const double t0 = (left - model_left) / freq; // seconds from the trace start at the left edge
    const double t1 = (right - model_left) / freq;
    const double len = t1 - t0;
    const int min_label_px = fontMetrics().horizontalAdvance("0000.000 ms") + 20;
    const double raw_step = len * min_label_px / width_px;
    const double magnitude = std::pow(10.0, std::floor(std::log10(raw_step)));
    double step = magnitude;
    if (raw_step > 5 * magnitude)
        step = 10 * magnitude;
    else if (raw_step > 2 * magnitude)
        step = 5 * magnitude;
    else if (raw_step > magnitude)
        step = 2 * magnitude;
    if (!(step > 0) || !std::isfinite(step)) {
        return;
    }
    // the unit of the labels follows the time shown, the number of decimals follows the step
    const double biggest = qMax(std::fabs(t0), std::fabs(t1));
    double unit = 1;
    const char *unit_name = "s";
    if (biggest < 1e-6) {
        unit = 1e-9;
        unit_name = "ns";
    } else if (biggest < 1e-3) {
        unit = 1e-6;
        unit_name = "us";
    } else if (biggest < 1) {
        unit = 1e-3;
        unit_name = "ms";
    }
    const int decimals = qBound(0, int(std::ceil(-std::log10(step / unit) - 1e-9)), 12);
    const int base_y = height() - 1;
    p.setPen(palette().color(QPalette::WindowText));
    p.drawLine(offset, base_y, offset + width_px, base_y);
    const double minor = step / 5;
    const long long first_minor = (long long)std::ceil(t0 / minor);
    const long long last_minor = (long long)std::floor(t1 / minor);
    if (last_minor - first_minor > 10000) {
        return; // cannot happen with the step chosen above, guards the loop anyway
    }
    for (long long k = first_minor; k <= last_minor; ++k) {
        const double t = k * minor;
        const int x = offset + int((t - t0) / len * width_px);
        if (k % 5 == 0) {
            p.drawLine(x, base_y - 6, x, base_y);
            p.drawText(x + 3, base_y - 4, QString::number(t / unit, 'f', decimals) + " " + unit_name);
        } else {
            p.drawLine(x, base_y - 3, x, base_y);
        }
    }
    if (cursor_x >= 0 && cursor_x <= width_px) {
        p.setPen(palette().color(QPalette::Highlight));
        p.drawLine(offset + cursor_x, overview_height, offset + cursor_x, base_y);
    }
}
