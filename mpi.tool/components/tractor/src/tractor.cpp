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

#include "hwloc.h"
#include "mpi.h"
#include <assert.h>
#include <dlfcn.h>
#include <libgen.h>
#include <limits.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>
#ifdef GPUPARSE
#include "level_zero/ze_api.h"
#include <CL/cl.h>
#endif
#include "tractor.h"

#ifdef _USE_SIGNALS_
#include "t_signals.h"
#endif

#if !defined(_INTELCOMPILER_)
#include <cpuid.h>
#endif


pid_t *t_base_event::tid2t_tid = NULL;

#if defined(__CPLUSPLUS__) || defined(__cplusplus)
extern "C" {
#endif
#if (defined(_TRACTOR_PASSTHROUGH_) && defined(_TRACTOR_VIRT2PHYS_))
#error "PTHREAD and VIRT2PHYS  are not available without trace collection"
#endif

#ifdef _TRACTOR_DIAG_
FILE *debug_fp;
#endif
#ifdef _T_OMPT_
extern ompt_finalize_tool_t ompt_finalize_tool;
#endif

int gettimeofday_counter = 0;
int clock_gettime_counter = 0;
MPI_Request (*MPI_Check_dummy_prev)(MPI_Request request) = 0;
long long cpu_proc_freq[2]={0,0};

static int MPI_Check_dummy_failed = 0;
MPI_Request MPI_Check_dummy(MPI_Request request)
{
    if (!MPI_Check_dummy_prev == NULL && !MPI_Check_dummy_failed) {
        MPI_Check_dummy_prev = (MPI_Request (*)(MPI_Request request))(dlsym(RTLD_NEXT, "MPI_Check_dummy"));
        if (MPI_Check_dummy_prev == NULL)
            MPI_Check_dummy_failed = 1;
    }
    if (MPI_Check_dummy_prev) {
        return MPI_Check_dummy_prev(request);
    }

    return request;
}

#include "mpi_func_ids_trans.h"

#ifdef _TRACTOR_COLLECT_STAT_
PEAT_Clock overall_tool_overhead[MPI_MAX_FUNCID_THROUGH];
PEAT_Clock overall_durations[MPI_MAX_FUNCID_THROUGH];
PEAT_Clock overall_entries[MPI_MAX_FUNCID_THROUGH];
static int print_all = 0;
#include "mpi_func_names.h"
#endif
char counting_permission[MPI_MAX_FUNCID_THROUGH];
char fullstack_permission[MPI_MAX_FUNCID_THROUGH];
char counting_permission_pcontrol[MPI_MAX_FUNCID_THROUGH];
bool tractor_finished = false;
long onetimealloc;
int t_mpi_rank = -1;
long long bytes_allocated;
int counting_permit = 0;

#define SEPARATOR(width)                      \
    gzprintf(fps_com, "%d:", PEAT_RT_Remark); \
    for (int i = 0; i < width; ++i) {         \
        gzprintf(fps_com, "%c", '-');         \
    }                                         \
    gzprintf(fps_com, "\n")

#define t_gzprintf          \
    if (t_write_main_files) \
    gzprintf

char *__msl_getenv(char *envvar)
{
    __DEBUG_PROLOG_TRACTOR_LAYER__
    char *tmp = getenv(envvar);
    if (tmp) {
        debug_printf("Getting %s...%s.\n", envvar, tmp);
    } else {
        debug_printf("Unable to get %s.\n", envvar);
    }
    __DEBUG_EPILOG_TRACTOR_LAYER__
    return tmp;
}

void so_ident(void)
{
    debug_printf("This Routine should never be called\n");
    return;
}

int parse_event_mask(char *events, char c_p[MPI_MAX_FUNCID_THROUGH])
{
    char *temp1, *temp2;
    char temp_fn[256];
    int i = 0;
    __DEBUG_PROLOG_TRACTOR_LAYER__
    temp1 = events;
    while ((temp2 = strchr(temp1, ',')) != NULL) {
        memcpy(temp_fn, temp1, temp2 - temp1);
        temp_fn[temp2 - temp1] = 0;
        // strupr(temp_fn);
        for (int f = MPI_MIN_FUNCID_THROUGH + 1; f < MPI_MAX_FUNCID_THROUGH; f++) {
            if (strcasecmp(mpi_function_names[f], temp_fn) == 0) {
                c_p[f] = !(c_p[f] > 0);
                i++;
            }
        }
        temp1 = temp2 + 1;
    };

    if (strlen(temp1) > 0) {
        strcpy(temp_fn, temp1);
        // strupr(temp_fn);
        for (int f = MPI_MIN_FUNCID_THROUGH + 1; f < MPI_MAX_FUNCID_THROUGH; f++) {
            if (strcasecmp(mpi_function_names[f], temp_fn) == 0) {
                c_p[f] = !(c_p[f] > 0);
                i++;
            }
        }
    }
    __DEBUG_EPILOG_TRACTOR_LAYER__
    return i;
}

int level0 = 0;
extern int mpi_skip_parse;
extern int gpu_skip_parse;

tractor::tractor(void)
{
    __DEBUG_PROLOG_TRACTOR_LAYER__
    debug_printf("Tractor is started\n");
    tractor_initialized = 0;
    stat_template = "tractor_t";
    stat_dir = ".";
    tmp_dir = ".";
    mpi_skip_parse = 1; // no mpi tracing until MPI_Init or MPI_Init_thread
    gpu_skip_parse = 0; // gpu tracing is always allowed
    print_names = 0;
    t_mpi_rank = -1;
    first_par_tsc = 0;
    last_par_tsc = 0;
    last_tsc = 0;
    max_ret_level = 0;
    full_ompt_unwind = 0;
    abort_sleep = 300;
    resolve_comms_and_ranks = 1;
    highval = (void *)0x7FFFFFFFFFFFFFFF;
    binval = (void *)0x7FFFFFFFFFFFFFFF;
    lowval = (void *)0;
    highfort = (void *)0;
    lowfort = (void *)0x7FFFFFFFFFFFFFFF;
#ifdef _OPENMPI_
    num_fort_ranges = 0;
#endif
    hightrac = (void *)0;
    lowtrac = (void *)0x7FFFFFFFFFFFFFFF;
    highheap = (void *)0;
    lowheap = (void *)0x7FFFFFFFFFFFFFFF;
    highstack = (void *)0;
    lowstack = (void *)0x7FFFFFFFFFFFFFFF;
    t_write_main_files = 1;
    t_use_signals = 0;
    uniq_index = NULL;
#ifdef _T_OMPT_
    parse_openmp = 1;
#endif
    user_region_quality_range = 0.75;
    fps_mpi = fps_hwc = fps_com = NULL;
    gz_ext = ".gz";
    compression = 1;
    compression_level = Z_DEFAULT_COMPRESSION;
    fps_events = fps_extras = NULL;

#ifdef _TRACTOR_VIRT2PHYS_
    pmfile = 0;
    pffile = 0;
#endif

    rank_smp = -1, size_smp = -1;
    rank_master = -1, size_master = -1;
    rank_cw = -1, size_cw = -1;
    comm_master = MPI_COMM_NULL;
    comm_smp = MPI_COMM_NULL;
    grp_world = MPI_GROUP_NULL;
    grp_smp = MPI_GROUP_NULL;
    grp_master = MPI_GROUP_NULL;
    local_comms = NULL;
    start_sync = stop_sync = NULL;
#ifdef USE_NANO_TIMER
    char *temp = __msl_getenv("PEAT_TRACTOR_SYSTIMER");
    if (temp)
        default_timer = (clockid_t)atoi(temp);
    peat_init_nano_sec();
    rdtsc_init();
    first_tsc = peat_get_nano_sec(); // no freq ratio yet
#else
    rdtsc_init();
    first_tsc = rdtsca();
#endif
#ifdef _TRACTOR_USE_PTPD_
    ptpd_ui = 0;
    use_ptpd = 0;
#endif

    store_map = 0;

#ifdef _TRANSPORT_PROFILE_
    t_permit_collect_transport = 0;
#endif

    onetimealloc = defonetimealloc;
    pcontrol = 0;
    overall_permit = 0;
    num_addrs = 1;
    ret_levels = &level0;
    write_on_finalize = 0;
    parse_env_vars();
    /* allocate (but don't init()) before PMPI_Init runs: MPICH can create internal sub-comms
       of MPI_COMM_WORLD via our mpi_internal_comm_create hook while still inside PMPI_Init */
    if (t_mpi_rank >= 0)
        local_comms = new t_local_comms;
#ifdef _HWCOUNTERS_ENABLED_
    num_hwcs = hwc.get_num_counters();
#endif
    map_file_parse();
    debug_printf("HIGHLOWBIN parsed values: %llx %llx %llx\n", binval, lowval, highval);
    debug_printf("HIGHLOWBIN fortran parsed values: %llx %llx\n", lowfort, highfort);
    debug_printf("HIGHLOWBIN Tractor parsed values: %llx %llx\n", lowtrac, hightrac);
    debug_printf("HIGHLOWBIN HEAP parsed values: %llx %llx\n", lowheap, highheap);
    debug_printf("HIGHLOWBIN STACK parsed values: %llx %llx\n", lowstack, highstack);
#ifdef _MEM_HOOKS_
    mprof.set_add_vars(lowheap, highheap, lowstack, highstack);
#endif
    //    if (t_mpi_rank<0) {
    init();
    //    }
    __DEBUG_EPILOG_TRACTOR_LAYER__
    // begin();
};

tractor::~tractor(void)
{
    __DEBUG_PROLOG_TRACTOR_LAYER__
    if (!write_on_finalize)
        tractor::end();
#ifdef _USE_SIGNALS_
    //  end of protecttion
    if (t_use_signals)
        restore_signals(t_mpi_rank);
#endif
    __DEBUG_EPILOG_TRACTOR_LAYER__
}

static int is_true(char *str, int previous_value)
{
    if (str == NULL)
        return previous_value;
    if (strcmp(str, "1") == 0)
        return 1;
    if (strcmp(str, "yes") == 0)
        return 1;
    if (strcmp(str, "on") == 0)
        return 1;
    return 0;
}
static int is_false(char *str, int previous_value)
{
    if (str == NULL)
        return previous_value;
    if (strcmp(str, "0") == 0)
        return 1;
    if (strcmp(str, "no") == 0)
        return 1;
    if (strcmp(str, "off") == 0)
        return 1;
    return 0;
}

#define FDELAY 1
static void calc_freq(void)
{

    char *freq_template = getenv("PEAT_TRACTOR_FREQUENCY");
    if (freq_template) {
        cpu_proc_freq[0] = atoll(freq_template); /* in Hz */
    } else {
        PEAT_Clock fc, sc;
        sc = rdtsc();
        sleep(FDELAY);
        fc = rdtsc();
        cpu_proc_freq[0] = (long long)((double)(fc - sc) / ((double)FDELAY)); /* in Hz */
    }
}

static double calc_ratio(void)
{
#ifdef USE_NANO_TIMER
    PEAT_Clock fc, sc;
    PEAT_Clock ft, st;
    double ratio;
    sc = rdtsc();
    st = peat_get_nano_sec();
    sc = (rdtsc() + sc) / 2;
    sleep(FDELAY);
    fc = rdtsc();
    ft = peat_get_nano_sec();
    fc = (rdtsc() + fc) / 2;
    ratio = (double)(fc - sc) / (double)(ft - st);
    return ratio;
#else
    return 1.0;
#endif
}

#ifdef _TRACTOR_EXTRA_FIELDS_
void tractor::print_address(gzFile fp, addr_entry *entry)
{
#ifdef _TRACTOR_VIRT2PHYS_
    t_gzprintf(fp, ":%llx:%lld:%llx:%llx",
               entry->virt_addr,
               entry->size,
               entry->phys_entry,
               entry->flags);
#else
    t_gzprintf(fp, ":%llx:%lld",
               entry->virt_addr,
               entry->size);
#endif
}
#endif

#if 1
long long g_bounds[] = {0, 16, 64, 256, 16384, 262144, LLONG_MAX};

struct func_stat {
    long long *bounds;
    int bound_num, dynamic_alloc;
    long long *raw_buffer;
    struct mpi_statistic_event {
        long long *durations;
        long long *sizes;
        long long *occurences;
        mpi_statistic_event() : durations(NULL), sizes(NULL), occurences(NULL) {};
    };
    func_stat(void)
    {
        dynamic_alloc = 0;
        bound_num = sizeof(g_bounds) / sizeof(g_bounds[0]);
        bounds = g_bounds;
        allocate();
    };
    void allocate(void)
    {
        raw_buffer = (long long *)t_malloc((MPI_MAX_FUNCID_THROUGH + 1) * 3 * bound_num * sizeof(long long), 128);
        memset(raw_buffer, 0, (MPI_MAX_FUNCID_THROUGH + 1) * 3 * bound_num * sizeof(long long));
        for (int i = 0; i < MPI_MAX_FUNCID_THROUGH; i++) {
            stat_for_func[i].durations = &raw_buffer[i * 3 * bound_num];
            stat_for_func[i].sizes = &raw_buffer[i * 3 * bound_num + bound_num];
            stat_for_func[i].occurences = &raw_buffer[i * 3 * bound_num + bound_num * 2];
        }
    };
    void reset(int new_bound_num, long long *new_bounds)
    {
        t_free(raw_buffer, (MPI_MAX_FUNCID_THROUGH + 1) * 3 * bound_num * sizeof(long long));
        if (dynamic_alloc) {
            t_free(bounds, (bound_num) * sizeof(long long));
        }
        bound_num = new_bound_num;
        bounds = (long long *)t_malloc((bound_num) * sizeof(long long), 128);
        memcpy(bounds, new_bounds, (bound_num) * sizeof(long long));
        for (int i = 0; i < bound_num; i++)
            debug_printf("Bound %d=%lld\n", bound_num, bounds);
        dynamic_alloc = 1;
        allocate();
    };
    ~func_stat(void)
    {
        t_free(raw_buffer, (MPI_MAX_FUNCID_THROUGH + 1) * 3 * bound_num * sizeof(long long));
        if (dynamic_alloc) {
            t_free(bounds, (bound_num) * sizeof(long long));
        }
    };
    mpi_statistic_event stat_for_func[MPI_MAX_FUNCID_THROUGH];

    int get_group(long long num)
    {
        for (int i = 1; i < bound_num; i++)
            if (bounds[i - 1] <= num && num < bounds[i])
                return i - 1;
        return bound_num - 1;
    };
} gt_func_stat;

#endif

int tractor::parse_boundaries(char *boundaries)
{
    __DEBUG_PROLOG_TRACTOR_LAYER__
    int len = strlen(boundaries);
    char t_string[len];
    int t_o_bounds;
    long long t_bounds[len];
    char *endptr;
    int comma, start;
    t_bounds[0] = 0;
    for (comma = 0, start = 0, t_o_bounds = 1; comma <= len; comma++) {
        if (boundaries[comma] == ',' || boundaries[comma] == 0) {
            memcpy(t_string, &boundaries[start], comma - start + 1);
            t_string[comma - start + 1] = 0;
            start = comma + 1;
            long long bound = strtoll(t_string, &endptr, 10);
            if (bound > 0) {
                t_bounds[t_o_bounds] =
                    t_o_bounds++;
            }
        }
    }
    gt_func_stat.reset(t_o_bounds, t_bounds);
    __DEBUG_EPILOG_TRACTOR_LAYER__
    return t_o_bounds;
}

void tractor::parse_pc_levels(char *levels)
{
    __DEBUG_PROLOG_TRACTOR_LAYER__
    int len = strlen(levels);
    char t_string[len];
    char *endptr;
    int comma, start;
    for (comma = 0, start = 0; comma <= len; comma++) {
        if (levels[comma] == ',' || levels[comma] == 0) {
            memcpy(t_string, &levels[start], comma - start + 1);
            t_string[comma - start + 1] = 0;
            start = comma + 1;
            int level = (int)strtol(t_string, &endptr, 10);
            pc_levels[level] = 1;
        }
    }
    __DEBUG_EPILOG_TRACTOR_LAYER__
    return;
}

int tractor::parse_levels(char *levels)
{
    __DEBUG_PROLOG_TRACTOR_LAYER__
    int len = strlen(levels);
    char t_string[len];
    char *endptr;
    int comma, start;
    int t_levels[len + 1];
    int prev_level = -1, level = 0;
    for (comma = 0, start = 0, t_o_levels = 0; comma <= len; comma++) {
        if (levels[comma] == ',' || levels[comma] == 0) {
            memcpy(t_string, &levels[start], comma - start + 1);
            t_string[comma - start + 1] = 0;
            start = comma + 1;
            level = strtol(t_string, &endptr, 10);
            if (level > prev_level) {
                t_levels[t_o_levels] = prev_level = level;
                t_o_levels++;
            }
        }
    }
    max_ret_level = level; // last level specified
    ret_levels = (int *)t_malloc(t_o_levels * sizeof(int), 128);
    debug_printf("%d:", t_o_levels);
    for (int i = 0; i < t_o_levels; i++) {
        ret_levels[i] = t_levels[i];
        debug_printf(" %d", ret_levels[i]);
    }
    debug_printf("\n");

    __DEBUG_EPILOG_TRACTOR_LAYER__
    return t_o_levels;
}

#ifdef _TRACTOR_VIRT2PHYS_
void tractor::translate_virt2phys(msl_event *event)
{
    const short func_base = mpi_func_ids_trans[event->func_id];
    __DEBUG_PROLOG_TRACTOR_LAYER__
    // walk through all possible pointers and translate their addresses
    if (func_base >= MPI_START_FILE_GROUP && func_base < MPI_END_FILE_GROUP) {
        if (event->mpi_fio.buf.virt_addr)
            __msl_virt2phys(pmfile, pffile, &event->mpi_fio.buf);
    }

    if (func_base >= MPI_START_BUFFERED_LOCAL_GROUP && func_base < MPI_END_BUFFERED_LOCAL_GROUP) {
        if (event->buf_local.buf[0].virt_addr)
            __msl_virt2phys(pmfile, pffile, &event->collective.buf[0]);
        if (event->buf_local.buf[1].virt_addr)
            __msl_virt2phys(pmfile, pffile, &event->collective.buf[1]);
    }

    if (func_base >= MPI_START_GENERAL_COLLECTIVE_GROUP && func_base < MPI_END_GENERAL_COLLECTIVE_GROUP) {
        if (event->collective.buf[0].virt_addr)
            __msl_virt2phys(pmfile, pffile, &event->collective.buf[0]);
        if (event->collective.buf[1].virt_addr)
            __msl_virt2phys(pmfile, pffile, &event->collective.buf[1]);
    }

    if (func_base >= MPI_P2P_TRANSFER_START && func_base < MPI_P2P_TRANSFER_END ||
        func_base >= MPI_P2P_PERSISTENT_INIT_START && func_base < MPI_P2P_PERSISTENT_INIT_END ||
        func_base >= MPI_PROBE_START && func_base < MPI_PROBE_END) {
        if (event->p2p_pr_pb.buf.virt_addr)
            __msl_virt2phys(pmfile, pffile, &event->p2p_pr_pb.buf);
    }
    if (func_base >= MPI_START_OSC_GROUP && func_base < MPI_END_OSC_GROUP) {
        if (event->one_sided_comm.buf.virt_addr)
            __msl_virt2phys(pmfile, pffile, &event->one_sided_comm.buf);
        if (event->one_sided_comm.rbuf.virt_addr)
            __msl_virt2phys(pmfile, pffile, &event->one_sided_comm.rbuf);
        if (event->one_sided_comm.cbuf.virt_addr)
            __msl_virt2phys(pmfile, pffile, &event->one_sided_comm.cbuf);
    }

    if (func_base >= MPI_REQUEST_START && func_base < MPI_REQUEST_END) {
        if (event->requests.requests.virt_addr)
            __msl_virt2phys(pmfile, pffile, &event->requests.requests);
        if (event->requests.statuses.virt_addr)
            __msl_virt2phys(pmfile, pffile, &event->requests.statuses);
    }

    __DEBUG_EPILOG_TRACTOR_LAYER__
}
#endif

void tractor::parse_env_vars(void)
{
    int individual_event_selection = 0, retval = 0;
    __DEBUG_PROLOG_TRACTOR_LAYER__
    char *temp;
    char *endptr;
    temp = __msl_getenv("PEAT_TRACTOR_RETURN_LEVELS");
    if (temp) {
        num_addrs = parse_levels(temp);
    } else {
        num_addrs = 1;
    }
    temp = __msl_getenv("PEAT_TRACTOR_PC_LEVELS");
    if (temp) {
        parse_pc_levels(temp);
    } else {
        pc_levels.clear();
    }

    char *include_events = __msl_getenv("PEAT_TRACTOR_INCLUDE_EVENTS");
    char *exclude_events = __msl_getenv("PEAT_TRACTOR_EXCLUDE_EVENTS");
    char *include_fsu_events = __msl_getenv("PEAT_TRACTOR_INCLUDE_FULL_STACK_EVENTS");
    char *exclude_fsu_events = __msl_getenv("PEAT_TRACTOR_EXCLUDE_FULL_STACK_EVENTS");
    char *boundaries = __msl_getenv("PEAT_TRACTOR_BOUNDARIES");
    temp = __msl_getenv("PEAT_TRACTOR_FILE");
    if (temp)
        stat_template = temp;
    temp = __msl_getenv("PEAT_TRACTOR_DIR");
    if (temp)
        stat_dir = temp;
    temp = __msl_getenv("PEAT_TRACTOR_TMP_DIR");
    if (temp)
        tmp_dir = temp;

#ifdef OPEN_MPI
    char *t_mpi_rank_temp = __msl_getenv("OMPI_COMM_WORLD_RANK");
    if (t_mpi_rank_temp == NULL) {
        debug_printf("Unable to determine Rank (open_mpi)...\n");
    } else {
        debug_printf("Rank (open_mpi) is %s...\n", t_mpi_rank_temp);
    }
#else
    char *t_mpi_rank_temp = __msl_getenv("PMI_RANK");
    if (t_mpi_rank_temp == NULL) {
        debug_printf("Unable to determine Rank (MPICH)...\n");
    } else {
        debug_printf("Rank (MPICH) is %s...\n", t_mpi_rank_temp);
    }
#endif
    if (t_mpi_rank_temp)
        t_mpi_rank = atoi(t_mpi_rank_temp);

#ifdef _TRACTOR_COLLECT_STAT_
    print_all = is_true(__msl_getenv("PEAT_TRACTOR_PRINT_ALL_STAT"), print_all);
#endif
    t_mpi_rank_temp = __msl_getenv("PEAT_TRACTOR_RESOLVE");
    if (t_mpi_rank_temp) {
        resolve_comms_and_ranks = is_true(t_mpi_rank_temp, resolve_comms_and_ranks);
    }
    t_mpi_rank_temp = __msl_getenv("PEAT_TRACTOR_USE_REALTIME_DUMP");
    if (t_mpi_rank_temp) {
        use_realtime_dump = is_true(t_mpi_rank_temp, use_realtime_dump);
    }
    store_map = is_true(__msl_getenv("PEAT_TRACTOR_STORE_MAP"), store_map);

    if (include_events != NULL) {
        memset(counting_permission, 0, sizeof(counting_permission));
        parse_event_mask(include_events, counting_permission);
        //        counting_permission[MPI_INIT_ID_THROUGH] = counting_permission[MPI_INIT_THREAD_ID_THROUGH] =
        //        counting_permission[MPI_FINALIZE_ID_THROUGH] = 1;
    } else {
        memset(counting_permission, 1, sizeof(counting_permission));
        if (exclude_events != NULL) {
            parse_event_mask(exclude_events, counting_permission);
        }
        //        counting_permission[MPI_INIT_ID_THROUGH] = counting_permission[MPI_INIT_THREAD_ID_THROUGH] =
        //        counting_permission[MPI_FINALIZE_ID_THROUGH] = 1;
    }
    if (include_fsu_events != NULL) {
        memset(fullstack_permission, 0, sizeof(fullstack_permission));
        parse_event_mask(include_fsu_events, fullstack_permission);
    } else {
        memset(fullstack_permission, 1, sizeof(fullstack_permission));
        if (exclude_fsu_events != NULL) {
            parse_event_mask(exclude_fsu_events, fullstack_permission);
        }
    }

    if (boundaries != NULL) {
        parse_boundaries(boundaries);
    }
    onetimealloc = defonetimealloc;
    temp = __msl_getenv("PEAT_TRACTOR_ONE_TIME_ALLOC");
    if (temp)
        onetimealloc = atoll(temp);
    temp = __msl_getenv("PEAT_TRACTOR_MAX_TID_NUM");
    if (temp)
        t_max_tid_num = atoll(temp);

    if (onetimealloc < minonetimealloc)
        onetimealloc = minonetimealloc;
    temp = __msl_getenv("PEAT_TRACTOR_CALLS_LIMIT");
    if (temp)
        calls_limit = atoll(temp);

    print_names = is_true(__msl_getenv("PEAT_TRACTOR_PRINT_FUNC_NAMES"), print_names);

    temp = __msl_getenv("PEAT_TRACTOR_PCONTROL");
    if (temp)
        pcontrol = atoi(temp);
    if (pcontrol > 0) {
        temp = __msl_getenv("PEAT_TRACTOR_PCONTROL_FUNCS");
        if (temp) {
            memset(counting_permission_pcontrol, 0, sizeof(counting_permission_pcontrol));
            parse_event_mask(temp, counting_permission_pcontrol);
        }
        // these functions handled anyhow
        counting_permission_pcontrol[MPI_INIT_ID_THROUGH] = 1;
        counting_permission_pcontrol[MPI_INIT_THREAD_ID_THROUGH] = 1;
        counting_permission_pcontrol[MPI_FINALIZE_ID_THROUGH] = 1;
    } else {
        memset(counting_permission_pcontrol, 1, sizeof(counting_permission_pcontrol));
        counting_permission_pcontrol[0] = 0; // User Code
    }
    temp = __msl_getenv("PEAT_TRACTOR_MIN_ADDRESS");
    if (temp)
        lowval = (void *)strtoull(temp, &endptr, 16);
    temp = __msl_getenv("PEAT_TRACTOR_MAX_ADDRESS");
    if (temp)
        highval = (void *)strtoull(temp, &endptr, 16);

#ifdef _TRACTOR_USE_PTPD_
    ptpd_execfile = __msl_getenv("PEAT_TRACTOR_PTPD_EXECUTABLE");
    ptpd_outputfile = __msl_getenv("PEAT_TRACTOR_PTPD_OUTPUT");
    ptpd_nodefile = __msl_getenv("PEAT_TRACTOR_PTPD_NODEFILE");
    ptpd_timenode = __msl_getenv("PEAT_TRACTOR_PTPD_TIMENODE");
    ptpd_ui = __msl_getenv("PEAT_TRACTOR_PTPD_UI");
    if (ptpd_ui == NULL)
        ptpd_ui = "100000"; // each 0.1 sec
    temp = __msl_getenv("PEAT_TRACTOR_USE_PTPD");
    if (temp)
        use_ptpd = is_true(temp, use_ptpd);
#endif
    t_write_main_files = is_true(__msl_getenv("PEAT_TRACTOR_WRITE_TRACE"), t_write_main_files);
    t_use_signals = is_true(__msl_getenv("PEAT_TRACTOR_USE_SIGNALS"), t_use_signals);
    debug_printf("Writing Main Files => %d\n", t_write_main_files);
    temp = __msl_getenv("PEAT_TRACTOR_MAX_THREADS");
    if (temp)
        t_max_threads = atoi(temp);

    temp = __msl_getenv("PEAT_TRACTOR_MAX_NESTING_LEVELS");
    if (temp)
        t_max_nesting_levels = atoi(temp);

    temp = __msl_getenv("PEAT_TRACTOR_FULL_OMPT_UNWIND");
    if (temp)
        full_ompt_unwind = atoi(temp);
    temp = __msl_getenv("PEAT_TRACTOR_ABORT_SLEEP");
    if (temp)
        abort_sleep = atoi(temp);
#ifdef _TRANSPORT_PROFILE_
        t_collect_transport = is_true(__msl_getenv("PEAT_TRACTOR_COLLECT_TRANSPORT"), t_collect_transport);
#endif

    if (!t_write_main_files) {
        calls_limit = 0;
    }

#ifdef _T_OMPT_
    parse_openmp = is_true(__msl_getenv("PEAT_TRACTOR_PARSE_OPENMP"), parse_openmp);
#endif
    write_on_finalize = is_true(__msl_getenv("PEAT_TRACTOR_WRITE_ON_FINALIZE"), write_on_finalize);

    temp = __msl_getenv("PEAT_TRACTOR_COMPRESS_EVENTS");
    if (temp)
        compress_events = atoi(temp);
/*
#if defined(_TRANSPORT_PROFILE_)
    temp = __msl_getenv("PEAT_TRACTOR_TRANSPORT_LAYER");
    transport_layer = T_TRANSPORT_LAYER_NONE;
    if (temp) {
        if (strcasecmp(temp,"VERBS")==0) transport_layer=T_TRANSPORT_LAYER_VERBS;
        else if (strcasecmp(temp,"OFI")==0) transport_layer=T_TRANSPORT_LAYER_OFI;
        else if (strcasecmp(temp,"PSM2")==0) transport_layer=T_TRANSPORT_LAYER_PSM2;
        else if (strcasecmp(temp,"PSM3")==0) transport_layer=T_TRANSPORT_LAYER_PSM3;
        else if (strcasecmp(temp,"UCX")==0) transport_layer=T_TRANSPORT_LAYER_UCX;
        if (transport_layer == T_TRANSPORT_LAYER_NONE) {
            printf(" Unknown Transport Layer specified:%s. No Transport events will be traced\n",temp);
        }
    }
#endif
*/
    __DEBUG_EPILOG_TRACTOR_LAYER__
}

#ifdef _OPENMPI_
/* Open MPI splits the Fortran wrappers: libmpi_mpifh (mpif.h / use mpi; also called by the f08 layer),
   libmpi_usempif08 (use mpi_f08), libmpi_usempi_ignore_tkr / libmpi_usempi (use mpi module code). */
static const char *const t_fortran_wrapper_libs[] = {"libmpi_mpifh", "libmpi_usempif08", "libmpi_usempi_ignore_tkr", "libmpi_usempi", NULL};
static bool t_is_fortran_wrapper_lib(const char *path_or_maps_line)
{
    for (int i = 0; t_fortran_wrapper_libs[i]; ++i) {
        if (strstr(path_or_maps_line, t_fortran_wrapper_libs[i]))
            return true;
    }
    return false;
}
#endif

#if !defined(_TRACTOR_LEGACY_PROC_MAPS_)
namespace {
struct t_maps_phdr_ctx {
    void *binval;
#ifdef _OPENMPI_
    int num_fort;
    void *lowfort[tractor::T_MAX_FORT_RANGES], *highfort[tractor::T_MAX_FORT_RANGES];
#else
    void *lowfort, *highfort;
#endif
    void *lowtrac, *hightrac;
};

int t_maps_phdr_callback(struct dl_phdr_info *info, size_t size, void *data)
{
    t_maps_phdr_ctx *ctx = (t_maps_phdr_ctx *)data;
    bool is_main = (info->dlpi_name == NULL || info->dlpi_name[0] == 0);
#ifdef _OPENMPI_
    bool is_fort = !is_main && t_is_fortran_wrapper_lib(info->dlpi_name);
#else
    bool is_fort = !is_main && strstr(info->dlpi_name, "libmpifort");
#endif
    bool is_trac = !is_main && strstr(info->dlpi_name, "libtractor");
    if (!is_main && !is_fort && !is_trac)
        return 0;
    ElfW(Addr) low = (ElfW(Addr))-1, high = 0;
    for (int i = 0; i < info->dlpi_phnum; ++i) {
        const ElfW(Phdr) &phdr = info->dlpi_phdr[i];
        if (phdr.p_type != PT_LOAD || !(phdr.p_flags & PF_X))
            continue;
        ElfW(Addr) seg_low = info->dlpi_addr + phdr.p_vaddr;
        ElfW(Addr) seg_high = seg_low + phdr.p_memsz;
        if (seg_low < low)
            low = seg_low;
        if (seg_high > high)
            high = seg_high;
        if (is_main) // mirror legacy behavior: only the first PT_LOAD segment matters
            break;
    }
    if (high == 0)
        return 0;
    if (is_main)
        ctx->binval = (void *)high;
    else if (is_fort) {
#ifdef _OPENMPI_
        if (ctx->num_fort < tractor::T_MAX_FORT_RANGES) {
            ctx->lowfort[ctx->num_fort] = (void *)low;
            ctx->highfort[ctx->num_fort] = (void *)high;
            ctx->num_fort++;
        }
#else
        ctx->lowfort = (void *)low;
        ctx->highfort = (void *)high;
#endif
    } else if (is_trac) {
        ctx->lowtrac = (void *)low;
        ctx->hightrac = (void *)high;
    }
    return 0;
}
} // namespace

void tractor::map_file_parse(void)
{
    t_maps_phdr_ctx ctx = {0};
    dl_iterate_phdr(t_maps_phdr_callback, &ctx);
    if (ctx.binval)
        binval = ctx.binval;
#ifdef _OPENMPI_
    if (ctx.num_fort > 0) {
        num_fort_ranges = ctx.num_fort;
        for (int i = 0; i < num_fort_ranges; ++i) {
            lowfort_r[i] = ctx.lowfort[i];
            highfort_r[i] = ctx.highfort[i];
        }
        lowfort = lowfort_r[0];
        highfort = highfort_r[0];
    }
#else
    if (ctx.highfort) {
        lowfort = ctx.lowfort;
        highfort = ctx.highfort;
    }
#endif
    if (ctx.hightrac) {
        lowtrac = ctx.lowtrac;
        hightrac = ctx.hightrac;
    }

    // main arena heap bounds: cache the break at first call, track current top thereafter
    static void *heap_base = NULL;
    if (!heap_base)
        heap_base = sbrk(0);
    void *heap_top = sbrk(0);
    if (heap_top != heap_base) {
        lowheap = heap_base;
        highheap = heap_top;
    }

    pthread_attr_t attr;
    if (0 == pthread_getattr_np(pthread_self(), &attr)) {
        void *stack_addr;
        size_t stack_size;
        if (0 == pthread_attr_getstack(&attr, &stack_addr, &stack_size)) {
            lowstack = stack_addr;
            highstack = (void *)((char *)stack_addr + stack_size);
        }
        pthread_attr_destroy(&attr);
    }
}
#else
void tractor::map_file_parse(void)
{
    char pid[16];
    char tbuf[65536];
    FILE *tfp;
    sprintf(tbuf, "/proc/%u/maps", t_pid);
    tfp = fopen(tbuf, "rt");
    if (tfp) {
        unsigned long long stub;
        tbuf[0] = 0;
#ifdef _OPENMPI_
        num_fort_ranges = 0; /* rebuilt on every parse; this function runs more than once */
#endif
        /* handle main binary */
        fgets(tbuf, MAX_PATH, tfp);
        sscanf(tbuf, "%llx-%llx", &stub, &binval);
        while (!feof(tfp)) {
            tbuf[0] = 0;
            fgets(tbuf, 65536, tfp);
#ifdef _OPENMPI_
            if (t_is_fortran_wrapper_lib(tbuf)) {
                void *low_t, *high_t;
                char attr[65536];
                sscanf(tbuf, "%llx-%llx %s", &low_t, &high_t, attr);
                if (strstr(attr, "x") && num_fort_ranges < T_MAX_FORT_RANGES) {
                    lowfort_r[num_fort_ranges] = low_t;
                    highfort_r[num_fort_ranges] = high_t;
                    num_fort_ranges++;
                    lowfort = lowfort_r[0];
                    highfort = highfort_r[0];
                }
            }
#else
            if (strstr(tbuf, "libmpifort")) {
                void *low_t, *high_t;
                char attr[65536];
                sscanf(tbuf, "%llx-%llx %s", &low_t, &high_t, attr);
                if (strstr(attr, "x")) {
                    lowfort = low_t;
                    highfort = high_t;
                }
            }
#endif
            if (strstr(tbuf, "libtractor")) {
                void *low_t, *high_t;
                char attr[65536];
                sscanf(tbuf, "%llx-%llx %s", &low_t, &high_t, attr);
                if (strstr(attr, "x")) {
                    lowtrac = low_t;
                    hightrac = high_t;
                }
            }
            if (strstr(tbuf, "[heap]")) {
                void *low_t, *high_t;
                sscanf(tbuf, "%llx-%llx", &low_t, &high_t);
                // debug_printf ("heap:%s <-------> %llx %llx\n",tbuf,low_t,high_t);
                lowheap = low_t;
                highheap = high_t;
            }
            if (strstr(tbuf, "[stack]")) {
                void *low_t, *high_t;
                sscanf(tbuf, "%llx-%llx", &low_t, &high_t);
                // debug_printf ("stack:%s <-------> %llx %llx\n",tbuf,low_t,high_t);
                lowstack = low_t;
                highstack = high_t;
            }
            if (tbuf[0] == 0)
                break;
        }
        fclose(tfp);
    }
}
#endif
void tractor::init(void)
{
    __DEBUG_PROLOG_TRACTOR_LAYER__
    int i, j, retval;
    PEAT_Clock tsc = rdtsca();
    char tfilename[1024];
    // char filename[1024];
    char pid[16];
    msl_event *event;
    debug_printf("Rank %d: Tractor pre_initialization started...\n", t_mpi_rank);
#if defined(_TRANSPORT_PROFILE_)
    if (t_collect_transport && t_mpi_rank >= 0) {
        // t_transport_prof.init_hooks();
        t_transport_prof.calls_limit = calls_limit;
    }
#endif

#ifdef _MEM_HOOKS_
    mprof.enable_memory_profiling();
#endif
    //__msl_getenv("LD_PRELOAD");
    debug_printf("Rank %d: Tractor Constructor started...\n", t_mpi_rank);
    // unsetenv("LD_PRELOAD");
    // num_addrs=max_ret_level+1;
    sprintf(tfilename, "%s/%s", stat_dir, stat_template);
    // strcpy(tfilename, stat_template);
    strcpy(gz_mode, "w9"); // simple text output for all files
    if (compression || Z_DEFAULT_COMPRESSION != compression_level) {
        if (Z_DEFAULT_COMPRESSION != compression_level) {
            sprintf(gz_mode, "w%d", compression_level);
            compression = true;
            gz_ext = ".gz";
        }
    } else {
        gz_ext = "";
        strcpy(gz_mode, "wT");
    }

    extras = 1;
    t_base_event::init(t_max_threads,t_max_nesting_levels);

    if (use_realtime_dump) {
        const int slots = t_max_threads * t_max_nesting_levels;
        fps_events = (FILE **)t_malloc(slots * sizeof(FILE *), 128);
        memset(fps_events, 0, slots * sizeof(FILE *));
        fps_extras = (FILE **)t_malloc(slots * sizeof(FILE *), 128);
        memset(fps_extras, 0, slots * sizeof(FILE *));
    }

#if defined(_TRANSPORT_PROFILE_)
    if (t_collect_transport && t_mpi_rank >= 0) {
        t_transport_prof.init(t_max_threads, stat_dir, tmp_dir, stat_template, use_realtime_dump);
        t_permit_collect_transport = 1;
    }
#endif
    char tbuf[MAX_PATH];
    FILE *tfp, *tfp1;
    pid[0] = 0;
    if (t_mpi_rank >= 0)
        sprintf(pid, ".%u", t_mpi_rank);
    else
        sprintf(pid, ".%u", t_pid);
    sprintf(mpi_filename, "%s.mpi%s%s", tfilename, pid, gz_ext);
    sprintf(hwc_filename, "%s.hwc%s%s", tfilename, pid, gz_ext);
    sprintf(com_filename, "%s.com%s%s", tfilename, pid, gz_ext);

    if (t_mpi_rank == 0) {
        sprintf(app_filename, "%s.app%s", tfilename, gz_ext);
    }
    if (t_mpi_rank < 0) {
        sprintf(app_filename, "%s.app%s%s", tfilename, pid, gz_ext);
    }
    if (store_map) {
        sprintf(tbuf, "/proc/%u/maps", t_pid);
        tfp = fopen(tbuf, "rt");
        sprintf(tbuf, "%s/%s.map%s", stat_dir, tfilename, pid);
        tfp1 = fopen(tbuf, "wb");
        if (tfp && tfp1) {
            do {
                tbuf[0] = 0;
                fgets(tbuf, MAX_PATH, tfp);
                if (tbuf[0] == 0)
                    break;
                fputs(tbuf, tfp1);
            } while (!feof(tfp));
            fclose(tfp1);
            fclose(tfp);
        }
    }

#ifdef _TRACTOR_VIRT2PHYS_
    {
        char tbuf[MAX_PATH];
        sprintf(tbuf, "/proc/%u/pagemap", t_pid);
        pmfile = __msl_open_pfile(tbuf);
        sprintf(tbuf, "/proc/kpageflags");
        pffile = __msl_open_pfile(tbuf);
        if (pmfile <= 0)
            abort(); // abort if error
    }
#endif
#if defined(_INTELCOMPILER_)
    int cpu_info[4] = {0, 0, 0, 0};
    __cpuid(cpu_info, 0);
#else
    unsigned int cpu_info[4] = {0, 0, 0, 0};
    __get_cpuid(0, &cpu_info[0], &cpu_info[1], &cpu_info[2], &cpu_info[3]);
#endif
    // CPUID leaf 0x16 (Processor Frequency Information) is Intel-specific;
    // AMD CPUs commonly report a max leaf above 0x16 but return garbage for it.
    char vendor_id[13];
    memcpy(&vendor_id[0], &cpu_info[1], 4);
    memcpy(&vendor_id[4], &cpu_info[3], 4);
    memcpy(&vendor_id[8], &cpu_info[2], 4);
    vendor_id[12] = 0;
    bool is_intel = (strcmp(vendor_id, "GenuineIntel") == 0);
    if (is_intel && cpu_info[0] >= 0x16) {
#if defined(_INTELCOMPILER_)
        __cpuid(cpu_info, 0x16);
#else
        __get_cpuid(0x16, &cpu_info[0], &cpu_info[1], &cpu_info[2], &cpu_info[3]);
#endif
        cpu_proc_freq[0] = cpu_info[0];
        cpu_proc_freq[1] = cpu_info[1];
        cpu_proc_freq[0] *= 1000000;
        cpu_proc_freq[1] *= 1000000;
    } else {
        sprintf(tbuf, "/proc/cpuinfo");
        tfp = fopen(tbuf, "rt");
        if (tfp) {
            while (!feof(tfp)) {
                tbuf[0] = 0;
                fgets(tbuf, MAX_PATH, tfp);
                if (strstr(tbuf, "model name") && cpu_proc_freq[0] == 0) {
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
                        cpu_proc_freq[0] = (long long)(strtod(tbuf, &freq_start) * power);
                        if (cpu_proc_freq[0] > 0 && cpu_proc_freq[1] > 0)
                            break;
                    }
                }

                if (strstr(tbuf, "cpu MHz") && cpu_proc_freq[1] == 0) {
                    char *freq_start = strstr(tbuf, ":");
                    if (freq_start) {
                        int i = 0;
                        freq_start++;
                        while (freq_start[i] >= '0' && freq_start[i] <= '9' || freq_start[i] == ' ' || freq_start[i] == '.')
                            ++i;
                        int power = 1000000;
                        freq_start[i] = 0;
                        strcpy(tbuf, freq_start);
                        cpu_proc_freq[1] = (long long)(strtod(tbuf, &freq_start) * power);
                        if (cpu_proc_freq[0] > 0 && cpu_proc_freq[1] > 0)
                            break;
                    }
                }
                if (tbuf[0] == 0)
                    break;
            }
            fclose(tfp);
        }
    }
    if (cpu_proc_freq[0] == 0 && cpu_proc_freq[1] == 0) {
        calc_freq();
    } else {
        if (cpu_proc_freq[0] == 0 && cpu_proc_freq[1] != 0) cpu_proc_freq[0] = cpu_proc_freq[1];
        if (cpu_proc_freq[0] != 0 && cpu_proc_freq[1] == 0) cpu_proc_freq[1] = cpu_proc_freq[0];
    }
#ifdef USE_NANO_TIMER
    proc_freq[0] = proc_freq[1] = PEAT_BILLION;
#else
    proc_freq[0] = cpu_proc_freq[0];
    proc_freq[1] = cpu_proc_freq[1];
#endif
    debug_printf("Rank %d: Prolog finished...\n", t_mpi_rank);
    tractor_initialized = 1;
    counting_permit = !pcontrol;

    debug_printf("Rank %d: Tractor Constructor finished...\n", t_mpi_rank);
    debug_printf("Event Masks:\n", t_mpi_rank);
    char *conoff[2] = {"OFF", "ON"};
    for (i = 0; i < MPI_MAX_FUNCID_THROUGH; i++) {
        debug_printf("%s = %s\n", mpi_function_names[i], conoff[counting_permission[i] != 0]);
    }
    if (t_mpi_rank < 0) {
        post_mpi_init();
    }

    __DEBUG_EPILOG_TRACTOR_LAYER__
}

