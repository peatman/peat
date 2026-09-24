# patches - MPI implementation patches

Part of the mpi.tool.

## What it does

Source patches applied to supported open-source MPI implementations so that `tractor` can hook into their internals (e.g. collective algorithm selection, request/communicator internals) where public MPI/PMPI interfaces aren't sufficient.

## Layout

- `mpich-4.3.2/mpich-4.3.2.patch` - patch for MPICH 4.3.2.
- `mpich-5.0.1/mpich-5.0.1.patch` - patch for MPICH 5.0.1.
- `mvapich-2.3.5/mvapich2-2.3.5.patch` and `mvapich-2.3.5/src/` - patch and collective call interceptor sources (`mpi/coll`) for MVAPICH2 2.3.5. (deprecated)
- `mvapich-4.1/mvapich-4.1.patch` - patch for MVAPICH 4.1.
- `openmpi-5.0.10/`
                `openmpi-5.0.10.patch` - untested entierly experimental patch for OpenMPI 5.0.10. For trace COLLECTION only. Neither mat nor qTV recognise these new functions. 
                `openmpi-5.0.10.fortran.patch` - patch for fortran proxy functions, do not use this patched builds with other collectors (like mpiP): you will (at least) get incorrect statistic (or they may crash at all)
                `openmpi-5.0.10.ze.patch` - patch for level zero (modern ze_api is incompatible with one used/referred in openmpi).
 