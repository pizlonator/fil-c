/*
 * Copyright © 2016 Red Hat.
 * Copyright © 2016 Bas Nieuwenhuizen
 *
 * based in part on anv driver which is:
 * Copyright © 2015 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 */

#include "meta/radv_meta.h"
#include "nir/nir.h"
#include "nir/nir_builder.h"
#include "nir/nir_serialize.h"
#include "nir/radv_nir.h"
#include "spirv/nir_spirv.h"
#include "util/disk_cache.h"
#include "util/mesa-sha1.h"
#include "util/os_time.h"
#include "util/u_atomic.h"
#include "radv_cs.h"
#include "radv_debug.h"
#include "radv_pipeline_cache.h"
#include "radv_rmv.h"
#include "radv_shader.h"
#include "radv_shader_args.h"
#include "vk_nir_convert_ycbcr.h"
#include "vk_pipeline.h"
#include "vk_render_pass.h"
#include "vk_util.h"

#include "util/u_debug.h"
#include "ac_binary.h"
#include "ac_nir.h"
#include "ac_shader_util.h"
#include "aco_interface.h"
#include "sid.h"
#include "vk_format.h"

static uint32_t
radv_get_compute_resource_limits(const struct radv_physical_device *pdev, const struct radv_shader *cs)
{
   unsigned threads_per_threadgroup;
   unsigned threadgroups_per_cu = 1;
   unsigned waves_per_threadgroup;
   unsigned max_waves_per_sh = 0;

   /* Calculate best compute resource limits. */
   threads_per_threadgroup = cs->info.cs.block_size[0] * cs->info.cs.block_size[1] * cs->info.cs.block_size[2];
   waves_per_threadgroup = DIV_ROUND_UP(threads_per_threadgroup, cs->info.wave_size);

   if (pdev->info.gfx_level >= GFX10 && waves_per_threadgroup == 1)
      threadgroups_per_cu = 2;

   return ac_get_compute_resource_limits(&pdev->info, waves_per_threadgroup, max_waves_per_sh, threadgroups_per_cu);
}

void
radv_get_compute_pipeline_metadata(const struct radv_device *device, const struct radv_compute_pipeline *pipeline,
                                   struct radv_compute_pipeline_metadata *metadata)
{
   const struct radv_physical_device *pdev = radv_device_physical(device);
   const struct radv_shader *cs = pipeline->base.shaders[MESA_SHADER_COMPUTE];
   uint32_t upload_sgpr = 0, inline_sgpr = 0;

   memset(metadata, 0, sizeof(*metadata));

   metadata->shader_va = radv_shader_get_va(cs) >> 8;
   metadata->rsrc1 = cs->config.rsrc1;
   metadata->rsrc2 = cs->config.rsrc2;
   metadata->rsrc3 = cs->config.rsrc3;
   metadata->compute_resource_limits = radv_get_compute_resource_limits(pdev, cs);
   metadata->block_size_x = cs->info.cs.block_size[0];
   metadata->block_size_y = cs->info.cs.block_size[1];
   metadata->block_size_z = cs->info.cs.block_size[2];
   metadata->wave32 = cs->info.wave_size == 32;

   const struct radv_userdata_info *grid_size_loc = radv_get_user_sgpr(cs, AC_UD_CS_GRID_SIZE);
   if (grid_size_loc->sgpr_idx != -1) {
      metadata->grid_base_sgpr = (cs->info.user_data_0 + 4 * grid_size_loc->sgpr_idx - SI_SH_REG_OFFSET) >> 2;
   }

   const struct radv_userdata_info *push_constant_loc = radv_get_user_sgpr(cs, AC_UD_PUSH_CONSTANTS);
   if (push_constant_loc->sgpr_idx != -1) {
      upload_sgpr = (cs->info.user_data_0 + 4 * push_constant_loc->sgpr_idx - SI_SH_REG_OFFSET) >> 2;
   }

