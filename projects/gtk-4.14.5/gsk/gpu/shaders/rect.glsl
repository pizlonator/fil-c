#ifndef _RECT_
#define _RECT_

struct Rect
{
  /* x,y and y,w make up the 2 points of this rect,
     note that this is not containing width or height */
  vec4 bounds;
};

Rect
rect_new_size (vec4 coords)
{
  return Rect (coords + vec4 (0.0, 0.0, coords.xy));
}

Rect
rect_from_gsk (vec4 coords)
{
  Rect result = rect_new_size (coords);
  result.bounds *= GSK_GLOBAL_SCALE.xyxy;
  return result;
}

float
rect_distance (Rect r, vec2 p)
{
  vec4 distance = (r.bounds - p.xyxy) * vec4(1.0, 1.0, -1.0, -1.0);
  vec2 max2 = max (distance.xy, distance.zw);
  return length (max (max2, 0.0)) + min (max(max2.x, max2.y), 0.0);
}

vec2
rect_size (Rect r)
{
  return r.bounds.zw - r.bounds.xy;
}

Rect
rect_round_larger (Rect r)
{
  return Rect (vec4 (floor(r.bounds.xy), ceil (r.bounds.zw)));
}

Rect
rect_round_larger_smaller (Rect r)
{
  return Rect (mix (floor(r.bounds), ceil (r.bounds), bvec4(0, 1, 1, 0)));
}

Rect
rect_round_smaller_larger (Rect r)
{
  return Rect (mix (floor(r.bounds), ceil (r.bounds), bvec4(1, 0, 0, 1)));
}

Rect
rect_intersect (Rect a, Rect b)
{
  vec4 result = vec4(max(a.bounds.xy, b.bounds.xy), min(a.bounds.zw, b.bounds.zw));
  if (any (greaterThanEqual (result.xy, result.zw)))
    return Rect (vec4(0.0));
  return Rect(result);
}

Rect
rect_union (Rect a, Rect b)
{
  return Rect (vec4 (min (a.bounds.xy, b.bounds.xy), max (a.bounds.zw, b.bounds.zw)));
}

vec2
rect_get_coord (Rect r, vec2 pt)
{
  return (pt - r.bounds.xy) / rect_size (r);
}

#ifdef GSK_FRAGMENT_SHADER

float
rect_coverage (Rect r,
               vec2 p,
               vec2 dFdp)
{
  Rect prect = Rect(vec4(p - 0.5 * dFdp, p + 0.5 * dFdp));
  Rect coverect = rect_intersect (r, prect);
  vec2 coverage = rect_size(coverect) / dFdp;
  return coverage.x * coverage.y;
}

float
rect_coverage (Rect r, vec2 p)
{
  return rect_coverage (r, p, abs (fwidth (p)));
}


#endif

#endif
