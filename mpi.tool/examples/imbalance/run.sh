#!/bin/bash

compiler=oneAPI:2026.1.0
export mpi_type=mpich:5.0.1x-mt-ofi-eng
export libfabric_eng='-eng'
#export mpi_type=mpich:4.3.2x-mt-ofi
. env.sh
set -x

LD_PRELOAD=../../lib/intel/libtractor.mpich5.eng.so 
LD_PRELOAD="$LD_PRELOAD:../../lib/intel/libtractor_fab_ofi.so"


export PEAT_TRACTOR_PRINT_FUNC_NAMES=1
export PEAT_TRACTOR_STORE_MAP=1
export PEAT_TRACTOR_COLLECT_FAB=1
export PEAT_TRACTOR_FAB_LAYER=ofi
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
IFS=: read -ra _preload_libs <<< "$LD_PRELOAD"
ldd "${_preload_libs[@]}"
LD_PRELOAD=" -genv LD_PRELOAD $LD_PRELOAD "
fi

$MPICC -O2 -g -o alltoall alltoall.c
#$MPICC -O2 -g -o repro_final  repro_final.c

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
export FI_PROVIDER=shm
#export I_MPI_SPIN_COUNT=1
export HYDRA_TOPO_DEBUG=1
#export FI_LOG_LEVEL=debug
#GDB=gdb.sh
#export FI_LOG_PROV=shm
#export FI_HMEM=none
export MPICH_POSIX_NUM_COLLS_THRESHOLD=9999 # should be quite big but this one is enought for the example

mpiexec.hydra -launcher ssh -genvall -bind-to hwthread -map-by core  $LD_PRELOAD $PBS_NF -ppn $ppn -np $ranks $GDB ./alltoall $nnodes $ppn 4194304 10 #24
rm -f alltoall

#mpiexec.hydra -launcher ssh -genvall -bind-to hwthread -map-by core  $LD_PRELOAD $PBS_NF -ppn $ppn -np $ranks $GDB ./repro_final 



