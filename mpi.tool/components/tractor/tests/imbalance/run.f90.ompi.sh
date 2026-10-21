#!/bin/bash

compiler=oneAPI:2026.1.0
export mpi_type=openmpi:5.0.10x-eng
#export mpi_type=openmpi:5.0.10x
export libfabric_eng='-eng'
. env.sh
set -x

libdir=$(realpath ../../../../lib/intel)
if [ "${TRACTOR:-1}" != "0" ] ; then
LD_PRELOAD=$libdir/libtractor.openmpi5.eng.so
fi

export PEAT_TRACTOR_PRINT_FUNC_NAMES=1
export PEAT_TRACTOR_STORE_MAP=1
export PEAT_TRACTOR_COLLECT_FAB=1
export PEAT_TRACTOR_FAB_LAYER=ofi
export PEAT_TRACTOR_WRITE_TRACE=1
export I_MPI_DEBUG=4
export OMPI_FORTRAN_MPI_REDIRECT=1
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
IFS=: read -ra _preload_libs <<< "$LD_PRELOAD"
ldd "${_preload_libs[@]}"
LD_PRELOAD=" -x LD_PRELOAD=$LD_PRELOAD "
fi

$MPIFC -O2 -g -fpp -o alltoall alltoall.f90
#$MPICC -O2 -g -o repro_final  repro_final.c

if [ -f "$PBS_NODEFILE" ]   ; then
nnodes=`sort -u $PBS_NODEFILE | wc -l`
ppn=16
PBS_NF="-hostfile $PBS_NODEFILE"
else
nnodes=8
ppn=1
fi
ranks=$((nnodes * ppn))

#export FI_LOG_LEVEL=debug
#GDB=gdb.sh
#export FI_LOG_PROV=shm
#export FI_HMEM=none

mpirun $LD_PRELOAD $PBS_NF -np $ranks $GDB ./alltoall $nnodes $ppn 4194304 10 #24
rm -f alltoall




