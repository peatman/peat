# patches - MPI implementation patches

Part of the mpi.tool.

## What it does

Source patches applied to supported open-source MPI implementations so that `tractor` can hook into their internals (e.g. collective algorithm selection, request/communicator internals) where public MPI/PMPI interfaces aren't sufficient.

## Layout

- `mpich-4.3.2/mpich-4.3.2.patch` - patch for MPICH 4.3.2.
- `mpich-5.0.1/mpich-5.0.1.patch` - patch for MPICH 5.0.1.
- `mvapich-2.3.5/mvapich2-2.3.5.patch` and `mvapich-2.3.5/src/` - patch and collective call interceptor sources (`mpi/coll`) for MVAPICH2 2.3.5. (deprecated)
- `mvapich-4.1/mvapich-4.1.patch` - patch for MVAPICH 4.1.

 