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

#ifndef __HELPVIEWER_H__
#define __HELPVIEWER_H__

#include <QDialog>

class QTextBrowser;
class QHelpEngine;

// Window with the help pages of qTV (help/*.html).
// The pages are read from the Qt Help file qTV.qch, made from help/qTV.qhp and placed next to
// the executable; the window then has a table of contents. If qTV is built without the Qt Help
// module, or qTV.qch is not found, the copy of the pages built into the executable is shown.
class HelpViewer : public QDialog
{
    Q_OBJECT
  public:
    explicit HelpViewer(QWidget *parent = nullptr);
    void show_page(const QString &page); // "index.html", "controls.html", ...

  private:
    QTextBrowser *browser;
    QHelpEngine *engine; // 0 when the built-in pages are used
    QString base_url;    // where the pages are: qthelp://<namespace>/doc/ or qrc:/help/
    bool setup_qt_help();
};

#endif // __HELPVIEWER_H__
