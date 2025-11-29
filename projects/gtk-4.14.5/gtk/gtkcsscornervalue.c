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

#include "gtkcsscornervalueprivate.h"
#include "gtkcssnumbervalueprivate.h"
#include "gtkcssdimensionvalueprivate.h"

struct _GtkCssValue {
  GTK_CSS_VALUE_BASE
  GtkCssValue *x;
  GtkCssValue *y;
};

static void
gtk_css_value_corner_free (GtkCssValue *value)
{
  _gtk_css_value_unref (value->x);
  _gtk_css_value_unref (value->y);

  g_free (value);
}

static GtkCssValue *
gtk_css_value_corner_compute (GtkCssValue      *corner,
                              guint             property_id,
                              GtkStyleProvider *provider,
                              GtkCssStyle      *style,
                              GtkCssStyle      *parent_style)
{
  GtkCssValue *x, *y;

  x = _gtk_css_value_compute (corner->x, property_id, provider, style, parent_style);
  y = _gtk_css_value_compute (corner->y, property_id, provider, style, parent_style);
  if (x == corner->x && y == corner->y)
    {
      _gtk_css_value_unref (x);
      _gtk_css_value_unref (y);
      return _gtk_css_value_ref (corner);
    }

  return _gtk_css_corner_value_new (x, y);
}

static gboolean
gtk_css_value_corner_equal (const GtkCssValue *corner1,
                            const GtkCssValue *corner2)
{
  return _gtk_css_value_equal (corner1->x, corner2->x)
      && _gtk_css_value_equal (corner1->y, corner2->y);
}

static GtkCssValue *
gtk_css_value_corner_transition (GtkCssValue *start,
                                 GtkCssValue *end,
                                 guint        property_id,
                                 double       progress)
{
  GtkCssValue *x, *y;

  x = _gtk_css_value_transition (start->x, end->x, property_id, progress);
  if (x == NULL)
    return NULL;
  y = _gtk_css_value_transition (start->y, end->y, property_id, progress);
  if (y == NULL)
    {
      _gtk_css_value_unref (x);
      return NULL;
    }

  return _gtk_css_corner_value_new (x, y);
}

static void
gtk_css_value_corner_print (const GtkCssValue *corner,
                            GString           *string)
{
  _gtk_css_value_print (corner->x, string);
  if (!_gtk_css_value_equal (corner->x, corner->y))
    {
      g_string_append_c (string, ' ');
      _gtk_css_value_print (corner->y, string);
    }
}

static const GtkCssValueClass GTK_CSS_VALUE_CORNER = {
  "GtkCssCornerValue",
  gtk_css_value_corner_free,
  gtk_css_value_corner_compute,
  gtk_css_value_corner_equal,
  gtk_css_value_corner_transition,
  NULL,
  NULL,
  gtk_css_value_corner_print
};

static GtkCssValue corner_singletons[] = {
  { &GTK_CSS_VALUE_CORNER, 1, TRUE, NULL, NULL },
  { &GTK_CSS_VALUE_CORNER, 1, TRUE, NULL, NULL },
  { &GTK_CSS_VALUE_CORNER, 1, TRUE, NULL, NULL },
  { &GTK_CSS_VALUE_CORNER, 1, TRUE, NULL, NULL },
  { &GTK_CSS_VALUE_CORNER, 1, TRUE, NULL, NULL },
  { &GTK_CSS_VALUE_CORNER, 1, TRUE, NULL, NULL },
  { &GTK_CSS_VALUE_CORNER, 1, TRUE, NULL, NULL },
  { &GTK_CSS_VALUE_CORNER, 1, TRUE, NULL, NULL },
};

static inline void
initialize_corner_singletons (void)
{
  static gboolean initialized = FALSE;

  if (initialized)
    return;

  for (unsigned int i = 0; i < G_N_ELEMENTS (corner_singletons); i++)
    {
      corner_singletons[i].x = gtk_css_dimension_value_new (i, GTK_CSS_PX);
      corner_singletons[i].y = gtk_css_value_ref (corner_singletons[i].x);
    }

  initialized = TRUE;
}

GtkCssValue *
_gtk_css_corner_value_new (GtkCssValue *x,
                           GtkCssValue *y)
{
  GtkCssValue *result;

  if (x == y &&
      gtk_css_number_value_get_dimension (x) == GTK_CSS_DIMENSION_LENGTH)
    {
      initialize_corner_singletons ();

      for (unsigned int i = 0; i < G_N_ELEMENTS (corner_singletons); i++)
        {
          if (corner_singletons[i].x == x)
            {
              gtk_css_value_unref (x);
              gtk_css_value_unref (y);

              return gtk_css_value_ref (&corner_singletons[i]);
            }
        }
    }

  result = _gtk_css_value_new (GtkCssValue, &GTK_CSS_VALUE_CORNER);
  result->x = x;
  result->y = y;

  return result;
}

GtkCssValue *
_gtk_css_corner_value_parse (GtkCssParser *parser)
{
  GtkCssValue *x, *y;

  x = _gtk_css_number_value_parse (parser,
                                   GTK_CSS_POSITIVE_ONLY
                                   | GTK_CSS_PARSE_PERCENT
                                   | GTK_CSS_PARSE_LENGTH);
  if (x == NULL)
    return NULL;

  if (!gtk_css_number_value_can_parse (parser))
    y = _gtk_css_value_ref (x);
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

  return _gtk_css_corner_value_new (x, y);
}

double
_gtk_css_corner_value_get_x (const GtkCssValue *corner,
                             double             one_hundred_percent)
{
  g_return_val_if_fail (corner != NULL, 0.0);
  g_return_val_if_fail (corner->class == &GTK_CSS_VALUE_CORNER, 0.0);

  return _gtk_css_number_value_get (corner->x, one_hundred_percent);
}

double
_gtk_css_corner_value_get_y (const GtkCssValue *corner,
                             double             one_hundred_percent)
{
  g_return_val_if_fail (corner != NULL, 0.0);
  g_return_val_if_fail (corner->class == &GTK_CSS_VALUE_CORNER, 0.0);

  return _gtk_css_number_value_get (corner->y, one_hundred_percent);
}

gboolean
gtk_css_corner_value_is_zero (const GtkCssValue *corner)
{
  if (corner->class != &GTK_CSS_VALUE_CORNER)
    return gtk_css_dimension_value_is_zero (corner);

  return gtk_css_dimension_value_is_zero (corner->x) &&
         gtk_css_dimension_value_is_zero (corner->y);
}
