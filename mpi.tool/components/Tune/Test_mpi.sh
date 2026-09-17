#!/bin/sh
#export compver=2019u3
#export ENG_MPI_BUILD=yes
#export mpi_type=mvapich:2.3.5-st-ib-verbs-eng
#export mpi_type=mvapich:2.3.5x-st-ib-verbs-eng 
#export mpi_type=mvapich:4.1x-mt-ucx-eng
#export mpi_type=mpich:4.3.2x-mt-ucx-eng
#export mpi_type=mvapich:4.1x-mt-ucx-eng
#export mpi_type=mvapich:4.1x-mt-ucx
#export mpi_type=impi:2019.7.217
#export mpi_type=impi:2019.3.199
. env.sh
. slurm2pbs.sh
ulimit -c unlimited                  

#$LD_PRELOAD=../../lib/libtractor_dbg.mvapich2.ct.so

#LD_PRELOAD=../../lib/libtractor_dbg.mvapich2.verbs.ct.so

#LD_PRELOAD=../../lib/libtractor.mvapich2.psm2.ct.so
#LD_PRELOAD=../../lib/libtractor_dbg.mvapich2.verbs.ct.so
#LD_PRELOAD=../../lib/libtractor.mvapich2.verbs.ct.so
#LD_PRELOAD=../../lib/libtractor.mvapich4.ct.so 
#LD_PRELOAD=../../lib/libtractor.mpich4.ct.so 
#LD_PRELOAD=../../lib/libtractor_dbg.mpich4.ct.so 
#LD_PRELOAD=../../lib/libtractor.mvapich4.so 

#LD_PRELOAD=../../lib/libtractor-lite.mvapich2.ct.so 
LD_PRELOAD=../../lib/libtractor_dbg.impi.so 
#LD_PRELOAD=../../lib/libtractor.impi.so 
#LD_PRELOAD=../../lib/libtractor_dbg.impi.so 
#LD_PRELOAD=../../lib/libtractor.impi.ct.ompt.so 
#LD_PRELOAD=/panfs/projects/innl/tools/peat/2025-06-27/lib/libtractor.impi.so 
#export LD_LIBRARY_PATH=/global/panfs02/innl/aknyaze1/cvs/peat.dev/misc.lib/libze:$LD_LIBRARY_PATH
#export PEAT_TRACTOR_PRINT_ALL_STAT=1
export PEAT_TRACTOR_PRINT_FUNC_NAMES=1
export PEAT_TRACTOR_STORE_MAP=1
export PEAT_TRACTOR_USE_SIGNALS=1
export PEAT_TRACTOR_WRITE_TRACE=1
export PEAT_TRACTOR_PARSE_OPENMP=1
export PEAT_TRACTOR_MAX_THREADS=20
export I_MPI_DEBUG=4
export HFI_BACKTRACE=1
#export I_MPI_PINPROC=2,3
#export I_MPI_SSHM=on
export KMP_AFFINITY=compact,granularity=fine,verbose
export PEAT_TRACTOR_CORRECT_EVENTS=1
#export PEAT_TRACTOR_ONE_TIME_ALLOC=100
export PEAT_TRACTOR_RETURN_LEVELS=0 #,2,4,5,6
#export PEAT_TRACTOR_PCONTROL=1
#export PEAT_TRACTOR_PCONTROL_FUNCS=MPI_SEND_INIT,MPI_RECV_INIT
#export PEAT_TRACTOR_PC_LEVELS=1,2,3
export PEAT_TRACTOR_SYSTIMER=0
export I_MPI_JOB_ABORT_SIGNAL=15
#export I_MPI_PINPROC=scatter
#export KMP_AFFINITY=compact,verbose
#export KMP_IGNORE_MPPBEG=false
#mpiexec -n 4 set.sh
export OMP_NUM_THREADS=4
rm -f *tractor_t* core.*

if [ -n "$PBS_NODEFILE" ] ; then
HOSTFILE="-machinefile $PBS_NODEFILE "

NPROCS=`cat $PBS_NODEFILE | wc -l` 

export I_MPI_FABRICS=shm:ofi
#export I_MPI_OFI_PROVIDER=psm2
export PSM2_IDENTIFY=1
export I_MPI_FABRICS=shm,ofi
export OMP_NUM_THREADS=4
#export PSM2_RCVTHREAD=0
else 
NPROCS=8
export I_MPI_FABRICS=shm
export OMP_NUM_THREADS=4
fi
export MV2_BCAST_HWLOC_TOPOLOGY=0
export MV2_USE_SHMEM_COLL=0
export MV2_USE_SHMEM_BCAST=0
export MV2_USE_SHMEM_REDUCE=0
export MV2_USE_SHMEM_BARRIER=0
export MV2_USE_SHMEM_ALLREDUCE=0
if [ -n "$LD_PRELOAD" ] ; then
ldd $LD_PRELOAD
echo +++
LD_PRELOAD="-genv LD_PRELOAD $LD_PRELOAD"
fi
##GDB=gdb.sh
#mpiexec.hydra $HOSTFILE -genvall $LD_PRELOAD -n $NPROCS ./gdb.sh ./test_pet 2>test.err 1>test.log 
#mpiexec.hydra $HOSTFILE -genvall $LD_PRELOAD -n $NPROCS ./test_pet 2>test.err 1>test.log 
mpiexec.hydra $HOSTFILE -genvall $LD_PRELOAD -n $NPROCS $GDB ./test_mpi_c
#mpiexec.hydra $HOSTFILE -genvall $LD_PRELOAD -n $NPROCS ./test_mpi_f
#mpiexec.hydra $HOSTFILE -genvall  $LD_PRELOAD -n $NPROCS  `pwd`/src/test_mpi_f
