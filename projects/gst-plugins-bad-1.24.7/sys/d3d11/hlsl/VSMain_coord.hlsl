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
struct VS_INPUT
{
  float4 Position : POSITION;
  float2 Texture : TEXCOORD;
};

struct VS_OUTPUT
{
  float4 Position : SV_POSITION;
  float2 Texture : TEXCOORD;
};

VS_OUTPUT VSMain_coord (VS_INPUT input)
{
  return input;
}
#else
static const char g_VSMain_coord_str[] =
"struct VS_INPUT\n"
"{\n"
"  float4 Position : POSITION;\n"
"  float2 Texture : TEXCOORD;\n"
"};\n"
"\n"
"struct VS_OUTPUT\n"
"{\n"
"  float4 Position : SV_POSITION;\n"
"  float2 Texture : TEXCOORD;\n"
"};\n"
"\n"
"VS_OUTPUT VSMain_coord (VS_INPUT input)\n"
"{\n"
"  return input;\n"
"}\n";
#endif
