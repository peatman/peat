#!/bin/bash
# Build and run the MPI+OpenACC tests under tractor with the real OpenACC program offloaded to an NVIDIA GPU.
# Same as run.sh, except that acc_test is compiled with nvc (NVHPC) instead of GCC -fopenacc, which only runs on the host.
#   ./run_nvc.sh [iterations] [ranks]
# acc_test     - real OpenACC program (nvc -acc), kernels and data transfers run on the GPU
# acc_sim_test - raises the device events itself (GCC, needs no accelerator), as in run.sh
# The toolchain comes from env.sh: compiler=nvc:<version>[:<version>] puts nvc in PATH and selects the NVHPC release and/or
# the CUDA toolkit it compiles with (and adds the WSL CUDA driver library). The default nvc:12.9 is CUDA 12.9 because CUDA 13
# dropped Pascal (sm_61), so a GTX 10xx needs CUDA <= 12.9. Set NVC_COMPILER (e.g. nvc:26.9:12.9) to use another one.
# Set ACC_GPU_FLAGS to change the GPU target (default -gpu=cc61), TRACTOR=0 to run it plain,
# TRACTOR_LIB=<path> to preload a specific library (default lib/gnu/libtractor.mpich5.gpu.so, the OpenACC support
# follows PARSE_GPU=on and is not built with COMPILER=intel).

iterations=${1:-5}
ranks=${2:-2}

export compiler=${NVC_COMPILER:-nvc:12.9}
export mpi_type=${mpi_type:-mpich:5.0.1g-mt-ofi-eng}
export libfabric_eng='-eng'
. env.sh
set +x

command -v nvc > /dev/null || { echo "nvc not found (compiler=$compiler; NVHPC compilers are needed for the GPU run, use run.sh for the host run)"; exit 77; }
acc_gpu_flags=${ACC_GPU_FLAGS:--gpu=cc61}

mpi_root=$(dirname $(dirname $(which mpiexec)))
mpi_link="-L$mpi_root/lib -Wl,-rpath,$mpi_root/lib -Wl,--allow-shlib-undefined -lmpi"
nvc -acc $acc_gpu_flags -O1 -g -I$mpi_root/include acc_test.c -o acc_test $mpi_link || exit 1
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
    # the function name is preceded by one id column per nesting level of the recorded calls: zeInit, called by libfabric inside
    # MPI_Init and recorded by the Level Zero wrapper, makes it two (4:-1:0:0:acc_...), without it there is one (4:0:0:acc_...)
    n=$(zcat $trace/tractor_t.mpi.0.gz | grep -c -E '^[-0-9:]+:acc_')
    echo "$t: rank 0 recorded $n OpenACC events (trace dir: $trace)"
    [ "$n" -gt 0 ] || { echo "no OpenACC events in the trace (library built without PARSE_GPU=on ?)"; status=77; continue; }
    zcat $trace/tractor_t.com.0.gz | grep -A8 "GPU Statistics"
    if [ $t == acc_test ] ; then
        # the host runtime raises no launch or transfer events, so these show that the kernels really ran on the GPU
        launches=$(zcat $trace/tractor_t.mpi.0.gz | grep -c -E '^[-0-9:]+:acc_enqueue_launch')
        echo "$t: rank 0 recorded $launches kernel launches"
        [ "$launches" -gt 0 ] || { echo "no kernel launches recorded: acc_test did not run on the GPU"; status=1; }
    fi
    for what in "acc_enqueue_upload.*Host->Device" "acc_enqueue_download.*Device->Host" "acc_enqueue_launch" "acc_wait" ; do
        zcat $trace/tractor_t.com.0.gz | grep -q "$what" || { echo "missing in GPU statistics ($t): $what"; status=1; }
    done
done
rm -f acc_test acc_sim_test

exit $status
