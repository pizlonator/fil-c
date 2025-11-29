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

#include "gtkcsseasevalueprivate.h"

#include <math.h>

typedef enum {
  GTK_CSS_EASE_CUBIC_BEZIER,
  GTK_CSS_EASE_STEPS
} GtkCssEaseType;

struct _GtkCssValue {
  GTK_CSS_VALUE_BASE
  GtkCssEaseType type;
  union {
    struct {
      double x1;
      double y1;
      double x2;
      double y2;
    } cubic;
    struct {
      guint steps;
      gboolean start;
    } steps;
  } u;
};

static void
gtk_css_value_ease_free (GtkCssValue *value)
{
  g_free (value);
}

static GtkCssValue *
gtk_css_value_ease_compute (GtkCssValue      *value,
                            guint             property_id,
                            GtkStyleProvider *provider,
                            GtkCssStyle      *style,
                            GtkCssStyle      *parent_style)
{
  return _gtk_css_value_ref (value);
}

static gboolean
gtk_css_value_ease_equal (const GtkCssValue *ease1,
                          const GtkCssValue *ease2)
{
  if (ease1->type != ease2->type)
    return FALSE;
  
  switch (ease1->type)
    {
    case GTK_CSS_EASE_CUBIC_BEZIER:
      return ease1->u.cubic.x1 == ease2->u.cubic.x1 &&
             ease1->u.cubic.y1 == ease2->u.cubic.y1 &&
             ease1->u.cubic.x2 == ease2->u.cubic.x2 &&
             ease1->u.cubic.y2 == ease2->u.cubic.y2;
    case GTK_CSS_EASE_STEPS:
      return ease1->u.steps.steps == ease2->u.steps.steps &&
             ease1->u.steps.start == ease2->u.steps.start;
    default:
      g_assert_not_reached ();
      return FALSE;
    }
}

static GtkCssValue *
gtk_css_value_ease_transition (GtkCssValue *start,
                               GtkCssValue *end,
                               guint        property_id,
                               double       progress)
{
  return NULL;
}

static void
gtk_css_value_ease_print (const GtkCssValue *ease,
                          GString           *string)
{
  switch (ease->type)
    {
    case GTK_CSS_EASE_CUBIC_BEZIER:
      if (ease->u.cubic.x1 == 0.25 && ease->u.cubic.y1 == 0.1 &&
          ease->u.cubic.x2 == 0.25 && ease->u.cubic.y2 == 1.0)
        g_string_append (string, "ease");
      else if (ease->u.cubic.x1 == 0.0 && ease->u.cubic.y1 == 0.0 &&
               ease->u.cubic.x2 == 1.0 && ease->u.cubic.y2 == 1.0)
        g_string_append (string, "linear");
      else if (ease->u.cubic.x1 == 0.42 && ease->u.cubic.y1 == 0.0 &&
               ease->u.cubic.x2 == 1.0  && ease->u.cubic.y2 == 1.0)
        g_string_append (string, "ease-in");
      else if (ease->u.cubic.x1 == 0.0  && ease->u.cubic.y1 == 0.0 &&
               ease->u.cubic.x2 == 0.58 && ease->u.cubic.y2 == 1.0)
        g_string_append (string, "ease-out");
      else if (ease->u.cubic.x1 == 0.42 && ease->u.cubic.y1 == 0.0 &&
               ease->u.cubic.x2 == 0.58 && ease->u.cubic.y2 == 1.0)
        g_string_append (string, "ease-in-out");
      else
        g_string_append_printf (string, "cubic-bezier(%g,%g,%g,%g)", 
                                ease->u.cubic.x1, ease->u.cubic.y1,
                                ease->u.cubic.x2, ease->u.cubic.y2);
      break;
    case GTK_CSS_EASE_STEPS:
      if (ease->u.steps.steps == 1)
        {
          g_string_append (string, ease->u.steps.start ? "step-start" : "step-end");
        }
      else
        {
          g_string_append_printf (string, "steps(%u%s)", ease->u.steps.steps, ease->u.steps.start ? ",start" : "");
        }
      break;
    default:
      g_assert_not_reached ();
      break;
    }
}

