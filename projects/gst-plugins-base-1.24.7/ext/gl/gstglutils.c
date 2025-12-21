/* 
 * GStreamer
 * Copyright (C) 2016 Matthew Waters <matthew@centricular.com>
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

#include <gst/gl/gstglfuncs.h>

#include "gstglutils.h"

struct _compile_shader
{
  GstGLShader **shader;
  const gchar *vertex_src;
  const gchar *fragment_src;
};

static void
_compile_shader (GstGLContext * context, struct _compile_shader *data)
{
  GstGLShader *shader;
  GstGLSLStage *vert, *frag;
  GError *error = NULL;

  shader = gst_gl_shader_new (context);

  if (data->vertex_src) {
    vert = gst_glsl_stage_new_with_string (context, GL_VERTEX_SHADER,
        GST_GLSL_VERSION_NONE,
        GST_GLSL_PROFILE_ES | GST_GLSL_PROFILE_COMPATIBILITY, data->vertex_src);
    if (!gst_glsl_stage_compile (vert, &error)) {
      GST_ERROR_OBJECT (vert, "%s", error->message);
      gst_object_unref (vert);
      gst_object_unref (shader);
      return;
    }
    if (!gst_gl_shader_attach (shader, vert)) {
      gst_object_unref (shader);
      return;
    }
  }

  if (data->fragment_src) {
    frag = gst_glsl_stage_new_with_string (context, GL_FRAGMENT_SHADER,
        GST_GLSL_VERSION_NONE,
        GST_GLSL_PROFILE_ES | GST_GLSL_PROFILE_COMPATIBILITY,
        data->fragment_src);
    if (!gst_glsl_stage_compile (frag, &error)) {
      GST_ERROR_OBJECT (frag, "%s", error->message);
      gst_object_unref (frag);
      gst_object_unref (shader);
      return;
    }
    if (!gst_gl_shader_attach (shader, frag)) {
      gst_object_unref (shader);
      return;
    }
  }

  if (!gst_gl_shader_link (shader, &error)) {
    GST_ERROR_OBJECT (shader, "%s", error->message);
    g_error_free (error);
    error = NULL;
    gst_gl_context_clear_shader (context);
    gst_object_unref (shader);
    return;
  }

  *data->shader = shader;
}

/* Called by glfilter */
gboolean
gst_gl_context_gen_shader (GstGLContext * context, const gchar * vert_src,
    const gchar * frag_src, GstGLShader ** shader)
{
  struct _compile_shader data;

  g_return_val_if_fail (frag_src != NULL || vert_src != NULL, FALSE);
  g_return_val_if_fail (shader != NULL, FALSE);

  data.shader = shader;
  data.vertex_src = vert_src;
  data.fragment_src = frag_src;

  gst_gl_context_thread_add (context, (GstGLContextThreadFunc) _compile_shader,
      &data);

  return *shader != NULL;
}
