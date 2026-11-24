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

#version
MAJOR=0
MINOR=1


#files
# What is built (TARGET):
#   core              the core library, libtractor[_dbg].core[.mem<x>][.<hwc>].so (no MPI: built with $(CXX))
#   <mpi>             the MPI layer of that MPI, libtractor[_dbg].<mpi>[.mbs][.v2p][.eng].so, and its PTPD helper; it
#                     needs a core library: its DT_NEEDED is the core's SONAME libtractor.core.so.$(MAJOR), found next to
#                     it ($ORIGIN) unless a core library variant is preloaded before it
#   class_layers      the event-class shims (OMPT, OpenACC, OpenCL, Level Zero, CUDA, llama.cpp)
#   transport_layers  the transport shims (psm2, psm3, ucx, ofi)
MPI_TARGETS=impi mpich3 mpich4 mpich5 mvapich2 mvapich4 openmpi5
#objects
CORE_OBJECTS=t_base_event.o tractor.o t_stat.o elf_parser.o t_topo.o t_globals.o t_transport.o
MPI_OBJECTS=mpi_layer.o t_comm.o t_mpi.o
FUNCIDS=mpi_func_ids_through.h mpi_func_names.h mpi_func_ids_trans.h
FUNCID=mpi_func_ids_through.h # can pick any of 3 above
#build options
shell=bash
SUFFIX=
PTPD=on
MEMPROFILE=off
TARGET=impi
PARSE_MPI_INTERNAL=off
OVERALLSTATONLY=off
VIRT2PHYS=off
DIAG=off
HWCOUNTERS=off
USE_PRAGMA_WEAK=on #enabled by default
USE_SIGNALS=on
PRINT_EXTENDED_STUFF=off #on# to enable
MAXMPI=3
mpi_layer_skip_core=off
OMPT=on

CPP:=/usr/bin/cpp -E

ifeq "$(TARGET)" "core"
IS_CORE=1
endif
ifneq "$(filter $(MPI_TARGETS),$(TARGET))" ""
IS_MPI=1
endif
# the event classes other than OMPT (OpenCL, Level Zero, OpenACC, CUDA, llama.cpp) come from TARGET=class_layers: they
# are shims, built when their headers are found; the core may have OMPT built in (OMPT=on), the MPI layers have none
ifneq "$(IS_CORE)$(IS_MPI)" ""
override CUPTI_PARSE:=off
override LLAMA_PARSE:=off
endif
ifdef IS_MPI
override OMPT:=off
endif

TRACTOR_BUILD_DATE:="$(shell date)"
#compiler options
OPT=-O3 #-Zp4
FPIC=-fpic
INCLUDE=-I. -I../../common -I$(VERBSPATH)/include -I$(PSM2PATH)/include -I../../contrib/psm3/include -I$(UCXPATH)/include -I$(OFIPATH)/include
LIBS=-lz -lhwloc
PTPD_LIBS=-lz
# HWLOC_PATH is optional: when unset, -I$(HWLOC_PATH)/include and -L$(HWLOC_PATH)/lib
# collapse to -I/include and -L/lib - on merged-usr systems /lib is /usr/lib, so the
# stray -L/lib (ahead of $(MPICXX)'s own -L<mpi>/lib) can make the linker resolve -lmpi
# against whatever other MPI's libmpi.so happens to live in /usr/lib instead of the
# intended one - only add these flags when HWLOC_PATH is actually set.
ifneq "$(HWLOC_PATH)" ""
INCLUDE+=-I$(HWLOC_PATH)/include
LIBS+=-L$(HWLOC_PATH)/lib
endif

TRANSPORT=off

PCT=off

CONTROLS= -D_TRACTOR_COLLECT_STAT_ -D_T_PER_INTERVAL_STAT_WRITE_ -D_USE_COMM_DISAMBIGUATION_ -D_TRACTOR_PCALL_ \
-D_TRACTOR_MAJOR_VERSION_=$(MAJOR) -D_TRACTOR_MINOR_VERSION_=$(MINOR) -D_TRACTOR_BUILD_DATE_=\"$(TRACTOR_BUILD_DATE)\" -DUSE_NANO_TIMER # -D_T_USE_GETTID_INT_ -D_T_SKIP_VOLATILE_ -D_TRACTOR_PASSTHROUGH_ #-D_TRACTOR_STORE_UNIQ_INDEX_ 

ifdef USE_SIGNALS
CONTROLS+=-D_USE_SIGNALS_
CORE_OBJECTS+=t_signals.o
endif

# TARGET=class_layers: OpenCL and Level Zero are each looked for with the compiler that builds the shims (header compiles,
# library links) and built when found (libtractor.opencl.so, libtractor.levelzero.so). OCL_PARSE / LZE_PARSE: auto
# (default) = when found, on = a missing one is an error, off = skip. The OpenACC shim has its own probe (below): the
# OpenACC profiling interface (acc_prof.h: GCC >= 9, NVHPC) does not exist for the Intel compilers.
OCL_PARSE?=auto
LZE_PARSE?=auto
# helpers for the compile probes (GPU below, MPI 4.1 const-ness further down)
HASH:=\#
COMMA:=,
EMPTY:=
SPACE:=$(EMPTY) $(EMPTY)
ifeq "$(TARGET)" "class_layers"
ifeq "$(COMPILER)" "intel"
GPU_PROBE_FLAGS=-I$(CMPLR_ROOT)/include -L$(CMPLR_ROOT)/lib
endif
# $(call gpu_probe,<header>,<link flags>) -> yes|no
gpu_probe=$(shell printf '$(HASH)include <$(1)>\nint main(){return 0;}\n' | $(MPICXX) $(GPU_PROBE_FLAGS) -x c++ - $(2) -o /dev/null >/dev/null 2>&1 && echo yes || echo no)

$(foreach v,OCL_PARSE LZE_PARSE,$(if $(filter auto on off,$($(v))),,$(error $(v)=$($(v)) is not one of auto|on|off)))

GPU_CL=off
ifneq "$(OCL_PARSE)" "off"
ifeq "$(call gpu_probe,CL/cl.h,-lOpenCL)" "yes"
GPU_CL=on
else ifeq "$(OCL_PARSE)" "on"
$(error OCL_PARSE=on but CL/cl.h or libOpenCL was not found (compiler $(MPICXX)))
endif
endif

GPU_ZE=off
ifneq "$(LZE_PARSE)" "off"
ifeq "$(call gpu_probe,level_zero/ze_api.h,-lze_loader)" "yes"
GPU_ZE=on
else ifeq "$(LZE_PARSE)" "on"
$(error LZE_PARSE=on but level_zero/ze_api.h or libze_loader was not found (compiler $(MPICXX)))
endif
endif

