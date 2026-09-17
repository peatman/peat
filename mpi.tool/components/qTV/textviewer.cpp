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

#include "textviewer.h"
#include <QApplication>
#include <QCheckBox>
#include <QCloseEvent>
#include <QDialog>
#include <QDir>
#include <QDirIterator>
#include <QFileDialog>
#include <QFontMetrics>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QScreen>
#include <QSettings>
#include <QStandardPaths>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextStream>
#include <QVBoxLayout>

TextViewer::TextViewer(QWidget *parent)
    : QWidget(parent)
{
    // Create toolbar
    tool_bar = new QToolBar(this);
    tool_bar->setMovable(false);

    // Add save toggle checkbox to toolbar
    save_toggle_check_box = new QCheckBox(tr("Allow Save"), this);
    save_toggle_check_box->setChecked(false);
    save_toggle_check_box->setToolTip(tr("Toggle to enable file saving (makes editor editable)"));
    tool_bar->addWidget(save_toggle_check_box);
    connect(save_toggle_check_box, &QCheckBox::toggled, this, &TextViewer::on_save_toggled);

    // Add separator
    tool_bar->addSeparator();

    // Add save button with Ctrl+S shortcut
    save_action = new QAction(tr("Save"), this);
    save_action->setShortcut(QKeySequence::Save); // Ctrl+S
    save_action->setToolTip(tr("Save file (Ctrl+S)"));
    save_action->setEnabled(false); // Disabled by default (read-only mode)
    tool_bar->addAction(save_action);
    connect(save_action, &QAction::triggered, this, &TextViewer::save_file);

    // Add separator
    tool_bar->addSeparator();

    // Add Find button with Ctrl+F shortcut
    find_action = new QAction(tr("Find"), this);
    find_action->setShortcut(QKeySequence::Find); // Ctrl+F
    find_action->setToolTip(tr("Find text (Ctrl+F)"));
    tool_bar->addAction(find_action);
    connect(find_action, &QAction::triggered, this, &TextViewer::find_text);

    // Add Replace button with Ctrl+R shortcut
    replace_action = new QAction(tr("Replace"), this);
    replace_action->setShortcut(QKeySequence(tr("Ctrl+R")));
    replace_action->setToolTip(tr("Replace text (Ctrl+R)"));
    tool_bar->addAction(replace_action);
    connect(replace_action, &QAction::triggered, this, &TextViewer::replace_text);

    // Add Go to Line button with Ctrl+G shortcut
    go_to_line_action = new QAction(tr("Go to Line"), this);
    go_to_line_action->setShortcut(QKeySequence(tr("Ctrl+G")));
    go_to_line_action->setToolTip(tr("Go to line (Ctrl+G)"));
    tool_bar->addAction(go_to_line_action);
    connect(go_to_line_action, &QAction::triggered, this, &TextViewer::goto_line);

    // Create text edit
    text_edit = new QTextEdit(this);
    text_edit->setAcceptRichText(false);
    text_edit->setFocusPolicy(Qt::StrongFocus);
    text_edit->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);

    // Create statusbar
    status_bar = new QStatusBar(this);
    line_column_label = new QLabel(tr("Line: 1, Column: 1"), this);
    status_bar->addPermanentWidget(line_column_label);

    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(tool_bar);
    layout->addWidget(text_edit);
    layout->addWidget(status_bar);
    setLayout(layout);

    // resize(800, 600);
    set_current_file(QString());

    connect(text_edit, &QTextEdit::cursorPositionChanged, this, &TextViewer::highlight_current_line);
    connect(text_edit, &QTextEdit::selectionChanged, this, &TextViewer::highlight_current_line);
    connect(text_edit, &QTextEdit::cursorPositionChanged, this, &TextViewer::update_status_bar);
    highlight_current_line();
    update_status_bar();
}

TextViewer::~TextViewer()
{
}

void TextViewer::show()
{
    QWidget::show();
    text_edit->show();
    text_edit->setFocus(Qt::OtherFocusReason);
    text_edit->ensureCursorVisible();
}

