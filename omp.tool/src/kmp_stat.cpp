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

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>
// #include "kmp.h"    // KMP_TEST_THEN_INC32
#include "kmp_stat.h"

#ifdef USE_PAPI
#include "papi.h"
#endif

#ifdef __DEBUG__
#define debug_printf printf
#else
void debug_printf(const char *format, ...) {};
#endif

#ifndef MAX_PATH
#define MAX_PATH 2048
#endif

#define MAX_NUM_CPU_EVENTS 8

int kmp_sec_ret_addr = 0;
void *kmp_highval = (void *)0x7FFFFFFFFFFFFFFF;
void *kmp_binval = (void *)0x7FFFFFFFFFFFFFFF;
void *kmp_lowval = (void *)0;

static long long uniq_id = 0;
typedef struct _microtask_event_chain_ microtask_event_chain;

typedef struct _microtask_event_chain_ {
    microtask_event_chain *next;
    microtask_event *event_array;
    int ready_for_store;
} microtask_event_chain;

static microtask_event **current_events;
static microtask_event_chain **start, **current;
const int defonetimealloc = 100000;
static int *event_index;
static int *event_printed;
static char *thread_shutdown;
static char *papi_thread_init;
static int app_max_threads;
static int app_max_cores;
static long long proc_freq = 0;
static char *executable_file;
static char *stat_template;
static char *mpi_rank;
static long long start_tsc = 0, end_tsc = 0x7fffffffffffffff;
#ifdef USE_PAPI

static int cpu_events[MAX_NUM_CPU_EVENTS] = {PAPI_TOT_INS, PAPI_TOT_CYC};
static int cpu_events_temp[MAX_NUM_CPU_EVENTS];
static char counter_description[MAX_NUM_CPU_EVENTS][32] = {"PAPI_TOT_INS", "PAPI_TOT_CYC"};
static char counter_description_temp[MAX_NUM_CPU_EVENTS][32];

static int num_cpu_events = 2;
#endif

FILE **fps_t;
FILE **fps_b;

static int async_write = 0;
static int async_shutdown = 0;
static pthread_t async_write_thread;
static pthread_mutex_t async_write_mutex = PTHREAD_MUTEX_INITIALIZER;
static int use_thread_mutex = 0;
static pthread_mutex_t *thread_mutex;

const int ASYNC_SLEEP_TIME = 1;
static int events_enabled[OSL_LIST_END + 1];
static int onetimealloc;
static int condemand = 1;
static double cpause = 0.0;
static double cduration = 0.0;
static int use_binary_files = 0;
static int repack = 0;
static int is_true(char *str)
{
    if (str == NULL)
        return 0;
    if (strcmp(str, "1") == 0)
        return 1;
    if (strcmp(str, "yes") == 0)
        return 1;
    if (strcmp(str, "on") == 0)
        return 1;
    return 0;
}

#define FDELAY 1
static void calc_freq(void)
{

    char *freq_template = getenv("OSL_FREQUENCY");
    if (freq_template) {
        proc_freq = atoll(freq_template); /* in Hz */
    } else {
        long long fc, sc;
        sc = rdtsc();
        sleep(FDELAY);
        fc = rdtsc();
        proc_freq = (long long)((double)(fc - sc) / ((double)FDELAY)); /* in Hz */
    }
}

static microtask_event_chain *add_new_cell(microtask_event_chain *chain)
{
    microtask_event_chain *new_chain = (microtask_event_chain *)malloc(sizeof(microtask_event_chain));
    memset(new_chain, 0, sizeof(microtask_event_chain));
    new_chain->event_array = (microtask_event *)malloc((onetimealloc + 1) * sizeof(microtask_event));
    if (chain != NULL)
        chain->next = new_chain;
    return (new_chain);
}

static int destroy_chain(microtask_event_chain *chain)
{
    microtask_event_chain *next;
    while (chain != NULL) {
        next = chain->next;
        if (chain->event_array)
            free(chain->event_array);
        free(chain);
        chain = next;
    }
    return (0);
}

static int destroy_shift_start(microtask_event_chain **start)
{
    microtask_event_chain *next;
    next = (*start)->next;
    if ((*start)->event_array)
        free((*start)->event_array);
    free(*start);
    *start = next;
    return (0);
}

