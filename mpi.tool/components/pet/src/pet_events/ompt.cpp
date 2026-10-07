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
#include <assert.h>
#include <errno.h>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits.h>
#include <numeric> //std::accumulate
#include <set>
#include <sstream>
#include <stdexcept>
#include <stdlib.h>

#include "pet_core.h"
#include "stat.h"
#include "utils.h"

#ifdef PEAT_MPI
#include <mpi.h>
#endif

PET_OMPT_Event::PET_OMPT_Event(PET_Core *p) : PET_Local_Event(p), pair_event(NULL), t0(NULL), is_all_estimated(false), kind(-1), endpoint(-1), parallel_data(-1), task_data(-1), addr_id(-1)
{
}

PET_OMPT_Event::~PET_OMPT_Event()
{
}

PEAT_Bool PET_OMPT_Event::read_event(PEAT_Rank r_rank, const String_Split &mpi_string, const String_Split &hwc_string, PET_Event *host, const PEAT_Record_Header &hdr)
{
    PEAT_Bool r = PET_Local_Event::read_event(r_rank, mpi_string, hwc_string, host, hdr);
    extract(kind, mpi_string.at(hdr.payload + 1));
    extract(endpoint, mpi_string.at(hdr.payload + 2));
    extract(task_data, mpi_string.at(hdr.payload + 3));
    extract(parallel_data, mpi_string.at(hdr.payload + 4));
    extract(addr_id, mpi_string.at(hdr.payload + 5));
    return r;
}

PET_Event *PET_OMPT_Event::write_event(File_Storage_GZ *gz_MPI, File_Storage_GZ *gz_HWC, File_Storage_GZ *gz_bind, const char *mpi_eol, const char *hwc_eol)
{
    if (thread_id == 0 || (func_id != OMPT_PARALLEL_BEGIN_ID && func_id != OMPT_PARALLEL_END_ID))
        return PET_Local_Event::write_event(gz_MPI, gz_HWC, gz_bind, mpi_eol, hwc_eol);
    is_written = true;
    return NULL;
}
void PET_OMPT_Event::insert(PEAT_Bool skip_nested) // special version for children
{
    pet_thread->check_nested(this);
    if (func_id == OMPT_PARALLEL_END_ID) {
        // adjust event in case of timers errors
        PET_Event *last_event = get_last_event(); // get_prev_event(true);
        if (last_event) {
            if (last_event->thread_id == thread_id) {
                if (last_event->e_time.func.out > e_time.func.in) {
                    move_event(e_time.func.in - last_event->e_time.func.out - 1);
                }
            } else {
                assert(0); // at least parallel begin has to be present
            }
        } else {
            assert(0); // at least parallel begin has to be present
        }
    }
    /*
    if (func_id == OMPT_PARALLEL_BEGIN_ID) {
        //adjust events in case of timers errors
        PET_Event* last_event = event->get_prev_event();
        if (prev_event) {
            if (last_event ->thread_id == t) {
                if (last_event->e_time.func.out > e_time.func.in) {
                    last_event->move_event(e_time.func.out - last_event->e_time.func.in - 1); //move orphan event to parallel section
                    last_event->assign_parallel_section_begin(event); // assign parallel section for it
                    //re-order thread
                }
            }
        }


    }*/
    PET_Local_Event::insert();
    if (func_id == OMPT_PARALLEL_BEGIN_ID) {
        pet_thread->insert_parallel_section(this);
    }
    if (func_id == OMPT_PARALLEL_END_ID) {
        PET_OMPT_Event *begin = pet_thread->remove_parallel_section();
        if (begin->parallel_data != parallel_data) {
            pet_throw_error(get_core(), this, NULL, "Pair Event mismatch\n");
        }
        assign_parallel_section_begin(begin);
        begin->assign_pair_event(this);
    }
}
void PET_OMPT_Event::insert()
{
    PET_Core *core = get_core();
    pet_thread->adjust_nested(this);
    if (thread_id == 0 && (func_id == OMPT_PARALLEL_BEGIN_ID || func_id == OMPT_PARALLEL_END_ID)) {
        PEAT_Thread max_thread_id = core->get_max_thread_id(rank);
        sections.resize(max_thread_id + 1);
        sections[0] = this;
        for (PEAT_Thread t = 1; t <= max_thread_id; t++) {
            PET_OMPT_Event *event;
            event = new (PET_OMPT_Event)(core);
            *event = *this;
            event->thread_id = t;
            event->set_t0(this);
            event->sections.clear();
            event->assign_cache_pointers();
            event->insert(true);
            sections[t] = event;
        }
    }
    if (func_id == OMPT_PARALLEL_END_ID) {
        // adjust event in case of timers errors
        PET_Event *last_event = get_last_event(); // get_prev_event(true);
        if (last_event) {
            if (last_event->thread_id == thread_id) {
                if (last_event->e_time.func.out > e_time.func.in) {
                    move_event(e_time.func.in - last_event->e_time.func.out - 1);
                }
            } else {
                assert(0); // at least parallel begin has to be present
            }
        } else {
            assert(0); // at least parallel begin has to be present
        }
    }
    /*
    if (func_id == OMPT_PARALLEL_BEGIN_ID) {
        //adjust events in case of timers errors
        PET_Event* last_event = event->get_prev_event();
        if (prev_event) {
            if (last_event ->thread_id == t) {
                if (last_event->e_time.func.out > e_time.func.in) {
                    last_event->move_event(e_time.func.out - last_event->e_time.func.in - 1); //move orphan event to parallel section
                    last_event->assign_parallel_section_begin(event); // assign parallel section for it
                    //re-order thread
                }
            }
        }


    }*/
    PET_Local_Event::insert();
    if (func_id == OMPT_PARALLEL_BEGIN_ID) {
        pet_thread->insert_parallel_section(this);
    }
    if (func_id == OMPT_PARALLEL_END_ID) {
        PET_OMPT_Event *begin = pet_thread->remove_parallel_section();
        assign_parallel_section_begin(begin);
        begin->assign_pair_event(this);
    }
};

