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

#ifndef __PET_FABRIC_EVENT_H__
#define __PET_FABRIC_EVENT_H__

#include "common_types.h"
#include <assert.h>

enum PEAT_Transfer_Type {
    PEAT_TT_UNDEFFINED,
    PEAT_TT_INTRA,
    PEAT_TT_INTER,
    PEAT_TT_BOTH,
    PEAT_TT_MAX
};
struct PET_Fabric_Event : public PET_Time_Event {
    PEAT_UID primary_id;
    PEAT_MPI_Func_ID func_id;
    PEAT_Clock total_clocks_bitween_calls;
    PEAT_LNum l_num_calls;
    PEAT_Thread thread;
    PET_Fabric_Event() : primary_id(-1), func_id(TMPI_FUNC_ID_NONE), total_clocks_bitween_calls(-1), l_num_calls(-1), thread(-1) {}

  public:
    PEAT_Clock remove_tractor_everhead()
    {
        PEAT_Clock overhead = e_time.mpi.in - e_time.func.in + e_time.func.out - e_time.mpi.out;
        e_time.func.out -= overhead;
        e_time.mpi.in = e_time.func.in;
        e_time.mpi.out = e_time.func.out;
        return overhead;
    }
    PEAT_Bool check_and_split(PEAT_Time &bounds)
    {
        if ( // o_time.func.in > bounds.func.in &&
            o_time.func.out < bounds.func.out)
            return true;
        return false;
    }
    PEAT_Transfer_Type get_t_type() const
    {
        switch (func_id) {
        case SMPI_CHECK_AVAIL_ID:
        case SMPI_COMPLETE_SEND_ID:
        case SMPI_COMPLETE_RECV_ID:
        case PROCESS_VM_READV_ID:
        case PROCESS_VM_WRITEV_ID:
        case MPIDI_CH3I_SMP_PROCESS_HEADER_ID:
            return PEAT_TT_INTRA;
        case IBV_POST_SEND_ID:
        case IBV_POST_SRQ_RECV_ID:
        case IBV_POST_RECV_ID:
        case IBV_POLL_CQ_ID:
        case IBV_QUERY_QP_ID:
            return PEAT_TT_INTER;
        case PSM2_MQ_SEND_ID:
        case PSM2_MQ_SEND2_ID:
        case PSM2_MQ_ISEND_ID:
        case PSM2_MQ_ISEND2_ID:
        case PSM2_MQ_IRECV_ID:
        case PSM2_MQ_IRECV2_ID:
        case PSM2_MQ_IMRECV_ID:
        case PSM2_MQ_IPEEK2_ID:
        case PSM2_MQ_IPEEK_ID:
        case PSM2_MQ_WAIT_ID:
        case PSM2_MQ_WAIT2_ID:
        case PSM2_MQ_TEST_ID:
        case PSM2_MQ_TEST2_ID:
        case PSM3_MQ_SEND2_ID:
        case PSM3_MQ_ISEND2_ID:
        case PSM3_MQ_IRECV2_ID:
        case PSM3_MQ_IMRECV_ID:
        case PSM3_MQ_IPEEK2_ID:
        case PSM3_MQ_WAIT2_ID:
        case PSM3_MQ_TEST2_ID:
        case UCP_TAG_SEND_NB_ID:
        case UCP_TAG_SEND_NBR_ID:
        case UCP_TAG_SEND_SYNC_NB_ID:
        case UCP_TAG_RECV_NB_ID:
        case UCP_TAG_MSG_RECV_NB_ID:
        case UCP_WORKER_PROGRESS_ID:
        case UCP_WORKER_WAIT_ID:
        case UCP_REQUEST_CHECK_STATUS_ID:
        case UCP_REQUEST_TEST_ID:
            return PEAT_TT_BOTH;
        case FI_SEND_ID:
        case FI_TSEND_ID:
        case FI_RECV_ID:
        case FI_TRECV_ID:
        case FI_CQ_READ_ID:
        case FI_CQ_SREAD_ID:
            return PEAT_TT_INTER;
        default:
            break;
        }
        return PEAT_TT_UNDEFFINED;
    }
    PEAT_Bool is_send() const
    {
        if (func_id == PSM2_MQ_SEND_ID || func_id == PSM2_MQ_SEND2_ID || func_id == PROCESS_VM_WRITEV_ID || func_id == SMPI_COMPLETE_SEND_ID || func_id == SMPI_CHECK_AVAIL_ID || func_id == PSM3_MQ_SEND2_ID)
            return true;
        return false;
    }
    PEAT_Bool is_recv() const
    {
        if (func_id == PROCESS_VM_READV_ID || func_id == SMPI_COMPLETE_RECV_ID)
            return true;
        return false;
    }
    PEAT_Bool is_ack() const
    {
        if (func_id == MPIDI_CH3I_SMP_PROCESS_HEADER_ID)
            return true;
        return false;
    }
    PEAT_Bool is_isend() const
    {
        if (func_id == PSM2_MQ_ISEND_ID || func_id == PSM2_MQ_ISEND2_ID || func_id == IBV_POST_SEND_ID || func_id == PSM3_MQ_ISEND2_ID || func_id == UCP_TAG_SEND_NB_ID || func_id == UCP_TAG_SEND_NBR_ID || func_id == UCP_TAG_SEND_SYNC_NB_ID || func_id == FI_SEND_ID || func_id == FI_TSEND_ID)
            return true;
        return false;
    }
    PEAT_Bool is_irecv() const
    {
        if (func_id == PSM2_MQ_IRECV_ID || func_id == PSM2_MQ_IRECV2_ID || func_id == PSM2_MQ_IMRECV_ID || func_id == IBV_POST_RECV_ID || func_id == IBV_POST_SRQ_RECV_ID || func_id == PSM3_MQ_IRECV2_ID || func_id == PSM3_MQ_IMRECV_ID || func_id == UCP_TAG_RECV_NB_ID || func_id == UCP_TAG_MSG_RECV_NB_ID || func_id == FI_RECV_ID || func_id == FI_TRECV_ID)
            return true;
        return false;
    }

