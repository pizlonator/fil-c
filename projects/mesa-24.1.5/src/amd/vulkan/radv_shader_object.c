/*
 * Copyright © 2024 Valve Corporation
 *
 * SPDX-License-Identifier: MIT
 */

#include "vk_log.h"

#include "radv_device.h"
#include "radv_entrypoints.h"
#include "radv_physical_device.h"
#include "radv_pipeline_cache.h"
#include "radv_pipeline_compute.h"
#include "radv_pipeline_graphics.h"
#include "radv_shader_object.h"

static void
radv_shader_object_destroy_variant(struct radv_device *device, VkShaderCodeTypeEXT code_type,
                                   struct radv_shader *shader, struct radv_shader_binary *binary)
{
   if (shader)
      radv_shader_unref(device, shader);

   if (code_type == VK_SHADER_CODE_TYPE_SPIRV_EXT)
      free(binary);
}

static void
radv_shader_object_destroy(struct radv_device *device, struct radv_shader_object *shader_obj,
                           const VkAllocationCallbacks *pAllocator)
{
   radv_shader_object_destroy_variant(device, shader_obj->code_type, shader_obj->as_ls.shader,
                                      shader_obj->as_ls.binary);
   radv_shader_object_destroy_variant(device, shader_obj->code_type, shader_obj->as_es.shader,
                                      shader_obj->as_es.binary);
   radv_shader_object_destroy_variant(device, shader_obj->code_type, shader_obj->gs.copy_shader,
                                      shader_obj->gs.copy_binary);
   radv_shader_object_destroy_variant(device, shader_obj->code_type, shader_obj->shader, shader_obj->binary);

   vk_object_base_finish(&shader_obj->base);
   vk_free2(&device->vk.alloc, pAllocator, shader_obj);
}

VKAPI_ATTR void VKAPI_CALL
radv_DestroyShaderEXT(VkDevice _device, VkShaderEXT shader, const VkAllocationCallbacks *pAllocator)
{
   VK_FROM_HANDLE(radv_device, device, _device);
   VK_FROM_HANDLE(radv_shader_object, shader_obj, shader);

   if (!shader)
      return;

   radv_shader_object_destroy(device, shader_obj, pAllocator);
}

static void
radv_shader_stage_init(const VkShaderCreateInfoEXT *sinfo, struct radv_shader_stage *out_stage)
{
   uint16_t dynamic_shader_stages = 0;

   memset(out_stage, 0, sizeof(*out_stage));

   out_stage->stage = vk_to_mesa_shader_stage(sinfo->stage);
   out_stage->next_stage = MESA_SHADER_NONE;
   out_stage->entrypoint = sinfo->pName;
   out_stage->spec_info = sinfo->pSpecializationInfo;
   out_stage->feedback.flags = VK_PIPELINE_CREATION_FEEDBACK_VALID_BIT;
   out_stage->spirv.data = (const char *)sinfo->pCode;
   out_stage->spirv.size = sinfo->codeSize;

   for (uint32_t i = 0; i < sinfo->setLayoutCount; i++) {
      VK_FROM_HANDLE(radv_descriptor_set_layout, set_layout, sinfo->pSetLayouts[i]);

      if (set_layout == NULL)
         continue;

      out_stage->layout.num_sets = MAX2(i + 1, out_stage->layout.num_sets);
      out_stage->layout.set[i].layout = set_layout;

      out_stage->layout.set[i].dynamic_offset_start = out_stage->layout.dynamic_offset_count;
      out_stage->layout.dynamic_offset_count += set_layout->dynamic_offset_count;

      dynamic_shader_stages |= set_layout->dynamic_shader_stages;
   }

   if (out_stage->layout.dynamic_offset_count && (dynamic_shader_stages & sinfo->stage)) {
      out_stage->layout.use_dynamic_descriptors = true;
   }

   for (unsigned i = 0; i < sinfo->pushConstantRangeCount; ++i) {
      const VkPushConstantRange *range = sinfo->pPushConstantRanges + i;
      out_stage->layout.push_constant_size = MAX2(out_stage->layout.push_constant_size, range->offset + range->size);
   }

   out_stage->layout.push_constant_size = align(out_stage->layout.push_constant_size, 16);

   const VkShaderRequiredSubgroupSizeCreateInfoEXT *const subgroup_size =
      vk_find_struct_const(sinfo->pNext, SHADER_REQUIRED_SUBGROUP_SIZE_CREATE_INFO_EXT);

   if (subgroup_size) {
      if (subgroup_size->requiredSubgroupSize == 32)
         out_stage->key.subgroup_required_size = RADV_REQUIRED_WAVE32;
      else if (subgroup_size->requiredSubgroupSize == 64)
         out_stage->key.subgroup_required_size = RADV_REQUIRED_WAVE64;
      else
         unreachable("Unsupported required subgroup size.");
   }

   if (sinfo->flags & VK_SHADER_CREATE_REQUIRE_FULL_SUBGROUPS_BIT_EXT) {
      out_stage->key.subgroup_require_full = 1;
   }

   if (out_stage->stage == MESA_SHADER_MESH) {
      out_stage->key.has_task_shader = !(sinfo->flags & VK_SHADER_CREATE_NO_TASK_SHADER_BIT_EXT);
   }
}

