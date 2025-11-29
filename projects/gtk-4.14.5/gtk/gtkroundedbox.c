/* GTK - The GIMP Toolkit
 * Copyright (C) 2011 Benjamin Otte <otte@gnome.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include "gtkroundedboxprivate.h"

#include "gtkcsscornervalueprivate.h"
#include "gtkcssnumbervalueprivate.h"
#include "gtkcsstypesprivate.h"

#include "gtkprivate.h"

#include <string.h>

typedef struct {
  double angle1;
  double angle2;
  gboolean negative;
} Arc;

static inline guint
mem_hash (gconstpointer v, int len)
{
  const signed char *p;
  const signed char *end;
  guint32 h = 5381;

  p = v;
  end = p + len;
  for (;  p < end; p++)
    h = (h << 5) + h + *p;

  return h;
}

static guint
arc_path_hash (Arc *arc)
{
  return mem_hash ((gconstpointer)arc, sizeof (Arc));
}

static gboolean
arc_path_equal (Arc *arc1,
                Arc *arc2)
{
  return arc1->angle1 == arc2->angle1 &&
         arc1->angle2 == arc2->angle2 &&
         arc1->negative == arc2->negative;
}

/* We need the path to start with a line-to */
static cairo_path_t *
fixup_path (cairo_path_t *path)
{
  cairo_path_data_t *data;

  data = &path->data[0];
  if (data->header.type == CAIRO_PATH_MOVE_TO)
    data->header.type = CAIRO_PATH_LINE_TO;

  return path;
}

static void
append_arc (cairo_t *cr, double angle1, double angle2, gboolean negative)
{
  static GHashTable *arc_path_cache;
  Arc key;
  cairo_path_t *arc;

  memset (&key, 0, sizeof (Arc));
  key.angle1 = angle1;
  key.angle2 = angle2;
  key.negative = negative;

  if (arc_path_cache == NULL)
    arc_path_cache = g_hash_table_new_full ((GHashFunc)arc_path_hash,
                                            (GEqualFunc)arc_path_equal,
                                            g_free, (GDestroyNotify)cairo_path_destroy);

  arc = g_hash_table_lookup (arc_path_cache, &key);
  if (arc == NULL)
    {
      cairo_surface_t *surface;
      cairo_t *tmp;

      surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, 1, 1);
      tmp = cairo_create (surface);

      if (negative)
        cairo_arc_negative (tmp, 0.0, 0.0, 1.0, angle1, angle2);
      else
        cairo_arc (tmp, 0.0, 0.0, 1.0, angle1, angle2);

      arc = fixup_path (cairo_copy_path (tmp));
      g_hash_table_insert (arc_path_cache, g_memdup2 (&key, sizeof (key)), arc);

      cairo_destroy (tmp);
      cairo_surface_destroy (surface);
    }

  cairo_append_path (cr, arc);
}

static void
_cairo_ellipsis (cairo_t *cr,
	         double xc, double yc,
	         double xradius, double yradius,
	         double angle1, double angle2)
{
  cairo_matrix_t save;

  if (xradius <= 0.0 || yradius <= 0.0)
    {
      cairo_line_to (cr, xc, yc);
      return;
    }

  cairo_get_matrix (cr, &save);
  cairo_translate (cr, xc, yc);
  cairo_scale (cr, xradius, yradius);
  append_arc (cr, angle1, angle2, FALSE);
  cairo_set_matrix (cr, &save);
}

static void
_cairo_ellipsis_negative (cairo_t *cr,
                          double xc, double yc,
                          double xradius, double yradius,
                          double angle1, double angle2)
{
  cairo_matrix_t save;

  if (xradius <= 0.0 || yradius <= 0.0)
    {
      cairo_line_to (cr, xc, yc);
      return;
    }

  cairo_get_matrix (cr, &save);
  cairo_translate (cr, xc, yc);
  cairo_scale (cr, xradius, yradius);
  append_arc (cr, angle1, angle2, TRUE);
  cairo_set_matrix (cr, &save);
}

