/* GTK - The GIMP Toolkit
 * Copyright (C) 2016 Benjamin Otte <otte@gnome.org>
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

#include "gtkcssnumbervalueprivate.h"

#include "gtkcsscalcvalueprivate.h"
#include "gtkcssenumvalueprivate.h"
#include "gtkcssdimensionvalueprivate.h"
#include "gtkcssstyleprivate.h"
#include "gtkprivate.h"

static GtkCssValue *        gtk_css_calc_value_new         (guint n_terms);
static GtkCssValue *        gtk_css_calc_value_new_sum     (GtkCssValue *a,
                                                            GtkCssValue *b);
static gsize                gtk_css_value_calc_get_size    (gsize n_terms);

enum {
  TYPE_CALC = 0,
  TYPE_DIMENSION = 1,
};

struct _GtkCssValue {
  GTK_CSS_VALUE_BASE
  guint type : 1; /* Calc or dimension */
  union {
    struct {
      GtkCssUnit unit;
      double value;
    } dimension;
    struct {
      guint n_terms;
      GtkCssValue *terms[1];
    } calc;
  };
};


static GtkCssValue *
gtk_css_calc_value_new_from_array (GtkCssValue **values,
                                   guint         n_values)
{
  GtkCssValue *result;

  if (n_values > 1)
    {
      result = gtk_css_calc_value_new (n_values);
      memcpy (result->calc.terms, values, n_values * sizeof (GtkCssValue *));
    }
  else
    {
      result = values[0];
    }

  return result;
}

static void
gtk_css_value_number_free (GtkCssValue *number)
{
  if (number->type == TYPE_CALC)
    {
      const guint n_terms = number->calc.n_terms;

      for (guint i = 0; i < n_terms; i++)
        _gtk_css_value_unref (number->calc.terms[i]);
    }

  g_free (number);
}

static double
get_dpi (GtkCssStyle *style)
{
  return _gtk_css_number_value_get (style->core->dpi, 96);
}

static double
get_base_font_size_px (guint             property_id,
                       GtkStyleProvider *provider,
                       GtkCssStyle      *style,
                       GtkCssStyle      *parent_style)
{
  if (property_id == GTK_CSS_PROPERTY_FONT_SIZE)
    {
      if (parent_style)
        return _gtk_css_number_value_get (parent_style->core->font_size, 100);
      else
        return gtk_css_font_size_get_default_px (provider, style);
    }

  return _gtk_css_number_value_get (style->core->font_size, 100);
}

