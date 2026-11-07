/* Point-to-point class test.
   Ring pattern (send to right, receive from left) covering blocking send modes,
   nonblocking sends/receives, all completion calls, probe/matched probe,
   persistent requests, cancel, wildcard receives (MPI_ANY_SOURCE/MPI_ANY_TAG with every
   sending and receiving call) and (MPI-4) large-count, Isendrecv and partitioned communication.
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

/* Wildcard receives (MPI_ANY_SOURCE / MPI_ANY_TAG) with every sending and receiving call. Every rank sends the
   same WC_MSGS messages (same tag; message k carries k in its data and has its own length) to right and posts WC_MSGS
   consecutive receives from left. Receive kinds: Recv, Irecv, Recv_init, Sendrecv(_replace), Mprobe/Mrecv,
   Improbe/Imrecv and, MPI-4, Isendrecv(_replace) and all large-count _c variants; send kinds: Send, Isend, Ssend,
   Issend, Bsend, Ibsend, Send_init, Ssend_init, Bsend_init (+ _c). When the receive is a send-receive call its send
   half carries the message. Kinds depend on the rank, so left's sending call differs from this rank's receiving one
   (Send against Sendrecv, Isendrecv against Recv, ...); across the sequences of an iteration every kind comes at every
   position, each receive with an envelope from (ANY,ANY), (left,ANY), (ANY,T), (left,T), and nonblocking ones are
   completed in different orders. MPI matches one sender's messages in sending order to receives in posting order, so
   receive k must get message k. Even ranks send before receiving, odd ones after, so synchronous sends can't deadlock
   the ring. Rsend kinds are left out: they need the receive posted first (persistent() covers them).
   Then two tags with a specific receive ahead of the wildcards, and (3+ ranks) a fan-in taken with MPI_ANY_SOURCE.
   Runs on its own communicators: an MPI_ANY_TAG receive on MPI_COMM_WORLD could take the other phases' messages, and
   the root's wildcard receives on wcomm the fan-in messages of ranks already past phases 1 and 2. */
#define WC_MSGS 4
#define WC_CAP (8 * WC_MSGS)
#define WC_TAG 90
#define WC_TAG2 91
static MPI_Comm wcomm = MPI_COMM_NULL;
static MPI_Comm fcomm = MPI_COMM_NULL; /* the fan-in: its senders run ahead into it while the root still receives on wcomm */

enum wc_kind {
    WK_RECV, WK_IRECV, WK_RECV_INIT, WK_SENDRECV, WK_SENDRECV_REPLACE, WK_MPROBE, WK_IMPROBE,
#if HAVE_MPI4
    WK_ISENDRECV, WK_ISENDRECV_REPLACE,
    WK_RECV_C, WK_IRECV_C, WK_RECV_INIT_C, WK_SENDRECV_C, WK_SENDRECV_REPLACE_C, WK_MPROBE_C, WK_IMPROBE_C,
    WK_ISENDRECV_C, WK_ISENDRECV_REPLACE_C,
#endif
    WK_COUNT
};
static const char *wc_kind_name[WK_COUNT] = {
    "Recv", "Irecv", "Recv_init", "Sendrecv", "Sendrecv_replace", "Mprobe/Mrecv", "Improbe/Imrecv",
#if HAVE_MPI4
    "Isendrecv", "Isendrecv_replace",
    "Recv_c", "Irecv_c", "Recv_init_c", "Sendrecv_c", "Sendrecv_replace_c", "Mprobe/Mrecv_c", "Improbe/Imrecv_c",
    "Isendrecv_c", "Isendrecv_replace_c",
#endif
};
enum wc_send_kind {
    SK_SEND, SK_ISEND, SK_SSEND, SK_ISSEND, SK_BSEND, SK_IBSEND, SK_SEND_INIT, SK_SSEND_INIT, SK_BSEND_INIT,
#if HAVE_MPI4
    SK_SEND_C, SK_ISEND_C, SK_SSEND_C, SK_ISSEND_C, SK_BSEND_C, SK_IBSEND_C, SK_SEND_INIT_C, SK_SSEND_INIT_C,
    SK_BSEND_INIT_C,
#endif
    SK_COUNT
};

