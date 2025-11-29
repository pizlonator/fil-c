#include "common.glsl"
#include "pattern.glsl"

PASS(0) vec2 _pos;
PASS_FLAT(1) Rect _rect;
PASS_FLAT(2) uint _pattern_id;


#ifdef GSK_VERTEX_SHADER

IN(0) vec4 in_rect;
IN(1) uint in_pattern_id;

void
run (out vec2 pos)
{
  Rect r = rect_from_gsk (in_rect);
  
  pos = rect_get_position (r);

  _pos = pos;
  _rect = r;
  _pattern_id = in_pattern_id;
}

#endif



#ifdef GSK_FRAGMENT_SHADER

void
run (out vec4 color,
     out vec2 position)
{
  color = pattern (_pattern_id, _pos);
  color.a *= rect_coverage (_rect, _pos);
  position = _pos;
}

#endif
