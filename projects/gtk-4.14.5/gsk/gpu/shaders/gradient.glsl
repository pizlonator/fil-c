#ifndef _GRADIENT_
#define _GRADIENT_

#ifdef GSK_FRAGMENT_SHADER

#include "common.glsl"

struct Gradient
{
  int n_stops;
  uint offset;
};

float
gradient_read_offset (Gradient self,
                      int      i)
{
  return gsk_get_float (self.offset + uint(i) * 5u);
}

vec4
gradient_read_color (Gradient self,
                     int      i)
{
  uint u = uint (clamp (i, 0, self.n_stops - 1));
  return vec4 (gsk_get_float (self.offset + u * 5u + 1u),
               gsk_get_float (self.offset + u * 5u + 2u),
               gsk_get_float (self.offset + u * 5u + 3u),
               gsk_get_float (self.offset + u * 5u + 4u));
}

vec4
gradient_get_color_for_range_unscaled (Gradient self,
                                       float    start,
                                       float    end)
{
  vec4 result = vec4 (0.0);
  float offset;
  int i;

  for (i = 0; i < self.n_stops; i++)
    {
      offset = gradient_read_offset (self, i);
      if (offset >= start)
        break;
    }
  if (i == self.n_stops)
    offset = 1.0;

  float last_offset = i > 0 ? gradient_read_offset (self, i - 1) : 0.0;
  vec4 last_color = gradient_read_color (self, i - 1);
  vec4 color = gradient_read_color (self, i);
  if (last_offset < start)
    {
      last_color = mix (last_color, color, (start - last_offset) / (offset - last_offset));
      last_offset = start;
    }
  if (end <= start)
    return last_color;

  for (; i < self.n_stops; i++)
    {
      offset = gradient_read_offset (self, i);
      color = gradient_read_color (self, i);
      if (offset >= end)
        break;
      result += 0.5 * (color + last_color) * (offset - last_offset);
      last_offset = offset;
      last_color = color;
    }
  if (i == self.n_stops)
    {
      offset = 1.0;
      color = gradient_read_color (self, i);
    }
  if (offset > end)
    {
      color = mix (last_color, color, (end - last_offset) / (offset - last_offset));
      offset = end;
    }
  result += 0.5 * (color + last_color) * (offset - last_offset);

  return result;
}

vec4
gradient_get_color_repeating (Gradient self,
                              float    start,
                              float    end)
{
  vec4 c;

  if (floor (end) > floor (start))
    {
      float fract_end = fract(end);
      float fract_start = fract(start);
      float n = floor (end) - floor (start);
      if (fract_end > fract_start + 0.01)
        c = gradient_get_color_for_range_unscaled (self, fract_start, fract_end);
      else if (fract_start > fract_end + 0.01)
        c = - gradient_get_color_for_range_unscaled (self, fract_end, fract_start);
      c += gradient_get_color_for_range_unscaled (self, 0.0, 1.0) * n;
    }
  else
    {
      start = fract (start);
      end = fract (end);
      c = gradient_get_color_for_range_unscaled (self, start, end);
    }
  c /= end - start;

  return color_premultiply (c);
}

vec4
gradient_get_color (Gradient self,
                    float    start,
                    float    end)
{
  vec4 c;

  start = clamp (start, 0.0, 1.0);
  end = clamp (end, 0.0, 1.0);
  c = gradient_get_color_for_range_unscaled (self, start, end);
  if (end > start)
    c /= end - start;

  return color_premultiply (c);
}

uint
gradient_get_size (Gradient self)
{
  return uint (self.n_stops) * 5u + 1u;
}

Gradient
gradient_new (uint offset)
{
  Gradient self = Gradient (gsk_get_int (offset),
                            offset + 1u);

  return self;
}

#endif /* GSK_FRAGMENT_SHADER */
#endif /* __GRADIENT__ */
