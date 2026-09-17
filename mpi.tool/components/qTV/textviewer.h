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

#ifndef __TEXTVIEWER_H__
#define __TEXTVIEWER_H__

#include <QAction>
#include <QCheckBox>
#include <QFileInfo>
#include <QLabel>
#include <QMenu>
#include <QMenuBar>
#include <QStatusBar>
#include <QString>
#include <QTextEdit>
#include <QToolBar>
#include <QWidget>

class TextViewer : public QWidget
{
    Q_OBJECT

  public:
    explicit TextViewer(QWidget *parent = nullptr);
    ~TextViewer();
    int load_file(QStringList &search_dirs, const QString &file_name);
    int load_file(const QString &file_name);
    void set_current_file(const QString &file_name);
    void goto_line();
    void go_to_line(int line);

    // Search position enum
    enum SearchStartPosition {
        SearchFromBeginning,
        SearchFromCurrent,
        SearchFromEnd
    };

    // Enhanced search function
    // Parameters:
    //   search_string: The text to search for
    //   forward: true for forward search, false for backward search
    //   start_pos: Where to start the search (Beginning, Current position, or End)
    //   case_sensitive: true for case-sensitive search, false for case-insensitive
    //   whole_words: true to match whole words only
    // Returns: line number (1-based) if found, -1 if not found
    //
    // Example usage:
    //   // Search forward from current position, case-insensitive
    //   int line = viewer->search_text("MyFunction", true, TextViewer::SearchFromCurrent, false);
    //   if (line != -1) {
    //       qDebug() << "Found at line:" << line;
    //   }
    //
    //   // Search backward from end, case-sensitive
    //   int line = viewer->search_text("TODO", false, TextViewer::SearchFromEnd, true);
    //
    //   // Search from beginning, forward, whole words only
    //   int line = viewer->search_text("int", true, TextViewer::SearchFromBeginning, false, true);
    int search_text(const QString &search_string,
                   bool forward = true,
                   SearchStartPosition start_pos = SearchFromCurrent,
                   bool case_sensitive = false,
                   bool whole_words = false);

    // Dialogless file search - returns list of matching files
    QStringList find_files(const QString &search_dir, const QString &pattern, int max_results = 500);

  protected:
  public slots:
    int find_file(QStringList &search_dirs, const QString &initial_dir = QString(), const QString &initial_pattern = QString());
    void show();

  private slots:
    void highlight_current_line();
    void update_status_bar();
    void on_save_toggled(bool checked);
    void save_file();
    void find_text();
    void replace_text();

  private:
    QString stripped_name(const QString &full_file_name);
    void search_files_in_directory(const QString &dir_path, const QString &pattern, QStringList &results, int max_results = 100);
    void do_load_file(const QString &file_name, const QString &content);

    QTextEdit *text_edit;
    QString current_file;
    QToolBar *tool_bar;
    QStatusBar *status_bar;
    QLabel *line_column_label;
    QCheckBox *save_toggle_check_box;
    QAction *save_action;
    QAction *find_action;
    QAction *replace_action;
    QAction *go_to_line_action;
};

#endif // __TEXTVIEWER_H__
