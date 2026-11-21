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

#include "comm.h"
#include "utils.h"
#include <assert.h>
#include <stdexcept>

Comm::Comm(
		   /*
		  #ifdef PEAT_MPI
			  const std::vector<PEAT_Rank>& wR, const std::vector<PEAT_Rank>& mRs, const MPI_Comm mat_cc, const PEAT_Rank m_r,
		  #endif
		  */
		   int v) : /*
					#ifdef PEAT_MPI
						whole_ranks(wR), my_ranks(mRs), mat_compute_comm(mat_cc), my_rank(m_r),
					#endif
					*/
					verbose(v)
{
	comm_store = NULL;
	ptp = NULL;
	req_store = NULL;
}

void Comm::initialize(PTP *p, Communicators *c, Request_Store *rs, Requests* r, File_Storage_GZ *MPI, File_Storage_GZ *bind)
{
	comm_store = c;
	ptp = p;
	req_store = rs;
	request_func_store = r;
	Outputs::initialize(MPI, bind);
}

void Comm::parse_comm(const PEAT_Rank rank, Tractor_Event &event)
{
    if(event.Comms.request > 0) {
        MAT_Request_Store_Record req;
        MAT_Comm_Record* record = new (MAT_Comm_Record);
        record->pc_level = event.PControl.ctl;
        record->comm = event.Comms.comm;
        record->new_comm = event.Comms.newcomm;
        record->remote_comm = event.Comms.remote_comm;
        record->rank = rank;
        record->request = event.Comms.request;
        record->ph = Phase(event);
        req.request_type = REQUEST_TYPE_USER_COMM;
        req.record = record;
        req_store->add_request(rank, event.Comms.request, &req);
    }
    print_comm_out(event);
}
PEAT_Bool Comm::update_request(const PEAT_Rank rank, PEAT_MPI_Request request, const Phase& phase, PEAT_Bool is_remote)
{
    MAT_Comm_Record* record = NULL;
    MAT_Request_Store_Record ret_it;
    if(request > 0) {
        if(req_store->find_request(rank, request, REQUEST_TYPE_USER_COMM, &ret_it, true)) { //find and delete the request from the store, as it's being completed
            record = static_cast<MAT_Comm_Record*>(ret_it.record);
            request_func_store->print_req_bind(rank, request, record->ph, phase, is_remote);
            delete ret_it.record;
            return true;
        }
    }
    return false;
}

void Comm::print_comm_out(const Tractor_Event &event)
{
#ifdef MAT_PROCESS_MPI_OUTPUT
    output_MPI->printf(event.rank, "%s:%x:%x:%x:%llx", global_func_id.name_by_idx(event.func_id),
                       event.Comms.comm,
                       event.Comms.newcomm,
                       event.Comms.remote_comm,
                       event.Comms.request);
    output_MPI->printf(event.rank, "\n");
    event.write_trace_line(output_MPI);
#endif
}
