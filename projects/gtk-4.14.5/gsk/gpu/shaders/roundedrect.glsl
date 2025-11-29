#ifndef _ROUNDED_RECT_
#define _ROUNDED_RECT_

#include "ellipse.glsl"
#include "rect.glsl"

struct RoundedRect
{
  vec4 bounds;
  vec4 corner_widths;
  vec4 corner_heights;
};

RoundedRect
rounded_rect_from_rect (Rect r)
{
  return RoundedRect (r.bounds, vec4 (0.0), vec4 (0.0));
}

RoundedRect
rounded_rect_from_gsk (mat3x4 gsk_rounded_rect)
{
  return RoundedRect ((gsk_rounded_rect[0].xyxy + vec4 (0.0, 0.0, gsk_rounded_rect[0].zw)) * GSK_GLOBAL_SCALE.xyxy,
                      gsk_rounded_rect[1] * GSK_GLOBAL_SCALE.xxxx,
                      gsk_rounded_rect[2] * GSK_GLOBAL_SCALE.yyyy);
}

float
rounded_rect_distance (RoundedRect r, vec2 p)
{
  Rect bounds = Rect(vec4(r.bounds));

  float bounds_distance = rect_distance (bounds, p);
                      
  Ellipse tl = Ellipse (r.bounds.xy + vec2( r.corner_widths.x,  r.corner_heights.x),
                        vec2(r.corner_widths.x, r.corner_heights.x));
  Ellipse tr = Ellipse (r.bounds.zy + vec2(-r.corner_widths.y,  r.corner_heights.y),
                        vec2(r.corner_widths.y, r.corner_heights.y));
  Ellipse br = Ellipse (r.bounds.zw + vec2(-r.corner_widths.z, -r.corner_heights.z),
                        vec2(r.corner_widths.z, r.corner_heights.z));
  Ellipse bl = Ellipse (r.bounds.xw + vec2( r.corner_widths.w, -r.corner_heights.w),
                        vec2(r.corner_widths.w, r.corner_heights.w));

 vec4 distances = vec4(ellipse_distance (tl, p),
                       ellipse_distance (tr, p),
                       ellipse_distance (br, p),
                       ellipse_distance (bl, p));

  bvec4 is_out = bvec4(p.x < tl.center.x && p.y < tl.center.y,
                       p.x > tr.center.x && p.y < tr.center.y,
                       p.x > br.center.x && p.y > br.center.y,
                       p.x < bl.center.x && p.y > bl.center.y);
  distances = mix (vec4(bounds_distance), distances, is_out);

  vec2 max2 = max (distances.xy, distances.zw);
  return max (max2.x, max2.y);
}

RoundedRect
rounded_rect_shrink (RoundedRect r, vec4 amount)
{
  vec4 new_bounds = r.bounds + vec4(1.0,1.0,-1.0,-1.0) * amount.wxyz;
  vec4 new_widths = max (r.corner_widths - sign (r.corner_widths) * amount.wyyw, 0.0);
  vec4 new_heights = max (r.corner_heights - sign (r.corner_heights) * amount.xxzz, 0.0);
  new_widths = min (new_widths, new_bounds.z - new_bounds.x);
  new_heights = min (new_heights, new_bounds.w - new_bounds.y);
              
  return RoundedRect (new_bounds, new_widths, new_heights);
}

void
rounded_rect_scale (inout RoundedRect r,
                    vec2              scale)
{
  r.bounds *= scale.xyxy;
  r.corner_widths *= scale.xxxx;
  r.corner_heights *= scale.yyyy;
}

void
rounded_rect_offset (inout RoundedRect r,
                     vec2              offset)
{
  r.bounds += offset.xyxy;
}

bool
rounded_rect_is_slicable (RoundedRect r)
{
  vec2 size = rect_size (Rect (r.bounds));
  return (r.corner_widths[TOP_LEFT] + r.corner_widths[BOTTOM_RIGHT] <= size.x ||
          r.corner_heights[TOP_LEFT] + r.corner_heights[BOTTOM_RIGHT] <= size.y)
      && (r.corner_widths[BOTTOM_LEFT] + r.corner_widths[TOP_RIGHT] <= size.x ||
          r.corner_heights[BOTTOM_LEFT] + r.corner_heights[TOP_RIGHT] <= size.y);
}

Rect
rounded_rect_intersection_fallback_slice (RoundedRect outside,
                                          RoundedRect inside,
                                          uint        slice)
{
  switch (slice)
    {
    default:
    case SLICE_TOP:
    case SLICE_RIGHT:
    case SLICE_BOTTOM:
    case SLICE_LEFT:
      return Rect (vec4 (0.0));

    case SLICE_TOP_LEFT:
      return Rect (vec4 (outside.bounds.xy, 0.5 * (outside.bounds.xy + outside.bounds.zw)));

    case SLICE_TOP_RIGHT:
      return Rect (vec4 (0.5 * (outside.bounds.x + outside.bounds.z), outside.bounds.y,
                         outside.bounds.z, 0.5 * (outside.bounds.y + outside.bounds.w)));

    case SLICE_BOTTOM_RIGHT:
      return Rect (vec4 (0.5 * (outside.bounds.xy + outside.bounds.zw), outside.bounds.zw));

    case SLICE_BOTTOM_LEFT:
      return Rect (vec4 (outside.bounds.x, 0.5 * (outside.bounds.y + outside.bounds.w),
                         0.5 * (outside.bounds.x + outside.bounds.z), outside.bounds.w));
    }
}

