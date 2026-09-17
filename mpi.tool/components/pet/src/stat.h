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

#ifndef __STAT_H__
#define __STAT_H__

#include <climits>
#include <cstring>
#include <iostream>
#include <map>

#include "common_types.h"
#include "func_id.h"
#include "utils.h"

// MPI_Pcontrol mode
struct MPI_PControl_Stat {
    MPI_PControl_Stat() : region_start(LLONG_MAX), region_end(0), region_length(0), region_count(0), pc_time(0) {}
    PEAT_Clock region_start;  // total start of the Pcontrol ID
    PEAT_Clock region_end;    // total end of the Pcontrol ID
    PEAT_Clock region_length; // comulative length of an events(user code included) in active Pcontrol region
    PEAT_Clock region_count;  // count of an active Pcontrol region
    PEAT_Clock pc_time;       // contains start of the particular measurement event set
    PEAT_Num pack_size() const
    { // Not object size. Just size for data serialization.
        return sizeof(region_start) + sizeof(region_end) + sizeof(region_length) + sizeof(region_count) + sizeof(pc_time);
    }
    PEAT_Num pack(char *ptr) const
    {
        PEAT_Num ssize = 0, ptr_it = 0;
        ssize = sizeof(region_start);
        memcpy(ptr + ptr_it, &region_start, ssize);
        ptr_it += ssize;
        ssize = sizeof(region_end);
        memcpy(ptr + ptr_it, &region_end, ssize);
        ptr_it += ssize;
        ssize = sizeof(region_length);
        memcpy(ptr + ptr_it, &region_length, ssize);
        ptr_it += ssize;
        ssize = sizeof(region_count);
        memcpy(ptr + ptr_it, &region_count, ssize);
        ptr_it += ssize;
        ssize = sizeof(pc_time);
        memcpy(ptr + ptr_it, &pc_time, ssize);
        ptr_it += ssize;
        return ptr_it;
    }
    PEAT_Num unpack(const char *ptr)
    {
        PEAT_Num ssize = 0, ptr_it = 0;
        ssize = sizeof(region_start);
        memcpy(&region_start, ptr + ptr_it, ssize);
        ptr_it += ssize;
        ssize = sizeof(region_end);
        memcpy(&region_end, ptr + ptr_it, ssize);
        ptr_it += ssize;
        ssize = sizeof(region_length);
        memcpy(&region_length, ptr + ptr_it, ssize);
        ptr_it += ssize;
        ssize = sizeof(region_count);
        memcpy(&region_count, ptr + ptr_it, ssize);
        ptr_it += ssize;
        ssize = sizeof(pc_time);
        memcpy(&pc_time, ptr + ptr_it, ssize);
        ptr_it += ssize;
        return ptr_it;
    }
};

struct Stat {
    Stat() : rank_start(LLONG_MAX), rank_end(0), rank_mpi_init_start(-1), rank_mpi_init_end(-1), rank_mpi_finalize_start(-1), rank_mpi_finalize_end(-1),
             num_events(0), cpu_len(0), fabric_len(0), changed_UR_num(0), covered_UR_length(0), uncovered_UR_length(0), pcontrol_removed_user_code_length(0) {}
    PEAT_Clock rank_start;
    PEAT_Clock rank_end;
    PEAT_Clock rank_mpi_init_start;
    PEAT_Clock rank_mpi_init_end;
    PEAT_Clock rank_mpi_finalize_start;
    PEAT_Clock rank_mpi_finalize_end;
    PEAT_Clock num_events;
    PEAT_Clock cpu_len;
    PEAT_Clock fabric_len;

    PEAT_Clock changed_UR_num;      // number of user regions changed by ratio from file
    PEAT_Clock covered_UR_length;   // comulative length of all user regions changed by ratio from file
    PEAT_Clock uncovered_UR_length; // comulative length of all user regions UNchanged by ratio from file

    std::map<PEAT_MPI_Func_ID, std::map<PEAT_Message_Size, PEAT_Clock>> min_msg_length;
    std::map<PEAT_MPI_Func_ID, std::map<PEAT_Message_Size, PEAT_Clock>> max_msg_length;
    std::map<PEAT_MPI_Func_ID, PEAT_Clock> min_event_length;
    std::map<PEAT_MPI_Func_ID, PEAT_Clock> max_event_length;
    PEAT_Clock get_max_event_length() const;
    PEAT_Clock get_min_event_length() const;
    PEAT_Clock get_max_msg_length() const;
    PEAT_Clock get_min_msg_length() const;
    void add_mpi_msg_len_stat(const PEAT_MPI_Func_ID func_id, const PEAT_Message_Size msg_send_size, const PEAT_Clock msg_length);
    void add_mpi_stat(const PEAT_MPI_Func_ID func, const PEAT_Clock in, const PEAT_Clock out, const PEAT_Clock len_in, const PEAT_Clock cpu_len_in);
    void add_uc_stat(const PEAT_Clock len_in, PEAT_Bool changed);

