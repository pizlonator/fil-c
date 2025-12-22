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
#ifdef BUILDING_CSMain_YUY2_to_AYUV
Texture2D<float4> inTex : register(t0);
RWTexture2D<unorm float4> outTex : register(u0);

void Execute (uint3 tid)
{
  float4 val = inTex.Load (tid);
  float Y0 = val.r;
  float U = val.g;
  float Y1 = val.b;
  float V = val.a;

  outTex[uint2(tid.x * 2, tid.y)] = float4 (1.0, Y0, U, V);
  outTex[uint2(tid.x * 2 + 1, tid.y)] = float4 (1.0, Y1, U, V);
}
#endif

#ifdef BUILDING_CSMain_UYVY_to_AYUV
Texture2D<float4> inTex : register(t0);
RWTexture2D<unorm float4> outTex : register(u0);

void Execute (uint3 tid)
{
  float4 val = inTex.Load (tid);
  float Y0 = val.g;
  float U = val.r;
  float Y1 = val.a;
  float V = val.b;

  outTex[uint2(tid.x * 2, tid.y)] = float4 (1.0, Y0, U, V);
  outTex[uint2(tid.x * 2 + 1, tid.y)] = float4 (1.0, Y1, U, V);
}
#endif

#ifdef BUILDING_CSMain_VYUY_to_AYUV
Texture2D<float4> inTex : register(t0);
RWTexture2D<unorm float4> outTex : register(u0);

void Execute (uint3 tid)
{
  float4 val = inTex.Load (tid);
  float Y0 = val.g;
  float U = val.b;
  float Y1 = val.a;
  float V = val.r;

  outTex[uint2(tid.x * 2, tid.y)] = float4 (1.0, Y0, U, V);
  outTex[uint2(tid.x * 2 + 1, tid.y)] = float4 (1.0, Y1, U, V);
}
#endif

#ifdef BUILDING_CSMain_YVYU_to_AYUV
Texture2D<float4> inTex : register(t0);
RWTexture2D<unorm float4> outTex : register(u0);

void Execute (uint3 tid)
{
  float4 val = inTex.Load (tid);
  float Y0 = val.r;
  float U = val.a;
  float Y1 = val.b;
  float V = val.g;

  outTex[uint2(tid.x * 2, tid.y)] = float4 (1.0, Y0, U, V);
  outTex[uint2(tid.x * 2 + 1, tid.y)] = float4 (1.0, Y1, U, V);
}
#endif

#ifdef BUILDING_CSMain_v210_to_AYUV
Texture2D<float4> inTex : register(t0);
RWTexture2D<unorm float4> outTex : register(u0);

void Execute (uint3 tid)
{
  uint xpos = tid.x * 4;
  float3 val = inTex.Load (uint3 (xpos, tid.y, 0)).xyz;
  float U0 = val.r;
  float Y0 = val.g;
  float V0 = val.b;

  val = inTex.Load (uint3 (xpos + 1, tid.y, 0)).xyz;
  float Y1 = val.r;
  float U2 = val.g;
  float Y2 = val.b;

  val = inTex.Load (uint3 (xpos + 2, tid.y, 0)).xyz;
  float V2 = val.r;
  float Y3 = val.g;
  float U4 = val.b;

  val = inTex.Load (uint3 (xpos + 3, tid.y, 0)).xyz;
  float Y4 = val.r;
  float V4 = val.g;
  float Y5 = val.b;

  xpos = tid.x * 6;
  outTex[uint2(xpos, tid.y)]     = float4 (1.0, Y0, U0, V0);
  outTex[uint2(xpos + 1, tid.y)] = float4 (1.0, Y1, U0, V0);
  outTex[uint2(xpos + 2, tid.y)] = float4 (1.0, Y2, U2, V2);
  outTex[uint2(xpos + 3, tid.y)] = float4 (1.0, Y3, U2, V2);
  outTex[uint2(xpos + 4, tid.y)] = float4 (1.0, Y4, U4, V4);
  outTex[uint2(xpos + 5, tid.y)] = float4 (1.0, Y5, U4, V4);
}
#endif

#ifdef BUILDING_CSMain_v308_to_AYUV
Texture2D<float4> inTex : register(t0);
RWTexture2D<unorm float4> outTex : register(u0);

void Execute (uint3 tid)
{
  float4 val = inTex.Load (uint3 (tid.x * 3, tid.y, 0));
  float Y0 = val.x;
  float U0 = val.y;
  float V0 = val.z;
  float Y1 = val.w;

  val = inTex.Load (uint3 (tid.x * 3 + 1, tid.y, 0));
  float U1 = val.x;
  float V1 = val.y;
  float Y2 = val.z;
  float U2 = val.w;

  val = inTex.Load (uint3 (tid.x * 3 + 2, tid.y, 0));
  float V2 = val.x;
  float Y3 = val.y;
  float U3 = val.z;
  float V3 = val.w;

  outTex[uint2(tid.x * 4, tid.y)]     = float4 (1.0, Y0, U0, V0);
  outTex[uint2(tid.x * 4 + 1, tid.y)] = float4 (1.0, Y1, U1, V1);
  outTex[uint2(tid.x * 4 + 2, tid.y)] = float4 (1.0, Y2, U2, V2);
  outTex[uint2(tid.x * 4 + 3, tid.y)] = float4 (1.0, Y3, U3, V3);
}
#endif

#ifdef BUILDING_CSMain_IYU2_to_AYUV
Texture2D<float4> inTex : register(t0);
RWTexture2D<unorm float4> outTex : register(u0);

void Execute (uint3 tid)
{
  float4 val = inTex.Load (uint3 (tid.x * 3, tid.y, 0));
  float U0 = val.x;
  float Y0 = val.y;
  float V0 = val.z;
  float U1 = val.w;

  val = inTex.Load (uint3 (tid.x * 3 + 1, tid.y, 0));
  float Y1 = val.x;
  float V1 = val.y;
  float U2 = val.z;
  float Y2 = val.w;

  val = inTex.Load (uint3 (tid.x * 3 + 2, tid.y, 0));
  float V2 = val.x;
  float U3 = val.y;
  float Y3 = val.z;
  float V3 = val.w;

  outTex[uint2(tid.x * 4, tid.y)]     = float4 (1.0, Y0, U0, V0);
  outTex[uint2(tid.x * 4 + 1, tid.y)] = float4 (1.0, Y1, U1, V1);
  outTex[uint2(tid.x * 4 + 2, tid.y)] = float4 (1.0, Y2, U2, V2);
  outTex[uint2(tid.x * 4 + 3, tid.y)] = float4 (1.0, Y3, U3, V3);
}
#endif

