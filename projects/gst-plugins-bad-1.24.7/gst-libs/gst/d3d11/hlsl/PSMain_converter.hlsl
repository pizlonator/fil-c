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
struct PSColorSpace
{
  float3 CoeffX;
  float3 CoeffY;
  float3 CoeffZ;
  float3 Offset;
  float3 Min;
  float3 Max;
  float padding;
};

cbuffer PsConstBuffer : register(b0)
{
  PSColorSpace preCoeff;
  PSColorSpace postCoeff;
  PSColorSpace primariesCoeff;
  float alphaFactor;
};

Texture2D shaderTexture[4] : register(t0);
Texture1D<float> gammaDecLUT : register(t4);
Texture1D<float> gammaEncLUT: register(t5);
SamplerState samplerState : register(s0);
SamplerState linearSampler : register(s1);

struct PS_INPUT
{
  float4 Position: SV_POSITION;
  float2 Texture: TEXCOORD;
};

struct PS_OUTPUT_LUMA
{
  float4 Plane0: SV_TARGET0;
};

struct PS_OUTPUT_CHROMA
{
  float4 Plane0: SV_TARGET0;
};

struct PS_OUTPUT_CHROMA_PLANAR
{
  float4 Plane0: SV_TARGET0;
  float4 Plane1: SV_TARGET1;
};

struct PS_OUTPUT_PLANAR
{
  float4 Plane0: SV_TARGET0;
  float4 Plane1: SV_TARGET1;
  float4 Plane2: SV_TARGET2;
};

struct PS_OUTPUT_PLANAR_FULL
{
  float4 Plane0: SV_TARGET0;
  float4 Plane1: SV_TARGET1;
  float4 Plane2: SV_TARGET2;
  float4 Plane3: SV_TARGET3;
};

struct PS_OUTPUT_PACKED
{
  float4 Plane0: SV_TARGET0;
};

float4 DoAlphaPremul (float4 sample)
{
  float4 premul_tex;
  premul_tex.rgb = sample.rgb * sample.a;
  premul_tex.a = sample.a;
  return premul_tex;
}

float4 DoAlphaUnpremul (float4 sample)
{
  float4 unpremul_tex;
  if (sample.a == 0 || sample.a == 1)
    return sample;

  unpremul_tex.rgb = saturate (sample.rgb / sample.a);
  unpremul_tex.a = sample.a;
  return unpremul_tex;
}

interface ISampler
{
  float4 Execute (float2 uv);
};

class SamplerGRAY : ISampler
{
  float4 Execute (float2 uv)
  {
    float4 sample;
    sample.x = shaderTexture[0].Sample(samplerState, uv).x;
    sample.y = 0.5;
    sample.z = 0.5;
    sample.a = 1.0;
    return sample;
  }
};

class SamplerNV12 : ISampler
{
  float4 Execute (float2 uv)
  {
    float4 sample;
    sample.x = shaderTexture[0].Sample(samplerState, uv).x;
    sample.yz = shaderTexture[1].Sample(samplerState, uv).xy;
    sample.a = 1.0;
    return sample;
  }
};

class SamplerNV21 : ISampler
{
  float4 Execute (float2 uv)
  {
    float4 sample;
    sample.x = shaderTexture[0].Sample(samplerState, uv).x;
    sample.yz = shaderTexture[1].Sample(samplerState, uv).yx;
    sample.a = 1.0;
    return sample;
  }
};

class SamplerI420 : ISampler
{
  float4 Execute (float2 uv)
  {
    float4 sample;
    sample.x = shaderTexture[0].Sample(samplerState, uv).x;
    sample.y = shaderTexture[1].Sample(samplerState, uv).x;
    sample.z = shaderTexture[2].Sample(samplerState, uv).x;
    sample.a = 1.0;
    return sample;
  }
};

class SamplerYV12 : ISampler
{
  float4 Execute (float2 uv)
  {
    float4 sample;
    sample.x = shaderTexture[0].Sample(samplerState, uv).x;
    sample.z = shaderTexture[1].Sample(samplerState, uv).x;
    sample.y = shaderTexture[2].Sample(samplerState, uv).x;
    sample.a = 1.0;
    return sample;
  }
};

class SamplerI420_10 : ISampler
{
  float4 Execute (float2 uv)
  {
    float3 sample;
    sample.x = shaderTexture[0].Sample(samplerState, uv).x;
    sample.y = shaderTexture[1].Sample(samplerState, uv).x;
    sample.z = shaderTexture[2].Sample(samplerState, uv).x;
    return float4 (saturate (sample * 64.0), 1.0);
  }
};

class SamplerI420_12 : ISampler
{
  float4 Execute (float2 uv)
  {
    float3 sample;
    sample.x = shaderTexture[0].Sample(samplerState, uv).x;
    sample.y = shaderTexture[1].Sample(samplerState, uv).x;
    sample.z = shaderTexture[2].Sample(samplerState, uv).x;
    return float4 (saturate (sample * 16.0), 1.0);
  }
};

class SamplerVUYA : ISampler
{
  float4 Execute (float2 uv)
  {
    return shaderTexture[0].Sample(samplerState, uv).zyxw;
  }
};

class SamplerVUYAPremul : ISampler
{
  float4 Execute (float2 uv)
  {
    return DoAlphaUnpremul (shaderTexture[0].Sample(samplerState, uv).zyxw);
  }
};

class SamplerY410 : ISampler
{
  float4 Execute (float2 uv)
  {
    return float4 (shaderTexture[0].Sample(samplerState, uv).yxz, 1.0);
  }
};

class SamplerY412 : ISampler
{
  float4 Execute (float2 uv)
  {
    return shaderTexture[0].Sample(samplerState, uv).grba;
  }
};

class SamplerY412Premul : ISampler
{
  float4 Execute (float2 uv)
  {
    return DoAlphaUnpremul (shaderTexture[0].Sample(samplerState, uv).grba);
  }
};

class SamplerAYUV : ISampler
{
  float4 Execute (float2 uv)
  {
    return shaderTexture[0].Sample(samplerState, uv).yzwx;
  }
};

class SamplerAYUVPremul : ISampler
{
  float4 Execute (float2 uv)
  {
    return DoAlphaUnpremul (shaderTexture[0].Sample(samplerState, uv).yzwx);
  }
};

class SamplerRGBA : ISampler
{
  float4 Execute (float2 uv)
  {
    return shaderTexture[0].Sample(samplerState, uv);
  }
};

class SamplerRGBAPremul : ISampler
{
  float4 Execute (float2 uv)
  {
    return DoAlphaUnpremul (shaderTexture[0].Sample(samplerState, uv));
  }
};

class SamplerRGBx : ISampler
{
  float4 Execute (float2 uv)
  {
    return float4 (shaderTexture[0].Sample(samplerState, uv).rgb, 1.0);
  }
};

class SamplerxRGB : ISampler
{
  float4 Execute (float2 uv)
  {
    return float4 (shaderTexture[0].Sample(samplerState, uv).gba, 1.0);
  }
};

class SamplerARGB : ISampler
{
  float4 Execute (float2 uv)
  {
    return shaderTexture[0].Sample(samplerState, uv).gbar;
  }
};

class SamplerARGBPremul : ISampler
{
  float4 Execute (float2 uv)
  {
    return DoAlphaUnpremul (shaderTexture[0].Sample(samplerState, uv).gbar);
  }
};

class SamplerxBGR : ISampler
{
  float4 Execute (float2 uv)
  {
    return float4 (shaderTexture[0].Sample(samplerState, uv).abg, 1.0);
  }
};

class SamplerABGR : ISampler
{
  float4 Execute (float2 uv)
  {
    return shaderTexture[0].Sample(samplerState, uv).abgr;
  }
};

class SamplerABGRPremul : ISampler
{
  float4 Execute (float2 uv)
  {
    return DoAlphaUnpremul (shaderTexture[0].Sample(samplerState, uv).abgr);
  }
};

class SamplerBGR10A2 : ISampler
{
  float4 Execute (float2 uv)
  {
    return float4 (shaderTexture[0].Sample(samplerState, uv).zyx, 1.0);
  }
};

class SamplerBGRA64 : ISampler
{
  float4 Execute (float2 uv)
  {
    return shaderTexture[0].Sample(samplerState, uv).bgra;
  }
};

class SamplerBGRA64Premul : ISampler
{
  float4 Execute (float2 uv)
  {
    return DoAlphaUnpremul (shaderTexture[0].Sample(samplerState, uv).bgra);
  }
};

class SamplerGBR : ISampler
{
  float4 Execute (float2 uv)
  {
    float4 sample;
    sample.g = shaderTexture[0].Sample(samplerState, uv).x;
    sample.b = shaderTexture[1].Sample(samplerState, uv).x;
    sample.r = shaderTexture[2].Sample(samplerState, uv).x;
    sample.a = 1.0;
    return sample;
  }
};

class SamplerGBR_10 : ISampler
{
  float4 Execute (float2 uv)
  {
    float3 sample;
    sample.g = shaderTexture[0].Sample(samplerState, uv).x;
    sample.b = shaderTexture[1].Sample(samplerState, uv).x;
    sample.r = shaderTexture[2].Sample(samplerState, uv).x;
    return float4 (saturate (sample * 64.0), 1.0);
  }
};

class SamplerGBR_12 : ISampler
{
  float4 Execute (float2 uv)
  {
    float3 sample;
    sample.g = shaderTexture[0].Sample(samplerState, uv).x;
    sample.b = shaderTexture[1].Sample(samplerState, uv).x;
    sample.r = shaderTexture[2].Sample(samplerState, uv).x;
    return float4 (saturate (sample * 16.0), 1.0);
  }
};

