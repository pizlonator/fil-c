/*
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

#include "gtkstylepropertyprivate.h"

#include <gobject/gvaluecollector.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <math.h>

#include <gtk/css/gtkcss.h>
#include "gtk/css/gtkcsstokenizerprivate.h"
#include "gtk/css/gtkcssparserprivate.h"
#include "gtkcssstylepropertyprivate.h"
#include "gtkcsstypesprivate.h"
#include "gtkprivatetypebuiltins.h"

/* the actual parsers we have */
#include "gtkcssarrayvalueprivate.h"
#include "gtkcssbgsizevalueprivate.h"
#include "gtkcssbordervalueprivate.h"
#include "gtkcsscolorvalueprivate.h"
#include "gtkcsscornervalueprivate.h"
#include "gtkcsseasevalueprivate.h"
#include "gtkcssfiltervalueprivate.h"
#include "gtkcssfontfeaturesvalueprivate.h"
#include "gtkcssimageprivate.h"
#include "gtkcssimagevalueprivate.h"
#include "gtkcssinitialvalueprivate.h"
#include "gtkcssenumvalueprivate.h"
#include "gtkcssnumbervalueprivate.h"
#include "gtkcsspalettevalueprivate.h"
#include "gtkcsspositionvalueprivate.h"
#include "gtkcssrepeatvalueprivate.h"
#include "gtkcssshadowvalueprivate.h"
#include "gtkcssstringvalueprivate.h"
#include "gtkcsstransformvalueprivate.h"
#include "gtkcssfontvariationsvalueprivate.h"
#include "gtkcsslineheightvalueprivate.h"
#include "gtktypebuiltins.h"

/*** REGISTRATION ***/

typedef enum {
  GTK_STYLE_PROPERTY_INHERIT = (1 << 0),
  GTK_STYLE_PROPERTY_ANIMATED = (1 << 1),
} GtkStylePropertyFlags;

static void
gtk_css_style_property_register (const char *                   name,
                                 guint                          expected_id,
                                 GtkStylePropertyFlags          flags,
                                 GtkCssAffects                  affects,
                                 GtkCssStylePropertyParseFunc   parse_value,
                                 GtkCssValue *                  initial_value)
{
  GtkCssStyleProperty *node;

  g_assert (initial_value != NULL);
  g_assert (parse_value != NULL);

  node = g_object_new (GTK_TYPE_CSS_STYLE_PROPERTY,
                       "affects", affects,
                       "animated", (flags & GTK_STYLE_PROPERTY_ANIMATED) ? TRUE : FALSE,
                       "inherit", (flags & GTK_STYLE_PROPERTY_INHERIT) ? TRUE : FALSE,
                       "initial-value", initial_value,
                       "name", name,
                       NULL);

  node->parse_value = parse_value;

  _gtk_css_value_unref (initial_value);

  g_assert (_gtk_css_style_property_get_id (node) == expected_id);
}

/*** IMPLEMENTATIONS ***/

static GtkCssValue *
color_parse (GtkCssStyleProperty *property,
             GtkCssParser        *parser)
{
  return _gtk_css_color_value_parse (parser);
}

static GtkCssValue *
font_family_parse_one (GtkCssParser *parser)
{
  char *name;

  if (gtk_css_parser_has_token (parser, GTK_CSS_TOKEN_IDENT))
    {
      GString *string = g_string_new (NULL);

      name = gtk_css_parser_consume_ident (parser);
      g_string_append (string, name);
      g_free (name);
      while (gtk_css_parser_has_token (parser, GTK_CSS_TOKEN_IDENT))
        {
          name = gtk_css_parser_consume_ident (parser);
          g_string_append_c (string, ' ');
          g_string_append (string, name);
          g_free (name);
        }
      name = g_string_free (string, FALSE);
    }
  else 
    {
      name = gtk_css_parser_consume_string (parser);
      if (name == NULL)
        return NULL;
    }

  return _gtk_css_string_value_new_take (name);
}

GtkCssValue *
gtk_css_font_family_value_parse (GtkCssParser *parser)
{
  return _gtk_css_array_value_parse (parser, font_family_parse_one);
}

static GtkCssValue *
font_family_parse (GtkCssStyleProperty *property,
                   GtkCssParser        *parser)
{
  return gtk_css_font_family_value_parse (parser);
}

static GtkCssValue *
font_style_parse (GtkCssStyleProperty *property,
                  GtkCssParser        *parser)
{
  GtkCssValue *value = _gtk_css_font_style_value_try_parse (parser);
  
  if (value == NULL)
    gtk_css_parser_error_syntax (parser, "unknown font style value");

  return value;
}

static GtkCssValue *
font_weight_parse (GtkCssStyleProperty *property,
                   GtkCssParser        *parser)
{
  GtkCssValue *value;
  
  value = gtk_css_font_weight_value_try_parse (parser);
  if (value == NULL)
    {
      value = _gtk_css_number_value_parse (parser, GTK_CSS_PARSE_NUMBER | GTK_CSS_POSITIVE_ONLY);
      if (value == NULL)
        return NULL;

      if (_gtk_css_number_value_get (value, 100) < 1 || 
          _gtk_css_number_value_get (value, 100) > 1000)
        {
          gtk_css_parser_error_value (parser, "Font weight values must be between 1 and 1000");
          g_clear_pointer (&value, gtk_css_value_unref);
        }
    }

  return value;
}

static GtkCssValue *
font_stretch_parse (GtkCssStyleProperty *property,
                    GtkCssParser        *parser)
{
  GtkCssValue *value = _gtk_css_font_stretch_value_try_parse (parser);

  if (value == NULL)
    gtk_css_parser_error_syntax (parser, "unknown font stretch value");

  return value;
}

static GtkCssValue *
parse_border_style (GtkCssStyleProperty *property,
                    GtkCssParser        *parser)
{
  GtkCssValue *value = _gtk_css_border_style_value_try_parse (parser);
  
  if (value == NULL)
    gtk_css_parser_error_syntax (parser, "unknown border style value");

  return value;
}

static GtkCssValue *
parse_css_area_one (GtkCssParser *parser)
{
  GtkCssValue *value = _gtk_css_area_value_try_parse (parser);
  
  if (value == NULL)
    gtk_css_parser_error_syntax (parser, "unknown box value");

  return value;
}

static GtkCssValue *
parse_css_area (GtkCssStyleProperty *property,
                GtkCssParser        *parser)
{
  return _gtk_css_array_value_parse (parser, parse_css_area_one);
}

static GtkCssValue *
parse_one_css_direction (GtkCssParser *parser)
{
  GtkCssValue *value = _gtk_css_direction_value_try_parse (parser);
  
  if (value == NULL)
    gtk_css_parser_error_syntax (parser, "unknown direction value");

  return value;
}

static GtkCssValue *
parse_css_direction (GtkCssStyleProperty *property,
                     GtkCssParser        *parser)
{
  return _gtk_css_array_value_parse (parser, parse_one_css_direction);
}

static GtkCssValue *
opacity_parse (GtkCssStyleProperty *property,
	       GtkCssParser        *parser)
{
  return _gtk_css_number_value_parse (parser, GTK_CSS_PARSE_NUMBER);
}

static GtkCssValue *
parse_one_css_play_state (GtkCssParser *parser)
{
  GtkCssValue *value = _gtk_css_play_state_value_try_parse (parser);
  
  if (value == NULL)
    gtk_css_parser_error_syntax (parser, "unknown play state value");

  return value;
}

static GtkCssValue *
parse_css_play_state (GtkCssStyleProperty *property,
                      GtkCssParser        *parser)
{
  return _gtk_css_array_value_parse (parser, parse_one_css_play_state);
}

