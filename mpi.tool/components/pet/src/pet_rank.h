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

#ifndef __PET_RANK_H__
#define __PET_RANK_H__

#include "common_types.h"
#include "pet_event.h"
#include "pet_fabric_event.h"
#include "pet_thread.h"
#include "stat.h"
#include "timeline.h"
#include <assert.h>
#include <set>

struct Extended_Request {
    PEAT_MPI_Request request;
    PEAT_Bool completed;
    std::list<PET_Completion_Event *> events;
    Extended_Request() : request(-1), completed(false) {};
};

struct Completion_Chain {
    PET_Event *locker;
    Completion_Chain() : locker(NULL) {}
    std::vector<Extended_Request> entry;
    void resize(PEAT_Num n) { entry.resize(n); }
    PEAT_Bool is_completed()
    {
        if (entry.empty())
            return false;
        for (PEAT_Num i = 0; i < entry.size(); i++) {
            if (!entry[i].completed)
                return false;
        }
        return true;
    }
    PEAT_Bool is_locked()
    {
        return locker != NULL;
    }
    PET_Event *get_locker()
    {
        return locker;
    }
    void lock(PET_Event *evt) { locker = evt; }
    void unlock() { locker = NULL; }
};

struct Completion_Entry {
    PEAT_Num num_reqs;
    PEAT_Addr addr;
    //	std::list <PEAT_MPI_Request> reqs;
    Completion_Entry(PEAT_Num n, PEAT_Addr a)
    {
        num_reqs = n;
        addr = a;
    }
    PEAT_Bool operator<(const Completion_Entry &rhs) const
    {
        if (num_reqs < rhs.num_reqs) {
            return true;
        }
        if (num_reqs > rhs.num_reqs) {
            return false;
        }
        if (addr < rhs.addr) {
            return true;
        }
        if (addr > rhs.addr) {
            return false;
        }
        /*		for (std::list <PEAT_MPI_Request>::const_iterator l_it = reqs.cbegin(), r_it = rhs.reqs.cbegin(); l_it != reqs.cend(); ++l_it, ++r_it) {
                    if (*l_it < *r_it) { return true; }
                    if (*l_it > *r_it) { return false; }
                }
        */
        return false;
    }
};
struct PET_Region {
    PEAT_Time_Event o_entry, o_exit;
    PEAT_Time_Event t_entry, t_exit;
    PEAT_Time_Event e_entry, e_exit;
};
class PET_Node;
class PET_Rank
{
    PEAT_Clock tractor_overhead_time; // moving event due to removing tractor overhead
    PEAT_Clock sub_time;              // moving event due to estimation
    PEAT_Bool rank_finished;
    // PET_Event * event_being_estimated;
    Estimation_Timeline progress_engine;
    PEAT_Clock last_progress_notify;
    // maps here are only for fast access
    /* These members are moving to PET_Thread */
    std::list<PET_Event *> pairing_rank_events;
    std::vector<PET_Thread *> threads;
    std::map<PEAT_Rank, Event_Set_Stat> stats;
    std::list<PET_Event *> omp_parallel_sections;
    // std::list<PET_Event*> omp_parallel_begin_sections;
    int verbose; // if true, add algo debug output
    std::map<Completion_Entry, Completion_Chain> completion_chains;
    std::map<PEAT_MPI_Request, PET_ToR_Event> pers_store;
    std::list<PET_Fabric_Event> fabric_events;
    static PEAT_LNum total_fabric_events;
    static PEAT_LNum total_fabric_events_added;
    static PEAT_LNum total_fabric_events_removed;
    PET_Event *last_read_event;
    PEAT_Bool orphans_processed;
    std::list<PET_Collective_Event *> nb_collectives; // outstanding nonblocking collectives (owners of foreign progress records)
    std::map<PEAT_Event_IDs, PET_Event *> events_by_ids; // every currently-live event of this rank, keyed by its full id path (any nesting depth)