static VkResult
radv_shader_object_init_graphics(struct radv_shader_object *shader_obj, struct radv_device *device,
                                 const VkShaderCreateInfoEXT *pCreateInfo)
{
   const struct radv_physical_device *pdev = radv_device_physical(device);
   gl_shader_stage stage = vk_to_mesa_shader_stage(pCreateInfo->stage);
   struct radv_shader_stage stages[MESA_VULKAN_SHADER_STAGES];

   for (unsigned i = 0; i < MESA_VULKAN_SHADER_STAGES; i++) {
      stages[i].entrypoint = NULL;
      stages[i].nir = NULL;
      stages[i].spirv.size = 0;
      stages[i].next_stage = MESA_SHADER_NONE;
   }

   radv_shader_stage_init(pCreateInfo, &stages[stage]);

   struct radv_graphics_state_key gfx_state = {0};

   gfx_state.vs.has_prolog = true;
   gfx_state.ps.has_epilog = true;
   gfx_state.dynamic_rasterization_samples = true;
   gfx_state.unknown_rast_prim = true;
   gfx_state.dynamic_provoking_vtx_mode = true;
   gfx_state.dynamic_line_rast_mode = true;

   if (pdev->info.gfx_level >= GFX11)
      gfx_state.ps.exports_mrtz_via_epilog = true;

   struct radv_shader *shader = NULL;
   struct radv_shader_binary *binary = NULL;

   VkShaderStageFlags next_stages = pCreateInfo->nextStage;
   if (!next_stages) {
      /* When next stage is 0, gather all valid next stages. */
      switch (pCreateInfo->stage) {
      case VK_SHADER_STAGE_VERTEX_BIT:
         next_stages |=
            VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT | VK_SHADER_STAGE_GEOMETRY_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
         break;
      case VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT:
         next_stages |= VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;
         break;
      case VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT:
         next_stages |= VK_SHADER_STAGE_GEOMETRY_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
         break;
      case VK_SHADER_STAGE_GEOMETRY_BIT:
      case VK_SHADER_STAGE_MESH_BIT_EXT:
         next_stages |= VK_SHADER_STAGE_FRAGMENT_BIT;
         break;
      case VK_SHADER_STAGE_TASK_BIT_EXT:
         next_stages |= VK_SHADER_STAGE_MESH_BIT_EXT;
         break;
      case VK_SHADER_STAGE_FRAGMENT_BIT:
      case VK_SHADER_STAGE_COMPUTE_BIT:
         break;
      default:
         unreachable("Invalid shader stage");
      }
   }

   if (!next_stages) {
      struct radv_shader *shaders[MESA_VULKAN_SHADER_STAGES] = {NULL};
      struct radv_shader_binary *binaries[MESA_VULKAN_SHADER_STAGES] = {NULL};

      radv_graphics_shaders_compile(device, NULL, stages, &gfx_state, true, false, false, NULL, false, shaders,
                                    binaries, &shader_obj->gs.copy_shader, &shader_obj->gs.copy_binary);

      shader = shaders[stage];
      binary = binaries[stage];

      ralloc_free(stages[stage].nir);

      shader_obj->shader = shader;
      shader_obj->binary = binary;
   } else {
      radv_foreach_stage(next_stage, next_stages)
      {
         struct radv_shader *shaders[MESA_VULKAN_SHADER_STAGES] = {NULL};
         struct radv_shader_binary *binaries[MESA_VULKAN_SHADER_STAGES] = {NULL};

         radv_shader_stage_init(pCreateInfo, &stages[stage]);
         stages[stage].next_stage = next_stage;

         radv_graphics_shaders_compile(device, NULL, stages, &gfx_state, true, false, false, NULL, false, shaders,
                                       binaries, &shader_obj->gs.copy_shader, &shader_obj->gs.copy_binary);

         shader = shaders[stage];
         binary = binaries[stage];

         ralloc_free(stages[stage].nir);

         if (stage == MESA_SHADER_VERTEX) {
            if (next_stage == MESA_SHADER_TESS_CTRL) {
               shader_obj->as_ls.shader = shader;
               shader_obj->as_ls.binary = binary;
            } else if (next_stage == MESA_SHADER_GEOMETRY) {
               shader_obj->as_es.shader = shader;
               shader_obj->as_es.binary = binary;
            } else {
               shader_obj->shader = shader;
               shader_obj->binary = binary;
            }
         } else if (stage == MESA_SHADER_TESS_EVAL) {
            if (next_stage == MESA_SHADER_GEOMETRY) {
               shader_obj->as_es.shader = shader;
               shader_obj->as_es.binary = binary;
            } else {
               shader_obj->shader = shader;
               shader_obj->binary = binary;
            }
         } else {
            shader_obj->shader = shader;
            shader_obj->binary = binary;
         }
      }
   }

   return VK_SUCCESS;
}

