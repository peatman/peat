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

#define MAX_NAME_LEN 1024
#define MAX_DESC_LEN 1024

// #define READ_IDLEVAL //Intel MPI specific !!!

int main(int argc, char *argv[])
{
#ifdef READ_IDLEVAL
    MPI_T_pvar_session session_idle;
    MPI_T_pvar_handle handle_idle;
#endif
    int provided = MPI_THREAD_SINGLE;
#ifdef READ_IDLEVAL
    MPI_T_init_thread(MPI_THREAD_SINGLE, &provided);
#endif
    MPI_Init(&argc, &argv);
    // let make it easier and read number of nodes and ppn  from command line
#ifdef READ_IDLEVAL
    MPI_T_pvar_session_create(&session_idle);
    if (session_idle) {
        int count = 0;
        MPI_T_pvar_handle_alloc(session_idle, 0, NULL, &handle_idle, &count); // 0  - sec 1 - ticks?
    }
#endif
    if (argc >= 4) {
        double val_after = 0;
        double val_before = 0;
        double val = 0;
        double idle_val_after = 0;
        double idle_val_before = 0;
        double idle_val = 0;
        int rank = -1, size = -1;
        MPI_Comm_size(MPI_COMM_WORLD, &size);
        MPI_Comm_rank(MPI_COMM_WORLD, &rank);
        int nnodes = atoi(argv[1]);
        int ppn = atoi(argv[2]);
        int buf_size = atoi(argv[3]);
        int tot_it = atoi(argv[4]);
        if (rank == 0) {
            printf("nnodes=%d,ppn=%d,buf_size=%d,iterations=%d\n", nnodes, ppn, buf_size, tot_it);
        }
        // create some comms 1 rank from each node
        int ranks_comms[ppn][nnodes];
        MPI_Group groups_comms[ppn];
        MPI_Group groups_world;
        MPI_Comm comms[ppn];
        int r = 0;
        MPI_Comm_group(MPI_COMM_WORLD, &groups_world);
        for (int n = 0; n < nnodes; n++) {
            for (int p = 0; p < ppn; p++) {
                ranks_comms[p][n] = r++;
            }
        }
        for (int p = 0; p < ppn; p++) {
            MPI_Group_incl(groups_world, nnodes, ranks_comms[p], &groups_comms[p]);
            MPI_Comm_create(MPI_COMM_WORLD, groups_comms[p], &comms[p]);
            if (rank == 0) {
                printf("Comm Created. Ranks:");
                for (int n = 0; n < nnodes; n++) {
                    printf("%d ", ranks_comms[p][n]);
                }
                printf("\n");
            }
        }
        void *sendbuf;
        void *recvbuf;
        posix_memalign(&sendbuf, 4096, buf_size * nnodes);
        posix_memalign(&recvbuf, 4096, buf_size * nnodes);
        MPI_Barrier(MPI_COMM_WORLD);
        for (int c = 0; c < tot_it; c++) {
            val_before = MPI_Wtime();
#ifdef READ_IDLEVAL
            MPI_T_pvar_read(session_idle, handle_idle, &idle_val_before);
#endif
            MPI_Alltoall(sendbuf, buf_size, MPI_CHAR, recvbuf, buf_size, MPI_CHAR, comms[rank % ppn]);
#ifdef READ_IDLEVAL
            MPI_T_pvar_read(session_idle, handle_idle, &idle_val_after);
#endif
            val_after = MPI_Wtime();
            idle_val += (idle_val_after - idle_val_before);
            val += (val_after - val_before);
            MPI_Barrier(MPI_COMM_WORLD);
        }
        printf("Total time %.2lf, Idle time %.2lf\n", val, idle_val);
        free(sendbuf);
        free(recvbuf);
    }

#ifdef READ_IDLEVAL
    MPI_T_pvar_handle_free(session_idle, &handle_idle);
    MPI_T_pvar_session_free(&session_idle);
    MPI_T_finalize();
#endif
    MPI_Finalize();
    return 0;
}
