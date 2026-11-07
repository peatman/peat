#!/bin/bash
# Run serial mat and mat.omp on one raw trace and compare their output.
#   ./compare.sh <trace dir> [thread counts]        default thread counts: "1 2 3 4 8"
# MAT=<serial mat> and MAT_OMP=<mat.omp> select the binaries (default: mpi.tool/bin/mat, mpi.tool/bin/mat.omp).
# WORK=<dir> is where the runs go (default: <trace dir>.compare); every run gets its own copy of the raw trace.
# mat.omp must write the same files as serial mat: .mpi/.hwc/.trace.info identical after unpacking, every .bind file the
# same records (sorted: threads may write a rank's bind records in a different order), and the same statistics report
# apart from the timing lines. Differences are printed and kept in <WORK>/omp<N>/compare/. Exit status 1 on any.

set -u
trace=$(realpath "${1:?usage: compare.sh <trace dir> [thread counts]}")
threads=${2:-1 2 3 4 8}
here=$(cd "$(dirname "$0")" && pwd)
bin=$(realpath "$here/../../../../bin")
mat=$(realpath "${MAT:-$bin/mat}")
mat_omp=$(realpath "${MAT_OMP:-$bin/mat.omp}")
work=${WORK:-$trace.compare}
[ -x "$mat" ] || { echo "no serial mat: $mat"; exit 2; }
[ -x "$mat_omp" ] || { echo "no mat.omp: $mat_omp"; exit 2; }
ls "$trace"/tractor_t.mpi.* > /dev/null 2>&1 || { echo "no raw trace in $trace"; exit 2; }

unpack() { case $1 in *.gz) zcat "$1" ;; *) cat "$1" ;; esac ; }
# the report without timing/progress lines (they differ from run to run)
report() { grep -vE "Speed is|events per second|Total Time|Start processing|End processing|successfully closed|is handling rank" "$1" | sed "s|$(dirname "$1")|RUNDIR|g" ; }
now() { date +%s%N ; }
secs() { local d=$(( $(now) - $1 )) ; printf '%d.%02d' $((d / 1000000000)) $((d % 1000000000 / 10000000)) ; }

# run <binary> <dir> [threads]
run() {
    rm -rf "$2" && mkdir -p "$2" && cp "$trace"/tractor_t.* "$2"/
    local rc=0
    ( cd "$2" && OMP_NUM_THREADS=${3:-1} timeout 600 "$1" > mat.log 2>&1 ) || rc=$?
    return $rc
}

compare() {
    local ref=$1 new=$2 cmp=$2/compare f same=0 tag=${2##*/}
    rm -rf "$cmp" && mkdir -p "$cmp"
    diff <(cd "$ref/out" && ls) <(cd "$new/out" && ls) > "$cmp/files.diff" || {
        same=1; echo "    $tag: different output files (< serial, > $tag):"; sed 's/^/      /' "$cmp/files.diff"; }
    for f in $(cd "$ref/out" && ls) ; do
        [ -f "$new/out/$f" ] || continue
        if [[ $f == *.bind.* ]] ; then
            diff <(unpack "$ref/out/$f" | sort) <(unpack "$new/out/$f" | sort) > "$cmp/$f.diff"
        else
            diff <(unpack "$ref/out/$f") <(unpack "$new/out/$f") > "$cmp/$f.diff"
        fi
        if [ $? -ne 0 ] ; then
            same=1
            echo "    $tag: $f differs ($(grep -c '^[<>]' "$cmp/$f.diff") lines; < serial, > $tag):"
            head -8 "$cmp/$f.diff" | sed 's/^/      /'
        else
            rm -f "$cmp/$f.diff"
        fi
    done
    if ! diff <(report "$ref/mat.log") <(report "$new/mat.log") > "$cmp/report.diff" ; then
        same=1
        echo "    $tag: statistics report differs ($(grep -c '^[<>]' "$cmp/report.diff") lines):"
        head -8 "$cmp/report.diff" | sed 's/^/      /'
    else
        rm -f "$cmp/report.diff"
    fi
    [ $same -eq 0 ] && rm -rf "$cmp"
    return $same
}

status=0
mkdir -p "$work"
echo "== trace $trace ($(ls "$trace"/tractor_t.mpi.* | wc -l) ranks)"
start=$(now)
run "$mat" "$work/serial" || { echo "serial mat failed (log $work/serial/mat.log)"; tail -5 "$work/serial/mat.log"; exit 2; }
echo "  serial mat: $(secs $start) s"
for n in $threads ; do
    start=$(now)
    rc=0 ; run "$mat_omp" "$work/omp$n" "$n" || rc=$?
    t=$(secs $start)
    if [ $rc -ne 0 ] ; then
        echo "  FAIL mat.omp $n threads: exit status $rc ($t s, log $work/omp$n/mat.log)"; tail -5 "$work/omp$n/mat.log" | sed 's/^/      /'; status=1
    elif ! grep -q "End processing" "$work/omp$n/mat.log" ; then
        echo "  FAIL mat.omp $n threads: did not finish ($t s)"; status=1
    elif compare "$work/serial" "$work/omp$n" ; then
        echo "  ok   mat.omp $n threads: identical to serial mat ($t s)"
    else
        echo "  FAIL mat.omp $n threads: output differs ($t s, diffs in $work/omp$n/compare)"; status=1
    fi
done
exit $status
