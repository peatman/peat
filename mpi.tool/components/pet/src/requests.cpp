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

#include "requests.h"
#include <cstring>

PEAT_Num Request::pack_size() const
{ // Not object size. Just size for data serialization.
    return sizeof(request) + sizeof(rank) + sizeof(peer_rank) + sizeof(tag) + sizeof(acknowledgement_sent) +
           sizeof(comm) + sizeof(size) + sizeof(start_time) + sizeof(start_time_orig) + sizeof(uniq_id) +
           sizeof(func_id) + sizeof(kind) + sizeof(completed) + sizeof(canceled) + sizeof(persistent);
}

PEAT_Num Request::pack(char *ptr) const
{
    PEAT_Num ssize = 0, ptr_it = 0;
    ssize = sizeof(request);
    memcpy(ptr + ptr_it, &request, ssize);
    ptr_it += ssize;
    ssize = sizeof(rank);
    memcpy(ptr + ptr_it, &rank, ssize);
    ptr_it += ssize;
    ssize = sizeof(peer_rank);
    memcpy(ptr + ptr_it, &peer_rank, ssize);
    ptr_it += ssize;
    ssize = sizeof(tag);
    memcpy(ptr + ptr_it, &tag, ssize);
    ptr_it += ssize;
    ssize = sizeof(acknowledgement_sent);
    memcpy(ptr + ptr_it, &acknowledgement_sent, ssize);
    ptr_it += ssize;
    ssize = sizeof(comm);
    memcpy(ptr + ptr_it, &comm, ssize);
    ptr_it += ssize;
    ssize = sizeof(size);
    memcpy(ptr + ptr_it, &size, ssize);
    ptr_it += ssize;
    ssize = sizeof(start_time);
    memcpy(ptr + ptr_it, &start_time, ssize);
    ptr_it += ssize;
    ssize = sizeof(start_time_orig);
    memcpy(ptr + ptr_it, &start_time_orig, ssize);
    ptr_it += ssize;
    ssize = sizeof(uniq_id);
    memcpy(ptr + ptr_it, &uniq_id, ssize);
    ptr_it += ssize;
    ssize = sizeof(func_id);
    memcpy(ptr + ptr_it, &func_id, ssize);
    ptr_it += ssize;
    ssize = sizeof(kind);
    memcpy(ptr + ptr_it, &kind, ssize);
    ptr_it += ssize;
    ssize = sizeof(completed);
    memcpy(ptr + ptr_it, &completed, ssize);
    ptr_it += ssize;
    ssize = sizeof(canceled);
    memcpy(ptr + ptr_it, &canceled, ssize);
    ptr_it += ssize;
    ssize = sizeof(persistent);
    memcpy(ptr + ptr_it, &persistent, ssize);
    ptr_it += ssize;
    return ptr_it;
}

PEAT_Num Request::unpack(const char *ptr)
{
    PEAT_Num ssize = 0, ptr_it = 0;
    ssize = sizeof(request);
    memcpy(&request, ptr + ptr_it, ssize);
    ptr_it += ssize;
    ssize = sizeof(rank);
    memcpy(&rank, ptr + ptr_it, ssize);
    ptr_it += ssize;
    ssize = sizeof(peer_rank);
    memcpy(&peer_rank, ptr + ptr_it, ssize);
    ptr_it += ssize;
    ssize = sizeof(tag);
    memcpy(&tag, ptr + ptr_it, ssize);
    ptr_it += ssize;
    ssize = sizeof(acknowledgement_sent);
    memcpy(&acknowledgement_sent, ptr + ptr_it, ssize);
    ptr_it += ssize;
    ssize = sizeof(comm);
    memcpy(&comm, ptr + ptr_it, ssize);
    ptr_it += ssize;
    ssize = sizeof(size);
    memcpy(&size, ptr + ptr_it, ssize);
    ptr_it += ssize;
    ssize = sizeof(start_time);
    memcpy(&start_time, ptr + ptr_it, ssize);
    ptr_it += ssize;
    ssize = sizeof(start_time_orig);
    memcpy(&start_time_orig, ptr + ptr_it, ssize);
    ptr_it += ssize;
    ssize = sizeof(uniq_id);
    memcpy(&uniq_id, ptr + ptr_it, ssize);
    ptr_it += ssize;
    ssize = sizeof(func_id);
    memcpy(&func_id, ptr + ptr_it, ssize);
    ptr_it += ssize;
    ssize = sizeof(kind);
    memcpy(&kind, ptr + ptr_it, ssize);
    ptr_it += ssize;
    ssize = sizeof(completed);
    memcpy(&completed, ptr + ptr_it, ssize);
    ptr_it += ssize;
    ssize = sizeof(canceled);
    memcpy(&canceled, ptr + ptr_it, ssize);
    ptr_it += ssize;
    ssize = sizeof(persistent);
    memcpy(&persistent, ptr + ptr_it, ssize);
    ptr_it += ssize;
    return ptr_it;
}
