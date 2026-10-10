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

#include "requests.h"
#include "utils.h"
#include <assert.h>
#include <stdexcept>
#ifdef PEAT_MPI
#error "MPI version is not yet supported. Please use serial one."
#endif

Request::Request(int s,
#ifdef PEAT_MPI
				 std::list<PEAT_Event_Delivery> *ad, const std::vector<PEAT_Rank> &wR, const std::vector<PEAT_Rank> &mRs, const MPI_Comm mat_cc, const PEAT_Rank m_r,
#endif
				 int v) : skip_locals(s), verbose(v)
#ifdef PEAT_MPI
						  ,
						  active_deliveries(ad), mat_compute_comm(mat_cc), whole_ranks(wR), my_ranks(mRs), my_rank(m_r)
#endif

{
	comm_store = NULL;
	ptp = NULL;
	req_store = NULL;
	p2p_func_store = NULL;
	coll_func_store = NULL;
}

void Request::Initialize(PTP *p, Communicators *c, Request_Store *r, P2PAL *pp, Collectives *cl, File_Storage_GZ *MPI, File_Storage_GZ *HWC, File_Storage_GZ *bind)
{
	comm_store = c;
	ptp = p;
	req_store = r;
	p2p_func_store = pp;
	coll_func_store = cl;
	Outputs::Initialize(MPI, HWC, bind);
}

Request::~Request()
{
    ptp = 0;
    ptp = 0;
    comm_store = 0;
}
bool Request::discard_P2P_event(const PEAT_Rank rank, const PEAT_MPI_Request request, bool is_remote)
{
    // cerr << "DBG1 " << event << "\n";
    // pull requests from all available queues: message_queues[SENDQ],message_queues[RECVQ]
    if (request == comm_store->get_MPI_REQUEST_NULL()) {
        std::cerr << "Null request detected. Rank: " << rank << ". Skipped." << std::endl;
        return false;
    }
    MAT_Request_Store_Record dummy;
    return req_store->remove_request(rank, request, REQUEST_TYPE_USER_P2P, &dummy);
}

bool Request::discard_collective_event(const PEAT_Rank rank, const PEAT_MPI_Request request, bool is_remote)
{
    if (request == comm_store->get_MPI_REQUEST_NULL()) {
        std::cerr << "Null request detected. Rank: " << rank << ". Skipped." << std::endl;
        return false;
    }
    Collectives::collective_list::iterator coll;
    MAT_Request_Store_Record dummy;
    return req_store->remove_request(rank, request, REQUEST_TYPE_USER_COLLECTIVE, &dummy);
}
Request_Store_Record_Type  Request::select_event(PEAT_Rank rank, const PEAT_MPI_Request request, bool is_remote)
{
    MAT_Request_Store_Record ret_it;
    if(req_store->find_request(rank, request, REQUEST_TYPE_ANY_NORMAL, &ret_it, false)) {
        return ret_it.request_type;
    }
    return REQUEST_TYPE_NULL;
}

bool Request::parse_P2P_completion_phase(const PEAT_Rank rank, const Request_Store_Record_Type type, const Phase &phase,
                                         PEAT_MPI_Request request,
                                         const PEAT_MPI_Tag tag,
                                         const PEAT_Rank source,
                                         const PEAT_Message_Size size,
                                         const int pc_level,
                                         bool is_remote)
{
    // cerr << "DBG1 " << event << "\n";
    bool found_one = false;
    // pull requests from all available queues: message_queues[SENDQ],message_queues[RECVQ]
    if (request == comm_store->get_MPI_REQUEST_NULL()) {
        std::cerr << "Null request detected. Rank: " << rank << ". Skipped." << std::endl;
        return found_one;
    }
    bool check_and_clear_req[qt_size2] = {false, false, false};
    bool rFound = false;
    if (request > 0) {
        p2p_func_store->update_request(rank, type, tag, source, size, request, phase,is_remote);
    }

    return found_one;
}
bool Request::parse_collective_completion_phase(const PEAT_Rank rank, const Phase &phase, const PEAT_MPI_Request request, const int pc_level, bool is_remote)
{
    if (request == comm_store->get_MPI_REQUEST_NULL()) {
        std::cerr << "Null request detected. Rank: " << rank << ". Skipped." << std::endl;
        return false;
    }

    MAT_Request_Store_Record ret_it;
    if(req_store->find_request(rank, request, REQUEST_TYPE_USER_COLLECTIVE, &ret_it, false)) {
        MAT_Collectives_Record* coll = (MAT_Collectives_Record *) ret_it.record;
        coll_func_store->update_request(rank, request, phase, coll->coll_event);
        return true;
    }
    return false;
}