static GtkCssValue *
gtk_css_value_number_compute (GtkCssValue      *number,
                              guint             property_id,
                              GtkStyleProvider *provider,
                              GtkCssStyle      *style,
                              GtkCssStyle      *parent_style)
{
  double value;

  if (G_UNLIKELY (number->type == TYPE_CALC))
    {
      const guint n_terms = number->calc.n_terms;
      GtkCssValue *result;
      gboolean changed = FALSE;
      gsize i;
      GtkCssValue **new_values;

      new_values = g_alloca (sizeof (GtkCssValue *) * n_terms);

      for (i = 0; i < n_terms; i++)
        {
          GtkCssValue *computed = _gtk_css_value_compute (number->calc.terms[i],
                                                          property_id, provider, style,
                                                          parent_style);
          changed |= computed != number->calc.terms[i];
          new_values[i] = computed;
        }

      if (changed)
        {
          result = gtk_css_calc_value_new_from_array (new_values, n_terms);
        }
      else
        {
          for (i = 0; i < n_terms; i++)
            gtk_css_value_unref (new_values[i]);

          result = _gtk_css_value_ref (number);
        }

      return result;
    }

  g_assert (number->type == TYPE_DIMENSION);

  value = number->dimension.value;
  switch (number->dimension.unit)
    {
    case GTK_CSS_PERCENT:
      /* percentages for font sizes are computed, other percentages aren't */
      if (property_id == GTK_CSS_PROPERTY_FONT_SIZE)
        return gtk_css_dimension_value_new (value / 100.0 *
                                            get_base_font_size_px (property_id, provider, style, parent_style),
                                            GTK_CSS_PX);
      G_GNUC_FALLTHROUGH;
    case GTK_CSS_NUMBER:
    case GTK_CSS_PX:
    case GTK_CSS_DEG:
    case GTK_CSS_S:
      return _gtk_css_value_ref (number);
    case GTK_CSS_PT:
      return gtk_css_dimension_value_new (value * get_dpi (style) / 72.0,
                                          GTK_CSS_PX);
    case GTK_CSS_PC:
      return gtk_css_dimension_value_new (value * get_dpi (style) / 72.0 * 12.0,
                                          GTK_CSS_PX);
    case GTK_CSS_IN:
      return gtk_css_dimension_value_new (value * get_dpi (style),
                                          GTK_CSS_PX);
    case GTK_CSS_CM:
      return gtk_css_dimension_value_new (value * get_dpi (style) * 0.39370078740157477,
                                          GTK_CSS_PX);
    case GTK_CSS_MM:
      return gtk_css_dimension_value_new (value * get_dpi (style) * 0.039370078740157477,
                                          GTK_CSS_PX);
    case GTK_CSS_EM:
      return gtk_css_dimension_value_new (value *
                                          get_base_font_size_px (property_id, provider, style, parent_style),
                                          GTK_CSS_PX);
    case GTK_CSS_EX:
      /* for now we pretend ex is half of em */
      return gtk_css_dimension_value_new (value * 0.5 *
                                          get_base_font_size_px (property_id, provider, style, parent_style),
                                          GTK_CSS_PX);
    case GTK_CSS_REM:
      return gtk_css_dimension_value_new (value *
                                          gtk_css_font_size_get_default_px (provider, style),
                                          GTK_CSS_PX);
    case GTK_CSS_RAD:
      return gtk_css_dimension_value_new (value * 360.0 / (2 * G_PI),
                                          GTK_CSS_DEG);
    case GTK_CSS_GRAD:
      return gtk_css_dimension_value_new (value * 360.0 / 400.0,
                                          GTK_CSS_DEG);
    case GTK_CSS_TURN:
      return gtk_css_dimension_value_new (value * 360.0,
                                          GTK_CSS_DEG);
    case GTK_CSS_MS:
      return gtk_css_dimension_value_new (value / 1000.0,
                                          GTK_CSS_S);
    default:
      g_assert_not_reached();
    }
}

static gboolean
gtk_css_value_number_equal (const GtkCssValue *val1,
                            const GtkCssValue *val2)
{
  guint i;

  if (val1->type != val2->type)
    return FALSE;

  if (G_LIKELY (val1->type == TYPE_DIMENSION))
    {
      return val1->dimension.unit == val2->dimension.unit &&
             val1->dimension.value == val2->dimension.value;
    }

  g_assert (val1->type == TYPE_CALC);

  if (val1->calc.n_terms != val2->calc.n_terms)
    return FALSE;

  for (i = 0; i < val1->calc.n_terms; i++)
    {
      if (!_gtk_css_value_equal (val1->calc.terms[i], val2->calc.terms[i]))
        return FALSE;
    }

  return TRUE;
}

