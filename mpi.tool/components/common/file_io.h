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

#ifndef __FILE_IO_H__
#define __FILE_IO_H__

#include <fstream>
#include <map>
#include <string>

#include <stdarg.h>
#include <sys/stat.h>

#include "zlib.h"

#include "utils.h"
#define PEAT_FIO_FLAG_READ 1
#define PEAT_FIO_FLAG_LAZY 2
#define PEAT_FIO_FLAG_ICLOSE 4
#define PEAT_FIO_FLAG_WRITE 65536

struct File_Name_And_Ptr_GZ {
    File_Name_And_Ptr_GZ() : ptr(0), was_closed(false) {}
    std::string name;
    gzFile ptr;
#if !defined(_WIN32) || defined(__MINGW32__)
    struct stat sb;
#else
    struct _stat64 sb;
#endif
    PEAT_Bool was_closed;
#ifdef PEAT_OMP
    PEAT_Mutex read_lock;
    PEAT_Mutex write_lock;
    /*
    File_Name_And_Ptr_GZ(const File_Name_And_Ptr_GZ& s) : name(s.name), ptr(s.ptr), was_closed(s.was_closed),sb(s.sb) {}
    File_Name_And_Ptr_GZ& operator=(const File_Name_And_Ptr_GZ& s)  {
        ptr = s.ptr;
        was_closed = s.was_closed;
        sb = s.sb;
        name = s.name;
        return *this;
    }*/
#endif
};

class File_Storage_GZ
{
  public:
    File_Storage_GZ(const std::map<PEAT_Rank, std::string> &files, const int flags, const char *mode_str = NULL, const int v = false);
    File_Storage_GZ(const std::string &file, const int flags, const char *mode_str = NULL, const int v = false);
    void open(const std::map<PEAT_Rank, std::string> &files, const char *mode_str = NULL);
    void open(const PEAT_Rank rank, const char *mode_str = NULL);
    void close(const PEAT_Rank rank);
    bool check_open(const PEAT_Rank rank)
    {
        if (fptr.at(rank).ptr)
            return true;
        if (flags & PEAT_FIO_FLAG_LAZY && (!fptr.at(rank).was_closed)) {
            open(rank);
            if (0 == fptr.at(rank).ptr) {
                return false;
            } else {
                return true;
            }
        } else {
            return false;
        }
        return false;
    }
    virtual ~File_Storage_GZ();

