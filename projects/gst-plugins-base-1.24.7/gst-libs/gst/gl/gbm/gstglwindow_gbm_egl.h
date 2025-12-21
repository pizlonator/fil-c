/*
 * GStreamer
 * Copyright (C) 2018 Carlos Rafael Giani <dv@pseudoterminal.org>
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

#ifndef __GST_GL_WINDOW_GBM_EGL_H__
#define __GST_GL_WINDOW_GBM_EGL_H__

#include <gbm.h>
#include <gst/gl/gl.h>
#include <gst/gl/egl/gstegl.h>
#include <gst/gl/gbm/gstgldisplay_gbm.h>

G_BEGIN_DECLS

#define GST_TYPE_GL_WINDOW_GBM_EGL         (gst_gl_window_gbm_egl_get_type())
#define GST_GL_WINDOW_GBM_EGL(o)           (G_TYPE_CHECK_INSTANCE_CAST((o), GST_TYPE_GL_WINDOW_GBM_EGL, GstGLWindowGBMEGL))
#define GST_GL_WINDOW_GBM_EGL_CLASS(k)     (G_TYPE_CHECK_CLASS((k), GST_TYPE_GL_WINDOW_GBM_EGL, GstGLWindowGBMEGLClass))
#define GST_IS_GL_WINDOW_GBM_EGL(o)        (G_TYPE_CHECK_INSTANCE_TYPE((o), GST_TYPE_GL_WINDOW_GBM_EGL))
#define GST_IS_GL_WINDOW_GBM_EGL_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE((k), GST_TYPE_GL_WINDOW_GBM_EGL))
#define GST_GL_WINDOW_GBM_EGL_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS((o), GST_TYPE_GL_WINDOW_GBM_EGL, GstGLWindowGBMEGL_Class))

typedef struct _GstGLWindowGBMEGL        GstGLWindowGBMEGL;
typedef struct _GstGLWindowGBMEGLClass   GstGLWindowGBMEGLClass;

struct _GstGLWindowGBMEGL {
  /*< private >*/
  GstGLWindow parent;

  struct gbm_surface *gbm_surf;
  struct gbm_bo *current_bo, *prev_bo;
  int waiting_for_flip;

  drmModeCrtc *saved_crtc;

  GstGLDisplayGBM *display;

  gpointer _reserved[GST_PADDING];
};

struct _GstGLWindowGBMEGLClass {
  /*< private >*/
  GstGLWindowClass parent_class;

  /*< private >*/
  gpointer _reserved[GST_PADDING_LARGE];
};

GType gst_gl_window_gbm_egl_get_type (void);

GstGLWindowGBMEGL * gst_gl_window_gbm_egl_new (GstGLDisplay * display);
gboolean gst_gl_window_gbm_egl_create_window (GstGLWindowGBMEGL * window_egl);

G_END_DECLS

#endif /* __GST_GL_WINDOW_GBM_EGL_H__ */