static void
gtk_css_value_number_print (const GtkCssValue *value,
                            GString           *string)
{
  guint i;

  if (G_LIKELY (value->type == TYPE_DIMENSION))
    {
      const char *names[] = {
        /* [GTK_CSS_NUMBER] = */ "",
        /* [GTK_CSS_PERCENT] = */ "%",
        /* [GTK_CSS_PX] = */ "px",
        /* [GTK_CSS_PT] = */ "pt",
        /* [GTK_CSS_EM] = */ "em",
        /* [GTK_CSS_EX] = */ "ex",
        /* [GTK_CSS_REM] = */ "rem",
        /* [GTK_CSS_PC] = */ "pc",
        /* [GTK_CSS_IN] = */ "in",
        /* [GTK_CSS_CM] = */ "cm",
        /* [GTK_CSS_MM] = */ "mm",
        /* [GTK_CSS_RAD] = */ "rad",
        /* [GTK_CSS_DEG] = */ "deg",
        /* [GTK_CSS_GRAD] = */ "grad",
        /* [GTK_CSS_TURN] = */ "turn",
        /* [GTK_CSS_S] = */ "s",
        /* [GTK_CSS_MS] = */ "ms",
      };
      char buf[G_ASCII_DTOSTR_BUF_SIZE];

      if (isinf (value->dimension.value))
        g_string_append (string, "infinite");
      else
        {
          g_ascii_dtostr (buf, sizeof (buf), value->dimension.value);
          g_string_append (string, buf);
          if (value->dimension.value != 0.0)
            g_string_append (string, names[value->dimension.unit]);
        }

      return;
    }

  g_assert (value->type == TYPE_CALC);

  g_string_append (string, "calc(");
  _gtk_css_value_print (value->calc.terms[0], string);

  for (i = 1; i < value->calc.n_terms; i++)
    {
      g_string_append (string, " + ");
      _gtk_css_value_print (value->calc.terms[i], string);
    }
  g_string_append (string, ")");
}

static GtkCssValue *
gtk_css_value_number_transition (GtkCssValue *start,
                                 GtkCssValue *end,
                                 guint        property_id,
                                 double       progress)
{
  GtkCssValue *result, *mul_start, *mul_end;

  if (start == end)
    return _gtk_css_value_ref (start);

  if (G_LIKELY (start->type == TYPE_DIMENSION && end->type == TYPE_DIMENSION))
    {
      if (start->dimension.unit == end->dimension.unit)
        {
          const double start_value = start->dimension.value;
          const double end_value = end->dimension.value;
          return gtk_css_dimension_value_new (start_value + (end_value - start_value) * progress,
                                              start->dimension.unit);
        }
    }

  mul_start = gtk_css_number_value_multiply (start, 1 - progress);
  mul_end = gtk_css_number_value_multiply (end, progress);

  result = gtk_css_number_value_add (mul_start, mul_end);

  _gtk_css_value_unref (mul_start);
  _gtk_css_value_unref (mul_end);

  return result;
}

static const GtkCssValueClass GTK_CSS_VALUE_NUMBER = {
  "GtkCssNumberValue",
  gtk_css_value_number_free,
  gtk_css_value_number_compute,
  gtk_css_value_number_equal,
  gtk_css_value_number_transition,
  NULL,
  NULL,
  gtk_css_value_number_print
};

static gsize
gtk_css_value_calc_get_size (gsize n_terms)
{
  g_assert (n_terms > 0);

  return sizeof (GtkCssValue) + sizeof (GtkCssValue *) * (n_terms - 1);
}

static GtkCssValue *
gtk_css_calc_value_new (guint n_terms)
{
  GtkCssValue *result;

  result = _gtk_css_value_alloc (&GTK_CSS_VALUE_NUMBER,
                                 gtk_css_value_calc_get_size (n_terms));
  result->type = TYPE_CALC;
  result->calc.n_terms = n_terms;

  return result;
}

