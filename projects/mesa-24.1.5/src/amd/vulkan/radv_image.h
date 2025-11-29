/*
 * Copyright © 2016 Red Hat.
 * Copyright © 2016 Bas Nieuwenhuizen
 *
 * based in part on anv driver which is:
 * Copyright © 2015 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef RADV_IMAGE_H
#define RADV_IMAGE_H

#include "ac_surface.h"

#include "radv_device.h"
#include "radv_physical_device.h"
#include "radv_radeon_winsys.h"

#include "vk_format.h"
#include "vk_image.h"

static const VkImageUsageFlags RADV_IMAGE_USAGE_WRITE_BITS =
   VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT |
   VK_IMAGE_USAGE_STORAGE_BIT;

struct radv_image_plane {
   VkFormat format;
   struct radeon_surf surface;
};

struct radv_image_binding {
   /* Set when bound */
   struct radeon_winsys_bo *bo;
   VkDeviceSize offset;
   uint64_t bo_va;
   uint64_t range;
};

struct radv_image {
   struct vk_image vk;

   VkDeviceSize size;
   uint32_t alignment;

   unsigned queue_family_mask;
   bool exclusive;
   bool shareable;
   bool l2_coherent;
   bool dcc_sign_reinterpret;
   bool support_comp_to_single;

   struct radv_image_binding bindings[3];
   bool tc_compatible_cmask;

   uint64_t clear_value_offset;
   uint64_t fce_pred_offset;
   uint64_t dcc_pred_offset;

   /*
    * Metadata for the TC-compat zrange workaround. If the 32-bit value
    * stored at this offset is UINT_MAX, the driver will emit
    * DB_Z_INFO.ZRANGE_PRECISION=0, otherwise it will skip the
    * SET_CONTEXT_REG packet.
    */
   uint64_t tc_compat_zrange_offset;

   /* For VK_ANDROID_native_buffer, the WSI image owns the memory, */
   VkDeviceMemory owned_memory;

   unsigned plane_count;
   bool disjoint;
   struct radv_image_plane planes[0];
};

VK_DEFINE_NONDISP_HANDLE_CASTS(radv_image, vk.base, VkImage, VK_OBJECT_TYPE_IMAGE)

static inline bool
radv_image_extent_compare(const struct radv_image *image, const VkExtent3D *extent)
{
   if (extent->width != image->vk.extent.width || extent->height != image->vk.extent.height ||
       extent->depth != image->vk.extent.depth)
      return false;
   return true;
}

/**
 * Return whether the image has CMASK metadata for color surfaces.
 */
static inline bool
radv_image_has_cmask(const struct radv_image *image)
{
   return image->planes[0].surface.cmask_offset;
}

/**
 * Return whether the image has FMASK metadata for color surfaces.
 */
static inline bool
radv_image_has_fmask(const struct radv_image *image)
{
   return image->planes[0].surface.fmask_offset;
}

/**
 * Return whether the image has DCC metadata for color surfaces.
 */
static inline bool
radv_image_has_dcc(const struct radv_image *image)
{
   return !(image->planes[0].surface.flags & RADEON_SURF_Z_OR_SBUFFER) && image->planes[0].surface.meta_offset;
}

/**
 * Return whether the image is TC-compatible CMASK.
 */
static inline bool
radv_image_is_tc_compat_cmask(const struct radv_image *image)
{
   return radv_image_has_fmask(image) && image->tc_compatible_cmask;
}

/**
 * Return whether DCC metadata is enabled for a level.
 */
static inline bool
radv_dcc_enabled(const struct radv_image *image, unsigned level)
{
   return radv_image_has_dcc(image) && level < image->planes[0].surface.num_meta_levels;
}

/**
 * Return whether the image has CB metadata.
 */
static inline bool
radv_image_has_CB_metadata(const struct radv_image *image)
{
   return radv_image_has_cmask(image) || radv_image_has_fmask(image) || radv_image_has_dcc(image);
}

/**
 * Return whether the image has HTILE metadata for depth surfaces.
 */
static inline bool
radv_image_has_htile(const struct radv_image *image)
{
   return image->planes[0].surface.flags & RADEON_SURF_Z_OR_SBUFFER && image->planes[0].surface.meta_size;
}

/**
 * Return whether the image has VRS HTILE metadata for depth surfaces
 */