class SamplerGBRA : ISampler
{
  float4 Execute (float2 uv)
  {
    float4 sample;
    sample.g = shaderTexture[0].Sample(samplerState, uv).x;
    sample.b = shaderTexture[1].Sample(samplerState, uv).x;
    sample.r = shaderTexture[2].Sample(samplerState, uv).x;
    sample.a = shaderTexture[3].Sample(samplerState, uv).x;
    return sample;
  }
};

class SamplerGBRAPremul : ISampler
{
  float4 Execute (float2 uv)
  {
    float4 sample;
    sample.g = shaderTexture[0].Sample(samplerState, uv).x;
    sample.b = shaderTexture[1].Sample(samplerState, uv).x;
    sample.r = shaderTexture[2].Sample(samplerState, uv).x;
    sample.a = shaderTexture[3].Sample(samplerState, uv).x;
    return DoAlphaUnpremul (sample);
  }
};

class SamplerGBRA_10 : ISampler
{
  float4 Execute (float2 uv)
  {
    float4 sample;
    sample.g = shaderTexture[0].Sample(samplerState, uv).x;
    sample.b = shaderTexture[1].Sample(samplerState, uv).x;
    sample.r = shaderTexture[2].Sample(samplerState, uv).x;
    sample.a = shaderTexture[3].Sample(samplerState, uv).x;
    return saturate (sample * 64.0);
  }
};

class SamplerGBRAPremul_10 : ISampler
{
  float4 Execute (float2 uv)
  {
    float4 sample;
    sample.g = shaderTexture[0].Sample(samplerState, uv).x;
    sample.b = shaderTexture[1].Sample(samplerState, uv).x;
    sample.r = shaderTexture[2].Sample(samplerState, uv).x;
    sample.a = shaderTexture[3].Sample(samplerState, uv).x;
    return DoAlphaUnpremul (saturate (sample * 64.0));
  }
};

class SamplerGBRA_12 : ISampler
{
  float4 Execute (float2 uv)
  {
    float4 sample;
    sample.g = shaderTexture[0].Sample(samplerState, uv).x;
    sample.b = shaderTexture[1].Sample(samplerState, uv).x;
    sample.r = shaderTexture[2].Sample(samplerState, uv).x;
    sample.a = shaderTexture[3].Sample(samplerState, uv).x;
    return saturate (sample * 16.0);
  }
};

class SamplerGBRAPremul_12 : ISampler
{
  float4 Execute (float2 uv)
  {
    float4 sample;
    sample.g = shaderTexture[0].Sample(samplerState, uv).x;
    sample.b = shaderTexture[1].Sample(samplerState, uv).x;
    sample.r = shaderTexture[2].Sample(samplerState, uv).x;
    sample.a = shaderTexture[3].Sample(samplerState, uv).x;
    return DoAlphaUnpremul (saturate (sample * 16.0));
  }
};

class SamplerRGBP : ISampler
{
  float4 Execute (float2 uv)
  {
    float4 sample;
    sample.r = shaderTexture[0].Sample(samplerState, uv).x;
    sample.g = shaderTexture[1].Sample(samplerState, uv).x;
    sample.b = shaderTexture[2].Sample(samplerState, uv).x;
    sample.a = 1.0;
    return sample;
  }
};

class SamplerBGRP : ISampler
{
  float4 Execute (float2 uv)
  {
    float4 sample;
    sample.b = shaderTexture[0].Sample(samplerState, uv).x;
    sample.g = shaderTexture[1].Sample(samplerState, uv).x;
    sample.r = shaderTexture[2].Sample(samplerState, uv).x;
    sample.a = 1.0;
    return sample;
  }
};

class SamplerRBGA : ISampler
{
  float4 Execute (float2 uv)
  {
    return shaderTexture[0].Sample(samplerState, uv).rbga;
  }
};

class SamplerRBGAPremul : ISampler
{
  float4 Execute (float2 uv)
  {
    return DoAlphaUnpremul (shaderTexture[0].Sample(samplerState, uv).rbga);
  }
};

interface IConverter
{
  float4 Execute (float4 sample);
};

class ConverterIdentity : IConverter
{
  float4 Execute (float4 sample)
  {
    return sample;
  }
};

class ConverterRange : IConverter
{
  float4 Execute (float4 sample)
  {
    float3 out_space;
    out_space.x = postCoeff.CoeffX.x * sample.x;
    out_space.y = postCoeff.CoeffY.y * sample.y;
    out_space.z = postCoeff.CoeffZ.z * sample.z;
    out_space += postCoeff.Offset;
    return float4 (clamp (out_space, postCoeff.Min, postCoeff.Max), sample.a);
  }
};

class ConverterSimple : IConverter
{
  float4 Execute (float4 sample)
  {
    float3 out_space;
    out_space.x = dot (postCoeff.CoeffX, sample.xyz);
    out_space.y = dot (postCoeff.CoeffY, sample.xyz);
    out_space.z = dot (postCoeff.CoeffZ, sample.xyz);
    out_space += postCoeff.Offset;
    return float4 (clamp (out_space, postCoeff.Min, postCoeff.Max), sample.a);
  }
};

class ConverterGamma : IConverter
{
  float4 Execute (float4 sample)
  {
    float3 rgb_space;
    float3 out_space;
    rgb_space.x = dot (preCoeff.CoeffX, sample.xyz);
    rgb_space.y = dot (preCoeff.CoeffY, sample.xyz);
    rgb_space.z = dot (preCoeff.CoeffZ, sample.xyz);
    rgb_space += preCoeff.Offset;
    rgb_space = clamp (rgb_space, preCoeff.Min, preCoeff.Max);

    rgb_space.x = gammaDecLUT.Sample (linearSampler, rgb_space.x);
    rgb_space.y = gammaDecLUT.Sample (linearSampler, rgb_space.y);
    rgb_space.z = gammaDecLUT.Sample (linearSampler, rgb_space.z);

    rgb_space.x = gammaEncLUT.Sample (linearSampler, rgb_space.x);
    rgb_space.y = gammaEncLUT.Sample (linearSampler, rgb_space.y);
    rgb_space.z = gammaEncLUT.Sample (linearSampler, rgb_space.z);

    out_space.x = dot (postCoeff.CoeffX, rgb_space);
    out_space.y = dot (postCoeff.CoeffY, rgb_space);
    out_space.z = dot (postCoeff.CoeffZ, rgb_space);
    out_space += postCoeff.Offset;
    return float4 (clamp (out_space, postCoeff.Min, postCoeff.Max), sample.a);
  }
};

class ConverterPrimary : IConverter
{
  float4 Execute (float4 sample)
  {
    float3 rgb_space;
    float3 primary_converted;
    float3 out_space;

    rgb_space.x = dot (preCoeff.CoeffX, sample.xyz);
    rgb_space.y = dot (preCoeff.CoeffY, sample.xyz);
    rgb_space.z = dot (preCoeff.CoeffZ, sample.xyz);
    rgb_space += preCoeff.Offset;
    rgb_space = clamp (rgb_space, preCoeff.Min, preCoeff.Max);

    rgb_space.x = gammaDecLUT.Sample (linearSampler, rgb_space.x);
    rgb_space.y = gammaDecLUT.Sample (linearSampler, rgb_space.y);
    rgb_space.z = gammaDecLUT.Sample (linearSampler, rgb_space.z);

    primary_converted.x = dot (primariesCoeff.CoeffX, rgb_space);
    primary_converted.y = dot (primariesCoeff.CoeffY, rgb_space);
    primary_converted.z = dot (primariesCoeff.CoeffZ, rgb_space);

    rgb_space.x = gammaEncLUT.Sample (linearSampler, primary_converted.x);
    rgb_space.y = gammaEncLUT.Sample (linearSampler, primary_converted.y);
    rgb_space.z = gammaEncLUT.Sample (linearSampler, primary_converted.z);

    out_space.x = dot (postCoeff.CoeffX, rgb_space);
    out_space.y = dot (postCoeff.CoeffY, rgb_space);
    out_space.z = dot (postCoeff.CoeffZ, rgb_space);
    out_space += postCoeff.Offset;
    return float4 (clamp (out_space, postCoeff.Min, postCoeff.Max), sample.a);
  }
};

float UnormTo10bit (float sample)
{
  return sample * 1023.0 / 65535.0;
}

float2 UnormTo10bit (float2 sample)
{
  return sample * 1023.0 / 65535.0;
}

float3 UnormTo10bit (float3 sample)
{
  return sample * 1023.0 / 65535.0;
}

float4 UnormTo10bit (float4 sample)
{
  return sample * 1023.0 / 65535.0;
}

float UnormTo12bit (float sample)
{
  return sample * 4095.0 / 65535.0;
}

float2 UnormTo12bit (float2 sample)
{
  return sample * 4095.0 / 65535.0;
}

float3 UnormTo12bit (float3 sample)
{
  return sample * 4095.0 / 65535.0;
}

float4 UnormTo12bit (float4 sample)
{
  return sample * 4095.0 / 65535.0;
}

interface IOutputLuma
{
  PS_OUTPUT_LUMA Build (float4 sample);
};

class OutputLuma : IOutputLuma
{
  PS_OUTPUT_LUMA Build (float4 sample)
  {
    PS_OUTPUT_LUMA output;
    output.Plane0 = float4 (sample.x, 0, 0, 0);
    return output;
  }
};

class OutputLuma_10 : IOutputLuma
{
  PS_OUTPUT_LUMA Build (float4 sample)
  {
    PS_OUTPUT_LUMA output;
    output.Plane0 = float4 (UnormTo10bit (sample.x), 0, 0, 0);
    return output;
  }
};

class OutputLuma_12 : IOutputLuma
{
  PS_OUTPUT_LUMA Build (float4 sample)
  {
    PS_OUTPUT_LUMA output;
    output.Plane0 = float4 (UnormTo12bit (sample.x), 0, 0, 0);
    return output;
  }
};

interface IOutputChroma
{
  PS_OUTPUT_CHROMA Build (float4 sample);
};