GtkCssValue *
gtk_css_dimension_value_new (double     value,
                             GtkCssUnit unit)
{
  static GtkCssValue number_singletons[] = {
    { &GTK_CSS_VALUE_NUMBER, 1, TRUE, TYPE_DIMENSION, {{ GTK_CSS_NUMBER, 0 }} },
    { &GTK_CSS_VALUE_NUMBER, 1, TRUE, TYPE_DIMENSION, {{ GTK_CSS_NUMBER, 1 }} },
    { &GTK_CSS_VALUE_NUMBER, 1, TRUE, TYPE_DIMENSION, {{ GTK_CSS_NUMBER, 96 }} }, /* DPI default */
  };
  static GtkCssValue px_singletons[] = {
    { &GTK_CSS_VALUE_NUMBER, 1, TRUE, TYPE_DIMENSION, {{ GTK_CSS_PX, 0 }} },
    { &GTK_CSS_VALUE_NUMBER, 1, TRUE, TYPE_DIMENSION, {{ GTK_CSS_PX, 1 }} },
    { &GTK_CSS_VALUE_NUMBER, 1, TRUE, TYPE_DIMENSION, {{ GTK_CSS_PX, 2 }} },
    { &GTK_CSS_VALUE_NUMBER, 1, TRUE, TYPE_DIMENSION, {{ GTK_CSS_PX, 3 }} },
    { &GTK_CSS_VALUE_NUMBER, 1, TRUE, TYPE_DIMENSION, {{ GTK_CSS_PX, 4 }} },
    { &GTK_CSS_VALUE_NUMBER, 1, TRUE, TYPE_DIMENSION, {{ GTK_CSS_PX, 5 }} },
    { &GTK_CSS_VALUE_NUMBER, 1, TRUE, TYPE_DIMENSION, {{ GTK_CSS_PX, 6 }} },
    { &GTK_CSS_VALUE_NUMBER, 1, TRUE, TYPE_DIMENSION, {{ GTK_CSS_PX, 7 }} },
    { &GTK_CSS_VALUE_NUMBER, 1, TRUE, TYPE_DIMENSION, {{ GTK_CSS_PX, 8 }} },
    { &GTK_CSS_VALUE_NUMBER, 1, TRUE, TYPE_DIMENSION, {{ GTK_CSS_PX, 16 }} }, /* Icon size default */
    { &GTK_CSS_VALUE_NUMBER, 1, TRUE, TYPE_DIMENSION, {{ GTK_CSS_PX, 32 }} },
    { &GTK_CSS_VALUE_NUMBER, 1, TRUE, TYPE_DIMENSION, {{ GTK_CSS_PX, 64 }} },
  };
  static GtkCssValue percent_singletons[] = {
    { &GTK_CSS_VALUE_NUMBER, 1, TRUE,  TYPE_DIMENSION, {{ GTK_CSS_PERCENT, 0 }} },
    { &GTK_CSS_VALUE_NUMBER, 1, FALSE, TYPE_DIMENSION, {{ GTK_CSS_PERCENT, 50 }} },
    { &GTK_CSS_VALUE_NUMBER, 1, FALSE, TYPE_DIMENSION, {{ GTK_CSS_PERCENT, 100 }} },
  };
  static GtkCssValue second_singletons[] = {
    { &GTK_CSS_VALUE_NUMBER, 1, TRUE, TYPE_DIMENSION, {{ GTK_CSS_S, 0 }} },
    { &GTK_CSS_VALUE_NUMBER, 1, TRUE, TYPE_DIMENSION, {{ GTK_CSS_S, 1 }} },
  };
  static GtkCssValue deg_singletons[] = {
    { &GTK_CSS_VALUE_NUMBER, 1, TRUE, TYPE_DIMENSION, {{ GTK_CSS_DEG, 0 }} },
    { &GTK_CSS_VALUE_NUMBER, 1, TRUE, TYPE_DIMENSION, {{ GTK_CSS_DEG, 90 }} },
    { &GTK_CSS_VALUE_NUMBER, 1, TRUE, TYPE_DIMENSION, {{ GTK_CSS_DEG, 180 }} },
    { &GTK_CSS_VALUE_NUMBER, 1, TRUE, TYPE_DIMENSION, {{ GTK_CSS_DEG, 270 }} },
  };
  GtkCssValue *result;

  switch ((guint)unit)
    {
    case GTK_CSS_NUMBER:
      if (value == 0 || value == 1)
        return _gtk_css_value_ref (&number_singletons[(int) value]);

      if (value == 96)
        return _gtk_css_value_ref (&number_singletons[2]);

      break;

    case GTK_CSS_PX:
      if (value == 0 ||
          value == 1 ||
          value == 2 ||
          value == 3 ||
          value == 4 ||
          value == 5 ||
          value == 6 ||
          value == 7 ||
          value == 8)
        return _gtk_css_value_ref (&px_singletons[(int) value]);
      if (value == 16)
        return _gtk_css_value_ref (&px_singletons[9]);
      if (value == 32)
        return _gtk_css_value_ref (&px_singletons[10]);
      if (value == 64)
        return _gtk_css_value_ref (&px_singletons[11]);

      break;

    case GTK_CSS_PERCENT:
      if (value == 0)
        return _gtk_css_value_ref (&percent_singletons[0]);
      if (value == 50)
        return _gtk_css_value_ref (&percent_singletons[1]);
      if (value == 100)
        return _gtk_css_value_ref (&percent_singletons[2]);

      break;

    case GTK_CSS_S:
      if (value == 0 || value == 1)
        return _gtk_css_value_ref (&second_singletons[(int)value]);

      break;

    case GTK_CSS_DEG:
      if (value == 0)
        return _gtk_css_value_ref (&deg_singletons[0]);
      if (value == 90)
        return _gtk_css_value_ref (&deg_singletons[1]);
      if (value == 180)
        return _gtk_css_value_ref (&deg_singletons[2]);
      if (value == 270)
        return _gtk_css_value_ref (&deg_singletons[3]);

      break;

    default:
      ;
    }

  result = _gtk_css_value_new (GtkCssValue, &GTK_CSS_VALUE_NUMBER);
  result->type = TYPE_DIMENSION;
  result->dimension.unit = unit;
  result->dimension.value = value;
  result->is_computed = value == 0 ||
                        unit == GTK_CSS_NUMBER ||
                        unit == GTK_CSS_PX ||
                        unit == GTK_CSS_DEG ||
                        unit == GTK_CSS_S;
  return result;
}