#ifdef BUILDING_CSMain_AYUV_to_YUY2
Texture2D<float4> inTex : register(t0);
RWTexture2D<unorm float4> outTex : register(u0);

void Execute (uint3 tid)
{
  float3 val = inTex.Load (uint3(tid.x * 2, tid.y, 0)).yzw;
  float Y0 = val.x;
  float U = val.y;
  float V = val.z;
  float Y1 = inTex.Load (uint3(tid.x * 2 + 1, tid.y, 0)).y;

  outTex[tid.xy] = float4 (Y0, U, Y1, V);
}
#endif

#ifdef BUILDING_CSMain_AYUV_to_UYVY
Texture2D<float4> inTex : register(t0);
RWTexture2D<unorm float4> outTex : register(u0);

void Execute (uint3 tid)
{
  float3 val = inTex.Load (uint3(tid.x * 2, tid.y, 0)).yzw;
  float Y0 = val.x;
  float U = val.y;
  float V = val.z;
  float Y1 = inTex.Load (uint3(tid.x * 2 + 1, tid.y, 0)).y;

  outTex[tid.xy] = float4 (U, Y0, V, Y1);
}
#endif

#ifdef BUILDING_CSMain_AYUV_to_VYUY
Texture2D<float4> inTex : register(t0);
RWTexture2D<unorm float4> outTex : register(u0);

void Execute (uint3 tid)
{
  float3 val = inTex.Load (uint3(tid.x * 2, tid.y, 0)).yzw;
  float Y0 = val.x;
  float U = val.y;
  float V = val.z;
  float Y1 = inTex.Load (uint3(tid.x * 2 + 1, tid.y, 0)).y;

  outTex[tid.xy] = float4 (V, Y0, U, Y1);
}
#endif

#ifdef BUILDING_CSMain_AYUV_to_YVYU
Texture2D<float4> inTex : register(t0);
RWTexture2D<unorm float4> outTex : register(u0);

void Execute (uint3 tid)
{
  float3 val = inTex.Load (uint3(tid.x * 2, tid.y, 0)).yzw;
  float Y0 = val.x;
  float U = val.y;
  float V = val.z;
  float Y1 = inTex.Load (uint3(tid.x * 2 + 1, tid.y, 0)).y;

  outTex[tid.xy] = float4 (Y0, V, Y1, U);
}
#endif

#ifdef BUILDING_CSMain_AYUV_to_v210
Texture2D<float4> inTex : register(t0);
RWTexture2D<unorm float4> outTex : register(u0);

void Execute (uint3 tid)
{
  uint xpos = tid.x * 6;
  float3 val = inTex.Load (uint3 (xpos, tid.y, 0)).yzw;
  float Y0 = val.x;
  float U0 = val.y;
  float V0 = val.z;
  float Y1 = inTex.Load (uint3 (xpos + 1, tid.y, 0)).y;

  val = inTex.Load (uint3 (xpos + 2, tid.y, 0)).yzw;
  float Y2 = val.x;
  float U2 = val.y;
  float V2 = val.z;
  float Y3 = inTex.Load (uint3 (xpos + 3, tid.y, 0)).y;

  val = inTex.Load (uint3 (xpos + 4, tid.y, 0)).yzw;
  float Y4 = val.x;
  float U4 = val.y;
  float V4 = val.z;
  float Y5 = inTex.Load (uint3 (xpos + 5, tid.y, 0)).y;

  xpos = tid.x * 4;
  outTex[uint2(xpos, tid.y)]     = float4 (U0, Y0, V0, 0);
  outTex[uint2(xpos + 1, tid.y)] = float4 (Y1, U2, Y2, 0);
  outTex[uint2(xpos + 2, tid.y)] = float4 (V2, Y3, U4, 0);
  outTex[uint2(xpos + 3, tid.y)] = float4 (Y4, V4, Y5, 0);
}
#endif

#ifdef BUILDING_CSMain_AYUV_to_v308
Texture2D<float4> inTex : register(t0);
RWTexture2D<unorm float4> outTex : register(u0);

void Execute (uint3 tid)
{
  float3 val0 = inTex.Load (uint3 (tid.x * 4, tid.y, 0)).yzw;
  float3 val1 = inTex.Load (uint3 (tid.x * 4 + 1, tid.y, 0)).yzw;
  float3 val2 = inTex.Load (uint3 (tid.x * 4 + 2, tid.y, 0)).yzw;
  float3 val3 = inTex.Load (uint3 (tid.x * 4 + 3, tid.y, 0)).yzw;

  outTex[uint2(tid.x * 3, tid.y)]     = float4 (val0.x, val0.y, val0.z, val1.x);
  outTex[uint2(tid.x * 3 + 1, tid.y)] = float4 (val1.y, val1.z, val2.x, val2.y);
  outTex[uint2(tid.x * 3 + 2, tid.y)] = float4 (val2.z, val3.x, val3.y, val3.z);
}
#endif

#ifdef BUILDING_CSMain_AYUV_to_IYU2
Texture2D<float4> inTex : register(t0);
RWTexture2D<unorm float4> outTex : register(u0);

void Execute (uint3 tid)
{
  float3 val0 = inTex.Load (uint3 (tid.x * 4, tid.y, 0)).zyw;
  float3 val1 = inTex.Load (uint3 (tid.x * 4 + 1, tid.y, 0)).zyw;
  float3 val2 = inTex.Load (uint3 (tid.x * 4 + 2, tid.y, 0)).zyw;
  float3 val3 = inTex.Load (uint3 (tid.x * 4 + 3, tid.y, 0)).zyw;

  outTex[uint2(tid.x * 3, tid.y)]     = float4 (val0.x, val0.y, val0.z, val1.x);
  outTex[uint2(tid.x * 3 + 1, tid.y)] = float4 (val1.y, val1.z, val2.x, val2.y);
  outTex[uint2(tid.x * 3 + 2, tid.y)] = float4 (val2.z, val3.x, val3.y, val3.z);
}
#endif

#ifdef BUILDING_CSMain_AYUV_to_Y410
Texture2D<float4> inTex : register(t0);
RWTexture2D<unorm float4> outTex : register(u0);

void Execute (uint3 tid)
{
  float4 val = inTex.Load (tid);
  float Y = val.y;
  float U = val.z;
  float V = val.w;
  float A = val.x;

  outTex[tid.xy] = float4 (U, Y, V, A);
}
#endif

