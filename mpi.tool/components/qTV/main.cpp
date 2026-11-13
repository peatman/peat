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
#include "tvmainwindow.h"

#include <QApplication>
#include <QTimer>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    QCoreApplication::setOrganizationName("PEAT");
    QCoreApplication::setApplicationName("qTV");

    tv_colors.load();

    // a.setStyle("WindowsVista");
    a.setStyle("Fusion");
    TVMainWindow w;
    w.show();
    // qTV <trace info file>: opened once the window is up
    const QStringList args = a.arguments();
    if (args.size() > 1) {
        const QString file_name = args.at(1);
        QTimer::singleShot(0, &w, [&w, file_name]() { w.open_trace_file(file_name); });
    }
    int r = a.exec();
    tv_colors.save();
    return r;
}
