/*
 * Copyright (C) 2021 Red Hat Inc.
 *
 * Author:
 *      Matthias Clasen <mclasen@redhat.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include <gtk/gtk.h>
#include "gtk/gtkcssvalueprivate.h"
#include "gtk/gtkcsscolorvalueprivate.h"
#include "gtk/gtkcssnumbervalueprivate.h"
#include "gtk/gtkcssstylepropertyprivate.h"
#include "gtk/gtkcssstaticstyleprivate.h"
#include "gtk/gtkcsspalettevalueprivate.h"

static gboolean
color_is_near (const GdkRGBA *color1,
               const GdkRGBA *color2)
{
  return (fabs (color1->red - color2->red) <= FLT_EPSILON &&
          fabs (color1->green - color2->green) <= FLT_EPSILON &&
          fabs (color1->blue - color2->blue) <= FLT_EPSILON &&
          fabs (color1->alpha - color2->alpha) <= FLT_EPSILON);
}

static gboolean
value_is_near (int          prop,
               GtkCssValue *value1,
               GtkCssValue *value2)
{
  if (_gtk_css_value_equal (value1, value2))
    return TRUE;

  switch (prop)
    {
    case GTK_CSS_PROPERTY_COLOR:
      return color_is_near (gtk_css_color_value_get_rgba (value1),
                            gtk_css_color_value_get_rgba (value2));
      break;

    case GTK_CSS_PROPERTY_ICON_PALETTE:
      return color_is_near (gtk_css_palette_value_get_color (value1, "error"),
                            gtk_css_palette_value_get_color (value2, "error")) &&
             color_is_near (gtk_css_palette_value_get_color (value1, "warning"),
                            gtk_css_palette_value_get_color (value2, "warning")) &&
             color_is_near (gtk_css_palette_value_get_color (value1, "test"),
                            gtk_css_palette_value_get_color (value2, "test"));
      break;

    case GTK_CSS_PROPERTY_FONT_SIZE:
      return fabs (_gtk_css_number_value_get (value1, 100) -
                   _gtk_css_number_value_get (value2, 100)) < FLT_EPSILON;
      break;

    default:
      break;
    }

  return FALSE;
}

static void
assert_css_value (int          prop,
                  GtkCssValue *result,
                  GtkCssValue *expected)
{
  if (result == expected)
    return;

  if (((result == NULL) != (expected == NULL)) ||
      !value_is_near (prop, result, expected))
    {
      char *r = result ? _gtk_css_value_to_string (result) : g_strdup ("(nil)");
      char *e = expected ? _gtk_css_value_to_string (expected) : g_strdup ("(nil)");
      g_print ("Expected %s\nGot %s\n", e, r);
      g_free (r);
      g_free (e);

      g_assert_not_reached ();
    }
}

/* Tests for css transitions */

typedef struct {
  int prop;
  const char *value1;
  const char *value2;
  double progress;
  const char *value3;
} ValueTransitionTest;

