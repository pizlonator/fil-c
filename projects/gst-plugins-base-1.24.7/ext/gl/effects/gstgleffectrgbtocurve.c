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

static void
gst_gl_effects_rgb_to_curve (GstGLEffects * effects,
    const GstGLEffectsCurve * curve, gint curve_index, GstGLMemory * in_tex,
    GstGLMemory * out_tex)
{
  GstGLContext *context = GST_GL_BASE_FILTER (effects)->context;
  GstGLFilter *filter = GST_GL_FILTER (effects);
  const GstGLFuncs *gl = context->gl_vtable;
  GstGLShader *shader;

  shader = gst_gl_effects_get_fragment_shader (effects, "rgb_to_curve",
      rgb_to_curve_fragment_source_gles2);

  if (!shader)
    return;

#if GST_GL_HAVE_OPENGL
  if (USING_OPENGL (context)) {
    gl->MatrixMode (GL_PROJECTION);
    gl->LoadIdentity ();
  }
#endif

  if (effects->curve[curve_index] == 0) {
    /* this parameters are needed to have a right, predictable, mapping */
    gl->GenTextures (1, &effects->curve[curve_index]);

    gl->BindTexture (GL_TEXTURE_2D, effects->curve[curve_index]);
    gl->TexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    gl->TexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    gl->TexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    gl->TexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    gl->TexImage2D (GL_TEXTURE_2D, 0, GL_RGB,
        curve->width, 1, 0, GL_RGB, GL_UNSIGNED_BYTE, curve->pixel_data);
  }

  gst_gl_shader_use (shader);
  gl->ActiveTexture (GL_TEXTURE2);
  gl->BindTexture (GL_TEXTURE_2D, effects->curve[curve_index]);

  gst_gl_shader_set_uniform_1i (shader, "curve", 2);

  gst_gl_filter_render_to_target_with_shader (filter, in_tex, out_tex, shader);
}

void
gst_gl_effects_xpro (GstGLEffects * effects)
{
  gst_gl_effects_rgb_to_curve (effects, &xpro_curve, GST_GL_EFFECTS_CURVE_XPRO,
      effects->intexture, effects->outtexture);
}
