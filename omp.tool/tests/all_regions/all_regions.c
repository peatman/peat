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

/*
 * Exercises every OpenMP construct that ompcalls.cpp intercepts, so one run
 * yields at least one occurrence of every event libompstat can emit.
 * Run with OMP_NUM_THREADS >= 2 (run.sh uses 4).
 *
 *   omp_control_tool(start/end)      -> START_ONDEMAND / STOP_ONDEMAND
 *   parallel                         -> FORK_CALL, MICROTASK_*
 *   implicit barrier / worker wait   -> JOIN_BARRIER_*, FORK_BARRIER_*
 *   barrier                          -> PLAIN_BARRIER_*
 *   single, master                   -> SINGLE_* (master is folded in)
 *   critical                         -> CRITICAL_SECTION_*, CRITICAL_BARRIER_*
 *   task, taskwait                   -> TASK_*, TASKWAIT_*
 *   parallel nested in parallel      -> SERIALIZED_MICROTASK_* (nesting is off
 *                                       by default, so team size is 1)
 *
 * APPLICATION_STARTS/ENDS come from the tool's initialize()/finalize().
 */

#include <omp.h>
#include <stdio.h>

int main(void)
{
    volatile long sum = 0;

    omp_control_tool(omp_control_tool_start, 0, NULL);

#pragma omp parallel num_threads(4)
    {
#pragma omp master
        {
            printf("master: %d threads\n", omp_get_num_threads());
        }

#pragma omp single
        {
            sum += 1;
        }

#pragma omp critical
        {
            sum += 1;
        }

#pragma omp barrier

#pragma omp task
        {
#pragma omp atomic
            sum += 1;
        }
#pragma omp taskwait

#pragma omp parallel
        {
#pragma omp atomic
            sum += 1;
        }
    }

#pragma omp parallel num_threads(4)
    {
#pragma omp atomic
        sum += 1;
    }

    omp_control_tool(omp_control_tool_end, 0, NULL);

    printf("sum=%ld\n", sum);
    return 0;
}