static ValueTransitionTest tests[] = {
  { GTK_CSS_PROPERTY_COLOR, "transparent", "rgb(255,0,0)", 0.25, "rgba(255,0,0,0.25)" },
  { GTK_CSS_PROPERTY_COLOR, "alpha(red, 0.2)", "alpha(rgb(255,0,0),0.8)", 0.5, "rgba(255,0,0,0.5)" },
  { GTK_CSS_PROPERTY_BOX_SHADOW, "none", "2px 2px 10px 4px rgb(200,200,200)", 0.5, "1px 1px 5px 2px rgba(200,200,200,0.5)" },
  { GTK_CSS_PROPERTY_BOX_SHADOW, "2px 2px 10px 4px rgb(200,200,200)", "none", 0.5, "1px 1px 5px 2px rgba(200,200,200,0.5)" },
  { GTK_CSS_PROPERTY_BOX_SHADOW, "2px 2px 10px 4px rgb(200,200,200), 0px 10px 8px 6px rgb(200,100,0)", "none", 0.5, "1px 1px 5px 2px rgba(200,200,200,0.5), 0px 5px 4px 3px rgba(200,100,0,0.5)" },
  { GTK_CSS_PROPERTY_FONT_SIZE, "12px", "16px", 0.25, "13px" },
  { GTK_CSS_PROPERTY_FONT_SIZE, "10px", "10pt", 0.5, "11.66666667px" },
  { GTK_CSS_PROPERTY_FONT_FAMILY, "cantarell", "sans", 0, "cantarell"},
  { GTK_CSS_PROPERTY_FONT_FAMILY, "cantarell", "sans", 1, "sans" },
  { GTK_CSS_PROPERTY_FONT_FAMILY, "cantarell", "sans", 0.5, NULL },
  { GTK_CSS_PROPERTY_BACKGROUND_POSITION, "20px 10px", "40px", 0.5, "30px calc(5px + 25%)" },
  { GTK_CSS_PROPERTY_BACKGROUND_POSITION, "left, right, 50% 80%",
                                          "right, right, 100%",
                                          0.5,
                                          "50%, 100%, 75% 65%" },
  //TODO We don't currently transition border-image-width
  //{ GTK_CSS_PROPERTY_BORDER_IMAGE_WIDTH, "10px 20px", "0px", 0.5, "5px 10px 0.5px 0.5px" },
  { GTK_CSS_PROPERTY_FILTER, "none", "blur(6px)", 0.5, "blur(3px)" },
  { GTK_CSS_PROPERTY_FILTER, "none", "blur(6px),contrast(0.6)", 0.5, "blur(3px),contrast(0.3)" },
  { GTK_CSS_PROPERTY_FILTER, "contrast(0.6)", "blur(6px)", 0.5, NULL},
  { GTK_CSS_PROPERTY_FILTER, "blur(3px) brightness(60) contrast(0.6) grayscale(60) hue-rotate(calc(5deg + 5deg)) invert(10) opacity(60) saturate(60) sepia(10) drop-shadow(3em 10px 10px red)",
                             "blur(5px) brightness(80) contrast(0.8) grayscale(80) hue-rotate(30deg) invert(30) opacity(80) saturate(80) sepia(30) drop-shadow(5em 30px 30px red)",
                             0.5,
                             "blur(4px) brightness(70) contrast(0.7) grayscale(70) hue-rotate(20deg) invert(20) opacity(70) saturate(70) sepia(20) drop-shadow(4em 20px 20px red)" },
  { GTK_CSS_PROPERTY_FILTER, "brightness(100)",
                             "brightness(100) contrast(0.5) grayscale(20) hue-rotate(100deg) invert(100) opacity(0.5) saturate(0.5) sepia(0.5) blur(10px) drop-shadow(2px 2px 2px red)",
                             0.5,
                             "brightness(100) contrast(0.75) grayscale(10) hue-rotate(50deg) invert(50) opacity(0.75) saturate(0.75) sepia(0.25) blur(5px) drop-shadow(1px 1px 1px red)" },
  { GTK_CSS_PROPERTY_FONT_FEATURE_SETTINGS, "\"dlig\" 0, \"clig\" off, \"c2sc\" 1",
                                            "\"dlig\" 1, \"clig\" 0",
                                            0.3,
                                            "\"dlig\" 0, \"clig\" 0, \"c2sc\" 1" },
  { GTK_CSS_PROPERTY_FONT_FEATURE_SETTINGS, "\"dlig\" 0, \"clig\" off, \"c2sc\" 1",
                                            "\"dlig\" 1, \"clig\" 0",
                                            0.6,
                                            "\"dlig\" 1, \"clig\" 0, \"c2sc\" 1" },
  { GTK_CSS_PROPERTY_FONT_VARIATION_SETTINGS, "\"wght\" 100, \"wdth\" 75",
                                              "\"wght\" 400, \"slnt\" 10",
                                              0.5,
                                              "\"wght\" 250, \"wdth\" 75, \"slnt\" 10" },
  { GTK_CSS_PROPERTY_BORDER_TOP_LEFT_RADIUS, "0", "10px", 0.5, "5px" },
  { GTK_CSS_PROPERTY_BORDER_TOP_LEFT_RADIUS, "2px", "10px", 0.5, "6px" },
  { GTK_CSS_PROPERTY_BORDER_TOP_LEFT_RADIUS, "2px 10px", "10px", 0.5, "6px 10px" },
  { GTK_CSS_PROPERTY_TRANSFORM, "translate(1px,2px) rotate(10deg) scale(1,1) skew(10deg,10deg) skewX(10deg) skewY(10deg)",
                                "translate(3px,4px) rotate(50deg) scale(5,7) skew(20deg,30deg) skewX(20deg) skewY(30deg)",
                                0.5,
                                "translate(2px,3px) rotate(30deg) scale(3,4) skew(15deg,20deg) skewX(15deg) skewY(20deg)" },
  { GTK_CSS_PROPERTY_TRANSFORM, "translate(1px,2px)",
                                "translate(3px,4px) rotate(50deg) scale(5,7) skew(20deg,30deg) skewX(20deg) skewY(30deg)",
                                0.5,
                                "translate(2px,3px) rotate(25deg) scale(3,4) skew(10deg,15deg) skewX(10deg) skewY(15deg)" },
  { GTK_CSS_PROPERTY_TRANSFORM, "translate(2px,3px)", "none", 0.5, "translate(1px,1.5px)" },
  { GTK_CSS_PROPERTY_LINE_HEIGHT, "1.0", "2.0", 0.5, "1.5" },
  { GTK_CSS_PROPERTY_LINE_HEIGHT, "10px", "20px", 0.5, "15px" },
  { GTK_CSS_PROPERTY_LINE_HEIGHT, "100%", "200%", 0.5, "150%" },
  { GTK_CSS_PROPERTY_BACKGROUND_SIZE, "25% 100px", "75% 200px", 0.5, "50% 150px" },
  { GTK_CSS_PROPERTY_BACKGROUND_SIZE, "cover", "cover", 0.3, "cover" },
  { GTK_CSS_PROPERTY_BACKGROUND_SIZE, "contain", "contain", 0.6, "contain" },
  { GTK_CSS_PROPERTY_BACKGROUND_SIZE, "cover", "contain", 0, "cover" },
  { GTK_CSS_PROPERTY_BACKGROUND_SIZE, "cover", "contain", 1, "contain" },
  { GTK_CSS_PROPERTY_ICON_PALETTE, "error rgb(200,0,0), warning rgb(100,100,0), test rgb(20,30,40)",
                                   "warning rgb(200,0,0), error rgb(100,100,0), test rgb(30,40,50)",
                                   0.5,
                                   "error rgb(150,50,0), test rgb(25,35,45), warning rgb(150,50,0)" },
};

