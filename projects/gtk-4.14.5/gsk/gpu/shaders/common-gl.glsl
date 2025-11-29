precision highp float;

#if __VERSION__ < 420 || (defined(GSK_GLES) && __VERSION__ < 310)
layout(std140)
#else
layout(std140, binding = 0)
#endif
uniform PushConstants
{
    mat4 mvp;
    mat3x4 clip;
    vec2 scale;
} push;

#define GSK_GLOBAL_MVP push.mvp
#define GSK_GLOBAL_CLIP push.clip
#define GSK_GLOBAL_CLIP_RECT push.clip[0]
#define GSK_GLOBAL_SCALE push.scale

#if __VERSION__ < 420 || (defined(GSK_GLES) && __VERSION__ < 310)
layout(std140)
#else
layout(std140, binding = 1)
#endif
uniform Floats
{
  vec4 really_just_floats[1024];
} floats[11];

#if N_EXTERNAL_TEXTURES > 0
uniform samplerExternalOES external_textures[N_EXTERNAL_TEXTURES];
#endif
#if N_TEXTURES > 0
uniform sampler2D textures[N_TEXTURES];
#endif

#define GSK_VERTEX_INDEX gl_VertexID


#ifdef GSK_VERTEX_SHADER
#define IN(_loc) in
#define PASS(_loc) out
#define PASS_FLAT(_loc) flat out
#endif


#ifdef GSK_FRAGMENT_SHADER
#define PASS(_loc) in
#define PASS_FLAT(_loc) flat in

float
gsk_get_float (int id)
{
  int float_id = id & 0x3FFFFF;
  int array_id = (id >> 22) & 0xFF;
  switch (array_id)
    {
      case 0:
        return floats[0].really_just_floats[float_id >> 2][float_id & 3];
      case 1:
        return floats[1].really_just_floats[float_id >> 2][float_id & 3];
      case 2:
        return floats[2].really_just_floats[float_id >> 2][float_id & 3];
      case 3:
        return floats[3].really_just_floats[float_id >> 2][float_id & 3];
      case 4:
        return floats[4].really_just_floats[float_id >> 2][float_id & 3];
      case 5:
        return floats[5].really_just_floats[float_id >> 2][float_id & 3];
      case 6:
        return floats[6].really_just_floats[float_id >> 2][float_id & 3];
      case 7:
        return floats[7].really_just_floats[float_id >> 2][float_id & 3];
      case 8:
        return floats[8].really_just_floats[float_id >> 2][float_id & 3];
      case 9:
        return floats[9].really_just_floats[float_id >> 2][float_id & 3];
      case 10:
        return floats[10].really_just_floats[float_id >> 2][float_id & 3];
      default:
        return 0.0;
    }
}

float
gsk_get_float (uint id)
{
  return gsk_get_float (int (id));
}

#define gsk_get_int(id) (floatBitsToInt(gsk_get_float(id)))
#define gsk_get_uint(id) (floatBitsToUint(gsk_get_float(id)))

#if __VERSION__ < 400 || defined(GSK_GLES)

