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

#ifndef _VK_IMAGE_IDENTITY_H_
#define _VK_IMAGE_IDENTITY_H_

#include <gst/gst.h>
#include <gst/video/video.h>
#include <gst/vulkan/vulkan.h>

G_BEGIN_DECLS

#define GST_TYPE_VULKAN_IMAGE_IDENTITY            (gst_vulkan_image_identity_get_type())
#define GST_VULKAN_IMAGE_IDENTITY(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_VULKAN_IMAGE_IDENTITY,GstVulkanImageIdentity))
#define GST_VULKAN_IMAGE_IDENTITY_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_VULKAN_IMAGE_IDENTITY,GstVulkanImageIdentityClass))
#define GST_IS_VULKAN_IMAGE_IDENTITY(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_VULKAN_IMAGE_IDENTITY))
#define GST_IS_VULKAN_IMAGE_IDENTITY_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_VULKAN_IMAGE_IDENTITY))

typedef struct _GstVulkanImageIdentity GstVulkanImageIdentity;
typedef struct _GstVulkanImageIdentityClass GstVulkanImageIdentityClass;

struct _GstVulkanImageIdentity
{
  GstVulkanVideoFilter              parent;

  GstVulkanFullScreenQuad          *quad;
  GstMemory                        *uniforms;
};

struct _GstVulkanImageIdentityClass
{
  GstVulkanVideoFilterClass parent_class;
};

GType gst_vulkan_image_identity_get_type(void);

GST_ELEMENT_REGISTER_DECLARE (vulkanimageidentity);

G_END_DECLS

#endif