#ifdef BUILDING_CSMain_RGB_to_RGBA
Texture2D<float4> inTex : register(t0);
RWTexture2D<unorm float4> outTex : register(u0);

void Execute (uint3 tid)
{
  float4 val = inTex.Load (uint3 (tid.x * 3, tid.y, 0));
  float R0 = val.r;
  float G0 = val.g;
  float B0 = val.b;
  float R1 = val.a;

  val = inTex.Load (uint3 (tid.x * 3 + 1, tid.y, 0));
  float G1 = val.r;
  float B1 = val.g;
  float R2 = val.b;
  float G2 = val.a;

  val = inTex.Load (uint3 (tid.x * 3 + 2, tid.y, 0));
  float B2 = val.r;
  float R3 = val.g;
  float G3 = val.b;
  float B3 = val.a;

  outTex[uint2(tid.x * 4, tid.y)]     = float4 (R0, G0, B0, 1.0);
  outTex[uint2(tid.x * 4 + 1, tid.y)] = float4 (R1, G1, B1, 1.0);
  outTex[uint2(tid.x * 4 + 2, tid.y)] = float4 (R2, G2, B2, 1.0);
  outTex[uint2(tid.x * 4 + 3, tid.y)] = float4 (R3, G3, B3, 1.0);
}
#endif

#ifdef BUILDING_CSMain_BGR_to_RGBA
Texture2D<float4> inTex : register(t0);
RWTexture2D<unorm float4> outTex : register(u0);

void Execute (uint3 tid)
{
  float4 val = inTex.Load (uint3 (tid.x * 3, tid.y, 0));
  float B0 = val.r;
  float G0 = val.g;
  float R0 = val.b;
  float B1 = val.a;

  val = inTex.Load (uint3 (tid.x * 3 + 1, tid.y, 0));
  float G1 = val.r;
  float R1 = val.g;
  float B2 = val.b;
  float G2 = val.a;

  val = inTex.Load (uint3 (tid.x * 3 + 2, tid.y, 0));
  float R2 = val.r;
  float B3 = val.g;
  float G3 = val.b;
  float R3 = val.a;

  outTex[uint2(tid.x * 4, tid.y)]     = float4 (R0, G0, B0, 1.0);
  outTex[uint2(tid.x * 4 + 1, tid.y)] = float4 (R1, G1, B1, 1.0);
  outTex[uint2(tid.x * 4 + 2, tid.y)] = float4 (R2, G2, B2, 1.0);
  outTex[uint2(tid.x * 4 + 3, tid.y)] = float4 (R3, G3, B3, 1.0);
}
#endif

#ifdef BUILDING_CSMain_RGB16_to_RGBA
Texture2D<uint> inTex : register(t0);
RWTexture2D<unorm float4> outTex : register(u0);

void Execute (uint3 tid)
{
  uint val = inTex.Load (tid);
  float R = float ((val & 0xf800) >> 11) / 31;
  float G = float ((val & 0x7e0) >> 5) / 63;
  float B = float ((val & 0x1f)) / 31;

  outTex[tid.xy] = float4 (R, G, B, 1.0);
}
#endif

#ifdef BUILDING_CSMain_BGR16_to_RGBA
Texture2D<uint> inTex : register(t0);
RWTexture2D<unorm float4> outTex : register(u0);

void Execute (uint3 tid)
{
  uint val = inTex.Load (tid);
  float B = float ((val & 0xf800) >> 11) / 31;
  float G = float ((val & 0x7e0) >> 5) / 63;
  float R = float ((val & 0x1f)) / 31;

  outTex[tid.xy] = float4 (R, G, B, 1.0);
}
#endif

#ifdef BUILDING_CSMain_RGB15_to_RGBA
Texture2D<uint> inTex : register(t0);
RWTexture2D<unorm float4> outTex : register(u0);

void Execute (uint3 tid)
{
  uint val = inTex.Load (tid);
  uint R = (val & 0x7c00) >> 10;
  uint G = (val & 0x3e0) >> 5;
  uint B = (val & 0x1f);

  outTex[tid.xy] = float4 (float3 (R, G, B) / 31, 1.0);
}
#endif

#ifdef BUILDING_CSMain_BGR15_to_RGBA
Texture2D<uint> inTex : register(t0);
RWTexture2D<unorm float4> outTex : register(u0);

void Execute (uint3 tid)
{
  uint val = inTex.Load (tid);
  uint B = (val & 0x7c00) >> 10;
  uint G = (val & 0x3e0) >> 5;
  uint R = (val & 0x1f);

  outTex[tid.xy] = float4 (float3 (R, G, B) / 31, 1.0);
}
#endif

#ifdef BUILDING_CSMain_r210_to_RGBA
Texture2D<uint> inTex : register(t0);
RWTexture2D<unorm float4> outTex : register(u0);

void Execute (uint3 tid)
{
  uint val = inTex.Load (tid);
  uint val_be = ((val & 0xff) << 24) | ((val & 0xff00) << 8) |
      ((val & 0xff0000) >> 8) | ((val & 0xff000000) >> 24);
  uint R = (val_be >> 20) & 0x3ff;
  uint G = (val_be >> 10) & 0x3ff;
  uint B = val_be & 0x3ff;

  outTex[tid.xy] = float4 (float3 (R, G, B) / 1023, 1.0);
}
#endif

#ifdef BUILDING_CSMain_RGBA_to_RGB
Texture2D<float4> inTex : register(t0);
RWTexture2D<unorm float4> outTex : register(u0);

void Execute (uint3 tid)
{
  float3 val0 = inTex.Load (uint3 (tid.x * 4, tid.y, 0)).rgb;
  float3 val1 = inTex.Load (uint3 (tid.x * 4 + 1, tid.y, 0)).rgb;
  float3 val2 = inTex.Load (uint3 (tid.x * 4 + 2, tid.y, 0)).rgb;
  float3 val3 = inTex.Load (uint3 (tid.x * 4 + 3, tid.y, 0)).rgb;

  outTex[uint2(tid.x * 3, tid.y)]     = float4 (val0.r, val0.g, val0.b, val1.r);
  outTex[uint2(tid.x * 3 + 1, tid.y)] = float4 (val1.g, val1.b, val2.r, val2.g);
  outTex[uint2(tid.x * 3 + 2, tid.y)] = float4 (val2.b, val3.r, val3.g, val3.b);
}
#endif

#ifdef BUILDING_CSMain_RGBA_to_BGR
Texture2D<float4> inTex : register(t0);
RWTexture2D<unorm float4> outTex : register(u0);

