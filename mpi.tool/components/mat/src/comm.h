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

#ifndef __COMM_H__
#define __COMM_H__

#include <string>
#include <vector>

#include "common_types.h"
#include "file_io.h"
#include "ptp.h"
#include "tevent.h"
#include "request_store.h"
#include "requests.h"

class Comm : public Outputs
{
  public:
    Comm(
         /*
        #ifdef PEAT_MPI
                const std::vector<PEAT_Rank>& wR, const std::vector<PEAT_Rank>& mRs, const MPI_Comm mat_cc, const PEAT_Rank m_r,
        #endif
        */
         int v = false);

    void initialize(PTP *p, Communicators *c, Request_Store *rs, Requests* r, File_Storage_GZ *MPI, File_Storage_GZ *HWC, File_Storage_GZ *bind);

    void parse_comm(const PEAT_Rank rank, Tractor_Event &event);
    void print_comm_out(const Tractor_Event &event);
    PEAT_Bool update_request(const PEAT_Rank rank, PEAT_MPI_Request request, const Phase& phase, PEAT_Bool is_remote);

    PTP *ptp;
    Communicators *comm_store;
    Request_Store* req_store;
    Requests* request_func_store;
    int verbose;
};

#endif // __COMM_H__
