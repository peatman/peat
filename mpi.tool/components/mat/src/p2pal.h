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

#ifndef __P2PAL_H__
#define __P2PAL_H__
#include "communicators.h"
#include "func_id.h"
#include "gpu.h"
#include "ptp.h"
#include "records.h"
#include "tevent.h"

#include <list>
#include <map>
#include <string>
#include <vector>

#define INITIAL_ABF 10
#define USE_FAST_CHECK

class P2PAL : public Outputs
{
  public:
    struct P2P_Event_Key {
        PEAT_MPI_Tag tag;
        PEAT_Rank peer;
        PEAT_MPI_Message message;
        PEAT_MPI_Comm comm;
        P2P_Event_Key(PEAT_MPI_Tag t, PEAT_Rank p, PEAT_MPI_Message m, PEAT_MPI_Comm c) : tag(t), peer(p), message(m), comm(c) {
                                                                                          };
        P2P_Event_Key(MAT_Record &r) : tag(r.tag), peer(r.peer_rank), message(r.message), comm(r.comm) {};
        bool operator<(const P2P_Event_Key &s) const
        {
            if (tag < s.tag)
                return true;
            if (tag > s.tag)
                return false;
            if (peer < s.peer)
                return true;
            if (peer > s.peer)
                return false;
            if (comm < s.comm)
                return true;
            if (comm > s.comm)
                return false;
            if (message < s.message)
                return true;
            if (message > s.message)
                return false;
            return false;
        }
    };

    File_Storage_GZ *input_MPI;
    std::vector<PEAT_LNum> messages_bound;
    int skip_locals, print_on_send;
    P2PAL(PTP *p, Communicators *c, File_Storage_GZ *f, GPU *g, int s, int pos,
#ifdef PEAT_MPI
          std::list<PEAT_Event_Delivery> *ad, const std::vector<PEAT_Rank> &wR, const std::vector<PEAT_Rank> &mRs, const MPI_Comm mat_cc, const PEAT_Rank m_r,
#endif
          int verbose = false, const std::string &begline = "MSG", const char delim = '|');
    virtual ~P2PAL();
    // matching_queues/request_queues/waiting_events* own raw MAT_Record* that are explicitly deleted elsewhere;
    // a shallow copy of this class would duplicate those pointers across two instances and cause double-free.
    P2PAL(const P2PAL &) = delete;
    P2PAL &operator=(const P2PAL &) = delete;

    void parse_P2PTx(PEAT_Rank rank, const Tractor_Event &event, bool print_out);
    void parse_P2PRx(PEAT_Rank rank, const Tractor_Event &event, bool increase_MPI = true);
    void parse_P2PPb(PEAT_Rank rank, const Tractor_Event &event);

    void parse_Local(PEAT_Rank rank, const Tractor_Event &event);

    void write_HNT() const;
    bool empty() const;
    PEAT_Num queue_size_at_rank(const PEAT_Rank rank, const PEAT_Num qN) const
    {
        return matching_queues[qN].at(rank).size();
        return 0;
    }
    void print_sizes() const;
    PEAT_Bool check_tx(PEAT_MPI_Func_ID func_id) const
    {
        return (global_func_id.check_property(func_id, PEAT_PROP_DATA_TX));
    }
    PEAT_Bool check_rx(PEAT_MPI_Func_ID func_id) const
    {
        return (global_func_id.check_property(func_id, PEAT_PROP_DATA_RX));
    }
    /*
    bool is_rank_blocked(const PEAT_Rank rank) {
        return (queue_size_at_rank(rank, qt_send) > adaptive_blocking_threshold || queue_size_at_rank(rank, qt_recv) > adaptive_blocking_threshold);
    }
    PEAT_Num increase_abf(void) { return ++adaptive_blocking_threshold; }
    */
    std::string q_name_s[qt_size];
    std::string q_name_s2[qt_size2];
    std::map<int, std::map<PEAT_P2P_Record, PEAT_Single_Event_Clock>> printed_p2p;

    typedef std::vector<std::map<PEAT_MPI_Request, std::list<MAT_Record *>>> request_queues_type;
    typedef std::vector<std::map<P2P_Event_Key, std::list<MAT_Record *>>> matching_event_type;
    typedef std::vector<std::list<MAT_Record *>> waiting_event_type;
    typedef std::vector<std::map<PEAT_MPI_Request, MAT_Record *>> waiting_event_type_f;

    friend std::ostream &operator<<(std::ostream &dbg, const P2PAL &rec);
    friend std::ostream &operator<<(std::ostream &dbg, const P2PAL::matching_event_type &queue);
    bool find_event(int queue_type, PEAT_Rank rank, MAT_Record *rec, std::list<MAT_Record *>::iterator &ret_it);

