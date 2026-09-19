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

#include "common_plain_types.h"
#include "mpi.h"
#include "t_defines.h"
#include "zlib.h"
#include <malloc.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

MPI_Comm comm_smp = MPI_COMM_NULL, comm_master = MPI_COMM_NULL, parent_comm = MPI_COMM_NULL;
int rank_master = -1, size_master = -1;
int master_rank = -1;
int rank = -1, size = -1;
int rank_smp = -1, size_smp = -1;
int rank_correction = 0;
gzFile fps = 0;

#ifndef MAX_PATH
#define MAX_PATH 2048
#endif

#ifndef PROC_MAX_LINE
#define PROC_MAX_LINE 65536
#endif

#define TAG_SYNC 1
#define TAG_FOLLOWUP 2
#define TAG_DELAY_REQ 3
#define TAG_DELAYS_TF 4
#define TAG_START 10
#define TAG_STOP 11

#define MAX_ATTEMPTS 4
#ifndef USLEEP_TIME
#define USLEEP_TIME 1000
#endif

#ifndef MAX_HOSTNAME
#define MAX_HOSTNAME 256
#endif

#ifdef _TRACTOR_DIAG_
FILE *debug_fp;
#endif

/*
#ifdef _TRACTOR_DIAG_
#define debug_printf printf
#else
void debug_printf(const char *format, ...) {};
#endif
*/
unsigned long update_interval; // in usec
int exit_flag = 0, start_counting = 1, stop_counting = 0, sigterm_received = 0;
MPI_Request Rstart, Rstop;
MPI_Status Sstart, Sstop;

