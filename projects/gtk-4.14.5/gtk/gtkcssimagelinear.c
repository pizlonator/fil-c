/*
 * Copyright © 2012 Red Hat Inc.
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

#include "gtkcssimagelinearprivate.h"

#include <math.h>

#include "gtkcsscolorvalueprivate.h"
#include "gtkcssnumbervalueprivate.h"
#include "gtkcsscolorvalueprivate.h"
#include "gtkcssprovider.h"

G_DEFINE_TYPE (GtkCssImageLinear, _gtk_css_image_linear, GTK_TYPE_CSS_IMAGE)

static void
gtk_css_image_linear_get_repeating_start_end (GtkCssImageLinear *linear,
                                              double             length,
                                              double            *start,
                                              double            *end)
{
  const GtkCssImageLinearColorStop *stop;
  double pos;
  guint i;

  g_assert (linear->repeating);

  stop = &linear->color_stops[0];
  if (stop->offset == NULL)
    *start = 0;
  else
    *start = _gtk_css_number_value_get (stop->offset, length) / length;

  *end = *start;

  for (i = 0; i < linear->n_stops; i++)
    {
      stop = &linear->color_stops[i];

      if (stop->offset == NULL)
        continue;

      pos = _gtk_css_number_value_get (stop->offset, length) / length;

      *end = MAX (pos, *end);
    }

  if (stop->offset == NULL)
    *end = MAX (*end, 1.0);
}

static void
gtk_css_image_linear_compute_start_point (double angle_in_degrees,
                                          double width,
                                          double height,
                                          double *out_x,
                                          double *out_y,
                                          double *out_length)
{
  double angle, c, slope, perpendicular;
  double x, y, length;

  angle = fmod (angle_in_degrees, 360);
  if (angle < 0)
    angle += 360;

  if (angle == 0)
    {
      *out_x = 0;
      *out_y = -height;
      *out_length = height;
      return;
    }
  else if (angle == 90)
    {
      *out_x = width;
      *out_y = 0;
      *out_length = width;
      return;
    }
  else if (angle == 180)
    {
      *out_x = 0;
      *out_y = height;
      *out_length = height;
      return;
    }
  else if (angle == 270)
    {
      *out_x = -width;
      *out_y = 0;
      *out_length = width;
      return;
    }

  /* The tan() is confusing because the angle is clockwise
   * from 'to top' */
  perpendicular = tan (angle * G_PI / 180);
  slope = -1 / perpendicular;

  if (angle > 180)
    width = - width;
  if (angle < 90 || angle > 270)
    height = - height;

  /* Compute c (of y = mx + c) of perpendicular */
  c = height - perpendicular * width;

  x = c / (slope - perpendicular);
  y = perpendicular * x + c;

  length = sqrt (x * x + y * y);

  *out_x = x;
  *out_y = y;
  *out_length = length;
}