double
_gtk_rounded_box_guess_length (const GskRoundedRect *box,
                               GtkCssSide            side)
{
  double length;
  GtkCssSide before, after;

  before = side;
  after = (side + 1) % 4;

  if (side & 1)
    length = box->bounds.size.height
             - box->corner[before].height
             - box->corner[after].height;
  else
    length = box->bounds.size.width
             - box->corner[before].width
             - box->corner[after].width;

  length += G_PI * 0.125 * (box->corner[before].width
                            + box->corner[before].height
                            + box->corner[after].width
                            + box->corner[after].height);

  return length;
}

void
_gtk_rounded_box_path_side (const GskRoundedRect *box,
                            cairo_t              *cr,
                            GtkCssSide            side)
{
  switch (side)
    {
    case GTK_CSS_TOP:
      _cairo_ellipsis (cr,
                       box->bounds.origin.x + box->corner[GSK_CORNER_TOP_LEFT].width,
                       box->bounds.origin.y + box->corner[GSK_CORNER_TOP_LEFT].height,
                       box->corner[GSK_CORNER_TOP_LEFT].width,
                       box->corner[GSK_CORNER_TOP_LEFT].height,
                       5 * G_PI_4, 3 * G_PI_2);
      _cairo_ellipsis (cr, 
                       box->bounds.origin.x + box->bounds.size.width - box->corner[GSK_CORNER_TOP_RIGHT].width,
                       box->bounds.origin.y + box->corner[GSK_CORNER_TOP_RIGHT].height,
                       box->corner[GSK_CORNER_TOP_RIGHT].width,
                       box->corner[GSK_CORNER_TOP_RIGHT].height,
                       - G_PI_2, -G_PI_4);
      break;
    case GTK_CSS_RIGHT:
      _cairo_ellipsis (cr, 
                       box->bounds.origin.x + box->bounds.size.width - box->corner[GSK_CORNER_TOP_RIGHT].width,
                       box->bounds.origin.y + box->corner[GSK_CORNER_TOP_RIGHT].height,
                       box->corner[GSK_CORNER_TOP_RIGHT].width,
                       box->corner[GSK_CORNER_TOP_RIGHT].height,
                       - G_PI_4, 0);
      _cairo_ellipsis (cr,
                       box->bounds.origin.x + box->bounds.size.width - box->corner[GSK_CORNER_BOTTOM_RIGHT].width,
                       box->bounds.origin.y + box->bounds.size.height - box->corner[GSK_CORNER_BOTTOM_RIGHT].height,
                       box->corner[GSK_CORNER_BOTTOM_RIGHT].width,
                       box->corner[GSK_CORNER_BOTTOM_RIGHT].height,
                       0, G_PI_4);
      break;
    case GTK_CSS_BOTTOM:
      _cairo_ellipsis (cr,
                       box->bounds.origin.x + box->bounds.size.width - box->corner[GSK_CORNER_BOTTOM_RIGHT].width,
                       box->bounds.origin.y + box->bounds.size.height - box->corner[GSK_CORNER_BOTTOM_RIGHT].height,
                       box->corner[GSK_CORNER_BOTTOM_RIGHT].width,
                       box->corner[GSK_CORNER_BOTTOM_RIGHT].height,
                       G_PI_4, G_PI_2);
      _cairo_ellipsis (cr,
                       box->bounds.origin.x + box->corner[GSK_CORNER_BOTTOM_LEFT].width,
                       box->bounds.origin.y + box->bounds.size.height - box->corner[GSK_CORNER_BOTTOM_LEFT].height,
                       box->corner[GSK_CORNER_BOTTOM_LEFT].width,
                       box->corner[GSK_CORNER_BOTTOM_LEFT].height,
                       G_PI_2, 3 * G_PI_4);
      break;
    case GTK_CSS_LEFT:
      _cairo_ellipsis (cr,
                       box->bounds.origin.x + box->corner[GSK_CORNER_BOTTOM_LEFT].width,
                       box->bounds.origin.y + box->bounds.size.height - box->corner[GSK_CORNER_BOTTOM_LEFT].height,
                       box->corner[GSK_CORNER_BOTTOM_LEFT].width,
                       box->corner[GSK_CORNER_BOTTOM_LEFT].height,
                       3 * G_PI_4, G_PI);
      _cairo_ellipsis (cr,
                       box->bounds.origin.x + box->corner[GSK_CORNER_TOP_LEFT].width,
                       box->bounds.origin.y + box->corner[GSK_CORNER_TOP_LEFT].height,
                       box->corner[GSK_CORNER_TOP_LEFT].width,
                       box->corner[GSK_CORNER_TOP_LEFT].height,
                       G_PI, 5 * G_PI_4);
      break;
    default:
      g_assert_not_reached ();
      break;
    }
}

