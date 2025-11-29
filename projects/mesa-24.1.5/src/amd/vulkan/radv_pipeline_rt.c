/*
 * Copyright © 2021 Google
 *
 * SPDX-License-Identifier: MIT
 */

#include "nir/nir.h"
#include "nir/nir_builder.h"

#include "vk_shader_module.h"

#include "nir/radv_nir.h"
#include "radv_debug.h"
#include "radv_descriptor_set.h"
#include "radv_entrypoints.h"
#include "radv_pipeline_cache.h"
#include "radv_pipeline_rt.h"
#include "radv_rmv.h"
#include "radv_shader.h"

struct rt_handle_hash_entry {
   uint32_t key;
   char hash[20];
};

static uint32_t
handle_from_stages(struct radv_device *device, const unsigned char *shader_sha1, bool replay_namespace)
{
   uint32_t ret;

   memcpy(&ret, shader_sha1, sizeof(ret));

   /* Leave the low half for resume shaders etc. */
   ret |= 1u << 31;

   /* Ensure we have dedicated space for replayable shaders */
   ret &= ~(1u << 30);
   ret |= replay_namespace << 30;

   simple_mtx_lock(&device->rt_handles_mtx);

   struct hash_entry *he = NULL;
   for (;;) {
      he = _mesa_hash_table_search(device->rt_handles, &ret);
      if (!he)
         break;

      if (memcmp(he->data, shader_sha1, SHA1_DIGEST_LENGTH) == 0)
         break;

      ++ret;
   }

   if (!he) {
      struct rt_handle_hash_entry *e = ralloc(device->rt_handles, struct rt_handle_hash_entry);
      e->key = ret;
      memcpy(e->hash, shader_sha1, SHA1_DIGEST_LENGTH);
      _mesa_hash_table_insert(device->rt_handles, &e->key, &e->hash);
   }

   simple_mtx_unlock(&device->rt_handles_mtx);

   return ret;
}

static void
radv_generate_rt_shaders_key(const struct radv_device *device, const VkRayTracingPipelineCreateInfoKHR *pCreateInfo,
                             struct radv_shader_stage_key *stage_keys)
{
   VkPipelineCreateFlags2KHR create_flags = vk_rt_pipeline_create_flags(pCreateInfo);

   for (uint32_t i = 0; i < pCreateInfo->stageCount; i++) {
      const VkPipelineShaderStageCreateInfo *stage = &pCreateInfo->pStages[i];
      gl_shader_stage s = vk_to_mesa_shader_stage(stage->stage);

      stage_keys[s] = radv_pipeline_get_shader_key(device, stage, create_flags, pCreateInfo->pNext);
   }

   if (pCreateInfo->pLibraryInfo) {
      for (unsigned i = 0; i < pCreateInfo->pLibraryInfo->libraryCount; ++i) {
         VK_FROM_HANDLE(radv_pipeline, pipeline_lib, pCreateInfo->pLibraryInfo->pLibraries[i]);
         struct radv_ray_tracing_pipeline *library_pipeline = radv_pipeline_to_ray_tracing(pipeline_lib);
         /* apply shader robustness from merged shaders */
         if (library_pipeline->traversal_storage_robustness2)
            stage_keys[MESA_SHADER_INTERSECTION].storage_robustness2 = true;

         if (library_pipeline->traversal_uniform_robustness2)
            stage_keys[MESA_SHADER_INTERSECTION].uniform_robustness2 = true;
      }
   }
}

static VkResult
radv_create_group_handles(struct radv_device *device, const VkRayTracingPipelineCreateInfoKHR *pCreateInfo,
                          const struct radv_ray_tracing_stage *stages, struct radv_ray_tracing_group *groups)
{
   VkPipelineCreateFlags2KHR create_flags = vk_rt_pipeline_create_flags(pCreateInfo);
   bool capture_replay = create_flags & VK_PIPELINE_CREATE_2_RAY_TRACING_SHADER_GROUP_HANDLE_CAPTURE_REPLAY_BIT_KHR;
   for (unsigned i = 0; i < pCreateInfo->groupCount; ++i) {
      const VkRayTracingShaderGroupCreateInfoKHR *group_info = &pCreateInfo->pGroups[i];
      switch (group_info->type) {
      case VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR:
         if (group_info->generalShader != VK_SHADER_UNUSED_KHR) {
            const struct radv_ray_tracing_stage *stage = &stages[group_info->generalShader];
            groups[i].handle.general_index = handle_from_stages(device, stage->sha1, capture_replay);
         }
         break;
      case VK_RAY_TRACING_SHADER_GROUP_TYPE_PROCEDURAL_HIT_GROUP_KHR:
         if (group_info->closestHitShader != VK_SHADER_UNUSED_KHR) {
            const struct radv_ray_tracing_stage *stage = &stages[group_info->closestHitShader];
            groups[i].handle.closest_hit_index = handle_from_stages(device, stage->sha1, capture_replay);
         }

         if (group_info->intersectionShader != VK_SHADER_UNUSED_KHR) {
            unsigned char sha1[SHA1_DIGEST_LENGTH];
            struct mesa_sha1 ctx;

            _mesa_sha1_init(&ctx);
            _mesa_sha1_update(&ctx, stages[group_info->intersectionShader].sha1, SHA1_DIGEST_LENGTH);
            if (group_info->anyHitShader != VK_SHADER_UNUSED_KHR)
               _mesa_sha1_update(&ctx, stages[group_info->anyHitShader].sha1, SHA1_DIGEST_LENGTH);
            _mesa_sha1_final(&ctx, sha1);

            groups[i].handle.intersection_index = handle_from_stages(device, sha1, capture_replay);
         }
         break;
      case VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR:
         if (group_info->closestHitShader != VK_SHADER_UNUSED_KHR) {
            const struct radv_ray_tracing_stage *stage = &stages[group_info->closestHitShader];
            groups[i].handle.closest_hit_index = handle_from_stages(device, stage->sha1, capture_replay);
         }

         if (group_info->anyHitShader != VK_SHADER_UNUSED_KHR) {
            const struct radv_ray_tracing_stage *stage = &stages[group_info->anyHitShader];
            groups[i].handle.any_hit_index = handle_from_stages(device, stage->sha1, capture_replay);
         }
         break;
      case VK_SHADER_GROUP_SHADER_MAX_ENUM_KHR:
         unreachable("VK_SHADER_GROUP_SHADER_MAX_ENUM_KHR");
      }

      if (group_info->pShaderGroupCaptureReplayHandle) {
         const struct radv_rt_capture_replay_handle *handle = group_info->pShaderGroupCaptureReplayHandle;
         if (memcmp(&handle->non_recursive_idx, &groups[i].handle.any_hit_index, sizeof(uint32_t)) != 0) {
            return VK_ERROR_INVALID_OPAQUE_CAPTURE_ADDRESS;
         }
      }
   }

   return VK_SUCCESS;
}

