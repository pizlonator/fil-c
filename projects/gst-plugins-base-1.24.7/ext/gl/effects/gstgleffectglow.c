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

static gboolean kernel_ready = FALSE;
static float gauss_kernel[7];

void
gst_gl_effects_glow (GstGLEffects * effects)
{
  const GstGLFuncs *gl = GST_GL_BASE_FILTER (effects)->context->gl_vtable;
  GstGLFilter *filter = GST_GL_FILTER (effects);
  GstGLShader *shader;

  if (!kernel_ready) {
    fill_gaussian_kernel (gauss_kernel, 7, 10.0);
    kernel_ready = TRUE;
  }

  /* threshold */
  shader = gst_gl_effects_get_fragment_shader (effects, "luma_threshold",
      luma_threshold_fragment_source_gles2);
  gst_gl_filter_render_to_target_with_shader (filter, effects->intexture,
      effects->midtexture[0], shader);

  /* blur */
  shader = gst_gl_effects_get_fragment_shader (effects, "hconv7",
      hconv7_fragment_source_gles2);
  gst_gl_shader_use (shader);
  gst_gl_shader_set_uniform_1fv (shader, "kernel", 7, gauss_kernel);
  gst_gl_shader_set_uniform_1f (shader, "gauss_width",
      GST_VIDEO_INFO_WIDTH (&filter->out_info));
  gst_gl_filter_render_to_target_with_shader (filter, effects->midtexture[0],
      effects->midtexture[1], shader);

  shader = gst_gl_effects_get_fragment_shader (effects, "vconv7",
      vconv7_fragment_source_gles2);
  gst_gl_shader_use (shader);
  gst_gl_shader_set_uniform_1fv (shader, "kernel", 7, gauss_kernel);
  gst_gl_shader_set_uniform_1f (shader, "gauss_height",
      GST_VIDEO_INFO_HEIGHT (&filter->out_info));
  gst_gl_filter_render_to_target_with_shader (filter, effects->midtexture[1],
      effects->midtexture[2], shader);

  /* add blurred luma to intexture */
  shader = gst_gl_effects_get_fragment_shader (effects, "sum",
      sum_fragment_source_gles2);
  gst_gl_shader_use (shader);

  gl->ActiveTexture (GL_TEXTURE2);
  gl->BindTexture (GL_TEXTURE_2D,
      gst_gl_memory_get_texture_id (effects->intexture));

  gst_gl_shader_set_uniform_1f (shader, "alpha", 1.0f);
  gst_gl_shader_set_uniform_1i (shader, "base", 2);

  gl->ActiveTexture (GL_TEXTURE1);
  gl->BindTexture (GL_TEXTURE_2D,
      gst_gl_memory_get_texture_id (effects->midtexture[2]));

  gst_gl_shader_set_uniform_1f (shader, "beta", (gfloat) 1 / 3.5f);
  gst_gl_shader_set_uniform_1i (shader, "blend", 1);
  gst_gl_filter_render_to_target_with_shader (filter, effects->midtexture[2],
      effects->outtexture, shader);
}