static GtkCssValue *
parse_one_css_fill_mode (GtkCssParser *parser)
{
  GtkCssValue *value = _gtk_css_fill_mode_value_try_parse (parser);
  
  if (value == NULL)
    gtk_css_parser_error_syntax (parser, "unknown fill mode value");

  return value;
}

static GtkCssValue *
parse_css_fill_mode (GtkCssStyleProperty *property,
                     GtkCssParser        *parser)
{
  return _gtk_css_array_value_parse (parser, parse_one_css_fill_mode);
}

static GtkCssValue *
icon_size_parse (GtkCssStyleProperty *property,
		 GtkCssParser        *parser)
{
  return _gtk_css_number_value_parse (parser, 
                                      GTK_CSS_PARSE_LENGTH
                                      | GTK_CSS_PARSE_PERCENT
                                      | GTK_CSS_POSITIVE_ONLY);
}

static GtkCssValue *
icon_palette_parse (GtkCssStyleProperty *property,
		    GtkCssParser        *parser)
{
  return gtk_css_palette_value_parse (parser);
}

static GtkCssValue *
icon_style_parse (GtkCssStyleProperty *property,
		  GtkCssParser        *parser)
{
  GtkCssValue *value = _gtk_css_icon_style_value_try_parse (parser);

  if (value == NULL)
    gtk_css_parser_error_syntax (parser, "unknown icon style value");

  return value;
}

static GtkCssValue *
parse_letter_spacing (GtkCssStyleProperty *property,
                      GtkCssParser        *parser)
{
  return _gtk_css_number_value_parse (parser, GTK_CSS_PARSE_LENGTH);
}

static gboolean
value_is_done_parsing (GtkCssParser *parser)
{
  return gtk_css_parser_has_token (parser, GTK_CSS_TOKEN_EOF) ||
         gtk_css_parser_has_token (parser, GTK_CSS_TOKEN_COMMA) ||
         gtk_css_parser_has_token (parser, GTK_CSS_TOKEN_SEMICOLON) ||
         gtk_css_parser_has_token (parser, GTK_CSS_TOKEN_CLOSE_CURLY);
}

static GtkCssValue *
parse_text_decoration_line (GtkCssStyleProperty *property,
                            GtkCssParser        *parser)
{
  GtkCssValue *value = NULL;
  GtkTextDecorationLine line;

  line = 0;
  do {
    GtkTextDecorationLine parsed;

    parsed = _gtk_css_text_decoration_line_try_parse_one (parser, line);
    if (parsed == 0 || parsed == line)
      {
        gtk_css_parser_error_syntax (parser, "Not a valid value");
        return NULL;
      }
    line = parsed;
  } while (!value_is_done_parsing (parser));

  value = _gtk_css_text_decoration_line_value_new (line);
  if (value == NULL)
    gtk_css_parser_error_syntax (parser, "Invalid combination of values");

  return value;
}

static GtkCssValue *
parse_text_decoration_style (GtkCssStyleProperty *property,
                             GtkCssParser        *parser)
{
  GtkCssValue *value = _gtk_css_text_decoration_style_value_try_parse (parser);

  if (value == NULL)
    gtk_css_parser_error_syntax (parser, "unknown text decoration style value");

  return value;
}

static GtkCssValue *
parse_text_transform (GtkCssStyleProperty *property,
                      GtkCssParser        *parser)
{
  GtkCssValue *value = _gtk_css_text_transform_value_try_parse (parser);

  if (value == NULL)
    gtk_css_parser_error_syntax (parser, "unknown text transform value");

  return value;
}

static GtkCssValue *
parse_font_kerning (GtkCssStyleProperty *property,
                    GtkCssParser        *parser)
{
  GtkCssValue *value = _gtk_css_font_kerning_value_try_parse (parser);

  if (value == NULL)
    gtk_css_parser_error_syntax (parser, "unknown font kerning value");

  return value;
}

static GtkCssValue *
parse_font_variant_ligatures (GtkCssStyleProperty *property,
                              GtkCssParser        *parser)
{
  GtkCssValue *value = NULL;
  GtkCssFontVariantLigature ligatures;

  ligatures = 0;
  do {
    GtkCssFontVariantLigature parsed;

    parsed = _gtk_css_font_variant_ligature_try_parse_one (parser, ligatures);
    if (parsed == 0 || parsed == ligatures)
      {
        gtk_css_parser_error_syntax (parser, "Not a valid value");
        return NULL;
      }
    ligatures = parsed;
  } while (!value_is_done_parsing (parser));

  value = _gtk_css_font_variant_ligature_value_new (ligatures);
  if (value == NULL)
    gtk_css_parser_error_syntax (parser, "Invalid combination of values");

  return value;
}

static GtkCssValue *
parse_font_variant_position (GtkCssStyleProperty *property,
                             GtkCssParser        *parser)
{
  GtkCssValue *value = _gtk_css_font_variant_position_value_try_parse (parser);

  if (value == NULL)
    gtk_css_parser_error_syntax (parser, "unknown font variant position value");

  return value;
}

static GtkCssValue *
parse_font_variant_caps (GtkCssStyleProperty *property,
                         GtkCssParser        *parser)
{
  GtkCssValue *value = _gtk_css_font_variant_caps_value_try_parse (parser);

  if (value == NULL)
    gtk_css_parser_error_syntax (parser, "unknown font variant caps value");

  return value;
}

static GtkCssValue *
parse_font_variant_numeric (GtkCssStyleProperty *property,
                            GtkCssParser        *parser)
{
  GtkCssValue *value = NULL;
  GtkCssFontVariantNumeric numeric;

  numeric = 0;
  do {
    GtkCssFontVariantNumeric parsed;

    parsed = _gtk_css_font_variant_numeric_try_parse_one (parser, numeric);
    if (parsed == 0 || parsed == numeric)
      {
        gtk_css_parser_error_syntax (parser, "Not a valid value");
        return NULL;
      }
    numeric = parsed;
  } while (!value_is_done_parsing (parser));

  value = _gtk_css_font_variant_numeric_value_new (numeric);
  if (value == NULL)
    gtk_css_parser_error_syntax (parser, "Invalid combination of values");

  return value;
}

static GtkCssValue *
parse_font_variant_alternates (GtkCssStyleProperty *property,
                               GtkCssParser        *parser)
{
  GtkCssValue *value = _gtk_css_font_variant_alternate_value_try_parse (parser);

  if (value == NULL)
    gtk_css_parser_error_syntax (parser, "unknown font variant alternate value");

  return value;
}

static GtkCssValue *
parse_font_variant_east_asian (GtkCssStyleProperty *property,
                               GtkCssParser        *parser)
{
  GtkCssValue *value = NULL;
  GtkCssFontVariantEastAsian east_asian;

  east_asian = 0;
  do {
    GtkCssFontVariantEastAsian parsed;

    parsed = _gtk_css_font_variant_east_asian_try_parse_one (parser, east_asian);
    if (parsed == 0 || parsed == east_asian)
      {
        gtk_css_parser_error_syntax (parser, "Not a valid value");
        return NULL;
      }
    east_asian = parsed;
  } while (!value_is_done_parsing (parser));

  value = _gtk_css_font_variant_east_asian_value_new (east_asian);
  if (value == NULL)
    gtk_css_parser_error_syntax (parser, "Invalid combination of values");

  return value;
}

static GtkCssValue *
parse_font_feature_settings (GtkCssStyleProperty *property,
                             GtkCssParser        *parser)
{
  return gtk_css_font_features_value_parse (parser);
}

