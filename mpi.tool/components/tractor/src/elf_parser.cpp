/*
 * This file is part of PEAT (Performance Estimation & Analyzing Toolkit).
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

#include "elf_parser.h"
#include <assert.h>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <stdio.h>
#include <string>
#include <vector>

elf_symbols::elf_symbols() : debug_line_section_name(".debug_line")
{
    __DEBUG_PROLOG_TRACTOR_LAYER__
    debug_line_section_name_len = strlen(debug_line_section_name);
    source_dir_compilation_default = source_dir_common_storage.insert(source_dir_common_storage.begin(), "."); // DWARF directory table has default element [0] as pointer to default directory
    __DEBUG_EPILOG_TRACTOR_LAYER__
}

void elf_symbols::initialize(object_info &object)
{
    // printf("PATH %s\n", object.obj_path.c_str());
    __DEBUG_PROLOG_TRACTOR_LAYER__
    assert(!object.obj_path.empty());
    FILE *file;
    std::ostringstream err_txt;
    file = fopen(object.obj_path.c_str(), "rb");
    if (NULL == file) {
        err_txt << "Input file '" << object.obj_path << "' cannot be opened.\n";
        std::cerr << err_txt.str();
        return;
        // throw std::runtime_error(err_txt.str());
    }

    Elf64_Ehdr elf_header;
    read_data(file, &elf_header, 0, sizeof(Elf64_Ehdr));

    if (elf_header.e_ident[EI_MAG0] != ELFMAG0 || elf_header.e_ident[EI_MAG1] != ELFMAG1 ||
        elf_header.e_ident[EI_MAG2] != ELFMAG2 || elf_header.e_ident[EI_MAG3] != ELFMAG3) {

        err_txt << "Not an ELF file\n";
        std::cerr << err_txt.str();
        return;
        // throw std::runtime_error(err_txt.str());
    }
    if (ELFDATA2LSB != elf_header.e_ident[EI_DATA]) {
        err_txt << "Little endian supported only\n";
        std::cerr << err_txt.str();
        return;
        // throw std::runtime_error(err_txt.str());
    }
    if (ELFCLASS64 != elf_header.e_ident[EI_CLASS]) {
        err_txt << "ELF64 supported only\n";
        std::cerr << err_txt.str();
        return;
        // throw std::runtime_error(err_txt.str());
    }

    // if section header table file offset is present
    if (!elf_header.e_shoff) {
        return;
    }
    if (SHN_UNDEF == elf_header.e_shstrndx) {
        err_txt << "Section headers string table is not present\n";
        std::cerr << err_txt.str();
        return;
        // throw std::runtime_error(err_txt.str());
    }

    std::vector<Elf64_Shdr> section_headers(elf_header.e_shnum);
    read_data(file, section_headers.data(), elf_header.e_shoff, elf_header.e_shentsize * elf_header.e_shnum);

    // load strings section (shstrtab)
    // uncomment if you need to print section names
    std::vector<char> section_name_strings_table(section_headers[elf_header.e_shstrndx].sh_size);
    read_data(file, section_name_strings_table.data(), section_headers[elf_header.e_shstrndx].sh_offset, section_headers[elf_header.e_shstrndx].sh_size);

    Elf64_Shdr *dynsym_header = 0;
    Elf64_Shdr *dynsymstrings_header = 0;
    Elf64_Shdr *symtab_header = 0;
    Elf64_Shdr *symtabstrings_header = 0;
    Elf64_Shdr *dwarf_debug_line_header = 0;
    Elf64_Shdr *dwarf_debug_line_str_header = 0; // DWARF 5: strings of the line table (DW_FORM_line_strp)
    Elf64_Shdr *dwarf_debug_str_header = 0;      // DWARF 5: strings referenced with DW_FORM_strp

    debug_printf("Section headers count %d\n", elf_header.e_shnum);
    for (int i = 0; i < elf_header.e_shnum; ++i) {
        const char *section_name = section_name_strings_table.data() + section_headers[i].sh_name;
        debug_printf("[%d]: %s size %ld type %u link %u\n", i, section_name, section_headers[i].sh_size, section_headers[i].sh_type, section_headers[i].sh_link);
        if (SHN_UNDEF != section_headers[i].sh_name) {
            //            if(i == elf_header.e_shstrndx) {
            //                printf("=>");
            //            }
            //            if(SHT_SYMTAB_SHNDX == section_headers[i].sh_type) {
            //                printf("section SYMTAB_SHNDX\n");
            //            }
            //            if(SHT_STRTAB == section_headers[i].sh_type) {
            //                printf("section STRINGS\n");
            //            }
            if (SHT_DYNSYM == section_headers[i].sh_type) {
                if (dynsym_header) {
                    throw std::runtime_error("Duplicated .dynsym section found");
                }
                dynsym_header = &section_headers[i];
                const long string_header_id = section_headers[i].sh_link;
                assert(0 <= string_header_id);
                assert(elf_header.e_shnum > string_header_id);
                dynsymstrings_header = &section_headers[string_header_id];
                // printf("section DYNAMIC_SYMBOL found\n");
            }
            if (SHT_SYMTAB == section_headers[i].sh_type) {
                if (symtab_header) {
                    throw std::runtime_error("Duplicated .symtab section found");
                }
                symtab_header = &section_headers[i];
                const long string_header_id = section_headers[i].sh_link;
                assert(0 <= string_header_id);
                assert(elf_header.e_shnum > string_header_id);
                symtabstrings_header = &section_headers[string_header_id];
                // printf("section SYMBOL found\n");
            }
            if (0 == strcmp(section_name, debug_line_section_name)) { /* exact: DWARF 5 also has .debug_line_str */
                if (dwarf_debug_line_header) {
                    throw std::runtime_error("Duplicated .debug_line section found");
                }
                dwarf_debug_line_header = &section_headers[i];
            }
            if (0 == strcmp(section_name, ".debug_line_str")) {
                dwarf_debug_line_str_header = &section_headers[i];
            }
            if (0 == strcmp(section_name, ".debug_str")) {
                dwarf_debug_str_header = &section_headers[i];
            }
        }
    }
    // Look for symbol name in .symtab first
    if (symtab_header) {
        process_sym_tab(file, symtab_header, symtabstrings_header, object);
    } else if (dynsym_header) { // try to look into dynsym if no .symtab found
        process_sym_tab(file, dynsym_header, dynsymstrings_header, object);
    }
    // look into DWARF .debug_line for source file name and line
    if (dwarf_debug_line_header) {
        process_DWARF_line_info(file, dwarf_debug_line_header, dwarf_debug_line_str_header, dwarf_debug_str_header, object);
    }
    fclose(file);
    __DEBUG_EPILOG_TRACTOR_LAYER__
}

