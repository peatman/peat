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
# CMake build for pet (mirrors src/Makefile) - a single serial+OpenMP
# executable, no MPI/TARGET variants. Point cmake at a specific toolchain the
# usual way, e.g. cmake -DCMAKE_CXX_COMPILER=icpx ..
#
cmake_minimum_required(VERSION 3.16...4.4)
project(pet CXX)

find_package(ZLIB REQUIRED)
find_package(OpenMP REQUIRED)

set(PET_COMMON_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../common)
set(PET_EVENTS_DIR ${CMAKE_CURRENT_SOURCE_DIR}/pet_events)

add_executable(pet
  main.cpp collstorage.cpp pet_bind.cpp pet_core.cpp pet_node.cpp pet_rank.cpp pet_thread.cpp
  requests.cpp stat.cpp te_timeline.cpp tempdebug.cpp timeline.cpp events.cpp
  ${PET_EVENTS_DIR}/base.cpp ${PET_EVENTS_DIR}/coll.cpp ${PET_EVENTS_DIR}/collective.cpp
  ${PET_EVENTS_DIR}/comm.cpp ${PET_EVENTS_DIR}/completion.cpp ${PET_EVENTS_DIR}/local.cpp
  ${PET_EVENTS_DIR}/localbuf.cpp ${PET_EVENTS_DIR}/osc.cpp ${PET_EVENTS_DIR}/start.cpp ${PET_EVENTS_DIR}/tar.cpp
  ${PET_EVENTS_DIR}/tor.cpp ${PET_EVENTS_DIR}/gpu.cpp ${PET_EVENTS_DIR}/ompt.cpp
  ${PET_COMMON_DIR}/file_io.cpp ${PET_COMMON_DIR}/func_id.cpp ${PET_COMMON_DIR}/utils.cpp
  ${PET_COMMON_DIR}/common_types.cpp ${PET_COMMON_DIR}/communicators.cpp ${PET_COMMON_DIR}/transport.cpp
  ${PET_COMMON_DIR}/model_id.cpp ${PET_COMMON_DIR}/ptp.cpp ${PET_COMMON_DIR}/func_split.cpp)

target_include_directories(pet PRIVATE . ${PET_COMMON_DIR})
target_compile_features(pet PRIVATE cxx_std_20)
target_compile_options(pet PRIVATE -O2 -g)
target_compile_definitions(pet PRIVATE REMOVE_T_OVERHEAD _PET_USE_BIND_STORE_)
target_link_libraries(pet PRIVATE ZLIB::ZLIB OpenMP::OpenMP_CXX)

install(TARGETS pet RUNTIME DESTINATION ${CMAKE_CURRENT_SOURCE_DIR}/../../../bin)
