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
MAJOR=1
MINOR=0


#files
#objects
OBJECTS=t_base_event.o tractor.o t_stat.o t_comm.o elf_parser.o t_topo.o mpi_layer.o t_globals.o
FUNCIDS=mpi_func_ids_through.h mpi_func_names.h mpi_func_ids_trans.h
FUNCID=mpi_func_ids_through.h # can pick any of 3 above
#build options
shell=bash
SUFFIX=
PTPD=on
MEMPROFILE=off
TARGET=impi
PARSE_MPI_INTERNAL=off
PARSE_GPU=off
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
OBJECTS+=t_signals.o
endif

ifeq "$(PARSE_GPU)" "on"
LIBS+=-lOpenCL -lze_loader
CONTROLS+=-DGPUPARSE
EXTRANAMELIB:=$(EXTRANAMELIB).gpu
endif

# TRANSPORT=on enables core-side transport support: profiling struct/T_TRANSPORT_LAYER parsing,
# t_collect_transport event collection, and (for TARGET=mvapich2) verbs/smpi proxy calls,
# which are invoked unconditionally by that patched binary so they stay in core too.
ifeq "$(TRANSPORT)" "on"
CONTROLS+=-D_TRANSPORT_PROFILE_ 
OBJECTS+=t_transport.o
t_transport_layer_h=t_transport_layer_h.h
#EXTRANAMELIB:=$(EXTRANAMELIB).transport
endif

# TARGET=transport_layers: standalone build of just the transport .so's.
# psm2/psm3/ucx/ofi (genuine dlsym/RTLD_NEXT interception) each get generated into
# their own mpi_layer_transport_<layer>.cpp -> libtractor_transport_<layer>.so; 
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
TRANSPORT_SHIM_BASENAMES=tractor$(TRANSPORTLIBSUFFIX)_transport_psm2 tractor$(TRANSPORTLIBSUFFIX)_transport_psm3 tractor$(TRANSPORTLIBSUFFIX)_transport_ucx tractor$(TRANSPORTLIBSUFFIX)_transport_ofi
TRANSPORT_SHIM_LIBS=$(foreach b,$(TRANSPORT_SHIM_BASENAMES),lib$(b).so.$(MAJOR).$(MINOR))
endif

ifeq "$(TARGET)" "impi"
CONTROLS+=-D_MPICH_=3 -D_IMPI_
cpp_input=-D_MPICH_=3 -D_IMPI_

CONTROLS+=-D_STATUS_LO_HI_ -D_MPI_MESSAGE_DEFINED_ -D_USE_PROTORYPES_WITH_CONST_ -D_I_MPI_WEAK_EXPORTS_ -DI_MPI_BUILD 
MAXMPI=4
endif

ifeq "$(TARGET)" "mpich4"
CONTROLS+=-D_MPICH_=4 
cpp_input=-D_MPICH_=4 

CONTROLS+=-D_STATUS_LO_HI_ -D_MPI_MESSAGE_DEFINED_ -D_USE_PROTORYPES_WITH_CONST_ 
MAXMPI=4
endif

ifeq "$(TARGET)" "mpich5"
CONTROLS+=-D_MPICH_=5 
cpp_input=-D_MPICH_=5 

CONTROLS+=-D_STATUS_LO_HI_ -D_MPI_MESSAGE_DEFINED_ -D_USE_PROTORYPES_WITH_CONST_ 
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


# legacy single-layer-only builds, kept for reference (never hit by current Build*.sh)
ifeq "$(TRANSPORT)" "PSM2"
CONTROLS+=-D_PSM2_PROFILE_ 
#LIBS+=-L$(PSM2PATH)/lib64 -lpsm2
OBJECTS+=t_transport.o 
t_transport_layer_h=t_transport_layer_h.h
transport_layer=psm2
#transport_func_layers=-l psm2
EXTRANAMELIB:=$(EXTRANAMELIB).psm2
endif


ifeq "$(TRANSPORT)" "VERBS"
CONTROLS+= -D_VERBS_PROFILE_
#LIBS+=-L$(VERBSPATH)/lib64 -libverbs
OBJECTS+=t_transport.o 
t_transport_layer_h=t_transport_layer_h.h
transport_layer=verbs
transport_func_layers=-l verbs -l smpi
EXTRANAMELIB:=$(EXTRANAMELIB).verbs