static void
error_cb (GtkCssParser         *parser,
          const GtkCssLocation *start,
          const GtkCssLocation *end,
          const GError         *error,
          gpointer              user_data)
{
  *(GError **)user_data = g_error_copy (error);
}

static GtkCssValue *
value_from_string (GtkStyleProperty *prop,
                   const char       *str)
{
  GBytes *bytes;
  GtkCssParser *parser;
  GtkCssValue *value;
  GError *error = NULL;

  bytes = g_bytes_new_static (str, strlen (str));
  parser = gtk_css_parser_new_for_bytes (bytes, NULL, error_cb, &error, NULL);
  value = _gtk_style_property_parse_value (prop, parser);
  g_assert_no_error (error);
  gtk_css_parser_unref (parser);
  g_bytes_unref (bytes);

  return value;
}

static void
test_transition (gconstpointer data)
{
  ValueTransitionTest *test = &tests[GPOINTER_TO_INT (data)];
  GtkStyleProperty *prop;
  GtkCssValue *value1;
  GtkCssValue *value2;
  GtkCssValue *value3;
  GtkCssValue *computed1;
  GtkCssValue *computed2;
  GtkCssValue *computed3;
  GtkCssValue *result;
  GtkStyleProvider *provider;
  GtkCssStyle *style;

  provider = GTK_STYLE_PROVIDER (gtk_settings_get_default ());
  style = gtk_css_static_style_get_default ();

  prop = (GtkStyleProperty *)_gtk_css_style_property_lookup_by_id (test->prop);

  value1 = value_from_string (prop, test->value1);
  g_assert_nonnull (value1);
  computed1 = _gtk_css_value_compute (value1, test->prop, provider, style, NULL);

  value2 = value_from_string (prop, test->value2);
  g_assert_nonnull (value2);
  computed2 = _gtk_css_value_compute (value2, test->prop, provider, style, NULL);

  if (test->value3)
    {
      value3 = value_from_string (prop, test->value3);
      computed3 = _gtk_css_value_compute (value3, test->prop, provider, style, NULL);
    }
  else
    {
      value3 = computed3 = NULL;
    }

  result = _gtk_css_value_transition (computed1, computed2, test->prop, test->progress);
  assert_css_value (test->prop, result, computed3);

  gtk_css_value_unref (value1);
  gtk_css_value_unref (value2);
  if (value3)
    gtk_css_value_unref (value3);
  gtk_css_value_unref (computed1);
  gtk_css_value_unref (computed2);
  if (computed3)
    gtk_css_value_unref (computed3);
  gtk_css_value_unref (result);
}

int
main (int argc, char **argv)
{
  GtkStyleProperty *previous;
  int j;

  gtk_test_init (&argc, &argv);

  previous = NULL;
  j = 0;
  for (int i = 0; i < G_N_ELEMENTS (tests); i++)
    {
      ValueTransitionTest *test = &tests[i];
      GtkStyleProperty *prop;
      char *path;

      prop = (GtkStyleProperty *)_gtk_css_style_property_lookup_by_id (test->prop);
      if (prop != previous)
        {
          previous = prop;
          j = 0;
        }
      else
        j++;

      path = g_strdup_printf ("/css/value/transition/%s/%d", _gtk_style_property_get_name ((GtkStyleProperty *)prop), j);
      g_test_add_data_func (path, GINT_TO_POINTER (i), test_transition);
      g_free (path);
    }

  return g_test_run ();
}
