#include "common.glsl"

PASS(0) vec2 _pos;
PASS_FLAT(1) vec4 _color;
PASS_FLAT(2) RoundedRect _outside;
PASS_FLAT(5) RoundedRect _inside;



#ifdef GSK_VERTEX_SHADER

IN(0) mat4 in_border_colors;
IN(4) mat3x4 in_outline;
IN(7) vec4 in_border_widths;
IN(8) vec2 in_offset;

vec4
compute_color (void)
{
  uint triangle_index = uint (GSK_VERTEX_INDEX) / 3u;
  uint index, fallback;

  switch (triangle_index)
  {
    case 2u * SLICE_TOP_LEFT + 1u:
      index = TOP;
      fallback = LEFT;
      break;
    case 2u * SLICE_TOP:
    case 2u * SLICE_TOP + 1u:
      index = TOP;
      fallback = TOP;
      break;
    case 2u * SLICE_TOP_RIGHT:
      index = TOP;
      fallback = RIGHT;
      break;
    case 2u * SLICE_TOP_RIGHT + 1u:
      index = RIGHT;
      fallback = TOP;
      break;
    case 2u * SLICE_RIGHT:
    case 2u * SLICE_RIGHT + 1u:
      index = RIGHT;
      fallback = RIGHT;
      break;
    case 2u * SLICE_BOTTOM_RIGHT:
      index = RIGHT;
      fallback = BOTTOM;
      break;
    case 2u * SLICE_BOTTOM_RIGHT + 1u:
      index = BOTTOM;
      fallback = RIGHT;
      break;
    case 2u * SLICE_BOTTOM:
    case 2u * SLICE_BOTTOM + 1u:
      index = BOTTOM;
      fallback = BOTTOM;
      break;
    case 2u * SLICE_BOTTOM_LEFT:
      index = BOTTOM;
      fallback = LEFT;
      break;
    case 2u * SLICE_BOTTOM_LEFT + 1u:
      index = LEFT;
      fallback = BOTTOM;
      break;
    case 2u * SLICE_LEFT:
    case 2u * SLICE_LEFT + 1u:
      index = LEFT;
      fallback = LEFT;
      break;
    case 2u * SLICE_TOP_LEFT:
      index = LEFT;
      fallback = TOP;
      break;
  }
  
  if (in_border_widths[index] > 0.0)
    return color_premultiply (in_border_colors[index]);
  else
    return color_premultiply (in_border_colors[fallback]);
}

void
run (out vec2 pos)
{
  vec4 border_widths = in_border_widths * GSK_GLOBAL_SCALE.yxyx;
  RoundedRect outside = rounded_rect_from_gsk (in_outline);
  RoundedRect inside = rounded_rect_shrink (outside, border_widths);
  rounded_rect_offset (inside, in_offset * GSK_GLOBAL_SCALE);

  pos = border_get_position (outside, inside);

  _pos = pos;
  _color = compute_color ();
  _outside = outside;
  _inside = inside;
}

#endif

#ifdef GSK_FRAGMENT_SHADER

void
run (out vec4 color,
     out vec2 position)
{
  float alpha = clamp (rounded_rect_coverage (_outside, _pos) -
                       rounded_rect_coverage (_inside, _pos),
                       0.0, 1.0);

  position = _pos;
  color = _color * alpha;
}

#endif
