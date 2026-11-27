peat/omp.tool

Status: TECHNICAL PREVIEW!!!

However you're welcome to download/compile and use it in your own work but no extensive testing of this part was performed.
Based on customised Intel OpenMP 3.0 Runtime (2012-13) (no OMPT in those times yet).
Rewritten 2026 on OMPT callback with Anthropic Claude Sonnet support.

Join barrier and fork barrier:
- JOIN_BARRIER_STARTS/ENDS is the implicit barrier at the end of a parallel region (`sync_region`, kind `implicit_parallel`). A worker only registers there, so its join barrier is short; the main thread waits there for the workers.
- FORK_BARRIER_STARTS/ENDS is the idle wait of a worker for the next parallel region. It starts when the worker has ended its implicit task (`implicit_task` end, which comes after the barrier events) and ends when the worker is released for the next region (`implicit_task` begin) or when the thread ends. The main thread has no fork barrier.
- Order on a worker: `MS JBS JBE ME FBS FBE MS ...`. The intervals follow each other, they are never nested (the former mapping took the fork barrier from `sync_region_wait`, which is nested in the join barrier, and is gone).
- The runtime has to dispatch the end events of the implicit barrier on a worker (`sync_region` end, `implicit_task` end) when the worker has left the join barrier, not when it is released for the next region. Intel libiomp5 and LLVM libomp are treated as the same runtime; without that fix the idle time is reported as join barrier and the fork barrier has zero length. `tests/barrier_gap` checks this.

Number of threads:
- The tool allocates one event chain per thread when the runtime initializes it. It must not ask the OpenMP runtime for the number of threads at that point (`omp_get_max_threads()` deadlocks on LLVM libomp, which holds its initialization lock while it calls the tool), so the number is taken from `OSL_MAX_THREADS`, else the first number of `OMP_NUM_THREADS`, else the number of processors.
- Threads beyond that number are not tracked: their events are not recorded and a warning is printed once. Set `OSL_MAX_THREADS` if the program uses more threads than `OMP_NUM_THREADS` (for example with `num_threads(n)` or `omp_set_num_threads(n)`).

Known issues:
Since gnu compiler still doesn't have ompt implemented in its omp runtime it was only tested with Intel OneAPI compiler. It was compiled with gcc/g++ but now hooks were called.

## Layout

- `src/` - the tool itself: `ompcalls.cpp` (OMPT callback registration/handlers), `kmp_stat.cpp`/`kmp_stat.h` (per-region OpenMP statistic collection and reporting), `offconv.c` (offload/region name converter utility).
- `bin/` - helper scripts: `offconv.sh` wraps the offload converter, `omp.stat.pl` post-processes/summarises collected statistics.
- `examples/omptest/` - a small OpenMP test application (`omptest.c`) with run scripts for GNU and Intel compilers, used to exercise the tool.
- `tests/barrier_gap/` - `gap.c` runs two parallel regions with a 300 ms serial gap; `run.sh [intel|gnu]` checks that on a worker the join barrier is short, the gap is reported as fork barrier and the events follow each other (`MS JBS JBE ME FBS FBE MS ...`); fails if the runtime reports the end of the worker's barrier late (exit 77 = no OMPT runtime, skipped).
- `tests/all_regions/` - `all_regions.c` uses every construct the tool intercepts; `run.sh [intel|gnu]` builds it, runs it under the tool and checks that every event type shows up in the logs (exit 77 = no OMPT runtime, skipped).
- `lib/` - build output location for the resulting OMPT statistic library.