static void
gtk_css_image_linear_snapshot (GtkCssImage        *image,
                               GtkSnapshot        *snapshot,
                               double              width,
                               double              height)
{
  GtkCssImageLinear *linear = GTK_CSS_IMAGE_LINEAR (image);
  GskColorStop *stops;
  double angle; /* actual angle of the gradient line in degrees */
  double x, y; /* coordinates of start point */
  double length; /* distance in pixels for 100% */
  double start, end; /* position of first/last point on gradient line - with gradient line being [0, 1] */
  double offset;
  int i, last;

  if (linear->side)
    {
      /* special casing the regular cases here so we don't get rounding errors */
      switch (linear->side)
      {
        case 1 << GTK_CSS_RIGHT:
          angle = 90;
          break;
        case 1 << GTK_CSS_LEFT:
          angle = 270;
          break;
        case 1 << GTK_CSS_TOP:
          angle = 0;
          break;
        case 1 << GTK_CSS_BOTTOM:
          angle = 180;
          break;
        default:
          angle = atan2 (linear->side & 1 << GTK_CSS_TOP ? -width : width,
                         linear->side & 1 << GTK_CSS_LEFT ? -height : height);
          angle = 180 * angle / G_PI + 90;
          break;
      }
    }
  else
    {
      angle = _gtk_css_number_value_get (linear->angle, 100);
    }

  gtk_css_image_linear_compute_start_point (angle,
                                            width, height,
                                            &x, &y,
                                            &length);

  if (linear->repeating)
    {
      gtk_css_image_linear_get_repeating_start_end (linear, length, &start, &end);

      if (start == end)
        {
          /* repeating gradients with all color stops sharing the same offset
           * get the color of the last color stop */
          const GtkCssImageLinearColorStop *stop = &linear->color_stops[linear->n_stops - 1];

          gtk_snapshot_append_color (snapshot,
                                     gtk_css_color_value_get_rgba (stop->color),
                                     &GRAPHENE_RECT_INIT (0, 0, width, height));
          return;
        }
    }
  else
    {
      start = 0;
      end = 1;
    }

  offset = start;
  last = -1;
  stops = g_newa (GskColorStop, linear->n_stops);

  for (i = 0; i < linear->n_stops; i++)
    {
      const GtkCssImageLinearColorStop *stop = &linear->color_stops[i];
      double pos, step;

      if (stop->offset == NULL)
        {
          if (i == 0)
            pos = 0.0;
          else if (i + 1 == linear->n_stops)
            pos = 1.0;
          else
            continue;
        }
      else
        {
          pos = _gtk_css_number_value_get (stop->offset, length) / length;
          pos = CLAMP (pos, 0.0, 1.0);
        }

      pos = MAX (pos, offset);
      step = (pos - offset) / (i - last);
      for (last = last + 1; last <= i; last++)
        {
          stop = &linear->color_stops[last];

          offset += step;

          stops[last].offset = (offset - start) / (end - start);
          stops[last].color = *gtk_css_color_value_get_rgba (stop->color);
        }

      offset = pos;
      last = i;
    }

  if (linear->repeating)
    {
      gtk_snapshot_append_repeating_linear_gradient (
          snapshot,
          &GRAPHENE_RECT_INIT (0, 0, width, height),
          &GRAPHENE_POINT_INIT (width / 2 + x * (start - 0.5), height / 2 + y * (start - 0.5)),
          &GRAPHENE_POINT_INIT (width / 2 + x * (end - 0.5),   height / 2 + y * (end - 0.5)),
          stops,
          linear->n_stops);
    }
  else
    {
      gtk_snapshot_append_linear_gradient (
          snapshot,
          &GRAPHENE_RECT_INIT (0, 0, width, height),
          &GRAPHENE_POINT_INIT (width / 2 + x * (start - 0.5), height / 2 + y * (start - 0.5)),
          &GRAPHENE_POINT_INIT (width / 2 + x * (end - 0.5),   height / 2 + y * (end - 0.5)),
          stops,
          linear->n_stops);
    }
}

static guint
gtk_css_image_linear_parse_color_stop (GtkCssImageLinear *self,
                                       GtkCssParser      *parser,
                                       GArray            *stop_array)
{
  GtkCssImageLinearColorStop stop;

  stop.color = _gtk_css_color_value_parse (parser);
  if (stop.color == NULL)
    return 0;

  if (gtk_css_number_value_can_parse (parser))
    {
      stop.offset = _gtk_css_number_value_parse (parser,
                                                 GTK_CSS_PARSE_PERCENT
                                                 | GTK_CSS_PARSE_LENGTH);
      if (stop.offset == NULL)
        {
          _gtk_css_value_unref (stop.color);
          return 0;
        }
    }
  else
    {
      stop.offset = NULL;
    }

  g_array_append_val (stop_array, stop);

  return 1;
}

