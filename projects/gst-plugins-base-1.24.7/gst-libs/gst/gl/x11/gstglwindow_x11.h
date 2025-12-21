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

#ifndef __GST_GL_WINDOW_X11_H__
#define __GST_GL_WINDOW_X11_H__

#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include <gst/video/video.h>

#include <gst/gl/gstgl_fwd.h>
#include <gst/gl/gstglwindow.h>

G_BEGIN_DECLS

#define GST_TYPE_GL_WINDOW_X11         (gst_gl_window_x11_get_type())
G_GNUC_INTERNAL GType gst_gl_window_x11_get_type (void);

#define GST_GL_WINDOW_X11(o)           (G_TYPE_CHECK_INSTANCE_CAST((o), GST_TYPE_GL_WINDOW_X11, GstGLWindowX11))
#define GST_GL_WINDOW_X11_CLASS(k)     (G_TYPE_CHECK_CLASS((k), GST_TYPE_GL_WINDOW_X11, GstGLWindowX11Class))
#define GST_IS_GL_WINDOW_X11(o)        (G_TYPE_CHECK_INSTANCE_TYPE((o), GST_TYPE_GL_WINDOW_X11))
#define GST_IS_GL_WINDOW_X11_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE((k), GST_TYPE_GL_WINDOW_X11))
#define GST_GL_WINDOW_X11_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS((o), GST_TYPE_GL_WINDOW_X11, GstGLWindowX11Class))

typedef struct _GstGLWindowX11        GstGLWindowX11;
typedef struct _GstGLWindowX11Private GstGLWindowX11Private;
typedef struct _GstGLWindowX11Class   GstGLWindowX11Class;

/**
 * GstGLWindowX11:
 *
 * Opaque #GstGLWindowX11 object
 */
struct _GstGLWindowX11
{
  /*< private >*/
  GstGLWindow parent;

  gboolean      running;
  gboolean      visible;
  gboolean      allow_extra_expose_events;

  /* opengl context */
  Display      *device;
  Screen       *screen;
  gint          screen_num;
  Visual       *visual;
  Window        root;
  gulong        white;
  gulong        black;
  gint          depth;
  gint          device_width;
  gint          device_height;
  gint          connection;
  XVisualInfo  *visual_info;
  Window        parent_win;

  /* X window */
  Window        internal_win_id;

  GSource *x11_source;

  /*< private >*/
  GstGLWindowX11Private *priv;
  
  gpointer _reserved[GST_PADDING];
};

/**
 * GstGLWindowX11Class:
 *
 * Opaque #GstGLWindowX11Class object
 */
struct _GstGLWindowX11Class {
  /*< private >*/
  GstGLWindowClass parent_class;

  /*< private >*/
  gpointer _reserved[GST_PADDING_LARGE];
};

G_GNUC_INTERNAL
GstGLWindowX11 * gst_gl_window_x11_new (GstGLDisplay * display);

G_GNUC_INTERNAL
void gst_gl_window_x11_trap_x_errors (void);

G_GNUC_INTERNAL
gint gst_gl_window_x11_untrap_x_errors (void);

G_GNUC_INTERNAL
gboolean gst_gl_window_x11_create_window (GstGLWindowX11 * window_x11);

G_END_DECLS

#endif /* __GST_GL_WINDOW_X11_H__ */