PEAT_Bool PET_OMPT_Event::estimate(Estimation_Event *event)
{
    // if ( event == NULL ) return;
    switch (event->type) {
    case EET_FUNC_START:
        // assert(is_estimated() == false);
        if (is_estimated()) {
            pet_throw_error(pet_core, this, event, "Repeated Estimation Attempt.\n");
        }
        estimation_started = true;
        /*
                if (!get_core()->get_mo()) {
                    split_event();
                    concatenate_event();
                }
        */
        submit(e_time.func.out, EET_FUNC_END, this, this, NULL);
        break;
    case EET_FUNC_END:
        if (!estimation_started) {
            pet_throw_error(pet_core, this, event, "Function is not estimated.\n");
        }
        // assert(estimation_started);
        set_estimated();
        put_transport_event_stat();
        if (is_estimated()) { // checkl if other paralle_ends finish
            update_finish();
        }
        break;
    case EET_PROGRESS_NOTIFY:
        break;
    case EET_NOTIFY:
        break;
    default:
        pet_throw_error(pet_core, this, event, "Inappropriate  Code.\n");
        // assert(0);
        break;
    }
    get_core()->delete_ee_entry(event);
    return true;
}

PET_Event *PET_OMPT_Event::get_prev_event(PEAT_Bool reverse)
{
    if (func_id == OMPT_PARALLEL_BEGIN_ID && thread_id > 0 && t0)
        return t0->get_prev_event(reverse);
    PET_Event *evt = PET_Local_Event::get_prev_event(reverse);
    return evt;
};

PET_Event *PET_OMPT_Event::get_last_event()
{
    PET_Event *evt = get_core()->get_last_event_long(this);
    return evt;
};

PEAT_Bool PET_OMPT_Event::can_be_destroyed()
{
    PEAT_Bool answer = true;
    if (func_id == OMPT_PARALLEL_BEGIN_ID) {
        assert(pair_event);
        answer = pair_event->is_all_estimated;
    }
    return answer && PET_Event::can_be_destroyed();
}

PEAT_Bool PET_OMPT_Event::initiate()
{
    if (is_initiated || is_estimated()) {
        return true;
    }
    if (get_parallel_section_begin() == NULL && func_id != OMPT_PARALLEL_BEGIN_ID) {
        is_initiated = true;
        set_estimated(); // assume this orphan event estimated to prevent pipeline from falling
        submit(0, NULL);
    } else {
        // place func start into estmation queue
        submit(e_time.mpi.in, EET_FUNC_START, this, this, NULL);
        is_initiated = true;
    }
    return true;
}
// TODO: ADD better syncpoints calculations
void PET_OMPT_Event::update_finish()
{
    if (is_written)
        return;
    if (func_id != OMPT_PARALLEL_END_ID)
        return;
    if (t0) {
        t0->update_finish();
        return;
    }
    if (thread_id != 0) {
        // pet_throw_error(get_core(),  this, NULL, "Incorrect Thread ID\n");
        return;
    }
    if (!is_estimated())
        return;
    PEAT_Clock t0_t_out = e_time.func.out;
    PEAT_Clock t_out = e_time.func.out;
    for (std::vector<PET_OMPT_Event *>::iterator thread_it = sections.begin(); thread_it != sections.end(); ++thread_it) {
        if (t_out < (*thread_it)->e_time.func.out)
            t_out = (*thread_it)->e_time.func.out;
    }
    PEAT_Clock sub_t = t0_t_out - t_out;
    adjust_event(sub_t);
    // sub_t_ae += sub_t;
    concatenate_event();
    get_core()->update_active_event_chain(sub_t, this);
}