static inline bool
radv_image_has_vrs_htile(const struct radv_device *device, const struct radv_image *image)
{
   const struct radv_physical_device *pdev = radv_device_physical(device);
   const enum amd_gfx_level gfx_level = pdev->info.gfx_level;

   /* Any depth buffer can potentially use VRS on GFX10.3. */
   return gfx_level == GFX10_3 && device->vk.enabled_features.attachmentFragmentShadingRate &&
          radv_image_has_htile(image) && (image->vk.usage & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT);
}

/**
 * Return whether HTILE metadata is enabled for a level.
 */
static inline bool
radv_htile_enabled(const struct radv_image *image, unsigned level)
{
   return radv_image_has_htile(image) && level < image->planes[0].surface.num_meta_levels;
}

/**
 * Return whether the image is TC-compatible HTILE.
 */
static inline bool
radv_image_is_tc_compat_htile(const struct radv_image *image)
{
   return radv_image_has_htile(image) && (image->planes[0].surface.flags & RADEON_SURF_TC_COMPATIBLE_HTILE);
}

/**
 * Return whether the entire HTILE buffer can be used for depth in order to
 * improve HiZ Z-Range precision.
 */
static inline bool
radv_image_tile_stencil_disabled(const struct radv_device *device, const struct radv_image *image)
{
   const struct radv_physical_device *pdev = radv_device_physical(device);

   if (pdev->info.gfx_level >= GFX9) {
      return !vk_format_has_stencil(image->vk.format) && !radv_image_has_vrs_htile(device, image);
   } else {
      /* Due to a hw bug, TILE_STENCIL_DISABLE must be set to 0 for
       * the TC-compat ZRANGE issue even if no stencil is used.
       */
      return !vk_format_has_stencil(image->vk.format) && !radv_image_is_tc_compat_htile(image);
   }
}

static inline bool
radv_image_has_clear_value(const struct radv_image *image)
{
   return image->clear_value_offset != 0;
}

static inline uint64_t
radv_image_get_fast_clear_va(const struct radv_image *image, uint32_t base_level)
{
   assert(radv_image_has_clear_value(image));

   uint64_t va = radv_buffer_get_va(image->bindings[0].bo);
   va += image->bindings[0].offset + image->clear_value_offset + base_level * 8;
   return va;
}

static inline uint64_t
radv_image_get_fce_pred_va(const struct radv_image *image, uint32_t base_level)
{
   assert(image->fce_pred_offset != 0);

   uint64_t va = radv_buffer_get_va(image->bindings[0].bo);
   va += image->bindings[0].offset + image->fce_pred_offset + base_level * 8;
   return va;
}

static inline uint64_t
radv_image_get_dcc_pred_va(const struct radv_image *image, uint32_t base_level)
{
   assert(image->dcc_pred_offset != 0);

   uint64_t va = radv_buffer_get_va(image->bindings[0].bo);
   va += image->bindings[0].offset + image->dcc_pred_offset + base_level * 8;
   return va;
}

static inline uint64_t
radv_get_tc_compat_zrange_va(const struct radv_image *image, uint32_t base_level)
{
   assert(image->tc_compat_zrange_offset != 0);

   uint64_t va = radv_buffer_get_va(image->bindings[0].bo);
   va += image->bindings[0].offset + image->tc_compat_zrange_offset + base_level * 4;
   return va;
}

static inline uint64_t
radv_get_ds_clear_value_va(const struct radv_image *image, uint32_t base_level)
{
   assert(radv_image_has_clear_value(image));

   uint64_t va = radv_buffer_get_va(image->bindings[0].bo);
   va += image->bindings[0].offset + image->clear_value_offset + base_level * 8;
   return va;
}

static inline uint32_t
radv_get_htile_initial_value(const struct radv_device *device, const struct radv_image *image)
{
   uint32_t initial_value;

   if (radv_image_tile_stencil_disabled(device, image)) {
      /* Z only (no stencil):
       *
       * |31     18|17      4|3     0|
       * +---------+---------+-------+
       * |  Max Z  |  Min Z  | ZMask |
       */
      initial_value = 0xfffc000f;
   } else {
      /* Z and stencil:
       *
       * |31       12|11 10|9    8|7   6|5   4|3     0|
       * +-----------+-----+------+-----+-----+-------+
       * |  Z Range  |     | SMem | SR1 | SR0 | ZMask |
       *
       * SR0/SR1 contains the stencil test results. Initializing
       * SR0/SR1 to 0x3 means the stencil test result is unknown.
       *
       * Z, stencil and 4 bit VRS encoding:
       * |31       12|11        10|9    8|7          6|5   4|3     0|
       * +-----------+------------+------+------------+-----+-------+
       * |  Z Range  | VRS y-rate | SMem | VRS x-rate | SR0 | ZMask |
       */
      if (radv_image_has_vrs_htile(device, image)) {
         /* Initialize the VRS x-rate value at 0, so the hw interprets it as 1 sample. */
         initial_value = 0xfffff33f;
      } else {
         initial_value = 0xfffff3ff;
      }
   }

   return initial_value;
}

