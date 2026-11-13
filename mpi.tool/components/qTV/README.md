# qTV - Qt Trace Viewer

Part of peat.

## What it does

A Qt-based GUI application to load and visually inspect `tractor` traces: a timeline view of MPI/collective/user-region events per rank/thread, message legends, cut-off filtering and a text viewer for raw trace records.

Available on Linux and Windows.

A trace info file can be given on the command line (`qTV run.trace.info`) or dropped onto the window. `Help > Contents` (F1) opens the help, `Help > Controls` its page with the mouse and keyboard controls.

The help is built from `help/qTV.qhp` into `qTV.qch`, which has to stay next to the executable. This needs the Qt Help module and `qhelpgenerator` (Qt Tools); without them, or without `qTV.qch`, qTV shows the copy of the same pages built into the executable.

Navigation and analysis aids:

- time axis above the picture (time from the start of the loaded trace) with an overview bar of the whole loaded trace;
- view history (`Alt+Left`/`Alt+Right`, mouse back/forward), bookmarks per trace, `Go to time`;
- search by rank and index, or by function name (next/previous call);
- statistics of the view or of a selected region: time per function, MPI time per rank, P2P messages per rank pair;
- list of the views found by the `Analyse` actions, including late sender / late receiver messages;
- a click selects an event and frames the events joined with it by messages;
- row height (`Ctrl+Up`/`Ctrl+Down`), colouring by function name, dimming of all but one function, a legend window;
- `Esc` cancels a rendering in progress;
- copy/CSV export from the tables, snapshot to a file (PNG, JPEG, PDF) or to the clipboard.


## Layout

- `main.cpp`, `qTV.cpp`/`qTV.h` - application entry point and top-level widget.
- `tvmainwindow.cpp`/`.h`/`.ui` - main window and menu/toolbar wiring (`tvmenubar.cpp`/`.h`).
- `timeline`/`tv_event.cpp`/`.h` - trace timeline rendering and event model.
- `traceinfo.cpp`/`.h`, `tracedialog.cpp`/`.h`/`.ui` - trace file loading/selection dialogs.
- `messagestabwidget.cpp`/`.h`/`.ui` - per-message detail tab widget.
- `legenddialog.cpp`/`.h`/`.ui`, `cutoffdialog.cpp`/`.h`/`.ui`, `colorlistwidget.cpp`/`.h` - legend, filtering and colour-customisation dialogs.
- `timeruler.cpp`/`.h` - time axis and overview bar above the picture.
- `statsdialog.cpp`/`.h`/`.ui` - statistics of a time window.
- `finddialog.cpp`/`.h`/`.ui`, `candidatesdialog.cpp`/`.h`/`.ui`, `highlightdialog.cpp`/`.h`/`.ui` - find a function by name, list of the views found by Analyse, highlighting conditions.
- `tableexport.cpp`/`.h`, `tableitems.h` - copy/CSV export menu and sortable cells for tables and trees.
- `help/` - help pages (`*.html`) and the Qt Help project `qTV.qhp`; `helpviewer.cpp`/`.h` shows them, `help.qrc` embeds a copy.
- `textviewer.cpp`/`.h` - raw trace text viewer.
- `elements.cpp`/`.h`, `globals.cpp`/`.h` - shared UI element helpers and global/application state.
- `CMakeLists.txt`/`build.sh` - build configuration for the Qt application.
 