void Request::parse_pers_start_phase(const PEAT_Rank rank, const Phase &phase, const PEAT_MPI_Request request, const int pc_level, bool is_remote)
{
    MAT_Request_Store_Record ret_it;

    if (req_store->find_request(rank, request, REQUEST_TYPE_PERSISTENT, &ret_it, false)) {
        if (ret_it.record->record_type()==1) {
            MAT_P2P_Record *record = (MAT_P2P_Record *)ret_it.record;
            record->ph[1] = phase;
            record->pc_level = pc_level;
            p2p_func_store->add_pers_req_start(rank, record);
            print_req_bind(rank, request, record->ph[0], phase, is_remote);
        } 
        if(ret_it.record->record_type() == 2) {
			//collective request
            peat_throw_error("Persistent collectives are not (yet) supported.\n");
            assert(0);
        }
    } 
}
bool Request::remove_pers_req(const PEAT_Rank rank, const PEAT_MPI_Request request, MAT_Request_Store_Record* ret_it)
{

#ifdef PEAT_MPI
    bool req_found = req_store->find_thrown_away_request(qt_pers_thrown, rank, request, true);
    if(req_found) {
        return false;
    }
#endif
    return req_store->remove_request(rank, request, REQUEST_TYPE_PERSISTENT, ret_it);
}
void Request::parse_cn(const PEAT_Rank rank, Tractor_Event &event, PEAT_Bool is_remote)
{
    // cerr << "DBG1 " << event << "\n";
    assert(ptp);
    assert(comm_store);
    ptp->add_rank_MPI(rank, event.tsc_time.func.out - event.tsc_time.func.in);

    /*    vector<PEAT_MPI_Request> extra_info=event.requests;
    vector<PEAT_MPI_Request> built_requests;
    vector<PEAT_MPI_Tag> tags;
    vector<PEAT_Rank> sources;*/

    Phase ph_cn;
    ph_cn.func_id = event.func_id;
    ph_cn.thread_id = event.thread_id;
    ph_cn.tsc_time = event.tsc_time;
    ph_cn.tsc_u_time = event.tsc_u_time;
    ph_cn.addrs = event.addrs;
    ph_cn.ids = event.ids;
    if (!is_remote) {
        print_req_out(event);
    }
    if (event.func_id == MPI_PREADY_ID || event.func_id == MPI_PREADY_LIST_ID || event.func_id == MPI_PREADY_RANGE_ID || event.func_id == MPI_PARRIVED_ID) {
		//skip these starting events. They are not needed for request tracking. We will track only completion events.
        return;
	}
        // build list of completed requests
    if (event.func_id == MPI_REQUEST_FREE_ID) { // mpi_request_free
#ifdef PEAT_MPI
        if (!is_remote) {
            if (!req_store->flown_away_pers_requests.empty()) {
                if (req_store->flown_away_pers_requests.count(rank)) {
                    std::map<PEAT_MPI_Request, MAT_Request_Record>::iterator req_it = req_store->flown_away_pers_requests.at(rank).find(event.requests.at(0));
                    if (req_it != req_store->flown_away_pers_requests.at(rank).end()) {
                        send_cn(event, req_it->second.peer);
                        req_store->flown_away_pers_requests.at(rank).erase(req_it);
                        return;
                    } /*else {
                        std::cerr << "Invalid persistent request " << std::hex << event.requests.at(0) << std::dec << " detected during p2p persistent free.Rank: " << rank << ". primary_id: " << event.ids[0] << " ids: " << peat_ids_string(event.ids) << " Remote: " << is_remote << ".Skipped." << std::endl;
                    }*/
                }
            }
            if (!req_store->flown_away_requests.empty()) {
                if (req_store->flown_away_requests.count(rank)) {
                    std::map<PEAT_MPI_Request, MAT_Request_Record>::iterator req_it = req_store->flown_away_requests.at(rank).find(event.requests.at(0));
                    if (req_it != req_store->flown_away_requests.at(rank).end()) {
                        send_cn(event, req_it->second.peer);
                        req_store->flown_away_requests.at(rank).erase(req_it);
                        return;
                    }
                }
            }
        }
#endif
        MAT_Request_Store_Record resp;
        bool found = remove_pers_req(rank, event.requests.at(0), &resp);
        /*
#ifndef PEAT_MPI
            int my_rank = 0;
#endif
            fprintf(stderr, "Removing request %llx rank %d event %lld from rank %d in %d rank.(remote=%d)...", event.requests.at(0), rank, event.ids[0], rank, my_rank, is_remote);
        */
        if (found) {
             print_req_bind(rank, event.requests.at(0), resp.record->get_ph(0), event, is_remote);
            // fprintf(stderr, "done.\n");
            return;
        }
        // fprintf(stderr, "failed.\n");
    }
    if (event.func_id == MPI_CANCEL_ID) {
#ifdef PEAT_MPI
        // do the same thing as for MPI_REQUEST_FREE
        if (!is_remote) {
            if (!req_store->flown_away_requests.empty()) {
                if (req_store->flown_away_requests.count(rank)) {
                    std::map<PEAT_MPI_Request, MAT_Request_Record>::iterator req_it = req_store->flown_away_requests.at(rank).find(event.requests.at(0));
                    send_cn(event, req_it->second.peer);
                    req_store->flown_away_requests.at(rank).erase(req_it);
                    return;
                }
            }
        }
#endif
        // only 1 of following 2 should hit
        discard_P2P_event(rank, event.requests.at(0), is_remote);
        discard_collective_event(rank, event.requests.at(0), is_remote);
        return;
    }
#if 0
    if (event.func_id == MPID_REQUEST_RELEASE_ID) {
        // MPICH's internal completion for requests that never go through MPI_Wait/Test (e.g. a collective
        // implementation's internal progress requests). The tractor's hand-coded wrapper deliberately leaves
        // triggered_request_num at 0 (see T_MPID_Request_release in t_mpidef.h), so this can't fall through to
        // the generic wait/test loop below like OpenMPI's ompi_coll_p2p_free/cancel do (those set it via
        // PMPI_Request_get_status). Same peer lookup as MPI_REQUEST_FREE_ID; unlike free/cancel, a release
        // means the request genuinely completed, so there's no discard fallback if it isn't found.
        // fill it will predefined dummies 
        event.Requests.triggered_request_num = 1;
		event.statuses.resize(1);
        event.statuses[0].bytes = -1;;
        event.statuses[0].tag = comm_store->get_MPI_ANY_TAG();
        event.statuses[0].src = comm_store->get_MPI_ANY_SOURCE();
        event.statuses[0].index = 0;

#ifdef PEAT_MPI
        if (!is_remote) {
            if (!p2p_func_store->flown_away_requests.empty()) {
                if (p2p_func_store->flown_away_requests.count(rank)) {
                    std::map<PEAT_MPI_Request, std::list<MAT_Request_Record>>::iterator req_it = p2p_func_store->flown_away_requests.at(rank).find(event.requests.at(0));
                    if (req_it != p2p_func_store->flown_away_requests.at(rank).end()) {
                        if (!req_it->second.empty()) {
                            send_cn(event, req_it->second.front().peer);
                            req_it->second.pop_front();
                        }
                        return;
                    }
                }
            }
            if (!coll_func_store->flown_away_requests.empty()) {
                if (coll_func_store->flown_away_requests.count(rank)) {
                    std::map<PEAT_MPI_Request, MAT_Request_Record>::iterator req_it = coll_func_store->flown_away_requests.at(rank).find(event.requests.at(0));
                    if (req_it != coll_func_store->flown_away_requests.at(rank).end()) {
                        send_cn(event, req_it->second.peer);
                        coll_func_store->flown_away_requests.at(rank).erase(req_it);
                        return;
                    }
                }
            }
        }
#endif
        // return;
    }
#endif
    if (global_func_id.check_property(event.func_id, PEAT_PROP_PERSISTENT_REQUEST_START)) { // start,startall
#ifdef PEAT_MPI
        if (!is_remote) {
            for (std::vector<PEAT_MPI_Request>::iterator it = event.requests.begin(); it != event.requests.end(); ++it) {
                if (!req_store->flown_away_pers_requests.empty()) {
                    if (req_store->flown_away_pers_requests.count(rank)) {
                        std::map<PEAT_MPI_Request, MAT_Request_Record>::iterator req_it = req_store->flown_away_pers_requests.at(rank).find(*it);
                        if (req_it != req_store->flown_away_pers_requests.at(rank).end()) {
                            Tractor_Event tmp_evt = event;
                            tmp_evt.statuses.clear();
                            tmp_evt.requests.clear();
                            tmp_evt.addrs.clear();
                            tmp_evt.buffers.clear();
                            tmp_evt.Requests.triggered_request_num = tmp_evt.Requests.input_request_num = 1;
                            tmp_evt.requests.push_back(*it);
                            req_store->flown_away_requests[rank][*it] = req_it->second;
                            send_cn(tmp_evt, req_it->second.peer);
                            *it = comm_store->get_MPI_REQUEST_NULL(); // make it invalid
                        }
                    }
                }
            }
        }
        for (std::vector<PEAT_MPI_Request>::const_iterator it = event.requests.begin(); it != event.requests.end(); ++it) {
            if (*it != comm_store->get_MPI_REQUEST_NULL() && req_store->thrown_away_pers_requests[rank].find(*it) == req_store->thrown_away_pers_requests[rank].end()) { // only handle active requests
                parse_pers_start_phase(rank, ph_cn, *it, event.PControl.ctl, is_remote);
            } else {
                if (*it != comm_store->get_MPI_REQUEST_NULL()) { // only handle active requests
                    std::cerr << "Invalid persistent request " << std::hex << *it << std::dec << " detected.Rank: " << rank << ". primary_id: " << event.ids[0] << " ids: " << peat_ids_string(event.ids) << " Remote: " << is_remote << ".Skipped." << std::endl;
                    req_store->thrown_away_requests[rank].insert(*it); // for later mpi_test/wait
                }
            }
        }
#endif
        for (std::vector<PEAT_MPI_Request>::const_iterator it = event.requests.begin(); it != event.requests.end(); ++it) {

            MAT_Request_Store_Record ret_it;
            if (req_store->find_request(rank, *it, REQUEST_TYPE_PERSISTENT, &ret_it, false)) {
                if(ret_it.record->record_type()==1) {
                    parse_P2P_start_phase(rank, ph_cn, *it, &ret_it, event.PControl.ctl, is_remote);
                }
                if(ret_it.record->record_type() == 2) {
                    parse_collective_start_phase(rank, ph_cn, *it, &ret_it, event.PControl.ctl, is_remote);
                }

			}
        }
        return;
    }
    // special cases handled above. Now ordinal ones (wait/test)
    // bool found_one = false;
    for (PEAT_Num i = 0; i < event.Requests.triggered_request_num; ++i) {
        // pull requests from all available queues: message_queues[SENDQ],message_queues[RECVQ]
        PEAT_MPI_Request request = event.requests.at(event.statuses.at(i).index);
        const PEAT_MPI_Tag tag = event.statuses.at(i).tag;
        const PEAT_Rank source = event.statuses.at(i).src;
        const PEAT_Message_Size size = event.statuses.at(i).bytes;
        if (request == comm_store->get_MPI_REQUEST_NULL()) {
            std::cerr << "Null request detected. Rank: " << rank << ". Skipped." << std::endl;
            continue;
        }
#ifdef PEAT_MPI
        std::set<PEAT_MPI_Request>::iterator thrown_it = req_store->thrown_away_requests[rank].find(request);
        if (thrown_it != req_store->thrown_away_requests[rank].end()) {
            std::cerr << "Invalid request " << std::hex << request << std::dec << " detected. Rank: " << rank << ". primary_id: " << event.ids[0] << " ids: " << peat_ids_string(event.ids) << " Remote: " << is_remote << std::endl;
            req_store->thrown_away_requests[rank].erase(thrown_it);
            continue;
        }
#endif
#if 0
        if (tag == comm_store->get_MPI_ANY_TAG() && source == comm_store->get_MPI_ANY_SOURCE()) {
            MAT_P2P_Record *req_it;
            if (p2p_func_store->find_request(qt_pers, rank, request, &req_it, false)) { // check should never hit?
                continue; //happens when TEST or WAIT for inactive persistent request. skip
            }
        }
#endif
#ifdef PEAT_MPI
        if (!is_remote) {
            if ((!req_store->flown_away_requests.empty()) && request != comm_store->get_MPI_REQUEST_NULL()) {
                if (req_store->flown_away_requests.count(rank)) {
                    std::map<PEAT_MPI_Request, MAT_Request_Record>::iterator it = req_store->flown_away_requests.at(rank).find(request);
                    if(it != req_store->flown_away_requests.at(rank).end()) {
                        Tractor_Event tmp_evt = event;
                        tmp_evt.statuses.clear();
                        tmp_evt.requests.clear();
                        tmp_evt.addrs.clear();
                        tmp_evt.buffers.clear();
                        tmp_evt.Requests.triggered_request_num = tmp_evt.Requests.input_request_num = 1;
                        tmp_evt.requests.push_back(request);
                        PEAT_MPI_R4Status s = event.statuses.at(i);
                        s.index = 0;
                        tmp_evt.statuses.push_back(s);
                        send_cn(tmp_evt, it->second.peer);
                        request = comm_store->get_MPI_REQUEST_NULL();
                    }
                }
            }
        }
#endif
        if(event.ids[0] == 129 && rank == 1 && request == 0xac000007) {
            int id = event.ids[0];
        }
        if (request != comm_store->get_MPI_REQUEST_NULL()) {
            switch (select_event(rank, request, is_remote)) {
            case REQUEST_TYPE_USER_P2P:
                parse_P2P_completion_phase(rank, REQUEST_TYPE_USER_P2P, ph_cn, request, tag, source, size, event.PControl.ctl, is_remote);
                break;
            case REQUEST_TYPE_INTERNAL:
                parse_P2P_completion_phase(rank, REQUEST_TYPE_INTERNAL, ph_cn, request, tag, source, size, event.PControl.ctl, is_remote);
                break;
            case REQUEST_TYPE_USER_COLLECTIVE:
                parse_collective_completion_phase(rank, ph_cn, request, event.PControl.ctl, is_remote);
                break;
            default:
                if (!(tag == comm_store->get_MPI_ANY_TAG() && source == comm_store->get_MPI_ANY_SOURCE()) && request != 0 ) {
#ifndef PEAT_MPI
                    PEAT_Rank my_rank = 0;
#endif
                    std::cerr << "Rank " << my_rank << ". Request " << std::hex << request << std::dec << " is 'lost'. Rank: " << rank << ". primary_id: " << event.ids[0] << " ids: " << peat_ids_string(event.ids) << " Remote: " << is_remote << " Skipped." << std::endl;
                    // bool found_one_p2_p = parse_P2P_completion_phase(rank, ph_cn, request, tag, source, size, record.pc_level, qn_start, qn_stop, is_remote);
                }
                break;
            }
#if 0
            bool found_one_p2_p = parse_P2P_completion_phase(rank, ph_cn, request, tag, source, size, event.PControl.ctl, is_remote);
            bool found_one_collective = false;
            if (!found_one_p2_p) {
                found_one_collective = parse_collective_completion_phase(rank, ph_cn, request, event.PControl.ctl, is_remote);
                if (!found_one_collective && event.Requests.input_request_num==1) { // there're cases where MPI_{WAIT|TEST}{ALL|ANY|SOME} propagated to ranks they don't belong. it's tended normal just ignore missing requests
#ifndef PEAT_MPI
                    PEAT_Rank my_rank = 0;
#endif
                    std::cerr << "Rank " << my_rank << ". Request " << std::hex << request << std::dec << " is 'lost'. Rank: " << rank << ". primary_id: " << event.ids[0] << " ids: " << peat_ids_string(event.ids) << " Remote: " << is_remote << " Skipped." << std::endl;
                    // bool found_one_p2_p = parse_P2P_completion_phase(rank, ph_cn, request, tag, source, size, record.pc_level, qn_start, qn_stop, is_remote);
                }
            }
            // found_one |= found_one_p2_p | found_one_collective;
#endif
        }
    }
    /*if(!found_one) {
        p2p_func_store->print_local(record); //print "local" record
    }*/
    return;
}
void Request::print_req_bind(PEAT_Rank rank, PEAT_MPI_Request req, const Phase &phase, const Phase &bind, bool is_remote)
{
    if (is_remote) {
#ifdef PEAT_MPI // in case of serial is_remote is always false
        char *tbuf = (char *)peat_malloc_aligned(1024);
        int *rnk = (int *)tbuf;
        *rnk = rank; // TODO need real rank
        PEAT_Rank mat_peer = whole_ranks[rank];
        PEAT_Event_Delivery delivery;
        delivery.buffer = tbuf;
        int tag = PEAT_RT_Bind_Completion;
        if (phase.func_id == MPI_SEND_INIT_ID || phase.func_id == MPI_RECV_INIT_ID) {
            tag = PEAT_RT_Bind_Initiation;
        }

        if (bind.func_id == MPI_REQUEST_FREE_ID || bind.func_id == MPI_CANCEL_ID) {
            tag = PEAT_RT_Bind_Destroying;
        }
        sprintf(&tbuf[4], "%d:%s:%s:%s:%s\n", tag,
                global_func_id.name_by_idx(phase.func_id),
                peat_ids_string(phase.ids).c_str(),
                global_func_id.name_by_idx(bind.func_id),
                peat_ids_string(bind.ids).c_str());
        delivery.receiver = mat_peer;
        delivery.func = phase.func_id;
        PEAT_Signed_Num size = (PEAT_Signed_Num)strlen(&tbuf[4]) + 5;
        MPI_Isend(delivery.buffer, size, MPI_CHAR, mat_peer, TRACTOR_REQ_PRINT_STRING_TAG, mat_compute_comm, &delivery.request);
        active_deliveries->push_back(delivery);
#endif
    } else {
        // gzFile Bind = output_bind->get_file_ptr(rank);

        // output_bind->printf(rank, "%d:%llx:%lld:%lld:%llx:%lld\n", PEAT_RT_Bind_Completion, req, primary_id, secondary_id, bind_primary_id, bind_secondary_id);

        int tag = PEAT_RT_Bind_Completion;
        if (global_func_id.check_property(phase.func_id,PEAT_PROP_PERSISTENT_REQUEST_INIT)) {
            tag = PEAT_RT_Bind_Initiation;
        }

        if (bind.func_id == MPI_REQUEST_FREE_ID || bind.func_id == MPI_CANCEL_ID) {
            tag = PEAT_RT_Bind_Destroying;
        }
        output_bind->printf(rank, "%d:%s:%s:%s:%s\n", tag,
                            global_func_id.name_by_idx(phase.func_id),
                            peat_ids_string(phase.ids).c_str(),
                            global_func_id.name_by_idx(bind.func_id),
                            peat_ids_string(bind.ids).c_str());
        p2p_func_store->messages_bound[rank]++;
    }
}

