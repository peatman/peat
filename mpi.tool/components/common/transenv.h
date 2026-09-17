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

#ifndef __TRANSENV_H__
#define __TRANSENV_H__
#include "common_plain_types.h"
#include "limits.h"

struct PEAT_Trasport_Environment {
    PEAT_FREQ frequency = -1; //(in Hz)
                              // all times below are in ticks
    PEAT_BW general_bw = 0.0;
    PEAT_LAT general_lat = 0;
    PEAT_Float cross_packet_delay = -1;
    PEAT_LAT pre_delay = 0;
    PEAT_LAT post_delay = 0;
    PEAT_Transports t_id = PEAT_TRANSPORT_MAX;
    PEAT_MPI_Func_ID func_id = TMPI_FUNC_ID_NONE;
    PEAT_Bool initialized = false;
    PEAT_Signed_Num is_parallel = 0;
    PEAT_Signed_Num unit_size = 0;
    PEAT_Signed_Num packet_size = 0;
    PEAT_Signed_Num chain_num = 0;
    PEAT_Clock get_cpu_times() const
    {
        return (PEAT_Clock)(pre_delay + post_delay);
    };
};
struct PEAT_Ext_Trasport_Environment : public PEAT_Trasport_Environment {
    PEAT_MPI_Message message_size;
    PEAT_Ratio mult_lat = 0.0;
    PEAT_LAT min_pre_delay = 0;
    PEAT_LAT min_post_delay = 0;
    PEAT_LAT max_pre_delay = 0;
    PEAT_LAT max_post_delay = 0;
    PEAT_LAT avg_pre_delay = 0;
    PEAT_LAT avg_post_delay = 0;
    PEAT_LAT median_pre_delay = 0;
    PEAT_LAT median_post_delay = 0;
    PEAT_Ratio left_ratio = 1.00;
    PEAT_Ratio right_ratio = 1.00;
    PEAT_Num left_ratio_num = 0;
    PEAT_Num right_ratio_num = 1;
    PEAT_Ext_Trasport_Environment &operator+=(PEAT_Ext_Trasport_Environment &r)
    {
        general_bw += r.general_bw;
        general_lat += r.general_lat;
        cross_packet_delay += r.cross_packet_delay;
        min_pre_delay += r.min_pre_delay;
        min_post_delay += r.min_post_delay;
        avg_pre_delay += r.avg_pre_delay;
        avg_post_delay += r.avg_post_delay;
        max_pre_delay += r.max_pre_delay;
        max_post_delay += r.max_post_delay;
        median_pre_delay += r.median_pre_delay;
        median_post_delay += r.median_post_delay;
        return *this;
    }
    PEAT_Ext_Trasport_Environment &operator*=(PEAT_Ratio r)
    {
        general_bw = (PEAT_BW)(general_bw * r);
        general_lat = (PEAT_LAT)(general_lat * r);
        cross_packet_delay = (PEAT_Float)(cross_packet_delay * r);
        min_pre_delay = (PEAT_LAT)(min_pre_delay * r);
        min_post_delay = (PEAT_LAT)(min_post_delay * r);
        avg_pre_delay = (PEAT_LAT)(avg_pre_delay * r);
        avg_post_delay = (PEAT_LAT)(avg_post_delay * r);
        max_pre_delay = (PEAT_LAT)(max_pre_delay * r);
        max_post_delay = (PEAT_LAT)(max_post_delay * r);
        median_pre_delay = (PEAT_LAT)(median_pre_delay * r);
        median_post_delay = (PEAT_LAT)(median_post_delay * r);
        return *this;
    }
};
struct Trasport_Corection_Environment {
    std::map<PEAT_Func_ID_Pair, std::vector<PEAT_Ext_Trasport_Environment>> corrections;
};

struct peat_func_env_stat {
    PEAT_Clock min = LLONG_MAX, max = 0, sum = 0, avg = 0, num = 0;
    void fill(PEAT_Clock duration)
    {
        num++;
        sum += duration;
        avg = sum / num;
        if (max < duration)
            max = duration;
        if (min > duration)
            min = duration;
    }
};
struct Function_Environment {
    peat_func_env_stat entry, exit;
};
#endif // __TRANSENV_H__