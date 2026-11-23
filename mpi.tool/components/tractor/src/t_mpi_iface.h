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

/* What the core needs of the MPI layer, without MPI: the core is built without <mpi.h> and does not know the MPI part of
 * a record (msl_mpi_event, t_mpi_event.h) nor any MPI handle type. The MPI layer (t_mpi.cpp, the generated wrappers,
 * t_comm.cpp) defines t_mpi_layer, a constant (static) initialization: valid as soon as the library is loaded, before any
 * constructor has run, so the core's constructor can use it. Without an MPI layer (t_mpi_layer is a weak reference, NULL) the
 * core runs as a single process.
 *
 * Handles cross this interface as 64-bit integers: an MPICH-family handle (an int) zero-extended, an Open MPI handle (a
 * pointer) as it is - the same values the trace has always shown.
 *
 * Rules as for t_shim_state.h: members are only appended, plain C types only, no #ifdef'd members. */
#ifndef __T_MPI_IFACE_H__
#define __T_MPI_IFACE_H__

#include "addr_entry.h"

#define T_MPI_IFACE_VERSION 1

#if defined(__CPLUSPLUS__) || defined(__cplusplus)
extern "C" {
#endif

/* the process groups of nodes_info(): all ranks (a private duplicate of MPI_COMM_WORLD), the ranks of this node, the
   first rank of every node */
enum t_mpi_group { T_MPI_WORLD = 0, T_MPI_SMP = 1, T_MPI_MASTER = 2 };
enum t_mpi_type { T_MPI_BYTE = 0, T_MPI_INT = 1, T_MPI_LONG_LONG = 2, T_MPI_DOUBLE = 3 };
enum t_mpi_op { T_MPI_MIN = 0, T_MPI_MAX = 1 };

#define T_MPI_MAX_BUFS 3

/* The parameters of a recorded MPI call (t_mpi::get_view), whatever its group: the writer prints the members of the
   group the function belongs to (mpi_func_ids.h group ranges), the others are 0. */
struct t_mpi_view {
    long long comm, newcomm, remotecomm; /* communicator handles */
    long long request;                   /* request handle, 0 for none or a dummy one (t_mpi::check_request) */
    long long message, window, fh;       /* message, window, file handles */
    long long transfer_bytes, offset, count, size;
    int peer, tag, root, ctl;
    int sr_bytes[2];
    int input_request_num, triggered_request_num, flag;
    void *request_addr;
    /* buffers (MPIBUFFERS, VIRT2PHYS builds of the MPI layer), in the order they are written */
    int num_bufs;
    const struct addr_entry *bufs[T_MPI_MAX_BUFS];
};

struct t_mpi {
    int version; /* T_MPI_IFACE_VERSION */
    int size;    /* sizeof(struct t_mpi) */

    /* identification, for the "Modules" remark: "MPI <version>.<subversion>(<library suffix>)[ MPI-internal]" */
    const char *modules;
    /* environment variables holding the rank in MPI_COMM_WORLD set by the launcher, NULL-terminated: the first one set
       makes the process an MPI rank (else the core treats it as a single process) */
    const char *const *rank_env;
    /* parts of the names of the MPI Fortran wrapper libraries, NULL-terminated: frames inside them are skipped by
       the stack unwinding, so the call site is the user's Fortran code */
    const char *const *fortran_libs;
    /* bytes of a record the MPI layer writes (header + MPI part, msl_mpi_event) */
    int record_size;

    /* life cycle: setup in the core's constructor of a rank (before MPI_Init: an engineering MPI creates internal
       communicators inside it); post_init after MPI_Init (the private world, communicator tracking); release before
       MPI_Finalize/MPI_Abort; end when the core is done */
    void (*setup)(void);
    void (*post_init)(void);
    void (*bind_comms)(void); /* collective: global communicator ids, before release */
    void (*release)(void);
    void (*end)(void);
    void (*abort)(int code); /* the job: tractor's own fatal errors */

    /* collective operations over a group (only post_init..release); rank/size are -1 where this process is not a
       member (not a node master) or the group does not exist yet */
    int (*group_rank)(int group);
    int (*group_size)(int group);
    void (*barrier)(int group);
    void (*gather)(int group, const void *send, int count, int type, void *recv, int root);
    void (*gatherv)(int group, const void *send, int count, int type, void *recv, const int *counts, const int *displs, int root);
    void (*allgather)(int group, const void *send, int count, int type, void *recv);
    void (*reduce)(int group, const void *send, void *recv, int count, int type, int op, int root);
    void (*allreduce)(int group, const void *send, void *recv, int count, int type, int op);
    /* the group T_MPI_SMP or T_MPI_MASTER of the given ranks of the world (collective over the world) */
    void (*create_group)(int group, int count, const int *world_ranks);
    /* ranks 0..count-1 of a group -> their world ranks */
    void (*translate_ranks)(int group, int count, int *world_ranks);

    /* PTPD: spawn the time daemon from rank 0 (1: started), stop it */
    int (*ptpd_start)(const char *exec, char **argv, int nprocs, const char *hostfile);
    void (*ptpd_stop)(void);

    /* writer: the parameters of a record (rec: the record as written; func_base: the base id of its function,
       mpi_func_ids.h, which picks the group); nothing for a function of another class */
    void (*get_view)(const void *rec, int func_base, struct t_mpi_view *view);
    long long (*check_request)(long long request); /* 0 for a dummy request (one that must not be tracked) */
    long long (*comm_null)(void);                  /* the MPI_COMM_NULL handle */
    /* global communicator id and world rank of a rank of a communicator at time tsc; resolve 0: as recorded */
    int (*global_comm_id)(long long comm, long long tsc, int is_internal, int resolve);
    int (*real_rank)(long long comm, int rank, long long tsc, int is_internal, int resolve);
    /* com file parts (gz: a gzFile) */
    void (*write_comms_report)(void *gz, int line_prefix);
    void (*write_predefined)(void *gz, int line_prefix);
    void (*write_layout)(void *gz, int line_prefix, int record_size, int transport_size);

    /* compress_events: a repeated call may take over the previous record of the same function unless that one is a
       test or probe that found something */
    int (*reusable)(const void *rec);

    /* app file: the communicators of all ranks (rank 0, after bind_comms) */
    void (*write_global_comms_report)(void *gz, int line_prefix);

    /* names of the functions (through ids, mpi_func_ids_through.h) as this MPI layer's build knows them (the core's own
       table lacks the MPI-specific ones), num_functions = MPI_MAX_FUNCID_THROUGH of its build */
    const char *const *function_names;
    int num_functions;
    /* the PTPD helper of this MPI ("ptpd.mpich5", next to the library or in ../bin) */
    const char *ptpd_name;
    /* 1: no hwloc topology in the com file (Intel MPI ships its own hwloc) */
    int skip_topology;
};

/* defined by the MPI layer; NULL without one */
extern const struct t_mpi t_mpi_layer __attribute__((weak));

#if defined(__CPLUSPLUS__) || defined(__cplusplus)
}
#endif
#endif /* __T_MPI_IFACE_H__ */
