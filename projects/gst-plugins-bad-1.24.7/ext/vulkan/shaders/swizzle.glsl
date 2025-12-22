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

#ifndef _SWIZZLE_H_
#define _SWIZZLE_H_

vec4 swizzle(in vec4 texel, in ivec4 swizzle_idx)
{
  return vec4(texel[swizzle_idx[0]], texel[swizzle_idx[1]], texel[swizzle_idx[2]], texel[swizzle_idx[3]]);
}

vec3 swizzle(in vec3 texel, in ivec3 swizzle_idx)
{
  return vec3(texel[swizzle_idx[0]], texel[swizzle_idx[1]], texel[swizzle_idx[2]]);
}
#endif
