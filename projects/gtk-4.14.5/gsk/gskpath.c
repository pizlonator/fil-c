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

#include "gskpathprivate.h"

#include "gskcurveprivate.h"
#include "gskpathbuilder.h"
#include "gskpathpoint.h"
#include "gskcontourprivate.h"

/**
 * GskPath:
 *
 * A `GskPath` describes lines and curves that are more complex
 * than simple rectangles.
 *
 * Paths can used for rendering (filling or stroking) and for animations
 * (e.g. as trajectories).
 *
 * `GskPath` is an immutable, opaque, reference-counted struct.
 * After creation, you cannot change the types it represents. Instead,
 * new `GskPath` objects have to be created. The [struct@Gsk.PathBuilder]
 * structure is meant to help in this endeavor.
 *
 * Conceptually, a path consists of zero or more contours (continuous, connected
 * curves), each of which may or may not be closed. Contours are typically
 * constructed from Bézier segments.
 *
 * <picture>
 *   <source srcset="path-dark.png" media="(prefers-color-scheme: dark)">
 *   <img alt="A Path" src="path-light.png">
 * </picture>
 *
 * Since: 4.14
 */

struct _GskPath
{
  /*< private >*/
  guint ref_count;

  GskPathFlags flags;

  gsize n_contours;
  GskContour *contours[];
  /* followed by the contours data */
};

G_DEFINE_BOXED_TYPE (GskPath, gsk_path, gsk_path_ref, gsk_path_unref)

/* {{{ Private API */

GskPath *
gsk_path_new_from_contours (const GSList *contours)
{
  GskPath *path;
  const GSList *l;
  gsize size;
  gsize n_contours;
  guint8 *contour_data;
  GskPathFlags flags;

  flags = GSK_PATH_CLOSED | GSK_PATH_FLAT;
  size = 0;
  n_contours = 0;
  for (l = contours; l; l = l->next)
    {
      GskContour *contour = l->data;

      n_contours++;
      size += sizeof (GskContour *);
      size += gsk_contour_get_size (contour);
      flags &= gsk_contour_get_flags (contour);
    }

  path = g_malloc0 (sizeof (GskPath) + size);
  path->ref_count = 1;
  path->flags = flags;
  path->n_contours = n_contours;
  contour_data = (guint8 *) &path->contours[n_contours];
  n_contours = 0;

  for (l = contours; l; l = l->next)
    {
      GskContour *contour = l->data;

      path->contours[n_contours] = (GskContour *) contour_data;
      gsk_contour_copy ((GskContour *) contour_data, contour);
      size = gsk_contour_get_size (contour);
      contour_data += size;
      n_contours++;
    }

  return path;
}

const GskContour *
gsk_path_get_contour (const GskPath *self,
                      gsize          i)
{
  if (i < self->n_contours)
    return self->contours[i];
  else
    return NULL;
}

GskPathFlags
gsk_path_get_flags (const GskPath *self)
{
  return self->flags;
}

gsize
gsk_path_get_n_contours (const GskPath *self)
{
  return self->n_contours;
}

/* }}} */
/* {{{ Public API */

/**
 * gsk_path_ref:
 * @self: a `GskPath`
 *
 * Increases the reference count of a `GskPath` by one.
 *
 * Returns: the passed in `GskPath`.
 *
 * Since: 4.14
 */
GskPath *
gsk_path_ref (GskPath *self)
{
  g_return_val_if_fail (self != NULL, NULL);

  self->ref_count++;

  return self;
}

/**
 * gsk_path_unref:
 * @self: a `GskPath`
 *
 * Decreases the reference count of a `GskPath` by one.
 *
 * If the resulting reference count is zero, frees the path.
 *
 * Since: 4.14
 */
void
gsk_path_unref (GskPath *self)
{
  g_return_if_fail (self != NULL);
  g_return_if_fail (self->ref_count > 0);

  self->ref_count--;
  if (self->ref_count > 0)
    return;

  g_free (self);
}

/**
 * gsk_path_print:
 * @self: a `GskPath`
 * @string:  The string to print into
 *
 * Converts @self into a human-readable string representation suitable
 * for printing.
 *
 * The string is compatible with (a superset of)
 * [SVG path syntax](https://www.w3.org/TR/SVG11/paths.html#PathData),
 * see [func@Gsk.Path.parse] for a summary of the syntax.
 *
 * Since: 4.14
 */