void Execute (uint3 tid)
{
  float3 val0 = inTex.Load (uint3 (tid.x * 4, tid.y, 0)).rgb;
  float3 val1 = inTex.Load (uint3 (tid.x * 4 + 1, tid.y, 0)).rgb;
  float3 val2 = inTex.Load (uint3 (tid.x * 4 + 2, tid.y, 0)).rgb;
  float3 val3 = inTex.Load (uint3 (tid.x * 4 + 3, tid.y, 0)).rgb;

  outTex[uint2(tid.x * 3, tid.y)]     = float4 (val0.b, val0.g, val0.r, val1.b);
  outTex[uint2(tid.x * 3 + 1, tid.y)] = float4 (val1.g, val1.r, val2.b, val2.g);
  outTex[uint2(tid.x * 3 + 2, tid.y)] = float4 (val2.r, val3.b, val3.g, val3.r);
}
#endif

#ifdef BUILDING_CSMain_RGBA_to_RGB16
Texture2D<float4> inTex : register(t0);
RWTexture2D<uint> outTex : register(u0);

void Execute (uint3 tid)
{
  float3 val = inTex.Load (tid).rgb;
  uint R = val.r * 31;
  uint G = val.g * 63;
  uint B = val.b * 31;

  outTex[tid.xy] = (R << 11) | (G << 5) | B;
}
#endif

#ifdef BUILDING_CSMain_RGBA_to_BGR16
Texture2D<float4> inTex : register(t0);
RWTexture2D<uint> outTex : register(u0);

void Execute (uint3 tid)
{
  float3 val = inTex.Load (tid).rgb;
  uint R = val.r * 31;
  uint G = val.g * 63;
  uint B = val.b * 31;

  outTex[tid.xy] = (B << 11) | (G << 5) | R;
}
#endif

#ifdef BUILDING_CSMain_RGBA_to_RGB15
Texture2D<float4> inTex : register(t0);
RWTexture2D<uint> outTex : register(u0);

void Execute (uint3 tid)
{
  uint3 val = inTex.Load (tid).rgb * 31;

  outTex[tid.xy] = (val.r << 10) | (val.g << 5) | val.b;
}
#endif

#ifdef BUILDING_CSMain_RGBA_to_BGR15
Texture2D<float4> inTex : register(t0);
RWTexture2D<uint> outTex : register(u0);

void Execute (uint3 tid)
{
  uint3 val = inTex.Load (tid).rgb * 31;

  outTex[tid.xy] = (val.b << 10) | (val.g << 5) | val.r;
}
#endif

#ifdef BUILDING_CSMain_RGBA_to_r210
Texture2D<float4> inTex : register(t0);
RWTexture2D<uint> outTex : register(u0);

void Execute (uint3 tid)
{
  uint3 val = inTex.Load (tid).rgb * 1023;
  uint packed = (val.r << 20) | (val.g << 10) | val.b;
  uint packed_be = ((packed & 0xff) << 24) | ((packed & 0xff00) << 8) |
      ((packed & 0xff0000) >> 8) | ((packed & 0xff000000) >> 24);

  outTex[tid.xy] = packed_be;
}
#endif

#ifdef BUILDING_CSMain_RGBA_to_BGRA
Texture2D<float4> inTex : register(t0);
RWTexture2D<unorm float4> outTex : register(u0);

void Execute (uint3 tid)
{
  float4 val = inTex.Load (tid);

  outTex[tid.xy] = val.bgra;
}
#endif

