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

#include "gtkcssenumvalueprivate.h"

#include "gtkcssstyleprivate.h"
#include "gtkcssnumbervalueprivate.h"
#include "gtkstyleproviderprivate.h"
#include "gtksettingsprivate.h"

#include "gtkpopcountprivate.h"

/* repeated API */

struct _GtkCssValue {
  GTK_CSS_VALUE_BASE
  int value;
  const char *name;
};

static void
gtk_css_value_enum_free (GtkCssValue *value)
{
  g_free (value);
}

static GtkCssValue *
gtk_css_value_enum_compute (GtkCssValue      *value,
                            guint             property_id,
                            GtkStyleProvider *provider,
                            GtkCssStyle      *style,
                            GtkCssStyle      *parent_style)
{
  return _gtk_css_value_ref (value);
}

static gboolean
gtk_css_value_enum_equal (const GtkCssValue *enum1,
                          const GtkCssValue *enum2)
{
  return enum1 == enum2;
}

static GtkCssValue *
gtk_css_value_enum_transition (GtkCssValue *start,
                               GtkCssValue *end,
                               guint        property_id,
                               double       progress)
{
  return NULL;
}

static void
gtk_css_value_enum_print (const GtkCssValue *value,
                          GString           *string)
{
  g_string_append (string, value->name);
}

/* GtkBorderStyle */

static const GtkCssValueClass GTK_CSS_VALUE_BORDER_STYLE = {
  "GtkCssBorderStyleValue",
  gtk_css_value_enum_free,
  gtk_css_value_enum_compute,
  gtk_css_value_enum_equal,
  gtk_css_value_enum_transition,
  NULL,
  NULL,
  gtk_css_value_enum_print
};

static GtkCssValue border_style_values[] = {
  { &GTK_CSS_VALUE_BORDER_STYLE, 1, TRUE, GTK_BORDER_STYLE_NONE, "none" },
  { &GTK_CSS_VALUE_BORDER_STYLE, 1, TRUE, GTK_BORDER_STYLE_SOLID, "solid" },
  { &GTK_CSS_VALUE_BORDER_STYLE, 1, TRUE, GTK_BORDER_STYLE_INSET, "inset" },
  { &GTK_CSS_VALUE_BORDER_STYLE, 1, TRUE, GTK_BORDER_STYLE_OUTSET, "outset" },
  { &GTK_CSS_VALUE_BORDER_STYLE, 1, TRUE, GTK_BORDER_STYLE_HIDDEN, "hidden" },
  { &GTK_CSS_VALUE_BORDER_STYLE, 1, TRUE, GTK_BORDER_STYLE_DOTTED, "dotted" },
  { &GTK_CSS_VALUE_BORDER_STYLE, 1, TRUE, GTK_BORDER_STYLE_DASHED, "dashed" },
  { &GTK_CSS_VALUE_BORDER_STYLE, 1, TRUE, GTK_BORDER_STYLE_DOUBLE, "double" },
  { &GTK_CSS_VALUE_BORDER_STYLE, 1, TRUE, GTK_BORDER_STYLE_GROOVE, "groove" },
  { &GTK_CSS_VALUE_BORDER_STYLE, 1, TRUE, GTK_BORDER_STYLE_RIDGE, "ridge" }
};

GtkCssValue *
_gtk_css_border_style_value_new (GtkBorderStyle border_style)
{
  g_return_val_if_fail (border_style < G_N_ELEMENTS (border_style_values), NULL);

  return _gtk_css_value_ref (&border_style_values[border_style]);
}

GtkCssValue *
_gtk_css_border_style_value_try_parse (GtkCssParser *parser)
{
  guint i;

  g_return_val_if_fail (parser != NULL, NULL);

  for (i = 0; i < G_N_ELEMENTS (border_style_values); i++)
    {
      if (gtk_css_parser_try_ident (parser, border_style_values[i].name))
        return _gtk_css_value_ref (&border_style_values[i]);
    }

  return NULL;
}

GtkBorderStyle
_gtk_css_border_style_value_get (const GtkCssValue *value)
{
  g_return_val_if_fail (value->class == &GTK_CSS_VALUE_BORDER_STYLE, GTK_BORDER_STYLE_NONE);

  return value->value;
}

/* GtkCssBlendMode */

static const GtkCssValueClass GTK_CSS_VALUE_BLEND_MODE = {
  "GtkCssBlendModeValue",
  gtk_css_value_enum_free,
  gtk_css_value_enum_compute,
  gtk_css_value_enum_equal,
  gtk_css_value_enum_transition,
  NULL,
  NULL,
  gtk_css_value_enum_print
};

static GtkCssValue blend_mode_values[] = {
  { &GTK_CSS_VALUE_BLEND_MODE, 1, TRUE, GSK_BLEND_MODE_DEFAULT, "normal" },
  { &GTK_CSS_VALUE_BLEND_MODE, 1, TRUE, GSK_BLEND_MODE_MULTIPLY, "multiply" },
  { &GTK_CSS_VALUE_BLEND_MODE, 1, TRUE, GSK_BLEND_MODE_SCREEN, "screen" },
  { &GTK_CSS_VALUE_BLEND_MODE, 1, TRUE, GSK_BLEND_MODE_OVERLAY, "overlay" },
  { &GTK_CSS_VALUE_BLEND_MODE, 1, TRUE, GSK_BLEND_MODE_DARKEN, "darken" },
  { &GTK_CSS_VALUE_BLEND_MODE, 1, TRUE, GSK_BLEND_MODE_LIGHTEN, "lighten" },
  { &GTK_CSS_VALUE_BLEND_MODE, 1, TRUE, GSK_BLEND_MODE_COLOR_DODGE, "color-dodge" },
  { &GTK_CSS_VALUE_BLEND_MODE, 1, TRUE, GSK_BLEND_MODE_COLOR_BURN, "color-burn" },
  { &GTK_CSS_VALUE_BLEND_MODE, 1, TRUE, GSK_BLEND_MODE_HARD_LIGHT, "hard-light" },
  { &GTK_CSS_VALUE_BLEND_MODE, 1, TRUE, GSK_BLEND_MODE_SOFT_LIGHT, "soft-light" },
  { &GTK_CSS_VALUE_BLEND_MODE, 1, TRUE, GSK_BLEND_MODE_DIFFERENCE, "difference" },
  { &GTK_CSS_VALUE_BLEND_MODE, 1, TRUE, GSK_BLEND_MODE_EXCLUSION, "exclusion" },
  { &GTK_CSS_VALUE_BLEND_MODE, 1, TRUE, GSK_BLEND_MODE_COLOR, "color" },
  { &GTK_CSS_VALUE_BLEND_MODE, 1, TRUE, GSK_BLEND_MODE_HUE, "hue" },
  { &GTK_CSS_VALUE_BLEND_MODE, 1, TRUE, GSK_BLEND_MODE_SATURATION, "saturation" },
  { &GTK_CSS_VALUE_BLEND_MODE, 1, TRUE, GSK_BLEND_MODE_LUMINOSITY, "luminosity" }
};

GtkCssValue *
_gtk_css_blend_mode_value_new (GskBlendMode blend_mode)
{
  g_return_val_if_fail (blend_mode < G_N_ELEMENTS (blend_mode_values), NULL);

  return _gtk_css_value_ref (&blend_mode_values[blend_mode]);
}

GtkCssValue *
_gtk_css_blend_mode_value_try_parse (GtkCssParser *parser)
{
  guint i;

  g_return_val_if_fail (parser != NULL, NULL);

  for (i = 0; i < G_N_ELEMENTS (blend_mode_values); i++)
    {
      if (gtk_css_parser_try_ident (parser, blend_mode_values[i].name))
        return _gtk_css_value_ref (&blend_mode_values[i]);
    }

  return NULL;
}

GskBlendMode
_gtk_css_blend_mode_value_get (const GtkCssValue *value)
{
  g_return_val_if_fail (value->class == &GTK_CSS_VALUE_BLEND_MODE, GSK_BLEND_MODE_DEFAULT);

  return value->value;
}

