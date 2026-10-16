#!/bin/sh

# EXAMPLE !!! of build(all) script 

export compiler=oneAPI:2026.1.0 # for environment script
export COMPILER=intel #for make

rm -f Build.${COMPILER}.log
rm -f Build.${COMPILER}.dir


#do_not_remove=1 ./Build.sh mvapich:4.1x-mt-ofi-eng         mvapich4 off   off   off  off off off 
#do_not_remove=1 ./Build.sh mpich:4.3.2x-mt-ofi-eng         mpich4    off   on   on   off off  on 
#do_not_remove=1 ./Build.sh mvapich:4.1x-mt-ofi-eng         mvapich4  off   on  off  off off  off 
#do_not_remove=1 ./Build.sh mpich:4.3.2x-mt-ofi-eng         mpich4    off   on  off  off off  off 
#do_not_remove=1 ./Build.sh mpich:5.0.1x-mt-ofi-eng         mpich5    off   on  off  off off  off
#do_not_remove=1 ./Build.sh  mpich:5.0.1x-mt-ofi-eng        mpich5     off   off  off  off off off
#do_not_remove=1 ./Build.sh  mpich:5.0.1x-mt-ofi-eng        mpich5   off   off  off  off off off

#exit
#do_not_remove=1 ./Build.sh mpich:5.0.1x-mt-ofi-eng           mpich5     off   on  off  off off off
#do_not_remove=1 ./Build.sh openmpi:5.0.10x                   openmpi5   off   on  off  off off off 
#do_not_remove=1 ./Build.sh mvapich:4.1x-mt-ofi-eng         mvapich4  off   on   off  off off off

#exit

#          mpi_type                         target  mem  diag  GPU  buf v2p  hwc  
./Build.sh impi                               impi  off   off  off  off off off &
./Build.sh impi                               impi  off   on   off  off off off &


./Build.sh openmpi:5.0.10x                  openmpi5   off   off  off  off off off &
./Build.sh openmpi:5.0.10x                  openmpi5   off   on   off  off off off &



#          1                                  2       3    4     5    6   7    8 
#       mpi_type                           target    mem  diag  GPU  buf v2p  hwc  

# orinary mvapich4 builds 

./Build.sh mpich:4.3.2x-mt-ofi             mpich4    off   off  off  off off  off &
#./Build.sh mpich:4.3.2x-mt-ofi             mpich4    off   off  on   off off  off &
./Build.sh mpich:4.3.2x-mt-ofi             mpich4    off   on   off  off off  off &
#./Build.sh mpich:4.3.2x-mt-ofi             mpich4    off   on   on   off off  off &

 ./Build.sh mvapich:4.1x-mt-ofi            mvapich4  off   off  off  off off off &
# ./Build.sh mvapich:4.1x-mt-ofi            mvapich4  off   off  on   off off off &
 ./Build.sh mvapich:4.1x-mt-ofi            mvapich4  off   on   off  off off off &
# ./Build.sh mvapich:4.1x-mt-ofi            mvapich4  off   on   on   off off off &


./Build.sh mpich:5.0.2x-mt-ofi             mpich5    off   off  off  off off  off &
#./Build.sh mpich:5.0.2x-mt-ofi             mpich5    off   off  on   off off  off &
./Build.sh mpich:5.0.2x-mt-ofi             mpich5    off   on   off  off off  off &
#./Build.sh mpich:5.0.2x-mt-ofi             mpich5    off   on   on   off off  off &



# engineering mpich4 builds  

./Build.sh mpich:4.3.2x-mt-ofi-eng         mpich4    off   off  off  off off  off &
#./Build.sh mpich:4.3.2x-mt-ofi-eng         mpich4    off   off  on   off off  off &
./Build.sh mpich:4.3.2x-mt-ofi-eng         mpich4    off   on   off  off off  off &
#./Build.sh mpich:4.3.2x-mt-ofi-eng         mpich4    off   on   on   off off  off &



./Build.sh mpich:5.0.2x-mt-ofi-eng         mpich5    off   off  off  off off  off &
#./Build.sh mpich:5.0.2x-mt-ofi-eng         mpich5    off   off  on   off off  off &
./Build.sh mpich:5.0.2x-mt-ofi-eng         mpich5    off   on   off  off off  off &
#./Build.sh mpich:5.0.2x-mt-ofi-eng         mpich5    off   on   on   off off  off &


./Build.sh mvapich:4.1x-mt-ofi-eng         mvapich4  off   off  off  off off off &
#./Build.sh mvapich:4.1x-mt-ofi-eng         mvapich4  off   off  on   off off off &
./Build.sh mvapich:4.1x-mt-ofi-eng         mvapich4  off   on   off  off off off &
#./Build.sh mvapich:4.1x-mt-ofi-eng         mvapich4  off   on   on   off off off &

wait
