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

#include "mpi.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#define MAX_HOSTNAME 255
const long sizes[] = {0, 16, 64, 512, 4096, 131072, 1048576, 67108864, 268435456, 1073741824};
const long steps[] = {1, 1, 4, 16, 128, 4096, 65536, 1048576, 268435456};
const long iterations[] = {100, 100, 100, 100, 50, 20, 10, 10, 10};
#define DYNAMIC_ALLOC

int main(int argc, char **argv)
{
    MPI_Request req;
    MPI_Request reqs[2];
    MPI_Status status;
    MPI_Status statuses[2];
    int rank, size, requested = MPI_THREAD_SINGLE /*MPI_THREAD_MULTIPLE*/, provided;

#ifdef DYNAMIC_ALLOC
    char *buf;
    char *buf2;
#else
    char buf[1073741824 + 1000];
    char buf2[1073741824 + 1000];
#endif
    int i, j, k, num_reqs = 2;
    int send_delay = 0, recv_delay = 0, wait_delay = 0;
    long tune_mask = -1;
    long size_mask = -1;
    long long t_start, t_stop;
    MPI_Init_thread(&argc, &argv, requested, &provided);

    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

#ifdef DYNAMIC_ALLOC
    buf = malloc(1073741824 + 1000);
    buf2 = malloc(1073741824 + 1000);
#endif
    char *buf_bsend = buf;
    gethostname(buf, 1024);
    if (argc >= 3) {
        char *endptr;
        int radix = atoi(argv[1]);
        tune_mask = strtol(argv[2], &endptr, radix);
        if (argc >= 4)
            size_mask = strtol(argv[3], &endptr, radix);
    }
    char *num_req_env = getenv("PET_SIM_REQS");
    if (num_req_env)
        num_reqs = atoi(num_req_env);
    char *send_delay_env = getenv("PET_SEND_DELAY");
    if (send_delay_env)
        send_delay = atoi(send_delay_env);
    char *wait_delay_env = getenv("PET_WAIT_DELAY");
    if (wait_delay_env)
        wait_delay = atoi(wait_delay_env);
    char *recv_delay_env = getenv("PET_RECV_DELAY");
    if (recv_delay_env)
        recv_delay = atoi(recv_delay_env);
    printf("rank=%d, on host %s, tune_mask=%ld size_mask=%ld, levels=%d-%d\n", rank, buf, tune_mask, size_mask, requested, provided);
    printf("rank=%d,send_delay=%d, recv_delay=%d, wait_delay=%d\n", rank, send_delay, recv_delay, wait_delay);

    if (num_reqs <= 0)
        num_reqs = 2;
#if 1
#pragma omp target data map(from : buf, buf2)
#pragma omp parallel for
    for (i = 0; i < 1073741824 + 1000; i++) {
        buf[i] = i;
        buf2[i] = 1073741824 + 1000 - i;
    }
#endif
    t_start = __rdtsc();
    if (rank == 0)
        printf("Rank %d. Testing MPI_Send...\n", rank);
    // ordinal send
    if (tune_mask & 1) {
        for (k = 0; k < sizeof(steps) / sizeof(steps[0]); k++) {
            if (size_mask & (1 << k)) {
                if (rank == 0)
                    printf("Rank %d. Testing sizes %ld-%ld.\n", rank, sizes[k], sizes[k + 1]);
                for (j = sizes[k]; j < sizes[k + 1]; j += steps[k]) {
                    for (i = 0; i < iterations[k]; i++) {
                        if (rank & 1) {
                            if (recv_delay > 0)
                                usleep(recv_delay);
                            MPI_Recv(buf, j, MPI_CHAR, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
                            buf[0] = 0;
                        } else {
                            if (send_delay > 0)
                                usleep(send_delay);
                            MPI_Send(buf, j, MPI_CHAR, rank + 1, rank, MPI_COMM_WORLD);
                        }
                    }
                }
            } else if (rank == 0)
                printf("Rank=%d, Skipped sizes %ld-%ld.\n", rank, sizes[k], sizes[k + 1]);
        }
        if (rank == 0)
            printf("Rank=%d, Done.\n", rank);
    } else {
        if (rank == 0)
            printf("Rank=%d, Skipped.\n", rank);
    }
    // sync send
    if (rank == 0)
        printf("Rank %d. Testing MPI_Ssend...\n", rank);
    if (tune_mask & 2) {
        for (k = 0; k < sizeof(steps) / sizeof(steps[0]); k++) {
            if (size_mask & (1 << k)) {
                if (rank == 0)
                    printf("Rank %d. Testing sizes %ld-%ld.\n", rank, sizes[k], sizes[k + 1]);
                for (j = sizes[k]; j < sizes[k + 1]; j += steps[k]) {
                    for (i = 0; i < iterations[k]; i++) {
                        if (rank & 1) {
                            if (recv_delay > 0)
                                usleep(recv_delay);
                            MPI_Recv(buf, j, MPI_CHAR, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
                        } else {
                            if (send_delay > 0)
                                usleep(send_delay);
                            MPI_Ssend(buf, j, MPI_CHAR, rank + 1, rank, MPI_COMM_WORLD);
                        }
                    }
                }
            } else if (rank == 0)
                printf("Rank=%d, Skipped sizes %ld-%ld.\n", rank, sizes[k], sizes[k + 1]);
        }
        if (rank == 0)
            printf("Rank=%d, Done.\n", rank);
    } else {
        if (rank == 0)
            printf("Rank=%d, Skipped.\n", rank);
    }
    // ready send
    /*
        if (rank==0) printf("Rank %d. Testing MPI_Rsend...\n",rank);
        if ( tune_mask & 4 ) {
            for ( k = 0; k < sizeof( steps ) / sizeof( steps[0] ); k++ ) {
                if (size_mask&(1<<k) ) {
                            if (rank==0) printf( "Rank %d. Testing sizes %ld-%ld.\n",rank,sizes[k],sizes[k+1] );
                for ( j = sizes[k]; j < sizes[k + 1]; j += steps[k] ) {
                    for ( i = 0; i < iterations[k]; i++ ) {
                        if ( rank & 1 ) {
                            if (recv_delay>0) usleep( recv_delay );
                                                    // printf( "Rank=%d, Entering Recv.\n",rank );
                            MPI_Recv( buf, j, MPI_CHAR, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status );
                            //MPI_Recv( buf, j, MPI_CHAR, rank - 1, rank - 1, MPI_COMM_WORLD, &status );
                                                    // printf( "Rank=%d, Leaving Recv.\n",rank );
                        } else {
                            if (send_delay>0) usleep( send_delay );
                            MPI_Rsend( buf, j, MPI_CHAR, rank + 1, rank, MPI_COMM_WORLD );
                        }
                    }
                }
                } else if (rank==0) printf( "Rank=%d, Skipped sizes %ld-%ld.\n",rank,sizes[k],sizes[k+1] );
            }
            if (rank==0) printf( "Rank=%d, Done.\n",rank );
        } else {
            if (rank==0) printf( "Rank=%d, Skipped.\n",rank );
        }
    */
    // nonblocking send
    if (rank == 0)
        printf("Rank %d. Testing MPI_Isend...\n", rank);
    if (tune_mask & 8) {
        for (k = 0; k < sizeof(steps) / sizeof(steps[0]); k++) {
            if (size_mask & (1 << k)) {
                if (rank == 0)
                    printf("Rank %d. Testing sizes %ld-%ld.\n", rank, sizes[k], sizes[k + 1]);
                for (j = sizes[k]; j < sizes[k + 1]; j += steps[k]) {
                    for (i = 0; i < iterations[k]; i++) {
                        if (rank & 1) {
                            if (recv_delay > 0)
                                usleep(recv_delay);
                            MPI_Recv(buf, j, MPI_CHAR, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
                        } else {
                            if (send_delay > 0)
                                usleep(send_delay);
                            MPI_Isend(buf, j, MPI_CHAR, rank + 1, rank, MPI_COMM_WORLD, &req);
                            if (wait_delay > 0)
                                usleep(wait_delay);
                            MPI_Wait(&req, &status);
                        }
                    }
                }
            } else if (rank == 0)
                printf("Rank=%d, Skipped sizes %ld-%ld.\n", rank, sizes[k], sizes[k + 1]);
        }
        if (rank == 0)
            printf("Rank=%d, Done.\n", rank);
    } else {
        if (rank == 0)
            printf("Rank=%d, Skipped.\n", rank);
    }
    // nonblocking sync send
    if (rank == 0)
        printf("Rank %d. Testing MPI_Issend...\n", rank);
    if (tune_mask & 16) {
        for (k = 0; k < sizeof(steps) / sizeof(steps[0]); k++) {
            if (size_mask & (1 << k)) {
                if (rank == 0)
                    printf("Rank %d. Testing sizes %ld-%ld.\n", rank, sizes[k], sizes[k + 1]);
                for (j = sizes[k]; j < sizes[k + 1]; j += steps[k]) {
                    for (i = 0; i < iterations[k]; i++) {
                        if (rank & 1) {
                            if (recv_delay > 0)
                                usleep(recv_delay);
                            MPI_Recv(buf, j, MPI_CHAR, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
                        } else {
                            if (send_delay > 0)
                                usleep(send_delay);
                            MPI_Issend(buf, j, MPI_CHAR, rank + 1, rank, MPI_COMM_WORLD, &req);
                            if (wait_delay > 0)
                                usleep(wait_delay);
                            MPI_Wait(&req, &status);
                        }
                    }
                }
            } else if (rank == 0)
                printf("Rank=%d, Skipped sizes %ld-%ld.\n", rank, sizes[k], sizes[k + 1]);
        }
        if (rank == 0)
            printf("Rank=%d, Done.\n", rank);
    } else {
        if (rank == 0)
            printf("Rank=%d, Skipped.\n", rank);
    }
    /*
    // nonblocking ready send
        if (rank==0) printf("Rank %d. Testing MPI_Irsend...\n",rank);
        if ( tune_mask & 32 ) {
            for ( k = 0; k < sizeof( steps ) / sizeof( steps[0] ); k++ ) {
                if (size_mask&(1<<k) ) {
                            if (rank==0) printf( "Rank %d. Testing sizes %ld-%ld.\n",rank,sizes[k],sizes[k+1] );
                for ( j = sizes[k]; j < sizes[k + 1]; j += steps[k] ) {
                    for ( i = 0; i < iterations[k]; i++ ) {
                        if ( rank & 1 ) {
                            if (recv_delay>0) usleep( recv_delay );
                            MPI_Recv( buf, j, MPI_CHAR, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status );
                            //MPI_Recv( buf, j, MPI_CHAR, rank - 1, rank - 1, MPI_COMM_WORLD, &status );
                        } else {
                            if (send_delay>0) usleep( send_delay );
                            MPI_Irsend( buf, j, MPI_CHAR, rank + 1, rank, MPI_COMM_WORLD, &req );
                            if (wait_delay>0) usleep( wait_delay );
                            MPI_Wait( &req, &status );
                        }
                    }
                }
                } else if (rank==0) printf( "Rank=%d, Skipped sizes %ld-%ld.\n",rank,sizes[k],sizes[k+1] );
            }
            if (rank==0) printf( "Rank=%d, Done.\n",rank );
        } else {
            if (rank==0) printf( "Rank=%d, Skipped.\n",rank );
        }
    */
    // persistent send
    if (rank == 0)
        printf("Rank %d. Testing MPI_Start/send...\n", rank);
    if (tune_mask & 64) {
        for (k = 0; k < sizeof(steps) / sizeof(steps[0]); k++) {
            if (size_mask & (1 << k)) {
                if (rank == 0)
                    printf("Rank %d. Testing sizes %ld-%ld.\n", rank, sizes[k], sizes[k + 1]);
                for (j = sizes[k]; j < sizes[k + 1]; j += steps[k]) {
                    if ((rank & 1) == 0) {
                        MPI_Send_init(buf, j, MPI_CHAR, rank + 1, rank, MPI_COMM_WORLD, &req);
                    }
                    for (i = 0; i < iterations[k]; i++) {
                        if (rank & 1) {
                            if (recv_delay > 0)
                                usleep(recv_delay);
                            MPI_Recv(buf, j, MPI_CHAR, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
                        } else {
                            if (send_delay > 0)
                                usleep(send_delay);
                            MPI_Start(&req);
                            if (wait_delay > 0)
                                usleep(wait_delay);
                            MPI_Wait(&req, &status);
                        }
                    }
                    if ((rank & 1) == 0) {
                        MPI_Request_free(&req);
                    }
                }
            } else if (rank == 0)
                printf("Rank=%d, Skipped sizes %ld-%ld.\n", rank, sizes[k], sizes[k + 1]);
        }
        if (rank == 0)
            printf("Rank=%d, Done.\n", rank);
    } else {
        if (rank == 0)
            printf("Rank=%d, Skipped.\n", rank);
    }
    if (rank == 0)
        printf("Rank %d. Testing MPI_Start/ssend...\n", rank);
    if (tune_mask & 128) {
        for (k = 0; k < sizeof(steps) / sizeof(steps[0]); k++) {
            if (size_mask & (1 << k)) {
                if (rank == 0)
                    printf("Rank %d. Testing sizes %ld-%ld.\n", rank, sizes[k], sizes[k + 1]);
                for (j = sizes[k]; j < sizes[k + 1]; j += steps[k]) {
                    if ((rank & 1) == 0) {
                        MPI_Ssend_init(buf, j, MPI_CHAR, rank + 1, rank, MPI_COMM_WORLD, &req);
                    }
                    for (i = 0; i < iterations[k]; i++) {
                        if (rank & 1) {
                            if (recv_delay > 0)
                                usleep(recv_delay);
                            MPI_Recv(buf, j, MPI_CHAR, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
                        } else {
                            if (send_delay > 0)
                                usleep(send_delay);
                            MPI_Start(&req);
                            if (wait_delay > 0)
                                usleep(wait_delay);
                            MPI_Wait(&req, &status);
                        }
                    }
                    if ((rank & 1) == 0) {
                        MPI_Request_free(&req);
                    }
                }
            } else if (rank == 0)
                printf("Rank=%d, Skipped sizes %ld-%ld.\n", rank, sizes[k], sizes[k + 1]);
        }
        if (rank == 0)
            printf("Rank=%d, Done.\n", rank);
    } else {
        if (rank == 0)
            printf("Rank=%d, Skipped.\n", rank);
    }
    /*
        if (rank==0) printf("Rank %d. Testing MPI_Start/rsend...\n",rank);
        if ( tune_mask & 256 ) {
            for ( k = 0; k < sizeof( steps ) / sizeof( steps[0] ); k++ ) {
                if (size_mask&(1<<k) ) {
                            if (rank==0) printf( "Rank %d. Testing sizes %ld-%ld.\n",rank,sizes[k],sizes[k+1] );
                for ( j = sizes[k]; j < sizes[k + 1]; j += steps[k] ) {
                    if ( (rank & 1) == 0 ) {
                        MPI_Rsend_init( buf, j, MPI_CHAR, rank + 1, rank, MPI_COMM_WORLD, &req );
                    }
                    for ( i = 0; i < iterations[k]; i++ ) {
                        if ( rank & 1 ) {
                            if (recv_delay>0) usleep( recv_delay );
                            //MPI_Recv( buf, j, MPI_CHAR, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status );
                            MPI_Recv( buf, j, MPI_CHAR, rank - 1, rank - 1, MPI_COMM_WORLD, &status );
                        } else {
                            if (send_delay>0) usleep( send_delay );
                            MPI_Start( &req );
                            if (wait_delay>0) usleep( wait_delay );
                            MPI_Wait( &req, &status );
                        }
                    }
                    if ( (rank & 1) == 0 ) {
                        MPI_Request_free( &req );
                    }
                }
                } else if (rank==0) printf( "Rank=%d, Skipped sizes %ld-%ld.\n",rank,sizes[k],sizes[k+1] );
            }
            if (rank==0) printf( "Rank=%d, Done.\n",rank );
        } else {
            if (rank==0) printf( "Rank=%d, Skipped.\n",rank );
        }
    */
    /*
        if (rank==0) printf("Rank %d. Testing MPI_Start/bsend...\n",rank);
        if ( tune_mask & 512 ) {
            for ( k = 0; k < sizeof( steps ) / sizeof( steps[0] ); k++ ) {
                if (size_mask&(1<<k) ) {
                            if (rank==0) printf( "Rank %d. Testing sizes %ld-%ld.\n",rank,sizes[k],sizes[k+1] );
                for ( j = sizes[k]; j < sizes[k + 1]; j += steps[k] ) {
                    if ( (rank & 1) == 0 ) {
                        MPI_Bsend_init( buf, j, MPI_CHAR, rank + 1, rank, MPI_COMM_WORLD, &req );
                    }
                    for ( i = 0; i < iterations[k]; i++ ) {
                        if ( rank & 1 ) {
                            MPI_Recv( buf, j, MPI_CHAR, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status );
                        } else {
                            usleep( 10 );
                            MPI_Start( &req );
                            MPI_Wait( &req, &status );
                        }
                    }
                    if ( (rank & 1) == 0 ) {
                        MPI_Request_free( &req );
                    }
                }
                } else if (rank==0) printf( "Rank=%d, Skipped sizes %ld-%ld.\n",rank,sizes[k],sizes[k+1] );
            }
            if (rank==0) printf( "Rank=%d, Done.\n",rank );
        } else {
            if (rank==0) printf( "Rank=%d, Skipped.\n",rank );
        }
    */
    if (rank == 0)
        printf("Rank %d. Testing MPI_Startall/send...\n", rank);
    if (tune_mask & 512) {
        for (k = 0; k < sizeof(steps) / sizeof(steps[0]); k++) {
            if (size_mask & (1 << k)) {
                if (rank == 0)
                    printf("Rank %d. Testing sizes %ld-%ld.\n", rank, sizes[k], sizes[k + 1]);
                for (j = sizes[k]; j < sizes[k + 1]; j += steps[k]) {
                    if ((rank & 1) == 0) {
                        MPI_Send_init(buf, j, MPI_CHAR, rank + 1, rank, MPI_COMM_WORLD, &reqs[0]);
                        // MPI_Send_init( buf2, j, MPI_CHAR, rank + 1, rank, MPI_COMM_WORLD, &reqs[1] );
                    } else {
                        MPI_Recv_init(buf, j, MPI_CHAR, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &reqs[0]);
                        // MPI_Recv_init( buf2, j, MPI_CHAR, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &reqs[1] );
                    }
                    for (i = 0; i < iterations[k]; i++) {
                        if (!(rank & 1)) {
                            if (send_delay > 0)
                                usleep(send_delay);
                        } else {
                            if (recv_delay > 0)
                                usleep(recv_delay);
                        }
                        MPI_Startall(1, reqs);
                        if (wait_delay > 0)
                            usleep(wait_delay);
                        MPI_Waitall(1, reqs, statuses);
                    }
                    MPI_Request_free(&reqs[0]);
                    // MPI_Request_free( &reqs[1] );
                }
            } else if (rank == 0)
                printf("Rank=%d, Skipped sizes %ld-%ld.\n", rank, sizes[k], sizes[k + 1]);
        }
#if 0
		for ( k = 0; k < sizeof( steps ) / sizeof( steps[0] ); k++ ) {
		    if (size_mask&(1<<k) ) {
                        if (rank==0) printf( "Rank %d. Testing sizes %ld-%ld.\n",rank,sizes[k],sizes[k+1] );	
			for ( j = sizes[k]; j < sizes[k + 1]; j += steps[k] ) {
				if ( (rank & 1) == 0 ) {
					MPI_Send_init( buf, j, MPI_CHAR, rank + 1, rank, MPI_COMM_WORLD, &reqs[0] );
					MPI_Recv_init( buf2, j, MPI_CHAR, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &reqs[1] );
				} else {
					MPI_Send_init( buf, j, MPI_CHAR, rank - 1, rank, MPI_COMM_WORLD, &reqs[0] );
					MPI_Recv_init( buf2, j, MPI_CHAR, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &reqs[1] );
				}
				for ( i = 0; i < iterations[k]; i++ ) {
					if ( !(rank & 1) ) {
						usleep( 10 );
					}
					MPI_Startall( 2, reqs );
					MPI_Waitall( 2, reqs, statuses );
				}
				MPI_Request_free( &reqs[0] );
				MPI_Request_free( &reqs[1] );
			}
		    } else if (rank==0) printf( "Rank=%d, Skipped sizes %ld-%ld.\n",rank,sizes[k],sizes[k+1] );	
		}
#endif
        if (rank == 0)
            printf("Rank=%d, Done.\n", rank);
    } else {
        if (rank == 0)
            printf("Rank=%d, Skipped.\n", rank);
    }

    // collective bcast send
    if (rank == 0)
        printf("Rank %d. Testing MPI_Bcast/MPIC_Send...\n", rank);
    if (tune_mask & 1024) {
        for (k = 0; k < sizeof(steps) / sizeof(steps[0]); k++) {
            if (size_mask & (1 << k)) {
                if (rank == 0)
                    printf("Rank %d. Testing sizes %ld-%ld.\n", rank, sizes[k], sizes[k + 1]);
                for (j = sizes[k]; j < sizes[k + 1]; j += steps[k]) {
                    for (i = 0; i < iterations[k]; i++) {
                        if (rank & 1) {
                            if (recv_delay > 0)
                                usleep(recv_delay);
                        } else {
                            if (send_delay > 0)
                                usleep(send_delay);
                        }
                        MPI_Bcast(buf, j, MPI_CHAR, 0, MPI_COMM_WORLD);
                    }
                }
            } else if (rank == 0)
                printf("Rank=%d, Skipped sizes %ld-%ld.\n", rank, sizes[k], sizes[k + 1]);
        }
        if (rank == 0)
            printf("Rank=%d, Done.\n", rank);
    } else {
        if (rank == 0)
            printf("Rank=%d, Skipped.\n", rank);
    }

    // collective scatter send
    if (rank == 0)
        printf("Rank %d. Testing MPI_Scatter/MPIC_Isend...\n", rank);
    if (tune_mask & 2048) {
        for (k = 0; k < sizeof(steps) / sizeof(steps[0]); k++) {
            if (size_mask & (1 << k)) {
                if (rank == 0)
                    printf("Rank %d. Testing sizes %ld-%ld.\n", rank, sizes[k], sizes[k + 1]);
                for (j = sizes[k]; j < sizes[k + 1]; j += steps[k]) {
                    for (i = 0; i < iterations[k]; i++) {
                        if (rank & 1) {
                            if (recv_delay > 0)
                                usleep(recv_delay);
                        } else {
                            if (send_delay > 0)
                                usleep(send_delay);
                        }
                        MPI_Scatter(buf, j, MPI_CHAR, buf2, j, MPI_CHAR, 0, MPI_COMM_WORLD);
                    }
                }
            } else if (rank == 0)
                printf("Rank=%d, Skipped sizes %ld-%ld.\n", rank, sizes[k], sizes[k + 1]);
        }
        if (rank == 0)
            printf("Rank=%d, Done.\n", rank);
    } else {
        if (rank == 0)
            printf("Rank=%d, Skipped.\n", rank);
    }

    // nonblocking send
    if (rank == 0)
        printf("Rank %d. Testing Multiple MPI_Isend...\n", rank);
    if (tune_mask & 4096) {
        MPI_Status statuses[num_reqs];
        MPI_Request requests[num_reqs];
        if (rank == 0)
            printf("Number of requests=%d...\n", num_reqs);

        for (k = 0; k < sizeof(steps) / sizeof(steps[0]); k++) {
            if (size_mask & (1 << k)) {
                if (rank == 0)
                    printf("Rank %d. Testing sizes %ld-%ld.\n", rank, sizes[k], sizes[k + 1]);
                for (j = sizes[k]; j < sizes[k + 1]; j += steps[k]) {
                    for (i = 0; i < iterations[k]; i++) {
                        int cur_req;
                        if (rank & 1) {
                            if (recv_delay > 0)
                                usleep(recv_delay);
                            for (cur_req = 0; cur_req < num_reqs; cur_req++) {
                                MPI_Irecv(buf, j, MPI_CHAR, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &requests[cur_req]);
                            }
                        } else {
                            usleep(10);
                            for (cur_req = 0; cur_req < num_reqs; cur_req++) {
                                MPI_Isend(buf, j, MPI_CHAR, rank + 1, rank, MPI_COMM_WORLD, &requests[cur_req]);
                            }
                        }
                        if (wait_delay > 0)
                            usleep(wait_delay);
                        MPI_Waitall(num_reqs, requests, statuses);
                    }
                }
            } else if (rank == 0)
                printf("Rank=%d, Skipped sizes %ld-%ld.\n", rank, sizes[k], sizes[k + 1]);
        }
        if (rank == 0)
            printf("Rank=%d, Done.\n", rank);
    } else {
        if (rank == 0)
            printf("Rank=%d, Skipped.\n", rank);
    }

    if (rank == 0)
        printf("Rank %d. Testing Cross Multiple MPI_Isend...\n", rank);
    if (tune_mask & 8192) {
        MPI_Status statuses[num_reqs * 2];
        MPI_Request requests[num_reqs * 2];
        if (rank == 0)
            printf("Number of requests=%d...\n", num_reqs);

        for (k = 0; k < sizeof(steps) / sizeof(steps[0]); k++) {
            if (size_mask & (1 << k)) {
                if (rank == 0)
                    printf("Rank %d. Testing sizes %ld-%ld.\n", rank, sizes[k], sizes[k + 1]);
                for (j = sizes[k]; j < sizes[k + 1]; j += steps[k]) {
                    for (i = 0; i < iterations[k]; i++) {
                        int cur_req;
                        if (rank & 1) {
                            for (cur_req = 0; cur_req < num_reqs; cur_req++) {
                                MPI_Irecv(buf, j, MPI_CHAR, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &requests[2 * cur_req]);
                                MPI_Isend(buf, j, MPI_CHAR, rank - 1, rank, MPI_COMM_WORLD, &requests[2 * cur_req + 1]);
                            }
                        } else {
                            for (cur_req = 0; cur_req < num_reqs; cur_req++) {
                                MPI_Irecv(buf, j, MPI_CHAR, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &requests[2 * cur_req]);
                                MPI_Isend(buf, j, MPI_CHAR, rank + 1, rank, MPI_COMM_WORLD, &requests[2 * cur_req + 1]);
                            }
                        }
                        if (wait_delay > 0)
                            usleep(wait_delay);
                        MPI_Waitall(num_reqs * 2, requests, statuses);
                    }
                }
            } else if (rank == 0)
                printf("Rank=%d, Skipped sizes %ld-%ld.\n", rank, sizes[k], sizes[k + 1]);
        }
        if (rank == 0)
            printf("Rank=%d, Done.\n", rank);
    } else {
        if (rank == 0)
            printf("Rank=%d, Skipped.\n", rank);
    }

    // collective bcast send
    if (rank == 0)
        printf("Rank %d. Testing MPI_Ibcast/MPIC_Isend...\n", rank);
    if (tune_mask & 16384) {
        MPI_Status statuses[num_reqs * 3];
        MPI_Request requests[num_reqs * 3];
        MPI_Comm masters[num_reqs];
        MPI_Comm smps[num_reqs];
        MPI_Comm worlds[num_reqs];
        MPI_Comm comm_smp;
        MPI_Group grp_world, grp_smp, grp_master;
        char localhost[MAX_HOSTNAME];
        char all_hosts[size * MAX_HOSTNAME], *temp_host;
        gethostname(localhost, MAX_HOSTNAME);
        int local_ranks[size];
        int all_ranks[size];
        int rank_smp, local_ranks_num;

        MPI_Allgather(localhost, MAX_HOSTNAME, MPI_BYTE, all_hosts, MAX_HOSTNAME, MPI_BYTE, MPI_COMM_WORLD);

        for (i = 0, temp_host = all_hosts, local_ranks_num = 0; i < size; temp_host += MAX_HOSTNAME, i++) {
            if (strcmp(localhost, temp_host) == 0)
                local_ranks[local_ranks_num++] = i;
        }

        MPI_Comm_group(MPI_COMM_WORLD, &grp_world);
        MPI_Group_incl(grp_world, local_ranks_num, local_ranks, &grp_smp);
        MPI_Comm_create(MPI_COMM_WORLD, grp_smp, &comm_smp);
        printf("Rank %d check 1 ...\n", rank);

        MPI_Comm_rank(comm_smp, &rank_smp);
        i = (rank_smp == 0);
        MPI_Allgather(&i, 1, MPI_INT, all_ranks, 1, MPI_INT, MPI_COMM_WORLD);

        for (i = 0, local_ranks_num = 0; i < size; i++) {
            if (all_ranks[i] > 0)
                local_ranks[local_ranks_num++] = i;
        }
        printf("Rank %d check 2 ...\n", rank);

        MPI_Group_incl(grp_world, local_ranks_num, local_ranks, &grp_master);

        if (0) {
            for (i = 0; i < num_reqs; i++) {
                MPI_Comm_create(MPI_COMM_WORLD, grp_master, &masters[i]);
                MPI_Comm_create(MPI_COMM_WORLD, grp_smp, &smps[i]);
                MPI_Comm_create(MPI_COMM_WORLD, grp_world, &worlds[i]);
            }
        }
        MPI_Comm_free(&comm_smp);
        printf("Rank %d check 3 ...\n", rank);
        if (0) {
            if (rank == 0)
                printf("Number of requests=%d...\n", num_reqs);
            for (k = 0; k < sizeof(steps) / sizeof(steps[0]); k++) {
                if (size_mask & (1 << k)) {
                    if (rank == 0)
                        printf("Rank %d. Testing sizes %ld-%ld.\n", rank, sizes[k], sizes[k + 1]);
                    for (j = sizes[k]; j < sizes[k + 1]; j += steps[k]) {
                        for (i = 0; i < iterations[k]; i++) {
                            int cur_req;
                            if (rank & 1) {
                                if (recv_delay > 0)
                                    usleep(recv_delay);
                            } else {
                                if (send_delay > 0)
                                    usleep(send_delay);
                            }
                            for (cur_req = 0; cur_req < num_reqs; cur_req++) {
                                MPI_Ibcast(buf, j, MPI_CHAR, 0, smps[cur_req], &requests[cur_req * 3 + 0]);
                                MPI_Ibcast(buf, j, MPI_CHAR, 0, masters[cur_req], &requests[cur_req * 3 + 1]);
                                MPI_Ibcast(buf, j, MPI_CHAR, 0, worlds[cur_req], &requests[cur_req * 3 + 2]);
                            }
                            if (wait_delay > 0)
                                usleep(wait_delay);
                            MPI_Waitall(num_reqs * 3, requests, statuses);
                        }
                    }
                } else if (rank == 0)
                    printf("Rank=%d, Skipped sizes %ld-%ld.\n", rank, sizes[k], sizes[k + 1]);
            }
        }
        if (0) {
            for (i = 0; i < num_reqs; i++) {
                MPI_Comm_free(&masters[i]);
                MPI_Comm_free(&smps[i]);
                MPI_Comm_free(&worlds[i]);
            }
        }
        if (rank == 0)
            printf("Rank=%d, Done.\n", rank);
    } else {
        if (rank == 0)
            printf("Rank=%d, Skipped.\n", rank);
    }

    MPI_Alltoall(buf, 65536, MPI_CHAR, buf2, 65536, MPI_CHAR, MPI_COMM_WORLD);
    t_stop = __rdtsc();
    double t_time = ((double)t_stop - (double)t_start);
    t_time /= 3500000000.0;

    printf("rank=%d, time=%lld %lld %lld (%lf)\n", rank, t_start, t_stop, t_stop - t_start, t_time);
#ifdef DYNAMIC_ALLOC
    free(buf);
    free(buf2);
#endif
    MPI_Finalize();
    return 0;
}
