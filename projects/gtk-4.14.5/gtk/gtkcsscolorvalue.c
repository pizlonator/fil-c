/* GTK - The GIMP Toolkit
 * Copyright (C) 2010 Carlos Garnacho <carlosg@gnome.org>
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

#include "gtkcsscolorvalueprivate.h"

#include "gtkcssstylepropertyprivate.h"
#include "gtkprivate.h"
#include "gtkstylepropertyprivate.h"
#include "gtkcssstyleprivate.h"
#include "gtkstyleproviderprivate.h"

#include "gdk/gdkhslaprivate.h"
#include "gdk/gdkrgbaprivate.h"

typedef enum {
  COLOR_TYPE_LITERAL,
  COLOR_TYPE_NAME,
  COLOR_TYPE_SHADE,
  COLOR_TYPE_ALPHA,
  COLOR_TYPE_MIX,
  COLOR_TYPE_CURRENT_COLOR
} ColorType;

struct _GtkCssValue
{
  GTK_CSS_VALUE_BASE
  ColorType type;
  GtkCssValue *last_value;

  union
  {
    char *name;
    GdkRGBA rgba;

    struct
    {
      GtkCssValue *color;
      double factor;
    } shade, alpha;

    struct
    {
      GtkCssValue *color1;
      GtkCssValue *color2;
      double factor;
    } mix;
  } sym_col;
};

static void
gtk_css_value_color_free (GtkCssValue *color)
{
  if (color->last_value)
    _gtk_css_value_unref (color->last_value);

  switch (color->type)
    {
    case COLOR_TYPE_NAME:
      g_free (color->sym_col.name);
      break;
    case COLOR_TYPE_SHADE:
      _gtk_css_value_unref (color->sym_col.shade.color);
      break;
    case COLOR_TYPE_ALPHA:
      _gtk_css_value_unref (color->sym_col.alpha.color);
      break;
    case COLOR_TYPE_MIX:
      _gtk_css_value_unref (color->sym_col.mix.color1);
      _gtk_css_value_unref (color->sym_col.mix.color2);
      break;
    case COLOR_TYPE_LITERAL:
    case COLOR_TYPE_CURRENT_COLOR:
    default:
      break;
    }

  g_free (color);
}

static GtkCssValue *
gtk_css_value_color_get_fallback (guint             property_id,
                                  GtkStyleProvider *provider,
                                  GtkCssStyle      *style,
                                  GtkCssStyle      *parent_style)
{
  switch (property_id)
    {
      case GTK_CSS_PROPERTY_BACKGROUND_IMAGE:
      case GTK_CSS_PROPERTY_BORDER_IMAGE_SOURCE:
      case GTK_CSS_PROPERTY_TEXT_SHADOW:
      case GTK_CSS_PROPERTY_ICON_SHADOW:
      case GTK_CSS_PROPERTY_BOX_SHADOW:
        return gtk_css_color_value_new_transparent ();
      case GTK_CSS_PROPERTY_COLOR:
      case GTK_CSS_PROPERTY_BACKGROUND_COLOR:
      case GTK_CSS_PROPERTY_BORDER_TOP_COLOR:
      case GTK_CSS_PROPERTY_BORDER_RIGHT_COLOR:
      case GTK_CSS_PROPERTY_BORDER_BOTTOM_COLOR:
      case GTK_CSS_PROPERTY_BORDER_LEFT_COLOR:
      case GTK_CSS_PROPERTY_OUTLINE_COLOR:
      case GTK_CSS_PROPERTY_CARET_COLOR:
      case GTK_CSS_PROPERTY_SECONDARY_CARET_COLOR:
        return _gtk_css_value_compute (_gtk_css_style_property_get_initial_value (_gtk_css_style_property_lookup_by_id (property_id)),
                                       property_id,
                                       provider,
                                       style,
                                       parent_style);
      case GTK_CSS_PROPERTY_ICON_PALETTE:
        return _gtk_css_value_ref (style->core->color);
      default:
        if (property_id < GTK_CSS_PROPERTY_N_PROPERTIES)
          g_warning ("No fallback color defined for property '%s'", 
                     _gtk_style_property_get_name (GTK_STYLE_PROPERTY (_gtk_css_style_property_lookup_by_id (property_id))));
        return gtk_css_color_value_new_transparent ();
    }
}

static GtkCssValue *
gtk_css_value_color_compute (GtkCssValue      *value,
                             guint             property_id,
                             GtkStyleProvider *provider,
                             GtkCssStyle      *style,
                             GtkCssStyle      *parent_style)
{
  GtkCssValue *resolved;

  /* The computed value of the ‘currentColor’ keyword is the computed
   * value of the ‘color’ property. If the ‘currentColor’ keyword is
   * set on the ‘color’ property itself, it is treated as ‘color: inherit’. 
   */
  if (property_id == GTK_CSS_PROPERTY_COLOR)
    {
      GtkCssValue *current;

      if (parent_style)
        current = parent_style->core->color;
      else
        current = NULL;

      resolved = _gtk_css_color_value_resolve (value,
                                               provider,
                                               current,
                                               NULL);
    }
  else if (value->type == COLOR_TYPE_LITERAL)
    {
      resolved = _gtk_css_value_ref (value);
    }
  else
    {
      GtkCssValue *current = style->core->color;

      resolved = _gtk_css_color_value_resolve (value,
                                               provider,
                                               current,
                                               NULL);
    }

  if (resolved == NULL)
    return gtk_css_value_color_get_fallback (property_id, provider, style, parent_style);

  return resolved;
}

