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

/*
            long long &max_ext_count = max_extras[tid];
            int &ext_index = extras_index[tid];
            msl_base_chain *cur_exts=current_extras_chain[tid];
            msl_extra_info *cur_exts_array=current_extras_array[tid];
*/

/* extras recording: one stream per (thread, level); a block that fills up is handed to dump_extras() in
   realtime-dump mode. All five functions share this body. */
#define __T_RECORD_BODY__(STORE)                                                                      \
    assert(tid >= 0);                                                                                 \
    if (counting_permit && num_allocated <= calls_limit) {                                            \
        if (!extras_chain[tid][level])                                                                \
            init_level_storage(tid, level);                                                           \
        msl_extra_info *cur = extras_array[tid][level];                                               \
        int &idx = extras_index[tid][level];                                                          \
        STORE                                                                                         \
        idx++;                                                                                        \
        if (idx >= onetimealloc) {                                                                    \
            msl_base_chain *temp = extras_chain[tid][level];                                          \
            extras_chain[tid][level] = new msl_base_chain(extras_chain[tid][level], sizeof(msl_extra_info)); \
            temp->num_events = idx;                                                                   \
            extras_array[tid][level] = (msl_extra_info *)extras_chain[tid][level]->events_array;      \
            idx = 0;                                                                                  \
            if (use_realtime_dump)                                                                    \
                dump_extras(tid, level, &extras_start_chain[tid][level]);                             \
        }                                                                                             \
    }

int t_base_event::record_ints(const int tid, const int level, const int *values, const int num)
{
    for (int i = 0; i < num; i++) {
        __T_RECORD_BODY__(cur[idx].values[0] = values[i]; cur[idx].values[1] = -1;)
    }
    return 0;
}

int t_base_event::record_int(const int tid, const int level, const int value)
{
    __T_RECORD_BODY__(cur[idx].values[0] = value; cur[idx].values[1] = -1;)
    return 0;
}

int t_base_event::record_longs(const int tid, const int level, const long long *values, const int num)
{
    for (int i = 0; i < num; i++) {
        __T_RECORD_BODY__(cur[idx].value = values[i];)
    }
    return 0;
}

int t_base_event::record_low_longs(const int tid, const int level, const long long *values, const int num)
{
    for (int i = 0; i < num; i++) {
        __T_RECORD_BODY__(cur[idx].values[0] = (int)values[i]; cur[idx].values[1] = -1;)
    }
    return 0;
}

int t_base_event::record_long(const int tid, const int level, const long long value)
{
    __T_RECORD_BODY__(cur[idx].value = value;)
    return 0;
}
#undef __T_RECORD_BODY__
