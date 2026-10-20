#!/bin/bash
# Build and run the per-class MPI function tests under tractor.
#   ./run.sh [test|all] [iterations] [ranks]
# e.g. ./run.sh p2p 20 4      ./run.sh all 5
# Set TRACTOR=0 to run the plain tests without preloading the tractor library.

test_name=${1:-all}
iterations=${2:-10}
ranks_arg=$3

compiler=oneAPI:2026.1.0
export mpi_type=${mpi_type:-mpich:5.0.2x-mt-ofi-eng}
export libfabric_eng='-eng'
. env.sh
set -x
#TRACTOR=0
libdir=$(realpath ../../../../lib/intel)
if [ "${TRACTOR:-1}" != "0" ] ; then
LD_PRELOAD=$libdir/libtractor.mpich5.eng.so
fi

export PEAT_TRACTOR_PRINT_FUNC_NAMES=1
export PEAT_TRACTOR_STORE_MAP=1
export PEAT_TRACTOR_COLLECT_FAB=1
export PEAT_TRACTOR_FAB_LAYER=ofi
export PEAT_TRACTOR_WRITE_TRACE=1
export PEAT_TRACTOR_RETURN_LEVELS=0
#export PEAT_TRACTOR_TMP_DIR="/tmp"
#export PEAT_TRACTOR_RESOLVE=0
if [ -n "$LD_PRELOAD" ] ; then
IFS=: read -ra _preload_libs <<< "$LD_PRELOAD"
ldd "${_preload_libs[@]}"
LD_PRELOAD=" -genv LD_PRELOAD $LD_PRELOAD "
fi

make MPICC=$MPICC || exit 1

if [ -f "$PBS_NODEFILE" ]   ; then
nnodes=`sort -u $PBS_NODEFILE | wc -l`
ppn=4
PBS_NF="-f $PBS_NODEFILE"
else
nnodes=1
ppn=${ranks_arg:-4}
fi
ranks=$((nnodes * ppn))

export I_MPI_DEBUG=3
export I_MPI_FABRICS=shm
export FI_PROVIDER=shm
export HYDRA_TOPO_DEBUG=1
export MPICH_POSIX_NUM_COLLS_THRESHOLD=9999

if [ "$test_name" == "all" ] ; then
tests="p2p coll comm rma fio"
else
tests="$test_name"
fi

status=0
for t in $tests ; do
rm -rf trace.$t
mkdir -p trace.$t
( cd trace.$t && mpiexec.hydra -launcher ssh -genvall -bind-to hwthread -map-by core $LD_PRELOAD $PBS_NF -ppn $ppn -np $ranks $GDB ../${t}_test $iterations ) || status=1
done
exit $status