Rect
rounded_rect_intersection_slice (RoundedRect outside,
                                 RoundedRect inside,
                                 uint        slice)
{
  float left, right, top, bottom;

  if (!rounded_rect_is_slicable (outside) ||
      !rounded_rect_is_slicable (inside))
    return rounded_rect_intersection_fallback_slice (outside, inside, slice);

  switch (slice)
    {
    case SLICE_TOP_LEFT:
      return Rect (vec4 (outside.bounds.x,
                         outside.bounds.y,
                         max (outside.bounds.x + outside.corner_widths[TOP_LEFT],
                              inside.bounds.x + inside.corner_widths[TOP_LEFT]),
                         max (outside.bounds.y + outside.corner_heights[TOP_LEFT],
                              inside.bounds.y + inside.corner_heights[TOP_LEFT])));

    case SLICE_TOP:
      left = max (outside.bounds.x + outside.corner_widths[TOP_LEFT],
                  inside.bounds.x + inside.corner_widths[TOP_LEFT]);
      right = min (outside.bounds.z - outside.corner_widths[TOP_RIGHT],
                   inside.bounds.z - inside.corner_widths[TOP_RIGHT]);
      return Rect (vec4 (left,
                         outside.bounds.y,
                         max (left, right),
                         max (outside.bounds.y, inside.bounds.y)));

    case SLICE_TOP_RIGHT:
      left = max (min (outside.bounds.z - outside.corner_widths[TOP_RIGHT],
                       inside.bounds.z - inside.corner_widths[TOP_RIGHT]),
                  max (outside.bounds.x + outside.corner_widths[TOP_LEFT],
                       inside.bounds.x + inside.corner_widths[TOP_LEFT]));
      return Rect (vec4 (left,
                         outside.bounds.y,
                         outside.bounds.z,
                         max (outside.bounds.y + outside.corner_heights[TOP_RIGHT],
                              inside.bounds.y + inside.corner_heights[TOP_RIGHT])));

    case SLICE_RIGHT:
      top = max (outside.bounds.y + outside.corner_heights[TOP_RIGHT],
                 inside.bounds.y + inside.corner_heights[TOP_RIGHT]);
      bottom = min (outside.bounds.w - outside.corner_heights[BOTTOM_RIGHT],
                    inside.bounds.w - inside.corner_heights[BOTTOM_RIGHT]);
      return Rect (vec4 (min (outside.bounds.z, inside.bounds.z),
                         top,
                         outside.bounds.z,
                         max (bottom, top)));

    case SLICE_BOTTOM_RIGHT:
      left = max (min (outside.bounds.z - outside.corner_widths[BOTTOM_RIGHT],
                       inside.bounds.z - inside.corner_widths[BOTTOM_RIGHT]),
                  max (outside.bounds.x + outside.corner_widths[BOTTOM_LEFT],
                       inside.bounds.x + inside.corner_widths[BOTTOM_LEFT]));
      bottom = max (min (outside.bounds.w - outside.corner_heights[BOTTOM_RIGHT],
                         inside.bounds.w - inside.corner_heights[BOTTOM_RIGHT]),
                    max (outside.bounds.y + outside.corner_heights[TOP_RIGHT],
                         inside.bounds.y + inside.corner_heights[TOP_RIGHT]));
      return Rect (vec4 (left,
                         bottom,
                         outside.bounds.z,
                         outside.bounds.w));

    case SLICE_BOTTOM:
      left = max (outside.bounds.x + outside.corner_widths[BOTTOM_LEFT],
                  inside.bounds.x + inside.corner_widths[BOTTOM_LEFT]);
      right = min (outside.bounds.z - outside.corner_widths[BOTTOM_RIGHT],
                   inside.bounds.z - inside.corner_widths[BOTTOM_RIGHT]);
      return Rect (vec4 (left,
                         min (outside.bounds.w, inside.bounds.w),
                         max (left, right),
                         outside.bounds.w));

    case SLICE_BOTTOM_LEFT:
      bottom = max (min (outside.bounds.w - outside.corner_heights[BOTTOM_LEFT],
                         inside.bounds.w - inside.corner_heights[BOTTOM_LEFT]),
                    max (outside.bounds.y + outside.corner_heights[TOP_LEFT],
                         inside.bounds.y + inside.corner_heights[TOP_LEFT]));
      return Rect (vec4 (outside.bounds.x,
                         bottom,
                         max (outside.bounds.x + outside.corner_widths[BOTTOM_LEFT],
                              inside.bounds.x + inside.corner_widths[BOTTOM_LEFT]),
                         outside.bounds.w));

    case SLICE_LEFT:
      top = max (outside.bounds.y + outside.corner_heights[TOP_LEFT],
                 inside.bounds.y + inside.corner_heights[TOP_LEFT]);
      bottom = min (outside.bounds.w - outside.corner_heights[BOTTOM_LEFT],
                    inside.bounds.w - inside.corner_heights[BOTTOM_LEFT]);
      return Rect (vec4 (outside.bounds.x,
                         top,
                         max (outside.bounds.x, inside.bounds.x),
                         max (bottom, top)));

    default:
      return Rect (vec4 (0.0));
    }
}

#ifdef GSK_FRAGMENT_SHADER

float
rounded_rect_coverage (RoundedRect r, vec2 p)
{
  vec2 fw = abs (fwidth (p));
  float distance_scale = max (fw.x, fw.y);
  float distance = rounded_rect_distance (r, p) / distance_scale;
  float coverage = 0.5 - distance;

  return clamp (coverage, 0.0, 1.0);
}

#endif

#endif
