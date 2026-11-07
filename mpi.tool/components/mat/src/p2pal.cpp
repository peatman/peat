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
#include <iterator>
#include <sstream>

MAT_P2P_Record::MAT_P2P_Record(const Tractor_Event& s): MAT_Record()
{
    rank = s.rank;
    pc_level = s.level;
    comm = s.P2P.comm;
    size = s.P2P.transfered_bytes;
    tag = s.P2P.tag;
    message = s.P2P.message;
    peer_rank = s.P2P.peer;
    request = s.P2P.request;
    peer_assigned = false;
    completed = false;
    is_send = (s.func_class == PEAT_PROP_CLASS_P2P && global_func_id.check_property(s.func_id, PEAT_PROP_DATA_TX));
}   


P2PAL::P2PAL(File_Storage_GZ *f, int s, int pos,
#ifdef PEAT_MPI
             std::list<PEAT_Event_Delivery> *ad, const std::vector<PEAT_Rank> &wR, const std::vector<PEAT_Rank> &mRs, const MPI_Comm mat_cc, const PEAT_Rank m_r,
#endif
             int v, const std::string &bl, const char dl) :
                                                            skip_locals(s), print_on_send(pos), verbose(v)
#ifdef PEAT_MPI
                                                            , active_deliveries(ad), mat_compute_comm(mat_cc), whole_ranks(wR), my_ranks(mRs), my_rank(m_r)
#endif
                                                            //, adaptive_blocking_threshold(INITIAL_ABF)

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

void P2PAL::initialize(PTP *p, Communicators *c, Request_Store *rs, Requests *r, GPU *g, File_Storage_GZ *MPI, File_Storage_GZ *HWC, File_Storage_GZ *bind)
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
    fprintf(stderr, "%d:%s:%s\n",
            event.rank,
            global_func_id.name_by_idx(event.ph.func_id),
            peat_ids_string(event.ph.ids).c_str());
}

