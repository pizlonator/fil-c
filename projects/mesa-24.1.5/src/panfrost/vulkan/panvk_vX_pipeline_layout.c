/*
 * Copyright © 2021 Collabora Ltd.
 *
 * SPDX-License-Identifier: MIT
 */

#include "genxml/gen_macros.h"

#include "vk_log.h"

#include "panvk_device.h"
#include "panvk_entrypoints.h"
#include "panvk_macros.h"
#include "panvk_pipeline_layout.h"
#include "panvk_sampler.h"
#include "panvk_shader.h"

#include "util/mesa-sha1.h"

/*
 * Pipeline layouts.  These have nothing to do with the pipeline.  They are
 * just multiple descriptor set layouts pasted together.
 */

VKAPI_ATTR VkResult VKAPI_CALL
panvk_per_arch(CreatePipelineLayout)(
   VkDevice _device, const VkPipelineLayoutCreateInfo *pCreateInfo,
   const VkAllocationCallbacks *pAllocator, VkPipelineLayout *pPipelineLayout)
{
   VK_FROM_HANDLE(panvk_device, device, _device);
   struct panvk_pipeline_layout *layout;
   struct mesa_sha1 ctx;

   layout =
      vk_pipeline_layout_zalloc(&device->vk, sizeof(*layout), pCreateInfo);
   if (layout == NULL)
      return vk_error(device, VK_ERROR_OUT_OF_HOST_MEMORY);

   _mesa_sha1_init(&ctx);

   unsigned sampler_idx = 0, tex_idx = 0, ubo_idx = 0;
   unsigned dyn_ubo_idx = 0, dyn_ssbo_idx = 0, img_idx = 0;
   for (unsigned set = 0; set < pCreateInfo->setLayoutCount; set++) {
      const struct panvk_descriptor_set_layout *set_layout =
         vk_to_panvk_descriptor_set_layout(layout->vk.set_layouts[set]);

      layout->sets[set].sampler_offset = sampler_idx;
      layout->sets[set].tex_offset = tex_idx;
      layout->sets[set].ubo_offset = ubo_idx;
      layout->sets[set].dyn_ubo_offset = dyn_ubo_idx;
      layout->sets[set].dyn_ssbo_offset = dyn_ssbo_idx;
      layout->sets[set].img_offset = img_idx;
      sampler_idx += set_layout->num_samplers;
      tex_idx += set_layout->num_textures;
      ubo_idx += set_layout->num_ubos;
      dyn_ubo_idx += set_layout->num_dyn_ubos;
      dyn_ssbo_idx += set_layout->num_dyn_ssbos;
      img_idx += set_layout->num_imgs;

      for (unsigned b = 0; b < set_layout->binding_count; b++) {
         const struct panvk_descriptor_set_binding_layout *binding_layout =
            &set_layout->bindings[b];

         if (binding_layout->immutable_samplers) {
            for (unsigned s = 0; s < binding_layout->array_size; s++) {
               struct panvk_sampler *sampler =
                  binding_layout->immutable_samplers[s];

               _mesa_sha1_update(&ctx, &sampler->desc, sizeof(sampler->desc));
            }
         }
         _mesa_sha1_update(&ctx, &binding_layout->type,
                           sizeof(binding_layout->type));
         _mesa_sha1_update(&ctx, &binding_layout->array_size,
                           sizeof(binding_layout->array_size));
         _mesa_sha1_update(&ctx, &binding_layout->shader_stages,
                           sizeof(binding_layout->shader_stages));
      }
   }

   for (unsigned range = 0; range < pCreateInfo->pushConstantRangeCount;
        range++) {
      layout->push_constants.size =
         MAX2(pCreateInfo->pPushConstantRanges[range].offset +
                 pCreateInfo->pPushConstantRanges[range].size,
              layout->push_constants.size);
   }

   layout->num_samplers = sampler_idx;
   layout->num_textures = tex_idx;
   layout->num_ubos = ubo_idx;
   layout->num_dyn_ubos = dyn_ubo_idx;
   layout->num_dyn_ssbos = dyn_ssbo_idx;
   layout->num_imgs = img_idx;

   /* Some NIR texture operations don't require a sampler, but Bifrost/Midgard
    * ones always expect one. Add a dummy sampler to deal with this limitation.
    */
   if (layout->num_textures) {
      layout->num_samplers++;
      for (unsigned set = 0; set < pCreateInfo->setLayoutCount; set++)
         layout->sets[set].sampler_offset++;
   }

   _mesa_sha1_final(&ctx, layout->sha1);

   *pPipelineLayout = panvk_pipeline_layout_to_handle(layout);
   return VK_SUCCESS;
}

unsigned
panvk_per_arch(pipeline_layout_ubo_start)(
   const struct panvk_pipeline_layout *layout, unsigned set, bool is_dynamic)
{
   const struct panvk_descriptor_set_layout *set_layout =
      vk_to_panvk_descriptor_set_layout(layout->vk.set_layouts[set]);

   unsigned offset = PANVK_NUM_BUILTIN_UBOS + layout->sets[set].ubo_offset +
                     layout->sets[set].dyn_ubo_offset;

   if (is_dynamic)
      offset += set_layout->num_ubos;

   return offset;
}

unsigned
panvk_per_arch(pipeline_layout_ubo_index)(
   const struct panvk_pipeline_layout *layout, unsigned set, unsigned binding,
   unsigned array_index)
{
   const struct panvk_descriptor_set_layout *set_layout =
      vk_to_panvk_descriptor_set_layout(layout->vk.set_layouts[set]);
   const struct panvk_descriptor_set_binding_layout *binding_layout =
      &set_layout->bindings[binding];

   const bool is_dynamic =
      binding_layout->type == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
   const uint32_t ubo_idx =
      is_dynamic ? binding_layout->dyn_ubo_idx : binding_layout->ubo_idx;

   return panvk_per_arch(pipeline_layout_ubo_start)(layout, set, is_dynamic) +
          ubo_idx + array_index;
}