vec4
gsk_texture (uint id,
             vec2 pos)
{
  if ((id & 1u) != 0u)
    {
      switch (id >> 1u)
        {
        case 0u:
#if N_EXTERNAL_TEXTURES > 0
          return texture (external_textures[0], pos);
#endif
#if N_EXTERNAL_TEXTURES > 1
        case 1u:
          return texture (external_textures[1], pos);
#endif
        case 2u:
#if N_EXTERNAL_TEXTURES > 2
          return texture (external_textures[2], pos);
#endif
        case 3u:
#if N_EXTERNAL_TEXTURES > 3
          return texture (external_textures[3], pos);
#endif
        case 4u:
#if N_EXTERNAL_TEXTURES > 4
          return texture (external_textures[4], pos);
#endif
        case 5u:
#if N_EXTERNAL_TEXTURES > 5
          return texture (external_textures[5], pos);
#endif
        default:
          break;
        }
    }
  else
    {
      switch (id >> 1u)
        {
        case 0u:
#if N_TEXTURES > 0
          return texture (textures[0], pos);
#endif
        case 1u:
#if N_TEXTURES > 1
          return texture (textures[1], pos);
#endif
        case 2u:
#if N_TEXTURES > 2
          return texture (textures[2], pos);
#endif
        case 3u:
#if N_TEXTURES > 3
          return texture (textures[3], pos);
#endif
        case 4u:
#if N_TEXTURES > 4
          return texture (textures[4], pos);
#endif
        case 5u:
#if N_TEXTURES > 5
          return texture (textures[5], pos);
#endif
        case 6u:
#if N_TEXTURES > 6
          return texture (textures[6], pos);
#endif
        case 7u:
#if N_TEXTURES > 7
          return texture (textures[7], pos);
#endif
        case 8u:
#if N_TEXTURES > 8
          return texture (textures[8], pos);
#endif
        case 9u:
#if N_TEXTURES > 9
          return texture (textures[9], pos);
#endif
        case 10u:
#if N_TEXTURES > 10
          return texture (textures[10], pos);
#endif
        case 11u:
#if N_TEXTURES > 11
          return texture (textures[11], pos);
#endif
        case 12u:
#if N_TEXTURES > 12
          return texture (textures[12], pos);
#endif
        case 13u:
#if N_TEXTURES > 13
          return texture (textures[13], pos);
#endif
        case 14u:
#if N_TEXTURES > 14
          return texture (textures[14], pos);
#endif
        case 15u:
#if N_TEXTURES > 15
          return texture (textures[15], pos);
#endif
        default:
          break;
        }
    }
  return vec4 (1.0, 0.0, 0.8, 1.0);
}

ivec2
gsk_texture_size (uint id,
                  int  lod)
{
  if ((id & 1u) != 0u)
    {
      switch (id >> 1u)
        {
        case 0u:
#if N_EXTERNAL_TEXTURES > 0
          return textureSize (external_textures[0], lod);
#endif
#if N_EXTERNAL_TEXTURES > 1
        case 1u:
          return textureSize (external_textures[1], lod);
#endif
        case 2u:
#if N_EXTERNAL_TEXTURES > 2
          return textureSize (external_textures[2], lod);
#endif
        case 3u:
#if N_EXTERNAL_TEXTURES > 3
          return textureSize (external_textures[3], lod);
#endif
        case 4u:
#if N_EXTERNAL_TEXTURES > 4
          return textureSize (external_textures[4], lod);
#endif
        case 5u:
#if N_EXTERNAL_TEXTURES > 5
          return textureSize (external_textures[5], lod);
#endif
        default:
          break;
        }
    }
  else
    {
      switch (id >> 1u)
        {
        case 0u:
#if N_TEXTURES > 0
          return textureSize (textures[0], lod);
#endif
        case 1u:
#if N_TEXTURES > 1
          return textureSize (textures[1], lod);
#endif
        case 2u:
#if N_TEXTURES > 2
          return textureSize (textures[2], lod);
#endif
        case 3u:
#if N_TEXTURES > 3
          return textureSize (textures[3], lod);
#endif
        case 4u:
#if N_TEXTURES > 4
          return textureSize (textures[4], lod);
#endif
        case 5u:
#if N_TEXTURES > 5
          return textureSize (textures[5], lod);
#endif
        case 6u:
#if N_TEXTURES > 6
          return textureSize (textures[6], lod);
#endif
        case 7u:
#if N_TEXTURES > 7
          return textureSize (textures[7], lod);
#endif
        case 8u:
#if N_TEXTURES > 8
          return textureSize (textures[8], lod);
#endif
        case 9u:
#if N_TEXTURES > 9
          return textureSize (textures[9], lod);
#endif
        case 10u:
#if N_TEXTURES > 10
          return textureSize (textures[10], lod);
#endif
        case 11u:
#if N_TEXTURES > 11
          return textureSize (textures[11], lod);
#endif
        case 12u:
#if N_TEXTURES > 12
          return textureSize (textures[12], lod);
#endif
        case 13u:
#if N_TEXTURES > 13
          return textureSize (textures[13], lod);
#endif
        case 14u:
#if N_TEXTURES > 14
          return textureSize (textures[14], lod);
#endif
        case 15u:
#if N_TEXTURES > 15
          return textureSize (textures[15], lod);
#endif
        default:
          break;
        }
    }
  return ivec2 (1, 1);
}

