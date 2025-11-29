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

#include "gdkcairocontext-wayland.h"

#include "gdkprivate-wayland.h"
#include "gdksurface-wayland-private.h"

#include "gdkprofilerprivate.h"

static const cairo_user_data_key_t gdk_wayland_cairo_context_key;
static const cairo_user_data_key_t gdk_wayland_cairo_region_key;

G_DEFINE_TYPE (GdkWaylandCairoContext, gdk_wayland_cairo_context, GDK_TYPE_CAIRO_CONTEXT)

static void
gdk_wayland_cairo_context_surface_add_region (cairo_surface_t      *surface,
                                              const cairo_region_t *region)
{
  cairo_region_t *surface_region;

  surface_region = cairo_surface_get_user_data (surface, &gdk_wayland_cairo_region_key);
  if (surface_region == NULL)
    {
      surface_region = cairo_region_copy (region);
      cairo_surface_set_user_data (surface,
                                   &gdk_wayland_cairo_region_key,
                                   surface_region,
                                   (cairo_destroy_func_t) cairo_region_destroy);
    }
  else
    {
      cairo_region_union (surface_region, region);
    }
}

static void
gdk_wayland_cairo_context_surface_clear_region (cairo_surface_t *surface)
{
  cairo_surface_set_user_data (surface, &gdk_wayland_cairo_region_key, NULL, NULL);
}

static const cairo_region_t *
gdk_wayland_cairo_context_surface_get_region (cairo_surface_t *surface)
{
  return cairo_surface_get_user_data (surface, &gdk_wayland_cairo_region_key);
}

static GdkWaylandCairoContext *
gdk_wayland_cairo_context_get_from_surface (cairo_surface_t *surface)
{
  return cairo_surface_get_user_data (surface, &gdk_wayland_cairo_context_key);
}

static void
gdk_wayland_cairo_context_add_surface (GdkWaylandCairoContext *self,
                                       cairo_surface_t        *surface)
{
  cairo_surface_reference (surface);
  cairo_surface_set_user_data (surface, &gdk_wayland_cairo_context_key, self, NULL);

  self->surfaces = g_slist_prepend (self->surfaces, surface);
}

static void
gdk_wayland_cairo_context_remove_surface (GdkWaylandCairoContext *self,
                                          cairo_surface_t        *surface)
{
  self->surfaces = g_slist_remove (self->surfaces, surface);

  cairo_surface_set_user_data (surface, &gdk_wayland_cairo_context_key, NULL, NULL);
  cairo_surface_destroy (surface);
}

static void
gdk_wayland_cairo_context_buffer_release (void             *_data,
                                          struct wl_buffer *wl_buffer)
{
  cairo_surface_t *cairo_surface = _data;
  GdkWaylandCairoContext *self = gdk_wayland_cairo_context_get_from_surface (cairo_surface);

  /* context was destroyed before compositor released this buffer */
  if (self == NULL)
    return;

  /* Cache one surface for reuse when drawing */
  if (self->cached_surface == NULL)
    {
      self->cached_surface = cairo_surface;
      return;
    }

  /* Get rid of all the extra ones */
  gdk_wayland_cairo_context_remove_surface (self, cairo_surface);
  /* Release the reference the compositor held to this surface */
  cairo_surface_destroy (cairo_surface);
}

static const struct wl_buffer_listener buffer_listener = {
  gdk_wayland_cairo_context_buffer_release
};

static cairo_surface_t *
gdk_wayland_cairo_context_create_surface (GdkWaylandCairoContext *self)
{
  GdkWaylandDisplay *display_wayland = GDK_WAYLAND_DISPLAY (gdk_draw_context_get_display (GDK_DRAW_CONTEXT (self)));
  GdkSurface *surface = gdk_draw_context_get_surface (GDK_DRAW_CONTEXT (self));
  cairo_surface_t *cairo_surface;
  struct wl_buffer *buffer;
  cairo_region_t *region;
  int width, height;

  width = gdk_surface_get_width (surface);
  height = gdk_surface_get_height (surface);
  cairo_surface = gdk_wayland_display_create_shm_surface (display_wayland,
                                                          width, height,
                                                          &GDK_WAYLAND_SURFACE (surface)->scale);
  buffer = _gdk_wayland_shm_surface_get_wl_buffer (cairo_surface);
  wl_buffer_add_listener (buffer, &buffer_listener, cairo_surface);
  gdk_wayland_cairo_context_add_surface (self, cairo_surface);

  region = cairo_region_create_rectangle (&(cairo_rectangle_int_t) { 0, 0, width, height });
  gdk_wayland_cairo_context_surface_add_region (cairo_surface, region);
  cairo_region_destroy (region);

  return cairo_surface;
}