$(info tractor: GPU shims: OpenCL=$(GPU_CL) LevelZero=$(GPU_ZE))
# the shims: no link to libOpenCL / libze_loader (the wrappers find the functions with dlsym(RTLD_NEXT))
ifeq "$(GPU_CL)" "on"
CLASS_SHIM_BASENAMES+=tractor$(TRANSPORTLIBSUFFIX).opencl
endif
ifeq "$(GPU_ZE)" "on"
CLASS_SHIM_BASENAMES+=tractor$(TRANSPORTLIBSUFFIX).levelzero
endif
endif

ifndef IS_CORE
# libfabric / MPI edition check (same rule as CMakeLists.txt): the engineering libfabric exports functions the stock one only
# has as "static inline" (fi_close, ...), and the engineering libmpi references them as undefined symbols. The two have to
# agree, and the transport_layers shims (which redefine those functions) only compile against the engineering libfabric.
# libfabric is the system one unless OFIPATH points elsewhere (env.sh: libfabric_eng=-eng).
ifneq "$(shell which nm readelf 2>/dev/null | wc -l)" "2"
$(info tractor: libfabric edition not checked (nm or readelf not found))
else
ifneq "$(OFIPATH)" ""
LIBFABRIC_LIB:=$(firstword $(wildcard $(OFIPATH)/lib/libfabric.so $(OFIPATH)/lib64/libfabric.so))
else
LIBFABRIC_LIB:=$(firstword $(wildcard $(shell $(MPICXX) -print-file-name=libfabric.so 2>/dev/null)))
endif
MPI_BINDIR:=$(dir $(shell which $(MPICXX) 2>/dev/null))
MPI_LIB:=$(firstword $(wildcard $(MPI_BINDIR)../lib/libmpi.so $(MPI_BINDIR)../lib64/libmpi.so))
ifeq "$(LIBFABRIC_LIB)" ""
$(info tractor: libfabric edition not checked (libfabric not found))
else
LIBFABRIC_ENG:=$(if $(filter 0,$(shell nm -D --defined-only $(LIBFABRIC_LIB) | grep -c ' [TtWw] fi_close$$')),stock,engineering)
ifeq "$(TARGET)" "transport_layers"
ifeq "$(LIBFABRIC_ENG)" "stock"
$(error TARGET=transport_layers needs the engineering libfabric, but $(LIBFABRIC_LIB) is a stock build (fi_close is only an inline function there). Point OFIPATH to the engineering libfabric (libfabric_eng=-eng before env.sh))
endif
else ifneq "$(MPI_LIB)" ""
ifneq "$(shell readelf -d $(MPI_LIB) | grep -c 'NEEDED.*libfabric')" "0"
MPI_ENG:=$(if $(filter-out 0,$(shell nm -D --undefined-only $(MPI_LIB) | grep -c ' [Uw] fi_close$$') $(shell nm -D --defined-only $(MPI_LIB) | grep -c ' [TtWw] mpi_internal_comm_create$$')),engineering,stock)
ifneq "$(MPI_ENG)" "$(LIBFABRIC_ENG)"
$(error libfabric version mismatch: the MPI library $(MPI_LIB) is a $(MPI_ENG) build but $(LIBFABRIC_LIB) is a $(LIBFABRIC_ENG) libfabric. Use matching ones (OFIPATH, libfabric_eng=-eng before env.sh))
endif
endif
endif
endif
endif

endif # IS_CORE: no MPI checks for the core

# transport records: always supported by the core (collected with PEAT_TRACTOR_COLLECT_TRANSPORT=1, the wrappers are
# the transport shims), and in the MPI layer of TARGET=mvapich2 (its verbs/smpi/psm2 layers, see below); the former
# TRANSPORT switch is not used any more
ifneq "$(IS_CORE)$(IS_MPI)" ""
CONTROLS+=-D_TRANSPORT_PROFILE_
endif

# TARGET=transport_layers: standalone build of just the transport .so's.
# psm2/psm3/ucx/ofi (genuine dlsym/RTLD_NEXT interception) each get generated into
# their own mpi_layer_transport_<layer>.cpp -> libtractor.transport.<layer>.so; 
# verbs/smpi are NOT here: ibv_* fast-path calls (ibv_post_send, ibv_poll_cq, ...)
# are static inline in real <infiniband/verbs.h> - there's no dynamic symbol to
# dlsym/LD_PRELOAD-intercept at all, so verbs profiling only works when patched
# mvapich2 source calls mv2_proxy_* directly; see TARGET=mvapich2 below.
# No $(TARGET) in these names: the tractor struct layout these shims rely on
# (gt_info(...)) doesn't depend on TARGET, so one set of shims works for every
# MPI target built with the same other flags (TRANSPORT/MEMPROFILE/HWCOUNTERS/etc)
ifeq "$(TARGET)" "transport_layers"
# each shim needs tractor::t_transport_prof/t_transport_layer and the real vendor headers
# (t_transport.h), all gated behind _TRANSPORT_PROFILE_ - normally set by TRANSPORT=on, but that's
# a separate variable from TARGET, so set it here too rather than relying on the
# caller to pass both
CONTROLS+=-D_TRANSPORT_PROFILE_
t_transport_layer_h=t_transport_layer_h.h
#LIBS+=-L$(PSM2PATH)/lib64 -lpsm2 -L$(PSM3PATH)/lib64 -lpsm3 -L$(UCXPATH)/lib -lucp -lucs -L$(OFIPATH)/lib64 -lfabric
transport_func_layers=-l psm2=mpi_layer_transport_psm2.cpp -l psm3=mpi_layer_transport_psm3.cpp -l ucx=mpi_layer_transport_ucx.cpp -l ofi=mpi_layer_transport_ofi.cpp
# no plain (non-shim) layers here, so the core mpi_layer.cpp content is unused - skip generating it
mpi_layer_skip_core=on
TRANSPORT_LAYER_SRCS=mpi_layer_transport_psm2.cpp mpi_layer_transport_psm3.cpp mpi_layer_transport_ucx.cpp mpi_layer_transport_ofi.cpp
TRANSPORT_LAYER_OBJS=mpi_layer_transport_psm2.o mpi_layer_transport_psm3.o mpi_layer_transport_ucx.o mpi_layer_transport_ofi.o
# same lib<name>.so.MAJOR.MINOR + .so.MAJOR/.so symlink scheme as $(MTB)/$(MTBL)
TRANSPORT_SHIM_BASENAMES=tractor$(TRANSPORTLIBSUFFIX).transport.psm2 tractor$(TRANSPORTLIBSUFFIX).transport.psm3 tractor$(TRANSPORTLIBSUFFIX).transport.ucx tractor$(TRANSPORTLIBSUFFIX).transport.ofi
TRANSPORT_SHIM_LIBS=$(foreach b,$(TRANSPORT_SHIM_BASENAMES),lib$(b).so.$(MAJOR).$(MINOR))
endif

