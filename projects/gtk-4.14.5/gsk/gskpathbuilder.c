/*
 * Copyright © 2020 Benjamin Otte
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * Authors: Benjamin Otte <otte@gnome.org>
 */

#include "config.h"

#include <math.h>

#include "gskpathbuilder.h"

#include "gskpathprivate.h"
#include "gskcurveprivate.h"
#include "gskpathpointprivate.h"
#include "gskcontourprivate.h"

/**
 * GskPathBuilder:
 *
 * `GskPathBuilder` is an auxiliary object for constructing
 * `GskPath` objects.
 *
 * A path is constructed like this:
 *
 * |[<!-- language="C" -->
 * GskPath *
 * construct_path (void)
 * {
 *   GskPathBuilder *builder;
 *
 *   builder = gsk_path_builder_new ();
 *
 *   // add contours to the path here
 *
 *   return gsk_path_builder_free_to_path (builder);
 * ]|
 *
 * Adding contours to the path can be done in two ways.
 * The easiest option is to use the `gsk_path_builder_add_*` group
 * of functions that add predefined contours to the current path,
 * either common shapes like [method@Gsk.PathBuilder.add_circle]
 * or by adding from other paths like [method@Gsk.PathBuilder.add_path].
 *
 * The `gsk_path_builder_add_*` methods always add complete contours,
 * and do not use or modify the current point.
 *
 * The other option is to define each line and curve manually with
 * the `gsk_path_builder_*_to` group of functions. You start with
 * a call to [method@Gsk.PathBuilder.move_to] to set the starting point
 * and then use multiple calls to any of the drawing functions to
 * move the pen along the plane. Once you are done, you can call
 * [method@Gsk.PathBuilder.close] to close the path by connecting it
 * back with a line to the starting point.
 *
 * This is similar to how paths are drawn in Cairo.
 *
 * Note that `GskPathBuilder` will reduce the degree of added Bézier
 * curves as much as possible, to simplify rendering.
 *
 * Since: 4.14
 */

struct _GskPathBuilder
{
  int ref_count;

  GSList *contours; /* (reverse) list of already recorded contours */

  GskPathFlags flags; /* flags for the current path */
  graphene_point_t current_point; /* the point all drawing ops start from */
  GArray *ops; /* operations for current contour - size == 0 means no current contour */
  GArray *points; /* points for the operations */
};

G_DEFINE_BOXED_TYPE (GskPathBuilder,
                     gsk_path_builder,
                     gsk_path_builder_ref,
                     gsk_path_builder_unref)


/**
 * gsk_path_builder_new:
 *
 * Create a new `GskPathBuilder` object.
 *
 * The resulting builder would create an empty `GskPath`.
 * Use addition functions to add types to it.
 *
 * Returns: a new `GskPathBuilder`
 *
 * Since: 4.14
 */
GskPathBuilder *
gsk_path_builder_new (void)
{
  GskPathBuilder *self;

  self = g_slice_new0 (GskPathBuilder);
  self->ref_count = 1;

  self->ops = g_array_new (FALSE, FALSE, sizeof (gskpathop));
  self->points = g_array_new (FALSE, FALSE, sizeof (graphene_point_t));

  /* Be explicit here */
  self->current_point = GRAPHENE_POINT_INIT (0, 0);

  return self;
}

/**
 * gsk_path_builder_ref:
 * @self: a `GskPathBuilder`
 *
 * Acquires a reference on the given builder.
 *
 * This function is intended primarily for language bindings.
 * `GskPathBuilder` objects should not be kept around.
 *
 * Returns: (transfer none): the given `GskPathBuilder` with
 *   its reference count increased
 *
 * Since: 4.14
 */
GskPathBuilder *
gsk_path_builder_ref (GskPathBuilder *self)
{
  g_return_val_if_fail (self != NULL, NULL);
  g_return_val_if_fail (self->ref_count > 0, NULL);

  self->ref_count += 1;

  return self;
}

/* We're cheating here. Out pathops are relative to the NULL pointer,
 * so that we can not care about the points GArray reallocating itself
 * until we create the contour.
 * This does however mean that we need to not use gsk_pathop_get_points()
 * without offsetting the returned pointer.
 */
static inline gskpathop
gsk_pathop_encode_index (GskPathOperation op,
                         gsize            index)
{
  return gsk_pathop_encode (op, ((graphene_point_t *) NULL) + index);
}

static void
gsk_path_builder_ensure_current (GskPathBuilder *self)
{
  if (self->ops->len != 0)
    return;

  self->flags = GSK_PATH_FLAT;
  g_array_append_vals (self->ops, (gskpathop[1]) { gsk_pathop_encode_index (GSK_PATH_MOVE, 0) }, 1);
  g_array_append_val (self->points, self->current_point);
}

static void
gsk_path_builder_append_current (GskPathBuilder         *self,
                                 GskPathOperation        op,
                                 gsize                   n_points,
                                 const graphene_point_t *points)
{
  gsk_path_builder_ensure_current (self);

  g_array_append_vals (self->ops, (gskpathop[1]) { gsk_pathop_encode_index (op, self->points->len - 1) }, 1);
  g_array_append_vals (self->points, points, n_points);

  self->current_point = points[n_points - 1];
}

static void
gsk_path_builder_end_current (GskPathBuilder *self)
{
  GskContour *contour;

  if (self->ops->len == 0)
   return;

  contour = gsk_standard_contour_new (self->flags,
                                      (graphene_point_t *) self->points->data,
                                      self->points->len,
                                      (gskpathop *) self->ops->data,
                                      self->ops->len,
                                      (graphene_point_t *) self->points->data - (graphene_point_t *) NULL);

  g_array_set_size (self->ops, 0);
  g_array_set_size (self->points, 0);

  /* do this at the end to avoid inflooping when add_contour calls back here */
  gsk_path_builder_add_contour (self, contour);
}

static void
gsk_path_builder_clear (GskPathBuilder *self)
{
  gsk_path_builder_end_current (self);

  g_slist_free_full (self->contours, g_free);
  self->contours = NULL;
}

/**
 * gsk_path_builder_unref:
 * @self: a `GskPathBuilder`
 *
 * Releases a reference on the given builder.
 *
 * Since: 4.14
 */
void
gsk_path_builder_unref (GskPathBuilder *self)
{
  g_return_if_fail (self != NULL);
  g_return_if_fail (self->ref_count > 0);

  self->ref_count -= 1;

  if (self->ref_count > 0)
    return;

  gsk_path_builder_clear (self);
  g_array_unref (self->ops);
  g_array_unref (self->points);
  g_slice_free (GskPathBuilder, self);
}

