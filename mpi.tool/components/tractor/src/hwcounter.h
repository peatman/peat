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

#ifndef __HWCOUNTER_H__
#define __HWCOUNTER_H__

#include "common_plain_types.h"
#include "t_defines.h"
#include <cassert>
#include <list>
#include <map>
#include <string>
#include <zlib.h>

#ifdef _HWCOUNTERS_RDPMC_
#include <linux/perf_event.h>

struct HW_counter {
    HW_counter()
    {
        counter_open(PERF_COUNT_HW_INSTRUCTIONS, &counter[0]);
        counter_open(PERF_COUNT_HW_CPU_CYCLES, &counter[1]);
    }
    ~HW_counter()
    {
        counter_close(&counter[0]);
        counter_close(&counter[1]);
    }
    struct PMC_struct {
        PMC_struct() : fd(-1), buf(0) {}
        int fd;
        struct perf_event_mmap_page *buf;
    };
    PMC_struct counter[2];

    void counter_open(unsigned long long id, PMC_struct *ctx) const;
    void counter_close(PMC_struct *ctx) const;

    // #define rmb() asm volatile("" ::: "memory") //two additional mov around
    unsigned long long get_counter(const int id) const
    {
        unsigned long long val;
        //        unsigned seq;
        unsigned long long offset;
        perf_event_mmap_page *buf = counter[id].buf;

//        do {
//            seq = buf->lock;
//            rmb();
#if 1
        val = rdpmc_intrin(buf->index - 1);
#else
        val = __builtin_ia32_rdpmc(buf->index - 1);
#endif
        offset = buf->offset;
        //            rmb();
        //        } while (buf->lock != seq);
        return val + offset;
    }
    int get_num_counters() { return (2); }
    void get_all_counters(long long *store)
    {
        store[0] = get_counter(0);
        store[1] = get_counter(1);
    }
};
#else

#include <linux/perf_event.h>
#include <sys/ioctl.h>
#include <unistd.h>

class HW_counter
{
    int fd[2];

  public:
    HW_counter();
    virtual ~HW_counter()
    {
        for (int i = 0; i < 2; ++i) {
            if (fd[0] != -1) {
                close(fd[0]);
            }
        }
    }
    long long get_counter(const int num) const
    {
        long long count = 0;
        // ioctl(fd[num], PERF_EVENT_IOC_ENABLE, 0);
        long err = read(fd[num], &count, sizeof(long long));
        if (err != -1) {
            return count;
        } else {
            return 0;
        }
    }
    int get_num_counters() { return (2); }
    void get_all_counters(long long *store)
    {
        store[0] = get_counter(0);
        store[1] = get_counter(1);
    }
    //    int reset(const int num) const {
    //        ioctl(fd[num], PERF_EVENT_IOC_RESET, 0);
    //        return ioctl(fd[num], PERF_EVENT_IOC_ENABLE, 0);
    //    }
};

#endif

#endif // __HWCOUNTER_H__