int TextViewer::find_file(QStringList &search_dirs, const QString &initial_dir, const QString &initial_pattern)
{
    // Create custom dialog
    int file_opened = -2;
    QDialog dialog(this);
    dialog.setWindowTitle(tr("Find File"));
    dialog.setMinimumWidth(600);
    dialog.setMinimumHeight(500);

    QVBoxLayout *main_layout = new QVBoxLayout(&dialog);

    // Directory selection
    QHBoxLayout *dir_layout = new QHBoxLayout();
    QLabel *dir_label = new QLabel(tr("Search in:"), &dialog);
    QLineEdit *dir_edit = new QLineEdit(&dialog);
    QString search_dir = initial_dir.isEmpty() ? QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) : initial_dir;
    dir_edit->setText(search_dir);
    QPushButton *browse_btn = new QPushButton(tr("Browse..."), &dialog);

    dir_layout->addWidget(dir_label);
    dir_layout->addWidget(dir_edit);
    dir_layout->addWidget(browse_btn);

    // File pattern
    QHBoxLayout *pattern_layout = new QHBoxLayout();
    QLabel *pattern_label = new QLabel(tr("File pattern:"), &dialog);
    QLineEdit *pattern_edit = new QLineEdit(&dialog);
    QString search_pattern = initial_pattern.isEmpty() ? "*.txt" : initial_pattern;
    pattern_edit->setText(search_pattern);
    pattern_edit->setPlaceholderText(tr("e.g., *.txt, *.cpp, myfile*"));
    QPushButton *search_btn = new QPushButton(tr("Search"), &dialog);

    pattern_layout->addWidget(pattern_label);
    pattern_layout->addWidget(pattern_edit);
    pattern_layout->addWidget(search_btn);

    // Results list
    QLabel *results_label = new QLabel(tr("Results:"), &dialog);
    QListWidget *results_list = new QListWidget(&dialog);

    // Buttons
    QHBoxLayout *button_layout = new QHBoxLayout();
    QPushButton *open_btn = new QPushButton(tr("Open"), &dialog);
    open_btn->setEnabled(false);
    QPushButton *close_btn = new QPushButton(tr("Close"), &dialog);

    button_layout->addStretch();
    button_layout->addWidget(open_btn);
    button_layout->addWidget(close_btn);

    main_layout->addLayout(dir_layout);
    main_layout->addLayout(pattern_layout);
    main_layout->addWidget(results_label);
    main_layout->addWidget(results_list);
    main_layout->addLayout(button_layout);

    // Connect browse button
    connect(browse_btn, &QPushButton::clicked, [&]() {
        QString dir = QFileDialog::getExistingDirectory(
            &dialog,
            tr("Select Directory"),
            dir_edit->text(),
            QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
        if (!dir.isEmpty()) {
            dir_edit->setText(dir);
        }
    });

    // Connect search button
    connect(search_btn, &QPushButton::clicked, [&]() {
        results_list->clear();
        QString dir_path = dir_edit->text();
        QString pattern = pattern_edit->text().trimmed();

        if (dir_path.isEmpty()) {
            QMessageBox::warning(&dialog, tr("Find File"), tr("Please specify a directory."));
            return;
        }

        if (pattern.isEmpty()) {
            pattern = "*";
        }

        QApplication::setOverrideCursor(Qt::WaitCursor);
        QStringList results;
        search_files_in_directory(dir_path, pattern, results, 500);
        QApplication::restoreOverrideCursor();

        if (results.isEmpty()) {
            results_list->addItem(tr("No files found."));
            open_btn->setEnabled(false);
        } else {
            for (const QString &file_path : std::as_const(results)) {
                results_list->addItem(file_path);
            }
            if (results.size() >= 500) {
                results_list->addItem(tr("... (search limited to 500 results)"));
            }
        }
    });

    // Enable/disable open button based on selection
    connect(results_list, &QListWidget::itemSelectionChanged, [&]() {
        bool has_selection = results_list->currentItem() != nullptr;
        QString item_text = has_selection ? results_list->currentItem()->text() : "";
        open_btn->setEnabled(has_selection && !item_text.startsWith("...") && !item_text.startsWith("No files"));
    });

    // Open on double-click
    connect(results_list, &QListWidget::itemDoubleClicked, [&](QListWidgetItem *item) {
        QString file_path = item->text();
        if (!file_path.startsWith("...") && !file_path.startsWith("No files")) {
            search_dirs.append(dir_edit->text());
            file_opened = load_file(file_path);
            dialog.accept();
        }
    });

    // Connect open button
    connect(open_btn, &QPushButton::clicked, [&]() {
        QListWidgetItem *item = results_list->currentItem();
        if (item) {
            QString file_path = item->text();
            search_dirs.append(dir_edit->text());
            load_file(file_path);
            dialog.accept();
        }
    });

    // Connect close button
    connect(close_btn, &QPushButton::clicked, &dialog, &QDialog::reject);

    // Auto-search on Enter in pattern field
    connect(pattern_edit, &QLineEdit::returnPressed, search_btn, &QPushButton::click);

    dialog.exec();
    return file_opened;
}

