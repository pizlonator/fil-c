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

#include "gtkcssrepeatvalueprivate.h"

#include "gtkcssnumbervalueprivate.h"

struct _GtkCssValue {
  GTK_CSS_VALUE_BASE
  GtkCssRepeatStyle x;
  GtkCssRepeatStyle y;
};

static void
gtk_css_value_repeat_free (GtkCssValue *value)
{
  g_free (value);
}

static GtkCssValue *
gtk_css_value_repeat_compute (GtkCssValue      *value,
                              guint             property_id,
                              GtkStyleProvider *provider,
                              GtkCssStyle      *style,
                              GtkCssStyle      *parent_style)
{
  return _gtk_css_value_ref (value);
}

static gboolean
gtk_css_value_repeat_equal (const GtkCssValue *repeat1,
                            const GtkCssValue *repeat2)
{
  return repeat1->x == repeat2->x
      && repeat1->y == repeat2->y;
}

static GtkCssValue *
gtk_css_value_repeat_transition (GtkCssValue *start,
                                 GtkCssValue *end,
                                 guint        property_id,
                                 double       progress)
{
  return NULL;
}

static void
gtk_css_value_background_repeat_print (const GtkCssValue *repeat,
                                       GString           *string)
{
  static const char *names[] = {
    "no-repeat",
    "repeat",
    "round",
    "space"
  };

  if (repeat->x == repeat->y)
    {
      g_string_append (string, names[repeat->x]);
    }
  else if (repeat->x == GTK_CSS_REPEAT_STYLE_REPEAT &&
           repeat->y == GTK_CSS_REPEAT_STYLE_NO_REPEAT)
    {
      g_string_append (string, "repeat-x");
    }
  else if (repeat->x == GTK_CSS_REPEAT_STYLE_NO_REPEAT &&
           repeat->y == GTK_CSS_REPEAT_STYLE_REPEAT)
    {
      g_string_append (string, "repeat-y");
    }
  else
    {
      g_string_append (string, names[repeat->x]);
      g_string_append_c (string, ' ');
      g_string_append (string, names[repeat->y]);
    }
}

static void
gtk_css_value_border_repeat_print (const GtkCssValue *repeat,
                                   GString           *string)
{
  static const char *names[] = {
    "stretch",
    "repeat",
    "round",
    "space"
  };

  g_string_append (string, names[repeat->x]);
  if (repeat->x != repeat->y)
    {
      g_string_append_c (string, ' ');
      g_string_append (string, names[repeat->y]);
    }
}

static const GtkCssValueClass GTK_CSS_VALUE_BACKGROUND_REPEAT = {
  "GtkCssBackgroundRepeatValue",
  gtk_css_value_repeat_free,
  gtk_css_value_repeat_compute,
  gtk_css_value_repeat_equal,
  gtk_css_value_repeat_transition,
  NULL,
  NULL,
  gtk_css_value_background_repeat_print
};

static const GtkCssValueClass GTK_CSS_VALUE_BORDER_REPEAT = {
  "GtkCssBorderRepeatValue",
  gtk_css_value_repeat_free,
  gtk_css_value_repeat_compute,
  gtk_css_value_repeat_equal,
  gtk_css_value_repeat_transition,
  NULL,
  NULL,
  gtk_css_value_border_repeat_print
};
/* BACKGROUND REPEAT */

