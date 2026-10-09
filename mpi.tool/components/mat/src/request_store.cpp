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
    q_name_s[qt_send] = "Send";
    q_name_s[qt_recv] = "Recv";
    q_name_s2[qt_pers] = "Persistent";
    q_name_s2[qt_hanged] = "Hanged";
    q_name_s2[qt_normal] = "Normal";
    assert(p);
    ptp = p;
    resize_queues(ptp->get_rank_num());
}

Request_Store::~Request_Store()
{
    ptp = 0;
}

bool Request_Store::find_request(PEAT_Num queue_type, PEAT_Rank rank, PEAT_MPI_Request request, MAT_Record** ret_it, PEAT_Bool erase)
{
    std::map<PEAT_Rank, std::set<PEAT_MPI_Request>>* queue;
    switch (queue_type) {
    case qt_pers_thrown:
        queue = &thrown_away_pers_requests;
        break;
    case qt_thrown:
        queue = &thrown_away_requests;
        break;
    default: {
#if 1
        {
            request_queues_type& queue = request_queues[queue_type];
            if (!queue[rank].empty()) {
                std::map<PEAT_MPI_Request, std::list<MAT_Record*>>::iterator event_it = queue[rank].find(request);
                if (event_it != queue[rank].end()) {
                    if (!event_it->second.empty()) {
                        assert(!(queue_type == qt_pers && event_it->second.size() > 1)); // only 1 persistent request is allowed at any given time
                        *ret_it = event_it->second.front();
                        if (erase)
                            event_it->second.pop_front();
                        return true;
                    }
                }
            }
            return false;
        }
#else
        request_queues_type& queue = request_queues[queue_type];
        if (!queue[rank].empty()) {
            std::map<PEAT_MPI_Request, std::list<MAT_Record*>>::iterator rank_it = queue[rank].find(request);
            if (rank_it != queue[rank].end()) {
                if (!rank_it->second.empty()) {
                    assert(!(queue_type == qt_pers && rank_it->second.size() > 1)); // only 1 persistent request is allowed at any given time
                    for (std::list<MAT_Record*>::iterator event_it = rank_it->second.begin(); event_it != rank_it->second.end(); ++event_it) {
                        if ((*event_it)->request == request) {
                            *ret_it = *event_it;
                            if (erase) {
                                rank_it->second.erase(event_it);
                            }
                            return true;
                        }
                    }
                }
            }
        }
        return false;
#endif
    }
    }
    if (!(*queue)[rank].empty()) {
        std::set<PEAT_MPI_Request>::iterator event_it = (*queue)[rank].find(request);
        if (event_it != (*queue)[rank].end()) {
            if (erase)
                (*queue)[rank].erase(event_it);
            *ret_it = NULL;
            return true;
        } /*else {
            fprintf(stderr,"No active persistent request %x found int %d",request, rank);
        }*/
    }
    return false;
}
PEAT_Bool Request_Store::remove_pers_req(const PEAT_Rank rank, const PEAT_MPI_Request request, MAT_Record** req_it)
{
    bool req_found = false;
    req_found = find_request(qt_pers, rank, request, req_it, true);
    if (req_found) {
        return true;
    }
    req_found = find_request(qt_pers_thrown, rank, request, req_it, true);
    if (req_found) {
        return true;
    }
    return false; // didn't find request, move to ordinary queues
}
PEAT_Bool Request_Store::remove_req(const PEAT_Rank rank, const PEAT_MPI_Request request)
{
    MAT_Record* req_it;
    bool req_found = false;
    req_found = find_request(qt_normal, rank, request, &req_it, true);
    if (req_found) {
        return true;
    }
    req_found = find_request(qt_hanged, rank, request, &req_it, true);
    if (req_found) {
        return true;
    }
    return false; // didn't find request, move to ordinary queues
}


