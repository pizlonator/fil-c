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

G_BEGIN_DECLS

#define GST_TYPE_WL_BUFFER gst_wl_buffer_get_type ()

GST_WL_API
G_DECLARE_FINAL_TYPE (GstWlBuffer, gst_wl_buffer, GST, WL_BUFFER, GObject);

struct _GstWlBuffer
{
  GObject parent_instance;
};

GST_WL_API
GstWlBuffer * gst_buffer_add_wl_buffer (GstBuffer * gstbuffer,
    struct wl_buffer * wlbuffer, GstWlDisplay * display);

GST_WL_API
GstWlBuffer * gst_buffer_get_wl_buffer (GstWlDisplay * display, GstBuffer * gstbuffer);

GST_WL_API
void gst_wl_buffer_force_release_and_unref (GstBuffer *buf, GstWlBuffer * self);

GST_WL_API
void gst_wl_buffer_attach (GstWlBuffer * self, struct wl_surface *surface);

GST_WL_API
void gst_wl_buffer_ref_gst_buffer(GstWlBuffer * self);

GST_WL_API
void gst_wl_buffer_unref_buffer(GstWlBuffer * self);

GST_WL_API
GstWlDisplay *gst_wl_buffer_get_display (GstWlBuffer * self);

G_END_DECLS
