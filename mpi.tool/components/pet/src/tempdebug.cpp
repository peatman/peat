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

#include <algorithm>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>

#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <stdlib.h>

#ifdef PEAT_MPI
#include <mpi.h>
#endif
#include "pet_core.h"
#include "pet_event.h"
#include "te_timeline.h"
#include <stdio.h>
static const char *ee_event_info[] = {
    "EET_NONE",
    "EET_FUNC_START",
    "EET_FUNC_END",
    "EET_RTS",
    "EET_RTS_RECV_NOTIFY",
    "EET_RTS_RECV",
    "EET_CTS",
    "EET_CTS_RECV_NOTIFY",
    "EET_CTS_RECV",
    "EET_MSG_TX",
    "EET_MSG_RECV_NOTIFY",
    "EET_MSG_RECV",
    "EET_ACK",
    "EET_ACK_RECV_NOTIFY",
    "EET_ACK_RECV",
    "EET_NOTIFY",
    "EET_CHILD_COMPLETED",
    "EET_PROGRESS_NOTIFY",
    "EET_MAX"};
static const char *te_event_info[] = {
    "None",
    "RTS",
    "RTS_R",
    "CTS",
    "CTS_R",
    "TX",
    "RX",
    "ACK",
    "ACK_R",
    "MAX"};

void pet_throw_error(const void *pet_core_ptr, const void *event_ptr, const void *aux_event_ptr, const char *str)
{
    const PET_Core *pet_core = static_cast<const PET_Core *>(pet_core_ptr);
    const PET_Event *event = static_cast<const PET_Event *>(event_ptr);
    const TE_Event *te_event = static_cast<const TE_Event *>(aux_event_ptr);
    const Estimation_Event *ee_event = static_cast<const Estimation_Event *>(aux_event_ptr);
    perror(str);
}
void pet_warning(const void *pet_core_ptr, const void *event_ptr, const char *str)
{
    const PET_Core *pet_core = static_cast<const PET_Core *>(pet_core_ptr);
    const PET_Event *event = static_cast<const PET_Event *>(event_ptr);
    fprintf(stderr, str);
}

void Estimation_Event::print(FILE *fp, const char *str)
{
    fprintf(fp, str);
    fprintf(fp, " %s:%p:%lld:%lld", ee_event_info[(int)type], this, start, order_id);
    if (owner) {
        fprintf(fp, " -o- %s:%d(%d):%lld:%lld:%lld -s- %s:%d(%d):%lld:%lld:%lld", global_func_id.name_by_idx(owner->func_id), owner->rank, owner->thread_id, owner->primary_id, owner->secondary_id, owner->remaining_ee_events,
                global_func_id.name_by_idx(submiter->func_id), submiter->rank, submiter->thread_id, submiter->primary_id, submiter->secondary_id, submiter->remaining_ee_events);
    }
    if (info.owner) {
        info.print(fp, " <-> ");
    } else {
        fprintf(fp, "\n");
    }
    fflush(fp);
}
void TE_Event::print(FILE *fp, const char *str)
{
    fprintf(fp, str);
    fprintf(fp, " %s:%lld:%lld:%lld:%p:%lld:%lld:%lld:%lld:%lld:%lld", te_event_info[(int)type], size, num_units, remainder, this, order_id, global_order_id, completed_id, uniq_id, start, finish);
    if (owner) {
        fprintf(fp, " -o- %s:%d(%d):%lld:%lld:", global_func_id.name_by_idx(owner->func_id), owner->rank, owner->thread_id, owner->primary_id, owner->secondary_id);
    }
    fprintf(fp, " \n");
    fflush(fp);
}

const PEAT_Ratio round_val = 0.9;
std::map<PEAT_Message_Size, PEAT_Trasport_Environment> envs[PET_Transport_Selector_MAX + 1];

