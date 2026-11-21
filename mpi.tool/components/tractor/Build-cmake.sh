#!/bin/sh


mpi_type=$1
compiler=oneapi:2026.1.0
libfabric_eng='-eng'
. env.sh

src_tmp='src-build'
rm -rf $src_tmp
mkdir -p $src_tmp
cd $src_tmp

# llama.cpp and CUDA tracing (LLAMA_PARSE, LLAMA_ROOT, LLAMA_DEPRECATED, CUPTI_PARSE, CUDA_ROOT) are read from the
# environment by CMakeLists.txt, as the Makefile does
cmake -DCMAKE_C_COMPILER=$MPICC -DCMAKE_CXX_COMPILER=$MPICXX -DPEAT_TIMER=nano \
    ../src

cmake --build .
cmake --build . -- install

exit

rm -rf *

cmake -DCMAKE_C_COMPILER=$MPICC -DCMAKE_CXX_COMPILER=$MPICXX  -DTRACTOR_TARGET=transport_layers\
    ../src

cmake --build .
cmake --build . -- install



