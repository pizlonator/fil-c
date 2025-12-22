/* GStreamer
 * Copyright (C) 2023 Seungha Yang <seungha@centricular.com>
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
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

cbuffer CheckerConstBuffer : register(b0)
{
  float width;
  float height;
  float checker_size;
  float alpha;
};

struct PS_INPUT
{
  float4 Position: SV_POSITION;
  float2 Texture: TEXCOORD;
};

float4 PSMain_checker (PS_INPUT input) : SV_Target
{
  float4 output;
  float2 xy_mod = floor (0.5 * input.Texture * float2 (width, height) / checker_size);
  float result = fmod (xy_mod.x + xy_mod.y, 2.0);
  output.r = step (result, 0.5);
  output.g = 1.0 - output.r;
  output.b = 0;
  output.a = alpha;
  return output;
}