void tractor::post_ptpd_init(void)
{
    __DEBUG_PROLOG_TRACTOR_LAYER__
#ifdef USE_NANO_TIMER
    first_par_tsc = peat_get_nano_sec();
#else
    first_par_tsc = rdtsca();
#endif
    debug_printf("rank %d: msl_post_ptpd_init called...\n", t_mpi_rank);
    __DEBUG_EPILOG_TRACTOR_LAYER__
}

void tractor::post_mpi_init(void)
{
    __DEBUG_PROLOG_TRACTOR_LAYER__
    debug_printf("rank %d: msl_post_mpi_init called...\n", t_mpi_rank);
    if (tractor_initialized < 1)
        return;
    int old_skip_parse =  mpi_skip_parse;
    mpi_skip_parse = 1;
    if (t_mpi_rank >= 0) {
        PMPI_Comm_dup(MPI_COMM_WORLD, &TRACTOR_COMM_WORLD);
        gt_ninfo = new t_nodes_info;
        nodes_info(NULL);
        local_comms->init(); /* no-op if already done by an internal comm created during PMPI_Init */
    }
    mpi_skip_parse = old_skip_parse;
#ifdef _USE_SIGNALS_
    //  protect writing trace
    if (t_use_signals)
        install_signals(t_mpi_rank);
#endif
    __DEBUG_EPILOG_TRACTOR_LAYER__
}
void tractor::pre_finalize(int abort)
{
    __DEBUG_PROLOG_TRACTOR_LAYER__
#ifdef USE_NANO_TIMER
    last_par_tsc = peat_get_nano_sec();
#else
    last_par_tsc = rdtsca();
#endif
    debug_printf("rank %d: msl_pre_finalize called...\n", t_mpi_rank);
    if (tractor_initialized < 1)
        return;
#ifdef _TRANSPORT_PROFILE_
    t_permit_collect_transport = 0;
#endif
    int old_skip_parse =  mpi_skip_parse;
    mpi_skip_parse = 1;
    if (t_mpi_rank >= 0 && abort == 0) {
        nodes_info(gt_ninfo);
        global_comms.bind_local_comms();
        /* one id column count for the whole trace: bind records refer to records of other ranks by their id path */
        int local_levels = t_achieved_nesting_levels, global_levels = local_levels;
        PMPI_Allreduce(&local_levels, &global_levels, 1, MPI_INT, MPI_MAX, TRACTOR_COMM_WORLD);
        t_achieved_nesting_levels = global_levels;
    }
    if (t_mpi_rank >= 0) {
        PMPI_Comm_free(&TRACTOR_COMM_WORLD);
        if (grp_smp != MPI_GROUP_NULL)
            PMPI_Group_free(&grp_smp);
        if (comm_smp != MPI_COMM_NULL)
            PMPI_Comm_free(&comm_smp);
        if (grp_master != MPI_GROUP_NULL)
            PMPI_Group_free(&grp_master);
        if (comm_master != MPI_COMM_NULL)
            PMPI_Comm_free(&comm_master);
        if (grp_world != MPI_GROUP_NULL)
            PMPI_Group_free(&grp_world);
        local_comms->finalize();
    }
    mpi_skip_parse = old_skip_parse;
    __DEBUG_EPILOG_TRACTOR_LAYER__
}