void
_gtk_rounded_box_path_top (const GskRoundedRect *outer,
                           const GskRoundedRect *inner,
                           cairo_t              *cr)
{
  double start_angle, middle_angle, end_angle;

  if (outer->bounds.origin.y == inner->bounds.origin.y)
    return;

  if (outer->bounds.origin.x == inner->bounds.origin.x)
    start_angle = G_PI;
  else
    start_angle = 5 * G_PI_4;
  middle_angle = 3 * G_PI_2;
  if (outer->bounds.origin.x + outer->bounds.size.width == inner->bounds.origin.x + inner->bounds.size.width)
    end_angle = 0;
  else
    end_angle = 7 * G_PI_4;

  cairo_new_sub_path (cr);

  _cairo_ellipsis (cr,
                   outer->bounds.origin.x + outer->corner[GSK_CORNER_TOP_LEFT].width,
                   outer->bounds.origin.y + outer->corner[GSK_CORNER_TOP_LEFT].height,
                   outer->corner[GSK_CORNER_TOP_LEFT].width,
                   outer->corner[GSK_CORNER_TOP_LEFT].height,
                   start_angle, middle_angle);
  _cairo_ellipsis (cr, 
                   outer->bounds.origin.x + outer->bounds.size.width - outer->corner[GSK_CORNER_TOP_RIGHT].width,
                   outer->bounds.origin.y + outer->corner[GSK_CORNER_TOP_RIGHT].height,
                   outer->corner[GSK_CORNER_TOP_RIGHT].width,
                   outer->corner[GSK_CORNER_TOP_RIGHT].height,
                   middle_angle, end_angle);

  _cairo_ellipsis_negative (cr, 
                            inner->bounds.origin.x + inner->bounds.size.width - inner->corner[GSK_CORNER_TOP_RIGHT].width,
                            inner->bounds.origin.y + inner->corner[GSK_CORNER_TOP_RIGHT].height,
                            inner->corner[GSK_CORNER_TOP_RIGHT].width,
                            inner->corner[GSK_CORNER_TOP_RIGHT].height,
                            end_angle, middle_angle);
  _cairo_ellipsis_negative (cr,
                            inner->bounds.origin.x + inner->corner[GSK_CORNER_TOP_LEFT].width,
                            inner->bounds.origin.y + inner->corner[GSK_CORNER_TOP_LEFT].height,
                            inner->corner[GSK_CORNER_TOP_LEFT].width,
                            inner->corner[GSK_CORNER_TOP_LEFT].height,
                            middle_angle, start_angle);

  cairo_close_path (cr);
}

