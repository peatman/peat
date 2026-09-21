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

#ifndef __T_FAB_H__
#define __T_FAB_H__

#if 0
#ifdef _VERBS_PROFILE_
#ifdef __cplusplus
#define __t_cplusplus __cplusplus
#undef __cplusplus
#endif

#include <infiniband/verbs.h>

#ifdef __t_cplusplus
#define __cplusplus __t_cplusplus
#undef __t_cplusplus
#endif

#endif
#endif

#ifdef _FAB_PROFILE_
#include <psm2.h>
#include <psm2_am.h>
#include <psm2_mq.h>
#include <psm3.h>
#include <ucp/api/ucp.h>
#include <rdma/fabric.h>
#include <rdma/fi_domain.h>
#include <rdma/fi_endpoint.h>
#include <rdma/fi_eq.h>
#include <rdma/fi_cm.h>
#endif

#include <sys/syscall.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>

#include "common_plain_types.h"
#include "t_base_event.h"
#include "zlib.h"

struct iovec;
struct MPIDI_PG_t;
struct MPIDI_VC_t;
struct MPIDI_CH3_Pkt_t;
struct vbuf;
typedef enum {
    ONE_FREE = 1,
    TWO_FREE
} smp_ctrl_avail_flag_t;

struct t_fab_profiler : t_base_event {
    t_fab_profiler();
    ~t_fab_profiler();
    long long write_fab_file(bool);
    // void init_hooks();
    void init(int max_threads, char *_stat_dir, char *_tmp_dir, char *_stat_template, int _use_realtime_dump);
    char *gz_ext;
    FILE **fps_events;
    FILE **fps_extras;
    char *stat_template;
    char *stat_dir;
    char *tmp_dir;

    bool compression;
    int compression_level;
    virtual void dump_extras(pid_t tid, msl_base_chain **chain, int is_colt);
    virtual void dump_events(pid_t tid, msl_adv_chain **chain);
};
#include <infiniband/verbs.h>

#endif //__T_FAB_H__