static VkResult
radv_shader_object_init_compute(struct radv_shader_object *shader_obj, struct radv_device *device,
                                const VkShaderCreateInfoEXT *pCreateInfo)
{
   struct radv_shader_binary *cs_binary;
   struct radv_shader_stage stage = {0};

   radv_shader_stage_init(pCreateInfo, &stage);

   struct radv_shader *cs_shader = radv_compile_cs(device, NULL, &stage, true, false, false, &cs_binary);

   ralloc_free(stage.nir);

   shader_obj->shader = cs_shader;
   shader_obj->binary = cs_binary;

   return VK_SUCCESS;
}

static void
radv_get_shader_layout(const VkShaderCreateInfoEXT *pCreateInfo, struct radv_shader_layout *layout)
{
   uint16_t dynamic_shader_stages = 0;

   memset(layout, 0, sizeof(*layout));

   layout->dynamic_offset_count = 0;

   for (uint32_t i = 0; i < pCreateInfo->setLayoutCount; i++) {
      VK_FROM_HANDLE(radv_descriptor_set_layout, set_layout, pCreateInfo->pSetLayouts[i]);

      if (set_layout == NULL)
         continue;

      layout->num_sets = MAX2(i + 1, layout->num_sets);

      layout->set[i].layout = set_layout;
      layout->set[i].dynamic_offset_start = layout->dynamic_offset_count;

      layout->dynamic_offset_count += set_layout->dynamic_offset_count;
      dynamic_shader_stages |= set_layout->dynamic_shader_stages;
   }

   if (layout->dynamic_offset_count && (dynamic_shader_stages & pCreateInfo->stage)) {
      layout->use_dynamic_descriptors = true;
   }

   layout->push_constant_size = 0;

   for (unsigned i = 0; i < pCreateInfo->pushConstantRangeCount; ++i) {
      const VkPushConstantRange *range = pCreateInfo->pPushConstantRanges + i;
      layout->push_constant_size = MAX2(layout->push_constant_size, range->offset + range->size);
   }

   layout->push_constant_size = align(layout->push_constant_size, 16);
}