static GtkCssValue *
parse_font_variation_settings (GtkCssStyleProperty *property,
                               GtkCssParser        *parser)
{
  return gtk_css_font_variations_value_parse (parser);
}

static GtkCssValue *
box_shadow_value_parse (GtkCssStyleProperty *property,
                        GtkCssParser        *parser)
{
  return gtk_css_shadow_value_parse (parser, TRUE);
}

static GtkCssValue *
shadow_value_parse (GtkCssStyleProperty *property,
                    GtkCssParser        *parser)
{
  return gtk_css_shadow_value_parse (parser, FALSE);
}

static GtkCssValue *
transform_value_parse (GtkCssStyleProperty *property,
                       GtkCssParser        *parser)
{
  return _gtk_css_transform_value_parse (parser);
}

static GtkCssValue *
filter_value_parse (GtkCssStyleProperty *property,
                    GtkCssParser        *parser)
{
  return gtk_css_filter_value_parse (parser);
}

static GtkCssValue *
border_spacing_value_parse (GtkCssStyleProperty *property,
                            GtkCssParser        *parser)
{
  return gtk_css_position_value_parse_spacing (parser);
}

static GtkCssValue *
border_corner_radius_value_parse (GtkCssStyleProperty *property,
                                  GtkCssParser        *parser)
{
  return _gtk_css_corner_value_parse (parser);
}

static GtkCssValue *
css_image_value_parse (GtkCssStyleProperty *property,
                       GtkCssParser        *parser)
{
  GtkCssImage *image;

  if (gtk_css_parser_try_ident (parser, "none"))
    image = NULL;
  else
    {
      image = _gtk_css_image_new_parse (parser);
      if (image == NULL)
        return NULL;
    }

  return _gtk_css_image_value_new (image);
}

static GtkCssValue *
background_image_value_parse_one (GtkCssParser *parser)
{
  return css_image_value_parse (NULL, parser);
}

static GtkCssValue *
background_image_value_parse (GtkCssStyleProperty *property,
                              GtkCssParser        *parser)
{
  return _gtk_css_array_value_parse (parser, background_image_value_parse_one);
}

static GtkCssValue *
dpi_parse (GtkCssStyleProperty *property,
	   GtkCssParser        *parser)
{
  return _gtk_css_number_value_parse (parser, GTK_CSS_PARSE_NUMBER);
}

GtkCssValue *
gtk_css_font_size_value_parse (GtkCssParser *parser)
{
  GtkCssValue *value;

  value = _gtk_css_font_size_value_try_parse (parser);
  if (value)
    return value;

  return _gtk_css_number_value_parse (parser,
                                      GTK_CSS_PARSE_LENGTH
                                      | GTK_CSS_PARSE_PERCENT
                                      | GTK_CSS_POSITIVE_ONLY);
}

static GtkCssValue *
font_size_parse (GtkCssStyleProperty *property,
                 GtkCssParser        *parser)
{
  return gtk_css_font_size_value_parse (parser);
}

static GtkCssValue *
outline_parse (GtkCssStyleProperty *property,
               GtkCssParser        *parser)
{
  return _gtk_css_number_value_parse (parser,
                                      GTK_CSS_PARSE_LENGTH);
}

static GtkCssValue *
border_image_repeat_parse (GtkCssStyleProperty *property,
                           GtkCssParser        *parser)
{
  GtkCssValue *value = _gtk_css_border_repeat_value_try_parse (parser);

  if (value == NULL)
    {
      gtk_css_parser_error_syntax (parser, "Not a valid border repeat value");
      return NULL;
    }

  return value;
}

static GtkCssValue *
border_image_slice_parse (GtkCssStyleProperty *property,
                          GtkCssParser        *parser)
{
  return _gtk_css_border_value_parse (parser,
                                      GTK_CSS_PARSE_PERCENT
                                      | GTK_CSS_PARSE_NUMBER
                                      | GTK_CSS_POSITIVE_ONLY,
                                      FALSE,
                                      TRUE);
}

static GtkCssValue *
border_image_width_parse (GtkCssStyleProperty *property,
                          GtkCssParser        *parser)
{
  return _gtk_css_border_value_parse (parser,
                                      GTK_CSS_PARSE_PERCENT
                                      | GTK_CSS_PARSE_LENGTH
                                      | GTK_CSS_PARSE_NUMBER
                                      | GTK_CSS_POSITIVE_ONLY,
                                      TRUE,
                                      FALSE);
}

static GtkCssValue *
minmax_parse (GtkCssStyleProperty *property,
              GtkCssParser        *parser)
{
  return _gtk_css_number_value_parse (parser,
                                      GTK_CSS_PARSE_LENGTH
                                      | GTK_CSS_POSITIVE_ONLY);
}

static GtkCssValue *
transition_property_parse_one (GtkCssParser *parser)
{
  GtkCssValue *value;

  value = _gtk_css_ident_value_try_parse (parser);

  if (value == NULL)
    {
      gtk_css_parser_error_syntax (parser, "Expected an identifier");
      return NULL;
    }

  return value;
}

static GtkCssValue *
transition_property_parse (GtkCssStyleProperty *property,
                           GtkCssParser        *parser)
{
  return _gtk_css_array_value_parse (parser, transition_property_parse_one);
}

static GtkCssValue *
transition_time_parse_one (GtkCssParser *parser)
{
  return _gtk_css_number_value_parse (parser, GTK_CSS_PARSE_TIME);
}

static GtkCssValue *
transition_time_parse (GtkCssStyleProperty *property,
                       GtkCssParser        *parser)
{
  return _gtk_css_array_value_parse (parser, transition_time_parse_one);
}

static GtkCssValue *
transition_timing_function_parse (GtkCssStyleProperty *property,
                                  GtkCssParser        *parser)
{
  return _gtk_css_array_value_parse (parser, _gtk_css_ease_value_parse);
}

static GtkCssValue *
iteration_count_parse_one (GtkCssParser *parser)
{
  if (gtk_css_parser_try_ident (parser, "infinite"))
    return _gtk_css_number_value_new (HUGE_VAL, GTK_CSS_NUMBER);

  return _gtk_css_number_value_parse (parser, GTK_CSS_PARSE_NUMBER | GTK_CSS_POSITIVE_ONLY);
}

static GtkCssValue *
iteration_count_parse (GtkCssStyleProperty *property,
                       GtkCssParser        *parser)
{
  return _gtk_css_array_value_parse (parser, iteration_count_parse_one);
}

static GtkCssValue *
parse_margin (GtkCssStyleProperty *property,
              GtkCssParser        *parser)
{
  return _gtk_css_number_value_parse (parser,
                                      GTK_CSS_PARSE_LENGTH);
}

static GtkCssValue *
parse_padding (GtkCssStyleProperty *property,
               GtkCssParser        *parser)
{
  return _gtk_css_number_value_parse (parser,
                                      GTK_CSS_POSITIVE_ONLY
                                      | GTK_CSS_PARSE_LENGTH);
}

static GtkCssValue *
parse_border_width (GtkCssStyleProperty *property,
                    GtkCssParser        *parser)
{
  return _gtk_css_number_value_parse (parser,
                                      GTK_CSS_POSITIVE_ONLY
                                      | GTK_CSS_PARSE_LENGTH);
}

static GtkCssValue *
blend_mode_value_parse_one (GtkCssParser        *parser)
{
  GtkCssValue *value = _gtk_css_blend_mode_value_try_parse (parser);

  if (value == NULL)
    gtk_css_parser_error_syntax (parser, "Unknown blend mode value");

  return value;
}

static GtkCssValue *
blend_mode_value_parse (GtkCssStyleProperty *property,
                        GtkCssParser        *parser)
{
  return _gtk_css_array_value_parse (parser, blend_mode_value_parse_one);
}

