#!/bin/sh

#export mpi_type=mvapich:4.1x-mt-ucx
export mpi_type=mpich:4.3.2x-mt-sock-eng
#export compiler=oneAPI:2023.2.0.49397
export compiler=oneAPI:2025.2.1
#export compiler=oneAPI:2025.2.0
. env.sh
#export ENG_MPI_BUILD=yes

#mpicc -o test_rdtsc -debug extended -fno-omit-frame-pointer -O2  -g -debug extended test_rdtsc.c
mpiicx -cc=icx -fiopenmp -fopenmp-targets=spir64 -o tune_pet -debug extended -fno-omit-frame-pointer -O2   tune_pet.c
exit
#mpicc -o tune_pet_hex -debug extended -fno-omit-frame-pointer -O2  tune_pet_hex.c
#mpicc -o tune_pet2 -debug extended -fno-omit-frame-pointer -O2  tune_pet2.c

#icx -o test_pet -O0 -debug extended -fno-omit-frame-pointer -fno-inline test_pet.c 
#mpiicpx -o test_pet -debug extended -fno-omit-frame-pointer -O2  -pthread -qopenmp test_pet.cpp 
mpif90 -o test_mpi_f -debug all -fno-omit-frame-pointer -O2  -qopenmp test_mpi.f 
mpicc  -o test_mpi_c -debug all -fno-omit-frame-pointer -O2  -qopenmp test_mpi.c 
#./test_pet
exit
#mpicc -o test_v -std=c++11 -debug extended -fno-omit-frame-pointer -O2  -I/global/panfs02/innl/aknyaze1/hwloc/1.11.13/include/ -L/global/panfs02/innl/aknyaze1/hwloc/1.11.13/lib -lhwloc test_v.cpp 
#gcc -shared -fPIC -o dt.x  -O2  dt.c 
gcc -shared -g -fPIC -o pcontrol.so  -O0  pcontrol.c -ldl

#export mpi_type=openmpi
#which mpicc
/global/panfs01/admin/opt/crtdc/openmpi/4.1.4/openmpi-4.1.4/bin/mpicc  -shared -fPIC -o pcontrol_openmpi.so  -O0 -g  pcontrol_mpi.c 

#export mpi_type=mvapich:2.3.5-st-ib-verbs-eng
#. env.sh
#which mpicc
/global/panfs01/admin/opt/crtdc/mvapich2/2.2-gcc/bin/mpicc -shared -fPIC -o pcontrol_mpich.so  -O0 -g  pcontrol_mpi.c 