static struct {
  const char *name;
  GtkCssValue values[4];
} background_repeat_values[4] = {
  { "no-repeat",
  { { &GTK_CSS_VALUE_BACKGROUND_REPEAT, 1, TRUE, GTK_CSS_REPEAT_STYLE_NO_REPEAT, GTK_CSS_REPEAT_STYLE_NO_REPEAT },
    { &GTK_CSS_VALUE_BACKGROUND_REPEAT, 1, TRUE, GTK_CSS_REPEAT_STYLE_NO_REPEAT, GTK_CSS_REPEAT_STYLE_REPEAT    },
    { &GTK_CSS_VALUE_BACKGROUND_REPEAT, 1, TRUE, GTK_CSS_REPEAT_STYLE_NO_REPEAT, GTK_CSS_REPEAT_STYLE_ROUND     },
    { &GTK_CSS_VALUE_BACKGROUND_REPEAT, 1, TRUE, GTK_CSS_REPEAT_STYLE_NO_REPEAT, GTK_CSS_REPEAT_STYLE_SPACE     }
  } },
  { "repeat",
  { { &GTK_CSS_VALUE_BACKGROUND_REPEAT, 1, TRUE, GTK_CSS_REPEAT_STYLE_REPEAT,    GTK_CSS_REPEAT_STYLE_NO_REPEAT },
    { &GTK_CSS_VALUE_BACKGROUND_REPEAT, 1, TRUE, GTK_CSS_REPEAT_STYLE_REPEAT,    GTK_CSS_REPEAT_STYLE_REPEAT    },
    { &GTK_CSS_VALUE_BACKGROUND_REPEAT, 1, TRUE, GTK_CSS_REPEAT_STYLE_REPEAT,    GTK_CSS_REPEAT_STYLE_ROUND     },
    { &GTK_CSS_VALUE_BACKGROUND_REPEAT, 1, TRUE, GTK_CSS_REPEAT_STYLE_REPEAT,    GTK_CSS_REPEAT_STYLE_SPACE     }
  } }, 
  { "round",
  { { &GTK_CSS_VALUE_BACKGROUND_REPEAT, 1, TRUE, GTK_CSS_REPEAT_STYLE_ROUND,     GTK_CSS_REPEAT_STYLE_NO_REPEAT },
    { &GTK_CSS_VALUE_BACKGROUND_REPEAT, 1, TRUE, GTK_CSS_REPEAT_STYLE_ROUND,     GTK_CSS_REPEAT_STYLE_REPEAT    },
    { &GTK_CSS_VALUE_BACKGROUND_REPEAT, 1, TRUE, GTK_CSS_REPEAT_STYLE_ROUND,     GTK_CSS_REPEAT_STYLE_ROUND     },
    { &GTK_CSS_VALUE_BACKGROUND_REPEAT, 1, TRUE, GTK_CSS_REPEAT_STYLE_ROUND,     GTK_CSS_REPEAT_STYLE_SPACE     }
  } }, 
  { "space",
  { { &GTK_CSS_VALUE_BACKGROUND_REPEAT, 1, TRUE, GTK_CSS_REPEAT_STYLE_SPACE,     GTK_CSS_REPEAT_STYLE_NO_REPEAT },
    { &GTK_CSS_VALUE_BACKGROUND_REPEAT, 1, TRUE, GTK_CSS_REPEAT_STYLE_SPACE,     GTK_CSS_REPEAT_STYLE_REPEAT    },
    { &GTK_CSS_VALUE_BACKGROUND_REPEAT, 1, TRUE, GTK_CSS_REPEAT_STYLE_SPACE,     GTK_CSS_REPEAT_STYLE_ROUND     },
    { &GTK_CSS_VALUE_BACKGROUND_REPEAT, 1, TRUE, GTK_CSS_REPEAT_STYLE_SPACE,     GTK_CSS_REPEAT_STYLE_SPACE     }
  } }
};

GtkCssValue *
_gtk_css_background_repeat_value_new (GtkCssRepeatStyle x,
                                      GtkCssRepeatStyle y)
{
  return _gtk_css_value_ref (&background_repeat_values[x].values[y]);
}

static gboolean
_gtk_css_background_repeat_style_try (GtkCssParser      *parser,
                                      GtkCssRepeatStyle *result)
{
  guint i;

  for (i = 0; i < G_N_ELEMENTS (background_repeat_values); i++)
    {
      if (gtk_css_parser_try_ident (parser, background_repeat_values[i].name))
        {
          *result = i;
          return TRUE;
        }
    }

  return FALSE;
}

GtkCssValue *
_gtk_css_background_repeat_value_try_parse (GtkCssParser *parser)
{
  GtkCssRepeatStyle x, y;

  g_return_val_if_fail (parser != NULL, NULL);

  if (gtk_css_parser_try_ident (parser, "repeat-x"))
    return _gtk_css_background_repeat_value_new (GTK_CSS_REPEAT_STYLE_REPEAT, GTK_CSS_REPEAT_STYLE_NO_REPEAT);
  if (gtk_css_parser_try_ident (parser, "repeat-y"))
    return _gtk_css_background_repeat_value_new (GTK_CSS_REPEAT_STYLE_NO_REPEAT, GTK_CSS_REPEAT_STYLE_REPEAT);

  if (!_gtk_css_background_repeat_style_try (parser, &x))
    return NULL;

  if (!_gtk_css_background_repeat_style_try (parser, &y))
    y = x;

  return _gtk_css_background_repeat_value_new (x, y);
}

GtkCssRepeatStyle
_gtk_css_background_repeat_value_get_x (const GtkCssValue *repeat)
{
  g_return_val_if_fail (repeat->class == &GTK_CSS_VALUE_BACKGROUND_REPEAT, GTK_CSS_REPEAT_STYLE_NO_REPEAT);

  return repeat->x;
}

GtkCssRepeatStyle
_gtk_css_background_repeat_value_get_y (const GtkCssValue *repeat)
{
  g_return_val_if_fail (repeat->class == &GTK_CSS_VALUE_BACKGROUND_REPEAT, GTK_CSS_REPEAT_STYLE_NO_REPEAT);

  return repeat->y;
}

/* BORDER IMAGE REPEAT */