void elf_symbols::read_data(const FILE *file, void *data, const size_t offset, const size_t size) const
{
    __DEBUG_PROLOG_TRACTOR_LAYER__
    if (0 < size) {
        fseek(const_cast<FILE *>(file), offset, SEEK_SET);
        size_t result = fread(data, size, 1, const_cast<FILE *>(file)); // return number of items read
        if (1 != result) {
            throw std::runtime_error("Can't read debug information from file");
        }
    }
    __DEBUG_EPILOG_TRACTOR_LAYER__
}

void elf_symbols::process_sym_tab(const FILE *file, const void *symtab_header, const void *symtab_string_header, object_info &object)
{
    __DEBUG_PROLOG_TRACTOR_LAYER__
    const Elf64_Shdr *header = reinterpret_cast<const Elf64_Shdr *>(symtab_header);
    const Elf64_Shdr *strings_header = reinterpret_cast<const Elf64_Shdr *>(symtab_string_header);
    assert(header);
    assert(strings_header);
    assert(SHT_DYNSYM == header->sh_type || SHT_SYMTAB == header->sh_type); // should be symbol table
    const PEAT_Addr obj_address_load_base = object.obj_addr_base;
    debug_printf("section strings SIZE %ld\n", strings_header->sh_size);

    // load symbol_table strings section first
    std::vector<char> strings_for_symbol_table(strings_header->sh_size);
    read_data(file, strings_for_symbol_table.data(), strings_header->sh_offset, strings_header->sh_size);

    // sh_size is the section size in bytes, not the entry count, so divide by entry size to get the symbol count
    const size_t symbol_entry_size = header->sh_entsize ? header->sh_entsize : sizeof(Elf64_Sym);
    const size_t symbol_count = header->sh_size / symbol_entry_size;
    std::vector<Elf64_Sym> symbol_entry(symbol_count);
    read_data(file, symbol_entry.data(), header->sh_offset, header->sh_size);

    debug_printf("symbol_entry SIZE %zu\n", symbol_count);
    for (size_t j = 0; j < symbol_count; ++j) {
        if (SHN_UNDEF != symbol_entry[j].st_shndx) {
            // symbol is not external
            if (STT_FUNC == ELF32_ST_TYPE(symbol_entry[j].st_info)) {

                std::string obj_name = (symbol_entry[j].st_name < strings_header->sh_size) ? strings_for_symbol_table.data() + symbol_entry[j].st_name : "";
                // printf("\t%3d: size %ld \tvalue 0x%lx info 0x%x shndx %d other %d _name %d %s\n", j, symbol_entry[j].st_size, symbol_entry[j].st_value,
                //        symbol_entry[j].st_info, symbol_entry[j].st_shndx, symbol_entry[j].st_other, symbol_entry[j].st_name, obj_name.c_str());

                if (!obj_name.empty() && symbol_entry[j].st_size && symbol_entry[j].st_value) {
                    const PEAT_Addr symbol_address = reinterpret_cast<PEAT_Addr>(reinterpret_cast<unsigned long>(obj_address_load_base) + symbol_entry[j].st_value);
                    symbol_info &sym_info = object.obj_symbols[symbol_address];
                    sym_info.addr_begin = reinterpret_cast<PEAT_Addr>(symbol_address);
                    sym_info.addr_end = reinterpret_cast<PEAT_Addr>(reinterpret_cast<unsigned long>(symbol_address) + symbol_entry[j].st_size);
                    sym_info.symbol_name = obj_name;
                    debug_printf("Obj=%s Base %llx st_value=%llx\n", obj_name.c_str(), obj_address_load_base, symbol_entry[j].st_value);
                }
            }
        }
    }
    __DEBUG_EPILOG_TRACTOR_LAYER__
}

