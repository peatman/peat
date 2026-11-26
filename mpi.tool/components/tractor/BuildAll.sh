#!/bin/bash
#
# Build every tractor library for one compiler, each in the plain (release) and the DIAG (_dbg) variant:
#
#   ./BuildAll.sh gnu|intel|nvidia|clang|amd [-j <parallel builds>]
#
# The libraries go to ../../lib/<compiler> (the logs: Build.<compiler>.log):
#   MPI independent
#     libtractor[_dbg].core.so                the core (no MPI): one for every MPI
#     libtractor[_dbg].transport.<api>.so     the transport shims (psm2, psm3, ucx, ofi)
#     libtractor.<class>.so                   the event-class shims: ompt (not with nvidia), acc (not with intel), opencl,
#                                             levelzero, cuda, llama; each one is built when its headers are found
#                                             (LLAMA_ROOT=<llama.cpp>, CUDA_ROOT=<CUDA toolkit>). One set: it works with
#                                             the core of either variant (DIAG too), so there is no _dbg one
#   MPI dependent
#     libtractor[_dbg].<mpi>[.eng].so         the MPI layer of an MPI (the .eng ones for the engineering, patched MPIs); it
#                                             loads the core next to it (or a core variant preloaded before it), so
#                                             preloading it alone is enough
# e.g. LD_PRELOAD=libtractor.mpich5.eng.so:libtractor.transport.ofi.so:libtractor.ompt.so
#
# The variants with MPI buffers (.mbs), virtual->physical addresses (.v2p), memory profiling (.mem<x>) and hardware
# counters are not built here: set MPIBUFFERS=on / VIRT2PHYS=on / MEMPROFILE=<x> / HWCOUNTERS=<x> in the environment (or run
# Build.sh by hand).
# amd is the clang family built with ROCm's amdclang (env.sh: compiler=amd[:<ROCm root>]); it has its own lib/amd, so it does not
# replace the libraries of the system clang. No OpenACC shim (no acc_prof.h).
# Build.sh takes the MPIs from $HOME/{mpich,mvapich,openmpi}/<version><g|x>[-mt-ofi][-eng] (env.sh): g for the MPIs built
# with GCC (used with every compiler but intel), x for the ones built with the Intel compilers.

cd "$(dirname "$0")" || exit 1
jobs=4
case "$1" in
    gnu)    export compiler=gcc ;               L=g ;;
    intel)  export compiler=oneAPI:2026.1.0 ;   L=x ;;
    nvidia) export compiler=nvc:12.9 ;          L=g ;; # CUDA 12.9: needed for Pascal GPUs
    clang)  export compiler=clang ;             L=g ;;
    amd)    export compiler=amd ;               L=g ;; # AMD's LLVM compiler (ROCm: amdclang, $ROCM_PATH or /opt/rocm); clang options, own lib/amd
    *) sed -n '3,6p' "$0" | sed 's/^# \{0,1\}//'; exit 2 ;;
esac
export COMPILER=$1
shift
[ "$1" = -j ] && jobs=$2

rm -f Build.${COMPILER}.log Build.${COMPILER}.dir
failed=$(mktemp)

# build <mpi_type[:version]> <target> <diag>: at most $jobs at a time
build() {
    while [ "$(jobs -rp | wc -l)" -ge "$jobs" ] ; do wait -n ; done
    ( ./Build.sh "$@" || echo "$*" >>$failed ) &
}

# the MPI does not matter for the MPI independent targets (the shims take mpi.h only): any one will do
any_mpi=mpich:5.0.2$L-mt-ofi-eng

for diag in off on ; do
    build $any_mpi core $diag
    build $any_mpi transport_layers $diag
    # the event-class shims fit every core variant (DIAG included: they reach the core through t_shared only): one build
    [ $diag = off ] && build $any_mpi class_layers off

    # target = the MPI layer; transport layer and collective transfers are explicitly turned off for Intel MPI (no sources)
    build impi                         impi      $diag
    build openmpi:5.0.11$L             openmpi5  $diag
    build openmpi:5.0.11$L-eng         openmpi5  $diag
    build mpich:4.3.2$L-mt-ofi         mpich4    $diag
    build mpich:4.3.2$L-mt-ofi-eng     mpich4    $diag
    build mpich:5.0.2$L-mt-ofi         mpich5    $diag
    build mpich:5.0.2$L-mt-ofi-eng     mpich5    $diag
    build mvapich:4.1$L-mt-ofi         mvapich4  $diag
    build mvapich:4.1$L-mt-ofi-eng     mvapich4  $diag
    # MVAPICH2 (verbs/smpi/psm2 layers; its engineering build needs verbs headers, and psm2 for the psm2 device):
    #build mvapich:2.3.7$L              mvapich2  $diag
    #build mvapich:2.3.7$L-eng          mvapich2  $diag
done
wait

if [ -s $failed ] ; then
    echo "BuildAll.sh $COMPILER: FAILED (see Build.${COMPILER}.log):"
    sed 's/^/  Build.sh /' $failed
    rm -f $failed
    exit 1
fi
rm -f $failed
echo "BuildAll.sh $COMPILER: all built"
