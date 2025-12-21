/*
 * GStreamer
 * Copyright (C) 2008 Filippo Argiolas <filippo.argiolas@gmail.com>
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gst/gl/gstglconfig.h>

#include "../gstgleffects.h"
#include "gstgleffectssources.h"
#include <math.h>

/* A common file for sources is needed since shader sources can be
 * generic and reused by several effects */

/* FIXME */
/* Move sooner or later into single .frag .vert files and either bake
 * them into a c file at compile time or load them at run time */


/* fill a normalized and zero centered gaussian vector for separable
 * gaussian convolution */

void
fill_gaussian_kernel (float *kernel, int size, float sigma)
{
  int i;
  float sum;
  int l;

  /* need an odd sized vector to center it at zero */
  g_return_if_fail ((size % 2) != 0);

  sum = 0.0;
  l = (size - 1) / 2;

  for (i = 0; i < size; i++) {
    kernel[i] = expf (-0.5 * pow ((i - l) / sigma, 2.0));
    sum += kernel[i];
  }

  for (i = 0; i < size; i++) {
    kernel[i] /= sum;
  }
}

/* *INDENT-OFF* */

/* Mirror effect */
const gchar *mirror_fragment_source_opengl =
  "uniform sampler2D tex;"
  "void main () {"
  "  vec2 texturecoord = gl_TexCoord[0].xy;"
  "  vec2 normcoord;"
  "  normcoord = texturecoord - 0.5;"
  "  normcoord.x *= sign (normcoord.x);"
  "  texturecoord = normcoord + 0.5;"
  "  vec4 color = texture2D (tex, texturecoord);"
  "  gl_FragColor = color * gl_Color;"
  "}";

const gchar *mirror_fragment_source_gles2 =
  "varying vec2 v_texcoord;"
  "uniform sampler2D tex;"
  "void main () {"
  "  vec2 texturecoord = v_texcoord.xy;"
  "  float normcoord = texturecoord.x - 0.5;"
  "  normcoord *= sign (normcoord);"
  "  texturecoord.x = normcoord + 0.5;"
  "  gl_FragColor = texture2D (tex, texturecoord);"
  "}";

const gchar *squeeze_fragment_source_gles2 =
  "varying vec2 v_texcoord;"
  "uniform sampler2D tex;"
  "void main () {"
  "  vec2 texturecoord = v_texcoord.xy;"
  "  vec2 normcoord = texturecoord - 0.5;"
  /* Add a very small value to length otherwise it could be 0 */
  "  float r = length (normcoord)+0.01;"
  "  r = pow(r, 0.40)*1.3;"
  "  normcoord = normcoord / r;"
  "  texturecoord = (normcoord + 0.5);"
  "  gl_FragColor = texture2D (tex, texturecoord);"
  "}";

const gchar *stretch_fragment_source_gles2 =
  "varying vec2 v_texcoord;"
  "uniform sampler2D tex;"
  "void main () {"
	"  vec2 texturecoord = v_texcoord.xy;"
  "  vec2 normcoord;"
  "  normcoord = texturecoord - 0.5;"
  "  float r = length (normcoord);"
  "  normcoord *= 2.0 - smoothstep(0.0, 0.35, r);"
  "  texturecoord = normcoord + 0.5;"
  "  gl_FragColor = texture2D (tex, texturecoord);"
  "}";

const gchar *tunnel_fragment_source_gles2 =
  "varying vec2 v_texcoord;"
  "uniform sampler2D tex;"
  "void main () {"
  "  vec2 texturecoord = v_texcoord.xy;"
  "  vec2 normcoord;"
  /* little trick with normalized coords to obtain a circle with
   * rect textures */
  "  normcoord = (texturecoord - 0.5);"
  "  float r = length(normcoord);"
  "  if (r > 0.0)"
  "    normcoord *= clamp (r, 0.0, 0.275) / r;"
  "  texturecoord = normcoord + 0.5;"
  "  gl_FragColor = texture2D (tex, texturecoord);"
  "}";