void Request::print_req_bind(PEAT_Rank rank, const Collective_Entry &coll, const Phase &phase, bool is_remote)
{
    if (is_remote) {
#ifdef PEAT_MPI // in case of serial is_remote is always false
        char *tbuf = (char *)peat_malloc_aligned(1024);
        int *rnk = (int *)tbuf;
        *rnk = rank; // TODO need real rank
        PEAT_Rank mat_peer = whole_ranks[rank];
        PEAT_Event_Delivery delivery;
        delivery.buffer = tbuf;
        delivery.receiver = mat_peer;
        delivery.func = coll.func_id;
        sprintf(&tbuf[4], "%d:%s:%s:%s:%s\n", PEAT_RT_Bind_Completion,
                global_func_id.name_by_idx(coll.func_id),
                peat_ids_string(coll.coll_events_list.at(rank).ids).c_str(),
                global_func_id.name_by_idx(phase.func_id),
                peat_ids_string(phase.ids).c_str());
        PEAT_Signed_Num size = (PEAT_Signed_Num)strlen(&tbuf[4]) + 5;
        MPI_Isend(delivery.buffer, size, MPI_CHAR, mat_peer, TRACTOR_REQ_PRINT_STRING_TAG, mat_compute_comm, &delivery.request);
        active_deliveries->push_back(delivery);
#endif
    } else {
        output_bind->printf(rank, "%d:%s:%s:%s:%s\n", PEAT_RT_Bind_Completion,
                            global_func_id.name_by_idx(coll.func_id),
                            peat_ids_string(coll.coll_events_list.at(rank).ids).c_str(),
                            global_func_id.name_by_idx(phase.func_id),
                            peat_ids_string(phase.ids).c_str());
        p2p_func_store->messages_bound[rank]++;
    }
}

