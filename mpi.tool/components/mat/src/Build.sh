#!/bin/sh

compiler=$1
mpi_type=$2
. env.sh

which $CXX

make clean TARGET=$3
make -j 16 TARGET=$3  2>&1 | tee -a Build.log
make install TARGET=$3 
