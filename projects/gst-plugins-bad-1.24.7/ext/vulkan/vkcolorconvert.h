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

#ifndef _VK_COLOR_CONVERT_H_
#define _VK_COLOR_CONVERT_H_

#include <gst/gst.h>
#include <gst/video/video.h>
#include <gst/vulkan/vulkan.h>

G_BEGIN_DECLS

#define GST_TYPE_VULKAN_COLOR_CONVERT            (gst_vulkan_color_convert_get_type())
#define GST_VULKAN_COLOR_CONVERT(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_VULKAN_COLOR_CONVERT,GstVulkanColorConvert))
#define GST_VULKAN_COLOR_CONVERT_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_VULKAN_COLOR_CONVERT,GstVulkanColorConvertClass))
#define GST_IS_VULKAN_COLOR_CONVERT(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_VULKAN_COLOR_CONVERT))
#define GST_IS_VULKAN_COLOR_CONVERT_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_VULKAN_COLOR_CONVERT))

typedef struct _GstVulkanColorConvert GstVulkanColorConvert;
typedef struct _GstVulkanColorConvertClass GstVulkanColorConvertClass;

#define MAX_PUSH_CONSTANTS 4

typedef struct _shader_info shader_info;

typedef GstMemory * (*CommandCreateUniformMemory) (GstVulkanColorConvert * conv, shader_info * sinfo, GstVulkanImageView ** src_views, GstVulkanImageView ** dest_views);

struct _shader_info
{
  GstVideoFormat from;
  GstVideoFormat to;
  CommandCreateUniformMemory cmd_create_uniform;
  gchar *frag_code;
  gsize frag_size;
  gsize uniform_size;
  GDestroyNotify notify;
  gpointer user_data;
};

struct _GstVulkanColorConvert
{
  GstVulkanVideoFilter              parent;

  GstVulkanFullScreenQuad          *quad;

  shader_info                      *current_shader;
};

struct _GstVulkanColorConvertClass
{
  GstVulkanVideoFilterClass parent_class;
};

GType gst_vulkan_color_convert_get_type(void);

GST_ELEMENT_REGISTER_DECLARE (vulkancolorconvert);

G_END_DECLS

#endif