static gboolean
gtk_css_value_color_equal (const GtkCssValue *value1,
                           const GtkCssValue *value2)
{
  if (value1->type != value2->type)
    return FALSE;

  switch (value1->type)
    {
    case COLOR_TYPE_LITERAL:
      return gdk_rgba_equal (&value1->sym_col.rgba, &value2->sym_col.rgba);
    case COLOR_TYPE_NAME:
      return g_str_equal (value1->sym_col.name, value2->sym_col.name);
    case COLOR_TYPE_SHADE:
      return value1->sym_col.shade.factor == value2->sym_col.shade.factor &&
             _gtk_css_value_equal (value1->sym_col.shade.color,
                                   value2->sym_col.shade.color);
    case COLOR_TYPE_ALPHA:
      return value1->sym_col.alpha.factor == value2->sym_col.alpha.factor &&
             _gtk_css_value_equal (value1->sym_col.alpha.color,
                                   value2->sym_col.alpha.color);
    case COLOR_TYPE_MIX:
      return value1->sym_col.mix.factor == value2->sym_col.mix.factor &&
             _gtk_css_value_equal (value1->sym_col.mix.color1,
                                   value2->sym_col.mix.color1) &&
             _gtk_css_value_equal (value1->sym_col.mix.color2,
                                   value2->sym_col.mix.color2);
    case COLOR_TYPE_CURRENT_COLOR:
      return TRUE;
    default:
      g_assert_not_reached ();
      return FALSE;
    }
}

static GtkCssValue *
gtk_css_value_color_transition (GtkCssValue *start,
                                GtkCssValue *end,
                                guint        property_id,
                                double       progress)
{
  return _gtk_css_color_value_new_mix (start, end, progress);
}

static void
gtk_css_value_color_print (const GtkCssValue *value,
                           GString           *string)
{
  switch (value->type)
    {
    case COLOR_TYPE_LITERAL:
      {
        char *s = gdk_rgba_to_string (&value->sym_col.rgba);
        g_string_append (string, s);
        g_free (s);
      }
      break;
    case COLOR_TYPE_NAME:
      g_string_append (string, "@");
      g_string_append (string, value->sym_col.name);
      break;
    case COLOR_TYPE_SHADE:
      {
        char factor[G_ASCII_DTOSTR_BUF_SIZE];

        g_string_append (string, "shade(");
        _gtk_css_value_print (value->sym_col.shade.color, string);
        g_string_append (string, ", ");
        g_ascii_dtostr (factor, sizeof (factor), value->sym_col.shade.factor);
        g_string_append (string, factor);
        g_string_append (string, ")");
      }
      break;
    case COLOR_TYPE_ALPHA:
      {
        char factor[G_ASCII_DTOSTR_BUF_SIZE];

        g_string_append (string, "alpha(");
        _gtk_css_value_print (value->sym_col.alpha.color, string);
        g_string_append (string, ", ");
        g_ascii_dtostr (factor, sizeof (factor), value->sym_col.alpha.factor);
        g_string_append (string, factor);
        g_string_append (string, ")");
      }
      break;
    case COLOR_TYPE_MIX:
      {
        char factor[G_ASCII_DTOSTR_BUF_SIZE];

        g_string_append (string, "mix(");
        _gtk_css_value_print (value->sym_col.mix.color1, string);
        g_string_append (string, ", ");
        _gtk_css_value_print (value->sym_col.mix.color2, string);
        g_string_append (string, ", ");
        g_ascii_dtostr (factor, sizeof (factor), value->sym_col.mix.factor);
        g_string_append (string, factor);
        g_string_append (string, ")");
      }
      break;
    case COLOR_TYPE_CURRENT_COLOR:
      g_string_append (string, "currentColor");
      break;
    default:
      g_assert_not_reached ();
    }
}

