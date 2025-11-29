/*
 * Copyright © 2016 Red Hat.
 * Copyright © 2016 Bas Nieuwenhuizen
 *
 * based in part on anv driver which is:
 * Copyright © 2015 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef RADV_PIPELINE_RT_H
#define RADV_PIPELINE_RT_H

#include "radv_pipeline_compute.h"
#include "radv_shader.h"

struct radv_ray_tracing_pipeline {
   struct radv_compute_pipeline base;

   struct radv_shader *prolog;

   struct radv_ray_tracing_stage *stages;
   struct radv_ray_tracing_group *groups;
   unsigned stage_count;
   unsigned non_imported_stage_count;
   unsigned group_count;

   uint32_t stack_size;

   /* set if any shaders from this pipeline require robustness2 in the merged traversal shader */
   bool traversal_storage_robustness2 : 1;
   bool traversal_uniform_robustness2 : 1;
};

RADV_DECL_PIPELINE_DOWNCAST(ray_tracing, RADV_PIPELINE_RAY_TRACING)

struct radv_pipeline_group_handle {
   uint64_t recursive_shader_ptr;

   union {
      uint32_t general_index;
      uint32_t closest_hit_index;
   };
   union {
      uint32_t intersection_index;
      uint32_t any_hit_index;
   };
};

struct radv_rt_capture_replay_handle {
   struct radv_serialized_shader_arena_block recursive_shader_alloc;
   uint32_t non_recursive_idx;
};

struct radv_ray_tracing_group {
   VkRayTracingShaderGroupTypeKHR type;
   uint32_t recursive_shader; /* generalShader or closestHitShader */
   uint32_t any_hit_shader;
   uint32_t intersection_shader;
   struct radv_pipeline_group_handle handle;
};

enum radv_rt_const_arg_state {
   RADV_RT_CONST_ARG_STATE_UNINITIALIZED,
   RADV_RT_CONST_ARG_STATE_VALID,
   RADV_RT_CONST_ARG_STATE_INVALID,
};

struct radv_rt_const_arg_info {
   enum radv_rt_const_arg_state state;
   uint32_t value;
};

struct radv_ray_tracing_stage_info {
   bool can_inline;

   BITSET_DECLARE(unused_args, AC_MAX_ARGS);

   struct radv_rt_const_arg_info tmin;
   struct radv_rt_const_arg_info tmax;

   struct radv_rt_const_arg_info sbt_offset;
   struct radv_rt_const_arg_info sbt_stride;

   struct radv_rt_const_arg_info miss_index;

   uint32_t set_flags;
   uint32_t unset_flags;
};

struct radv_ray_tracing_stage {
   struct vk_pipeline_cache_object *nir;
   struct radv_shader *shader;
   gl_shader_stage stage;
   uint32_t stack_size;

   struct radv_ray_tracing_stage_info info;

   uint8_t sha1[SHA1_DIGEST_LENGTH];
};

void radv_destroy_ray_tracing_pipeline(struct radv_device *device, struct radv_ray_tracing_pipeline *pipeline);

#endif /* RADV_PIPELINE_RT */
