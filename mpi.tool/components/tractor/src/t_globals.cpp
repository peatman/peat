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

/* time spent in the calls of each class and the switches of the classes: the core's, used by the MPI layer and the
   event-class wrappers (built in or shims) */
PEAT_Clock global_tool_overhead = 0;
PEAT_Clock global_mpi_time = 0;
PEAT_Clock global_gpu_time = 0;
PEAT_Clock global_llama_time = 0; /* llama.cpp/ggml stages (_T_LLAMA_); nested stages are counted at every level */
PEAT_Clock global_cuda_time = 0;  /* CUDA records were taken (the CUDA class): tractor::end() writes the trace of a non-MPI process */
PEAT_Clock global_parallel_app_time = 0;
PEAT_Clock global_parallel_start = 0;
PEAT_Clock global_parallel_stop = 0;
PEAT_Clock global_progress_calls = 0;
PEAT_Clock global_progress_yields = 0;
int mpi_skip_parse = 0;
int gpu_skip_parse = 0;
int llama_skip_parse = 0;    /* llama.cpp/ggml pipeline wrappers (_T_LLAMA_), PEAT_TRACTOR_PARSE_LLAMA=0 sets it */
int ggml_cpu_skip_parse = 1; /* ggml CPU kernels (_T_LLAMA_), off unless PEAT_TRACTOR_PARSE_GGML_CPU=1: one record per thread per node */
int cuda_skip_parse = 1;     /* CUDA (CUPTI, t_cupti.cpp) and cuBLAS (_T_CUPTI_), off unless PEAT_TRACTOR_PARSE_CUDA=1 */

#if defined(__CPLUSPLUS__) || defined(__cplusplus)
}
#endif
