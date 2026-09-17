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

#ifndef __PET_CORE_H__
#define __PET_CORE_H__

#include <climits>
#include <fstream>
#include <mutex>
#include <set>

#include "pet_event.h"
#include "pet_node.h"
#include "ptp.h"

// #include "collectives.h"
#include "collstorage.h"
#include "communicators.h"
#include "func_split.h"
#include "model_id.h"
#include "pet_fabric_event.h"
#include "requests.h"
#include "te_timeline.h"
#include "transport.h"

enum HANDLE_EVENT_CODE {
    EVENT_NOT_BOUND = -3,
    EVENT_NOT_READY,
    EVENT_NOT_ESTIMATED,
    EVENT_OK,
    EVENT_INITIATED
};

struct Timeline_Entry {
    PEAT_Rank rank;
    PEAT_UID idx;
};

#ifdef _DEBUG
#define _DEBUG_CURRENT_EVENTS_
struct PET_Event_Definition {
    PEAT_Rank rank;
    PEAT_UID primary_id, secondary_id;
    PEAT_Thread thread_id;
    PET_Event_Definition(PEAT_Rank r, PEAT_UID p, PEAT_UID s, PEAT_Thread t) : rank(r), primary_id(p), secondary_id(s), thread_id(t) {};
    PEAT_Bool operator<(const PET_Event_Definition &rhs) const
    {
        if (rank < rhs.rank) {
            return true;
        }
        if (rank > rhs.rank) {
            return false;
        }
        if (primary_id < rhs.primary_id) {
            return true;
        }
        if (primary_id > rhs.primary_id) {
            return false;
        }
        if (secondary_id < rhs.secondary_id) {
            return true;
        }
        if (secondary_id > rhs.secondary_id) {
            return false;
        }
        if (thread_id < rhs.thread_id) {
            return true;
        }
        if (thread_id > rhs.thread_id) {
            return false;
        }
        return false;
    }
};
#endif
class PET_Core
{
    std::map<PEAT_Rank, PEAT_Clock> last_read_idx;
    PEAT_Bool e_timeline_state_changed;
    Estimation_Event global_min_event;
    std::map<PEAT_Rank, PEAT_Bool> ranks_read;
#ifdef _DEBUG_CURRENT_EVENTS_
    std::map<PEAT_Rank, std::map<PEAT_Thread, PET_Event *>> dbg_event_being_estimated;
    std::map<PEAT_Clock, PET_Event *> dbg_event_being_estimated_clock;
#endif
#ifdef _DEBUG_DESTROYED_EVENT
    std::map<PET_Event_Definition, PEAT_MPI_Func_ID> deleted_events;
#endif
  public:
    PET_Core(const Global_Parameters &file_names, const Model_ID &model_id_storage);
    bool process_events();
    bool remove_orphans(PEAT_Rank rank);

    void process_exhaust_events();
    TE_Event *update_te_timelines(PEAT_Clock clk);
    TE_Event *update_te_timelines_parallel(PEAT_Clock clk);
    TE_Event *update_te_timelines_serial(PEAT_Clock clk);
    TE_Event *complete_te_event(TE_Event *evt);
    TE_Event *insert_te_event(TE_Event *evt);
    PEAT_Num check_for_outstanding_progress_messages(PEAT_Node node, PEAT_Rank rank, PEAT_Clock time);

    PEAT_Bool re_fill_queue;
    PEAT_LNum collectives_bound;
    std::vector<PEAT_LNum> messages_bound;
    std::vector<PEAT_LNum> events_processed;
    HANDLE_EVENT_CODE handle_event(PET_Event *event);
    PEAT_Bool write_event_and_update_stat(PET_Event *event);

    void register_parallel_region(PET_Event *event) { nodes.at(event->node).ranks.at(event->rank).register_parallel_region(event); }
    PEAT_Thread get_max_thread_id(const PEAT_Rank rank) const { return user_region_file->last_thread_idx.at(rank); }
    PEAT_Addr get_addr_by_id(const PEAT_Rank rank, const PEAT_UID addr) const { return user_region_file->get_addr_by_id(rank, addr)->addr; }

    PET_Thread *get_pet_thread(PET_Event *evt)
    {
        PEAT_Node node = evt->node;
        PEAT_Rank rank = evt->rank;
        PEAT_Thread thread = evt->thread_id;
        return nodes[node].ranks[rank].threads[thread];
    }

    PET_Rank *get_pet_rank(PEAT_Rank rank)
    {
        PEAT_Node node = ptp->get_tgt_node(rank);
#ifdef PEAT_OMP
        if (nodes[node].ranks.count(rank) == 1) { // nothing to create just get the pointer
            return &nodes[node].ranks[rank];
        }
        PET_Rank *pet_rank;
        rank_mutex.lock(); // structure is changed use lock/unlock
        pet_rank = &nodes[node].ranks[rank];
        rank_mutex.unlock();
        return pet_rank;
#else
        return &nodes[node].ranks[rank];
#endif
    }

