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

#if 0
#include "p2pal.h"
#include "requests.h"
void P2PAL::parse_p2_p_tx(PEAT_Rank rank, const Tractor_Event &event)
{
    assert(fId); assert(ptp); assert(com_store);
    const PEAT_MPI_Comm comm = !fId->check_property(event.func_id, PEAT_PROP_COMM) ? PEAT_MPI_Comm(2) : event.P2P.comm;
    ptp->add_rank_MPI(rank, event.tsc_time.func.out - event.tsc_time.func.in);
    PEAT_Rank recv_rank = event.P2P.peer;//com_store->get_rank_from_comm(comm, rank, event.P2P.peer, event.tsc_time.func.in);
    urpu_record record;
    record.pc_level = event.PControl.ctl;
    record.comm = comm;
    record.rank = rank;
    record.peer_rank = recv_rank;
    record.tag = event.P2P.tag;
    record.size = event.P2P.transfered_bytes;
    if(event.P2P.request) {
        record.request = event.P2P.request;
    }
    Phase ph_tx;
    ph_tx.func_id = event.func_id;
    ph_tx.tid = event.tid;
    ph_tx.tsc_time = event.tsc_time;
    ph_tx.tsc_u_time = event.tsc_u_time;
    ph_tx.addr = event.addr;
    ph_tx.eaddr = event.eaddr;
    ph_tx.uniq_id = event.uniq_id;
    record.ph[0] = record.ph[1] = record.ph[2] = ph_tx;
#ifndef PEAT_MPI
    print_p2_pl(event);
#endif
    if(fId->check_property(event.func_id, PEAT_PROP_PERSISTENT_REQUEST)) {//add persistent request to queue and finish
        p2pal_queues[qt_pers][rank].push_back(record);
        return;
    }
    if(record.peer_rank != com_store->get_MPI_PROC_NULL() || record.request) {
        std::list<urpu_record>::iterator matching_rec_it;
        if(match_peer(p2pal_queues[qt_recv], matching_rec_it, record)) {
            if(record.request > 0 || matching_rec_it->request > 0) {
                std::list<urpu_record> &rank_to_add = p2pal_queues[qt_send][rank];
                std::list<urpu_record>::iterator new_tx_it = rank_to_add.insert(rank_to_add.end(), record);
                new_tx_it->peer = matching_rec_it;
                new_tx_it->peer_assigned = true;
                matching_rec_it->peer = new_tx_it;
                matching_rec_it->peer_assigned = true;
            } else {
                print_p2_pf(record, *matching_rec_it);
                p2pal_queues[qt_recv].at(matching_rec_it->rank).erase(matching_rec_it);
            }
        } else {
            p2pal_queues[qt_send][rank].push_back(record);
        }
    }
}
void P2PAL::parse_p2_p_rx(PEAT_Rank rank, const Tractor_Event &event, PEAT_Bool increase_mpi)
{
    assert(fId);  assert(ptp); assert(com_store);
    const PEAT_MPI_Comm comm = !fId->check_property(event.func_id, PEAT_PROP_COMM) ? 2 : event.P2P.comm;
    if(increase_mpi) {
        ptp->add_rank_MPI(rank, event.tsc_time.func.out - event.tsc_time.func.in);
    }
    PEAT_Rank send_rank = event.P2P.peer;// com_store->get_rank_from_comm(comm, rank, event.P2P.peer, event.tsc_time.func.in);
    urpu_record record;
    record.pc_level = event.PControl.ctl;
    record.comm = comm;
    record.rank = rank;
    record.peer_rank = send_rank;
    record.tag = event.P2P.tag;
    record.size = event.P2P.transfered_bytes;
    if(event.P2P.request) {
        record.request = event.P2P.request;
    }
    Phase ph_rx;
    ph_rx.func_id = event.func_id;
    ph_rx.tid = event.tid;
    ph_rx.tsc_time = event.tsc_time;
    ph_rx.tsc_u_time = event.tsc_u_time;
    ph_rx.addr = event.addr;
    ph_rx.eaddr = event.eaddr;
    ph_rx.uniq_id = event.uniq_id;
    record.ph[0] = record.ph[1] = record.ph[2] = ph_rx;
    print_p2_pl(event);
    if(fId->check_property(event.func_id, PEAT_PROP_PERSISTENT_REQUEST)) {//add persistent request to queue and finish
        p2pal_queues[qt_pers][rank].push_back(record);
        return;
    }
    if(record.peer_rank != com_store->get_MPI_PROC_NULL() || record.request) {
        std::list<urpu_record>::iterator matching_rec_it;
        if(match_peer(p2pal_queues[qt_send], matching_rec_it, record)) {
            if(record.request > 0 || matching_rec_it->request > 0) {
                std::list<urpu_record> &rank_to_add = p2pal_queues[qt_recv][rank];
                std::list<urpu_record>::iterator new_it_rx = rank_to_add.insert(rank_to_add.end(), record);
                new_it_rx->peer = matching_rec_it;
                new_it_rx->peer_assigned = true;
                matching_rec_it->peer = new_it_rx;
                matching_rec_it->peer_assigned = true;
            } else {
                print_p2_pf(*matching_rec_it, record);
                p2pal_queues[qt_send].at(matching_rec_it->rank).erase(matching_rec_it);
            }
        } else {
            p2pal_queues[qt_recv][rank].push_back(record);
        }
    }
}

