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

#ifndef __COMMON_TYPES_H__
#define __COMMON_TYPES_H__
#ifdef PEAT_MPI
#include "mpi.h"
#endif

#ifdef _OPENMP
#define PEAT_OMP
#include "omp.h"
#include <mutex>
#endif

#include <assert.h>
#include <climits>
#include <float.h> //DBL_MAX
#include <fstream>
#include <iostream>
#include <list>
#include <map>
#include <string>
#include <vector>

#include "common_plain_types.h"
#include "file_io.h"
#include "func_id.h"
#include "utils.h"

enum PET_Transport_Selector {
    PET_Transport_Selector_MIN,
    PET_Transport_Selector_AVG,
    PET_Transport_Selector_MEDIAN,
    PET_Transport_Selector_MAX
};
enum PET_Part_Receiver {
    PET_Part_Receiver_PROLOG,
    PET_Part_Receiver_MEAT,
    PET_Part_Receiver_EPILOG,
    PET_Part_Receiver_MAX
};

enum QName {
    qt_send,
    qt_recv,
    qt_size
};
enum QName2 {
    qt_pers,
    qt_hanged,
    qt_normal,
    qt_size2,
    qt_thrown,
    qt_pers_thrown,
};

enum PEAT_Transports {
    PEAT_TRANSPORT_DDR = 0,
    PEAT_TRANSPORT_HBM,
    PEAT_TRANSPORT_QPI,
    PEAT_TRANSPORT_NIC,
    PEAT_TRANSPORT_COLLECTIVE,
    PEAT_TRANSPORT_MAX
};

struct PEAT_MPI_Time {
    PEAT_Clock clocks = 0;
    std::string percentage;
};

struct PEAT_Node_Desc {
    PEAT_Node number = 0;
    PEAT_FREQ freq = 0;
    PEAT_Clock clocks = 0;
    std::string name;
    std::vector<PEAT_Rank> ranks;
};

void peat_throw_error(const char *str);
void peat_warning(const char *str);

struct PEAT_Func_ID_Pair {
    PEAT_MPI_Func_ID id, parent_id;
    PEAT_Func_ID_Pair(PEAT_MPI_Func_ID _id, PEAT_MPI_Func_ID _parent_id) : id(_id), parent_id(_parent_id) {};
    PEAT_Func_ID_Pair() : id(TMPI_FUNC_ID_NONE), parent_id(TMPI_FUNC_ID_NONE) {};
    bool operator<(const PEAT_Func_ID_Pair &r) const
    {
        if (id < r.id)
            return true;
        if (id > r.id)
            return false;
        if (parent_id < r.parent_id)
            return true;
        if (parent_id > r.parent_id)
            return false;
        return false;
    }
};

struct PEAT_Rank_Pair {
    PEAT_Rank from, to, max_rank;
    PEAT_Rank_Pair(PEAT_Rank _from, PEAT_Rank _to, PEAT_Rank _max_rank) : from(_from), to(_to), max_rank(_max_rank) {};
    PEAT_Rank_Pair(PEAT_Rank _max_rank) : from(-1), to(-1), max_rank(_max_rank) {};
    PEAT_Rank_Pair() : from(-1), to(-1), max_rank(-1) {};
    bool operator<(const PEAT_Rank_Pair &r) const
    {
        if (from < r.from)
            return true;
        if (from > r.from)
            return false;
        if (to < r.to)
            return true;
        if (to > r.to)
            return false;
        return false;
    }
    operator PEAT_Rank() const
    {
        return to * max_rank + from;
    }
    PEAT_Rank max() const
    {
        return max_rank * max_rank;
    }
};

inline PEAT_Transports t_inc(PEAT_Transports s)
{
    int s_inc = (int)s;
    s_inc++;
    return (PEAT_Transports)s_inc;
}

struct PEAT_Time_Event {
    PEAT_Clock in;
    PEAT_Clock out;
    PEAT_Time_Event(PEAT_Clock init = -1) : in(init), out(init) {};
    PEAT_Clock duration() const
    {
        /*if ( out < in ) {
            peat_throw_error( "Time Event is negative\n" );
        };*/
        return out - in;
    }
    PEAT_Bool check() const
    {
        if (out < in) {
            peat_throw_error("Time Event is negative\n");
            return false;
        };
        if (out < 0) {
            peat_throw_error("Time Event (in) is negative\n");
            return false;
        };
        if (in < 0) {
            peat_throw_error("Time Event (in) is negative\n");
            return false;
        };

        return true;
    }

    PEAT_Num pack_size() const
    { // Not object size. Just size for data serialization.
        return sizeof(in) + sizeof(out);
    }
    PEAT_Num pack(char *ptr) const
    {
        PEAT_Num ssize = 0, ptr_it = 0;
        ssize = sizeof(in);
        memcpy(ptr + ptr_it, &in, ssize);
        ptr_it += ssize;
        ssize = sizeof(out);
        memcpy(ptr + ptr_it, &out, ssize);
        ptr_it += ssize;
        return ptr_it;
    }
    PEAT_Num unpack(const char *ptr)
    {
        PEAT_Num ssize = 0, ptr_it = 0;
        ssize = sizeof(in);
        memcpy(&in, ptr + ptr_it, ssize);
        ptr_it += ssize;
        ssize = sizeof(out);
        memcpy(&out, ptr + ptr_it, ssize);
        ptr_it += ssize;
        return ptr_it;
    }
    PEAT_Clock move(PEAT_Clock sub_t)
    {
        if (in > sub_t) {
            in -= sub_t;
            out -= sub_t;
            return sub_t;
        }
        if (out < 0) {
            peat_throw_error("Time Event (in) is negative\n");
        };
        if (in < 0) {
            peat_throw_error("Time Event (in) is negative\n");
        };
        return 0;
    }
    PEAT_Clock adjust(PEAT_Clock sub_t)
    {
        if (in > 0) {
            out -= sub_t;
            if (out < in) {
                peat_throw_error("Time Event is negative\n");
                return false;
            };
            return sub_t;
        }
        peat_throw_error("Time Event (in) is negative\n");
        return 0;
    }
    bool operator>(PEAT_Clock t)
    {
        if (in > t)
            return true;
        return false;
    }
};

