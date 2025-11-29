/*
 * Copyright © 2022 Collabora, LTD
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#include "vk_pipeline.h"

#include "vk_alloc.h"
#include "vk_common_entrypoints.h"
#include "vk_command_buffer.h"
#include "vk_descriptor_set_layout.h"
#include "vk_device.h"
#include "vk_graphics_state.h"
#include "vk_log.h"
#include "vk_nir.h"
#include "vk_physical_device.h"
#include "vk_pipeline_layout.h"
#include "vk_shader.h"
#include "vk_shader_module.h"
#include "vk_util.h"

#include "nir_serialize.h"

#include "util/mesa-sha1.h"

bool
vk_pipeline_shader_stage_is_null(const VkPipelineShaderStageCreateInfo *info)
{
   if (info->module != VK_NULL_HANDLE)
      return false;

   vk_foreach_struct_const(ext, info->pNext) {
      if (ext->sType == VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO ||
          ext->sType == VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_MODULE_IDENTIFIER_CREATE_INFO_EXT)
         return false;
   }

   return true;
}

bool
vk_pipeline_shader_stage_has_identifier(const VkPipelineShaderStageCreateInfo *info)
{
   const VkPipelineShaderStageModuleIdentifierCreateInfoEXT *id_info =
      vk_find_struct_const(info->pNext, PIPELINE_SHADER_STAGE_MODULE_IDENTIFIER_CREATE_INFO_EXT);

   return id_info && id_info->identifierSize != 0;
}

static nir_shader *
get_builtin_nir(const VkPipelineShaderStageCreateInfo *info)
{
   VK_FROM_HANDLE(vk_shader_module, module, info->module);

   nir_shader *nir = NULL;
   if (module != NULL) {
      nir = module->nir;
   } else {
      const VkPipelineShaderStageNirCreateInfoMESA *nir_info =
         vk_find_struct_const(info->pNext, PIPELINE_SHADER_STAGE_NIR_CREATE_INFO_MESA);
      if (nir_info != NULL)
         nir = nir_info->nir;
   }

   if (nir == NULL)
      return NULL;

   assert(nir->info.stage == vk_to_mesa_shader_stage(info->stage));
   ASSERTED nir_function_impl *entrypoint = nir_shader_get_entrypoint(nir);
   assert(strcmp(entrypoint->function->name, info->pName) == 0);
   assert(info->pSpecializationInfo == NULL);

   return nir;
}

static uint32_t
get_required_subgroup_size(const void *info_pNext)
{
   const VkPipelineShaderStageRequiredSubgroupSizeCreateInfo *rss_info =
      vk_find_struct_const(info_pNext,
                           PIPELINE_SHADER_STAGE_REQUIRED_SUBGROUP_SIZE_CREATE_INFO);
   return rss_info != NULL ? rss_info->requiredSubgroupSize : 0;
}

enum gl_subgroup_size
vk_get_subgroup_size(uint32_t spirv_version,
                     gl_shader_stage stage,
                     const void *info_pNext,
                     bool allow_varying,
                     bool require_full)
{
   uint32_t req_subgroup_size = get_required_subgroup_size(info_pNext);
   if (req_subgroup_size > 0) {
      assert(util_is_power_of_two_nonzero(req_subgroup_size));
      assert(req_subgroup_size >= 8 && req_subgroup_size <= 128);
      return req_subgroup_size;
   } else if (allow_varying || spirv_version >= 0x10600) {
      /* Starting with SPIR-V 1.6, varying subgroup size the default */
      return SUBGROUP_SIZE_VARYING;
   } else if (require_full) {
      assert(stage == MESA_SHADER_COMPUTE);
      return SUBGROUP_SIZE_FULL_SUBGROUPS;
   } else {
      return SUBGROUP_SIZE_API_CONSTANT;
   }
}

VkResult
vk_pipeline_shader_stage_to_nir(struct vk_device *device,
                                const VkPipelineShaderStageCreateInfo *info,
                                const struct spirv_to_nir_options *spirv_options,
                                const struct nir_shader_compiler_options *nir_options,
                                void *mem_ctx, nir_shader **nir_out)
{
   VK_FROM_HANDLE(vk_shader_module, module, info->module);
   const gl_shader_stage stage = vk_to_mesa_shader_stage(info->stage);

   assert(info->sType == VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO);

   nir_shader *builtin_nir = get_builtin_nir(info);
   if (builtin_nir != NULL) {
      nir_validate_shader(builtin_nir, "internal shader");

      nir_shader *clone = nir_shader_clone(mem_ctx, builtin_nir);
      if (clone == NULL)
         return vk_error(device, VK_ERROR_OUT_OF_HOST_MEMORY);

      assert(clone->options == NULL || clone->options == nir_options);
      clone->options = nir_options;

      *nir_out = clone;
      return VK_SUCCESS;
   }

   const uint32_t *spirv_data;
   uint32_t spirv_size;
   if (module != NULL) {
      spirv_data = (uint32_t *)module->data;
      spirv_size = module->size;
   } else {
      const VkShaderModuleCreateInfo *minfo =
         vk_find_struct_const(info->pNext, SHADER_MODULE_CREATE_INFO);
      if (unlikely(minfo == NULL)) {
         return vk_errorf(device, VK_ERROR_UNKNOWN,
                          "No shader module provided");
      }
      spirv_data = minfo->pCode;
      spirv_size = minfo->codeSize;
   }

   enum gl_subgroup_size subgroup_size = vk_get_subgroup_size(
      vk_spirv_version(spirv_data, spirv_size),
      stage, info->pNext,
      info->flags & VK_PIPELINE_SHADER_STAGE_CREATE_ALLOW_VARYING_SUBGROUP_SIZE_BIT,
      info->flags & VK_PIPELINE_SHADER_STAGE_CREATE_REQUIRE_FULL_SUBGROUPS_BIT);

   nir_shader *nir = vk_spirv_to_nir(device, spirv_data, spirv_size, stage,
                                     info->pName, subgroup_size,
                                     info->pSpecializationInfo,
                                     spirv_options, nir_options,
                                     false /* internal */,
                                     mem_ctx);
   if (nir == NULL)
      return vk_errorf(device, VK_ERROR_UNKNOWN, "spirv_to_nir failed");

   *nir_out = nir;

   return VK_SUCCESS;
}

void
vk_pipeline_hash_shader_stage(const VkPipelineShaderStageCreateInfo *info,
                              const struct vk_pipeline_robustness_state *rstate,
                              unsigned char *stage_sha1)
{
   VK_FROM_HANDLE(vk_shader_module, module, info->module);

   const nir_shader *builtin_nir = get_builtin_nir(info);
   if (builtin_nir != NULL) {
      /* Internal NIR module: serialize and hash the NIR shader.
       * We don't need to hash other info fields since they should match the
       * NIR data.
       */
      struct blob blob;

      blob_init(&blob);
      nir_serialize(&blob, builtin_nir, false);
      assert(!blob.out_of_memory);
      _mesa_sha1_compute(blob.data, blob.size, stage_sha1);
      blob_finish(&blob);
      return;
   }

   const VkShaderModuleCreateInfo *minfo =
      vk_find_struct_const(info->pNext, SHADER_MODULE_CREATE_INFO);
   const VkPipelineShaderStageModuleIdentifierCreateInfoEXT *iinfo =
      vk_find_struct_const(info->pNext, PIPELINE_SHADER_STAGE_MODULE_IDENTIFIER_CREATE_INFO_EXT);

   struct mesa_sha1 ctx;

   _mesa_sha1_init(&ctx);

   _mesa_sha1_update(&ctx, &info->flags, sizeof(info->flags));

   assert(util_bitcount(info->stage) == 1);
   _mesa_sha1_update(&ctx, &info->stage, sizeof(info->stage));

   if (module) {
      _mesa_sha1_update(&ctx, module->hash, sizeof(module->hash));
   } else if (minfo) {
      blake3_hash spirv_hash;

      _mesa_blake3_compute(minfo->pCode, minfo->codeSize, spirv_hash);
      _mesa_sha1_update(&ctx, spirv_hash, sizeof(spirv_hash));
   } else {
      /* It is legal to pass in arbitrary identifiers as long as they don't exceed
       * the limit. Shaders with bogus identifiers are more or less guaranteed to fail. */
      assert(iinfo);
      assert(iinfo->identifierSize <= VK_MAX_SHADER_MODULE_IDENTIFIER_SIZE_EXT);
      _mesa_sha1_update(&ctx, iinfo->pIdentifier, iinfo->identifierSize);
   }

   if (rstate) {
      _mesa_sha1_update(&ctx, &rstate->storage_buffers, sizeof(rstate->storage_buffers));
      _mesa_sha1_update(&ctx, &rstate->uniform_buffers, sizeof(rstate->uniform_buffers));
      _mesa_sha1_update(&ctx, &rstate->vertex_inputs, sizeof(rstate->vertex_inputs));
      _mesa_sha1_update(&ctx, &rstate->images, sizeof(rstate->images));
   }

   _mesa_sha1_update(&ctx, info->pName, strlen(info->pName));

   if (info->pSpecializationInfo) {
      _mesa_sha1_update(&ctx, info->pSpecializationInfo->pMapEntries,
                        info->pSpecializationInfo->mapEntryCount *
                        sizeof(*info->pSpecializationInfo->pMapEntries));
      _mesa_sha1_update(&ctx, info->pSpecializationInfo->pData,
                        info->pSpecializationInfo->dataSize);
   }

   uint32_t req_subgroup_size = get_required_subgroup_size(info);
   _mesa_sha1_update(&ctx, &req_subgroup_size, sizeof(req_subgroup_size));

   _mesa_sha1_final(&ctx, stage_sha1);
}

static VkPipelineRobustnessBufferBehaviorEXT
vk_device_default_robust_buffer_behavior(const struct vk_device *device)
{
   if (device->enabled_features.robustBufferAccess2) {
      return VK_PIPELINE_ROBUSTNESS_BUFFER_BEHAVIOR_ROBUST_BUFFER_ACCESS_2_EXT;
   } else if (device->enabled_features.robustBufferAccess) {
      return VK_PIPELINE_ROBUSTNESS_BUFFER_BEHAVIOR_ROBUST_BUFFER_ACCESS_EXT;
   } else {
      return VK_PIPELINE_ROBUSTNESS_BUFFER_BEHAVIOR_DISABLED_EXT;
   }
}

static VkPipelineRobustnessImageBehaviorEXT
vk_device_default_robust_image_behavior(const struct vk_device *device)
{
   if (device->enabled_features.robustImageAccess2) {
      return VK_PIPELINE_ROBUSTNESS_IMAGE_BEHAVIOR_ROBUST_IMAGE_ACCESS_2_EXT;
   } else if (device->enabled_features.robustImageAccess) {
      return VK_PIPELINE_ROBUSTNESS_IMAGE_BEHAVIOR_ROBUST_IMAGE_ACCESS_EXT;
   } else {
      return VK_PIPELINE_ROBUSTNESS_IMAGE_BEHAVIOR_DISABLED_EXT;
   }
}

