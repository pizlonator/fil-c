/* GDK - The GIMP Drawing Kit
 *
 * gdkdrawcontext.c: base class for rendering system support
 *
 * Copyright © 2016  Benjamin Otte
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

#include "gdkdrawcontextprivate.h"

#include "gdkdebugprivate.h"
#include <glib/gi18n-lib.h>
#include "gdkprofilerprivate.h"
#include "gdksurfaceprivate.h"

/**
 * GdkDrawContext:
 *
 * Base class for objects implementing different rendering methods.
 *
 * `GdkDrawContext` is the base object used by contexts implementing different
 * rendering methods, such as [class@Gdk.CairoContext] or [class@Gdk.GLContext].
 * It provides shared functionality between those contexts.
 *
 * You will always interact with one of those subclasses.
 *
 * A `GdkDrawContext` is always associated with a single toplevel surface.
 */

typedef struct _GdkDrawContextPrivate GdkDrawContextPrivate;

struct _GdkDrawContextPrivate {
  GdkDisplay *display;
  GdkSurface *surface;

  cairo_region_t *frame_region;
};

enum {
  PROP_0,

  PROP_DISPLAY,
  PROP_SURFACE,

  LAST_PROP
};

static GParamSpec *pspecs[LAST_PROP] = { NULL, };

G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE (GdkDrawContext, gdk_draw_context, G_TYPE_OBJECT)

static void
gdk_draw_context_default_surface_resized (GdkDrawContext *context)
{
}

static void
gdk_draw_context_default_empty_frame (GdkDrawContext *context)
{
  g_warning ("FIXME: Implement GdkDrawContext.empty_frame in %s", G_OBJECT_TYPE_NAME (context));
}

static void
gdk_draw_context_dispose (GObject *gobject)
{
  GdkDrawContext *context = GDK_DRAW_CONTEXT (gobject);
  GdkDrawContextPrivate *priv = gdk_draw_context_get_instance_private (context);

  if (priv->surface)
    {
      priv->surface->draw_contexts = g_slist_remove (priv->surface->draw_contexts, context);
      g_clear_object (&priv->surface);
    }
  g_clear_object (&priv->display);

  G_OBJECT_CLASS (gdk_draw_context_parent_class)->dispose (gobject);
}

static void
gdk_draw_context_set_property (GObject      *gobject,
                               guint         prop_id,
                               const GValue *value,
                               GParamSpec   *pspec)
{
  GdkDrawContext *context = GDK_DRAW_CONTEXT (gobject);
  GdkDrawContextPrivate *priv = gdk_draw_context_get_instance_private (context);

  switch (prop_id)
    {
    case PROP_DISPLAY:
      if (priv->display != NULL)
        {
          g_assert (g_value_get_object (value) == NULL);
        }
      else
        {
          priv->display = g_value_dup_object (value);
        }
      break;

    case PROP_SURFACE:
      priv->surface = g_value_dup_object (value);
      if (priv->surface)
        {
          priv->surface->draw_contexts = g_slist_prepend (priv->surface->draw_contexts, context);
          if (priv->display)
            {
              g_assert (priv->display == gdk_surface_get_display (priv->surface));
            }
          else
            {
              priv->display = g_object_ref (gdk_surface_get_display (priv->surface));
            }
        }
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
    }
}

static void
gdk_draw_context_get_property (GObject    *gobject,
                               guint       prop_id,
                               GValue     *value,
                               GParamSpec *pspec)
{
  GdkDrawContext *context = GDK_DRAW_CONTEXT (gobject);
  GdkDrawContextPrivate *priv = gdk_draw_context_get_instance_private (context);

  switch (prop_id)
    {
    case PROP_DISPLAY:
      g_value_set_object (value, gdk_draw_context_get_display (context));
      break;

    case PROP_SURFACE:
      g_value_set_object (value, priv->surface);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
    }
}