static VkResult
radv_shader_object_init_binary(struct radv_device *device, struct blob_reader *blob, struct radv_shader **shader_out,
                               struct radv_shader_binary **binary_out)
{
   const char *binary_sha1 = blob_read_bytes(blob, SHA1_DIGEST_LENGTH);
   const uint32_t binary_size = blob_read_uint32(blob);
   const struct radv_shader_binary *binary = blob_read_bytes(blob, binary_size);
   unsigned char sha1[SHA1_DIGEST_LENGTH];

   _mesa_sha1_compute(binary, binary->total_size, sha1);
   if (memcmp(sha1, binary_sha1, SHA1_DIGEST_LENGTH))
      return VK_ERROR_INCOMPATIBLE_SHADER_BINARY_EXT;

   *shader_out = radv_shader_create(device, NULL, binary, true);
   *binary_out = (struct radv_shader_binary *)binary;

   return VK_SUCCESS;
}

static VkResult
radv_shader_object_init(struct radv_shader_object *shader_obj, struct radv_device *device,
                        const VkShaderCreateInfoEXT *pCreateInfo)
{
   const struct radv_physical_device *pdev = radv_device_physical(device);
   struct radv_shader_layout layout;
   VkResult result;

   radv_get_shader_layout(pCreateInfo, &layout);

   shader_obj->stage = vk_to_mesa_shader_stage(pCreateInfo->stage);
   shader_obj->code_type = pCreateInfo->codeType;
   shader_obj->push_constant_size = layout.push_constant_size;
   shader_obj->dynamic_offset_count = layout.dynamic_offset_count;

   if (pCreateInfo->codeType == VK_SHADER_CODE_TYPE_BINARY_EXT) {
      if (pCreateInfo->codeSize < VK_UUID_SIZE + sizeof(uint32_t)) {
         return VK_ERROR_INCOMPATIBLE_SHADER_BINARY_EXT;
      }

      struct blob_reader blob;
      blob_reader_init(&blob, pCreateInfo->pCode, pCreateInfo->codeSize);

      const uint8_t *cache_uuid = blob_read_bytes(&blob, VK_UUID_SIZE);

      if (memcmp(cache_uuid, pdev->cache_uuid, VK_UUID_SIZE))
         return VK_ERROR_INCOMPATIBLE_SHADER_BINARY_EXT;

      const bool has_main_binary = blob_read_uint32(&blob);

      if (has_main_binary) {
         result = radv_shader_object_init_binary(device, &blob, &shader_obj->shader, &shader_obj->binary);
         if (result != VK_SUCCESS)
            return result;
      }

      if (shader_obj->stage == MESA_SHADER_VERTEX) {
         const bool has_es_binary = blob_read_uint32(&blob);
         if (has_es_binary) {
            result =
               radv_shader_object_init_binary(device, &blob, &shader_obj->as_es.shader, &shader_obj->as_es.binary);
            if (result != VK_SUCCESS)
               return result;
         }

         const bool has_ls_binary = blob_read_uint32(&blob);
         if (has_ls_binary) {
            result =
               radv_shader_object_init_binary(device, &blob, &shader_obj->as_ls.shader, &shader_obj->as_ls.binary);
            if (result != VK_SUCCESS)
               return result;
         }
      } else if (shader_obj->stage == MESA_SHADER_TESS_EVAL) {
         const bool has_es_binary = blob_read_uint32(&blob);
         if (has_es_binary) {
            result =
               radv_shader_object_init_binary(device, &blob, &shader_obj->as_es.shader, &shader_obj->as_es.binary);
            if (result != VK_SUCCESS)
               return result;
         }
      } else if (shader_obj->stage == MESA_SHADER_GEOMETRY) {
         const bool has_gs_copy_binary = blob_read_uint32(&blob);
         if (has_gs_copy_binary) {
            result =
               radv_shader_object_init_binary(device, &blob, &shader_obj->gs.copy_shader, &shader_obj->gs.copy_binary);
            if (result != VK_SUCCESS)
               return result;
         }
      }
   } else {
      assert(pCreateInfo->codeType == VK_SHADER_CODE_TYPE_SPIRV_EXT);

      if (pCreateInfo->stage == VK_SHADER_STAGE_COMPUTE_BIT) {
         result = radv_shader_object_init_compute(shader_obj, device, pCreateInfo);
      } else {
         result = radv_shader_object_init_graphics(shader_obj, device, pCreateInfo);
      }

      if (result != VK_SUCCESS)
         return result;
   }

   return VK_SUCCESS;
}