static const GtkCssValueClass GTK_CSS_VALUE_COLOR = {
  "GtkCssColorValue",
  gtk_css_value_color_free,
  gtk_css_value_color_compute,
  gtk_css_value_color_equal,
  gtk_css_value_color_transition,
  NULL,
  NULL,
  gtk_css_value_color_print
};

static void
apply_alpha (const GdkRGBA *in,
             GdkRGBA       *out,
             double         factor)
{
  *out = *in;

  out->alpha = CLAMP (in->alpha * factor, 0, 1);
}

static void
apply_shade (const GdkRGBA *in,
             GdkRGBA       *out,
             double         factor)
{
  GdkHSLA hsla;

  _gdk_hsla_init_from_rgba (&hsla, in);
  _gdk_hsla_shade (&hsla, &hsla, factor);

  _gdk_rgba_init_from_hsla (out, &hsla);
}

static inline double
transition (double start,
            double end,
             double progress)
{
  return start + (end - start) * progress;
}

static void
apply_mix (const GdkRGBA *in1,
           const GdkRGBA *in2,
           GdkRGBA       *out,
           double         factor)
{
  out->alpha = CLAMP (transition (in1->alpha, in2->alpha, factor), 0, 1);

  if (out->alpha <= 0.0)
    {
      out->red = out->green = out->blue = 0.0;
    }
  else
    {
      out->red   = CLAMP (transition (in1->red * in1->alpha, in2->red * in2->alpha, factor), 0,  1) / out->alpha;
      out->green = CLAMP (transition (in1->green * in1->alpha, in2->green * in2->alpha, factor), 0,  1) / out->alpha;
      out->blue  = CLAMP (transition (in1->blue * in1->alpha, in2->blue * in2->alpha, factor), 0,  1) / out->alpha;
    }
}