   const struct radv_userdata_info *inline_push_constant_loc = radv_get_user_sgpr(cs, AC_UD_INLINE_PUSH_CONSTANTS);
   if (inline_push_constant_loc->sgpr_idx != -1) {
      inline_sgpr = (cs->info.user_data_0 + 4 * inline_push_constant_loc->sgpr_idx - SI_SH_REG_OFFSET) >> 2;
   }

   metadata->push_const_sgpr = upload_sgpr | (inline_sgpr << 16);
   metadata->inline_push_const_mask = cs->info.inline_push_constant_mask;
}

void
radv_emit_compute_shader(const struct radv_physical_device *pdev, struct radeon_cmdbuf *cs,
                         const struct radv_shader *shader)
{
   uint64_t va = radv_shader_get_va(shader);

   radeon_set_sh_reg(cs, R_00B830_COMPUTE_PGM_LO, va >> 8);

   radeon_set_sh_reg_seq(cs, R_00B848_COMPUTE_PGM_RSRC1, 2);
   radeon_emit(cs, shader->config.rsrc1);
   radeon_emit(cs, shader->config.rsrc2);
   if (pdev->info.gfx_level >= GFX10) {
      radeon_set_sh_reg(cs, R_00B8A0_COMPUTE_PGM_RSRC3, shader->config.rsrc3);
   }

   radeon_set_sh_reg(cs, R_00B854_COMPUTE_RESOURCE_LIMITS, radv_get_compute_resource_limits(pdev, shader));

   radeon_set_sh_reg_seq(cs, R_00B81C_COMPUTE_NUM_THREAD_X, 3);
   radeon_emit(cs, S_00B81C_NUM_THREAD_FULL(shader->info.cs.block_size[0]));
   radeon_emit(cs, S_00B81C_NUM_THREAD_FULL(shader->info.cs.block_size[1]));
   radeon_emit(cs, S_00B81C_NUM_THREAD_FULL(shader->info.cs.block_size[2]));
}

static void
radv_compute_generate_pm4(const struct radv_device *device, struct radv_compute_pipeline *pipeline,
                          struct radv_shader *shader)
{
   const struct radv_physical_device *pdev = radv_device_physical(device);
   struct radeon_cmdbuf *cs = &pipeline->base.cs;

   cs->reserved_dw = cs->max_dw = pdev->info.gfx_level >= GFX10 ? 19 : 16;
   cs->buf = malloc(cs->max_dw * 4);

   radv_emit_compute_shader(pdev, cs, shader);

   assert(pipeline->base.cs.cdw <= pipeline->base.cs.max_dw);
}

void
radv_compute_pipeline_init(const struct radv_device *device, struct radv_compute_pipeline *pipeline,
                           const struct radv_pipeline_layout *layout, struct radv_shader *shader)
{
   pipeline->base.need_indirect_descriptor_sets |= radv_shader_need_indirect_descriptor_sets(shader);

   pipeline->base.push_constant_size = layout->push_constant_size;
   pipeline->base.dynamic_offset_count = layout->dynamic_offset_count;

   radv_compute_generate_pm4(device, pipeline, shader);
}

struct radv_shader *
radv_compile_cs(struct radv_device *device, struct vk_pipeline_cache *cache, struct radv_shader_stage *cs_stage,
                bool keep_executable_info, bool keep_statistic_info, bool is_internal,
                struct radv_shader_binary **cs_binary)
{
   struct radv_shader *cs_shader;

   /* Compile SPIR-V shader to NIR. */
   cs_stage->nir = radv_shader_spirv_to_nir(device, cs_stage, NULL, is_internal);

   radv_optimize_nir(cs_stage->nir, cs_stage->key.optimisations_disabled);

   /* Gather info again, information such as outputs_read can be out-of-date. */
   nir_shader_gather_info(cs_stage->nir, nir_shader_get_entrypoint(cs_stage->nir));

