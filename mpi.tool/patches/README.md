# patches - MPI implementation patches

Part of the mpi.tool.

## What it does

Source patches applied to supported open-source MPI implementations so that `tractor` can hook trace their internals (e.g. collective algorithm) where public MPI/PMPI interfaces aren't sufficient.

## Layout

Each implementation has two independent patches (disjoint file sets; apply with `patch -p1` from the top of the source tree, in any order):

- `<name>.patch` - hooks for trace collection (libmpi side: request/communicator helpers `mpi_internal_*`, `MPIC_*` as weak aliases of `PMPIC_*`, scheduler hooks; for MVAPICH2 2.3.x also the psm_queue.c / error_handling.c fixes).
- `<name>.fortran.patch` - Fortran bindings (libmpifort side). The Fortran wrappers call `PMPI_*` by default; when the redirect variable below is set (to anything other than `""` or `"0"`) they call the C `MPI_*` entry points instead, so a collector that wraps only the C layer (tractor) also sees Fortran calls (mpif.h, `use mpi` and `use mpi_f08`), exactly once per Fortran call. For MPICH 4.x/5.x and MVAPICH 4.x the `use mpi_f08` interfaces without buffer arguments are re-bound to generated shims in `wrappers_c/f08_redirect.c`, and `maint/local_python/binding_f08.py` is changed accordingly so an `autogen.sh` run produces the same binding; for MVAPICH2 2.3.x the generated `mpi_c_interface_nobuf.f90` is patched directly.

| directory          | patches                                                    | redirect variable            | notes |
|--------------------|------------------------------------------------------------|------------------------------|-------|
| `mpich-4.3.2/`     | `mpich-4.3.2.patch`, `mpich-4.3.2.fortran.patch`           | `MPICH_FORTRAN_MPI_REDIRECT` | |
| `mpich-5.0.1/`     | `mpich-5.0.1.patch`, `mpich-5.0.1.fortran.patch`           | `MPICH_FORTRAN_MPI_REDIRECT` | |
| `mvapich-2.3.5/`   | `mvapich2-2.3.5.patch`, `mvapich2-2.3.5.fortran.patch`     | `MV2_FORTRAN_MPI_REDIRECT`   | deprecated; creates `src/mpi/coll/helper_fns.c` |
| `mvapich-2.3.7-1/` | `mvapich2-2.3.7-1.patch`, `mvapich2-2.3.7-1.fortran.patch` | `MV2_FORTRAN_MPI_REDIRECT`   | port of the 2.3.5 patch; creates `src/mpi/coll/helper_fns.c` |
| `mvapich-4.1/`     | `mvapich-4.1.patch`, `mvapich-4.1.fortran.patch`           | `MVP_FORTRAN_MPI_REDIRECT`   | |