void
vk_pipeline_robustness_state_fill(const struct vk_device *device,
                                  struct vk_pipeline_robustness_state *rs,
                                  const void *pipeline_pNext,
                                  const void *shader_stage_pNext)
{
   rs->uniform_buffers = VK_PIPELINE_ROBUSTNESS_BUFFER_BEHAVIOR_DEVICE_DEFAULT_EXT;
   rs->storage_buffers = VK_PIPELINE_ROBUSTNESS_BUFFER_BEHAVIOR_DEVICE_DEFAULT_EXT;
   rs->vertex_inputs = VK_PIPELINE_ROBUSTNESS_BUFFER_BEHAVIOR_DEVICE_DEFAULT_EXT;
   rs->images = VK_PIPELINE_ROBUSTNESS_IMAGE_BEHAVIOR_DEVICE_DEFAULT_EXT;

   const VkPipelineRobustnessCreateInfoEXT *shader_info =
      vk_find_struct_const(shader_stage_pNext,
                           PIPELINE_ROBUSTNESS_CREATE_INFO_EXT);
   if (shader_info) {
      rs->storage_buffers = shader_info->storageBuffers;
      rs->uniform_buffers = shader_info->uniformBuffers;
      rs->vertex_inputs = shader_info->vertexInputs;
      rs->images = shader_info->images;
   } else {
      const VkPipelineRobustnessCreateInfoEXT *pipeline_info =
         vk_find_struct_const(pipeline_pNext,
                              PIPELINE_ROBUSTNESS_CREATE_INFO_EXT);
      if (pipeline_info) {
         rs->storage_buffers = pipeline_info->storageBuffers;
         rs->uniform_buffers = pipeline_info->uniformBuffers;
         rs->vertex_inputs = pipeline_info->vertexInputs;
         rs->images = pipeline_info->images;
      }
   }

   if (rs->storage_buffers ==
       VK_PIPELINE_ROBUSTNESS_BUFFER_BEHAVIOR_DEVICE_DEFAULT_EXT)
      rs->storage_buffers = vk_device_default_robust_buffer_behavior(device);

   if (rs->uniform_buffers ==
       VK_PIPELINE_ROBUSTNESS_BUFFER_BEHAVIOR_DEVICE_DEFAULT_EXT)
      rs->uniform_buffers = vk_device_default_robust_buffer_behavior(device);

   if (rs->vertex_inputs ==
       VK_PIPELINE_ROBUSTNESS_BUFFER_BEHAVIOR_DEVICE_DEFAULT_EXT)
      rs->vertex_inputs = vk_device_default_robust_buffer_behavior(device);

   if (rs->images == VK_PIPELINE_ROBUSTNESS_IMAGE_BEHAVIOR_DEVICE_DEFAULT_EXT)
      rs->images = vk_device_default_robust_image_behavior(device);
}

void *
vk_pipeline_zalloc(struct vk_device *device,
                   const struct vk_pipeline_ops *ops,
                   VkPipelineBindPoint bind_point,
                   VkPipelineCreateFlags2KHR flags,
                   const VkAllocationCallbacks *alloc,
                   size_t size)
{
   struct vk_pipeline *pipeline;

   pipeline = vk_object_zalloc(device, alloc, size, VK_OBJECT_TYPE_PIPELINE);
   if (pipeline == NULL)
      return NULL;

   pipeline->ops = ops;
   pipeline->bind_point = bind_point;
   pipeline->flags = flags;

   return pipeline;
}

void
vk_pipeline_free(struct vk_device *device,
                 const VkAllocationCallbacks *alloc,
                 struct vk_pipeline *pipeline)
{
   vk_object_free(device, alloc, &pipeline->base);
}

VKAPI_ATTR void VKAPI_CALL
vk_common_DestroyPipeline(VkDevice _device,
                          VkPipeline _pipeline,
                          const VkAllocationCallbacks *pAllocator)
{
   VK_FROM_HANDLE(vk_device, device, _device);
   VK_FROM_HANDLE(vk_pipeline, pipeline, _pipeline);

   if (pipeline == NULL)
      return;

   pipeline->ops->destroy(device, pipeline, pAllocator);
}

VKAPI_ATTR VkResult VKAPI_CALL
vk_common_GetPipelineExecutablePropertiesKHR(
   VkDevice _device,
   const VkPipelineInfoKHR *pPipelineInfo,
   uint32_t *pExecutableCount,
   VkPipelineExecutablePropertiesKHR *pProperties)
{
   VK_FROM_HANDLE(vk_device, device, _device);
   VK_FROM_HANDLE(vk_pipeline, pipeline, pPipelineInfo->pipeline);

   return pipeline->ops->get_executable_properties(device, pipeline,
                                                   pExecutableCount,
                                                   pProperties);
}

VKAPI_ATTR VkResult VKAPI_CALL
vk_common_GetPipelineExecutableStatisticsKHR(
    VkDevice _device,
    const VkPipelineExecutableInfoKHR *pExecutableInfo,
    uint32_t *pStatisticCount,
    VkPipelineExecutableStatisticKHR *pStatistics)
{
   VK_FROM_HANDLE(vk_device, device, _device);
   VK_FROM_HANDLE(vk_pipeline, pipeline, pExecutableInfo->pipeline);

   return pipeline->ops->get_executable_statistics(device, pipeline,
                                                   pExecutableInfo->executableIndex,
                                                   pStatisticCount, pStatistics);
}

VKAPI_ATTR VkResult VKAPI_CALL
vk_common_GetPipelineExecutableInternalRepresentationsKHR(
    VkDevice _device,
    const VkPipelineExecutableInfoKHR *pExecutableInfo,
    uint32_t *pInternalRepresentationCount,
    VkPipelineExecutableInternalRepresentationKHR* pInternalRepresentations)
{
   VK_FROM_HANDLE(vk_device, device, _device);
   VK_FROM_HANDLE(vk_pipeline, pipeline, pExecutableInfo->pipeline);

   return pipeline->ops->get_internal_representations(device, pipeline,
                                                      pExecutableInfo->executableIndex,
                                                      pInternalRepresentationCount,
                                                      pInternalRepresentations);
}

VKAPI_ATTR void VKAPI_CALL
vk_common_CmdBindPipeline(VkCommandBuffer commandBuffer,
                          VkPipelineBindPoint pipelineBindPoint,
                          VkPipeline _pipeline)
{
   VK_FROM_HANDLE(vk_command_buffer, cmd_buffer, commandBuffer);
   VK_FROM_HANDLE(vk_pipeline, pipeline, _pipeline);

   assert(pipeline->bind_point == pipelineBindPoint);

   pipeline->ops->cmd_bind(cmd_buffer, pipeline);
}

static const struct vk_pipeline_cache_object_ops pipeline_shader_cache_ops;

static struct vk_shader *
vk_shader_from_cache_obj(struct vk_pipeline_cache_object *object)
{
   assert(object->ops == &pipeline_shader_cache_ops);
   return container_of(object, struct vk_shader, pipeline.cache_obj);
}

static bool
vk_pipeline_shader_serialize(struct vk_pipeline_cache_object *object,
                             struct blob *blob)
{
   struct vk_shader *shader = vk_shader_from_cache_obj(object);
   struct vk_device *device = shader->base.device;

   return shader->ops->serialize(device, shader, blob);
}

static void
vk_shader_init_cache_obj(struct vk_device *device, struct vk_shader *shader,
                         const void *key_data, size_t key_size)
{
   assert(key_size == sizeof(shader->pipeline.cache_key));
   memcpy(&shader->pipeline.cache_key, key_data,
          sizeof(shader->pipeline.cache_key));

   vk_pipeline_cache_object_init(device, &shader->pipeline.cache_obj,
                                 &pipeline_shader_cache_ops,
                                 &shader->pipeline.cache_key,
                                 sizeof(shader->pipeline.cache_key));
}

static struct vk_pipeline_cache_object *
vk_pipeline_shader_deserialize(struct vk_pipeline_cache *cache,
                               const void *key_data, size_t key_size,
                               struct blob_reader *blob)
{
   struct vk_device *device = cache->base.device;
   const struct vk_device_shader_ops *ops = device->shader_ops;

   /* TODO: Do we really want to always use the latest version? */
   const uint32_t version = device->physical->properties.shaderBinaryVersion;

   struct vk_shader *shader;
   VkResult result = ops->deserialize(device, blob, version,
                                      &device->alloc, &shader);
   if (result != VK_SUCCESS) {
      assert(result == VK_ERROR_OUT_OF_HOST_MEMORY);
      return NULL;
   }

   vk_shader_init_cache_obj(device, shader, key_data, key_size);

   return &shader->pipeline.cache_obj;
}

static void
vk_pipeline_shader_destroy(struct vk_device *device,
                           struct vk_pipeline_cache_object *object)
{
   struct vk_shader *shader = vk_shader_from_cache_obj(object);
   assert(shader->base.device == device);

   vk_shader_destroy(device, shader, &device->alloc);
}

static const struct vk_pipeline_cache_object_ops pipeline_shader_cache_ops = {
   .serialize = vk_pipeline_shader_serialize,
   .deserialize = vk_pipeline_shader_deserialize,
   .destroy = vk_pipeline_shader_destroy,
};

static struct vk_shader *
vk_shader_ref(struct vk_shader *shader)
{
   vk_pipeline_cache_object_ref(&shader->pipeline.cache_obj);
   return shader;
}

static void
vk_shader_unref(struct vk_device *device, struct vk_shader *shader)
{
   vk_pipeline_cache_object_unref(device, &shader->pipeline.cache_obj);
}

PRAGMA_DIAGNOSTIC_PUSH
PRAGMA_DIAGNOSTIC_ERROR(-Wpadded)
struct vk_pipeline_tess_info {
   unsigned tcs_vertices_out : 8;
   unsigned primitive_mode : 2; /* tess_primitive_mode */
   unsigned spacing : 2; /* gl_tess_spacing */
   unsigned ccw : 1;
   unsigned point_mode : 1;
   unsigned _pad : 18;
};
PRAGMA_DIAGNOSTIC_POP
static_assert(sizeof(struct vk_pipeline_tess_info) == 4,
              "This struct has no holes");

static void
vk_pipeline_gather_nir_tess_info(const nir_shader *nir,
                                 struct vk_pipeline_tess_info *info)
{
   info->tcs_vertices_out  = nir->info.tess.tcs_vertices_out;
   info->primitive_mode    = nir->info.tess._primitive_mode;
   info->spacing           = nir->info.tess.spacing;
   info->ccw               = nir->info.tess.ccw;
   info->point_mode        = nir->info.tess.point_mode;
}

