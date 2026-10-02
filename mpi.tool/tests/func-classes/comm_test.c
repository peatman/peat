/* Communicator class test.
   Communicator creation/duplication/splitting, topologies, inter-communicators,
   groups, communicator info/names and the pack/unpack functions (which belong to
   the same tractor function group). Dynamic-process functions (spawn, connect,
   accept, join) are not exercised because they need a launcher with dynamic
   process support.
   Usage: comm_test [iterations]     (any number of ranks) */
#include "test_common.h"

/* small collective sanity check on a freshly created intra-communicator */
static void use_comm(MPI_Comm c, const char *what)
{
    int r, s, sum = 0, one = 1;
    MPI_Comm_rank(c, &r);
    MPI_Comm_size(c, &s);
    MPI_Allreduce(&one, &sum, 1, MPI_INT, MPI_SUM, c);
    EXPECT(sum == s, "%s: allreduce over new comm gave %d, expected %d", what, sum, s);
    MPI_Barrier(c);
}

static void dup_split(int it)
{
    MPI_Comm dup, dup2, idup, split, shared;
    MPI_Info info;
    MPI_Request req;
    int cmp;
    char name[MPI_MAX_OBJECT_NAME];
    int len;

    MPI_Comm_dup(MPI_COMM_WORLD, &dup);
    MPI_Comm_compare(MPI_COMM_WORLD, dup, &cmp);
    EXPECT(cmp == MPI_CONGRUENT, "dup is not congruent (%d)", cmp);
    snprintf(name, sizeof(name), "dup_%d", it);
    MPI_Comm_set_name(dup, name);
    MPI_Comm_get_name(dup, name, &len);
    EXPECT(len > 0 && strncmp(name, "dup_", 4) == 0, "Comm_get_name returned '%s'", name);
    use_comm(dup, "Comm_dup");

    MPI_Info_create(&info);
    MPI_Info_set(info, "mpi_assert_no_any_tag", "true");
    MPI_Comm_dup_with_info(MPI_COMM_WORLD, info, &dup2);
    MPI_Comm_set_info(dup2, info);
    {
        MPI_Info got;
        MPI_Comm_get_info(dup2, &got);
        MPI_Info_free(&got);
    }
    use_comm(dup2, "Comm_dup_with_info");

    MPI_Comm_idup(dup, &idup, &req);
    MPI_Wait(&req, MPI_STATUS_IGNORE);
    use_comm(idup, "Comm_idup");
    MPI_Comm_free(&idup);
#if HAVE_MPI4
    MPI_Comm_idup_with_info(dup, info, &idup, &req);
    MPI_Wait(&req, MPI_STATUS_IGNORE);
    use_comm(idup, "Comm_idup_with_info");
    MPI_Comm_free(&idup);
#endif
    MPI_Info_free(&info);

    MPI_Comm_split(MPI_COMM_WORLD, g_rank % 2, g_size - g_rank, &split);
    {
        int r, s;
        MPI_Comm_rank(split, &r);
        MPI_Comm_size(split, &s);
        EXPECT(s == g_size / 2 + ((g_size % 2) && (g_rank % 2 == 0) ? 1 : 0),
               "Comm_split size %d", s);
        EXPECT(r == (s - 1) - g_rank / 2, "Comm_split key ordering: rank %d", r);
    }
    use_comm(split, "Comm_split");
    MPI_Comm_free(&split);

    MPI_Comm_split(MPI_COMM_WORLD, g_rank == 0 ? MPI_UNDEFINED : 1, g_rank, &split);
    if (g_rank == 0)
        EXPECT(split == MPI_COMM_NULL, "Comm_split with MPI_UNDEFINED returned a comm");
    else {
        use_comm(split, "Comm_split(subset)");
        MPI_Comm_free(&split);
    }

    MPI_Comm_split_type(MPI_COMM_WORLD, MPI_COMM_TYPE_SHARED, g_rank, MPI_INFO_NULL, &shared);
    use_comm(shared, "Comm_split_type");
    MPI_Comm_free(&shared);

    MPI_Comm_free(&dup2);
    MPI_Comm_free(&dup);
}

