/* One-sided (RMA) class test.
   Window creation flavours (create, allocate, allocate_shared, dynamic), the
   three synchronisation models (fence, post/start/complete/wait, lock/unlock and
   lock_all), all communication calls including request-based ones, flushes,
   window attributes/info/names/errhandlers and (MPI-4) large-count variants.
   Usage: rma_test [iterations]      (any number of ranks; 2 or more preferred) */
#include "test_common.h"

#define N 256
/* window layout (ints): [0,N) put inbox, [N,2N) accumulate inbox,
   [2N,3N) source data for gets, [3N,3N+8) atomics */
#define WSIZE (3 * N + 8)

static int right, left;

static void reset_window(int *w, int it)
{
    memset(w, 0, WSIZE * sizeof(int));
    fill_int(w + 2 * N, N, g_rank * 10 + it); /* what neighbours will Get */
}

static void fence_epoch(MPI_Win win, int *w, int it)
{
    int sbuf[N], gbuf[N], abuf[N], rbuf[N], i;
    int one = 1, fetched = -1, cmp = 0, swap = g_rank + 1, cas_res = -1;

    fill_int(sbuf, N, g_rank + it);
    for (i = 0; i < N; i++)
        abuf[i] = 1;

    MPI_Win_fence(MPI_MODE_NOPRECEDE, win);
    MPI_Put(sbuf, N, MPI_INT, right, 0, N, MPI_INT, win);
    MPI_Get(gbuf, N, MPI_INT, left, 2 * N, N, MPI_INT, win);
    MPI_Accumulate(abuf, N, MPI_INT, right, N, N, MPI_INT, MPI_SUM, win);
    MPI_Accumulate(abuf, N, MPI_INT, left, N, N, MPI_INT, MPI_SUM, win);
    MPI_Win_fence(0, win);
    check_int(w, N, left + it, "Put (fence)");
    check_int(gbuf, N, left * 10 + it, "Get (fence)");
    /* the accumulate inbox is checked after the next epoch: neighbours may
       already be adding to it once they leave the fence */

    MPI_Get_accumulate(abuf, N, MPI_INT, rbuf, N, MPI_INT, right, N, N, MPI_INT, MPI_SUM, win);
    MPI_Fetch_and_op(&one, &fetched, MPI_INT, right, 3 * N, MPI_SUM, win);
    MPI_Compare_and_swap(&swap, &cmp, &cas_res, MPI_INT, right, 3 * N + 1, win);
    MPI_Win_fence(0, win);
    for (i = 0; i < N; i++)
        EXPECT(rbuf[i] == 2, "Get_accumulate (fence) result at %d: %d", i, rbuf[i]);
    for (i = 0; i < N; i++)
        EXPECT(w[N + i] == 3, "Accumulate + Get_accumulate (fence) target at %d: %d", i, w[N + i]);
    EXPECT(fetched == 0, "Fetch_and_op returned %d", fetched);
    EXPECT(w[3 * N] == 1, "Fetch_and_op target %d", w[3 * N]);
    EXPECT(cas_res == 0, "Compare_and_swap returned %d", cas_res);
    EXPECT(w[3 * N + 1] == left + 1, "Compare_and_swap target %d", w[3 * N + 1]);
    MPI_Win_fence(MPI_MODE_NOSUCCEED, win);
}