vec4
gsk_texel_fetch (uint  id,
                 ivec2 pos,
                 int   lod)
{
  if ((id & 1u) != 0u)
    {
      switch (id >> 1u)
        {
        case 0u:
#if N_EXTERNAL_TEXTURES > 0
          return texelFetch (external_textures[0], pos, lod);
#endif
#if N_EXTERNAL_TEXTURES > 1
        case 1u:
          return texelFetch (external_textures[1], pos, lod);
#endif
        case 2u:
#if N_EXTERNAL_TEXTURES > 2
          return texelFetch (external_textures[2], pos, lod);
#endif
        case 3u:
#if N_EXTERNAL_TEXTURES > 3
          return texelFetch (external_textures[3], pos, lod);
#endif
        case 4u:
#if N_EXTERNAL_TEXTURES > 4
          return texelFetch (external_textures[4], pos, lod);
#endif
        case 5u:
#if N_EXTERNAL_TEXTURES > 5
          return texelFetch (external_textures[5], pos, lod);
#endif
        default:
          break;
        }
    }
  else
    {
      switch (id >> 1u)
        {
        case 0u:
#if N_TEXTURES > 0
          return texelFetch (textures[0], pos, lod);
#endif
        case 1u:
#if N_TEXTURES > 1
          return texelFetch (textures[1], pos, lod);
#endif
        case 2u:
#if N_TEXTURES > 2
          return texelFetch (textures[2], pos, lod);
#endif
        case 3u:
#if N_TEXTURES > 3
          return texelFetch (textures[3], pos, lod);
#endif
        case 4u:
#if N_TEXTURES > 4
          return texelFetch (textures[4], pos, lod);
#endif
        case 5u:
#if N_TEXTURES > 5
          return texelFetch (textures[5], pos, lod);
#endif
        case 6u:
#if N_TEXTURES > 6
          return texelFetch (textures[6], pos, lod);
#endif
        case 7u:
#if N_TEXTURES > 7
          return texelFetch (textures[7], pos, lod);
#endif
        case 8u:
#if N_TEXTURES > 8
          return texelFetch (textures[8], pos, lod);
#endif
        case 9u:
#if N_TEXTURES > 9
          return texelFetch (textures[9], pos, lod);
#endif
        case 10u:
#if N_TEXTURES > 10
          return texelFetch (textures[10], pos, lod);
#endif
        case 11u:
#if N_TEXTURES > 11
          return texelFetch (textures[11], pos, lod);
#endif
        case 12u:
#if N_TEXTURES > 12
          return texelFetch (textures[12], pos, lod);
#endif
        case 13u:
#if N_TEXTURES > 13
          return texelFetch (textures[13], pos, lod);
#endif
        case 14u:
#if N_TEXTURES > 14
          return texelFetch (textures[14], pos, lod);
#endif
        case 15u:
#if N_TEXTURES > 15
          return texelFetch (textures[15], pos, lod);
#endif
        default:
          break;
        }
    }
  return vec4 (1.0, 0.0, 0.8, 1.0);
}

#else /* !GSK_GLES */

#define gsk_texture(id, pos) texture (textures[id >> 1], pos)
#define gsk_texture_size(id, lod) textureSize (textures[id >> 1], lod)
#define gsk_texel_fetch(id, pos, lod) texelFetch (textures[id >> 1], pos, lod)

#endif

layout(location = 0) out vec4 out_color;
void
gsk_set_output_color (vec4 color)
{
  out_color = color;
}

#endif
