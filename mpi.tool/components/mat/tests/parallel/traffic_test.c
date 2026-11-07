/* Cross-rank traffic test for parallel mat (pmat, mat.omp).
   Parallel mat splits the trace ranks over its workers (rank % workers), so every message
   between ranks owned by different workers has to be matched across them. This test makes
   that the common case and mixes in everything that depends on matching order:
     - all-pairs nonblocking exchange (every channel used every iteration),
     - several messages with the same envelope on one channel (FIFO order must hold),
     - blocking pairwise exchange with varied sizes,
     - wildcard receives (MPI_ANY_SOURCE, MPI_ANY_TAG or both; blocking and nonblocking),
     - wildcard receives pending while later receives are posted (pairing order, cancel, persistent),
     - persistent send/receive requests restarted every iteration,
     - Probe + Recv, Waitany / Testsome completion,
     - collectives on MPI_COMM_WORLD and on split communicators, blocking and nonblocking,
     - deliberate compute imbalance so ranks drift apart in time.
   Self-checking; no MPI_Abort.
   Usage: traffic_test [iterations]      (2 or more ranks) */
#include "test_common.h"

#define FIFO_MSGS 4
#define MAXN 512

static int msg_len(int src, int dst, int it)
{
    return 1 + (src * 7 + dst * 13 + it * 5) % MAXN;
}

/* busy loop: ranks get unequal work, so their traces drift apart in time */
static volatile double sink;
static void imbalance(int it)
{
    long i, n = 20000L * (1 + (g_rank * 3 + it) % 5);
    double x = 0.0;
    for (i = 0; i < n; i++)
        x += (double)i * 1e-9;
    sink = x;
}

static void all_pairs(int it)
{
    int p, n = 0, sz = g_size;
    int *sbuf = malloc(sizeof(int) * MAXN * sz);
    int *rbuf = malloc(sizeof(int) * MAXN * sz);
    MPI_Request *req = malloc(sizeof(MPI_Request) * 2 * sz);
    for (p = 0; p < sz; p++) {
        if (p == g_rank)
            continue;
        MPI_Irecv(rbuf + p * MAXN, msg_len(p, g_rank, it), MPI_INT, p, 100 + it % 3, MPI_COMM_WORLD, &req[n++]);
    }
    for (p = 0; p < sz; p++) {
        if (p == g_rank)
            continue;
        fill_int(sbuf + p * MAXN, msg_len(g_rank, p, it), g_rank * 1000 + it);
        MPI_Isend(sbuf + p * MAXN, msg_len(g_rank, p, it), MPI_INT, p, 100 + it % 3, MPI_COMM_WORLD, &req[n++]);
    }
    MPI_Waitall(n, req, MPI_STATUSES_IGNORE);
    for (p = 0; p < sz; p++)
        if (p != g_rank)
            check_int(rbuf + p * MAXN, msg_len(p, g_rank, it), p * 1000 + it, "all_pairs");
    free(sbuf);
    free(rbuf);
    free(req);
}

/* several messages with one envelope (same source, tag, comm): matched strictly in order */
static void fifo_channel(int it)
{
    int right = (g_rank + 1) % g_size, left = (g_rank + g_size - 1) % g_size;
    int sbuf[FIFO_MSGS][MAXN], rbuf[FIFO_MSGS][MAXN], k, cnt;
    MPI_Request sreq[FIFO_MSGS];
    MPI_Status st;
    for (k = 0; k < FIFO_MSGS; k++) {
        fill_int(sbuf[k], 1 + k * 37, g_rank * 100 + k + it);
        MPI_Isend(sbuf[k], 1 + k * 37, MPI_INT, right, 7, MPI_COMM_WORLD, &sreq[k]);
    }
    for (k = 0; k < FIFO_MSGS; k++) {
        if (k % 2) {
            MPI_Recv(rbuf[k], MAXN, MPI_INT, left, 7, MPI_COMM_WORLD, &st);
        } else {
            MPI_Request r;
            MPI_Irecv(rbuf[k], MAXN, MPI_INT, left, 7, MPI_COMM_WORLD, &r);
            MPI_Wait(&r, &st);
        }
        MPI_Get_count(&st, MPI_INT, &cnt);
        EXPECT(cnt == 1 + k * 37, "fifo: message %d has %d ints", k, cnt);
        check_int(rbuf[k], 1 + k * 37, left * 100 + k + it, "fifo");
    }
    MPI_Waitall(FIFO_MSGS, sreq, MPI_STATUSES_IGNORE);
}

