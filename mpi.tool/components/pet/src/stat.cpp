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

#include "stat.h"

#include "pet_event.h"
#include <cassert>
#include <cstdlib>
#include <iomanip>

PEAT_Clock Stat::get_max_event_length() const
{
    PEAT_Clock result = 0;
    for (std::map<PEAT_MPI_Func_ID, PEAT_Clock>::const_iterator it = max_event_length.begin(); it != max_event_length.end(); ++it) {
        if (result < it->second) {
            result = it->second;
        }
    }
    return result;
}

PEAT_Clock Stat::get_max_msg_length() const
{
    PEAT_Clock result = 0;
    for (std::map<PEAT_MPI_Func_ID, std::map<PEAT_Message_Size, PEAT_Clock>>::const_iterator func = max_msg_length.begin(); func != max_msg_length.end(); ++func) {
        for (std::map<PEAT_Message_Size, PEAT_Clock>::const_iterator size = func->second.begin(); size != func->second.end(); ++size) {
            if (result < size->second) {
                result = size->second;
            }
        }
    }
    return result;
}

PEAT_Clock Stat::get_min_event_length() const
{
    PEAT_Clock result = LLONG_MAX;
    if (min_event_length.empty()) {
        return 0;
    }
    for (std::map<PEAT_MPI_Func_ID, PEAT_Clock>::const_iterator it = min_event_length.begin(); it != min_event_length.end(); ++it) {
        if (result > it->second) {
            result = it->second;
        }
    }
    return result;
}

PEAT_Clock Stat::get_min_msg_length() const
{
    PEAT_Clock result = LLONG_MAX;
    for (std::map<PEAT_MPI_Func_ID, std::map<PEAT_Message_Size, PEAT_Clock>>::const_iterator func = min_msg_length.begin(); func != min_msg_length.end(); ++func) {
        for (std::map<PEAT_Message_Size, PEAT_Clock>::const_iterator size = func->second.begin(); size != func->second.end(); ++size) {
            if (result > size->second) {
                result = size->second;
            }
        }
    }
    return result;
}

void Stat::add_mpi_msg_len_stat(const PEAT_MPI_Func_ID func_id, const PEAT_Message_Size msg_send_size, const PEAT_Clock msg_length)
{
    if (msg_length >= 0) {
        if (!max_msg_length.count(func_id)) {
            max_msg_length[func_id][msg_send_size] = msg_length;
        }
        if (!max_msg_length.at(func_id).count(msg_send_size)) {
            max_msg_length.at(func_id)[msg_send_size] = msg_length;
        }
        if (max_msg_length.at(func_id).at(msg_send_size) < msg_length) {
            max_msg_length.at(func_id).at(msg_send_size) = msg_length;
        }
        if (!min_msg_length.count(func_id)) {
            min_msg_length[func_id][msg_send_size] = msg_length;
        }
        if (!min_msg_length.at(func_id).count(msg_send_size)) {
            min_msg_length.at(func_id)[msg_send_size] = msg_length;
        }
        if (min_msg_length.at(func_id).at(msg_send_size) > msg_length) {
            min_msg_length.at(func_id).at(msg_send_size) = msg_length;
        }
    }
}

void Stat::add_uc_stat(const PEAT_Clock len_in, PEAT_Bool changed)
{
    if (changed) {
        changed_UR_num++;
        covered_UR_length += len_in;
    } else {
        uncovered_UR_length += len_in;
    }
}

