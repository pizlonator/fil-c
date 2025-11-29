/* GTK - The GIMP Toolkit
 * Copyright (C) 2021 Red Hat, Inc.
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

#include "gtkcsslineheightvalueprivate.h"
#include "gtkcssnumbervalueprivate.h"
#include "gtkcssstyleprivate.h"

struct _GtkCssValue {
  GTK_CSS_VALUE_BASE
  GtkCssValue *height;
};

static GtkCssValue *default_line_height;

static GtkCssValue *gtk_css_value_line_height_new_empty (void);
static GtkCssValue *gtk_css_value_line_height_new       (GtkCssValue *height);

static void
gtk_css_value_line_height_free (GtkCssValue *value)
{
  _gtk_css_value_unref (value->height);
  g_free (value);
}

static GtkCssValue *
gtk_css_value_line_height_compute (GtkCssValue      *value,
                                   guint             property_id,
                                   GtkStyleProvider *provider,
                                   GtkCssStyle      *style,
                                   GtkCssStyle      *parent_style)
{
  GtkCssValue *height;

  height = _gtk_css_value_compute (value->height, property_id, provider, style, parent_style);

  if (gtk_css_number_value_get_dimension (height) == GTK_CSS_DIMENSION_PERCENTAGE)
    {
      double factor;
      GtkCssValue *computed;

      factor = _gtk_css_number_value_get (height, 1);
      computed = gtk_css_number_value_multiply (style->core->font_size, factor);

      _gtk_css_value_unref (height);

      return computed;
    }
  else
    {
      return height;
    }
}

static gboolean
gtk_css_value_line_height_equal (const GtkCssValue *value1,
                                 const GtkCssValue *value2)
{
  if (value1->height == NULL || value2->height == NULL)
    return FALSE;

  return _gtk_css_value_equal (value1->height, value2->height);
}

static GtkCssValue *
gtk_css_value_line_height_transition (GtkCssValue *start,
                                      GtkCssValue *end,
                                      guint        property_id,
                                      double       progress)
{
  GtkCssValue *height;

  if (start->height == NULL || end->height == NULL)
    return NULL;

  height = _gtk_css_value_transition (start->height, end->height, property_id, progress);
  if (height == NULL)
    return NULL;

  return gtk_css_value_line_height_new (height);
}

static void
gtk_css_value_line_height_print (const GtkCssValue *value,
                                 GString           *string)
{
  if (value->height == NULL)
    g_string_append (string, "normal");
  else
    _gtk_css_value_print (value->height, string);
}

static const GtkCssValueClass GTK_CSS_VALUE_LINE_HEIGHT = {
  "GtkCssLineHeightValue",
  gtk_css_value_line_height_free,
  gtk_css_value_line_height_compute,
  gtk_css_value_line_height_equal,
  gtk_css_value_line_height_transition,
  NULL,
  NULL,
  gtk_css_value_line_height_print
};

static GtkCssValue *
gtk_css_value_line_height_new_empty (void)
{
  GtkCssValue *result;

  result = _gtk_css_value_new (GtkCssValue, &GTK_CSS_VALUE_LINE_HEIGHT);
  result->height = NULL;
  result->is_computed = TRUE;

  return result;
}

static GtkCssValue *
gtk_css_value_line_height_new (GtkCssValue *height)
{
  GtkCssValue *result;

  result = _gtk_css_value_new (GtkCssValue, &GTK_CSS_VALUE_LINE_HEIGHT);
  result->height = height;

  return result;
}

GtkCssValue *
gtk_css_line_height_value_get_default (void)
{
  if (default_line_height == NULL)
    default_line_height = gtk_css_value_line_height_new_empty ();

  return default_line_height;
}

GtkCssValue *
gtk_css_line_height_value_parse (GtkCssParser *parser)
{
  GtkCssValue *height;

  if (gtk_css_parser_try_ident (parser, "normal"))
    return _gtk_css_value_ref (gtk_css_line_height_value_get_default ());

  height = _gtk_css_number_value_parse (parser, GTK_CSS_PARSE_NUMBER |
                                                GTK_CSS_PARSE_PERCENT |
                                                GTK_CSS_PARSE_LENGTH |
                                                GTK_CSS_POSITIVE_ONLY);
  if (!height)
    return NULL;

  return gtk_css_value_line_height_new (height);
}

double
gtk_css_line_height_value_get (const GtkCssValue *value)
{
  if (value->class == &GTK_CSS_VALUE_LINE_HEIGHT)
    return 0.0;

  return _gtk_css_number_value_get (value, 1);
}