void P2PAL::process_remote_event(PEAT_Signed_Num size, PEAT_Rank src) {
    char *buffer = (char *)_alloca(size);
    MPI_Status status;
    MPI_Recv(buffer, size, MPI_CHAR, src, TRACTOR_P2P_EVENT_TAG, urpu_compute_comm, &status);
    Tractor_Event temp_event;
    temp_event.unpack(buffer);
    parse_p2_p_tx(temp_event.rank, temp_event);
}

void  Request::process_remote_request(PEAT_Signed_Num size, PEAT_Rank src) {
    char *buffer = (char *)_alloca(size);
    MPI_Status status;
    tractor_request_event temp_req_event;
    MPI_Recv(buffer, size, MPI_CHAR, src, TRACTOR_REQUEST_TAG, urpu_compute_comm, &status);
    temp_req_event.unpack(buffer);
    if(fId->check_property(temp_req_event.func_id, PEAT_PROP_PERSISTENT_REQUEST)) { //start,startall
        parse_p2_p_start_phase(temp_req_event.rank, temp_req_event, temp_req_event.request, temp_req_event.pc_level);
    } else {
        if(temp_req_event.func_id == MPI_REQUEST_FREE_ID) {
            if(remove_pers_req(temp_req_event.rank, temp_req_event.request) >= 0) {
                urpu_record record;//Fill 'record' structure for further writing.
                record.comm = 0;
                record.rank = temp_req_event.rank;
                record.peer_rank = -1;
                record.tag = -1;
                record.size = 0;
                record.ph[2] = temp_req_event;
                record.pc_level = temp_req_event.pc_level;
                //p2p_store->print_local(record);//Record event to "local" file.
                return;
            }
        }
        PEAT_Bool found_one_p2_p = parse_p2_p_completion_phase(temp_req_event.rank, temp_req_event, temp_req_event.request, -2, -1, temp_req_event.pc_level, qt_send, qt_send);
        PEAT_Bool found_one_collective = false;
        if(!found_one_p2_p) {
            PEAT_Bool found_one_collective = parse_collective_completion_phase(temp_req_event.rank, temp_req_event, temp_req_event.request, temp_req_event.pc_level);
            if(!found_one_collective) {
                std::cerr << "Request " << std::hex << temp_req_event.request << std::dec << " is 'lost'. Rank: " << temp_req_event.rank << ". uniq_id: " << temp_req_event.uniq_id << " Skipped." << std::endl;
                PEAT_Bool found_one_p2_pr = parse_p2_p_completion_phase(temp_req_event.rank, temp_req_event, temp_req_event.request, -2, -1, temp_req_event.pc_level, qt_send, qt_send);
                if(!found_one_p2_pr) {
                    PEAT_Bool found_one_collective = parse_collective_completion_phase(temp_req_event.rank, temp_req_event, temp_req_event.request, temp_req_event.pc_level);
                }
            }
        }
    }
    return;
}
PEAT_Bool P2PAL::find_request(PEAT_Num queue_type, PEAT_Rank rank, PEAT_MPI_Request request, std::list<urpu_record>::iterator &ret_it)
{
    std::list<urpu_record> &queue = p2pal_queues[queue_type][rank];
    if(!queue.empty()) {
        for(std::list<urpu_record>::iterator it = queue.begin(); it != queue.end(); ++it) {
            if(it->request == request) {
                ret_it = it;
                return true;
            }
        }
    }
    return false;
}

