#include "common.glsl"

#define VARIATION_OPACITY        (1u << 0)
#define VARIATION_STRAIGHT_ALPHA (1u << 1)

#define HAS_VARIATION(var) ((GSK_VARIATION & var) == var)

PASS(0) vec2 _pos;
PASS_FLAT(1) Rect _rect;
PASS(2) vec2 _tex_coord;
PASS_FLAT(3) uint _tex_id;
PASS_FLAT(4) float _opacity;


#ifdef GSK_VERTEX_SHADER

IN(0) vec4 in_rect;
IN(1) vec4 in_tex_rect;
IN(2) uint in_tex_id;
IN(3) float in_opacity;

void
run (out vec2 pos)
{
  Rect r = rect_from_gsk (in_rect);
  
  pos = rect_get_position (r);

  _pos = pos;
  _rect = r;
  _tex_coord = rect_get_coord (rect_from_gsk (in_tex_rect), pos);
  _tex_id = in_tex_id;
  _opacity = in_opacity;
}

#endif



#ifdef GSK_FRAGMENT_SHADER

void
run (out vec4 color,
     out vec2 position)
{
  float alpha = rect_coverage (_rect, _pos);
  if (HAS_VARIATION (VARIATION_OPACITY))
    alpha *= _opacity;

  if (HAS_VARIATION (VARIATION_STRAIGHT_ALPHA))
    color = gsk_texture_straight_alpha (_tex_id, _tex_coord) * alpha;
  else
    color = gsk_texture (_tex_id, _tex_coord) * alpha;

  position = _pos;
}

#endif