static void
vk_pipeline_replace_nir_tess_info(nir_shader *nir,
                                  const struct vk_pipeline_tess_info *info)
{
   nir->info.tess.tcs_vertices_out  = info->tcs_vertices_out;
   nir->info.tess._primitive_mode   = info->primitive_mode;
   nir->info.tess.spacing           = info->spacing;
   nir->info.tess.ccw               = info->ccw;
   nir->info.tess.point_mode        = info->point_mode;
}

static void
vk_pipeline_tess_info_merge(struct vk_pipeline_tess_info *dst,
                            const struct vk_pipeline_tess_info *src)
{
   /* The Vulkan 1.0.38 spec, section 21.1 Tessellator says:
    *
    *    "PointMode. Controls generation of points rather than triangles
    *     or lines. This functionality defaults to disabled, and is
    *     enabled if either shader stage includes the execution mode.
    *
    * and about Triangles, Quads, IsoLines, VertexOrderCw, VertexOrderCcw,
    * PointMode, SpacingEqual, SpacingFractionalEven, SpacingFractionalOdd,
    * and OutputVertices, it says:
    *
    *    "One mode must be set in at least one of the tessellation
    *     shader stages."
    *
    * So, the fields can be set in either the TCS or TES, but they must
    * agree if set in both.
    */
   assert(dst->tcs_vertices_out == 0 ||
          src->tcs_vertices_out == 0 ||
          dst->tcs_vertices_out == src->tcs_vertices_out);
   dst->tcs_vertices_out |= src->tcs_vertices_out;

   static_assert(TESS_SPACING_UNSPECIFIED == 0, "");
   assert(dst->spacing == TESS_SPACING_UNSPECIFIED ||
          src->spacing == TESS_SPACING_UNSPECIFIED ||
          dst->spacing == src->spacing);
   dst->spacing |= src->spacing;

   static_assert(TESS_PRIMITIVE_UNSPECIFIED == 0, "");
   assert(dst->primitive_mode == TESS_PRIMITIVE_UNSPECIFIED ||
          src->primitive_mode == TESS_PRIMITIVE_UNSPECIFIED ||
          dst->primitive_mode == src->primitive_mode);
   dst->primitive_mode |= src->primitive_mode;
   dst->ccw |= src->ccw;
   dst->point_mode |= src->point_mode;
}

struct vk_pipeline_precomp_shader {
   struct vk_pipeline_cache_object cache_obj;

   /* Key for this cache_obj in the pipeline cache.
    *
    * This is always the output of vk_pipeline_hash_shader_stage() so it must
    * be a SHA1 hash.
    */
   uint8_t cache_key[SHA1_DIGEST_LENGTH];

   gl_shader_stage stage;

   struct vk_pipeline_robustness_state rs;

   /* Tessellation info if the shader is a tessellation shader */
   struct vk_pipeline_tess_info tess;

   /* Hash of the vk_pipeline_precomp_shader
    *
    * This is the hash of the final compiled NIR together with tess info and
    * robustness state.  It's used as a key for final binary lookups.  By
    * having this as a separate key, we can de-duplicate cases where you have
    * different SPIR-V or specialization constants but end up compiling the
    * same NIR shader in the end anyway.
    */
   blake3_hash blake3;

   struct blob nir_blob;
};

static struct vk_pipeline_precomp_shader *
vk_pipeline_precomp_shader_ref(struct vk_pipeline_precomp_shader *shader)
{
   vk_pipeline_cache_object_ref(&shader->cache_obj);
   return shader;
}

static void
vk_pipeline_precomp_shader_unref(struct vk_device *device,
                                 struct vk_pipeline_precomp_shader *shader)
{
   vk_pipeline_cache_object_unref(device, &shader->cache_obj);
}

static const struct vk_pipeline_cache_object_ops pipeline_precomp_shader_cache_ops;

static struct vk_pipeline_precomp_shader *
vk_pipeline_precomp_shader_from_cache_obj(struct vk_pipeline_cache_object *obj)
{
   assert(obj->ops == & pipeline_precomp_shader_cache_ops);
   return container_of(obj, struct vk_pipeline_precomp_shader, cache_obj);
}

static struct vk_pipeline_precomp_shader *
vk_pipeline_precomp_shader_create(struct vk_device *device,
                                  const void *key_data, size_t key_size,
                                  const struct vk_pipeline_robustness_state *rs,
                                  nir_shader *nir)
{
   struct blob blob;
   blob_init(&blob);

   nir_serialize(&blob, nir, false);

   if (blob.out_of_memory)
      goto fail_blob;

   struct vk_pipeline_precomp_shader *shader =
      vk_zalloc(&device->alloc, sizeof(*shader), 8,
                VK_SYSTEM_ALLOCATION_SCOPE_DEVICE);
   if (shader == NULL)
      goto fail_blob;

   assert(sizeof(shader->cache_key) == key_size);
   memcpy(shader->cache_key, key_data, sizeof(shader->cache_key));

   vk_pipeline_cache_object_init(device, &shader->cache_obj,
                                 &pipeline_precomp_shader_cache_ops,
                                 shader->cache_key,
                                 sizeof(shader->cache_key));

   shader->stage = nir->info.stage;
   shader->rs = *rs;

   vk_pipeline_gather_nir_tess_info(nir, &shader->tess);

   struct mesa_blake3 blake3_ctx;
   _mesa_blake3_init(&blake3_ctx);
   _mesa_blake3_update(&blake3_ctx, rs, sizeof(*rs));
   _mesa_blake3_update(&blake3_ctx, blob.data, blob.size);
   _mesa_blake3_final(&blake3_ctx, shader->blake3);

   shader->nir_blob = blob;

   return shader;

fail_blob:
   blob_finish(&blob);

   return NULL;
}

static bool
vk_pipeline_precomp_shader_serialize(struct vk_pipeline_cache_object *obj,
                                     struct blob *blob)
{
   struct vk_pipeline_precomp_shader *shader =
      vk_pipeline_precomp_shader_from_cache_obj(obj);

   blob_write_uint32(blob, shader->stage);
   blob_write_bytes(blob, &shader->rs, sizeof(shader->rs));
   blob_write_bytes(blob, &shader->tess, sizeof(shader->tess));
   blob_write_bytes(blob, shader->blake3, sizeof(shader->blake3));
   blob_write_uint64(blob, shader->nir_blob.size);
   blob_write_bytes(blob, shader->nir_blob.data, shader->nir_blob.size);

   return !blob->out_of_memory;
}

static struct vk_pipeline_cache_object *
vk_pipeline_precomp_shader_deserialize(struct vk_pipeline_cache *cache,
                                       const void *key_data, size_t key_size,
                                       struct blob_reader *blob)
{
   struct vk_device *device = cache->base.device;

   struct vk_pipeline_precomp_shader *shader =
      vk_zalloc(&device->alloc, sizeof(*shader), 8,
                VK_SYSTEM_ALLOCATION_SCOPE_DEVICE);
   if (shader == NULL)
      return NULL;

   assert(sizeof(shader->cache_key) == key_size);
   memcpy(shader->cache_key, key_data, sizeof(shader->cache_key));

   vk_pipeline_cache_object_init(device, &shader->cache_obj,
                                 &pipeline_precomp_shader_cache_ops,
                                 shader->cache_key,
                                 sizeof(shader->cache_key));

   shader->stage = blob_read_uint32(blob);
   blob_copy_bytes(blob, &shader->rs, sizeof(shader->rs));
   blob_copy_bytes(blob, &shader->tess, sizeof(shader->tess));
   blob_copy_bytes(blob, shader->blake3, sizeof(shader->blake3));

   uint64_t nir_size = blob_read_uint64(blob);
   if (blob->overrun || nir_size > SIZE_MAX)
      goto fail_shader;

   const void *nir_data = blob_read_bytes(blob, nir_size);
   if (blob->overrun)
      goto fail_shader;

   blob_init(&shader->nir_blob);
   blob_write_bytes(&shader->nir_blob, nir_data, nir_size);
   if (shader->nir_blob.out_of_memory)
      goto fail_nir_blob;

   return &shader->cache_obj;

fail_nir_blob:
   blob_finish(&shader->nir_blob);
fail_shader:
   vk_pipeline_cache_object_finish(&shader->cache_obj);
   vk_free(&device->alloc, shader);

   return NULL;
}

static void
vk_pipeline_precomp_shader_destroy(struct vk_device *device,
                                   struct vk_pipeline_cache_object *obj)
{
   struct vk_pipeline_precomp_shader *shader =
      vk_pipeline_precomp_shader_from_cache_obj(obj);

   blob_finish(&shader->nir_blob);
   vk_pipeline_cache_object_finish(&shader->cache_obj);
   vk_free(&device->alloc, shader);
}

static nir_shader *
vk_pipeline_precomp_shader_get_nir(const struct vk_pipeline_precomp_shader *shader,
                                   const struct nir_shader_compiler_options *nir_options)
{
   struct blob_reader blob;
   blob_reader_init(&blob, shader->nir_blob.data, shader->nir_blob.size);

   nir_shader *nir = nir_deserialize(NULL, nir_options, &blob);
   if (blob.overrun) {
      ralloc_free(nir);
      return NULL;
   }

   return nir;
}

static const struct vk_pipeline_cache_object_ops pipeline_precomp_shader_cache_ops = {
   .serialize = vk_pipeline_precomp_shader_serialize,
   .deserialize = vk_pipeline_precomp_shader_deserialize,
   .destroy = vk_pipeline_precomp_shader_destroy,
};

static VkResult
vk_pipeline_precompile_shader(struct vk_device *device,
                              struct vk_pipeline_cache *cache,
                              VkPipelineCreateFlags2KHR pipeline_flags,
                              const void *pipeline_info_pNext,
                              const VkPipelineShaderStageCreateInfo *info,
                              struct vk_pipeline_precomp_shader **ps_out)
{
   const struct vk_device_shader_ops *ops = device->shader_ops;
   VkResult result;

   struct vk_pipeline_robustness_state rs;
   vk_pipeline_robustness_state_fill(device, &rs,
                                     pipeline_info_pNext,
                                     info->pNext);

   uint8_t stage_sha1[SHA1_DIGEST_LENGTH];
   vk_pipeline_hash_shader_stage(info, &rs, stage_sha1);

   if (cache != NULL) {
      struct vk_pipeline_cache_object *cache_obj =
         vk_pipeline_cache_lookup_object(cache, stage_sha1, sizeof(stage_sha1),
                                         &pipeline_precomp_shader_cache_ops,
                                         NULL /* cache_hit */);
      if (cache_obj != NULL) {
         *ps_out = vk_pipeline_precomp_shader_from_cache_obj(cache_obj);
         return VK_SUCCESS;
      }
   }