endif

ifeq "$(TRANSPORT)" "PSM3"
CONTROLS+=-D_PSM3_PROFILE_ 
#LIBS+=-L$(PSM3PATH)/lib64 -lpsm3
OBJECTS+=t_transport.o 
t_transport_layer_h=t_transport_layer_h.h
transport_layer=psm3
transport_func_layers=-l psm3
EXTRANAMELIB:=$(EXTRANAMELIB).psm3
endif

ifeq "$(TRANSPORT)" "UCX"
CONTROLS+=-D_UCX_PROFILE_ 
#LIBS+=-L$(UCXPATH)/lib -lucp -lucs
OBJECTS+=t_transport.o 
t_transport_layer_h=t_transport_layer_h.h
transport_layer=ucx
transport_func_layers=-l ucx
EXTRANAMELIB:=$(EXTRANAMELIB).ucx
endif

ifeq "$(TRANSPORT)" "OFI"
CONTROLS+=-D_OFI_PROFILE_ 
#LIBS+=-L$(OFIPATH)/lib64 -lfabric
OBJECTS+=t_transport.o 
t_transport_layer_h=t_transport_layer_h.h
transport_layer=ofi
transport_func_layers=-l ofi
EXTRANAMELIB:=$(EXTRANAMELIB).ofi
endif

# _STATUS_LO_HI_ picks the MPICH-family status struct layout (count_hi_and_cancelled);
# OpenMPI uses its own (_ucount, gated by OPEN_MPI from its own mpi.h) - see t_mpidef.h
ifeq "$(filter openmpi%,$(TARGET))" ""
CONTROLS+=-D_STATUS_LO_HI_
endif

CONTROLS+=-D_MPI_MESSAGE_DEFINED_ -D_USE_PROTORYPES_WITH_CONST_ -D_NO_C_VR
# MAXMPI is the default from the top of the file unless a TARGET block above/below overrides it

ifeq "$(PARSE_MPI_INTERNAL)" "on" 
CONTROLS+=-D_PARSE_MPI_INTERNAL_ #-D_TRACTOR_PASSTHROUGH_ #-D_TRACTOR_STORE_UNIQ_INDEX_
PCT=on
CT_SUFFIX=.eng
cpp_input+=-D_PARSE_MPI_INTERNAL_
endif

ifeq "$(TARGET)" "mvapich4"

CONTROLS+=-D_MPICH_=4 -D_MVAPICH_
cpp_input=-D_MPICH_=4 -D_MVAPICH_

CONTROLS+=-D_STATUS_LO_HI_  -D_MPI_MESSAGE_DEFINED_ -D_USE_PROTORYPES_WITH_CONST_ -D_NO_C_VR
MAXMPI=4

endif

#LIBS+= -L../../../misc -lmpiif 

ifeq "$(TARGET)" "openmpi5"
CONTROLS+=-D_USE_PROTORYPES_WITH_CONST_ -D_MPI_MESSAGE_DEFINED_ -D_NO_C_VR -D_OPENMPI_=5  -fms-extensions -Wno-int-to-pointer-cast #-D_TRACTOR_EXTRA_FIELDS_ 
cpp_input=-D_OPENMPI_=5
MAXMPI=3
endif

SHARED=-shared $(FPIC)

DEBUG=-g # -D_TRACTOR_PASSTHROUGH_
DEBUG_TESTS=-g -fno-inline 

ifeq "$(DIAG)" "on"
DBG=_dbg
DEBUG+= -fno-inline -D_TRACTOR_DIAG_ -D_TRACTOR_DEBUG_TRACTOR_LAYER_ENTRY_EXIT_  -D_TRACTOR_DEBUG_TRANSPORT_LAYER_ENTRY_EXIT_  -D_TRACTOR_DEBUG_MPI_LAYER_ENTRY_EXIT_ -D_TRACTOR_DEBUG_GPU_LAYER_ENTRY_EXIT_ -D_TRACTOR_DEBUG_OMPT_ENTRY_EXIT_  -D_TRACTOR_DEBUG_COMM_ENTRY_EXIT_ 
OPT=-O0
OBJECTS+=t_debug.o
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


FMISC= -fno-omit-frame-pointer 

#MISC+=-D_TRACTOR_USE_SIMPLE_PTP_ 
ifeq "$(PTPD)" "on"
CMISC+=-D_TRACTOR_USE_PTPD_ 
endif