static void passive_target(MPI_Win win, int *w, int it)
{
    int sbuf[N], gbuf[N], abuf[N], rbuf[N], i;
    MPI_Request reqs[4];

    fill_int(sbuf, N, g_rank + it + 1);
    for (i = 0; i < N; i++)
        abuf[i] = 2;
    reset_window(w, it);
    MPI_Barrier(MPI_COMM_WORLD);

    MPI_Win_lock(MPI_LOCK_EXCLUSIVE, right, 0, win);
    MPI_Put(sbuf, N, MPI_INT, right, 0, N, MPI_INT, win);
    MPI_Win_flush(right, win);
    MPI_Get(gbuf, N, MPI_INT, right, 2 * N, N, MPI_INT, win);
    MPI_Win_flush_local(right, win);
    check_int(gbuf, N, right * 10 + it, "Get (lock)");
    MPI_Win_unlock(right, win);

    MPI_Win_lock(MPI_LOCK_SHARED, left, MPI_MODE_NOCHECK, win);
    MPI_Rput(sbuf, N, MPI_INT, left, N, N, MPI_INT, win, &reqs[0]);
    MPI_Rget(gbuf, N, MPI_INT, left, 2 * N, N, MPI_INT, win, &reqs[1]);
    MPI_Waitall(2, reqs, MPI_STATUSES_IGNORE);
    MPI_Win_flush(left, win); /* order the put before the accumulates */
    MPI_Raccumulate(abuf, N, MPI_INT, left, N, N, MPI_INT, MPI_SUM, win, &reqs[2]);
    MPI_Rget_accumulate(abuf, N, MPI_INT, rbuf, N, MPI_INT, left, N, N, MPI_INT, MPI_SUM, win, &reqs[3]);
    MPI_Waitall(2, &reqs[2], MPI_STATUSES_IGNORE);
    MPI_Win_unlock(left, win);
    check_int(gbuf, N, left * 10 + it, "Rget (lock)");

    MPI_Win_lock_all(0, win);
    MPI_Put(sbuf, N / 2, MPI_INT, right, 0, N / 2, MPI_INT, win);
    MPI_Win_flush_all(win);
    MPI_Get(gbuf, N, MPI_INT, left, 2 * N, N, MPI_INT, win);
    MPI_Win_flush_local_all(win);
    MPI_Win_sync(win);
    MPI_Win_unlock_all(win);
    check_int(gbuf, N, left * 10 + it, "Get (lock_all)");

    MPI_Barrier(MPI_COMM_WORLD);
    check_int(w, N, left + it + 1, "Put (lock)");
    /* accumulate inbox: Rput (rank+it+1+i) from right, then +2 twice */
    for (i = 0; i < N; i++)
        EXPECT(w[N + i] == right + it + 1 + i + 4, "Raccumulate/Rget_accumulate at %d: %d", i, w[N + i]);
}

static void pscw(MPI_Win win, int *w, int it)
{
    MPI_Group wgroup, nb;
    int ranks[2], n, sbuf[N], flag;

    fill_int(sbuf, N, g_rank + it + 2);
    reset_window(w, it);
    MPI_Barrier(MPI_COMM_WORLD);

    ranks[0] = left;
    ranks[1] = right;
    n = (left == right) ? 1 : 2;
    MPI_Win_get_group(win, &wgroup);
    MPI_Group_incl(wgroup, n, ranks, &nb);
    MPI_Group_free(&wgroup);

    MPI_Win_post(nb, 0, win);
    MPI_Win_start(nb, 0, win);
    MPI_Put(sbuf, N, MPI_INT, right, 0, N, MPI_INT, win);
    MPI_Win_complete(win);
    do {
        MPI_Win_test(win, &flag);
    } while (!flag);
    check_int(w, N, left + it + 2, "Put (post/start/complete/test)");

    MPI_Win_post(nb, MPI_MODE_NOCHECK, win);
    MPI_Barrier(MPI_COMM_WORLD); /* NOCHECK: posts must precede the starts */
    MPI_Win_start(nb, MPI_MODE_NOCHECK, win);
    MPI_Accumulate(sbuf, N, MPI_INT, left, N, N, MPI_INT, MPI_REPLACE, win);
    MPI_Win_complete(win);
    MPI_Win_wait(win);
    check_int(w + N, N, right + it + 2, "Accumulate (post/start/complete/wait)");

    MPI_Group_free(&nb);
}

static int keyval_copies = 0;
static int keyval_copy(MPI_Win oldwin, int keyval, void *extra, void *attr_in, void *attr_out, int *flag)
{
    (void)oldwin; (void)keyval; (void)extra; (void)attr_in; (void)attr_out;
    *flag = 0;
    keyval_copies++;
    return MPI_SUCCESS;
}