GtkCssValue *
_gtk_css_color_value_resolve (GtkCssValue      *color,
                              GtkStyleProvider *provider,
                              GtkCssValue      *current,
                              GSList           *cycle_list)
{
  GtkCssValue *value;

  gtk_internal_return_val_if_fail (color != NULL, NULL);

  switch (color->type)
    {
    case COLOR_TYPE_LITERAL:
      return _gtk_css_value_ref (color);
    case COLOR_TYPE_NAME:
      {
	GtkCssValue *named;
        GSList cycle = { color, cycle_list };

        /* If color exists in cycle_list, we're currently resolving it.
         * So we've detected a cycle. */
        if (g_slist_find (cycle_list, color))
          return NULL;

        named = gtk_style_provider_get_color (provider, color->sym_col.name);
	if (named == NULL)
	  return NULL;

        value = _gtk_css_color_value_resolve (named, provider, current, &cycle);
	if (value == NULL)
	  return NULL;
      }

      break;
    case COLOR_TYPE_SHADE:
      {
        const GdkRGBA *c;
        GtkCssValue *val;
        GdkRGBA shade;

        val = _gtk_css_color_value_resolve (color->sym_col.shade.color, provider, current, cycle_list);
        if (val == NULL)
          return NULL;
        c = gtk_css_color_value_get_rgba (val);

        apply_shade (c, &shade, color->sym_col.shade.factor);

        value = _gtk_css_color_value_new_literal (&shade);
        _gtk_css_value_unref (val);
      }

      break;
    case COLOR_TYPE_ALPHA:
      {
        const GdkRGBA *c;
        GtkCssValue *val;
        GdkRGBA alpha;

        val = _gtk_css_color_value_resolve (color->sym_col.alpha.color, provider, current, cycle_list);
        if (val == NULL)
          return NULL;
        c = gtk_css_color_value_get_rgba (val);

        apply_alpha (c, &alpha, color->sym_col.alpha.factor);

        value = _gtk_css_color_value_new_literal (&alpha);
        _gtk_css_value_unref (val);
      }
      break;

    case COLOR_TYPE_MIX:
      {
        const GdkRGBA *color1, *color2;
        GtkCssValue *val1, *val2;
        GdkRGBA res;

        val1 = _gtk_css_color_value_resolve (color->sym_col.mix.color1, provider, current, cycle_list);
        if (val1 == NULL)
          return NULL;
        color1 = gtk_css_color_value_get_rgba (val1);

        val2 = _gtk_css_color_value_resolve (color->sym_col.mix.color2, provider, current, cycle_list);
        if (val2 == NULL)
          return NULL;
        color2 = gtk_css_color_value_get_rgba (val2);

        apply_mix (color1, color2, &res, color->sym_col.mix.factor);

        value = _gtk_css_color_value_new_literal (&res);
        _gtk_css_value_unref (val1);
        _gtk_css_value_unref (val2);
      }

      break;
    case COLOR_TYPE_CURRENT_COLOR:
      if (current)
        {
          return _gtk_css_value_ref (current);
        }
      else
        {
          GtkCssValue *initial = _gtk_css_style_property_get_initial_value (_gtk_css_style_property_lookup_by_id (GTK_CSS_PROPERTY_COLOR));

          if (initial->class == &GTK_CSS_VALUE_COLOR)
            {
              return _gtk_css_color_value_resolve (initial,
                                                   provider,
                                                   NULL,
                                                   cycle_list);
            }
          else
            {
              return _gtk_css_value_ref (initial);
            }
        }
      break;
    default:
      value = NULL;
      g_assert_not_reached ();
    }

  if (color->last_value != NULL &&
      _gtk_css_value_equal (color->last_value, value))
    {
      _gtk_css_value_unref (value);
      value = _gtk_css_value_ref (color->last_value);
    }
  else
    {
      if (color->last_value != NULL)
        _gtk_css_value_unref (color->last_value);
      color->last_value = _gtk_css_value_ref (value);
    }

  return value;
}

static GtkCssValue transparent_black_singleton = { &GTK_CSS_VALUE_COLOR, 1, TRUE, COLOR_TYPE_LITERAL, NULL,
                                                   .sym_col.rgba = {0, 0, 0, 0} };
static GtkCssValue white_singleton             = { &GTK_CSS_VALUE_COLOR, 1, TRUE, COLOR_TYPE_LITERAL, NULL,
                                                   .sym_col.rgba = {1, 1, 1, 1} };


GtkCssValue *
gtk_css_color_value_new_transparent (void)
{
  return _gtk_css_value_ref (&transparent_black_singleton);
}

GtkCssValue *
gtk_css_color_value_new_white (void)
{
  return _gtk_css_value_ref (&white_singleton);
}

GtkCssValue *
_gtk_css_color_value_new_literal (const GdkRGBA *color)
{
  GtkCssValue *value;

  g_return_val_if_fail (color != NULL, NULL);

  if (gdk_rgba_equal (color, &white_singleton.sym_col.rgba))
    return _gtk_css_value_ref (&white_singleton);

  if (gdk_rgba_equal (color, &transparent_black_singleton.sym_col.rgba))
    return _gtk_css_value_ref (&transparent_black_singleton);

  value = _gtk_css_value_new (GtkCssValue, &GTK_CSS_VALUE_COLOR);
  value->type = COLOR_TYPE_LITERAL;
  value->is_computed = TRUE;
  value->sym_col.rgba = *color;

  return value;
}

GtkCssValue *
_gtk_css_color_value_new_name (const char *name)
{
  GtkCssValue *value;

  gtk_internal_return_val_if_fail (name != NULL, NULL);

  value = _gtk_css_value_new (GtkCssValue, &GTK_CSS_VALUE_COLOR);
  value->type = COLOR_TYPE_NAME;
  value->sym_col.name = g_strdup (name);

  return value;
}