    // MPI_Pcontrol mode start
    void add_mpi_pcontrol_stat(const PEAT_MPI_Func_ID func_id, const PEAT_MPI_Tag pc_tag, const PEAT_Clock in, const PEAT_Clock out);
    void add_pcontrol_user_removed_len(const PEAT_MPI_Func_ID func_id, const PEAT_MPI_Tag pc_tag, const PEAT_Clock region_length);
    std::map<PEAT_MPI_Tag, MPI_PControl_Stat> pcontrol_regions;
    PEAT_Clock pcontrol_removed_user_code_length;
    // MPI_Pcontrol mode end

    PEAT_Num pack_size() const
    {                                                   // Not object size. Just size for data serialization.
        PEAT_Num min_msg_length_size = sizeof(PEAT_Num); // put size of elems_size_mpi variable
        for (std::map<PEAT_MPI_Func_ID, std::map<PEAT_Message_Size, PEAT_Clock>>::const_iterator it = min_msg_length.begin(); it != min_msg_length.end(); ++it) {
            min_msg_length_size += sizeof(it->first);
            min_msg_length_size += pack_pod_size(it->second);
        }
        PEAT_Num max_msg_length_size = sizeof(PEAT_Num); // put size of elems_size_mpi variable
        for (std::map<PEAT_MPI_Func_ID, std::map<PEAT_Message_Size, PEAT_Clock>>::const_iterator it = max_msg_length.begin(); it != max_msg_length.end(); ++it) {
            max_msg_length_size += sizeof(it->first);
            max_msg_length_size += pack_pod_size(it->second);
        }

        PEAT_Num pcontrol_regions_size = sizeof(PEAT_Num); // put size of elements_size variable
        for (std::map<PEAT_MPI_Tag, MPI_PControl_Stat>::const_iterator it = pcontrol_regions.begin(); it != pcontrol_regions.end(); ++it) {
            pcontrol_regions_size += sizeof(it->first);
            pcontrol_regions_size += it->second.pack_size();
        }

        return sizeof(rank_start) +
               sizeof(rank_end) +
               sizeof(rank_mpi_init_start) +
               sizeof(rank_mpi_init_end) +
               sizeof(rank_mpi_finalize_start) +
               sizeof(rank_mpi_finalize_end) +
               sizeof(num_events) +
               sizeof(cpu_len) +
               sizeof(fabric_len) +
               sizeof(changed_UR_num) +
               sizeof(covered_UR_length) +
               sizeof(uncovered_UR_length) +
               min_msg_length_size +
               max_msg_length_size +
               pack_pod_size(min_event_length) +
               pack_pod_size(max_event_length) +
               pcontrol_regions_size +
               sizeof(pcontrol_removed_user_code_length);
    }