#define fwd_scale_cpu_time (0.9)
#define bwd_scale_cpu_time (1.0 - fwd_scale_cpu_time)
#define RDV_SCALE_FACTOR_1 1.0
#define RDV_SCALE_FACTOR_2 1.0
PEAT_Message_Size get_effective_size(PEAT_Transports t, PEAT_Message_Size message_size)
{
    switch (t) {
    case PEAT_TRANSPORT_DDR:
    case PEAT_TRANSPORT_HBM:
    case PEAT_TRANSPORT_QPI:
        if (message_size <= 64)
            return 64;
        return message_size - message_size % 64 + 64 * ((message_size % 64) > 0);
        break;
    case PEAT_TRANSPORT_NIC:
        if (message_size < 8)
            return 8;
        if (message_size <= 64)
            return 64;
        // message_size += 56;
        return message_size - message_size % 64 + 64 * ((message_size % 64) > 0);
        break;
    default:
        assert(0);
    }
    return message_size;
}
struct PEAT_Fabric {
    PEAT_Clock time = 0;
    PEAT_Bool is_acked = false;
};
#if 0
void PET_Core::write_transport_profile(const std::string &name, const Event_Set_Stat::t_stat_type type)
{
	std::map<PEAT_Rank, std::string> file_name;
	file_name[0] = name;
	File_Storage_GZ profile_file(file_name, PEAT_FIO_FLAG_WRITE , globals.verbose);
	if (!profile_file.is_file_good(0)) {
		std::ostringstream tmp;
		tmp << "Can't open file (to write) " << name;
		throw std::runtime_error(tmp.str());
	}
	gzFile prof_file_ptr = profile_file.get_file_ptr(0);
	PEAT_BW ideal_bw_corrected = -1;
	start_over:
	for ( PEAT_Transports t = PEAT_TRANSPORT_DDR; t < PEAT_TRANSPORT_MAX; t = t_inc( t ) ) {
		if ( t == PEAT_TRANSPORT_COLLECTIVE ) continue;
		std::map<PEAT_Func_ID_Pair, std::map<PEAT_Rank, std::map<PEAT_Message_Size, std::map<PEAT_Message_Size, std::list<MPI_Event_Stat> > > > >&stat_local =  stat.get_ts_fs_stat( type,t );
		if ( stat_local.empty() ) continue;
		std::map<PEAT_Func_ID_Pair, std::map < PEAT_Message_Size, MPI_Event_Stat> > working_set;
		PEAT_BW ideal_bw = 0;
		PEAT_Clock general_lat = 20;
		bool is_smp = true;
		PEAT_Num numa_ppn = ptp->get_numa_pp( 0, 0);
		PEAT_Trasport_Environment  env_t=transport->get_env( t, stat_local.cbegin()->first.id, stat_local.cbegin()->first.parent_id, 0, numa_ppn, globals.transport_selector );
		switch ( t ) {
			case PEAT_TRANSPORT_DDR:
				general_lat = 20;
				is_smp = true;
				break;
			case PEAT_TRANSPORT_HBM:
				general_lat = 20;
				is_smp = true;
				break;
			case PEAT_TRANSPORT_QPI:
				general_lat = 20;
				is_smp = true;
				break;
			case PEAT_TRANSPORT_NIC:
				general_lat = 20;
				is_smp = false;
				break;
			default:assert( 0 );
		}
		ideal_bw = env_t.general_bw;
		if ( ideal_bw_corrected > 0 )ideal_bw = ideal_bw_corrected;
		//ideal_bw = ideal_bw * 1000000000 / ptp->get_trace_freq(0);
		std::map < PEAT_Func_ID_Pair, std::vector<PEAT_Message_Size>> switchover_threshold;
		std::map < PEAT_Func_ID_Pair, std::vector<PEAT_Clock>> cpu_times;
		std::map < PEAT_Func_ID_Pair, std::vector < PEAT_Message_Size >> degradation_thresholds;
		std::map < PEAT_Func_ID_Pair, std::vector < PEAT_BW>> degradation_bw;
		std::map < PEAT_Func_ID_Pair, PEAT_BW > top_bw;
		std::map < PEAT_Func_ID_Pair, PEAT_Message_Size > ms_threshold;
		std::map < PEAT_Func_ID_Pair, std::map< PEAT_Message_Size,PEAT_Fabric>> fabric_times;
#define CUT_THRESHOLD (1.5)

		for ( std::map < PEAT_Func_ID_Pair, std::map<PEAT_Rank, std::map<PEAT_Message_Size, std::map<PEAT_Message_Size, std::list<MPI_Event_Stat> > > > >::const_iterator func_stat_it = stat_local.cbegin(); func_stat_it != stat_local.cend(); ++func_stat_it ) {
			PEAT_Clock last_cpu_time = 0;
			PEAT_Func_ID_Pair fid_pair = func_stat_it->first;
			PEAT_MPI_Func_ID func_id = fid_pair.id;
			PEAT_MPI_Func_ID parent_func_id = fid_pair.parent_id;
			if ( !(global_func_id.check_tx(func_id) && global_func_id.check_tx(parent_func_id)) ) continue;
			//switchover_threshold.clear(); cpu_times.clear(); degradation_thresholds.clear(); fabric_times.clear(); top_bw = 0;
			PEAT_Bool bw_increased = false;
			for ( std::map < PEAT_Rank, std::map<PEAT_Message_Size, std::map<PEAT_Message_Size, std::list<MPI_Event_Stat> > > >::const_iterator ranks_stat_it = func_stat_it->second.cbegin(); ranks_stat_it != func_stat_it->second.cend(); ++ranks_stat_it ) {
				for ( std::map < PEAT_Message_Size, std::map<PEAT_Message_Size, std::list<MPI_Event_Stat> > >::const_iterator send_stat_it = ranks_stat_it->second.cbegin(); send_stat_it != ranks_stat_it->second.cend(); ++send_stat_it ) {
					std::map <PEAT_Clock, MPI_Event_Stat> sorted_working_set;
					PEAT_Message_Size message_size = send_stat_it->first;
					PEAT_LNum size = 0;
					PEAT_Bool is_acked = true;
					PEAT_Bool is_rdv = false;
					for ( std::map < PEAT_Message_Size, std::list<MPI_Event_Stat> >::const_iterator MPI_Event_Stat_it = send_stat_it->second.cbegin(); MPI_Event_Stat_it != send_stat_it->second.cend(); ++MPI_Event_Stat_it ) {
						for ( std::list<MPI_Event_Stat>::const_iterator event_it = MPI_Event_Stat_it->second.cbegin(); event_it != MPI_Event_Stat_it->second.cend(); ++event_it ) {
							PEAT_Clock fabric_len = event_it->fabric;
							if ( fabric_len <= 0 ) {
								pet_warning( this, NULL, "Send event with zero fabric time.\n");
								continue;
							}
							if ( sorted_working_set.count(fabric_len) < 1 ) {
								sorted_working_set[fabric_len] = *event_it;
							} else {
								sorted_working_set.at(fabric_len).num_events++;
							}
							size++;
							is_acked = event_it->is_acked;
							is_rdv = event_it->is_rndv;
						}
					}
					PEAT_LNum effective_size = size * round_val;
					PEAT_LNum start = 0;// (size - effective_size) / 2;
					PEAT_LNum end = effective_size;//  size - (size - effective_size) / 2;
					PEAT_LNum index = 0;
					// PEAT_LNum avg_val = 0;
					PEAT_LNum min_val = LLONG_MAX;
					//PEAT_LNum max_val = 0;
					//PEAT_LNum ustdev_val = 0;
					PEAT_LNum effective_events = 0;
					for ( std::map <PEAT_Clock, MPI_Event_Stat>::const_iterator event_it = sorted_working_set.cbegin(); event_it != sorted_working_set.cend(); ++event_it ) {
						const MPI_Event_Stat& evt = event_it->second;
						//if ( max_val < evt.fabric ) max_val = evt.fabric;
						if ( index < start || index >= end ) {
							index += evt.num_events;
							continue;
						}
						index += evt.num_events;
						if ( min_val > evt.fabric ) min_val = evt.fabric;
						effective_events += evt.num_events;
						assert( evt.fabric == event_it->first );
						// for ( int i = 0; i < evt.num_events; i++ ) avg_val += evt.fabric;
					}
					if ( effective_events == 0 ) continue;
					fabric_times[fid_pair][message_size].time = min_val;
					fabric_times[fid_pair][message_size].is_acked = is_acked;
					PEAT_Message_Size effective_message_size = get_effective_size(t,message_size);
					PEAT_BW real_bw;
					PEAT_Clock cur_cpu_time;

					switch ( func_id ) {
						case MPI_SSEND_ID:
						rdv_send_p:
							real_bw = ( PEAT_Ratio ) ( effective_message_size + 2 * get_effective_size(t, 0) + is_acked * get_effective_size(t, 0)) / ( PEAT_Ratio ) ( min_val );
							cur_cpu_time = min_val - ( effective_message_size + 2 * get_effective_size(t, 0) + is_acked * get_effective_size(t, 0)) / ideal_bw;
							// cur_cpu_time /= (2 + is_acked);
							break;
						case MPI_IRSEND_ID:
							//if ( message_size >= get_eager_threshold( is_smp, true ) ) continue;
							goto non_block_eager_send_p;
						non_block_eager_send_p:
						case MPI_RSEND_ID:
						eager_send_p:
							cur_cpu_time = min_val - ( effective_message_size + is_acked * get_effective_size(t, 0)) / ideal_bw;
							real_bw = ( PEAT_Ratio ) ( effective_message_size + is_acked * get_effective_size(t, 0)) / ( PEAT_Ratio ) ( min_val );
							// cur_cpu_time /= ( 1 + is_acked );
							break;
						case MPI_ISEND_ID:
							// if ( message_size >= get_eager_threshold( is_smp, true ) ) continue;// goto non_block_rdv_send_p;
						case MPI_ISSEND_ID:
						non_block_rdv_send_p:
							cur_cpu_time = min_val - 64 / ideal_bw;
							real_bw = ( PEAT_Ratio ) ( 64 ) / ( PEAT_Ratio ) ( min_val );
							// cur_cpu_time /= 2;
							break;
						default:
							if ( global_func_id.check_property( func_id, PEAT_PROP_NON_BLOCK ) ) {
								if ( message_size >= get_eager_threshold( is_smp, true ) ) goto non_block_rdv_send_p;
								goto non_block_eager_send_p;
							} else {
								if ( message_size >= get_eager_threshold( is_smp, false ) ) goto rdv_send_p;
								goto eager_send_p;
							}
					}
					// if ( !is_acked) continue;
					if ( cur_cpu_time < 0 ) {
						fprintf(stderr, "CPU Time < 0. Ideal BW %lf<%lf is too little %\n",ideal_bw, real_bw);
						ideal_bw_corrected = real_bw;
						goto start_over;
					}
					if ( real_bw < 0 ) {
						pet_warning(this, NULL, "Real BW <0\n");
					}
					re_iterate:
					if ( cur_cpu_time > CUT_THRESHOLD * last_cpu_time ) {
						switchover_threshold[ fid_pair ].push_back( message_size );
						cpu_times[ fid_pair ].push_back( cur_cpu_time );
					} else {
						if ( cur_cpu_time * CUT_THRESHOLD < last_cpu_time && bw_increased ) {
							switchover_threshold[fid_pair].pop_back();
							cpu_times[fid_pair].pop_back();
							last_cpu_time = cpu_times[fid_pair].back();
							goto re_iterate;
						}
					}
					std::vector<PEAT_Clock>::reverse_iterator cpu_time_it = cpu_times.at(fid_pair).rbegin();
					if ( cur_cpu_time < *cpu_time_it && bw_increased ) {
						*cpu_time_it = cur_cpu_time;
					}
					last_cpu_time = cur_cpu_time;
					if ( real_bw >= top_bw[ fid_pair ] ) {
						//bw is still increasing: update top_bw
						top_bw[ fid_pair ] = real_bw;
						bw_increased = true;
						degradation_thresholds[fid_pair].clear();
						degradation_bw[fid_pair].clear();
					} else {
						if ( bw_increased ) {
							// 1st point after peak. write current point down.
							degradation_thresholds[ fid_pair ].push_back( message_size );
							degradation_bw[ fid_pair ].push_back( top_bw.at( fid_pair ) );
							ms_threshold[ fid_pair ] = message_size;
							bw_increased = false;
						}

					}
				}

				/// cut fake thresholds here
				//assuming that below ms_threshold we're operating at full speed, calculating top_bw at this time:
				if ( ms_threshold.count(fid_pair) > 0 ) {
					PEAT_Num so_size = switchover_threshold.at(fid_pair).size();
					for ( PEAT_Num index = 0; index < so_size; index++ ) {
						if ( switchover_threshold.at(fid_pair)[index] > ms_threshold.at(fid_pair) ) {
							switchover_threshold.at(fid_pair).resize(index);
							cpu_times.at(fid_pair).resize(index);
							break;
						}
					}
				}
				if ( fabric_times[fid_pair].empty() ) continue;
				for ( std::map<PEAT_Message_Size, PEAT_Fabric>::const_iterator fab_len_it = fabric_times.at(fid_pair).cbegin(); fab_len_it != fabric_times.at(fid_pair).cend(); ++fab_len_it ) {
					std::map <PEAT_Clock, MPI_Event_Stat> sorted_working_set;
					PEAT_Message_Size message_size = fab_len_it->first;
					PEAT_Message_Size effective_message_size = get_effective_size(t, message_size);
					PEAT_BW real_bw = 0;
					PEAT_Clock fab_val = fab_len_it->second.time;
					PEAT_Bool is_acked = fab_len_it->second.is_acked;
					PEAT_Num so_size = switchover_threshold[ fid_pair ].size();
					PEAT_Num index;
					for ( index = 0; index < so_size; index++ ) {
						if ( switchover_threshold[ fid_pair ][ index ] > message_size ) {
							break;
						}
					}
					PEAT_Clock cur_cpu_time = 0;
					if ( index > 0 ) {
						index--;
						cur_cpu_time = cpu_times.at(fid_pair)[ index ];
					}
					PEAT_Clock min_cpu_time = 0;

					switch ( func_id ) {
						case MPI_SSEND_ID:
						rdv_send_p2:
							fab_val -= cur_cpu_time;
							real_bw = ( PEAT_Ratio ) ( effective_message_size + 2 * get_effective_size(t, 0) + is_acked * get_effective_size(t, 0)) / ( PEAT_Ratio ) ( fab_val );
							min_cpu_time =  ( effective_message_size + 2 * get_effective_size(t, 0) + is_acked * get_effective_size(t, 0)) / ideal_bw;
							break;
						case MPI_IRSEND_ID:
							if ( message_size >= get_eager_threshold( is_smp, true ) ) continue;
							goto non_block_eager_send_p2;
						non_block_eager_send_p2:
						case MPI_RSEND_ID:
						eager_send_p2:
							fab_val -= cur_cpu_time;
							real_bw = ( PEAT_Ratio ) ( effective_message_size ) / ( PEAT_Ratio ) ( fab_val );
							min_cpu_time = ( effective_message_size ) / ideal_bw;
							break;
						case MPI_ISEND_ID:
							// if ( message_size >= get_eager_threshold( is_smp, true ) ) continue;
							// goto non_block_rdv_send_p2;
						case MPI_ISSEND_ID:
							if ( message_size > 64 ) continue;
						non_block_rdv_send_p2:
							fab_val -= cur_cpu_time;
							real_bw = ( PEAT_Ratio ) (get_effective_size(t, 0)) / ( PEAT_Ratio ) ( fab_val );
							min_cpu_time = (get_effective_size(t, 0)) / ideal_bw;
							break;
						default:
							if ( global_func_id.check_property( func_id, PEAT_PROP_NON_BLOCK ) ) {
								if ( message_size >= get_eager_threshold( is_smp, true ) ) goto non_block_rdv_send_p2;
								goto non_block_eager_send_p2;
							} else {
								if ( message_size >= get_eager_threshold( is_smp, false ) ) goto rdv_send_p2;
								goto eager_send_p2;
							}
					}
					// if ( !is_completed && is_not_verified ) continue;
					if ( real_bw >= top_bw[ fid_pair ] ) {
						top_bw[ fid_pair ] = real_bw;
					}
					if ( top_bw[ fid_pair ] > ideal_bw ) {
						ideal_bw_corrected = top_bw[fid_pair];
						goto start_over;
						/*
						pet_throw_error(this, NULL, NULL, "Top BW  > Ideal BW");
						top_bw[ fid_pair ] = ideal_bw;
						if ( index >= 0 ) {
								cpu_times.at( fid_pair )[ index ] = min_cpu_time;
						}*/
					}
				}
			}
		}
		PEAT_Ratio global_min_pre_delay = 0, global_min_post_delay = 0;
		PEAT_Ratio global_avg_pre_delay = 0, global_avg_post_delay = 0;
		PEAT_Ratio global_median_pre_delay = 0, global_median_post_delay = 0;
		PEAT_Ratio global_max_pre_delay = 0, global_max_post_delay = 0;

		// assuming that cost of sending rst/cts/ack are equal to sending 0-length eager message 
		for ( std::map < PEAT_Func_ID_Pair, std::map<PEAT_Rank, std::map<PEAT_Message_Size, std::map<PEAT_Message_Size, std::list<MPI_Event_Stat> > > > >::const_iterator func_stat_it = stat_local.cbegin(); func_stat_it != stat_local.cend(); ++func_stat_it ) {
			PEAT_Func_ID_Pair fid_pair = func_stat_it->first;
			PEAT_MPI_Func_ID func_id = fid_pair.id;
			PEAT_MPI_Func_ID parent_func_id = fid_pair.parent_id;
			if ( !(global_func_id.check_tx(func_id) && global_func_id.check_tx(parent_func_id)) ) continue;
			fprintf( stderr, "%d:%d:%lld:%s:%s", PEAT_RT_TRANSPORT_Correction_Vars, t, ptp->get_trace_freq( 0 ), global_func_id.name_by_idx( func_id ), global_func_id.name_by_idx( parent_func_id ) );
			for ( std::map < PEAT_Rank, std::map<PEAT_Message_Size, std::map<PEAT_Message_Size, std::list<MPI_Event_Stat> > > >::const_iterator ranks_stat_it = func_stat_it->second.cbegin(); ranks_stat_it != func_stat_it->second.cend(); ++ranks_stat_it ) {
				for ( std::map < PEAT_Message_Size, std::map<PEAT_Message_Size, std::list<MPI_Event_Stat> > >::const_iterator send_stat_it = ranks_stat_it->second.cbegin(); send_stat_it != ranks_stat_it->second.cend(); ++send_stat_it ) {
					PEAT_Ratio min_pre_delay = 0, min_post_delay = 0;
					PEAT_Ratio avg_pre_delay = 0, avg_post_delay = 0;
					PEAT_Ratio median_pre_delay = 0, median_post_delay = 0;
					PEAT_Ratio max_pre_delay = 0, max_post_delay = 0;
					PEAT_Ratio mult_bw = 1.0;
					PEAT_Ratio mult_lat = 1.0;
					std::map <PEAT_Clock, MPI_Event_Stat> sorted_working_set;
					PEAT_Message_Size message_size = send_stat_it->first;
					PEAT_LNum size = 0;
					PEAT_Bool is_acked = false, is_rdv = false;
					for ( std::map < PEAT_Message_Size, std::list<MPI_Event_Stat> >::const_iterator MPI_Event_Stat_it = send_stat_it->second.cbegin(); MPI_Event_Stat_it != send_stat_it->second.cend(); ++MPI_Event_Stat_it ) {
						for ( std::list<MPI_Event_Stat>::const_iterator event_it = MPI_Event_Stat_it->second.cbegin(); event_it != MPI_Event_Stat_it->second.cend(); ++event_it ) {
							PEAT_Clock fabric_len = event_it->fabric;
							if ( sorted_working_set.count( fabric_len ) < 1 ) {
								sorted_working_set[ fabric_len ] = *event_it;
							}
							sorted_working_set.at( fabric_len ).num_events++;
							size++;
							is_acked = event_it->is_acked;
							is_rdv = event_it->is_rndv;
						}
						PEAT_LNum effective_size = size * round_val;
						PEAT_LNum start = ( size - effective_size ) / 2;
						PEAT_LNum end = size - ( size - effective_size ) / 2;
						PEAT_LNum index = 0;
						PEAT_LNum min_val = LLONG_MAX;
						PEAT_LNum avg_val = 0;
						PEAT_LNum median_val = 0;
						PEAT_LNum max_val = 0;
						PEAT_LNum ustdev_val = 0;
						PEAT_LNum effective_events = 0;
						for ( std::map <PEAT_Clock, MPI_Event_Stat>::const_iterator event_it = sorted_working_set.cbegin(); event_it != sorted_working_set.cend(); ++event_it ) {
							const MPI_Event_Stat& evt = event_it->second;
							if ( index < start || index >= end ) {
								index += evt.num_events;
								continue;
							}
							index += evt.num_events;
							if ( min_val > evt.fabric ) min_val = evt.fabric;
							if ( max_val < evt.fabric ) max_val = evt.fabric;
							effective_events += evt.num_events;
							if ( effective_events > size / 2 && median_val <= 0 ) median_val = evt.fabric;
							assert( evt.fabric == event_it->first );
							//for ( int i = 0; i < evt.num_events; i++ ) avg_val += evt.fabric;
							avg_val += evt.fabric * evt.num_events;
						}
						if ( effective_events == 0 ) continue;
						avg_val /= effective_events;
						assert( min_val <= avg_val );
						assert( max_val >= avg_val );
						PEAT_Message_Size effective_message_size = message_size + 56;
						if ( effective_message_size % 64 > 0 ) effective_message_size = effective_message_size - effective_message_size % 64 + 64;
						index = 0;
						for ( std::map <PEAT_Clock, MPI_Event_Stat>::const_iterator event_it = sorted_working_set.cbegin(); event_it != sorted_working_set.cend(); ++event_it ) {
							const MPI_Event_Stat& evt = event_it->second;
							index += evt.num_events;
							if ( index < start || index >= end ) continue;
							for ( int i = 0; i < evt.num_events; i++ ) ustdev_val += ( evt.fabric - avg_val ) * ( evt.fabric - avg_val );
						}
						PEAT_Ratio u_st_dev_val = sqrt( ustdev_val / effective_events );
						PEAT_Clock ideal_time;
						PEAT_BW used_bw;
						PEAT_LAT t_time;
						PEAT_LAT overall_cpu_times;
						PEAT_BW real_bw;
						PEAT_Num so_size = switchover_threshold[ fid_pair ].size();
						for ( index = 0; index < so_size; index++ ) {
							if ( switchover_threshold[ fid_pair ][ index ] > message_size ) {
								break;
							}
						}
						index--;
						PEAT_Clock cur_cpu_time = index >= 0 ? cpu_times.at(fid_pair)[ index ] : 0;

						switch ( func_id ) {
							case MPI_SSEND_ID:
								// is_not_verified = false;
							rdv_send:
								real_bw = ( PEAT_Ratio ) ( effective_message_size + 2 * get_effective_size(t, 0) + is_acked * get_effective_size(t, 0)) / ( PEAT_Ratio ) ( min_val - cur_cpu_time );
								if ( ms_threshold.count(fid_pair) ) {
									if ( message_size > ms_threshold.at(fid_pair) ) {
										mult_bw = real_bw / top_bw.at(fid_pair);
									} else {
										mult_bw = 1;
									}
								} else {
									mult_bw = 1;
								}
								used_bw = top_bw.at(fid_pair) * mult_bw;
								ideal_time = ( effective_message_size + 2 * get_effective_size(t, 0) + is_acked * get_effective_size(t, 0)) / used_bw; // (rts+cts+message)
								// if ( !is_completed && is_not_verified ) ideal_time = 0;
								overall_cpu_times = 0;// (general_lat) * 2;
								t_time = min_val - ideal_time - overall_cpu_times;
								// subtract cpu times for rts/ack 
								min_pre_delay = floor( t_time * fwd_scale_cpu_time / RDV_SCALE_FACTOR_1 ) - global_min_pre_delay * (is_acked + 1);
								min_post_delay = floor( t_time * bwd_scale_cpu_time / RDV_SCALE_FACTOR_2 ) - global_min_post_delay * ( is_acked + 1 );

								overall_cpu_times = 0;// (general_lat) * 2;
								t_time = avg_val - ideal_time - overall_cpu_times;
								avg_pre_delay = floor( t_time * fwd_scale_cpu_time / RDV_SCALE_FACTOR_1 )  - global_min_pre_delay * ( is_acked + 1 );
								avg_post_delay = floor( t_time * bwd_scale_cpu_time / RDV_SCALE_FACTOR_2 ) - global_min_post_delay * ( is_acked + 1 );

								overall_cpu_times = 0;// (general_lat) * 2;
								t_time = median_val - ideal_time - overall_cpu_times;
								median_pre_delay = floor( t_time * fwd_scale_cpu_time / RDV_SCALE_FACTOR_1 ) - global_min_pre_delay * ( is_acked + 1 );
								median_post_delay = floor( t_time * bwd_scale_cpu_time / RDV_SCALE_FACTOR_2 ) - global_min_post_delay * ( is_acked + 1 );

								overall_cpu_times = 0;// (general_lat) * 2;
								t_time = max_val - ideal_time - overall_cpu_times;
								max_pre_delay = floor( t_time * fwd_scale_cpu_time / RDV_SCALE_FACTOR_1 ) - global_min_pre_delay * ( is_acked + 1 );
								max_post_delay = floor( t_time * bwd_scale_cpu_time / RDV_SCALE_FACTOR_2 ) - global_min_post_delay * ( is_acked + 1 );
								break;
							case MPI_IRSEND_ID:
								// is_not_verified = true;
								effective_message_size = 64;
								if ( message_size >= get_eager_threshold( is_smp, true ) ) continue;
							case MPI_ISEND_ID:
								effective_message_size = 64;
								// is_not_verified = true;
								if ( message_size >= get_eager_threshold( is_smp, true ) ) goto non_block_rdv_send;
								goto non_block_eager_send;
							case MPI_RSEND_ID:
								// is_not_verified = false;
							eager_send:
							non_block_eager_send:
								real_bw = ( PEAT_Ratio ) ( effective_message_size + is_acked * get_effective_size(t, 0)) / ( PEAT_Ratio ) ( min_val - cur_cpu_time );
								if ( ms_threshold.count(fid_pair) ) {
									if ( message_size > ms_threshold.at(fid_pair) ) {
										mult_bw = real_bw / top_bw.at(fid_pair);
									} else {
										mult_bw = 1;
									}
								} else {
									mult_bw = 1;
								}
								used_bw = top_bw.at(fid_pair) * mult_bw;
								ideal_time = ( effective_message_size + is_acked * get_effective_size(t, 0)) / used_bw;
								// if ( !is_completed && is_not_verified ) ideal_time = 0;
								overall_cpu_times = 0;
								t_time = min_val - ideal_time - overall_cpu_times;
								min_pre_delay = floor( t_time * fwd_scale_cpu_time ) / (is_acked + 1);
								min_post_delay = floor( t_time * bwd_scale_cpu_time ) / ( is_acked + 1 );

								t_time = avg_val - ideal_time;
								avg_pre_delay = floor( t_time * fwd_scale_cpu_time ) / ( is_acked + 1 );
								avg_post_delay = floor( t_time * bwd_scale_cpu_time ) / ( is_acked + 1 );

								t_time = median_val - ideal_time - overall_cpu_times;
								median_pre_delay = floor( t_time * fwd_scale_cpu_time ) / ( is_acked + 1 );
								median_post_delay = floor( t_time * bwd_scale_cpu_time ) / ( is_acked + 1 );

								t_time = max_val - ideal_time;
								max_pre_delay = floor( t_time * fwd_scale_cpu_time ) / ( is_acked + 1 );
								max_post_delay = floor( t_time * bwd_scale_cpu_time ) / ( is_acked + 1 );
								break;
							case MPI_ISSEND_ID:
								// is_not_verified = true;
							non_block_rdv_send:
								real_bw = ( PEAT_Ratio ) (get_effective_size(t, 0)) / ( PEAT_Ratio ) ( min_val - cur_cpu_time );
								if ( ms_threshold.count(fid_pair) ) {
									if ( message_size > ms_threshold.at(fid_pair) ) {
										mult_bw = real_bw / top_bw.at(fid_pair);
									} else {
										mult_bw = 1;
									}
								} else {
									mult_bw = 1;
								}
								used_bw = top_bw.at(fid_pair) * mult_bw;
								ideal_time = (get_effective_size(t, 0)) / used_bw; // (rts+cts+message)
								//if ( !is_completed && is_not_verified ) ideal_time = 0;
								overall_cpu_times = 0;
								t_time = min_val - ideal_time - overall_cpu_times;
								min_pre_delay = floor( t_time * fwd_scale_cpu_time );
								min_post_delay = floor( t_time * bwd_scale_cpu_time );

								overall_cpu_times = 0;
								t_time = avg_val - ideal_time - overall_cpu_times;
								avg_pre_delay = floor( t_time * fwd_scale_cpu_time );
								avg_post_delay = floor( t_time * bwd_scale_cpu_time );

								overall_cpu_times = 0;
								t_time = median_val - ideal_time - overall_cpu_times;
								median_pre_delay = floor( t_time * fwd_scale_cpu_time );
								median_post_delay = floor( t_time * bwd_scale_cpu_time );

								overall_cpu_times = 0;
								t_time = max_val - ideal_time - overall_cpu_times;
								max_pre_delay = floor( t_time * bwd_scale_cpu_time );
								max_post_delay = floor( t_time * fwd_scale_cpu_time );
								break;
							default:
								// is_not_verified = false;
								if ( global_func_id.check_property( func_id, PEAT_PROP_NON_BLOCK ) ) {
									if ( message_size >= get_eager_threshold( is_smp, true ) ) goto non_block_rdv_send;
									goto non_block_eager_send;
								} else {
									if ( message_size >= get_eager_threshold( is_smp, false ) ) goto rdv_send;
									goto eager_send;
								}
						}
						if ( overall_cpu_times < 0 ) {
							pet_warning(this, NULL, "Overall CPU Times <0\n");
						}
						if ( real_bw < 0 ) {
							pet_warning(this, NULL, "Real BW <0\n");
						}
						if ( min_pre_delay < 0 || min_post_delay < 0 ) {
							pet_warning(this, NULL, "min_pre_delay or min_post_delay <0\n");
						}

						envs[ PET_Transport_Selector_MIN ][ message_size ].pre_delay = min_pre_delay;
						envs[ PET_Transport_Selector_MIN ][ message_size ].post_delay = min_post_delay;
						envs[ PET_Transport_Selector_AVG ][ message_size ].pre_delay = avg_pre_delay;
						envs[ PET_Transport_Selector_AVG ][ message_size ].post_delay = avg_post_delay;
						envs[ PET_Transport_Selector_median ][ message_size ].pre_delay = median_pre_delay;
						envs[ PET_Transport_Selector_median ][ message_size ].post_delay = median_post_delay;
						envs[ PET_Transport_Selector_MAX ][ message_size ].pre_delay = max_pre_delay;
						envs[ PET_Transport_Selector_MAX ][ message_size ].post_delay = max_post_delay;
						if ( global_min_pre_delay > min_pre_delay || global_min_pre_delay == 0) global_min_pre_delay = min_pre_delay;
						if ( global_min_post_delay > min_post_delay || global_min_post_delay == 0 ) global_min_post_delay = min_post_delay;
						if ( global_avg_pre_delay > avg_pre_delay || global_avg_pre_delay == 0 ) global_avg_pre_delay = avg_pre_delay;
						if ( global_avg_post_delay > avg_post_delay || global_avg_post_delay == 0 ) global_avg_post_delay = avg_post_delay;
						if ( global_median_pre_delay > median_pre_delay || global_median_pre_delay == 0 ) global_median_pre_delay = median_pre_delay;
						if ( global_median_post_delay > median_post_delay || global_median_post_delay == 0 ) global_median_post_delay = median_post_delay;
						if ( global_max_pre_delay > max_pre_delay || global_max_pre_delay == 0 ) global_max_pre_delay = max_pre_delay;
						if ( global_max_post_delay > max_post_delay || global_max_post_delay == 0 ) global_max_post_delay = max_post_delay;
						if ( ( message_size >= get_eager_threshold( is_smp, true ) || global_func_id.check_property( func_id, PEAT_PROP_DATA_SYNC ) ) && global_func_id.check_property( func_id, PEAT_PROP_NON_BLOCK ) ) continue; //skip writing
						gzprintf( prof_file_ptr, " ID=%d Func:%s, Size:%lld, fabric: min=%lld median=%lld avx=%lld max=%lld ustdev=%lf, Ratio:%lf-%lf,%lf, method: %d,%d\n", t, global_func_id.name_by_idx( func_id ), message_size, min_val, avg_val, median_val, max_val, u_st_dev_val, real_bw, used_bw, mult_bw, is_rdv, is_acked );
						fprintf( stderr, ":%lld:%lf:%lf", message_size, mult_bw, mult_lat );
						fprintf( stderr, ":%lld:%lld", ( PEAT_LAT ) min_pre_delay, ( PEAT_LAT ) min_post_delay );
						fprintf( stderr, ":%lld:%lld", ( PEAT_LAT ) avg_pre_delay, ( PEAT_LAT ) avg_post_delay );
						fprintf( stderr, ":%lld:%lld", ( PEAT_LAT ) median_pre_delay, ( PEAT_LAT ) median_post_delay );
						fprintf( stderr, ":%lld:%lld", ( PEAT_LAT ) max_pre_delay, ( PEAT_LAT ) max_post_delay );
					}
				}
			}
			fprintf( stderr, ":-1:1.0:1.0" );
			fprintf( stderr, ":%lld:%lld", ( PEAT_LAT ) global_min_pre_delay, ( PEAT_LAT ) global_min_post_delay );
			fprintf( stderr, ":%lld:%lld", ( PEAT_LAT ) global_avg_pre_delay, ( PEAT_LAT ) global_avg_post_delay );
			fprintf( stderr, ":%lld:%lld", ( PEAT_LAT ) global_median_pre_delay, ( PEAT_LAT ) global_median_post_delay );
			fprintf( stderr, ":%lld:%lld", ( PEAT_LAT ) global_max_pre_delay, ( PEAT_LAT ) global_max_post_delay );
			fprintf( stderr, "\n" );
		}
	}
}
#else
struct PET_Fab_Stat {
    PEAT_Message_Size message_size = -1;
    PEAT_Clock min_val = -1, avg_val = -1, median_val = -1, max_val = -1, fab_val = -1;
    PEAT_Ratio u_st_dev_val = 0.0;
    PEAT_Bool is_rdv = false, is_acked = false;
    PEAT_BW real_bw = 0;
    PEAT_Ratio ratio_bw = 1.0, ratio_lat = 1.0;
};
void PET_Core::write_transport_profile(const std::string &name, const Event_Set_Stat::t_stat_type type)
{
    std::map<PEAT_Func_ID_Pair, std::map<PEAT_Message_Size, PET_Fab_Stat>> fab_stat[PEAT_TRANSPORT_MAX];
    std::map<PEAT_Func_ID_Pair, std::map<PEAT_Message_Size, PET_Fab_Stat>> fab_stat_corrected[PEAT_TRANSPORT_MAX];
    std::map<PEAT_Func_ID_Pair, PEAT_BW> most_bw[PEAT_TRANSPORT_MAX], most_bw_corrected[PEAT_TRANSPORT_MAX];
    std::map<PEAT_Func_ID_Pair, PEAT_Message_Size> most_bw_size[PEAT_TRANSPORT_MAX];
    std::map<PEAT_Rank, std::string> file_name;
    file_name[0] = name;
    File_Storage_GZ profile_file(file_name, PEAT_FIO_FLAG_WRITE, NULL, globals.verbose);
    if (!profile_file.is_file_good(0)) {
        std::ostringstream tmp;
        tmp << "Can't open file (to write) " << name;
        throw std::runtime_error(tmp.str());
    }
    profile_file.printf(0, "  0: ID:      Func:    Parent:         Size:Complete size:       fab:       min:    median:   average:       max:    ustdev:RDV:ACK: Effective BW:Ratio\n");
    for (PEAT_Transports t = PEAT_TRANSPORT_DDR; t < PEAT_TRANSPORT_MAX; t = t_inc(t)) {
        if (t == PEAT_TRANSPORT_COLLECTIVE)
            continue;
        std::map<PEAT_Func_ID_Pair, std::map<PEAT_Rank, std::map<PEAT_Message_Size, std::map<PEAT_Message_Size, std::list<MPI_Event_Stat>>>>> &stat_local = stat.get_ts_fs_stat(type, t);
        if (stat_local.empty())
            continue;
        std::map<PEAT_Func_ID_Pair, std::map<PEAT_Message_Size, MPI_Event_Stat>> working_set;
        bool is_smp = true;
        PEAT_Num numa_ppn = ptp->get_numa_pp(0, 0);
        PEAT_Trasport_Environment env_t = transport->get_env(t, stat_local.cbegin()->first.id, stat_local.cbegin()->first.parent_id, 0, numa_ppn, globals.transport_selector);
        switch (t) {
        case PEAT_TRANSPORT_DDR:
            is_smp = true;
            break;
        case PEAT_TRANSPORT_HBM:
            is_smp = true;
            break;
        case PEAT_TRANSPORT_QPI:
            is_smp = true;
            break;
        case PEAT_TRANSPORT_NIC:
            is_smp = false;
            break;
        default:
            assert(0);
        }
        for (std::map<PEAT_Func_ID_Pair, std::map<PEAT_Rank, std::map<PEAT_Message_Size, std::map<PEAT_Message_Size, std::list<MPI_Event_Stat>>>>>::const_iterator func_stat_it = stat_local.cbegin(); func_stat_it != stat_local.cend(); ++func_stat_it) {
            PEAT_Clock last_cpu_time = 0;
            PEAT_Func_ID_Pair fid_pair = func_stat_it->first;
            PEAT_MPI_Func_ID func_id = fid_pair.id;
            PEAT_MPI_Func_ID parent_func_id = fid_pair.parent_id;
            if (!(global_func_id.check_tx(func_id) && global_func_id.check_tx(parent_func_id)))
                continue;
            PEAT_Bool bw_increased = false;
            for (std::map<PEAT_Rank, std::map<PEAT_Message_Size, std::map<PEAT_Message_Size, std::list<MPI_Event_Stat>>>>::const_iterator ranks_stat_it = func_stat_it->second.cbegin(); ranks_stat_it != func_stat_it->second.cend(); ++ranks_stat_it) {
                for (std::map<PEAT_Message_Size, std::map<PEAT_Message_Size, std::list<MPI_Event_Stat>>>::const_iterator send_stat_it = ranks_stat_it->second.cbegin(); send_stat_it != ranks_stat_it->second.cend(); ++send_stat_it) {
                    std::map<PEAT_Clock, MPI_Event_Stat> sorted_working_set;
                    PEAT_Message_Size message_size = send_stat_it->first;
                    PEAT_LNum size = 0;
                    PEAT_Bool is_acked = true;
                    PEAT_Bool is_rdv = false;
                    for (std::map<PEAT_Message_Size, std::list<MPI_Event_Stat>>::const_iterator MPI_Event_Stat_it = send_stat_it->second.cbegin(); MPI_Event_Stat_it != send_stat_it->second.cend(); ++MPI_Event_Stat_it) {
                        for (std::list<MPI_Event_Stat>::const_iterator event_it = MPI_Event_Stat_it->second.cbegin(); event_it != MPI_Event_Stat_it->second.cend(); ++event_it) {
                            PEAT_Clock fabric_len = event_it->fabric;
                            if (fabric_len <= 0) {
                                pet_warning(this, NULL, "Send event with zero fabric time.\n");
                                continue;
                            }
                            if (sorted_working_set.count(fabric_len) < 1) {
                                sorted_working_set[fabric_len] = *event_it;
                            } else {
                                sorted_working_set.at(fabric_len).num_events++;
                            }
                            size++;
                            is_acked = event_it->is_acked;
                            is_rdv = event_it->is_rndv;
                        }
                    }
                    PEAT_LNum effective_size = (PEAT_LNum)(size * round_val);
                    PEAT_LNum start = 0;            // (size - effective_size) / 2;
                    PEAT_LNum end = effective_size; //  size - (size - effective_size) / 2;
                    PEAT_LNum index = 0;
                    PEAT_LNum avg_val = 0;
                    PEAT_LNum min_val = LLONG_MAX;
                    PEAT_LNum median_val = -1;
                    PEAT_LNum max_val = 0;
                    PEAT_LNum ustdev_val = 0;
                    PEAT_LNum effective_events = 0;
                    for (std::map<PEAT_Clock, MPI_Event_Stat>::iterator event_it = sorted_working_set.begin(); index < start; index += event_it->second.num_events, event_it = sorted_working_set.begin()) {
                        sorted_working_set.erase(event_it->first);
                    }
                    index = size - 1;
                    for (std::map<PEAT_Clock, MPI_Event_Stat>::reverse_iterator event_it = sorted_working_set.rbegin(); index > end; index -= event_it->second.num_events, event_it = sorted_working_set.rbegin()) {

                        sorted_working_set.erase(event_it->first);
                    }
                    for (std::map<PEAT_Clock, MPI_Event_Stat>::const_iterator event_it = sorted_working_set.cbegin(); event_it != sorted_working_set.cend(); ++event_it) {
                        const MPI_Event_Stat &evt = event_it->second;
                        if (max_val < evt.fabric)
                            max_val = evt.fabric;
                        if (min_val > evt.fabric)
                            min_val = evt.fabric;
                        effective_events += evt.num_events;
                        if (effective_events > (PEAT_LNum)sorted_working_set.size() / 2 && median_val <= 0)
                            median_val = evt.fabric;
                        assert(evt.fabric == event_it->first);
                        avg_val += evt.fabric * evt.num_events;
                    }
                    if (effective_events == 0)
                        continue;
                    avg_val /= effective_events;
                    PEAT_Message_Size effective_message_size = get_effective_size(t, message_size);
                    index = 0;
                    for (std::map<PEAT_Clock, MPI_Event_Stat>::const_iterator event_it = sorted_working_set.cbegin(); event_it != sorted_working_set.cend(); ++event_it) {
                        const MPI_Event_Stat &evt = event_it->second;
                        for (int i = 0; i < evt.num_events; i++)
                            ustdev_val += (evt.fabric - avg_val) * (evt.fabric - avg_val);
                    }
                    PEAT_Ratio u_st_dev_val = sqrt(ustdev_val / effective_events);

                    PEAT_Message_Size complete_message_size;
                    PEAT_BW real_bw;

                    switch (func_id) {
                    case MPI_SSEND_ID:
                    rdv_send_p:
                        // is_rdv = true;
                        complete_message_size = (effective_message_size + 2 * get_effective_size(t, 0) + is_acked * get_effective_size(t, 0));
                        break;
                    case MPI_IRSEND_ID:
                        goto non_block_eager_send_p;
                    non_block_eager_send_p:
                    case MPI_RSEND_ID:
                    eager_send_p:
                        complete_message_size = (effective_message_size + is_acked * get_effective_size(t, 0));
                        break;
                    case MPI_ISEND_ID:
                        // if ( message_size >= get_eager_threshold( is_smp, true ) ) continue;// goto non_block_rdv_send_p;
                    case MPI_ISSEND_ID:
                    non_block_rdv_send_p:
                        complete_message_size = get_effective_size(t, 0);
                        break;
                    default:
                        if (global_func_id.check_property(func_id, PEAT_PROP_NON_BLOCK)) {
                            if (message_size >= get_eager_threshold(is_smp, true))
                                goto non_block_rdv_send_p;
                            goto non_block_eager_send_p;
                        } else {
                            if (message_size >= get_eager_threshold(is_smp, false))
                                goto rdv_send_p;
                            goto eager_send_p;
                        }
                    }
                    real_bw = (PEAT_Ratio)complete_message_size / (PEAT_Ratio)min_val;
#define FIELD1_LEN 10
#define FIELD2_LEN 13
#define FIELD3_LEN 3
                    fab_stat[t][fid_pair][message_size].message_size = complete_message_size;
                    PET_Fab_Stat &current_stat = fab_stat[t][fid_pair][message_size];
                    current_stat.min_val = min_val;
                    current_stat.fab_val = min_val;
                    current_stat.avg_val = avg_val;
                    current_stat.median_val = median_val;
                    current_stat.u_st_dev_val = u_st_dev_val;
                    current_stat.max_val = max_val;
                    current_stat.is_rdv = is_rdv;
                    current_stat.is_acked = is_acked;
                    current_stat.real_bw = real_bw;
                    profile_file.printf(0, "%*d:%*d:%*s:%*s:%*lld:%*lld:%*lld:%*lld:%*lld:%*lld:%*lld:%*.2lf:%*d:%*d:%*.2lf:%*.3lf\n",
                                       FIELD3_LEN, PEAT_RT_TRANSPORT_Vars, FIELD3_LEN, t,
                                       FIELD1_LEN, global_func_id.name_by_idx(func_id),
                                       FIELD1_LEN, global_func_id.name_by_idx(parent_func_id),
                                       FIELD2_LEN, message_size, FIELD2_LEN, current_stat.message_size,
                                       FIELD1_LEN, current_stat.fab_val, FIELD1_LEN, current_stat.min_val, FIELD1_LEN, current_stat.median_val, FIELD1_LEN, current_stat.avg_val, FIELD1_LEN, current_stat.max_val, FIELD1_LEN, current_stat.u_st_dev_val,
                                       FIELD3_LEN, current_stat.is_rdv, FIELD3_LEN, current_stat.is_acked, FIELD2_LEN, current_stat.real_bw, FIELD3_LEN + 2, current_stat.ratio_bw);
                }
            }
        }
    }
    // is_rdv is_acked
    PEAT_Ratio cpu_clock_divider[2][2] = {
        {1, 1.5},
        {1.5, 2},
    };
    for (PEAT_Transports t = PEAT_TRANSPORT_DDR; t < PEAT_TRANSPORT_MAX; t = t_inc(t)) {
        fab_stat_corrected[t] = fab_stat[t];
        if (t == PEAT_TRANSPORT_COLLECTIVE)
            continue;
        for (std::map<PEAT_Func_ID_Pair, std::map<PEAT_Message_Size, PET_Fab_Stat>>::iterator funcid_it = fab_stat[t].begin(); funcid_it != fab_stat[t].end(); ++funcid_it) {
            PEAT_Func_ID_Pair fid_pair = funcid_it->first;
            PEAT_LAT least_fab = LLONG_MAX;
            PEAT_Message_Size least_fab_size = -1;
            for (std::map<PEAT_Message_Size, PET_Fab_Stat>::iterator message_size_it = funcid_it->second.begin(); message_size_it != funcid_it->second.end(); ++message_size_it) {
                PEAT_Message_Size message_size = message_size_it->first;
                if (most_bw[t][fid_pair] < message_size_it->second.real_bw) {
                    most_bw[t][fid_pair] = message_size_it->second.real_bw;
                    most_bw_size[t][fid_pair] = message_size;
                }
                if (least_fab > message_size_it->second.fab_val) {
                    least_fab = message_size_it->second.fab_val;
                    least_fab_size = message_size;
                }
            }
            PET_Fab_Stat &current_stat = fab_stat_corrected[t][fid_pair][least_fab_size];
            PEAT_Clock least_cpu = (PEAT_Clock)((current_stat.fab_val - current_stat.message_size / most_bw[t][fid_pair]) / cpu_clock_divider[current_stat.is_rdv][current_stat.is_acked]);

            for (std::map<PEAT_Message_Size, PET_Fab_Stat>::iterator message_size_it = funcid_it->second.begin(); message_size_it != funcid_it->second.end(); ++message_size_it) {
                PEAT_Message_Size message_size = message_size_it->first;
                PET_Fab_Stat &current_stat = fab_stat_corrected[t][fid_pair][message_size];

                if (message_size < most_bw_size[t][fid_pair])
                    continue;
                if (message_size > most_bw_size[t][fid_pair]) {
                    current_stat.ratio_bw = current_stat.real_bw / most_bw[t][fid_pair];
                } else {
                    PEAT_Clock fab_clock = current_stat.fab_val - least_cpu; //'ideal' fabric_time
                    current_stat.real_bw = most_bw_corrected[t][fid_pair] = (PEAT_BW)current_stat.message_size / (PEAT_BW)fab_clock;
                    current_stat.min_val -= fab_clock;
                    current_stat.avg_val -= fab_clock;
                    current_stat.median_val -= fab_clock;
                    current_stat.max_val -= fab_clock;
                    current_stat.fab_val = fab_clock;
                }
            }
        }
        PEAT_BW most_bw_all = 0.0;
        for (std::map<PEAT_Func_ID_Pair, PEAT_BW>::iterator bw_it = most_bw_corrected[t].begin(); bw_it != most_bw_corrected[t].end(); ++bw_it) {
            if (most_bw_all < bw_it->second) {
                most_bw_all = bw_it->second;
            }
        }
        for (std::map<PEAT_Func_ID_Pair, std::map<PEAT_Message_Size, PET_Fab_Stat>>::iterator funcid_it = fab_stat[t].begin(); funcid_it != fab_stat[t].end(); ++funcid_it) {
            PEAT_Func_ID_Pair fid_pair = funcid_it->first;
            for (std::map<PEAT_Message_Size, PET_Fab_Stat>::iterator message_size_it = funcid_it->second.begin(); message_size_it != funcid_it->second.end(); ++message_size_it) {
                PEAT_Message_Size message_size = message_size_it->first;
                PET_Fab_Stat &current_stat = fab_stat_corrected[t][fid_pair][message_size];
                if (message_size == most_bw_size[t][fid_pair])
                    continue;
                if (message_size < most_bw_size[t][fid_pair]) {
                    PEAT_Clock fab_clock = (PEAT_Clock)(current_stat.message_size / most_bw_corrected[t][fid_pair]); //'ideal' fabric_time
                    current_stat.min_val -= fab_clock;
                    current_stat.avg_val -= fab_clock;
                    current_stat.median_val -= fab_clock;
                    current_stat.max_val -= fab_clock;
                    current_stat.fab_val = fab_clock;
                    current_stat.real_bw = (PEAT_Ratio)current_stat.message_size / (PEAT_Ratio)current_stat.fab_val;
                } else {
                    PEAT_Clock fab_clock = (PEAT_Clock)(current_stat.message_size / (most_bw_corrected[t][fid_pair] * current_stat.ratio_bw)); //'ideal' fabric_time
                    current_stat.min_val -= fab_clock;
                    current_stat.avg_val -= fab_clock;
                    current_stat.median_val -= fab_clock;
                    current_stat.max_val -= fab_clock;
                    current_stat.fab_val = fab_clock;
                    current_stat.real_bw = (PEAT_Ratio)current_stat.message_size / (PEAT_Ratio)current_stat.fab_val;
                }
                profile_file.printf(0, "%*d:%*d:%*s:%*s:%*lld:%*lld:%*lld:%*lld:%*lld:%*lld:%*lld:%*.2lf:%*d:%*d:%*.2lf:%*.2lf\n",
                                   FIELD3_LEN, PEAT_RT_TRANSPORT_Vars, FIELD3_LEN, t,
                                   FIELD1_LEN, global_func_id.name_by_idx(fid_pair.id),
                                   FIELD1_LEN, global_func_id.name_by_idx(fid_pair.parent_id),
                                   FIELD2_LEN, message_size, FIELD2_LEN, current_stat.message_size,
                                   FIELD1_LEN, current_stat.fab_val, FIELD1_LEN, current_stat.min_val, FIELD1_LEN, current_stat.median_val, FIELD1_LEN, current_stat.avg_val, FIELD1_LEN, current_stat.max_val, FIELD1_LEN, current_stat.u_st_dev_val,
                                   FIELD3_LEN, current_stat.is_rdv, FIELD3_LEN, current_stat.is_acked, FIELD2_LEN, current_stat.real_bw, FIELD3_LEN + 2, current_stat.ratio_bw);
            }
            // fprintf(stderr, "%d:%d:%lld:%s:%s:%.2lf", PEAT_RT_TRANSPORT_Correction_Vars, t, ptp->get_trace_freq(0), global_func_id.name_by_idx(fid_pair.id), global_func_id.name_by_idx(fid_pair.parent_id), most_bw_corrected);
            fprintf(stderr, "%d:%d:%lld:%s:%s", PEAT_RT_TRANSPORT_Correction_Vars, t, ptp->get_trace_freq(0), global_func_id.name_by_idx(fid_pair.id), global_func_id.name_by_idx(fid_pair.parent_id));
            PEAT_Ratio global_min_pre_delay = 0, global_min_post_delay = 0;
            PEAT_Ratio global_avg_pre_delay = 0, global_avg_post_delay = 0;
            PEAT_Ratio global_median_pre_delay = 0, global_median_post_delay = 0;
            PEAT_Ratio global_max_pre_delay = 0, global_max_post_delay = 0;

            for (std::map<PEAT_Message_Size, PET_Fab_Stat>::iterator message_size_it = funcid_it->second.begin(); message_size_it != funcid_it->second.end(); ++message_size_it) {
                PEAT_Message_Size message_size = message_size_it->first;
                PET_Fab_Stat &current_stat = fab_stat_corrected[t][fid_pair][message_size];
                PEAT_Ratio min_pre_delay = 0, min_post_delay = 0;
                PEAT_Ratio avg_pre_delay = 0, avg_post_delay = 0;
                PEAT_Ratio median_pre_delay = 0, median_post_delay = 0;
                PEAT_Ratio max_pre_delay = 0, max_post_delay = 0;
                PEAT_Bool is_acked = current_stat.is_acked;

                PEAT_LAT t_time;
                if (current_stat.is_rdv) {
                    t_time = current_stat.min_val;
                    min_pre_delay = floor(t_time * fwd_scale_cpu_time / cpu_clock_divider[1][is_acked]);
                    min_post_delay = floor(t_time * bwd_scale_cpu_time / cpu_clock_divider[1][is_acked]);

                    t_time = current_stat.avg_val;

                    avg_pre_delay = floor(t_time * fwd_scale_cpu_time / cpu_clock_divider[1][is_acked]);
                    avg_post_delay = floor(t_time * bwd_scale_cpu_time / cpu_clock_divider[1][is_acked]);

                    t_time = current_stat.median_val;
                    median_pre_delay = floor(t_time * fwd_scale_cpu_time / cpu_clock_divider[1][is_acked]);
                    median_post_delay = floor(t_time * bwd_scale_cpu_time / cpu_clock_divider[1][is_acked]);

                    t_time = current_stat.max_val;
                    max_pre_delay = floor(t_time * fwd_scale_cpu_time / cpu_clock_divider[1][is_acked]);
                    max_post_delay = floor(t_time * bwd_scale_cpu_time / cpu_clock_divider[1][is_acked]);
                } else {
                    t_time = current_stat.min_val;
                    min_pre_delay = floor(t_time * fwd_scale_cpu_time / cpu_clock_divider[0][is_acked]);
                    min_post_delay = floor(t_time * bwd_scale_cpu_time / cpu_clock_divider[0][is_acked]);

                    t_time = current_stat.avg_val;
                    avg_pre_delay = floor(t_time * fwd_scale_cpu_time / cpu_clock_divider[0][is_acked]);
                    avg_post_delay = floor(t_time * bwd_scale_cpu_time / cpu_clock_divider[0][is_acked]);

                    t_time = current_stat.median_val;
                    median_pre_delay = floor(t_time * fwd_scale_cpu_time / cpu_clock_divider[0][is_acked]);
                    median_post_delay = floor(t_time * bwd_scale_cpu_time / cpu_clock_divider[0][is_acked]);

                    t_time = current_stat.max_val;
                    max_pre_delay = floor(t_time * fwd_scale_cpu_time / cpu_clock_divider[0][is_acked]);
                    max_post_delay = floor(t_time * bwd_scale_cpu_time / cpu_clock_divider[0][is_acked]);
                }
                fprintf(stderr, ":%lld:%.2lf:%.2lf", message_size, current_stat.ratio_bw * most_bw_corrected[t].at(fid_pair) / most_bw_all, current_stat.ratio_lat);
                fprintf(stderr, ":%lld:%lld", (PEAT_LAT)min_pre_delay, (PEAT_LAT)min_post_delay);
                fprintf(stderr, ":%lld:%lld", (PEAT_LAT)avg_pre_delay, (PEAT_LAT)avg_post_delay);
                fprintf(stderr, ":%lld:%lld", (PEAT_LAT)median_pre_delay, (PEAT_LAT)median_post_delay);
                fprintf(stderr, ":%lld:%lld", (PEAT_LAT)max_pre_delay, (PEAT_LAT)max_post_delay);
                if (global_min_pre_delay > min_pre_delay || global_min_pre_delay == 0)
                    global_min_pre_delay = min_pre_delay;
                if (global_min_post_delay > min_post_delay || global_min_post_delay == 0)
                    global_min_post_delay = min_post_delay;
                if (global_avg_pre_delay > avg_pre_delay || global_avg_pre_delay == 0)
                    global_avg_pre_delay = avg_pre_delay;
                if (global_avg_post_delay > avg_post_delay || global_avg_post_delay == 0)
                    global_avg_post_delay = avg_post_delay;
                if (global_median_pre_delay > median_pre_delay || global_median_pre_delay == 0)
                    global_median_pre_delay = median_pre_delay;
                if (global_median_post_delay > median_post_delay || global_median_post_delay == 0)
                    global_median_post_delay = median_post_delay;
                if (global_max_pre_delay > max_pre_delay || global_max_pre_delay == 0)
                    global_max_pre_delay = max_pre_delay;
                if (global_max_post_delay > max_post_delay || global_max_post_delay == 0)
                    global_max_post_delay = max_post_delay;
            }
            PEAT_Ratio ratio_bw = most_bw_corrected[t].at(fid_pair) / most_bw_all, ratio_lat = 1.0;
            PEAT_Clock rts_size = -1;
            fprintf(stderr, ":%lld:%.2lf:%.2lf", rts_size, ratio_bw, ratio_lat);
            fprintf(stderr, ":%lld:%lld", (PEAT_LAT)global_min_pre_delay, (PEAT_LAT)global_min_post_delay);
            fprintf(stderr, ":%lld:%lld", (PEAT_LAT)global_avg_pre_delay, (PEAT_LAT)global_avg_post_delay);
            fprintf(stderr, ":%lld:%lld", (PEAT_LAT)global_median_pre_delay, (PEAT_LAT)global_median_post_delay);
            fprintf(stderr, ":%lld:%lld", (PEAT_LAT)global_max_pre_delay, (PEAT_LAT)global_max_post_delay);
            fprintf(stderr, "\n");
            fprintf(stderr, "%d:%d:%lld:1000000000:%.2lf\n", PEAT_RT_TRANSPORT_Vars, t, ptp->get_trace_freq(0), most_bw_corrected[t][fid_pair] * ptp->get_trace_freq(0) / 1000000000.0);
        }
    }
}
#endif

