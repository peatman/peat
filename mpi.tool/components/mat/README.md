# mat - Matching Tool

Part of peat.

## What it does

Reads the raw trace files produced by `tractor` (P2P, collective, GPU and communicator records), matches send/receive and collective calls across ranks, and builds per-function timing and message-size statistics (occurrences, durations, transferred bytes).

Buildable in two flavours from the same sources (`Makefile` `TARGET=`):
- `serial` - plain single-process `mat` binary, simplest option for traces that fit in one system's RAM.
- `impi`/`mvapich2`/`ompi` - MPI-parallelized `pmat.<target>` ("parallel mat") binary that distributes matching/statistics work across ranks, for extremely large traces where a single system's RAM isn't sufficient.

Available on Linux (GNU Makefile) and Windows (MSVC solution).


## Layout

- `mat.cpp`/`mat.h` - core matching/statistic engine.
- `collectives.cpp`/`.h` - collective-operation specific matching logic.
- `p2pal.cpp`/`.h` - point-to-point (send/recv) matching logic.
- `gpu.cpp`/`.h` - GPU activity record handling.
- `comm.cpp`/`.h`, `requests.cpp`/`.h`, `tevent.cpp`/`.h` - communicator, request and timed-event bookkeeping.
- `main.cpp` - CLI entry point, argument parsing and output file generation.
- `Build.sh`/`BuildAll.sh`/`Makefile` - build scripts; `Makefile TARGET=` selects `serial` (`mat`) or `impi`/`mvapich2`/`ompi` (`pmat.<target>`).