void elf_symbols::process_DWARF_line_info(const FILE *fileptr, const void *section_header, const void *line_str_section_header, const void *str_section_header, elf_symbols::object_info &object)
{
    __DEBUG_PROLOG_TRACTOR_LAYER__
    assert(4 == sizeof(int));
    assert(8 == sizeof(long));
    const Elf64_Shdr *header = reinterpret_cast<const Elf64_Shdr *>(section_header);
    assert(header);
    debug_printf("DWARF .debug_line section %ld\n", header->sh_size);
    DWARF_registers stm_regs;
    // load .debug_line
    std::vector<unsigned char> info_table(header->sh_size);
    read_data(fileptr, info_table.data(), header->sh_offset, header->sh_size);
    // DWARF 5 string sections (may be absent)
    std::vector<unsigned char> line_str_table;
    std::vector<unsigned char> str_table;
    if (line_str_section_header) {
        const Elf64_Shdr *h = reinterpret_cast<const Elf64_Shdr *>(line_str_section_header);
        line_str_table.resize(h->sh_size);
        read_data(fileptr, line_str_table.data(), h->sh_offset, h->sh_size);
    }
    if (str_section_header) {
        const Elf64_Shdr *h = reinterpret_cast<const Elf64_Shdr *>(str_section_header);
        str_table.resize(h->sh_size);
        read_data(fileptr, str_table.data(), h->sh_offset, h->sh_size);
    }

    unsigned char *data_ptr = info_table.data();
    unsigned char *section_end = info_table.data() + header->sh_size;
    while (data_ptr < section_end) {
        // Local file names and directory names storages. All strings go to common storages
        std::map<int, std::set<std::string>::const_iterator> debug_line_dir_table;
        std::map<int, DWARF_line_info> debug_line_file_table;

        const unsigned char *start_of_block = data_ptr;
        unsigned long unit_length = *reinterpret_cast<const unsigned int *>(data_ptr);
        unsigned int offset_size = 4;         // size of section offsets and of header_length: 4 in 32-bit DWARF, 8 in 64-bit DWARF (7.4)
        unsigned int initial_length_size = 4; // bytes taken by the unit_length field itself
        if (0xffffffff == unit_length) {
            // 64-bit DWARF: escape value followed by the 8-byte unit length
            unit_length = *reinterpret_cast<const unsigned long *>(data_ptr + 4);
            offset_size = 8;
            initial_length_size = 12;
        } else if (unit_length >= 0xfffffff0) {
            debug_printf("elf_symbols::process_dwarf_line_info. Error: reserved unit length value 0x%lx.\n", unit_length);
            return;
        }
        const unsigned char *unit_end = start_of_block + initial_length_size + unit_length;
        if (unit_end > section_end) {
            debug_printf("elf_symbols::process_dwarf_line_info. Error: .debug_line is corrupted, unit length %lu exceeds the section (%lu bytes)\n", unit_length, header->sh_size);
            return;
        }
        debug_printf(".debug_info unit length %lu (%s DWARF)\n", unit_length, 8 == offset_size ? "64-bit" : "32-bit");
        data_ptr += initial_length_size;

        const unsigned int version_value = *reinterpret_cast<const unsigned short *>(data_ptr); // 2-byte field; do not read/write beyond it (DWARF 5 keeps address_size right after)
        const unsigned int *version = &version_value;
        debug_printf(".debug_info DWARF Version %d\n", *version);
        if (2 != *version && 3 != *version && 4 != *version && 5 != *version) { // A version number (see Appendix F). This number is specific to the line number information and is independent of the DWARF version number.
            debug_printf("elf_symbols::process_dwarf_line_info. Error: .debug_line DWARF version is %u. Versions 2,3,4,5 are supported only.\n", *version);
            return;
        }
        data_ptr += 2; // skip 2 bytes

        if (5 <= *version) {
            // DWARF 5: address_size (ubyte) and segment_selector_size (ubyte) precede header_length
            const unsigned int address_size = *data_ptr;
            const unsigned int segment_selector_size = *(data_ptr + 1);
            data_ptr += 2;
            debug_printf(".debug_info address size %u, segment selector size %u\n", address_size, segment_selector_size);
            if (8 != address_size || 0 != segment_selector_size) {
                debug_printf("elf_symbols::process_dwarf_line_info. Error: only 8-byte addresses without segment selectors are supported.\n");
                return;
            }
        }

#if DWARFDebug
        const unsigned long header_length = (8 == offset_size) ? *reinterpret_cast<const unsigned long *>(data_ptr) : *reinterpret_cast<const unsigned int *>(data_ptr);
        debug_printf(".debug_info header length %lu\n", header_length);
#endif
        data_ptr += offset_size; // header_length: 4 bytes in 32-bit DWARF, 8 bytes in 64-bit DWARF

        const unsigned int min_instr_len = *data_ptr;
        debug_printf(".debug_info min instr len %d\n", min_instr_len);
        data_ptr += 1; // skip 1 bytes

        unsigned int max_op_per_instr = 1;
        if (4 <= *version) {
            max_op_per_instr = *data_ptr;
            debug_printf(".debug_info max oper per instructions %d\n", max_op_per_instr);
            data_ptr += 1; // skip 1 bytes
            if (0 == max_op_per_instr) {
                max_op_per_instr = 1;
            }
        }
        if (1 != max_op_per_instr) {
            debug_printf("maximum_operations_per_instruction %d is not supported.\n", max_op_per_instr);
            return;
        }

        bool stm_is_stmt_default = *data_ptr; // 5. default_is_stmt (ubyte)
        stm_regs.stm_is_stmt = stm_is_stmt_default;
        debug_printf(".debug_info is_stmt %d\n", stm_regs.stm_is_stmt);
        data_ptr += 1; // skip 1 bytes

        const int line_base = *(reinterpret_cast<char *>(data_ptr)); // 6. line_base (sbyte) (do not forget about sign propagation)
        debug_printf(".debug_info line base %d\n", line_base);
        data_ptr += 1; // skip 1 bytes

        const unsigned int line_range = *data_ptr; // 7. line_range (ubyte)
        debug_printf(".debug_info line range %d\n", line_range);
        data_ptr += 1; // skip 1 bytes

        const unsigned int opcode_base = *data_ptr; // 8. opcode_base (ubyte)
        debug_printf(".debug_info opcode base %d\n", opcode_base);
        data_ptr += 1; // skip 1 bytes

#if DWARFDebug
        debug_printf(".debug_info Opcodes\n");
        unsigned char opcode_len[opcode_base]; // 9. standard_opcode_lengths (array of ubyte)
        for (unsigned int i = 1; i < opcode_base; ++i) {
            opcode_len[i] = *data_ptr;
            debug_printf("\t[%d] value %d\n", i, opcode_len[i]);
            data_ptr += 1;
        }
#else
        data_ptr += opcode_base - 1;
#endif

        if (5 <= *version) {
            // DWARF 5 (6.2.4.1): directory and file name tables are described by entry formats; indices start at 0
            debug_printf(".debug_info Directories (DWARF 5)\n");
            if (!read_DWARF5_entry_table(data_ptr, offset_size, line_str_table, str_table, debug_line_dir_table, NULL)) {
                return;
            }
            debug_printf(".debug_info File names (DWARF 5)\n");
            if (!read_DWARF5_entry_table(data_ptr, offset_size, line_str_table, str_table, debug_line_dir_table, &debug_line_file_table)) {
                return;
            }
        } else {
            debug_printf(".debug_info Directories\n"); // 10. include_directories (sequence of path names)
            if (0 == *data_ptr) {
                // debug_printf("\tNo directory table exists.\n");
            } else {
                debug_line_dir_table[0] = source_dir_compilation_default;
                int idx = 1; // debug_line_dir_table[0] - The current directory of the compilation is understood to be the first entry and is not explicitly represented.
                while (*data_ptr) {
                    const std::string item = reinterpret_cast<char *>(data_ptr); // TODO what if last zero is far away
                    debug_line_dir_table[idx] = source_dir_common_storage.insert(source_dir_common_storage.begin(), item);
                    debug_printf("\t[%d] %s\n", idx, item.c_str());
                    ++idx;
                    data_ptr += item.size() + 1; // skip termination NULL
                }
            }
            assert(0 == *data_ptr); // last element must be NULL
            data_ptr += 1;          // skip last NULL item

            debug_printf(".debug_info File names\n"); // 11. file_names (sequence of file entries)
            if (0 == *data_ptr) {
                debug_printf("\tNo file name table exists.\n");
            } else {
                int idx = 1;
                while (*data_ptr) {
                    DWARF_line_info &item = debug_line_file_table[idx];
                    const std::string item_file_name = reinterpret_cast<char *>(data_ptr); // TODO better to limit string size
                    item.source_file = source_file_common_storage.insert(source_file_common_storage.begin(), item_file_name);
                    data_ptr += item_file_name.size() + 1; // skip termination NULL
                    unsigned long dir_id = 0;
                    data_ptr += get_LEB_128(data_ptr, dir_id, 0);
                    std::map<int, std::set<std::string>::const_iterator>::const_iterator local_dir_id = debug_line_dir_table.find(dir_id);
                    if (debug_line_dir_table.end() == local_dir_id) {
                        item.source_dir = source_dir_compilation_default;
                    } else {
                        item.source_dir = local_dir_id->second;
                    }
                    data_ptr += get_LEB_128(data_ptr, item.mod_time, 0);
                    data_ptr += get_LEB_128(data_ptr, item.file_size, 0);
                    debug_printf("\t[%d] dir %lu time %lu size %lu %s\n", idx, dir_id, item.mod_time, item.file_size, item_file_name.c_str());
                    ++idx;
                }
            }
            assert(0 == *data_ptr); // last element must be NULL
            data_ptr += 1;          // skip last NULL item

        }
        debug_printf("Line Information start %ld to the end %lu\n", data_ptr - start_of_block, unit_length);
        // The line information state machine start
        while (data_ptr < unit_end) {
            bool is_the_line_info_added = false;
            unsigned char opcode = *data_ptr;
            ++data_ptr;
            if (opcode_base <= opcode) {
                // debug_printf("\nSpecial opcode %d\n", opcode);
                opcode -= opcode_base;
                long oper = (opcode / line_range) * min_instr_len;
                stm_regs.stm_address = reinterpret_cast<PEAT_Addr>(reinterpret_cast<unsigned long>(stm_regs.stm_address) + oper);
                debug_printf("  Special opcode %d: advance Address by %ld to 0x%lx", opcode, oper, stm_regs.stm_address);
                oper = (opcode % line_range) + line_base;
                stm_regs.stm_line += oper;
                debug_printf(" and Line by %ld to %d\n", oper, stm_regs.stm_line);
                is_the_line_info_added = true;
            } else {
                switch (opcode) {
                case 0: // DW_LNS_extended_op
                {
                    unsigned long field_len = 0;
                    data_ptr += get_LEB_128(data_ptr, field_len, 0);
                    if (0 == field_len) {
                        debug_printf("Wrong DWARF2 extended opcode\n");
                    } else {
                        opcode = *data_ptr; // new extended opcode
                        ++data_ptr;
                        debug_printf("  Extended opcode %d: ", opcode);
                        switch (opcode) {
                        case 1: // DW_LNE_end_sequence
                        {
                            // stm_end_sequence = true;
                            is_the_line_info_added = true;
                            debug_printf("End of Sequence\n\n");
                            stm_regs = DWARF_registers(); // initialize
                            stm_regs.stm_is_stmt = stm_is_stmt_default;
                        } break;
                        case 2: // DW_LNE_set_address
                        {
                            assert(8 == sizeof(long *));
                            const unsigned long *fixed_address = reinterpret_cast<const unsigned long *>(data_ptr); // sizeof(void*) is 8
                            data_ptr += 8;                                                                         // skip 8 bytes
                            debug_printf("set Address to 0x%lx\n", *fixed_address);
                            stm_regs.stm_address = (PEAT_Addr)*fixed_address;
                        } break;
                        case 3: // DW_LNE_define_file
                        {
                            DWARF_line_info &item = debug_line_file_table[stm_regs.stm_file];
                            const std::string item_file_name = reinterpret_cast<char *>(data_ptr); // TODO better to limit string size
                            item.source_file = source_file_common_storage.insert(source_file_common_storage.begin(), item_file_name);
                            data_ptr += item_file_name.size() + 1; // skip termination NULL
                            unsigned long dir_id = 0;
                            data_ptr += get_LEB_128(data_ptr, dir_id, 0);
                            std::map<int, std::set<std::string>::const_iterator>::const_iterator local_dir_id = debug_line_dir_table.find(dir_id);
                            if (debug_line_dir_table.end() == local_dir_id) {
                                item.source_dir = source_dir_compilation_default;
                            } else {
                                item.source_dir = local_dir_id->second;
                            }
                            data_ptr += get_LEB_128(data_ptr, item.mod_time, 0);
                            data_ptr += get_LEB_128(data_ptr, item.file_size, 0);
                            debug_printf("\t[%d] dir %lu time %lu size %lu %s\n", stm_regs.stm_file, dir_id, item.mod_time, item.file_size, item_file_name.c_str());
                        } break;
                        case 4: // DW_LNE_set_discriminator
                        {
                            data_ptr += get_LEB_128(data_ptr, stm_regs.stmt_discriminator, 0);
                            debug_printf("set Discriminator to %ld\n", stm_regs.stmt_discriminator);

                        } break;
                        default:
                            debug_printf("unknown extended opcode %d\n", opcode);
                            break;
                        }
                    }
                } break;
                case 1: // DW_LNS_copy
                {
                    is_the_line_info_added = true;
                    debug_printf("  Copy\n");
                } break;
                case 2: // DW_LNS_advance_pc
                {
                    unsigned long operand = 0;
                    data_ptr += get_LEB_128(data_ptr, operand, 0);
                    stm_regs.stm_address = reinterpret_cast<PEAT_Addr>(reinterpret_cast<unsigned long>(stm_regs.stm_address) + min_instr_len * operand);
                    debug_printf("  Advance PC by %ld to %lx\n", min_instr_len * operand, stm_regs.stm_address);
                } break;
                case 3: // DW_LNS_advance_line
                {
                    unsigned long operand = 0;
                    data_ptr += get_LEB_128(data_ptr, operand, 1);
                    stm_regs.stm_line += operand;
                    debug_printf("  Advance Line by %ld to %d\n", operand, stm_regs.stm_line);
                } break;
                case 4: // DW_LNS_set_file
                {
                    unsigned long operand = 0;
                    data_ptr += get_LEB_128(data_ptr, operand, 0);
                    stm_regs.stm_file = operand;
                    debug_printf("  Set File Name to entry %ld in the File Name Table\n", operand);
                } break;
                case 5: // DW_LNS_set_column
                {
                    unsigned long operand = 0;
                    data_ptr += get_LEB_128(data_ptr, operand, 0);
                    stm_regs.stm_column = operand;
                    debug_printf("  Set column to %ld\n", operand);
                } break;
                case 6: // DW_LNS_negate_stmt
                {
                    stm_regs.stm_is_stmt = !stm_regs.stm_is_stmt;
                    debug_printf("  Set is_stmt to %d\n", stm_regs.stm_is_stmt);
                } break;
                case 7: // DW_LNS_set_basic_block
                {
                    stm_regs.stm_basic_block = true;
                    debug_printf("  Set basic block\n");
                } break;
                case 8: // DW_LNS_const_add_pc
                {
                    stm_regs.stm_address = reinterpret_cast<PEAT_Addr>(reinterpret_cast<unsigned long>(stm_regs.stm_address) + (((255 - opcode_base) / line_range) * min_instr_len));
                    debug_printf("  Advance PC by constant %d to 0x%lx\n", ((255 - opcode_base) / line_range) * min_instr_len, stm_regs.stm_address);
                } break;
                case 9: // DW_LNS_fixed_advance_pc
                {
                    const unsigned int operand_value = *reinterpret_cast<const unsigned short *>(data_ptr); // uhalf operand; the old in-place mask clobbered the next opcode
                    const unsigned int *operand = &operand_value;
                    data_ptr += 2; // skip 2 bytes
                    stm_regs.stm_address = reinterpret_cast<PEAT_Addr>(reinterpret_cast<unsigned long>(stm_regs.stm_address) + *operand);
                    debug_printf("  Advance PC by fixed size amount %d to 0x%lx\n", *operand, stm_regs.stm_address);
                } break;
                case 10: // DW_LNS_set_prologue_end
                {
                    stm_regs.stm_prologue_end = true;
                    debug_printf("  Set prologue_end to true\n");
                } break;

                case 11: // DW_LNS_set_epilogue_begin
                {
                    stm_regs.stm_epilogue_begin = true;
                    debug_printf("  Set epilogue_begin to true\n");
                } break;

                case 12: // DW_LNS_set_isa
                {
                    data_ptr += get_LEB_128(data_ptr, stm_regs.stmt_isa, 0);
                    debug_printf("  Set ISA to %ld\n", stm_regs.stmt_isa);
                } break;
                default:
                    debug_printf("  Unknown opcode %d\n", opcode);
                    break;
                }
            }
            // Only Special opcodes, DW_LNS_copy and DW_LNE_end_sequence adds a row to the DWARF address/line matrix.
            if (is_the_line_info_added && stm_regs.stm_address) {
                DWARF_lines_addresses &obj_source_line = object.obj_sources[stm_regs.stm_address];
                obj_source_line.address = stm_regs.stm_address;
                obj_source_line.source_line = stm_regs.stm_line;
                std::map<int, DWARF_line_info>::const_iterator file_it = debug_line_file_table.find(stm_regs.stm_file);
                if (debug_line_file_table.end() == file_it) {
                    if (source_file_common_storage.empty()) {
                        source_file_common_storage.insert(source_file_common_storage.begin(), "Error in DWARF debug_line parser");
                    }
                    obj_source_line.source_file = source_file_common_storage.begin(); // TODO has no better idea
                    obj_source_line.source_dir = source_dir_compilation_default;
                } else {
                    obj_source_line.source_file = file_it->second.source_file;
                    obj_source_line.source_dir = file_it->second.source_dir;
                }
            }
        } // end of while for line information state machine programm
    }
#if DWARFDebug
    for (map<unsigned long, DWARF_lines_addresses>::const_iterator it = object.obj_sources.begin(); it != object.obj_sources.end(); ++it) {
        const DWARF_lines_addresses &obj_src = it->second;
        debug_printf("%s  %11d  %#18lx\n", (*obj_src.source_file).c_str(), obj_src.source_line, obj_src.address);
    }
#endif
    __DEBUG_EPILOG_TRACTOR_LAYER__
}