ifeq "$(MPIBUFFERS)" "on"
CMISC+= -D_TRACTOR_EXTRA_FIELDS_ 
EXTRANAMELIB:=$(EXTRANAMELIB).mbs
PRINT_EXTENDED_STUFF=on
endif


ifeq "$(VIRT2PHYS)" "on"
CONTROLS+=-D_TRACTOR_VIRT2PHYS_ -D_TRACTOR_EXTRA_FIELDS_ 
OBJECTS+=virt2phys.o
EXTRANAMELIB:=$(EXTRANAMELIB).v2p
PRINT_EXTENDED_STUFF=on
endif

ifneq "$(MEMPROFILE)" "off"

MEM_PROFILE=$(shell echo $(MEMPROFILE) | tr a-z A-Z )

OBJECTS+=memprofile.o #t_alloc.o
EXTRANAMELIB:=$(EXTRANAMELIB).mem$(MEMPROFILE)
CONTROLS+=-D_MEMPROFILE_ -D_MEMPROFILE_$(MEM_PROFILE)_ -D_TRACTOR_ZERO_ON_ALLOCATE_
ifeq "$(MEM_PROFILE)" "HOOKS"
CONTROLS+=-D_MEM_HOOKS_ -D_MEMPROFILE_STORE_
OBJECTS+=t_alloc.o
endif
endif

ifeq "$(OMPT)" "on"
OBJECTS+=t_ompt.o
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
OBJECTS+=hwcounter.o
EXTRANAMELIB:=$(EXTRANAMELIB).$(HWCOUNTERS)
endif


LIBSUFFIX=$(SUFFIX)$(DBG).$(TARGET)$(EXTRANAMELIB)$(CT_SUFFIX)
# transport shims are TARGET-independent (see TRANSPORT=on block above) and never get
# CT_SUFFIX either, since PARSE_MPI_INTERNAL doesn't affect the transport wrapper functions
TRANSPORTLIBSUFFIX=$(SUFFIX)$(DBG)$(EXTRANAMELIB)
BINSUFFIX=$(SUFFIX)$(DBG).$(TARGET)
MTBL=tractor$(LIBSUFFIX)
MTB=lib$(MTBL).so.$(MAJOR).$(MINOR)
PTPDL=ptpd$(BINSUFFIX)
PTPD=$(PTPDL).$(MAJOR).$(MINOR)

CMISC+=-D_TRACTOR_LIB_SUFFIX_=\"$(LIBSUFFIX)\" -D_TRACTOR_BIN_SUFFIX_=\"$(BINSUFFIX)\"   -D_TRACTOR_ZERO_ON_ALLOCATE_ #-D_TRACTOR_PASSTHROUGH_

ifeq "$(TARGET)" "transport_layers"
# standalone: only the transport shims, no core lib/ptpd built here
all: $(TRANSPORT_SHIM_LIBS)

install: $(TRANSPORT_SHIM_LIBS)
	(mkdir -p ../../../lib/$(COMPILER); chmod o+rx $(TRANSPORT_SHIM_LIBS); cp -v $(TRANSPORT_SHIM_LIBS) ../../../lib/$(COMPILER); \
	cd ../../../lib/$(COMPILER); for b in $(TRANSPORT_SHIM_BASENAMES); do ln -sf lib$$b.so.$(MAJOR).$(MINOR) lib$$b.so.$(MAJOR); ln -sf lib$$b.so.$(MAJOR) lib$$b.so; done )
else
all: $(MTB) $(PTPD)

install: $(MTB) $(PTPD)
	(mkdir -p ../../../lib/$(COMPILER); chmod o+rx $(MTB); cp -v $(MTB) ../../../lib/$(COMPILER); mkdir -p ../../../bin/$(COMPILER); cp -v $(PTPD) ../../../bin/$(COMPILER);\
	cd ../../../lib/$(COMPILER); ln -sf $(MTB) lib$(MTBL).so.$(MAJOR); ln -sf lib$(MTBL).so.$(MAJOR) lib$(MTBL).so; \
	cd ../../bin/$(COMPILER); \
	ln -sf $(PTPD) $(PTPDL) )
endif

