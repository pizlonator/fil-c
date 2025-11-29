/* Color utilities
 *
 * Copyright (C) 1999 The Free Software Foundation
 *
 * Authors: Simon Budig <Simon.Budig@unix-ag.org> (original code)
 *          Federico Mena-Quintero <federico@gimp.org> (cleanup for GTK+)
 *          Jonathan Blandford <jrb@redhat.com> (cleanup for GTK+)
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

#include <math.h>
#include <string.h>

#include "gtkcolorutils.h"

/* Converts from RGB to HSV */
static void
rgb_to_hsv (float *r,
            float *g,
            float *b)
{
  float red, green, blue;
  float h, s, v;
  float min, max;
  float delta;

  red = *r;
  green = *g;
  blue = *b;

  h = 0.0;

  if (red > green)
    {
      if (red > blue)
        max = red;
      else
        max = blue;

      if (green < blue)
        min = green;
      else
        min = blue;
    }
  else
    {
      if (green > blue)
        max = green;
      else
        max = blue;

      if (red < blue)
        min = red;
      else
        min = blue;
    }

  v = max;

  if (max != 0.0)
    s = (max - min) / max;
  else
    s = 0.0;

  if (s == 0.0)
    h = 0.0;
  else
    {
      delta = max - min;

      if (red == max)
        h = (green - blue) / delta;
      else if (green == max)
        h = 2 + (blue - red) / delta;
      else if (blue == max)
        h = 4 + (red - green) / delta;

      h /= 6.0;

      if (h < 0.0)
        h += 1.0;
      else if (h > 1.0)
        h -= 1.0;
    }

  *r = h;
  *g = s;
  *b = v;
}

/**
 * gtk_hsv_to_rgb:
 * @h: Hue
 * @s: Saturation
 * @v: Value
 * @r: (out): Return value for the red component
 * @g: (out): Return value for the green component
 * @b: (out): Return value for the blue component
 *
 * Converts a color from HSV space to RGB.
 *
 * Input values must be in the [0.0, 1.0] range;
 * output values will be in the same range.
 */
void
gtk_hsv_to_rgb (float  h, float  s, float  v,
                float *r, float *g, float *b)
{
  float hue;
  float f, p;
  int ihue;

  g_return_if_fail (h >= 0.0 && h <= 1.0);
  g_return_if_fail (s >= 0.0 && s <= 1.0);
  g_return_if_fail (v >= 0.0 && v <= 1.0);
  g_return_if_fail (r);
  g_return_if_fail (g);
  g_return_if_fail (b);

  if (s == 0.0)
    {
      *r = v;
      *g = v;
      *b = v;
      return;
    }

  hue = h * 6.0;

  if (hue == 6.0)
    hue = 0.0;

  ihue = (int)hue;
  f = hue - ihue;
  p = v * (1.0 - s);

  if (ihue == 0)
    {
      *r = v;
      *g = v * (1.0 - s * (1.0 - f));
      *b = p;
    }
  else if (ihue == 1)
    {
      *r = v * (1.0 - s * f);
      *g = v;
      *b = p;
    }
  else if (ihue == 2)
    {
      *r = p;
      *g = v;
      *b = v * (1.0 - s * (1.0 - f));
    }
  else if (ihue == 3)
    {
      *r = p;
      *g = v * (1.0 - s * f);
      *b = v;
    }
  else if (ihue == 4)
    {
      *r = v * (1.0 - s * (1.0 - f));
      *g = p;
      *b = v;
    }
  else if (ihue == 5)
    {
      *r = v;
      *g = p;
      *b = v * (1.0 - s * f);
    }
}

/**
 * gtk_rgb_to_hsv:
 * @r: Red
 * @g: Green
 * @b: Blue
 * @h: (out): Return value for the hue component
 * @s: (out): Return value for the saturation component
 * @v: (out): Return value for the value component
 *
 * Converts a color from RGB space to HSV.
 *
 * Input values must be in the [0.0, 1.0] range;
 * output values will be in the same range.
 */
void
gtk_rgb_to_hsv (float  r, float  g, float  b,
                float *h, float *s, float *v)
{
  g_return_if_fail (r >= 0.0 && r <= 1.0);
  g_return_if_fail (g >= 0.0 && g <= 1.0);
  g_return_if_fail (b >= 0.0 && b <= 1.0);

  rgb_to_hsv (&r, &g, &b);

  if (h)
    *h = r;

  if (s)
    *s = g;

  if (v)
    *v = b;
}
