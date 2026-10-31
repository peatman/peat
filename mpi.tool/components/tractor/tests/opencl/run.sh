#!/bin/bash
# Build and run the OpenCL classification test under tractor (library built with PARSE_GPU=on).
#   ./run.sh [iterations] [ranks]
# cl_sim_test calls OpenCL enqueue functions that are implemented by cl_fake.c (they only sleep), so the
# transfer / fill / execution / sync classification in the tractor GPU statistics is checked without a device.
# Set TRACTOR_LIB=<path> to preload a specific library, default is lib/intel/libtractor.mpich5.gpu.so.

iterations=${1:-5}
ranks=${2:-2}

export compiler=oneAPI:2026.1.0
export mpi_type=${mpi_type:-mpich:5.0.1x-mt-ofi-eng}
export libfabric_eng='-eng'
. env.sh
set +x

mpi_root=$(dirname $(dirname $(which mpiexec)))
cl_flags="-DCL_TARGET_OPENCL_VERSION=300 -I$(dirname $(dirname $(which icx)))/include"
gcc -shared -fPIC $cl_flags cl_fake.c -o libclfake.so || exit 1
gcc -O1 -g $cl_flags -I$mpi_root/include cl_sim_test.c -o cl_sim_test -L. -Wl,-rpath,$PWD -lclfake \
    -L$mpi_root/lib -Wl,-rpath,$mpi_root/lib -Wl,--allow-shlib-undefined -lmpi || exit 1

lib=${TRACTOR_LIB:-$(realpath ../../../../lib/intel)/libtractor.mpich5.gpu.so}
export FI_PROVIDER=shm
export PEAT_TRACTOR_PRINT_FUNC_NAMES=1
export PEAT_TRACTOR_WRITE_TRACE=1
export PEAT_TRACTOR_RETURN_LEVELS=0
export PEAT_TRACTOR_TMP_DIR=/tmp

trace=trace.$$
mkdir -p $trace
( cd $trace && mpiexec -np $ranks -genv LD_PRELOAD $lib ../cl_sim_test $iterations ) || exit 1

status=0
zcat $trace/tractor_t.com.0.gz | grep -A14 "GPU Statistics"
for what in "clEnqueueWriteBuffer.*Host->Device" "clEnqueueReadBuffer.*Device->Host" "clEnqueueCopyBuffer.*Device->Device" \
            "clEnqueueSVMMemcpy.*Shared->Shared" "clEnqueueFillBuffer.*Fill Device" "clEnqueueNDRangeKernel" "clFinish" ; do
    zcat $trace/tractor_t.com.0.gz | grep -q "$what" || { echo "missing in GPU statistics: $what"; status=1; }
done
# event handles: the enqueue calls record the event they return (output parameter) and the wait list (extra parameters)
mpi=$trace/tractor_t.mpi.0.gz
zcat $mpi | grep -q "clEnqueueWriteBuffer:9:1:0:.*:[0-9a-f]*[1-9a-f][0-9a-f]*$" || { echo "no event handle recorded for clEnqueueWriteBuffer"; status=1; }
zcat $mpi | grep -q "clEnqueueNDRangeKernel:9:1:1:" || { echo "wait list of clEnqueueNDRangeKernel not recorded"; status=1; }
zcat $mpi | grep -q "clWaitForEvents:2:0:1:" || { echo "event list of clWaitForEvents not recorded"; status=1; }
# mat pairs the commands with what completes them (needs the mat binary: MAT=<path>)
if [ -n "$MAT" ] ; then
    mkdir -p $trace/mat
    $MAT -w $trace -i tractor_t -o $trace/mat > $trace/mat.log 2>&1
    # in-order queue: the blocking SVM copy finishes everything enqueued before it; a wait finishes the commands before its event
    grep -q "clEnqueueCopyBuffer *clEnqueueSVMMemcpy *65536" $trace/mat.log || { echo "mat: in-order copy not finished by the blocking call after it"; status=1; }
    grep -q "clEnqueueCopyBuffer *clWaitForEvents *16384" $trace/mat.log || { echo "mat: in-order copy not completed by the later wait"; status=1; }
    # out-of-order queue: a wait finishes only the event and what it waits for
    grep -q "clEnqueueNDRangeKernel *clWaitForEvents" $trace/mat.log || { echo "mat: wait list dependency not completed"; status=1; }
    # out-of-order queue: the wait for the second command leaves the first one for clFinish
    grep -q "clEnqueueCopyBuffer *clFinish *8192" $trace/mat.log || { echo "mat: out-of-order copy not left for clFinish"; status=1; }
    grep -q "clEnqueueWriteBuffer *clWaitForEvents *512" $trace/mat.log || { echo "mat: out-of-order write not completed by its wait"; status=1; }
    grep -q "clEnqueueNDRangeKernel *clEnqueueNDRangeKernel" $trace/mat.log || { echo "mat: unwaited kernel not counted by its enqueue call"; status=1; }
    zcat $trace/mat/tractor_t.bind.0.gz | grep -q "clEnqueueTask:.*:clWaitForEvents:" || { echo "mat: no bind line for the task"; status=1; }
fi
exit $status
