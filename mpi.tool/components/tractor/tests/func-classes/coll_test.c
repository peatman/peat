/* Collective class test.
   Blocking, nonblocking and (MPI-4) persistent and large-count collectives on
   MPI_COMM_WORLD, plus neighborhood collectives on a periodic 1-D Cartesian
   communicator.
   Usage: coll_test [iterations]     (any number of ranks) */
#include "test_common.h"

#define M 16 /* elements per rank per collective */

static int *sbuf, *rbuf, *gbuf, *counts, *displs, *bdispls; /* bdispls: byte displacements */
static MPI_Datatype *types;
static MPI_Aint *adispls;
static MPI_Comm cart;
static int nb_sum, nb_prefix, nb_exprefix; /* expected reductions over ranks */

static void fill_vectors(void)
{
    int i;
    for (i = 0; i < g_size; i++) {
        counts[i] = M;
        displs[i] = i * M;
        bdispls[i] = i * M * (int)sizeof(int);
        adispls[i] = (MPI_Aint)(i * M * sizeof(int));
        types[i] = MPI_INT;
    }
}

static void blocking(int it)
{
    int root = it % g_size;
    int i;

    MPI_Barrier(MPI_COMM_WORLD);

    fill_int(sbuf, M, root + it);
    if (g_rank != root)
        memset(sbuf, 0, M * sizeof(int));
    MPI_Bcast(sbuf, M, MPI_INT, root, MPI_COMM_WORLD);
    check_int(sbuf, M, root + it, "Bcast");

    fill_int(sbuf, M, g_rank + it);
    MPI_Gather(sbuf, M, MPI_INT, gbuf, M, MPI_INT, root, MPI_COMM_WORLD);
    if (g_rank == root)
        for (i = 0; i < g_size; i++)
            check_int(gbuf + i * M, M, i + it, "Gather");
    MPI_Gatherv(sbuf, M, MPI_INT, gbuf, counts, displs, MPI_INT, root, MPI_COMM_WORLD);
    if (g_rank == root)
        for (i = 0; i < g_size; i++)
            check_int(gbuf + i * M, M, i + it, "Gatherv");

    for (i = 0; i < g_size; i++)
        fill_int(gbuf + i * M, M, i + it + 1);
    MPI_Scatter(gbuf, M, MPI_INT, rbuf, M, MPI_INT, root, MPI_COMM_WORLD);
    check_int(rbuf, M, g_rank + it + 1, "Scatter");
    MPI_Scatterv(gbuf, counts, displs, MPI_INT, rbuf, M, MPI_INT, root, MPI_COMM_WORLD);
    check_int(rbuf, M, g_rank + it + 1, "Scatterv");

    MPI_Allgather(sbuf, M, MPI_INT, gbuf, M, MPI_INT, MPI_COMM_WORLD);
    for (i = 0; i < g_size; i++)
        check_int(gbuf + i * M, M, i + it, "Allgather");
    MPI_Allgatherv(sbuf, M, MPI_INT, gbuf, counts, displs, MPI_INT, MPI_COMM_WORLD);
    for (i = 0; i < g_size; i++)
        check_int(gbuf + i * M, M, i + it, "Allgatherv");

    /* all-to-all: block j sent to rank j carries value g_rank*100+j+it */
    for (i = 0; i < g_size; i++)
        fill_int(gbuf + i * M, M, g_rank * 100 + i + it);
    MPI_Alltoall(gbuf, M, MPI_INT, rbuf, M, MPI_INT, MPI_COMM_WORLD);
    for (i = 0; i < g_size; i++)
        check_int(rbuf + i * M, M, i * 100 + g_rank + it, "Alltoall");
    MPI_Alltoallv(gbuf, counts, displs, MPI_INT, rbuf, counts, displs, MPI_INT, MPI_COMM_WORLD);
    for (i = 0; i < g_size; i++)
        check_int(rbuf + i * M, M, i * 100 + g_rank + it, "Alltoallv");
    MPI_Alltoallw(gbuf, counts, bdispls, types, rbuf, counts, bdispls, types, MPI_COMM_WORLD);
    for (i = 0; i < g_size; i++)
        check_int(rbuf + i * M, M, i * 100 + g_rank + it, "Alltoallw");

    /* reductions: every rank contributes rank+i */
    fill_int(sbuf, M, g_rank);
    MPI_Reduce(sbuf, rbuf, M, MPI_INT, MPI_SUM, root, MPI_COMM_WORLD);
    if (g_rank == root)
        for (i = 0; i < M; i++)
            EXPECT(rbuf[i] == nb_sum + i * g_size, "Reduce wrong at %d", i);
    MPI_Allreduce(sbuf, rbuf, M, MPI_INT, MPI_SUM, MPI_COMM_WORLD);
    for (i = 0; i < M; i++)
        EXPECT(rbuf[i] == nb_sum + i * g_size, "Allreduce wrong at %d", i);
    MPI_Allreduce(MPI_IN_PLACE, sbuf, M, MPI_INT, MPI_MAX, MPI_COMM_WORLD);
    check_int(sbuf, M, g_size - 1, "Allreduce in place");

    for (i = 0; i < g_size * M; i++)
        gbuf[i] = g_rank;
    MPI_Reduce_scatter(gbuf, rbuf, counts, MPI_INT, MPI_SUM, MPI_COMM_WORLD);
    for (i = 0; i < M; i++)
        EXPECT(rbuf[i] == nb_sum, "Reduce_scatter wrong at %d", i);
    MPI_Reduce_scatter_block(gbuf, rbuf, M, MPI_INT, MPI_SUM, MPI_COMM_WORLD);
    for (i = 0; i < M; i++)
        EXPECT(rbuf[i] == nb_sum, "Reduce_scatter_block wrong at %d", i);

    fill_int(sbuf, M, g_rank);
    MPI_Scan(sbuf, rbuf, M, MPI_INT, MPI_SUM, MPI_COMM_WORLD);
    for (i = 0; i < M; i++)
        EXPECT(rbuf[i] == nb_prefix + i * (g_rank + 1), "Scan wrong at %d", i);
    MPI_Exscan(sbuf, rbuf, M, MPI_INT, MPI_SUM, MPI_COMM_WORLD);
    if (g_rank > 0)
        for (i = 0; i < M; i++)
            EXPECT(rbuf[i] == nb_exprefix + i * g_rank, "Exscan wrong at %d", i);
}

