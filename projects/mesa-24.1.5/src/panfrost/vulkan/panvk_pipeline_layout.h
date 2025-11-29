/*
 * Copyright © 2021 Collabora Ltd.
 * SPDX-License-Identifier: MIT
 */

#ifndef PANVK_PIPELINE_LAYOUT_H
#define PANVK_PIPELINE_LAYOUT_H

#ifndef PAN_ARCH
#error "PAN_ARCH must be defined"
#endif

#include <stdint.h>

#include "vk_pipeline_layout.h"

#include "panvk_descriptor_set_layout.h"
#include "panvk_macros.h"

#define MAX_SETS                    4
#define MAX_DYNAMIC_UNIFORM_BUFFERS 16
#define MAX_DYNAMIC_STORAGE_BUFFERS 8
#define MAX_DYNAMIC_BUFFERS                                                    \
   (MAX_DYNAMIC_UNIFORM_BUFFERS + MAX_DYNAMIC_STORAGE_BUFFERS)

struct panvk_pipeline_layout {
   struct vk_pipeline_layout vk;

   unsigned char sha1[20];

   unsigned num_samplers;
   unsigned num_textures;
   unsigned num_ubos;
   unsigned num_dyn_ubos;
   unsigned num_dyn_ssbos;
   uint32_t num_imgs;

   struct {
      uint32_t size;
   } push_constants;

   struct {
      unsigned sampler_offset;
      unsigned tex_offset;
      unsigned ubo_offset;
      unsigned dyn_ubo_offset;
      unsigned dyn_ssbo_offset;
      unsigned img_offset;
   } sets[MAX_SETS];
};

VK_DEFINE_NONDISP_HANDLE_CASTS(panvk_pipeline_layout, vk.base, VkPipelineLayout,
                               VK_OBJECT_TYPE_PIPELINE_LAYOUT)

unsigned panvk_per_arch(pipeline_layout_ubo_start)(
   const struct panvk_pipeline_layout *layout, unsigned set, bool is_dynamic);

unsigned panvk_per_arch(pipeline_layout_ubo_index)(
   const struct panvk_pipeline_layout *layout, unsigned set, unsigned binding,
   unsigned array_index);

#endif
