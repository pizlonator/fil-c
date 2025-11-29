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

#include "gtkcssimageconicprivate.h"

#include <math.h>

#include "gtkcsscolorvalueprivate.h"
#include "gtkcssnumbervalueprivate.h"
#include "gtkcsspositionvalueprivate.h"
#include "gtkcssprovider.h"

G_DEFINE_TYPE (GtkCssImageConic, gtk_css_image_conic, GTK_TYPE_CSS_IMAGE)

static void
gtk_css_image_conic_snapshot (GtkCssImage        *image,
                              GtkSnapshot        *snapshot,
                              double              width,
                              double              height)
{
  GtkCssImageConic *self = GTK_CSS_IMAGE_CONIC (image);
  GskColorStop *stops;
  int i, last;
  double offset;

  stops = g_newa (GskColorStop, self->n_stops);

  last = -1;
  offset = 0;
  for (i = 0; i < self->n_stops; i++)
    {
      const GtkCssImageConicColorStop *stop = &self->color_stops[i];
      double pos, step;

      if (stop->offset == NULL)
        {
          if (i == 0)
            pos = 0.0;
          else if (i + 1 == self->n_stops)
            pos = 1.0;
          else
            continue;
        }
      else
        {
          pos = _gtk_css_number_value_get (stop->offset, 360) / 360;
          pos = CLAMP (pos, 0.0, 1.0);
        }

      pos = MAX (pos, offset);
      step = (pos - offset) / (i - last);
      for (last = last + 1; last <= i; last++)
        {
          stop = &self->color_stops[last];

          offset += step;

          stops[last].offset = offset;
          stops[last].color = *gtk_css_color_value_get_rgba (stop->color);
        }

      offset = pos;
      last = i;
    }

    gtk_snapshot_append_conic_gradient (
          snapshot,
          &GRAPHENE_RECT_INIT (0, 0, width, height),
          &GRAPHENE_POINT_INIT (_gtk_css_position_value_get_x (self->center, width),
                                _gtk_css_position_value_get_y (self->center, height)),
          _gtk_css_number_value_get (self->rotation, 360),
          stops,
          self->n_stops);
}

static gboolean
parse_angles (GtkCssParser *parser,
              gpointer      option_data,
              gpointer      unused)
{
  GtkCssValue **angles = option_data;
  
  angles[0] = _gtk_css_number_value_parse (parser, GTK_CSS_PARSE_ANGLE | GTK_CSS_PARSE_PERCENT);
  if (angles[0] == NULL)
    return FALSE;

  if (gtk_css_number_value_can_parse (parser))
    {
      angles[1] = _gtk_css_number_value_parse (parser, GTK_CSS_PARSE_ANGLE | GTK_CSS_PARSE_PERCENT);
      if (angles[1] == NULL)
        return FALSE;
    }

  return TRUE;
}

static gboolean
parse_color (GtkCssParser *parser,
             gpointer      option_data,
             gpointer      unused)
{
  GtkCssValue **color = option_data;
  
  *color = _gtk_css_color_value_parse (parser);
  if (*color == NULL)
    return FALSE;

  return TRUE;
}

static guint
gtk_css_image_conic_parse_color_stop (GtkCssImageConic *self,
                                      GtkCssParser      *parser,
                                      GArray            *stop_array)
{
  GtkCssValue *angles[2] = { NULL, NULL };
  GtkCssValue *color = NULL;
  GtkCssParseOption options[] =
    {
      { (void *) gtk_css_number_value_can_parse, parse_angles, &angles },
      { (void *) gtk_css_color_value_can_parse, parse_color, &color },
    };

  if (!gtk_css_parser_consume_any (parser, options, G_N_ELEMENTS (options), NULL))
    goto fail;

  if (color == NULL)
    {
      gtk_css_parser_error_syntax (parser, "Expected shadow value to contain a length");
      goto fail;
    }

  g_array_append_vals (stop_array, (GtkCssImageConicColorStop[1]) {
                         { angles[0], color }
                       },
                       1);
  if (angles[1])
    g_array_append_vals (stop_array, (GtkCssImageConicColorStop[1]) {
                           { angles[1], gtk_css_value_ref (color) }
                         },
                         1);

  return 1;

fail:
  g_clear_pointer (&angles[0], gtk_css_value_unref);
  g_clear_pointer (&angles[1], gtk_css_value_unref);
  g_clear_pointer (&color, gtk_css_value_unref);
  return 0;
}

