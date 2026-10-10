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

#ifndef __REQUESTS_H__
#define __REQUESTS_H__

#include <string>
#include <vector>

#include "collectives.h"
#include "common_types.h"
#include "file_io.h"
#include "p2pal.h"
#include "ptp.h"
#include "tevent.h"

class Request : public Outputs
{
    int skip_locals;

  public:
    Request(int s,
#ifdef PEAT_MPI
            std::list<PEAT_Event_Delivery> *ad, const std::vector<PEAT_Rank> &wR, const std::vector<PEAT_Rank> &mRs, const MPI_Comm mat_cc, const PEAT_Rank m_r,
#endif
            int v = false);
    ~Request();

    void Initialize(PTP *p, Communicators *c, Request_Store *r, P2PAL *pp, Collectives *cl, File_Storage_GZ *MPI, File_Storage_GZ *HWC, File_Storage_GZ *bind);

    void parse_cn(const PEAT_Rank rank, Tractor_Event &event, PEAT_Bool is_remote);
    bool parse_P2P_completion_phase(PEAT_Rank rank, const Request_Store_Record_Type type, const Phase &phase,
                                    const PEAT_MPI_Request request,
                                    const PEAT_MPI_Tag tag,
                                    const PEAT_Rank source,
                                    const PEAT_Message_Size size,
                                    const int pc_level,
                                    bool is_remote);
    Request_Store_Record_Type  select_event(PEAT_Rank rank, const PEAT_MPI_Request request, bool is_remote);
    bool discard_P2P_event(const PEAT_Rank rank, const PEAT_MPI_Request request, bool is_remote);
    bool discard_collective_event(const PEAT_Rank rank, const PEAT_MPI_Request request, bool is_remote);

    void parse_P2P_start_phase(const PEAT_Rank rank, const Phase& phase, const PEAT_MPI_Request request, MAT_Request_Store_Record* ret_it, const int pc_level, bool is_remote);
    void parse_collective_start_phase(const PEAT_Rank rank, const Phase& phase, const PEAT_MPI_Request request, MAT_Request_Store_Record* ret_it, const int pc_level, bool is_remote);
    bool parse_collective_completion_phase(const PEAT_Rank rank, const Phase &phase, const PEAT_MPI_Request request, const int pc_level, bool is_remote);

    void parse_pers_start_phase(const PEAT_Rank rank, const Phase &phase, const PEAT_MPI_Request request, const int pc_level, bool is_remote);
    bool remove_pers_req(const PEAT_Rank rank, const PEAT_MPI_Request request, MAT_Request_Store_Record* ret_it);
    void print_req_out(const Tractor_Event &event);
    void print_req_bind(PEAT_Rank rank, PEAT_MPI_Request req, const Phase &phase, const Phase &bind, bool is_remote);
    void print_req_bind(PEAT_Rank rank, const Collective_Entry &coll, const Phase &phase, bool is_remote);

    PTP *ptp;
    Communicators *comm_store;
    P2PAL *p2p_func_store;
    Collectives *coll_func_store;
    Request_Store* req_store;

    int verbose;
#ifdef PEAT_MPI
    // std::map<PEAT_Rank, std::list<MAT_Request_Record> > flown_away_requests;
    // std::map<PEAT_Rank, std::list<MAT_Request_Record> > flown_away_pers_requests;
    std::list<PEAT_Event_Delivery> *active_deliveries;
    MPI_Comm mat_compute_comm;
    std::vector<PEAT_Rank> whole_ranks;
    std::vector<PEAT_Rank> my_ranks;
    PEAT_Rank my_rank;

  public:
    void send_cn(const Tractor_Event &req_event, PEAT_Rank peer);
    /*
    void send_P2P_cn(const Tractor_Request_Event& req_event);
    void send_coll_cn(const Tractor_Request_Event& req_event);
    */
    void process_remote_request(PEAT_Signed_Num size, PEAT_Rank src);
    /*
    PEAT_Bool get_flown_away_pers_req(const PEAT_Rank rank, const PEAT_MPI_Request request, Phase& ph, PEAT_Bool del);
    PEAT_Bool get_flown_away_req(const PEAT_Rank rank, const PEAT_MPI_Request request, Phase& ph, PEAT_Bool del);
    PEAT_Bool get_flown_away_coll_req(const PEAT_Rank rank, const PEAT_MPI_Request request, Phase& ph, PEAT_Bool del);
    */

    PEAT_Num check_completions();
#endif
};

#endif // __REQUESTS_H__
