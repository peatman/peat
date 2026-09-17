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

#include "memprofile.h"
#include "t_stat.h"

#include <malloc.h>

#include <sys/resource.h> //getrusage
#include <sys/time.h>     //getrusage

#include <fcntl.h> //open/close/read
#include <sstream>
#include <stdexcept>

// using namespace std;
#ifdef _MEM_HOOKS_
__thread void *(*t_memory_profiler::malloc_orig)(size_t) = 0;           // original function handler. Initialized in Ctor
__thread void *(*t_memory_profiler::calloc_orig)(size_t, size_t) = 0;   // original function handler. Initialized in Ctor
__thread void *(*t_memory_profiler::realloc_orig)(void *, size_t) = 0;  // original function handler. Initialized in Ctor
__thread void *(*t_memory_profiler::memalign_orig)(size_t, size_t) = 0; // original function handler. Initialized in Ctor
__thread void (*t_memory_profiler::free_orig)(void *) = 0;              // original function handler. Initialized in Ctor
#if 1
__thread bool t_memory_profiler::can_collect = false;
//__thread PEAT_Clock t_memory_profiler::total_events = 0;
//__thread long long t_memory_profiler::mem_used_by_tractor = 0;
#else
bool t_memory_profiler::can_collect = false;
#endif
PEAT_Clock t_memory_profiler::total_events = 0;

t_memory_profiler_accumulator t_memory_profiler::acc_before_ctor;
#endif

t_memory_profiler::t_memory_profiler() : statm_file(0)
{
    system_page_size = (PEAT_Addr)sysconf(_SC_PAGESIZE);
#ifdef _MEM_HOOKS_
    highheap = (void *)0;
    lowheap = (void *)0x7FFFFFFFFFFFFFFF;
    highstack = (void *)0;
    lowstack = (void *)0x7FFFFFFFFFFFFFFF;
    max_water_level = 0;

    if (!malloc_orig) {
        malloc_orig = reinterpret_cast<void *(*)(size_t)>(dlsym(RTLD_NEXT, MEMPROF_MALLOC_NAME));
    }
    if (!calloc_orig) {
        calloc_orig = reinterpret_cast<void *(*)(size_t, size_t)>(dlsym(RTLD_NEXT, MEMPROF_CALLOC_NAME));
    }
    if (!realloc_orig) {
        realloc_orig = reinterpret_cast<void *(*)(void *, size_t)>(dlsym(RTLD_NEXT, MEMPROF_REALLOC_NAME));
    }
    if (!memalign_orig) {
        memalign_orig = reinterpret_cast<void *(*)(size_t, size_t)>(dlsym(RTLD_NEXT, MEMPROF_MEMALIGN_NAME));
    }
    if (!free_orig) {
        free_orig = reinterpret_cast<void (*)(void *)>(dlsym(RTLD_NEXT, MEMPROF_FREE_NAME));
    }
#endif
    // can_collect = true;  controlled by Collector class
#ifdef _MEMPROFILE_STORE_
    statm_file = open("/proc/self/statm", O_RDONLY);
    // statm_file = open("/proc/self/status", O_RDONLY);
    if (-1 == statm_file) { // /proc/self/status
        throw std::runtime_error("Error opening statm file in /proc FS");
    }
#endif
}

t_memory_profiler::~t_memory_profiler()
{
#ifdef _MEM_HOOKS_
    can_collect = false; // disable collection first
    malloc_orig = 0;
    calloc_orig = 0;
    realloc_orig = 0;
    memalign_orig = 0;
    free_orig = 0;
#endif //_MEM_HOOKS_
#ifdef _MEMPROFILE_STORE_
    close(statm_file);
#endif
}
#ifdef _MEM_HOOKS_

void t_memory_profiler::initialize_object_addresses(const std::map<PEAT_Num, shared_object_info> &obj)
{
    for (std::map<PEAT_Num, shared_object_info>::const_iterator it = obj.begin(); it != obj.end(); ++it) {
        PEAT_Addr hi = 0;
        PEAT_Addr low = (PEAT_Addr)LLONG_MAX;
        const shared_object_info &info = it->second;
        for (std::list<shared_object_header>::const_iterator header_it = info.dlpi_phdr.begin(); header_it != info.dlpi_phdr.end(); ++header_it) {
            if (0 == header_it->load_address || 0 == header_it->end_address) { // ELF sections can have 0 addresses (like STACK)
                continue;
            }
            hi = (PEAT_Addr)std::max((long)hi, (long)header_it->end_address);
            low = (PEAT_Addr)std::min((long)low, (long)header_it->load_address);
        }
        if (0 != hi && (PEAT_Addr)LLONG_MAX != low) {

            t_memory_profiler_object_addresses &new_object = obj_id[hi];
            assert(-1 == new_object.id);
            new_object.id = it->first;
            new_object.low = low;
            new_object.high = hi;
            new_object.obj_name = info.dlpi_name;
            debug_printf("New object: id %d,low %llx, high %llx,name %s\n", new_object.id, new_object.low, new_object.high, new_object.obj_name.c_str());
        }
    }
}