static void groups(int it)
{
    MPI_Group world, sub, rev;
    MPI_Comm created, grp_comm;
    int *ranks, i, ranges[1][3], gsize, grank;

    ranks = (int *)malloc(g_size * sizeof(int));
    for (i = 0; i < g_size; i++)
        ranks[i] = g_size - 1 - i; /* reversed order */

    MPI_Comm_group(MPI_COMM_WORLD, &world);
    MPI_Group_incl(world, g_size, ranks, &rev);
    MPI_Comm_create(MPI_COMM_WORLD, rev, &created);
    MPI_Comm_rank(created, &grank);
    EXPECT(grank == g_size - 1 - g_rank, "Comm_create rank order: %d", grank);
    use_comm(created, "Comm_create");
    MPI_Comm_free(&created);

    /* only the upper half creates a comm via Comm_create_group */
    ranges[0][0] = g_size / 2;
    ranges[0][1] = g_size - 1;
    ranges[0][2] = 1;
    MPI_Group_range_incl(world, 1, ranges, &sub);
    MPI_Group_size(sub, &gsize);
    if (g_rank >= g_size / 2) {
        MPI_Comm_create_group(MPI_COMM_WORLD, sub, 1000 + it, &grp_comm);
        {
            int s;
            MPI_Comm_size(grp_comm, &s);
            EXPECT(s == gsize, "Comm_create_group size %d != %d", s, gsize);
        }
        use_comm(grp_comm, "Comm_create_group");
        MPI_Comm_free(&grp_comm);
    }
#if HAVE_MPI4
    {
        char tag[64];
        snprintf(tag, sizeof(tag), "comm_test.create_from_group.%d", it);
        MPI_Comm_create_from_group(rev, tag, MPI_INFO_NULL, MPI_ERRORS_RETURN, &grp_comm);
        use_comm(grp_comm, "Comm_create_from_group");
        MPI_Comm_free(&grp_comm);
    }
#endif
    MPI_Group_free(&sub);
    MPI_Group_free(&rev);
    MPI_Group_free(&world);
    free(ranks);
}

static void topologies(int it)
{
    MPI_Comm cart, sub, graph, dgraph, dgraph2;
    int dims[2] = { 0, 0 }, periods[2] = { 1, 1 }, remain[2] = { 1, 0 };
    int coords[2], src, dst, ndims;
    int *index, *edges, i;
    int left = (g_rank + g_size - 1) % g_size, right = (g_rank + 1) % g_size;
    int sources[1], degrees[1], destinations[2], weights[2] = { 1, 1 };
    int in_nb[2], out_nb[2], indeg, outdeg, wtd;

    MPI_Dims_create(g_size, 2, dims);
    MPI_Cart_create(MPI_COMM_WORLD, 2, dims, periods, it % 2, &cart);
    MPI_Cartdim_get(cart, &ndims);
    EXPECT(ndims == 2, "Cartdim_get %d", ndims);
    {
        int crank;
        MPI_Comm_rank(cart, &crank);
        MPI_Cart_coords(cart, crank, 2, coords);
        MPI_Cart_shift(cart, 0, 1, &src, &dst);
    }
    use_comm(cart, "Cart_create");
    MPI_Cart_sub(cart, remain, &sub);
    {
        int s;
        MPI_Comm_size(sub, &s);
        EXPECT(s == dims[0], "Cart_sub size %d != %d", s, dims[0]);
    }
    use_comm(sub, "Cart_sub");
    MPI_Comm_free(&sub);
    MPI_Comm_free(&cart);

    /* classic graph: ring */
    index = (int *)malloc(g_size * sizeof(int));
    edges = (int *)malloc(2 * g_size * sizeof(int));
    for (i = 0; i < g_size; i++) {
        index[i] = 2 * (i + 1);
        edges[2 * i] = (i + g_size - 1) % g_size;
        edges[2 * i + 1] = (i + 1) % g_size;
    }
    MPI_Graph_create(MPI_COMM_WORLD, g_size, index, edges, 0, &graph);
    use_comm(graph, "Graph_create");
    MPI_Comm_free(&graph);
    free(index);
    free(edges);

    /* distributed graph: ring, adjacent form and general form */
    in_nb[0] = left;
    in_nb[1] = right;
    out_nb[0] = right;
    out_nb[1] = left;
    MPI_Dist_graph_create_adjacent(MPI_COMM_WORLD, 2, in_nb, MPI_UNWEIGHTED, 2, out_nb, MPI_UNWEIGHTED,
                                   MPI_INFO_NULL, 0, &dgraph);
    MPI_Dist_graph_neighbors_count(dgraph, &indeg, &outdeg, &wtd);
    EXPECT(indeg == 2 && outdeg == 2, "Dist_graph_neighbors_count %d/%d", indeg, outdeg);
    use_comm(dgraph, "Dist_graph_create_adjacent");
    MPI_Comm_free(&dgraph);

    sources[0] = g_rank;
    degrees[0] = 2;
    destinations[0] = right;
    destinations[1] = left;
    MPI_Dist_graph_create(MPI_COMM_WORLD, 1, sources, degrees, destinations, weights, MPI_INFO_NULL, 1, &dgraph2);
    use_comm(dgraph2, "Dist_graph_create");
    MPI_Comm_free(&dgraph2);
}

