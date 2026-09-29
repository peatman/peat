# patches - MPI implementation patches

Part of the mpi.tool.

## What it does

Source patches applied to supported open-source MPI implementations so that `tractor` can hook trace their internals (e.g. collective algorithm) where public MPI/PMPI interfaces aren't sufficient.

## Layout

Each implementation has two or three independent patches (disjoint file sets; apply with `patch -p1` from the top of the source tree, in any order):

- `<name>.patch` - hooks for trace collection (libmpi side: request/communicator helpers `mpi_internal_*`, `MPIC_*` as weak aliases of `PMPIC_*`, scheduler hooks; for MVAPICH2 2.3.x also the psm_queue.c / error_handling.c fixes).
- `<name>.fortran.patch` - Fortran bindings (libmpifort side). The Fortran wrappers call `PMPI_*` by default; when the redirect variable below is set (to anything other than `""` or `"0"`) they call the C `MPI_*` entry points instead, so a collector that wraps only the C layer (tractor) also sees Fortran calls (mpif.h, `use mpi` and `use mpi_f08`), exactly once per Fortran call. For MPICH 4.x/5.x and MVAPICH 4.x the `use mpi_f08` interfaces without buffer arguments are re-bound to generated shims in `wrappers_c/f08_redirect.c`, and `maint/local_python/binding_f08.py` is changed accordingly so an `autogen.sh` run produces the same binding; for MVAPICH2 2.3.x and MPICH 3.4.3 (no `binding_f08.py`) the `mpi_c_interface_nobuf.f90` is patched directly; for MPICH 3.4.3 `wrappers_c/cdesc.h` and `wrappers_c/Makefile.mk` are generated, so `wrappers_c/buildiface` is patched too and `autogen.sh` reproduces them.
- `<name>.autoconf.patch` - build-system fix only (MPICH 3.4.3): its `src/mpl/configure.ac` nests unquoted `AC_TRY_RUN`/`AC_TRY_COMPILE` calls in the `linux86_cycle` timer check; autoconf 2.72+ wraps else-branches in `case e in ... esac`, and the rescanned text loses a `case`, so the regenerated MPL configure dies with `syntax error near unexpected token ';;'`. Replaced with the properly quoted `AC_RUN_IFELSE`/`AC_COMPILE_IFELSE` block from MPICH 4.3.2 (same checks, same results). Needed whenever `autogen.sh` is run with autoconf >= 2.72 - and for 3.4.3 `autogen.sh` is required after patching, because any `Makefile.mk` change makes `make` want the exact automake (1.15) that produced `Makefile.in`.

| directory          | patches                                                                                   | redirect variable            | notes                                                                                                               |
|--------------------|-------------------------------------------------------------------------------------------|------------------------------|---------------------------------------------------------------------------------------------------------------------|
| `mpich-3.4.3/`     | `mpich-3.4.3.patch`, `mpich-3.4.3.fortran.patch`, `mpich-3.4.3.autoconf.patch`            | `MPICH_FORTRAN_MPI_REDIRECT` | hooks ported from mpich-4.3.2 (`mpi_internal_*` live in `group_translate_ranks.c`); run `autogen.sh` after patching |
| `mpich-4.3.2/`     | `mpich-4.3.2.patch`, `mpich-4.3.2.fortran.patch`                                          | `MPICH_FORTRAN_MPI_REDIRECT` |                                                                                                                     |
| `mpich-5.0.1/`     | `mpich-5.0.1.patch`, `mpich-5.0.1.fortran.patch`                                          | `MPICH_FORTRAN_MPI_REDIRECT` |                                                                                                                     |
| `mpich-5.0.2/`     | `mpich-5.0.2.patch`, `mpich-5.0.2.fortran.patch`                                          | `MPICH_FORTRAN_MPI_REDIRECT` |                                                                                                                     |
| `mvapich-2.3.5/`   | `mvapich2-2.3.5.patch`, `mvapich2-2.3.5.fortran.patch`                                    | `MV2_FORTRAN_MPI_REDIRECT`   | deprecated; creates `src/mpi/coll/helper_fns.c`                                                                     |
| `mvapich-2.3.7-1/` | `mvapich2-2.3.7-1.patch`, `mvapich2-2.3.7-1.fortran.patch`                                | `MV2_FORTRAN_MPI_REDIRECT`   | port of the 2.3.5 patch; creates `src/mpi/coll/helper_fns.c`                                                        |
| `mvapich-4.1/`     | `mvapich-4.1.patch`, `mvapich-4.1.fortran.patch`                                          | `MVP_FORTRAN_MPI_REDIRECT`   |                                                                                                                     |
