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

#include "pet_bind.h"
#include "file_io.h"
#include "utils.h"
#include <cstdio>
#include <set>

void PET_Bind_Store::parse_rank_file(const Global_Parameters &globals, PEAT_Rank rank, int id_columns, int verbose)
{
    std::map<PEAT_Rank, std::string> one_file;
    {
        std::set<PEAT_Rank> just_this_rank;
        just_this_rank.insert(rank);
        std::map<PEAT_Rank, std::string> found = get_file_names(globals.input_path, globals.trace_files, globals.bind_files, just_this_rank, false);
        if (found.count(rank) == 0)
            return; // no bind file for this rank: mat hasn't run (yet), or this rank isn't part of the trace
        one_file[rank] = found.at(rank);
    }
    File_Storage_GZ bind_file(one_file, PEAT_FIO_FLAG_READ | PEAT_FIO_FLAG_LAZY | PEAT_FIO_FLAG_ICLOSE, NULL, verbose);
    String_Split line;
    while (bind_file.read_lines(line, rank, ':')) {
        if (line.size() < 2)
            continue;
        PEAT_Record_Type type = PEAT_RT_Remark;
        extract(type, line.at(0));
        switch (type) {
        case PEAT_RT_Bind_Peer: { // func_A:rank_A:idsA:func_B:rank_B:idsB:size - both sides at phase[1] (the post); see P2PAL::print_P2PF
            PEAT_Rank rank_a = -1;
            extract(rank_a, line.at(2));
            PEAT_Event_IDs ids_a;
            int pos = peat_ids_parse(line, 3, id_columns, ids_a);
            PEAT_Rank rank_b = -1;
            extract(rank_b, line.at(pos + 1));
            PEAT_Event_IDs ids_b;
            peat_ids_parse(line, pos + 2, id_columns, ids_b);
            // the line is always sender-half:receiver-half (see P2PAL::print_P2PF); never symmetric
            send_peer_map[PET_Bind_Key(rank_a, ids_a)] = PET_Bind_Key(rank_b, ids_b);
            recv_peer_map[PET_Bind_Key(rank_b, ids_b)] = PET_Bind_Key(rank_a, ids_a);
            peer_count++;
        } break;
        case PEAT_RT_Bind_Completion:
        case PEAT_RT_Bind_Initiation:
        case PEAT_RT_Bind_Destroying: { // func_A:idsA:func_B:idsB (both records belong to this rank)
            PEAT_Event_IDs ids_a;
            int pos = peat_ids_parse(line, 2, id_columns, ids_a);
            PEAT_Event_IDs ids_b;
            peat_ids_parse(line, pos + 1, id_columns, ids_b);
            PET_Bind_Key key_a(rank, ids_a), key_b(rank, ids_b);
            completion_map[key_a] = key_b; // A is the initiator/request: B completes it
            completed_map[key_b] = key_a;  // B is the completion: it completes A
            completion_count++;
        } break;
        case PEAT_RT_Bind_Collective: { // func:(rank:ids)*
            std::vector<PET_Bind_Key> members;
            for (int i = 2; i + id_columns < (int)line.size();) {
                PEAT_Rank member_rank = -1;
                extract(member_rank, line.at(i));
                PEAT_Event_IDs member_ids;
                i = peat_ids_parse(line, i + 1, id_columns, member_ids);
                members.push_back(PET_Bind_Key(member_rank, member_ids));
            }
            for (size_t i = 0; i < members.size(); i++)
                collective_map[members[i]] = members;
            collective_count++;
        } break;
        default:
            break;
        }
    }
}

void PET_Bind_Store::load(const Global_Parameters &globals, const Address_File_Storage &user_region_file, const std::vector<PEAT_Rank> &ranks, int verbose)
{
    for (size_t i = 0; i < ranks.size(); i++) {
        try {
            parse_rank_file(globals, ranks[i], user_region_file.get_id_columns(ranks[i]), verbose);
            loaded = true; // at least one rank's bind file was found and parsed
        } catch (const std::exception &e) {
            parse_errors++;
            if (verbose)
                fprintf(stderr, "PET_Bind_Store: failed to read bind file for rank %d: %s\n", ranks[i], e.what());
        }
    }
}

void PET_Bind_Store::print_stats(FILE *fp) const
{
    fprintf(fp, "Bind store: loaded=%d peers=%lld completions=%lld collectives=%lld parse_errors=%lld\n",
            (int)loaded, (long long)peer_count, (long long)completion_count, (long long)collective_count, (long long)parse_errors);
}

const PET_Bind_Key *PET_Bind_Store::find_send_peer(PEAT_Rank rank, const PEAT_Event_IDs &ids) const
{
    std::map<PET_Bind_Key, PET_Bind_Key>::const_iterator it = send_peer_map.find(PET_Bind_Key(rank, ids));
    return it == send_peer_map.end() ? NULL : &it->second;
}

const PET_Bind_Key *PET_Bind_Store::find_recv_peer(PEAT_Rank rank, const PEAT_Event_IDs &ids) const
{
    std::map<PET_Bind_Key, PET_Bind_Key>::const_iterator it = recv_peer_map.find(PET_Bind_Key(rank, ids));
    return it == recv_peer_map.end() ? NULL : &it->second;
}

const PET_Bind_Key *PET_Bind_Store::find_completion(PEAT_Rank rank, const PEAT_Event_IDs &ids) const
{
    std::map<PET_Bind_Key, PET_Bind_Key>::const_iterator it = completion_map.find(PET_Bind_Key(rank, ids));
    return it == completion_map.end() ? NULL : &it->second;
}

const PET_Bind_Key *PET_Bind_Store::find_completed_event(PEAT_Rank rank, const PEAT_Event_IDs &ids) const
{
    std::map<PET_Bind_Key, PET_Bind_Key>::const_iterator it = completed_map.find(PET_Bind_Key(rank, ids));
    return it == completed_map.end() ? NULL : &it->second;
}

const std::vector<PET_Bind_Key> *PET_Bind_Store::find_collective(PEAT_Rank rank, const PEAT_Event_IDs &ids) const
{
    std::map<PET_Bind_Key, std::vector<PET_Bind_Key>>::const_iterator it = collective_map.find(PET_Bind_Key(rank, ids));
    return it == collective_map.end() ? NULL : &it->second;
}
