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

#include <sstream>
#include <stdexcept>

#include <assert.h>
#if !defined(_WIN32) || defined(__MINGW32__)
#include <dirent.h>
#include <sys/stat.h> //mkdir()
#else
#include <strsafe.h>
#include <tchar.h>
#include <windows.h>
#endif

#include <ctype.h>
#include <errno.h> //EEXIST

#include "utils.h"
// int verbose;

#define LOCOUT   \
    if (verbose) \
    std::cout
#define LOCERR   \
    if (verbose) \
    std::cerr

String_Split::String_Split(int v) : verbose(v)
{
    buf_size = 2048; // initial size of the buffer mantained
    plain_data_size = 0;
    buf = static_cast<char *>(peat_malloc_aligned(buf_size * sizeof(char), 64));
    tokens_ptr_max_size = 64; // initial number of tokens
    tokens_ptr = static_cast<char **>(peat_malloc_aligned(tokens_ptr_max_size * sizeof(char *), 64));
    ptr_head = 0;
}

String_Split::~String_Split()
{
    peat_free(buf);
    buf = 0;
    buf_size = 0;
    peat_free(tokens_ptr);
    tokens_ptr = 0;
    tokens_ptr_max_size = 0;
    ptr_head = 0;
}

String_Split &String_Split::operator=(const String_Split &from)
{
    if (this != &from) {
        peat_free(buf);
        buf = 0;
        peat_free(tokens_ptr);
        tokens_ptr = 0;
        buf_size = from.buf_size;
        buf = static_cast<char *>(peat_malloc_aligned(buf_size * sizeof(char), 64));
        memcpy(buf, from.buf, buf_size * sizeof(char));
        tokens_ptr_max_size = from.tokens_ptr_max_size;
        ptr_head = from.ptr_head;
        plain_data_size = from.plain_data_size;
        tokens_ptr = static_cast<char **>(peat_malloc_aligned(tokens_ptr_max_size * sizeof(char *), 64));
        char *begline = buf;
        char *endline = buf;
        unsigned long locPtrHead = 0;
        while (locPtrHead < ptr_head) {
            while (*endline != 0x0) {
                ++endline;
            }
            tokens_ptr[locPtrHead] = begline;
            ++endline;
            begline = endline;
            ++locPtrHead;
        }
    } else {
        LOCOUT << "operator= in String_Split tried to copy into itself.\n";
    }
    return *this;
}

std::ostream &operator<<(std::ostream &dbg, const String_Split &data)
{
    dbg << "{";
    for (unsigned long i = 0; i < data.size(); i++) {
        dbg << "{" << data.at(i) << "}";
    }
    dbg << "}";
    return dbg;
}

long String_Split::split(char delimeter, char terminator)
{
    if (!buf) {
        std::ostringstream tmp;
        tmp << "String_Split::split(). No line to split.\n";
        throw std::runtime_error(tmp.str());
    }
    if (ptr_head) {
        std::ostringstream tmp;
        tmp << "String_Split::split(). The line splitted already.\n";
        throw std::runtime_error(tmp.str());
    }
    char *begline = buf;
    char *endline = buf;
    plain_data_size = 0;
    PEAT_Bool endOfLine = (*endline == 0x0);
    while (endOfLine == false) {
        PEAT_Bool stringFound = false;
        while (!(*endline == delimeter || *endline == 0x0 || *endline == getWinEOL() || *endline == get_eventOL() || *endline == terminator)) { // take care about Windows end of line symbols
            ++endline;
            ++plain_data_size;
        }
        if (*endline == getWinEOL() || *endline == get_eventOL() || *endline == terminator) { // set Win EndOFLine to reqular EndOfLine
            *endline = 0x0;
        }
        if (*endline == 0x0) {
            endOfLine = true;
            stringFound = true;
        }
        if (*endline == delimeter) {
            stringFound = true;
        }
        if (stringFound) {
            push_back(begline);
            *endline = 0x0;
        }
        ++endline;
        ++plain_data_size;
        begline = endline;
    }
    return ptr_head;
}

const char *String_Split::at(PEAT_Num id) const
{
    if (id < ptr_head) {
        return tokens_ptr[id];
    } else {
        std::ostringstream tErr;
        tErr << "out of bounds in String_Split::at(" << id << ") in " << __FILE__ << ":" << __LINE__ << " The line is: " << *this << "\n";
        throw std::range_error(tErr.str());
    }
}