static int wc_sends(int kind) /* the receiving call's send half carries the message */
{
    switch (kind) {
    case WK_SENDRECV: case WK_SENDRECV_REPLACE:
#if HAVE_MPI4
    case WK_ISENDRECV: case WK_ISENDRECV_REPLACE: case WK_SENDRECV_C: case WK_SENDRECV_REPLACE_C:
    case WK_ISENDRECV_C: case WK_ISENDRECV_REPLACE_C:
#endif
        return 1;
    }
    return 0;
}
static int wc_len(int k)
{
    return 8 * (k + 1);
}
static int wc_base(int src, int it, int k)
{
    return src * 1000 + it * 10 + k;
}

/* Send len ints with one send kind. req stays MPI_REQUEST_NULL for blocking kinds; *persistent is set for the _init
   kinds (started here; the request must be freed after completing). */
static void wc_send(int kind, const int *sbuf, int len, int dest, int tag, MPI_Comm comm, MPI_Request *req, int *persistent)
{
    *req = MPI_REQUEST_NULL;
    *persistent = 0;
    switch (kind) {
    case SK_SEND: MPI_Send(sbuf, len, MPI_INT, dest, tag, comm); break;
    case SK_ISEND: MPI_Isend(sbuf, len, MPI_INT, dest, tag, comm, req); break;
    case SK_SSEND: MPI_Ssend(sbuf, len, MPI_INT, dest, tag, comm); break;
    case SK_ISSEND: MPI_Issend(sbuf, len, MPI_INT, dest, tag, comm, req); break;
    case SK_BSEND: MPI_Bsend(sbuf, len, MPI_INT, dest, tag, comm); break;
    case SK_IBSEND: MPI_Ibsend(sbuf, len, MPI_INT, dest, tag, comm, req); break;
    case SK_SEND_INIT: MPI_Send_init(sbuf, len, MPI_INT, dest, tag, comm, req); *persistent = 1; break;
    case SK_SSEND_INIT: MPI_Ssend_init(sbuf, len, MPI_INT, dest, tag, comm, req); *persistent = 1; break;
    case SK_BSEND_INIT: MPI_Bsend_init(sbuf, len, MPI_INT, dest, tag, comm, req); *persistent = 1; break;
#if HAVE_MPI4
    case SK_SEND_C: MPI_Send_c(sbuf, len, MPI_INT, dest, tag, comm); break;
    case SK_ISEND_C: MPI_Isend_c(sbuf, len, MPI_INT, dest, tag, comm, req); break;
    case SK_SSEND_C: MPI_Ssend_c(sbuf, len, MPI_INT, dest, tag, comm); break;
    case SK_ISSEND_C: MPI_Issend_c(sbuf, len, MPI_INT, dest, tag, comm, req); break;
    case SK_BSEND_C: MPI_Bsend_c(sbuf, len, MPI_INT, dest, tag, comm); break;
    case SK_IBSEND_C: MPI_Ibsend_c(sbuf, len, MPI_INT, dest, tag, comm, req); break;
    case SK_SEND_INIT_C: MPI_Send_init_c(sbuf, len, MPI_INT, dest, tag, comm, req); *persistent = 1; break;
    case SK_SSEND_INIT_C: MPI_Ssend_init_c(sbuf, len, MPI_INT, dest, tag, comm, req); *persistent = 1; break;
    case SK_BSEND_INIT_C: MPI_Bsend_init_c(sbuf, len, MPI_INT, dest, tag, comm, req); *persistent = 1; break;
#endif
    }
    if (*persistent)
        MPI_Start(req);
}

/* Post (blocking kinds: complete) one receive of the given kind from src/tag into rbuf. If the kind sends, its send
   half sends slen ints of sbuf to dest with tag stag. req stays MPI_REQUEST_NULL for blocking kinds (st is filled),
   *persistent is set for the Recv_init kinds. */
