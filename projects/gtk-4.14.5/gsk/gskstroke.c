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

#include "gskstrokeprivate.h"

/**
 * GskStroke:
 *
 * A `GskStroke` struct collects the parameters that influence
 * the operation of stroking a path.
 *
 * Since: 4.14
 */

G_DEFINE_BOXED_TYPE (GskStroke, gsk_stroke, gsk_stroke_copy, gsk_stroke_free)


/**
 * gsk_stroke_new:
 * @line_width: line width of the stroke. Must be > 0
 *
 * Creates a new `GskStroke` with the given @line_width.
 *
 * Returns: a new `GskStroke`
 *
 * Since: 4.14
 */
GskStroke *
gsk_stroke_new (float line_width)
{
  GskStroke *self;

  g_return_val_if_fail (line_width > 0, NULL);

  self = g_new0 (GskStroke, 1);

  self->line_width = line_width;
  self->line_cap = GSK_LINE_CAP_BUTT;
  self->line_join = GSK_LINE_JOIN_MITER;
  self->miter_limit = 4.f; /* following svg */

  return self;
}

/**
 * gsk_stroke_copy:
 * @other: `GskStroke` to copy
 *
 * Creates a copy of the given @other stroke.
 *
 * Returns: a new `GskStroke`. Use [method@Gsk.Stroke.free] to free it
 *
 * Since: 4.14
 */
GskStroke *
gsk_stroke_copy (const GskStroke *other)
{
  GskStroke *self;

  g_return_val_if_fail (other != NULL, NULL);

  self = g_new (GskStroke, 1);

  *self = GSK_STROKE_INIT_COPY (other);

  return self;
}

/**
 * gsk_stroke_free:
 * @self: a `GskStroke`
 *
 * Frees a `GskStroke`.
 *
 * Since: 4.14
 */
void
gsk_stroke_free (GskStroke *self)
{
  if (self == NULL)
    return;

  gsk_stroke_clear (self);

  g_free (self);
}

/**
 * gsk_stroke_to_cairo:
 * @self: a `GskStroke`
 * @cr: the cairo context to configure
 *
 * A helper function that sets the stroke parameters
 * of @cr from the values found in @self.
 *
 * Since: 4.14
 */
void
gsk_stroke_to_cairo (const GskStroke *self,
                     cairo_t         *cr)
{
  cairo_set_line_width (cr, self->line_width);

  /* gcc can optimize that to a direct case. This catches later additions to the enum */
  switch (self->line_cap)
    {
      case GSK_LINE_CAP_BUTT:
        cairo_set_line_cap (cr, CAIRO_LINE_CAP_BUTT);
        break;
      case GSK_LINE_CAP_ROUND:
        cairo_set_line_cap (cr, CAIRO_LINE_CAP_ROUND);
        break;
      case GSK_LINE_CAP_SQUARE:
        cairo_set_line_cap (cr, CAIRO_LINE_CAP_SQUARE);
        break;
      default:
        g_assert_not_reached ();
        break;
    }

  /* gcc can optimize that to a direct case. This catches later additions to the enum */
  switch (self->line_join)
    {
      case GSK_LINE_JOIN_MITER:
        cairo_set_line_join (cr, CAIRO_LINE_JOIN_MITER);
        break;
      case GSK_LINE_JOIN_ROUND:
        cairo_set_line_join (cr, CAIRO_LINE_JOIN_ROUND);
        break;
      case GSK_LINE_JOIN_BEVEL:
        cairo_set_line_join (cr, CAIRO_LINE_JOIN_BEVEL);
        break;
      default:
        g_assert_not_reached ();
        break;
    }

  cairo_set_miter_limit (cr, self->miter_limit);

  if (self->dash_length)
    {
      gsize i;
      double *dash = g_newa (double, self->n_dash);

      for (i = 0; i < self->n_dash; i++)
        {
          dash[i] = self->dash[i];
        }
      cairo_set_dash (cr, dash, self->n_dash, self->dash_offset);
    }
  else
    cairo_set_dash (cr, NULL, 0, 0.0);
}

/**
 * gsk_stroke_equal:
 * @stroke1: the first `GskStroke`
 * @stroke2: the second `GskStroke`
 *
 * Checks if 2 strokes are identical.
 *
 * Returns: `TRUE` if the 2 strokes are equal, `FALSE` otherwise
 *
 * Since: 4.14
 */
