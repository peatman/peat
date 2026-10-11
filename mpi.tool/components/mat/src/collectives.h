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

#ifndef __COLLECTIVES_H__
#define __COLLECTIVES_H__
#include "common_types.h"
#include "communicators.h"
#include "func_id.h"
#include "ptp.h"
#include "records.h"
#include "tevent.h"
#include "request_store.h"

#include <assert.h>
#include <list>
#include <map>
#include <string>
#include <vector>

#define UPDATE_FIRST_ENTRY

struct Collective_Event { // tsc_u_time is for debugging purposes only!
    Collective_Event() : request(0), permanent_request(0), comm(0) {}
    Collective_Event(const Tractor_Event& s);
    // PEAT_Collective_Bytes send_bytes,recv_bytes;
    //  std::vector<PEAT_Addr> addrs;
    std::vector<PEAT_UID> addrs;
    PEAT_Time tsc_time, tsc_u_time;
    PEAT_MPI_Request request, permanent_request;
    PEAT_MPI_Comm comm;
    Phase ph[3];
};

struct Collective_Entry {
    Collective_Entry() : comm(0), func_id(TMPI_FUNC_ID_NONE), size(0), root(-1), pc_level(-1) {}
    PEAT_MPI_Comm comm;
    PEAT_MPI_Func_ID func_id;
    PEAT_Rank size; // Number of ranks in collective
    PEAT_Rank root;
    int pc_level;
    std::map<PEAT_Rank, Collective_Event*> coll_events_list; // map<source_rank, destination_properties>
    PEAT_Collective_Bytes send_bytes, recv_bytes;
};
#if 0
typedef std::map<PEAT_Rank, PEAT_Rank> Collective_Ranks;
#else
typedef std::set<PEAT_Rank> Collective_Ranks;
#endif
struct MAT_Collective : public Collective_Entry {
    Collective_Ranks expected_ranks;
    Collective_Ranks init_ranks;
};
struct collective_key {
    PEAT_MPI_Comm comm;
    PEAT_Num func_id;
    collective_key(PEAT_MPI_Comm c, PEAT_Num f) : comm(c), func_id(f) {}
    friend std::ostream &operator<<(std::ostream &dbg, const collective_key &key);
    bool operator<(const collective_key &key) const
    {
        if (comm < key.comm) {
            return true;
        }
        if (comm > key.comm) {
            return false;
        }
        if (func_id < key.func_id) {
            return true;
        }
        if (func_id > key.func_id) {
            return false;
        }
        return false;
    }
};

struct MAT_Collective_Record {
    PEAT_MPI_Func_ID func_id;
    PEAT_Collective_Bytes send_bytes, recv_bytes;
    // std::map<PEAT_Rank, std::string> comp_funcs;//map<source_rank, destination_properties>
    std::string comp_func;
    PEAT_Rank root, size;
    PEAT_MPI_Comm comm;
    MAT_Collective_Record() : root(-1), size(-1), comm(0)
    {
        func_id = TMPI_FUNC_ID_NONE;
    };
    bool operator<(const MAT_Collective_Record &right) const
    {
        if (size < right.size)
            return true;
        if (size > right.size)
            return false;
        if (root < right.root)
            return true;
        if (root > right.root)
            return false;
        if (comm < right.comm)
            return true;
        if (comm > right.comm)
            return false;
        if (func_id < right.func_id)
            return true;
        if (func_id > right.func_id)
            return false;
        if (send_bytes < right.send_bytes)
            return true;
        if (right.send_bytes < send_bytes)
            return false;
        if (recv_bytes < right.recv_bytes)
            return true;
        if (right.recv_bytes < recv_bytes)
            return false;
        // if(comp_funcs < right.comp_funcs) return true;
        // if(right.comp_funcs < comp_funcs) return false;
        if (comp_func < right.comp_func)
            return true;
        if (right.comp_func < comp_func)
            return false;
        return false;
    }
};

struct MAT_Collective_Clock {
    PEAT_Clock total_c, imb_c, min_c, max_c;
    PEAT_LNum occ;
    MAT_Collective_Clock() : total_c(0), imb_c(0), min_c(0), max_c(0), occ(0) {};

    MAT_Collective_Clock(const MAT_Collective_Clock &right)
    {
        total_c = right.total_c;
        imb_c = right.imb_c;
        min_c = right.min_c;
        max_c = right.max_c;
        occ = right.occ;
    };

