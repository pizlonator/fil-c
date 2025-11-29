#ifdef HAVE_VULKAN_1_2
#extension GL_EXT_nonuniform_qualifier : enable
#endif

#include "enums.glsl"

layout(push_constant) uniform PushConstants {
    mat4 mvp;
    mat3x4 clip;
    vec2 scale;
} push;

layout(constant_id=0) const uint GSK_SHADER_CLIP = GSK_GPU_SHADER_CLIP_NONE;
layout(constant_id=1) const uint GSK_N_IMMUTABLE_SAMPLERS = 32;
layout(constant_id=2) const uint GSK_N_SAMPLERS = 32;
layout(constant_id=3) const uint GSK_N_BUFFERS = 32;
layout(constant_id=4) const uint GSK_VARIATION = 0;

#define GSK_GLOBAL_MVP push.mvp
#define GSK_GLOBAL_CLIP push.clip
#define GSK_GLOBAL_CLIP_RECT push.clip[0]
#define GSK_GLOBAL_SCALE push.scale

#define GSK_VERTEX_INDEX gl_VertexIndex

#ifdef GSK_VERTEX_SHADER
#define IN(_loc) layout(location = _loc) in
#define PASS(_loc) layout(location = _loc) out
#define PASS_FLAT(_loc) layout(location = _loc) flat out
#endif


#ifdef GSK_FRAGMENT_SHADER
#define PASS(_loc) layout(location = _loc) in
#define PASS_FLAT(_loc) layout(location = _loc) flat in

layout(set = 0, binding = 0) uniform sampler2D immutable_textures[GSK_N_IMMUTABLE_SAMPLERS];
layout(set = 0, binding = 1) uniform sampler2D textures[GSK_N_SAMPLERS];
layout(set = 1, binding = 0) readonly buffer FloatBuffers {
  float floats[];
} buffers[GSK_N_BUFFERS];

layout(location = 0) out vec4 out_color;

