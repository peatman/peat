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
#include "request_store.h"
#include "mat_locks.h"

#include <list>
#include <map>
#include <memory>
#include <string>
#include <vector>

#define INITIAL_ABF 10
#define USE_FAST_CHECK
class Requests;
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
        P2P_Event_Key(MAT_P2P_Record &r) : tag(r.tag), peer(r.peer_rank), message(r.message), comm(r.comm) {};
        PEAT_Bool operator<(const P2P_Event_Key &s) const
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
    P2PAL(File_Storage_GZ *f, int s, int pos,
#ifdef PEAT_MPI
          std::list<PEAT_Event_Delivery> *ad, const std::vector<PEAT_Rank> &wR, const std::vector<PEAT_Rank> &mRs, const MPI_Comm mat_cc, const PEAT_Rank m_r,
#endif
          int verbose = false, const std::string &begline = "MSG", const char delim = '|');
    virtual ~P2PAL();
    P2PAL(const P2PAL &) = delete;
    P2PAL &operator=(const P2PAL &) = delete;

    void initialize(PTP *p, Communicators *c, Request_Store *rs, Requests *r, GPU *g, File_Storage_GZ *MPI, File_Storage_GZ *HWC, File_Storage_GZ *bind);

    void parse_P2PTx(PEAT_Rank rank, const Tractor_Event &event, PEAT_Bool print_out);
    void parse_P2PRx(PEAT_Rank rank, const Tractor_Event &event, PEAT_Bool increase_MPI = true);
    void parse_P2PPb(PEAT_Rank rank, const Tractor_Event &event);

    void parse_Local(PEAT_Rank rank, const Tractor_Event &event);

    void write_HNT() const;
    PEAT_Bool empty() const;
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
    PEAT_Bool is_rank_blocked(const PEAT_Rank rank) {
        return (queue_size_at_rank(rank, qt_send) > adaptive_blocking_threshold || queue_size_at_rank(rank, qt_recv) > adaptive_blocking_threshold);
    }
    PEAT_Num increase_abf(void) { return ++adaptive_blocking_threshold; }
    */
    std::string q_name_s[qt_size];
    std::map<int, std::map<PEAT_P2P_Record, PEAT_Single_Event_Clock>> printed_p2p;

    typedef std::vector<std::map<PEAT_MPI_Request, std::list<MAT_P2P_Record *>>> request_queues_type;
    typedef std::vector<std::map<P2P_Event_Key, std::list<MAT_P2P_Record *>>> matching_event_type;

    friend std::ostream &operator<<(std::ostream &dbg, const P2PAL &rec);
    friend std::ostream &operator<<(std::ostream &dbg, const P2PAL::matching_event_type &queue);

  private:
    // typedef std::map<PEAT_Rank, std::map<PEAT_MPI_Request, std::list<MAT_P2P_Record> > >queues_type;
    matching_event_type matching_queues[qt_size]; // send - 0 recv - 1
    std::map<PEAT_Rank, std::map<PEAT_MPI_Message, PEAT_MPI_Translation_Message>> matching_messages;
    PTP *ptp;
    GPU *gpu;
    Communicators* comm_store;
    Request_Store* req_store;
    Requests* request_func_store;
    int verbose;
    // PEAT_Num adaptive_blocking_threshold;
  public:
    void resize_queues(size_t size)
    {
        matching_queues[0].resize(size);
        matching_queues[1].resize(size);
        held_receives.resize(size);
#ifdef PEAT_OMP
        rank_locks.reset(new Mat_Spin_Lock[size]);
        n_rank_locks = static_cast<PEAT_Rank>(size);
        for (PEAT_Rank r = 0; r < static_cast<PEAT_Rank>(size); r++) // every rank's entry exists before the threads start
            matching_messages[r];
#endif
    }
    /* Wildcard receives. A nonblocking receive posted with MPI_ANY_SOURCE and/or MPI_ANY_TAG learns its source and tag
       only from its completion, and until then it could take any message on its communicator that a later receive
       would otherwise get. So from its posting on, every receive of that rank on that communicator is held back (in
       posting order) until all wildcard receives ahead of it are resolved; then they enter the matching queues in
       posting order and are paired with the sends already waiting. Sends are never held. */
    void drop_wildcard_barrier(const PEAT_Rank rank, MAT_P2P_Record *record); // cancelled/freed before it completed

  private:
#ifdef PEAT_OMP
    /* mat.omp: a rank's matching queues are guarded by its lock (see mat_locks.h); pairing a send of s with a receive
       of r holds both. The P2P statistics and bind counters have their own. */
    std::unique_ptr<Mat_Spin_Lock[]> rank_locks;
    PEAT_Rank n_rank_locks = 0;
    std::mutex stats_lock;
