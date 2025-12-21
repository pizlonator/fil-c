/*
 * GStreamer
 * Copyright (C) 2008 Julien Isorce <julien.isorce@gmail.com>
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

#define GLIB_DISABLE_DEPRECATION_WARNINGS

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <locale.h>
#include <sys/mman.h>
#include <unistd.h>

#include "wayland_event_source.h"

#include "../gstgl_fwd.h"
#include <gst/gl/gstglcontext.h>

#include "gstgldisplay_wayland.h"
#include "gstglwindow_wayland_egl.h"

#include "../gstglwindow_private.h"

const gchar *WlEGLErrorString ();

#define GST_CAT_DEFAULT gst_gl_window_debug

#define gst_gl_window_wayland_egl_parent_class parent_class
G_DEFINE_TYPE (GstGLWindowWaylandEGL, gst_gl_window_wayland_egl,
    GST_TYPE_GL_WINDOW);

static guintptr gst_gl_window_wayland_egl_get_window_handle (GstGLWindow *
    window);
static void gst_gl_window_wayland_egl_set_window_handle (GstGLWindow * window,
    guintptr handle);
static void gst_gl_window_wayland_egl_show (GstGLWindow * window);
static void gst_gl_window_wayland_egl_draw (GstGLWindow * window);
static void gst_gl_window_wayland_egl_close (GstGLWindow * window);
static gboolean gst_gl_window_wayland_egl_open (GstGLWindow * window,
    GError ** error);
static guintptr gst_gl_window_wayland_egl_get_display (GstGLWindow * window);
static gboolean gst_gl_window_wayland_egl_set_render_rectangle (GstGLWindow *
    window, gint x, gint y, gint width, gint height);
static void gst_gl_window_wayland_egl_set_preferred_size (GstGLWindow * window,
    gint width, gint height);

static void
pointer_handle_enter (void *data, struct wl_pointer *pointer, uint32_t serial,
    struct wl_surface *surface, wl_fixed_t sx_w, wl_fixed_t sy_w)
{
  GstGLWindowWaylandEGL *window_egl = data;
  struct wl_buffer *buffer;
  struct wl_cursor_image *image = NULL;

  window_egl->display.serial = serial;

  /* FIXME: Not sure how useful this is */
  if (window_egl->display.default_cursor) {
    image = window_egl->display.default_cursor->images[0];
    buffer = wl_cursor_image_get_buffer (image);
    wl_pointer_set_cursor (pointer, serial,
        window_egl->display.cursor_surface, image->hotspot_x, image->hotspot_y);
    wl_surface_attach (window_egl->display.cursor_surface, buffer, 0, 0);
    wl_surface_damage (window_egl->display.cursor_surface, 0, 0,
        image->width, image->height);
    wl_surface_commit (window_egl->display.cursor_surface);
  }
}

static void
pointer_handle_leave (void *data, struct wl_pointer *pointer, uint32_t serial,
    struct wl_surface *surface)
{
  GstGLWindowWaylandEGL *window_egl = data;
  window_egl->display.serial = serial;
}

static void
pointer_handle_motion (void *data, struct wl_pointer *pointer, uint32_t time,
    wl_fixed_t sx_w, wl_fixed_t sy_w)
{
  GstGLWindowWaylandEGL *window_egl = data;
  GstGLWindow *window = GST_GL_WINDOW (window_egl);

  window_egl->display.pointer_x = wl_fixed_to_double (sx_w);
  window_egl->display.pointer_y = wl_fixed_to_double (sy_w);
  gst_gl_window_send_mouse_event (window, "mouse-move", 0,
      window_egl->display.pointer_x, window_egl->display.pointer_y);
}

static void
pointer_handle_button (void *data, struct wl_pointer *pointer, uint32_t serial,
    uint32_t time, uint32_t button, uint32_t state)
{
  GstGLWindowWaylandEGL *window_egl = data;
  GstGLWindow *window = GST_GL_WINDOW (window_egl);
  const char *event_type;

  event_type = state == 1 ? "mouse-button-press" : "mouse-button-release";
  gst_gl_window_send_mouse_event (window, event_type, button,
      window_egl->display.pointer_x, window_egl->display.pointer_y);
}

