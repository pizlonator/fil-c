/*
 * GStreamer
 * Copyright (C) 2023 Igalia, S.L.
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

#pragma once

#include <gst/gst.h>
#include <gst/vulkan/gstvkapi.h>

G_BEGIN_DECLS

/**
 * GstVulkanVideoProfile:
 * @profile: the generic vulkan video profile
 * @codec: the specific codec profile
 *
 * Since: 1.24
 */
struct _GstVulkanVideoProfile
{
  /*< private >*/
#if GST_VULKAN_HAVE_VIDEO_EXTENSIONS
  VkVideoProfileInfoKHR profile;
  union {
    VkVideoDecodeUsageInfoKHR decode;
  } usage;

  union {
    VkBaseInStructure base;
    VkVideoDecodeH264ProfileInfoKHR h264dec;
    VkVideoDecodeH265ProfileInfoKHR h265dec;
  } codec;
#endif
  gpointer _reserved[GST_PADDING];
};

/**
 * GstVulkanVideoCapabilities:
 *
 * Since: 1.24
 */
struct _GstVulkanVideoCapabilities
{
  /*< private >*/
#if GST_VULKAN_HAVE_VIDEO_EXTENSIONS
  VkVideoCapabilitiesKHR caps;
  union
  {
    VkBaseInStructure base;
    VkVideoDecodeH264CapabilitiesKHR h264dec;
    VkVideoDecodeH265CapabilitiesKHR h265dec;
  } codec;
#endif
  gpointer _reserved[GST_PADDING];
};

/**
 * GstVulkanVideoOperation:
 * @GST_VULKAN_VIDEO_OPERATION_DECODE: decode operation
 * @GST_VULKAN_VIDEO_OPERATION_ENCODE: encode operation
 * @GST_VULKAN_VIDEO_OPERATION_UNKNOWN: unknown
 *
 * The type of video operation.
 *
 * Since: 1.24
 */
typedef enum  {
  GST_VULKAN_VIDEO_OPERATION_DECODE = 0,
  GST_VULKAN_VIDEO_OPERATION_ENCODE,
  GST_VULKAN_VIDEO_OPERATION_UNKNOWN,
} GstVulkanVideoOperation;

GST_VULKAN_API
GstCaps *               gst_vulkan_video_profile_to_caps        (const GstVulkanVideoProfile * profile);
GST_VULKAN_API
gboolean                gst_vulkan_video_profile_from_caps      (GstVulkanVideoProfile * profile,
                                                                 GstCaps * caps,
                                                                 GstVulkanVideoOperation video_operation);
GST_VULKAN_API
gboolean                gst_vulkan_video_profile_is_valid       (GstVulkanVideoProfile * profile,
                                                                 guint codec);
GST_VULKAN_API
gboolean                gst_vulkan_video_profile_is_equal       (const GstVulkanVideoProfile * a,
                                                                 const GstVulkanVideoProfile * b);

G_END_DECLS