class OutputChromaNV12 : IOutputChroma
{
  PS_OUTPUT_CHROMA Build (float4 sample)
  {
    PS_OUTPUT_CHROMA output;
    output.Plane0 = float4 (sample.yz, 0, 0);
    return output;
  }
};

class OutputChromaNV21 : IOutputChroma
{
  PS_OUTPUT_CHROMA Build (float4 sample)
  {
    PS_OUTPUT_CHROMA output;
    output.Plane0 = float4 (sample.zy, 0, 0);
    return output;
  }
};

interface IOutputChromaPlanar
{ PS_OUTPUT_CHROMA_PLANAR Build (float4 sample);
};

class OutputChromaI420 : IOutputChromaPlanar
{
  PS_OUTPUT_CHROMA_PLANAR Build (float4 sample)
  {
    PS_OUTPUT_CHROMA_PLANAR output;
    output.Plane0 = float4 (sample.y, 0, 0, 0);
    output.Plane1 = float4 (sample.z, 0, 0, 0);
    return output;
  }
};

class OutputChromaYV12 : IOutputChromaPlanar
{
  PS_OUTPUT_CHROMA_PLANAR Build (float4 sample)
  {
    PS_OUTPUT_CHROMA_PLANAR output;
    output.Plane0 = float4 (sample.z, 0, 0, 0);
    output.Plane1 = float4 (sample.y, 0, 0, 0);
    return output;
  }
};

class OutputChromaI420_10 : IOutputChromaPlanar
{
  PS_OUTPUT_CHROMA_PLANAR Build (float4 sample)
  {
    PS_OUTPUT_CHROMA_PLANAR output;
    float2 scaled = UnormTo10bit (sample.yz);
    output.Plane0 = float4 (scaled.x, 0, 0, 0);
    output.Plane1 = float4 (scaled.y, 0, 0, 0);
    return output;
  }
};

class OutputChromaI420_12 : IOutputChromaPlanar
{
  PS_OUTPUT_CHROMA_PLANAR Build (float4 sample)
  {
    PS_OUTPUT_CHROMA_PLANAR output;
    float2 scaled = UnormTo12bit (sample.yz);
    output.Plane0 = float4 (scaled.x, 0, 0, 0);
    output.Plane1 = float4 (scaled.y, 0, 0, 0);
    return output;
  }
};

interface IOutputPlanar
{
  PS_OUTPUT_PLANAR Build (float4 sample);
};

class OutputY444 : IOutputPlanar
{
  PS_OUTPUT_PLANAR Build (float4 sample)
  {
    PS_OUTPUT_PLANAR output;
    output.Plane0 = float4 (sample.x, 0, 0, 0);
    output.Plane1 = float4 (sample.y, 0, 0, 0);
    output.Plane2 = float4 (sample.z, 0, 0, 0);
    return output;
  }
};

class OutputY444_10 : IOutputPlanar
{
  PS_OUTPUT_PLANAR Build (float4 sample)
  {
    PS_OUTPUT_PLANAR output;
    float3 scaled = UnormTo10bit (sample.xyz);
    output.Plane0 = float4 (scaled.x, 0, 0, 0);
    output.Plane1 = float4 (scaled.y, 0, 0, 0);
    output.Plane2 = float4 (scaled.z, 0, 0, 0);
    return output;
  }
};

class OutputY444_12 : IOutputPlanar
{
  PS_OUTPUT_PLANAR Build (float4 sample)
  {
    PS_OUTPUT_PLANAR output;
    float3 scaled = UnormTo12bit (sample.xyz);
    output.Plane0 = float4 (scaled.x, 0, 0, 0);
    output.Plane1 = float4 (scaled.y, 0, 0, 0);
    output.Plane2 = float4 (scaled.z, 0, 0, 0);
    return output;
  }
};

class OutputGBR : IOutputPlanar
{
  PS_OUTPUT_PLANAR Build (float4 sample)
  {
    PS_OUTPUT_PLANAR output;
    output.Plane0 = float4 (sample.g, 0, 0, 0);
    output.Plane1 = float4 (sample.b, 0, 0, 0);
    output.Plane2 = float4 (sample.r, 0, 0, 0);
    return output;
  }
};

class OutputGBR_10 : IOutputPlanar
{
  PS_OUTPUT_PLANAR Build (float4 sample)
  {
    PS_OUTPUT_PLANAR output;
    float3 scaled = UnormTo10bit (sample.rgb);
    output.Plane0 = float4 (scaled.g, 0, 0, 0);
    output.Plane1 = float4 (scaled.b, 0, 0, 0);
    output.Plane2 = float4 (scaled.r, 0, 0, 0);
    return output;
  }
};

class OutputGBR_12 : IOutputPlanar
{
  PS_OUTPUT_PLANAR Build (float4 sample)
  {
    PS_OUTPUT_PLANAR output;
    float3 scaled = UnormTo12bit (sample.rgb);
    output.Plane0 = float4 (scaled.g, 0, 0, 0);
    output.Plane1 = float4 (scaled.b, 0, 0, 0);
    output.Plane2 = float4 (scaled.r, 0, 0, 0);
    return output;
  }
};

class OutputRGBP : IOutputPlanar
{
  PS_OUTPUT_PLANAR Build (float4 sample)
  {
    PS_OUTPUT_PLANAR output;
    output.Plane0 = float4 (sample.r, 0, 0, 0);
    output.Plane1 = float4 (sample.g, 0, 0, 0);
    output.Plane2 = float4 (sample.b, 0, 0, 0);
    return output;
  }
};

class OutputBGRP : IOutputPlanar
{
  PS_OUTPUT_PLANAR Build (float4 sample)
  {
    PS_OUTPUT_PLANAR output;
    output.Plane0 = float4 (sample.b, 0, 0, 0);
    output.Plane1 = float4 (sample.g, 0, 0, 0);
    output.Plane2 = float4 (sample.r, 0, 0, 0);
    return output;
  }
};

interface IOutputPlanarFull
{
  PS_OUTPUT_PLANAR_FULL Build (float4 sample);
};

class OutputGBRA : IOutputPlanarFull
{
  PS_OUTPUT_PLANAR_FULL Build (float4 sample)
  {
    PS_OUTPUT_PLANAR_FULL output;
    output.Plane0 = float4 (sample.g, 0, 0, 0);
    output.Plane1 = float4 (sample.b, 0, 0, 0);
    output.Plane2 = float4 (sample.r, 0, 0, 0);
    output.Plane3 = float4 (sample.a * alphaFactor, 0, 0, 0);
    return output;
  }
};

class OutputGBRAPremul : IOutputPlanarFull
{
  PS_OUTPUT_PLANAR_FULL Build (float4 sample)
  {
    PS_OUTPUT_PLANAR_FULL output;
    float4 premul;
    sample.a *= alphaFactor;
    premul = DoAlphaPremul (sample);
    output.Plane0 = float4 (premul.g, 0, 0, 0);
    output.Plane1 = float4 (premul.b, 0, 0, 0);
    output.Plane2 = float4 (premul.r, 0, 0, 0);
    output.Plane3 = float4 (premul.a, 0, 0, 0);
    return output;
  }
};

class OutputGBRA_10 : IOutputPlanarFull
{
  PS_OUTPUT_PLANAR_FULL Build (float4 sample)
  {
    PS_OUTPUT_PLANAR_FULL output;
    float4 scaled;
    sample.a *= alphaFactor;
    scaled = UnormTo10bit (sample);
    output.Plane0 = float4 (scaled.g, 0, 0, 0);
    output.Plane1 = float4 (scaled.b, 0, 0, 0);
    output.Plane2 = float4 (scaled.r, 0, 0, 0);
    output.Plane3 = float4 (scaled.a, 0, 0, 0);
    return output;
  }
};

class OutputGBRAPremul_10 : IOutputPlanarFull
{
  PS_OUTPUT_PLANAR_FULL Build (float4 sample)
  {
    PS_OUTPUT_PLANAR_FULL output;
    float4 scaled;
    sample.a *= alphaFactor;
    scaled = UnormTo10bit (DoAlphaPremul (sample));
    output.Plane0 = float4 (scaled.g, 0, 0, 0);
    output.Plane1 = float4 (scaled.b, 0, 0, 0);
    output.Plane2 = float4 (scaled.r, 0, 0, 0);
    output.Plane3 = float4 (scaled.a, 0, 0, 0);
    return output;
  }
};

class OutputGBRA_12 : IOutputPlanarFull
{
  PS_OUTPUT_PLANAR_FULL Build (float4 sample)
  {
    PS_OUTPUT_PLANAR_FULL output;
    float4 scaled;
    sample.a *= alphaFactor;
    scaled = UnormTo12bit (sample);
    output.Plane0 = float4 (scaled.g, 0, 0, 0);
    output.Plane1 = float4 (scaled.b, 0, 0, 0);
    output.Plane2 = float4 (scaled.r, 0, 0, 0);
    output.Plane3 = float4 (scaled.a, 0, 0, 0);
    return output;
  }
};

class OutputGBRAPremul_12 : IOutputPlanarFull
{
  PS_OUTPUT_PLANAR_FULL Build (float4 sample)
  {
    PS_OUTPUT_PLANAR_FULL output;
    float4 scaled;
    sample.a *= alphaFactor;
    scaled = UnormTo12bit (DoAlphaPremul (sample));
    output.Plane0 = float4 (scaled.g, 0, 0, 0);
    output.Plane1 = float4 (scaled.b, 0, 0, 0);
    output.Plane2 = float4 (scaled.r, 0, 0, 0);
    output.Plane3 = float4 (scaled.a, 0, 0, 0);
    return output;
  }
};

interface IOutputPacked
{
  PS_OUTPUT_PACKED Build (float4 sample);
};

