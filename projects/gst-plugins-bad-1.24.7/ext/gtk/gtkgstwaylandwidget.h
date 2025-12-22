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

#pragma once

#include <gtk/gtk.h>
#include <gst/gst.h>

#include "gtkgstbasewidget.h"

G_BEGIN_DECLS

#define GTK_TYPE_GST_WAYLAND_WIDGET gtk_gst_wayland_widget_get_type ()
G_DECLARE_FINAL_TYPE (GtkGstWaylandWidget, gtk_gst_wayland_widget, GTK, GST_WAYLAND_WIDGET, GtkDrawingArea);

/**
 * GtkGstWaylandWidget:
 *
 * Opaque #GtkGstWaylandWidget object
 */
struct _GtkGstWaylandWidget
{
  /* <private> */
  GtkGstBaseWidget parent;
};

GtkWidget * gtk_gst_wayland_widget_new (void);

G_END_DECLS
