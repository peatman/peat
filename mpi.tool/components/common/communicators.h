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

#ifndef __COMMUNICATORS_H__
#define __COMMUNICATORS_H__

#include "common_types.h"
#include "model_id.h"
#include "utils.h"
#include <map>
#include <string>
#include <vector>

class Communicators
{
    const Model_ID *m_id;
    const Global_Parameters &globals;

  public:
    explicit Communicators(const Global_Parameters &glob);
    void initialize(const std::map<PEAT_Rank, std::string> &commonfile_names, const std::map<PEAT_Rank, std::string> appfile_names, const Model_ID *id); // allow to construct uninitialized object for MPI environment
    PEAT_Rank get_comm_world_rank_count() const
    {
        return commworld_rank_num;
    }
    PEAT_Rank get_app_rank_count() const
    {
        return app_rank_num;
    }
    const PEAT_MPI_Time &get_mpi_rank_time(PEAT_Rank rank) const
    {
        return mpi_time.at(rank);
    }
    struct Communicator_Info {
        Communicator_Info() : comm_id(0), is_internal(false) {}
        PEAT_MPI_Comm comm_id;                                 // Estimator Communicator ID index
        bool is_internal;                                      // created by the MPI library itself (not by the application)
        std::vector<PEAT_Rank> ranks;                          // index is local ranks in communicator. Value is global rank in COMM_WORLD.
        std::map<PEAT_Rank, std::vector<PEAT_Rank>> groups[2]; // rank groups (0 - local, 1 remote) this is a waaaaaay overkill to store ewach rank group (and surely can be optimized)  but helps debuging sometimes
                                                               /*
                                                                       PEAT_Num pack_size() const {//Not object size. Just size for data serialization.
                                                                           return sizeof(ranks) +
                                                                                  pack_pod_size(begin) +
                                                                                  pack_pod_size(end) +
                                                                                  pack_pod_size(mpiCommName) +
                                                                                  pack_pod_size(elems);
                                                                       }
                                                                       PEAT_Num pack(char *ptr) const {
                                                                           PEAT_Num ssize = 0, ptr_it = 0;
                                                                           ssize = sizeof(ranks); memcpy(ptr + ptr_it, &ranks, ssize); ptr_it += ssize;
                                                                           ptr_it += pack_pod(ptr + ptr_it, begin);
                                                                           ptr_it += pack_pod(ptr + ptr_it, end);
                                                                           ptr_it += pack_pod(ptr + ptr_it, mpiCommName);
                                                                           ptr_it += pack_pod(ptr + ptr_it, elems);
                                                                           return ptr_it;
                                                                       }
                                                                       PEAT_Num unpack(const char *ptr) {
                                                                           PEAT_Num ssize = 0, ptr_it = 0;
                                                                           ssize = sizeof(ranks); memcpy(&ranks, ptr + ptr_it, ssize); ptr_it += ssize;
                                                                           ptr_it += unpack_pod(ptr + ptr_it, begin);
                                                                           ptr_it += unpack_pod(ptr + ptr_it, end);
                                                                           ptr_it += unpack_pod(ptr + ptr_it, mpiCommName);
                                                                           ptr_it += unpack_pod(ptr + ptr_it, elems);
                                                                           return ptr_it;
                                                                       }
                                                               */
        // friend std::ostream &operator<<(std::ostream &dbg, const Communicator_Info &mp);
    };
    PEAT_Rank get_rank_from_comm(const PEAT_MPI_Comm comm, const PEAT_Rank destinationRank) const;
    std::map<PEAT_Rank, PEAT_Rank> get_ranks_map_from_comm(const PEAT_MPI_Comm comm) const;
    std::set<PEAT_Rank> get_ranks_set_from_comm(const PEAT_MPI_Comm comm) const;
    std::map<PEAT_MPI_Comm, Communicator_Info> comms;
    PEAT_Num get_comms_num() const { return comms.size(); }
    PEAT_MPI_Comm get_MPI_COMM_NULL() const
    {
        return CMPI_COMM_NULL;
    }
    PEAT_MPI_Comm get_MPI_COMM_WORLD() const
    {
        return CMPI_COMM_WORLD;
    }
    PEAT_MPI_Const get_MPI_UNDEFINED() const
    {
        return CMPI_UNDEFINED;
    }
    PEAT_MPI_Tag get_MPI_ANY_TAG() const
    {
        return CMPI_ANY_TAG;
    }
    PEAT_Rank get_MPI_ANY_SOURCE() const
    {
        return CMPI_ANY_SOURCE;
    }
    PEAT_MPI_Const get_MPI_REQUEST_NULL() const
    {
        return CMPI_REQUEST_NULL;
    }
    PEAT_MPI_Const get_MPI_PROC_NULL() const
    {
        return CMPI_PROC_NULL;
    }

    // Frame information
    std::map<PEAT_Rank, PEAT_Clock> frame_tsc_start;
    PEAT_Rank frame_tsc_field_size;
    PEAT_Rank frame_hw_field_size;

    struct Time_Stamps {
        Time_Stamps() : after_init_time(0), after_finalize_time(0), after_init_tsc(0), after_finalize_tsc(0), after_init_time_shift(0), rank_freq(-1), first_tsc(0) {}
        PEAT_Clock after_init_time;      // rank real time after MPI_Init time stamps in real nanoseconds
        PEAT_Clock after_finalize_time;  // rank real time after MPI_Finalize time stamps in real nanoseconds
        PEAT_Clock after_init_tsc;       // rank real time after MPI_Init time stamps in TSC
        PEAT_Clock after_finalize_tsc;   // rank real time after MPI_Finalize time stamps in TSC
        PEAT_Clock after_init_time_shift; // shift for real time trace for ranks
        PEAT_Clock rank_freq;
        PEAT_Clock first_tsc;
    };

    std::map<PEAT_Rank, Time_Stamps> time_stamps;
    std::map<PEAT_Rank, PEAT_MPI_Time> mpi_time;
    PEAT_Clock get_real_time(const PEAT_Clock raw_tsc, const PEAT_Rank rank);

  private:
    void parse(const String_Split &lines, const int rank, PEAT_Bool is_app);
    void process_def_comm(const String_Split &line);
    void process_local_communicators(const PEAT_Rank rank, const String_Split &line);
    void process_global_communicators(const String_Split &line);
    void process_frame_info(const String_Split &line, PEAT_Rank rank);

    PEAT_MPI_Comm CMPI_COMM_WORLD, CMPI_COMM_SELF, CMPI_COMM_NULL;
    PEAT_MPI_Const CMPI_SUCCESS, CMPI_UNDEFINED, CMPI_PROC_NULL, CMPI_REQUEST_NULL;
    PEAT_MPI_Tag CMPI_ANY_TAG;
    PEAT_Rank CMPI_ANY_SOURCE;
    PEAT_Rank app_rank_num, commworld_rank_num;
    String_Split lines;
};
#endif // __COMMUNICATORS_H__
