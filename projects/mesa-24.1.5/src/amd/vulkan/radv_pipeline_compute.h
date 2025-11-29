/*
 * Copyright © 2016 Red Hat.
 * Copyright © 2016 Bas Nieuwenhuizen
 *
 * based in part on anv driver which is:
 * Copyright © 2015 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef RADV_PIPELINE_COMPUTE_H
#define RADV_PIPELINE_COMPUTE_H

#include "radv_pipeline.h"

struct radv_physical_device;
struct radv_shader_binary;

struct radv_compute_pipeline {
   struct radv_pipeline base;

   struct {
      uint64_t va;
      uint64_t size;
   } indirect;
};

RADV_DECL_PIPELINE_DOWNCAST(compute, RADV_PIPELINE_COMPUTE)

struct radv_compute_pipeline_metadata {
   uint32_t shader_va;
   uint32_t rsrc1;
   uint32_t rsrc2;
   uint32_t rsrc3;
   uint32_t compute_resource_limits;
   uint32_t block_size_x;
   uint32_t block_size_y;
   uint32_t block_size_z;
   uint32_t wave32;
   uint32_t grid_base_sgpr;
   uint32_t push_const_sgpr;
   uint64_t inline_push_const_mask;
};

void radv_get_compute_pipeline_metadata(const struct radv_device *device, const struct radv_compute_pipeline *pipeline,
                                        struct radv_compute_pipeline_metadata *metadata);

void radv_emit_compute_shader(const struct radv_physical_device *pdev, struct radeon_cmdbuf *cs,
                              const struct radv_shader *shader);

void radv_compute_pipeline_init(const struct radv_device *device, struct radv_compute_pipeline *pipeline,
                                const struct radv_pipeline_layout *layout, struct radv_shader *shader);

struct radv_shader *radv_compile_cs(struct radv_device *device, struct vk_pipeline_cache *cache,
                                    struct radv_shader_stage *cs_stage, bool keep_executable_info,
                                    bool keep_statistic_info, bool is_internal, struct radv_shader_binary **cs_binary);

VkResult radv_compute_pipeline_create(VkDevice _device, VkPipelineCache _cache,
                                      const VkComputePipelineCreateInfo *pCreateInfo,
                                      const VkAllocationCallbacks *pAllocator, VkPipeline *pPipeline);

void radv_destroy_compute_pipeline(struct radv_device *device, struct radv_compute_pipeline *pipeline);

#endif /* RADV_PIPELINE_COMPUTE_H */
