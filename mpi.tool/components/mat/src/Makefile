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

#ZLIBHOME=$(ZLIB_PATH)
#ZLIBINCLUDE=-I$(ZLIBHOME)/include
ZLIBLIB=-lz

DEFINES=-DMAT_PROCESS_HWC_MPI_OUTPUT

INCLUDES=-I../../common -I. $(ZLIBINCLUDE)
TARGET=serial
COMMON_FLAGS=-O3 -std=c++20
#COMMON_FLAGS=-O0 -std=c++20 -g

ifeq "$(TARGET)" "impi"
CXX=mpiicpx
CXXFLAGS=$(COMMON_FLAGS) $(INCLUDES) -DPEAT_MPI
endif

ifeq "$(TARGET)" "mvapich2"
CXX=mpicxx
CXXFLAGS=$(COMMON_FLAGS) $(INCLUDES) -DPEAT_MPI
endif

ifeq "$(TARGET)" "ompi"
CXX=mpicxx
CXXFLAGS=$(COMMON_FLAGS) $(INCLUDES) -DPEAT_MPI
endif

EXECUTABLE=pmat.$(TARGET)


ifeq "$(TARGET)" "serial"
CXX=g++
CXXFLAGS=-W -Wall -Wnon-virtual-dtor -Wold-style-cast -Wctor-dtor-privacy $(COMMON_FLAGS) $(INCLUDES)
EXECUTABLE=mat
endif

ifeq "$(TARGET)" "openmp"
CXX=g++
CXXFLAGS=-W -Wall -Wnon-virtual-dtor -Wold-style-cast -Wctor-dtor-privacy -fopenmp $(COMMON_FLAGS) $(INCLUDES) $(DEFINES)
EXECUTABLE=mat.omp
endif

#-Wunreachable-code

.SUFFIXES: .cpp .o .h

MAIN_FILES = collectives mat p2pal requests tempdebug tevent gpu comm request_store
COMMON_FILES = file_io func_id utils common_types ptp communicators model_id

#HEADERS=$(addsuffix .h, $(MAIN_FILES)) $(addprefix ../../common/, $(addsuffix .h, $(COMMON_FILES)))
OBJECTS=$(addsuffix .o, $(COMMON_FILES)) $(addsuffix .o, $(MAIN_FILES)) main.o

$(EXECUTABLE): $(OBJECTS)
	$(CXX) $(CXXFLAGS) $(OBJECTS) $(ZLIBLIB) -o $@

%.o : ../../common/%.cpp #$(HEADERS)
	$(CXX) -c $(CXXFLAGS) $< -o $@

%.o : %.cpp #$(HEADERS)
	$(CXX) -c $(CXXFLAGS) $< -o $@

clean:
	rm -f $(OBJECTS) $(EXECUTABLE) 

install: $(EXECUTABLE)
	(mkdir -p ../../../bin; mv $(EXECUTABLE) ../../../bin/$(EXECUTABLE);)

