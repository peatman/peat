/* Small reproducer for progress-driven nesting of colt records:
   an outstanding MPI_Ibcast is progressed from inside blocking collectives,
   and MPI_Comm_create waits on an internal nonblocking allreduce. */
#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv)
{
    int rank, size, i, iter;
    const int n = 64 * 1024; /* 256 KiB of ints */
    int *bbuf, *sbuf, *rbuf;
    MPI_Request req;
    MPI_Group world_group, sub_group;
    MPI_Comm subcomm;
    int *ranks;

    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    bbuf = (int *)malloc(n * sizeof(int));
    sbuf = (int *)malloc(size * 16 * sizeof(int));
    rbuf = (int *)malloc(size * 16 * sizeof(int));
    for (i = 0; i < n; i++)
        bbuf[i] = rank;
    memset(sbuf, rank, size * 16 * sizeof(int));

    for (iter = 0; iter < 3; iter++) {
        MPI_Ibcast(bbuf, n, MPI_INT, 0, MPI_COMM_WORLD, &req);
        MPI_Barrier(MPI_COMM_WORLD);
        MPI_Alltoall(sbuf, 16, MPI_INT, rbuf, 16, MPI_INT, MPI_COMM_WORLD);
        MPI_Wait(&req, MPI_STATUS_IGNORE);
    }

    ranks = (int *)malloc(size * sizeof(int));
    for (i = 0; i < size; i++)
        ranks[i] = i;
    MPI_Comm_group(MPI_COMM_WORLD, &world_group);
    MPI_Group_incl(world_group, size, ranks, &sub_group);
    MPI_Comm_create(MPI_COMM_WORLD, sub_group, &subcomm);
    MPI_Barrier(subcomm);
    MPI_Comm_free(&subcomm);
    MPI_Group_free(&sub_group);
    MPI_Group_free(&world_group);

    if (rank == 0)
        printf("nest_test done\n");
    free(ranks);
    free(bbuf);
    free(sbuf);
    free(rbuf);
    MPI_Finalize();
    return 0;
}
