#include "common.glsl"

PASS(0) vec2 _pos;
PASS_FLAT(1) Rect _rect;
PASS_FLAT(2) vec4 _color;
PASS(3) vec2 _tex_coord;
PASS_FLAT(4) uint _tex_id;



#ifdef GSK_VERTEX_SHADER

IN(0) vec4 in_rect;
IN(1) vec4 in_color;
IN(2) vec4 in_tex_rect;
IN(3) uint in_tex_id;

void
run (out vec2 pos)
{
  Rect r = rect_from_gsk (in_rect);
  
  pos = rect_get_position (r);

  _pos = pos;
  _rect = r;
  _color = color_premultiply (in_color);
  _tex_coord = rect_get_coord (rect_from_gsk (in_tex_rect), pos);
  _tex_id = in_tex_id;
}

#endif



#ifdef GSK_FRAGMENT_SHADER

void
run (out vec4 color,
     out vec2 position)
{
  float alpha = gsk_texture (_tex_id, _tex_coord).a * rect_coverage (_rect, _pos);
  color = _color * alpha;
  position = _pos;
}

#endif
