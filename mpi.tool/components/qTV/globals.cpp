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

#include "globals.h"
#include <QSettings>
#if defined(_WIN32) && !defined(__MINGW32__)
#include <intrin.h>
#else
#include <cpuid.h>
#endif
long long freq;
TV_Global_Colors tv_colors;

void get_cpu_freq()
{
#if defined(_WIN32) && !defined(__MINGW32__)
    int cpu_info[4] = {0, 0, 0, 0};
    __cpuid(cpu_info, 0);
#else
    unsigned int cpu_info[4] = {0, 0, 0, 0};
    __get_cpuid(0, &cpu_info[0], &cpu_info[1], &cpu_info[2], &cpu_info[3]);
#endif
    if (cpu_info[0] >= 0x16) {
#if defined(_WIN32) && !defined(__MINGW32__)
        __cpuid(cpu_info, 0x16);
#else
        __get_cpuid(0x16, &cpu_info[0], &cpu_info[1], &cpu_info[2], &cpu_info[3]);
#endif
        freq = cpu_info[0];
    }
}
TV_Colors::TV_Colors() : rank_text(Qt::yellow, 0, Qt::SolidLine),
                         func_line(Qt::white, 0, Qt::SolidLine),
                         pe_line(Qt::black, 0, Qt::SolidLine),
                         func_text(Qt::white, 0, Qt::SolidLine),
                         failed_msg(Qt::darkCyan, Qt::SolidPattern)
{
}

TV_Global_Colors::TV_Global_Colors() : TV_Colors()
{
    message_pens[TV_PEN_MESSAGE_TYPE_NONE].setBrush(Qt::magenta);
    message_pens[TV_PEN_MESSAGE_TYPE_NORMAL].setBrush(Qt::blue);
    message_pens[TV_PEN_MESSAGE_TYPE_REVERSED].setBrush(Qt::black);
    message_pens[TV_PEN_MESSAGE_TYPE_INITIATION].setBrush(Qt::black);
    message_pens[TV_PEN_MESSAGE_TYPE_COMPLETION].setBrush(Qt::black);
    message_pens[TV_PEN_MESSAGE_TYPE_COLLECTIVE].setBrush(Qt::yellow);

    message_pens[TV_PEN_MESSAGE_TYPE_NONE].setWidth(4);
    message_pens[TV_PEN_MESSAGE_TYPE_NORMAL].setWidth(2);
    message_pens[TV_PEN_MESSAGE_TYPE_REVERSED].setWidth(2);
    message_pens[TV_PEN_MESSAGE_TYPE_INITIATION].setWidth(2);
    message_pens[TV_PEN_MESSAGE_TYPE_COMPLETION].setWidth(2);
    message_pens[TV_PEN_MESSAGE_TYPE_GPU_HD_TRANSFER].setWidth(2);
    message_pens[TV_PEN_MESSAGE_TYPE_GPU_EXEC].setWidth(2);
    message_pens[TV_PEN_MESSAGE_TYPE_COLLECTIVE].setWidth(2);

    event_brushes[TV_BRUSH_EVENT_TYPE_USER_CODE].setColor(Qt::darkBlue);
    event_brushes[TV_BRUSH_EVENT_TYPE_SOURCE].setColor(Qt::red);
    event_brushes[TV_BRUSH_EVENT_TYPE_DESTINATION].setColor(Qt::green);
    event_brushes[TV_BRUSH_EVENT_TYPE_SAND].setColor(Qt::darkRed);
    event_brushes[TV_BRUSH_EVENT_TYPE_COLLECTIVE].setColor(Qt::yellow);
    event_brushes[TV_BRUSH_EVENT_TYPE_START].setColor(Qt::darkMagenta);
    event_brushes[TV_BRUSH_EVENT_TYPE_COMPLETION].setColor(Qt::magenta);
    event_brushes[TV_BRUSH_EVENT_TYPE_OTHER].setColor(Qt::lightGray);
    event_brushes[TV_BRUSH_EVENT_TYPE_OMPT_PARALLEL].setColor(Qt::cyan);
    event_brushes[TV_BRUSH_EVENT_TYPE_OMPT_LOCAL].setColor(Qt::darkCyan);
    event_brushes[TV_BRUSH_EVENT_TYPE_GPU].setColor(Qt::darkGreen);
    event_brushes[TV_BRUSH_EVENT_TYPE_PE].setColor(Qt::darkCyan);

    event_brushes[TV_BRUSH_EVENT_TYPE_USER_CODE].setStyle(Qt::SolidPattern);
    event_brushes[TV_BRUSH_EVENT_TYPE_SOURCE].setStyle(Qt::SolidPattern);
    event_brushes[TV_BRUSH_EVENT_TYPE_DESTINATION].setStyle(Qt::SolidPattern);
    event_brushes[TV_BRUSH_EVENT_TYPE_SAND].setStyle(Qt::SolidPattern);
    event_brushes[TV_BRUSH_EVENT_TYPE_COLLECTIVE].setStyle(Qt::SolidPattern);
    event_brushes[TV_BRUSH_EVENT_TYPE_START].setStyle(Qt::SolidPattern);
    event_brushes[TV_BRUSH_EVENT_TYPE_COMPLETION].setStyle(Qt::SolidPattern);
    event_brushes[TV_BRUSH_EVENT_TYPE_OTHER].setStyle(Qt::SolidPattern);
    event_brushes[TV_BRUSH_EVENT_TYPE_OMPT_PARALLEL].setStyle(Qt::SolidPattern);
    event_brushes[TV_BRUSH_EVENT_TYPE_OMPT_LOCAL].setStyle(Qt::SolidPattern);
    event_brushes[TV_BRUSH_EVENT_TYPE_GPU].setStyle(Qt::SolidPattern);
    event_brushes[TV_BRUSH_EVENT_TYPE_PE].setStyle(Qt::SolidPattern);
}