static void intercomms(int it)
{
    MPI_Comm half, inter, merged;
    int color = g_rank < g_size / 2 ? 0 : 1;
    int remote_leader = color == 0 ? g_size / 2 : 0;
    int rsize, is_inter, expect_remote;

    if (g_size < 2)
        return;

    MPI_Comm_split(MPI_COMM_WORLD, color, g_rank, &half);
    MPI_Intercomm_create(half, 0, MPI_COMM_WORLD, remote_leader, 2000 + it, &inter);
    MPI_Comm_test_inter(inter, &is_inter);
    EXPECT(is_inter, "Intercomm_create did not yield an inter-communicator");
    MPI_Comm_remote_size(inter, &rsize);
    expect_remote = color == 0 ? g_size - g_size / 2 : g_size / 2;
    EXPECT(rsize == expect_remote, "Intercomm remote size %d != %d", rsize, expect_remote);
    {
        /* one exchange across the inter-communicator */
        int sval = 1234 + it, rval = -1;
        int root = color == 0 ? (g_rank == 0 ? MPI_ROOT : MPI_PROC_NULL) : 0;
        MPI_Bcast(color == 0 ? &sval : &rval, 1, MPI_INT, root, inter);
        if (color == 1)
            EXPECT(rval == 1234 + it, "Bcast over inter-communicator got %d", rval);
    }
    MPI_Intercomm_merge(inter, color, &merged);
    {
        int s;
        MPI_Comm_size(merged, &s);
        EXPECT(s == g_size, "Intercomm_merge size %d", s);
    }
    use_comm(merged, "Intercomm_merge");
    MPI_Comm_free(&merged);
    MPI_Comm_disconnect(&inter);

#if HAVE_MPI4
    {
        MPI_Group world, local, remote;
        int lo = color == 0 ? 0 : g_size / 2;
        int hi = color == 0 ? g_size / 2 - 1 : g_size - 1;
        int rlo = color == 0 ? g_size / 2 : 0;
        int rhi = color == 0 ? g_size - 1 : g_size / 2 - 1;
        int lrange[1][3], rrange[1][3];
        char tag[64];

        lrange[0][0] = lo; lrange[0][1] = hi; lrange[0][2] = 1;
        rrange[0][0] = rlo; rrange[0][1] = rhi; rrange[0][2] = 1;
        MPI_Comm_group(MPI_COMM_WORLD, &world);
        MPI_Group_range_incl(world, 1, lrange, &local);
        MPI_Group_range_incl(world, 1, rrange, &remote);
        snprintf(tag, sizeof(tag), "comm_test.intercomm_from_groups.%d", it);
        MPI_Intercomm_create_from_groups(local, 0, remote, 0, tag, MPI_INFO_NULL, MPI_ERRORS_RETURN, &inter);
        MPI_Comm_remote_size(inter, &rsize);
        EXPECT(rsize == expect_remote, "Intercomm_create_from_groups remote size %d", rsize);
        MPI_Comm_free(&inter);
        MPI_Group_free(&remote);
        MPI_Group_free(&local);
        MPI_Group_free(&world);
    }
#endif
    MPI_Comm_free(&half);
}