struct PEAT_Time {
    PEAT_Time_Event func;
#ifndef PEAT_NO_MPI_TIME
    PEAT_Time_Event mpi;
#endif
#ifndef PEAT_NO_PE_TIME
    PEAT_Time_Event pe;
#endif
    PEAT_Num pack_size() const
    { // Not object size. Just size for data serialization.
        return func.pack_size()
#ifndef PEAT_NO_MPI_TIME
               + mpi.pack_size()
#endif
#ifndef PEAT_NO_PE_TIME
               + pe.pack_size()
#endif
            ;
    }
    PEAT_Num pack(char *ptr) const
    {
        PEAT_Num ssize = 0, ptr_it = 0;
        ssize = func.pack(ptr + ptr_it);
        ptr_it += ssize;
#ifndef PEAT_NO_MPI_TIME
        ssize = mpi.pack(ptr + ptr_it);
        ptr_it += ssize;
#endif
#ifndef PEAT_NO_PE_TIME
        ssize = pe.pack(ptr + ptr_it);
        ptr_it += ssize;
#endif
        return ptr_it;
    }
    PEAT_Num unpack(const char *ptr)
    {
        PEAT_Num ssize = 0, ptr_it = 0;
        ssize = func.unpack(ptr + ptr_it);
        ptr_it += ssize;
#ifndef PEAT_NO_MPI_TIME
        ssize = mpi.unpack(ptr + ptr_it);
        ptr_it += ssize;
#endif
#ifndef PEAT_NO_PE_TIME
        ssize = pe.unpack(ptr + ptr_it);
        ptr_it += ssize;
#endif
        return ptr_it;
    }
    PEAT_Clock move(PEAT_Clock sub_t)
    {
        func.move(sub_t);
#ifndef PEAT_NO_MPI_TIME
        mpi.move(sub_t);
#endif
#ifndef PEAT_NO_PE_TIME
        if (pe > 0)
            pe.move(sub_t);
#endif
        return sub_t;
    }
    PEAT_Clock adjust(PEAT_Clock sub_t)
    {
        func.adjust(sub_t);
#ifndef PEAT_NO_MPI_TIME
        mpi.adjust(sub_t);
#endif
#ifndef PEAT_NO_PE_TIME
        if (pe > 0)
            pe.adjust(sub_t);
#endif
        return sub_t;
    } // pe_entry should not be adjusted
};

struct PET_Time_Event {
    PEAT_Time e_time, o_time, t_time, ut_time;
#if 1 // def _DEBUG
    PEAT_Time raw_time;
    PEAT_Clock time_predictor;
    PET_Time_Event() : time_predictor(-1) {};
#endif
};

struct PEAT_Collective_Bytes {
    PEAT_Collective_Bytes() : is_vector(false) {}
    PEAT_Num empty() const
    {
        return bytes.empty();
    }
    void set_scalar(const PEAT_Message_Size size)
    {
        bytes[0] = size;
    }
    void set_vector(const PEAT_Message_Size size, const PEAT_Rank Rank)
    {
        bytes[Rank] = size;
        is_vector = true;
    }
    PEAT_Message_Size getAny() const
    {
        if (bytes.empty()) {
            return -1;
        }
        return bytes.begin()->second;
    }
    PEAT_Message_Size get(const PEAT_Rank rank) const
    {
        if (bytes.empty()) {
            return -1;
        }
        if (!is_vector) { // MPI_BCAST send same bytes to all. No need to keep all ranks
            return bytes.at(0);
        }
        if (bytes.count(rank)) {
            return bytes.at(rank);
        } else {
            return -1;
        }
    }
    PEAT_Message_Size get_total() const
    {
        if (bytes.empty()) {
            return 0;
        }
        PEAT_Message_Size total = 0;
        for (std::map<PEAT_Rank, PEAT_Message_Size>::const_iterator bytes_it = bytes.cbegin(); bytes_it != bytes.cend(); ++bytes_it) {
            total += bytes_it->second;
        }

        return total;
    }
    PEAT_Message_Size get_rank_bytes(PEAT_Rank rank) const
    {
        if (bytes.empty()) {
            return 0;
        }
        if (bytes.count(rank) == 1)
            return bytes.at(rank);
        return bytes.at(0);
    }

    PEAT_Num size() const { return (bytes.size()); }
    PEAT_Num pack_size() const
    { // Not object size. Just size for data serialization.
        return sizeof(is_vector) +
               pack_pod_size(bytes);
    }
    PEAT_Num pack(char *ptr) const
    {
        PEAT_Num ssize = 0, ptr_it = 0;
        ssize = sizeof(is_vector);
        memcpy(ptr + ptr_it, &is_vector, ssize);
        ptr_it += ssize;
        ptr_it += pack_pod(ptr + ptr_it, bytes);
        return ptr_it;
    }
    PEAT_Num unpack(const char *ptr)
    {
        PEAT_Num ssize = 0, ptr_it = 0;
        ssize = sizeof(is_vector);
        memcpy(&is_vector, ptr + ptr_it, ssize);
        ptr_it += ssize;
        ptr_it += unpack_pod(ptr + ptr_it, bytes);
        return ptr_it;
    }

