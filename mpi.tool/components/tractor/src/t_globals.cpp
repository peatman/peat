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

#include "t_defines.h"

#if defined(__CPLUSPLUS__) || defined(__cplusplus)
extern "C" {
#endif
#ifdef USE_NANO_TIMER
PEAT_Clock t_nano_init = 0;
PEAT_Clock tsc_adjustments[MAX_CORE_ID];
PEAT_Byte tsc_adjustments_made[MAX_CORE_ID];
t_tsc_core_adjust tsc_core_adjust[MAX_CORE_ID];
PEAT_Proc lead_core_id = -1;
#endif
PEAT_Clock t_tsc_init = 0;
long long proc_freq[2] = {0, 0};
clockid_t default_timer = CLOCK_MONOTONIC_RAW;
double freq_ratio = 1.0;
double freq_ratio_uncorrected = 1.0;

#if defined(__CPLUSPLUS__) || defined(__cplusplus)
}
#endif
