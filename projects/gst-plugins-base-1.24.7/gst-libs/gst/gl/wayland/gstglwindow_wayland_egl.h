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

#ifndef __GST_GL_WINDOW_WAYLAND_EGL_H__
#define __GST_GL_WINDOW_WAYLAND_EGL_H__

#include <wayland-client.h>
#include "xdg-shell-client-protocol.h"
#include <wayland-egl.h>
#include <wayland-cursor.h>

#include <gst/gl/gl.h>

G_BEGIN_DECLS

#define GST_TYPE_GL_WINDOW_WAYLAND_EGL         (gst_gl_window_wayland_egl_get_type())
#define GST_GL_WINDOW_WAYLAND_EGL(o)           (G_TYPE_CHECK_INSTANCE_CAST((o), GST_TYPE_GL_WINDOW_WAYLAND_EGL, GstGLWindowWaylandEGL))
#define GST_GL_WINDOW_WAYLAND_EGL_CLASS(k)     (G_TYPE_CHECK_CLASS((k), GST_TYPE_GL_WINDOW_WAYLAND_EGL, GstGLWindowWaylandEGLClass))
#define GST_IS_GL_WINDOW_WAYLAND_EGL(o)        (G_TYPE_CHECK_INSTANCE_TYPE((o), GST_TYPE_GL_WINDOW_WAYLAND_EGL))
#define GST_IS_GL_WINDOW_WAYLAND_EGL_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE((k), GST_TYPE_GL_WINDOW_WAYLAND_EGL))
#define GST_GL_WINDOW_WAYLAND_EGL_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS((o), GST_TYPE_GL_WINDOW_WAYLAND_EGL, GstGLWindowWaylandEGL_Class))

typedef struct _GstGLWindowWaylandEGL        GstGLWindowWaylandEGL;
typedef struct _GstGLWindowWaylandEGLClass   GstGLWindowWaylandEGLClass;

struct window;

struct display {
  struct wl_display      *display;
  struct wl_registry     *registry;
  struct wl_compositor   *compositor;
  struct wl_subcompositor *subcompositor;
  struct wl_shell        *shell;
  struct wl_seat         *seat;
  struct wl_pointer      *pointer;
  struct wl_keyboard     *keyboard;
  struct wl_shm          *shm;
  struct wl_cursor_theme *cursor_theme;
  struct wl_cursor       *default_cursor;
  struct wl_surface      *cursor_surface;
  struct xdg_wm_base     *xdg_wm_base;
  struct window          *window;
  guint32                 serial;

  gdouble pointer_x;
  gdouble pointer_y;
};

struct window {
  struct display *display;

  struct wl_event_queue     *queue;

  /* wl_shell */
  struct wl_surface         *surface;
  struct wl_shell_surface   *wl_shell_surface;
  /* XDG-shell */
  struct xdg_surface        *xdg_surface;
  struct xdg_toplevel       *xdg_toplevel;

  struct wl_egl_window      *native;
  struct wl_surface         *foreign_surface;
  struct wl_subsurface      *subsurface;
  struct wl_callback        *callback;
  int fullscreen, configured;
  int window_width, window_height;
  int preferred_width, preferred_height;
  int window_x, window_y;
  GstVideoRectangle render_rect;
};

struct _GstGLWindowWaylandEGL {
  /*< private >*/
  GstGLWindow parent;

  struct display display;
  struct window  window;

  GSource *wl_source;

  gpointer _reserved[GST_PADDING];
};

struct _GstGLWindowWaylandEGLClass {
  /*< private >*/
  GstGLWindowClass parent_class;

  /*< private >*/
  gpointer _reserved[GST_PADDING];
};

G_GNUC_INTERNAL
GType                   gst_gl_window_wayland_egl_get_type (void);

G_GNUC_INTERNAL
GstGLWindowWaylandEGL * gst_gl_window_wayland_egl_new  (GstGLDisplay * display);

G_GNUC_INTERNAL
void gst_gl_window_wayland_egl_create_window (GstGLWindowWaylandEGL * window_egl);

G_END_DECLS

#endif /* __GST_GL_WINDOW_WAYLAND_EGL_H__ */
