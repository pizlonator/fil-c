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

#include "gtkcssarrayvalueprivate.h"
#include "gtkcssimagevalueprivate.h"
#include "gtkcssstylepropertyprivate.h"

#include <string.h>

struct _GtkCssValue {
  GTK_CSS_VALUE_BASE
  guint         n_values;
  GtkCssValue  *values[1];
};

static void
gtk_css_value_array_free (GtkCssValue *value)
{
  guint i;

  for (i = 0; i < value->n_values; i++)
    _gtk_css_value_unref (value->values[i]);

  g_free (value);
}

static GtkCssValue *
gtk_css_value_array_compute (GtkCssValue      *value,
                             guint             property_id,
                             GtkStyleProvider *provider,
                             GtkCssStyle      *style,
                             GtkCssStyle      *parent_style)
{
  GtkCssValue *result;
  GtkCssValue *i_value;
  guint i, j;

  result = NULL;
  for (i = 0; i < value->n_values; i++)
    {
      i_value =  _gtk_css_value_compute (value->values[i], property_id, provider, style, parent_style);

      if (result == NULL &&
	  i_value != value->values[i])
	{
	  result = _gtk_css_array_value_new_from_array (value->values, value->n_values);
	  for (j = 0; j < i; j++)
	    _gtk_css_value_ref (result->values[j]);
	}

      if (result != NULL)
	result->values[i] = i_value;
      else
	_gtk_css_value_unref (i_value);
    }

  if (result == NULL)
    return _gtk_css_value_ref (value);

  return result;
}

static gboolean
gtk_css_value_array_equal (const GtkCssValue *value1,
                           const GtkCssValue *value2)
{
  guint i;

  if (value1->n_values != value2->n_values)
    return FALSE;

  for (i = 0; i < value1->n_values; i++)
    {
      if (!_gtk_css_value_equal (value1->values[i],
                                 value2->values[i]))
        return FALSE;
    }

  return TRUE;
}

static inline guint
gcd (guint a, guint b)
{
  while (b != 0)
    {
      guint t = b;
      b = a % b;
      a = t;
    }
  return a;
}

static inline guint
lcm (guint a, guint b)
{
  return a / gcd (a, b) * b;
}

static GtkCssValue *
gtk_css_value_array_transition_repeat (GtkCssValue *start,
                                       GtkCssValue *end,
                                       guint        property_id,
                                       double       progress)
{
  GtkCssValue **transitions;
  guint i, n;

  n = lcm (start->n_values, end->n_values);
  transitions = g_newa (GtkCssValue *, n);

  for (i = 0; i < n; i++)
    {
      transitions[i] = _gtk_css_value_transition (start->values[i % start->n_values],
                                                  end->values[i % end->n_values],
                                                  property_id,
                                                  progress);
      if (transitions[i] == NULL)
        {
          while (i--)
            _gtk_css_value_unref (transitions[i]);
          return NULL;
        }
    }

  return _gtk_css_array_value_new_from_array (transitions, n);
}

static GtkCssValue *
gtk_css_array_value_create_default_transition_value (guint property_id)
{
  switch (property_id)
    {
    case GTK_CSS_PROPERTY_BACKGROUND_IMAGE:
      return _gtk_css_image_value_new (NULL);
    default:
      g_return_val_if_reached (NULL);
    }
}

static GtkCssValue *
gtk_css_value_array_transition_extend (GtkCssValue *start,
                                       GtkCssValue *end,
                                       guint        property_id,
                                       double       progress)
{
  GtkCssValue **transitions;
  guint i, n;

  n = MAX (start->n_values, end->n_values);
  transitions = g_newa (GtkCssValue *, n);

  for (i = 0; i < MIN (start->n_values, end->n_values); i++)
    {
      transitions[i] = _gtk_css_value_transition (start->values[i],
                                                  end->values[i],
                                                  property_id,
                                                  progress);
      if (transitions[i] == NULL)
        {
          while (i--)
            _gtk_css_value_unref (transitions[i]);
          return NULL;
        }
    }

  if (start->n_values != end->n_values)
    {
      GtkCssValue *default_value;

      default_value = gtk_css_array_value_create_default_transition_value (property_id);

      for (; i < start->n_values; i++)
        {
          transitions[i] = _gtk_css_value_transition (start->values[i],
                                                      default_value,
                                                      property_id,
                                                      progress);
          if (transitions[i] == NULL)
            {
              while (i--)
                _gtk_css_value_unref (transitions[i]);
              return NULL;
            }
        }

      for (; i < end->n_values; i++)
        {
          transitions[i] = _gtk_css_value_transition (default_value,
                                                      end->values[i],
                                                      property_id,
                                                      progress);
          if (transitions[i] == NULL)
            {
              while (i--)
                _gtk_css_value_unref (transitions[i]);
              return NULL;
            }
        }

    }

  g_assert (i == n);

  return _gtk_css_array_value_new_from_array (transitions, n);
}