void
gsk_path_print (GskPath *self,
                GString *string)
{
  gsize i;

  g_return_if_fail (self != NULL);
  g_return_if_fail (string != NULL);

  for (i = 0; i < self->n_contours; i++)
    {
      if (i > 0)
        g_string_append_c (string, ' ');
      gsk_contour_print (self->contours[i], string);
    }
}

/**
 * gsk_path_to_string:
 * @self: a `GskPath`
 *
 * Converts the path into a string that is suitable for printing.
 *
 * You can use this function in a debugger to get a quick overview
 * of the path.
 *
 * This is a wrapper around [method@Gsk.Path.print], see that function
 * for details.
 *
 * Returns: A new string for @self
 *
 * Since: 4.14
 */
char *
gsk_path_to_string (GskPath *self)
{
  GString *string;

  g_return_val_if_fail (self != NULL, NULL);

  string = g_string_new ("");

  gsk_path_print (self, string);

  return g_string_free (string, FALSE);
}

static gboolean
gsk_path_to_cairo_add_op (GskPathOperation        op,
                          const graphene_point_t *pts,
                          gsize                   n_pts,
                          float                   weight,
                          gpointer                cr)
{
  switch (op)
  {
    case GSK_PATH_MOVE:
      cairo_move_to (cr, pts[0].x, pts[0].y);
      break;

    case GSK_PATH_CLOSE:
      cairo_close_path (cr);
      break;

    case GSK_PATH_LINE:
      cairo_line_to (cr, pts[1].x, pts[1].y);
      break;

    case GSK_PATH_CUBIC:
      cairo_curve_to (cr, pts[1].x, pts[1].y, pts[2].x, pts[2].y, pts[3].x, pts[3].y);
      break;

    case GSK_PATH_QUAD:
    case GSK_PATH_CONIC:
    default:
      g_assert_not_reached ();
      return FALSE;
  }

  return TRUE;
}

/**
 * gsk_path_to_cairo:
 * @self: a `GskPath`
 * @cr: a cairo context
 *
 * Appends the given @path to the given cairo context for drawing
 * with Cairo.
 *
 * This may cause some suboptimal conversions to be performed as
 * Cairo does not support all features of `GskPath`.
 *
 * This function does not clear the existing Cairo path. Call
 * cairo_new_path() if you want this.
 *
 * Since: 4.14
 */
void
gsk_path_to_cairo (GskPath *self,
                   cairo_t *cr)
{
  g_return_if_fail (self != NULL);
  g_return_if_fail (cr != NULL);

  gsk_path_foreach_with_tolerance (self,
                                   GSK_PATH_FOREACH_ALLOW_CUBIC,
                                   cairo_get_tolerance (cr),
                                   gsk_path_to_cairo_add_op,
                                   cr);
}

/**
 * gsk_path_is_empty:
 * @self: a `GskPath`
 *
 * Checks if the path is empty, i.e. contains no lines or curves.
 *
 * Returns: `TRUE` if the path is empty
 *
 * Since: 4.14
 */
gboolean
gsk_path_is_empty (GskPath *self)
{
  g_return_val_if_fail (self != NULL, FALSE);

  return self->n_contours == 0;
}

/**
 * gsk_path_is_closed:
 * @self: a `GskPath`
 *
 * Returns if the path represents a single closed
 * contour.
 *
 * Returns: `TRUE` if the path is closed
 *
 * Since: 4.14
 */
gboolean
gsk_path_is_closed (GskPath *self)
{
  g_return_val_if_fail (self != NULL, FALSE);

  /* XXX: is the empty path closed? Currently it's not */
  if (self->n_contours != 1)
    return FALSE;

  return gsk_contour_get_flags (self->contours[0]) & GSK_PATH_CLOSED ? TRUE : FALSE;
}

/**
 * gsk_path_get_bounds:
 * @self: a `GskPath`
 * @bounds: (out caller-allocates): the bounds of the given path
 *
 * Computes the bounds of the given path.
 *
 * The returned bounds may be larger than necessary, because this
 * function aims to be fast, not accurate. The bounds are guaranteed
 * to contain the path.
 *
 * It is possible that the returned rectangle has 0 width and/or height.
 * This can happen when the path only describes a point or an
 * axis-aligned line.
 *
 * If the path is empty, `FALSE` is returned and @bounds are set to
 * graphene_rect_zero(). This is different from the case where the path
 * is a single point at the origin, where the @bounds will also be set to
 * the zero rectangle but `TRUE` will be returned.
 *
 * Returns: `TRUE` if the path has bounds, `FALSE` if the path is known
 *   to be empty and have no bounds.
 *
 * Since: 4.14
 */