static inline bool
radv_image_get_iterate256(const struct radv_device *device, struct radv_image *image)
{
   const struct radv_physical_device *pdev = radv_device_physical(device);

   /* ITERATE_256 is required for depth or stencil MSAA images that are TC-compatible HTILE. */
   return pdev->info.gfx_level >= GFX10 &&
          (image->vk.usage & (VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT)) &&
          radv_image_is_tc_compat_htile(image) && image->vk.samples > 1;
}

bool radv_are_formats_dcc_compatible(const struct radv_physical_device *pdev, const void *pNext, VkFormat format,
                                     VkImageCreateFlags flags, bool *sign_reinterpret);

bool radv_image_use_dcc_image_stores(const struct radv_device *device, const struct radv_image *image);

bool radv_image_use_dcc_predication(const struct radv_device *device, const struct radv_image *image);

unsigned radv_map_swizzle(unsigned swizzle);

void radv_compose_swizzle(const struct util_format_description *desc, const VkComponentMapping *mapping,
                          enum pipe_swizzle swizzle[4]);

bool vi_alpha_is_on_msb(const struct radv_device *device, const VkFormat format);

void radv_init_metadata(struct radv_device *device, struct radv_image *image, struct radeon_bo_metadata *metadata);

void radv_image_override_offset_stride(struct radv_device *device, struct radv_image *image, uint64_t offset,
                                       uint32_t stride);

bool radv_image_can_fast_clear(const struct radv_device *device, const struct radv_image *image);

struct ac_surf_info radv_get_ac_surf_info(struct radv_device *device, const struct radv_image *image);

struct radv_image_create_info {
   const VkImageCreateInfo *vk_info;
   bool scanout;
   bool no_metadata_planes;
   bool prime_blit_src;
   const struct radeon_bo_metadata *bo_metadata;
};

VkResult radv_image_create_layout(struct radv_device *device, struct radv_image_create_info create_info,
                                  const struct VkImageDrmFormatModifierExplicitCreateInfoEXT *mod_info,
                                  const struct VkVideoProfileListInfoKHR *profile_list, struct radv_image *image);

VkResult radv_image_create(VkDevice _device, const struct radv_image_create_info *info,
                           const VkAllocationCallbacks *alloc, VkImage *pImage, bool is_internal);

unsigned radv_plane_from_aspect(VkImageAspectFlags mask);

VkFormat radv_get_aspect_format(struct radv_image *image, VkImageAspectFlags mask);

/* Whether the image has a htile  that is known consistent with the contents of
 * the image and is allowed to be in compressed form.
 *
 * If this is false reads that don't use the htile should be able to return
 * correct results.
 */
bool radv_layout_is_htile_compressed(const struct radv_device *device, const struct radv_image *image,
                                     VkImageLayout layout, unsigned queue_mask);

bool radv_layout_can_fast_clear(const struct radv_device *device, const struct radv_image *image, unsigned level,
                                VkImageLayout layout, unsigned queue_mask);

bool radv_layout_dcc_compressed(const struct radv_device *device, const struct radv_image *image, unsigned level,
                                VkImageLayout layout, unsigned queue_mask);

enum radv_fmask_compression {
   RADV_FMASK_COMPRESSION_NONE,
   RADV_FMASK_COMPRESSION_PARTIAL,
   RADV_FMASK_COMPRESSION_FULL,
};

enum radv_fmask_compression radv_layout_fmask_compression(const struct radv_device *device,
                                                          const struct radv_image *image, VkImageLayout layout,
                                                          unsigned queue_mask);

unsigned radv_image_queue_family_mask(const struct radv_image *image, enum radv_queue_family family,
                                      enum radv_queue_family queue_family);

bool radv_image_is_renderable(const struct radv_device *device, const struct radv_image *image);

unsigned radv_tile_mode_index(const struct radv_image_plane *plane, unsigned level, bool stencil);

#endif /* RADV_IMAGE_H */
