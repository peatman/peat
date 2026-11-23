#!/bin/sh

# Same builds as BuildAllg.sh, with the NVHPC compilers (nvc/nvc++, CUDA 12.9: needed for Pascal GPUs) instead of GCC.
# The MPIs stay the GNU builds ('g') for now. Results go to lib/nvc and bin/nvc.
export compiler=nvc:12.9 # for environment script
export COMPILER=nvc #for make

# The event classes are shims (TARGET=class_layers, preloaded next to a core): libtractor_ompt.so, _acc.so (gcc/nvc
# only), _opencl.so, _levelzero.so, _cuda.so, _llama.so, each one built when its headers are found (LLAMA_ROOT=<llama.cpp
# source tree or prefix>, CUDA_ROOT=<CUDA toolkit>, else the installed ones). With CLASS_SHIMS=on (Build.sh) the cores
# are built without the classes, so there are no .gpu core variants any more: the GPU column of the core lines is off.
export CLASS_SHIMS=on

rm -f Build.${COMPILER}.log
rm -f Build.${COMPILER}.dir

export TRANSPORT=on

./Build.sh  mpich:5.0.1g-mt-ofi-eng     transport_layers off   off  off  off off off &
./Build.sh  mpich:5.0.1g-mt-ofi-eng     transport_layers off   on   off  off off off &

unset TRANSPORT

# the event-class shims (the MPI does not matter here, only for mpi.h; no DIAG/HWC/MEM: they fit any core variant)
./Build.sh  mpich:5.0.1g-mt-ofi-eng     class_layers off   off  off  off off off &


#          mpi_type                         target  mem  diag  GPU  buf v2p  hwc
./Build.sh impi                               impi  off   off  off  off off off &
#./Build.sh impi                               impi  off   off  on   off off off & # GPU=on: the same core as GPU=off with CLASS_SHIMS=on
./Build.sh impi                               impi  off   on   off  off off off &
#./Build.sh impi                               impi  off   on   on  off off off & # GPU=on: the same core as GPU=off with CLASS_SHIMS=on

# export ENG_MPI_BUILD=on
#          1                                  2       3    4     5    6   7    8
#       mpi_type                             target  mem  diag  GPU  buf v2p  hwc

./Build.sh openmpi:5.0.10g                  openmpi   off   off  off  off off off &
./Build.sh openmpi:5.0.10g                  openmpi   off   on   off  off off off &
#./Build.sh openmpi:5.0.10g                  openmpi   off   off  on   off off off & # GPU=on: the same core as GPU=off with CLASS_SHIMS=on
#./Build.sh openmpi:5.0.10g                  openmpi   off   on   on   off off off & # GPU=on: the same core as GPU=off with CLASS_SHIMS=on

#          1                                  2       3    4     5    6   7    8 
#       mpi_type                           target    mem  diag  GPU  buf v2p  hwc

./Build.sh mpich:4.3.2g-mt-ofi             mpich4    off   off  off  off off  off &
#./Build.sh mpich:4.3.2g-mt-ofi             mpich4    off   off  on   off off  off & # GPU=on: the same core as GPU=off with CLASS_SHIMS=on
./Build.sh mpich:4.3.2g-mt-ofi             mpich4    off   on   off  off off  off &
#./Build.sh mpich:4.3.2g-mt-ofi             mpich4    off   on   on   off off  off & # GPU=on: the same core as GPU=off with CLASS_SHIMS=on


./Build.sh mpich:5.0.1g-mt-ofi             mpich5    off   off  off  off off  off &
#./Build.sh mpich:5.0.1g-mt-ofi             mpich5    off   off  on   off off  off & # GPU=on: the same core as GPU=off with CLASS_SHIMS=on
./Build.sh mpich:5.0.1g-mt-ofi             mpich5    off   on   off  off off  off &
#./Build.sh mpich:5.0.1g-mt-ofi             mpich5    off   on   on   off off  off & # GPU=on: the same core as GPU=off with CLASS_SHIMS=on


./Build.sh mvapich:4.1g-mt-ofi             mvapich4  off   off  off  off off off &
#./Build.sh mvapich:4.1g-mt-ofi             mvapich4  off   off  on   off off off & # GPU=on: the same core as GPU=off with CLASS_SHIMS=on
./Build.sh mvapich:4.1g-mt-ofi             mvapich4  off   on   off  off off off &
#./Build.sh mvapich:4.1g-mt-ofi             mvapich4  off   on   on   off off off & # GPU=on: the same core as GPU=off with CLASS_SHIMS=on


export TRANSPORT=on

./Build.sh mpich:4.3.2g-mt-ofi-eng         mpich4    off   off  off  off off  off &
#./Build.sh mpich:4.3.2g-mt-ofi-eng         mpich4    off   off  on   off off  off & # GPU=on: the same core as GPU=off with CLASS_SHIMS=on
./Build.sh mpich:4.3.2g-mt-ofi-eng         mpich4    off   on   off  off off  off &
#./Build.sh mpich:4.3.2g-mt-ofi-eng         mpich4    off   on   on   off off  off & # GPU=on: the same core as GPU=off with CLASS_SHIMS=on

./Build.sh mpich:5.0.1g-mt-ofi-eng         mpich5    off   off  off  off off  off &
#./Build.sh mpich:5.0.1g-mt-ofi-eng         mpich5    off   off  on   off off  off & # GPU=on: the same core as GPU=off with CLASS_SHIMS=on
./Build.sh mpich:5.0.1g-mt-ofi-eng         mpich5    off   on   off  off off  off &
#./Build.sh mpich:5.0.1g-mt-ofi-eng         mpich5    off   on   on   off off  off & # GPU=on: the same core as GPU=off with CLASS_SHIMS=on

./Build.sh mvapich:4.1g-mt-ofi-eng         mvapich4  off   off  off  off off off &
#./Build.sh mvapich:4.1g-mt-ofi-eng         mvapich4  off   off  on   off off off & # GPU=on: the same core as GPU=off with CLASS_SHIMS=on
./Build.sh mvapich:4.1g-mt-ofi-eng         mvapich4  off   on   off  off off off &
#./Build.sh mvapich:4.1g-mt-ofi-eng         mvapich4  off   on   on   off off off & # GPU=on: the same core as GPU=off with CLASS_SHIMS=on




wait
