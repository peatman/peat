#!/bin/bash

compiler=oneAPI:2026.1.0
libfabric_eng='-eng'
export mpi_type=mpich:5.0.1x-mt-ofi-eng
. env.sh
set -x

LD_PRELOAD=../../../../lib/intel/libtractor.mpich5.eng.so 


export PEAT_TRACTOR_PRINT_FUNC_NAMES=1
export PEAT_TRACTOR_STORE_MAP=1
export PEAT_TRACTOR_WRITE_TRACE=1
export I_MPI_DEBUG=4
#export PEAT_TRACTOR_ONE_TIME_ALLOC=100
#export PEAT_TRACTOR_USE_REALTIME_DUMP=1
export PEAT_TRACTOR_RETURN_LEVELS=0 #,2,4,5,6
export PEAT_TRACTOR_TMP_DIR="/tmp"
#export PEAT_TRACTOR_ONDEMAND=0
#export PEAT_TRACTOR_PC_LEVELS=1,2,3
#export I_MPI_PINPROC=scatter
#export KMP_IGNORE_MPPBEG=false
rm -f *tractor_t* #core.*

if [ -n "$LD_PRELOAD" ] ; then
ldd $LD_PRELOAD
LD_PRELOAD=" -genv LD_PRELOAD $LD_PRELOAD "
fi

#$MPICC -O2 -g -o alltoall alltoall.c
$MPIFC -O2 -g -fpp -o alltoall alltoall.f90

if [ -f "$PBS_NODEFILE" ]   ; then
nnodes=`sort -u $PBS_NODEFILE | wc -l`
ppn=16
PBS_NF="-f $PBS_NODEFILE"
else
nnodes=8
ppn=1
fi
ranks=$((nnodes * ppn))

export I_MPI_DEBUG=3
export I_MPI_FABRICS=shm
#export FI_PROVIDER=sock
#export I_MPI_SPIN_COUNT=1
export HYDRA_TOPO_DEBUG=1
export MPICH_POSIX_NUM_COLLS_THRESHOLD=9999 # should be quite big but this one is enought for the example
export MPICH_FORTRAN_MPI_REDIRECT=1

mpiexec.hydra -launcher ssh -genvall -bind-to hwthread -map-by core  $LD_PRELOAD $PBS_NF -ppn $ppn -np $ranks ./alltoall $nnodes $ppn 4194304 10 #24

rm -f alltoall 