static void wc_post(int kind, int src, int tag, int *rbuf, const int *sbuf, int slen, int dest, int stag,
                    MPI_Comm comm, MPI_Request *req, int *persistent, MPI_Status *st)
{
    MPI_Message msg;
    int cnt, flag;
    *req = MPI_REQUEST_NULL;
    *persistent = 0;
    switch (kind) {
    case WK_RECV: MPI_Recv(rbuf, WC_CAP, MPI_INT, src, tag, comm, st); break;
    case WK_IRECV: MPI_Irecv(rbuf, WC_CAP, MPI_INT, src, tag, comm, req); break;
    case WK_RECV_INIT:
        MPI_Recv_init(rbuf, WC_CAP, MPI_INT, src, tag, comm, req);
        MPI_Start(req);
        *persistent = 1;
        break;
    case WK_SENDRECV:
        MPI_Sendrecv(sbuf, slen, MPI_INT, dest, stag, rbuf, WC_CAP, MPI_INT, src, tag, comm, st);
        break;
    case WK_SENDRECV_REPLACE: /* one count for both halves: the message received has the same length */
        memcpy(rbuf, sbuf, slen * sizeof(int));
        MPI_Sendrecv_replace(rbuf, slen, MPI_INT, dest, stag, src, tag, comm, st);
        break;
    case WK_MPROBE:
        MPI_Mprobe(src, tag, comm, &msg, st);
        MPI_Get_count(st, MPI_INT, &cnt);
        MPI_Mrecv(rbuf, cnt, MPI_INT, &msg, st);
        break;
    case WK_IMPROBE:
        do
            MPI_Improbe(src, tag, comm, &flag, &msg, st);
        while (!flag);
        MPI_Get_count(st, MPI_INT, &cnt);
        MPI_Imrecv(rbuf, cnt, MPI_INT, &msg, req);
        break;
#if HAVE_MPI4
    case WK_ISENDRECV:
        MPI_Isendrecv(sbuf, slen, MPI_INT, dest, stag, rbuf, WC_CAP, MPI_INT, src, tag, comm, req);
        break;
    case WK_ISENDRECV_REPLACE:
        memcpy(rbuf, sbuf, slen * sizeof(int));
        MPI_Isendrecv_replace(rbuf, slen, MPI_INT, dest, stag, src, tag, comm, req);
        break;
    case WK_RECV_C: MPI_Recv_c(rbuf, WC_CAP, MPI_INT, src, tag, comm, st); break;
    case WK_IRECV_C: MPI_Irecv_c(rbuf, WC_CAP, MPI_INT, src, tag, comm, req); break;
    case WK_RECV_INIT_C:
        MPI_Recv_init_c(rbuf, WC_CAP, MPI_INT, src, tag, comm, req);
        MPI_Start(req);
        *persistent = 1;
        break;
    case WK_SENDRECV_C:
        MPI_Sendrecv_c(sbuf, slen, MPI_INT, dest, stag, rbuf, WC_CAP, MPI_INT, src, tag, comm, st);
        break;
    case WK_SENDRECV_REPLACE_C:
        memcpy(rbuf, sbuf, slen * sizeof(int));
        MPI_Sendrecv_replace_c(rbuf, slen, MPI_INT, dest, stag, src, tag, comm, st);
        break;
    case WK_MPROBE_C:
        MPI_Mprobe(src, tag, comm, &msg, st);
        MPI_Get_count(st, MPI_INT, &cnt);
        MPI_Mrecv_c(rbuf, cnt, MPI_INT, &msg, st);
        break;
    case WK_IMPROBE_C:
        do
            MPI_Improbe(src, tag, comm, &flag, &msg, st);
        while (!flag);
        MPI_Get_count(st, MPI_INT, &cnt);
        MPI_Imrecv_c(rbuf, cnt, MPI_INT, &msg, req);
        break;
    case WK_ISENDRECV_C:
        MPI_Isendrecv_c(sbuf, slen, MPI_INT, dest, stag, rbuf, WC_CAP, MPI_INT, src, tag, comm, req);
        break;
    case WK_ISENDRECV_REPLACE_C:
        memcpy(rbuf, sbuf, slen * sizeof(int));
        MPI_Isendrecv_replace_c(rbuf, slen, MPI_INT, dest, stag, src, tag, comm, req);
        break;
#endif
    }
}

/* Complete the requests still pending (blocking kinds left MPI_REQUEST_NULL and already have their status), keeping
   each status with its request; mode picks Waitall, Wait in reverse order, Waitany or Testsome. Frees persistent ones. */
