#ifndef _BLENDMODE_
#define _BLENDMODE_

vec4
composite (vec4 Cs, vec4 Cb, vec3 B)
{
  float ao = Cs.a + Cb.a * (1.0 - Cs.a);
  vec3 Co = (Cs.a*(1.0 - Cb.a)*Cs.rgb + Cs.a*Cb.a*B + (1.0 - Cs.a)*Cb.a*Cb.rgb) / ao;
  return vec4(Co, ao);
}

vec4
normal (vec4 Cs, vec4 Cb)
{
  return composite (Cs, Cb, Cs.rgb);
}

vec4
multiply (vec4 Cs, vec4 Cb)
{
  return composite (Cs, Cb, Cs.rgb * Cb.rgb);
}

vec4
difference (vec4 Cs, vec4 Cb)
{
  return composite (Cs, Cb, abs(Cs.rgb - Cb.rgb));
}

vec4
screen (vec4 Cs, vec4 Cb)
{
  return composite (Cs, Cb, Cs.rgb + Cb.rgb - Cs.rgb * Cb.rgb);
}

float
hard_light (float source, float backdrop)
{
  if (source <= 0.5)
    return 2.0 * backdrop * source;
  else
    return 2.0 * (backdrop + source - backdrop * source) - 1.0;
}

vec4
hard_light (vec4 Cs, vec4 Cb)
{
  vec3 B = vec3 (hard_light (Cs.r, Cb.r),
                 hard_light (Cs.g, Cb.g),
                 hard_light (Cs.b, Cb.b));
  return composite (Cs, Cb, B);
}

float
soft_light (float source, float backdrop)
{
  float db;

  if (backdrop <= 0.25)
    db = ((16.0 * backdrop - 12.0) * backdrop + 4.0) * backdrop;
  else
    db = sqrt (backdrop);

  if (source <= 0.5)
    return backdrop - (1.0 - 2.0 * source) * backdrop * (1.0 - backdrop);
  else
    return backdrop + (2.0 * source - 1.0) * (db - backdrop);
}

vec4
soft_light (vec4 Cs, vec4 Cb)
{
  vec3 B = vec3 (soft_light (Cs.r, Cb.r),
                 soft_light (Cs.g, Cb.g),
                 soft_light (Cs.b, Cb.b));
  return composite (Cs, Cb, B);
}

vec4
overlay (vec4 Cs, vec4 Cb)
{
  vec3 B = vec3 (hard_light (Cb.r, Cs.r),
                 hard_light (Cb.g, Cs.g),
                 hard_light (Cb.b, Cs.b));
  return composite (Cs, Cb, B);
}

vec4
darken (vec4 Cs, vec4 Cb)
{
  vec3 B = min (Cs.rgb, Cb.rgb);
  return composite (Cs, Cb, B);
}

vec4
lighten (vec4 Cs, vec4 Cb)
{
  vec3 B = max (Cs.rgb, Cb.rgb);
  return composite (Cs, Cb, B);
}

float
color_dodge (float source, float backdrop)
{
  return (source == 1.0) ? source : min (backdrop / (1.0 - source), 1.0);
}

vec4
color_dodge (vec4 Cs, vec4 Cb)
{
  vec3 B = vec3 (color_dodge (Cs.r, Cb.r),
                 color_dodge (Cs.g, Cb.g),
                 color_dodge (Cs.b, Cb.b));
  return composite (Cs, Cb, B);
}


float
color_burn (float source, float backdrop)
{
  return (source == 0.0) ? source : max ((1.0 - ((1.0 - backdrop) / source)), 0.0);
}

vec4
color_burn (vec4 Cs, vec4 Cb)
{
  vec3 B = vec3 (color_burn (Cs.r, Cb.r),
                 color_burn (Cs.g, Cb.g),
                 color_burn (Cs.b, Cb.b));
  return composite (Cs, Cb, B);
}

vec4
exclusion (vec4 Cs, vec4 Cb)
{
  vec3 B = Cb.rgb + Cs.rgb - 2.0 * Cb.rgb * Cs.rgb;
  return composite (Cs, Cb, B);
}

float
lum (vec3 c)
{
  return 0.3 * c.r + 0.59 * c.g + 0.11 * c.b;
}

