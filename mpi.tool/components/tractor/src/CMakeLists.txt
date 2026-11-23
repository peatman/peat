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
# TARGET=transport_layers standalone shim build and the legacy single-layer
# TRANSPORT=PSM2/VERBS/PSM3/UCX/OFI modes. BUILD_TRACTOR_ENGINEERING_VERSION=auto
# detects a patched MPI by trying to link against mpi_internal_comm_create
# (the exported communicator hook every ../../patches/* hook patch adds) - see its
# definition below for the reasoning and caveats.
#
# CMAKE_CXX_COMPILER/CMAKE_C_COMPILER are expected to already be the MPI
# wrappers (mpicxx/mpicc, mpiicpc/mpiicc, Cray CC/cc, ...) - same convention
# as MPICXX/MPICC in env.sh. Point cmake at them the usual way, e.g.:
#   cmake -DCMAKE_CXX_COMPILER=mpicxx -DCMAKE_C_COMPILER=mpicc ..
#
cmake_minimum_required(VERSION 3.16...4.4)
# The compiler test must not link: with an MPI wrapper as compiler the link fails when libmpi and libfabric do not match
# (engineering vs stock), and the clear "version mismatch" error further down is wanted instead of a linker log.
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)
project(tractor CXX C)
unset(CMAKE_TRY_COMPILE_TARGET_TYPE)
# Without a linking compiler test CMake detects no implicit link directories (nor CMAKE_LIBRARY_ARCHITECTURE), so
# find_library would miss the ones only the compiler knows, e.g. /usr/lib/x86_64-linux-gnu on Debian/Ubuntu (libz,
# libhwloc, libfabric, libcupti, ...). Take them from the compiler itself (-print-search-dirs: GCC, clang, icpx; nvc++
# has none and adds nothing) as system library directories: searched after everything given explicitly (HINTS,
# CMAKE_PREFIX_PATH, OFIPATH, ...), and not at all where NO_DEFAULT_PATH is used.
execute_process(COMMAND ${CMAKE_CXX_COMPILER} -print-search-dirs
                OUTPUT_VARIABLE _out ERROR_QUIET RESULT_VARIABLE _rc)
if(_rc EQUAL 0 AND _out MATCHES "(^|\n)libraries: *=?([^\n]*)")
  string(REPLACE ":" ";" _dirs "${CMAKE_MATCH_2}")
  set(_tractor_compiler_lib_dirs)
  foreach(_d IN LISTS _dirs)
    if(_d AND IS_DIRECTORY "${_d}")
      get_filename_component(_d "${_d}" ABSOLUTE) # resolve the ../.. of the compiler's own paths
      if(NOT _d IN_LIST _tractor_compiler_lib_dirs AND NOT _d IN_LIST CMAKE_SYSTEM_LIBRARY_PATH)
        list(APPEND _tractor_compiler_lib_dirs "${_d}")
      endif()
    endif()
  endforeach()
  list(APPEND CMAKE_SYSTEM_LIBRARY_PATH ${_tractor_compiler_lib_dirs})
  string(REPLACE ";" " " _shown "${_tractor_compiler_lib_dirs}")
  message(STATUS "tractor: library directories of the compiler: ${_shown}")
endif()

# libfabric: the system one unless OFIPATH (-DOFIPATH, or $OFIPATH / $OFI_PATH as env.sh sets them; libfabric_eng=-eng
# selects the engineering one there) points to another installation
if(NOT OFIPATH)
  if(DEFINED ENV{OFIPATH})
    set(OFIPATH "$ENV{OFIPATH}")
  elseif(DEFINED ENV{OFI_PATH})
    set(OFIPATH "$ENV{OFI_PATH}")
  endif()
endif()
set(TRACTOR_OFI_INCLUDE_DIR "")
if(OFIPATH)
  set(TRACTOR_OFI_INCLUDE_DIR "${OFIPATH}/include")
endif()

# Engineering libfabric / MPI consistency. The engineering libfabric exports functions that the stock one only has as
# "static inline" in its headers (fi_close, ...); the engineering libmpi references them as undefined symbols. The
# two have to agree: an engineering MPI needs the engineering libfabric ($OFIPATH, libfabric_eng='-eng' in env.sh)
# and the other way round. The transport_layers shims redefine those same functions, so they only compile against
# the engineering libfabric (headers and library).
find_program(TRACTOR_NM_EXE nm)
find_program(TRACTOR_READELF_EXE readelf)
if(OFIPATH)
  find_library(TRACTOR_OFI_LIBRARY fabric HINTS ${OFIPATH}/lib ${OFIPATH}/lib64 NO_DEFAULT_PATH)
else()
  find_library(TRACTOR_OFI_LIBRARY fabric)
endif()
if(TRACTOR_TARGET STREQUAL "class_layers")
  message(STATUS "tractor: class_layers: the event-class shims, no MPI library or libfabric check")
elseif(TRACTOR_TARGET STREQUAL "transport_layers")
  if(TRACTOR_NM_EXE AND TRACTOR_OFI_LIBRARY)
    execute_process(COMMAND ${TRACTOR_NM_EXE} -D --defined-only ${TRACTOR_OFI_LIBRARY}
                    OUTPUT_VARIABLE _ofi_syms ERROR_QUIET)
    if(NOT _ofi_syms MATCHES "(^|\n)[0-9a-f]+ [TtWw] fi_close(\n|$)")
      message(FATAL_ERROR "tractor: TRACTOR_TARGET=transport_layers needs the engineering libfabric, but ${TRACTOR_OFI_LIBRARY} is "
        "a stock build (fi_close is only an inline function there, the shims redefine it). Point OFIPATH to the engineering "
        "libfabric (libfabric_eng='-eng' before env.sh).")
    endif()
    message(STATUS "tractor: engineering libfabric for the transport shims (${TRACTOR_OFI_LIBRARY})")
  else()
    message(STATUS "tractor: libfabric edition not checked (nm or libfabric not found)")
  endif()
else()
  # FindMPI leaves the library list empty when the compiler already is the MPI wrapper, so ask the wrapper
  # (MPICH style "-show": -L<dir>) and look next to it (<prefix>/bin/mpicc -> <prefix>/lib)
  set(_mpi_hints "")
  if(CMAKE_C_COMPILER) # the MPI wrapper
    get_filename_component(_mpi_bin "${CMAKE_C_COMPILER}" DIRECTORY)
    list(APPEND _mpi_hints ${_mpi_bin}/../lib ${_mpi_bin}/../lib64)
    execute_process(COMMAND ${CMAKE_C_COMPILER} -show OUTPUT_VARIABLE _mpi_show ERROR_QUIET OUTPUT_STRIP_TRAILING_WHITESPACE)
    string(REGEX MATCHALL "-L[^ ]+" _mpi_ldirs "${_mpi_show}")
    foreach(_d ${_mpi_ldirs})
      string(SUBSTRING "${_d}" 2 -1 _d)
      list(APPEND _mpi_hints ${_d})
    endforeach()
  endif()
  unset(TRACTOR_MPI_LIBRARY CACHE)
  find_library(TRACTOR_MPI_LIBRARY NAMES mpi HINTS ${_mpi_hints} NO_DEFAULT_PATH)
  set(_mpi_lib "${TRACTOR_MPI_LIBRARY}")
  set(_mpi_needs_ofi ON)
  if(TRACTOR_READELF_EXE AND _mpi_lib)
    execute_process(COMMAND ${TRACTOR_READELF_EXE} -d ${_mpi_lib} OUTPUT_VARIABLE _mpi_dyn ERROR_QUIET)
    if(NOT _mpi_dyn MATCHES "NEEDED[^\n]*libfabric")
      set(_mpi_needs_ofi OFF)
    endif()
  endif()
  if(NOT _mpi_needs_ofi)
    # e.g. MVAPICH 2 (verbs) or a stock Open MPI: no libfabric dependency, so there is nothing to match
    message(STATUS "tractor: ${_mpi_lib} does not link libfabric, engineering libfabric check skipped")
  elseif(TRACTOR_NM_EXE AND TRACTOR_OFI_LIBRARY AND _mpi_lib)
    execute_process(COMMAND ${TRACTOR_NM_EXE} -D --defined-only ${TRACTOR_OFI_LIBRARY}
                    OUTPUT_VARIABLE _ofi_syms ERROR_QUIET)
    execute_process(COMMAND ${TRACTOR_NM_EXE} -D --undefined-only ${_mpi_lib}
                    OUTPUT_VARIABLE _mpi_syms ERROR_QUIET)
    execute_process(COMMAND ${TRACTOR_NM_EXE} -D --defined-only ${_mpi_lib}
                    OUTPUT_VARIABLE _mpi_defs ERROR_QUIET)
    set(_ofi_eng OFF)
    set(_mpi_ofi_eng OFF)
    if(_ofi_syms MATCHES "(^|\n)[0-9a-f]+ [TtWw] fi_close(\n|$)")
      set(_ofi_eng ON)
    endif()
    # engineering MPI: it needs fi_close & co from libfabric, or - when its libfabric use is limited to ordinary calls
    # (the GNU Open MPI builds) - it is the patched one that defines mpi_internal_comm_create
    if(_mpi_syms MATCHES "(^|\n) +[Uw] fi_close(\n|$)" OR _mpi_defs MATCHES "(^|\n)[0-9a-f]+ [TtWw] mpi_internal_comm_create(\n|$)")
      set(_mpi_ofi_eng ON)
    endif()
    if(_ofi_eng AND _mpi_ofi_eng)
      message(STATUS "tractor: engineering libfabric (${TRACTOR_OFI_LIBRARY}) and engineering MPI (${_mpi_lib})")
    elseif(NOT _ofi_eng AND NOT _mpi_ofi_eng)
      message(STATUS "tractor: stock libfabric (${TRACTOR_OFI_LIBRARY}) and stock MPI (${_mpi_lib})")
    elseif(_mpi_ofi_eng)
      message(FATAL_ERROR "tractor: version mismatch - the MPI library ${_mpi_lib} is an engineering build (it needs fi_close and "
        "friends from libfabric) but ${TRACTOR_OFI_LIBRARY} is a stock libfabric. Point OFIPATH to the engineering libfabric "
        "(libfabric_eng='-eng' before env.sh).")
    else()
      message(FATAL_ERROR "tractor: version mismatch - ${TRACTOR_OFI_LIBRARY} is an engineering libfabric but the MPI library "
        "${_mpi_lib} is a stock build. Use the engineering MPI or point OFIPATH to the stock libfabric.")
    endif()
  else()
    message(STATUS "tractor: libfabric/MPI engineering consistency not checked (nm, libfabric or libmpi not found)")
  endif()
