PEAT/mpi.tool

Status: TECHNICAL PREVIEW!!!

You're welcome to download/compile and use it in your own work.


## What it is

Performance Estimation & Analysing Toolkit (PEAT): a collection of tools to collect, decode and analyse MPI application traces (message passing, collectives, GPU activity, PTP-synchronised timings), for Intel MPI, MPICH, MVAPICH and Open MPI.

## Layout

- `components/common/` - shared C++ sources used by all analysis tools: MPI function ID tables, communicator/transport modelling, PTP time correction, common file I/O and utility helpers.
- `components/tractor/` - the trace collector runtime (`libtractor.*.so`, `LD_PRELOAD`-based), the core data-gathering piece that produces the raw trace files consumed by everything else. See [components/tractor/README.md](components/tractor/README.md).
- `components/Tune/` - sample captured traces and small standalone MPI test programs (`tune_pet.c`) used to tune/validate PET's performance model.
- `components/mat/` - Matching Tool: matches P2P/collective/GPU trace records across ranks and builds per-call statistics. See [components/mat/README.md](components/mat/README.md).
- `components/ctt/` - Calls Tracing Tool (ctt): post-processes traces into call-level timing/classification reports. See [components/ctt/README.md](components/ctt/README.md).
- `components/qTV/` - Qt-based graphical trace viewer for visualising tractor traces. See [components/qTV/README.md](components/qTV/README.md).
- `examples/imbalance/` - sample MPI application/scripts used to demonstrate/reproduce load-imbalance calculation failure in Intel MPI/APS.
- `examples/compressions/` - sample MPI application/scripts used to demonstrate event compression in tractor.
- `patches/` - source patches applied to supported MPI implementations (MPICH, MVAPICH) to enable tracing hooks. See [patches/README.md](patches/README.md).

## Acknowledgements

Some common classes and low level routines are partially based on implementations by Sergey Shalnov ([https://github.com/shssf](https://github.com/shssf)):