static void window_local(MPI_Win win, int it)
{
    char name[MPI_MAX_OBJECT_NAME];
    int len, flag, keyval, *disp;
    MPI_Aint *wsize;
    MPI_Info info;
    MPI_Errhandler eh;
    static int attr_val = 42;
    void *got;
    int *pflavor, *pmodel;

    snprintf(name, sizeof(name), "rma_win_%d", it);
    MPI_Win_set_name(win, name);
    MPI_Win_get_name(win, name, &len);
    EXPECT(strncmp(name, "rma_win_", 8) == 0, "Win_get_name '%s'", name);

    MPI_Win_get_attr(win, MPI_WIN_SIZE, &wsize, &flag);
    EXPECT(flag, "MPI_WIN_SIZE attribute missing");
    MPI_Win_get_attr(win, MPI_WIN_DISP_UNIT, &disp, &flag);
    EXPECT(flag && *disp == (int)sizeof(int), "MPI_WIN_DISP_UNIT wrong");
    MPI_Win_get_attr(win, MPI_WIN_CREATE_FLAVOR, &pflavor, &flag);
    MPI_Win_get_attr(win, MPI_WIN_MODEL, &pmodel, &flag);

    MPI_Win_create_keyval(keyval_copy, MPI_WIN_NULL_DELETE_FN, &keyval, NULL);
    MPI_Win_set_attr(win, keyval, &attr_val);
    MPI_Win_get_attr(win, keyval, &got, &flag);
    EXPECT(flag && got == &attr_val, "user window attribute lost");
    MPI_Win_delete_attr(win, keyval);
    MPI_Win_get_attr(win, keyval, &got, &flag);
    EXPECT(!flag, "user window attribute survived delete");
    MPI_Win_free_keyval(&keyval);

    MPI_Info_create(&info);
    MPI_Info_set(info, "no_locks", "false");
    MPI_Win_set_info(win, info);
    MPI_Info_free(&info);
    MPI_Win_get_info(win, &info);
    MPI_Info_free(&info);

    MPI_Win_get_errhandler(win, &eh);
    MPI_Win_set_errhandler(win, MPI_ERRORS_RETURN);
    MPI_Win_call_errhandler(win, MPI_SUCCESS);
    MPI_Win_set_errhandler(win, eh);
    MPI_Errhandler_free(&eh);
}

static void run_on_window(MPI_Win win, int *w, int it)
{
    reset_window(w, it);
    MPI_Barrier(MPI_COMM_WORLD);
    fence_epoch(win, w, it);
    passive_target(win, w, it);
    pscw(win, w, it);
    window_local(win, it);
}

static void shared_and_dynamic(int it)
{
    MPI_Comm shm;
    MPI_Win win;
    int *base, *peer, srank, ssize, sright, sleft, dunit;
    MPI_Aint ssz, *addrs, myaddr;
    int dyn[N], gbuf[N];

    /* allocate_shared + shared_query */
    MPI_Comm_split_type(MPI_COMM_WORLD, MPI_COMM_TYPE_SHARED, g_rank, MPI_INFO_NULL, &shm);
    MPI_Comm_rank(shm, &srank);
    MPI_Comm_size(shm, &ssize);
    sright = (srank + 1) % ssize;
    sleft = (srank + ssize - 1) % ssize;
    MPI_Win_allocate_shared(N * sizeof(int), sizeof(int), MPI_INFO_NULL, shm, &base, &win);
    MPI_Win_shared_query(win, sright, &ssz, &dunit, &peer);
    EXPECT(ssz == (MPI_Aint)(N * sizeof(int)) && dunit == (int)sizeof(int), "Win_shared_query size/disp");
    MPI_Win_lock_all(MPI_MODE_NOCHECK, win);
    fill_int(base, N, srank + it);
    MPI_Win_sync(win);
    MPI_Barrier(shm);
    MPI_Win_sync(win);
    check_int(peer, N, sright + it, "Win_allocate_shared direct load");
    MPI_Win_unlock_all(win);
    MPI_Win_free(&win);
    (void)sleft;
    MPI_Comm_free(&shm);

    /* create_dynamic + attach/detach */
    MPI_Win_create_dynamic(MPI_INFO_NULL, MPI_COMM_WORLD, &win);
    fill_int(dyn, N, g_rank * 3 + it);
    MPI_Win_attach(win, dyn, N * sizeof(int));
    MPI_Get_address(dyn, &myaddr);
    addrs = (MPI_Aint *)malloc(g_size * sizeof(MPI_Aint));
    MPI_Allgather(&myaddr, 1, MPI_AINT, addrs, 1, MPI_AINT, MPI_COMM_WORLD);
    MPI_Win_lock(MPI_LOCK_SHARED, left, 0, win);
    MPI_Get(gbuf, N, MPI_INT, left, addrs[left], N, MPI_INT, win);
    MPI_Win_unlock(left, win);
    check_int(gbuf, N, left * 3 + it, "Get on dynamic window");
    MPI_Barrier(MPI_COMM_WORLD);
    MPI_Win_detach(win, dyn);
    MPI_Win_free(&win);
    free(addrs);
}

