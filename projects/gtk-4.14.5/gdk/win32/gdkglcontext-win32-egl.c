/* GDK - The GIMP Drawing Kit
 *
 * gdkglcontext-win32.c: Win32 specific OpenGL wrappers
 *
 * Copyright © 2014 Emmanuele Bassi
 * Copyright © 2014 Alexander Larsson
 * Copyright © 2014 Chun-wei Fan
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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include "gdkprivate-win32.h"
#include "gdksurface-win32.h"
#include "gdkglcontext-win32.h"
#include "gdkdisplay-win32.h"

#include "gdkwin32display.h"
#include "gdkwin32glcontext.h"
#include "gdkwin32misc.h"
#include "gdkwin32screen.h"
#include "gdkwin32surface.h"

#include "gdkglcontext.h"
#include "gdksurface.h"
#include <glib/gi18n-lib.h>

#include <cairo.h>
#include <epoxy/egl.h>

struct _GdkWin32GLContextEGL
{
  GdkWin32GLContext parent_instance;

  /* EGL (Angle) Context Items */
  EGLContext egl_context;
  guint do_frame_sync : 1;
};

typedef struct _GdkWin32GLContextClass    GdkWin32GLContextEGLClass;

G_DEFINE_TYPE (GdkWin32GLContextEGL, gdk_win32_gl_context_egl, GDK_TYPE_WIN32_GL_CONTEXT)

static gboolean
is_egl_force_redraw (GdkSurface *surface)
{
  /* We only need to call gdk_window_invalidate_rect () if necessary */
  if (surface->gl_paint_context != NULL && gdk_gl_context_get_use_es (surface->gl_paint_context))
    {
      GdkWin32Surface *impl = GDK_WIN32_SURFACE (surface);

      return impl->egl_force_redraw_all;
    }
  return FALSE;
}

static void
reset_egl_force_redraw (GdkSurface *surface)
{
  if (surface->gl_paint_context != NULL && gdk_gl_context_get_use_es (surface->gl_paint_context))
    {
      GdkWin32Surface *impl = GDK_WIN32_SURFACE (surface);

      if (impl->egl_force_redraw_all)
        impl->egl_force_redraw_all = FALSE;
    }
}

static void
gdk_win32_gl_context_egl_end_frame (GdkDrawContext *draw_context,
                                    cairo_region_t *painted)
{
  GdkGLContext *context = GDK_GL_CONTEXT (draw_context);
  GdkSurface *surface = gdk_gl_context_get_surface (context);
  GdkDisplay *display = gdk_gl_context_get_display (context);
  EGLSurface egl_surface;

  GDK_DRAW_CONTEXT_CLASS (gdk_win32_gl_context_egl_parent_class)->end_frame (draw_context, painted);

  gdk_gl_context_make_current (context);

  egl_surface = gdk_surface_get_egl_surface (surface);

  if (is_egl_force_redraw (surface))
    {
      GdkRectangle rect = {0, 0, gdk_surface_get_width (surface), gdk_surface_get_height (surface)};

      /* We need to do gdk_window_invalidate_rect() so that we don't get glitches after maximizing or
       *  restoring or using aerosnap
       */
      gdk_surface_invalidate_rect (surface, &rect);
      reset_egl_force_redraw (surface);
    }

  eglSwapBuffers (gdk_display_get_egl_display (display), egl_surface);
}

static void
gdk_win32_gl_context_egl_begin_frame (GdkDrawContext *draw_context,
                                      GdkMemoryDepth  depth,
                                      cairo_region_t *update_area)
{
  gdk_win32_surface_handle_queued_move_resize (draw_context);

  GDK_DRAW_CONTEXT_CLASS (gdk_win32_gl_context_egl_parent_class)->begin_frame (draw_context, depth, update_area);
}

static void
gdk_win32_gl_context_egl_empty_frame (GdkDrawContext *draw_context)
{
}

static void
gdk_win32_gl_context_egl_class_init (GdkWin32GLContextClass *klass)
{
  GdkGLContextClass *context_class = GDK_GL_CONTEXT_CLASS(klass);
  GdkDrawContextClass *draw_context_class = GDK_DRAW_CONTEXT_CLASS(klass);

  context_class->backend_type = GDK_GL_EGL;

  draw_context_class->begin_frame = gdk_win32_gl_context_egl_begin_frame;
  draw_context_class->end_frame = gdk_win32_gl_context_egl_end_frame;
  draw_context_class->empty_frame = gdk_win32_gl_context_egl_empty_frame;
}

static void
gdk_win32_gl_context_egl_init (GdkWin32GLContextEGL *egl_context)
{
}
