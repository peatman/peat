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

#include "collectives.h"
#include "communicators.h"
#include "utils.h"
#include "requests.h"
#include "comm.h"
#include <assert.h>
#include <sstream>


MAT_FIO_Record::MAT_FIO_Record(const Tractor_Event& s) : MAT_Record()
{
    rank = s.rank;
    pc_level = s.level;
    comm = s.MPIFIO.comm;
    request = s.MPIFIO.request;
    fh = s.MPIFIO.fh;
	ph = s;
}

Phase& MAT_Collectives_Record::get_ph() { return coll_event->ph; }
MAT_Collectives_Record::MAT_Collectives_Record(const MAT_Collectives_Record& s) :MAT_Record(s)    
{
    coll_event = new Collective_Event(*s.coll_event);
}
MAT_Collectives_Record::MAT_Collectives_Record(const Tractor_Event& s) : MAT_Record()
{
    rank = s.rank;
    pc_level = s.level; 
    coll_event = new Collective_Event(s);
}
MAT_Collectives_Record::~MAT_Collectives_Record()
{
    delete coll_event;
}
Collective_Event::Collective_Event(const Tractor_Event& s) : addrs(s.addrs), tsc_time(s.tsc_time), tsc_u_time(s.tsc_u_time)
{
    switch(s.func_class) {
    case PEAT_PROP_CLASS_FIO:
        request = permanent_request = s.MPIFIO.request;
        comm = s.MPIFIO.comm;
        break;
    case PEAT_PROP_CLASS_COLLECTIVE:
        request = permanent_request = s.Collective.request;
        comm = s.Collective.comm;
        break;
    case PEAT_PROP_CLASS_COMM:
        request = permanent_request = s.Comms.request;
        comm = s.Comms.comm;
        break;
    default:
        assert(0);
        break;
    }
    ph = s;
}


Collectives::Collectives(int pos
#ifdef PEAT_MPI
                         ,
                         std::list<PEAT_Event_Delivery> *ad, const std::vector<PEAT_Rank> &wR, const std::vector<PEAT_Rank> &mRs, const MPI_Comm mat_cc, const PEAT_Num mat_ccs, const PEAT_Rank m_r
#endif
                         ) : print_on_first(pos), collectives_bound(0)
#ifdef PEAT_MPI
                             ,
                             active_deliveries(ad), mat_compute_comm(mat_cc), mat_compute_comm_size(mat_ccs), my_rank(m_r), whole_ranks(wR), my_ranks(mRs)
#endif

{
    comm_store = NULL;
    ptp = NULL;
    req_store = NULL;
#ifdef PEAT_MPI
    collective_handlers.resize(wR.size());
#endif
}

void Collectives::initialize(PTP *p, Communicators *cs, Request_Store *rs, Comm *c, Requests* r, File_Storage_GZ *MPI, File_Storage_GZ *HWC, File_Storage_GZ *bind)
{
    assert(c);
    assert(p);
    comm_store = cs;
    ptp = p;
    req_store = rs;
    comm_func_store = c;
    request_func_store = r;
    Outputs::initialize(MPI, HWC, bind);
}