static VkResult
radv_rt_init_capture_replay(struct radv_device *device, const VkRayTracingPipelineCreateInfoKHR *pCreateInfo,
                            const struct radv_ray_tracing_stage *stages, const struct radv_ray_tracing_group *groups,
                            struct radv_serialized_shader_arena_block *capture_replay_blocks)
{
   VkResult result = VK_SUCCESS;
   uint32_t idx;

   for (idx = 0; idx < pCreateInfo->groupCount; idx++) {
      if (!pCreateInfo->pGroups[idx].pShaderGroupCaptureReplayHandle)
         continue;

      const struct radv_rt_capture_replay_handle *handle =
         (const struct radv_rt_capture_replay_handle *)pCreateInfo->pGroups[idx].pShaderGroupCaptureReplayHandle;

      if (groups[idx].recursive_shader < pCreateInfo->stageCount) {
         capture_replay_blocks[groups[idx].recursive_shader] = handle->recursive_shader_alloc;
      } else if (groups[idx].recursive_shader != VK_SHADER_UNUSED_KHR) {
         struct radv_shader *library_shader = stages[groups[idx].recursive_shader].shader;
         simple_mtx_lock(&library_shader->replay_mtx);
         /* If arena_va is 0, the pipeline is monolithic and the shader was inlined into raygen */
         if (!library_shader->has_replay_alloc && handle->recursive_shader_alloc.arena_va) {
            union radv_shader_arena_block *new_block =
               radv_replay_shader_arena_block(device, &handle->recursive_shader_alloc, library_shader);
            if (!new_block) {
               result = VK_ERROR_INVALID_OPAQUE_CAPTURE_ADDRESS;
               goto reloc_out;
            }

            radv_shader_wait_for_upload(device, library_shader->upload_seq);
            radv_free_shader_memory(device, library_shader->alloc);

            library_shader->alloc = new_block;
            library_shader->has_replay_alloc = true;

            library_shader->bo = library_shader->alloc->arena->bo;
            library_shader->va = radv_buffer_get_va(library_shader->bo) + library_shader->alloc->offset;

            if (!radv_shader_reupload(device, library_shader)) {
               result = VK_ERROR_UNKNOWN;
               goto reloc_out;
            }
         }

         reloc_out:
            simple_mtx_unlock(&library_shader->replay_mtx);
            if (result != VK_SUCCESS)
               return result;
         }
   }

   return result;
}

static VkResult
radv_rt_fill_group_info(struct radv_device *device, const VkRayTracingPipelineCreateInfoKHR *pCreateInfo,
                        const struct radv_ray_tracing_stage *stages, struct radv_ray_tracing_group *groups)
{
   VkResult result = radv_create_group_handles(device, pCreateInfo, stages, groups);

   uint32_t idx;
   for (idx = 0; idx < pCreateInfo->groupCount; idx++) {
      groups[idx].type = pCreateInfo->pGroups[idx].type;
      if (groups[idx].type == VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR)
         groups[idx].recursive_shader = pCreateInfo->pGroups[idx].generalShader;
      else
         groups[idx].recursive_shader = pCreateInfo->pGroups[idx].closestHitShader;
      groups[idx].any_hit_shader = pCreateInfo->pGroups[idx].anyHitShader;
      groups[idx].intersection_shader = pCreateInfo->pGroups[idx].intersectionShader;
   }

   /* copy and adjust library groups (incl. handles) */
   if (pCreateInfo->pLibraryInfo) {
      unsigned stage_count = pCreateInfo->stageCount;
      for (unsigned i = 0; i < pCreateInfo->pLibraryInfo->libraryCount; ++i) {
         VK_FROM_HANDLE(radv_pipeline, pipeline_lib, pCreateInfo->pLibraryInfo->pLibraries[i]);
         struct radv_ray_tracing_pipeline *library_pipeline = radv_pipeline_to_ray_tracing(pipeline_lib);

         for (unsigned j = 0; j < library_pipeline->group_count; ++j) {
            struct radv_ray_tracing_group *dst = &groups[idx + j];
            *dst = library_pipeline->groups[j];
            if (dst->recursive_shader != VK_SHADER_UNUSED_KHR)
               dst->recursive_shader += stage_count;
            if (dst->any_hit_shader != VK_SHADER_UNUSED_KHR)
               dst->any_hit_shader += stage_count;
            if (dst->intersection_shader != VK_SHADER_UNUSED_KHR)
               dst->intersection_shader += stage_count;
            /* Don't set the shader VA since the handles are part of the pipeline hash */
            dst->handle.recursive_shader_ptr = 0;
         }
         idx += library_pipeline->group_count;
         stage_count += library_pipeline->stage_count;
      }
   }

   return result;
}

static void
radv_rt_fill_stage_info(const VkRayTracingPipelineCreateInfoKHR *pCreateInfo, struct radv_ray_tracing_stage *stages)
{
   uint32_t idx;
   for (idx = 0; idx < pCreateInfo->stageCount; idx++)
      stages[idx].stage = vk_to_mesa_shader_stage(pCreateInfo->pStages[idx].stage);

   if (pCreateInfo->pLibraryInfo) {
      for (unsigned i = 0; i < pCreateInfo->pLibraryInfo->libraryCount; ++i) {
         VK_FROM_HANDLE(radv_pipeline, pipeline, pCreateInfo->pLibraryInfo->pLibraries[i]);
         struct radv_ray_tracing_pipeline *library_pipeline = radv_pipeline_to_ray_tracing(pipeline);
         for (unsigned j = 0; j < library_pipeline->stage_count; ++j) {
            if (library_pipeline->stages[j].nir)
               stages[idx].nir = vk_pipeline_cache_object_ref(library_pipeline->stages[j].nir);
            if (library_pipeline->stages[j].shader)
               stages[idx].shader = radv_shader_ref(library_pipeline->stages[j].shader);

            stages[idx].stage = library_pipeline->stages[j].stage;
            stages[idx].stack_size = library_pipeline->stages[j].stack_size;
            stages[idx].info = library_pipeline->stages[j].info;
            memcpy(stages[idx].sha1, library_pipeline->stages[j].sha1, SHA1_DIGEST_LENGTH);
            idx++;
         }
      }
   }
}

