# Cross-rank traffic test for parallel mat. Build with the MPI compiler wrapper in PATH.
MPICC   ?= mpicc
CFLAGS  ?= -O2 -g -Wall
COMMON  = ../../../tractor/tests/func-classes

all: traffic_test

traffic_test: traffic_test.c $(COMMON)/test_common.h
	$(MPICC) $(CFLAGS) -I$(COMMON) -o $@ $<

clean:
	rm -f traffic_test

.PHONY: all clean