static guint
gtk_css_image_linear_parse_first_arg (GtkCssImageLinear *linear,
                                      GtkCssParser      *parser,
                                      GArray            *stop_array)
{
  guint i;

  if (gtk_css_parser_try_ident (parser, "to"))
    {
      for (i = 0; i < 2; i++)
        {
          if (gtk_css_parser_try_ident (parser, "left"))
            {
              if (linear->side & ((1 << GTK_CSS_LEFT) | (1 << GTK_CSS_RIGHT)))
                {
                  gtk_css_parser_error_syntax (parser, "Expected 'top', 'bottom' or comma");
                  return 0;
                }
              linear->side |= (1 << GTK_CSS_LEFT);
            }
          else if (gtk_css_parser_try_ident (parser, "right"))
            {
              if (linear->side & ((1 << GTK_CSS_LEFT) | (1 << GTK_CSS_RIGHT)))
                {
                  gtk_css_parser_error_syntax (parser, "Expected 'top', 'bottom' or comma");
                  return 0;
                }
              linear->side |= (1 << GTK_CSS_RIGHT);
            }
          else if (gtk_css_parser_try_ident (parser, "top"))
            {
              if (linear->side & ((1 << GTK_CSS_TOP) | (1 << GTK_CSS_BOTTOM)))
                {
                  gtk_css_parser_error_syntax (parser, "Expected 'left', 'right' or comma");
                  return 0;
                }
              linear->side |= (1 << GTK_CSS_TOP);
            }
          else if (gtk_css_parser_try_ident (parser, "bottom"))
            {
              if (linear->side & ((1 << GTK_CSS_TOP) | (1 << GTK_CSS_BOTTOM)))
                {
                  gtk_css_parser_error_syntax (parser, "Expected 'left', 'right' or comma");
                  return 0;
                }
              linear->side |= (1 << GTK_CSS_BOTTOM);
            }
          else
            break;
        }

      if (linear->side == 0)
        {
          gtk_css_parser_error_syntax (parser, "Expected side that gradient should go to");
          return 0;
        }

      return 1;
    }
  else if (gtk_css_number_value_can_parse (parser))
    {
      linear->angle = _gtk_css_number_value_parse (parser, GTK_CSS_PARSE_ANGLE);
      if (linear->angle == NULL)
        return 0;

      return 1;
    }
  else
    {
      linear->side = 1 << GTK_CSS_BOTTOM;
      if (!gtk_css_image_linear_parse_color_stop (linear, parser, stop_array))
        return 0;

      return 2;
    }
}

typedef struct
{
  GtkCssImageLinear *self;
  GArray *stop_array;
} ParseData;

static guint
gtk_css_image_linear_parse_arg (GtkCssParser *parser,
                                guint         arg,
                                gpointer      user_data)
{
  ParseData *parse_data = user_data;
  GtkCssImageLinear *self = parse_data->self;

  if (arg == 0)
    return gtk_css_image_linear_parse_first_arg (self, parser, parse_data->stop_array);
  else
    return gtk_css_image_linear_parse_color_stop (self, parser, parse_data->stop_array);
}

static gboolean
gtk_css_image_linear_parse (GtkCssImage  *image,
                            GtkCssParser *parser)
{
  GtkCssImageLinear *self = GTK_CSS_IMAGE_LINEAR (image);
  ParseData parse_data;
  gboolean success;

  if (gtk_css_parser_has_function (parser, "repeating-linear-gradient"))
    self->repeating = TRUE;
  else if (gtk_css_parser_has_function (parser, "linear-gradient"))
    self->repeating = FALSE;
  else
    {
      gtk_css_parser_error_syntax (parser, "Not a linear gradient");
      return FALSE;
    }

  parse_data.self = self;
  parse_data.stop_array = g_array_new (TRUE, FALSE, sizeof (GtkCssImageLinearColorStop));

  success = gtk_css_parser_consume_function (parser, 3, G_MAXUINT, gtk_css_image_linear_parse_arg, &parse_data);

  if (!success)
    {
      g_array_free (parse_data.stop_array, TRUE);
    }
  else
    {
      self->n_stops = parse_data.stop_array->len;
      self->color_stops = (GtkCssImageLinearColorStop *)g_array_free (parse_data.stop_array, FALSE);
    }

  return success;
}