vec4
gsk_texture (uint id,
             vec2 pos)
{
  if ((id & 1) != 0)
    {
      id >>= 1;
      if (id < 16)
        {
          if (id < 8)
            {
              if (id < 4)
                {
                  if (id < 2)
                    {
                      if (id < 1)
                        return texture (immutable_textures[0], pos);
                      else if (GSK_N_IMMUTABLE_SAMPLERS > 1)
                        return texture (immutable_textures[1], pos);
                    }
                  else if (GSK_N_IMMUTABLE_SAMPLERS > 2)
                    {
                      if (id < 3)
                        return texture (immutable_textures[2], pos);
                      else if (GSK_N_IMMUTABLE_SAMPLERS > 3)
                        return texture (immutable_textures[3], pos);
                    }
                }
              else if (GSK_N_IMMUTABLE_SAMPLERS > 4)
                {
                  if (id < 6)
                    {
                      if (id < 5)
                        return texture (immutable_textures[4], pos);
                      else if (GSK_N_IMMUTABLE_SAMPLERS > 5)
                        return texture (immutable_textures[5], pos);
                    }
                  else if (GSK_N_IMMUTABLE_SAMPLERS > 6)
                    {
                      if (id < 7)
                        return texture (immutable_textures[6], pos);
                      else if (GSK_N_IMMUTABLE_SAMPLERS > 7)
                        return texture (immutable_textures[7], pos);
                    }
                }
            }
          else if (GSK_N_IMMUTABLE_SAMPLERS > 8)
            {
              if (id < 12)
                {
                  if (id < 10)
                    {
                      if (id < 9)
                        return texture (immutable_textures[8], pos);
                      else if (GSK_N_IMMUTABLE_SAMPLERS > 9)
                        return texture (immutable_textures[9], pos);
                    }
                  else if (GSK_N_IMMUTABLE_SAMPLERS > 10)
                    {
                      if (id < 11)
                        return texture (immutable_textures[10], pos);
                      else if (GSK_N_IMMUTABLE_SAMPLERS > 11)
                        return texture (immutable_textures[11], pos);
                    }
                }
              else if (GSK_N_IMMUTABLE_SAMPLERS > 12)
                {
                  if (id < 14)
                    {
                      if (id < 13)
                        return texture (immutable_textures[12], pos);
                      else if (GSK_N_IMMUTABLE_SAMPLERS > 13)
                        return texture (immutable_textures[13], pos);
                    }
                  else if (GSK_N_IMMUTABLE_SAMPLERS > 14)
                    {
                      if (id < 15)
                        return texture (immutable_textures[14], pos);
                      else if (GSK_N_IMMUTABLE_SAMPLERS > 15)
                        return texture (immutable_textures[15], pos);
                    }
                }
            }
        }
      else if (GSK_N_IMMUTABLE_SAMPLERS > 16)
        {
          if (id < 24)
            {
              if (id < 20)
                {
                  if (id < 18)
                    {
                      if (id < 17)
                        return texture (immutable_textures[16], pos);
                      else if (GSK_N_IMMUTABLE_SAMPLERS > 17)
                        return texture (immutable_textures[17], pos);
                    }
                  else if (GSK_N_IMMUTABLE_SAMPLERS > 18)
                    {
                      if (id < 19)
                        return texture (immutable_textures[18], pos);
                      else if (GSK_N_IMMUTABLE_SAMPLERS > 19)
                        return texture (immutable_textures[19], pos);
                    }
                }
              else if (GSK_N_IMMUTABLE_SAMPLERS > 20)
                {
                  if (id < 22)
                    {
                      if (id < 21)
                        return texture (immutable_textures[20], pos);
                      else if (GSK_N_IMMUTABLE_SAMPLERS > 21)
                        return texture (immutable_textures[21], pos);
                    }
                  else if (GSK_N_IMMUTABLE_SAMPLERS > 22)
                    {
                      if (id < 23)
                        return texture (immutable_textures[22], pos);
                      else if (GSK_N_IMMUTABLE_SAMPLERS > 23)
                        return texture (immutable_textures[23], pos);
                    }
                }
            }
          else if (GSK_N_IMMUTABLE_SAMPLERS > 24)
            {
              if (id < 28)
                {
                  if (id < 26)
                    {
                      if (id < 25)
                        return texture (immutable_textures[24], pos);
                      else if (GSK_N_IMMUTABLE_SAMPLERS > 25)
                        return texture (immutable_textures[25], pos);
                    }
                  else if (GSK_N_IMMUTABLE_SAMPLERS > 26)
                    {
                      if (id < 27)
                        return texture (immutable_textures[26], pos);
                      else if (GSK_N_IMMUTABLE_SAMPLERS > 27)
                        return texture (immutable_textures[27], pos);
                    }
                }
              else if (GSK_N_IMMUTABLE_SAMPLERS > 28)
                {
                  if (id < 30)
                    {
                      if (id < 29)
                        return texture (immutable_textures[28], pos);
                      else if (GSK_N_IMMUTABLE_SAMPLERS > 29)
                        return texture (immutable_textures[29], pos);
                    }
                  else if (GSK_N_IMMUTABLE_SAMPLERS > 30)
                    {
                      if (id < 31)
                        return texture (immutable_textures[30], pos);
                      else if (GSK_N_IMMUTABLE_SAMPLERS > 31)
                        return texture (immutable_textures[31], pos);
                    }
                }
            }
        }
    }
  else
    {
      id >>= 1;
#ifdef HAVE_VULKAN_1_2
      return texture (textures[nonuniformEXT (id)], pos);
#else
      if (id < 16)
        {
          if (id < 8)
            {
              if (id < 4)
                {
                  if (id < 2)
                    {
                      if (id < 1)
                        return texture (textures[0], pos);
                      else if (GSK_N_SAMPLERS > 1)
                        return texture (textures[1], pos);
                    }
                  else if (GSK_N_SAMPLERS > 2)
                    {
                      if (id < 3)
                        return texture (textures[2], pos);
                      else if (GSK_N_SAMPLERS > 3)
                        return texture (textures[3], pos);
                    }
                }
              else if (GSK_N_SAMPLERS > 4)
                {
                  if (id < 6)
                    {
                      if (id < 5)
                        return texture (textures[4], pos);
                      else if (GSK_N_SAMPLERS > 5)
                        return texture (textures[5], pos);
                    }
                  else if (GSK_N_SAMPLERS > 6)
                    {
                      if (id < 7)
                        return texture (textures[6], pos);
                      else if (GSK_N_SAMPLERS > 7)
                        return texture (textures[7], pos);
                    }
                }
            }
          else if (GSK_N_SAMPLERS > 8)
            {
              if (id < 12)
                {
                  if (id < 10)
                    {
                      if (id < 9)
                        return texture (textures[8], pos);
                      else if (GSK_N_SAMPLERS > 9)
                        return texture (textures[9], pos);
                    }
                  else if (GSK_N_SAMPLERS > 10)
                    {
                      if (id < 11)
                        return texture (textures[10], pos);
                      else if (GSK_N_SAMPLERS > 11)
                        return texture (textures[11], pos);
                    }
                }
              else if (GSK_N_SAMPLERS > 12)
                {
                  if (id < 14)
                    {
                      if (id < 13)
                        return texture (textures[12], pos);
                      else if (GSK_N_SAMPLERS > 13)
                        return texture (textures[13], pos);
                    }
                  else if (GSK_N_SAMPLERS > 14)
                    {
                      if (id < 15)
                        return texture (textures[14], pos);
                      else if (GSK_N_SAMPLERS > 15)
                        return texture (textures[15], pos);
                    }
                }
            }
        }
      else if (GSK_N_SAMPLERS > 16)
        {
          if (id < 24)
            {
              if (id < 20)
                {
                  if (id < 18)
                    {
                      if (id < 17)
                        return texture (textures[16], pos);
                      else if (GSK_N_SAMPLERS > 17)
                        return texture (textures[17], pos);
                    }
                  else if (GSK_N_SAMPLERS > 18)
                    {
                      if (id < 19)
                        return texture (textures[18], pos);
                      else if (GSK_N_SAMPLERS > 19)
                        return texture (textures[19], pos);
                    }
                }
              else if (GSK_N_SAMPLERS > 20)
                {
                  if (id < 22)
                    {
                      if (id < 21)
                        return texture (textures[20], pos);
                      else if (GSK_N_SAMPLERS > 21)
                        return texture (textures[21], pos);
                    }
                  else if (GSK_N_SAMPLERS > 22)
                    {
                      if (id < 23)
                        return texture (textures[22], pos);
                      else if (GSK_N_SAMPLERS > 23)
                        return texture (textures[23], pos);
                    }
                }
            }
          else if (GSK_N_SAMPLERS > 24)
            {
              if (id < 28)
                {
                  if (id < 26)
                    {
                      if (id < 25)
                        return texture (textures[24], pos);
                      else if (GSK_N_SAMPLERS > 25)
                        return texture (textures[25], pos);
                    }
                  else if (GSK_N_SAMPLERS > 26)
                    {
                      if (id < 27)
                        return texture (textures[26], pos);
                      else if (GSK_N_SAMPLERS > 27)
                        return texture (textures[27], pos);
                    }
                }
              else if (GSK_N_SAMPLERS > 28)
                {
                  if (id < 30)
                    {
                      if (id < 29)
                        return texture (textures[28], pos);
                      else if (GSK_N_SAMPLERS > 29)
                        return texture (textures[29], pos);
                    }
                  else if (GSK_N_SAMPLERS > 30)
                    {
                      if (id < 31)
                        return texture (textures[30], pos);
                      else if (GSK_N_SAMPLERS > 31)
                        return texture (textures[31], pos);
                    }
                }
            }
        }
#endif
    }
  return vec4 (1.0, 0.0, 0.8, 1.0);
}