#endif
    /* holds the locks of one rank or of a pair, the lower rank first; ranks that aren't ranks (MPI_ANY_SOURCE,
       MPI_PROC_NULL, ...) are skipped. Nothing in serial mat. */
    class Rank_Guard
    {
#ifdef PEAT_OMP
        P2PAL &p2p;
        PEAT_Rank first, second;
        PEAT_Rank valid(PEAT_Rank r) const { return r >= 0 && r < p2p.n_rank_locks ? r : -1; }

      public:
        Rank_Guard(P2PAL &p, PEAT_Rank a, PEAT_Rank b = -1) : p2p(p)
        {
            a = valid(a);
            b = valid(b);
            if (a == b)
                b = -1;
            first = (a >= 0 && (b < 0 || a < b)) ? a : b;
            second = first == a ? b : a;
            if (first >= 0)
                p2p.rank_locks[first].lock();
            if (second >= 0)
                p2p.rank_locks[second].lock();
        }
        ~Rank_Guard()
        {
            if (second >= 0)
                p2p.rank_locks[second].unlock();
            if (first >= 0)
                p2p.rank_locks[first].unlock();
        }
#else
      public:
        Rank_Guard(P2PAL &, PEAT_Rank, PEAT_Rank = -1) {}
#endif
        Rank_Guard(const Rank_Guard &) = delete;
        Rank_Guard &operator=(const Rank_Guard &) = delete;
    };
    std::vector<std::map<PEAT_MPI_Comm, std::list<MAT_P2P_Record *>>> held_receives; // [receiving rank][comm], posting order
    PEAT_Bool unresolved_wildcard(const MAT_P2P_Record &r) const { return !r.is_send && r.request > 0 && is_wild_card(r); }
    PEAT_Bool hold_receive(const PEAT_Rank rank, MAT_P2P_Record *record);
    void release_receives(const PEAT_Rank rank, const PEAT_MPI_Comm comm);
    void release_all_receives(); // end of the trace: whatever is still held goes to the queues as it is
    void match_waiting_sends(const PEAT_Rank recv_rank, const MAT_P2P_Record &recv);
    void bind_pair(std::list<MAT_P2P_Record *>::iterator send_it, std::list<MAT_P2P_Record *>::iterator recv_it);

  public:
    void print_P2PL(const Tractor_Event &event);
    void print_P2PE(const MAT_P2P_Record &event);
    void print_P2PF(const MAT_P2P_Record &left, const MAT_P2P_Record &right);
    void add_imbalance(const MAT_P2P_Record &left, const MAT_P2P_Record &right, PEAT_Single_Event_Clock &left_clock, PEAT_Single_Event_Clock &right_clock);
    void print_Local(const Tractor_Event &event);
    PEAT_Bool update_request(const PEAT_Rank rank, const Request_Store_Record_Type type, const PEAT_MPI_Tag tag, const PEAT_Rank source, const PEAT_Message_Size size, PEAT_MPI_Request request, const Phase &phase, PEAT_Bool is_remote);
    PEAT_Bool match_peer(matching_event_type &queues, std::list<MAT_P2P_Record *>::iterator &ret_it, const MAT_P2P_Record &input_record) const;
    PEAT_Bool add_pers_req_start(const PEAT_Rank rank, PEAT_MPI_Func_ID init_func_id, MAT_P2P_Record *record);
    void process_unbound_events(PEAT_Rank my_rank);
    PEAT_Signed_Num check_unbound_events();
    PEAT_Bool is_wild_card(const MAT_P2P_Record &r) const
    {
        return (r.peer_rank == comm_store->get_MPI_ANY_SOURCE() || r.tag == comm_store->get_MPI_ANY_TAG());
    }

#ifdef PEAT_MPI
    // void print_P2PR(int size, int src);
    /*
    PEAT_Bool get_flown_away_pers_req(const PEAT_Rank rank, const PEAT_MPI_Request request, Phase& ph, PEAT_Bool del);
    PEAT_Bool get_flown_away_req(const PEAT_Rank rank, const PEAT_MPI_Request request, Phase& ph, PEAT_Bool del);
    */
    std::list<PEAT_Event_Delivery> *active_deliveries;
    MPI_Comm mat_compute_comm;
    std::vector<PEAT_Rank> whole_ranks;
    std::vector<PEAT_Rank> my_ranks;
    PEAT_Rank my_rank;

  public:
    void send_P2PTx(PEAT_Rank rank, const Tractor_Event &event);
    void process_remote_event(PEAT_Signed_Num size, PEAT_Rank src);
    PEAT_Num check_completions();
#endif

    friend class Request;
};
std::ostream &operator<<(std::ostream &dbg, const MAT_P2P_Record &rec);

#endif // __P2PAL_H__
