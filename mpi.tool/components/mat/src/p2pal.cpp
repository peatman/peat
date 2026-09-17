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
#include "utils.h"

#include <assert.h>
#include <sstream>

P2PAL::P2PAL(PTP *p, Communicators *c, File_Storage_GZ *f, GPU *g, int s, int pos,
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
    q_name_s2[qt_pers] = "Persistent";
    q_name_s2[qt_hanged] = "Hanged";
    q_name_s2[qt_normal] = "Normal";
    assert(c);
    assert(p);
    assert(g);
    com_store = c;
    input_MPI = f;
    ptp = p;
    gpu = g;
    messages_bound.resize(ptp->get_rank_num());
}

P2PAL::~P2PAL()
{
    ptp = 0;
    com_store = 0;
}

void P2PAL::print_P2PL(const Tractor_Event &event)
{
    output_MPI->printf(event.rank, "%lld:%lld:%d:%s:%d:%d:%llx:%llx:%d:%lld\n", event.primary_id, event.secondary_id, event.thread_id, global_func_id.name_by_idx(event.func_id),
                       event.P2P.comm, event.P2P.peer, event.P2P.request, event.P2P.message, event.P2P.tag, event.P2P.transfered_bytes);

#if 0
    output_HWC->printf(event.rank, "%lld:%lld:%d:%lld:%lld:%lld:%lld:%lld:%lld", event.primary_id, event.secondary_id, event.thread_id,
        event.tsc_time.func.in,
        event.tsc_time.mpi.in,
        event.tsc_time.mpi.out,
        event.tsc_time.func.out,
        event.tsc_time.pe.in,
        event.tsc_time.pe.out);

#else
    output_HWC->printf(event.rank, "%lld:%lld:%d:%lld:%lld:%lld:%lld", event.primary_id, event.secondary_id, event.thread_id,
                       event.tsc_time.mpi.in,
                       event.tsc_time.mpi.out,
                       event.tsc_time.pe.in,
                       event.tsc_time.pe.out);
#endif
    for (PEAT_Num i = 0; i < event.addrs.size(); i++) {
        output_HWC->printf(event.rank, ":%llx", event.addrs.at(i));
    }
    output_HWC->printf(event.rank, "\n");
}
void P2PAL::print_P2PE(const MAT_Record &event)
{
    // gzFile Bind = output_bind->get_file_ptr(rec.rank);
    fprintf(stderr, "%d:%s:%lld:%lld:%s:%lld:%lld:%s:%lld:%lld\n",
            event.rank,
            global_func_id.name_by_idx(event.ph[0].func_id),
            event.ph[0].primary_id, event.ph[0].secondary_id,
            global_func_id.name_by_idx(event.ph[1].func_id),
            event.ph[1].primary_id, event.ph[1].secondary_id,
            global_func_id.name_by_idx(event.ph[2].func_id),
            event.ph[2].primary_id, event.ph[2].secondary_id);
}

#if _DEBUG
static long long max_dif = 0;
#endif
void P2PAL::add_imbalance(const MAT_Record &left, const MAT_Record &right, PEAT_Single_Event_Clock &left_clock, PEAT_Single_Event_Clock &right_clock)
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
void P2PAL::print_P2PF(const MAT_Record &left, const MAT_Record &right)
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
    sprintf(&tbuf[4], "%d:%s:%d:%lld:%lld:%s:%d:%lld:%lld:%lld\n", PEAT_RT_Bind_Peer,
            global_func_id.name_by_idx(left.ph[1].func_id),
            left.rank, left.ph[1].primary_id, left.ph[1].secondary_id,
            global_func_id.name_by_idx(right.ph[2].func_id),
            right.rank, right.ph[2].primary_id, right.ph[2].secondary_id,
            left.size);
    assert(left.size == right.size);
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
    {
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

        if (left.comm != right.comm) {
            fprintf(stderr, "p2p. Ranks:%d,%d ids:%lld,%lld,%lld (%lld) => (%lld) %lld,%lld,%lld\n", left.rank, right.rank,
                    left.ph[0].primary_id, left.ph[1].primary_id, left.ph[2].primary_id,
                    left.size, right.size,
                    right.ph[0].primary_id, right.ph[1].primary_id, right.ph[2].primary_id);
            peat_throw_error("Communicators mismatch\n");
        }
        if (left.tag != right.tag) {
            fprintf(stderr, "p2p. Ranks:%d,%d ids:%lld,%lld,%lld (%lld) => (%lld) %lld,%lld,%lld\n", left.rank, right.rank,
                    left.ph[0].primary_id, left.ph[1].primary_id, left.ph[2].primary_id,
                    left.size, right.size,
                    right.ph[0].primary_id, right.ph[1].primary_id, right.ph[2].primary_id);
            peat_throw_error("Tags mismatch\n");
        }
        if (left.size != right.size) {
            fprintf(stderr, "p2p. Ranks:%d,%d ids:%lld,%lld,%lld (%lld) => (%lld) %lld,%lld,%lld\n", left.rank, right.rank,
                    left.ph[0].primary_id, left.ph[1].primary_id, left.ph[2].primary_id,
                    left.size, right.size,
                    right.ph[0].primary_id, right.ph[1].primary_id, right.ph[2].primary_id);
            peat_throw_error("Sizes mismatch\n");
        }
        if (left.ph[0].func_id == right.ph[0].func_id && right.ph[0].func_id != MPI_SENDRECV_ID && left.ph[0].func_id != MPI_SENDRECV_REPLACE_ID) {
            peat_throw_error("Same functions used for send and receive\n");
        }
    }
