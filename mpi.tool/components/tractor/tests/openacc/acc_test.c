/* MPI + OpenACC test: exercises every construct tractor records through the OpenACC profiling interface
 * (device init, data create/delete, enter/exit data, update, compute construct, launch, upload/download, wait)
 * and verifies the numerical result, so it also works as a plain (non-traced) check.
 *   acc_test [iterations] */
#include <mpi.h>
#include <openacc.h>
#include <stdio.h>
#include <stdlib.h>

#define N 4096

int main(int argc, char **argv)
{
    int rank, size, iterations = argc > 1 ? atoi(argv[1]) : 5, errors = 0;
    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    double *a = malloc(N * sizeof(double)), *b = malloc(N * sizeof(double));
    for (int i = 0; i < N; i++) {
        a[i] = i;
        b[i] = 0;
    }

#pragma acc enter data copyin(a [0:N]) create(b [0:N])
    for (int it = 0; it < iterations; it++) {
#pragma acc parallel loop present(a, b)
        for (int i = 0; i < N; i++)
            b[i] = a[i] * 2.0 + rank;
#pragma acc parallel loop async(1) present(a, b)
        for (int i = 0; i < N; i++)
            a[i] = b[i] - rank - a[i]; /* a[i] unchanged: b - rank = 2a, minus a */
#pragma acc wait(1)
#pragma acc update self(b [0:N])
        for (int i = 0; i < N; i++)
            if (b[i] != i * 2.0 + rank)
                errors++;
        double local = b[N - 1], sum = 0;
        MPI_Allreduce(&local, &sum, 1, MPI_DOUBLE, MPI_SUM, MPI_COMM_WORLD);
        if (sum != (N - 1) * 2.0 * size + (double)size * (size - 1) / 2)
            errors++;
    }
#pragma acc kernels present(a)
    for (int i = 0; i < N; i++)
        a[i] += 1.0;
#pragma acc exit data copyout(a [0:N]) delete(b [0:N])
    for (int i = 0; i < N; i++)
        if (a[i] != i + 1.0)
            errors++;

    int total = 0;
    MPI_Allreduce(&errors, &total, 1, MPI_INT, MPI_SUM, MPI_COMM_WORLD);
    if (rank == 0)
        printf("acc_test: %s (%d errors)\n", total ? "FAILED" : "PASSED", total);
    free(a);
    free(b);
    MPI_Finalize();
    return total != 0;
}