    friend std::ostream &operator<<(std::ostream &stream, const PEAT_Collective_Bytes &me)
    {
        // stream << std::dec << me.bytes.size();
        if (me.bytes.size() < 1) {
            stream << "None\n";
            return stream;
        }
        if (me.bytes.size() == 1) {
            stream << std::dec << me.bytes.begin()->second << '\n';
            return stream;
        }
        stream << std::dec;
        for (std::map<PEAT_Rank, PEAT_Message_Size>::const_iterator it = me.bytes.begin(); it != me.bytes.end(); it++) {
            stream << " r:" << it->first << "s:" << it->second;
        }
        stream << '\n';
        return stream;
    }
    // private:
    std::map<PEAT_Rank, PEAT_Message_Size> bytes; // is_vector==False map<0, sendBytesToAllRanks>. is_vector==True map<rankReceiver, sendBytesToRankReceiver>
    PEAT_Bool operator<(const PEAT_Collective_Bytes &right) const
    {
        int size = (int)bytes.size();
        int rsize = (int)right.bytes.size();
        if (size < rsize)
            return true;
        if (size > rsize)
            return false;
        for (std::map<PEAT_Rank, PEAT_Message_Size>::const_iterator mess_it = bytes.begin(), r_mess_it = right.bytes.begin(); size > 0; ++mess_it, ++r_mess_it, --size) {
            if (mess_it->first < r_mess_it->first)
                return true;
            if (mess_it->first > r_mess_it->first)
                return false;
            if (mess_it->second < r_mess_it->second)
                return true;
            if (mess_it->second > r_mess_it->second)
                return false;
        }
        return false;
    }
    PEAT_Bool is_vector;
    bool operator==(const PEAT_Collective_Bytes s)
    {
        // light copmpare not always accurate but since it's purely for debug it's OK
        if (is_vector != s.is_vector)
            return false;
        return (get_total() == s.get_total());
    }
    void clear()
    {
        bytes.clear();
        is_vector = false;
    }
};

struct Outputs {
    File_Storage_GZ *output_MPI;
    File_Storage_GZ *output_HWC;
    File_Storage_GZ *output_bind;
    Outputs() : output_MPI(NULL), output_HWC(NULL), output_bind(NULL) {}
    void Initialize(File_Storage_GZ *MPI, File_Storage_GZ *HWC, File_Storage_GZ *bind)
    {
        output_MPI = MPI;
        output_HWC = HWC;
        output_bind = bind;
    };
};
#define GLOBAL_nic_EAGER_THRESHOLD 64000
#define GLOBAL_SMP_EAGER_THRESHOLD 16000
#define GLOBAL_SHORT_THRESHOLD 5120
#define GLOBAL_SHORT_THRESHOLD_INTRA 8
#define GLOBAL_SHORT_THRESHOLD_INTER 8
#define CLK_PER_NETMOD_CALL 0.0000008
#define CLK_PER_SHMEM_CALL 0.0000006
struct Global_Parameters {
    Global_Parameters() : model_output(0), pcontrol_mode(0), pcontrol_mode_rank(-1),
                          source_freq(0), target_freq(0), target_ppn(0), target_numa_pp(0), estimate_read(false), progress_engine(0), use_up(0), matching_only(0), skip_processing(0), print_location(0),
                          nic_eager_threshold(GLOBAL_nic_EAGER_THRESHOLD), smp_eager_threshold(GLOBAL_SMP_EAGER_THRESHOLD),
                          HBM_enabled(false), MPI_CPI_ratio(1.0), user_regions_CPI_ratio(1.0),
                          ddr_bw_o(0.0), qpi_bw_o(0.0), nic_bw_o(0.0), ddr_bw_t(0.0), qpi_bw_t(0.0), nic_bw_t(0.0), ddr_pft(1.2), qpi_pft(1.5), nic_pft(1.5),
                          clk_per_shmem_call(CLK_PER_SHMEM_CALL), clk_per_netmod_call(CLK_PER_NETMOD_CALL),
                          short_to_med(64), med_to_long(2048), read_ahead_ratio(0.0), gather_transport_stat(0), onetime_pass_per_rank(-1), max_list_size(100), aggregate_funcs(true),
#ifdef PEAT_MPI
                          comm_compute(MPI_COMM_NULL), cycle_events(0),
#endif
                          num_threads(1),
                          comm_rank(0), comm_size(1), trace_comm_size(0), verbose(false)
    {
        short_threshold[PEAT_TRANSPORT_QPI] = GLOBAL_SHORT_THRESHOLD_INTRA;
        short_threshold[PEAT_TRANSPORT_DDR] = GLOBAL_SHORT_THRESHOLD_INTRA;
        short_threshold[PEAT_TRANSPORT_HBM] = GLOBAL_SHORT_THRESHOLD_INTRA;
        short_threshold[PEAT_TRANSPORT_NIC] = GLOBAL_SHORT_THRESHOLD_INTER;
        volume_ratios[0] = 0.0;
        volume_ratios[1] = 0.5;
        volume_ratios[2] = 1.0;
        transport_selector = PET_Transport_Selector_MIN;
        part_receiver = PET_Part_Receiver_EPILOG;
    }
    std::string user_region_file;
    std::string transport_file;
    std::string com_ext;
    std::string trace_files;
    std::string mpi_time_file;
    std::string output_files;
    std::string bind_files;
    std::string mpi_files;
    std::string hwc_files;
    std::string fab_files;
    std::string ptpd_ext;
    std::string app_ext;
    std::string map_file;
    std::string functions_split_file;
    std::string input_path;
    std::string output_path;
    std::string pc_levels;
    int model_output;
    int pcontrol_mode;
    PEAT_Rank pcontrol_mode_rank;
    PEAT_FREQ source_freq;
    PEAT_FREQ target_freq;
    PEAT_Rank target_ppn, target_numa_pp;
    bool estimate_read;
    int progress_engine;
    int use_up;
    int matching_only;
    int skip_processing;
    int print_location;
    PEAT_Message_Size nic_eager_threshold;
    PEAT_Message_Size smp_eager_threshold;
    PEAT_Message_Size short_threshold[PEAT_TRANSPORT_MAX];
    PEAT_Bool HBM_enabled;
    PEAT_Ratio MPI_CPI_ratio;
    PEAT_Ratio user_regions_CPI_ratio;
    PEAT_BW ddr_bw_o, qpi_bw_o, nic_bw_o;
    PEAT_BW ddr_bw_t, qpi_bw_t, nic_bw_t;
    PEAT_Ratio ddr_pft, qpi_pft, nic_pft;
    PEAT_Float clk_per_shmem_call, clk_per_netmod_call;
    PEAT_Message_Size short_to_med, med_to_long;
    PEAT_Ratio volume_ratios[3];
    PEAT_Ratio read_ahead_ratio;
    PET_Transport_Selector transport_selector;
    PET_Part_Receiver part_receiver;
    int gather_transport_stat;
    PEAT_Num onetime_pass_per_rank;
    size_t max_list_size;
    bool aggregate_funcs;
#ifdef PEAT_MPI
    MPI_Comm comm_compute;
    unsigned long long cycle_events;
#endif // PEAT_MPI
    int num_threads;
    std::vector<PEAT_Rank> whole_ranks;
    std::vector<PEAT_Rank> my_ranks;