static void
gdk_draw_context_class_init (GdkDrawContextClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->set_property = gdk_draw_context_set_property;
  gobject_class->get_property = gdk_draw_context_get_property;
  gobject_class->dispose = gdk_draw_context_dispose;

  klass->surface_resized = gdk_draw_context_default_surface_resized;
  klass->empty_frame = gdk_draw_context_default_empty_frame;

  /**
   * GdkDrawContext:display: (attributes org.gtk.Property.get=gdk_draw_context_get_display)
   *
   * The `GdkDisplay` used to create the `GdkDrawContext`.
   */
  pspecs[PROP_DISPLAY] =
    g_param_spec_object ("display", NULL, NULL,
                         GDK_TYPE_DISPLAY,
                         G_PARAM_READWRITE |
                         G_PARAM_CONSTRUCT_ONLY |
                         G_PARAM_STATIC_STRINGS);

  /**
   * GdkDrawContext:surface: (attributes org.gtk.Property.get=gdk_draw_context_get_surface)
   *
   * The `GdkSurface` the context is bound to.
   */
  pspecs[PROP_SURFACE] =
    g_param_spec_object ("surface", NULL, NULL,
                         GDK_TYPE_SURFACE,
                         G_PARAM_READWRITE |
                         G_PARAM_CONSTRUCT_ONLY |
                         G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (gobject_class, LAST_PROP, pspecs);
}

static guint pixels_counter;

static void
gdk_draw_context_init (GdkDrawContext *self)
{
  if (pixels_counter == 0)
    pixels_counter = gdk_profiler_define_int_counter ("frame pixels", "Pixels drawn per frame");
}

/**
 * gdk_draw_context_is_in_frame:
 * @context: a `GdkDrawContext`
 *
 * Returns %TRUE if @context is in the process of drawing to its surface.
 *
 * This is the case between calls to [method@Gdk.DrawContext.begin_frame]
 * and [method@Gdk.DrawContext.end_frame]. In this situation, drawing commands
 * may be effecting the contents of the @context's surface.
 *
 * Returns: %TRUE if the context is between [method@Gdk.DrawContext.begin_frame]
 *   and [method@Gdk.DrawContext.end_frame] calls.
 */
gboolean
gdk_draw_context_is_in_frame (GdkDrawContext *context)
{
  GdkDrawContextPrivate *priv = gdk_draw_context_get_instance_private (context);

  g_return_val_if_fail (GDK_IS_DRAW_CONTEXT (context), FALSE);

  return priv->frame_region != NULL;
}

/*< private >
 * gdk_draw_context_surface_resized:
 * @context: a `GdkDrawContext`
 *
 * Called by the surface the @context belongs to when the size of the surface
 * changes.
 */
void
gdk_draw_context_surface_resized (GdkDrawContext *context)
{
  GDK_DRAW_CONTEXT_GET_CLASS (context)->surface_resized (context);
}

/**
 * gdk_draw_context_get_display: (attributes org.gtk.Method.get_property=display)
 * @context: a `GdkDrawContext`
 *
 * Retrieves the `GdkDisplay` the @context is created for
 *
 * Returns: (nullable) (transfer none): the `GdkDisplay`
 */
GdkDisplay *
gdk_draw_context_get_display (GdkDrawContext *context)
{
  GdkDrawContextPrivate *priv = gdk_draw_context_get_instance_private (context);

  g_return_val_if_fail (GDK_IS_DRAW_CONTEXT (context), NULL);

  return priv->display;
}

/**
 * gdk_draw_context_get_surface: (attributes org.gtk.Method.get_property=surface)
 * @context: a `GdkDrawContext`
 *
 * Retrieves the surface that @context is bound to.
 *
 * Returns: (nullable) (transfer none): a `GdkSurface`
 */
GdkSurface *
gdk_draw_context_get_surface (GdkDrawContext *context)
{
  GdkDrawContextPrivate *priv = gdk_draw_context_get_instance_private (context);

  g_return_val_if_fail (GDK_IS_DRAW_CONTEXT (context), NULL);

  return priv->surface;
}

/**
 * gdk_draw_context_begin_frame:
 * @context: the `GdkDrawContext` used to draw the frame. The context must
 *   have a surface.
 * @region: minimum region that should be drawn
 *
 * Indicates that you are beginning the process of redrawing @region
 * on the @context's surface.
 *
 * Calling this function begins a drawing operation using @context on the
 * surface that @context was created from. The actual requirements and
 * guarantees for the drawing operation vary for different implementations
 * of drawing, so a [class@Gdk.CairoContext] and a [class@Gdk.GLContext]
 * need to be treated differently.
 *
 * A call to this function is a requirement for drawing and must be
 * followed by a call to [method@Gdk.DrawContext.end_frame], which will
 * complete the drawing operation and ensure the contents become visible
 * on screen.
 *
 * Note that the @region passed to this function is the minimum region that
 * needs to be drawn and depending on implementation, windowing system and
 * hardware in use, it might be necessary to draw a larger region. Drawing
 * implementation must use [method@Gdk.DrawContext.get_frame_region] to
 * query the region that must be drawn.
 *
 * When using GTK, the widget system automatically places calls to
 * gdk_draw_context_begin_frame() and gdk_draw_context_end_frame() via the
 * use of [GskRenderer](../gsk4/class.Renderer.html)s, so application code
 * does not need to call these functions explicitly.
 */
void
gdk_draw_context_begin_frame (GdkDrawContext       *context,
                              const cairo_region_t *region)
{
  GdkDrawContextPrivate *priv = gdk_draw_context_get_instance_private (context);

  g_return_if_fail (GDK_IS_DRAW_CONTEXT (context));
  g_return_if_fail (priv->surface != NULL);
  g_return_if_fail (region != NULL);

  gdk_draw_context_begin_frame_full (context, GDK_MEMORY_U8, region);
}

/*
 * @depth: best depth to render in
 *
 * If the given depth is not `GDK_MEMORY_U8`, GDK will see about providing a
 * rendering target that supports a higher bit depth than 8 bits per channel.
 * Typically this means a target supporting 16bit floating point pixels, but
 * that is not guaranteed.
 *
 * This is only a request and if the GDK backend does not support HDR rendering
 * or does not consider it worthwhile, it may choose to not honor the request.
 * It may also choose to provide a different depth even if it was not requested.
 * Typically the steps undertaken by a backend are:
 * 1. Check if high depth is supported by this drawing backend.
 * 2. Check if the compositor supports high depth.
 * 3. Check if the compositor prefers regular bit depth. This is usually the case
 *    when the attached monitors do not support high depth content or when the
 *    system is resource constrained.
 * In either of those cases, the context will usually choose to not honor the request.
 *
 * The rendering code must be able to deal with content in any bit depth, no matter
 * the preference. The depth argument is only a hint and GDK is free
 * to choose.
 */
void
gdk_draw_context_begin_frame_full (GdkDrawContext       *context,
                                   GdkMemoryDepth        depth,
                                   const cairo_region_t *region)
{
  GdkDrawContextPrivate *priv = gdk_draw_context_get_instance_private (context);

  if (GDK_SURFACE_DESTROYED (priv->surface))
    return;

  if (priv->surface->paint_context != NULL)
    {
      if (priv->surface->paint_context == context)
        {
          g_critical ("The surface %p is already drawing. You must finish the "
                      "previous drawing operation with gdk_draw_context_end_frame() first.",
                      priv->surface);
        }
      else
        {
          g_critical ("The surface %p is already being drawn by %s %p. "
                      "You cannot draw a surface with multiple contexts at the same time.",
                      priv->surface,
                      G_OBJECT_TYPE_NAME (priv->surface->paint_context), priv->surface->paint_context);
        }
      return;
    }

  if (gdk_display_get_debug_flags (priv->display) & GDK_DEBUG_HIGH_DEPTH)
    depth = GDK_MEMORY_FLOAT32;

  priv->frame_region = cairo_region_copy (region);
  priv->surface->paint_context = g_object_ref (context);

  GDK_DRAW_CONTEXT_GET_CLASS (context)->begin_frame (context, depth, priv->frame_region);

  cairo_region_intersect_rectangle (priv->frame_region,
                                    &(cairo_rectangle_int_t) {
                                      0, 0,
                                      priv->surface->width, priv->surface->height
                                    });
}

#ifdef HAVE_SYSPROF
static gint64
region_get_pixels (cairo_region_t *region)
{
  int i, n;
  cairo_rectangle_int_t rect;
  gint64 pixels = 0;

  n = cairo_region_num_rectangles (region);
  for (i = 0; i < n; i++)
    {
      cairo_region_get_rectangle (region, i, &rect);
      pixels += rect.width * rect.height;
    }

  return pixels;
}
#endif

/**
 * gdk_draw_context_end_frame:
 * @context: a `GdkDrawContext`
 *
 * Ends a drawing operation started with gdk_draw_context_begin_frame().
 *
 * This makes the drawing available on screen.
 * See [method@Gdk.DrawContext.begin_frame] for more details about drawing.
 *
 * When using a [class@Gdk.GLContext], this function may call `glFlush()`
 * implicitly before returning; it is not recommended to call `glFlush()`
 * explicitly before calling this function.
 */
void
gdk_draw_context_end_frame (GdkDrawContext *context)
{
  GdkDrawContextPrivate *priv = gdk_draw_context_get_instance_private (context);

  g_return_if_fail (GDK_IS_DRAW_CONTEXT (context));
  g_return_if_fail (priv->surface != NULL);

  if (GDK_SURFACE_DESTROYED (priv->surface))
    return;

  if (priv->surface->paint_context == NULL)
    {
      g_critical ("The surface %p has no drawing context. You must call "
                  "gdk_draw_context_begin_frame() before calling "
                  "gdk_draw_context_end_frame().", priv->surface);
      return;
    }
  else if (priv->surface->paint_context != context)
    {
      g_critical ("The surface %p is not drawn by this context but by %s %p.",
                  priv->surface, 
                  G_OBJECT_TYPE_NAME (priv->surface->paint_context), priv->surface->paint_context);
      return;
    }

  GDK_DRAW_CONTEXT_GET_CLASS (context)->end_frame (context, priv->frame_region);

  gdk_profiler_set_int_counter (pixels_counter, region_get_pixels (priv->frame_region));

  g_clear_pointer (&priv->frame_region, cairo_region_destroy);
  g_clear_object (&priv->surface->paint_context);
}

/**
 * gdk_draw_context_get_frame_region:
 * @context: a `GdkDrawContext`
 *
 * Retrieves the region that is currently being repainted.
 *
 * After a call to [method@Gdk.DrawContext.begin_frame] this function will
 * return a union of the region passed to that function and the area of the
 * surface that the @context determined needs to be repainted.
 *
 * If @context is not in between calls to [method@Gdk.DrawContext.begin_frame]
 * and [method@Gdk.DrawContext.end_frame], %NULL will be returned.
 *
 * Returns: (transfer none) (nullable): a Cairo region
 */
const cairo_region_t *
gdk_draw_context_get_frame_region (GdkDrawContext *context)
{
  GdkDrawContextPrivate *priv = gdk_draw_context_get_instance_private (context);

  g_return_val_if_fail (GDK_IS_DRAW_CONTEXT (context), NULL);

  return priv->frame_region;
}

void
gdk_draw_context_empty_frame (GdkDrawContext *context)
{
  GdkDrawContextPrivate *priv = gdk_draw_context_get_instance_private (context);

  g_return_if_fail (GDK_IS_DRAW_CONTEXT (context));
  g_return_if_fail (priv->surface != NULL);

  if (GDK_SURFACE_DESTROYED (priv->surface))
    return;

  GDK_DRAW_CONTEXT_GET_CLASS (context)->empty_frame (context);
}