/*
 * gtk_css_number_value_get_calc_term_order:
 * @value: Value to compute order for
 *
 * Determines the position of @value when printed as part of a calc()
 * expression. Values with lower numbers are printed first. Note that
 * these numbers are arbitrary, so when adding new types of values to
 * print, feel free to change them in implementations so that they
 * match.
 *
 * Returns: Magic value determining placement when printing calc()
 *   expression.
 */
static int
gtk_css_number_value_get_calc_term_order (const GtkCssValue *value)
{
  if (G_LIKELY (value->type == TYPE_DIMENSION))
    {
      /* note: the order is alphabetic */
      static const guint order_per_unit[] = {
        /* [GTK_CSS_NUMBER] = */ 0,
        /* [GTK_CSS_PERCENT] = */ 16,
        /* [GTK_CSS_PX] = */ 11,
        /* [GTK_CSS_PT] = */ 10,
        /* [GTK_CSS_EM] = */ 3,
        /* [GTK_CSS_EX] = */ 4,
        /* [GTK_CSS_REM] = */ 13,
        /* [GTK_CSS_PC] = */ 9,
        /* [GTK_CSS_IN] = */ 6,
        /* [GTK_CSS_CM] = */ 1,
        /* [GTK_CSS_MM] = */ 7,
        /* [GTK_CSS_RAD] = */ 12,
        /* [GTK_CSS_DEG] = */ 2,
        /* [GTK_CSS_GRAD] = */ 5,
        /* [GTK_CSS_TURN] = */ 15,
        /* [GTK_CSS_S] = */ 14,
        /* [GTK_CSS_MS] = */ 8
      };
      return 1000 + order_per_unit[value->dimension.unit];
    }

  g_assert (value->type == TYPE_CALC);

  /* This should never be needed because calc() can't contain calc(),
   * but eh...
   */
  return 0;
}