gboolean
gsk_path_get_bounds (GskPath         *self,
                     graphene_rect_t *bounds)
{
  GskBoundingBox b;

  g_return_val_if_fail (self != NULL, FALSE);
  g_return_val_if_fail (bounds != NULL, FALSE);

  if (self->n_contours == 0)
    {
      graphene_rect_init_from_rect (bounds, graphene_rect_zero ());
      return FALSE;
    }

  gsk_contour_get_bounds (self->contours[0], &b);

  for (gsize i = 1; i < self->n_contours; i++)
    {
      GskBoundingBox tmp;

      gsk_contour_get_bounds (self->contours[i], &tmp);
      gsk_bounding_box_union (&b, &tmp, &b);
    }

  gsk_bounding_box_to_rect (&b, bounds);

  return TRUE;
}

/**
 * gsk_path_get_stroke_bounds:
 * @self: a #GtkPath
 * @stroke: stroke parameters
 * @bounds: (out caller-allocates): the bounds to fill in
 *
 * Computes the bounds for stroking the given path with the
 * parameters in @stroke.
 *
 * The returned bounds may be larger than necessary, because this
 * function aims to be fast, not accurate. The bounds are guaranteed
 * to contain the area affected by the stroke, including protrusions
 * like miters.
 *
 * Returns: `TRUE` if the path has bounds, `FALSE` if the path is known
 *   to be empty and have no bounds.
 *
 * Since: 4.14
 */
gboolean
gsk_path_get_stroke_bounds (GskPath         *self,
                            const GskStroke *stroke,
                            graphene_rect_t *bounds)
{
  GskBoundingBox b;

  g_return_val_if_fail (self != NULL, FALSE);
  g_return_val_if_fail (bounds != NULL, FALSE);

  if (self->n_contours == 0)
    {
      graphene_rect_init_from_rect (bounds, graphene_rect_zero ());
      return FALSE;
    }

  gsk_contour_get_stroke_bounds (self->contours[0], stroke, &b);

  for (gsize i = 1; i < self->n_contours; i++)
    {
      GskBoundingBox tmp;

      if (gsk_contour_get_stroke_bounds (self->contours[i], stroke, &tmp))
        gsk_bounding_box_union (&b, &tmp, &b);
    }

  gsk_bounding_box_to_rect (&b, bounds);

  return TRUE;
}

/**
 * gsk_path_in_fill:
 * @self: a `GskPath`
 * @point: the point to test
 * @fill_rule: the fill rule to follow
 *
 * Returns whether the given point is inside the area
 * that would be affected if the path was filled according
 * to @fill_rule.
 *
 * Note that this function assumes that filling a contour
 * implicitly closes it.
 *
 * Returns: `TRUE` if @point is inside
 *
 * Since: 4.14
 */
gboolean
gsk_path_in_fill (GskPath                *self,
                  const graphene_point_t *point,
                  GskFillRule             fill_rule)
{
  int winding = 0;

  for (int i = 0; i < self->n_contours; i++)
    winding += gsk_contour_get_winding (self->contours[i], point);

  switch (fill_rule)
    {
    case GSK_FILL_RULE_EVEN_ODD:
      return winding & 1;
    case GSK_FILL_RULE_WINDING:
      return winding != 0;
    default:
      g_assert_not_reached ();
    }
}

/**
 * gsk_path_get_start_point:
 * @self: a `GskPath`
 * @result: (out caller-allocates): return location for point
 *
 * Gets the start point of the path.
 *
 * An empty path has no points, so `FALSE`
 * is returned in this case.
 *
 * Returns: `TRUE` if @result was filled
 *
 * Since: 4.14
 */
gboolean
gsk_path_get_start_point (GskPath      *self,
                          GskPathPoint *result)
{
  g_return_val_if_fail (self != NULL, FALSE);
  g_return_val_if_fail (result != NULL, FALSE);

  if (self->n_contours == 0)
    return FALSE;

  /* Conceptually, there is always a move at the
   * beginning, which jumps from where to the start
   * point of the contour, so we use idx == 1 here.
   */
  result->contour = 0;
  result->idx = 1;
  result->t = 0;

  return TRUE;
}

/**
 * gsk_path_get_end_point:
 * @self: a `GskPath`
 * @result: (out caller-allocates): return location for point
 *
 * Gets the end point of the path.
 *
 * An empty path has no points, so `FALSE`
 * is returned in this case.
 *
 * Returns: `TRUE` if @result was filled
 *
 * Since: 4.14
 */
