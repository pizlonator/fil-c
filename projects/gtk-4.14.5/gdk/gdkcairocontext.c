/* GDK - The GIMP Drawing Kit
 *
 * gdkcairocontext.c: Cairo wrappers
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

#include "gdkcairocontext.h"

#include "gdkcairocontextprivate.h"

#include "gdkcairo.h"

/**
 * GdkCairoContext:
 *
 * `GdkCairoContext` is an object representing the platform-specific
 * draw context.
 *
 * `GdkCairoContext`s are created for a surface using
 * [method@Gdk.Surface.create_cairo_context], and the context
 * can then be used to draw on that surface.
 */

typedef struct _GdkCairoContextPrivate GdkCairoContextPrivate;

struct _GdkCairoContextPrivate {
  gpointer unused;
};

G_DEFINE_ABSTRACT_TYPE_WITH_CODE (GdkCairoContext, gdk_cairo_context, GDK_TYPE_DRAW_CONTEXT,
                                  G_ADD_PRIVATE (GdkCairoContext))

static void
gdk_cairo_context_class_init (GdkCairoContextClass *klass)
{
}

static void
gdk_cairo_context_init (GdkCairoContext *self)
{
}

/**
 * gdk_cairo_context_cairo_create:
 * @self: a `GdkCairoContext` that is currently drawing
 *
 * Retrieves a Cairo context to be used to draw on the `GdkSurface`
 * of @context.
 *
 * A call to [method@Gdk.DrawContext.begin_frame] with this
 * @context must have been done or this function will return %NULL.
 *
 * The returned context is guaranteed to be valid until
 * [method@Gdk.DrawContext.end_frame] is called.
 *
 * Returns: (transfer full) (nullable): a Cairo context
 *   to draw on `GdkSurface
 */
cairo_t *
gdk_cairo_context_cairo_create (GdkCairoContext *self)
{
  GdkDrawContext *draw_context;
  cairo_t *cr;

  g_return_val_if_fail (GDK_IS_CAIRO_CONTEXT (self), NULL);

  draw_context = GDK_DRAW_CONTEXT (self);

  if (!gdk_draw_context_is_in_frame (draw_context))
    return NULL;

  cr = GDK_CAIRO_CONTEXT_GET_CLASS (self)->cairo_create (self);

  gdk_cairo_region (cr, gdk_draw_context_get_frame_region (draw_context));
  cairo_clip (cr);

  return cr;
}