endif()

# (run before find_package(MPI): a libmpi/libfabric mismatch makes that link test fail with a linker log)

set(TRACTOR_MAJOR 0)
set(TRACTOR_MINOR 1)
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
# 1. Compiler family: gnu / intel / nvc / clang
#    (override with -DTRACTOR_COMPILER=gnu|intel|nvc|clang to skip auto-detection; pgi = nvc)
# ---------------------------------------------------------------------------
if(NOT TRACTOR_COMPILER)
  if(CMAKE_CXX_COMPILER_ID STREQUAL "Intel" OR CMAKE_CXX_COMPILER_ID STREQUAL "IntelLLVM")
    set(_tractor_compiler intel)
  elseif(CMAKE_CXX_COMPILER_ID STREQUAL "PGI" OR CMAKE_CXX_COMPILER_ID STREQUAL "NVHPC")
    set(_tractor_compiler nvc)
  elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang") # Clang, AppleClang
    set(_tractor_compiler clang)
  elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    set(_tractor_compiler gnu)
  else()
    message(WARNING "tractor: unrecognized CXX compiler id '${CMAKE_CXX_COMPILER_ID}', defaulting TRACTOR_COMPILER=gnu")
    set(_tractor_compiler gnu)
  endif()
  set(TRACTOR_COMPILER ${_tractor_compiler} CACHE STRING "toolchain family: gnu|intel|nvc|clang" FORCE)
endif()
set_property(CACHE TRACTOR_COMPILER PROPERTY STRINGS gnu intel nvc clang)
message(STATUS "tractor: COMPILER=${TRACTOR_COMPILER} (CXX compiler id: ${CMAKE_CXX_COMPILER_ID})")

# ---------------------------------------------------------------------------
# 2. MPI vendor + version -> Makefile-style TARGET
#    -DTRACTOR_TARGET=auto (default) probes CMAKE_CXX_COMPILER to detect the
#    vendor/version; set it to impi|mpich3|mpich4|mpich5|mvapich2|mvapich4|openmpi|
#    cray|transport_layers|class_layers to skip probing and use that flavor directly ('cray' is
#    just an alias that normalizes to mpich4 - same MPICH-family struct layout).
# ---------------------------------------------------------------------------
set(TRACTOR_TARGET "auto" CACHE STRING
  "MPI flavor: auto|impi|mpich3|mpich4|mpich5|mvapich2|mvapich4|openmpi|cray|transport_layers|class_layers. \
'auto' detects the vendor/version from CMAKE_CXX_COMPILER; any other value is used \
directly and skips probing.")
set_property(CACHE TRACTOR_TARGET PROPERTY STRINGS auto impi mpich3 mpich4 mpich5 mvapich2 mvapich4 openmpi cray transport_layers class_layers)
string(TOLOWER "${TRACTOR_TARGET}" _tractor_target_lower)

if(_tractor_target_lower STREQUAL "cray")
  set(_tractor_target_lower mpich4) # Cray MPICH builds like stock MPICH 4.x
  set(TRACTOR_TARGET mpich4 CACHE STRING "MPI flavor: auto|impi|mpich3|mpich4|mpich5|mvapich2|mvapich4|openmpi|cray|transport_layers|class_layers" FORCE)
endif()

if(NOT _tractor_target_lower MATCHES "^(auto|impi|mpich3|mpich4|mpich5|mvapich2|mvapich4|openmpi|transport_layers|class_layers)$")
  message(FATAL_ERROR "tractor: invalid TRACTOR_TARGET='${TRACTOR_TARGET}' (expected auto|impi|mpich3|mpich4|mpich5|mvapich2|mvapich4|openmpi|cray|transport_layers|class_layers)")
endif()

if(NOT _tractor_target_lower MATCHES "^(auto|transport_layers|class_layers)$")
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
    elseif(_mpi_version MATCHES "^3\\.")
      set(_tractor_target mpich3)
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
  set(TRACTOR_TARGET ${_tractor_target} CACHE STRING "MPI flavor: auto|impi|mpich3|mpich4|mpich5|mvapich2|mvapich4|openmpi|cray|transport_layers|class_layers" FORCE)
endif()
message(STATUS "tractor: MPI vendor=${TRACTOR_MPI_VENDOR} version=${TRACTOR_MPI_VERSION} -> TARGET=${TRACTOR_TARGET}")

# ---------------------------------------------------------------------------
# 3. Remaining switches - passed as -D<NAME>=... on the cmake command line
# ---------------------------------------------------------------------------
set(MEM  "off" CACHE STRING "memory profiling tag: off|<value used as _MEMPROFILE_<MEM>_>, e.g. HOOKS")
set(DIAG OFF   CACHE BOOL   "debug/diagnostic build (-O0, _dbg suffix, entry/exit tracing)")
set(GPU  OFF   CACHE BOOL   "GPU call interception (PARSE_GPU): OpenCL, Level Zero and OpenACC, each one only when found")
# With GPU=ON every API below is looked for at configure time: AUTO enables it when its header (and library) are found,
# ON makes a missing one an error, OFF skips it.
set(OCL_PARSE "AUTO" CACHE STRING "OpenCL interception: AUTO|ON|OFF (needs CL/cl.h and libOpenCL)")
set(LZE_PARSE "AUTO" CACHE STRING "Level Zero interception: AUTO|ON|OFF (needs level_zero/ze_api.h and libze_loader)")
set(ACC_PARSE "AUTO" CACHE STRING "OpenACC interception: AUTO|ON|OFF (needs acc_prof.h, never built with the Intel compilers)")
set_property(CACHE OCL_PARSE PROPERTY STRINGS AUTO ON OFF)
set_property(CACHE LZE_PARSE PROPERTY STRINGS AUTO ON OFF)
set_property(CACHE ACC_PARSE PROPERTY STRINGS AUTO ON OFF)
set(LLAMA "" CACHE STRING "llama.cpp/ggml pipeline stage and CPU kernel interception (LLAMA_PARSE): AUTO|ON|OFF, part of the base build when it can be built (default AUTO)")
set_property(CACHE LLAMA PROPERTY STRINGS AUTO ON OFF)
set(LLAMA_ROOT "" CACHE PATH "llama.cpp source tree or install prefix the traced llama.cpp was built from (default: contrib/llama.cpp if checked out, else the installed headers)")
set(LLAMA_DEPRECATED OFF CACHE BOOL "also intercept the deprecated llama.cpp entry points (llama_decode, llama_encode)")
set(CUPTI "" CACHE STRING "CUDA through CUPTI (GPU activity, host runtime calls) and cuBLAS wrappers (CUPTI_PARSE): OFF|ON|AUTO, default AUTO with GPU=ON, OFF otherwise")
set_property(CACHE CUPTI PROPERTY STRINGS "" OFF ON AUTO)
set(CUDA_ROOT "" CACHE PATH "CUDA toolkit with CUPTI, of the CUDA version the traced application runs with (default: searched)")
# The same switches from the environment as the Makefile reads them (LLAMA_PARSE, LLAMA_ROOT, LLAMA_DEPRECATED,
# CUPTI_PARSE, CUDA_ROOT), used where the -D<NAME> one is not given. A LLAMA_ROOT from the environment alone makes
# llama.cpp mandatory (LLAMA=ON: its headers not being usable is an error, not a silent skip).
if(NOT LLAMA AND NOT "$ENV{LLAMA_PARSE}" STREQUAL "")
  set(LLAMA "$ENV{LLAMA_PARSE}")