void Stat::add_mpi_stat(const PEAT_MPI_Func_ID func, const PEAT_Clock in, const PEAT_Clock out, const PEAT_Clock len_in, const PEAT_Clock cpu_len_in)
{
    // process events
    if (rank_start > in) {
        rank_start = in;
    }
    if (rank_end < out) {
        rank_end = out;
    } else {
        assert(0); //supposedly it should be monotonic
    }
    // process MPI_INIT
    if (func == MPI_INIT_ID || func == MPI_INIT_THREAD_ID) {
        if (rank_mpi_init_start != -1 || rank_mpi_init_end != -1) {
            std::cerr << "Second MPI_INIT. func_id " << func << "\n";
        }
        rank_mpi_init_start = in;
        rank_mpi_init_end = out;
    }
    // process MPI_FINALIZE
    if (func == MPI_FINALIZE_ID) {
        if (rank_mpi_finalize_start != -1 || rank_mpi_finalize_end != -1) {
            std::cerr << "Second MPI_Finalize. func_id " << func << "\n";
        }
        rank_mpi_finalize_start = in;
        rank_mpi_finalize_end = out;
        // assert(rank_mpi_finalize_end == rank_end);
    }
    num_events++;
    cpu_len += cpu_len_in;
    fabric_len += (len_in - cpu_len_in);
    if (!max_event_length.count(func)) {
        max_event_length[func] = len_in;
    }
    if (max_event_length[func] < len_in) {
        max_event_length[func] = len_in;
    }
    if (!min_event_length.count(func)) {
        min_event_length[func] = len_in;
    }
    if (min_event_length[func] > len_in) {
        min_event_length[func] = len_in;
    }
}

void Stat::add_mpi_pcontrol_stat(const PEAT_MPI_Func_ID func_id, const PEAT_MPI_Tag pc_tag, const PEAT_Clock in, const PEAT_Clock out)
{
    if (MPI_PCONTROL_ID == func_id) {
        if (0 != pc_tag) { // Tag zero is special. Used to flush the trace - not for mark regions
            const PEAT_MPI_Tag pcontrol_region_id = abs(pc_tag);
            MPI_PControl_Stat &reg_stat = pcontrol_regions[pcontrol_region_id];
            if (pc_tag < 0) { // stop measurement region
                if (LLONG_MAX == reg_stat.region_start) {
                    std::cerr << "Stat::add_mpi_pcontrol_stat End of MPI_Pcontrol(" << pc_tag << ") found with no start region mark\n";
                }
                reg_stat.region_end = __max__(reg_stat.region_end, in);
                ++reg_stat.region_count;
                reg_stat.region_length += in - reg_stat.pc_time;
            } else { // start measurement region
                // cerr << &pcontrol_regions << " DEBUG Region " << pcontrol_region_id << " start " << reg_stat.region_start << " TSC " << out << '\n';
                reg_stat.region_start = __min__(reg_stat.region_start, out);
                reg_stat.pc_time = out;
            }
        }
    }
}

void Stat::add_pcontrol_user_removed_len(const PEAT_MPI_Func_ID func_id, const PEAT_MPI_Tag pc_tag, const PEAT_Clock region_length)
{
    if (MPI_PCONTROL_ID == func_id) {
        // const PEAT_MPI_Tag pcontrol_region_id = abs(pc_tag);
        if (pc_tag > 0) { // start measurement region
            // MPI_PControl_Stat &reg_stat = pcontrol_regions[pcontrol_region_id];
            pcontrol_removed_user_code_length += region_length;
        } else {
            assert(!"Stat::add_pcontrol_user_removed_len called with negative Pcontrol region tag.");
        }
    }
}

