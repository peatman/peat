#!/bin/bash
# llama.cpp / CUDA smoke test for CI (or any box with a llama.cpp build):
#   run llama-simple under tractor (LD_PRELOAD, no MPI: a one-process trace tractor_t_<pid>.*) -> mat and ctt on the trace.
#
#   ./llama.sh <libtractor.so> <llama.cpp bin dir (llama-simple)> <model.gguf> [tokens]
#   e.g. ./llama.sh $PEAT/mpi.tool/lib/gnu/libtractor.openmpi.so ~/llama.cpp/build/bin stories15M-q4_0.gguf 16
#
# Expects mat and ctt in <repo>/mpi.tool/bin (as installed by their Makefiles; SMOKE_BIN=<dir> to take them from
# elsewhere), a llama.cpp built with shared libraries
# (the wrappers interpose the calls between libllama/libggml*, so the calls must go through their PLT: GCC builds).
# llama-simple samples greedily, so every run must print exactly what the reference run without tractor printed.
#
# Runs (each in <work>/<run>):
#   ref      llama-simple without tractor (the reference output)
#   stages   llama.cpp stages (default): trace written, "Modules:" remark in the .com file says llama.cpp stages on and
#            CPU kernels off, mat and ctt process it, ctt reports the stages llama-simple goes through and no CPU kernel
#   kernels  PEAT_TRACTOR_PARSE_GGML_CPU=1: the same plus ggml CPU kernels (ggml_compute_forward_* or the repacked
#            matmuls' ggml_cpu_extra_compute_forward; they run on all the CPU backend's threads)
#   off      PEAT_TRACTOR_PARSE_LLAMA=0: no llama.cpp function in the trace (if one is written at all)
#   cuda     only for a library built with CUDA (CUPTI + cuBLAS wrappers): PEAT_TRACTOR_PARSE_CUDA=1 on a CPU-only
#            llama.cpp (CI has no GPU: CUPTI may fail to start, which must only be reported, not break the run): the
#            run behaves as "stages", the remark names CUDA/CUPTI (recording "failed" without a driver, "on" with one),
#            and the library exports the cuBLAS wrappers.
# LLAMA_SMOKE_CUDA=1 requires the library to be a CUDA one (=0: skip the cuda run; unset: run it when it is one).
# Exits non-zero if any run, trace or tool step fails.

set -u

lib=${1:?usage: llama.sh <libtractor.so> <llama.cpp bin dir> <model.gguf> [tokens]}
llama_bin=${2:?usage: llama.sh <libtractor.so> <llama.cpp bin dir> <model.gguf> [tokens]}
model=${3:?usage: llama.sh <libtractor.so> <llama.cpp bin dir> <model.gguf> [tokens]}
tokens=${4:-16}
prompt="Once upon a time"

here=$(cd "$(dirname "$0")" && pwd)
bin=$(cd "${SMOKE_BIN:-$here/../../../../bin}" && pwd)
lib=$(realpath "$lib")
simple=$(realpath "$llama_bin/llama-simple")
model=$(realpath "$model")
work=${SMOKE_WORKDIR:-$(mktemp -d)}

[ -f "$lib" ]     || { echo "no tractor library: $lib"; exit 2; }
[ -x "$simple" ]  || { echo "no llama-simple in $llama_bin"; exit 2; }
[ -f "$model" ]   || { echo "no model: $model"; exit 2; }
[ -x "$bin/mat" ] || { echo "no mat in $bin"; exit 2; }
[ -x "$bin/ctt" ] || { echo "no ctt in $bin"; exit 2; }
mkdir -p "$work"

status=0
fail() { echo "FAIL: $*"; [ -z "${GITHUB_ACTIONS:-}" ] || echo "::error title=llama smoke::$*"; status=1; }

# the library must have the llama.cpp wrappers at all
nm -D --defined-only "$lib" | grep -qw llama_process || { echo "$lib has no llama.cpp wrappers (built without llama.cpp headers?)"; exit 2; }
cublas_wrappers="cublasSgemm_v2 cublasGemmEx cublasGemmBatchedEx cublasGemmStridedBatchedEx cublasSgemmBatched cublasSgemmStridedBatched cublasStrsmBatched cublasLtMatmul"
if nm -D --defined-only "$lib" | grep -qw cublasGemmEx ; then cuda_lib=1 ; else cuda_lib=0 ; fi
case "${LLAMA_SMOKE_CUDA:-}" in
    1) [ $cuda_lib = 1 ] || { echo "LLAMA_SMOKE_CUDA=1 but $lib is not a CUDA build (no cuBLAS wrappers)"; exit 2; } ;;
    0) cuda_lib=0 ;;
