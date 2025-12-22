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

#ifndef __GST_VULKAN_WINDOW_IOS_H__
#define __GST_VULKAN_WINDOW_IOS_H__

#include <gst/vulkan/vulkan.h>
#include <vulkan/vulkan_ios.h>

G_BEGIN_DECLS

#define GST_TYPE_VULKAN_WINDOW_IOS         (gst_vulkan_window_ios_get_type())
#define GST_VULKAN_WINDOW_IOS(o)           (G_TYPE_CHECK_INSTANCE_CAST((o), GST_TYPE_VULKAN_WINDOW_IOS, GstVulkanWindowIos))
#define GST_VULKAN_WINDOW_IOS_CLASS(k)     (G_TYPE_CHECK_CLASS((k), GST_TYPE_VULKAN_WINDOW_IOS, GstVulkanWindowIosClass))
#define GST_IS_VULKAN_WINDOW_IOS(o)        (G_TYPE_CHECK_INSTANCE_TYPE((o), GST_TYPE_VULKAN_WINDOW_IOS))
#define GST_IS_VULKAN_WINDOW_IOS_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE((k), GST_TYPE_VULKAN_WINDOW_IOS))
#define GST_VULKAN_WINDOW_IOS_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS((o), GST_TYPE_VULKAN_WINDOW_IOS, GstVulkanWindowIosClass))

typedef struct _GstVulkanWindowIos        GstVulkanWindowIos;
typedef struct _GstVulkanWindowIosPrivate GstVulkanWindowIosPrivate;
typedef struct _GstVulkanWindowIosClass   GstVulkanWindowIosClass;

/**
 * GstVulkanWindowIos:
 *
 * Opaque #GstVulkanWindowIos object
 */
struct _GstVulkanWindowIos
{
  /*< private >*/
  GstVulkanWindow parent;

  gpointer view;

  gint          visible :1;

  PFN_vkCreateIOSSurfaceMVK CreateIOSSurface;

  /*< private >*/
  gpointer _reserved[GST_PADDING];
};

/**
 * GstVulkanWindowIosClass:
 *
 * Opaque #GstVulkanWindowIosClass object
 */
struct _GstVulkanWindowIosClass {
  /*< private >*/
  GstVulkanWindowClass parent_class;

  /*< private >*/
  gpointer _reserved[GST_PADDING_LARGE];
};

GType gst_vulkan_window_ios_get_type     (void);

GstVulkanWindowIos * gst_vulkan_window_ios_new (GstVulkanDisplay * display);

gboolean gst_vulkan_window_ios_create_window (GstVulkanWindowIos * window_ios);

G_END_DECLS

#endif /* __GST_VULKAN_WINDOW_IOS_H__ */