std::ostream &operator<<(std::ostream &dbg, const Stat &stat)
{
    const PEAT_Ratio rank_len = PEAT_Ratio(stat.rank_end - stat.rank_start);
    const PEAT_Clock mpi_func_len = stat.cpu_len + stat.fabric_len;
    const PEAT_Clock max_event_length = stat.get_max_event_length();
    const PEAT_Clock min_event_length = stat.get_min_event_length();
    const PEAT_Clock max_msg_length = stat.get_max_msg_length();
    const PEAT_Clock min_msg_length = stat.get_min_msg_length();
    dbg << std::setprecision(0) << std::fixed
        << " " << stat.rank_start
        << "-" << stat.rank_end
        << ", msg_len " << min_msg_length
        << "-" << max_msg_length
        << ", event_len " << min_event_length
        << "-" << max_event_length
        << ", Events " << stat.num_events
        << "(" << stat.changed_UR_num << ")"
        << ", user_code_len " << stat.covered_UR_length
        << "+" << stat.uncovered_UR_length
        << std::setprecision(2) << "(" << PEAT_Ratio(stat.covered_UR_length) / rank_len << "+"
        << PEAT_Ratio(stat.uncovered_UR_length) / rank_len << ")"
        << ", mpi_active_len " << stat.cpu_len
        << ", mpi_passive_len " << stat.fabric_len
        << std::setprecision(2) << "(" << PEAT_Ratio(mpi_func_len) / rank_len << ")";
    return dbg;
}
void Event_Set_Stat::add_ts_fs_event_stat(const t_stat_type t, PET_Event *event)
{
    PEAT_Transports ct = event->get_msg_transport_id();
    PEAT_Func_ID_Pair fid_pair(event->func_id, event->parent_func_id);
    PEAT_Rank num_ranks = event->get_ranks_num();
    PEAT_Bool is_rndv = false, is_acked = false;
    if (event->get_event_type() == PET_EVENT_TYPE_TOR || event->get_event_type() == PET_EVENT_TYPE_TAR) {
        PET_ToR_Event *evt = (PET_ToR_Event *)event;
        if (evt->check_tx()) {
            is_rndv = evt->is_rdv(PEAT_DIRECTION_TX);
            is_acked = evt->check_up_status() || (evt->is_smp(PEAT_DIRECTION_TX) && is_rndv);
        }
    }
    if (ct < PEAT_TRANSPORT_MAX && event->get_ranks_num() > 1) {
        MPI_Event_Stat evt_stat;
        evt_stat.prolog = event->o_event_parts.prolog;
        evt_stat.fabric = event->o_event_parts.meat;
        evt_stat.centre = event->get_centre_code(Event_Set_Stat::tst_orig);
        evt_stat.epilog = event->o_event_parts.epilog;
        // evt_stat.wait = event->o_event_parts.stat.wait;
        // evt_stat.overlap = event->o_event_parts.stat.overlap;
        PEAT_Message_Size send_bytes = event->get_real_tx_bytes();
        PEAT_Message_Size recv_bytes = event->get_real_rx_bytes();
        evt_stat.total_send_length = send_bytes;
        evt_stat.total_recv_length = recv_bytes;
        if (send_bytes < 0)
            send_bytes = 0;
        if (recv_bytes < 0)
            recv_bytes = 0;
        evt_stat.is_rndv = is_rndv;
        evt_stat.is_acked = is_acked;
        evt_stat.num_events = 1;
        ts_fs_stat[t][ct][fid_pair][num_ranks][send_bytes][recv_bytes].push_back(evt_stat);
        // std::list<MPI_Event_Stat>& tmp_list = ts_fs_stat[t][ct][fid_pair][num_ranks][send_bytes][recv_bytes];
        // int size = tmp_list.size();
        // size++;
    }
    return;
}