void
_gtk_rounded_box_path_right (const GskRoundedRect *outer,
                             const GskRoundedRect *inner,
                             cairo_t              *cr)
{
  double start_angle, middle_angle, end_angle;

  if (outer->bounds.origin.x + outer->bounds.size.width == inner->bounds.origin.x + inner->bounds.size.width)
    return;

  if (outer->bounds.origin.y == inner->bounds.origin.y)
    start_angle = 3 * G_PI_2;
  else
    start_angle = 7 * G_PI_4;
  middle_angle = 0;
  if (outer->bounds.origin.y + outer->bounds.size.height == inner->bounds.origin.y + inner->bounds.size.height)
    end_angle = G_PI_2;
  else
    end_angle = G_PI_4;

  cairo_new_sub_path (cr);

  _cairo_ellipsis (cr, 
                   outer->bounds.origin.x + outer->bounds.size.width - outer->corner[GSK_CORNER_TOP_RIGHT].width,
                   outer->bounds.origin.y + outer->corner[GSK_CORNER_TOP_RIGHT].height,
                   outer->corner[GSK_CORNER_TOP_RIGHT].width,
                   outer->corner[GSK_CORNER_TOP_RIGHT].height,
                   start_angle, middle_angle);
  _cairo_ellipsis (cr,
                   outer->bounds.origin.x + outer->bounds.size.width - outer->corner[GSK_CORNER_BOTTOM_RIGHT].width,
                   outer->bounds.origin.y + outer->bounds.size.height - outer->corner[GSK_CORNER_BOTTOM_RIGHT].height,
                   outer->corner[GSK_CORNER_BOTTOM_RIGHT].width,
                   outer->corner[GSK_CORNER_BOTTOM_RIGHT].height,
                   middle_angle, end_angle);

  _cairo_ellipsis_negative (cr,
                            inner->bounds.origin.x + inner->bounds.size.width - inner->corner[GSK_CORNER_BOTTOM_RIGHT].width,
                            inner->bounds.origin.y + inner->bounds.size.height - inner->corner[GSK_CORNER_BOTTOM_RIGHT].height,
                            inner->corner[GSK_CORNER_BOTTOM_RIGHT].width,
                            inner->corner[GSK_CORNER_BOTTOM_RIGHT].height,
                            end_angle, middle_angle);
  _cairo_ellipsis_negative (cr, 
                            inner->bounds.origin.x + inner->bounds.size.width - inner->corner[GSK_CORNER_TOP_RIGHT].width,
                            inner->bounds.origin.y + inner->corner[GSK_CORNER_TOP_RIGHT].height,
                            inner->corner[GSK_CORNER_TOP_RIGHT].width,
                            inner->corner[GSK_CORNER_TOP_RIGHT].height,
                            middle_angle, start_angle);

  cairo_close_path (cr);
}

void
_gtk_rounded_box_path_bottom (const GskRoundedRect *outer,
                              const GskRoundedRect *inner,
                              cairo_t              *cr)
{
  double start_angle, middle_angle, end_angle;

  if (outer->bounds.origin.y + outer->bounds.size.height == inner->bounds.origin.y + inner->bounds.size.height)
    return;

  if (outer->bounds.origin.x + outer->bounds.size.width == inner->bounds.origin.x + inner->bounds.size.width)
    start_angle = 0;
  else
    start_angle = G_PI_4;
  middle_angle = G_PI_2;
  if (outer->bounds.origin.x == inner->bounds.origin.x)
    end_angle = G_PI;
  else
    end_angle = 3 * G_PI_4;

  cairo_new_sub_path (cr);

  _cairo_ellipsis (cr,
                   outer->bounds.origin.x + outer->bounds.size.width - outer->corner[GSK_CORNER_BOTTOM_RIGHT].width,
                   outer->bounds.origin.y + outer->bounds.size.height - outer->corner[GSK_CORNER_BOTTOM_RIGHT].height,
                   outer->corner[GSK_CORNER_BOTTOM_RIGHT].width,
                   outer->corner[GSK_CORNER_BOTTOM_RIGHT].height,
                   start_angle, middle_angle);
  _cairo_ellipsis (cr,
                   outer->bounds.origin.x + outer->corner[GSK_CORNER_BOTTOM_LEFT].width,
                   outer->bounds.origin.y + outer->bounds.size.height - outer->corner[GSK_CORNER_BOTTOM_LEFT].height,
                   outer->corner[GSK_CORNER_BOTTOM_LEFT].width,
                   outer->corner[GSK_CORNER_BOTTOM_LEFT].height,
                   middle_angle, end_angle);

  _cairo_ellipsis_negative (cr,
                            inner->bounds.origin.x + inner->corner[GSK_CORNER_BOTTOM_LEFT].width,
                            inner->bounds.origin.y + inner->bounds.size.height - inner->corner[GSK_CORNER_BOTTOM_LEFT].height,
                            inner->corner[GSK_CORNER_BOTTOM_LEFT].width,
                            inner->corner[GSK_CORNER_BOTTOM_LEFT].height,
                            end_angle, middle_angle);
  _cairo_ellipsis_negative (cr,
                            inner->bounds.origin.x + inner->bounds.size.width - inner->corner[GSK_CORNER_BOTTOM_RIGHT].width,
                            inner->bounds.origin.y + inner->bounds.size.height - inner->corner[GSK_CORNER_BOTTOM_RIGHT].height,
                            inner->corner[GSK_CORNER_BOTTOM_RIGHT].width,
                            inner->corner[GSK_CORNER_BOTTOM_RIGHT].height,
                            middle_angle, start_angle);

  cairo_close_path (cr);
}