#ifdef _TRACTOR_USE_PTPD_
#define TAG_START 10
#define TAG_STOP 11

static MPI_Comm ptpd_comm = MPI_COMM_NULL;
void tractor::ptpd_init(void)
{
    __DEBUG_PROLOG_TRACTOR_LAYER__
    char ptpd_fn[1024], ptpd_nfn[1024];
    char tfn[1024];
    MPI_Status Sstart;
    int rank, size;
    if (tractor_initialized < 1)
        return;
    if (use_ptpd < 1 || t_mpi_rank < 1) {
        debug_printf("PTPD is disabled...\n");
        __DEBUG_EPILOG_TRACTOR_LAYER__
        return;
    }
    PMPI_Comm_rank(TRACTOR_COMM_WORLD, &rank);
    debug_printf("Rank %d: ptpd_init called ...\n", rank);

    if (rank == 0) {
        debug_printf("Rank 0: PTPD spawn started...\n");
        char **ptpd_argv;
        MPI_Info ptpd_info;
        int *ptpd_errcodes;
        // sprintf(tfn,"%s/%s",stat_dir,stat_template);
        strcpy(tfn, stat_template);
        if (ptpd_outputfile) {
            sprintf(ptpd_fn, "%s%s", ptpd_outputfile, gz_ext);
        } else {
            sprintf(ptpd_fn, "%s/ptpd-%s%s", stat_dir, tfn, gz_ext);
        }
        if (ptpd_execfile == NULL) {
#if 1
            Dl_info info;
            const char *sopath;
            char *libsuffix = _TRACTOR_LIB_SUFFIX_;
            char *binsuffix = _TRACTOR_BIN_SUFFIX_;
            char *sodir, *bindir;
            void *addr = (void *)&so_ident;
            dladdr(addr, &info);
            sopath = info.dli_fname;
            //            try to find either in the same directory or in ../bin
            strcpy(ptpd_in_libpath, sopath);
            sodir = dirname(ptpd_in_libpath);
            strcpy(ptpd_in_binpath, sodir);
            bindir = dirname(ptpd_in_binpath);
            if (sodir)
                sprintf(ptpd_in_libpath, "%s/ptpd%s", sodir, binsuffix);
            if (bindir)
                sprintf(ptpd_in_binpath, "%s/bin/ptpd%s", bindir, binsuffix);
            debug_printf("Rank 0: PTPD search locations %s and %s ...\n", ptpd_in_libpath, ptpd_in_binpath);
            //          now try verify if file exist and it's executable
            if (access(ptpd_in_binpath, 0) == 0) {
                ptpd_execfile = ptpd_in_binpath;
            } else {
                if (access(ptpd_in_libpath, 0) == 0) {
                    ptpd_execfile = ptpd_in_libpath;
                } else {
                    debug_printf("Rank 0: PTPD_Init: PTPD executable is not found... Abort.\n");
                    PMPI_Abort(TRACTOR_COMM_WORLD, -1);
                }
            }
            debug_printf("Rank 0: PTPD_Init: PTPD executable is found in %s .\n", ptpd_execfile);
#else
            debug_printf("Rank 0: PTPD_Init: PTPD executable is not specified... PTPD disabled.\n");
            return;
#endif

            //            PMPI_Abort(TRACTOR_COMM_WORLD,-1);
        }
        if (size_master <= 1) {
            debug_printf("Rank 0: PTPD_Init: Single Node run. PTPD is not required. Disabling...\n");
            return;
        }
        if (ptpd_nodefile == NULL) {
            sprintf(ptpd_nfn, "%s/ptpd-nodes-%s", stat_dir, stat_template);
            ptpd_nodefile = ptpd_nfn;
            debug_printf("Rank 0: PTPD_Init: PTPD nodefile is not specified... Using default (%s) name.\n", ptpd_nfn);
            //            return;
            //            PMPI_Abort(TRACTOR_COMM_WORLD,-1);
        }
        ptpd_errcodes = (int *)t_malloc((size_master + 2) * sizeof(int), 128);
        ptpd_argv = (char **)t_malloc(6 * sizeof(char *), 128);
        PMPI_Info_create(&ptpd_info);
        PMPI_Info_set(ptpd_info, "hostfile", ptpd_nodefile);
        //        PMPI_Info_set(ptpd_info,"perhost","1");
        ptpd_argv[0] = ptpd_ui;
        ptpd_argv[1] = ptpd_fn;
        ptpd_argv[2] = gz_mode;
        if (ptpd_timenode)
            ptpd_argv[3] = "1";
        else
            ptpd_argv[3] = "0";
        ptpd_argv[4] = NULL;
        int r_size_master = size_master + ((int)(ptpd_timenode != NULL));
        debug_printf("Rank 0: MPI_Comm_spawn (%d %s %s %s %s) is being executed.\n", r_size_master, ptpd_execfile, ptpd_ui, ptpd_fn, gz_mode);
        int old_skip_parse =  mpi_skip_parse;
        mpi_skip_parse = 1;

        PMPI_Comm_spawn(ptpd_execfile, ptpd_argv, r_size_master, ptpd_info, 0, MPI_COMM_SELF, &ptpd_comm, ptpd_errcodes);
        mpi_skip_parse = old_skip_parse;
        if (ptpd_errcodes)
            t_free(ptpd_errcodes, (size_master + 2) * sizeof(int));
        if (ptpd_argv)
            t_free(ptpd_argv, 6 * sizeof(char *));

        if (ptpd_comm != MPI_COMM_NULL) {
            int start_counting = 1;
            PMPI_Send(&start_counting, 1, MPI_INT, 0, TAG_START, ptpd_comm);
            debug_printf("Rank 0: MPI_Comm_spawn (%d %s %s %s) was successful.\n", r_size_master, ptpd_execfile, ptpd_ui, ptpd_fn);
        } else {
            debug_printf("Rank 0: MPI_Comm_spawn (%d %s %s %s) was unsuccessful.\n", r_size_master, ptpd_execfile, ptpd_ui, ptpd_fn);
            // return;
            //            PMPI_Abort(MPI_COMM_NULL,-1);
        }
        debug_printf("Rank 0: PTPD spawn finished...\n");
        PMPI_Info_free(&ptpd_info);
    }
    __DEBUG_EPILOG_TRACTOR_LAYER__
}

void tractor::ptpd_finalize(void)
{
    __DEBUG_PROLOG_TRACTOR_LAYER__
    if (tractor_initialized < 1 || t_mpi_rank < 1) {
        __DEBUG_EPILOG_TRACTOR_LAYER__
        return;
    }
    MPI_Status Sstop;
    int stop_counting = 1, rank;
    if (ptpd_comm == MPI_COMM_NULL) {
        __DEBUG_EPILOG_TRACTOR_LAYER__
        return;
    }
    PMPI_Comm_rank(TRACTOR_COMM_WORLD, &rank);
    if (rank == 0) {
        PMPI_Send(&stop_counting, 1, MPI_INT, 0, TAG_STOP, ptpd_comm);         // wait for permission to count if started by MPI_Comm_spawn
        PMPI_Recv(&stop_counting, 1, MPI_INT, 0, TAG_STOP, ptpd_comm, &Sstop); // acknowledge
    }
    PMPI_Comm_free(&ptpd_comm);

    __DEBUG_EPILOG_TRACTOR_LAYER__
}

#endif
extern PEAT_Clock global_tool_overhead;
extern PEAT_Clock global_mpi_time;
extern PEAT_Clock global_gpu_time;
extern PEAT_Clock global_parallel_app_time;
extern PEAT_Clock global_progress_calls;
extern PEAT_Clock global_progress_yields;

void tractor::finalize(void)
{
    __DEBUG_PROLOG_TRACTOR_LAYER__
    int i, j, retval;
    debug_printf("Rank %d: Epilog started...\n", t_mpi_rank);
    if (tractor_initialized < 1)
        return;
    so_walker(g_shared_objects);
#ifdef _MEM_HOOKS_
    map_file_parse(); // parse it once again since heap and stack might be changed
    mprof.set_add_vars(lowheap, highheap, lowstack, highstack);
    mprof.initialize_object_addresses(g_shared_objects);
#endif
    /*
        write_trace(fps_mpi, fps_hwc,fps_com,fps_app);
        for (i=0;i<t_tid_counter;i++) {
            destroy_whole_chain(events_start[i]);
            destroy_whole_chain(extras_start[i]);
        }

        if (gt_ninfo)  delete gt_ninfo;
        if (local_comms) delete local_comms;
    */
    debug_printf("Rank %d: Epilog finished...\n", t_mpi_rank);
    // end();
#ifdef _MEM_HOOKS_
    mprof.disable_memory_profiling();
#endif
#ifdef USE_NANO_TIMER
    last_tsc = peat_get_nano_sec();
#else
    last_tsc = rdtsca();
#endif
    if (write_on_finalize) {
        end();
    }
    __DEBUG_EPILOG_TRACTOR_LAYER__
}

void tractor::abort(void)
{
    __DEBUG_PROLOG_TRACTOR_LAYER__
#ifdef USE_NANO_TIMER
    last_par_tsc = peat_get_nano_sec();
#else
    last_par_tsc = rdtsca();
#endif
    int i, j, retval;
    if (tractor_initialized < 1)
        return;
    so_walker(g_shared_objects);
#ifdef _MEM_HOOKS_
    map_file_parse(); // parse it once again since heap and stack might be changed
    mprof.set_add_vars(lowheap, highheap, lowstack, highstack);
    mprof.initialize_object_addresses(g_shared_objects);
#endif
#ifdef _MEM_HOOKS_
    mprof.disable_memory_profiling();
#endif
#ifdef USE_NANO_TIMER
    last_tsc = peat_get_nano_sec();
#else
    last_tsc = rdtsca();
#endif
    write_on_finalize = 1;
    resolve_comms_and_ranks = 0;
    end();
    __DEBUG_EPILOG_TRACTOR_LAYER__
}

void tractor::end(void)
{
    __DEBUG_PROLOG_TRACTOR_LAYER__
    int i;
    debug_printf("Rank %d: Tractor Destructor started...\n", t_mpi_rank);
    if (tractor_initialized < 1)
        return;
    if (t_mpi_rank < 0) { //|| last_par_tsc==0) {
        pre_finalize(0);
        finalize();
    }
#ifdef _T_OMPT_
    if (ompt_finalize_tool)
        ompt_finalize_tool();
#endif

#ifdef _MEMPROFILE_
    PEAT_LNum max_memory_application_rss = mprof.get_max_memory_RSS_in_KB(); // in kibibytes
    PEAT_LNum tractor_allocated = bytes_allocated;
    PEAT_LNum max_memory_application_rs_scorrected = max_memory_application_rss - ((tractor_allocated /* + (mprof.get_total_events() * sizeof(memory_profiler_event))*/) / 1024); // in kibibytes
#endif
    tractor_initialized = 0;
    if (global_mpi_time > 0)
        write_trace(); // in case LD_PRELOAD was used with 'wrong' app (like mpiexec.hydra etc)
    if (gt_ninfo)
        delete gt_ninfo;
    if (local_comms)
        delete local_comms;
#ifdef _TRACTOR_VIRT2PHYS_
    {
        if (pmfile > 0)
            __msl_close_pfile(pmfile);
        if (pffile > 0)
            __msl_close_pfile(pffile);
    }
#endif

    double p2dratio, m2pratio, t2pratio, g2pratio;
    PEAT_Clock init_tool_overhead = overall_tool_overhead[MPI_INIT_ID_THROUGH] + overall_tool_overhead[MPI_INIT_THREAD_ID_THROUGH] + overall_tool_overhead[MPI_FINALIZE_ID_THROUGH];
    PEAT_Clock mpi_init_time = overall_durations[MPI_INIT_ID_THROUGH] + overall_durations[MPI_INIT_THREAD_ID_THROUGH] + overall_durations[MPI_FINALIZE_ID_THROUGH];
    PEAT_Clock total_app_time_correted = (last_tsc - first_tsc) - overall_tool_overhead[MPI_INIT_ID_THROUGH] - overall_tool_overhead[MPI_INIT_THREAD_ID_THROUGH] - overall_tool_overhead[MPI_FINALIZE_ID_THROUGH];
    p2dratio = ((double)(((last_par_tsc - first_par_tsc) + mpi_init_time) * 100.0)) / ((double)(total_app_time_correted));
    m2pratio = ((double)(global_mpi_time * 100.0)) / ((double)(last_par_tsc - first_par_tsc) + mpi_init_time);
    t2pratio = ((double)(global_tool_overhead * 100.0)) / ((double)(last_par_tsc - first_par_tsc));
    g2pratio = ((double)(global_gpu_time * 100.0)) / ((double)(last_par_tsc - first_par_tsc));
    if (fps_com) {
#if 0
        gzprintf(fps_com, "%d:Application Ends.\n%d:Duration:%lld.\n",
                 PEAT_RT_Remark,
                 PEAT_RT_Remark,
                 (PEAT_Clock)((last_tsc - first_tsc))); 

        gzprintf(fps_com, "%d:Parallel Time: %lld(%.2lf%).\n",
                 PEAT_RT_Remark,
                 ((PEAT_Clock)((last_par_tsc - first_par_tsc))) + mpi_init_time, p2dratio);

        gzprintf(fps_com, "%d:MPI Time: %lld(%.2lf%).\n",
                 PEAT_RT_Remark,
                 global_mpi_time, m2pratio);

        gzprintf(fps_com, "%d:Internal Tool overhead: %lld(%.2lf%).\n",
                 PEAT_RT_Remark,
                 global_tool_overhead, t2pratio);

        gzprintf(fps_com, "%d:Init/Finalize Tool overhead: %lld.\n",
                 PEAT_RT_Remark,
                 init_tool_overhead);
#else
        gzprintf(fps_com, "%d:Application Ends.\n%d:Duration:%lld.\n%d:Parallel Time: %lld(%.2lf%).\n%d:MPI Time: %lld(%.2lf%).\n%d:GPU Time: %lld(%.2lf%).\n%d:Internal Tool overhead: %lld(%.2lf%).\n%d:Init/Finalize Tool overhead: %lld.\n",
                 PEAT_RT_Remark,
                 PEAT_RT_Remark,
                 (PEAT_Clock)((last_tsc - first_tsc)),
                 PEAT_RT_Remark,
                 ((PEAT_Clock)((last_par_tsc - first_par_tsc))) + mpi_init_time, p2dratio,
                 PEAT_RT_Remark,
                 global_mpi_time, m2pratio,
                 PEAT_RT_Remark,
                 global_gpu_time, g2pratio,
                 PEAT_RT_Remark,
                 global_tool_overhead, t2pratio,
                 PEAT_RT_Remark,
                 init_tool_overhead);
        gzprintf(fps_com, "%d:%lld:%.2lf%", PEAT_RT_MPI_Time, global_mpi_time, m2pratio);
#endif
    }

#ifdef _MEMPROFILE_
    if (fps_com)
        gzprintf(fps_com, "%d:Memory allocated  by application maximum RSS(KB). Raw(whole): %ld, Tractor: %ld, corrected (-Tractor): %ld\n", PEAT_RT_Remark,
                 max_memory_application_rss,
                 tractor_allocated / 1024,
                 max_memory_application_rs_scorrected);
#endif

    if (start_sync)
        t_free(start_sync, size_cw * sizeof(PEAT_Clock));
    if (stop_sync)
        t_free(stop_sync, size_cw * sizeof(PEAT_Clock));

    if (ret_levels != &level0)
        t_free(ret_levels, t_o_levels * sizeof(int));
    if (fps_com)
        gzclose(fps_com);

    if (fps_events || fps_extras) {
        const int slots = t_max_threads * t_max_nesting_levels;
        for (int idx = 0; idx < slots; idx++) {
            char temp_filename[MAX_PATH];
            const int tid = idx / t_max_nesting_levels, level = idx % t_max_nesting_levels;
            if (fps_events && fps_events[idx]) {
                fclose(fps_events[idx]);
                sprintf(temp_filename, "%s/%s.%d.%d.%d.mevts.tmp", tmp_dir, stat_template, t_mpi_rank, tid, level);
                unlink(temp_filename);
            }
            if (fps_extras && fps_extras[idx]) {
                fclose(fps_extras[idx]);
                sprintf(temp_filename, "%s/%s.%d.%d.%d.mexts.tmp", tmp_dir, stat_template, t_mpi_rank, tid, level);
                unlink(temp_filename);
            }
        }
        if (fps_events)
            t_free(fps_events, slots * sizeof(FILE *));
        if (fps_extras)
            t_free(fps_extras, slots * sizeof(FILE *));
        fps_events = fps_extras = NULL;
    }
    /* from here on, any instrumentation hook fired by a library's own ELF-level destructors
       (e.g. libfabric's vrb_fini running after this object's dtor) must become a no-op */
    tractor_finished = true;
    debug_printf("Rank %d: Tractor Destructor finished...\n", t_mpi_rank);
    __DEBUG_EPILOG_TRACTOR_LAYER__
}