/* GtkCssFontSize */

static double
get_dpi (GtkCssStyle *style)
{
  return _gtk_css_number_value_get (style->core->dpi, 96);
}

/* XXX: Kinda bad to have that machinery here, nobody expects vital font
 * size code to appear in gtkcssvalueenum.c.
 */
#define DEFAULT_FONT_SIZE_PT 10

double
gtk_css_font_size_get_default_px (GtkStyleProvider *provider,
                                  GtkCssStyle      *style)
{
  GtkSettings *settings;
  int font_size;

  settings = gtk_style_provider_get_settings (provider);
  if (settings == NULL)
    return DEFAULT_FONT_SIZE_PT * get_dpi (style) / 72.0;

  font_size = gtk_settings_get_font_size (settings);
  if (font_size == 0)
    return DEFAULT_FONT_SIZE_PT * get_dpi (style) / 72.0;
  else if (gtk_settings_get_font_size_is_absolute (settings))
    return (double) font_size / PANGO_SCALE;
  else
    return ((double) font_size / PANGO_SCALE) * get_dpi (style) / 72.0;
}

static GtkCssValue *
gtk_css_value_font_size_compute (GtkCssValue      *value,
                                 guint             property_id,
                                 GtkStyleProvider *provider,
                                 GtkCssStyle      *style,
                                 GtkCssStyle      *parent_style)
{
  double font_size;

  switch (value->value)
    {
    case GTK_CSS_FONT_SIZE_XX_SMALL:
      font_size = gtk_css_font_size_get_default_px (provider, style) * 3. / 5;
      break;
    case GTK_CSS_FONT_SIZE_X_SMALL:
      font_size = gtk_css_font_size_get_default_px (provider, style) * 3. / 4;
      break;
    case GTK_CSS_FONT_SIZE_SMALL:
      font_size = gtk_css_font_size_get_default_px (provider, style) * 8. / 9;
      break;
    default:
      g_assert_not_reached ();
      /* fall thru */
    case GTK_CSS_FONT_SIZE_MEDIUM:
      font_size = gtk_css_font_size_get_default_px (provider, style);
      break;
    case GTK_CSS_FONT_SIZE_LARGE:
      font_size = gtk_css_font_size_get_default_px (provider, style) * 6. / 5;
      break;
    case GTK_CSS_FONT_SIZE_X_LARGE:
      font_size = gtk_css_font_size_get_default_px (provider, style) * 3. / 2;
      break;
    case GTK_CSS_FONT_SIZE_XX_LARGE:
      font_size = gtk_css_font_size_get_default_px (provider, style) * 2;
      break;
    case GTK_CSS_FONT_SIZE_SMALLER:
      if (parent_style)
        font_size = _gtk_css_number_value_get (parent_style->core->font_size, 100);
      else
        font_size = gtk_css_font_size_get_default_px (provider, style);
      /* This is what WebKit does... */
      font_size /= 1.2;
      break;
    case GTK_CSS_FONT_SIZE_LARGER:
      if (parent_style)
        font_size = _gtk_css_number_value_get (parent_style->core->font_size, 100);
      else
        font_size = gtk_css_font_size_get_default_px (provider, style);
      /* This is what WebKit does... */
      font_size *= 1.2;
      break;
  }

  return _gtk_css_number_value_new (font_size, GTK_CSS_PX);
}

static const GtkCssValueClass GTK_CSS_VALUE_FONT_SIZE = {
  "GtkCssFontSizeValue",
  gtk_css_value_enum_free,
  gtk_css_value_font_size_compute,
  gtk_css_value_enum_equal,
  gtk_css_value_enum_transition,
  NULL,
  NULL,
  gtk_css_value_enum_print
};

static GtkCssValue font_size_values[] = {
  { &GTK_CSS_VALUE_FONT_SIZE, 1, FALSE, GTK_CSS_FONT_SIZE_SMALLER, "smaller" },
  { &GTK_CSS_VALUE_FONT_SIZE, 1, FALSE, GTK_CSS_FONT_SIZE_LARGER, "larger" },
  { &GTK_CSS_VALUE_FONT_SIZE, 1, FALSE, GTK_CSS_FONT_SIZE_XX_SMALL, "xx-small" },
  { &GTK_CSS_VALUE_FONT_SIZE, 1, FALSE, GTK_CSS_FONT_SIZE_X_SMALL, "x-small" },
  { &GTK_CSS_VALUE_FONT_SIZE, 1, FALSE, GTK_CSS_FONT_SIZE_SMALL, "small" },
  { &GTK_CSS_VALUE_FONT_SIZE, 1, FALSE, GTK_CSS_FONT_SIZE_MEDIUM, "medium" },
  { &GTK_CSS_VALUE_FONT_SIZE, 1, FALSE, GTK_CSS_FONT_SIZE_LARGE, "large" },
  { &GTK_CSS_VALUE_FONT_SIZE, 1, FALSE, GTK_CSS_FONT_SIZE_X_LARGE, "x-large" },
  { &GTK_CSS_VALUE_FONT_SIZE, 1, FALSE, GTK_CSS_FONT_SIZE_XX_LARGE, "xx-large" }
};

GtkCssValue *
_gtk_css_font_size_value_new (GtkCssFontSize font_size)
{
  g_return_val_if_fail (font_size < G_N_ELEMENTS (font_size_values), NULL);

  return _gtk_css_value_ref (&font_size_values[font_size]);
}

GtkCssValue *
_gtk_css_font_size_value_try_parse (GtkCssParser *parser)
{
  guint i;

  g_return_val_if_fail (parser != NULL, NULL);

  for (i = 0; i < G_N_ELEMENTS (font_size_values); i++)
    {
      if (gtk_css_parser_try_ident (parser, font_size_values[i].name))
        return _gtk_css_value_ref (&font_size_values[i]);
    }

  return NULL;
}

GtkCssFontSize
_gtk_css_font_size_value_get (const GtkCssValue *value)
{
  g_return_val_if_fail (value->class == &GTK_CSS_VALUE_FONT_SIZE, GTK_CSS_FONT_SIZE_MEDIUM);

  return value->value;
}

/* PangoStyle */

static const GtkCssValueClass GTK_CSS_VALUE_FONT_STYLE = {
  "GtkCssFontStyleValue",
  gtk_css_value_enum_free,
  gtk_css_value_enum_compute,
  gtk_css_value_enum_equal,
  gtk_css_value_enum_transition,
  NULL,
  NULL,
  gtk_css_value_enum_print
};

static GtkCssValue font_style_values[] = {
  { &GTK_CSS_VALUE_FONT_STYLE, 1, TRUE, PANGO_STYLE_NORMAL, "normal" },
  { &GTK_CSS_VALUE_FONT_STYLE, 1, TRUE, PANGO_STYLE_OBLIQUE, "oblique" },
  { &GTK_CSS_VALUE_FONT_STYLE, 1, TRUE, PANGO_STYLE_ITALIC, "italic" }
};

GtkCssValue *
_gtk_css_font_style_value_new (PangoStyle font_style)
{
  g_return_val_if_fail (font_style < G_N_ELEMENTS (font_style_values), NULL);

  return _gtk_css_value_ref (&font_style_values[font_style]);
}

GtkCssValue *
_gtk_css_font_style_value_try_parse (GtkCssParser *parser)
{
  guint i;

  g_return_val_if_fail (parser != NULL, NULL);

  for (i = 0; i < G_N_ELEMENTS (font_style_values); i++)
    {
      if (gtk_css_parser_try_ident (parser, font_style_values[i].name))
        return _gtk_css_value_ref (&font_style_values[i]);
    }

  return NULL;
}

PangoStyle
_gtk_css_font_style_value_get (const GtkCssValue *value)
{
  g_return_val_if_fail (value->class == &GTK_CSS_VALUE_FONT_STYLE, PANGO_STYLE_NORMAL);

  return value->value;
}

