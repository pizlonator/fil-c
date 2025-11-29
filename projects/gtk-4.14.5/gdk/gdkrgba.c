/* GDK - The GIMP Drawing Kit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
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

/*
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/.
 */

#include "config.h"

#include "gdkrgbaprivate.h"

#include <string.h>
#include <errno.h>
#include <math.h>

#include "gdkhslaprivate.h"

G_DEFINE_BOXED_TYPE (GdkRGBA, gdk_rgba,
                     gdk_rgba_copy, gdk_rgba_free)

/**
 * GdkRGBA:
 * @red: The intensity of the red channel from 0.0 to 1.0 inclusive
 * @green: The intensity of the green channel from 0.0 to 1.0 inclusive
 * @blue: The intensity of the blue channel from 0.0 to 1.0 inclusive
 * @alpha: The opacity of the color from 0.0 for completely translucent to
 *   1.0 for opaque
 *
 * A `GdkRGBA` is used to represent a color, in a way that is compatible
 * with cairo’s notion of color.
 *
 * `GdkRGBA` is a convenient way to pass colors around. It’s based on
 * cairo’s way to deal with colors and mirrors its behavior. All values
 * are in the range from 0.0 to 1.0 inclusive. So the color
 * (0.0, 0.0, 0.0, 0.0) represents transparent black and
 * (1.0, 1.0, 1.0, 1.0) is opaque white. Other values will
 * be clamped to this range when drawing.
 */

/**
 * gdk_rgba_copy:
 * @rgba: a `GdkRGBA`
 *
 * Makes a copy of a `GdkRGBA`.
 *
 * The result must be freed through [method@Gdk.RGBA.free].
 *
 * Returns: A newly allocated `GdkRGBA`, with the same contents as @rgba
 */
GdkRGBA *
gdk_rgba_copy (const GdkRGBA *rgba)
{
  GdkRGBA *copy;

  copy = g_new (GdkRGBA, 1);
  memcpy (copy, rgba, sizeof (GdkRGBA));

  return copy;
}

/**
 * gdk_rgba_free:
 * @rgba: a `GdkRGBA`
 *
 * Frees a `GdkRGBA`.
 */
void
gdk_rgba_free (GdkRGBA *rgba)
{
  g_free (rgba);
}

/**
 * gdk_rgba_is_clear:
 * @rgba: a `GdkRGBA`
 *
 * Checks if an @rgba value is transparent.
 *
 * That is, drawing with the value would not produce any change.
 *
 * Returns: %TRUE if the @rgba is clear
 */
gboolean
gdk_rgba_is_clear (const GdkRGBA *rgba)
{
  return rgba->alpha < ((float) 0x00ff / (float) 0xffff);
}

/**
 * gdk_rgba_is_opaque:
 * @rgba: a `GdkRGBA`
 *
 * Checks if an @rgba value is opaque.
 *
 * That is, drawing with the value will not retain any results
 * from previous contents.
 *
 * Returns: %TRUE if the @rgba is opaque
 */
gboolean
gdk_rgba_is_opaque (const GdkRGBA *rgba)
{
  return rgba->alpha > ((float)0xff00 / (float)0xffff);
}

#define SKIP_WHITESPACES(s) while (*(s) == ' ') (s)++;

/* Parses a single color component from a rgb() or rgba() specification
 * according to CSS3 rules. Compared to exact CSS3 parsing we are liberal
 * in what we accept as follows:
 *
 *  - For non-percentage values, we accept floats in the range 0-255
 *    not just [0-9]+ integers
 *  - For percentage values we accept any float, not just [ 0-9]+ | [0-9]* “.” [0-9]+
 *  - We accept mixed percentages and non-percentages in a single
 *    rgb() or rgba() specification.
 */