const gchar *fisheye_fragment_source_gles2 =
  "varying vec2 v_texcoord;"
  "uniform sampler2D tex;"
  "void main () {"
  "  vec2 texturecoord = v_texcoord.xy;"
  "  vec2 normcoord;"
  "  normcoord = texturecoord - 0.5;"
  "  float r = length (normcoord);"
  "  normcoord *= r * 1.41421;" /* sqrt (2) */
  "  texturecoord = normcoord + 0.5;"
  "  gl_FragColor = texture2D (tex, texturecoord);"
  "}";

const gchar *twirl_fragment_source_gles2 =
  "varying vec2 v_texcoord;"
  "uniform sampler2D tex;"
  "void main () {"
  "  vec2 texturecoord = v_texcoord.xy;"
  "  vec2 normcoord;"
  "  normcoord = texturecoord - 0.5;"
  "  float r = length (normcoord);"
  /* calculate rotation angle: maximum (about pi/2) at the origin and
   * gradually decrease it up to 0.6 of each quadrant */
  "  float phi = (1.0 - smoothstep (0.0, 0.3, r)) * 1.6;"
  /* precalculate sin phi and cos phi, save some alu */
  "  float s = sin(phi);"
  "  float c = cos(phi);"
  /* rotate */
  "  normcoord *= mat2(c, s, -s, c);"
  "  texturecoord = normcoord + 0.5;"
  "  gl_FragColor = texture2D (tex, texturecoord);"
  "}";

const gchar *bulge_fragment_source_gles2 =
  "varying vec2 v_texcoord;"
  "uniform sampler2D tex;"
  "void main () {"
  "  vec2 texturecoord = v_texcoord.xy;"
  "  vec2 normcoord;"
  "  normcoord = texturecoord - 0.5;"
  "  float r =  length (normcoord);"
  "  normcoord *= smoothstep (-0.05, 0.25, r);"
  "  texturecoord = normcoord + 0.5;"
  "  gl_FragColor = texture2D (tex, texturecoord);"
  "}";

const gchar *square_fragment_source_gles2 =
  "varying vec2 v_texcoord;"
  "uniform sampler2D tex;"
  "void main () {"
  "  vec2 texturecoord = v_texcoord.xy;"
  "  vec2 normcoord;"
  "  normcoord = texturecoord - 0.5;"
  "  float r = length (normcoord);"
  "  normcoord *= 1.0 + smoothstep(0.125, 0.25, abs(normcoord));"
  "  normcoord /= 2.0; /* zoom amount */"
  "  texturecoord = normcoord + 0.5;"
  "  gl_FragColor = texture2D (tex, texturecoord);"
  "}";

const gchar *luma_threshold_fragment_source_gles2 =
  "varying vec2 v_texcoord;"
  "uniform sampler2D tex;"
  "void main () {"
  "  vec2 texturecoord = v_texcoord.xy;"
  "  vec4 color = texture2D(tex, texturecoord);"
  "  float luma = dot(color.rgb, vec3(0.2125, 0.7154, 0.0721));"    /* BT.709 (from orange book) */
  "  gl_FragColor = vec4 (vec3 (smoothstep (0.30, 0.50, luma)), color.a);"
  "}";

const gchar *sep_sobel_length_fragment_source_gles2 =
  "varying vec2 v_texcoord;"
  "uniform sampler2D tex;"
  "uniform bool invert;"
  "void main () {"
  "  vec4 g = texture2D (tex, v_texcoord.xy);"
  /* restore black background with grey edges */
  "  g -= vec4(0.5, 0.5, 0.0, 0.0);"
  "  float len = length (g);"
  /* little trick to avoid IF operator */
  /* TODO: test if a standalone inverting pass is worth */
  "  gl_FragColor = abs(vec4(vec3(float(invert) - len), 1.0));"
  "}";

const gchar *desaturate_fragment_source_gles2 =
  "varying vec2 v_texcoord;"
  "uniform sampler2D tex;"
  "void main () {"
  "  vec4 color = texture2D (tex, v_texcoord.xy);"
  "  float luma = dot(color.rgb, vec3(0.2125, 0.7154, 0.0721));"
  "  gl_FragColor = vec4(vec3(luma), color.a);"
  "}";