static void nonblocking(int it)
{
    int root = it % g_size;
    MPI_Request reqs[16];
    int n = 0, i;
    static int *b[16];
    static int inited = 0;

    if (!inited) {
        for (i = 0; i < 16; i++)
            b[i] = (int *)calloc(g_size * M, sizeof(int));
        inited = 1;
    }

    fill_int(b[0], M, root + it);
    if (g_rank != root)
        memset(b[0], 0, M * sizeof(int));
    MPI_Ibarrier(MPI_COMM_WORLD, &reqs[n++]);
    MPI_Ibcast(b[0], M, MPI_INT, root, MPI_COMM_WORLD, &reqs[n++]);
    fill_int(b[1], M, g_rank + it);
    MPI_Igather(b[1], M, MPI_INT, b[2], M, MPI_INT, root, MPI_COMM_WORLD, &reqs[n++]);
    MPI_Igatherv(b[1], M, MPI_INT, b[3], counts, displs, MPI_INT, root, MPI_COMM_WORLD, &reqs[n++]);
    for (i = 0; i < g_size; i++)
        fill_int(b[4] + i * M, M, i + it + 1);
    MPI_Iscatter(b[4], M, MPI_INT, b[5], M, MPI_INT, root, MPI_COMM_WORLD, &reqs[n++]);
    MPI_Iscatterv(b[4], counts, displs, MPI_INT, b[6], M, MPI_INT, root, MPI_COMM_WORLD, &reqs[n++]);
    MPI_Iallgather(b[1], M, MPI_INT, b[7], M, MPI_INT, MPI_COMM_WORLD, &reqs[n++]);
    MPI_Iallgatherv(b[1], M, MPI_INT, b[8], counts, displs, MPI_INT, MPI_COMM_WORLD, &reqs[n++]);
    for (i = 0; i < g_size; i++)
        fill_int(b[9] + i * M, M, g_rank * 100 + i + it);
    MPI_Ialltoall(b[9], M, MPI_INT, b[10], M, MPI_INT, MPI_COMM_WORLD, &reqs[n++]);
    MPI_Ialltoallv(b[9], counts, displs, MPI_INT, b[11], counts, displs, MPI_INT, MPI_COMM_WORLD, &reqs[n++]);
    MPI_Ialltoallw(b[9], counts, bdispls, types, b[12], counts, bdispls, types, MPI_COMM_WORLD, &reqs[n++]);
    fill_int(b[13], M, g_rank);
    MPI_Ireduce(b[13], b[14], M, MPI_INT, MPI_SUM, root, MPI_COMM_WORLD, &reqs[n++]);
    MPI_Iallreduce(b[13], b[15], M, MPI_INT, MPI_SUM, MPI_COMM_WORLD, &reqs[n++]);
    MPI_Waitall(n, reqs, MPI_STATUSES_IGNORE);

    check_int(b[0], M, root + it, "Ibcast");
    if (g_rank == root)
        for (i = 0; i < g_size; i++) {
            check_int(b[2] + i * M, M, i + it, "Igather");
            check_int(b[3] + i * M, M, i + it, "Igatherv");
        }
    check_int(b[5], M, g_rank + it + 1, "Iscatter");
    check_int(b[6], M, g_rank + it + 1, "Iscatterv");
    for (i = 0; i < g_size; i++) {
        check_int(b[7] + i * M, M, i + it, "Iallgather");
        check_int(b[8] + i * M, M, i + it, "Iallgatherv");
        check_int(b[10] + i * M, M, i * 100 + g_rank + it, "Ialltoall");
        check_int(b[11] + i * M, M, i * 100 + g_rank + it, "Ialltoallv");
        check_int(b[12] + i * M, M, i * 100 + g_rank + it, "Ialltoallw");
    }
    if (g_rank == root)
        for (i = 0; i < M; i++)
            EXPECT(b[14][i] == nb_sum + i * g_size, "Ireduce wrong at %d", i);
    for (i = 0; i < M; i++)
        EXPECT(b[15][i] == nb_sum + i * g_size, "Iallreduce wrong at %d", i);

    /* the scan family and reduce_scatter, one at a time */
    n = 0;
    for (i = 0; i < g_size * M; i++)
        b[0][i] = g_rank;
    MPI_Ireduce_scatter(b[0], b[1], counts, MPI_INT, MPI_SUM, MPI_COMM_WORLD, &reqs[n++]);
    MPI_Ireduce_scatter_block(b[0], b[2], M, MPI_INT, MPI_SUM, MPI_COMM_WORLD, &reqs[n++]);
    fill_int(b[3], M, g_rank);
    MPI_Iscan(b[3], b[4], M, MPI_INT, MPI_SUM, MPI_COMM_WORLD, &reqs[n++]);
    MPI_Iexscan(b[3], b[5], M, MPI_INT, MPI_SUM, MPI_COMM_WORLD, &reqs[n++]);
    MPI_Waitall(n, reqs, MPI_STATUSES_IGNORE);
    for (i = 0; i < M; i++) {
        EXPECT(b[1][i] == nb_sum, "Ireduce_scatter wrong at %d", i);
        EXPECT(b[2][i] == nb_sum, "Ireduce_scatter_block wrong at %d", i);
        EXPECT(b[4][i] == nb_prefix + i * (g_rank + 1), "Iscan wrong at %d", i);
        if (g_rank > 0)
            EXPECT(b[5][i] == nb_exprefix + i * g_rank, "Iexscan wrong at %d", i);
    }
}