/* PangoWeight */

#define BOLDER -1
#define LIGHTER -2

static GtkCssValue *
gtk_css_value_font_weight_compute (GtkCssValue      *value,
                                   guint             property_id,
                                   GtkStyleProvider *provider,
                                   GtkCssStyle      *style,
                                   GtkCssStyle      *parent_style)
{
  PangoWeight new_weight;
  int parent_value;

  if (value->value >= 0)
    return _gtk_css_value_ref (value);

  if (parent_style)
    parent_value = _gtk_css_number_value_get (parent_style->font->font_weight, 100);
  else
    parent_value = 400;

  if (value->value == BOLDER)
    {
      if (parent_value < 350)
        new_weight = 400;
      else if (parent_value < 550)
        new_weight = 700;
      else
        new_weight = 900;
    }
  else if (value->value == LIGHTER)
    {
      if (parent_value > 750)
        new_weight = 700;
      else if (parent_value > 550)
        new_weight = 400;
      else
        new_weight = 100;
    }
  else
    {
      g_assert_not_reached ();
      new_weight = PANGO_WEIGHT_NORMAL;
    }

  return _gtk_css_number_value_new (new_weight, GTK_CSS_NUMBER);
}

static const GtkCssValueClass GTK_CSS_VALUE_FONT_WEIGHT = {
  "GtkCssFontWeightValue",
  gtk_css_value_enum_free,
  gtk_css_value_font_weight_compute,
  gtk_css_value_enum_equal,
  NULL,
  NULL,
  NULL,
  gtk_css_value_enum_print
};

static GtkCssValue font_weight_values[] = {
  { &GTK_CSS_VALUE_FONT_WEIGHT, 1, FALSE, BOLDER, "bolder" },
  { &GTK_CSS_VALUE_FONT_WEIGHT, 1, FALSE, LIGHTER, "lighter" },
};

GtkCssValue *
gtk_css_font_weight_value_try_parse (GtkCssParser *parser)
{
  guint i;

  g_return_val_if_fail (parser != NULL, NULL);

  for (i = 0; i < G_N_ELEMENTS (font_weight_values); i++)
    {
      if (gtk_css_parser_try_ident (parser, font_weight_values[i].name))
        return _gtk_css_value_ref (&font_weight_values[i]);
    }

  if (gtk_css_parser_try_ident (parser, "normal"))
    return _gtk_css_number_value_new (PANGO_WEIGHT_NORMAL, GTK_CSS_NUMBER);
  if (gtk_css_parser_try_ident (parser, "bold"))
    return _gtk_css_number_value_new (PANGO_WEIGHT_BOLD, GTK_CSS_NUMBER);

  return NULL;
}

PangoWeight
gtk_css_font_weight_value_get (const GtkCssValue *value)
{
  g_return_val_if_fail (value->class == &GTK_CSS_VALUE_FONT_WEIGHT, PANGO_WEIGHT_NORMAL);

  return value->value;
}

#undef BOLDER
#undef LIGHTER

/* PangoStretch */

static const GtkCssValueClass GTK_CSS_VALUE_FONT_STRETCH = {
  "GtkCssFontStretchValue",
  gtk_css_value_enum_free,
  gtk_css_value_enum_compute,
  gtk_css_value_enum_equal,
  gtk_css_value_enum_transition,
  NULL,
  NULL,
  gtk_css_value_enum_print
};

static GtkCssValue font_stretch_values[] = {
  { &GTK_CSS_VALUE_FONT_STRETCH, 1, TRUE, PANGO_STRETCH_ULTRA_CONDENSED, "ultra-condensed" },
  { &GTK_CSS_VALUE_FONT_STRETCH, 1, TRUE, PANGO_STRETCH_EXTRA_CONDENSED, "extra-condensed" },
  { &GTK_CSS_VALUE_FONT_STRETCH, 1, TRUE, PANGO_STRETCH_CONDENSED, "condensed" },
  { &GTK_CSS_VALUE_FONT_STRETCH, 1, TRUE, PANGO_STRETCH_SEMI_CONDENSED, "semi-condensed" },
  { &GTK_CSS_VALUE_FONT_STRETCH, 1, TRUE, PANGO_STRETCH_NORMAL, "normal" },
  { &GTK_CSS_VALUE_FONT_STRETCH, 1, TRUE, PANGO_STRETCH_SEMI_EXPANDED, "semi-expanded" },
  { &GTK_CSS_VALUE_FONT_STRETCH, 1, TRUE, PANGO_STRETCH_EXPANDED, "expanded" },
  { &GTK_CSS_VALUE_FONT_STRETCH, 1, TRUE, PANGO_STRETCH_EXTRA_EXPANDED, "extra-expanded" },
  { &GTK_CSS_VALUE_FONT_STRETCH, 1, TRUE, PANGO_STRETCH_ULTRA_EXPANDED, "ultra-expanded" },
};

GtkCssValue *
_gtk_css_font_stretch_value_new (PangoStretch font_stretch)
{
  g_return_val_if_fail (font_stretch < G_N_ELEMENTS (font_stretch_values), NULL);

  return _gtk_css_value_ref (&font_stretch_values[font_stretch]);
}

GtkCssValue *
_gtk_css_font_stretch_value_try_parse (GtkCssParser *parser)
{
  guint i;

  g_return_val_if_fail (parser != NULL, NULL);

  for (i = 0; i < G_N_ELEMENTS (font_stretch_values); i++)
    {
      if (gtk_css_parser_try_ident (parser, font_stretch_values[i].name))
        return _gtk_css_value_ref (&font_stretch_values[i]);
    }

  return NULL;
}

PangoStretch
_gtk_css_font_stretch_value_get (const GtkCssValue *value)
{
  g_return_val_if_fail (value->class == &GTK_CSS_VALUE_FONT_STRETCH, PANGO_STRETCH_NORMAL);

  return value->value;
}

/* GtkTextDecorationStyle */

static const GtkCssValueClass GTK_CSS_VALUE_TEXT_DECORATION_STYLE = {
  "GtkCssTextDecorationStyleValue",
  gtk_css_value_enum_free,
  gtk_css_value_enum_compute,
  gtk_css_value_enum_equal,
  gtk_css_value_enum_transition,
  NULL,
  NULL,
  gtk_css_value_enum_print
};

static GtkCssValue text_decoration_style_values[] = {
  { &GTK_CSS_VALUE_TEXT_DECORATION_STYLE, 1, TRUE, GTK_CSS_TEXT_DECORATION_STYLE_SOLID, "solid" },
  { &GTK_CSS_VALUE_TEXT_DECORATION_STYLE, 1, TRUE, GTK_CSS_TEXT_DECORATION_STYLE_DOUBLE, "double" },
  { &GTK_CSS_VALUE_TEXT_DECORATION_STYLE, 1, TRUE, GTK_CSS_TEXT_DECORATION_STYLE_WAVY, "wavy" },
};

GtkCssValue *
_gtk_css_text_decoration_style_value_new (GtkTextDecorationStyle style)
{
  g_return_val_if_fail (style < G_N_ELEMENTS (text_decoration_style_values), NULL);

  return _gtk_css_value_ref (&text_decoration_style_values[style]);
}

GtkCssValue *
_gtk_css_text_decoration_style_value_try_parse (GtkCssParser *parser)
{
  guint i;

  g_return_val_if_fail (parser != NULL, NULL);

  for (i = 0; i < G_N_ELEMENTS (text_decoration_style_values); i++)
    {
      if (gtk_css_parser_try_ident (parser, text_decoration_style_values[i].name))
        return _gtk_css_value_ref (&text_decoration_style_values[i]);
    }

  return NULL;
}

GtkTextDecorationStyle
_gtk_css_text_decoration_style_value_get (const GtkCssValue *value)
{
  g_return_val_if_fail (value->class == &GTK_CSS_VALUE_TEXT_DECORATION_STYLE, GTK_CSS_TEXT_DECORATION_STYLE_SOLID);

  return value->value;
}

