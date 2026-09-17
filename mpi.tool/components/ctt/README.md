# ctt - Calls Tracing Tool

Part of peat.

## What it does

Post-processes `tractor` trace files into call-level reports: classifies MPI calls, computes per-call durations/occurrences per rank, and can select representative ranks (fastest/slowest) for further analysis.

Available on Linux (GNU Makefile) and Windows (MSVC solution).

## Layout

- `ctt.cpp`/`ctt.h` - core tracing/classification engine.
- `tevent.cpp`/`tevent.h` - timed-event bookkeeping shared with other analysis components.
- `main.cpp` - CLI entry point, argument parsing and driving the analysis.
- `Build.sh`/`BuildAll.sh`/`Makefile` - build scripts producing the `ctt` binary.