void tractor::get_rank_time(PEAT_Clock *start, PEAT_Clock *start_parallel, PEAT_Clock *end_parallel, PEAT_Clock *end, PEAT_Clock *overall_tool_overhead, PEAT_Clock *overall_MPI, PEAT_Clock *overall_GPU)
{
    __DEBUG_PROLOG_TRACTOR_LAYER__
    *start = first_tsc ;
    *start_parallel = first_par_tsc;
    *end = last_tsc;
    *end_parallel = last_par_tsc;
    *overall_tool_overhead = global_tool_overhead;
    *overall_MPI = global_mpi_time;
    *overall_GPU = global_gpu_time;
    __DEBUG_EPILOG_TRACTOR_LAYER__
}

void tractor::nodes_info(t_nodes_info *ninfo)
{
    __DEBUG_PROLOG_TRACTOR_LAYER__
    debug_printf("Rank %d: msl_nodes_info started...\n", t_mpi_rank);
    if (tractor_initialized < 1)
        return;
    assert(sizeof(long long) == sizeof(double));
    char localhost[MAX_HOSTNAME];
    int local_ranks_num = 0;
    int i, j;
    MPI_Status mpi_stat_tmp;
    if (tractor_initialized < 1)
        return;

    PMPI_Comm_rank(TRACTOR_COMM_WORLD, &rank_cw);
    PMPI_Comm_size(TRACTOR_COMM_WORLD, &size_cw);
    if (start_sync == NULL) {
        start_sync = (PEAT_Clock *)t_malloc(size_cw * sizeof(PEAT_Clock), 128);
    }
    if (stop_sync == NULL) {
        stop_sync = (PEAT_Clock *)t_malloc(size_cw * sizeof(PEAT_Clock), 128);
    }
    PMPI_Barrier(TRACTOR_COMM_WORLD);
#ifdef USE_NANO_TIMER
    PEAT_Clock local_clck = peat_get_nano_sec();
#else
    PEAT_Clock local_clck = rdtsca();
#endif
    if (ninfo == NULL) {
        PMPI_Gather(&local_clck, 1, MPI_LONG_LONG, start_sync, 1, MPI_LONG_LONG, 0, TRACTOR_COMM_WORLD);
    } else {
        PMPI_Gather(&local_clck, 1, MPI_LONG_LONG, stop_sync, 1, MPI_LONG_LONG, 0, TRACTOR_COMM_WORLD);
    }
    char all_hosts[size_cw * MAX_HOSTNAME], *temp_host;
    gethostname(localhost, MAX_HOSTNAME);
    int local_ranks[size_cw];
    int all_ranks[size_cw];

    if (comm_master == MPI_COMM_NULL && comm_smp == MPI_COMM_NULL) {
        PMPI_Allgather(localhost, MAX_HOSTNAME, MPI_BYTE, all_hosts, MAX_HOSTNAME, MPI_BYTE, TRACTOR_COMM_WORLD);

        for (i = 0, temp_host = all_hosts; i < size_cw; temp_host += MAX_HOSTNAME, i++) {
            if (strcmp(localhost, temp_host) == 0)
                local_ranks[local_ranks_num++] = i;
        }
        PMPI_Comm_group(TRACTOR_COMM_WORLD, &grp_world);
        PMPI_Group_incl(grp_world, local_ranks_num, local_ranks, &grp_smp);
        PMPI_Comm_create(TRACTOR_COMM_WORLD, grp_smp, &comm_smp);

        PMPI_Comm_rank(comm_smp, &rank_smp);
        PMPI_Comm_size(comm_smp, &size_smp);
        i = (rank_smp == 0);
        PMPI_Allgather(&i, 1, MPI_INT, all_ranks, 1, MPI_INT, TRACTOR_COMM_WORLD);

        for (i = 0, local_ranks_num = 0; i < size_cw; i++) {
            if (all_ranks[i] > 0)
                local_ranks[local_ranks_num++] = i;
        }

        PMPI_Group_incl(grp_world, local_ranks_num, local_ranks, &grp_master);
        PMPI_Comm_create(TRACTOR_COMM_WORLD, grp_master, &comm_master);
        if (comm_master != MPI_COMM_NULL) {
            PMPI_Comm_rank(comm_master, &rank_master);
            PMPI_Comm_size(comm_master, &size_master);
        }
    }

    PEAT_Clock local_tscs_in[3];
    PEAT_Clock local_tscs[3];
    local_tscs_in[0] = first_tsc;
    local_tscs_in[1] = first_par_tsc;
    local_tscs_in[2] = last_par_tsc;
    PMPI_Reduce(local_tscs_in, local_tscs, 2, MPI_LONG_LONG_INT, MPI_MIN, 0, comm_smp);
    PMPI_Reduce(&local_tscs_in[2], &local_tscs[2], 1, MPI_LONG_LONG_INT, MPI_MAX, 0, comm_smp);

    debug_printf("Rank %d: size_master is %d rank_master is %d ...\n", t_mpi_rank, size_master, rank_master);

    // now Adjust "remote master" timers
    // Gather all hosts to write to PTPD host file
    if (size_master > 0) {
#ifdef _TRACTOR_USE_PTPD_
        char ptpd_fn[1024];
        FILE *ptpd_nodes;
        if (use_ptpd && !ninfo) {
            PMPI_Gather(localhost, MAX_HOSTNAME, MPI_BYTE, all_hosts, MAX_HOSTNAME, MPI_BYTE, 0, comm_master);
        }
#endif
        // if (ninfo) get_topo();
        // std::cerr << "abrashvabracadabra " << node_topology <<std::endl;
        if (!rank_smp) {
            int k;
            int displ[size_cw];
            int smp_sizes[size_cw];
            long long all_proc_freqs[2 * size_cw];
            long long all_cpu_proc_freqs[2 * size_cw];
            PEAT_Clock all_tscs[size_cw * 3];
            PMPI_Gather(proc_freq, 2, MPI_DOUBLE, all_proc_freqs, 2, MPI_DOUBLE, 0, comm_master);
            PMPI_Gather(cpu_proc_freq, 2, MPI_DOUBLE, all_cpu_proc_freqs, 2, MPI_DOUBLE, 0, comm_master);
            PMPI_Gather(localhost, MAX_HOSTNAME, MPI_BYTE, all_hosts, MAX_HOSTNAME, MPI_BYTE, 0, comm_master);
            PMPI_Gather(local_tscs, 3, MPI_DOUBLE, all_tscs, 3, MPI_DOUBLE, 0, comm_master);
            if (ninfo)
                get_topo();
            if (!rank_cw && ninfo) { // master
                for (i = 0; i < size_master; i++)
                    local_ranks[i] = i;
                PMPI_Group_translate_ranks(grp_master, size_master, local_ranks, grp_world, all_ranks);

                ninfo->allocate(size_master);
                for (i = 0; i < size_master; i++) {
                    ninfo->nodes[i].set_hostname(&all_hosts[i * MAX_HOSTNAME]);
                    ninfo->nodes[i].proc_freq[0] = all_proc_freqs[i * 2 + 0];
                    ninfo->nodes[i].proc_freq[1] = all_proc_freqs[i * 2 + 1];
                    ninfo->nodes[i].cpu_proc_freq[0] = all_cpu_proc_freqs[i * 2 + 0];
                    ninfo->nodes[i].cpu_proc_freq[1] = all_cpu_proc_freqs[i * 2 + 1];
                    ninfo->nodes[i].t_time[0] = all_tscs[i * 3 + 0];
                    ninfo->nodes[i].t_time[1] = all_tscs[i * 3 + 1];
                    ninfo->nodes[i].t_time[2] = all_tscs[i * 3 + 2];
                }
            }
            for (i = 0; i < size_smp; i++)
                local_ranks[i] = i;
            PMPI_Group_translate_ranks(grp_smp, size_smp, local_ranks, grp_world, all_ranks);
            memcpy(local_ranks, all_ranks, size_smp * sizeof(int));
            //          gather smp sizes from all master nodes
            PMPI_Allgather(&size_smp, 1, MPI_INT, smp_sizes, 1, MPI_INT, comm_master);

            //          mark displacements
            displ[0] = 0;
            for (i = 1; i < size_master; i++)
                displ[i] = displ[i - 1] + smp_sizes[i - 1];

            PMPI_Gatherv(local_ranks, size_smp, MPI_INT, all_ranks, smp_sizes, displ, MPI_INT, 0, comm_master);

            if (!rank_cw && ninfo) { // master
                for (i = 0, k = 0; i < size_master; i++) {
                    //                    debug_printf("SMP Size %d,%d\n", i, smp_sizes[i]);
                    ninfo->nodes[i].allocate(smp_sizes[i]);
                    for (j = 0; j < smp_sizes[i]; j++, k++) {
                        ninfo->nodes[i].mpi_ranks[j] = all_ranks[k];
                        //                        debug_printf("SMP Rank %d,%d,%d\n", i, j,all_ranks[k]);
                    }
                }
            }
        }
        if (!rank_cw && !ninfo) { // master
#ifdef _TRACTOR_USE_PTPD_
            if (use_ptpd) {
                if (ptpd_nodefile == NULL) {
                    sprintf(ptpd_fn, "%s/ptpd-nodes-%s", stat_dir, stat_template);
                    debug_printf("Rank 0 PTPD_Node_Init: PTPD nodefile is not specified... Using default (%s) name.\n", ptpd_fn);
                } else {
                    strcpy(ptpd_fn, ptpd_nodefile);
                    debug_printf("Rank 0 PTPD_Init: PTPD nodefile is specified... Using supplied (%s) name.\n", ptpd_fn);
                }
                ptpd_nodes = fopen(ptpd_fn, "w+b");
                if (ptpd_nodes) {
                    if (ptpd_timenode)
                        fprintf(ptpd_nodes, "%s\n", ptpd_timenode);
                    for (i = 0; i < size_master; i++)
                        fprintf(ptpd_nodes, "%s\n", &all_hosts[i * MAX_HOSTNAME]);
                    fclose(ptpd_nodes);
                }
            }
#endif
        }
    }
    PMPI_Barrier(TRACTOR_COMM_WORLD);
    debug_printf("Rank %d: msl_nodes_info finished...\n", t_mpi_rank);
    __DEBUG_EPILOG_TRACTOR_LAYER__
}

PEAT_Clock tractor::find_ELF_symbols(void)
{
    const PEAT_Clock time_start = rdtsca();
    int index = 0;
    elf_symbols symbol_table;
    for (std::map<std::string, t_bin>::iterator it = binaries_map.begin(); it != binaries_map.end(); ++it, index++) {
        PEAT_Addr low = it->second.load_addr, high = it->second.end_addr, base = it->second.base_addr;
        for (std::list<PEAT_Addr>::iterator addr_it = it->second.addrs.begin(); addr_it != it->second.addrs.end(); ++addr_it) {
            const PEAT_Addr &addr = *addr_it;
            t_address &t_addr = addr_map.at(addr);
            symbol_table.find_symbol(addr, index, base, it->first, t_addr.func_name, t_addr.source_name, t_addr.line_number);
        }
    }
    // symbol_table.dump_symbols(base_trace_path, base_trace_mask, global_stat.node_info.world_rank);
    return rdtsca() - time_start;
}

int tractor::write_trace(void)
{
    __DEBUG_PROLOG_TRACTOR_LAYER__
    // user_region_store l_user_region_store;
    //    if (t_write_main_files) {
    if (use_realtime_dump) {
        for (int i = 0; i < t_tid_counter; i++) {
            for (int level = 0; level < t_max_nesting_levels; level++) {
                if (events_chain[i][level]) {
                    events_chain[i][level]->num_events = event_index[i][level];
                    dump_events(i, level, &events_start_chain[i][level]);
                }
                if (extras_chain[i][level]) {
                    extras_chain[i][level]->num_events = extras_index[i][level];
                    dump_extras(i, level, &extras_start_chain[i][level]);
                }
            }
        }
    }
    //    }

    num_processed_events events_num = write_main_files(/*l_user_region_store*/);
    PEAT_Clock time_symbols_search = 0;
    // apply_shared_object_info_to_regions(l_user_region_store);
    time_symbols_search = find_ELF_symbols();
#ifdef _MEM_HOOKS_
    char filename[MAX_PATH];
    sprintf(filename, "%d.memprf%s", t_mpi_rank, gz_ext);
    PEAT_Clock memory_profiler_trace_write_len = mprof.fill_common_statistic(filename, gz_mode);
#endif
    write_com_file(events_num, /* l_user_region_store,*/ time_symbols_search);
    write_app_file(gt_ninfo);
    __DEBUG_EPILOG_TRACTOR_LAYER__
}

PEAT_Rank tractor::get_real_rank(MPI_Comm comm, PEAT_Rank rank_in, PEAT_Clock tsc, int is_internal)
{
    __DEBUG_PROLOG_TRACTOR_LAYER__
    if (resolve_comms_and_ranks) {
        if (rank_in == MPI_ROOT) {
            __DEBUG_EPILOG_TRACTOR_LAYER__
            return t_mpi_rank; // Assume it's correct
        }
        if (rank_in < 0) {
            __DEBUG_EPILOG_TRACTOR_LAYER__
            return rank_in;
        }
        std::list<t_comm>::iterator event_comm_it;
        if (comm == MPI_COMM_WORLD) {
            __DEBUG_EPILOG_TRACTOR_LAYER__
            return rank_in;
        }
        if (comm == MPI_COMM_SELF) {
            __DEBUG_EPILOG_TRACTOR_LAYER__
            return t_mpi_rank;
        }
        if (comm == MPI_COMM_NULL)  {
            __DEBUG_EPILOG_TRACTOR_LAYER__
            return rank_in;
        }
        const bool communicator_id_found = local_comms->get_local_comm_id(comm, tsc, is_internal, event_comm_it);
        if (communicator_id_found) {
            if (event_comm_it->kind == T_MPI_INTER_COMM) {
                if (get_val(event_comm_it->l_comm2world_ranks, t_mpi_rank) >= 0) {
                    if (event_comm_it->r_comm2world_ranks.size() > rank_in) {
                        __DEBUG_EPILOG_TRACTOR_LAYER__
                        return event_comm_it->r_comm2world_ranks.at(rank_in);
                    } else {
                        debug_printf("Destination rank %d is out of range for rank %d in intercomm %x, remote size %ld\n", rank_in, t_mpi_rank, comm, event_comm_it->r_comm2world_ranks.size());
                        __DEBUG_EPILOG_TRACTOR_LAYER__
                        return rank_in;
                    }
                }
                debug_printf("Something wrong happen: %d is not in local group in intercomm %x\n", t_mpi_rank, comm);
                if (get_val(event_comm_it->r_comm2world_ranks, t_mpi_rank) >= 0) {
                    // I suppose this shouldn't happen
                    if (event_comm_it->l_comm2world_ranks.size() > rank_in) {
                        __DEBUG_EPILOG_TRACTOR_LAYER__
                        return event_comm_it->l_comm2world_ranks.at(rank_in);
                    } else {
                        debug_printf("Destination rank %d is out of range for rank %d in intercomm %x local size %ld\n", rank_in, t_mpi_rank, comm, event_comm_it->l_comm2world_ranks.size());
                        __DEBUG_EPILOG_TRACTOR_LAYER__
                        return rank_in;
                    }
                }
            } else {
                // T_MPI_INTRA_COMM
                if (event_comm_it->l_comm2world_ranks.size() > rank_in) {
                    __DEBUG_EPILOG_TRACTOR_LAYER__
                    return event_comm_it->l_comm2world_ranks.at(rank_in);
                } else {
                    debug_printf("Destination rank %d is out of range for rank %d in intracomm %x local size %ld\n", rank_in, t_mpi_rank, comm, event_comm_it->l_comm2world_ranks.size());
                    __DEBUG_EPILOG_TRACTOR_LAYER__
                    return rank_in;
                }
            }
        }
        debug_printf("No comm %x has been found in rank %d\n", comm, t_mpi_rank);
        __DEBUG_EPILOG_TRACTOR_LAYER__
        return -rank_in; // to signalize error
    } else {
        __DEBUG_EPILOG_TRACTOR_LAYER__
        return rank_in;
    }
}
PEAT_MPI_Comm tractor::get_global_comm_id(MPI_Comm comm, PEAT_Clock tsc,int is_internal)
{
    __DEBUG_PROLOG_TRACTOR_LAYER__
    if (resolve_comms_and_ranks) {
        if (comm == MPI_COMM_WORLD) {
            __DEBUG_EPILOG_TRACTOR_LAYER__
            return 2;
        }
        if (comm == MPI_COMM_SELF)  {
            __DEBUG_EPILOG_TRACTOR_LAYER__
            return 1;
        }
        if (comm == MPI_COMM_NULL)  {
            __DEBUG_EPILOG_TRACTOR_LAYER__
            return 0;
        }
        __DEBUG_EPILOG_TRACTOR_LAYER__
        return local_comms->get_global_comm_id(comm, tsc, is_internal);
    } else {
        __DEBUG_EPILOG_TRACTOR_LAYER__
        return (PEAT_MPI_Comm)((PEAT_MPI_Long_Comm)comm);
        /*
        #ifndef OMPI_MPI_H
                return (PEAT_MPI_Comm) comm;
        #else
                PEAT_MPI_Comm *comm_ptr = (PEAT_MPI_Comm *) &comm;
                return (PEAT_MPI_Comm) *comm_ptr;
        #endif
        */
    }
    __DEBUG_EPILOG_TRACTOR_LAYER__
   return 0;
}