static void
gtk_css_image_linear_print (GtkCssImage *image,
                            GString     *string)
{
  GtkCssImageLinear *linear = GTK_CSS_IMAGE_LINEAR (image);
  guint i;

  if (linear->repeating)
    g_string_append (string, "repeating-linear-gradient(");
  else
    g_string_append (string, "linear-gradient(");

  if (linear->side)
    {
      if (linear->side != (1 << GTK_CSS_BOTTOM))
        {
          g_string_append (string, "to");

          if (linear->side & (1 << GTK_CSS_TOP))
            g_string_append (string, " top");
          else if (linear->side & (1 << GTK_CSS_BOTTOM))
            g_string_append (string, " bottom");

          if (linear->side & (1 << GTK_CSS_LEFT))
            g_string_append (string, " left");
          else if (linear->side & (1 << GTK_CSS_RIGHT))
            g_string_append (string, " right");

          g_string_append (string, ", ");
        }
    }
  else
    {
      _gtk_css_value_print (linear->angle, string);
      g_string_append (string, ", ");
    }

  for (i = 0; i < linear->n_stops; i++)
    {
      const GtkCssImageLinearColorStop *stop = &linear->color_stops[i];

      if (i > 0)
        g_string_append (string, ", ");

      _gtk_css_value_print (stop->color, string);

      if (stop->offset)
        {
          g_string_append (string, " ");
          _gtk_css_value_print (stop->offset, string);
        }
    }

  g_string_append (string, ")");
}

static GtkCssImage *
gtk_css_image_linear_compute (GtkCssImage      *image,
                              guint             property_id,
                              GtkStyleProvider *provider,
                              GtkCssStyle      *style,
                              GtkCssStyle      *parent_style)
{
  GtkCssImageLinear *linear = GTK_CSS_IMAGE_LINEAR (image);
  GtkCssImageLinear *copy;
  guint i;

  copy = g_object_new (GTK_TYPE_CSS_IMAGE_LINEAR, NULL);
  copy->repeating = linear->repeating;
  copy->side = linear->side;

  if (linear->angle)
    copy->angle = _gtk_css_value_compute (linear->angle, property_id, provider, style, parent_style);

  copy->n_stops = linear->n_stops;
  copy->color_stops = g_malloc (sizeof (GtkCssImageLinearColorStop) * copy->n_stops);
  for (i = 0; i < linear->n_stops; i++)
    {
      const GtkCssImageLinearColorStop *stop = &linear->color_stops[i];
      GtkCssImageLinearColorStop *scopy = &copy->color_stops[i];

      scopy->color = _gtk_css_value_compute (stop->color, property_id, provider, style, parent_style);

      if (stop->offset)
        {
          scopy->offset = _gtk_css_value_compute (stop->offset, property_id, provider, style, parent_style);
        }
      else
        {
          scopy->offset = NULL;
        }
    }

  return GTK_CSS_IMAGE (copy);
}

static GtkCssImage *
gtk_css_image_linear_transition (GtkCssImage *start_image,
                                 GtkCssImage *end_image,
                                 guint        property_id,
                                 double       progress)
{
  GtkCssImageLinear *start, *end, *result;
  guint i;

  start = GTK_CSS_IMAGE_LINEAR (start_image);

  if (end_image == NULL)
    return GTK_CSS_IMAGE_CLASS (_gtk_css_image_linear_parent_class)->transition (start_image, end_image, property_id, progress);

  if (!GTK_IS_CSS_IMAGE_LINEAR (end_image))
    return GTK_CSS_IMAGE_CLASS (_gtk_css_image_linear_parent_class)->transition (start_image, end_image, property_id, progress);

  end = GTK_CSS_IMAGE_LINEAR (end_image);

  if ((start->repeating != end->repeating)
      || (start->n_stops != end->n_stops))
    return GTK_CSS_IMAGE_CLASS (_gtk_css_image_linear_parent_class)->transition (start_image, end_image, property_id, progress);

  result = g_object_new (GTK_TYPE_CSS_IMAGE_LINEAR, NULL);
  result->repeating = start->repeating;

  if (start->side != end->side)
    goto fail;

  result->side = start->side;
  if (result->side == 0)
    result->angle = _gtk_css_value_transition (start->angle, end->angle, property_id, progress);
  if (result->angle == NULL)
    goto fail;

  /* Maximum amountof stops */
  result->color_stops = g_malloc (sizeof (GtkCssImageLinearColorStop) * start->n_stops);
  result->n_stops = 0;
  for (i = 0; i < start->n_stops; i++)
    {
      const GtkCssImageLinearColorStop *start_stop = &start->color_stops[i];
      const GtkCssImageLinearColorStop *end_stop = &end->color_stops[i];
      GtkCssImageLinearColorStop *stop = &result->color_stops[i];

      if ((start_stop->offset != NULL) != (end_stop->offset != NULL))
        goto fail;

      if (start_stop->offset == NULL)
        {
          stop->offset = NULL;
        }
      else
        {
          stop->offset = _gtk_css_value_transition (start_stop->offset,
                                                    end_stop->offset,
                                                    property_id,
                                                    progress);
          if (stop->offset == NULL)
            goto fail;
        }

      stop->color = _gtk_css_value_transition (start_stop->color,
                                               end_stop->color,
                                               property_id,
                                               progress);
      if (stop->color == NULL)
        {
          if (stop->offset)
            _gtk_css_value_unref (stop->offset);
          goto fail;
        }

      result->n_stops ++;
    }

  return GTK_CSS_IMAGE (result);

fail:
  g_object_unref (result);
  return GTK_CSS_IMAGE_CLASS (_gtk_css_image_linear_parent_class)->transition (start_image, end_image, property_id, progress);
}