PEAT_Bool P2PAL::update_request(PEAT_Num queue_type, PEAT_Rank rank, const PEAT_MPI_Tag tag, const PEAT_Rank source, PEAT_MPI_Request request, const Phase &phase, std::list<urpu_record>::iterator &req_it)
{
    std::list<urpu_record>::iterator peer_it;
    if(req_it->request == request) {
        if(req_it->peer_rank == com_store->get_MPI_PROC_NULL()) {
            //print_local(*req_it);
            p2pal_queues[queue_type].at(rank).erase(req_it);
            return true;
        }
        if(queue_type == qt_recv) {
            if(req_it->tag == com_store->get_MPI_ANY_TAG()) { //any tag
                req_it->tag = tag;
            } else {
                if(req_it->tag != tag) {
                    //                                        cerr << "Tags mismatch: " << it->tag << ":" << tag << " ignored." << endl;
                }
            }
            if(req_it->peer_rank == com_store->get_MPI_ANY_SOURCE()) { //any source
                req_it->peer_rank = source;// com_store->get_rank_from_comm(it->comm, rank, source, it->ph[1].tsc_u_time.func.in);
            } else {
                if(req_it->peer_rank != source) {
                    //                                        cerr << "Ranks mismatch " << it->peer_rank << ":" << source << " ignored." << endl;
                }
            }
        }
        req_it->request = 0;
        //update completion tsc if matched
        req_it->ph[2] = phase;
        if(!req_it->peer_assigned) {
            if(match_peer(p2pal_queues[(queue_type == qt_send) ? qt_recv : qt_send], peer_it, *req_it)) {
                peer_it->peer_assigned = true;
                req_it->peer_assigned = true;
                req_it->peer = peer_it;
                peer_it->peer = req_it;
            } else {
                return true;
            }
        } else {
            peer_it = req_it->peer;
        }
        if(peer_it->request == 0) {
            std::list<urpu_record>::iterator lP = (queue_type == qt_send) ? req_it : peer_it;
            std::list<urpu_record>::iterator rP = (queue_type == qt_send) ? peer_it : req_it;
            print_p2_pf(*lP, *rP);
            p2pal_queues[qt_send].at(lP->rank).erase(lP);
            p2pal_queues[qt_recv].at(rP->rank).erase(rP);
        }
        return true;
    }
    return false;
}
void Request::parse_cn(const PEAT_Rank rank, Tractor_Event &event) {
    //cerr << "DBG1 " << event << "\n";
    const PEAT_Num qn_start = qt_send;
    const PEAT_Num qn_stop = qt_recv;
    assert(fId); assert(ptp); assert(com_store);
    ptp->add_rank_MPI(rank, event.tsc_time.func.out - event.tsc_time.func.in);
    /*    vector<PEAT_MPI_Request> extra_info=event.requests;
    vector<PEAT_MPI_Request> built_requests;
    vector<PEAT_MPI_Tag> tags;
    vector<PEAT_Rank> sources;*/
    urpu_record record;//Fill 'record' structure for further writing.
    record.pc_level = event.PControl.ctl;
    record.comm = 0;
    record.rank = rank;
    record.peer_rank = -1;
    record.tag = -1;
    record.size = 0;

    Phase ph_cn;
    ph_cn.func_id = event.func_id;
    ph_cn.tid = event.tid;
    ph_cn.tsc_time = event.tsc_time;
    ph_cn.tsc_u_time = event.tsc_u_time;
    ph_cn.addr = event.addr;
    ph_cn.eaddr = event.eaddr;
    ph_cn.uniq_id = event.uniq_id;
    record.ph[0] = record.ph[1] = record.ph[2] = ph_cn;
    print_req_out(event);
    //build list of completed requests
    if(event.func_id == MPI_REQUEST_FREE_ID) { //mpi_request_free
#ifdef PEAT_MPI
        if(!p2p_store->flown_away_pers_requests.empty()) {
            if(p2p_store->flown_away_pers_requests.count(rank)) {
                for(std::list<urpu_request_record>::iterator req_it = p2p_store->flown_away_pers_requests.at(rank).begin(); req_it != p2p_store->flown_away_pers_requests.at(rank).end(); ++req_it) {
                    if(req_it->request == event.requests.at(0)) {
                        tractor_request_event req_event(ph_cn, req_it->request, rank, req_it->peer);
                        print_req_bind(rank, req_it->request, event.uniq_id, req_it->uniq_id);
                        send_p2_p_cn(req_event);
                        p2p_store->flown_away_pers_requests.at(rank).erase(req_it);
                        return;
                    }
                }
            }
        }
        if(!p2p_store->flown_away_requests.empty()) {
            if(p2p_store->flown_away_requests.count(rank)) {
                for(std::list<urpu_request_record>::iterator req_it = p2p_store->flown_away_requests.at(rank).begin(); req_it != p2p_store->flown_away_requests.at(rank).end(); ++req_it) {
                    if(req_it->request == event.requests.at(0)) {
                        tractor_request_event req_event(ph_cn, req_it->request, rank, req_it->peer);
                        print_req_bind(rank, req_it->request, event.uniq_id, req_it->uniq_id);
                        send_p2_p_cn(req_event);
                        p2p_store->flown_away_requests.at(rank).erase(req_it);
                        return;
                    }
                }
            }
        }

#endif
        PEAT_UID req_uniq_id = remove_pers_req(rank, event.requests.at(0));
        if(req_uniq_id >= 0) {
            print_req_bind(rank, event.requests.at(0), event.uniq_id, req_uniq_id);
            //p2p_store->print_local(record);//Record event to "local" file.
            return;
        }

    }
    if(fId->check_property(event.func_id, PEAT_PROP_PERSISTENT_REQUEST)) { //start,startall
#ifdef PEAT_MPI
        for(std::vector<PEAT_MPI_Request>::iterator it = event.requests.begin(); it != event.requests.end(); ++it) {
            if(!p2p_store->flown_away_pers_requests.empty()) {
                if(p2p_store->flown_away_pers_requests.count(rank)) {
                    for(std::list<urpu_request_record>::iterator req_it = p2p_store->flown_away_pers_requests.at(rank).begin(); req_it != p2p_store->flown_away_pers_requests.at(rank).end(); ++req_it) {
                        if(*it == req_it->request) {
                            tractor_request_event req_event(ph_cn, *it, rank, req_it->peer);
                            send_p2_p_cn(req_event);
                            *it = com_store->get_MPI_REQUEST_NULL(); ///make it invalid
                            break;
                        }
                    }
                }
            }
        }
#endif
        for(std::vector<PEAT_MPI_Request>::const_iterator it = event.requests.begin(); it != event.requests.end(); ++it) {
            if(*it != com_store->get_MPI_REQUEST_NULL()) {
                parse_p2_p_start_phase(rank, ph_cn, *it, record.pc_level);
            }
        }
        return;
    }
    //cerr << "DBG2 " << event << "\n";
    PEAT_Bool found_one = false;
    for(PEAT_Num i = 0; i < event.Requests.triggered_request_num; ++i) {
        //pull requests from all available queues: message_queues[SENDQ],message_queues[RECVQ]
        PEAT_MPI_Request request = event.requests.at(event.statuses.at(i).index);
        const PEAT_MPI_Tag tag = event.statuses.at(i).tag;
        const PEAT_Rank source = event.statuses.at(i).src;
        if(request == com_store->get_MPI_REQUEST_NULL()) {
            std::cerr << "Null request detected. Rank: " << rank << ". Skipped." << std::endl;
            continue;
        }
        if(tag == com_store->get_MPI_ANY_TAG() && source == com_store->get_MPI_ANY_SOURCE()) {
            continue; //happens when TST or WAIT for inactive persistent request. skip
        }
#ifdef PEAT_MPI
        if(!p2p_store->flown_away_pers_requests.empty()) {
            if(p2p_store->flown_away_pers_requests.count(rank)) {
                for(std::list<urpu_request_record>::iterator it = p2p_store->flown_away_pers_requests.at(rank).begin(); it != p2p_store->flown_away_pers_requests.at(rank).end(); ++it) {//it's safe to delete iterator because "break" after list::erase()
                    if(it->request == request) {
                        tractor_request_event req_event(ph_cn, request, rank, it->peer);
                        print_req_bind(rank, it->request, event.uniq_id, it->uniq_id);
                        send_p2_p_cn(req_event);
                        request = com_store->get_MPI_REQUEST_NULL();
                        found_one = true;
                        break;
                    }
                }
            }
        }
        if(!p2p_store->flown_away_requests.empty()) {
            if(p2p_store->flown_away_requests.count(rank)) {
                for(std::list<urpu_request_record>::iterator it = p2p_store->flown_away_requests.at(rank).begin(); it != p2p_store->flown_away_requests.at(rank).end(); ++it) {//it's safe to delete iterator because "break" after list::erase()
                    if(it->request == request) {
                        tractor_request_event req_event(ph_cn, request, rank, it->peer);
                        print_req_bind(rank, it->request, event.uniq_id, it->uniq_id);
                        send_p2_p_cn(req_event);
                        p2p_store->flown_away_requests.at(rank).erase(it);
                        request = com_store->get_MPI_REQUEST_NULL();
                        found_one = true;
                        break;
                    }
                }
            }
        }
        if(!coll_store->flown_away_requests.empty()) {
            if(coll_store->flown_away_requests.count(rank)) {
                for(std::list<urpu_request_record>::iterator it = coll_store->flown_away_requests.at(rank).begin(); it != coll_store->flown_away_requests.at(rank).end(); ++it) {//it's safe to delete iterator because "break" after list::erase()
                    if(it->request == request) {
                        tractor_request_event req_event(ph_cn, request, rank, it->peer);
                        print_req_bind(rank, it->request, event.uniq_id, it->uniq_id);
                        send_coll_cn(req_event);
                        coll_store->flown_away_requests.at(rank).erase(it);
                        request = com_store->get_MPI_REQUEST_NULL();
                        found_one = true;
                        break;
                    }
                }
            }
        }

#endif

        if(request != com_store->get_MPI_REQUEST_NULL()) {
            PEAT_Bool found_one_p2_p = parse_p2_p_completion_phase(rank, ph_cn, request, tag, source, record.pc_level, qn_start, qn_stop);
            PEAT_Bool found_one_collective = false;
            if(!found_one_p2_p) {
                found_one_collective = parse_collective_completion_phase(rank, ph_cn, request, record.pc_level);
                if(!found_one_collective) {
                    std::cerr << "Request " << std::hex << request << std::dec << " is 'lost'. Rank: " << rank << ". uniq_id: " << event.uniq_id << " Skipped." << std::endl;
                    found_one_p2_p = parse_p2_p_completion_phase(rank, ph_cn, request, tag, source, record.pc_level, qn_start, qn_stop);
                    found_one_collective = parse_collective_completion_phase(rank, ph_cn, request, record.pc_level);
                }
            }
            found_one |= found_one_p2_p | found_one_collective;
        }
    }
    /*if(!found_one) {
    p2p_store->print_local(record); //print "local" record
    }*/
    return;
}
void P2PAL::send_p2_p_tx(PEAT_Rank rank, const Tractor_Event &event)
{
    if(event.P2P.peer < 0) {
        parse_local(rank, event);
        return;
    }
    const PEAT_MPI_Comm comm = !fId->check_property(event.func_id, PEAT_PROP_COMM) ? 2 : event.P2P.comm;
    PEAT_Rank peer = event.P2P.peer;//com_store->get_rank_from_comm(comm, rank, event.P2P.peer, event.tsc_in);
    PEAT_Rank urpu_peer = whole_ranks[peer];
    print_p2_pl(event);
#ifdef USE_LOCAL_PARSE
    if(urpu_peer == my_rank) {
        parse_p2_p_tx(rank, event);
        return;
    }
#endif
    urpu_request_record record;
    record.peer = peer;
    record.uniq_id = event.uniq_id;
    if(event.P2P.request > 0) {
        record.request = event.P2P.request;
        if(fId->check_property(event.func_id, PEAT_PROP_PERSISTENT_REQUEST)) {
            flown_away_pers_requests[rank].push_back(record);
        } else {
            flown_away_requests[rank].push_back(record);
        }
    }
    PEAT_Event_Delivery delivery;
    PEAT_Signed_Num size = (PEAT_Signed_Num)event.pack_size();
    delivery.buffer = (char *)peat_malloc_aligned(size);
    event.pack(delivery.buffer);
    MPI_Isend(delivery.buffer, size, MPI_CHAR, urpu_peer, TRACTOR_P2P_EVENT_TAG, urpu_compute_comm, &delivery.request);
    active_deliveries.push_back(delivery);
}
#endif