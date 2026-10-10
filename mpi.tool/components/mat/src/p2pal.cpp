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

#include "p2pal.h"
#include "requests.h"
#include "utils.h"

#include <assert.h>
#include <sstream>

P2PAL::P2PAL(File_Storage_GZ *f, int s, int pos,
#ifdef PEAT_MPI
             std::list<PEAT_Event_Delivery> *ad, const std::vector<PEAT_Rank> &wR, const std::vector<PEAT_Rank> &mRs, const MPI_Comm mat_cc, const PEAT_Rank m_r,
#endif
             int v, const std::string &bl, const char dl) :
#ifdef PEAT_MPI
                                                            active_deliveries(ad), whole_ranks(wR), my_ranks(mRs), mat_compute_comm(mat_cc), my_rank(m_r),
#endif
                                                            skip_locals(s), print_on_send(pos), verbose(v) //, adaptive_blocking_threshold(INITIAL_ABF)

{
    q_name_s[qt_send] = "Send";
    q_name_s[qt_recv] = "Recv";
    comm_store = NULL;
    input_MPI = f;
    ptp = NULL;
    req_store = NULL;
    request_func_store = NULL;
    gpu = NULL;
}

void P2PAL::initialize(PTP *p, Communicators *c, Request_Store *rs, Request *r, GPU *g, File_Storage_GZ *MPI, File_Storage_GZ *HWC, File_Storage_GZ *bind)
{
    assert(c);
    assert(p);
    assert(g);
    comm_store = c;
    ptp = p;
    req_store = rs;
    request_func_store = r;
    gpu = g;
    messages_bound.resize(ptp->get_rank_num());
    Outputs::initialize(MPI, HWC, bind);
}

P2PAL::~P2PAL()
{
    ptp = 0;
    comm_store = 0;
}