    PEAT_Rank comm_rank, comm_size;
    PEAT_Num trace_comm_size;

    std::map<PEAT_Rank, std::string> pe_MPI_files_in_rank;
    std::map<PEAT_Rank, std::string> pe_HWC_files_in_rank;
    std::map<PEAT_Rank, std::string> pe_fab_files_in_rank;
    std::map<PEAT_Rank, std::string> pe_MPI_files_in_rank_out;
    std::map<PEAT_Rank, std::string> pe_HWC_files_in_rank_out;
    std::map<PEAT_Rank, std::string> pe_bind_files_in_rank_out;
    std::set<PEAT_Rank> rank_set;
    std::set<std::string> func_set;
    void print();
    void check();
    int verbose;
};

struct User_Address_Info {
    User_Address_Info() : addr_offset(0), addr(0), base_addr(0), load_addr(0), obj_id(-1), addr_id(-1), line(-1) {}
    PEAT_LNum addr_offset;
    PEAT_Addr addr;
    PEAT_Addr base_addr;
    PEAT_Addr load_addr;
    PEAT_UID obj_id;
    PEAT_UID addr_id;
    std::string full_path;
    std::string func;
    std::string source;
    int line;
};

struct User_Address_Key {
    std::vector<PEAT_UID> addrs;
    User_Address_Key()
    {
    }
    User_Address_Key(const std::vector<PEAT_UID> &addr)
    {
        addrs = addr;
    }
    User_Address_Key(const User_Address_Key &key)
    {
        addrs = key.addrs;
    }
    void add(PEAT_UID addr_in)
    {
        addrs.push_back(addr_in);
    }
    PEAT_Bool operator<(const User_Address_Key &rhs) const
    {
        PEAT_Num size = addrs.size();
        PEAT_Num rhs_size = rhs.addrs.size();
        if (size < rhs_size)
            return true;
        if (size > rhs_size)
            return false;
        for (PEAT_Num i = 0; i < size; i++) {
            if (addrs[i] < rhs.addrs[i]) {
                return true;
            }
            if (addrs[i] > rhs.addrs[i]) {
                return false;
            }
        }
        return false;
    }
    User_Address_Key &operator+(const User_Address_Key &add)
    {
        for (std::vector<PEAT_UID>::const_iterator addr_it = add.addrs.cbegin(); addr_it != add.addrs.cend(); ++addr_it) {
            addrs.push_back(*addr_it);
        }
        return *this;
    }

    PEAT_Num pack_size() const
    { // Not object size. Just size for data serialization.
        return pack_pod_size(addrs);
    }
    PEAT_Num pack(char *ptr) const
    {
        PEAT_Num ptr_it = 0;
        ptr_it += pack_pod(ptr + ptr_it, addrs);
        return ptr_it;
    }
    PEAT_Num unpack(const char *ptr)
    {
        PEAT_Num ptr_it = 0;
        ptr_it += unpack_pod(ptr + ptr_it, addrs);
        return ptr_it;
    }
};

struct Event_Address_Info {
#ifdef TRANSLATE_UIDS
    PEAT_Addr *addr;
#else
    PEAT_UID *addr;
#endif
    PEAT_Num num_addr;
    Event_Address_Info(PEAT_Num n)
    {
        num_addr = n;
#ifdef TRANSLATE_UIDS
        addr = new PEAT_Addr[num_addr]; // must be initialized!
#else
        addr = new PEAT_UID[num_addr]; // must be initialized!
#endif
    }
    ~Event_Address_Info()
    {
        delete[] addr; // addr is allocated with new[] above; plain delete is UB (alloc-dealloc-mismatch)
    }
    Event_Address_Info &operator=(Event_Address_Info &s)
    {
        /*if ( num_addr != s.num_addr ) {
            pet_throw_error( NULL, NULL, NULL, "num_addr mismatch\n" );
        }*/
        for (PEAT_Num i = 0; i < num_addr; i++)
            addr[i] = s.addr[i];
        return *this;
    }
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
};

