/* GDK - The GIMP Drawing Kit
 *
 * Copyright © 2018  Benjamin Otte
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

#include "gdkconfig.h"

#include "gdkcairocontext-win32.h"
#include "gdkprivate-win32.h"
#include "gdksurface-win32.h"
#include "gdkwin32misc.h"

#include <cairo-win32.h>

#include <windows.h>

G_DEFINE_TYPE (GdkWin32CairoContext, gdk_win32_cairo_context, GDK_TYPE_CAIRO_CONTEXT)

static cairo_surface_t *
create_cairo_surface_for_surface (GdkSurface *surface,
                                  int         scale)
{
  cairo_surface_t *cairo_surface;
  HDC hdc;

  hdc = GetDC (GDK_SURFACE_HWND (surface));
  if (!hdc)
    {
      WIN32_GDI_FAILED ("GetDC");
      return NULL;
    }

  cairo_surface = cairo_win32_surface_create_with_format (hdc, CAIRO_FORMAT_ARGB32);
  cairo_surface_set_device_scale (cairo_surface, scale, scale);

  return cairo_surface;
}

static void
gdk_win32_cairo_context_begin_frame (GdkDrawContext *draw_context,
                                     GdkMemoryDepth  depth,
                                     cairo_region_t *region)
{
  GdkWin32CairoContext *self = GDK_WIN32_CAIRO_CONTEXT (draw_context);
  GdkSurface *surface;
  int scale;
  cairo_t *cr;
  int width, height;
  RECT queued_window_rect;

  surface = gdk_draw_context_get_surface (draw_context);
  scale = gdk_surface_get_scale_factor (surface);

  queued_window_rect = gdk_win32_surface_handle_queued_move_resize (draw_context);

  width = queued_window_rect.right - queued_window_rect.left;
  height = queued_window_rect.bottom - queued_window_rect.top;
  width = MAX (width, 1);
  height = MAX (height, 1);

  self->window_surface = create_cairo_surface_for_surface (surface, scale);

  if (!self->double_buffered)
    /* Non-double-buffered windows paint on the window surface directly */
    self->paint_surface = cairo_surface_reference (self->window_surface);
  else
    {
      if (width > self->db_width ||
          height > self->db_height)
        {
          self->db_width = MAX (width, self->db_width);
          self->db_height = MAX (height, self->db_height);

          g_clear_pointer (&self->db_surface, cairo_surface_destroy);

G_GNUC_BEGIN_IGNORE_DEPRECATIONS
          self->db_surface = gdk_surface_create_similar_surface (surface,
                                                                 cairo_surface_get_content (self->window_surface),
                                                                 self->db_width,
                                                                 self->db_height);
G_GNUC_END_IGNORE_DEPRECATIONS
        }

      /* Double-buffered windows paint on a DB surface.
       * Due to performance concerns we don't recreate it unless forced to.
       */
      self->paint_surface = cairo_surface_reference (self->db_surface);
    }

  /* Clear the paint region.
   * For non-double-buffered rendering we must clear it, otherwise
   * semi-transparent pixels will "add up" with each repaint.
   * We must also clear the old pixels from the DB cache surface
   * that we're going to use as a buffer.
   */
  cr = cairo_create (self->paint_surface);
  cairo_set_source_rgba (cr, 0, 0, 0, 00);
  cairo_set_operator (cr, CAIRO_OPERATOR_SOURCE);
  gdk_cairo_region (cr, region);
  cairo_clip (cr);
  cairo_paint (cr);
  cairo_destroy (cr);
}

static void
gdk_win32_cairo_context_end_frame (GdkDrawContext *draw_context,
                                   cairo_region_t *painted)
{
  GdkWin32CairoContext *self = GDK_WIN32_CAIRO_CONTEXT (draw_context);

  /* The code to resize double-buffered windows immediately
   * before blitting the buffer contents onto them used
   * to be here.
   */

  /* For double-buffered windows we need to blit
   * the DB buffer contents into the window itself.
   */
  if (self->double_buffered)
    {
      cairo_t *cr;

      cr = cairo_create (self->window_surface);

      cairo_set_source_surface (cr, self->paint_surface, 0, 0);
      gdk_cairo_region (cr, painted);
      cairo_clip (cr);

      cairo_set_operator (cr, CAIRO_OPERATOR_SOURCE);
      cairo_paint (cr);

      cairo_destroy (cr);
    }

  cairo_surface_flush (self->window_surface);

  g_clear_pointer (&self->paint_surface, cairo_surface_destroy);
  g_clear_pointer (&self->window_surface, cairo_surface_destroy);
}

static void
gdk_win32_cairo_context_empty_frame (GdkDrawContext *draw_context)
{
}

static cairo_t *
gdk_win32_cairo_context_cairo_create (GdkCairoContext *context)
{
  GdkWin32CairoContext *self = GDK_WIN32_CAIRO_CONTEXT (context);

  return cairo_create (self->paint_surface);
}

static void
gdk_win32_cairo_context_finalize (GObject *object)
{
  GdkWin32CairoContext *self = GDK_WIN32_CAIRO_CONTEXT (object);

  g_clear_pointer (&self->db_surface, cairo_surface_destroy);

  G_OBJECT_CLASS (gdk_win32_cairo_context_parent_class)->finalize (object);
}

static void
gdk_win32_cairo_context_class_init (GdkWin32CairoContextClass *klass)
{
  GdkDrawContextClass *draw_context_class = GDK_DRAW_CONTEXT_CLASS (klass);
  GdkCairoContextClass *cairo_context_class = GDK_CAIRO_CONTEXT_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gdk_win32_cairo_context_finalize;

  draw_context_class->begin_frame = gdk_win32_cairo_context_begin_frame;
  draw_context_class->end_frame = gdk_win32_cairo_context_end_frame;
  draw_context_class->empty_frame = gdk_win32_cairo_context_empty_frame;

  cairo_context_class->cairo_create = gdk_win32_cairo_context_cairo_create;
}

static void
gdk_win32_cairo_context_init (GdkWin32CairoContext *self)
{
  self->double_buffered = g_strcmp0 (g_getenv ("GDK_WIN32_CAIRO_DB"), "1") == 0;
  self->db_width = -1;
  self->db_height = -1;
}