static GtkCssValue *
gtk_css_value_array_transition (GtkCssValue *start,
                                GtkCssValue *end,
                                guint        property_id,
                                double       progress)
{
  switch (property_id)
    {
    case GTK_CSS_PROPERTY_BACKGROUND_CLIP:
    case GTK_CSS_PROPERTY_BACKGROUND_ORIGIN:
    case GTK_CSS_PROPERTY_BACKGROUND_SIZE:
    case GTK_CSS_PROPERTY_BACKGROUND_POSITION:
    case GTK_CSS_PROPERTY_BACKGROUND_REPEAT:
      return gtk_css_value_array_transition_repeat (start, end, property_id, progress);
    case GTK_CSS_PROPERTY_BACKGROUND_IMAGE:
      return gtk_css_value_array_transition_extend (start, end, property_id, progress);
    case GTK_CSS_PROPERTY_COLOR:
    case GTK_CSS_PROPERTY_FONT_SIZE:
    case GTK_CSS_PROPERTY_BACKGROUND_COLOR:
    case GTK_CSS_PROPERTY_FONT_FAMILY:
    case GTK_CSS_PROPERTY_FONT_STYLE:
    case GTK_CSS_PROPERTY_FONT_WEIGHT:
    case GTK_CSS_PROPERTY_TEXT_SHADOW:
    case GTK_CSS_PROPERTY_ICON_SHADOW:
    case GTK_CSS_PROPERTY_BOX_SHADOW:
    case GTK_CSS_PROPERTY_MARGIN_TOP:
    case GTK_CSS_PROPERTY_MARGIN_LEFT:
    case GTK_CSS_PROPERTY_MARGIN_BOTTOM:
    case GTK_CSS_PROPERTY_MARGIN_RIGHT:
    case GTK_CSS_PROPERTY_PADDING_TOP:
    case GTK_CSS_PROPERTY_PADDING_LEFT:
    case GTK_CSS_PROPERTY_PADDING_BOTTOM:
    case GTK_CSS_PROPERTY_PADDING_RIGHT:
    case GTK_CSS_PROPERTY_BORDER_TOP_STYLE:
    case GTK_CSS_PROPERTY_BORDER_TOP_WIDTH:
    case GTK_CSS_PROPERTY_BORDER_LEFT_STYLE:
    case GTK_CSS_PROPERTY_BORDER_LEFT_WIDTH:
    case GTK_CSS_PROPERTY_BORDER_BOTTOM_STYLE:
    case GTK_CSS_PROPERTY_BORDER_BOTTOM_WIDTH:
    case GTK_CSS_PROPERTY_BORDER_RIGHT_STYLE:
    case GTK_CSS_PROPERTY_BORDER_RIGHT_WIDTH:
    case GTK_CSS_PROPERTY_BORDER_TOP_LEFT_RADIUS:
    case GTK_CSS_PROPERTY_BORDER_TOP_RIGHT_RADIUS:
    case GTK_CSS_PROPERTY_BORDER_BOTTOM_RIGHT_RADIUS:
    case GTK_CSS_PROPERTY_BORDER_BOTTOM_LEFT_RADIUS:
    case GTK_CSS_PROPERTY_OUTLINE_STYLE:
    case GTK_CSS_PROPERTY_OUTLINE_WIDTH:
    case GTK_CSS_PROPERTY_OUTLINE_OFFSET:
    case GTK_CSS_PROPERTY_BORDER_TOP_COLOR:
    case GTK_CSS_PROPERTY_BORDER_RIGHT_COLOR:
    case GTK_CSS_PROPERTY_BORDER_BOTTOM_COLOR:
    case GTK_CSS_PROPERTY_BORDER_LEFT_COLOR:
    case GTK_CSS_PROPERTY_OUTLINE_COLOR:
    case GTK_CSS_PROPERTY_BORDER_IMAGE_SOURCE:
    case GTK_CSS_PROPERTY_BORDER_IMAGE_REPEAT:
    case GTK_CSS_PROPERTY_BORDER_IMAGE_SLICE:
    case GTK_CSS_PROPERTY_BORDER_IMAGE_WIDTH:
    default:
      /* keep all values that are not arrays here, so we get a warning if we ever turn them
       * into arrays and start animating them. */
      g_warning ("Don't know how to transition arrays for property '%s'", 
                 _gtk_style_property_get_name (GTK_STYLE_PROPERTY (_gtk_css_style_property_lookup_by_id (property_id))));
      return NULL;
    case GTK_CSS_PROPERTY_TRANSITION_PROPERTY:
    case GTK_CSS_PROPERTY_TRANSITION_DURATION:
    case GTK_CSS_PROPERTY_TRANSITION_TIMING_FUNCTION:
    case GTK_CSS_PROPERTY_TRANSITION_DELAY:
      return NULL;
    }
}