static gboolean
parse_rgb_value (const char   *str,
                 char        **endp,
                 double       *number)
{
  const char *p;

  *number = g_ascii_strtod (str, endp);
  if (errno == ERANGE || *endp == str ||
      isinf (*number) || isnan (*number))
    return FALSE;

  p = *endp;

  SKIP_WHITESPACES (p);

  if (*p == '%')
    {
      *endp = (char *)(p + 1);
      *number = CLAMP(*number / 100., 0., 1.);
    }
  else
    {
      *number = CLAMP(*number / 255., 0., 1.);
    }

  return TRUE;
}

/**
 * gdk_rgba_parse:
 * @rgba: the `GdkRGBA` to fill in
 * @spec: the string specifying the color
 *
 * Parses a textual representation of a color.
 *
 * The string can be either one of:
 *
 * - A standard name (Taken from the CSS specification).
 * - A hexadecimal value in the form “\#rgb”, “\#rrggbb”,
 *   “\#rrrgggbbb” or ”\#rrrrggggbbbb”
 * - A hexadecimal value in the form “\#rgba”, “\#rrggbbaa”,
 *   or ”\#rrrrggggbbbbaaaa”
 * - A RGB color in the form “rgb(r,g,b)” (In this case the color
 *   will have full opacity)
 * - A RGBA color in the form “rgba(r,g,b,a)”
 * - A HSL color in the form "hsl(hue, saturation, lightness)"
 * - A HSLA color in the form "hsla(hue, saturation, lightness, alpha)"
 *
 * Where “r”, “g”, “b” and “a” are respectively the red, green,
 * blue and alpha color values. In the last two cases, “r”, “g”,
 * and “b” are either integers in the range 0 to 255 or percentage
 * values in the range 0% to 100%, and a is a floating point value
 * in the range 0 to 1.
 *
 * Returns: %TRUE if the parsing succeeded
 */
gboolean
gdk_rgba_parse (GdkRGBA    *rgba,
                const char *spec)
{
  gboolean has_alpha;
  gboolean is_hsl;
  double r, g, b, a;
  char *str = (char *) spec;
  char *p;

  g_return_val_if_fail (spec != NULL, FALSE);


  if (strncmp (str, "rgba", 4) == 0)
    {
      has_alpha = TRUE;
      is_hsl = FALSE;
      str += 4;
    }
  else if (strncmp (str, "rgb", 3) == 0)
    {
      has_alpha = FALSE;
      is_hsl = FALSE;
      a = 1;
      str += 3;
    }
  else if (strncmp (str, "hsla", 4) == 0)
    {
      has_alpha = TRUE;
      is_hsl = TRUE;
      str += 4;
    }
  else if (strncmp (str, "hsl", 3) == 0)
    {
      has_alpha = FALSE;
      is_hsl = TRUE;
      a = 1;
      str += 3;
    }
  else
    {
      PangoColor pango_color;
      guint16 alpha;

      /* Resort on PangoColor for rgb.txt color
       * map and '#' prefixed colors
       */
      if (pango_color_parse_with_alpha (&pango_color, &alpha, str))
        {
          if (rgba)
            {
              rgba->red = pango_color.red / 65535.;
              rgba->green = pango_color.green / 65535.;
              rgba->blue = pango_color.blue / 65535.;
              rgba->alpha = alpha / 65535.;
            }

          return TRUE;
        }
      else
        return FALSE;
    }

  SKIP_WHITESPACES (str);

  if (*str != '(')
    return FALSE;

  str++;

  /* Parse red */
  SKIP_WHITESPACES (str);
  if (!parse_rgb_value (str, &str, &r))
    return FALSE;
  SKIP_WHITESPACES (str);

  if (*str != ',')
    return FALSE;

  str++;

  /* Parse green */
  SKIP_WHITESPACES (str);
  if (!parse_rgb_value (str, &str, &g))
    return FALSE;
  SKIP_WHITESPACES (str);

  if (*str != ',')
    return FALSE;

  str++;

  /* Parse blue */
  SKIP_WHITESPACES (str);
  if (!parse_rgb_value (str, &str, &b))
    return FALSE;
  SKIP_WHITESPACES (str);

  if (has_alpha)
    {
      if (*str != ',')
        return FALSE;

      str++;

      SKIP_WHITESPACES (str);
      a = g_ascii_strtod (str, &p);
      if (errno == ERANGE || p == str ||
          isinf (a) || isnan (a))
        return FALSE;
      str = p;
      SKIP_WHITESPACES (str);
    }

  if (*str != ')')
    return FALSE;

  str++;

  SKIP_WHITESPACES (str);

  if (*str != '\0')
    return FALSE;

  if (rgba)
    {
      if (is_hsl)
        {
          GdkHSLA hsla;
          hsla.hue = r * 255;
          hsla.saturation = CLAMP (g, 0, 1);
          hsla.lightness = CLAMP (b, 0, 1);
          hsla.alpha = CLAMP (a, 0, 1);
          _gdk_rgba_init_from_hsla (rgba, &hsla);
        }
      else
        {
          rgba->red = CLAMP (r, 0, 1);
          rgba->green = CLAMP (g, 0, 1);
          rgba->blue = CLAMP (b, 0, 1);
          rgba->alpha = CLAMP (a, 0, 1);
        }
    }

  return TRUE;
}