struct User_Region {
    User_Region() : min_instr(LLONG_MAX), max_instr(0), sum_instr(0), median_instr(0.0), u_st_dev_instr(0.0),
                    min_clock(LLONG_MAX), max_clock(0), sum_clock(0), median_clock(0.0), u_st_dev_clock(0.0),
                    min_CPI(DBL_MAX), max_CPI(0.0), sum_CPI(0.0), sum_m_CPI(0.0), median_CPI(0.0), median_m_CPI(0.0), true_median_CPI(DBL_MAX),
                    u_st_dev_CPI(0.0), u_st_dev_m_CPI(0.0), true_u_st_dev_CPI(0.0), num(0) {}
    PEAT_Clock min_instr;
    PEAT_Clock max_instr;
    PEAT_Clock sum_instr;
    PEAT_Ratio median_instr;
    PEAT_Ratio u_st_dev_instr; // Sum((Xi-Xmedian)^2)
    PEAT_Clock min_clock;
    PEAT_Clock max_clock;
    PEAT_Clock sum_clock;
    PEAT_Ratio median_clock;
    PEAT_Ratio u_st_dev_clock; // Sum((Xi-Xmedian)^2)
    PEAT_Ratio min_CPI;
    PEAT_Ratio max_CPI;
    PEAT_Ratio sum_CPI;
    PEAT_Ratio sum_m_CPI;
    PEAT_Ratio median_CPI;
    PEAT_Ratio median_m_CPI;
    PEAT_Ratio true_median_CPI;
    PEAT_Ratio u_st_dev_CPI;      // Sum((Xi-Xmedian)^2)
    PEAT_Ratio u_st_dev_m_CPI;    // Sum((Xi-Xmedian)^2)
    PEAT_Ratio true_u_st_dev_CPI; // Sum((Xi-Xmedian)^2)

    PEAT_LNum num;
    struct Elems {
        Elems() : instr(0), clocks(0), cpi(0.0) {}
        PEAT_Clock instr;
        PEAT_Clock clocks;
        PEAT_Ratio cpi;
        bool operator<(const struct Elems &r) const
        {
            if (cpi < r.cpi)
                return true;
            return false;
        }
    };
    std::vector<Elems> elements;
    void process(PEAT_Ratio window);
    void add(const Elems &el);
};

struct User_Region_Store {
    User_Region_Store() {}
    std::map<PEAT_Rank, std::map<User_Address_Key, User_Region>> user_regions;
    struct Totals {
        PEAT_Clock uc_instr;
        PEAT_Clock uc_clock;
        PEAT_Clock mpi_instr;
        PEAT_Clock mpi_clock;
        Totals &operator+(Totals &r)
        {
            uc_instr += r.uc_instr;
            uc_clock += r.uc_clock;
            mpi_instr += r.mpi_instr;
            mpi_clock += r.mpi_clock;
            return *this;
        }
    };
    std::map<PEAT_Rank, Totals> totals;
};

struct Object_File_Definition {
    PEAT_UID obj_id;
    std::string full_path;
    PEAT_Addr base_addr, load_addr, end_addr;
    Object_File_Definition() : obj_id(-1), base_addr(0), load_addr(0), end_addr(0) {}
};

struct Model_ID;
extern std::set<PEAT_Rank> empty_rank_set;

/* Id path of a trace record: ids[0] is the top-level id, ids[l] the index inside the parent at depth l,
   unused entries are -1. The path identifies a record in the trace and in the bind files. */
typedef std::vector<PEAT_UID> PEAT_Event_IDs;
std::string peat_ids_string(const PEAT_Event_IDs &ids);                                        // "id_0:...:id_{n-1}"
int peat_ids_parse(const String_Split &line, int pos, int columns, PEAT_Event_IDs &ids); // reads columns fields from pos, returns the next position
int peat_ids_level(const PEAT_Event_IDs &ids);                                                 // nesting depth, 0 for a top-level record
inline PEAT_Bool peat_ids_nested(const PEAT_Event_IDs &ids) { return peat_ids_level(ids) > 0; }

/* Header shared by .mpi and .hwc trace records.
   nested format (tractor with nesting levels): id_0:...:id_{n-1}:tid:num_children:<payload>
       n is the number of id columns stored in the com file (PEAT_RT_Number_Of_Events), unused ids are -1,
       records form a pre-order tree per thread: the num_children records following a record are its children.
   legacy format: primary_id:secondary_id:tid:<payload>
       a path of depth one: secondary_id is the index of a colt record inside its host (-1 for a top-level record),
       the number of colt records is kept in the class-specific fields (num_colts). */
