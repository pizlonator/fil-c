/*
 * GStreamer
 * Copyright (C) 2019 Matthew Waters <matthew@centricular.com>
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

#ifndef _VK_VIEW_CONVERT_H_
#define _VK_VIEW_CONVERT_H_

#include <gst/gst.h>
#include <gst/video/video.h>
#include <gst/vulkan/vulkan.h>

G_BEGIN_DECLS

#define GST_TYPE_VULKAN_VIEW_CONVERT            (gst_vulkan_view_convert_get_type())
#define GST_VULKAN_VIEW_CONVERT(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_VULKAN_VIEW_CONVERT,GstVulkanViewConvert))
#define GST_VULKAN_VIEW_CONVERT_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_VULKAN_VIEW_CONVERT,GstVulkanViewConvertClass))
#define GST_IS_VULKAN_VIEW_CONVERT(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_VULKAN_VIEW_CONVERT))
#define GST_IS_VULKAN_VIEW_CONVERT_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_VULKAN_VIEW_CONVERT))

typedef struct _GstVulkanViewConvert GstVulkanViewConvert;
typedef struct _GstVulkanViewConvertClass GstVulkanViewConvertClass;

typedef enum
{
  GST_VULKAN_STEREO_DOWNMIX_ANAGLYPH_GREEN_MAGENTA_DUBOIS,
  GST_VULKAN_STEREO_DOWNMIX_ANAGLYPH_RED_CYAN_DUBOIS,
  GST_VULKAN_STEREO_DOWNMIX_ANAGLYPH_AMBER_BLUE_DUBOIS,
} GstVulkanStereoDownmix;

struct _GstVulkanViewConvert
{
  GstVulkanVideoFilter              parent;

  GstVulkanFullScreenQuad          *quad;

  /* properties */
  GstVideoMultiviewMode             input_mode_override;
  GstVideoMultiviewFlags            input_flags_override;
  GstVideoMultiviewMode             output_mode_override;
  GstVideoMultiviewFlags            output_flags_override;

  GstVulkanStereoDownmix            downmix_mode;

  GstMemory                        *uniform;
};

struct _GstVulkanViewConvertClass
{
  GstVulkanVideoFilterClass parent_class;
};

GType gst_vulkan_view_convert_get_type(void);

GST_ELEMENT_REGISTER_DECLARE (vulkanviewconvert);

G_END_DECLS

#endif
