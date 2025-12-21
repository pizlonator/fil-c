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

#ifndef __GST_GL_WINDOW_WIN32_H__
#define __GST_GL_WINDOW_WIN32_H__

#include <gst/gl/gl.h>

#undef UNICODE
#include <windows.h>
#define UNICODE

G_BEGIN_DECLS

#define GST_TYPE_GL_WINDOW_WIN32         (gst_gl_window_win32_get_type())
#define GST_GL_WINDOW_WIN32(o)           (G_TYPE_CHECK_INSTANCE_CAST((o), GST_TYPE_GL_WINDOW_WIN32, GstGLWindowWin32))
#define GST_GL_WINDOW_WIN32_CLASS(k)     (G_TYPE_CHECK_CLASS((k), GST_TYPE_GL_WINDOW_WIN32, GstGLWindowWin32Class))
#define GST_IS_GL_WINDOW_WIN32(o)        (G_TYPE_CHECK_INSTANCE_TYPE((o), GST_TYPE_GL_WINDOW_WIN32))
#define GST_IS_GL_WINDOW_WIN32_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE((k), GST_TYPE_GL_WINDOW_WIN32))
#define GST_GL_WINDOW_WIN32_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS((o), GST_TYPE_GL_WINDOW_WIN32, GstGLWindowWin32Class))

typedef struct _GstGLWindowWin32        GstGLWindowWin32;
typedef struct _GstGLWindowWin32Private GstGLWindowWin32Private;
typedef struct _GstGLWindowWin32Class   GstGLWindowWin32Class;

struct _GstGLWindowWin32 {
  /*< private >*/
  GstGLWindow parent;

  HWND internal_win_id;
  HWND parent_win_id;
  HDC device;
  gboolean is_closed;
  gboolean visible;

  GSource *msg_source;

  /*< private >*/
  GstGLWindowWin32Private *priv;

  gpointer _reserved[GST_PADDING];
};

struct _GstGLWindowWin32Class {
  /*< private >*/
  GstGLWindowClass parent_class;

  gboolean (*choose_format)     (GstGLWindowWin32 *window);
  gboolean (*create_context)    (GstGLWindowWin32 *window, GstGLAPI gl_api,
                                 guintptr external_gl_context, GError ** error);
  gboolean (*share_context)     (GstGLWindowWin32 *window, guintptr external_gl_context);
  void     (*swap_buffers)      (GstGLWindowWin32 *window);
  gboolean (*activate)          (GstGLWindowWin32 *window, gboolean activate);
  void     (*destroy_context)   (GstGLWindowWin32 *window);
  guintptr (*get_gl_context)    (GstGLWindowWin32 *window);

  /*< private >*/
  gpointer _reserved[GST_PADDING_LARGE];
};

GType gst_gl_window_win32_get_type     (void);

GstGLWindowWin32 * gst_gl_window_win32_new          (GstGLDisplay * display);

gboolean gst_gl_window_win32_create_window (GstGLWindowWin32 * window_win32, GError ** error);

G_END_DECLS

#endif /* __GST_GL_WINDOW_WIN32_H__ */