PEAT_Signed_Num t_memory_profiler::get_obj_id(const PEAT_Addr addr) const
{
    PEAT_Signed_Num result = -1;
    if (!obj_id.empty()) {
        std::map<PEAT_Addr, t_memory_profiler_object_addresses>::const_iterator it = obj_id.upper_bound(addr);
        if (obj_id.end() != it) {
            // printf("ADDR %llx FOUND ID %ld\n", addr, it->second.id);
            if (it->second.low <= addr && it->second.high >= addr) {
                result = it->second.id;
            }
        }
    }
    if (result == -1) {
        if (addr >= lowheap && addr <= highheap)
            result = -2;
        if (addr >= lowstack && addr <= highstack)
            result = -3;
    }
    return result;
}

std::string t_memory_profiler::get_obj_name(const PEAT_Signed_Num id) const
{
    std::string result("[Unknown]");
    if (id >= 0) {
        for (std::map<PEAT_Addr, t_memory_profiler_object_addresses>::const_iterator it = obj_id.begin(); it != obj_id.end(); ++it) {
            const t_memory_profiler_object_addresses &obj_addr = it->second;
            if (id == obj_addr.id) {
                const size_t pos = obj_addr.obj_name.find_last_of("/");
                if (pos == std::string::npos) {
                    result = obj_addr.obj_name;
                } else {
                    result = obj_addr.obj_name.substr(pos + 1);
                }
                break;
            }
        }
    } else {
        if (id == -2)
            result = "[heap]";
        if (id == -3)
            result = "[stack]";
    }
    return result;
}

PEAT_Clock t_memory_profiler::fill_common_statistic(const std::string &name, const std::string &mode)
{
    PEAT_Clock trace_write_time = rdtsca();
    if (events.empty()) {
        return rdtsca() - trace_write_time;
    }
    std::map<PEAT_Addr, PEAT_Num> address_cache;
    PEAT_Clock memory_leak_count = 0;
    gzFile comm_file = gzopen(name.c_str(), mode.c_str());
    if (comm_file == NULL) {
        std::ostringstream err_txt;
        err_txt << "Can't open file " << name;
        throw std::runtime_error(err_txt.str());
    }
    PEAT_Clock total_size = 0;
    PEAT_Clock time_shift = events.begin()->time;
    for (std::list<t_memory_profiler_event>::iterator it = events.begin(); it != events.end(); ++it) {
        PEAT_Signed_Num id = get_obj_id(it->addr_obj);
        t_memory_profiler_common_stat &stat = mp_common_stat[id];
        stat.count += 1;
        PEAT_Clock cur_size = 0;
        if (mpe_malloc == it->type || mpe_calloc == it->type || mpe_memalign == it->type) {
            address_cache[it->addr_ptr_new] = it->size;
            cur_size = it->size;
        }
        if (mpe_free == it->type) {
            if (it->addr_ptr_free) { // free(0) do nothing
                std::map<PEAT_Addr, PEAT_Num>::iterator free_it = address_cache.find(it->addr_ptr_free);
                if (address_cache.end() != free_it) {
                    cur_size = -(free_it->second);
                    address_cache.erase(free_it);
                } else {
                    ++memory_leak_count;
                }
            }
        }
        if (mpe_realloc == it->type) {
            if (it->addr_ptr_free) { // realloc(0, 123) look like malloc(123)
                std::map<PEAT_Addr, PEAT_Num>::iterator free_it = address_cache.find(it->addr_ptr_free);
                if (address_cache.end() != free_it) {
                    cur_size = -(free_it->second);
                    address_cache.erase(free_it);
                } else {
                    ++memory_leak_count;
                }
            }
            address_cache[it->addr_ptr_new] = it->size;
            cur_size += it->size;
        }
        stat.current_size += cur_size; // add to object_id statistic
        stat.max_size = std::max(stat.max_size, stat.current_size);
        total_size += cur_size; // add to waterlevel field
        max_water_level = std::max(max_water_level, total_size);
        gzprintf(comm_file, "%lld:%d:%d:%llx:%llx:%ld:%lld\n", it->time - time_shift, it->type, id, it->addr_ptr_new, it->addr_ptr_free, cur_size, total_size);
    }
    gzprintf(comm_file, "Unknown pointers %lld Memory leaks %ld\n", memory_leak_count, address_cache.size());
    for (std::map<PEAT_Addr, PEAT_Num>::iterator it = address_cache.begin(); it != address_cache.end(); ++it) {
        gzprintf(comm_file, "Pointer: %llx. Bytes: %lld. Module: %d - %s.\n", it->first, it->second, get_obj_id(it->first), get_obj_name(get_obj_id(it->first)).c_str());
    }
    gzclose(comm_file);
    return rdtsca() - trace_write_time;
}
#endif
void t_memory_profiler::get_mem_status(t_memory_event *evt)
{

#if 1
    const long page_size_KB = (long)system_page_size / 1024; // to convert numbers into ki_b
    //    int statm_file = open("/proc/self/statm", O_RDONLY);
    char stat_buffer[64];
    lseek(statm_file, 0, SEEK_SET);
    read(statm_file, stat_buffer, 64);
    sscanf(stat_buffer, "%ld %ld", &evt->vm_size, &evt->vm_RSS);
    evt->vm_size *= page_size_KB;
    evt->vm_RSS *= page_size_KB;
//    close(statm_file);
#else
    // values are in kB but I suspect these are in ki_b
    char stat_buffer[1024];
    char *stat_buffer_ptr = stat_buffer;
    lseek(statm_file, 0, SEEK_SET);
    read(statm_file, stat_buffer, 1024);
    stat_buffer[1024 - 1] = 0x0;
    // sscanf(stat_buffer, "vm_peak: %ld %ld", &vm_size, &vm_RSS);

    const int vm_size_len = sizeof("vm_size:") - 1; // strlen
    const int vm_RSS_len = sizeof("vm_RSS:") - 1;   // strlen
    while (*stat_buffer_ptr) {
        if ('V' == *stat_buffer_ptr && 'm' == *(stat_buffer_ptr + 1)) {
            if (0 == strncmp(stat_buffer_ptr, "vm_size:", vm_size_len)) {
                vm_size = strtol(stat_buffer_ptr + vm_size_len, &stat_buffer_ptr, 10);
                continue;
            }
            if (0 == strncmp(stat_buffer_ptr, "vm_RSS:", vm_RSS_len)) {
                vm_RSS = strtol(stat_buffer_ptr + vm_RSS_len, &stat_buffer_ptr, 10);
                break; // continue;
            }
        }
        ++stat_buffer_ptr;
    }
#endif
    evt->ru_maxrss = get_max_memory_RSS_in_KB(); // the value is in kilobytes
    evt->mem_used_by_tractor = bytes_allocated / 1024;
}

