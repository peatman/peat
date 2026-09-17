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

#ifndef __PET_EVENT_H__
#define __PET_EVENT_H__

#include "common_types.h"
#include "func_id.h"
#include "func_split.h"
#include "pet_defines.h"
#include "pet_fabric_event.h"
#include "stat.h"
#include "te_timeline.h"
#include "timeline.h"
#include <assert.h>

enum event_pending_reason {
    pr_none = 0,
    pr_not_on_top = 1,
    pr_event_not_estimated = 2,
    pr_fabric_not_estimated = 4,
    pr_size
};

enum PET_Event_Type {
    PET_EVENT_TYPE_NONE = 0,
    PET_EVENT_TYPE_LOCAL,
    PET_EVENT_TYPE_LOCAL_BUF,
    PET_EVENT_TYPE_FAKE,
    PET_EVENT_TYPE_COLL,
    PET_EVENT_TYPE_COLLECTIVE,
    PET_EVENT_TYPE_COMM,
    PET_EVENT_TYPE_TOR,
    PET_EVENT_TYPE_TAR,
    PET_EVENT_TYPE_START,
    PET_EVENT_TYPE_COMPLETION,
    PET_EVENT_TYPE_GPU,
    PET_EVENT_TYPE_OMPT,
    PET_EVENT_TYPE_MAX
};
#if 0
//debug function
static PEAT_Bool equal(PET_Event_Type send, PET_Event_Type recv)
{
	if (send == recv) return true;
	if (send == PET_EVENT_TYPE_TAR && recv == PET_EVENT_TYPE_START) return true;
	if (recv == PET_EVENT_TYPE_TAR && send == PET_EVENT_TYPE_START) return true;
	return false;
}
#endif
class PET_Core;
class PET_Node;
class PET_Rank;
class PET_Thread;
struct PET_Coll_Event;
struct PET_ToR_Event;
struct PET_OMPT_Event;
struct PET_Event : public PET_Time_Event {
    PET_Core *pet_core;
    PET_Node *pet_node;
    PET_Rank *pet_rank;
    PET_Thread *pet_thread;
    PEAT_MPI_Func_ID func_id;
    PEAT_MPI_Func_ID parent_func_id;
    PEAT_UID primary_id, secondary_id;
    PEAT_Time_Parts event_parts, o_event_parts;
    PEAT_Rank rank;
    PEAT_Node node;
    PEAT_Thread thread_id;
    PEAT_Bool is_user_code_applied;
    PEAT_Bool estimation_started;
    PEAT_Bool is_written;
    PEAT_Bool is_initiated;
    event_pending_reason pending_reason;
    Event_Address_Info addrs;
    PET_Event *parent_event;
    PET_Coll_Event *host_event;
    PET_OMPT_Event *parallel_section_begin;
    PET_Event *prev_event; // caching purposes
    PET_Event *next_event; // caching purposes
    //	PET_Event *prev_event, *next_event;
    PEAT_Bool sub_t_applied;
    PEAT_Clock sub_t_ae, sub_t_t;
    PEAT_Clock sub_t_ae_t, sub_t_t_t;
    PEAT_Clock uc_length, uco_length;
    PEAT_Bool finish_calculations;
    PEAT_LNum remaining_ee_events;
    std::list<PET_Fabric_Event> fabric_events;
    std::list<PET_Fabric_Event> uc_fabric_events;

  public:
    PET_Event(PET_Core *p);
    virtual PEAT_Bool is_bound() const { return true; }
    PET_Core *get_core() const { return pet_core; }
    PET_Node *get_node() const { return pet_node; }
    PET_Rank *get_rank() const { return pet_rank; }
    PET_Thread *get_thread() const { return pet_thread; }
    void set_parent_event(PET_Event *p)
    {
        parent_event = p;
        parent_func_id = p->func_id;
    }
    PET_Event *get_parent_event() { return (parent_event); }
    void set_host_event(PET_Coll_Event *h) { host_event = h; }
    PET_Coll_Event *get_host_event() { return host_event; }
    void assign_parallel_section_begin(PET_OMPT_Event *r) { parallel_section_begin = r; };
    virtual PET_OMPT_Event *get_parallel_section_begin();
    virtual PET_OMPT_Event *get_parallel_section_end();
    PET_Event *get_current_event();
    void decrease_ee_events()
    {
        remaining_ee_events--;
        if (remaining_ee_events < 0) {
            pet_throw_error(get_core(), this, NULL, "Number of assotiated estimation events falls below 0.\n");
        }
    }
    void increase_ee_events()
    {
        if (remaining_ee_events < 0) {
            pet_throw_error(get_core(), this, NULL, "Number of assotiated estimation events falls below 0.\n");
        }
        remaining_ee_events++;
    }
    virtual PEAT_Bool check_tx() const
    {
        return (global_func_id.check_property(func_id, PEAT_PROP_DATA_TX) || global_func_id.check_property(func_id, PEAT_PROP_DATA_TXRX));
    }
    virtual PEAT_Bool check_rx() const
    {
        return (global_func_id.check_property(func_id, PEAT_PROP_DATA_RX) || global_func_id.check_property(func_id, PEAT_PROP_DATA_TXRX));
    }
    virtual PEAT_Bool check_real_tx() const
    {
        return ((global_func_id.check_property(func_id, PEAT_PROP_DATA_TX) || global_func_id.check_property(func_id, PEAT_PROP_DATA_TXRX)) && !is_persistent_init());
    }
    virtual PEAT_Bool check_real_rx() const
    {
        return ((global_func_id.check_property(func_id, PEAT_PROP_DATA_RX) || global_func_id.check_property(func_id, PEAT_PROP_DATA_TXRX)) && !is_persistent_init());
    }
    virtual PEAT_Clock get_overlap() const { return 0; }
    virtual PEAT_Clock get_fabric() const { return 0; }
    virtual PET_Event *get_estimated_event() { return this; }
    virtual void notify_before_destroy(PET_Event *event) {};
    PEAT_Clock apply_user_code(PEAT_Ratio r, PET_Event *prev_event)
    {
        if (is_user_code_applied)
            return 0;
        PEAT_Clock sub_t = uc_length = uco_length = get_user_code_length(prev_event);
        uc_length = (PEAT_Clock)(uc_length * r);
        sub_t -= uc_length;
        is_user_code_applied = true;
        return move_event(sub_t);
    }
    PEAT_Clock get_user_code_length(PET_Event *prev_event);
    PEAT_Bool set_estimated();
    PEAT_Bool clear_estimated();
    virtual PEAT_Bool can_be_destroyed();

    virtual PEAT_Bool read_event(PEAT_Rank r, const String_Split &mpi_string, const String_Split &hwc_string, PET_Coll_Event *host);
    virtual void post_read_event() {}

    virtual void scale_event();
    virtual PET_Event *write_event(File_Storage_GZ *gz_MPI, File_Storage_GZ *gz_HWC, File_Storage_GZ *gz_bind, const char *mpi_eol = "\n", const char *hwc_eol = "\n");
    virtual PEAT_Bool bind() { return true; }
    virtual PEAT_Bool query_release();
    virtual PEAT_Bool release();
    virtual PEAT_Bool update_pending();
    virtual PEAT_Bool safe_to_delete();
    virtual PEAT_Bool get_tx_paired() const { return true; };
    virtual PEAT_Bool get_rx_paired() const { return true; };
    virtual PEAT_Message_Size get_tx_bytes() const { return -1; };
    virtual PEAT_Message_Size get_rx_bytes() const { return -1; };
    virtual PEAT_Message_Size get_real_tx_bytes() const { return -1; };
    virtual PEAT_Message_Size get_real_rx_bytes() const { return -1; };
    virtual PET_Event *query_request(PEAT_MPI_Request req) { return NULL; }
    virtual PEAT_MPI_Request get_mpi_request() const { return 0; }
    virtual PEAT_MPI_Message get_mpi_message() const { return 0; }
    virtual PEAT_Bool ready_to_estimate();
    virtual PEAT_Bool initiate();
    virtual PEAT_Bool estimate(Estimation_Event *event);
    virtual PEAT_Bool on_update(TE_Event *te_event) { return false; }
    // PEAT_Bool check_estimated() { estimate(NULL);  return is_estimated(); }
    virtual PEAT_Bool is_estimated() const
    {
        return !check_reason(pr_event_not_estimated);
    };
    PEAT_Bool is_colt() const
    {
        return (global_func_id.check_property(func_id, PEAT_PROP_COLT));
    }