endif()
if(NOT LLAMA_ROOT AND NOT "$ENV{LLAMA_ROOT}" STREQUAL "")
  set(LLAMA_ROOT "$ENV{LLAMA_ROOT}")
  if(NOT LLAMA)
    set(LLAMA ON)
  endif()
endif()
if(NOT LLAMA_DEPRECATED)
  set(_env_llama_deprecated "$ENV{LLAMA_DEPRECATED}")
  if(_env_llama_deprecated)
    set(LLAMA_DEPRECATED ON)
  endif()
endif()
if(NOT CUPTI AND NOT "$ENV{CUPTI_PARSE}" STREQUAL "")
  set(CUPTI "$ENV{CUPTI_PARSE}")
endif()
if(NOT CUDA_ROOT AND NOT "$ENV{CUDA_ROOT}" STREQUAL "")
  set(CUDA_ROOT "$ENV{CUDA_ROOT}")
endif()
set(BUF  OFF   CACHE BOOL   "extra MPI buffer fields (MPIBUFFERS)")
set(V2P  OFF   CACHE BOOL   "virtual->physical address resolution (VIRT2PHYS)")
set(HWC  "off" CACHE STRING "hardware counters tag: off|RDPMC")

set(TRANSPORT "off" CACHE STRING
  "transport-API interception: off|on|PSM2|VERBS|PSM3|UCX|OFI. 'on' and the legacy PSM2/VERBS/PSM3/UCX/OFI \
values embed one layer's interception into the core lib; for the standalone psm2+psm3+ucx+ofi shim \
.so build use -DTRACTOR_TARGET=transport_layers instead")
set_property(CACHE TRANSPORT PROPERTY STRINGS off on PSM2 VERBS PSM3 UCX OFI)

set(OMPT       ON  CACHE BOOL "OpenMP interception")
set(PTPD_BUILD ON  CACHE BOOL "build the ptpd PTP-daemon binary")

set(PEAT_TIMER "nano" CACHE STRING
  "timer backend for entry/exit timestamps: tsc|nano. 'tsc' uses raw/calibrated TSC reads \
(rdtsc family); 'nano' uses clock_gettime (via peat_get_time()) for a node-wide, core-invariant \
timer, at the cost of slightly higher per-call overhead (USE_NANO_TIMER). Default nano, as the Makefile \
(which always builds nano): a core and the shims preloaded with it have to share the timer (they check it), \
and CUPTI needs nano.")
set_property(CACHE PEAT_TIMER PROPERTY STRINGS tsc nano)
string(TOLOWER "${PEAT_TIMER}" _timer_lower)
if(NOT _timer_lower MATCHES "^(tsc|nano)$")
  message(FATAL_ERROR "tractor: invalid PEAT_TIMER='${PEAT_TIMER}' (expected tsc|nano)")
endif()

set(BUILD_TRACTOR_ENGINEERING_VERSION "auto" CACHE STRING
  "mpi-internal (\"MPI_INTERNAL\"/PARSE_MPI_INTERNAL) engineering build, needs a patched MPI: auto|off|on.")
set_property(CACHE BUILD_TRACTOR_ENGINEERING_VERSION PROPERTY STRINGS auto off on)
string(TOLOWER "${BUILD_TRACTOR_ENGINEERING_VERSION}" _eng_lower)
if(_eng_lower STREQUAL "auto")
  if(TRACTOR_TARGET MATCHES "^(transport|class)_layers$")
    message(STATUS "tractor: BUILD_TRACTOR_ENGINEERING_VERSION=auto -> 'off' (not applicable to TARGET=${TRACTOR_TARGET})")
    set(_eng_lower off)
  else()
    # every hook patch under ../../patches (MPICH, MVAPICH, Open MPI) adds the same exported communicator
    # hook mpi_internal_comm_create(); a stock/unpatched MPI never defines it, so
    # trying to link a program that references it tells us the install is patched
    # - this assumes CMAKE_C_COMPILER is the MPI wrapper (mpicc), same as elsewhere
    include(CheckFunctionExists)
    set(CMAKE_REQUIRED_QUIET ON)
    check_function_exists(mpi_internal_comm_create TRACTOR_MPI_PATCHED)
    unset(CMAKE_REQUIRED_QUIET)
    if(TRACTOR_MPI_PATCHED)
      message(STATUS "tractor: BUILD_TRACTOR_ENGINEERING_VERSION=auto -> 'on' (mpi_internal_comm_create found: patched/engineering MPI)")
      set(_eng_lower on)
    else()
      message(STATUS "tractor: BUILD_TRACTOR_ENGINEERING_VERSION=auto -> 'off' (mpi_internal_comm_create not found: stock MPI)")
      set(_eng_lower off)
    endif()
  endif()
endif()

string(TOLOWER "${MEM}" _mem_lower)
string(TOLOWER "${HWC}" _hwc_lower)
string(TOUPPER "${TRANSPORT}" _transport_upper)

# ---------------------------------------------------------------------------
# Third-party dependencies
# ---------------------------------------------------------------------------
find_package(ZLIB REQUIRED)

# only used when TRANSPORT != off or TRACTOR_TARGET=transport_layers. VERBSPATH defaults to /usr; PSM2/PSM3/UCX use the
# system installation unless a path is given (-D<NAME> or the environment variable, as env.sh and Build.sh export them)
if(NOT VERBSPATH)
  if(DEFINED ENV{VERBSPATH})
    set(VERBSPATH "$ENV{VERBSPATH}")
  else()
    set(VERBSPATH "/usr")
  endif()
endif()
if(NOT PSM2PATH AND DEFINED ENV{PSM2PATH})
  set(PSM2PATH "$ENV{PSM2PATH}")
endif()
if(NOT PSM3PATH AND DEFINED ENV{PSM3PATH})
  set(PSM3PATH "$ENV{PSM3PATH}")
endif()
if(NOT UCXPATH AND DEFINED ENV{UCXPATH})
  set(UCXPATH "$ENV{UCXPATH}")
endif()
set(TRACTOR_PSM2_INCLUDE_DIR "")
if(PSM2PATH)
  set(TRACTOR_PSM2_INCLUDE_DIR "${PSM2PATH}/include")
endif()
set(TRACTOR_UCX_INCLUDE_DIR "")
if(UCXPATH)
  set(TRACTOR_UCX_INCLUDE_DIR "${UCXPATH}/include")
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
  _MPI_MESSAGE_DEFINED_ _NO_C_VR
)
set(TRACTOR_SOURCES
  t_base_event.cpp tractor.cpp t_stat.cpp t_comm.cpp elf_parser.cpp t_topo.cpp t_globals.cpp t_signals.cpp
  ${CMAKE_CURRENT_BINARY_DIR}/mpi_layer.cpp
)
set(TRACTOR_LIBS ZLIB::ZLIB)
set(TRACTOR_EXTRANAME "")
set(_print_extended_stuff off)

# TRACTOR_TARGET=class_layers: the event-class shims (see the Makefile's TARGET=class_layers): libtractor_ompt.so always,
# libtractor_acc.so (acc_prof.h: not with the Intel compilers), libtractor_cuda.so (CUPTI found), libtractor_llama.so
# (llama.cpp headers usable), libtractor_opencl.so / libtractor_levelzero.so (headers found). They reach the core only
# through t_shared (t_shim_state.h), so one build fits every core variant - except the timer (PEAT_TIMER), which the
# core and the shims have to share (the shims check it; the CUDA shim needs nano). No HWC/MEM/DIAG: core switches.
set(_class_layers OFF)
if(TRACTOR_TARGET STREQUAL "class_layers")
  set(_class_layers ON)
  if(DIAG OR NOT _hwc_lower STREQUAL "off" OR NOT _mem_lower STREQUAL "off")
    message(FATAL_ERROR "tractor: TRACTOR_TARGET=class_layers is built without DIAG, HWC and MEM: those are switches of the core, the shims fit any of its variants")
  endif()
endif()