ivec2
gsk_texture_size (uint id,
                  int  lod)
{
  if ((id & 1) != 0)
    {
      id >>= 1;
      if (id < 16)
        {
          if (id < 8)
            {
              if (id < 4)
                {
                  if (id < 2)
                    {
                      if (id < 1)
                        return textureSize (immutable_textures[0], lod);
                      else if (GSK_N_IMMUTABLE_SAMPLERS > 1)
                        return textureSize (immutable_textures[1], lod);
                    }
                  else if (GSK_N_IMMUTABLE_SAMPLERS > 2)
                    {
                      if (id < 3)
                        return textureSize (immutable_textures[2], lod);
                      else if (GSK_N_IMMUTABLE_SAMPLERS > 3)
                        return textureSize (immutable_textures[3], lod);
                    }
                }
              else if (GSK_N_IMMUTABLE_SAMPLERS > 4)
                {
                  if (id < 6)
                    {
                      if (id < 5)
                        return textureSize (immutable_textures[4], lod);
                      else if (GSK_N_IMMUTABLE_SAMPLERS > 5)
                        return textureSize (immutable_textures[5], lod);
                    }
                  else if (GSK_N_IMMUTABLE_SAMPLERS > 6)
                    {
                      if (id < 7)
                        return textureSize (immutable_textures[6], lod);
                      else if (GSK_N_IMMUTABLE_SAMPLERS > 7)
                        return textureSize (immutable_textures[7], lod);
                    }
                }
            }
          else if (GSK_N_IMMUTABLE_SAMPLERS > 8)
            {
              if (id < 12)
                {
                  if (id < 10)
                    {
                      if (id < 9)
                        return textureSize (immutable_textures[8], lod);
                      else if (GSK_N_IMMUTABLE_SAMPLERS > 9)
                        return textureSize (immutable_textures[9], lod);
                    }
                  else if (GSK_N_IMMUTABLE_SAMPLERS > 10)
                    {
                      if (id < 11)
                        return textureSize (immutable_textures[10], lod);
                      else if (GSK_N_IMMUTABLE_SAMPLERS > 11)
                        return textureSize (immutable_textures[11], lod);
                    }
                }
              else if (GSK_N_IMMUTABLE_SAMPLERS > 12)
                {
                  if (id < 14)
                    {
                      if (id < 13)
                        return textureSize (immutable_textures[12], lod);
                      else if (GSK_N_IMMUTABLE_SAMPLERS > 13)
                        return textureSize (immutable_textures[13], lod);
                    }
                  else if (GSK_N_IMMUTABLE_SAMPLERS > 14)
                    {
                      if (id < 15)
                        return textureSize (immutable_textures[14], lod);
                      else if (GSK_N_IMMUTABLE_SAMPLERS > 15)
                        return textureSize (immutable_textures[15], lod);
                    }
                }
            }
        }
      else if (GSK_N_IMMUTABLE_SAMPLERS > 16)
        {
          if (id < 24)
            {
              if (id < 20)
                {
                  if (id < 18)
                    {
                      if (id < 17)
                        return textureSize (immutable_textures[16], lod);
                      else if (GSK_N_IMMUTABLE_SAMPLERS > 17)
                        return textureSize (immutable_textures[17], lod);
                    }
                  else if (GSK_N_IMMUTABLE_SAMPLERS > 18)
                    {
                      if (id < 19)
                        return textureSize (immutable_textures[18], lod);
                      else if (GSK_N_IMMUTABLE_SAMPLERS > 19)
                        return textureSize (immutable_textures[19], lod);
                    }
                }
              else if (GSK_N_IMMUTABLE_SAMPLERS > 20)
                {
                  if (id < 22)
                    {
                      if (id < 21)
                        return textureSize (immutable_textures[20], lod);
                      else if (GSK_N_IMMUTABLE_SAMPLERS > 21)
                        return textureSize (immutable_textures[21], lod);
                    }
                  else if (GSK_N_IMMUTABLE_SAMPLERS > 22)
                    {
                      if (id < 23)
                        return textureSize (immutable_textures[22], lod);
                      else if (GSK_N_IMMUTABLE_SAMPLERS > 23)
                        return textureSize (immutable_textures[23], lod);
                    }
                }
            }
          else if (GSK_N_IMMUTABLE_SAMPLERS > 24)
            {
              if (id < 28)
                {
                  if (id < 26)
                    {
                      if (id < 25)
                        return textureSize (immutable_textures[24], lod);
                      else if (GSK_N_IMMUTABLE_SAMPLERS > 25)
                        return textureSize (immutable_textures[25], lod);
                    }
                  else if (GSK_N_IMMUTABLE_SAMPLERS > 26)
                    {
                      if (id < 27)
                        return textureSize (immutable_textures[26], lod);
                      else if (GSK_N_IMMUTABLE_SAMPLERS > 27)
                        return textureSize (immutable_textures[27], lod);
                    }
                }
              else if (GSK_N_IMMUTABLE_SAMPLERS > 28)
                {
                  if (id < 30)
                    {
                      if (id < 29)
                        return textureSize (immutable_textures[28], lod);
                      else if (GSK_N_IMMUTABLE_SAMPLERS > 29)
                        return textureSize (immutable_textures[29], lod);
                    }
                  else if (GSK_N_IMMUTABLE_SAMPLERS > 30)
                    {
                      if (id < 31)
                        return textureSize (immutable_textures[30], lod);
                      else if (GSK_N_IMMUTABLE_SAMPLERS > 31)
                        return textureSize (immutable_textures[31], lod);
                    }
                }
            }
        }
    }
  else
    {
      id >>= 1;
#ifdef HAVE_VULKAN_1_2
      return textureSize (textures[nonuniformEXT (id)], lod);
#else
      if (id < 16)
        {
          if (id < 8)
            {
              if (id < 4)
                {
                  if (id < 2)
                    {
                      if (id < 1)
                        return textureSize (textures[0], lod);
                      else if (GSK_N_SAMPLERS > 1)
                        return textureSize (textures[1], lod);
                    }
                  else if (GSK_N_SAMPLERS > 2)
                    {
                      if (id < 3)
                        return textureSize (textures[2], lod);
                      else if (GSK_N_SAMPLERS > 3)
                        return textureSize (textures[3], lod);
                    }
                }
              else if (GSK_N_SAMPLERS > 4)
                {
                  if (id < 6)
                    {
                      if (id < 5)
                        return textureSize (textures[4], lod);
                      else if (GSK_N_SAMPLERS > 5)
                        return textureSize (textures[5], lod);
                    }
                  else if (GSK_N_SAMPLERS > 6)
                    {
                      if (id < 7)
                        return textureSize (textures[6], lod);
                      else if (GSK_N_SAMPLERS > 7)
                        return textureSize (textures[7], lod);
                    }
                }
            }
          else if (GSK_N_SAMPLERS > 8)
            {
              if (id < 12)
                {
                  if (id < 10)
                    {
                      if (id < 9)
                        return textureSize (textures[8], lod);
                      else if (GSK_N_SAMPLERS > 9)
                        return textureSize (textures[9], lod);
                    }
                  else if (GSK_N_SAMPLERS > 10)
                    {
                      if (id < 11)
                        return textureSize (textures[10], lod);
                      else if (GSK_N_SAMPLERS > 11)
                        return textureSize (textures[11], lod);
                    }
                }
              else if (GSK_N_SAMPLERS > 12)
                {
                  if (id < 14)
                    {
                      if (id < 13)
                        return textureSize (textures[12], lod);
                      else if (GSK_N_SAMPLERS > 13)
                        return textureSize (textures[13], lod);
                    }
                  else if (GSK_N_SAMPLERS > 14)
                    {
                      if (id < 15)
                        return textureSize (textures[14], lod);
                      else if (GSK_N_SAMPLERS > 15)
                        return textureSize (textures[15], lod);
                    }
                }
            }
        }
      else if (GSK_N_SAMPLERS > 16)
        {
          if (id < 24)
            {
              if (id < 20)
                {
                  if (id < 18)
                    {
                      if (id < 17)
                        return textureSize (textures[16], lod);
                      else if (GSK_N_SAMPLERS > 17)
                        return textureSize (textures[17], lod);
                    }
                  else if (GSK_N_SAMPLERS > 18)
                    {
                      if (id < 19)
                        return textureSize (textures[18], lod);
                      else if (GSK_N_SAMPLERS > 19)
                        return textureSize (textures[19], lod);
                    }
                }
              else if (GSK_N_SAMPLERS > 20)
                {
                  if (id < 22)
                    {
                      if (id < 21)
                        return textureSize (textures[20], lod);
                      else if (GSK_N_SAMPLERS > 21)
                        return textureSize (textures[21], lod);
                    }
                  else if (GSK_N_SAMPLERS > 22)
                    {
                      if (id < 23)
                        return textureSize (textures[22], lod);
                      else if (GSK_N_SAMPLERS > 23)
                        return textureSize (textures[23], lod);
                    }
                }
            }
          else if (GSK_N_SAMPLERS > 24)
            {
              if (id < 28)
                {
                  if (id < 26)
                    {
                      if (id < 25)
                        return textureSize (textures[24], lod);
                      else if (GSK_N_SAMPLERS > 25)
                        return textureSize (textures[25], lod);
                    }
                  else if (GSK_N_SAMPLERS > 26)
                    {
                      if (id < 27)
                        return textureSize (textures[26], lod);
                      else if (GSK_N_SAMPLERS > 27)
                        return textureSize (textures[27], lod);
                    }
                }
              else if (GSK_N_SAMPLERS > 28)
                {
                  if (id < 30)
                    {
                      if (id < 29)
                        return textureSize (textures[28], lod);
                      else if (GSK_N_SAMPLERS > 29)
                        return textureSize (textures[29], lod);
                    }
                  else if (GSK_N_SAMPLERS > 30)
                    {
                      if (id < 31)
                        return textureSize (textures[30], lod);
                      else if (GSK_N_SAMPLERS > 31)
                        return textureSize (textures[31], lod);
                    }
                }
            }
        }
#endif
    }
  return ivec2 (1, 1);
}

