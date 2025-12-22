/* GStreamer Wayland Library
 *
 * Copyright (C) 2014 Collabora Ltd.
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
 * License along with this library; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301 USA.
 */

#pragma once

#include <gst/wayland/wayland.h>
#include <gst/video/video.h>

G_BEGIN_DECLS

#define GST_TYPE_WL_WINDOW (gst_wl_window_get_type ())

GST_WL_API
G_DECLARE_FINAL_TYPE (GstWlWindow, gst_wl_window, GST, WL_WINDOW, GObject);

struct _GstWlWindow
{
  GObject parent_instance;
};

GST_WL_API
void gst_wl_window_ensure_fullscreen (GstWlWindow * self,
        gboolean fullscreen);

GST_WL_API
GstWlWindow *gst_wl_window_new_toplevel (GstWlDisplay * display,
        const GstVideoInfo * info, gboolean fullscreen, GMutex * render_lock);

GST_WL_API
GstWlWindow *gst_wl_window_new_in_surface (GstWlDisplay * display,
        struct wl_surface * parent, GMutex * render_lock);

GST_WL_API
GstWlDisplay *gst_wl_window_get_display (GstWlWindow * self);

GST_WL_API
struct wl_surface *gst_wl_window_get_wl_surface (GstWlWindow * self);

GST_WL_API
struct wl_subsurface *gst_wl_window_get_subsurface (GstWlWindow * self);

GST_WL_API
gboolean gst_wl_window_is_toplevel (GstWlWindow * self);

GST_WL_API
gboolean gst_wl_window_render (GstWlWindow * self, GstWlBuffer * buffer,
        const GstVideoInfo * info);

GST_WL_API
void gst_wl_window_set_render_rectangle (GstWlWindow * self, gint x, gint y,
        gint w, gint h);

GST_WL_API
const GstVideoRectangle *gst_wl_window_get_render_rectangle (GstWlWindow * self);

GST_WL_API
void gst_wl_window_set_rotate_method (GstWlWindow               *self,
        GstVideoOrientationMethod  rotate_method);

G_END_DECLS
