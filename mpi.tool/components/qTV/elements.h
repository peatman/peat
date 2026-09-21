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

#ifndef __ELEMENTS_H__
#define __ELEMENTS_H__

#include <QFileInfo>
#include <QPen>
#include <QProgressBar>
#include <QProgressDialog>
#include <QScrollBar>
#include <QStatusBar>
#include <QTableWidget>
#include <QTreeWidget>
#include <mutex>

#include "globals.h"
#include "traceinfo.h"
#include "tv_event.h"
#define QTV_TR_VECTOR

#define outputDouble(x) QString("%L1").arg(PEAT_Ratio(x), 8, 'f', 0)
struct PEAT_Viewport {
    PEAT_Clock start, end;
    PEAT_Viewport(PEAT_Clock s = 0, PEAT_Clock e = 0) : start(s), end(e) {};
};
struct Elements_Paint {
    int width, height;
    int thread_height;
    std::vector<qint32> mpi_ulc;
    std::vector<qint32> fab_ulc;
    std::vector<qint32> omp_ulc;
    std::vector<qint32> gpu_ulc;
    void clear(int ranks)
    {
        width = height = thread_height = 0;
        mpi_ulc.clear();
        fab_ulc.clear();
        omp_ulc.clear();
        gpu_ulc.clear();
        if (ranks > 0) {
            mpi_ulc.resize(ranks);
            fab_ulc.resize(ranks);
            omp_ulc.resize(ranks);
            gpu_ulc.resize(ranks);
        }
    }
    Elements_Paint() { clear(0); }
};
class Elements
{
    std::vector<std::list<TV_Event *>> psection;
#ifdef _OPENMP
    String_Split *trace_line_MPIs;
    String_Split *trace_line_HWCs;
    String_Split *trace_line_binds;
    String_Split *trace_line_fabs;
    String_Split *translated_hwcs;
#else
    String_Split trace_line_MPI;
    String_Split trace_line_HWC;
    String_Split trace_line_bind;
    String_Split trace_line_fab;
    String_Split translated_hwc;
#endif
    PTP *ptp; 
    Model_ID *model_id; 
    std::vector<PEAT_Clock> tsc_predictor;
    std::vector<PEAT_Clock> fab_tsc_predictor;
    void translate_hwc_line(const PEAT_Rank rank, const PEAT_MPI_Func_ID func_id, const bool is_second_line, PTP *ptp, const String_Split &hwc, String_Split &out_hwc);
  public:
    Elements(QFileInfo &name, TV_TraceParts *swn);
    ~Elements();
    bool load_model_data(QProgressBar *pb, QScrollBar *sr,
                         std::map<PEAT_Rank, std::string> &mpi_file_names,
                         std::map<PEAT_Rank, std::string> &hwc_file_names,
                         std::map<PEAT_Rank, std::string> &bind_file_names,
                         std::map<PEAT_Rank, std::string> &fab_file_names,
                         const std::set<PEAT_Rank> &ranks_to_load,
                         int tmin, int tmax, int load_threads, TV_TraceParts &ign);
    bool parse_trace_info_file(
        QFileInfo &trace_info,
        std::map<PEAT_Rank, std::string> &mpi_file_names,
        std::map<PEAT_Rank, std::string> &hwc_file_names,
        std::map<PEAT_Rank, std::string> &bind_file_names,
        std::map<PEAT_Rank, std::string> &fab_file_names,
        const std::set<PEAT_Rank> &ranks_to_load);

    void draw_elements(QProgressBar *pb, QScrollBar *sr, QImage *finalImage, QImage *threadImages[], int render_threads, qint32 start, qint32 stop);
    void draw_elements(QProgressBar *pb, QScrollBar *sr, QImage *finalImage, QImage *threadImages[], int render_threads, const qreal start_time, const qreal stop_time);
    void move_viewport(int direction);
    PEAT_Bool get_event(const PEAT_Rank rank, File_Storage_GZ *input_MPI, File_Storage_GZ *input_HWC, PEAT_Clock min_clock, PEAT_Clock max_clock, TV_Event **event, PTP *ptp = NULL);
    PEAT_Bool get_message(const PEAT_Rank rank, File_Storage_GZ *input_bind, TV_TraceParts &tp);
    PEAT_Bool get_fab_event(const PEAT_Rank rank, File_Storage_GZ *input_fab, TV_Fab_Event **event);
    void compact_events(int nthreads);
    void compact_fab_events(int nthreads);
    
    bool find_idx(PEAT_Rank rank, PEAT_UID idx);
    TV_Event *find_event(PEAT_Rank rank, PEAT_UID primary_id, PEAT_UID secondary_id) const;
    TV_Event *find_event(PEAT_Rank rank, PEAT_Clock clk, int thread, int selector) const;
    TV_Fab_Event *find_fab_event(PEAT_Rank rank, PEAT_Clock clk, int thread) const;