/* GtkCssArea */

static const GtkCssValueClass GTK_CSS_VALUE_AREA = {
  "GtkCssAreaValue",
  gtk_css_value_enum_free,
  gtk_css_value_enum_compute,
  gtk_css_value_enum_equal,
  gtk_css_value_enum_transition,
  NULL,
  NULL,
  gtk_css_value_enum_print
};

static GtkCssValue area_values[] = {
  { &GTK_CSS_VALUE_AREA, 1, TRUE, GTK_CSS_AREA_BORDER_BOX, "border-box" },
  { &GTK_CSS_VALUE_AREA, 1, TRUE, GTK_CSS_AREA_PADDING_BOX, "padding-box" },
  { &GTK_CSS_VALUE_AREA, 1, TRUE, GTK_CSS_AREA_CONTENT_BOX, "content-box" }
};

GtkCssValue *
_gtk_css_area_value_new (GtkCssArea area)
{
  guint i;

  for (i = 0; i < G_N_ELEMENTS (area_values); i++)
    {
      if (area_values[i].value == area)
        return _gtk_css_value_ref (&area_values[i]);
    }

  g_return_val_if_reached (NULL);
}

GtkCssValue *
_gtk_css_area_value_try_parse (GtkCssParser *parser)
{
  guint i;

  g_return_val_if_fail (parser != NULL, NULL);

  for (i = 0; i < G_N_ELEMENTS (area_values); i++)
    {
      if (gtk_css_parser_try_ident (parser, area_values[i].name))
        return _gtk_css_value_ref (&area_values[i]);
    }

  return NULL;
}

GtkCssArea
_gtk_css_area_value_get (const GtkCssValue *value)
{
  g_return_val_if_fail (value->class == &GTK_CSS_VALUE_AREA, GTK_CSS_AREA_BORDER_BOX);

  return value->value;
}

/* GtkCssDirection */

static const GtkCssValueClass GTK_CSS_VALUE_DIRECTION = {
  "GtkCssDirectionValue",
  gtk_css_value_enum_free,
  gtk_css_value_enum_compute,
  gtk_css_value_enum_equal,
  gtk_css_value_enum_transition,
  NULL,
  NULL,
  gtk_css_value_enum_print
};

static GtkCssValue direction_values[] = {
  { &GTK_CSS_VALUE_DIRECTION, 1, TRUE, GTK_CSS_DIRECTION_NORMAL, "normal" },
  { &GTK_CSS_VALUE_DIRECTION, 1, TRUE, GTK_CSS_DIRECTION_REVERSE, "reverse" },
  { &GTK_CSS_VALUE_DIRECTION, 1, TRUE, GTK_CSS_DIRECTION_ALTERNATE, "alternate" },
  { &GTK_CSS_VALUE_DIRECTION, 1, TRUE, GTK_CSS_DIRECTION_ALTERNATE_REVERSE, "alternate-reverse" }
};

GtkCssValue *
_gtk_css_direction_value_new (GtkCssDirection direction)
{
  guint i;

  for (i = 0; i < G_N_ELEMENTS (direction_values); i++)
    {
      if (direction_values[i].value == direction)
        return _gtk_css_value_ref (&direction_values[i]);
    }

  g_return_val_if_reached (NULL);
}

GtkCssValue *
_gtk_css_direction_value_try_parse (GtkCssParser *parser)
{
  int i;

  g_return_val_if_fail (parser != NULL, NULL);

  /* need to parse backwards here, otherwise "alternate" will also match "alternate-reverse".
   * Our parser rocks!
   */
  for (i = G_N_ELEMENTS (direction_values) - 1; i >= 0; i--)
    {
      if (gtk_css_parser_try_ident (parser, direction_values[i].name))
        return _gtk_css_value_ref (&direction_values[i]);
    }

  return NULL;
}

GtkCssDirection
_gtk_css_direction_value_get (const GtkCssValue *value)
{
  g_return_val_if_fail (value->class == &GTK_CSS_VALUE_DIRECTION, GTK_CSS_DIRECTION_NORMAL);

  return value->value;
}

/* GtkCssPlayState */

static const GtkCssValueClass GTK_CSS_VALUE_PLAY_STATE = {
  "GtkCssPlayStateValue",
  gtk_css_value_enum_free,
  gtk_css_value_enum_compute,
  gtk_css_value_enum_equal,
  gtk_css_value_enum_transition,
  NULL,
  NULL,
  gtk_css_value_enum_print
};

static GtkCssValue play_state_values[] = {
  { &GTK_CSS_VALUE_PLAY_STATE, 1, TRUE, GTK_CSS_PLAY_STATE_RUNNING, "running" },
  { &GTK_CSS_VALUE_PLAY_STATE, 1, TRUE, GTK_CSS_PLAY_STATE_PAUSED, "paused" }
};

GtkCssValue *
_gtk_css_play_state_value_new (GtkCssPlayState play_state)
{
  guint i;

  for (i = 0; i < G_N_ELEMENTS (play_state_values); i++)
    {
      if (play_state_values[i].value == play_state)
        return _gtk_css_value_ref (&play_state_values[i]);
    }

  g_return_val_if_reached (NULL);
}

GtkCssValue *
_gtk_css_play_state_value_try_parse (GtkCssParser *parser)
{
  guint i;

  g_return_val_if_fail (parser != NULL, NULL);

  for (i = 0; i < G_N_ELEMENTS (play_state_values); i++)
    {
      if (gtk_css_parser_try_ident (parser, play_state_values[i].name))
        return _gtk_css_value_ref (&play_state_values[i]);
    }

  return NULL;
}

GtkCssPlayState
_gtk_css_play_state_value_get (const GtkCssValue *value)
{
  g_return_val_if_fail (value->class == &GTK_CSS_VALUE_PLAY_STATE, GTK_CSS_PLAY_STATE_RUNNING);

  return value->value;
}

/* GtkCssFillMode */

static const GtkCssValueClass GTK_CSS_VALUE_FILL_MODE = {
  "GtkCssFillModeValue",
  gtk_css_value_enum_free,
  gtk_css_value_enum_compute,
  gtk_css_value_enum_equal,
  gtk_css_value_enum_transition,
  NULL,
  NULL,
  gtk_css_value_enum_print
};

static GtkCssValue fill_mode_values[] = {
  { &GTK_CSS_VALUE_FILL_MODE, 1, TRUE, GTK_CSS_FILL_NONE, "none" },
  { &GTK_CSS_VALUE_FILL_MODE, 1, TRUE, GTK_CSS_FILL_FORWARDS, "forwards" },
  { &GTK_CSS_VALUE_FILL_MODE, 1, TRUE, GTK_CSS_FILL_BACKWARDS, "backwards" },
  { &GTK_CSS_VALUE_FILL_MODE, 1, TRUE, GTK_CSS_FILL_BOTH, "both" }
};

GtkCssValue *
_gtk_css_fill_mode_value_new (GtkCssFillMode fill_mode)
{
  guint i;

  for (i = 0; i < G_N_ELEMENTS (fill_mode_values); i++)
    {
      if (fill_mode_values[i].value == fill_mode)
        return _gtk_css_value_ref (&fill_mode_values[i]);
    }

  g_return_val_if_reached (NULL);
}

GtkCssValue *
_gtk_css_fill_mode_value_try_parse (GtkCssParser *parser)
{
  guint i;

  g_return_val_if_fail (parser != NULL, NULL);

  for (i = 0; i < G_N_ELEMENTS (fill_mode_values); i++)
    {
      if (gtk_css_parser_try_ident (parser, fill_mode_values[i].name))
        return _gtk_css_value_ref (&fill_mode_values[i]);
    }

  return NULL;
}

GtkCssFillMode
_gtk_css_fill_mode_value_get (const GtkCssValue *value)
{
  g_return_val_if_fail (value->class == &GTK_CSS_VALUE_FILL_MODE, GTK_CSS_FILL_NONE);

  return value->value;
}

/* GtkCssIconStyle */

