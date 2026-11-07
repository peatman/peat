# Per-class MPI function tests. Build with the MPI compiler wrapper in PATH:
#   make                 all five tests, without MPI-4 functions (large-count, persistent collectives,
#                        partitioned communication, ...): main's tractor traces up to MPI-3.1 only
#   make NO_MPI4=        include the MPI-4 functions
#   make NO_MPI1_REMOVED=1  leave out MPI-1 functions removed in MPI-3.0 (MPI_Type_extent, ...);
#                        detected automatically for Open MPI built without MPI-1 compatibility
MPICC   ?= mpicc
CFLAGS  ?= -O2 -g -Wall
NO_MPI4 ?= 1
ifneq "$(NO_MPI4)" ""
CFLAGS  += -DNO_MPI4
endif
ifdef NO_MPI1_REMOVED
CFLAGS  += -DNO_MPI1_REMOVED
endif

TESTS = p2p_test coll_test comm_test rma_test fio_test

all: $(TESTS)

%: %.c test_common.h
	$(MPICC) $(CFLAGS) -o $@ $<

clean:
	rm -f $(TESTS) *.o fio_test.*.dat

.PHONY: all clean