#if _DEBUG
static long long max_dif = 0;
#endif
void P2PAL::add_imbalance(const MAT_P2P_Record &left, const MAT_P2P_Record &right, PEAT_Single_Event_Clock &left_clock, PEAT_Single_Event_Clock &right_clock)
{
    PEAT_Clock left_in = left.ph.tsc_time.func.in;
    PEAT_Clock left_out = left.ph.tsc_time.func.out; 
    PEAT_Clock right_in = right.ph.tsc_time.func.in; 
    PEAT_Clock right_out = right.ph.tsc_time.func.out;   
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
#if 0 //_DEBUG
    if (right.ph.tsc_time.func.out < left.ph.tsc_time.func.in) {
        long long diff = left.ph.tsc_time.func.in - right.ph.tsc_time.func.out;
        if (max_dif < diff) { // less recording
            MAT_CERR("Receive ended earlier than Transmit (tsc diff:" << std::dec << diff << ") {\nSend(host:" << ptp->get_host_name(left.rank) << "): " << left << "\nRecv(host:" << ptp->get_host_name(right.rank) << "): " << right << "\n}" << std::endl);
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
            global_func_id.name_by_idx(left.ph.func_id), left.rank, peat_ids_string(left.ph.ids).c_str(),
            global_func_id.name_by_idx(right.ph.func_id), right.rank, peat_ids_string(right.ph.ids).c_str(),
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
        // counted by the receiving pmat rank when it writes the record (Mat::print_remote_string), like the remote
        // request binds - counting here too would count every record twice in the .trace.info Bind_Peer totals
#else
        output_bind->puts(left.rank, &tbuf[4]);
        #pragma omp atomic
        messages_bound[left.rank]++;
#endif
    } else {
        #pragma omp atomic
        messages_bound[right.rank]++;
        output_bind->puts(right.rank, &tbuf[4]);
    }
#ifdef PRINT_STAT
    if (output_HWC && output_HWC->get_number_of_files() > 0) { // no timing data to compute stats/imbalance from when hwc files aren't read
#ifdef PEAT_OMP
        std::lock_guard<std::mutex> stats_guard(stats_lock); // printed_p2p is shared by all ranks
#endif
        int pc_level = left.pc_level;
        PEAT_P2P_Record r_left;
        r_left.func_id = left.ph.func_id;
        r_left.bytes = left.size;
        r_left.peer = left.peer_rank;
        r_left.rank = left.rank;
        PEAT_Single_Event_Clock &left_clock = printed_p2p[pc_level][r_left];
        PEAT_Clock duration = left.ph.tsc_time.mpi.out - left.ph.tsc_time.mpi.in;
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
        r_right.func_id = right.ph.func_id;
        r_right.bytes = right.size;
        r_right.peer = right.peer_rank;
        r_right.rank = right.rank;

        PEAT_Single_Event_Clock &right_clock = printed_p2p[pc_level][r_right];
        duration = right.ph.tsc_time.mpi.out - right.ph.tsc_time.mpi.in;
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
            fprintf(stderr, "p2p. Ranks:%d,%d ids:%lld (%lld) => (%lld),%lld\n", left.rank, right.rank,
                left.ph.ids[0],
                left.size, right.size,
                right.ph.ids[0]);
            peat_throw_error("Communicators mismatch\n");
        }
        if (left.tag != right.tag) {
            fprintf(stderr, "p2p. Ranks:%d,%d ids:%lld (%lld) => (%lld) %lld\n", left.rank, right.rank,
                    left.ph.ids[0],
                    left.size, right.size,
                    right.ph.ids[0]);
            peat_throw_error("Tags mismatch\n");
        }
        // a nonblocking/persistent receive not completed yet still has the posted count as its size, not the received one
        const PEAT_Bool right_size_final = right.request == 0 || right.completed;
        if (left.size != right.size && right_size_final) {
            fprintf(stderr, "p2p. Ranks:%d,%d ids:%lld (%lld) => (%lld) %lld\n", left.rank, right.rank,
                    left.ph.ids[0],
                    left.size, right.size,
                    right.ph.ids[0]);
            peat_warning("Sizes mismatch\n");
        }
        if (left.ph.func_id == right.ph.func_id && !global_func_id.check_property(right.ph.func_id, PEAT_PROP_DATA_TXRX) && !global_func_id.check_property(right.ph.func_id, PEAT_PROP_PERSISTENT_REQUEST_START)) {
            peat_throw_error("Same functions used for send and receive\n");
        }
    }
#endif
}

