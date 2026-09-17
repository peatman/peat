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

#ifdef _HWCOUNTERS_RDPMC_
#include <iostream>
#include <stdexcept>

#include <cassert>
#include <unistd.h>

#include <asm/unistd.h>
#include <cstring>
#include <linux/limits.h>     //PATH_MAX
#include <linux/perf_event.h> //__NR_perf_event_open

#include <sys/mman.h> //mmap, munmap - map or unmap files or devices into memory

#include "hwcounter.h"

void HW_counter::counter_open(unsigned long long id, PMC_struct *ctx) const
{
    struct perf_event_attr attr;
    memset(&attr, 0, sizeof(perf_event_attr));
    attr.type = id > 10 ? PERF_TYPE_RAW : PERF_TYPE_HARDWARE;
    attr.size = PERF_ATTR_SIZE_VER0; // sizeof(struct perf_event_attr)
    attr.config = id;
    attr.sample_type = PERF_SAMPLE_READ;
    attr.exclude_kernel = 1; // not sure in this

    ctx->fd = syscall(__NR_perf_event_open, &attr, 0, -1, -1, 0);
    if (ctx->fd < 0) {
        throw std::runtime_error("Error opening HW performance event leader");
    }
    ctx->buf = (perf_event_mmap_page *)mmap(NULL, sysconf(_SC_PAGESIZE), PROT_READ, MAP_SHARED, ctx->fd, 0);
    if (ctx->buf == MAP_FAILED) {
        ::close(ctx->fd);
        throw std::runtime_error("Error map file into memory");
    }
}

void HW_counter::counter_close(PMC_struct *ctx) const
{
    munmap(ctx->buf, sysconf(_SC_PAGESIZE));
    ::close(ctx->fd);
}

#else
#include <stdexcept>

#include <cassert>
#include <unistd.h>

#include <asm/unistd.h>
#include <cstring>
#include <linux/perf_event.h>
#include <sys/ioctl.h>

#include "hwcounter.h"

// MSR addreses from "Intel 64 and IA-32 Architectures Software Developers Manual Volume 3B: System Programming Guide, Part 2", Appendix A "PERFORMANCE-MONITORING EVENTS"
// #define INST_RETIRED_ANY_ADDR           (0x309)
// #define CPU_CLK_UNHALTED_THREAD_ADDR    (0x30A)
// #define CPU_CLK_UNHALTED_REF_ADDR       (0x30B)

HW_counter::HW_counter()
{
    for (int i = 0; i < 2; ++i) {
        fd[i] = -1;
    }
    struct perf_event_attr pe;
    memset(&pe, 0, sizeof(struct perf_event_attr));
    pe.type = PERF_TYPE_HARDWARE;
    pe.size = sizeof(struct perf_event_attr);
    pe.config = PERF_COUNT_HW_INSTRUCTIONS;
    pe.disabled = 1;
    pe.exclude_kernel = 1;
    pe.exclude_hv = 1;

    fd[0] = syscall(__NR_perf_event_open, &pe, 0, -1, -1, 0); // man perf_event_open
    if (fd[0] == -1) {
        throw std::runtime_error("Error opening leader PERF_COUNT_HW_INSTRUCTIONS");
    }
    pe.config = PERF_COUNT_HW_CPU_CYCLES;
    fd[1] = syscall(__NR_perf_event_open, &pe, 0, -1, -1, 0); // man perf_event_open
    if (fd[1] == -1) {
        throw std::runtime_error("Error opening leader PERF_COUNT_HW_CPU_CYCLES");
    }
    ioctl(fd[0], PERF_EVENT_IOC_ENABLE, 0);
    ioctl(fd[1], PERF_EVENT_IOC_ENABLE, 0);
}

#endif
