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

#ifndef __UTILS_H__
#define __UTILS_H__

#include <iostream>
#include <list>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <typeinfo>
#include <vector>

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#if defined(_WIN32) && !defined(__MINGW32__)
#include <direct.h>
#define _SC_PAGESIZE 4096
#define sysconf(x) (x)
#define __max__(x, y) __max(x, y)
#define __min__(x, y) __min(x, y)
#else
#include <unistd.h> //_SC_PAGESIZE
#define __max__(a, b) (((a) > (b)) ? (a) : (b))
#define __min__(a, b) (((a) < (b)) ? (a) : (b))
#endif

#include "common_plain_types.h"
// extern int verbose;

std::map<PEAT_Rank, std::string> get_file_names(const std::string &dir, const std::string &beginName, const std::string &fileExtension, bool throw_exeptions);
std::map<PEAT_Rank, std::string> get_file_names(const std::string &dir, const std::string &beginName, const std::string &fileExtension, const std::set<PEAT_Rank> &rank_set, bool throw_exeptions);

#ifndef _SC_PAGESIZE
#define _SC_PAGESIZE 4096
#define sysconf(x) _SC_PAGESIZE
#endif
inline void *peat_malloc_aligned(const size_t size, const size_t alignment = sysconf(_SC_PAGESIZE))
{
    void *chunk = 0;
#if defined(__MINGW32__)
    chunk = __mingw_aligned_malloc(size, alignment);
#elif defined(_WIN32)
    chunk = _aligned_malloc(size, alignment);
#else
    if (posix_memalign(&chunk, alignment, size))
        return 0;
#endif

    return chunk;
}
inline void peat_free(void *ptr)
{
#if defined(__MINGW32__)
    __mingw_aligned_free(ptr);
#elif defined(_WIN32)
    _aligned_free(ptr);
#else
    free(ptr);
#endif
}

class String_Split
{
  public:
    explicit String_Split(int verbose = 0);
    virtual ~String_Split();
    long split(char delimeter, char terminator = 0); // return number of split parts
    const char *at(PEAT_Num id) const;               // throw exception if no part corresponding to id
    char *getNewBuffer(long &size);                  // returns buffer to place the string with size bytes
    char *getAdditionBuffer(long &sizeToAdd);        // returns pointer to additional spece in buffer to place the string with sizeToAdd bytes.
    void clear()
    {
        ptr_head = 0;
        plain_data_size = 0;
        buf[0] = 0;
        tokens_ptr[0] = buf;
    }
    const char *data() const
    {
        return buf;
    }
    long dataSize() const
    {
        return plain_data_size;
    }
    PEAT_Bool empty() const
    {
        return (ptr_head == 0);
    }
    unsigned long size() const
    {
        return ptr_head;
    }
    const char *back()
    {
        if (empty()) {
            return 0;
        } else {
            return tokens_ptr[ptr_head - 1];
        }
    }
    const char *front()
    {
        if (empty()) {
            return 0;
        } else {
            return tokens_ptr[0];
        }
    }
    static char getWinEOL()
    {
        return 13;
    }
    static char get_eventOL()
    {
        return 10;
    }
    String_Split &operator=(const String_Split &from);
    friend std::ostream &operator<<(std::ostream &dbg, const String_Split &data);
#if 0
#if defined(_WIN32) && !defined(__MINGW32__)
    static void* operator new(size_t size)
    {
        return concurrency::Alloc(size);
    }
    static void operator delete(void* p)
    {
        concurrency::Free(p);
    }
#endif
#endif

  private:
    void push_back(char *elem);
    char *buf;
    long buf_size;
    long plain_data_size; // size between first string begin and last string end. Used to copy all data in the class.
    char **tokens_ptr;
    unsigned long ptr_head;
    unsigned long tokens_ptr_max_size;
    int verbose;
};

void splt_slow(std::string const &inputString, std::vector<std::string> &outputVector, const char delim);

#define CHECKERRNO                                                                                                                                                          \
    if (errno == ERANGE || errno != 0) {                                                                                                                                    \
        std::ostringstream tmp;                                                                                                                                             \
        tmp << "Failed to extract numeric value from the string {" << str << "} Result " << val << ". Errno " << errno << ". File " << __FILE__ << ":" << __LINE__ << "\n"; \
        throw std::invalid_argument(tmp.str());                                                                                                                             \
    }
#define CHECKENDPTR                                                                                                 \
    if (endptr == str && check) {                                                                                   \
        std::ostringstream tmp;                                                                                     \
        tmp << "No digits were found in the string \"" << str << "\" File " << __FILE__ << ":" << __LINE__ << "\n"; \
        throw std::invalid_argument(tmp.str());                                                                     \
    }
