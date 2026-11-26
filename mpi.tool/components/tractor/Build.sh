#!/bin/bash
#
# Build and install one tractor library variant (src/Makefile) in a private copy of src/:
#
#   [COMPILER=gnu|intel|nvidia|clang|amd compiler=<env.sh compiler spec>] ./Build.sh <mpi_type[:version]> <target> [diag]
#
#   mpi_type[:version]  the MPI that sets up the environment (env.sh): impi, mpich:<version>, mvapich:<version>,
#                       openmpi:<version>; the 'eng' in the version selects the engineering (patched) MPI and libfabric.
#                       For the MPI independent targets (core, transport_layers, class_layers) it does not matter which MPI
#                       it is (class_layers only takes mpi.h from it).
#   target              core              the core library (no MPI)
#                       transport_layers  the transport shims (psm2, psm3, ucx, ofi), MPI independent
#                       class_layers      the event-class shims (ompt, acc, opencl, levelzero, cuda, llama), MPI independent;
#                                         each is built when its compiler/headers support is found
#                       <mpi>             the MPI layer of an MPI: impi mpich3 mpich4 mpich5 mvapich2 mvapich4 openmpi5
#   diag                off (default) or on: the _dbg variant (not for class_layers: its shims fit the core of either variant)
#
# The core switches MEMPROFILE, HWCOUNTERS and the MPI layer ones MPIBUFFERS, VIRT2PHYS are off unless set in the
# environment (they change the library names: .mem<x>, .<hwc>, .mbs, .v2p). OMPT is always a shim (class_layers), never
# built into the core.
# See BuildAll.sh, which builds all of them for a compiler.

mpi_type=$1
TARGET=$2
DIAG=${3:-off}

if [ -z "$mpi_type" -o -z "$TARGET" ] ; then
    sed -n '3,22p' "$0" | sed 's/^# \{0,1\}//'
    exit 2
fi

: ${COMPILER:=gnu}
: ${compiler:=gcc} # for the environment script
export COMPILER compiler
MEMPROFILE=${MEMPROFILE:-off}
HWCOUNTERS=${HWCOUNTERS:-off}
MPIBUFFERS=${MPIBUFFERS:-off}
VIRT2PHYS=${VIRT2PHYS:-off}

. env.sh

if [ -z "$ENG_MPI_BUILD" ] ; then
    ENG_MPI_BUILD=`expr "$mpi_version" : '.*\(eng\)'`
fi
if [ -n "$ENG_MPI_BUILD" ] ; then
    mpi_internal="PARSE_MPI_INTERNAL=on"
    libfabric_eng=-eng
fi
export LD_LIBRARY_PATH=$HOME/libfabric${libfabric_eng}/lib:$LD_LIBRARY_PATH # need path pointed to correct libfabric

export PSM2PATH=$HOME/psm2
export LD_LIBRARY_PATH=$PSM2PATH/lib64:$LD_LIBRARY_PATH
export VERBSPATH=/usr
export OFI_PATH=$HOME/libfabric${libfabric_eng}
export OFIPATH=$OFI_PATH
export PSM3PATH=$HOME/psm3
export UCXPATH=$HOME/ucx
export LD_LIBRARY_PATH=$OFI_PATH/lib64:$PSM3PATH/lib64:$UCXPATH/lib64:$LD_LIBRARY_PATH

# the name of the private source copy: the compiler, the target, the engineering MPI and what is not default
src_tmp="src-${COMPILER}-${TARGET}${ENG_MPI_BUILD:+-eng}-diag${DIAG}"
for v in MEMPROFILE HWCOUNTERS MPIBUFFERS VIRT2PHYS ; do
    [ "${!v}" = off ] || src_tmp="$src_tmp-$v=${!v}"
done

make_args="COMPILER=$COMPILER TARGET=$TARGET OMPT=off DIAG=$DIAG MEMPROFILE=$MEMPROFILE HWCOUNTERS=$HWCOUNTERS MPIBUFFERS=$MPIBUFFERS VIRT2PHYS=$VIRT2PHYS $mpi_internal"

echo $src_tmp >>Build.${COMPILER}.dir

if [ -n "$do_not_remove" ] ; then
    rm -rf $src_tmp
fi
if [ -d $src_tmp ] ; then
    echo "Directory $src_tmp exists...exiting"
    exit 1
fi
mkdir -p $src_tmp
cp -r src/* $src_tmp
cd $src_tmp || exit 1

# a failed make does not stop the others of BuildAll.sh: the exit status says it, the log has the details
set -o pipefail
rc=0
set -x
make clean $make_args 2>&1
make -j 4 $make_args 2>&1 | tee ./Build.log || rc=1
[ $rc = 0 ] && { make install $make_args 2>&1 | tee -a ./Build.log || rc=1 ; }
set +x
echo "Log From $src_tmp" >>../Build.${COMPILER}.log
cat Build.log >>../Build.${COMPILER}.log
cd - >/dev/null

if [ -z "$do_not_remove" ] ; then
    rm -rf $src_tmp
fi
exit $rc