    PET_Rank *get_pet_rank(PET_Event *evt)
    {
        PEAT_Node node = evt->node;
        PEAT_Rank rank = evt->rank;
#ifdef PEAT_OMP
        if (nodes[node].ranks.count(rank) == 1) {
            return &nodes[node].ranks[rank]; // nothing to create just get the pointer
        }
        PET_Rank *pet_rank;
        rank_mutex.lock(); // structure is changed use lock/unlock
        pet_rank = &nodes[node].ranks[rank];
        rank_mutex.unlock();
        return pet_rank;
#else
        return &nodes[node].ranks[rank];
#endif
    }

    PET_Node *get_pet_node(PET_Event *evt)
    {
        PEAT_Node node = evt->node;
        if (node == -1)
            node = ptp->get_tgt_node(evt->rank);
        return &nodes[node];
    }
    PET_Node *get_pet_node(PEAT_Node node)
    {
        return &nodes[node];
    }
    PEAT_Bool add_request_to_chain(PEAT_Rank rank, Completion_Entry &entry, PEAT_Num index, PEAT_MPI_Request req, PET_Completion_Event *event, PEAT_Bool completed)
    {
        if (req == com_store->get_MPI_REQUEST_NULL()) {
            return false;
        }
        return get_pet_rank(rank)->add_request_to_chain(entry, index, req, event, completed);
    }
    Completion_Chain *get_chain(PEAT_Rank rank, Completion_Entry &entry)
    {
        return get_pet_rank(rank)->get_chain(entry);
    }
    PET_Event *get_lock(PEAT_Rank rank, Completion_Entry &entry)
    {
        return get_pet_rank(rank)->get_lock(entry);
    }
    PEAT_Bool destroy_chain(PEAT_Rank rank, Completion_Entry &entry, PET_Completion_Event *event)
    {
        return get_pet_rank(rank)->destroy_chain(entry, event);
    }

    PEAT_Clock get_blank() { return 10000; }
    PET_Event *get_prev_event(PET_Event *event, PEAT_Bool reverse);
    PET_Event *get_prev_event(PET_Event *event, PEAT_Thread thread_id, PEAT_Bool reverse, PEAT_Bool use_cache);
    PET_Event *get_next_event(PET_Event *event);
    PET_Event *get_last_event(PET_Event *event);
    PET_Event *get_last_event_long(PET_Event *event);
    void fill_next_event(PET_Event *event);
#if 0
	PEAT_Bool remove_entry(PET_Event *event)
	{
		PET_Rank *pet_rank = get_pet_rank(event);
#ifdef PEAT_OMP
		mutexes[event->rank].lock();
#endif

		std::list<PET_Event*>::iterator event_it = pet_rank->threads[event->thread_id]->find_event(event);
		pet_rank->threads[event->thread_id]->active_events.erase(event_it);
#ifdef PEAT_OMP
		mutexes[event->rank].unlock();
#endif
		return false;
	}
	PEAT_Bool insert_entry(PET_Event* event, PEAT_Bool check_place)
	{
		PET_Rank* pet_rank = get_pet_rank(event);
#ifdef PEAT_OMP
		mutexes[event->rank].lock();
#endif
		pet_rank->threads[event->thread_id]->insert_ael_entry(event, check_place);
		pet_rank->insert_ael_entry(event);
#ifdef PEAT_OMP
		mutexes[event->rank].unlock();
#endif

		return true;
	}
#else
    PEAT_Bool remove_entry(PET_Event *event);
    PEAT_Bool insert_entry(PET_Event *event, PEAT_Bool check_place);
#endif
    PEAT_Bool insert_ee_entry(Estimation_Event *event)
    {
        if (event == NULL)
            return true;
        e_timeline_state_changed = true;
        event->owner->get_node()->e_timeline.insert_event(event, false);
        return true;
    }
    PEAT_Bool repeat_last_ee_entry(PET_Event *event)
    {
        e_timeline_state_changed = true;
        event->get_node()->e_timeline.repeat_last_ee_entry(event, true);
        return true;
    }
    PEAT_Bool insert_progress_ee_entry(Estimation_Event *event)
    {
        PEAT_Node node = event->owner->node;
        PEAT_Node rank = event->owner->rank;
        e_timeline_state_changed = true;
        nodes[node].ranks[rank].progress_engine.insert_event(event, true);
        return true;
    }
    Estimation_Event *get_min_ee_entry()
    {
        Estimation_Event *min_event = NULL;
        for (std::vector<PET_Node>::iterator node_it = nodes.begin(); node_it != nodes.end(); ++node_it) {
            min_event = min_ee_event(min_event, node_it->e_timeline.get_min_event());
        }
        return min_event;
    }
    PEAT_Bool delete_ee_entry(Estimation_Event *event)
    {
        e_timeline_state_changed = true;
        event->owner->get_node()->e_timeline.delete_event(event, false);
        return true;
    }
    PEAT_Bool delete_progress_ee_entry(Estimation_Event *event)
    {
        e_timeline_state_changed = true;
        event->owner->get_rank()->progress_engine.delete_event(event, true);
        return true;
    }
    Estimation_Event *get_progress_ee_event(PET_Event *event)
    {
        Estimation_Event *min_event = event->get_rank()->progress_engine.get_min_event();
        return min_event;
    }