void PET_Core::write_function_split_profile(const std::string &name)
{
    std::map<PEAT_Rank, std::string> file_name;
    file_name[0] = name;
    File_Storage_GZ profile_file(file_name, PEAT_FIO_FLAG_WRITE, NULL, globals.verbose);
    if (!profile_file.is_file_good(0)) {
        std::ostringstream tmp;
        tmp << "Can't open file (to write) " << name;
        throw std::runtime_error(tmp.str());
    }
    gzFile prof_file_ptr = profile_file.get_file_ptr(0);

    for (PEAT_Transports t = PEAT_TRANSPORT_DDR; t < PEAT_TRANSPORT_MAX; t = t_inc(t)) {
        std::map<PEAT_Func_ID_Pair, std::map<PEAT_Rank, std::map<PEAT_Message_Size, std::map<PEAT_Message_Size, std::list<MPI_Event_Stat>>>>> &stat_local = stat.get_ts_fs_stat(Event_Set_Stat::tst_trc, t);
        if (stat_local.empty())
            continue;
        for (std::map<PEAT_Func_ID_Pair, std::map<PEAT_Rank, std::map<PEAT_Message_Size, std::map<PEAT_Message_Size, std::list<MPI_Event_Stat>>>>>::const_iterator func_stat_it = stat_local.cbegin(); func_stat_it != stat_local.cend(); ++func_stat_it) {
            PEAT_Func_ID_Pair fid_pair = func_stat_it->first;
            PEAT_MPI_Func_ID func_id = fid_pair.id;
            PEAT_MPI_Func_ID parent_func_id = fid_pair.parent_id;
            for (std::map<PEAT_Rank, std::map<PEAT_Message_Size, std::map<PEAT_Message_Size, std::list<MPI_Event_Stat>>>>::const_iterator ranks_stat_it = func_stat_it->second.cbegin(); ranks_stat_it != func_stat_it->second.cend(); ++ranks_stat_it) {
                PEAT_Rank num_ranks = ranks_stat_it->first;
                fprintf(stderr, "%d:%d:%lld:%s:%s:%d", PEAT_RT_Func_Split, t, ptp->get_trace_freq(0), global_func_id.name_by_idx(func_id), global_func_id.name_by_idx(parent_func_id), num_ranks);
                for (std::map<PEAT_Message_Size, std::map<PEAT_Message_Size, std::list<MPI_Event_Stat>>>::const_iterator send_stat_it = ranks_stat_it->second.cbegin(); send_stat_it != ranks_stat_it->second.cend(); ++send_stat_it) {
                    PEAT_Message_Size send_message_size = send_stat_it->first;
                    for (std::map<PEAT_Message_Size, std::list<MPI_Event_Stat>>::const_iterator MPI_Event_Stat_it = send_stat_it->second.cbegin(); MPI_Event_Stat_it != send_stat_it->second.cend(); ++MPI_Event_Stat_it) {
                        PEAT_Message_Size recv_message_size = MPI_Event_Stat_it->first;
                        PEAT_LNum avg_prolog = 0;
                        PEAT_LNum min_prolog = LLONG_MAX;
                        PEAT_LNum median_prolog = 0;
                        PEAT_LNum max_prolog = 0;
                        PEAT_LNum ustdev_prolog = 0;
                        PEAT_LNum avg_epilog = 0;
                        PEAT_LNum min_epilog = LLONG_MAX;
                        PEAT_LNum median_epilog = 0;
                        PEAT_LNum max_epilog = 0;
                        PEAT_LNum ustdev_epilog = 0;
                        PEAT_LNum effective_events = 0;
                        for (std::list<MPI_Event_Stat>::const_iterator event_it = MPI_Event_Stat_it->second.cbegin(); event_it != MPI_Event_Stat_it->second.cend(); ++event_it) {
                            const MPI_Event_Stat &evt = *event_it;
                            if (min_prolog > evt.prolog)
                                min_prolog = evt.prolog;
                            if (max_prolog < evt.prolog)
                                max_prolog = evt.prolog;
                            if (min_epilog > evt.epilog)
                                min_epilog = evt.epilog;
                            if (max_epilog < evt.epilog)
                                max_epilog = evt.epilog;
                            effective_events++;
                            avg_prolog += evt.prolog;
                            avg_epilog += evt.epilog;
                        }
                        if (effective_events == 0)
                            continue;
                        avg_prolog /= effective_events;
                        avg_epilog /= effective_events;
                        PEAT_Clock index = 0;
                        for (std::list<MPI_Event_Stat>::const_iterator event_it = MPI_Event_Stat_it->second.cbegin(); event_it != MPI_Event_Stat_it->second.cend(); ++event_it) {
                            const MPI_Event_Stat &evt = *event_it;
                            index++;
                            ustdev_prolog += (evt.prolog - avg_prolog) * (evt.prolog - avg_prolog);
                            ustdev_epilog += (evt.epilog - avg_epilog) * (evt.epilog - avg_epilog);
                            if (index == effective_events / 2) {
                                median_prolog = evt.prolog;
                                median_epilog = evt.epilog;
                            }
                        }
                        PEAT_Ratio u_st_dev_prolog = sqrt(ustdev_prolog / effective_events);
                        PEAT_Ratio u_st_dev_epilog = sqrt(ustdev_epilog / effective_events);

                        fprintf(stderr, ":%lld:%lld", send_message_size, recv_message_size);
                        fprintf(stderr, ":%lld:%lld", (PEAT_LAT)min_prolog, (PEAT_LAT)min_epilog);
                        fprintf(stderr, ":%lld:%lld", (PEAT_LAT)avg_prolog, (PEAT_LAT)avg_epilog);
                        fprintf(stderr, ":%lld:%lld", (PEAT_LAT)median_prolog, (PEAT_LAT)median_epilog);
                        fprintf(stderr, ":%lld:%lld", (PEAT_LAT)max_prolog, (PEAT_LAT)max_epilog);
                        gzprintf(prof_file_ptr, "ID=%d, Func:%s, Size:%lld-%lld, prolog: min=%lld median=%lld avx=%lld max=%lld ustdev=%lf epilog: min=%lld median=%lld avx=%lld max=%lld ustdev=%lf\n", t, global_func_id.name_by_idx(func_id),
                                 send_message_size, recv_message_size,
                                 min_prolog, avg_prolog, median_prolog, max_prolog, u_st_dev_prolog, min_epilog, avg_epilog, median_epilog, max_epilog, u_st_dev_epilog);
                    }
                }
                fprintf(stderr, "\n");
            }
        }
    }
}

