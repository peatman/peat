# patches - MPI implementation patches

Part of the mpi.tool.

## What it does

Source patches applied to supported open-source MPI implementations so that `tractor` can hook into and trace their internals (e.g. collective algorithms) where public MPI/PMPI interfaces aren't sufficient.

## Layout

Each implementation has up to three independent patches (disjoint file sets; apply with `patch -p1` from the top of the source tree, in any order):

- `<name>.patch` - hooks for trace collection (libmpi side).
- `<name>.fortran.patch` - Fortran bindings (libmpifort / libmpi_mpifh side). The Fortran wrappers call `PMPI_*` by default; when the environment variable below is set (to anything other than `""` or `"0"`) they call the C `MPI_*` entry points instead, so a collector that wraps only the C layer (`tractor`) also sees Fortran calls (`mpif.h`, `use mpi` and `use mpi_f08`), exactly once per Fortran call. Note that for MPICH/MVAPICH the unpatched wrappers call C `MPI_*`, so with the variable unset a C-only collector sees no Fortran calls at all on a patched build.
  Other collectors: tools whose Fortran wrappers forward into the C layer (mpiP, TAU, `wrap.py`-based tools such as Caliper) replace the libmpifort wrappers and are unaffected either way. Tools whose Fortran wrappers forward to the Fortran `pmpi_*` routines (Extrae) count each Fortran call twice when the variable is set (once in their Fortran wrapper, once again when `pmpi_*` reaches the C `MPI_*` wrapper), so leave it unset with such tools.
- `<name>.transport.patch` - transport-layer changes (MVAPICH2 only: SMP/CMA and verbs proxies, PSM).
- `<name>.autoconf.patch` - build-system fix only (MPICH 3.4.3 only): its `src/mpl/configure.ac` nests unquoted `AC_TRY_RUN`/`AC_TRY_COMPILE` calls in the `linux86_cycle` timer check; autoconf 2.72+ wraps else-branches in `case e in ... esac`, and the rescanned text loses a `case`, so the regenerated MPL configure dies with `syntax error near unexpected token ';;'`. The patch replaces the block with the properly quoted `AC_RUN_IFELSE`/`AC_COMPILE_IFELSE` form from MPICH 4.3.2 (same checks, same results). Needed whenever `autogen.sh` is run with autoconf >= 2.72; harmless otherwise.

| directory          | patches                                                                                        | redirect variable            | notes                                                                                                                |
|--------------------|------------------------------------------------------------------------------------------------|------------------------------|----------------------------------------------------------------------------------------------------------------------|
| `mpich-3.4.3/`     | `mpich-3.4.3.patch`, `mpich-3.4.3.fortran.patch`, `mpich-3.4.3.autoconf.patch`                 | `MPICH_FORTRAN_MPI_REDIRECT` | hooks ported from mpich-4.3.2 (`mpi_internal_*` live in `group_translate_ranks.c`); run `autogen.sh` after patching. |
| `mpich-4.3.2/`     | `mpich-4.3.2.patch`, `mpich-4.3.2.fortran.patch`                                               | `MPICH_FORTRAN_MPI_REDIRECT` |                                                                                                                      |
| `mpich-5.0.1/`     | `mpich-5.0.1.patch`, `mpich-5.0.1.fortran.patch`                                               | `MPICH_FORTRAN_MPI_REDIRECT` |                                                                                                                      |
| `mpich-5.0.2/`     | `mpich-5.0.2.patch`, `mpich-5.0.2.fortran.patch`                                               | `MPICH_FORTRAN_MPI_REDIRECT` |                                                                                                                      |
| `mvapich-2.3.5/`   | `mvapich2-2.3.5.patch`, `mvapich2-2.3.5.fortran.patch`, `mvapich2-2.3.5.transport.patch`       | `MV2_FORTRAN_MPI_REDIRECT`   | (deprecated)                                                                                                         |
| `mvapich-2.3.7-1/` | `mvapich2-2.3.7-1.patch`, `mvapich2-2.3.7-1.fortran.patch`, `mvapich2-2.3.7-1.transport.patch` | `MV2_FORTRAN_MPI_REDIRECT`   | port from 2.3.5                                                                                                      |
| `mvapich-4.1/`     | `mvapich-4.1.patch`, `mvapich-4.1.fortran.patch`                                               | `MVP_FORTRAN_MPI_REDIRECT`   |                                                                                                                      |
| `openmpi-5.0.10/`  | `openmpi-5.0.10.patch`, `openmpi-5.0.10.fortran.patch`, `openmpi-5.0.10.ze.patch`              | `OMPI_FORTRAN_MPI_REDIRECT`  | untested, experimental; Level Zero: the modern `ze_api` is incompatible with the one referenced by Open MPI              |

Notes on the MPICH-family Fortran patches:
1. The wrapper sources are untouched; `mpi_fortimpl.h` and `wrappers_c/cdesc.h` add self-referencing macros (`#define MPI_Send (redirect() ? MPI_Send : PMPI_Send)`), 
the `use mpi_f08` interfaces without buffer arguments are re-bound to generated shims in `wrappers_c/f08_redirect.c`, and `maint/local_python/binding_f08.py` is changed accordingly so an `autogen.sh` run produces the same binding. 
2. All patches still contain the back-tracing of `parent_comm`, which is deprecated and no longer required by `tractor` to identify real ranks in internal communication. It was used before the creation and destruction of internal communicators was intercepted as well.
