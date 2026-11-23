#!/bin/sh

export compiler=gcc # for environment script
export COMPILER=gnu # for make

# The libraries (lib/$COMPILER):
#   libtractor.core.so (libtractor_dbg.core.so: DIAG)  the core, no MPI: one for every MPI
#   libtractor.<mpi>[.eng].so                          the MPI layer of an MPI; it loads the core next to it (or a core
#                                                      variant preloaded before it), so preloading it alone is enough
#   libtractor_transport_<api>.so                      the transport shims (psm2, psm3, ucx, ofi)
#   libtractor_<class>.so                              the event-class shims: ompt, acc (gcc/nvc only), opencl, levelzero,
#                                                      cuda, llama, each one built when its headers are found
#                                                      (LLAMA_ROOT=<llama.cpp>, CUDA_ROOT=<CUDA toolkit>)
# e.g. LD_PRELOAD=libtractor.mpich5.eng.so:libtractor_transport_ofi.so:libtractor_ompt.so
# DIAG is a switch of the core: the MPI layers are release builds (DIAG MPI layers by hand, with a DIAG core).

rm -f Build.${COMPILER}.log
rm -f Build.${COMPILER}.dir


# the core (the MPI only sets up the environment: the core is built with the plain compilers)
./Build.sh mpich:5.0.1g-mt-ofi-eng        core  off   off  off  off off off &
./Build.sh mpich:5.0.1g-mt-ofi-eng        core  off   on   off  off off off &

./Build.sh  mpich:5.0.1g-mt-ofi-eng     transport_layers off   off  off  off off off &
./Build.sh  mpich:5.0.1g-mt-ofi-eng     transport_layers off   on   off  off off off &


# the event-class shims (the MPI does not matter here, only for mpi.h; no DIAG/HWC/MEM: they fit any core variant)
./Build.sh  mpich:5.0.1g-mt-ofi-eng     class_layers off   off  off  off off off &


#          mpi_type                         target  mem  diag  GPU  buf v2p  hwc
./Build.sh impi                               impi  off   off  off  off off off &
#./Build.sh impi                               impi  off   off  on   off off off & # GPU=on: the same core as GPU=off with CLASS_SHIMS=on
#./Build.sh impi                               impi  off   on   off  off off off & # DIAG: the core only
#./Build.sh impi                               impi  off   on   on  off off off & # GPU=on: the same core as GPU=off with CLASS_SHIMS=on

# export ENG_MPI_BUILD=on
#          1                                  2       3    4     5    6   7    8
#       mpi_type                             target  mem  diag  GPU  buf v2p  hwc

./Build.sh openmpi:5.0.10g                  openmpi5   off   off  off  off off off &
#./Build.sh openmpi:5.0.10g                  openmpi5   off   on   off  off off off & # DIAG: the core only
#./Build.sh openmpi:5.0.10g                  openmpi   off   off  on   off off off & # GPU=on: the same core as GPU=off with CLASS_SHIMS=on
#./Build.sh openmpi:5.0.10g                  openmpi   off   on   on   off off off & # GPU=on: the same core as GPU=off with CLASS_SHIMS=on

#          1                                  2       3    4     5    6   7    8 
#       mpi_type                           target    mem  diag  GPU  buf v2p  hwc

./Build.sh mpich:4.3.2g-mt-ofi             mpich4    off   off  off  off off  off &
#./Build.sh mpich:4.3.2g-mt-ofi             mpich4    off   off  on   off off  off & # GPU=on: the same core as GPU=off with CLASS_SHIMS=on
#./Build.sh mpich:4.3.2g-mt-ofi             mpich4    off   on   off  off off  off & # DIAG: the core only
#./Build.sh mpich:4.3.2g-mt-ofi             mpich4    off   on   on   off off  off & # GPU=on: the same core as GPU=off with CLASS_SHIMS=on


./Build.sh mpich:5.0.1g-mt-ofi             mpich5    off   off  off  off off  off &
#./Build.sh mpich:5.0.1g-mt-ofi             mpich5    off   off  on   off off  off & # GPU=on: the same core as GPU=off with CLASS_SHIMS=on
#./Build.sh mpich:5.0.1g-mt-ofi             mpich5    off   on   off  off off  off & # DIAG: the core only
#./Build.sh mpich:5.0.1g-mt-ofi             mpich5    off   on   on   off off  off & # GPU=on: the same core as GPU=off with CLASS_SHIMS=on


./Build.sh mvapich:4.1g-mt-ofi             mvapich4  off   off  off  off off off &
#./Build.sh mvapich:4.1g-mt-ofi             mvapich4  off   off  on   off off off & # GPU=on: the same core as GPU=off with CLASS_SHIMS=on
#./Build.sh mvapich:4.1g-mt-ofi             mvapich4  off   on   off  off off off & # DIAG: the core only
#./Build.sh mvapich:4.1g-mt-ofi             mvapich4  off   on   on   off off off & # GPU=on: the same core as GPU=off with CLASS_SHIMS=on



./Build.sh mpich:4.3.2g-mt-ofi-eng         mpich4    off   off  off  off off  off &
#./Build.sh mpich:4.3.2g-mt-ofi-eng         mpich4    off   off  on   off off  off & # GPU=on: the same core as GPU=off with CLASS_SHIMS=on
#./Build.sh mpich:4.3.2g-mt-ofi-eng         mpich4    off   on   off  off off  off & # DIAG: the core only
#./Build.sh mpich:4.3.2g-mt-ofi-eng         mpich4    off   on   on   off off  off & # GPU=on: the same core as GPU=off with CLASS_SHIMS=on

./Build.sh mpich:5.0.1g-mt-ofi-eng         mpich5    off   off  off  off off  off &
#./Build.sh mpich:5.0.1g-mt-ofi-eng         mpich5    off   off  on   off off  off & # GPU=on: the same core as GPU=off with CLASS_SHIMS=on
#./Build.sh mpich:5.0.1g-mt-ofi-eng         mpich5    off   on   off  off off  off & # DIAG: the core only
#./Build.sh mpich:5.0.1g-mt-ofi-eng         mpich5    off   on   on   off off  off & # GPU=on: the same core as GPU=off with CLASS_SHIMS=on

./Build.sh mvapich:4.1g-mt-ofi-eng         mvapich4  off   off  off  off off off &
#./Build.sh mvapich:4.1g-mt-ofi-eng         mvapich4  off   off  on   off off off & # GPU=on: the same core as GPU=off with CLASS_SHIMS=on
#./Build.sh mvapich:4.1g-mt-ofi-eng         mvapich4  off   on   off  off off off & # DIAG: the core only
#./Build.sh mvapich:4.1g-mt-ofi-eng         mvapich4  off   on   on   off off off & # GPU=on: the same core as GPU=off with CLASS_SHIMS=on




wait