PEAT_Num t_memory_profiler::get_max_memory_RSS_in_KB() const
{
    PEAT_Num result = 0;
    struct rusage usage;
    if (0 == getrusage(RUSAGE_SELF, &usage)) {
        result = usage.ru_maxrss; // the value is in kilobytes
    }
    return result;
}
#ifdef _MEM_HOOKS_

#define PRINT_PARAMETERS(width, string) int(width + strlen(string)) / 2, string, int(width - strlen(string)) / 2, ""
#define SEPARATOR(width)                       \
    gzprintf(file, "%*d:", 2, PEAT_RT_Remark); \
    for (int i = 0; i < width; ++i) {          \
        gzprintf(file, "%c", '-');             \
    }                                          \
    gzprintf(file, "\n")

void t_memory_profiler::print_memory_profile_report(gzFile file, const PEAT_Record_Type line_prefix1, const PEAT_Record_Type line_prefix2) const
{
    // for(map<PEAT_Addr, t_memory_profiler_object_addresses>::const_iterator it = obj_id.begin(); it != obj_id.end(); ++it) {
    //     gzprintf(file, "KEY %lx ID %ld LOW %llx HIGH %llx\n", it->first, it->second.id, it->second.low, it->second.high);
    // }
    if (mp_common_stat.empty()) {
        return;
    }
    const int name_id_len = 4;
    const int field_len = 18;
    const int separator_length = name_id_len + 1 + 3 * field_len + 2;
    SEPARATOR(separator_length);
    gzprintf(file, "%*d", 2, PEAT_RT_Remark);
    gzprintf(file, ":Before init count: %ld: Size: %lld bytes", acc_before_ctor.count, acc_before_ctor.size);
    gzprintf(file, "\n");
    SEPARATOR(separator_length);
    gzprintf(file, "%*d", 2, PEAT_RT_Remark);
    gzprintf(file, ":%*s%*s", PRINT_PARAMETERS(name_id_len, "ID"));
    gzprintf(file, ":%*s%*s", PRINT_PARAMETERS(field_len, "Count "));
    gzprintf(file, ":%*s%*s", PRINT_PARAMETERS(field_len, "Size"));
    gzprintf(file, ":%*s%*s", PRINT_PARAMETERS(field_len, "Object name"));
    gzprintf(file, "\n");
    SEPARATOR(separator_length);
    PEAT_Num total_count = 0;
    // PEAT_MPI_Message_Size total_size = 0;
    for (std::map<PEAT_Signed_Num, t_memory_profiler_common_stat>::const_iterator it = mp_common_stat.begin(); it != mp_common_stat.end(); ++it) {
        total_count += it->second.count;
        // total_size += it->second.max_size;
        gzprintf(file, "%d", line_prefix1);
        gzprintf(file, ":%*d", name_id_len, it->first);
        gzprintf(file, ":%*ld", field_len, it->second.count);
        gzprintf(file, ":%*lld", field_len, it->second.max_size);
        gzprintf(file, ":%*s", field_len, get_obj_name(it->first).c_str());
        gzprintf(file, "\n");
    }
    SEPARATOR(separator_length);
    gzprintf(file, "%d", line_prefix2);
    gzprintf(file, ":%*ld", name_id_len + 1 + field_len, total_count);
    gzprintf(file, ":%*lld", field_len, max_water_level);
    gzprintf(file, "\n");
    SEPARATOR(separator_length);
}
#undef SEPARATOR
#undef PRINT_PARAMETERS
#endif