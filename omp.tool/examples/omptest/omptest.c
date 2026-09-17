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

#include <stdio.h>

static __inline__ long long rdtsc(void)
{
    unsigned long long high, low;
    __asm__ __volatile__("rdtsc" : "=a"(low), "=d"(high));
    return (high << 32) + low;
}

int main()
{
    long long t = 0;
#pragma omp parallel for reduction(+ : t) /* This loop is to initialize ompt layer */
    for (long long a = 0; a < 100; a++) {
        t += a;
    }

    long long start = rdtsc();
#pragma omp parallel for reduction(+ : t)
    for (long long a = 0; a < 10000000; a++) {
        t += a * a;
    }
    long long stop = rdtsc();
    printf("%lld,%lld\n", t, stop - start);
    return 0;
}