struct PEAT_Record_Header {
    PEAT_Event_IDs ids;           // id path, ids[0] is the top-level (primary) id
    PEAT_Thread thread_id;
    PEAT_Signed_Num num_children; // direct children; -1 in the legacy format (unknown at header level)
    int level;                    // nesting depth, 0 for a top-level record
    int payload;                  // index of the first payload field on the .mpi line (function name)
    int hwc_payload;              // index of the first payload field on the .hwc line (first tsc): nested lines carry the ids only
    PEAT_Record_Header() : thread_id(0), num_children(-1), level(0), payload(3), hwc_payload(3) {}
    void parse(const String_Split &line, int id_columns);     // from the .mpi line
    void parse_hwc(const String_Split &line, int id_columns); // from the .hwc line alone (no thread/num_children in the nested format)
    PEAT_Bool same_ids(const PEAT_Record_Header &other) const { return ids == other.ids && thread_id == other.thread_id; }
    PEAT_Bool same_ids(const String_Split &line) const; // compares the id columns of another line (the .hwc twin)
    std::string to_string() const;                     // the .mpi header as written: "id_0:...:tid[:num_children]"
    std::string to_hwc_string() const;                 // the .hwc header as written: nested "id_0:...", legacy "primary:secondary:tid"
};
/* the .mpi header as written to a trace: "id_0:...:tid[:num_children]" (num_children < 0: legacy format) */
std::string peat_record_header_string(const std::vector<PEAT_UID> &ids, PEAT_Thread thread_id, PEAT_Signed_Num num_children);
/* the .hwc header as written to a trace: nested format "id_0:...:id_{n-1}", legacy "primary:secondary:tid" */
std::string peat_record_hwc_header_string(const std::vector<PEAT_UID> &ids, PEAT_Thread thread_id);

struct Address_File_Storage {
    PEAT_Num num_addrs;
    User_Address_Info empty_info;
    Object_File_Definition empty_obj;
    std::map<PEAT_Rank, std::map<PEAT_UID, Object_File_Definition>> modules;
    std::map<PEAT_Rank, std::map<PEAT_UID, User_Address_Info>> appAddresses;
    std::map<PEAT_Rank, std::map<User_Address_Key, PEAT_Ratio>> mAddresses; // addresses from input model
    std::map<PEAT_Rank, PEAT_UID> last_event_idx;
    std::map<PEAT_Rank, PEAT_UID> last_fab_event_idx;
    std::map<PEAT_Rank, PEAT_Thread> last_thread_idx;
    std::map<PEAT_Rank, int> id_columns;          // id columns in the .mpi/.hwc records (nested format; 2 for legacy traces)
    File_Storage_GZ *UR_File;
    const Model_ID *m_id;

  public:
    // Address_File_Storage(const std::string& path, const std::string& user_region_file_name, const std::string& file_name, const std::string& com_file_ext, const std::string& app_file_ext, int flags, const Model_ID* id, int v = 0);
    // Address_File_Storage(const std::string& path, const std::string& user_region_file_name, const std::string& file_name, const std::string& com_file_ext, const std::string& app_file_ext, const std::set <PEAT_Rank>& rank_set,int flags, const Model_ID* id);
    Address_File_Storage(const Global_Parameters &glob, int flags, const Model_ID *id);
    ~Address_File_Storage()
    {
        if (UR_File)
            delete UR_File;
    }
    PEAT_Bool empty() const
    {
        return mAddresses.empty();
    }
    void parse(const String_Split &lines, const PEAT_Rank rank);
    int get_id_columns(const PEAT_Rank rank) const
    {
        std::map<PEAT_Rank, int>::const_iterator it = id_columns.find(rank);
        return it == id_columns.end() ? 2 : it->second;
    }
    int max_id_columns() const
    {
        int cols = 2;
        for (std::map<PEAT_Rank, int>::const_iterator it = id_columns.begin(); it != id_columns.end(); ++it)
            if (it->second > cols)
                cols = it->second;
        return cols;
    }
    void parse_record_header(PEAT_Record_Header &header, const String_Split &line, const PEAT_Rank rank) const
    {
        header.parse(line, get_id_columns(rank));
    }
    void parse_record_hwc_header(PEAT_Record_Header &header, const String_Split &line, const PEAT_Rank rank) const
    {
        header.parse_hwc(line, get_id_columns(rank));
    }
    // Address_File_Storage &operator = (const Address_File_Storage &rhs);
    void put_records(const User_Region_Store &user_regions);
    void get_records();
    PEAT_UID get_num_events(const std::vector<PEAT_Rank> &ranks);
    void put_string(const char *str, PEAT_Rank rank);
    gzFile get_file(PEAT_Rank rank);
    PEAT_Bool get_length(const PEAT_Rank rank, const User_Address_Key &key, PEAT_Clock &retLength, PEAT_Ratio &retRatio, const PEAT_UID &idxStart, const PEAT_UID &idxStop, const int subDirStat);
    void put_value(const PEAT_Rank rank, const User_Address_Key &key, const PEAT_Clock &len, const PEAT_UID &altLIdx);
    // Common files information
    PEAT_Ratio get_overall_weighted();
    void process_user_region_table(const String_Split &line);
    void process_modules_table(const String_Split &line, const PEAT_Rank rank);
    void process_addresses_table(const String_Split &line, const PEAT_Rank rank);
    PEAT_UID convert_obj_id_beetween_ranks(const PEAT_UID objId, const PEAT_Rank from, const PEAT_Rank to) const;
    Object_File_Definition *get_object_by_name(PEAT_Rank rank, std::string full_path);
    User_Address_Info *get_addr_by_id(const PEAT_Rank rank, const PEAT_UID addr_id);
    PEAT_UID get_obj_id_by_name(const std::string &str, const PEAT_Rank rank);

  private:
    String_Split lines;
};