void P2PAL::print_Local(const Tractor_Event &event)
{
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

PEAT_Bool P2PAL::match_peer(matching_event_type &queues, std::list<MAT_P2P_Record *>::iterator &ret_it, const MAT_P2P_Record &input_record) const
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


PEAT_Bool P2PAL::update_request(const PEAT_Rank rank, const Request_Store_Record_Type type, const PEAT_MPI_Tag tag, const PEAT_Rank source, const PEAT_Message_Size size, PEAT_MPI_Request request, const Phase &phase, PEAT_Bool is_remote)
{
	MAT_P2P_Record* record = NULL;
    MAT_Request_Store_Record ret_it;
    if (request > 0) {
		if (req_store->find_request(rank, request, type, &ret_it, true)) { //find and delete the request from the store, as it's being completed
            record = static_cast<MAT_P2P_Record*>(ret_it.record);
        }   
    }

    if (record) {
        if (record->peer_rank == comm_store->get_MPI_PROC_NULL()) {
            assert(0);
            return true;
        }
        const PEAT_Bool was_wildcard = unresolved_wildcard(*record);
        { // a record in the matching queues can be paired by its peer's thread meanwhile; a held wildcard receive can't
        Rank_Guard guard(*this, was_wildcard ? -1 : rank, was_wildcard ? -1 : record->peer_rank);
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
        record->completed = true;
        request_func_store->print_req_bind(rank, request, record->ph, phase, is_remote);
        }
        if (was_wildcard) // resolved now: it and the receives held behind it may enter the matching queues
            release_receives(rank, record->comm);

        Rank_Guard guard(*this, rank, record->peer_rank);
        PEAT_Num queue_type = record->is_send ? qt_send : qt_recv;
        PEAT_Num peer_queue_type = record->is_send ? qt_recv : qt_send;
        P2P_Event_Key key(*record);
        std::list<MAT_P2P_Record *>::iterator req_it = matching_queues[queue_type][rank][key].begin();
        std::list<MAT_P2P_Record *>::iterator peer_it;
        if (req_it != matching_queues[queue_type][rank][key].end()) {
            if (match_peer(matching_queues[peer_queue_type], peer_it, **req_it)) {
                if ((*req_it)->is_send)
                    bind_pair(req_it, peer_it);
                else
                    bind_pair(peer_it, req_it);
            }
        }
        return true;
    }
    return false;
}

PEAT_Bool P2PAL::hold_receive(const PEAT_Rank rank, MAT_P2P_Record *record)
{
    std::map<PEAT_MPI_Comm, std::list<MAT_P2P_Record *>> &rank_held = held_receives[rank];
    std::map<PEAT_MPI_Comm, std::list<MAT_P2P_Record *>>::iterator held = rank_held.find(record->comm);
    if (held == rank_held.end()) {
        if (!unresolved_wildcard(*record))
            return false;
        held = rank_held.insert(std::make_pair(record->comm, std::list<MAT_P2P_Record *>())).first;
    }
    held->second.push_back(record);
    return true;
}
void P2PAL::release_receives(const PEAT_Rank rank, const PEAT_MPI_Comm comm)
{
    std::map<PEAT_MPI_Comm, std::list<MAT_P2P_Record *>>::iterator held = held_receives[rank].find(comm);
    if (held == held_receives[rank].end())
        return;
    while (!held->second.empty() && !unresolved_wildcard(*held->second.front())) {
        MAT_P2P_Record *record = held->second.front();
        held->second.pop_front();
        Rank_Guard guard(*this, rank, record->peer_rank); // this rank's queue and the sender's
        matching_queues[qt_recv][rank][P2P_Event_Key(*record)].push_back(record);
        match_waiting_sends(rank, *record);
    }
    if (held->second.empty())
        held_receives[rank].erase(held);
}
void P2PAL::release_all_receives()
{
    for (PEAT_Rank rank = 0; rank < static_cast<PEAT_Rank>(held_receives.size()); rank++) {
        for (std::map<PEAT_MPI_Comm, std::list<MAT_P2P_Record *>>::iterator held = held_receives[rank].begin(); held != held_receives[rank].end(); ++held) {
            for (std::list<MAT_P2P_Record *>::iterator it = held->second.begin(); it != held->second.end(); ++it)
                matching_queues[qt_recv][rank][P2P_Event_Key(**it)].push_back(*it);
        }
        held_receives[rank].clear();
    }
}
void P2PAL::drop_wildcard_barrier(const PEAT_Rank rank, MAT_P2P_Record *record)
{
    if (record->is_send || rank < 0 || rank >= static_cast<PEAT_Rank>(held_receives.size()))
        return;
    std::map<PEAT_MPI_Comm, std::list<MAT_P2P_Record *>>::iterator held = held_receives[rank].find(record->comm);
    if (held == held_receives[rank].end() || !unresolved_wildcard(*record))
        return;
    for (std::list<MAT_P2P_Record *>::iterator it = held->second.begin(); it != held->second.end(); ++it) {
        if (*it == record) {
            // it never resolves: it goes where it was before wildcards were held (unpaired at the end, as then)
            held->second.erase(it);
            {
                Rank_Guard guard(*this, rank);
                matching_queues[qt_recv][rank][P2P_Event_Key(*record)].push_back(record);
            }
            release_receives(rank, record->comm);
            return;
        }
    }
}
/* recv has just entered the matching queues: pair the sends waiting for its envelope (any message handle) in order.
   Its envelope is copied first: pairing deletes the records without a request, recv possibly among them. */
void P2PAL::match_waiting_sends(const PEAT_Rank recv_rank, const MAT_P2P_Record &recv)
{
    const PEAT_Rank send_rank = recv.peer_rank;
    const PEAT_MPI_Tag tag = recv.tag;
    const PEAT_MPI_Comm comm = recv.comm;
    if (send_rank < 0 || send_rank >= static_cast<PEAT_Rank>(matching_queues[qt_send].size()))
        return;
    std::map<P2P_Event_Key, std::list<MAT_P2P_Record *>> &sends = matching_queues[qt_send][send_rank];
    for (std::map<P2P_Event_Key, std::list<MAT_P2P_Record *>>::iterator q = sends.begin(); q != sends.end(); ++q) {
        if (q->first.tag != tag || q->first.peer != recv_rank || q->first.comm != comm)
            continue;
        std::list<MAT_P2P_Record *>::iterator recv_it;
        while (!q->second.empty() && match_peer(matching_queues[qt_recv], recv_it, *q->second.front()))
            bind_pair(q->second.begin(), recv_it);
    }
}
void P2PAL::bind_pair(std::list<MAT_P2P_Record *>::iterator send_it, std::list<MAT_P2P_Record *>::iterator recv_it)
{
    MAT_P2P_Record *lP = *send_it;
    MAT_P2P_Record *rP = *recv_it;
    P2P_Event_Key send_key(*lP), recv_key(*rP);
    lP->peer_assigned = rP->peer_assigned = true;
    print_P2PF(*lP, *rP);
    matching_queues[qt_send][lP->rank][send_key].erase(send_it);
    matching_queues[qt_recv][rP->rank][recv_key].erase(recv_it);
    if (lP->request == 0)
        delete lP;
    if (rP->request == 0)
        delete rP;
}

void P2PAL::write_HNT() const
{
    std::ostringstream tmp;
    tmp << "FREQ" << ":" << ptp->get_trace_freq(-1) << "\n";
}
PEAT_Bool P2PAL::empty() const
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

PEAT_Bool P2PAL::add_pers_req_start(const PEAT_Rank rank, PEAT_MPI_Func_ID init_func_id, MAT_P2P_Record *record)
{
    if (record->peer_rank == comm_store->get_MPI_PROC_NULL()) {
        return false;
    }
    P2P_Event_Key key(*record);
    if (global_func_id.check_property(init_func_id, PEAT_PROP_DATA_RX)) {
        if (!hold_receive(rank, record)) {
            Rank_Guard guard(*this, rank);
            matching_queues[qt_recv][rank][key].push_back(record);
        }
    } else if (global_func_id.check_property(init_func_id, PEAT_PROP_DATA_TX)) {
        Rank_Guard guard(*this, rank);
        matching_queues[qt_send][rank][key].push_back(record);
    } else {
        MAT_CERR("Weird persistent request." << std::endl);
        return false;
    }
    MAT_Request_Store_Record tmp;
	tmp.request_type = REQUEST_TYPE_USER_P2P;
	tmp.record = record;
	req_store->add_request(rank, record->request, &tmp);
    return true;
}
void P2PAL::parse_P2PTx(PEAT_Rank rank, const Tractor_Event &event, PEAT_Bool print_out)
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
        record->ph = event;
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
            tmp.request_type = global_func_id.check_property(event.func_id, PEAT_PROP_MPI_INTERNAL) ? REQUEST_TYPE_INTERNAL : REQUEST_TYPE_USER_P2P;
            tmp.record = record;
            req_store->add_request(rank, record->request, &tmp);
        }
        Rank_Guard guard(*this, rank, record->peer_rank); // this rank's queue and the receiver's
        P2P_Event_Key key(*record);
        matching_queues[qt_send][rank][key].push_back(record);
        std::list<MAT_P2P_Record *>::iterator matching_rec_it;
        std::list<MAT_P2P_Record *>::iterator record_it = matching_queues[qt_send][rank][key].begin();
        if (record_it != matching_queues[qt_send][rank][key].end()) {
            if (match_peer(matching_queues[qt_recv], matching_rec_it, **record_it)) {
                bind_pair(record_it, matching_rec_it);
            }
        }
    } else {
        if (print_out)
            print_P2PL(event);
#ifdef PEAT_MPI
        if (event.P2P.request > 0) {
            if (global_func_id.check_property(event.func_id, PEAT_PROP_PERSISTENT_REQUEST_INIT)) {
                if (req_store->thrown_away_pers_requests[rank].find(event.P2P.request) != req_store->thrown_away_pers_requests[rank].end()) {
                    MAT_CERR("Persistent request " << std::hex << event.P2P.request << std::dec << " conflict.Rank: " << rank << ". ids: " << peat_ids_string(event.ids) << std::endl);
                };
                req_store->thrown_away_pers_requests[rank].insert(event.P2P.request);
                MAT_CERR("Invalid persistent request " << std::hex << event.P2P.request << std::dec << " thrown away.Rank: " << rank << ". ids: " << peat_ids_string(event.ids) << std::endl);
            } else {
                if (req_store->thrown_away_requests[rank].find(event.P2P.request) != req_store->thrown_away_requests[rank].end()) {
                    MAT_CERR("Request " << std::hex << event.P2P.request << std::dec << " conflict.Rank: " << rank << ". ids: " << peat_ids_string(event.ids) << std::endl);
                }
                req_store->thrown_away_requests[rank].insert(event.P2P.request);
                MAT_CERR("Invalid request " << std::hex << event.P2P.request << std::dec << " thrown away.Rank: " << rank << ". ids: " << peat_ids_string(event.ids) << std::endl);
            }
        }
#endif
    }
}
void P2PAL::parse_P2PRx(PEAT_Rank rank, const Tractor_Event &event, PEAT_Bool increase_MPI)
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
        record->ph = event;
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
            tmp.request_type = global_func_id.check_property(event.func_id, PEAT_PROP_MPI_INTERNAL) ? REQUEST_TYPE_INTERNAL : REQUEST_TYPE_USER_P2P;
            tmp.record = record;
            req_store->add_request(rank, record->request, &tmp);
        }
        if (hold_receive(rank, record))
            return;

        Rank_Guard guard(*this, rank, record->peer_rank); // this rank's queue and the sender's
        P2P_Event_Key key(*record);
        matching_queues[qt_recv][rank][key].push_back(record);
        std::list<MAT_P2P_Record *>::iterator record_it = matching_queues[qt_recv][rank][key].begin();
        std::list<MAT_P2P_Record *>::iterator matching_rec_it;

        if (record_it != matching_queues[qt_recv][rank][key].end()) {
            if (match_peer(matching_queues[qt_send], matching_rec_it, **record_it)) {
                bind_pair(matching_rec_it, record_it);
            }
        }
    } else {
        print_P2PL(event);
#if 0//def PEAT_MPI
        if (event.P2P.request > 0) {
            if (global_func_id.check_property(event.func_id, PEAT_PROP_PERSISTENT_REQUEST_INIT)) {
                if (req_store->thrown_away_pers_requests[rank].find(event.P2P.request) != req_store->thrown_away_pers_requests[rank].end()) {
                    MAT_CERR("Persistent request " << std::hex << event.P2P.request << std::dec << " conflict.Rank: " << rank << ". ids: " << peat_ids_string(event.ids) << std::endl);
                };
                req_store->thrown_away_pers_requests[rank].insert(event.P2P.request);
                MAT_CERR("Invalid persistent request " << std::hex << event.P2P.request << std::dec << " thrown away.Rank: " << rank << ". ids: " << peat_ids_string(event.ids) << std::endl);
            }
            else {
                if (req_store->thrown_away_requests[rank].find(event.P2P.request) != req_store->thrown_away_requests[rank].end()) {
                    MAT_CERR("Request " << std::hex << event.P2P.request << std::dec << " conflict.Rank: " << rank << ". ids: " << peat_ids_string(event.ids) << std::endl);
                }
                req_store->thrown_away_requests[rank].insert(event.P2P.request);
                MAT_CERR("Invalid request " << std::hex << event.P2P.request << std::dec << " thrown away.Rank: " << rank << ". ids: " << peat_ids_string(event.ids) << std::endl);
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
                    MAT_CERR("Persistent request " << std::hex << event.P2P.request << std::dec << " conflict.Rank: " << rank << ". ids: " << peat_ids_string(event.ids) << std::endl);
                };
                req_store->thrown_away_pers_requests[rank].insert(event.P2P.request);
                MAT_CERR("Invalid persistent request " << std::hex << event.P2P.request << std::dec << " thrown away.Rank: " << rank << ". ids: " << peat_ids_string(event.ids) << std::endl);
            } else {
                if (req_store->thrown_away_requests[rank].find(event.P2P.request) != req_store->thrown_away_requests[rank].end()) {
                    MAT_CERR("Request conflict " << std::hex << event.P2P.request << std::dec << " conflict.Rank: " << rank << ". ids: " << peat_ids_string(event.ids) << std::endl);
                }
                req_store->thrown_away_requests[rank].insert(event.P2P.request);
                MAT_CERR("Invalid request " << std::hex << event.P2P.request << std::dec << " thrown away.Rank: " << rank << ". ids: " << peat_ids_string(event.ids) << std::endl);
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

    if (event.P2P.request > 0) {
        record.request = event.P2P.request;
        if (global_func_id.check_property(event.func_id, PEAT_PROP_PERSISTENT_REQUEST_INIT)) {
            req_store->flown_away_pers_requests[rank][record.request] = record;
        } else {
            if(global_func_id.check_property(event.func_id, PEAT_PROP_MPI_INTERNAL))
                req_store->flown_away_internal_requests[rank][record.request] = record;
            else
                req_store->flown_away_user_requests[rank][record.request].push_back(record);
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

void P2PAL::process_unbound_events(PEAT_Rank my_rank)
{
    release_all_receives();
    for (matching_event_type::iterator rank_it = matching_queues[qt_send].begin(); rank_it != matching_queues[qt_send].end(); ++rank_it) {
        for (std::map<P2P_Event_Key, std::list<MAT_P2P_Record *>>::iterator send_it = rank_it->begin(); send_it != rank_it->end(); ++send_it) {
            for (std::list<MAT_P2P_Record *>::iterator event_it = send_it->second.begin(); event_it != send_it->second.end();) {
                std::list<MAT_P2P_Record *>::iterator matching_rec_it;
                if (match_peer(matching_queues[qt_recv], matching_rec_it, **event_it)) {
                    std::list<MAT_P2P_Record *>::iterator next = std::next(event_it);
                    bind_pair(event_it, matching_rec_it);
                    event_it = next;
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
                    std::list<MAT_P2P_Record *>::iterator next = std::next(event_it);
                    bind_pair(matching_rec_it, event_it);
                    event_it = next;
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
    release_all_receives();
    int total_unpaired_events = 0;
    int hanged_events = 0;
    for (matching_event_type::iterator rank_it = matching_queues[qt_send].begin(); rank_it != matching_queues[qt_send].end(); ++rank_it) {
        for (std::map<P2P_Event_Key, std::list<MAT_P2P_Record *>>::iterator send_it = rank_it->begin(); send_it != rank_it->end(); ++send_it) {
            for (std::list<MAT_P2P_Record *>::iterator event_it = send_it->second.begin(); event_it != send_it->second.end();) {
                std::list<MAT_P2P_Record *>::iterator matching_rec_it;

                if (match_peer(matching_queues[qt_recv], matching_rec_it, **event_it)) {
                    // a request still held indicates a hanged one (send/recv without wait/test)
                    hanged_events += ((*event_it)->request > 0) + ((*matching_rec_it)->request > 0);
                    std::list<MAT_P2P_Record *>::iterator next = std::next(event_it);
                    bind_pair(event_it, matching_rec_it);
                    event_it = next;
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
                    hanged_events += ((*event_it)->request > 0) + ((*matching_rec_it)->request > 0);
                    std::list<MAT_P2P_Record *>::iterator next = std::next(event_it);
                    bind_pair(matching_rec_it, event_it);
                    event_it = next;
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
#ifdef PEAT_OMP
            if (rank < n_rank_locks)
                rank_locks[rank].lock();
#endif
            for (std::map<P2P_Event_Key, std::list<MAT_P2P_Record *>>::const_iterator key_it = matching_queues_it->cbegin(); key_it != matching_queues_it->cend(); ++key_it) {
                size += key_it->second.size();
            }
#ifdef PEAT_OMP
            if (rank < n_rank_locks)
                rank_locks[rank].unlock();
#endif
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
        << "," << rec.ph
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