   if (pipeline_flags &
       VK_PIPELINE_CREATE_2_FAIL_ON_PIPELINE_COMPILE_REQUIRED_BIT_KHR)
      return VK_PIPELINE_COMPILE_REQUIRED;

   const gl_shader_stage stage = vk_to_mesa_shader_stage(info->stage);
   const struct nir_shader_compiler_options *nir_options =
      ops->get_nir_options(device->physical, stage, &rs);
   const struct spirv_to_nir_options spirv_options =
      ops->get_spirv_options(device->physical, stage, &rs);

   nir_shader *nir;
   result = vk_pipeline_shader_stage_to_nir(device, info, &spirv_options,
                                            nir_options, NULL, &nir);
   if (result != VK_SUCCESS)
      return result;

   if (ops->preprocess_nir != NULL)
      ops->preprocess_nir(device->physical, nir);

   struct vk_pipeline_precomp_shader *shader =
      vk_pipeline_precomp_shader_create(device, stage_sha1,
                                        sizeof(stage_sha1),
                                        &rs, nir);
   ralloc_free(nir);
   if (shader == NULL)
      return vk_error(device, VK_ERROR_OUT_OF_HOST_MEMORY);

   if (cache != NULL) {
      struct vk_pipeline_cache_object *cache_obj = &shader->cache_obj;
      cache_obj = vk_pipeline_cache_add_object(cache, cache_obj);
      shader = vk_pipeline_precomp_shader_from_cache_obj(cache_obj);
   }

   *ps_out = shader;

   return VK_SUCCESS;
}

struct vk_pipeline_stage {
   gl_shader_stage stage;

   struct vk_pipeline_precomp_shader *precomp;
   struct vk_shader *shader;
};

static int
cmp_vk_pipeline_stages(const void *_a, const void *_b)
{
   const struct vk_pipeline_stage *a = _a, *b = _b;
   return vk_shader_cmp_graphics_stages(a->stage, b->stage);
}

static bool
vk_pipeline_stage_is_null(const struct vk_pipeline_stage *stage)
{
   return stage->precomp == NULL && stage->shader == NULL;
}

static void
vk_pipeline_stage_finish(struct vk_device *device,
                         struct vk_pipeline_stage *stage)
{
   if (stage->precomp != NULL)
      vk_pipeline_precomp_shader_unref(device, stage->precomp);

   if (stage->shader)
      vk_shader_unref(device, stage->shader);
}

static struct vk_pipeline_stage
vk_pipeline_stage_clone(const struct vk_pipeline_stage *in)
{
   struct vk_pipeline_stage out = {
      .stage = in->stage,
   };

   if (in->precomp)
      out.precomp = vk_pipeline_precomp_shader_ref(in->precomp);

   if (in->shader)
      out.shader = vk_shader_ref(in->shader);

   return out;
}

struct vk_graphics_pipeline {
   struct vk_pipeline base;

   union {
      struct {
         struct vk_graphics_pipeline_all_state all_state;
         struct vk_graphics_pipeline_state state;
      } lib;

      struct {
         struct vk_vertex_input_state _dynamic_vi;
         struct vk_sample_locations_state _dynamic_sl;
         struct vk_dynamic_graphics_state dynamic;
      } linked;
   };

   uint32_t set_layout_count;
   struct vk_descriptor_set_layout *set_layouts[MESA_VK_MAX_DESCRIPTOR_SETS];

   uint32_t stage_count;
   struct vk_pipeline_stage stages[MESA_VK_MAX_GRAPHICS_PIPELINE_STAGES];
};

static void
vk_graphics_pipeline_destroy(struct vk_device *device,
                             struct vk_pipeline *pipeline,
                             const VkAllocationCallbacks *pAllocator)
{
   struct vk_graphics_pipeline *gfx_pipeline =
      container_of(pipeline, struct vk_graphics_pipeline, base);

   for (uint32_t i = 0; i < gfx_pipeline->stage_count; i++)
      vk_pipeline_stage_finish(device, &gfx_pipeline->stages[i]);

   for (uint32_t i = 0; i < gfx_pipeline->set_layout_count; i++) {
      if (gfx_pipeline->set_layouts[i] != NULL)
         vk_descriptor_set_layout_unref(device, gfx_pipeline->set_layouts[i]);
   }

   vk_pipeline_free(device, pAllocator, pipeline);
}

static bool
vk_device_supports_stage(struct vk_device *device,
                         gl_shader_stage stage)
{
   const struct vk_features *features = &device->physical->supported_features;

   switch (stage) {
   case MESA_SHADER_VERTEX:
   case MESA_SHADER_FRAGMENT:
   case MESA_SHADER_COMPUTE:
      return true;
   case MESA_SHADER_TESS_CTRL:
   case MESA_SHADER_TESS_EVAL:
      return features->tessellationShader;
   case MESA_SHADER_GEOMETRY:
      return features->geometryShader;
   case MESA_SHADER_TASK:
      return features->taskShader;
   case MESA_SHADER_MESH:
      return features->meshShader;
   default:
      return false;
   }
}

static const gl_shader_stage all_gfx_stages[] = {
   MESA_SHADER_VERTEX,
   MESA_SHADER_TESS_CTRL,
   MESA_SHADER_TESS_EVAL,
   MESA_SHADER_GEOMETRY,
   MESA_SHADER_TASK,
   MESA_SHADER_MESH,
   MESA_SHADER_FRAGMENT,
};

static void
vk_graphics_pipeline_cmd_bind(struct vk_command_buffer *cmd_buffer,
                              struct vk_pipeline *pipeline)
{
   struct vk_device *device = cmd_buffer->base.device;
   const struct vk_device_shader_ops *ops = device->shader_ops;

   struct vk_graphics_pipeline *gfx_pipeline = NULL;
   struct vk_shader *stage_shader[PIPE_SHADER_MESH_TYPES] = { NULL, };
   if (pipeline != NULL) {
      assert(pipeline->bind_point == VK_PIPELINE_BIND_POINT_GRAPHICS);
      assert(!(pipeline->flags & VK_PIPELINE_CREATE_2_LIBRARY_BIT_KHR));
      gfx_pipeline = container_of(pipeline, struct vk_graphics_pipeline, base);

      for (uint32_t i = 0; i < gfx_pipeline->stage_count; i++) {
         struct vk_shader *shader = gfx_pipeline->stages[i].shader;
         stage_shader[shader->stage] = shader;
      }
   }

   uint32_t stage_count = 0;
   gl_shader_stage stages[ARRAY_SIZE(all_gfx_stages)];
   struct vk_shader *shaders[ARRAY_SIZE(all_gfx_stages)];

   VkShaderStageFlags vk_stages = 0;
   for (uint32_t i = 0; i < ARRAY_SIZE(all_gfx_stages); i++) {
      gl_shader_stage stage = all_gfx_stages[i];
      if (!vk_device_supports_stage(device, stage)) {
         assert(stage_shader[stage] == NULL);
         continue;
      }

      vk_stages |= mesa_to_vk_shader_stage(stage);

      stages[stage_count] = stage;
      shaders[stage_count] = stage_shader[stage];
      stage_count++;
   }
   ops->cmd_bind_shaders(cmd_buffer, stage_count, stages, shaders);

   if (gfx_pipeline != NULL) {
      cmd_buffer->pipeline_shader_stages |= vk_stages;
      ops->cmd_set_dynamic_graphics_state(cmd_buffer,
                                          &gfx_pipeline->linked.dynamic);
   } else {
      cmd_buffer->pipeline_shader_stages &= ~vk_stages;
   }
}

static VkShaderCreateFlagsEXT
vk_pipeline_to_shader_flags(VkPipelineCreateFlags2KHR pipeline_flags,
                            gl_shader_stage stage)
{
   VkShaderCreateFlagsEXT shader_flags = 0;

   if (pipeline_flags & VK_PIPELINE_CREATE_2_CAPTURE_INTERNAL_REPRESENTATIONS_BIT_KHR)
      shader_flags |= VK_SHADER_CREATE_CAPTURE_INTERNAL_REPRESENTATIONS_BIT_MESA;

   if (stage == MESA_SHADER_FRAGMENT) {
      if (pipeline_flags & VK_PIPELINE_CREATE_2_RENDERING_FRAGMENT_SHADING_RATE_ATTACHMENT_BIT_KHR)
         shader_flags |= VK_SHADER_CREATE_FRAGMENT_SHADING_RATE_ATTACHMENT_BIT_EXT;

      if (pipeline_flags & VK_PIPELINE_CREATE_2_RENDERING_FRAGMENT_DENSITY_MAP_ATTACHMENT_BIT_EXT)
         shader_flags |= VK_SHADER_CREATE_FRAGMENT_DENSITY_MAP_ATTACHMENT_BIT_EXT;
   }

   if (stage == MESA_SHADER_COMPUTE) {
      if (pipeline_flags & VK_PIPELINE_CREATE_2_DISPATCH_BASE_BIT_KHR)
         shader_flags |= VK_SHADER_CREATE_DISPATCH_BASE_BIT_EXT;
   }

   return shader_flags;
}

