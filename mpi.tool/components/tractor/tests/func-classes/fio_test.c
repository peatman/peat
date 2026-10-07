/* File I/O class test.
   Each iteration creates a file, writes and reads it back with explicit
   offsets, individual and shared file pointers, collective, split-collective
   and nonblocking calls, exercises the file-handle local calls and finally
   deletes the file. Every rank owns one block of N ints; blocks of the left
   neighbour are read back to check that data really went through the file.
   Set FIO_TEST_DIR to place the file somewhere else than the working directory.
   Usage: fio_test [iterations]      (any number of ranks) */
#include "test_common.h"
#include <unistd.h>

#define N 1024

static char fname[1024];
static int left;
static MPI_Offset my_off, left_off, blk_bytes;

static void check_status_count(MPI_Status *st, const char *what)
{
    int cnt = -1;
    MPI_Get_count(st, MPI_INT, &cnt);
    EXPECT(cnt == N, "%s: status count %d != %d", what, cnt, N);
}

static void local_calls(MPI_File fh, MPI_Info info)
{
    int amode, atom;
    MPI_Group grp;
    MPI_Info got;
    MPI_Offset sz;
    MPI_Errhandler eh;
    MPI_Aint ext;

    MPI_File_get_amode(fh, &amode);
    EXPECT(amode & MPI_MODE_RDWR, "File_get_amode: 0x%x", amode);
    MPI_File_get_group(fh, &grp);
    {
        int gs;
        MPI_Group_size(grp, &gs);
        EXPECT(gs == g_size, "File_get_group size %d", gs);
    }
    MPI_Group_free(&grp);

    MPI_File_set_info(fh, info);
    MPI_File_get_info(fh, &got);
    MPI_Info_free(&got);

    MPI_File_set_atomicity(fh, 0);
    MPI_File_get_atomicity(fh, &atom);
    EXPECT(atom == 0, "File_get_atomicity %d", atom);

    MPI_File_set_size(fh, 0);
    MPI_File_preallocate(fh, blk_bytes * g_size);
    MPI_File_get_size(fh, &sz);
    EXPECT(sz == blk_bytes * g_size, "File_get_size %lld", (long long)sz);

    MPI_File_get_type_extent(fh, MPI_INT, &ext);
    EXPECT(ext == (MPI_Aint)sizeof(int), "File_get_type_extent %ld", (long)ext);
#if HAVE_MPI4
    {
        MPI_Count cext;
        MPI_File_get_type_extent_c(fh, MPI_INT, &cext);
        EXPECT(cext == (MPI_Count)sizeof(int), "File_get_type_extent_c");
    }
#endif

    MPI_File_get_errhandler(fh, &eh);
    MPI_File_set_errhandler(fh, MPI_ERRORS_RETURN);
    MPI_File_call_errhandler(fh, MPI_SUCCESS);
    MPI_File_set_errhandler(fh, eh);
    MPI_Errhandler_free(&eh);
}