   /* Run the shader info pass. */
   radv_nir_shader_info_init(cs_stage->stage, MESA_SHADER_NONE, &cs_stage->info);
   radv_nir_shader_info_pass(device, cs_stage->nir, &cs_stage->layout, &cs_stage->key, NULL, RADV_PIPELINE_COMPUTE,
                             false, &cs_stage->info);

   radv_declare_shader_args(device, NULL, &cs_stage->info, MESA_SHADER_COMPUTE, MESA_SHADER_NONE, &cs_stage->args);

   cs_stage->info.user_sgprs_locs = cs_stage->args.user_sgprs_locs;
   cs_stage->info.inline_push_constant_mask = cs_stage->args.ac.inline_push_const_mask;

   /* Postprocess NIR. */
   radv_postprocess_nir(device, NULL, cs_stage);

   if (radv_can_dump_shader(device, cs_stage->nir, false))
      nir_print_shader(cs_stage->nir, stderr);

   /* Compile NIR shader to AMD assembly. */
   bool dump_shader = radv_can_dump_shader(device, cs_stage->nir, false);

   *cs_binary =
      radv_shader_nir_to_asm(device, cs_stage, &cs_stage->nir, 1, NULL, keep_executable_info, keep_statistic_info);

   cs_shader = radv_shader_create(device, cache, *cs_binary, keep_executable_info || dump_shader);

   radv_shader_generate_debug_info(device, dump_shader, keep_executable_info, *cs_binary, cs_shader, &cs_stage->nir, 1,
                                   &cs_stage->info);

   if (keep_executable_info && cs_stage->spirv.size) {
      cs_shader->spirv = malloc(cs_stage->spirv.size);
      memcpy(cs_shader->spirv, cs_stage->spirv.data, cs_stage->spirv.size);
      cs_shader->spirv_size = cs_stage->spirv.size;
   }

   return cs_shader;
}

static void
radv_compute_pipeline_hash(const struct radv_device *device, const VkComputePipelineCreateInfo *pCreateInfo,
                           unsigned char *hash)
{
   VkPipelineCreateFlags2KHR create_flags = vk_compute_pipeline_create_flags(pCreateInfo);
   VK_FROM_HANDLE(radv_pipeline_layout, pipeline_layout, pCreateInfo->layout);
   const VkPipelineShaderStageCreateInfo *sinfo = &pCreateInfo->stage;
   struct mesa_sha1 ctx;

   struct radv_shader_stage_key stage_key =
      radv_pipeline_get_shader_key(device, sinfo, create_flags, pCreateInfo->pNext);

   _mesa_sha1_init(&ctx);
   radv_pipeline_hash(device, pipeline_layout, &ctx);
   radv_pipeline_hash_shader_stage(sinfo, &stage_key, &ctx);
   _mesa_sha1_final(&ctx, hash);
}

