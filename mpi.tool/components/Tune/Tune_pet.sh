#!/bin/sh
export compiler=oneAPI:latest
export ENG_MPI_BUILD=yes
#export mpi_type=mvapich:4.1g-mt-sock-eng 
export mpi_type=mvapich:4.1x-mt-sock-eng 
#export mpi_type=mpich:4.3.2x-mt-sock-eng
arguments="$@"
. env.sh
set -x
ulimit -c unlimited                  
LD_PRELOAD=../../lib/libtractor.mvapich4.ct.so 
#LD_PRELOAD=../../lib/libtractor_dbg.mvapich4.ct.so 
#LD_PRELOAD=../../lib/libtractor.impi.so 
#LD_PRELOAD=../../lib/libtractor_dbg.impi.so 
#export PEAT_TRACTOR_PRINT_ALL_STAT=1
export PEAT_TRACTOR_PRINT_FUNC_NAMES=1
#export PEAT_TRACTOR_STORE_MAP=1
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

if [ -n "$PBS_NODEFILE" ] ; then
if [ -z  "$NPROCS" ] ; then 
NPROCS=`cat $PBS_NODEFILE | wc -l` 
fi
cat $PBS_NODEFILE | sort -u >$PBS_NODEFILE.my
#cat $PBS_NODEFILE | head -1 >$PBS_NODEFILE.my
HOSTFILE="-machinefile $PBS_NODEFILE.my"
else 
NPROCS=4
fi
export OMP_NUM_THREADS=2

export I_MPI_FABRICS=shm

export MV2_BCAST_HWLOC_TOPOLOGY=0
export MV2_USE_SHMEM_COLL=0
export MV2_USE_SHMEM_BCAST=0
export MV2_USE_SHMEM_REDUCE=0
export MV2_USE_SHMEM_BARRIER=0
export MV2_USE_SHMEM_ALLREDUCE=0
export MV2_ENABLE_AFFINITY=1
export I_MPI_COLL_INTRANODE=pt2pt

#Turn it back on
#export MV2_USE_RDMA_FAST_PATH=0
#export MV2_USE_RMA_FAST_PATH=0
#export MV2_RDMA_FAST_PATH_BUF_SIZE=0
#export MV2_USE_COALESCE=0
#export MV2_USE_MCAST=0
#export MV2_USE_EAGER_FAST_SEND=0
#export MV2_USE_RDMA_ONE_SIDED=0

#export MV2_NUM_HCAS=2
#export MV2_USE_BLOCKING=1

#export MV2_NUM_CQES_PER_POLL=1

#export MV2_USE_ONLY_UD=1
#export MV2_USE_UD_HYBRID=1
#export MV2_SMP_USE_CMA=0

#export MV2_DEBUG_CORESIZE=9999999999
#export MV2_DEBUG_SHOW_BACKTRACE=1
#export MV2_DEBUG_SETUP_SIGHDLR=1
#export MV2_ASYNC_PROGRESS=1
#export MV2_ENABLE_PROGRESS_AFFINITY=0
export MV2_SHOW_CPU_BINDING=1
case "$TEST_TYPE" in
 "DDR") 
   export I_MPI_PIN_PROCESSOR_LIST=allcores:map=bunch
   export MV2_CPU_BINDING_POLICY=bunch
  ;;
 "QPI") 
   export I_MPI_PIN_PROCESSOR_LIST=allcores:map=scatter
   export MV2_CPU_BINDING_POLICY=scatter
  ;;
 "NIC") 
   export I_MPI_PIN_PROCESSOR_LIST=allcores:map=bunch
   #export MV2_CPU_MAPPING=24:48
   export MV2_CPU_BINDING_POLICY=bunch
  ;;
 *) 
   export I_MPI_PIN_PROCESSOR_LIST=bunch
   export MV2_CPU_BINDING_POLICY=allcores:map=bunch
  ;;
esac
echo TEST_TYPE is $TEST_TYPE, MV2_CPU_BINDING_POLICY=$MV2_CPU_BINDING_POLICY MV2_CPU_MAPPING=$MV2_CPU_MAPPING
if [ -n "" ] ; then
export MV2_CPU_BINDING_POLICY=hybrid 
export MV2_THREADS_PER_PROCESS=2
export OMP_NUM_THREADS=2
export MV2_HYBRID_BINDING_POLICY=compact
fi

 
if [ -n "$PET_SIM_REQS" ] ; then
export PET_SIM_REQS
else
export PET_SIM_REQS=1
fi
if [ -n "$LD_PRELOAD" ] ; then
ldd $LD_PRELOAD
LD_PRELOAD=" -genv LD_PRELOAD $LD_PRELOAD "
fi

if [ -n "$PET_SEND_DELAY" ] ; then
export PET_SEND_DELAY
fi

if [ -n "$PET_RECV_DELAY" ] ; then
export PET_RECV_DELAY
fi

if [ -n "$PET_WAIT_DELAY" ] ; then
export PET_WAIT_DELAY
fi

ulimit -c unlimited 
echo PET_SEND_DELAY=$PET_SEND_DELAY, PET_RECV_DELAY=$PET_RECV_DELAY , PET_WAIT_DELAY=$PET_WAIT_DELAY
which mpiexec.hydra

#GDB=./gdb.sh

ldd `pwd`/tune_pet
mpiexec.hydra -launcher ssh $HOSTFILE -genvall $LD_PRELOAD -n $NPROCS  $GDB `pwd`/tune_pet $arguments # 2>log.verbose

rm $PBS_NODEFILE.my