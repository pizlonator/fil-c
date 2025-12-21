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

#include "../gstgleffects.h"
#include "gstgleffectscurves.h"
#include "gstgleffectlumatocurve.h"

static gboolean kernel_ready = FALSE;
static float gauss_kernel[7];

void
gst_gl_effects_xray (GstGLEffects * effects)
{
  const GstGLFuncs *gl = GST_GL_BASE_FILTER (effects)->context->gl_vtable;
  GstGLFilter *filter = GST_GL_FILTER (effects);
  GstGLShader *shader;

  if (!kernel_ready) {
    fill_gaussian_kernel (gauss_kernel, 7, 1.5);
    kernel_ready = TRUE;
  }

  /* map luma to xray curve */
  gst_gl_effects_luma_to_curve (effects, &xray_curve, GST_GL_EFFECTS_CURVE_XRAY,
      effects->intexture, effects->midtexture[0]);

  /* horizontal blur */
  shader = gst_gl_effects_get_fragment_shader (effects, "hconv7",
      hconv7_fragment_source_gles2);
  gst_gl_shader_use (shader);
  gst_gl_shader_set_uniform_1fv (shader, "kernel", 9, gauss_kernel);
  gst_gl_shader_set_uniform_1f (shader, "gauss_width",
      GST_VIDEO_INFO_WIDTH (&filter->in_info));
  gst_gl_filter_render_to_target_with_shader (filter, effects->midtexture[0],
      effects->midtexture[1], shader);

  /* vertical blur */
  shader = gst_gl_effects_get_fragment_shader (effects, "vconv7",
      vconv7_fragment_source_gles2);
  gst_gl_shader_use (shader);
  gst_gl_shader_set_uniform_1fv (shader, "kernel", 9, gauss_kernel);
  gst_gl_shader_set_uniform_1f (shader, "gauss_height",
      GST_VIDEO_INFO_HEIGHT (&filter->out_info));
  gst_gl_filter_render_to_target_with_shader (filter, effects->midtexture[1],
      effects->midtexture[2], shader);

  /* detect edges with Sobel */
  /* the old version used edges from the blurred texture, this uses
   * the ones from original texture, still not sure what I like
   * more. This one gives better edges obviously but behaves badly
   * with noise */
  /* desaturate */
  shader = gst_gl_effects_get_fragment_shader (effects, "desaturate",
      desaturate_fragment_source_gles2);
  gst_gl_filter_render_to_target_with_shader (filter, effects->intexture,
      effects->midtexture[3], shader);

  /* horizontal convolution */
  shader = gst_gl_effects_get_fragment_shader (effects, "sobel_hconv3",
      sep_sobel_hconv3_fragment_source_gles2);
  gst_gl_shader_use (shader);
  gst_gl_shader_set_uniform_1f (shader, "width",
      GST_VIDEO_INFO_WIDTH (&filter->out_info));
  gst_gl_filter_render_to_target_with_shader (filter, effects->midtexture[3],
      effects->midtexture[4], shader);

  /* vertical convolution */
  shader = gst_gl_effects_get_fragment_shader (effects, "sobel_vconv3",
      sep_sobel_vconv3_fragment_source_gles2);
  gst_gl_shader_use (shader);
  gst_gl_shader_set_uniform_1f (shader, "height",
      GST_VIDEO_INFO_HEIGHT (&filter->out_info));
  gst_gl_filter_render_to_target_with_shader (filter, effects->midtexture[4],
      effects->midtexture[3], shader);

  /* gradient length */
  shader = gst_gl_effects_get_fragment_shader (effects, "sobel_length",
      sep_sobel_length_fragment_source_gles2);
  gst_gl_shader_use (shader);
  gst_gl_shader_set_uniform_1i (shader, "invert", TRUE);
  gst_gl_filter_render_to_target_with_shader (filter, effects->midtexture[3],
      effects->midtexture[4], shader);

  /* multiply edges with the blurred image */
  shader = gst_gl_effects_get_fragment_shader (effects, "multiply",
      multiply_fragment_source_gles2);
  gst_gl_shader_use (shader);

  gl->ActiveTexture (GL_TEXTURE2);
  gl->BindTexture (GL_TEXTURE_2D,
      gst_gl_memory_get_texture_id (effects->midtexture[2]));

  gst_gl_shader_set_uniform_1i (shader, "base", 2);

  gl->ActiveTexture (GL_TEXTURE1);
  gl->BindTexture (GL_TEXTURE_2D,
      gst_gl_memory_get_texture_id (effects->midtexture[4]));

  gst_gl_shader_set_uniform_1f (shader, "alpha", (gfloat) 0.5f);
  gst_gl_shader_set_uniform_1i (shader, "blend", 1);

  gst_gl_filter_render_to_target_with_shader (filter, effects->midtexture[4],
      effects->outtexture, shader);
}