GtkCssValue *
_gtk_css_color_value_new_shade (GtkCssValue *color,
                                double       factor)
{
  GtkCssValue *value;

  gtk_internal_return_val_if_fail (color->class == &GTK_CSS_VALUE_COLOR, NULL);

  if (color->type == COLOR_TYPE_LITERAL)
    {
      GdkRGBA c;

      apply_shade (&color->sym_col.rgba, &c, factor);

      return _gtk_css_color_value_new_literal (&c);
    }

  value = _gtk_css_value_new (GtkCssValue, &GTK_CSS_VALUE_COLOR);
  value->type = COLOR_TYPE_SHADE;
  value->sym_col.shade.color = _gtk_css_value_ref (color);
  value->sym_col.shade.factor = factor;

  return value;
}

GtkCssValue *
_gtk_css_color_value_new_alpha (GtkCssValue *color,
                                double       factor)
{
  GtkCssValue *value;

  gtk_internal_return_val_if_fail (color->class == &GTK_CSS_VALUE_COLOR, NULL);

  if (color->type == COLOR_TYPE_LITERAL)
    {
      GdkRGBA c;

      apply_alpha (&color->sym_col.rgba, &c, factor);

      return _gtk_css_color_value_new_literal (&c);
    }

  value = _gtk_css_value_new (GtkCssValue, &GTK_CSS_VALUE_COLOR);
  value->type = COLOR_TYPE_ALPHA;
  value->sym_col.alpha.color = _gtk_css_value_ref (color);
  value->sym_col.alpha.factor = factor;

  return value;
}

GtkCssValue *
_gtk_css_color_value_new_mix (GtkCssValue *color1,
                              GtkCssValue *color2,
                              double       factor)
{
  GtkCssValue *value;

  gtk_internal_return_val_if_fail (color1->class == &GTK_CSS_VALUE_COLOR, NULL);
  gtk_internal_return_val_if_fail (color2->class == &GTK_CSS_VALUE_COLOR, NULL);

  if (color1->type == COLOR_TYPE_LITERAL &&
      color2->type == COLOR_TYPE_LITERAL)
    {
      GdkRGBA result;

      apply_mix (&color1->sym_col.rgba, &color2->sym_col.rgba, &result, factor);

      return _gtk_css_color_value_new_literal (&result);

    }

  value = _gtk_css_value_new (GtkCssValue, &GTK_CSS_VALUE_COLOR);
  value->type = COLOR_TYPE_MIX;
  value->sym_col.mix.color1 = _gtk_css_value_ref (color1);
  value->sym_col.mix.color2 = _gtk_css_value_ref (color2);
  value->sym_col.mix.factor = factor;

  return value;
}

GtkCssValue *
_gtk_css_color_value_new_current_color (void)
{
  static GtkCssValue current_color = { &GTK_CSS_VALUE_COLOR, 1, FALSE, COLOR_TYPE_CURRENT_COLOR, NULL, };

  return _gtk_css_value_ref (&current_color);
}

typedef struct 
{
  GtkCssValue *color;
  GtkCssValue *color2;
  double       value;
} ColorFunctionData;

static guint
parse_color_mix (GtkCssParser *parser,
                 guint         arg,
                 gpointer      data_)
{
  ColorFunctionData *data = data_;

  switch (arg)
  {
    case 0:
      data->color = _gtk_css_color_value_parse (parser);
      if (data->color == NULL)
        return 0;
      return 1;

    case 1:
      data->color2 = _gtk_css_color_value_parse (parser);
      if (data->color2 == NULL)
        return 0;
      return 1;

    case 2:
      if (!gtk_css_parser_consume_number (parser, &data->value))
        return 0;
      return 1;

    default:
      g_return_val_if_reached (0);
  }
}

static guint
parse_color_number (GtkCssParser *parser,
                    guint         arg,
                    gpointer      data_)
{
  ColorFunctionData *data = data_;

  switch (arg)
  {
    case 0:
      data->color = _gtk_css_color_value_parse (parser);
      if (data->color == NULL)
        return 0;
      return 1;

    case 1:
      if (!gtk_css_parser_consume_number (parser, &data->value))
        return 0;
      return 1;

    default:
      g_return_val_if_reached (0);
  }
}