const std::string tractor::get_binary_by_addr(PEAT_Addr addr, PEAT_Addr &base, PEAT_Addr &low, PEAT_Addr &high)
{
    __DEBUG_PROLOG_TRACTOR_LAYER__
    std::string str = "<unknown>";
    for (std::map<PEAT_Num, shared_object_info>::const_iterator it = g_shared_objects.begin(); it != g_shared_objects.end(); ++it) {
        for (std::list<shared_object_header>::const_iterator header_it = it->second.dlpi_phdr.begin(); header_it != it->second.dlpi_phdr.end(); ++header_it) {
            const shared_object_header &elf_sec = *header_it;
            if (shared_object_header::is_exec_and_load(elf_sec.p_flags, elf_sec.p_type)) { // take executable, loadable section
                if (addr >= elf_sec.load_address && addr <= elf_sec.end_address) {
                    str = it->second.dlpi_name;
                    debug_printf("Rank: %d, addr %lld hits to %s\n", t_mpi_rank, addr, str.c_str());
                    base = elf_sec.base_address;
                    low = elf_sec.load_address;
                    high = elf_sec.end_address;
                    return str;
                }
            }
        }
    }
    debug_printf("Rank: %d, addr %lld doesn't hit anywhere\n", t_mpi_rank, addr);
    return str;
    __DEBUG_EPILOG_TRACTOR_LAYER__
}

bool is_coll_transfer(short x)
{
    if ((x >= MPI_P2P_COLL_TRANSFER_START && x < MPI_P2P_COLL_TRANSFER_END) ||
        (x >= MPI_P2P_COLL_PROBE_START && x < MPI_P2P_COLL_PROBE_END) ||
        (x >= MPI_P2P_COLL_REQUEST_START && x < MPI_P2P_COLL_REQUEST_END))
        return true;
    return false;
}
void tractor::update_extras_pointers(int tid, int level, long long *max_ext_count, msl_extra_info **cur_exts_array)
{
    FILE *fps_ext = fps_extras[fp_index(tid, level)];
    if (fps_ext) {
        *max_ext_count = fread(*cur_exts_array, sizeof(msl_extra_info), onetimealloc, fps_ext);
        debug_printf("TID %d level %d, %ld extras read back.\n", tid, level, *max_ext_count);
    } else {
        *max_ext_count = 0;
    }
}

#ifdef GPUPARSE

//    std::map<void *, T_LOC> addr_location;
//    std::map<short,std::map<T_LOC,std::map<T_LOC,std::map<PEAT_Message_Size, T_Stat_Record>>>> gpu_transfers; // func,from,to,size,time
//    std::map<short,PEAT_Clock> gpu_executions; // func,time
//    std::map<short,PEAT_Clock> gpu_syncs; // func,time
const char *directions[] = {
    "Host->Host",
    "Host->Device",
    "Host->Shared",
    "Device->Host",
    "Device->Device",
    "Device->Shared",
    "Shared->Host",
    "Shared->Device",
    "Shared->Host"};
void tractor::print_gpu_stat(void)
{
    __DEBUG_PROLOG_TRACTOR_LAYER__
    gzprintf(fps_com, "%d:GPU Statistics:\n", PEAT_RT_Remark);
    long long total = 0;
    SEPARATOR(2 + 34 + 16 + 16 + 16 + 16 + 3);
    gzprintf(fps_com, "%d:%34s|%16s|%16s|%16s|%16s|\n", PEAT_RT_Remark, "Function", "Direction", "Size", "Occurences", "Duration");
    for (std::map<short, std::map<T_LOC, std::map<T_LOC, std::map<PEAT_Message_Size, T_Stat_Record>>>>::const_iterator func_it = gpu_transfers.cbegin(); func_it != gpu_transfers.cend(); ++func_it) {
        for (std::map<T_LOC, std::map<T_LOC, std::map<PEAT_Message_Size, T_Stat_Record>>>::const_iterator from_it = func_it->second.cbegin(); from_it != func_it->second.cend(); ++from_it) {
            for (std::map<T_LOC, std::map<PEAT_Message_Size, T_Stat_Record>>::const_iterator to_it = from_it->second.cbegin(); to_it != from_it->second.cend(); ++to_it) {
                for (std::map<PEAT_Message_Size, T_Stat_Record>::const_iterator message_it = to_it->second.cbegin(); message_it != to_it->second.cend(); ++message_it) {
                    const T_Stat_Record &record = message_it->second;
                    int index = ((int)from_it->first) * 3 + (int)(to_it->first);
                    gzprintf(fps_com, "%d:%34s|%16s|%16lld|%16lld|%16lld|\n", PEAT_RT_Remark, mpi_function_names[func_it->first], directions[index], message_it->first, message_it->second.occurences, message_it->second.duration);
                }
            }
        }
    }
    SEPARATOR(2 + 34 + 16 + 16 + 16 + 16 + 3);
    for (std::map<short, PEAT_Clock>::const_iterator func_it = gpu_executions.cbegin(); func_it != gpu_executions.cend(); ++func_it) {
        gzprintf(fps_com, "%d:%34s|%16lld|\n", PEAT_RT_Remark, mpi_function_names[func_it->first], func_it->second);
    }
    for (std::map<short, PEAT_Clock>::const_iterator func_it = gpu_syncs.cbegin(); func_it != gpu_syncs.cend(); ++func_it) {
        gzprintf(fps_com, "%d:%34s|%16lld|\n", PEAT_RT_Remark, mpi_function_names[func_it->first], func_it->second);
    }
    SEPARATOR(2 + 32 + 16 + 2);

    __DEBUG_EPILOG_TRACTOR_LAYER__
}
#endif

struct T_MPI_Comm {
    MPI_Comm comm;
    long long time;
    int is_internal;
    T_MPI_Comm(MPI_Comm c, long long t, int i) : comm(c), time(t), is_internal(i) {};
    T_MPI_Comm() : comm(MPI_COMM_NULL), time(-1), is_internal(0) {};
    T_MPI_Comm(const T_MPI_Comm &s) : comm(s.comm), time(s.time), is_internal(s.is_internal) {};
};


