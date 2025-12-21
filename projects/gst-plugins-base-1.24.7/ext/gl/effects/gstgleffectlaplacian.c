/*
 * GStreamer
 * Copyright (C) 2008-2010 Filippo Argiolas <filippo.argiolas@gmail.com>
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

static gfloat kernel[9] = {
  0.0, -1.0, 0.0,
  -1.0, 4.0, -1.0,
  0.0, -1.0, 0.0
};

void
gst_gl_effects_laplacian (GstGLEffects * effects)
{
  GstGLFilter *filter = GST_GL_FILTER (effects);
  GstGLShader *shader;

  shader = gst_gl_effects_get_fragment_shader (effects, "conv0",
      conv9_fragment_source_gles2);
  gst_gl_shader_use (shader);
  gst_gl_shader_set_uniform_1f (shader, "height",
      GST_VIDEO_INFO_HEIGHT (&filter->in_info));
  gst_gl_shader_set_uniform_1f (shader, "width",
      GST_VIDEO_INFO_WIDTH (&filter->in_info));
  gst_gl_shader_set_uniform_1fv (shader, "kernel", 9, kernel);
  gst_gl_shader_set_uniform_1i (shader, "invert", effects->invert);

  gst_gl_filter_render_to_target_with_shader (filter, effects->intexture,
      effects->outtexture, shader);
}