char *String_Split::getNewBuffer(long &sizeP)
{
    sizeP = buf_size;
    // for(long i=0; i<buf_size; i++)buf[i]='+';
    // buf[buf_size - 1] = 0x0;
    *buf = 0x0;
    ptr_head = 0; // reset tokenized data
    return buf;
}

char *String_Split::getAdditionBuffer(long &sizeToAdd)
{
    char *addHere = 0;
    char *_buf = static_cast<char *>(peat_malloc_aligned(buf_size * 2 * sizeof(char), 64));
    memcpy(_buf, buf, buf_size);
    peat_free(buf);
    buf = _buf;
    addHere = buf + buf_size - 1; // fgets places 0 at the end of the string
    sizeToAdd = buf_size + 1;
    buf_size = buf_size * 2;
    // buf[buf_size - 1] = 0x0;
    return addHere;
}

void String_Split::push_back(char *elem)
{
    if (!(ptr_head < tokens_ptr_max_size)) {
        char **_ptr = static_cast<char **>(peat_malloc_aligned(tokens_ptr_max_size * 2 * sizeof(char *), 64)); // double the pointers storage
        memcpy(_ptr, tokens_ptr, tokens_ptr_max_size * sizeof(char *));
        peat_free(tokens_ptr);
        tokens_ptr = _ptr;
        tokens_ptr_max_size = tokens_ptr_max_size * 2;
    }
    tokens_ptr[ptr_head] = elem;
    ptr_head++;
}

void splt_slow(const std::string &str, std::vector<std::string> &v, const char delim) // TODO should be deleted
{
    // doesn't care of Windows end of line symbol
    v.clear();
    if (str.empty()) {
        return;
    }
    std::string::size_type begLine = 0;
    std::string::size_type endLine = 0;
    std::string dstr(1, delim);
    // const char winEndOfLine=13;
    do {
        endLine = str.find(dstr, begLine);
        std::string newLex(str, begLine, endLine - begLine);
        v.push_back(newLex);
        begLine = endLine + 1;
    } while (endLine != std::string::npos);
}

