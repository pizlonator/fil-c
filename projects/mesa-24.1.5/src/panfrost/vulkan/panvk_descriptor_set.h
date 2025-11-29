/*
 * Copyright © 2021 Collabora Ltd.
 * SPDX-License-Identifier: MIT
 */

#ifndef PANVK_DESCRIPTOR_SET_H
#define PANVK_DESCRIPTOR_SET_H

#ifndef PAN_ARCH
#error "PAN_ARCH must be defined"
#endif

#include <stdint.h>

#include "vk_object.h"

#include "panvk_macros.h"

#define PANVK_MAX_PUSH_DESCS      32
#define PANVK_MAX_DESC_SIZE       32
#define PANVK_MAX_DESC_UBO_STRIDE 8

struct panvk_cmd_buffer;
struct panvk_descriptor_pool;
struct panvk_descriptor_set_layout;
struct panvk_priv_bo;

struct panvk_desc_pool_counters {
   unsigned samplers;
   unsigned combined_image_samplers;
   unsigned sampled_images;
   unsigned storage_images;
   unsigned uniform_texel_bufs;
   unsigned storage_texel_bufs;
   unsigned input_attachments;
   unsigned uniform_bufs;
   unsigned storage_bufs;
   unsigned uniform_dyn_bufs;
   unsigned storage_dyn_bufs;
   unsigned sets;
};

struct panvk_descriptor_pool {
   struct vk_object_base base;
   struct panvk_desc_pool_counters max;
   struct panvk_desc_pool_counters cur;
   struct panvk_descriptor_set *sets;
};

VK_DEFINE_NONDISP_HANDLE_CASTS(panvk_descriptor_pool, base, VkDescriptorPool,
                               VK_OBJECT_TYPE_DESCRIPTOR_POOL)

/* This has to match nir_address_format_64bit_bounded_global */
struct panvk_ssbo_addr {
   uint64_t base_addr;
   uint32_t size;
   uint32_t zero; /* Must be zero! */
};

struct panvk_bview_desc {
   uint32_t elems;
};

struct panvk_image_desc {
   uint16_t width;
   uint16_t height;
   uint16_t depth;
   uint8_t levels;
   uint8_t samples;
};

struct panvk_buffer_desc {
   struct panvk_buffer *buffer;
   VkDeviceSize offset;
   VkDeviceSize size;
};

struct panvk_descriptor_set {
   struct vk_object_base base;
   struct panvk_descriptor_pool *pool;
   const struct panvk_descriptor_set_layout *layout;
   struct panvk_buffer_desc *dyn_ssbos;
   void *ubos;
   struct panvk_buffer_desc *dyn_ubos;
   void *samplers;
   void *textures;
   void *img_attrib_bufs;
   uint32_t *img_fmts;

   struct {
      struct panvk_priv_bo *bo;
      struct {
         uint64_t dev;
         void *host;
      } addr;
   } desc_ubo;
};

VK_DEFINE_NONDISP_HANDLE_CASTS(panvk_descriptor_set, base, VkDescriptorSet,
                               VK_OBJECT_TYPE_DESCRIPTOR_SET)

struct panvk_push_descriptor_set {
   struct {
      uint8_t descs[PANVK_MAX_PUSH_DESCS * PANVK_MAX_DESC_SIZE];
      uint8_t desc_ubo[PANVK_MAX_PUSH_DESCS * PANVK_MAX_DESC_UBO_STRIDE];
      uint32_t img_fmts[PANVK_MAX_PUSH_DESCS];
   } storage;
   struct panvk_descriptor_set set;
};

#ifdef PAN_ARCH
void
panvk_per_arch(push_descriptor_set_assign_layout)(
   struct panvk_push_descriptor_set *push_set,
   const struct panvk_descriptor_set_layout *layout);

void
panvk_per_arch(push_descriptor_set)(
   struct panvk_push_descriptor_set *push_set,
   const struct panvk_descriptor_set_layout *layout,
   uint32_t write_count, const VkWriteDescriptorSet *writes);

void
panvk_per_arch(push_descriptor_set_with_template)(
   struct panvk_push_descriptor_set *push_set,
   const struct panvk_descriptor_set_layout *layout,
   VkDescriptorUpdateTemplate templ, const void *data);
#endif

#endif