static struct {
  const char *name;
  GtkCssValue values[4];
} border_repeat_values[4] = {
  { "stretch",
  { { &GTK_CSS_VALUE_BORDER_REPEAT, 1, TRUE, GTK_CSS_REPEAT_STYLE_STRETCH, GTK_CSS_REPEAT_STYLE_STRETCH },
    { &GTK_CSS_VALUE_BORDER_REPEAT, 1, TRUE, GTK_CSS_REPEAT_STYLE_STRETCH, GTK_CSS_REPEAT_STYLE_REPEAT  },
    { &GTK_CSS_VALUE_BORDER_REPEAT, 1, TRUE, GTK_CSS_REPEAT_STYLE_STRETCH, GTK_CSS_REPEAT_STYLE_ROUND   },
    { &GTK_CSS_VALUE_BORDER_REPEAT, 1, TRUE, GTK_CSS_REPEAT_STYLE_STRETCH, GTK_CSS_REPEAT_STYLE_SPACE   }
  } },
  { "repeat",
  { { &GTK_CSS_VALUE_BORDER_REPEAT, 1, TRUE, GTK_CSS_REPEAT_STYLE_REPEAT,  GTK_CSS_REPEAT_STYLE_STRETCH },
    { &GTK_CSS_VALUE_BORDER_REPEAT, 1, TRUE, GTK_CSS_REPEAT_STYLE_REPEAT,  GTK_CSS_REPEAT_STYLE_REPEAT  },
    { &GTK_CSS_VALUE_BORDER_REPEAT, 1, TRUE, GTK_CSS_REPEAT_STYLE_REPEAT,  GTK_CSS_REPEAT_STYLE_ROUND   },
    { &GTK_CSS_VALUE_BORDER_REPEAT, 1, TRUE, GTK_CSS_REPEAT_STYLE_REPEAT,  GTK_CSS_REPEAT_STYLE_SPACE   }
  } }, 
  { "round",
  { { &GTK_CSS_VALUE_BORDER_REPEAT, 1, TRUE, GTK_CSS_REPEAT_STYLE_ROUND,   GTK_CSS_REPEAT_STYLE_STRETCH },
    { &GTK_CSS_VALUE_BORDER_REPEAT, 1, TRUE, GTK_CSS_REPEAT_STYLE_ROUND,   GTK_CSS_REPEAT_STYLE_REPEAT  },
    { &GTK_CSS_VALUE_BORDER_REPEAT, 1, TRUE, GTK_CSS_REPEAT_STYLE_ROUND,   GTK_CSS_REPEAT_STYLE_ROUND   },
    { &GTK_CSS_VALUE_BORDER_REPEAT, 1, TRUE, GTK_CSS_REPEAT_STYLE_ROUND,   GTK_CSS_REPEAT_STYLE_SPACE   }
  } }, 
  { "space",
  { { &GTK_CSS_VALUE_BORDER_REPEAT, 1, TRUE, GTK_CSS_REPEAT_STYLE_SPACE,   GTK_CSS_REPEAT_STYLE_STRETCH },
    { &GTK_CSS_VALUE_BORDER_REPEAT, 1, TRUE, GTK_CSS_REPEAT_STYLE_SPACE,   GTK_CSS_REPEAT_STYLE_REPEAT  },
    { &GTK_CSS_VALUE_BORDER_REPEAT, 1, TRUE, GTK_CSS_REPEAT_STYLE_SPACE,   GTK_CSS_REPEAT_STYLE_ROUND   },
    { &GTK_CSS_VALUE_BORDER_REPEAT, 1, TRUE, GTK_CSS_REPEAT_STYLE_SPACE,   GTK_CSS_REPEAT_STYLE_SPACE   }
  } }
};

GtkCssValue *
_gtk_css_border_repeat_value_new (GtkCssRepeatStyle x,
                                  GtkCssRepeatStyle y)
{
  return _gtk_css_value_ref (&border_repeat_values[x].values[y]);
}

static gboolean
_gtk_css_border_repeat_style_try (GtkCssParser      *parser,
                                  GtkCssRepeatStyle *result)
{
  guint i;

  for (i = 0; i < G_N_ELEMENTS (border_repeat_values); i++)
    {
      if (gtk_css_parser_try_ident (parser, border_repeat_values[i].name))
        {
          *result = i;
          return TRUE;
        }
    }

  return FALSE;
}

GtkCssValue *
_gtk_css_border_repeat_value_try_parse (GtkCssParser *parser)
{
  GtkCssRepeatStyle x, y;

  g_return_val_if_fail (parser != NULL, NULL);

  if (!_gtk_css_border_repeat_style_try (parser, &x))
    return NULL;

  if (!_gtk_css_border_repeat_style_try (parser, &y))
    y = x;

  return _gtk_css_border_repeat_value_new (x, y);
}

GtkCssRepeatStyle
_gtk_css_border_repeat_value_get_x (const GtkCssValue *repeat)
{
  g_return_val_if_fail (repeat->class == &GTK_CSS_VALUE_BORDER_REPEAT, GTK_CSS_REPEAT_STYLE_STRETCH);

  return repeat->x;
}

GtkCssRepeatStyle
_gtk_css_border_repeat_value_get_y (const GtkCssValue *repeat)
{
  g_return_val_if_fail (repeat->class == &GTK_CSS_VALUE_BORDER_REPEAT, GTK_CSS_REPEAT_STYLE_STRETCH);

  return repeat->y;
}