void TextViewer::search_files_in_directory(const QString &dir_path, const QString &pattern, QStringList &results, int max_results)
{
    QDir dir(dir_path);
    if (!dir.exists()) {
        return;
    }

    QDirIterator it(dir_path, QStringList() << pattern, QDir::Files, QDirIterator::Subdirectories);

    while (it.hasNext() && results.size() < max_results) {
        results.append(it.next());
    }
}

QStringList TextViewer::find_files(const QString &search_dir, const QString &pattern, int max_results)
{
    QStringList results;

    if (search_dir.isEmpty()) {
        return results;
    }

    QString search_pattern = pattern.isEmpty() ? "*" : pattern;
    search_files_in_directory(search_dir, search_pattern, results, max_results);

    return results;
}

void TextViewer::goto_line()
{
    bool ok;
    int total_lines = text_edit->document()->blockCount();
    int current_line = text_edit->textCursor().blockNumber() + 1;

    int line_number = QInputDialog::getInt(
        this,
        tr("Go to Line"),
        tr("Enter line number (1-%1):").arg(total_lines),
        current_line,
        1,
        total_lines,
        1,
        &ok);

    if (ok) {
        QTextCursor cursor(text_edit->document()->findBlockByLineNumber(line_number - 1));
        text_edit->setTextCursor(cursor);
        text_edit->setFocus();
    }
}

void TextViewer::go_to_line(int line)
{
    QTextCursor cursor(text_edit->document()->findBlockByLineNumber(line - 1));
    text_edit->setTextCursor(cursor);
    text_edit->ensureCursorVisible();
}

void TextViewer::set_current_file(const QString &file_name)
{
    current_file = file_name;
    text_edit->document()->setModified(false);

    QString shown_name = current_file.isEmpty() ? tr("untitled.txt") : stripped_name(current_file);
}

QString TextViewer::stripped_name(const QString &full_file_name)
{
    return QFileInfo(full_file_name).fileName();
}

