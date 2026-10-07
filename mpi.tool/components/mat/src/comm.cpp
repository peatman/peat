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

Comm::Comm(PTP *p, Communicators *c,
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
    com_store = c;
    ptp = p;
}

Comm::~Comm()
{
    ptp = 0;
    com_store = 0;
}

void Comm::parse_comm(const PEAT_Rank rank, Tractor_Event &event)
{
    print_comm_out(event);
}

void Comm::print_comm_out(const Tractor_Event &event)
{
#ifdef MAT_PROCESS_HWC_MPI_OUTPUT
    output_MPI->printf(event.rank, "%s:%s:%x:%x:%x:%x:%llx", event.header_str().c_str(), global_func_id.name_by_idx(event.func_id),
                       event.Comms.comm,
                       event.Comms.newcomm,
                       event.Comms.remote_comm,
                       event.Comms.inter_comm,
                       event.Comms.request);
    if (!event.nested)
        output_MPI->printf(event.rank, ":%d", event.num_children); // legacy num_colts
    output_MPI->printf(event.rank, "\n");
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
#endif
}