/* periodic 1-D ring: two neighbours (left, right), possibly the same rank */
static void neighborhood(int it)
{
    int right = (g_rank + 1) % g_size, left = (g_rank + g_size - 1) % g_size;
    int nsend[2 * M], nrecv[2 * M], nrecv2[2 * M];
    int ncounts[2] = { M, M }, ndispls[2] = { 0, M };
    MPI_Aint nadispls[2] = { 0, (MPI_Aint)(M * sizeof(int)) };
    MPI_Datatype ntypes[2] = { MPI_INT, MPI_INT };
    MPI_Request reqs[5];

    fill_int(nsend, M, g_rank + it);          /* block for left neighbour */
    fill_int(nsend + M, M, g_rank + it + 7);  /* block for right neighbour */

    MPI_Neighbor_allgather(nsend, M, MPI_INT, nrecv, M, MPI_INT, cart);
    check_int(nrecv, M, left + it, "Neighbor_allgather(left)");
    check_int(nrecv + M, M, right + it, "Neighbor_allgather(right)");
    MPI_Neighbor_allgatherv(nsend, M, MPI_INT, nrecv, ncounts, ndispls, MPI_INT, cart);
    check_int(nrecv, M, left + it, "Neighbor_allgatherv(left)");
    check_int(nrecv + M, M, right + it, "Neighbor_allgatherv(right)");

    /* alltoall: I receive left's "right block" and right's "left block" */
    MPI_Neighbor_alltoall(nsend, M, MPI_INT, nrecv, M, MPI_INT, cart);
    check_int(nrecv, M, left + it + 7, "Neighbor_alltoall(left)");
    check_int(nrecv + M, M, right + it, "Neighbor_alltoall(right)");
    MPI_Neighbor_alltoallv(nsend, ncounts, ndispls, MPI_INT, nrecv, ncounts, ndispls, MPI_INT, cart);
    check_int(nrecv, M, left + it + 7, "Neighbor_alltoallv(left)");
    check_int(nrecv + M, M, right + it, "Neighbor_alltoallv(right)");
    MPI_Neighbor_alltoallw(nsend, ncounts, nadispls, ntypes, nrecv, ncounts, nadispls, ntypes, cart);
    check_int(nrecv, M, left + it + 7, "Neighbor_alltoallw(left)");
    check_int(nrecv + M, M, right + it, "Neighbor_alltoallw(right)");

    MPI_Ineighbor_allgather(nsend, M, MPI_INT, nrecv, M, MPI_INT, cart, &reqs[0]);
    MPI_Ineighbor_alltoall(nsend, M, MPI_INT, nrecv2, M, MPI_INT, cart, &reqs[1]);
    MPI_Waitall(2, reqs, MPI_STATUSES_IGNORE);
    check_int(nrecv, M, left + it, "Ineighbor_allgather(left)");
    check_int(nrecv2, M, left + it + 7, "Ineighbor_alltoall(left)");
    MPI_Ineighbor_allgatherv(nsend, M, MPI_INT, nrecv, ncounts, ndispls, MPI_INT, cart, &reqs[0]);
    MPI_Ineighbor_alltoallv(nsend, ncounts, ndispls, MPI_INT, nrecv2, ncounts, ndispls, MPI_INT, cart, &reqs[1]);
    MPI_Waitall(2, reqs, MPI_STATUSES_IGNORE);
    check_int(nrecv + M, M, right + it, "Ineighbor_allgatherv(right)");
    check_int(nrecv2 + M, M, right + it, "Ineighbor_alltoallv(right)");
    MPI_Ineighbor_alltoallw(nsend, ncounts, nadispls, ntypes, nrecv, ncounts, nadispls, ntypes, cart, &reqs[0]);
    MPI_Wait(&reqs[0], MPI_STATUS_IGNORE);
    check_int(nrecv, M, left + it + 7, "Ineighbor_alltoallw(left)");
}

