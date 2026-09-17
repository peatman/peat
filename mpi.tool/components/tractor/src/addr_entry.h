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

#ifndef __ADDR_ENTRY_H__
#define __ADDR_ENTRY_H__

#ifdef _TRACTOR_VIRT2PHYS_
struct phys_addr {
    unsigned long long phys_entry;
    unsigned long long flags;
    unsigned long long get_pfn(void)
    {
        return (phys_entry & (((unsigned long long)1 << 54) - 1));
    };
    bool is_page_present(void)
    {
        return (phys_entry & (((unsigned long long)1 << 63))) > 0;
    };
    bool is_page_swapped(void)
    {
        return (phys_entry & (((unsigned long long)1 << 62))) > 0;
    };
};
#endif
struct addr_entry
#ifdef _TRACTOR_VIRT2PHYS_
    : phys_addr
#endif
{
    const void *virt_addr;
    unsigned long long size;
};
#endif //__ADDR_ENTRY_H__
