/*
 * Copyright © 2023 Red Hat, Inc.
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
 * Authors: Matthias Clasen <mclasen@redhat.com>
 */

#include "config.h"

#include <math.h>

#include "gskpathpointprivate.h"
#include "gskcontourprivate.h"
#include "gdk/gdkprivate.h"

#define RAD_TO_DEG(x)          ((x) / (G_PI / 180.f))

/**
 * GskPathPoint:
 *
 * `GskPathPoint` is an opaque type representing a point on a path.
 *
 * It can be queried for properties of the path at that point, such as
 * its tangent or its curvature.
 *
 * To obtain a `GskPathPoint`, use [method@Gsk.Path.get_closest_point],
 * [method@Gsk.Path.get_start_point], [method@Gsk.Path.get_end_point]
 * or [method@Gsk.PathMeasure.get_point].
 *
 * Note that `GskPathPoint` structs are meant to be stack-allocated,
 * and don't hold a reference to the path object they are obtained from.
 * It is the callers responsibility to keep a reference to the path
 * as long as the `GskPathPoint` is used.
 *
 * Since: 4.14
 */

G_DEFINE_BOXED_TYPE (GskPathPoint, gsk_path_point,
                     gsk_path_point_copy,
                     gsk_path_point_free)

GskPathPoint *
gsk_path_point_copy (GskPathPoint *point)
{
  GskPathPoint *copy;

  copy = g_new0 (GskPathPoint, 1);

  memcpy (copy, point, sizeof (GskPathPoint));

  return copy;
}

void
gsk_path_point_free (GskPathPoint *point)
{
  g_free (point);
}

/**
 * gsk_path_point_equal:
 * @point1: a `GskPathPoint`
 * @point2: another `GskPathPoint`
 *
 * Returns whether the two path points refer to the same
 * location on all paths.
 *
 * Note that the start- and endpoint of a closed contour
 * will compare nonequal according to this definition.
 * Use [method@Gsk.Path.is_closed] to find out if the
 * start- and endpoint of a concrete path refer to the
 * same location.
 *
 * Return: `TRUE` if @point1 and @point2 are equal
 *
 * Since: 4.14
 */
gboolean
gsk_path_point_equal (const GskPathPoint *point1,
                      const GskPathPoint *point2)
{
  if (point1->contour == point2->contour)
    {
      if ((point1->idx     == point2->idx     && point1->t == point2->t) ||
          (point1->idx + 1 == point2->idx     && point1->t == 1 && point2->t == 0) ||
          (point1->idx     == point2->idx + 1 && point1->t == 0 && point2->t == 1))
        return TRUE;
    }

  return FALSE;
}

/**
 * gsk_path_point_compare:
 * @point1: a `GskPathPoint`
 * @point2: another `GskPathPoint`
 *
 * Returns whether @point1 is before or after @point2.
 *
 * Returns: -1 if @point1 is before @point2,
 *   1 if @point1 is after @point2,
 *   0 if they are equal
 *
 * Since: 4.14
 */
int
gsk_path_point_compare (const GskPathPoint *point1,
                        const GskPathPoint *point2)
{
  if (gsk_path_point_equal (point1, point2))
    return 0;

  if (point1->contour < point2->contour)
    return -1;
  else if (point1->contour > point2->contour)
    return 1;
  else if (point1->idx < point2->idx)
    return -1;
  else if (point1->idx > point2->idx)
    return 1;
  else if (point1->t < point2->t)
    return -1;
  else if (point1->t > point2->t)
    return 1;

  return 0;
}

/**
 * gsk_path_point_get_position:
 * @point: a `GskPathPoint`
 * @path: the path that @point is on
 * @position: (out caller-allocates): Return location for
 *   the coordinates of the point
 *
 * Gets the position of the point.
 *
 * Since: 4.14
 */
void
gsk_path_point_get_position (const GskPathPoint *point,
                             GskPath            *path,
                             graphene_point_t   *position)
{
  const GskContour *contour;

  g_return_if_fail (path != NULL);
  g_return_if_fail (gsk_path_point_valid (point, path));
  g_return_if_fail (position != NULL);

  contour = gsk_path_get_contour (path, point->contour),
  gsk_contour_get_position (contour, point, position);
}

