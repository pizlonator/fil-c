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

#ifndef __GST_VULKAN_WINDOW_H__
#define __GST_VULKAN_WINDOW_H__

#include <gst/gst.h>

#include <gst/vulkan/vulkan.h>

G_BEGIN_DECLS

#define GST_TYPE_VULKAN_WINDOW         (gst_vulkan_window_get_type())
#define GST_VULKAN_WINDOW(o)           (G_TYPE_CHECK_INSTANCE_CAST((o), GST_TYPE_VULKAN_WINDOW, GstVulkanWindow))
#define GST_VULKAN_WINDOW_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), GST_TYPE_VULKAN_WINDOW, GstVulkanWindowClass))
#define GST_IS_VULKAN_WINDOW(o)        (G_TYPE_CHECK_INSTANCE_TYPE((o), GST_TYPE_VULKAN_WINDOW))
#define GST_IS_VULKAN_WINDOW_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE((k), GST_TYPE_VULKAN_WINDOW))
#define GST_VULKAN_WINDOW_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS((o), GST_TYPE_VULKAN_WINDOW, GstVulkanWindowClass))
GST_VULKAN_API
GType gst_vulkan_window_get_type     (void);

/**
 * GST_VULKAN_WINDOW_LOCK:
 *
 * Since: 1.18
 */
#define GST_VULKAN_WINDOW_LOCK(w) g_mutex_lock(&GST_VULKAN_WINDOW(w)->lock)
/**
 * GST_VULKAN_WINDOW_UNLOCK:
 *
 * Since: 1.18
 */
#define GST_VULKAN_WINDOW_UNLOCK(w) g_mutex_unlock(&GST_VULKAN_WINDOW(w)->lock)
/**
 * GST_VULKAN_WINDOW_GET_LOCK:
 *
 * Since: 1.18
 */
#define GST_VULKAN_WINDOW_GET_LOCK(w) (&GST_VULKAN_WINDOW(w)->lock)

/**
 * GST_VULKAN_WINDOW_ERROR:
 *
 * Since: 1.18
 */
#define GST_VULKAN_WINDOW_ERROR (gst_vulkan_window_error_quark ())
/**
 * gst_vulkan_window_error_quark:
 *
 * Since: 1.18
 */
GST_VULKAN_API
GQuark gst_vulkan_window_error_quark (void);

/**
 * GstVulkanWindowError:
 * @GST_VULKAN_WINDOW_ERROR_FAILED: failed
 * @GST_VULKAN_WINDOW_ERROR_OLD_LIBS: old libraries
 * @GST_VULKAN_WINDOW_ERROR_RESOURCE_UNAVAILABLE: resource unavailable
 *
 * Since: 1.18
 */
typedef enum
{
  GST_VULKAN_WINDOW_ERROR_FAILED,
  GST_VULKAN_WINDOW_ERROR_OLD_LIBS,
  GST_VULKAN_WINDOW_ERROR_RESOURCE_UNAVAILABLE,
} GstVulkanWindowError;

/**
 * GstVulkanWindow:
 *
 * #GstVulkanWindow is an opaque struct and should only be accessed through the
 * provided api.
 *
 * Since: 1.18
 */
struct _GstVulkanWindow {
  /*< private >*/
  GstObject parent;

  GstVulkanDisplay       *display;

//  GMutex                  lock;

  gpointer                _reserved[GST_PADDING];
};

/**
 * GstVulkanWindowClass:
 * @parent_class: Parent class
 * @open: open the connection to the display
 * @close: close the connection to the display
 * @get_surface: retrieve the vulkan surface for this window
 * @get_presentation_support: retrieve whether this window supports presentation
 * @set_window_handle: set the external window handle to render into
 * @get_surface_dimensions: retrieve the current size of the window
 * @handle_event: set whether to handle extra window system events
 *
 * Since: 1.18
 */
struct _GstVulkanWindowClass {
  GstObjectClass parent_class;

  gboolean      (*open)                         (GstVulkanWindow *window,
                                                 GError **error);
  void          (*close)                        (GstVulkanWindow *window);

  VkSurfaceKHR  (*get_surface)                  (GstVulkanWindow *window,
                                                 GError **error);
  gboolean      (*get_presentation_support)     (GstVulkanWindow *window,
                                                 GstVulkanDevice *device,
                                                 guint32 queue_family_idx);
  void          (*set_window_handle)            (GstVulkanWindow *window,
                                                 guintptr handle);
  void          (*get_surface_dimensions)       (GstVulkanWindow *window,
                                                 guint * width,
                                                 guint * height);
  void          (*handle_events)                (GstVulkanWindow *window,
                                                 gboolean handle_events);

  /*< private >*/
  gpointer _reserved[GST_PADDING];
};

G_DEFINE_AUTOPTR_CLEANUP_FUNC(GstVulkanWindow, gst_object_unref)

GST_VULKAN_API
GstVulkanWindow *  gst_vulkan_window_new                            (GstVulkanDisplay *display);

GST_VULKAN_API
GstVulkanDisplay * gst_vulkan_window_get_display                    (GstVulkanWindow *window);
GST_VULKAN_API
VkSurfaceKHR       gst_vulkan_window_get_surface                    (GstVulkanWindow *window,
                                                                     GError **error);
GST_VULKAN_API
gboolean           gst_vulkan_window_get_presentation_support       (GstVulkanWindow *window,
                                                                     GstVulkanDevice *device,
                                                                     guint32 queue_family_idx);
GST_VULKAN_API
void               gst_vulkan_window_get_surface_dimensions         (GstVulkanWindow *window,
                                                                     guint *width,
                                                                     guint *height);
GST_VULKAN_API
void               gst_vulkan_window_set_window_handle              (GstVulkanWindow *window,
                                                                     guintptr handle);

GST_VULKAN_API
void                gst_vulkan_window_handle_events                 (GstVulkanWindow * window,
                                                                     gboolean handle_events);
GST_VULKAN_API
void                gst_vulkan_window_send_key_event                (GstVulkanWindow * window,
                                                                     const char * event_type,
                                                                     const char * key_str);
GST_VULKAN_API
void                gst_vulkan_window_send_mouse_event              (GstVulkanWindow * window,
                                                                     const char * event_type,
                                                                     int button,
                                                                     double posx,
                                                                     double posy);

GST_VULKAN_API
gboolean           gst_vulkan_window_open                           (GstVulkanWindow *window,
                                                                     GError ** error);
GST_VULKAN_API
void               gst_vulkan_window_close                          (GstVulkanWindow *window);

GST_VULKAN_API
void               gst_vulkan_window_resize                         (GstVulkanWindow *window,
                                                                     gint width,
                                                                     gint height);
GST_VULKAN_API
void               gst_vulkan_window_redraw                         (GstVulkanWindow *window);

G_END_DECLS

#endif /* __GST_VULKAN_WINDOW_H__ */
