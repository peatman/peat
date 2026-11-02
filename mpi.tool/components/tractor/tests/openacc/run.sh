#!/bin/bash
# Build and run the MPI+OpenACC tests under tractor (library built with PARSE_GPU=on and COMPILER=gnu).
#   ./run.sh [iterations] [ranks]
# acc_test     - real OpenACC program (GCC -fopenacc)
# acc_sim_test - raises the device events itself (uploads, launches, downloads, waits), so the GPU classification
#                (transfers / executions / syncs in the tractor GPU statistics) is checked without an accelerator
# The test is compiled with GCC -fopenacc (acc_prof.h comes with GCC >= 9; libgomp runs it on the host
# when no offload device is installed, which is enough to exercise the profiling interface).
# OpenACC follows PARSE_GPU=on and is not built with COMPILER=intel, so the default library is lib/gnu/libtractor.mpich5.gpu.so.
# Set TRACTOR=0 to run it plain, TRACTOR_LIB=<path> to preload a specific library.

iterations=${1:-5}
ranks=${2:-2}

export compiler=gcc #oneAPI:2026.1.0
export mpi_type=${mpi_type:-mpich:5.0.1g-mt-ofi-eng}
export libfabric_eng='-eng'
. env.sh
set +x

mpi_root=$(dirname $(dirname $(which mpiexec)))
mpi_link="-L$mpi_root/lib -Wl,-rpath,$mpi_root/lib -Wl,--allow-shlib-undefined -lmpi"
gcc -fopenacc -O1 -g -I$mpi_root/include acc_test.c -o acc_test $mpi_link || exit 1
gcc -O1 -g -rdynamic -I$mpi_root/include acc_sim_test.c -o acc_sim_test $mpi_link || exit 1

preload=
if [ "${TRACTOR:-1}" != "0" ] ; then
    lib=${TRACTOR_LIB:-$(realpath ../../../../lib/gnu)/libtractor.mpich5.gpu.so}
    preload="-genv LD_PRELOAD $lib"
fi

export FI_PROVIDER=shm
export PEAT_TRACTOR_PRINT_FUNC_NAMES=1
export PEAT_TRACTOR_WRITE_TRACE=1
export PEAT_TRACTOR_RETURN_LEVELS=0
export PEAT_TRACTOR_TMP_DIR=/tmp
# export PEAT_TRACTOR_PARSE_OPENACC=0   # switch the OpenACC events off

status=0
for t in acc_test acc_sim_test ; do
    trace=trace.$t.$$
    mkdir -p $trace
    ( cd $trace && mpiexec -np $ranks $preload ../$t $iterations ) || { status=1; continue; }
    [ -n "$preload" ] || continue
    n=$(zcat $trace/tractor_t.mpi.0.gz | grep -c -E '^[-0-9:]+:acc_')
    echo "$t: rank 0 recorded $n OpenACC events (trace dir: $trace)"
    [ "$n" -gt 0 ] || { echo "no OpenACC events in the trace (library built without PARSE_GPU=on ?)"; status=77; continue; }
    if [ $t == acc_sim_test ] ; then
        zcat $trace/tractor_t.com.0.gz | grep -A8 "GPU Statistics"
        for what in "acc_enqueue_upload.*Host->Device" "acc_enqueue_download.*Device->Host" "acc_enqueue_launch" "acc_wait" ; do
            zcat $trace/tractor_t.com.0.gz | grep -q "$what" || { echo "missing in GPU statistics: $what"; status=1; }
        done
    fi
done
rm -f acc_test acc_sim_test

exit $status