num_processed_events tractor::write_main_files(/*user_region_store &user_region_store*/)
{
    __DEBUG_PROLOG_TRACTOR_LAYER__
    num_processed_events ret;
    if (calls_limit == 0)
        return ret;
    std::map<MPI_Request, T_MPI_Comm> req_comm;
    std::map<MPI_Win, MPI_Comm> win_comm;
    // int tsc_mpi_entry_ind = 1;
    // int tsc_mpi_exit_ind = 2;
    long processed_events = 0;
    static PEAT_Clock tsc_counter = 0; /* start of the previously written record: starts are written relative to it */
    static long global_id = 0;
    const int levels = t_max_nesting_levels;
    /* id columns: the running index of the top-level record, then the index of the record among the children
       of its parent, one column per nesting level; at least two columns so flat traces keep their old layout */
    const int id_columns = t_max(2, t_achieved_nesting_levels);
    const int slots = t_max_threads * levels; /* cursor per (thread, level): index tid * levels + level */

    std::vector<msl_adv_chain *> current_chain(slots, (msl_adv_chain *)NULL);
    std::vector<msl_base_chain *> current_extras_chain(slots, (msl_base_chain *)NULL);
    std::vector<msl_event *> current_array(slots, (msl_event *)NULL);
    std::vector<void **> current_addrs_array(slots, (void **)NULL);
    std::vector<msl_extra_info *> current_extras_array(slots, (msl_extra_info *)NULL);
    std::vector<long long> record_counter(slots, 0), max_events(slots, 0), max_extras(slots, 0), extras_counter(slots, 0);
#ifdef _HWCOUNTERS_ENABLED_
    std::vector<long long *> current_hwc_array(slots, (long long *)NULL);
    long long hwc_counter[t_max_threads][num_hwcs];
#endif
#ifdef _MEMPROFILE_STORE_
    std::vector<t_memory_event *> current_mems_array(slots, (t_memory_event *)NULL);
#endif

    long long uniq_index = 0;
    long max_thread_id = 0;
    std::vector<long long> ids(id_columns, -1);
    std::map<t_timeline, int> timeline;
    t_address t_addr;
    t_addr.id = global_id++;
    t_addr.binary = "libmpi.so";
    t_addr.func_name = "??";
    t_addr.source_name = "??";
    t_addr.line_number = 0;
    addr_map[0] = t_addr;

    if (t_write_main_files)
        fps_mpi = gzopen(mpi_filename, gz_mode);
    if (t_write_main_files)
        fps_hwc = gzopen(hwc_filename, gz_mode);

    /* (re)load the current block of a (thread, level) stream; realtime mode reads it back from the temporary file */
    auto load_block = [&](int tid, int level, bool first) -> void {
        const int idx = tid * levels + level;
        record_counter[idx] = 0;
        if (use_realtime_dump) {
            FILE *fp = fps_events[fp_index(tid, level)];
            max_events[idx] = 0;
            if (fp == NULL)
                return;
            if (first) {
                rewind(fp);
                current_array[idx] = (msl_event *)t_malloc((onetimealloc + 1) * sizeof(msl_event), 128);
                current_addrs_array[idx] = (void **)t_malloc((onetimealloc + 1) * sizeof(void *) * num_addrs, 128);
#ifdef _HWCOUNTERS_ENABLED_
                current_hwc_array[idx] = (long long *)t_malloc((onetimealloc + 1) * sizeof(long long) * num_hwcs * 2, 128);
#endif
#ifdef _MEMPROFILE_STORE_
                current_mems_array[idx] = (t_memory_event *)t_malloc((onetimealloc + 1) * t_memory_event_size * 2, 128);
#endif
            }
            if (fread(&max_events[idx], 4, 1, fp) != 1) {
                max_events[idx] = 0;
                return;
            }
            long tmp_mi = fread(current_array[idx], sizeof(msl_event), max_events[idx], fp);
            if (tmp_mi != max_events[idx])
                debug_printf("TID %d level %d, %ld!=%ld events read count mismatch.\n", tid, level, max_events[idx], tmp_mi);
            tmp_mi = fread(current_addrs_array[idx], sizeof(void *) * num_addrs, max_events[idx], fp);
            if (tmp_mi != max_events[idx])
                debug_printf("TID %d level %d, %ld!=%ld events read count mismatch.\n", tid, level, max_events[idx], tmp_mi);
#ifdef _HWCOUNTERS_ENABLED_
            tmp_mi = fread(current_hwc_array[idx], sizeof(long long) * num_hwcs * 2, max_events[idx], fp);
            if (tmp_mi != max_events[idx])
                debug_printf("TID %d level %d, %ld!=%ld events read count mismatch.\n", tid, level, max_events[idx], tmp_mi);
#endif
#ifdef _MEMPROFILE_STORE_
            tmp_mi = fread(current_mems_array[idx], t_memory_event_size * 2, max_events[idx], fp);
            if (tmp_mi != max_events[idx])
                debug_printf("TID %d level %d, %ld!=%ld events read count mismatch.\n", tid, level, max_events[idx], tmp_mi);
#endif
            debug_printf("TID %d level %d, %ld events read back.\n", tid, level, max_events[idx]);
        } else {
            if (first) {
                current_chain[idx] = events_start_chain[tid][level];
            } else if (current_chain[idx]) {
                current_chain[idx] = (msl_adv_chain *)current_chain[idx]->next;
            }
            if (current_chain[idx] == NULL) {
                max_events[idx] = 0;
                return;
            }
            max_events[idx] = (current_chain[idx]->next == NULL) ? event_index[tid][level] : onetimealloc;
            current_array[idx] = (msl_event *)current_chain[idx]->events_array;
            current_addrs_array[idx] = (void **)current_chain[idx]->addr_array;
#ifdef _HWCOUNTERS_ENABLED_
            current_hwc_array[idx] = (long long *)current_chain[idx]->hwc_array;
#endif
#ifdef _MEMPROFILE_STORE_
            current_mems_array[idx] = (t_memory_event *)current_chain[idx]->mem_array;
#endif
        }
    };

    for (int i = 0; i < t_tid_counter; i++) {
#ifdef _HWCOUNTERS_ENABLED_
        for (int hwcs_ind = 0; hwcs_ind < num_hwcs; hwcs_ind++) {
            hwc_counter[i][hwcs_ind] = 0;
        }
#endif
        for (int level = 0; level < levels; level++) {
            const int idx = i * levels + level;
            load_block(i, level, true);
            extras_counter[idx] = 0;
            if (use_realtime_dump) {
                FILE *fpx = fps_extras[fp_index(i, level)];
                current_extras_array[idx] = (msl_extra_info *)t_malloc((onetimealloc + 1) * sizeof(msl_extra_info), 128);
                max_extras[idx] = 0;
                if (fpx) {
                    rewind(fpx);
                    max_extras[idx] = fread(current_extras_array[idx], sizeof(msl_extra_info), onetimealloc, fpx);
                }
            } else {
                current_extras_chain[idx] = extras_start_chain[i][level];
                if (current_extras_chain[idx]) {
                    current_extras_array[idx] = (msl_extra_info *)current_extras_chain[idx]->events_array;
                    max_extras[idx] = (current_extras_chain[idx]->next == NULL) ? extras_index[i][level] : onetimealloc;
                } else {
                    max_extras[idx] = 0;
                }
            }
        }
        const int idx0 = i * levels;
        if (max_events[idx0] > 0) {
            if (!use_realtime_dump && (current_array[idx0][0].tscs[0] == 0 || current_array[idx0][0].func_id == 0)) {
                std::ostringstream err_txt;
                err_txt << "Internal event storage error (extra empty thread) " << i << "." << std::endl;
                throw std::runtime_error(err_txt.str());
            }
            timeline[t_timeline(current_array[idx0][0].tscs[0], i)] = i;
        }
    }

    /* write one record of (tid, level) with the given ids; returns its number of children */
    auto emit_record = [&](int tid, int level, bool second_line) -> long long {
        const int idx = tid * levels + level;
        const long long rc = record_counter[idx];
        msl_event &rec = current_array[idx][rc];
        void **cur_addrs = current_addrs_array[idx];
#ifdef _HWCOUNTERS_ENABLED_
        long long *cur_hwc = current_hwc_array[idx];
#endif
#ifdef _MEMPROFILE_STORE_
        t_memory_event *cur_mems = current_mems_array[idx];
#endif
        const short func_base = mpi_func_ids_trans[rec.func_id];
        const long long children = second_line ? 0 : rec.num_children;
        processed_events++;
        for (int k = 0; k < id_columns; k++) {
            t_gzprintf(fps_mpi, k == 0 ? "%lld" : ":%lld", ids[k]);
            t_gzprintf(fps_hwc, k == 0 ? "%lld" : ":%lld", ids[k]);
        }
        if (print_names) {
            t_gzprintf(fps_mpi, ":%d:%lld:%s", tid, children, mpi_function_names[rec.func_id]);
        } else {
            t_gzprintf(fps_mpi, ":%d:%lld:%d", tid, children, (int)func_base);
        }

        if (max_thread_id < tid)
            max_thread_id = tid;
        PEAT_Clock l_tscs[4];
        //perform local conversion instead of doing it in 1000 other places
        l_tscs[0] = rec.tscs[0] - tsc_counter;
        l_tscs[1] = rec.tscs[1] - rec.tscs[0];
        l_tscs[2] = rec.tscs[3] - rec.tscs[2];
        l_tscs[3] = rec.tscs[3] - rec.tscs[0];
        tsc_counter = rec.tscs[0];
        long long l_tsc_counter = rec.tscs[0]; // for comm queries
        gzprintf(fps_hwc, ":%lld:%lld:%lld:%lld", l_tscs[0], l_tscs[1], l_tscs[2], l_tscs[3]);

            for (int addr_ind = 0; addr_ind < num_addrs; addr_ind++) {
                PEAT_Addr addr = cur_addrs[rc * num_addrs + addr_ind];
                // debug_printf("Rank:%d  Addr(ind) = %lx(%d)\n",t_mpi_rank,addr,addr_ind);
                if (addr_map.count(addr) > 0) {
                    t_gzprintf(fps_hwc, ":%d", addr_map.at(addr).id);
                } else {
                    t_address t_addr;
                    t_addr.id = global_id++;
                    // todo add line number
                    PEAT_Addr low, high, base;
                    t_addr.binary = get_binary_by_addr(addr, base, low, high);
                    //                   debug_printf("Rank: %d, addr %lld hits to %s(maps)\n",t_mpi_rank,addr,t_addr.binary.c_str());
                    t_gzprintf(fps_hwc, ":%d", t_addr.id);
                    binaries_map[t_addr.binary].addrs.push_back(addr);
                    binaries_map[t_addr.binary].base_addr = base;
                    binaries_map[t_addr.binary].load_addr = low;
                    binaries_map[t_addr.binary].end_addr = high;
                    addr_map[addr] = t_addr;
                }
            }

    #ifdef _HWCOUNTERS_ENABLED_
            for (int hwcs_ind = 0; hwcs_ind < num_hwcs * 2; hwcs_ind++) {
                long long chwc = cur_hwc[rc * num_hwcs * 2 + hwcs_ind];
                t_gzprintf(fps_hwc, ":%lld", chwc
    #if 1
                                                 - hwc_counter[tid][hwcs_ind % num_hwcs]
    #endif
                );
                if (hwcs_ind < num_hwcs)
                    hwc_counter[tid][hwcs_ind] = cur_hwc[rc * num_hwcs * 2 + hwcs_ind];
            }
    #endif
    #ifdef _MEMPROFILE_STORE_
            t_gzprintf(fps_hwc, ":%ld:%ld:%ld:%ld:%ld:%ld:%ld:%ld",
                       cur_mems[rc * 2].vm_size,
                       cur_mems[rc * 2].vm_RSS,
                       cur_mems[rc * 2].ru_maxrss,
                       cur_mems[rc * 2].mem_used_by_tractor,
                       cur_mems[rc * 2 + 1].vm_size,
                       cur_mems[rc * 2 + 1].vm_RSS,
                       cur_mems[rc * 2 + 1].ru_maxrss,
                       cur_mems[rc * 2 + 1].mem_used_by_tractor);
    #endif
            PEAT_Clock tsc_mpi_entry = rec.tscs[1];
            PEAT_Clock tsc_mpi_exit = rec.tscs[2];
            long long *ext_count = &extras_counter[idx];
            long long *max_ext_count = &max_extras[idx];
            int *ext_index = &extras_index[tid][level];
            msl_base_chain **cur_exts = &current_extras_chain[idx];
            msl_extra_info **cur_exts_array = &current_extras_array[idx];
            /***********************************************************************************************************************************************************************************************/

            if (func_base == MPI_PCONTROL_ID) {
                t_gzprintf(fps_mpi, ":%d", rec.pcontrol.ctl); // 4
            } // special case for pcontrol

            /***********************************************************************************************************************************************************************************************/

            if (func_base >= MPI_START_FILE_GROUP && func_base < MPI_END_FILE_GROUP) {
    #if 0
                    if(func_base==MPI_FILE_OPEN_ID) {
                        t_gzprintf(fps_mpi, ":%x:%llx",
                                 get_global_comm_id(rec.mpi_fio.comm,
                                 tsc_mpi_entry, 0),            // 4
                                 rec.mpi_fio.fh                // 5
                                 );
                    } else {
                        t_gzprintf(fps_mpi, ":%llx:%lld:%llx:%lld",
                                 rec.mpi_fio.fh,                // 4
                                 rec.mpi_fio.offset,            // 5
                                 MPI_Check_dummy(rec.mpi_fio.request),           // 6
                                 rec.mpi_fio.transfer_bytes     // 7
                                 );
                    }
    #else
                t_gzprintf(fps_mpi, ":%llx:%llx:%llx:%lld:%lld",
                           rec.mpi_fio.fh, // 4
                           get_global_comm_id(rec.mpi_fio.comm,
                                              tsc_mpi_entry, 0), // 5
                           MPI_Check_dummy(rec.mpi_fio.request), // 6
                           rec.mpi_fio.offset,                   // 7
                           rec.mpi_fio.transfer_bytes            // 8
                );
    #endif
            }
            int stat_processed = 0;
            if (func_base >= MPI_START_BUFFERED_LOCAL_GROUP && func_base < MPI_END_BUFFERED_LOCAL_GROUP) {
                t_gzprintf(fps_mpi, ":%lld:%lld",
                           rec.buf_local.count, // 4
                           rec.buf_local.size   // 5
                );
    #ifdef _T_PER_INTERVAL_STAT_WRITE_
                int group = gt_func_stat.get_group(rec.buf_local.size);
                int func_id_t = rec.func_id;
                gt_func_stat.stat_for_func[func_id_t].durations[group] += tsc_mpi_exit - tsc_mpi_entry;
                gt_func_stat.stat_for_func[func_id_t].occurences[group]++;
                gt_func_stat.stat_for_func[func_id_t].sizes[group] += rec.buf_local.size;
                stat_processed = 1;
    #endif // _T_PER_INTERVAL_STAT_WRITE_
            }

    /***********************************************************************************************************************************************************************************************/
    #ifdef _T_OMPT_
            if (func_base >= OMPT_START_GROUP && func_base < OMPT_END_GROUP) {
                t_gzprintf(fps_mpi, ":%d:%d:%lld:%lld",
                           rec.ompt.kind,         // 4
                           rec.ompt.endpoint,     // 5
                           rec.ompt.task_data,    // 6
                           rec.ompt.parallel_data // 7
                );
                PEAT_Addr addr = rec.ompt.pcode;
                if (addr_map.count(addr) > 0) {
                    t_gzprintf(fps_mpi, ":%d", addr_map.at(addr).id);
                } else {
                    t_address t_addr;
                    t_addr.id = global_id++;
                    // todo add line number
                    PEAT_Addr low, high, base;
                    t_addr.binary = get_binary_by_addr(addr, base, low, high);
                    //                       debug_printf("Rank: %d, addr %lld hits to %s(maps)\n",t_mpi_rank,addr,t_addr.binary.c_str());
                    t_gzprintf(fps_mpi, ":%d", t_addr.id);
                    binaries_map[t_addr.binary].addrs.push_back(addr);
                    binaries_map[t_addr.binary].base_addr = base;
                    binaries_map[t_addr.binary].load_addr = low;
                    binaries_map[t_addr.binary].end_addr = high;
                    addr_map[addr] = t_addr;
                }
            }
    #endif
    /***********************************************************************************************************************************************************************************************/
    #ifdef GPUPARSE
            if (func_base >= CL_START_GROUP && func_base < CL_END_GROUP ||
                func_base >= ZE_START_GROUP && func_base < ZE_END_GROUP) {
                t_gzprintf(fps_mpi, ":%d:%d:%d",
                           rec.gpu.num_arguments,      //
                           rec.gpu.num_out_arguments,  //
                           rec.gpu.num_extra_arguments //
                );

                int in_args_num = rec.gpu.num_arguments > 0 ? rec.gpu.num_arguments : 1;
                int out_args_num = rec.gpu.num_out_arguments > 0 ? rec.gpu.num_out_arguments : 1;
                int extra_args_num = rec.gpu.num_extra_arguments > 0 ? rec.gpu.num_extra_arguments : 1;

                long long in_args[in_args_num], out_args[out_args_num], extra_args[extra_args_num];
                for (int j = 0; j < rec.gpu.num_arguments; j++, (*ext_count)++) { /* write input here */
                    if (*ext_count >= *max_ext_count && *max_ext_count > 0) {
                        if (use_realtime_dump) {
                            update_extras_pointers(tid, level, max_ext_count, cur_exts_array);
                        } else {
                            *cur_exts = (*cur_exts)->next;
                            *max_ext_count = ((*cur_exts)->next == NULL) ? *ext_index : onetimealloc;
                            *cur_exts_array = (msl_extra_info *)(*cur_exts)->events_array;
                        }
                        *ext_count = 0;
                    }
                    in_args[j] = (*cur_exts_array)[*ext_count].value;
                    t_gzprintf(fps_mpi, ":%llx", in_args[j]);
                }

                for (int j = 0; j < rec.gpu.num_out_arguments; j++, (*ext_count)++) { /* write output here */
                    if (*ext_count >= *max_ext_count && *max_ext_count > 0) {
                        if (use_realtime_dump) {
                            update_extras_pointers(tid, level, max_ext_count, cur_exts_array);
                        } else {
                            *cur_exts = (*cur_exts)->next;
                            *max_ext_count = ((*cur_exts)->next == NULL) ? *ext_index : onetimealloc;
                            *cur_exts_array = (msl_extra_info *)(*cur_exts)->events_array;
                        }
                        *ext_count = 0;
                    }
                    out_args[j] = (*cur_exts_array)[*ext_count].value;
                    t_gzprintf(fps_mpi, ":%llx", out_args[j]);
                }
                for (int j = 0; j < rec.gpu.num_extra_arguments; j++, (*ext_count)++) { /* write output here */
                    if (*ext_count >= *max_ext_count && *max_ext_count > 0) {
                        if (use_realtime_dump) {
                            update_extras_pointers(tid, level, max_ext_count, cur_exts_array);
                        } else {
                            *cur_exts = (*cur_exts)->next;
                            *max_ext_count = ((*cur_exts)->next == NULL) ? *ext_index : onetimealloc;
                            *cur_exts_array = (msl_extra_info *)(*cur_exts)->events_array;
                        }
                        *ext_count = 0;
                    }
                    extra_args[j] = (*cur_exts_array)[*ext_count].value;
                    t_gzprintf(fps_mpi, ":%llx", extra_args[j]);
                }

                short func_id = rec.func_id;
                PEAT_Message_Size size = 0;
                T_LOC from, to;
                void *ptr = NULL;
                void *src_ptr = NULL;
                void *dst_ptr = NULL;
                switch (func_id) {
                case ZECOMMANDQUEUESYNCHRONIZE_ID_THROUGH:
                    // TODO: parse handles to sort times out
                    gpu_syncs[func_id] += tsc_mpi_exit - tsc_mpi_entry;
                    break;
                case ZEEVENTHOSTSYNCHRONIZE_ID_THROUGH:
                    gpu_syncs[func_id] += tsc_mpi_exit - tsc_mpi_entry;
                    break;
                case ZEMEMALLOCDEVICE_ID_THROUGH:
                    ptr = (void *)out_args[0];
                    addr_location[ptr] = T_LOC_DEVICE;
                    break;
                case ZEMEMALLOCSHARED_ID_THROUGH:
                    ptr = (void *)out_args[0];
                    addr_location[ptr] = T_LOC_SHARED;
                    break;
                case ZEMEMALLOCHOST_ID_THROUGH:
                    ptr = (void *)out_args[0];
                    addr_location[ptr] = T_LOC_HOST;
                    break;
                case ZECOMMANDLISTAPPENDMEMORYCOPY_ID_THROUGH:
                    dst_ptr = (void *)in_args[1];
                    src_ptr = (void *)in_args[2];
                    if (addr_location.count(dst_ptr) == 1 && addr_location.count(src_ptr) == 1) {
                        size = in_args[3];
                        to = addr_location.at(dst_ptr);
                        from = addr_location.at(src_ptr);
                        T_Stat_Record sr(tsc_mpi_exit - tsc_mpi_entry, 1);
                        gpu_transfers[func_id][from][to][size] += sr;
                    } else {
                        debug_printf("Unknown Address %p or %p\n", dst_ptr, src_ptr);
                    }
                    break;
                case ZECOMMANDLISTAPPENDMEMORYFILL_ID_THROUGH:
                    ptr = (void *)in_args[1];
                    if (addr_location.count(ptr) == 1) {
                        size = in_args[4];
                        to = addr_location.at(ptr);
                        T_Stat_Record sr(tsc_mpi_exit - tsc_mpi_entry, 1);
                        gpu_fills[func_id][to][size] += sr;
                    } else {
                        debug_printf("Unknown Address %p\n", ptr);
                    }
                    break;
                    // TODO: Add handlers later
                case ZECOMMANDLISTAPPENDMEMORYCOPYREGION_ID_THROUGH:
                    break;
                case ZECOMMANDLISTAPPENDMEMORYCOPYFROMCONTEXT_ID_THROUGH:
                    break;
                case ZECOMMANDLISTAPPENDIMAGECOPY_ID_THROUGH:
                    break;
                case ZECOMMANDLISTAPPENDIMAGECOPYREGION_ID_THROUGH:
                    break;
                case ZECOMMANDLISTAPPENDIMAGECOPYTOMEMORY_ID_THROUGH:
                    break;
                case ZECOMMANDLISTAPPENDIMAGECOPYFROMMEMORY_ID_THROUGH:
                    break;
                case ZECOMMANDLISTAPPENDMEMORYPREFETCH_ID_THROUGH:
                    break;
                    // END OF TODO: Add handlers later
                case ZEMODULECREATE_ID_THROUGH:
                    gpu_executions[func_id] += tsc_mpi_exit - tsc_mpi_entry;
                    break;
                case ZECOMMANDQUEUEEXECUTECOMMANDLISTS_ID_THROUGH:
                    gpu_executions[func_id] += tsc_mpi_exit - tsc_mpi_entry;
                    break;
                default:
                    break;
                }

    #ifdef _T_PER_INTERVAL_STAT_WRITE_
                if (size > 0) {
                    int group = gt_func_stat.get_group(size);
                    int func_id_t = rec.func_id;
                    gt_func_stat.stat_for_func[func_id_t].durations[group] += tsc_mpi_exit - tsc_mpi_entry;
                    gt_func_stat.stat_for_func[func_id_t].occurences[group]++;
                    gt_func_stat.stat_for_func[func_id_t].sizes[group] += size;
                    stat_processed = 1;
                }
    #endif // _T_PER_INTERVAL_STAT_WRITE_
            }
    #endif
            /***********************************************************************************************************************************************************************************************/

            if (func_base >= MPI_START_COMM_COLLECTIVE_GROUP &&
                    func_base < MPI_END_COMM_COLLECTIVE_GROUP ||
                func_base == MPI_COMM_FREE_ID || func_base == MPID_COMM_FREE_ID || func_base == MPID_COMM_CREATE_ID) {
                int is_internal = (func_base == MPID_COMM_FREE_ID || func_base == MPID_COMM_CREATE_ID);
                t_gzprintf(fps_mpi, ":%x:%x:%x:%llx",
                           get_global_comm_id(rec.comms.comm, tsc_mpi_entry, is_internal),       // 4
                           get_global_comm_id(rec.comms.newcomm, tsc_mpi_entry, is_internal),    // 5
                           get_global_comm_id(rec.comms.remotecomm, tsc_mpi_entry, is_internal), // 6
                           MPI_Check_dummy(rec.comms.request)                                    // 7
                );
                req_comm[MPI_Check_dummy(rec.comms.request)] = T_MPI_Comm(rec.comms.newcomm, tsc_mpi_entry, 0); // save comm for later WAIT or TEST
            }

            /***********************************************************************************************************************************************************************************************/

            if (func_base >= MPI_P2P_TRANSFER_START && func_base < MPI_P2P_TRANSFER_END ||
                func_base >= MPI_P2P_PERSISTENT_INIT_START && func_base < MPI_P2P_PERSISTENT_INIT_END ||
                func_base >= MPI_PROBE_START && func_base < MPI_PROBE_END) {
                // peer and other   5   6   7  8  9 10
                t_gzprintf(fps_mpi, ":%x:%d:%llx:%llx:%d:%lld",
                           get_global_comm_id(rec.p2p_pr_pb.comm,
                                              tsc_mpi_entry, 0),   // 4
                           get_real_rank(rec.p2p_pr_pb.comm,
                                         rec.p2p_pr_pb.peer,
                                         tsc_mpi_entry, 0),        // 5
                           MPI_Check_dummy(rec.p2p_pr_pb.request), // 6
                           rec.p2p_pr_pb.message,                  // 7
                           rec.p2p_pr_pb.tag,                      // 8
                           rec.p2p_pr_pb.transfer_bytes            // 9
                );

    #ifdef _T_PER_INTERVAL_STAT_WRITE_
                int group = gt_func_stat.get_group(rec.p2p_pr_pb.transfer_bytes);
                int func_id_t = rec.func_id;
                gt_func_stat.stat_for_func[func_id_t].durations[group] += tsc_mpi_exit - tsc_mpi_entry;
                gt_func_stat.stat_for_func[func_id_t].occurences[group]++;
                gt_func_stat.stat_for_func[func_id_t].sizes[group] += rec.p2p_pr_pb.transfer_bytes;
                stat_processed = 1;
    #endif                                                                                                                                                                                                // _T_PER_INTERVAL_STAT_WRITE_
                req_comm[MPI_Check_dummy(rec.p2p_pr_pb.request)] = T_MPI_Comm(rec.p2p_pr_pb.comm, tsc_mpi_entry, 0); // save comm for later WAIT or TEST
            }

            /***********************************************************************************************************************************************************************************************/

            if (func_base >= MPI_P2P_COLL_TRANSFER_START && func_base < MPI_P2P_COLL_TRANSFER_END ||
                func_base >= MPI_P2P_COLL_PROBE_START && func_base < MPI_P2P_COLL_PROBE_END) {
                // peer and other   5   6   7  8  9 10
#if 0
                // do not try to resolve ranks for collective transfers
                t_gzprintf(fps_mpi, ":%x:%d:%llx:%llx:%d:%lld",
                           get_global_comm_id(rec.p2p_pr_pb.comm,
                                              tsc_mpi_entry, 0),   // 4
                           rec.p2p_pr_pb.peer,                     // 5
                           MPI_Check_dummy(rec.p2p_pr_pb.request), // 6
                           rec.p2p_pr_pb.message,                  // 7
                           rec.p2p_pr_pb.tag,                      // 8
                           rec.p2p_pr_pb.transfer_bytes            // 9
                );
#else
                // try to resolve ranks for collective transfers
                t_gzprintf(fps_mpi, ":%x:%d:%llx:%llx:%d:%lld",
                           get_global_comm_id(rec.p2p_pr_pb.comm,
                                              tsc_mpi_entry, 1),   // 4
                           get_real_rank(rec.p2p_pr_pb.comm,
                                         rec.p2p_pr_pb.peer,
                                         tsc_mpi_entry, 1),        // 5
                           MPI_Check_dummy(rec.p2p_pr_pb.request), // 6
                           rec.p2p_pr_pb.message,                  // 7
                           rec.p2p_pr_pb.tag,                      // 8
                           rec.p2p_pr_pb.transfer_bytes            // 9
                );
#endif

    #ifdef _T_PER_INTERVAL_STAT_WRITE_
                int group = gt_func_stat.get_group(rec.p2p_pr_pb.transfer_bytes);
                int func_id_t = rec.func_id;
                gt_func_stat.stat_for_func[func_id_t].durations[group] += tsc_mpi_exit - tsc_mpi_entry;
                gt_func_stat.stat_for_func[func_id_t].occurences[group]++;
                gt_func_stat.stat_for_func[func_id_t].sizes[group] += rec.p2p_pr_pb.transfer_bytes;
                stat_processed = 1;
    #endif                                                                                                                                                                                                // _T_PER_INTERVAL_STAT_WRITE_
                req_comm[MPI_Check_dummy(rec.p2p_pr_pb.request)] = T_MPI_Comm(rec.p2p_pr_pb.comm, tsc_mpi_entry, 1); // save comm for later WAIT or TEST
            }

            if (func_base >= MPI_START_OSC_GROUP && func_base < MPI_END_OSC_GROUP) {
                // peer and other   5   6   7  8  9 10
                MPI_Comm comm = rec.one_sided_comm.comm;
                if (comm == 0) {
                    comm = win_comm[rec.one_sided_comm.window];
                } else {
                    win_comm[rec.one_sided_comm.window] = comm;
                }
                t_gzprintf(fps_mpi, ":%x:%d:%llx:%llx:%d:%lld",
                           get_global_comm_id(comm, tsc_mpi_entry, 0),  // 4
                           get_real_rank(comm,
                                         rec.one_sided_comm.peer,
                                         tsc_mpi_entry, 0),             // 5
                           MPI_Check_dummy(rec.one_sided_comm.request), // 6
                           rec.one_sided_comm.window,                   // 7
                           rec.one_sided_comm.size,                     // 8
                           rec.one_sided_comm.transfer_bytes            // 9
                );
    #ifdef _T_PER_INTERVAL_STAT_WRITE_
                int group = gt_func_stat.get_group(rec.one_sided_comm.transfer_bytes);
                int func_id_t = rec.func_id;
                gt_func_stat.stat_for_func[func_id_t].durations[group] += tsc_mpi_exit - tsc_mpi_entry;
                gt_func_stat.stat_for_func[func_id_t].occurences[group]++;
                gt_func_stat.stat_for_func[func_id_t].sizes[group] += rec.one_sided_comm.transfer_bytes;
                stat_processed = 1;
    #endif                                                                                                                                                                                                          // _T_PER_INTERVAL_STAT_WRITE_
                req_comm[MPI_Check_dummy(rec.one_sided_comm.request)] = T_MPI_Comm(rec.one_sided_comm.comm, tsc_mpi_entry, 0); // save comm for later WAIT or TEST
            }

            /***********************************************************************************************************************************************************************************************/

            if (func_base >= MPI_REQUEST_START && func_base < MPI_REQUEST_END ||
                func_base >= MPI_MISC_REQUEST_START && func_base < MPI_MISC_REQUEST_END ||
                func_base >= MPI_P2P_COLL_REQUEST_START && func_base < MPI_P2P_COLL_REQUEST_END) {
                // peer and other   5  6  7   8
                t_gzprintf(fps_mpi, ":%d:%d:%d:%llx",
                           rec.requests.input_request_num,     // 4
                           rec.requests.triggered_request_num, // 5
                           rec.requests.flag,                  // 6
                           rec.requests.request_addr           // 7
                );
                MPI_Request input_requests[rec.requests.input_request_num];
                for (int j = 0; j < rec.requests.input_request_num; j++, (*ext_count)++) { /* write input requests here */
                    if (*ext_count >= *max_ext_count && *max_ext_count > 0) {
                        if (use_realtime_dump) {
                            update_extras_pointers(tid, level, max_ext_count, cur_exts_array);
                        } else {
                            *cur_exts = (*cur_exts)->next;
                            *max_ext_count = ((*cur_exts)->next == NULL) ? *ext_index : onetimealloc;
                            *cur_exts_array = (msl_extra_info *)(*cur_exts)->events_array;
                        }
                        *ext_count = 0;
                    }
                    input_requests[j] = MPI_Check_dummy((MPI_Request)(*cur_exts_array)[*ext_count].values[0]);
                    if ((*cur_exts_array)[*ext_count].values[1] == -1) {
                        t_gzprintf(fps_mpi, ":%x", (*cur_exts_array)[*ext_count].values[0]);
                    } else {
                        t_gzprintf(fps_mpi, ":%llx", (*cur_exts_array)[*ext_count].value);
                    }
                }

                if (rec.requests.flag) {
                    int index;
                    for (int j = 0; j < rec.requests.triggered_request_num * 4; j++, (*ext_count)++) { /* write 4-tuples(index,tag,source,transferred_bytes) here */
                        if (*ext_count >= *max_ext_count && *max_ext_count > 0) {
                            if (use_realtime_dump) {
                                update_extras_pointers(tid, level, max_ext_count, cur_exts_array);
                            } else {
                                *cur_exts = (*cur_exts)->next;
                                *max_ext_count = ((*cur_exts)->next == NULL) ? *ext_index : onetimealloc;
                                *cur_exts_array = (msl_extra_info *)(*cur_exts)->events_array;
                            }
                            *ext_count = 0;
                        }
                        if (j % 4 == 0) { // index
                            index = (*cur_exts_array)[*ext_count].values[0];
                        }
                        if (j % 4 == 2 && (index < 0 || index >= rec.requests.input_request_num)) { // source
                            debug_printf("Rank:%d  Index=%d is out of range (0...%d) \n", t_mpi_rank, index, rec.requests.input_request_num);
                        }

                        if ((*cur_exts_array)[*ext_count].values[1] == -1) {
                            if (j % 4 == 2 && index >= 0 && index < rec.requests.input_request_num) { // source
                                t_gzprintf(fps_mpi, ":%d", get_real_rank(req_comm[input_requests[index]].comm, (*cur_exts_array)[*ext_count].value, req_comm[input_requests[index]].time, req_comm[input_requests[index]].is_internal));
                            } else {
                                t_gzprintf(fps_mpi, ":%d", (*cur_exts_array)[*ext_count].values[0]);
                            }
                        } else {
                            if (j % 4 == 2 && index >= 0 && index < rec.requests.input_request_num) { // source
                                t_gzprintf(fps_mpi, ":%d", get_real_rank(req_comm[input_requests[index]].comm, (*cur_exts_array)[*ext_count].value, req_comm[input_requests[index]].time, req_comm[input_requests[index]].is_internal));
                            } else {
                                t_gzprintf(fps_mpi, ":%lld", (*cur_exts_array)[*ext_count].value);
                            }
                        }
                    }
                }
            }

            /***********************************************************************************************************************************************************************************************/

            if (func_base >= MPI_START_GENERAL_COLLECTIVE_GROUP && func_base < MPI_END_GENERAL_COLLECTIVE_GROUP) {
                // collectives      5  6  7  8  9  10 11
                t_gzprintf(fps_mpi, ":%x:%d:%llx:%d:%d",
                           get_global_comm_id(rec.collective.comm,
                                              tsc_mpi_entry, 0),    // 4
                           get_real_rank(rec.collective.comm,
                                         rec.collective.root,
                                         tsc_mpi_entry, 0),         // 5
                           MPI_Check_dummy(rec.collective.request), // 6
                           rec.collective.sr_bytes[0],              // 7
                           rec.collective.sr_bytes[1]               // 8
                );
                long long cumulative_size = 0;
                for (int j = 0; j < rec.collective.sr_bytes[0]; j++, (*ext_count)++) {
                    if (*ext_count >= *max_ext_count) {
                        if (use_realtime_dump && *max_ext_count > 0) {
                            update_extras_pointers(tid, level, max_ext_count, cur_exts_array);
                        } else {
                            *cur_exts = (*cur_exts)->next;
                            *max_ext_count = ((*cur_exts)->next == NULL) ? *ext_index : onetimealloc;
                            *cur_exts_array = (msl_extra_info *)(*cur_exts)->events_array;
                        }
                        *ext_count = 0;
                    }
                    if ((*cur_exts_array)[*ext_count].values[1] == -1) {
                        t_gzprintf(fps_mpi, ":%d", (*cur_exts_array)[*ext_count].values[0]);
                    } else {
                        t_gzprintf(fps_mpi, ":%lld", (*cur_exts_array)[*ext_count].value);
                    }
                    cumulative_size += (*cur_exts_array)[*ext_count].value;
                }
                for (int j = 0; j < rec.collective.sr_bytes[1]; j++, (*ext_count)++) {
                    if (*ext_count >= *max_ext_count) {
                        if (use_realtime_dump && *max_ext_count > 0) {
                            update_extras_pointers(tid, level, max_ext_count, cur_exts_array);
                        } else {
                            *cur_exts = (*cur_exts)->next;
                            *max_ext_count = ((*cur_exts)->next == NULL) ? *ext_index : onetimealloc;
                            *cur_exts_array = (msl_extra_info *)(*cur_exts)->events_array;
                        }
                        *ext_count = 0;
                    }
                    if ((*cur_exts_array)[*ext_count].values[1] == -1) {
                        t_gzprintf(fps_mpi, ":%d", (*cur_exts_array)[*ext_count].values[0]);
                    } else {
                        t_gzprintf(fps_mpi, ":%lld", (*cur_exts_array)[*ext_count].value);
                    }
                    cumulative_size += (*cur_exts_array)[*ext_count].value;
                }
    #ifdef _T_PER_INTERVAL_STAT_WRITE_
                int group = gt_func_stat.get_group(cumulative_size);
                int func_id_t = rec.func_id;
                gt_func_stat.stat_for_func[func_id_t].durations[group] += tsc_mpi_exit - tsc_mpi_entry;
                gt_func_stat.stat_for_func[func_id_t].occurences[group]++;
                gt_func_stat.stat_for_func[func_id_t].sizes[group] += cumulative_size;
                stat_processed = 1;
    #endif                                                                                                                                                                                                  // _T_PER_INTERVAL_STAT_WRITE_
                req_comm[MPI_Check_dummy(rec.collective.request)] = T_MPI_Comm(rec.collective.comm, tsc_mpi_entry, 0); // save comm for later WAIT or TEST
            }

    /***********************************************************************************************************************************************************************************************/
    #if 0
    #ifdef _T_PER_INTERVAL_STAT_WRITE_
                if(stat_processed != 1) {
                    int group=gt_func_stat.get_group(0);
                    int func_id_t=rec.func_id;
                    gt_func_stat.stat_for_func[func_id_t].durations[group]+=tsc_mpi_exit - tsc_mpi_entry;
                    gt_func_stat.stat_for_func[func_id_t].occurences[group]++;
                    gt_func_stat.stat_for_func[func_id_t].sizes[group]+=0;
                    stat_processed = 1;
                }
    #endif // _T_PER_INTERVAL_STAT_WRITE_
    #endif

    #ifdef _TRACTOR_EXTRA_FIELDS_
            if (func_base >= MPI_START_FILE_GROUP && func_base < MPI_END_FILE_GROUP) {
                print_address(fps_hwc, &rec.mpi_fio.buf);
            }

            if (func_base >= MPI_START_BUFFERED_LOCAL_GROUP && func_base < MPI_END_BUFFERED_LOCAL_GROUP) {
                print_address(fps_hwc, &rec.buf_local.buf[0]);
                print_address(fps_hwc, &rec.buf_local.buf[1]);
            }
            if (func_base >= MPI_P2P_TRANSFER_START && func_base < MPI_P2P_TRANSFER_END ||
                func_base >= MPI_P2P_PERSISTENT_INIT_START && func_base < MPI_P2P_PERSISTENT_INIT_END ||
                func_base >= MPI_PROBE_START && func_base < MPI_PROBE_END) {
                print_address(fps_hwc, &rec.p2p_pr_pb.buf);
            }
            if (func_base >= MPI_START_OSC_GROUP && func_base < MPI_END_OSC_GROUP) {
                print_address(fps_hwc, &rec.one_sided_comm.buf);
                print_address(fps_hwc, &rec.one_sided_comm.rbuf);
                print_address(fps_hwc, &rec.one_sided_comm.cbuf);
            }

            if (func_base >= MPI_REQUEST_START && func_base < MPI_REQUEST_END) {
                print_address(fps_hwc, &rec.requests.requests);
                print_address(fps_hwc, &rec.requests.statuses);
            }
            if (func_base >= MPI_START_GENERAL_COLLECTIVE_GROUP && func_base < MPI_END_GENERAL_COLLECTIVE_GROUP) {
                print_address(fps_hwc, &rec.collective.buf[0]);
                print_address(fps_hwc, &rec.collective.buf[1]);
            }
    #endif
        t_gzprintf(fps_hwc, "\n");
        t_gzprintf(fps_mpi, "\n");
        return children;
    };

    /* step the cursor of (tid, level) to the next record, loading the next block when needed */
    auto advance = [&](int tid, int level) -> void {
        const int idx = tid * levels + level;
        record_counter[idx]++;
        if (record_counter[idx] >= max_events[idx])
            load_block(tid, level, false);
    };
    auto has_record = [&](int tid, int level) -> bool {
        const int idx = tid * levels + level;
        return level < levels && max_events[idx] > 0 && record_counter[idx] < max_events[idx];
    };
    /* the second record of a sendrecv pair (zero mpi entry offset) follows the first one with the same ids */
    auto is_second_line = [&](int tid, int level, PEAT_Clock tsc_counter, short func_id) -> bool {
        const int idx = tid * levels + level;
        if (!has_record(tid, level))
            return false;
        const msl_event &next = current_array[idx][record_counter[idx]];
        return next.tscs[0] == tsc_counter && next.func_id == func_id ;
    };

    struct t_walk_frame {
        long long remaining; /* children of the record at this frame's level still to write */
        long long index;     /* index of the child being written */
    };
    while (!timeline.empty()) {
        std::map<t_timeline, int>::iterator timeline_it = timeline.begin();
        const int tid = timeline_it->second;
        timeline.erase(timeline_it);
        /* pre-order walk of the tree rooted at the head record of level 0 */
        std::vector<t_walk_frame> stack;
        int level = 0;
        for (int k = 0; k < id_columns; k++)
            ids[k] = -1;
        ids[0] = ++uniq_index; /* top-level records are numbered from 1, as before */
        bool walking = true;
        while (walking) {
            if (!has_record(tid, level)) {
                debug_printf("TID %d level %d: record missing while walking nested records.\n", tid, level);
                break;
            }
            const short func_id = current_array[tid * levels + level][record_counter[tid * levels + level]].func_id;
            long long children = emit_record(tid, level, false);
            advance(tid, level);
            if (is_second_line(tid, level, tsc_counter, func_id)) {
                emit_record(tid, level, true);
                advance(tid, level);
            }
            if (children > 0 && level + 1 < levels) {
                t_walk_frame frame;
                frame.remaining = children;
                frame.index = 0;
                stack.push_back(frame);
                level++;
                ids[level] = 0;
                continue;
            }
            /* this record is done: account it in its parent's frame, climb while parents are complete */
            while (!stack.empty()) {
                t_walk_frame &frame = stack.back();
                frame.remaining--;
                if (frame.remaining > 0) {
                    frame.index++;
                    ids[level] = frame.index;
                    break;
                }
                stack.pop_back();
                ids[level] = -1;
                level--;
            }
            if (stack.empty())
                walking = false;
        }
        if (has_record(tid, 0)) {
            const int idx0 = tid * levels;
            timeline[t_timeline(current_array[idx0][record_counter[idx0]].tscs[0], tid)] = tid;
        }
    }
    if (use_realtime_dump) {
        for (int i = 0; i < t_tid_counter; i++) {
            for (int level = 0; level < levels; level++) {
                const int idx = i * levels + level;
                if (current_array[idx])
                    t_free(current_array[idx], (onetimealloc + 1) * sizeof(msl_event));
                if (current_addrs_array[idx])
                    t_free(current_addrs_array[idx], (onetimealloc + 1) * sizeof(void *) * num_addrs);
#ifdef _HWCOUNTERS_ENABLED_
                if (current_hwc_array[idx])
                    t_free(current_hwc_array[idx], (onetimealloc + 1) * sizeof(long long) * num_hwcs * 2);
#endif
#ifdef _MEMPROFILE_STORE_
                if (current_mems_array[idx])
                    t_free(current_mems_array[idx], (onetimealloc + 1) * t_memory_event_size * 2);
#endif
                if (current_extras_array[idx])
                    t_free(current_extras_array[idx], (onetimealloc + 1) * sizeof(msl_extra_info));
            }
        }
    }

    ret.max_thread_id = max_thread_id;
    ret.processed_events = processed_events;
    ret.max_uniq_idx = uniq_index;
    ret.nesting_levels = id_columns;
    if (fps_mpi)
        gzclose(fps_mpi);
    if (fps_hwc)
        gzclose(fps_hwc);

#if defined(_TRANSPORT_PROFILE_)
    if (t_write_main_files && t_collect_transport && t_mpi_rank >= 0)
        ret.max_transport_uniq_idx = t_transport_prof.write_transport_file(print_names);
#endif
    __DEBUG_EPILOG_TRACTOR_LAYER__
    return (ret);
}

