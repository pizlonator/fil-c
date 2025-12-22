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

vec3 upsample_YUY2(in sampler2D tex, in vec2 texCoord, in vec2 texSize, in ivec4 inReorderIdx)
{
  vec2 dx = vec2(-1.0 / texSize.x, 0.0);
  if (mod (texCoord.x * texSize.x, 2.0) < 1.0) {
    dx[1] = -dx[0];
    dx[0] = 0.0;
  }

  vec3 yuv;
  yuv.x = texture(tex, texCoord)[inReorderIdx[0]];
  /* FIXME: should get cosited sampling right... */
  vec4 texel;
  texel.xy = texture(tex, texCoord + vec2(dx[0], 0.0)).rg;
  texel.zw = texture(tex, texCoord + vec2(dx[1], 0.0)).rg;
  yuv.y = texel[inReorderIdx[1]];
  yuv.z = texel[inReorderIdx[3]];

  return yuv;
}
