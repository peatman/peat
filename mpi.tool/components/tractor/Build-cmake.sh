#!/bin/sh


mpi_type=$1
compiler=oneapi:2026.1.0
libfabric_eng='-eng'
. env.sh

src_tmp='src-build'
rm -rf $src_tmp
mkdir -p $src_tmp
cd $src_tmp

cmake -DCMAKE_CXX_COMPILER=$MPICXX -DPEAT_TIMER=nano \
    ../src

cmake --build .
cmake --build . -- install

exit

rm -rf *

cmake -DCMAKE_CXX_COMPILER=$MPICXX  -DTRACTOR_TARGET=fab_layers\
    ../src

cmake --build .
cmake --build . -- install



