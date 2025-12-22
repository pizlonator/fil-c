/* GStreamer
 * Copyright (C) 2020 Matthew Waters <matthew@centricular.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#version 450 core

#include "view_defines.h"
#include "swizzle.glsl"

layout(location = 0) in vec2 inTexCoord;

layout(set = 0, binding = 0) uniform ViewConvert {
  ivec4 in_reorder_idx;
  ivec4 out_reorder_idx;
  vec4 tex_offset;
  vec4 tex_scale;
  ivec2 tex_size;
  int output_type;
  mat3 downmix[2];
};
layout(set = 0, binding = 1) uniform sampler2D l_tex;
layout(set = 0, binding = 2) uniform sampler2D r_tex;

layout(location = 0) out vec4 outColor0;

void main()
{
  vec2 l_coord = inTexCoord * tex_scale.xy + tex_offset.xy;
  vec2 r_coord = inTexCoord * tex_scale.zw + tex_offset.zw;
  vec4 l = swizzle(texture(l_tex, l_coord), in_reorder_idx);
  vec4 r = swizzle(texture(r_tex, r_coord), in_reorder_idx);

  if (output_type == VIEW_MONO_DOWNMIX) {
    vec3 lcol = l.rgb * l.a + vec3(1.0-l.a);
    vec3 rcol = r.rgb * r.a + vec3(1.0-r.a);
    if (l.a + r.a > 0.0) {
      lcol = clamp (downmix[0] * lcol, 0.0, 1.0);
      rcol = clamp (downmix[1] * rcol, 0.0, 1.0);
      outColor0 = vec4 (lcol + rcol, 1.0);
    } else {
      outColor0 = vec4 (0.0);
    }
  } else if (output_type == VIEW_MONO_LEFT) {
    outColor0 = swizzle(l, out_reorder_idx);
  } else if (output_type == VIEW_MONO_RIGHT) {
    outColor0 = swizzle(r, out_reorder_idx);
  } else if (output_type == VIEW_SIDE_BY_SIDE) {
    if (inTexCoord.x < 0.5) {
      outColor0 = swizzle(l, out_reorder_idx);
    } else {
      outColor0 = swizzle(r, out_reorder_idx);
    }
  } else if (output_type == VIEW_TOP_BOTTOM) {
    if (inTexCoord.y < 0.5) {
      outColor0 = swizzle(l, out_reorder_idx);
    } else {
      outColor0 = swizzle(r, out_reorder_idx);
    }
  } else if (output_type == VIEW_COLUMN_INTERLEAVED) {
    if (int(mod(l_coord.x * tex_size.x, 2.0)) == 0) {
      outColor0 = swizzle(l, out_reorder_idx);
    } else {
      outColor0 = swizzle(r, out_reorder_idx);
    }
  } else if (output_type == VIEW_ROW_INTERLEAVED) {
    if (int(mod(l_coord.y * tex_size.y, 2.0)) == 0) {
      outColor0 = swizzle(l, out_reorder_idx);
    } else {
      outColor0 = swizzle(r, out_reorder_idx);
    }
  } else if (output_type == VIEW_CHECKERBOARD) {
     if (int(mod(l_coord.x * tex_size.x, 2.0)) ==
         int(mod(l_coord.y * tex_size.y, 2.0))) {
      outColor0 = swizzle(l, out_reorder_idx);
    } else {
      outColor0 = swizzle(r, out_reorder_idx);
    }
  } else {
    outColor0 = vec4(1.0, 0.0, 1.0, 1.0);
  }
}