#ifdef __DEBUG_OUTPUT__
static const char *event_description[] = {
    "Unknown",
    "Microtask_Starts", "Microtask_Ends",
    "Taskwait_Starts", "Taskwait_Ends",
    "Task_Starts", "Task_Ends",
    "Critical_Section_Starts", "Critical_Section_Ends",
    "Critical_Barrier_Starts", "Critical_Barrier_Ends",
    "Plain_Barrier_Starts", "Plain_Barrier_Ends",
    "Fork_Barrier_Starts", "Join_Barrier_Ends",
    "Join_Barrier_Starts", "Join_Barrier_Ends",
    "Single_Starts", "Single_Ends",
    "Application_Starts", "Application_Ends",
    "Calibration_Starts",
    "Frequency",
    "Executable_File",
    "Serialized_Microtask_Starts", "Serialized_Microtask_Ends",
    "Team_Microtask_Starts", "Team_Microtask_Ends",
    "Fork_Call",
    "Start_On_Demand", "Stop_On_Demand", "Counter_Description"};
#else
static const char *event_description[] = {
    "U",
    "MS", "ME",
    "TWS", "TWE",
    "TS", "TE",
    "CSS", "CSE",
    "CBS", "CBE",
    "PBS", "PBE",
    "FBS", "FBE",
    "JBS", "JBE",
    "SS", "SE",
    "AS", "AE",
    "CLS",
    "F",
    "E",
    "SMS", "SME",
    "TMS", "TME",
    "FC", "TF",
    "STOD", "SPOD", "CD"};
#endif
static int print_thread_stat(int gtid, int print_all_chain)
{
    int max_events, i, j;
    microtask_event_chain *current_chain = start[gtid];
    microtask_event *current_array;
    if (event_printed[gtid])
        return -1;
    // printf("Printing Thread %d statistics,events in current chain %d,\n",tid,event_index[gtid]);
    do {
        max_events = (current_chain->next == NULL && print_all_chain) ? event_index[gtid] : onetimealloc;
        current_array = current_chain->event_array;
        if (use_binary_files) {
            int fake_ret = fwrite(current_array, sizeof(current_array[0]), max_events, fps_b[gtid]);
        } else {
            fprintf(fps_t[gtid], "Begin Chain:max_events %d:\n", max_events);
            for (i = 0; i < max_events; i++) {
                /*         if (current_array[i].event_type==OSL_TASKFORK_CALL || current_array[i].event_type==OSL_FORK_CALL) { */
                //                                                                                1                                       2
                fprintf(fps_t[gtid], "OSE:%s:%lld:%d:%d:%d:%lld:%p", event_description[current_array[i].event_type], current_array[i].uniq_id,
                        //                                                     3                    4                    5
                        current_array[i].tid, current_array[i].gtid, current_array[i].gtmid,
                        //                                                     6
                        current_array[i].tsc,
                        //                                                     7
                        current_array[i].p_entry);
/*         } else {
             fprintf(fps_t[gtid],"OSE:%s:%lld:%d:%d:%d:%lld:%p",event_description[current_array[i].event_type],current_array[i].uniq_id,
                                            current_array[i].tid,current_array[i].gtid,current_array[i].gtmid,
                                            current_array[i].tsc,
                                            current_array[i].p_entry);
         }
*/
#ifdef USE_PAPI
                for (j = 0; j < num_cpu_events; j++) {
                    fprintf(fps_t[gtid], ":%lld", current_array[i].counters[j]);
                }
#endif
                fprintf(fps_t[gtid], "\n");
            }
            fprintf(fps_t[gtid], "End Chain\n");
        }
        current_chain = current_chain->next;
    } while (max_events >= onetimealloc && print_all_chain);
    if (print_all_chain)
        event_printed[gtid] = 1;
    return (0);
}

