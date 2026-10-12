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
# CMake build for mat (mirrors src/Makefile). MAT_TARGET selects the build
# variant, same as the Makefile's TARGET: serial|openmp|impi|mvapich2|ompi
# (plus mpich, for a plain/non-vendor MPICH install the Makefile has no
# dedicated case for). If MAT_TARGET isn't given explicitly, it's
# auto-detected the same way as tractor's CMakeLists.txt: probe PATH/env for
# an MPI compiler wrapper (impi > cray > mvapich2 > ompi > plain mpich) and
# fall back to a serial build if none is found at all. Unless
# CMAKE_CXX_COMPILER is already set, it's picked to match MAT_TARGET before
# project() - pass -DCMAKE_CXX_COMPILER=... to override with a specific
# wrapper/toolchain.
#
cmake_minimum_required(VERSION 3.16...4.4)

set(MAT_TARGET "" CACHE STRING "build variant: serial|openmp|impi|mvapich2|ompi|mpich (empty = auto-detect)")
set_property(CACHE MAT_TARGET PROPERTY STRINGS serial openmp impi mvapich2 ompi mpich)

find_program(MAT_MPIICPX_EXE NAMES mpiicpx mpiicpc)
find_program(MAT_MPINAME_EXE NAMES mpiname)
find_program(MAT_OMPIINFO_EXE NAMES ompi_info)
find_program(MAT_MPICXX_EXE NAMES mpicxx)
find_program(MAT_CRAY_CC_EXE NAMES CC)

if(NOT MAT_TARGET)
  if(DEFINED ENV{I_MPI_ROOT} OR MAT_MPIICPX_EXE)
    set(_mat_target impi)
  elseif(DEFINED ENV{CRAY_MPICH_DIR} OR DEFINED ENV{CRAYPE_VERSION})
    set(_mat_target mpich) # Cray MPICH - PEAT_MPI code doesn't distinguish vendors, only cares that MPI is present
  elseif(MAT_MPINAME_EXE)
    set(_mat_target mvapich2)
  elseif(MAT_OMPIINFO_EXE)
    set(_mat_target ompi)
  elseif(MAT_MPICXX_EXE)
    set(_mat_target mpich)
  else()
    set(_mat_target serial) # no MPI compiler wrapper found on PATH at all
  endif()
  set(MAT_TARGET ${_mat_target} CACHE STRING "build variant: serial|openmp|impi|mvapich2|ompi|mpich (empty = auto-detect)" FORCE)
  message(STATUS "mat: no MAT_TARGET given, auto-detected MAT_TARGET=${MAT_TARGET}")
endif()

if(NOT CMAKE_CXX_COMPILER)
  if(MAT_TARGET STREQUAL "impi" AND MAT_MPIICPX_EXE)
    set(CMAKE_CXX_COMPILER ${MAT_MPIICPX_EXE})
  elseif(MAT_TARGET STREQUAL "impi")
    set(CMAKE_CXX_COMPILER mpiicpx)
  elseif(MAT_TARGET STREQUAL "mpich" AND DEFINED ENV{CRAY_MPICH_DIR} AND MAT_CRAY_CC_EXE)
    set(CMAKE_CXX_COMPILER ${MAT_CRAY_CC_EXE})
  elseif(MAT_TARGET MATCHES "^(mvapich2|ompi|mpich)$" AND MAT_MPICXX_EXE)
    set(CMAKE_CXX_COMPILER ${MAT_MPICXX_EXE})
  elseif(MAT_TARGET MATCHES "^(mvapich2|ompi|mpich)$")
    set(CMAKE_CXX_COMPILER mpicxx)
  else()
    set(CMAKE_CXX_COMPILER g++)
  endif()
endif()

project(mat CXX)

add_compile_definitions(MAT_PROCESS_HWC_MPI_OUTPUT)

find_package(ZLIB REQUIRED)

set(MAT_COMMON_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../common)

if(MAT_TARGET STREQUAL "openmp")
  set(MAT_EXE mat.omp)
elseif(MAT_TARGET STREQUAL "serial")
  set(MAT_EXE mat)
else()
  set(MAT_EXE pmat.${MAT_TARGET})
endif()

add_executable(${MAT_EXE}
  main.cpp collectives.cpp mat.cpp p2pal.cpp requests.cpp osc.cpp tempdebug.cpp tevent.cpp gpu.cpp comm.cpp request_store.cpp
  ${MAT_COMMON_DIR}/file_io.cpp ${MAT_COMMON_DIR}/func_id.cpp ${MAT_COMMON_DIR}/utils.cpp
  ${MAT_COMMON_DIR}/common_types.cpp ${MAT_COMMON_DIR}/ptp.cpp ${MAT_COMMON_DIR}/communicators.cpp
  ${MAT_COMMON_DIR}/model_id.cpp)

target_include_directories(${MAT_EXE} PRIVATE . ${MAT_COMMON_DIR})
target_compile_features(${MAT_EXE} PRIVATE cxx_std_20)
target_compile_options(${MAT_EXE} PRIVATE -O3 -W -Wall -Wnon-virtual-dtor -Wold-style-cast -Wctor-dtor-privacy)
target_link_libraries(${MAT_EXE} PRIVATE ZLIB::ZLIB)

if(MAT_TARGET STREQUAL "openmp")
  find_package(OpenMP REQUIRED)
  target_link_libraries(${MAT_EXE} PRIVATE OpenMP::OpenMP_CXX)
endif()

if(MAT_TARGET MATCHES "^(impi|mvapich2|ompi|mpich)$")
  target_compile_definitions(${MAT_EXE} PRIVATE PEAT_MPI)
endif()

install(TARGETS ${MAT_EXE} RUNTIME DESTINATION ${CMAKE_CURRENT_SOURCE_DIR}/../../../bin)
