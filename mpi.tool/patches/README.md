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
- `<name>.autoconf.patch` - build-system fix only (MPICH 3.4.3 only): its `src/mpl/configure.ac` nests unquoted `AC_TRY_RUN`/`AC_TRY_COMPILE` calls in the `linux86_cycle` timer check; autoconf 2.72+ wraps else-branches in `case e in ... esac`, and the rescanned text loses a `case`, so the regenerated MPL configure dies with `syntax error near unexpected token ';;'`. The patch replaces the block with the properly quoted `AC_RUN_IFELSE`/`AC_COMPILE_IFELSE` form from MPICH 4.3.2 (same checks, same results). Needed whenever `autogen.sh` is run with autoconf >= 2.72; harmless otherwise.

| directory          | patches                                                                                  | redirect variable            | notes                                                                                                                |
|--------------------|------------------------------------------------------------------------------------------|------------------------------|----------------------------------------------------------------------------------------------------------------------|
| `mpich-3.4.3/`     | `mpich-3.4.3.patch`, `mpich-3.4.3.fortran.patch`, `mpich-3.4.3.autoconf.patch`           | `MPICH_FORTRAN_MPI_REDIRECT` | hooks ported from mpich-4.3.2 (`mpi_internal_*` live in `group_translate_ranks.c`); run `autogen.sh` after patching. |
| `mpich-4.3.2/`     | `mpich-4.3.2.patch`, `mpich-4.3.2.fortran.patch`                                         | `MPICH_FORTRAN_MPI_REDIRECT` |                                                                                                                      |
| `mpich-5.0.1/`     | `mpich-5.0.1.patch`, `mpich-5.0.1.fortran.patch`                                         | `MPICH_FORTRAN_MPI_REDIRECT` |                                                                                                                      |
| `mpich-5.0.2/`     | `mpich-5.0.2.patch`, `mpich-5.0.2.fortran.patch`                                         | `MPICH_FORTRAN_MPI_REDIRECT` |                                                                                                                      |
| `mvapich-2.3.5/`   | `mvapich2-2.3.5.patch`, `mvapich2-2.3.5.fortran.patch`, `mvapich2-2.3.5.fab.patch`       | `MV2_FORTRAN_MPI_REDIRECT`   | (deprecated)                                                                                                         |
| `mvapich-2.3.7-1/` | `mvapich2-2.3.7-1.patch`, `mvapich2-2.3.7-1.fortran.patch`, `mvapich2-2.3.7-1.fab.patch` | `MV2_FORTRAN_MPI_REDIRECT`   | port from 2.3.5                                                                                                      |
| `mvapich-4.1/`     | `mvapich-4.1.patch`, `mvapich-4.1.fortran.patch`                                         | `MVP_FORTRAN_MPI_REDIRECT`   |                                                                                                                      |
| `openmpi-5.0.10/`  | `openmpi-5.0.10.patch`, `openmpi-5.0.10.fortran.patch`, `openmpi-5.0.10.ze.patch`        | `OMPI_FORTRAN_MPI_REDIRECT`  | untested, experimental,  level zero: modern ze_api is incompatible with the one referred to in Open MPI              |

Notes on the MPICH-family Fortran patches: the wrapper sources are untouched; `mpi_fortimpl.h` and `wrappers_c/cdesc.h` add self-referencing macros (`#define MPI_Send (redirect() ? MPI_Send : PMPI_Send)`), 
the `use mpi_f08` interfaces without buffer arguments are re-bound to generated shims in `wrappers_c/f08_redirect.c`, and `maint/local_python/binding_f08.py` is changed accordingly so an `autogen.sh` run produces the same binding. 

## Differences from the `peat` repository

`~/peat/mpi.tool/patches/` carries the same implementations minus Open MPI, from the same sources; the patch sets differ as follows:

- **Only in `peat.fab`:** `openmpi-5.0.10/` (all three patches) and the `.fab.patch` fabric-layer patches for `mvapich-2.3.5/` and `mvapich-2.3.7-1/`.
- **Pre-completed request handles (all MPICH 3.4.3/4.3.2/5.0.x and MVAPICH 4.1 hook patches):** MPICH serves MPI_PROC_NULL and inline-completed sends/receives from a few statically allocated `MPIR_Request` objects (`MPIR_REQUEST_COMPLETE` .. `+MPIR_REQUEST_N_BUILTIN`, `MPIR_REQUEST_BUILTIN_COUNT` in 3.4.x), so every such call returns the same handle for its kind. `peat.fab`'s `mpi_internal_get_request`, `mpi_internal_get_requests` and `MPI_Check_dummy` map those handles to 0 (the tractor's "no request" value, distinct from `MPI_REQUEST_NULL`); `peat` returns the raw handle and only keeps the `#ifdef CHANNEL_MRAIL` `mv2_dummy_request` check inherited from the MVAPICH2 2.3.x patch, which is never compiled for MPICH. This is the only content difference in those hook patches (plus a one-word comment difference in the `version.m4` hunks of `mvapich-4.1.patch`). To move a hook patch between the repositories, swap just that block.
- **MVAPICH2 2.3.x hook patches:** `peat` keeps the `psm_queue.c` yield fix (and, for 2.3.5, the `error_handling.c` `strsignal` fix) inside `<name>.patch`; `peat.fab` moved them into `<name>.fab.patch` next to the SMP/CMA/verbs-proxy changes. The remaining hunks are identical.
- **Fortran patches** (`<name>.fortran.patch`) and `mpich-3.4.3.autoconf.patch` are byte-identical in both repositories.