    void clear();
    inline PEAT_UID fsize() const
    { // number of function stored
        PEAT_UID result = 0;
        for (std::vector<std::vector<TV_Event *>>::const_iterator it_rank = eventsV.begin(); it_rank != eventsV.end(); ++it_rank) {
            result += it_rank->size();
        }
        return result;
    }
    inline PEAT_UID fabsize() const
    { // number of fabric events stored
        PEAT_UID result = 0;
        for (std::vector<std::vector<TV_Fab_Event *>>::const_iterator it_rank = events_fabV.begin(); it_rank != events_fabV.end(); ++it_rank) {
            result += it_rank->size();
        }
        return result;
    }
    inline PEAT_UID msize() const
    { // number of messages stored
        PEAT_UID result = 0;
        for (std::vector<std::map<PEAT_Clock, TV_Message>>::const_iterator it_rank = messages.begin(); it_rank != messages.end(); ++it_rank) {
            result += it_rank->size();
        }
        return result;
    }
    inline PEAT_Thread get_max_mpi_threads() const
    {
        PEAT_Thread tid = 0;
        for (auto a : max_mpi_tid) {
            if (tid < a)
                tid = a;
        }
        return tid;
    }
    inline PEAT_Thread get_max_omp_threads() const
    {
        PEAT_Thread tid = 0;
        for (auto a : max_omp_tid) {
            if (tid < a)
                tid = a;
        }
        return tid;
    }
    inline PEAT_Thread get_max_gpu_threads() const
    {
        PEAT_Thread tid = 0;
        for (auto a : max_gpu_tid) {
            if (tid < a)
                tid = a;
        }
        return tid;
    }
    inline PEAT_Thread get_mpi_threads(PEAT_Thread t) const
    {
        return max_mpi_tid.at(ranks_to_load_v.at(t));
    }
    inline PEAT_Thread get_fab_threads(PEAT_Thread t) const
    {
        return max_fab_tid.at(ranks_to_load_v.at(t));
    }
    inline PEAT_Thread get_omp_threads(PEAT_Thread t) const
    {
        return max_omp_tid.at(ranks_to_load_v.at(t));
    }
    inline PEAT_Thread get_gpu_threads(PEAT_Thread t) const
    {
        return max_gpu_tid.at(ranks_to_load_v.at(t));
    }

    inline PEAT_Rank get_draw_rank_count() const
    {
        return ranks_to_load_v.size();
    }
    inline PEAT_Rank get_total_rank_count() const
    {
        return ranks_to_load_v_rev.size();
    }
    inline qreal get_left_bound() const
    {
        return zoom_time_start;
    }
    inline qreal get_right_bound() const
    {
        return zoom_time_stop;
    }
    inline qreal get_model_left_bound() const
    {
        return model_time_start;
    }
    inline qreal get_model_right_bound() const
    {
        return model_time_stop;
    }
    inline qreal get_frequency() const
    {
        return trace_info.freq;
    }
    inline qreal get_no_draw_elems() const
    {
        return no_draw_elems;
    }
    inline int get_current_candidate() const
    {
        return current_candidate;
    }
    void reset_zoom()
    {
        zoom_time_start = model_time_start;
        zoom_time_stop = model_time_stop;
    }
    void update_zoom(qreal zt, qreal zp)
    {
        zoom_time_start = zt;
        zoom_time_stop = zp;
    }
    std::vector<PEAT_Rank> &get_loaded_ranks()
    {
        return ranks_to_load_v;
    }
    // current, min, max are the mouse cursor coordinates
    inline PEAT_Clock convert_mouse_to_ticks(long current, long min, long max) const
    {
        return static_cast<PEAT_Clock>(zoom_time_start + (((zoom_time_stop - zoom_time_start) * current) / qAbs(max - min)));
    }

    inline void get_minmax(int &tmin, int &tmax) const
    {
        tmin = (zoom_time_start - trace_info.rank_times.at(0).t1) * 100 / (trace_info.rank_times.at(0).t4 - trace_info.rank_times.at(0).t1);
        tmax = (zoom_time_stop - trace_info.rank_times.at(0).t1) * 100 / (trace_info.rank_times.at(0).t4 - trace_info.rank_times.at(0).t1);
        if (tmax == tmin) {
            if (tmax < 100)
                tmax++;
            else
                tmin--;
        }
        return;
    }

    void get_functions_in_region(QTableWidget *mpi_tw, QTableWidget *omp_tw, QTableWidget *gpu_tw, QTableWidget *fab_tw, const PEAT_Clock start, const PEAT_Clock stop, const PEAT_Num height) const;
    void get_messages_in_region(QTableWidget *initiation, QTableWidget *completion, QTableWidget *p2p, QTreeWidget *collective, QTableWidget *gpu, const PEAT_Clock start, const PEAT_Clock stop, const PEAT_Num height) const;
    void get_user_addresses_in_region(QTreeWidget *tw, const PEAT_Clock start, const PEAT_Clock stop, const PEAT_Num height) const;
    TV_Event *get_event(int x, int y, int width) const;
    TraceInfo &get_trace_info()
    {
        return trace_info;
    }
    void update_src_dirs() { trace_info.update_src_dirs(); };
    PEAT_Bool find_coll_most_imbalance();
    PEAT_Bool find_coll_least_bw(PEAT_Message_Size cutoff);
    PEAT_Bool find_p2p_most_imbalance();
    PEAT_Bool find_p2p_least_bw(PEAT_Message_Size cutoff);
    // rank statistic
    Address_File_Storage *user_regions;