#if HAVE_MPI4
#define NPERS 13
static MPI_Request pers[NPERS];
static int *pb[2 * NPERS];
static int pers_root = 0;

static void persistent_init(void)
{
    int i, n = 0;
    for (i = 0; i < 2 * NPERS; i++)
        pb[i] = (int *)calloc((g_size > 2 ? g_size : 2) * M, sizeof(int)); /* neighbourhood buffers hold 2 blocks */
    MPI_Barrier_init(MPI_COMM_WORLD, MPI_INFO_NULL, &pers[n++]);
    MPI_Bcast_init(pb[0], M, MPI_INT, pers_root, MPI_COMM_WORLD, MPI_INFO_NULL, &pers[n++]);
    MPI_Gather_init(pb[1], M, MPI_INT, pb[2], M, MPI_INT, pers_root, MPI_COMM_WORLD, MPI_INFO_NULL, &pers[n++]);
    MPI_Gatherv_init(pb[1], M, MPI_INT, pb[3], counts, displs, MPI_INT, pers_root, MPI_COMM_WORLD, MPI_INFO_NULL, &pers[n++]);
    MPI_Scatter_init(pb[4], M, MPI_INT, pb[5], M, MPI_INT, pers_root, MPI_COMM_WORLD, MPI_INFO_NULL, &pers[n++]);
    MPI_Scatterv_init(pb[4], counts, displs, MPI_INT, pb[6], M, MPI_INT, pers_root, MPI_COMM_WORLD, MPI_INFO_NULL, &pers[n++]);
    MPI_Allgather_init(pb[1], M, MPI_INT, pb[7], M, MPI_INT, MPI_COMM_WORLD, MPI_INFO_NULL, &pers[n++]);
    MPI_Allgatherv_init(pb[1], M, MPI_INT, pb[8], counts, displs, MPI_INT, MPI_COMM_WORLD, MPI_INFO_NULL, &pers[n++]);
    MPI_Alltoall_init(pb[9], M, MPI_INT, pb[10], M, MPI_INT, MPI_COMM_WORLD, MPI_INFO_NULL, &pers[n++]);
    MPI_Alltoallv_init(pb[9], counts, displs, MPI_INT, pb[11], counts, displs, MPI_INT, MPI_COMM_WORLD, MPI_INFO_NULL, &pers[n++]);
    MPI_Alltoallw_init(pb[9], counts, bdispls, types, pb[12], counts, bdispls, types, MPI_COMM_WORLD, MPI_INFO_NULL, &pers[n++]);
    MPI_Reduce_init(pb[13], pb[14], M, MPI_INT, MPI_SUM, pers_root, MPI_COMM_WORLD, MPI_INFO_NULL, &pers[n++]);
    MPI_Allreduce_init(pb[13], pb[15], M, MPI_INT, MPI_SUM, MPI_COMM_WORLD, MPI_INFO_NULL, &pers[n++]);
}

