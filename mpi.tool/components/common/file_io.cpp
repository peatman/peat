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

#include <cassert>
#include <iomanip>
#include <iostream>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#if !defined(_WIN32) || defined(__MINGW32__)
#include <unistd.h>
#endif
#include <vector>

#include "common_types.h"
#include "file_io.h"

#define UTILCOUT \
    if (verbose) \
    std::cout
#define UTILCERR \
    if (verbose) \
    std::cerr
#define UTILDBG(x)    \
    if (verbose >= x) \
    std::cerr
#if defined(_WIN32) && !defined(__MINGW32__)
#define stat(x, y) _stat64(x, y)
#endif

void File_Storage_GZ::open(const PEAT_Rank rank, const char *mode_str)
{
#ifdef PEAT_OMP
    fptr.at(rank).read_lock.lock();
#endif
    std::string mode;
    if (flags & PEAT_FIO_FLAG_READ) {
        if (mode_str)
            mode = mode_str;
        else
            mode = "r";
    } else if (flags & PEAT_FIO_FLAG_WRITE) {
        if (mode_str)
            mode = mode_str;
        else
            mode = "wT";
    } else {
        throw std::logic_error("Unknow file mode in FileStorage ctor.");
    }
    fptr.at(rank).ptr = gzopen(fptr.at(rank).name.c_str(), mode.c_str());
    if (fptr.at(rank).ptr) {
        UTILCOUT << "File " << fptr.at(rank).name << " successfully opened." << std::endl;
    } else {
        UTILCERR << "Can't open file " << fptr.at(rank).name << " error " << strerror(errno) << std::endl;
        // throw std::runtime_error(errTxt.str());
    }
#ifdef PEAT_OMP
    fptr.at(rank).read_lock.unlock();
#endif
}
void File_Storage_GZ::close(const PEAT_Rank rank)
{
    if (fptr.count(rank) == 1) {
        if (fptr.at(rank).ptr) {
            gzclose(fptr.at(rank).ptr);
            fptr.at(rank).ptr = NULL;
        } else {
            UTILCERR << "File " << fptr.at(rank).name << " is already closed" << strerror(errno) << std::endl;
            // throw std::runtime_error(errTxt.str());
        }
    }
}

void File_Storage_GZ::open(const std::map<PEAT_Rank, std::string> &files, const char *mode_str)
{
    std::string mode;
    if (flags & PEAT_FIO_FLAG_READ) {
        if (mode_str)
            mode = mode_str;
        else
            mode = "r";
    } else if (flags & PEAT_FIO_FLAG_WRITE) {
        if (mode_str)
            mode = mode_str;
        else
            mode = "wT";
    } else {
        throw std::logic_error("Unknow file mode in FileStorage ctor.");
    }

    for (std::map<PEAT_Rank, std::string>::const_iterator it = files.begin(); it != files.end(); ++it) {
        std::pair<std::map<PEAT_Rank, File_Name_And_Ptr_GZ>::iterator, PEAT_Bool> ret;
        const std::string &currentfile_name = it->second;

        File_Name_And_Ptr_GZ fnameptr;
        fnameptr.name = currentfile_name;
        if (flags & PEAT_FIO_FLAG_READ) { // no need to check file if writing
            if (stat(fnameptr.name.c_str(), &fnameptr.sb)) {
                // int this_errno = errno;
                fnameptr.name += ".gz";
                stat(fnameptr.name.c_str(), &fnameptr.sb);
            }
        }
#ifdef PEAT_OMP
        fnameptr.read_lock.lock();
#endif
        if (!(flags & PEAT_FIO_FLAG_LAZY)) {
            fnameptr.ptr = gzopen(fnameptr.name.c_str(), mode.c_str());
            if (fnameptr.ptr) {
                UTILCOUT << "File " << fnameptr.name << " successfully opened." << std::endl;
            } else {
                UTILCERR << "Can't open file " << fnameptr.name << " error " << strerror(errno) << std::endl;
                // throw std::runtime_error(errTxt.str());
            }
        }
        ret = fptr.insert(std::pair<PEAT_Rank, File_Name_And_Ptr_GZ>(it->first, fnameptr));
        if (!ret.second) {
            UTILCERR << "Can't store pointer to file " << fnameptr.name << std::endl;
        }
#ifdef PEAT_OMP
        fnameptr.read_lock.unlock();
#endif
    }
    return;
}

File_Storage_GZ::File_Storage_GZ(const std::map<PEAT_Rank, std::string> &files, const int iflags, const char *mode_str, const int v) : verbose(v), flags(iflags), bytes_read_total(0)
{
    open(files, mode_str);
}

