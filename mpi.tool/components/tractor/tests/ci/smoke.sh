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
#
# mat runs in <test>.<names|ids>/serial, on its own copy of the raw trace, without switches (output in serial/out).
# SMOKE_PMAT=<pmat binary> also runs parallel mat (pmat.<target>, built with the same MPI) once per rank count in
# SMOKE_PMAT_RANKS (default "1 2 4"), each the same way in its own <test>.<names|ids>/pmat<N> (log pmat<N>/mat.log). It
# must exit 0, finish processing, report ("Rank:<r> is handling rank <t>.", printed only by a PEAT_MPI build) every trace
# rank exactly once from the right one of its N ranks (t % N) - a serial mat run under the launcher would not - and
# write the same output as serial mat: .mpi/.hwc/.trace.info identical after unpacking, .bind the same records after
# concatenating and sorting all bind files. Differences are printed and kept in pmat<N>/compare/.
# pmat is experimental: its failures are reported ("PMAT FAIL", a ::warning:: annotation on GitHub Actions) but do not
# change the exit status, unless SMOKE_PMAT_STRICT=1.

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
pmat=${SMOKE_PMAT:-}
pmat_ranks=${SMOKE_PMAT_RANKS:-1 2 4}
if [ -n "$pmat" ]; then
    pmat=$(realpath "$pmat")
    [ -x "$pmat" ] || { echo "no pmat: $pmat"; exit 2; }
fi

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

# pmat runs without tractor: launcher with <n> ranks, no LD_PRELOAD
pmat_launch() {
    if [ $openmpi -eq 1 ] ; then
        launch=(mpirun --oversubscribe -n "$1")
    else
        launch=(mpiexec -n "$1")
    fi
}

echo "== tractor: $lib (engineering=$engineering)"
[ -z "$pmat" ] || echo "== pmat: $pmat (ranks: $pmat_ranks)"
ldd "$lib" | grep -E "not found" && { echo "unresolved dependencies"; exit 1; }

status=0
fail() { echo "FAIL: $*"; status=1; }
pmat_status=0
pmat_fail() {
    echo "PMAT FAIL (experimental): $*"; pmat_status=1
    [ -z "${GITHUB_ACTIONS:-}" ] || echo "::warning title=pmat (experimental)::$*"
}

# new_run_dir <name>: <name>/ in the current (trace) directory with a copy of the raw trace
new_run_dir() {
    rm -rf "$1" && mkdir -p "$1" && cp tractor_t.* "$1"/
}

# show_crash <log>: the error lines of a failed run (MPI aborts, exceptions, signals), then its last lines
show_crash() {
    grep -iE "abort|fatal|error|unmatched|terminate|exception|signal|segmentation|killed" "$1" | grep -v "^Model contains" | head -10 | sed 's/^/        /'
    echo "        ... last lines:"; tail -n 8 "$1" | sed 's/^/        /'
}

unpack() { case $1 in *.gz) zcat "$1" ;; *) cat "$1" ;; esac ; }