gboolean
gsk_path_get_end_point (GskPath      *self,
                        GskPathPoint *result)
{
  g_return_val_if_fail (self != NULL, FALSE);
  g_return_val_if_fail (result != NULL, FALSE);

  if (self->n_contours == 0)
    return FALSE;

  result->contour = self->n_contours - 1;
  result->idx = gsk_contour_get_n_ops (self->contours[self->n_contours - 1]) - 1;
  result->t = 1;

  return TRUE;
}

/**
 * gsk_path_get_closest_point:
 * @self: a `GskPath`
 * @point: the point
 * @threshold: maximum allowed distance
 * @result: (out caller-allocates): return location for the closest point
 * @distance: (out) (optional): return location for the distance
 *
 * Computes the closest point on the path to the given point
 * and sets the @result to it.
 *
 * If there is no point closer than the given threshold,
 * `FALSE` is returned.
 *
 * Returns: `TRUE` if @point was set to the closest point
 *   on @self, `FALSE` if no point is closer than @threshold
 *
 * Since: 4.14
 */
gboolean
gsk_path_get_closest_point (GskPath                *self,
                            const graphene_point_t *point,
                            float                   threshold,
                            GskPathPoint           *result,
                            float                  *distance)
{
  gboolean found;

  g_return_val_if_fail (self != NULL, FALSE);
  g_return_val_if_fail (point != NULL, FALSE);
  g_return_val_if_fail (threshold >= 0, FALSE);
  g_return_val_if_fail (result != NULL, FALSE);

  found = FALSE;

  for (int i = 0; i < self->n_contours; i++)
    {
      float dist;

      if (gsk_contour_get_closest_point (self->contours[i], point, threshold, result, &dist))
        {
          found = TRUE;
          g_assert (0 <= result->t && result->t <= 1);
          result->contour = i;
          threshold = dist;

          if (distance)
            *distance = dist;
        }
    }

  return found;
}

/* }}} */
/* {{{ Foreach and decomposition */

/**
 * gsk_path_foreach:
 * @self: a `GskPath`
 * @flags: flags to pass to the foreach function. See [flags@Gsk.PathForeachFlags]
 *   for details about flags
 * @func: (scope call) (closure user_data): the function to call for operations
 * @user_data: (nullable): user data passed to @func
 *
 * Calls @func for every operation of the path.
 *
 * Note that this may only approximate @self, because paths can contain
 * optimizations for various specialized contours, and depending on the
 * @flags, the path may be decomposed into simpler curves than the ones
 * that it contained originally.
 *
 * This function serves two purposes:
 *
 * - When the @flags allow everything, it provides access to the raw,
 *   unmodified data of the path.
 * - When the @flags disallow certain operations, it provides
 *   an approximation of the path using just the allowed operations.
 *
 * Returns: `FALSE` if @func returned FALSE`, `TRUE` otherwise.
 *
 * Since: 4.14
 */
gboolean
gsk_path_foreach (GskPath             *self,
                  GskPathForeachFlags  flags,
                  GskPathForeachFunc   func,
                  gpointer             user_data)
{
  g_return_val_if_fail (self != NULL, FALSE);
  g_return_val_if_fail (func, FALSE);

  return gsk_path_foreach_with_tolerance (self,
                                          flags,
                                          GSK_PATH_TOLERANCE_DEFAULT,
                                          func,
                                          user_data);
}

typedef struct _GskPathForeachTrampoline GskPathForeachTrampoline;
struct _GskPathForeachTrampoline
{
  GskPathForeachFlags flags;
  double tolerance;
  GskPathForeachFunc func;
  gpointer user_data;
};

static gboolean
gsk_path_foreach_trampoline_add_line (const graphene_point_t *from,
                                      const graphene_point_t *to,
                                      float                   from_progress,
                                      float                   to_progress,
                                      GskCurveLineReason      reason,
                                      gpointer                data)
{
  GskPathForeachTrampoline *trampoline = data;

  return trampoline->func (GSK_PATH_LINE,
                           (graphene_point_t[2]) { *from, *to },
                           2,
                           0.f,
                           trampoline->user_data);
}

static gboolean
gsk_path_foreach_trampoline_add_curve (GskPathOperation        op,
                                       const graphene_point_t *pts,
                                       gsize                   n_pts,
                                       float                   weight,
                                       gpointer                data)
{
  GskPathForeachTrampoline *trampoline = data;

  return trampoline->func (op, pts, n_pts, weight, trampoline->user_data);
}