static VkResult
vk_graphics_pipeline_compile_shaders(struct vk_device *device,
                                     struct vk_pipeline_cache *cache,
                                     struct vk_graphics_pipeline *pipeline,
                                     struct vk_pipeline_layout *pipeline_layout,
                                     const struct vk_graphics_pipeline_state *state,
                                     uint32_t stage_count,
                                     struct vk_pipeline_stage *stages,
                                     VkPipelineCreationFeedback *stage_feedbacks)
{
   const struct vk_device_shader_ops *ops = device->shader_ops;
   VkResult result;

   if (stage_count == 0)
      return VK_SUCCESS;

   /* If we're linking, throw away any previously compiled shaders as they
    * likely haven't been properly linked.  We keep the precompiled shaders
    * and we still look it up in the cache so it may still be fast.
    */
   if (pipeline->base.flags & VK_PIPELINE_CREATE_2_LINK_TIME_OPTIMIZATION_BIT_EXT) {
      for (uint32_t i = 0; i < stage_count; i++) {
         if (stages[i].shader != NULL) {
            vk_shader_unref(device, stages[i].shader);
            stages[i].shader = NULL;
         }
      }
   }

   bool have_all_shaders = true;
   VkShaderStageFlags all_stages = 0;
   struct vk_pipeline_precomp_shader *tcs_precomp = NULL, *tes_precomp = NULL;
   for (uint32_t i = 0; i < stage_count; i++) {
      all_stages |= mesa_to_vk_shader_stage(stages[i].stage);

      if (stages[i].shader == NULL)
         have_all_shaders = false;

      if (stages[i].stage == MESA_SHADER_TESS_CTRL)
         tcs_precomp = stages[i].precomp;

      if (stages[i].stage == MESA_SHADER_TESS_EVAL)
         tes_precomp = stages[i].precomp;
   }

   /* If we already have a shader for each stage, there's nothing to do. */
   if (have_all_shaders)
      return VK_SUCCESS;

   struct vk_pipeline_tess_info tess_info = { ._pad = 0 };
   if (tcs_precomp != NULL && tes_precomp != NULL) {
      tess_info = tcs_precomp->tess;
      vk_pipeline_tess_info_merge(&tess_info, &tes_precomp->tess);
   }

   struct mesa_blake3 blake3_ctx;
   _mesa_blake3_init(&blake3_ctx);
   for (uint32_t i = 0; i < pipeline->set_layout_count; i++) {
      if (pipeline->set_layouts[i] != NULL) {
         _mesa_blake3_update(&blake3_ctx, pipeline->set_layouts[i]->blake3,
                           sizeof(pipeline->set_layouts[i]->blake3));
      }
   }
   if (pipeline_layout != NULL) {
      _mesa_blake3_update(&blake3_ctx, &pipeline_layout->push_ranges,
                        sizeof(pipeline_layout->push_ranges[0]) *
                           pipeline_layout->push_range_count);
   }
   blake3_hash layout_blake3;
   _mesa_blake3_final(&blake3_ctx, layout_blake3);

   /* Partition the shaders */
   uint32_t part_count;
   uint32_t partition[MESA_VK_MAX_GRAPHICS_PIPELINE_STAGES + 1] = { 0 };
   if (pipeline->base.flags & VK_PIPELINE_CREATE_2_LINK_TIME_OPTIMIZATION_BIT_EXT) {
      partition[1] = stage_count;
      part_count = 1;
   } else if (ops->link_geom_stages) {
      if (stages[0].stage == MESA_SHADER_FRAGMENT) {
         assert(stage_count == 1);
         partition[1] = stage_count;
         part_count = 1;
      } else if (stages[stage_count - 1].stage == MESA_SHADER_FRAGMENT) {
         /* In this case we have both */
         assert(stage_count > 1);
         partition[1] = stage_count - 1;
         partition[2] = stage_count;
         part_count = 2;
      } else {
         /* In this case we only have geometry */
         partition[1] = stage_count;
         part_count = 1;
      }
   } else {
      /* Otherwise, we're don't want to link anything */
      part_count = stage_count;
      for (uint32_t i = 0; i < stage_count; i++)
         partition[i + 1] = i + 1;
   }

   for (uint32_t p = 0; p < part_count; p++) {
      const int64_t part_start = os_time_get_nano();

      /* Don't try to re-compile any fast-link shaders */
      if (!(pipeline->base.flags &
            VK_PIPELINE_CREATE_2_LINK_TIME_OPTIMIZATION_BIT_EXT)) {
         assert(partition[p + 1] == partition[p] + 1);
         if (stages[partition[p]].shader != NULL)
            continue;
      }

      struct vk_shader_pipeline_cache_key shader_key = { 0 };

      _mesa_blake3_init(&blake3_ctx);

      VkShaderStageFlags part_stages = 0;
      for (uint32_t i = partition[p]; i < partition[p + 1]; i++) {
         const struct vk_pipeline_stage *stage = &stages[i];

         part_stages |= mesa_to_vk_shader_stage(stage->stage);
         _mesa_blake3_update(&blake3_ctx, stage->precomp->blake3,
                             sizeof(stage->precomp->blake3));

         VkShaderCreateFlagsEXT shader_flags =
            vk_pipeline_to_shader_flags(pipeline->base.flags, stage->stage);
         _mesa_blake3_update(&blake3_ctx, &shader_flags, sizeof(shader_flags));
      }

      blake3_hash state_blake3;
      ops->hash_graphics_state(device->physical, state,
                               part_stages, state_blake3);

      _mesa_blake3_update(&blake3_ctx, state_blake3, sizeof(state_blake3));
      _mesa_blake3_update(&blake3_ctx, layout_blake3, sizeof(layout_blake3));

      if (part_stages & (VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT |
                         VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT))
         _mesa_blake3_update(&blake3_ctx, &tess_info, sizeof(tess_info));

      /* The set of geometry stages used together is used to generate the
       * nextStage mask as well as VK_SHADER_CREATE_NO_TASK_SHADER_BIT_EXT.
       */
      const VkShaderStageFlags geom_stages =
         all_stages & ~VK_SHADER_STAGE_FRAGMENT_BIT;
      _mesa_blake3_update(&blake3_ctx, &geom_stages, sizeof(geom_stages));

      _mesa_blake3_final(&blake3_ctx, shader_key.blake3);

      if (cache != NULL) {
         /* From the Vulkan 1.3.278 spec:
          *
          *    "VK_PIPELINE_CREATION_FEEDBACK_APPLICATION_PIPELINE_CACHE_HIT_BIT
          *    indicates that a readily usable pipeline or pipeline stage was
          *    found in the pipelineCache specified by the application in the
          *    pipeline creation command.
          *
          *    [...]
          *
          *    Note
          *
          *    Implementations are encouraged to provide a meaningful signal
          *    to applications using this bit. The intention is to communicate
          *    to the application that the pipeline or pipeline stage was
          *    created “as fast as it gets” using the pipeline cache provided
          *    by the application. If an implementation uses an internal
          *    cache, it is discouraged from setting this bit as the feedback
          *    would be unactionable."
          *
          * The cache_hit value returned by vk_pipeline_cache_lookup_object()
          * is only set to true when the shader is found in the provided
          * pipeline cache.  It is left false if we fail to find it in the
          * memory cache but find it in the disk cache even though that's
          * still a cache hit from the perspective of the compile pipeline.
          */
         bool all_shaders_found = true;
         bool all_cache_hits = true;
         for (uint32_t i = partition[p]; i < partition[p + 1]; i++) {
            struct vk_pipeline_stage *stage = &stages[i];

            shader_key.stage = stage->stage;

            if (stage->shader) {
               /* If we have a shader from some library pipeline and the key
                * matches, just use that.
                */
               if (memcmp(&stage->shader->pipeline.cache_key,
                          &shader_key, sizeof(shader_key)) == 0)
                  continue;

               /* Otherwise, throw it away */
               vk_shader_unref(device, stage->shader);
               stage->shader = NULL;
            }

            bool cache_hit = false;
            struct vk_pipeline_cache_object *cache_obj =
               vk_pipeline_cache_lookup_object(cache, &shader_key,
                                               sizeof(shader_key),
                                               &pipeline_shader_cache_ops,
                                               &cache_hit);
            if (cache_obj != NULL) {
               assert(stage->shader == NULL);
               stage->shader = vk_shader_from_cache_obj(cache_obj);
            } else {
               all_shaders_found = false;
            }

            if (cache_obj == NULL && !cache_hit)
               all_cache_hits = false;
         }

         if (all_cache_hits && cache != device->mem_cache) {
            /* The pipeline cache only really helps if we hit for everything
             * in the partition.  Otherwise, we have to go re-compile it all
             * anyway.
             */
            for (uint32_t i = partition[p]; i < partition[p + 1]; i++) {
               struct vk_pipeline_stage *stage = &stages[i];

               stage_feedbacks[stage->stage].flags |=
                  VK_PIPELINE_CREATION_FEEDBACK_APPLICATION_PIPELINE_CACHE_HIT_BIT;
            }
         }

         if (all_shaders_found) {
            /* Update duration to take cache lookups into account */
            const int64_t part_end = os_time_get_nano();
            for (uint32_t i = partition[p]; i < partition[p + 1]; i++) {
               struct vk_pipeline_stage *stage = &stages[i];
               stage_feedbacks[stage->stage].duration += part_end - part_start;
            }
            continue;
         }
      }

      if (pipeline->base.flags &
          VK_PIPELINE_CREATE_2_FAIL_ON_PIPELINE_COMPILE_REQUIRED_BIT_KHR)
         return VK_PIPELINE_COMPILE_REQUIRED;

      struct vk_shader_compile_info infos[MESA_VK_MAX_GRAPHICS_PIPELINE_STAGES];
      for (uint32_t i = partition[p]; i < partition[p + 1]; i++) {
         struct vk_pipeline_stage *stage = &stages[i];

         VkShaderCreateFlagsEXT shader_flags =
            vk_pipeline_to_shader_flags(pipeline->base.flags, stage->stage);

         if (partition[p + 1] - partition[p] > 1)
            shader_flags |= VK_SHADER_CREATE_LINK_STAGE_BIT_EXT;

         if ((part_stages & VK_SHADER_STAGE_MESH_BIT_EXT) &&
             !(geom_stages & VK_SHADER_STAGE_TASK_BIT_EXT))
            shader_flags = VK_SHADER_CREATE_NO_TASK_SHADER_BIT_EXT;

         VkShaderStageFlags next_stage;
         if (stage->stage == MESA_SHADER_FRAGMENT) {
            next_stage = 0;
         } else if (i + 1 < stage_count) {
            /* We hash geom_stages above so this is safe */
            next_stage = mesa_to_vk_shader_stage(stages[i + 1].stage);
         } else {
            /* We're the last geometry stage */
            next_stage = VK_SHADER_STAGE_FRAGMENT_BIT;
         }

         const struct nir_shader_compiler_options *nir_options =
            ops->get_nir_options(device->physical, stage->stage,
                                 &stage->precomp->rs);

         nir_shader *nir =
            vk_pipeline_precomp_shader_get_nir(stage->precomp, nir_options);
         if (nir == NULL) {
            for (uint32_t j = partition[p]; j < i; j++)
               ralloc_free(infos[i].nir);

            return vk_error(device, VK_ERROR_OUT_OF_HOST_MEMORY);
         }

         if (stage->stage == MESA_SHADER_TESS_CTRL ||
             stage->stage == MESA_SHADER_TESS_EVAL)
            vk_pipeline_replace_nir_tess_info(nir, &tess_info);

         const VkPushConstantRange *push_range = NULL;
         if (pipeline_layout != NULL) {
            for (uint32_t r = 0; r < pipeline_layout->push_range_count; r++) {
               if (pipeline_layout->push_ranges[r].stageFlags &
                   mesa_to_vk_shader_stage(stage->stage)) {
                  assert(push_range == NULL);
                  push_range = &pipeline_layout->push_ranges[r];
               }
            }
         }

         infos[i] = (struct vk_shader_compile_info) {
            .stage = stage->stage,
            .flags = shader_flags,
            .next_stage_mask = next_stage,
            .nir = nir,
            .robustness = &stage->precomp->rs,
            .set_layout_count = pipeline->set_layout_count,
            .set_layouts = pipeline->set_layouts,
            .push_constant_range_count = push_range != NULL,
            .push_constant_ranges = push_range != NULL ? push_range : NULL,
         };
      }

      /* vk_shader_ops::compile() consumes the NIR regardless of whether or
       * not it succeeds and only generates shaders on success. Once this
       * returns, we own the shaders but not the NIR in infos.
       */
      struct vk_shader *shaders[MESA_VK_MAX_GRAPHICS_PIPELINE_STAGES];
      result = ops->compile(device, partition[p + 1] - partition[p],
                            &infos[partition[p]],
                            state,
                            &device->alloc,
                            &shaders[partition[p]]);
      if (result != VK_SUCCESS)
         return result;

      const int64_t part_end = os_time_get_nano();
      for (uint32_t i = partition[p]; i < partition[p + 1]; i++) {
         struct vk_pipeline_stage *stage = &stages[i];

         shader_key.stage = stage->stage;
         vk_shader_init_cache_obj(device, shaders[i], &shader_key,
                                  sizeof(shader_key));

         if (stage->shader == NULL) {
            struct vk_pipeline_cache_object *cache_obj =
               &shaders[i]->pipeline.cache_obj;
            if (cache != NULL)
               cache_obj = vk_pipeline_cache_add_object(cache, cache_obj);

            stage->shader = vk_shader_from_cache_obj(cache_obj);
         } else {
            /* This can fail to happen if only some of the shaders were found
             * in the pipeline cache.  In this case, we just throw away the
             * shader as vk_pipeline_cache_add_object() would throw it away
             * for us anyway.
             */
            assert(memcmp(&stage->shader->pipeline.cache_key,
                          &shaders[i]->pipeline.cache_key,
                          sizeof(shaders[i]->pipeline.cache_key)) == 0);

            vk_shader_unref(device, shaders[i]);
         }

         stage_feedbacks[stage->stage].duration += part_end - part_start;
      }
   }

   return VK_SUCCESS;
}