esac

echo "== tractor: $lib (CUDA build: $cuda_lib)"
echo "== llama-simple: $simple, model: $model, $tokens tokens"

# tractor settings shared by the runs; the per-run ones are passed to run_traced
export PEAT_TRACTOR_STORE_MAP=1
export PEAT_TRACTOR_WRITE_TRACE=1
export PEAT_TRACTOR_PRINT_FUNC_NAMES=1
export PEAT_TRACTOR_TMP_DIR=$work/tmp
mkdir -p "$PEAT_TRACTOR_TMP_DIR"

# reference: no tractor
d=$work/ref
rm -rf "$d" && mkdir -p "$d"
if ! (cd "$d" && timeout 300 "$simple" -m "$model" -n "$tokens" "$prompt" > out.txt 2> err.txt) ; then
    tail -n 20 "$d/err.txt" ; echo "llama-simple fails without tractor"; exit 2
fi
[ -s "$d/out.txt" ] || { echo "llama-simple printed nothing"; exit 2; }
echo "    reference: $(head -c 120 "$d/out.txt" | tr '\n' ' ')"

# run_traced <run> [VAR=value...]: llama-simple under tractor in <work>/<run>; sets trace=<template> (empty if none)
run_traced() {
    local run=$1 ; shift
    d=$work/$run
    trace=
    rm -rf "$d" && mkdir -p "$d"
    echo "== $run ${*:-(defaults)}"
    # LD_PRELOAD for llama-simple only (not timeout: it would get a tractor too)
    if ! (cd "$d" && timeout 300 env "$@" LD_PRELOAD="$lib" "$simple" -m "$model" -n "$tokens" "$prompt" > out.txt 2> err.txt) ; then
        tail -n 20 "$d/err.txt" ; fail "$run: llama-simple under tractor" ; return 1
    fi
    grep -h "tractor" "$d/err.txt" | grep -iE "fail|error|abort" | head -5 | sed 's/^/    stderr: /'
    cmp -s "$work/ref/out.txt" "$d/out.txt" || { diff "$work/ref/out.txt" "$d/out.txt" | head ; fail "$run: output differs from the run without tractor" ; }
    local app
    app=$(cd "$d" && ls tractor_t_*.app.gz 2>/dev/null)
    [ $(echo -n "$app" | grep -c .) -le 1 ] || { echo "$app" ; fail "$run: more than one trace" ; return 1 ; }
    trace=${app%.app.gz}
    return 0
}

# check_trace <run> <modules regex>: one-process trace complete, Modules remark matches, mat + ctt; functions.txt
check_trace() {
    local run=$1 modules_re=$2 f
    [ -n "$trace" ] || { ls "$d" ; fail "$run: no trace (tractor_t_<pid>.app.gz) written" ; return 1 ; }
    [[ $trace =~ ^tractor_t_[0-9]+$ ]] || fail "$run: trace $trace is not named tractor_t_<pid>"
    for f in app.gz com.0.gz mpi.0.gz ; do
        [ -s "$d/$trace.$f" ] || fail "$run: missing $trace.$f"
    done
    local m
    m=$(zcat "$d/$trace.com.0.gz" | grep -m1 ":Modules: ")
    echo "    ${m#*:}"
    [ -n "$m" ] || fail "$run: no Modules remark in $trace.com.0.gz"
    echo "$m" | grep -qE "$modules_re" || fail "$run: Modules remark does not match '$modules_re'"

    mkdir -p "$d/serial" && cp "$d/$trace".* "$d/serial/"
    (cd "$d/serial" && timeout 300 "$bin/mat" -i "$trace" > mat.log 2>&1) || { tail -n 20 "$d/serial/mat.log" ; fail "$run: mat" ; }
    [ -z "$(ls "$d/serial/out" 2>/dev/null)" ] && fail "$run: mat wrote nothing to out/"
    # ctt: report on stdout, progress on stderr
    (cd "$d" && timeout 300 "$bin/ctt" -i "$trace" > ctt.log 2> ctt.err) || { tail -n 20 "$d/ctt.log" "$d/ctt.err" ; fail "$run: ctt" ; }
    grep -q "End processing" "$d/ctt.log" "$d/ctt.err" || fail "$run: ctt did not finish"
    if grep -h "not in the trace's function name/ID table" "$d/serial/mat.log" "$d/ctt.log" "$d/ctt.err" ; then
        fail "$run: function ids missing from the .app name/ID table"
    fi
    grep "^Function " "$d/ctt.log" | sed 's/^Function //' | awk '{print $1}' | sort -u > "$d/functions.txt"
    echo "    $(wc -l < "$d/functions.txt") functions in ctt report"
}

