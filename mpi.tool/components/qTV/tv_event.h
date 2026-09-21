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

#ifndef __TV_EVENT_H__
#define __TV_EVENT_H__

#include "../common/common_types.h"
#include "../common/func_id.h"
#include "globals.h"
#include <assert.h>

enum TV_Event_Type {
    TV_EVENT_TYPE_NONE = 0,
    TV_EVENT_TYPE_LOCAL,
    TV_EVENT_TYPE_LOCAL_BUF,
    TV_EVENT_TYPE_FAKE,
    TV_EVENT_TYPE_COLL,
    TV_EVENT_TYPE_COLLECTIVE,
    TV_EVENT_TYPE_COMM,
    TV_EVENT_TYPE_TOR,
    TV_EVENT_TYPE_TAR,
    TV_EVENT_TYPE_START,
    TV_EVENT_TYPE_COMPLETION,
    TV_EVENT_TYPE_OMPT_LOCAL,
    TV_EVENT_TYPE_OMPT_TARGET_DATA,
    TV_EVENT_TYPE_OMPT_PARALLEL,
    TV_EVENT_TYPE_GPU_LOCAL,
    TV_EVENT_TYPE_GPU_TRANSFER,
    TV_EVENT_TYPE_GPU_EXECUTION,
    TV_EVENT_TYPE_MAX
};
#if 0
//debug function
static PEAT_Bool equal(TV_Event_Type send, TV_Event_Type recv)
{
	if (send == recv) return true;
	if (send == TV_EVENT_TYPE_TAR && recv == TV_EVENT_TYPE_START) return true;
	if (recv == TV_EVENT_TYPE_TAR && send == TV_EVENT_TYPE_START) return true;
	return false;
}
#endif
struct TV_Coll_Event;

struct TV_Event {
    PEAT_Time e_time;
    PEAT_MPI_Func_ID func_id;
    PEAT_UID primary_id, secondary_id;
    PEAT_Rank rank;
    PEAT_Thread thread;
    Event_Address_Info *addrs;

  public:
    virtual TV_Event_Type get_event_type() const = 0; // { assert(0); return TV_EVENT_TYPE_NONE; }
    virtual int get_event_group() const = 0;          // { assert(0); return 0; }
    TV_Event();
    virtual ~TV_Event();
    virtual PEAT_Bool read_event(PEAT_Rank r, const String_Split &mpi_string, const String_Split &hwc_string, const PEAT_Time& evt_time, const PEAT_Num num_addrs);
    virtual PEAT_Bool check_tx() const
    {
        return (global_func_id.check_property(func_id, PEAT_PROP_DATA_TX) || global_func_id.check_property(func_id, PEAT_PROP_DATA_TXRX));
    }
    virtual PEAT_Bool check_rx() const
    {
        return (global_func_id.check_property(func_id, PEAT_PROP_DATA_RX) || global_func_id.check_property(func_id, PEAT_PROP_DATA_TXRX));
    }
    virtual PEAT_Message_Size get_tx_bytes() const { return -1; };
    virtual PEAT_Message_Size get_rx_bytes() const { return -1; };
    virtual PEAT_Message_Size get_tx_bytes(PEAT_Rank) const { return -1; };
    virtual PEAT_Message_Size get_rx_bytes(PEAT_Rank) const { return -1; };
    virtual PEAT_Clock get_message_draw_start() const { return e_time.func.in; }
    virtual PEAT_Clock get_message_draw_end() const { return e_time.func.out; }
    virtual PEAT_Rank get_root() const { return -1; };

    virtual const TV_Event *get_child(int f) const { return NULL; }
    ALT_ALLOCATOR;
    virtual bool is_mpi() const { return true; }
    virtual bool is_omp() const { return false; }
    virtual bool is_gpu() const { return false; }
};

struct TV_Local_Event : public TV_Event {
    PEAT_Signed_Num p_control_id;

  public:
    virtual int get_event_group() const { return 0; }
    virtual TV_Event_Type get_event_type() const { return TV_EVENT_TYPE_LOCAL; }
    TV_Local_Event() : p_control_id(0), TV_Event() {}
    virtual PEAT_Bool read_event(PEAT_Rank r, const String_Split &mpi_string, const String_Split &hwc_string, const PEAT_Time& evt_time, const PEAT_Num num_addrs);
};