static VkResult
vk_graphics_pipeline_get_executable_properties(
   struct vk_device *device,
   struct vk_pipeline *pipeline,
   uint32_t *executable_count,
   VkPipelineExecutablePropertiesKHR *properties)
{
   struct vk_graphics_pipeline *gfx_pipeline =
      container_of(pipeline, struct vk_graphics_pipeline, base);
   VkResult result;

   if (properties == NULL) {
      *executable_count = 0;
      for (uint32_t i = 0; i < gfx_pipeline->stage_count; i++) {
         struct vk_shader *shader = gfx_pipeline->stages[i].shader;

         uint32_t shader_exec_count = 0;
         result = shader->ops->get_executable_properties(device, shader,
                                                         &shader_exec_count,
                                                         NULL);
         assert(result == VK_SUCCESS);
         *executable_count += shader_exec_count;
      }
   } else {
      uint32_t arr_len = *executable_count;
      *executable_count = 0;
      for (uint32_t i = 0; i < gfx_pipeline->stage_count; i++) {
         struct vk_shader *shader = gfx_pipeline->stages[i].shader;

         uint32_t shader_exec_count = arr_len - *executable_count;
         result = shader->ops->get_executable_properties(device, shader,
                                                         &shader_exec_count,
                                                         &properties[*executable_count]);
         if (result != VK_SUCCESS)
            return result;

         *executable_count += shader_exec_count;
      }
   }

   return VK_SUCCESS;
}

static inline struct vk_shader *
vk_graphics_pipeline_executable_shader(struct vk_device *device,
                                       struct vk_graphics_pipeline *gfx_pipeline,
                                       uint32_t *executable_index)
{
   for (uint32_t i = 0; i < gfx_pipeline->stage_count; i++) {
      struct vk_shader *shader = gfx_pipeline->stages[i].shader;

      uint32_t shader_exec_count = 0;
      shader->ops->get_executable_properties(device, shader,
                                             &shader_exec_count, NULL);

      if (*executable_index < shader_exec_count)
         return shader;
      else
         *executable_index -= shader_exec_count;
   }

   return NULL;
}

static VkResult
vk_graphics_pipeline_get_executable_statistics(
   struct vk_device *device,
   struct vk_pipeline *pipeline,
   uint32_t executable_index,
   uint32_t *statistic_count,
   VkPipelineExecutableStatisticKHR *statistics)
{
   struct vk_graphics_pipeline *gfx_pipeline =
      container_of(pipeline, struct vk_graphics_pipeline, base);

   struct vk_shader *shader =
      vk_graphics_pipeline_executable_shader(device, gfx_pipeline,
                                             &executable_index);
   if (shader == NULL) {
      *statistic_count = 0;
      return VK_SUCCESS;
   }

   return shader->ops->get_executable_statistics(device, shader,
                                                 executable_index,
                                                 statistic_count,
                                                 statistics);
}

static VkResult
vk_graphics_pipeline_get_internal_representations(
   struct vk_device *device,
   struct vk_pipeline *pipeline,
   uint32_t executable_index,
   uint32_t *internal_representation_count,
   VkPipelineExecutableInternalRepresentationKHR* internal_representations)
{
   struct vk_graphics_pipeline *gfx_pipeline =
      container_of(pipeline, struct vk_graphics_pipeline, base);

   struct vk_shader *shader =
      vk_graphics_pipeline_executable_shader(device, gfx_pipeline,
                                             &executable_index);
   if (shader == NULL) {
      *internal_representation_count = 0;
      return VK_SUCCESS;
   }

   return shader->ops->get_executable_internal_representations(
      device, shader, executable_index,
      internal_representation_count, internal_representations);
}

static const struct vk_pipeline_ops vk_graphics_pipeline_ops = {
   .destroy = vk_graphics_pipeline_destroy,
   .get_executable_statistics = vk_graphics_pipeline_get_executable_statistics,
   .get_executable_properties = vk_graphics_pipeline_get_executable_properties,
   .get_internal_representations = vk_graphics_pipeline_get_internal_representations,
   .cmd_bind = vk_graphics_pipeline_cmd_bind,
};

