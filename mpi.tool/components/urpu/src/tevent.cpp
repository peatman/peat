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

Tractor_Event::Tractor_Event(const PEAT_Rank r, const String_Split &line_hwc, const PEAT_Num num_tscs, const PEAT_Num num_hwcs, const PEAT_Num num_addrs, Address_File_Storage *user_region_file) : rank(r)
{
    // no need to check line.size() due line.at() throw an exception
    //  First read fixed part
    extract(uniq_id, line_hwc.at(0));
    for (int i = 0; i < (int)num_addrs; i++) {
        PEAT_UID addr_id;
        extract(addr_id, line_hwc.at(2 + num_tscs + i));
        User_Address_Info *info = user_region_file->get_addr_by_id(rank, addr_id);
        addr_key.add(addr_id);
    }
    for (int i = 0; i < (int)num_hwcs * 2; i++) {
        PEAT_HWC_Value hwc_value;
        extract(hwc_value, line_hwc.at(2 + num_tscs + num_addrs + i));
        hwc_values.push_back(hwc_value);
    }
}