static const GtkCssValueClass GTK_CSS_VALUE_EASE = {
  "GtkCssEaseValue",
  gtk_css_value_ease_free,
  gtk_css_value_ease_compute,
  gtk_css_value_ease_equal,
  gtk_css_value_ease_transition,
  NULL,
  NULL,
  gtk_css_value_ease_print
};

GtkCssValue *
_gtk_css_ease_value_new_cubic_bezier (double x1,
                                      double y1,
                                      double x2,
                                      double y2)
{
  GtkCssValue *value;

  g_return_val_if_fail (x1 >= 0.0, NULL);
  g_return_val_if_fail (x1 <= 1.0, NULL);
  g_return_val_if_fail (x2 >= 0.0, NULL);
  g_return_val_if_fail (x2 <= 1.0, NULL);

  value = _gtk_css_value_new (GtkCssValue, &GTK_CSS_VALUE_EASE);

  value->type = GTK_CSS_EASE_CUBIC_BEZIER;
  value->u.cubic.x1 = x1;
  value->u.cubic.y1 = y1;
  value->u.cubic.x2 = x2;
  value->u.cubic.y2 = y2;
  value->is_computed = TRUE;

  return value;
}

static GtkCssValue *
_gtk_css_ease_value_new_steps (guint n_steps,
                               gboolean start)
{
  GtkCssValue *value;

  g_return_val_if_fail (n_steps > 0, NULL);

  value = _gtk_css_value_new (GtkCssValue, &GTK_CSS_VALUE_EASE);

  value->type = GTK_CSS_EASE_STEPS;
  value->u.steps.steps = n_steps;
  value->u.steps.start = start;
  value->is_computed = TRUE;

  return value;
}

static const struct {
  const char *name;
  guint is_bezier :1;
  guint is_function :1;
  double values[4];
} parser_values[] = {
  { "linear",       TRUE,  FALSE, { 0.0,  0.0, 1.0,  1.0 } },
  { "ease-in-out",  TRUE,  FALSE, { 0.42, 0.0, 0.58, 1.0 } },
  { "ease-in",      TRUE,  FALSE, { 0.42, 0.0, 1.0,  1.0 } },
  { "ease-out",     TRUE,  FALSE, { 0.0,  0.0, 0.58, 1.0 } },
  { "ease",         TRUE,  FALSE, { 0.25, 0.1, 0.25, 1.0 } },
  { "step-start",   FALSE, FALSE, { 1.0,  1.0, 0.0,  0.0 } },
  { "step-end",     FALSE, FALSE, { 1.0,  0.0, 0.0,  0.0 } },
  { "steps",        FALSE, TRUE,  { 0.0,  0.0, 0.0,  0.0 } },
  { "cubic-bezier", TRUE,  TRUE,  { 0.0,  0.0, 0.0,  0.0 } }
};

gboolean
_gtk_css_ease_value_can_parse (GtkCssParser *parser)
{
  guint i;

  for (i = 0; i < G_N_ELEMENTS (parser_values); i++)
    {
      if (parser_values[i].is_function)
        {
          if (gtk_css_parser_has_function (parser, parser_values[i].name))
            return TRUE;
        }
      else
        {
          if (gtk_css_parser_has_ident (parser, parser_values[i].name))
            return TRUE;
        }
    }

  return FALSE;
}

static guint
gtk_css_ease_value_parse_cubic_bezier_arg (GtkCssParser *parser,
                                           guint         arg,
                                           gpointer      data)
{
  double *values = data;

  if (!gtk_css_parser_consume_number (parser, &values[arg]))
    return 0;

  if (arg % 2 == 0)
    {
      if (values[arg] < 0 || values[arg] > 1.0)
        {
          gtk_css_parser_error_value (parser, "value %g out of range. Must be from 0.0 to 1.0", values[arg]);
          return 0;
        }
    }

  return 1;
}

static GtkCssValue *
gtk_css_ease_value_parse_cubic_bezier (GtkCssParser *parser)
{
  double values[4];

  if (!gtk_css_parser_consume_function (parser, 4, 4, gtk_css_ease_value_parse_cubic_bezier_arg, values))
    return NULL;

  return _gtk_css_ease_value_new_cubic_bezier (values[0], values[1], values[2], values[3]);
}