    void rewind(const PEAT_Rank rank = -1)
    {
        if (rank == -1) {
            for (std::map<PEAT_Rank, File_Name_And_Ptr_GZ>::iterator it = fptr.begin(); it != fptr.end(); ++it) {
                it->second.was_closed = false;
                check_open(it->first);
#ifdef PEAT_OMP
                it->second.write_lock.lock();
#endif
                gzrewind(it->second.ptr);
#ifdef PEAT_OMP
                it->second.write_lock.unlock();
#endif
            }
            bytes_read_total = 0;
        } else {
            fptr.at(rank).was_closed = false;
            if (check_open(rank)) {
                bytes_read_total -= gzoffset(fptr.at(rank).ptr);
#ifdef PEAT_OMP
                fptr.at(rank).write_lock.lock();
#endif
                gzrewind(fptr.at(rank).ptr);
#ifdef PEAT_OMP
                fptr.at(rank).write_lock.unlock();
#endif
            }
        }
    }
    virtual PEAT_Num get_number_of_files() const
    {
        return fptr.size();
    }
    virtual PEAT_Bool is_file_good(const PEAT_Rank rank)
    {
        size_t count = fptr.count(rank);
        if (count == 0) {
            return false;
        }
        const File_Name_And_Ptr_GZ &curFile = fptr.at(rank);
        if (!check_open(rank)) {
            return false;
        }
        return !gzeof(curFile.ptr);
    }
    virtual PEAT_Bool is_any_file_good()
    {
        for (std::map<PEAT_Rank, File_Name_And_Ptr_GZ>::const_iterator it = fptr.begin(); it != fptr.end(); ++it) {
            if (is_file_good(it->first)) {
                return true;
            }
        }
        return false;
    }
    virtual PEAT_Bool read_lines(String_Split &lines, const PEAT_Rank rank, const char delim, const char term = 0);
    virtual void puts(PEAT_Rank rank, const std::string &line)
    {
        if (!(flags & PEAT_FIO_FLAG_WRITE)) {
            std::ostringstream tmp;
            tmp << "You couldn't write to file " << fptr.at(0).name << " opened to read. location: " << __FILE__ << ":" << __LINE__;
            throw std::logic_error(tmp.str());
        }
        if (fptr.empty())
            return;
        if (rank == -1)
            rank = fptr.begin()->first;
        if (fptr.count(rank) < 1) {
            fprintf(stderr, "write_line: No file is opened for %d rank. Returning NULL. Program may crash.\n", rank);
            return;
        }
        if (!check_open(rank)) {
            return;
        }
#ifdef PEAT_OMP
        fptr.at(rank).write_lock.lock();
#endif
        gzputs(fptr.at(rank).ptr, line.c_str());
#ifdef PEAT_OMP
        fptr.at(rank).write_lock.unlock();
#endif
    }
    virtual void puts(PEAT_Rank rank, const char *line)
    {
        if (!(flags & PEAT_FIO_FLAG_WRITE)) {
            std::ostringstream tmp;
            tmp << "You couldn't write to file " << fptr.at(0).name << " opened to read. location: " << __FILE__ << ":" << __LINE__;
            throw std::logic_error(tmp.str());
        }
        if (fptr.empty())
            return;
        if (rank == -1)
            rank = fptr.begin()->first;

        if (fptr.count(rank) < 1) {
            fprintf(stderr, "write_line: No file is opened for %d rank. Returning NULL. Program may crash.\n", rank);
            return;
        }
        if (!check_open(rank)) {
            return;
        }
#ifdef PEAT_OMP
        fptr.at(rank).write_lock.lock();
#endif
        gzputs(fptr.at(rank).ptr, line);
#ifdef PEAT_OMP
        fptr.at(rank).write_lock.unlock();
#endif
    }
    virtual void printf(PEAT_Rank rank, const char *format, ...)
    {
        if (!(flags & PEAT_FIO_FLAG_WRITE)) {
            std::ostringstream tmp;
            tmp << "You couldn't write to file " << fptr.at(0).name << " opened to read. location: " << __FILE__ << ":" << __LINE__;
            throw std::logic_error(tmp.str());
        }
        if (fptr.empty())
            return;
        if (rank == -1)
            rank = fptr.begin()->first;

        if (fptr.count(rank) < 1) {
            fprintf(stderr, "write_line: No file is opened for %d rank. Returning NULL. Program may crash.\n", rank);
            return;
        }
        if (!check_open(rank)) {
            return;
        }
        va_list argptr;
        va_start(argptr, format);
#ifdef PEAT_OMP
        fptr.at(rank).write_lock.lock();
#endif
        gzvprintf(fptr.at(rank).ptr, format, argptr);
#ifdef PEAT_OMP
        fptr.at(rank).write_lock.unlock();
#endif
    }

    gzFile get_file_ptr(const PEAT_Rank rank)
    {
        if (fptr.count(rank) < 1) {
            fprintf(stderr, "get_file_ptr:No file is opened for %d rank. Returning NULL. Program may crash.\n", rank);
            return NULL;
        }
        const File_Name_And_Ptr_GZ &curFile = fptr.at(rank);
        check_open(rank);
        return curFile.ptr;
    }
    gzFile get_first_file_ptr()
    {
        if (fptr.empty()) {
            fprintf(stderr, "get_file_ptr:No files are opened. Returning NULL. Program may crash.\n");
            return NULL;
        }
        check_open(fptr.begin()->first);
        const File_Name_And_Ptr_GZ &curFile = fptr.begin()->second;
        return curFile.ptr;
    }
    PEAT_Message_Size get_file_size(int rank) const
    {
        if (!fptr.empty()) {
            if (fptr.count(rank)) {
                return fptr.at(rank).sb.st_size;
            } else {
                return 0;
            }
        }
        return 0;
    }
    PEAT_Message_Size get_all_files_size() const
    {
        PEAT_Message_Size totalBytes = 0;
        if (!fptr.empty()) {
            for (std::map<PEAT_Rank, File_Name_And_Ptr_GZ>::const_iterator it = fptr.begin(); it != fptr.end(); ++it) {
                totalBytes += it->second.sb.st_size;
            }
        }
        return totalBytes;
    }
    PEAT_Message_Size get_bytes_read() const
    {
        return bytes_read_total;
    }
    PEAT_Message_Size get_offest(PEAT_Rank rank)
    {
        check_open(rank);
        return gzoffset(fptr.at(rank).ptr);
    }

    friend std::ostream &operator<<(std::ostream &, const File_Storage_GZ *);

  protected:
    // std::map<PEAT_Rank, std::string> get_file_names(const char *dir, const char *tmpl);
    std::map<PEAT_Rank, File_Name_And_Ptr_GZ> fptr;
    int verbose;
    int flags;
    PEAT_Message_Size bytes_read_total;
};

#endif // __FILE_IO_H__