    virtual ~PET_Core();
    PEAT_Message_Size get_eager_threshold(PEAT_Bool is_smp, PEAT_Bool is_non_block)
    {
        // if ( is_non_block ) return globals.nonblock_short_threshold;
        if (is_smp)
            return globals.smp_eager_threshold;
        else
            return globals.nic_eager_threshold;
    }
    PEAT_Message_Size get_short_threshold(PEAT_Transports t)
    {
        return globals.short_threshold[t];
    }

    void estimate();
    void cache();
    PEAT_Clock get_tractor_overhead(PEAT_Rank rank)
    {
        return get_pet_rank(rank)->tractor_overhead_time;
    }

    PEAT_Clock get_sub_time(PEAT_Rank rank)
    {
        return get_pet_rank(rank)->sub_time;
    }
    PTP *get_ptp()
    {
        assert(ptp);
        return ptp;
    }
    Communicators *get_comm()
    {
        assert(com_store);
        return com_store;
    }

    PEAT_Ratio get_user_code_ratio(PET_Event *prev_event, PET_Event *this_event);

    PEAT_Bool find_ucr(const Event_Address_Info &c_addr, const Event_Address_Info &p_addr, PEAT_Ratio &r);
    PEAT_Ratio analyze(const std::string &output_msg_file);
    PEAT_Ratio print_report(const Event_Set_Stat::t_stat_type type, const PEAT_Record_Type line_prefix);
    void write_ompt_regions();
    void print_parallel_region_report(const Event_Set_Stat::t_stat_type type, const PEAT_Record_Type line_prefix);
    PEAT_FREQ get_trace_freq() const
    {
        return freq;
    }
    PEAT_FREQ get_target_freq() const
    {
        if (globals.target_freq > 0) {
            return globals.target_freq;
        } else {
            return get_trace_freq();
        }
    }
    void get_cpu_diff(PEAT_Ratio &global_freq_ratio, PEAT_Ratio &global_cpi_ratio)
    {
        global_cpi_ratio = 1.0 / globals.MPI_CPI_ratio;
        global_freq_ratio = (PEAT_Ratio)get_trace_freq() / (PEAT_Ratio)get_target_freq();
    }
    PEAT_Bool handle_coll(PET_Coll_Event *);
    PEAT_Bool find_coll(PET_Coll_Event *, std::list<Coll_Storage>::iterator &);
    PEAT_Bool get_coll(const PET_Coll_Event *, std::list<Coll_Storage>::iterator &);
    PEAT_Bool get_coll_const(const PET_Coll_Event *, std::list<Coll_Storage>::const_iterator &) const;
    PEAT_Bool new_coll(PET_Coll_Event *);

    void update_active_event_chain(PEAT_Clock sub_t, PET_Event *event);
    void update_active_event_chain_t0(PEAT_Clock sub_t, PET_Event *event);
    void update_active_event_chain_tn0_out(PEAT_Clock sub_t, PET_Event *event);
    void update_active_event_chain_tn0_in(PEAT_Clock sub_t, PET_Event *event);