int TextViewer::load_file(QStringList &search_dirs, const QString &file_name)
{
    QFile file(file_name);
    if (!file.open(QFile::ReadOnly | QFile::Text)) {
        for (QString search_dir : search_dirs) {
            QFileInfo file_info(file_name);
            QString search_pattern = file_info.fileName();
            QStringList files = find_files(search_dir, search_pattern);
            if (!files.empty()) {
                file.setFileName(*files.begin());
                break;
            }
        }
    }
    if (!file.open(QFile::ReadOnly | QFile::Text)) {
        QMessageBox::StandardButton reply = QMessageBox::question(
            this,
            tr("Text Viewer"),
            tr("Cannot read file %1:\n%2.\n\nWould you like to search for this file?")
                .arg(QDir::toNativeSeparators(file_name), file.errorString()),
            QMessageBox::Yes | QMessageBox::No,
            QMessageBox::Yes);

        if (reply == QMessageBox::Yes) {
            // Extract directory and filename from the failed path
            QFileInfo file_info(file_name);
            QString search_dir = file_info.absolutePath();
            QString search_pattern = file_info.fileName();

            // If the directory doesn't exist, use the user's home directory
            if (!QDir(search_dir).exists()) {
                search_dir = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
            }

            return find_file(search_dirs, search_dir, search_pattern);
        } else {
            return -1;
        }
    }

    QTextStream in(&file);
    QApplication::setOverrideCursor(Qt::WaitCursor);
    QString content = in.readAll();
    QApplication::restoreOverrideCursor();
    do_load_file(file.fileName(), content);
    return 0;
}

int TextViewer::load_file(const QString &file_name)
{
    QFile file(file_name);
    if (!file.open(QFile::ReadOnly | QFile::Text)) {
        return -1;
    }

    QTextStream in(&file);
    QApplication::setOverrideCursor(Qt::WaitCursor);
    QString content = in.readAll();
    QApplication::restoreOverrideCursor();

    do_load_file(file_name, content);
    return 0;
}

void TextViewer::highlight_current_line()
{
    QList<QTextEdit::ExtraSelection> extraSelections;

    // Only highlight current line if there's no text selection
    if (!text_edit->textCursor().hasSelection()) {
        QTextEdit::ExtraSelection selection;

        QColor lineColor = QColor(Qt::yellow).lighter(160);
        QColor textColor = QColor(Qt::black); // Dark text for good contrast

        selection.format.setBackground(lineColor);
        selection.format.setForeground(textColor); // Set text color
        selection.format.setProperty(QTextFormat::FullWidthSelection, true);
        selection.cursor = text_edit->textCursor();
        selection.cursor.clearSelection();
        extraSelections.append(selection);
    }

    text_edit->setExtraSelections(extraSelections);
}

void TextViewer::do_load_file(const QString &file_name, const QString &content)
{
    // Set the content
    text_edit->setPlainText(content);
    text_edit->setReadOnly(true);
    text_edit->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);

    set_current_file(file_name);

    // Calculate optimal window size based on content
    QStringList lines = content.split('\n');
    int lineCount = lines.size();
    int maxLineLength = 0;

    for (const QString &line : lines) {
        if (line.length() > maxLineLength) {
            maxLineLength = line.length();
        }
    }

    // Use font metrics to calculate actual width
    QFontMetrics fm(text_edit->font());
    int char_width = std::min(fm.maxWidth(), fm.averageCharWidth() * 2);
    int line_height = fm.lineSpacing();

    // Calculate desired size with some padding
    int desired_width = maxLineLength * char_width + 60; // +60 for scrollbar and margins
    int desired_height = lineCount * line_height + 60;   // +60 for margins

    // Apply reasonable limits
    QScreen *screen = QApplication::primaryScreen();
    QRect screen_geometry = screen->availableGeometry();
    int max_width = screen_geometry.width() * 0.9;
    int max_height = screen_geometry.height() * 0.9;
    int min_width = 400;
    int min_height = 300;

    int final_width = qBound(min_width, desired_width, max_width);
    int final_height = qBound(min_height, desired_height, max_height);

    resize(final_width, final_height);

    text_edit->setFocus();
}

void TextViewer::update_status_bar()
{
    QTextCursor cursor = text_edit->textCursor();
    int line = cursor.blockNumber() + 1;    // blockNumber is 0-based
    int column = cursor.columnNumber() + 1; // columnNumber is 0-based

    line_column_label->setText(tr("Line: %1, Column: %2").arg(line).arg(column));
}

