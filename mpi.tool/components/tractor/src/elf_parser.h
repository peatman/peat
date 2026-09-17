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

#ifndef __ELF_PARSER_H__
#define __ELF_PARSER_H__

#include <zlib.h>

#include <list>
#include <map>
#include <set>
#include <string>

#include "common_plain_types.h"
#include "t_stat.h"

#if defined(__CPLUSPLUS__) || defined(__cplusplus)
extern "C" {
#endif
extern int mpi_rank;
struct elf_symbols {
    elf_symbols();
    void find_symbol(const PEAT_Addr address, const int obj_id, const PEAT_Addr obj_load_base, const std::string &object_path,
                     std::string &func_name, std::string &source_name, long &line_number)
    {
        __DEBUG_PROLOG_TRACTOR_LAYER__
        if (!address || object_path.empty() || 0 > obj_id) {
            __DEBUG_EPILOG_TRACTOR_LAYER__
            return;
        }
        object_info &object = objects[obj_id]; // find corresponded library by object_id from collector
        if (object.obj_path.empty()) {
            // initialize symbol table information
            object.obj_path = object_path;
            object.obj_addr_base = obj_load_base;
            initialize(object);
        }
        func_name = "??";
        for (std::map<PEAT_Addr, symbol_info>::const_iterator it = object.obj_symbols.begin(); it != object.obj_symbols.end(); ++it) {
            const symbol_info &sym = it->second;
            // debug_printf("Address %llx, symbol:%s %llx-%llx\n",address,sym.symbol_name.c_str(),sym.addr_begin,sym.addr_end);
            if (sym.addr_begin <= address && sym.addr_end > address) {
                func_name = sym.symbol_name;
                break;
            }
        }
        source_name = "??";
        line_number = 0;
        // provide next source line for the address. TODO need to provide previous one if source the same

        std::map<PEAT_Addr, DWARF_lines_addresses>::const_iterator src_ptr = object.obj_sources.lower_bound(address); // return last if nothing found
        if (src_ptr != object.obj_sources.end()) {
            if (src_ptr != object.obj_sources.begin()) {
                --src_ptr;
            }
        }
        if (object.obj_sources.end() != src_ptr) {
            const DWARF_lines_addresses &src_store = src_ptr->second;
            line_number = src_store.source_line;
            source_name = *src_store.source_file; // TODO dangerous operation. Pointer might be better. if(0 != ptr) {
        }
        __DEBUG_EPILOG_TRACTOR_LAYER__
    }
    std::string find_function_symbol(const PEAT_Addr address) const
    {
        __DEBUG_PROLOG_TRACTOR_LAYER__
        for (std::map<int, object_info>::const_iterator obj_it = objects.begin(); obj_it != objects.end(); ++obj_it) {
            const object_info &obj_info = obj_it->second;
            for (std::map<PEAT_Addr, symbol_info>::const_iterator it = obj_info.obj_symbols.begin(); it != obj_info.obj_symbols.end(); ++it) {
                const symbol_info &sym = it->second;
                if (sym.addr_begin <= address && sym.addr_end > address) {
                    __DEBUG_EPILOG_TRACTOR_LAYER__
                    return sym.symbol_name;
                }
            }
        }
        __DEBUG_EPILOG_TRACTOR_LAYER__
        return "<unknown>";
    }
    void find_source_and_line(const PEAT_Addr address, std::string &source_name, int &line_number) const
    {
        __DEBUG_PROLOG_TRACTOR_LAYER__
        for (std::map<int, object_info>::const_iterator obj_it = objects.begin(); obj_it != objects.end(); ++obj_it) {
            const object_info &obj_info = obj_it->second;
            std::map<PEAT_Addr, DWARF_lines_addresses>::const_iterator src_ptr = obj_info.obj_sources.lower_bound(address); // return last if nothing found
            if (obj_info.obj_sources.end() != src_ptr) {
                const DWARF_lines_addresses &src_store = src_ptr->second;
                line_number = src_store.source_line;
                source_name = *src_store.source_file; // TODO dangerous operation. Pointer might be better. if(0 != ptr) {
                break;
            }
        }
        __DEBUG_EPILOG_TRACTOR_LAYER__
    }
    void dump_symbols(const std::string &path, const std::string &mask, const int rank) const;

  private:
    struct symbol_info {
        symbol_info() : addr_begin(0), addr_end(0) {}
        PEAT_Addr addr_begin;
        PEAT_Addr addr_end;
        std::string symbol_name;
    };

    struct DWARF_lines_addresses {
        DWARF_lines_addresses() : address(0), source_line(-1) {}
        PEAT_Addr address; // Source line address. Duplicate the key in map<>
        int source_line;
        std::set<std::string>::const_iterator source_file; // iterator to common file names storage
        std::set<std::string>::const_iterator source_dir;  // iterator to common directory names storage
    };

    struct object_info {
        object_info() : obj_addr_base(0) {}
        PEAT_Addr obj_addr_base;
        std::string obj_path;
        std::map<PEAT_Addr, symbol_info> obj_symbols;
        std::map<PEAT_Addr, DWARF_lines_addresses> obj_sources;
    };

    std::map<int, object_info> objects;

    void initialize(object_info &object);
    void read_data(const FILE *file, void *data, const size_t offset, const size_t size) const;
    void process_sym_tab(const FILE *file, const void *symtab_header, const void *symtab_string_header, object_info &object);
    void process_DWARF_line_info(const FILE *file, const void *section_header, object_info &object);

    // local file information data structure in DWARF
    struct DWARF_line_info {
        DWARF_line_info() : mod_time(0), file_size(0) {}
        unsigned long mod_time;
        unsigned long file_size;
        std::set<std::string>::const_iterator source_file; // iterator to common file names storage
        std::set<std::string>::const_iterator source_dir;  // iterator to common directory names storage
    };
    std::set<std::string> source_file_common_storage;                     // Common storage for unique file names for all CU and objects
    std::set<std::string> source_dir_common_storage;                      // Common storage for unique directory names for all CU and objects
    std::set<std::string>::const_iterator source_dir_compilation_default; // iterator to default compilation directory name

    // DWARF state machine registers
    struct DWARF_registers {
        DWARF_registers() : stm_address(0), stm_op_index(0), stm_file(1), stm_line(1), stm_column(0),
                            stm_is_stmt(false), stm_basic_block(false), stm_prologue_end(false), stm_epilogue_begin(false),
                            stmt_isa(0), stmt_discriminator(0), stm_end_sequence(false) {}
        PEAT_Addr stm_address;
        unsigned int stm_op_index; // DWARF4
        unsigned int stm_file;
        unsigned int stm_line;
        unsigned int stm_column;
        bool stm_is_stmt; // determined by default_is_stmt in the line number program header
        bool stm_basic_block;
        bool stm_prologue_end;            // DWARF4
        bool stm_epilogue_begin;          // DWARF4
        unsigned long stmt_isa;           // DWARF4 The isa value 0 specifies that the instruction set is the architecturally determined default instruction set.
        unsigned long stmt_discriminator; // DWARF4
        bool stm_end_sequence;
    };

    int get_LEB_128(unsigned char *data, unsigned long &result, const int sign) const;
    const char *debug_line_section_name;
    int debug_line_section_name_len;
};

#if defined(__CPLUSPLUS__) || defined(__cplusplus)
} // extern "C"
#endif

#endif // __ELF_PARSER_H__
