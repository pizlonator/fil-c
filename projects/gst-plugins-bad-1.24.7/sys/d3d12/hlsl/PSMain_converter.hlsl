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

cbuffer PsAlphaFactor : register(b0, space0)
{
  float alphaFactor;
};

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

cbuffer PsConstBuffer : register(b1, space0)
{
  PSColorSpace preCoeff;
  PSColorSpace postCoeff;
  PSColorSpace primariesCoeff;
};

#ifdef NUM_SRV_1
Texture2D shaderTexture_0 : register(t0, space0);
#endif
#ifdef NUM_SRV_2
Texture2D shaderTexture_0 : register(t0, space0);
Texture2D shaderTexture_1 : register(t1, space0);
#endif
#ifdef NUM_SRV_3
Texture2D shaderTexture_0 : register(t0, space0);
Texture2D shaderTexture_1 : register(t1, space0);
Texture2D shaderTexture_2 : register(t2, space0);
#endif
#ifdef NUM_SRV_4
Texture2D shaderTexture_0 : register(t0, space0);
Texture2D shaderTexture_1 : register(t1, space0);
Texture2D shaderTexture_2 : register(t2, space0);
Texture2D shaderTexture_3 : register(t3, space0);
#endif

SamplerState samplerState : register(s0, space0);

#ifdef BUILD_LUT
Texture1D<float> gammaDecLUT : register(t4, space0);
Texture1D<float> gammaEncLUT : register(t5, space0);
SamplerState lutSamplerState : register(s1, space0);
#endif

struct PS_INPUT
{
  float4 Position: SV_POSITION;
  float2 Texture: TEXCOORD;
};

