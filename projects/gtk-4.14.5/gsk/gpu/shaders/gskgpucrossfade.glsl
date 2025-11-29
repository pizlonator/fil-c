#include "common.glsl"

PASS(0) vec2 _pos;
PASS_FLAT(1) Rect _start_rect;
PASS_FLAT(2) Rect _end_rect;
PASS(3) vec2 _start_coord;
PASS(4) vec2 _end_coord;
PASS_FLAT(5) uint _start_id;
PASS_FLAT(6) uint _end_id;
PASS_FLAT(7) float _start_opacity;
PASS_FLAT(8) float _end_opacity;


#ifdef GSK_VERTEX_SHADER

IN(0) vec4 in_rect;
IN(1) vec4 in_start_rect;
IN(2) uint in_start_id;
IN(3) vec4 in_end_rect;
IN(4) uint in_end_id;
IN(5) vec2 in_opacity_progress;

void
run (out vec2 pos)
{
  Rect r = rect_from_gsk (in_rect);
  
  pos = rect_get_position (r);

  _pos = pos;

  Rect start_rect = rect_from_gsk (in_start_rect);
  _start_rect = start_rect;
  _start_coord = rect_get_coord (start_rect, pos);
  _start_id = in_start_id;
  _start_opacity = in_opacity_progress[0] * (1.0 - in_opacity_progress[1]);

  Rect end_rect = rect_from_gsk (in_end_rect);
  _end_rect = end_rect;
  _end_coord = rect_get_coord (end_rect, pos);
  _end_id = in_end_id;
  _end_opacity = in_opacity_progress[0] * in_opacity_progress[1];
}

#endif



#ifdef GSK_FRAGMENT_SHADER

void
run (out vec4 color,
     out vec2 position)
{
  color = gsk_texture (_start_id, _start_coord) *
          rect_coverage (_start_rect, _pos) *
          _start_opacity +
          gsk_texture (_end_id, _end_coord) *
          rect_coverage (_end_rect, _pos) *
          _end_opacity;
  position = _pos;
}

#endif
