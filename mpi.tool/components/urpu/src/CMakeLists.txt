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
# CMake build for urpu (mirrors src/Makefile). URPU_TARGET selects the build
# variant, same as the Makefile's TARGET: serial|impi|mvapich2|ompi (plus
# mpich, for a plain/non-vendor MPICH install the Makefile has no dedicated
# case for). If URPU_TARGET isn't given explicitly, it's auto-detected the
# same way as tractor's CMakeLists.txt: probe PATH/env for an MPI compiler
# wrapper (impi > cray > mvapich2 > ompi > plain mpich) and fall back to a
# serial build if none is found at all. Unless CMAKE_CXX_COMPILER is already
# set, it's picked to match URPU_TARGET before project() - pass
# -DCMAKE_CXX_COMPILER=... to override with a specific wrapper/toolchain.
#
cmake_minimum_required(VERSION 3.16...4.4)

set(URPU_TARGET "" CACHE STRING "build variant: serial|impi|mvapich2|ompi|mpich (empty = auto-detect)")
set_property(CACHE URPU_TARGET PROPERTY STRINGS serial impi mvapich2 ompi mpich)

find_program(URPU_MPIICPX_EXE NAMES mpiicpx mpiicpc)
find_program(URPU_MPINAME_EXE NAMES mpiname)
find_program(URPU_OMPIINFO_EXE NAMES ompi_info)
find_program(URPU_MPICXX_EXE NAMES mpicxx)
find_program(URPU_CRAY_CC_EXE NAMES CC)

if(NOT URPU_TARGET)
  if(DEFINED ENV{I_MPI_ROOT} OR URPU_MPIICPX_EXE)
    set(_urpu_target impi)
  elseif(DEFINED ENV{CRAY_MPICH_DIR} OR DEFINED ENV{CRAYPE_VERSION})
    set(_urpu_target mpich) # Cray MPICH - PEAT_PARALLEL code doesn't distinguish vendors, only cares that MPI is present
  elseif(URPU_MPINAME_EXE)
    set(_urpu_target mvapich2)
  elseif(URPU_OMPIINFO_EXE)
    set(_urpu_target ompi)
  elseif(URPU_MPICXX_EXE)
    set(_urpu_target mpich)
  else()
    set(_urpu_target serial) # no MPI compiler wrapper found on PATH at all
  endif()
  set(URPU_TARGET ${_urpu_target} CACHE STRING "build variant: serial|impi|mvapich2|ompi|mpich (empty = auto-detect)" FORCE)
  message(STATUS "urpu: no URPU_TARGET given, auto-detected URPU_TARGET=${URPU_TARGET}")
endif()

if(NOT CMAKE_CXX_COMPILER)
  if(URPU_TARGET STREQUAL "impi" AND URPU_MPIICPX_EXE)
    set(CMAKE_CXX_COMPILER ${URPU_MPIICPX_EXE})
  elseif(URPU_TARGET STREQUAL "impi")
    set(CMAKE_CXX_COMPILER mpiicpx)
  elseif(URPU_TARGET STREQUAL "mpich" AND DEFINED ENV{CRAY_MPICH_DIR} AND URPU_CRAY_CC_EXE)
    set(CMAKE_CXX_COMPILER ${URPU_CRAY_CC_EXE})
  elseif(URPU_TARGET MATCHES "^(mvapich2|ompi|mpich)$" AND URPU_MPICXX_EXE)
    set(CMAKE_CXX_COMPILER ${URPU_MPICXX_EXE})
  elseif(URPU_TARGET MATCHES "^(mvapich2|ompi|mpich)$")
    set(CMAKE_CXX_COMPILER mpicxx)
  else()
    set(CMAKE_CXX_COMPILER g++)
  endif()
endif()

project(urpu CXX)

find_package(ZLIB REQUIRED)

set(URPU_COMMON_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../common)

if(URPU_TARGET STREQUAL "serial")
  set(URPU_EXE urpu)
else()
  set(URPU_EXE purpu.${URPU_TARGET})
endif()

add_executable(${URPU_EXE}
  main.cpp tempdebug.cpp tevent.cpp urpu.cpp
  ${URPU_COMMON_DIR}/file_io.cpp ${URPU_COMMON_DIR}/func_id.cpp ${URPU_COMMON_DIR}/utils.cpp
  ${URPU_COMMON_DIR}/common_types.cpp ${URPU_COMMON_DIR}/ptp.cpp ${URPU_COMMON_DIR}/model_id.cpp)

target_include_directories(${URPU_EXE} PRIVATE . ${URPU_COMMON_DIR})
target_compile_features(${URPU_EXE} PRIVATE cxx_std_20)
target_compile_options(${URPU_EXE} PRIVATE -O3 -W -Wall -Wnon-virtual-dtor -Wold-style-cast -Wctor-dtor-privacy)
target_link_libraries(${URPU_EXE} PRIVATE ZLIB::ZLIB)

if(URPU_TARGET MATCHES "^(impi|mvapich2|ompi|mpich)$")
  target_compile_definitions(${URPU_EXE} PRIVATE PEAT_PARALLEL)
endif()

install(TARGETS ${URPU_EXE} RUNTIME DESTINATION ${CMAKE_CURRENT_SOURCE_DIR}/../../../bin)
