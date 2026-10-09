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

#ifndef __REQUEST_STORE_H__
#define __REQUEST_STORE_H__
#include "communicators.h"
#include "func_id.h"
#include "ptp.h"
#include "records.h"

#include <list>
#include <map>
#include <string>
#include <vector>

#define INITIAL_ABF 10
#define USE_FAST_CHECK
typedef std::vector<std::map<PEAT_MPI_Request, std::list<MAT_Record*>>> request_queues_type;

class Request_Store
{
public:
    PTP *ptp;
    std::string q_name_s[qt_size];
    std::string q_name_s2[qt_size2];
    Request_Store(PTP* p, int v);
    ~Request_Store();
    request_queues_type request_queues[qt_size2]; // persistent - 0 hanged - 1 normal -  2 (hanged and normal for sends only)
    void resize_queues(size_t size)
    {
        request_queues[0].resize(size);
        request_queues[1].resize(size);
        request_queues[2].resize(size);
    }
    bool find_request(PEAT_Num queue_type, PEAT_Rank rank, PEAT_MPI_Request request, MAT_Record** ret_it, PEAT_Bool erase);
    PEAT_Bool remove_pers_req(const PEAT_Rank rank, const PEAT_MPI_Request request, MAT_Record** req_it);
    PEAT_Bool remove_req(const PEAT_Rank rank, const PEAT_MPI_Request request);
    std::map<PEAT_Rank, std::set<PEAT_MPI_Request>> thrown_away_requests;
    std::map<PEAT_Rank, std::set<PEAT_MPI_Request>> thrown_away_pers_requests;

    friend class Request;
};
#endif // __REQUEST_STORE_H__