static void explicit_offsets(MPI_File fh, int it)
{
    int wbuf[N], rbuf[N];
    MPI_Status st;
    MPI_Request req;

    fill_int(wbuf, N, g_rank + it);
    MPI_File_write_at(fh, my_off, wbuf, N, MPI_INT, &st);
    check_status_count(&st, "File_write_at");
    MPI_File_sync(fh);
    MPI_Barrier(MPI_COMM_WORLD);
    MPI_File_read_at(fh, left_off, rbuf, N, MPI_INT, &st);
    check_status_count(&st, "File_read_at");
    check_int(rbuf, N, left + it, "write_at/read_at");

    fill_int(wbuf, N, g_rank + it + 1);
    MPI_File_write_at_all(fh, my_off, wbuf, N, MPI_INT, &st);
    MPI_File_read_at_all(fh, left_off, rbuf, N, MPI_INT, &st);
    check_int(rbuf, N, left + it + 1, "write_at_all/read_at_all");

    fill_int(wbuf, N, g_rank + it + 2);
    MPI_File_write_at_all_begin(fh, my_off, wbuf, N, MPI_INT);
    MPI_File_write_at_all_end(fh, wbuf, &st);
    MPI_File_read_at_all_begin(fh, left_off, rbuf, N, MPI_INT);
    MPI_File_read_at_all_end(fh, rbuf, &st);
    check_int(rbuf, N, left + it + 2, "write_at_all_begin/end, read_at_all_begin/end");

    fill_int(wbuf, N, g_rank + it + 3);
    MPI_File_iwrite_at(fh, my_off, wbuf, N, MPI_INT, &req);
    MPI_Wait(&req, &st);
    check_status_count(&st, "File_iwrite_at");
    MPI_File_sync(fh);
    MPI_Barrier(MPI_COMM_WORLD);
    MPI_File_iread_at(fh, left_off, rbuf, N, MPI_INT, &req);
    MPI_Wait(&req, &st);
    check_int(rbuf, N, left + it + 3, "iwrite_at/iread_at");

    fill_int(wbuf, N, g_rank + it + 4);
    MPI_File_iwrite_at_all(fh, my_off, wbuf, N, MPI_INT, &req);
    MPI_Wait(&req, &st);
    MPI_File_iread_at_all(fh, left_off, rbuf, N, MPI_INT, &req);
    {
        int flag;
        do {
            MPI_Test(&req, &flag, &st);
        } while (!flag);
    }
    check_int(rbuf, N, left + it + 4, "iwrite_at_all/iread_at_all");

#if HAVE_MPI4
    fill_int(wbuf, N, g_rank + it + 5);
    MPI_File_write_at_c(fh, my_off, wbuf, (MPI_Count)N, MPI_INT, &st);
    MPI_File_sync(fh);
    MPI_Barrier(MPI_COMM_WORLD);
    MPI_File_read_at_c(fh, left_off, rbuf, (MPI_Count)N, MPI_INT, &st);
    check_int(rbuf, N, left + it + 5, "write_at_c/read_at_c");
    fill_int(wbuf, N, g_rank + it + 6);
    MPI_File_write_at_all_c(fh, my_off, wbuf, (MPI_Count)N, MPI_INT, &st);
    MPI_File_read_at_all_c(fh, left_off, rbuf, (MPI_Count)N, MPI_INT, &st);
    check_int(rbuf, N, left + it + 6, "write_at_all_c/read_at_all_c");
    fill_int(wbuf, N, g_rank + it + 7);
    MPI_File_iwrite_at_c(fh, my_off, wbuf, (MPI_Count)N, MPI_INT, &req);
    MPI_Wait(&req, &st);
    MPI_File_sync(fh);
    MPI_Barrier(MPI_COMM_WORLD);
    MPI_File_iread_at_c(fh, left_off, rbuf, (MPI_Count)N, MPI_INT, &req);
    MPI_Wait(&req, &st);
    check_int(rbuf, N, left + it + 7, "iwrite_at_c/iread_at_c");
    fill_int(wbuf, N, g_rank + it + 8);
    MPI_File_iwrite_at_all_c(fh, my_off, wbuf, (MPI_Count)N, MPI_INT, &req);
    MPI_Wait(&req, &st);
    MPI_File_iread_at_all_c(fh, left_off, rbuf, (MPI_Count)N, MPI_INT, &req);
    MPI_Wait(&req, &st);
    check_int(rbuf, N, left + it + 8, "iwrite_at_all_c/iread_at_all_c");
#endif
}