static void
gtk_css_calc_array_add (GPtrArray *array, GtkCssValue *value)
{
  gsize i;
  int calc_term_order;

  calc_term_order = gtk_css_number_value_get_calc_term_order (value);

  for (i = 0; i < array->len; i++)
    {
      GtkCssValue *sum = gtk_css_number_value_try_add (g_ptr_array_index (array, i), value);

      if (sum)
        {
          g_ptr_array_index (array, i) = sum;
          _gtk_css_value_unref (value);
          return;
        }
      else if (gtk_css_number_value_get_calc_term_order (g_ptr_array_index (array, i)) > calc_term_order)
        {
          g_ptr_array_insert (array, i, value);
          return;
        }
    }

  g_ptr_array_add (array, value);
}

static GtkCssValue *
gtk_css_calc_value_new_sum (GtkCssValue *value1,
                            GtkCssValue *value2)
{
  GPtrArray *array;
  GtkCssValue *result;
  gsize i;

  array = g_ptr_array_new ();

  if (value1->class == &GTK_CSS_VALUE_NUMBER &&
      value1->type == TYPE_CALC)
    {
      for (i = 0; i < value1->calc.n_terms; i++)
        {
          gtk_css_calc_array_add (array, _gtk_css_value_ref (value1->calc.terms[i]));
        }
    }
  else
    {
      gtk_css_calc_array_add (array, _gtk_css_value_ref (value1));
    }

  if (value2->class == &GTK_CSS_VALUE_NUMBER &&
      value2->type == TYPE_CALC)
    {
      for (i = 0; i < value2->calc.n_terms; i++)
        {
          gtk_css_calc_array_add (array, _gtk_css_value_ref (value2->calc.terms[i]));
        }
    }
  else
    {
      gtk_css_calc_array_add (array, _gtk_css_value_ref (value2));
    }

  result = gtk_css_calc_value_new_from_array ((GtkCssValue **)array->pdata, array->len);
  g_ptr_array_free (array, TRUE);

  return result;
}

GtkCssDimension
gtk_css_number_value_get_dimension (const GtkCssValue *value)
{
  GtkCssDimension dimension = GTK_CSS_DIMENSION_PERCENTAGE;
  guint i;

  if (G_LIKELY (value->type == TYPE_DIMENSION))
    return gtk_css_unit_get_dimension (value->dimension.unit);

  g_assert (value->type == TYPE_CALC);

  for (i = 0; i < value->calc.n_terms && dimension == GTK_CSS_DIMENSION_PERCENTAGE; i++)
    {
      dimension = gtk_css_number_value_get_dimension (value->calc.terms[i]);
      if (dimension != GTK_CSS_DIMENSION_PERCENTAGE)
        break;
    }

  return dimension;
}

gboolean
gtk_css_number_value_has_percent (const GtkCssValue *value)
{
  guint i;

  if (G_LIKELY (value->type == TYPE_DIMENSION))
    {
      return gtk_css_unit_get_dimension (value->dimension.unit) == GTK_CSS_DIMENSION_PERCENTAGE;
    }

  for (i = 0; i < value->calc.n_terms; i++)
    {
      if (gtk_css_number_value_has_percent (value->calc.terms[i]))
        return TRUE;
    }

  return FALSE;
}

GtkCssValue *
gtk_css_number_value_multiply (GtkCssValue *value,
                               double       factor)
{
  GtkCssValue *result;
  guint i;

  if (factor == 1)
    return _gtk_css_value_ref (value);

  if (G_LIKELY (value->type == TYPE_DIMENSION))
    {
      return gtk_css_dimension_value_new (value->dimension.value * factor,
                                          value->dimension.unit);
    }

  g_assert (value->type == TYPE_CALC);

  result = gtk_css_calc_value_new (value->calc.n_terms);
  for (i = 0; i < value->calc.n_terms; i++)
    result->calc.terms[i] = gtk_css_number_value_multiply (value->calc.terms[i], factor);

  return result;
}

