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

#include "gtkcsspositionvalueprivate.h"

#include "gtkcssnumbervalueprivate.h"

struct _GtkCssValue {
  GTK_CSS_VALUE_BASE
  GtkCssValue *x;
  GtkCssValue *y;
};

static void
gtk_css_value_position_free (GtkCssValue *value)
{
  _gtk_css_value_unref (value->x);
  _gtk_css_value_unref (value->y);
  g_free (value);
}

static GtkCssValue *
gtk_css_value_position_compute (GtkCssValue      *position,
                                guint             property_id,
                                GtkStyleProvider *provider,
                                GtkCssStyle      *style,
                                GtkCssStyle      *parent_style)
{
  GtkCssValue *x, *y;

  x = _gtk_css_value_compute (position->x, property_id, provider, style, parent_style);
  y = _gtk_css_value_compute (position->y, property_id, provider, style, parent_style);
  if (x == position->x && y == position->y)
    {
      _gtk_css_value_unref (x);
      _gtk_css_value_unref (y);
      return _gtk_css_value_ref (position);
    }

  return _gtk_css_position_value_new (x, y);
}

static gboolean
gtk_css_value_position_equal (const GtkCssValue *position1,
                              const GtkCssValue *position2)
{
  return _gtk_css_value_equal (position1->x, position2->x)
      && _gtk_css_value_equal (position1->y, position2->y);
}

static GtkCssValue *
gtk_css_value_position_transition (GtkCssValue *start,
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

  return _gtk_css_position_value_new (x, y);
}

static void
gtk_css_value_position_print (const GtkCssValue *position,
                              GString           *string)
{
  struct {
    const char *x_name;
    const char *y_name;
    GtkCssValue *number;
  } values[] = { 
    { "left",   "top",    _gtk_css_number_value_new (0, GTK_CSS_PERCENT) },
    { "right",  "bottom", _gtk_css_number_value_new (100, GTK_CSS_PERCENT) }
  };
  GtkCssValue *center = _gtk_css_number_value_new (50, GTK_CSS_PERCENT);
  guint i;

  if (_gtk_css_value_equal (position->x, center))
    {
      if (_gtk_css_value_equal (position->y, center))
        {
          g_string_append (string, "center");
          goto done;
        }
    }
  else
    {
      for (i = 0; i < G_N_ELEMENTS (values); i++)
        {
          if (_gtk_css_value_equal (position->x, values[i].number))
            {
              g_string_append (string, values[i].x_name);
              break;
            }
        }
      if (i == G_N_ELEMENTS (values))
        _gtk_css_value_print (position->x, string);

      if (_gtk_css_value_equal (position->y, center))
        goto done;

      g_string_append_c (string, ' ');
    }

  for (i = 0; i < G_N_ELEMENTS (values); i++)
    {
      if (_gtk_css_value_equal (position->y, values[i].number))
        {
          g_string_append (string, values[i].y_name);
          goto done;
        }
    }
  if (i == G_N_ELEMENTS (values))
    {
      if (_gtk_css_value_equal (position->x, center))
        g_string_append (string, "center ");
      _gtk_css_value_print (position->y, string);
    }

done:
  for (i = 0; i < G_N_ELEMENTS (values); i++)
    _gtk_css_value_unref (values[i].number);
  _gtk_css_value_unref (center);
}

static const GtkCssValueClass GTK_CSS_VALUE_POSITION = {
  "GtkCssPositionValue",
  gtk_css_value_position_free,
  gtk_css_value_position_compute,
  gtk_css_value_position_equal,
  gtk_css_value_position_transition,
  NULL,
  NULL,
  gtk_css_value_position_print
};

GtkCssValue *
_gtk_css_position_value_new (GtkCssValue *x,
                             GtkCssValue *y)
{
  GtkCssValue *result;

  result = _gtk_css_value_new (GtkCssValue, &GTK_CSS_VALUE_POSITION);
  result->x = x;
  result->y = y;
  result->is_computed = gtk_css_value_is_computed (x) &&
                        gtk_css_value_is_computed (y);

  return result;
}