struct TV_Local_Buf_Event : public TV_Local_Event {
    PEAT_Signed_Num inbuf_size, outbuf_size;

  public:
    virtual TV_Event_Type get_event_type() const { return TV_EVENT_TYPE_LOCAL_BUF; }
    TV_Local_Buf_Event() : TV_Local_Event(), inbuf_size(-1), outbuf_size(-1) {}
    virtual PEAT_Bool read_event(PEAT_Rank r, const String_Split &mpi_string, const String_Split &hwc_string, const PEAT_Time& evt_time, const PEAT_Num num_addrs);
};
struct TV_ToR_Event : public TV_Local_Event {
    PEAT_LNum t_bytes;
    PEAT_Rank peer_rank;
    PEAT_MPI_Tag tag;
    PEAT_MPI_Comm comm;
    PEAT_MPI_Request request;
    PEAT_MPI_Message message;
    TV_Event *aux_event, *aux_event_2;

  public:
    virtual TV_Event_Type get_event_type() const { return TV_EVENT_TYPE_TOR; }
    TV_ToR_Event() : TV_Local_Event(), comm(0), t_bytes(0), peer_rank(-1), tag(-1), request(0), message(0),
                     aux_event(NULL), aux_event_2(NULL)
    {
    }

    TV_ToR_Event(const TV_ToR_Event &s) : TV_Local_Event(), comm(s.comm), t_bytes(s.t_bytes), peer_rank(s.peer_rank), tag(s.tag),
                                          request(s.request), message(s.message), aux_event(s.aux_event), aux_event_2(s.aux_event_2)
    {
        func_id = s.func_id;
        primary_id = s.primary_id;
        secondary_id = s.secondary_id;
        rank = s.rank;
        thread = s.thread;
    }
    virtual PEAT_Bool read_event(PEAT_Rank r, const String_Split &mpi_string, const String_Split &hwc_string, const PEAT_Time& evt_time, const PEAT_Num num_addrs);
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
};

struct TV_TaR_Event : public TV_ToR_Event {
    PEAT_LNum t_bytes_2;
    PEAT_MPI_Tag tag_2;
    PEAT_Bool process_second_line;
    // for RX part
    PEAT_Rank peer_rank_2;
    PEAT_UID peer_primary_id_2, peer_secondary_id_2;
    PEAT_MPI_Func_ID peer_func_id_2, peer_parent_func_id_2;

  public:
    TV_TaR_Event() : TV_ToR_Event(), process_second_line(false),
                     peer_primary_id_2(-1), peer_secondary_id_2(-1), peer_func_id_2(TMPI_FUNC_ID_NONE), peer_parent_func_id_2(TMPI_FUNC_ID_NONE),
                     t_bytes_2(0), peer_rank_2(-1), tag_2(-1)
    {
    }
    TV_TaR_Event(const TV_TaR_Event &s) : TV_ToR_Event(), peer_primary_id_2(s.peer_primary_id_2), peer_secondary_id_2(s.peer_secondary_id_2),
                                          peer_func_id_2(s.peer_func_id_2), peer_parent_func_id_2(s.peer_parent_func_id_2), tag_2(s.tag_2), peer_rank_2(s.peer_rank_2), process_second_line(s.process_second_line), t_bytes_2(s.t_bytes_2)
    {
    }
    virtual TV_Event_Type get_event_type() const { return TV_EVENT_TYPE_TAR; }
    virtual PEAT_Bool read_event(PEAT_Rank r, const String_Split &mpi_string, const String_Split &hwc_string, const PEAT_Time& evt_time, const PEAT_Num num_addrs);
    virtual PEAT_Message_Size get_tx_bytes() const { return t_bytes; };
    virtual PEAT_Message_Size get_rx_bytes() const { return t_bytes_2; };
};

struct TV_Coll_Event : public TV_Local_Event {
  protected:
    PEAT_Num int_event_index;