/**
 * gsk_path_point_get_tangent:
 * @point: a `GskPathPoint`
 * @path: the path that @point is on
 * @direction: the direction for which to return the tangent
 * @tangent: (out caller-allocates): Return location for
 *   the tangent at the point
 *
 * Gets the tangent of the path at the point.
 *
 * Note that certain points on a path may not have a single
 * tangent, such as sharp turns. At such points, there are
 * two tangents -- the direction of the path going into the
 * point, and the direction coming out of it. The @direction
 * argument lets you choose which one to get.
 *
 * If the path is just a single point (e.g. a circle with
 * radius zero), then @tangent is set to `0, 0`.
 *
 * If you want to orient something in the direction of the
 * path, [method@Gsk.PathPoint.get_rotation] may be more
 * convenient to use.
 *
 * Since: 4.14
 */
void
gsk_path_point_get_tangent (const GskPathPoint *point,
                            GskPath            *path,
                            GskPathDirection    direction,
                            graphene_vec2_t    *tangent)
{
  const GskContour *contour;

  g_return_if_fail (path != NULL);
  g_return_if_fail (gsk_path_point_valid (point, path));
  g_return_if_fail (tangent != NULL);

  contour = gsk_path_get_contour (path, point->contour),
  gsk_contour_get_tangent (contour, point, direction, tangent);
}

/**
 * gsk_path_point_get_rotation:
 * @point: a `GskPathPoint`
 * @path: the path that @point is on
 * @direction: the direction for which to return the rotation
 *
 * Gets the direction of the tangent at a given point.
 *
 * This is a convenience variant of [method@Gsk.PathPoint.get_tangent]
 * that returns the angle between the tangent and the X axis. The angle
 * can e.g. be used in
 * [gtk_snapshot_rotate()](../gtk4/method.Snapshot.rotate.html).
 *
 * Returns: the angle between the tangent and the X axis, in degrees
 *
 * Since: 4.14
 */
float
gsk_path_point_get_rotation (const GskPathPoint *point,
                             GskPath            *path,
                             GskPathDirection    direction)
{
  graphene_vec2_t tangent;

  g_return_val_if_fail (path != NULL, 0);
  g_return_val_if_fail (gsk_path_point_valid (point, path), 0);

  gsk_path_point_get_tangent (point, path, direction, &tangent);

  return RAD_TO_DEG (atan2f (graphene_vec2_get_y (&tangent),
                             graphene_vec2_get_x (&tangent)));
}

/**
 * gsk_path_point_get_curvature:
 * @point: a `GskPathPoint`
 * @path: the path that @point is on
 * @direction: the direction for which to return the curvature
 * @center: (out caller-allocates) (nullable): Return location for
 *   the center of the osculating circle
 *
 * Calculates the curvature of the path at the point.
 *
 * Optionally, returns the center of the osculating circle as well.
 * The curvature is the inverse of the radius of the osculating circle.
 *
 * Lines have a curvature of zero (indicating an osculating circle of
 * infinite radius. In this case, the @center is not modified.
 *
 * Circles with a radius of zero have `INFINITY` as curvature
 *
 * Note that certain points on a path may not have a single curvature,
 * such as sharp turns. At such points, there are two curvatures --
 * the (limit of) the curvature of the path going into the point,
 * and the (limit of) the curvature of the path coming out of it.
 * The @direction argument lets you choose which one to get.
 *
 * <picture>
 *   <source srcset="curvature-dark.png" media="(prefers-color-scheme: dark)">
 *   <img alt="Osculating circle" src="curvature-light.png">
 * </picture>

 * Returns: The curvature of the path at the given point
 *
 * Since: 4.14
 */
float
gsk_path_point_get_curvature (const GskPathPoint *point,
                              GskPath            *path,
                              GskPathDirection    direction,
                              graphene_point_t   *center)
{
  const GskContour *contour;

  g_return_val_if_fail (path != NULL, 0);
  g_return_val_if_fail (gsk_path_point_valid (point, path), 0);

  contour = gsk_path_get_contour (path, point->contour);
  return gsk_contour_get_curvature (contour, point, direction, center);
}
