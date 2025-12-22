/*
 * GStreamer
 * Copyright (C) 2015 Matthew Waters <matthew@centricular.com>
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

#ifndef __GST_VULKAN_DISPLAY_H__
#define __GST_VULKAN_DISPLAY_H__

#include <gst/gst.h>

#include <gst/vulkan/gstvkwindow.h>
#include <gst/vulkan/gstvkinstance.h>

G_BEGIN_DECLS

#define GST_TYPE_VULKAN_DISPLAY             (gst_vulkan_display_get_type())
#define GST_VULKAN_DISPLAY(obj)             (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_VULKAN_DISPLAY,GstVulkanDisplay))
#define GST_VULKAN_DISPLAY_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST((klass), GST_TYPE_VULKAN_DISPLAY,GstVulkanDisplayClass))
#define GST_IS_VULKAN_DISPLAY(obj)          (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_VULKAN_DISPLAY))
#define GST_IS_VULKAN_DISPLAY_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE((klass), GST_TYPE_VULKAN_DISPLAY))
#define GST_VULKAN_DISPLAY_GET_CLASS(o)     (G_TYPE_INSTANCE_GET_CLASS((o), GST_TYPE_VULKAN_DISPLAY, GstVulkanDisplayClass))
GST_VULKAN_API
GType gst_vulkan_display_get_type (void);
/**
 * GST_VULKAN_DISPLAY_CAST
 *
 * Since: 1.18
 */
#define GST_VULKAN_DISPLAY_CAST(obj)        ((GstVulkanDisplay*)(obj))

/**
 * GST_VULKAN_DISPLAY_CONTEXT_TYPE_STR
 *
 * Since: 1.18
 */
#define GST_VULKAN_DISPLAY_CONTEXT_TYPE_STR "gst.vulkan.display"

typedef struct _GstVulkanDisplay GstVulkanDisplay;
typedef struct _GstVulkanDisplayClass GstVulkanDisplayClass;
typedef struct _GstVulkanDisplayPrivate GstVulkanDisplayPrivate;

/**
 * GstVulkanDisplayType:
 * @GST_VULKAN_DISPLAY_TYPE_NONE: no display
 * @GST_VULKAN_DISPLAY_TYPE_XCB: XCB display
 * @GST_VULKAN_DISPLAY_TYPE_WAYLAND: wayland display
 * @GST_VULKAN_DISPLAY_TYPE_COCOA: cocoa display for macOS
 * @GST_VULKAN_DISPLAY_TYPE_IOS: ios display
 * @GST_VULKAN_DISPLAY_TYPE_WIN32: win32 display
 * @GST_VULKAN_DISPLAY_TYPE_ANY: any display type
 *
 * Since: 1.18
 */
typedef enum
{
  GST_VULKAN_DISPLAY_TYPE_NONE = 0,
  GST_VULKAN_DISPLAY_TYPE_XCB = (1 << 0),
  GST_VULKAN_DISPLAY_TYPE_WAYLAND = (1 << 1),
  GST_VULKAN_DISPLAY_TYPE_COCOA = (1 << 2),
  GST_VULKAN_DISPLAY_TYPE_IOS = (1 << 3),
  GST_VULKAN_DISPLAY_TYPE_WIN32 = (1 << 4),
  GST_VULKAN_DISPLAY_TYPE_ANDROID = (1 << 5),

  GST_VULKAN_DISPLAY_TYPE_ANY = G_MAXUINT32
} GstVulkanDisplayType;

/**
 * GstVulkanDisplay:
 *
 * The contents of a #GstVulkanDisplay are private and should only be accessed
 * through the provided API
 *
 * Since: 1.18
 */
struct _GstVulkanDisplay
{
  /* <private> */
  GstObject                 object;

  GstVulkanDisplayType      type;

  GstVulkanInstance        *instance;

  /* <protected> */
  GList                    *windows;        /* OBJECT lock */
  GMainContext             *main_context;
  GMainLoop                *main_loop;
  GSource                  *event_source;

  /* <private> */
  gpointer _reserved        [GST_PADDING];
};

/**
 * GstVulkanDisplayClass:
 * @object_class: parent #GstObjectClass
 * @get_handle: get the native handle to the display
 * @create_window: create a window
 *
 * Since: 1.18
 */
struct _GstVulkanDisplayClass
{
  GstObjectClass object_class;

  gpointer          (*get_handle)           (GstVulkanDisplay * display);
  GstVulkanWindow * (*create_window)        (GstVulkanDisplay * display);

  /* <private> */
  gpointer _reserved        [GST_PADDING];
};

G_DEFINE_AUTOPTR_CLEANUP_FUNC (GstVulkanDisplay, gst_object_unref)

GST_VULKAN_API
GstVulkanDisplay *      gst_vulkan_display_new                      (GstVulkanInstance *instance);
GST_VULKAN_API
GstVulkanDisplay *      gst_vulkan_display_new_with_type            (GstVulkanInstance *instance,
                                                                     GstVulkanDisplayType type);
GST_VULKAN_API
GstVulkanDisplayType    gst_vulkan_display_choose_type              (GstVulkanInstance *instance);
GST_VULKAN_API
const gchar *           gst_vulkan_display_type_to_extension_string (GstVulkanDisplayType type);


GST_VULKAN_API
gpointer                gst_vulkan_display_get_handle               (GstVulkanDisplay * display);
GST_VULKAN_API
GstVulkanDisplayType    gst_vulkan_display_get_handle_type          (GstVulkanDisplay * display);
GST_VULKAN_API
GstVulkanWindow *       gst_vulkan_display_create_window            (GstVulkanDisplay * display);

GST_VULKAN_API
gboolean                gst_context_get_vulkan_display              (GstContext * context,
                                                                     GstVulkanDisplay ** display);
GST_VULKAN_API
void                    gst_context_set_vulkan_display              (GstContext * context,
                                                                     GstVulkanDisplay * display);
GST_VULKAN_API
gboolean                gst_vulkan_display_handle_context_query     (GstElement * element,
                                                                     GstQuery * query,
                                                                     GstVulkanDisplay * display);
GST_VULKAN_API
gboolean                gst_vulkan_display_run_context_query        (GstElement * element,
                                                                     GstVulkanDisplay ** display);

/* GstVulkanWindow usage only */
GST_VULKAN_API
gboolean                gst_vulkan_display_remove_window            (GstVulkanDisplay * display, GstVulkanWindow * window);
GST_VULKAN_API
GstVulkanWindow *       gst_vulkan_display_find_window              (GstVulkanDisplay * display, gpointer data, GCompareFunc compare_func);

G_END_DECLS

#endif /* __GST_VULKAN_DISPLAY_H__ */