static void wc_complete(int n, MPI_Request *req, MPI_Status *st, const int *persistent, int mode)
{
    MPI_Status *tmp = malloc(n * sizeof(MPI_Status));
    int *pending = malloc(n * sizeof(int)), *idx = malloc(n * sizeof(int));
    int i, outcount, done = 0, active = 0;
    for (i = 0; i < n; i++)
        active += pending[i] = req[i] != MPI_REQUEST_NULL;
    switch (mode % 4) {
    case 0:
        MPI_Waitall(n, req, tmp);
        for (i = 0; i < n; i++)
            if (pending[i])
                st[i] = tmp[i];
        break;
    case 1:
        for (i = n - 1; i >= 0; i--)
            if (pending[i])
                MPI_Wait(&req[i], &st[i]);
        break;
    case 2:
        for (; done < active; done++) {
            MPI_Waitany(n, req, &i, &tmp[0]);
            if (i == MPI_UNDEFINED)
                break;
            st[i] = tmp[0]; /* a completed persistent request is inactive: Waitany doesn't return it again */
        }
        break;
    default: /* Testsome: status k belongs to request idx[k] */
        while (done < active) {
            MPI_Testsome(n, req, &outcount, idx, tmp);
            if (outcount == MPI_UNDEFINED)
                break;
            for (i = 0; i < outcount; i++, done++)
                st[idx[i]] = tmp[i];
        }
        break;
    }
    for (i = 0; i < n; i++)
        if (persistent[i])
            MPI_Request_free(&req[i]);
    free(tmp);
    free(pending);
    free(idx);
}

/* a receive got message m of src with tag tag */
static void wc_check(const int *rbuf, const MPI_Status *st, int it, int src, int m, int tag, const char *what, const char *kind)
{
    int cnt;
    MPI_Get_count(st, MPI_INT, &cnt);
    EXPECT(st->MPI_SOURCE == src, "%s (%s): source %d, expected %d", what, kind, st->MPI_SOURCE, src);
    EXPECT(st->MPI_TAG == tag, "%s (%s): tag %d, expected %d", what, kind, st->MPI_TAG, tag);
    EXPECT(cnt == wc_len(m), "%s (%s): %d ints, expected %d (message %d)", what, kind, cnt, wc_len(m), m);
    check_int(rbuf, wc_len(m), wc_base(src, it, m), what);
}

