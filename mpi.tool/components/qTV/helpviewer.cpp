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

#include "helpviewer.h"
#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QPushButton>
#include <QSplitter>
#include <QStandardPaths>
#include <QTextBrowser>
#include <QVBoxLayout>
#ifdef QTV_HAS_QTHELP
#include <QHelpContentWidget>
#include <QHelpEngine>
#endif

namespace
{
// Text browser that takes the pages of the qthelp: scheme from the help engine.
class Help_Browser : public QTextBrowser
{
  public:
    explicit Help_Browser(QWidget *parent) : QTextBrowser(parent), help_engine(0) {}
    void set_engine(QHelpEngine *e)
    {
        help_engine = e;
    }
    QVariant loadResource(int type, const QUrl &name) override
    {
#ifdef QTV_HAS_QTHELP
        const QUrl url = name.isRelative() ? source().resolved(name) : name;
        if (help_engine && url.scheme() == QLatin1String("qthelp")) {
            return help_engine->fileData(url);
        }
#endif
        return QTextBrowser::loadResource(type, name);
    }

  private:
    QHelpEngine *help_engine;
};
} // namespace

HelpViewer::HelpViewer(QWidget *parent) : QDialog(parent), browser(0), engine(0)
{
    setWindowTitle(tr("qTV Help"));
    setSizeGripEnabled(true);
    resize(950, 700);
    QVBoxLayout *layout = new QVBoxLayout(this);
    QPushButton *back_button = new QPushButton(tr("&Back"), this);
    QPushButton *forward_button = new QPushButton(tr("&Forward"), this);
    QPushButton *home_button = new QPushButton(tr("&Contents"), this);
    QHBoxLayout *buttons = new QHBoxLayout;
    buttons->addWidget(back_button);
    buttons->addWidget(forward_button);
    buttons->addWidget(home_button);
    buttons->addStretch(1);
    layout->addLayout(buttons);
    QSplitter *splitter = new QSplitter(this);
    layout->addWidget(splitter, 1);
    Help_Browser *help_browser = new Help_Browser(splitter);
    browser = help_browser;
    browser->setOpenExternalLinks(true);
    connect(back_button, &QPushButton::clicked, browser, &QTextBrowser::backward);
    connect(forward_button, &QPushButton::clicked, browser, &QTextBrowser::forward);
    connect(home_button, &QPushButton::clicked, this, [this]() { show_page("index.html"); });
    connect(browser, &QTextBrowser::backwardAvailable, back_button, &QPushButton::setEnabled);
    connect(browser, &QTextBrowser::forwardAvailable, forward_button, &QPushButton::setEnabled);
    back_button->setEnabled(false);
    forward_button->setEnabled(false);

    if (setup_qt_help()) {
#ifdef QTV_HAS_QTHELP
        help_browser->set_engine(engine);
        QHelpContentWidget *contents = engine->contentWidget();
        splitter->insertWidget(0, contents);
        splitter->setStretchFactor(0, 1);
        splitter->setStretchFactor(1, 3);
        connect(contents, &QHelpContentWidget::linkActivated, browser, [this](const QUrl &link) { browser->setSource(link); });
        connect(engine->contentModel(), &QHelpContentModel::contentsCreated, contents, [contents]() { contents->expandAll(); });
#endif
    } else {
        base_url = "qrc:/help/";
    }
}

// Registers qTV.qch in a help collection of the user. False if Qt Help cannot be used.
bool HelpViewer::setup_qt_help()
{
#ifdef QTV_HAS_QTHELP
    const QString qch = QCoreApplication::applicationDirPath() + "/qTV.qch"; // installed next to the executable
    if (!QFileInfo::exists(qch)) {
        return false;
    }
    // the collection file is a small database the help engine writes to: it has to be in a writable place
    const QString data_dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (data_dir.isEmpty() || !QDir().mkpath(data_dir)) {
        qWarning() << "qTV help: cannot create" << data_dir << "for the help collection - the built-in pages are shown";
        return false;
    }
    engine = new QHelpEngine(data_dir + "/qTV.qhc", this);
    engine->setReadOnly(false); // read-only by default, and then nothing can be registered
    const QString name_space = QHelpEngineCore::namespaceName(qch);
    bool ready = !name_space.isEmpty() && engine->setupData();
    if (ready) {
        // registered again on every start: the file may have been replaced by a newer one, or moved
        if (engine->registeredDocumentations().contains(name_space)) {
            engine->unregisterDocumentation(name_space);
        }
        ready = engine->registerDocumentation(qch) && engine->setupData();
    }
    if (!ready) {
        qWarning() << "qTV help: cannot use" << qch << "-" << engine->error() << "- the built-in pages are shown";
        delete engine;
        engine = 0;
        return false;
    }
    base_url = "qthelp://" + name_space + "/doc/"; // "doc" is the virtual folder of help/qTV.qhp
    return true;
#else
    return false;
#endif
}

void HelpViewer::show_page(const QString &page)
{
    browser->setSource(QUrl(base_url + page));
    show();
    raise();
    activateWindow();
}
