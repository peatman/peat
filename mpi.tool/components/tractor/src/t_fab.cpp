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

#ifdef _VERBS_PROFILE_
#include <infiniband/verbs.h>
#endif

#include "t_extradef.h"
#include "t_fab.h"
#include "tractor.h"
#include <cstdint>
#include <dlfcn.h>
#include <libgen.h>
#include <limits>
#include <list>
#include <map>
#include <sstream>
#include <stdio.h>
#include <unistd.h>

static int f_init_parse = 0;
extern long long global_tool_overhead;

/* static void init_fab_hooks();

void t_fab_profiler::init_hooks()  {
    init_fab_hooks();
}
*/
t_fab_profiler::t_fab_profiler()
{
    __DEBUG_PROLOG_TRACTOR_LAYER__
    gz_ext = ".gz";
    compression = 1;
    compression_level = Z_DEFAULT_COMPRESSION;
    fps_events = fps_extras = NULL;
    num_addrs =
#ifdef _HWCOUNTERS_ENABLED_
        num_hwcs =
#endif
#ifdef _MEMPROFILE_STORE_
            t_memory_event_size =
#endif
                0;
    __DEBUG_EPILOG_TRACTOR_LAYER__
}

t_fab_profiler::~t_fab_profiler()
{
    __DEBUG_PROLOG_TRACTOR_LAYER__
    if (fps_events) {
        int tid = 0;
        while (fps_events[tid]) {
            fclose(fps_events[tid]);
            char temp_filename[MAX_PATH];
            sprintf(temp_filename, "%s/%s.%d.%d.fevts.tmp", tmp_dir, stat_template, t_mpi_rank, tid);
            debug_printf("Deleting Temporary File %s.\n", temp_filename);
            unlink(temp_filename);
            tid++;
        }
        t_free(fps_events, t_max_threads * sizeof(FILE *));
    }
    if (fps_extras) {
        int tid = 0;
        while (fps_extras[tid]) {
            fclose(fps_extras[tid]);
            char temp_filename[MAX_PATH];
            sprintf(temp_filename, "%s/%s.%d.%d.fexts.tmp", tmp_dir, stat_template, t_mpi_rank, tid);
            debug_printf("Deleting Temporary File %s.\n", temp_filename);
            unlink(temp_filename);
            tid++;
        }
        t_free(fps_extras, t_max_threads * sizeof(FILE *));
    }

    __DEBUG_EPILOG_TRACTOR_LAYER__
}
void t_fab_profiler::init(int max_threads, char *_stat_dir, char *_tmp_dir, char *_stat_template, int _use_realtime_dump)
{
    __DEBUG_PROLOG_TRACTOR_LAYER__
    t_base_event::init(max_threads, 1); // fabric calls never nest
    use_realtime_dump = _use_realtime_dump;
    debug_printf("Fabric Profiler:max_threads=%d. %s %s \n", t_max_threads, _stat_dir, _stat_template);
    stat_template = _stat_template;
    stat_dir = _stat_dir;
    tmp_dir = _tmp_dir;
    if (use_realtime_dump) {
        fps_events = (FILE **)t_malloc(t_max_threads * sizeof(FILE *), 128);
        memset(fps_events, 0, t_max_threads * sizeof(FILE *));
    }
    __DEBUG_EPILOG_TRACTOR_LAYER__
}

void t_fab_profiler::dump_extras(pid_t tid, int level, msl_base_chain **chain)
{
    __DEBUG_PROLOG_TRACTOR_LAYER__
    __DEBUG_EPILOG_TRACTOR_LAYER__
}
void t_fab_profiler::dump_events(pid_t tid, int level, msl_adv_chain **chain)
{
    __DEBUG_PROLOG_TRACTOR_LAYER__
    if (fps_events[tid] == NULL) {
        char temp_filename[MAX_PATH];
        sprintf(temp_filename, "%s/%s.%d.%d.fevts.tmp", tmp_dir, stat_template, t_mpi_rank, tid);
        fps_events[tid] = fopen(temp_filename, "w+b");
        if (fps_events[tid])
            debug_printf("File %s opened..\n", temp_filename);
    }
    if (fps_events[tid]) {
        fwrite((*chain)->events_array, (*chain)->events_size, (*chain)->num_events, fps_events[tid]);
        debug_printf("Dumping %ld events.\n", (*chain)->num_events);
        destroy_shift_start((msl_base_chain **)chain);
    }

    __DEBUG_EPILOG_TRACTOR_LAYER__
}