const gchar *sep_sobel_hconv3_fragment_source_gles2 =
  "varying vec2 v_texcoord;"
  "uniform sampler2D tex;"
  "uniform float width;"
  "void main () {"
  "  float w = 1.0 / width;"
  "  vec2 texturecoord[3];"
  "  texturecoord[1] = v_texcoord.xy;"
  "  texturecoord[0] = texturecoord[1] - vec2(w, 0.0);"
  "  texturecoord[2] = texturecoord[1] + vec2(w, 0.0);"
  "  float grad_kern[3];"
  "  grad_kern[0] = 1.0;"
  "  grad_kern[1] = 0.0;"
  "  grad_kern[2] = -1.0;"
  "  float blur_kern[3];"
  "  blur_kern[0] = 0.25;"
  "  blur_kern[1] = 0.5;"
  "  blur_kern[2] = 0.25;"
  "  int i;"
  "  vec4 sum = vec4 (0.0);"
  "  for (i = 0; i < 3; i++) { "
  "    vec4 neighbor = texture2D(tex, texturecoord[i]); "
  "    sum.r = neighbor.r * blur_kern[i] + sum.r;"
  "    sum.g = neighbor.g * grad_kern[i] + sum.g;"
  "  }"
  "  gl_FragColor = sum + vec4(0.0, 0.5, 0.0, 0.0);"
  "}";

const gchar *sep_sobel_vconv3_fragment_source_gles2 =
  "varying vec2 v_texcoord;"
  "uniform sampler2D tex;"
  "uniform float height;"
  "void main () {"
  "  float h = 1.0 / height;"
  "  vec2 texturecoord[3];"
  "  texturecoord[1] = v_texcoord.xy;"
  "  texturecoord[0] = texturecoord[1] - vec2(0.0, h);"
  "  texturecoord[2] = texturecoord[1] + vec2(0.0, h);"
  "  float grad_kern[3];"
  "  grad_kern[0] = 1.0;"
  "  grad_kern[1] = 0.0;"
  "  grad_kern[2] = -1.0;"
  "  float blur_kern[3];"
  "  blur_kern[0] = 0.25;"
  "  blur_kern[1] = 0.5;"
  "  blur_kern[2] = 0.25;"
  "  int i;"
  "  vec4 sum = vec4 (0.0);"
  "  for (i = 0; i < 3; i++) { "
  "    vec4 neighbor = texture2D(tex, texturecoord[i]); "
  "    sum.r = neighbor.r * grad_kern[i] + sum.r;"
  "    sum.g = neighbor.g * blur_kern[i] + sum.g;"
  "  }"
  "  gl_FragColor = sum + vec4(0.5, 0.0, 0.0, 0.0);"
  "}";

const gchar *hconv7_fragment_source_gles2 =
  "varying vec2 v_texcoord;"
  "uniform sampler2D tex;"
  "uniform float kernel[7];"
  "uniform float gauss_width;"
  "void main () {"
  "  float w = 1.0 / gauss_width;"
  "  vec2 texturecoord[7];"
  "  texturecoord[3] = v_texcoord.xy;"
  "  texturecoord[2] = texturecoord[3] - vec2(w, 0.0);"
  "  texturecoord[1] = texturecoord[2] - vec2(w, 0.0);"
  "  texturecoord[0] = texturecoord[1] - vec2(w, 0.0);"
  "  texturecoord[4] = texturecoord[3] + vec2(w, 0.0);"
  "  texturecoord[5] = texturecoord[4] + vec2(w, 0.0);"
  "  texturecoord[6] = texturecoord[5] + vec2(w, 0.0);"
  "  int i;"
  "  vec4 sum = vec4 (0.0);"
  "  for (i = 0; i < 7; i++) { "
  "    vec4 neighbor = texture2D(tex, texturecoord[i]); "
  "    sum += neighbor * kernel[i];"
  "  }"
  "  gl_FragColor = sum;"
  "}";