static void
pointer_handle_axis (void *data, struct wl_pointer *pointer, uint32_t time,
    uint32_t axis, wl_fixed_t value)
{
  GstGLWindowWaylandEGL *window_egl = data;
  GstGLWindow *window = GST_GL_WINDOW (window_egl);
  gdouble delta_x, delta_y;
  gdouble delta = -wl_fixed_to_double (value);
  if (axis == 1) {
    delta_x = delta;
    delta_y = 0;
  } else {
    delta_x = 0;
    delta_y = delta;
  }
  gst_gl_window_send_scroll_event (window, window_egl->display.pointer_x,
      window_egl->display.pointer_y, delta_x, delta_y);
}

static const struct wl_pointer_listener pointer_listener = {
  pointer_handle_enter,
  pointer_handle_leave,
  pointer_handle_motion,
  pointer_handle_button,
  pointer_handle_axis,
};

static void
seat_handle_capabilities (void *data, struct wl_seat *seat,
    enum wl_seat_capability caps)
{
  GstGLWindowWaylandEGL *window_egl = data;
  struct display *display = &window_egl->display;

  if ((caps & WL_SEAT_CAPABILITY_POINTER) && !display->pointer) {
    display->pointer = wl_seat_get_pointer (seat);
    wl_pointer_set_user_data (display->pointer, window_egl);
    wl_pointer_add_listener (display->pointer, &pointer_listener, window_egl);
  } else if (!(caps & WL_SEAT_CAPABILITY_POINTER) && display->pointer) {
    wl_pointer_destroy (display->pointer);
    display->pointer = NULL;
  }
#if 0
  if ((caps & WL_SEAT_CAPABILITY_KEYBOARD) && !input->keyboard) {
    input->keyboard = wl_seat_get_keyboard (seat);
    wl_keyboard_set_user_data (input->keyboard, input);
    wl_keyboard_add_listener (input->keyboard, &keyboard_listener, input);
  } else if (!(caps & WL_SEAT_CAPABILITY_KEYBOARD) && input->keyboard) {
    wl_keyboard_destroy (input->keyboard);
    input->keyboard = NULL;
  }
#endif
}

static void
seat_name (void *data, struct wl_seat *seat, const char *name)
{
  GstGLWindowWaylandEGL *window_egl = data;

  GST_TRACE_OBJECT (window_egl, "seat %p has name %s", seat, name);
}

static const struct wl_seat_listener seat_listener = {
  seat_handle_capabilities,
  seat_name
};

static void
handle_ping (void *data, struct wl_shell_surface *wl_shell_surface,
    uint32_t serial)
{
  GstGLWindowWaylandEGL *window_egl = data;

  GST_TRACE_OBJECT (window_egl, "ping received serial %u", serial);

  wl_shell_surface_pong (wl_shell_surface, serial);
}

static void window_resize (GstGLWindowWaylandEGL * window_egl, guint width,
    guint height);

static void
handle_configure (void *data, struct wl_shell_surface *wl_shell_surface,
    uint32_t edges, int32_t width, int32_t height)
{
  GstGLWindowWaylandEGL *window_egl = data;

  GST_DEBUG ("configure event on surface %p, %ix%i", wl_shell_surface, width,
      height);

  window_resize (window_egl, width, height);
}

static void
handle_popup_done (void *data, struct wl_shell_surface *wl_shell_surface)
{
}

static const struct wl_shell_surface_listener wl_shell_surface_listener = {
  handle_ping,
  handle_configure,
  handle_popup_done
};

static void
handle_xdg_toplevel_close (void *data, struct xdg_toplevel *xdg_toplevel)
{
  GstGLWindow *window = data;

  GST_DEBUG ("XDG toplevel got a \"close\" event.");

  if (window->close)
    window->close (window->close_data);

  gst_gl_display_remove_window (window->display, window);
}

