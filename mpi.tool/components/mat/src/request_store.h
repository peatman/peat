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
 /* technically it's no required to distinguish P2P from collective or from internal, but it may be useful for debugging purposes */
enum Request_Store_Record_Type {
    REQUEST_TYPE_NULL, // error signal
    REQUEST_TYPE_ANY_PERSISTENT, // for searching purposes only, not for storing
    REQUEST_TYPE_ANY_NOT_PERSISTENT, // for searching purposes only, not for storing
    REQUEST_TYPE_PERSISTENT, // User persistent requests (from XXX_init functions)
    REQUEST_TYPE_USER_P2P, // User normal requests (from pt2pt MPI_Ixxx of MPI_START)
    REQUEST_TYPE_USER_COLLECTIVE, // User normal requests (from collective MPI_Ixxx of MPI_START)
    REQUEST_TYPE_USER_COMM, // User normal requests (from comm MPI_Ixxx )
    REQUEST_TYPE_INTERNAL // MPI internal requests (from MPIC_Ixxx) 
};
struct MAT_Request_Store_Record {
    Request_Store_Record_Type  request_type;
	MAT_Record* record;
};
typedef std::vector<std::map<PEAT_MPI_Request, std::list<MAT_Request_Store_Record>>> request_queues_type;

class Request_Store
{
private:
    PTP *ptp;
    request_queues_type queues; 
    void resize_queues(size_t size)
    {
        queues.resize(size);
    }
public:
    Request_Store(PTP* p, int v);
    bool find_request(const PEAT_Rank rank, const PEAT_MPI_Request request, const Request_Store_Record_Type type, MAT_Request_Store_Record* ret_it, const PEAT_Bool erase);
    PEAT_Bool add_request(const PEAT_Rank rank, const PEAT_MPI_Request request, const MAT_Request_Store_Record* rec);
    PEAT_Bool remove_request(const PEAT_Rank rank, const PEAT_MPI_Request request, const Request_Store_Record_Type type, MAT_Request_Store_Record* rec);
#ifdef PEAT_MPI
    bool find_thrown_away_request(PEAT_Num queue_type, PEAT_Rank rank, PEAT_MPI_Request request, PEAT_Bool erase);
    std::map<PEAT_Rank, std::map<PEAT_MPI_Request, MAT_Request_Record>> flown_away_requests, flown_away_pers_requests; // requests to be handled on another rank
    std::map<PEAT_Rank, std::set<PEAT_MPI_Request>> thrown_away_requests,thrown_away_pers_requests;// requests with MPI_PROC_NULL (weird but it happens)

#endif
    friend class Request;
};
#endif // __REQUEST_STORE_H__