  public:
    virtual TV_Event_Type get_event_type() const
    {
        assert(0);
        return TV_EVENT_TYPE_COLL;
    }
    PEAT_MPI_Comm comm;
    std::vector<TV_Event *> t_events;
    TV_Coll_Event() : TV_Local_Event(), comm(0), int_event_index(0) {}
    virtual ~TV_Coll_Event();
    TV_Event *find_event(TV_Event *event)
    {
        if (event->primary_id == primary_id) {
            return t_events[event->secondary_id];
        }
        return NULL;
    }
    TV_Event *find_event(PEAT_UID secondary_id)
    {
        return t_events[secondary_id];
    }
    PEAT_Bool put_event(TV_Event *event)
    {
        if (event->primary_id == primary_id) {
            t_events[event->secondary_id] = event;
            return true;
        }
        return false;
    }
    virtual const TV_Event *get_child(int f) const
    {
        if (t_events.size() > f)
            return t_events[f];
        else
            return NULL;
    }
};
struct TV_Collective_Bytes {
    PEAT_Collective_Bytes tx_bytes, rx_bytes;
    bool operator==(const TV_Collective_Bytes s)
    {
        return (tx_bytes == s.tx_bytes) && (rx_bytes == s.rx_bytes);
    }
    void clear()
    {
        tx_bytes.clear();
        rx_bytes.clear();
    }
};

struct TV_Collective_Event : public TV_Coll_Event {
    PEAT_Rank root;
    PEAT_MPI_Request request;
    PEAT_MPI_Message message;
    TV_Collective_Bytes bytes;
    TV_Collective_Bytes *msg_bytes;
    TV_Event *completion_event;
    std::map<PEAT_Rank, PEAT_Rank> ranks;

  public:
    TV_Collective_Event() : TV_Coll_Event(), root(-1), request(0), message(0), completion_event(NULL), msg_bytes(NULL) { p_control_id = 0; /* for MPI_INIT/FINALIZE */ }
    TV_Collective_Event(const TV_Collective_Event &s) : TV_Coll_Event(), root(s.root),
                                                        request(s.request), message(s.message), completion_event(s.completion_event)
    {
        func_id = s.func_id;
        primary_id = s.primary_id;
        secondary_id = s.secondary_id;
        p_control_id = s.p_control_id;
        msg_bytes = s.msg_bytes;
        bytes = s.bytes;
    }
    virtual TV_Event_Type get_event_type() const { return TV_EVENT_TYPE_COLLECTIVE; }
    virtual PEAT_Bool read_event(PEAT_Rank r, const String_Split &mpi_string, const String_Split &hwc_string, const PEAT_Time& evt_time, const PEAT_Num num_addrs);
    virtual PEAT_Rank get_root() const { return root; };
    virtual PEAT_Message_Size get_tx_bytes(PEAT_Rank rank) const
    {
        if (msg_bytes) {
            return msg_bytes->tx_bytes.get_rank_bytes(rank);
        } else {
            return bytes.tx_bytes.get_rank_bytes(rank);
        }
    };
    virtual PEAT_Message_Size get_rx_bytes(PEAT_Rank rank) const
    {
        if (msg_bytes) {
            return msg_bytes->rx_bytes.get_rank_bytes(rank);
        } else {
            return bytes.rx_bytes.get_rank_bytes(rank);
        }
    };
    virtual PEAT_Message_Size get_tx_bytes() const
    {
        if (msg_bytes) {
            return msg_bytes->tx_bytes.get_total();
        } else {
            return bytes.tx_bytes.get_total();
        }
    }
    virtual PEAT_Message_Size get_rx_bytes() const
    {
        if (msg_bytes) {
            return msg_bytes->rx_bytes.get_total();
        } else {
            return bytes.rx_bytes.get_total();
        }
    }
    void print_tx_bytes(std::ostringstream &str)
    {
        if (msg_bytes) {
            str << msg_bytes->tx_bytes;
        } else {
            str << bytes.tx_bytes;
        }
    }
    void print_rx_bytes(std::ostringstream &str)
    {
        if (msg_bytes) {
            str << msg_bytes->rx_bytes;
        } else {
            str << bytes.rx_bytes;
        }
    }

    virtual PEAT_Clock get_message_draw_start() const { return (e_time.func.in + e_time.func.out) / 2; } // center for collectives
    virtual PEAT_Clock get_message_draw_end() const { return (e_time.func.in + e_time.func.out) / 2; }   // center for collectives
};

struct TV_Comm_Event : public TV_Coll_Event {
    PEAT_MPI_Comm newcomm, remotecomm, intercomm;
    PEAT_MPI_Request request;

