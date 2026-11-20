#!/bin/sh


mpi_type=$1
compiler=oneapi:2026.1.0
libfabric_eng='-eng'
. env.sh

src_tmp='src-build'
rm -rf $src_tmp
mkdir -p $src_tmp
cd $src_tmp

# llama.cpp and CUDA tracing from the environment, as in Build.sh: LLAMA_ROOT=<llama.cpp source tree>
# [LLAMA_DEPRECATED=on], CUPTI_PARSE=auto|on [CUDA_ROOT=<CUDA toolkit>]
cmake -DCMAKE_C_COMPILER=$MPICC -DCMAKE_CXX_COMPILER=$MPICXX -DPEAT_TIMER=nano \
    ${LLAMA_ROOT:+-DLLAMA=ON -DLLAMA_ROOT=$LLAMA_ROOT} ${LLAMA_DEPRECATED:+-DLLAMA_DEPRECATED=$LLAMA_DEPRECATED} \
    ${CUPTI_PARSE:+-DCUPTI=$CUPTI_PARSE} ${CUDA_ROOT:+-DCUDA_ROOT=$CUDA_ROOT} \
    ../src

cmake --build .
cmake --build . -- install

exit

rm -rf *

cmake -DCMAKE_C_COMPILER=$MPICC -DCMAKE_CXX_COMPILER=$MPICXX  -DTRACTOR_TARGET=transport_layers\
    ../src

cmake --build .
cmake --build . -- install