void Event_Set_Stat::add_event_stat(const t_stat_type t, const PEAT_Transports ct, const PEAT_Rank rank, const PEAT_MPI_Func_ID func, const PEAT_MPI_Func_ID parent_func, const PEAT_Rank num_ranks, const PEAT_Message_Size send_size, const PEAT_Message_Size recv_size,
                                    const PEAT_Clock prolog, const PEAT_Clock fabric, const PEAT_Clock centre, const PEAT_Clock epilog, const PEAT_Clock wait, const PEAT_Clock overlap, PEAT_Bool is_rndv, PEAT_Bool is_not_verified)
{
    if (func == TMPI_FUNC_ID_NONE) {
        std::ostringstream tmp;
        tmp << "Empty MPI function ";
        throw std::runtime_error(tmp.str());
    }
    PEAT_Func_ID_Pair fid_pair(func, parent_func);
    if (send_size >= 0 && recv_size >= 0) {
        MPI_Event_Stat &sendrecv_stat = stat_per_func[t][rank][fid_pair][recv_size];
        ++sendrecv_stat.num_events;
        sendrecv_stat.prolog += prolog;
        sendrecv_stat.fabric += fabric;
        sendrecv_stat.epilog += epilog;
        sendrecv_stat.wait += wait;
        sendrecv_stat.overlap += overlap;
        if (send_size >= 0)
            sendrecv_stat.total_send_length += send_size;
        if (recv_size >= 0)
            sendrecv_stat.total_recv_length += recv_size;
        sendrecv_stat.cpu_min = __min__(sendrecv_stat.cpu_min, prolog + epilog);
        sendrecv_stat.cpu_max = __max__(sendrecv_stat.cpu_max, prolog + epilog);
        sendrecv_stat.fabric_min = __min__(sendrecv_stat.fabric_min, fabric);
        sendrecv_stat.fabric_max = __max__(sendrecv_stat.fabric_max, fabric);
    }
    if (send_size >= 0) {
        MPI_Event_Stat &send_stat = stat_per_func[t][rank][fid_pair][send_size];
        ++send_stat.num_events;
        send_stat.prolog += prolog;
        send_stat.fabric += fabric;
        send_stat.epilog += epilog;
        send_stat.wait += wait;
        send_stat.overlap += overlap;
        if (send_size >= 0)
            send_stat.total_send_length += send_size;
        send_stat.cpu_min = __min__(send_stat.cpu_min, prolog + epilog);
        send_stat.cpu_max = __max__(send_stat.cpu_max, prolog + epilog);
        send_stat.fabric_min = __min__(send_stat.fabric_min, fabric);
        send_stat.fabric_max = __max__(send_stat.fabric_max, fabric);
        return;
    }
    if (recv_size >= 0) {
        MPI_Event_Stat &recv_stat = stat_per_func[t][rank][fid_pair][recv_size];
        ++recv_stat.num_events;
        recv_stat.prolog += prolog;
        recv_stat.fabric += fabric;
        recv_stat.epilog += epilog;
        recv_stat.wait += wait;
        recv_stat.overlap += overlap;
        if (recv_size >= 0)
            recv_stat.total_recv_length += recv_size;
        recv_stat.cpu_min = __min__(recv_stat.cpu_min, prolog + epilog);
        recv_stat.cpu_max = __max__(recv_stat.cpu_max, prolog + epilog);
        recv_stat.fabric_min = __min__(recv_stat.fabric_min, fabric);
        recv_stat.fabric_max = __max__(recv_stat.fabric_max, fabric);
    }
}

void Event_Set_Stat::add_mpi_event_stat(const t_stat_type t, const PEAT_Rank rank, const PEAT_MPI_Func_ID func, const PEAT_Clock in, const PEAT_Clock out, const PEAT_Clock len_in, const PEAT_Clock cpu_len_in)
{
    Stat &evt_stat = stat[t][rank];
    evt_stat.add_mpi_stat(func, in, out, len_in, cpu_len_in);
}
void Event_Set_Stat::add_uc_event_stat(const t_stat_type t, const PEAT_Rank rank, const PEAT_Clock len_in, const PEAT_Bool changed)
{
    Stat &evt_stat = stat[t][rank];
    evt_stat.add_uc_stat(len_in, changed);
}

void Event_Set_Stat::add_mpi_length_stat(const t_stat_type t, const PEAT_Rank rank, const PEAT_MPI_Func_ID func, const PEAT_Message_Size msg_size, const PEAT_Clock msg_len)
{
    Stat &evt_stat = stat[t][rank];
    evt_stat.add_mpi_msg_len_stat(func, msg_size, msg_len);
}
