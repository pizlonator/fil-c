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

struct ColorMatrices
{
  mat4 to_RGB_matrix;
  mat4 primaries_matrix;
  mat4 to_YUV_matrix;
};

vec3 color_matrix_convert (in vec3 texel, in mat4 color_matrix)
{
  vec4 rgb_ = color_matrix * vec4(texel, 1.0);
  return rgb_.rgb;
}

vec3 color_convert_texel (in vec3 texel, in ColorMatrices m)
{
  /* FIXME: need to add gamma remapping between these stages */
  vec3 tmp = color_matrix_convert (texel, m.to_RGB_matrix);
  tmp = color_matrix_convert (tmp, m.primaries_matrix);
  return color_matrix_convert (tmp, m.to_YUV_matrix);
}