static VkResult
radv_shader_object_create(VkDevice _device, const VkShaderCreateInfoEXT *pCreateInfo,
                          const VkAllocationCallbacks *pAllocator, VkShaderEXT *pShader)
{
   VK_FROM_HANDLE(radv_device, device, _device);
   struct radv_shader_object *shader_obj;
   VkResult result;

   shader_obj = vk_zalloc2(&device->vk.alloc, pAllocator, sizeof(*shader_obj), 8, VK_SYSTEM_ALLOCATION_SCOPE_OBJECT);
   if (shader_obj == NULL)
      return vk_error(device, VK_ERROR_OUT_OF_HOST_MEMORY);

   vk_object_base_init(&device->vk, &shader_obj->base, VK_OBJECT_TYPE_SHADER_EXT);

   result = radv_shader_object_init(shader_obj, device, pCreateInfo);
   if (result != VK_SUCCESS) {
      radv_shader_object_destroy(device, shader_obj, pAllocator);
      return result;
   }

   *pShader = radv_shader_object_to_handle(shader_obj);

   return VK_SUCCESS;
}

static VkResult
radv_shader_object_create_linked(VkDevice _device, uint32_t createInfoCount, const VkShaderCreateInfoEXT *pCreateInfos,
                                 const VkAllocationCallbacks *pAllocator, VkShaderEXT *pShaders)
{
   VK_FROM_HANDLE(radv_device, device, _device);
   const struct radv_physical_device *pdev = radv_device_physical(device);
   struct radv_shader_stage stages[MESA_VULKAN_SHADER_STAGES];

   for (unsigned i = 0; i < MESA_VULKAN_SHADER_STAGES; i++) {
      stages[i].entrypoint = NULL;
      stages[i].nir = NULL;
      stages[i].spirv.size = 0;
      stages[i].next_stage = MESA_SHADER_NONE;
   }

   struct radv_graphics_state_key gfx_state = {0};

   gfx_state.vs.has_prolog = true;
   gfx_state.ps.has_epilog = true;
   gfx_state.dynamic_rasterization_samples = true;
   gfx_state.unknown_rast_prim = true;
   gfx_state.dynamic_provoking_vtx_mode = true;
   gfx_state.dynamic_line_rast_mode = true;

   if (pdev->info.gfx_level >= GFX11)
      gfx_state.ps.exports_mrtz_via_epilog = true;

   for (unsigned i = 0; i < createInfoCount; i++) {
      const VkShaderCreateInfoEXT *pCreateInfo = &pCreateInfos[i];
      gl_shader_stage s = vk_to_mesa_shader_stage(pCreateInfo->stage);

      radv_shader_stage_init(pCreateInfo, &stages[s]);
   }

   /* Determine next stage. */
   for (unsigned i = 0; i < MESA_VULKAN_SHADER_STAGES; i++) {
      if (!stages[i].entrypoint)
         continue;

      switch (stages[i].stage) {
      case MESA_SHADER_VERTEX:
         if (stages[MESA_SHADER_TESS_CTRL].entrypoint) {
            stages[i].next_stage = MESA_SHADER_TESS_CTRL;
         } else if (stages[MESA_SHADER_GEOMETRY].entrypoint) {
            stages[i].next_stage = MESA_SHADER_GEOMETRY;
         } else if (stages[MESA_SHADER_FRAGMENT].entrypoint) {
            stages[i].next_stage = MESA_SHADER_FRAGMENT;
         }
         break;
      case MESA_SHADER_TESS_CTRL:
         stages[i].next_stage = MESA_SHADER_TESS_EVAL;
         break;
      case MESA_SHADER_TESS_EVAL:
         if (stages[MESA_SHADER_GEOMETRY].entrypoint) {
            stages[i].next_stage = MESA_SHADER_GEOMETRY;
         } else if (stages[MESA_SHADER_FRAGMENT].entrypoint) {
            stages[i].next_stage = MESA_SHADER_FRAGMENT;
         }
         break;
      case MESA_SHADER_GEOMETRY:
      case MESA_SHADER_MESH:
         if (stages[MESA_SHADER_FRAGMENT].entrypoint) {
            stages[i].next_stage = MESA_SHADER_FRAGMENT;
         }
         break;
      case MESA_SHADER_FRAGMENT:
         stages[i].next_stage = MESA_SHADER_NONE;
         break;
      case MESA_SHADER_TASK:
         stages[i].next_stage = MESA_SHADER_MESH;
         break;
      default:
         assert(0);
      }
   }

   struct radv_shader *shaders[MESA_VULKAN_SHADER_STAGES] = {NULL};
   struct radv_shader_binary *binaries[MESA_VULKAN_SHADER_STAGES] = {NULL};
   struct radv_shader *gs_copy_shader = NULL;
   struct radv_shader_binary *gs_copy_binary = NULL;

   radv_graphics_shaders_compile(device, NULL, stages, &gfx_state, true, false, false, NULL, false, shaders, binaries,
                                 &gs_copy_shader, &gs_copy_binary);

   for (unsigned i = 0; i < createInfoCount; i++) {
      const VkShaderCreateInfoEXT *pCreateInfo = &pCreateInfos[i];
      gl_shader_stage s = vk_to_mesa_shader_stage(pCreateInfo->stage);
      struct radv_shader_object *shader_obj;

      shader_obj = vk_zalloc2(&device->vk.alloc, pAllocator, sizeof(*shader_obj), 8, VK_SYSTEM_ALLOCATION_SCOPE_OBJECT);
      if (shader_obj == NULL)
         return vk_error(device, VK_ERROR_OUT_OF_HOST_MEMORY);

      vk_object_base_init(&device->vk, &shader_obj->base, VK_OBJECT_TYPE_SHADER_EXT);

      shader_obj->stage = s;
      shader_obj->code_type = pCreateInfo->codeType;
      shader_obj->push_constant_size = stages[s].layout.push_constant_size;
      shader_obj->dynamic_offset_count = stages[s].layout.dynamic_offset_count;

      if (s == MESA_SHADER_VERTEX) {
         if (stages[s].next_stage == MESA_SHADER_TESS_CTRL) {
            shader_obj->as_ls.shader = shaders[s];
            shader_obj->as_ls.binary = binaries[s];
         } else if (stages[s].next_stage == MESA_SHADER_GEOMETRY) {
            shader_obj->as_es.shader = shaders[s];
            shader_obj->as_es.binary = binaries[s];
         } else {
            shader_obj->shader = shaders[s];
            shader_obj->binary = binaries[s];
         }
      } else if (s == MESA_SHADER_TESS_EVAL) {
         if (stages[s].next_stage == MESA_SHADER_GEOMETRY) {
            shader_obj->as_es.shader = shaders[s];
            shader_obj->as_es.binary = binaries[s];
         } else {
            shader_obj->shader = shaders[s];
            shader_obj->binary = binaries[s];
         }
      } else {
         shader_obj->shader = shaders[s];
         shader_obj->binary = binaries[s];
      }

      if (s == MESA_SHADER_GEOMETRY) {
         shader_obj->gs.copy_shader = gs_copy_shader;
         shader_obj->gs.copy_binary = gs_copy_binary;
      }

      ralloc_free(stages[s].nir);

      pShaders[i] = radv_shader_object_to_handle(shader_obj);
   }

   return VK_SUCCESS;
}