void TextViewer::on_save_toggled(bool checked)
{
    if (checked) {
        // Enable editing
        text_edit->setReadOnly(false);
        text_edit->setTextInteractionFlags(Qt::TextEditorInteraction);
        // save_toggle_check_box->setText(tr("Disable Save"));
        save_toggle_check_box->setToolTip(tr("Uncheck to disable editing (switch to read-only mode)"));
        save_action->setEnabled(true); // Enable save button
        status_bar->showMessage(tr("Editor is now editable"), 3000);
    } else {
        // Disable editing (view-only mode)
        text_edit->setReadOnly(true);
        text_edit->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);
        // save_toggle_check_box->setText(tr("Enable Save"));
        save_toggle_check_box->setToolTip(tr("Check to enable file saving (makes editor editable)"));
        save_action->setEnabled(false); // Disable save button
        status_bar->showMessage(tr("Editor is now read-only"), 3000);
    }
}

void TextViewer::save_file()
{
    if (current_file.isEmpty()) {
        status_bar->showMessage(tr("No file to save"), 3000);
        return;
    }

    QFile file(current_file);
    if (!file.open(QFile::WriteOnly | QFile::Text)) {
        QMessageBox::warning(
            this,
            tr("Text Viewer"),
            tr("Cannot write file %1:\n%2.")
                .arg(QDir::toNativeSeparators(current_file), file.errorString()));
        return;
    }

    QTextStream out(&file);
    QApplication::setOverrideCursor(Qt::WaitCursor);
    out << text_edit->toPlainText();
    QApplication::restoreOverrideCursor();

    text_edit->document()->setModified(false);
    status_bar->showMessage(tr("File saved successfully"), 3000);
}

void TextViewer::find_text()
{
    // Create custom find dialog
    QDialog dialog(this);
    dialog.setWindowTitle(tr("Find Text"));
    dialog.setMinimumWidth(400);

    QVBoxLayout *main_layout = new QVBoxLayout(&dialog);

    // Search text input
    QHBoxLayout *search_layout = new QHBoxLayout();
    QLabel *search_label = new QLabel(tr("Find:"), &dialog);
    QLineEdit *search_edit = new QLineEdit(&dialog);

    // Pre-fill with selected text if any
    QString selected_text = text_edit->textCursor().selectedText();
    if (!selected_text.isEmpty()) {
        search_edit->setText(selected_text);
    }

    search_layout->addWidget(search_label);
    search_layout->addWidget(search_edit);

    // Checkboxes for options
    QCheckBox *case_sensitive_check = new QCheckBox(tr("Case sensitive"), &dialog);
    QCheckBox *whole_words_check = new QCheckBox(tr("Whole words only"), &dialog);

    // Buttons
    QHBoxLayout *button_layout = new QHBoxLayout();
    QPushButton *find_next_btn = new QPushButton(tr("Find Next"), &dialog);
    QPushButton *find_prev_btn = new QPushButton(tr("Find Previous"), &dialog);
    QPushButton *close_btn = new QPushButton(tr("Close"), &dialog);

    button_layout->addWidget(find_next_btn);
    button_layout->addWidget(find_prev_btn);
    button_layout->addStretch();
    button_layout->addWidget(close_btn);

    main_layout->addLayout(search_layout);
    main_layout->addWidget(case_sensitive_check);
    main_layout->addWidget(whole_words_check);
    main_layout->addLayout(button_layout);

    // Find function
    auto find_function = [&](bool forward) {
        QString search_text_local = search_edit->text();
        if (search_text_local.isEmpty()) {
            status_bar->showMessage(tr("Please enter search text"), 3000);
            return;
        }

        QTextDocument::FindFlags flags;
        if (!forward) {
            flags |= QTextDocument::FindBackward;
        }
        if (case_sensitive_check->isChecked()) {
            flags |= QTextDocument::FindCaseSensitively;
        }
        if (whole_words_check->isChecked()) {
            flags |= QTextDocument::FindWholeWords;
        }

        bool found = text_edit->find(search_text_local, flags);
        if (!found) {
            // Wrap around search
            QTextCursor cursor = text_edit->textCursor();
            if (forward) {
                text_edit->moveCursor(QTextCursor::Start);
            } else {
                text_edit->moveCursor(QTextCursor::End);
            }
            found = text_edit->find(search_text_local, flags);

            if (found) {
                status_bar->showMessage(tr("Search wrapped"), 2000);
            } else {
                status_bar->showMessage(tr("Text not found"), 3000);
            }
        }
    };

    // Connect buttons
    connect(find_next_btn, &QPushButton::clicked, [&]() { find_function(true); });
    connect(find_prev_btn, &QPushButton::clicked, [&]() { find_function(false); });
    connect(close_btn, &QPushButton::clicked, &dialog, &QDialog::accept);
    connect(search_edit, &QLineEdit::returnPressed, [&]() { find_function(true); });

    search_edit->setFocus();
    dialog.exec();
}

