/*
 * GStreamer
 * Copyright (C) 2013 Sebastian Dröge <slomo@circular-chaos.org>
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

#ifndef __GST_GL_CONTEXT_EGL_H__
#define __GST_GL_CONTEXT_EGL_H__

#include <gst/gl/gstglcontext.h>
#include <gst/gl/egl/gstgldisplay_egl.h>

G_BEGIN_DECLS

typedef struct _GstGLDmaModifier GstGLDmaModifier;
typedef struct _GstGLContextEGL GstGLContextEGL;
typedef struct _GstGLContextEGLClass GstGLContextEGLClass;

G_GNUC_INTERNAL GType gst_gl_context_egl_get_type (void);
#define GST_TYPE_GL_CONTEXT_EGL         (gst_gl_context_egl_get_type())

#define GST_GL_CONTEXT_EGL(o)           (G_TYPE_CHECK_INSTANCE_CAST((o), GST_TYPE_GL_CONTEXT_EGL, GstGLContextEGL))
#define GST_GL_CONTEXT_EGL_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), GST_TYPE_GL_CONTEXT_EGL, GstGLContextEGLClass))
#define GST_IS_GL_CONTEXT_EGL(o)        (G_TYPE_CHECK_INSTANCE_TYPE((o), GST_TYPE_GL_CONTEXT_EGL))
#define GST_IS_GL_CONTEXT_EGL_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE((k), GST_TYPE_GL_CONTEXT_EGL))
#define GST_GL_CONTEXT_EGL_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS((o), GST_TYPE_GL_CONTEXT_EGL, GstGLContextEGLClass))

G_DEFINE_AUTOPTR_CLEANUP_FUNC(GstGLContextEGL, gst_object_unref)

/**
 * GstGLDmaModifier: (skip)
 *
 * Opaque struct
 */
struct _GstGLDmaModifier
{
  /*< private >*/
  guint64 modifier;
  gboolean external_only;
};

/**
 * GstGLContextEGL:
 *
 * Opaque #GstGLContextEGL struct
 */
struct _GstGLContextEGL
{
  /*< private >*/
  GstGLContext context;

  GstGLDisplayEGL *display_egl;

  gpointer egl_context;
  gpointer egl_display;
  gpointer egl_surface;
  gpointer egl_config;

  gint egl_major;
  gint egl_minor;

  GstGLAPI gl_api;

  const gchar *egl_exts;

  /* Cached handle */
  guintptr window_handle;
  gulong window_handle_signal;

  GstStructure *requested_config;

  GArray *dma_formats;
};

/**
 * GstGLContextEGLCLass:
 *
 * Opaque #GstGLContextEGLClass struct
 */
struct _GstGLContextEGLClass
{
  /*< private >*/
  GstGLContextClass parent;
};

G_GNUC_INTERNAL
GstGLContextEGL *   gst_gl_context_egl_new                  (GstGLDisplay * display);

G_GNUC_INTERNAL
guintptr            gst_gl_context_egl_get_current_context  (void);

G_GNUC_INTERNAL
gpointer            gst_gl_context_egl_get_proc_address     (GstGLAPI gl_api, const gchar * name);

G_GNUC_INTERNAL
gboolean            gst_gl_context_egl_fill_info            (GstGLContext * context, GError ** error);

G_GNUC_INTERNAL
gboolean            gst_gl_context_egl_get_format_modifiers (GstGLContext * context,
                                                             gint fourcc,
                                                             const GArray ** modifiers);
G_GNUC_INTERNAL
gboolean            gst_gl_context_egl_supports_modifier     (GstGLContext * context);
G_END_DECLS

#endif /* __GST_GL_CONTEXT_EGL_H__ */