    virtual PEAT_Bool set_blank(PET_Event *evt) { return true; };
    virtual PET_Event_Type get_event_type() const
    {
        assert(0);
        return PET_EVENT_TYPE_NONE;
    }
    virtual PET_Event *get_event_from_req(PEAT_MPI_Request req) { return this; }
    virtual void remote_written(PEAT_Bool is_send) {};
    virtual void set_written() { is_written = true; };
    virtual PEAT_Clock remove_tractor_overhead(PEAT_Clock *prolog_overhead = NULL);
    virtual void set_sub_t(PEAT_Clock t_sub_t_t, PEAT_Clock t_sub_t_ae)
    {
        sub_t_t_t = t_sub_t_t;
        sub_t_ae_t = t_sub_t_ae;
    }
    virtual PEAT_Clock move_event(PEAT_Clock sub_t)
    {
        e_time.move(sub_t);
        sub_t_ae_t += sub_t;
        return sub_t;
    }
    virtual PEAT_Clock adjust_event(PEAT_Clock sub_t)
    {
        e_time.adjust(sub_t);
        event_parts.adjust(0, sub_t, 0);
        return sub_t;
    }
    virtual PEAT_Bool split_event();
    virtual PEAT_Bool get_bounds();
    virtual void concatenate_event();
    virtual PEAT_Clock scale_cpu_part(PEAT_Ratio ratio);
    virtual PEAT_MPI_Comm get_comm() const
    {
        return -100; //(MPI_ANY_TAG) * 100 , just invalid value which never occurs
    }
    virtual PEAT_MPI_Tag get_send_tag() const
    {
        return -100; //(MPI_ANY_TAG) * 100 , just invalid value which never occurs
    }
    virtual PEAT_MPI_Tag get_recv_tag() const
    {
        return -100; //(MPI_ANY_TAG) * 100 , just invalid value which never occurs
    }
    virtual PEAT_Rank get_dest() const
    {
        return -200; //(MPI_ANY_SOURCE) * 100 , just invalid value which never occurs
    }
    virtual PEAT_Rank get_src() const
    {
        return -200; //(MPI_ANY_SOURCE) * 100 , just invalid value which never occurs
    }
    virtual PET_Event *get_completion() const
    {
        return NULL;
    }
    virtual PET_Event *get_peer_for_send() const
    {
        return NULL;
    }
    virtual PET_Event *get_peer_for_recv() const
    {
        return NULL;
    }
    virtual void set_recv_tag(PEAT_MPI_Tag t) {}
    virtual void set_src(PEAT_Rank r) {}
    virtual void set_t_bytes(PEAT_LNum b) {}

    virtual void set_completion(PET_Event *event) {}
    virtual void set_peer_for_send(PET_ToR_Event *event) {}
    virtual void set_peer_for_recv(PET_ToR_Event *event) {}
    virtual PEAT_Bool is_paired() const { return true; }

    virtual PEAT_Bool ready_to_bind() const { return true; }

    virtual ~PET_Event();
    void clear_reason(event_pending_reason r) { pending_reason = (event_pending_reason)((int)(pending_reason) & (int)~r); }
    void set_reason(event_pending_reason r) { pending_reason = (event_pending_reason)((int)(pending_reason) | (int)r); }
    PEAT_Bool check_reason(event_pending_reason r) const { return ((((int)(pending_reason) & (int)r)) > 0); }
    PEAT_Bool is_persistent_init() const
    {
        return global_func_id.check_property(func_id, PEAT_PROP_PERSISTENT_REQUEST_INIT);
    }
    PEAT_Bool is_mpic() const
    {
        return global_func_id.check_property(func_id, PEAT_PROP_COLT);
    }
    PEAT_Bool is_send() const
    {
        return global_func_id.check_property(func_id, PEAT_PROP_DATA_TX) || global_func_id.check_property(func_id, PEAT_PROP_DATA_TXRX);
    }

    virtual PET_Event *get_real_event(PET_Event *evt)
    {
        assert(0);
        return NULL;
    }
    virtual PEAT_Bool completion_received() const { return false; }
    virtual PET_Event *get_first_child_event();
    virtual PET_Event *get_next_child_event();
    virtual PET_Event *get_prev_child_event();
    virtual PET_Event *get_last_child_event();
    virtual PET_Event *get_next_event();
    virtual PET_Event *get_prev_event(PEAT_Bool reverse);
    virtual void insert();
    void assign_cache_pointers();
    PEAT_Bool submit(PEAT_Clock clk_start, Estimation_Event_Type t, PET_Event *owner, PET_Event *submiter, TE_Event *info);
    PEAT_Bool submit(PEAT_Clock clk_start, Estimation_Event *event);
    PEAT_Bool post(PEAT_Clock clk_start, Estimation_Event_Type t, PET_Event *owner, PET_Event *submiter, TE_Event *info);
    virtual PEAT_Rank get_rank_read_suggestion() { return -1; }
    virtual PEAT_MPI_Func_ID missing_event() { return TMPI_FUNC_ID_NONE; }
    virtual PEAT_Transports get_msg_transport_id() const { return PEAT_TRANSPORT_MAX; }
    virtual PEAT_Rank get_ranks_num() const { return 1; }
    PEAT_Bool guess_blocking() const;
    virtual PEAT_Bool check_up_status() const
    {
        return false;
    };
    virtual PEAT_Num check_for_outstanding_progress_messages(PEAT_Clock clk)
    {
        return 0;
    }
    PEAT_Num handle_progress_messages(PEAT_Clock clk, PEAT_Bool just_check);
    PEAT_Num ping_progress(Estimation_Event *evt);
    PEAT_Bool put_transport_event_stat();
    virtual PEAT_Clock get_centre_code(Event_Set_Stat::t_stat_type t) { return 0; }
};

struct PET_Local_Event : public PET_Event {
  public:
    PEAT_Signed_Num p_control_id;
    PET_Local_Event(PET_Core *p) : PET_Event(p), p_control_id(0) {}
    virtual PET_Event_Type get_event_type() const { return PET_EVENT_TYPE_LOCAL; }
    virtual PEAT_Bool read_event(PEAT_Rank r_rank, const String_Split &mpi_string, const String_Split &hwc_string, PET_Coll_Event *host);
    virtual PET_Event *write_event(File_Storage_GZ *gz_MPI, File_Storage_GZ *gz_HWC, File_Storage_GZ *gz_bind, const char *mpi_eol = "\n", const char *hwc_eol = "\n");
};

struct PET_GPU_Event : public PET_Local_Event {
    std::vector<PEAT_ULNum> params;
    std::vector<PEAT_ULNum> out_params;