#endif
    /*
        fprintf(stderr, "p2p. Ranks:%d,%d ids:%lld,%lld,%lld (%lld) => (%lld) %lld,%lld,%lld\n", left.rank, right.rank,
            left.ph[0].primary_id, left.ph[1].primary_id, left.ph[2].primary_id,z
            left.size, right.size,
            right.ph[0].primary_id, right.ph[1].primary_id, right.ph[2].primary_id);
    */
}

void P2PAL::print_Local(const Tractor_Event &event)
{
    // cerr << "Local. Rank:" << local_record.rank << ". uniq_id:" << local_record.ph[0].uniq_id << endl;
    if (skip_locals)
        return;
    if (event.landbuf.inbuf_size >= 0 || event.landbuf.outbuf_size >= 0) {
        output_MPI->printf(event.rank, "%lld:%lld:%d:%s:%d:%d\n", event.primary_id, event.secondary_id, event.thread_id, global_func_id.name_by_idx(event.func_id), event.landbuf.inbuf_size, event.landbuf.outbuf_size);
    } else {
        output_MPI->printf(event.rank, "%lld:%lld:%d:%s\n", event.primary_id, event.secondary_id, event.thread_id, global_func_id.name_by_idx(event.func_id));
    }
#if 0
    output_HWC->printf(event.rank, "%lld:%lld:%d:%lld:%lld:%lld:%lld:%lld:%lld", event.primary_id, event.secondary_id, event.thread_id,
        event.tsc_time.func.in,
        event.tsc_time.mpi.in,
        event.tsc_time.mpi.out,
        event.tsc_time.func.out,
        event.tsc_time.pe.in,
        event.tsc_time.pe.out);

#else
    output_HWC->printf(event.rank, "%lld:%lld:%d:%lld:%lld:%lld:%lld", event.primary_id, event.secondary_id, event.thread_id,
                       event.tsc_time.mpi.in,
                       event.tsc_time.mpi.out,
                       event.tsc_time.pe.in,
                       event.tsc_time.pe.out);

#endif
    for (PEAT_Num i = 0; i < event.addrs.size(); i++) {
        output_HWC->printf(event.rank, ":%llx", event.addrs.at(i));
    }
    output_HWC->printf(event.rank, "\n");
}