#ifdef USE_PAPI
void __osl_papi_init_thread(int gtid)
{
    int retval;
    if ((retval = PAPI_thread_init(pthread_self)) != PAPI_OK) {
        fprintf(stderr, "Error: Papi Thread Not intialized %d\n", retval);
        exit(1);
    }
    if ((retval = PAPI_start_counters(cpu_events, num_cpu_events)) != PAPI_OK) {
        fprintf(stderr, "Error: Papi Not started %d. Thread %d\n", retval, gtid);
        exit(1);
    }

    //      fprintf(stderr, "Papi Thread %d intialized \n",gtid);
}
#endif

long long __osl_record_event(int event_type, int tid, int gtid, int gtmid, const void *p_entry, long long uniq_id_in, int forced)
{

    if (thread_shutdown && events_enabled[event_type] && (((condemand & 1) * 2) == (condemand & 2) || forced)) {
#ifdef USE_PAPI
        if (papi_thread_init[gtid] == 0) {
            __osl_papi_init_thread(gtid);
            papi_thread_init[gtid] = 1;
        }
#endif

        if (thread_shutdown[gtid] == 0) {
            long long tsc = rdtsc();
            if (tsc >= start_tsc && tsc <= end_tsc) {
                /*       if (event_type==OSL_FORK_CALL || event_type==OSL_START_ONDEMAND || event_type==OSL_STOP_ONDEMAND ) {
                          int i;
                          for (i=0;i<app_max_threads;i++) {
                              current_events[i][event_index[i]].event_type=event_type;
                              current_events[i][event_index[i]].tsc=tsc;
                              current_events[i][event_index[i]].tid=i;
                              current_events[i][event_index[i]].gtid=gtid;
                              current_events[i][event_index[i]].p_entry=p_entry;
                              event_index[i]++;
                              if (event_index[i]>=onetimealloc)
                              {
                                  microtask_event_chain *temp=current[i];
                                  current[i]=add_new_cell(current[i]);
                                  temp->ready_for_store=1;
                                  current_events[i]=current[i]->event_array;
                                  event_index[i]=0;
                              }
                          }
                       }else*/
                {
                    /*         fprintf(stderr,"OSE:%d:%d:%d:%p %lld %lld %lld \n",tid,gtid,event_type,p_entry,tsc,start_tsc,end_tsc);*/
                    if (use_thread_mutex) {
                        pthread_mutex_lock(&thread_mutex[gtid]);
                    }
                    if (uniq_id_in == -1)
                        uniq_id_in = uniq_id++;
                    int oldindex = event_index[gtid]++;
                    current_events[gtid][oldindex].event_type = event_type;
                    current_events[gtid][oldindex].tsc = tsc;
                    current_events[gtid][oldindex].uniq_id = uniq_id_in;
                    current_events[gtid][oldindex].tid = tid;
                    current_events[gtid][oldindex].gtid = gtid;
                    current_events[gtid][oldindex].gtmid = gtmid;
                    current_events[gtid][oldindex].p_entry = (void *)p_entry;
#ifdef USE_PAPI
                    PAPI_read_counters(current_events[gtid][oldindex].counters, num_cpu_events);
#endif
                    if (event_index[gtid] >= onetimealloc) {
                        microtask_event_chain *temp = current[gtid];
                        current[gtid] = add_new_cell(current[gtid]);
                        temp->ready_for_store = 1;
                        current_events[gtid] = current[gtid]->event_array;
                        event_index[gtid] = 0;
                    }
                    if (use_thread_mutex) {
                        pthread_mutex_unlock(&thread_mutex[gtid]);
                    }
                }
            }
        }
    }
    return uniq_id_in;
}

static void handle_events(int gtid)
{
    print_thread_stat(gtid, 0);
    destroy_shift_start(&start[gtid]);
}

