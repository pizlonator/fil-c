/* GTK - The GIMP Toolkit
 * Copyright (C) 2011 Red Hat, Inc.
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

#include "gtkcssbgsizevalueprivate.h"

#include "gtkcssnumbervalueprivate.h"

struct _GtkCssValue {
  GTK_CSS_VALUE_BASE
  guint cover :1;
  guint contain :1;
  GtkCssValue *x;
  GtkCssValue *y;
};

static void
gtk_css_value_bg_size_free (GtkCssValue *value)
{
  if (value->x)
    _gtk_css_value_unref (value->x);
  if (value->y)
    _gtk_css_value_unref (value->y);

  g_free (value);
}

static GtkCssValue *
gtk_css_value_bg_size_compute (GtkCssValue      *value,
                               guint             property_id,
                               GtkStyleProvider *provider,
                               GtkCssStyle      *style,
                               GtkCssStyle      *parent_style)
{
  GtkCssValue *x, *y;

  if (value->x == NULL && value->y == NULL)
    return _gtk_css_value_ref (value);

  x = y = NULL;

  if (value->x)
    x = _gtk_css_value_compute (value->x, property_id, provider, style, parent_style);

  if (value->y)
    y = _gtk_css_value_compute (value->y, property_id, provider, style, parent_style);

  if (x == value->x && y == value->y)
    {
      if (x)
        _gtk_css_value_unref (x);
      if (y)
        _gtk_css_value_unref (y);

      return _gtk_css_value_ref (value);
    }

  return _gtk_css_bg_size_value_new (value->x ? x : NULL,
                                     value->y ? y : NULL);
}

static gboolean
gtk_css_value_bg_size_equal (const GtkCssValue *value1,
                             const GtkCssValue *value2)
{
  return value1->cover == value2->cover &&
         value1->contain == value2->contain &&
         (value1->x == value2->x ||
          (value1->x != NULL && value2->x != NULL &&
           _gtk_css_value_equal (value1->x, value2->x))) &&
         (value1->y == value2->y ||
          (value1->y != NULL && value2->y != NULL &&
           _gtk_css_value_equal (value1->y, value2->y)));
}

static GtkCssValue *
gtk_css_value_bg_size_transition (GtkCssValue *start,
                                  GtkCssValue *end,
                                  guint        property_id,
                                  double       progress)
{
  GtkCssValue *x, *y;

  if (start->cover)
    return end->cover ? _gtk_css_value_ref (end) : NULL;
  if (start->contain)
    return end->contain ? _gtk_css_value_ref (end) : NULL;

  if ((start->x != NULL) ^ (end->x != NULL) ||
      (start->y != NULL) ^ (end->y != NULL))
    return NULL;

  if (start->x)
    {
      x = _gtk_css_value_transition (start->x, end->x, property_id, progress);
      if (x == NULL)
        return NULL;
    }
  else
    x = NULL;

  if (start->y)
    {
      y = _gtk_css_value_transition (start->y, end->y, property_id, progress);
      if (y == NULL)
        {
          _gtk_css_value_unref (x);
          return NULL;
        }
    }
  else
    y = NULL;

  return _gtk_css_bg_size_value_new (x, y);
}

static void
gtk_css_value_bg_size_print (const GtkCssValue *value,
                             GString           *string)
{
  if (value->cover)
    g_string_append (string, "cover");
  else if (value->contain)
    g_string_append (string, "contain");
  else
    {
      if (value->x == NULL)
        g_string_append (string, "auto");
      else
        _gtk_css_value_print (value->x, string);

      if (value->y)
        {
          g_string_append_c (string, ' ');
          _gtk_css_value_print (value->y, string);
        }
    }
}

static const GtkCssValueClass GTK_CSS_VALUE_BG_SIZE = {
  "GtkCssBgSizeValue",
  gtk_css_value_bg_size_free,
  gtk_css_value_bg_size_compute,
  gtk_css_value_bg_size_equal,
  gtk_css_value_bg_size_transition,
  NULL,
  NULL,
  gtk_css_value_bg_size_print
};

static GtkCssValue auto_singleton = { &GTK_CSS_VALUE_BG_SIZE, 1, TRUE, FALSE, FALSE, NULL, NULL };
static GtkCssValue cover_singleton = { &GTK_CSS_VALUE_BG_SIZE, 1, TRUE, TRUE, FALSE, NULL, NULL };
static GtkCssValue contain_singleton = { &GTK_CSS_VALUE_BG_SIZE, 1, TRUE, FALSE, TRUE, NULL, NULL };

GtkCssValue *
_gtk_css_bg_size_value_new (GtkCssValue *x,
                            GtkCssValue *y)
{
  GtkCssValue *result;

  if (x == NULL && y == NULL)
    return _gtk_css_value_ref (&auto_singleton);

  result = _gtk_css_value_new (GtkCssValue, &GTK_CSS_VALUE_BG_SIZE);
  result->x = x;
  result->y = y;
  result->is_computed = (!x || gtk_css_value_is_computed (x)) &&
                        (!y || gtk_css_value_is_computed (y));

  return result;
}

GtkCssValue *
_gtk_css_bg_size_value_parse (GtkCssParser *parser)
{
  GtkCssValue *x, *y;

  if (gtk_css_parser_try_ident (parser, "cover"))
    return _gtk_css_value_ref (&cover_singleton);
  else if (gtk_css_parser_try_ident (parser, "contain"))
    return _gtk_css_value_ref (&contain_singleton);

  if (gtk_css_parser_try_ident (parser, "auto"))
    x = NULL;
  else
    {
      x = _gtk_css_number_value_parse (parser,
                                       GTK_CSS_POSITIVE_ONLY
                                       | GTK_CSS_PARSE_PERCENT
                                       | GTK_CSS_PARSE_LENGTH);
      if (x == NULL)
        return NULL;
    }

  if (gtk_css_parser_try_ident (parser, "auto"))
    y = NULL;
  else if (!gtk_css_number_value_can_parse (parser))
    y = NULL;
  else
    {
      y = _gtk_css_number_value_parse (parser,
                                       GTK_CSS_POSITIVE_ONLY
                                       | GTK_CSS_PARSE_PERCENT
                                       | GTK_CSS_PARSE_LENGTH);
      if (y == NULL)
        {
          _gtk_css_value_unref (x);
          return NULL;
        }
    }

  return _gtk_css_bg_size_value_new (x, y);
}

static void
gtk_css_bg_size_compute_size_for_cover_contain (gboolean     cover,
                                                GtkCssImage *image,
                                                double       width,
                                                double       height,
                                                double      *concrete_width,
                                                double      *concrete_height)
{
  double aspect, image_aspect;
  
  image_aspect = _gtk_css_image_get_aspect_ratio (image);
  if (image_aspect == 0.0)
    {
      *concrete_width = width;
      *concrete_height = height;
      return;
    }

  aspect = width / height;

  if ((aspect >= image_aspect && cover) ||
      (aspect < image_aspect && !cover))
    {
      *concrete_width = width;
      *concrete_height = width / image_aspect;
    }
  else
    {
      *concrete_height = height;
      *concrete_width = height * image_aspect;
    }
}

void
_gtk_css_bg_size_value_compute_size (const GtkCssValue *value,
                                     GtkCssImage       *image,
                                     double             area_width,
                                     double             area_height,
                                     double            *out_width,
                                     double            *out_height)
{
  g_return_if_fail (value->class == &GTK_CSS_VALUE_BG_SIZE);

  if (value->contain || value->cover)
    {
      gtk_css_bg_size_compute_size_for_cover_contain (value->cover,
                                                      image,
                                                      area_width, area_height,
                                                      out_width, out_height);
    }
  else
    {
      double x, y;

      /* note: 0 does the right thing later for 'auto' */
      x = value->x ? _gtk_css_number_value_get (value->x, area_width) : 0;
      y = value->y ? _gtk_css_number_value_get (value->y, area_height) : 0;

      if ((x <= 0 && value->x) ||
          (y <= 0 && value->y))
        {
          *out_width = 0;
          *out_height = 0;
        }
      else
        {
          _gtk_css_image_get_concrete_size (image,
                                            x, y,
                                            area_width, area_height,
                                            out_width, out_height);
        }
    }
}