  public:
    TV_Comm_Event() : TV_Coll_Event(), request(0), newcomm(0), intercomm(0), remotecomm(0) {}
    virtual TV_Event_Type get_event_type() const { return TV_EVENT_TYPE_COMM; }
    virtual PEAT_Bool read_event(PEAT_Rank r, const String_Split &mpi_string, const String_Split &hwc_string, const PEAT_Time& evt_time, const PEAT_Num num_addrs);
};
struct TV_Request {
    PEAT_Rank src;
    PEAT_MPI_Tag tag;
    PEAT_LNum t_bytes;
    PEAT_Num index;
    PEAT_MPI_Request request;
    TV_Request() : src(-2), tag(-1), t_bytes(0), index(-1), request(0) {}
};

struct TV_Start_Event : public TV_Coll_Event {
    PEAT_Num input_request_num, triggered_request_num;
    int flag;
    PEAT_Addr addr;
    std::vector<PEAT_MPI_Request> input_requests;
    std::map<PEAT_MPI_Request, TV_ToR_Event *> tor_events;
    std::map<PEAT_MPI_Request, TV_Collective_Event *> collective_events;
    std::vector<TV_Event *> p_events;

  public:
    TV_Start_Event() : TV_Coll_Event(), input_request_num(0), triggered_request_num(0), flag(-1), addr(NULL) {}
    virtual ~TV_Start_Event();

    virtual TV_Event_Type get_event_type() const { return TV_EVENT_TYPE_START; }
    virtual PEAT_Bool read_event(PEAT_Rank r, const String_Split &mpi_string, const String_Split &hwc_string, const PEAT_Time& evt_time, const PEAT_Num num_addrs);
};
struct TV_Completion_Event : public TV_Start_Event {
    std::list<TV_Request> triggered_requests;

  public:
    TV_Completion_Event() : TV_Start_Event() {}
    virtual TV_Event_Type get_event_type() const
    {
        if (flag == 0)
            return TV_EVENT_TYPE_LOCAL;
        else
            return TV_EVENT_TYPE_COMPLETION;
    }
    virtual PEAT_Bool read_event(PEAT_Rank r, const String_Split &mpi_string, const String_Split &hwc_string, const PEAT_Time& evt_time, const PEAT_Num num_addrs);
};

struct TV_OMPT_Event : public TV_Local_Event {
  public:
    virtual int get_event_group() const { return 1; }
    virtual TV_Event_Type get_event_type() const
    {
        if (func_id == OMPT_PARALLEL_ID)
            return TV_EVENT_TYPE_OMPT_PARALLEL;
        if (func_id == OMPT_TARGET_EMI_ID || func_id == OMPT_TARGET_DATA_OP_EMI_ID || func_id == OMPT_TARGET_SUBMIT_EMU_ID || func_id == OMPT_TARGET_MAP_EMI_ID)
            return TV_EVENT_TYPE_OMPT_TARGET_DATA;
        return TV_EVENT_TYPE_OMPT_LOCAL;
    }
    TV_OMPT_Event() : TV_Local_Event() {}
    virtual PEAT_Bool read_event(PEAT_Rank r, const String_Split &mpi_string, const String_Split &hwc_string, const PEAT_Time& evt_time, const PEAT_Num num_addrs);
    virtual bool is_mpi() const { return false; }
    virtual bool is_omp() const { return true; }
    virtual bool is_gpu() const { return false; }
};

struct TV_GPU_Event : public TV_Local_Event {
  public:
    virtual int get_event_group() const { return 2; }
    virtual TV_Event_Type get_event_type() const { return TV_EVENT_TYPE_GPU_LOCAL; }
    TV_GPU_Event() : TV_Local_Event() {}
    virtual PEAT_Bool read_event(PEAT_Rank r, const String_Split &mpi_string, const String_Split &hwc_string, const PEAT_Time& evt_time, const PEAT_Num num_addrs);
    virtual bool is_mpi() const { return false; }
    virtual bool is_omp() const { return false; }
    virtual bool is_gpu() const { return true; }
};

struct TV_Message {
    TV_Event *from;
    TV_Event *to;
    PEAT_Rank rank;
    PEAT_UID index;
    TV_Pen_Message_Type msg_type;
    TV_Message() : from(NULL), to(NULL), msg_type(TV_PEN_MESSAGE_TYPE_NONE), rank(-1), index(-1) {};
};

#endif