    PEAT_Bool is_peek() const
    {
        if (func_id == PSM2_MQ_IPEEK2_ID || func_id == PSM2_MQ_IPEEK_ID || func_id == PSM3_MQ_IPEEK2_ID)
            return true;
        return false;
    }
    PEAT_Bool is_poll() const
    {
        if (func_id == IBV_POLL_CQ_ID || func_id == UCP_WORKER_PROGRESS_ID || func_id == FI_CQ_READ_ID)
            return true;
        return false;
    }
    PEAT_Bool is_notify() const
    {
        if (func_id == IBV_REQ_NOTIFY_CQ_ID || func_id == UCP_WORKER_ARM_ID || func_id == FI_TRYWAIT_ID)
            return true;
        return false;
    }
    PEAT_Bool is_probe() const
    {
        if (func_id == PSM2_MQ_IPROBE_ID || func_id == PSM2_MQ_IPROBE2_ID || func_id == PSM2_MQ_IMPROBE_ID || func_id == PSM2_MQ_IMPROBE2_ID || func_id == PSM3_MQ_IPROBE2_ID || func_id == PSM3_MQ_IMPROBE2_ID || func_id == UCP_TAG_PROBE_NB_ID)
            return true;
        return false;
    }
    PEAT_Bool is_wait() const
    {
        if (func_id == PSM2_MQ_WAIT_ID || func_id == PSM2_MQ_WAIT2_ID || func_id == IBV_GET_CQ_EVENT_ID || func_id == MPIDI_CH3I_SMP_PROCESS_HEADER_ID || func_id == PSM3_MQ_WAIT2_ID || func_id == UCP_WORKER_WAIT_ID || func_id == FI_CQ_SREAD_ID)
            return true;
        return false;
    }
    PEAT_Bool is_test() const
    {
        if (func_id == PSM2_MQ_TEST_ID || func_id == PSM2_MQ_TEST2_ID || func_id == PSM3_MQ_TEST2_ID || func_id == UCP_REQUEST_CHECK_STATUS_ID || func_id == UCP_REQUEST_TEST_ID)
            return true; // || func_id == IBV_POST_SRQ_RECV_ID
        return false;
    }
    PEAT_Bool is_any_rx() const
    {
        return is_recv() || is_irecv() || is_peek() || is_probe() || is_test() || is_wait() || is_poll() || is_notify();
    }
    PEAT_Bool is_any_tx() const
    {
        return is_send() || is_isend() || is_peek() || is_test() || is_wait() || is_poll() || is_notify();
    }
    PEAT_Bool is_tx() const
    {
        return is_send() || is_isend();
    }
    PEAT_Bool is_rx() const
    {
        return is_recv() || is_irecv();
    }
    PEAT_Bool is_non_block() const
    {
        return !(is_send() || is_wait());
    }
};

#endif // __PET_FABRIC_EVENT_H__