void TextViewer::replace_text()
{
    // Only allow replace in edit mode
    if (text_edit->isReadOnly()) {
        QMessageBox::information(
            this,
            tr("Replace Text"),
            tr("Please enable editing mode (check 'Allow Save') to use replace functionality."));
        return;
    }

    // Create custom replace dialog
    QDialog dialog(this);
    dialog.setWindowTitle(tr("Replace Text"));
    dialog.setMinimumWidth(450);

    QVBoxLayout *main_layout = new QVBoxLayout(&dialog);

    // Find text input
    QHBoxLayout *find_layout = new QHBoxLayout();
    QLabel *find_label = new QLabel(tr("Find:"), &dialog);
    find_label->setMinimumWidth(60);
    QLineEdit *find_edit = new QLineEdit(&dialog);

    // Pre-fill with selected text if any
    QString selected_text = text_edit->textCursor().selectedText();
    if (!selected_text.isEmpty()) {
        find_edit->setText(selected_text);
    }

    find_layout->addWidget(find_label);
    find_layout->addWidget(find_edit);

    // Replace text input
    QHBoxLayout *replace_layout = new QHBoxLayout();
    QLabel *replace_label = new QLabel(tr("Replace:"), &dialog);
    replace_label->setMinimumWidth(60);
    QLineEdit *replace_edit = new QLineEdit(&dialog);

    replace_layout->addWidget(replace_label);
    replace_layout->addWidget(replace_edit);

    // Checkboxes for options
    QCheckBox *case_sensitive_check = new QCheckBox(tr("Case sensitive"), &dialog);
    QCheckBox *whole_words_check = new QCheckBox(tr("Whole words only"), &dialog);

    // Buttons
    QHBoxLayout *button_layout1 = new QHBoxLayout();
    QPushButton *find_next_btn = new QPushButton(tr("Find Next"), &dialog);
    QPushButton *replace_btn = new QPushButton(tr("Replace"), &dialog);
    QPushButton *replace_all_btn = new QPushButton(tr("Replace All"), &dialog);

    button_layout1->addWidget(find_next_btn);
    button_layout1->addWidget(replace_btn);
    button_layout1->addWidget(replace_all_btn);

    QHBoxLayout *button_layout2 = new QHBoxLayout();
    QPushButton *close_btn = new QPushButton(tr("Close"), &dialog);
    button_layout2->addStretch();
    button_layout2->addWidget(close_btn);

    main_layout->addLayout(find_layout);
    main_layout->addLayout(replace_layout);
    main_layout->addWidget(case_sensitive_check);
    main_layout->addWidget(whole_words_check);
    main_layout->addLayout(button_layout1);
    main_layout->addLayout(button_layout2);

    // Find function
    auto find_function = [&]() -> bool {
        QString search_text_local = find_edit->text();
        if (search_text_local.isEmpty()) {
            status_bar->showMessage(tr("Please enter search text"), 3000);
            return false;
        }

        QTextDocument::FindFlags flags;
        if (case_sensitive_check->isChecked()) {
            flags |= QTextDocument::FindCaseSensitively;
        }
        if (whole_words_check->isChecked()) {
            flags |= QTextDocument::FindWholeWords;
        }

        bool found = text_edit->find(search_text_local, flags);
        if (!found) {
            // Wrap around search
            text_edit->moveCursor(QTextCursor::Start);
            found = text_edit->find(search_text_local, flags);

            if (found) {
                status_bar->showMessage(tr("Search wrapped"), 2000);
            } else {
                status_bar->showMessage(tr("Text not found"), 3000);
            }
        }
        return found;
    };

    // Replace current selection
    auto replace_function = [&]() {
        if (text_edit->textCursor().hasSelection()) {
            QTextCursor cursor = text_edit->textCursor();
            cursor.insertText(replace_edit->text());
            status_bar->showMessage(tr("Replaced"), 2000);
            find_function(); // Find next occurrence
        } else {
            find_function();
        }
    };

    // Replace all occurrences
    auto replace_all_function = [&]() {
        QString search_text_local = find_edit->text();
        if (search_text_local.isEmpty()) {
            status_bar->showMessage(tr("Please enter search text"), 3000);
            return;
        }

        QTextDocument::FindFlags flags;
        if (case_sensitive_check->isChecked()) {
            flags |= QTextDocument::FindCaseSensitively;
        }
        if (whole_words_check->isChecked()) {
            flags |= QTextDocument::FindWholeWords;
        }

        int count = 0;
        text_edit->moveCursor(QTextCursor::Start);

        while (text_edit->find(search_text_local, flags)) {
            QTextCursor cursor = text_edit->textCursor();
            cursor.insertText(replace_edit->text());
            count++;
        }

        if (count > 0) {
            status_bar->showMessage(tr("Replaced %1 occurrence(s)").arg(count), 3000);
        } else {
            status_bar->showMessage(tr("Text not found"), 3000);
        }
    };

    // Connect buttons
    connect(find_next_btn, &QPushButton::clicked, find_function);
    connect(replace_btn, &QPushButton::clicked, replace_function);
    connect(replace_all_btn, &QPushButton::clicked, replace_all_function);
    connect(close_btn, &QPushButton::clicked, &dialog, &QDialog::accept);
    connect(find_edit, &QLineEdit::returnPressed, find_function);

    find_edit->setFocus();
    dialog.exec();
}

