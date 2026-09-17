#!/bin/bash

rm -f omptest *omp_stat*

gcc -o omptest -O2 -fopenmp -g omptest.c

libgomp=$(gcc -print-file-name=libgomp.so)
if ! nm -D "$libgomp" | grep -q 'ompt_start_tool'; then
	echo "GNU libgomp does not provide OMPT; libompstat cannot collect OpenMP events with this runtime." >&2
	echo "Use an OMPT-capable runtime or add libgomp-specific interception support." >&2
	exit 77
fi

export OMP_TOOL=enabled
export OMP_TOOL_LIBRARIES=../../lib/gnu/libompstat.so
export OSL_FREQUENCY=2000000000
export OMP_NUM_THREADS=1
./omptest

export OMP_NUM_THREADS=2
./omptest

export OMP_NUM_THREADS=4
./omptest