static void
radv_init_rt_stage_hashes(const struct radv_device *device, const VkRayTracingPipelineCreateInfoKHR *pCreateInfo,
                          struct radv_ray_tracing_stage *stages, const struct radv_shader_stage_key *stage_keys)
{
   for (uint32_t idx = 0; idx < pCreateInfo->stageCount; idx++) {
      const VkPipelineShaderStageCreateInfo *sinfo = &pCreateInfo->pStages[idx];
      gl_shader_stage s = vk_to_mesa_shader_stage(sinfo->stage);
      struct mesa_sha1 ctx;

      _mesa_sha1_init(&ctx);
      radv_pipeline_hash_shader_stage(sinfo, &stage_keys[s], &ctx);
      _mesa_sha1_final(&ctx, stages[idx].sha1);
   }
}

static VkRayTracingPipelineCreateInfoKHR
radv_create_merged_rt_create_info(const VkRayTracingPipelineCreateInfoKHR *pCreateInfo)
{
   VkRayTracingPipelineCreateInfoKHR local_create_info = *pCreateInfo;
   uint32_t total_stages = pCreateInfo->stageCount;
   uint32_t total_groups = pCreateInfo->groupCount;

   if (pCreateInfo->pLibraryInfo) {
      for (unsigned i = 0; i < pCreateInfo->pLibraryInfo->libraryCount; ++i) {
         VK_FROM_HANDLE(radv_pipeline, pipeline, pCreateInfo->pLibraryInfo->pLibraries[i]);
         struct radv_ray_tracing_pipeline *library_pipeline = radv_pipeline_to_ray_tracing(pipeline);

         total_stages += library_pipeline->stage_count;
         total_groups += library_pipeline->group_count;
      }
   }
   local_create_info.stageCount = total_stages;
   local_create_info.groupCount = total_groups;

   return local_create_info;
}

static bool
should_move_rt_instruction(nir_intrinsic_instr *instr)
{
   switch (instr->intrinsic) {
   case nir_intrinsic_load_hit_attrib_amd:
      return nir_intrinsic_base(instr) < RADV_MAX_HIT_ATTRIB_DWORDS;
   case nir_intrinsic_load_rt_arg_scratch_offset_amd:
   case nir_intrinsic_load_ray_flags:
   case nir_intrinsic_load_ray_object_origin:
   case nir_intrinsic_load_ray_world_origin:
   case nir_intrinsic_load_ray_t_min:
   case nir_intrinsic_load_ray_object_direction:
   case nir_intrinsic_load_ray_world_direction:
   case nir_intrinsic_load_ray_t_max:
      return true;
   default:
      return false;
   }
}

static void
move_rt_instructions(nir_shader *shader)
{
   nir_cursor target = nir_before_impl(nir_shader_get_entrypoint(shader));

   nir_foreach_block (block, nir_shader_get_entrypoint(shader)) {
      nir_foreach_instr_safe (instr, block) {
         if (instr->type != nir_instr_type_intrinsic)
            continue;

         nir_intrinsic_instr *intrinsic = nir_instr_as_intrinsic(instr);

         if (!should_move_rt_instruction(intrinsic))
            continue;

         nir_instr_move(target, instr);
      }
   }

   nir_metadata_preserve(nir_shader_get_entrypoint(shader), nir_metadata_all & (~nir_metadata_instr_index));
}

static VkResult
radv_rt_nir_to_asm(struct radv_device *device, struct vk_pipeline_cache *cache,
                   const VkRayTracingPipelineCreateInfoKHR *pCreateInfo, struct radv_ray_tracing_pipeline *pipeline,
                   bool monolithic, struct radv_shader_stage *stage, uint32_t *stack_size,
                   struct radv_ray_tracing_stage_info *stage_info,
                   const struct radv_ray_tracing_stage_info *traversal_stage_info,
                   struct radv_serialized_shader_arena_block *replay_block, struct radv_shader **out_shader)
{
   struct radv_physical_device *pdev = radv_device_physical(device);
   struct radv_shader_binary *binary;
   bool keep_executable_info = radv_pipeline_capture_shaders(device, pipeline->base.base.create_flags);
   bool keep_statistic_info = radv_pipeline_capture_shader_stats(device, pipeline->base.base.create_flags);

   radv_nir_lower_rt_io(stage->nir, monolithic, 0);

   /* Gather shader info. */
   nir_shader_gather_info(stage->nir, nir_shader_get_entrypoint(stage->nir));
   radv_nir_shader_info_init(stage->stage, MESA_SHADER_NONE, &stage->info);
   radv_nir_shader_info_pass(device, stage->nir, &stage->layout, &stage->key, NULL, RADV_PIPELINE_RAY_TRACING, false,
                             &stage->info);

   /* Declare shader arguments. */
   radv_declare_shader_args(device, NULL, &stage->info, stage->stage, MESA_SHADER_NONE, &stage->args);

   stage->info.user_sgprs_locs = stage->args.user_sgprs_locs;
   stage->info.inline_push_constant_mask = stage->args.ac.inline_push_const_mask;

   /* Move ray tracing system values to the top that are set by rt_trace_ray
    * to prevent them from being overwritten by other rt_trace_ray calls.
    */
   NIR_PASS_V(stage->nir, move_rt_instructions);

   uint32_t num_resume_shaders = 0;
   nir_shader **resume_shaders = NULL;

   if (stage->stage != MESA_SHADER_INTERSECTION && !monolithic) {
      nir_builder b = nir_builder_at(nir_after_impl(nir_shader_get_entrypoint(stage->nir)));
      nir_rt_return_amd(&b);

      const nir_lower_shader_calls_options opts = {
         .address_format = nir_address_format_32bit_offset,
         .stack_alignment = 16,
         .localized_loads = true,
         .vectorizer_callback = radv_mem_vectorize_callback,
         .vectorizer_data = &pdev->info.gfx_level,
      };
      nir_lower_shader_calls(stage->nir, &opts, &resume_shaders, &num_resume_shaders, stage->nir);
   }

   unsigned num_shaders = num_resume_shaders + 1;
   nir_shader **shaders = ralloc_array(stage->nir, nir_shader *, num_shaders);
   if (!shaders)
      return VK_ERROR_OUT_OF_HOST_MEMORY;