GtkCssValue *
gtk_css_number_value_add (GtkCssValue *value1,
                          GtkCssValue *value2)
{
  GtkCssValue *sum;

  sum = gtk_css_number_value_try_add (value1, value2);
  if (sum == NULL)
    sum = gtk_css_calc_value_new_sum (value1, value2);

  return sum;
}

GtkCssValue *
gtk_css_number_value_try_add (GtkCssValue *value1,
                              GtkCssValue *value2)
{
  if (G_UNLIKELY (value1->type != value2->type))
    return NULL;

  if (G_LIKELY (value1->type == TYPE_DIMENSION))
    {
      if (value1->dimension.unit != value2->dimension.unit)
        return NULL;

      if (value1->dimension.value == 0)
        return _gtk_css_value_ref (value2);

      if (value2->dimension.value == 0)
        return _gtk_css_value_ref (value1);

      return gtk_css_dimension_value_new (value1->dimension.value + value2->dimension.value,
                                          value1->dimension.unit);
    }

  g_assert (value1->type == TYPE_CALC);
  /* Not possible for calc() values */
  return NULL;
}

GtkCssValue *
_gtk_css_number_value_new (double     value,
                           GtkCssUnit unit)
{
  return gtk_css_dimension_value_new (value, unit);
}

gboolean
gtk_css_number_value_can_parse (GtkCssParser *parser)
{
  return gtk_css_parser_has_token (parser, GTK_CSS_TOKEN_SIGNED_NUMBER)
      || gtk_css_parser_has_token (parser, GTK_CSS_TOKEN_SIGNLESS_NUMBER)
      || gtk_css_parser_has_token (parser, GTK_CSS_TOKEN_SIGNED_INTEGER)
      || gtk_css_parser_has_token (parser, GTK_CSS_TOKEN_SIGNLESS_INTEGER)
      || gtk_css_parser_has_token (parser, GTK_CSS_TOKEN_PERCENTAGE)
      || gtk_css_parser_has_token (parser, GTK_CSS_TOKEN_SIGNED_INTEGER_DIMENSION)
      || gtk_css_parser_has_token (parser, GTK_CSS_TOKEN_SIGNLESS_INTEGER_DIMENSION)
      || gtk_css_parser_has_token (parser, GTK_CSS_TOKEN_SIGNED_DIMENSION)
      || gtk_css_parser_has_token (parser, GTK_CSS_TOKEN_SIGNLESS_DIMENSION)
      || gtk_css_parser_has_function (parser, "calc");
}

GtkCssValue *
_gtk_css_number_value_parse (GtkCssParser           *parser,
                             GtkCssNumberParseFlags  flags)
{
  if (gtk_css_parser_has_function (parser, "calc"))
    return gtk_css_calc_value_parse (parser, flags);

  return gtk_css_dimension_value_parse (parser, flags);
}

double
_gtk_css_number_value_get (const GtkCssValue *value,
                           double             one_hundred_percent)
{
  double result;
  guint i;

  if (G_LIKELY (value->type == TYPE_DIMENSION))
    {
      if (value->dimension.unit == GTK_CSS_PERCENT)
        return value->dimension.value * one_hundred_percent / 100;
      else
        return value->dimension.value;
    }

  g_assert (value->type == TYPE_CALC);

  result = 0.0;
  for (i = 0; i < value->calc.n_terms; i++)
    result += _gtk_css_number_value_get (value->calc.terms[i], one_hundred_percent);

  return result;
}

gboolean
gtk_css_dimension_value_is_zero (const GtkCssValue *value)
{
  g_assert (value != 0);
  g_assert (value->class == &GTK_CSS_VALUE_NUMBER);

  if (value->type != TYPE_DIMENSION)
    return FALSE;

  return value->dimension.value == 0;
}