# TARGET=class_layers: standalone build of the event-class shims (OMPT, OpenACC, OpenCL, Level Zero, CUDA, llama.cpp, each when
# it can be built: libtractor.<class>.so). Like the transport
# shims they use the core only through t_shared (t_shim_state.h), so they do not depend on TARGET or on the core's
# HWCOUNTERS/MEMPROFILE switches; preload one next to a core built with OMPT=off (a core built with OMPT=on has
# the same code built in, then the shim is not needed). The OpenMP flags only give the compiler's omp-tools.h, the shim
# does not link the OpenMP runtime.
# The OpenACC shim (libtractor.acc.so) needs acc_prof.h (GCC >= 9, NVHPC), so it is built with those compilers only;
# it works with a core of any compiler, e.g. a GCC-built libtractor.acc.so next to an Intel-built core.
ifeq "$(TARGET)" "class_layers"
# += : the OpenCL/Level Zero probes above may have added theirs. No OMPT shim with nvc (no OpenMP tools interface).
ifeq "$(COMPILER)" "nvidia"
$(info tractor: class_layers: no OMPT shim with nvc (it has no OpenMP tools interface))
else
CLASS_SHIM_BASENAMES+=tractor$(TRANSPORTLIBSUFFIX).ompt
endif
ifeq "$(COMPILER)" "intel"
CLASS_SHIM_OMP=-qopenmp
$(info tractor: class_layers: no OpenACC shim with the Intel compilers (no acc_prof.h), build it with COMPILER=gnu or nvidia)
else
CLASS_SHIM_OMP=-fopenmp
ifeq "$(shell printf '$(HASH)include <acc_prof.h>\nint main(){return 0;}\n' | $(MPICXX) -x c++ - -o /dev/null >/dev/null 2>&1 && echo yes || echo no)" "yes"
CLASS_SHIM_BASENAMES+=tractor$(TRANSPORTLIBSUFFIX).acc
else
$(info tractor: class_layers: no OpenACC shim, acc_prof.h not found (compiler $(MPICXX)))
endif
endif
CLASS_SHIM_LIBS=$(foreach b,$(CLASS_SHIM_BASENAMES),lib$(b).so.$(MAJOR).$(MINOR))
endif

# Intel MPI goes by _IMPI_ alone (no _MPICH_): its MPI-internal helpers differ from MPICH 3's
ifeq "$(TARGET)" "impi"
CONTROLS+=-D_IMPI_
cpp_input=-D_IMPI_

CONTROLS+=-D_STATUS_LO_HI_ -D_MPI_MESSAGE_DEFINED_ -D_I_MPI_WEAK_EXPORTS_ -DI_MPI_BUILD 
MAXMPI=4
endif

# MPICH 3.x (MPI 3.1)
ifeq "$(TARGET)" "mpich3"
CONTROLS+=-D_MPICH_=3 
cpp_input=-D_MPICH_=3 

CONTROLS+=-D_STATUS_LO_HI_ -D_MPI_MESSAGE_DEFINED_ 
MAXMPI=3
endif

ifeq "$(TARGET)" "mpich4"
CONTROLS+=-D_MPICH_=4 
cpp_input=-D_MPICH_=4 

CONTROLS+=-D_STATUS_LO_HI_ -D_MPI_MESSAGE_DEFINED_ 
MAXMPI=4
endif

ifeq "$(TARGET)" "mpich5"
CONTROLS+=-D_MPICH_=5 
cpp_input=-D_MPICH_=5 

CONTROLS+=-D_STATUS_LO_HI_ -D_MPI_MESSAGE_DEFINED_ 
MAXMPI=5
endif


ifeq "$(TARGET)" "mvapich2"

CONTROLS+=-D_MPICH_=3 -D_MVAPICH_
cpp_input=-D_MPICH_=3 -D_MVAPICH_
# ibv_* fast-path calls are static inline in real verbs.h (no dynamic symbol to
# dlsym/LD_PRELOAD-intercept), so patched mvapich2 source calls mv2_proxy_ibv_*
# directly instead; verbs+smpi are always invoked unconditionally by that patched
# binary, nothing to pick at runtime, so they're compiled into core (not a shim).
# t_transport.o/t_transport_layer_h already come from TRANSPORT=on above
CONTROLS+=-D_VERBS_PROFILE_
# psm2 is dlsym/RTLD_NEXT-interceptable (real exported symbols, unlike ibv_*), so
# it's compiled in here rather than needing a mv2_proxy_psm2_* source patch; the
# generated check on the runtime T_TRANSPORT_LAYER picks whichever of verbs/smpi/psm2
# actually applies to a given mvapich2 build (ib-verbs vs opa-psm2 channel)
transport_func_layers+=-l verbs -l smpi -l psm2

endif


# _STATUS_LO_HI_ picks the MPICH-family status struct layout (count_hi_and_cancelled);
# OpenMPI uses its own (_ucount, gated by OPEN_MPI from its own mpi.h) - see t_mpidef.h
ifeq "$(filter openmpi%,$(TARGET))" ""
CONTROLS+=-D_STATUS_LO_HI_
endif

CONTROLS+=-D_MPI_MESSAGE_DEFINED_ -D_NO_C_VR
# MAXMPI is the default from the top of the file unless a TARGET block above/below overrides it

ifeq "$(TARGET)" "mvapich4"

CONTROLS+=-D_MPICH_=4 -D_MVAPICH_
cpp_input=-D_MPICH_=4 -D_MVAPICH_

CONTROLS+=-D_STATUS_LO_HI_  -D_MPI_MESSAGE_DEFINED_ -D_NO_C_VR
MAXMPI=4

endif

#LIBS+= -L../../../misc -lmpiif 

ifeq "$(TARGET)" "openmpi5"
CONTROLS+=-D_MPI_MESSAGE_DEFINED_ -D_NO_C_VR -D_OPENMPI_=5  -fms-extensions -Wno-int-to-pointer-cast #-D_TRACTOR_EXTRA_FIELDS_ 
cpp_input=-D_OPENMPI_=5
MAXMPI=3
endif

# engineering build (patched MPI): after the TARGET blocks, which assign cpp_input with '=' - placed before them,
# the mvapich4/openmpi5 blocks dropped -D_PARSE_MPI_INTERNAL_, and mpi_func.inc's MPI-internal entries with it
ifeq "$(PARSE_MPI_INTERNAL)" "on"
CONTROLS+=-D_PARSE_MPI_INTERNAL_ #-D_TRACTOR_PASSTHROUGH_ #-D_TRACTOR_STORE_UNIQ_INDEX_
PCT=on
CT_SUFFIX=.eng
cpp_input+=-D_PARSE_MPI_INTERNAL_
endif