void ptp_rank_timer_init(char *filename, char *mode)
{
    char *all_hosts, *temp_host;
    char localhost[MAX_HOSTNAME];
    int local_ranks_num = 0;
    int *local_ranks;
    int *all_ranks;
    int i, j;
    FILE *tfp;
    long long proc_freq[2] = {0, 0};
    long long *all_proc_freqs;
    char tbuf[PROC_MAX_LINE];
    MPI_Status mpi_stat_tmp;
    gethostname(localhost, MAX_HOSTNAME);
    PMPI_Comm_rank(MPI_COMM_WORLD, &rank);
    PMPI_Comm_size(MPI_COMM_WORLD, &size);
    MPI_Group grp_world, grp_smp, grp_master;
    all_hosts = (char *)malloc(size * MAX_HOSTNAME * sizeof(char));
    all_ranks = (int *)malloc(size * sizeof(int));
    local_ranks = (int *)malloc(size * sizeof(int));
    all_proc_freqs = (long long *)malloc(size * sizeof(long long) * 2);
    PMPI_Allgather(localhost, MAX_HOSTNAME, MPI_BYTE, all_hosts, MAX_HOSTNAME, MPI_BYTE, MPI_COMM_WORLD);
    for (i = 0, temp_host = all_hosts; i < size; temp_host += MAX_HOSTNAME, i++) {
        if (strcmp(localhost, temp_host) == 0) {
            local_ranks[local_ranks_num++] = i;
        }
    }
    PMPI_Comm_group(MPI_COMM_WORLD, &grp_world);
    PMPI_Group_incl(grp_world, local_ranks_num, local_ranks, &grp_smp);
    PMPI_Comm_create(MPI_COMM_WORLD, grp_smp, &comm_smp);
    PMPI_Comm_rank(comm_smp, &rank_smp);
    PMPI_Comm_size(comm_smp, &size_smp);
    i = (rank_smp == 0);
    PMPI_Allgather(&i, 1, MPI_INT, all_ranks, 1, MPI_INT, MPI_COMM_WORLD);
    for (i = 0, local_ranks_num = 0; i < size; i++) {
        if (all_ranks[i] > 0) {
            local_ranks[local_ranks_num++] = i;
        }
    }
    PMPI_Group_incl(grp_world, local_ranks_num, local_ranks, &grp_master);
    PMPI_Comm_create(MPI_COMM_WORLD, grp_master, &comm_master);
#ifdef USE_NANO_TIMER
    // delay[] is captured in real nanosec below, so report a matching 1GHz
    // "frequency" (nanosec already IS ticks-at-1GHz) rather than this node's
    // actual CPU frequency, which would be the wrong unit for downstream math.
    proc_freq[0] = proc_freq[1] = PEAT_BILLION;
#else
    sprintf(tbuf, "/proc/cpuinfo");
    tfp = fopen(tbuf, "rt");
    if (tfp) {
        while (!feof(tfp)) {
            tbuf[0] = 0;
            fgets(tbuf, MAX_PATH, tfp);
            if (strstr(tbuf, "model name") && proc_freq[0] == 0) {
                char *freq_start = strstr(tbuf, "@");
                if (freq_start) {
                    int i = 0;
                    freq_start++;
                    while (freq_start[i] >= '0' && freq_start[i] <= '9' || freq_start[i] == ' ' || freq_start[i] == '.')
                        ++i;
                    int power = 0;
                    if (strstr(freq_start, "GHz"))
                        power = 1000000000;
                    if (strstr(freq_start, "MHz"))
                        power = 1000000;
                    if (strstr(freq_start, "kHz"))
                        power = 1000;
                    freq_start[i] = 0;
                    strcpy(tbuf, freq_start);
                    proc_freq[0] = (long long)(strtod(tbuf, &freq_start) * power);
                    if (proc_freq[0] > 0 && proc_freq[1] > 0)
                        break;
                }
            }

            if (strstr(tbuf, "cpu MHz") && proc_freq[1] == 0) {
                char *freq_start = strstr(tbuf, ":");
                if (freq_start) {
                    int i = 0;
                    freq_start++;
                    while (freq_start[i] >= '0' && freq_start[i] <= '9' || freq_start[i] == ' ' || freq_start[i] == '.')
                        ++i;
                    int power = 1000000;
                    freq_start[i] = 0;
                    strcpy(tbuf, freq_start);
                    proc_freq[1] = (long long)(strtod(tbuf, &freq_start) * power);
                    if (proc_freq[0] > 0 && proc_freq[1] > 0)
                        break;
                }
            }

            if (tbuf[0] == 0)
                break;
        };
        fclose(tfp);
    }
#endif
    if (comm_master != MPI_COMM_NULL) {
        PMPI_Comm_rank(comm_master, &rank_master);
        PMPI_Comm_size(comm_master, &size_master);
    }
    if (size_master > 0) {
        if (rank_master != rank) {
            debug_printf("ptpd: Rank master and world didn't match. Abort.\n");
            PMPI_Abort(MPI_COMM_WORLD, -3);
        }
        PMPI_Gather(proc_freq, 2, MPI_DOUBLE, all_proc_freqs, 2, MPI_DOUBLE, 0, comm_master);
        if (!rank) { // master
            fps = gzopen(filename, mode);
            if (fps == 0) {
                debug_printf("ptpd: Unable to open ptpd file. Abort...\n");
                PMPI_Abort(MPI_COMM_WORLD, -1);
            }
            for (i = 0; i < size_master; i++) {
                local_ranks[i] = i;
            }
            PMPI_Group_translate_ranks(grp_master, size_master, local_ranks, grp_world, all_ranks);
            gzprintf(fps, "%d:%d\n", PEAT_RT_PTP_Mode, rank_correction);
            gzprintf(fps, "%d:%d:%d", PEAT_RT_PTP_Ranks, 0, size_master);
            for (i = 0; i < size_master; i++) {
                gzprintf(fps, ":%d", all_ranks[i]);
            }
            gzprintf(fps, "\n");
            for (i = 0; i < size_master; i++) {
                gzprintf(fps, "%d:%d:%lld:%lld:%s\n", PEAT_RT_PTP_Nodes_Freq, i, all_proc_freqs[i * 2 + 0], all_proc_freqs[i * 2 + 1], &all_hosts[i * MAX_HOSTNAME]);
            }
        }
    }
    free(all_hosts);
    free(all_proc_freqs);
    free(local_ranks);
    free(all_ranks);
}