static const GtkCssValueClass GTK_CSS_VALUE_ICON_STYLE = {
  "GtkCssIconStyleValue",
  gtk_css_value_enum_free,
  gtk_css_value_enum_compute,
  gtk_css_value_enum_equal,
  gtk_css_value_enum_transition,
  NULL,
  NULL,
  gtk_css_value_enum_print
};

static GtkCssValue icon_style_values[] = {
  { &GTK_CSS_VALUE_ICON_STYLE, 1, TRUE, GTK_CSS_ICON_STYLE_REQUESTED, "requested" },
  { &GTK_CSS_VALUE_ICON_STYLE, 1, TRUE, GTK_CSS_ICON_STYLE_REGULAR, "regular" },
  { &GTK_CSS_VALUE_ICON_STYLE, 1, TRUE, GTK_CSS_ICON_STYLE_SYMBOLIC, "symbolic" }
};

GtkCssValue *
_gtk_css_icon_style_value_new (GtkCssIconStyle icon_style)
{
  guint i;

  for (i = 0; i < G_N_ELEMENTS (icon_style_values); i++)
    {
      if (icon_style_values[i].value == icon_style)
        return _gtk_css_value_ref (&icon_style_values[i]);
    }

  g_return_val_if_reached (NULL);
}

GtkCssValue *
_gtk_css_icon_style_value_try_parse (GtkCssParser *parser)
{
  guint i;

  g_return_val_if_fail (parser != NULL, NULL);

  for (i = 0; i < G_N_ELEMENTS (icon_style_values); i++)
    {
      if (gtk_css_parser_try_ident (parser, icon_style_values[i].name))
        return _gtk_css_value_ref (&icon_style_values[i]);
    }

  return NULL;
}

GtkCssIconStyle
_gtk_css_icon_style_value_get (const GtkCssValue *value)
{
  g_return_val_if_fail (value->class == &GTK_CSS_VALUE_ICON_STYLE, GTK_CSS_ICON_STYLE_REQUESTED);

  return value->value;
}

/* GtkCssFontKerning */

static const GtkCssValueClass GTK_CSS_VALUE_FONT_KERNING = {
  "GtkCssFontKerningValue",
  gtk_css_value_enum_free,
  gtk_css_value_enum_compute,
  gtk_css_value_enum_equal,
  gtk_css_value_enum_transition,
  NULL,
  NULL,
  gtk_css_value_enum_print
};

static GtkCssValue font_kerning_values[] = {
  { &GTK_CSS_VALUE_FONT_KERNING, 1, TRUE, GTK_CSS_FONT_KERNING_AUTO, "auto" },
  { &GTK_CSS_VALUE_FONT_KERNING, 1, TRUE, GTK_CSS_FONT_KERNING_NORMAL, "normal" },
  { &GTK_CSS_VALUE_FONT_KERNING, 1, TRUE, GTK_CSS_FONT_KERNING_NONE, "none" }
};

GtkCssValue *
_gtk_css_font_kerning_value_new (GtkCssFontKerning kerning)
{
  guint i;

  for (i = 0; i < G_N_ELEMENTS (font_kerning_values); i++)
    {
      if (font_kerning_values[i].value == kerning)
        return _gtk_css_value_ref (&font_kerning_values[i]);
    }

  g_return_val_if_reached (NULL);
}

GtkCssValue *
_gtk_css_font_kerning_value_try_parse (GtkCssParser *parser)
{
  guint i;

  g_return_val_if_fail (parser != NULL, NULL);

  for (i = 0; i < G_N_ELEMENTS (font_kerning_values); i++)
    {
      if (gtk_css_parser_try_ident (parser, font_kerning_values[i].name))
        return _gtk_css_value_ref (&font_kerning_values[i]);
    }

  return NULL;
}

GtkCssFontKerning
_gtk_css_font_kerning_value_get (const GtkCssValue *value)
{
  g_return_val_if_fail (value->class == &GTK_CSS_VALUE_FONT_KERNING, GTK_CSS_FONT_KERNING_AUTO);

  return value->value;
}

/* GtkCssFontVariantPos */

static const GtkCssValueClass GTK_CSS_VALUE_FONT_VARIANT_POSITION = {
  "GtkCssFontVariationPositionValue",
  gtk_css_value_enum_free,
  gtk_css_value_enum_compute,
  gtk_css_value_enum_equal,
  gtk_css_value_enum_transition,
  NULL,
  NULL,
  gtk_css_value_enum_print
};

static GtkCssValue font_variant_position_values[] = {
  { &GTK_CSS_VALUE_FONT_VARIANT_POSITION, 1, TRUE, GTK_CSS_FONT_VARIANT_POSITION_NORMAL, "normal" },
  { &GTK_CSS_VALUE_FONT_VARIANT_POSITION, 1, TRUE, GTK_CSS_FONT_VARIANT_POSITION_SUB, "sub" },
  { &GTK_CSS_VALUE_FONT_VARIANT_POSITION, 1, TRUE, GTK_CSS_FONT_VARIANT_POSITION_SUPER, "super" }
};

GtkCssValue *
_gtk_css_font_variant_position_value_new (GtkCssFontVariantPosition position)
{
  guint i;

  for (i = 0; i < G_N_ELEMENTS (font_variant_position_values); i++)
    {
      if (font_variant_position_values[i].value == position)
        return _gtk_css_value_ref (&font_variant_position_values[i]);
    }

  g_return_val_if_reached (NULL);
}

GtkCssValue *
_gtk_css_font_variant_position_value_try_parse (GtkCssParser *parser)
{
  guint i;

  g_return_val_if_fail (parser != NULL, NULL);

  for (i = 0; i < G_N_ELEMENTS (font_variant_position_values); i++)
    {
      if (gtk_css_parser_try_ident (parser, font_variant_position_values[i].name))
        return _gtk_css_value_ref (&font_variant_position_values[i]);
    }

  return NULL;
}

GtkCssFontVariantPosition
_gtk_css_font_variant_position_value_get (const GtkCssValue *value)
{
  g_return_val_if_fail (value->class == &GTK_CSS_VALUE_FONT_VARIANT_POSITION, GTK_CSS_FONT_VARIANT_POSITION_NORMAL);

  return value->value;
}

/* GtkCssFontVariantCaps */

static const GtkCssValueClass GTK_CSS_VALUE_FONT_VARIANT_CAPS = {
  "GtkCssFontVariantCapsValue",
  gtk_css_value_enum_free,
  gtk_css_value_enum_compute,
  gtk_css_value_enum_equal,
  gtk_css_value_enum_transition,
  NULL,
  NULL,
  gtk_css_value_enum_print
};

static GtkCssValue font_variant_caps_values[] = {
  { &GTK_CSS_VALUE_FONT_VARIANT_CAPS, 1, TRUE, GTK_CSS_FONT_VARIANT_CAPS_NORMAL, "normal" },
  { &GTK_CSS_VALUE_FONT_VARIANT_CAPS, 1, TRUE, GTK_CSS_FONT_VARIANT_CAPS_SMALL_CAPS, "small-caps" },
  { &GTK_CSS_VALUE_FONT_VARIANT_CAPS, 1, TRUE, GTK_CSS_FONT_VARIANT_CAPS_ALL_SMALL_CAPS, "all-small-caps" },
  { &GTK_CSS_VALUE_FONT_VARIANT_CAPS, 1, TRUE, GTK_CSS_FONT_VARIANT_CAPS_PETITE_CAPS, "petite-caps" },
  { &GTK_CSS_VALUE_FONT_VARIANT_CAPS, 1, TRUE, GTK_CSS_FONT_VARIANT_CAPS_ALL_PETITE_CAPS, "all-petite-caps" },
  { &GTK_CSS_VALUE_FONT_VARIANT_CAPS, 1, TRUE, GTK_CSS_FONT_VARIANT_CAPS_UNICASE, "unicase" },
  { &GTK_CSS_VALUE_FONT_VARIANT_CAPS, 1, TRUE, GTK_CSS_FONT_VARIANT_CAPS_TITLING_CAPS, "titling-caps" }
};

