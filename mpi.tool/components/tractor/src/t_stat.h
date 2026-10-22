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

#ifndef __T_STAT_H__
#define __T_STAT_H__
#include "common_plain_types.h"
#include "mpi.h"
#include "mpi_func_ids.h"
#include "mpi_func_ids_through.h"
#include "t_alloc.h"
#include "t_defines.h"
#include "zlib.h"
#include <dlfcn.h>
#include <link.h>
#include <list>
#include <malloc.h>
#include <map>
#include <string>
#if defined(__CPLUSPLUS__) || defined(__cplusplus)
extern "C" {
#endif

struct t_node_info {
    char hostname[MAX_HOSTNAME];
    PEAT_Clock t_time[3];
    PEAT_Clock init_delay;
    PEAT_Clock proc_freq[2];
    PEAT_Clock cpu_proc_freq[2];
    int mpi_rank_num;
    int *mpi_ranks;
    t_node_info() : mpi_rank_num(0), mpi_ranks(NULL), init_delay(0) {}
    ~t_node_info()
    {
        if (mpi_ranks)
            t_free(mpi_ranks, mpi_rank_num * sizeof(int));
        mpi_ranks = NULL;
    }
    void allocate(int num)
    {
        mpi_ranks = (int *)t_malloc(num * sizeof(int), 128);
        mpi_rank_num = num;
    }
    void set_hostname(const char *name) { strcpy(hostname, name); }
};
struct t_nodes_info {
    int nodes_num;
    t_node_info *nodes;
    t_nodes_info() : nodes_num(0), nodes(NULL) {}
    void allocate(int num)
    {
        nodes = new t_node_info[num];
        nodes_num = num;
    }
    ~t_nodes_info()
    {
        if (nodes)
            delete[] nodes;
    }
};

struct shared_object_header {
    shared_object_header() : base_address(0), load_address(0), end_address(0), p_type(0), p_flags(0), p_offset(0), p_vaddr(0), p_paddr(0), p_filesz(0), p_memsz(0), p_align(0) {}
    PEAT_Addr base_address; // base address in virtual memory
    PEAT_Addr load_address; // start address of the header in virtual memory
    PEAT_Addr end_address;  // end address of the header in virtual memory
    PEAT_ULNum p_type;      // Segment type
    PEAT_ULNum p_flags;     // Segment flags
    PEAT_ULNum p_offset;    // Segment file offset
    PEAT_Addr p_vaddr;      // Segment virtual address
    PEAT_Addr p_paddr;      // Segment physical address
    PEAT_ULNum p_filesz;    // Segment size in file
    PEAT_ULNum p_memsz;     // Segment size in memory
    PEAT_ULNum p_align;     // Segment alignment
    static std::string print_flags(const PEAT_ULNum flg);
    static std::string print_type(const PEAT_ULNum t);
    static bool is_exec_and_load(const PEAT_ULNum flg, const PEAT_ULNum t);
};

struct shared_object_info {
    shared_object_info() : dlpi_addr(0), dlpi_phnum(0) {}
    unsigned long long dlpi_addr;              // Base object address
    std::string dlpi_name;                     // Object name
    std::list<shared_object_header> dlpi_phdr; // Elf program headers
    unsigned int dlpi_phnum;                   // Number of elf program headers
};

void so_walker(std::map<PEAT_Num, shared_object_info> &shared_objects /*, PEAT_Addr &low_addr, PEAT_Addr &high_addr,const bool get_main_executable_addresses_only=false*/);

#if defined(__CPLUSPLUS__) || defined(__cplusplus)
}
#endif

#endif