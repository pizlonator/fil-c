#ifndef _PATTERN_
#define _PATTERN_

#include "common.glsl"
#include "blendmode.glsl"
#include "gradient.glsl"
#include "rect.glsl"

#ifdef GSK_FRAGMENT_SHADER

vec4 stack[GSK_GPU_PATTERN_STACK_SIZE];
uint stack_size = 0u;

void
stack_push (vec4 data)
{
  stack[stack_size] = data;
  stack_size++;
}

vec4
stack_pop (void)
{
  stack_size--;
  return stack[stack_size];
}

struct Position
{
  /* pos.xy is the actual position
     pos.zw is the fwidth() of it
  */
  vec4 pos;
};

vec2
position (Position pos)
{
  return pos.pos.xy;
}

vec2
position_fwidth (Position pos)
{
  return pos.pos.zw;
}

Position
position_new (vec2 pos)
{
  return Position (vec4 (pos, fwidth (pos)));
}

uint
read_uint (inout uint reader)
{
  uint result = gsk_get_uint (reader);
  reader++;
  return result;
}

float
read_float (inout uint reader)
{
  float result = gsk_get_float (reader);
  reader++;
  return result;
}

vec2
read_vec2 (inout uint reader)
{
  return vec2 (read_float (reader), read_float (reader));
}

vec4
read_vec4 (inout uint reader)
{
  return vec4 (read_float (reader), read_float (reader), read_float (reader), read_float (reader));
}

Rect
read_rect (inout uint reader)
{
  return rect_new_size (read_vec4 (reader));
}

mat4
read_mat4 (inout uint reader)
{
  mat4 result = mat4 (gsk_get_float (reader + 0u), gsk_get_float (reader + 1u),
                      gsk_get_float (reader + 2u), gsk_get_float (reader + 3u),
                      gsk_get_float (reader + 4u), gsk_get_float (reader + 5u),
                      gsk_get_float (reader + 6u), gsk_get_float (reader + 7u),
                      gsk_get_float (reader + 8u), gsk_get_float (reader + 9u),
                      gsk_get_float (reader + 10u), gsk_get_float (reader + 11u),
                      gsk_get_float (reader + 12u), gsk_get_float (reader + 13u),
                      gsk_get_float (reader + 14u), gsk_get_float (reader + 15u));
  reader += 16u;
  return result;
}

Gradient
read_gradient (inout uint reader)
{
  Gradient gradient = gradient_new (reader);
  reader += gradient_get_size (gradient);

  return gradient;
}

void
clip_pattern (inout uint reader,
              inout vec4 color,
              Position   pos)
{
  Rect clip = read_rect (reader);
  float alpha = rect_coverage (clip, position (pos), abs (position_fwidth (pos)));

  color *= alpha;
}

void
opacity_pattern (inout uint reader,
                 inout vec4 color)
{
  float opacity = read_float (reader);

  color *= opacity;
}

void
color_matrix_pattern (inout uint reader,
                      inout vec4 color)
{
  mat4 matrix = read_mat4 (reader);
  vec4 offset = read_vec4 (reader);

  color = color_unpremultiply (color);

  color = matrix * color + offset;
  color = clamp(color, 0.0, 1.0);

  color = color_premultiply (color);
}

void
repeat_push_pattern (inout uint     reader,
                     inout Position pos)
{
  stack_push (pos.pos);
  Rect bounds = read_rect (reader);

  vec2 size = rect_size (bounds);
  pos.pos.xy = mod (pos.pos.xy - bounds.bounds.xy, size);
  /* make sure we have a positive result */
  pos.pos.xy = mix (pos.pos.xy, pos.pos.xy + size, lessThan (pos.pos.xy, vec2 (0.0)));
  pos.pos.xy += bounds.bounds.xy;
}

void
affine_pattern (inout uint     reader,
                inout Position pos)
{
  stack_push (pos.pos);
  vec4 transform = read_vec4 (reader);

  pos.pos.zw *= transform.zw;
  pos.pos.xy -= transform.xy;
  pos.pos.xy *= transform.zw;
}

void
position_pop_pattern (inout uint     reader,
                      inout Position pos)
{
  pos = Position (stack_pop ());
}

void
cross_fade_pattern (inout uint reader,
                    inout vec4 color)
{
  vec4 start = stack_pop ();
  float progress = read_float (reader);

  color = mix (start, color, progress);
}

void
mask_alpha_pattern (inout uint reader,
                    inout vec4 color)
{
  vec4 source = stack_pop ();

  color = source * color.a;
}