std::map<PEAT_Rank, std::string> get_file_names(const std::string &dir, const std::string &beginName, const std::string &fileExtension, PEAT_Bool throw_exeptions)
{
#if !defined(_WIN32) || defined(__MINGW32__)
    DIR *curDir = 0;
    struct dirent *cur_file = 0;
    char pathBuf[PATH_MAX + 1];
    std::map<PEAT_Rank, std::string> fileset;

    curDir = opendir(dir.c_str());
    if (!curDir) {
        std::ostringstream tmp;
        tmp << "Error getting directory info in " << dir << "\n";
        throw std::runtime_error(tmp.str());
    }
    while ((cur_file = readdir(curDir))) {
        if (strlen(cur_file->d_name) > beginName.size()) {
            if (!strncmp(cur_file->d_name, beginName.c_str(), beginName.size())) {
                const std::string curFile = cur_file->d_name;
                std::size_t foundExt = curFile.find(fileExtension);
                if (std::string::npos != foundExt) {
                    std::string::size_type foundPos = 0;
                    while (!isdigit(curFile[foundPos])) {
                        foundPos = curFile.find_first_of(".", foundPos);
                        if (foundPos == std::string::npos) {
                            break;
                        }
                        ++foundPos;
                    }
                    if (foundPos == std::string::npos) {
                        continue;
                    }
                    // curFile = curFile.substr(0, foundPos); //cut last ".00"
                    // foundPos = curFile.find_last_of(".");
                    // string curfile_name = curFile.substr(0, foundPos); //cut last ".12"
                    // string curFileRank = curFile.substr(foundPos + 1); //get last "12" as rank number
                    int rank = 0;
                    extract(rank, &curFile[foundPos]); // convert rank into numeric

                    // cout << "DEBUG123 "<< cur_file->d_name << "_" << rank << endl;
#if defined(__MINGW32__)
                    char *resultPath = _fullpath(pathBuf, (dir + DIR_DELIMITER + curFile).c_str(), PATH_MAX);
#else
                    char *resultPath = realpath((dir + DIR_DELIMITER + curFile).c_str(), pathBuf);
#endif
                    if (!resultPath) {
                        std::ostringstream tmp;
                        tmp << "Can't convert file '" << curFile << "' to canonical path in ' " << dir << "\n";
                        throw std::runtime_error(tmp.str());
                    }
                    fileset[rank] = resultPath;
                }
            }
        }
    }
    if (fileset.empty() && throw_exeptions) {
        closedir(curDir);
        char ctmp[1024];
        size_t stmp = 1024;
        std::ostringstream tmp;
        tmp << "No files found given by template (\"" << beginName << "\") in " << getcwd(ctmp, stmp) << "\n";
        throw std::runtime_error(tmp.str());
    }
    closedir(curDir);
    return fileset;
#else
    WIN32_FIND_DATA FindData;
    HANDLE hfnd = INVALID_HANDLE_VALUE;
    std::map<PEAT_Rank, std::string> fileset;
    char fullname[MAX_PATH];

    sprintf_s(fullname, "%s\\%s.*", dir.c_str(), beginName.c_str());
    hfnd = FindFirstFile((LPCSTR)fullname, &FindData);
    if (hfnd == INVALID_HANDLE_VALUE) {
        std::ostringstream tmp;
        tmp << "Error getting directory info in " << dir << ". Error code " << GetLastError() << std::endl;
        throw std::runtime_error(tmp.str());
    }
    do {
        if (FindData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
            continue; /* skip directories */
        size_t len1 = strlen((const char *)FindData.cFileName);
        size_t len2 = beginName.length();
        if (len1 >= len2) {
            if (!strncmp((const char *)FindData.cFileName, beginName.c_str(), beginName.length())) {
                std::string curFile = (const char *)FindData.cFileName;
                // std::string::size_type foundPos = curFile.find_last_of(".");

                size_t foundExt = curFile.find(fileExtension);
                if (std::string::npos != foundExt) {
                    std::string::size_type foundPos = 0;
                    while (!isdigit(curFile[foundPos])) {
                        foundPos = curFile.find_first_of(".", foundPos);
                        if (foundPos == std::string::npos) {
                            break;
                        }
                        ++foundPos;
                    }
                    if (foundPos == std::string::npos) {
                        if (fileExtension.length() == 0) {
                            std::string file = (char *)FindData.cFileName;
                            std::string full_path = dir + DIR_DELIMITER + file;
                            fileset[0] = full_path.c_str();
                        } else {
                            continue;
                        }
                    } else {
                        char *endptr;
                        int rank = 0;
                        extract(rank, &curFile[foundPos], &endptr); // convert rank into numeric
                        std::string file = (char *)FindData.cFileName;
                        std::string full_path = dir + DIR_DELIMITER + file;
                        fileset[rank] = full_path.c_str();
                    }
                }
            }
        }
    } while (FindNextFile(hfnd, &FindData) != 0);
    FindClose(hfnd);

    if (fileset.empty() && throw_exeptions) {
        std::ostringstream tmp;
        tmp << "No files found given by template (\"" << beginName << "\") in " << dir << std::endl;
        throw std::runtime_error(tmp.str());
    }
    return fileset;
#endif
}

std::map<PEAT_Rank, std::string> get_file_names(const std::string &dir, const std::string &beginName, const std::string &fileExtension, const std::set<PEAT_Rank> &rank_set, PEAT_Bool throw_exeptions)
{
    if (rank_set.empty())
        return get_file_names(dir, beginName, fileExtension, throw_exeptions);
#if !defined(_WIN32) || defined(__MINGW32__)
    DIR *curDir = 0;
    struct dirent *cur_file = 0;
    char pathBuf[PATH_MAX + 1];
    std::map<PEAT_Rank, std::string> fileset;

    curDir = opendir(dir.c_str());
    if (!curDir) {
        std::ostringstream tmp;
        tmp << "Error getting directory info in " << dir << "\n";
        throw std::runtime_error(tmp.str());
    }
    while ((cur_file = readdir(curDir))) {
        if (strlen(cur_file->d_name) > beginName.size()) {
            if (!strncmp(cur_file->d_name, beginName.c_str(), beginName.size())) {
                const std::string curFile = cur_file->d_name;
                std::size_t foundExt = curFile.find(fileExtension);
                if (std::string::npos != foundExt) {
                    std::string::size_type foundPos = 0;
                    while (!isdigit(curFile[foundPos])) {
                        foundPos = curFile.find_first_of(".", foundPos);
                        if (foundPos == std::string::npos) {
                            break;
                        }
                        ++foundPos;
                    }
                    if (foundPos == std::string::npos) {
                        continue;
                    }
                    // curFile = curFile.substr(0, foundPos); //cut last ".00"
                    // foundPos = curFile.find_last_of(".");
                    // string curfile_name = curFile.substr(0, foundPos); //cut last ".12"
                    // string curFileRank = curFile.substr(foundPos + 1); //get last "12" as rank number
                    int rank = 0;
                    extract(rank, &curFile[foundPos]); // convert rank into numeric

                    // cout << "DEBUG123 "<< cur_file->d_name << "_" << rank << endl;
#if defined(__MINGW32__)
                    char *resultPath = _fullpath(pathBuf, (dir + DIR_DELIMITER + curFile).c_str(), PATH_MAX);
#else
                    char *resultPath = realpath((dir + DIR_DELIMITER + curFile).c_str(), pathBuf);
#endif
                    if (!resultPath) {
                        std::ostringstream tmp;
                        tmp << "Can't convert file '" << curFile << "' to canonical path in ' " << dir << "\n";
                        throw std::runtime_error(tmp.str());
                    }
                    if (rank_set.count(rank))
                        fileset[rank] = resultPath;
                }
            }
        }
    }
    if (fileset.empty() && throw_exeptions) {
        closedir(curDir);
        char ctmp[1024];
        size_t stmp = 1024;
        std::ostringstream tmp;
        tmp << "No files found given by template (\"" << beginName << "\") in " << getcwd(ctmp, stmp) << "\n";
        throw std::runtime_error(tmp.str());
    }
    closedir(curDir);
    return fileset;
#else
    WIN32_FIND_DATA FindData;
    HANDLE hfnd = INVALID_HANDLE_VALUE;
    std::map<PEAT_Rank, std::string> fileset;
    char fullname[MAX_PATH];

    sprintf_s(fullname, "%s\\%s.*", dir.c_str(), beginName.c_str());
    hfnd = FindFirstFile((LPCSTR)fullname, &FindData);
    if (hfnd == INVALID_HANDLE_VALUE) {
        std::ostringstream tmp;
        tmp << "Error getting directory info in " << dir << ". Error code " << GetLastError() << std::endl;
        throw std::runtime_error(tmp.str());
    }
    do {
        if (FindData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
            continue; /* skip directories */
        size_t len1 = strlen((const char *)FindData.cFileName);
        size_t len2 = beginName.length();
        if (len1 >= len2) {
            if (!strncmp((const char *)FindData.cFileName, beginName.c_str(), beginName.length())) {
                std::string curFile = (const char *)FindData.cFileName;
                // std::string::size_type foundPos = curFile.find_last_of(".");

                size_t foundExt = curFile.find(fileExtension);
                if (std::string::npos != foundExt) {
                    std::string::size_type foundPos = 0;
                    while (!isdigit(curFile[foundPos])) {
                        foundPos = curFile.find_first_of(".", foundPos);
                        if (foundPos == std::string::npos) {
                            break;
                        }
                        ++foundPos;
                    }
                    if (foundPos == std::string::npos) {
                        if (fileExtension.length() == 0) {
                            std::string file = (char *)FindData.cFileName;
                            std::string full_path = dir + DIR_DELIMITER + file;
                            fileset[0] = full_path.c_str();
                        } else {
                            continue;
                        }
                    } else {
                        char *endptr;
                        int rank = 0;
                        extract(rank, &curFile[foundPos], &endptr); // convert rank into numeric
                        std::string file = (char *)FindData.cFileName;
                        std::string full_path = dir + DIR_DELIMITER + file;
                        if (rank_set.count(rank))
                            fileset[rank] = full_path.c_str();
                    }
                }
            }
        }
    } while (FindNextFile(hfnd, &FindData) != 0);
    FindClose(hfnd);

    if (fileset.empty() && throw_exeptions) {
        std::ostringstream tmp;
        tmp << "No files found given by template (\"" << beginName << "\") in " << dir << std::endl;
        throw std::runtime_error(tmp.str());
    }
    return fileset;
#endif
}

PEAT_Bool just_drive(std::string &dir)
{
#if !defined(_WIN32) || defined(__MINGW32__)
    return false;
#else
    size_t len = dir.length();
    size_t pos = dir.find_last_of(':');
    if (pos == len - 1)
        return true;
    return false;
#endif
}
PEAT_Bool peat_mkdir(std::string fullPath, int verbose)
{
    size_t pre = 0, pos = 0;
    std::string dir;
    int result = 0;
    LOCOUT << " Request of Creating  " << fullPath << '\n';

    if (*fullPath.rbegin() != DIR_DELIMITER) {
        fullPath += DIR_DELIMITER;
    }
    while ((pos = fullPath.find_first_of(DIR_DELIMITER, pre)) != std::string::npos) {
        dir = fullPath.substr(0, pos);
        ++pos;
        pre = pos;
        if (dir.empty() || just_drive(dir)) {
            continue; // if leading '/' first time is 0 length
        }
        LOCOUT << " Creating  " << dir << '\n';
#if !defined(_WIN32)
        result = mkdir(dir.c_str(), 0777);
#elif defined(__MINGW32__)
        result = mkdir(dir.c_str());
#else
        result = _mkdir(dir.c_str());
#endif
        if (EEXIST == errno) {
            LOCOUT << dir << " exists " << result << " errno " << errno << '\n';
            result = 0;
        } else {
            LOCERR << dir << " result " << result << " errno " << errno << strerror(errno) << '\n';
        }
        if (result) {
            return result;
        }
    }
    return result;
}
#include <time.h>
double Wtime(void)
{
    double t_clock = clock();
    t_clock /= CLOCKS_PER_SEC;
    return t_clock;
}

char *peat_strupr(char *str)
{
    while (*str)
        *str++ = toupper(*str);
    return str;
}

#if defined(_WIN32) && !defined(__MINGW32__)
#include <Shlwapi.h>
#pragma comment(lib, "Shlwapi.lib")
#else
#include <sstream>
#include <vector>
#endif

std::string peat_get_relative_path(const std::string &from, const std::string &to)
{
#if defined(_WIN32) && !defined(__MINGW32__)
    char result[MAX_PATH];
    DWORD from_attr = PathIsDirectoryA(from.c_str()) ? FILE_ATTRIBUTE_DIRECTORY : FILE_ATTRIBUTE_NORMAL;
    DWORD to_attr = PathIsDirectoryA(to.c_str()) ? FILE_ATTRIBUTE_DIRECTORY : FILE_ATTRIBUTE_NORMAL;
    if (!PathRelativePathToA(result, from.c_str(), from_attr, to.c_str(), to_attr)) {
        return to;
    }
    return std::string(result);
#else
    std::vector<std::string> from_parts;
    std::vector<std::string> to_parts;
    {
        std::stringstream ss(from);
        std::string item;
        while (std::getline(ss, item, '/')) {
            if (!item.empty()) {
                from_parts.push_back(item);
            }
        }
    }
    {
        std::stringstream ss(to);
        std::string item;
        while (std::getline(ss, item, '/')) {
            if (!item.empty()) {
                to_parts.push_back(item);
            }
        }
    }

    size_t common = 0;
    size_t max_common = std::min(from_parts.size(), to_parts.size());
    while (common < max_common && from_parts[common] == to_parts[common]) {
        ++common;
    }

    // from/to are both directories, so every part beyond the common prefix needs a "../"
    size_t up_steps = from_parts.size() > common ? from_parts.size() - common : 0;
    std::vector<std::string> result_parts(up_steps, "..");
    result_parts.insert(result_parts.end(), to_parts.begin() + common, to_parts.end());

    std::string result;
    for (size_t i = 0; i < result_parts.size(); ++i) {
        if (i != 0) {
            result += "/";
        }
        result += result_parts[i];
    }
    // printf("Calculating related path bitween %s and %s. Result is %s \n",from.c_str(),to.c_str(),result.c_str());

    if (result.empty()) {
        result = ".";
    }
    return result;
#endif
}