   shaders[0] = stage->nir;
   for (uint32_t i = 0; i < num_resume_shaders; i++)
      shaders[i + 1] = resume_shaders[i];

   if (stage_info)
      memset(stage_info->unused_args, 0xFF, sizeof(stage_info->unused_args));

   /* Postprocess shader parts. */
   for (uint32_t i = 0; i < num_shaders; i++) {
      struct radv_shader_stage temp_stage = *stage;
      temp_stage.nir = shaders[i];
      radv_nir_lower_rt_abi(temp_stage.nir, pCreateInfo, &temp_stage.args, &stage->info, stack_size, i > 0, device,
                            pipeline, monolithic, traversal_stage_info);

      /* Info might be out-of-date after inlining in radv_nir_lower_rt_abi(). */
      nir_shader_gather_info(temp_stage.nir, nir_shader_get_entrypoint(temp_stage.nir));

      radv_optimize_nir(temp_stage.nir, stage->key.optimisations_disabled);
      radv_postprocess_nir(device, NULL, &temp_stage);

      if (stage_info)
         radv_gather_unused_args(stage_info, shaders[i]);

      if (radv_can_dump_shader(device, temp_stage.nir, false))
         nir_print_shader(temp_stage.nir, stderr);
   }

   bool dump_shader = radv_can_dump_shader(device, shaders[0], false);
   bool replayable =
      pipeline->base.base.create_flags & VK_PIPELINE_CREATE_2_RAY_TRACING_SHADER_GROUP_HANDLE_CAPTURE_REPLAY_BIT_KHR;

   /* Compile NIR shader to AMD assembly. */
   binary =
      radv_shader_nir_to_asm(device, stage, shaders, num_shaders, NULL, keep_executable_info, keep_statistic_info);
   struct radv_shader *shader;
   if (replay_block || replayable) {
      VkResult result = radv_shader_create_uncached(device, binary, replayable, replay_block, &shader);
      if (result != VK_SUCCESS) {
         free(binary);
         return result;
      }
   } else
      shader = radv_shader_create(device, cache, binary, keep_executable_info || dump_shader);

   if (shader) {
      radv_shader_generate_debug_info(device, dump_shader, keep_executable_info, binary, shader, shaders, num_shaders,
                                      &stage->info);

      if (shader && keep_executable_info && stage->spirv.size) {
         shader->spirv = malloc(stage->spirv.size);
         memcpy(shader->spirv, stage->spirv.data, stage->spirv.size);
         shader->spirv_size = stage->spirv.size;
      }
   }

   free(binary);

   *out_shader = shader;

   if (radv_can_dump_shader_stats(device, stage->nir))
      radv_dump_shader_stats(device, &pipeline->base.base, shader, stage->nir->info.stage, stderr);

   return shader ? VK_SUCCESS : VK_ERROR_OUT_OF_HOST_MEMORY;
}

static void
radv_update_const_info(enum radv_rt_const_arg_state *state, bool equal)
{
   if (*state == RADV_RT_CONST_ARG_STATE_UNINITIALIZED)
      *state = RADV_RT_CONST_ARG_STATE_VALID;
   else if (*state == RADV_RT_CONST_ARG_STATE_VALID && !equal)
      *state = RADV_RT_CONST_ARG_STATE_INVALID;
}

static void
radv_gather_trace_ray_src(struct radv_rt_const_arg_info *info, nir_src src)
{
   if (nir_src_is_const(src)) {
      radv_update_const_info(&info->state, info->value == nir_src_as_uint(src));
      info->value = nir_src_as_uint(src);
   } else {
      info->state = RADV_RT_CONST_ARG_STATE_INVALID;
   }
}

static void
radv_rt_const_arg_info_combine(struct radv_rt_const_arg_info *dst, const struct radv_rt_const_arg_info *src)
{
   if (src->state != RADV_RT_CONST_ARG_STATE_UNINITIALIZED) {
      radv_update_const_info(&dst->state, dst->value == src->value);
      if (src->state == RADV_RT_CONST_ARG_STATE_INVALID)
         dst->state = RADV_RT_CONST_ARG_STATE_INVALID;
      dst->value = src->value;
   }
}

static struct radv_ray_tracing_stage_info
radv_gather_ray_tracing_stage_info(nir_shader *nir)
{
   struct radv_ray_tracing_stage_info info = {
      .can_inline = true,
      .set_flags = 0xFFFFFFFF,
      .unset_flags = 0xFFFFFFFF,
   };

   nir_function_impl *impl = nir_shader_get_entrypoint(nir);
   nir_foreach_block (block, impl) {
      nir_foreach_instr (instr, block) {
         if (instr->type != nir_instr_type_intrinsic)
            continue;

         nir_intrinsic_instr *intr = nir_instr_as_intrinsic(instr);
         if (intr->intrinsic != nir_intrinsic_trace_ray)
            continue;

         info.can_inline = false;

         radv_gather_trace_ray_src(&info.tmin, intr->src[7]);
         radv_gather_trace_ray_src(&info.tmax, intr->src[9]);
         radv_gather_trace_ray_src(&info.sbt_offset, intr->src[3]);
         radv_gather_trace_ray_src(&info.sbt_stride, intr->src[4]);
         radv_gather_trace_ray_src(&info.miss_index, intr->src[5]);

         nir_src flags = intr->src[1];
         if (nir_src_is_const(flags)) {
            info.set_flags &= nir_src_as_uint(flags);
            info.unset_flags &= ~nir_src_as_uint(flags);
         } else {
            info.set_flags = 0;
            info.unset_flags = 0;
         }
      }
   }

   if (nir->info.stage == MESA_SHADER_RAYGEN || nir->info.stage == MESA_SHADER_ANY_HIT ||
       nir->info.stage == MESA_SHADER_INTERSECTION)
      info.can_inline = true;
   else if (nir->info.stage == MESA_SHADER_CALLABLE)
      info.can_inline = false;

   return info;
}

static inline bool
radv_ray_tracing_stage_is_always_inlined(struct radv_ray_tracing_stage *stage)
{
   return stage->stage == MESA_SHADER_ANY_HIT || stage->stage == MESA_SHADER_INTERSECTION;
}

static VkResult
radv_rt_compile_shaders(struct radv_device *device, struct vk_pipeline_cache *cache,
                        const VkRayTracingPipelineCreateInfoKHR *pCreateInfo,
                        const VkPipelineCreationFeedbackCreateInfo *creation_feedback,
                        const struct radv_shader_stage_key *stage_keys, struct radv_ray_tracing_pipeline *pipeline,
                        struct radv_serialized_shader_arena_block *capture_replay_handles)
{
   VK_FROM_HANDLE(radv_pipeline_layout, pipeline_layout, pCreateInfo->layout);

