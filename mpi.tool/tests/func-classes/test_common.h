/* Shared helpers for the per-class MPI function tests (p2p, coll, comm, rma, fio).
   Every test takes the iteration count as its first argument. */
#ifndef TEST_COMMON_H
#define TEST_COMMON_H

#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* MPI-4 features (large-count _c variants, persistent collectives, partitioned
   communication, ...). Build with -DNO_MPI4 to leave them out. */
#if defined(MPI_VERSION) && MPI_VERSION >= 4 && !defined(NO_MPI4)
#define HAVE_MPI4 1
#else
#define HAVE_MPI4 0
#endif

static int g_rank = -1;
static int g_size = 0;
static int g_errors = 0;

#define FAIL(...)                                                       \
    do {                                                                \
        fprintf(stderr, "[%d] %s:%d: ", g_rank, __FILE__, __LINE__);    \
        fprintf(stderr, __VA_ARGS__);                                   \
        fputc('\n', stderr);                                            \
        g_errors++;                                                     \
    } while (0)

#define EXPECT(cond, ...)                                               \
    do {                                                                \
        if (!(cond))                                                    \
            FAIL(__VA_ARGS__);                                          \
    } while (0)

/* Call an MPI function and count a failure if it does not return MPI_SUCCESS
   (only reached when the error handler is MPI_ERRORS_RETURN). */
#define CHECK(call)                                                     \
    do {                                                                \
        int _e = (call);                                                \
        if (_e != MPI_SUCCESS) {                                        \
            char _s[MPI_MAX_ERROR_STRING];                              \
            int _l = 0;                                                 \
            MPI_Error_string(_e, _s, &_l);                              \
            FAIL("%s -> %s", #call, _s);                                \
        }                                                               \
    } while (0)

static void test_init(int *argc, char ***argv)
{
    MPI_Init(argc, argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &g_rank);
    MPI_Comm_size(MPI_COMM_WORLD, &g_size);
}

static int parse_iterations(int argc, char **argv, int def)
{
    int it = def;
    if (argc > 1) {
        it = atoi(argv[1]);
        if (it <= 0) {
            if (g_rank == 0)
                fprintf(stderr, "usage: %s [iterations]\n", argv[0]);
            it = def;
        }
    }
    return it;
}

static void fill_int(int *b, int n, int base)
{
    int i;
    for (i = 0; i < n; i++)
        b[i] = base + i;
}

static void check_int(const int *b, int n, int base, const char *what)
{
    int i, bad = 0, first = -1;
    for (i = 0; i < n; i++)
        if (b[i] != base + i) {
            if (first < 0)
                first = i;
            bad++;
        }
    if (bad)
        FAIL("%s: %d of %d values wrong (first at %d: %d != %d)", what, bad, n,
             first, b[first], base + first);
}

/* Sum up the error counts, print the verdict on rank 0 and return the exit code. */
static int test_report(const char *name, int iterations)
{
    int total = 0;
    MPI_Allreduce(&g_errors, &total, 1, MPI_INT, MPI_SUM, MPI_COMM_WORLD);
    if (g_rank == 0)
        printf("%s: %d ranks, %d iterations, %s (%d errors)\n", name, g_size,
               iterations, total ? "FAILED" : "PASSED", total);
    return total ? 1 : 0;
}

#endif
