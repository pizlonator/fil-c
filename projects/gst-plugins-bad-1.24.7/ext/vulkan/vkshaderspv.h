/*
 * GStreamer
 * Copyright (C) 2021 Martin Reboredo <yakoyoku@gmail.com>
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

#ifndef _VK_SHADER_SPV_H_
#define _VK_SHADER_SPV_H_

#include <gst/gst.h>
#include <gst/video/video.h>
#include <gst/vulkan/vulkan.h>

G_BEGIN_DECLS

#define GST_TYPE_VULKAN_SHADER_SPV            (gst_vulkan_shader_spv_get_type())
#define GST_VULKAN_SHADER_SPV(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_VULKAN_SHADER_SPV,GstVulkanShaderSpv))
#define GST_VULKAN_SHADER_SPV_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_VULKAN_SHADER_SPV,GstVulkanShaderSpvClass))
#define GST_IS_VULKAN_SHADER_SPV(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_VULKAN_SHADER_SPV))
#define GST_IS_VULKAN_SHADER_SPV_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_VULKAN_SHADER_SPV))

typedef struct _GstVulkanShaderSpv GstVulkanShaderSpv;
typedef struct _GstVulkanShaderSpvClass GstVulkanShaderSpvClass;

struct _GstVulkanShaderSpv
{
  GstVulkanVideoFilter filter;

  GBytes *vert;
  GBytes *frag;
  gchararray vert_path;
  gchararray frag_path;

  GstVulkanFullScreenQuad *quad;
  GstMemory               *uniforms;

  gboolean period;
};

struct _GstVulkanShaderSpvClass
{
  GstVulkanVideoFilterClass parent_class;
};

GType gst_vulkan_shader_spv_get_type(void);

GST_ELEMENT_REGISTER_DECLARE (vulkanshaderspv);

G_END_DECLS

#endif