   if (pipeline->base.base.create_flags & VK_PIPELINE_CREATE_2_FAIL_ON_PIPELINE_COMPILE_REQUIRED_BIT_KHR)
      return VK_PIPELINE_COMPILE_REQUIRED;
   VkResult result = VK_SUCCESS;

   struct radv_ray_tracing_stage *rt_stages = pipeline->stages;

   struct radv_shader_stage *stages = calloc(pCreateInfo->stageCount, sizeof(struct radv_shader_stage));
   if (!stages)
      return VK_ERROR_OUT_OF_HOST_MEMORY;

   bool library = pipeline->base.base.create_flags & VK_PIPELINE_CREATE_2_LIBRARY_BIT_KHR;

   bool monolithic = !library;
   for (uint32_t i = 0; i < pCreateInfo->stageCount; i++) {
      if (rt_stages[i].shader || rt_stages[i].nir)
         continue;

      int64_t stage_start = os_time_get_nano();

      struct radv_shader_stage *stage = &stages[i];
      gl_shader_stage s = vk_to_mesa_shader_stage(pCreateInfo->pStages[i].stage);
      radv_pipeline_stage_init(&pCreateInfo->pStages[i], pipeline_layout, &stage_keys[s], stage);

      /* precompile the shader */
      stage->nir = radv_shader_spirv_to_nir(device, stage, NULL, false);

      NIR_PASS(_, stage->nir, radv_nir_lower_hit_attrib_derefs);

      rt_stages[i].info = radv_gather_ray_tracing_stage_info(stage->nir);

      stage->feedback.duration = os_time_get_nano() - stage_start;
   }

   bool has_callable = false;
   /* TODO: Recompile recursive raygen shaders instead. */
   bool raygen_imported = false;
   for (uint32_t i = 0; i < pipeline->stage_count; i++) {
      has_callable |= rt_stages[i].stage == MESA_SHADER_CALLABLE;
      monolithic &= rt_stages[i].info.can_inline;

      if (i >= pCreateInfo->stageCount)
         raygen_imported |= rt_stages[i].stage == MESA_SHADER_RAYGEN;
   }

   for (uint32_t idx = 0; idx < pCreateInfo->stageCount; idx++) {
      if (rt_stages[idx].shader || rt_stages[idx].nir)
         continue;

      int64_t stage_start = os_time_get_nano();

      struct radv_shader_stage *stage = &stages[idx];

      /* Cases in which we need to keep around the NIR:
       *    - pipeline library: The final pipeline might be monolithic in which case it will need every NIR shader.
       *                        If there is a callable shader, we can be sure that the final pipeline won't be
       *                        monolithic.
       *    - non-recursive:    Non-recursive shaders are inlined into the traversal shader.
       *    - monolithic:       Callable shaders (chit/miss) are inlined into the raygen shader.
       */
      bool always_inlined = radv_ray_tracing_stage_is_always_inlined(&rt_stages[idx]);
      bool nir_needed =
         (library && !has_callable) || always_inlined || (monolithic && rt_stages[idx].stage != MESA_SHADER_RAYGEN);
      nir_needed &= !rt_stages[idx].nir;
      if (nir_needed) {
         rt_stages[idx].stack_size = stage->nir->scratch_size;
         rt_stages[idx].nir = radv_pipeline_cache_nir_to_handle(device, cache, stage->nir, rt_stages[idx].sha1,
                                                                !stage->key.optimisations_disabled);
      }

      stage->feedback.duration += os_time_get_nano() - stage_start;
   }

   for (uint32_t idx = 0; idx < pCreateInfo->stageCount; idx++) {
      int64_t stage_start = os_time_get_nano();
      struct radv_shader_stage *stage = &stages[idx];

      /* Cases in which we need to compile the shader (raygen/callable/chit/miss):
       *    TODO: - monolithic: Extend the loop to cover imported stages and force compilation of imported raygen
       *                        shaders since pipeline library shaders use separate compilation.
       *    - separate:   Compile any recursive stage if wasn't compiled yet.
       */
      bool shader_needed = !radv_ray_tracing_stage_is_always_inlined(&rt_stages[idx]) && !rt_stages[idx].shader;
      if (rt_stages[idx].stage == MESA_SHADER_CLOSEST_HIT || rt_stages[idx].stage == MESA_SHADER_MISS)
         shader_needed &= !monolithic || raygen_imported;

      if (shader_needed) {
         uint32_t stack_size = 0;
         struct radv_serialized_shader_arena_block *replay_block =
            capture_replay_handles[idx].arena_va ? &capture_replay_handles[idx] : NULL;

         bool monolithic_raygen = monolithic && stage->stage == MESA_SHADER_RAYGEN;

         result = radv_rt_nir_to_asm(device, cache, pCreateInfo, pipeline, monolithic_raygen, stage, &stack_size,
                                     &rt_stages[idx].info, NULL, replay_block, &rt_stages[idx].shader);
         if (result != VK_SUCCESS)
            goto cleanup;

         assert(rt_stages[idx].stack_size <= stack_size);
         rt_stages[idx].stack_size = stack_size;
      }

      if (creation_feedback && creation_feedback->pipelineStageCreationFeedbackCount) {
         assert(idx < creation_feedback->pipelineStageCreationFeedbackCount);
         stage->feedback.duration += os_time_get_nano() - stage_start;
         creation_feedback->pPipelineStageCreationFeedbacks[idx] = stage->feedback;
      }
   }

   /* Monolithic raygen shaders do not need a traversal shader. Skip compiling one if there are only monolithic raygen
    * shaders.
    */
   bool traversal_needed = !library && (!monolithic || raygen_imported);
   if (!traversal_needed) {
      result = VK_SUCCESS;
      goto cleanup;
   }

   struct radv_ray_tracing_stage_info traversal_info = {
      .set_flags = 0xFFFFFFFF,
      .unset_flags = 0xFFFFFFFF,
   };

   memset(traversal_info.unused_args, 0xFF, sizeof(traversal_info.unused_args));

