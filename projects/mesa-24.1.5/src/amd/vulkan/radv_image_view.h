/*
 * Copyright © 2016 Red Hat.
 * Copyright © 2016 Bas Nieuwenhuizen
 *
 * based in part on anv driver which is:
 * Copyright © 2015 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef RADV_IMAGE_VIEW_H
#define RADV_IMAGE_VIEW_H

#include "ac_surface.h"

#include "radv_image.h"

union radv_descriptor {
   struct {
      uint32_t plane0_descriptor[8];
      uint32_t fmask_descriptor[8];
   };
   struct {
      uint32_t plane_descriptors[3][8];
   };
};

struct radv_image_view {
   struct vk_image_view vk;
   struct radv_image *image; /**< VkImageViewCreateInfo::image */

   unsigned plane_id;
   VkExtent3D extent; /**< Extent of VkImageViewCreateInfo::baseMipLevel. */

   /* Whether the image iview supports fast clear. */
   bool support_fast_clear;

   bool disable_dcc_mrt;

   union radv_descriptor descriptor;

   /* Descriptor for use as a storage image as opposed to a sampled image.
    * This has a few differences for cube maps (e.g. type).
    */
   union radv_descriptor storage_descriptor;

   /* Block-compressed image views on GFX10+. */
   struct ac_surf_nbc_view nbc_view;
};

VK_DEFINE_NONDISP_HANDLE_CASTS(radv_image_view, vk.base, VkImageView, VK_OBJECT_TYPE_IMAGE_VIEW);

struct radv_image_view_extra_create_info {
   bool disable_compression;
   bool enable_compression;
   bool disable_dcc_mrt;
   bool from_client; /**< Set only if this came from vkCreateImage */
};

void radv_image_view_init(struct radv_image_view *view, struct radv_device *device,
                          const VkImageViewCreateInfo *pCreateInfo, VkImageCreateFlags img_create_flags,
                          const struct radv_image_view_extra_create_info *extra_create_info);
void radv_image_view_finish(struct radv_image_view *iview);

void radv_set_mutable_tex_desc_fields(struct radv_device *device, struct radv_image *image,
                                      const struct legacy_surf_level *base_level_info, unsigned plane_id,
                                      unsigned base_level, unsigned first_level, unsigned block_width, bool is_stencil,
                                      bool is_storage_image, bool disable_compression, bool enable_write_compression,
                                      uint32_t *state, const struct ac_surf_nbc_view *nbc_view);

void radv_make_texture_descriptor(struct radv_device *device, struct radv_image *image, bool is_storage_image,
                                  VkImageViewType view_type, VkFormat vk_format, const VkComponentMapping *mapping,
                                  unsigned first_level, unsigned last_level, unsigned first_layer, unsigned last_layer,
                                  unsigned width, unsigned height, unsigned depth, float min_lod, uint32_t *state,
                                  uint32_t *fmask_state, VkImageCreateFlags img_create_flags,
                                  const struct ac_surf_nbc_view *nbc_view,
                                  const VkImageViewSlicedCreateInfoEXT *sliced_3d);

#endif /* RADV_IMAGE_VIEW_H */