# bind_where <out dir> <record>: the bind file(s) and line(s) holding <record>
bind_where() {
    local f
    for f in "$1"/*.bind.* ; do
        zcat "$f" | grep -nxF -- "$2" | sed "s|^\([0-9]*\):.*|$(basename "$f"):\1|"
    done | head -3 | paste -sd' '
}

# compare_out <serial run dir> <pmat run dir>: <run>/out must hold the same files; .mpi/.hwc/.trace.info identical after
# unpacking, .bind the same records once all files are concatenated and sorted (pmat may split them differently).
# Differences are printed (file, line, first records) and kept in full in <pmat run dir>/compare/.
compare_out() {
    local ref=$1/out new=$2/out cmp=$2/compare f nd same=0 rec where
    local tag=${2##*/}
    rm -rf "$cmp" && mkdir -p "$cmp"
    [ -d "$new" ] || { echo "      $tag: no output directory $new"; return 1; }
    diff <(cd "$ref" && ls | grep -v '\.bind\.') <(cd "$new" && ls | grep -v '\.bind\.') > "$cmp/files.diff" || {
        same=1; echo "      $tag: different output files (< serial, > $tag):"; sed 's/^/        /' "$cmp/files.diff"; }
    for f in $(cd "$ref" && ls | grep -v '\.bind\.') ; do
        [ -f "$new/$f" ] || continue
        if ! diff <(unpack "$ref/$f") <(unpack "$new/$f") > "$cmp/$f.diff" ; then
            same=1
            nd=$(grep -c '^[<>]' "$cmp/$f.diff")
            echo "      $tag: $f differs ($nd lines; line numbers serial,$tag; < serial, > $tag):"
            head -12 "$cmp/$f.diff" | sed 's/^/        /'
        else
            rm -f "$cmp/$f.diff"
        fi
    done
    if ! diff <(zcat "$ref"/*.bind.* | sort) <(zcat "$new"/*.bind.* | sort) > "$cmp/bind.diff" ; then
        same=1
        echo "      $tag: bind records differ: serial $(zcat "$ref"/*.bind.* | wc -l) records" \
             "($(for f in "$ref"/*.bind.* ; do zcat "$f" | wc -l ; done | paste -sd/)), $tag $(zcat "$new"/*.bind.* | wc -l)" \
             "($(for f in "$new"/*.bind.* ; do zcat "$f" | wc -l ; done | paste -sd/)); only in serial: $(grep -c '^<' "$cmp/bind.diff")," \
             "only in $tag: $(grep -c '^>' "$cmp/bind.diff"); first ones (record  [file:line]):"
        { grep '^<' "$cmp/bind.diff" | head -5 ; grep '^>' "$cmp/bind.diff" | head -5 ; } | while IFS= read -r rec ; do
            if [ "${rec:0:1}" = "<" ] ; then where=$(bind_where "$ref" "${rec:2}") ; else where=$(bind_where "$new" "${rec:2}") ; fi
            echo "        ${rec}  [$where]"
        done
    else
        rm -f "$cmp/bind.diff"
    fi
    [ $same -eq 0 ] && rm -rf "$cmp"
    return $same
}

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

    # every (p)mat run gets its own copy of the raw trace and runs there without switches (output in <run>/out)
    new_run_dir serial
    (cd serial && timeout 300 "$bin/mat" > mat.log 2>&1) || { tail -n 20 serial/mat.log ; fail "$t/$mode: mat" ; }
    if [ -n "$pmat" ] ; then
        local n ok= rc
        for n in $pmat_ranks ; do
            new_run_dir "pmat$n"
            pmat_launch "$n"
            rc=0 ; (cd "pmat$n" && timeout 300 "${launch[@]}" "$pmat" > mat.log 2>&1) || rc=$?
            if [ $rc -ne 0 ] ; then
                show_crash "pmat$n/mat.log" ; pmat_fail "$t/$mode: pmat on $n ranks: exit status $rc (log $d/pmat$n/mat.log)"
            elif ! grep -q "End processing" "pmat$n/mat.log" ; then
                tail -n 20 "pmat$n/mat.log" ; pmat_fail "$t/$mode: pmat on $n ranks did not finish"
            elif [ "$(sed -nE 's/^Rank:([0-9]+) is handling rank ([0-9]+)\.$/\1 \2/p' "pmat$n/mat.log" | sort -n -k2)" != \
                   "$(for r in $(seq 0 $((ranks - 1))) ; do echo "$((r % n)) $r" ; done)" ] ; then
                grep "is handling rank" "pmat$n/mat.log" ; pmat_fail "$t/$mode: pmat on $n ranks did not distribute the $ranks trace ranks over its ranks (not a PEAT_MPI build?)"
            elif compare_out "$d/serial" "$d/pmat$n" ; then
                ok+=" $n"
            else
                pmat_fail "$t/$mode: pmat on $n ranks: output differs from serial mat (diffs in $d/pmat$n/compare)"
            fi
        done
        echo "    $mode: pmat output identical to serial mat on ranks:${ok:- none}"
    fi
    # ctt: report on stdout, progress on stderr - keep them apart so progress can't split report lines
    timeout 300 "$bin/ctt" > ctt.log 2> ctt.err || { tail -n 20 ctt.log ctt.err ; fail "$t/$mode: ctt" ; }
    grep -q "End processing" ctt.log ctt.err    || fail "$t/$mode: ctt did not finish"
    if grep -h "not in the trace's function name/ID table" serial/mat.log ctt.log ctt.err ; then
        fail "$t/$mode: function ids missing from the .app name/ID table"
    fi
    grep "^Function " ctt.log | sort -u > functions.txt
    echo "    $mode: $(wc -l < functions.txt) functions in ctt report"
}

# MPI functions <test> calls: its own code (and test_common.h) preprocessed with this MPI's mpicc - so #if MPI_VERSION
# and similar blocks resolve as they were compiled - without mpi.h, limited to the functions tractor can record
# (with the Makefile's own flags: NO_MPI4 by default here, so the MPI-4 blocks don't count)
expected_calls() {
    # shellcheck disable=SC2046 # the flags are meant to split
    "${MPICC:-mpicc}" $(make -s -C "$src" print-cflags) -E "$src/$1_test.c" 2>/dev/null |
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
if [ -n "$pmat" ] ; then
    if [ $pmat_status -eq 0 ] ; then echo "PMAT PASSED"
    else
        echo "PMAT FAILED (experimental$([ "${SMOKE_PMAT_STRICT:-0}" = 1 ] && echo ", strict: counted" || echo ", not counted"))"
        [ "${SMOKE_PMAT_STRICT:-0}" = 1 ] && status=1
    fi
fi
[ $status -eq 0 ] && echo "SMOKE PASSED" || echo "SMOKE FAILED"
exit $status
