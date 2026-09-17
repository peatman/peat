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

#include <kmp_stat.h>
#include <stdio.h>
#include <stdlib.h>

#ifdef __DEBUG_OUTPUT__
static char *event_description[] = {
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
    "Fork_Call",
    "Start_On_Demand", "Stop_On_Demand", "Counter_Description"};
#else
static char *event_description[] = {
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
    "FC",
    "STOD", "SPOD", "CD"};
#endif

FILE *fps_t;
FILE *fps_b;

int main(int argc, char **argv)
{
    microtask_event temp_event;
    if (argc < 3) {
        fprintf(stderr, "Lack of arguments\n");
        return (-2);
    }
    fps_b = fopen(argv[1], "r+b");
    fps_t = fopen(argv[2], "a+t");

    if (fps_b == NULL || fps_t == NULL) {
        fprintf(stderr, "File open error\n");
        return (-2);
    }
    fprintf(stderr, "Processing files %s->%s...", argv[1], argv[2]);
    while (!feof(fps_b)) {
        temp_event.event_type = 0;
        fread(&temp_event, sizeof(temp_event), 1, fps_b);
        if (temp_event.event_type != 0)
#ifdef __DEBUG_OUTPUT__
            fprintf(fps_t, "OMPStatEvent: Thread %d %d %d : %s at %lld addr:%p\n", temp_event.tid, temp_event.gtid, temp_event.gtmid,
                    event_description[temp_event.event_type],
                    temp_event.tsc,
                    temp_event.p_entry);
#else
            fprintf(fps_t, "OSE:%s:%lld:%d:%d:%d:%lld:%p", event_description[temp_event.event_type], temp_event.uniq_id,
                    temp_event.tid, temp_event.gtid, temp_event.gtmid,
                    temp_event.tsc,
                    temp_event.p_entry);
#endif
        fflush(fps_t);
    }
    fprintf(stderr, "Done.\n");
    fclose(fps_b);
    fclose(fps_t);
}
