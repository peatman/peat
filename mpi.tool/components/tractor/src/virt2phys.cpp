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

#include "addr_entry.h"
#include "stdio.h"
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>

#define PGM_SIZE (sizeof(unsigned long long))
#ifdef _TRACTOR_VIRT2PHYS_
static int pagesize = 4096;
int __msl_open_pfile(char *ppath)
{
    int pfile = 0;
    pagesize = getpagesize();
    pfile = open(ppath, O_LARGEFILE | O_RDONLY);
    return (pfile);
}
void __msl_close_pfile(int pfile)
{
    assert(pfile);
    close(pfile);
    return;
}

bool __msl_virt2phys(int pmfile, int pffile, addr_entry *entry)
{
    off64_t offset;
    ssize_t status;
    unsigned long long virt_addr = (unsigned long long)entry->virt_addr;
    assert(pmfile);
    assert(pffile);
    assert(entry);
    offset = virt_addr / pagesize * PGM_SIZE;
    status = lseek64(pmfile, offset, SEEK_SET);
    status = read(pmfile, &entry->phys_entry, PGM_SIZE);
    if (entry->is_page_present() && pffile > 0) {
        off64_t pfn = entry->get_pfn();
        offset = pfn * PGM_SIZE;
        status = lseek64(pffile, offset, SEEK_SET);
        status = read(pffile, &entry->flags, PGM_SIZE);
    }
    return true;
}
#endif
