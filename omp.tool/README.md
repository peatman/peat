peat/omp.tool

Status: TECHNICAL PREVIEW!!!

However you're welcome to download/compile and use it in your own work but no extensive testing of this part was performed.
Based on customised Intel OpenMP 3.0 Runtime (2012-13) (no OMPT in those times yet).
Rewritten 2026 on OMPT callback with Anthropic Claude Sonnet support.

Known issues:
Since gnu compiler still doesn't have ompt implemented in its omp runtime it was only tested with Intel OneAPI compiler. It was compiled with gcc/g++ but now hooks were called.

## Layout

- `src/` - the tool itself: `ompcalls.cpp` (OMPT callback registration/handlers), `kmp_stat.cpp`/`kmp_stat.h` (per-region OpenMP statistic collection and reporting), `offconv.c` (offload/region name converter utility).
- `bin/` - helper scripts: `offconv.sh` wraps the offload converter, `omp.stat.pl` post-processes/summarises collected statistics.
- `examples/omptest/` - a small OpenMP test application (`omptest.c`) with run scripts for GNU and Intel compilers, used to exercise the tool.
- `tests/all_regions/` - `all_regions.c` uses every construct the tool intercepts; `run.sh [intel|gnu]` builds it, runs it under the tool and checks that every event type shows up in the logs (exit 77 = no OMPT runtime, skipped).
- `lib/` - build output location for the resulting OMPT statistic library.

