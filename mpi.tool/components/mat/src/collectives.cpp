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
#include <assert.h>
#include <sstream>

Collectives::Collectives(PTP *p, Communicators *c, int pos
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
    assert(c);
    assert(p);
    com_store = c;
    ptp = p;
#ifdef PEAT_MPI
    collective_handlers.resize(wR.size());
#endif
}

Collectives::~Collectives()
{
    ptp = 0;
    com_store = 0;
}
void Collectives::print_collective_out(const Tractor_Event &event)
{
#ifdef MAT_PROCESS_HWC_MPI_OUTPUT
    if (event.func_class == PEAT_PROP_CLASS_FIO) {
        output_MPI->printf(event.rank, "%lld:%lld:%d:%s:%x:%llx:%llx:%lld:%lld", event.primary_id, event.secondary_id, event.thread_id, global_func_id.name_by_idx(event.func_id),
                           event.MPIFIO.comm, event.MPIFIO.fh, event.MPIFIO.request, event.MPIFIO.offset, event.MPIFIO.bytes);
    } else {
        output_MPI->printf(event.rank, "%lld:%lld:%d:%s:%x:%d:%llx:%d:%d:%d", event.primary_id, event.secondary_id, event.thread_id, global_func_id.name_by_idx(event.func_id),
                           event.Collective.comm, event.Collective.root, event.Collective.request, event.Collective.rw_fields0, event.Collective.rw_fields1, event.num_colts);
        for (PEAT_Signed_Num j = 0; j < event.Collective.rw_fields0; j++) {
            output_MPI->printf(event.rank, ":%lld", event.send_bytes[j]);
        }
        for (PEAT_Signed_Num j = 0; j < event.Collective.rw_fields1; j++) {
            output_MPI->printf(event.rank, ":%lld", event.recv_bytes[j]);
        }
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
        output_HWC->printf(event.rank, ":%lld", event.addrs.at(i));
    }
    output_HWC->printf(event.rank, "\n");
    output_MPI->printf(event.rank, "\n");
#endif
}
void Collectives::print_collective_err(const MAT_Collective &record)
{
    std::string uids;
    std::string missing_ranks;
    char header[1024];
    if (!record.coll_events_list.empty()) {
        for (std::map<PEAT_Rank, Collective_Event>::const_iterator it = record.coll_events_list.begin(); it != record.coll_events_list.end(); ++it) {
            const PEAT_Rank rank = it->first;
            const Collective_Event &prop = it->second;
            char local_uids[32];
            sprintf(local_uids, ":%d:%lld", rank, prop.primary_id);
            uids = uids + local_uids;
        }
        for (Collective_Ranks::const_iterator it = record.expected_ranks.begin(); it != record.expected_ranks.end(); ++it) {
            const PEAT_Rank rank = *it;
            char local_uids[32];
            sprintf(local_uids, ":%d", rank);
            missing_ranks = missing_ranks + local_uids;
        }
        sprintf(header, "%d:%s", PEAT_RT_Bind_Collective, global_func_id.name_by_idx(record.func_id));
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
        for (std::map<PEAT_Rank, Collective_Event>::const_iterator it = record.coll_events_list.begin(); it != record.coll_events_list.end(); ++it) {
            const PEAT_Rank rank = it->first;
            const Collective_Event &prop = it->second;
            char local_uids[32];
            sprintf(local_uids, ":%d:%lld", rank, prop.primary_id);
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
    if (!record.coll_events_list.empty() && output_HWC->get_number_of_files() > 0) { // no timing data to compute stats/imbalance from when hwc files aren't read
        // col_file->write_line(header);
        // col_file->put_records(record);
        PEAT_Collective_Record r_col;
        r_col.func_id = record.func_id;
        r_col.send_bytes = record.send_bytes;
        r_col.recv_bytes = record.recv_bytes;

        r_col.comp_func = global_func_id.name_by_idx(record.func_id); // ???
        for (std::map<PEAT_Rank, Collective_Event>::const_iterator it = record.coll_events_list.begin(); it != record.coll_events_list.end(); ++it) {
            const PEAT_Rank rank = it->first;
            const Collective_Event &prop = it->second;
            // r_col.comp_funcs[rank] = global_func_id.name_by_idx(record.func_id); // ???
            // if (prop.permanent_request > 0) r_col.comp_funcs[rank] = global_func_id.name_by_idx(prop.completion.func_id);
            if (prop.permanent_request > 0)
                r_col.comp_func = global_func_id.name_by_idx(prop.completion.func_id); // TODO remove
            char local_uids[32];
            sprintf(local_uids, " %d,%lld", rank, prop.primary_id);
            uids = uids + local_uids;
        }
        r_col.root = record.root;
        r_col.size = record.size;
        r_col.comm = record.comm;
        int pc_level = record.pc_level;
        PEAT_Collective_Clocks r_col_clk = printed_collective[pc_level][r_col];
        PEAT_Clock max_in = 0;
        for (std::map<PEAT_Rank, Collective_Event>::const_iterator it = record.coll_events_list.begin(); it != record.coll_events_list.end(); ++it) {
            // PEAT_Rank rank = it->first;
            Collective_Event evt = it->second;
            PEAT_Clock duraton = evt.tsc_time.mpi.out - evt.tsc_time.mpi.in;
            r_col_clk.clocks[it->first].total_c += duraton;
            r_col_clk.clocks[it->first].occ++;
            if (r_col_clk.clocks[it->first].min_c == 0 || r_col_clk.clocks[it->first].min_c > duraton)
                r_col_clk.clocks[it->first].min_c = duraton;
            if (r_col_clk.clocks[it->first].max_c < duraton)
                r_col_clk.clocks[it->first].max_c = duraton;
            max_in = std::max(max_in, evt.tsc_time.mpi.in);
        }
        for (std::map<PEAT_Rank, Collective_Event>::const_iterator it = record.coll_events_list.begin(); it != record.coll_events_list.end(); ++it) {
            Collective_Event evt = it->second;
            PEAT_Clock in = evt.tsc_time.mpi.in;
            r_col_clk.clocks[it->first].imb_c += max_in - in;
        }
        printed_collective[pc_level][r_col] = r_col_clk;
    }
#endif
}

bool Collectives::match_collective(collective_list::iterator &first, collective_list::iterator &last, collective_list::iterator &record, PEAT_Rank rank) const
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

bool Collectives::match_collective(collective_list_struct &queue, collective_list::iterator &record, PEAT_Rank rank) const
{
    if (!queue.collectives.empty()) {
#ifdef UPDATE_FIRST_ENTRY
        if (queue.first.count(rank) < 1)
            queue.first[rank] = queue.collectives.begin();
        collective_list::iterator &first = queue.first.at(rank);
        collective_list::iterator last = queue.collectives.end();
        if (first == queue.collectives.end())
            first = queue.collectives.begin();
        bool ret = match_collective(first, last, record, rank);
        // if (ret) first = record;
        return ret;
#else
        return match_collective(queue.collectives.begin(), queue.collectives.end(), record, rank);
#endif
    }
    return false;
}

bool Collectives::empty() const
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

void Collectives::parse_collective(const PEAT_Rank rank, const Tractor_Event &event, bool is_remote)
{
    assert(ptp);
    assert(com_store);
    PEAT_MPI_Comm comm;
    const PEAT_MPI_Func_ID mpifunc = event.func_id;
    if (mpifunc == MPI_FILE_OPEN_ID) {
        fh_comms[event.MPIFIO.fh] = event.MPIFIO.comm;
    }
    if (mpifunc == MPI_FILE_CLOSE_ID) {
        fh_comms.erase(event.MPIFIO.fh);
    }

    if (!global_func_id.check_property(mpifunc, PEAT_PROP_COMM)) {
        if (global_func_id.check_property(mpifunc, PEAT_PROP_FILE_HANDLE)) {
            comm = fh_comms.at(event.MPIFIO.fh);
        } else {
            comm = 2; // mpi_comm_world
        }
    } else {
        // Temporal stub
        if (!global_func_id.check_property(mpifunc, PEAT_PROP_FILE_HANDLE) && !global_func_id.check_property(mpifunc, PEAT_PROP_FILE_POINTER_HANDLE)) {
            if (mpifunc >= MPI_COMM_CREATE_ID && event.Collective.comm == 0) {
                comm = 2;
            } else {
                comm = event.Collective.comm;
            }
        } else {
            comm = event.MPIFIO.comm;
        }
    }

    if (!is_remote)
        print_collective_out(event);

    // const PEAT_Rank root = event.Collective.root;// com_store->get_rank_from_comm(comm, rank, event.Collective.root, event.tsc_time.func.in); //must be signed
    if (!(mpifunc == MPI_INIT_ID || mpifunc == MPI_INIT_THREAD_ID || mpifunc == MPI_FINALIZE_ID)) {
        ptp->add_rank_MPI(rank, event.tsc_time.func.out - event.tsc_time.func.in);
    }
    if (global_func_id.check_property(mpifunc, PEAT_PROP_DATA_COLLECTIVE) || event.func_class == PEAT_PROP_CLASS_COLLECTIVE) { // since some FIO operation are also parsed here we need to check if it's really collective operation
        collective_key collective_key(comm, event.func_id);
        collective_list::iterator matched_record;
        bool matched_collective_exists = false;

        if (!coll_queues.empty()) {
            if (coll_queues.count(collective_key)) {
                // cerr << "Collective key " << collective_key << " mpifunc " << mpifunc << " comm " << comm << " count " << collective_queues.count(collective_key) << endl;
                matched_collective_exists = match_collective(coll_queues.at(collective_key), matched_record, rank);
#if 0 // def _DEBUG
                collective_list::iterator matched_record_debug;
                bool matched_collective_exists_debug = match_collective_old(coll_queues.at(collective_key), matched_record_debug, rank);
                if (matched_collective_exists_debug != matched_collective_exists) {
                    matched_collective_exists = match_collective(coll_queues.at(collective_key), matched_record, rank);
                };
                if (matched_collective_exists_debug) assert(matched_record_debug == matched_record);
#endif
            }
        }
        if (matched_collective_exists) {
        match_existing_collective:
            Collective_Ranks &expected_ranks = coll_get_value(matched_record, expected_ranks);
#if defined PROCESS_WHOLE_EVENT
            Collective_Ranks &init_ranks = matched_record->init_ranks;
#endif
            std::map<PEAT_Rank, Collective_Event> &properties_list = coll_get_value(matched_record, coll_events_list);
            assert(!expected_ranks.empty());
            if (expected_ranks.count(rank)) {
                Collective_Event col_prop;
                col_prop.tsc_time = event.tsc_time;
                col_prop.tsc_u_time = event.tsc_u_time;
                col_prop.addrs = event.addrs;
                col_prop.primary_id = event.primary_id;
                col_prop.secondary_id = event.secondary_id;
                col_prop.request = col_prop.permanent_request = event.func_class == PEAT_PROP_CLASS_FIO ? event.MPIFIO.request : event.Collective.request;
                expected_ranks.erase(rank);
#if defined PROCESS_WHOLE_EVENT
                if (event.func_class == PEAT_PROP_CLASS_COLLECTIVE) {
                    if (event.Collective.rw_fields0 > 0 && matched_record->send_bytes.empty()) {
                        if (event.send_bytes.size() == 1) {
                            matched_record->send_bytes.set_scalar(event.send_bytes.at(0));
                        } else {
                            for (Collective_Ranks::const_iterator it = init_ranks.begin(); it != init_ranks.end(); ++it) {
                                matched_record->send_bytes.set_vector(event.send_bytes.at(it->second), it->first);
                            }
                        }
                    }
                    if (event.Collective.rw_fields1 > 0 && matched_record->send_bytes.empty()) {
                        if (event.recv_bytes.size() == 1) {
                            matched_record->recv_bytes.set_scalar(event.recv_bytes.at(0));
                        } else {
                            for (Collective_Ranks::const_iterator it = init_ranks.begin(); it != init_ranks.end(); ++it) {
                                matched_record->recv_bytes.set_vector(event.recv_bytes.at(it->second), it->first);
                            }
                        }
                    }
                }
#endif
                properties_list[rank] = col_prop;
            } else {
                // matched_collective_exists = match_collective(coll_queues.at(collective_key), matched_record, rank); //check againg with exclusive right
                peat_throw_error("Matched collective has significant rank absent\n"); // it's verified during match_collective so shouldn't happen
                assert(0);
                // goto add_new_collective;
            }
            if (expected_ranks.empty()) {
                if (check_completion(matched_record)) {
                    print_collective_bind(coll_get_record(matched_record));
                    // coll_queues.at(collective_key).erase(matched_record);
                    erase_collective(collective_key, matched_record);
                }
            }
        } else {
            // add_new_collective://push collective into collective queue
            std::map<PEAT_Rank, Collective_Event> properties_list;
            Collective_Ranks expected_ranks = com_store->get_ranks_set_from_comm(comm);
            Collective_Ranks init_ranks = expected_ranks;
            long ranks = (long)expected_ranks.size();
            expected_ranks.erase(rank); // delete self from waiting ranks
            MAT_Collective collective_entry;
            Collective_Event properties;
            properties.tsc_time = event.tsc_time;
            properties.tsc_u_time = event.tsc_u_time;
            properties.addrs = event.addrs;
            properties.primary_id = event.primary_id;
            properties.secondary_id = event.secondary_id;
            properties.request = properties.permanent_request = event.func_class == PEAT_PROP_CLASS_FIO ? event.MPIFIO.request : event.Collective.request;
#if defined PROCESS_WHOLE_EVENT
            if (event.func_class == PEAT_PROP_CLASS_COLLECTIVE) {
                if (event.Collective.rw_fields0 > 0) {
                    if (event.send_bytes.size() == 1) {
                        collective_entry.send_bytes.set_scalar(event.send_bytes.at(0));
                    } else {
                        for (Collective_Ranks::const_iterator it = init_ranks.begin(); it != init_ranks.end(); ++it) {
                            collective_entry.send_bytes.set_vector(event.send_bytes.at(it->second), it->first);
                        }
                    }
                }
                if (event.Collective.rw_fields1 > 0) {
                    if (event.recv_bytes.size() == 1) {
                        collective_entry.recv_bytes.set_scalar(event.recv_bytes.at(0));
                    } else {
                        for (Collective_Ranks::const_iterator it = init_ranks.begin(); it != init_ranks.end(); ++it) {
                            collective_entry.recv_bytes.set_vector(event.recv_bytes.at(it->second), it->first);
                        }
                    }
                }
            }
#endif
            properties_list[rank] = properties;
            collective_entry.expected_ranks = expected_ranks;
            collective_entry.init_ranks = init_ranks;
            collective_entry.coll_events_list = properties_list;
            collective_entry.comm = comm;
            collective_entry.size = ranks;
            collective_entry.func_id = event.func_id;
            collective_entry.pc_level = event.PControl.ctl;

            if (!expected_ranks.empty()) {
                coll_queues[collective_key].collectives.push_back(collective_entry);
            } else {
                if (check_completion(collective_entry)) {
                    print_collective_bind(collective_entry);
                } else {
                    coll_queues[collective_key].collectives.push_back(collective_entry);
                }
            }
        }
    } else {
        if (event.MPIFIO.request) {
            MAT_Record record;
            record.pc_level = event.PControl.ctl;
            record.comm = comm;
            record.rank = rank;
            record.request = event.MPIFIO.request;
            Phase ph;
            ph.func_id = event.func_id;
            ph.thread_id = event.thread_id;
            ph.tsc_time = event.tsc_time;
            ph.tsc_u_time = event.tsc_u_time;
            ph.addrs = event.addrs;
            ph.primary_id = event.primary_id;
            ph.secondary_id = event.secondary_id;
            record.ph[0] = record.ph[1] = record.ph[2] = ph;
            fio_requests[rank][record.request] = record;
        }
    }
}

bool Collectives::check_completion(const MAT_Collective &record)
{
    for (std::map<PEAT_Rank, Collective_Event>::const_iterator c_it = record.coll_events_list.cbegin(); c_it != record.coll_events_list.cend(); ++c_it) {
        if (c_it->second.request > 0)
            return false;
    }
    return true;
}

bool Collectives::check_completion(collective_list::iterator &record)
{
    for (std::map<PEAT_Rank, Collective_Event>::const_iterator c_it = coll_get_value(record, coll_events_list).cbegin(); c_it != coll_get_value(record, coll_events_list).cend(); ++c_it) {
        if (c_it->second.request > 0)
            return false;
    }
    return true;
}
bool Collectives::erase_collective(collective_list::iterator &record)
{
    collective_key c_key(coll_get_value(record, comm), coll_get_value(record, func_id));
//    coll_queues[c_key].collectives.erase(record);
#ifdef UPDATE_FIRST_ENTRY
    // invalidate all first iterators
    for (std::map<PEAT_Rank, collective_list::iterator>::iterator first_it = coll_queues[c_key].first.begin(); first_it != coll_queues[c_key].first.end(); ++first_it) {
        if (first_it->second == record)
            first_it->second = coll_queues[c_key].collectives.end(); // corect it later
    }
    /*
    for (std::map<PEAT_Rank, collective_list::iterator>::iterator first_it = coll_queues[c_key].first.begin(); first_it != coll_queues[c_key].first.end(); ++first_it) {
        first_it->second = coll_queues[c_key].collectives.begin();
    }
    */
#endif
    coll_queues[c_key].collectives.erase(record);
    if (coll_queues.at(c_key).collectives.empty())
        coll_queues.erase(c_key);
    return true;
}
bool Collectives::erase_collective(collective_key &c_key, collective_list::iterator &record)
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

bool Collectives::find_request(PEAT_Rank rank, PEAT_MPI_Request request, collective_list::iterator &coll)
{
    for (std::map<collective_key, collective_list_struct>::iterator queue = coll_queues.begin(); queue != coll_queues.end(); ++queue) {
        for (collective_list::iterator it = queue->second.collectives.begin(); it != queue->second.collectives.end(); ++it) {
            if (coll_get_value(it, coll_events_list).count(rank) > 0) {
                Collective_Event &coll_event = coll_get_value(it, coll_events_list).at(rank);
                if (coll_event.request == request) {
                    coll = it;
                    return true;
                }
            }
        }
    }
    return false;
}
bool Collectives::update_request(PEAT_Rank rank, PEAT_MPI_Request request, const Phase &phase, collective_list::iterator &coll)
{
    coll_get_value(coll, coll_events_list).at(rank).completion = phase;
    coll_get_value(coll, coll_events_list).at(rank).request = 0;
    if (check_completion(coll) && coll_get_value(coll, expected_ranks).empty()) {
        print_collective_bind(coll_get_record(coll));
        erase_collective(coll);
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
        fh_comms[event.MPIFIO.fh] = event.MPIFIO.comm;
    }
    if (mpifunc == MPI_FILE_CLOSE_ID) {
        fh_comms.erase(event.MPIFIO.fh);
    }

    if (!global_func_id.check_property(mpifunc, PEAT_PROP_COMM)) {
        if (global_func_id.check_property(mpifunc, PEAT_PROP_FILE_HANDLE)) {
            comm = fh_comms.at(event.MPIFIO.fh);
        } else {
            comm = 2; // mpi_comm_world
        }
    } else {
        // Temporal stub
        if (!global_func_id.check_property(mpifunc, PEAT_PROP_FILE_HANDLE) && !global_func_id.check_property(mpifunc, PEAT_PROP_FILE_POINTER_HANDLE)) {
            if (mpifunc >= MPI_COMM_CREATE_ID && event.Collective.comm == 0) {
                comm = 2;
            } else {
                comm = event.Collective.comm;
            }
        } else {
            comm = event.MPIFIO.comm;
        }
    }

    if (global_func_id.check_property(mpifunc, PEAT_PROP_DATA_COLLECTIVE) || event.func_class == PEAT_PROP_CLASS_COLLECTIVE) { // since some FIO operation are also parsed here we need to check if it's really collective operation

        PEAT_Event_Delivery delivery;
        PEAT_Signed_Num size = (PEAT_Signed_Num)event.pack_size();
        collective_key cKey(comm, event.func_id);
        PEAT_Rank mat_peer = collective_handlers[rank][cKey];
        collective_handlers[rank][cKey]++;
        collective_handlers[rank][cKey] %= mat_compute_comm_size;
#ifdef USE_LOCAL_PARSE
        if (mat_peer == my_rank) {
            parse_collective(rank, event, false);
            return;
        }
#endif
        MAT_Request_Record record;
        record.peer = mat_peer;
        record.primary_id = event.primary_id;
        record.secondary_id = event.secondary_id;
        PEAT_MPI_Request request = event.func_class == PEAT_PROP_CLASS_FIO ? event.MPIFIO.request : event.Collective.request;
        if (request > 0) {
            record.request = request;
            flown_away_requests[rank][request] = record;
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

void Collectives::process_unbound_events()
{
    for (std::map<collective_key, collective_list_struct>::iterator coll_it = coll_queues.begin(); coll_it != coll_queues.end(); ++coll_it) {
        if (!coll_it->second.collectives.empty()) {
            peat_throw_error("Collective queue not empty\n");
            for (collective_list::iterator it = coll_it->second.collectives.begin(); it != coll_it->second.collectives.end(); ++it) {
                print_collective_err(coll_get_record(it));
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

bool Collectives::find_request(PEAT_Rank rank, PEAT_MPI_Request request, MAT_Record *ret_it, PEAT_Bool erase)
{
    if (!fio_requests[rank].empty()) {
        if (fio_requests[rank].count(request) > 0) {
            *ret_it = fio_requests[rank][request];
            if (erase)
                fio_requests[rank].erase(request);
            return true;
        }
    }
    return false;
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
