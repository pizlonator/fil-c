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
static const float blocksize = 8.0;
static const float4 high = float4 (0.5, 0.5, 0.667, 1.0);
static const float4 low = float4 (0.5, 0.5, 0.333, 1.0);

struct PS_INPUT
{
  float4 Position : SV_POSITION;
};

struct PS_OUTPUT
{
  float4 Plane : SV_TARGET;
};

PS_OUTPUT PSMain_checker_vuya (PS_INPUT input)
{
  PS_OUTPUT output;
  if ((input.Position.x % (blocksize * 2.0)) >= blocksize) {
    if ((input.Position.y % (blocksize * 2.0)) >= blocksize)
      output.Plane = low;
    else
      output.Plane = high;
  } else {
    if ((input.Position.y % (blocksize * 2.0)) < blocksize)
      output.Plane = low;
    else
      output.Plane = high;
  }
  return output;
}
#else
static const char g_PSMain_checker_vuya_str[] =
"static const float blocksize = 8.0;\n"
"static const float4 high = float4 (0.5, 0.5, 0.667, 1.0);\n"
"static const float4 low = float4 (0.5, 0.5, 0.333, 1.0);\n"
"\n"
"struct PS_INPUT\n"
"{\n"
"  float4 Position : SV_POSITION;\n"
"};\n"
"\n"
"struct PS_OUTPUT\n"
"{\n"
"  float4 Plane : SV_TARGET;\n"
"};\n"
"\n"
"PS_OUTPUT PSMain_checker_vuya (PS_INPUT input)\n"
"{\n"
"  PS_OUTPUT output;\n"
"  if ((input.Position.x % (blocksize * 2.0)) >= blocksize) {\n"
"    if ((input.Position.y % (blocksize * 2.0)) >= blocksize)\n"
"      output.Plane = low;\n"
"    else\n"
"      output.Plane = high;\n"
"  } else {\n"
"    if ((input.Position.y % (blocksize * 2.0)) < blocksize)\n"
"      output.Plane = low;\n"
"    else\n"
"      output.Plane = high;\n"
"  }\n"
"  return output;\n"
"}\n";
#endif
