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

#ifdef BUILDING_HLSL
struct PS_INPUT
{
  float4 Position: SV_POSITION;
  float4 Color: COLOR;
};

float4 PSMain_color (PS_INPUT input) : SV_TARGET
{
  return input.Color;
}
#else
static const char g_PSMain_color_str[] =
"struct PS_INPUT\n"
"{\n"
"  float4 Position: SV_POSITION;\n"
"  float4 Color: COLOR;\n"
"};\n"
"\n"
"float4 PSMain_color (PS_INPUT input) : SV_TARGET\n"
"{\n"
"  return input.Color;\n"
"}\n";
#endif