   for (uint32_t i = 0; i < pipeline->stage_count; i++) {
      if (!pipeline->stages[i].shader)
         continue;

      struct radv_ray_tracing_stage_info *info = &pipeline->stages[i].info;

      BITSET_AND(traversal_info.unused_args, traversal_info.unused_args, info->unused_args);

      radv_rt_const_arg_info_combine(&traversal_info.tmin, &info->tmin);
      radv_rt_const_arg_info_combine(&traversal_info.tmax, &info->tmax);
      radv_rt_const_arg_info_combine(&traversal_info.sbt_offset, &info->sbt_offset);
      radv_rt_const_arg_info_combine(&traversal_info.sbt_stride, &info->sbt_stride);
      radv_rt_const_arg_info_combine(&traversal_info.miss_index, &info->miss_index);

      traversal_info.set_flags &= info->set_flags;
      traversal_info.unset_flags &= info->unset_flags;
   }

   /* create traversal shader */
   nir_shader *traversal_nir = radv_build_traversal_shader(device, pipeline, pCreateInfo, &traversal_info);
   struct radv_shader_stage traversal_stage = {
      .stage = MESA_SHADER_INTERSECTION,
      .nir = traversal_nir,
      .key = stage_keys[MESA_SHADER_INTERSECTION],
   };
   radv_shader_layout_init(pipeline_layout, MESA_SHADER_INTERSECTION, &traversal_stage.layout);
   result = radv_rt_nir_to_asm(device, cache, pCreateInfo, pipeline, false, &traversal_stage, NULL, NULL,
                               &traversal_info, NULL, &pipeline->base.base.shaders[MESA_SHADER_INTERSECTION]);
   ralloc_free(traversal_nir);

cleanup:
   for (uint32_t i = 0; i < pCreateInfo->stageCount; i++)
      ralloc_free(stages[i].nir);
   free(stages);
   return result;
}

static bool
radv_rt_pipeline_has_dynamic_stack_size(const VkRayTracingPipelineCreateInfoKHR *pCreateInfo)
{
   if (!pCreateInfo->pDynamicState)
      return false;

   for (unsigned i = 0; i < pCreateInfo->pDynamicState->dynamicStateCount; ++i) {
      if (pCreateInfo->pDynamicState->pDynamicStates[i] == VK_DYNAMIC_STATE_RAY_TRACING_PIPELINE_STACK_SIZE_KHR)
         return true;
   }

   return false;
}

static void
compute_rt_stack_size(const VkRayTracingPipelineCreateInfoKHR *pCreateInfo, struct radv_ray_tracing_pipeline *pipeline)
{
   if (radv_rt_pipeline_has_dynamic_stack_size(pCreateInfo)) {
      pipeline->stack_size = -1u;
      return;
   }

   unsigned raygen_size = 0;
   unsigned callable_size = 0;
   unsigned chit_miss_size = 0;
   unsigned intersection_size = 0;
   unsigned any_hit_size = 0;

   for (unsigned i = 0; i < pipeline->stage_count; ++i) {
      uint32_t size = pipeline->stages[i].stack_size;
      switch (pipeline->stages[i].stage) {
      case MESA_SHADER_RAYGEN:
         raygen_size = MAX2(raygen_size, size);
         break;
      case MESA_SHADER_CLOSEST_HIT:
      case MESA_SHADER_MISS:
         chit_miss_size = MAX2(chit_miss_size, size);
         break;
      case MESA_SHADER_CALLABLE:
         callable_size = MAX2(callable_size, size);
         break;
      case MESA_SHADER_INTERSECTION:
         intersection_size = MAX2(intersection_size, size);
         break;
      case MESA_SHADER_ANY_HIT:
         any_hit_size = MAX2(any_hit_size, size);
         break;
      default:
         unreachable("Invalid stage type in RT shader");
      }
   }
   pipeline->stack_size =
      raygen_size +
      MIN2(pCreateInfo->maxPipelineRayRecursionDepth, 1) * MAX2(chit_miss_size, intersection_size + any_hit_size) +
      MAX2(0, (int)(pCreateInfo->maxPipelineRayRecursionDepth) - 1) * chit_miss_size + 2 * callable_size;
}

static void
combine_config(struct ac_shader_config *config, struct ac_shader_config *other)
{
   config->num_sgprs = MAX2(config->num_sgprs, other->num_sgprs);
   config->num_vgprs = MAX2(config->num_vgprs, other->num_vgprs);
   config->num_shared_vgprs = MAX2(config->num_shared_vgprs, other->num_shared_vgprs);
   config->spilled_sgprs = MAX2(config->spilled_sgprs, other->spilled_sgprs);
   config->spilled_vgprs = MAX2(config->spilled_vgprs, other->spilled_vgprs);
   config->lds_size = MAX2(config->lds_size, other->lds_size);
   config->scratch_bytes_per_wave = MAX2(config->scratch_bytes_per_wave, other->scratch_bytes_per_wave);

   assert(config->float_mode == other->float_mode);
}

static void
postprocess_rt_config(struct ac_shader_config *config, enum amd_gfx_level gfx_level, unsigned wave_size)
{
   config->rsrc1 =
      (config->rsrc1 & C_00B848_VGPRS) | S_00B848_VGPRS((config->num_vgprs - 1) / (wave_size == 32 ? 8 : 4));
   if (gfx_level < GFX10)
      config->rsrc1 = (config->rsrc1 & C_00B848_SGPRS) | S_00B848_SGPRS((config->num_sgprs - 1) / 8);

   config->rsrc2 = (config->rsrc2 & C_00B84C_LDS_SIZE) | S_00B84C_LDS_SIZE(config->lds_size);
   config->rsrc3 = (config->rsrc3 & C_00B8A0_SHARED_VGPR_CNT) | S_00B8A0_SHARED_VGPR_CNT(config->num_shared_vgprs / 8);
}

static void
compile_rt_prolog(struct radv_device *device, struct radv_ray_tracing_pipeline *pipeline)
{
   const struct radv_physical_device *pdev = radv_device_physical(device);

   pipeline->prolog = radv_create_rt_prolog(device);

   /* create combined config */
   struct ac_shader_config *config = &pipeline->prolog->config;
   for (unsigned i = 0; i < pipeline->stage_count; i++)
      if (pipeline->stages[i].shader)
         combine_config(config, &pipeline->stages[i].shader->config);

   if (pipeline->base.base.shaders[MESA_SHADER_INTERSECTION])
      combine_config(config, &pipeline->base.base.shaders[MESA_SHADER_INTERSECTION]->config);

   postprocess_rt_config(config, pdev->info.gfx_level, pdev->rt_wave_size);

   pipeline->prolog->max_waves = radv_get_max_waves(device, config, &pipeline->prolog->info);
}

