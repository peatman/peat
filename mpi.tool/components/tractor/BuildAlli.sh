#!/bin/sh

# EXAMPLE !!! of build(all) script 

export compiler=oneAPI:2026.1.0 # for environment script
export COMPILER=intel #for make

rm -f Build.${COMPILER}.log
rm -f Build.${COMPILER}.dir

#do_not_remove=1 ./Build.sh mvapich:2.3.5x-mt-ib-verbs-eng  mvapich2 off   on   off  off off off 
#do_not_remove=1 ./Build.sh mvapich:2.3.5x-mt-ib-verbs      mvapich2 off   on   off  off off off 
export TRANSPORT=on
#do_not_remove=1 ./Build.sh mvapich:2.3.5x-mt-ib-verbs-eng  mvapich2 off   on   off  off off off 
#do_not_remove=1 ./Build.sh mvapich:2.3.5x-mt-ib-verbs      mvapich2 off   on   off  off off off 
#exit

#do_not_remove=1 ./Build.sh mvapich:2.3.5x-st-ib-verbs-eng  mvapich2 off   off  off  off off off 
#do_not_remove=1 ./Build.sh mvapich:4.1x-mt-ofi-eng         mvapich4 off   off   off  off off off 
#do_not_remove=1 ./Build.sh mpich:4.3.2x-mt-ofi-eng         mpich4    off   on   on   off off  on 
#do_not_remove=1 ./Build.sh mvapich:4.1x-mt-ofi-eng         mvapich4  off   on  off  off off  off 
#do_not_remove=1 ./Build.sh mpich:4.3.2x-mt-ofi-eng         mpich4    off   on  off  off off  off 
#do_not_remove=1 ./Build.sh mpich:5.0.2x-mt-ofi-eng         mpich5    off   on  off  off off  off
#do_not_remove=1 ./Build.sh  mpich:5.0.2x-mt-ofi-eng        transport_layers off   off  off  off off off
#do_not_remove=1 ./Build.sh  mpich:5.0.2x-mt-ofi-eng        mpich5     off   off  off  off off off
#export TRANSPORT=on
#do_not_remove=1 ./Build.sh  mpich:5.0.2x-mt-ofi-eng        mpich5   off   off  off  off off off

#exit
# build transport layers (mpi doesn't matter here so use mpich) 
#do_not_remove=1 ./Build.sh  mpich:5.0.2x-mt-ofi-eng          transport_layers off   off  off  off off off
#do_not_remove=1 ./Build.sh  mpich:5.0.2x-mt-ofi-eng          transport_layers off   on  off  off off off
#do_not_remove=1 ./Build.sh mpich:5.0.2x-mt-ofi-eng           mpich5     off   on  off  off off off
#do_not_remove=1 ./Build.sh openmpi:5.0.10x-eng               openmpi5   off   on  off  off off off 
#do_not_remove=1 ./Build.sh openmpi:5.0.10x                   openmpi5   off   on  off  off off off 
#do_not_remove=1 ./Build.sh mvapich:4.1x-mt-ofi-eng         mvapich4  off   on   off  off off off

#exit

./Build.sh  mpich:5.0.2x-mt-ofi-eng     transport_layers off   off  off  off off off &
./Build.sh  mpich:5.0.2x-mt-ofi-eng     transport_layers off   on   off  off off off &

unset TRANSPORT

# transport layer and colletive transfers are explicitly turned off for Intel MPI (no source in public) 
#          mpi_type                         target  mem  diag  GPU  buf v2p  hwc  
./Build.sh impi                               impi  off   off  off  off off off &
#./Build.sh impi                               impi  off   off  on   off off off &
./Build.sh impi                               impi  off   on   off  off off off &
#./Build.sh impi                               impi  off   on   on   off off off &


./Build.sh openmpi:5.0.10x                  openmpi5   off   off  off  off off off &
./Build.sh openmpi:5.0.10x                  openmpi5   off   on   off  off off off &