static gboolean
gsk_path_foreach_trampoline (GskPathOperation        op,
                             const graphene_point_t *pts,
                             gsize                   n_pts,
                             float                   weight,
                             gpointer                data)
{
  GskPathForeachTrampoline *trampoline = data;

  switch (op)
    {
    case GSK_PATH_MOVE:
    case GSK_PATH_CLOSE:
    case GSK_PATH_LINE:
      return trampoline->func (op, pts, n_pts, weight, trampoline->user_data);

    case GSK_PATH_QUAD:
      {
        GskCurve curve;

        if (trampoline->flags & GSK_PATH_FOREACH_ALLOW_QUAD)
          return trampoline->func (op, pts, n_pts, weight, trampoline->user_data);
        else if (trampoline->flags & GSK_PATH_FOREACH_ALLOW_CUBIC)
          {
            return trampoline->func (GSK_PATH_CUBIC,
                                     (graphene_point_t[4]) {
                                       pts[0],
                                       GRAPHENE_POINT_INIT ((pts[0].x + 2 * pts[1].x) / 3,
                                                            (pts[0].y + 2 * pts[1].y) / 3),
                                       GRAPHENE_POINT_INIT ((pts[2].x + 2 * pts[1].x) / 3,
                                                            (pts[2].y + 2 * pts[1].y) / 3),
                                       pts[2],
                                     },
                                     4,
                                     weight,
                                     trampoline->user_data);
          }

        gsk_curve_init (&curve, gsk_pathop_encode (GSK_PATH_QUAD, pts));
        return gsk_curve_decompose (&curve,
                                    trampoline->tolerance,
                                    gsk_path_foreach_trampoline_add_line,
                                    trampoline);
      }

    case GSK_PATH_CUBIC:
      {
        GskCurve curve;

        if (trampoline->flags & GSK_PATH_FOREACH_ALLOW_CUBIC)
          return trampoline->func (op, pts, n_pts, weight, trampoline->user_data);

        gsk_curve_init (&curve, gsk_pathop_encode (GSK_PATH_CUBIC, pts));
        if (trampoline->flags & (GSK_PATH_FOREACH_ALLOW_QUAD|GSK_PATH_FOREACH_ALLOW_CONIC))
          return gsk_curve_decompose_curve (&curve,
                                            trampoline->flags,
                                            trampoline->tolerance,
                                            gsk_path_foreach_trampoline_add_curve,
                                            trampoline);

        return gsk_curve_decompose (&curve,
                                    trampoline->tolerance,
                                    gsk_path_foreach_trampoline_add_line,
                                    trampoline);
      }

    case GSK_PATH_CONIC:
      {
        GskCurve curve;

        if (trampoline->flags & GSK_PATH_FOREACH_ALLOW_CONIC)
          return trampoline->func (op, pts, n_pts, weight, trampoline->user_data);

        gsk_curve_init (&curve, gsk_pathop_encode (GSK_PATH_CONIC, (graphene_point_t[4]) { pts[0], pts[1], { weight, 0.f }, pts[2] } ));
        if (trampoline->flags & (GSK_PATH_FOREACH_ALLOW_QUAD|GSK_PATH_FOREACH_ALLOW_CUBIC))
          return gsk_curve_decompose_curve (&curve,
                                            trampoline->flags,
                                            trampoline->tolerance,
                                            gsk_path_foreach_trampoline_add_curve,
                                            trampoline);

        return gsk_curve_decompose (&curve,
                                    trampoline->tolerance,
                                    gsk_path_foreach_trampoline_add_line,
                                    trampoline);
      }

    default:
      g_assert_not_reached ();
      return FALSE;
    }
}

#define ALLOW_ANY (GSK_PATH_FOREACH_ALLOW_QUAD  | \
                   GSK_PATH_FOREACH_ALLOW_CUBIC | \
                   GSK_PATH_FOREACH_ALLOW_CONIC)

gboolean
gsk_path_foreach_with_tolerance (GskPath             *self,
                                 GskPathForeachFlags  flags,
                                 double               tolerance,
                                 GskPathForeachFunc   func,
                                 gpointer             user_data)
{
  GskPathForeachTrampoline trampoline;
  gsize i;

  /* If we need to massage the data, set up a trampoline here */
  if ((flags & ALLOW_ANY) != ALLOW_ANY)
    {
      trampoline = (GskPathForeachTrampoline) { flags, tolerance, func, user_data };
      func = gsk_path_foreach_trampoline;
      user_data = &trampoline;
    }

  for (i = 0; i < self->n_contours; i++)
    {
      if (!gsk_contour_foreach (self->contours[i], func, user_data))
        return FALSE;
    }

  return TRUE;
}

/* }}} */

/* vim:set foldmethod=marker expandtab: */