/* vertical convolution 7x7 */
const gchar *vconv7_fragment_source_gles2 =
  "varying vec2 v_texcoord;"
  "uniform sampler2D tex;"
  "uniform float kernel[7];"
  "uniform float gauss_height;"
  "void main () {"
  "  float h = 1.0 / gauss_height;"
  "  vec2 texturecoord[7];"
  "  texturecoord[3] = v_texcoord.xy;"
  "  texturecoord[2] = texturecoord[3] - vec2(0.0, h);"
  "  texturecoord[1] = texturecoord[2] - vec2(0.0, h);"
  "  texturecoord[0] = texturecoord[1] - vec2(0.0, h);"
  "  texturecoord[4] = texturecoord[3] + vec2(0.0, h);"
  "  texturecoord[5] = texturecoord[4] + vec2(0.0, h);"
  "  texturecoord[6] = texturecoord[5] + vec2(0.0, h);"
  "  int i;"
  "  vec4 sum = vec4 (0.0);"
  "  for (i = 0; i < 7; i++) { "
  "    vec4 neighbor = texture2D(tex, texturecoord[i]);"
  "    sum += neighbor * kernel[i];"
  "  }"
  "  gl_FragColor = sum;"
  "}";

/* TODO: support several blend modes */
const gchar *sum_fragment_source_gles2 =
  "varying vec2 v_texcoord;"
  "uniform sampler2D base;"
  "uniform sampler2D blend;"
  "uniform float alpha;"
  "uniform float beta;"
  "void main () {"
  "  vec4 basecolor = texture2D (base, v_texcoord.xy);"
  "  vec4 blendcolor = texture2D (blend, v_texcoord.xy);"
  "  gl_FragColor = alpha * basecolor + beta * blendcolor;"
  "}";

const gchar *multiply_fragment_source_gles2 =
  "varying vec2 v_texcoord;"
  "uniform sampler2D base;"
  "uniform sampler2D blend;"
  "uniform float alpha;"
  "void main () {"
  "  vec4 basecolor = texture2D (base, v_texcoord.xy);"
  "  vec4 blendcolor = texture2D (blend, v_texcoord.xy);"
  "  gl_FragColor = (1.0 - alpha) * basecolor + alpha * basecolor * blendcolor;"
  "}";

/* lut operations, map luma to tex1d, see orange book (chapter 19) */
const gchar *luma_to_curve_fragment_source_gles2 =
  "varying vec2 v_texcoord;"
  "uniform sampler2D tex;"
  "uniform sampler2D curve;"
  "void main () {"
  "  vec2 texturecoord = v_texcoord.xy;"
  "  vec4 color = texture2D (tex, texturecoord);"
  "  float luma = dot(color.rgb, vec3(0.2125, 0.7154, 0.0721));"
  "  color = texture2D (curve, vec2(luma, 0.0));"
  "  gl_FragColor = color;"
  "}";

/* lut operations, map rgb to tex1d, see orange book (chapter 19) */
const gchar *rgb_to_curve_fragment_source_gles2 =
  "varying vec2 v_texcoord;"
  "uniform sampler2D tex;"
  "uniform sampler2D curve;"
  "void main () {"
  "  vec4 color = texture2D (tex, v_texcoord.xy);"
  "  vec4 outcolor;"
  "  outcolor.r = texture2D (curve, vec2(color.r, 0.0)).r;"
  "  outcolor.g = texture2D (curve, vec2(color.g, 0.0)).g;"
  "  outcolor.b = texture2D (curve, vec2(color.b, 0.0)).b;"
  "  outcolor.a = color.a;"
  "  gl_FragColor = outcolor;"
  "}";

const gchar *sin_fragment_source_gles2 =
  "varying vec2 v_texcoord;"
  "uniform sampler2D tex;"
  "void main () {"
  "  vec4 color = texture2D (tex, vec2(v_texcoord.xy));"
  "  float luma = dot(color.rgb, vec3(0.2125, 0.7154, 0.0721));"
/* calculate hue with the Preucil formula */
  "  float cosh = color.r - 0.5*(color.g + color.b);"
/* sqrt(3)/2 = 0.866 */
  "  float sinh = 0.866*(color.g - color.b);"
/* hue = atan2 h */
  "  float sch = (1.0-sinh)*cosh;"
