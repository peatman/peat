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
#
# CMake build for tractor (mirrors src/Makefile), including the
# TARGET=fab_layers standalone shim build and the legacy single-fabric
# FABRIC=PSM2/VERBS/PSM3/UCX/OFI modes. BUILD_TRACTOR_ENGINEERING_VERSION=auto
# detects a patched MPI by trying to link against mpi_internal_comm_translate_rank
# (an MPICH_API_PUBLIC symbol every ../../patches/* patch adds) - see its
# definition below for the reasoning and caveats.
#
# CMAKE_CXX_COMPILER/CMAKE_C_COMPILER are expected to already be the MPI
# wrappers (mpicxx/mpicc, mpiicpc/mpiicc, Cray CC/cc, ...) - same convention
# as MPICXX/MPICC in env.sh. Point cmake at them the usual way, e.g.:
#   cmake -DCMAKE_CXX_COMPILER=mpicxx -DCMAKE_C_COMPILER=mpicc ..
#
cmake_minimum_required(VERSION 3.16...4.4)
project(tractor CXX C)

set(TRACTOR_MAJOR 1)
set(TRACTOR_MINOR 0)
string(TIMESTAMP TRACTOR_BUILD_DATE "%Y-%m-%d %H:%M:%S")

# ---------------------------------------------------------------------------
# 0. Perl - required at build time to run the code generators (mpi_layer.pl,
#    ../../common/mpi_func_ids.pl); without it mpi_layer.cpp / the func-id
#    headers can't be produced at all.
# ---------------------------------------------------------------------------
find_package(Perl REQUIRED)
find_package(MPI REQUIRED)
message(STATUS "tractor: using Perl ${PERL_VERSION_STRING} (${PERL_EXECUTABLE})")

# ---------------------------------------------------------------------------
# 1. Compiler family: gnu / intel / pgi
#    (override with -DTRACTOR_COMPILER=gnu|intel|pgi to skip auto-detection)
# ---------------------------------------------------------------------------
if(NOT TRACTOR_COMPILER)
  if(CMAKE_CXX_COMPILER_ID STREQUAL "Intel" OR CMAKE_CXX_COMPILER_ID STREQUAL "IntelLLVM")
    set(_tractor_compiler intel)
  elseif(CMAKE_CXX_COMPILER_ID STREQUAL "PGI" OR CMAKE_CXX_COMPILER_ID STREQUAL "NVHPC")
    set(_tractor_compiler pgi)
  elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    set(_tractor_compiler gnu)
  else()
    message(WARNING "tractor: unrecognized CXX compiler id '${CMAKE_CXX_COMPILER_ID}', defaulting TRACTOR_COMPILER=gnu")
    set(_tractor_compiler gnu)
  endif()
  set(TRACTOR_COMPILER ${_tractor_compiler} CACHE STRING "toolchain family: gnu|intel|pgi" FORCE)
endif()
set_property(CACHE TRACTOR_COMPILER PROPERTY STRINGS gnu intel pgi)
message(STATUS "tractor: COMPILER=${TRACTOR_COMPILER} (CXX compiler id: ${CMAKE_CXX_COMPILER_ID})")

# ---------------------------------------------------------------------------
# 2. MPI vendor + version -> Makefile-style TARGET
#    -DTRACTOR_TARGET=auto (default) probes CMAKE_CXX_COMPILER to detect the
#    vendor/version; set it to impi|mpich4|mpich5|mvapich2|mvapich4|openmpi|
#    cray|fab_layers to skip probing and use that flavor directly ('cray' is
#    just an alias that normalizes to mpich4 - same MPICH-family struct layout).
# ---------------------------------------------------------------------------
set(TRACTOR_TARGET "auto" CACHE STRING
  "MPI flavor: auto|impi|mpich4|mpich5|mvapich2|mvapich4|openmpi|cray|fab_layers. \
'auto' detects the vendor/version from CMAKE_CXX_COMPILER; any other value is used \
directly and skips probing.")
set_property(CACHE TRACTOR_TARGET PROPERTY STRINGS auto impi mpich4 mpich5 mvapich2 mvapich4 openmpi cray fab_layers)
string(TOLOWER "${TRACTOR_TARGET}" _tractor_target_lower)

if(_tractor_target_lower STREQUAL "cray")
  set(_tractor_target_lower mpich4) # Cray MPICH builds like stock MPICH 4.x
  set(TRACTOR_TARGET mpich4 CACHE STRING "MPI flavor: auto|impi|mpich4|mpich5|mvapich2|mvapich4|openmpi|cray|fab_layers" FORCE)
endif()

if(NOT _tractor_target_lower MATCHES "^(auto|impi|mpich4|mpich5|mvapich2|mvapich4|openmpi|fab_layers)$")
  message(FATAL_ERROR "tractor: invalid TRACTOR_TARGET='${TRACTOR_TARGET}' (expected auto|impi|mpich4|mpich5|mvapich2|mvapich4|openmpi|cray|fab_layers)")
endif()