static GtkCssValue *
background_repeat_value_parse_one (GtkCssParser *parser)
{
  GtkCssValue *value = _gtk_css_background_repeat_value_try_parse (parser);

  if (value == NULL)
    {
      gtk_css_parser_error_syntax (parser, "Unknown repeat value");
      return NULL;
    }

  return value;
}

static GtkCssValue *
background_repeat_value_parse (GtkCssStyleProperty *property,
                               GtkCssParser        *parser)
{
  return _gtk_css_array_value_parse (parser, background_repeat_value_parse_one);
}

static GtkCssValue *
background_size_parse (GtkCssStyleProperty *property,
                       GtkCssParser        *parser)
{
  return _gtk_css_array_value_parse (parser, _gtk_css_bg_size_value_parse);
}

static GtkCssValue *
background_position_parse (GtkCssStyleProperty *property,
			   GtkCssParser        *parser)
{
  return _gtk_css_array_value_parse (parser, _gtk_css_position_value_parse);
}

static GtkCssValue *
transform_origin_parse (GtkCssStyleProperty *property,
                        GtkCssParser        *parser)
{
  return _gtk_css_position_value_parse (parser);
}

static GtkCssValue *
parse_line_height (GtkCssStyleProperty *property,
                   GtkCssParser        *parser)
{
  return gtk_css_line_height_value_parse (parser);
}

/*** REGISTRATION ***/

G_STATIC_ASSERT (GTK_CSS_PROPERTY_COLOR == 0);
G_STATIC_ASSERT (GTK_CSS_PROPERTY_DPI < GTK_CSS_PROPERTY_FONT_SIZE);