  public:
    void register_nb_collective(PET_Collective_Event *event) { nb_collectives.push_back(event); }
    void unregister_nb_collective(PET_Collective_Event *event) { nb_collectives.remove(event); }
    void register_event_by_ids(PET_Event *event) { events_by_ids[event->ids] = event; }
    void unregister_event_by_ids(PET_Event *event)
    {
        std::map<PEAT_Event_IDs, PET_Event *>::iterator it = events_by_ids.find(event->ids);
        if (it != events_by_ids.end() && it->second == event)
            events_by_ids.erase(it);
    }
    PET_Event *find_event_by_ids(const PEAT_Event_IDs &ids)
    {
        std::map<PEAT_Event_IDs, PET_Event *>::iterator it = events_by_ids.find(ids);
        return it == events_by_ids.end() ? NULL : it->second;
    }
    PET_Collective_Event *find_nb_collective(PEAT_MPI_Comm comm, PEAT_MPI_Tag tag, const PET_Event *exclude)
    {
        // most recent first: tags are reused once a collective is done
        for (std::list<PET_Collective_Event *>::reverse_iterator it = nb_collectives.rbegin(); it != nb_collectives.rend(); ++it) {
            if (*it != exclude && (*it)->uses_tag(comm, tag))
                return *it;
        }
        return NULL;
    }
    PET_Rank() : tractor_overhead_time(0), sub_time(0), rank_finished(false), last_progress_notify(0), last_read_event(NULL), orphans_processed(false)
    {
        verbose = 1;
        // threads.resize(1);
        // threads[0] = new PET_Thread(this,0);
    }
    ~PET_Rank()
    {
        for (std::vector<PET_Thread *>::iterator thread_it = threads.begin(); thread_it != threads.end(); ++thread_it) {
            delete *thread_it;
        }
    }
    void init_threads(PEAT_Thread max_thread_id)
    {
        threads.resize(max_thread_id + 1);
        for (PEAT_Thread t = 0; t <= max_thread_id; t++) {
            threads[t] = new PET_Thread(this, t);
        }
    }
    void set_finished()
    {
        rank_finished = true;
    }

    PEAT_Bool is_finished() const
    {
        return rank_finished;
    }
    PET_Event *get_start_omp_paralel_section(PET_Event *) const;
    PET_Event *get_finish_omp_paralel_section(PET_Event *) const;
    /* These members are moving to PET_Thread */
    PEAT_Bool get_head_event(std::list<PET_Event *>::iterator &evt_it)
    {
        evt_it = pairing_rank_events.begin();
        if (evt_it != pairing_rank_events.end())
            return true;
        return false;
    }
    PEAT_Bool get_next_event(std::list<PET_Event *>::iterator &evt_it)
    {
        ++evt_it;
        if (evt_it != pairing_rank_events.end())
            return true;
        return false;
    }
    PEAT_Bool get_tail_event(std::list<PET_Event *>::reverse_iterator &evt_it)
    {
        evt_it = pairing_rank_events.rbegin();
        if (evt_it != pairing_rank_events.rend())
            return true;
        return false;
    }
    PEAT_Bool get_prev_event(std::list<PET_Event *>::reverse_iterator &evt_it)
    {
        ++evt_it;
        if (evt_it != pairing_rank_events.rend())
            return true;
        return false;
    }

