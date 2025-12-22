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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gtkgstwaylandwidget.h"

/**
 * SECTION:gtkgstwidget
 * @title: GtkGstWaylandWidget
 * @short_description: a #GtkWidget that renders GStreamer video #GstBuffers
 * @see_also: #GtkDrawingArea, #GstBuffer
 *
 * #GtkGstWaylandWidget is an #GtkWidget that renders GStreamer video buffers.
 */

G_DEFINE_TYPE (GtkGstWaylandWidget, gtk_gst_wayland_widget,
    GTK_TYPE_DRAWING_AREA);

static void
gtk_gst_wayland_widget_finalize (GObject * object)
{
  gtk_gst_base_widget_finalize (object);

  G_OBJECT_CLASS (gtk_gst_wayland_widget_parent_class)->finalize (object);
}

static void
gtk_gst_wayland_widget_class_init (GtkGstWaylandWidgetClass * klass)
{
  GObjectClass *gobject_klass = (GObjectClass *) klass;

  gtk_gst_base_widget_class_init (GTK_GST_BASE_WIDGET_CLASS (klass));
  gobject_klass->finalize = gtk_gst_wayland_widget_finalize;
}

static void
gtk_gst_wayland_widget_init (GtkGstWaylandWidget * widget)
{
  gtk_gst_base_widget_init (GTK_GST_BASE_WIDGET (widget));
}

GtkWidget *
gtk_gst_wayland_widget_new (void)
{
  return (GtkWidget *) g_object_new (GTK_TYPE_GST_WAYLAND_WIDGET, NULL);
}
