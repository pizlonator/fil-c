/* GTK - The GIMP Toolkit
 * Copyright (C) 2012 Benjamin Otte <otte@gnome.org>
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

#include "gdkhslaprivate.h"

#include <math.h>

void
_gdk_hsla_init_from_rgba (GdkHSLA       *hsla,
                          const GdkRGBA *rgba)
{
  float min;
  float max;
  float red;
  float green;
  float blue;
  float delta;

  g_return_if_fail (hsla != NULL);
  g_return_if_fail (rgba != NULL);

  red = rgba->red;
  green = rgba->green;
  blue = rgba->blue;

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

  hsla->lightness = (max + min) / 2;
  hsla->saturation = 0;
  hsla->hue = 0;
  hsla->alpha = rgba->alpha;

  if (max != min)
    {
      if (hsla->lightness <= 0.5)
        hsla->saturation = (max - min) / (max + min);
      else
        hsla->saturation = (max - min) / (2 - max - min);

      delta = max -min;
      if (red == max)
        hsla->hue = (green - blue) / delta;
      else if (green == max)
        hsla->hue = 2 + (blue - red) / delta;
      else if (blue == max)
        hsla->hue = 4 + (red - green) / delta;

      hsla->hue *= 60;
      if (hsla->hue < 0.0)
        hsla->hue += 360;
    }
}

void
_gdk_rgba_init_from_hsla (GdkRGBA       *rgba,
                          const GdkHSLA *hsla)
{
  float hue;
  float lightness;
  float saturation;
  float m1, m2;

  lightness = hsla->lightness;
  saturation = hsla->saturation;

  if (lightness <= 0.5)
    m2 = lightness * (1 + saturation);
  else
    m2 = lightness + saturation - lightness * saturation;
  m1 = 2 * lightness - m2;

  rgba->alpha = hsla->alpha;

  if (saturation == 0)
    {
      rgba->red = lightness;
      rgba->green = lightness;
      rgba->blue = lightness;
    }
  else
    {
      hue = hsla->hue + 120;
      while (hue > 360)
        hue -= 360;
      while (hue < 0)
        hue += 360;

      if (hue < 60)
        rgba->red = m1 + (m2 - m1) * hue / 60;
      else if (hue < 180)
        rgba->red = m2;
      else if (hue < 240)
        rgba->red = m1 + (m2 - m1) * (240 - hue) / 60;
      else
        rgba->red = m1;

      hue = hsla->hue;
      while (hue > 360)
        hue -= 360;
      while (hue < 0)
        hue += 360;

      if (hue < 60)
        rgba->green = m1 + (m2 - m1) * hue / 60;
      else if (hue < 180)
        rgba->green = m2;
      else if (hue < 240)
        rgba->green = m1 + (m2 - m1) * (240 - hue) / 60;
      else
        rgba->green = m1;

      hue = hsla->hue - 120;
      while (hue > 360)
        hue -= 360;
      while (hue < 0)
        hue += 360;

      if (hue < 60)
        rgba->blue = m1 + (m2 - m1) * hue / 60;
      else if (hue < 180)
        rgba->blue = m2;
      else if (hue < 240)
        rgba->blue = m1 + (m2 - m1) * (240 - hue) / 60;
      else
        rgba->blue = m1;
    }
}

void
_gdk_hsla_shade (GdkHSLA       *dest,
                 const GdkHSLA *src,
                 float          factor)
{
  g_return_if_fail (dest != NULL);
  g_return_if_fail (src != NULL);

  dest->hue = src->hue;

  dest->lightness = src->lightness * factor;
  dest->lightness = CLAMP (dest->lightness, 0.0, 1.0);

  dest->saturation = src->saturation * factor;
  dest->saturation = CLAMP (dest->saturation, 0.0, 1.0);

  dest->alpha = src->alpha;
}
