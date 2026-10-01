/* Point-to-point class test.
   Ring pattern (send to right, receive from left) covering blocking send modes,
   nonblocking sends/receives, all completion calls, probe/matched probe,
   persistent requests, cancel and (MPI-4) large-count, Isendrecv and
   partitioned communication.
   Usage: p2p_test [iterations]      (2 or more ranks) */
#include "test_common.h"

#define N 1024
#define NREQ 4

static int right, left;

static void blocking_modes(int it)
{
    int sbuf[N], rbuf[N];
    MPI_Status st;
    MPI_Request rreq;
    int cnt;

    /* Send / Recv, even ranks send first to keep the ring free of deadlock */
    fill_int(sbuf, N, g_rank + it);
    if (g_rank % 2 == 0) {
        MPI_Send(sbuf, N, MPI_INT, right, 1, MPI_COMM_WORLD);
        MPI_Recv(rbuf, N, MPI_INT, left, 1, MPI_COMM_WORLD, &st);
    } else {
        MPI_Recv(rbuf, N, MPI_INT, left, 1, MPI_COMM_WORLD, &st);
        MPI_Send(sbuf, N, MPI_INT, right, 1, MPI_COMM_WORLD);
    }
    MPI_Get_count(&st, MPI_INT, &cnt);
    EXPECT(cnt == N && st.MPI_SOURCE == left && st.MPI_TAG == 1, "Recv status wrong");
    check_int(rbuf, N, left + it, "Send/Recv");

    /* Ssend */
    if (g_rank % 2 == 0) {
        MPI_Ssend(sbuf, N, MPI_INT, right, 2, MPI_COMM_WORLD);
        MPI_Recv(rbuf, N, MPI_INT, left, 2, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
    } else {
        MPI_Recv(rbuf, N, MPI_INT, left, 2, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        MPI_Ssend(sbuf, N, MPI_INT, right, 2, MPI_COMM_WORLD);
    }
    check_int(rbuf, N, left + it, "Ssend");

    /* Bsend (buffer attached in main) */
    MPI_Bsend(sbuf, N, MPI_INT, right, 3, MPI_COMM_WORLD);
    MPI_Recv(rbuf, N, MPI_INT, left, 3, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
    check_int(rbuf, N, left + it, "Bsend");

    /* Rsend: the receive must be posted before the send starts */
    MPI_Irecv(rbuf, N, MPI_INT, left, 4, MPI_COMM_WORLD, &rreq);
    MPI_Barrier(MPI_COMM_WORLD);
    MPI_Rsend(sbuf, N, MPI_INT, right, 4, MPI_COMM_WORLD);
    MPI_Wait(&rreq, &st);
    check_int(rbuf, N, left + it, "Rsend");

    /* Sendrecv and Sendrecv_replace */
    MPI_Sendrecv(sbuf, N, MPI_INT, right, 5, rbuf, N, MPI_INT, left, 5, MPI_COMM_WORLD, &st);
    check_int(rbuf, N, left + it, "Sendrecv");
    memcpy(rbuf, sbuf, sizeof(rbuf));
    MPI_Sendrecv_replace(rbuf, N, MPI_INT, right, 6, left, 6, MPI_COMM_WORLD, &st);
    check_int(rbuf, N, left + it, "Sendrecv_replace");
}

static void nonblocking_modes(int it)
{
    int sbuf[NREQ][N], rbuf[NREQ][N];
    MPI_Request reqs[2 * NREQ];
    MPI_Status stats[2 * NREQ];
    int indices[2 * NREQ];
    int i, flag, done, idx, outcount;

    for (i = 0; i < NREQ; i++)
        fill_int(sbuf[i], N, g_rank + it + i);

    /* Isend / Irecv + Waitall */
    for (i = 0; i < NREQ; i++) {
        MPI_Irecv(rbuf[i], N, MPI_INT, left, 10 + i, MPI_COMM_WORLD, &reqs[i]);
        MPI_Isend(sbuf[i], N, MPI_INT, right, 10 + i, MPI_COMM_WORLD, &reqs[NREQ + i]);
    }
    MPI_Waitall(2 * NREQ, reqs, stats);
    for (i = 0; i < NREQ; i++)
        check_int(rbuf[i], N, left + it + i, "Isend/Irecv");

    /* Issend + Test loop, Irecv + Request_get_status */
    MPI_Irecv(rbuf[0], N, MPI_INT, left, 20, MPI_COMM_WORLD, &reqs[0]);
    MPI_Issend(sbuf[0], N, MPI_INT, right, 20, MPI_COMM_WORLD, &reqs[1]);
    do {
        MPI_Test(&reqs[1], &flag, MPI_STATUS_IGNORE);
    } while (!flag);
    do {
        MPI_Request_get_status(reqs[0], &flag, &stats[0]);
    } while (!flag);
    EXPECT(stats[0].MPI_TAG == 20, "Request_get_status tag wrong");
    MPI_Wait(&reqs[0], MPI_STATUS_IGNORE);
    check_int(rbuf[0], N, left + it, "Issend");

    /* Ibsend + Waitany */
    MPI_Irecv(rbuf[0], N, MPI_INT, left, 21, MPI_COMM_WORLD, &reqs[0]);
    MPI_Ibsend(sbuf[0], N, MPI_INT, right, 21, MPI_COMM_WORLD, &reqs[1]);
    for (done = 0; done < 2; done++) {
        MPI_Waitany(2, reqs, &idx, &stats[0]);
        EXPECT(idx == 0 || idx == 1, "Waitany index %d", idx);
    }
    check_int(rbuf[0], N, left + it, "Ibsend");

    /* Irsend + Testall loop */
    MPI_Irecv(rbuf[0], N, MPI_INT, left, 22, MPI_COMM_WORLD, &reqs[0]);
    MPI_Barrier(MPI_COMM_WORLD);
    MPI_Irsend(sbuf[0], N, MPI_INT, right, 22, MPI_COMM_WORLD, &reqs[1]);
    do {
        MPI_Testall(2, reqs, &flag, stats);
    } while (!flag);
    check_int(rbuf[0], N, left + it, "Irsend");

    /* Waitsome */
    for (i = 0; i < NREQ; i++) {
        MPI_Irecv(rbuf[i], N, MPI_INT, left, 30 + i, MPI_COMM_WORLD, &reqs[i]);
        MPI_Isend(sbuf[i], N, MPI_INT, right, 30 + i, MPI_COMM_WORLD, &reqs[NREQ + i]);
    }
    done = 0;
    while (done < 2 * NREQ) {
        MPI_Waitsome(2 * NREQ, reqs, &outcount, indices, stats);
        EXPECT(outcount != MPI_UNDEFINED && outcount > 0, "Waitsome outcount %d", outcount);
        done += outcount;
    }
    for (i = 0; i < NREQ; i++)
        check_int(rbuf[i], N, left + it + i, "Waitsome");

    /* Testsome */
    for (i = 0; i < NREQ; i++) {
        MPI_Irecv(rbuf[i], N, MPI_INT, left, 40 + i, MPI_COMM_WORLD, &reqs[i]);
        MPI_Isend(sbuf[i], N, MPI_INT, right, 40 + i, MPI_COMM_WORLD, &reqs[NREQ + i]);
    }
    done = 0;
    while (done < 2 * NREQ) {
        MPI_Testsome(2 * NREQ, reqs, &outcount, indices, stats);
        if (outcount != MPI_UNDEFINED)
            done += outcount;
    }
    for (i = 0; i < NREQ; i++)
        check_int(rbuf[i], N, left + it + i, "Testsome");

    /* Testany */
    MPI_Irecv(rbuf[0], N, MPI_INT, left, 50, MPI_COMM_WORLD, &reqs[0]);
    MPI_Isend(sbuf[0], N, MPI_INT, right, 50, MPI_COMM_WORLD, &reqs[1]);
    for (done = 0; done < 2;) {
        MPI_Testany(2, reqs, &idx, &flag, MPI_STATUS_IGNORE);
        if (flag)
            done++;
    }
    check_int(rbuf[0], N, left + it, "Testany");
}

static void probes(int it)
{
    int sbuf[N], *rbuf;
    MPI_Status st;
    MPI_Message msg;
    MPI_Request req;
    int cnt, flag;

    fill_int(sbuf, N, g_rank + it);

    /* Probe + Recv with wildcard source */
    MPI_Isend(sbuf, N, MPI_INT, right, 60, MPI_COMM_WORLD, &req);
    MPI_Probe(MPI_ANY_SOURCE, 60, MPI_COMM_WORLD, &st);
    MPI_Get_count(&st, MPI_INT, &cnt);
    EXPECT(cnt == N && st.MPI_SOURCE == left, "Probe status wrong");
    rbuf = (int *)malloc(cnt * sizeof(int));
    MPI_Recv(rbuf, cnt, MPI_INT, st.MPI_SOURCE, 60, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
    check_int(rbuf, N, left + it, "Probe/Recv");
    free(rbuf);
    MPI_Wait(&req, MPI_STATUS_IGNORE);

    /* Iprobe loop */
    MPI_Isend(sbuf, N, MPI_INT, right, 61, MPI_COMM_WORLD, &req);
    do {
        MPI_Iprobe(left, MPI_ANY_TAG, MPI_COMM_WORLD, &flag, &st);
    } while (!flag);
    MPI_Get_count(&st, MPI_INT, &cnt);
    EXPECT(cnt == N && st.MPI_TAG == 61, "Iprobe status wrong");
    rbuf = (int *)malloc(cnt * sizeof(int));
    MPI_Recv(rbuf, cnt, MPI_INT, left, st.MPI_TAG, MPI_COMM_WORLD, &st);
    check_int(rbuf, N, left + it, "Iprobe/Recv");
    free(rbuf);
    MPI_Wait(&req, MPI_STATUS_IGNORE);

    /* Mprobe + Mrecv */
    MPI_Isend(sbuf, N, MPI_INT, right, 62, MPI_COMM_WORLD, &req);
    MPI_Mprobe(left, 62, MPI_COMM_WORLD, &msg, &st);
    MPI_Get_count(&st, MPI_INT, &cnt);
    rbuf = (int *)malloc(cnt * sizeof(int));
    MPI_Mrecv(rbuf, cnt, MPI_INT, &msg, &st);
    check_int(rbuf, N, left + it, "Mprobe/Mrecv");
    free(rbuf);
    MPI_Wait(&req, MPI_STATUS_IGNORE);

    /* Improbe + Imrecv */
    MPI_Isend(sbuf, N, MPI_INT, right, 63, MPI_COMM_WORLD, &req);
    do {
        MPI_Improbe(left, 63, MPI_COMM_WORLD, &flag, &msg, &st);
    } while (!flag);
    MPI_Get_count(&st, MPI_INT, &cnt);
    rbuf = (int *)malloc(cnt * sizeof(int));
    {
        MPI_Request rreq;
        MPI_Imrecv(rbuf, cnt, MPI_INT, &msg, &rreq);
        MPI_Wait(&rreq, &st);
    }
    check_int(rbuf, N, left + it, "Improbe/Imrecv");
    free(rbuf);
    MPI_Wait(&req, MPI_STATUS_IGNORE);
}

static void persistent(int it)
{
    int sbuf[4][N], rbuf[4][N];
    MPI_Request reqs[8];
    MPI_Status stats[8];
    int i, k;

    for (i = 0; i < 4; i++)
        fill_int(sbuf[i], N, g_rank + it + i);

    /* one receive per send mode: Send_init, Bsend_init, Ssend_init, Rsend_init */
    for (i = 0; i < 4; i++)
        MPI_Recv_init(rbuf[i], N, MPI_INT, left, 70 + i, MPI_COMM_WORLD, &reqs[i]);
    MPI_Send_init(sbuf[0], N, MPI_INT, right, 70, MPI_COMM_WORLD, &reqs[4]);
    MPI_Bsend_init(sbuf[1], N, MPI_INT, right, 71, MPI_COMM_WORLD, &reqs[5]);
    MPI_Ssend_init(sbuf[2], N, MPI_INT, right, 72, MPI_COMM_WORLD, &reqs[6]);
    MPI_Rsend_init(sbuf[3], N, MPI_INT, right, 73, MPI_COMM_WORLD, &reqs[7]);

    for (k = 0; k < 3; k++) {
        /* receives first (Rsend needs them posted), then the sends */
        MPI_Startall(4, reqs);
        MPI_Barrier(MPI_COMM_WORLD);
        MPI_Start(&reqs[4]);
        MPI_Start(&reqs[5]);
        MPI_Startall(2, &reqs[6]);
        MPI_Waitall(8, reqs, stats);
        for (i = 0; i < 4; i++)
            check_int(rbuf[i], N, left + it + i, "persistent");
    }
    for (i = 0; i < 8; i++)
        MPI_Request_free(&reqs[i]);
}

static void cancel_and_free(int it)
{
    int rbuf[N], sbuf[N];
    MPI_Request req;
    MPI_Status st;
    int flag;

    /* cancel a receive nobody will ever match */
    MPI_Irecv(rbuf, N, MPI_INT, left, 999, MPI_COMM_WORLD, &req);
    MPI_Cancel(&req);
    MPI_Wait(&req, &st);
    MPI_Test_cancelled(&st, &flag);
    EXPECT(flag, "cancelled receive not reported as cancelled");

    /* Request_free on an active send, completion detected via the receive */
    fill_int(sbuf, N, g_rank + it);
    MPI_Isend(sbuf, N, MPI_INT, right, 80, MPI_COMM_WORLD, &req);
    MPI_Request_free(&req);
    MPI_Recv(rbuf, N, MPI_INT, left, 80, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
    check_int(rbuf, N, left + it, "Request_free");
    MPI_Barrier(MPI_COMM_WORLD); /* send buffer must stay alive until matched */
}

#if HAVE_MPI4
static void mpi4_features(int it)
{
    int sbuf[N], rbuf[N];
    MPI_Request reqs[2];
    MPI_Status st;
    MPI_Count cnt;
    int i, flag;
    const int parts = 4;

    fill_int(sbuf, N, g_rank + it);

    /* Isendrecv / Isendrecv_replace */
    MPI_Isendrecv(sbuf, N, MPI_INT, right, 90, rbuf, N, MPI_INT, left, 90, MPI_COMM_WORLD, &reqs[0]);
    MPI_Wait(&reqs[0], &st);
    check_int(rbuf, N, left + it, "Isendrecv");
    memcpy(rbuf, sbuf, sizeof(rbuf));
    MPI_Isendrecv_replace(rbuf, N, MPI_INT, right, 91, left, 91, MPI_COMM_WORLD, &reqs[0]);
    MPI_Wait(&reqs[0], MPI_STATUS_IGNORE);
    check_int(rbuf, N, left + it, "Isendrecv_replace");

    /* large-count variants */
    MPI_Irecv_c(rbuf, (MPI_Count)N, MPI_INT, left, 92, MPI_COMM_WORLD, &reqs[0]);
    MPI_Send_c(sbuf, (MPI_Count)N, MPI_INT, right, 92, MPI_COMM_WORLD);
    MPI_Wait(&reqs[0], &st);
    MPI_Get_count_c(&st, MPI_INT, &cnt);
    EXPECT(cnt == N, "Get_count_c wrong");
    check_int(rbuf, N, left + it, "Send_c/Irecv_c");

    MPI_Isend_c(sbuf, (MPI_Count)N, MPI_INT, right, 93, MPI_COMM_WORLD, &reqs[0]);
    MPI_Recv_c(rbuf, (MPI_Count)N, MPI_INT, left, 93, MPI_COMM_WORLD, &st);
    MPI_Wait(&reqs[0], MPI_STATUS_IGNORE);
    check_int(rbuf, N, left + it, "Isend_c/Recv_c");

    MPI_Sendrecv_c(sbuf, (MPI_Count)N, MPI_INT, right, 94, rbuf, (MPI_Count)N, MPI_INT, left, 94,
                   MPI_COMM_WORLD, &st);
    check_int(rbuf, N, left + it, "Sendrecv_c");

    MPI_Recv_init_c(rbuf, (MPI_Count)N, MPI_INT, left, 95, MPI_COMM_WORLD, &reqs[0]);
    MPI_Send_init_c(sbuf, (MPI_Count)N, MPI_INT, right, 95, MPI_COMM_WORLD, &reqs[1]);
    MPI_Startall(2, reqs);
    MPI_Waitall(2, reqs, MPI_STATUSES_IGNORE);
    check_int(rbuf, N, left + it, "Send_init_c/Recv_init_c");
    MPI_Request_free(&reqs[0]);
    MPI_Request_free(&reqs[1]);

    /* partitioned communication */
    MPI_Precv_init(rbuf, parts, N / parts, MPI_INT, left, 96, MPI_COMM_WORLD, MPI_INFO_NULL, &reqs[0]);
    MPI_Psend_init(sbuf, parts, N / parts, MPI_INT, right, 96, MPI_COMM_WORLD, MPI_INFO_NULL, &reqs[1]);
    MPI_Startall(2, reqs);
    MPI_Pready(0, reqs[1]);
    MPI_Pready_range(1, 2, reqs[1]);
    {
        int list[1] = { 3 };
        MPI_Pready_list(1, list, reqs[1]);
    }
    for (i = 0; i < parts; i++) {
        do {
            MPI_Parrived(reqs[0], i, &flag);
        } while (!flag);
    }
    MPI_Waitall(2, reqs, MPI_STATUSES_IGNORE);
    check_int(rbuf, N, left + it, "partitioned");
    MPI_Request_free(&reqs[0]);
    MPI_Request_free(&reqs[1]);
}
#endif

int main(int argc, char **argv)
{
    int iterations, it, bsize;
    void *bsend_buf, *detached;

    test_init(&argc, &argv);
    iterations = parse_iterations(argc, argv, 10);
    if (g_size < 2) {
        if (g_rank == 0)
            fprintf(stderr, "p2p_test needs at least 2 ranks\n");
        MPI_Finalize();
        return 1;
    }
    right = (g_rank + 1) % g_size;
    left = (g_rank + g_size - 1) % g_size;

    MPI_Pack_size(N, MPI_INT, MPI_COMM_WORLD, &bsize);
    bsize = 8 * (bsize + MPI_BSEND_OVERHEAD);
    bsend_buf = malloc(bsize);
    MPI_Buffer_attach(bsend_buf, bsize);

    for (it = 0; it < iterations; it++) {
        blocking_modes(it);
        nonblocking_modes(it);
        probes(it);
        persistent(it);
        cancel_and_free(it);
#if HAVE_MPI4
        mpi4_features(it);
#endif
    }

    MPI_Buffer_detach(&detached, &bsize);
    free(bsend_buf);

    {
        int rc = test_report("p2p_test", iterations);
        MPI_Finalize();
        return rc;
    }
}
