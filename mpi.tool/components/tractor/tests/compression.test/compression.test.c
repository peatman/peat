/*
 * This file is part of PEAT (Performance Estimation & Analysing Toolkit).
 * Copyright (C) 2026 by Alex Knyazev
 *
 * PEAT is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * PEAT is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with PEAT.  If not, see <https://www.gnu.org/licenses/>.
 */

#include <malloc.h>
#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define MAX_NAME_LEN 1024
#define MAX_DESC_LEN 1024

int main(int argc, char *argv[])
{
    int provided = MPI_THREAD_SINGLE;
    MPI_T_init_thread(MPI_THREAD_SINGLE, &provided);
    MPI_Init(&argc, &argv);
    int rank = -1, size = -1;
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    if (rank == 0) {
        for (int i = 0; i < 100; i++) {
            usleep(100);
            MPI_Send(&i, 1, MPI_INT, 1, i, MPI_COMM_WORLD);
        }
        for (int i = 0; i < 100; i++) {
            usleep(100);
            MPI_Send(&i, 1, MPI_INT, 1, i, MPI_COMM_WORLD);
        }
        for (int i = 0; i < 100; i++) {
            usleep(100);
            MPI_Send(&i, 1, MPI_INT, 1, i, MPI_COMM_WORLD);
        }
    }
    if (rank == 1) {
        for (int i = 0; i < 100; i++) {
            int f = 0;
            MPI_Status s;
            do {

                MPI_Iprobe(0, i, MPI_COMM_WORLD, &f, &s);
            } while (f == 0);
            MPI_Recv(&f, 1, MPI_INT, 0, i, MPI_COMM_WORLD, &s);
        }
        for (int i = 0; i < 100; i++) {
            int f = 0;
            MPI_Status s;
            MPI_Request r;
            int b;
            MPI_Irecv(&b, 1, MPI_INT, 0, i, MPI_COMM_WORLD, &r);
            do {
                MPI_Test(&r, &f, &s);
            } while (f == 0);
        }
        for (int i = 0; i < 100; i++) {
            int f = 0;
            MPI_Status s;
            MPI_Request r;
            int b;
            MPI_Irecv(&b, 1, MPI_INT, 0, i, MPI_COMM_WORLD, &r);
            MPI_Wait(&r, &s);
        }
    }
    MPI_Finalize();
    return 0;
}
