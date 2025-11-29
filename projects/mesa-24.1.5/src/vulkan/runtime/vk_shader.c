/*
 * Copyright © 2024 Collabora, Ltd.
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

#include "vk_shader.h"

#include "vk_alloc.h"
#include "vk_command_buffer.h"
#include "vk_common_entrypoints.h"
#include "vk_descriptor_set_layout.h"
#include "vk_device.h"
#include "vk_nir.h"
#include "vk_physical_device.h"
#include "vk_pipeline.h"

#include "util/mesa-sha1.h"

void *
vk_shader_zalloc(struct vk_device *device,
                 const struct vk_shader_ops *ops,
                 gl_shader_stage stage,
                 const VkAllocationCallbacks *alloc,
                 size_t size)
{
   /* For internal allocations, we need to allocate from the device scope
    * because they might be put in pipeline caches.  Importantly, it is
    * impossible for the client to get at this pointer and we apply this
    * heuristic before we account for allocation fallbacks so this will only
    * ever happen for internal shader objectx.
    */
   const VkSystemAllocationScope alloc_scope =
      alloc == &device->alloc ? VK_SYSTEM_ALLOCATION_SCOPE_DEVICE
                              : VK_SYSTEM_ALLOCATION_SCOPE_OBJECT;

   struct vk_shader *shader = vk_zalloc2(&device->alloc, alloc, size, 8,
                                         alloc_scope);
   if (shader == NULL)
      return NULL;

   vk_object_base_init(device, &shader->base, VK_OBJECT_TYPE_SHADER_EXT);
   shader->ops = ops;
   shader->stage = stage;

   return shader;
}

void
vk_shader_free(struct vk_device *device,
               const VkAllocationCallbacks *alloc,
               struct vk_shader *shader)
{
   vk_object_base_finish(&shader->base);
   vk_free2(&device->alloc, alloc, shader);
}

int
vk_shader_cmp_graphics_stages(gl_shader_stage a, gl_shader_stage b)
{
   static const int stage_order[MESA_SHADER_MESH + 1] = {
      [MESA_SHADER_VERTEX] = 1,
      [MESA_SHADER_TESS_CTRL] = 2,
      [MESA_SHADER_TESS_EVAL] = 3,
      [MESA_SHADER_GEOMETRY] = 4,
      [MESA_SHADER_TASK] = 5,
      [MESA_SHADER_MESH] = 6,
      [MESA_SHADER_FRAGMENT] = 7,
   };

   assert(a < ARRAY_SIZE(stage_order) && stage_order[a] > 0);
   assert(b < ARRAY_SIZE(stage_order) && stage_order[b] > 0);

   return stage_order[a] - stage_order[b];
}

struct stage_idx {
   gl_shader_stage stage;
   uint32_t idx;
};

static int
cmp_stage_idx(const void *_a, const void *_b)
{
   const struct stage_idx *a = _a, *b = _b;
   return vk_shader_cmp_graphics_stages(a->stage, b->stage);
}

static nir_shader *
vk_shader_to_nir(struct vk_device *device,
                 const VkShaderCreateInfoEXT *info,
                 const struct vk_pipeline_robustness_state *rs)
{
   const struct vk_device_shader_ops *ops = device->shader_ops;

   const gl_shader_stage stage = vk_to_mesa_shader_stage(info->stage);
   const nir_shader_compiler_options *nir_options =
      ops->get_nir_options(device->physical, stage, rs);
   struct spirv_to_nir_options spirv_options =
      ops->get_spirv_options(device->physical, stage, rs);

   enum gl_subgroup_size subgroup_size = vk_get_subgroup_size(
      vk_spirv_version(info->pCode, info->codeSize),
      stage, info->pNext,
      info->flags & VK_SHADER_CREATE_ALLOW_VARYING_SUBGROUP_SIZE_BIT_EXT,
      info->flags &VK_SHADER_CREATE_REQUIRE_FULL_SUBGROUPS_BIT_EXT);

   nir_shader *nir = vk_spirv_to_nir(device,
                                     info->pCode, info->codeSize,
                                     stage, info->pName,
                                     subgroup_size,
                                     info->pSpecializationInfo,
                                     &spirv_options, nir_options,
                                     false /* internal */, NULL);
   if (nir == NULL)
      return NULL;

   if (ops->preprocess_nir != NULL)
      ops->preprocess_nir(device->physical, nir);

   return nir;
}