void
mask_inverted_alpha_pattern (inout uint reader,
                             inout vec4 color)
{
  vec4 source = stack_pop ();

  color = source * (1.0 - color.a);
}

void
mask_luminance_pattern (inout uint reader,
                        inout vec4 color)
{
  vec4 source = stack_pop ();

  color = source * luminance (color.rgb);
}

void
mask_inverted_luminance_pattern (inout uint reader,
                                 inout vec4 color)
{
  vec4 source = stack_pop ();

  color = source * (color.a - luminance (color.rgb));
}

void
blend_mode_pattern (inout vec4 color,
                    uint       mode)
{
  vec4 bottom = stack_pop ();
  
  color = blend_mode (bottom, color, mode);
}

vec4
glyphs_pattern (inout uint reader,
                Position   pos)
{
  float opacity = 0.0;
  vec4 color = color_premultiply (read_vec4 (reader));
  uint num_glyphs = read_uint (reader);
  uint i;

  vec2 p = position (pos);
  vec2 dFdp = abs (position_fwidth (pos));
  for (i = 0u; i < num_glyphs; i++)
    {
      uint tex_id = read_uint (reader);
      Rect glyph_bounds = read_rect (reader);
      vec4 tex_rect = read_vec4 (reader);

      float coverage = rect_coverage (glyph_bounds, p, dFdp);
      if (coverage > 0.0)
        opacity += coverage * gsk_texture (tex_id, (p - tex_rect.xy) / tex_rect.zw).a;
    }

  return color * opacity;
}

vec4
texture_pattern (inout uint reader,
                 Position   pos)
{
  uint tex_id = read_uint (reader);
  vec4 tex_rect = read_vec4 (reader);

  return gsk_texture (tex_id, (position (pos) - tex_rect.xy) / tex_rect.zw);
}

vec4
straight_alpha_pattern (inout uint reader,
                        Position   pos)
{
  uint tex_id = read_uint (reader);
  vec4 tex_rect = read_vec4 (reader);

  return gsk_texture_straight_alpha (tex_id, (position (pos) - tex_rect.xy) / tex_rect.zw);
}

vec4
linear_gradient_pattern (inout uint reader,
                         Position   pos,
                         bool       repeating)
{
  vec2 start = read_vec2 (reader);
  vec2 end = read_vec2 (reader);
  Gradient gradient = read_gradient (reader);

  vec2 line = end - start;
  float line_length = dot (line, line);
  float offset = dot (position (pos) - start, line) / line_length;
  float other_offset = dot (position (pos) + position_fwidth (pos) - start, line) / line_length;
  float d_offset = 0.5 * abs (offset - other_offset);

  if (repeating)
    return gradient_get_color_repeating (gradient, offset - d_offset, offset + d_offset);
  else
    return gradient_get_color (gradient, offset - d_offset, offset + d_offset);
}

vec4
radial_gradient_pattern (inout uint reader,
                         Position   pos,
                         bool       repeating)
{
  vec2 center = read_vec2 (reader);
  vec2 radius = read_vec2 (reader);
  float start = read_float (reader);
  float end = read_float (reader);
  Gradient gradient = read_gradient (reader);

  float offset = length ((position (pos) - center) / radius);
  float other_offset = length ((position (pos) + position_fwidth (pos) - center) / radius);
  offset = (offset - start) / (end - start);
  other_offset = (other_offset - start) / (end - start);
  float d_offset = abs (0.5 * (offset - other_offset));

  if (repeating)
    return gradient_get_color_repeating (gradient, offset - d_offset, offset + d_offset);
  else
    return gradient_get_color (gradient, offset - d_offset, offset + d_offset);
}

vec4
conic_gradient_pattern (inout uint reader,
                        Position   pos)
{
  vec2 center = read_vec2 (reader);
  float angle = read_float (reader);
  Gradient gradient = read_gradient (reader);

  /* scaling modifies angles, so be sure to use right coordinate system */
  vec2 dpos = position (pos) - center;
  vec2 dpos2 = (position (pos) + position_fwidth (pos)) - center;
  float offset = atan (dpos.y, dpos.x);
  float offset2 = atan (dpos2.y, dpos2.x);
  offset = degrees (offset + angle) / 360.0;
  offset2 = degrees (offset2 + angle) / 360.0;
  float overflow = fract (offset + 0.5);
  float overflow2 = fract (offset2 + 0.5);
  offset = fract (offset);
  offset2 = fract (offset2);
  float d_offset = max (0.00001, 0.5 * min (abs (offset - offset2), abs (overflow - overflow2)));

  return gradient_get_color_repeating (gradient, offset - d_offset, offset + d_offset);
}