gboolean
gtk_css_color_value_can_parse (GtkCssParser *parser)
{
  /* This is way too generous, but meh... */
  return gtk_css_parser_has_token (parser, GTK_CSS_TOKEN_IDENT)
      || gtk_css_parser_has_token (parser, GTK_CSS_TOKEN_AT_KEYWORD)
      || gtk_css_parser_has_token (parser, GTK_CSS_TOKEN_HASH_ID)
      || gtk_css_parser_has_token (parser, GTK_CSS_TOKEN_HASH_UNRESTRICTED)
      || gtk_css_parser_has_function (parser, "lighter")
      || gtk_css_parser_has_function (parser, "darker")
      || gtk_css_parser_has_function (parser, "shade")
      || gtk_css_parser_has_function (parser, "alpha")
      || gtk_css_parser_has_function (parser, "mix")
      || gtk_css_parser_has_function (parser, "hsl")
      || gtk_css_parser_has_function (parser, "hsla")
      || gtk_css_parser_has_function (parser, "rgb")
      || gtk_css_parser_has_function (parser, "rgba");
}

GtkCssValue *
_gtk_css_color_value_parse (GtkCssParser *parser)
{
  ColorFunctionData data = { NULL, };
  GtkCssValue *value;
  GdkRGBA rgba;

  if (gtk_css_parser_try_ident (parser, "currentColor"))
    {
      return _gtk_css_color_value_new_current_color ();
    }
  else if (gtk_css_parser_has_token (parser, GTK_CSS_TOKEN_AT_KEYWORD))
    {
      const GtkCssToken *token = gtk_css_parser_get_token (parser);

      value = _gtk_css_color_value_new_name (gtk_css_token_get_string (token));
      gtk_css_parser_consume_token (parser);

      return value;
    }
  else if (gtk_css_parser_has_function (parser, "lighter"))
    {
      if (gtk_css_parser_consume_function (parser, 1, 1, parse_color_number, &data))
        value = _gtk_css_color_value_new_shade (data.color, 1.3);
      else
        value = NULL;

      g_clear_pointer (&data.color, gtk_css_value_unref);
      return value;
    }
  else if (gtk_css_parser_has_function (parser, "darker"))
    {
      if (gtk_css_parser_consume_function (parser, 1, 1, parse_color_number, &data))
        value = _gtk_css_color_value_new_shade (data.color, 0.7);
      else
        value = NULL;

      g_clear_pointer (&data.color, gtk_css_value_unref);
      return value;
    }
  else if (gtk_css_parser_has_function (parser, "shade"))
    {
      if (gtk_css_parser_consume_function (parser, 2, 2, parse_color_number, &data))
        value = _gtk_css_color_value_new_shade (data.color, data.value);
      else
        value = NULL;

      g_clear_pointer (&data.color, gtk_css_value_unref);
      return value;
    }
  else if (gtk_css_parser_has_function (parser, "alpha"))
    {
      if (gtk_css_parser_consume_function (parser, 2, 2, parse_color_number, &data))
        value = _gtk_css_color_value_new_alpha (data.color, data.value);
      else
        value = NULL;

      g_clear_pointer (&data.color, gtk_css_value_unref);
      return value;
    }
  else if (gtk_css_parser_has_function (parser, "mix"))
    {
      if (gtk_css_parser_consume_function (parser, 3, 3, parse_color_mix, &data))
        value = _gtk_css_color_value_new_mix (data.color, data.color2, data.value);
      else
        value = NULL;

      g_clear_pointer (&data.color, gtk_css_value_unref);
      g_clear_pointer (&data.color2, gtk_css_value_unref);
      return value;
    }

  if (gdk_rgba_parser_parse (parser, &rgba))
    return _gtk_css_color_value_new_literal (&rgba);
  else
    return NULL;
}

const GdkRGBA *
gtk_css_color_value_get_rgba (const GtkCssValue *color)
{
  g_assert (color->class == &GTK_CSS_VALUE_COLOR);
  g_assert (color->type == COLOR_TYPE_LITERAL);

  return &color->sym_col.rgba;
}
