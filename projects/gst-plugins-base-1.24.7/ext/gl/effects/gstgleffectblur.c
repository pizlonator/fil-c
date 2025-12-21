/*
 * GStreamer
 * Copyright (C) 2008 Filippo Argiolas <filippo.argiolas@gmail.com>
 * Copyright (C) 2015 Michał Dębski <debski.mi.zd@gmail.com>
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

static gpointer
init_kernel (gpointer data)
{
  float *kernel = g_malloc (sizeof (gfloat) * 9);
  fill_gaussian_kernel (kernel, 7, 3.f);
  return kernel;
}

static float *
gst_gl_effects_blur_kernel (void)
{
  /* gaussian kernel (well, actually vector), size 9, standard
   * deviation 3.0 */
  /* FIXME: make this a runtime property */
  static GOnce my_once = G_ONCE_INIT;

  g_once (&my_once, init_kernel, NULL);
  return my_once.retval;
}

void
gst_gl_effects_blur (GstGLEffects * effects)
{
  GstGLFilter *filter = GST_GL_FILTER (effects);
  GstGLShader *shader;

  shader = gst_gl_effects_get_fragment_shader (effects, "hconv0",
      hconv7_fragment_source_gles2);
  gst_gl_shader_use (shader);
  gst_gl_shader_set_uniform_1f (shader, "gauss_width",
      GST_VIDEO_INFO_WIDTH (&filter->in_info));
  gst_gl_shader_set_uniform_1fv (shader, "kernel", 7,
      gst_gl_effects_blur_kernel ());
  gst_gl_filter_render_to_target_with_shader (filter, effects->intexture,
      effects->midtexture[0], shader);

  shader = gst_gl_effects_get_fragment_shader (effects, "vconv0",
      vconv7_fragment_source_gles2);
  gst_gl_shader_use (shader);
  gst_gl_shader_set_uniform_1f (shader, "gauss_height",
      GST_VIDEO_INFO_HEIGHT (&filter->in_info));
  gst_gl_shader_set_uniform_1fv (shader, "kernel", 7,
      gst_gl_effects_blur_kernel ());
  gst_gl_filter_render_to_target_with_shader (filter, effects->midtexture[0],
      effects->outtexture, shader);
}