bool P2PAL::match_peer(matching_event_type &queues, std::list<MAT_Record *>::iterator &ret_it, const MAT_Record &input_record) const
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

    if (peer_rank == com_store->get_MPI_PROC_NULL()) {
        return false;
    }
    if (peer_rank == com_store->get_MPI_ANY_SOURCE()) {
        assert(0);
        return false;
    }
    if (tag == com_store->get_MPI_ANY_TAG()) {
        assert(0);
        return false;
    }
    if (queues.empty()) {
        return false;
    }
    P2P_Event_Key key(tag, rank, message, comm);

    std::list<MAT_Record *> &queue = queues[peer_rank][key];
    if (!queue.empty()) {
        std::list<MAT_Record *>::iterator it = queues[peer_rank][key].begin();
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
bool P2PAL::find_request(PEAT_Num queue_type, PEAT_Rank rank, PEAT_MPI_Request request, MAT_Record **ret_it, PEAT_Bool erase)
{
    std::map<PEAT_Rank, std::set<PEAT_MPI_Request>> *queue;
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
            request_queues_type &queue = request_queues[queue_type];
            if (!queue[rank].empty()) {
                std::map<PEAT_MPI_Request, std::list<MAT_Record *>>::iterator event_it = queue[rank].find(request);
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
        request_queues_type &queue = request_queues[queue_type];
        if (!queue[rank].empty()) {
            std::map<PEAT_MPI_Request, std::list<MAT_Record *>>::iterator rank_it = queue[rank].find(request);
            if (rank_it != queue[rank].end()) {
                if (!rank_it->second.empty()) {
                    assert(!(queue_type == qt_pers && rank_it->second.size() > 1)); // only 1 persistent request is allowed at any given time
                    for (std::list<MAT_Record *>::iterator event_it = rank_it->second.begin(); event_it != rank_it->second.end(); ++event_it) {
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

MAT_Record *P2PAL::check_waiting_queue(PEAT_Rank rank, PEAT_MPI_Request request)
{
#ifdef USE_FAST_CHECK
    if (request > 0) {
        std::map<PEAT_MPI_Request, MAT_Record *>::iterator event_it = waiting_events_f[rank].find(request);
        if (event_it != waiting_events_f[rank].end()) {
            return event_it->second;
        }
    }
#else
    if (request > 0) {
        std::list<MAT_Record *>::iterator event_it = waiting_events[rank].begin();
        for (; event_it != waiting_events[rank].end(); ++event_it) {
            if ((*event_it)->request == request)
                return *event_it;
        }
    }
#endif
    return NULL;
}
int P2PAL::update_waiting_queue(PEAT_Rank rank, const PEAT_MPI_Tag tag, const PEAT_Rank source, const PEAT_Message_Size size, PEAT_MPI_Request request, const Phase &phase, MAT_Record **req_it)
{
    int found = 0;
    std::list<MAT_Record *>::iterator event_it = waiting_events[rank].begin();
    if (event_it != waiting_events[rank].end()) {
        if ((*event_it)->request == 0) {
            P2P_Event_Key key(**event_it);
            matching_queues[qt_recv][rank][key].push_back(*event_it);
            event_it = waiting_events[rank].erase(event_it);
            *req_it = *matching_queues[qt_recv][rank][key].begin();
            found = 1;
        }
    }
    if (request > 0) {
#ifdef USE_FAST_CHECK
        std::map<PEAT_MPI_Request, MAT_Record *>::iterator event_it = waiting_events_f[rank].find(request);

        if (event_it != waiting_events_f[rank].end()) {
            event_it->second->request = 0;
            if (!event_it->second->is_send) {
                if (event_it->second->tag == com_store->get_MPI_ANY_TAG()) { // any tag
                    event_it->second->tag = tag;
                } else {
                    if (event_it->second->tag != tag) {
                        //                                        cerr << "Tags mismatch: " << it->tag << ":" << tag << " ignored." << endl;
                    }
                }
                if (event_it->second->peer_rank == com_store->get_MPI_ANY_SOURCE()) { // any source
                    event_it->second->peer_rank = source;                             // com_store->get_rank_from_comm(it->comm, rank, source, it->ph[1].tsc_u_time.func.in);
                } else {
                    if (event_it->second->peer_rank != source) {
                        //                                        cerr << "Ranks mismatch " << it->peer_rank << ":" << source << " ignored." << endl;
                    }
                }
                if (size >= 0)
                    event_it->second->size = size;
            }
            event_it->second->ph[2] = phase;
            waiting_events_f[rank].erase(event_it);
            found |= 2;
            MAT_Record *dummy;
            find_request(qt_normal, rank, request, &dummy, true);
        }
#else
        for (; event_it != waiting_events[rank].end(); ++event_it) {
            if ((*event_it)->request == request) {
                (*event_it)->request = 0;
                if (!(*event_it)->is_send) {
                    if ((*event_it)->tag == com_store->get_MPI_ANY_TAG()) { // any tag
                        (*event_it)->tag = tag;
                    } else {
                        if ((*event_it)->tag != tag) {
                            //                                        cerr << "Tags mismatch: " << it->tag << ":" << tag << " ignored." << endl;
                        }
                    }
                    if ((*event_it)->peer_rank == com_store->get_MPI_ANY_SOURCE()) { // any source
                        (*event_it)->peer_rank = source;                             // com_store->get_rank_from_comm(it->comm, rank, source, it->ph[1].tsc_u_time.func.in);
                    } else {
                        if ((*event_it)->peer_rank != source) {
                            //                                        cerr << "Ranks mismatch " << it->peer_rank << ":" << source << " ignored." << endl;
                        }
                    }
                    if (size >= 0)
                        (*event_it)->size = size;
                }
                (*event_it)->ph[2] = phase;
                found |= 2;
                MAT_Record *dummy;
                find_request(qt_normal, rank, request, &dummy, true);
                // assert(dummy);
                break;
            }
        }
#endif
    }
    return found;
}

bool P2PAL::find_event(int queue_type, PEAT_Rank rank, MAT_Record *rec, std::list<MAT_Record *>::iterator &ret_it)
{
    for (std::list<MAT_Record *>::iterator it = request_queues[queue_type][rank][rec->request].begin(); it != request_queues[queue_type][rank][rec->request].end(); ++it) {
        if (*it == rec) {
            ret_it = it;
            return true;
        }
    }
    return false;
}

bool P2PAL::update_request(PEAT_Num queue_type, PEAT_Rank rank, const PEAT_MPI_Tag tag, const PEAT_Rank source, const PEAT_Message_Size size, PEAT_MPI_Request request, const Phase &phase, MAT_Record *record)
{
    if (request > 0) {
        if (queue_type == qt_hanged) {
            std::map<PEAT_MPI_Request, std::list<MAT_Record *>>::iterator it = request_queues[queue_type][rank].find(request);
            if (it != request_queues[queue_type][rank].end()) {
                if (!it->second.empty()) {
                    MAT_Record *req_d = it->second.front();
                    assert(req_d->peer_assigned);
                    it->second.pop_front();
                    delete req_d;
                    return true; // qt_hanged requests are handled(bound) in the caller
                }
            }
        }
    }

    if (record->request == request) {
        if (record->peer_rank == com_store->get_MPI_PROC_NULL()) {
            assert(0);
            return true;
        }
        if (!record->is_send) {
            if (record->tag == com_store->get_MPI_ANY_TAG()) { // any tag
                record->tag = tag;
            } else {
                if (record->tag != tag) {
                    //                                        cerr << "Tags mismatch: " << it->tag << ":" << tag << " ignored." << endl;
                }
            }
            if (record->peer_rank == com_store->get_MPI_ANY_SOURCE()) { // any source
                record->peer_rank = source;                             // com_store->get_rank_from_comm(it->comm, rank, source, it->ph[1].tsc_u_time.func.in);
            } else {
                if (record->peer_rank != source) {
                    //                                        cerr << "Ranks mismatch " << it->peer_rank << ":" << source << " ignored." << endl;
                }
            }
            if (size >= 0)
                record->size = size;
        }
        if (queue_type == qt_normal) {
            record->request = 0;
            // update completion tsc if matched
            record->ph[2] = phase;
        }
        PEAT_Num queue_type = record->is_send ? qt_send : qt_recv;
        PEAT_Num peer_queue_type = record->is_send ? qt_recv : qt_send;
        P2P_Event_Key key(*record);
        std::list<MAT_Record *>::iterator req_it = matching_queues[queue_type][rank][key].begin();
        std::list<MAT_Record *>::iterator peer_it;
        if (req_it != matching_queues[queue_type][rank][key].end()) {
            if (match_peer(matching_queues[peer_queue_type], peer_it, **req_it)) {
                std::list<MAT_Record *>::iterator lP = ((*req_it)->is_send) ? req_it : peer_it;
                std::list<MAT_Record *>::iterator rP = ((*req_it)->is_send) ? peer_it : req_it;
                MAT_Record *lpr = *lP;
                MAT_Record *rpr = *rP;
                print_P2PF(*lpr, *rpr);
                lpr->peer_assigned = rpr->peer_assigned = true;
                if (lpr->request > 0) {
                    std::list<MAT_Record *>::iterator it;
                    if (find_event(qt_normal, lpr->rank, lpr, it)) {
                        request_queues[qt_normal][lpr->rank][lpr->request].erase(it);
                    } else {
                        assert(0);
                    }
                    request_queues[qt_hanged][lpr->rank][lpr->request].push_back(lpr);
                }
                if (rpr->request > 0) {
                    assert(0);
                }
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

bool P2PAL::add_pers_req_start(const PEAT_Rank rank, MAT_Record *record)
{
    if (record->peer_rank == com_store->get_MPI_PROC_NULL()) {
        return false;
    }
    /*
    if (req_it->peer_rank == com_store->get_MPI_ANY_SOURCE() || req_it->tag == com_store->get_MPI_ANY_TAG()) {
        waiting_events[rank].push_back(req_it);
        return true;
    }*/
    P2P_Event_Key key(*record);
    if (global_func_id.check_property(record->ph[0].func_id, PEAT_PROP_DATA_RX)) {
        if (waiting_events[rank].empty() && record->request == 0) {
            assert(0); // never happen
            matching_queues[qt_recv][rank][key].push_back(record);
        } else {
            waiting_events[rank].push_back(record);
#ifdef USE_FAST_CHECK
            assert(record->request);
            waiting_events_f[rank][record->request] = record;
#endif
#if 1 // todo check code below
            for (std::list<MAT_Record *>::iterator event_it = waiting_events.at(rank).begin(); event_it != waiting_events.at(rank).end();) {
                PEAT_MPI_Request req = (*event_it)->request;
                if (req > 0)
                    break;
                P2P_Event_Key key(**event_it);
                matching_queues[qt_recv][rank][key].push_back(*event_it);
                event_it = waiting_events.at(rank).erase(event_it);
            }
#endif
        }
    } else if (global_func_id.check_property(record->ph[0].func_id, PEAT_PROP_DATA_TX)) {
        matching_queues[qt_send][rank][key].push_back(record);
        if (!request_queues[qt_normal][rank][record->request].empty()) {
            MAT_Record *r = request_queues[qt_normal][rank].at(record->request).front();
            assert(record->is_send);
            request_queues[qt_hanged][rank][record->request].push_back(r);
        }
        request_queues[qt_normal][rank][record->request].push_back(record);
    } else {
        std::cerr << "Weird persistent request." << std::endl;
        return false;
    }
    return true;
}
PEAT_Bool P2PAL::remove_pers_req(const PEAT_Rank rank, const PEAT_MPI_Request request, MAT_Record **req_it)
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
PEAT_Bool P2PAL::remove_req(const PEAT_Rank rank, const PEAT_MPI_Request request)
{
    MAT_Record *req_it;
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

void P2PAL::parse_P2PTx(PEAT_Rank rank, const Tractor_Event &event, bool print_out)
{
    assert(ptp);
    assert(com_store);
    const PEAT_MPI_Comm comm = !global_func_id.check_property(event.func_id, PEAT_PROP_COMM) ? PEAT_MPI_Comm(2) : event.P2P.comm;
    if (print_out)
        ptp->add_rank_MPI(rank, event.tsc_time.func.out - event.tsc_time.func.in);
    if (event.P2P.peer != com_store->get_MPI_PROC_NULL()) {
        MAT_Record *record = new (MAT_Record);
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
        Phase ph_tx;
        ph_tx.func_id = event.func_id;
        ph_tx.thread_id = event.thread_id;
        ph_tx.tsc_time = event.tsc_time;
        ph_tx.tsc_u_time = event.tsc_u_time;
        ph_tx.addrs = event.addrs;
        ph_tx.primary_id = event.primary_id;
        ph_tx.secondary_id = event.secondary_id;
        record->ph[0] = record->ph[1] = record->ph[2] = ph_tx;
        if (print_out)
            print_P2PL(event);
        if (global_func_id.check_property(event.func_id, PEAT_PROP_PERSISTENT_REQUEST_INIT)) { // add persistent request to queue and finish
            request_queues[qt_pers][rank][record->request].push_back(record);
            return;
        }
        if (record->request)
            request_queues[qt_normal][rank][record->request].push_back(record);
        P2P_Event_Key key(*record);
        matching_queues[qt_send][rank][key].push_back(record);
        std::list<MAT_Record *>::iterator matching_rec_it;
        std::list<MAT_Record *>::iterator record_it = matching_queues[qt_send][rank][key].begin();
        if (record_it != matching_queues[qt_send][rank][key].end()) {
            if (match_peer(matching_queues[qt_recv], matching_rec_it, **record_it)) {
                MAT_Record *lP = *record_it;
                MAT_Record *rP = *matching_rec_it;
                P2P_Event_Key send_key(*lP);
                P2P_Event_Key recv_key(*rP);
                lP->peer_assigned = rP->peer_assigned = true;
                print_P2PF(*lP, *rP);
                PEAT_Rank send_rank = lP->rank;
                PEAT_Rank recv_rank = rP->rank;

                if (lP->request > 0) {
                    std::list<MAT_Record *>::iterator it;
                    ;
                    if (find_event(qt_normal, send_rank, lP, it)) {
                        request_queues[qt_normal][send_rank][lP->request].erase(it);
                    } else {
                        assert(0);
                    }
                    request_queues[qt_hanged][send_rank][lP->request].push_back(lP);
                } else {
                    delete lP;
                }
                matching_queues[qt_send][send_rank][send_key].erase(record_it);
                if (rP->request > 0) {
                    assert(0);
                } else {
                    delete rP;
                }
                matching_queues[qt_recv][recv_rank][recv_key].erase(matching_rec_it);
            }
        }
    } else {
        if (print_out)
            print_P2PL(event);
        if (event.P2P.request > 0) {
            if (global_func_id.check_property(event.func_id, PEAT_PROP_PERSISTENT_REQUEST_INIT)) {
                if (thrown_away_pers_requests[rank].find(event.P2P.request) != thrown_away_pers_requests[rank].end()) {
                    std::cerr << "Persistent request " << std::hex << event.P2P.request << std::dec << " conflict.Rank: " << rank << ". primary_id: " << event.primary_id << " secondary_id: " << event.secondary_id << std::endl;
                };
                thrown_away_pers_requests[rank].insert(event.P2P.request);
                std::cerr << "Invalid persistent request " << std::hex << event.P2P.request << std::dec << " thrown away.Rank: " << rank << ". primary_id: " << event.primary_id << " secondary_id: " << event.secondary_id << std::endl;
            } else {
                if (thrown_away_requests[rank].find(event.P2P.request) != thrown_away_requests[rank].end()) {
                    std::cerr << "Request " << std::hex << event.P2P.request << std::dec << " conflict.Rank: " << rank << ". primary_id: " << event.primary_id << " secondary_id: " << event.secondary_id << std::endl;
                }
                thrown_away_requests[rank].insert(event.P2P.request);
                std::cerr << "Invalid request " << std::hex << event.P2P.request << std::dec << " thrown away.Rank: " << rank << ". primary_id: " << event.primary_id << " secondary_id: " << event.secondary_id << std::endl;
            }
        }
    }
}
void P2PAL::parse_P2PRx(PEAT_Rank rank, const Tractor_Event &event, bool increase_MPI)
{
    assert(ptp);
    assert(com_store);
    const PEAT_MPI_Comm comm = !global_func_id.check_property(event.func_id, PEAT_PROP_COMM) ? 2 : event.P2P.comm;
    if (increase_MPI)
        ptp->add_rank_MPI(rank, event.tsc_time.func.out - event.tsc_time.func.in);
    if (event.P2P.peer != com_store->get_MPI_PROC_NULL()) {
        MAT_Record *record = new (MAT_Record);
        record->pc_level = event.PControl.ctl;
        record->comm = comm;
        record->rank = rank;
        record->peer_rank = event.P2P.peer;
        record->tag = event.P2P.tag;
        record->size = event.P2P.transfered_bytes;
        record->is_send = false;
        if (event.P2P.request) {
            record->request = event.P2P.request;
        }
        Phase ph_rx;
        ph_rx.func_id = event.func_id;
        ph_rx.thread_id = event.thread_id;
        ph_rx.tsc_time = event.tsc_time;
        ph_rx.tsc_u_time = event.tsc_u_time;
        ph_rx.addrs = event.addrs;
        ph_rx.primary_id = event.primary_id;
        ph_rx.secondary_id = event.secondary_id;
        record->ph[0] = record->ph[1] = record->ph[2] = ph_rx;
        print_P2PL(event);
        if (global_func_id.check_property(event.func_id, PEAT_PROP_PERSISTENT_REQUEST_INIT)) { // add persistent request to queue and finish
            request_queues[qt_pers][rank][record->request].push_back(record);
            return;
        }
        if (record->request > 0) {
            request_queues[qt_normal][rank][record->request].push_back(record);
            waiting_events[rank].push_back(record);
#ifdef USE_FAST_CHECK
            assert(record->request);
            waiting_events_f[rank][record->request] = record;
#endif
            return;
        }

        if (waiting_events[rank].empty()) {
            P2P_Event_Key key(*record);
            matching_queues[qt_recv][rank][key].push_back(record);
            std::list<MAT_Record *>::iterator record_it = matching_queues[qt_recv][rank][key].begin();
            std::list<MAT_Record *>::iterator matching_rec_it;

            if (record_it != matching_queues[qt_recv][rank][key].end()) {
                if (match_peer(matching_queues[qt_send], matching_rec_it, **record_it)) {
                    MAT_Record *lP = *matching_rec_it;
                    MAT_Record *rP = *record_it;
                    P2P_Event_Key send_key(*lP);
                    P2P_Event_Key recv_key(*rP);
                    print_P2PF(*lP, *rP);
                    PEAT_Rank send_rank = lP->rank;
                    PEAT_Rank recv_rank = rP->rank;
                    lP->peer_assigned = rP->peer_assigned = true;
                    if (lP->request > 0) {
                        std::list<MAT_Record *>::iterator it;
                        ;
                        if (find_event(qt_normal, send_rank, lP, it)) {
                            request_queues[qt_normal][send_rank][lP->request].erase(it);
                        } else {
                            assert(0);
                        }
                        request_queues[qt_hanged][send_rank][lP->request].push_back(lP);
                    } else {
                        delete lP;
                    }
                    matching_queues[qt_send][send_rank][send_key].erase(matching_rec_it);
                    if (rP->request > 0) {
                        assert(0);
                    } else {
                        delete rP;
                    }
                    matching_queues[qt_recv][recv_rank][recv_key].erase(record_it);
                }
            }
        } else {
            waiting_events[rank].push_back(record);
        }
    } else {
        print_P2PL(event);
        if (event.P2P.request > 0) {
            if (global_func_id.check_property(event.func_id, PEAT_PROP_PERSISTENT_REQUEST_INIT)) {
                if (thrown_away_pers_requests[rank].find(event.P2P.request) != thrown_away_pers_requests[rank].end()) {
                    std::cerr << "Persistent request " << std::hex << event.P2P.request << std::dec << " conflict.Rank: " << rank << ". primary_id: " << event.primary_id << " secondary_id: " << event.secondary_id << std::endl;
                };
                thrown_away_pers_requests[rank].insert(event.P2P.request);
                std::cerr << "Invalid persistent request " << std::hex << event.P2P.request << std::dec << " thrown away.Rank: " << rank << ". primary_id: " << event.primary_id << " secondary_id: " << event.secondary_id << std::endl;
            } else {
                if (thrown_away_requests[rank].find(event.P2P.request) != thrown_away_requests[rank].end()) {
                    std::cerr << "Request " << std::hex << event.P2P.request << std::dec << " conflict.Rank: " << rank << ". primary_id: " << event.primary_id << " secondary_id: " << event.secondary_id << std::endl;
                }
                thrown_away_requests[rank].insert(event.P2P.request);
                std::cerr << "Invalid request " << std::hex << event.P2P.request << std::dec << " thrown away.Rank: " << rank << ". primary_id: " << event.primary_id << " secondary_id: " << event.secondary_id << std::endl;
            }
        }
    }
}

void P2PAL::parse_Local(PEAT_Rank rank, const Tractor_Event &event)
{
    assert(ptp);
    assert(com_store);
    if (!is_omp(event.func_class))
        ptp->add_rank_MPI(rank, event.tsc_time.func.out - event.tsc_time.func.in);
#if 0
    const PEAT_MPI_Comm comm = 0;// !global_func_id.prop_by_idx(event.func_id, PEAT_PROP_COMM) ? PEAT_MPI_Comm(Communicators::rc_mpi_comm_world) : com_store->get_uniq_comm(event.Local.comm, rank, event.tsc_time.func.in);
    MAT_Record record;
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
    assert(com_store);
    ptp->add_rank_MPI(rank, event.tsc_time.func.out - event.tsc_time.func.in);
#if 0
    const PEAT_MPI_Comm comm = !global_func_id.check_property(event.func_id, PEAT_PROP_COMM) ? 2 : event.P2P.comm;
    PEAT_MPI_Status flag = event.P2P.flag;
    MAT_Record record;
    record.pc_level = ctl;
    record.comm = comm;
    record.rank = rank;
    record.peer_rank = event.P2P.peer;//com_store->get_rank_from_comm(comm, rank, event.P2P.peer, event.tsc_time.func.in);
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
    if (event.P2P.peer == com_store->get_MPI_PROC_NULL()) {
        print_P2PL(event);
        // parse_Local(rank, event);
        if (event.P2P.request > 0) {
            if (global_func_id.check_property(event.func_id, PEAT_PROP_PERSISTENT_REQUEST_INIT)) {
                if (thrown_away_pers_requests[rank].find(event.P2P.request) != thrown_away_pers_requests[rank].end()) {
                    std::cerr << "Persistent request " << std::hex << event.P2P.request << std::dec << " conflict.Rank: " << rank << ". primary_id: " << event.primary_id << " secondary_id: " << event.secondary_id << std::endl;
                };
                thrown_away_pers_requests[rank].insert(event.P2P.request);
                std::cerr << "Invalid persistent request " << std::hex << event.P2P.request << std::dec << " thrown away.Rank: " << rank << ". primary_id: " << event.primary_id << " secondary_id: " << event.secondary_id << std::endl;
            } else {
                if (thrown_away_requests[rank].find(event.P2P.request) != thrown_away_requests[rank].end()) {
                    std::cerr << "Request conflict " << std::hex << event.P2P.request << std::dec << " conflict.Rank: " << rank << ". primary_id: " << event.primary_id << " secondary_id: " << event.secondary_id << std::endl;
                }
                thrown_away_requests[rank].insert(event.P2P.request);
                std::cerr << "Invalid request " << std::hex << event.P2P.request << std::dec << " thrown away.Rank: " << rank << ". primary_id: " << event.primary_id << " secondary_id: " << event.secondary_id << std::endl;
            }
        }
        return;
    }
    assert(event.P2P.peer >= 0);
    // const PEAT_MPI_Comm comm = !global_func_id.check_property(event.func_id, PEAT_PROP_COMM) ? 2 : event.P2P.comm;
    PEAT_Rank peer = event.P2P.peer; // com_store->get_rank_from_comm(comm, rank, event.P2P.peer, event.tsc_in);
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
    record.primary_id = event.primary_id;
    record.secondary_id = event.secondary_id;

    /*
    record.ph2.func_id = event.func_id;
    record.ph2.thread_id = event.thread_id;
    record.ph2.tsc_time = event.tsc_time;
    record.ph2.tsc_u_time = event.tsc_u_time;
    record.ph2.addrs = event.addrs;
    record.ph2.primary_id = event.primary_id;
    record.ph2.secondary_id = event.secondary_id;
    */
    if (event.P2P.request > 0) {
        record.request = event.P2P.request;
        if (global_func_id.check_property(event.func_id, PEAT_PROP_PERSISTENT_REQUEST_INIT)) {
            flown_away_pers_requests[rank][record.request] = record;
        } else {
            flown_away_requests[rank][record.request].push_back(record);
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
    // fprintf(stdout, "Rank %d: Received p2p (%lld,%d,%llx) from rank %d\n", my_rank, temp_event.primary_id, temp_event.P2P.peer, temp_event.P2P.request, temp_event.rank);
}
#endif

void P2PAL::process_unbound_events()
{
    for (matching_event_type::iterator rank_it = matching_queues[qt_send].begin(); rank_it != matching_queues[qt_send].end(); ++rank_it) {
        for (std::map<P2P_Event_Key, std::list<MAT_Record *>>::iterator send_it = rank_it->begin(); send_it != rank_it->end(); ++send_it) {
            for (std::list<MAT_Record *>::iterator event_it = send_it->second.begin(); event_it != send_it->second.end();) {
                std::list<MAT_Record *>::iterator matching_rec_it;
                if (match_peer(matching_queues[qt_recv], matching_rec_it, **event_it)) {
                    PEAT_Rank send_rank = (*event_it)->rank;
                    PEAT_Rank recv_rank = (*event_it)->peer_rank;
                    print_P2PF(**event_it, **matching_rec_it);
                    P2P_Event_Key send_key(**event_it), recv_key(**matching_rec_it);
                    (*event_it)->peer_assigned = (*matching_rec_it)->peer_assigned = true;
                    if ((*event_it)->request > 0) {
                        request_queues[qt_hanged][(*event_it)->rank][(*event_it)->request].push_back(*event_it);
                    } else {
                        delete *event_it;
                    }
                    if ((*matching_rec_it)->request > 0) {
                        assert(0);
                    } else {
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
        for (std::map<P2P_Event_Key, std::list<MAT_Record *>>::iterator recv_it = rank_it->begin(); recv_it != rank_it->end(); ++recv_it) {
            for (std::list<MAT_Record *>::iterator event_it = recv_it->second.begin(); event_it != recv_it->second.end();) {
                std::list<MAT_Record *>::iterator matching_rec_it;
                if (match_peer(matching_queues[qt_send], matching_rec_it, **event_it)) {
                    PEAT_Rank send_rank = (*event_it)->peer_rank;
                    PEAT_Rank recv_rank = (*event_it)->rank;
                    print_P2PF(**matching_rec_it, **event_it);
                    (*event_it)->peer_assigned = (*matching_rec_it)->peer_assigned = true;
                    P2P_Event_Key send_key(**matching_rec_it), recv_key(**event_it);
                    if ((*event_it)->request > 0) {
                        request_queues[qt_hanged][(*event_it)->rank][(*event_it)->request].push_back(*event_it);
                    } else {
                        delete *event_it;
                    }
                    if ((*matching_rec_it)->request > 0) {
                        assert(0);
                    } else {
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
        for (std::map<P2P_Event_Key, std::list<MAT_Record *>>::iterator send_it = rank_it->begin(); send_it != rank_it->end(); ++send_it) {
            for (std::list<MAT_Record *>::iterator event_it = send_it->second.begin(); event_it != send_it->second.end();) {
                std::list<MAT_Record *>::iterator matching_rec_it;

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
        for (std::map<P2P_Event_Key, std::list<MAT_Record *>>::iterator recv_it = rank_it->begin(); recv_it != rank_it->end(); ++recv_it) {
            for (std::list<MAT_Record *>::iterator event_it = recv_it->second.begin(); event_it != recv_it->second.end();) {
                std::list<MAT_Record *>::iterator matching_rec_it;
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
    printf("P2P Queues:\nRequest");
    for (int q = 0; q < qt_size2; q++) {
        printf("\n%s", q_name_s2[q].c_str());
        PEAT_Rank rank = 0;
        for (request_queues_type::const_iterator request_queues_it = request_queues[q].cbegin(); request_queues_it != request_queues[q].cend(); ++request_queues_it, rank++) {
            size_t size = 0;
            for (std::map<PEAT_MPI_Request, std::list<MAT_Record *>>::const_iterator request_it = request_queues_it->cbegin(); request_it != request_queues_it->cend(); ++request_it) {
                size += request_it->second.size();
            }
            if (size > 0)
                printf(":r%d-%zd", rank, size);
        }
    }
    printf("\nMatching");
    for (int q = 0; q < qt_size; q++) {
        printf("\n%s", q_name_s[q].c_str());
        PEAT_Rank rank = 0;
        for (matching_event_type::const_iterator matching_queues_it = matching_queues[q].cbegin(); matching_queues_it != matching_queues[q].cend(); ++matching_queues_it, rank++) {
            size_t size = 0;
            for (std::map<P2P_Event_Key, std::list<MAT_Record *>>::const_iterator key_it = matching_queues_it->cbegin(); key_it != matching_queues_it->cend(); ++key_it) {
                size += key_it->second.size();
            }
            if (size > 0)
                printf(":r%d-%zd", rank, size);
        }
    }
    PEAT_Rank rank = 0;
    printf("\nWaiting ");
    for (waiting_event_type::const_iterator waiting_event_it = waiting_events.cbegin(); waiting_event_it != waiting_events.cend(); ++waiting_event_it, rank++) {
        size_t size = 0;
        size += waiting_event_it->size();
        if (size > 0)
            printf(":r%d-%zd", rank, size);
    }
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
        for (std::map<P2PAL::P2P_Event_Key, std::list<MAT_Record *>>::const_iterator queue_it = rank_it->cbegin(); queue_it != rank_it->cend();) {
            for (std::list<MAT_Record *>::const_iterator event_it = queue_it->second.cbegin(); event_it != queue_it->second.cend();) {
                dbg << **event_it;
            }
        }
    }
    return dbg;
}

std::ostream &operator<<(std::ostream &dbg, const MAT_Record &rec)
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
    dbg << std::dec << "{" << global_func_id.name_by_idx(ph.func_id) << "," << ph.primary_id << "," << ph.secondary_id << "," << ph.tsc_time.func.in << "," << ph.tsc_time.func.out << "}";
    return dbg;
}

std::ostream &operator<<(std::ostream &dbg, const Record &rec)
{
    dbg << "{r" << rec.rank << ",s" << rec.size << ",t" << rec.tag << ",c" << rec.comm << "," << rec.ph[0] << "," << rec.ph[1] << "," << rec.ph[2] << "}";
    return dbg;
}

std::ostream &operator<<(std::ostream &dbg, const std::map<PEAT_Rank, std::list<Record>> &rec)
{
    dbg << "\n<<";
    for (std::map<PEAT_Rank, std::list<Record>>::const_iterator it = rec.begin(); it != rec.end(); ++it) {
        dbg << "Rank " << it->first << it->second;
    }
    dbg << ">>" << std::endl;
    return dbg;
}