    PEAT_Bool delete_event(std::list<PET_Event *>::iterator &evt_it)
    {
        evt_it = pairing_rank_events.erase(evt_it);
        if (evt_it != pairing_rank_events.end())
            return true;
        return false;
    }
    void collect_garbage();
    friend PET_Core;
    PEAT_Bool add_request_to_chain(Completion_Entry &entry, PEAT_Num index, PEAT_MPI_Request req, PET_Completion_Event *event, PEAT_Bool completed)
    {
        PEAT_Bool exist = completion_chains.count(entry);
        Completion_Chain &chain = completion_chains[entry];
        if (chain.is_completed()) {
            assert(0);
            return false;
        }
        if (chain.is_locked()) {
            assert(0);
            return false;
        }
        if (!exist) {
            chain.resize(entry.num_reqs);
        }
        Extended_Request &e_req = chain.entry[index];
        if (exist) {
            // check it first
            if (e_req.completed == true) {
                assert(0);
                return false;
            }
            if (e_req.request != req) { // handle zombie request (buffer address has been changed unexpectively)
                for (std::list<PET_Completion_Event *>::iterator evt_it = e_req.events.begin(); evt_it != e_req.events.end(); ++evt_it) {
                    if ((*evt_it)->can_be_destroyed()) {
                        delete *evt_it;
                    } else {
                        assert(0);
                    }
                }
                e_req.events.clear();
            }
        }
        e_req.request = req;
        e_req.events.push_back(event);
        e_req.completed = completed;
        chain.entry[index] = e_req;
        if (chain.is_completed()) {
            chain.lock(event);
        }
        return true;
    }
    Completion_Chain *get_chain(Completion_Entry &entry)
    {
        return &completion_chains[entry];
    }
    PET_Event *get_lock(Completion_Entry &entry)
    {
        return get_chain(entry)->get_locker();
    }

    void check_and_allocate_threads(PEAT_Thread max_thread_id)
    {
        if (threads.empty())
            init_threads(max_thread_id);
    }

    void remove_pre_entry(PET_Event *event)
    {
        if (is_pairing_event(event)) {
            for (std::list<PET_Event *>::iterator event_it = pairing_rank_events.begin(); event_it != pairing_rank_events.end(); ++event_it) {
                if (event == *event_it) {
                    pairing_rank_events.erase(event_it);
                    break;
                }
            }
        }
    };