    MAT_Collective_Clock &operator=(const MAT_Collective_Clock &right)
    {
        total_c = right.total_c;
        imb_c = right.imb_c;
        min_c = right.min_c;
        max_c = right.max_c;
        occ = right.occ;
        return *this;
    };
};
struct MAT_Collective_Clocks {
    std::map<PEAT_Rank, MAT_Collective_Clock> clocks;
};
/* Collective storage itself */
class Requests;
class Collectives : public Outputs
{ // it still called collectives but it also handles File IO
  public:
    int print_on_first;
    PEAT_LNum collectives_bound;
    Collectives(int pos
#ifdef PEAT_MPI
                ,
                std::list<PEAT_Event_Delivery> *ad, const std::vector<PEAT_Rank> &wR, const std::vector<PEAT_Rank> &mRs, const MPI_Comm mat_cc, const PEAT_Num mat_ccs, const PEAT_Rank m_r
#endif

    );
    virtual ~Collectives();
    void initialize(PTP *p, Communicators *c, Request_Store *rs, Requests* r, File_Storage_GZ *MPI, File_Storage_GZ *HWC, File_Storage_GZ *bind);
    void parse_collective(const PEAT_Rank rank, const Tractor_Event &event, bool is_remote);
    bool empty() const;
    PEAT_Num collective_queue_size() const;
    typedef std::list<MAT_Collective> collective_list;
    struct collective_list_struct {
        std::list<MAT_Collective> collectives;
#ifdef UPDATE_FIRST_ENTRY
        std::map<PEAT_Rank, collective_list::iterator> first;
#endif
    };
#define coll_get_value(x, y) x->y
#define coll_get_record(x) *x

    // friend std::ostream &operator<<(std::ostream &dbg, const Collectives &rec);
    std::map<int, std::map<MAT_Collective_Record, MAT_Collective_Clocks>> printed_collective;
    std::map<PEAT_Rank, std::map<PEAT_MPI_Request, MAT_FIO_Record>> fio_requests;

    bool add_pers_request(const PEAT_Rank rank, const PEAT_MPI_Request request);
    bool remove_pers_request(const PEAT_Rank rank, const PEAT_MPI_Request request, Phase& ph);
    bool update_request(const PEAT_Rank rank, const PEAT_MPI_Request request, const Phase& phase, Collective_Event* event);
    bool find_collective(const PEAT_Rank rank, Collective_Event* event, collective_list::iterator* record);
    bool find_collective(const PEAT_Rank rank, const MAT_Request_Store_Record* rec, collective_list::iterator* record);
    bool add_pers_req_start(const PEAT_Rank rank, MAT_Collectives_Record* record);

    bool find_request(PEAT_Rank rank, PEAT_MPI_Request request, MAT_FIO_Record *ret_it, PEAT_Bool erase);

    void process_unbound_events();
#ifdef PEAT_MPI
    PEAT_Signed_Num check_unbound_events();
#endif

    bool erase_collective(collective_key &key, collective_list::iterator &record);
    bool erase_collective(collective_list::iterator &record);
    void print_collective_bind_str(PEAT_Rank rank, const char *str);
    void print_sizes() const;

  private:
    bool match_collective(collective_list_struct &queue, collective_list::iterator &record, PEAT_Rank rank) const;
    bool match_collective(collective_list::iterator &first, collective_list::iterator &last, collective_list::iterator &record, PEAT_Rank rank) const;
    void process_collective(const PEAT_Rank rank, MAT_Collectives_Record* event, int pc_level, bool is_remote);

    void print_collective_out(const Tractor_Event &event);
    void print_collective_err(const MAT_Collective &record);
    void print_collective_bind(const MAT_Collective &record);
    bool check_completion(const MAT_Collective &record);
    bool check_completion(collective_list::iterator &record);
    std::map<collective_key, collective_list_struct> coll_queues;
    std::map<PEAT_MPI_File, PEAT_MPI_Comm> fh_comms;
    PTP *ptp;
    Communicators *comm_store;
    Request_Store* req_store;
    Requests* request_func_store;

    // std::vector<PEAT_Num> waiting_collectives;

#ifdef PEAT_MPI
    std::list<PEAT_Event_Delivery> *active_deliveries;
    MPI_Comm mat_compute_comm;
    PEAT_Num mat_compute_comm_size;
    PEAT_Rank my_rank;
    std::vector<PEAT_Rank> whole_ranks;
    std::vector<PEAT_Rank> my_ranks;
    std::vector<std::map<collective_key, PEAT_Rank>> collective_handlers;

  public:

    void send_collective(PEAT_Rank rank, const Tractor_Event &event);
    void process_remote_collective(PEAT_Signed_Num size, PEAT_Rank src);
    PEAT_Num check_completions();
#endif
};

#endif // __COLLECTIVES_H__