static gboolean
gtk_css_value_array_is_dynamic (const GtkCssValue *value)
{
  guint i;

  for (i = 0; i < value->n_values; i++)
    {
      if (gtk_css_value_is_dynamic (value->values[i]))
        return TRUE;
    }

  return FALSE;
}

static GtkCssValue *
gtk_css_value_array_get_dynamic_value (GtkCssValue *value,
                                       gint64       monotonic_time)
{
  GtkCssValue *result;
  GtkCssValue *i_value;
  guint i, j;

  if (!gtk_css_value_is_dynamic (value))
    return gtk_css_value_ref (value);

  result = NULL;
  for (i = 0; i < value->n_values; i++)
    {
      i_value = gtk_css_value_get_dynamic_value (value->values[i], monotonic_time);

      if (result == NULL &&
	  i_value != value->values[i])
	{
	  result = _gtk_css_array_value_new_from_array (value->values, value->n_values);
	  for (j = 0; j < i; j++)
	    _gtk_css_value_ref (result->values[j]);
	}

      if (result != NULL)
	result->values[i] = i_value;
      else
	_gtk_css_value_unref (i_value);
    }

  if (result == NULL)
    return _gtk_css_value_ref (value);

  return result;
}

static void
gtk_css_value_array_print (const GtkCssValue *value,
                           GString           *string)
{
  guint i;

  if (value->n_values == 0)
    {
      g_string_append (string, "none");
      return;
    }

  for (i = 0; i < value->n_values; i++)
    {
      if (i > 0)
        g_string_append (string, ", ");
      _gtk_css_value_print (value->values[i], string);
    }
}

static const GtkCssValueClass GTK_CSS_VALUE_ARRAY = {
  "GtkCssArrayValue",
  gtk_css_value_array_free,
  gtk_css_value_array_compute,
  gtk_css_value_array_equal,
  gtk_css_value_array_transition,
  gtk_css_value_array_is_dynamic,
  gtk_css_value_array_get_dynamic_value,
  gtk_css_value_array_print
};

GtkCssValue *
_gtk_css_array_value_new (GtkCssValue *content)
{
  g_return_val_if_fail (content != NULL, NULL);

  return _gtk_css_array_value_new_from_array (&content, 1);
}

GtkCssValue *
_gtk_css_array_value_new_from_array (GtkCssValue **values,
                                     guint         n_values)
{
  GtkCssValue *result;
  guint i;

  g_return_val_if_fail (values != NULL, NULL);
  g_return_val_if_fail (n_values > 0, NULL);

  if (n_values == 1)
    return values[0];

  result = _gtk_css_value_alloc (&GTK_CSS_VALUE_ARRAY, sizeof (GtkCssValue) + sizeof (GtkCssValue *) * (n_values - 1));
  result->n_values = n_values;
  memcpy (&result->values[0], values, sizeof (GtkCssValue *) * n_values);

  result->is_computed = TRUE;
  for (i = 0; i < n_values; i ++)
    {
      if (!gtk_css_value_is_computed (values[i]))
        {
          result->is_computed = FALSE;
          break;
        }
    }

  return result;
}

GtkCssValue *
_gtk_css_array_value_parse (GtkCssParser *parser,
                            GtkCssValue  *(* parse_func) (GtkCssParser *parser))
{
  GtkCssValue *value, *result;
  GtkCssValue *values[128];
  guint n_values = 0;
  guint i;

  do {
    value = parse_func (parser);

    if (value == NULL)
      {
        for (i = 0; i < n_values; i ++)
          _gtk_css_value_unref (values[i]);

        return NULL;
      }

    values[n_values] = value;
    n_values ++;
    if (G_UNLIKELY (n_values > G_N_ELEMENTS (values)))
      g_error ("Only %d elements in a css array are allowed", (int)G_N_ELEMENTS (values));
  } while (gtk_css_parser_try_token (parser, GTK_CSS_TOKEN_COMMA));

  result = _gtk_css_array_value_new_from_array (values, n_values);
  return result;
}

GtkCssValue *
_gtk_css_array_value_get_nth (GtkCssValue *value,
                              guint        i)
{
  if (value->class != &GTK_CSS_VALUE_ARRAY)
      return value;

  g_return_val_if_fail (value != NULL, NULL);
  g_return_val_if_fail (value->class == &GTK_CSS_VALUE_ARRAY, NULL);
  g_return_val_if_fail (value->n_values > 0, NULL);

  return value->values[i % value->n_values];
}

guint
_gtk_css_array_value_get_n_values (const GtkCssValue *value)
{
  if (value->class != &GTK_CSS_VALUE_ARRAY)
    return 1;

  g_return_val_if_fail (value != NULL, 0);
  g_return_val_if_fail (value->class == &GTK_CSS_VALUE_ARRAY, 0);

  return value->n_values;
}