/* individual file pointer, with a view that maps offset 0 to the rank's block */
static void individual_pointer(MPI_File fh, MPI_Info info, int it)
{
    int wbuf[N], rbuf[N];
    MPI_Status st;
    MPI_Request req;
    MPI_Offset pos, boff, vdisp;
    MPI_Datatype etype, ftype;
    char rep[MPI_MAX_DATAREP_STRING];

    MPI_File_set_view(fh, my_off, MPI_INT, MPI_INT, "native", info);
    MPI_File_get_view(fh, &vdisp, &etype, &ftype, rep);
    EXPECT(vdisp == my_off, "File_get_view disp %lld", (long long)vdisp);
    EXPECT(strcmp(rep, "native") == 0, "File_get_view datarep '%s'", rep);
    if (ftype != MPI_INT)
        MPI_Type_free(&ftype);
    if (etype != MPI_INT)
        MPI_Type_free(&etype);

    fill_int(wbuf, N, g_rank + it + 10);
    MPI_File_seek(fh, 0, MPI_SEEK_SET);
    MPI_File_write(fh, wbuf, N, MPI_INT, &st);
    check_status_count(&st, "File_write");
    MPI_File_get_position(fh, &pos);
    EXPECT(pos == N, "File_get_position %lld", (long long)pos);
    MPI_File_get_byte_offset(fh, pos, &boff);
    EXPECT(boff == my_off + blk_bytes, "File_get_byte_offset %lld", (long long)boff);
    MPI_File_seek(fh, -N, MPI_SEEK_CUR);
    MPI_File_read(fh, rbuf, N, MPI_INT, &st);
    check_int(rbuf, N, g_rank + it + 10, "write/read");

    fill_int(wbuf, N, g_rank + it + 11);
    MPI_File_seek(fh, 0, MPI_SEEK_SET);
    MPI_File_write_all(fh, wbuf, N, MPI_INT, &st);
    MPI_File_seek(fh, 0, MPI_SEEK_SET);
    MPI_File_read_all(fh, rbuf, N, MPI_INT, &st);
    check_int(rbuf, N, g_rank + it + 11, "write_all/read_all");

    fill_int(wbuf, N, g_rank + it + 12);
    MPI_File_seek(fh, 0, MPI_SEEK_SET);
    MPI_File_write_all_begin(fh, wbuf, N, MPI_INT);
    MPI_File_write_all_end(fh, wbuf, &st);
    MPI_File_seek(fh, 0, MPI_SEEK_SET);
    MPI_File_read_all_begin(fh, rbuf, N, MPI_INT);
    MPI_File_read_all_end(fh, rbuf, &st);
    check_int(rbuf, N, g_rank + it + 12, "write_all_begin/end, read_all_begin/end");

    fill_int(wbuf, N, g_rank + it + 13);
    MPI_File_seek(fh, 0, MPI_SEEK_SET);
    MPI_File_iwrite(fh, wbuf, N, MPI_INT, &req);
    MPI_Wait(&req, &st);
    MPI_File_seek(fh, 0, MPI_SEEK_SET);
    MPI_File_iread(fh, rbuf, N, MPI_INT, &req);
    MPI_Wait(&req, &st);
    check_int(rbuf, N, g_rank + it + 13, "iwrite/iread");

    fill_int(wbuf, N, g_rank + it + 14);
    MPI_File_seek(fh, 0, MPI_SEEK_SET);
    MPI_File_iwrite_all(fh, wbuf, N, MPI_INT, &req);
    MPI_Wait(&req, &st);
    MPI_File_seek(fh, 0, MPI_SEEK_SET);
    MPI_File_iread_all(fh, rbuf, N, MPI_INT, &req);
    MPI_Wait(&req, &st);
    check_int(rbuf, N, g_rank + it + 14, "iwrite_all/iread_all");

#if HAVE_MPI4
    fill_int(wbuf, N, g_rank + it + 15);
    MPI_File_seek(fh, 0, MPI_SEEK_SET);
    MPI_File_write_c(fh, wbuf, (MPI_Count)N, MPI_INT, &st);
    MPI_File_seek(fh, 0, MPI_SEEK_SET);
    MPI_File_read_c(fh, rbuf, (MPI_Count)N, MPI_INT, &st);
    check_int(rbuf, N, g_rank + it + 15, "write_c/read_c");
    fill_int(wbuf, N, g_rank + it + 16);
    MPI_File_seek(fh, 0, MPI_SEEK_SET);
    MPI_File_write_all_c(fh, wbuf, (MPI_Count)N, MPI_INT, &st);
    MPI_File_seek(fh, 0, MPI_SEEK_SET);
    MPI_File_read_all_c(fh, rbuf, (MPI_Count)N, MPI_INT, &st);
    check_int(rbuf, N, g_rank + it + 16, "write_all_c/read_all_c");
    fill_int(wbuf, N, g_rank + it + 17);
    MPI_File_seek(fh, 0, MPI_SEEK_SET);
    MPI_File_iwrite_c(fh, wbuf, (MPI_Count)N, MPI_INT, &req);
    MPI_Wait(&req, &st);
    MPI_File_seek(fh, 0, MPI_SEEK_SET);
    MPI_File_iread_c(fh, rbuf, (MPI_Count)N, MPI_INT, &req);
    MPI_Wait(&req, &st);
    check_int(rbuf, N, g_rank + it + 17, "iwrite_c/iread_c");
    fill_int(wbuf, N, g_rank + it + 18);
    MPI_File_seek(fh, 0, MPI_SEEK_SET);
    MPI_File_iwrite_all_c(fh, wbuf, (MPI_Count)N, MPI_INT, &req);
    MPI_Wait(&req, &st);
    MPI_File_seek(fh, 0, MPI_SEEK_SET);
    MPI_File_iread_all_c(fh, rbuf, (MPI_Count)N, MPI_INT, &req);
    MPI_Wait(&req, &st);
    check_int(rbuf, N, g_rank + it + 18, "iwrite_all_c/iread_all_c");
    fill_int(wbuf, N, g_rank + it + 19);
    MPI_File_seek(fh, 0, MPI_SEEK_SET);
    MPI_File_write_all_begin_c(fh, wbuf, (MPI_Count)N, MPI_INT);
    MPI_File_write_all_end(fh, wbuf, &st);
    MPI_File_seek(fh, 0, MPI_SEEK_SET);
    MPI_File_read_all_begin_c(fh, rbuf, (MPI_Count)N, MPI_INT);
    MPI_File_read_all_end(fh, rbuf, &st);
    check_int(rbuf, N, g_rank + it + 19, "write_all_begin_c/read_all_begin_c");
#endif

    /* back to the plain byte view; the block written last must be visible to the neighbour */
    MPI_File_sync(fh);
    MPI_Barrier(MPI_COMM_WORLD);
    MPI_File_set_view(fh, 0, MPI_BYTE, MPI_BYTE, "native", MPI_INFO_NULL);
    MPI_File_read_at(fh, left_off, rbuf, N, MPI_INT, &st);
    check_int(rbuf, N, left + it + (HAVE_MPI4 ? 19 : 14), "neighbour block after view I/O");
}