vec3
clip_color (vec3 c)
{
  float l = lum (c);
  float n = min (c.r, min (c.g, c.b));
  float x = max (c.r, max (c.g, c.b));
  if (n < 0.0) c = l + (((c - l) * l) / (l - n));
  if (x > 1.0) c = l + (((c - l) * (1.0 - l)) / (x - l));
  return c;
}

vec3
set_lum (vec3 c, float l)
{
  float d = l - lum (c);
  return clip_color (vec3 (c.r + d, c.g + d, c.b + d));
}

float
sat (vec3 c)
{
  return max (c.r, max (c.g, c.b)) - min (c.r, min (c.g, c.b));
}

vec3
set_sat (vec3 c, float s)
{
  float cmin = min (c.r, min (c.g, c.b));
  float cmax = max (c.r, max (c.g, c.b));
  vec3 res;

  if (cmax == cmin)
    res = vec3 (0, 0, 0);
  else
    {
      if (c.r == cmax)
        {
          if (c.g == cmin)
            {
              res.b = ((c.b - cmin) * s) / (cmax - cmin);
              res.g = 0.0;
            }
          else
            {
              res.g = ((c.g - cmin) * s) / (cmax - cmin);
              res.b = 0.0;
            }
          res.r = s;
        }
      else if (c.g == cmax)
        {
          if (c.r == cmin)
            {
              res.b = ((c.b - cmin) * s) / (cmax - cmin);
              res.r = 0.0;
            }
          else
            {
              res.r = ((c.r - cmin) * s) / (cmax - cmin);
              res.b = 0.0;
            }
          res.g = s;
        }
      else
        {
          if (c.r == cmin)
            {
              res.g = ((c.g - cmin) * s) / (cmax - cmin);
              res.r = 0.0;
            }
          else
            {
              res.r = ((c.r - cmin) * s) / (cmax - cmin);
              res.g = 0.0;
            }
          res.b = s;
        }
    }
  return res;
}

vec4
color (vec4 Cs, vec4 Cb)
{
  vec3 B = set_lum (Cs.rgb, lum (Cb.rgb));
  return composite (Cs, Cb, B);
}

vec4
hue (vec4 Cs, vec4 Cb)
{
  vec3 B = set_lum (set_sat (Cs.rgb, sat (Cb.rgb)), lum (Cb.rgb));
  return composite (Cs, Cb, B);
}

vec4
saturation (vec4 Cs, vec4 Cb)
{
  vec3 B = set_lum (set_sat (Cb.rgb, sat (Cs.rgb)), lum (Cb.rgb));
  return composite (Cs, Cb, B);
}

vec4
luminosity (vec4 Cs, vec4 Cb)
{
  vec3 B = set_lum (Cb.rgb, lum (Cs.rgb));
  return composite (Cs, Cb, B);
}

vec4
blend_mode (vec4 bottom_color,
            vec4 top_color,
            uint u_mode)
{
  vec4 result;

  if (u_mode == 0u)
    result = normal(top_color, bottom_color);
  else if (u_mode == 1u)
    result = multiply(top_color, bottom_color);
  else if (u_mode == 2u)
    result = screen(top_color, bottom_color);
  else if (u_mode == 3u)
    result = overlay(top_color, bottom_color);
  else if (u_mode == 4u)
    result = darken(top_color, bottom_color);
  else if (u_mode == 5u)
    result = lighten(top_color, bottom_color);
  else if (u_mode == 6u)
    result = color_dodge(top_color, bottom_color);
  else if (u_mode == 7u)
    result = color_burn(top_color, bottom_color);
  else if (u_mode == 8u)
    result = hard_light(top_color, bottom_color);
  else if (u_mode == 9u)
    result = soft_light(top_color, bottom_color);
  else if (u_mode == 10u)
    result = difference(top_color, bottom_color);
  else if (u_mode == 11u)
    result = exclusion(top_color, bottom_color);
  else if (u_mode == 12u)
    result = color(top_color, bottom_color);
  else if (u_mode == 13u)
    result = hue(top_color, bottom_color);
  else if (u_mode == 14u)
    result = saturation(top_color, bottom_color);
  else if (u_mode == 15u)
    result = luminosity(top_color, bottom_color);
  else
    result = vec4 (10.0, 0.0, 0.8, 1.0);

  return result;
}

#endif