static void
handle_xdg_toplevel_configure (void *data, struct xdg_toplevel *xdg_toplevel,
    int32_t width, int32_t height, struct wl_array *states)
{
  GstGLWindowWaylandEGL *window_egl = data;
  const uint32_t *state;

  GST_DEBUG ("configure event on XDG toplevel %p, %ix%i", xdg_toplevel,
      width, height);

  wl_array_for_each (state, states) {
    switch (*state) {
      case XDG_TOPLEVEL_STATE_FULLSCREEN:
        window_egl->window.fullscreen = TRUE;
        break;
      case XDG_TOPLEVEL_STATE_MAXIMIZED:
      case XDG_TOPLEVEL_STATE_RESIZING:
      case XDG_TOPLEVEL_STATE_ACTIVATED:
        break;
    }
  }

  if (width > 0 && height > 0)
    window_resize (window_egl, width, height);
}

static const struct xdg_toplevel_listener xdg_toplevel_listener = {
  handle_xdg_toplevel_configure,
  handle_xdg_toplevel_close,
};

static void
handle_xdg_surface_configure (void *data, struct xdg_surface *xdg_surface,
    uint32_t serial)
{
  xdg_surface_ack_configure (xdg_surface, serial);
}

static const struct xdg_surface_listener xdg_surface_listener = {
  handle_xdg_surface_configure,
};

static void
destroy_surfaces (GstGLWindowWaylandEGL * window_egl)
{
  g_clear_pointer (&window_egl->window.subsurface, wl_subsurface_destroy);
  g_clear_pointer (&window_egl->window.xdg_toplevel, xdg_toplevel_destroy);
  g_clear_pointer (&window_egl->window.xdg_surface, xdg_surface_destroy);
  g_clear_pointer (&window_egl->window.wl_shell_surface,
      wl_shell_surface_destroy);
  g_clear_pointer (&window_egl->window.surface, wl_surface_destroy);
  g_clear_pointer (&window_egl->window.native, wl_egl_window_destroy);
}

static void
create_xdg_surface_and_toplevel (GstGLWindowWaylandEGL * window_egl)
{
  struct xdg_surface *xdg_surface;
  struct xdg_toplevel *xdg_toplevel;

  GST_DEBUG ("Creating surfaces XDG-shell");

  /* First create the XDG surface */
  xdg_surface = xdg_wm_base_get_xdg_surface (window_egl->display.xdg_wm_base,
      window_egl->window.surface);
  xdg_surface_add_listener (xdg_surface, &xdg_surface_listener, window_egl);

  /* Then the XDG top-level */
  xdg_toplevel = xdg_surface_get_toplevel (xdg_surface);
  if (g_get_prgname ()) {
    xdg_toplevel_set_app_id (xdg_toplevel, g_get_prgname ());
  } else {
    xdg_toplevel_set_app_id (xdg_toplevel, "org.gstreamer.wayland");
  }
  xdg_toplevel_set_title (xdg_toplevel, "OpenGL Renderer");
  xdg_toplevel_add_listener (xdg_toplevel, &xdg_toplevel_listener, window_egl);

  /* Commit the xdg_surface state */
  wl_surface_commit (window_egl->window.surface);

  /* And save them into the fields */
  window_egl->window.xdg_surface = xdg_surface;
  window_egl->window.xdg_toplevel = xdg_toplevel;
}

static void
create_wl_shell_surface (GstGLWindowWaylandEGL * window_egl)
{
  struct wl_shell_surface *wl_shell_surface;

  GST_DEBUG ("Creating surfaces for wl-shell");

  wl_shell_surface = wl_shell_get_shell_surface (window_egl->display.shell,
      window_egl->window.surface);

  wl_shell_surface_add_listener (wl_shell_surface, &wl_shell_surface_listener,
      window_egl);
  wl_shell_surface_set_title (wl_shell_surface, "OpenGL Renderer");
  wl_shell_surface_set_toplevel (wl_shell_surface);

  window_egl->window.wl_shell_surface = wl_shell_surface;
}

