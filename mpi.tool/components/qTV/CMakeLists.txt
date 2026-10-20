#
# This file is part of PEAT (Performance Estimation & Analysing Toolkit).
# Copyright (C) 2026 by Alex Knyazev
#
# PEAT is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# PEAT is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with PEAT.  If not, see <https://www.gnu.org/licenses/>.
#

cmake_minimum_required(VERSION 3.16)

project(qTV VERSION 0.1 LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CONFIGURATION_TYPES "Debug;Release;RelWithDebInfo;MinSizeRel" CACHE STRING "Available build types" FORCE)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set (CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Core Widgets Svg)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core Widgets Svg)
find_package(OpenMP REQUIRED)

set(SOURCES
		main.cpp
		tvmenubar.cpp
		tvmainwindow.cpp
		tvmainwindow.ui
		tracedialog.cpp
		tracedialog.ui
		qTV.cpp
		elements.cpp
		tdebug.cpp
		tv_event.cpp
		trace_info.cpp
		globals.cpp
		cutoffdialog.cpp
		cutoffdialog.ui
		legenddialog.cpp
		legenddialog.ui
		colorlistwidget.cpp
		messagestabwidget.cpp
		messagestabwidget.ui
		functionstabwidget.cpp
		functionstabwidget.ui
		textviewer.cpp
		communicatorsdialog.cpp
		communicatorsdialog.ui
		filterdialog.cpp
		filterdialog.ui
		icons.qrc
		../common/file_io.cpp
		../common/common_types.cpp
		../common/func_id.cpp
		../common/utils.cpp
		../common/ptp.cpp
		../common/communicators.cpp
		../common/model_id.cpp
)

# Header files
set(HEADERS
        tvmenubar.h
        qTV.h
        legenddialog.h
        globals.h
        tv_event.h
        elements.h
        trace_info.h
        cutoffdialog.h
	colorlistwidget.h 
        textviewer.h
        tvmainwindow.h
        tracedialog.h
        messagestabwidget.h
        functionstabwidget.h
        communicatorsdialog.h
        filterdialog.h 
        ../common/file_io.h
        ../common/common_types.h
        ../common/func_id.h
        ../common/utils.h
        ../common/ptp.h
        ../common/model_id.h
)

#find_library(zlib zlib ../external/zlib/lib64)
include_directories(.)

# By default, qTV ignores mat's processed .mpi/.hwc output and always reads the original raw
# trace files (doing its own PTP correction). Remove this define to make qTV prefer mat's
# processed .mpi/.hwc output when present, falling back to raw only if it's missing.
add_compile_definitions(TV_IGNORE_PROCESSED_OUTPUT)

if (MSVC)
#include_directories(../contrib/zlib/install/include ../contrib/jemalloc/include ../contrib/jemalloc/include/msvc_compat)
include_directories(../contrib/zlib/install/msvc/include)
link_directories(../contrib/zlib/install/msvc/lib)
#link_directories(../contrib/zlib/install/lib ../contrib/jemalloc/lib64)
#add_compile_definitions(USE_JE_MALLOC JEMALLOC_STATIC JEMALLOC_EXPORT= JEMALLOC_NO_DEMANGLE)
set(TV_ZLIB libzs)
elseif (WIN32)
include_directories(../contrib/zlib/install/mingw/include)
link_directories(../contrib/zlib/install/mingw/lib)
set(TV_ZLIB zs)
else()
set(TV_ZLIB z)
endif()

if (${MSVC})
set_property(SOURCE tdebug.cpp  APPEND_STRING PROPERTY COMPILE_FLAGS "/Od")
else()
set_property(SOURCE tdebug.cpp  APPEND_STRING PROPERTY COMPILE_FLAGS "-O0")
endif()


if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
    qt_add_executable(qTV
        MANUAL_FINALIZATION
        ${SOURCES}
        ${HEADERS}
        
    )
# Define target properties for Android with Qt 6 as:
#    set_property(TARGET qTV APPEND PROPERTY QT_ANDROID_PACKAGE_SOURCE_DIR
#                 ${CMAKE_CURRENT_SOURCE_DIR}/android)
# For more information, see https://doc.qt.io/qt-6/qt-add-executable.html#target-creation
else()
    if(ANDROID)
        add_library(qTV SHARED
            ${SOURCES}
        )
# Define properties for Android with Qt 5 after find_package() calls as:
#    set(ANDROID_PACKAGE_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/android")
    else()
        add_executable(qTV
            ${SOURCES}
        )
    endif()
endif()

target_link_libraries(qTV PRIVATE Qt${QT_VERSION_MAJOR}::Widgets Qt${QT_VERSION_MAJOR}::Svg OpenMP::OpenMP_CXX ${TV_ZLIB})
#target_link_libraries(qTV PRIVATE Qt${QT_VERSION_MAJOR}::Widgets Qt${QT_VERSION_MAJOR}::Svg OpenMP::OpenMP_CXX ${TV_ZLIB} jemalloc)


# Qt for iOS sets MACOSX_BUNDLE_GUI_IDENTIFIER automatically since Qt 6.1.
# If you are developing for iOS or macOS you should consider setting an
# explicit, fixed bundle identifier manually though.
if(${QT_VERSION} VERSION_LESS 6.1.0)
  set(BUNDLE_ID_OPTION MACOSX_BUNDLE_GUI_IDENTIFIER com.example.qTV)
endif()
set_target_properties(qTV PROPERTIES
    ${BUNDLE_ID_OPTION}
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

qt_disable_unicode_defines(qTV)

include(GNUInstallDirs)
install(TARGETS qTV
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

if(QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_executable(qTV)
endif()
