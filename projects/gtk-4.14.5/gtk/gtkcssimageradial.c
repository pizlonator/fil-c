/*
 * Copyright © 2015 Red Hat Inc.
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

#include "gtkcssimageradialprivate.h"

#include <math.h>

#include "gtkcsscolorvalueprivate.h"
#include "gtkcssnumbervalueprivate.h"
#include "gtkcsspositionvalueprivate.h"
#include "gtkcsscolorvalueprivate.h"
#include "gtkcssprovider.h"

G_DEFINE_TYPE (GtkCssImageRadial, _gtk_css_image_radial, GTK_TYPE_CSS_IMAGE)

static void
gtk_css_image_radial_get_start_end (GtkCssImageRadial *radial,
                                    double             radius,
                                    double            *start,
                                    double            *end)
{
  const GtkCssImageRadialColorStop *stop;
  double pos;
  guint i;

  if (radial->repeating)
    {
      stop = &radial->color_stops[0];
      if (stop->offset == NULL)
        *start = 0;
      else
        *start = _gtk_css_number_value_get (stop->offset, radius) / radius;

      *end = *start;

      for (i = 0; i < radial->n_stops; i++)
        {
          stop = &radial->color_stops[i];

          if (stop->offset == NULL)
            continue;

          pos = _gtk_css_number_value_get (stop->offset, radius) / radius;

          *end = MAX (pos, *end);
        }

      if (stop->offset == NULL)
        *end = MAX (*end, 1.0);
    }
  else
    {
      *start = 0;
      *end = 1;
    }
}

static void
gtk_css_image_radial_snapshot (GtkCssImage *image,
                               GtkSnapshot *snapshot,
                               double       width,
                               double       height)
{
  GtkCssImageRadial *radial = GTK_CSS_IMAGE_RADIAL (image);
  GskColorStop *stops;
  double x, y;
  double hradius, vradius;
  double start, end;
  double r1, r2, r3, r4, r;
  double offset;
  int i, last;

  x = _gtk_css_position_value_get_x (radial->position, width);
  y = _gtk_css_position_value_get_y (radial->position, height);

  if (radial->circle)
    {
      switch (radial->size)
        {
        case GTK_CSS_EXPLICIT_SIZE:
          hradius = _gtk_css_number_value_get (radial->sizes[0], width);
          break;
        case GTK_CSS_CLOSEST_SIDE:
          hradius = MIN (MIN (x, width - x), MIN (y, height - y));
          break;
        case GTK_CSS_FARTHEST_SIDE:
          hradius = MAX (MAX (x, width - x), MAX (y, height - y));
          break;
        case GTK_CSS_CLOSEST_CORNER:
        case GTK_CSS_FARTHEST_CORNER:
          r1 = x*x + y*y;
          r2 = x*x + (height - y)*(height - y);
          r3 = (width - x)*(width - x) + y*y;
          r4 = (width - x)*(width - x) + (height - y)*(height - y);
          if (radial->size == GTK_CSS_CLOSEST_CORNER)
            r = MIN ( MIN (r1, r2), MIN (r3, r4));
          else
            r = MAX ( MAX (r1, r2), MAX (r3, r4));
          hradius = sqrt (r);
          break;
        default:
          g_assert_not_reached ();
        }

      hradius = MAX (1.0, hradius);
      vradius = hradius;
    }
  else
    {
      switch (radial->size)
        {
        case GTK_CSS_EXPLICIT_SIZE:
          hradius = _gtk_css_number_value_get (radial->sizes[0], width);
          vradius = _gtk_css_number_value_get (radial->sizes[1], height);
          break;
        case GTK_CSS_CLOSEST_SIDE:
          hradius = MIN (x, width - x);
          vradius = MIN (y, height - y);
          break;
        case GTK_CSS_FARTHEST_SIDE:
          hradius = MAX (x, width - x);
          vradius = MAX (y, height - y);
          break;
        case GTK_CSS_CLOSEST_CORNER:
          hradius = M_SQRT2 * MIN (x, width - x);
          vradius = M_SQRT2 * MIN (y, height - y);
          break;
        case GTK_CSS_FARTHEST_CORNER:
          hradius = M_SQRT2 * MAX (x, width - x);
          vradius = M_SQRT2 * MAX (y, height - y);
          break;
        default:
          g_assert_not_reached ();
        }

      hradius = MAX (1.0, hradius);
      vradius = MAX (1.0, vradius);
    }

  gtk_css_image_radial_get_start_end (radial, hradius, &start, &end);

  offset = start;
  last = -1;
  stops = g_newa (GskColorStop, radial->n_stops);

  for (i = 0; i < radial->n_stops; i++)
    {
      const GtkCssImageRadialColorStop *stop = &radial->color_stops[i];
      double pos, step;

      if (stop->offset == NULL)
        {
          if (i == 0)
            pos = 0.0;
          else if (i + 1 == radial->n_stops)
            pos = 1.0;
          else
            continue;
        }
      else
        pos = MIN (1.0, _gtk_css_number_value_get (stop->offset, hradius) / hradius);

      pos = MAX (pos, offset);
      step = (pos - offset) / (i - last);
      for (last = last + 1; last <= i; last++)
        {
          stop = &radial->color_stops[last];

          offset += step;

          stops[last].offset = (offset - start) / (end - start);
          stops[last].color = *gtk_css_color_value_get_rgba (stop->color);
        }

      offset = pos;
      last = i;
    }

 if (radial->repeating)
   gtk_snapshot_append_repeating_radial_gradient (snapshot,
                                                  &GRAPHENE_RECT_INIT (0, 0, width, height),
                                                  &GRAPHENE_POINT_INIT (x, y),
                                                  hradius,
                                                  vradius,
                                                  start,
                                                  end,
                                                  stops,
                                                  radial->n_stops);
  else
    gtk_snapshot_append_radial_gradient (snapshot,
                                         &GRAPHENE_RECT_INIT (0, 0, width, height),
                                         &GRAPHENE_POINT_INIT (x, y),
                                         hradius,
                                         vradius,
                                         start,
                                         end,
                                         stops,
                                         radial->n_stops);
}

static guint
gtk_css_image_radial_parse_color_stop (GtkCssImageRadial *radial,
                                       GtkCssParser      *parser,
                                       GArray            *stop_array)
{
  GtkCssImageRadialColorStop stop;

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
gtk_css_image_radial_parse_first_arg (GtkCssImageRadial *radial,
                                      GtkCssParser      *parser,
                                      GArray            *stop_array)
{
  gboolean has_shape = FALSE;
  gboolean has_size = FALSE;
  gboolean found_one = FALSE;
  guint i;
  static struct {
    const char *name;
    guint       value;
  } names[] = {
    { "closest-side", GTK_CSS_CLOSEST_SIDE },
    { "farthest-side", GTK_CSS_FARTHEST_SIDE },
    { "closest-corner", GTK_CSS_CLOSEST_CORNER },
    { "farthest-corner", GTK_CSS_FARTHEST_CORNER }
  };

  found_one = FALSE;

  do {
    if (!has_shape && gtk_css_parser_try_ident (parser, "circle"))
      {
        radial->circle = TRUE;
        found_one = has_shape = TRUE;
      }
    else if (!has_shape && gtk_css_parser_try_ident (parser, "ellipse"))
      {
        radial->circle = FALSE;
        found_one = has_shape = TRUE;
      }
    else if (!has_size)
      {
        for (i = 0; i < G_N_ELEMENTS (names); i++)
          {
            if (gtk_css_parser_try_ident (parser, names[i].name))
              {
                found_one = has_size = TRUE;
                radial->size = names[i].value;
                break;
              }
          }

        if (!has_size && gtk_css_number_value_can_parse (parser))
          {
            radial->sizes[0] = _gtk_css_number_value_parse (parser, GTK_CSS_PARSE_LENGTH | GTK_CSS_PARSE_PERCENT);
            if (radial->sizes[0] == NULL)
              return 0;
            if (gtk_css_number_value_can_parse (parser))
              {
                radial->sizes[1] = _gtk_css_number_value_parse (parser, GTK_CSS_PARSE_LENGTH | GTK_CSS_PARSE_PERCENT);
                if (radial->sizes[1] == NULL)
                  return 0;
              }
            found_one = has_size = TRUE;
          }
        if (!has_size)
          break;
      }
    else
      {
        break;
      }
  } while (!(has_shape && has_size));

  if (gtk_css_parser_try_ident (parser, "at"))
    {
      radial->position = _gtk_css_position_value_parse (parser);
      if (!radial->position)
        return 0;
      found_one = TRUE;
    }
  else
    {
      radial->position = _gtk_css_position_value_new (_gtk_css_number_value_new (50, GTK_CSS_PERCENT),
                                                      _gtk_css_number_value_new (50, GTK_CSS_PERCENT));
    }

  if (!has_size)
    {
      radial->size = GTK_CSS_FARTHEST_CORNER;
    }

  if (!has_shape)
    {
      if (radial->sizes[0] && !radial->sizes[1])
        radial->circle = TRUE;
      else
        radial->circle = FALSE;
    }

  if (has_shape && radial->circle)
    {
      if (radial->sizes[0] && radial->sizes[1])
        {
          gtk_css_parser_error_syntax (parser, "Circular gradient can only have one size");
          return 0;
        }

      if (radial->sizes[0] && gtk_css_number_value_has_percent (radial->sizes[0]))
        {
          gtk_css_parser_error_syntax (parser, "Circular gradient cannot have percentage as size");
          return 0;
        }
    }

  if (has_size && !radial->circle)
    {
      if (radial->sizes[0] && !radial->sizes[1])
        radial->sizes[1] = _gtk_css_value_ref (radial->sizes[0]);
    }

  if (found_one)
    return 1;

  if (!gtk_css_image_radial_parse_color_stop (radial, parser, stop_array))
    return 0;

  return 2;
}

typedef struct
{
  GtkCssImageRadial *self;
  GArray *stop_array;
} ParseData;

static guint
gtk_css_image_radial_parse_arg (GtkCssParser *parser,
                                guint         arg,
                                gpointer      user_data)
{
  ParseData *parse_data = user_data;
  GtkCssImageRadial *self = parse_data->self;

  if (arg == 0)
    return gtk_css_image_radial_parse_first_arg (self, parser, parse_data->stop_array);
  else
    return gtk_css_image_radial_parse_color_stop (self, parser, parse_data->stop_array);

}

static gboolean
gtk_css_image_radial_parse (GtkCssImage  *image,
                            GtkCssParser *parser)
{
  GtkCssImageRadial *self = GTK_CSS_IMAGE_RADIAL (image);
  ParseData parse_data;
  gboolean success;

  if (gtk_css_parser_has_function (parser, "repeating-radial-gradient"))
    self->repeating = TRUE;
  else if (gtk_css_parser_has_function (parser, "radial-gradient"))
    self->repeating = FALSE;
  else
    {
      gtk_css_parser_error_syntax (parser, "Not a radial gradient");
      return FALSE;
    }

  parse_data.self = self;
  parse_data.stop_array = g_array_new (TRUE, FALSE, sizeof (GtkCssImageRadialColorStop));

  success = gtk_css_parser_consume_function (parser, 3, G_MAXUINT, gtk_css_image_radial_parse_arg, &parse_data);

  if (!success)
    {
      g_array_free (parse_data.stop_array, TRUE);
    }
  else
    {
      self->n_stops = parse_data.stop_array->len;
      self->color_stops = (GtkCssImageRadialColorStop *)g_array_free (parse_data.stop_array, FALSE);
    }

  return success;
}

static void
gtk_css_image_radial_print (GtkCssImage *image,
                            GString     *string)
{
  GtkCssImageRadial *radial = GTK_CSS_IMAGE_RADIAL (image);
  guint i;
  const char *names[] = {
    NULL,
    "closest-side",
    "farthest-side",
    "closest-corner",
    "farthest-corner"
  };

  if (radial->repeating)
    g_string_append (string, "repeating-radial-gradient(");
  else
    g_string_append (string, "radial-gradient(");

  if (radial->circle)
    g_string_append (string, "circle ");
  else
    g_string_append (string, "ellipse ");

  if (radial->size != 0)
    g_string_append (string, names[radial->size]);
  else
    {
      if (radial->sizes[0])
        _gtk_css_value_print (radial->sizes[0], string);
      if (radial->sizes[1])
        {
          g_string_append (string, " ");
          _gtk_css_value_print (radial->sizes[1], string);
        }
    }

  g_string_append (string, " at ");
  _gtk_css_value_print (radial->position, string);

  g_string_append (string, ", ");

  for (i = 0; i < radial->n_stops; i++)
    {
      const GtkCssImageRadialColorStop *stop = &radial->color_stops[i];

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
gtk_css_image_radial_compute (GtkCssImage      *image,
                              guint             property_id,
                              GtkStyleProvider *provider,
                              GtkCssStyle      *style,
                              GtkCssStyle      *parent_style)
{
  GtkCssImageRadial *radial = GTK_CSS_IMAGE_RADIAL (image);
  GtkCssImageRadial *copy;
  guint i;

  copy = g_object_new (GTK_TYPE_CSS_IMAGE_RADIAL, NULL);
  copy->repeating = radial->repeating;
  copy->circle = radial->circle;
  copy->size = radial->size;

  copy->position = _gtk_css_value_compute (radial->position, property_id, provider, style, parent_style);

  if (radial->sizes[0])
    copy->sizes[0] = _gtk_css_value_compute (radial->sizes[0], property_id, provider, style, parent_style);

  if (radial->sizes[1])
    copy->sizes[1] = _gtk_css_value_compute (radial->sizes[1], property_id, provider, style, parent_style);

  copy->n_stops = radial->n_stops;
  copy->color_stops = g_malloc (sizeof (GtkCssImageRadialColorStop) * copy->n_stops);
  for (i = 0; i < radial->n_stops; i++)
    {
      const GtkCssImageRadialColorStop *stop = &radial->color_stops[i];
      GtkCssImageRadialColorStop *scopy = &copy->color_stops[i];

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
gtk_css_image_radial_transition (GtkCssImage *start_image,
                                 GtkCssImage *end_image,
                                 guint        property_id,
                                 double       progress)
{
  GtkCssImageRadial *start, *end, *result;
  guint i;

  start = GTK_CSS_IMAGE_RADIAL (start_image);

  if (end_image == NULL)
    return GTK_CSS_IMAGE_CLASS (_gtk_css_image_radial_parent_class)->transition (start_image, end_image, property_id, progress);

  if (!GTK_IS_CSS_IMAGE_RADIAL (end_image))
    return GTK_CSS_IMAGE_CLASS (_gtk_css_image_radial_parent_class)->transition (start_image, end_image, property_id, progress);

  end = GTK_CSS_IMAGE_RADIAL (end_image);

  if (start->repeating != end->repeating ||
      start->n_stops != end->n_stops ||
      start->size != end->size ||
      start->circle != end->circle)
    return GTK_CSS_IMAGE_CLASS (_gtk_css_image_radial_parent_class)->transition (start_image, end_image, property_id, progress);

  result = g_object_new (GTK_TYPE_CSS_IMAGE_RADIAL, NULL);
  result->repeating = start->repeating;
  result->circle = start->circle;
  result->size = start->size;

  result->position = _gtk_css_value_transition (start->position, end->position, property_id, progress);
  if (result->position == NULL)
    goto fail;

  if (start->sizes[0] && end->sizes[0])
    {
      result->sizes[0] = _gtk_css_value_transition (start->sizes[0], end->sizes[0], property_id, progress);
      if (result->sizes[0] == NULL)
        goto fail;
    }
  else
    result->sizes[0] = 0;

  if (start->sizes[1] && end->sizes[1])
    {
      result->sizes[1] = _gtk_css_value_transition (start->sizes[1], end->sizes[1], property_id, progress);
      if (result->sizes[1] == NULL)
        goto fail;
    }
  else
    result->sizes[1] = 0;

  result->color_stops = g_malloc (sizeof (GtkCssImageRadialColorStop) * start->n_stops);
  result->n_stops = 0;
  for (i = 0; i < start->n_stops; i++)
    {
      const GtkCssImageRadialColorStop *start_stop = &start->color_stops[i];
      const GtkCssImageRadialColorStop *end_stop = &end->color_stops[i];
      GtkCssImageRadialColorStop *stop = &result->color_stops[i];

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

      result->n_stops++;
    }

  return GTK_CSS_IMAGE (result);

fail:
  g_object_unref (result);
  return GTK_CSS_IMAGE_CLASS (_gtk_css_image_radial_parent_class)->transition (start_image, end_image, property_id, progress);
}

static gboolean
gtk_css_image_radial_equal (GtkCssImage *image1,
                            GtkCssImage *image2)
{
  GtkCssImageRadial *radial1 = (GtkCssImageRadial *) image1;
  GtkCssImageRadial *radial2 = (GtkCssImageRadial *) image2;
  guint i;

  if (radial1->repeating != radial2->repeating ||
      radial1->size != radial2->size ||
      !_gtk_css_value_equal (radial1->position, radial2->position) ||
      ((radial1->sizes[0] == NULL) != (radial2->sizes[0] == NULL)) ||
      (radial1->sizes[0] && radial2->sizes[0] && !_gtk_css_value_equal (radial1->sizes[0], radial2->sizes[0])) ||
      ((radial1->sizes[1] == NULL) != (radial2->sizes[1] == NULL)) ||
      (radial1->sizes[1] && radial2->sizes[1] && !_gtk_css_value_equal (radial1->sizes[1], radial2->sizes[1])) ||
      radial1->n_stops != radial2->n_stops)
    return FALSE;

  for (i = 0; i < radial1->n_stops; i++)
    {
      const GtkCssImageRadialColorStop *stop1 = &radial1->color_stops[i];
      const GtkCssImageRadialColorStop *stop2 = &radial2->color_stops[i];

      if (!_gtk_css_value_equal0 (stop1->offset, stop2->offset) ||
          !_gtk_css_value_equal (stop1->color, stop2->color))
        return FALSE;
    }

  return TRUE;
}

static void
gtk_css_image_radial_dispose (GObject *object)
{
  GtkCssImageRadial *radial = GTK_CSS_IMAGE_RADIAL (object);
  guint i;

  for (i = 0; i < radial->n_stops; i ++)
    {
      GtkCssImageRadialColorStop *stop = &radial->color_stops[i];

      _gtk_css_value_unref (stop->color);
      if (stop->offset)
        _gtk_css_value_unref (stop->offset);
    }
  g_free (radial->color_stops);

  if (radial->position)
    {
      _gtk_css_value_unref (radial->position);
      radial->position = NULL;
    }

  for (i = 0; i < 2; i++)
    if (radial->sizes[i])
      {
        _gtk_css_value_unref (radial->sizes[i]);
        radial->sizes[i] = NULL;
      }

  G_OBJECT_CLASS (_gtk_css_image_radial_parent_class)->dispose (object);
}

static gboolean
gtk_css_image_radial_is_computed (GtkCssImage *image)
{
  GtkCssImageRadial *radial = GTK_CSS_IMAGE_RADIAL (image);
  guint i;
  gboolean computed = TRUE;

  computed = computed && (!radial->position || gtk_css_value_is_computed (radial->position));
  computed = computed && (!radial->sizes[0] || gtk_css_value_is_computed (radial->sizes[0]));
  computed = computed && (!radial->sizes[1] || gtk_css_value_is_computed (radial->sizes[1]));

  if (computed)
    for (i = 0; i < radial->n_stops; i ++)
      {
        const GtkCssImageRadialColorStop *stop = &radial->color_stops[i];

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
_gtk_css_image_radial_class_init (GtkCssImageRadialClass *klass)
{
  GtkCssImageClass *image_class = GTK_CSS_IMAGE_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  image_class->snapshot = gtk_css_image_radial_snapshot;
  image_class->parse = gtk_css_image_radial_parse;
  image_class->print = gtk_css_image_radial_print;
  image_class->compute = gtk_css_image_radial_compute;
  image_class->transition = gtk_css_image_radial_transition;
  image_class->equal = gtk_css_image_radial_equal;
  image_class->is_computed = gtk_css_image_radial_is_computed;

  object_class->dispose = gtk_css_image_radial_dispose;
}

static void
_gtk_css_image_radial_init (GtkCssImageRadial *radial)
{
}

