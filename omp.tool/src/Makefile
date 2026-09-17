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
OBJECTS=kmp_stat.o ompcalls.o
COBJECTS=offconv.o
#build options
shell=bash
SUFFIX=
PTPD=on

CPP=/usr/bin/cpp -E

TRACTOR_BUILD_DATE:="$(shell date)"
#compiler options
OPT=-O3 #-Zp4
FPIC=-fpic
INCLUDE=-I.
LIBS=

ifeq "$(COMPILER)" "intel"
SINCLUDE=-I$(CMPLR_ROOT)/include
CXXMISC=-std=c++11 -fno-omit-frame-pointer -w -use-intel-optimized-headers $(SINCLUDE) #-Wwritable-strings -Wformat 
CMISC=-fno-omit-frame-pointer -w -use-intel-optimized-headers $(SINCLUDE)
CONTROLS+=-D_INTELCOMPILER_ -g
OMP=-qopenmp
CXX=icpx
#-fasm-blocks
else
SINCLUDE=-isystem /usr/include # omp.h
COMPILER=gnu #default
CXXMISC=-Wno-write-strings
CMISC=-fno-omit-frame-pointer -fpermissive
CONTROLS+=-D_GNUCOMPILER_ 
OMP=-fopenmp
CXX=g++
#-fasm-blocks
endif


LIBSUFFIX=$(SUFFIX)$(DBG)
MTBL=ompstat$(LIBSUFFIX)
MTB=lib$(MTBL).so.$(MAJOR).$(MINOR)
CONV=offconv
SHARED=-shared $(FPIC)


all: $(MTB) $(CONV)

$(MTB): $(OBJECTS)
	$(CXX) -o $(MTB) $(DEBUG) $(OPT) $(OMP) $(CONTROLS) $(SHARED) $(OBJECTS) $(LIBS)

$(CONV):$(COBJECTS)
	$(CC) -o $(CONV) $(DEBUG) $(OPT) $(OMP) $(CONTROLS) $(COBJECTS) $(LIBS)
 
install: $(MTB) $(CONV) 
	(mkdir -p ../lib/$(COMPILER); chmod o+rx $(MTB); mv $(MTB) ../lib/$(COMPILER); \
	cd ../lib/$(COMPILER); ln -sf $(MTB) lib$(MTBL).so.$(MAJOR); ln -sf lib$(MTBL).so.$(MAJOR) lib$(MTBL).so; )
	(mkdir -p ../bin; chmod o+rx $(CONV); mv -v $(CONV) ../bin; )


.c.o:   
#	$(CC) -E -o $@.pp -c $(INCLUDE) $(SINCLUDE) $(CONTROLS) $(CMISC) $(DEBUG) $(OPT) $(OMP) $(FPIC) $*.c
	$(CC) -o $@ -c $(INCLUDE) $(SINCLUDE) $(CONTROLS) $(CMISC) $(DEBUG) $(OPT) $(OMP) $(FPIC) $*.c

.cpp.o: 
#	$(CXX) -o $@.pp -E $(INCLUDE) $(SINCLUDE) $(CONTROLS) $(CMISC) $(CXXMISC) $(DEBUG) $(OPT) $(OMP) $(FPIC) $*.cpp
	$(CXX) -o $@ -c $(INCLUDE) $(SINCLUDE) $(CONTROLS) $(CMISC) $(CXXMISC) $(DEBUG) $(OPT) $(OMP) $(FPIC) $*.cpp

.f.o:   
	$(FC) -o $@ -c $(INCLUDE) $(CONTROLS) $(FMISC) $(DEBUG) $(OPT) $(OMP) $(FPIC) $*.f

clean:
	rm -f $(OBJECTS) $(COBJECTS) lib$(MTBL).so lib$(MTBL).so.$(MAJOR) $(MTB)  $(CONV) 