ifndef IS_CORE
# MPI 4.x prototype probes: some MPI 4.x implementations declare a few functions without the standard's
# const (MPICH 4.0: MPI_Psend_init, MPI_Pready_list; MPICH 4.2/MVAPICH 4.1/Intel MPI 2021: MPI_Status_get_*,
# MPI_Request_get_status_*) or lack them (MPICH 4.0: MPI_Status_set_elements_c); mpi_func.inc picks the
# matching entries. Compile-only: C++ rejects the pointer assignment when the prototype differs or is missing.
# Placed after the TARGET blocks, which assign cpp_input with '='.
mpi_proto_probe=$(shell printf '$(HASH)include <mpi.h>\n$(1);\nint main(){return 0;}\n' | $(MPICXX) -x c++ -c - -o /dev/null >/dev/null 2>&1 && echo yes || echo no)
ifeq "$(call mpi_proto_probe,int (*p)(int$(COMMA) MPI_Request*$(COMMA) int*$(COMMA) MPI_Status*) = MPI_Request_get_status_all)" "yes"
cpp_input+=-D_MPI41_REQUEST_NONCONST_
endif
ifeq "$(call mpi_proto_probe,int (*p)(MPI_Status*$(COMMA) int*) = MPI_Status_get_tag)" "yes"
cpp_input+=-D_MPI41_STATUS_NONCONST_
endif
ifeq "$(call mpi_proto_probe,int (*p)(void*$(COMMA) int$(COMMA) MPI_Count$(COMMA) MPI_Datatype$(COMMA) int$(COMMA) int$(COMMA) MPI_Comm$(COMMA) MPI_Info$(COMMA) MPI_Request*) = MPI_Psend_init)" "yes"
cpp_input+=-D_MPI4_PSEND_INIT_NONCONST_
endif
ifeq "$(call mpi_proto_probe,int (*p)(int$(COMMA) int*$(COMMA) MPI_Request) = MPI_Pready_list)" "yes"
cpp_input+=-D_MPI4_PREADY_LIST_NONCONST_
endif
ifeq "$(call mpi_proto_probe,int (*p)(MPI_Status*$(COMMA) MPI_Datatype$(COMMA) MPI_Count) = MPI_Status_set_elements_c)" "no"
cpp_input+=-D_MPI4_NO_STATUS_SET_ELEMENTS_C_
endif

endif # IS_CORE

SHARED=-shared $(FPIC)

NOINLINE=-fno-inline # nvc++ does not know it, see COMPILER=nvidia below
DEBUG=-g # -D_TRACTOR_PASSTHROUGH_
DEBUG_TESTS=-g $(NOINLINE) 

ifeq "$(DIAG)" "on"
DBG=_dbg
DEBUG+= $(NOINLINE) -D_TRACTOR_DIAG_ -D_TRACTOR_DEBUG_TRACTOR_LAYER_ENTRY_EXIT_  -D_TRACTOR_DEBUG_TRANSPORT_LAYER_ENTRY_EXIT_  -D_TRACTOR_DEBUG_MPI_LAYER_ENTRY_EXIT_ -D_TRACTOR_DEBUG_GPU_LAYER_ENTRY_EXIT_ -D_TRACTOR_DEBUG_LLAMA_LAYER_ENTRY_EXIT_ -D_TRACTOR_DEBUG_OMPT_ENTRY_EXIT_ -D_TRACTOR_DEBUG_ACC_ENTRY_EXIT_ -D_TRACTOR_DEBUG_COMM_ENTRY_EXIT_
OPT=-O0
CORE_OBJECTS+=t_debug.o
LIBS+=-lunwind
endif


ifeq "$(COMPILER)" "intel"
#SINCLUDE=-I$(CMPLR_ROOT)/linux/include/sycl #for opencl headers obsolete path ?
SINCLUDE=-I$(CMPLR_ROOT)/include #for opencl headers
CXXMISC=-std=c++11 -fno-omit-frame-pointer -w $(SINCLUDE) # -use-intel-optimized-headers -Wwritable-strings -Wformat 
CMISC=-fno-omit-frame-pointer -w -use-intel-optimized-headers $(SINCLUDE)
CONTROLS+=-D_INTELCOMPILER_
else
COMPILER=gnu #default
CXXMISC=-Wno-write-strings
CMISC=-fno-omit-frame-pointer -x c++ -fpermissive
CONTROLS+=-D_GNUCOMPILER_
endif

ifeq "$(COMPILER)" "nvidia"
# nvc takes the GCC options above, but the C driver rejects "-x c++" (the .c files are C++): build them with nvc++,
# i.e. through the MPI C++ wrapper (MPICH_CXX=nvc++ etc. are set by env.sh for compiler=nvc)
override MPICC=$(MPICXX)
NOINLINE=-Mnoinline
endif

# llama.cpp / ggml pipeline stages and CPU kernels (CLASS_LLAMA, CLASS_GGML_CPU in mpi_func.inc): part of the base build
# like OMPT, when it can be built. LLAMA_PARSE: auto (default: built when llama.cpp's headers are found and the probe
# compiles), on (else an error) or off. The headers: LLAMA_ROOT (a source tree: include/ and ggml/include/, or an install
# prefix: include/; default: the contrib/llama.cpp submodule if checked out), else the installed ones (pkg-config llama, /usr/local/include, /usr/include). The probe,
# t_llama_probe.cpp, takes every wrapped public function with the signature of its wrapper: a llama.cpp with another API
# is left out instead of breaking the wrappers (two take structures by value, so it has to be the llama.cpp the traced
# application is built with). No change of the library name. LLAMA_DEPRECATED=on adds the deprecated llama_decode,
# llama_encode. Placed after the TARGET blocks (they assign cpp_input with '=') and the compiler ones (CXXMISC).
LLAMA_PARSE?=auto
LLAMA_DEPRECATED?=off
LLAMA_ENABLED=off
$(if $(filter auto on off,$(LLAMA_PARSE)),,$(error LLAMA_PARSE=$(LLAMA_PARSE) is not one of auto|on|off))
ifneq "$(LLAMA_PARSE)" "off"
# LLAMA_ROOT not given: the llama.cpp submodule of contrib (when it is checked out), else the installed headers below
ifeq "$(LLAMA_ROOT)" ""
ifneq "$(wildcard ../../contrib/llama.cpp/include/llama.h)" ""
LLAMA_ROOT:=$(abspath ../../contrib/llama.cpp)
endif
endif
ifneq "$(LLAMA_ROOT)" ""
LLAMA_H_DIR:=$(if $(wildcard $(LLAMA_ROOT)/include/llama.h),$(LLAMA_ROOT)/include)
LLAMA_INC:=$(LLAMA_H_DIR) $(if $(LLAMA_H_DIR),$(wildcard $(LLAMA_ROOT)/ggml/include))
else
LLAMA_H_DIR:=$(firstword $(foreach d,$(shell pkg-config --variable=includedir llama 2>/dev/null) /usr/local/include /usr/include,$(if $(wildcard $(d)/llama.h),$(d))))
LLAMA_INC:=$(LLAMA_H_DIR)
endif
# never -I/usr/include: it is searched anyway, and given explicitly it breaks the #include_next of the C++ headers
LLAMA_IFLAGS:=$(addprefix -I,$(filter-out /usr/include,$(LLAMA_INC)))
llama_probe=$(shell $(MPICXX) $(CXXMISC) $(LLAMA_IFLAGS) $(if $(filter on,$(LLAMA_DEPRECATED)),-D_PARSE_LLAMA_DEPRECATED_) \
            -c t_llama_probe.cpp -o /dev/null >/dev/null 2>&1 && echo yes || echo no)