[numthreads(8, 8, 1)]
void ENTRY_POINT (uint3 tid : SV_DispatchThreadID)
{
  Execute (tid);
}
#else
static const char g_CSMain_converter_str[] =
"#ifdef BUILDING_CSMain_YUY2_to_AYUV\n"
"Texture2D<float4> inTex : register(t0);\n"
"RWTexture2D<unorm float4> outTex : register(u0);\n"
"\n"
"void Execute (uint3 tid)\n"
"{\n"
"  float4 val = inTex.Load (tid);\n"
"  float Y0 = val.r;\n"
"  float U = val.g;\n"
"  float Y1 = val.b;\n"
"  float V = val.a;\n"
"\n"
"  outTex[uint2(tid.x * 2, tid.y)] = float4 (1.0, Y0, U, V);\n"
"  outTex[uint2(tid.x * 2 + 1, tid.y)] = float4 (1.0, Y1, U, V);\n"
"}\n"
"#endif\n"
"\n"
"#ifdef BUILDING_CSMain_UYVY_to_AYUV\n"
"Texture2D<float4> inTex : register(t0);\n"
"RWTexture2D<unorm float4> outTex : register(u0);\n"
"\n"
"void Execute (uint3 tid)\n"
"{\n"
"  float4 val = inTex.Load (tid);\n"
"  float Y0 = val.g;\n"
"  float U = val.r;\n"
"  float Y1 = val.a;\n"
"  float V = val.b;\n"
"\n"
"  outTex[uint2(tid.x * 2, tid.y)] = float4 (1.0, Y0, U, V);\n"
"  outTex[uint2(tid.x * 2 + 1, tid.y)] = float4 (1.0, Y1, U, V);\n"
"}\n"
"#endif\n"
"\n"
"#ifdef BUILDING_CSMain_VYUY_to_AYUV\n"
"Texture2D<float4> inTex : register(t0);\n"
"RWTexture2D<unorm float4> outTex : register(u0);\n"
"\n"
"void Execute (uint3 tid)\n"
"{\n"
"  float4 val = inTex.Load (tid);\n"
"  float Y0 = val.g;\n"
"  float U = val.b;\n"
"  float Y1 = val.a;\n"
"  float V = val.r;\n"
"\n"
"  outTex[uint2(tid.x * 2, tid.y)] = float4 (1.0, Y0, U, V);\n"
"  outTex[uint2(tid.x * 2 + 1, tid.y)] = float4 (1.0, Y1, U, V);\n"
"}\n"
"#endif\n"
"\n"
"#ifdef BUILDING_CSMain_YVYU_to_AYUV\n"
"Texture2D<float4> inTex : register(t0);\n"
"RWTexture2D<unorm float4> outTex : register(u0);\n"
"\n"
"void Execute (uint3 tid)\n"
"{\n"
"  float4 val = inTex.Load (tid);\n"
"  float Y0 = val.r;\n"
"  float U = val.a;\n"
"  float Y1 = val.b;\n"
"  float V = val.g;\n"
"\n"
"  outTex[uint2(tid.x * 2, tid.y)] = float4 (1.0, Y0, U, V);\n"
"  outTex[uint2(tid.x * 2 + 1, tid.y)] = float4 (1.0, Y1, U, V);\n"
"}\n"
"#endif\n"
"\n"
"#ifdef BUILDING_CSMain_v210_to_AYUV\n"
"Texture2D<float4> inTex : register(t0);\n"
"RWTexture2D<unorm float4> outTex : register(u0);\n"
"\n"
"void Execute (uint3 tid)\n"
"{\n"
"  uint xpos = tid.x * 4;\n"
"  float3 val = inTex.Load (uint3 (xpos, tid.y, 0)).xyz;\n"
"  float U0 = val.r;\n"
"  float Y0 = val.g;\n"
"  float V0 = val.b;\n"
"\n"
"  val = inTex.Load (uint3 (xpos + 1, tid.y, 0)).xyz;\n"
"  float Y1 = val.r;\n"
"  float U2 = val.g;\n"
"  float Y2 = val.b;\n"
"\n"
"  val = inTex.Load (uint3 (xpos + 2, tid.y, 0)).xyz;\n"
"  float V2 = val.r;\n"
"  float Y3 = val.g;\n"
"  float U4 = val.b;\n"
"\n"
"  val = inTex.Load (uint3 (xpos + 3, tid.y, 0)).xyz;\n"
"  float Y4 = val.r;\n"
"  float V4 = val.g;\n"
"  float Y5 = val.b;\n"
"\n"
"  xpos = tid.x * 6;\n"
"  outTex[uint2(xpos, tid.y)]     = float4 (1.0, Y0, U0, V0);\n"
"  outTex[uint2(xpos + 1, tid.y)] = float4 (1.0, Y1, U0, V0);\n"
"  outTex[uint2(xpos + 2, tid.y)] = float4 (1.0, Y2, U2, V2);\n"
"  outTex[uint2(xpos + 3, tid.y)] = float4 (1.0, Y3, U2, V2);\n"
"  outTex[uint2(xpos + 4, tid.y)] = float4 (1.0, Y4, U4, V4);\n"
"  outTex[uint2(xpos + 5, tid.y)] = float4 (1.0, Y5, U4, V4);\n"
"}\n"
"#endif\n"
"\n"
"#ifdef BUILDING_CSMain_v308_to_AYUV\n"
"Texture2D<float4> inTex : register(t0);\n"
"RWTexture2D<unorm float4> outTex : register(u0);\n"
"\n"
"void Execute (uint3 tid)\n"
"{\n"
"  float4 val = inTex.Load (uint3 (tid.x * 3, tid.y, 0));\n"
"  float Y0 = val.x;\n"
"  float U0 = val.y;\n"
"  float V0 = val.z;\n"
"  float Y1 = val.w;\n"
"\n"
"  val = inTex.Load (uint3 (tid.x * 3 + 1, tid.y, 0));\n"
"  float U1 = val.x;\n"
"  float V1 = val.y;\n"
"  float Y2 = val.z;\n"
"  float U2 = val.w;\n"
"\n"
"  val = inTex.Load (uint3 (tid.x * 3 + 2, tid.y, 0));\n"
"  float V2 = val.x;\n"
"  float Y3 = val.y;\n"
"  float U3 = val.z;\n"
"  float V3 = val.w;\n"
"\n"
"  outTex[uint2(tid.x * 4, tid.y)]     = float4 (1.0, Y0, U0, V0);\n"
"  outTex[uint2(tid.x * 4 + 1, tid.y)] = float4 (1.0, Y1, U1, V1);\n"
"  outTex[uint2(tid.x * 4 + 2, tid.y)] = float4 (1.0, Y2, U2, V2);\n"
"  outTex[uint2(tid.x * 4 + 3, tid.y)] = float4 (1.0, Y3, U3, V3);\n"
"}\n"
"#endif\n"
"\n"
"#ifdef BUILDING_CSMain_IYU2_to_AYUV\n"
"Texture2D<float4> inTex : register(t0);\n"
"RWTexture2D<unorm float4> outTex : register(u0);\n"
"\n"
"void Execute (uint3 tid)\n"
"{\n"
"  float4 val = inTex.Load (uint3 (tid.x * 3, tid.y, 0));\n"
"  float U0 = val.x;\n"
"  float Y0 = val.y;\n"
"  float V0 = val.z;\n"
"  float U1 = val.w;\n"
"\n"
"  val = inTex.Load (uint3 (tid.x * 3 + 1, tid.y, 0));\n"
"  float Y1 = val.x;\n"
"  float V1 = val.y;\n"
"  float U2 = val.z;\n"
"  float Y2 = val.w;\n"
"\n"
"  val = inTex.Load (uint3 (tid.x * 3 + 2, tid.y, 0));\n"
"  float V2 = val.x;\n"
"  float U3 = val.y;\n"
"  float Y3 = val.z;\n"
"  float V3 = val.w;\n"
"\n"
"  outTex[uint2(tid.x * 4, tid.y)]     = float4 (1.0, Y0, U0, V0);\n"
"  outTex[uint2(tid.x * 4 + 1, tid.y)] = float4 (1.0, Y1, U1, V1);\n"
"  outTex[uint2(tid.x * 4 + 2, tid.y)] = float4 (1.0, Y2, U2, V2);\n"
"  outTex[uint2(tid.x * 4 + 3, tid.y)] = float4 (1.0, Y3, U3, V3);\n"
"}\n"
"#endif\n"
"\n"
"#ifdef BUILDING_CSMain_AYUV_to_YUY2\n"
"Texture2D<float4> inTex : register(t0);\n"
"RWTexture2D<unorm float4> outTex : register(u0);\n"
"\n"
"void Execute (uint3 tid)\n"
"{\n"
"  float3 val = inTex.Load (uint3(tid.x * 2, tid.y, 0)).yzw;\n"
"  float Y0 = val.x;\n"
"  float U = val.y;\n"
"  float V = val.z;\n"
"  float Y1 = inTex.Load (uint3(tid.x * 2 + 1, tid.y, 0)).y;\n"
"\n"
"  outTex[tid.xy] = float4 (Y0, U, Y1, V);\n"
"}\n"
"#endif\n"
"\n"
"#ifdef BUILDING_CSMain_AYUV_to_UYVY\n"
"Texture2D<float4> inTex : register(t0);\n"
"RWTexture2D<unorm float4> outTex : register(u0);\n"
"\n"
"void Execute (uint3 tid)\n"
"{\n"
"  float3 val = inTex.Load (uint3(tid.x * 2, tid.y, 0)).yzw;\n"
"  float Y0 = val.x;\n"
"  float U = val.y;\n"
"  float V = val.z;\n"
"  float Y1 = inTex.Load (uint3(tid.x * 2 + 1, tid.y, 0)).y;\n"
"\n"
"  outTex[tid.xy] = float4 (U, Y0, V, Y1);\n"
"}\n"
"#endif\n"
"\n"
"#ifdef BUILDING_CSMain_AYUV_to_VYUY\n"
"Texture2D<float4> inTex : register(t0);\n"
"RWTexture2D<unorm float4> outTex : register(u0);\n"
"\n"
"void Execute (uint3 tid)\n"
"{\n"
"  float3 val = inTex.Load (uint3(tid.x * 2, tid.y, 0)).yzw;\n"
"  float Y0 = val.x;\n"
"  float U = val.y;\n"
"  float V = val.z;\n"
"  float Y1 = inTex.Load (uint3(tid.x * 2 + 1, tid.y, 0)).y;\n"
"\n"
"  outTex[tid.xy] = float4 (V, Y0, U, Y1);\n"
"}\n"
"#endif\n"
"\n"
"#ifdef BUILDING_CSMain_AYUV_to_YVYU\n"
"Texture2D<float4> inTex : register(t0);\n"
"RWTexture2D<unorm float4> outTex : register(u0);\n"
"\n"
"void Execute (uint3 tid)\n"
"{\n"
"  float3 val = inTex.Load (uint3(tid.x * 2, tid.y, 0)).yzw;\n"
"  float Y0 = val.x;\n"
"  float U = val.y;\n"
"  float V = val.z;\n"
"  float Y1 = inTex.Load (uint3(tid.x * 2 + 1, tid.y, 0)).y;\n"
"\n"
"  outTex[tid.xy] = float4 (Y0, V, Y1, U);\n"
"}\n"
"#endif\n"
"\n"
"#ifdef BUILDING_CSMain_AYUV_to_v210\n"
"Texture2D<float4> inTex : register(t0);\n"
"RWTexture2D<unorm float4> outTex : register(u0);\n"
"\n"
"void Execute (uint3 tid)\n"
"{\n"
"  uint xpos = tid.x * 6;\n"
"  float3 val = inTex.Load (uint3 (xpos, tid.y, 0)).yzw;\n"
"  float Y0 = val.x;\n"
"  float U0 = val.y;\n"
"  float V0 = val.z;\n"
"  float Y1 = inTex.Load (uint3 (xpos + 1, tid.y, 0)).y;\n"
"\n"
"  val = inTex.Load (uint3 (xpos + 2, tid.y, 0)).yzw;\n"
"  float Y2 = val.x;\n"
"  float U2 = val.y;\n"
"  float V2 = val.z;\n"
"  float Y3 = inTex.Load (uint3 (xpos + 3, tid.y, 0)).y;\n"
"\n"
"  val = inTex.Load (uint3 (xpos + 4, tid.y, 0)).yzw;\n"
"  float Y4 = val.x;\n"
"  float U4 = val.y;\n"
"  float V4 = val.z;\n"
"  float Y5 = inTex.Load (uint3 (xpos + 5, tid.y, 0)).y;\n"
"\n"
"  xpos = tid.x * 4;\n"
"  outTex[uint2(xpos, tid.y)]     = float4 (U0, Y0, V0, 0);\n"
"  outTex[uint2(xpos + 1, tid.y)] = float4 (Y1, U2, Y2, 0);\n"
"  outTex[uint2(xpos + 2, tid.y)] = float4 (V2, Y3, U4, 0);\n"
"  outTex[uint2(xpos + 3, tid.y)] = float4 (Y4, V4, Y5, 0);\n"
"}\n"
"#endif\n"
"\n"
"#ifdef BUILDING_CSMain_AYUV_to_v308\n"
"Texture2D<float4> inTex : register(t0);\n"
"RWTexture2D<unorm float4> outTex : register(u0);\n"
"\n"
"void Execute (uint3 tid)\n"
"{\n"
"  float3 val0 = inTex.Load (uint3 (tid.x * 4, tid.y, 0)).yzw;\n"
"  float3 val1 = inTex.Load (uint3 (tid.x * 4 + 1, tid.y, 0)).yzw;\n"
"  float3 val2 = inTex.Load (uint3 (tid.x * 4 + 2, tid.y, 0)).yzw;\n"
"  float3 val3 = inTex.Load (uint3 (tid.x * 4 + 3, tid.y, 0)).yzw;\n"
"\n"
"  outTex[uint2(tid.x * 3, tid.y)]     = float4 (val0.x, val0.y, val0.z, val1.x);\n"
"  outTex[uint2(tid.x * 3 + 1, tid.y)] = float4 (val1.y, val1.z, val2.x, val2.y);\n"
"  outTex[uint2(tid.x * 3 + 2, tid.y)] = float4 (val2.z, val3.x, val3.y, val3.z);\n"
"}\n"
"#endif\n"
"\n"
"#ifdef BUILDING_CSMain_AYUV_to_IYU2\n"
"Texture2D<float4> inTex : register(t0);\n"
"RWTexture2D<unorm float4> outTex : register(u0);\n"
"\n"
"void Execute (uint3 tid)\n"
"{\n"
"  float3 val0 = inTex.Load (uint3 (tid.x * 4, tid.y, 0)).zyw;\n"
"  float3 val1 = inTex.Load (uint3 (tid.x * 4 + 1, tid.y, 0)).zyw;\n"
"  float3 val2 = inTex.Load (uint3 (tid.x * 4 + 2, tid.y, 0)).zyw;\n"
"  float3 val3 = inTex.Load (uint3 (tid.x * 4 + 3, tid.y, 0)).zyw;\n"
"\n"
"  outTex[uint2(tid.x * 3, tid.y)]     = float4 (val0.x, val0.y, val0.z, val1.x);\n"
"  outTex[uint2(tid.x * 3 + 1, tid.y)] = float4 (val1.y, val1.z, val2.x, val2.y);\n"
"  outTex[uint2(tid.x * 3 + 2, tid.y)] = float4 (val2.z, val3.x, val3.y, val3.z);\n"
"}\n"
"#endif\n"
"\n"
"#ifdef BUILDING_CSMain_AYUV_to_Y410\n"
"Texture2D<float4> inTex : register(t0);\n"
"RWTexture2D<unorm float4> outTex : register(u0);\n"
"\n"
"void Execute (uint3 tid)\n"
"{\n"
"  float4 val = inTex.Load (tid);\n"
"  float Y = val.y;\n"
"  float U = val.z;\n"
"  float V = val.w;\n"
"  float A = val.x;\n"
"\n"
"  outTex[tid.xy] = float4 (U, Y, V, A);\n"
"}\n"
"#endif\n"
"\n"
"#ifdef BUILDING_CSMain_RGB_to_RGBA\n"
"Texture2D<float4> inTex : register(t0);\n"
"RWTexture2D<unorm float4> outTex : register(u0);\n"
"\n"
"void Execute (uint3 tid)\n"
"{\n"
"  float4 val = inTex.Load (uint3 (tid.x * 3, tid.y, 0));\n"
"  float R0 = val.r;\n"
"  float G0 = val.g;\n"
"  float B0 = val.b;\n"
"  float R1 = val.a;\n"
"\n"
"  val = inTex.Load (uint3 (tid.x * 3 + 1, tid.y, 0));\n"
"  float G1 = val.r;\n"
"  float B1 = val.g;\n"
"  float R2 = val.b;\n"
"  float G2 = val.a;\n"
"\n"
"  val = inTex.Load (uint3 (tid.x * 3 + 2, tid.y, 0));\n"
"  float B2 = val.r;\n"
"  float R3 = val.g;\n"
"  float G3 = val.b;\n"
"  float B3 = val.a;\n"
"\n"
"  outTex[uint2(tid.x * 4, tid.y)]     = float4 (R0, G0, B0, 1.0);\n"
"  outTex[uint2(tid.x * 4 + 1, tid.y)] = float4 (R1, G1, B1, 1.0);\n"
"  outTex[uint2(tid.x * 4 + 2, tid.y)] = float4 (R2, G2, B2, 1.0);\n"
"  outTex[uint2(tid.x * 4 + 3, tid.y)] = float4 (R3, G3, B3, 1.0);\n"
"}\n"
"#endif\n"
"\n"
"#ifdef BUILDING_CSMain_BGR_to_RGBA\n"
"Texture2D<float4> inTex : register(t0);\n"
"RWTexture2D<unorm float4> outTex : register(u0);\n"
"\n"
"void Execute (uint3 tid)\n"
"{\n"
"  float4 val = inTex.Load (uint3 (tid.x * 3, tid.y, 0));\n"
"  float B0 = val.r;\n"
"  float G0 = val.g;\n"
"  float R0 = val.b;\n"
"  float B1 = val.a;\n"
"\n"
"  val = inTex.Load (uint3 (tid.x * 3 + 1, tid.y, 0));\n"
"  float G1 = val.r;\n"
"  float R1 = val.g;\n"
"  float B2 = val.b;\n"
"  float G2 = val.a;\n"
"\n"
"  val = inTex.Load (uint3 (tid.x * 3 + 2, tid.y, 0));\n"
"  float R2 = val.r;\n"
"  float B3 = val.g;\n"
"  float G3 = val.b;\n"
"  float R3 = val.a;\n"
"\n"
"  outTex[uint2(tid.x * 4, tid.y)]     = float4 (R0, G0, B0, 1.0);\n"
"  outTex[uint2(tid.x * 4 + 1, tid.y)] = float4 (R1, G1, B1, 1.0);\n"
"  outTex[uint2(tid.x * 4 + 2, tid.y)] = float4 (R2, G2, B2, 1.0);\n"
"  outTex[uint2(tid.x * 4 + 3, tid.y)] = float4 (R3, G3, B3, 1.0);\n"
"}\n"
"#endif\n"
"\n"
"#ifdef BUILDING_CSMain_RGB16_to_RGBA\n"
"Texture2D<uint> inTex : register(t0);\n"
"RWTexture2D<unorm float4> outTex : register(u0);\n"
"\n"
"void Execute (uint3 tid)\n"
"{\n"
"  uint val = inTex.Load (tid);\n"
"  float R = float ((val & 0xf800) >> 11) / 31;\n"
"  float G = float ((val & 0x7e0) >> 5) / 63;\n"
"  float B = float ((val & 0x1f)) / 31;\n"
"\n"
"  outTex[tid.xy] = float4 (R, G, B, 1.0);\n"
"}\n"
"#endif\n"
"\n"
"#ifdef BUILDING_CSMain_BGR16_to_RGBA\n"
"Texture2D<uint> inTex : register(t0);\n"
"RWTexture2D<unorm float4> outTex : register(u0);\n"
"\n"
"void Execute (uint3 tid)\n"
"{\n"
"  uint val = inTex.Load (tid);\n"
"  float B = float ((val & 0xf800) >> 11) / 31;\n"
"  float G = float ((val & 0x7e0) >> 5) / 63;\n"
"  float R = float ((val & 0x1f)) / 31;\n"
"\n"
"  outTex[tid.xy] = float4 (R, G, B, 1.0);\n"
"}\n"
"#endif\n"
"\n"
"#ifdef BUILDING_CSMain_RGB15_to_RGBA\n"
"Texture2D<uint> inTex : register(t0);\n"
"RWTexture2D<unorm float4> outTex : register(u0);\n"
"\n"
"void Execute (uint3 tid)\n"
"{\n"
"  uint val = inTex.Load (tid);\n"
"  uint R = (val & 0x7c00) >> 10;\n"
"  uint G = (val & 0x3e0) >> 5;\n"
"  uint B = (val & 0x1f);\n"
"\n"
"  outTex[tid.xy] = float4 (float3 (R, G, B) / 31, 1.0);\n"
"}\n"
"#endif\n"
"\n"
"#ifdef BUILDING_CSMain_BGR15_to_RGBA\n"
"Texture2D<uint> inTex : register(t0);\n"
"RWTexture2D<unorm float4> outTex : register(u0);\n"
"\n"
"void Execute (uint3 tid)\n"
"{\n"
"  uint val = inTex.Load (tid);\n"
"  uint B = (val & 0x7c00) >> 10;\n"
"  uint G = (val & 0x3e0) >> 5;\n"
"  uint R = (val & 0x1f);\n"
"\n"
"  outTex[tid.xy] = float4 (float3 (R, G, B) / 31, 1.0);\n"
"}\n"
"#endif\n"
"\n"
"#ifdef BUILDING_CSMain_r210_to_RGBA\n"
"Texture2D<uint> inTex : register(t0);\n"
"RWTexture2D<unorm float4> outTex : register(u0);\n"
"\n"
"void Execute (uint3 tid)\n"
"{\n"
"  uint val = inTex.Load (tid);\n"
"  uint val_be = ((val & 0xff) << 24) | ((val & 0xff00) << 8) |\n"
"      ((val & 0xff0000) >> 8) | ((val & 0xff000000) >> 24);\n"
"  uint R = (val_be >> 20) & 0x3ff;\n"
"  uint G = (val_be >> 10) & 0x3ff;\n"
"  uint B = val_be & 0x3ff;\n"
"\n"
"  outTex[tid.xy] = float4 (float3 (R, G, B) / 1023, 1.0);\n"
"}\n"
"#endif\n"
"\n"
"#ifdef BUILDING_CSMain_RGBA_to_RGB\n"
"Texture2D<float4> inTex : register(t0);\n"
"RWTexture2D<unorm float4> outTex : register(u0);\n"
"\n"
"void Execute (uint3 tid)\n"
"{\n"
"  float3 val0 = inTex.Load (uint3 (tid.x * 4, tid.y, 0)).rgb;\n"
"  float3 val1 = inTex.Load (uint3 (tid.x * 4 + 1, tid.y, 0)).rgb;\n"
"  float3 val2 = inTex.Load (uint3 (tid.x * 4 + 2, tid.y, 0)).rgb;\n"
"  float3 val3 = inTex.Load (uint3 (tid.x * 4 + 3, tid.y, 0)).rgb;\n"
"\n"
"  outTex[uint2(tid.x * 3, tid.y)]     = float4 (val0.r, val0.g, val0.b, val1.r);\n"
"  outTex[uint2(tid.x * 3 + 1, tid.y)] = float4 (val1.g, val1.b, val2.r, val2.g);\n"
"  outTex[uint2(tid.x * 3 + 2, tid.y)] = float4 (val2.b, val3.r, val3.g, val3.b);\n"
"}\n"
"#endif\n"
"\n"
"#ifdef BUILDING_CSMain_RGBA_to_BGR\n"
"Texture2D<float4> inTex : register(t0);\n"
"RWTexture2D<unorm float4> outTex : register(u0);\n"
"\n"
"void Execute (uint3 tid)\n"
"{\n"
"  float3 val0 = inTex.Load (uint3 (tid.x * 4, tid.y, 0)).rgb;\n"
"  float3 val1 = inTex.Load (uint3 (tid.x * 4 + 1, tid.y, 0)).rgb;\n"
"  float3 val2 = inTex.Load (uint3 (tid.x * 4 + 2, tid.y, 0)).rgb;\n"
"  float3 val3 = inTex.Load (uint3 (tid.x * 4 + 3, tid.y, 0)).rgb;\n"
"\n"
"  outTex[uint2(tid.x * 3, tid.y)]     = float4 (val0.b, val0.g, val0.r, val1.b);\n"
"  outTex[uint2(tid.x * 3 + 1, tid.y)] = float4 (val1.g, val1.r, val2.b, val2.g);\n"
"  outTex[uint2(tid.x * 3 + 2, tid.y)] = float4 (val2.r, val3.b, val3.g, val3.r);\n"
"}\n"
"#endif\n"
"\n"
"#ifdef BUILDING_CSMain_RGBA_to_RGB16\n"
"Texture2D<float4> inTex : register(t0);\n"
"RWTexture2D<uint> outTex : register(u0);\n"
"\n"
"void Execute (uint3 tid)\n"
"{\n"
"  float3 val = inTex.Load (tid).rgb;\n"
"  uint R = val.r * 31;\n"
"  uint G = val.g * 63;\n"
"  uint B = val.b * 31;\n"
"\n"
"  outTex[tid.xy] = (R << 11) | (G << 5) | B;\n"
"}\n"
"#endif\n"
"\n"
"#ifdef BUILDING_CSMain_RGBA_to_BGR16\n"
"Texture2D<float4> inTex : register(t0);\n"
"RWTexture2D<uint> outTex : register(u0);\n"
"\n"
"void Execute (uint3 tid)\n"
"{\n"
"  float3 val = inTex.Load (tid).rgb;\n"
"  uint R = val.r * 31;\n"
"  uint G = val.g * 63;\n"
"  uint B = val.b * 31;\n"
"\n"
"  outTex[tid.xy] = (B << 11) | (G << 5) | R;\n"
"}\n"
"#endif\n"
"\n"
"#ifdef BUILDING_CSMain_RGBA_to_RGB15\n"
"Texture2D<float4> inTex : register(t0);\n"
"RWTexture2D<uint> outTex : register(u0);\n"
"\n"
"void Execute (uint3 tid)\n"
"{\n"
"  uint3 val = inTex.Load (tid).rgb * 31;\n"
"\n"
"  outTex[tid.xy] = (val.r << 10) | (val.g << 5) | val.b;\n"
"}\n"
"#endif\n"
"\n"
"#ifdef BUILDING_CSMain_RGBA_to_BGR15\n"
"Texture2D<float4> inTex : register(t0);\n"
"RWTexture2D<uint> outTex : register(u0);\n"
"\n"
"void Execute (uint3 tid)\n"
"{\n"
"  uint3 val = inTex.Load (tid).rgb * 31;\n"
"\n"
"  outTex[tid.xy] = (val.b << 10) | (val.g << 5) | val.r;\n"
"}\n"
"#endif\n"
"\n"
"#ifdef BUILDING_CSMain_RGBA_to_r210\n"
"Texture2D<float4> inTex : register(t0);\n"
"RWTexture2D<uint> outTex : register(u0);\n"
"\n"
"void Execute (uint3 tid)\n"
"{\n"
"  uint3 val = inTex.Load (tid).rgb * 1023;\n"
"  uint packed = (val.r << 20) | (val.g << 10) | val.b;\n"
"  uint packed_be = ((packed & 0xff) << 24) | ((packed & 0xff00) << 8) |\n"
"      ((packed & 0xff0000) >> 8) | ((packed & 0xff000000) >> 24);\n"
"\n"
"  outTex[tid.xy] = packed_be;\n"
"}\n"
"#endif\n"
"\n"
"#ifdef BUILDING_CSMain_RGBA_to_BGRA\n"
"Texture2D<float4> inTex : register(t0);\n"
"RWTexture2D<unorm float4> outTex : register(u0);\n"
"\n"
"void Execute (uint3 tid)\n"
"{\n"
"  float4 val = inTex.Load (tid);\n"
"\n"
"  outTex[tid.xy] = val.bgra;\n"
"}\n"
"#endif\n"
"\n"
"[numthreads(8, 8, 1)]\n"
"void ENTRY_POINT (uint3 tid : SV_DispatchThreadID)\n"
"{\n"
"  Execute (tid);\n"
"}\n";
#endif