vec4
gsk_texel_fetch (uint  id,
                 ivec2 pos,
                 int   lod)
{
  if ((id & 1) != 0)
    {
      id >>= 1;
      if (id < 16)
        {
          if (id < 8)
            {
              if (id < 4)
                {
                  if (id < 2)
                    {
                      if (id < 1)
                        return texelFetch (immutable_textures[0], pos, lod);
                      else if (GSK_N_IMMUTABLE_SAMPLERS > 1)
                        return texelFetch (immutable_textures[1], pos, lod);
                    }
                  else if (GSK_N_IMMUTABLE_SAMPLERS > 2)
                    {
                      if (id < 3)
                        return texelFetch (immutable_textures[2], pos, lod);
                      else if (GSK_N_IMMUTABLE_SAMPLERS > 3)
                        return texelFetch (immutable_textures[3], pos, lod);
                    }
                }
              else if (GSK_N_IMMUTABLE_SAMPLERS > 4)
                {
                  if (id < 6)
                    {
                      if (id < 5)
                        return texelFetch (immutable_textures[4], pos, lod);
                      else if (GSK_N_IMMUTABLE_SAMPLERS > 5)
                        return texelFetch (immutable_textures[5], pos, lod);
                    }
                  else if (GSK_N_IMMUTABLE_SAMPLERS > 6)
                    {
                      if (id < 7)
                        return texelFetch (immutable_textures[6], pos, lod);
                      else if (GSK_N_IMMUTABLE_SAMPLERS > 7)
                        return texelFetch (immutable_textures[7], pos, lod);
                    }
                }
            }
          else if (GSK_N_IMMUTABLE_SAMPLERS > 8)
            {
              if (id < 12)
                {
                  if (id < 10)
                    {
                      if (id < 9)
                        return texelFetch (immutable_textures[8], pos, lod);
                      else if (GSK_N_IMMUTABLE_SAMPLERS > 9)
                        return texelFetch (immutable_textures[9], pos, lod);
                    }
                  else if (GSK_N_IMMUTABLE_SAMPLERS > 10)
                    {
                      if (id < 11)
                        return texelFetch (immutable_textures[10], pos, lod);
                      else if (GSK_N_IMMUTABLE_SAMPLERS > 11)
                        return texelFetch (immutable_textures[11], pos, lod);
                    }
                }
              else if (GSK_N_IMMUTABLE_SAMPLERS > 12)
                {
                  if (id < 14)
                    {
                      if (id < 13)
                        return texelFetch (immutable_textures[12], pos, lod);
                      else if (GSK_N_IMMUTABLE_SAMPLERS > 13)
                        return texelFetch (immutable_textures[13], pos, lod);
                    }
                  else if (GSK_N_IMMUTABLE_SAMPLERS > 14)
                    {
                      if (id < 15)
                        return texelFetch (immutable_textures[14], pos, lod);
                      else if (GSK_N_IMMUTABLE_SAMPLERS > 15)
                        return texelFetch (immutable_textures[15], pos, lod);
                    }
                }
            }
        }
      else if (GSK_N_IMMUTABLE_SAMPLERS > 16)
        {
          if (id < 24)
            {
              if (id < 20)
                {
                  if (id < 18)
                    {
                      if (id < 17)
                        return texelFetch (immutable_textures[16], pos, lod);
                      else if (GSK_N_IMMUTABLE_SAMPLERS > 17)
                        return texelFetch (immutable_textures[17], pos, lod);
                    }
                  else if (GSK_N_IMMUTABLE_SAMPLERS > 18)
                    {
                      if (id < 19)
                        return texelFetch (immutable_textures[18], pos, lod);
                      else if (GSK_N_IMMUTABLE_SAMPLERS > 19)
                        return texelFetch (immutable_textures[19], pos, lod);
                    }
                }
              else if (GSK_N_IMMUTABLE_SAMPLERS > 20)
                {
                  if (id < 22)
                    {
                      if (id < 21)
                        return texelFetch (immutable_textures[20], pos, lod);
                      else if (GSK_N_IMMUTABLE_SAMPLERS > 21)
                        return texelFetch (immutable_textures[21], pos, lod);
                    }
                  else if (GSK_N_IMMUTABLE_SAMPLERS > 22)
                    {
                      if (id < 23)
                        return texelFetch (immutable_textures[22], pos, lod);
                      else if (GSK_N_IMMUTABLE_SAMPLERS > 23)
                        return texelFetch (immutable_textures[23], pos, lod);
                    }
                }
            }
          else if (GSK_N_IMMUTABLE_SAMPLERS > 24)
            {
              if (id < 28)
                {
                  if (id < 26)
                    {
                      if (id < 25)
                        return texelFetch (immutable_textures[24], pos, lod);
                      else if (GSK_N_IMMUTABLE_SAMPLERS > 25)
                        return texelFetch (immutable_textures[25], pos, lod);
                    }
                  else if (GSK_N_IMMUTABLE_SAMPLERS > 26)
                    {
                      if (id < 27)
                        return texelFetch (immutable_textures[26], pos, lod);
                      else if (GSK_N_IMMUTABLE_SAMPLERS > 27)
                        return texelFetch (immutable_textures[27], pos, lod);
                    }
                }
              else if (GSK_N_IMMUTABLE_SAMPLERS > 28)
                {
                  if (id < 30)
                    {
                      if (id < 29)
                        return texelFetch (immutable_textures[28], pos, lod);
                      else if (GSK_N_IMMUTABLE_SAMPLERS > 29)
                        return texelFetch (immutable_textures[29], pos, lod);
                    }
                  else if (GSK_N_IMMUTABLE_SAMPLERS > 30)
                    {
                      if (id < 31)
                        return texelFetch (immutable_textures[30], pos, lod);
                      else if (GSK_N_IMMUTABLE_SAMPLERS > 31)
                        return texelFetch (immutable_textures[31], pos, lod);
                    }
                }
            }
        }
    }
  else
    {
      id >>= 1;
#ifdef HAVE_VULKAN_1_2
      return texelFetch (textures[nonuniformEXT (id)], pos, lod);
#else
      if (id < 16)
        {
          if (id < 8)
            {
              if (id < 4)
                {
                  if (id < 2)
                    {
                      if (id < 1)
                        return texelFetch (textures[0], pos, lod);
                      else if (GSK_N_SAMPLERS > 1)
                        return texelFetch (textures[1], pos, lod);
                    }
                  else if (GSK_N_SAMPLERS > 2)
                    {
                      if (id < 3)
                        return texelFetch (textures[2], pos, lod);
                      else if (GSK_N_SAMPLERS > 3)
                        return texelFetch (textures[3], pos, lod);
                    }
                }
              else if (GSK_N_SAMPLERS > 4)
                {
                  if (id < 6)
                    {
                      if (id < 5)
                        return texelFetch (textures[4], pos, lod);
                      else if (GSK_N_SAMPLERS > 5)
                        return texelFetch (textures[5], pos, lod);
                    }
                  else if (GSK_N_SAMPLERS > 6)
                    {
                      if (id < 7)
                        return texelFetch (textures[6], pos, lod);
                      else if (GSK_N_SAMPLERS > 7)
                        return texelFetch (textures[7], pos, lod);
                    }
                }
            }
          else if (GSK_N_SAMPLERS > 8)
            {
              if (id < 12)
                {
                  if (id < 10)
                    {
                      if (id < 9)
                        return texelFetch (textures[8], pos, lod);
                      else if (GSK_N_SAMPLERS > 9)
                        return texelFetch (textures[9], pos, lod);
                    }
                  else if (GSK_N_SAMPLERS > 10)
                    {
                      if (id < 11)
                        return texelFetch (textures[10], pos, lod);
                      else if (GSK_N_SAMPLERS > 11)
                        return texelFetch (textures[11], pos, lod);
                    }
                }
              else if (GSK_N_SAMPLERS > 12)
                {
                  if (id < 14)
                    {
                      if (id < 13)
                        return texelFetch (textures[12], pos, lod);
                      else if (GSK_N_SAMPLERS > 13)
                        return texelFetch (textures[13], pos, lod);
                    }
                  else if (GSK_N_SAMPLERS > 14)
                    {
                      if (id < 15)
                        return texelFetch (textures[14], pos, lod);
                      else if (GSK_N_SAMPLERS > 15)
                        return texelFetch (textures[15], pos, lod);
                    }
                }
            }
        }
      else if (GSK_N_SAMPLERS > 16)
        {
          if (id < 24)
            {
              if (id < 20)
                {
                  if (id < 18)
                    {
                      if (id < 17)
                        return texelFetch (textures[16], pos, lod);
                      else if (GSK_N_SAMPLERS > 17)
                        return texelFetch (textures[17], pos, lod);
                    }
                  else if (GSK_N_SAMPLERS > 18)
                    {
                      if (id < 19)
                        return texelFetch (textures[18], pos, lod);
                      else if (GSK_N_SAMPLERS > 19)
                        return texelFetch (textures[19], pos, lod);
                    }
                }
              else if (GSK_N_SAMPLERS > 20)
                {
                  if (id < 22)
                    {
                      if (id < 21)
                        return texelFetch (textures[20], pos, lod);
                      else if (GSK_N_SAMPLERS > 21)
                        return texelFetch (textures[21], pos, lod);
                    }
                  else if (GSK_N_SAMPLERS > 22)
                    {
                      if (id < 23)
                        return texelFetch (textures[22], pos, lod);
                      else if (GSK_N_SAMPLERS > 23)
                        return texelFetch (textures[23], pos, lod);
                    }
                }
            }
          else if (GSK_N_SAMPLERS > 24)
            {
              if (id < 28)
                {
                  if (id < 26)
                    {
                      if (id < 25)
                        return texelFetch (textures[24], pos, lod);
                      else if (GSK_N_SAMPLERS > 25)
                        return texelFetch (textures[25], pos, lod);
                    }
                  else if (GSK_N_SAMPLERS > 26)
                    {
                      if (id < 27)
                        return texelFetch (textures[26], pos, lod);
                      else if (GSK_N_SAMPLERS > 27)
                        return texelFetch (textures[27], pos, lod);
                    }
                }
              else if (GSK_N_SAMPLERS > 28)
                {
                  if (id < 30)
                    {
                      if (id < 29)
                        return texelFetch (textures[28], pos, lod);
                      else if (GSK_N_SAMPLERS > 29)
                        return texelFetch (textures[29], pos, lod);
                    }
                  else if (GSK_N_SAMPLERS > 30)
                    {
                      if (id < 31)
                        return texelFetch (textures[30], pos, lod);
                      else if (GSK_N_SAMPLERS > 31)
                        return texelFetch (textures[31], pos, lod);
                    }
                }
            }
        }
#endif
    }
  return vec4 (1.0, 0.0, 0.8, 1.0);
}