#undef SKIP_WHITESPACES

/**
 * gdk_rgba_hash:
 * @p: (type GdkRGBA): a `GdkRGBA`
 *
 * A hash function suitable for using for a hash
 * table that stores `GdkRGBA`s.
 *
 * Returns: The hash value for @p
 */
guint
gdk_rgba_hash (gconstpointer p)
{
  const GdkRGBA *rgba = p;

  return ((guint) (rgba->red * 65535) +
          ((guint) (rgba->green * 65535) << 11) +
          ((guint) (rgba->blue * 65535) << 22) +
          ((guint) (rgba->alpha * 65535) >> 6));
}

/**
 * gdk_rgba_equal:
 * @p1: (type GdkRGBA): a `GdkRGBA`
 * @p2: (type GdkRGBA): another `GdkRGBA`
 *
 * Compares two `GdkRGBA` colors.
 *
 * Returns: %TRUE if the two colors compare equal
 */
gboolean
gdk_rgba_equal (gconstpointer p1,
                gconstpointer p2)
{
  const GdkRGBA *rgba1, *rgba2;

  rgba1 = p1;
  rgba2 = p2;

  if (rgba1->red == rgba2->red &&
      rgba1->green == rgba2->green &&
      rgba1->blue == rgba2->blue &&
      rgba1->alpha == rgba2->alpha)
    return TRUE;

  return FALSE;
}

/**
 * gdk_rgba_to_string:
 * @rgba: a `GdkRGBA`
 *
 * Returns a textual specification of @rgba in the form
 * `rgb(r,g,b)` or `rgba(r,g,b,a)`, where “r”, “g”, “b” and
 * “a” represent the red, green, blue and alpha values
 * respectively. “r”, “g”, and “b” are represented as integers
 * in the range 0 to 255, and “a” is represented as a floating
 * point value in the range 0 to 1.
 *
 * These string forms are string forms that are supported by
 * the CSS3 colors module, and can be parsed by [method@Gdk.RGBA.parse].
 *
 * Note that this string representation may lose some precision,
 * since “r”, “g” and “b” are represented as 8-bit integers. If
 * this is a concern, you should use a different representation.
 *
 * Returns: A newly allocated text string
 */
