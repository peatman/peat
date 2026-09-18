PEAT - Performance Estimation & Analysing Toolkit

Status: TECHNICAL PREVIEW!!! 
Experimental Branch!!!

However you're welcome to download/compile and use it in your own work.

Contains: omp.tool & mpi.tool 

## Repository Layout

- `mpi.tool/` - A set of MPI profiling/tracing/analysis tools (trace collector, statistic modules, GPU/collective analysers, trace viewer). See [mpi.tool/readme.md](mpi.tool/readme.md).
- `omp.tool/` - OpenMP profiling tool built on the OMPT callback interface (originally based on a customised Intel OpenMP runtime), producing per-region OpenMP statistics. See [omp.tool/readme.md](omp.tool/readme.md).