/**
 * gsk_path_builder_free_to_path: (skip)
 * @self: a `GskPathBuilder`
 *
 * Creates a new `GskPath` from the current state of the
 * given builder, and unrefs the @builder instance.
 *
 * Returns: (transfer full): the newly created `GskPath`
 *   with all the contours added to the builder
 *
 * Since: 4.14
 */
GskPath *
gsk_path_builder_free_to_path (GskPathBuilder *self)
{
  GskPath *res;

  g_return_val_if_fail (self != NULL, NULL);

  res = gsk_path_builder_to_path (self);

  gsk_path_builder_unref (self);

  return res;
}

/**
 * gsk_path_builder_to_path:
 * @self: a `GskPathBuilder`
 *
 * Creates a new `GskPath` from the given builder.
 *
 * The given `GskPathBuilder` is reset once this function returns;
 * you cannot call this function multiple times on the same builder
 * instance.
 *
 * This function is intended primarily for language bindings.
 * C code should use [method@Gsk.PathBuilder.free_to_path].
 *
 * Returns: (transfer full): the newly created `GskPath`
 *   with all the contours added to the builder
 *
 * Since: 4.14
 */
GskPath *
gsk_path_builder_to_path (GskPathBuilder *self)
{
  GskPath *path;

  g_return_val_if_fail (self != NULL, NULL);

  gsk_path_builder_end_current (self);

  self->contours = g_slist_reverse (self->contours);

  path = gsk_path_new_from_contours (self->contours);

  gsk_path_builder_clear (self);

  return path;
}

void
gsk_path_builder_add_contour (GskPathBuilder *self,
                              GskContour     *contour)
{
  gsk_path_builder_end_current (self);

  self->contours = g_slist_prepend (self->contours, contour);
}

/**
 * gsk_path_builder_get_current_point:
 * @self: a `GskPathBuilder`
 *
 * Gets the current point.
 *
 * The current point is used for relative drawing commands and
 * updated after every operation.
 *
 * When the builder is created, the default current point is set
 * to `0, 0`. Note that this is different from cairo, which starts
 * out without a current point.
 *
 * Returns: (transfer none): The current point
 *
 * Since: 4.14
 */
const graphene_point_t *
gsk_path_builder_get_current_point (GskPathBuilder *self)
{
  g_return_val_if_fail (self != NULL, NULL);

  return &self->current_point;
}

/**
 * gsk_path_builder_add_path:
 * @self: a `GskPathBuilder`
 * @path: (transfer none): the path to append
 *
 * Appends all of @path to the builder.
 *
 * Since: 4.14
 */
void
gsk_path_builder_add_path (GskPathBuilder *self,
                           GskPath        *path)
{
  g_return_if_fail (self != NULL);
  g_return_if_fail (path != NULL);

  for (gsize i = 0; i < gsk_path_get_n_contours (path); i++)
    {
      const GskContour *contour = gsk_path_get_contour (path, i);

      gsk_path_builder_add_contour (self, gsk_contour_dup (contour));
    }
}

/**
 * gsk_path_builder_add_reverse_path:
 * @self: a `GskPathBuilder`
 * @path: (transfer none): the path to append
 *
 * Appends all of @path to the builder, in reverse order.
 *
 * Since: 4.14
 */
void
gsk_path_builder_add_reverse_path (GskPathBuilder *self,
                                   GskPath        *path)
{
  g_return_if_fail (self != NULL);
  g_return_if_fail (path != NULL);

  for (gsize i = gsk_path_get_n_contours (path); i > 0; i--)
    {
      const GskContour *contour = gsk_path_get_contour (path, i - 1);

      gsk_path_builder_add_contour (self, gsk_contour_reverse (contour));
    }
}

/**
 * gsk_path_builder_add_cairo_path:
 * @self: a `GskPathBuilder`
 *
 * Adds a Cairo path to the builder.
 *
 * You can use cairo_copy_path() to access the path
 * from a Cairo context.
 *
 * Since: 4.14
 */
void
gsk_path_builder_add_cairo_path (GskPathBuilder     *self,
                                 const cairo_path_t *path)
{
  graphene_point_t current;

  g_return_if_fail (self != NULL);
  g_return_if_fail (path != NULL);

  current = self->current_point;

  for (gsize i = 0; i < path->num_data; i += path->data[i].header.length)
    {
      const cairo_path_data_t *data = &path->data[i];

      switch (data->header.type)
        {
        case CAIRO_PATH_MOVE_TO:
          gsk_path_builder_move_to (self, data[1].point.x, data[1].point.y);
          break;

        case CAIRO_PATH_LINE_TO:
          gsk_path_builder_line_to (self, data[1].point.x, data[1].point.y);
          break;

        case CAIRO_PATH_CURVE_TO:
          gsk_path_builder_cubic_to (self,
                                     data[1].point.x, data[1].point.y,
                                     data[2].point.x, data[2].point.y,
                                     data[3].point.x, data[3].point.y);
          break;

        case CAIRO_PATH_CLOSE_PATH:
          gsk_path_builder_close (self);
          break;

        default:
          g_assert_not_reached ();
          break;
        }
    }

  gsk_path_builder_end_current (self);
  self->current_point = current;
}

/**
 * gsk_path_builder_add_rect:
 * @self: A `GskPathBuilder`
 * @rect: The rectangle to create a path for
 *
 * Adds @rect as a new contour to the path built by the builder.
 *
 * The path is going around the rectangle in clockwise direction.
 *
 * If the the width or height are 0, the path will be a closed
 * horizontal or vertical line. If both are 0, it'll be a closed dot.
 *
 * Since: 4.14
 */
void
gsk_path_builder_add_rect (GskPathBuilder        *self,
                           const graphene_rect_t *rect)
{
  graphene_rect_t r;

  g_return_if_fail (self != NULL);
  g_return_if_fail (rect != NULL);

  graphene_rect_normalize_r (rect, &r);
  gsk_path_builder_add_contour (self, gsk_rect_contour_new (&r));
}

/**
 * gsk_path_builder_add_rounded_rect:
 * @self: a #GskPathBuilder
 * @rect: the rounded rect
 *
 * Adds @rect as a new contour to the path built in @self.
 *
 * The path is going around the rectangle in clockwise direction.
 *
 * Since: 4.14
 */