char *
gdk_rgba_to_string (const GdkRGBA *rgba)
{
  if (rgba->alpha > 0.999)
    {
      return g_strdup_printf ("rgb(%d,%d,%d)",
                              (int)(0.5 + CLAMP (rgba->red, 0., 1.) * 255.),
                              (int)(0.5 + CLAMP (rgba->green, 0., 1.) * 255.),
                              (int)(0.5 + CLAMP (rgba->blue, 0., 1.) * 255.));
    }
  else
    {
      char alpha[G_ASCII_DTOSTR_BUF_SIZE];

      g_ascii_formatd (alpha, G_ASCII_DTOSTR_BUF_SIZE, "%g", CLAMP (rgba->alpha, 0, 1));

      return g_strdup_printf ("rgba(%d,%d,%d,%s)",
                              (int)(0.5 + CLAMP (rgba->red, 0., 1.) * 255.),
                              (int)(0.5 + CLAMP (rgba->green, 0., 1.) * 255.),
                              (int)(0.5 + CLAMP (rgba->blue, 0., 1.) * 255.),
                              alpha);
    }
}

static gboolean
parse_color_channel_value (GtkCssParser *parser,
                           float        *value,
                           gboolean      is_percentage)
{
  double dvalue;

  if (is_percentage)
    {
      if (!gtk_css_parser_consume_percentage (parser, &dvalue))
        return FALSE;

      *value = CLAMP (dvalue, 0.0, 100.0) / 100.0;
      return TRUE;
    }
  else
    {
      if (!gtk_css_parser_consume_number (parser, &dvalue))
        return FALSE;

      *value = CLAMP (dvalue, 0.0, 255.0) / 255.0;
      return TRUE;
    }
}

static guint
parse_color_channel (GtkCssParser *parser,
                     guint         arg,
                     gpointer      data)
{
  GdkRGBA *rgba = data;
  double dvalue;

  switch (arg)
  {
    case 0:
      /* We abuse rgba->alpha to store if we use percentages or numbers */
      if (gtk_css_token_is (gtk_css_parser_get_token (parser), GTK_CSS_TOKEN_PERCENTAGE))
        rgba->alpha = 1.0;
      else
        rgba->alpha = 0.0;

      if (!parse_color_channel_value (parser, &rgba->red, rgba->alpha != 0.0))
        return 0;
      return 1;

    case 1:
      if (!parse_color_channel_value (parser, &rgba->green, rgba->alpha != 0.0))
        return 0;
      return 1;

    case 2:
      if (!parse_color_channel_value (parser, &rgba->blue, rgba->alpha != 0.0))
        return 0;
      return 1;

    case 3:
      if (!gtk_css_parser_consume_number (parser, &dvalue))
        return 0;

      rgba->alpha = CLAMP (dvalue, 0.0, 1.0);
      return 1;

    default:
      g_assert_not_reached ();
      return 0;
  }
}

static guint
parse_hsla_color_channel (GtkCssParser *parser,
                          guint         arg,
                          gpointer      data)
{
  GdkHSLA *hsla = data;
  double dvalue;

  switch (arg)
  {
    case 0:
      if (!gtk_css_parser_consume_number (parser, &dvalue))
        return 0;
      hsla->hue = dvalue;
      return 1;

    case 1:
      if (!gtk_css_parser_consume_percentage (parser, &dvalue))
        return 0;
      hsla->saturation = CLAMP (dvalue, 0.0, 100.0) / 100.0;
      return 1;

    case 2:
      if (!gtk_css_parser_consume_percentage (parser, &dvalue))
        return 0;
      hsla->lightness = CLAMP (dvalue, 0.0, 100.0) / 100.0;
      return 1;

    case 3:
      if (!gtk_css_parser_consume_number (parser, &dvalue))
        return 0;

      hsla->alpha = CLAMP (dvalue, 0.0, 1.0) / 1.0;
      return 1;

    default:
      g_assert_not_reached ();
      return 0;
  }
}

