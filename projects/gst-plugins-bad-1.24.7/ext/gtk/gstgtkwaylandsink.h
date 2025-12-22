/*
 * GStreamer
 * Copyright (C) 2021 Collabora Ltd.
 *   @author George Kiagiadakis <george.kiagiadakis@collabora.com>
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

#pragma once

#include <gtk/gtk.h>
#include <gst/gst.h>
#include <gst/video/video.h>

G_BEGIN_DECLS

#define GST_TYPE_GTK_WAYLAND_SINK gst_gtk_wayland_sink_get_type ()
G_DECLARE_FINAL_TYPE (GstGtkWaylandSink, gst_gtk_wayland_sink, GST, GTK_WAYLAND_SINK, GstVideoSink);

/**
 * GstGtkWaylandSink:
 *
 * Opaque #GstGtkWaylandSink object
 *
 * Since: 1.22
 */
struct _GstGtkWaylandSink
{
  GstVideoSink parent;
};

GST_ELEMENT_REGISTER_DECLARE (gtkwaylandsink);

G_END_DECLS