  public:
    PET_GPU_Event(PET_Core *p);
    ~PET_GPU_Event();
    virtual PEAT_Bool read_event(PEAT_Rank r_rank, const String_Split &mpi_string, const String_Split &hwc_string, PET_Coll_Event *host);
    virtual PET_Event *write_event(File_Storage_GZ *gz_MPI, File_Storage_GZ *gz_HWC, File_Storage_GZ *gz_bind, const char *mpi_eol = "\n", const char *hwc_eol = "\n");
    virtual PET_Event_Type get_event_type() const { return PET_EVENT_TYPE_GPU; }
};

struct PET_Local_Buf_Event : public PET_Local_Event {
    PEAT_Num inbuf_size, outbuf_size;

  public:
    virtual PET_Event_Type get_event_type() const { return PET_EVENT_TYPE_LOCAL_BUF; }
    PET_Local_Buf_Event(PET_Core *p) : PET_Local_Event(p), inbuf_size(0), outbuf_size(0) {}
    virtual PEAT_Bool read_event(PEAT_Rank r, const String_Split &mpi_string, const String_Split &hwc_string, PET_Coll_Event *host);
    virtual PET_Event *write_event(File_Storage_GZ *gz_MPI, File_Storage_GZ *gz_HWC, File_Storage_GZ *gz_bind, const char *mpi_eol = "\n", const char *hwc_eol = "\n");
};
struct PET_ToR_Event : public PET_Local_Event {
    PEAT_MPI_Comm comm;
    PEAT_LNum t_bytes;
    PEAT_MPI_Tag tag;
    PEAT_Rank peer_rank;
    PEAT_MPI_Request request;
    PEAT_MPI_Message message;
    PEAT_UID peer_primary_id, peer_secondary_id;
    PEAT_MPI_Func_ID peer_func_id, peer_completion_func_id;
    PEAT_UID completion_primary_id, completion_secondary_id;
    PEAT_UID peer_completion_primary_id, peer_completion_secondary_id;
    PET_Event *aux_event, *aux_event_2;
    PEAT_Bool block_override;
    PEAT_Bool rs_created;
    TE_Event rts, cts, tx_message, ack;
    TE_Event p_rts, p_cts, rx_message, p_ack;
    TE_Event fake_evt;
    PEAT_Bool treat_as_local;
    std::list<PET_Event *> blanks;

  public:
    void set_treat_as_local() { treat_as_local = true; }
    PET_ToR_Event(PET_Core *p) : PET_Local_Event(p), comm(0), t_bytes(0), tag(-1), peer_rank(-1), request(0), message(0),
                                 peer_primary_id(-1), peer_secondary_id(-1), peer_func_id(TMPI_FUNC_ID_NONE), peer_completion_func_id(TMPI_FUNC_ID_NONE),
                                 completion_primary_id(-1), completion_secondary_id(-1),
                                 peer_completion_primary_id(-1), peer_completion_secondary_id(-1),
                                 aux_event(NULL), aux_event_2(NULL), block_override(false), rs_created(false), treat_as_local(false)
    {
    }
    PET_ToR_Event(const PET_ToR_Event &s) : PET_Local_Event(s.pet_core), comm(s.comm), t_bytes(s.t_bytes), tag(s.tag), peer_rank(s.peer_rank), request(s.request), message(s.message),
                                            peer_primary_id(s.peer_primary_id), peer_secondary_id(s.peer_secondary_id), peer_func_id(s.peer_func_id), peer_completion_func_id(s.peer_completion_func_id),
                                            completion_primary_id(s.completion_primary_id), completion_secondary_id(s.completion_secondary_id),
                                            aux_event(s.aux_event), aux_event_2(s.aux_event_2),
                                            block_override(s.block_override), rs_created(s.rs_created),
                                            rts(s.rts), cts(s.cts), tx_message(s.tx_message), ack(s.ack),
                                            p_rts(s.p_rts), p_cts(s.p_cts), rx_message(s.rx_message), p_ack(s.p_ack),
                                            treat_as_local(s.treat_as_local)
    {
        func_id = s.func_id;
        parent_func_id = s.parent_func_id;
        primary_id = s.primary_id;
        secondary_id = s.secondary_id;
        rank = s.rank;
        node = s.node;
        thread_id = s.thread_id;
        parent_event = s.parent_event;
        host_event = s.host_event;
        parallel_section_begin = s.parallel_section_begin;
        estimation_started = s.estimation_started;
        finish_calculations = s.finish_calculations;
    }
    virtual PEAT_Clock get_overlap() const
    {
        if (is_non_block() || is_persistent_init())
            return 0;
        if (check_tx() && check_rx()) {
            return rts.get_overlap() + tx_message.get_overlap() + cts.get_overlap() + ack.get_overlap();
        }

        if (check_tx()) {
            return rts.get_overlap() + tx_message.get_overlap();
        }
        if (check_rx()) {
            return cts.get_overlap() + ack.get_overlap();
        }
        return 0;
    }
    virtual PEAT_Clock get_fabric() const
    {
        if (is_non_block() || is_persistent_init())
            return 0;
        if (check_tx() && check_rx()) {
            return rts.get_fabric() + tx_message.get_fabric() + cts.get_fabric() + ack.get_fabric();
        }

        if (check_tx()) {
            return rts.get_fabric() + tx_message.get_fabric();
        }
        if (check_rx()) {
            return cts.get_fabric() + ack.get_fabric();
        }
        return 0;
    }
    virtual PEAT_Clock move_event(PEAT_Clock sub_t)
    {
        PET_Local_Event::move_event(sub_t);
        // PET_ToR_Event *peer = ( PET_ToR_Event * )get_peer_for_send();
        return sub_t;
    }

    virtual PEAT_Bool on_update(TE_Event *te_event);
    virtual PET_Event_Type get_event_type() const { return PET_EVENT_TYPE_TOR; }
    virtual ~PET_ToR_Event();
    virtual void notify_before_destroy(PET_Event *event);

    virtual PEAT_Bool read_event(PEAT_Rank r, const String_Split &mpi_string, const String_Split &hwc_string, PET_Coll_Event *host);
    PEAT_Bool write_bind(File_Storage_GZ *gz_bind);
    virtual PET_Event *write_event(File_Storage_GZ *gz_MPI, File_Storage_GZ *gz_HWC, File_Storage_GZ *gz_bind, const char *mpi_eol = "\n", const char *hwc_eol = "\n");
    virtual PEAT_Bool safe_to_delete();
    virtual PEAT_Bool ready_to_estimate();
    virtual PEAT_Bool can_be_destroyed();
    PEAT_Bool check_completion();

    virtual PEAT_Bool estimate(Estimation_Event *event);
    // virtual PEAT_Bool is_estimated();
    virtual PEAT_Message_Size get_tx_bytes() const
    {
        if (check_tx())
            return t_bytes;
        return -1;
    };
    virtual PEAT_Message_Size get_rx_bytes() const
    {
        if (check_rx())
            return t_bytes;
        return -1;
    };
    virtual PEAT_Message_Size get_real_tx_bytes() const
    {
        if (check_real_tx())
            return t_bytes;
        return -1;
    };
    virtual PEAT_Message_Size get_real_rx_bytes() const
    {
        if (check_real_rx())
            return t_bytes;
        return -1;
    };

    virtual void handle_event(Estimation_Event *event);
    PEAT_Bool estimate_blocked();
    PEAT_Bool estimate_nonblocked();
    PEAT_Bool place_rts(PEAT_Clock clk_start);
    PEAT_Bool place_cts(PEAT_Clock clk_start);
    PEAT_Bool place_message(PEAT_Clock clk_start);
    PEAT_Bool place_ack(PEAT_Clock clk_start);
    PET_Fabric_Event *get_send_event(PEAT_UID index);
    PET_Fabric_Event *get_recv_event(PEAT_UID index);
    virtual PEAT_Bool is_rdv(PEAT_DIRECTION direction) const;
    virtual PEAT_Bool is_smp(PEAT_DIRECTION direction) const;
    PEAT_Bool is_short(PEAT_Transports t) const;
    // PEAT_Bool is_not_verified();