struct set_layouts {
   struct vk_descriptor_set_layout *set_layouts[MESA_VK_MAX_DESCRIPTOR_SETS];
};

static void
vk_shader_compile_info_init(struct vk_shader_compile_info *info,
                            struct set_layouts *set_layouts,
                            const VkShaderCreateInfoEXT *vk_info,
                            const struct vk_pipeline_robustness_state *rs,
                            nir_shader *nir)
{
   for (uint32_t sl = 0; sl < vk_info->setLayoutCount; sl++) {
      set_layouts->set_layouts[sl] =
         vk_descriptor_set_layout_from_handle(vk_info->pSetLayouts[sl]);
   }

   *info = (struct vk_shader_compile_info) {
      .stage = nir->info.stage,
      .flags = vk_info->flags,
      .next_stage_mask = vk_info->nextStage,
      .nir = nir,
      .robustness = rs,
      .set_layout_count = vk_info->setLayoutCount,
      .set_layouts = set_layouts->set_layouts,
      .push_constant_range_count = vk_info->pushConstantRangeCount,
      .push_constant_ranges = vk_info->pPushConstantRanges,
   };
}

PRAGMA_DIAGNOSTIC_PUSH
PRAGMA_DIAGNOSTIC_ERROR(-Wpadded)
struct vk_shader_bin_header {
   char mesavkshaderbin[16];
   VkDriverId driver_id;
   uint8_t uuid[VK_UUID_SIZE];
   uint32_t version;
   uint64_t size;
   uint8_t sha1[SHA1_DIGEST_LENGTH];
   uint32_t _pad;
};
PRAGMA_DIAGNOSTIC_POP
static_assert(sizeof(struct vk_shader_bin_header) == 72,
              "This struct has no holes");

static void
vk_shader_bin_header_init(struct vk_shader_bin_header *header,
                          struct vk_physical_device *device)
{
   *header = (struct vk_shader_bin_header) {
      .mesavkshaderbin = "MesaVkShaderBin",
      .driver_id = device->properties.driverID,
   };

   memcpy(header->uuid, device->properties.shaderBinaryUUID, VK_UUID_SIZE);
   header->version = device->properties.shaderBinaryVersion;
}

static VkResult
vk_shader_serialize(struct vk_device *device,
                    struct vk_shader *shader,
                    struct blob *blob)
{
   struct vk_shader_bin_header header;
   vk_shader_bin_header_init(&header, device->physical);

   ASSERTED intptr_t header_offset = blob_reserve_bytes(blob, sizeof(header));
   assert(header_offset == 0);

   bool success = shader->ops->serialize(device, shader, blob);
   if (!success || blob->out_of_memory)
      return VK_INCOMPLETE;

   /* Finalize and write the header */
   header.size = blob->size;
   if (blob->data != NULL) {
      assert(sizeof(header) <= blob->size);

      struct mesa_sha1 sha1_ctx;
      _mesa_sha1_init(&sha1_ctx);

      /* Hash the header with a zero SHA1 */
      _mesa_sha1_update(&sha1_ctx, &header, sizeof(header));

      /* Hash the serialized data */
      _mesa_sha1_update(&sha1_ctx, blob->data + sizeof(header),
                        blob->size - sizeof(header));

      _mesa_sha1_final(&sha1_ctx, header.sha1);

      blob_overwrite_bytes(blob, header_offset, &header, sizeof(header));
   }

   return VK_SUCCESS;
}

static VkResult
vk_shader_deserialize(struct vk_device *device,
                      size_t data_size, const void *data,
                      const VkAllocationCallbacks* pAllocator,
                      struct vk_shader **shader_out)
{
   const struct vk_device_shader_ops *ops = device->shader_ops;

   struct blob_reader blob;
   blob_reader_init(&blob, data, data_size);

   struct vk_shader_bin_header header, ref_header;
   blob_copy_bytes(&blob, &header, sizeof(header));
   if (blob.overrun)
      return vk_error(device, VK_ERROR_INCOMPATIBLE_SHADER_BINARY_EXT);

   vk_shader_bin_header_init(&ref_header, device->physical);

   if (memcmp(header.mesavkshaderbin, ref_header.mesavkshaderbin,
              sizeof(header.mesavkshaderbin)))
      return vk_error(device, VK_ERROR_INCOMPATIBLE_SHADER_BINARY_EXT);

