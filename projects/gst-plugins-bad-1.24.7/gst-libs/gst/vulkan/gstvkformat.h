/*
 * GStreamer
 * Copyright (C) 2016 Matthew Waters <matthew@centricular.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifndef __GST_VULKAN_FORMAT_H__
#define __GST_VULKAN_FORMAT_H__

#include <gst/vulkan/vulkan.h>
#include <gst/video/video.h>

G_BEGIN_DECLS

typedef struct _GstVulkanFormatInfo GstVulkanFormatInfo;

/**
 * GST_VULKAN_MAX_COMPONENTS:
 *
 * Since: 1.18
 */
#define GST_VULKAN_MAX_COMPONENTS 4

/**
 * GstVulkanFormatScaling:
 * @GST_VULKAN_FORMAT_SCALING_UNORM: [0, 2^n - 1] -> [0.0, 1.0]
 * @GST_VULKAN_FORMAT_SCALING_SNORM: [-2^(n-1), 2^(n-1) - 1] -> [-1.0, 1.0]
 * @GST_VULKAN_FORMAT_SCALING_USCALED: [0, 2^n - 1] -> [0.0, float(2^n - 1)]
 * @GST_VULKAN_FORMAT_SCALING_SSCALED: [-2^(n-1), 2^(n-1) - 1] -> [float(-2^(n-1)), float(2^(n-1) - 1)]
 * @GST_VULKAN_FORMAT_SCALING_UINT: [0, 2^n - 1] -> [0, 2^n - 1]
 * @GST_VULKAN_FORMAT_SCALING_SINT: [-2^(n-1), 2^(n-1) - 1] -> [-2^(n-1), 2^(n-1) - 1]
 * @GST_VULKAN_FORMAT_SCALING_SRGB: @GST_VULKAN_FORMAT_SCALING_UNORM but the first three components are gamma corrected for the sRGB colour space.
 *
 * Since: 1.18
 */
typedef enum
{
  GST_VULKAN_FORMAT_SCALING_UNORM = 1,
  GST_VULKAN_FORMAT_SCALING_SNORM,
  GST_VULKAN_FORMAT_SCALING_USCALED,
  GST_VULKAN_FORMAT_SCALING_SSCALED,
  GST_VULKAN_FORMAT_SCALING_UINT,
  GST_VULKAN_FORMAT_SCALING_SINT,
  GST_VULKAN_FORMAT_SCALING_SRGB,
} GstVulkanFormatScaling;

/**
 * GstVulkanFormatFlags:
 * @GST_VULKAN_FORMAT_FLAG_YUV: is a YUV format
 * @GST_VULKAN_FORMAT_FLAG_RGB: is a RGB format
 * @GST_VULKAN_FORMAT_FLAG_ALPHA: has an alpha channel
 * @GST_VULKAN_FORMAT_FLAG_LE: data is stored in little-endiate byte order
 * @GST_VULKAN_FORMAT_FLAG_COMPLEX: data is stored complex and cannot be read/write only using the information in the #GstVulkanFormatInfo
 *
 * Since: 1.18
 */
typedef enum
{
  GST_VULKAN_FORMAT_FLAG_YUV = (1 << 0),
  GST_VULKAN_FORMAT_FLAG_RGB = (1 << 1),
  GST_VULKAN_FORMAT_FLAG_ALPHA = (1 << 2),
  GST_VULKAN_FORMAT_FLAG_LE = (1 << 3),
  GST_VULKAN_FORMAT_FLAG_COMPLEX = (1 << 4),
} GstVulkanFormatFlags;

/**
 * GstVulkanFormatInfo:
 * @format: the Vulkan format being described
 * @name: name of this format
 * @scaling: how raw data is interpreted and scaled
 * @flags: flags that apply to this format
 * @bits: The number of bits used to pack data items. This can be less than
 *        8 when multiple pixels are stored in a byte. for values > 8 multiple
 *        bytes should be read according to the endianness flag before
 *        applying the shift and mask.
 * @n_components; number of components in this format
 * @shift: the number of bits to shift away to get the component data
 * @depth: the depth in bits for each component
 * @n_planes: the number of planes for this format. The number of planes can
 *            be less than the amount of components when multiple components
 *            are packed into one plane.
 * @plane: the plane number where a component can be found
 * @poffset: the offset in the plane where the first pixel of the components
 *           can be found.
 * @w_sub: subsampling factor of the width for the component.
 *         Use GST_VIDEO_SUB_SCALE to scale a width.
 * @h_sub: subsampling factor of the height for the component.
 *         Use GST_VIDEO_SUB_SCALE to scale a height.
 *
 * Since: 1.18
 */
struct _GstVulkanFormatInfo
{
  VkFormat format;
  const gchar *name;
  GstVulkanFormatScaling scaling;
  GstVulkanFormatFlags flags;
  guint bits;
  guint n_components;
  guint8 shift[GST_VULKAN_MAX_COMPONENTS];
  guint8 depth[GST_VULKAN_MAX_COMPONENTS];
  gint8 pixel_stride[GST_VULKAN_MAX_COMPONENTS];
  guint n_planes;
  guint8 plane[GST_VULKAN_MAX_COMPONENTS];
  guint8 poffset[GST_VULKAN_MAX_COMPONENTS];
  guint8 w_sub[GST_VULKAN_MAX_COMPONENTS];
  guint8 h_sub[GST_VULKAN_MAX_COMPONENTS];

  /**
   * GstVulkanFormatInfo.aspect:
   *
   * image aspect of this format
   *
   * Since: 1.24
   */
  VkImageAspectFlags aspect;
};

GST_VULKAN_API
const GstVulkanFormatInfo *     gst_vulkan_format_get_info                      (VkFormat format);

GST_VULKAN_API
guint                           gst_vulkan_format_get_aspect                    (VkFormat format);

GST_VULKAN_API
VkFormat                        gst_vulkan_format_from_video_info               (GstVideoInfo * v_info,
                                                                                 guint plane);

GST_VULKAN_API
gboolean                        gst_vulkan_format_from_video_info_2            (GstVulkanPhysicalDevice * physical_device,
                                                                                GstVideoInfo * info,
                                                                                VkImageTiling tiling,
                                                                                gboolean no_multiplane,
                                                                                VkImageUsageFlags requested_usage,
                                                                                VkFormat fmts[GST_VIDEO_MAX_PLANES],
                                                                                int * n_imgs,
                                                                                VkImageUsageFlags * usage);

GST_VULKAN_API
GstVideoFormat                  gst_vulkan_format_to_video_format              (VkFormat vk_format);

G_END_DECLS

#endif /* __GST_VULKAN_FORMAT_H__ */