ifeq "$(if $(LLAMA_H_DIR),$(llama_probe),no)" "yes"
LLAMA_ENABLED=on
INCLUDE+=$(LLAMA_IFLAGS)
CONTROLS+=-D_T_LLAMA_
ifeq "$(TARGET)" "class_layers"
# the llama.cpp shim: the llama.cpp / ggml wrappers (mpi_layer_llama.cpp)
CLASS_SHIM_BASENAMES+=tractor$(TRANSPORTLIBSUFFIX).llama
endif
ifeq "$(LLAMA_DEPRECATED)" "on"
cpp_input+=-D_PARSE_LLAMA_DEPRECATED_
endif
else ifeq "$(LLAMA_PARSE)" "on"
$(error LLAMA_PARSE=on but the llama.cpp headers are not usable with $(MPICXX) $(CXXMISC) (llama.h in '$(LLAMA_H_DIR)', LLAMA_ROOT='$(LLAMA_ROOT)'): not found, or another llama.cpp API than the wrappers (see t_llama_probe.cpp))
endif
$(info tractor: llama.cpp parsing: $(LLAMA_ENABLED) (LLAMA_PARSE=$(LLAMA_PARSE), llama.h in '$(LLAMA_H_DIR)'))
endif

# CUDA through CUPTI (t_cupti.cpp) and cuBLAS wrappers (CLASS_CUDA/CLASS_CUBLAS in mpi_func.inc): the CUDA shim of
# TARGET=class_layers. CUPTI_PARSE: auto (default: built when the toolkit is found and the probe compiles, like
# OCL_PARSE/LZE_PARSE), on (a missing toolkit or a failing probe is an error) or off. Only host CUDA headers are
# compiled, so any of the compilers can do it; the probe is run with the compiler's own options (CXXMISC: -std=c++11 for intel), that's why this block
# follows the compiler ones. The toolkit is CUDA_ROOT, else NVHPC_CUDA_HOME (env.sh, compiler=nvc:<cuda version>),
# CUDA_HOME, CUDA_PATH, /opt/cuda, /usr/local/cuda, /usr. CUPTI is looked for in all layouts: extras/CUPTI/{include,lib64}
# (NVIDIA installers, NVHPC), targets/x86_64-linux/{include,lib} (e.g. Arch's cuda) and include, lib/<multiarch>
# (Debian/Ubuntu nvidia-cuda-dev + libcupti-dev, CUDA_ROOT=/usr: system directories are not added to -I/-rpath).
# libcupti has to be the one of the CUDA version the application runs with (found through the rpath set here).
# Recording is off unless PEAT_TRACTOR_PARSE_CUDA=1.
CUPTI_PARSE?=auto
CUPTI_ENABLED=off
$(if $(filter auto on off,$(CUPTI_PARSE)),,$(error CUPTI_PARSE=$(CUPTI_PARSE) is not one of auto|on|off))
ifneq "$(CUPTI_PARSE)" "off"
cuda_root_of=$(if $(wildcard $(1)/include/cuda.h $(1)/targets/x86_64-linux/include/cuda.h),$(1))
CUDA_ROOT?=$(firstword $(foreach d,$(NVHPC_CUDA_HOME) $(CUDA_HOME) $(CUDA_PATH) /opt/cuda /usr/local/cuda /usr,$(call cuda_root_of,$(d))))
CUDA_INC:=$(patsubst %/cublas_v2.h,%,$(firstword $(wildcard $(CUDA_ROOT)/include/cublas_v2.h $(CUDA_ROOT)/targets/x86_64-linux/include/cublas_v2.h)))
CUPTI_INC:=$(patsubst %/cupti.h,%,$(firstword $(wildcard $(CUDA_ROOT)/extras/CUPTI/include/cupti.h $(CUDA_ROOT)/targets/x86_64-linux/include/cupti.h $(CUDA_ROOT)/include/cupti.h)))
CUPTI_LIB:=$(patsubst %/libcupti.so,%,$(firstword $(wildcard $(CUDA_ROOT)/extras/CUPTI/lib64/libcupti.so $(CUDA_ROOT)/targets/x86_64-linux/lib/libcupti.so $(CUDA_ROOT)/lib64/libcupti.so $(CUDA_ROOT)/lib/*-linux-gnu/libcupti.so)))
cuda_sys_dir=$(filter /usr/include /usr/lib/%-linux-gnu /usr/lib /usr/lib64,$(1))
CUPTI_CFLAGS:=$(addprefix -I,$(filter-out $(call cuda_sys_dir,$(CUDA_INC) $(CUPTI_INC)),$(sort $(CUDA_INC) $(CUPTI_INC))))
CUPTI_LDFLAGS:=$(if $(call cuda_sys_dir,$(CUPTI_LIB)),,-L$(CUPTI_LIB) -Wl,-rpath,$(CUPTI_LIB)) -lcupti
cupti_probe=$(shell printf '$(HASH)include <cupti.h>\n$(HASH)include <cublas_v2.h>\n$(HASH)include <cublasLt.h>\n$(HASH)include <cxxabi.h>\nint main(){uint64_t t; return (int)cuptiGetTimestamp(&t);}\n' | \
             $(MPICXX) $(CXXMISC) $(CUPTI_CFLAGS) -x c++ - $(CUPTI_LDFLAGS) -o /dev/null >/dev/null 2>&1 && echo yes || echo no)
ifeq "$(if $(and $(CUDA_INC),$(CUPTI_INC),$(CUPTI_LIB)),$(cupti_probe),no)" "yes"
CUPTI_ENABLED=on
INCLUDE+=$(CUPTI_CFLAGS)
# the CUDA shim: t_cupti.cpp and the cuBLAS wrappers (mpi_layer_cuda.cpp)
CLASS_SHIM_BASENAMES+=tractor$(TRANSPORTLIBSUFFIX).cuda
else ifeq "$(CUPTI_PARSE)" "on"
$(error CUPTI_PARSE=on but CUPTI/cuBLAS are not usable with $(MPICXX) $(CXXMISC) (CUDA_ROOT='$(CUDA_ROOT)': cuda/cublas headers in '$(CUDA_INC)', cupti.h in '$(CUPTI_INC)', libcupti in '$(CUPTI_LIB)'))
endif
$(info tractor: CUDA (CUPTI) parsing: $(CUPTI_ENABLED) (CUPTI_PARSE=$(CUPTI_PARSE), CUDA_ROOT='$(CUDA_ROOT)'))
endif
FMISC= -fno-omit-frame-pointer 

#MISC+=-D_TRACTOR_USE_SIMPLE_PTP_ 
ifeq "$(PTPD)" "on"
CMISC+=-D_TRACTOR_USE_PTPD_ 
endif

# MPIBUFFERS, VIRT2PHYS: buffer addresses in the MPI records (MPI layer; VIRT2PHYS changes how the core prints them too)
ifeq "$(MPIBUFFERS)" "on"
CMISC+= -D_TRACTOR_EXTRA_FIELDS_ 
MPI_EXTRA:=$(MPI_EXTRA).mbs
PRINT_EXTENDED_STUFF=on
endif


ifeq "$(VIRT2PHYS)" "on"
CONTROLS+=-D_TRACTOR_VIRT2PHYS_ -D_TRACTOR_EXTRA_FIELDS_ 
CORE_OBJECTS+=virt2phys.o
MPI_EXTRA:=$(MPI_EXTRA).v2p
CORE_EXTRA:=$(CORE_EXTRA).v2p
PRINT_EXTENDED_STUFF=on
endif

# MEMPROFILE, HWCOUNTERS: samples the core takes (the MPI layer asks for them through t_shared): core switches
ifndef IS_CORE
ifneq "$(MEMPROFILE)$(HWCOUNTERS)" "offoff"
$(info tractor: MEMPROFILE/HWCOUNTERS are switches of TARGET=core, ignored for TARGET=$(TARGET))
override MEMPROFILE:=off
override HWCOUNTERS:=off
endif
endif
ifneq "$(MEMPROFILE)" "off"

MEM_PROFILE=$(shell echo $(MEMPROFILE) | tr a-z A-Z )

CORE_OBJECTS+=memprofile.o #t_alloc.o
CORE_EXTRA:=$(CORE_EXTRA).mem$(MEMPROFILE)
CONTROLS+=-D_MEMPROFILE_ -D_MEMPROFILE_$(MEM_PROFILE)_ -D_TRACTOR_ZERO_ON_ALLOCATE_
ifeq "$(MEM_PROFILE)" "HOOKS"
CONTROLS+=-D_MEM_HOOKS_ -D_MEMPROFILE_STORE_
CORE_OBJECTS+=t_alloc.o
endif
endif

ifeq "$(OMPT)" "on"
CORE_OBJECTS+=t_ompt.o
ifeq "$(COMPILER)" "intel"
OMP+=-qopenmp
else
OMP+=-fopenmp
endif
#EXTRANAMELIB:=$(EXTRANAMELIB).ompt
CONTROLS+=-D_T_OMPT_
endif

ifneq "$(HWCOUNTERS)" "off"
HW_COUNTERS=$(shell echo $(HWCOUNTERS) | tr a-z A-Z )
CONTROLS+=-D_HWCOUNTERS_ENABLED_ -D_HWCOUNTERS_$(HW_COUNTERS)_
CORE_OBJECTS+=hwcounter.o
CORE_EXTRA:=$(CORE_EXTRA).$(HWCOUNTERS)
endif


# the MPI layer as the trace's "Modules" remark names it (t_mpi.cpp): .<mpi>[.mbs][.v2p][.eng]
LIBSUFFIX=$(SUFFIX)$(DBG).$(TARGET)$(MPI_EXTRA)$(CT_SUFFIX)
# transport and class shims are TARGET-independent and never get CT_SUFFIX either, since PARSE_MPI_INTERNAL doesn't
# affect them
TRANSPORTLIBSUFFIX=$(SUFFIX)$(DBG)
BINSUFFIX=$(SUFFIX)$(DBG).$(TARGET)
# the core: one SONAME for all its variants, so that an MPI layer (DT_NEEDED) takes whichever variant is preloaded
CORE_LIBNAME=tractor$(SUFFIX)$(DBG).core$(CORE_EXTRA)
CORE_LIB=lib$(CORE_LIBNAME).so.$(MAJOR).$(MINOR)
CORE_SONAME=libtractor.core.so.$(MAJOR)
# the MPI layer: libtractor[_dbg].<mpi>[.mbs][.v2p][.eng].so
MPILAYER_LIBNAME=tractor$(SUFFIX)$(DBG).$(TARGET)$(MPI_EXTRA)$(CT_SUFFIX)
MPILAYER_LIB=lib$(MPILAYER_LIBNAME).so.$(MAJOR).$(MINOR)
PTPDL=ptpd$(BINSUFFIX)
PTPD=$(PTPDL).$(MAJOR).$(MINOR)

ifdef IS_MPI
# the MPI layer reaches the core through t_shared only (t_mpi_event.h)
CONTROLS+=-D_T_MPI_LAYER_
endif
ifdef IS_CORE
# no MPI in the core: the plain compilers (env.sh: CXX, CC), not the MPI wrappers
override MPICXX:=$(CXX)
override MPICC:=$(CC)
endif

CMISC+=-D_TRACTOR_LIB_SUFFIX_=\"$(LIBSUFFIX)\" -D_TRACTOR_BIN_SUFFIX_=\"$(BINSUFFIX)\"   -D_TRACTOR_ZERO_ON_ALLOCATE_ #-D_TRACTOR_PASSTHROUGH_

ifeq "$(TARGET)" "transport_layers"
# standalone: only the transport shims, no core lib/ptpd built here
all: $(TRANSPORT_SHIM_LIBS)

install: $(TRANSPORT_SHIM_LIBS)
	(mkdir -p ../../../lib/$(COMPILER); chmod o+rx $(TRANSPORT_SHIM_LIBS); cp -v $(TRANSPORT_SHIM_LIBS) ../../../lib/$(COMPILER); \
	cd ../../../lib/$(COMPILER); for b in $(TRANSPORT_SHIM_BASENAMES); do ln -sf lib$$b.so.$(MAJOR).$(MINOR) lib$$b.so.$(MAJOR); ln -sf lib$$b.so.$(MAJOR) lib$$b.so; done )
else ifeq "$(TARGET)" "class_layers"
# standalone: only the event-class shims, no core lib/ptpd built here
all: $(CLASS_SHIM_LIBS)

install: $(CLASS_SHIM_LIBS)
	(mkdir -p ../../../lib/$(COMPILER); chmod o+rx $(CLASS_SHIM_LIBS); cp -v $(CLASS_SHIM_LIBS) ../../../lib/$(COMPILER); \
	cd ../../../lib/$(COMPILER); for b in $(CLASS_SHIM_BASENAMES); do ln -sf lib$$b.so.$(MAJOR).$(MINOR) lib$$b.so.$(MAJOR); ln -sf lib$$b.so.$(MAJOR) lib$$b.so; done )
else ifdef IS_CORE
all: $(CORE_LIB)

install: $(CORE_LIB)
	(mkdir -p ../../../lib/$(COMPILER); chmod o+rx $(CORE_LIB); cp -v $(CORE_LIB) ../../../lib/$(COMPILER); \
	cd ../../../lib/$(COMPILER); ln -sf $(CORE_LIB) lib$(CORE_LIBNAME).so.$(MAJOR); ln -sf lib$(CORE_LIBNAME).so.$(MAJOR) lib$(CORE_LIBNAME).so )
else ifdef IS_MPI
all: $(MPILAYER_LIB) $(PTPD)

install: $(MPILAYER_LIB) $(PTPD)
	(mkdir -p ../../../lib/$(COMPILER); chmod o+rx $(MPILAYER_LIB); cp -v $(MPILAYER_LIB) ../../../lib/$(COMPILER); mkdir -p ../../../bin/$(COMPILER); cp -v $(PTPD) ../../../bin/$(COMPILER);\
	cd ../../../lib/$(COMPILER); ln -sf $(MPILAYER_LIB) lib$(MPILAYER_LIBNAME).so.$(MAJOR); ln -sf lib$(MPILAYER_LIBNAME).so.$(MAJOR) lib$(MPILAYER_LIBNAME).so; \
	cd ../../bin/$(COMPILER); \
	ln -sf $(PTPD) $(PTPDL) )
else
$(error TARGET=$(TARGET): one of core, $(MPI_TARGETS), class_layers, transport_layers)
endif

ifdef IS_CORE
$(CORE_LIB): $(CORE_OBJECTS)
	$(CXX) -o $@ $(DEBUG) $(OPT) $(OMP) $(LDOPTMT) $(SHARED) -Wl,-soname,$(CORE_SONAME) $(CORE_OBJECTS) $(LIBS)
endif

# the MPI layer needs the core: linked against a stub that has the core's SONAME only (DT_NEEDED libtractor.core.so.0;
# the core's symbols are resolved at load time), found next to the MPI layer ($ORIGIN) or preloaded before it
ifdef IS_MPI
core_stub/$(CORE_SONAME):
	mkdir -p core_stub && printf 'extern "C" void t_core_stub(void) {}\n' | $(CXX) -x c++ - $(SHARED) -Wl,-soname,$(CORE_SONAME) -o $@

$(MPILAYER_LIB): $(MPI_OBJECTS) core_stub/$(CORE_SONAME)
	$(MPICXX) -o $@ $(DEBUG) $(OPT) $(LDOPTMT) $(SHARED) -Wl,-soname,lib$(MPILAYER_LIBNAME).so.$(MAJOR) $(MPI_OBJECTS) \
	    -Wl,--no-as-needed -Lcore_stub -l:$(CORE_SONAME) -Wl,-rpath,'$$ORIGIN' -lz
endif

# transport shims dlsym() their real symbols at runtime and never call real MPI, so they're
# linked with plain CXX (CXX comes from env.sh; no MPI runtime dependency) and don't
# link against any specific core .so - gt_info(...)/t_info resolve at load time
# against whatever core lib is loaded alongside them in the target process
libtractor$(TRANSPORTLIBSUFFIX).transport.%.so.$(MAJOR).$(MINOR): mpi_layer_transport_%.o
	$(CXX) -o $@ $(DEBUG) $(OPT) $(SHARED) $< -ldl

# event-class shims: linked with plain CXX like the transport shims (no MPI or OpenMP runtime dependency); t_shared
# resolves at load time against the core library loaded alongside
libtractor$(TRANSPORTLIBSUFFIX).ompt.so.$(MAJOR).$(MINOR): t_ompt_shim.o
	$(CXX) -o $@ $(DEBUG) $(OPT) $(SHARED) $< -ldl

t_ompt_shim.o: t_ompt.cpp $(FUNCID)
	$(MPICXX) -o $@ -c $(INCLUDE) $(CONTROLS) -D_T_OMPT_ -D_T_CLASS_SHIM_ $(CMISC) $(CXXMISC) $(DEBUG) $(OPT) $(CLASS_SHIM_OMP) $(FPIC) t_ompt.cpp

libtractor$(TRANSPORTLIBSUFFIX).acc.so.$(MAJOR).$(MINOR): t_acc_shim.o
	$(CXX) -o $@ $(DEBUG) $(OPT) $(SHARED) $< -ldl

t_acc_shim.o: t_acc.cpp $(FUNCID)
	$(MPICXX) -o $@ -c $(INCLUDE) $(CONTROLS) -D_T_ACC_ -D_T_CLASS_SHIM_ $(CMISC) $(CXXMISC) $(DEBUG) $(OPT) $(FPIC) t_acc.cpp

# CUDA shim: links libcupti (rpath to the toolkit's), the cuBLAS calls are found with dlsym(RTLD_NEXT)
libtractor$(TRANSPORTLIBSUFFIX).cuda.so.$(MAJOR).$(MINOR): t_cupti_shim.o mpi_layer_cuda.o
	$(CXX) -o $@ $(DEBUG) $(OPT) $(SHARED) t_cupti_shim.o mpi_layer_cuda.o $(CUPTI_LDFLAGS) -ldl

t_cupti_shim.o: t_cupti.cpp $(FUNCID)
	$(MPICXX) -o $@ -c $(INCLUDE) $(CONTROLS) -D_T_CUPTI_ -D_T_CLASS_SHIM_ $(CMISC) $(CXXMISC) $(DEBUG) $(OPT) $(FPIC) t_cupti.cpp

# the cuBLAS wrappers alone (-U); -s on: no core mpi_layer.cpp content (it goes to a scratch file, never mpi_layer.cpp)
mpi_layer_cuda.cpp: mpi_layer.cpp.in $(FUNCID)
	./mpi_layer.pl -p $(USE_PRAGMA_WEAK) -e $(PRINT_EXTENDED_STUFF) -m $(MAXMPI) -t $(PCT) -s on -U mpi_layer_cuda.cpp -i mpi_func.inc -o mpi_layer_cuda.unused

# OpenCL / Level Zero shims: one API each, the wrappers find the functions with dlsym(RTLD_NEXT), no link to the runtimes
libtractor$(TRANSPORTLIBSUFFIX).opencl.so.$(MAJOR).$(MINOR): mpi_layer_cl.o
	$(CXX) -o $@ $(DEBUG) $(OPT) $(SHARED) mpi_layer_cl.o -ldl

libtractor$(TRANSPORTLIBSUFFIX).levelzero.so.$(MAJOR).$(MINOR): mpi_layer_ze.o
	$(CXX) -o $@ $(DEBUG) $(OPT) $(SHARED) mpi_layer_ze.o -ldl

mpi_layer_cl.cpp mpi_layer_ze.cpp: mpi_layer.cpp.in $(FUNCID)
	./mpi_layer.pl -p $(USE_PRAGMA_WEAK) -e $(PRINT_EXTENDED_STUFF) -m $(MAXMPI) -t $(PCT) -s on -C cl=mpi_layer_cl.cpp -C ze=mpi_layer_ze.cpp -i mpi_func.inc -o mpi_layer_gpu.unused

mpi_layer_cl.o: mpi_layer_cl.cpp
	$(MPICXX) -o $@ -c $(INCLUDE) $(CONTROLS) -DGPUPARSE -DGPUPARSE_CL -D_T_CLASS_SHIM_ $(GPU_PROBE_FLAGS) $(CMISC) $(CXXMISC) $(DEBUG) $(OPT) $(FPIC) mpi_layer_cl.cpp

mpi_layer_ze.o: mpi_layer_ze.cpp
	$(MPICXX) -o $@ -c $(INCLUDE) $(CONTROLS) -DGPUPARSE -DGPUPARSE_ZE -D_T_CLASS_SHIM_ $(GPU_PROBE_FLAGS) $(CMISC) $(CXXMISC) $(DEBUG) $(OPT) $(FPIC) mpi_layer_ze.cpp

# llama.cpp shim: the wrappers find the llama.cpp/ggml functions with dlsym(RTLD_NEXT), no link to llama.cpp
libtractor$(TRANSPORTLIBSUFFIX).llama.so.$(MAJOR).$(MINOR): mpi_layer_llama.o
	$(CXX) -o $@ $(DEBUG) $(OPT) $(SHARED) mpi_layer_llama.o -ldl

mpi_layer_llama.cpp: mpi_layer.cpp.in $(FUNCID)
	./mpi_layer.pl -p $(USE_PRAGMA_WEAK) -e $(PRINT_EXTENDED_STUFF) -m $(MAXMPI) -t $(PCT) -s on -G mpi_layer_llama.cpp -i mpi_func.inc -o mpi_layer_llama.unused

mpi_layer_llama.o: mpi_layer_llama.cpp
	$(MPICXX) -o $@ -c $(INCLUDE) $(CONTROLS) -D_T_LLAMA_ -D_T_CLASS_SHIM_ $(CMISC) $(CXXMISC) $(DEBUG) $(OPT) $(FPIC) mpi_layer_llama.cpp

mpi_layer_cuda.o: mpi_layer_cuda.cpp
	$(MPICXX) -o $@ -c $(INCLUDE) $(CONTROLS) -D_T_CUPTI_ -D_T_CLASS_SHIM_ $(CMISC) $(CXXMISC) $(DEBUG) $(OPT) $(FPIC) mpi_layer_cuda.cpp

mpi_layer.o: mpi_layer.cpp

mpi_func.inc: ../../common/mpi_func.inc ../../common/mpi_func_ids.h
	$(CPP) $(cpp_input) ../../common/mpi_func.inc > mpi_func.inc

$(FUNCID): mpi_func.inc ../../common/mpi_func_ids.h
	../../common/mpi_func_ids.pl -i ../../common/mpi_func_ids.h mpi_func.inc -o mpi_func_ids_through.h mpi_func_names.h mpi_func_ids_trans.h  

mpi_layer.cpp: mpi_layer.cpp.in mpi_layer.cpp.in.end $(FUNCID)
	./mpi_layer.pl -p $(USE_PRAGMA_WEAK) -e $(PRINT_EXTENDED_STUFF) -m $(MAXMPI) -t $(PCT) -g $(LLAMA_ENABLED) -u $(CUPTI_ENABLED) -s $(mpi_layer_skip_core) -i mpi_func.inc -o mpi_layer.cpp $(transport_func_layers)

# transport_func_layers with "=outfile" makes mpi_layer.cpp's recipe also emit these
# as a side effect; own recipe (not merged suffix-rule) avoids a -j race where
# the shim .so link could start before the .o was actually compiled
mpi_layer_transport_%.o: mpi_layer.cpp
	$(MPICXX) -o $@ -c $(INCLUDE) $(CONTROLS) $(CMISC) $(CXXMISC) $(DEBUG) $(OPT) $(OMP) $(FPIC) mpi_layer_transport_$*.cpp

$(CORE_OBJECTS) $(MPI_OBJECTS):   $(FUNCID)

$(PTPD): $(MPILAYER_LIB)
	$(MPICXX) -o $(PTPD) $(INCLUDE) $(CONTROLS) $(OPT) $(OMP) $(LDOPTMT) $(CXXMISC) src-ptpd.cpp t_globals.cpp $(DEBUG) $(PTPD_LIBS)

.c.o:
	$(MPICC) -o $@ -c $(INCLUDE) $(CONTROLS) $(CMISC) $(DEBUG) $(OPT) $(OMP) $(FPIC) $*.c
#	$(MPICC) -o $@.s -s $(INCLUDE) $(CONTROLS) $(CMISC) $(DEBUG) $(OPT) $(OMP) $(FPIC) $*.c

.cpp.o:
	$(MPICXX) -o $@ -c $(INCLUDE) $(CONTROLS) $(CMISC) $(CXXMISC) $(DEBUG) $(OPT) $(OMP) $(FPIC) $*.cpp
#	$(MPICXX) -o $@.pp -E $(INCLUDE) $(CONTROLS) $(CMISC) $(CXXMISC) $(DEBUG) $(OPT) $(OMP) $(FPIC) $*.cpp
#	$(MPICXX) -S $(INCLUDE) $(CONTROLS) $(CMISC) $(CXXMISC) $(DEBUG) $(OPT) $(OMP) $(FPIC) $*.cpp

.f.o:   
	$(MPIFC) -o $@ -c $(INCLUDE) $(CONTROLS) $(FMISC) $(DEBUG) $(OPT) $(OMP) $(FPIC) $*.f

# mpi_func.inc and mpi_layer.cpp too: they depend on TARGET/PARSE_MPI_INTERNAL/TRANSPORT, which make cannot see
clean:
	rm -rf mpi_func_ids_through.h mpi_func_names.h mpi_func_ids_trans.h $(CORE_OBJECTS) $(MPI_OBJECTS) $(CORE_LIB) $(MPILAYER_LIB) core_stub $(PTPD) $(PTPDL) mpi_layer.c  \
        mpi_func.inc mpi_layer.cpp \
        t_transport_layer.h t_transport_layer_const.h t_transport_layer_h.h \
        $(TRANSPORT_LAYER_SRCS) $(TRANSPORT_LAYER_OBJS) $(TRANSPORT_SHIM_LIBS) t_ompt_shim.o t_acc_shim.o t_cupti_shim.o mpi_layer_cuda.o mpi_layer_cuda.cpp mpi_layer_cuda.unused mpi_layer_llama.o mpi_layer_llama.cpp mpi_layer_llama.unused \
        mpi_layer_cl.o mpi_layer_cl.cpp mpi_layer_ze.o mpi_layer_ze.cpp mpi_layer_gpu.unused $(CLASS_SHIM_LIBS)