static guint
gtk_css_image_conic_parse_first_arg (GtkCssImageConic *self,
                                     GtkCssParser      *parser,
                                     GArray            *stop_array)
{
  gboolean nothing_parsed = TRUE;

  if (gtk_css_parser_try_ident (parser, "from"))
    {
      self->rotation = _gtk_css_number_value_parse (parser, GTK_CSS_PARSE_ANGLE);
      if (self->rotation == NULL)
        return 0;
      nothing_parsed = FALSE;
    }
  else
    {
      self->rotation = _gtk_css_number_value_new (0, GTK_CSS_DEG);
    }

  if (gtk_css_parser_try_ident (parser, "at"))
    {
      self->center = _gtk_css_position_value_parse (parser);
      if (self->center == NULL)
        return 0;
      nothing_parsed = FALSE;
    }
  else
    {
      self->center = _gtk_css_position_value_new (_gtk_css_number_value_new (50, GTK_CSS_PERCENT),
                                                   _gtk_css_number_value_new (50, GTK_CSS_PERCENT));
    }

  if (!nothing_parsed)
    return 1;

  return 1 + gtk_css_image_conic_parse_color_stop (self, parser, stop_array);
}

typedef struct
{
  GtkCssImageConic *self;
  GArray *stop_array;
} ParseData;

static guint
gtk_css_image_conic_parse_arg (GtkCssParser *parser,
                                guint         arg,
                                gpointer      user_data)
{
  ParseData *parse_data = user_data;
  GtkCssImageConic *self = parse_data->self;

  if (arg == 0)
    return gtk_css_image_conic_parse_first_arg (self, parser, parse_data->stop_array);
  else
    return gtk_css_image_conic_parse_color_stop (self, parser, parse_data->stop_array);
}

static gboolean
gtk_css_image_conic_parse (GtkCssImage  *image,
                            GtkCssParser *parser)
{
  GtkCssImageConic *self = GTK_CSS_IMAGE_CONIC (image);
  ParseData parse_data;
  gboolean success;

  if (!gtk_css_parser_has_function (parser, "conic-gradient"))
    {
      gtk_css_parser_error_syntax (parser, "Not a conic gradient");
      return FALSE;
    }

  parse_data.self = self;
  parse_data.stop_array = g_array_new (TRUE, FALSE, sizeof (GtkCssImageConicColorStop));

  success = gtk_css_parser_consume_function (parser, 3, G_MAXUINT, gtk_css_image_conic_parse_arg, &parse_data);

  if (!success)
    {
      g_array_free (parse_data.stop_array, TRUE);
    }
  else
    {
      self->n_stops = parse_data.stop_array->len;
      self->color_stops = (GtkCssImageConicColorStop *)g_array_free (parse_data.stop_array, FALSE);
    }

  return success;
}