int TextViewer::search_text(const QString &search_string,
                           bool forward,
                           SearchStartPosition start_pos,
                           bool case_sensitive,
                           bool whole_words)
{
    if (search_string.isEmpty()) {
        status_bar->showMessage(tr("Search string is empty"), 3000);
        return -1;
    }

    // Save current cursor position for potential restoration
    QTextCursor original_cursor = text_edit->textCursor();

    // Set the start position based on the parameter
    switch (start_pos) {
    case SearchFromBeginning:
        text_edit->moveCursor(QTextCursor::Start);
        break;
    case SearchFromEnd:
        text_edit->moveCursor(QTextCursor::End);
        break;
    case SearchFromCurrent:
        // Don't move, stay at current position
        break;
    }

    // Build find flags
    QTextDocument::FindFlags flags;
    if (!forward) {
        flags |= QTextDocument::FindBackward;
    }
    if (case_sensitive) {
        flags |= QTextDocument::FindCaseSensitively;
    }
    if (whole_words) {
        flags |= QTextDocument::FindWholeWords;
    }

    // Perform the search
    bool found = text_edit->find(search_string, flags);

    if (found) {
        // Get the line number where the text was found (1-based)
        QTextCursor cursor = text_edit->textCursor();
        int line_number = cursor.blockNumber() + 1;

        // Ensure the found text is visible
        text_edit->ensureCursorVisible();
        status_bar->showMessage(tr("Found: \"%1\" at line %2").arg(search_string).arg(line_number), 3000);
        return line_number;
    } else {
        // Restore original cursor position if not found
        text_edit->setTextCursor(original_cursor);
        status_bar->showMessage(tr("Text not found: \"%1\"").arg(search_string), 3000);
        return -1;
    }
}