static void persistent_run(int it)
{
    int i;
    MPI_Request more[6];
    int n = 0;

    fill_int(pb[0], M, pers_root + it);
    if (g_rank != pers_root)
        memset(pb[0], 0, M * sizeof(int));
    fill_int(pb[1], M, g_rank + it);
    for (i = 0; i < g_size; i++) {
        fill_int(pb[4] + i * M, M, i + it + 1);
        fill_int(pb[9] + i * M, M, g_rank * 100 + i + it);
    }
    fill_int(pb[13], M, g_rank);

    MPI_Startall(NPERS, pers);
    MPI_Waitall(NPERS, pers, MPI_STATUSES_IGNORE);

    check_int(pb[0], M, pers_root + it, "Bcast_init");
    if (g_rank == pers_root)
        for (i = 0; i < g_size; i++) {
            check_int(pb[2] + i * M, M, i + it, "Gather_init");
            check_int(pb[3] + i * M, M, i + it, "Gatherv_init");
        }
    check_int(pb[5], M, g_rank + it + 1, "Scatter_init");
    check_int(pb[6], M, g_rank + it + 1, "Scatterv_init");
    for (i = 0; i < g_size; i++) {
        check_int(pb[7] + i * M, M, i + it, "Allgather_init");
        check_int(pb[8] + i * M, M, i + it, "Allgatherv_init");
        check_int(pb[10] + i * M, M, i * 100 + g_rank + it, "Alltoall_init");
        check_int(pb[11] + i * M, M, i * 100 + g_rank + it, "Alltoallv_init");
        check_int(pb[12] + i * M, M, i * 100 + g_rank + it, "Alltoallw_init");
    }
    if (g_rank == pers_root)
        for (i = 0; i < M; i++)
            EXPECT(pb[14][i] == nb_sum + i * g_size, "Reduce_init wrong at %d", i);
    for (i = 0; i < M; i++)
        EXPECT(pb[15][i] == nb_sum + i * g_size, "Allreduce_init wrong at %d", i);

    /* the rest, created and freed per iteration, started one by one */
    for (i = 0; i < g_size * M; i++)
        pb[16][i] = g_rank;
    fill_int(pb[19], M, g_rank);
    MPI_Reduce_scatter_init(pb[16], pb[17], counts, MPI_INT, MPI_SUM, MPI_COMM_WORLD, MPI_INFO_NULL, &more[n++]);
    MPI_Reduce_scatter_block_init(pb[16], pb[18], M, MPI_INT, MPI_SUM, MPI_COMM_WORLD, MPI_INFO_NULL, &more[n++]);
    MPI_Scan_init(pb[19], pb[20], M, MPI_INT, MPI_SUM, MPI_COMM_WORLD, MPI_INFO_NULL, &more[n++]);
    MPI_Exscan_init(pb[19], pb[21], M, MPI_INT, MPI_SUM, MPI_COMM_WORLD, MPI_INFO_NULL, &more[n++]);
    fill_int(pb[22], M, g_rank + it);
    fill_int(pb[22] + M, M, g_rank + it + 7);
    MPI_Neighbor_allgather_init(pb[22], M, MPI_INT, pb[23], M, MPI_INT, cart, MPI_INFO_NULL, &more[n++]);
    MPI_Neighbor_alltoall_init(pb[22], M, MPI_INT, pb[24], M, MPI_INT, cart, MPI_INFO_NULL, &more[n++]);
    for (i = 0; i < n; i++) {
        MPI_Start(&more[i]);
        MPI_Wait(&more[i], MPI_STATUS_IGNORE);
    }
    for (i = 0; i < M; i++) {
        EXPECT(pb[17][i] == nb_sum, "Reduce_scatter_init wrong at %d", i);
        EXPECT(pb[18][i] == nb_sum, "Reduce_scatter_block_init wrong at %d", i);
        EXPECT(pb[20][i] == nb_prefix + i * (g_rank + 1), "Scan_init wrong at %d", i);
        if (g_rank > 0)
            EXPECT(pb[21][i] == nb_exprefix + i * g_rank, "Exscan_init wrong at %d", i);
    }
    check_int(pb[23], M, (g_rank + g_size - 1) % g_size + it, "Neighbor_allgather_init");
    check_int(pb[24], M, (g_rank + g_size - 1) % g_size + it + 7, "Neighbor_alltoall_init");
    for (i = 0; i < n; i++)
        MPI_Request_free(&more[i]);
}