    virtual PEAT_Bool set_blank(PET_Event *evt)
    {
        blanks.push_back(evt);
        return true;
    };
    PEAT_Bool is_non_block() const
    {
        if ((global_func_id.check_property(func_id, PEAT_PROP_NON_BLOCK) || is_persistent_init()) && !block_override)
            return true;
        if (global_func_id.check_property(func_id, PEAT_PROP_BLOCK) || block_override)
            return false;
        return !guess_blocking();
    }
    void set_block(PEAT_Bool b) { block_override = b; }
    PEAT_Bool get_block(PEAT_Bool b) { return block_override; }
    virtual PEAT_Bool split_event();
    virtual PEAT_Bool get_bounds();

    virtual PEAT_MPI_Comm get_comm() const { return comm; }
    virtual PEAT_Bool get_tx_paired() const { return aux_event != NULL; };
    virtual PEAT_Bool get_rx_paired() const { return aux_event != NULL; };

    virtual PEAT_MPI_Tag get_send_tag() const
    {
        if (treat_as_local)
            return PET_Event::get_send_tag();
        if (check_tx())
            return tag;
        return PEAT_ERROR_INVALID_TX_PARAM; //(MPI_ANY_TAG)
    }
    virtual PEAT_MPI_Tag get_recv_tag() const
    {
        if (treat_as_local)
            return PET_Event::get_recv_tag();
        if (check_rx())
            return tag;
        return PEAT_ERROR_INVALID_RX_PARAM; //(MPI_ANY_TAG)
    }
    virtual PEAT_Rank get_dest() const
    {
        if (treat_as_local)
            return PET_Event::get_dest();
        if (check_tx())
            return peer_rank;
        return PEAT_ERROR_INVALID_TX_PARAM; //(MPI_ANY_SOURCE)
    }
    virtual PEAT_Rank get_src() const
    {
        if (treat_as_local)
            return PET_Event::get_src();
        if (check_rx())
            return peer_rank;
        return PEAT_ERROR_INVALID_RX_PARAM; //(MPI_ANY_SOURCE)
    }

    virtual void set_recv_tag(PEAT_MPI_Tag t)
    {
        if (check_rx()) {
            if (tag != t && tag >= 0) {
                char buf[256];
                sprintf(buf, "Tag Mismatch %d received %d rank %d,primary_id=%lld secondary_id=%lld, parent=%lld\n", tag, t, rank, primary_id, secondary_id, aux_event_2->primary_id);
                pet_throw_error(pet_core, this, NULL, buf);
            }
            tag = t;
        }
    }
    virtual void set_src(PEAT_Rank r)
    {
        if (check_rx()) {
            if (peer_rank != r && peer_rank >= 0) {
                char buf[256];
                sprintf(buf, "Rank Mismatch %d received %d rank %d,primary_id=%lld secondary_id=%lld, parent=%lld\n", peer_rank, r, rank, primary_id, secondary_id, aux_event_2->primary_id);
                pet_throw_error(pet_core, this, NULL, buf);
            }
            peer_rank = r;
        }
    }
    virtual void set_t_bytes(PEAT_LNum b)
    {
        if (check_rx())
            t_bytes = b;
    }
    virtual PET_Event *get_completion() const
    {
        return aux_event_2;
    }
    virtual PET_Event *get_peer_for_send() const
    {
        if (check_tx())
            return aux_event;
        return NULL;
    }
    virtual PET_Event *get_peer_for_recv() const
    {
        if (check_rx())
            return aux_event;
        return NULL;
    }
    virtual void set_peer_for_send(PET_ToR_Event *event)
    {
        if (check_tx()) {
            aux_event = event;
            peer_primary_id = event->primary_id;
            peer_secondary_id = event->secondary_id;
            peer_completion_func_id = event->get_completion() ? event->get_completion()->func_id : event->func_id;
            peer_completion_primary_id = event->get_completion() ? event->get_completion()->primary_id : event->primary_id;
            peer_completion_secondary_id = event->get_completion() ? event->get_completion()->secondary_id : event->secondary_id;
            peer_func_id = event->func_id;
        }
    }
    virtual void set_peer_for_recv(PET_ToR_Event *event)
    {
        if (check_rx()) {
            aux_event = event;
            peer_primary_id = event->primary_id;
            peer_secondary_id = event->secondary_id;
            peer_completion_func_id = event->get_completion() ? event->get_completion()->func_id : event->func_id;
            // not used actually but set anyway
            peer_completion_primary_id = event->get_completion() ? event->get_completion()->primary_id : event->primary_id;
            peer_completion_secondary_id = event->get_completion() ? event->get_completion()->secondary_id : event->secondary_id;
            peer_func_id = event->func_id;
        }
    }
    virtual void set_completion(PET_Event *event) { aux_event_2 = event; }

    virtual PEAT_Bool bind();
    virtual PET_Event *query_request(PEAT_MPI_Request req)
    {
        if ((req == request) && (get_completion() == NULL))
            return this;
        return NULL;
    }
    virtual PEAT_MPI_Request get_mpi_request() const { return request; }
    virtual PEAT_MPI_Message get_mpi_message() const { return message; }
    PEAT_Clock get_rts_start() const;
    PEAT_Clock get_cts_start() const;
    PEAT_Clock get_ack_start() const;
    PEAT_Clock get_rts_finish(PEAT_DIRECTION direction) const;
    PEAT_Clock get_cts_finish(PEAT_DIRECTION direction) const;
    PEAT_Clock get_message_start(PEAT_DIRECTION direction) const;
    PEAT_Clock get_message_finish(PEAT_DIRECTION direction) const;
    PEAT_Clock get_ack_finish(PEAT_DIRECTION direction) const;
    PEAT_Bool get_up_status(PEAT_DIRECTION direction) const;
    virtual PEAT_Bool check_up_status() const;

    const TE_Event &get_highest_message_completion() const
    {
        PEAT_Clock tx_message_comp = tx_message.finish;
        PEAT_Clock rx_message_comp = rx_message.finish;
        if (tx_message_comp > rx_message_comp)
            return tx_message;
        if (tx_message_comp <= rx_message_comp)
            return rx_message;
        return rx_message;
    }
    const TE_Event &get_lowest_message_completion() const
    {
        PEAT_Clock tx_message_comp = tx_message.finish > 0 ? tx_message.finish : LLONG_MAX;
        PEAT_Clock rx_message_comp = rx_message.finish > 0 ? rx_message.finish : LLONG_MAX;
        if (tx_message_comp <= rx_message_comp)
            return tx_message;
        if (tx_message_comp > rx_message_comp)
            return rx_message;
        return tx_message;
    }
    PET_Event *get_peer() const { return aux_event; }
    virtual PEAT_Bool is_bound() const
    {
        if (treat_as_local)
            return true;
        return (get_peer() != NULL) || is_persistent_init();
    }
    virtual PET_Event *get_real_event(PET_Event *evt);
    virtual PEAT_Bool completion_received() const
    {
        return (!(aux_event_2 == NULL)) || request == 0 || is_persistent_init() || treat_as_local;
    }
    virtual PEAT_Bool ready_to_bind() const
    {
        return completion_received() || treat_as_local;
    }
    virtual PEAT_Rank get_rank_read_suggestion()
    {
        if (!completion_received())
            return rank;
        if (get_peer() == NULL && !treat_as_local)
            return peer_rank;
        return -1;
    }
    PEAT_MPI_Func_ID translate_func_id() const;
    PEAT_MPI_Func_ID translate_parent_func_id() const;

