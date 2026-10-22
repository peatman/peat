#!/bin/sh

export compiler=gcc # for environment script
export COMPILER=gnu #for make

rm -f Build.${COMPILER}.log
rm -f Build.${COMPILER}.dir


#          mpi_type                         target  diag  GPU
./Build.sh impi                               impi  off  off &
# ./Build.sh impi                               impi  off  on &
./Build.sh impi                               impi  on  off &
# ./Build.sh impi                               impi  on  on &

# export ENG_MPI_BUILD=on
#          1                                  2       3     4
#       mpi_type                             target  diag  GPU

./Build.sh openmpi:5.0.10g                 openmpi5  off  off &
./Build.sh openmpi:5.0.10g                 openmpi5  on  off &

#          1                                  2       3     4
#       mpi_type                           target    diag  GPU

 ./Build.sh mpich:4.3.2g-mt-ofi             mpich4  off  off &
# ./Build.sh mpich:4.3.2g-mt-ofi             mpich4  off  on &
 ./Build.sh mpich:4.3.2g-mt-ofi             mpich4  on  off &
# ./Build.sh mpich:4.3.2g-mt-ofi             mpich4  on  on &


./Build.sh mpich:5.0.2g-mt-ofi             mpich5  off  off &
#./Build.sh mpich:5.0.2g-mt-ofi             mpich5  off  on &
./Build.sh mpich:5.0.2g-mt-ofi             mpich5  on  off &
#./Build.sh mpich:5.0.2g-mt-ofi             mpich5  on  on &


 ./Build.sh mvapich:4.1g-mt-ofi             mvapich4  off  off &
# ./Build.sh mvapich:4.1g-mt-ofi             mvapich4  off  on &
 ./Build.sh mvapich:4.1g-mt-ofi             mvapich4  on  off &
# ./Build.sh mvapich:4.1g-mt-ofi             mvapich4  on  on &


./Build.sh mpich:4.3.2g-mt-ofi-eng         mpich4  off  off &
# ./Build.sh mpich:4.3.2g-mt-ofi-eng         mpich4  off  on &
./Build.sh mpich:4.3.2g-mt-ofi-eng         mpich4  on  off &
# ./Build.sh mpich:4.3.2g-mt-ofi-eng         mpich4  on  on &

./Build.sh mpich:5.0.2g-mt-ofi-eng         mpich5  off  off &
# ./Build.sh mpich:5.0.2g-mt-ofi-eng         mpich5  off  on &
./Build.sh mpich:5.0.2g-mt-ofi-eng         mpich5  on  off &
# ./Build.sh mpich:5.0.2g-mt-ofi-eng         mpich5  on  on &

./Build.sh mvapich:4.1g-mt-ofi-eng         mvapich4  off  off &
# ./Build.sh mvapich:4.1g-mt-ofi-eng         mvapich4  off  on &
./Build.sh mvapich:4.1g-mt-ofi-eng         mvapich4  on  off &
# ./Build.sh mvapich:4.1g-mt-ofi-eng         mvapich4  on  on &


wait