static void *async_write_func(void *vptr_args)
{
    int i;
    FILE **fps;
    if (use_binary_files) {
        fps = fps_b;
    } else {
        fps = fps_t;
    }
    pthread_mutex_lock(&async_write_mutex);
    do {
        int events_chains;
        for (i = 0, events_chains = 0; i < app_max_threads; i++) {
            if (start[i]->ready_for_store == 1) {
                events_chains++;
                handle_events(i);
            }
        }
        if (events_chains == 0) {
            sleep(ASYNC_SLEEP_TIME);
            // KMP_CPU_PAUSE();
            // KMP_YIELD(1);
        }
    } while (!async_shutdown);

    for (i = 0; i < app_max_threads; i++) {
        fflush(fps[i]);
    }

    pthread_mutex_unlock(&async_write_mutex);

    return NULL;
}
/*
    "Microtask_Starts","Microtask_Ends",
    "Task_Starts","Task_Ends",
    "Critical_Section_Starts","Critical_Section_Ends",
    "Critical_Barrier_Starts","Critical_Barrier_Ends",
    "Plain_Barrier_Starts","Plain_Barrier_Ends",
    "Fork_Barrier_Starts","Join_Barrier_Ends",
    "Join_Barrier_Starts","Join_Barrier_Ends",
    "Single_Starts","Single_Ends",
    OSL_EMPTY_EVENT=0,
    OSL_MICROTASK_STARTS,
    OSL_MICROTASK_ENDS,
    OSL_CRITICAL_SECTION_STARTS,
    OSL_CRITICAL_SECTION_ENDS,
    OSL_CRITICAL_BARRIER_STARTS,
    OSL_CRITICAL_BARRIER_ENDS,
    OSL_PLAIN_BARRIER_STARTS,
    OSL_PLAIN_BARRIER_ENDS,
    OSL_FORK_BARRIER_STARTS,
    OSL_FORK_BARRIER_ENDS,
    OSL_JOIN_BARRIER_STARTS,
    OSL_JOIN_BARRIER_ENDS,
    OSL_SINGLE_STARTS,
    OSL_SINGLE_ENDS,

*/
#ifdef USE_PAPI
int parse_papi_events(char *events)
{
    char *temp1, *temp2;
    int i = 0 /*,code*/;
    /*   fprintf (stderr,"entering parse\n");*/
    temp1 = events;
    while ((temp2 = strchr(temp1, ',')) != NULL && i < MAX_NUM_CPU_EVENTS) {
        memcpy(counter_description_temp[i], temp1, temp2 - temp1);
        counter_description_temp[i][temp2 - temp1] = 0;
        /*      fprintf (stderr,"found event ->%s<-\n",counter_description_temp[i]);*/
        if ((/*code=*/PAPI_event_name_to_code(counter_description_temp[i], &cpu_events_temp[i])) == PAPI_OK) {
            i++;
            /*         fprintf (stderr,"success\n");
                  }else{
                     fprintf (stderr,"failure %d\n",code);*/
        }
        temp1 = temp2 + 1;
    };

    if (strlen(temp1) > 0 && i < MAX_NUM_CPU_EVENTS) {
        strcpy(counter_description_temp[i], temp1);
        /*      fprintf (stderr,"found event ->%s<-\n",counter_description_temp[i]);*/
        if ((/*code=*/PAPI_event_name_to_code(counter_description_temp[i], &cpu_events_temp[i])) == PAPI_OK) {
            i++;
            /*         fprintf (stderr,"success\n");
                  }else{
                     fprintf (stderr,"failure %d\n",code);*/
        }
    }
    return i;
}
#endif

