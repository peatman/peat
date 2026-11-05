#!/bin/bash
# End-to-end smoke test for CI (or any box with a distro MPI in PATH):
#   build func-classes tests -> run each under tractor (LD_PRELOAD) -> run mat and ctt on the trace.
#   Every test is traced twice: with function names (PEAT_TRACTOR_PRINT_FUNC_NAMES=1) and with the collector's
#   numeric ids (=0), which mat/ctt must translate through the .app name/ID table; both must give the same functions.
#   Every MPI function a test calls (taken from its source preprocessed with this MPI's mpicc, limited to what tractor
#   can record, see ../../../common/mpi_func.inc) must show up in that test's trace. An engineering library (built for a
#   patched MPI) must record MPI-internal functions in coll, a stock one none at all.
#
#   ./smoke.sh <libtractor.so> [ranks] [iterations] [tests]
#   e.g. ./smoke.sh $PEAT/mpi.tool/lib/gnu/libtractor.mpich4.so 4 3 "p2p coll comm rma"
#
# Expects mat and ctt in <repo>/mpi.tool/bin (as installed by their Makefiles).
# Works with Open MPI (mpirun -x) and MPICH/MVAPICH hydra (mpiexec -genv).
# Exits non-zero if any test, trace or tool step fails.
#
# SMOKE_ENGINEERING=1|0 says whether <libtractor.so> is an engineering build (CI sets it from the matrix); unset, it is
# guessed: ".eng" in the library name (GNU make) or an exported MPIC_Send wrapper (MPICH family).

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

traceable=$work/traceable.txt   # MPI functions tractor can record
mkdir -p "$work"
sed -nE 's/^\{ [A-Z0-9_]+_ID COMMA "[^"]*" COMMA "(MPI_[A-Za-z0-9_]+)".*/\1/p' "$here/../../../common/mpi_func.inc" | sort -u > "$traceable"
[ -s "$traceable" ] || { echo "no function names from mpi_func.inc"; exit 2; }

if [ -n "${SMOKE_ENGINEERING:-}" ]; then
    engineering=$SMOKE_ENGINEERING
elif [[ $(basename "$lib") == *.eng.so* ]] || nm -D --defined-only "$lib" 2>/dev/null | grep -qw MPIC_Send; then
    engineering=1
else
    engineering=0
fi
internal_re='^Function (MPIC_|MPID_|mpi_internal_|ompi_coll_p2p_)'

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

echo "== tractor: $lib (engineering=$engineering)"
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

    timeout 300 "$bin/mat" -o out > mat.log 2>&1 || { tail -n 20 mat.log ; fail "$t/$mode: mat" ; }
    # ctt: report on stdout, progress on stderr - keep them apart so progress can't split report lines
    timeout 300 "$bin/ctt" > ctt.log 2> ctt.err || { tail -n 20 ctt.log ctt.err ; fail "$t/$mode: ctt" ; }
    grep -q "End processing" ctt.log ctt.err    || fail "$t/$mode: ctt did not finish"
    if grep -h "not in the trace's function name/ID table" mat.log ctt.log ctt.err ; then
        fail "$t/$mode: function ids missing from the .app name/ID table"
    fi
    grep "^Function " ctt.log | sort -u > functions.txt
    echo "    $mode: $(wc -l < functions.txt) functions in ctt report"
}

# MPI functions <test> calls: its own code (and test_common.h) preprocessed with this MPI's mpicc - so #if MPI_VERSION
# and similar blocks resolve as they were compiled - without mpi.h, limited to the functions tractor can record
expected_calls() {
    "${MPICC:-mpicc}" -E "$src/$1_test.c" 2>/dev/null |
        awk -v t="$1_test.c" '/^# [0-9]+ "/ { f = $3; gsub(/"/, "", f); n = split(f, a, "/");
                                             keep = (a[n] == t || a[n] == "test_common.h"); next }
                              keep' |
        grep -oE '\bMPI_[A-Z][A-Za-z0-9_]*[a-z][A-Za-z0-9_]*[[:space:]]*\(' | sed -E 's/[[:space:]]*\($//' |
        sort -u | grep -Fx -f "$traceable"
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
    if [ -f "$work/$t.names/functions.txt" ] ; then
        # every MPI function the test calls must be in its trace
        expected_calls "$t" > "$work/$t.expected.txt"
        [ -s "$work/$t.expected.txt" ] || fail "$t: no MPI calls found in $t""_test.c (preprocessing with ${MPICC:-mpicc} failed?)"
        sed 's/^Function //' "$work/$t.names/functions.txt" | sort -u > "$work/$t.traced.txt"
        missing=$(comm -23 "$work/$t.expected.txt" "$work/$t.traced.txt")
        echo "    $(wc -l < "$work/$t.expected.txt") MPI functions called by $t""_test.c, $(echo -n "$missing" | grep -c .) missing in the trace"
        [ -z "$missing" ] || { echo "$missing" | sed 's/^/      missing: /' ; fail "$t: MPI functions called by the test are missing in the trace" ; }
        # MPI-internal functions: recorded by an engineering library (coll always uses them), never by a stock one
        if [ "$engineering" = 1 ] ; then
            [ "$t" != coll ] || grep -qE "$internal_re" "$work/$t.names/functions.txt" \
                || fail "$t: engineering library, but no MPI-internal function (MPIC_*, MPID_*, mpi_internal_*, ompi_coll_p2p_*) in the trace"
        elif grep -E "$internal_re" "$work/$t.names/functions.txt" | head -5 | grep . ; then
            fail "$t: stock library, but MPI-internal functions in the trace"
        fi
    fi
done

echo "== work dir: $work"
[ $status -eq 0 ] && echo "SMOKE PASSED" || echo "SMOKE FAILED"
exit $status
