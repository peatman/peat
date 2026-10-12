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

#include "osc.h"
#include "utils.h"
#include <assert.h>
#include <stdexcept>

MAT_OSC_Record::MAT_OSC_Record(const Tractor_Event& s) : MAT_Record() {
    comm = s.one_sided_comm.comm;
    request = s.one_sided_comm.request;
    ph = s;
}

OSC::OSC(int v) : verbose(v)
{
	comm_store = NULL;
	ptp = NULL;
	req_store = NULL;
	request_func_store = NULL;
}

void OSC::initialize(PTP *p, Communicators *c, Request_Store *rs, Requests* r, File_Storage_GZ *MPI, File_Storage_GZ *HWC, File_Storage_GZ *bind)
{
	comm_store = c;
	ptp = p;
	req_store = rs;
	request_func_store = r;
	Outputs::initialize(MPI, HWC, bind);
}

void OSC::parse_osc(const PEAT_Rank rank, Tractor_Event &event)
{
    if(event.one_sided_comm.request > 0) {
        MAT_Request_Store_Record req;
        MAT_OSC_Record* record = new MAT_OSC_Record(event);
        req.request_type = REQUEST_TYPE_USER_OSC;
        req.record = record;
        req_store->add_request(rank, event.one_sided_comm.request, &req);
    }
    print_osc_out(event);
}
PEAT_Bool OSC::update_request(const PEAT_Rank rank, PEAT_MPI_Request request, const Phase& phase, PEAT_Bool is_remote)
{
    MAT_OSC_Record* record = NULL;
    MAT_Request_Store_Record ret_it;
    if(request > 0) {
        if(req_store->find_request(rank, request, REQUEST_TYPE_USER_OSC, &ret_it, true)) { //find and delete the request from the store, as it's being completed
            record = static_cast<MAT_OSC_Record*>(ret_it.record);
            request_func_store->print_req_bind(rank, request, record->ph, phase, is_remote);
            delete ret_it.record;
            return true;
        }
    }
    return false;
}

void OSC::print_osc_out(const Tractor_Event &event)
{
#ifdef MAT_PROCESS_HWC_MPI_OUTPUT
    output_MPI->printf(event.rank, "%s:%s:%x:%x:%x:%llx", event.header_str().c_str(), global_func_id.name_by_idx(event.func_id),
                       event.Comms.comm,
                       event.Comms.newcomm,
                       event.Comms.remote_comm,
                       event.Comms.request);
    output_MPI->printf(event.rank, "\n");
    output_HWC->printf(event.rank, "%s:%lld:%lld:%lld:%lld", event.hwc_header_str().c_str(),
                       event.tsc_time.mpi.in,
                       event.tsc_time.mpi.out,
                       event.tsc_time.pe.in,
                       event.tsc_time.pe.out);
    for (PEAT_Num i = 0; i < event.addrs.size(); i++) {
        output_HWC->printf(event.rank, ":%lld", event.addrs.at(i));
    }
    output_HWC->printf(event.rank, "\n");
#endif
}
