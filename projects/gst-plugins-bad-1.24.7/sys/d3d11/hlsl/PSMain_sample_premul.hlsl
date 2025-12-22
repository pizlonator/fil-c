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
Texture2D shaderTexture;
SamplerState samplerState;

struct PS_INPUT
{
  float4 Position : SV_POSITION;
  float2 Texture : TEXCOORD;
};

float4 PSMain_sample_premul (PS_INPUT input): SV_TARGET
{
  float4 sample = shaderTexture.Sample (samplerState, input.Texture);
  float4 premul_sample;
  premul_sample.r = saturate (sample.r * sample.a);
  premul_sample.g = saturate (sample.g * sample.a);
  premul_sample.b = saturate (sample.b * sample.a);
  premul_sample.a = sample.a;
  return premul_sample;
}
#else
static const char g_PSMain_sample_premul_str[] =
"Texture2D shaderTexture;\n"
"SamplerState samplerState;\n"
"\n"
"struct PS_INPUT\n"
"{\n"
"  float4 Position : SV_POSITION;\n"
"  float2 Texture : TEXCOORD;\n"
"};\n"
"\n"
"float4 PSMain_sample_premul (PS_INPUT input): SV_TARGET\n"
"{\n"
"  float4 sample = shaderTexture.Sample (samplerState, input.Texture);\n"
"  float4 premul_sample;\n"
"  premul_sample.r = saturate (sample.r * sample.a);\n"
"  premul_sample.g = saturate (sample.g * sample.a);\n"
"  premul_sample.b = saturate (sample.b * sample.a);\n"
"  premul_sample.a = sample.a;\n"
"  return premul_sample;\n"
"}\n";
#endif