    virtual PEAT_Transports get_msg_transport_id() const;
    PEAT_Trasport_Environment *get_pre_transport();
    virtual PEAT_Rank get_ranks_num() const { return 2; }
    virtual PEAT_Num check_for_outstanding_progress_messages(PEAT_Clock clk);
    PEAT_Bool is_TT_match(PEAT_Transfer_Type type)
    {
        if (type == PEAT_TT_BOTH)
            return true;
        PEAT_Trasport_Environment *env = get_pre_transport();
        if (type == PEAT_TT_INTER && env->t_id == PEAT_TRANSPORT_NIC)
            return true;
        if (type == PEAT_TT_INTRA && (env->t_id == PEAT_TRANSPORT_DDR || env->t_id == PEAT_TRANSPORT_QPI || env->t_id == PEAT_TRANSPORT_HBM))
            return true;
        return false;
    }
};

struct PET_TaR_Event : public PET_ToR_Event {
    // for RX part
    PEAT_Bool process_second_line;
    PEAT_LNum t_bytes_2;
    PEAT_MPI_Tag tag_2;
    PEAT_Rank peer_rank_2;
    PEAT_UID peer_primary_id_2, peer_secondary_id_2;
    PEAT_UID peer_completion_primary_id_2, peer_completion_secondary_id_2;
    PEAT_MPI_Func_ID peer_func_id_2, peer_completion_func_id_2;

  public:
    PET_TaR_Event(PET_Core *p) : PET_ToR_Event(p), process_second_line(false), t_bytes_2(-1), tag_2(-1), peer_rank_2(-1),
                                 peer_primary_id_2(-1), peer_secondary_id_2(-1),
                                 peer_completion_primary_id_2(-1), peer_completion_secondary_id_2(-1),
                                 peer_func_id_2(TMPI_FUNC_ID_NONE), peer_completion_func_id_2(TMPI_FUNC_ID_NONE)
    {
    }
    PET_TaR_Event(const PET_TaR_Event &s) : PET_ToR_Event(s.pet_core), process_second_line(s.process_second_line),
                                            t_bytes_2(s.t_bytes_2), tag_2(s.tag_2), peer_rank_2(s.peer_rank_2),
                                            peer_primary_id_2(s.peer_primary_id_2), peer_secondary_id_2(s.peer_secondary_id_2),
                                            peer_completion_primary_id_2(s.peer_completion_primary_id_2), peer_completion_secondary_id_2(s.peer_completion_secondary_id_2),
                                            peer_func_id_2(s.peer_func_id_2), peer_completion_func_id_2(s.peer_completion_func_id_2)
    {
    }
    PET_TaR_Event(const PET_ToR_Event &s) : PET_ToR_Event(s.pet_core), process_second_line(false),
                                            t_bytes_2(-1), tag_2(-1), peer_rank_2(-1),
                                            peer_primary_id_2(-1), peer_secondary_id_2(-1), peer_func_id_2(TMPI_FUNC_ID_NONE), peer_completion_func_id_2(TMPI_FUNC_ID_NONE)
    {
        if (s.check_tx()) {
            peer_func_id_2 = s.peer_func_id;
            peer_completion_func_id_2 = s.peer_completion_func_id;
            peer_primary_id_2 = s.peer_primary_id;
            peer_secondary_id_2 = s.peer_secondary_id;
            peer_completion_primary_id_2 = s.peer_completion_primary_id;
            peer_completion_secondary_id_2 = s.peer_completion_secondary_id;
            peer_func_id = TMPI_FUNC_ID_NONE;
            peer_completion_func_id = TMPI_FUNC_ID_NONE;
            peer_primary_id = -1;
            peer_secondary_id = -1;
        }
    }
    virtual PET_Event_Type get_event_type() const { return PET_EVENT_TYPE_TAR; }
    virtual PEAT_Bool read_event(PEAT_Rank r, const String_Split &mpi_string, const String_Split &hwc_string, PET_Coll_Event *host);
    virtual PET_Event *write_event(File_Storage_GZ *gz_MPI, File_Storage_GZ *gz_HWC, File_Storage_GZ *gz_bind, const char *mpi_eol = "\n", const char *hwc_eol = "\n");
    virtual PEAT_Bool estimate(Estimation_Event *event);
    // virtual PEAT_Bool is_estimated();
    virtual PEAT_Bool get_tx_paired() const { return aux_event != NULL; };
    virtual PEAT_Bool get_rx_paired() const { return aux_event_2 != NULL; };
    virtual PEAT_Message_Size get_tx_bytes() const { return t_bytes; };
    virtual PEAT_Message_Size get_rx_bytes() const { return t_bytes_2; };
    virtual PEAT_MPI_Tag get_send_tag() const
    {
        return tag;
    }
    virtual PEAT_MPI_Tag get_recv_tag() const
    {
        return tag_2;
    }
    virtual PEAT_Rank get_dest() const
    {
        return peer_rank;
    }
    virtual PEAT_Rank get_src() const
    {
        return peer_rank_2;
    }
    virtual PEAT_Bool is_rdv(PEAT_DIRECTION direction) const;
    virtual PEAT_Bool is_smp(PEAT_DIRECTION direction) const;
    virtual PEAT_Bool bind();
    virtual PEAT_MPI_Request get_mpi_request() const { return 0; }
    virtual PET_Event *get_completion() const
    {
        return NULL;
    }
    virtual PET_Event *get_peer_for_send() const
    {
        return aux_event;
    }
    virtual PET_Event *get_peer_for_recv() const
    {
        return aux_event_2;
    }
    virtual void set_completion(PET_Event *event) {}
    virtual void set_peer_for_send(PET_ToR_Event *event)
    {
        aux_event = event;
        peer_completion_func_id = event->get_completion() ? event->get_completion()->func_id : event->func_id;
        peer_completion_primary_id = event->get_completion() ? event->get_completion()->primary_id : event->primary_id;
        peer_completion_secondary_id = event->get_completion() ? event->get_completion()->secondary_id : event->secondary_id;
        peer_func_id = event->func_id;
        peer_primary_id = event->primary_id;
        peer_secondary_id = event->secondary_id;
    }
    virtual void set_peer_for_recv(PET_ToR_Event *event)
    {
        aux_event_2 = event;
        peer_completion_func_id_2 = event->get_completion() ? event->get_completion()->func_id : event->func_id;
        peer_completion_primary_id_2 = event->get_completion() ? event->get_completion()->primary_id : event->primary_id;
        peer_completion_secondary_id_2 = event->get_completion() ? event->get_completion()->secondary_id : event->secondary_id;
        peer_func_id_2 = event->func_id;
        peer_primary_id_2 = event->primary_id;
        peer_secondary_id_2 = event->secondary_id;
    }
    virtual PEAT_Bool is_bound() const
    {
        return (get_peer_for_send() != NULL) && (get_peer_for_recv() != NULL);
    }
    PEAT_Bool write_bind(File_Storage_GZ *gz_bind);
    virtual PEAT_Rank get_rank_read_suggestion()
    {
        if (get_peer_for_send() == NULL)
            return get_dest();
        if (get_peer_for_recv() == NULL)
            return get_src();
        return -1;
    }
    virtual PEAT_Rank get_ranks_num() const { return 3; }
};

struct PET_Coll_Event : public PET_Local_Event {
  protected:
    PEAT_Num int_event_index;