gboolean
gsk_stroke_equal (gconstpointer stroke1,
                  gconstpointer stroke2)
{
  const GskStroke *self1 = stroke1;
  const GskStroke *self2 = stroke2;

  if (self1->line_width != self2->line_width ||
      self1->line_cap != self2->line_cap ||
      self1->line_join != self2->line_join ||
      self1->miter_limit != self2->miter_limit ||
      self1->n_dash != self2->n_dash ||
      self1->dash_offset != self2->dash_offset)
    return FALSE;

  for (gsize i = 0; i < self1->n_dash; i++)
    if (self1->dash[i] != self2->dash[i])
      return FALSE;

  return TRUE;
}

/**
 * gsk_stroke_set_line_width:
 * @self: a `GskStroke`
 * @line_width: width of the line in pixels
 *
 * Sets the line width to be used when stroking.
 *
 * The line width must be > 0.
 *
 * Since: 4.14
 */
void
gsk_stroke_set_line_width (GskStroke *self,
                           float      line_width)
{
  g_return_if_fail (self != NULL);
  g_return_if_fail (line_width > 0);

  self->line_width = line_width;
}

/**
 * gsk_stroke_get_line_width:
 * @self: a `GskStroke`
 *
 * Gets the line width used.
 *
 * Returns: The line width
 *
 * Since: 4.14
 */
float
gsk_stroke_get_line_width (const GskStroke *self)
{
  g_return_val_if_fail (self != NULL, 0.0);

  return self->line_width;
}

/**
 * gsk_stroke_set_line_cap:
 * @self: a`GskStroke`
 * @line_cap: the `GskLineCap`
 *
 * Sets the line cap to be used when stroking.
 *
 * See [enum@Gsk.LineCap] for details.
 *
 * Since: 4.14
 */
void
gsk_stroke_set_line_cap (GskStroke  *self,
                         GskLineCap  line_cap)
{
  g_return_if_fail (self != NULL);

  self->line_cap = line_cap;
}

/**
 * gsk_stroke_get_line_cap:
 * @self: a `GskStroke`
 *
 * Gets the line cap used.
 *
 * See [enum@Gsk.LineCap] for details.
 *
 * Returns: The line cap
 *
 * Since: 4.14
 */
GskLineCap
gsk_stroke_get_line_cap (const GskStroke *self)
{
  g_return_val_if_fail (self != NULL, 0.0);

  return self->line_cap;
}

/**
 * gsk_stroke_set_line_join:
 * @self: a `GskStroke`
 * @line_join: The line join to use
 *
 * Sets the line join to be used when stroking.
 *
 * See [enum@Gsk.LineJoin] for details.
 *
 * Since: 4.14
 */
void
gsk_stroke_set_line_join (GskStroke   *self,
                          GskLineJoin  line_join)
{
  g_return_if_fail (self != NULL);

  self->line_join = line_join;
}

/**
 * gsk_stroke_get_line_join:
 * @self: a `GskStroke`
 *
 * Gets the line join used.
 *
 * See [enum@Gsk.LineJoin] for details.
 *
 * Returns: The line join
 *
 * Since: 4.14
 */
GskLineJoin
gsk_stroke_get_line_join (const GskStroke *self)
{
  g_return_val_if_fail (self != NULL, 0.0);

  return self->line_join;
}

/**
 * gsk_stroke_set_miter_limit:
 * @self: a `GskStroke`
 * @limit: the miter limit
 *
 * Sets the limit for the distance from the corner where sharp
 * turns of joins get cut off.
 *
 * The miter limit is in units of line width and must be non-negative.
 *
 * For joins of type `GSK_LINE_JOIN_MITER` that exceed the miter
 * limit, the join gets rendered as if it was of type
 * `GSK_LINE_JOIN_BEVEL`.
 *
 * Since: 4.14
 */
void
gsk_stroke_set_miter_limit (GskStroke  *self,
                            float       limit)
{
  g_return_if_fail (self != NULL);
  g_return_if_fail (limit >= 0);

  self->miter_limit = limit;
}

/**
 * gsk_stroke_get_miter_limit:
 * @self: a `GskStroke`
 *
 * Returns the miter limit of a `GskStroke`.
 *
 * Since: 4.14
 */
float
gsk_stroke_get_miter_limit (const GskStroke *self)
{
  g_return_val_if_fail (self != NULL, 4.f);

  return self->miter_limit;
}