static void persistent_free(void)
{
    int i;
    for (i = 0; i < NPERS; i++)
        MPI_Request_free(&pers[i]);
    for (i = 0; i < 2 * NPERS; i++)
        free(pb[i]);
}

static void large_count(int it)
{
    int root = it % g_size, i;
    MPI_Count cM = M;
    MPI_Count *ccounts = (MPI_Count *)malloc(g_size * sizeof(MPI_Count));
    MPI_Aint *cdispls = (MPI_Aint *)malloc(g_size * sizeof(MPI_Aint));
    MPI_Request reqs[3];

    for (i = 0; i < g_size; i++) {
        ccounts[i] = M;
        cdispls[i] = i * M;
    }

    fill_int(sbuf, M, root + it);
    if (g_rank != root)
        memset(sbuf, 0, M * sizeof(int));
    MPI_Bcast_c(sbuf, cM, MPI_INT, root, MPI_COMM_WORLD);
    check_int(sbuf, M, root + it, "Bcast_c");
    fill_int(sbuf, M, g_rank + it);
    MPI_Gather_c(sbuf, cM, MPI_INT, gbuf, cM, MPI_INT, root, MPI_COMM_WORLD);
    MPI_Gatherv_c(sbuf, cM, MPI_INT, rbuf, ccounts, cdispls, MPI_INT, root, MPI_COMM_WORLD);
    if (g_rank == root)
        for (i = 0; i < g_size; i++) {
            check_int(gbuf + i * M, M, i + it, "Gather_c");
            check_int(rbuf + i * M, M, i + it, "Gatherv_c");
        }
    MPI_Scatter_c(gbuf, cM, MPI_INT, rbuf, cM, MPI_INT, root, MPI_COMM_WORLD);
    check_int(rbuf, M, g_rank + it, "Scatter_c");
    MPI_Scatterv_c(gbuf, ccounts, cdispls, MPI_INT, rbuf, cM, MPI_INT, root, MPI_COMM_WORLD);
    check_int(rbuf, M, g_rank + it, "Scatterv_c");
    MPI_Allgather_c(sbuf, cM, MPI_INT, gbuf, cM, MPI_INT, MPI_COMM_WORLD);
    MPI_Allgatherv_c(sbuf, cM, MPI_INT, rbuf, ccounts, cdispls, MPI_INT, MPI_COMM_WORLD);
    for (i = 0; i < g_size; i++) {
        check_int(gbuf + i * M, M, i + it, "Allgather_c");
        check_int(rbuf + i * M, M, i + it, "Allgatherv_c");
    }
    for (i = 0; i < g_size; i++)
        fill_int(gbuf + i * M, M, g_rank * 100 + i + it);
    MPI_Alltoall_c(gbuf, cM, MPI_INT, rbuf, cM, MPI_INT, MPI_COMM_WORLD);
    for (i = 0; i < g_size; i++)
        check_int(rbuf + i * M, M, i * 100 + g_rank + it, "Alltoall_c");
    MPI_Alltoallv_c(gbuf, ccounts, cdispls, MPI_INT, rbuf, ccounts, cdispls, MPI_INT, MPI_COMM_WORLD);
    for (i = 0; i < g_size; i++)
        check_int(rbuf + i * M, M, i * 100 + g_rank + it, "Alltoallv_c");
    for (i = 0; i < g_size; i++)
        cdispls[i] = (MPI_Aint)(i * M * sizeof(int));
    MPI_Alltoallw_c(gbuf, ccounts, cdispls, types, rbuf, ccounts, cdispls, types, MPI_COMM_WORLD);
    for (i = 0; i < g_size; i++)
        check_int(rbuf + i * M, M, i * 100 + g_rank + it, "Alltoallw_c");

    fill_int(sbuf, M, g_rank);
    MPI_Reduce_c(sbuf, rbuf, cM, MPI_INT, MPI_SUM, root, MPI_COMM_WORLD);
    if (g_rank == root)
        for (i = 0; i < M; i++)
            EXPECT(rbuf[i] == nb_sum + i * g_size, "Reduce_c wrong at %d", i);
    MPI_Allreduce_c(sbuf, rbuf, cM, MPI_INT, MPI_SUM, MPI_COMM_WORLD);
    for (i = 0; i < M; i++)
        EXPECT(rbuf[i] == nb_sum + i * g_size, "Allreduce_c wrong at %d", i);
    MPI_Scan_c(sbuf, rbuf, cM, MPI_INT, MPI_SUM, MPI_COMM_WORLD);
    for (i = 0; i < M; i++)
        EXPECT(rbuf[i] == nb_prefix + i * (g_rank + 1), "Scan_c wrong at %d", i);
    MPI_Exscan_c(sbuf, rbuf, cM, MPI_INT, MPI_SUM, MPI_COMM_WORLD);
    for (i = 0; i < g_size * M; i++)
        gbuf[i] = g_rank;
    MPI_Reduce_scatter_c(gbuf, rbuf, ccounts, MPI_INT, MPI_SUM, MPI_COMM_WORLD);
    for (i = 0; i < M; i++)
        EXPECT(rbuf[i] == nb_sum, "Reduce_scatter_c wrong at %d", i);
    MPI_Reduce_scatter_block_c(gbuf, rbuf, cM, MPI_INT, MPI_SUM, MPI_COMM_WORLD);
    for (i = 0; i < M; i++)
        EXPECT(rbuf[i] == nb_sum, "Reduce_scatter_block_c wrong at %d", i);

    fill_int(sbuf, M, root + it);
    if (g_rank != root)
        memset(sbuf, 0, M * sizeof(int));
    MPI_Ibcast_c(sbuf, cM, MPI_INT, root, MPI_COMM_WORLD, &reqs[0]);
    fill_int(gbuf, M, g_rank);
    MPI_Iallreduce_c(gbuf, rbuf, cM, MPI_INT, MPI_SUM, MPI_COMM_WORLD, &reqs[1]);
    MPI_Ialltoall_c(gbuf, cM / g_size > 0 ? cM / g_size : 1, MPI_INT, gbuf + M, cM / g_size > 0 ? cM / g_size : 1,
                    MPI_INT, MPI_COMM_WORLD, &reqs[2]);
    MPI_Waitall(3, reqs, MPI_STATUSES_IGNORE);
    check_int(sbuf, M, root + it, "Ibcast_c");
    for (i = 0; i < M; i++)
        EXPECT(rbuf[i] == nb_sum + i * g_size, "Iallreduce_c wrong at %d", i);

    free(ccounts);
    free(cdispls);
}
#endif