static void wildcards(int it)
{
    const int src4[4] = {MPI_ANY_SOURCE, left, MPI_ANY_SOURCE, left};
    const int tag4[4] = {MPI_ANY_TAG, MPI_ANY_TAG, WC_TAG, WC_TAG};
    int sbuf[WC_MSGS][WC_CAP], rbuf[WC_MSGS][WC_CAP], kinds[WC_MSGS], rpers[WC_MSGS], spers[WC_MSGS], seq, k;
    int recv_kinds[WK_COUNT], nrk = 0;
    MPI_Request sreq[WC_MSGS], rreq[WC_MSGS];
    MPI_Status st[WC_MSGS];
    char what[96];
    const int send_first = g_rank % 2 == 0;

    for (k = 0; k < WK_COUNT; k++) /* kinds that only receive, for the phases with fixed sends */
        if (!wc_sends(k))
            recv_kinds[nrk++] = k;

    /* 1. same sends, consecutive receives: every receive kind against every send kind, every envelope */
    const int nseq = (int)WK_COUNT > (int)SK_COUNT ? (int)WK_COUNT : (int)SK_COUNT;
    for (seq = 0; seq < nseq; seq++) {
        for (k = 0; k < WC_MSGS; k++) {
            const int env = (seq + 3 * k + it + g_rank) % 4;
            const int skind = (seq + 2 * k + 3 * g_rank + it) % SK_COUNT;
            kinds[k] = (seq + k + g_rank) % WK_COUNT;
            fill_int(sbuf[k], wc_len(k), wc_base(g_rank, it, k));
            sreq[k] = MPI_REQUEST_NULL;
            spers[k] = 0;
            if (!wc_sends(kinds[k]) && send_first)
                wc_send(skind, sbuf[k], wc_len(k), right, WC_TAG, wcomm, &sreq[k], &spers[k]);
            wc_post(kinds[k], src4[env], tag4[env], rbuf[k], sbuf[k], wc_len(k), right, WC_TAG, wcomm, &rreq[k],
                    &rpers[k], &st[k]);
            if (!wc_sends(kinds[k]) && !send_first)
                wc_send(skind, sbuf[k], wc_len(k), right, WC_TAG, wcomm, &sreq[k], &spers[k]);
        }
        wc_complete(WC_MSGS, rreq, st, rpers, seq + it);
        snprintf(what, sizeof(what), "wildcards 1, sequence %d, receive", seq);
        for (k = 0; k < WC_MSGS; k++)
            wc_check(rbuf[k], &st[k], it, left, k, WC_TAG, what, wc_kind_name[kinds[k]]);
        {
            MPI_Status sst[WC_MSGS];
            wc_complete(WC_MSGS, sreq, sst, spers, 0);
        }
    }

    /* 2. two tags, sent T,T2,T,T2: the receive (left,T2) posted first takes message 1, then (ANY,ANY) message 0,
          (left,T) message 2 and (ANY,T2) message 3; send and receive kinds rotate */
    {
        const int tags[WC_MSGS] = {WC_TAG, WC_TAG2, WC_TAG, WC_TAG2};
        const int src[WC_MSGS] = {left, MPI_ANY_SOURCE, left, MPI_ANY_SOURCE};
        const int tag[WC_MSGS] = {WC_TAG2, MPI_ANY_TAG, WC_TAG, WC_TAG2};
        const int want[WC_MSGS] = {1, 0, 2, 3};
        /* all four messages go out first (nonblocking/buffered kinds only: the receives are posted after them) */
        const int early_sends[] = {SK_ISEND, SK_IBSEND, SK_SEND_INIT, SK_BSEND, SK_ISSEND, SK_SSEND_INIT
#if HAVE_MPI4
                                   , SK_ISEND_C, SK_IBSEND_C, SK_SEND_INIT_C, SK_BSEND_C, SK_ISSEND_C, SK_SSEND_INIT_C
#endif
        };
        const int nes = sizeof(early_sends) / sizeof(early_sends[0]);
        for (k = 0; k < WC_MSGS; k++) {
            fill_int(sbuf[k], wc_len(k), wc_base(g_rank, it, k));
            wc_send(early_sends[(it + k + g_rank) % nes], sbuf[k], wc_len(k), right, tags[k], wcomm, &sreq[k], &spers[k]);
        }
        for (k = 0; k < WC_MSGS; k++) {
            kinds[k] = recv_kinds[(it + k + g_rank) % nrk];
            wc_post(kinds[k], src[k], tag[k], rbuf[k], NULL, 0, right, 0, wcomm, &rreq[k], &rpers[k], &st[k]);
        }
        wc_complete(WC_MSGS, rreq, st, rpers, it);
        for (k = 0; k < WC_MSGS; k++)
            wc_check(rbuf[k], &st[k], it, left, want[k], tags[want[k]], "wildcards 2 (two tags)", wc_kind_name[kinds[k]]);
        {
            MPI_Status sst[WC_MSGS];
            wc_complete(WC_MSGS, sreq, sst, spers, 0);
        }
    }

    /* 3. fan-in (3+ ranks): the others send 2 messages each to one root (tag WC_TAG + sender) with rotating send
          kinds; the root takes them with MPI_ANY_SOURCE and MPI_ANY_TAG in rotating receive kinds. Per sender the
          earlier posted receive must get the first message. */
    if (g_size > 2) {
        const int root = it % g_size;
        if (g_rank == root) {
            const int n = 2 * (g_size - 1);
            int *got = calloc(g_size, sizeof(int)), *kind = malloc(n * sizeof(int)), *pers = malloc(n * sizeof(int));
            int (*b)[WC_CAP] = malloc(n * sizeof(*b));
            MPI_Request *r = malloc(n * sizeof(MPI_Request));
            MPI_Status *s = malloc(n * sizeof(MPI_Status));
            for (k = 0; k < n; k++) {
                kind[k] = recv_kinds[(it + k) % nrk];
                wc_post(kind[k], MPI_ANY_SOURCE, MPI_ANY_TAG, b[k], NULL, 0, 0, 0, fcomm, &r[k], &pers[k], &s[k]);
            }
            wc_complete(n, r, s, pers, it);
            for (k = 0; k < n; k++) {
                const int src = s[k].MPI_SOURCE;
                EXPECT(src >= 0 && src < g_size && src != root, "wildcards 3 (fan-in, %s): source %d", wc_kind_name[kind[k]], src);
                if (src >= 0 && src < g_size && src != root)
                    wc_check(b[k], &s[k], it, src, got[src]++, WC_TAG + src, "wildcards 3 (fan-in)", wc_kind_name[kind[k]]);
            }
            for (k = 0; k < g_size; k++)
                EXPECT(k == root || got[k] == 2, "wildcards 3 (fan-in): %d messages from %d", got[k], k);
            free(got);
            free(kind);
            free(pers);
            free(b);
            free(r);
            free(s);
        } else {
            MPI_Status sst[2];
            for (k = 0; k < 2; k++) {
                fill_int(sbuf[k], wc_len(k), wc_base(g_rank, it, k));
                wc_send((it + k + g_rank) % SK_COUNT, sbuf[k], wc_len(k), root, WC_TAG + g_rank, fcomm, &sreq[k], &spers[k]);
            }
            wc_complete(2, sreq, sst, spers, 0);
        }
    }
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
        for (i = 0; i < 4; i++) {
#if defined(OPEN_MPI) && (OMPI_MAJOR_VERSION < 5 || (OMPI_MAJOR_VERSION == 5 && OMPI_MINOR_VERSION == 0 && OMPI_RELEASE_VERSION < 11))
            /* Known Open MPI bug (4.x and 5.0.0-5.0.10): a restarted MPI_Bsend_init request delivers wrong data
               (also without tractor; Open MPI 5.0.11 is fine). Report it instead of failing. */
            if (i == 1) {
                static int reported = 0;
                if (rbuf[1][0] != left + it + 1 && !reported++ && g_rank == 0)
                    fprintf(stderr, "KNOWN OPEN MPI BUG (< 5.0.11): restarted persistent MPI_Bsend_init delivers wrong data\n");
                continue;
            }
#endif
            check_int(rbuf[i], N, left + it + i, "persistent");
        }
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
    int icnt;
    int i, flag;
    const int parts = 4;

    fill_int(sbuf, N, g_rank + it);

    /* Isendrecv / Isendrecv_replace */
    /* clear the status first: a status left over from earlier receives must not hide a wrong one */
    memset(&st, 0, sizeof(st));
    MPI_Isendrecv(sbuf, N, MPI_INT, right, 90, rbuf, N, MPI_INT, left, 90, MPI_COMM_WORLD, &reqs[0]);
    MPI_Wait(&reqs[0], &st);
    MPI_Get_count(&st, MPI_INT, &icnt);
    if (icnt != N || st.MPI_SOURCE != left || st.MPI_TAG != 90) {
#ifdef MPICH_NUMVERSION
        /* Known MPICH bug (stock 4.x/5.x, ch3 and ch4): the status of an MPI_Isendrecv request is not filled
           from the receive (ch3 returns zeros, ch4 leaves the caller's status untouched). Fixed by the
           mpi.tool/patches/mpich-* patches; report it on unpatched MPICH-based MPIs instead of failing. */
        static int reported = 0;
        if (!reported++ && g_rank == 0)
            fprintf(stderr, "KNOWN MPICH BUG: MPI_Isendrecv status count=%d source=%d tag=%d, expected %d/%d/90 "
                            "(unpatched MPICH, see mpi.tool/patches)\n", icnt, st.MPI_SOURCE, st.MPI_TAG, N, left);
#else
        FAIL("Isendrecv status wrong: count=%d source=%d tag=%d, expected %d/%d/90", icnt, st.MPI_SOURCE, st.MPI_TAG, N, left);
#endif
    }
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
    MPI_Comm_dup(MPI_COMM_WORLD, &wcomm);
    MPI_Comm_dup(MPI_COMM_WORLD, &fcomm);

    for (it = 0; it < iterations; it++) {
        blocking_modes(it);
        nonblocking_modes(it);
        probes(it);
        persistent(it);
        cancel_and_free(it);
        wildcards(it);
#if HAVE_MPI4
        mpi4_features(it);
#endif
    }

    MPI_Comm_free(&wcomm);
    MPI_Comm_free(&fcomm);
    MPI_Buffer_detach(&detached, &bsize);
    free(bsend_buf);

    {
        int rc = test_report("p2p_test", iterations);
        MPI_Finalize();
        return rc;
    }
}
