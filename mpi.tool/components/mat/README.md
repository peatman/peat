# mat - Matching Tool

Part of peat.

## What it does

Reads the raw trace files produced by `tractor` (P2P, collective, GPU and communicator records), matches send/receive and collective calls across ranks, and builds per-function timing and message-size statistics (occurrences, durations, transferred bytes).

Buildable in two flavors from the same sources (`Makefile` `TARGET=`):
- `serial` - plain single-process `mat` binary, simplest option for traces that fit in one system's RAM.
- `impi`/`mvapich2`/`ompi` - MPI-parallelised `pmat.<target>` ("parallel mat") binary that distributes matching/statistics work across ranks, for extremely large traces where a single system's RAM isn't sufficient.

Available on Linux (GNU Makefile) and Windows (MSVC solution).

## Modes

- Default (binding only): writes the `.bind` files and `.trace.info` next to the trace (or into `-o <dir>`), without reading the
  timestamps of the trace. pet reads them together with the raw trace: `mat -i tractor_t`, then
  `pet -w <trace dir> -i tractor_t ...`. `-m`/`--matching-only` selects this mode too; it is kept for old command lines.
- `-f`/`--full`: also rewrites the `.mpi` files with absolute times into `./out` (or `-o <dir>`), corrects negative times and
  calculates the imbalance statistics. Neither pet nor qTV reads the rewritten `.mpi` files: both read the raw trace with
  the `.bind` files and the `.trace.info` (qTV is built with `TV_IGNORE_PROCESSED_OUTPUT`).


## Layout

- `mat.cpp`/`mat.h` - core matching/statistic engine.
- `collectives.cpp`/`.h` - collective-operation specific matching logic.
- `p2pal.cpp`/`.h` - point-to-point (send/recv) matching logic.
- `gpu.cpp`/`.h` - GPU activity record handling.
- `comm.cpp`/`.h`, `requests.cpp`/`.h`, `tevent.cpp`/`.h` - communicator, request and timed-event bookkeeping.
- `main.cpp` - CLI entry point, argument parsing and output file generation.
- `Build.sh`/`BuildAll.sh`/`Makefile` - build scripts; `Makefile TARGET=` selects `serial` (`mat`) or `impi`/`mvapich2`/`ompi` (`pmat.<target>`).