VKAPI_ATTR VkResult VKAPI_CALL
radv_CreateShadersEXT(VkDevice _device, uint32_t createInfoCount, const VkShaderCreateInfoEXT *pCreateInfos,
                      const VkAllocationCallbacks *pAllocator, VkShaderEXT *pShaders)
{
   VkResult result = VK_SUCCESS;
   unsigned i = 0;

   if (createInfoCount > 1 && !!(pCreateInfos[0].flags & VK_SHADER_CREATE_LINK_STAGE_BIT_EXT) &&
       pCreateInfos[0].codeType == VK_SHADER_CODE_TYPE_SPIRV_EXT) {
      for (unsigned j = 0; j < createInfoCount; j++) {
         assert(pCreateInfos[i].flags & VK_SHADER_CREATE_LINK_STAGE_BIT_EXT);
      }

      return radv_shader_object_create_linked(_device, createInfoCount, pCreateInfos, pAllocator, pShaders);
   }

   for (; i < createInfoCount; i++) {
      VkResult r;

      r = radv_shader_object_create(_device, &pCreateInfos[i], pAllocator, &pShaders[i]);
      if (r != VK_SUCCESS) {
         result = r;
         pShaders[i] = VK_NULL_HANDLE;
      }
   }

   for (; i < createInfoCount; ++i)
      pShaders[i] = VK_NULL_HANDLE;

   return result;
}