void ptp_rank_timer_step(void)
{
    long long delay[MAX_ATTEMPTS][4]; // T1,T2,T3,T4
    int i, j;
    MPI_Status mpi_stat_tmp;
    PEAT_Proc begin_core, end_core;
    PMPI_Bcast(&exit_flag, 1, MPI_INT, 0, MPI_COMM_WORLD); // broadcast exit flag
    // now Adjust "remote master" timers
    if (size_master > 0 && !exit_flag) {
        if (!rank) { // master
            // debug_printf(".");
            for (i = 1; i < size_master; i++) {
                for (j = 0; j < MAX_ATTEMPTS; j++) {
                    PMPI_Ssend(delay[j], 0, MPI_DOUBLE, i, TAG_SYNC, comm_master);
                    //                    usleep(USLEEP_TIME);
#ifdef USE_NANO_TIMER
                    delay[j][0] = peat_get_nano_sec();
#else
                    delay[j][0] = rdtsc();
#endif
                    PMPI_Send(delay[j], 0, MPI_DOUBLE, i, TAG_FOLLOWUP, comm_master);
                    //                    PMPI_Recv(delay[j], 0, MPI_DOUBLE, i, TAG_SYNC, comm_master, &mpi_stat_tmp);
                    PMPI_Recv(delay[j], 0, MPI_DOUBLE, i, TAG_DELAY_REQ, comm_master, &mpi_stat_tmp);
#ifdef USE_NANO_TIMER
                    delay[j][1] = peat_get_nano_sec();
#else
                    delay[j][1] = rdtsc();
#endif
                    PMPI_Recv(&delay[j][2], 2, MPI_DOUBLE, i, TAG_DELAYS_TF, comm_master, &mpi_stat_tmp);
                }
                long long min_delays[4] = {0, 0, 0, 0}; // T1,T2,T3,T4
                min_delays[0] = delay[0][0];
                min_delays[3] = delay[0][1];
                min_delays[1] = delay[0][2];
                min_delays[2] = delay[0][3];
                for (j = 1; j < MAX_ATTEMPTS; j++) {
#if 0
                    if(delay[j][2] - delay[j][0] < min_delays[1] - min_delays[0]) {
                        min_delays[0] = delay[j][0];
                        min_delays[1] = delay[j][2];
                    }
                    if(delay[j][1] - delay[j][3] < min_delays[3] - min_delays[2]) {
                        min_delays[3] = delay[j][1];
                        min_delays[2] = delay[j][3];
                    }
#else
                    if (delay[j][2] - delay[j][0] + delay[j][1] - delay[j][3] < min_delays[1] - min_delays[0] + min_delays[3] - min_delays[2]) {
                        min_delays[0] = delay[j][0];
                        min_delays[1] = delay[j][2];
                        min_delays[3] = delay[j][1];
                        min_delays[2] = delay[j][3];
                    }
#endif
                }
                gzprintf(fps, "%d:%d:%d:%d:%lld:%lld:%lld:%lld\n", PEAT_RT_PTP_Main_Record, i - rank_correction, begin_core, end_core, min_delays[0], min_delays[1], min_delays[2], min_delays[3]);
            }
        } else { // slave
            for (j = 0; j < MAX_ATTEMPTS; j++) {
                PMPI_Recv(delay[j], 0, MPI_DOUBLE, 0, TAG_SYNC, comm_master, &mpi_stat_tmp);
                PMPI_Recv(delay[j], 0, MPI_DOUBLE, 0, TAG_FOLLOWUP, comm_master, &mpi_stat_tmp);
#ifdef USE_NANO_TIMER
                delay[j][0] = peat_get_nano_sec();
#else
                delay[j][0] = rdtsc();
#endif
                //                PMPI_Ssend(delay[j], 0, MPI_DOUBLE, 0, TAG_SYNC, comm_master);
                //                usleep(USLEEP_TIME);
#ifdef USE_NANO_TIMER
                delay[j][1] = peat_get_nano_sec();
#else
                delay[j][1] = rdtsc();
#endif
                PMPI_Send(delay[j], 0, MPI_DOUBLE, 0, TAG_DELAY_REQ, comm_master);
                PMPI_Send(delay[j], 2, MPI_DOUBLE, 0, TAG_DELAYS_TF, comm_master);
            }
        }
    }
}
void ptp_rank_timer_finalize(void)
{
    if (comm_smp != MPI_COMM_NULL) {
        PMPI_Comm_free(&comm_smp);
    }
    if (comm_master != MPI_COMM_NULL) {
        PMPI_Comm_free(&comm_master);
    }
    if (fps) {
        gzclose(fps);
        fps = NULL;
    }
}