   if (header.driver_id != ref_header.driver_id)
      return vk_error(device, VK_ERROR_INCOMPATIBLE_SHADER_BINARY_EXT);

   if (memcmp(header.uuid, ref_header.uuid, sizeof(header.uuid)))
      return vk_error(device, VK_ERROR_INCOMPATIBLE_SHADER_BINARY_EXT);

   /* From the Vulkan 1.3.276 spec:
    *
    *    "Guaranteed compatibility of shader binaries is expressed through a
    *    combination of the shaderBinaryUUID and shaderBinaryVersion members
    *    of the VkPhysicalDeviceShaderObjectPropertiesEXT structure queried
    *    from a physical device. Binary shaders retrieved from a physical
    *    device with a certain shaderBinaryUUID are guaranteed to be
    *    compatible with all other physical devices reporting the same
    *    shaderBinaryUUID and the same or higher shaderBinaryVersion."
    *
    * We handle the version check here on behalf of the driver and then pass
    * the version into the driver's deserialize callback.
    *
    * If a driver doesn't want to mess with versions, they can always make the
    * UUID a hash and always report version 0 and that will make this check
    * effectively a no-op.
    */
   if (header.version > ref_header.version)
      return vk_error(device, VK_ERROR_INCOMPATIBLE_SHADER_BINARY_EXT);

   /* Reject shader binaries that are the wrong size. */
   if (header.size != data_size)
      return vk_error(device, VK_ERROR_INCOMPATIBLE_SHADER_BINARY_EXT);

   assert(blob.current == (uint8_t *)data + sizeof(header));
   blob.end = (uint8_t *)data + data_size;

   struct mesa_sha1 sha1_ctx;
   _mesa_sha1_init(&sha1_ctx);

   /* Hash the header with a zero SHA1 */
   struct vk_shader_bin_header sha1_header = header;
   memset(sha1_header.sha1, 0, sizeof(sha1_header.sha1));
   _mesa_sha1_update(&sha1_ctx, &sha1_header, sizeof(sha1_header));

   /* Hash the serialized data */
   _mesa_sha1_update(&sha1_ctx, (uint8_t *)data + sizeof(header),
                     data_size - sizeof(header));

   _mesa_sha1_final(&sha1_ctx, ref_header.sha1);
   if (memcmp(header.sha1, ref_header.sha1, sizeof(header.sha1)))
      return vk_error(device, VK_ERROR_INCOMPATIBLE_SHADER_BINARY_EXT);

   /* We've now verified that the header matches and that the data has the
    * right SHA1 hash so it's safe to call into the driver.
    */
   return ops->deserialize(device, &blob, header.version,
                           pAllocator, shader_out);
}

VKAPI_ATTR VkResult VKAPI_CALL
vk_common_GetShaderBinaryDataEXT(VkDevice _device,
                                 VkShaderEXT _shader,
                                 size_t *pDataSize,
                                 void *pData)
{
   VK_FROM_HANDLE(vk_device, device, _device);
   VK_FROM_HANDLE(vk_shader, shader, _shader);
   VkResult result;

   /* From the Vulkan 1.3.275 spec:
    *
    *    "If pData is NULL, then the size of the binary shader code of the
    *    shader object, in bytes, is returned in pDataSize. Otherwise,
    *    pDataSize must point to a variable set by the user to the size of the
    *    buffer, in bytes, pointed to by pData, and on return the variable is
    *    overwritten with the amount of data actually written to pData. If
    *    pDataSize is less than the size of the binary shader code, nothing is
    *    written to pData, and VK_INCOMPLETE will be returned instead of
    *    VK_SUCCESS."
    *
    * This is annoying.  Unlike basically every other Vulkan data return
    * method, we're not allowed to overwrite the client-provided memory region
    * on VK_INCOMPLETE.  This means we either need to query the blob size
    * up-front by serializing twice or we need to serialize into temporary
    * memory and memcpy into the client-provided region.  We choose the first
    * approach.
    *
    * In the common case, this means that vk_shader_ops::serialize will get
    * called 3 times: Once for the client to get the size, once for us to
    * validate the client's size, and once to actually write the data.  It's a
    * bit heavy-weight but this shouldn't be in a hot path and this is better
    * for memory efficiency.  Also, the vk_shader_ops::serialize should be
    * pretty fast on a null blob.
    */
   struct blob blob;
   blob_init_fixed(&blob, NULL, SIZE_MAX);
   result = vk_shader_serialize(device, shader, &blob);
   assert(result == VK_SUCCESS);

   if (result != VK_SUCCESS) {
      *pDataSize = 0;
      return result;
   } else if (pData == NULL) {
      *pDataSize = blob.size;
      return VK_SUCCESS;
   } else if (blob.size > *pDataSize) {
      /* No data written */
      *pDataSize = 0;
      return VK_INCOMPLETE;
   }

   blob_init_fixed(&blob, pData, *pDataSize);
   result = vk_shader_serialize(device, shader, &blob);
   assert(result == VK_SUCCESS);

   *pDataSize = blob.size;

   return result;
}