GtkCssValue *
_gtk_css_font_variant_caps_value_new (GtkCssFontVariantCaps caps)
{
  guint i;

  for (i = 0; i < G_N_ELEMENTS (font_variant_caps_values); i++)
    {
      if (font_variant_caps_values[i].value == caps)
        return _gtk_css_value_ref (&font_variant_caps_values[i]);
    }

  g_return_val_if_reached (NULL);
}

GtkCssValue *
_gtk_css_font_variant_caps_value_try_parse (GtkCssParser *parser)
{
  guint i;

  g_return_val_if_fail (parser != NULL, NULL);

  for (i = 0; i < G_N_ELEMENTS (font_variant_caps_values); i++)
    {
      if (gtk_css_parser_try_ident (parser, font_variant_caps_values[i].name))
        return _gtk_css_value_ref (&font_variant_caps_values[i]);
    }

  return NULL;
}

GtkCssFontVariantCaps
_gtk_css_font_variant_caps_value_get (const GtkCssValue *value)
{
  g_return_val_if_fail (value->class == &GTK_CSS_VALUE_FONT_VARIANT_CAPS, GTK_CSS_FONT_VARIANT_CAPS_NORMAL);

  return value->value;
}

/* GtkCssFontVariantAlternate */

static const GtkCssValueClass GTK_CSS_VALUE_FONT_VARIANT_ALTERNATE = {
  "GtkCssFontVariantAlternateValue",
  gtk_css_value_enum_free,
  gtk_css_value_enum_compute,
  gtk_css_value_enum_equal,
  gtk_css_value_enum_transition,
  NULL,
  NULL,
  gtk_css_value_enum_print
};

static GtkCssValue font_variant_alternate_values[] = {
  { &GTK_CSS_VALUE_FONT_VARIANT_ALTERNATE, 1, TRUE, GTK_CSS_FONT_VARIANT_ALTERNATE_NORMAL, "normal" },
  { &GTK_CSS_VALUE_FONT_VARIANT_ALTERNATE, 1, TRUE, GTK_CSS_FONT_VARIANT_ALTERNATE_HISTORICAL_FORMS, "historical-forms" }
};

GtkCssValue *
_gtk_css_font_variant_alternate_value_new (GtkCssFontVariantAlternate alternate)
{
  guint i;

  for (i = 0; i < G_N_ELEMENTS (font_variant_alternate_values); i++)
    {
      if (font_variant_alternate_values[i].value == alternate)
        return _gtk_css_value_ref (&font_variant_alternate_values[i]);
    }

  g_return_val_if_reached (NULL);
}

GtkCssValue *
_gtk_css_font_variant_alternate_value_try_parse (GtkCssParser *parser)
{
  guint i;

  g_return_val_if_fail (parser != NULL, NULL);

  for (i = 0; i < G_N_ELEMENTS (font_variant_alternate_values); i++)
    {
      if (gtk_css_parser_try_ident (parser, font_variant_alternate_values[i].name))
        return _gtk_css_value_ref (&font_variant_alternate_values[i]);
    }

  return NULL;
}

GtkCssFontVariantAlternate
_gtk_css_font_variant_alternate_value_get (const GtkCssValue *value)
{
  g_return_val_if_fail (value->class == &GTK_CSS_VALUE_FONT_VARIANT_ALTERNATE, GTK_CSS_FONT_VARIANT_ALTERNATE_NORMAL);

  return value->value;
}

/* below are flags, which are handle a bit differently. We allocate values dynamically,
 * and we parse one bit at a time, while allowing for detection of invalid combinations.
 */

typedef struct {
  int value;
  const char *name;
} FlagsValue;

static gboolean
gtk_css_value_flags_equal (const GtkCssValue *enum1,
                           const GtkCssValue *enum2)
{
  return enum1->value == enum2->value;
}

static void
gtk_css_value_flags_print (const FlagsValue  *values,
                           guint              n_values,
                           const GtkCssValue *value,
                           GString           *string)
{
  guint i;
  const char *sep = "";

  for (i = 0; i < n_values; i++)
    {
      if (value->value & values[i].value)
        {
          g_string_append (string, sep);
          g_string_append (string, values[i].name);
          sep = " ";
        }
    }
}

/* GtkTextDecorationLine */

static FlagsValue text_decoration_line_values[] = {
  { GTK_CSS_TEXT_DECORATION_LINE_NONE, "none" },
  { GTK_CSS_TEXT_DECORATION_LINE_UNDERLINE, "underline" },
  { GTK_CSS_TEXT_DECORATION_LINE_OVERLINE, "overline" },
  { GTK_CSS_TEXT_DECORATION_LINE_LINE_THROUGH, "line-through" },
};

static void
gtk_css_text_decoration_line_value_print (const GtkCssValue *value,
                                          GString           *string)
{
  gtk_css_value_flags_print (text_decoration_line_values,
                             G_N_ELEMENTS (text_decoration_line_values),
                             value, string);
}

static const GtkCssValueClass GTK_CSS_VALUE_TEXT_DECORATION_LINE = {
  "GtkCssTextDecorationLine",
  gtk_css_value_enum_free,
  gtk_css_value_enum_compute,
  gtk_css_value_flags_equal,
  gtk_css_value_enum_transition,
  NULL,
  NULL,
  gtk_css_text_decoration_line_value_print
};

static gboolean
text_decoration_line_is_valid (GtkTextDecorationLine line)
{
  if ((line & GTK_CSS_TEXT_DECORATION_LINE_NONE) &&
      (line != GTK_CSS_TEXT_DECORATION_LINE_NONE))
    return FALSE;

  return TRUE;
}

GtkCssValue *
_gtk_css_text_decoration_line_value_new (GtkTextDecorationLine line)
{
  GtkCssValue *value;

  if (!text_decoration_line_is_valid (line))
    return NULL;

  value = _gtk_css_value_new (GtkCssValue, &GTK_CSS_VALUE_TEXT_DECORATION_LINE);
  value->value = line;
  value->name = NULL;
  value->is_computed = TRUE;

  return value;
}

GtkTextDecorationLine
_gtk_css_text_decoration_line_try_parse_one (GtkCssParser          *parser,
                                             GtkTextDecorationLine  base)
{
  guint i;
  GtkTextDecorationLine value = 0;

  g_return_val_if_fail (parser != NULL, 0);

  for (i = 0; i < G_N_ELEMENTS (text_decoration_line_values); i++)
    {
      if (gtk_css_parser_try_ident (parser, text_decoration_line_values[i].name))
        {
          value = text_decoration_line_values[i].value;
          break;
        }
    }

  if (value == 0)
    return base; /* not parsing this value */

  if ((base | value) == base)
    return 0; /* repeated value */

  if (!text_decoration_line_is_valid (base | value))
    return 0; /* bad combination */

  return base | value;
}

GtkTextDecorationLine
_gtk_css_text_decoration_line_value_get (const GtkCssValue *value)
{
  g_return_val_if_fail (value->class == &GTK_CSS_VALUE_TEXT_DECORATION_LINE, GTK_CSS_TEXT_DECORATION_LINE_NONE);

  return value->value;
}

/* GtkCssFontVariantLigature */

static FlagsValue font_variant_ligature_values[] = {
  { GTK_CSS_FONT_VARIANT_LIGATURE_NORMAL, "normal" },
  { GTK_CSS_FONT_VARIANT_LIGATURE_NONE, "none" },
  { GTK_CSS_FONT_VARIANT_LIGATURE_COMMON_LIGATURES, "common-ligatures" },
  { GTK_CSS_FONT_VARIANT_LIGATURE_NO_COMMON_LIGATURES, "no-common-ligatures" },
  { GTK_CSS_FONT_VARIANT_LIGATURE_DISCRETIONARY_LIGATURES, "discretionary-ligatures" },
  { GTK_CSS_FONT_VARIANT_LIGATURE_NO_DISCRETIONARY_LIGATURES, "no-discretionary-ligatures" },
  { GTK_CSS_FONT_VARIANT_LIGATURE_HISTORICAL_LIGATURES, "historical-ligatures" },
  { GTK_CSS_FONT_VARIANT_LIGATURE_NO_HISTORICAL_LIGATURES, "no-historical-ligatures" },
  { GTK_CSS_FONT_VARIANT_LIGATURE_CONTEXTUAL, "contextual" },
  { GTK_CSS_FONT_VARIANT_LIGATURE_NO_CONTEXTUAL, "no-contextual" }
};

