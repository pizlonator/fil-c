/*
 * GStreamer
 * Copyright (C) 2012 Matthew Waters <ystreet00@gmail.com>
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

#ifndef __GST_VULKAN_WINDOW_XCB_H__
#define __GST_VULKAN_WINDOW_XCB_H__

#include <xcb/xcb.h>

#include <gst/vulkan/vulkan.h>

#include <vulkan/vulkan_xcb.h>

G_BEGIN_DECLS

#define GST_TYPE_VULKAN_WINDOW_XCB         (gst_vulkan_window_xcb_get_type())
#define GST_VULKAN_WINDOW_XCB(o)           (G_TYPE_CHECK_INSTANCE_CAST((o), GST_TYPE_VULKAN_WINDOW_XCB, GstVulkanWindowXCB))
#define GST_VULKAN_WINDOW_XCB_CLASS(k)     (G_TYPE_CHECK_CLASS((k), GST_TYPE_VULKAN_WINDOW_XCB, GstVulkanWindowXCBClass))
#define GST_IS_VULKAN_WINDOW_XCB(o)        (G_TYPE_CHECK_INSTANCE_TYPE((o), GST_TYPE_VULKAN_WINDOW_XCB))
#define GST_IS_VULKAN_WINDOW_XCB_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE((k), GST_TYPE_VULKAN_WINDOW_XCB))
#define GST_VULKAN_WINDOW_XCB_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS((o), GST_TYPE_VULKAN_WINDOW_XCB, GstVulkanWindowXCBClass))

typedef struct _GstVulkanWindowXCB        GstVulkanWindowXCB;
typedef struct _GstVulkanWindowXCBPrivate GstVulkanWindowXCBPrivate;
typedef struct _GstVulkanWindowXCBClass   GstVulkanWindowXCBClass;

/**
 * GstVulkanWindowXCB:
 *
 * Opaque #GstVulkanWindowXCB object
 */
struct _GstVulkanWindowXCB
{
  /*< private >*/
  GstVulkanWindow parent;

  /* X window */
  xcb_window_t win_id;

  guint          visible :1;

  PFN_vkCreateXcbSurfaceKHR CreateXcbSurface;
  PFN_vkGetPhysicalDeviceXcbPresentationSupportKHR GetPhysicalDeviceXcbPresentationSupport;

  /*< private >*/
  gpointer _reserved[GST_PADDING];
};

/**
 * GstVulkanWindowXCBClass:
 *
 * Opaque #GstVulkanWindowXCBClass object
 */
struct _GstVulkanWindowXCBClass {
  /*< private >*/
  GstVulkanWindowClass parent_class;

  /*< private >*/
  gpointer _reserved[GST_PADDING_LARGE];
};

GType gst_vulkan_window_xcb_get_type     (void);

GstVulkanWindowXCB * gst_vulkan_window_xcb_new (GstVulkanDisplay * display);

gboolean gst_vulkan_window_xcb_create_window (GstVulkanWindowXCB * window_xcb);

G_END_DECLS

#endif /* __GST_VULKAN_WINDOW_XCB_H__ */
