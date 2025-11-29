/*
 * Copyright © 2021 Collabora Ltd.
 * SPDX-License-Identifier: MIT
 */

#ifndef PANVK_PIPELINE_H
#define PANVK_PIPELINE_H

#ifndef PAN_ARCH
#error "PAN_ARCH must be defined"
#endif

#include <stdbool.h>
#include <stdint.h>

#include "vk_object.h"

#include "util/pan_ir.h"

#include "pan_blend.h"
#include "pan_desc.h"

#include "panvk_varyings.h"

#define MAX_RTS 8

struct panvk_attrib_info {
   unsigned buf;
   unsigned offset;
   enum pipe_format format;
};

struct panvk_attrib_buf_info {
   bool special;
   union {
      struct {
         unsigned stride;
         bool per_instance;
         uint32_t instance_divisor;
      };
      unsigned special_id;
   };
};

struct panvk_attribs_info {
   struct panvk_attrib_info attrib[PAN_MAX_ATTRIBUTE];
   unsigned attrib_count;
   struct panvk_attrib_buf_info buf[PAN_MAX_ATTRIBUTE];
   unsigned buf_count;
};

struct panvk_pipeline {
   struct vk_object_base base;

   struct panvk_varyings_info varyings;
   struct panvk_attribs_info attribs;

   const struct panvk_pipeline_layout *layout;

   unsigned active_stages;

   uint32_t dynamic_state_mask;

   struct panvk_priv_bo *binary_bo;
   struct panvk_priv_bo *state_bo;

   uint64_t vpd;
   uint64_t rsds[MESA_SHADER_STAGES];

   /* shader stage bit is set of the stage accesses storage images */
   uint32_t img_access_mask;

   unsigned num_ubos;

   struct {
      unsigned ubo_idx;
   } sysvals[MESA_SHADER_STAGES];

   unsigned tls_size;
   unsigned wls_size;

   struct {
      uint64_t address;
      struct pan_shader_info info;
      struct mali_renderer_state_packed rsd_template;
      bool required;
      bool dynamic_rsd;
      uint8_t rt_mask;
   } fs;

   struct {
      struct pan_compute_dim local_size;
   } cs;

   struct {
      unsigned topology;
      bool writes_point_size;
      bool primitive_restart;
   } ia;

   struct {
      bool clamp_depth;
      float line_width;
      struct {
         bool enable;
         float constant_factor;
         float clamp;
         float slope_factor;
      } depth_bias;
      bool front_ccw;
      bool cull_front_face;
      bool cull_back_face;
      bool enable;
   } rast;

   struct {
      bool z_test;
      bool z_write;
      unsigned z_compare_func;
      bool s_test;
      struct {
         unsigned fail_op;
         unsigned pass_op;
         unsigned z_fail_op;
         unsigned compare_func;
         uint8_t compare_mask;
         uint8_t write_mask;
         uint8_t ref;
      } s_front, s_back;
   } zs;

   struct {
      uint8_t rast_samples;
      uint8_t min_samples;
      uint16_t sample_mask;
      bool alpha_to_coverage;
      bool alpha_to_one;
   } ms;

   struct {
      struct pan_blend_state state;
      struct mali_blend_packed bd_template[8];
      struct {
         uint8_t index;
         uint16_t bifrost_factor;
      } constant[8];
      bool reads_dest;
   } blend;

   VkViewport viewport;
   VkRect2D scissor;
};

VK_DEFINE_NONDISP_HANDLE_CASTS(panvk_pipeline, base, VkPipeline,
                               VK_OBJECT_TYPE_PIPELINE)

#endif