/* blocking exchange with a partner that changes every iteration */
static void pairwise_blocking(int it)
{
    int shift = 1 + it % (g_size - 1);
    int to = (g_rank + shift) % g_size, from = (g_rank + g_size - shift) % g_size;
    int sbuf[MAXN], rbuf[MAXN];
    fill_int(sbuf, msg_len(g_rank, to, it), g_rank + it);
    if (it % 2) {
        MPI_Sendrecv(sbuf, msg_len(g_rank, to, it), MPI_INT, to, 11, rbuf, MAXN, MPI_INT, from, 11, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
    } else {
        MPI_Request r;
        MPI_Irecv(rbuf, MAXN, MPI_INT, from, 12, MPI_COMM_WORLD, &r);
        MPI_Ssend(sbuf, msg_len(g_rank, to, it), MPI_INT, to, 12, MPI_COMM_WORLD);
        MPI_Wait(&r, MPI_STATUS_IGNORE);
    }
    check_int(rbuf, msg_len(from, g_rank, it), from + it, "pairwise");
}

/* gather point with wildcards: the root learns source and tag only from the status. Runs on its own communicator:
   an MPI_ANY_TAG receive on MPI_COMM_WORLD would take messages other ranks already sent for the later phases */
static void wildcards(int it, MPI_Comm wcomm)
{
    int root = it % g_size, buf[MAXN], k;
    if (g_rank == root) {
        int seen = 0;
        for (k = 0; k < g_size - 1; k++) {
            MPI_Status st;
            MPI_Request r;
            int cnt;
            switch ((k + it) % 4) { /* every receive form gets every position over the iterations */
            case 0:
                MPI_Recv(buf, MAXN, MPI_INT, MPI_ANY_SOURCE, MPI_ANY_TAG, wcomm, &st);
                break;
            case 1:
                MPI_Irecv(buf, MAXN, MPI_INT, MPI_ANY_SOURCE, 20 + it % 2, wcomm, &r);
                MPI_Wait(&r, &st);
                break;
            case 2:
                MPI_Irecv(buf, MAXN, MPI_INT, MPI_ANY_SOURCE, MPI_ANY_TAG, wcomm, &r);
                MPI_Wait(&r, &st);
                break;
            default:
                MPI_Recv(buf, MAXN, MPI_INT, MPI_ANY_SOURCE, 20 + it % 2, wcomm, &st);
                break;
            }
            MPI_Get_count(&st, MPI_INT, &cnt);
            EXPECT(st.MPI_TAG == 20 + it % 2, "wildcard tag %d", st.MPI_TAG);
            EXPECT(cnt == msg_len(st.MPI_SOURCE, root, it), "wildcard count %d from %d", cnt, st.MPI_SOURCE);
            check_int(buf, cnt, st.MPI_SOURCE + it, "wildcard");
            seen++;
        }
        EXPECT(seen == g_size - 1, "wildcard: %d messages", seen);
    } else {
        fill_int(buf, msg_len(g_rank, root, it), g_rank + it);
        MPI_Send(buf, msg_len(g_rank, root, it), MPI_INT, root, 20 + it % 2, wcomm);
    }
}

/* MPI_ANY_TAG from a known source: ring on its own communicator (the gather above takes any tag from any source on
   wcomm), tag changing every iteration, blocking and nonblocking receives alternating */
static void any_tag_ring(int it, MPI_Comm tcomm)
{
    int right = (g_rank + 1) % g_size, left = (g_rank + g_size - 1) % g_size;
    int sbuf[MAXN], rbuf[MAXN], cnt, k;
    MPI_Request sr[2], r;
    MPI_Status st;
    fill_int(sbuf, MAXN, g_rank + it);
    for (k = 0; k < 2; k++)
        MPI_Isend(sbuf, 24 + it + k, MPI_INT, right, 22 + (it + k) % 3, tcomm, &sr[k]);
    for (k = 0; k < 2; k++) {
        if ((it + k) % 2) {
            MPI_Recv(rbuf, MAXN, MPI_INT, left, MPI_ANY_TAG, tcomm, &st);
        } else {
            MPI_Irecv(rbuf, MAXN, MPI_INT, left, MPI_ANY_TAG, tcomm, &r);
            MPI_Wait(&r, &st);
        }
        MPI_Get_count(&st, MPI_INT, &cnt);
        EXPECT(st.MPI_TAG == 22 + (it + k) % 3, "any_tag: tag %d", st.MPI_TAG);
        EXPECT(cnt == 24 + it + k, "any_tag: count %d", cnt);
        check_int(rbuf, cnt, left + it, "any_tag");
    }
    MPI_Waitall(2, sr, MPI_STATUSES_IGNORE);
}

/* A wildcard receive pending while later receives are posted on the same communicator: it must get the first message
   it can take, and the later receives the ones after it. Rank 2i sends to 2i+1 on a communicator of their own; which
   side is first in time alternates every 6 iterations, so mat sees the sends both before and after the receives. */
#define WO_T1 70
#define WO_T2 71
#define WO_TOKEN 72
static void wo_fill(int *b, int it, int k)
{
    fill_int(b, 8, it * 100 + k);
}
static void wo_check(const int *b, int it, int k, const char *what)
{
    check_int(b, 8, it * 100 + k, what);
}
static void wildcard_order(int it, MPI_Comm ocomm)
{
    const int scenario = it % 6, sender = g_rank % 2 == 0, p = g_rank ^ 1;
    int b[3][8], k, idx, flag;
    MPI_Request r[3];
    MPI_Status st;
    if (p >= g_size)
        return;
    if (((it / 6) % 2 == 0) == sender)
        imbalance(it + 7); /* this side comes later in time */
    if (sender) {
        const int count = scenario == 3 ? 3 : (scenario == 4 ? 1 : 2);
        if (scenario == 4) /* the receiver cancels a wildcard first: send only after it says so */
            MPI_Recv(b[0], 1, MPI_INT, p, WO_TOKEN, ocomm, MPI_STATUS_IGNORE);
        for (k = 0; k < count; k++) {
            wo_fill(b[k], it, k);
            MPI_Send(b[k], 8, MPI_INT, p, scenario == 2 && k == 1 ? WO_T2 : WO_T1, ocomm);
        }
        return;
    }
    switch (scenario) {
    case 0: /* Irecv(ANY, ANY), Irecv(p, t1), waits in order */
        MPI_Irecv(b[0], 8, MPI_INT, MPI_ANY_SOURCE, MPI_ANY_TAG, ocomm, &r[0]);
        MPI_Irecv(b[1], 8, MPI_INT, p, WO_T1, ocomm, &r[1]);
        MPI_Wait(&r[0], &st);
        MPI_Wait(&r[1], MPI_STATUS_IGNORE);
        wo_check(b[0], it, 0, "wildcard_order 0 first");
        wo_check(b[1], it, 1, "wildcard_order 0 second");
        break;
    case 1: /* Irecv(ANY, ANY), then a blocking Recv(p, t1) while the wildcard is pending */
        MPI_Irecv(b[0], 8, MPI_INT, MPI_ANY_SOURCE, MPI_ANY_TAG, ocomm, &r[0]);
        MPI_Recv(b[1], 8, MPI_INT, p, WO_T1, ocomm, MPI_STATUS_IGNORE);
        MPI_Wait(&r[0], &st);
        wo_check(b[0], it, 0, "wildcard_order 1 first");
        wo_check(b[1], it, 1, "wildcard_order 1 second");
        break;
    case 2: /* Irecv(p, ANY_TAG) pending, then Recv(p, t2): the wildcard takes t1, the Recv t2 */
        MPI_Irecv(b[0], 8, MPI_INT, p, MPI_ANY_TAG, ocomm, &r[0]);
        MPI_Recv(b[1], 8, MPI_INT, p, WO_T2, ocomm, MPI_STATUS_IGNORE);
        MPI_Wait(&r[0], &st);
        EXPECT(st.MPI_TAG == WO_T1, "wildcard_order 2: tag %d", st.MPI_TAG);
        wo_check(b[0], it, 0, "wildcard_order 2 first");
        wo_check(b[1], it, 1, "wildcard_order 2 second");
        break;
    case 3: /* two different wildcards and a specific receive, completed in any order */
        MPI_Irecv(b[0], 8, MPI_INT, MPI_ANY_SOURCE, MPI_ANY_TAG, ocomm, &r[0]);
        MPI_Irecv(b[1], 8, MPI_INT, MPI_ANY_SOURCE, WO_T1, ocomm, &r[1]);
        MPI_Irecv(b[2], 8, MPI_INT, p, WO_T1, ocomm, &r[2]);
        for (k = 0; k < 3; k++)
            MPI_Waitany(3, r, &idx, MPI_STATUS_IGNORE);
        for (k = 0; k < 3; k++)
            wo_check(b[k], it, k, "wildcard_order 3");
        break;
    case 4: /* a cancelled wildcard must not hold up the receives after it */
        MPI_Irecv(b[0], 8, MPI_INT, MPI_ANY_SOURCE, MPI_ANY_TAG, ocomm, &r[0]);
        MPI_Cancel(&r[0]);
        MPI_Wait(&r[0], &st);
        MPI_Test_cancelled(&st, &flag);
        EXPECT(flag, "wildcard_order 4: wildcard receive not cancelled");
        MPI_Irecv(b[1], 8, MPI_INT, p, WO_T1, ocomm, &r[1]);
        MPI_Send(&flag, 1, MPI_INT, p, WO_TOKEN, ocomm);
        MPI_Wait(&r[1], MPI_STATUS_IGNORE);
        wo_check(b[1], it, 0, "wildcard_order 4");
        break;
    default: /* persistent wildcard receive, started ahead of a specific one */
        MPI_Recv_init(b[0], 8, MPI_INT, MPI_ANY_SOURCE, WO_T1, ocomm, &r[0]);
        MPI_Start(&r[0]);
        MPI_Irecv(b[1], 8, MPI_INT, p, WO_T1, ocomm, &r[1]);
        MPI_Waitall(2, r, MPI_STATUSES_IGNORE);
        MPI_Request_free(&r[0]);
        wo_check(b[0], it, 0, "wildcard_order 5 first");
        wo_check(b[1], it, 1, "wildcard_order 5 second");
        break;
    }
}

/* Probe the source first, then receive from it; sender 2 hops away */
static void probe_recv(int it)
{
    int to = (g_rank + 2) % g_size, from = (g_rank + g_size - 2) % g_size;
    int sbuf[MAXN], rbuf[MAXN], cnt;
    MPI_Request r;
    MPI_Status st;
    fill_int(sbuf, msg_len(g_rank, to, it), g_rank * 3 + it);
    MPI_Isend(sbuf, msg_len(g_rank, to, it), MPI_INT, to, 30, MPI_COMM_WORLD, &r);
    MPI_Probe(MPI_ANY_SOURCE, 30, MPI_COMM_WORLD, &st);
    MPI_Get_count(&st, MPI_INT, &cnt);
    MPI_Recv(rbuf, cnt, MPI_INT, st.MPI_SOURCE, 30, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
    EXPECT(st.MPI_SOURCE == from, "probe source %d", st.MPI_SOURCE);
    check_int(rbuf, cnt, from * 3 + it, "probe");
    MPI_Wait(&r, MPI_STATUS_IGNORE);
}

/* nonblocking ring both ways, completed with Waitany and Testsome */
static void any_some(int it)
{
    int right = (g_rank + 1) % g_size, left = (g_rank + g_size - 1) % g_size;
    int s[2][MAXN], r[2][MAXN], idx, done = 0, outc, outi[4];
    MPI_Request req[4];
    MPI_Irecv(r[0], MAXN, MPI_INT, left, 40, MPI_COMM_WORLD, &req[0]);
    MPI_Irecv(r[1], MAXN, MPI_INT, right, 41, MPI_COMM_WORLD, &req[1]);
    fill_int(s[0], 64, g_rank + it);
    fill_int(s[1], 96, g_rank + it + 1);
    MPI_Isend(s[0], 64, MPI_INT, right, 40, MPI_COMM_WORLD, &req[2]);
    MPI_Isend(s[1], 96, MPI_INT, left, 41, MPI_COMM_WORLD, &req[3]);
    if (it % 2) {
        while (done < 4) {
            MPI_Waitany(4, req, &idx, MPI_STATUS_IGNORE);
            done++;
        }
    } else {
        while (done < 4) {
            MPI_Testsome(4, req, &outc, outi, MPI_STATUSES_IGNORE);
            if (outc != MPI_UNDEFINED)
                done += outc;
        }
    }
    check_int(r[0], 64, left + it, "any_some left");
    check_int(r[1], 96, right + it + 1, "any_some right");
}

static void collectives(int it, MPI_Comm sub)
{
    int root = it % g_size, v = g_rank + it, sum = 0, *all, *a2a_s, *a2a_r, p, buf[8];
    int subrank, subsize, subsum = 0;
    MPI_Request r[2];
    all = malloc(sizeof(int) * g_size);
    a2a_s = malloc(sizeof(int) * g_size);
    a2a_r = malloc(sizeof(int) * g_size);

    MPI_Barrier(MPI_COMM_WORLD);
    fill_int(buf, 8, g_rank == root ? root * 10 + it : -1);
    MPI_Bcast(buf, 8, MPI_INT, root, MPI_COMM_WORLD);
    check_int(buf, 8, root * 10 + it, "bcast");
    MPI_Allreduce(&v, &sum, 1, MPI_INT, MPI_SUM, MPI_COMM_WORLD);
    EXPECT(sum == g_size * (g_size - 1) / 2 + g_size * it, "allreduce %d", sum);
    MPI_Reduce(&v, &sum, 1, MPI_INT, MPI_MAX, root, MPI_COMM_WORLD);
    if (g_rank == root)
        EXPECT(sum == g_size - 1 + it, "reduce %d", sum);
    MPI_Allgather(&v, 1, MPI_INT, all, 1, MPI_INT, MPI_COMM_WORLD);
    for (p = 0; p < g_size; p++)
        EXPECT(all[p] == p + it, "allgather[%d] %d", p, all[p]);
    for (p = 0; p < g_size; p++)
        a2a_s[p] = g_rank * 100 + p;
    MPI_Alltoall(a2a_s, 1, MPI_INT, a2a_r, 1, MPI_INT, MPI_COMM_WORLD);
    for (p = 0; p < g_size; p++)
        EXPECT(a2a_r[p] == p * 100 + g_rank, "alltoall[%d] %d", p, a2a_r[p]);

    /* nonblocking collectives overlapping each other */
    MPI_Iallreduce(&v, &sum, 1, MPI_INT, MPI_SUM, MPI_COMM_WORLD, &r[0]);
    MPI_Ibcast(buf, 8, MPI_INT, (root + 1) % g_size, MPI_COMM_WORLD, &r[1]);
    MPI_Waitall(2, r, MPI_STATUSES_IGNORE);
    EXPECT(sum == g_size * (g_size - 1) / 2 + g_size * it, "iallreduce %d", sum);

    /* the same operations on a split communicator, run by half the ranks at a time */
    MPI_Comm_rank(sub, &subrank);
    MPI_Comm_size(sub, &subsize);
    MPI_Allreduce(&subrank, &subsum, 1, MPI_INT, MPI_SUM, sub);
    EXPECT(subsum == subsize * (subsize - 1) / 2, "sub allreduce %d", subsum);
    fill_int(buf, 8, subrank == 0 ? 77 + it : -1);
    MPI_Bcast(buf, 8, MPI_INT, 0, sub);
    check_int(buf, 8, 77 + it, "sub bcast");
    MPI_Ibarrier(sub, &r[0]);
    MPI_Wait(&r[0], MPI_STATUS_IGNORE);
    free(all);
    free(a2a_s);
    free(a2a_r);
}

/* p2p inside the split communicator (comm-relative ranks) */
static void sub_p2p(int it, MPI_Comm sub)
{
    int subrank, subsize, sbuf[MAXN], rbuf[MAXN];
    MPI_Comm_rank(sub, &subrank);
    MPI_Comm_size(sub, &subsize);
    if (subsize < 2)
        return;
    int to = (subrank + 1) % subsize, from = (subrank + subsize - 1) % subsize;
    fill_int(sbuf, 32, subrank + it);
    MPI_Sendrecv(sbuf, 32, MPI_INT, to, 50, rbuf, 32, MPI_INT, from, 50, sub, MPI_STATUS_IGNORE);
    check_int(rbuf, 32, from + it, "sub p2p");
}

int main(int argc, char **argv)
{
    int it, iterations;
    MPI_Comm sub, wcomm, tcomm, ocomm;
    int right, left, ps[MAXN], pr[MAXN];
    MPI_Request preq[2];

    test_init(&argc, &argv);
    iterations = parse_iterations(argc, argv, 10);
    if (g_size < 2) {
        if (g_rank == 0)
            fprintf(stderr, "traffic_test needs 2 or more ranks\n");
        MPI_Finalize();
        return 1;
    }
    right = (g_rank + 1) % g_size;
    left = (g_rank + g_size - 1) % g_size;
    MPI_Comm_split(MPI_COMM_WORLD, g_rank % 2, g_rank, &sub);
    MPI_Comm_dup(MPI_COMM_WORLD, &wcomm);
    MPI_Comm_dup(MPI_COMM_WORLD, &tcomm);
    MPI_Comm_dup(MPI_COMM_WORLD, &ocomm);
    MPI_Send_init(ps, 48, MPI_INT, left, 60, MPI_COMM_WORLD, &preq[0]);
    MPI_Recv_init(pr, 48, MPI_INT, right, 60, MPI_COMM_WORLD, &preq[1]);

    for (it = 0; it < iterations; it++) {
        imbalance(it);
        all_pairs(it);
        fifo_channel(it);
        pairwise_blocking(it);
        wildcards(it, wcomm);
        any_tag_ring(it, tcomm);
        wildcard_order(it, ocomm);
        probe_recv(it);
        any_some(it);

        fill_int(ps, 48, g_rank * 5 + it);
        MPI_Startall(2, preq);
        MPI_Waitall(2, preq, MPI_STATUSES_IGNORE);
        check_int(pr, 48, right * 5 + it, "persistent");

        collectives(it, sub);
        sub_p2p(it, sub);
    }
    MPI_Request_free(&preq[0]);
    MPI_Request_free(&preq[1]);
    MPI_Comm_free(&sub);
    MPI_Comm_free(&wcomm);
    MPI_Comm_free(&tcomm);
    MPI_Comm_free(&ocomm);
    it = test_report("traffic_test", iterations);
    MPI_Finalize();
    return it;
}
