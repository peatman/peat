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
 * Two parallel regions with a 300 ms serial gap between them. During the gap
 * the worker is idle: that time has to be reported as FORK_BARRIER, and the
 * JOIN_BARRIER of the worker has to be short (the worker only registers at
 * the end of the region). See run.sh.
 */

#include <omp.h>
#include <unistd.h>

int main(void)
{
    volatile int x = 0;

#pragma omp parallel num_threads(2)
    {
        x++;
    }

    usleep(300000);

#pragma omp parallel num_threads(2)
    {
        x++;
    }
    return 0;
}
