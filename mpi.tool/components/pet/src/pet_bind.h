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

#ifndef __PET_BIND_H__
#define __PET_BIND_H__

#include "common_types.h"
#include <map>
#include <vector>

/* Identity of one trace record for bind-file lookups: (rank, full id path). Mirrors PET_Event_Definition
   (pet_core.h) but doesn't need a thread id, since a bind file line always names a specific record unambiguously. */
struct PET_Bind_Key {
    PEAT_Rank rank;
    PEAT_Event_IDs ids;
    PET_Bind_Key() : rank(-1) {}
    PET_Bind_Key(PEAT_Rank r, const PEAT_Event_IDs &i) : rank(r), ids(i) {}
    PEAT_Bool operator<(const PET_Bind_Key &rhs) const
    {
        if (rank != rhs.rank)
            return rank < rhs.rank;
        return ids < rhs.ids;
    }
    PEAT_Bool valid() const { return rank >= 0 && !ids.empty() && ids[0] >= 0; }
};

/* Reads the tractor_t.bind.<rank>.out.gz files mat writes (peer/completion/collective relationships,
   computed once, offline, over the whole trace) and serves them as direct (rank,ids) lookups, replacing
   PET's own live peer/completion/collective search (find_peer, find_colt_peer_anywhere, Coll_Storage). */
class PET_Bind_Store
{
    /* mat's Bind_Peer line is always "sender-half : receiver-half" (see P2PAL::print_P2PF), never symmetric.
       A plain send/recv record appears in only one of these maps, consistently. A sendrecv (TaR) record
       shares one (rank,ids) for both roles - its send-half and receive-half are matched independently by
       mat - so it legitimately gets an entry in *both* maps under the same key: send_peer_map for who it
       sends to, recv_peer_map for who it receives from. */
    std::map<PET_Bind_Key, PET_Bind_Key> send_peer_map; // this record is the sender-half of some line: who receives it
    std::map<PET_Bind_Key, PET_Bind_Key> recv_peer_map; // this record is the receiver-half of some line: who sends to it
    std::map<PET_Bind_Key, PET_Bind_Key> completion_map;  // initiator/request -> the record that completes/starts it (Bind_Completion/Initiation/Destroying)
    std::map<PET_Bind_Key, PET_Bind_Key> completed_map;   // reverse of completion_map: completion record -> what it completes
    std::map<PET_Bind_Key, std::vector<PET_Bind_Key>> collective_map; // any member -> every member of that collective call (Bind_Collective)
    PEAT_Bool loaded;
    PEAT_LNum peer_count, completion_count, collective_count, parse_errors;

  public:
    PET_Bind_Store() : loaded(false), peer_count(0), completion_count(0), collective_count(0), parse_errors(0) {}
    /* Reads <input_path>/<trace_files>.bind.<rank>.out.gz for every rank in ranks (mat's default location
       for a matching-only run; see mat's -m/--matching-only option). Missing files are not an error: is_loaded()
       is only true once at least one bind file was found, so callers can fall back or report clearly.
       user_region_file supplies each rank's id_columns (see Address_File_Storage::get_id_columns). */
    void load(const Global_Parameters &globals, const Address_File_Storage &user_region_file, const std::vector<PEAT_Rank> &ranks, int verbose);
    PEAT_Bool is_loaded() const { return loaded; }
    void print_stats(FILE *fp) const;

    const PET_Bind_Key *find_send_peer(PEAT_Rank rank, const PEAT_Event_IDs &ids) const; // this record sends: who receives it
    const PET_Bind_Key *find_recv_peer(PEAT_Rank rank, const PEAT_Event_IDs &ids) const; // this record receives: who sends to it
    const PET_Bind_Key *find_completion(PEAT_Rank rank, const PEAT_Event_IDs &ids) const;      // this record is the initiator: what completes it
    const PET_Bind_Key *find_completed_event(PEAT_Rank rank, const PEAT_Event_IDs &ids) const; // this record is the completion: what it completes
    const std::vector<PET_Bind_Key> *find_collective(PEAT_Rank rank, const PEAT_Event_IDs &ids) const;

  private:
    void parse_rank_file(const Global_Parameters &globals, PEAT_Rank rank, int id_columns, int verbose);
};

#endif //__PET_BIND_H__