/**
 * gsk_stroke_set_dash:
 * @self: a `GskStroke`
 * @dash: (array length=n_dash) (transfer none) (nullable):
 *   the array of dashes
 * @n_dash: number of elements in @dash
 *
 * Sets the dash pattern to use by this stroke.
 *
 * A dash pattern is specified by an array of alternating non-negative
 * values. Each value provides the length of alternate "on" and "off"
 * portions of the stroke.
 *
 * Each "on" segment will have caps applied as if the segment were a
 * separate contour. In particular, it is valid to use an "on" length
 * of 0 with `GSK_LINE_CAP_ROUND` or `GSK_LINE_CAP_SQUARE` to draw dots
 * or squares along a path.
 *
 * If @n_dash is 0, if all elements in @dash are 0, or if there are
 * negative values in @dash, then dashing is disabled.
 *
 * If @n_dash is 1, an alternating "on" and "off" pattern with the
 * single dash length provided is assumed.
 *
 * If @n_dash is uneven, the dash array will be used with the first
 * element in @dash defining an "on" or "off" in alternating passes
 * through the array.
 *
 * You can specify a starting offset into the dash with
 * [method@Gsk.Stroke.set_dash_offset].
 *
 * Since: 4.14
 */
void
gsk_stroke_set_dash (GskStroke   *self,
                     const float *dash,
                     gsize        n_dash)
{
  float dash_length;
  gsize i;

  g_return_if_fail (self != NULL);
  g_return_if_fail (dash != NULL || n_dash == 0);

  dash_length = 0;
  for (i = 0; i < n_dash; i++)
    {
      if (!(dash[i] >= 0)) /* should catch NaN */
        {
          g_critical ("invalid value in dash array at position %zu", i);
          return;
        }
      dash_length += dash[i];
    }

  self->dash_length = dash_length;
  g_free (self->dash);
  self->dash = g_memdup (dash, sizeof (gfloat) * n_dash);
  self->n_dash = n_dash;
}

/**
 * gsk_stroke_get_dash:
 * @self: a `GskStroke`
 * @n_dash: (out): number of elements in the array returned
 *
 * Gets the dash array in use or `NULL` if dashing is disabled.
 *
 * Returns: (array length=n_dash) (transfer none) (nullable):
 *   The dash array or `NULL` if the dash array is empty.
 *
 * Since: 4.14
 */
const float *
gsk_stroke_get_dash (const GskStroke *self,
                     gsize           *n_dash)
{
  g_return_val_if_fail (self != NULL, NULL);
  g_return_val_if_fail (n_dash != NULL, NULL);

  *n_dash = self->n_dash;

  return self->dash;
}

/**
 * gsk_stroke_set_dash_offset:
 * @self: a `GskStroke`
 * @offset: offset into the dash pattern
 *
 * Sets the offset into the dash pattern where dashing should begin.
 *
 * This is an offset into the length of the path, not an index into
 * the array values of the dash array.
 *
 * See [method@Gsk.Stroke.set_dash] for more details on dashing.
 *
 * Since: 4.14
 */
void
gsk_stroke_set_dash_offset (GskStroke *self,
                            float      offset)
{
  g_return_if_fail (self != NULL);

  self->dash_offset = offset;
}

/**
 * gsk_stroke_get_dash_offset:
 * @self: a `GskStroke`
 *
 * Returns the dash_offset of a `GskStroke`.
 *
 * Since: 4.14
 */
float
gsk_stroke_get_dash_offset (const GskStroke *self)
{
  g_return_val_if_fail (self != NULL, 4.f);

  return self->dash_offset;
}

/*< private >
 * gsk_stroke_get_join_width:
 * @stroke: a `GskStroke`
 *
 * Return a width that is sufficient to use
 * when calculating stroke bounds around joins
 * and caps.
 *
 * Returns: the join width
 */
float
gsk_stroke_get_join_width (const GskStroke *stroke)
{
  float width;

  switch (stroke->line_cap)
    {
    case GSK_LINE_CAP_BUTT:
      width = 0;
      break;
    case GSK_LINE_CAP_ROUND:
      width = stroke->line_width;
      break;
    case GSK_LINE_CAP_SQUARE:
      width = G_SQRT2 * stroke->line_width;
      break;
    default:
      g_assert_not_reached ();
    }

  switch (stroke->line_join)
    {
    case GSK_LINE_JOIN_MITER:
      width = MAX (width, MAX (stroke->miter_limit, 1.f) * stroke->line_width);
      break;
    case GSK_LINE_JOIN_ROUND:
    case GSK_LINE_JOIN_BEVEL:
      width = MAX (width, stroke->line_width);
      break;
    default:
      g_assert_not_reached ();
    }

  return width;
}

