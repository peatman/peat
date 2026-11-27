#!/bin/bash
#
# Checks the join barrier (JB) and the fork barrier (FB) of a worker thread.
#
# gap.c runs two parallel regions with a 300 ms serial gap. For the worker the log must show,
# one after the other and never nested:
#
#   MS JBS JBE ME FBS FBE MS JBS JBE ME FBS FBE
#
#   JB  = JBS..JBE   the worker arrives at the end of the region and registers: short
#   FB  = FBS..FBE   idle wait for the next region (or for the end of the program): the gap, 300 ms
#
# The main thread (tid 0) has JB but no FB.
#
# A runtime that dispatches the end events of the implicit barrier of a worker only when the worker is
# released for the next region (no fix for the OMPT end events of the worker barrier) reports the gap as
# JB and an FB of zero length: this test fails then.
#
# usage: run.sh [intel|gnu]      (default: intel)
# env:   LIBOMPSTAT=<path>       override the tool library location
#
# exit: 0 pass, 1 fail, 77 skipped (no OMPT runtime)

compiler=${1:-intel}
here=$(cd "$(dirname "$0")" && pwd)
LIBOMPSTAT=${LIBOMPSTAT:-$here/../../lib/$compiler/libompstat.so}

if [ "$compiler" = gnu ]; then
    cc=gcc
    ompflag=-fopenmp
    libgomp=$(gcc -print-file-name=libgomp.so)
    if ! nm -D "$libgomp" | grep -q 'ompt_start_tool'; then
        echo "GNU libgomp does not provide OMPT; skipping." >&2
        exit 77
    fi
else
    cc=icx
    ompflag=-qopenmp
    if ! command -v icx >/dev/null 2>&1 && [ -f "$here/../all_regions/env.sh" ]; then
        . "$here/../all_regions/env.sh"
    fi
    if ! command -v icx >/dev/null 2>&1; then
        echo "icx not found (source the oneAPI environment first); skipping." >&2
        exit 77
    fi
fi
# CC=<compiler> ompflag can be overridden from the environment (e.g. clang -fopenmp)
cc=${TEST_CC:-$cc}
ompflag=${TEST_OMPFLAG:-$ompflag}

if [ ! -f "$LIBOMPSTAT" ]; then
    echo "$LIBOMPSTAT not found; build and install omp.tool/src first." >&2
    exit 1
fi

work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
cd "$work" || exit 1

$cc -o gap -O2 $ompflag -g "$here/gap.c" || exit 1

export OMP_TOOL=enabled
export OMP_TOOL_LIBRARIES=$LIBOMPSTAT
export OMP_NUM_THREADS=2
export OSL_FREQUENCY=2000000000
export OSL_FILE=omp_stat.log

rm -f omp_stat.log.*
./gap || { echo "FAIL: gap exited with an error"; exit 1; }

fail=0
# events of one thread in time order: "<tag> <seconds>" (sorted on the raw time stamp counter: events of a
# barrier are only a few hundred nanoseconds apart)
events() { # <tid>
    cat omp_stat.log.* | awk -F: -v t="$1" \
        '$1 == "OSE" && $4 == t && $2 ~ /^(MS|ME|JBS|JBE|FBS|FBE)$/ { printf "%s %.0f\n", $2, $7 }' |
        sort -k2 -n | awk -v f="$OSL_FREQUENCY" '{ printf "%s %.9f\n", $1, $2 / f }'
}

expect="MS JBS JBE ME FBS FBE MS JBS JBE ME FBS FBE"
worker=$(events 1 | awk '{ printf "%s ", $1 }; END { print "" }' | sed 's/ $//')
if [ "$worker" != "$expect" ]; then
    echo "FAIL: worker (tid 1) events"
    echo "  expected: $expect"
    echo "  found:    $worker"
    fail=1
fi

main=$(events 0 | awk '{ printf "%s ", $1 }; END { print "" }' | sed 's/ $//')
expect_main="MS JBS JBE ME MS JBS JBE ME"
if [ "$main" != "$expect_main" ]; then
    echo "FAIL: main thread (tid 0) events"
    echo "  expected: $expect_main"
    echo "  found:    $main"
    fail=1
fi

# durations of the first region of the worker: JB short, FB is the gap
jb=$(events 1 | awk '$1 == "JBS" { s = $2 } $1 == "JBE" && s != "" { printf "%.6f", $2 - s; exit }')
fb=$(events 1 | awk '$1 == "FBS" { s = $2 } $1 == "FBE" && s != "" { printf "%.6f", $2 - s; exit }')
echo "worker, first region: JB=${jb}s FB=${fb}s"
awk -v jb="$jb" 'BEGIN { exit !(jb != "" && jb < 0.05) }' ||
    { echo "FAIL: JB of the worker is not short (idle time reported as join barrier?)"; fail=1; }
awk -v fb="$fb" 'BEGIN { exit !(fb != "" && fb > 0.2) }' ||
    { echo "FAIL: FB of the worker is shorter than the 300 ms gap"; fail=1; }

[ $fail -eq 0 ] && echo "PASS: worker JB is short, the idle gap is FB, no overlap"
exit $fail