#ifdef __BACK_TO_IT_LATER__
void tractor::calculate_us_sd_dev(user_region_store &user_region_store)
{
    // calculate mean for all regions
    for (std::map<user_address_key, user_region_statistic>::iterator it = user_region_store.regions.begin(); it != user_region_store.regions.end(); ++it) {
        user_region_statistic &stat = it->second;
        if (stat.should_not_be_used_in_statistic) {
            continue;
        }
        if (stat.num > 0) {
            stat.mean_clock = PEAT_Ratio(stat.sum_clock) / PEAT_Ratio(stat.num);
            stat.mean_instr = PEAT_Ratio(stat.sum_instr) / PEAT_Ratio(stat.num);
            stat.mean_cpi = stat.sum_cpi / PEAT_Ratio(stat.num);
            stat.elements.reserve(stat.num);
        }
    }
    // second iteration over all CPU events
    PEAT_Addr prev_addr = 0, prev_eaddr = 0;
    for (std::list<collector_cpu_event *>::const_iterator it = storage_events_cpu.begin(); it != storage_events_cpu.end(); ++it) {
        PEAT_Num max_position_in_chunk = 0;
        std::list<collector_cpu_event *>::const_iterator next_it = it;
        ++next_it;
        if (next_it == storage_events_cpu.end()) { // last chunk
            max_position_in_chunk = storage_events_mpi_pos + 1;
        } else { // no last chunk
            max_position_in_chunk = peat_memory_mpi_chunk_size;
        }
        collector_cpu_event *chunk = *it;
        for (PEAT_Num i = 0; i < max_position_in_chunk; ++i) {
            const collector_cpu_event &event_cpu = chunk[i];
            user_address_key addr(prev_addr, prev_eaddr, event_cpu.addr, event_cpu.eaddr);
            user_region_statistic &region_stat = user_region_store.regions[addr]; // don't want an exception here
            if (0 == region_stat.num) {                                         // unexpected new event
                COLLERR << "Collector::calculate_unbiased_sample_standard_deviation. Can't find address key " << addr << "\n";
                break; // something going wrong. Skip the chunk and write the trace as is
            }
            if (!region_stat.should_not_be_used_in_statistic) {
                region_stat.u_st_dev_clock += pow(PEAT_Ratio(event_cpu.cpu_clocks) - region_stat.mean_clock, 2);
                region_stat.u_st_dev_instr += pow(PEAT_Ratio(event_cpu.cpu_instructions) - region_stat.mean_instr, 2);
                PEAT_Ratio cpi = (0 == event_cpu.cpu_instructions) ? 0.0 : (PEAT_Ratio(event_cpu.cpu_clocks) / PEAT_Ratio(event_cpu.cpu_instructions));
                region_stat.u_st_dev_cpi += pow(cpi - region_stat.mean_cpi, 2);

                user_region_statistic::Elems region_elements;
                region_elements.clocks = event_cpu.cpu_clocks;
                region_elements.instr = event_cpu.cpu_instructions;
                region_elements.cpi = (0 == event_cpu.cpu_instructions) ? 0.0 : PEAT_Ratio(event_cpu.cpu_clocks) / PEAT_Ratio(event_cpu.cpu_instructions);
                region_stat.elements.push_back(region_elements);
            }
            prev_addr = event_cpu.addr;
            prev_eaddr = event_cpu.eaddr;
        }
    }
    // finalize u_st_dev
    for (std::map<user_address_key, user_region_statistic>::iterator it = user_region_store.regions.begin(); it != user_region_store.regions.end(); ++it) {
        user_region_statistic &stat = it->second;
        if (stat.num > 0 && !stat.should_not_be_used_in_statistic) {
            if (stat.u_st_dev_clock) {
                stat.u_st_dev_clock = sqrt(stat.u_st_dev_clock / (PEAT_Ratio(stat.num) - 1.5));
            }
            if (stat.u_st_dev_instr) {
                stat.u_st_dev_instr = sqrt(stat.u_st_dev_instr / (PEAT_Ratio(stat.num) - 1.5));
            }
            if (stat.u_st_dev_cpi) {
                stat.u_st_dev_cpi = sqrt(stat.u_st_dev_cpi / (PEAT_Ratio(stat.num) - 1.5));
            }
            // Approximation quality
            sort(stat.elements.begin(), stat.elements.end());

            PEAT_Num region_size = PEAT_Num(PEAT_Ratio(stat.elements.size()) * user_region_quality_range + 0.5);
            if (region_size > 0) {
                PEAT_Num first_idx = (stat.elements.size() - region_size) / 2;
                PEAT_Num last_idx = first_idx + region_size - 1;
                // size_t assert(first_idx >= 0);
                assert(last_idx < stat.elements.size());
                for (PEAT_Num i = first_idx; i <= last_idx; ++i) {
                    stat.sum_clock2 += stat.elements[i].clocks;
                    stat.sum_instr2 += stat.elements[i].instr;
                }
                stat.cpi2 = (0 == stat.sum_instr2) ? 0.0 : PEAT_Ratio(stat.sum_clock2) / PEAT_Ratio(stat.sum_instr2);
                if (region_size > 1) {
                    PEAT_Ratio square_sum = 0;
                    for (PEAT_Num i = first_idx; i <= last_idx; ++i) {
                        square_sum += pow(stat.elements[i].cpi - stat.cpi2, 2);
                    }
                    stat.u_st_dev_cpi2 = sqrt(square_sum / (PEAT_Ratio(region_size) - 1.5));
                }
            }
        }
    }
}
#endif

void tractor::write_stat_com_file(void)
{
    __DEBUG_PROLOG_TRACTOR_LAYER__
#ifdef _TRACTOR_COLLECT_STAT_
    gzprintf(fps_com, "%d:Function Statistics:\n", PEAT_RT_Remark);
    long long total = 0;
    SEPARATOR(2 + 34 + 19 + 36 + 38 + 4);
    gzprintf(fps_com, "%d:%34s|\t%19s|\t%37s|\t%23s  (%)|\n", PEAT_RT_Remark, "Function", "Occurences", "Duration(%par)(%mpi|omp|gpu)", "Tool Overhead");
    for (int f = MPI_MIN_FUNCID_THROUGH + 1; f < MPI_MAX_FUNCID_THROUGH; f++) {
        if (print_all + overall_entries[f] > 0) {
            double ratio = ((double)(overall_tool_overhead[f] * 100.0)) / ((double)(overall_durations[f]));
            total += overall_tool_overhead[f];
            double fratio = ((double)(overall_durations[f] * 100.0)) / ((double)(last_par_tsc - first_par_tsc));

            if (f >= MPI_P2P_TRANSFER_START_THROUGH && f <= MPI_END_LOCAL_GROUP_THROUGH) {
                double fratio_mpi = ((double)(overall_durations[f] * 100.0)) / ((double)(global_mpi_time));
                gzprintf(fps_com, "%d:%34s|\t%19lld|\t%19lld(%06.2f%)(%06.2f%)|\t%18lld(%07.2f%)|\n", PEAT_RT_Remark, mpi_function_names[f], overall_entries[f], overall_durations[f], fratio, fratio_mpi, overall_tool_overhead[f], ratio);
            } else {
                if (f == OMPT_PARALLEL_ID_THROUGH) {
                    gzprintf(fps_com, "%d:%34s|\t%19lld|\t%19lld(%06.2f%)(%06.2f%)|\t%18lld(%07.2f%)|\n", PEAT_RT_Remark, mpi_function_names[f], overall_entries[f], overall_durations[f], fratio, (double)0.0, overall_tool_overhead[f], ratio);
                } else {
                    if (f >= CL_START_GROUP_THROUGH && f <= CL_END_GROUP_THROUGH || f >= ZE_START_GROUP_THROUGH && f <= ZE_END_GROUP_THROUGH) {
                        double fratio_gpu = ((double)(overall_durations[f] * 100.0)) / ((double)(global_gpu_time));
                        gzprintf(fps_com, "%d:%34s|\t%19lld|\t%19lld(%06.2f%)(%06.2f%)|\t%18lld(%07.2f%)|\n", PEAT_RT_Remark, mpi_function_names[f], overall_entries[f], overall_durations[f], fratio, fratio_gpu, overall_tool_overhead[f], ratio);
                    } else {
                        double fratio_ompt_par = overall_durations[OMPT_PARALLEL_ID_THROUGH] > 0 ? ((double)(overall_durations[f] * 100.0)) / ((double)(overall_durations[OMPT_PARALLEL_ID_THROUGH]) * t_tid_counter) : 0.0;
                        gzprintf(fps_com, "%d:%34s|\t%19lld|\t%19lld(%06.2f%)(%06.2f%)|\t%18lld(%07.2f%)|\n", PEAT_RT_Remark, mpi_function_names[f], overall_entries[f], overall_durations[f], fratio, fratio_ompt_par, overall_tool_overhead[f], ratio);
                    }
                }
            }
        }
    }
    gzprintf(fps_com, "%d:%34s|\t%19s|\t%37s|\t%28lld|\n", PEAT_RT_Remark, "Total", " ", " ", total);
    SEPARATOR(2 + 34 + 19 + 36 + 38 + 4);
#endif

#if 1 // def _T_PER_INTERVAL_STAT_WRITE_
    SEPARATOR(38 * (gt_func_stat.bound_num - 1) + 33);
    gzprintf(fps_com, "%d:%34s|", PEAT_RT_Remark, "Function");
    for (int g = 0; g < gt_func_stat.bound_num - 1; g++) {
        char tmp[1024];
        sprintf(tmp, "%lld <= size < %lld", gt_func_stat.bounds[g], gt_func_stat.bounds[g + 1]);
        gzprintf(fps_com, "%37s|", tmp);
    }
    gzprintf(fps_com, "\n");

    SEPARATOR(38 * (gt_func_stat.bound_num - 1) + 35);

    for (int f = MPI_MIN_FUNCID_THROUGH + 1; f < MPI_MAX_FUNCID_THROUGH; f++) {
        if (print_all + overall_entries[f] > 0) {
            long long total_transfers = 0;
            for (int g = 0; g < gt_func_stat.bound_num - 1; g++) {
                total_transfers += gt_func_stat.stat_for_func[f].occurences[g];
            }
            if (print_all + total_transfers > 0) {
                gzprintf(fps_com, "%d:%34s|", PEAT_RT_Remark, mpi_function_names[f]);
                for (int g = 0; g < gt_func_stat.bound_num - 1; g++) {
                    double ratio = 100.0 * gt_func_stat.stat_for_func[f].durations[g] / ((double)(last_par_tsc - first_par_tsc));
                    char tmp[1024];
                    if (gt_func_stat.stat_for_func[f].occurences[g] > 0) {
                        sprintf(tmp, "%lld:%lld:%lld:%.2f",
                                (long long)gt_func_stat.stat_for_func[f].sizes[g] / gt_func_stat.stat_for_func[f].occurences[g],
                                gt_func_stat.stat_for_func[f].occurences[g],
                                (long long)gt_func_stat.stat_for_func[f].durations[g] / gt_func_stat.stat_for_func[f].occurences[g], ratio);
                        gzprintf(fps_com, "%37s|", tmp);
                    } else {
                        // print zeros
                        sprintf(tmp, "%lld:%lld:%lld:%.2f",
                                (long long)gt_func_stat.stat_for_func[f].sizes[g],
                                gt_func_stat.stat_for_func[f].occurences[g],
                                (long long)gt_func_stat.stat_for_func[f].durations[g], ratio);
                        gzprintf(fps_com, "%37s|", tmp);
                    }
                }
                gzprintf(fps_com, "\n");
            }
        }
    }
    SEPARATOR(38 * (gt_func_stat.bound_num - 1) + 35);
#ifdef GPUPARSE
    print_gpu_stat();
#endif
    __DEBUG_EPILOG_TRACTOR_LAYER__

#endif // _T_PER_INTERVAL_STAT_WRITE_
}

