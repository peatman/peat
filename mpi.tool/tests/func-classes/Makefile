# Per-class MPI function tests. Build with the MPI compiler wrapper in PATH:
#   make                 all five tests
#   make NO_MPI4=1       leave out MPI-4 functions (large-count, persistent
#                        collectives, partitioned communication, ...)
MPICC   ?= mpicc
CFLAGS  ?= -O2 -g -Wall
ifdef NO_MPI4
CFLAGS  += -DNO_MPI4
endif

TESTS = p2p_test coll_test comm_test rma_test fio_test

all: $(TESTS)

%: %.c test_common.h
	$(MPICC) $(CFLAGS) -o $@ $<

clean:
	rm -f $(TESTS) *.o fio_test.*.dat

.PHONY: all clean
