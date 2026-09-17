#!/bin/bash

. env.sh

rm -f omptest *omp_stat*

icx -o omptest -O2 -qopenmp -g omptest.c
export LD_PRELOAD=../../lib/intel/libompstat.so 
#export OSL_FREQUENCY=2000000000

export KMP_AFFINITY=compact,1

export OMP_NUM_THREADS=1
./omptest

export OMP_NUM_THREADS=2
./omptest

export OMP_NUM_THREADS=4
./omptest