void Request::print_req_out(const Tractor_Event &event)
{
    if (event.Requests.flag == 0 && skip_locals)
        return; // skip missed Tests
#ifdef MAT_PROCESS_HWC_MPI_OUTPUT
    output_MPI->printf(event.rank, "%s:%s:%d:%d:%d", event.header_str().c_str(), global_func_id.name_by_idx(event.func_id),
                       event.Requests.input_request_num,
                       event.Requests.triggered_request_num,
                       event.Requests.flag);
    if (!event.nested)
        output_MPI->printf(event.rank, ":%d", event.num_children); // legacy num_colts
    output_MPI->printf(event.rank, ":%llx", event.Requests.buf);
    for (int i = 0; i < event.Requests.input_request_num; i++) {
        output_MPI->printf(event.rank, ":%llx", event.requests[i]);
    }
    // 4 - tuples(index, tag, source, bytes)
    for (int i = 0; i < (int)event.Requests.triggered_request_num; i++) {
        output_MPI->printf(event.rank, ":%d:%d:%d:%lld", event.statuses[i].index, event.statuses[i].tag, event.statuses[i].src, event.statuses[i].bytes);
    }
    output_MPI->printf(event.rank, "\n");
#if 0
    output_HWC->printf(event.rank, "%s:%lld:%lld:%lld:%lld:%lld:%lld", event.hwc_header_str().c_str(),
        event.tsc_time.func.in,
        event.tsc_time.mpi.in,
        event.tsc_time.mpi.out,
        event.tsc_time.func.out,
        event.tsc_time.pe.in,
        event.tsc_time.pe.out);

#else
    output_HWC->printf(event.rank, "%s:%lld:%lld:%lld:%lld", event.hwc_header_str().c_str(),
                       event.tsc_time.mpi.in,
                       event.tsc_time.mpi.out,
                       event.tsc_time.pe.in,
                       event.tsc_time.pe.out);
#endif
    for (PEAT_Num i = 0; i < event.addrs.size(); i++) {
        output_HWC->printf(event.rank, ":%lld", event.addrs.at(i));
    }
    output_HWC->printf(event.rank, "\n");
#endif
}