void
gsk_path_builder_add_rounded_rect (GskPathBuilder       *self,
                                   const GskRoundedRect *rect)
{
  g_return_if_fail (self != NULL);
  g_return_if_fail (rect != NULL);

  gsk_path_builder_add_contour (self, gsk_rounded_rect_contour_new (rect));
}

/**
 * gsk_path_builder_add_circle:
 * @self: a `GskPathBuilder`
 * @center: the center of the circle
 * @radius: the radius of the circle
 *
 * Adds a circle with the @center and @radius.
 *
 * The path is going around the circle in clockwise direction.
 *
 * If @radius is zero, the contour will be a closed point.
 *
 * Since: 4.14
 */
void
gsk_path_builder_add_circle (GskPathBuilder         *self,
                             const graphene_point_t *center,
                             float                   radius)
{
  g_return_if_fail (self != NULL);
  g_return_if_fail (center != NULL);
  g_return_if_fail (radius >= 0);

  gsk_path_builder_add_contour (self, gsk_circle_contour_new (center, radius));
}

/**
 * gsk_path_builder_move_to:
 * @self: a `GskPathBuilder`
 * @x: x coordinate
 * @y: y coordinate
 *
 * Starts a new contour by placing the pen at @x, @y.
 *
 * If this function is called twice in succession, the first
 * call will result in a contour made up of a single point.
 * The second call will start a new contour.
 *
 * Since: 4.14
 */
void
gsk_path_builder_move_to (GskPathBuilder *self,
                          float           x,
                          float           y)
{
  g_return_if_fail (self != NULL);

  gsk_path_builder_end_current (self);

  self->current_point = GRAPHENE_POINT_INIT(x, y);

  gsk_path_builder_ensure_current (self);
}

/**
 * gsk_path_builder_rel_move_to:
 * @self: a `GskPathBuilder`
 * @x: x offset
 * @y: y offset
 *
 * Starts a new contour by placing the pen at @x, @y
 * relative to the current point.
 *
 * This is the relative version of [method@Gsk.PathBuilder.move_to].
 *
 * Since: 4.14
 */
void
gsk_path_builder_rel_move_to (GskPathBuilder *self,
                              float           x,
                              float           y)
{
  g_return_if_fail (self != NULL);

  gsk_path_builder_move_to (self,
                            self->current_point.x + x,
                            self->current_point.y + y);
}

/**
 * gsk_path_builder_line_to:
 * @self: a `GskPathBuilder`
 * @x: x coordinate
 * @y: y coordinate
 *
 * Draws a line from the current point to @x, @y and makes it
 * the new current point.
 *
 * <picture>
 *   <source srcset="line-dark.png" media="(prefers-color-scheme: dark)">
 *   <img alt="Line To" src="line-light.png">
 * </picture>
 *
 * Since: 4.14
 */
void
gsk_path_builder_line_to (GskPathBuilder *self,
                          float           x,
                          float           y)
{
  g_return_if_fail (self != NULL);

  /* skip the line if it goes to the same point */
  if (graphene_point_equal (&self->current_point,
                            &GRAPHENE_POINT_INIT (x, y)))
    return;

  gsk_path_builder_append_current (self,
                                   GSK_PATH_LINE,
                                   1, (graphene_point_t[1]) {
                                     GRAPHENE_POINT_INIT (x, y)
                                   });
}

/**
 * gsk_path_builder_rel_line_to:
 * @self: a `GskPathBuilder`
 * @x: x offset
 * @y: y offset
 *
 * Draws a line from the current point to a point offset from it
 * by @x, @y and makes it the new current point.
 *
 * This is the relative version of [method@Gsk.PathBuilder.line_to].
 *
 * Since: 4.14
 */
void
gsk_path_builder_rel_line_to (GskPathBuilder *self,
                              float           x,
                              float           y)
{
  g_return_if_fail (self != NULL);

  gsk_path_builder_line_to (self,
                            self->current_point.x + x,
                            self->current_point.y + y);
}

static inline void
closest_point (const graphene_point_t *p,
               const graphene_point_t *a,
               const graphene_point_t *b,
               graphene_point_t       *q)
{
  graphene_vec2_t n;
  graphene_vec2_t ap;
  float t;

  graphene_vec2_init (&n, b->x - a->x, b->y - a->y);
  graphene_vec2_init (&ap, p->x - a->x, p->y - a->y);

  t = graphene_vec2_dot (&ap, &n) / graphene_vec2_dot (&n, &n);

  q->x = a->x + t * (b->x - a->x);
  q->y = a->y + t * (b->y - a->y);
}

static inline gboolean
collinear (const graphene_point_t *p,
           const graphene_point_t *a,
           const graphene_point_t *b)
{
  graphene_point_t q;

  if (graphene_point_equal (a, b))
    return TRUE;

  closest_point (p, a, b, &q);

  return graphene_point_near (p, &q, 0.001);
}

/**
 * gsk_path_builder_quad_to:
 * @self: a #GskPathBuilder
 * @x1: x coordinate of control point
 * @y1: y coordinate of control point
 * @x2: x coordinate of the end of the curve
 * @y2: y coordinate of the end of the curve
 *
 * Adds a [quadratic Bézier curve](https://en.wikipedia.org/wiki/B%C3%A9zier_curve)
 * from the current point to @x2, @y2 with @x1, @y1 as the control point.
 *
 * After this, @x2, @y2 will be the new current point.
 *
 * <picture>
 *   <source srcset="quad-dark.png" media="(prefers-color-scheme: dark)">
 *   <img alt="Quad To" src="quad-light.png">
 * </picture>
 *
 * Since: 4.14
 */