  public:
    PEAT_MPI_Comm comm;
    std::vector<PET_Event *> t_events;
    PEAT_Bool force_write;
    PEAT_Signed_Num moved_index, index_event_being_estimated;
    PET_Coll_Event(PET_Core *p) : PET_Local_Event(p), int_event_index(0), comm(0), force_write(false), moved_index(-1), index_event_being_estimated(-1) {}
    virtual ~PET_Coll_Event();
    virtual PEAT_Bool put_t_event(PET_Event *event)
    {
        t_events[event->secondary_id] = event;
        event->host_event = this;
        return true;
    }
    virtual PEAT_Bool split_event();
    PEAT_Signed_Num find_t_event(PET_Event *event)
    {
        PEAT_Num size = t_events.size();
        if (size > 0) {
            for (PEAT_Num i = 0; i < size; i++) {
                if (t_events[i] == event)
                    return (PEAT_Signed_Num)i;
            }
        }
        return -1;
    }

    PEAT_Signed_Num move_events(PEAT_Signed_Num index, PEAT_Clock sub_t, PEAT_Bool pre_move)
    {
        PEAT_Num size = t_events.size();
        if (size > 0) {
            if (moved_index < index || pre_move) {
                for (PEAT_Num i = PEAT_Num(index); i < size; i++) {
                    t_events[i]->move_event(sub_t);
                }
                if (!pre_move)
                    moved_index = index;
            } else {
                pet_throw_error(pet_core, this, NULL, "Duplicate moving request (END_FUNC came twice).\n");
            }
        }
        return -1;
    }

    virtual PEAT_Clock scale_cpu_part(PEAT_Ratio ratio);
    virtual PEAT_Bool safe_to_delete();
    virtual PEAT_Bool can_be_destroyed();
    virtual void notify_before_destroy(PET_Event *event);

    virtual PET_Event_Type get_event_type() const
    {
        assert(0);
        return PET_EVENT_TYPE_COLL;
    }
    virtual PEAT_Bool bind();
    virtual PEAT_Clock move_event(PEAT_Clock sub_t);
    virtual void concatenate_event();
    virtual void scale_event();

    virtual PEAT_Clock remove_tractor_overhead(PEAT_Clock *prolog_overhead = NULL);
    virtual PEAT_Bool destroy()
    {
        int size = (int)t_events.size();
        for (int i = 0; i < size; i++) {
            delete t_events[i];
        }
        return true;
    }
    virtual void set_sub_t(PEAT_Clock t_sub_t_t, PEAT_Clock t_sub_t_ae)
    {
        sub_t_t_t = t_sub_t_t;
        sub_t_ae_t = t_sub_t_ae;
        int size = (int)t_events.size();
        for (int i = 0; i < size; i++) {
            t_events[i]->set_sub_t(t_sub_t_t, t_sub_t_ae);
        }
    }
    virtual PET_Event *get_estimated_event()
    {
#if 0
		int size = ( int ) t_events.size();
		for ( int i = 0; i < size; i++ ) {
			if (t_events[ i ]->estimation_started && !t_events[ i ]->finish_calculations) return t_events[ i ];
			if ( (!t_events[ i ]->estimation_started) && i > 0) return t_events[ i-1 ];
		}
#else
        if (index_event_being_estimated >= 0) {
            return t_events[index_event_being_estimated];
        }
#endif
        return this;
    }
    PEAT_Clock get_event_start()
    {
        int size = (int)t_events.size();
        if (size) {
            if (t_events[0]->is_estimated())
                return t_events[0]->e_time.func.in;
            else
                return -1;
        }
        return e_time.func.in;
    }
    PEAT_Clock get_event_finish()
    {
        int size = (int)t_events.size();
        if (size) {
            if (t_events[size - 1]->is_estimated())
                return t_events[size - 1]->e_time.func.out;
            else
                return -1;
        }
        return e_time.func.in;
    }

    virtual PET_Event *get_first_child_event();
    virtual PET_Event *get_next_child_event();
    virtual PET_Event *get_prev_child_event();
    virtual PET_Event *get_last_child_event();

    PET_Event *get_prev_child_event(PET_Event *event);
    virtual PEAT_Bool is_assembled() const;
    virtual PEAT_Bool is_bound() const;
    virtual PEAT_Bool ready_to_estimate();
    virtual PEAT_Bool estimate(Estimation_Event *event);
    virtual PEAT_Bool is_estimated() const;
    virtual PET_Event *write_event(File_Storage_GZ *gz_MPI, File_Storage_GZ *gz_HWC, File_Storage_GZ *gz_bind, const char *mpi_eol = "\n", const char *hwc_eol = "\n");
    virtual PEAT_Rank get_rank_read_suggestion();
    virtual PEAT_Clock get_overlap() const
    {
        PEAT_Num size = t_events.size();
        PEAT_Clock total_overlap = 0;
        if (size > 0) {
            for (PEAT_Num i = 0; i < size; i++) {
                total_overlap += t_events[i]->get_overlap();
            }
        }
        return total_overlap;
        ;
    }
    virtual PEAT_Clock get_fabric() const
    {
        PEAT_Num size = t_events.size();
        PEAT_Clock total_fabric = 0;
        if (size > 0) {
            for (PEAT_Num i = 0; i < size; i++) {
                total_fabric += t_events[i]->get_fabric();
            }
        }
        return total_fabric;
    }
    virtual PEAT_Clock get_centre_code(Event_Set_Stat::t_stat_type t);
    virtual PET_Event *query_request(PEAT_MPI_Request req)
    {
        int size = (int)t_events.size();
        for (int i = 0; i < size; i++) {
            if (t_events[i]) {
                if (t_events[i]->query_request(req)) {
                    return t_events[i];
                }
            } else {
                break; // this part is not yet filled
            }
        }
        return NULL;
    }
    virtual void post_read_event();
};

struct PET_Collective_Event : public PET_Coll_Event {
    PEAT_Num root;
    PEAT_MPI_Request request;
    PEAT_MPI_Message message;
    PEAT_Collective_Bytes tx_bytes, rx_bytes;
    PET_Event *completion_event;
    std::map<PEAT_Rank, PEAT_Rank> ranks;
    PEAT_Bool block_override;

  public:
    PET_Collective_Event(PET_Core *p) : PET_Coll_Event(p), root(-1), request(0), message(0), completion_event(NULL), block_override(false) { p_control_id = 0; /* for MPI_INIT/FINALIZE */ }
    PET_Collective_Event(const PET_Collective_Event &s) : PET_Coll_Event(s.pet_core), root(s.root), request(s.request), message(s.message),
                                                          completion_event(s.completion_event), block_override(s.block_override)
    {
        func_id = s.func_id;
        primary_id = s.primary_id;
        secondary_id = s.secondary_id;
        p_control_id = s.p_control_id;
    }
    virtual PET_Event *get_completion() const { return completion_event; }
    virtual PEAT_Num check_for_outstanding_progress_messages(PEAT_Clock clk);
    virtual ~PET_Collective_Event();
    PEAT_Clock get_message_finish() const;
    PEAT_Clock get_message_start() const;
    PEAT_Clock get_ack_finish() const;

    PEAT_UID get_highest_uid() const
    {
        return -1;
    }
    PEAT_UID get_lowest_uid() const
    {
        return -1;
    }

    PEAT_Bool is_non_block() const
    {
        return (global_func_id.check_property(func_id, PEAT_PROP_NON_BLOCK) || is_persistent_init()) && !block_override;
    }
    void set_block(PEAT_Bool b) { block_override = b; }

