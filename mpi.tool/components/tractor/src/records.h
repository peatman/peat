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

int __record_ints__(const int tid, const int *values, const int num)
{
    //        __DEBUG_PROLOG_TRACTOR_LAYER__
    int i;
    long long tsc;
    assert(tid >= 0);
    /*      fprintf (stderr,"req:%d %p %d\n",event_type,requests,num);*/
    if (counting_permit && num_allocated <= calls_limit) {
        for (i = 0; i < num; i++) {
#ifdef _TRACTOR_STORE_UNIQ_INDEX_
            __current_extras__[__extras_index__].uniq_index = uniq_index[tid];
#endif
            __current_extras__[__extras_index__].values[0] = values[i];
            __current_extras__[__extras_index__].values[1] = -1;
            __extras_index__++;
            if (__extras_index__ >= onetimealloc) {
                msl_base_chain *temp = __extras_current__;
                __extras_current__ = new msl_base_chain(__extras_current__, sizeof(msl_extra_info));
                temp->num_events = __extras_index__;
                __current_extras__ = (msl_extra_info *)__extras_current__->events_array;
                __extras_index__ = 0;
                if (use_realtime_dump)
                    dump_extras(tid, &__extras_start__, __is_colt__);
            }
        }
        //            __DEBUG_EPILOG_TRACTOR_LAYER__
        return 0;
    }
    //        __DEBUG_EPILOG_TRACTOR_LAYER__
    return 0;
}

int __record_int__(const int tid, const int value)
{
    //        __DEBUG_PROLOG_TRACTOR_LAYER__
    int i;
    long long tsc;
    assert(tid >= 0);
    if (counting_permit && num_allocated <= calls_limit) {
        __current_extras__[__extras_index__].values[0] = value;
        __current_extras__[__extras_index__].values[1] = -1;
#ifdef _TRACTOR_STORE_UNIQ_INDEX_
        __current_extras__[__extras_index__].uniq_index = uniq_index[tid];
#endif

        __extras_index__++;
        if (__extras_index__ >= onetimealloc) {
            msl_base_chain *temp = __extras_current__;
            __extras_current__ = new msl_base_chain(__extras_current__, sizeof(msl_extra_info));
            temp->num_events = __extras_index__;
            __current_extras__ = (msl_extra_info *)__extras_current__->events_array;
            __extras_index__ = 0;
            if (use_realtime_dump)
                dump_extras(tid, &__extras_start__, __is_colt__);
        }
        //            __DEBUG_EPILOG_TRACTOR_LAYER__
        return 0;
    }
    //        __DEBUG_EPILOG_TRACTOR_LAYER__
    return 0;
}

int __record_longs__(const int tid, const long long *values, const int num)
{
    //        __DEBUG_PROLOG_TRACTOR_LAYER__
    int i;
    long long tsc;
    assert(tid >= 0);
    /*      fprintf (stderr,"req:%d %p %d\n",event_type,requests,num);*/
    if (counting_permit && num_allocated <= calls_limit) {
        for (i = 0; i < num; i++) {
#ifdef _TRACTOR_STORE_UNIQ_INDEX_
            __current_extras__[__extras_index__].uniq_index = uniq_index[tid];
#endif
            __current_extras__[__extras_index__].value = values[i];
            __extras_index__++;
            if (__extras_index__ >= onetimealloc) {
                msl_base_chain *temp = __extras_current__;
                __extras_current__ = new msl_base_chain(__extras_current__, sizeof(msl_extra_info));
                temp->num_events = __extras_index__;
                __current_extras__ = (msl_extra_info *)__extras_current__->events_array;
                __extras_index__ = 0;
                if (use_realtime_dump)
                    dump_extras(tid, &__extras_start__, __is_colt__);
            }
        }
        //            __DEBUG_EPILOG_TRACTOR_LAYER__
        return 0;
    }
    //        __DEBUG_EPILOG_TRACTOR_LAYER__
    return 0;
}
int __record_low_longs__(const int tid, const long long *values, const int num)
{
    //        __DEBUG_PROLOG_TRACTOR_LAYER__
    int i;
    long long tsc;
    assert(tid >= 0);
    /*      fprintf (stderr,"req:%d %p %d\n",event_type,requests,num);*/
    if (counting_permit && num_allocated <= calls_limit) {
        for (i = 0; i < num; i++) {
#ifdef _TRACTOR_STORE_UNIQ_INDEX_
            __current_extras__[__extras_index__].uniq_index = uniq_index[tid];
#endif
            __current_extras__[__extras_index__].values[0] = (int)values[i];
            __current_extras__[__extras_index__].values[1] = -1;
            __extras_index__++;
            if (__extras_index__ >= onetimealloc) {
                msl_base_chain *temp = __extras_current__;
                __extras_current__ = new msl_base_chain(__extras_current__, sizeof(msl_extra_info));
                temp->num_events = __extras_index__;
                __current_extras__ = (msl_extra_info *)__extras_current__->events_array;
                __extras_index__ = 0;
                if (use_realtime_dump)
                    dump_extras(tid, &__extras_start__, __is_colt__);
            }
        }
        //            __DEBUG_EPILOG_TRACTOR_LAYER__
        return 0;
    }
    //        __DEBUG_EPILOG_TRACTOR_LAYER__
    return 0;
}

int __record_long__(const int tid, const long long value)
{
    //        __DEBUG_PROLOG_TRACTOR_LAYER__
    int i;
    long long tsc;
    assert(tid >= 0);
    if (counting_permit && num_allocated <= calls_limit) {
        __current_extras__[__extras_index__].value = value;
#ifdef _TRACTOR_STORE_UNIQ_INDEX_
        __current_extras__[__extras_index__].uniq_index = uniq_index[tid];
#endif

        __extras_index__++;
        if (__extras_index__ >= onetimealloc) {
            msl_base_chain *temp = __extras_current__;
            __extras_current__ = new msl_base_chain(__extras_current__, sizeof(msl_extra_info));
            temp->num_events = __extras_index__;
            __current_extras__ = (msl_extra_info *)__extras_current__->events_array;
            __extras_index__ = 0;
            if (use_realtime_dump)
                dump_extras(tid, &__extras_start__, __is_colt__);
        }
        //            __DEBUG_EPILOG_TRACTOR_LAYER__
        return 0;
    }
    //        __DEBUG_EPILOG_TRACTOR_LAYER__
    return 0;
}