long long t_fab_profiler::write_fab_file(bool print_names)
{
    __DEBUG_PROLOG_TRACTOR_LAYER__
    if (calls_limit == 0) {
    __DEBUG_EPILOG_TRACTOR_LAYER__
        return 0;
    }
    char tfilename[1024];
    char filename[1024];
    char pid[16];
    char gz_mode[16];
    pid_t upid = getpid();
    sprintf(tfilename, "%s/%s", stat_dir, stat_template);
    long long uniq_index = 0;
    // strcpy(tfilename, stat_template);
    strcpy(gz_mode, "w"); // simple text output for all files
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
    if (t_mpi_rank >= 0)
        sprintf(pid, ".%u", t_mpi_rank); // else sprintf(pid, ".%u.%d", upid, i);
    sprintf(filename, "%s.fab%s%s", tfilename, pid, gz_ext);
    gzFile fps = gzopen(filename, gz_mode);
    debug_printf("FAB Events:max_threads=%d. %d %d \n", t_max_threads, onetimealloc, num_addrs);

    if (fps) {
        int tsc_mpi_entry_ind = 1;
        int tsc_mpi_exit_ind = 2;
        long long tsc_counter = 0;
        msl_adv_chain *current_chain[t_max_threads];
        msl_event *current_array[t_max_threads];

        long long record_counter[t_max_threads];
        long long max_events[t_max_threads];
        std::map<t_timeline, int> timeline;
        if (use_realtime_dump) {
            for (int i = 0; i < t_tid_counter; i++) {
                events_chain[i][0]->num_events = event_index[i][0];
                dump_events(i, 0, &events_start_chain[i][0]);
                current_array[i] = (msl_event *)t_malloc((onetimealloc + 1) * sizeof(msl_event), 128);
                rewind(fps_events[i]);
                record_counter[i] = 0;
                max_events[i] = fread(current_array[i], sizeof(msl_event), onetimealloc, fps_events[i]);
                debug_printf("TID %d, %ld events read back.\n", i, max_events[i]);
                t_timeline t_tl(current_array[i][0].tscs[0], i);
                if (max_events[i] > 0)
                    timeline[t_tl] = i;
            }
        } else {
            for (int i = 0; i < t_tid_counter; i++) {
                current_chain[i] = events_start_chain[i][0];

                record_counter[i] = 0;
                max_events[i] = (current_chain[i]->next == NULL) ? event_index[i][0] : onetimealloc;

                current_array[i] = (msl_event *)current_chain[i]->events_array;
                debug_printf("FAB Events:thread %d. %d %llx, %llx - %llx %llx \n", i, max_events[i], this, current_chain[i], events_start_chain[i], current_array[i]);
                t_timeline t_tl(current_array[i][0].tscs[0], i);
                if (max_events[i] > 0)
                    timeline[t_tl] = i;
            }
        }
        while (!timeline.empty()) {
            std::map<t_timeline, int>::iterator timeline_it;
            timeline_it = timeline.begin();
            if (timeline_it == timeline.end()) {
                std::ostringstream err_txt;
                err_txt << "Internal timeline error\n";
                throw std::runtime_error(err_txt.str());
            }
            int tid = timeline_it->second;
            timeline.erase(timeline_it);
            uniq_index++;
            if (print_names) {
                gzprintf(fps, "%lld:%d:%s",
                         uniq_index, tid,                                                       // 0,1
                         mpi_function_names[current_array[tid][record_counter[tid]].func_id] // 2
                );
            } else {
                gzprintf(fps, "%lld:%d:%d",
                         uniq_index, tid,                                   // 0,1
                         mpi_func_ids_trans[current_array[tid][record_counter[tid]].func_id] // 2
                );
            }

            long long l_tscs[4];
            l_tscs[0] = current_array[tid][record_counter[tid]].tscs[0];
            l_tscs[1] = current_array[tid][record_counter[tid]].tscs[1];
            l_tscs[2] = current_array[tid][record_counter[tid]].tscs[2];
            l_tscs[3] = current_array[tid][record_counter[tid]].tscs[3];
            l_tscs[0] -= tsc_counter;
            tsc_counter = current_array[tid][record_counter[tid]].tscs[0];
            int l_num_calls = current_array[tid][record_counter[tid]].fab.num_calls;
            long long total_clocks_bitween_calls = current_array[tid][record_counter[tid]].fab.total_clocks_bitween_calls;
            gzprintf(fps, ":%lld:%lld:%lld:%lld:%lld:%d\n", l_tscs[0], l_tscs[1], l_tscs[2], l_tscs[3], total_clocks_bitween_calls, l_num_calls);
            record_counter[tid]++;
            if (record_counter[tid] >= max_events[tid]) {
                if (use_realtime_dump) {
                    record_counter[tid] = 0;
                    max_events[tid] = fread(current_array[tid], sizeof(msl_event), onetimealloc, fps_events[tid]);
                    debug_printf("TID %d, %ld events read back.\n", tid, max_events[tid]);
                } else {
                    if (current_chain[tid]->next != NULL) {
                        current_chain[tid] = (msl_adv_chain *)current_chain[tid]->next;
                        max_events[tid] = (current_chain[tid]->next == NULL) ? event_index[tid][0] : onetimealloc;
                        current_array[tid] = (msl_event *)current_chain[tid]->events_array;
                        record_counter[tid] = 0;
                    } else {
                        max_events[tid] = 0;
                    }
                }
            }
            if (max_events[tid] > 0) {
                t_timeline t_tl(current_array[tid][record_counter[tid]].tscs[0], tid);
                timeline[t_tl] = tid;
            }
        }
        if (use_realtime_dump) {
            for (int i = 0; i < t_tid_counter; i++) {
                t_free(current_array[i], (onetimealloc + 1) * sizeof(msl_event));
            }
        }
        gzclose(fps);
    }
    return uniq_index;
    __DEBUG_EPILOG_TRACTOR_LAYER__
}
#if defined(_FAB_PROFILE_)
#ifndef _TRACTOR_FAB_LIB_SUFFIX_
#define _TRACTOR_FAB_LIB_SUFFIX_ ""
#endif


#if defined(__CPLUSPLUS__) || defined(__cplusplus)
extern "C" {
#endif
/*
#ifdef _VERBS_PROFILE_
#include "t_fab_layer_h.h"
#endif
#include "t_fab_layer.h"

static void init_fab_hooks()  {
   if (!f_init_parse) {
     #include "t_fab_layer_const.h"
   }
    //printf ("init_hooks()\n");
    f_init_parse=1;
}
*/
#if defined(__CPLUSPLUS__) || defined(__cplusplus)
} // extern "C"
#endif
#endif
