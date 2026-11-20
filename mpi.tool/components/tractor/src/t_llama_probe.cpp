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

// Build probe of the llama.cpp layer (LLAMA_PARSE, not part of the library): compiled by the Makefile and CMakeLists.txt
// against the llama.cpp headers found. Every public function tractor wraps (the CLASS_LLAMA entries of mpi_func.inc and
// ggml_graph_compute of CLASS_GGML_CPU) is taken with the exact signature of its wrapper: a llama.cpp with another API
// (older, without llama_process, or changed parameters - two are structures passed by value) fails here, and is then
// left out (LLAMA_PARSE=auto) or stops the build (LLAMA_PARSE=on), instead of breaking the wrappers.
// Keep in sync with the CLASS_LLAMA entries of mpi_func.inc.

#include "llama.h"

typedef struct llama_model *(*t_model_load_from_file)(const char *, struct llama_model_params);
typedef struct llama_context *(*t_init_from_model)(struct llama_model *, struct llama_context_params);
typedef int32_t (*t_tokenize)(const struct llama_vocab *, const char *, int32_t, llama_token *, int32_t, bool, bool);
typedef int32_t (*t_process)(struct llama_context *, enum llama_process_type, struct llama_batch_ext *);
typedef float *(*t_get_logits_ith)(struct llama_context *, int32_t);
typedef llama_token (*t_sampler_sample)(struct llama_sampler *, struct llama_context *, int32_t);
typedef int32_t (*t_token_to_piece)(const struct llama_vocab *, llama_token, char *, int32_t, int32_t, bool);
typedef void (*t_free)(struct llama_context *);
typedef void (*t_model_free)(struct llama_model *);
typedef bool (*t_sched_alloc_graph)(ggml_backend_sched_t, struct ggml_cgraph *);
typedef enum ggml_status (*t_sched_graph_compute_async)(ggml_backend_sched_t, struct ggml_cgraph *);
typedef void (*t_sched_synchronize)(ggml_backend_sched_t);
typedef enum ggml_status (*t_graph_compute)(struct ggml_cgraph *, struct ggml_cplan *);

static t_model_load_from_file p_model_load_from_file = llama_model_load_from_file;
static t_init_from_model p_init_from_model = llama_init_from_model;
static t_tokenize p_tokenize = llama_tokenize;
static t_process p_process = llama_process;
static t_get_logits_ith p_get_logits_ith = llama_get_logits_ith;
static t_sampler_sample p_sampler_sample = llama_sampler_sample;
static t_token_to_piece p_token_to_piece = llama_token_to_piece;
static t_free p_free = llama_free;
static t_model_free p_model_free = llama_model_free;
static t_sched_alloc_graph p_sched_alloc_graph = ggml_backend_sched_alloc_graph;
static t_sched_graph_compute_async p_sched_graph_compute_async = ggml_backend_sched_graph_compute_async;
static t_sched_synchronize p_sched_synchronize = ggml_backend_sched_synchronize;
static t_graph_compute p_graph_compute = ggml_graph_compute;

#ifdef _PARSE_LLAMA_DEPRECATED_
typedef int32_t (*t_decode)(struct llama_context *, struct llama_batch);
static t_decode p_decode = llama_decode;
static t_decode p_encode = llama_encode;
#endif

int main()
{
    // referenced, so that -Wunused does not matter; never run (the probe is only compiled)
    return p_model_load_from_file == 0 || p_init_from_model == 0 || p_tokenize == 0 || p_process == 0 || p_get_logits_ith == 0 ||
           p_sampler_sample == 0 || p_token_to_piece == 0 || p_free == 0 || p_model_free == 0 || p_sched_alloc_graph == 0 ||
           p_sched_graph_compute_async == 0 || p_sched_synchronize == 0 || p_graph_compute == 0
#ifdef _PARSE_LLAMA_DEPRECATED_
           || p_decode == 0 || p_encode == 0
#endif
        ;
}