#ifdef HAVE_VULKAN_1_2
#define gsk_get_buffer(id) buffers[nonuniformEXT (id)]
#define gsk_get_float(id) gsk_get_buffer(id >> 22).floats[id & 0x3FFFFF]
#else
float
gsk_get_float (uint id)
{
  uint buffer_id = id >> 22;
  uint float_id = id & 0x3FFFFF;
  if (buffer_id < 16)
    {
      if (buffer_id < 8)
        {
          if (buffer_id < 4)
            {
              if (buffer_id < 2)
                {
                  if (buffer_id < 1)
                    return buffers[0].floats[float_id];
                  else if (GSK_N_BUFFERS > 1)
                    return buffers[1].floats[float_id];
                }
              else if (GSK_N_BUFFERS > 2)
                {
                  if (buffer_id < 3)
                    return buffers[2].floats[float_id];
                  else if (GSK_N_BUFFERS > 3)
                    return buffers[3].floats[float_id];
                }
            }
          else if (GSK_N_BUFFERS > 4)
            {
              if (buffer_id < 6)
                {
                  if (buffer_id < 5)
                    return buffers[4].floats[float_id];
                  else if (GSK_N_BUFFERS > 5)
                    return buffers[5].floats[float_id];
                }
              else if (GSK_N_BUFFERS > 6)
                {
                  if (buffer_id < 7)
                    return buffers[6].floats[float_id];
                  else if (GSK_N_BUFFERS > 7)
                    return buffers[7].floats[float_id];
                }
            }
        }
      else if (GSK_N_BUFFERS > 8)
        {
          if (buffer_id < 12)
            {
              if (buffer_id < 10)
                {
                  if (buffer_id < 9)
                    return buffers[8].floats[float_id];
                  else if (GSK_N_BUFFERS > 9)
                    return buffers[9].floats[float_id];
                }
              else if (GSK_N_BUFFERS > 10)
                {
                  if (buffer_id < 11)
                    return buffers[10].floats[float_id];
                  else if (GSK_N_BUFFERS > 11)
                    return buffers[11].floats[float_id];
                }
            }
          else if (GSK_N_BUFFERS > 12)
            {
              if (buffer_id < 14)
                {
                  if (buffer_id < 13)
                    return buffers[12].floats[float_id];
                  else if (GSK_N_BUFFERS > 13)
                    return buffers[13].floats[float_id];
                }
              else if (GSK_N_BUFFERS > 14)
                {
                  if (buffer_id < 15)
                    return buffers[14].floats[float_id];
                  else if (GSK_N_BUFFERS > 15)
                    return buffers[15].floats[float_id];
                }
            }
        }
    }
  else if (GSK_N_BUFFERS > 16)
    {
      if (buffer_id < 24)
        {
          if (buffer_id < 20)
            {
              if (buffer_id < 18)
                {
                  if (buffer_id < 17)
                    return buffers[16].floats[float_id];
                  else if (GSK_N_BUFFERS > 17)
                    return buffers[17].floats[float_id];
                }
              else if (GSK_N_BUFFERS > 18)
                {
                  if (buffer_id < 19)
                    return buffers[18].floats[float_id];
                  else if (GSK_N_BUFFERS > 19)
                    return buffers[19].floats[float_id];
                }
            }
          else if (GSK_N_BUFFERS > 20)
            {
              if (buffer_id < 22)
                {
                  if (buffer_id < 21)
                    return buffers[20].floats[float_id];
                  else if (GSK_N_BUFFERS > 21)
                    return buffers[21].floats[float_id];
                }
              else if (GSK_N_BUFFERS > 22)
                {
                  if (buffer_id < 23)
                    return buffers[22].floats[float_id];
                  else if (GSK_N_BUFFERS > 23)
                    return buffers[23].floats[float_id];
                }
            }
        }
      else if (GSK_N_BUFFERS > 24)
        {
          if (buffer_id < 28)
            {
              if (buffer_id < 26)
                {
                  if (buffer_id < 25)
                    return buffers[24].floats[float_id];
                  else if (GSK_N_BUFFERS > 25)
                    return buffers[25].floats[float_id];
                }
              else if (GSK_N_BUFFERS > 26)
                {
                  if (buffer_id < 27)
                    return buffers[26].floats[float_id];
                  else if (GSK_N_BUFFERS > 27)
                    return buffers[27].floats[float_id];
                }
            }
          else if (GSK_N_BUFFERS > 28)
            {
              if (buffer_id < 30)
                {
                  if (buffer_id < 29)
                    return buffers[28].floats[float_id];
                  else if (GSK_N_BUFFERS > 29)
                    return buffers[29].floats[float_id];
                }
              else if (GSK_N_BUFFERS > 30)
                {
                  if (buffer_id < 31)
                    return buffers[30].floats[float_id];
                  else if (GSK_N_BUFFERS > 31)
                    return buffers[31].floats[float_id];
                }
            }
        }
    }
  return 0.0;
}
#endif
#define gsk_get_int(id) (floatBitsToInt(gsk_get_float(id)))
#define gsk_get_uint(id) (floatBitsToUint(gsk_get_float(id)))

void
gsk_set_output_color (vec4 color)
{
  out_color = color;
}

#endif