# GPU API detection: sets _gpu_cl / _gpu_ze / _gpu_acc (ON/OFF) for what is built
set(_gpu_cl OFF)
set(_gpu_ze OFF)
set(_gpu_acc OFF)
set(TRACTOR_GPU_INCLUDE_DIRS "")
if(GPU OR _class_layers) # class_layers: their shims, when found
  foreach(_opt OCL_PARSE LZE_PARSE ACC_PARSE)
    string(TOUPPER "${${_opt}}" ${_opt})
    if(NOT ${_opt} MATCHES "^(AUTO|ON|OFF)$")
      message(FATAL_ERROR "tractor: -D${_opt}=${${_opt}} is not one of AUTO|ON|OFF")
    endif()
  endforeach()

  # A header counts as present when the compiler can include it (that sees CPATH, the compiler's own include
  # directories, ...). find_path only supplies an explicit -I for headers outside of that.
  include(CheckIncludeFileCXX)
  if(NOT OCL_PARSE STREQUAL "OFF")
    # the oneAPI compiler ships its own OpenCL headers and loader (${CMPLR_ROOT}/include/CL/cl.h, ${CMPLR_ROOT}/lib)
    if(TRACTOR_COMPILER STREQUAL "intel")
      find_path(TRACTOR_OPENCL_INCLUDE_DIR CL/cl.h HINTS ENV CMPLR_ROOT PATH_SUFFIXES include)
      find_library(TRACTOR_OPENCL_LIBRARY OpenCL HINTS ENV CMPLR_ROOT PATH_SUFFIXES lib)
    else()
      find_path(TRACTOR_OPENCL_INCLUDE_DIR CL/cl.h)
      find_library(TRACTOR_OPENCL_LIBRARY OpenCL)
    endif()
    set(CMAKE_REQUIRED_QUIET ON)
    set(CMAKE_REQUIRED_INCLUDES ${TRACTOR_OPENCL_INCLUDE_DIR})
    check_include_file_cxx(CL/cl.h TRACTOR_HAVE_CL_H)
    unset(CMAKE_REQUIRED_INCLUDES)
    unset(CMAKE_REQUIRED_QUIET)
    if(TRACTOR_HAVE_CL_H AND TRACTOR_OPENCL_LIBRARY)
      set(_gpu_cl ON)
      if(TRACTOR_OPENCL_INCLUDE_DIR)
        list(APPEND TRACTOR_GPU_INCLUDE_DIRS ${TRACTOR_OPENCL_INCLUDE_DIR})
      endif()
      list(APPEND TRACTOR_LIBS ${TRACTOR_OPENCL_LIBRARY})
    elseif(OCL_PARSE STREQUAL "ON")
      message(FATAL_ERROR "tractor: OCL_PARSE=ON but CL/cl.h or libOpenCL was not found (set CMAKE_PREFIX_PATH or -DOCL_PARSE=AUTO|OFF)")
    endif()
  endif()

  if(NOT LZE_PARSE STREQUAL "OFF")
    find_path(TRACTOR_LZE_INCLUDE_DIR level_zero/ze_api.h)
    find_library(TRACTOR_LZE_LIBRARY ze_loader)
    set(CMAKE_REQUIRED_QUIET ON)
    set(CMAKE_REQUIRED_INCLUDES ${TRACTOR_LZE_INCLUDE_DIR})
    check_include_file_cxx(level_zero/ze_api.h TRACTOR_HAVE_ZE_API_H)
    unset(CMAKE_REQUIRED_INCLUDES)
    unset(CMAKE_REQUIRED_QUIET)
    if(TRACTOR_HAVE_ZE_API_H AND TRACTOR_LZE_LIBRARY)
      set(_gpu_ze ON)
      if(TRACTOR_LZE_INCLUDE_DIR)
        list(APPEND TRACTOR_GPU_INCLUDE_DIRS ${TRACTOR_LZE_INCLUDE_DIR})
      endif()
      list(APPEND TRACTOR_LIBS ${TRACTOR_LZE_LIBRARY})
    elseif(LZE_PARSE STREQUAL "ON")
      message(FATAL_ERROR "tractor: LZE_PARSE=ON but level_zero/ze_api.h or libze_loader was not found (set CMAKE_PREFIX_PATH or -DLZE_PARSE=AUTO|OFF)")
    endif()
  endif()

  # acc_prof.h comes with the compiler (GCC >= 9, NVHPC); the Intel compilers do not have it
  if(NOT ACC_PARSE STREQUAL "OFF")
    if(TRACTOR_COMPILER STREQUAL "intel")
      if(ACC_PARSE STREQUAL "ON")
        message(WARNING "tractor: ACC_PARSE=ON ignored, the Intel compilers have no OpenACC profiling interface header")
      endif()
    else()
      check_include_file_cxx(acc_prof.h TRACTOR_HAVE_ACC_PROF_H)
      if(TRACTOR_HAVE_ACC_PROF_H)
        set(_gpu_acc ON)
      elseif(ACC_PARSE STREQUAL "ON")
        message(FATAL_ERROR "tractor: ACC_PARSE=ON but acc_prof.h was not found (GCC >= 9 or NVHPC needed; or -DACC_PARSE=AUTO|OFF)")
      endif()
    endif()
  endif()

  message(STATUS "tractor: GPU parsing: OpenCL=${_gpu_cl} LevelZero=${_gpu_ze} OpenACC=${_gpu_acc}")
  # "no GPU API" is told after CUDA (CUPTI) is looked for, further down
endif()

set(_gpu_apis "")
if(_gpu_cl)
  list(APPEND _gpu_apis cl)
  list(APPEND TRACTOR_CONTROLS GPUPARSE_CL)
endif()
if(_gpu_ze)
  list(APPEND _gpu_apis ze)
  list(APPEND TRACTOR_CONTROLS GPUPARSE_ZE)
endif()
if(_gpu_apis)
  list(APPEND TRACTOR_CONTROLS GPUPARSE)
endif()
if((_gpu_apis OR _gpu_acc) AND NOT _class_layers) # the shim names get no .gpu
  set(TRACTOR_EXTRANAME "${TRACTOR_EXTRANAME}.gpu")
endif()

# transport_func_layers: extra "-l <layer>[=outfile]" args forwarded to mpi_layer.pl.
# TARGET=mvapich2 always forwards verbs/smpi/psm2 into the core mpi_layer.cpp
# (ibv_*/smpi calls are static-inline, so the patched mvapich2 source calls
# mv2_proxy_* directly instead of dlsym-interception); TARGET=transport_layers
# overrides this entirely with its own 4 standalone-shim outfiles.
set(_transport_func_layers)
set(_mpi_layer_skip_core off)
set(_transport_shim_outputs)
if(TRACTOR_TARGET STREQUAL "transport_layers")
  set(_mpi_layer_skip_core on)
  set(_transport_func_layers
    -l psm2=mpi_layer_transport_psm2.cpp -l psm3=mpi_layer_transport_psm3.cpp
    -l ucx=mpi_layer_transport_ucx.cpp   -l ofi=mpi_layer_transport_ofi.cpp)
  set(_transport_shim_outputs
    ${CMAKE_CURRENT_BINARY_DIR}/mpi_layer_transport_psm2.cpp ${CMAKE_CURRENT_BINARY_DIR}/mpi_layer_transport_psm3.cpp
    ${CMAKE_CURRENT_BINARY_DIR}/mpi_layer_transport_ucx.cpp  ${CMAKE_CURRENT_BINARY_DIR}/mpi_layer_transport_ofi.cpp)
else()
  if(TRACTOR_TARGET STREQUAL "mvapich2")
    list(APPEND _transport_func_layers -l verbs -l smpi -l psm2)
  endif()

  # TRANSPORT: 'on' is generic multi-layer core support; PSM2/VERBS/PSM3/UCX/OFI
  # are the legacy single-layer-only core-embedded builds
  if(_transport_upper STREQUAL "ON")
    list(APPEND TRACTOR_CONTROLS _TRANSPORT_PROFILE_)
    list(APPEND TRACTOR_SOURCES t_transport.cpp)
  elseif(_transport_upper STREQUAL "PSM2")
    list(APPEND TRACTOR_CONTROLS _PSM2_PROFILE_)
    list(APPEND TRACTOR_SOURCES t_transport.cpp)
    list(APPEND _transport_func_layers -l psm2)
    set(TRACTOR_EXTRANAME "${TRACTOR_EXTRANAME}.psm2")
  elseif(_transport_upper STREQUAL "VERBS")
    list(APPEND TRACTOR_CONTROLS _VERBS_PROFILE_)
    list(APPEND TRACTOR_SOURCES t_transport.cpp)
    list(APPEND _transport_func_layers -l verbs -l smpi)
    set(TRACTOR_EXTRANAME "${TRACTOR_EXTRANAME}.verbs")
  elseif(_transport_upper STREQUAL "PSM3")
    list(APPEND TRACTOR_CONTROLS _PSM3_PROFILE_)
    list(APPEND TRACTOR_SOURCES t_transport.cpp)
    list(APPEND _transport_func_layers -l psm3)
    set(TRACTOR_EXTRANAME "${TRACTOR_EXTRANAME}.psm3")
  elseif(_transport_upper STREQUAL "UCX")
    list(APPEND TRACTOR_CONTROLS _UCX_PROFILE_)
    list(APPEND TRACTOR_SOURCES t_transport.cpp)
    list(APPEND _transport_func_layers -l ucx)
    set(TRACTOR_EXTRANAME "${TRACTOR_EXTRANAME}.ucx")
  elseif(_transport_upper STREQUAL "OFI")
    list(APPEND TRACTOR_CONTROLS _OFI_PROFILE_)
    list(APPEND TRACTOR_SOURCES t_transport.cpp)
    list(APPEND _transport_func_layers -l ofi)
    set(TRACTOR_EXTRANAME "${TRACTOR_EXTRANAME}.ofi")
  elseif(NOT _transport_upper STREQUAL "OFF")
    message(FATAL_ERROR "tractor: invalid TRANSPORT='${TRANSPORT}' (expected off|on|PSM2|VERBS|PSM3|UCX|OFI)")
  endif()
