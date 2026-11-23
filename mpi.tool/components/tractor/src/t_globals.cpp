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
/* defined with either timer: a shim built with the other one has to load, find the mismatch (t_shim_state_check) and
   stay inert, instead of stopping the program with an undefined symbol */
PEAT_Clock t_nano_init = 0;
PEAT_Clock t_tsc_init = 0;
long long proc_freq[2] = {0, 0};
clockid_t default_timer = CLOCK_MONOTONIC_RAW;

#if defined(__CPLUSPLUS__) || defined(__cplusplus)
}
#endif