  private:
    // typedef std::map<PEAT_Rank, std::map<PEAT_MPI_Request, std::list<MAT_Record> > >queues_type;
    request_queues_type request_queues[qt_size2]; // persistent - 0 hanged - 1 normal -  2 (hanged and normal for sends only)
    matching_event_type matching_queues[qt_size]; // send - 0 recv - 1
    waiting_event_type waiting_events;            // for recv only
    std::map<PEAT_Rank, std::map<PEAT_MPI_Message, PEAT_MPI_Translation_Message>> matching_messages;
#ifdef USE_FAST_CHECK
    waiting_event_type_f waiting_events_f; // for recv only
#endif
    PTP *ptp;
    GPU *gpu;
    Communicators *com_store;

    int verbose;
    // PEAT_Num adaptive_blocking_threshold;
  public:
    void resize_queues(size_t size)
    {
        matching_queues[0].resize(size);
        matching_queues[1].resize(size);
        request_queues[0].resize(size);
        request_queues[1].resize(size);
        request_queues[2].resize(size);
        waiting_events.resize(size);
#ifdef USE_FAST_CHECK
        waiting_events_f.resize(size);
#endif
    }
    void print_P2PL(const Tractor_Event &event);
    void print_P2PE(const MAT_Record &event);
    void print_P2PF(const MAT_Record &left, const MAT_Record &right);
    void add_imbalance(const MAT_Record &left, const MAT_Record &right, PEAT_Single_Event_Clock &left_clock, PEAT_Single_Event_Clock &right_clock);
    void print_Local(const Tractor_Event &event);
    bool find_request(PEAT_Num queue_type, PEAT_Rank rank, PEAT_MPI_Request request, MAT_Record **ret_it, PEAT_Bool erase);
    bool update_request(PEAT_Num queue_type, PEAT_Rank rank, const PEAT_MPI_Tag tag, const PEAT_Rank source, const PEAT_Message_Size size, PEAT_MPI_Request request, const Phase &phase, MAT_Record *record);
    int update_waiting_queue(PEAT_Rank rank, const PEAT_MPI_Tag tag, const PEAT_Rank source, const PEAT_Message_Size size, PEAT_MPI_Request request, const Phase &phase, MAT_Record **req_it);
    MAT_Record *check_waiting_queue(PEAT_Rank rank, PEAT_MPI_Request request);
    bool match_peer(matching_event_type &queues, std::list<MAT_Record *>::iterator &ret_it, const MAT_Record &input_record) const;
    bool add_pers_req_start(const PEAT_Rank rank, MAT_Record *record);
    PEAT_Bool remove_pers_request(const PEAT_Rank rank, const PEAT_MPI_Request request, MAT_Record **req_it);
    PEAT_Bool remove_request(const PEAT_Rank rank, const PEAT_MPI_Request request);
    void process_unbound_events();
    PEAT_Signed_Num check_unbound_events();
    bool is_wild_card(const MAT_Record &r) const
    {
        return (r.peer_rank == com_store->get_MPI_ANY_SOURCE() || r.tag == com_store->get_MPI_ANY_TAG());
    }

#ifdef PEAT_MPI
    // void print_P2PR(int size, int src);
    /*
    PEAT_Bool get_flown_away_pers_req(const PEAT_Rank rank, const PEAT_MPI_Request request, Phase& ph, PEAT_Bool del);
    PEAT_Bool get_flown_away_req(const PEAT_Rank rank, const PEAT_MPI_Request request, Phase& ph, PEAT_Bool del);
    */
    std::map<PEAT_Rank, std::map<PEAT_MPI_Request, std::list<MAT_Request_Record>>> flown_away_requests;
    std::map<PEAT_Rank, std::map<PEAT_MPI_Request, MAT_Request_Record>> flown_away_pers_requests;
    std::list<PEAT_Event_Delivery> *active_deliveries;
    MPI_Comm mat_compute_comm;
    std::vector<PEAT_Rank> whole_ranks;
    std::vector<PEAT_Rank> my_ranks;
    PEAT_Rank my_rank;

  public:
    void send_P2PTx(PEAT_Rank rank, const Tractor_Event &event);
    void process_remote_event(PEAT_Signed_Num size, PEAT_Rank src);
    PEAT_Num check_completions();
    std::map<PEAT_Rank, std::set<PEAT_MPI_Request>> thrown_away_requests;
    std::map<PEAT_Rank, std::set<PEAT_MPI_Request>> thrown_away_pers_requests;
#endif

    friend class Request;
};
std::ostream &operator<<(std::ostream &dbg, const MAT_Record &rec);

#endif // __P2PAL_H__
