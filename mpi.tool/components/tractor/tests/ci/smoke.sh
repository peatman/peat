#!/bin/bash
# End-to-end smoke test for CI (or any box with a distro MPI in PATH):
#   build func-classes tests -> run each under tractor (LD_PRELOAD) -> run mat and ctt on the trace.
#   Every test is traced twice: with function names (PEAT_TRACTOR_PRINT_FUNC_NAMES=1) and with the collector's
#   numeric ids (=0), which mat/ctt must translate through the .app name/ID table; both must give the same functions.
#
#   ./smoke.sh <libtractor.so> [ranks] [iterations] [tests]
#   e.g. ./smoke.sh $PEAT/mpi.tool/lib/gnu/libtractor.mpich4.so 4 3 "p2p coll comm rma"
#
# Expects mat and ctt in <repo>/mpi.tool/bin (as installed by their Makefiles).
# Works with Open MPI (mpirun -x) and MPICH/MVAPICH hydra (mpiexec -genv).
# Exits non-zero if any test, trace or tool step fails.

set -u

lib=${1:?usage: smoke.sh <libtractor.so> [ranks] [iterations] [tests]}
ranks=${2:-4}   # coll needs >2 ranks (neighbour collectives on a 2-rank ring give wrong values)
iterations=${3:-3}
# fio is left out by default: its split-collective/nonblocking collective I/O checks fail intermittently
#   under Open MPI 5 even without tractor (about half of the runs).
# rma hangs under Open MPI 5 (even without tractor); pass "p2p coll comm" there.
tests=${4:-p2p coll comm rma}

here=$(cd "$(dirname "$0")" && pwd)
src=$here/../func-classes
bin=$(cd "$here/../../../../bin" && pwd)
lib=$(realpath "$lib")
work=${SMOKE_WORKDIR:-$(mktemp -d)}

[ -f "$lib" ]     || { echo "no tractor library: $lib"; exit 2; }
[ -x "$bin/mat" ] || { echo "no mat in $bin"; exit 2; }
[ -x "$bin/ctt" ] || { echo "no ctt in $bin"; exit 2; }

# Same tractor settings as func-classes/run.sh; PEAT_TRACTOR_PRINT_FUNC_NAMES is set per run below.
export PEAT_TRACTOR_STORE_MAP=1
export PEAT_TRACTOR_WRITE_TRACE=1
export PEAT_TRACTOR_RETURN_LEVELS=0
export PEAT_TRACTOR_TMP_DIR=$work/tmp
mkdir -p "$PEAT_TRACTOR_TMP_DIR"

tractor_vars="PEAT_TRACTOR_PRINT_FUNC_NAMES PEAT_TRACTOR_STORE_MAP PEAT_TRACTOR_WRITE_TRACE PEAT_TRACTOR_RETURN_LEVELS PEAT_TRACTOR_TMP_DIR"

mpirun --version 2>&1 | grep -qi "open mpi" && openmpi=1 || openmpi=0

# LD_PRELOAD is passed to the ranks only, not to the launcher itself.
set_launch() {
    if [ $openmpi -eq 1 ] ; then
        launch=(mpirun --oversubscribe -n "$ranks" -x "LD_PRELOAD=$lib")
        for v in $tractor_vars ; do launch+=(-x "$v") ; done
    else
        launch=(mpiexec -n "$ranks" -genv LD_PRELOAD "$lib")
        for v in $tractor_vars ; do launch+=(-genv "$v" "${!v}") ; done
    fi
}

echo "== tractor: $lib"
ldd "$lib" | grep -E "not found" && { echo "unresolved dependencies"; exit 1; }

status=0
fail() { echo "FAIL: $*"; status=1; }

# trace_and_analyse <test> <names:1|0> -> traces, mat and ctt output in $work/<test>.<names|ids>
trace_and_analyse() {
    local t=$1 names=$2 mode d r
    [ "$names" = 1 ] && mode=names || mode=ids
    export PEAT_TRACTOR_PRINT_FUNC_NAMES=$names
    set_launch
    d=$work/$t.$mode
    rm -rf "$d" && mkdir -p "$d" && cd "$d" || exit 1

    if ! timeout 300 "${launch[@]}" "$src/${t}_test" "$iterations" > run.log 2>&1 ; then
        cat run.log ; fail "$t/$mode: run" ; return
    fi
    grep -q PASSED run.log || { cat run.log ; fail "$t/$mode: test did not report PASSED" ; return ; }

    for r in $(seq 0 $((ranks - 1))) ; do
        [ -s "tractor_t.mpi.$r.gz" ] || fail "$t/$mode: missing tractor_t.mpi.$r.gz"
    done

    timeout 300 "$bin/mat" -o out > mat.log 2>&1 || { tail -20 mat.log ; fail "$t/$mode: mat" ; }
    # ctt: report on stdout, progress on stderr - keep them apart so progress can't split report lines
    timeout 300 "$bin/ctt" > ctt.log 2> ctt.err || { tail -20 ctt.log ctt.err ; fail "$t/$mode: ctt" ; }
    grep -q "End processing" ctt.log ctt.err    || fail "$t/$mode: ctt did not finish"
    if grep -h "not in the trace's function name/ID table" mat.log ctt.log ctt.err ; then
        fail "$t/$mode: function ids missing from the .app name/ID table"
    fi
    grep "^Function " ctt.log | sort -u > functions.txt
    echo "    $mode: $(wc -l < functions.txt) functions in ctt report"
}

for t in $tests ; do
    echo "== $t"
    # -B: always rebuild, a binary left over from another MPI would otherwise be reused
    make -s -B -C "$src" "${t}_test" MPICC="${MPICC:-mpicc}" || { fail "$t: build"; continue; }
    trace_and_analyse "$t" 1
    trace_and_analyse "$t" 0
    # numeric ids must translate to the same functions as names
    if [ -f "$work/$t.names/functions.txt" ] && [ -f "$work/$t.ids/functions.txt" ] ; then
        diff "$work/$t.names/functions.txt" "$work/$t.ids/functions.txt" > /dev/null \
            || { diff "$work/$t.names/functions.txt" "$work/$t.ids/functions.txt" | head -20 ; fail "$t: names and ids traces give different functions" ; }
    fi
done

echo "== work dir: $work"
[ $status -eq 0 ] && echo "SMOKE PASSED" || echo "SMOKE FAILED"
exit $status
