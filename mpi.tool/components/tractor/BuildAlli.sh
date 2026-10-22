#!/bin/sh

# EXAMPLE !!! of build(all) script 

export compiler=oneAPI:2026.1.0 # for environment script
export COMPILER=intel #for make

rm -f Build.${COMPILER}.log
rm -f Build.${COMPILER}.dir


#do_not_remove=1 ./Build.sh mvapich:4.1x-mt-ofi-eng         mvapich4  off  off
#do_not_remove=1 ./Build.sh mpich:4.3.2x-mt-ofi-eng         mpich4  on  on
#do_not_remove=1 ./Build.sh mvapich:4.1x-mt-ofi-eng         mvapich4  on  off
#do_not_remove=1 ./Build.sh mpich:4.3.2x-mt-ofi-eng         mpich4  on  off
#do_not_remove=1 ./Build.sh mpich:5.0.1x-mt-ofi-eng         mpich5  on  off
#do_not_remove=1 ./Build.sh  mpich:5.0.1x-mt-ofi-eng        mpich5  off  off
#do_not_remove=1 ./Build.sh  mpich:5.0.1x-mt-ofi-eng        mpich5  off  off

#exit
#do_not_remove=1 ./Build.sh mpich:5.0.1x-mt-ofi-eng           mpich5  on  off
#do_not_remove=1 ./Build.sh openmpi:5.0.10x                   openmpi5  on  off
#do_not_remove=1 ./Build.sh mvapich:4.1x-mt-ofi-eng         mvapich4  on  off

#exit

#          mpi_type                         target  diag  GPU
./Build.sh impi                               impi  off  off &
./Build.sh impi                               impi  on  off &


./Build.sh openmpi:5.0.10x                  openmpi5  off  off &
./Build.sh openmpi:5.0.10x                  openmpi5  on  off &



#          1                                  2       3     4
#       mpi_type                           target    diag  GPU

# orinary mvapich4 builds 

./Build.sh mpich:4.3.2x-mt-ofi             mpich4  off  off &
#./Build.sh mpich:4.3.2x-mt-ofi             mpich4  off  on &
./Build.sh mpich:4.3.2x-mt-ofi             mpich4  on  off &
#./Build.sh mpich:4.3.2x-mt-ofi             mpich4  on  on &

 ./Build.sh mvapich:4.1x-mt-ofi            mvapich4  off  off &
# ./Build.sh mvapich:4.1x-mt-ofi            mvapich4  off  on &
 ./Build.sh mvapich:4.1x-mt-ofi            mvapich4  on  off &
# ./Build.sh mvapich:4.1x-mt-ofi            mvapich4  on  on &


./Build.sh mpich:5.0.2x-mt-ofi             mpich5  off  off &
#./Build.sh mpich:5.0.2x-mt-ofi             mpich5  off  on &
./Build.sh mpich:5.0.2x-mt-ofi             mpich5  on  off &
#./Build.sh mpich:5.0.2x-mt-ofi             mpich5  on  on &



# engineering mpich4 builds  

./Build.sh mpich:4.3.2x-mt-ofi-eng         mpich4  off  off &
#./Build.sh mpich:4.3.2x-mt-ofi-eng         mpich4  off  on &
./Build.sh mpich:4.3.2x-mt-ofi-eng         mpich4  on  off &
#./Build.sh mpich:4.3.2x-mt-ofi-eng         mpich4  on  on &



./Build.sh mpich:5.0.2x-mt-ofi-eng         mpich5  off  off &
#./Build.sh mpich:5.0.2x-mt-ofi-eng         mpich5  off  on &
./Build.sh mpich:5.0.2x-mt-ofi-eng         mpich5  on  off &
#./Build.sh mpich:5.0.2x-mt-ofi-eng         mpich5  on  on &


./Build.sh mvapich:4.1x-mt-ofi-eng         mvapich4  off  off &
#./Build.sh mvapich:4.1x-mt-ofi-eng         mvapich4  off  on &
./Build.sh mvapich:4.1x-mt-ofi-eng         mvapich4  on  off &
#./Build.sh mvapich:4.1x-mt-ofi-eng         mvapich4  on  on &

wait