void
gsk_path_builder_quad_to (GskPathBuilder *self,
                          float           x1,
                          float           y1,
                          float           x2,
                          float           y2)
{
  graphene_point_t p0 = self->current_point;
  graphene_point_t p1 = GRAPHENE_POINT_INIT (x1, y1);
  graphene_point_t p2 = GRAPHENE_POINT_INIT (x2, y2);

  g_return_if_fail (self != NULL);

  if (collinear (&p0, &p1, &p2))
    {
      GskBoundingBox bb;

      /* We simplify degenerate quads to one or two lines */
      if (!gsk_bounding_box_contains_point (gsk_bounding_box_init (&bb, &p0, &p2), &p1))
        {
          GskCurve c;

          gsk_curve_init_foreach (&c, GSK_PATH_QUAD,
                                  (const graphene_point_t []) { p0, p1, p2 },
                                  3, 0.f);
          gsk_curve_get_tight_bounds (&c, &bb);
          for (int i = 0; i < 4; i++)
            {
              graphene_point_t q;

              gsk_bounding_box_get_corner (&bb, i, &q);
              if (graphene_point_equal (&p0, &q) ||
                  graphene_point_equal (&p2, &q))
                {
                  gsk_bounding_box_get_corner (&bb, (i + 2) % 4, &q);
                  gsk_path_builder_line_to (self, q.x, q.y);
                  break;
                }
            }
        }

      gsk_path_builder_line_to (self, x2, y2);

      return;
    }

  self->flags &= ~GSK_PATH_FLAT;
  gsk_path_builder_append_current (self,
                                   GSK_PATH_QUAD,
                                   2, (graphene_point_t[2]) { p1, p2 });
}

/**
 * gsk_path_builder_rel_quad_to:
 * @self: a `GskPathBuilder`
 * @x1: x offset of control point
 * @y1: y offset of control point
 * @x2: x offset of the end of the curve
 * @y2: y offset of the end of the curve
 *
 * Adds a [quadratic Bézier curve](https://en.wikipedia.org/wiki/B%C3%A9zier_curve)
 * from the current point to @x2, @y2 with @x1, @y1 the control point.
 *
 * All coordinates are given relative to the current point.
 *
 * This is the relative version of [method@Gsk.PathBuilder.quad_to].
 *
 * Since: 4.14
 */
void
gsk_path_builder_rel_quad_to (GskPathBuilder *self,
                              float           x1,
                              float           y1,
                              float           x2,
                              float           y2)
{
  g_return_if_fail (self != NULL);

  gsk_path_builder_quad_to (self,
                            self->current_point.x + x1,
                            self->current_point.y + y1,
                            self->current_point.x + x2,
                            self->current_point.y + y2);
}

static gboolean
point_is_between (const graphene_point_t *q,
                  const graphene_point_t *p0,
                  const graphene_point_t *p1)
{
  return collinear (p0, p1, q) &&
         fabsf (graphene_point_distance (p0, q, NULL, NULL) + graphene_point_distance (p1, q, NULL, NULL) - graphene_point_distance (p0, p1, NULL, NULL)) < 0.001;
}

static gboolean
bounding_box_corner_between (const GskBoundingBox   *bb,
                             const graphene_point_t *p0,
                             const graphene_point_t *p1,
                             graphene_point_t       *p)
{
  for (int i = 0; i < 4; i++)
    {
      graphene_point_t q;

      gsk_bounding_box_get_corner (bb, i, &q);

      if (point_is_between (&q, p0, p1))
        {
          *p = q;
          return TRUE;
        }
    }

  return FALSE;
}


/**
 * gsk_path_builder_cubic_to:
 * @self: a `GskPathBuilder`
 * @x1: x coordinate of first control point
 * @y1: y coordinate of first control point
 * @x2: x coordinate of second control point
 * @y2: y coordinate of second control point
 * @x3: x coordinate of the end of the curve
 * @y3: y coordinate of the end of the curve
 *
 * Adds a [cubic Bézier curve](https://en.wikipedia.org/wiki/B%C3%A9zier_curve)
 * from the current point to @x3, @y3 with @x1, @y1 and @x2, @y2 as the control
 * points.
 *
 * After this, @x3, @y3 will be the new current point.
 *
 * <picture>
 *   <source srcset="cubic-dark.png" media="(prefers-color-scheme: dark)">
 *   <img alt="Cubic To" src="cubic-light.png">
 * </picture>
 *
 * Since: 4.14
 */
void
gsk_path_builder_cubic_to (GskPathBuilder *self,
                           float           x1,
                           float           y1,
                           float           x2,
                           float           y2,
                           float           x3,
                           float           y3)
{
  graphene_point_t p0 = self->current_point;
  graphene_point_t p1 = GRAPHENE_POINT_INIT (x1, y1);
  graphene_point_t p2 = GRAPHENE_POINT_INIT (x2, y2);
  graphene_point_t p3 = GRAPHENE_POINT_INIT (x3, y3);
  graphene_point_t p, q;
  gboolean p01, p12, p23;

  g_return_if_fail (self != NULL);

  p01 = graphene_point_equal (&p0, &p1);
  p12 = graphene_point_equal (&p1, &p2);
  p23 = graphene_point_equal (&p2, &p3);

  if (p01 && p12 && p23)
    return;

  if ((p01 && p23) || (p12 && (p01 || p23)))
    {
      gsk_path_builder_line_to (self, x3, y3);
      return;
    }

  if (collinear (&p0, &p1, &p2) &&
      collinear (&p1, &p2, &p3) &&
      (!p12 || collinear (&p0, &p1, &p3)))
    {
      GskBoundingBox bb;
      gboolean p1in, p2in;

      gsk_bounding_box_init (&bb, &p0, &p3);
      p1in = gsk_bounding_box_contains_point (&bb, &p1);
      p2in = gsk_bounding_box_contains_point (&bb, &p2);
      if (p1in && p2in)
        {
          gsk_path_builder_line_to (self, x3, y3);
        }
      else
        {
          GskCurve c;

          gsk_curve_init_foreach (&c,
                                  GSK_PATH_CUBIC,
                                  (const graphene_point_t[]) { p0, p1, p2, p3 },
                                  4,
                                  0.f);
          gsk_curve_get_tight_bounds (&c, &bb);
          if (!p1in)
            {
              /* Find the intersection of bb with p0 - p1.
               * It must be a corner
               */
              bounding_box_corner_between (&bb, &p0, &p1, &p);
              gsk_path_builder_line_to (self, p.x, p.y);
            }
          if (!p2in)
            {
              /* Find the intersection of bb with p2 - p3. */
              bounding_box_corner_between (&bb, &p3, &p2, &p);
              gsk_path_builder_line_to (self, p.x, p.y);
            }
          gsk_path_builder_line_to (self, x3, y3);
        }

      return;
    }

  /* reduce to a quadratic if possible */
  graphene_point_interpolate (&p0, &p1, 1.5, &p);
  graphene_point_interpolate (&p3, &p2, 1.5, &q);
  if (graphene_point_near (&p, &q, 0.001))
    {
      gsk_path_builder_quad_to (self, p.x, p.y, x3, y3);
      return;
    }

  self->flags &= ~GSK_PATH_FLAT;

  /* At this point, we are dealing with a cubic that can't be reduced to
   * lines or quadratics. Check for cusps.
   */
    {
      GskCurve c, c1, c2, c3, c4;
      float t[2];
      int n;

      gsk_curve_init_foreach (&c,
                              GSK_PATH_CUBIC,
                              (const graphene_point_t[]) { p0, p1, p2, p3 },
                              4,
                              0.f);

      n = gsk_curve_get_cusps (&c, t);
      if (n == 1)
        {
          gsk_curve_split (&c, t[0], &c1, &c2);
          gsk_path_builder_append_current (self,
                                           GSK_PATH_CUBIC,
                                           3, &c1.cubic.points[1]);
          gsk_path_builder_append_current (self,
                                           GSK_PATH_CUBIC,
                                           3, &c2.cubic.points[1]);
          return;
        }
      else if (n == 2)
        {
          if (t[1] < t[0])
            {
              float s = t[0];
              t[0] = t[1];
              t[1] = s;
            }

          gsk_curve_split (&c, t[0], &c1, &c2);
          gsk_curve_split (&c2, (t[1] - t[0]) / (1 - t[0]), &c3, &c4);
          gsk_path_builder_append_current (self,
                                           GSK_PATH_CUBIC,
                                           3, &c1.cubic.points[1]);
          gsk_path_builder_append_current (self,
                                           GSK_PATH_CUBIC,
                                           3, &c3.cubic.points[1]);
          gsk_path_builder_append_current (self,
                                           GSK_PATH_CUBIC,
                                           3, &c4.cubic.points[1]);
          return;
        }
    }

  gsk_path_builder_append_current (self,
                                   GSK_PATH_CUBIC,
                                   3, (graphene_point_t[3]) { p1, p2, p3 });
}