int main(int argc, char **argv)
{
    int iterations, it, dims, periods = 1;

    test_init(&argc, &argv);
    iterations = parse_iterations(argc, argv, 10);

    sbuf = (int *)malloc(g_size * M * sizeof(int));
    rbuf = (int *)malloc(g_size * M * sizeof(int));
    gbuf = (int *)malloc(2 * g_size * M * sizeof(int));
    counts = (int *)malloc(g_size * sizeof(int));
    displs = (int *)malloc(g_size * sizeof(int));
    bdispls = (int *)malloc(g_size * sizeof(int));
    adispls = (MPI_Aint *)malloc(g_size * sizeof(MPI_Aint));
    types = (MPI_Datatype *)malloc(g_size * sizeof(MPI_Datatype));
    fill_vectors();
    nb_sum = g_size * (g_size - 1) / 2;
    nb_prefix = g_rank * (g_rank + 1) / 2;
    nb_exprefix = g_rank * (g_rank - 1) / 2;

    dims = g_size;
    MPI_Cart_create(MPI_COMM_WORLD, 1, &dims, &periods, 0, &cart);

#if HAVE_MPI4
    persistent_init();
#endif
    for (it = 0; it < iterations; it++) {
        blocking(it);
        nonblocking(it);
        neighborhood(it);
#if HAVE_MPI4
        persistent_run(it);
        large_count(it);
#endif
    }
#if HAVE_MPI4
    persistent_free();
#endif

    MPI_Comm_free(&cart);
    {
        int rc = test_report("coll_test", iterations);
        MPI_Finalize();
        return rc;
    }
    return 0;
}
