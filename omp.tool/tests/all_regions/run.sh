#!/bin/bash
#
# Builds all_regions.c, runs it under libompstat and checks that the logs
# contain every event ompcalls.cpp can produce.
#
# usage: run.sh [intel|gnu]      (default: intel)
# env:   LIBOMPSTAT=<path>       override the tool library location
#
# exit: 0 pass, 1 events missing / run failed, 77 skipped (no OMPT runtime)

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
    if ! command -v icx >/dev/null 2>&1 && [ -f "$here/env.sh" ]; then
        . "$here/env.sh"
    fi
    if ! command -v icx >/dev/null 2>&1; then
        echo "icx not found (source the oneAPI environment first); skipping." >&2
        exit 77
    fi
fi

if [ ! -f "$LIBOMPSTAT" ]; then
    echo "$LIBOMPSTAT not found; build and install omp.tool/src first." >&2
    exit 1
fi

work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
cd "$work" || exit 1

$cc -o all_regions -O2 $ompflag -g "$here/all_regions.c" || exit 1

export OMP_TOOL=enabled
export OMP_TOOL_LIBRARIES=$LIBOMPSTAT
export OMP_NUM_THREADS=4
export OSL_FREQUENCY=2000000000
export OSL_FILE=omp_stat.log

# Run 1: record everything.
# Run 2: on-demand mode, only START/STOP_ONDEMAND are forced into the log
#        by omp_control_tool(), so they are checked separately.
fail=0
check() { # <label> <tag>...
    local label=$1 tag
    shift
    for tag in "$@"; do
        if ! cat omp_stat.log.* | grep -q "^OSE:$tag:"; then
            echo "FAIL [$label]: no $tag event in the log"
            fail=1
        fi
    done
}

rm -f omp_stat.log.*
./all_regions > /dev/null || { echo "FAIL: all_regions exited with an error"; exit 1; }
check all \
    AS AE F FC \
    MS ME SMS SME \
    JBS JBE FBS FBE PBS PBE \
    SS SE \
    CSS CSE CBS CBE \
    TS TE TWS TWE

rm -f omp_stat.log.*
OSL_ONDEMAND=1 ./all_regions > /dev/null || { echo "FAIL: on-demand run exited with an error"; exit 1; }
check ondemand STOD SPOD

[ $fail -eq 0 ] && echo "PASS: all intercepted regions present"
exit $fail