static void
create_surfaces (GstGLWindowWaylandEGL * window_egl)
{
  gint width, height;

  if (!window_egl->window.surface) {
    window_egl->window.surface =
        wl_compositor_create_surface (window_egl->display.compositor);
  }

  if (window_egl->window.foreign_surface) {
    /* (re)parent */
    if (!window_egl->display.subcompositor) {
      GST_ERROR_OBJECT (window_egl,
          "Wayland server does not support subsurfaces");
      window_egl->window.foreign_surface = NULL;
      goto shell_window;
    }

    if (!window_egl->window.subsurface) {
      window_egl->window.subsurface =
          wl_subcompositor_get_subsurface (window_egl->display.subcompositor,
          window_egl->window.surface, window_egl->window.foreign_surface);

      wl_subsurface_set_position (window_egl->window.subsurface,
          window_egl->window.window_x, window_egl->window.window_y);
      wl_subsurface_set_desync (window_egl->window.subsurface);
    }
  } else {
  shell_window:
    if (window_egl->display.xdg_wm_base) {
      if (!window_egl->window.xdg_surface)
        create_xdg_surface_and_toplevel (window_egl);
    } else if (!window_egl->window.wl_shell_surface) {
      create_wl_shell_surface (window_egl);
    }
  }

  /*
   * render_rect is the application requested size so choose that first if
   * available.
   * Else choose the already chosen size if set
   * Else choose the preferred size if set
   * Else choose a default value
   */
  if (window_egl->window.render_rect.w > 0)
    width = window_egl->window.render_rect.w;
  else if (window_egl->window.window_width > 0)
    width = window_egl->window.window_width;
  else if (window_egl->window.preferred_width > 0)
    width = window_egl->window.preferred_width;
  else
    width = 320;
  window_egl->window.window_width = width;

  if (window_egl->window.render_rect.h > 0)
    height = window_egl->window.render_rect.h;
  else if (window_egl->window.window_height > 0)
    height = window_egl->window.window_height;
  else if (window_egl->window.preferred_height > 0)
    height = window_egl->window.preferred_height;
  else
    height = 240;
  window_egl->window.window_height = height;

  if (!window_egl->window.native) {
    gst_gl_window_resize (GST_GL_WINDOW (window_egl), width, height);

    window_egl->window.native =
        wl_egl_window_create (window_egl->window.surface, width, height);
  }
}

static void
gst_gl_window_wayland_egl_class_init (GstGLWindowWaylandEGLClass * klass)
{
  GstGLWindowClass *window_class = (GstGLWindowClass *) klass;

  window_class->get_window_handle =
      GST_DEBUG_FUNCPTR (gst_gl_window_wayland_egl_get_window_handle);
  window_class->set_window_handle =
      GST_DEBUG_FUNCPTR (gst_gl_window_wayland_egl_set_window_handle);
  window_class->show = GST_DEBUG_FUNCPTR (gst_gl_window_wayland_egl_show);
  window_class->draw = GST_DEBUG_FUNCPTR (gst_gl_window_wayland_egl_draw);
  window_class->close = GST_DEBUG_FUNCPTR (gst_gl_window_wayland_egl_close);
  window_class->open = GST_DEBUG_FUNCPTR (gst_gl_window_wayland_egl_open);
  window_class->get_display =
      GST_DEBUG_FUNCPTR (gst_gl_window_wayland_egl_get_display);
  window_class->set_render_rectangle =
      GST_DEBUG_FUNCPTR (gst_gl_window_wayland_egl_set_render_rectangle);
  window_class->set_preferred_size =
      GST_DEBUG_FUNCPTR (gst_gl_window_wayland_egl_set_preferred_size);
}

static void
gst_gl_window_wayland_egl_init (GstGLWindowWaylandEGL * window)
{
  window->window.render_rect.w = window->window.render_rect.h = -1;
}

/* Must be called in the gl thread */
GstGLWindowWaylandEGL *
gst_gl_window_wayland_egl_new (GstGLDisplay * display)
{
  GstGLWindowWaylandEGL *window;

  if ((gst_gl_display_get_handle_type (display) & GST_GL_DISPLAY_TYPE_WAYLAND)
      == 0)
    /* we require a wayland display to create wayland surfaces */
    return NULL;

  GST_DEBUG ("creating Wayland EGL window");

  window = g_object_new (GST_TYPE_GL_WINDOW_WAYLAND_EGL, NULL);
  gst_object_ref_sink (window);

  return window;
}