static GtkCssValue *
position_value_parse (GtkCssParser *parser, gboolean try)
{
  static const struct {
    const char *name;
    guint       percentage;
    gboolean    horizontal;
    gboolean    swap;
  } names[] = {
    { "left",     0, TRUE,  FALSE },
    { "right",  100, TRUE,  FALSE },
    { "center",  50, TRUE,  TRUE  },
    { "top",      0, FALSE, FALSE },
    { "bottom", 100, FALSE, FALSE  },
  };
  GtkCssValue *x = NULL, *y = NULL;
  gboolean swap = FALSE;
  guint i;

  for (i = 0; i < G_N_ELEMENTS (names); i++)
    {
      if (gtk_css_parser_try_ident (parser, names[i].name))
        {
          if (names[i].horizontal)
	    x = _gtk_css_number_value_new (names[i].percentage, GTK_CSS_PERCENT);
          else
	    y = _gtk_css_number_value_new (names[i].percentage, GTK_CSS_PERCENT);
          swap = names[i].swap;
          break;
        }
    }
  if (i == G_N_ELEMENTS (names))
    {
      if (gtk_css_number_value_can_parse (parser))
        {
          x = _gtk_css_number_value_parse (parser,
                                           GTK_CSS_PARSE_PERCENT
                                           | GTK_CSS_PARSE_LENGTH);

          if (x == NULL)
            return NULL;
        }
      else
        {
          if (!try)
            gtk_css_parser_error_syntax (parser, "Unrecognized position value");
          return NULL;
        }
    }

  for (i = 0; i < G_N_ELEMENTS (names); i++)
    {
      if (!swap && !names[i].swap)
        {
          if (names[i].horizontal && x != NULL)
            continue;
          if (!names[i].horizontal && y != NULL)
            continue;
        }

      if (gtk_css_parser_try_ident (parser, names[i].name))
        {
          if (x)
            {
              if (names[i].horizontal && !names[i].swap)
                {
                  y = x;
	          x = _gtk_css_number_value_new (names[i].percentage, GTK_CSS_PERCENT);
                }
              else
                {
	          y = _gtk_css_number_value_new (names[i].percentage, GTK_CSS_PERCENT);
                }
            }
          else
            {
              g_assert (names[i].horizontal || names[i].swap);
	      x = _gtk_css_number_value_new (names[i].percentage, GTK_CSS_PERCENT);
            }
          break;
        }
    }

  if (i == G_N_ELEMENTS (names))
    {
      if (gtk_css_number_value_can_parse (parser))
        {
          if (y != NULL)
            {
              if (!try)
                gtk_css_parser_error_syntax (parser, "Invalid combination of values");
              _gtk_css_value_unref (y);
              return NULL;
            }
          y = _gtk_css_number_value_parse (parser,
                                           GTK_CSS_PARSE_PERCENT
                                           | GTK_CSS_PARSE_LENGTH);
          if (y == NULL)
            {
              _gtk_css_value_unref (x);
	      return NULL;
            }
        }
      else
        {
          if (y)
            x = _gtk_css_number_value_new (50, GTK_CSS_PERCENT);
          else
            y = _gtk_css_number_value_new (50, GTK_CSS_PERCENT);
        }
    }

  return _gtk_css_position_value_new (x, y);
}

GtkCssValue *
_gtk_css_position_value_parse (GtkCssParser *parser)
{
  return position_value_parse (parser, FALSE);
}

GtkCssValue *
_gtk_css_position_value_try_parse (GtkCssParser *parser)
{
  return position_value_parse (parser, TRUE);
}

GtkCssValue *
gtk_css_position_value_parse_spacing (GtkCssParser *parser)
{
  GtkCssValue *x, *y;

  x = _gtk_css_number_value_parse (parser, GTK_CSS_PARSE_LENGTH | GTK_CSS_POSITIVE_ONLY);
  if (x == NULL)
    return NULL;

  if (gtk_css_number_value_can_parse (parser))
    {
      y = _gtk_css_number_value_parse (parser, GTK_CSS_PARSE_LENGTH | GTK_CSS_POSITIVE_ONLY);
      if (y == NULL)
        {
          _gtk_css_value_unref (x);
          return NULL;
        }
    }
  else
    {
      y = _gtk_css_value_ref (x);
    }

  return _gtk_css_position_value_new (x, y);
}

double
_gtk_css_position_value_get_x (const GtkCssValue *position,
                               double             one_hundred_percent)
{
  g_return_val_if_fail (position != NULL, 0.0);
  g_return_val_if_fail (position->class == &GTK_CSS_VALUE_POSITION, 0.0);

  return _gtk_css_number_value_get (position->x, one_hundred_percent);
}

double
_gtk_css_position_value_get_y (const GtkCssValue *position,
                               double             one_hundred_percent)
{
  g_return_val_if_fail (position != NULL, 0.0);
  g_return_val_if_fail (position->class == &GTK_CSS_VALUE_POSITION, 0.0);

  return _gtk_css_number_value_get (position->y, one_hundred_percent);
}