void TV_Global_Colors::load()
{
    QSettings settings;
    settings.beginGroup("Colors");

    // Load message pens
    settings.beginGroup("MessagePens");
    for (int i = 0; i < TV_PEN_MESSAGE_TYPE_MAX; ++i) {
        QString key = QString("pen_%1").arg(i);
        if (settings.contains(key)) {
            QColor color;
            color.setRgba64(QRgba64::fromRgba64(settings.value(key).toULongLong()));
            message_pens[i].setColor(color);
        }
    }
    settings.endGroup();

    // Load event brushes
    settings.beginGroup("EventBrushes");
    for (int i = 0; i < TV_BRUSH_EVENT_TYPE_MAX; ++i) {
        QString key = QString("brush_%1").arg(i);
        if (settings.contains(key)) {
            QColor color;
            color.setRgba64(QRgba64::fromRgba64(settings.value(key).toULongLong()));
            event_brushes[i].setColor(color);
        }
    }
    settings.endGroup();

    // Load other colors
    settings.beginGroup("OtherColors");
    if (settings.contains("func_text")) {
        QColor color;
        color.setRgba64(QRgba64::fromRgba64(settings.value("func_text").toULongLong()));
        func_text.setColor(color);
    }
    if (settings.contains("rank_text")) {
        QColor color;
        color.setRgba64(QRgba64::fromRgba64(settings.value("rank_text").toULongLong()));
        rank_text.setColor(color);
    }
    if (settings.contains("func_line")) {
        QColor color;
        color.setRgba64(QRgba64::fromRgba64(settings.value("func_line").toULongLong()));
        func_line.setColor(color);
    }
    if (settings.contains("pe_line")) {
        QColor color;
        color.setRgba64(QRgba64::fromRgba64(settings.value("pe_line").toULongLong()));
        pe_line.setColor(color);
    }
    if (settings.contains("failed_msg")) {
        QColor color;
        color.setRgba64(QRgba64::fromRgba64(settings.value("failed_msg").toULongLong()));
        failed_msg.setColor(color);
    }
    settings.endGroup();

    settings.endGroup();
}

void TV_Global_Colors::save()
{
    QSettings settings;
    settings.beginGroup("Colors");

    // Save message pens
    settings.beginGroup("MessagePens");
    for (int i = 0; i < TV_PEN_MESSAGE_TYPE_MAX; ++i) {
        QColor c = message_pens[i].color();
        quint64 rgb = c.rgba64();
        QString key = QString("pen_%1").arg(i);
        settings.setValue(key, rgb);
    }
    settings.endGroup();

    // Save event brushes
    settings.beginGroup("EventBrushes");
    for (int i = 0; i < TV_BRUSH_EVENT_TYPE_MAX; ++i) {
        QColor c = event_brushes[i].color();
        quint64 rgb = c.rgba64();
        QString key = QString("brush_%1").arg(i);
        settings.setValue(key, rgb);
    }
    settings.endGroup();

    // Save other colors
    settings.beginGroup("OtherColors");
    settings.setValue("func_text", (quint64)func_text.color().rgba64());
    settings.setValue("rank_text", (quint64)rank_text.color().rgba64());
    settings.setValue("func_line", (quint64)func_line.color().rgba64());
    settings.setValue("pe_line", (quint64)pe_line.color().rgba64());
    settings.setValue("failed_msg", (quint64)failed_msg.color().rgba64());
    settings.endGroup();

    settings.endGroup();
    settings.sync();
}