/* The only place where we have "real" linking is graphics shaders and there
 * is a limit as to how many of them can be linked together at one time.
 */
#define VK_MAX_LINKED_SHADER_STAGES MESA_VK_MAX_GRAPHICS_PIPELINE_STAGES

VKAPI_ATTR VkResult VKAPI_CALL
vk_common_CreateShadersEXT(VkDevice _device,
                           uint32_t createInfoCount,
                           const VkShaderCreateInfoEXT *pCreateInfos,
                           const VkAllocationCallbacks *pAllocator,
                           VkShaderEXT *pShaders)
{
   VK_FROM_HANDLE(vk_device, device, _device);
   const struct vk_device_shader_ops *ops = device->shader_ops;
   VkResult first_fail_or_success = VK_SUCCESS;

   struct vk_pipeline_robustness_state rs = {
      .storage_buffers = VK_PIPELINE_ROBUSTNESS_BUFFER_BEHAVIOR_DISABLED_EXT,
      .uniform_buffers = VK_PIPELINE_ROBUSTNESS_BUFFER_BEHAVIOR_DISABLED_EXT,
      .vertex_inputs = VK_PIPELINE_ROBUSTNESS_BUFFER_BEHAVIOR_DISABLED_EXT,
      .images = VK_PIPELINE_ROBUSTNESS_IMAGE_BEHAVIOR_DISABLED_EXT,
   };

   /* From the Vulkan 1.3.274 spec:
    *
    *    "When this function returns, whether or not it succeeds, it is
    *    guaranteed that every element of pShaders will have been overwritten
    *    by either VK_NULL_HANDLE or a valid VkShaderEXT handle."
    *
    * Zeroing up-front makes the error path easier.
    */
   memset(pShaders, 0, createInfoCount * sizeof(*pShaders));

   bool has_linked_spirv = false;
   for (uint32_t i = 0; i < createInfoCount; i++) {
      if (pCreateInfos[i].codeType == VK_SHADER_CODE_TYPE_SPIRV_EXT &&
          (pCreateInfos[i].flags & VK_SHADER_CREATE_LINK_STAGE_BIT_EXT))
         has_linked_spirv = true;
   }

   uint32_t linked_count = 0;
   struct stage_idx linked[VK_MAX_LINKED_SHADER_STAGES];

   for (uint32_t i = 0; i < createInfoCount; i++) {
      const VkShaderCreateInfoEXT *vk_info = &pCreateInfos[i];
      VkResult result = VK_SUCCESS;

      switch (vk_info->codeType) {
      case VK_SHADER_CODE_TYPE_BINARY_EXT: {
         /* This isn't required by Vulkan but we're allowed to fail binary
          * import for basically any reason.  This seems like a pretty good
          * reason.
          */
         if (has_linked_spirv &&
             (vk_info->flags & VK_SHADER_CREATE_LINK_STAGE_BIT_EXT)) {
            result = vk_errorf(device, VK_ERROR_INCOMPATIBLE_SHADER_BINARY_EXT,
                               "Cannot mix linked binary and SPIR-V");
            break;
         }

         struct vk_shader *shader;
         result = vk_shader_deserialize(device, vk_info->codeSize,
                                        vk_info->pCode, pAllocator,
                                        &shader);
         if (result != VK_SUCCESS)
            break;

         pShaders[i] = vk_shader_to_handle(shader);
         break;
      }

      case VK_SHADER_CODE_TYPE_SPIRV_EXT: {
         if (vk_info->flags & VK_SHADER_CREATE_LINK_STAGE_BIT_EXT) {
            /* Stash it and compile later */
            assert(linked_count < ARRAY_SIZE(linked));
            linked[linked_count++] = (struct stage_idx) {
               .stage = vk_to_mesa_shader_stage(vk_info->stage),
               .idx = i,
            };
         } else {
            nir_shader *nir = vk_shader_to_nir(device, vk_info, &rs);
            if (nir == NULL) {
               result = vk_errorf(device, VK_ERROR_UNKNOWN,
                                  "Failed to compile shader to NIR");
               break;
            }

            struct vk_shader_compile_info info;
            struct set_layouts set_layouts;
            vk_shader_compile_info_init(&info, &set_layouts,
                                        vk_info, &rs, nir);

            struct vk_shader *shader;
            result = ops->compile(device, 1, &info, NULL /* state */,
                                  pAllocator, &shader);
            if (result != VK_SUCCESS)
               break;

            pShaders[i] = vk_shader_to_handle(shader);
         }
         break;
      }

      default:
         unreachable("Unknown shader code type");
      }

      if (first_fail_or_success == VK_SUCCESS)
         first_fail_or_success = result;
   }

   if (linked_count > 0) {
      struct set_layouts set_layouts[VK_MAX_LINKED_SHADER_STAGES];
      struct vk_shader_compile_info infos[VK_MAX_LINKED_SHADER_STAGES];
      VkResult result = VK_SUCCESS;

      /* Sort so we guarantee the driver always gets them in-order */
      qsort(linked, linked_count, sizeof(*linked), cmp_stage_idx);

      /* Memset for easy error handling */
      memset(infos, 0, sizeof(infos));

      for (uint32_t l = 0; l < linked_count; l++) {
         const VkShaderCreateInfoEXT *vk_info = &pCreateInfos[linked[l].idx];

         nir_shader *nir = vk_shader_to_nir(device, vk_info, &rs);
         if (nir == NULL) {
            result = vk_errorf(device, VK_ERROR_UNKNOWN,
                               "Failed to compile shader to NIR");
            break;
         }

         vk_shader_compile_info_init(&infos[l], &set_layouts[l],
                                     vk_info, &rs, nir);
      }

      if (result == VK_SUCCESS) {
         struct vk_shader *shaders[VK_MAX_LINKED_SHADER_STAGES];

         result = ops->compile(device, linked_count, infos, NULL /* state */,
                               pAllocator, shaders);
         if (result == VK_SUCCESS) {
            for (uint32_t l = 0; l < linked_count; l++)
               pShaders[linked[l].idx] = vk_shader_to_handle(shaders[l]);
         }
      } else {
         for (uint32_t l = 0; l < linked_count; l++) {
            if (infos[l].nir != NULL)
               ralloc_free(infos[l].nir);
         }
      }

      if (first_fail_or_success == VK_SUCCESS)
         first_fail_or_success = result;
   }

   return first_fail_or_success;
}