/**
 * gsk_path_builder_rel_cubic_to:
 * @self: a `GskPathBuilder`
 * @x1: x offset of first control point
 * @y1: y offset of first control point
 * @x2: x offset of second control point
 * @y2: y offset of second control point
 * @x3: x offset of the end of the curve
 * @y3: y offset of the end of the curve
 *
 * Adds a [cubic Bézier curve](https://en.wikipedia.org/wiki/B%C3%A9zier_curve)
 * from the current point to @x3, @y3 with @x1, @y1 and @x2, @y2 as the control
 * points.
 *
 * All coordinates are given relative to the current point.
 *
 * This is the relative version of [method@Gsk.PathBuilder.cubic_to].
 *
 * Since: 4.14
 */
void
gsk_path_builder_rel_cubic_to (GskPathBuilder *self,
                               float           x1,
                               float           y1,
                               float           x2,
                               float           y2,
                               float           x3,
                               float           y3)
{
  g_return_if_fail (self != NULL);

  gsk_path_builder_cubic_to (self,
                             self->current_point.x + x1,
                             self->current_point.y + y1,
                             self->current_point.x + x2,
                             self->current_point.y + y2,
                             self->current_point.x + x3,
                             self->current_point.y + y3);
}

/**
 * gsk_path_builder_conic_to:
 * @self: a `GskPathBuilder`
 * @x1: x coordinate of control point
 * @y1: y coordinate of control point
 * @x2: x coordinate of the end of the curve
 * @y2: y coordinate of the end of the curve
 * @weight: weight of the control point, must be greater than zero
 *
 * Adds a [conic curve](https://en.wikipedia.org/wiki/Non-uniform_rational_B-spline)
 * from the current point to @x2, @y2 with the given @weight and @x1, @y1 as the
 * control point.
 *
 * The weight determines how strongly the curve is pulled towards the control point.
 * A conic with weight 1 is identical to a quadratic Bézier curve with the same points.
 *
 * Conic curves can be used to draw ellipses and circles. They are also known as
 * rational quadratic Bézier curves.
 *
 * After this, @x2, @y2 will be the new current point.
 *
 * <picture>
 *   <source srcset="conic-dark.png" media="(prefers-color-scheme: dark)">
 *   <img alt="Conic To" src="conic-light.png">
 * </picture>
 *
 * Since: 4.14
 */
void
gsk_path_builder_conic_to (GskPathBuilder *self,
                           float           x1,
                           float           y1,
                           float           x2,
                           float           y2,
                           float           weight)
{
  graphene_point_t p0 = self->current_point;
  graphene_point_t p1 = GRAPHENE_POINT_INIT (x1, y1);
  graphene_point_t p2 = GRAPHENE_POINT_INIT (x2, y2);

  g_return_if_fail (self != NULL);
  g_return_if_fail (weight > 0);

  if (weight == 1)
    {
      gsk_path_builder_quad_to (self, x1, y1, x2, y2);
      return;
    }

  if (collinear (&p0, &p1, &p2))
    {
      GskBoundingBox bb;

      /* We simplify degenerate quads to one or two lines
       * (two lines are needed if there's a cusp).
       */
      if (!gsk_bounding_box_contains_point (gsk_bounding_box_init (&bb, &p0, &p2), &p1))
        {
          GskCurve c;

          gsk_curve_init_foreach (&c, GSK_PATH_CONIC,
                                  (const graphene_point_t []) { p0, p1, p2 },
                                  3, weight);
          gsk_curve_get_tight_bounds (&c, &bb);
          for (int i = 0; i < 4; i++)
            {
              graphene_point_t q;

              gsk_bounding_box_get_corner (&bb, i, &q);
              if (graphene_point_equal (&p0, &q) ||
                  graphene_point_equal (&p2, &q))
                {
                  gsk_bounding_box_get_corner (&bb, (i + 2) % 4, &q);
                  gsk_path_builder_line_to (self, q.x, q.y);
                  break;
                }
            }
        }

      gsk_path_builder_line_to (self, x2, y2);

      return;
    }

  self->flags &= ~GSK_PATH_FLAT;
  gsk_path_builder_append_current (self,
                                   GSK_PATH_CONIC,
                                   3, (graphene_point_t[3]) {
                                     GRAPHENE_POINT_INIT (x1, y1),
                                     GRAPHENE_POINT_INIT (weight, 0),
                                     GRAPHENE_POINT_INIT (x2, y2)
                                   });
}