    PEAT_Num pack(char *ptr) const
    {
        PEAT_Num ssize = 0, ptr_it = 0;
        // min_msg_length
        const PEAT_Num min_msg_length_size = min_msg_length.size(), min_key_size = sizeof(PEAT_MPI_Func_ID);
        ssize = sizeof(min_msg_length_size);
        memcpy(ptr + ptr_it, &min_msg_length_size, ssize);
        ptr_it += ssize; // put the number of elements of std::map
        for (std::map<PEAT_MPI_Func_ID, std::map<PEAT_Message_Size, PEAT_Clock>>::const_iterator it = min_msg_length.begin(); it != min_msg_length.end(); ++it) {
            memcpy(ptr + ptr_it, &(it->first), min_key_size);
            ptr_it += min_key_size;
            ptr_it += pack_pod(ptr + ptr_it, it->second);
        }
        // max_msg_length
        const PEAT_Num max_msg_length_size = max_msg_length.size(), max_key_size = sizeof(PEAT_MPI_Func_ID);
        ssize = sizeof(max_msg_length_size);
        memcpy(ptr + ptr_it, &max_msg_length_size, ssize);
        ptr_it += ssize; // put the number of elements of std::map
        for (std::map<PEAT_MPI_Func_ID, std::map<PEAT_Message_Size, PEAT_Clock>>::const_iterator it = max_msg_length.begin(); it != max_msg_length.end(); ++it) {
            memcpy(ptr + ptr_it, &(it->first), max_key_size);
            ptr_it += max_key_size;
            ptr_it += pack_pod(ptr + ptr_it, it->second);
        }
        // pcontrol_regions
        const PEAT_Num pcontrol_size = pcontrol_regions.size(), pcontrol_key_size = sizeof(PEAT_MPI_Tag);
        ssize = sizeof(pcontrol_size);
        memcpy(ptr + ptr_it, &pcontrol_size, ssize);
        ptr_it += ssize; // put the number of elements of std::map
        for (std::map<PEAT_MPI_Tag, MPI_PControl_Stat>::const_iterator it = pcontrol_regions.begin(); it != pcontrol_regions.end(); ++it) {
            memcpy(ptr + ptr_it, &(it->first), pcontrol_key_size);
            ptr_it += pcontrol_key_size;
            ptr_it += it->second.pack(ptr + ptr_it);
        }
        ssize = sizeof(rank_start);
        memcpy(ptr + ptr_it, &rank_start, ssize);
        ptr_it += ssize;
        ssize = sizeof(rank_end);
        memcpy(ptr + ptr_it, &rank_end, ssize);
        ptr_it += ssize;
        ssize = sizeof(rank_mpi_init_start);
        memcpy(ptr + ptr_it, &rank_mpi_init_start, ssize);
        ptr_it += ssize;
        ssize = sizeof(rank_mpi_init_end);
        memcpy(ptr + ptr_it, &rank_mpi_init_end, ssize);
        ptr_it += ssize;
        ssize = sizeof(rank_mpi_finalize_start);
        memcpy(ptr + ptr_it, &rank_mpi_finalize_start, ssize);
        ptr_it += ssize;
        ssize = sizeof(rank_mpi_finalize_end);
        memcpy(ptr + ptr_it, &rank_mpi_finalize_end, ssize);
        ptr_it += ssize;
        ssize = sizeof(num_events);
        memcpy(ptr + ptr_it, &num_events, ssize);
        ptr_it += ssize;
        ssize = sizeof(cpu_len);
        memcpy(ptr + ptr_it, &cpu_len, ssize);
        ptr_it += ssize;
        ssize = sizeof(fabric_len);
        memcpy(ptr + ptr_it, &fabric_len, ssize);
        ptr_it += ssize;
        ssize = sizeof(changed_UR_num);
        memcpy(ptr + ptr_it, &changed_UR_num, ssize);
        ptr_it += ssize;
        ssize = sizeof(covered_UR_length);
        memcpy(ptr + ptr_it, &covered_UR_length, ssize);
        ptr_it += ssize;
        ssize = sizeof(uncovered_UR_length);
        memcpy(ptr + ptr_it, &uncovered_UR_length, ssize);
        ptr_it += ssize;
        ptr_it += pack_pod(ptr + ptr_it, min_event_length);
        ptr_it += pack_pod(ptr + ptr_it, max_event_length);
        ssize = sizeof(pcontrol_removed_user_code_length);
        memcpy(ptr + ptr_it, &pcontrol_removed_user_code_length, ssize);
        ptr_it += ssize;

        return ptr_it;
    }
    PEAT_Num unpack(const char *ptr)
    {
        PEAT_Num ssize = 0, ptr_it = 0;
        // min_msg_length
        PEAT_Num min_msg_length_size = 0;
        ssize = sizeof(min_msg_length_size);
        memcpy(&min_msg_length_size, ptr + ptr_it, ssize);
        ptr_it += ssize; // get the number of elements of std::map
        while (min_msg_length_size) {
            PEAT_MPI_Func_ID key;
            std::map<PEAT_Message_Size, PEAT_Clock> value;
            std::pair<std::map<PEAT_MPI_Func_ID, std::map<PEAT_Message_Size, PEAT_Clock>>::iterator, PEAT_Bool> ret;
            ssize = sizeof(key);
            memcpy(&key, ptr + ptr_it, ssize);
            ptr_it += ssize;
            ptr_it += unpack_pod(ptr + ptr_it, value);
            ret = min_msg_length.insert(std::pair<PEAT_MPI_Func_ID, std::map<PEAT_Message_Size, PEAT_Clock>>(key, value));
            if (ret.second == false) {
                std::cerr << "Failed to unpack data in Stat::unpack. min_msg_length Key " << key << " Value " << value << "\n";
            }
            --min_msg_length_size;
        }
        // max_msg_length
        PEAT_Num max_msg_length_size = 0;
        ssize = sizeof(max_msg_length_size);
        memcpy(&max_msg_length_size, ptr + ptr_it, ssize);
        ptr_it += ssize; // get the number of elements of std::map
        while (max_msg_length_size) {
            PEAT_MPI_Func_ID key;
            std::map<PEAT_Message_Size, PEAT_Clock> value;
            std::pair<std::map<PEAT_MPI_Func_ID, std::map<PEAT_Message_Size, PEAT_Clock>>::iterator, PEAT_Bool> ret;
            ssize = sizeof(key);
            memcpy(&key, ptr + ptr_it, ssize);
            ptr_it += ssize;
            ptr_it += unpack_pod(ptr + ptr_it, value);
            ret = max_msg_length.insert(std::pair<PEAT_MPI_Func_ID, std::map<PEAT_Message_Size, PEAT_Clock>>(key, value));
            if (ret.second == false) {
                std::cerr << "Failed to unpack data in Stat::unpack. max_msg_length Key " << key << " Value " << value << "\n";
            }
            --max_msg_length_size;
        }
        // pcontrol_regions
        PEAT_Num pcontrol_size = 0;
        ssize = sizeof(pcontrol_size);
        memcpy(&pcontrol_size, ptr + ptr_it, ssize);
        ptr_it += ssize; // get the number of elements of std::map
        while (pcontrol_size) {
            PEAT_MPI_Tag key;
            MPI_PControl_Stat value;
            std::pair<std::map<PEAT_MPI_Tag, MPI_PControl_Stat>::iterator, PEAT_Bool> ret;
            ssize = sizeof(key);
            memcpy(&key, ptr + ptr_it, ssize);
            ptr_it += ssize;
            ptr_it += value.unpack(ptr + ptr_it);
            ret = pcontrol_regions.insert(std::pair<PEAT_MPI_Tag, MPI_PControl_Stat>(key, value));
            if (ret.second == false) {
                std::cerr << "Failed to unpack data in Stat::unpack. pcontrol_regions Key " << key << " Value " << value.region_count << "\n";
            }
            --pcontrol_size;
        }
        ssize = sizeof(rank_start);
        memcpy(&rank_start, ptr + ptr_it, ssize);
        ptr_it += ssize;
        ssize = sizeof(rank_end);
        memcpy(&rank_end, ptr + ptr_it, ssize);
        ptr_it += ssize;
        ssize = sizeof(rank_mpi_init_start);
        memcpy(&rank_mpi_init_start, ptr + ptr_it, ssize);
        ptr_it += ssize;
        ssize = sizeof(rank_mpi_init_end);
        memcpy(&rank_mpi_init_end, ptr + ptr_it, ssize);
        ptr_it += ssize;
        ssize = sizeof(rank_mpi_finalize_start);
        memcpy(&rank_mpi_finalize_start, ptr + ptr_it, ssize);
        ptr_it += ssize;
        ssize = sizeof(rank_mpi_finalize_end);
        memcpy(&rank_mpi_finalize_end, ptr + ptr_it, ssize);
        ptr_it += ssize;
        ssize = sizeof(num_events);
        memcpy(&num_events, ptr + ptr_it, ssize);
        ptr_it += ssize;
        ssize = sizeof(cpu_len);
        memcpy(&cpu_len, ptr + ptr_it, ssize);
        ptr_it += ssize;
        ssize = sizeof(fabric_len);
        memcpy(&fabric_len, ptr + ptr_it, ssize);
        ptr_it += ssize;
        ssize = sizeof(changed_UR_num);
        memcpy(&changed_UR_num, ptr + ptr_it, ssize);
        ptr_it += ssize;
        ssize = sizeof(covered_UR_length);
        memcpy(&covered_UR_length, ptr + ptr_it, ssize);
        ptr_it += ssize;
        ssize = sizeof(uncovered_UR_length);
        memcpy(&uncovered_UR_length, ptr + ptr_it, ssize);
        ptr_it += ssize;
        ptr_it += unpack_pod(ptr + ptr_it, min_event_length);
        ptr_it += unpack_pod(ptr + ptr_it, max_event_length);
        ssize = sizeof(pcontrol_removed_user_code_length);
        memcpy(&pcontrol_removed_user_code_length, ptr + ptr_it, ssize);
        ptr_it += ssize;
        return ptr_it;
    }
};
struct MPI_Event_Stat {
    MPI_Event_Stat() : num_events(0), prolog(0), centre(0), epilog(0), fabric(0), wait(0), overlap(0),
                       cpu_min(LLONG_MAX), cpu_max(0), fabric_min(LLONG_MAX), fabric_max(0),
                       total_send_length(0), total_recv_length(0), is_rndv(false), is_acked(false) {}
    PEAT_Clock num_events;
    PEAT_Clock prolog, centre, epilog;
    PEAT_Clock fabric;
    PEAT_Clock wait;
    PEAT_Clock overlap;
    PEAT_Clock cpu_min;
    PEAT_Clock cpu_max;
    PEAT_Clock fabric_min;
    PEAT_Clock fabric_max;
    PEAT_Message_Size total_send_length;
    PEAT_Message_Size total_recv_length;
    PEAT_Bool is_rndv, is_acked;
    PEAT_Num pack_size() const
    {
        return sizeof(num_events) +
               sizeof(prolog) +
               sizeof(fabric) +
               sizeof(centre) +
               sizeof(epilog) +
               sizeof(wait) +
               sizeof(overlap) +
               sizeof(cpu_min) +
               sizeof(cpu_max) +
               sizeof(fabric_min) +
               sizeof(fabric_max) +
               sizeof(total_send_length) +
               sizeof(total_recv_length) +
               sizeof(is_rndv) +
               sizeof(is_acked);
    }
    PEAT_Num pack(char *ptr) const
    {
        PEAT_Num ssize = 0, ptr_it = 0;
        ssize = sizeof(num_events);
        memcpy(ptr + ptr_it, &num_events, ssize);
        ptr_it += ssize;
        ssize = sizeof(prolog);
        memcpy(ptr + ptr_it, &prolog, ssize);
        ptr_it += ssize;
        ssize = sizeof(fabric);
        memcpy(ptr + ptr_it, &fabric, ssize);
        ptr_it += ssize;
        ssize = sizeof(centre);
        memcpy(ptr + ptr_it, &centre, ssize);
        ptr_it += ssize;
        ssize = sizeof(epilog);
        memcpy(ptr + ptr_it, &epilog, ssize);
        ptr_it += ssize;
        ssize = sizeof(wait);
        memcpy(ptr + ptr_it, &wait, ssize);
        ptr_it += ssize;
        ssize = sizeof(overlap);
        memcpy(ptr + ptr_it, &overlap, ssize);
        ptr_it += ssize;
        ssize = sizeof(cpu_min);
        memcpy(ptr + ptr_it, &cpu_min, ssize);
        ptr_it += ssize;
        ssize = sizeof(cpu_max);
        memcpy(ptr + ptr_it, &cpu_max, ssize);
        ptr_it += ssize;
        ssize = sizeof(fabric_min);
        memcpy(ptr + ptr_it, &fabric_min, ssize);
        ptr_it += ssize;
        ssize = sizeof(fabric_max);
        memcpy(ptr + ptr_it, &fabric_max, ssize);
        ptr_it += ssize;
        ssize = sizeof(total_send_length);
        memcpy(ptr + ptr_it, &total_send_length, ssize);
        ptr_it += ssize;
        ssize = sizeof(total_recv_length);
        memcpy(ptr + ptr_it, &total_recv_length, ssize);
        ptr_it += ssize;
        ssize = sizeof(is_rndv);
        memcpy(ptr + ptr_it, &is_rndv, ssize);
        ptr_it += ssize;
        ssize = sizeof(is_acked);
        memcpy(ptr + ptr_it, &is_acked, ssize);
        ptr_it += ssize;
        return ptr_it;
    }
    PEAT_Num unpack(const char *ptr)
    {
        PEAT_Num ssize = 0, ptr_it = 0;
        ssize = sizeof(num_events);
        memcpy(&num_events, ptr + ptr_it, ssize);
        ptr_it += ssize;
        ssize = sizeof(prolog);
        memcpy(&prolog, ptr + ptr_it, ssize);
        ptr_it += ssize;
        ssize = sizeof(fabric);
        memcpy(&fabric, ptr + ptr_it, ssize);
        ptr_it += ssize;
        ssize = sizeof(centre);
        memcpy(&centre, ptr + ptr_it, ssize);
        ptr_it += ssize;
        ssize = sizeof(epilog);
        memcpy(&epilog, ptr + ptr_it, ssize);
        ptr_it += ssize;
        ssize = sizeof(wait);
        memcpy(&wait, ptr + ptr_it, ssize);
        ptr_it += ssize;
        ssize = sizeof(overlap);
        memcpy(&overlap, ptr + ptr_it, ssize);
        ptr_it += ssize;
        ssize = sizeof(cpu_min);
        memcpy(&cpu_min, ptr + ptr_it, ssize);
        ptr_it += ssize;
        ssize = sizeof(cpu_max);
        memcpy(&cpu_max, ptr + ptr_it, ssize);
        ptr_it += ssize;
        ssize = sizeof(fabric_min);
        memcpy(&fabric_min, ptr + ptr_it, ssize);
        ptr_it += ssize;
        ssize = sizeof(fabric_max);
        memcpy(&fabric_max, ptr + ptr_it, ssize);
        ptr_it += ssize;
        ssize = sizeof(total_send_length);
        memcpy(&total_send_length, ptr + ptr_it, ssize);
        ptr_it += ssize;
        ssize = sizeof(total_recv_length);
        memcpy(&total_recv_length, ptr + ptr_it, ssize);
        ptr_it += ssize;
        ssize = sizeof(is_rndv);
        memcpy(&is_rndv, ptr + ptr_it, ssize);
        ptr_it += ssize;
        ssize = sizeof(is_acked);
        memcpy(&is_acked, ptr + ptr_it, ssize);
        ptr_it += ssize;
        return ptr_it;
    }
    PEAT_Clock cpu() const
    {
        return prolog + epilog;
    }
};
struct PET_Event;
struct Event_Set_Stat {
    enum t_stat_type {
        tst_orig,
        tst_trc,
        tst_post,
        tst_size
    };
    std::map<PEAT_Rank, Stat> &get_stat(const t_stat_type t)
    {
        return stat[t];
    }
    std::map<PEAT_Func_ID_Pair, std::map<PEAT_Rank, std::map<PEAT_Message_Size, std::map<PEAT_Message_Size, std::list<MPI_Event_Stat>>>>> &get_ts_fs_stat(const t_stat_type t, const PEAT_Transports ct)
    {
        return ts_fs_stat[t][ct];
    }
    std::map<PEAT_Rank, std::map<PEAT_Func_ID_Pair, std::map<PEAT_Message_Size, MPI_Event_Stat>>> &get_func_stat(const t_stat_type t)
    {
        return stat_per_func[t];
    }
    void add_event_stat(const t_stat_type t, const PEAT_Transports ct, const PEAT_Rank rank, const PEAT_MPI_Func_ID func, const PEAT_MPI_Func_ID parent_func, const PEAT_Rank num_ranks, const PEAT_Message_Size send_size, const PEAT_Message_Size recv_size,
                        const PEAT_Clock prolog, const PEAT_Clock fabric, const PEAT_Clock centre, const PEAT_Clock epilog, const PEAT_Clock wait, const PEAT_Clock overlap, PEAT_Bool is_rndv, PEAT_Bool is_not_verified);
    void add_ts_fs_event_stat(const t_stat_type t, PET_Event *event);
    void add_mpi_event_stat(const t_stat_type t, const PEAT_Rank rank, const PEAT_MPI_Func_ID func, const PEAT_Clock in, const PEAT_Clock out, const PEAT_Clock len_in, const PEAT_Clock cpu_len_in);
    void add_uc_event_stat(const t_stat_type t, const PEAT_Rank rank, const PEAT_Clock len_in, const PEAT_Bool changed);
    void add_mpi_length_stat(const t_stat_type t, const PEAT_Rank rank, const PEAT_MPI_Func_ID func, const PEAT_Message_Size msg_size, const PEAT_Clock msg_len);

  private:
    std::map<PEAT_Rank, Stat> stat[tst_size];
    std::map<PEAT_Rank, std::map<PEAT_Func_ID_Pair, std::map<PEAT_Message_Size, MPI_Event_Stat>>> stat_per_func[tst_size];
    std::map<PEAT_Func_ID_Pair, std::map<PEAT_Rank, std::map<PEAT_Message_Size, std::map<PEAT_Message_Size, std::list<MPI_Event_Stat>>>>> ts_fs_stat[tst_size][PEAT_TRANSPORT_MAX];
};

#endif // __STAT_H__