Collectives::~Collectives()
{
    ptp = 0;
    comm_store = 0;
}
void Collectives::print_collective_out(const Tractor_Event &event)
{
#ifdef MAT_PROCESS_HWC_MPI_OUTPUT
    switch(event.func_class) {
    case PEAT_PROP_CLASS_FIO:
        output_MPI->printf(event.rank, "%s:%s:%x:%llx:%llx:%lld:%lld", event.header_str().c_str(), global_func_id.name_by_idx(event.func_id),
            event.MPIFIO.comm, event.MPIFIO.fh, event.MPIFIO.request, event.MPIFIO.offset, event.MPIFIO.bytes);
        break;
    case PEAT_PROP_CLASS_COMM:
		comm_func_store->print_comm_out(event);
        break;
    case PEAT_PROP_CLASS_COLLECTIVE:
        output_MPI->printf(event.rank, "%s:%s:%x:%d:%llx:%d:%d", event.header_str().c_str(), global_func_id.name_by_idx(event.func_id),
                           event.Collective.comm, event.Collective.root, event.Collective.request, event.Collective.rw_fields0, event.Collective.rw_fields1);
        for (PEAT_Signed_Num j = 0; j < event.Collective.rw_fields0; j++) {
            output_MPI->printf(event.rank, ":%lld", event.send_bytes[j]);
        }
        for (PEAT_Signed_Num j = 0; j < event.Collective.rw_fields1; j++) {
            output_MPI->printf(event.rank, ":%lld", event.recv_bytes[j]);
        }
        break;
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
    output_MPI->printf(event.rank, "\n");
#endif
}
void Collectives::print_collective_err(const MAT_Collective &record, PEAT_Rank my_rank)
{
    std::string uids;
    std::string missing_ranks;
    char header[1024];
    if (!record.coll_events_list.empty()) {
        for (std::map<PEAT_Rank, Collective_Event*>::const_iterator it = record.coll_events_list.begin(); it != record.coll_events_list.end(); ++it) {
            const PEAT_Rank rank = it->first;
            const Collective_Event *prop = it->second;
            char local_uids[128];
            snprintf(local_uids, sizeof(local_uids), ":%d:%s", rank, peat_ids_string(prop->ph.ids).c_str());
            uids = uids + local_uids;
        }
        for (Collective_Ranks::const_iterator it = record.expected_ranks.begin(); it != record.expected_ranks.end(); ++it) {
            const PEAT_Rank rank = *it;
            char local_uids[128];
            sprintf(local_uids, ":%d", rank);
            missing_ranks = missing_ranks + local_uids;
        }
        sprintf(header, "Rank:%d:%d:%s", my_rank,PEAT_RT_Bind_Collective, global_func_id.name_by_idx(record.func_id));
        std::string final = header;
        final = final + uids + " Missing Ranks:" + missing_ranks + "\n";
        fputs(final.c_str(), stderr);
    }
}

void Collectives::print_collective_bind_str(PEAT_Rank rank, const char *str)
{
    output_bind->puts(rank, str);
    collectives_bound++;
}
#define PRINT_STAT
void Collectives::print_collective_bind(const MAT_Collective &record)
{
    std::string uids;
    char header[1024];
    if (!record.coll_events_list.empty()) {
        for (std::map<PEAT_Rank, Collective_Event*>::const_iterator it = record.coll_events_list.begin(); it != record.coll_events_list.end(); ++it) {
            const PEAT_Rank rank = it->first;
            const Collective_Event *prop = it->second;
            char local_uids[128];
            snprintf(local_uids, sizeof(local_uids), ":%d:%s", rank, peat_ids_string(prop->ph.ids).c_str());
            uids = uids + local_uids;
        }
        sprintf(header, "%d:%s", PEAT_RT_Bind_Collective, global_func_id.name_by_idx(record.func_id));
        std::string final = header;
        final = final + uids + "\n";
        // for(std::map<PEAT_Rank, Collective_Event>::const_iterator it = record.coll_events_list.begin(); it != record.coll_events_list.end(); ++it) {
        PEAT_Rank root_rank = record.coll_events_list.begin()->first;
        if (print_on_first) {
#ifdef PEAT_MPI
            char *tbuf = (char *)peat_malloc_aligned(strlen(final.c_str()) + 5);
            const PEAT_Rank rank = root_rank;
            int *rnk = (int *)tbuf;
            *rnk = rank;
            PEAT_Rank mat_peer = whole_ranks[root_rank];
            PEAT_Event_Delivery delivery;
            strcpy(&tbuf[4], final.c_str());
            PEAT_Signed_Num size = (PEAT_Signed_Num)strlen(&tbuf[4]) + 5;
            delivery.buffer = tbuf;
            delivery.receiver = mat_peer;
            delivery.func = TMPI_FUNC_ID_NONE;
            MPI_Isend(delivery.buffer, size, MPI_CHAR, mat_peer, TRACTOR_COLL_PRINT_STRING_TAG, mat_compute_comm, &delivery.request);
            active_deliveries->push_back(delivery);
#else
            output_bind->puts(root_rank, final);
            collectives_bound++;
#endif
        } else {
#ifdef PEAT_MPI
            root_rank = -1; // auto select
#endif
            output_bind->puts(root_rank, final);
            collectives_bound++;
        }
    }
#ifdef PRINT_STAT
    /***************************************************************************/
    if (!record.coll_events_list.empty() && output_HWC && output_HWC->get_number_of_files() > 0) { // no timing data to compute stats/imbalance from when hwc files aren't read
        // col_file->write_line(header);
        // col_file->put_records(record);
        MAT_Collective_Record r_col;
        r_col.func_id = record.func_id;
        r_col.send_bytes = record.send_bytes;
        r_col.recv_bytes = record.recv_bytes;

        r_col.comp_func = global_func_id.name_by_idx(record.func_id); // ???
        for (std::map<PEAT_Rank, Collective_Event*>::const_iterator it = record.coll_events_list.begin(); it != record.coll_events_list.end(); ++it) {
            const PEAT_Rank rank = it->first;
            const Collective_Event *prop = it->second;
            // r_col.comp_funcs[rank] = global_func_id.name_by_idx(record.func_id); // ???
            // if (prop.permanent_request > 0) r_col.comp_funcs[rank] = global_func_id.name_by_idx(prop.completion.func_id);
            if (prop->permanent_request > 0)
                r_col.comp_func = global_func_id.name_by_idx(prop->ph.func_id); // TODO remove
            char local_uids[128];
            snprintf(local_uids, sizeof(local_uids), " %d,%s", rank, peat_ids_string(prop->ph.ids).c_str());
            uids = uids + local_uids;
        }
        r_col.root = record.root;
        r_col.size = record.size;
        r_col.comm = record.comm;
        int pc_level = record.pc_level;
        MAT_Collective_Clocks r_col_clk = printed_collective[pc_level][r_col];
        PEAT_Clock max_in = 0;
        for (std::map<PEAT_Rank, Collective_Event*>::const_iterator it = record.coll_events_list.begin(); it != record.coll_events_list.end(); ++it) {
            // PEAT_Rank rank = it->first;
            Collective_Event *evt = it->second;
            PEAT_Clock duraton = evt->tsc_time.mpi.out - evt->tsc_time.mpi.in;
            r_col_clk.clocks[it->first].total_c += duraton;
            r_col_clk.clocks[it->first].occ++;
            if (r_col_clk.clocks[it->first].min_c == 0 || r_col_clk.clocks[it->first].min_c > duraton)
                r_col_clk.clocks[it->first].min_c = duraton;
            if (r_col_clk.clocks[it->first].max_c < duraton)
                r_col_clk.clocks[it->first].max_c = duraton;
            max_in = std::max(max_in, evt->tsc_time.mpi.in);
        }
        for (std::map<PEAT_Rank, Collective_Event*>::const_iterator it = record.coll_events_list.begin(); it != record.coll_events_list.end(); ++it) {
            Collective_Event *evt = it->second;
            PEAT_Clock in = evt->tsc_time.mpi.in;
            r_col_clk.clocks[it->first].imb_c += max_in - in;
        }
        printed_collective[pc_level][r_col] = r_col_clk;
    }
#endif
}

PEAT_Bool Collectives::match_collective(collective_list::iterator &first, collective_list::iterator &last, collective_list::iterator &record, PEAT_Rank rank) const
{
    collective_list::iterator it = first;
    for (; it != last; ++it) {
        Collective_Ranks &checked_hash = coll_get_value(it, expected_ranks);
        // assert(!checked_hash.empty()); // No longer valid due to non-blocking collectgives
        if (checked_hash.count(rank)) {
            record = it;
#ifdef UPDATE_FIRST_ENTRY
            first = it;
#endif
            return true;
        }
    }
#ifdef UPDATE_FIRST_ENTRY
    first = it;
#endif
    return false;
}

PEAT_Bool Collectives::match_collective(collective_list_struct &queue, collective_list::iterator &record, PEAT_Rank rank) const
{
    if (!queue.collectives.empty()) {
#ifdef UPDATE_FIRST_ENTRY
        if (queue.first.count(rank) < 1)
            queue.first[rank] = queue.collectives.begin();
        collective_list::iterator &first = queue.first.at(rank);
        collective_list::iterator last = queue.collectives.end();
        if (first == queue.collectives.end())
            first = queue.collectives.begin();
        PEAT_Bool ret = match_collective(first, last, record, rank);
        // if (ret) first = record;
        return ret;
#else
        return match_collective(queue.collectives.begin(), queue.collectives.end(), record, rank);
#endif
    }
    return false;
}

PEAT_Bool Collectives::empty() const
{
    if (!coll_queues.empty()) {
        for (std::map<collective_key, collective_list_struct>::const_iterator it = coll_queues.begin(); it != coll_queues.end(); ++it) {
            if (!it->second.collectives.empty()) {
                return false;
            }
        }
    }
    return true;
}
void Collectives::process_collective(const PEAT_Rank rank, MAT_Collectives_Record* event, int pc_level, PEAT_Bool is_remote)
{
    collective_key key(event->coll_event->comm, event->coll_event->ph.func_id);
    collective_list::iterator matched_record;
    PEAT_Bool matched_collective_exists = false;
    if(event->coll_event->request > 0) {
        MAT_Request_Store_Record tmp;
        tmp.request_type = REQUEST_TYPE_USER_COLLECTIVE;
        tmp.record = event;
        req_store->add_request(rank, event->coll_event->request, &tmp);
    }
    if(!coll_queues.empty()) {
        if(coll_queues.count(key)) {
            // cerr << "Collective key " << collective_key << " mpifunc " << mpifunc << " comm " << comm << " count " << collective_queues.count(collective_key) << endl;
            matched_collective_exists = match_collective(coll_queues.at(key), matched_record, rank);
#if 0 // def _DEBUG
            collective_list::iterator matched_record_debug;
            PEAT_Bool matched_collective_exists_debug = match_collective_old(coll_queues.at(collective_key), matched_record_debug, rank);
            if(matched_collective_exists_debug != matched_collective_exists) {
                matched_collective_exists = match_collective(coll_queues.at(collective_key), matched_record, rank);
            };
            if(matched_collective_exists_debug) assert(matched_record_debug == matched_record);
#endif
        }
    }
    if(matched_collective_exists) {
        Collective_Ranks& expected_ranks = coll_get_value(matched_record, expected_ranks);
#if defined PROCESS_WHOLE_EVENT
        Collective_Ranks& init_ranks = matched_record->init_ranks;
#endif
        std::map<PEAT_Rank, Collective_Event*>& properties_list = coll_get_value(matched_record, coll_events_list);
        assert(!expected_ranks.empty());
        if(expected_ranks.count(rank)) {
            expected_ranks.erase(rank);
#if defined PROCESS_WHOLE_EVENT
            if(event->func_class == PEAT_PROP_CLASS_COLLECTIVE) {
                if(event->Collective.rw_fields0 > 0 && matched_record->send_bytes.empty()) {
                    if(event->send_bytes.size() == 1) {
                        matched_record->send_bytes.set_scalar(event.send_bytes.at(0));
                    } else {
                        for(Collective_Ranks::const_iterator it = init_ranks.begin(); it != init_ranks.end(); ++it) {
                            matched_record->send_bytes.set_vector(event.send_bytes.at(it->second), it->first);
                        }
                    }
                }
                if(event.Collective.rw_fields1 > 0 && matched_record->send_bytes.empty()) {
                    if(event.recv_bytes.size() == 1) {
                        matched_record->recv_bytes.set_scalar(event.recv_bytes.at(0));
                    } else {
                        for(Collective_Ranks::const_iterator it = init_ranks.begin(); it != init_ranks.end(); ++it) {
                            matched_record->recv_bytes.set_vector(event.recv_bytes.at(it->second), it->first);
                        }
                    }
                }
            }
#endif
            properties_list[rank] = event->coll_event;
        } else {
            // matched_collective_exists = match_collective(coll_queues.at(collective_key), matched_record, rank); //check againg with exclusive right
            peat_throw_error("Matched collective has significant rank absent\n"); // it's verified during match_collective so shouldn't happen
            assert(0);
            // goto add_new_collective;
        }
        if(expected_ranks.empty()) {
            if(check_completion(matched_record)) {
                print_collective_bind(coll_get_record(matched_record));
                // queues.at(key).erase(matched_record);
                erase_collective(key, matched_record);
            }
        }
    } else {
        // add_new_collective://push collective into collective queue
        std::map<PEAT_Rank, Collective_Event *> properties_list;
        Collective_Ranks expected_ranks = comm_store->get_ranks_set_from_comm(event->coll_event->comm);
        Collective_Ranks init_ranks = expected_ranks;
        long ranks = (long)expected_ranks.size();
        expected_ranks.erase(rank); // delete self from waiting ranks
        MAT_Collective collective_entry;
#if defined PROCESS_WHOLE_EVENT
        if(event.func_class == PEAT_PROP_CLASS_COLLECTIVE) {
            if(event.Collective.rw_fields0 > 0) {
                if(event.send_bytes.size() == 1) {
                    collective_entry.send_bytes.set_scalar(event.send_bytes.at(0));
                } else {
                    for(Collective_Ranks::const_iterator it = init_ranks.begin(); it != init_ranks.end(); ++it) {
                        collective_entry.send_bytes.set_vector(event.send_bytes.at(it->second), it->first);
                    }
                }
            }
            if(event.Collective.rw_fields1 > 0) {
                if(event.recv_bytes.size() == 1) {
                    collective_entry.recv_bytes.set_scalar(event.recv_bytes.at(0));
                } else {
                    for(Collective_Ranks::const_iterator it = init_ranks.begin(); it != init_ranks.end(); ++it) {
                        collective_entry.recv_bytes.set_vector(event.recv_bytes.at(it->second), it->first);
                    }
                }
            }
        }
#endif
        properties_list[rank] = event->coll_event;
        collective_entry.expected_ranks = expected_ranks;
        collective_entry.init_ranks = init_ranks;
        collective_entry.coll_events_list = properties_list;
        collective_entry.comm = event->coll_event->comm;
        collective_entry.size = ranks;
        collective_entry.func_id = event->coll_event->ph.func_id;
        collective_entry.pc_level = pc_level;

        if(!expected_ranks.empty()) {
            coll_queues[key].collectives.push_back(collective_entry);
        } else {
            if(check_completion(collective_entry)) {
                print_collective_bind(collective_entry);
            } else {
                coll_queues[key].collectives.push_back(collective_entry);
            }
        }
    }
}

void Collectives::parse_collective(const PEAT_Rank rank, const Tractor_Event &event, PEAT_Bool is_remote)
{
    assert(ptp);
    assert(comm_store);
    if (event.func_id == MPI_FILE_OPEN_ID) {
        fh_comms[rank][event.MPIFIO.fh] = event.MPIFIO.comm;
    }
    if (event.func_id == MPI_FILE_CLOSE_ID) {
        fh_comms[rank].erase(event.MPIFIO.fh);
    }


    if (!is_remote)
        print_collective_out(event);

    // const PEAT_Rank root = event.Collective.root;// comm_store->get_rank_from_comm(comm, rank, event.Collective.root, event.tsc_time.func.in); //must be signed
    if (!(event.func_id == MPI_INIT_ID || event.func_id == MPI_INIT_THREAD_ID || event.func_id == MPI_FINALIZE_ID)) {
        ptp->add_rank_MPI(rank, event.tsc_time.func.out - event.tsc_time.func.in);
    }
    MAT_Collectives_Record* record = new MAT_Collectives_Record(event);

    if(!global_func_id.check_property(event.func_id, PEAT_PROP_COMM)) { // fio opretations without comm (usually read/wrie)
        if(global_func_id.check_property(event.func_id, PEAT_PROP_FILE_HANDLE)) {
            record->coll_event->comm = fh_comms.at(rank).at(event.MPIFIO.fh);
        } else {
            record->coll_event->comm = 2; // mpi_comm_world
        }
    } else {
        // Collectives, comms and fio with comm (opena/close)
        if(!global_func_id.check_property(event.func_id, PEAT_PROP_FILE_HANDLE) && !global_func_id.check_property(event.func_id, PEAT_PROP_FILE_POINTER_INPUT) && !global_func_id.check_property(event.func_id, PEAT_PROP_FILE_POINTER_OUTPUT)) {
            // not FIO
            if(event.func_class == PEAT_PROP_CLASS_COMM) {
                record->coll_event->comm = event.Comms.comm;
            } else {
                if(event.func_id >= MPI_COMM_CREATE_ID && event.Collective.comm == 0) {
                    record->coll_event->comm = 2;
                } else {
                    record->coll_event->comm = event.Collective.comm;
                }
            }
        } else {
            // FIO
            record->coll_event->comm = event.MPIFIO.comm;
        }
    }

    if(global_func_id.check_property(event.func_id, PEAT_PROP_PERSISTENT_REQUEST_INIT)) { // add persistent request to queue and finish
        MAT_Request_Store_Record tmp;
        tmp.request_type = REQUEST_TYPE_PERSISTENT;
        tmp.record = record;
        req_store->add_request(rank, record->coll_event->request, &tmp);
        return;
    }

    if (global_func_id.check_property(event.func_id, PEAT_PROP_DATA_COLLECTIVE) || event.func_class == PEAT_PROP_CLASS_COLLECTIVE || global_func_id.check_property(event.func_id, PEAT_PROP_PERSISTENT_REQUEST_START) ) { // since some FIO operation are also parsed here we need to check if it's really collective operation
		process_collective(rank, record, event.PControl.ctl,is_remote);
    } else {
        if (event.MPIFIO.request) {

            MAT_Request_Store_Record req;
            MAT_FIO_Record *record = new MAT_FIO_Record(event);
            req.record = record;
            req.request_type = REQUEST_TYPE_USER_FIO;
            req_store->add_request(rank, event.MPIFIO.request, &req);
        }
    }
}
PEAT_Bool Collectives::add_pers_req_start(const PEAT_Rank rank, MAT_Collectives_Record* record)
{
    /*MAT_Request_Store_Record tmp;
    tmp.request_type = REQUEST_TYPE_USER_COLLECTIVE;
    tmp.record = record;
    req_store->add_request(rank, record->coll_event->request, &tmp);*/
	process_collective(rank, record, record->pc_level, false);
    return true;
}

PEAT_Bool Collectives::check_completion(const MAT_Collective &record)
{
    for (std::map<PEAT_Rank, Collective_Event*>::const_iterator c_it = record.coll_events_list.cbegin(); c_it != record.coll_events_list.cend(); ++c_it) {
        if (c_it->second->request > 0)
            return false;
    }
    return true;
}

PEAT_Bool Collectives::check_completion(collective_list::iterator &record)
{
    for (std::map<PEAT_Rank, Collective_Event*>::const_iterator c_it = coll_get_value(record, coll_events_list).cbegin(); c_it != coll_get_value(record, coll_events_list).cend(); ++c_it) {
        if (c_it->second->request > 0)
            return false;
    }
    return true;
}
PEAT_Bool Collectives::erase_collective(collective_list::iterator &record)
{
    collective_key c_key(coll_get_value(record, comm), coll_get_value(record, func_id));
//    coll_queues[c_key].collectives.erase(record);
#ifdef UPDATE_FIRST_ENTRY
    // invalidate all first iterators
    for (std::map<PEAT_Rank, collective_list::iterator>::iterator first_it = coll_queues[c_key].first.begin(); first_it != coll_queues[c_key].first.end(); ++first_it) {
        if (first_it->second == record)
            first_it->second = coll_queues[c_key].collectives.end(); // corect it later
    }
#endif
    coll_queues[c_key].collectives.erase(record);
    if (coll_queues.at(c_key).collectives.empty())
        coll_queues.erase(c_key);
    return true;
}
PEAT_Bool Collectives::erase_collective(collective_key &c_key, collective_list::iterator &record)
{
//    coll_queues[c_key].collectives.erase(record);
#ifdef UPDATE_FIRST_ENTRY
    for (std::map<PEAT_Rank, collective_list::iterator>::iterator first_it = coll_queues[c_key].first.begin(); first_it != coll_queues[c_key].first.end(); ++first_it) {
        if (first_it->second == record)
            first_it->second = coll_queues[c_key].collectives.end(); // corect it later
    }
#endif
    coll_queues[c_key].collectives.erase(record);
    if (coll_queues.at(c_key).collectives.empty())
        coll_queues.erase(c_key);
    return true;
}
PEAT_Bool Collectives::find_collective(const PEAT_Rank rank, const MAT_Request_Store_Record *rec, collective_list::iterator* record)
{
	MAT_Collectives_Record* event = static_cast<MAT_Collectives_Record*>(rec->record);
    collective_key c_key(event->coll_event->comm, event->get_ph().func_id);
    if(coll_queues.count(c_key)) {
        return match_collective(coll_queues.at(c_key), *record, rank);
    }
    return false;
}


/*
    This 'find' uses inverse logic : instead of finding the collective there this rank is NOT(yet) arrived and registered
    it finds the collective where it ALREADY registered but not yet 'completed'
 */
PEAT_Bool Collectives::find_collective(const PEAT_Rank rank, Collective_Event* event, collective_list::iterator* record)
{
    collective_key c_key(event->comm, event->ph.func_id);
    if(coll_queues.count(c_key)) {
        for(collective_list::iterator it = coll_queues.at(c_key).collectives.begin(); it != coll_queues.at(c_key).collectives.end(); ++it) {
            Collective_Ranks& checked_hash = coll_get_value(it, expected_ranks);
            // assert(!checked_hash.empty()); // No longer valid due to non-blocking collectgives
            if(!checked_hash.count(rank) && coll_get_value(it, coll_events_list).at(rank)->request == event->permanent_request) {
                *record = it;
                return true;
            }
        }
    }
    return false;
}

PEAT_Bool Collectives::update_request(const PEAT_Rank rank, const PEAT_MPI_Request request, const Phase &phase, Collective_Event *event, PEAT_Bool is_remote)
{
    collective_list::iterator coll;
    if(find_collective(rank,event, &coll)) {
        assert(coll_get_value(coll, coll_events_list).at(rank)->request == request);
        coll_get_value(coll, coll_events_list).at(rank)->request = 0;
        request_func_store->print_req_bind(rank, request, coll_get_value(coll, coll_events_list).at(rank)->ph, phase, is_remote);
        if(coll_get_value(coll, expected_ranks).empty()) {

            if(check_completion(coll)) {
                print_collective_bind(coll_get_record(coll));
                erase_collective(coll);
            } else {
                int id = 0;
            }
        }
    }
    return true;
}

#ifdef PEAT_MPI
void Collectives::process_remote_collective(PEAT_Signed_Num size, PEAT_Rank src)
{
#if defined(_WIN32) && !defined(__MINGW32__)
    char *buffer = (char *)_malloca(size);
#else
    char buffer[size];
#endif
    MPI_Status status;
    MPI_Recv(buffer, size, MPI_CHAR, src, TRACTOR_COLLECTIVE_EVENT_TAG, mat_compute_comm, &status);
    Tractor_Event temp_event;
    temp_event.unpack(buffer);
    parse_collective(temp_event.rank, temp_event, true);
}
void Collectives::send_collective(PEAT_Rank rank, const Tractor_Event &event)
{
    PEAT_MPI_Comm comm;
    const PEAT_MPI_Func_ID mpifunc = event.func_id;
    if (mpifunc == MPI_FILE_OPEN_ID) {
        fh_comms[rank][event.MPIFIO.fh] = event.MPIFIO.comm;
    }
    if (mpifunc == MPI_FILE_CLOSE_ID) {
        fh_comms[rank].erase(event.MPIFIO.fh);
    }

    if (!global_func_id.check_property(mpifunc, PEAT_PROP_COMM)) {
        if (global_func_id.check_property(mpifunc, PEAT_PROP_FILE_HANDLE)) {
            comm = fh_comms.at(rank).at(event.MPIFIO.fh);
        } else {
            comm = 2; // mpi_comm_world
        }
    } else {
        // Temporal stub
        if (!global_func_id.check_property(mpifunc, PEAT_PROP_FILE_HANDLE) && !global_func_id.check_property(event.func_id, PEAT_PROP_FILE_POINTER_INPUT) && !global_func_id.check_property(event.func_id, PEAT_PROP_FILE_POINTER_OUTPUT)) {
            if (mpifunc >= MPI_COMM_CREATE_ID && event.Collective.comm == 0) {
                comm = 2;
            } else {
                comm = event.Collective.comm;
            }
        } else {
            comm = event.MPIFIO.comm;
        }
    }

    if (global_func_id.check_property(mpifunc, PEAT_PROP_DATA_COLLECTIVE) || event.func_class == PEAT_PROP_CLASS_COLLECTIVE || event.func_class == PEAT_PROP_REQUEST) { 
        // since some FIO operation are also parsed here we need to check if it's really collective operation
        PEAT_Event_Delivery delivery;
        PEAT_Signed_Num size = (PEAT_Signed_Num)event.pack_size();
        collective_key cKey(comm, event.func_id);
        PEAT_Rank mat_peer = collective_handlers[rank][cKey];
        collective_handlers[rank][cKey]++;
        collective_handlers[rank][cKey] %= mat_compute_comm_size;
        if(mpifunc == MPI_FILE_OPEN_ID) {
            fh_comms_redirect[rank][event.MPIFIO.fh] = mat_peer;
        }
        if (global_func_id.check_property(mpifunc, PEAT_PROP_FILE_HANDLE)) {
            mat_peer = fh_comms_redirect.at(rank).at(event.MPIFIO.fh);
        }
#ifdef USE_LOCAL_PARSE
        if (mat_peer == my_rank) {
            parse_collective(rank, event, false);
            return;
        }
#endif
        PEAT_MPI_Request request = event.func_class == PEAT_PROP_CLASS_FIO ? event.MPIFIO.request : event.Collective.request;
        if (request > 0) {
            MAT_Request_Record record;
            record.peer = mat_peer;
            record.ids = event.ids;
            record.request = request;
            if(global_func_id.check_property(event.func_id, PEAT_PROP_PERSISTENT_REQUEST_INIT)) {
                req_store->flown_away_pers_requests[rank][record.request] = record;
            } else {
                req_store->flown_away_user_requests[rank][record.request].push_back(record);
            }
        }

        print_collective_out(event);
        delivery.buffer = (char *)peat_malloc_aligned(size, 64);
        delivery.receiver = mat_peer;
        delivery.func = event.func_id;
        event.pack(delivery.buffer);
        MPI_Isend(delivery.buffer, size, MPI_CHAR, mat_peer, TRACTOR_COLLECTIVE_EVENT_TAG, mat_compute_comm, &delivery.request);
        active_deliveries->push_back(delivery);
    } else {
        parse_collective(rank, event, false);
    }
}
PEAT_Num Collectives::check_completions()
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
/*
void Collectives::print_collective_r(int size, int src) {

}

PEAT_Bool Collectives::get_flown_away_req(const PEAT_Rank rank, const PEAT_MPI_Request request, Phase& ph, PEAT_Bool del)
{
    return false;
}
*/
#endif

void Collectives::process_unbound_events(PEAT_Rank my_rank)
{
    for (std::map<collective_key, collective_list_struct>::iterator coll_it = coll_queues.begin(); coll_it != coll_queues.end(); ++coll_it) {
        if (!coll_it->second.collectives.empty()) {
            peat_throw_error("Collective queue not empty\n");
            for (collective_list::iterator it = coll_it->second.collectives.begin(); it != coll_it->second.collectives.end(); ++it) {
                print_collective_err(coll_get_record(it),my_rank);
            }
        }
    }
}
void Collectives::print_sizes() const
{
    printf("Collective Queues");
    size_t num = 0;
    PEAT_Num keys = 0;
    for (std::map<collective_key, collective_list_struct>::const_iterator it = coll_queues.begin(); it != coll_queues.end(); ++it, keys++) {
        num += it->second.collectives.size();
    }
    printf(":k%zd %zd\n", keys, num);
}

#ifdef PEAT_MPI
PEAT_Signed_Num Collectives::check_unbound_events()
{
    int total_unbound = 0;
    for (std::map<collective_key, collective_list_struct>::iterator coll_it = coll_queues.begin(); coll_it != coll_queues.end(); ++coll_it) {
        if (!coll_it->second.collectives.empty()) {
            for (collective_list::iterator it = coll_it->second.collectives.begin(); it != coll_it->second.collectives.end(); ++it) {
                total_unbound += 2;
            }
        }
    }
    return total_unbound;
}
#endif

PEAT_Num Collectives::collective_queue_size() const
{
    PEAT_Num num = 0;
    for (std::map<collective_key, collective_list_struct>::const_iterator it = coll_queues.begin(); it != coll_queues.end(); ++it) {
        num += it->second.collectives.size();
    }
    return num;
}

std::ostream &operator<<(std::ostream &dbg, const collective_key &key)
{
    dbg << "comm " << key.comm << " func_id: <<" << key.func_id << "\n"
        << std::endl;
    return dbg;
}

std::ostream &operator<<(std::ostream &dbg, const Collective_Event &ph)
{
    dbg << ph.tsc_time.func.in << "," << ph.tsc_time.func.out << std::endl;
    return dbg;
}

std::ostream &operator<<(std::ostream &dbg, const std::map<PEAT_Rank, Collective_Event> &req)
{
    dbg << "{";
    for (std::map<PEAT_Rank, Collective_Event>::const_iterator it = req.begin(); it != req.end(); ++it) {
        dbg << "[" << it->first << "] = {" << it->second << "}";
    }
    dbg << "}";
    return dbg;
}