/**
 * gsk_path_builder_rel_conic_to:
 * @self: a `GskPathBuilder`
 * @x1: x offset of control point
 * @y1: y offset of control point
 * @x2: x offset of the end of the curve
 * @y2: y offset of the end of the curve
 * @weight: weight of the curve, must be greater than zero
 *
 * Adds a [conic curve](https://en.wikipedia.org/wiki/Non-uniform_rational_B-spline)
 * from the current point to @x2, @y2 with the given @weight and @x1, @y1 as the
 * control point.
 *
 * All coordinates are given relative to the current point.
 *
 * This is the relative version of [method@Gsk.PathBuilder.conic_to].
 *
 * Since: 4.14
 */
void
gsk_path_builder_rel_conic_to (GskPathBuilder *self,
                               float           x1,
                               float           y1,
                               float           x2,
                               float           y2,
                               float           weight)
{
  g_return_if_fail (self != NULL);
  g_return_if_fail (weight > 0);

  gsk_path_builder_conic_to (self,
                             self->current_point.x + x1,
                             self->current_point.y + y1,
                             self->current_point.x + x2,
                             self->current_point.y + y2,
                             weight);
}

/**
 * gsk_path_builder_arc_to:
 * @self: a `GskPathBuilder`
 * @x1: x coordinate of first control point
 * @y1: y coordinate of first control point
 * @x2: x coordinate of second control point
 * @y2: y coordinate of second control point
 *
 * Adds an elliptical arc from the current point to @x2, @y2
 * with @x1, @y1 determining the tangent directions.
 *
 * After this, @x2, @y2 will be the new current point.
 *
 * Note: Two points and their tangents do not determine
 * a unique ellipse, so GSK just picks one. If you need more
 * precise control, use [method@Gsk.PathBuilder.conic_to]
 * or [method@Gsk.PathBuilder.svg_arc_to].
 *
 * <picture>
 *   <source srcset="arc-dark.png" media="(prefers-color-scheme: dark)">
 *   <img alt="Arc To" src="arc-light.png">
 * </picture>
 *
 * Since: 4.14
 */
void
gsk_path_builder_arc_to (GskPathBuilder *self,
                         float           x1,
                         float           y1,
                         float           x2,
                         float           y2)
{
  g_return_if_fail (self != NULL);

  gsk_path_builder_conic_to (self, x1, y1, x2, y2, M_SQRT1_2);
}

/**
 * gsk_path_builder_rel_arc_to:
 * @self: a `GskPathBuilder`
 * @x1: x coordinate of first control point
 * @y1: y coordinate of first control point
 * @x2: x coordinate of second control point
 * @y2: y coordinate of second control point
 *
 * Adds an elliptical arc from the current point to @x2, @y2
 * with @x1, @y1 determining the tangent directions.
 *
 * All coordinates are given relative to the current point.
 *
 * This is the relative version of [method@Gsk.PathBuilder.arc_to].
 *
 * Since: 4.14
 */
void
gsk_path_builder_rel_arc_to (GskPathBuilder *self,
                             float           x1,
                             float           y1,
                             float           x2,
                             float           y2)
{
  g_return_if_fail (self != NULL);

  gsk_path_builder_arc_to (self,
                           self->current_point.x + x1,
                           self->current_point.y + y1,
                           self->current_point.x + x2,
                           self->current_point.y + y2);
}

/**
 * gsk_path_builder_close:
 * @self: a `GskPathBuilder`
 *
 * Ends the current contour with a line back to the start point.
 *
 * Note that this is different from calling [method@Gsk.PathBuilder.line_to]
 * with the start point in that the contour will be closed. A closed
 * contour behaves differently from an open one. When stroking, its
 * start and end point are considered connected, so they will be
 * joined via the line join, and not ended with line caps.
 *
 * Since: 4.14
 */
void
gsk_path_builder_close (GskPathBuilder *self)
{
  g_return_if_fail (self != NULL);

  if (self->ops->len == 0)
    return;

  self->flags |= GSK_PATH_CLOSED;
  gsk_path_builder_append_current (self,
                                   GSK_PATH_CLOSE,
                                   1, (graphene_point_t[1]) {
                                     g_array_index (self->points, graphene_point_t, 0)
                                   });

  gsk_path_builder_end_current (self);
}

static void
arc_segment (GskPathBuilder *self,
             double          cx,
             double          cy,
             double          rx,
             double          ry,
             double          sin_phi,
             double          cos_phi,
             double          sin_th0,
             double          cos_th0,
             double          sin_th1,
             double          cos_th1,
             double          t)
{
  double x1, y1, x2, y2, x3, y3;

  x1 = rx * (cos_th0 - t * sin_th0);
  y1 = ry * (sin_th0 + t * cos_th0);
  x3 = rx * cos_th1;
  y3 = ry * sin_th1;
  x2 = x3 + rx * (t * sin_th1);
  y2 = y3 + ry * (-t * cos_th1);

  gsk_path_builder_cubic_to (self,
                             cx + cos_phi * x1 - sin_phi * y1,
                             cy + sin_phi * x1 + cos_phi * y1,
                             cx + cos_phi * x2 - sin_phi * y2,
                             cy + sin_phi * x2 + cos_phi * y2,
                             cx + cos_phi * x3 - sin_phi * y3,
                             cy + sin_phi * x3 + cos_phi * y3);
}

static inline void
_sincos (double angle,
         double *y,
         double *x)
{
#ifdef HAVE_SINCOS
  sincos (angle, y, x);
#else
  *x = cos (angle);
  *y = sin (angle);
#endif
}

/**
 * gsk_path_builder_svg_arc_to:
 * @self: a `GskPathBuilder`
 * @rx: X radius
 * @ry: Y radius
 * @x_axis_rotation: the rotation of the ellipsis
 * @large_arc: whether to add the large arc
 * @positive_sweep: whether to sweep in the positive direction
 * @x: the X coordinate of the endpoint
 * @y: the Y coordinate of the endpoint
 *
 * Implements arc-to according to the SVG spec.
 *
 * A convenience function that implements the
 * [SVG arc_to](https://www.w3.org/TR/SVG11/paths.html#PathDataEllipticalArcCommands)
 * functionality.
 *
 * After this, @x, @y will be the new current point.
 *
 * Since: 4.14
 */