#define CHECKENDPTR2                                                                                                \
    if (endptr && endptr[0] != 0 && endptr[0] != ' ' && endptr[0] != ',' && endptr[0] != '.' && check) {            \
        std::ostringstream tmp;                                                                                     \
        tmp << "No digits were found in the string \"" << str << "\" File " << __FILE__ << ":" << __LINE__ << "\n"; \
        throw std::invalid_argument(tmp.str());                                                                     \
    }
/*
inline void extract(long long &x, const char *p)//fast extract
{
    while(' ' == *p) {//skip prefix spaces
        ++p;
    }
    x = 0;
    PEAT_Bool neg = false;
    if (*p == '-') {
        neg = true;
        ++p;
    }
    while (*p >= '0' && *p <= '9') {
        x = (x*10) + (*p - '0');
        ++p;
    }
    if (neg) {
        x = -x;
    }
}

inline void extract(unsigned long long &x, const char *p)//fast extract
{
    while(' ' == *p) {//skip prefix spaces
        ++p;
    }
    x = 0;
    while (*p >= '0' && *p <= '9') {
        x = (x*10) + (*p - '0');
        ++p;
    }
}

inline void extract(void* &pt, const char *p)//fast extract
{
    extract((unsigned long long&) pt,p);
}
*/
inline void extract(unsigned long long &val, const char *str, const int base = 10, const PEAT_Bool check = true)
{
    errno = 0;
    char *endptr = 0;
    val = strtoull(str, &endptr, base);
    CHECKERRNO;
    CHECKENDPTR;
    CHECKENDPTR2;
}

inline void extract(void *&val, const char *str, const int base = 10, const PEAT_Bool check = true)
{
    errno = 0;
    char *endptr = 0;
    val = (void *)strtoull(str, &endptr, base);
    CHECKERRNO;
    CHECKENDPTR;
    CHECKENDPTR2;
}

inline void extract(long long &val, const char *str, const int base = 10, const PEAT_Bool check = true)
{
    errno = 0;
    char *endptr = 0;
    val = strtoll(str, &endptr, base);
    CHECKERRNO;
    CHECKENDPTR;
    CHECKENDPTR2;
}
inline void extract(long long &val, const char *str, char **endptr, const int base = 10)
{
    errno = 0;
    val = strtoll(str, endptr, base);
    CHECKERRNO;
}

inline void extract(unsigned int &val, const char *str, const int base = 10, const PEAT_Bool check = true)
{
    errno = 0;
    char *endptr = 0;
    val = static_cast<unsigned int>(strtoul(str, &endptr, base));
    CHECKERRNO;
    CHECKENDPTR;
    CHECKENDPTR2;
}

inline void extract(int &val, const char *str, const int base = 10, const PEAT_Bool check = true)
{
    errno = 0;
    char *endptr = 0;
    if (base == 16) {
        val = static_cast<int>(strtoul(str, &endptr, 16));
    } else {
        val = static_cast<int>(strtol(str, &endptr, base));
    }
    CHECKERRNO;
    CHECKENDPTR;
    CHECKENDPTR2;
}
inline void extract(int &val, const char *str, char **endptr, const int base = 10)
{
    errno = 0;
    if (base == 16) {
        val = static_cast<int>(strtoul(str, endptr, 16));
    } else {
        val = static_cast<int>(strtol(str, endptr, base));
    }
    CHECKERRNO;
}

inline void extract(PEAT_MPI_Func_ID &val, const char *str, const int base = 10, const PEAT_Bool check = true)
{
    errno = 0;
    char *endptr = 0;
    if (base == 16) {
        val = static_cast<PEAT_MPI_Func_ID>(strtoul(str, &endptr, 16));
    } else {
        val = static_cast<PEAT_MPI_Func_ID>(strtol(str, &endptr, base));
    }
    CHECKERRNO;
    CHECKENDPTR;
    CHECKENDPTR2;
}

inline void extract(PEAT_Record_Type &val, const char *str, const int base = 10, const PEAT_Bool check = true)
{
    errno = 0;
    char *endptr = 0;
    if (base == 16) {
        val = static_cast<PEAT_Record_Type>(strtoul(str, &endptr, 16));
    } else {
        val = static_cast<PEAT_Record_Type>(strtol(str, &endptr, base));
    }
    CHECKERRNO;
    CHECKENDPTR;
    CHECKENDPTR2;
}

inline void extract(long &val, const char *str, const int base = 10, const PEAT_Bool check = true)
{
    errno = 0;
    char *endptr = 0;
    val = strtol(str, &endptr, base);
    CHECKERRNO;
    CHECKENDPTR;
    CHECKENDPTR2;
}

inline void extract(unsigned long &val, const char *str, const int base = 10, const PEAT_Bool check = true)
{
    errno = 0;
    char *endptr = 0;
    val = strtoul(str, &endptr, base);
    CHECKERRNO;
    CHECKENDPTR;
    CHECKENDPTR2;
}

