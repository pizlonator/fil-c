/* GDK - The GIMP Drawing Kit
 *
 * gdkglcontext-egl.c: EGL-X11 specific wrappers
 *
 * SPDX-FileCopyrightText: 2014  Emmanuele Bassi
 * SPDX-FileCopyrightText: 2021  GNOME Foundation
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "config.h"

#include "gdkglcontext-x11.h"
#include "gdkdisplay-x11.h"
#include "gdkprivate-x11.h"
#include "gdkscreen-x11.h"

#include "gdkx11display.h"
#include "gdkx11glcontext.h"
#include "gdkx11screen.h"
#include "gdkx11property.h"
#include <X11/Xatom.h>

#include "gdkprofilerprivate.h"
#include <glib/gi18n-lib.h>

#include <cairo-xlib.h>

#include <epoxy/egl.h>

struct _GdkX11GLContextEGL
{
  GdkX11GLContext parent_instance;

  guint do_frame_sync : 1;
};

typedef struct _GdkX11GLContextClass    GdkX11GLContextEGLClass;

G_DEFINE_TYPE (GdkX11GLContextEGL, gdk_x11_gl_context_egl, GDK_TYPE_X11_GL_CONTEXT)

/**
 * gdk_x11_display_get_egl_display:
 * @display: (type GdkX11Display): an X11 display
 *
 * Retrieves the EGL display connection object for the given GDK display.
 *
 * This function returns `NULL` if GDK is using GLX.
 *
 * Returns: (nullable): the EGL display object
 *
 * Since: 4.4
 */
gpointer
gdk_x11_display_get_egl_display (GdkDisplay *display)
{
  g_return_val_if_fail (GDK_IS_X11_DISPLAY (display), NULL);

  return gdk_display_get_egl_display (display);
}

static gboolean
gdk_x11_gl_context_egl_make_current (GdkGLContext *context,
                                     gboolean      surfaceless)
{
  GdkX11GLContextEGL *self = GDK_X11_GL_CONTEXT_EGL (context);
  GdkDisplay *display = gdk_gl_context_get_display (context);
  EGLDisplay egl_display = gdk_display_get_egl_display (display);
  gboolean do_frame_sync = FALSE;

  if (!GDK_GL_CONTEXT_CLASS (gdk_x11_gl_context_egl_parent_class)->make_current (context, surfaceless))
    return FALSE;

  if (surfaceless)
    return TRUE;

  /* If the WM is compositing there is no particular need to delay
   * the swap when drawing on the offscreen, rendering to the screen
   * happens later anyway, and its up to the compositor to sync that
   * to the vblank. */
  do_frame_sync = ! gdk_display_is_composited (display);

  if (do_frame_sync != self->do_frame_sync)
    {
      self->do_frame_sync = do_frame_sync;

      if (do_frame_sync)
        eglSwapInterval (egl_display, 1);
      else
        eglSwapInterval (egl_display, 0);
    }

  return TRUE;
}

static void
gdk_x11_gl_context_egl_class_init (GdkX11GLContextEGLClass *klass)
{
  GdkGLContextClass *context_class = GDK_GL_CONTEXT_CLASS (klass);

  context_class->backend_type = GDK_GL_EGL;

  context_class->make_current = gdk_x11_gl_context_egl_make_current;
}

static void
gdk_x11_gl_context_egl_init (GdkX11GLContextEGL *self)
{
  self->do_frame_sync = TRUE;
}

/**
 * gdk_x11_display_get_egl_version:
 * @display: (type GdkX11Display): a `GdkDisplay`
 * @major: (out): return location for the EGL major version
 * @minor: (out): return location for the EGL minor version
 *
 * Retrieves the version of the EGL implementation.
 *
 * Returns: %TRUE if EGL is available
 *
 * Since: 4.4
 */
gboolean
gdk_x11_display_get_egl_version (GdkDisplay *display,
                                 int        *major,
                                 int        *minor)
{
  g_return_val_if_fail (GDK_IS_DISPLAY (display), FALSE);

  if (!GDK_IS_X11_DISPLAY (display))
    return FALSE;

  GdkX11Display *display_x11 = GDK_X11_DISPLAY (display);

  if (gdk_display_get_egl_display (display) == NULL)
    return FALSE;

  if (major != NULL)
    *major = display_x11->egl_version / 10;
  if (minor != NULL)
    *minor = display_x11->egl_version % 10;

  return TRUE;
}
