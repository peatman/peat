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
#CXX=icpx
CXX=g++

CONTROLS=-DREMOVE_T_OVERHEAD -D_PET_USE_BIND_STORE_
#CXXFLAGS=-O2 -std=c++20 -g  -I. -I../../common $(ZLIBINCLUDE) -qopenmp $(CONTROLS) #-static-intel #-DNDEBUG=1 -W -Wall -Wnon-virtual-dtor -Wctor-dtor-privacy -Wshadow
CXXFLAGS=-O2 -std=c++20 -g  -I. -I../../common $(ZLIBINCLUDE) -fopenmp $(CONTROLS) #-static-intel #-DNDEBUG=1 -W -Wall -Wnon-virtual-dtor -Wctor-dtor-privacy -Wshadow
#-Wold-style-cast 
# -pedantic -std=c++11 -Wformat=2 -Wfloat-equal -Wconversion -Wlogical-op -Wcast-align -Wcast-qual -Wshadow 
#CXXFLAGS+=-D_GLIBCXX_DEBUG -D_GLIBCXX_DEBUG_PEDANTIC
#-DNDEBUG
#-D_FORTIFY_SOURCE=2
#-fsanitize=address
#-fsanitize=undefined

#-Wunreachable-code

.SUFFIXES: .cpp .o .h

EXECUTABLE=pet
PET_MAIN_FILES = collstorage pet_bind pet_core pet_node pet_rank pet_thread requests stat te_timeline tempdebug timeline events
PET_EVENT_FILES = base coll collective comm completion local localbuf start tar tor gpu ompt
COMMON_FILES = file_io func_id utils common_types communicators transport model_id ptp func_split

HEADERS=$(addsuffix .h, $(PET_MAIN_FILES)) $(addprefix ../../common/, $(addsuffix .h, $(COMMON_FILES)))
OBJECTS=$(addsuffix .o, $(COMMON_FILES)) $(addsuffix .o, $(PET_EVENT_FILES)) $(addsuffix .o, $(PET_MAIN_FILES)) main.o

$(EXECUTABLE): $(OBJECTS)
	$(CXX) $(CXXFLAGS) $(OBJECTS) -o $@ $(ZLIBLIB)

%.o : %.cpp  #$(HEADERS)
	$(CXX) -c $(CXXFLAGS) $< -o $@

%.o : ../../common/%.cpp  #$(HEADERS)
	$(CXX) -c $(CXXFLAGS) $< -o $@

%.o : pet_events/%.cpp  #$(HEADERS)
	$(CXX) -c $(CXXFLAGS) $< -o $@

clean:
	rm -f $(OBJECTS) $(EXECUTABLE)

install: $(EXECUTABLE)
	mkdir -p ../../../bin
#	strip $(EXECUTABLE)
	mv $(EXECUTABLE) ../../../bin/$(EXECUTABLE)
