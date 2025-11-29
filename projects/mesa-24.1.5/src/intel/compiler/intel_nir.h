/*
 * Copyright (c) 2015-2023 Intel Corporation
 * SPDX-License-Identifier: MIT
 */

#ifndef INTEL_NIR_H
#define INTEL_NIR_H

#include "nir.h"

#ifdef __cplusplus
extern "C" {
#endif

struct intel_device_info;

void intel_nir_apply_tcs_quads_workaround(nir_shader *nir);
bool intel_nir_blockify_uniform_loads(nir_shader *shader,
                                      const struct intel_device_info *devinfo);
bool intel_nir_clamp_image_1d_2d_array_sizes(nir_shader *shader);
bool intel_nir_clamp_per_vertex_loads(nir_shader *shader);
bool intel_nir_cleanup_resource_intel(nir_shader *shader);

bool intel_nir_lower_conversions(nir_shader *nir);
bool intel_nir_lower_non_uniform_barycentric_at_sample(nir_shader *nir);
bool intel_nir_lower_non_uniform_resource_intel(nir_shader *shader);
bool intel_nir_lower_patch_vertices_in(nir_shader *shader, unsigned input_vertices);
bool intel_nir_lower_shading_rate_output(nir_shader *nir);
bool intel_nir_lower_sparse_intrinsics(nir_shader *nir);

struct intel_nir_lower_texture_opts {
   bool combined_lod_and_array_index;
   bool combined_lod_or_bias_and_offset;
};
bool intel_nir_lower_texture(nir_shader *nir,
                             const struct intel_nir_lower_texture_opts *opts);

bool intel_nir_opt_peephole_ffma(nir_shader *shader);
bool intel_nir_opt_peephole_imul32x16(nir_shader *shader);

bool intel_nir_pulls_at_sample(nir_shader *shader);

#ifdef __cplusplus
}
#endif

#endif /* INTEL_NIR_H */