void Request::parse_P2P_start_phase(const PEAT_Rank rank, const Phase& phase, const PEAT_MPI_Request request, MAT_Request_Store_Record* ret_it, const int pc_level,  bool is_remote)
{
    MAT_P2P_Record* req_it=static_cast<MAT_P2P_Record*> (ret_it->record);

    MAT_P2P_Record* record = new MAT_P2P_Record(*req_it);
    record->ph[1] = phase;
    record->pc_level = pc_level;
    MAT_Request_Store_Record req;
	req.record = record;
	req.request_type = REQUEST_TYPE_USER_P2P;
    req_store->add_request(rank, request, &req);
    print_req_bind(rank, request, req_it->get_ph(0), phase, is_remote);
}

void Request::parse_collective_start_phase(const PEAT_Rank rank, const Phase& phase, const PEAT_MPI_Request request, MAT_Request_Store_Record* ret_it, const int pc_level, bool is_remote)
{
    MAT_Collectives_Record* req_it = static_cast<MAT_Collectives_Record*> (ret_it->record);

    MAT_Collectives_Record* record = new MAT_Collectives_Record(*req_it);
    record->coll_event->ph[1] = phase;
    record->pc_level = pc_level;
    MAT_Request_Store_Record req;
    req.record = record;
    req.request_type = REQUEST_TYPE_USER_COLLECTIVE;
    req_store->add_request(rank, request, &req);
    print_req_bind(rank, request, req_it->get_ph(0), phase, is_remote);
}