    static PEAT_Bool is_pairing_event(PET_Event *event)
    {
        PET_Event_Type t = event->get_event_type();
        if (t == PET_EVENT_TYPE_TOR || t == PET_EVENT_TYPE_TAR)
            return true;
        // nonblocking collectives / communicator creation are completed by MPI_Wait & co, so requests must find them
        if ((t == PET_EVENT_TYPE_COLLECTIVE || t == PET_EVENT_TYPE_COMM) && event->get_mpi_request() != 0)
            return true;
        return false;
    }
    void insert_pre_entry(PET_Event *event)
    {
        if (is_pairing_event(event)) {
            pairing_rank_events.push_back(event);
        }
    }
    /*
    PEAT_Bool delete_request_from_chain(PEAT_Rank rank, Completion_Entry& entry, PEAT_Num index, PEAT_MPI_Request req, PET_Completion_Event *event)
    {
        PEAT_Bool exist = completion_chains[rank].count(entry);
        if (!exist) return true;
        Completion_Chain& chain = completion_chains[rank].at(entry);
        Extended_Request& e_req = chain.entry[index];
        assert(e_req.completed);
        for (std::list <PET_Completion_Event*>::iterator event_it=e_req.events.begin(); event_it != e_req.events.end(); ++event_it) {
            if (*event_it == event) {
                e_req.events.erase(event_it);
            }
        }
        if (e_req.events.empty()) e_req.deleted = true;
        PEAT_Num num_remained=entry.num_reqs;
        for (PEAT_Num i = 0; i < entry.num_reqs; i++) {
            num_remained -= (PEAT_Num) chain.entry[i].deleted;
        }
        if (num_remained == 0) completion_chains[rank].erase(entry);
        return true;
    }*/
    PEAT_Bool destroy_chain(Completion_Entry &entry, PET_Completion_Event *event)
    {
        PEAT_Bool exist = completion_chains.count(entry);
        if (!exist)
            return true;
        completion_chains.erase(entry);
        return true;
    }
    PEAT_Bool add_persistent_request(const PET_ToR_Event &event)
    {
        assert(pers_store.count(event.request) == 0);
        pers_store.emplace(event.request, event);
        PET_ToR_Event &event_pers = pers_store.at(event.request);
        event_pers.ids.assign(2, -1); // template of a persistent request: no record of its own
        return true;
    };
    PEAT_Bool remove_persistent_request(PEAT_MPI_Request request)
    {
        if (pers_store.count(request)) {
            pers_store.erase(request);
            return true;
        }
        return false;
    };
    PET_ToR_Event *find_event_by_pers_request(PEAT_MPI_Request request)
    {
        if (pers_store.count(request) == 1) {
            return &pers_store.at(request);
        }
        return NULL;
    }
    PET_Event *find_event_by_id(PEAT_UID uidx)
    {
        std::list<PET_Event *>::iterator event_it;
        PEAT_Bool r = get_head_event(event_it);
        // try to search in active events first
        while (r) {
            PET_Event *event = *event_it;
            if (event->ids[0] == uidx) {
                return event;
            }
            r = get_next_event(event_it);
        };
        return NULL;
    }
    PET_ToR_Event *find_event_by_request(PEAT_MPI_Request request)
    {
#if 0
		std::list <PET_Event*>::iterator event_it;
		PEAT_Bool r = get_head_event(event_it);
		//skip persistent event
		while (r) {
			PET_Event* event = *event_it;
			PET_Event* evt = event->query_request(request);
			if (evt) {
				if (!evt->is_persistent_init()) {
					return evt;
				}
			}
			r = get_next_event(event_it);
		};
#else
        std::list<PET_Event *>::reverse_iterator event_it;
        PEAT_Bool r = get_tail_event(event_it);
        // skip persistent event
        while (r) {
            PET_Event *event = *event_it;
            PET_Event *evt = event->query_request(request);
            if (evt) {
                if (!evt->is_persistent_init()) {
                    return static_cast<PET_ToR_Event *>(evt);
                }
            }
            r = get_prev_event(event_it);
        };
#endif
        // colt records issued from progress inside a blocking call (e.g. MPI_Barrier) are not reachable through the
        // pairing list: scan every active call of the rank, query_request() descends into nested records
        for (std::vector<PET_Thread *>::reverse_iterator thread_it = threads.rbegin(); thread_it != threads.rend(); ++thread_it) {
            for (std::list<PET_Event *>::reverse_iterator evt_it = (*thread_it)->active_events.rbegin(); evt_it != (*thread_it)->active_events.rend(); ++evt_it) {
                PET_Event *evt = (*evt_it)->query_request(request);
                if (evt && !evt->is_persistent_init()) {
                    return static_cast<PET_ToR_Event *>(evt);
                }
            }
        }
        return NULL;
    }
    void register_parallel_region(PET_Event *event);
    std::map<PEAT_Signed_Num, std::list<PET_Region>> parallel_regions;
    PEAT_Bool insert_fabric_event(PET_Fabric_Event &event);
    PEAT_Bool remove_fabric_events(PEAT_Clock clk);
    PEAT_Num check_for_outstanding_progress_messages(PEAT_Clock clk)
    {
        PEAT_Num event_processed = 0;
        for (std::vector<PET_Thread *>::iterator thread_it = threads.begin(); thread_it != threads.end(); ++thread_it) {
            if ((*thread_it)->event_being_estimated) {
                event_processed += (*thread_it)->event_being_estimated->check_for_outstanding_progress_messages(clk);
            }
        }
        return event_processed;
    }

#if 0
	PET_Event* get_currently_estimating_event()
	{
		if ( event_being_estimated ) {
			return event_being_estimated->get_estimated_event();
		} else {
			return NULL;
		}
	}
#endif
    friend PET_Node;
    friend PET_Thread;
    friend PET_Event;
};

#endif //__PET_RANK_H__
