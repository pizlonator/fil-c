#include "common.glsl"

/* blur radius (aka in_blur_direction) 0 is NOT supported and MUST be caught before */

#define VARIATION_COLORIZE ((GSK_VARIATION & 1u) == 1u)

PASS(0) vec2 _pos;
PASS_FLAT(1) Rect _rect;
PASS_FLAT(2) vec4 _blur_color;
PASS(3) vec2 _tex_coord;
PASS_FLAT(4) vec2 _tex_blur_step;
PASS_FLAT(5) uint _samples_per_side;
PASS_FLAT(6) uint _tex_id;
PASS_FLAT(7) vec3 _initial_gaussian;


#ifdef GSK_VERTEX_SHADER

IN(0) vec4 in_rect;
IN(1) vec4 in_blur_color;
IN(2) vec4 in_tex_rect;
IN(3) vec2 in_blur_direction;
IN(4) uint in_tex_id;

void
run (out vec2 pos)
{
  Rect r = rect_from_gsk (in_rect);
  
  pos = rect_get_position (r);

  _pos = pos;
  _rect = r;
  _blur_color = in_blur_color;
  Rect tex_rect = rect_from_gsk (in_tex_rect);
  _tex_coord = rect_get_coord (tex_rect, pos);
  _tex_id = in_tex_id;

  float blur_radius = length (GSK_GLOBAL_SCALE * in_blur_direction);
  _tex_blur_step = GSK_GLOBAL_SCALE * in_blur_direction / blur_radius / rect_size (tex_rect);
  _samples_per_side = uint (floor (blur_radius));
  float sigma = blur_radius / 2.0;
  _initial_gaussian.x = 1.0 / (sqrt (2.0 * PI) * sigma);
  _initial_gaussian.y = exp (-0.5 / (sigma * sigma));
  _initial_gaussian.z = _initial_gaussian.y * _initial_gaussian.y;
}

#endif


#ifdef GSK_FRAGMENT_SHADER

// Partially from http://callumhay.blogspot.com/2010/09/gaussian-blur-shader-glsl.html
void
run (out vec4 color,
     out vec2 position)
{
  vec3 incremental_gaussian = _initial_gaussian;
  vec4 sum = gsk_texture (_tex_id, _tex_coord) * incremental_gaussian.x;
  float coefficient_sum = incremental_gaussian.x;
  incremental_gaussian.xy *= incremental_gaussian.yz;
  vec2 p = _tex_blur_step;

  for (uint i = 0u; i < _samples_per_side; i++)
    {
      sum += gsk_texture (_tex_id, _tex_coord - p) * incremental_gaussian.x;
      sum += gsk_texture (_tex_id, _tex_coord + p) * incremental_gaussian.x;

      coefficient_sum += 2.0 * incremental_gaussian.x;
      incremental_gaussian.xy *= incremental_gaussian.yz;

      p += _tex_blur_step;
    }

  if (VARIATION_COLORIZE)
    color = _blur_color * sum.a / coefficient_sum;
  else
    color = sum / coefficient_sum;
  position = _pos;
}

#endif