static VkResult
radv_rt_pipeline_compile(struct radv_device *device, const VkRayTracingPipelineCreateInfoKHR *pCreateInfo,
                         struct radv_ray_tracing_pipeline *pipeline, struct vk_pipeline_cache *cache,
                         struct radv_shader_stage_key *stage_keys,
                         struct radv_serialized_shader_arena_block *capture_replay_blocks,
                         const VkPipelineCreationFeedbackCreateInfo *creation_feedback)
{
   const bool keep_executable_info = radv_pipeline_capture_shaders(device, pipeline->base.base.create_flags);
   const bool emit_ray_history = !!device->rra_trace.ray_history_buffer;
   VkPipelineCreationFeedback pipeline_feedback = {
      .flags = VK_PIPELINE_CREATION_FEEDBACK_VALID_BIT,
   };
   bool skip_shaders_cache = false;
   VkResult result = VK_SUCCESS;

   int64_t pipeline_start = os_time_get_nano();

   radv_hash_rt_shaders(device, pipeline->base.base.sha1, pipeline->stages, pCreateInfo, pipeline->groups);
   pipeline->base.base.pipeline_hash = *(uint64_t *)pipeline->base.base.sha1;

   /* Skip the shaders cache when any of the below are true:
    * - shaders are captured because it's for debugging purposes
    * - ray history is enabled
    */
   if (keep_executable_info || emit_ray_history) {
      skip_shaders_cache = true;
   }

   bool found_in_application_cache = true;
   if (!skip_shaders_cache &&
       radv_ray_tracing_pipeline_cache_search(device, cache, pipeline, &found_in_application_cache)) {
      if (found_in_application_cache)
         pipeline_feedback.flags |= VK_PIPELINE_CREATION_FEEDBACK_APPLICATION_PIPELINE_CACHE_HIT_BIT;
      result = VK_SUCCESS;
      goto done;
   }

   result = radv_rt_compile_shaders(device, cache, pCreateInfo, creation_feedback, stage_keys, pipeline,
                                    capture_replay_blocks);

   if (result != VK_SUCCESS)
      return result;

   if (!skip_shaders_cache)
      radv_ray_tracing_pipeline_cache_insert(device, cache, pipeline, pCreateInfo->stageCount);

done:
   pipeline_feedback.duration = os_time_get_nano() - pipeline_start;

   if (creation_feedback)
      *creation_feedback->pPipelineCreationFeedback = pipeline_feedback;

   return result;
}

static VkResult
radv_rt_pipeline_create(VkDevice _device, VkPipelineCache _cache, const VkRayTracingPipelineCreateInfoKHR *pCreateInfo,
                        const VkAllocationCallbacks *pAllocator, VkPipeline *pPipeline)
{
   VK_FROM_HANDLE(radv_device, device, _device);
   VK_FROM_HANDLE(vk_pipeline_cache, cache, _cache);
   VK_FROM_HANDLE(radv_pipeline_layout, pipeline_layout, pCreateInfo->layout);
   VkResult result;
   const VkPipelineCreationFeedbackCreateInfo *creation_feedback =
      vk_find_struct_const(pCreateInfo->pNext, PIPELINE_CREATION_FEEDBACK_CREATE_INFO);

   VkRayTracingPipelineCreateInfoKHR local_create_info = radv_create_merged_rt_create_info(pCreateInfo);

   VK_MULTIALLOC(ma);
   VK_MULTIALLOC_DECL(&ma, struct radv_ray_tracing_pipeline, pipeline, 1);
   VK_MULTIALLOC_DECL(&ma, struct radv_ray_tracing_stage, stages, local_create_info.stageCount);
   VK_MULTIALLOC_DECL(&ma, struct radv_ray_tracing_group, groups, local_create_info.groupCount);
   VK_MULTIALLOC_DECL(&ma, struct radv_serialized_shader_arena_block, capture_replay_blocks, pCreateInfo->stageCount);
   if (!vk_multialloc_zalloc2(&ma, &device->vk.alloc, pAllocator, VK_SYSTEM_ALLOCATION_SCOPE_OBJECT))
      return VK_ERROR_OUT_OF_HOST_MEMORY;

   radv_pipeline_init(device, &pipeline->base.base, RADV_PIPELINE_RAY_TRACING);
   pipeline->base.base.create_flags = vk_rt_pipeline_create_flags(pCreateInfo);
   pipeline->stage_count = local_create_info.stageCount;
   pipeline->non_imported_stage_count = pCreateInfo->stageCount;
   pipeline->group_count = local_create_info.groupCount;
   pipeline->stages = stages;
   pipeline->groups = groups;

   radv_rt_fill_stage_info(pCreateInfo, stages);

   struct radv_shader_stage_key stage_keys[MESA_VULKAN_SHADER_STAGES] = {0};

   radv_generate_rt_shaders_key(device, pCreateInfo, stage_keys);

   /* cache robustness state for making merged shaders */
   if (stage_keys[MESA_SHADER_INTERSECTION].storage_robustness2)
      pipeline->traversal_storage_robustness2 = true;

   if (stage_keys[MESA_SHADER_INTERSECTION].uniform_robustness2)
      pipeline->traversal_uniform_robustness2 = true;

   radv_init_rt_stage_hashes(device, pCreateInfo, stages, stage_keys);
   result = radv_rt_fill_group_info(device, pCreateInfo, stages, pipeline->groups);
   if (result != VK_SUCCESS)
      goto fail;

   result = radv_rt_init_capture_replay(device, pCreateInfo, stages, pipeline->groups, capture_replay_blocks);
   if (result != VK_SUCCESS)
      goto fail;

   result = radv_rt_pipeline_compile(device, pCreateInfo, pipeline, cache, stage_keys, capture_replay_blocks,
                                     creation_feedback);
   if (result != VK_SUCCESS)
      goto fail;

   if (!(pipeline->base.base.create_flags & VK_PIPELINE_CREATE_2_LIBRARY_BIT_KHR)) {
      compute_rt_stack_size(pCreateInfo, pipeline);
      compile_rt_prolog(device, pipeline);

      radv_compute_pipeline_init(device, &pipeline->base, pipeline_layout, pipeline->prolog);
   }

   /* write shader VAs into group handles */
   for (unsigned i = 0; i < pipeline->group_count; i++) {
      if (pipeline->groups[i].recursive_shader != VK_SHADER_UNUSED_KHR) {
         struct radv_shader *shader = pipeline->stages[pipeline->groups[i].recursive_shader].shader;
         if (shader)
            pipeline->groups[i].handle.recursive_shader_ptr = shader->va | radv_get_rt_priority(shader->info.stage);
      }
   }

   *pPipeline = radv_pipeline_to_handle(&pipeline->base.base);
   radv_rmv_log_rt_pipeline_create(device, pipeline);

   return result;

fail:
   radv_pipeline_destroy(device, &pipeline->base.base, pAllocator);
   return result;
}