void parse_env_vars(void)
{
    int individual_event_selection = 0;
    char *temp;
    char *endptr;
    stat_template = getenv("OSL_FILE");
    executable_file = getenv("OSL_EXECUTABLE_FILE");
    mpi_rank = getenv("PMI_RANK");
    async_write = is_true(getenv("OSL_ASYNC_WRITE"));
    individual_event_selection = is_true(getenv("OSL_EVENT_SELECTION"));
    memset(&events_enabled, -1, sizeof(events_enabled));
    if (individual_event_selection) {
        events_enabled[OSL_MICROTASK_STARTS] = events_enabled[OSL_MICROTASK_ENDS] =
            events_enabled[OSL_SERIALIZED_MICROTASK_STARTS] = events_enabled[OSL_SERIALIZED_MICROTASK_ENDS] =
                is_true(getenv("OSL_EVENT_MICROTASK"));
        events_enabled[OSL_TASK_STARTS] = events_enabled[OSL_TASK_ENDS] =
            is_true(getenv("OSL_EVENT_TASK"));
        events_enabled[OSL_TASKWAIT_STARTS] = events_enabled[OSL_TASKWAIT_ENDS] =
            is_true(getenv("OSL_EVENT_TASKWAIT"));
        events_enabled[OSL_CRITICAL_SECTION_STARTS] = events_enabled[OSL_CRITICAL_SECTION_ENDS] =
            is_true(getenv("OSL_EVENT_CRITICAL_SECTION"));
        events_enabled[OSL_CRITICAL_BARRIER_STARTS] = events_enabled[OSL_CRITICAL_BARRIER_ENDS] =
            is_true(getenv("OSL_EVENT_CRITICAL_BARRIER"));
        events_enabled[OSL_PLAIN_BARRIER_STARTS] = events_enabled[OSL_PLAIN_BARRIER_ENDS] =
            is_true(getenv("OSL_EVENT_PLAIN_BARRIER"));
        events_enabled[OSL_FORK_BARRIER_STARTS] = events_enabled[OSL_FORK_BARRIER_ENDS] =
            is_true(getenv("OSL_EVENT_FORK_BARRIER"));
        events_enabled[OSL_JOIN_BARRIER_STARTS] = events_enabled[OSL_JOIN_BARRIER_ENDS] =
            is_true(getenv("OSL_EVENT_JOIN_BARRIER"));
        events_enabled[OSL_SINGLE_STARTS] = events_enabled[OSL_SINGLE_ENDS] =
            is_true(getenv("OSL_EVENT_SINGLE"));
        events_enabled[OSL_FORK_CALL] = is_true(getenv("OSL_EVENT_FORK_CALL"));
    }
    onetimealloc = defonetimealloc;
    temp = getenv("OSL_ONE_TIME_ALLOC");
    if (temp)
        onetimealloc = atoll(temp);
    use_binary_files = is_true(getenv("OSL_USE_BINARY_FILES"));
    if (use_binary_files)
        repack = is_true(getenv("OSL_REPACK"));
    use_thread_mutex = is_true(getenv("OSL_USE_THREAD_MUTEX"));

    condemand = is_true(getenv("OSL_ONDEMAND")) << 1; // 0 or 2
    temp = getenv("OSL_PAUSE");
    if (temp)
        cpause = atof(temp);
    temp = getenv("OSL_DURATION");
    if (temp)
        cduration = atof(temp);
    temp = getenv("OSL_2ND_RETURN_LEVEL");
    if (temp)
        kmp_sec_ret_addr = atol(temp);
    temp = getenv("OSL_MIN_ADDRESS");
    if (temp)
        kmp_lowval = (void *)strtoull(temp, &endptr, 16);
    temp = getenv("OSL_MAX_ADDRESS");
    if (temp)
        kmp_highval = (void *)strtoull(temp, &endptr, 16);

#ifdef USE_PAPI
    temp = getenv("OSL_PAPI_EVENTS");
    if (temp) {
        int num_parsed_cpu_events = parse_papi_events(temp);
        if (num_parsed_cpu_events > 0) {
            memcpy(counter_description, counter_description_temp, sizeof(counter_description));
            memcpy(cpu_events, cpu_events_temp, sizeof(cpu_events_temp));
            num_cpu_events = num_parsed_cpu_events;
        }
    }
#endif
}
void __osl_begin_application(int max_threads, int max_cores)
{
    int i, j, k;
    long long tsc;
#ifdef USE_PAPI
    long long papi_adj_counters[16][MAX_NUM_CPU_EVENTS], record_adj_counters[MAX_NUM_CPU_EVENTS];
#endif
    FILE *fp;
    char tfilename[1024];
    char filename[1024];
    char bfilename[1024];
    char pid[16];
    int ipid = getpid();
#ifdef USE_PAPI
    int retval;
    if ((retval = PAPI_library_init(PAPI_VER_CURRENT)) != PAPI_VER_CURRENT) {
        fprintf(stderr, "Error: Papi Not intialized %d\n", retval);
        exit(1);
    }
#endif

    parse_env_vars();
    if (stat_template)
        strcpy(tfilename, stat_template);
    else
        strcpy(tfilename, "omp_stat.log");
    if (proc_freq == 0)
        calc_freq();

    fps_t = (FILE **)malloc(max_threads * sizeof(FILE *));
    if (use_binary_files)
        fps_b = (FILE **)malloc(max_threads * sizeof(FILE *));

    if (use_thread_mutex) {
        thread_mutex = (pthread_mutex_t *)malloc(max_threads * sizeof(pthread_mutex_t));
        for (i = 0; i < max_threads; i++) {
            pthread_mutex_init(&thread_mutex[i], NULL);
        }
    }

    for (i = 0; i < max_threads; i++) {
        if (mpi_rank)
            sprintf(pid, ".%s.%d", mpi_rank, i);
        else
            sprintf(pid, ".%d.%d", ipid, i);
        strcpy(filename, tfilename);
        strcat(filename, pid);
        if (use_binary_files) {
            strcpy(bfilename, "bin-");
            strcat(bfilename, filename);
            fp = fopen(bfilename, "w+b");
            fps_b[i] = fp;
            if (repack) {
                fp = fopen(filename, "w+t");
            } else {
                strcpy(bfilename, "txt-");
                strcat(bfilename, filename);
                fp = fopen(bfilename, "w+t");
            }
            fps_t[i] = fp;
        } else {
            fp = fopen(filename, "w+t");
            fps_t[i] = fp;
        }
#pragma warning(disable : 2218)
        fprintf(fps_t[i], "Application begins. Max Threads %d, cores %d.Async write: %d. Thread mutex %d.\n", max_threads, max_cores, async_write, use_thread_mutex);
        if (i == 0) {
            FILE *tfp, *tfp1;
            char tbuf[65536];
            sprintf(tbuf, "/proc/%d/maps", ipid);
            tfp = fopen(tbuf, "rt");
            sprintf(tbuf, "maps.%s", filename);
            tfp1 = fopen(tbuf, "wt");
            if (tfp && tfp1) {
                unsigned long long stub;
                tbuf[0] = 0;
                /* handle main binary */
                fgets(tbuf, MAX_PATH, tfp);
                sscanf(tbuf, "%llx-%llx", &stub, &kmp_binval);
                fprintf(fps_t[i], "HIGHLOWBIN parsed values: %llx %llx %llx\n", kmp_binval, kmp_lowval, kmp_highval);
                fputs(tbuf, tfp1);
                while (!feof(tfp)) {
                    tbuf[0] = 0;
                    fgets(tbuf, 65536, tfp);
                    if (tbuf[0] == 0)
                        break;
                    fputs(tbuf, tfp1);
                }
                fclose(tfp);
                fclose(tfp1);
            }
        }
    }
    start = (microtask_event_chain **)malloc(max_threads * sizeof(microtask_event_chain *));
    current = (microtask_event_chain **)malloc(max_threads * sizeof(microtask_event_chain *));
    current_events = (microtask_event **)malloc(max_threads * sizeof(microtask_event *));
    event_index = (int *)malloc(max_threads * sizeof(int));
    event_printed = (int *)malloc(max_threads * sizeof(int));
    thread_shutdown = (char *)malloc(max_threads * sizeof(char));
    papi_thread_init = (char *)malloc(max_threads * sizeof(char));

    tsc = rdtsc();
    for (i = 0; i < max_threads; i++) {
        current[i] = add_new_cell(NULL);
        start[i] = current[i];
        current_events[i] = start[i]->event_array;
        thread_shutdown[i] = 0;
        papi_thread_init[i] = 0;
        event_printed[i] = 0;
#ifdef USE_PAPI
        if (i == 0) {
            if ((retval = PAPI_start_counters(cpu_events, num_cpu_events)) != PAPI_OK) {
                fprintf(stderr, "Error: Papi Not started %d\n", retval);
                exit(1);
            }
            int tcondemand = condemand;
            condemand = 0;

            PAPI_read_counters(papi_adj_counters[0], num_cpu_events);
            PAPI_read_counters(papi_adj_counters[0], num_cpu_events);
            PAPI_read_counters(papi_adj_counters[1], num_cpu_events);
            PAPI_read_counters(papi_adj_counters[2], num_cpu_events);
            PAPI_read_counters(papi_adj_counters[3], num_cpu_events);
            PAPI_read_counters(papi_adj_counters[4], num_cpu_events);
            PAPI_read_counters(papi_adj_counters[5], num_cpu_events);
            PAPI_read_counters(papi_adj_counters[6], num_cpu_events);
            PAPI_read_counters(papi_adj_counters[7], num_cpu_events);
            PAPI_read_counters(papi_adj_counters[8], num_cpu_events);
            PAPI_stop_counters(papi_adj_counters[9], num_cpu_events);
            __osl_record_event(OSL_APPLICATION_STARTS, i, i, i, NULL, -1, 0);
            __osl_record_event(OSL_APPLICATION_STARTS, i, i, i, NULL, -1, 0);
            __osl_record_event(OSL_APPLICATION_STARTS, i, i, i, NULL, -1, 0);
            __osl_record_event(OSL_APPLICATION_STARTS, i, i, i, NULL, -1, 0);
            __osl_record_event(OSL_APPLICATION_STARTS, i, i, i, NULL, -1, 0);
            __osl_record_event(OSL_APPLICATION_STARTS, i, i, i, NULL, -1, 0);
            __osl_record_event(OSL_APPLICATION_STARTS, i, i, i, NULL, -1, 0);
            __osl_record_event(OSL_APPLICATION_STARTS, i, i, i, NULL, -1, 0);
            __osl_record_event(OSL_APPLICATION_STARTS, i, i, i, NULL, -1, 0);
            __osl_record_event(OSL_APPLICATION_STARTS, i, i, i, NULL, -1, 0);
            PAPI_read_counters(record_adj_counters, num_cpu_events);
            condemand = tcondemand;
            for (j = 0; j < num_cpu_events; j++) {
                for (k = 1; k < 10; k++) {
                    record_adj_counters[j] += current_events[0][k].counters[j];
                    papi_adj_counters[0][j] += papi_adj_counters[k][j];
                }
                record_adj_counters[j] /= 10;
                papi_adj_counters[0][j] /= 10;
            }
        }

#endif
        current_events[i][0].event_type = OSL_APPLICATION_STARTS;
        current_events[i][0].tsc = tsc;
        current_events[i][0].tid = 0;
        current_events[i][0].gtid = i;
        current_events[i][0].gtmid = 0;
        current_events[i][0].uniq_id = 0;
        current_events[i][0].p_entry = NULL;
        event_index[i] = 1;
        fprintf(fps_t[i], "OSE:%s:%d:%d:%lld\n", event_description[OSL_FREQUENCY], i, i, proc_freq);
#ifdef USE_PAPI
        fprintf(fps_t[i], "OSE:%s:%d", event_description[OSL_EVENT_DESCRIPTION], num_cpu_events);
        for (j = 0; j < num_cpu_events; j++) {
            fprintf(fps_t[i], ":%s:%lld:%lld", counter_description[j], papi_adj_counters[0][j], record_adj_counters[j]);
        }
        fprintf(fps_t[i], "\n");
#endif
    }
    app_max_cores = max_cores;
    app_max_threads = max_threads;

    if (async_write) {
        // async stuff follows here
        async_write = 1;
        if (pthread_create(&async_write_thread, NULL, async_write_func, NULL) != 0 || pthread_mutex_init(&async_write_mutex, NULL) != 0) {
            async_write = -1;
        }
    }
    if (cpause > 0 || cduration > 0) {
        start_tsc = tsc + proc_freq * cpause;
        if (cduration > 0)
            end_tsc = tsc + proc_freq * (cpause + cduration);
        for (i = 0; i < max_threads; i++) {
            current_events[i][1].event_type = OSL_START_ONDEMAND;
            current_events[i][1].tsc = start_tsc;
            current_events[i][1].tid = 0;
            current_events[i][1].gtid = i;
            current_events[i][1].p_entry = NULL;
            current_events[i][2].event_type = OSL_STOP_ONDEMAND;
            current_events[i][2].tsc = end_tsc;
            current_events[i][2].tid = 0;
            current_events[i][2].gtid = i;
            current_events[i][2].p_entry = NULL;
            event_index[i] = 3;
            event_printed[i] = 0;
            thread_shutdown[i] = 0;
        }
        condemand = 0;
    }
}

