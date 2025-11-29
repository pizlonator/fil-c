/*
 * Copyright © 2021 Collabora Ltd.
 * SPDX-License-Identifier: MIT
 */

#ifndef PANVK_SHADER_H
#define PANVK_SHADER_H

#ifndef PAN_ARCH
#error "PAN_ARCH must be defined"
#endif

#include "util/u_dynarray.h"

#include "util/pan_ir.h"

#include "pan_desc.h"

#include "panvk_descriptor_set.h"
#include "panvk_macros.h"
#include "panvk_pipeline_layout.h"

#define PANVK_SYSVAL_UBO_INDEX     0
#define PANVK_NUM_BUILTIN_UBOS     1

struct nir_shader;
struct pan_blend_state;
struct panvk_device;

union panvk_sysval_vec4 {
   float f32[4];
   uint32_t u32[4];
};

struct panvk_sysvals {
   union {
      struct {
         /* Only for graphics */
         union panvk_sysval_vec4 viewport_scale;
         union panvk_sysval_vec4 viewport_offset;
         union panvk_sysval_vec4 blend_constants;

         uint32_t first_vertex;
         uint32_t base_vertex;
         uint32_t base_instance;
      };

      struct {
         /* Only for compute */
         union panvk_sysval_vec4 num_work_groups;
         union panvk_sysval_vec4 local_group_size;
      };
   };

   /* The back-end compiler doesn't know about any sysvals after this point */

   struct panvk_ssbo_addr dyn_ssbos[MAX_DYNAMIC_STORAGE_BUFFERS];
};

struct panvk_shader {
   struct pan_shader_info info;
   struct util_dynarray binary;
   unsigned sysval_ubo;
   struct pan_compute_dim local_size;
   bool has_img_access;
};

bool panvk_per_arch(blend_needs_lowering)(const struct panvk_device *dev,
                                          const struct pan_blend_state *state,
                                          unsigned rt);

struct panvk_shader *panvk_per_arch(shader_create)(
   struct panvk_device *dev, gl_shader_stage stage,
   const VkPipelineShaderStageCreateInfo *stage_info,
   const struct panvk_pipeline_layout *layout, unsigned sysval_ubo,
   struct pan_blend_state *blend_state, bool static_blend_constants,
   const VkAllocationCallbacks *alloc);

void panvk_per_arch(shader_destroy)(struct panvk_device *dev,
                                    struct panvk_shader *shader,
                                    const VkAllocationCallbacks *alloc);

bool panvk_per_arch(nir_lower_descriptors)(
   struct nir_shader *nir, struct panvk_device *dev,
   const struct panvk_pipeline_layout *layout, bool *has_img_access_out);

#endif