void
_gtk_css_style_property_init_properties (void)
{
  /* Initialize "color", "-gtk-dpi" and "font-size" first,
   * so that when computing values later they are
   * done first. That way, 'currentColor' and font
   * sizes in em can be looked up properly */
  gtk_css_style_property_register        ("color",
                                          GTK_CSS_PROPERTY_COLOR,
                                          GTK_STYLE_PROPERTY_INHERIT | GTK_STYLE_PROPERTY_ANIMATED,
                                          GTK_CSS_AFFECTS_CONTENT | GTK_CSS_AFFECTS_ICON_REDRAW_SYMBOLIC,
                                          color_parse,
                                          gtk_css_color_value_new_white ());
  gtk_css_style_property_register        ("-gtk-dpi",
                                          GTK_CSS_PROPERTY_DPI,
                                          GTK_STYLE_PROPERTY_INHERIT | GTK_STYLE_PROPERTY_ANIMATED,
                                          GTK_CSS_AFFECTS_SIZE | GTK_CSS_AFFECTS_TEXT_SIZE,
                                          dpi_parse,
                                          _gtk_css_number_value_new (96.0, GTK_CSS_NUMBER));
  gtk_css_style_property_register        ("font-size",
                                          GTK_CSS_PROPERTY_FONT_SIZE,
                                          GTK_STYLE_PROPERTY_INHERIT | GTK_STYLE_PROPERTY_ANIMATED,
                                          GTK_CSS_AFFECTS_SIZE | GTK_CSS_AFFECTS_TEXT_SIZE,
                                          font_size_parse,
                                          _gtk_css_font_size_value_new (GTK_CSS_FONT_SIZE_MEDIUM));
  gtk_css_style_property_register        ("-gtk-icon-palette",
					  GTK_CSS_PROPERTY_ICON_PALETTE,
					  GTK_STYLE_PROPERTY_ANIMATED | GTK_STYLE_PROPERTY_INHERIT,
                                          GTK_CSS_AFFECTS_ICON_REDRAW_SYMBOLIC,
					  icon_palette_parse,
					  gtk_css_palette_value_new_default ());


  /* properties that aren't referenced when computing values
   * start here */
  gtk_css_style_property_register        ("background-color",
                                          GTK_CSS_PROPERTY_BACKGROUND_COLOR,
                                          GTK_STYLE_PROPERTY_ANIMATED,
                                          GTK_CSS_AFFECTS_BACKGROUND,
                                          color_parse,
                                          gtk_css_color_value_new_transparent ());

  gtk_css_style_property_register        ("font-family",
                                          GTK_CSS_PROPERTY_FONT_FAMILY,
                                          GTK_STYLE_PROPERTY_INHERIT,
                                          GTK_CSS_AFFECTS_TEXT_SIZE,
                                          font_family_parse,
                                          _gtk_css_string_value_new ("Sans"));
  gtk_css_style_property_register        ("font-style",
                                          GTK_CSS_PROPERTY_FONT_STYLE,
                                          GTK_STYLE_PROPERTY_INHERIT,
                                          GTK_CSS_AFFECTS_TEXT_SIZE,
                                          font_style_parse,
                                          _gtk_css_font_style_value_new (PANGO_STYLE_NORMAL));
  gtk_css_style_property_register        ("font-weight",
                                          GTK_CSS_PROPERTY_FONT_WEIGHT,
                                          GTK_STYLE_PROPERTY_INHERIT | GTK_STYLE_PROPERTY_ANIMATED,
                                          GTK_CSS_AFFECTS_TEXT_SIZE,
                                          font_weight_parse,
                                          _gtk_css_number_value_new (PANGO_WEIGHT_NORMAL, GTK_CSS_NUMBER));
  gtk_css_style_property_register        ("font-stretch",
                                          GTK_CSS_PROPERTY_FONT_STRETCH,
                                          GTK_STYLE_PROPERTY_INHERIT,
                                          GTK_CSS_AFFECTS_TEXT_SIZE,
                                          font_stretch_parse,
                                          _gtk_css_font_stretch_value_new (PANGO_STRETCH_NORMAL));

  gtk_css_style_property_register        ("letter-spacing",
                                          GTK_CSS_PROPERTY_LETTER_SPACING,
                                          GTK_STYLE_PROPERTY_INHERIT | GTK_STYLE_PROPERTY_ANIMATED,
                                          GTK_CSS_AFFECTS_TEXT_ATTRS | GTK_CSS_AFFECTS_TEXT_SIZE,
                                          parse_letter_spacing,
                                          _gtk_css_number_value_new (0.0, GTK_CSS_PX));

  gtk_css_style_property_register        ("text-decoration-line",
                                          GTK_CSS_PROPERTY_TEXT_DECORATION_LINE,
                                          0,
                                          GTK_CSS_AFFECTS_TEXT_ATTRS,
                                          parse_text_decoration_line,
                                          _gtk_css_text_decoration_line_value_new (GTK_CSS_TEXT_DECORATION_LINE_NONE));
  gtk_css_style_property_register        ("text-decoration-color",
                                          GTK_CSS_PROPERTY_TEXT_DECORATION_COLOR,
                                          GTK_STYLE_PROPERTY_ANIMATED,
                                          GTK_CSS_AFFECTS_TEXT_ATTRS,
                                          color_parse,
                                          _gtk_css_color_value_new_current_color ());
  gtk_css_style_property_register        ("text-decoration-style",
                                          GTK_CSS_PROPERTY_TEXT_DECORATION_STYLE,
                                          0,
                                          GTK_CSS_AFFECTS_TEXT_ATTRS,
                                          parse_text_decoration_style,
                                          _gtk_css_text_decoration_style_value_new (GTK_CSS_TEXT_DECORATION_STYLE_SOLID));
  gtk_css_style_property_register        ("text-transform",
                                          GTK_CSS_PROPERTY_TEXT_TRANSFORM,
                                          0,
                                          GTK_CSS_AFFECTS_TEXT_ATTRS | GTK_CSS_AFFECTS_TEXT_SIZE,
                                          parse_text_transform,
                                          _gtk_css_text_transform_value_new (GTK_CSS_TEXT_TRANSFORM_NONE));
  gtk_css_style_property_register        ("font-kerning",
                                          GTK_CSS_PROPERTY_FONT_KERNING,
                                          0,
                                          GTK_CSS_AFFECTS_TEXT_ATTRS | GTK_CSS_AFFECTS_TEXT_SIZE,
                                          parse_font_kerning,
                                          _gtk_css_font_kerning_value_new (GTK_CSS_FONT_KERNING_AUTO));
  gtk_css_style_property_register        ("font-variant-ligatures",
                                          GTK_CSS_PROPERTY_FONT_VARIANT_LIGATURES,
                                          0,
                                          GTK_CSS_AFFECTS_TEXT_ATTRS,
                                          parse_font_variant_ligatures,
                                          _gtk_css_font_variant_ligature_value_new (GTK_CSS_FONT_VARIANT_LIGATURE_NORMAL));
  gtk_css_style_property_register        ("font-variant-position",
                                          GTK_CSS_PROPERTY_FONT_VARIANT_POSITION,
                                          0,
                                          GTK_CSS_AFFECTS_TEXT_ATTRS,
                                          parse_font_variant_position,
                                          _gtk_css_font_variant_position_value_new (GTK_CSS_FONT_VARIANT_POSITION_NORMAL));
  gtk_css_style_property_register        ("font-variant-caps",
                                          GTK_CSS_PROPERTY_FONT_VARIANT_CAPS,
                                          0,
                                          GTK_CSS_AFFECTS_TEXT_ATTRS,
                                          parse_font_variant_caps,
                                          _gtk_css_font_variant_caps_value_new (GTK_CSS_FONT_VARIANT_CAPS_NORMAL));
  gtk_css_style_property_register        ("font-variant-numeric",
                                          GTK_CSS_PROPERTY_FONT_VARIANT_NUMERIC,
                                          0,
                                          GTK_CSS_AFFECTS_TEXT_ATTRS,
                                          parse_font_variant_numeric,
                                          _gtk_css_font_variant_numeric_value_new (GTK_CSS_FONT_VARIANT_NUMERIC_NORMAL));
  gtk_css_style_property_register        ("font-variant-alternates",
                                          GTK_CSS_PROPERTY_FONT_VARIANT_ALTERNATES,
                                          0,
                                          GTK_CSS_AFFECTS_TEXT_ATTRS,
                                          parse_font_variant_alternates,
                                          _gtk_css_font_variant_alternate_value_new (GTK_CSS_FONT_VARIANT_ALTERNATE_NORMAL));
  gtk_css_style_property_register        ("font-variant-east-asian",
                                          GTK_CSS_PROPERTY_FONT_VARIANT_EAST_ASIAN,
                                          0,
                                          GTK_CSS_AFFECTS_TEXT_ATTRS,
                                          parse_font_variant_east_asian,
                                          _gtk_css_font_variant_east_asian_value_new (GTK_CSS_FONT_VARIANT_EAST_ASIAN_NORMAL));
  gtk_css_style_property_register        ("text-shadow",
                                          GTK_CSS_PROPERTY_TEXT_SHADOW,
                                          GTK_STYLE_PROPERTY_INHERIT | GTK_STYLE_PROPERTY_ANIMATED,
                                          GTK_CSS_AFFECTS_TEXT_CONTENT,
                                          shadow_value_parse,
                                          gtk_css_shadow_value_new_none ());

  gtk_css_style_property_register        ("box-shadow",
                                          GTK_CSS_PROPERTY_BOX_SHADOW,
                                          GTK_STYLE_PROPERTY_ANIMATED,
                                          GTK_CSS_AFFECTS_BACKGROUND,
                                          box_shadow_value_parse,
                                          gtk_css_shadow_value_new_none ());

  gtk_css_style_property_register        ("margin-top",
                                          GTK_CSS_PROPERTY_MARGIN_TOP,
                                          GTK_STYLE_PROPERTY_ANIMATED,
                                          GTK_CSS_AFFECTS_SIZE,
                                          parse_margin,
                                          _gtk_css_number_value_new (0.0, GTK_CSS_PX));
  gtk_css_style_property_register        ("margin-left",
                                          GTK_CSS_PROPERTY_MARGIN_LEFT,
                                          GTK_STYLE_PROPERTY_ANIMATED,
                                          GTK_CSS_AFFECTS_SIZE,
                                          parse_margin,
                                          _gtk_css_number_value_new (0.0, GTK_CSS_PX));
  gtk_css_style_property_register        ("margin-bottom",
                                          GTK_CSS_PROPERTY_MARGIN_BOTTOM,
                                          GTK_STYLE_PROPERTY_ANIMATED,
                                          GTK_CSS_AFFECTS_SIZE,
                                          parse_margin,
                                          _gtk_css_number_value_new (0.0, GTK_CSS_PX));
  gtk_css_style_property_register        ("margin-right",
                                          GTK_CSS_PROPERTY_MARGIN_RIGHT,
                                          GTK_STYLE_PROPERTY_ANIMATED,
                                          GTK_CSS_AFFECTS_SIZE,
                                          parse_margin,
                                          _gtk_css_number_value_new (0.0, GTK_CSS_PX));
  gtk_css_style_property_register        ("padding-top",
                                          GTK_CSS_PROPERTY_PADDING_TOP,
                                          GTK_STYLE_PROPERTY_ANIMATED,
                                          GTK_CSS_AFFECTS_SIZE,
                                          parse_padding,
                                          _gtk_css_number_value_new (0.0, GTK_CSS_PX));
  gtk_css_style_property_register        ("padding-left",
                                          GTK_CSS_PROPERTY_PADDING_LEFT,
                                          GTK_STYLE_PROPERTY_ANIMATED,
                                          GTK_CSS_AFFECTS_SIZE,
                                          parse_padding,
                                          _gtk_css_number_value_new (0.0, GTK_CSS_PX));
  gtk_css_style_property_register        ("padding-bottom",
                                          GTK_CSS_PROPERTY_PADDING_BOTTOM,
                                          GTK_STYLE_PROPERTY_ANIMATED,
                                          GTK_CSS_AFFECTS_SIZE,
                                          parse_padding,
                                          _gtk_css_number_value_new (0.0, GTK_CSS_PX));
  gtk_css_style_property_register        ("padding-right",
                                          GTK_CSS_PROPERTY_PADDING_RIGHT,
                                          GTK_STYLE_PROPERTY_ANIMATED,
                                          GTK_CSS_AFFECTS_SIZE,
                                          parse_padding,
                                          _gtk_css_number_value_new (0.0, GTK_CSS_PX));
  /* IMPORTANT: the border-width properties must come after border-style properties,
   * they depend on them for their value computation.
   */
  gtk_css_style_property_register        ("border-top-style",
                                          GTK_CSS_PROPERTY_BORDER_TOP_STYLE,
                                          0,
                                          GTK_CSS_AFFECTS_BORDER,
                                          parse_border_style,
                                          _gtk_css_border_style_value_new (GTK_BORDER_STYLE_NONE));
  gtk_css_style_property_register        ("border-top-width",
                                          GTK_CSS_PROPERTY_BORDER_TOP_WIDTH,
                                          GTK_STYLE_PROPERTY_ANIMATED,
                                          GTK_CSS_AFFECTS_BORDER | GTK_CSS_AFFECTS_SIZE,
                                          parse_border_width,
                                          _gtk_css_number_value_new (0.0, GTK_CSS_PX));
  gtk_css_style_property_register        ("border-left-style",
                                          GTK_CSS_PROPERTY_BORDER_LEFT_STYLE,
                                          0,
                                          GTK_CSS_AFFECTS_BORDER,
                                          parse_border_style,
                                          _gtk_css_border_style_value_new (GTK_BORDER_STYLE_NONE));
  gtk_css_style_property_register        ("border-left-width",
                                          GTK_CSS_PROPERTY_BORDER_LEFT_WIDTH,
                                          GTK_STYLE_PROPERTY_ANIMATED,
                                          GTK_CSS_AFFECTS_BORDER | GTK_CSS_AFFECTS_SIZE,
                                          parse_border_width,
                                          _gtk_css_number_value_new (0.0, GTK_CSS_PX));
  gtk_css_style_property_register        ("border-bottom-style",
                                          GTK_CSS_PROPERTY_BORDER_BOTTOM_STYLE,
                                          0,
                                          GTK_CSS_AFFECTS_BORDER,
                                          parse_border_style,
                                          _gtk_css_border_style_value_new (GTK_BORDER_STYLE_NONE));
  gtk_css_style_property_register        ("border-bottom-width",
                                          GTK_CSS_PROPERTY_BORDER_BOTTOM_WIDTH,
                                          GTK_STYLE_PROPERTY_ANIMATED,
                                          GTK_CSS_AFFECTS_BORDER | GTK_CSS_AFFECTS_SIZE,
                                          parse_border_width,
                                          _gtk_css_number_value_new (0.0, GTK_CSS_PX));
  gtk_css_style_property_register        ("border-right-style",
                                          GTK_CSS_PROPERTY_BORDER_RIGHT_STYLE,
                                          0,
                                          GTK_CSS_AFFECTS_BORDER,
                                          parse_border_style,
                                          _gtk_css_border_style_value_new (GTK_BORDER_STYLE_NONE));
  gtk_css_style_property_register        ("border-right-width",
                                          GTK_CSS_PROPERTY_BORDER_RIGHT_WIDTH,
                                          GTK_STYLE_PROPERTY_ANIMATED,
                                          GTK_CSS_AFFECTS_BORDER | GTK_CSS_AFFECTS_SIZE,
                                          parse_border_width,
                                          _gtk_css_number_value_new (0.0, GTK_CSS_PX));

  gtk_css_style_property_register        ("border-top-left-radius",
                                          GTK_CSS_PROPERTY_BORDER_TOP_LEFT_RADIUS,
                                          GTK_STYLE_PROPERTY_ANIMATED,
                                          GTK_CSS_AFFECTS_BACKGROUND | GTK_CSS_AFFECTS_BORDER,
                                          border_corner_radius_value_parse,
                                          _gtk_css_corner_value_new (_gtk_css_number_value_new (0, GTK_CSS_PX),
                                                                     _gtk_css_number_value_new (0, GTK_CSS_PX)));
  gtk_css_style_property_register        ("border-top-right-radius",
                                          GTK_CSS_PROPERTY_BORDER_TOP_RIGHT_RADIUS,
                                          GTK_STYLE_PROPERTY_ANIMATED,
                                          GTK_CSS_AFFECTS_BACKGROUND | GTK_CSS_AFFECTS_BORDER,
                                          border_corner_radius_value_parse,
                                          _gtk_css_corner_value_new (_gtk_css_number_value_new (0, GTK_CSS_PX),
                                                                     _gtk_css_number_value_new (0, GTK_CSS_PX)));
  gtk_css_style_property_register        ("border-bottom-right-radius",
                                          GTK_CSS_PROPERTY_BORDER_BOTTOM_RIGHT_RADIUS,
                                          GTK_STYLE_PROPERTY_ANIMATED,
                                          GTK_CSS_AFFECTS_BACKGROUND | GTK_CSS_AFFECTS_BORDER,
                                          border_corner_radius_value_parse,
                                          _gtk_css_corner_value_new (_gtk_css_number_value_new (0, GTK_CSS_PX),
                                                                     _gtk_css_number_value_new (0, GTK_CSS_PX)));
  gtk_css_style_property_register        ("border-bottom-left-radius",
                                          GTK_CSS_PROPERTY_BORDER_BOTTOM_LEFT_RADIUS,
                                          GTK_STYLE_PROPERTY_ANIMATED,
                                          GTK_CSS_AFFECTS_BACKGROUND | GTK_CSS_AFFECTS_BORDER,
                                          border_corner_radius_value_parse,
                                          _gtk_css_corner_value_new (_gtk_css_number_value_new (0, GTK_CSS_PX),
                                                                     _gtk_css_number_value_new (0, GTK_CSS_PX)));

  gtk_css_style_property_register        ("outline-style",
                                          GTK_CSS_PROPERTY_OUTLINE_STYLE,
                                          0,
                                          GTK_CSS_AFFECTS_OUTLINE,
                                          parse_border_style,
                                          _gtk_css_border_style_value_new (GTK_BORDER_STYLE_NONE));
  gtk_css_style_property_register        ("outline-width",
                                          GTK_CSS_PROPERTY_OUTLINE_WIDTH,
                                          GTK_STYLE_PROPERTY_ANIMATED,
                                          GTK_CSS_AFFECTS_OUTLINE,
                                          parse_border_width,
                                          _gtk_css_number_value_new (0.0, GTK_CSS_PX));
  gtk_css_style_property_register        ("outline-offset",
                                          GTK_CSS_PROPERTY_OUTLINE_OFFSET,
                                          GTK_STYLE_PROPERTY_ANIMATED,
                                          GTK_CSS_AFFECTS_OUTLINE,
                                          outline_parse,
                                          _gtk_css_number_value_new (0.0, GTK_CSS_PX));
  gtk_css_style_property_register        ("background-clip",
                                          GTK_CSS_PROPERTY_BACKGROUND_CLIP,
                                          0,
                                          GTK_CSS_AFFECTS_BACKGROUND,
                                          parse_css_area,
                                          _gtk_css_area_value_new (GTK_CSS_AREA_BORDER_BOX));
  gtk_css_style_property_register        ("background-origin",
                                          GTK_CSS_PROPERTY_BACKGROUND_ORIGIN,
                                          0,
                                          GTK_CSS_AFFECTS_BACKGROUND,
                                          parse_css_area,
                                          _gtk_css_area_value_new (GTK_CSS_AREA_PADDING_BOX));
  gtk_css_style_property_register        ("background-size",
                                          GTK_CSS_PROPERTY_BACKGROUND_SIZE,
                                          GTK_STYLE_PROPERTY_ANIMATED,
                                          GTK_CSS_AFFECTS_BACKGROUND,
                                          background_size_parse,
                                          _gtk_css_bg_size_value_new (NULL, NULL));
  gtk_css_style_property_register        ("background-position",
                                          GTK_CSS_PROPERTY_BACKGROUND_POSITION,
                                          GTK_STYLE_PROPERTY_ANIMATED,
                                          GTK_CSS_AFFECTS_BACKGROUND,
                                          background_position_parse,
                                          _gtk_css_position_value_new (_gtk_css_number_value_new (0, GTK_CSS_PERCENT),
                                                                       _gtk_css_number_value_new (0, GTK_CSS_PERCENT)));

  gtk_css_style_property_register        ("border-top-color",
                                          GTK_CSS_PROPERTY_BORDER_TOP_COLOR,
                                          GTK_STYLE_PROPERTY_ANIMATED,
                                          GTK_CSS_AFFECTS_BORDER,
                                          color_parse,
                                          _gtk_css_color_value_new_current_color ());
  gtk_css_style_property_register        ("border-right-color",
                                          GTK_CSS_PROPERTY_BORDER_RIGHT_COLOR,
                                          GTK_STYLE_PROPERTY_ANIMATED,
                                          GTK_CSS_AFFECTS_BORDER,
                                          color_parse,
                                          _gtk_css_color_value_new_current_color ());
  gtk_css_style_property_register        ("border-bottom-color",
                                          GTK_CSS_PROPERTY_BORDER_BOTTOM_COLOR,
                                          GTK_STYLE_PROPERTY_ANIMATED,
                                          GTK_CSS_AFFECTS_BORDER,
                                          color_parse,
                                          _gtk_css_color_value_new_current_color ());
  gtk_css_style_property_register        ("border-left-color",
                                          GTK_CSS_PROPERTY_BORDER_LEFT_COLOR,
                                          GTK_STYLE_PROPERTY_ANIMATED,
                                          GTK_CSS_AFFECTS_BORDER,
                                          color_parse,
                                          _gtk_css_color_value_new_current_color ());
  gtk_css_style_property_register        ("outline-color",
                                          GTK_CSS_PROPERTY_OUTLINE_COLOR,
                                          GTK_STYLE_PROPERTY_ANIMATED,
                                          GTK_CSS_AFFECTS_OUTLINE,
                                          color_parse,
                                          _gtk_css_color_value_new_current_color ());

  gtk_css_style_property_register        ("background-repeat",
                                          GTK_CSS_PROPERTY_BACKGROUND_REPEAT,
                                          0,
                                          GTK_CSS_AFFECTS_BACKGROUND,
                                          background_repeat_value_parse,
                                          _gtk_css_background_repeat_value_new (GTK_CSS_REPEAT_STYLE_REPEAT,
                                                                                GTK_CSS_REPEAT_STYLE_REPEAT));
  gtk_css_style_property_register        ("background-image",
                                          GTK_CSS_PROPERTY_BACKGROUND_IMAGE,
                                          GTK_STYLE_PROPERTY_ANIMATED,
                                          GTK_CSS_AFFECTS_BACKGROUND,
                                          background_image_value_parse,
                                          _gtk_css_image_value_new (NULL));

  gtk_css_style_property_register        ("background-blend-mode",
                                          GTK_CSS_PROPERTY_BACKGROUND_BLEND_MODE,
                                          0,
                                          GTK_CSS_AFFECTS_BACKGROUND,
                                          blend_mode_value_parse,
                                          _gtk_css_blend_mode_value_new (GSK_BLEND_MODE_DEFAULT));

  gtk_css_style_property_register        ("border-image-source",
                                          GTK_CSS_PROPERTY_BORDER_IMAGE_SOURCE,
                                          GTK_STYLE_PROPERTY_ANIMATED,
                                          GTK_CSS_AFFECTS_BORDER,
                                          css_image_value_parse,
                                          _gtk_css_image_value_new (NULL));
  gtk_css_style_property_register        ("border-image-repeat",
                                          GTK_CSS_PROPERTY_BORDER_IMAGE_REPEAT,
                                          0,
                                          GTK_CSS_AFFECTS_BORDER,
                                          border_image_repeat_parse,
                                          _gtk_css_border_repeat_value_new (GTK_CSS_REPEAT_STYLE_STRETCH,
                                                                            GTK_CSS_REPEAT_STYLE_STRETCH));

  gtk_css_style_property_register        ("border-image-slice",
                                          GTK_CSS_PROPERTY_BORDER_IMAGE_SLICE,
                                          0,
                                          GTK_CSS_AFFECTS_BORDER,
                                          border_image_slice_parse,
                                          _gtk_css_border_value_new (_gtk_css_number_value_new (100, GTK_CSS_PERCENT),
                                                                     _gtk_css_number_value_new (100, GTK_CSS_PERCENT),
                                                                     _gtk_css_number_value_new (100, GTK_CSS_PERCENT),
                                                                     _gtk_css_number_value_new (100, GTK_CSS_PERCENT)));
  gtk_css_style_property_register        ("border-image-width",
                                          GTK_CSS_PROPERTY_BORDER_IMAGE_WIDTH,
                                          0,
                                          GTK_CSS_AFFECTS_BORDER,
                                          border_image_width_parse,
                                          _gtk_css_border_value_new (_gtk_css_number_value_new (1, GTK_CSS_NUMBER),
                                                                     _gtk_css_number_value_new (1, GTK_CSS_NUMBER),
                                                                     _gtk_css_number_value_new (1, GTK_CSS_NUMBER),
                                                                     _gtk_css_number_value_new (1, GTK_CSS_NUMBER)));

  gtk_css_style_property_register        ("-gtk-icon-source",
                                          GTK_CSS_PROPERTY_ICON_SOURCE,
                                          GTK_STYLE_PROPERTY_ANIMATED,
                                          GTK_CSS_AFFECTS_ICON_TEXTURE,
                                          css_image_value_parse,
                                          _gtk_css_image_value_new (NULL));
  gtk_css_style_property_register        ("-gtk-icon-size",
                                          GTK_CSS_PROPERTY_ICON_SIZE,
                                          GTK_STYLE_PROPERTY_INHERIT | GTK_STYLE_PROPERTY_ANIMATED,
                                          GTK_CSS_AFFECTS_SIZE | GTK_CSS_AFFECTS_ICON_SIZE,
                                          icon_size_parse,
                                          _gtk_css_number_value_new (16, GTK_CSS_PX));
  gtk_css_style_property_register        ("-gtk-icon-shadow",
                                          GTK_CSS_PROPERTY_ICON_SHADOW,
                                          GTK_STYLE_PROPERTY_INHERIT | GTK_STYLE_PROPERTY_ANIMATED,
                                          GTK_CSS_AFFECTS_ICON_REDRAW,
                                          shadow_value_parse,
                                          gtk_css_shadow_value_new_none ());
  gtk_css_style_property_register        ("-gtk-icon-style",
                                          GTK_CSS_PROPERTY_ICON_STYLE,
                                          GTK_STYLE_PROPERTY_INHERIT,
                                          GTK_CSS_AFFECTS_ICON_TEXTURE,
                                          icon_style_parse,
                                          _gtk_css_icon_style_value_new (GTK_CSS_ICON_STYLE_REQUESTED));
  gtk_css_style_property_register        ("-gtk-icon-transform",
                                          GTK_CSS_PROPERTY_ICON_TRANSFORM,
                                          GTK_STYLE_PROPERTY_ANIMATED,
                                          GTK_CSS_AFFECTS_CONTENT,
                                          transform_value_parse,
                                          _gtk_css_transform_value_new_none ());
  gtk_css_style_property_register        ("-gtk-icon-filter",
                                          GTK_CSS_PROPERTY_ICON_FILTER,
                                          GTK_STYLE_PROPERTY_ANIMATED,
                                          GTK_CSS_AFFECTS_CONTENT,
                                          filter_value_parse,
                                          gtk_css_filter_value_new_none ());
  gtk_css_style_property_register        ("border-spacing",
                                          GTK_CSS_PROPERTY_BORDER_SPACING,
                                          GTK_STYLE_PROPERTY_ANIMATED,
                                          GTK_CSS_AFFECTS_SIZE,
                                          border_spacing_value_parse,
                                          _gtk_css_position_value_new (_gtk_css_number_value_new (0, GTK_CSS_PX),
                                                                       _gtk_css_number_value_new (0, GTK_CSS_PX)));

  gtk_css_style_property_register        ("transform",
                                          GTK_CSS_PROPERTY_TRANSFORM,
                                          GTK_STYLE_PROPERTY_ANIMATED,
                                          GTK_CSS_AFFECTS_TRANSFORM,
                                          transform_value_parse,
                                          _gtk_css_transform_value_new_none ());
  gtk_css_style_property_register        ("transform-origin",
                                          GTK_CSS_PROPERTY_TRANSFORM_ORIGIN,
                                          GTK_STYLE_PROPERTY_ANIMATED,
                                          GTK_CSS_AFFECTS_TRANSFORM,
                                          transform_origin_parse,
                                          _gtk_css_position_value_new (_gtk_css_number_value_new (50, GTK_CSS_PERCENT),
                                                                       _gtk_css_number_value_new (50, GTK_CSS_PERCENT)));
  gtk_css_style_property_register        ("min-width",
                                          GTK_CSS_PROPERTY_MIN_WIDTH,
                                          GTK_STYLE_PROPERTY_ANIMATED,
                                          GTK_CSS_AFFECTS_SIZE,
                                          minmax_parse,
                                          _gtk_css_number_value_new (0, GTK_CSS_PX));
  gtk_css_style_property_register        ("min-height",
                                          GTK_CSS_PROPERTY_MIN_HEIGHT,
                                          GTK_STYLE_PROPERTY_ANIMATED,
                                          GTK_CSS_AFFECTS_SIZE,
                                          minmax_parse,
                                          _gtk_css_number_value_new (0, GTK_CSS_PX));

  gtk_css_style_property_register        ("transition-property",
                                          GTK_CSS_PROPERTY_TRANSITION_PROPERTY,
                                          0,
                                          0,
                                          transition_property_parse,
                                          _gtk_css_ident_value_new ("all"));
  gtk_css_style_property_register        ("transition-duration",
                                          GTK_CSS_PROPERTY_TRANSITION_DURATION,
                                          0,
                                          0,
                                          transition_time_parse,
                                          _gtk_css_number_value_new (0, GTK_CSS_S));
  gtk_css_style_property_register        ("transition-timing-function",
                                          GTK_CSS_PROPERTY_TRANSITION_TIMING_FUNCTION,
                                          0,
                                          0,
                                          transition_timing_function_parse,
                                          _gtk_css_ease_value_new_cubic_bezier (0.25, 0.1, 0.25, 1.0));
  gtk_css_style_property_register        ("transition-delay",
                                          GTK_CSS_PROPERTY_TRANSITION_DELAY,
                                          0,
                                          0,
                                          transition_time_parse,
                                          _gtk_css_number_value_new (0, GTK_CSS_S));

  gtk_css_style_property_register        ("animation-name",
                                          GTK_CSS_PROPERTY_ANIMATION_NAME,
                                          0,
                                          0,
                                          transition_property_parse,
                                          _gtk_css_ident_value_new ("none"));
  gtk_css_style_property_register        ("animation-duration",
                                          GTK_CSS_PROPERTY_ANIMATION_DURATION,
                                          0,
                                          0,
                                          transition_time_parse,
                                          _gtk_css_number_value_new (0, GTK_CSS_S));
  gtk_css_style_property_register        ("animation-timing-function",
                                          GTK_CSS_PROPERTY_ANIMATION_TIMING_FUNCTION,
                                          0,
                                          0,
                                          transition_timing_function_parse,
                                          _gtk_css_ease_value_new_cubic_bezier (0.25, 0.1, 0.25, 1.0));
  gtk_css_style_property_register        ("animation-iteration-count",
                                          GTK_CSS_PROPERTY_ANIMATION_ITERATION_COUNT,
                                          0,
                                          0,
                                          iteration_count_parse,
                                          _gtk_css_number_value_new (1, GTK_CSS_NUMBER));
  gtk_css_style_property_register        ("animation-direction",
                                          GTK_CSS_PROPERTY_ANIMATION_DIRECTION,
                                          0,
                                          0,
                                          parse_css_direction,
                                          _gtk_css_direction_value_new (GTK_CSS_DIRECTION_NORMAL));
  gtk_css_style_property_register        ("animation-play-state",
                                          GTK_CSS_PROPERTY_ANIMATION_PLAY_STATE,
                                          0,
                                          0,
                                          parse_css_play_state,
                                          _gtk_css_play_state_value_new (GTK_CSS_PLAY_STATE_RUNNING));
  gtk_css_style_property_register        ("animation-delay",
                                          GTK_CSS_PROPERTY_ANIMATION_DELAY,
                                          0,
                                          0,
                                          transition_time_parse,
                                          _gtk_css_number_value_new (0, GTK_CSS_S));
  gtk_css_style_property_register        ("animation-fill-mode",
                                          GTK_CSS_PROPERTY_ANIMATION_FILL_MODE,
                                          0,
                                          0,
                                          parse_css_fill_mode,
                                          _gtk_css_fill_mode_value_new (GTK_CSS_FILL_NONE));

  gtk_css_style_property_register        ("opacity",
                                          GTK_CSS_PROPERTY_OPACITY,
                                          GTK_STYLE_PROPERTY_ANIMATED,
                                          GTK_CSS_AFFECTS_POSTEFFECT,
                                          opacity_parse,
                                          _gtk_css_number_value_new (1, GTK_CSS_NUMBER));
  gtk_css_style_property_register        ("filter",
                                          GTK_CSS_PROPERTY_FILTER,
                                          GTK_STYLE_PROPERTY_ANIMATED,
                                          GTK_CSS_AFFECTS_POSTEFFECT,
                                          filter_value_parse,
                                          gtk_css_filter_value_new_none ());

  gtk_css_style_property_register        ("caret-color",
                                          GTK_CSS_PROPERTY_CARET_COLOR,
                                          GTK_STYLE_PROPERTY_INHERIT | GTK_STYLE_PROPERTY_ANIMATED,
                                          GTK_CSS_AFFECTS_CONTENT,
                                          color_parse,
                                          _gtk_css_color_value_new_current_color ());
  gtk_css_style_property_register        ("-gtk-secondary-caret-color",
                                          GTK_CSS_PROPERTY_SECONDARY_CARET_COLOR,
                                          GTK_STYLE_PROPERTY_INHERIT | GTK_STYLE_PROPERTY_ANIMATED,
                                          GTK_CSS_AFFECTS_CONTENT,
                                          color_parse,
                                          _gtk_css_color_value_new_current_color ());
  gtk_css_style_property_register        ("font-feature-settings",
                                          GTK_CSS_PROPERTY_FONT_FEATURE_SETTINGS,
                                          GTK_STYLE_PROPERTY_INHERIT | GTK_STYLE_PROPERTY_ANIMATED,
                                          GTK_CSS_AFFECTS_TEXT_ATTRS | GTK_CSS_AFFECTS_TEXT_SIZE,
                                          parse_font_feature_settings,
                                          gtk_css_font_features_value_new_default ());
  gtk_css_style_property_register        ("font-variation-settings",
                                          GTK_CSS_PROPERTY_FONT_VARIATION_SETTINGS,
                                          GTK_STYLE_PROPERTY_INHERIT | GTK_STYLE_PROPERTY_ANIMATED,
                                          GTK_CSS_AFFECTS_TEXT_ATTRS | GTK_CSS_AFFECTS_TEXT_SIZE,
                                          parse_font_variation_settings,
                                          gtk_css_font_variations_value_new_default ());
  gtk_css_style_property_register        ("line-height",
                                          GTK_CSS_PROPERTY_LINE_HEIGHT,
                                          GTK_STYLE_PROPERTY_INHERIT | GTK_STYLE_PROPERTY_ANIMATED,
                                          GTK_CSS_AFFECTS_TEXT_ATTRS | GTK_CSS_AFFECTS_TEXT_SIZE,
                                          parse_line_height,
                                          _gtk_css_value_ref (gtk_css_line_height_value_get_default ()));
}
