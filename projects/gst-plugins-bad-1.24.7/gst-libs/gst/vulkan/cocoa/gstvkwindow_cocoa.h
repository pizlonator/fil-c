/*
 * GStreamer
 * Copyright (C) 2019 Matthew Waters <ystreet00@gmail.com>
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

#ifndef __GST_VULKAN_WINDOW_COCOA_H__
#define __GST_VULKAN_WINDOW_COCOA_H__

#include <gst/vulkan/vulkan.h>
#include <vulkan/vulkan_macos.h>

G_BEGIN_DECLS

#define GST_TYPE_VULKAN_WINDOW_COCOA         (gst_vulkan_window_cocoa_get_type())
#define GST_VULKAN_WINDOW_COCOA(o)           (G_TYPE_CHECK_INSTANCE_CAST((o), GST_TYPE_VULKAN_WINDOW_COCOA, GstVulkanWindowCocoa))
#define GST_VULKAN_WINDOW_COCOA_CLASS(k)     (G_TYPE_CHECK_CLASS((k), GST_TYPE_VULKAN_WINDOW_COCOA, GstVulkanWindowCocoaClass))
#define GST_IS_VULKAN_WINDOW_COCOA(o)        (G_TYPE_CHECK_INSTANCE_TYPE((o), GST_TYPE_VULKAN_WINDOW_COCOA))
#define GST_IS_VULKAN_WINDOW_COCOA_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE((k), GST_TYPE_VULKAN_WINDOW_COCOA))
#define GST_VULKAN_WINDOW_COCOA_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS((o), GST_TYPE_VULKAN_WINDOW_COCOA, GstVulkanWindowCocoaClass))

typedef struct _GstVulkanWindowCocoa        GstVulkanWindowCocoa;
typedef struct _GstVulkanWindowCocoaPrivate GstVulkanWindowCocoaPrivate;
typedef struct _GstVulkanWindowCocoaClass   GstVulkanWindowCocoaClass;

/**
 * GstVulkanWindowCocoa:
 *
 * Opaque #GstVulkanWindowCocoa object
 */
struct _GstVulkanWindowCocoa
{
  /*< private >*/
  GstVulkanWindow parent;

  gpointer view;

  gint          visible :1;

  PFN_vkCreateMacOSSurfaceMVK CreateMacOSSurface;

  /*< private >*/  
  gpointer _reserved[GST_PADDING];
};

/**
 * GstVulkanWindowCocoaClass:
 *
 * Opaque #GstVulkanWindowCocoaClass object
 */
struct _GstVulkanWindowCocoaClass {
  /*< private >*/
  GstVulkanWindowClass parent_class;

  /*< private >*/
  gpointer _reserved[GST_PADDING_LARGE];
};

GType gst_vulkan_window_cocoa_get_type     (void);

GstVulkanWindowCocoa * gst_vulkan_window_cocoa_new (GstVulkanDisplay * display);

gboolean gst_vulkan_window_cocoa_create_window (GstVulkanWindowCocoa * window_cocoa);

G_END_DECLS

#endif /* __GST_VULKAN_WINDOW_COCOA_H__ */