static void
gst_gl_window_wayland_egl_close (GstGLWindow * gl_window)
{
  GstGLWindowWaylandEGL *window_egl;
  struct display *display;

  window_egl = GST_GL_WINDOW_WAYLAND_EGL (gl_window);
  display = &window_egl->display;

  if (display->pointer != NULL) {
    wl_pointer_destroy (display->pointer);
    display->pointer = NULL;
  }

  destroy_surfaces (window_egl);

  /* As we are about to destroy the wl_source, we need to ensure everything
   * has been sent synchronously, otherwise we will be leaking surfaces on
   * server, leaving the window visible and unrefreshed on screen. */
  wl_display_flush (GST_GL_DISPLAY_WAYLAND (gl_window->display)->display);

  g_source_destroy (window_egl->wl_source);
  g_source_unref (window_egl->wl_source);
  window_egl->wl_source = NULL;

  if (window_egl->display.registry)
    wl_registry_destroy (window_egl->display.registry);

  if (window_egl->display.compositor)
    wl_compositor_destroy (window_egl->display.compositor);

  if (window_egl->display.subcompositor)
    wl_subcompositor_destroy (window_egl->display.subcompositor);

  if (window_egl->display.xdg_wm_base)
    xdg_wm_base_destroy (window_egl->display.xdg_wm_base);

  if (window_egl->display.shell)
    wl_shell_destroy (window_egl->display.shell);

  if (window_egl->display.seat)
    wl_seat_destroy (window_egl->display.seat);

  wl_proxy_wrapper_destroy (window_egl->display.display);
  wl_event_queue_destroy (window_egl->window.queue);

  GST_GL_WINDOW_CLASS (parent_class)->close (gl_window);
}

static void
handle_xdg_wm_base_ping (void *user_data, struct xdg_wm_base *xdg_wm_base,
    uint32_t serial)
{
  xdg_wm_base_pong (xdg_wm_base, serial);
}

static const struct xdg_wm_base_listener xdg_wm_base_listener = {
  handle_xdg_wm_base_ping
};

static void
registry_handle_global (void *data, struct wl_registry *registry,
    uint32_t name, const char *interface, uint32_t version)
{
  GstGLWindowWaylandEGL *window_wayland = data;

  GST_TRACE_OBJECT (window_wayland, "registry_handle_global with registry %p, "
      "name %" G_GUINT32_FORMAT ", interface %s, version %u", registry, name,
      interface, version);

  if (g_strcmp0 (interface, "wl_compositor") == 0) {
    window_wayland->display.compositor =
        wl_registry_bind (registry, name, &wl_compositor_interface, 1);
  } else if (g_strcmp0 (interface, "wl_subcompositor") == 0) {
    window_wayland->display.subcompositor =
        wl_registry_bind (registry, name, &wl_subcompositor_interface, 1);
  } else if (g_strcmp0 (interface, "xdg_wm_base") == 0) {
    window_wayland->display.xdg_wm_base =
        wl_registry_bind (registry, name, &xdg_wm_base_interface, 1);
    xdg_wm_base_add_listener (window_wayland->display.xdg_wm_base,
        &xdg_wm_base_listener, window_wayland);
  } else if (g_strcmp0 (interface, "wl_shell") == 0) {
    window_wayland->display.shell =
        wl_registry_bind (registry, name, &wl_shell_interface, 1);
  } else if (g_strcmp0 (interface, "wl_seat") == 0) {
    window_wayland->display.seat =
        wl_registry_bind (registry, name, &wl_seat_interface, 4);
    wl_seat_add_listener (window_wayland->display.seat, &seat_listener,
        window_wayland);
  }
}

static void
registry_handle_global_remove (void *data, struct wl_registry *registry,
    uint32_t name)
{
  GstGLWindowWaylandEGL *window_wayland = data;

  /* TODO: deal with any registry objects that may be removed */
  GST_TRACE_OBJECT (window_wayland, "wl_registry %p global_remove %"
      G_GUINT32_FORMAT, registry, name);
}