    virtual PET_Event *query_request(PEAT_MPI_Request req)
    {
        if (req == request && get_completion() == NULL)
            return this;
        return PET_Coll_Event::query_request(req);
    }
    virtual PET_Event_Type get_event_type() const { return PET_EVENT_TYPE_COLLECTIVE; }
    virtual PEAT_Bool read_event(PEAT_Rank r, const String_Split &mpi_string, const String_Split &hwc_string, PET_Coll_Event *host);
    virtual void post_read_event();
    virtual PET_Event *write_event(File_Storage_GZ *gz_MPI, File_Storage_GZ *gz_HWC, File_Storage_GZ *gz_bind, const char *mpi_eol = "\n", const char *hwc_eol = "\n");
    PEAT_Bool write_bind(File_Storage_GZ *gz_bind);
    virtual PEAT_Transports get_msg_transport_id() const { return PEAT_TRANSPORT_COLLECTIVE; }
    virtual PEAT_Rank get_ranks_num() const { return (PEAT_Rank)ranks.size(); }

    virtual PEAT_Message_Size get_real_tx_bytes() const;
    virtual PEAT_Message_Size get_real_rx_bytes() const;
    virtual PEAT_Bool completion_received() const
    {
        return !(completion_event == NULL) || request == 0 || is_persistent_init();
    }
    virtual PEAT_Bool ready_to_bind() const
    {
        return completion_received();
    }
    virtual PEAT_Bool is_bound() const;
    virtual PEAT_Bool is_assembled() const;
    virtual void set_completion(PET_Event *event) { completion_event = event; }
    PEAT_Bool safe_to_delete();
    PEAT_Bool can_be_destroyed();
};

struct PET_Comm_Event : public PET_Coll_Event {
    PEAT_MPI_Comm newcomm, intercomm, remotecomm;
    PEAT_MPI_Request request;

  public:
    PET_Comm_Event(PET_Core *p) : PET_Coll_Event(p), newcomm(0), intercomm(0), remotecomm(0), request(0) {}
    virtual PET_Event_Type get_event_type() const { return PET_EVENT_TYPE_COMM; }
    virtual PEAT_Bool read_event(PEAT_Rank r, const String_Split &mpi_string, const String_Split &hwc_string, PET_Coll_Event *host);
    virtual PET_Event *write_event(File_Storage_GZ *gz_MPI, File_Storage_GZ *gz_HWC, File_Storage_GZ *gz_bind, const char *eol = "\n", const char *hwc_eol = "\n");
    virtual void post_read_event();
    virtual PEAT_Bool is_bound() const;
    virtual PEAT_Bool is_assembled() const;
};
struct PET_Request {
    PEAT_Rank src;
    PEAT_MPI_Tag tag;
    PEAT_LNum t_bytes;
    PEAT_Num index;
    PEAT_MPI_Request request;
    PET_Event *event;
    PET_Request() : src(-2), tag(-1), t_bytes(0), index(-1), request(0), event(NULL) {}
};

struct PET_Start_Event : public PET_Coll_Event {
    PEAT_Num input_request_num, triggered_request_num;
    int flag;
    PEAT_Bool is_all_bound, shutdown_initiated;
    PEAT_Addr addr;
    std::vector<PEAT_MPI_Request> input_requests;
    std::map<PEAT_MPI_Request, PET_ToR_Event *> tor_events;
    std::map<PEAT_MPI_Request, PET_Collective_Event *> collective_events;
    std::vector<PET_Event *> p_events;

  public:
    PET_Start_Event(PET_Core *p) : PET_Coll_Event(p), input_request_num(0), triggered_request_num(0), flag(-1), is_all_bound(false), shutdown_initiated(false), addr(NULL) {}
    virtual ~PET_Start_Event();
    virtual PET_Event_Type get_event_type() const { return PET_EVENT_TYPE_START; }
    virtual PEAT_Bool read_event(PEAT_Rank r, const String_Split &mpi_string, const String_Split &hwc_string, PET_Coll_Event *host);
    virtual void scale_event();
    void set_times();
    virtual PET_Event *write_event(File_Storage_GZ *gz_MPI, File_Storage_GZ *gz_HWC, File_Storage_GZ *gz_bind, const char *eol = "\n", const char *hwc_eol = "\n");
    std::map<PEAT_MPI_Request, PET_ToR_Event> inits;
    virtual PEAT_Bool safe_to_delete();
    virtual PEAT_Bool can_be_destroyed();
    PEAT_Bool remove_event(PEAT_MPI_Request req);
    virtual PEAT_Bool bind();
    virtual PEAT_Rank get_rank_read_suggestion();
    virtual PEAT_Bool is_bound() const { return is_all_bound; }
    virtual PEAT_Bool is_assembled() const;
    virtual void notify_before_destroy(PET_Event *event);
    void handle_event(Estimation_Event *event);
    virtual void concatenate_event();
    PEAT_Bool get_init(PEAT_MPI_Request r, PET_ToR_Event &event)
    {
        if (inits.count(r) == 1) {
            event = inits.at(r);
            return true;
        }
        return false;
    };
    virtual PEAT_Clock move_event(PEAT_Clock sub_t)
    {
        PET_Coll_Event::move_event(sub_t);
        int size = (int)p_events.size();
        for (int i = 0; i < size; i++) {
            if (p_events[i]->e_time.func > 0)
                p_events[i]->move_event(sub_t);
        }
        return sub_t;
    }
    virtual PET_Event *get_estimated_event()
    {
        int size = (int)t_events.size();
        if (size > 0) {
            return PET_Coll_Event::get_estimated_event();
        } else {
            return PET_Local_Event::get_estimated_event();
        }
    }
    virtual PEAT_Bool split_event();
    virtual PEAT_Bool estimate(Estimation_Event *event);
    virtual PEAT_Bool ready_to_estimate();

    virtual PEAT_Bool get_first_tor_event(std::map<PEAT_MPI_Request, PET_ToR_Event *>::iterator &evt)
    {
        if (tor_events.empty())
            return false;
        evt = tor_events.begin();
        return true;
    }
    virtual PEAT_Bool get_next_tor_event(std::map<PEAT_MPI_Request, PET_ToR_Event *>::iterator &evt)
    {
        if (++evt == tor_events.end())
            return false;
        return true;
    }
    virtual PET_Event *get_event_from_req(PEAT_MPI_Request req)
    {
        if (tor_events.count(req) == 1) {
            return (tor_events.at(req));
        }
        if (collective_events.count(req) == 1) {
            return (collective_events.at(req));
        }
        assert(0);
        return this;
    }

    virtual PET_Event *query_request(PEAT_MPI_Request req)
    {

        if (tor_events.count(req) == 1) {
            if (tor_events.at(req)->get_completion() == NULL)
                return tor_events.at(req);
        }
        if (collective_events.count(req) == 1) {
            if (collective_events.at(req)->get_completion() == NULL)
                return collective_events.at(req);
        }
        return NULL;
    }
    virtual PET_Event *get_real_event(PET_Event *evt);
    virtual PET_Event *get_first_child_event();
    virtual PET_Event *get_next_child_event();
    virtual PET_Event *get_prev_child_event();
    virtual PET_Event *get_last_child_event();
    virtual PEAT_Message_Size get_tx_bytes() const;
    virtual PEAT_Message_Size get_rx_bytes() const;
    virtual PEAT_Message_Size get_real_tx_bytes() const;
    virtual PEAT_Message_Size get_real_rx_bytes() const;
    virtual PEAT_Bool check_tx() const;
    virtual PEAT_Bool check_rx() const;
    virtual PEAT_Bool check_real_tx() const;
    virtual PEAT_Bool check_real_rx() const;
    virtual PEAT_Bool get_bounds();
    virtual PEAT_Rank get_ranks_num() const
    {
        PEAT_Rank total_ranks = 0;
        PEAT_Signed_Num size = (PEAT_Signed_Num)p_events.size();
        for (int i = 0; i < size; i++) {
            total_ranks += p_events[i]->get_ranks_num();
        }
        return total_ranks;
    }
};
struct Completion_Chain;
struct PET_Completion_Event : public PET_Start_Event {
    PEAT_Bool is_children_overridden, is_chained, end_submitted;
    std::list<PET_Request> triggered_requests;