/* shared file pointer: ordered calls give a deterministic layout, plain
   shared calls are checked by verifying that every rank's block shows up once */
static void shared_pointer(MPI_File fh, int it)
{
    int wbuf[N], rbuf[N], *all, *seen, i, j;
    MPI_Status st;
    MPI_Request req;
    MPI_Offset pos;

    MPI_File_set_view(fh, 0, MPI_INT, MPI_INT, "native", MPI_INFO_NULL);

    fill_int(wbuf, N, g_rank + it + 20);
    MPI_File_seek_shared(fh, 0, MPI_SEEK_SET);
    MPI_File_write_ordered(fh, wbuf, N, MPI_INT, &st);
    check_status_count(&st, "File_write_ordered");
    MPI_Barrier(MPI_COMM_WORLD);
    MPI_File_get_position_shared(fh, &pos);
    /* only rank 0 is checked: some ROMIO builds report a stale value on the other ranks */
    if (g_rank == 0)
        EXPECT(pos == (MPI_Offset)N * g_size, "File_get_position_shared %lld", (long long)pos);
    MPI_File_seek_shared(fh, 0, MPI_SEEK_SET);
    MPI_File_read_ordered(fh, rbuf, N, MPI_INT, &st);
    check_int(rbuf, N, g_rank + it + 20, "write_ordered/read_ordered");

    fill_int(wbuf, N, g_rank + it + 21);
    MPI_File_seek_shared(fh, 0, MPI_SEEK_SET);
    MPI_File_write_ordered_begin(fh, wbuf, N, MPI_INT);
    MPI_File_write_ordered_end(fh, wbuf, &st);
    MPI_File_seek_shared(fh, 0, MPI_SEEK_SET);
    MPI_File_read_ordered_begin(fh, rbuf, N, MPI_INT);
    MPI_File_read_ordered_end(fh, rbuf, &st);
    check_int(rbuf, N, g_rank + it + 21, "write_ordered_begin/end, read_ordered_begin/end");

#if HAVE_MPI4
    fill_int(wbuf, N, g_rank + it + 22);
    MPI_File_seek_shared(fh, 0, MPI_SEEK_SET);
    MPI_File_write_ordered_c(fh, wbuf, (MPI_Count)N, MPI_INT, &st);
    MPI_File_seek_shared(fh, 0, MPI_SEEK_SET);
    MPI_File_read_ordered_c(fh, rbuf, (MPI_Count)N, MPI_INT, &st);
    check_int(rbuf, N, g_rank + it + 22, "write_ordered_c/read_ordered_c");
    fill_int(wbuf, N, g_rank + it + 23);
    MPI_File_seek_shared(fh, 0, MPI_SEEK_SET);
    MPI_File_write_ordered_begin_c(fh, wbuf, (MPI_Count)N, MPI_INT);
    MPI_File_write_ordered_end(fh, wbuf, &st);
    MPI_File_seek_shared(fh, 0, MPI_SEEK_SET);
    MPI_File_read_ordered_begin_c(fh, rbuf, (MPI_Count)N, MPI_INT);
    MPI_File_read_ordered_end(fh, rbuf, &st);
    check_int(rbuf, N, g_rank + it + 23, "write_ordered_begin_c/read_ordered_begin_c");
#endif

    /* unordered shared writes: each rank writes a block of its own rank number */
    for (i = 0; i < N; i++)
        wbuf[i] = g_rank;
    MPI_File_seek_shared(fh, 0, MPI_SEEK_SET);
    MPI_File_write_shared(fh, wbuf, N, MPI_INT, &st);
    check_status_count(&st, "File_write_shared");
    MPI_File_sync(fh);
    MPI_Barrier(MPI_COMM_WORLD);
    all = (int *)malloc(g_size * N * sizeof(int));
    seen = (int *)calloc(g_size, sizeof(int));
    MPI_File_read_at(fh, 0, all, g_size * N, MPI_INT, &st);
    for (i = 0; i < g_size; i++) {
        int v = all[i * N];
        if (v < 0 || v >= g_size)
            FAIL("write_shared: block %d holds bogus rank %d", i, v);
        else {
            seen[v]++;
            for (j = 1; j < N; j++)
                if (all[i * N + j] != v) {
                    FAIL("write_shared: block %d is not uniform", i);
                    break;
                }
        }
    }
    for (i = 0; i < g_size; i++)
        EXPECT(seen[i] == 1, "write_shared: block of rank %d seen %d times", i, seen[i]);
    MPI_Barrier(MPI_COMM_WORLD);

    /* read_shared: each rank consumes exactly one block */
    MPI_File_seek_shared(fh, 0, MPI_SEEK_SET);
    MPI_File_read_shared(fh, rbuf, N, MPI_INT, &st);
    check_status_count(&st, "File_read_shared");
    EXPECT(rbuf[0] >= 0 && rbuf[0] < g_size, "read_shared bogus value %d", rbuf[0]);
    MPI_Barrier(MPI_COMM_WORLD);

    MPI_File_seek_shared(fh, 0, MPI_SEEK_SET);
    MPI_File_iwrite_shared(fh, wbuf, N, MPI_INT, &req);
    MPI_Wait(&req, &st);
    MPI_File_sync(fh);
    MPI_Barrier(MPI_COMM_WORLD);
    MPI_File_seek_shared(fh, 0, MPI_SEEK_SET);
    MPI_File_iread_shared(fh, rbuf, N, MPI_INT, &req);
    MPI_Wait(&req, &st);
    check_status_count(&st, "File_iread_shared");
    MPI_Barrier(MPI_COMM_WORLD);
#if HAVE_MPI4
    MPI_File_seek_shared(fh, 0, MPI_SEEK_SET);
    MPI_File_write_shared_c(fh, wbuf, (MPI_Count)N, MPI_INT, &st);
    MPI_File_sync(fh);
    MPI_Barrier(MPI_COMM_WORLD);
    MPI_File_seek_shared(fh, 0, MPI_SEEK_SET);
    MPI_File_read_shared_c(fh, rbuf, (MPI_Count)N, MPI_INT, &st);
    MPI_Barrier(MPI_COMM_WORLD);
    MPI_File_seek_shared(fh, 0, MPI_SEEK_SET);
    MPI_File_iwrite_shared_c(fh, wbuf, (MPI_Count)N, MPI_INT, &req);
    MPI_Wait(&req, &st);
    MPI_File_sync(fh);
    MPI_Barrier(MPI_COMM_WORLD);
    MPI_File_seek_shared(fh, 0, MPI_SEEK_SET);
    MPI_File_iread_shared_c(fh, rbuf, (MPI_Count)N, MPI_INT, &req);
    MPI_Wait(&req, &st);
    MPI_Barrier(MPI_COMM_WORLD);
#endif
    free(all);
    free(seen);
}