void P2PAL::print_P2PL(const Tractor_Event &event)
{
#ifdef MAT_PROCESS_HWC_MPI_OUTPUT
    output_MPI->printf(event.rank, "%s:%s:%x:%d:%llx:%llx:%d:%lld\n", event.header_str().c_str(), global_func_id.name_by_idx(event.func_id),
                       event.P2P.comm, event.P2P.peer, event.P2P.request, event.P2P.message, event.P2P.tag, event.P2P.transfered_bytes);

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
void P2PAL::print_P2PE(const MAT_P2P_Record &event)
{
    // gzFile Bind = output_bind->get_file_ptr(rec.rank);
    fprintf(stderr, "%d:%s:%s:%s:%s:%s:%s\n",
            event.rank,
            global_func_id.name_by_idx(event.ph[0].func_id),
            peat_ids_string(event.ph[0].ids).c_str(),
            global_func_id.name_by_idx(event.ph[1].func_id),
            peat_ids_string(event.ph[1].ids).c_str(),
            global_func_id.name_by_idx(event.ph[2].func_id),
            peat_ids_string(event.ph[2].ids).c_str());
}

#if _DEBUG
static long long max_dif = 0;
#endif
void P2PAL::add_imbalance(const MAT_P2P_Record &left, const MAT_P2P_Record &right, PEAT_Single_Event_Clock &left_clock, PEAT_Single_Event_Clock &right_clock)
{
    PEAT_Clock left_in = left.ph[1].tsc_time.func.in;
    PEAT_Clock left_out = left.ph[2].tsc_time.func.out; 
    PEAT_Clock right_in = right.ph[1].tsc_time.func.in; 
    PEAT_Clock right_out = right.ph[2].tsc_time.func.out;   
    if (left_out < right_in) return; // most likely eager send, so no imbalance    
    if (right_out < left_in) return; // technically error but sometime happens can't do anything here
    if (left_in < right_in) {
        PEAT_Clock diff = right_in - left_in;
        left_clock.imb_c += diff;
    } else {
        PEAT_Clock diff = left_in - right_in;
        right_clock.imb_c += diff;
    }
}
#define PRINT_STAT
void P2PAL::print_P2PF(const MAT_P2P_Record &left, const MAT_P2P_Record &right)
{
#if _DEBUG
    if (right.ph[2].tsc_time.func.out < left.ph[1].tsc_time.func.in) {
        long long diff = left.ph[1].tsc_time.func.in - right.ph[2].tsc_time.func.out;
        if (max_dif < diff) { // less recording
            std::cerr << "Receive ended earlier than Transmit (tsc diff:" << std::dec << diff << ") {\nSend(host:" << ptp->get_host_name(left.rank) << "): " << left << "\nRecv(host:" << ptp->get_host_name(right.rank) << "): " << right << "\n}" << std::endl;
            max_dif = diff;
        }
    }
#endif
#ifdef PEAT_MPI
    char *tbuf = (char *)peat_malloc_aligned(1024);
#else
    char tbuf[1024];
#endif
    // phase[1] (post: Isend/Irecv itself) for both sides - what pet registers events under. The receiver's
    // completion (phase[2]: Wait/Test/Request_Release/...) is NOT duplicated here: it's already written as a
    // same-rank PEAT_RT_Bind_Completion line (post -> completion), which always precedes this line in the
    // same file, so qTV chases that instead of carrying a second copy of it in every peer line.
    sprintf(&tbuf[4], "%d:%s:%d:%s:%s:%d:%s:%lld\n", PEAT_RT_Bind_Peer,
            global_func_id.name_by_idx(left.ph[1].func_id), left.rank, peat_ids_string(left.ph[1].ids).c_str(),
            global_func_id.name_by_idx(right.ph[1].func_id), right.rank, peat_ids_string(right.ph[1].ids).c_str(),
            left.size);
    // assert(left.size == right.size);
    if (print_on_send) {
#ifdef PEAT_MPI
        int *rnk = (int *)tbuf;
        *rnk = left.rank;
        PEAT_Rank mat_peer = whole_ranks[left.rank];
        PEAT_Event_Delivery delivery;
        PEAT_Signed_Num size = (PEAT_Signed_Num)strlen(&tbuf[4]) + 5;
        delivery.buffer = tbuf;
        delivery.receiver = mat_peer;
        delivery.func = TMPI_FUNC_ID_NONE;
        MPI_Isend(delivery.buffer, size, MPI_CHAR, mat_peer, TRACTOR_P2P_PRINT_STRING_TAG, mat_compute_comm, &delivery.request);
        active_deliveries->push_back(delivery);
#else
        output_bind->puts(left.rank, &tbuf[4]);
#endif
        messages_bound[left.rank]++;
    } else {
        messages_bound[right.rank]++;
        output_bind->puts(right.rank, &tbuf[4]);
    }
#ifdef PRINT_STAT
    if (output_HWC && output_HWC->get_number_of_files() > 0) { // no timing data to compute stats/imbalance from when hwc files aren't read
        int pc_level = left.pc_level;
        PEAT_P2P_Record r_left;
        r_left.func_id[0] = left.ph[0].func_id;
        r_left.func_id[1] = left.ph[1].func_id;
        r_left.func_id[2] = left.ph[2].func_id;
        r_left.bytes = left.size;
        r_left.peer = left.peer_rank;
        r_left.rank = left.rank;
        PEAT_Single_Event_Clock &left_clock = printed_p2p[pc_level][r_left];
        PEAT_Clock duration = left.ph[2].tsc_time.mpi.out - left.ph[1].tsc_time.mpi.in;
        assert(duration > 0);
        left_clock.total_c += duration;
        left_clock.occ++;
        if (left_clock.min_c == 0 || left_clock.min_c > duration)
            left_clock.min_c = duration;
        if (left_clock.max_c < duration)
            left_clock.max_c = duration;
        // printed_p2p[pc_level][r_left] = left_clock;
        pc_level = right.pc_level;
        PEAT_P2P_Record r_right;
        r_right.func_id[0] = right.ph[0].func_id;
        r_right.func_id[1] = right.ph[1].func_id;
        r_right.func_id[2] = right.ph[2].func_id;
        r_right.bytes = right.size;
        r_right.peer = right.peer_rank;
        r_right.rank = right.rank;

        PEAT_Single_Event_Clock &right_clock = printed_p2p[pc_level][r_right];
        duration = right.ph[2].tsc_time.mpi.out - right.ph[1].tsc_time.mpi.in;
        assert(duration > 0);
        right_clock.total_c += duration;
        right_clock.occ++;
        if (right_clock.min_c == 0 || right_clock.min_c > duration)
            right_clock.min_c = duration;
        if (right_clock.max_c < duration)
            right_clock.max_c = duration;
        // printed_p2p[pc_level][r_right] = right_clock;
        add_imbalance(left, right, left_clock,right_clock);
    }

    {
        if (left.comm != right.comm) {
            fprintf(stderr, "p2p. Ranks:%d,%d ids:%lld,%lld,%lld (%lld) => (%lld) %lld,%lld,%lld\n", left.rank, right.rank,
                    left.ph[0].ids[0], left.ph[1].ids[0], left.ph[2].ids[0],
                    left.size, right.size,
                    right.ph[0].ids[0], right.ph[1].ids[0], right.ph[2].ids[0]);
            peat_throw_error("Communicators mismatch\n");
        }
        if (left.tag != right.tag) {
            fprintf(stderr, "p2p. Ranks:%d,%d ids:%lld,%lld,%lld (%lld) => (%lld) %lld,%lld,%lld\n", left.rank, right.rank,
                    left.ph[0].ids[0], left.ph[1].ids[0], left.ph[2].ids[0],
                    left.size, right.size,
                    right.ph[0].ids[0], right.ph[1].ids[0], right.ph[2].ids[0]);
            peat_throw_error("Tags mismatch\n");
        }
        if (left.size != right.size) {
            fprintf(stderr, "p2p. Ranks:%d,%d ids:%lld,%lld,%lld (%lld) => (%lld) %lld,%lld,%lld\n", left.rank, right.rank,
                    left.ph[0].ids[0], left.ph[1].ids[0], left.ph[2].ids[0],
                    left.size, right.size,
                    right.ph[0].ids[0], right.ph[1].ids[0], right.ph[2].ids[0]);
            peat_warning("Sizes mismatch\n");
        }
        if (left.ph[0].func_id == right.ph[0].func_id && !global_func_id.check_property(right.ph[0].func_id, PEAT_PROP_DATA_TXRX)) {
            peat_throw_error("Same functions used for send and receive\n");
        }
    }
#endif
    /*
        fprintf(stderr, "p2p. Ranks:%d,%d ids:%lld,%lld,%lld (%lld) => (%lld) %lld,%lld,%lld\n", left.rank, right.rank,
            left.ph[0].ids[0], left.ph[1].ids[0], left.ph[2].ids[0],z
            left.size, right.size,
            right.ph[0].ids[0], right.ph[1].ids[0], right.ph[2].ids[0]);
    */
}

void P2PAL::print_Local(const Tractor_Event &event)
{
    // cerr << "Local. Rank:" << local_record.rank << ". uniq_id:" << local_record.ph[0].uniq_id << endl;
    if (skip_locals)
        return;
#ifdef MAT_PROCESS_HWC_MPI_OUTPUT
    if (event.landbuf.buf_size >= 0 || event.landbuf.buf_count >= 0) {
        output_MPI->printf(event.rank, "%s:%s:%lld:%lld\n", event.header_str().c_str(), global_func_id.name_by_idx(event.func_id), event.landbuf.buf_count, event.landbuf.buf_size);
    } else {
        output_MPI->printf(event.rank, "%s:%s\n", event.header_str().c_str(), global_func_id.name_by_idx(event.func_id));
    }
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

bool P2PAL::match_peer(matching_event_type &queues, std::list<MAT_P2P_Record *>::iterator &ret_it, const MAT_P2P_Record &input_record) const
{
    const PEAT_MPI_Comm comm = input_record.comm;
    const PEAT_Rank peer_rank = input_record.peer_rank;
    const PEAT_Rank rank = input_record.rank;
    const PEAT_MPI_Tag tag = input_record.tag;
    const PEAT_MPI_Message message = input_record.message;
    const PEAT_MPI_Request req = input_record.request;
    if (req > 0 && !input_record.is_send) {
        return false;
    }

    if (peer_rank == comm_store->get_MPI_PROC_NULL()) {
        return false;
    }
    if (peer_rank == comm_store->get_MPI_ANY_SOURCE()) {
        assert(0);
        return false;
    }
    if (tag == comm_store->get_MPI_ANY_TAG()) {
        assert(0);
        return false;
    }
    if (queues.empty()) {
        return false;
    }
    P2P_Event_Key key(tag, rank, message, comm);

    std::list<MAT_P2P_Record *> &queue = queues[peer_rank][key];
    if (!queue.empty()) {
        std::list<MAT_P2P_Record *>::iterator it = queues[peer_rank][key].begin();
        if (it != queues[peer_rank][key].end()) {
            // sanity check
            ret_it = it;
            if ((*ret_it)->comm == comm && (*ret_it)->rank == peer_rank && (*ret_it)->tag == tag && (*ret_it)->peer_rank == rank) {
                assert(!(*ret_it)->peer_assigned);
                return true;
            } else {
                assert(0);
                return false;
            }
        }
    }
    return false;
}


bool P2PAL::update_request(const PEAT_Rank rank, const Request_Store_Record_Type type, const PEAT_MPI_Tag tag, const PEAT_Rank source, const PEAT_Message_Size size, PEAT_MPI_Request request, const Phase &phase, bool is_remote)
{
	MAT_P2P_Record* record = NULL;
    MAT_Request_Store_Record ret_it;
    if (request > 0) {
		if (req_store->find_request(rank, request, REQUEST_TYPE_USER_P2P, &ret_it, true)) { //find and delete the request from the store, as it's being completed
            record = static_cast<MAT_P2P_Record*>(ret_it.record);
        }   
    }

    if (record) {
        if (record->peer_rank == comm_store->get_MPI_PROC_NULL()) {
            assert(0);
            return true;
        }
        if (!record->is_send) {
            if (record->tag == comm_store->get_MPI_ANY_TAG()) { // any tag
                record->tag = tag;
            } else {
                if (record->tag != tag) {
                    //                                        cerr << "Tags mismatch: " << it->tag << ":" << tag << " ignored." << endl;
                }
            }
            if (record->peer_rank == comm_store->get_MPI_ANY_SOURCE()) { // any source
                record->peer_rank = source;                             // comm_store->get_rank_from_comm(it->comm, rank, source, it->ph[1].tsc_u_time.func.in);
            } else {
                if (record->peer_rank != source) {
                    //                                        cerr << "Ranks mismatch " << it->peer_rank << ":" << source << " ignored." << endl;
                }
            }
            if (size >= 0)
                record->size = size;
        }
        record->ph[2] = phase;
        request_func_store->print_req_bind(rank, request, record->ph[1], record->ph[2], is_remote);

        PEAT_Num queue_type = record->is_send ? qt_send : qt_recv;
        PEAT_Num peer_queue_type = record->is_send ? qt_recv : qt_send;
        P2P_Event_Key key(*record);
        std::list<MAT_P2P_Record *>::iterator req_it = matching_queues[queue_type][rank][key].begin();
        std::list<MAT_P2P_Record *>::iterator peer_it;
        if (req_it != matching_queues[queue_type][rank][key].end()) {
            if (match_peer(matching_queues[peer_queue_type], peer_it, **req_it)) {
                std::list<MAT_P2P_Record *>::iterator lP = ((*req_it)->is_send) ? req_it : peer_it;
                std::list<MAT_P2P_Record *>::iterator rP = ((*req_it)->is_send) ? peer_it : req_it;
                MAT_P2P_Record *lpr = *lP;
                MAT_P2P_Record *rpr = *rP;
                print_P2PF(*lpr, *rpr);
                lpr->peer_assigned = rpr->peer_assigned = true;
                P2P_Event_Key send_key(*lpr), recv_key(*rpr);
                matching_queues[qt_send][lpr->rank][send_key].erase(lP);
                matching_queues[qt_recv][rpr->rank][recv_key].erase(rP);
                if (lpr->request == 0) {
                    delete (lpr);
                }
                if (rpr->request == 0) {
                    delete (rpr);
                }
            }
        }
        return true;
    }
    return false;
}

void P2PAL::write_HNT() const
{
    std::ostringstream tmp;
    tmp << "FREQ" << ":" << ptp->get_trace_freq(-1) << "\n";
}
bool P2PAL::empty() const
{
#if 0
    for(PEAT_Num i = qt_send; i < qt_size; ++i){
        if(!p2pal_queues[i].empty()) {
            for(queues_type::const_iterator it = p2pal_queues[i].begin(); it != p2pal_queues[i].end(); ++it) {
                if(!it->second.empty()) {
                    return false;
                }
            }
        }
    }
    return true;
#else
    return false;
#endif
}

bool P2PAL::add_pers_req_start(const PEAT_Rank rank, MAT_P2P_Record *record)
{
    if (record->peer_rank == comm_store->get_MPI_PROC_NULL()) {
        return false;
    }
    P2P_Event_Key key(*record);
    if (global_func_id.check_property(record->ph[0].func_id, PEAT_PROP_DATA_RX)) {
        matching_queues[qt_recv][rank][key].push_back(record);
    } else if (global_func_id.check_property(record->ph[0].func_id, PEAT_PROP_DATA_TX)) {
        matching_queues[qt_send][rank][key].push_back(record);
    } else {
        std::cerr << "Weird persistent request." << std::endl;
        return false;
    }
    MAT_Request_Store_Record tmp;
	tmp.request_type = REQUEST_TYPE_USER_P2P;
	tmp.record = record;
	req_store->add_request(rank, record->request, &tmp);
    return true;
}
void P2PAL::parse_P2PTx(PEAT_Rank rank, const Tractor_Event &event, bool print_out)
{
    assert(ptp);
    assert(comm_store);
    const PEAT_MPI_Comm comm = !global_func_id.check_property(event.func_id, PEAT_PROP_COMM) ? PEAT_MPI_Comm(2) : event.P2P.comm;
    if (print_out)
        ptp->add_rank_MPI(rank, event.tsc_time.func.out - event.tsc_time.func.in);
    if (event.P2P.peer != comm_store->get_MPI_PROC_NULL()) {
        MAT_P2P_Record *record = new (MAT_P2P_Record);
        record->pc_level = event.PControl.ctl;
        record->comm = comm;
        record->rank = rank;
        record->peer_rank = event.P2P.peer;
        record->tag = event.P2P.tag;
        record->size = event.P2P.transfered_bytes;
        record->is_send = true;
        if (event.P2P.request) {
            record->request = event.P2P.request;
        }
        Phase ph_tx(event);
        record->ph[0] = record->ph[1] = record->ph[2] = ph_tx;
        if (print_out)
            print_P2PL(event);
        if (global_func_id.check_property(event.func_id, PEAT_PROP_PERSISTENT_REQUEST_INIT)) { // add persistent request to queue and finish
            MAT_Request_Store_Record tmp;
            tmp.request_type = REQUEST_TYPE_PERSISTENT;
			tmp.record = record;
			req_store->add_request(rank, record->request, &tmp);
            return;
        }
        if(record->request) {
            MAT_Request_Store_Record tmp;
            tmp.request_type = REQUEST_TYPE_USER_P2P;
            tmp.record = record;
            req_store->add_request(rank, record->request, &tmp);
        }
        P2P_Event_Key key(*record);
        matching_queues[qt_send][rank][key].push_back(record);
        std::list<MAT_P2P_Record *>::iterator matching_rec_it;
        std::list<MAT_P2P_Record *>::iterator record_it = matching_queues[qt_send][rank][key].begin();
        if (record_it != matching_queues[qt_send][rank][key].end()) {
            if (match_peer(matching_queues[qt_recv], matching_rec_it, **record_it)) {
                MAT_P2P_Record *lP = *record_it;
                MAT_P2P_Record *rP = *matching_rec_it;
                P2P_Event_Key send_key(*lP);
                P2P_Event_Key recv_key(*rP);
                lP->peer_assigned = rP->peer_assigned = true;
                print_P2PF(*lP, *rP);
                PEAT_Rank send_rank = lP->rank;
                PEAT_Rank recv_rank = rP->rank;

                matching_queues[qt_send][send_rank][send_key].erase(record_it);
                matching_queues[qt_recv][recv_rank][recv_key].erase(matching_rec_it);
                if (lP->request == 0) {
                    delete lP;
                }
                if (rP->request == 0) {
                    delete rP;
                }
            }
        }
    } else {
        if (print_out)
            print_P2PL(event);
#ifdef PEAT_MPI
        if (event.P2P.request > 0) {
            if (global_func_id.check_property(event.func_id, PEAT_PROP_PERSISTENT_REQUEST_INIT)) {
                if (req_store->thrown_away_pers_requests[rank].find(event.P2P.request) != req_store->thrown_away_pers_requests[rank].end()) {
                    std::cerr << "Persistent request " << std::hex << event.P2P.request << std::dec << " conflict.Rank: " << rank << ". primary_id: " << event.ids[0] << " ids: " << peat_ids_string(event.ids) << std::endl;
                };
                req_store->thrown_away_pers_requests[rank].insert(event.P2P.request);
                std::cerr << "Invalid persistent request " << std::hex << event.P2P.request << std::dec << " thrown away.Rank: " << rank << ". primary_id: " << event.ids[0] << " ids: " << peat_ids_string(event.ids) << std::endl;
            } else {
                if (req_store->thrown_away_requests[rank].find(event.P2P.request) != req_store->thrown_away_requests[rank].end()) {
                    std::cerr << "Request " << std::hex << event.P2P.request << std::dec << " conflict.Rank: " << rank << ". primary_id: " << event.ids[0] << " ids: " << peat_ids_string(event.ids) << std::endl;
                }
                req_store->thrown_away_requests[rank].insert(event.P2P.request);
                std::cerr << "Invalid request " << std::hex << event.P2P.request << std::dec << " thrown away.Rank: " << rank << ". primary_id: " << event.ids[0] << " ids: " << peat_ids_string(event.ids) << std::endl;
            }
        }
#endif
    }
}
void P2PAL::parse_P2PRx(PEAT_Rank rank, const Tractor_Event &event, bool increase_MPI)
{
    assert(ptp);
    assert(comm_store);
    PEAT_MPI_Comm comm = !global_func_id.check_property(event.func_id, PEAT_PROP_COMM) ? 2 : event.P2P.comm;
    if (increase_MPI)
        ptp->add_rank_MPI(rank, event.tsc_time.func.out - event.tsc_time.func.in);
    PEAT_MPI_Tag tag = event.P2P.tag;
    PEAT_Rank peer = event.P2P.peer;
	PEAT_Message_Size bytes = event.P2P.transfered_bytes;
    if (global_func_id.check_property(event.func_id, PEAT_PROP_MESSAGE)) {
        /* restore matching information from message*/
		PEAT_MPI_Translation_Message message = matching_messages.at(rank).at(event.P2P.message);
		tag = message.tag;
		peer = message.peer;    
		comm = message.comm;
		bytes = message.bytes;
    }
    if (event.P2P.peer != comm_store->get_MPI_PROC_NULL()) {
        MAT_P2P_Record *record = new (MAT_P2P_Record);
        record->pc_level = event.PControl.ctl;
        record->comm = comm;
        record->rank = rank;
        record->peer_rank = peer;
        record->tag = tag;
        record->size = bytes;
        record->is_send = false;
        if (event.P2P.request) {
            record->request = event.P2P.request;
        }
        Phase ph_rx(event);
        record->ph[0] = record->ph[1] = record->ph[2] = ph_rx;
        print_P2PL(event);
        if(global_func_id.check_property(event.func_id, PEAT_PROP_PERSISTENT_REQUEST_INIT)) { // add persistent request to queue and finish
            MAT_Request_Store_Record tmp;
            tmp.request_type = REQUEST_TYPE_PERSISTENT;
            tmp.record = record;
            req_store->add_request(rank, record->request, &tmp);
            return;
        }
        if(record->request) {
            MAT_Request_Store_Record tmp;
            tmp.request_type = REQUEST_TYPE_USER_P2P;
            tmp.record = record;
            req_store->add_request(rank, record->request, &tmp);
        }

        P2P_Event_Key key(*record);
        matching_queues[qt_recv][rank][key].push_back(record);
        std::list<MAT_P2P_Record *>::iterator record_it = matching_queues[qt_recv][rank][key].begin();
        std::list<MAT_P2P_Record *>::iterator matching_rec_it;

        if (record_it != matching_queues[qt_recv][rank][key].end()) {
            if (match_peer(matching_queues[qt_send], matching_rec_it, **record_it)) {
                MAT_P2P_Record *lP = *matching_rec_it;
                MAT_P2P_Record *rP = *record_it;
                P2P_Event_Key send_key(*lP);
                P2P_Event_Key recv_key(*rP);
                print_P2PF(*lP, *rP);
                PEAT_Rank send_rank = lP->rank;
                PEAT_Rank recv_rank = rP->rank;
                lP->peer_assigned = rP->peer_assigned = true;
                matching_queues[qt_send][send_rank][send_key].erase(matching_rec_it);
                matching_queues[qt_recv][recv_rank][recv_key].erase(record_it);
                if(lP->request == 0) {
                    delete lP;
                }
                if(rP->request == 0) {
                    delete rP;
                }
            }
        }
    } else {
        print_P2PL(event);
#ifdef PEAT_MPI
        if (event.P2P.request > 0) {
            if (global_func_id.check_property(event.func_id, PEAT_PROP_PERSISTENT_REQUEST_INIT)) {
                if (req_store->thrown_away_pers_requests[rank].find(event.P2P.request) != req_store->thrown_away_pers_requests[rank].end()) {
                    std::cerr << "Persistent request " << std::hex << event.P2P.request << std::dec << " conflict.Rank: " << rank << ". primary_id: " << event.ids[0] << " ids: " << peat_ids_string(event.ids) << std::endl;
                };
                req_store->thrown_away_pers_requests[rank].insert(event.P2P.request);
                std::cerr << "Invalid persistent request " << std::hex << event.P2P.request << std::dec << " thrown away.Rank: " << rank << ". primary_id: " << event.ids[0] << " ids: " << peat_ids_string(event.ids) << std::endl;
            }
            else {
                if (req_store->thrown_away_requests[rank].find(event.P2P.request) != req_store->thrown_away_requests[rank].end()) {
                    std::cerr << "Request " << std::hex << event.P2P.request << std::dec << " conflict.Rank: " << rank << ". primary_id: " << event.ids[0] << " ids: " << peat_ids_string(event.ids) << std::endl;
                }
                req_store->thrown_away_requests[rank].insert(event.P2P.request);
                std::cerr << "Invalid request " << std::hex << event.P2P.request << std::dec << " thrown away.Rank: " << rank << ". primary_id: " << event.ids[0] << " ids: " << peat_ids_string(event.ids) << std::endl;
            }
        }
#endif
    }
}

void P2PAL::parse_Local(PEAT_Rank rank, const Tractor_Event &event)
{
    assert(ptp);
    assert(comm_store);
    if (!is_omp(event.func_class))
        ptp->add_rank_MPI(rank, event.tsc_time.func.out - event.tsc_time.func.in);
#if 0
    const PEAT_MPI_Comm comm = 0;// !global_func_id.prop_by_idx(event.func_id, PEAT_PROP_COMM) ? PEAT_MPI_Comm(Communicators::rc_mpi_comm_world) : comm_store->get_uniq_comm(event.Local.comm, rank, event.tsc_time.func.in);
    MAT_P2P_Record record;
    record.pc_level = ctl;
    record.comm = comm;
    record.rank = rank;
    record.size = 0;
    record.peer_rank = -1;
    record.tag = -1;
    Phase lPh;
    lPh.func_id = event.func_id;
    lPh.tid= event.tid;
    lPh.tsc_time = event.tsc_time;
    lPh.tsc_u_time = event.tsc_u_time;
    lPh.addr = event.addr;
    lPh.eaddr = event.eaddr;
    lPh.uniq_id = event.uniq_id;
    record.ph[0] = record.ph[1] = record.ph[2] = lPh;
#endif
    print_Local(event); // Record event to "local" file.
}

void P2PAL::parse_P2PPb(PEAT_Rank rank, const Tractor_Event &event)
{
    assert(ptp);
    assert(comm_store);
    ptp->add_rank_MPI(rank, event.tsc_time.func.out - event.tsc_time.func.in);
    /* Add message matching scheme*/
    if (global_func_id.check_property(event.func_id, PEAT_PROP_MESSAGE)) {
        matching_messages[event.rank][event.P2P.message] = PEAT_MPI_Translation_Message(event.P2P.tag, event.P2P.peer, event.P2P.comm, event.P2P.transfered_bytes);
	}   
#if 0
    const PEAT_MPI_Comm comm = !global_func_id.check_property(event.func_id, PEAT_PROP_COMM) ? 2 : event.P2P.comm;
    PEAT_MPI_Status flag = event.P2P.flag;
    MAT_P2P_Record record;
    record.pc_level = ctl;
    record.comm = comm;
    record.rank = rank;
    record.peer_rank = event.P2P.peer;//comm_store->get_rank_from_comm(comm, rank, event.P2P.peer, event.tsc_time.func.in);
    record.tag = event.P2P.tag;
    Phase ph_pb;
    ph_pb.func_id = event.func_id;
    ph_pb.tid = event.tid;
    ph_pb.tsc_time = event.tsc_time;
    ph_pb.tsc_u_time = event.tsc_u_time;
    ph_pb.tsc_u_time.func.in = event.tsc_time.func.in;
    ph_pb.tsc_u_time.func.out = event.tsc_time.func.out;
    ph_pb.addr = event.addr;
    ph_pb.eaddr = event.eaddr;
    ph_pb.uniq_id = event.uniq_id;
    record.ph[0] = record.ph[1] = record.ph[2] = ph_pb;
#endif
    print_P2PL(event);
}

#ifdef PEAT_MPI
void P2PAL::send_P2PTx(PEAT_Rank rank, const Tractor_Event &event)
{
    ptp->add_rank_MPI(rank, event.tsc_time.func.out - event.tsc_time.func.in);
    if (event.P2P.peer == comm_store->get_MPI_PROC_NULL()) {
        print_P2PL(event);
        // parse_Local(rank, event);
        if (event.P2P.request > 0) {
            if (global_func_id.check_property(event.func_id, PEAT_PROP_PERSISTENT_REQUEST_INIT)) {
                if (req_store->thrown_away_pers_requests[rank].find(event.P2P.request) != req_store->thrown_away_pers_requests[rank].end()) {
                    std::cerr << "Persistent request " << std::hex << event.P2P.request << std::dec << " conflict.Rank: " << rank << ". primary_id: " << event.ids[0] << " ids: " << peat_ids_string(event.ids) << std::endl;
                };
                req_store->thrown_away_pers_requests[rank].insert(event.P2P.request);
                std::cerr << "Invalid persistent request " << std::hex << event.P2P.request << std::dec << " thrown away.Rank: " << rank << ". primary_id: " << event.ids[0] << " ids: " << peat_ids_string(event.ids) << std::endl;
            } else {
                if (req_store->thrown_away_requests[rank].find(event.P2P.request) != req_store->thrown_away_requests[rank].end()) {
                    std::cerr << "Request conflict " << std::hex << event.P2P.request << std::dec << " conflict.Rank: " << rank << ". primary_id: " << event.ids[0] << " ids: " << peat_ids_string(event.ids) << std::endl;
                }
                req_store->thrown_away_requests[rank].insert(event.P2P.request);
                std::cerr << "Invalid request " << std::hex << event.P2P.request << std::dec << " thrown away.Rank: " << rank << ". primary_id: " << event.ids[0] << " ids: " << peat_ids_string(event.ids) << std::endl;
            }
        }
        return;
    }
    assert(event.P2P.peer >= 0);
    // const PEAT_MPI_Comm comm = !global_func_id.check_property(event.func_id, PEAT_PROP_COMM) ? 2 : event.P2P.comm;
    PEAT_Rank peer = event.P2P.peer; // comm_store->get_rank_from_comm(comm, rank, event.P2P.peer, event.tsc_in);
    PEAT_Rank mat_peer = whole_ranks[peer];
    print_P2PL(event);
#ifdef USE_LOCAL_PARSE
    if (mat_peer == my_rank) {
        parse_P2PTx(rank, event, false);
        return;
    }
#endif
    MAT_Request_Record record;
    record.peer = peer;
    record.ids = event.ids;

    /*
    record.ph2.func_id = event.func_id;
    record.ph2.thread_id = event.thread_id;
    record.ph2.tsc_time = event.tsc_time;
    record.ph2.tsc_u_time = event.tsc_u_time;
    record.ph2.addrs = event.addrs;
    record.ph2.ids = event.ids;
    */
    if (event.P2P.request > 0) {
        record.request = event.P2P.request;
        if (global_func_id.check_property(event.func_id, PEAT_PROP_PERSISTENT_REQUEST_INIT)) {
            req_store->flown_away_pers_requests[rank][record.request] = record;
        } else {
            req_store->flown_away_requests[rank][record.request] = record;
        }
    }
    PEAT_Event_Delivery delivery;
    PEAT_Signed_Num size = (PEAT_Signed_Num)event.pack_size();
    delivery.buffer = (char *)peat_malloc_aligned(size);
    delivery.receiver = mat_peer;
    delivery.func = event.func_id;
    event.pack(delivery.buffer);
    MPI_Isend(delivery.buffer, size, MPI_CHAR, mat_peer, TRACTOR_P2P_EVENT_TAG, mat_compute_comm, &delivery.request);
    active_deliveries->push_back(delivery);
}

PEAT_Num P2PAL::check_completions()
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
void P2PAL::process_remote_event(PEAT_Signed_Num size, PEAT_Rank src)
{
#if defined(_WIN32) && !defined(__MINGW32__)
    char *buffer = (char *)_malloca(size);
#else
    char buffer[size];
#endif
    MPI_Status status;
    MPI_Recv(buffer, size, MPI_CHAR, src, TRACTOR_P2P_EVENT_TAG, mat_compute_comm, &status);
    Tractor_Event temp_event;
    temp_event.unpack(buffer);
    parse_P2PTx(temp_event.rank, temp_event, false);
    // fprintf(stdout, "Rank %d: Received p2p (%lld,%d,%llx) from rank %d\n", my_rank, temp_event.ids[0], temp_event.P2P.peer, temp_event.P2P.request, temp_event.rank);
}
#endif

void P2PAL::process_unbound_events()
{
    for (matching_event_type::iterator rank_it = matching_queues[qt_send].begin(); rank_it != matching_queues[qt_send].end(); ++rank_it) {
        for (std::map<P2P_Event_Key, std::list<MAT_P2P_Record *>>::iterator send_it = rank_it->begin(); send_it != rank_it->end(); ++send_it) {
            for (std::list<MAT_P2P_Record *>::iterator event_it = send_it->second.begin(); event_it != send_it->second.end();) {
                std::list<MAT_P2P_Record *>::iterator matching_rec_it;
                if (match_peer(matching_queues[qt_recv], matching_rec_it, **event_it)) {
                    PEAT_Rank send_rank = (*event_it)->rank;
                    PEAT_Rank recv_rank = (*event_it)->peer_rank;
                    print_P2PF(**event_it, **matching_rec_it);
                    P2P_Event_Key send_key(**event_it), recv_key(**matching_rec_it);
                    (*event_it)->peer_assigned = (*matching_rec_it)->peer_assigned = true;
                    if ((*event_it)->request == 0) {
                        delete *event_it;
                    }
                    if ((*matching_rec_it)->request == 0) {
                        delete *matching_rec_it;
                    }
                    event_it = matching_queues[qt_send][send_rank][send_key].erase(event_it);
                    matching_queues[qt_recv][recv_rank][recv_key].erase(matching_rec_it);
                } else {
                    peat_throw_error("Unpaired Event Found\n");
                    print_P2PE(**event_it);
                    ++event_it;
                }
            }
        }
    }
    for (matching_event_type::iterator rank_it = matching_queues[qt_recv].begin(); rank_it != matching_queues[qt_recv].end(); ++rank_it) {
        for (std::map<P2P_Event_Key, std::list<MAT_P2P_Record *>>::iterator recv_it = rank_it->begin(); recv_it != rank_it->end(); ++recv_it) {
            for (std::list<MAT_P2P_Record *>::iterator event_it = recv_it->second.begin(); event_it != recv_it->second.end();) {
                std::list<MAT_P2P_Record *>::iterator matching_rec_it;
                if (match_peer(matching_queues[qt_send], matching_rec_it, **event_it)) {
                    PEAT_Rank send_rank = (*event_it)->peer_rank;
                    PEAT_Rank recv_rank = (*event_it)->rank;
                    print_P2PF(**matching_rec_it, **event_it);
                    (*event_it)->peer_assigned = (*matching_rec_it)->peer_assigned = true;
                    P2P_Event_Key send_key(**matching_rec_it), recv_key(**event_it);
                    if ((*event_it)->request == 0) {
                        delete *event_it;
                    }
                    if ((*matching_rec_it)->request == 0) {
                        delete *matching_rec_it;
                    }
                    event_it = matching_queues[qt_recv][recv_rank][recv_key].erase(event_it);
                    matching_queues[qt_send][send_rank][send_key].erase(matching_rec_it);
                } else {
                    peat_throw_error("Unpaired Event Found\n");
                    print_P2PE(**event_it);
                    ++event_it;
                }
            }
        }
    }
}
#ifdef PEAT_MPI
PEAT_Signed_Num P2PAL::check_unbound_events()
{
    int total_unpaired_events = 0;
    int hanged_events = 0;
    for (matching_event_type::iterator rank_it = matching_queues[qt_send].begin(); rank_it != matching_queues[qt_send].end(); ++rank_it) {
        for (std::map<P2P_Event_Key, std::list<MAT_P2P_Record *>>::iterator send_it = rank_it->begin(); send_it != rank_it->end(); ++send_it) {
            for (std::list<MAT_P2P_Record *>::iterator event_it = send_it->second.begin(); event_it != send_it->second.end();) {
                std::list<MAT_P2P_Record *>::iterator matching_rec_it;

                if (match_peer(matching_queues[qt_recv], matching_rec_it, **event_it)) {
                    PEAT_Rank send_rank = (*event_it)->rank;
                    PEAT_Rank recv_rank = (*event_it)->peer_rank;
                    print_P2PF(**event_it, **matching_rec_it);
                    (*event_it)->peer_assigned = (*matching_rec_it)->peer_assigned = true;
                    P2P_Event_Key send_key(**event_it), recv_key(**matching_rec_it);
                    if ((*event_it)->request > 0) {
                        hanged_events++; // indicates hanged request (send/recv without wait/test).
                    } else {
                        delete *event_it;
                    }
                    if ((*matching_rec_it)->request > 0) {
                        hanged_events++; // indicates hanged request (send/recv without wait/test).
                    } else {
                        delete *matching_rec_it;
                    }
                    event_it = matching_queues[qt_send][send_rank][send_key].erase(event_it);
                    matching_queues[qt_recv][recv_rank][recv_key].erase(matching_rec_it);
                } else {
                    // peat_throw_error("Unpaired Event Found\n");
                    // print_P2PE(**event_it);
                    total_unpaired_events++;
                    ++event_it;
                }
            }
        }
    }
    for (matching_event_type::iterator rank_it = matching_queues[qt_recv].begin(); rank_it != matching_queues[qt_recv].end(); ++rank_it) {
        for (std::map<P2P_Event_Key, std::list<MAT_P2P_Record *>>::iterator recv_it = rank_it->begin(); recv_it != rank_it->end(); ++recv_it) {
            for (std::list<MAT_P2P_Record *>::iterator event_it = recv_it->second.begin(); event_it != recv_it->second.end();) {
                std::list<MAT_P2P_Record *>::iterator matching_rec_it;
                if (match_peer(matching_queues[qt_send], matching_rec_it, **event_it)) {
                    PEAT_Rank send_rank = (*event_it)->peer_rank;
                    PEAT_Rank recv_rank = (*event_it)->rank;
                    print_P2PF(**matching_rec_it, **event_it);
                    P2P_Event_Key send_key(**matching_rec_it), recv_key(**event_it);
                    (*event_it)->peer_assigned = (*matching_rec_it)->peer_assigned = true;
                    if ((*event_it)->request > 0) {
                        hanged_events++; // indicates hanged request (send/recv without wait/test).
                    } else {
                        delete *event_it;
                    }
                    if ((*matching_rec_it)->request > 0) {
                        hanged_events++; // indicates hanged request (send/recv without wait/test).
                    } else {
                        delete *matching_rec_it;
                    }
                    event_it = matching_queues[qt_recv][recv_rank][recv_key].erase(event_it);
                    matching_queues[qt_send][send_rank][send_key].erase(matching_rec_it);
                } else {
                    // peat_throw_error("Unpaired Event Found\n");
                    // print_P2PE(**event_it);
                    ++event_it;
                    total_unpaired_events++;
                }
            }
            // hanged_events += p2pal_queues[qt_hanged][rank_it->first].size();
        }
    }
    if (total_unpaired_events > 0)
        return total_unpaired_events;
    if (hanged_events > 0)
        return -1;
    return 0;
}
#endif

void P2PAL::print_sizes() const
{
    printf("P2P Queues:");
    printf("\nMatching");
    for (int q = 0; q < qt_size; q++) {
        printf("\n%s", q_name_s[q].c_str());
        PEAT_Rank rank = 0;
        for (matching_event_type::const_iterator matching_queues_it = matching_queues[q].cbegin(); matching_queues_it != matching_queues[q].cend(); ++matching_queues_it, rank++) {
            size_t size = 0;
            for (std::map<P2P_Event_Key, std::list<MAT_P2P_Record *>>::const_iterator key_it = matching_queues_it->cbegin(); key_it != matching_queues_it->cend(); ++key_it) {
                size += key_it->second.size();
            }
            if (size > 0)
                printf(":r%d-%zd", rank, size);
        }
    }
    PEAT_Rank rank = 0;
    printf("\n");
}

std::ostream &operator<<(std::ostream &dbg, const P2PAL &p2p)
{
    dbg << "\nP2PAL conains:\n";
    for (PEAT_Num i = qt_send; i < qt_size; ++i) {
        dbg << "\n"
            << std::dec << p2p.q_name_s[i] << " queue " << p2p.matching_queues[i].size() << " elements " << p2p.matching_queues[i] << "\n";
    }
    return dbg;
}

std::ostream &operator<<(std::ostream &dbg, const P2PAL::matching_event_type &queue)
{
    for (P2PAL::matching_event_type::const_iterator rank_it = queue.cbegin(); rank_it != queue.cend(); ++rank_it) {
        for (std::map<P2PAL::P2P_Event_Key, std::list<MAT_P2P_Record *>>::const_iterator queue_it = rank_it->cbegin(); queue_it != rank_it->cend();) {
            for (std::list<MAT_P2P_Record *>::const_iterator event_it = queue_it->second.cbegin(); event_it != queue_it->second.cend();) {
                dbg << **event_it;
            }
        }
    }
    return dbg;
}

std::ostream &operator<<(std::ostream &dbg, const MAT_P2P_Record &rec)
{
    dbg << std::dec
        << "{r" << rec.rank
        << "(" << rec.peer_rank << ")"
        << ",s" << rec.size
        << ",t" << rec.tag
        << ",c" << rec.comm
        << ",oR" << rec.request
        << "," << rec.ph[0] << "," << rec.ph[1] << "," << rec.ph[2]
        << "}";
    return dbg;
}

std::ostream &operator<<(std::ostream &dbg, const Phase &ph)
{
    dbg << std::dec << "{" << global_func_id.name_by_idx(ph.func_id) << "," << peat_ids_string(ph.ids) << "," << ph.tsc_time.func.in << "," << ph.tsc_time.func.out << "}";
    return dbg;
}

std::ostream &operator<<(std::ostream &dbg, const std::map<PEAT_Rank, std::list<MAT_P2P_Record>> &rec)
{
    dbg << "\n<<";
    for (std::map<PEAT_Rank, std::list<MAT_P2P_Record>>::const_iterator it = rec.begin(); it != rec.end(); ++it) {
        dbg << "Rank " << it->first << it->second;
    }
    dbg << ">>" << std::endl;
    return dbg;
}