endif()

# cpp_input / TARGET-specific CONTROLS
set(_cpp_input)
# highest MPI standard level whose wrappers mpi_layer.pl emits (-m); the generated code is
# additionally guarded by MPI_VERSION/MPI_SUBVERSION from the mpi.h in use
set(_maxmpi 3)
if(TRACTOR_TARGET STREQUAL "impi")
  # Intel MPI goes by _IMPI_ alone (no _MPICH_): its MPI-internal helpers differ from MPICH 3's
  list(APPEND _cpp_input -D_IMPI_)
  list(APPEND TRACTOR_CONTROLS _IMPI_ _I_MPI_WEAK_EXPORTS_ I_MPI_BUILD)
  set(_maxmpi 4)
elseif(TRACTOR_TARGET STREQUAL "mpich3")
  list(APPEND _cpp_input -D_MPICH_=3)
  list(APPEND TRACTOR_CONTROLS _MPICH_=3)
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
elseif(TRACTOR_TARGET STREQUAL "openmpi")
  # same as the Makefile's TARGET=openmpi5: the headers and mpi_layer.pl pick the Open MPI branches (pointer handles,
  # the Open MPI flavour of the MPI-internal hooks) by _OPENMPI_
  list(APPEND _cpp_input -D_OPENMPI_=5)
  list(APPEND TRACTOR_CONTROLS _OPENMPI_=5)
endif()
# MPI 4.x prototype probes: some MPI 4.x implementations declare a few functions without the standard's
# const (MPICH 4.0: MPI_Psend_init, MPI_Pready_list; MPICH 4.2/MVAPICH 4.1/Intel MPI 2021: MPI_Status_get_*,
# MPI_Request_get_status_*) or lack them (MPICH 4.0: MPI_Status_set_elements_c); mpi_func.inc picks the
# matching entries. Compile-only: C++ rejects the pointer assignment when the prototype differs or is missing.
# Each entry: <cache var>|<cpp define when the snippet compiles (or, with !, when it does not)>|<snippet>
include(CheckCXXSourceCompiles)
set(_tractor_proto_probes
  "TRACTOR_MPI41_REQUEST_NONCONST|_MPI41_REQUEST_NONCONST_|int (*p)(int, MPI_Request*, int*, MPI_Status*) = MPI_Request_get_status_all"
  "TRACTOR_MPI41_STATUS_NONCONST|_MPI41_STATUS_NONCONST_|int (*p)(MPI_Status*, int*) = MPI_Status_get_tag"
  "TRACTOR_MPI4_PSEND_INIT_NONCONST|_MPI4_PSEND_INIT_NONCONST_|int (*p)(void*, int, MPI_Count, MPI_Datatype, int, int, MPI_Comm, MPI_Info, MPI_Request*) = MPI_Psend_init"
  "TRACTOR_MPI4_PREADY_LIST_NONCONST|_MPI4_PREADY_LIST_NONCONST_|int (*p)(int, int*, MPI_Request) = MPI_Pready_list"
  "TRACTOR_MPI4_HAVE_STATUS_SET_ELEMENTS_C|!_MPI4_NO_STATUS_SET_ELEMENTS_C_|int (*p)(MPI_Status*, MPI_Datatype, MPI_Count) = MPI_Status_set_elements_c")
set(CMAKE_REQUIRED_QUIET ON)
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)
set(_tractor_proto_defs)
foreach(_probe IN LISTS _tractor_proto_probes)
  string(REPLACE "|" ";" _probe "${_probe}")
  list(GET _probe 0 _var)
  list(GET _probe 1 _def)
  list(GET _probe 2 _code)
  check_cxx_source_compiles("#include <mpi.h>\n${_code};\nint main() { return 0; }" ${_var})
  if(_def MATCHES "^!(.*)")
    if(NOT ${_var})
      list(APPEND _tractor_proto_defs ${CMAKE_MATCH_1})
    endif()
  elseif(${_var})
    list(APPEND _tractor_proto_defs ${_def})
  endif()
endforeach()
unset(CMAKE_TRY_COMPILE_TARGET_TYPE)
unset(CMAKE_REQUIRED_QUIET)
foreach(_def IN LISTS _tractor_proto_defs)
  list(APPEND _cpp_input -D${_def})
endforeach()
if(_tractor_proto_defs)
  message(STATUS "tractor: MPI prototype probes -> ${_tractor_proto_defs}")
else()
  message(STATUS "tractor: MPI prototype probes -> standard prototypes")
endif()
# _STATUS_LO_HI_ picks the MPICH-family status struct layout; OpenMPI uses its
# own (_ucount) - applies to every other TARGET, including transport_layers
if(NOT TRACTOR_TARGET STREQUAL "openmpi")
  list(APPEND TRACTOR_CONTROLS _STATUS_LO_HI_)
endif()

if(_eng_lower STREQUAL "on")
  list(APPEND TRACTOR_CONTROLS _PARSE_MPI_INTERNAL_ )
  # mpi_func.inc's MPI-internal entries (MPIC_*, MPID_Request_*) are under #ifdef _PARSE_MPI_INTERNAL_ too
  list(APPEND _cpp_input -D_PARSE_MPI_INTERNAL_)
  set(_pct on)
else()
  set(_pct off)
endif()

# llama.cpp / ggml pipeline stages and CPU kernels (CLASS_LLAMA, CLASS_GGML_CPU in mpi_func.inc), see the Makefile's
# LLAMA_PARSE: part of the base build when it can be built. LLAMA: AUTO (default), ON (else an error) or OFF. Headers:
# LLAMA_ROOT (source tree: include/ and ggml/include/, or install prefix: include/; default: the contrib/llama.cpp
# submodule if checked out), else the installed ones; the probe
# t_llama_probe.cpp checks that their API is the one of the wrappers.
set(TRACTOR_LLAMA_INCLUDE_DIRS)
set(_parse_llama_flag off)
string(TOUPPER "${LLAMA}" _llama_mode)
if(_llama_mode MATCHES "^(ON|YES|TRUE|1)$")
  set(_llama_mode ON)
elseif(_llama_mode MATCHES "^(OFF|NO|FALSE|0)$")
  set(_llama_mode OFF)
else()
  set(_llama_mode AUTO)
