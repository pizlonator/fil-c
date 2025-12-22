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

#ifndef __GST_VULKAN_DISPLAY_IOS_H__
#define __GST_VULKAN_DISPLAY_IOS_H__

#include <gst/gst.h>

#include <gst/vulkan/vulkan.h>
#include <vulkan/vulkan_ios.h>

G_BEGIN_DECLS

GType gst_vulkan_display_ios_get_type (void);

#define GST_TYPE_VULKAN_DISPLAY_IOS             (gst_vulkan_display_ios_get_type())
#define GST_VULKAN_DISPLAY_IOS(obj)             (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_VULKAN_DISPLAY_IOS,GstVulkanDisplayIos))
#define GST_VULKAN_DISPLAY_IOS_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST((klass), GST_TYPE_VULKAN_DISPLAY_IOS,GstVulkanDisplayIosClass))
#define GST_IS_VULKAN_DISPLAY_IOS(obj)          (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_VULKAN_DISPLAY_IOS))
#define GST_IS_VULKAN_DISPLAY_IOS_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE((klass), GST_TYPE_VULKAN_DISPLAY_IOS))
#define GST_VULKAN_DISPLAY_IOS_CAST(obj)        ((GstVulkanDisplayIos*)(obj))

typedef struct _GstVulkanDisplayIos GstVulkanDisplayIos;
typedef struct _GstVulkanDisplayIosClass GstVulkanDisplayIosClass;

/**
 * GstVulkanDisplayIos:
 *
 * the contents of a #GstVulkanDisplayIos are private and should only be accessed
 * through the provided API
 */
struct _GstVulkanDisplayIos
{
  GstVulkanDisplay          parent;
};

struct _GstVulkanDisplayIosClass
{
  GstVulkanDisplayClass object_class;
};

GstVulkanDisplayIos * gst_vulkan_display_ios_new                    (void);

G_END_DECLS

#endif /* __GST_VULKAN_DISPLAY_IOS_H__ */