class OutputRGBA : IOutputPacked
{
  PS_OUTPUT_PACKED Build (float4 sample)
  {
    PS_OUTPUT_PACKED output;
    output.Plane0 = float4 (sample.rgb, sample.a * alphaFactor);
    return output;
  }
};

class OutputRGBAPremul : IOutputPacked
{
  PS_OUTPUT_PACKED Build (float4 sample)
  {
    PS_OUTPUT_PACKED output;
    sample.a *= alphaFactor;
    output.Plane0 = DoAlphaPremul (sample);
    return output;
  }
};

class OutputRGBx : IOutputPacked
{
  PS_OUTPUT_PACKED Build (float4 sample)
  {
    PS_OUTPUT_PACKED output;
    output.Plane0 = float4 (sample.rgb, 1.0);
    return output;
  }
};

class OutputxRGB : IOutputPacked
{
  PS_OUTPUT_PACKED Build (float4 sample)
  {
    PS_OUTPUT_PACKED output;
    output.Plane0 = float4 (0.0, sample.rgb);
    return output;
  }
};

class OutputARGB : IOutputPacked
{
  PS_OUTPUT_PACKED Build (float4 sample)
  {
    PS_OUTPUT_PACKED output;
    output.Plane0 = sample.argb;
    return output;
  }
};

class OutputARGBPremul : IOutputPacked
{
  PS_OUTPUT_PACKED Build (float4 sample)
  {
    PS_OUTPUT_PACKED output;
    output.Plane0 = DoAlphaPremul (sample).argb;
    return output;
  }
};

class OutputxBGR : IOutputPacked
{
  PS_OUTPUT_PACKED Build (float4 sample)
  {
    PS_OUTPUT_PACKED output;
    output.Plane0 = float4 (0.0, sample.bgr);
    return output;
  }
};

class OutputABGR : IOutputPacked
{
  PS_OUTPUT_PACKED Build (float4 sample)
  {
    PS_OUTPUT_PACKED output;
    output.Plane0 = sample.abgr;
    return output;
  }
};

class OutputABGRPremul : IOutputPacked
{
  PS_OUTPUT_PACKED Build (float4 sample)
  {
    PS_OUTPUT_PACKED output;
    output.Plane0 = DoAlphaPremul (sample.abgr);
    return output;
  }
};

class OutputVUYA : IOutputPacked
{
  PS_OUTPUT_PACKED Build (float4 sample)
  {
    PS_OUTPUT_PACKED output;
    sample.a *= alphaFactor;
    output.Plane0 = sample.zyxw;
    return output;
  }
};

class OutputVUYAPremul : IOutputPacked
{
  PS_OUTPUT_PACKED Build (float4 sample)
  {
    PS_OUTPUT_PACKED output;
    sample.a *= alphaFactor;
    output.Plane0 = DoAlphaPremul (sample).zyxw;
    return output;
  }
};

class OutputAYUV : IOutputPacked
{
  PS_OUTPUT_PACKED Build (float4 sample)
  {
    PS_OUTPUT_PACKED output;
    sample.a *= alphaFactor;
    output.Plane0 = sample.wxyz;
    return output;
  }
};

class OutputAYUVPremul : IOutputPacked
{
  PS_OUTPUT_PACKED Build (float4 sample)
  {
    PS_OUTPUT_PACKED output;
    sample.a *= alphaFactor;
    output.Plane0 = DoAlphaPremul (sample).wxyz;
    return output;
  }
};

class OutputRBGA : IOutputPacked
{
  PS_OUTPUT_PACKED Build (float4 sample)
  {
    PS_OUTPUT_PACKED output;
    sample.a *= alphaFactor;
    output.Plane0 = sample.rbga;
    return output;
  }
};

class OutputRBGAPremul : IOutputPacked
{
  PS_OUTPUT_PACKED Build (float4 sample)
  {
    PS_OUTPUT_PACKED output;
    sample.a *= alphaFactor;
    output.Plane0 = DoAlphaPremul (sample).rbga;
    return output;
  }
};