static void
gtk_css_font_variant_ligature_value_print (const GtkCssValue *value,
                                           GString           *string)
{
  gtk_css_value_flags_print (font_variant_ligature_values,
                             G_N_ELEMENTS (font_variant_ligature_values),
                             value, string);
}

static const GtkCssValueClass GTK_CSS_VALUE_FONT_VARIANT_LIGATURE = {
  "GtkCssFontVariantLigatureValue",
  gtk_css_value_enum_free,
  gtk_css_value_enum_compute,
  gtk_css_value_flags_equal,
  gtk_css_value_enum_transition,
  NULL,
  NULL,
  gtk_css_font_variant_ligature_value_print
};

static gboolean
ligature_value_is_valid (GtkCssFontVariantLigature ligatures)
{
  if (((ligatures & GTK_CSS_FONT_VARIANT_LIGATURE_NORMAL) &&
       (ligatures != GTK_CSS_FONT_VARIANT_LIGATURE_NORMAL)) ||
      ((ligatures & GTK_CSS_FONT_VARIANT_LIGATURE_NONE) &&
       (ligatures != GTK_CSS_FONT_VARIANT_LIGATURE_NONE)) ||
      ((ligatures & GTK_CSS_FONT_VARIANT_LIGATURE_COMMON_LIGATURES) &&
       (ligatures & GTK_CSS_FONT_VARIANT_LIGATURE_NO_COMMON_LIGATURES)) ||
      ((ligatures & GTK_CSS_FONT_VARIANT_LIGATURE_DISCRETIONARY_LIGATURES) &&
       (ligatures & GTK_CSS_FONT_VARIANT_LIGATURE_NO_DISCRETIONARY_LIGATURES)) ||
      ((ligatures & GTK_CSS_FONT_VARIANT_LIGATURE_HISTORICAL_LIGATURES) &&
       (ligatures & GTK_CSS_FONT_VARIANT_LIGATURE_NO_HISTORICAL_LIGATURES)) ||
      ((ligatures & GTK_CSS_FONT_VARIANT_LIGATURE_CONTEXTUAL) &&
       (ligatures & GTK_CSS_FONT_VARIANT_LIGATURE_NO_CONTEXTUAL)))
    return FALSE;

  return TRUE;
}

GtkCssValue *
_gtk_css_font_variant_ligature_value_new (GtkCssFontVariantLigature ligatures)
{
  GtkCssValue *value;

  if (!ligature_value_is_valid (ligatures))
    return NULL;

  value = _gtk_css_value_new (GtkCssValue, &GTK_CSS_VALUE_FONT_VARIANT_LIGATURE);
  value->value = ligatures;
  value->name = NULL;
  value->is_computed = TRUE;

  return value;
}

GtkCssFontVariantLigature
_gtk_css_font_variant_ligature_try_parse_one (GtkCssParser              *parser,
                                              GtkCssFontVariantLigature  base)
{
  guint i;
  GtkCssFontVariantLigature value = 0;

  g_return_val_if_fail (parser != NULL, 0);

  for (i = 0; i < G_N_ELEMENTS (font_variant_ligature_values); i++)
    {
      if (gtk_css_parser_try_ident (parser, font_variant_ligature_values[i].name))
        {
          value = font_variant_ligature_values[i].value;
          break;
        }
    }

  if (value == 0)
    return base; /* not parsing this value */

  if ((base | value) == base)
    return 0; /* repeated value */

  if (!ligature_value_is_valid (base | value))
    return 0; /* bad combination */

  return base | value;
}

GtkCssFontVariantLigature
_gtk_css_font_variant_ligature_value_get (const GtkCssValue *value)
{
  g_return_val_if_fail (value->class == &GTK_CSS_VALUE_FONT_VARIANT_LIGATURE, GTK_CSS_FONT_VARIANT_LIGATURE_NORMAL);

  return value->value;
}

/* GtkCssFontVariantNumeric */

static FlagsValue font_variant_numeric_values[] = {
  { GTK_CSS_FONT_VARIANT_NUMERIC_NORMAL, "normal" },
  { GTK_CSS_FONT_VARIANT_NUMERIC_LINING_NUMS, "lining-nums" },
  { GTK_CSS_FONT_VARIANT_NUMERIC_OLDSTYLE_NUMS, "oldstyle-nums" },
  { GTK_CSS_FONT_VARIANT_NUMERIC_PROPORTIONAL_NUMS, "proportional-nums" },
  { GTK_CSS_FONT_VARIANT_NUMERIC_TABULAR_NUMS, "tabular-nums" },
  { GTK_CSS_FONT_VARIANT_NUMERIC_DIAGONAL_FRACTIONS, "diagonal-fractions" },
  { GTK_CSS_FONT_VARIANT_NUMERIC_STACKED_FRACTIONS, "stacked-fractions" },
  { GTK_CSS_FONT_VARIANT_NUMERIC_ORDINAL, "ordinal" },
  { GTK_CSS_FONT_VARIANT_NUMERIC_SLASHED_ZERO, "slashed-zero" }
};

static void
gtk_css_font_variant_numeric_value_print (const GtkCssValue *value,
                                          GString           *string)
{
  gtk_css_value_flags_print (font_variant_numeric_values,
                             G_N_ELEMENTS (font_variant_numeric_values),
                             value, string);
}

static const GtkCssValueClass GTK_CSS_VALUE_FONT_VARIANT_NUMERIC = {
  "GtkCssFontVariantNumbericValue",
  gtk_css_value_enum_free,
  gtk_css_value_enum_compute,
  gtk_css_value_flags_equal,
  gtk_css_value_enum_transition,
  NULL,
  NULL,
  gtk_css_font_variant_numeric_value_print
};

static gboolean
numeric_value_is_valid (GtkCssFontVariantNumeric numeric)
{
  if (((numeric & GTK_CSS_FONT_VARIANT_NUMERIC_NORMAL) &&
       (numeric != GTK_CSS_FONT_VARIANT_NUMERIC_NORMAL)) ||
      ((numeric & GTK_CSS_FONT_VARIANT_NUMERIC_LINING_NUMS) &&
       (numeric & GTK_CSS_FONT_VARIANT_NUMERIC_OLDSTYLE_NUMS)) ||
      ((numeric & GTK_CSS_FONT_VARIANT_NUMERIC_PROPORTIONAL_NUMS) &&
       (numeric & GTK_CSS_FONT_VARIANT_NUMERIC_TABULAR_NUMS)) ||
      ((numeric & GTK_CSS_FONT_VARIANT_NUMERIC_DIAGONAL_FRACTIONS) &&
       (numeric & GTK_CSS_FONT_VARIANT_NUMERIC_STACKED_FRACTIONS)))
    return FALSE;

  return TRUE;
}

GtkCssValue *
_gtk_css_font_variant_numeric_value_new (GtkCssFontVariantNumeric numeric)
{
  GtkCssValue *value;

  if (!numeric_value_is_valid (numeric))
    return NULL;

  value = _gtk_css_value_new (GtkCssValue, &GTK_CSS_VALUE_FONT_VARIANT_NUMERIC);
  value->value = numeric;
  value->name = NULL;
  value->is_computed = TRUE;

  return value;
}