File_Storage_GZ::File_Storage_GZ(const std::string &file, const int iflags, const char *mode_str, const int v) : verbose(v), flags(iflags), bytes_read_total(0)
{
    std::map<PEAT_Rank, std::string> file_stub;
    file_stub[0] = file;
    open(file_stub, mode_str);
}

File_Storage_GZ::~File_Storage_GZ()
{
    if (!fptr.empty()) {
        for (std::map<PEAT_Rank, File_Name_And_Ptr_GZ>::iterator it = fptr.begin(); it != fptr.end(); ++it) {
            UTILCOUT << "File " << it->second.name << " successfully closed." << std::endl;
#ifdef PEAT_OMP
            it->second.read_lock.lock();
#endif
            if (it->second.ptr)
                gzclose(it->second.ptr);
            it->second.ptr = 0;
#ifdef PEAT_OMP
            it->second.read_lock.unlock();
#endif
        }
        fptr.clear();
    }
}

PEAT_Bool File_Storage_GZ::read_lines(String_Split &lines, const PEAT_Rank rank, const char delim, const char term)
{
    PEAT_Message_Size localBytesRead = 0, beforeReadOffset = 0, afterReadOffset = 0;
    if (fptr.empty()) {
        return false;
    }
    if (!is_file_good(rank)) {
        return false;
    }
    if (!fptr.count(rank)) {
        std::ostringstream errTxt;
        errTxt << "Can't access to the file at rank " << rank << "\n";
        throw std::runtime_error(errTxt.str());
    }
    File_Name_And_Ptr_GZ &file = fptr.at(rank);
    assert(file.ptr);
#ifdef PEAT_OMP
    file.read_lock.lock();
#endif

    long maxLineInDatFile = 0;
    char *ptrBuf = lines.getNewBuffer(maxLineInDatFile);
    ptrBuf[maxLineInDatFile - 2] = 0x0; // indicator of the buffer is not enougth (instead strnlen() usage)
    beforeReadOffset = gzoffset(file.ptr);
    gzgets(file.ptr, ptrBuf, maxLineInDatFile);
    afterReadOffset = gzoffset(file.ptr);
    localBytesRead += llabs(afterReadOffset - beforeReadOffset);
    if (gzeof(file.ptr)) {
        UTILCOUT << "End of " << file.name << " reached" << std::endl;
        if (flags & PEAT_FIO_FLAG_ICLOSE) {
            if (file.ptr)
                gzclose(file.ptr);
            file.ptr = 0;
            file.was_closed = true;
            UTILCOUT << file.name << " was closed" << std::endl;
        }
    } else {
        while (0x0 != ptrBuf[maxLineInDatFile - 2] && String_Split::get_eventOL() != ptrBuf[maxLineInDatFile - 2] && String_Split::getWinEOL() != ptrBuf[maxLineInDatFile - 2]) { // gzgets put \0 byte at the end
            // UTILCERR << "Line in FileStorage::read_lines() longer than " << maxLineInDatFile << " size " << strnlen(err, maxLineInDatFile) << endl;
            ptrBuf = lines.getAdditionBuffer(maxLineInDatFile);
            ptrBuf[maxLineInDatFile - 2] = 0x0;
            beforeReadOffset = gzoffset(file.ptr);
            gzgets(file.ptr, ptrBuf, maxLineInDatFile); // file.ptr->get_event(tladd, maxLineInDatFile);
            afterReadOffset = gzoffset(file.ptr);
            localBytesRead += llabs(afterReadOffset - beforeReadOffset); // localBytesRead += file.ptr->gcount();
        }
    }

    lines.split(delim, term);
    // cerr << "DEBUG " << lines << "\n";
    if (lines.empty()) {
        UTILCERR << "No delimeted lines found in " << lines.data() << " while reading " << file.name << std::endl;
    }
    // P2PCERR << "FileStorage::read_lines " << lines << endl;
#ifdef PEAT_OMP
#pragma omp atomic
#endif
    bytes_read_total += localBytesRead;
#ifdef PEAT_OMP
    file.read_lock.unlock();
#endif
    return true;
}

std::ostream &operator<<(std::ostream &dbg, const File_Storage_GZ *me)
{
    long numberOfFiles = (long)me->fptr.size();
    dbg << "Rank\tName\tSize(bytes)" << std::endl;
    for (std::map<PEAT_Rank, File_Name_And_Ptr_GZ>::const_iterator it = me->fptr.begin(); it != me->fptr.end(); ++it) {
        dbg << it->first << "\t" << it->second.name << "\t" << it->second.sb.st_size << std::endl;
    }
    dbg << "Total " << numberOfFiles << " files in storage." << std::endl;
    return dbg;
}