static void packing(int it)
{
    int ints[8], outs[8], i, pos, size, size2;
    double dbls[4], outd[4];
    char *buf;
    MPI_Aint apos, asize;

    fill_int(ints, 8, g_rank + it);
    for (i = 0; i < 4; i++)
        dbls[i] = g_rank * 1.5 + i;

    MPI_Pack_size(8, MPI_INT, MPI_COMM_WORLD, &size);
    MPI_Pack_size(4, MPI_DOUBLE, MPI_COMM_WORLD, &size2);
    buf = (char *)malloc(size + size2);
    pos = 0;
    MPI_Pack(ints, 8, MPI_INT, buf, size + size2, &pos, MPI_COMM_WORLD);
    MPI_Pack(dbls, 4, MPI_DOUBLE, buf, size + size2, &pos, MPI_COMM_WORLD);
    EXPECT(pos <= size + size2, "Pack overflowed (%d > %d)", pos, size + size2);
    pos = 0;
    MPI_Unpack(buf, size + size2, &pos, outs, 8, MPI_INT, MPI_COMM_WORLD);
    MPI_Unpack(buf, size + size2, &pos, outd, 4, MPI_DOUBLE, MPI_COMM_WORLD);
    check_int(outs, 8, g_rank + it, "Pack/Unpack");
    for (i = 0; i < 4; i++)
        EXPECT(outd[i] == dbls[i], "Unpack double %d wrong", i);
    free(buf);

    MPI_Pack_external_size("external32", 8, MPI_INT, &asize);
    buf = (char *)malloc(asize);
    apos = 0;
    MPI_Pack_external("external32", ints, 8, MPI_INT, buf, asize, &apos);
    apos = 0;
    MPI_Unpack_external("external32", buf, asize, &apos, outs, 8, MPI_INT);
    check_int(outs, 8, g_rank + it, "Pack_external/Unpack_external");
    free(buf);

#if HAVE_MPI4
    {
        MPI_Count csize, cpos;
        MPI_Pack_size_c(8, MPI_INT, MPI_COMM_WORLD, &csize);
        buf = (char *)malloc((size_t)csize);
        cpos = 0;
        MPI_Pack_c(ints, 8, MPI_INT, buf, csize, &cpos, MPI_COMM_WORLD);
        cpos = 0;
        MPI_Unpack_c(buf, csize, &cpos, outs, 8, MPI_INT, MPI_COMM_WORLD);
        check_int(outs, 8, g_rank + it, "Pack_c/Unpack_c");
        free(buf);

        MPI_Pack_external_size_c("external32", 8, MPI_INT, &csize);
        buf = (char *)malloc((size_t)csize);
        cpos = 0;
        MPI_Pack_external_c("external32", ints, 8, MPI_INT, buf, csize, &cpos);
        cpos = 0;
        MPI_Unpack_external_c("external32", buf, csize, &cpos, outs, 8, MPI_INT);
        check_int(outs, 8, g_rank + it, "Pack_external_c/Unpack_external_c");
        free(buf);
    }
#endif
}

int main(int argc, char **argv)
{
    int iterations, it;

    test_init(&argc, &argv);
    iterations = parse_iterations(argc, argv, 10);

    for (it = 0; it < iterations; it++) {
        dup_split(it);
        groups(it);
        topologies(it);
        intercomms(it);
        packing(it);
    }

    {
        int rc = test_report("comm_test", iterations);
        MPI_Finalize();
        return rc;
    }
}