static void
gtk_css_image_conic_print (GtkCssImage *image,
                           GString     *string)
{
  GtkCssImageConic *self = GTK_CSS_IMAGE_CONIC (image);
  gboolean written = FALSE;
  guint i;

  g_string_append (string, "self-gradient(");

  if (self->center)
    {
      GtkCssValue *compare = _gtk_css_position_value_new (_gtk_css_number_value_new (50, GTK_CSS_PERCENT),
                                                          _gtk_css_number_value_new (50, GTK_CSS_PERCENT));

      if (!_gtk_css_value_equal (self->center, compare))
        {
          g_string_append (string, "from ");
          _gtk_css_value_print (self->center, string);
          written = TRUE;
        }

      gtk_css_value_unref (compare);
    }

  if (self->rotation && _gtk_css_number_value_get (self->rotation, 360) != 0)
    {
      if (written)
        g_string_append_c (string, ' ');
      g_string_append (string, "at ");
      _gtk_css_value_print (self->rotation, string);
    }

  if (written)
    g_string_append (string, ", ");

  for (i = 0; i < self->n_stops; i++)
    {
      const GtkCssImageConicColorStop *stop = &self->color_stops[i];

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
gtk_css_image_conic_compute (GtkCssImage      *image,
                             guint             property_id,
                             GtkStyleProvider *provider,
                             GtkCssStyle      *style,
                             GtkCssStyle      *parent_style)
{
  GtkCssImageConic *self = GTK_CSS_IMAGE_CONIC (image);
  GtkCssImageConic *copy;
  guint i;

  copy = g_object_new (GTK_TYPE_CSS_IMAGE_CONIC, NULL);

  copy->center = _gtk_css_value_compute (self->center, property_id, provider, style, parent_style);
  copy->rotation = _gtk_css_value_compute (self->rotation, property_id, provider, style, parent_style);

  copy->n_stops = self->n_stops;
  copy->color_stops = g_malloc (sizeof (GtkCssImageConicColorStop) * copy->n_stops);
  for (i = 0; i < self->n_stops; i++)
    {
      const GtkCssImageConicColorStop *stop = &self->color_stops[i];
      GtkCssImageConicColorStop *scopy = &copy->color_stops[i];

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
gtk_css_image_conic_transition (GtkCssImage *start_image,
                                GtkCssImage *end_image,
                                guint        property_id,
                                double       progress)
{
  GtkCssImageConic *start, *end, *result;
  guint i;

  start = GTK_CSS_IMAGE_CONIC (start_image);

  if (end_image == NULL)
    return GTK_CSS_IMAGE_CLASS (gtk_css_image_conic_parent_class)->transition (start_image, end_image, property_id, progress);

  if (!GTK_IS_CSS_IMAGE_CONIC (end_image))
    return GTK_CSS_IMAGE_CLASS (gtk_css_image_conic_parent_class)->transition (start_image, end_image, property_id, progress);

  end = GTK_CSS_IMAGE_CONIC (end_image);

  if (start->n_stops != end->n_stops)
    return GTK_CSS_IMAGE_CLASS (gtk_css_image_conic_parent_class)->transition (start_image, end_image, property_id, progress);

  result = g_object_new (GTK_TYPE_CSS_IMAGE_CONIC, NULL);

  result->center = _gtk_css_value_transition (start->center, end->center, property_id, progress);
  if (result->center == NULL)
    goto fail;

  result->rotation = _gtk_css_value_transition (start->rotation, end->rotation, property_id, progress);
  if (result->rotation == NULL)
    goto fail;

  result->color_stops = g_malloc (sizeof (GtkCssImageConicColorStop) * start->n_stops);
  result->n_stops = 0;
  for (i = 0; i < start->n_stops; i++)
    {
      const GtkCssImageConicColorStop *start_stop = &start->color_stops[i];
      const GtkCssImageConicColorStop *end_stop = &end->color_stops[i];
      GtkCssImageConicColorStop *stop = &result->color_stops[i];

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
  return GTK_CSS_IMAGE_CLASS (gtk_css_image_conic_parent_class)->transition (start_image, end_image, property_id, progress);
}

static gboolean
gtk_css_image_conic_equal (GtkCssImage *image1,
                            GtkCssImage *image2)
{
  GtkCssImageConic *conic1 = (GtkCssImageConic *) image1;
  GtkCssImageConic *conic2 = (GtkCssImageConic *) image2;
  guint i;

  if (!_gtk_css_value_equal (conic1->center, conic2->center) ||
      !_gtk_css_value_equal (conic1->rotation, conic2->rotation))
    return FALSE;

  for (i = 0; i < conic1->n_stops; i++)
    {
      const GtkCssImageConicColorStop *stop1 = &conic1->color_stops[i];
      const GtkCssImageConicColorStop *stop2 = &conic2->color_stops[i];

      if (!_gtk_css_value_equal0 (stop1->offset, stop2->offset) ||
          !_gtk_css_value_equal (stop1->color, stop2->color))
        return FALSE;
    }

  return TRUE;
}

static void
gtk_css_image_conic_dispose (GObject *object)
{
  GtkCssImageConic *self = GTK_CSS_IMAGE_CONIC (object);
  guint i;

  for (i = 0; i < self->n_stops; i ++)
    {
      GtkCssImageConicColorStop *stop = &self->color_stops[i];

      _gtk_css_value_unref (stop->color);
      if (stop->offset)
        _gtk_css_value_unref (stop->offset);
    }
  g_free (self->color_stops);

  g_clear_pointer (&self->center, gtk_css_value_unref);
  g_clear_pointer (&self->rotation, gtk_css_value_unref);

  G_OBJECT_CLASS (gtk_css_image_conic_parent_class)->dispose (object);
}

static gboolean
gtk_css_image_conic_is_computed (GtkCssImage *image)
{
  GtkCssImageConic *self = GTK_CSS_IMAGE_CONIC (image);
  guint i;
  gboolean computed = TRUE;

  computed = !self->center || gtk_css_value_is_computed (self->center);
  computed &= !self->rotation || gtk_css_value_is_computed (self->rotation);

  for (i = 0; i < self->n_stops; i ++)
    {
      const GtkCssImageConicColorStop *stop = &self->color_stops[i];

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
gtk_css_image_conic_class_init (GtkCssImageConicClass *klass)
{
  GtkCssImageClass *image_class = GTK_CSS_IMAGE_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  image_class->snapshot = gtk_css_image_conic_snapshot;
  image_class->parse = gtk_css_image_conic_parse;
  image_class->print = gtk_css_image_conic_print;
  image_class->compute = gtk_css_image_conic_compute;
  image_class->equal = gtk_css_image_conic_equal;
  image_class->transition = gtk_css_image_conic_transition;
  image_class->is_computed = gtk_css_image_conic_is_computed;

  object_class->dispose = gtk_css_image_conic_dispose;
}

static void
gtk_css_image_conic_init (GtkCssImageConic *self)
{
}

