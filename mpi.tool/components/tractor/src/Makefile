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
#objects
OBJECTS=t_base_event.o tractor.o t_stat.o t_comm.o elf_parser.o t_topo.o mpi_layer.o t_globals.o
FUNCIDS=mpi_func_ids_through.h mpi_func_names.h mpi_func_ids_trans.h
FUNCID=mpi_func_ids_through.h # can pick any of 3 above
#build options
shell=bash
SUFFIX=
PTPD=on
TARGET=impi
PARSE_MPI_INTERNAL=off
PARSE_GPU=off
OVERALLSTATONLY=off
DIAG=off
USE_PRAGMA_WEAK=on #enabled by default
USE_SIGNALS=on
MAXMPI=3
OMPT=on

CPP:=/usr/bin/cpp -E

TRACTOR_BUILD_DATE:="$(shell date)"
#compiler options
OPT=-O3 #-Zp4
FPIC=-fpic
INCLUDE=-I. -I../../common 
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
CONTROLS+=-D_VERBS_PROFILE_
endif


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
CONTROLS+=-D_MPI_MESSAGE_DEFINED_ -D_NO_C_VR -D_OPENMPI_=5  -fms-extensions -Wno-int-to-pointer-cast
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

# MPI 4.x prototype probes: some MPI 4.x implementations declare a few functions without the standard's
# const (MPICH 4.0: MPI_Psend_init, MPI_Pready_list; MPICH 4.2/MVAPICH 4.1/Intel MPI 2021: MPI_Status_get_*,
# MPI_Request_get_status_*) or lack them (MPICH 4.0: MPI_Status_set_elements_c); mpi_func.inc picks the
# matching entries. Compile-only: C++ rejects the pointer assignment when the prototype differs or is missing.
# Placed after the TARGET blocks, which assign cpp_input with '='.
HASH:=\#
COMMA:=,
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

SHARED=-shared $(FPIC)

DEBUG=-g # -D_TRACTOR_PASSTHROUGH_
DEBUG_TESTS=-g -fno-inline 

ifeq "$(DIAG)" "on"
DBG=_dbg
DEBUG+= -fno-inline -D_TRACTOR_DIAG_ -D_TRACTOR_DEBUG_TRACTOR_LAYER_ENTRY_EXIT_  -D_TRACTOR_DEBUG_MPI_LAYER_ENTRY_EXIT_ -D_TRACTOR_DEBUG_GPU_LAYER_ENTRY_EXIT_ -D_TRACTOR_DEBUG_OMPT_ENTRY_EXIT_  -D_TRACTOR_DEBUG_COMM_ENTRY_EXIT_ 
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


LIBSUFFIX=$(SUFFIX)$(DBG).$(TARGET)$(EXTRANAMELIB)$(CT_SUFFIX)
BINSUFFIX=$(SUFFIX)$(DBG).$(TARGET)
MTBL=tractor$(LIBSUFFIX)
MTB=lib$(MTBL).so.$(MAJOR).$(MINOR)
PTPDL=ptpd$(BINSUFFIX)
PTPD=$(PTPDL).$(MAJOR).$(MINOR)

CMISC+=-D_TRACTOR_LIB_SUFFIX_=\"$(LIBSUFFIX)\" -D_TRACTOR_BIN_SUFFIX_=\"$(BINSUFFIX)\"   -D_TRACTOR_ZERO_ON_ALLOCATE_ #-D_TRACTOR_PASSTHROUGH_

$(MTB): $(CPSOURCES) $(OBJECTS)
	$(MPICXX) -o $(MTB) $(DEBUG) $(OPT) $(OMP) $(LDOPTMT) $(CONTROLS) $(SHARED) $(OBJECTS) $(LIBS)

mpi_layer.o: mpi_layer.cpp

mpi_func.inc: ../../common/mpi_func.inc ../../common/mpi_func_ids.h
	$(CPP) $(cpp_input) ../../common/mpi_func.inc > mpi_func.inc

$(FUNCID): mpi_func.inc ../../common/mpi_func_ids.h
	../../common/mpi_func_ids.pl -i ../../common/mpi_func_ids.h mpi_func.inc -o mpi_func_ids_through.h mpi_func_names.h mpi_func_ids_trans.h  

mpi_layer.cpp: mpi_layer.cpp.in mpi_layer.cpp.in.end $(FUNCID)
	./mpi_layer.pl -p $(USE_PRAGMA_WEAK) -m $(MAXMPI) -t $(PCT) -c $(PARSE_GPU) -i mpi_func.inc -o mpi_layer.cpp 

$(OBJECTS):   $(FUNCID)

$(PTPD): $(MTB) 
# C++ sources: compile and link with the C++ wrapper (mpicc does not link libstdc++, and Open MPI 4's mpi.h
# pulls in its C++ bindings, which need libstdc++ and libmpi_cxx)
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

# mpi_func.inc and mpi_layer.cpp too: they depend on TARGET/PARSE_MPI_INTERNAL, which make cannot see
clean:
	rm -f mpi_func_ids_through.h mpi_func_names.h mpi_func_ids_trans.h $(OBJECTS) lib$(MTBL).so lib$(MTBL).so.$(MAJOR) $(MTB) $(PTPD) $(PTPDL) mpi_layer.c mpi_func.inc mpi_layer.cpp
        

all: $(MTB) $(PTPD)

install: $(MTB) $(PTPD)
	(mkdir -p ../../../lib/$(COMPILER); chmod o+rx $(MTB); cp -v $(MTB) ../../../lib/$(COMPILER); mkdir -p ../../../bin/$(COMPILER); cp -v $(PTPD) ../../../bin/$(COMPILER);\
	cd ../../../lib/$(COMPILER); ln -sf $(MTB) lib$(MTBL).so.$(MAJOR); ln -sf lib$(MTBL).so.$(MAJOR) lib$(MTBL).so; \
	cd ../../bin/$(COMPILER); \
	ln -sf $(PTPD) $(PTPDL) )