if(NOT _tractor_target_lower STREQUAL "auto" AND NOT _tractor_target_lower STREQUAL "fab_layers")
  # derive an informational vendor name for the status message below - no probing needed
  if(_tractor_target_lower MATCHES "^mpich")
    set(TRACTOR_MPI_VENDOR mpich CACHE STRING "MPI vendor (from explicit TRACTOR_TARGET)" FORCE)
  elseif(_tractor_target_lower MATCHES "^mvapich")
    set(TRACTOR_MPI_VENDOR mvapich CACHE STRING "MPI vendor (from explicit TRACTOR_TARGET)" FORCE)
  else()
    set(TRACTOR_MPI_VENDOR ${_tractor_target_lower} CACHE STRING "MPI vendor (from explicit TRACTOR_TARGET)" FORCE)
  endif()
  set(TRACTOR_MPI_VERSION "" CACHE STRING "MPI version (unknown - TRACTOR_TARGET set explicitly)" FORCE)
endif()

if(_tractor_target_lower STREQUAL "auto")
  set(_mpi_vendor "")
  set(_mpi_version "")

  execute_process(COMMAND ${CMAKE_CXX_COMPILER} -v
                   OUTPUT_VARIABLE _mpi_v_out ERROR_VARIABLE _mpi_v_err
                   OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_STRIP_TRAILING_WHITESPACE
                   RESULT_VARIABLE _mpi_v_rc)
  # newer Intel oneAPI mpiicpx/mpiicx wrappers only forward -v to the underlying
  # icpx/icx (no MPI banner); -V is what unconditionally prints "Intel(R) MPI
  # Library" regardless of environment, so probe both and merge
  execute_process(COMMAND ${CMAKE_CXX_COMPILER} -V
                   OUTPUT_VARIABLE _mpi_V_out ERROR_VARIABLE _mpi_V_err
                   OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_STRIP_TRAILING_WHITESPACE
                   RESULT_VARIABLE _mpi_V_rc)
  set(_mpi_v "${_mpi_v_out} ${_mpi_v_err} ${_mpi_V_out} ${_mpi_V_err}")

  if(_mpi_v MATCHES "Intel\\(R\\) MPI")
    set(_mpi_vendor impi)
  elseif(_mpi_v MATCHES "[Cc]ray")
    set(_mpi_vendor cray)
  else()
    find_program(TRACTOR_MPINAME_EXE mpiname)
    find_program(TRACTOR_OMPIINFO_EXE ompi_info)
    find_program(TRACTOR_MPICHVERSION_EXE mpichversion)
    if(TRACTOR_MPINAME_EXE)
      set(_mpi_vendor mvapich)
    elseif(TRACTOR_OMPIINFO_EXE)
      set(_mpi_vendor openmpi)
    elseif(TRACTOR_MPICHVERSION_EXE)
      set(_mpi_vendor mpich)
    elseif(_mpi_v MATCHES "MVAPICH")
      set(_mpi_vendor mvapich)
    elseif(_mpi_v MATCHES "[Oo]pen ?MPI")
      set(_mpi_vendor openmpi)
    elseif(_mpi_v MATCHES "MPICH")
      set(_mpi_vendor mpich)
    else()
      message(WARNING "tractor: could not auto-detect MPI vendor from '${CMAKE_CXX_COMPILER}', defaulting to mpich")
      set(_mpi_vendor mpich)
    endif()
  endif()

  # version string, per vendor (best-effort; used only to pick mpich4 vs
  # mpich5 / mvapich2 vs mvapich4 - whether it's a *patched* build is future work)
  if(_mpi_vendor STREQUAL "impi" AND DEFINED ENV{I_MPI_ROOT})
    get_filename_component(_mpi_version "$ENV{I_MPI_ROOT}" NAME)
  elseif(_mpi_vendor STREQUAL "mvapich" AND TRACTOR_MPINAME_EXE)
    execute_process(COMMAND ${TRACTOR_MPINAME_EXE} -a
                     OUTPUT_VARIABLE _mpiname_out OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
    if(_mpiname_out MATCHES "MVAPICH2?[ \t-]*([0-9]+\\.[0-9.]+)")
      set(_mpi_version "${CMAKE_MATCH_1}")
    endif()
  elseif(_mpi_vendor STREQUAL "openmpi" AND TRACTOR_OMPIINFO_EXE)
    execute_process(COMMAND ${TRACTOR_OMPIINFO_EXE} --version
                     OUTPUT_VARIABLE _ompi_out OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
    if(_ompi_out MATCHES "v?([0-9]+\\.[0-9]+\\.[0-9]+)")
      set(_mpi_version "${CMAKE_MATCH_1}")
    endif()
  elseif(_mpi_vendor MATCHES "^(mpich|cray)$")
    if(TRACTOR_MPICHVERSION_EXE)
      execute_process(COMMAND ${TRACTOR_MPICHVERSION_EXE}
                       OUTPUT_VARIABLE _mpich_out OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
      if(_mpich_out MATCHES "MPICH [Vv]ersion:[ \t]*([0-9]+\\.[0-9.]+)")
        set(_mpi_version "${CMAKE_MATCH_1}")
      endif()
    endif()
    if(NOT _mpi_version AND _mpi_v MATCHES "MPICH [Vv]ersion[: ]+([0-9]+\\.[0-9.]+)")
      set(_mpi_version "${CMAKE_MATCH_1}")
    endif()
  endif()

  if(NOT _mpi_version)
    message(WARNING "tractor: could not determine MPI version for vendor '${_mpi_vendor}' - assuming latest supported (4.x/2.x)")
  endif()

  # map (vendor, version) -> Makefile TARGET
  if(_mpi_vendor STREQUAL "impi")
    set(_tractor_target impi)
  elseif(_mpi_vendor STREQUAL "openmpi")
    set(_tractor_target openmpi)
  elseif(_mpi_vendor MATCHES "^(mpich|cray)$")
    if(_mpi_version MATCHES "^5\\.")
      set(_tractor_target mpich5)
    else()
      set(_tractor_target mpich4) # covers MPICH 4.x and unversioned/Cray MPICH
    endif()
  elseif(_mpi_vendor STREQUAL "mvapich")
    if(_mpi_version MATCHES "^2\\.")
      set(_tractor_target mvapich2)
    else()
      set(_tractor_target mvapich4) # covers MVAPICH 4.x and unversioned
    endif()
  else()
    set(_tractor_target mpich4)
  endif()

  set(TRACTOR_MPI_VENDOR  ${_mpi_vendor}  CACHE STRING "detected MPI vendor (mpich|mvapich|openmpi|impi|cray)" FORCE)
  set(TRACTOR_MPI_VERSION "${_mpi_version}" CACHE STRING "detected MPI version" FORCE)
  set(TRACTOR_TARGET ${_tractor_target} CACHE STRING "MPI flavor: auto|impi|mpich4|mpich5|mvapich2|mvapich4|openmpi|cray|fab_layers" FORCE)
endif()
message(STATUS "tractor: MPI vendor=${TRACTOR_MPI_VENDOR} version=${TRACTOR_MPI_VERSION} -> TARGET=${TRACTOR_TARGET}")

# ---------------------------------------------------------------------------
# 3. Remaining switches - passed as -D<NAME>=... on the cmake command line
# ---------------------------------------------------------------------------
set(MEM  "off" CACHE STRING "memory profiling tag: off|<value used as _MEMPROFILE_<MEM>_>, e.g. HOOKS")
set(DIAG OFF   CACHE BOOL   "debug/diagnostic build (-O0, _dbg suffix, entry/exit tracing)")
set(GPU  OFF   CACHE BOOL   "OpenCL/Level Zero call interception (PARSE_GPU)")
set(BUF  OFF   CACHE BOOL   "extra MPI buffer fields (MPIBUFFERS)")
set(V2P  OFF   CACHE BOOL   "virtual->physical address resolution (VIRT2PHYS)")
set(HWC  "off" CACHE STRING "hardware counters tag: off|RDPMC")

set(FABRIC "off" CACHE STRING
  "fabric-level interception: off|on|PSM2|VERBS|PSM3|UCX|OFI. 'on' and the legacy PSM2/VERBS/PSM3/UCX/OFI \
values embed one fabric's interception into the core lib; for the standalone psm2+psm3+ucx+ofi shim \
.so build use -DTRACTOR_TARGET=fab_layers instead")
set_property(CACHE FABRIC PROPERTY STRINGS off on PSM2 VERBS PSM3 UCX OFI)

set(OMPT       ON  CACHE BOOL "OpenMP interception")
set(PTPD_BUILD ON  CACHE BOOL "build the ptpd PTP-daemon binary")

set(PEAT_TIMER "tsc" CACHE STRING
  "timer backend for entry/exit timestamps: tsc|nano. 'tsc' uses raw/calibrated TSC reads \
(rdtsc family); 'nano' uses clock_gettime (via peat_get_time()) for a node-wide, core-invariant \
timer, at the cost of slightly higher per-call overhead (USE_NANO_TIMER).")
set_property(CACHE PEAT_TIMER PROPERTY STRINGS tsc nano)
string(TOLOWER "${PEAT_TIMER}" _timer_lower)
if(NOT _timer_lower MATCHES "^(tsc|nano)$")
  message(FATAL_ERROR "tractor: invalid PEAT_TIMER='${PEAT_TIMER}' (expected tsc|nano)")
endif()

set(BUILD_TRACTOR_ENGINEERING_VERSION "auto" CACHE STRING
  "collective-timeline (\"colt\"/PARSE_COLT) engineering build, needs a patched MPI: auto|off|on.")
set_property(CACHE BUILD_TRACTOR_ENGINEERING_VERSION PROPERTY STRINGS auto off on)
string(TOLOWER "${BUILD_TRACTOR_ENGINEERING_VERSION}" _eng_lower)
if(_eng_lower STREQUAL "auto")
  if(TRACTOR_TARGET STREQUAL "fab_layers")
    message(STATUS "tractor: BUILD_TRACTOR_ENGINEERING_VERSION=auto -> 'off' (not applicable to TARGET=fab_layers)")
    set(_eng_lower off)
  else()
    # every patch under ../../patches (mpich-3.4.3, mpich-4.3.2, mpich-5.0.1, mpich-5.0.2, mvapich-2.3.5,
    # mvapich-4.1) adds the same MPICH_API_PUBLIC (exported) symbol
    # mpi_internal_comm_translate_rank(); a stock/unpatched MPI never defines it, so
    # trying to link a program that references it tells us the install is patched
    # - this assumes CMAKE_C_COMPILER is the MPI wrapper (mpicc), same as elsewhere
    include(CheckFunctionExists)
    set(CMAKE_REQUIRED_QUIET ON)
    check_function_exists(mpi_internal_comm_translate_rank TRACTOR_MPI_PATCHED)
    unset(CMAKE_REQUIRED_QUIET)
    if(TRACTOR_MPI_PATCHED)
      message(STATUS "tractor: BUILD_TRACTOR_ENGINEERING_VERSION=auto -> 'on' (mpi_internal_comm_translate_rank found: patched/engineering MPI)")
      set(_eng_lower on)
    else()
      message(STATUS "tractor: BUILD_TRACTOR_ENGINEERING_VERSION=auto -> 'off' (mpi_internal_comm_translate_rank not found: stock MPI)")
      set(_eng_lower off)
    endif()
  endif()
endif()

string(TOLOWER "${MEM}" _mem_lower)
string(TOLOWER "${HWC}" _hwc_lower)
string(TOUPPER "${FABRIC}" _fabric_upper)

# ---------------------------------------------------------------------------
# Third-party dependencies
# ---------------------------------------------------------------------------
find_package(ZLIB REQUIRED)

# only used when FABRIC != off or TRACTOR_TARGET=fab_layers - defaults match Build.sh's
if(NOT VERBSPATH)
  if(DEFINED ENV{VERBSPATH})
    set(VERBSPATH "$ENV{VERBSPATH}")
  else()
    set(VERBSPATH "/usr")
  endif()
endif()
if(NOT PSM2PATH)
  if(DEFINED ENV{PSM2PATH})
    set(PSM2PATH "$ENV{PSM2PATH}")
  else()
    set(PSM2PATH "$ENV{HOME}/psm2")
  endif()
endif()
if(NOT PSM3PATH)
  if(DEFINED ENV{PSM3PATH})
    set(PSM3PATH "$ENV{PSM3PATH}")
  else()
    set(PSM3PATH "$ENV{HOME}/psm3")
  endif()
endif()
if(NOT UCXPATH)
  if(DEFINED ENV{UCXPATH})
    set(UCXPATH "$ENV{UCXPATH}")
  else()
    set(UCXPATH "$ENV{HOME}/ucx")
  endif()
endif()
if(NOT OFIPATH)
  if(DEFINED ENV{OFIPATH})
    set(OFIPATH "$ENV{OFIPATH}")
  elseif(DEFINED ENV{OFI_PATH})
    set(OFIPATH "$ENV{OFI_PATH}")
  else()
    set(OFIPATH "$ENV{HOME}/libfabric")
  endif()
endif()

find_program(TRACTOR_CPP_EXE NAMES cpp REQUIRED)

# ---------------------------------------------------------------------------
# CONTROLS (preprocessor defines) and OBJECTS (sources), built up exactly like
# the Makefile's ifeq blocks
# ---------------------------------------------------------------------------
set(TRACTOR_CONTROLS
  _TRACTOR_COLLECT_STAT_ _T_PER_INTERVAL_STAT_WRITE_ _USE_COMM_DISAMBIGUATION_ _TRACTOR_PCALL_
  _TRACTOR_MAJOR_VERSION_=${TRACTOR_MAJOR} _TRACTOR_MINOR_VERSION_=${TRACTOR_MINOR}
  _TRACTOR_BUILD_DATE_="${TRACTOR_BUILD_DATE}"
  _USE_SIGNALS_
  _MPI_MESSAGE_DEFINED_ _USE_PROTORYPES_WITH_CONST_ _NO_C_VR
)
set(TRACTOR_SOURCES
  t_base_event.cpp tractor.cpp t_stat.cpp t_comm.cpp elf_parser.cpp t_topo.cpp t_globals.cpp t_signals.cpp
  ${CMAKE_CURRENT_BINARY_DIR}/mpi_layer.cpp
)
set(TRACTOR_LIBS ZLIB::ZLIB)
set(TRACTOR_EXTRANAME "")
set(_print_extended_stuff off)

if(GPU)
  list(APPEND TRACTOR_CONTROLS GPUPARSE)
  list(APPEND TRACTOR_LIBS OpenCL ze_loader)
  set(TRACTOR_EXTRANAME "${TRACTOR_EXTRANAME}.gpu")
endif()

# fab_func_layers: extra "-l <layer>[=outfile]" args forwarded to mpi_layer.pl.
# TARGET=mvapich2 always forwards verbs/smpi/psm2 into the core mpi_layer.cpp
# (ibv_*/smpi calls are static-inline, so the patched mvapich2 source calls
# mv2_proxy_* directly instead of dlsym-interception); TARGET=fab_layers
# overrides this entirely with its own 4 standalone-shim outfiles.
set(_fab_func_layers)
set(_mpi_layer_skip_core off)
set(_fab_shim_outputs)
if(TRACTOR_TARGET STREQUAL "fab_layers")
  set(_mpi_layer_skip_core on)
  set(_fab_func_layers
    -l psm2=mpi_layer_fab_psm2.cpp -l psm3=mpi_layer_fab_psm3.cpp
    -l ucx=mpi_layer_fab_ucx.cpp   -l ofi=mpi_layer_fab_ofi.cpp)
  set(_fab_shim_outputs
    ${CMAKE_CURRENT_BINARY_DIR}/mpi_layer_fab_psm2.cpp ${CMAKE_CURRENT_BINARY_DIR}/mpi_layer_fab_psm3.cpp
    ${CMAKE_CURRENT_BINARY_DIR}/mpi_layer_fab_ucx.cpp  ${CMAKE_CURRENT_BINARY_DIR}/mpi_layer_fab_ofi.cpp)
else()
  if(TRACTOR_TARGET STREQUAL "mvapich2")
    list(APPEND _fab_func_layers -l verbs -l smpi -l psm2)
  endif()

  # FABRIC: 'on' is generic multi-layer core support; PSM2/VERBS/PSM3/UCX/OFI
  # are the legacy single-fabric-only core-embedded builds
  if(_fabric_upper STREQUAL "ON")
    list(APPEND TRACTOR_CONTROLS _FAB_PROFILE_)
    list(APPEND TRACTOR_SOURCES t_fab.cpp)
  elseif(_fabric_upper STREQUAL "PSM2")
    list(APPEND TRACTOR_CONTROLS _PSM2_PROFILE_)
    list(APPEND TRACTOR_SOURCES t_fab.cpp)
    list(APPEND _fab_func_layers -l psm2)
    set(TRACTOR_EXTRANAME "${TRACTOR_EXTRANAME}.psm2")
  elseif(_fabric_upper STREQUAL "VERBS")
    list(APPEND TRACTOR_CONTROLS _VERBS_PROFILE_)
    list(APPEND TRACTOR_SOURCES t_fab.cpp)
    list(APPEND _fab_func_layers -l verbs -l smpi)
    set(TRACTOR_EXTRANAME "${TRACTOR_EXTRANAME}.verbs")
  elseif(_fabric_upper STREQUAL "PSM3")
    list(APPEND TRACTOR_CONTROLS _PSM3_PROFILE_)
    list(APPEND TRACTOR_SOURCES t_fab.cpp)
    list(APPEND _fab_func_layers -l psm3)
    set(TRACTOR_EXTRANAME "${TRACTOR_EXTRANAME}.psm3")
  elseif(_fabric_upper STREQUAL "UCX")
    list(APPEND TRACTOR_CONTROLS _UCX_PROFILE_)
    list(APPEND TRACTOR_SOURCES t_fab.cpp)
    list(APPEND _fab_func_layers -l ucx)
    set(TRACTOR_EXTRANAME "${TRACTOR_EXTRANAME}.ucx")
  elseif(_fabric_upper STREQUAL "OFI")
    list(APPEND TRACTOR_CONTROLS _OFI_PROFILE_)
    list(APPEND TRACTOR_SOURCES t_fab.cpp)
    list(APPEND _fab_func_layers -l ofi)
    set(TRACTOR_EXTRANAME "${TRACTOR_EXTRANAME}.ofi")
  elseif(NOT _fabric_upper STREQUAL "OFF")
    message(FATAL_ERROR "tractor: invalid FABRIC='${FABRIC}' (expected off|on|PSM2|VERBS|PSM3|UCX|OFI)")
  endif()
endif()

# cpp_input / TARGET-specific CONTROLS
set(_cpp_input)
# highest MPI standard level whose wrappers mpi_layer.pl emits (-m); the generated code is
# additionally guarded by MPI_VERSION/MPI_SUBVERSION from the mpi.h in use
set(_maxmpi 3)
if(TRACTOR_TARGET STREQUAL "impi")
  list(APPEND _cpp_input -D_MPICH_=3 -D_IMPI_)
  list(APPEND TRACTOR_CONTROLS _MPICH_=3 _IMPI_ _I_MPI_WEAK_EXPORTS_ I_MPI_BUILD)
  set(_maxmpi 4)
elseif(TRACTOR_TARGET STREQUAL "mpich4")
  list(APPEND _cpp_input -D_MPICH_=4)
  list(APPEND TRACTOR_CONTROLS _MPICH_=4)
  set(_maxmpi 4)
elseif(TRACTOR_TARGET STREQUAL "mpich5")
  list(APPEND _cpp_input -D_MPICH_=5)
  list(APPEND TRACTOR_CONTROLS _MPICH_=5)
  set(_maxmpi 5)
elseif(TRACTOR_TARGET STREQUAL "mvapich2")
  list(APPEND _cpp_input -D_MPICH_=3 -D_MVAPICH_)
  list(APPEND TRACTOR_CONTROLS _MPICH_=3 _MVAPICH_ _VERBS_PROFILE_)
elseif(TRACTOR_TARGET STREQUAL "mvapich4")
  list(APPEND _cpp_input -D_MPICH_=4 -D_MVAPICH_)
  list(APPEND TRACTOR_CONTROLS _MPICH_=4 _MVAPICH_)
  set(_maxmpi 4)
endif()
# _STATUS_LO_HI_ picks the MPICH-family status struct layout; OpenMPI uses its
# own (_ucount) - applies to every other TARGET, including fab_layers
if(NOT TRACTOR_TARGET STREQUAL "openmpi")
  list(APPEND TRACTOR_CONTROLS _STATUS_LO_HI_)
endif()

if(_eng_lower STREQUAL "on")
  list(APPEND TRACTOR_CONTROLS _TRACTOR_PARSE_COLT_ _USE_ENG_MPI_)
  set(_pct on)
else()
  set(_pct off)
endif()

if(BUF)
  list(APPEND TRACTOR_CONTROLS _TRACTOR_EXTRA_FIELDS_)
  set(TRACTOR_EXTRANAME "${TRACTOR_EXTRANAME}.mbs")
  set(_print_extended_stuff on)
endif()

if(V2P)
  list(APPEND TRACTOR_CONTROLS _TRACTOR_VIRT2PHYS_ _TRACTOR_EXTRA_FIELDS_)
  list(APPEND TRACTOR_SOURCES virt2phys.cpp)
  set(TRACTOR_EXTRANAME "${TRACTOR_EXTRANAME}.v2p")
  set(_print_extended_stuff on)
endif()

if(NOT _mem_lower STREQUAL "off")
  string(TOUPPER "${MEM}" _mem_upper)
  list(APPEND TRACTOR_SOURCES memprofile.cpp)
  list(APPEND TRACTOR_CONTROLS _MEMPROFILE_ _MEMPROFILE_${_mem_upper}_ _TRACTOR_ZERO_ON_ALLOCATE_)
  set(TRACTOR_EXTRANAME "${TRACTOR_EXTRANAME}.mem${MEM}")
  if(_mem_upper STREQUAL "HOOKS")
    list(APPEND TRACTOR_CONTROLS _MEM_HOOKS_ _MEMPROFILE_STORE_)
    list(APPEND TRACTOR_SOURCES t_alloc.cpp)
  endif()
endif()

if(OMPT)
  list(APPEND TRACTOR_SOURCES t_ompt.cpp)
  list(APPEND TRACTOR_CONTROLS _T_OMPT_)
endif()

if(_timer_lower STREQUAL "nano")
  list(APPEND TRACTOR_CONTROLS USE_NANO_TIMER)
endif()

if(NOT _hwc_lower STREQUAL "off")
  string(TOUPPER "${HWC}" _hwc_upper)
  list(APPEND TRACTOR_CONTROLS _HWCOUNTERS_ENABLED_ _HWCOUNTERS_${_hwc_upper}_)
  list(APPEND TRACTOR_SOURCES hwcounter.cpp)
  set(TRACTOR_EXTRANAME "${TRACTOR_EXTRANAME}.${HWC}")
endif()

# ---------------------------------------------------------------------------
# Compiler-family specific bits
# ---------------------------------------------------------------------------
if(TRACTOR_COMPILER STREQUAL "intel")
  list(APPEND TRACTOR_CONTROLS _INTELCOMPILER_)
  set(TRACTOR_CXX_MISC -std=c++11 -fno-omit-frame-pointer -w)
  set(TRACTOR_OMP_FLAG -qopenmp)
elseif(TRACTOR_COMPILER STREQUAL "pgi")
  list(APPEND TRACTOR_CONTROLS _PGICOMPILER_)
  set(TRACTOR_CXX_MISC -Wno-write-strings)
  set(TRACTOR_OMP_FLAG -mp)
else()
  list(APPEND TRACTOR_CONTROLS _GNUCOMPILER_)
  set(TRACTOR_CXX_MISC -Wno-write-strings)
  set(TRACTOR_OMP_FLAG -fopenmp)
endif()

if(TRACTOR_TARGET STREQUAL "openmpi")
  list(APPEND TRACTOR_CXX_MISC -fms-extensions)
endif()

if(NOT OMPT)
  set(TRACTOR_OMP_FLAG "")
endif()

# ---------------------------------------------------------------------------
# DIAG / optimization level
# ---------------------------------------------------------------------------
if(DIAG)
  set(TRACTOR_DBG_SUFFIX "_dbg")
  set(TRACTOR_OPT -O0)
  list(APPEND TRACTOR_CONTROLS
    _TRACTOR_DIAG_ _TRACTOR_DEBUG_TRACTOR_LAYER_ENTRY_EXIT_ _TRACTOR_DEBUG_FAB_LAYER_ENTRY_EXIT_
    _TRACTOR_DEBUG_MPI_LAYER_ENTRY_EXIT_ _TRACTOR_DEBUG_GPU_LAYER_ENTRY_EXIT_
    _TRACTOR_DEBUG_OMPT_ENTRY_EXIT_ _TRACTOR_DEBUG_COMM_ENTRY_EXIT_)
else()
  set(TRACTOR_DBG_SUFFIX "")
  set(TRACTOR_OPT -O3)
endif()

set(TRACTOR_LIB_SUFFIX "${TRACTOR_DBG_SUFFIX}.${TRACTOR_TARGET}${TRACTOR_EXTRANAME}")
set(TRACTOR_BIN_SUFFIX "${TRACTOR_DBG_SUFFIX}.${TRACTOR_TARGET}")
# fab shims are TARGET-independent (one build works for every MPI, see Makefile) - no TARGET in the suffix
set(FABLIBSUFFIX "${TRACTOR_DBG_SUFFIX}${TRACTOR_EXTRANAME}")
list(APPEND TRACTOR_CONTROLS
  _TRACTOR_LIB_SUFFIX_="${TRACTOR_LIB_SUFFIX}" _TRACTOR_BIN_SUFFIX_="${TRACTOR_BIN_SUFFIX}"
  _TRACTOR_ZERO_ON_ALLOCATE_)
set(_tractor_defs ${TRACTOR_CONTROLS})
list(TRANSFORM _tractor_defs PREPEND "-D")

# ---------------------------------------------------------------------------
# Code generation (run in-place in src/, like the Makefile: the perl scripts
# use cwd-relative paths - "cat pinc/...", "cp mpi_layer.cpp.in mpi_layer.cpp" -
# so they must execute with CMAKE_CURRENT_BINARY_DIR as their working directory)
# ---------------------------------------------------------------------------
set(_common_dir ${CMAKE_CURRENT_SOURCE_DIR}/../../common)
if(GPU)
  set(_parse_gpu_flag on)
else()
  set(_parse_gpu_flag off)
endif()

add_custom_command(
  OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/pinc
  COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/pinc ${CMAKE_CURRENT_BINARY_DIR}/pinc
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMENT "tractor: copying templates pinc"
  VERBATIM)

add_custom_command(
  OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/mpi_layer.cpp.in ${CMAKE_CURRENT_BINARY_DIR}/mpi_layer.cpp.in.end 
  COMMAND ${CMAKE_COMMAND} -E copy 
  ${CMAKE_CURRENT_SOURCE_DIR}/mpi_layer_comm_begin.epilog.h 
  ${CMAKE_CURRENT_SOURCE_DIR}/mpi_layer_comm_begin.prolog.h 
  ${CMAKE_CURRENT_SOURCE_DIR}/mpi_layer_comm_end.epilog.h 
  ${CMAKE_CURRENT_SOURCE_DIR}/mpi_layer_comm_end.prolog.h 
  ${CMAKE_CURRENT_SOURCE_DIR}/mpi_layer.cpp.in 
  ${CMAKE_CURRENT_SOURCE_DIR}/mpi_layer.cpp.in.end 
  ${CMAKE_CURRENT_BINARY_DIR}
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMENT "tractor: copying templates mpi_layer"
  VERBATIM)

add_custom_command(
  OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/mpi_func.inc
  COMMAND ${TRACTOR_CPP_EXE} -E ${_cpp_input} ${_common_dir}/mpi_func.inc -o ${CMAKE_CURRENT_BINARY_DIR}/mpi_func.inc
  DEPENDS ${_common_dir}/mpi_func.inc ${_common_dir}/mpi_func_ids.h ${CMAKE_CURRENT_BINARY_DIR}/pinc
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMENT "tractor: preprocessing mpi_func.inc (TARGET=${TRACTOR_TARGET})"
  VERBATIM)

add_custom_command(
  OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/mpi_func_ids_through.h
         ${CMAKE_CURRENT_BINARY_DIR}/mpi_func_names.h
         ${CMAKE_CURRENT_BINARY_DIR}/mpi_func_ids_trans.h
  COMMAND ${PERL_EXECUTABLE} ${_common_dir}/mpi_func_ids.pl
          -i ${_common_dir}/mpi_func_ids.h mpi_func.inc
          -o mpi_func_ids_through.h mpi_func_names.h mpi_func_ids_trans.h
  DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/mpi_func.inc ${_common_dir}/mpi_func_ids.h ${_common_dir}/mpi_func_ids.pl
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMENT "tractor: generating mpi_func_ids_through.h / mpi_func_names.h / mpi_func_ids_trans.h"
  VERBATIM)

add_custom_command(
  OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/mpi_layer.cpp ${_fab_shim_outputs}
  COMMAND ${PERL_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/mpi_layer.pl -p on -e ${_print_extended_stuff} -m ${_maxmpi} -t ${_pct} -c ${_parse_gpu_flag}
          -s ${_mpi_layer_skip_core} -i mpi_func.inc -o mpi_layer.cpp ${_fab_func_layers}
  DEPENDS mpi_layer.cpp.in mpi_layer.cpp.in.end mpi_layer.pl
          ${CMAKE_CURRENT_BINARY_DIR}/mpi_func_ids_through.h
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMENT "tractor: generating mpi_layer.cpp (TARGET=${TRACTOR_TARGET}, FABRIC=${FABRIC})"
  VERBATIM)

add_custom_target(tractor_codegen DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/mpi_layer.cpp ${_fab_shim_outputs})

# ---------------------------------------------------------------------------
# Targets
# ---------------------------------------------------------------------------
if(TRACTOR_TARGET STREQUAL "fab_layers")
  # Standalone: only the 4 psm2/psm3/ucx/ofi shim .so's - no core lib/ptpd.
  # Each shim dlsym()'s its real symbols at runtime and never calls real MPI,
  # so (like the Makefile) it's compiled with the MPI compiler (for mpi.h)
  # but *linked* with a plain, non-MPI-wrapped C++ compiler + -ldl, to avoid
  # pulling in an unwanted libmpi dependency.
  set(TRACTOR_PLAIN_CXX "$ENV{CXX}" CACHE STRING
    "plain (non-MPI-wrapped) C++ compiler used only to LINK the fab_layers shim .so's; defaults to \$ENV{CXX} or CMAKE_CXX_COMPILER")
  if(NOT TRACTOR_PLAIN_CXX)
    set(TRACTOR_PLAIN_CXX ${CMAKE_CXX_COMPILER})
  endif()

  set(_fab_shim_libs)

  foreach(_layer psm2 psm3 ucx ofi)
    set(_src ${CMAKE_CURRENT_BINARY_DIR}/mpi_layer_fab_${_layer}.cpp)
    set(_obj ${CMAKE_CURRENT_BINARY_DIR}/mpi_layer_fab_${_layer}.o)
    set(_soname libtractor${FABLIBSUFFIX}_fab_${_layer}.so.${TRACTOR_MAJOR}.${TRACTOR_MINOR})
    set(_so ${CMAKE_CURRENT_BINARY_DIR}/${_soname})

    add_custom_command(
      OUTPUT ${_obj}
      COMMAND ${CMAKE_CXX_COMPILER} -o ${_obj} -c
              -I${CMAKE_CURRENT_SOURCE_DIR} -I${_common_dir} -I${CMAKE_CURRENT_BINARY_DIR}
              -I${VERBSPATH}/include -I${PSM2PATH}/include
              -I${CMAKE_CURRENT_SOURCE_DIR}/../../contrib/psm3/include
              -I${UCXPATH}/include -I${OFIPATH}/include
              ${_tractor_defs} -D_FAB_PROFILE_ ${TRACTOR_CXX_MISC} ${TRACTOR_OPT} -fPIC ${_src}
      DEPENDS ${_src}
      WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
      COMMENT "tractor(fab_layers): compiling ${_layer} shim"
      VERBATIM)

    add_custom_command(
      OUTPUT ${_so}
      COMMAND ${TRACTOR_PLAIN_CXX} -o ${_so} ${TRACTOR_OPT} -shared -fPIC ${_obj} -ldl
      DEPENDS ${_obj}
      COMMENT "tractor(fab_layers): linking ${_soname}"
      VERBATIM)

    list(APPEND _fab_shim_libs ${_so})
  endforeach()

  add_custom_target(tractor_fab_layers ALL DEPENDS ${_fab_shim_libs})
  add_dependencies(tractor_fab_layers tractor_codegen)

  set(_fab_install_dir ${CMAKE_CURRENT_SOURCE_DIR}/../../../lib/${TRACTOR_COMPILER})
  install(FILES ${_fab_shim_libs} DESTINATION ${_fab_install_dir}
          PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)
  foreach(_layer psm2 psm3 ucx ofi)
    set(_base tractor${FABLIBSUFFIX}_fab_${_layer})
    install(CODE "
      execute_process(COMMAND ${CMAKE_COMMAND} -E create_symlink
        lib${_base}.so.${TRACTOR_MAJOR}.${TRACTOR_MINOR} lib${_base}.so.${TRACTOR_MAJOR}
        WORKING_DIRECTORY \"${_fab_install_dir}\")
      execute_process(COMMAND ${CMAKE_COMMAND} -E create_symlink
        lib${_base}.so.${TRACTOR_MAJOR} lib${_base}.so
        WORKING_DIRECTORY \"${_fab_install_dir}\")
    ")
  endforeach()
else()
  list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/../cmake)
  find_package(hwloc REQUIRED)
  add_library(tractor SHARED ${TRACTOR_SOURCES})
  add_dependencies(tractor tractor_codegen)
  set_target_properties(tractor PROPERTIES
    OUTPUT_NAME "tractor${TRACTOR_LIB_SUFFIX}"
    VERSION ${TRACTOR_MAJOR}.${TRACTOR_MINOR}
    SOVERSION ${TRACTOR_MAJOR}
    POSITION_INDEPENDENT_CODE ON)
  target_compile_definitions(tractor PRIVATE ${TRACTOR_CONTROLS})
  target_compile_options(tractor PRIVATE ${TRACTOR_OPT} ${TRACTOR_CXX_MISC} ${TRACTOR_OMP_FLAG})
  target_link_options(tractor PRIVATE ${TRACTOR_OMP_FLAG})
  target_include_directories(tractor PRIVATE . ${_common_dir} ${hwloc_INCLUDE_DIRS} ${CMAKE_CURRENT_BINARY_DIR} ${CMAKE_CURRENT_SOURCE_DIR})
  target_link_libraries(tractor PRIVATE ${TRACTOR_LIBS} hwloc::hwloc)
  if(NOT _fabric_upper STREQUAL "OFF")
    target_include_directories(tractor PRIVATE
      ${VERBSPATH}/include ${PSM2PATH}/include
      ${CMAKE_CURRENT_SOURCE_DIR}/../../contrib/psm3/include ${UCXPATH}/include ${OFIPATH}/include)
  endif()

  if(PTPD_BUILD)
    add_executable(ptpd src-ptpd.cpp t_globals.cpp)
    add_dependencies(ptpd tractor_codegen)
    set_target_properties(ptpd PROPERTIES OUTPUT_NAME "ptpd${TRACTOR_BIN_SUFFIX}")
    target_compile_definitions(ptpd PRIVATE ${TRACTOR_CONTROLS})
    target_compile_options(ptpd PRIVATE ${TRACTOR_OPT} ${TRACTOR_CXX_MISC} ${TRACTOR_OMP_FLAG})
    target_link_options(ptpd PRIVATE ${TRACTOR_OMP_FLAG})
    target_include_directories(ptpd PRIVATE . ${_common_dir})
    target_link_libraries(ptpd PRIVATE ZLIB::ZLIB)
  endif()

  # -------------------------------------------------------------------------
  # Install, mirroring the Makefile's install: rule (mpi.tool/lib|bin/<COMPILER>/)
  # -------------------------------------------------------------------------
  install(TARGETS tractor LIBRARY DESTINATION ${CMAKE_CURRENT_SOURCE_DIR}/../../../lib/${TRACTOR_COMPILER})
  if(PTPD_BUILD)
    install(TARGETS ptpd RUNTIME DESTINATION ${CMAKE_CURRENT_SOURCE_DIR}/../../../bin/${TRACTOR_COMPILER})
  endif()
endif()