void __osl_end_application(void)
{
    long long tsc = rdtsc();
    int i, j;
    if (async_write > 0) {
        async_shutdown = 1;
        // wait for async_thread
        pthread_mutex_lock(&async_write_mutex);
    }

    if (thread_shutdown != NULL) {
        for (i = 0; i < app_max_threads; i++) {
            fprintf(fps_t[i], "Application Ends.\n");
            if (event_printed[i] == 0) {
                current_events[i][event_index[i]].event_type = OSL_APPLICATION_ENDS;
                current_events[i][event_index[i]].tsc = tsc;
                current_events[i][event_index[i]].gtmid = 0;
                current_events[i][event_index[i]].tid = i;
                current_events[i][event_index[i]].gtid = i;
                current_events[i][event_index[i]].p_entry = NULL;
                event_index[i]++;
                thread_shutdown[i] = 1;
                print_thread_stat(i, 1);
                if (use_binary_files)
                    fflush(fps_b[i]);
                else
                    fflush(fps_t[i]);
                destroy_chain(start[i]);
                if (repack) {
                    rewind(fps_b[i]);
                    microtask_event temp_event;
                    while (!feof(fps_b[i])) {
                        temp_event.event_type = OSL_EMPTY_EVENT;
                        int fake_ret = fread(&temp_event, sizeof(temp_event), 1, fps_b[i]);
                        if (temp_event.event_type != OSL_EMPTY_EVENT)
                            fprintf(fps_t[i], "OSE:%s:%lld:%d:%d:%d:%lld:%p", event_description[temp_event.event_type], temp_event.uniq_id,
                                    temp_event.tid, temp_event.gtid, temp_event.gtmid,
                                    temp_event.tsc,
                                    temp_event.p_entry);
#ifdef USE_PAPI
                        for (j = 0; j < num_cpu_events; j++) {
                            fprintf(fps_t[i], ":%lld", temp_event.counters[j]);
                        }
#endif
                        fprintf(fps_t[i], "\n");
                    }
                    fflush(fps_t[i]);
                    __osl_calibrate_counters(i);
                }
            }
        }
        free(thread_shutdown);
        thread_shutdown = NULL;
        free(start);
    }
    if (async_write > 0)
        pthread_mutex_unlock(&async_write_mutex);
    if (use_thread_mutex && thread_mutex) {
        for (i = 0; i < app_max_threads; i++) {
            pthread_mutex_destroy(&thread_mutex[i]);
        }
        free(thread_mutex);
        thread_mutex = NULL;
    }
}

