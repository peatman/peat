#!/bin/bash
# Build an MPI from its release tarball for CI, optionally with the mpi.tool/patches applied ("engineering" build).
#
#   ./build_mpi.sh <impl> <version> <prefix> [device] [patch-dir]
#
#   impl       mpich | mvapich | mvapich2 | openmpi
#   version    release, e.g. 5.0.2 (mvapich2: 2.3.7-1)
#   prefix     install prefix
#   device     MPICH-family --with-device (default ch4:ofi; mvapich2: ch3:mrail); ignored for Open MPI
#   patch-dir  apply every <patch-dir>/*.patch (patch -p1, sorted) before configuring; the MPICH family is then
#              re-bootstrapped with autogen.sh (the patches edit Makefile.mk / version.m4 / binding generators, and
#              make would otherwise demand the exact automake the tarball was made with)
#
# Work happens in $BUILD_MPI_WORKDIR (default: $RUNNER_TEMP or a mktemp dir). On failure the tail of the failing
# log is printed. BUILD_MPI_OPT (e.g. O0) overrides the optimization level for quick local test builds (MPICH family:
# --enable-fast=<opt>, default O2; Open MPI: CFLAGS/CXXFLAGS=-<opt>, default Open MPI's own); CI leaves it unset.

set -euo pipefail

impl=${1:?usage: build_mpi.sh <impl> <version> <prefix> [device] [patch-dir]}
ver=${2:?version}
prefix=${3:?prefix}
dev=${4:-}
patchdir=${5:-}
[ -n "$patchdir" ] && patchdir=$(realpath "$patchdir")

work=${BUILD_MPI_WORKDIR:-${RUNNER_TEMP:-$(mktemp -d)}}
jobs=$(nproc)
opt=${BUILD_MPI_OPT:-}

case $impl in
    mpich)    url=https://www.mpich.org/static/downloads/$ver/mpich-$ver.tar.gz;  top=mpich-$ver ;;
    # MVAPICH 3+/4+ tarballs live in the same mv2/ folder as MVAPICH2 (there is no mv4/; checked for 4.1)
    mvapich)  url=https://mvapich.cse.ohio-state.edu/download/mvapich/mv2/mvapich-$ver.tar.gz; top=mvapich-$ver ;;
    mvapich2) url=https://mvapich.cse.ohio-state.edu/download/mvapich/mv2/mvapich2-$ver.tar.gz; top=mvapich2-$ver ;;
    openmpi)  url=https://download.open-mpi.org/release/open-mpi/v${ver%.*}/openmpi-$ver.tar.bz2; top=openmpi-$ver ;;
    *) echo "unknown MPI implementation: $impl"; exit 2 ;;
esac

run() { # run <log> <cmd...>: quiet unless it fails
    local log=$1; shift
    "$@" > "$log" 2>&1 || { echo "FAILED: $* (last lines of $log)"; tail -n 60 "$log"; exit 1; }
}

mkdir -p "$work" && cd "$work"
rm -rf "$top"
curl -sSfL --retry 3 -o "${url##*/}" "$url"
tar -xf "${url##*/}"
cd "$top"

if [ -n "$patchdir" ]; then
    shopt -s nullglob
    patches=("$patchdir"/*.patch)
    [ ${#patches[@]} -gt 0 ] || { echo "no *.patch in $patchdir"; exit 1; }
    for p in "${patches[@]}"; do
        echo "applying ${p##*/}"
        patch -p1 --forward --batch -F0 < "$p" > "patch-${p##*/}.log" 2>&1 \
            || { cat "patch-${p##*/}.log"; exit 1; }
    done
    case $impl in
        openmpi)
            # the hook patch edits both ompi/mca/coll/base/Makefile.am and the Makefile.in generated from it; make
            # the .in files newer again so make does not try to rerun automake
            for p in "${patches[@]}"; do
                grep -E '^\+\+\+ ' "$p" | awk '{print $2}' | sed 's|^b/||' | { grep 'Makefile\.in$' || true; } | xargs -r touch
            done ;;
        *)
            echo "running autogen.sh"
            run autogen.log ./autogen.sh ;;
    esac
fi

case $impl in
    mpich|mvapich)
        dev=${dev:-ch4:ofi}
        extra=(); [ "$dev" = ch4:ofi ] && extra=(--with-libfabric=embedded)
        conf=(--with-device="$dev" "${extra[@]}" --with-pm=hydra --enable-romio --enable-threads=multiple
              --enable-shared --disable-fortran --with-hwloc=embedded --without-ze --without-cuda
              --enable-fast=${opt:-O2}) ;;
    mvapich2)
        # ch3:mrail (verbs): builds without an HCA, but cannot run on a CI VM. --disable-ibv-dlopen: libmpi links
        # libibverbs (the transport patch's verbs-proxy.c calls ibv_* directly); --disable-opencl: hwloc's OpenCL
        # backend does not build against current OpenCL headers
        # ch3:psm (PSM2): runs on one node without an Omni-Path HFI when PSM2_DEVICES=self,shm; libpsm2 from $PSM2PATH
        # or the system (libpsm2-dev)
        dev=${dev:-ch3:mrail}
        conf=(--with-device="$dev" --with-pm=hydra --enable-romio --enable-threads=multiple --enable-shared
              --disable-ibv-dlopen --disable-opencl --disable-fortran --enable-fast=${opt:-O2})
        case $dev in ch3:psm*)
            conf+=(--with-psm2${PSM2PATH:+=$PSM2PATH})
            # configure runs test programs linked against it
            [ -n "${PSM2PATH:-}" ] && export LD_LIBRARY_PATH=$PSM2PATH/lib64:$PSM2PATH/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH} ;;
        esac ;;
    openmpi)
        # bundled hwloc/libevent/PMIx/PRRTE so nothing depends on the distro's Open MPI 4.1 packages; no Fortran
        conf=(--disable-mpi-fortran --with-hwloc=internal --with-libevent=internal --with-pmix=internal
              --with-prrte=internal)
        # patched: no libfabric at all - tractor treats a patched libmpi that links a stock libfabric as a version
        # mismatch (the engineering libfabric is not built here), and the smoke test runs over TCP/shared memory anyway
        [ -n "$patchdir" ] && conf+=(--without-ofi)
        [ -n "$opt" ] && conf+=(CFLAGS=-$opt CXXFLAGS=-$opt) ;;
esac

echo "configure ${conf[*]}"
run configure.log ./configure --prefix="$prefix" --enable-silent-rules "${conf[@]}" CC=gcc CXX=g++
echo "make -j$jobs"
run make.log make -j"$jobs"
run install.log make install
echo "installed $impl $ver${patchdir:+ (patched)} in $prefix"