static const struct wl_registry_listener registry_listener = {
  registry_handle_global,
  registry_handle_global_remove,
};

static gboolean
gst_gl_window_wayland_egl_open (GstGLWindow * window, GError ** error)
{
  GstGLDisplayWayland *display;
  GstGLWindowWaylandEGL *window_egl = GST_GL_WINDOW_WAYLAND_EGL (window);

  if (!GST_IS_GL_DISPLAY_WAYLAND (window->display)) {
    g_set_error (error, GST_GL_WINDOW_ERROR,
        GST_GL_WINDOW_ERROR_RESOURCE_UNAVAILABLE,
        "Failed to retrieve Wayland display (wrong type?)");
    return FALSE;
  }
  display = GST_GL_DISPLAY_WAYLAND (window->display);

  if (!display->display) {
    g_set_error (error, GST_GL_WINDOW_ERROR,
        GST_GL_WINDOW_ERROR_RESOURCE_UNAVAILABLE,
        "Failed to retrieve Wayland display");
    return FALSE;
  }

  /* we create a proxy wrapper for the display so that we can set the queue on
   * it. This allows us to avoid sprinkling `wl_proxy_set_queue()` calls for
   * each wayland resource we create as well as removing a race between
   * creation and the `wl_proxy_set_queue()` call. */
  window_egl->display.display = wl_proxy_create_wrapper (display->display);
  window_egl->window.queue = wl_display_create_queue (display->display);

  wl_proxy_set_queue ((struct wl_proxy *) window_egl->display.display,
      window_egl->window.queue);

  window_egl->display.registry =
      wl_display_get_registry (window_egl->display.display);
  wl_registry_add_listener (window_egl->display.registry, &registry_listener,
      window_egl);

  if (wl_display_roundtrip_queue (display->display,
          window_egl->window.queue) < 0) {
    g_set_error (error, GST_GL_WINDOW_ERROR,
        GST_GL_WINDOW_ERROR_RESOURCE_UNAVAILABLE,
        "Failed to perform a wayland roundtrip");
    return FALSE;
  }

  window_egl->wl_source = wayland_event_source_new (display->display,
      window_egl->window.queue);

  if (!GST_GL_WINDOW_CLASS (parent_class)->open (window, error))
    return FALSE;

  g_source_attach (window_egl->wl_source, window->main_context);

  return TRUE;
}

void
gst_gl_window_wayland_egl_create_window (GstGLWindowWaylandEGL * window_egl)
{
  create_surfaces (window_egl);
}

static guintptr
gst_gl_window_wayland_egl_get_window_handle (GstGLWindow * window)
{
  return (guintptr) GST_GL_WINDOW_WAYLAND_EGL (window)->window.native;
}

static void
gst_gl_window_wayland_egl_set_window_handle (GstGLWindow * window,
    guintptr handle)
{
  GstGLWindowWaylandEGL *window_egl = GST_GL_WINDOW_WAYLAND_EGL (window);
  struct wl_surface *surface = (struct wl_surface *) handle;

  /* already set the NULL handle */
  if (surface == NULL && window_egl->window.foreign_surface == NULL)
    return;

  /* unparent */
  destroy_surfaces (window_egl);
  window_egl->window.foreign_surface = surface;
  create_surfaces (window_egl);
}

static void
_roundtrip_async (GstGLWindow * window)
{
  GstGLDisplayWayland *display = GST_GL_DISPLAY_WAYLAND (window->display);
  GstGLWindowWaylandEGL *window_egl = GST_GL_WINDOW_WAYLAND_EGL (window);

  create_surfaces (window_egl);

  if (wl_display_roundtrip_queue (display->display,
          window_egl->window.queue) < 0)
    GST_WARNING_OBJECT (window, "failed a roundtrip");
}

static void
gst_gl_window_wayland_egl_show (GstGLWindow * window)
{
  gst_gl_window_send_message (window, (GstGLWindowCB) _roundtrip_async, window);
}

