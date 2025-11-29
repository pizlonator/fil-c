/*
 * Copyright 2023 Alyssa Rosenzweig
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "nir.h"
#include "shader_enums.h"

enum mesa_prim;

struct agx_lower_output_to_var_state {
   struct nir_variable *outputs[NUM_TOTAL_VARYING_SLOTS];
};

bool agx_lower_output_to_var(struct nir_builder *b, struct nir_instr *instr,
                             void *data);

struct nir_def *agx_load_per_vertex_input(struct nir_builder *b,
                                          nir_intrinsic_instr *intr,
                                          struct nir_def *vertex);

struct nir_def *agx_vertex_id_for_topology_class(struct nir_builder *b,
                                                 struct nir_def *vert,
                                                 enum mesa_prim clas);

bool agx_nir_lower_index_buffer(struct nir_shader *s, unsigned index_size_B,
                                bool patches);

bool agx_nir_lower_sw_vs_id(nir_shader *s);

bool agx_nir_lower_vs_before_gs(struct nir_shader *vs,
                                const struct nir_shader *libagx,
                                uint64_t *outputs);

bool agx_nir_lower_gs(struct nir_shader *gs, const struct nir_shader *libagx,
                      bool rasterizer_discard, struct nir_shader **gs_count,
                      struct nir_shader **gs_copy, struct nir_shader **pre_gs,
                      enum mesa_prim *out_mode, unsigned *out_count_words);

void agx_nir_prefix_sum_gs(struct nir_builder *b, const void *data);

struct agx_gs_setup_indirect_key {
   enum mesa_prim prim;
};

void agx_nir_gs_setup_indirect(struct nir_builder *b, const void *key);

struct agx_unroll_restart_key {
   enum mesa_prim prim;
   unsigned index_size_B;
};

void agx_nir_unroll_restart(struct nir_builder *b, const void *key);

bool agx_nir_lower_tcs(struct nir_shader *tcs, const struct nir_shader *libagx);

bool agx_nir_lower_tes(struct nir_shader *tes, const struct nir_shader *libagx);

uint64_t agx_tcs_per_vertex_outputs(const struct nir_shader *nir);

unsigned agx_tcs_output_stride(const struct nir_shader *nir);