VKAPI_ATTR void VKAPI_CALL
vk_common_DestroyShaderEXT(VkDevice _device,
                           VkShaderEXT _shader,
                           const VkAllocationCallbacks *pAllocator)
{
   VK_FROM_HANDLE(vk_device, device, _device);
   VK_FROM_HANDLE(vk_shader, shader, _shader);

   if (shader == NULL)
      return;

   vk_shader_destroy(device, shader, pAllocator);
}

VKAPI_ATTR void VKAPI_CALL
vk_common_CmdBindShadersEXT(VkCommandBuffer commandBuffer,
                            uint32_t stageCount,
                            const VkShaderStageFlagBits *pStages,
                            const VkShaderEXT *pShaders)
{
   VK_FROM_HANDLE(vk_command_buffer, cmd_buffer, commandBuffer);
   struct vk_device *device = cmd_buffer->base.device;
   const struct vk_device_shader_ops *ops = device->shader_ops;

   STACK_ARRAY(gl_shader_stage, stages, stageCount);
   STACK_ARRAY(struct vk_shader *, shaders, stageCount);

   VkShaderStageFlags vk_stages = 0;
   for (uint32_t i = 0; i < stageCount; i++) {
      vk_stages |= pStages[i];
      stages[i] = vk_to_mesa_shader_stage(pStages[i]);
      shaders[i] = pShaders != NULL ? vk_shader_from_handle(pShaders[i]) : NULL;
   }

   vk_cmd_unbind_pipelines_for_stages(cmd_buffer, vk_stages);
   if (vk_stages & ~VK_SHADER_STAGE_COMPUTE_BIT)
      vk_cmd_set_rp_attachments(cmd_buffer, ~0);

   ops->cmd_bind_shaders(cmd_buffer, stageCount, stages, shaders);
}