static VkResult
radv_compute_pipeline_compile(const VkComputePipelineCreateInfo *pCreateInfo, struct radv_compute_pipeline *pipeline,
                              struct radv_pipeline_layout *pipeline_layout, struct radv_device *device,
                              struct vk_pipeline_cache *cache, const VkPipelineShaderStageCreateInfo *pStage,
                              const VkPipelineCreationFeedbackCreateInfo *creation_feedback)
{
   struct radv_shader_binary *cs_binary = NULL;
   bool keep_executable_info = radv_pipeline_capture_shaders(device, pipeline->base.create_flags);
   bool keep_statistic_info = radv_pipeline_capture_shader_stats(device, pipeline->base.create_flags);
   struct radv_shader_stage cs_stage = {0};
   VkPipelineCreationFeedback pipeline_feedback = {
      .flags = VK_PIPELINE_CREATION_FEEDBACK_VALID_BIT,
   };
   bool skip_shaders_cache = false;
   VkResult result = VK_SUCCESS;

   int64_t pipeline_start = os_time_get_nano();

   radv_compute_pipeline_hash(device, pCreateInfo, pipeline->base.sha1);

   pipeline->base.pipeline_hash = *(uint64_t *)pipeline->base.sha1;

   /* Skip the shaders cache when any of the below are true:
    * - shaders are captured because it's for debugging purposes
    */
   if (keep_executable_info) {
      skip_shaders_cache = true;
   }

   bool found_in_application_cache = true;
   if (!skip_shaders_cache &&
       radv_compute_pipeline_cache_search(device, cache, pipeline, &found_in_application_cache)) {
      if (found_in_application_cache)
         pipeline_feedback.flags |= VK_PIPELINE_CREATION_FEEDBACK_APPLICATION_PIPELINE_CACHE_HIT_BIT;
      result = VK_SUCCESS;
      goto done;
   }

   if (pipeline->base.create_flags & VK_PIPELINE_CREATE_2_FAIL_ON_PIPELINE_COMPILE_REQUIRED_BIT_KHR)
      return VK_PIPELINE_COMPILE_REQUIRED;

   int64_t stage_start = os_time_get_nano();

   const struct radv_shader_stage_key stage_key =
      radv_pipeline_get_shader_key(device, &pCreateInfo->stage, pipeline->base.create_flags, pCreateInfo->pNext);

   radv_pipeline_stage_init(pStage, pipeline_layout, &stage_key, &cs_stage);

   pipeline->base.shaders[MESA_SHADER_COMPUTE] = radv_compile_cs(
      device, cache, &cs_stage, keep_executable_info, keep_statistic_info, pipeline->base.is_internal, &cs_binary);

   cs_stage.feedback.duration += os_time_get_nano() - stage_start;

   if (!skip_shaders_cache) {
      radv_pipeline_cache_insert(device, cache, &pipeline->base);
   }

   free(cs_binary);
   if (radv_can_dump_shader_stats(device, cs_stage.nir)) {
      radv_dump_shader_stats(device, &pipeline->base, pipeline->base.shaders[MESA_SHADER_COMPUTE], MESA_SHADER_COMPUTE,
                             stderr);
   }
   ralloc_free(cs_stage.nir);

done:
   pipeline_feedback.duration = os_time_get_nano() - pipeline_start;

   if (creation_feedback) {
      *creation_feedback->pPipelineCreationFeedback = pipeline_feedback;

      if (creation_feedback->pipelineStageCreationFeedbackCount) {
         assert(creation_feedback->pipelineStageCreationFeedbackCount == 1);
         creation_feedback->pPipelineStageCreationFeedbacks[0] = cs_stage.feedback;
      }
   }

   return result;
}

