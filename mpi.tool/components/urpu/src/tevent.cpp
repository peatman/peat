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

#include "tevent.h"
#include "communicators.h"
#include "utils.h"

#include <assert.h>
#include <sstream>
#include <stdexcept>

Tractor_Event::Tractor_Event(const PEAT_Rank r, const String_Split &line_trace, const PEAT_Num num_tscs, const PEAT_Num num_hwcs, const PEAT_Num num_addrs, Address_File_Storage *user_region_file) : rank(r), num_children(0), uniq_id(0), level(0)
{
    PEAT_Record_Header hdr; // the header opens the trace-specific line of the record
    user_region_file->parse_record_header(hdr, line_trace, rank);
    num_children = hdr.num_children;
    uniq_id = hdr.ids.at(0);
    level = hdr.level;
    const int b = hdr.trace_payload + num_tscs; // addresses follow the tsc block
    for (int i = 0; i < static_cast<int>(num_addrs); i++) {
        PEAT_UID addr_id;
        extract(addr_id, line_trace.at(b + i));
        addr_key.add(addr_id);
    }
    for (int i = 0; i < static_cast<int>(num_hwcs) * 2; i++) {
        PEAT_HWC_Value hwc_value;
        extract(hwc_value, line_trace.at(b + num_addrs + i));
        hwc_values.push_back(hwc_value);
    }
}