struct PEAT_Time_Parts {
    PEAT_Clock prolog = 0;
    PEAT_Clock meat = 0;
    PEAT_Clock epilog = 0;
    /* statistic part */
    struct PEAT_Time_Parts_Stat {
        PEAT_Clock fabric = 0;
        PEAT_Clock wait = 0;
        PEAT_Clock overlap = 0;
    } stat;
    PEAT_Clock adjust(PEAT_Clock p_adj, PEAT_Clock f_adj, PEAT_Clock e_adj)
    {
        prolog -= p_adj;
        meat -= f_adj;
        epilog -= e_adj;
        return p_adj + f_adj + e_adj;
    }
    PEAT_Clock duration()
    {
        return prolog + meat + epilog;
    }
    PEAT_Clock cpu_duration()
    {
        return prolog + epilog;
    }
    PEAT_Time_Parts &operator=(PEAT_Clock t)
    {
        prolog = meat = epilog = t;
        return *this;
    }
};
struct PEAT_Ext_Time_Parts {
    PEAT_FREQ frequency = 0;
    PEAT_Message_Size message_size = 0;
    PEAT_Time_Parts time_parts[4];
};

struct PEAT_Numa_Node {
    PEAT_Node node;
    PEAT_Node numa_node;
    PEAT_Numa_Node() : node(-1), numa_node(-1) {};
    PEAT_Numa_Node(PEAT_Node n, PEAT_Node n_n) : node(n), numa_node(n_n) {};
    bool operator<(const PEAT_Numa_Node &r) const
    {
        if (node < r.node)
            return true;
        if (node > r.node)
            return false;
        if (numa_node < r.numa_node)
            return true;
        if (numa_node > r.numa_node)
            return false;
        return false;
    }
};
struct PEAT_APS_Time {
    PEAT_Float mpi_time, mpi_imb, app_time;
    PEAT_Float get_active_time() const { return mpi_time - mpi_imb; }
    PEAT_APS_Time(PEAT_Float init = 0.0) : mpi_time(init), mpi_imb(0.0), app_time(init) {};
    PEAT_APS_Time(PEAT_Float a_time, PEAT_Float m_time, PEAT_Float imb) : mpi_time(m_time), mpi_imb(imb), app_time(a_time) {};
    bool operator<(const PEAT_APS_Time &r) const
    {
        if (get_active_time() < r.get_active_time())
            return true;
        return false;
    }
    bool operator>(const PEAT_APS_Time &r) const
    {
        if (get_active_time() > r.get_active_time())
            return true;
        return false;
    }
    PEAT_APS_Time operator+(PEAT_APS_Time &d)
    {
        app_time += d.app_time;
        mpi_time += d.mpi_time;
        mpi_imb += d.mpi_imb;
        return *this;
    }
    PEAT_Ratio imb_ratio() const { return mpi_imb / mpi_time; }
    operator PEAT_Float() const { return mpi_time + mpi_imb; };
};

template <typename T>
struct PEAT_List_Member {
    PEAT_List_Member *next, *prev;
    mutable PEAT_Mutex mutex;
    long ref_count;
    T data;
    PEAT_List_Member() : next(NULL), prev(NULL), ref_count(0) {}
    PEAT_List_Member(PEAT_List_Member &s) : next(s.next), prev(s.prev), data(s.data), ref_count(s.ref_count) {}
    PEAT_List_Member &operator=(PEAT_List_Member &s)
    {
        next = s.next;
        prev = s.prev;
        data = s.data;
        ref_count = s.ref_count;
    }
    void ref_count_add()
    {
        mutex.lock();
        ref_count++;
        mutex.unlock();
    }
    void ref_count_dec()
    {
        mutex.lock();
        ref_count--;
        mutex.unlock();
        assert(ref_count >= 0);
    }
    ~PEAT_List_Member() {}
};
template <typename T>
struct PEAT_List_Member_Iterator {
    PEAT_List_Member<T> *m;
    PEAT_List_Member_Iterator() : m(NULL) {
                                  };
    PEAT_List_Member_Iterator(PEAT_List_Member<T> *d) : m(d)
    {
        m->ref_count_add();
    };
    PEAT_List_Member_Iterator &operator=(const PEAT_List_Member_Iterator &r)
    {
        if (m)
            m->ref_count_dec();
        m = r.m;
        if (m)
            m->ref_count_add();
        return *this;
    };
    PEAT_List_Member_Iterator(const PEAT_List_Member_Iterator &r)
    {
        // no default constructor was called prior this copy one so the data are garbage, hence no ref_count_inc
        //  if (m) m->ref_count_dec();
        m = r.m;
        if (m)
            m->ref_count_add();
    };
#if 0
	void move(PEAT_List_Member_Iterator& r)
	{
		r.m = m;
	};
	void invalidate()
	{
		m = NULL;
	};
#endif
    ~PEAT_List_Member_Iterator()
    {
        if (m)
            m->ref_count_dec();
    }
    PEAT_List_Member_Iterator &operator++()
    {
        if (m)
            m->ref_count_dec();
        if (m)
            m = m->next;
        if (m)
            m->ref_count_add();
        return *this;
    }
    PEAT_List_Member_Iterator &operator--()
    {
        if (m)
            m->ref_count_dec();
        if (m)
            m = m->prev;
        if (m)
            m->ref_count_add();
        // if ((m == NULL) && (s != NULL)) s->unlock();
        return *this;
    }
    // PEAT_List_Member<T> operator&() { return *m; }
    // T& operator&() { return m->data; }
    // T& operator->() { return m->data; }
    PEAT_List_Member<T> *operator->() { return m; }
    bool operator==(PEAT_List_Member_Iterator<T> &s) { return m == s.m; }
};