struct PS_OUTPUT_PACKED
{
  float4 Plane0: SV_TARGET0;
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

#ifdef NUM_SRV_1
class SamplerRGBA : ISampler
{
  float4 Execute (float2 uv)
  {
    return shaderTexture_0.Sample(samplerState, uv);
  }
};

class SamplerRGBAPremul : ISampler
{
  float4 Execute (float2 uv)
  {
    return DoAlphaUnpremul (shaderTexture_0.Sample(samplerState, uv));
  }
};

class SamplerRBGA : ISampler
{
  float4 Execute (float2 uv)
  {
    return shaderTexture_0.Sample(samplerState, uv).rbga;
  }
};

class SamplerVUYA : ISampler
{
  float4 Execute (float2 uv)
  {
    return shaderTexture_0.Sample(samplerState, uv).zyxw;
  }
};

class SamplerY410 : ISampler
{
  float4 Execute (float2 uv)
  {
    return float4 (shaderTexture_0.Sample(samplerState, uv).yxz, 1.0);
  }
};

class SamplerY412 : ISampler
{
  float4 Execute (float2 uv)
  {
    return shaderTexture_0.Sample(samplerState, uv).grba;
  }
};

class SamplerAYUV : ISampler
{
  float4 Execute (float2 uv)
  {
    return shaderTexture_0.Sample(samplerState, uv).yzwx;
  }
};

class SamplerRGBx : ISampler
{
  float4 Execute (float2 uv)
  {
    return float4 (shaderTexture_0.Sample(samplerState, uv).rgb, 1.0);
  }
};

class SamplerxRGB : ISampler
{
  float4 Execute (float2 uv)
  {
    return float4 (shaderTexture_0.Sample(samplerState, uv).gba, 1.0);
  }
};

class SamplerARGB : ISampler
{
  float4 Execute (float2 uv)
  {
    return shaderTexture_0.Sample(samplerState, uv).gbar;
  }
};

class SamplerxBGR : ISampler
{
  float4 Execute (float2 uv)
  {
    return float4 (shaderTexture_0.Sample(samplerState, uv).abg, 1.0);
  }
};

class SamplerABGR : ISampler
{
  float4 Execute (float2 uv)
  {
    return shaderTexture_0.Sample(samplerState, uv).abgr;
  }
};

class SamplerBGR10A2 : ISampler
{
  float4 Execute (float2 uv)
  {
    return float4 (shaderTexture_0.Sample(samplerState, uv).zyx, 1.0);
  }
};

class SamplerBGRA64 : ISampler
{
  float4 Execute (float2 uv)
  {
    return shaderTexture_0.Sample(samplerState, uv).bgra;
  }
};

class SamplerGRAY : ISampler
{
  float4 Execute (float2 uv)
  {
    float4 sample;
    sample.x = shaderTexture_0.Sample(samplerState, uv).x;
    sample.y = 0.5;
    sample.z = 0.5;
    sample.a = 1.0;
    return sample;
  }
};
#endif

#ifdef NUM_SRV_2
class SamplerNV12 : ISampler
{
  float4 Execute (float2 uv)
  {
    float4 sample;
    sample.x = shaderTexture_0.Sample(samplerState, uv).x;
    sample.yz = shaderTexture_1.Sample(samplerState, uv).xy;
    sample.a = 1.0;
    return sample;
  }
};

class SamplerNV21 : ISampler
{
  float4 Execute (float2 uv)
  {
    float4 sample;
    sample.x = shaderTexture_0.Sample(samplerState, uv).x;
    sample.yz = shaderTexture_1.Sample(samplerState, uv).yx;
    sample.a = 1.0;
    return sample;
  }
};
#endif

#ifdef NUM_SRV_3
class SamplerI420 : ISampler
{
  float4 Execute (float2 uv)
  {
    float4 sample;
    sample.x = shaderTexture_0.Sample(samplerState, uv).x;
    sample.y = shaderTexture_1.Sample(samplerState, uv).x;
    sample.z = shaderTexture_2.Sample(samplerState, uv).x;
    sample.a = 1.0;
    return sample;
  }
};

class SamplerYV12 : ISampler
{
  float4 Execute (float2 uv)
  {
    float4 sample;
    sample.x = shaderTexture_0.Sample(samplerState, uv).x;
    sample.z = shaderTexture_1.Sample(samplerState, uv).x;
    sample.y = shaderTexture_2.Sample(samplerState, uv).x;
    sample.a = 1.0;
    return sample;
  }
};

class SamplerI420_10 : ISampler
{
  float4 Execute (float2 uv)
  {
    float3 sample;
    sample.x = shaderTexture_0.Sample(samplerState, uv).x;
    sample.y = shaderTexture_1.Sample(samplerState, uv).x;
    sample.z = shaderTexture_2.Sample(samplerState, uv).x;
    return float4 (saturate (sample * 64.0), 1.0);
  }
};

class SamplerI420_12 : ISampler
{
  float4 Execute (float2 uv)
  {
    float3 sample;
    sample.x = shaderTexture_0.Sample(samplerState, uv).x;
    sample.y = shaderTexture_1.Sample(samplerState, uv).x;
    sample.z = shaderTexture_2.Sample(samplerState, uv).x;
    return float4 (saturate (sample * 16.0), 1.0);
  }
};

class SamplerGBR : ISampler
{
  float4 Execute (float2 uv)
  {
    float4 sample;
    sample.g = shaderTexture_0.Sample(samplerState, uv).x;
    sample.b = shaderTexture_1.Sample(samplerState, uv).x;
    sample.r = shaderTexture_2.Sample(samplerState, uv).x;
    sample.a = 1.0;
    return sample;
  }
};

class SamplerGBR_10 : ISampler
{
  float4 Execute (float2 uv)
  {
    float3 sample;
    sample.g = shaderTexture_0.Sample(samplerState, uv).x;
    sample.b = shaderTexture_1.Sample(samplerState, uv).x;
    sample.r = shaderTexture_2.Sample(samplerState, uv).x;
    return float4 (saturate (sample * 64.0), 1.0);
  }
};

class SamplerGBR_12 : ISampler
{
  float4 Execute (float2 uv)
  {
    float3 sample;
    sample.g = shaderTexture_0.Sample(samplerState, uv).x;
    sample.b = shaderTexture_1.Sample(samplerState, uv).x;
    sample.r = shaderTexture_2.Sample(samplerState, uv).x;
    return float4 (saturate (sample * 16.0), 1.0);
  }
};

class SamplerRGBP : ISampler
{
  float4 Execute (float2 uv)
  {
    float4 sample;
    sample.r = shaderTexture_0.Sample(samplerState, uv).x;
    sample.g = shaderTexture_1.Sample(samplerState, uv).x;
    sample.b = shaderTexture_2.Sample(samplerState, uv).x;
    sample.a = 1.0;
    return sample;
  }
};

class SamplerBGRP : ISampler
{
  float4 Execute (float2 uv)
  {
    float4 sample;
    sample.b = shaderTexture_0.Sample(samplerState, uv).x;
    sample.g = shaderTexture_1.Sample(samplerState, uv).x;
    sample.r = shaderTexture_2.Sample(samplerState, uv).x;
    sample.a = 1.0;
    return sample;
  }
};
#endif

#ifdef NUM_SRV_4
class SamplerGBRA : ISampler
{
  float4 Execute (float2 uv)
  {
    float4 sample;
    sample.g = shaderTexture_0.Sample(samplerState, uv).x;
    sample.b = shaderTexture_1.Sample(samplerState, uv).x;
    sample.r = shaderTexture_2.Sample(samplerState, uv).x;
    sample.a = shaderTexture_3.Sample(samplerState, uv).x;
    return sample;
  }
};

class SamplerGBRA_10 : ISampler
{
  float4 Execute (float2 uv)
  {
    float4 sample;
    sample.g = shaderTexture_0.Sample(samplerState, uv).x;
    sample.b = shaderTexture_1.Sample(samplerState, uv).x;
    sample.r = shaderTexture_2.Sample(samplerState, uv).x;
    sample.a = shaderTexture_3.Sample(samplerState, uv).x;
    return saturate (sample * 64.0);
  }
};

class SamplerGBRA_12 : ISampler
{
  float4 Execute (float2 uv)
  {
    float4 sample;
    sample.g = shaderTexture_0.Sample(samplerState, uv).x;
    sample.b = shaderTexture_1.Sample(samplerState, uv).x;
    sample.r = shaderTexture_2.Sample(samplerState, uv).x;
    sample.a = shaderTexture_3.Sample(samplerState, uv).x;
    return saturate (sample * 16.0);
  }
};
#endif

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

#ifdef BUILD_LUT
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

