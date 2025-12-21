/*
 * GStreamer
 * Copyright (C) 2013 Matthew Waters <ystreet00@gmail.com>
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

#ifndef __GST_GL_DISPLAY_WAYLAND_H__
#define __GST_GL_DISPLAY_WAYLAND_H__

#include <gst/gst.h>

#include <wayland-client.h>

#include <gst/gl/gstgl_fwd.h>
#include <gst/gl/gstgldisplay.h>

G_BEGIN_DECLS

GST_GL_API GType gst_gl_display_wayland_get_type (void);

#define GST_TYPE_GL_DISPLAY_WAYLAND             (gst_gl_display_wayland_get_type())
#define GST_GL_DISPLAY_WAYLAND(obj)             (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_GL_DISPLAY_WAYLAND,GstGLDisplayWayland))
#define GST_GL_DISPLAY_WAYLAND_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST((klass), GST_TYPE_GL_DISPLAY_WAYLAND,GstGLDisplayWaylandClass))
#define GST_IS_GL_DISPLAY_WAYLAND(obj)          (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_GL_DISPLAY_WAYLAND))
#define GST_IS_GL_DISPLAY_WAYLAND_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE((klass), GST_TYPE_GL_DISPLAY_WAYLAND))
#define GST_GL_DISPLAY_WAYLAND_CAST(obj)        ((GstGLDisplayWayland*)(obj))

typedef struct _GstGLDisplayWayland GstGLDisplayWayland;
typedef struct _GstGLDisplayWaylandClass GstGLDisplayWaylandClass;

/**
 * GstGLDisplayWayland:
 *
 * the contents of a #GstGLDisplayWayland are private and should only be accessed
 * through the provided API
 */
struct _GstGLDisplayWayland
{
  GstGLDisplay            parent;

  struct wl_display       *display;
  struct wl_registry      *registry;            /* unset */
  struct wl_compositor    *compositor;          /* unset */
  struct wl_subcompositor *subcompositor;       /* unset */

  /* Basic shell, see private struct for others (e.g. XDG-shell) */
  struct wl_shell     *shell;                   /* unset */

  /*< private >*/
  gboolean foreign_display;

  gpointer _padding[GST_PADDING];
};

struct _GstGLDisplayWaylandClass
{
  GstGLDisplayClass object_class;

  gpointer _padding[GST_PADDING];
};

GST_GL_API
GstGLDisplayWayland *gst_gl_display_wayland_new (const gchar * name);

GST_GL_API
GstGLDisplayWayland *gst_gl_display_wayland_new_with_display (struct wl_display *display);

G_END_DECLS

#endif /* __GST_GL_DISPLAY_WAYLAND_H__ */
