#!/bin/bash
# Build and run the per-class MPI function tests under tractor.
#   ./run.sh [test|all] [iterations] [ranks]
# e.g. ./run.sh p2p 20 4      ./run.sh all 5
# Set TRACTOR=0 to run the plain tests without preloading the tractor library.
# The toolchain is selected with TOOLCHAIN (default intel), the same script serves all of them:
#   TOOLCHAIN=intel   oneAPI compilers, MPICH 5.0.2x, libraries from lib/intel
#   TOOLCHAIN=gcc     GCC, MPICH 5.0.2g, libraries from lib/gnu
#   TOOLCHAIN=nvc     NVHPC (NVC_COMPILER, default nvc:12.9), MPICH 5.0.2g, libraries from lib/nvc
#   TOOLCHAIN=clang   system clang, MPICH 5.0.2g, libraries from lib/clang
# e.g. TOOLCHAIN=gcc ./run.sh p2p 20 4. mpi_type overrides the MPICH, TRACTOR_LIBDIR=<dir> the library directory.
# The tests are rebuilt when the toolchain or the MPI changes.

test_name=${1:-all}
iterations=${2:-10}
ranks_arg=$3

toolchain=${TOOLCHAIN:-intel}
case $toolchain in
    intel ) compiler=oneAPI:2026.1.0 ; mpi_default=mpich:5.0.2x-mt-ofi-eng ; libsubdir=intel ;;
    gcc )   compiler=gcc ; mpi_default=mpich:5.0.2g-mt-ofi-eng ; libsubdir=gnu ;;
    nvc )   compiler=${NVC_COMPILER:-nvc:12.9} ; mpi_default=mpich:5.0.2g-mt-ofi-eng ; libsubdir=nvc ;;
    clang ) compiler=clang ; mpi_default=mpich:5.0.2g-mt-ofi-eng ; libsubdir=clang ;;
    * )     echo "unknown TOOLCHAIN=$toolchain (intel, gcc, nvc or clang)" ; exit 2 ;;
esac
export compiler
export mpi_type=${mpi_type:-$mpi_default}
export libfabric_eng='-eng'
. env.sh
set -x
#TRACTOR=0
libdir=${TRACTOR_LIBDIR:-$(realpath ../../../../lib/$libsubdir)}
if [ "${TRACTOR:-1}" != "0" ] ; then
LD_PRELOAD=$libdir/libtractor.mpich5.eng.so
LD_PRELOAD="$LD_PRELOAD:$libdir/libtractor_transport_ofi.so"
fi

export PEAT_TRACTOR_PRINT_FUNC_NAMES=1
export PEAT_TRACTOR_STORE_MAP=1
export PEAT_TRACTOR_COLLECT_TRANSPORT=1
export PEAT_TRACTOR_TRANSPORT_LAYER=ofi
export PEAT_TRACTOR_WRITE_TRACE=1
export PEAT_TRACTOR_RETURN_LEVELS=0
export PEAT_TRACTOR_TMP_DIR="/tmp"
#export PEAT_TRACTOR_RESOLVE=0
if [ -n "$LD_PRELOAD" ] ; then
IFS=: read -ra _preload_libs <<< "$LD_PRELOAD"
ldd "${_preload_libs[@]}"
LD_PRELOAD=" -genv LD_PRELOAD $LD_PRELOAD "
fi

# the test programs depend on the compiler and the MPI, rebuild them when those change
built_with="$toolchain $compiler $mpi_type $mpi_version"
if [ "$(cat .built_with 2>/dev/null)" != "$built_with" ] ; then
make clean
fi
make MPICC=$MPICC || exit 1
echo "$built_with" > .built_with

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