inline void extract(double &val, const char *str, const PEAT_Bool check = true)
{
    errno = 0;
    char *endptr = 0;
    val = strtod(str, &endptr);
    CHECKERRNO;
    CHECKENDPTR;
    CHECKENDPTR2;
}
inline void extract(double &val, const char *str, char **endptr)
{
    errno = 0;
    val = strtod(str, endptr);
    CHECKERRNO;
}

// Don't delete for some time
#if 0
template <typename T> inline void extract(T &val, const char *str, const int base = 10)
{
    errno = 0;
    char *endptr = 0;
    if(typeid(T) == typeid(long)) {
        val = strtol(str, &endptr, base);
    } else if(typeid(T) == typeid(unsigned long)) {
        val = strtoul(str, &endptr, base);
    } else if(typeid(T) == typeid(unsigned int)) {
        val = static_cast<unsigned int>(strtoul(str, &endptr, base));
    } else if(typeid(T) == typeid(int)) {
        if(base == 16) {
            val = static_cast<int>(strtoul(str, &endptr, base));
        } else {
            val = static_cast<int>(strtol(str, &endptr, base));
        }
    } else if(typeid(T) == typeid(long long)) {
        val = strtoll(str, &endptr, base);
    } else if(typeid(T) == typeid(unsigned long long)) {
        val = strtoull(str, &endptr, base);
    } else if(typeid(T) == typeid(double)) {
        val = strtod(str, &endptr);
    } else {
        std::strcerr << "Undefined type " << typeid(T).name() << " to extract from the string {" << str << "}. File " << __FILE__ << ":" << __LINE__ << "\n";
    }
    CHECKERRNO
    CHECKENDPTR
}
#endif

template <typename K, typename T>
std::ostream &operator<<(std::ostream &dbg, const std::map<K, T> &req)
{
    std::string delim = "";
    dbg << "{";
    for (typename std::map<K, T>::const_iterator it = req.begin(); it != req.end(); ++it) {
        dbg << delim << "Key:" << it->first << " Value:" << it->second;
        delim = ", ";
    }
    dbg << "}";
    return dbg;
}

template <typename K, typename T>
std::ostream &operator<<(std::ostream &dbg, const std::multimap<K, T> &req)
{
    std::string delim = "";
    dbg << "{";
    for (typename std::multimap<K, T>::const_iterator it = req.begin(); it != req.end(); ++it) {
        dbg << delim << "Key:" << it->first << " Value:" << it->second;
        delim = ", ";
    }
    dbg << "}";
    return dbg;
}

template <typename T>
std::ostream &operator<<(std::ostream &dbg, const std::list<T> &rec)
{
    long listNumber = 0;
    dbg << " elements(" << rec.size() << "){";
    for (typename std::list<T>::const_iterator it = rec.begin(); it != rec.end(); ++it) {
        dbg << "[" << listNumber << "]={" << *it << "}";
        listNumber++;
    }
    dbg << "}";
    return dbg;
}

template <typename T>
std::ostream &operator<<(std::ostream &dbg, std::list<T> &rec)
{
    long listNumber = 0;
    dbg << "(" << rec.size() << ") elements: {";
    for (typename std::list<T>::iterator it = rec.begin(); it != rec.end(); ++it) {
        dbg << "\n[" << listNumber << "]={" << *it << "}";
        listNumber++;
    }
    dbg << "}";
    return dbg;
}

template <typename T>
std::ostream &operator<<(std::ostream &dbg, const std::set<T> &rec)
{
    long listNumber = 0;
    dbg << " elements(" << rec.size() << "){";
    for (typename std::set<T>::const_iterator it = rec.begin(); it != rec.end(); ++it) {
        dbg << "[" << listNumber << "]={" << *it << "}";
        listNumber++;
    }
    dbg << "}";
    return dbg;
}

template <typename T>
std::ostream &operator<<(std::ostream &dbg, std::set<T> &rec)
{
    long listNumber = 0;
    dbg << "(" << rec.size() << ") elements: {";
    for (typename std::set<T>::iterator it = rec.begin(); it != rec.end(); ++it) {
        dbg << "\n[" << listNumber << "]={" << *it << "}";
        listNumber++;
    }
    dbg << "}";
    return dbg;
}

template <typename T>
std::ostream &operator<<(std::ostream &dbg, const std::vector<T> &data)
{
    std::string delim = "";
    dbg << "{";
    for (typename std::vector<T>::const_iterator i = data.begin(); i != data.end(); ++i) {
        dbg << delim << "{" << *i << "}";
        delim = ",";
    }
    dbg << "}";
    return dbg;
}
template <typename K, typename T>
PEAT_Num pack_pod_size(const std::map<K, T> &obj) // types K and T must be Plain Old Data map<int,int>
{
    return (obj.size() * (sizeof(K) + sizeof(T))) + sizeof(PEAT_Num);
}