void
radv_destroy_ray_tracing_pipeline(struct radv_device *device, struct radv_ray_tracing_pipeline *pipeline)
{
   for (unsigned i = 0; i < pipeline->stage_count; i++) {
      if (pipeline->stages[i].nir)
         vk_pipeline_cache_object_unref(&device->vk, pipeline->stages[i].nir);
      if (pipeline->stages[i].shader)
         radv_shader_unref(device, pipeline->stages[i].shader);
   }

   if (pipeline->prolog)
      radv_shader_unref(device, pipeline->prolog);
   if (pipeline->base.base.shaders[MESA_SHADER_INTERSECTION])
      radv_shader_unref(device, pipeline->base.base.shaders[MESA_SHADER_INTERSECTION]);
}

VKAPI_ATTR VkResult VKAPI_CALL
radv_CreateRayTracingPipelinesKHR(VkDevice _device, VkDeferredOperationKHR deferredOperation,
                                  VkPipelineCache pipelineCache, uint32_t count,
                                  const VkRayTracingPipelineCreateInfoKHR *pCreateInfos,
                                  const VkAllocationCallbacks *pAllocator, VkPipeline *pPipelines)
{
   VkResult result = VK_SUCCESS;

   unsigned i = 0;
   for (; i < count; i++) {
      VkResult r;
      r = radv_rt_pipeline_create(_device, pipelineCache, &pCreateInfos[i], pAllocator, &pPipelines[i]);
      if (r != VK_SUCCESS) {
         result = r;
         pPipelines[i] = VK_NULL_HANDLE;

         const VkPipelineCreateFlagBits2KHR create_flags = vk_rt_pipeline_create_flags(&pCreateInfos[i]);
         if (create_flags & VK_PIPELINE_CREATE_2_EARLY_RETURN_ON_FAILURE_BIT_KHR)
            break;
      }
   }

   for (; i < count; ++i)
      pPipelines[i] = VK_NULL_HANDLE;

   if (result != VK_SUCCESS)
      return result;

   /* Work around Portal RTX not handling VK_OPERATION_NOT_DEFERRED_KHR correctly. */
   if (deferredOperation != VK_NULL_HANDLE)
      return VK_OPERATION_DEFERRED_KHR;

   return result;
}

VKAPI_ATTR VkResult VKAPI_CALL
radv_GetRayTracingShaderGroupHandlesKHR(VkDevice device, VkPipeline _pipeline, uint32_t firstGroup, uint32_t groupCount,
                                        size_t dataSize, void *pData)
{
   VK_FROM_HANDLE(radv_pipeline, pipeline, _pipeline);
   struct radv_ray_tracing_group *groups = radv_pipeline_to_ray_tracing(pipeline)->groups;
   char *data = pData;

   STATIC_ASSERT(sizeof(struct radv_pipeline_group_handle) <= RADV_RT_HANDLE_SIZE);

   memset(data, 0, groupCount * RADV_RT_HANDLE_SIZE);

   for (uint32_t i = 0; i < groupCount; ++i) {
      memcpy(data + i * RADV_RT_HANDLE_SIZE, &groups[firstGroup + i].handle, sizeof(struct radv_pipeline_group_handle));
   }

   return VK_SUCCESS;
}

VKAPI_ATTR VkDeviceSize VKAPI_CALL
radv_GetRayTracingShaderGroupStackSizeKHR(VkDevice device, VkPipeline _pipeline, uint32_t group,
                                          VkShaderGroupShaderKHR groupShader)
{
   VK_FROM_HANDLE(radv_pipeline, pipeline, _pipeline);
   struct radv_ray_tracing_pipeline *rt_pipeline = radv_pipeline_to_ray_tracing(pipeline);
   struct radv_ray_tracing_group *rt_group = &rt_pipeline->groups[group];
   switch (groupShader) {
   case VK_SHADER_GROUP_SHADER_GENERAL_KHR:
   case VK_SHADER_GROUP_SHADER_CLOSEST_HIT_KHR:
      return rt_pipeline->stages[rt_group->recursive_shader].stack_size;
   case VK_SHADER_GROUP_SHADER_ANY_HIT_KHR:
      return rt_pipeline->stages[rt_group->any_hit_shader].stack_size;
   case VK_SHADER_GROUP_SHADER_INTERSECTION_KHR:
      return rt_pipeline->stages[rt_group->intersection_shader].stack_size;
   default:
      return 0;
   }
}

VKAPI_ATTR VkResult VKAPI_CALL
radv_GetRayTracingCaptureReplayShaderGroupHandlesKHR(VkDevice device, VkPipeline _pipeline, uint32_t firstGroup,
                                                     uint32_t groupCount, size_t dataSize, void *pData)
{
   VK_FROM_HANDLE(radv_pipeline, pipeline, _pipeline);
   struct radv_ray_tracing_pipeline *rt_pipeline = radv_pipeline_to_ray_tracing(pipeline);
   struct radv_rt_capture_replay_handle *data = pData;

   memset(data, 0, groupCount * sizeof(struct radv_rt_capture_replay_handle));

   for (uint32_t i = 0; i < groupCount; ++i) {
      uint32_t recursive_shader = rt_pipeline->groups[firstGroup + i].recursive_shader;
      if (recursive_shader != VK_SHADER_UNUSED_KHR) {
         struct radv_shader *shader = rt_pipeline->stages[recursive_shader].shader;
         if (shader) {
            data[i].recursive_shader_alloc.offset = shader->alloc->offset;
            data[i].recursive_shader_alloc.size = shader->alloc->size;
            data[i].recursive_shader_alloc.arena_va = shader->alloc->arena->bo->va;
            data[i].recursive_shader_alloc.arena_size = shader->alloc->arena->size;
         }
      }
      data[i].non_recursive_idx = rt_pipeline->groups[firstGroup + i].handle.any_hit_index;
   }

   return VK_SUCCESS;
}
