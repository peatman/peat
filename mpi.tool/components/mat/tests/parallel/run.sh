#!/bin/bash
# Build traffic_test and record its trace under tractor.
#   ./run.sh [iterations] [ranks] [trace dir]      default: 20 iterations, 8 ranks, trace.traffic
# Same toolchain selection as tractor/tests/func-classes/run.sh (TOOLCHAIN=intel|gcc|nvc|clang, mpi_type,
# TRACTOR_LIBDIR); TRACTOR=0 runs the test without the tractor preload.
# Then compare serial mat with parallel mat on the trace: ./compare.sh <trace dir>

iterations=${1:-20}
ranks=${2:-8}
trace_dir=${3:-trace.traffic}

toolchain=${TOOLCHAIN:-intel}
case $toolchain in
    intel ) compiler=oneAPI:2026.1.0 ; mpi_default=mvapich:4.1x-mt-ofi-eng ; libsubdir=intel ; tlib=libtractor.mvapich4.eng.so ;;
    gcc )   compiler=gcc ; mpi_default=mpich:5.0.2g-mt-ofi-eng ; libsubdir=gnu ; tlib=libtractor.mpich5.eng.so ;;
    nvc )   compiler=${NVC_COMPILER:-nvc:12.9} ; mpi_default=mpich:5.0.2g-mt-ofi-eng ; libsubdir=nvc ; tlib=libtractor.mpich5.eng.so ;;
    clang ) compiler=clang ; mpi_default=mpich:5.0.2g-mt-ofi-eng ; libsubdir=clang ; tlib=libtractor.mpich5.eng.so ;;
    * )     echo "unknown TOOLCHAIN=$toolchain (intel, gcc, nvc or clang)" ; exit 2 ;;
esac
here=$(cd "$(dirname "$0")" && pwd)
export compiler
export mpi_type=${mpi_type:-$mpi_default}
export libfabric_eng='-eng'
. env.sh
set +x
libdir=${TRACTOR_LIBDIR:-$(realpath "$here/../../../../lib/$libsubdir")}
preload=
if [ "${TRACTOR:-1}" != "0" ] ; then
    preload="-genv LD_PRELOAD $libdir/${TRACTOR_LIB:-$tlib}:$libdir/libtractor_transport_ofi.so"
fi

export PEAT_TRACTOR_PRINT_FUNC_NAMES=1
export PEAT_TRACTOR_STORE_MAP=1
export PEAT_TRACTOR_COLLECT_TRANSPORT=1
export PEAT_TRACTOR_TRANSPORT_LAYER=ofi
export PEAT_TRACTOR_WRITE_TRACE=1
export PEAT_TRACTOR_RETURN_LEVELS=0
export PEAT_TRACTOR_TMP_DIR="/tmp"
export FI_PROVIDER=tcp
export MPICH_POSIX_NUM_COLLS_THRESHOLD=9999

# rebuild when the compiler or the MPI changes
built_with="$toolchain $compiler $mpi_type $mpi_version"
if [ "$(cat "$here/.built_with" 2>/dev/null)" != "$built_with" ] ; then
    make -C "$here" clean
fi
make -C "$here" MPICC=$MPICC || exit 1
echo "$built_with" > "$here/.built_with"

rm -rf "$trace_dir" && mkdir -p "$trace_dir"
cd "$trace_dir" && mpiexec.hydra -launcher ssh -genvall -bind-to hwthread -map-by core $preload -ppn "$ranks" -np "$ranks" "$here/traffic_test" "$iterations"