./Build.sh openmpi:5.0.10x-eng              openmpi5   off   off  off  off off off &
./Build.sh openmpi:5.0.10x-eng              openmpi5   off   on   off  off off off &

# export ENG_MPI_BUILD=on
#          1                                  2       3    4     5    6   7    8 
#       mpi_type                             target  mem  diag  GPU  buf v2p  hwc  
#exit
# ./Build.sh impi                               impi   off   off  off  off off off &
# ./Build.sh impi                               impi   off   off  on   off off off &
# ./Build.sh impi                               impi   off   on   off  off off off &
# ./Build.sh impi                               impi   off   on   on   off off off &
# unset ENG_MPI_BUILD


# orinary mvapich2 builds (no ct and transport) 
 ./Build.sh mvapich:2.3.5x-mt-ib-verbs       mvapich2 off   off  off  off off off &
# ./Build.sh mvapich:2.3.5x-mt-ib-verbs       mvapich2 off   off  on   off off off &
 ./Build.sh mvapich:2.3.5x-mt-ib-verbs       mvapich2 off   on   off  off off off &
# ./Build.sh mvapich:2.3.5x-mt-ib-verbs       mvapich2 off   on   on   off off off &

#          1                                  2       3    4     5    6   7    8 
#       mpi_type                           target    mem  diag  GPU  buf v2p  hwc  

# orinary mvapich4 builds (no ct and transport) 

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

export TRANSPORT=on

# ct and transport (SMPI/Verbs/psm2 combined) mvapich2 builds 

 ./Build.sh mvapich:2.3.5x-mt-ib-verbs-eng   mvapich2 off   off  off  off off off &
# ./Build.sh mvapich:2.3.5x-mt-ib-verbs-eng   mvapich2 off   off  on   off off off &
 ./Build.sh mvapich:2.3.5x-mt-ib-verbs-eng   mvapich2 off   on   off  off off off &
# ./Build.sh mvapich:2.3.5x-mt-ib-verbs-eng   mvapich2 off   on   on   off off off &


# ct and transport (psm2) mvapich2 builds 
# ./Build.sh mvapich:2.3.5x-st-opa-psm2-eng   mvapich2 off   off  off  off off off &
# ./Build.sh mvapich:2.3.5x-st-opa-psm2-eng   mvapich2 off   off  on   off off off &
# ./Build.sh mvapich:2.3.5x-st-opa-psm2-eng   mvapich2 off   on   off  off off off &
# ./Build.sh mvapich:2.3.5x-st-opa-psm2-eng   mvapich2 off   on   on   off off off &

# engineering mpich4 builds (ct and transport) 

./Build.sh mpich:4.3.2x-mt-ofi-eng         mpich4    off   off  off  off off  off &
#./Build.sh mpich:4.3.2x-mt-ofi-eng         mpich4    off   off  on   off off  off &
./Build.sh mpich:4.3.2x-mt-ofi-eng         mpich4    off   on   off  off off  off &
#./Build.sh mpich:4.3.2x-mt-ofi-eng         mpich4    off   on   on   off off  off &

# engineering mpich5 builds (ct and transport) 

./Build.sh mpich:5.0.2x-mt-ofi-eng         mpich5    off   off  off  off off  off &
#./Build.sh mpich:5.0.2x-mt-ofi-eng         mpich5    off   off  on   off off  off &
./Build.sh mpich:5.0.2x-mt-ofi-eng         mpich5    off   on   off  off off  off &
#./Build.sh mpich:5.0.2x-mt-ofi-eng         mpich5    off   on   on   off off  off &


./Build.sh mvapich:4.1x-mt-ofi-eng         mvapich4  off   off  off  off off off &
#./Build.sh mvapich:4.1x-mt-ofi-eng         mvapich4  off   off  on   off off off &
./Build.sh mvapich:4.1x-mt-ofi-eng         mvapich4  off   on   off  off off off &
#./Build.sh mvapich:4.1x-mt-ofi-eng         mvapich4  off   on   on   off off off &

wait