endif()
if(NOT _llama_mode STREQUAL "OFF")
  # LLAMA_ROOT not given: the llama.cpp submodule of contrib (when it is checked out), else the installed headers below
  if(NOT LLAMA_ROOT AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../../contrib/llama.cpp/include/llama.h")
    get_filename_component(LLAMA_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../contrib/llama.cpp" ABSOLUTE)
  endif()
  unset(_llama_h_dir)
  set(_llama_incs)
  if(LLAMA_ROOT)
    if(EXISTS "${LLAMA_ROOT}/include/llama.h")
      set(_llama_h_dir "${LLAMA_ROOT}/include")
      set(_llama_incs "${LLAMA_ROOT}/include")
      if(EXISTS "${LLAMA_ROOT}/ggml/include")
        list(APPEND _llama_incs "${LLAMA_ROOT}/ggml/include")
      endif()
    endif()
  else()
    find_package(PkgConfig QUIET)
    set(_llama_pc_inc "")
    if(PKG_CONFIG_FOUND)
      execute_process(COMMAND ${PKG_CONFIG_EXECUTABLE} --variable=includedir llama OUTPUT_VARIABLE _llama_pc_inc
                      OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
    endif()
    foreach(_d ${_llama_pc_inc} /usr/local/include /usr/include)
      if(NOT _llama_h_dir AND EXISTS "${_d}/llama.h")
        set(_llama_h_dir "${_d}")
        set(_llama_incs "${_d}")
      endif()
    endforeach()
  endif()
  list(REMOVE_ITEM _llama_incs /usr/include) # searched anyway; given explicitly it breaks #include_next of the C++ headers
  set(_llama_ok OFF)
  if(_llama_h_dir)
    include(CheckCXXSourceCompiles)
    file(READ "${CMAKE_CURRENT_SOURCE_DIR}/t_llama_probe.cpp" _llama_probe_src)
    set(CMAKE_REQUIRED_INCLUDES ${_llama_incs})
    set(CMAKE_REQUIRED_FLAGS "")
    if(TRACTOR_COMPILER STREQUAL "intel")
      set(CMAKE_REQUIRED_FLAGS "-std=c++11 -w") # TRACTOR_CXX_MISC of intel, set further down
    endif()
    if(LLAMA_DEPRECATED)
      set(CMAKE_REQUIRED_DEFINITIONS -D_PARSE_LLAMA_DEPRECATED_)
    endif()
    set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY) # compiled only: the probe references llama.cpp's functions
    unset(TRACTOR_LLAMA_PROBE CACHE)
    check_cxx_source_compiles("${_llama_probe_src}" TRACTOR_LLAMA_PROBE)
    unset(CMAKE_TRY_COMPILE_TARGET_TYPE)
    unset(CMAKE_REQUIRED_INCLUDES)
    unset(CMAKE_REQUIRED_FLAGS)
    unset(CMAKE_REQUIRED_DEFINITIONS)
    set(_llama_ok ${TRACTOR_LLAMA_PROBE})
  endif()
  if(_llama_ok)
    set(TRACTOR_LLAMA_INCLUDE_DIRS ${_llama_incs})
    list(APPEND TRACTOR_CONTROLS _T_LLAMA_)
    set(_parse_llama_flag on)
    if(LLAMA_DEPRECATED)
      list(APPEND _cpp_input -D_PARSE_LLAMA_DEPRECATED_)
    endif()
  elseif(_llama_mode STREQUAL "ON")
    message(FATAL_ERROR "tractor: LLAMA=ON but the llama.cpp headers are not usable with ${CMAKE_CXX_COMPILER} (llama.h in '${_llama_h_dir}', LLAMA_ROOT='${LLAMA_ROOT}'): not found, or another llama.cpp API than the wrappers (see t_llama_probe.cpp)")
  endif()
  message(STATUS "tractor: llama.cpp parsing: ${_parse_llama_flag} (LLAMA=${_llama_mode}, llama.h in '${_llama_h_dir}')")
endif()

# CUDA through CUPTI (t_cupti.cpp) and cuBLAS wrappers, see the Makefile's CUPTI_PARSE
# CUPTI: OFF, ON (missing toolkit or failing probe is an error) or AUTO (built when found and the probe compiles);
# unset: AUTO with GPU=ON, OFF otherwise. One of the GPU APIs: a build with it is a GPU build (.gpu in the library name).
# Toolkit: CUDA_ROOT, else NVHPC_CUDA_HOME, CUDA_HOME, CUDA_PATH (environment), /opt/cuda, /usr/local/cuda, /usr; CUPTI in
# extras/CUPTI/{include,lib64} (NVIDIA installers, NVHPC), targets/x86_64-linux/{include,lib} (e.g. Arch) or include,
# lib/<multiarch> (Debian/Ubuntu nvidia-cuda-dev + libcupti-dev; system directories are not added to -I/-rpath).
# The probe compiles the CUDA host headers with the flags of the compiler (-std=c++11 for intel), see the Makefile.
set(_parse_cuda_flag off)
string(TOUPPER "${CUPTI}" _cupti_mode)
if(_cupti_mode STREQUAL "")
  if(GPU OR _class_layers)
    set(_cupti_mode AUTO)
  else()
    set(_cupti_mode OFF)
  endif()
endif()
if(_cupti_mode MATCHES "^(ON|YES|TRUE|1)$")
  set(_cupti_mode ON)
elseif(NOT _cupti_mode STREQUAL "AUTO")
  set(_cupti_mode OFF)
endif()
if(NOT _cupti_mode STREQUAL "OFF")
  set(_cuda_root "${CUDA_ROOT}")
  if(NOT _cuda_root)
    foreach(_d $ENV{NVHPC_CUDA_HOME} $ENV{CUDA_HOME} $ENV{CUDA_PATH} /opt/cuda /usr/local/cuda /usr)
      if(EXISTS "${_d}/include/cuda.h" OR EXISTS "${_d}/targets/x86_64-linux/include/cuda.h")
        set(_cuda_root "${_d}")
        break()
      endif()
    endforeach()
  endif()
  unset(_cuda_inc)
  unset(_cupti_inc)
  unset(_cupti_lib)
  foreach(_d "${_cuda_root}/include" "${_cuda_root}/targets/x86_64-linux/include")
    if(NOT _cuda_inc AND EXISTS "${_d}/cublas_v2.h")
      set(_cuda_inc "${_d}")
    endif()
  endforeach()
  foreach(_d "${_cuda_root}/extras/CUPTI/include" "${_cuda_root}/targets/x86_64-linux/include" "${_cuda_root}/include")
    if(NOT _cupti_inc AND EXISTS "${_d}/cupti.h")
      set(_cupti_inc "${_d}")
    endif()
  endforeach()
  file(GLOB _cuda_multiarch_libs LIST_DIRECTORIES true "${_cuda_root}/lib/*-linux-gnu")
  foreach(_d "${_cuda_root}/extras/CUPTI/lib64" "${_cuda_root}/targets/x86_64-linux/lib" "${_cuda_root}/lib64" ${_cuda_multiarch_libs})
    if(NOT _cupti_lib AND EXISTS "${_d}/libcupti.so")
      set(_cupti_lib "${_d}")
    endif()
  endforeach()
  # no -I/usr/include (breaks #include_next of the C++ library) and no -L/-rpath to the system library directories
  set(_cupti_inc_dirs ${_cuda_inc} ${_cupti_inc})
  list(REMOVE_DUPLICATES _cupti_inc_dirs)
  list(REMOVE_ITEM _cupti_inc_dirs /usr/include)
  if(_cupti_lib MATCHES "^/usr/lib(64)?(/[^/]*-linux-gnu)?$")
    set(_cupti_link cupti)
  else()
    set(_cupti_link -L${_cupti_lib} cupti -Wl,-rpath,${_cupti_lib})
  endif()
  set(_cupti_ok OFF)
  if(_cuda_inc AND _cupti_inc AND _cupti_lib)
    include(CheckCXXSourceCompiles)
    set(CMAKE_REQUIRED_INCLUDES ${_cupti_inc_dirs})
    set(CMAKE_REQUIRED_LIBRARIES ${_cupti_link})
    set(CMAKE_REQUIRED_FLAGS "")
    if(TRACTOR_COMPILER STREQUAL "intel")
      set(CMAKE_REQUIRED_FLAGS "-std=c++11 -w") # TRACTOR_CXX_MISC of intel, set further down
    endif()
    unset(TRACTOR_CUPTI_PROBE CACHE) # the toolkit or the compiler may have changed since the last configuration
    check_cxx_source_compiles("#include <cupti.h>\n#include <cublas_v2.h>\n#include <cublasLt.h>\n#include <cxxabi.h>\nint main() { uint64_t t; return (int)cuptiGetTimestamp(&t); }" TRACTOR_CUPTI_PROBE)
    unset(CMAKE_REQUIRED_INCLUDES)
    unset(CMAKE_REQUIRED_LIBRARIES)
    unset(CMAKE_REQUIRED_FLAGS)
    set(_cupti_ok ${TRACTOR_CUPTI_PROBE})
  endif()
  if(_cupti_ok)
    list(APPEND TRACTOR_LLAMA_INCLUDE_DIRS ${_cupti_inc_dirs})
    list(APPEND TRACTOR_SOURCES t_cupti.cpp)
    list(APPEND TRACTOR_CONTROLS _T_CUPTI_)
    list(APPEND TRACTOR_LIBS ${_cupti_link})
    if(NOT TRACTOR_EXTRANAME MATCHES "\\.gpu" AND NOT _class_layers) # a GPU build: .gpu, first as for OpenCL/Level Zero/OpenACC
      set(TRACTOR_EXTRANAME ".gpu${TRACTOR_EXTRANAME}")
    endif()
    set(_parse_cuda_flag on)
    if(NOT _timer_lower STREQUAL "nano") # CUPTI timestamps are nanoseconds on tractor's clock (cuptiActivityRegisterTimestampCallback)
      message(STATUS "tractor: CUPTI uses PEAT_TIMER=nano (was ${PEAT_TIMER})")
      set(_timer_lower nano)
    endif()
  elseif(_cupti_mode STREQUAL "ON")
    message(FATAL_ERROR "tractor: CUPTI=ON but CUPTI/cuBLAS are not usable with ${CMAKE_CXX_COMPILER} (CUDA_ROOT='${_cuda_root}': cuda/cublas headers in '${_cuda_inc}', cupti.h in '${_cupti_inc}', libcupti in '${_cupti_lib}')")
  endif()
  message(STATUS "tractor: CUDA (CUPTI) parsing: ${_parse_cuda_flag} (CUPTI=${_cupti_mode}, CUDA_ROOT='${_cuda_root}')")
endif()
if(GPU AND NOT _class_layers AND NOT TRACTOR_EXTRANAME MATCHES "\\.gpu")
  message(WARNING "tractor: GPU=ON but no GPU API (OpenCL, Level Zero, OpenACC, CUDA) was found, GPU parsing is off")
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

# OpenACC interception (profiling interface, needs acc_prof.h): part of the GPU parsing, see the detection above
if(_gpu_acc)
  list(APPEND TRACTOR_SOURCES t_acc.cpp)
  list(APPEND TRACTOR_CONTROLS _T_ACC_)
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
elseif(TRACTOR_COMPILER STREQUAL "nvc" OR TRACTOR_COMPILER STREQUAL "pgi")
  # nvc takes the GCC options (it has no OMPT, the OpenMP tools interface is simply unused)
  list(APPEND TRACTOR_CONTROLS _PGICOMPILER_ _GNUCOMPILER_)
  set(TRACTOR_CXX_MISC -Wno-write-strings -fno-omit-frame-pointer)
  set(TRACTOR_OMP_FLAG -mp)
else() # gnu and clang
  list(APPEND TRACTOR_CONTROLS _GNUCOMPILER_)
  # the stack unwinder walks frame pointers: without this flag -O3 code crashes at start
  set(TRACTOR_CXX_MISC -Wno-write-strings -fno-omit-frame-pointer)
  set(TRACTOR_OMP_FLAG -fopenmp)
endif()

if(TRACTOR_TARGET STREQUAL "openmpi")
  list(APPEND TRACTOR_CXX_MISC -fms-extensions -Wno-int-to-pointer-cast) # as the Makefile (openmpi5)
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
    _TRACTOR_DIAG_ _TRACTOR_DEBUG_TRACTOR_LAYER_ENTRY_EXIT_ _TRACTOR_DEBUG_TRANSPORT_LAYER_ENTRY_EXIT_
    _TRACTOR_DEBUG_MPI_LAYER_ENTRY_EXIT_ _TRACTOR_DEBUG_GPU_LAYER_ENTRY_EXIT_ _TRACTOR_DEBUG_LLAMA_LAYER_ENTRY_EXIT_
    _TRACTOR_DEBUG_OMPT_ENTRY_EXIT_ _TRACTOR_DEBUG_ACC_ENTRY_EXIT_ _TRACTOR_DEBUG_COMM_ENTRY_EXIT_)
else()
  set(TRACTOR_DBG_SUFFIX "")
  set(TRACTOR_OPT -O3)
endif()

set(TRACTOR_LIB_SUFFIX "${TRACTOR_DBG_SUFFIX}.${TRACTOR_TARGET}${TRACTOR_EXTRANAME}")
set(TRACTOR_BIN_SUFFIX "${TRACTOR_DBG_SUFFIX}.${TRACTOR_TARGET}")
# transport shims are TARGET-independent (one build works for every MPI, see Makefile) - no TARGET in the suffix
set(TRANSPORTLIBSUFFIX "${TRACTOR_DBG_SUFFIX}${TRACTOR_EXTRANAME}")
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
if(_gpu_apis)
  string(REPLACE ";" "," _parse_gpu_flag "${_gpu_apis}") # mpi_layer.pl -c cl,ze
else()
  set(_parse_gpu_flag off)
endif()
# class_layers: no core mpi_layer.cpp content (-s on), the wrappers of each found class into the source of its shim
set(_class_shim_args)
set(_class_shim_outputs)
if(_class_layers)
  set(_mpi_layer_skip_core on)
  set(_parse_gpu_flag off)
  foreach(_api cl ze)
    if(_gpu_${_api})
      list(APPEND _class_shim_args -C ${_api}=mpi_layer_${_api}.cpp)
      list(APPEND _class_shim_outputs ${CMAKE_CURRENT_BINARY_DIR}/mpi_layer_${_api}.cpp)
    endif()
  endforeach()
  if(_parse_cuda_flag STREQUAL "on")
    list(APPEND _class_shim_args -U mpi_layer_cuda.cpp)
    list(APPEND _class_shim_outputs ${CMAKE_CURRENT_BINARY_DIR}/mpi_layer_cuda.cpp)
  endif()
  if(_parse_llama_flag STREQUAL "on")
    list(APPEND _class_shim_args -G mpi_layer_llama.cpp)
    list(APPEND _class_shim_outputs ${CMAKE_CURRENT_BINARY_DIR}/mpi_layer_llama.cpp)
  endif()
  set(_core_llama_flag off)
  set(_core_cuda_flag off)
else()
  set(_core_llama_flag ${_parse_llama_flag})
  set(_core_cuda_flag ${_parse_cuda_flag})
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
  OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/mpi_layer.cpp ${_transport_shim_outputs} ${_class_shim_outputs}
  COMMAND ${PERL_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/mpi_layer.pl -p on -e ${_print_extended_stuff} -m ${_maxmpi} -t ${_pct} -c ${_parse_gpu_flag} -g ${_core_llama_flag} -u ${_core_cuda_flag}
          -s ${_mpi_layer_skip_core} -i mpi_func.inc -o mpi_layer.cpp ${_transport_func_layers} ${_class_shim_args}
  DEPENDS mpi_layer.cpp.in mpi_layer.cpp.in.end mpi_layer.pl
          ${CMAKE_CURRENT_BINARY_DIR}/mpi_func_ids_through.h
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMENT "tractor: generating mpi_layer.cpp (TARGET=${TRACTOR_TARGET}, TRANSPORT=${TRANSPORT})"
  VERBATIM)

add_custom_target(tractor_codegen DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/mpi_layer.cpp ${_transport_shim_outputs} ${_class_shim_outputs})

# ---------------------------------------------------------------------------
# Targets
# ---------------------------------------------------------------------------
# The shims (transport_layers, class_layers) dlsym() their real symbols at runtime and never call real MPI, so (like the
# Makefile) they are compiled with the MPI compiler (for mpi.h) but *linked* with a plain, non-MPI-wrapped C++ compiler,
# to avoid pulling in an unwanted libmpi dependency.
set(TRACTOR_PLAIN_CXX "$ENV{CXX}" CACHE STRING
  "plain (non-MPI-wrapped) C++ compiler used only to LINK the transport_layers / class_layers shim .so's; defaults to \$ENV{CXX} or CMAKE_CXX_COMPILER")
if(NOT TRACTOR_PLAIN_CXX)
  set(TRACTOR_PLAIN_CXX ${CMAKE_CXX_COMPILER})
endif()

# the version symlinks of an installed shim, as the Makefile's install: rule
function(tractor_install_shim_links _dir _base)
  install(CODE "
    execute_process(COMMAND ${CMAKE_COMMAND} -E create_symlink
      lib${_base}.so.${TRACTOR_MAJOR}.${TRACTOR_MINOR} lib${_base}.so.${TRACTOR_MAJOR}
      WORKING_DIRECTORY \"${_dir}\")
    execute_process(COMMAND ${CMAKE_COMMAND} -E create_symlink
      lib${_base}.so.${TRACTOR_MAJOR} lib${_base}.so
      WORKING_DIRECTORY \"${_dir}\")
  ")
endfunction()

# tractor_class_shim(<name> SOURCES <files> [DEFS <-D...>] [FLAGS <compile flags>] [LINK <link args>]):
# libtractor<suffix>_<name>.so of event-class code built with -D_T_CLASS_SHIM_ (t_class_shim.h); appends the library to
# _class_shim_libs and its base name to _class_shim_bases of the caller
function(tractor_class_shim _name)
  cmake_parse_arguments(S "" "" "SOURCES;DEFS;FLAGS;LINK" ${ARGN})
  set(_objs)
  foreach(_src ${S_SOURCES})
    get_filename_component(_b ${_src} NAME_WE)
    set(_obj ${CMAKE_CURRENT_BINARY_DIR}/${_b}.${_name}_shim.o)
    add_custom_command(
      OUTPUT ${_obj}
      COMMAND ${CMAKE_CXX_COMPILER} -o ${_obj} -c
              -I${CMAKE_CURRENT_SOURCE_DIR} -I${_common_dir} -I${CMAKE_CURRENT_BINARY_DIR}
              ${_tractor_defs} ${S_DEFS} -D_T_CLASS_SHIM_ ${TRACTOR_CXX_MISC} ${TRACTOR_OPT} ${S_FLAGS} -fPIC ${_src}
      DEPENDS ${_src} ${CMAKE_CURRENT_BINARY_DIR}/mpi_func_ids_through.h
              ${CMAKE_CURRENT_SOURCE_DIR}/t_class_shim.h ${CMAKE_CURRENT_SOURCE_DIR}/t_shim_state.h
      WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
      COMMENT "tractor(class_layers): compiling ${_b} for the ${_name} shim"
      VERBATIM)
    list(APPEND _objs ${_obj})
  endforeach()
  set(_base tractor${TRANSPORTLIBSUFFIX}_${_name})
  set(_so ${CMAKE_CURRENT_BINARY_DIR}/lib${_base}.so.${TRACTOR_MAJOR}.${TRACTOR_MINOR})
  add_custom_command(
    OUTPUT ${_so}
    COMMAND ${TRACTOR_PLAIN_CXX} -o ${_so} ${TRACTOR_OPT} -shared -fPIC ${_objs} ${S_LINK} -ldl
    DEPENDS ${_objs}
    COMMENT "tractor(class_layers): linking lib${_base}.so"
    VERBATIM)
  set(_class_shim_libs ${_class_shim_libs} ${_so} PARENT_SCOPE)
  set(_class_shim_bases ${_class_shim_bases} ${_base} PARENT_SCOPE)
endfunction()

if(_class_layers)
  # Standalone: only the event-class shims, no core lib/ptpd. Each class is the code of the core build (t_ompt.cpp,
  # t_acc.cpp, t_cupti.cpp, the generated wrappers), the shims link no runtime they wrap (dlsym RTLD_NEXT), except
  # libcupti for the CUDA one.
  set(_class_shim_libs)
  set(_class_shim_bases)
  if(TRACTOR_COMPILER STREQUAL "intel")
    set(_class_omp_flag -qopenmp)
  else()
    set(_class_omp_flag -fopenmp)
  endif()
  if(TRACTOR_COMPILER STREQUAL "nvc" OR TRACTOR_COMPILER STREQUAL "pgi")
    message(STATUS "tractor: class_layers: no OMPT shim with ${TRACTOR_COMPILER} (it has no OpenMP tools interface)")
  else()
    tractor_class_shim(ompt SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/t_ompt.cpp DEFS -D_T_OMPT_ FLAGS ${_class_omp_flag})
  endif()
  if(_gpu_acc)
    tractor_class_shim(acc SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/t_acc.cpp DEFS -D_T_ACC_)
  elseif(TRACTOR_COMPILER STREQUAL "intel")
    message(STATUS "tractor: class_layers: no OpenACC shim with the Intel compilers (no acc_prof.h), build it with gnu or nvc")
  endif()
  set(_gpu_inc_flags ${TRACTOR_GPU_INCLUDE_DIRS})
  list(TRANSFORM _gpu_inc_flags PREPEND "-I")
  if(_gpu_cl)
    tractor_class_shim(opencl SOURCES ${CMAKE_CURRENT_BINARY_DIR}/mpi_layer_cl.cpp DEFS -DGPUPARSE -DGPUPARSE_CL FLAGS ${_gpu_inc_flags})
  endif()
  if(_gpu_ze)
    tractor_class_shim(levelzero SOURCES ${CMAKE_CURRENT_BINARY_DIR}/mpi_layer_ze.cpp DEFS -DGPUPARSE -DGPUPARSE_ZE FLAGS ${_gpu_inc_flags})
  endif()
  set(_llama_cuda_inc_flags ${TRACTOR_LLAMA_INCLUDE_DIRS}) # the llama.cpp headers and, when found, the CUDA/CUPTI ones
  list(TRANSFORM _llama_cuda_inc_flags PREPEND "-I")
  if(_parse_cuda_flag STREQUAL "on")
    # _cupti_link is for target_link_libraries (a bare "cupti"); the shim is linked by a plain command line
    string(REGEX REPLACE "(^|;)cupti(;|$)" "\\1-lcupti\\2" _cupti_link_flags "${_cupti_link}")
    tractor_class_shim(cuda SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/t_cupti.cpp ${CMAKE_CURRENT_BINARY_DIR}/mpi_layer_cuda.cpp
                       DEFS -D_T_CUPTI_ FLAGS ${_llama_cuda_inc_flags} LINK ${_cupti_link_flags})
  endif()
  if(_parse_llama_flag STREQUAL "on")
    tractor_class_shim(llama SOURCES ${CMAKE_CURRENT_BINARY_DIR}/mpi_layer_llama.cpp DEFS -D_T_LLAMA_ FLAGS ${_llama_cuda_inc_flags})
  endif()
  message(STATUS "tractor: class_layers: ${_class_shim_bases} (timer ${_timer_lower}: the cores they are preloaded with need the same PEAT_TIMER)")

  add_custom_target(tractor_class_layers ALL DEPENDS ${_class_shim_libs})
  add_dependencies(tractor_class_layers tractor_codegen)

  set(_class_install_dir ${CMAKE_CURRENT_SOURCE_DIR}/../../../lib/${TRACTOR_COMPILER})
  install(FILES ${_class_shim_libs} DESTINATION ${_class_install_dir}
          PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)
  foreach(_base ${_class_shim_bases})
    tractor_install_shim_links(${_class_install_dir} ${_base})
  endforeach()
elseif(TRACTOR_TARGET STREQUAL "transport_layers")
  # Standalone: only the 4 psm2/psm3/ucx/ofi shim .so's - no core lib/ptpd.

  set(_transport_shim_libs)

  foreach(_layer psm2 psm3 ucx ofi)
    set(_src ${CMAKE_CURRENT_BINARY_DIR}/mpi_layer_transport_${_layer}.cpp)
    set(_obj ${CMAKE_CURRENT_BINARY_DIR}/mpi_layer_transport_${_layer}.o)
    set(_soname libtractor${TRANSPORTLIBSUFFIX}_transport_${_layer}.so.${TRACTOR_MAJOR}.${TRACTOR_MINOR})
    set(_so ${CMAKE_CURRENT_BINARY_DIR}/${_soname})

    add_custom_command(
      OUTPUT ${_obj}
      COMMAND ${CMAKE_CXX_COMPILER} -o ${_obj} -c
              -I${CMAKE_CURRENT_SOURCE_DIR} -I${_common_dir} -I${CMAKE_CURRENT_BINARY_DIR}
              -I${VERBSPATH}/include $<$<BOOL:${TRACTOR_PSM2_INCLUDE_DIR}>:-I${TRACTOR_PSM2_INCLUDE_DIR}>
              -I${CMAKE_CURRENT_SOURCE_DIR}/../../contrib/psm3/include
              $<$<BOOL:${TRACTOR_UCX_INCLUDE_DIR}>:-I${TRACTOR_UCX_INCLUDE_DIR}> $<$<BOOL:${TRACTOR_OFI_INCLUDE_DIR}>:-I${TRACTOR_OFI_INCLUDE_DIR}>
              ${_tractor_defs} -D_TRANSPORT_PROFILE_ ${TRACTOR_CXX_MISC} ${TRACTOR_OPT} -fPIC ${_src}
      DEPENDS ${_src}
      WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
      COMMENT "tractor(transport_layers): compiling ${_layer} shim"
      VERBATIM)

    add_custom_command(
      OUTPUT ${_so}
      COMMAND ${TRACTOR_PLAIN_CXX} -o ${_so} ${TRACTOR_OPT} -shared -fPIC ${_obj} -ldl
      DEPENDS ${_obj}
      COMMENT "tractor(transport_layers): linking ${_soname}"
      VERBATIM)

    list(APPEND _transport_shim_libs ${_so})
  endforeach()

  add_custom_target(tractor_transport_layers ALL DEPENDS ${_transport_shim_libs})
  add_dependencies(tractor_transport_layers tractor_codegen)

  set(_transport_install_dir ${CMAKE_CURRENT_SOURCE_DIR}/../../../lib/${TRACTOR_COMPILER})
  install(FILES ${_transport_shim_libs} DESTINATION ${_transport_install_dir}
          PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)
  foreach(_layer psm2 psm3 ucx ofi)
    set(_base tractor${TRANSPORTLIBSUFFIX}_transport_${_layer})
    install(CODE "
      execute_process(COMMAND ${CMAKE_COMMAND} -E create_symlink
        lib${_base}.so.${TRACTOR_MAJOR}.${TRACTOR_MINOR} lib${_base}.so.${TRACTOR_MAJOR}
        WORKING_DIRECTORY \"${_transport_install_dir}\")
      execute_process(COMMAND ${CMAKE_COMMAND} -E create_symlink
        lib${_base}.so.${TRACTOR_MAJOR} lib${_base}.so
        WORKING_DIRECTORY \"${_transport_install_dir}\")
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
  target_include_directories(tractor PRIVATE . ${_common_dir} ${hwloc_INCLUDE_DIRS} ${CMAKE_CURRENT_BINARY_DIR} ${CMAKE_CURRENT_SOURCE_DIR} ${TRACTOR_GPU_INCLUDE_DIRS} ${TRACTOR_LLAMA_INCLUDE_DIRS})
  target_link_libraries(tractor PRIVATE ${TRACTOR_LIBS} hwloc::hwloc)
  if(NOT _transport_upper STREQUAL "OFF")
    target_include_directories(tractor PRIVATE
      ${VERBSPATH}/include ${TRACTOR_PSM2_INCLUDE_DIR}
      ${CMAKE_CURRENT_SOURCE_DIR}/../../contrib/psm3/include ${TRACTOR_UCX_INCLUDE_DIR} ${TRACTOR_OFI_INCLUDE_DIR})
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