    rgb_space.x = gammaDecLUT.Sample (lutSamplerState, rgb_space.x);
    rgb_space.y = gammaDecLUT.Sample (lutSamplerState, rgb_space.y);
    rgb_space.z = gammaDecLUT.Sample (lutSamplerState, rgb_space.z);

    rgb_space.x = gammaEncLUT.Sample (lutSamplerState, rgb_space.x);
    rgb_space.y = gammaEncLUT.Sample (lutSamplerState, rgb_space.y);
    rgb_space.z = gammaEncLUT.Sample (lutSamplerState, rgb_space.z);

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

    rgb_space.x = gammaDecLUT.Sample (lutSamplerState, rgb_space.x);
    rgb_space.y = gammaDecLUT.Sample (lutSamplerState, rgb_space.y);
    rgb_space.z = gammaDecLUT.Sample (lutSamplerState, rgb_space.z);

    primary_converted.x = dot (primariesCoeff.CoeffX, rgb_space);
    primary_converted.y = dot (primariesCoeff.CoeffY, rgb_space);
    primary_converted.z = dot (primariesCoeff.CoeffZ, rgb_space);

    rgb_space.x = gammaEncLUT.Sample (lutSamplerState, primary_converted.x);
    rgb_space.y = gammaEncLUT.Sample (lutSamplerState, primary_converted.y);
    rgb_space.z = gammaEncLUT.Sample (lutSamplerState, primary_converted.z);

    out_space.x = dot (postCoeff.CoeffX, rgb_space);
    out_space.y = dot (postCoeff.CoeffY, rgb_space);
    out_space.z = dot (postCoeff.CoeffZ, rgb_space);
    out_space += postCoeff.Offset;
    return float4 (clamp (out_space, postCoeff.Min, postCoeff.Max), sample.a);
  }
};
#endif

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
{
  PS_OUTPUT_CHROMA_PLANAR Build (float4 sample);
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

OUTPUT_TYPE ENTRY_POINT (PS_INPUT input)
{
  SAMPLER g_sampler;
  CONVERTER g_converter;
  OUTPUT_BUILDER g_builder;
  return g_builder.Build (g_converter.Execute (g_sampler.Execute (input.Texture)));
}