template <typename T>
struct PEAT_List {
    PEAT_List_Member<T> *head, *tail;
    std::list<PEAT_List_Member<T> *> to_remove;
    size_t l_size;
    mutable PEAT_Mutex mutex;
    using iterator = PEAT_List_Member_Iterator<T>;
    PEAT_List() : head(NULL), tail(NULL), l_size(0) {};
    ~PEAT_List()
    {
        while (head) {
            PEAT_List_Member_Iterator<T> tmp(head);
            erase(tmp);
        }
        collect_garbage();
    }
    PEAT_List_Member<T> *push_back(T &m)
    {
        mutex.lock();
        PEAT_List_Member<T> *tmp = new (PEAT_List_Member<T>);
        tmp->data = m;
        if (tail) {
            tail->next = tmp;
            tmp->prev = tail;
            tail = tmp;
            l_size++;
        } else { // empty list
            head = tail = tmp;
            l_size = 1;
        }
        mutex.unlock();
        collect_garbage();
    }
    PEAT_List_Member_Iterator<T> &erase(PEAT_List_Member_Iterator<T> &it)
    {
        PEAT_List_Member<T> *m = it.m;
        PEAT_List_Member<T> *next = m != NULL ? m->next : NULL;
        if ((m->prev) && (m->next)) { // in the middle of the list
            m->prev->next = m->next;
            m->next->prev = m->prev;
        } else if ((m->prev) && (!m->next)) { // at the end of the list
            m->prev->next = NULL;
            tail = m->prev;
        } else if ((!m->prev) && (m->next)) { // at the start of the list
            m->next->prev = NULL;
            head = m->next;
        } else { // 1 element list
            head = tail = NULL;
            assert(l_size == 1);
        }
        l_size--;
        if (m != NULL) {
            m->ref_count_dec();
            to_remove.push_back(m);
        }
        it.m = next;
        if (it.m)
            it.m->ref_count_add();
        return it;
    }
    void collect_garbage()
    {
        if (mutex.try_lock()) {
            typename std::list<PEAT_List_Member<T> *>::iterator remove_list_it;
            for (remove_list_it = to_remove.begin(); remove_list_it != to_remove.end();) {
                if ((*remove_list_it)->ref_count == 0)
                    remove_list_it = to_remove.erase(remove_list_it);
                else
                    ++remove_list_it;
            }
            mutex.unlock();
        }
    }
    PEAT_List_Member_Iterator<T> begin() const
    {
        return PEAT_List_Member_Iterator<T>(head);
    }
    PEAT_List_Member_Iterator<T> end() const { return PEAT_List_Member_Iterator<T>(); }
    size_t size() const { return l_size; }
    bool empty() const { return l_size == 0; }
    void lock() { mutex.lock(); }
    void lock_shared() { mutex.lock_shared(); }
    void unlock() { mutex.unlock(); }
    void unlock_shared() { mutex.unlock_shared(); }
};

template <typename T>
std::ostream &operator<<(std::ostream &dbg, const PEAT_List<T> &rec)
{
    long listNumber = 0;
    dbg << " elements(" << rec.size() << "){";
    for (PEAT_List_Member_Iterator<T> it = rec.begin(); it != rec.end(); ++it) {
        dbg << "[" << listNumber << "]={" << it->data << "}";
        listNumber++;
    }
    dbg << "}";
    return dbg;
}

template <typename T>
std::ostream &operator<<(std::ostream &dbg, PEAT_List<T> &rec)
{
    long listNumber = 0;
    dbg << " elements(" << rec.size() << "){";
    for (PEAT_List_Member_Iterator<T> it = rec.begin(); it != rec.end(); ++it) {
        dbg << "[" << listNumber << "]={" << it->data << "}";
        listNumber++;
    }
    dbg << "}";
    return dbg;
}

std::ostream &operator<<(std::ostream &dbg, const std::list<PEAT_Message_Size> &id);
std::ostream &operator<<(std::ostream &dbg, const PEAT_Collective_Bytes &rec);

std::ostream &operator<<(std::ostream &dbg, const User_Address_Key &addr);
std::ostream &operator<<(std::ostream &dbg, const User_Address_Info &addr);

#ifdef PEAT_MPI
struct PEAT_Event_Delivery {
    MPI_Request request;
    char *buffer;
    // some debugging information
    int receiver;
    PEAT_MPI_Func_ID func;
};
#endif

#define SEPARATOR(width)              \
    printf("%2d", PEAT_RT_Remark);    \
    for (int i = 0; i < width; ++i) { \
        printf("%c", '-');            \
    }                                 \
    printf("\n")

#define SEPARATORGZ(storage, rank, width)         \
    storage->printf(rank, "%d:", PEAT_RT_Remark); \
    for (int i = 0; i < width; ++i) {             \
        storage->printf(rank, "%c", '-');         \
    }                                             \
    storage->printf(rank, "\n")

#define PRINT_PARAMETERS(width, string) int((width + strlen(string) + 1) / 2), string, int(width - strlen(string)) / 2, ""

static bool is_mpi(PEAT_Property func_class)
{
    switch (func_class) {
    case PEAT_PROP_CLASS_START:
    case PEAT_PROP_CLASS_P2P:
    case PEAT_PROP_CLASS_PROBE:
    case PEAT_PROP_CLASS_REQUEST:
    case PEAT_PROP_CLASS_LOCAL:
    case PEAT_PROP_CLASS_COLLECTIVE:
    case PEAT_PROP_CLASS_BUF_LOCAL:
    case PEAT_PROP_CLASS_OSC:
    case PEAT_PROP_CLASS_FIO:
    case PEAT_PROP_CLASS_COMM:
        return true;
    }
    return false;
}
static bool is_omp(PEAT_Property func_class)
{
    return func_class == PEAT_PROP_CLASS_OMPT;
}

#endif // __COMMON_TYPES_H__
