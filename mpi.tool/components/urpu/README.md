# urpu - User Regions Processing Utility

Part of peat.

## What it does

Aggregates and reports statistics for user code regions. Used for estimation purposes. Usualy you don't need this utility for performance analysis.

Buildable in two flavors from the same sources (`Makefile` `TARGET=`):
- `serial` - plain single-process `urpu` binary, simplest option for traces that fit in one system's RAM.
- `impi`/`mvapich2`/`ompi` - MPI-parallelised `purpu.<target>` ("parallel urpu") binary that distributes aggregation work across ranks, for extremely large traces where a single system's RAM isn't sufficient.

Available on Linux (GNU Makefile) and Windows (MSVC solution).

## Layout

- `urpu.cpp`/`urpu.h` - core region-statistic aggregation engine.
- `tevent.cpp`/`tevent.h` - timed-event bookkeeping shared with other analysis components.
- `main.cpp` - CLI entry point, argument parsing and driving the analysis.
- `Build.sh`/`BuildAll.sh`/`Makefile` - build scripts; `Makefile TARGET=` selects `serial` (`urpu`) or `impi`/`mvapich2`/`ompi` (`purpu.<target>`).

This component was used for estimations only so it's being maintained in compilable state only (if required in the future)