# require <run> <function>...: every function in the run's ctt report
require() {
    local run=$1 fn ; shift
    for fn in "$@" ; do
        grep -qx "$fn" "$work/$run/functions.txt" || fail "$run: $fn not in the trace"
    done
}

# the stages llama-simple always goes through (llama_decode is a deprecated wrapper of llama_process)
stages="llama_model_load_from_file llama_init_from_model llama_tokenize llama_process llama_sampler_sample llama_token_to_piece
        ggml_backend_sched_graph_compute_async"
kernel_re='^(ggml_compute_forward_[a-z0-9_]+|ggml_cpu_extra_compute_forward)$'

if run_traced stages ; then
    if check_trace stages 'llama\.cpp\(stages on, ggml CPU kernels off\)' ; then
        require stages $stages
        ! grep -qE "$kernel_re" "$d/functions.txt" || fail "stages: CPU kernels traced although PEAT_TRACTOR_PARSE_GGML_CPU is off"
    fi
fi

if run_traced kernels PEAT_TRACTOR_PARSE_GGML_CPU=1 ; then
    if check_trace kernels 'llama\.cpp\(stages on, ggml CPU kernels on\)' ; then
        require kernels $stages
        n=$(grep -cE "$kernel_re" "$d/functions.txt")
        echo "    $n CPU kernels: $(grep -E "$kernel_re" "$d/functions.txt" | head -6 | tr '\n' ' ')..."
        [ "$n" -gt 0 ] || fail "kernels: no ggml CPU kernel (ggml_compute_forward_*, ggml_cpu_extra_compute_forward) in the trace"
    fi
fi

if run_traced off PEAT_TRACTOR_PARSE_LLAMA=0 ; then
    if [ -n "$trace" ] ; then
        check_trace off 'llama\.cpp\(stages off, ggml CPU kernels off\)' && \
            { ! grep -qE '^(llama_|ggml_)' "$d/functions.txt" || fail "off: llama.cpp functions traced with PEAT_TRACTOR_PARSE_LLAMA=0" ; }
    else
        echo "    no trace written (nothing recorded)"
    fi
fi

if [ $cuda_lib = 1 ] ; then
    for w in $cublas_wrappers ; do
        nm -D --defined-only "$lib" | grep -qw "$w" || fail "cuda: $lib does not export the $w wrapper"
    done
    ldd "$lib" | grep -q "libcupti.*=> /" || { ldd "$lib" | grep cupti ; fail "cuda: libcupti not resolved for $lib" ; }
    if run_traced cuda PEAT_TRACTOR_PARSE_CUDA=1 ; then
        # "failed": CUPTI did not start (no driver, as on CI); "on" with a GPU driver
        check_trace cuda 'CUDA [0-9]+\.[0-9]+/CUPTI [0-9]+\(recording (on|failed)\)' && require cuda $stages
        [ "$(grep -c 'CUDA is not traced' "$d/err.txt")" -le 1 ] || fail "cuda: CUPTI start failure reported more than once"
        # with a GPU (a CUDA llama.cpp on a box with a driver): host calls, device records and their binds (correlation)
        if zcat "$d/$trace.com.0.gz" | grep -q "(recording on)" ; then
            require cuda cudaLaunchKernel cuda_memcpy_htod
            n=$(grep -cvE '^(llama_|ggml_|cuda|cublas)' "$d/functions.txt")
            echo "    GPU: $n kernel names, $(zcat "$d"/serial/out/*.bind.* 2>/dev/null | wc -l) bind records"
            [ "$n" -gt 0 ] || fail "cuda: no GPU kernel records"
            [ "$(zcat "$d"/serial/out/*.bind.* 2>/dev/null | wc -l)" -gt 0 ] || fail "cuda: mat wrote no binds (launch -> kernel)"
        fi
    fi
else
    echo "== cuda: skipped (not a CUDA build)"
fi

echo "== work dir: $work"
[ $status -eq 0 ] && echo "LLAMA SMOKE PASSED" || echo "LLAMA SMOKE FAILED"
exit $status
