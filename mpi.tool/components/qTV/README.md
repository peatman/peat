# qTV - Qt Trace Viewer

Part of peat.

## What it does

A Qt-based GUI application to load and visually inspect `tractor` traces: a timeline view of MPI/collective/user-region events per rank/thread, message legends, cut-off filtering and a text viewer for raw trace records.

Available on Linux and Windows.


## Layout

- `main.cpp`, `qTV.cpp`/`qTV.h` - application entry point and top-level widget.
- `tvmainwindow.cpp`/`.h`/`.ui` - main window and menu/toolbar wiring (`tvmenubar.cpp`/`.h`).
- `timeline`/`tv_event.cpp`/`.h` - trace timeline rendering and event model.
- `traceinfo.cpp`/`.h`, `tracedialog.cpp`/`.h`/`.ui` - trace file loading/selection dialogs.
- `messagestabwidget.cpp`/`.h`/`.ui` - per-message detail tab widget.
- `legenddialog.cpp`/`.h`/`.ui`, `cutoffdialog.cpp`/`.h`/`.ui`, `colorlistwidget.cpp`/`.h` - legend, filtering and colour-customisation dialogs.
- `textviewer.cpp`/`.h` - raw trace text viewer.
- `elements.cpp`/`.h`, `globals.cpp`/`.h` - shared UI element helpers and global/application state.
- `CMakeLists.txt`/`build.sh` - build configuration for the Qt application.
 