void term_handler(int i)
{
    debug_printf("Rank %d. SIGTERM received...Exiting...\n", rank);
    sigterm_received = 1;
    if (rank == 0) {
        exit_flag = 1; // exit sign from other ranks
    } else {
        PMPI_Isend(&sigterm_received, 1, MPI_INT, 0, TAG_STOP, MPI_COMM_WORLD, &Rstop);
    }
}
int main(int argc, char *argv[])
{
    int i, rank, size;
    char *filename, *mode;
    char *endptr = 0;
    struct sigaction sa;
    debug_printf("ptpd is launched\n");
    PMPI_Init(&argc, &argv);
    PMPI_Comm_rank(MPI_COMM_WORLD, &rank);
    PMPI_Comm_size(MPI_COMM_WORLD, &size);
    if (rank == 0) {
        if (argc < 5) {
            debug_printf("ptpd: Too few parameters. Abort.\n");
            PMPI_Abort(MPI_COMM_WORLD, -2);
        }
        update_interval = strtoul(argv[1], &endptr, 10);
        filename = argv[2];
        mode = argv[3];
        rank_correction = strtol(argv[4], &endptr, 10);
        PMPI_Comm_get_parent(&parent_comm);
        ptp_rank_timer_init(filename, mode);
        debug_printf("Size of COMM_WORLD is  %d...", size);
        debug_printf("Update interval is  %d...", update_interval);
        debug_printf("Size of master_com is  %d...", size_master);
    } else {
        ptp_rank_timer_init(NULL, NULL);
    }
    PMPI_Bcast(&update_interval, 1, MPI_UNSIGNED_LONG, 0, MPI_COMM_WORLD);
    if (rank == 0) {
        debug_printf("Update interval is broadcasted\n");
        if (parent_comm != MPI_COMM_NULL) {
            debug_printf("Waiting for the permission to start counting...");
            PMPI_Recv(&start_counting, 1, MPI_INT, 0, TAG_START, parent_comm, &Sstart); // wait for permission to count if started by MPI_Comm_spawn
            PMPI_Irecv(&stop_counting, 1, MPI_INT, 0, TAG_STOP, parent_comm, &Rstop);   // exit sign from parent
            debug_printf("Granted.\n");
        } else {
            // install sigterm handler in this case
            sa.sa_handler = term_handler;
            sigaction(SIGTERM, &sa, 0);
            debug_printf("Rank 0.SIGTERM handler installed.\n");
            PMPI_Irecv(&stop_counting, 1, MPI_INT, MPI_ANY_SOURCE, TAG_STOP, MPI_COMM_WORLD, &Rstop); // exit sign from other ranks
        }
    } else {
        //        PMPI_Irecv(&stop_counting,1,MPI_INT,0,TAG_STOP,MPI_COMM_WORLD,&Rstop); // exit sign from root
        // just install handler
        //        if (parent_comm==MPI_COMM_NULL) {
        sa.sa_handler = term_handler;
        sigaction(SIGTERM, &sa, 0);
        debug_printf("Rank %d.SIGTERM handler installed.\n", rank);
        //        }
    }
    // start counting
    PMPI_Bcast(&start_counting, 1, MPI_INT, 0, MPI_COMM_WORLD);
    if (rank == 0) {
        debug_printf("Counting is started.\n");
    }
    do {
        usleep(update_interval);
        ptp_rank_timer_step();
        if (rank == 0) {
            if (!exit_flag) {
                PMPI_Test(&Rstop, &exit_flag, &Sstop);
            }
            if (exit_flag) {
                debug_printf("\nRank 0. Stop counting Signal Received.\n");
                ptp_rank_timer_step();
            }
        } else {
            //            PMPI_Test(&Rstop,&exit_flag,&Sstop);
            if (parent_comm != MPI_COMM_NULL) {
                if (exit_flag) {
                    debug_printf("Rank %d.Stop counting Signal Received from root.\n", rank);
                }
            } else {
                if (sigterm_received) {
                    debug_printf("Rank %d.Stop counting Signal Received from SIGTERM.\n", rank);
                    sigterm_received = 0; /*avoid multiple printing */
                }
            }
        }
    } while (!exit_flag);
    PMPI_Barrier(MPI_COMM_WORLD);
    ptp_rank_timer_finalize();
    if (rank == 0) {
        if (parent_comm != MPI_COMM_NULL) {
            debug_printf("Sending acknowledge to parent comm...\n");
            PMPI_Send(&start_counting, 1, MPI_INT, 0, TAG_STOP, parent_comm);
        }
    }
    PMPI_Finalize();
    return (0);
}
