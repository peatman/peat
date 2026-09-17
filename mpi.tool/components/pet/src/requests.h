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

#ifndef __REQUESTS_H__
#define __REQUESTS_H__

#include "common_plain_types.h"
#include "func_id.h"

struct Request {
    Request() : request(-1), rank(-1), peer_rank(-1), tag(0), acknowledgement_sent(false),
                comm(0), size(0), start_time(-1), start_time_orig(-1), uniq_id(-1), func_id(TMPI_FUNC_ID_NONE), kind(T_PROPERTY_NONE), completed(false), canceled(false), persistent(false) {}

    PEAT_MPI_Request request; // ID of the request
    PEAT_Rank rank;           // TODO. delete as ambiguous
    PEAT_Rank peer_rank;      // used to keep information from mpi::status - medians peer_rank Rank
    PEAT_MPI_Tag tag;         // used to keep information from mpi::status
    PEAT_Bool acknowledgement_sent;
    PEAT_MPI_Comm comm;
    PEAT_Message_Size size;
    mutable PEAT_Clock start_time;
    PEAT_Clock start_time_orig;
    PEAT_UID uniq_id;
    PEAT_MPI_Func_ID func_id;
    T_property kind;             // DATA_TX, DATA_RX, etc.
    mutable PEAT_Bool completed; // message belongs to the request received
    PEAT_Bool canceled;          // MPI_CANCEL
    PEAT_Bool persistent;        // MPI_Send_init, MPI_Recv_init

    // Have to provide uniq identification the Request across the Estimator
    // Can't use time as the key because it can be changed
    // No Request with same "request field" available at the same time (useing uniq_id makes no sence)
    PEAT_Bool operator<(const Request &rhs) const
    {
        if (request < rhs.request) {
            return true;
        }
        if (request > rhs.request) {
            return false;
        }
        if (uniq_id < rhs.uniq_id) {
            return true;
        } // useing uniq_id makes sence if "request" field equal to uniq_id (imagined)
        if (uniq_id > rhs.uniq_id) {
            return false;
        }
        if (rank < rhs.rank) {
            return true;
        }
        if (rank > rhs.rank) {
            return false;
        }
        return false;
    }

    PEAT_Num pack_size() const;
    PEAT_Num pack(char *ptr) const;
    PEAT_Num unpack(const char *ptr);
    friend std::ostream &operator<<(std::ostream &, const Request &);
};

#endif // __REQUESTS_H__
