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

#include "gskpathmeasure.h"

#include "gskpathbuilder.h"
#include "gskpathpointprivate.h"
#include "gskcontourprivate.h"
#include "gskpathprivate.h"

/**
 * GskPathMeasure:
 *
 * `GskPathMeasure` is an object that allows measurements
 * on `GskPath`s such as determining the length of the path.
 *
 * Many measuring operations require sampling the path length
 * at intermediate points. Therefore, a `GskPathMeasure` has
 * a tolerance that determines what precision is required
 * for such approximations.
 *
 * A `GskPathMeasure` struct is a reference counted struct
 * and should be treated as opaque.
 *
 * Since: 4.14
 */

typedef struct _GskContourMeasure GskContourMeasure;

struct _GskContourMeasure
{
  float length;
  gpointer contour_data;
};

struct _GskPathMeasure
{
  /*< private >*/
  guint ref_count;

  GskPath *path;
  float tolerance;

  float length;
  gsize n_contours;
  GskContourMeasure measures[];
};

G_DEFINE_BOXED_TYPE (GskPathMeasure, gsk_path_measure,
                     gsk_path_measure_ref,
                     gsk_path_measure_unref)

/**
 * gsk_path_measure_new:
 * @path: the path to measure
 *
 * Creates a measure object for the given @path with the
 * default tolerance.
 *
 * Returns: a new `GskPathMeasure` representing @path
 *
 * Since: 4.14
 */
GskPathMeasure *
gsk_path_measure_new (GskPath *path)
{
  return gsk_path_measure_new_with_tolerance (path, GSK_PATH_TOLERANCE_DEFAULT);
}

/**
 * gsk_path_measure_new_with_tolerance:
 * @path: the path to measure
 * @tolerance: the tolerance for measuring operations
 *
 * Creates a measure object for the given @path and @tolerance.
 *
 * Returns: a new `GskPathMeasure` representing @path
 *
 * Since: 4.14
 */
GskPathMeasure *
gsk_path_measure_new_with_tolerance (GskPath *path,
                                     float    tolerance)
{
  GskPathMeasure *self;
  gsize i, n_contours;

  g_return_val_if_fail (path != NULL, NULL);
  g_return_val_if_fail (tolerance > 0, NULL);

  n_contours = gsk_path_get_n_contours (path);

  self = g_malloc0 (sizeof (GskPathMeasure) + n_contours * sizeof (GskContourMeasure));

  self->ref_count = 1;
  self->path = gsk_path_ref (path);
  self->tolerance = tolerance;
  self->n_contours = n_contours;

  for (i = 0; i < n_contours; i++)
    {
      self->measures[i].contour_data = gsk_contour_init_measure (gsk_path_get_contour (path, i),
                                                                 self->tolerance,
                                                                 &self->measures[i].length);
      self->length += self->measures[i].length;
    }

  return self;
}

/**
 * gsk_path_measure_ref:
 * @self: a `GskPathMeasure`
 *
 * Increases the reference count of a `GskPathMeasure` by one.
 *
 * Returns: the passed in `GskPathMeasure`.
 *
 * Since: 4.14
 */
GskPathMeasure *
gsk_path_measure_ref (GskPathMeasure *self)
{
  g_return_val_if_fail (self != NULL, NULL);

  self->ref_count++;

  return self;
}

/**
 * gsk_path_measure_unref:
 * @self: a `GskPathMeasure`
 *
 * Decreases the reference count of a `GskPathMeasure` by one.
 *
 * If the resulting reference count is zero, frees the object.
 *
 * Since: 4.14
 */
void
gsk_path_measure_unref (GskPathMeasure *self)
{
  gsize i;

  g_return_if_fail (self != NULL);
  g_return_if_fail (self->ref_count > 0);

  self->ref_count--;
  if (self->ref_count > 0)
    return;

  for (i = 0; i < self->n_contours; i++)
    {
      gsk_contour_free_measure (gsk_path_get_contour (self->path, i),
                                self->measures[i].contour_data);
    }

  gsk_path_unref (self->path);
  g_free (self);
}

/**
 * gsk_path_measure_get_path:
 * @self: a `GskPathMeasure`
 *
 * Returns the path that the measure was created for.
 *
 * Returns: (transfer none): the path of @self
 *
 * Since: 4.14
 */
GskPath *
gsk_path_measure_get_path (GskPathMeasure *self)
{
  return self->path;
}

/**
 * gsk_path_measure_get_tolerance:
 * @self: a `GskPathMeasure`
 *
 * Returns the tolerance that the measure was created with.
 *
 * Returns: the tolerance of @self
 *
 * Since: 4.14
 */
float
gsk_path_measure_get_tolerance (GskPathMeasure *self)
{
  return self->tolerance;
}

/**
 * gsk_path_measure_get_length:
 * @self: a `GskPathMeasure`
 *
 * Gets the length of the path being measured.
 *
 * The length is cached, so this function does not do any work.
 *
 * Returns: The length of the path measured by @self
 *
 * Since: 4.14
 */
float
gsk_path_measure_get_length (GskPathMeasure *self)
{
  g_return_val_if_fail (self != NULL, 0);

  return self->length;
}

static float
gsk_path_measure_clamp_distance (GskPathMeasure *self,
                                 float           distance)
{
  if (isnan (distance))
    return 0;

  return CLAMP (distance, 0, self->length);
}

/**
 * gsk_path_measure_get_point:
 * @self: a `GskPathMeasure`
 * @distance: the distance
 * @result: (out caller-allocates): return location for the result
 *
 * Sets @result to the point at the given distance into the path.
 *
 * An empty path has no points, so `FALSE` is returned in that case.
 *
 * Returns: `TRUE` if @result was set
 *
 * Since: 4.14
 */
gboolean
gsk_path_measure_get_point (GskPathMeasure *self,
                            float           distance,
                            GskPathPoint   *result)
{
  gsize i;
  const GskContour *contour;

  g_return_val_if_fail (self != NULL, FALSE);
  g_return_val_if_fail (result != NULL, FALSE);

  if (self->n_contours == 0)
    return FALSE;

  distance = gsk_path_measure_clamp_distance (self, distance);

  for (i = 0; i < self->n_contours - 1; i++)
    {
      if (distance < self->measures[i].length)
        break;

      distance -= self->measures[i].length;
    }

  g_assert (0 <= i && i < self->n_contours);

  distance = CLAMP (distance, 0, self->measures[i].length);

  contour = gsk_path_get_contour (self->path, i);

  gsk_contour_get_point (contour, self->measures[i].contour_data, distance, result);

  g_assert (0 <= result->t && result->t <= 1);

  result->contour = i;

  return TRUE;
}

/**
 * gsk_path_point_get_distance:
 * @point: a `GskPathPoint on the path
 * @measure: a `GskPathMeasure` for the path
 *
 * Returns the distance from the beginning of the path
 * to @point.
 *
 * Returns: the distance of @point
 *
 * Since: 4.14
 */
float
gsk_path_point_get_distance (const GskPathPoint *point,
                             GskPathMeasure     *measure)
{
  float contour_offset = 0;

  g_return_val_if_fail (measure != NULL, 0);
  g_return_val_if_fail (gsk_path_point_valid (point, measure->path), 0);

  for (gsize i = 0; i < measure->n_contours; i++)
    {
      if (i == point->contour)
        return contour_offset + gsk_contour_get_distance (gsk_path_get_contour (measure->path, i),
                                                          point,
                                                          measure->measures[i].contour_data);

      contour_offset += measure->measures[i].length;
    }

  g_return_val_if_reached (0);
}