$(MTB): $(CPSOURCES) $(OBJECTS)
	$(MPICXX) -o $(MTB) $(DEBUG) $(OPT) $(OMP) $(LDOPTMT) $(CONTROLS) $(SHARED) $(OBJECTS) $(LIBS)

# transport shims dlsym() their real symbols at runtime and never call real MPI, so they're
# linked with plain CXX (CXX comes from env.sh; no MPI runtime dependency) and don't
# link against any specific core .so - gt_info(...)/t_info resolve at load time
# against whatever core lib is loaded alongside them in the target process
libtractor$(TRANSPORTLIBSUFFIX)_transport_%.so.$(MAJOR).$(MINOR): mpi_layer_transport_%.o
	$(CXX) -o $@ $(DEBUG) $(OPT) $(SHARED) $< -ldl

mpi_layer.o: mpi_layer.cpp

mpi_func.inc: ../../common/mpi_func.inc ../../common/mpi_func_ids.h
	$(CPP) $(cpp_input) ../../common/mpi_func.inc > mpi_func.inc

$(FUNCID): mpi_func.inc ../../common/mpi_func_ids.h
	../../common/mpi_func_ids.pl -i ../../common/mpi_func_ids.h mpi_func.inc -o mpi_func_ids_through.h mpi_func_names.h mpi_func_ids_trans.h  

mpi_layer.cpp: mpi_layer.cpp.in mpi_layer.cpp.in.end $(FUNCID)
	./mpi_layer.pl -p $(USE_PRAGMA_WEAK) -e $(PRINT_EXTENDED_STUFF) -m $(MAXMPI) -t $(PCT) -c $(PARSE_GPU) -s $(mpi_layer_skip_core) -i mpi_func.inc -o mpi_layer.cpp $(transport_func_layers)

# transport_func_layers with "=outfile" makes mpi_layer.cpp's recipe also emit these
# as a side effect; own recipe (not merged suffix-rule) avoids a -j race where
# the shim .so link could start before the .o was actually compiled
mpi_layer_transport_%.o: mpi_layer.cpp
	$(MPICXX) -o $@ -c $(INCLUDE) $(CONTROLS) $(CMISC) $(CXXMISC) $(DEBUG) $(OPT) $(OMP) $(FPIC) mpi_layer_transport_$*.cpp

$(OBJECTS):   $(FUNCID)

$(PTPD): $(MTB) 
	$(MPICC) -o $(PTPD) $(INCLUDE) $(CONTROLS) $(OPT) $(OMP) $(LDOPTMT) $(PTPD_LIBS) $(CXXMISC) src-ptpd.cpp t_globals.cpp  $(DEBUG) 

.c.o:
	$(MPICC) -o $@ -c $(INCLUDE) $(CONTROLS) $(CMISC) $(DEBUG) $(OPT) $(OMP) $(FPIC) $*.c
#	$(MPICC) -o $@.s -s $(INCLUDE) $(CONTROLS) $(CMISC) $(DEBUG) $(OPT) $(OMP) $(FPIC) $*.c

.cpp.o:
	$(MPICXX) -o $@ -c $(INCLUDE) $(CONTROLS) $(CMISC) $(CXXMISC) $(DEBUG) $(OPT) $(OMP) $(FPIC) $*.cpp
#	$(MPICXX) -o $@.pp -E $(INCLUDE) $(CONTROLS) $(CMISC) $(CXXMISC) $(DEBUG) $(OPT) $(OMP) $(FPIC) $*.cpp
#	$(MPICXX) -S $(INCLUDE) $(CONTROLS) $(CMISC) $(CXXMISC) $(DEBUG) $(OPT) $(OMP) $(FPIC) $*.cpp

.f.o:   
	$(MPIFC) -o $@ -c $(INCLUDE) $(CONTROLS) $(FMISC) $(DEBUG) $(OPT) $(OMP) $(FPIC) $*.f

clean:
	rm -f mpi_func_ids_through.h mpi_func_names.h mpi_func_ids_trans.h $(OBJECTS) lib$(MTBL).so lib$(MTBL).so.$(MAJOR) $(MTB) $(PTPD) $(PTPDL) mpi_layer.c  \
        t_transport_layer.h t_transport_layer_const.h t_transport_layer_h.h \
        $(TRANSPORT_LAYER_SRCS) $(TRANSPORT_LAYER_OBJS) $(TRANSPORT_SHIM_LIBS)

