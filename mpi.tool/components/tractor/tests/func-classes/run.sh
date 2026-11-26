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
#   TOOLCHAIN=amd     AMD's LLVM compiler (ROCm amdclang), MPICH 5.0.2g, libraries from lib/amd
# e.g. TOOLCHAIN=gcc ./run.sh p2p 20 4. mpi_type overrides the MPICH, TRACTOR_LIBDIR=<dir> the library directory.
# The tests are rebuilt when the toolchain or the MPI changes.

test_name=${1:-all}
iterations=${2:-10}
ranks_arg=$3

toolchain=${TOOLCHAIN:-intel}
case $toolchain in
    intel ) compiler=oneAPI:2026.1.0 ; mpi_default=mpich:4.3.2x-mt-ofi-eng ; libsubdir=intel ;;
#    intel ) compiler=oneAPI:2026.1.0 ; mpi_default=mpich:5.0.2x-mt-ofi-eng ; libsubdir=intel ;;
#    intel ) compiler=oneAPI:2026.1.0 ; mpi_default=mvapich:4.1x-mt-ofi-eng ; libsubdir=intel ;;
    gcc )   compiler=gcc ; mpi_default=mpich:5.0.2g-mt-ofi-eng ; libsubdir=gnu ;;
    nvc )   compiler=${NVC_COMPILER:-nvc:12.9} ; mpi_default=mpich:5.0.2g-mt-ofi-eng ; libsubdir=nvc ;;
    clang ) compiler=clang ; mpi_default=mpich:5.0.2g-mt-ofi-eng ; libsubdir=clang ;;
    amd )   compiler=amd ; mpi_default=mpich:5.0.2g-mt-ofi-eng ; libsubdir=amd ;;
    * )     echo "unknown TOOLCHAIN=$toolchain (intel, gcc, nvc, clang or amd)" ; exit 2 ;;
esac
export compiler
export mpi_type=${mpi_type:-$mpi_default}
# an engineering MPI links the engineering libfabric, a stock one the stock libfabric
case $mpi_type in
    *-eng ) export libfabric_eng='-eng' ;;
    * )     export libfabric_eng='' ;;
esac
. env.sh
set -x
#TRACTOR=0
libdir=${TRACTOR_LIBDIR:-$(realpath ../../../../lib/$libsubdir)}
# the MPI layer follows the MPI in use, e.g. mpich:5.0.2x-mt-ofi-eng -> libtractor.mpich5.eng.so
# (env.sh has split it into mpi_type=mpich and mpi_version=5.0.2x-mt-ofi-eng)
case $mpi_type:$mpi_version in
    mpich:3.* )   tractor_target=mpich3 ;;
    mpich:4.* )   tractor_target=mpich4 ;;
    mpich:5.* )   tractor_target=mpich5 ;;
    mvapich:2.* ) tractor_target=mvapich2 ;;
    mvapich:4.* ) tractor_target=mvapich4 ;;
    openmpi:5.* ) tractor_target=openmpi5 ;;
    impi:* )      tractor_target=impi ;;
    * )           echo "no tractor library known for mpi_type=$mpi_type mpi_version=$mpi_version" ; exit 2 ;;
esac
case $mpi_version in
    *-eng ) tractor_target=$tractor_target.eng ;;
esac
if [ "${TRACTOR:-1}" != "0" ] ; then
LD_PRELOAD=$libdir/libtractor.$tractor_target.so # the MPI layer: it loads the core next to it
# the transport shims need an engineering tractor (TRANSPORT=on) and the engineering libfabric
case $mpi_version in
    *-eng ) LD_PRELOAD="$LD_PRELOAD:$libdir/libtractor.transport.ofi.so" ;;
esac
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
#export FI_PROVIDER=shm
export FI_PROVIDER=tcp
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
