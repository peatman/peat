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
/* This file will contain universal request storage for all type of events involved (p2p,collective,fio,comm)*/
#include "p2pal.h"
#include "utils.h"
#include "request_store.h"

#include <assert.h>
#include <sstream>

Request_Store::Request_Store(PTP *p,int v) 
{
    assert(p);
    ptp = p;
    resize_queues(ptp->get_rank_num());
}

PEAT_Bool Request_Store::find_request(const PEAT_Rank rank, const PEAT_MPI_Request request, const Request_Store_Record_Type type, MAT_Request_Store_Record* ret_it, const PEAT_Bool erase)
{
    if (!queues[rank].empty()) {
        std::map<PEAT_MPI_Request, std::list<MAT_Request_Store_Record>>::iterator req_it = queues[rank].find(request);
        if(req_it != queues[rank].end()) {
            for(std::list<MAT_Request_Store_Record >::iterator event_it = req_it->second.begin(); event_it != req_it->second.end(); ++event_it) {
                if(event_it->request_type == type ||
                    (event_it->request_type == REQUEST_TYPE_PERSISTENT && type == REQUEST_TYPE_ANY_PERSISTENT) ||
                    ((event_it->request_type == REQUEST_TYPE_USER_P2P || event_it->request_type == REQUEST_TYPE_USER_COLLECTIVE ||
                        event_it->request_type == REQUEST_TYPE_USER_COMM || event_it->request_type == REQUEST_TYPE_USER_FIO ||
                        event_it->request_type == REQUEST_TYPE_USER_OSC) &&
                        type == REQUEST_TYPE_ANY_NOT_PERSISTENT)) {
                    *ret_it = *event_it;
                    if(erase) {
                        req_it->second.erase(event_it);
                    }
                    return true;
                }
            }
        }
    }
    return false;
}
PEAT_Bool Request_Store::add_request(const PEAT_Rank rank, const PEAT_MPI_Request request, const MAT_Request_Store_Record* rec)
{
	queues[rank][request].push_back(*rec);
    return true;
}

PEAT_Bool Request_Store::remove_request(const PEAT_Rank rank, const PEAT_MPI_Request request, Request_Store_Record_Type type, MAT_Request_Store_Record* req_it)
{
	return find_request(rank, request, type, req_it, true);
}

#ifdef PEAT_MPI
PEAT_Bool Request_Store::find_thrown_away_request(PEAT_Num queue_type, PEAT_Rank rank, PEAT_MPI_Request request, PEAT_Bool erase)
{
    std::map<PEAT_Rank, std::set<PEAT_MPI_Request>>* queue;
    switch(queue_type) {
    case qt_pers_thrown:
        queue = &thrown_away_pers_requests;
        break;
    case qt_thrown:
        queue = &thrown_away_requests;
        break;
    }
    if(!(*queue)[rank].empty()) {
        std::set<PEAT_MPI_Request>::iterator event_it = (*queue)[rank].find(request);
        if(event_it != (*queue)[rank].end()) {
            if(erase)
                (*queue)[rank].erase(event_it);
            return true;
        } /*else {
            fprintf(stderr,"No active persistent request %x found int %d",request, rank);
        }*/
    }
    return false;
}
#endif