void
gsk_path_builder_svg_arc_to (GskPathBuilder *self,
                             float           rx,
                             float           ry,
                             float           x_axis_rotation,
                             gboolean        large_arc,
                             gboolean        positive_sweep,
                             float           x,
                             float           y)
{
  graphene_point_t *current;
  double x1, y1, x2, y2;
  double phi, sin_phi, cos_phi;
  double mid_x, mid_y;
  double lambda;
  double d;
  double k;
  double x1_, y1_;
  double cx_, cy_;
  double  cx, cy;
  double ux, uy, u_len;
  double cos_theta1, theta1;
  double vx, vy, v_len;
  double dp_uv;
  double cos_delta_theta, delta_theta;
  int i, n_segs;
  double d_theta, theta;
  double sin_th0, cos_th0;
  double sin_th1, cos_th1;
  double th_half;
  double t;

  g_return_if_fail (self != NULL);

  if (self->points->len > 0)
    {
      current = &g_array_index (self->points, graphene_point_t, self->points->len - 1);
      x1 = current->x;
      y1 = current->y;
    }
  else
    {
      x1 = 0;
      y1 = 0;
    }
  x2 = x;
  y2 = y;

  phi = x_axis_rotation * M_PI / 180.0;
  _sincos (phi, &sin_phi, &cos_phi);

  rx = fabs (rx);
  ry = fabs (ry);

  mid_x = (x1 - x2) / 2;
  mid_y = (y1 - y2) / 2;

  x1_ = cos_phi * mid_x + sin_phi * mid_y;
  y1_ = - sin_phi * mid_x + cos_phi * mid_y;

  lambda = (x1_ / rx) * (x1_ / rx) + (y1_ / ry) * (y1_ / ry);
  if (lambda > 1)
    {
      lambda = sqrt (lambda);
      rx *= lambda;
      ry *= lambda;
    }

  d = (rx * y1_) * (rx * y1_) + (ry * x1_) * (ry * x1_);
  if (d == 0)
    return;

  k = sqrt (fabs ((rx * ry) * (rx * ry) / d - 1.0));
  if (positive_sweep == large_arc)
    k = -k;

  cx_ = k * rx * y1_ / ry;
  cy_ = -k * ry * x1_ / rx;

  cx = cos_phi * cx_ - sin_phi * cy_ + (x1 + x2) / 2;
  cy = sin_phi * cx_ + cos_phi * cy_ + (y1 + y2) / 2;

  ux = (x1_ - cx_) / rx;
  uy = (y1_ - cy_) / ry;
  u_len = sqrt (ux * ux + uy * uy);
  if (u_len == 0)
    return;

  cos_theta1 = CLAMP (ux / u_len, -1, 1);
  theta1 = acos (cos_theta1);
  if (uy < 0)
    theta1 = - theta1;

  vx = (- x1_ - cx_) / rx;
  vy = (- y1_ - cy_) / ry;
  v_len = sqrt (vx * vx + vy * vy);
  if (v_len == 0)
    return;

  dp_uv = ux * vx + uy * vy;
  cos_delta_theta = CLAMP (dp_uv / (u_len * v_len), -1, 1);
  delta_theta = acos (cos_delta_theta);
  if (ux * vy - uy * vx < 0)
    delta_theta = - delta_theta;
  if (positive_sweep && delta_theta < 0)
    delta_theta += 2 * M_PI;
  else if (!positive_sweep && delta_theta > 0)
    delta_theta -= 2 * M_PI;

  n_segs = ceil (fabs (delta_theta / (M_PI_2 + 0.001)));
  d_theta = delta_theta / n_segs;
  _sincos (theta1, &sin_th1, &cos_th1);

  th_half = d_theta / 2;
  t = (8.0 / 3.0) * sin (th_half / 2) * sin (th_half / 2) / sin (th_half);

  for (i = 0; i < n_segs; i++)
    {
      theta = theta1;
      theta1 = theta + d_theta;
      sin_th0 = sin_th1;
      cos_th0 = cos_th1;
      _sincos (theta1, &sin_th1, &cos_th1);
      arc_segment (self,
                   cx, cy, rx, ry,
                   sin_phi, cos_phi,
                   sin_th0, cos_th0,
                   sin_th1, cos_th1,
                   t);
    }
}

/**
 * gsk_path_builder_rel_svg_arc_to:
 * @self: a `GskPathBuilder`
 * @rx: X radius
 * @ry: Y radius
 * @x_axis_rotation: the rotation of the ellipsis
 * @large_arc: whether to add the large arc
 * @positive_sweep: whether to sweep in the positive direction
 * @x: the X coordinate of the endpoint
 * @y: the Y coordinate of the endpoint
 *
 * Implements arc-to according to the SVG spec.
 *
 * All coordinates are given relative to the current point.
 *
 * This is the relative version of [method@Gsk.PathBuilder.svg_arc_to].
 *
 * Since: 4.14
 */
void
gsk_path_builder_rel_svg_arc_to (GskPathBuilder *self,
                                 float           rx,
                                 float           ry,
                                 float           x_axis_rotation,
                                 gboolean        large_arc,
                                 gboolean        positive_sweep,
                                 float           x,
                                 float           y)
{
  gsk_path_builder_svg_arc_to (self,
                               rx, ry,
                               x_axis_rotation,
                               large_arc,
                               positive_sweep,
                               self->current_point.x + x,
                               self->current_point.y + y);
}

/* Return the angle between t1 and t2 in radians, such that
 * 0 means straight continuation
 * < 0 means right turn
 * > 0 means left turn
 */
static float
angle_between (const graphene_vec2_t *t1,
               const graphene_vec2_t *t2)
{
  float angle = atan2 (graphene_vec2_get_y (t2), graphene_vec2_get_x (t2))
                - atan2 (graphene_vec2_get_y (t1), graphene_vec2_get_x (t1));

  if (angle > M_PI)
    angle -= 2 * M_PI;
  if (angle < - M_PI)
    angle += 2 * M_PI;

  return angle;
}

#define RAD_TO_DEG(r) ((r)*180.f/M_PI)
#define DEG_TO_RAD(d) ((d)*M_PI/180.f)

static float
angle_between_points (const graphene_point_t *c,
                      const graphene_point_t *a,
                      const graphene_point_t *b)
{
  graphene_vec2_t t1, t2;

  graphene_vec2_init (&t1, a->x - c->x, a->y - c->y);
  graphene_vec2_init (&t2, b->x - c->x, b->y - c->y);

  return (float) RAD_TO_DEG (angle_between (&t1, &t2));
}

