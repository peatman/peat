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

#include "mpi.h"
#include <assert.h>
#include <dlfcn.h>
#include <libgen.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>
// #include "funcprop.h"
#include "t_stat.h"
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>

#include "tractor.h"

#if defined(__CPLUSPLUS__) || defined(__cplusplus)
extern "C" {
#endif

inline std::string get_self_path()
{
    //__DEBUG_PROLOG_TRACTOR_LAYER__
    char buff[MAX_PATH];
    ssize_t len = ::readlink("/proc/self/exe", buff, sizeof(buff) - 1);
    if (len != -1) {
        buff[len] = 0x0;
        //__DEBUG_EPILOG_TRACTOR_LAYER__
        return std::string(buff);
    }
    //__DEBUG_EPILOG_TRACTOR_LAYER__
    return "unknown";
}

static int callback(dl_phdr_info *info, size_t size, PEAT_Addr data)
{
    //    assert(size >= 64);//size of the dl_phdr_info structure
    //__DEBUG_PROLOG_TRACTOR_LAYER__
    shared_object_info so_info;
    so_info.dlpi_addr = info->dlpi_addr;
    so_info.dlpi_phnum = info->dlpi_phnum;
    so_info.dlpi_name = info->dlpi_name;
    if (so_info.dlpi_name.empty()) {
        so_info.dlpi_name = get_self_path();
    }
    std::list<shared_object_info> *shared_objects = reinterpret_cast<std::list<shared_object_info> *>(data);
    for (int j = 0; j < info->dlpi_phnum; j++) {
        shared_object_header shared_object_section;
        shared_object_section.base_address = reinterpret_cast<PEAT_Addr>(info->dlpi_addr);
        shared_object_section.load_address = reinterpret_cast<PEAT_Addr>(info->dlpi_addr + info->dlpi_phdr[j].p_vaddr);
        shared_object_section.end_address = reinterpret_cast<PEAT_Addr>(reinterpret_cast<unsigned long>(shared_object_section.load_address) + info->dlpi_phdr[j].p_memsz);
        shared_object_section.p_type = info->dlpi_phdr[j].p_type;
        shared_object_section.p_flags = info->dlpi_phdr[j].p_flags;
        shared_object_section.p_offset = info->dlpi_phdr[j].p_offset;
        shared_object_section.p_vaddr = reinterpret_cast<PEAT_Addr>(info->dlpi_phdr[j].p_vaddr);
        shared_object_section.p_paddr = reinterpret_cast<PEAT_Addr>(info->dlpi_phdr[j].p_paddr);
        shared_object_section.p_filesz = info->dlpi_phdr[j].p_filesz;
        shared_object_section.p_memsz = info->dlpi_phdr[j].p_memsz;
        shared_object_section.p_align = info->dlpi_phdr[j].p_align;
        so_info.dlpi_phdr.push_back(shared_object_section);
    }
    shared_objects->push_back(so_info);
    //__DEBUG_EPILOG_TRACTOR_LAYER__
    return 0;
}
void so_walker(std::map<PEAT_Num, shared_object_info> &shared_objects /*, PEAT_Addr &low_addr, PEAT_Addr &high_addr,const bool get_main_executable_addresses_only*/)
{
    //__DEBUG_PROLOG_TRACTOR_LAYER__
    std::list<shared_object_info> shared_objects_list;
    dl_iterate_phdr(callback, &shared_objects_list);
    PEAT_Num index = 0;
    for (std::list<shared_object_info>::iterator it = shared_objects_list.begin(); it != shared_objects_list.end(); ++it) {

        debug_printf("Rank: %d, binary %s:", t_mpi_rank, it->dlpi_name.c_str());
        //        if(0 == index)
        { // main executable
            for (std::list<shared_object_header>::iterator header_it = it->dlpi_phdr.begin(); header_it != it->dlpi_phdr.end(); ++header_it) {
                shared_object_header &elf_sec = *header_it;
                if (shared_object_header::is_exec_and_load(elf_sec.p_flags, elf_sec.p_type)) {
                    debug_printf("base: %llx, low: %llx, high: %llx ", elf_sec.base_address, elf_sec.load_address, elf_sec.end_address);
                    break;
                }
            }
        }
        debug_printf("\n");
        shared_objects[index] = *it;
        ++index;
    }
    //__DEBUG_EPILOG_TRACTOR_LAYER__
}

bool shared_object_header::is_exec_and_load(const PEAT_ULNum flg, const PEAT_ULNum t)
{
    //__DEBUG_PROLOG_TRACTOR_LAYER__
    if ((PF_X & flg) && (PT_LOAD == t)) {
        //__DEBUG_EPILOG_TRACTOR_LAYER__
        return true;
    } else {
        //__DEBUG_EPILOG_TRACTOR_LAYER__
        return false;
    }
}

std::string shared_object_header::print_flags(const PEAT_ULNum flg)
{
    //__DEBUG_PROLOG_TRACTOR_LAYER__
    std::string result;
    if (flg & PF_MASKOS) {
        result += "O";
    } else {
        result += "-";
    }
    if (flg & PF_MASKPROC) {
        result += "C";
    } else {
        result += "-";
    }
    if ((flg & PF_R)) {
        result += "r";
    } else {
        result += "-";
    }
    if ((flg & PF_W)) {
        result += "w";
    } else {
        result += "-";
    }
    if ((flg & PF_X)) {
        result += "x";
    } else {
        result += "-";
    }
    //__DEBUG_EPILOG_TRACTOR_LAYER__
    return result;
}

std::string shared_object_header::print_type(const PEAT_ULNum t)
{
    //__DEBUG_PROLOG_TRACTOR_LAYER__
    std::string result;
    switch (t) {
    case PT_NULL:
        result = "NULL";
        break;
    case PT_LOAD:
        result = "LOAD";
        break;
    case PT_DYNAMIC:
        result = "DYNAMIC";
        break;
    case PT_INTERP:
        result = "INTERP";
        break;
    case PT_NOTE:
        result = "NOTE";
        break;
    case PT_SHLIB:
        result = "SHLIB";
        break;
    case PT_PHDR:
        result = "PHDR";
        break;
    case PT_TLS:
        result = "TLS";
        break;
    case PT_NUM:
        result = "NUM";
        break;
    case PT_LOOS:
        result = "LOOS";
        break;
    case PT_GNU_EH_FRAME:
        result = "GNU_EH_FRAME";
        break;
    case PT_GNU_STACK:
        result = "GNU_STACK";
        break;
    case PT_GNU_RELRO:
        result = "GNU_RELRO";
        break;
    case PT_SUNWBSS:
        result = "SUNWBSS";
        break;
    case PT_SUNWSTACK:
        result = "SUNWSTACK";
        break;
    case PT_HIOS:
        result = "HIOS";
        break;
    case PT_LOPROC:
        result = "LOPROC";
        break;
    case PT_HIPROC:
        result = "HIPROC";
        break;
    default:
        result = "UNKNOWN";
    }
    //__DEBUG_EPILOG_TRACTOR_LAYER__
    return result;
}

#if defined(__CPLUSPLUS__) || defined(__cplusplus)
}
#endif
