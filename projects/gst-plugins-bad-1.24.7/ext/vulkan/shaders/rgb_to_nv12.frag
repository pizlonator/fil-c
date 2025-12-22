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

#include "color_convert_generic.glsl"
#include "swizzle.glsl"

layout(location = 0) in vec2 inTexCoord;

layout(set = 0, binding = 0) uniform reorder {
  ivec4 in_reorder_idx;
  ivec4 out_reorder_idx;
  ivec2 texSize;
  ColorMatrices matrices;
};
layout(set = 0, binding = 1) uniform sampler2D inTexture0;

layout(location = 0) out vec4 outColor0;
layout(location = 1) out vec4 outColor1;

void main()
{
  vec4 texel = swizzle (texture(inTexture0, inTexCoord), in_reorder_idx);
  vec4 uv_texel = swizzle (texture(inTexture0, inTexCoord * vec2(2.0)), in_reorder_idx);
  vec3 yuv1 = color_convert_texel (texel.rgb, matrices);
  vec3 yuv2 = color_convert_texel (uv_texel.rgb, matrices);
  vec3 yuv = vec3(yuv1.x, yuv2.y, yuv2.z);

  outColor0 = vec4(yuv[out_reorder_idx[0]], 0.0, 0.0, 1.0);
  outColor1 = vec4(yuv[out_reorder_idx[1]], yuv[out_reorder_idx[2]], 0.0, 1.0);
}