static gboolean
rgba_init_chars (GdkRGBA    *rgba,
                 const char  s[8])
{
  guint i;

  for (i = 0; i < 8; i++)
    {
      if (!g_ascii_isxdigit (s[i]))
        return FALSE;
    }

  rgba->red =   (g_ascii_xdigit_value (s[0]) * 16 + g_ascii_xdigit_value (s[1])) / 255.0;
  rgba->green = (g_ascii_xdigit_value (s[2]) * 16 + g_ascii_xdigit_value (s[3])) / 255.0;
  rgba->blue =  (g_ascii_xdigit_value (s[4]) * 16 + g_ascii_xdigit_value (s[5])) / 255.0;
  rgba->alpha = (g_ascii_xdigit_value (s[6]) * 16 + g_ascii_xdigit_value (s[7])) / 255.0;

  return TRUE;
}

gboolean
gdk_rgba_parser_parse (GtkCssParser *parser,
                       GdkRGBA      *rgba)
{
  const GtkCssToken *token;

  token = gtk_css_parser_get_token (parser);
  if (gtk_css_token_is_function (token, "rgb"))
    {
      if (!gtk_css_parser_consume_function (parser, 3, 3, parse_color_channel, rgba))
        return FALSE;

      rgba->alpha = 1.0;
      return TRUE;
    }
  else if (gtk_css_token_is_function (token, "rgba"))
    {
      return gtk_css_parser_consume_function (parser, 4, 4, parse_color_channel, rgba);
    }
  else if (gtk_css_token_is_function (token, "hsl") || gtk_css_token_is_function (token, "hsla"))
    {
      GdkHSLA hsla;

      hsla.alpha = 1.0;

      if (!gtk_css_parser_consume_function (parser, 3, 4, parse_hsla_color_channel, &hsla))
        return FALSE;

      _gdk_rgba_init_from_hsla (rgba, &hsla);
      return TRUE;
    }
  else if (gtk_css_token_is (token, GTK_CSS_TOKEN_HASH_ID) ||
           gtk_css_token_is (token, GTK_CSS_TOKEN_HASH_UNRESTRICTED))
    {
      const char *s = gtk_css_token_get_string (token);

      switch (strlen (s))
        {
          case 3:
            if (!rgba_init_chars (rgba, (char[8]) {s[0], s[0], s[1], s[1], s[2], s[2], 'F', 'F' }))
              {
                gtk_css_parser_error_value (parser, "Hash code is not a valid hex color.");
                return FALSE;
              }
            break;

          case 4:
            if (!rgba_init_chars (rgba, (char[8]) {s[0], s[0], s[1], s[1], s[2], s[2], s[3], s[3] }))
              {
                gtk_css_parser_error_value (parser, "Hash code is not a valid hex color.");
                return FALSE;
              }
            break;

          case 6:
            if (!rgba_init_chars (rgba, (char[8]) {s[0], s[1], s[2], s[3], s[4], s[5], 'F', 'F' }))
              {
                gtk_css_parser_error_value (parser, "Hash code is not a valid hex color.");
                return FALSE;
              }
            break;

          case 8:
            if (!rgba_init_chars (rgba, s))
              {
                gtk_css_parser_error_value (parser, "Hash code is not a valid hex color.");
                return FALSE;
              }
            break;

          default:
            gtk_css_parser_error_value (parser, "Hash code is not a valid hex color.");
            return FALSE;
            break;
        }

      gtk_css_parser_consume_token (parser);
      return TRUE;
    }
  else if (gtk_css_token_is (token, GTK_CSS_TOKEN_IDENT))
    {
      if (gtk_css_token_is_ident (token, "transparent"))
        {
          *rgba = GDK_RGBA_TRANSPARENT;
        }
      else if (gdk_rgba_parse (rgba, gtk_css_token_get_string (token)))
        {
          /* everything's fine */
        }
      else
        {
          gtk_css_parser_error_syntax (parser, "\"%s\" is not a valid color name.", gtk_css_token_get_string (token));
          return FALSE;
        }

      gtk_css_parser_consume_token (parser);
      return TRUE;
    }
  else
    {
      gtk_css_parser_error_syntax (parser, "Expected a valid color.");
      return FALSE;
    }
}