/* DWARF 5 line table entry formats (section 6.2.4.1). Returns the bytes consumed by one attribute value of
   the given form, or -1 for a form this reader does not know. Strings are returned in str_out
   (is_string = true), integers in value. */
long elf_symbols::read_DWARF5_form(unsigned char *data, const unsigned long form, const unsigned int offset_size,
                                   const std::vector<unsigned char> &line_str_table, const std::vector<unsigned char> &str_table,
                                   std::string &str_out, unsigned long &value, bool &is_string) const
{
    is_string = false;
    value = 0;
    switch (form) {
    case 0x08: /* DW_FORM_string: inline, NUL-terminated */
        str_out = reinterpret_cast<const char *>(data);
        is_string = true;
        return str_out.size() + 1;
    case 0x1f: /* DW_FORM_line_strp: offset into .debug_line_str (4 bytes in 32-bit DWARF, 8 in 64-bit) */
    case 0x0e: /* DW_FORM_strp: offset into .debug_str */
    {
        const std::vector<unsigned char> &table = (0x1f == form) ? line_str_table : str_table;
        const unsigned long offset = (8 == offset_size) ? *reinterpret_cast<const unsigned long *>(data) : *reinterpret_cast<const unsigned int *>(data);
        if (offset < table.size()) {
            str_out = reinterpret_cast<const char *>(table.data() + offset);
        } else {
            debug_printf("elf_symbols::read_DWARF5_form. Error: string offset %lu outside of the string section (%lu bytes)\n", offset, table.size());
            str_out.clear();
        }
        is_string = true;
        return offset_size;
    }
    case 0x17: /* DW_FORM_sec_offset */
        value = (8 == offset_size) ? *reinterpret_cast<const unsigned long *>(data) : *reinterpret_cast<const unsigned int *>(data);
        return offset_size;
    case 0x0f: /* DW_FORM_udata */
        return get_LEB_128(data, value, 0);
    case 0x0d: /* DW_FORM_sdata */
        return get_LEB_128(data, value, 1);
    case 0x0b: /* DW_FORM_data1 */
        value = *data;
        return 1;
    case 0x05: /* DW_FORM_data2 */
        value = *reinterpret_cast<const unsigned short *>(data);
        return 2;
    case 0x06: /* DW_FORM_data4 */
        value = *reinterpret_cast<const unsigned int *>(data);
        return 4;
    case 0x07: /* DW_FORM_data8 */
        value = *reinterpret_cast<const unsigned long *>(data);
        return 8;
    case 0x1e: /* DW_FORM_data16 (MD5) */
        return 16;
    case 0x09: /* DW_FORM_block: ULEB128 length + bytes */
    {
        unsigned long len = 0;
        const int n = get_LEB_128(data, len, 0);
        return n + len;
    }
    case 0x0a: /* DW_FORM_block1 */
        return 1 + *data;
    case 0x03: /* DW_FORM_block2 */
        return 2 + *reinterpret_cast<const unsigned short *>(data);
    case 0x04: /* DW_FORM_block4 */
        return 4 + *reinterpret_cast<const unsigned int *>(data);
    default: /* DW_FORM_strx* need .debug_str_offsets; not supported */
        debug_printf("elf_symbols::read_DWARF5_form. Error: unsupported form 0x%lx in the line table header\n", form);
        return -1;
    }
}

