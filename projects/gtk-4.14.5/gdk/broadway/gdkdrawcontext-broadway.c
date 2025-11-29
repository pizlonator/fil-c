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

#include "gdkdrawcontext-broadway.h"
#include "gdktextureprivate.h"
#include "gdkprivate-broadway.h"

G_DEFINE_TYPE (GdkBroadwayDrawContext, gdk_broadway_draw_context, GDK_TYPE_DRAW_CONTEXT)

static void
gdk_broadway_draw_context_dispose (GObject *object)
{
  G_OBJECT_CLASS (gdk_broadway_draw_context_parent_class)->dispose (object);
}

static void
gdk_broadway_draw_context_begin_frame (GdkDrawContext *draw_context,
                                       GdkMemoryDepth  depth,
                                       cairo_region_t *region)
{
  GdkBroadwayDrawContext *self = GDK_BROADWAY_DRAW_CONTEXT (draw_context);
  GdkSurface *surface = gdk_draw_context_get_surface (GDK_DRAW_CONTEXT (self));

  cairo_region_union_rectangle (region,
                                &(cairo_rectangle_int_t) {
                                    0, 0,
                                    gdk_surface_get_width (surface),
                                    gdk_surface_get_height (surface)
                                 });

  g_assert (self->nodes == NULL);
  g_assert (self->node_textures == NULL);

  self->nodes = g_array_new (FALSE, FALSE, sizeof(guint32));
  self->node_textures = g_ptr_array_new_with_free_func (g_object_unref);
}

static void
gdk_broadway_draw_context_end_frame (GdkDrawContext *draw_context,
                                     cairo_region_t *painted)
{
  GdkBroadwayDrawContext *self = GDK_BROADWAY_DRAW_CONTEXT (draw_context);
  GdkSurface *surface = gdk_draw_context_get_surface (draw_context);

  gdk_broadway_surface_set_nodes (surface, self->nodes, self->node_textures);

  g_array_unref (self->nodes);
  self->nodes = NULL;

  /* We now sent all new texture refs to the daemon via the nodes, so we can drop them here */
  g_ptr_array_unref (self->node_textures);
  self->node_textures = NULL;
}

static void
gdk_broadway_draw_context_surface_resized (GdkDrawContext *draw_context)
{
}

static void
gdk_broadway_draw_context_class_init (GdkBroadwayDrawContextClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GdkDrawContextClass *draw_context_class = GDK_DRAW_CONTEXT_CLASS (klass);

  gobject_class->dispose = gdk_broadway_draw_context_dispose;

  draw_context_class->begin_frame = gdk_broadway_draw_context_begin_frame;
  draw_context_class->end_frame = gdk_broadway_draw_context_end_frame;
  draw_context_class->surface_resized = gdk_broadway_draw_context_surface_resized;
}

static void
gdk_broadway_draw_context_init (GdkBroadwayDrawContext *self)
{
}

GdkBroadwayDrawContext *
gdk_broadway_draw_context_context (GdkSurface *surface)
{
  return g_object_new (GDK_TYPE_BROADWAY_DRAW_CONTEXT,
                       "surface", surface,
                       NULL);
}