void
_gtk_rounded_box_path_left (const GskRoundedRect *outer,
                            const GskRoundedRect *inner,
                            cairo_t              *cr)
{
  double start_angle, middle_angle, end_angle;

  if (outer->bounds.origin.x == inner->bounds.origin.x)
    return;

  if (outer->bounds.origin.y + outer->bounds.size.height == inner->bounds.origin.y + inner->bounds.size.height)
    start_angle = G_PI_2;
  else
    start_angle = 3 * G_PI_4;
  middle_angle = G_PI;
  if (outer->bounds.origin.y == inner->bounds.origin.y)
    end_angle = 3 * G_PI_2;
  else
    end_angle = 5 * G_PI_4;

  cairo_new_sub_path (cr);

  _cairo_ellipsis (cr,
                   outer->bounds.origin.x + outer->corner[GSK_CORNER_BOTTOM_LEFT].width,
                   outer->bounds.origin.y + outer->bounds.size.height - outer->corner[GSK_CORNER_BOTTOM_LEFT].height,
                   outer->corner[GSK_CORNER_BOTTOM_LEFT].width,
                   outer->corner[GSK_CORNER_BOTTOM_LEFT].height,
                   start_angle, middle_angle);
  _cairo_ellipsis (cr,
                   outer->bounds.origin.x + outer->corner[GSK_CORNER_TOP_LEFT].width,
                   outer->bounds.origin.y + outer->corner[GSK_CORNER_TOP_LEFT].height,
                   outer->corner[GSK_CORNER_TOP_LEFT].width,
                   outer->corner[GSK_CORNER_TOP_LEFT].height,
                   middle_angle, end_angle);

  _cairo_ellipsis_negative (cr,
                            inner->bounds.origin.x + inner->corner[GSK_CORNER_TOP_LEFT].width,
                            inner->bounds.origin.y + inner->corner[GSK_CORNER_TOP_LEFT].height,
                            inner->corner[GSK_CORNER_TOP_LEFT].width,
                            inner->corner[GSK_CORNER_TOP_LEFT].height,
                            end_angle, middle_angle);
  _cairo_ellipsis_negative (cr,
                            inner->bounds.origin.x + inner->corner[GSK_CORNER_BOTTOM_LEFT].width,
                            inner->bounds.origin.y + inner->bounds.size.height - inner->corner[GSK_CORNER_BOTTOM_LEFT].height,
                            inner->corner[GSK_CORNER_BOTTOM_LEFT].width,
                            inner->corner[GSK_CORNER_BOTTOM_LEFT].height,
                            middle_angle, start_angle);

  cairo_close_path (cr);
}

