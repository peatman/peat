#!/bin/bash

compiler=oneAPI:2026.0.0.198
#export mpi_type=mvapich:4.1x-mt-sock-eng 
#export mpi_type=mpich:4.3.2x-mt-sock-eng 
export mpi_type=mpich:5.0.1x-mt-sock-eng
#export mpi_type=mpich:5.0.1g-mt-sock-eng
#export mpi_type=mpich:5.0.1x-mt-sock
. env.sh
set -x

#LD_PRELOAD=../../../../lib/intel/libtractor_dbg.mvapich4.ct.so 
#LD_PRELOAD=../../../../lib/intel/libtractor.mvapich4.ct.so 
#LD_PRELOAD=../../../../lib/intel/libtractor_dbg.mpich4.ct.so 
#LD_PRELOAD=../../../../lib/intel/libtractor.mpich5.so 
LD_PRELOAD=../../../../lib/intel/libtractor.mpich5.ct.so 
#LD_PRELOAD=../../../../lib/gnu/libtractor_dbg.mpich5.so 
#LD_PRELOAD=../../../../lib/intel/libtractor.mvapich4.so 
#LD_PRELOAD=../../../../lib/intel/libtractor.impi.so 


export PEAT_TRACTOR_PRINT_FUNC_NAMES=1
export PEAT_TRACTOR_STORE_MAP=1
export PEAT_TRACTOR_WRITE_TRACE=1
export I_MPI_DEBUG=4
#export PEAT_TRACTOR_COMPRESS_EVENTS=2
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

$MPICC -O2 -g -o alltoall alltoall.c
if [ -f "$PBS_NODEFILE" ]   ; then
nnodes=`sort -u $PBS_NODEFILE | wc -l`
ppn=2
PBS_NF="-f $PBS_NODEFILE"
else
nnodes=1
ppn=2
fi
ranks=$((nnodes * ppn))

export I_MPI_DEBUG=3
export I_MPI_FABRICS=shm
#export FI_PROVIDER=sock
#export I_MPI_SPIN_COUNT=1
export HYDRA_TOPO_DEBUG=1

mpiexec.hydra -launcher ssh -genvall -bind-to hwthread -map-by core  $LD_PRELOAD $PBS_NF -ppn $ppn -np $ranks ./compression.test 



