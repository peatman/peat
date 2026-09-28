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

#include <algorithm>
#include <assert.h>
#include <errno.h>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits.h>
#include <numeric> //std::accumulate
#include <set>
#include <sstream>
#include <stdexcept>
#include <stdlib.h>

#include "pet_core.h"
#include "stat.h"
#include "utils.h"

#ifdef PEAT_MPI
#include <mpi.h>
#endif

#define P2PCOUT          \
    if (globals.verbose) \
    cout
#define P2PDBG(x)             \
    if (globals.verbose >= x) \
    cerr
#define P2PDELIM '|'
#define SEC_OUTPUT(x) fixed << setprecision(0) << x << " ( " << setprecision(2) << x / freq << " sec)"

PEAT_Bool PET_Local_Buf_Event::read_event(PEAT_Rank r_rank, const String_Split &mpi_string, const String_Split &hwc_string, PET_Event *host, const PEAT_Record_Header &hdr)
{
    PET_Local_Event::read_event(r_rank, mpi_string, hwc_string, host, hdr);
    extract(inbuf_size, mpi_string.at(hdr.payload + 1));
    extract(outbuf_size, mpi_string.at(hdr.payload + 2));
    return true;
}

PET_Event *PET_Local_Buf_Event::write_event(File_Storage_GZ *gz_MPI, File_Storage_GZ *gz_HWC, File_Storage_GZ *gz_bind, const char *mpi_eol, const char *hwc_eol)
{
    PET_Local_Event::write_event(gz_MPI, gz_HWC, gz_bind, "", hwc_eol);
    gz_MPI->printf(rank, ":%d:%d%s", inbuf_size, outbuf_size, mpi_eol);
    return this;
}
