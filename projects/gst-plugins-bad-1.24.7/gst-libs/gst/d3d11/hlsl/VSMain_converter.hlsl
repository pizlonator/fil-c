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
cbuffer VsConstBuffer : register(b0)
{
  matrix Transform;
};

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

VS_OUTPUT VSMain_converter (VS_INPUT input)
{
  VS_OUTPUT output;

  output.Position = mul (Transform, input.Position);
  output.Texture = input.Texture;

  return output;
}
#else
static const char g_VSMain_converter_str[] =
"cbuffer VsConstBuffer : register(b0)\n"
"{\n"
"  matrix Transform;\n"
"};\n"
"\n"
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
"VS_OUTPUT VSMain_converter (VS_INPUT input)\n"
"{\n"
"  VS_OUTPUT output;\n"
"\n"
"  output.Position = mul (Transform, input.Position);\n"
"  output.Texture = input.Texture;\n"
"\n"
"  return output;\n"
"}\n";
#endif