static VkResult
vk_create_graphics_pipeline(struct vk_device *device,
                            struct vk_pipeline_cache *cache,
                            const VkGraphicsPipelineCreateInfo *pCreateInfo,
                            const VkAllocationCallbacks *pAllocator,
                            VkPipeline *pPipeline)
{
   VK_FROM_HANDLE(vk_pipeline_layout, pipeline_layout, pCreateInfo->layout);
   const int64_t pipeline_start = os_time_get_nano();
   VkResult result;

   const VkPipelineCreateFlags2KHR pipeline_flags =
      vk_graphics_pipeline_create_flags(pCreateInfo);

   const VkPipelineCreationFeedbackCreateInfo *feedback_info =
      vk_find_struct_const(pCreateInfo->pNext,
                           PIPELINE_CREATION_FEEDBACK_CREATE_INFO);

   const VkPipelineLibraryCreateInfoKHR *libs_info =
      vk_find_struct_const(pCreateInfo->pNext,
                           PIPELINE_LIBRARY_CREATE_INFO_KHR);

   struct vk_graphics_pipeline *pipeline =
      vk_pipeline_zalloc(device, &vk_graphics_pipeline_ops,
                         VK_PIPELINE_BIND_POINT_GRAPHICS,
                         pipeline_flags, pAllocator, sizeof(*pipeline));
   if (pipeline == NULL)
      return vk_error(device, VK_ERROR_OUT_OF_HOST_MEMORY);

   struct vk_pipeline_stage stages[PIPE_SHADER_MESH_TYPES];
   memset(stages, 0, sizeof(stages));

   VkPipelineCreationFeedback stage_feedbacks[PIPE_SHADER_MESH_TYPES];
   memset(stage_feedbacks, 0, sizeof(stage_feedbacks));

   struct vk_graphics_pipeline_state state_tmp, *state;
   struct vk_graphics_pipeline_all_state all_state_tmp, *all_state;
   if (pipeline->base.flags & VK_PIPELINE_CREATE_2_LIBRARY_BIT_KHR) {
      /* For pipeline libraries, the state is stored in the pipeline */
      state = &pipeline->lib.state;
      all_state = &pipeline->lib.all_state;
   } else {
      /* For linked pipelines, we throw the state away at the end of pipeline
       * creation and only keep the dynamic state.
       */
      memset(&state_tmp, 0, sizeof(state_tmp));
      state = &state_tmp;
      all_state = &all_state_tmp;
   }

   /* If we have libraries, import them first. */
   if (libs_info) {
      for (uint32_t i = 0; i < libs_info->libraryCount; i++) {
         VK_FROM_HANDLE(vk_pipeline, lib_pipeline, libs_info->pLibraries[i]);
         assert(lib_pipeline->bind_point == VK_PIPELINE_BIND_POINT_GRAPHICS);
         assert(lib_pipeline->flags & VK_PIPELINE_CREATE_2_LIBRARY_BIT_KHR);
         struct vk_graphics_pipeline *lib_gfx_pipeline =
            container_of(lib_pipeline, struct vk_graphics_pipeline, base);

         vk_graphics_pipeline_state_merge(state, &lib_gfx_pipeline->lib.state);

         pipeline->set_layout_count = MAX2(pipeline->set_layout_count,
                                           lib_gfx_pipeline->set_layout_count);
         for (uint32_t i = 0; i < lib_gfx_pipeline->set_layout_count; i++) {
            if (lib_gfx_pipeline->set_layouts[i] == NULL)
               continue;

            if (pipeline->set_layouts[i] == NULL) {
               pipeline->set_layouts[i] =
                  vk_descriptor_set_layout_ref(lib_gfx_pipeline->set_layouts[i]);
            }
         }

         for (uint32_t i = 0; i < lib_gfx_pipeline->stage_count; i++) {
            const struct vk_pipeline_stage *lib_stage =
               &lib_gfx_pipeline->stages[i];

            /* We shouldn't have duplicated stages in the imported pipeline
             * but it's cheap enough to protect against it so we may as well.
             */
            assert(lib_stage->stage < ARRAY_SIZE(stages));
            assert(vk_pipeline_stage_is_null(&stages[lib_stage->stage]));
            if (!vk_pipeline_stage_is_null(&stages[lib_stage->stage]))
               continue;

            stages[lib_stage->stage] = vk_pipeline_stage_clone(lib_stage);
         }
      }
   }

   result = vk_graphics_pipeline_state_fill(device, state,
                                            pCreateInfo,
                                            NULL /* driver_rp */,
                                            0 /* driver_rp_flags */,
                                            all_state,
                                            NULL, 0, NULL);
   if (result != VK_SUCCESS)
      goto fail_stages;

   if (!(pipeline->base.flags & VK_PIPELINE_CREATE_2_LIBRARY_BIT_KHR)) {
      pipeline->linked.dynamic.vi = &pipeline->linked._dynamic_vi;
      pipeline->linked.dynamic.ms.sample_locations =
         &pipeline->linked._dynamic_sl;
      vk_dynamic_graphics_state_fill(&pipeline->linked.dynamic, &state_tmp);
   }

   if (pipeline_layout != NULL) {
      pipeline->set_layout_count = MAX2(pipeline->set_layout_count,
                                        pipeline_layout->set_count);
      for (uint32_t i = 0; i < pipeline_layout->set_count; i++) {
         if (pipeline_layout->set_layouts[i] == NULL)
            continue;

         if (pipeline->set_layouts[i] == NULL) {
            pipeline->set_layouts[i] =
               vk_descriptor_set_layout_ref(pipeline_layout->set_layouts[i]);
         }
      }
   }

   for (uint32_t i = 0; i < pCreateInfo->stageCount; i++) {
      const VkPipelineShaderStageCreateInfo *stage_info =
         &pCreateInfo->pStages[i];

      const int64_t stage_start = os_time_get_nano();

      assert(util_bitcount(stage_info->stage) == 1);
      if (!(state->shader_stages & stage_info->stage))
         continue;

      gl_shader_stage stage = vk_to_mesa_shader_stage(stage_info->stage);
      assert(vk_device_supports_stage(device, stage));

      stage_feedbacks[stage].flags |=
         VK_PIPELINE_CREATION_FEEDBACK_VALID_BIT;

      if (!vk_pipeline_stage_is_null(&stages[stage]))
         continue;

      struct vk_pipeline_precomp_shader *precomp;
      result = vk_pipeline_precompile_shader(device, cache, pipeline_flags,
                                             pCreateInfo->pNext,
                                             stage_info,
                                             &precomp);
      if (result != VK_SUCCESS)
         goto fail_stages;

      stages[stage] = (struct vk_pipeline_stage) {
         .stage = stage,
         .precomp = precomp,
      };

      const int64_t stage_end = os_time_get_nano();
      stage_feedbacks[stage].duration += stage_end - stage_start;
   }

   /* Compact the array of stages */
   uint32_t stage_count = 0;
   for (uint32_t s = 0; s < ARRAY_SIZE(stages); s++) {
      assert(s >= stage_count);
      if (!vk_pipeline_stage_is_null(&stages[s]))
         stages[stage_count++] = stages[s];
   }
   for (uint32_t s = stage_count; s < ARRAY_SIZE(stages); s++)
      memset(&stages[s], 0, sizeof(stages[s]));

   /* Sort so we always give the driver shaders in order.
    *
    * This makes everything easier for everyone.  This also helps stabilize
    * shader keys so that we get a cache hit even if the client gives us
    * the stages in a different order.
    */
   qsort(stages, stage_count, sizeof(*stages), cmp_vk_pipeline_stages);

   result = vk_graphics_pipeline_compile_shaders(device, cache, pipeline,
                                                 pipeline_layout, state,
                                                 stage_count, stages,
                                                 stage_feedbacks);
   if (result != VK_SUCCESS)
      goto fail_stages;

   /* Throw away precompiled shaders unless the client explicitly asks us to
    * keep them.
    */
   if (!(pipeline_flags &
         VK_PIPELINE_CREATE_2_RETAIN_LINK_TIME_OPTIMIZATION_INFO_BIT_EXT)) {
      for (uint32_t i = 0; i < stage_count; i++) {
         if (stages[i].precomp != NULL) {
            vk_pipeline_precomp_shader_unref(device, stages[i].precomp);
            stages[i].precomp = NULL;
         }
      }
   }

   pipeline->stage_count = stage_count;
   for (uint32_t i = 0; i < stage_count; i++)
      pipeline->stages[i] = stages[i];

   const int64_t pipeline_end = os_time_get_nano();
   if (feedback_info != NULL) {
      VkPipelineCreationFeedback pipeline_feedback = {
         .flags = VK_PIPELINE_CREATION_FEEDBACK_VALID_BIT,
         .duration = pipeline_end - pipeline_start,
      };

      /* From the Vulkan 1.3.275 spec:
       *
       *    "An implementation should set the
       *    VK_PIPELINE_CREATION_FEEDBACK_APPLICATION_PIPELINE_CACHE_HIT_BIT
       *    bit if it was able to avoid the large majority of pipeline or
       *    pipeline stage creation work by using the pipelineCache parameter"
       *
       * We really shouldn't set this bit unless all the shaders hit the
       * cache.
       */
      uint32_t cache_hit_count = 0;
      for (uint32_t i = 0; i < stage_count; i++) {
         const gl_shader_stage stage = stages[i].stage;
         if (stage_feedbacks[stage].flags &
             VK_PIPELINE_CREATION_FEEDBACK_APPLICATION_PIPELINE_CACHE_HIT_BIT)
            cache_hit_count++;
      }
      if (cache_hit_count > 0 && cache_hit_count == stage_count) {
         pipeline_feedback.flags |=
            VK_PIPELINE_CREATION_FEEDBACK_APPLICATION_PIPELINE_CACHE_HIT_BIT;
      }

      *feedback_info->pPipelineCreationFeedback = pipeline_feedback;

      /* VUID-VkGraphicsPipelineCreateInfo-pipelineStageCreationFeedbackCount-06594 */
      assert(feedback_info->pipelineStageCreationFeedbackCount == 0 ||
             feedback_info->pipelineStageCreationFeedbackCount ==
             pCreateInfo->stageCount);
      for (uint32_t i = 0;
           i < feedback_info->pipelineStageCreationFeedbackCount; i++) {
         const gl_shader_stage stage =
            vk_to_mesa_shader_stage(pCreateInfo->pStages[i].stage);

         feedback_info->pPipelineStageCreationFeedbacks[i] =
            stage_feedbacks[stage];
      }
   }

   *pPipeline = vk_pipeline_to_handle(&pipeline->base);

   return VK_SUCCESS;

fail_stages:
   for (uint32_t i = 0; i < ARRAY_SIZE(stages); i++)
      vk_pipeline_stage_finish(device, &stages[i]);

   vk_graphics_pipeline_destroy(device, &pipeline->base, pAllocator);

   return result;
}

VKAPI_ATTR VkResult VKAPI_CALL
vk_common_CreateGraphicsPipelines(VkDevice _device,
                                  VkPipelineCache pipelineCache,
                                  uint32_t createInfoCount,
                                  const VkGraphicsPipelineCreateInfo *pCreateInfos,
                                  const VkAllocationCallbacks *pAllocator,
                                  VkPipeline *pPipelines)
{
   VK_FROM_HANDLE(vk_device, device, _device);
   VK_FROM_HANDLE(vk_pipeline_cache, cache, pipelineCache);
   VkResult first_error_or_success = VK_SUCCESS;

   /* Use implicit pipeline cache if there's no cache set */
   if (!cache && device->mem_cache)
      cache = device->mem_cache;

   /* From the Vulkan 1.3.274 spec:
    *
    *    "When attempting to create many pipelines in a single command, it is
    *    possible that creation may fail for a subset of them. In this case,
    *    the corresponding elements of pPipelines will be set to
    *    VK_NULL_HANDLE.
    */
   memset(pPipelines, 0, createInfoCount * sizeof(*pPipelines));

   unsigned i = 0;
   for (; i < createInfoCount; i++) {
      VkResult result = vk_create_graphics_pipeline(device, cache,
                                                    &pCreateInfos[i],
                                                    pAllocator,
                                                    &pPipelines[i]);
      if (result == VK_SUCCESS)
         continue;

      if (first_error_or_success == VK_SUCCESS)
         first_error_or_success = result;

      /* Bail out on the first error != VK_PIPELINE_COMPILE_REQUIRED as it
       * is not obvious what error should be report upon 2 different failures.
       */
      if (result != VK_PIPELINE_COMPILE_REQUIRED)
         return result;

      const VkPipelineCreateFlags2KHR flags =
         vk_graphics_pipeline_create_flags(&pCreateInfos[i]);
      if (flags & VK_PIPELINE_CREATE_2_EARLY_RETURN_ON_FAILURE_BIT_KHR)
         return result;
   }

   return first_error_or_success;
}

struct vk_compute_pipeline {
   struct vk_pipeline base;
   struct vk_shader *shader;
};

static void
vk_compute_pipeline_destroy(struct vk_device *device,
                            struct vk_pipeline *pipeline,
                            const VkAllocationCallbacks *pAllocator)
{
   struct vk_compute_pipeline *comp_pipeline =
      container_of(pipeline, struct vk_compute_pipeline, base);

   vk_shader_unref(device, comp_pipeline->shader);
   vk_pipeline_free(device, pAllocator, pipeline);
}

static void
vk_compute_pipeline_cmd_bind(struct vk_command_buffer *cmd_buffer,
                             struct vk_pipeline *pipeline)
{
   struct vk_device *device = cmd_buffer->base.device;
   const struct vk_device_shader_ops *ops = device->shader_ops;

   struct vk_shader *shader = NULL;
   if (pipeline != NULL) {
      assert(pipeline->bind_point == VK_PIPELINE_BIND_POINT_COMPUTE);
      struct vk_compute_pipeline *comp_pipeline =
         container_of(pipeline, struct vk_compute_pipeline, base);

      shader = comp_pipeline->shader;

      cmd_buffer->pipeline_shader_stages |= VK_SHADER_STAGE_COMPUTE_BIT;
   } else {
      cmd_buffer->pipeline_shader_stages &= ~VK_SHADER_STAGE_COMPUTE_BIT;
   }

   gl_shader_stage stage = MESA_SHADER_COMPUTE;
   ops->cmd_bind_shaders(cmd_buffer, 1, &stage, &shader);
}