    // void move_openmp_regions(PEAT_Clock sub_t, PET_Event* event);
    // void update_current_event(PEAT_Clock start, PET_Event *);
    PET_Event *find_event_by_id(PEAT_Rank rank, PEAT_UID uidx)
    {
        return get_pet_rank(rank)->find_event_by_id(uidx);
    };
    PET_ToR_Event *find_event_by_request(PEAT_Rank rank, PEAT_MPI_Request request)
    {
        return get_pet_rank(rank)->find_event_by_request(request);
    };
    PET_ToR_Event *find_event_by_pers_request(PEAT_Rank rank, PEAT_MPI_Request request)
    {
        return get_pet_rank(rank)->find_event_by_pers_request(request);
    };
    PET_Event *find_peer(PET_Event *event, PEAT_Bool recv_part = false); // for sendrecv
    PET_Event *find_peer(PET_Event *event, PET_Coll_Event *host_evt, PEAT_Bool recv_part);
    PEAT_Bool is_rank_read(PEAT_Rank rank)
    {
        return ranks_read.count(rank) > 0;
    }
    PEAT_Bool is_rank_finished(PEAT_Rank rank)
    {
        PEAT_Node node = ptp->get_tgt_node(rank);
        PEAT_Bool finished = nodes.at(node).is_rank_finished(rank);
        return finished;
    }
    PEAT_Bool is_all_read()
    {
        return ranks_read.size() == globals.pe_MPI_files_in_rank.size();
    }
    PEAT_Bool is_all_finished()
    {
        for (std::vector<PET_Node>::iterator n_it = nodes.begin(); n_it != nodes.end(); ++n_it) {
            PEAT_Bool finished = n_it->is_all_finished();
            if (!finished)
                return false;
        }
        return true;
    }
    void set_rank_finished(PEAT_Rank rank)
    {
        PEAT_Node node = ptp->get_tgt_node(rank);
        nodes.at(node).set_finished(rank);
    }
    void set_all_ranks_finished()
    {
        for (std::vector<PET_Node>::iterator n_it = nodes.begin(); n_it != nodes.end(); ++n_it) {
            n_it->set_all_node_ranks_finished();
        }
    }
    void re_fill() { re_fill_queue = true; }
    friend std::ostream &operator<<(std::ostream &, const PET_Core &);
    PEAT_Bool collect_garbage();
    PEAT_Bool add_persistent_request(const PET_ToR_Event &event)
    {
        return get_pet_rank(event.rank)->add_persistent_request(event);
    };
    PEAT_Bool remove_persistent_request(PEAT_Rank rank, PEAT_MPI_Request request)
    {
        return get_pet_rank(rank)->remove_persistent_request(request);
    };
    PEAT_Bool assign_transport(TE_Event &evt, PEAT_MPI_Func_ID func_id, PEAT_MPI_Func_ID parent_id);
    PEAT_Time_Parts *get_func_split(PET_Event *evt, PEAT_Transports t, PEAT_Message_Size size)
    {
        PEAT_Func_ID_Pair pair(evt->func_id, evt->parent_func_id);
        return func_split->get_func_split(t, pair, evt->get_ranks_num(), size);
    }
    void build_fabric_events_list(PET_Event *event);
    PEAT_Bool read_fabric_events(PET_Event *event);
    PEAT_Bool update_time_predictor(PEAT_Clock &, PET_Time_Event *, PET_Event *);
    PEAT_Signed_Num get_pe_type() { return globals.progress_engine; }
    PEAT_Bool get_up_status() { return globals.use_up != 0; }
    void no_more_events(PEAT_Bool);
    void check_progress(PEAT_Bool, std::list<PET_Event *> &);
    long useless_iterations, useful_iterations;
    PEAT_UID event_processed, event_processed_delta;

  protected:
    std::vector<PET_Node> nodes; // events in ranks
#ifdef PEAT_OMP
    // PEAT_Mutex* ins_rem_mutexes;
    PEAT_Mutex *read_mutexes;
    // PEAT_Mutex* match_mutexes;
    PEAT_Mutex coll_mutex;
    PEAT_Mutex rank_mutex;
    String_Split *trace_line_MPIs;
    String_Split *trace_line_HWCs;
#else
    String_Split trace_line_MPI;
    String_Split trace_line_HWC;

#endif
    std::vector<std::list<Coll_Storage>> coll_store;

    PET_Event *get_event(const PEAT_Rank rank, String_Split &trace_line_MPI, String_Split &trace_line_HWC, PET_Coll_Event *host_evt);
    Event_Set_Stat stat;
    PTP *ptp;
    PEAT_Transport *transport;

    File_Storage_GZ *input_MPI;
    File_Storage_GZ *input_HWC;
    File_Storage_GZ *input_fab;
    File_Storage_GZ *output_MPI;
    File_Storage_GZ *output_HWC;
    File_Storage_GZ *output_bind;
    std::vector<PEAT_Clock> output_model_tsc;

    Address_File_Storage *user_region_file;
    PEAT_Function_Splitter *func_split;
    Global_Parameters globals;

    std::map<PEAT_Rank, User_Region_Store> addr_idx;
    PEAT_FREQ freq;

  public:
    Communicators *com_store;
    const Model_ID &model_id;
    std::vector<PEAT_Clock> tsc_corrector;
    std::vector<PEAT_Clock> fabric_tsc_corrector;
    PEAT_UID loop_counter;
    void write_event_profile(const std::string &name, const Event_Set_Stat::t_stat_type type);
    void write_trace_info(const std::string &name);
    void write_transport_profile(const std::string &name, const Event_Set_Stat::t_stat_type type);
    void write_function_split_profile(const std::string &name);
    PEAT_Bool get_mo() const { return (PEAT_Bool)globals.matching_only; }
    friend PET_Event;
    friend PET_OMPT_Event;
};
TE_Event *min_te_event(TE_Event *e, TE_Event *e_tmp);

#endif
