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
# CMake build for ctt (mirrors src/Makefile) - a single serial executable, no
# MPI/TARGET variants. Point cmake at a specific toolchain the usual way, e.g.
#   cmake -DCMAKE_CXX_COMPILER=icpx ..
#
cmake_minimum_required(VERSION 3.16...4.4)
project(ctt CXX)

find_package(ZLIB REQUIRED)
find_package(OpenMP REQUIRED)

set(CTT_COMMON_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../common)

add_executable(ctt
  main.cpp tempdebug.cpp tevent.cpp ctt.cpp
  ${CTT_COMMON_DIR}/file_io.cpp ${CTT_COMMON_DIR}/func_id.cpp ${CTT_COMMON_DIR}/utils.cpp
  ${CTT_COMMON_DIR}/common_types.cpp ${CTT_COMMON_DIR}/ptp.cpp ${CTT_COMMON_DIR}/communicators.cpp
  ${CTT_COMMON_DIR}/model_id.cpp)

target_include_directories(ctt PRIVATE . ${CTT_COMMON_DIR})
target_compile_features(ctt PRIVATE cxx_std_20)
target_compile_options(ctt PRIVATE -O3 -g -W -Wall -Wnon-virtual-dtor -Wold-style-cast -Wctor-dtor-privacy)
target_link_libraries(ctt PRIVATE ZLIB::ZLIB OpenMP::OpenMP_CXX)

install(TARGETS ctt RUNTIME DESTINATION ${CMAKE_CURRENT_SOURCE_DIR}/../../../bin)