typedef struct 
{
  int n_steps;
  gboolean start;
} ParseStepsData;

static guint
gtk_css_ease_value_parse_steps_arg (GtkCssParser *parser,
                                    guint         arg,
                                    gpointer      data_)
{
  ParseStepsData *data = data_;

  switch (arg)
  {
    case 0:
      if (!gtk_css_parser_consume_integer (parser, &data->n_steps))
        {
          return 0;
        }
      else if (data->n_steps < 1)
        {
          gtk_css_parser_error_value (parser, "Number of steps must be > 0");
          return 0;
        }
      return 1;

    case 1:
      if (gtk_css_parser_try_ident (parser, "start"))
        data->start = TRUE;
      else if (gtk_css_parser_try_ident (parser, "end"))
        data->start = FALSE;
      else
        {
          gtk_css_parser_error_syntax (parser, "Only allowed values are 'start' and 'end'");
          return 0;
        }
      return 1;

    default:
      g_return_val_if_reached (0);
  }
}

static GtkCssValue *
gtk_css_ease_value_parse_steps (GtkCssParser *parser)
{
  ParseStepsData data = { 0, FALSE };  

  if (!gtk_css_parser_consume_function (parser, 1, 2, gtk_css_ease_value_parse_steps_arg, &data))
    return NULL;

  return _gtk_css_ease_value_new_steps (data.n_steps, data.start);
}

GtkCssValue *
_gtk_css_ease_value_parse (GtkCssParser *parser)
{
  guint i;

  g_return_val_if_fail (parser != NULL, NULL);

  for (i = 0; i < G_N_ELEMENTS (parser_values); i++)
    {
      if (parser_values[i].is_function)
        {
          if (gtk_css_parser_has_function (parser, parser_values[i].name))
            {
              if (parser_values[i].is_bezier)
                return gtk_css_ease_value_parse_cubic_bezier (parser);
              else
                return gtk_css_ease_value_parse_steps (parser);
            }
        }
      else
        {
          if (gtk_css_parser_try_ident (parser, parser_values[i].name))
            {
              if (parser_values[i].is_bezier)
                return _gtk_css_ease_value_new_cubic_bezier (parser_values[i].values[0],
                                                             parser_values[i].values[1],
                                                             parser_values[i].values[2],
                                                             parser_values[i].values[3]);
              else
                return _gtk_css_ease_value_new_steps (parser_values[i].values[0],
                                                      parser_values[i].values[1] != 0.0);
            }
        }
    }

  gtk_css_parser_error_syntax (parser, "Expected a valid ease value");
  return NULL;
}

double
_gtk_css_ease_value_transform (const GtkCssValue *ease,
                               double             progress)
{
  g_return_val_if_fail (ease->class == &GTK_CSS_VALUE_EASE, 1.0);

  if (progress <= 0)
    return 0;
  if (progress >= 1)
    return 1;

  switch (ease->type)
    {
    case GTK_CSS_EASE_CUBIC_BEZIER:
      {
        static const double epsilon = 0.00001;
        double tmin, t, tmax;

        tmin = 0.0;
        tmax = 1.0;
        t = progress;

        while (tmin < tmax)
          {
             double sample;
             sample = (((1.0 + 3 * ease->u.cubic.x1 - 3 * ease->u.cubic.x2) * t
                       +      -6 * ease->u.cubic.x1 + 3 * ease->u.cubic.x2) * t
                       +       3 * ease->u.cubic.x1                       ) * t;
             if (fabs(sample - progress) < epsilon)
               break;

             if (progress > sample)
               tmin = t;
             else
               tmax = t;
             t = (tmax + tmin) * .5;
          }

        return (((1.0 + 3 * ease->u.cubic.y1 - 3 * ease->u.cubic.y2) * t
                +      -6 * ease->u.cubic.y1 + 3 * ease->u.cubic.y2) * t
                +       3 * ease->u.cubic.y1                       ) * t;
      }
    case GTK_CSS_EASE_STEPS:
      progress *= ease->u.steps.steps;
      progress = floor (progress) + (ease->u.steps.start ? 0 : 1);
      return progress / ease->u.steps.steps;
    default:
      g_assert_not_reached ();
      return 1.0;
    }
}


