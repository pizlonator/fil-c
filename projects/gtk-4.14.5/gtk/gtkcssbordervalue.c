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

#include "gtkcssbordervalueprivate.h"

#include "gtkcssnumbervalueprivate.h"

struct _GtkCssValue {
  GTK_CSS_VALUE_BASE
  guint fill :1;
  GtkCssValue *values[4];
};

static void
gtk_css_value_border_free (GtkCssValue *value)
{
  guint i;

  for (i = 0; i < 4; i++)
    {
      if (value->values[i])
        _gtk_css_value_unref (value->values[i]);
    }

  g_free (value);
}

static GtkCssValue *
gtk_css_value_border_compute (GtkCssValue      *value,
                              guint             property_id,
                              GtkStyleProvider *provider,
                              GtkCssStyle      *style,
                              GtkCssStyle      *parent_style)
{
  GtkCssValue *values[4];
  GtkCssValue *computed;
  gboolean changed = FALSE;
  guint i;

  for (i = 0; i < 4; i++)
    {
      if (value->values[i])
        {
          values[i] = _gtk_css_value_compute (value->values[i], property_id, provider, style, parent_style);
          changed |= (values[i] != value->values[i]);
        }
      else
        {
          values[i] = NULL;
        }
    }

  if (!changed)
    {
      for (i = 0; i < 4; i++)
        {
          if (values[i] != NULL)
            _gtk_css_value_unref (values[i]);
        }
      return _gtk_css_value_ref (value);
    }

  computed = _gtk_css_border_value_new (values[0], values[1], values[2], values[3]);
  computed->fill = value->fill;

  return computed;
}

static gboolean
gtk_css_value_border_equal (const GtkCssValue *value1,
                            const GtkCssValue *value2)
{
  guint i;

  if (value1->fill != value2->fill)
    return FALSE;

  for (i = 0; i < 4; i++)
    {
      if (!_gtk_css_value_equal0 (value1->values[i], value2->values[i]))
        return FALSE;
    }

  return TRUE;
}

static GtkCssValue *
gtk_css_value_border_transition (GtkCssValue *start,
                                 GtkCssValue *end,
                                 guint        property_id,
                                 double       progress)
{
  return NULL;
}

static void
gtk_css_value_border_print (const GtkCssValue *value,
                            GString           *string)
{
  guint i, n;

  if (!_gtk_css_value_equal0 (value->values[GTK_CSS_RIGHT], value->values[GTK_CSS_LEFT]))
    n = 4;
  else if (!_gtk_css_value_equal0 (value->values[GTK_CSS_TOP], value->values[GTK_CSS_BOTTOM]))
    n = 3;
  else if (!_gtk_css_value_equal0 (value->values[GTK_CSS_TOP], value->values[GTK_CSS_RIGHT]))
    n = 2;
  else
    n = 1;

  for (i = 0; i < n; i++)
    {
      if (i > 0)
        g_string_append_c (string, ' ');

      if (value->values[i] == NULL)
        g_string_append (string, "auto");
      else
        _gtk_css_value_print (value->values[i], string);
    }

  if (value->fill)
    g_string_append (string, " fill");
}

static const GtkCssValueClass GTK_CSS_VALUE_BORDER = {
  "GtkCssBorderValue",
  gtk_css_value_border_free,
  gtk_css_value_border_compute,
  gtk_css_value_border_equal,
  gtk_css_value_border_transition,
  NULL,
  NULL,
  gtk_css_value_border_print
};

GtkCssValue *
_gtk_css_border_value_new (GtkCssValue *top,
                           GtkCssValue *right,
                           GtkCssValue *bottom,
                           GtkCssValue *left)
{
  GtkCssValue *result;

  result = _gtk_css_value_new (GtkCssValue, &GTK_CSS_VALUE_BORDER);
  result->values[GTK_CSS_TOP] = top;
  result->values[GTK_CSS_RIGHT] = right;
  result->values[GTK_CSS_BOTTOM] = bottom;
  result->values[GTK_CSS_LEFT] = left;
  result->is_computed = (top && gtk_css_value_is_computed (top)) &&
                        (right && gtk_css_value_is_computed (right)) &&
                        (bottom && gtk_css_value_is_computed (bottom)) &&
                        (left && gtk_css_value_is_computed (left));

  return result;
}

GtkCssValue *
_gtk_css_border_value_parse (GtkCssParser           *parser,
                             GtkCssNumberParseFlags  flags,
                             gboolean                allow_auto,
                             gboolean                allow_fill)
{
  GtkCssValue *result;
  guint i;

  result = _gtk_css_border_value_new (NULL, NULL, NULL, NULL);

  if (allow_fill)
    result->fill = gtk_css_parser_try_ident (parser, "fill");

  for (i = 0; i < 4; i++)
    {
      if (allow_auto && gtk_css_parser_try_ident (parser, "auto"))
        continue;

      if (!gtk_css_number_value_can_parse (parser))
        break;

      result->values[i] = _gtk_css_number_value_parse (parser, flags);
      if (result->values[i] == NULL)
        {
          _gtk_css_value_unref (result);
          return NULL;
        }
    }

  if (i == 0)
    {
      gtk_css_parser_error_syntax (parser, "Expected a number");
      _gtk_css_value_unref (result);
      return NULL;
    }

  if (allow_fill && !result->fill)
    result->fill = gtk_css_parser_try_ident (parser, "fill");

  for (; i < 4; i++)
    {
      if (result->values[(i - 1) >> 1])
        result->values[i] = _gtk_css_value_ref (result->values[(i - 1) >> 1]);
    }

  result->is_computed = TRUE;
  for (i = 0; i < 4; i++)
    if (result->values[i] && !gtk_css_value_is_computed (result->values[i]))
      {
        result->is_computed = FALSE;
        break;
      }

  return result;
}

GtkCssValue *
_gtk_css_border_value_get_top (const GtkCssValue *value)
{
  g_return_val_if_fail (value->class == &GTK_CSS_VALUE_BORDER, NULL);

  return value->values[GTK_CSS_TOP];
}

GtkCssValue *
_gtk_css_border_value_get_right (const GtkCssValue *value)
{
  g_return_val_if_fail (value->class == &GTK_CSS_VALUE_BORDER, NULL);

  return value->values[GTK_CSS_RIGHT];
}

GtkCssValue *
_gtk_css_border_value_get_bottom (const GtkCssValue *value)
{
  g_return_val_if_fail (value->class == &GTK_CSS_VALUE_BORDER, NULL);

  return value->values[GTK_CSS_BOTTOM];
}

GtkCssValue *
_gtk_css_border_value_get_left (const GtkCssValue *value)
{
  g_return_val_if_fail (value->class == &GTK_CSS_VALUE_BORDER, NULL);

  return value->values[GTK_CSS_LEFT];
}