OUTPUT_TYPE ENTRY_POINT (PS_INPUT input)
{
  SAMPLER g_sampler;
  CONVERTER g_converter;
  OUTPUT_BUILDER g_builder;
  return g_builder.Build (g_converter.Execute (g_sampler.Execute (input.Texture)));
}
#else /* BUILDING_HLSL */
static const char g_PSMain_converter_str[] =
"struct PSColorSpace\n"
"{\n"
"  float3 CoeffX;\n"
"  float3 CoeffY;\n"
"  float3 CoeffZ;\n"
"  float3 Offset;\n"
"  float3 Min;\n"
"  float3 Max;\n"
"  float padding;\n"
"};\n"
"\n"
"cbuffer PsConstBuffer : register(b0)\n"
"{\n"
"  PSColorSpace preCoeff;\n"
"  PSColorSpace postCoeff;\n"
"  PSColorSpace primariesCoeff;\n"
"  float alphaFactor;\n"
"};\n"
"\n"
"Texture2D shaderTexture[4] : register(t0);\n"
"Texture1D<float> gammaDecLUT : register(t4);\n"
"Texture1D<float> gammaEncLUT: register(t5);\n"
"SamplerState samplerState : register(s0);\n"
"SamplerState linearSampler : register(s1);\n"
"\n"
"struct PS_INPUT\n"
"{\n"
"  float4 Position: SV_POSITION;\n"
"  float2 Texture: TEXCOORD;\n"
"};\n"
"\n"
"struct PS_OUTPUT_LUMA\n"
"{\n"
"  float4 Plane0: SV_TARGET0;\n"
"};\n"
"\n"
"struct PS_OUTPUT_CHROMA\n"
"{\n"
"  float4 Plane0: SV_TARGET0;\n"
"};\n"
"\n"
"struct PS_OUTPUT_CHROMA_PLANAR\n"
"{\n"
"  float4 Plane0: SV_TARGET0;\n"
"  float4 Plane1: SV_TARGET1;\n"
"};\n"
"\n"
"struct PS_OUTPUT_PLANAR\n"
"{\n"
"  float4 Plane0: SV_TARGET0;\n"
"  float4 Plane1: SV_TARGET1;\n"
"  float4 Plane2: SV_TARGET2;\n"
"};\n"
"\n"
"struct PS_OUTPUT_PLANAR_FULL\n"
"{\n"
"  float4 Plane0: SV_TARGET0;\n"
"  float4 Plane1: SV_TARGET1;\n"
"  float4 Plane2: SV_TARGET2;\n"
"  float4 Plane3: SV_TARGET3;\n"
"};\n"
"\n"
"struct PS_OUTPUT_PACKED\n"
"{\n"
"  float4 Plane0: SV_TARGET0;\n"
"};\n"
"\n"
"float4 DoAlphaPremul (float4 sample)\n"
"{\n"
"  float4 premul_tex;\n"
"  premul_tex.rgb = sample.rgb * sample.a;\n"
"  premul_tex.a = sample.a;\n"
"  return premul_tex;\n"
"}\n"
"\n"
"float4 DoAlphaUnpremul (float4 sample)\n"
"{\n"
"  float4 unpremul_tex;\n"
"  if (sample.a == 0 || sample.a == 1)\n"
"    return sample;\n"
"\n"
"  unpremul_tex.rgb = saturate (sample.rgb / sample.a);\n"
"  unpremul_tex.a = sample.a;\n"
"  return unpremul_tex;\n"
"}\n"
"\n"
"interface ISampler\n"
"{\n"
"  float4 Execute (float2 uv);\n"
"};\n"
"\n"
"class SamplerGRAY : ISampler\n"
"{\n"
"  float4 Execute (float2 uv)\n"
"  {\n"
"    float4 sample;\n"
"    sample.x = shaderTexture[0].Sample(samplerState, uv).x;\n"
"    sample.y = 0.5;\n"
"    sample.z = 0.5;\n"
"    sample.a = 1.0;\n"
"    return sample;\n"
"  }\n"
"};\n"
"\n"
"class SamplerNV12 : ISampler\n"
"{\n"
"  float4 Execute (float2 uv)\n"
"  {\n"
"    float4 sample;\n"
"    sample.x = shaderTexture[0].Sample(samplerState, uv).x;\n"
"    sample.yz = shaderTexture[1].Sample(samplerState, uv).xy;\n"
"    sample.a = 1.0;\n"
"    return sample;\n"
"  }\n"
"};\n"
"\n"
"class SamplerNV21 : ISampler\n"
"{\n"
"  float4 Execute (float2 uv)\n"
"  {\n"
"    float4 sample;\n"
"    sample.x = shaderTexture[0].Sample(samplerState, uv).x;\n"
"    sample.yz = shaderTexture[1].Sample(samplerState, uv).yx;\n"
"    sample.a = 1.0;\n"
"    return sample;\n"
"  }\n"
"};\n"
"\n"
"class SamplerI420 : ISampler\n"
"{\n"
"  float4 Execute (float2 uv)\n"
"  {\n"
"    float4 sample;\n"
"    sample.x = shaderTexture[0].Sample(samplerState, uv).x;\n"
"    sample.y = shaderTexture[1].Sample(samplerState, uv).x;\n"
"    sample.z = shaderTexture[2].Sample(samplerState, uv).x;\n"
"    sample.a = 1.0;\n"
"    return sample;\n"
"  }\n"
"};\n"
"\n"
"class SamplerYV12 : ISampler\n"
"{\n"
"  float4 Execute (float2 uv)\n"
"  {\n"
"    float4 sample;\n"
"    sample.x = shaderTexture[0].Sample(samplerState, uv).x;\n"
"    sample.z = shaderTexture[1].Sample(samplerState, uv).x;\n"
"    sample.y = shaderTexture[2].Sample(samplerState, uv).x;\n"
"    sample.a = 1.0;\n"
"    return sample;\n"
"  }\n"
"};\n"
"\n"
"class SamplerI420_10 : ISampler\n"
"{\n"
"  float4 Execute (float2 uv)\n"
"  {\n"
"    float3 sample;\n"
"    sample.x = shaderTexture[0].Sample(samplerState, uv).x;\n"
"    sample.y = shaderTexture[1].Sample(samplerState, uv).x;\n"
"    sample.z = shaderTexture[2].Sample(samplerState, uv).x;\n"
"    return float4 (saturate (sample * 64.0), 1.0);\n"
"  }\n"
"};\n"
"\n"
"class SamplerI420_12 : ISampler\n"
"{\n"
"  float4 Execute (float2 uv)\n"
"  {\n"
"    float3 sample;\n"
"    sample.x = shaderTexture[0].Sample(samplerState, uv).x;\n"
"    sample.y = shaderTexture[1].Sample(samplerState, uv).x;\n"
"    sample.z = shaderTexture[2].Sample(samplerState, uv).x;\n"
"    return float4 (saturate (sample * 16.0), 1.0);\n"
"  }\n"
"};\n"
"\n"
"class SamplerVUYA : ISampler\n"
"{\n"
"  float4 Execute (float2 uv)\n"
"  {\n"
"    return shaderTexture[0].Sample(samplerState, uv).zyxw;\n"
"  }\n"
"};\n"
"\n"
"class SamplerVUYAPremul : ISampler\n"
"{\n"
"  float4 Execute (float2 uv)\n"
"  {\n"
"    return DoAlphaUnpremul (shaderTexture[0].Sample(samplerState, uv).zyxw);\n"
"  }\n"
"};\n"
"\n"
"class SamplerY410 : ISampler\n"
"{\n"
"  float4 Execute (float2 uv)\n"
"  {\n"
"    return float4 (shaderTexture[0].Sample(samplerState, uv).yxz, 1.0);\n"
"  }\n"
"};\n"
"\n"
"class SamplerY412 : ISampler\n"
"{\n"
"  float4 Execute (float2 uv)\n"
"  {\n"
"    return shaderTexture[0].Sample(samplerState, uv).grba;\n"
"  }\n"
"};\n"
"\n"
"class SamplerY412Premul : ISampler\n"
"{\n"
"  float4 Execute (float2 uv)\n"
"  {\n"
"    return DoAlphaUnpremul (shaderTexture[0].Sample(samplerState, uv).grba);\n"
"  }\n"
"};\n"
"\n"
"class SamplerAYUV : ISampler\n"
"{\n"
"  float4 Execute (float2 uv)\n"
"  {\n"
"    return shaderTexture[0].Sample(samplerState, uv).yzwx;\n"
"  }\n"
"};\n"
"\n"
"class SamplerAYUVPremul : ISampler\n"
"{\n"
"  float4 Execute (float2 uv)\n"
"  {\n"
"    return DoAlphaUnpremul (shaderTexture[0].Sample(samplerState, uv).yzwx);\n"
"  }\n"
"};\n"
"\n"
"class SamplerRGBA : ISampler\n"
"{\n"
"  float4 Execute (float2 uv)\n"
"  {\n"
"    return shaderTexture[0].Sample(samplerState, uv);\n"
"  }\n"
"};\n"
"\n"
"class SamplerRGBAPremul : ISampler\n"
"{\n"
"  float4 Execute (float2 uv)\n"
"  {\n"
"    return DoAlphaUnpremul (shaderTexture[0].Sample(samplerState, uv));\n"
"  }\n"
"};\n"
"\n"
"class SamplerRGBx : ISampler\n"
"{\n"
"  float4 Execute (float2 uv)\n"
"  {\n"
"    return float4 (shaderTexture[0].Sample(samplerState, uv).rgb, 1.0);\n"
"  }\n"
"};\n"
"\n"
"class SamplerxRGB : ISampler\n"
"{\n"
"  float4 Execute (float2 uv)\n"
"  {\n"
"    return float4 (shaderTexture[0].Sample(samplerState, uv).gba, 1.0);\n"
"  }\n"
"};\n"
"\n"
"class SamplerARGB : ISampler\n"
"{\n"
"  float4 Execute (float2 uv)\n"
"  {\n"
"    return shaderTexture[0].Sample(samplerState, uv).gbar;\n"
"  }\n"
"};\n"
"\n"
"class SamplerARGBPremul : ISampler\n"
"{\n"
"  float4 Execute (float2 uv)\n"
"  {\n"
"    return DoAlphaUnpremul (shaderTexture[0].Sample(samplerState, uv).gbar);\n"
"  }\n"
"};\n"
"\n"
"class SamplerxBGR : ISampler\n"
"{\n"
"  float4 Execute (float2 uv)\n"
"  {\n"
"    return float4 (shaderTexture[0].Sample(samplerState, uv).abg, 1.0);\n"
"  }\n"
"};\n"
"\n"
"class SamplerABGR : ISampler\n"
"{\n"
"  float4 Execute (float2 uv)\n"
"  {\n"
"    return shaderTexture[0].Sample(samplerState, uv).abgr;\n"
"  }\n"
"};\n"
"\n"
"class SamplerABGRPremul : ISampler\n"
"{\n"
"  float4 Execute (float2 uv)\n"
"  {\n"
"    return DoAlphaUnpremul (shaderTexture[0].Sample(samplerState, uv).abgr);\n"
"  }\n"
"};\n"
"\n"
"class SamplerBGR10A2 : ISampler\n"
"{\n"
"  float4 Execute (float2 uv)\n"
"  {\n"
"    return float4 (shaderTexture[0].Sample(samplerState, uv).zyx, 1.0);\n"
"  }\n"
"};\n"
"\n"
"class SamplerBGRA64 : ISampler\n"
"{\n"
"  float4 Execute (float2 uv)\n"
"  {\n"
"    return shaderTexture[0].Sample(samplerState, uv).bgra;\n"
"  }\n"
"};\n"
"\n"
"class SamplerBGRA64Premul : ISampler\n"
"{\n"
"  float4 Execute (float2 uv)\n"
"  {\n"
"    return DoAlphaUnpremul (shaderTexture[0].Sample(samplerState, uv).bgra);\n"
"  }\n"
"};\n"
"\n"
"class SamplerGBR : ISampler\n"
"{\n"
"  float4 Execute (float2 uv)\n"
"  {\n"
"    float4 sample;\n"
"    sample.g = shaderTexture[0].Sample(samplerState, uv).x;\n"
"    sample.b = shaderTexture[1].Sample(samplerState, uv).x;\n"
"    sample.r = shaderTexture[2].Sample(samplerState, uv).x;\n"
"    sample.a = 1.0;\n"
"    return sample;\n"
"  }\n"
"};\n"
"\n"
"class SamplerGBR_10 : ISampler\n"
"{\n"
"  float4 Execute (float2 uv)\n"
"  {\n"
"    float3 sample;\n"
"    sample.g = shaderTexture[0].Sample(samplerState, uv).x;\n"
"    sample.b = shaderTexture[1].Sample(samplerState, uv).x;\n"
"    sample.r = shaderTexture[2].Sample(samplerState, uv).x;\n"
"    return float4 (saturate (sample * 64.0), 1.0);\n"
"  }\n"
"};\n"
"\n"
"class SamplerGBR_12 : ISampler\n"
"{\n"
"  float4 Execute (float2 uv)\n"
"  {\n"
"    float3 sample;\n"
"    sample.g = shaderTexture[0].Sample(samplerState, uv).x;\n"
"    sample.b = shaderTexture[1].Sample(samplerState, uv).x;\n"
"    sample.r = shaderTexture[2].Sample(samplerState, uv).x;\n"
"    return float4 (saturate (sample * 16.0), 1.0);\n"
"  }\n"
"};\n"
"\n"
"class SamplerGBRA : ISampler\n"
"{\n"
"  float4 Execute (float2 uv)\n"
"  {\n"
"    float4 sample;\n"
"    sample.g = shaderTexture[0].Sample(samplerState, uv).x;\n"
"    sample.b = shaderTexture[1].Sample(samplerState, uv).x;\n"
"    sample.r = shaderTexture[2].Sample(samplerState, uv).x;\n"
"    sample.a = shaderTexture[3].Sample(samplerState, uv).x;\n"
"    return sample;\n"
"  }\n"
"};\n"
"\n"
"class SamplerGBRAPremul : ISampler\n"
"{\n"
"  float4 Execute (float2 uv)\n"
"  {\n"
"    float4 sample;\n"
"    sample.g = shaderTexture[0].Sample(samplerState, uv).x;\n"
"    sample.b = shaderTexture[1].Sample(samplerState, uv).x;\n"
"    sample.r = shaderTexture[2].Sample(samplerState, uv).x;\n"
"    sample.a = shaderTexture[3].Sample(samplerState, uv).x;\n"
"    return DoAlphaUnpremul (sample);\n"
"  }\n"
"};\n"
"\n"
"class SamplerGBRA_10 : ISampler\n"
"{\n"
"  float4 Execute (float2 uv)\n"
"  {\n"
"    float4 sample;\n"
"    sample.g = shaderTexture[0].Sample(samplerState, uv).x;\n"
"    sample.b = shaderTexture[1].Sample(samplerState, uv).x;\n"
"    sample.r = shaderTexture[2].Sample(samplerState, uv).x;\n"
"    sample.a = shaderTexture[3].Sample(samplerState, uv).x;\n"
"    return saturate (sample * 64.0);\n"
"  }\n"
"};\n"
"\n"
"class SamplerGBRAPremul_10 : ISampler\n"
"{\n"
"  float4 Execute (float2 uv)\n"
"  {\n"
"    float4 sample;\n"
"    sample.g = shaderTexture[0].Sample(samplerState, uv).x;\n"
"    sample.b = shaderTexture[1].Sample(samplerState, uv).x;\n"
"    sample.r = shaderTexture[2].Sample(samplerState, uv).x;\n"
"    sample.a = shaderTexture[3].Sample(samplerState, uv).x;\n"
"    return DoAlphaUnpremul (saturate (sample * 64.0));\n"
"  }\n"
"};\n"
"\n"
"class SamplerGBRA_12 : ISampler\n"
"{\n"
"  float4 Execute (float2 uv)\n"
"  {\n"
"    float4 sample;\n"
"    sample.g = shaderTexture[0].Sample(samplerState, uv).x;\n"
"    sample.b = shaderTexture[1].Sample(samplerState, uv).x;\n"
"    sample.r = shaderTexture[2].Sample(samplerState, uv).x;\n"
"    sample.a = shaderTexture[3].Sample(samplerState, uv).x;\n"
"    return saturate (sample * 16.0);\n"
"  }\n"
"};\n"
"\n"
"class SamplerGBRAPremul_12 : ISampler\n"
"{\n"
"  float4 Execute (float2 uv)\n"
"  {\n"
"    float4 sample;\n"
"    sample.g = shaderTexture[0].Sample(samplerState, uv).x;\n"
"    sample.b = shaderTexture[1].Sample(samplerState, uv).x;\n"
"    sample.r = shaderTexture[2].Sample(samplerState, uv).x;\n"
"    sample.a = shaderTexture[3].Sample(samplerState, uv).x;\n"
"    return DoAlphaUnpremul (saturate (sample * 16.0));\n"
"  }\n"
"};\n"
"\n"
"class SamplerRGBP : ISampler\n"
"{\n"
"  float4 Execute (float2 uv)\n"
"  {\n"
"    float4 sample;\n"
"    sample.r = shaderTexture[0].Sample(samplerState, uv).x;\n"
"    sample.g = shaderTexture[1].Sample(samplerState, uv).x;\n"
"    sample.b = shaderTexture[2].Sample(samplerState, uv).x;\n"
"    sample.a = 1.0;\n"
"    return sample;\n"
"  }\n"
"};\n"
"\n"
"class SamplerBGRP : ISampler\n"
"{\n"
"  float4 Execute (float2 uv)\n"
"  {\n"
"    float4 sample;\n"
"    sample.b = shaderTexture[0].Sample(samplerState, uv).x;\n"
"    sample.g = shaderTexture[1].Sample(samplerState, uv).x;\n"
"    sample.r = shaderTexture[2].Sample(samplerState, uv).x;\n"
"    sample.a = 1.0;\n"
"    return sample;\n"
"  }\n"
"};\n"
"\n"
"class SamplerRBGA : ISampler\n"
"{\n"
"  float4 Execute (float2 uv)\n"
"  {\n"
"    return shaderTexture[0].Sample(samplerState, uv).rbga;\n"
"  }\n"
"};\n"
"\n"
"class SamplerRBGAPremul : ISampler\n"
"{\n"
"  float4 Execute (float2 uv)\n"
"  {\n"
"    return DoAlphaUnpremul (shaderTexture[0].Sample(samplerState, uv).rbga);\n"
"  }\n"
"};\n"
"\n"
"interface IConverter\n"
"{\n"
"  float4 Execute (float4 sample);\n"
"};\n"
"\n"
"class ConverterIdentity : IConverter\n"
"{\n"
"  float4 Execute (float4 sample)\n"
"  {\n"
"    return sample;\n"
"  }\n"
"};\n"
"\n"
"class ConverterRange : IConverter\n"
"{\n"
"  float4 Execute (float4 sample)\n"
"  {\n"
"    float3 out_space;\n"
"    out_space.x = postCoeff.CoeffX.x * sample.x;\n"
"    out_space.y = postCoeff.CoeffY.y * sample.y;\n"
"    out_space.z = postCoeff.CoeffZ.z * sample.z;\n"
"    out_space += postCoeff.Offset;\n"
"    return float4 (clamp (out_space, postCoeff.Min, postCoeff.Max), sample.a);\n"
"  }\n"
"};\n"
"\n"
"class ConverterSimple : IConverter\n"
"{\n"
"  float4 Execute (float4 sample)\n"
"  {\n"
"    float3 out_space;\n"
"    out_space.x = dot (postCoeff.CoeffX, sample.xyz);\n"
"    out_space.y = dot (postCoeff.CoeffY, sample.xyz);\n"
"    out_space.z = dot (postCoeff.CoeffZ, sample.xyz);\n"
"    out_space += postCoeff.Offset;\n"
"    return float4 (clamp (out_space, postCoeff.Min, postCoeff.Max), sample.a);\n"
"  }\n"
"};\n"
"\n"
"class ConverterGamma : IConverter\n"
"{\n"
"  float4 Execute (float4 sample)\n"
"  {\n"
"    float3 rgb_space;\n"
"    float3 out_space;\n"
"    rgb_space.x = dot (preCoeff.CoeffX, sample.xyz);\n"
"    rgb_space.y = dot (preCoeff.CoeffY, sample.xyz);\n"
"    rgb_space.z = dot (preCoeff.CoeffZ, sample.xyz);\n"
"    rgb_space += preCoeff.Offset;\n"
"    rgb_space = clamp (rgb_space, preCoeff.Min, preCoeff.Max);\n"
"\n"
"    rgb_space.x = gammaDecLUT.Sample (linearSampler, rgb_space.x);\n"
"    rgb_space.y = gammaDecLUT.Sample (linearSampler, rgb_space.y);\n"
"    rgb_space.z = gammaDecLUT.Sample (linearSampler, rgb_space.z);\n"
"\n"
"    rgb_space.x = gammaEncLUT.Sample (linearSampler, rgb_space.x);\n"
"    rgb_space.y = gammaEncLUT.Sample (linearSampler, rgb_space.y);\n"
"    rgb_space.z = gammaEncLUT.Sample (linearSampler, rgb_space.z);\n"
"\n"
"    out_space.x = dot (postCoeff.CoeffX, rgb_space);\n"
"    out_space.y = dot (postCoeff.CoeffY, rgb_space);\n"
"    out_space.z = dot (postCoeff.CoeffZ, rgb_space);\n"
"    out_space += postCoeff.Offset;\n"
"    return float4 (clamp (out_space, postCoeff.Min, postCoeff.Max), sample.a);\n"
"  }\n"
"};\n"
"\n"
"class ConverterPrimary : IConverter\n"
"{\n"
"  float4 Execute (float4 sample)\n"
"  {\n"
"    float3 rgb_space;\n"
"    float3 primary_converted;\n"
"    float3 out_space;\n"
"\n"
"    rgb_space.x = dot (preCoeff.CoeffX, sample.xyz);\n"
"    rgb_space.y = dot (preCoeff.CoeffY, sample.xyz);\n"
"    rgb_space.z = dot (preCoeff.CoeffZ, sample.xyz);\n"
"    rgb_space += preCoeff.Offset;\n"
"    rgb_space = clamp (rgb_space, preCoeff.Min, preCoeff.Max);\n"
"\n"
"    rgb_space.x = gammaDecLUT.Sample (linearSampler, rgb_space.x);\n"
"    rgb_space.y = gammaDecLUT.Sample (linearSampler, rgb_space.y);\n"
"    rgb_space.z = gammaDecLUT.Sample (linearSampler, rgb_space.z);\n"
"\n"
"    primary_converted.x = dot (primariesCoeff.CoeffX, rgb_space);\n"
"    primary_converted.y = dot (primariesCoeff.CoeffY, rgb_space);\n"
"    primary_converted.z = dot (primariesCoeff.CoeffZ, rgb_space);\n"
"\n"
"    rgb_space.x = gammaEncLUT.Sample (linearSampler, primary_converted.x);\n"
"    rgb_space.y = gammaEncLUT.Sample (linearSampler, primary_converted.y);\n"
"    rgb_space.z = gammaEncLUT.Sample (linearSampler, primary_converted.z);\n"
"\n"
"    out_space.x = dot (postCoeff.CoeffX, rgb_space);\n"
"    out_space.y = dot (postCoeff.CoeffY, rgb_space);\n"
"    out_space.z = dot (postCoeff.CoeffZ, rgb_space);\n"
"    out_space += postCoeff.Offset;\n"
"    return float4 (clamp (out_space, postCoeff.Min, postCoeff.Max), sample.a);\n"
"  }\n"
"};\n"
"\n"
"float UnormTo10bit (float sample)\n"
"{\n"
"  return sample * 1023.0 / 65535.0;\n"
"}\n"
"\n"
"float2 UnormTo10bit (float2 sample)\n"
"{\n"
"  return sample * 1023.0 / 65535.0;\n"
"}\n"
"\n"
"float3 UnormTo10bit (float3 sample)\n"
"{\n"
"  return sample * 1023.0 / 65535.0;\n"
"}\n"
"\n"
"float4 UnormTo10bit (float4 sample)\n"
"{\n"
"  return sample * 1023.0 / 65535.0;\n"
"}\n"
"\n"
"float UnormTo12bit (float sample)\n"
"{\n"
"  return sample * 4095.0 / 65535.0;\n"
"}\n"
"\n"
"float2 UnormTo12bit (float2 sample)\n"
"{\n"
"  return sample * 4095.0 / 65535.0;\n"
"}\n"
"\n"
"float3 UnormTo12bit (float3 sample)\n"
"{\n"
"  return sample * 4095.0 / 65535.0;\n"
"}\n"
"\n"
"float4 UnormTo12bit (float4 sample)\n"
"{\n"
"  return sample * 4095.0 / 65535.0;\n"
"}\n"
"\n"
"interface IOutputLuma\n"
"{\n"
"  PS_OUTPUT_LUMA Build (float4 sample);\n"
"};\n"
"\n"
"class OutputLuma : IOutputLuma\n"
"{\n"
"  PS_OUTPUT_LUMA Build (float4 sample)\n"
"  {\n"
"    PS_OUTPUT_LUMA output;\n"
"    output.Plane0 = float4 (sample.x, 0, 0, 0);\n"
"    return output;\n"
"  }\n"
"};\n"
"\n"
"class OutputLuma_10 : IOutputLuma\n"
"{\n"
"  PS_OUTPUT_LUMA Build (float4 sample)\n"
"  {\n"
"    PS_OUTPUT_LUMA output;\n"
"    output.Plane0 = float4 (UnormTo10bit (sample.x), 0, 0, 0);\n"
"    return output;\n"
"  }\n"
"};\n"
"\n"
"class OutputLuma_12 : IOutputLuma\n"
"{\n"
"  PS_OUTPUT_LUMA Build (float4 sample)\n"
"  {\n"
"    PS_OUTPUT_LUMA output;\n"
"    output.Plane0 = float4 (UnormTo12bit (sample.x), 0, 0, 0);\n"
"    return output;\n"
"  }\n"
"};\n"
"\n"
"interface IOutputChroma\n"
"{\n"
"  PS_OUTPUT_CHROMA Build (float4 sample);\n"
"};\n"
"\n"
"class OutputChromaNV12 : IOutputChroma\n"
"{\n"
"  PS_OUTPUT_CHROMA Build (float4 sample)\n"
"  {\n"
"    PS_OUTPUT_CHROMA output;\n"
"    output.Plane0 = float4 (sample.yz, 0, 0);\n"
"    return output;\n"
"  }\n"
"};\n"
"\n"
"class OutputChromaNV21 : IOutputChroma\n"
"{\n"
"  PS_OUTPUT_CHROMA Build (float4 sample)\n"
"  {\n"
"    PS_OUTPUT_CHROMA output;\n"
"    output.Plane0 = float4 (sample.zy, 0, 0);\n"
"    return output;\n"
"  }\n"
"};\n"
"\n"
"interface IOutputChromaPlanar\n"
"{ PS_OUTPUT_CHROMA_PLANAR Build (float4 sample);\n"
"};\n"
"\n"
"class OutputChromaI420 : IOutputChromaPlanar\n"
"{\n"
"  PS_OUTPUT_CHROMA_PLANAR Build (float4 sample)\n"
"  {\n"
"    PS_OUTPUT_CHROMA_PLANAR output;\n"
"    output.Plane0 = float4 (sample.y, 0, 0, 0);\n"
"    output.Plane1 = float4 (sample.z, 0, 0, 0);\n"
"    return output;\n"
"  }\n"
"};\n"
"\n"
"class OutputChromaYV12 : IOutputChromaPlanar\n"
"{\n"
"  PS_OUTPUT_CHROMA_PLANAR Build (float4 sample)\n"
"  {\n"
"    PS_OUTPUT_CHROMA_PLANAR output;\n"
"    output.Plane0 = float4 (sample.z, 0, 0, 0);\n"
"    output.Plane1 = float4 (sample.y, 0, 0, 0);\n"
"    return output;\n"
"  }\n"
"};\n"
"\n"
"class OutputChromaI420_10 : IOutputChromaPlanar\n"
"{\n"
"  PS_OUTPUT_CHROMA_PLANAR Build (float4 sample)\n"
"  {\n"
"    PS_OUTPUT_CHROMA_PLANAR output;\n"
"    float2 scaled = UnormTo10bit (sample.yz);\n"
"    output.Plane0 = float4 (scaled.x, 0, 0, 0);\n"
"    output.Plane1 = float4 (scaled.y, 0, 0, 0);\n"
"    return output;\n"
"  }\n"
"};\n"
"\n"
"class OutputChromaI420_12 : IOutputChromaPlanar\n"
"{\n"
"  PS_OUTPUT_CHROMA_PLANAR Build (float4 sample)\n"
"  {\n"
"    PS_OUTPUT_CHROMA_PLANAR output;\n"
"    float2 scaled = UnormTo12bit (sample.yz);\n"
"    output.Plane0 = float4 (scaled.x, 0, 0, 0);\n"
"    output.Plane1 = float4 (scaled.y, 0, 0, 0);\n"
"    return output;\n"
"  }\n"
"};\n"
"\n"
"interface IOutputPlanar\n"
"{\n"
"  PS_OUTPUT_PLANAR Build (float4 sample);\n"
"};\n"
"\n"
"class OutputY444 : IOutputPlanar\n"
"{\n"
"  PS_OUTPUT_PLANAR Build (float4 sample)\n"
"  {\n"
"    PS_OUTPUT_PLANAR output;\n"
"    output.Plane0 = float4 (sample.x, 0, 0, 0);\n"
"    output.Plane1 = float4 (sample.y, 0, 0, 0);\n"
"    output.Plane2 = float4 (sample.z, 0, 0, 0);\n"
"    return output;\n"
"  }\n"
"};\n"
"\n"
"class OutputY444_10 : IOutputPlanar\n"
"{\n"
"  PS_OUTPUT_PLANAR Build (float4 sample)\n"
"  {\n"
"    PS_OUTPUT_PLANAR output;\n"
"    float3 scaled = UnormTo10bit (sample.xyz);\n"
"    output.Plane0 = float4 (scaled.x, 0, 0, 0);\n"
"    output.Plane1 = float4 (scaled.y, 0, 0, 0);\n"
"    output.Plane2 = float4 (scaled.z, 0, 0, 0);\n"
"    return output;\n"
"  }\n"
"};\n"
"\n"
"class OutputY444_12 : IOutputPlanar\n"
"{\n"
"  PS_OUTPUT_PLANAR Build (float4 sample)\n"
"  {\n"
"    PS_OUTPUT_PLANAR output;\n"
"    float3 scaled = UnormTo12bit (sample.xyz);\n"
"    output.Plane0 = float4 (scaled.x, 0, 0, 0);\n"
"    output.Plane1 = float4 (scaled.y, 0, 0, 0);\n"
"    output.Plane2 = float4 (scaled.z, 0, 0, 0);\n"
"    return output;\n"
"  }\n"
"};\n"
"\n"
"class OutputGBR : IOutputPlanar\n"
"{\n"
"  PS_OUTPUT_PLANAR Build (float4 sample)\n"
"  {\n"
"    PS_OUTPUT_PLANAR output;\n"
"    output.Plane0 = float4 (sample.g, 0, 0, 0);\n"
"    output.Plane1 = float4 (sample.b, 0, 0, 0);\n"
"    output.Plane2 = float4 (sample.r, 0, 0, 0);\n"
"    return output;\n"
"  }\n"
"};\n"
"\n"
"class OutputGBR_10 : IOutputPlanar\n"
"{\n"
"  PS_OUTPUT_PLANAR Build (float4 sample)\n"
"  {\n"
"    PS_OUTPUT_PLANAR output;\n"
"    float3 scaled = UnormTo10bit (sample.rgb);\n"
"    output.Plane0 = float4 (scaled.g, 0, 0, 0);\n"
"    output.Plane1 = float4 (scaled.b, 0, 0, 0);\n"
"    output.Plane2 = float4 (scaled.r, 0, 0, 0);\n"
"    return output;\n"
"  }\n"
"};\n"
"\n"
"class OutputGBR_12 : IOutputPlanar\n"
"{\n"
"  PS_OUTPUT_PLANAR Build (float4 sample)\n"
"  {\n"
"    PS_OUTPUT_PLANAR output;\n"
"    float3 scaled = UnormTo12bit (sample.rgb);\n"
"    output.Plane0 = float4 (scaled.g, 0, 0, 0);\n"
"    output.Plane1 = float4 (scaled.b, 0, 0, 0);\n"
"    output.Plane2 = float4 (scaled.r, 0, 0, 0);\n"
"    return output;\n"
"  }\n"
"};\n"
"\n"
"class OutputRGBP : IOutputPlanar\n"
"{\n"
"  PS_OUTPUT_PLANAR Build (float4 sample)\n"
"  {\n"
"    PS_OUTPUT_PLANAR output;\n"
"    output.Plane0 = float4 (sample.r, 0, 0, 0);\n"
"    output.Plane1 = float4 (sample.g, 0, 0, 0);\n"
"    output.Plane2 = float4 (sample.b, 0, 0, 0);\n"
"    return output;\n"
"  }\n"
"};\n"
"\n"
"class OutputBGRP : IOutputPlanar\n"
"{\n"
"  PS_OUTPUT_PLANAR Build (float4 sample)\n"
"  {\n"
"    PS_OUTPUT_PLANAR output;\n"
"    output.Plane0 = float4 (sample.b, 0, 0, 0);\n"
"    output.Plane1 = float4 (sample.g, 0, 0, 0);\n"
"    output.Plane2 = float4 (sample.r, 0, 0, 0);\n"
"    return output;\n"
"  }\n"
"};\n"
"\n"
"interface IOutputPlanarFull\n"
"{\n"
"  PS_OUTPUT_PLANAR_FULL Build (float4 sample);\n"
"};\n"
"\n"
"class OutputGBRA : IOutputPlanarFull\n"
"{\n"
"  PS_OUTPUT_PLANAR_FULL Build (float4 sample)\n"
"  {\n"
"    PS_OUTPUT_PLANAR_FULL output;\n"
"    output.Plane0 = float4 (sample.g, 0, 0, 0);\n"
"    output.Plane1 = float4 (sample.b, 0, 0, 0);\n"
"    output.Plane2 = float4 (sample.r, 0, 0, 0);\n"
"    output.Plane3 = float4 (sample.a * alphaFactor, 0, 0, 0);\n"
"    return output;\n"
"  }\n"
"};\n"
"\n"
"class OutputGBRAPremul : IOutputPlanarFull\n"
"{\n"
"  PS_OUTPUT_PLANAR_FULL Build (float4 sample)\n"
"  {\n"
"    PS_OUTPUT_PLANAR_FULL output;\n"
"    float4 premul;\n"
"    sample.a *= alphaFactor;\n"
"    premul = DoAlphaPremul (sample);\n"
"    output.Plane0 = float4 (premul.g, 0, 0, 0);\n"
"    output.Plane1 = float4 (premul.b, 0, 0, 0);\n"
"    output.Plane2 = float4 (premul.r, 0, 0, 0);\n"
"    output.Plane3 = float4 (premul.a, 0, 0, 0);\n"
"    return output;\n"
"  }\n"
"};\n"
"\n"
"class OutputGBRA_10 : IOutputPlanarFull\n"
"{\n"
"  PS_OUTPUT_PLANAR_FULL Build (float4 sample)\n"
"  {\n"
"    PS_OUTPUT_PLANAR_FULL output;\n"
"    float4 scaled;\n"
"    sample.a *= alphaFactor;\n"
"    scaled = UnormTo10bit (sample);\n"
"    output.Plane0 = float4 (scaled.g, 0, 0, 0);\n"
"    output.Plane1 = float4 (scaled.b, 0, 0, 0);\n"
"    output.Plane2 = float4 (scaled.r, 0, 0, 0);\n"
"    output.Plane3 = float4 (scaled.a, 0, 0, 0);\n"
"    return output;\n"
"  }\n"
"};\n"
"\n"
"class OutputGBRAPremul_10 : IOutputPlanarFull\n"
"{\n"
"  PS_OUTPUT_PLANAR_FULL Build (float4 sample)\n"
"  {\n"
"    PS_OUTPUT_PLANAR_FULL output;\n"
"    float4 scaled;\n"
"    sample.a *= alphaFactor;\n"
"    scaled = UnormTo10bit (DoAlphaPremul (sample));\n"
"    output.Plane0 = float4 (scaled.g, 0, 0, 0);\n"
"    output.Plane1 = float4 (scaled.b, 0, 0, 0);\n"
"    output.Plane2 = float4 (scaled.r, 0, 0, 0);\n"
"    output.Plane3 = float4 (scaled.a, 0, 0, 0);\n"
"    return output;\n"
"  }\n"
"};\n"
"\n"
"class OutputGBRA_12 : IOutputPlanarFull\n"
"{\n"
"  PS_OUTPUT_PLANAR_FULL Build (float4 sample)\n"
"  {\n"
"    PS_OUTPUT_PLANAR_FULL output;\n"
"    float4 scaled;\n"
"    sample.a *= alphaFactor;\n"
"    scaled = UnormTo12bit (sample);\n"
"    output.Plane0 = float4 (scaled.g, 0, 0, 0);\n"
"    output.Plane1 = float4 (scaled.b, 0, 0, 0);\n"
"    output.Plane2 = float4 (scaled.r, 0, 0, 0);\n"
"    output.Plane3 = float4 (scaled.a, 0, 0, 0);\n"
"    return output;\n"
"  }\n"
"};\n"
"\n"
"class OutputGBRAPremul_12 : IOutputPlanarFull\n"
"{\n"
"  PS_OUTPUT_PLANAR_FULL Build (float4 sample)\n"
"  {\n"
"    PS_OUTPUT_PLANAR_FULL output;\n"
"    float4 scaled;\n"
"    sample.a *= alphaFactor;\n"
"    scaled = UnormTo12bit (DoAlphaPremul (sample));\n"
"    output.Plane0 = float4 (scaled.g, 0, 0, 0);\n"
"    output.Plane1 = float4 (scaled.b, 0, 0, 0);\n"
"    output.Plane2 = float4 (scaled.r, 0, 0, 0);\n"
"    output.Plane3 = float4 (scaled.a, 0, 0, 0);\n"
"    return output;\n"
"  }\n"
"};\n"
"\n"
"interface IOutputPacked\n"
"{\n"
"  PS_OUTPUT_PACKED Build (float4 sample);\n"
"};\n"
"\n"
"class OutputRGBA : IOutputPacked\n"
"{\n"
"  PS_OUTPUT_PACKED Build (float4 sample)\n"
"  {\n"
"    PS_OUTPUT_PACKED output;\n"
"    output.Plane0 = float4 (sample.rgb, sample.a * alphaFactor);\n"
"    return output;\n"
"  }\n"
"};\n"
"\n"
"class OutputRGBAPremul : IOutputPacked\n"
"{\n"
"  PS_OUTPUT_PACKED Build (float4 sample)\n"
"  {\n"
"    PS_OUTPUT_PACKED output;\n"
"    sample.a *= alphaFactor;\n"
"    output.Plane0 = DoAlphaPremul (sample);\n"
"    return output;\n"
"  }\n"
"};\n"
"\n"
"class OutputRGBx : IOutputPacked\n"
"{\n"
"  PS_OUTPUT_PACKED Build (float4 sample)\n"
"  {\n"
"    PS_OUTPUT_PACKED output;\n"
"    output.Plane0 = float4 (sample.rgb, 1.0);\n"
"    return output;\n"
"  }\n"
"};\n"
"\n"
"class OutputxRGB : IOutputPacked\n"
"{\n"
"  PS_OUTPUT_PACKED Build (float4 sample)\n"
"  {\n"
"    PS_OUTPUT_PACKED output;\n"
"    output.Plane0 = float4 (0.0, sample.rgb);\n"
"    return output;\n"
"  }\n"
"};\n"
"\n"
"class OutputARGB : IOutputPacked\n"
"{\n"
"  PS_OUTPUT_PACKED Build (float4 sample)\n"
"  {\n"
"    PS_OUTPUT_PACKED output;\n"
"    output.Plane0 = sample.argb;\n"
"    return output;\n"
"  }\n"
"};\n"
"\n"
"class OutputARGBPremul : IOutputPacked\n"
"{\n"
"  PS_OUTPUT_PACKED Build (float4 sample)\n"
"  {\n"
"    PS_OUTPUT_PACKED output;\n"
"    output.Plane0 = DoAlphaPremul (sample).argb;\n"
"    return output;\n"
"  }\n"
"};\n"
"\n"
"class OutputxBGR : IOutputPacked\n"
"{\n"
"  PS_OUTPUT_PACKED Build (float4 sample)\n"
"  {\n"
"    PS_OUTPUT_PACKED output;\n"
"    output.Plane0 = float4 (0.0, sample.bgr);\n"
"    return output;\n"
"  }\n"
"};\n"
"\n"
"class OutputABGR : IOutputPacked\n"
"{\n"
"  PS_OUTPUT_PACKED Build (float4 sample)\n"
"  {\n"
"    PS_OUTPUT_PACKED output;\n"
"    output.Plane0 = sample.abgr;\n"
"    return output;\n"
"  }\n"
"};\n"
"\n"
"class OutputABGRPremul : IOutputPacked\n"
"{\n"
"  PS_OUTPUT_PACKED Build (float4 sample)\n"
"  {\n"
"    PS_OUTPUT_PACKED output;\n"
"    output.Plane0 = DoAlphaPremul (sample.abgr);\n"
"    return output;\n"
"  }\n"
"};\n"
"\n"
"class OutputVUYA : IOutputPacked\n"
"{\n"
"  PS_OUTPUT_PACKED Build (float4 sample)\n"
"  {\n"
"    PS_OUTPUT_PACKED output;\n"
"    sample.a *= alphaFactor;\n"
"    output.Plane0 = sample.zyxw;\n"
"    return output;\n"
"  }\n"
"};\n"
"\n"
"class OutputVUYAPremul : IOutputPacked\n"
"{\n"
"  PS_OUTPUT_PACKED Build (float4 sample)\n"
"  {\n"
"    PS_OUTPUT_PACKED output;\n"
"    sample.a *= alphaFactor;\n"
"    output.Plane0 = DoAlphaPremul (sample).zyxw;\n"
"    return output;\n"
"  }\n"
"};\n"
"\n"
"class OutputAYUV : IOutputPacked\n"
"{\n"
"  PS_OUTPUT_PACKED Build (float4 sample)\n"
"  {\n"
"    PS_OUTPUT_PACKED output;\n"
"    sample.a *= alphaFactor;\n"
"    output.Plane0 = sample.wxyz;\n"
"    return output;\n"
"  }\n"
"};\n"
"\n"
"class OutputAYUVPremul : IOutputPacked\n"
"{\n"
"  PS_OUTPUT_PACKED Build (float4 sample)\n"
"  {\n"
"    PS_OUTPUT_PACKED output;\n"
"    sample.a *= alphaFactor;\n"
"    output.Plane0 = DoAlphaPremul (sample).wxyz;\n"
"    return output;\n"
"  }\n"
"};\n"
"\n"
"class OutputRBGA : IOutputPacked\n"
"{\n"
"  PS_OUTPUT_PACKED Build (float4 sample)\n"
"  {\n"
"    PS_OUTPUT_PACKED output;\n"
"    sample.a *= alphaFactor;\n"
"    output.Plane0 = sample.rbga;\n"
"    return output;\n"
"  }\n"
"};\n"
"\n"
"class OutputRBGAPremul : IOutputPacked\n"
"{\n"
"  PS_OUTPUT_PACKED Build (float4 sample)\n"
"  {\n"
"    PS_OUTPUT_PACKED output;\n"
"    sample.a *= alphaFactor;\n"
"    output.Plane0 = DoAlphaPremul (sample).rbga;\n"
"    return output;\n"
"  }\n"
"};\n"
"\n"
"OUTPUT_TYPE ENTRY_POINT (PS_INPUT input)\n"
"{\n"
"  SAMPLER g_sampler;\n"
"  CONVERTER g_converter;\n"
"  OUTPUT_BUILDER g_builder;\n"
"  return g_builder.Build (g_converter.Execute (g_sampler.Execute (input.Texture)));\n"
"}\n";
#endif
