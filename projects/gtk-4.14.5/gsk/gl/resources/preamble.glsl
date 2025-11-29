#if defined(GSK_GLES3)
#extension GL_OES_EGL_image_external_essl3 : require
#elif defined (GSK_GLES)
#extension GL_OES_EGL_image_external : require
#endif

#ifndef GSK_LEGACY
precision highp float;
#endif

#if defined(GSK_GLES) || defined(GSK_LEGACY)
#define _OUT_ varying
#define _IN_ varying
#define _GSK_ROUNDED_RECT_UNIFORM_ vec4[3]
#else
#define _OUT_ out
#define _IN_ in
#define _GSK_ROUNDED_RECT_UNIFORM_ GskRoundedRect
#endif


struct GskRoundedRect
{
  vec4 bounds; // Top left and bottom right
  // Look, arrays can't be in structs if you want to return the struct
  // from a function in gles or whatever. Just kill me.
  vec4 corner_points1; // xy = top left, zw = top right
  vec4 corner_points2; // xy = bottom right, zw = bottom left
};

void gsk_rounded_rect_normalize(inout GskRoundedRect r)
{
  if (r.bounds.x > r.bounds.z)
    {
      float t = r.bounds.x;
      r.bounds.x = r.bounds.z;
      r.bounds.z = t;

      vec2 c = r.corner_points1.xy;
      r.corner_points1.xy = r.corner_points1.zw;
      r.corner_points1.zw = c;

      c = r.corner_points2.xy;
      r.corner_points2.xy = r.corner_points2.zw;
      r.corner_points2.zw = c;
    }

  if (r.bounds.y > r.bounds.w)
    {
      float t = r.bounds.y;
      r.bounds.y = r.bounds.w;
      r.bounds.w = t;

      vec2 c = r.corner_points1.xy;
      r.corner_points1.xy = r.corner_points2.xy;
      r.corner_points2.xy = c;

      c = r.corner_points1.zw;
      r.corner_points1.zw = r.corner_points2.zw;
      r.corner_points2.zw = c;
    }
}

void gsk_bounds_normalize (inout vec4 bounds)
{
  if (bounds.x > bounds.z)
    {
      float t = bounds.x;
      bounds.x = bounds.z;
      bounds.z = t;
    }
  if (bounds.y > bounds.w)
    {
      float t = bounds.y;
      bounds.y = bounds.w;
      bounds.w = t;
    }
}

// Transform from a C GskRoundedRect to what we need.
GskRoundedRect
gsk_create_rect(vec4[3] data)
{
  vec4 bounds = vec4(data[0].xy, data[0].xy + data[0].zw);

  vec4 corner_points1 = vec4(bounds.xy + data[1].xy,
                             bounds.zy + vec2(data[1].zw * vec2(-1, 1)));
  vec4 corner_points2 = vec4(bounds.zw + (data[2].xy * vec2(-1, -1)),
                             bounds.xw + vec2(data[2].zw * vec2(1, -1)));

  GskRoundedRect rect = GskRoundedRect(bounds, corner_points1, corner_points2);

  gsk_rounded_rect_normalize (rect);

  return rect;
}

vec4
gsk_get_bounds(vec4[3] data)
{
  vec4 bounds = vec4(data[0].xy, data[0].xy + data[0].zw);

  gsk_bounds_normalize (bounds);

  return bounds;
}

vec4 gsk_premultiply(vec4 c) {
  return vec4(c.rgb * c.a, c.a);
}

vec4 gsk_scaled_premultiply(vec4 c, float s) {
  // Fast version of gsk_premultiply(c) * s
  // 4 muls instead of 7
  float a = s * c.a;

  return vec4(c.rgb * a, a);
}