void __osl_calibrate_counters(int gtid)
{
    //   long long tsc=rdtsc();
    long long tsc;
    long long tsc2;
    long long usec;
    struct timeval tp;
    if (gtid >= app_max_threads)
        return;
    if (async_write > 0) {
        async_shutdown = 1;
        // wait for async_thread
        pthread_mutex_lock(&async_write_mutex);
    }
    tsc = rdtsc();
    gettimeofday(&tp, NULL);
    tsc2 = rdtsc();
    usec = tp.tv_sec;
    usec *= 1000000;
    usec += tp.tv_usec;
    if (use_binary_files && fps_b[gtid]) {
        fclose(fps_b[gtid]);
        fps_b[gtid] = NULL;
    }
    if (fps_t[gtid]) {
        fprintf(fps_t[gtid], "OSE:TSCC:%d:%d:%lld:%lld:%lld \n", gtid, gtid, tsc, tsc2, usec);
        fclose(fps_t[gtid]);
        fps_t[gtid] = NULL;
    }
    if (async_write > 0)
        pthread_mutex_unlock(&async_write_mutex);
}

void __osl_start_microtiming(void *return_addr)
{
    if (condemand)
        __osl_record_event(OSL_START_ONDEMAND, 0, 0, 0, return_addr, -1, 1); // possible RC here
    condemand = condemand | (condemand >> 1);
}
void __osl_stop_microtiming(void *return_addr)
{
    condemand = condemand & 2;
    if (condemand)
        __osl_record_event(OSL_STOP_ONDEMAND, 0, 0, 0, return_addr, -1, 1); // possible RC here
}