static gboolean
gtk_css_image_linear_equal (GtkCssImage *image1,
                            GtkCssImage *image2)
{
  GtkCssImageLinear *linear1 = (GtkCssImageLinear *) image1;
  GtkCssImageLinear *linear2 = (GtkCssImageLinear *) image2;
  guint i;

  if (linear1->repeating != linear2->repeating ||
      linear1->side != linear2->side ||
      (linear1->side == 0 && !_gtk_css_value_equal (linear1->angle, linear2->angle)) ||
      linear1->n_stops != linear2->n_stops)
    return FALSE;

  for (i = 0; i < linear1->n_stops; i++)
    {
      const GtkCssImageLinearColorStop *stop1 = &linear1->color_stops[i];
      const GtkCssImageLinearColorStop *stop2 = &linear2->color_stops[i];

      if (!_gtk_css_value_equal0 (stop1->offset, stop2->offset) ||
          !_gtk_css_value_equal (stop1->color, stop2->color))
        return FALSE;
    }

  return TRUE;
}

static void
gtk_css_image_linear_dispose (GObject *object)
{
  GtkCssImageLinear *linear = GTK_CSS_IMAGE_LINEAR (object);
  guint i;

  for (i = 0; i < linear->n_stops; i ++)
    {
      GtkCssImageLinearColorStop *stop = &linear->color_stops[i];

      _gtk_css_value_unref (stop->color);
      if (stop->offset)
        _gtk_css_value_unref (stop->offset);
    }
  g_free (linear->color_stops);

  linear->side = 0;
  if (linear->angle)
    {
      _gtk_css_value_unref (linear->angle);
      linear->angle = NULL;
    }

  G_OBJECT_CLASS (_gtk_css_image_linear_parent_class)->dispose (object);
}

static gboolean
gtk_css_image_linear_is_computed (GtkCssImage *image)
{
  GtkCssImageLinear *linear = GTK_CSS_IMAGE_LINEAR (image);
  guint i;
  gboolean computed = TRUE;

  computed = !linear->angle || gtk_css_value_is_computed (linear->angle);

  for (i = 0; i < linear->n_stops; i ++)
    {
      const GtkCssImageLinearColorStop *stop = &linear->color_stops[i];

      if (stop->offset && !gtk_css_value_is_computed (stop->offset))
        {
          computed = FALSE;
          break;
        }

      if (!gtk_css_value_is_computed (stop->color))
        {
          computed = FALSE;
          break;
        }
    }

  return computed;
}

static void
_gtk_css_image_linear_class_init (GtkCssImageLinearClass *klass)
{
  GtkCssImageClass *image_class = GTK_CSS_IMAGE_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  image_class->snapshot = gtk_css_image_linear_snapshot;
  image_class->parse = gtk_css_image_linear_parse;
  image_class->print = gtk_css_image_linear_print;
  image_class->compute = gtk_css_image_linear_compute;
  image_class->equal = gtk_css_image_linear_equal;
  image_class->transition = gtk_css_image_linear_transition;
  image_class->is_computed = gtk_css_image_linear_is_computed;

  object_class->dispose = gtk_css_image_linear_dispose;
}

static void
_gtk_css_image_linear_init (GtkCssImageLinear *linear)
{
}

