/*
 * GStreamer
 * Copyright (C) 2013 Matthew Waters <ystreet00@gmail.com>
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

#ifndef __GST_VULKAN_DISPLAY_XCB_H__
#define __GST_VULKAN_DISPLAY_XCB_H__

#include <gst/gst.h>

#include <xcb/xcb.h>
#include <vulkan/vulkan_xcb.h>

#include <gst/vulkan/vulkan.h>

G_BEGIN_DECLS

GST_VULKAN_API
GType gst_vulkan_display_xcb_get_type (void);

#define GST_TYPE_VULKAN_DISPLAY_XCB             (gst_vulkan_display_xcb_get_type())
#define GST_VULKAN_DISPLAY_XCB(obj)             (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_VULKAN_DISPLAY_XCB,GstVulkanDisplayXCB))
#define GST_VULKAN_DISPLAY_XCB_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST((klass), GST_TYPE_VULKAN_DISPLAY_XCB,GstVulkanDisplayXCBClass))
#define GST_IS_VULKAN_DISPLAY_XCB(obj)          (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_VULKAN_DISPLAY_XCB))
#define GST_IS_VULKAN_DISPLAY_XCB_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE((klass), GST_TYPE_VULKAN_DISPLAY_XCB))
#define GST_VULKAN_DISPLAY_XCB_CAST(obj)        ((GstVulkanDisplayXCB*)(obj))

typedef struct _GstVulkanDisplayXCB GstVulkanDisplayXCB;
typedef struct _GstVulkanDisplayXCBClass GstVulkanDisplayXCBClass;

#define GST_VULKAN_DISPLAY_XCB_CONNECTION(d) (GST_VULKAN_DISPLAY_XCB(d)->connection)
#define GST_VULKAN_DISPLAY_XCB_ROOT_WINDOW(d) (GST_VULKAN_DISPLAY_XCB(d)->root_window)
#define GST_VULKAN_DISPLAY_XCB_SCREEN(d) (GST_VULKAN_DISPLAY_XCB(d)->screen)

/**
 * GstVulkanDisplayXCB:
 *
 * the contents of a #GstVulkanDisplayXCB are private and should only be accessed
 * through the provided API
 *
 * Since: 1.18
 */
struct _GstVulkanDisplayXCB
{
  GstVulkanDisplay          parent;

  xcb_connection_t *connection;
  xcb_window_t      root_window;
  xcb_screen_t     *screen;

  /* <private> */
  gboolean foreign_display;

  GSource *event_source;

  /* <private> */
  gpointer _reserved        [GST_PADDING];
};

/**
 * GstVulkanDisplayXCBClass:
 *
 * Since: 1.18
 */
struct _GstVulkanDisplayXCBClass
{
  GstVulkanDisplayClass object_class;

  /* <private> */
  gpointer _reserved        [GST_PADDING];
};

GST_VULKAN_API
GstVulkanDisplayXCB * gst_vulkan_display_xcb_new                    (const gchar * name);
GST_VULKAN_API
GstVulkanDisplayXCB * gst_vulkan_display_xcb_new_with_connection    (xcb_connection_t * connection,
                                                                     int screen_no);

G_END_DECLS

#endif /* __GST_VULKAN_DISPLAY_XCB_H__ */
