#include "common.glsl"

PASS(0) vec2 _pos;
PASS_FLAT(1) vec4 _color;
PASS_FLAT(2) RoundedRect _outline;

#ifdef GSK_VERTEX_SHADER

IN(0) mat3x4 in_outline;
IN(3) vec4 in_color;

void
run (out vec2 pos)
{
  RoundedRect outline = rounded_rect_from_gsk (in_outline);
  
  pos = rect_get_position (Rect (outline.bounds));

  _pos = pos;
  _outline= outline;
  _color = color_premultiply (in_color);
}

#endif

#ifdef GSK_FRAGMENT_SHADER

void
run (out vec4 color,
     out vec2 position)
{
  color = _color * rounded_rect_coverage (_outline, _pos);
  position = _pos;
}

#endif