static size_t
radv_get_shader_binary_size(const struct radv_shader_binary *binary)
{
   size_t size = sizeof(uint32_t); /* has_binary */

   if (binary)
      size += SHA1_DIGEST_LENGTH + 4 + ALIGN(binary->total_size, 4);

   return size;
}

static size_t
radv_get_shader_object_size(const struct radv_shader_object *shader_obj)
{
   size_t size = VK_UUID_SIZE;

   size += radv_get_shader_binary_size(shader_obj->binary);

   if (shader_obj->stage == MESA_SHADER_VERTEX) {
      size += radv_get_shader_binary_size(shader_obj->as_es.binary);
      size += radv_get_shader_binary_size(shader_obj->as_ls.binary);
   } else if (shader_obj->stage == MESA_SHADER_TESS_EVAL) {
      size += radv_get_shader_binary_size(shader_obj->as_es.binary);
   } else if (shader_obj->stage == MESA_SHADER_GEOMETRY) {
      size += radv_get_shader_binary_size(shader_obj->gs.copy_binary);
   }

   return size;
}

static void
radv_write_shader_binary(struct blob *blob, const struct radv_shader_binary *binary)
{
   unsigned char binary_sha1[SHA1_DIGEST_LENGTH];

   blob_write_uint32(blob, !!binary);

   if (binary) {
      _mesa_sha1_compute(binary, binary->total_size, binary_sha1);

      blob_write_bytes(blob, binary_sha1, sizeof(binary_sha1));
      blob_write_uint32(blob, binary->total_size);
      blob_write_bytes(blob, binary, binary->total_size);
   }
}

VKAPI_ATTR VkResult VKAPI_CALL
radv_GetShaderBinaryDataEXT(VkDevice _device, VkShaderEXT shader, size_t *pDataSize, void *pData)
{
   VK_FROM_HANDLE(radv_device, device, _device);
   VK_FROM_HANDLE(radv_shader_object, shader_obj, shader);
   const struct radv_physical_device *pdev = radv_device_physical(device);
   const size_t size = radv_get_shader_object_size(shader_obj);

   if (!pData) {
      *pDataSize = size;
      return VK_SUCCESS;
   }

   if (*pDataSize < size) {
      *pDataSize = 0;
      return VK_INCOMPLETE;
   }

   struct blob blob;
   blob_init_fixed(&blob, pData, *pDataSize);
   blob_write_bytes(&blob, pdev->cache_uuid, VK_UUID_SIZE);

   radv_write_shader_binary(&blob, shader_obj->binary);

   if (shader_obj->stage == MESA_SHADER_VERTEX) {
      radv_write_shader_binary(&blob, shader_obj->as_es.binary);
      radv_write_shader_binary(&blob, shader_obj->as_ls.binary);
   } else if (shader_obj->stage == MESA_SHADER_TESS_EVAL) {
      radv_write_shader_binary(&blob, shader_obj->as_es.binary);
   } else if (shader_obj->stage == MESA_SHADER_GEOMETRY) {
      radv_write_shader_binary(&blob, shader_obj->gs.copy_binary);
   }

   assert(!blob.out_of_memory);

   return VK_SUCCESS;
}