    friend QDebug operator<<(QDebug, Elements const &);
    ALT_ALLOCATOR;

  private:
    std::vector<PEAT_Rank> ranks_to_load_v;
    std::vector<PEAT_Rank> ranks_to_load_v_rev;

    // Event data structures
    // typedef std::map<PEAT_Clock, TV_Event*>::const_iterator EventRankTypeConstIter;
    typedef std::vector<TV_Event *>::const_iterator EventRankTypeConstIter;
    // Local (non-std) binary search by e_time.func.in, tolerant of NULL entries (see compact_events/
    // compact_fab_events). Templated on the pointee type so it works for both TV_Event and TV_Fab_Event.
    template <typename T>
    typename std::vector<T *>::const_iterator lower_bound(const std::vector<T *> &it_rank, PEAT_Clock start_time) const
    {
        PEAT_UID index = 0;
        PEAT_UID max_index = it_rank.size() - 1;
        PEAT_UID min_index = 0;
        do {
            index = (min_index + max_index) / 2;
            const T *event = it_rank.at(index);
            while (event == NULL && index < max_index - 1) {
                event = it_rank.at(++index);
            };

            if (event == NULL)
                index = (min_index + max_index) / 2; // restart

            while (event == NULL && index > min_index + 1) {
                event = it_rank.at(--index);
            };

            if (event == NULL)
                break;

            if (event->e_time.func.in == start_time)
                break;

            if (event->e_time.func.in < start_time) {
                min_index = index;
                // continue;
            }
            if (event->e_time.func.in > start_time) {
                max_index = index;
                // continue;
            }
        } while (max_index - min_index > 1);
        if (min_index < max_index) {
            const T *event = it_rank.at(max_index);
            if (event->e_time.func.in < start_time) {
                return it_rank.begin() + max_index;
            }
        }
        return it_rank.begin() + min_index;
    }

    // std::map<PEAT_Rank, std::map<PEAT_Clock, TV_Event*> > events;//map<rank, map<tscIn, Function> >
    std::vector<PEAT_Thread> max_mpi_tid;
    std::vector<PEAT_Thread> max_fab_tid;
    std::vector<PEAT_Thread> max_omp_tid;
    std::vector<PEAT_Thread> max_gpu_tid;

    // Messages related data structures
    std::vector<std::map<PEAT_UID, TV_Message>> messages;
    std::map<double, PEAT_Viewport> candidates;
    std::map<double, PEAT_Viewport>::iterator candidates_it;
    int current_candidate;
#ifdef _OPENMP
    std::mutex *locks;
#endif
    // Collective related data structures
    struct Collective {
        PEAT_MPI_Func_ID func_id;
        Collective() : func_id(TMPI_FUNC_ID_NONE) {}
        std::map<PEAT_Rank, TV_Event *> events;
        ALT_ALLOCATOR;
    };
    std::vector<std::vector<Collective>> coll; // techically outer vector is not required. It's only to get rid of lock/unlock while doing push_back to coll
    std::vector<std::vector<TV_Event *>> eventsV;
    std::vector<std::vector<TV_Fab_Event *>> events_fabV;
#ifdef QTV_TR_VECTOR
    std::vector<std::vector<PEAT_UID>> events_tr;
    std::vector<std::vector<PEAT_UID>> events_fab_tr;
#else
    std::vector<std::map<PEAT_UID, TV_Event*>> events_tr;
    std::vector<std::map<PEAT_UID, TV_Event*>> events_fab_tr;
#endif

    bool find_collective(Collective &entry);
    TraceInfo trace_info;
    QFileInfo trace_info_file;
    PEAT_Clock model_time_stop;
    PEAT_Clock model_time_start;
    qreal zoom_time_start;
    qreal zoom_time_stop;
    qint32 no_draw_elems;
    TV_TraceParts *shown;
    bool add_event(TV_Event* event, TV_TraceParts& tp);
    bool add_fab_event(TV_Fab_Event* event, TV_TraceParts& tp);
    void add_message(TV_Event *from, TV_Event *to, TV_Pen_Message_Type type, TV_TraceParts &tp, PEAT_Rank rank = -1, PEAT_UID index = -1);
    void cut_events(PEAT_Rank rank, PEAT_UID max_uid)
    {
        eventsV[rank].resize(max_uid);
#ifdef QTV_TR_VECTOR
        events_tr[rank].resize(max_uid);
#endif
    }
    void cut_fab_events(PEAT_Rank rank, PEAT_UID max_uid)
    {
        events_fabV[rank].resize(max_uid);
#ifdef QTV_TR_VECTOR
        events_fab_tr[rank].resize(max_uid);
#endif
    }

  public:
    Elements_Paint paint;
};

#endif // __ELEMENTS_H__