template <typename K, typename T>
PEAT_Num pack_pod(char *ptr, const std::map<K, T> &obj) // types K and T must be Plain Old Data map<int,int>
{
    PEAT_Num sizeK = sizeof(K), sizeT = sizeof(T), ssize = 0, ptr_it = 0;
    const PEAT_Num elemsSize = obj.size();
    ssize = sizeof(elemsSize);
    memcpy(ptr + ptr_it, &elemsSize, ssize);
    ptr_it += ssize; // put the number of elements of std::map
    for (typename std::map<K, T>::const_iterator it = obj.begin(); it != obj.end(); ++it) {
        memcpy(ptr + ptr_it, &(it->first), sizeK);
        ptr_it += sizeK;
        memcpy(ptr + ptr_it, &(it->second), sizeT);
        ptr_it += sizeT;
    }
    return ptr_it;
}

template <typename K, typename T>
PEAT_Num unpack_pod(const char *ptr, std::map<K, T> &obj) // types K and T must be Plain Old Data map<int,int>
{
    PEAT_Num sizeK = sizeof(K), sizeT = sizeof(T), ssize = 0, ptr_it = 0;
    PEAT_Num elemsSize = 0;
    ssize = sizeof(elemsSize);
    memcpy(&elemsSize, ptr + ptr_it, ssize);
    ptr_it += ssize; // get the number of elements of std::vector
    while (elemsSize) {
        K key = (K)0;
        T value = (T)0;
        std::pair<typename std::map<K, T>::iterator, PEAT_Bool> ret;
        memcpy(&key, ptr + ptr_it, sizeK);
        ptr_it += sizeK;
        memcpy(&value, ptr + ptr_it, sizeT);
        ptr_it += sizeT;
        ret = obj.insert(std::pair<K, T>(key, value));
        if (ret.second == false) {
            std::cerr << "Failed to unpack data in unpack_pod(std::map). Key " << key << " Value " << value << "\n";
        }
        // else {
        //     std::cerr << "Unpacked data in unpack_pod(std::map). Key " << key << " Value " << value << "\n";
        // }
        --elemsSize;
    }
    return ptr_it;
}

template <typename T>
PEAT_Num pack_pod_size(const std::vector<T> &obj) // type T must be Plain Old Data vector<int>
{
    return obj.size() * sizeof(T) + sizeof(PEAT_Num);
}

template <typename T>
PEAT_Num pack_pod(char *ptr, const std::vector<T> &obj) // type T must be Plain Old Data vector<int>
{
    PEAT_Num sizeT = sizeof(T), ssize = 0, ptr_it = 0;
    const PEAT_Num elemsSize = obj.size();
    ssize = sizeof(elemsSize);
    memcpy(ptr + ptr_it, &elemsSize, ssize);
    ptr_it += ssize; // put the number of elements of std::vector
    ssize = elemsSize * sizeT;
    memcpy(ptr + ptr_it, obj.data(), ssize);
    ptr_it += ssize;
    return ptr_it;
}

template <typename T>
PEAT_Num unpack_pod(const char *ptr, std::vector<T> &obj) // type T must be Plain Old Data vector<int>
{
    PEAT_Num sizeT = sizeof(T), ssize = 0, ptr_it = 0;
    PEAT_Num elemsSize = 0;
    ssize = sizeof(elemsSize);
    memcpy(&elemsSize, ptr + ptr_it, ssize);
    ptr_it += ssize; // get the number of elements of std::vector
    obj.clear();
    if (elemsSize > 0) {
        obj.reserve(elemsSize);
        for (PEAT_Num i = 0; i < elemsSize; ++i) {
            T tmp;
            memcpy(&tmp, ptr + ptr_it, sizeT);
            ptr_it += sizeT;
            obj.push_back(tmp);
        }
    }
    return ptr_it;
}

inline std::string trim(const std::string &str, const std::string &whitespace = " \t")
{
    const std::size_t strBegin = str.find_first_not_of(whitespace);
    if (strBegin == std::string::npos) {
        return ""; // no content
    }
    const std::size_t strEnd = str.find_last_not_of(whitespace);
    const std::size_t strRange = strEnd - strBegin + 1;

    return str.substr(strBegin, strRange);
}

// #pragma weak pet_throw_error=c_pet_throw_error
PEAT_Bool peat_mkdir(std::string fullPath, int verbose); // creates multy-sub-directories

std::string peat_get_relative_path(const std::string &from, const std::string &to);

char *peat_strupr(char *);
#ifdef PEAT_MPI
double Wtime(void);
// #define Wtime MPI_Wtime
#define __is_statement_printable__(x) (0 == x.comm_rank)
#else
#define __is_statement_printable__(x) (1)
double Wtime(void);
#endif
#endif // __UTILS_H__