/**
 * gsk_path_builder_html_arc_to:
 * @self: a `GskPathBuilder`
 * @x1: X coordinate of first control point
 * @y1: Y coordinate of first control point
 * @x2: X coordinate of second control point
 * @y2: Y coordinate of second control point
 * @radius: Radius of the circle
 *
 * Implements arc-to according to the HTML Canvas spec.
 *
 * A convenience function that implements the
 * [HTML arc_to](https://html.spec.whatwg.org/multipage/canvas.html#dom-context-2d-arcto-dev)
 * functionality.
 *
 * After this, the current point will be the point where
 * the circle with the given radius touches the line from
 * @x1, @y1 to @x2, @y2.
 *
 * Since: 4.14
 */
void
gsk_path_builder_html_arc_to (GskPathBuilder *self,
                              float           x1,
                              float           y1,
                              float           x2,
                              float           y2,
                              float           radius)
{
  float angle, b;
  graphene_vec2_t t;
  graphene_point_t p, q;

  g_return_if_fail (self != NULL);
  g_return_if_fail (radius > 0);

  angle = angle_between_points (&GRAPHENE_POINT_INIT (x1, y1),
                                &self->current_point,
                                &GRAPHENE_POINT_INIT (x2, y2));

  if (fabsf (angle) < 3)
    {
      gsk_path_builder_line_to (self, x2, y2);
      return;
    }

  b = radius / tanf (fabsf ((float) DEG_TO_RAD (angle / 2)));

  graphene_vec2_init (&t, self->current_point.x - x1, self->current_point.y - y1);
  graphene_vec2_normalize (&t, &t);

  p.x = x1 + b * graphene_vec2_get_x (&t);
  p.y = y1 + b * graphene_vec2_get_y (&t);

  graphene_vec2_init (&t, x2 - x1, y2 - y1);
  graphene_vec2_normalize (&t, &t);

  q.x = x1 + b * graphene_vec2_get_x (&t);
  q.y = y1 + b * graphene_vec2_get_y (&t);

  gsk_path_builder_line_to (self, p.x, p.y);

  gsk_path_builder_svg_arc_to (self, radius, radius, 0, FALSE, angle < 0, q.x, q.y);
}

/**
 * gsk_path_builder_rel_html_arc_to:
 * @self: a `GskPathBuilder`
 * @x1: X coordinate of first control point
 * @y1: Y coordinate of first control point
 * @x2: X coordinate of second control point
 * @y2: Y coordinate of second control point
 * @radius: Radius of the circle
 *
 * Implements arc-to according to the HTML Canvas spec.
 *
 * All coordinates are given relative to the current point.
 *
 * This is the relative version of [method@Gsk.PathBuilder.html_arc_to].
 *
 * Since: 4.14
 */
void
gsk_path_builder_rel_html_arc_to (GskPathBuilder *self,
                                  float           x1,
                                  float           y1,
                                  float           x2,
                                  float           y2,
                                  float           radius)
{
  gsk_path_builder_html_arc_to (self,
                                self->current_point.x + x1,
                                self->current_point.y + y1,
                                self->current_point.x + x2,
                                self->current_point.y + y2,
                                radius);
}

/**
 * gsk_path_builder_add_layout:
 * @self: a #GskPathBuilder
 * @layout: the pango layout to add
 *
 * Adds the outlines for the glyphs in @layout to the builder.
 *
 * Since: 4.14
 */
void
gsk_path_builder_add_layout (GskPathBuilder *self,
                             PangoLayout    *layout)
{
  cairo_surface_t *surface;
  cairo_t *cr;
  cairo_path_t *cairo_path;

  surface = cairo_recording_surface_create (CAIRO_CONTENT_COLOR_ALPHA, NULL);
  cr = cairo_create (surface);

  pango_cairo_layout_path (cr, layout);
  cairo_path = cairo_copy_path (cr);

  gsk_path_builder_add_cairo_path (self, cairo_path);

  cairo_path_destroy (cairo_path);
  cairo_destroy (cr);
  cairo_surface_destroy (surface);
}

/**
 * gsk_path_builder_add_segment:
 * @self: a `GskPathBuilder`
 * @path: the `GskPath` to take the segment to
 * @start: the point on @path to start at
 * @end: the point on @path to end at
 *
 * Adds to @self the segment of @path from @start to @end.
 *
 * If @start is equal to or after @end, the path will first add the
 * segment from @start to the end of the path, and then add the segment
 * from the beginning to @end. If the path is closed, these segments
 * will be connected.
 *
 * Note that this method always adds a path with the given start point
 * and end point. To add a closed path, use [method@Gsk.PathBuilder.add_path].
 *
 * Since: 4.14
 */
void
gsk_path_builder_add_segment (GskPathBuilder     *self,
                              GskPath            *path,
                              const GskPathPoint *start,
                              const GskPathPoint *end)
{
  const GskContour *contour;
  gsize n_contours = gsk_path_get_n_contours (path);
  graphene_point_t current;
  gsize n_ops;

  g_return_if_fail (self != NULL);
  g_return_if_fail (path != NULL);
  g_return_if_fail (gsk_path_point_valid (start, path));
  g_return_if_fail (gsk_path_point_valid (end, path));

  current = self->current_point;

  contour = gsk_path_get_contour (path, start->contour);
  n_ops = gsk_contour_get_n_ops (contour);

  if (start->contour == end->contour)
    {
      if (gsk_path_point_compare (start, end) < 0)
        {
          gsk_contour_add_segment (contour, self, TRUE, start, end);
          goto out;
        }
      else if (n_contours == 1)
        {
          if (n_ops > 1)
            gsk_contour_add_segment (contour, self, TRUE,
                                     start,
                                     &GSK_PATH_POINT_INIT (start->contour, n_ops - 1, 1.f));
          gsk_contour_add_segment (contour, self, n_ops <= 1,
                                   &GSK_PATH_POINT_INIT (start->contour, 1, 0.f),
                                   end);
          goto out;
        }
    }

  if (n_ops > 1)
    gsk_contour_add_segment (contour, self, TRUE,
                             start,
                             &GSK_PATH_POINT_INIT (start->contour, n_ops - 1, 1.f));

  for (gsize i = (start->contour + 1) % n_contours; i != end->contour; i = (i + 1) % n_contours)
    gsk_path_builder_add_contour (self, gsk_contour_dup (gsk_path_get_contour (path, i)));

  contour = gsk_path_get_contour (path, end->contour);
  n_ops = gsk_contour_get_n_ops (contour);

  if (n_ops > 1)
    gsk_contour_add_segment (contour, self, TRUE,
                             &GSK_PATH_POINT_INIT (end->contour, 1, 0.f),
                             end);

out:
  gsk_path_builder_end_current (self);
  self->current_point = current;
}
