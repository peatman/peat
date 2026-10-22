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

#ifndef __TRACTOR_H__
#define __TRACTOR_H__
#include "mpi.h"
#include <assert.h>

#include "elf_parser.h"
#include "t_comm.h"
#include "t_stat.h"
#include "zlib.h"

#include "t_base_event.h"

#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>

// #define max(a,b) ((a)>(b))?(a):(b)
// #define min(a,b) ((a)<(b))?(a):(b)

#if defined(__CPLUSPLUS__) || defined(__cplusplus)
extern "C" {
#endif
/*
static int get_ra_inc(void) {
PEAT_Addr tmp_addr;get_return_address(0,tmp_addr);
    if (tmp_addr>lowfort && tmp_addr<highfort) return 1;else return 0;
}
*/

struct t_address {
    long id;
    std::string binary;
    std::string func_name;
    std::string source_name;
    long line_number;
};

struct t_bin {
    std::list<PEAT_Addr> addrs;
    PEAT_Addr base_addr, load_addr, end_addr;
};

struct num_processed_events {
    long processed_events = 0;
    long max_uniq_idx = 0;
    long max_thread_id = 0;
    int nesting_levels = 1; /* achieved, number of id columns written */
};

#ifdef GPUPARSE
enum T_LOC {
    T_LOC_HOST,
    T_LOC_DEVICE,
    T_LOC_SHARED
};

struct T_Stat_Record {
    PEAT_Clock duration = 0;
    PEAT_LNum occurences = 0;
    T_Stat_Record(PEAT_Clock d = 0, PEAT_LNum o = 0) : duration(d), occurences(o) {};
    T_Stat_Record &operator+=(const T_Stat_Record &s)
    {
        duration += s.duration;
        occurences += s.occurences;
        return *this;
    }
};
#endif

struct tractor : t_base_event {
    int print_all;
    int tractor_initialized;
    int store_map;
    int overall_permit;
    int pcontrol;
    int t_write_main_files;
    int t_use_signals;
    char mpi_filename[1024];
    char hwc_filename[1024];
    char com_filename[1024];
    char app_filename[1024];

    int abort_sleep;
    int max_ret_level;
    int full_ompt_unwind;
    int resolve_comms_and_ranks;
    int *ret_levels;
    int t_o_levels;
    PEAT_Clock *start_sync, *stop_sync;
    std::map<int, int> pc_levels;


#ifdef _T_OMPT_
    int parse_openmp;
#endif

#ifdef GPUPARSE
    std::map<void *, T_LOC> addr_location;
    std::map<short, std::map<T_LOC, std::map<T_LOC, std::map<PEAT_Message_Size, T_Stat_Record>>>> gpu_transfers; // func,from,to,size,time
    std::map<short, std::map<T_LOC, std::map<PEAT_Message_Size, T_Stat_Record>>> gpu_fills;                      // func,from,to,size,time
    std::map<short, PEAT_Clock> gpu_executions;                                                                  // func,time
    std::map<short, PEAT_Clock> gpu_syncs;                                                                       // func,time
#endif

    char *stat_template;
    char *stat_dir;
    char *tmp_dir;
    int print_names;

    PEAT_Clock first_tsc;
    PEAT_Clock first_par_tsc;
    PEAT_Clock last_par_tsc;
    PEAT_Clock last_tsc;
    void *highval;
    void *binval;
    void *lowval;
    void *highfort;
    void *lowfort;
#ifdef _OPENMPI_
    /* Open MPI has several Fortran wrapper libraries; all of their executable ranges are skipped by stack_unwind* */
    enum { T_MAX_FORT_RANGES = 8 };
    int num_fort_ranges;
    void *lowfort_r[T_MAX_FORT_RANGES];
    void *highfort_r[T_MAX_FORT_RANGES];
#endif
    void *hightrac;
    void *lowtrac;
    void *highheap;
    void *lowheap;
    void *highstack;
    void *lowstack;
    PEAT_Ratio user_region_quality_range;

    gzFile fps_mpi;
    gzFile fps_hwc;
    gzFile fps_com;
    gzFile fps_app;
    char *gz_ext;
    bool compression;
    int compression_level;
    int write_on_finalize;
    t_nodes_info *gt_ninfo;


    int rank_smp, size_smp;
    int rank_master, size_master;
    int rank_cw, size_cw;

#ifdef _TRACTOR_USE_PTPD_
    char *ptpd_outputfile;
    char *ptpd_execfile;
    char *ptpd_nodefile;
    char *ptpd_timenode;
    char *ptpd_ui;
    int use_ptpd;
    char ptpd_in_libpath[MAX_PATH];
    char ptpd_in_binpath[MAX_PATH];
#endif

    char gz_mode[16];
    FILE **fps_events; /* realtime dump: one temporary file per (thread, level), index tid * levels + level */
    FILE **fps_extras;

    MPI_Group grp_world, grp_smp, grp_master;
    MPI_Comm comm_smp, comm_master, TRACTOR_COMM_WORLD;
    t_local_comms *local_comms;
    t_global_comms global_comms;
    std::map<PEAT_Addr, t_address> addr_map;
    std::map<std::string, t_bin> binaries_map;
    std::map<PEAT_Num, shared_object_info> g_shared_objects;
    std::string node_topology;
    tractor(void);
    ~tractor(void);
    virtual void dump_extras(pid_t tid, int level, msl_base_chain **chain);
    virtual void dump_events(pid_t tid, int level, msl_adv_chain **chain);
    int fp_index(int tid, int level) const { return tid * t_max_nesting_levels + level; }

    void init(void);
    int parse_boundaries(char *boundaries);
    int parse_levels(char *levels);
    void update_extras_pointers(int tid, int level, long long *max_ext_count, msl_extra_info **cur_exts_array);

    void finalize(void);
    void abort(void);
    void parse_env_vars(void);
    void begin(void);
    void end(void);
    void begin_comm(const int tid, const MPI_Comm comm, const int local_ranks, const int remote_ranks, const int *ranks);
    void end_comm(const int tid, const MPI_Comm comm);
    void pc_change_state(int ctl);
    void parse_pc_levels(char *levels);
    void ptpd_init(void);
    void ptpd_finalize(void);
    void post_mpi_init(void);
    void post_ptpd_init(void);
    void pre_finalize(int);
    void init_extra(const int tid);
    void finalize_extra(const int tid);
    void nodes_info(t_nodes_info *ninfo);
    void get_rank_time(PEAT_Clock *start, PEAT_Clock *start_parallel, PEAT_Clock *end_parallel, PEAT_Clock *end, PEAT_Clock *overall_tool_overhead, PEAT_Clock *overall_MPI, PEAT_Clock *overall_GPU);
    int t_check_chain(pid_t tid, int level);

    int write_trace(void);
    PEAT_Rank get_real_rank(MPI_Comm comm, PEAT_Rank rank_in, PEAT_Clock tsc, int is_internal);
    PEAT_MPI_Comm get_global_comm_id(MPI_Comm comm, PEAT_Clock tsc, int is_internal);
    num_processed_events write_main_files(/*,user_region_store &user_region_store*/);

    //    void calculate_us_sd_dev(user_region_store &user_region_store);
    void write_com_file(const num_processed_events &num_of_events, /* user_region_store &user_region_store,*/ const PEAT_Clock time_symbols_search);
    void write_stat_com_file(void);
    void print_address_map(const PEAT_Record_Type line_prefix);
    void print_function_report(const PEAT_Record_Type line_prefix, const PEAT_Clock app_start, const PEAT_Clock app_end, const PEAT_Clock overall_tool_overhead, const PEAT_Clock overall_MPI, const PEAT_Clock overall_GPU);
    void print_modules(const PEAT_Record_Type line_prefix);
    void write_app_file(t_nodes_info *ninfo);
    void map_file_parse(void);
    const std::string get_binary_by_addr(PEAT_Addr addr, PEAT_Addr &base, PEAT_Addr &low, PEAT_Addr &high);
    PEAT_Clock find_ELF_symbols(void);
    void get_topo(void);

#ifdef GPUPARSE
    void print_gpu_stat(void);
#endif

};
struct tractor_debug {
    tractor_debug(void);
    ~tractor_debug(void);
    tractor *t_info;
};
#ifdef _TRACTOR_DIAG_
extern tractor_debug t_info_debug;
#else
extern tractor t_info;
#endif
#if 0
long long bounds[] = {0,16,64,256,16384,262144,LLONG_MAX};
const int bound_num=sizeof(bounds)/sizeof(bounds[0]);

struct mpi_statistic_event {
    long long durations[bound_num];
    long long sizes[bound_num];
    long long occurences[bound_num];
};
mpi_statistic_event stat_for_func[MPI_MAX_FUNCID_THROUGH];

int get_group(long long num) {
    for (int i=1;i<bound_num;i++) if (bounds[i-1]<=num && num<bounds[i]) return i-1;
    return bound_num-1;
}
#endif

extern PEAT_Addr highfort;
extern PEAT_Addr lowfort;
const int minonetimealloc = 16;

#if defined(__CPLUSPLUS__) || defined(__cplusplus)
}
#endif
#endif // __TRACTOR_H__