static VkResult
vk_pipeline_compile_compute_stage(struct vk_device *device,
                                  struct vk_pipeline_cache *cache,
                                  struct vk_compute_pipeline *pipeline,
                                  struct vk_pipeline_layout *pipeline_layout,
                                  struct vk_pipeline_stage *stage,
                                  bool *cache_hit)
{
   const struct vk_device_shader_ops *ops = device->shader_ops;
   VkResult result;

   const VkPushConstantRange *push_range = NULL;
   if (pipeline_layout != NULL) {
      for (uint32_t r = 0; r < pipeline_layout->push_range_count; r++) {
         if (pipeline_layout->push_ranges[r].stageFlags &
             VK_SHADER_STAGE_COMPUTE_BIT) {
            assert(push_range == NULL);
            push_range = &pipeline_layout->push_ranges[r];
         }
      }
   }

   VkShaderCreateFlagsEXT shader_flags =
      vk_pipeline_to_shader_flags(pipeline->base.flags, MESA_SHADER_COMPUTE);

   struct mesa_blake3 blake3_ctx;
   _mesa_blake3_init(&blake3_ctx);

   _mesa_blake3_update(&blake3_ctx, stage->precomp->blake3,
                     sizeof(stage->precomp->blake3));

   _mesa_blake3_update(&blake3_ctx, &shader_flags, sizeof(shader_flags));

   for (uint32_t i = 0; i < pipeline_layout->set_count; i++) {
      if (pipeline_layout->set_layouts[i] != NULL) {
         _mesa_blake3_update(&blake3_ctx,
                             pipeline_layout->set_layouts[i]->blake3,
                             sizeof(pipeline_layout->set_layouts[i]->blake3));
      }
   }
   if (push_range != NULL)
      _mesa_blake3_update(&blake3_ctx, push_range, sizeof(*push_range));

   struct vk_shader_pipeline_cache_key shader_key = {
      .stage = MESA_SHADER_COMPUTE,
   };
   _mesa_blake3_final(&blake3_ctx, shader_key.blake3);

   if (cache != NULL) {
      struct vk_pipeline_cache_object *cache_obj =
         vk_pipeline_cache_lookup_object(cache, &shader_key,
                                         sizeof(shader_key),
                                         &pipeline_shader_cache_ops,
                                         cache_hit);
      if (cache_obj != NULL) {
         stage->shader = vk_shader_from_cache_obj(cache_obj);
         return VK_SUCCESS;
      }
   }

   if (pipeline->base.flags &
       VK_PIPELINE_CREATE_2_FAIL_ON_PIPELINE_COMPILE_REQUIRED_BIT_KHR)
      return VK_PIPELINE_COMPILE_REQUIRED;

   const struct nir_shader_compiler_options *nir_options =
      ops->get_nir_options(device->physical, stage->stage,
                           &stage->precomp->rs);

   nir_shader *nir = vk_pipeline_precomp_shader_get_nir(stage->precomp,
                                                        nir_options);
   if (nir == NULL)
      return vk_error(device, VK_ERROR_OUT_OF_HOST_MEMORY);

   /* vk_device_shader_ops::compile() consumes the NIR regardless of whether
    * or not it succeeds and only generates shaders on success. Once compile()
    * returns, we own the shaders but not the NIR in infos.
    */
   struct vk_shader_compile_info compile_info = {
      .stage = stage->stage,
      .flags = shader_flags,
      .next_stage_mask = 0,
      .nir = nir,
      .robustness = &stage->precomp->rs,
      .set_layout_count = pipeline_layout->set_count,
      .set_layouts = pipeline_layout->set_layouts,
      .push_constant_range_count = push_range != NULL,
      .push_constant_ranges = push_range != NULL ? push_range : NULL,
   };

   struct vk_shader *shader;
   result = ops->compile(device, 1, &compile_info, NULL,
                         &device->alloc, &shader);
   if (result != VK_SUCCESS)
      return result;

   vk_shader_init_cache_obj(device, shader, &shader_key, sizeof(shader_key));

   struct vk_pipeline_cache_object *cache_obj = &shader->pipeline.cache_obj;
   if (cache != NULL)
      cache_obj = vk_pipeline_cache_add_object(cache, cache_obj);

   stage->shader = vk_shader_from_cache_obj(cache_obj);

   return VK_SUCCESS;
}

static VkResult
vk_compute_pipeline_get_executable_properties(
   struct vk_device *device,
   struct vk_pipeline *pipeline,
   uint32_t *executable_count,
   VkPipelineExecutablePropertiesKHR *properties)
{
   struct vk_compute_pipeline *comp_pipeline =
      container_of(pipeline, struct vk_compute_pipeline, base);
   struct vk_shader *shader = comp_pipeline->shader;

   return shader->ops->get_executable_properties(device, shader,
                                                 executable_count,
                                                 properties);
}

static VkResult
vk_compute_pipeline_get_executable_statistics(
   struct vk_device *device,
   struct vk_pipeline *pipeline,
   uint32_t executable_index,
   uint32_t *statistic_count,
   VkPipelineExecutableStatisticKHR *statistics)
{
   struct vk_compute_pipeline *comp_pipeline =
      container_of(pipeline, struct vk_compute_pipeline, base);
   struct vk_shader *shader = comp_pipeline->shader;

   return shader->ops->get_executable_statistics(device, shader,
                                                 executable_index,
                                                 statistic_count,
                                                 statistics);
}

static VkResult
vk_compute_pipeline_get_internal_representations(
   struct vk_device *device,
   struct vk_pipeline *pipeline,
   uint32_t executable_index,
   uint32_t *internal_representation_count,
   VkPipelineExecutableInternalRepresentationKHR* internal_representations)
{
   struct vk_compute_pipeline *comp_pipeline =
      container_of(pipeline, struct vk_compute_pipeline, base);
   struct vk_shader *shader = comp_pipeline->shader;

   return shader->ops->get_executable_internal_representations(
      device, shader, executable_index,
      internal_representation_count, internal_representations);
}

static const struct vk_pipeline_ops vk_compute_pipeline_ops = {
   .destroy = vk_compute_pipeline_destroy,
   .get_executable_statistics = vk_compute_pipeline_get_executable_statistics,
   .get_executable_properties = vk_compute_pipeline_get_executable_properties,
   .get_internal_representations = vk_compute_pipeline_get_internal_representations,
   .cmd_bind = vk_compute_pipeline_cmd_bind,
};

static VkResult
vk_create_compute_pipeline(struct vk_device *device,
                           struct vk_pipeline_cache *cache,
                           const VkComputePipelineCreateInfo *pCreateInfo,
                           const VkAllocationCallbacks *pAllocator,
                           VkPipeline *pPipeline)
{
   VK_FROM_HANDLE(vk_pipeline_layout, pipeline_layout, pCreateInfo->layout);
   int64_t pipeline_start = os_time_get_nano();
   VkResult result;

   const VkPipelineCreateFlags2KHR pipeline_flags =
      vk_compute_pipeline_create_flags(pCreateInfo);

   const VkPipelineCreationFeedbackCreateInfo *feedback_info =
      vk_find_struct_const(pCreateInfo->pNext,
                           PIPELINE_CREATION_FEEDBACK_CREATE_INFO);

   struct vk_compute_pipeline *pipeline =
      vk_pipeline_zalloc(device, &vk_compute_pipeline_ops,
                         VK_PIPELINE_BIND_POINT_COMPUTE,
                         pipeline_flags, pAllocator, sizeof(*pipeline));
   if (pipeline == NULL)
      return vk_error(device, VK_ERROR_OUT_OF_HOST_MEMORY);

   struct vk_pipeline_stage stage = {
      .stage = MESA_SHADER_COMPUTE,
   };
   result = vk_pipeline_precompile_shader(device, cache, pipeline_flags,
                                          pCreateInfo->pNext,
                                          &pCreateInfo->stage,
                                          &stage.precomp);
   if (result != VK_SUCCESS)
      goto fail_pipeline;

   bool cache_hit;
   result = vk_pipeline_compile_compute_stage(device, cache, pipeline,
                                              pipeline_layout, &stage,
                                              &cache_hit);
   if (result != VK_SUCCESS)
      goto fail_stage;

   if (stage.precomp != NULL)
      vk_pipeline_precomp_shader_unref(device, stage.precomp);
   pipeline->shader = stage.shader;

   const int64_t pipeline_end = os_time_get_nano();
   if (feedback_info != NULL) {
      VkPipelineCreationFeedback pipeline_feedback = {
         .flags = VK_PIPELINE_CREATION_FEEDBACK_VALID_BIT,
         .duration = pipeline_end - pipeline_start,
      };
      if (cache_hit && cache != device->mem_cache) {
         pipeline_feedback.flags |=
            VK_PIPELINE_CREATION_FEEDBACK_APPLICATION_PIPELINE_CACHE_HIT_BIT;
      }

      *feedback_info->pPipelineCreationFeedback = pipeline_feedback;
      if (feedback_info->pipelineStageCreationFeedbackCount > 0) {
         feedback_info->pPipelineStageCreationFeedbacks[0] =
            pipeline_feedback;
      }
   }

   *pPipeline = vk_pipeline_to_handle(&pipeline->base);

   return VK_SUCCESS;

fail_stage:
   vk_pipeline_stage_finish(device, &stage);
fail_pipeline:
   vk_pipeline_free(device, pAllocator, &pipeline->base);

   return result;
}

VKAPI_ATTR VkResult VKAPI_CALL
vk_common_CreateComputePipelines(VkDevice _device,
                                 VkPipelineCache pipelineCache,
                                 uint32_t createInfoCount,
                                 const VkComputePipelineCreateInfo *pCreateInfos,
                                 const VkAllocationCallbacks *pAllocator,
                                 VkPipeline *pPipelines)
{
   VK_FROM_HANDLE(vk_device, device, _device);
   VK_FROM_HANDLE(vk_pipeline_cache, cache, pipelineCache);
   VkResult first_error_or_success = VK_SUCCESS;

   /* Use implicit pipeline cache if there's no cache set */
   if (!cache && device->mem_cache)
      cache = device->mem_cache;

   /* From the Vulkan 1.3.274 spec:
    *
    *    "When attempting to create many pipelines in a single command, it is
    *    possible that creation may fail for a subset of them. In this case,
    *    the corresponding elements of pPipelines will be set to
    *    VK_NULL_HANDLE.
    */
   memset(pPipelines, 0, createInfoCount * sizeof(*pPipelines));

   unsigned i = 0;
   for (; i < createInfoCount; i++) {
      VkResult result = vk_create_compute_pipeline(device, cache,
                                                   &pCreateInfos[i],
                                                   pAllocator,
                                                   &pPipelines[i]);
      if (result == VK_SUCCESS)
         continue;

      if (first_error_or_success == VK_SUCCESS)
         first_error_or_success = result;

      /* Bail out on the first error != VK_PIPELINE_COMPILE_REQUIRED as it
       * is not obvious what error should be report upon 2 different failures.
       */
      if (result != VK_PIPELINE_COMPILE_REQUIRED)
         return result;

      const VkPipelineCreateFlags2KHR flags =
         vk_compute_pipeline_create_flags(&pCreateInfos[i]);
      if (flags & VK_PIPELINE_CREATE_2_EARLY_RETURN_ON_FAILURE_BIT_KHR)
         return result;
   }

   return first_error_or_success;
}

void
vk_cmd_unbind_pipelines_for_stages(struct vk_command_buffer *cmd_buffer,
                                   VkShaderStageFlags stages)
{
   stages &= cmd_buffer->pipeline_shader_stages;

   if (stages & ~VK_SHADER_STAGE_COMPUTE_BIT)
      vk_graphics_pipeline_cmd_bind(cmd_buffer, NULL);

   if (stages & VK_SHADER_STAGE_COMPUTE_BIT)
      vk_compute_pipeline_cmd_bind(cmd_buffer, NULL);
}
