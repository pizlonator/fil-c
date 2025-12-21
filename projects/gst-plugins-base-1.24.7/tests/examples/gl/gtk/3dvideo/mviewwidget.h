/*
 * GStreamer
 * Copyright (C) 2014-2015 Jan Schmidt <jan@centricular.com>
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
#include <string.h>

#include <gst/gst.h>
#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include <gst/video/video-info.h>
#include <gst/gl/gl.h>

G_BEGIN_DECLS

#define GST_TYPE_MVIEW_WIDGET (gst_mview_widget_get_type())
#define GST_MVIEW_WIDGET(obj) \
        (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_MVIEW_WIDGET, GstMViewWidget))
#define GST_MVIEW_WIDGET_CLASS(klass) \
        (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_MVIEW_WIDGET, GstMViewWidgetClass))
#define GST_IS_MVIEW_WIDGET(obj) \
        (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_MVIEW_WIDGET))
#define GST_IS_MVIEW_WIDGET_CLASS(klass) \
        (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_MVIEW_WIDGET))
#define GST_MVIEW_WIDGET_GET_CLASS(obj) \
        (G_TYPE_INSTANCE_GET_CLASS((obj),GST_TYPE_MVIEW_WIDGET,GstMViewWidgetClass))

typedef struct _GstMViewWidget GstMViewWidget;
typedef struct _GstMViewWidgetClass GstMViewWidgetClass;

struct _GstMViewWidget {
  GtkGrid parent;

  gboolean is_output;

  GtkWidget *mode_selector;

  GstVideoMultiviewMode mode;
  GstVideoMultiviewFlags flags;
  GstGLStereoDownmix downmix_mode;

  /* Array of toggle buttons for flags */
  GtkWidget *lflip;
  GtkWidget *lflop;
  GtkWidget *rflip;
  GtkWidget *rflop;
  GtkWidget *half_aspect;
  GtkWidget *right_first;

  GtkWidget *downmix_combo;

  gboolean synching;
};

struct _GstMViewWidgetClass {
  GtkGridClass parent;
};

GType gst_mview_widget_get_type (void);
GtkWidget *gst_mview_widget_new (gboolean is_output);

G_END_DECLS