/* ok this is a little trick I came up because I didn't find any
 * detailed proof of the Preucil formula. The issue is that tan(h) is
 * pi-periodic so the smoothstep thing gives both reds (h = 0) and
 * cyans (h = 180). I don't want to use atan since it requires
 * branching and doesn't work on i915. So take only the right half of
 * the circle where cosine is positive */
/* take a slightly purple color trying to get rid of human skin reds */
/* tanh = +-1.0 for h = +-45, where yellow=60, magenta=-60 */
  "  float a = smoothstep (0.3, 1.0, sch);"
  "  float b = smoothstep (-0.4, -0.1, sinh);"
  "  float mix = a * b;"
  "  gl_FragColor = color * mix + luma * (1.0 - mix);"
  "}";

const gchar *interpolate_fragment_source =
  "varying vec2 v_texcoord;"
  "uniform sampler2D base;"
  "uniform sampler2D blend;"
  "void main () {"
  "vec4 basecolor = texture2D (base, v_texcoord);"
  "vec4 blendcolor = texture2D (blend, v_texcoord);"
  "vec4 white = vec4(1.0);"
  "gl_FragColor = blendcolor + (1.0 - blendcolor.a) * basecolor;"
  "}";

const gchar *texture_interp_fragment_source =
  "varying vec2 v_texcoord;"
  "uniform sampler2D base;"
  "uniform sampler2D blend;"
  "uniform sampler2D alpha;"
  "void main () {"
  "  vec4 basecolor = texture2D (base, v_texcoord);"
  "  vec4 blendcolor = texture2D (blend, v_texcoord);"
  "  vec4 alphacolor = texture2D (alpha, v_texcoord);"
  "  gl_FragColor = (alphacolor * blendcolor) + (1.0 - alphacolor) * basecolor;"
  "}";

const gchar *difference_fragment_source =
  "varying vec2 v_texcoord;"
  "uniform sampler2D saved;"
  "uniform sampler2D current;"
  "void main () {"
  "vec4 savedcolor = texture2D (saved, v_texcoord);"
  "vec4 currentcolor = texture2D (current, v_texcoord);"
  "gl_FragColor = vec4 (step (0.12, length (savedcolor - currentcolor)));"
  "}";

/* This filter is meant as a demo of gst-plugins-gl + glsl
   capabilities. So I'm keeping this shader readable enough. If and
   when this shader will be used in production be careful to hard code
   kernel into the shader and remove unneeded zero multiplications in
   the convolution */
const gchar *conv9_fragment_source_gles2 =
  "varying vec2 v_texcoord;"
  "uniform sampler2D tex;"
  "uniform float kernel[9];"
  "uniform float width, height;"
  "uniform bool invert;"
  "void main () {"
  "  float w = 1.0 / width;"
  "  float h = 1.0 / height;"
  "  vec2 texturecoord[9];"
  "  texturecoord[4] = v_texcoord.xy;"                    /*  0  0 */
  "  texturecoord[5] = texturecoord[4] + vec2(w,   0.0);" /*  1  0 */
  "  texturecoord[2] = texturecoord[5] - vec2(0.0, h);"   /*  1 -1 */
  "  texturecoord[1] = texturecoord[2] - vec2(w,   0.0);" /*  0 -1 */
  "  texturecoord[0] = texturecoord[1] - vec2(w,   0.0);" /* -1 -1 */
  "  texturecoord[3] = texturecoord[0] + vec2(0.0, h);"   /* -1  0 */
  "  texturecoord[6] = texturecoord[3] + vec2(0.0, h);"   /* -1  1 */
  "  texturecoord[7] = texturecoord[6] + vec2(w,   0.0);" /*  0  1 */
  "  texturecoord[8] = texturecoord[7] + vec2(w,   0.0);" /*  1  1 */
  "  int i;"
  "  vec3 sum = vec3 (0.0);"
  "  for (i = 0; i < 9; i++) { "
  "    vec4 neighbor = texture2D (tex, texturecoord[i]);"
  "    sum += neighbor.xyz * kernel[i];"
  "  }"
  "  gl_FragColor = vec4 (abs(sum - vec3(float(invert))), 1.0);"
  "}";

/* *INDENT-ON* */