void PET_Core::no_more_events(PEAT_Bool enough_events)
{
    // gather events being estimated into one map
    std::map<PEAT_Rank, PET_Event *> events_being_estimated;
    PEAT_UID min_secondary_id = LLONG_MAX;
    PEAT_Clock min_start = LLONG_MAX;
    PET_Event *min_s_event = NULL;
    PET_Event *min_t_event = NULL;
    PET_ToR_Event *susp_event = NULL;
    std::map<PEAT_Clock, Estimation_Event *> progresses;
    for (std::vector<PET_Node>::iterator node_it = nodes.begin(); node_it != nodes.end(); ++node_it) {
        for (std::map<PEAT_Rank, PET_Rank>::iterator rank_it = node_it->ranks.begin(); rank_it != node_it->ranks.end(); ++rank_it) {
            for (std::vector<PET_Thread *>::iterator thread_it = rank_it->second.threads.begin(); thread_it != rank_it->second.threads.end(); ++thread_it) {
                if ((*thread_it)->event_being_estimated) {
                    PET_Event *event = (*thread_it)->event_being_estimated->get_estimated_event();
                    events_being_estimated[event->rank] = event;
                    if (event->get_event_type() == PET_EVENT_TYPE_TOR || event->get_event_type() == PET_EVENT_TYPE_TAR) {
                        PET_ToR_Event *t_event = static_cast<PET_ToR_Event *>(event);
                        if (t_event->tx_message.finish > 0 && t_event->rx_message.finish > 0) {
                            susp_event = t_event;
                        }
                    }
                    Estimation_Event *e_evt = rank_it->second.progress_engine.get_min_event();
                    if (e_evt)
                        progresses[e_evt->start] = e_evt;
                    if (min_secondary_id > event->secondary_id) {
                        min_s_event = event;
                    }
                    if (min_start > event->e_time.func.in) {
                        min_t_event = event;
                    }
                }
            }
        }
    }
    return;
}
void PET_Core::check_progress(PEAT_Bool enough_events, std::list<PET_Event *> &broken_events)
{
    if (!enough_events)
        useless_iterations++;
    else
        useful_iterations++;
    return;
}

PET_Event *PET_Core::get_prev_event(PET_Event *event, PEAT_Thread thread_id, PEAT_Bool reverse, PEAT_Bool use_cache)
{
    PET_Thread *pet_thread = event->get_thread();
    if (reverse) {
        std::list<PET_Event *>::reverse_iterator event_it;
        event_it = pet_thread->find_rpevent(event->e_time.func.in);
        if (event_it == pet_thread->active_events.rend() || pet_thread->active_events.empty()) {
            return NULL; // special case with collectives when they are not in the quque
        }
        //++event_it; // no need in this case
        return *event_it;
    } else {
        std::list<PET_Event *>::iterator event_it;
        event_it = pet_thread->find_event(event->e_time.func.in, use_cache);
        if (event_it == pet_thread->active_events.end() || pet_thread->active_events.empty() || event_it == pet_thread->active_events.begin()) {
            return NULL; // special case with collectives when they are not in the quque
        }
        --event_it;
        return *event_it;
    }
}