static void
window_resize (GstGLWindowWaylandEGL * window_egl, guint width, guint height)
{
  GstGLWindow *window = GST_GL_WINDOW (window_egl);

  GST_DEBUG ("resizing window from %ux%u to %ux%u",
      window_egl->window.window_width, window_egl->window.window_height, width,
      height);

  if (window_egl->window.native) {
    wl_egl_window_resize (window_egl->window.native, width, height, 0, 0);
  }

  gst_gl_window_resize (window, width, height);

  window_egl->window.window_width = width;
  window_egl->window.window_height = height;
}

static void
draw_cb (gpointer data)
{
  GstGLWindowWaylandEGL *window_egl = data;
  GstGLWindow *window = GST_GL_WINDOW (window_egl);
  GstGLContext *context = gst_gl_window_get_context (window);

  create_surfaces (window_egl);

  if (window_egl->window.subsurface)
    wl_subsurface_set_desync (window_egl->window.subsurface);

  if (window->queue_resize) {
    guint width, height;

    gst_gl_window_get_surface_dimensions (window, &width, &height);
    gst_gl_window_resize (window, width, height);
  }

  if (window->draw)
    window->draw (window->draw_data);

  gst_gl_context_swap_buffers (context);

  if (window_egl->window.subsurface)
    wl_subsurface_set_desync (window_egl->window.subsurface);

  gst_object_unref (context);
}

static void
gst_gl_window_wayland_egl_draw (GstGLWindow * window)
{
  gst_gl_window_send_message (window, (GstGLWindowCB) draw_cb, window);
}

struct SetRenderRectangle
{
  GstGLWindowWaylandEGL *window_egl;
  GstVideoRectangle rect;
};

static void
_free_set_render_rectangle (struct SetRenderRectangle *render)
{
  if (render) {
    if (render->window_egl)
      gst_object_unref (render->window_egl);
    g_free (render);
  }
}

static void
_set_render_rectangle (gpointer data)
{
  struct SetRenderRectangle *render = data;

  GST_LOG_OBJECT (render->window_egl, "setting render rectangle %i,%i+%ix%i",
      render->rect.x, render->rect.y, render->rect.w, render->rect.h);

  if (render->window_egl->window.subsurface) {
    wl_subsurface_set_sync (render->window_egl->window.subsurface);
    wl_subsurface_set_position (render->window_egl->window.subsurface,
        render->rect.x, render->rect.y);
    render->window_egl->window.window_x = render->rect.x;
    render->window_egl->window.window_y = render->rect.y;
  }

  window_resize (render->window_egl, render->rect.w, render->rect.h);

  render->window_egl->window.render_rect = render->rect;
}

static gboolean
gst_gl_window_wayland_egl_set_render_rectangle (GstGLWindow * window,
    gint x, gint y, gint width, gint height)
{
  GstGLWindowWaylandEGL *window_egl = GST_GL_WINDOW_WAYLAND_EGL (window);
  struct SetRenderRectangle *render;

  render = g_new0 (struct SetRenderRectangle, 1);
  render->window_egl = gst_object_ref (window_egl);
  render->rect.x = x;
  render->rect.y = y;
  render->rect.w = width;
  render->rect.h = height;

  gst_gl_window_send_message_async (window,
      (GstGLWindowCB) _set_render_rectangle, render,
      (GDestroyNotify) _free_set_render_rectangle);

  return TRUE;
}

static void
gst_gl_window_wayland_egl_set_preferred_size (GstGLWindow * window, gint width,
    gint height)
{
  GstGLWindowWaylandEGL *window_egl = GST_GL_WINDOW_WAYLAND_EGL (window);

  window_egl->window.preferred_width = width;
  window_egl->window.preferred_height = height;
  if (window_egl->window.render_rect.w < 0
      && window_egl->window.render_rect.h < 0) {
    if (window_egl->window.window_height != height
        || window_egl->window.window_width != width) {
      window_resize (window_egl, width, height);
    }
  }
}

static guintptr
gst_gl_window_wayland_egl_get_display (GstGLWindow * window)
{
  GstGLDisplayWayland *display = GST_GL_DISPLAY_WAYLAND (window->display);

  return (guintptr) display->display;
}