static void
gdk_wayland_cairo_context_begin_frame (GdkDrawContext *draw_context,
                                       GdkMemoryDepth  depth,
                                       cairo_region_t *region)
{
  GdkWaylandCairoContext *self = GDK_WAYLAND_CAIRO_CONTEXT (draw_context);
  const cairo_region_t *surface_region;
  GSList *l;
  cairo_t *cr;

  if (self->cached_surface)
    self->paint_surface = g_steal_pointer (&self->cached_surface);
  else
    self->paint_surface = gdk_wayland_cairo_context_create_surface (self);

  surface_region = gdk_wayland_cairo_context_surface_get_region (self->paint_surface);
  if (surface_region)
    cairo_region_union (region, surface_region);

  for (l = self->surfaces; l; l = l->next)
    {
      gdk_wayland_cairo_context_surface_add_region (l->data, region);
    }

  /* clear the repaint area */
  cr = cairo_create (self->paint_surface);
  cairo_set_operator (cr, CAIRO_OPERATOR_CLEAR);
  gdk_cairo_region (cr, region);
  cairo_fill (cr);
  cairo_destroy (cr);
}

static void
gdk_wayland_cairo_context_end_frame (GdkDrawContext *draw_context,
                                     cairo_region_t *painted)
{
  GdkWaylandCairoContext *self = GDK_WAYLAND_CAIRO_CONTEXT (draw_context);
  GdkSurface *surface = gdk_draw_context_get_surface (draw_context);

  gdk_wayland_surface_sync (surface);
  gdk_wayland_surface_attach_image (surface, self->paint_surface, painted);
  gdk_wayland_surface_request_frame (surface);

  gdk_profiler_add_mark (GDK_PROFILER_CURRENT_TIME, 0, "Wayland surface commit", NULL);
  gdk_wayland_surface_commit (surface);
  gdk_wayland_surface_notify_committed (surface);

  gdk_wayland_cairo_context_surface_clear_region (self->paint_surface);
  self->paint_surface = NULL;
}

static void
gdk_wayland_cairo_context_empty_frame (GdkDrawContext *draw_context)
{
  GdkSurface *surface = gdk_draw_context_get_surface (draw_context);
  GdkWaylandSurface *impl = GDK_WAYLAND_SURFACE (surface);

  if (!impl->has_pending_subsurface_commits)
    return;

  gdk_wayland_surface_sync (surface);
  gdk_wayland_surface_request_frame (surface);

  gdk_profiler_add_mark (GDK_PROFILER_CURRENT_TIME, 0, "Wayland surface commit", NULL);
  gdk_wayland_surface_commit (surface);
  gdk_wayland_surface_notify_committed (surface);
}

static void
gdk_wayland_cairo_context_clear_all_cairo_surfaces (GdkWaylandCairoContext *self)
{
  g_clear_pointer (&self->cached_surface, cairo_surface_destroy);
  while (self->surfaces)
    gdk_wayland_cairo_context_remove_surface (self, self->surfaces->data);
}

static void
gdk_wayland_cairo_context_surface_resized (GdkDrawContext *draw_context)
{
  GdkWaylandCairoContext *self = GDK_WAYLAND_CAIRO_CONTEXT (draw_context);

  gdk_wayland_cairo_context_clear_all_cairo_surfaces (self);
}

static cairo_t *
gdk_wayland_cairo_context_cairo_create (GdkCairoContext *context)
{
  GdkWaylandCairoContext *self = GDK_WAYLAND_CAIRO_CONTEXT (context);

  return cairo_create (self->paint_surface);
}

static void
gdk_wayland_cairo_context_dispose (GObject *object)
{
  GdkWaylandCairoContext *self = GDK_WAYLAND_CAIRO_CONTEXT (object);

  gdk_wayland_cairo_context_clear_all_cairo_surfaces (self);
  g_assert (self->cached_surface == NULL);
  g_assert (self->paint_surface == NULL);

  G_OBJECT_CLASS (gdk_wayland_cairo_context_parent_class)->dispose (object);
}

static void
gdk_wayland_cairo_context_class_init (GdkWaylandCairoContextClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GdkDrawContextClass *draw_context_class = GDK_DRAW_CONTEXT_CLASS (klass);
  GdkCairoContextClass *cairo_context_class = GDK_CAIRO_CONTEXT_CLASS (klass);

  gobject_class->dispose = gdk_wayland_cairo_context_dispose;

  draw_context_class->begin_frame = gdk_wayland_cairo_context_begin_frame;
  draw_context_class->end_frame = gdk_wayland_cairo_context_end_frame;
  draw_context_class->empty_frame = gdk_wayland_cairo_context_empty_frame;
  draw_context_class->surface_resized = gdk_wayland_cairo_context_surface_resized;

  cairo_context_class->cairo_create = gdk_wayland_cairo_context_cairo_create;
}

static void
gdk_wayland_cairo_context_init (GdkWaylandCairoContext *self)
{
}

