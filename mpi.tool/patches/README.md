# patches - MPI implementation patches

Part of the mpi.tool.

## What it does

Source patches applied to supported open-source MPI implementations so that `tractor` can hook trace their internals (e.g. collective algorithm) where public MPI/PMPI interfaces aren't sufficient.

## Layout

Each implementation has up to three independent patches (disjoint file sets; apply with `patch -p1` from the top of the source tree, in any order):

- `<name>.patch` - hooks for trace collection (libmpi side).
- `<name>.fortran.patch` - Fortran bindings (libmpifort / libmpi_mpifh side). The Fortran wrappers call `PMPI_*` by default; when the environment variable below is set (to anything other than `""` or `"0"`) they call the C `MPI_*` entry points instead, so a collector that wraps only the C layer (tractor) also sees Fortran calls (mpif.h, `use mpi` and `use mpi_f08`), exactly once per Fortran call. Note that for MPICH/MVAPICH the unpatched wrappers call C `MPI_*`, so with the variable unset a C-only collector sees no Fortran calls at all on a patched build.
  Other collectors: tools whose Fortran wrappers forward into the C layer (mpiP, TAU, `wrap.py`-based tools such as Caliper) replace the libmpifort wrappers and are unaffected either way. Tools whose Fortran wrappers forward to the Fortran `pmpi_*` routines (Extrae) count each Fortran call twice when the variable is set (once in their Fortran wrapper, once again when `pmpi_*` reaches the C `MPI_*` wrapper), so leave it unset with such tools.
- `<name>.fab.patch` - fabric-layer changes (MVAPICH2 only: SMP/CMA and verbs proxies, PSM).

| directory          | patches                                                                                  | redirect variable            |Notes
|--------------------|------------------------------------------------------------------------------------------|------------------------------|
| `mpich-4.3.2/`     | `mpich-4.3.2.patch`, `mpich-4.3.2.fortran.patch`                                         | `MPICH_FORTRAN_MPI_REDIRECT` |
| `mpich-5.0.1/`     | `mpich-5.0.1.patch`, `mpich-5.0.1.fortran.patch`                                         | `MPICH_FORTRAN_MPI_REDIRECT` |
| `mvapich-2.3.5/`   | `mvapich2-2.3.5.patch`, `mvapich2-2.3.5.fortran.patch`, `mvapich2-2.3.5.fab.patch`       | `MV2_FORTRAN_MPI_REDIRECT`   | (deprecated)
| `mvapich-2.3.7-1/` | `mvapich2-2.3.7-1.patch`, `mvapich2-2.3.7-1.fortran.patch`, `mvapich2-2.3.7-1.fab.patch` | `MV2_FORTRAN_MPI_REDIRECT`   |
| `mvapich-4.1/`     | `mvapich-4.1.patch`, `mvapich-4.1.fortran.patch`                                         | `MVP_FORTRAN_MPI_REDIRECT`   |
| `openmpi-5.0.10/`  | `openmpi-5.0.10.patch`, `openmpi-5.0.10.fortran.patch`, `openmpi-5.0.10.ze.patch`        | `OMPI_FORTRAN_MPI_REDIRECT`  | untested, experimental, trace collection only; neither mat nor qTV recognise new functions, level zero: modern ze_api is incompatible with the one referred to in Open MPI

Notes on the MPICH-family Fortran patches: the wrapper sources are untouched; `mpi_fortimpl.h` and `wrappers_c/cdesc.h` add self-referencing macros (`#define MPI_Send (redirect() ? MPI_Send : PMPI_Send)`), 
the `use mpi_f08` interfaces without buffer arguments are re-bound to generated shims in `wrappers_c/f08_redirect.c`, and `maint/local_python/binding_f08.py` is changed accordingly so an `autogen.sh` run produces the same binding. 