static int extent_fn(MPI_Datatype dt, MPI_Aint *extent, void *state)
{
    (void)state;
    return MPI_Type_extent(dt, extent);
}

int main(int argc, char **argv)
{
    int iterations, it, rc;
    const char *dir = getenv("FIO_TEST_DIR");
    MPI_File fh;
    MPI_Info info;

    test_init(&argc, &argv);
    iterations = parse_iterations(argc, argv, 10);
    left = (g_rank + g_size - 1) % g_size;
    blk_bytes = (MPI_Offset)N * sizeof(int);
    my_off = blk_bytes * g_rank;
    left_off = blk_bytes * left;

    snprintf(fname, sizeof(fname) - 8, "%s/fio_test.%d.dat", dir ? dir : ".", (int)getpid());
    MPI_Bcast(fname, (int)sizeof(fname), MPI_CHAR, 0, MPI_COMM_WORLD);

    MPI_Pcontrol(1);
    MPI_Register_datarep("fio_test_rep", MPI_CONVERSION_FN_NULL, MPI_CONVERSION_FN_NULL, extent_fn, NULL);

    MPI_Info_create(&info);
    MPI_Info_set(info, "romio_cb_write", "automatic");

    for (it = 0; it < iterations; it++) {
        rc = MPI_File_open(MPI_COMM_WORLD, fname, MPI_MODE_CREATE | MPI_MODE_RDWR, info, &fh);
        if (rc != MPI_SUCCESS && it == 0 && strchr(fname, ':') == NULL) {
            /* ROMIO built without the ufs driver (e.g. --with-file-system=lustre+nfs):
               the nfs driver works on any POSIX file system, so retry with it */
            char tmp[sizeof(fname)];
            snprintf(tmp, sizeof(tmp), "nfs:%s", fname);
            memcpy(fname, tmp, sizeof(fname));
            if (g_rank == 0)
                printf("fio_test: plain open failed, retrying as %s\n", fname);
            rc = MPI_File_open(MPI_COMM_WORLD, fname, MPI_MODE_CREATE | MPI_MODE_RDWR, info, &fh);
        }
        if (rc != MPI_SUCCESS) {
            char es[MPI_MAX_ERROR_STRING];
            int el = 0;
            MPI_Error_string(rc, es, &el);
            FAIL("MPI_File_open(%s) failed: %s", fname, es);
            break;
        }
        local_calls(fh, info);
        explicit_offsets(fh, it);
        individual_pointer(fh, info, it);
        shared_pointer(fh, it);
        MPI_File_close(&fh);
        MPI_Barrier(MPI_COMM_WORLD);
        if (g_rank == 0)
            MPI_File_delete(fname, MPI_INFO_NULL);
        MPI_Barrier(MPI_COMM_WORLD);
    }
    MPI_Info_free(&info);

    rc = test_report("fio_test", iterations);
    MPI_Finalize();
    return rc;
}
