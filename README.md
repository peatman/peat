PEAT - Performance (-Estimation &-) Analysing Toolkit

Status: TECHNICAL PREVIEW!!!

However you're welcome to download/compile and use it in your own work.
Estimation capability (pet/urpu/omp.tool) is excluded in main branch.

Contains: mpi.tool 

## Branches

- `main` (this branch) - MPI analysis only (`mpi.tool`). Trace collection (tractor), matching (mat), call tracing (ctt) and a basic version of the qTV trace viewer. The estimation components are not included.
- [`E&E`](https://github.com/peatman/peat/tree/E%26E) - **Experimental** branch, superset of `main`: it contains everything in `main` and adds the estimation capability. Status: technical preview, experimental. The trace format, MPI patches and other common parts are developed there first and then adopted into `main`, so `E&E` may contain changes not yet in `main`. Extra capabilities:
  - `mpi.tool/components/qTV` - a much more developed **qTV** trace viewer than the one in `main`. Adds a time axis with an overview bar, view history and bookmarks, search by rank/index or function name, statistics of a view or selected region (time per function, MPI time per rank, P2P messages per rank pair), `Analyse` actions that list views with late sender / late receiver messages, event filtering and highlighting, message-linked event framing, table copy/CSV export, snapshots (PNG/JPEG/PDF), and built-in help. `main` keeps the basic viewer. See the qTV README on `E&E`.
  - `mpi.tool/components/pet` - **PET** (Performance Estimation Tool): models MPI application performance from tractor traces plus network/fabric profiles (per-rank/thread timelines, communicators, GPU, OpenMP and fabric events), to estimate behaviour under different transports/networks. Not actively developed: it follows the current trace format and is kept in compilable state only. Non-blocking collectives are **not** supported.
  - `mpi.tool/components/urpu` - **urpu** (User Regions Processing Utility): aggregates statistics for user-instrumented code regions. Serial build, or MPI-parallel `purpu` for traces too large for one node's RAM.
  - `omp.tool` - OpenMP profiling tool built on OMPT callbacks, producing per-region statistics. No extensive testing so far; verified only with the Intel oneAPI compiler (the GNU OpenMP runtime has no OMPT support, so no hooks are called).

PET and urpu are only needed for estimation; they are not required for performance analysis.

## Repository Layout

- `mpi.tool/` - A set of MPI profiling/tracing/analysis tools (trace collector, statistic modules, GPU/collective analysers, trace viewer). See [mpi.tool/README.md](mpi.tool/README.md).

