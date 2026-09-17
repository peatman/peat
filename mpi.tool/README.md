PEAT/mpi.tool

Status: TECHNICAL PREVIEW!!!

You're welcome to download/compile and use it in your own work.

## What it is

Performance Estimation & Analysing Toolkit (PEAT): a collection of tools to collect, decode and analyse MPI application traces (message passing, collectives, GPU activity, PTP-synchronised timings), for Intel MPI, MPICH, MVAPICH and Open MPI.

## Layout

- `components/common/` - shared C++ sources used by all analysis tools: MPI function ID tables, communicator/transport modelling, PTP time correction, common file I/O and utility helpers.
- `components/tractor/` - the trace collector runtime (`libtractor.*.so`, `LD_PRELOAD`-based), the core data-gathering piece that produces the raw trace files consumed by everything else. See [components/tractor/README.md](components/tractor/README.md).
- `components/Tune/` - sample captured traces and small standalone MPI test programs (`tune_pet.c`) used to tune/validate PET's performance model.
- `components/mat/` - Matching and Statistic Module (mat): matches P2P/collective/GPU trace records across ranks and builds per-call statistics. See [components/mat/README.md](components/mat/README.md).
- `components/ctt/` - Calls Tracing Tool (ctt): post-processes traces into call-level timing/classification reports. See [components/ctt/README.md](components/ctt/README.md).
- `components/urpu/` - User Regions Processing Utility (urpu): aggregates statistics for user-instrumented code regions. See [components/urpu/README.md](components/urpu/README.md).
- `components/pet/` - Performance Estimation Tool (PET): models/predicts MPI performance using collected traces and network profiles. See [components/pet/README.md](components/pet/README.md).
- `components/qTV/` - Qt-based graphical trace viewer for visualising tractor traces. See [components/qTV/README.md](components/qTV/README.md).
- `examples/imbalance/` - sample MPI application/scripts used to demonstrate/reproduce load-imbalance analysis.
- `patches/` - source patches applied to supported MPI implementations (MPICH, MVAPICH) to enable tracing hooks. See [patches/README.md](patches/README.md).
