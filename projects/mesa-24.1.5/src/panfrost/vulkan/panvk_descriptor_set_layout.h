/*
 * Copyright © 2021 Collabora Ltd.
 * SPDX-License-Identifier: MIT
 */

#ifndef PANVK_DESCRIPTOR_SET_LAYOUT_H
#define PANVK_DESCRIPTOR_SET_LAYOUT_H

#ifndef PAN_ARCH
#error "PAN_ARCH must be defined"
#endif

#include <stdint.h>

#include "vk_descriptor_set_layout.h"

struct panvk_descriptor_set_binding_layout {
   VkDescriptorType type;

   /* Number of array elements in this binding */
   unsigned array_size;

   /* Indices in the desc arrays */
   union {
      struct {
         union {
            unsigned sampler_idx;
            unsigned img_idx;
         };
         unsigned tex_idx;
      };
      unsigned dyn_ssbo_idx;
      unsigned ubo_idx;
      unsigned dyn_ubo_idx;
   };

   /* Offset into the descriptor UBO where this binding starts */
   uint32_t desc_ubo_offset;

   /* Stride between descriptors in this binding in the UBO */
   uint16_t desc_ubo_stride;

   /* Shader stages affected by this set+binding */
   uint16_t shader_stages;

   struct panvk_sampler **immutable_samplers;
};

struct panvk_descriptor_set_layout {
   struct vk_descriptor_set_layout vk;
   VkDescriptorSetLayoutCreateFlags flags;

   /* Shader stages affected by this descriptor set */
   uint16_t shader_stages;

   unsigned num_samplers;
   unsigned num_textures;
   unsigned num_ubos;
   unsigned num_dyn_ubos;
   unsigned num_dyn_ssbos;
   unsigned num_imgs;

   /* Size of the descriptor UBO */
   uint32_t desc_ubo_size;

   /* Index of the descriptor UBO */
   unsigned desc_ubo_index;

   /* Number of bindings in this descriptor set */
   uint32_t binding_count;

   /* Bindings in this descriptor set */
   struct panvk_descriptor_set_binding_layout bindings[0];
};

VK_DEFINE_NONDISP_HANDLE_CASTS(panvk_descriptor_set_layout, vk.base,
                               VkDescriptorSetLayout,
                               VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT)

static inline const struct panvk_descriptor_set_layout *
vk_to_panvk_descriptor_set_layout(const struct vk_descriptor_set_layout *layout)
{
   return container_of(layout, const struct panvk_descriptor_set_layout, vk);
}

#endif