GtkCssFontVariantNumeric
_gtk_css_font_variant_numeric_try_parse_one (GtkCssParser             *parser,
                                             GtkCssFontVariantNumeric  base)
{
  guint i;
  GtkCssFontVariantNumeric value = 0;

  g_return_val_if_fail (parser != NULL, 0);

  for (i = 0; i < G_N_ELEMENTS (font_variant_numeric_values); i++)
    {
      if (gtk_css_parser_try_ident (parser, font_variant_numeric_values[i].name))
        {
          value = font_variant_numeric_values[i].value;
          break;
        }
    }

  if (value == 0)
    return base; /* not parsing this value */

  if ((base | value) == base)
    return 0; /* repeated value */

  if (!numeric_value_is_valid (base | value))
    return 0; /* bad combination */

  return base | value;
}

GtkCssFontVariantNumeric
_gtk_css_font_variant_numeric_value_get (const GtkCssValue *value)
{
  g_return_val_if_fail (value->class == &GTK_CSS_VALUE_FONT_VARIANT_NUMERIC, GTK_CSS_FONT_VARIANT_NUMERIC_NORMAL);

  return value->value;
}

/* GtkCssFontVariantEastAsian */

static FlagsValue font_variant_east_asian_values[] = {
  { GTK_CSS_FONT_VARIANT_EAST_ASIAN_NORMAL, "normal" },
  { GTK_CSS_FONT_VARIANT_EAST_ASIAN_JIS78, "jis78" },
  { GTK_CSS_FONT_VARIANT_EAST_ASIAN_JIS83, "jis83" },
  { GTK_CSS_FONT_VARIANT_EAST_ASIAN_JIS90, "jis90" },
  { GTK_CSS_FONT_VARIANT_EAST_ASIAN_JIS04, "jis04" },
  { GTK_CSS_FONT_VARIANT_EAST_ASIAN_SIMPLIFIED, "simplified" },
  { GTK_CSS_FONT_VARIANT_EAST_ASIAN_TRADITIONAL, "traditional" },
  { GTK_CSS_FONT_VARIANT_EAST_ASIAN_FULL_WIDTH, "full-width" },
  { GTK_CSS_FONT_VARIANT_EAST_ASIAN_PROPORTIONAL, "proportional-width" },
  { GTK_CSS_FONT_VARIANT_EAST_ASIAN_RUBY, "ruby" }
};

static void
gtk_css_font_variant_east_asian_value_print (const GtkCssValue *value,
                                             GString           *string)
{
  gtk_css_value_flags_print (font_variant_east_asian_values,
                             G_N_ELEMENTS (font_variant_east_asian_values),
                             value, string);
}

static const GtkCssValueClass GTK_CSS_VALUE_FONT_VARIANT_EAST_ASIAN = {
  "GtkCssFontVariantEastAsianValue",
  gtk_css_value_enum_free,
  gtk_css_value_enum_compute,
  gtk_css_value_flags_equal,
  gtk_css_value_enum_transition,
  NULL,
  NULL,
  gtk_css_font_variant_east_asian_value_print
};

static gboolean
east_asian_value_is_valid (GtkCssFontVariantEastAsian east_asian)
{
  if ((east_asian & GTK_CSS_FONT_VARIANT_EAST_ASIAN_NORMAL) &&
      (east_asian != GTK_CSS_FONT_VARIANT_EAST_ASIAN_NORMAL))
    return FALSE;

  if (gtk_popcount (east_asian & (GTK_CSS_FONT_VARIANT_EAST_ASIAN_JIS78 |
                                  GTK_CSS_FONT_VARIANT_EAST_ASIAN_JIS83 |
                                  GTK_CSS_FONT_VARIANT_EAST_ASIAN_JIS90 |
                                  GTK_CSS_FONT_VARIANT_EAST_ASIAN_JIS04 |
                                  GTK_CSS_FONT_VARIANT_EAST_ASIAN_SIMPLIFIED |
                                  GTK_CSS_FONT_VARIANT_EAST_ASIAN_TRADITIONAL)) > 1)
    return FALSE;

  if (gtk_popcount (east_asian & (GTK_CSS_FONT_VARIANT_EAST_ASIAN_FULL_WIDTH |
                                  GTK_CSS_FONT_VARIANT_EAST_ASIAN_PROPORTIONAL)) > 1)
    return FALSE;

  return TRUE;
}

GtkCssValue *
_gtk_css_font_variant_east_asian_value_new (GtkCssFontVariantEastAsian east_asian)
{
  GtkCssValue *value;

  if (!east_asian_value_is_valid (east_asian))
    return NULL;

  value = _gtk_css_value_new (GtkCssValue, &GTK_CSS_VALUE_FONT_VARIANT_EAST_ASIAN);
  value->value = east_asian;
  value->name = NULL;
  value->is_computed = TRUE;

  return value;
}

GtkCssFontVariantEastAsian
_gtk_css_font_variant_east_asian_try_parse_one (GtkCssParser               *parser,
                                                GtkCssFontVariantEastAsian  base)
{
  guint i;
  GtkCssFontVariantEastAsian value = 0;

  g_return_val_if_fail (parser != NULL, 0);

  for (i = 0; i < G_N_ELEMENTS (font_variant_east_asian_values); i++)
    {
      if (gtk_css_parser_try_ident (parser, font_variant_east_asian_values[i].name))
        {
          value = font_variant_east_asian_values[i].value;
          break;
        }
    }

  if (value == 0)
    return base; /* not parsing this value */

  if ((base | value) == base)
    return 0; /* repeated value */

  if (!east_asian_value_is_valid (base | value))
    return 0; /* bad combination */

  return base | value;
}

GtkCssFontVariantEastAsian
_gtk_css_font_variant_east_asian_value_get (const GtkCssValue *value)
{
  g_return_val_if_fail (value->class == &GTK_CSS_VALUE_FONT_VARIANT_EAST_ASIAN, GTK_CSS_FONT_VARIANT_EAST_ASIAN_NORMAL);

  return value->value;
}

/* GtkTextTransform */

static const GtkCssValueClass GTK_CSS_VALUE_TEXT_TRANSFORM = {
  "GtkCssTextTransformValue",
  gtk_css_value_enum_free,
  gtk_css_value_enum_compute,
  gtk_css_value_enum_equal,
  gtk_css_value_enum_transition,
  NULL,
  NULL,
  gtk_css_value_enum_print
};

static GtkCssValue text_transform_values[] = {
  { &GTK_CSS_VALUE_TEXT_TRANSFORM, 1, TRUE, GTK_CSS_TEXT_TRANSFORM_NONE, "none" },
  { &GTK_CSS_VALUE_TEXT_TRANSFORM, 1, TRUE, GTK_CSS_TEXT_TRANSFORM_LOWERCASE, "lowercase" },
  { &GTK_CSS_VALUE_TEXT_TRANSFORM, 1, TRUE, GTK_CSS_TEXT_TRANSFORM_UPPERCASE, "uppercase" },
  { &GTK_CSS_VALUE_TEXT_TRANSFORM, 1, TRUE, GTK_CSS_TEXT_TRANSFORM_CAPITALIZE, "capitalize" },
};

GtkCssValue *
_gtk_css_text_transform_value_new (GtkTextTransform transform)
{
  g_return_val_if_fail (transform < G_N_ELEMENTS (text_transform_values), NULL);

  return _gtk_css_value_ref (&text_transform_values[transform]);
}

GtkCssValue *
_gtk_css_text_transform_value_try_parse (GtkCssParser *parser)
{
  guint i;

  g_return_val_if_fail (parser != NULL, NULL);

  for (i = 0; i < G_N_ELEMENTS (text_transform_values); i++)
    {
      if (gtk_css_parser_try_ident (parser, text_transform_values[i].name))
        return _gtk_css_value_ref (&text_transform_values[i]);
    }

  return NULL;
}

GtkTextTransform
_gtk_css_text_transform_value_get (const GtkCssValue *value)
{
  g_return_val_if_fail (value->class == &GTK_CSS_VALUE_TEXT_TRANSFORM, GTK_CSS_TEXT_TRANSFORM_NONE);

  return value->value;
}