vec4
color_pattern (inout uint reader)
{
  vec4 color = read_vec4 (reader);

  return color_premultiply (color);
}

vec4
pattern (uint reader,
         vec2 pos_)
{
  vec4 color = vec4 (1.0, 0.0, 0.8, 1.0); /* pink */
  Position pos = position_new (pos_ / GSK_GLOBAL_SCALE);

  for(;;)
    {
      uint type = read_uint (reader);
      switch (type)
      {
        default:
        case GSK_GPU_PATTERN_DONE:
          return color;
        case GSK_GPU_PATTERN_COLOR:
          color = color_pattern (reader);
          break;
        case GSK_GPU_PATTERN_TEXTURE:
          color = texture_pattern (reader, pos);
          break;
        case GSK_GPU_PATTERN_STRAIGHT_ALPHA:
          color = straight_alpha_pattern (reader, pos);
          break;
        case GSK_GPU_PATTERN_GLYPHS:
          color = glyphs_pattern (reader, pos);
          break;
        case GSK_GPU_PATTERN_COLOR_MATRIX:
          color_matrix_pattern (reader, color);
          break;
        case GSK_GPU_PATTERN_OPACITY:
          opacity_pattern (reader, color);
          break;
        case GSK_GPU_PATTERN_LINEAR_GRADIENT:
          color = linear_gradient_pattern (reader, pos, false);
          break;
        case GSK_GPU_PATTERN_REPEATING_LINEAR_GRADIENT:
          color = linear_gradient_pattern (reader, pos, true);
          break;
        case GSK_GPU_PATTERN_RADIAL_GRADIENT:
          color = radial_gradient_pattern (reader, pos, false);
          break;
        case GSK_GPU_PATTERN_REPEATING_RADIAL_GRADIENT:
          color = radial_gradient_pattern (reader, pos, true);
          break;
        case GSK_GPU_PATTERN_CONIC_GRADIENT:
          color = conic_gradient_pattern (reader, pos);
          break;
        case GSK_GPU_PATTERN_CLIP:
          clip_pattern (reader, color, pos);
          break;
        case GSK_GPU_PATTERN_REPEAT_PUSH:
          repeat_push_pattern (reader, pos);
          break;
        case GSK_GPU_PATTERN_POSITION_POP:
          position_pop_pattern (reader, pos);
          break;
        case GSK_GPU_PATTERN_PUSH_COLOR:
          stack_push (color);
          color = vec4 (0.0);
          break;
        case GSK_GPU_PATTERN_POP_CROSS_FADE:
          cross_fade_pattern (reader, color);
          break;
        case GSK_GPU_PATTERN_POP_MASK_ALPHA:
          mask_alpha_pattern (reader, color);
          break;
        case GSK_GPU_PATTERN_POP_MASK_INVERTED_ALPHA:
          mask_inverted_alpha_pattern (reader, color);
          break;
        case GSK_GPU_PATTERN_POP_MASK_LUMINANCE:
          mask_luminance_pattern (reader, color);
          break;
        case GSK_GPU_PATTERN_POP_MASK_INVERTED_LUMINANCE:
          mask_inverted_luminance_pattern (reader, color);
          break;
        case GSK_GPU_PATTERN_AFFINE:
          affine_pattern (reader, pos);
          break;
        case GSK_GPU_PATTERN_BLEND_DEFAULT:
        case GSK_GPU_PATTERN_BLEND_MULTIPLY:
        case GSK_GPU_PATTERN_BLEND_SCREEN:
        case GSK_GPU_PATTERN_BLEND_OVERLAY:
        case GSK_GPU_PATTERN_BLEND_DARKEN:
        case GSK_GPU_PATTERN_BLEND_LIGHTEN:
        case GSK_GPU_PATTERN_BLEND_COLOR_DODGE:
        case GSK_GPU_PATTERN_BLEND_COLOR_BURN:
        case GSK_GPU_PATTERN_BLEND_HARD_LIGHT:
        case GSK_GPU_PATTERN_BLEND_SOFT_LIGHT:
        case GSK_GPU_PATTERN_BLEND_DIFFERENCE:
        case GSK_GPU_PATTERN_BLEND_EXCLUSION:
        case GSK_GPU_PATTERN_BLEND_COLOR:
        case GSK_GPU_PATTERN_BLEND_HUE:
        case GSK_GPU_PATTERN_BLEND_SATURATION:
        case GSK_GPU_PATTERN_BLEND_LUMINOSITY:
          blend_mode_pattern (color, type - GSK_GPU_PATTERN_BLEND_DEFAULT);
          break;
      }
    }
}

#endif

#endif