void tractor::write_com_file(const num_processed_events &num_of_events, /*user_region_store &user_region_store,*/ const PEAT_Clock time_symbols_search)
{
    __DEBUG_PROLOG_TRACTOR_LAYER__
    int print_all = 0; // todo change accordingly
    fps_com = gzopen(com_filename, gz_mode);
    if (fps_com == NULL) {
        return;
    }
    PEAT_Clock com_write_len = rdtsca();
    long core_id = get_core_id(); // get proc id for further writing to com file

    gzprintf(fps_com, "%d:---=== PEAT TRACTOR V%d.%d. Build Date %s(%s). ===---\n", PEAT_RT_Remark, _TRACTOR_MAJOR_VERSION_, _TRACTOR_MINOR_VERSION_, _TRACTOR_BUILD_DATE_, __DATE__);
    gzprintf(fps_com, "%d:Application begins. Max Threads %d. Return Level %u.\n", PEAT_RT_Remark, t_max_threads, max_ret_level);
    gzprintf(fps_com, "%d:msl_event=%d : pp=%d,req=%d,coll=%d,comm=%d,bl=%d,osc=%d,fio=%d,l=%d,pc=%d,transport=%d\n", PEAT_RT_Remark, sizeof(msl_event),
             sizeof(msl_event::p2p_pr_pb), sizeof(msl_event::requests), sizeof(msl_event::collective),
             sizeof(msl_event::comms), sizeof(msl_event::buf_local), sizeof(msl_event::one_sided_comm), sizeof(msl_event::mpi_fio),
             sizeof(msl_event::local), sizeof(msl_event::pcontrol), sizeof(msl_event::transport));

    gzprintf(fps_com, "%d:These functions handled even outside pcontrols:\n", PEAT_RT_Remark);
    for (int i = 0; i < MPI_MAX_FUNCID_THROUGH; i++) {
        if (counting_permission_pcontrol[i] > 0) {
            gzprintf(fps_com, "%d:%s\n", PEAT_RT_Remark, mpi_function_names[i]);
        }
    }
    gzprintf(fps_com, "%d:end.\n", PEAT_RT_Remark);

    // Frame table currently has 1 record
    PEAT_Clock app_start = 0, app_end = 0, app_start_parallel = 0, app_end_parallel = 0, overall_tool_overhead = 0, overall_MPI = 0, overall_GPU = 0;
    get_rank_time(&app_start, &app_start_parallel, &app_end_parallel, &app_end, &overall_tool_overhead, &overall_MPI, &overall_GPU);

    gzprintf(fps_com, "%d:%lld:%lld:%lld:%lld\n", PEAT_RT_App_Start_Stop_Info, app_start, app_end, app_end - app_start, t_tsc_init);
    gzprintf(fps_com, "%d:%lld:%lld:%lld:%lld\n", PEAT_RT_MPM_Rank_Time_Stamp, app_start, app_start_parallel, app_end_parallel, app_end);
    gzprintf(fps_com, "%d:%d:%lld:%lld\n", PEAT_RT_CPU_Freq, 0, proc_freq[0], proc_freq[1]);
    gzprintf(fps_com, "%d:%lld:%lld:%lld:%d:%lld\n", PEAT_RT_Number_Of_Events, num_of_events.processed_events, num_of_events.max_uniq_idx, num_of_events.max_thread_id, num_of_events.nesting_levels, num_of_events.max_transport_uniq_idx); /* 4: id columns (nesting levels), 5: transport-API events */
    gzprintf(fps_com, "%d:%d\n", PEAT_RT_HWLOC_Proc_Record, core_id);
    if (node_topology.length() > 0)
        gzprintf(fps_com, "%d%s\n", PEAT_RT_HWLOC_Node_Record, node_topology.c_str());

    // gzprintf(fps_com, "%d:%lld:%lld:%lld:%lld:%lld:%d:%d\n", peat_rt_collector_frame_info, 0,0,num_of_events,app_start_parallel,app_end_parallel,0,0);
    // frame0.offset, frame0.idx_start, frame0.idx_stop, frame0.tsc_start, frame0.tsc_stop, frame0.size_tsc_block, frame0.size_hw_block);

    print_function_report(PEAT_RT_MPI_Functions, app_start, app_end, overall_tool_overhead, overall_MPI, overall_GPU);
    print_modules(PEAT_RT_Modules_Files_Definition);
    print_address_map(PEAT_RT_User_Addresses);
    gzprintf(fps_com, "%d:Overall intrusion into application rank(%%):%f\n", PEAT_RT_Remark, 100 * PEAT_Ratio(overall_tool_overhead) / PEAT_Ratio(app_end - app_start));
#ifdef __BACK_TO_IT_LATER__
    print_addr_report(PEAT_RT_User_Addresses, user_region_store);
    print_user_regions_report(PEAT_RT_User_Regions, user_region_store);
#endif
#ifdef PEAT_KEEPVA
    print_virtual_addresses_report(PEAT_RT_VAPA_Translation);
#endif
    if (local_comms)
        local_comms->print_comms_report(fps_com, PEAT_RT_Local_Communicators_Report);
#ifdef _MEM_HOOKS_
    mprof.print_memory_profile_report(fps_com, PEAT_RT_Memory_Profile_Report, PEAT_RT_Memory_Profile_Report_Total);
#endif

    gzprintf(fps_com, "%d:%d:%x:%x:%x:%x:%d:%d:%d:%x:%d\n", PEAT_RT_MPI_Predefined, MPI_SUCCESS, MPI_UNDEFINED, MPI_COMM_WORLD, MPI_COMM_SELF, MPI_COMM_NULL, MPI_PROC_NULL, MPI_ANY_TAG, MPI_ANY_SOURCE, MPI_REQUEST_NULL, MPI_ROOT);
    write_stat_com_file();
    __DEBUG_EPILOG_TRACTOR_LAYER__
}

#define PRINT_PARAMETERS(width, string) int(width + strlen(string)) / 2, string, int(width - strlen(string)) / 2, ""
#define SEPARATOR2(outfile, width)              \
    gzprintf(outfile, "%02d:", PEAT_RT_Remark); \
    for (int i = 0; i < width; ++i) {           \
        gzprintf(outfile, "%c", '-');           \
    }                                           \
    gzprintf(outfile, "\n")

void tractor::print_address_map(const PEAT_Record_Type line_prefix)
{
    for (std::map<PEAT_Addr, t_address>::const_iterator addr_it = addr_map.begin(); addr_it != addr_map.end(); ++addr_it) {
        const t_address &t_addr = addr_it->second;
        const PEAT_Addr addr = addr_it->first;
        gzprintf(fps_com, "%d:%ld:%llx:%s:%s:%s:%ld\n", line_prefix, t_addr.id, addr, t_addr.binary.c_str(), t_addr.func_name.c_str(), t_addr.source_name.c_str(), t_addr.line_number);
    }
}

void tractor::print_function_report(const PEAT_Record_Type line_prefix, const PEAT_Clock app_start, const PEAT_Clock app_end, const PEAT_Clock overall_tool_overhead, const PEAT_Clock overall_MPI, const PEAT_Clock overall_GPU)
{
}
void tractor::print_modules(const PEAT_Record_Type line_prefix)
{
    int index = 0;
    for (std::map<std::string, t_bin>::iterator it = binaries_map.begin(); it != binaries_map.end(); ++it, index++) {
        PEAT_Addr low = it->second.load_addr, high = it->second.end_addr, base = it->second.base_addr;
        gzprintf(fps_com, "%d:%d:%s:%llx:%llx:%llx\n", line_prefix, index, it->first.c_str(), base, low, high);
    }
}

void tractor::write_app_file(t_nodes_info *ninfo)
{
    __DEBUG_PROLOG_TRACTOR_LAYER__
    if (0 >= t_mpi_rank) {
        fps_app = gzopen(app_filename, gz_mode);
        if (fps_app == NULL) {
            std::ostringstream err_txt;
            err_txt << "Can't open MPM file\n";
            throw std::runtime_error(err_txt.str());
        }
        // print main IDs table start
        for (PEAT_Signed_Num it = 0; it <= PEAT_RT_End_Of_Definitions; ++it) {
            const char *name = print_record_type((PEAT_Record_Type)it);
            if (name) {
                gzprintf(fps_app, "%d:%d:%s\n", PEAT_RT_Definitions, it, name);
            }
        }
        gzprintf(fps_app, "%d:%d\n", PEAT_RT_Frame_Marker, TMPIfunc_id_Size); // Need ID that is not overlapped by func_id in *.rnk to get "frame handling" in trace file
        // print main IDs table stop
#ifdef _HWCOUNTERS_ENABLED_
        int __num_hwcs = hwc.get_num_counters();
#else
        int __num_hwcs = 0;
#endif

        gzprintf(fps_app, "%d:%d:%d", PEAT_RT_Var_Block_Info, num_addrs, __num_hwcs);
        for (int i = 0; i < num_addrs; i++) {
            gzprintf(fps_app, ":%d", ret_levels[i]);
        }
        gzprintf(fps_app, "\n");

        for (int i = 0; i < MPI_MAX_FUNCID_THROUGH; ++i) {
            gzprintf(fps_app, "%d:%d:%s\n", PEAT_RT_MPI_Function_Name_ID, mpi_func_ids_trans[i], mpi_function_names[i]);
        }
        global_comms.print_comms_report(fps_app, PEAT_RT_Global_Communicators_Report);

        if (ninfo) {

            gzprintf(fps_app, "%d:%d", PEAT_RT_MPM_Master_Ranks, ninfo->nodes_num); // node_count field is not used in Estimator
            for (int i = 0; i < ninfo->nodes_num; ++i) {
                debug_printf("%d:%d", ninfo->nodes_num, ninfo->nodes[i].mpi_ranks[0]);
                // fflush(stdout);
                gzprintf(fps_app, ":%d", ninfo->nodes[i].mpi_ranks[0]);
            }
            gzprintf(fps_app, "\n");
            for (int i = 0; i < ninfo->nodes_num; ++i) {
                gzprintf(fps_app, "%d:%d:%d:%lld:%lld:%lld:%s\n", PEAT_RT_MPM_Nodes_Freq, i, ninfo->nodes[i].mpi_ranks[0], ninfo->nodes[i].proc_freq[0], ninfo->nodes[i].proc_freq[1], ninfo->nodes[i].init_delay, ninfo->nodes[i].hostname);
            }
            gzprintf(fps_app, "%d:%d", PEAT_RT_Sync_Point, size_cw);
            for (int i = 0; i < size_cw; ++i) {
                gzprintf(fps_app, ":%lld", start_sync[i]);
            }
            gzprintf(fps_app, "\n");
            gzprintf(fps_app, "%d:%d", PEAT_RT_Sync_Point, size_cw);
            for (int i = 0; i < size_cw; ++i) {
                gzprintf(fps_app, ":%lld", stop_sync[i]);
            }
            gzprintf(fps_app, "\n");

            const int field_len = 6;
            const int name_len = 16;
            const int separator_length = 2 * name_len + 2 + 3 * field_len + 3;
            SEPARATOR2(fps_app, separator_length);
            gzprintf(fps_app, "%02d", PEAT_RT_Remark);
            gzprintf(fps_app, ":%*s%*s", PRINT_PARAMETERS(name_len, "Host"));
            gzprintf(fps_app, ":%*s%*s", PRINT_PARAMETERS(name_len, "Frequency "));
            gzprintf(fps_app, ":%*s%*s", PRINT_PARAMETERS(field_len, "PPN "));
            gzprintf(fps_app, ":%*s%*s", PRINT_PARAMETERS(field_len, "id"));
            gzprintf(fps_app, ":%*s%*s", PRINT_PARAMETERS(field_len, "Ranks "));
            gzprintf(fps_app, "\n");
            SEPARATOR2(fps_app, separator_length);
            for (int i = 0, masters = 0; i < ninfo->nodes_num; ++i) { // print node information only from master rank
                gzprintf(fps_app, "%d", PEAT_RT_MPM_Ranks_And_PPN);
                gzprintf(fps_app, ":%*s", name_len, ninfo->nodes[i].hostname);
                gzprintf(fps_app, ":%*lld", name_len, ninfo->nodes[i].cpu_proc_freq[0]);
                gzprintf(fps_app, ":%*d", field_len, ninfo->nodes[i].mpi_rank_num);
                gzprintf(fps_app, ":%*d", field_len, masters);
                char delim = ':';
                for (int j = 0; j < ninfo->nodes[i].mpi_rank_num; ++j) {
                    gzprintf(fps_app, "%c%d", delim, ninfo->nodes[i].mpi_ranks[j]);
                    if (':' == delim) {
                        delim = ',';
                    }
                }
                gzprintf(fps_app, "\n");
                ++masters;
            }
            SEPARATOR2(fps_app, separator_length);

            gzprintf(fps_app, "%d:%d", PEAT_RT_MPM_All_Ranks_Exec_Time, ninfo->nodes_num);
            for (int i = 0; i < ninfo->nodes_num; ++i) { // all ranks information
                gzprintf(fps_app, ":%lld:%lld:%lld", ninfo->nodes[i].t_time[0], ninfo->nodes[i].t_time[1], ninfo->nodes[i].t_time[2]);
            }
            gzprintf(fps_app, "\n");
        } else {
            gzprintf(fps_app, "\nNo nodes information available\n");
        }
        if (fps_app)
            gzclose(fps_app);
    }
    __DEBUG_EPILOG_TRACTOR_LAYER__
}

void tractor::pc_change_state(int ctl)
{
    __DEBUG_PROLOG_TRACTOR_LAYER__
    if (pcontrol) {
        if (pc_levels.empty() || pc_levels.count(ctl) == 1) {
            overall_permit += ctl;
            counting_permit = ((overall_permit > 0) && t_write_main_files);
            debug_printf("Rank %d: Counting Permit...%d\n", t_mpi_rank, counting_permit);
        }
    }
    __DEBUG_EPILOG_TRACTOR_LAYER__
}

void tractor::dump_extras(pid_t tid, int level, msl_base_chain **chain)
{
    __DEBUG_PROLOG_TRACTOR_LAYER__
    FILE **fps_ext = &fps_extras[fp_index(tid, level)];
    if (*fps_ext == NULL) {
        char temp_filename[MAX_PATH];
        sprintf(temp_filename, "%s/%s.%d.%d.%d.mexts.tmp", tmp_dir, stat_template, t_mpi_rank, tid, level);
        *fps_ext = fopen(temp_filename, "w+b");
        if (*fps_ext)
            debug_printf("File %s opened..\n", temp_filename);
    }
    if (*fps_ext) {
        fwrite((*chain)->events_array, (*chain)->events_size, (*chain)->num_events, *fps_ext);
        debug_printf("Dumping %ld extras.\n", (*chain)->num_events);
        destroy_shift_start(chain);
    }
    __DEBUG_EPILOG_TRACTOR_LAYER__
}

void tractor::dump_events(pid_t tid, int level, msl_adv_chain **chain)
{
    __DEBUG_PROLOG_TRACTOR_LAYER__
    FILE **fp = &fps_events[fp_index(tid, level)];
    if (*fp == NULL) {
        char temp_filename[MAX_PATH];
        sprintf(temp_filename, "%s/%s.%d.%d.%d.mevts.tmp", tmp_dir, stat_template, t_mpi_rank, tid, level);
        *fp = fopen(temp_filename, "w+b");
        if (*fp)
            debug_printf("File %s opened..\n", temp_filename);
    }
    if (*fp) {
        fwrite(&(*chain)->num_events, 4, 1, *fp);
        fwrite((*chain)->events_array, (*chain)->events_size, (*chain)->num_events, *fp);
        fwrite((*chain)->addr_array, (*chain)->addr_size, (*chain)->num_events, *fp);
#ifdef _HWCOUNTERS_ENABLED_
        fwrite((*chain)->hwc_array, (*chain)->hwc_size, (*chain)->num_events, *fp);
#endif
#ifdef _MEMPROFILE_STORE_
        fwrite((*chain)->mem_array, (*chain)->mem_size, (*chain)->num_events, *fp);
#endif
        debug_printf("Dumping %ld events.\n", (*chain)->num_events);
        destroy_shift_start((msl_base_chain **)chain);
    }

    __DEBUG_EPILOG_TRACTOR_LAYER__
}

int tractor::t_check_chain(pid_t tid, int level)
{
    if (use_realtime_dump) {
        check_chain(tid, level);
#if defined(_TRANSPORT_PROFILE_)
        if (t_collect_transport) {
            t_transport_prof.check_chain(tid, 0);
        }
#endif
    }
    return 0;
}

#ifdef _TRACTOR_DIAG_
tractor_debug::tractor_debug(void)
{
    __DEBUG_PROLOG_TRACTOR_LAYER__
    char dbg_filename[MAX_PATH];
#ifdef OPEN_MPI
    char *t_mpi_rank_temp = __msl_getenv("OMPI_COMM_WORLD_RANK");
    if (t_mpi_rank_temp == NULL) {
        debug_printf("Unable to determine Rank (open_mpi)...\n");
    } else {
        debug_printf("Rank (open_mpi) is %s...\n", t_mpi_rank_temp);
#else
    char *t_mpi_rank_temp = __msl_getenv("PMI_RANK");
    if (t_mpi_rank_temp == NULL) {
        debug_printf("Unable to determine Rank (MPICH)...\n");
    } else {
        debug_printf("Rank (MPICH) is %s...\n", t_mpi_rank_temp);
#endif
        char *stat_dir = __msl_getenv("PEAT_TRACTOR_DIR");
        if (stat_dir == NULL)
            stat_dir = ".";
        char *stat_template = __msl_getenv("PEAT_TRACTOR_FILE");
        if (stat_template == NULL)
            stat_template = "tractor_t";
        sprintf(dbg_filename, "%s/%s.dbg.%s", stat_dir, stat_template, t_mpi_rank_temp);
        debug_fp = fopen(dbg_filename, "w+t");
    }
    t_info = new tractor;
    __DEBUG_EPILOG_TRACTOR_LAYER__
}

#include <execinfo.h>

#define BT_BUF_SIZE 100

tractor_debug::~tractor_debug(void)
{
    __DEBUG_PROLOG_TRACTOR_LAYER__
    /*
        int nptrs;
        void *buffer[BT_BUF_SIZE];
        char **strings;

        nptrs = backtrace(buffer, BT_BUF_SIZE);
        debug_printf("backtrace() returned %d addresses\n", nptrs);

        strings = backtrace_symbols(buffer, nptrs);
        if (strings == NULL) {
            debug_printf("backtrace_symbols failed\n");
            exit(0);
        }
        for (int j = 0; j < nptrs; j++) {
            debug_printf("%s\n", strings[j]);
        }
        free(strings);
    */
    delete t_info;
    t_info = NULL;
    {
        if (debug_fp)
            fclose(debug_fp);
        debug_fp = NULL;
    }
    debug_printf("debug_fp=%p\n", debug_fp);
    __DEBUG_EPILOG_TRACTOR_LAYER__
}

tractor_debug t_info_debug;
#else
tractor t_info;

#ifdef _OPENMPI_
extern "C" int t_addr_in_fortran_lib(const void *addr)
{
    for (int i = 0; i < gt_info(num_fort_ranges); ++i) {
        if (addr > gt_info(lowfort_r)[i] && addr < gt_info(highfort_r)[i])
            return 1;
    }
    return 0;
}
#endif
#endif

#if defined(__CPLUSPLUS__) || defined(__cplusplus)
}
#endif
