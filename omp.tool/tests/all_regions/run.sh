#!/bin/bash
#
# Builds all_regions.c, runs it under libompstat, checks that the logs
# contain every event ompcalls.cpp can produce, and that bin/omp.stat.pl
# turns those logs into non-empty statistics / region / task / nested reports.
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

# Post-processor: feed run 1's logs to bin/omp.stat.pl and check that every
# report it produces actually saw the regions.
postproc=$here/../../bin/omp.stat.pl
if command -v perl >/dev/null 2>&1 && [ -f "$postproc" ]; then
    mask=$(ls omp_stat.log.*.0 | sed 's/\.0$//')
    if perl "$postproc" -i "$mask" -apc 127 -pc 253 -d 0.0001 \
        -os st.txt -oe ev.txt -or reg.txt -ot task.txt -on nest.txt 2> pp.err > /dev/null; then
        grep -q 'Ranks=1 .*Threads=4' st.txt ||
            { echo "FAIL [omp.stat.pl]: header is not Ranks=1 / Threads=4"; fail=1; }
        grep -qiE 'unknown switch|at .* line [0-9]+' pp.err &&
            { echo "FAIL [omp.stat.pl]: errors on stderr"; fail=1; }
        for sect in "Parallel Regions" "Single Section" "Critical Section" "Critical Barrier" \
                    "Plain Barrier" "Fork Barrier" "Join Barrier" "Task"; do
            awk -v s="Total $sect (" 'index($0, s) == 1 { getline; getline; n = split($0, f, ",");
                    for (i = 2; i <= n; i++) if (f[i] + 0 > 0) ok = 1 } END { exit !ok }' st.txt ||
                { echo "FAIL [omp.stat.pl]: no '$sect' activity in the statistics"; fail=1; }
        done
        awk -F, '/^Different Parallel regions/ { exit !($2 + 0 > 0) }' reg.txt ||
            { echo "FAIL [omp.stat.pl]: region report lists no parallel regions"; fail=1; }
        awk -F, '/Overall/ { getline; exit !($6 + 0 > 0) }' task.txt ||
            { echo "FAIL [omp.stat.pl]: task report counts no tasks"; fail=1; }
        grep -q ', S ,' nest.txt && grep -q ', M ,' nest.txt ||
            { echo "FAIL [omp.stat.pl]: nested report lacks serialized (S) / team (M) regions"; fail=1; }

        # switches that are easy to get wrong: -s <separator>, -apc one barrier kind at a time
        perl "$postproc" -i "$mask" -apc 0 -pc 4 -d 0.0001 -s ';' -os sep.txt > /dev/null 2>&1
        sed -n 4p sep.txt | grep -q ';' ||
            { echo "FAIL [omp.stat.pl]: -s ';' did not change the separator"; fail=1; }
        for opt in "16:Fork Barrier" "32:Join Barrier"; do
            perl "$postproc" -i "$mask" -apc "${opt%%:*}" -pc 4 -d 0.0001 -os apc.txt > /dev/null 2>&1
            awk -v s="Total ${opt#*:} (" 'index($0, s) == 1 { getline; getline; n = split($0, f, ",");
                    for (i = 2; i <= n; i++) if (f[i] + 0 > 0) ok = 1 } END { exit !ok }' apc.txt ||
                { echo "FAIL [omp.stat.pl]: -apc ${opt%%:*} shows no ${opt#*:} activity"; fail=1; }
        done
    else
        echo "FAIL [omp.stat.pl]: script exited with an error"; fail=1
    fi
else
    echo "SKIP [omp.stat.pl]: perl or $postproc not available" >&2
fi

rm -f omp_stat.log.*
OSL_ONDEMAND=1 ./all_regions > /dev/null || { echo "FAIL: on-demand run exited with an error"; exit 1; }
check ondemand STOD SPOD

[ $fail -eq 0 ] && echo "PASS: all intercepted regions present, omp.stat.pl reports are populated"
exit $fail