VkResult
radv_compute_pipeline_create(VkDevice _device, VkPipelineCache _cache, const VkComputePipelineCreateInfo *pCreateInfo,
                             const VkAllocationCallbacks *pAllocator, VkPipeline *pPipeline)
{
   VK_FROM_HANDLE(radv_device, device, _device);
   VK_FROM_HANDLE(vk_pipeline_cache, cache, _cache);
   VK_FROM_HANDLE(radv_pipeline_layout, pipeline_layout, pCreateInfo->layout);
   struct radv_compute_pipeline *pipeline;
   VkResult result;

   pipeline = vk_zalloc2(&device->vk.alloc, pAllocator, sizeof(*pipeline), 8, VK_SYSTEM_ALLOCATION_SCOPE_OBJECT);
   if (pipeline == NULL) {
      return vk_error(device, VK_ERROR_OUT_OF_HOST_MEMORY);
   }

   radv_pipeline_init(device, &pipeline->base, RADV_PIPELINE_COMPUTE);
   pipeline->base.create_flags = vk_compute_pipeline_create_flags(pCreateInfo);
   pipeline->base.is_internal = _cache == device->meta_state.cache;

   const VkPipelineCreationFeedbackCreateInfo *creation_feedback =
      vk_find_struct_const(pCreateInfo->pNext, PIPELINE_CREATION_FEEDBACK_CREATE_INFO);

   result = radv_compute_pipeline_compile(pCreateInfo, pipeline, pipeline_layout, device, cache, &pCreateInfo->stage,
                                          creation_feedback);
   if (result != VK_SUCCESS) {
      radv_pipeline_destroy(device, &pipeline->base, pAllocator);
      return result;
   }

   radv_compute_pipeline_init(device, pipeline, pipeline_layout, pipeline->base.shaders[MESA_SHADER_COMPUTE]);

   if (pipeline->base.create_flags & VK_PIPELINE_CREATE_INDIRECT_BINDABLE_BIT_NV) {
      const VkComputePipelineIndirectBufferInfoNV *indirect_buffer =
         vk_find_struct_const(pCreateInfo->pNext, COMPUTE_PIPELINE_INDIRECT_BUFFER_INFO_NV);
      struct radv_shader *cs = pipeline->base.shaders[MESA_SHADER_COMPUTE];

      pipeline->indirect.va = indirect_buffer->deviceAddress;
      pipeline->indirect.size = indirect_buffer->size;

      /* vkCmdUpdatePipelineIndirectBufferNV() can be called on any queues supporting transfer
       * operations and it's not required to call it on the same queue as the DGC execute. Because
       * it's not possible to know if the compute shader uses scratch when DGC execute is called,
       * the only solution is gather the max scratch size of all indirect pipelines.
       */
      simple_mtx_lock(&device->compute_scratch_mtx);
      device->compute_scratch_size_per_wave =
         MAX2(device->compute_scratch_size_per_wave, cs->config.scratch_bytes_per_wave);
      device->compute_scratch_waves = MAX2(device->compute_scratch_waves, radv_get_max_scratch_waves(device, cs));
      simple_mtx_unlock(&device->compute_scratch_mtx);
   }

   *pPipeline = radv_pipeline_to_handle(&pipeline->base);
   radv_rmv_log_compute_pipeline_create(device, &pipeline->base, pipeline->base.is_internal);
   return VK_SUCCESS;
}

static VkResult
radv_create_compute_pipelines(VkDevice _device, VkPipelineCache pipelineCache, uint32_t count,
                              const VkComputePipelineCreateInfo *pCreateInfos, const VkAllocationCallbacks *pAllocator,
                              VkPipeline *pPipelines)
{
   VkResult result = VK_SUCCESS;

   unsigned i = 0;
   for (; i < count; i++) {
      VkResult r;
      r = radv_compute_pipeline_create(_device, pipelineCache, &pCreateInfos[i], pAllocator, &pPipelines[i]);
      if (r != VK_SUCCESS) {
         result = r;
         pPipelines[i] = VK_NULL_HANDLE;

         VkPipelineCreateFlagBits2KHR create_flags = vk_compute_pipeline_create_flags(&pCreateInfos[i]);
         if (create_flags & VK_PIPELINE_CREATE_2_EARLY_RETURN_ON_FAILURE_BIT_KHR)
            break;
      }
   }

   for (; i < count; ++i)
      pPipelines[i] = VK_NULL_HANDLE;

   return result;
}

void
radv_destroy_compute_pipeline(struct radv_device *device, struct radv_compute_pipeline *pipeline)
{
   if (pipeline->base.shaders[MESA_SHADER_COMPUTE])
      radv_shader_unref(device, pipeline->base.shaders[MESA_SHADER_COMPUTE]);
}

VKAPI_ATTR VkResult VKAPI_CALL
radv_CreateComputePipelines(VkDevice _device, VkPipelineCache pipelineCache, uint32_t count,
                            const VkComputePipelineCreateInfo *pCreateInfos, const VkAllocationCallbacks *pAllocator,
                            VkPipeline *pPipelines)
{
   return radv_create_compute_pipelines(_device, pipelineCache, count, pCreateInfos, pAllocator, pPipelines);
}