/* Reads one DWARF 5 entry table (directories when file_table is NULL, file names otherwise):
   format_count (ubyte), format_count pairs of ULEB128 (content type, form), entries_count (ULEB128), entries.
   Returns false if a form is not supported; data_ptr is advanced past the table. */
bool elf_symbols::read_DWARF5_entry_table(unsigned char *&data_ptr, const unsigned int offset_size,
                                          const std::vector<unsigned char> &line_str_table, const std::vector<unsigned char> &str_table,
                                          std::map<int, std::set<std::string>::const_iterator> &dir_table,
                                          std::map<int, DWARF_line_info> *file_table)
{
    enum { DW_LNCT_path = 1, DW_LNCT_directory_index = 2, DW_LNCT_timestamp = 3, DW_LNCT_size = 4, DW_LNCT_MD5 = 5 };
    const unsigned int format_count = *data_ptr;
    data_ptr += 1;
    std::vector<std::pair<unsigned long, unsigned long> > formats(format_count); /* (content type, form) */
    for (unsigned int i = 0; i < format_count; ++i) {
        data_ptr += get_LEB_128(data_ptr, formats[i].first, 0);
        data_ptr += get_LEB_128(data_ptr, formats[i].second, 0);
    }
    unsigned long entries_count = 0;
    data_ptr += get_LEB_128(data_ptr, entries_count, 0);
    for (unsigned long idx = 0; idx < entries_count; ++idx) {
        std::string path;
        unsigned long dir_id = 0, mod_time = 0, file_size = 0;
        for (unsigned int i = 0; i < format_count; ++i) {
            std::string str;
            unsigned long value = 0;
            bool is_string = false;
            const long consumed = read_DWARF5_form(data_ptr, formats[i].second, offset_size, line_str_table, str_table, str, value, is_string);
            if (consumed < 0) {
                return false;
            }
            data_ptr += consumed;
            switch (formats[i].first) {
            case DW_LNCT_path:
                if (is_string) {
                    path = str;
                }
                break;
            case DW_LNCT_directory_index:
                dir_id = value;
                break;
            case DW_LNCT_timestamp:
                mod_time = value;
                break;
            case DW_LNCT_size:
                file_size = value;
                break;
            default: /* DW_LNCT_MD5 and vendor content types: skipped */
                break;
            }
        }
        if (NULL == file_table) {
            dir_table[idx] = source_dir_common_storage.insert(source_dir_common_storage.begin(), path);
            debug_printf("\t[%lu] %s\n", idx, path.c_str());
        } else {
            DWARF_line_info &item = (*file_table)[idx];
            item.source_file = source_file_common_storage.insert(source_file_common_storage.begin(), path);
            std::map<int, std::set<std::string>::const_iterator>::const_iterator local_dir_id = dir_table.find(dir_id);
            item.source_dir = (dir_table.end() == local_dir_id) ? source_dir_compilation_default : local_dir_id->second;
            item.mod_time = mod_time;
            item.file_size = file_size;
            debug_printf("\t[%lu] dir %lu time %lu size %lu %s\n", idx, dir_id, mod_time, file_size, path.c_str());
        }
    }
    return true;
}