#if HAVE_MPI4
static void large_count(int it)
{
    MPI_Win win;
    int *w, *abase, *sbase, *peer, sbuf[N], gbuf[N], abuf[N], rbuf[N], i;
    MPI_Aint dunit;
    MPI_Count cN = N;
    MPI_Request reqs[4];
    MPI_Comm shm;
    MPI_Aint ssz;

    w = (int *)malloc(WSIZE * sizeof(int));
    reset_window(w, it);
    fill_int(sbuf, N, g_rank + it + 5);
    for (i = 0; i < N; i++)
        abuf[i] = 3;

    MPI_Win_create_c(w, (MPI_Aint)(WSIZE * sizeof(int)), sizeof(int), MPI_INFO_NULL, MPI_COMM_WORLD, &win);
    MPI_Win_fence(0, win);
    MPI_Put_c(sbuf, cN, MPI_INT, right, 0, cN, MPI_INT, win);
    MPI_Get_c(gbuf, cN, MPI_INT, left, 2 * N, cN, MPI_INT, win);
    MPI_Accumulate_c(abuf, cN, MPI_INT, right, N, cN, MPI_INT, MPI_SUM, win);
    MPI_Win_fence(0, win);
    check_int(w, N, left + it + 5, "Put_c");
    check_int(gbuf, N, left * 10 + it, "Get_c");
    MPI_Get_accumulate_c(abuf, cN, MPI_INT, rbuf, cN, MPI_INT, right, N, cN, MPI_INT, MPI_SUM, win);
    MPI_Win_fence(0, win);
    for (i = 0; i < N; i++)
        EXPECT(rbuf[i] == 3 && w[N + i] == 6, "Accumulate_c/Get_accumulate_c at %d", i);
    MPI_Barrier(MPI_COMM_WORLD); /* everyone is done reading before the passive epoch writes */

    MPI_Win_lock_all(0, win);
    MPI_Rput_c(sbuf, cN, MPI_INT, left, 0, cN, MPI_INT, win, &reqs[0]);
    MPI_Rget_c(gbuf, cN, MPI_INT, right, 2 * N, cN, MPI_INT, win, &reqs[1]);
    MPI_Raccumulate_c(abuf, cN, MPI_INT, left, N, cN, MPI_INT, MPI_SUM, win, &reqs[2]);
    MPI_Rget_accumulate_c(abuf, cN, MPI_INT, rbuf, cN, MPI_INT, left, N, cN, MPI_INT, MPI_SUM, win, &reqs[3]);
    MPI_Waitall(4, reqs, MPI_STATUSES_IGNORE);
    MPI_Win_unlock_all(win);
    check_int(gbuf, N, right * 10 + it, "Rget_c");
    MPI_Barrier(MPI_COMM_WORLD);
    check_int(w, N, right + it + 5, "Rput_c");
    MPI_Win_free(&win);
    free(w);

    MPI_Win_allocate_c((MPI_Aint)(N * sizeof(int)), sizeof(int), MPI_INFO_NULL, MPI_COMM_WORLD, &abase, &win);
    fill_int(abase, N, g_rank * 7 + it);
    MPI_Win_fence(0, win);
    MPI_Get_c(gbuf, cN, MPI_INT, right, 0, cN, MPI_INT, win);
    MPI_Win_fence(0, win);
    check_int(gbuf, N, right * 7 + it, "Win_allocate_c/Get_c");
    MPI_Win_free(&win);

    MPI_Comm_split_type(MPI_COMM_WORLD, MPI_COMM_TYPE_SHARED, g_rank, MPI_INFO_NULL, &shm);
    MPI_Win_allocate_shared_c((MPI_Aint)(N * sizeof(int)), sizeof(int), MPI_INFO_NULL, shm, &sbase, &win);
    MPI_Win_shared_query_c(win, 0, &ssz, &dunit, &peer);
    EXPECT(ssz == (MPI_Aint)(N * sizeof(int)), "Win_shared_query_c size");
    MPI_Win_free(&win);
    MPI_Comm_free(&shm);
}
#endif

int main(int argc, char **argv)
{
    int iterations, it, *w, *abase;
    MPI_Win win;

    test_init(&argc, &argv);
    iterations = parse_iterations(argc, argv, 10);
    right = (g_rank + 1) % g_size;
    left = (g_rank + g_size - 1) % g_size;

    w = (int *)malloc(WSIZE * sizeof(int));

    for (it = 0; it < iterations; it++) {
        MPI_Win_create(w, (MPI_Aint)(WSIZE * sizeof(int)), sizeof(int), MPI_INFO_NULL, MPI_COMM_WORLD, &win);
        run_on_window(win, w, it);
        MPI_Win_free(&win);

        MPI_Win_allocate((MPI_Aint)(WSIZE * sizeof(int)), sizeof(int), MPI_INFO_NULL, MPI_COMM_WORLD, &abase, &win);
        run_on_window(win, abase, it);
        MPI_Win_free(&win);

        shared_and_dynamic(it);
#if HAVE_MPI4
        large_count(it);
#endif
    }
    free(w);

    {
        int rc = test_report("rma_test", iterations);
        MPI_Finalize();
        return rc;
    }
}