  public:
    PET_Completion_Event(PET_Core *p) : PET_Start_Event(p), is_children_overridden(false), is_chained(false), end_submitted(false), discarded(false) {}
    virtual PET_Event_Type get_event_type() const { return PET_EVENT_TYPE_COMPLETION; }
    virtual PEAT_Bool read_event(PEAT_Rank r, const String_Split &mpi_string, const String_Split &hwc_string, PET_Coll_Event *host);
    virtual PET_Event *write_event(File_Storage_GZ *gz_MPI, File_Storage_GZ *gz_HWC, File_Storage_GZ *gz_bind, const char *eol = "\n", const char *hwc_eol = "\n");
    virtual PEAT_Bool bind();
    virtual PEAT_Bool ready_to_estimate();
    virtual PEAT_Bool is_estimated() const;
    virtual PEAT_Bool is_assembled() const;
    virtual PEAT_Bool safe_to_delete();
    virtual PEAT_Bool can_be_destroyed();
    virtual PEAT_Bool split_event();
    virtual PEAT_Bool estimate(Estimation_Event *event);
    PEAT_Bool override_children();
    virtual PEAT_Bool is_bound() const { return PET_Coll_Event::is_bound(); }
    PEAT_Bool self_discard();
    PEAT_Bool discarded;
    PEAT_Bool patch_nd_event(Completion_Chain *);
    void handle_event(Estimation_Event *event);
    virtual PEAT_Bool get_bounds();
    virtual PEAT_Bool check_tx() const;
    virtual PEAT_Bool check_rx() const;
    virtual PEAT_Clock get_overlap() const;
    virtual PEAT_Clock get_fabric() const;
    virtual void concatenate_event();
    virtual PEAT_Rank get_rank_read_suggestion();

    virtual PEAT_Num check_for_outstanding_progress_messages(PEAT_Clock clk)
    {
        return handle_progress_messages(clk, true);
    }
    PET_Request *find_triggered_request(PEAT_MPI_Request req)
    {
        for (std::list<PET_Request>::iterator req_it = triggered_requests.begin(); req_it != triggered_requests.end(); ++req_it) {
            if (req_it->request == req)
                return &(*req_it);
        }
        return NULL;
    }

    virtual PET_Event *get_first_child_event()
    {
        return PET_Coll_Event::get_first_child_event();
    };
    virtual PET_Event *get_last_child_event()
    {
        return PET_Coll_Event::get_first_child_event();
    };
    virtual PET_Event *get_next_child_event()
    {
        return PET_Coll_Event::get_next_child_event();
    };
    virtual PET_Event *get_prev_child_event()
    {
        return PET_Coll_Event::get_prev_child_event();
    };
    virtual PEAT_Clock move_event(PEAT_Clock sub_t)
    {
        return PET_Coll_Event::move_event(sub_t);
    }
    virtual PET_Event *query_request(PEAT_MPI_Request req)
    {
        return PET_Coll_Event::query_request(req);
    }

    virtual PEAT_Bool initiate();
};

struct PET_OMPT_Event : public PET_Local_Event {
    std::vector<PET_OMPT_Event *> sections;
    PET_OMPT_Event *pair_event;
    PET_OMPT_Event *t0;
    PEAT_Bool is_all_estimated;
    PEAT_Signed_Num kind, endpoint;
    PEAT_UID parallel_data, task_data;

  public:
    PET_OMPT_Event(PET_Core *p);
    ~PET_OMPT_Event();
    virtual PET_Event_Type get_event_type() const { return PET_EVENT_TYPE_OMPT; }
    virtual PEAT_Bool estimate(Estimation_Event *event);
    virtual PEAT_Bool read_event(PEAT_Rank r_rank, const String_Split &mpi_string, const String_Split &hwc_string, PET_Coll_Event *host);
    virtual PET_Event *write_event(File_Storage_GZ *gz_MPI, File_Storage_GZ *gz_HWC, File_Storage_GZ *gz_bind, const char *eol = "\n", const char *hwc_eol = "\n");
    virtual void insert();
    void insert(PEAT_Bool skip_nested); // special version for children

    void assign_pair_event(PET_OMPT_Event *r) { pair_event = r; };
    void set_t0(PET_OMPT_Event *event) { t0 = event; }
    virtual PEAT_Bool is_estimated() const
    {
        if (is_all_estimated)
            return true;
        if (func_id != OMPT_PARALLEL_END_ID && func_id != OMPT_PARALLEL_BEGIN_ID)
            return PET_Local_Event::is_estimated();
        if (t0)
            return t0->is_estimated();
        if (sections.size() == 1) {
            sections[0]->is_all_estimated = PET_Local_Event::is_estimated();
            return is_all_estimated;
        }
        if (sections.empty()) {
            return PET_Local_Event::is_estimated();
        }
        PEAT_Bool final = true;
        for (std::vector<PET_OMPT_Event *>::const_iterator thread_it = sections.cbegin(); thread_it != sections.cend(); ++thread_it) {
            final = final && (*thread_it)->PET_Local_Event::is_estimated();
        }
        if (final) {
            for (std::vector<PET_OMPT_Event *>::const_iterator thread_it = sections.cbegin(); thread_it != sections.cend(); ++thread_it) {
                (*thread_it)->is_all_estimated = true;
            }
        }
        return final;
    }
    void update_finish();
    virtual PET_OMPT_Event *get_parallel_section_end()
    {
        if (func_id == OMPT_PARALLEL_END_ID)
            return this;
        if (func_id == OMPT_PARALLEL_BEGIN_ID)
            return pair_event;
        return PET_Event::get_parallel_section_end();
    }
    virtual PET_Event *get_prev_event(PEAT_Bool hint);
    PET_Event *get_last_event();
    virtual PEAT_Bool bind()
    {
        if (func_id == OMPT_PARALLEL_BEGIN_ID && get_parallel_section_end() == NULL)
            return false;
        return true;
    };
    virtual PEAT_Bool ready_to_estimate()
    {
        if (func_id == OMPT_PARALLEL_BEGIN_ID && thread_id > 0 && t0)
            return t0->ready_to_estimate();
        // not needed since already checking in bind()
        // if (func_id == OMPT_PARALLEL_BEGIN_ID) if (parallel_section_end == NULL) return false;
#if 0 // disable it for a while
		if (func_id != OMPT_THREAD_END_ID && func_id != OMPT_THREAD_BEGIN_ID &&
			func_id != OMPT_NESTED_PARALLEL_BEGIN_ID && func_id != OMPT_NESTED_PARALLEL_END_ID &&
			func_id != OMPT_PARALLEL_BEGIN_ID && parallel_section_begin == NULL &&
			thread_id > 0) {
			return true;//no omp clauses outside parallel regions, please 
		}
#endif
        return PET_Local_Event::ready_to_estimate();
    }
    PEAT_Bool can_be_destroyed();
    virtual PEAT_Bool initiate();
    virtual PEAT_Rank get_rank_read_suggestion()
    {
        if (func_id == OMPT_PARALLEL_BEGIN_ID && pair_event == NULL)
            return rank;
        return -1;
    }
    virtual PEAT_MPI_Func_ID missing_event()
    {
        if (func_id == OMPT_PARALLEL_BEGIN_ID && pair_event == NULL)
            return OMPT_PARALLEL_END_ID;
        return TMPI_FUNC_ID_NONE;
    }
    friend class PET_Core;
};

PEAT_Bool check_peer(PET_Event *send, PET_Event *recv);
PEAT_Bool is_wild_card(PET_Event *evt);

#endif // __PET_EVENT_H__
