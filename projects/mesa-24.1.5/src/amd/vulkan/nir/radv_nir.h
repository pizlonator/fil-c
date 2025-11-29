/*
 * Copyright © 2023 Valve Corporation
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef RADV_NIR_H
#define RADV_NIR_H

#include <stdbool.h>
#include <stdint.h>
#include "amd_family.h"
#include "nir.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct nir_shader nir_shader;
struct radeon_info;
struct radv_pipeline_layout;
struct radv_shader_stage;
struct radv_shader_info;
struct radv_shader_args;
struct radv_shader_layout;
struct radv_device;
struct radv_graphics_state_key;

void radv_nir_apply_pipeline_layout(nir_shader *shader, struct radv_device *device,
                                    const struct radv_shader_stage *stage);

void radv_nir_lower_abi(nir_shader *shader, enum amd_gfx_level gfx_level, const struct radv_shader_stage *stage,
                        const struct radv_graphics_state_key *gfx_state, uint32_t address32_hi);

bool radv_nir_lower_hit_attrib_derefs(nir_shader *shader);

bool radv_nir_lower_ray_payload_derefs(nir_shader *shader, uint32_t offset);

bool radv_nir_lower_ray_queries(nir_shader *shader, struct radv_device *device);

bool radv_nir_lower_vs_inputs(nir_shader *shader, const struct radv_shader_stage *vs_stage,
                              const struct radv_graphics_state_key *gfx_state, const struct radeon_info *gpu_info);

bool radv_nir_lower_primitive_shading_rate(nir_shader *nir, enum amd_gfx_level gfx_level);

bool radv_nir_lower_fs_intrinsics(nir_shader *nir, const struct radv_shader_stage *fs_stage,
                                  const struct radv_graphics_state_key *gfx_state);

bool radv_nir_lower_fs_barycentric(nir_shader *shader, const struct radv_graphics_state_key *gfx_state,
                                   unsigned rast_prim);

bool radv_nir_lower_intrinsics_early(nir_shader *nir, bool lower_view_index_to_zero);

bool radv_nir_lower_view_index(nir_shader *nir, bool per_primitive);

bool radv_nir_lower_viewport_to_zero(nir_shader *nir);

bool radv_nir_export_multiview(nir_shader *nir);

void radv_nir_lower_io_to_scalar_early(nir_shader *nir, nir_variable_mode mask);

unsigned radv_map_io_driver_location(unsigned semantic);

void radv_nir_lower_io(struct radv_device *device, nir_shader *nir);

bool radv_nir_lower_io_to_mem(struct radv_device *device, struct radv_shader_stage *stage);

void radv_nir_lower_poly_line_smooth(nir_shader *nir, const struct radv_graphics_state_key *gfx_state);

bool radv_nir_lower_cooperative_matrix(nir_shader *shader, unsigned wave_size);

bool radv_nir_lower_draw_id_to_zero(nir_shader *shader);

#ifdef __cplusplus
}
#endif

#endif /* RADV_NIR_H */