#ifdef PEAT_MPI
void Request::process_remote_request(PEAT_Signed_Num size, PEAT_Rank src)
{
#if defined(_WIN32) && !defined(__MINGW32__)
    char *buffer = (char *)_malloca(size);
#else
    char buffer[size];
#endif
    MPI_Status status;
    Tractor_Event temp_req_event;
    MPI_Recv(buffer, size, MPI_CHAR, src, TRACTOR_REQUEST_TAG, mat_compute_comm, &status);
    temp_req_event.unpack(buffer);
    parse_cn(temp_req_event.rank, temp_req_event, true);
    return;
}
PEAT_Num Request::check_completions()
{
    if (!active_deliveries->empty()) {
        for (std::list<PEAT_Event_Delivery>::iterator it = active_deliveries->begin(); it != active_deliveries->end();) {
            MPI_Status status;
            int flag = 0;
            MPI_Test(&it->request, &flag, &status);
            if (flag > 0) { // request completed
                assert(it->request == MPI_REQUEST_NULL);
                peat_free(it->buffer);
                it = active_deliveries->erase(it);
                continue;
            }
            ++it;
        }
        return active_deliveries->size();
    }
    return 0;
}
void Request::send_cn(const Tractor_Event &req_event, PEAT_Rank peer)
{
    PEAT_Event_Delivery delivery;
    PEAT_Signed_Num size = (PEAT_Signed_Num)req_event.pack_size();
    delivery.buffer = (char *)peat_malloc_aligned(size);
    delivery.func = req_event.func_id;
    req_event.pack(delivery.buffer);
    PEAT_Rank mat_peer = whole_ranks[peer];
    delivery.receiver = mat_peer;
    MPI_Isend(delivery.buffer, size, MPI_CHAR, mat_peer, TRACTOR_REQUEST_TAG, mat_compute_comm, &delivery.request);
    active_deliveries->push_back(delivery);
}

/*
void Request::send_P2P_cn(const Tractor_Request_Event& req_event) {
    PEAT_Event_Delivery delivery;
    PEAT_Signed_Num size = (PEAT_Signed_Num)req_event.pack_size();
    delivery.buffer = (char *)peat_malloc_aligned(size);
    req_event.pack(delivery.buffer);
    PEAT_Rank mat_peer = whole_ranks[req_event.peer];
    MPI_Isend(delivery.buffer, size, MPI_CHAR, mat_peer, TRACTOR_REQUEST_TAG, mat_compute_comm, &delivery.request);
    active_deliveries.push_back(delivery);
}

void Request::send_coll_cn(const Tractor_Request_Event& req_event) {
    PEAT_Event_Delivery delivery;
    PEAT_Signed_Num size = (PEAT_Signed_Num)req_event.pack_size();
    delivery.buffer = (char *)peat_malloc_aligned(size);
    req_event.pack(delivery.buffer);
    PEAT_Rank mat_peer = whole_ranks[req_event.peer];
    MPI_Isend(delivery.buffer, size, MPI_CHAR, mat_peer, TRACTOR_REQUEST_TAG, mat_compute_comm, &delivery.request);
    active_deliveries.push_back(delivery);
}
*/
#endif
