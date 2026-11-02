#!/bin/sh

# Same builds as BuildAllg.sh, with clang/clang++ instead of GCC (OpenMP tools interface on; no OpenACC: clang has no acc_prof.h).
# The MPIs stay the GNU builds ('g') for now. Results go to lib/clang and bin/clang.
export compiler=clang # for environment script
export COMPILER=clang # for make

rm -f Build.${COMPILER}.log
rm -f Build.${COMPILER}.dir

export TRANSPORT=on

./Build.sh  mpich:5.0.1g-mt-ofi-eng     transport_layers off   off  off  off off off &
./Build.sh  mpich:5.0.1g-mt-ofi-eng     transport_layers off   on   off  off off off &

unset TRANSPORT


#          mpi_type                         target  mem  diag  GPU  buf v2p  hwc
./Build.sh impi                               impi  off   off  off  off off off &
./Build.sh impi                               impi  off   off  on   off off off &
./Build.sh impi                               impi  off   on   off  off off off &
./Build.sh impi                               impi  off   on   on  off off off &

# export ENG_MPI_BUILD=on
#          1                                  2       3    4     5    6   7    8
#       mpi_type                             target  mem  diag  GPU  buf v2p  hwc

./Build.sh openmpi:5.0.10g                  openmpi   off   off  off  off off off &
./Build.sh openmpi:5.0.10g                  openmpi   off   on   off  off off off &
./Build.sh openmpi:5.0.10g                  openmpi   off   off  on   off off off &
./Build.sh openmpi:5.0.10g                  openmpi   off   on   on   off off off &

#          1                                  2       3    4     5    6   7    8 
#       mpi_type                           target    mem  diag  GPU  buf v2p  hwc

./Build.sh mpich:4.3.2g-mt-ofi             mpich4    off   off  off  off off  off &
./Build.sh mpich:4.3.2g-mt-ofi             mpich4    off   off  on   off off  off &
./Build.sh mpich:4.3.2g-mt-ofi             mpich4    off   on   off  off off  off &
./Build.sh mpich:4.3.2g-mt-ofi             mpich4    off   on   on   off off  off &


./Build.sh mpich:5.0.1g-mt-ofi             mpich5    off   off  off  off off  off &
./Build.sh mpich:5.0.1g-mt-ofi             mpich5    off   off  on   off off  off &
./Build.sh mpich:5.0.1g-mt-ofi             mpich5    off   on   off  off off  off &
./Build.sh mpich:5.0.1g-mt-ofi             mpich5    off   on   on   off off  off &


./Build.sh mvapich:4.1g-mt-ofi             mvapich4  off   off  off  off off off &
./Build.sh mvapich:4.1g-mt-ofi             mvapich4  off   off  on   off off off &
./Build.sh mvapich:4.1g-mt-ofi             mvapich4  off   on   off  off off off &
./Build.sh mvapich:4.1g-mt-ofi             mvapich4  off   on   on   off off off &


export TRANSPORT=on

./Build.sh mpich:4.3.2g-mt-ofi-eng         mpich4    off   off  off  off off  off &
./Build.sh mpich:4.3.2g-mt-ofi-eng         mpich4    off   off  on   off off  off &
./Build.sh mpich:4.3.2g-mt-ofi-eng         mpich4    off   on   off  off off  off &
./Build.sh mpich:4.3.2g-mt-ofi-eng         mpich4    off   on   on   off off  off &

./Build.sh mpich:5.0.1g-mt-ofi-eng         mpich5    off   off  off  off off  off &
./Build.sh mpich:5.0.1g-mt-ofi-eng         mpich5    off   off  on   off off  off &
./Build.sh mpich:5.0.1g-mt-ofi-eng         mpich5    off   on   off  off off  off &
./Build.sh mpich:5.0.1g-mt-ofi-eng         mpich5    off   on   on   off off  off &

./Build.sh mvapich:4.1g-mt-ofi-eng         mvapich4  off   off  off  off off off &
./Build.sh mvapich:4.1g-mt-ofi-eng         mvapich4  off   off  on   off off off &
./Build.sh mvapich:4.1g-mt-ofi-eng         mvapich4  off   on   off  off off off &
./Build.sh mvapich:4.1g-mt-ofi-eng         mvapich4  off   on   on   off off off &


wait