void elf_symbols::dump_symbols(const std::string &path, const std::string &mask, const int rank) const
{
    __DEBUG_PROLOG_TRACTOR_LAYER__
    std::ostringstream file_name;
    file_name << path << mask << '.' << rank << ".sym";
    FILE *file = fopen(file_name.str().c_str(), "w");
    for (std::map<int, object_info>::const_iterator obj_it = objects.begin(); obj_it != objects.end(); ++obj_it) {
        const object_info &obj_info = obj_it->second;
        fprintf(file, "[%d]: Base: %lx Name: %s\n", obj_it->first, obj_info.obj_addr_base, obj_info.obj_path.c_str());
        for (std::map<PEAT_Addr, symbol_info>::const_iterator it = obj_info.obj_symbols.begin(); it != obj_info.obj_symbols.end(); ++it) {
            const symbol_info &sym = it->second;
            fprintf(file, "%lx %lx %s\n", sym.addr_begin, sym.addr_end, sym.symbol_name.c_str());
        }
    }
    fclose(file);
    __DEBUG_EPILOG_TRACTOR_LAYER__
}

int elf_symbols::get_LEB_128(unsigned char *data, unsigned long &result, const int sign) const
{
    __DEBUG_PROLOG_TRACTOR_LAYER__
    int bytes_read = 0;
    int shift = 0;
    unsigned char byte;
    result = 0;

    do {
        byte = *data;
        bytes_read++;
        result |= (byte & 0x7f) << shift;
        shift += 7;
        ++data;
    } while (byte & 0x80);

    if (sign && (shift < 32) && (byte & 0x40)) {
        result |= -1 << shift;
    }
    __DEBUG_EPILOG_TRACTOR_LAYER__
    return bytes_read;
}
