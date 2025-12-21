/*
 * GStreamer
 * Copyright (C) 2015 Matthew Waters <matthew@centricular.com>
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

#include "gstglshaderstrings.h"

#define MEDIUMP_PRECISION \
   "#ifdef GL_ES\n" \
   "precision mediump float;\n" \
   "#endif\n"

#define HIGHP_PRECISION \
   "#ifdef GL_ES\n" \
   "precision highp float;\n" \
   "#endif\n"

/* *INDENT-OFF* */
const gchar *gst_gl_shader_string_fragment_mediump_precision = 
    MEDIUMP_PRECISION;

const gchar *gst_gl_shader_string_fragment_highp_precision = 
    HIGHP_PRECISION;

const gchar *gst_gl_shader_string_vertex_default =
    "attribute vec4 a_position;\n"
    "attribute vec2 a_texcoord;\n"
    "varying vec2 v_texcoord;\n"
    "void main()\n"
    "{\n"
    "   gl_Position = a_position;\n"
    "   v_texcoord = a_texcoord;\n"
    "}\n";

const gchar *gst_gl_shader_string_vertex_mat4_texture_transform =
    "uniform mat4 u_transformation;\n"
    "attribute vec4 a_position;\n"
    "attribute vec2 a_texcoord;\n"
    "varying vec2 v_texcoord;\n"
    "void main()\n"
    "{\n"
    "   gl_Position = a_position;\n"
    "   v_texcoord = (u_transformation * vec4(a_texcoord, 0, 1)).xy;\n"
    "}\n";

const gchar *gst_gl_shader_string_vertex_mat4_vertex_transform =
    "uniform mat4 u_transformation;\n"
    "attribute vec4 a_position;\n"
    "attribute vec2 a_texcoord;\n"
    "varying vec2 v_texcoord;\n"
    "void main()\n"
    "{\n"
    "   gl_Position = u_transformation * a_position;\n"
    "   v_texcoord = a_texcoord;\n"
    "}\n";

#define DEFAULT_FRAGMENT_BODY \
    "varying vec2 v_texcoord;\n" \
    "uniform sampler2D tex;\n" \
    "void main()\n" \
    "{\n" \
    "  gl_FragColor = texture2D(tex, v_texcoord);\n" \
    "}"
#ifndef GST_REMOVE_DEPRECATED
const gchar *gst_gl_shader_string_fragment_default =
    MEDIUMP_PRECISION
    DEFAULT_FRAGMENT_BODY;
#endif

#define EXTERNAL_FRAGMENT_HEADER \
    "#extension GL_OES_EGL_image_external : require\n"

#define EXTERNAL_FRAGMENT_BODY \
    "varying vec2 v_texcoord;\n" \
    "uniform samplerExternalOES tex;\n" \
    "void main()\n" \
    "{\n" \
    "  gl_FragColor = texture2D(tex, v_texcoord);\n" \
    "}"
#ifndef GST_REMOVE_DEPRECATED
const gchar *gst_gl_shader_string_fragment_external_oes_default =
    EXTERNAL_FRAGMENT_HEADER
    MEDIUMP_PRECISION
    EXTERNAL_FRAGMENT_BODY;
#endif
/* *INDENT-ON* */

/**
 * gst_gl_shader_string_get_highest_precision:
 * @context: a #GstGLContext
 * @version: a #GstGLSLVersion
 * @profile: a #GstGLSLProfile
 *
 * Generates a shader string that defines the precision of float types in
 * GLSL shaders.  This is particularly needed for fragment shaders in a
 * GLSL ES context where there is no default precision specified.
 *
 * Practically, this will return the string 'precision mediump float'
 * or 'precision highp float' depending on if high precision floats are
 * determined to be supported.
 *
 * Returns: a shader string defining the precision of float types based on
 *      @context, @version and @profile
 *
 * Since: 1.16
 */
const gchar *
gst_gl_shader_string_get_highest_precision (GstGLContext * context,
    GstGLSLVersion version, GstGLSLProfile profile)
{
  if (gst_gl_context_supports_precision (context, version, profile)) {
    if (gst_gl_context_supports_precision_highp (context, version, profile))
      return gst_gl_shader_string_fragment_highp_precision;
    else
      return gst_gl_shader_string_fragment_mediump_precision;
  }
  return "";
}

/**
 * gst_gl_shader_string_fragment_get_default:
 * @context: a #GstGLContext
 * @version: a #GstGLSLVersion
 * @profile: a #GstGLSLProfile
 *
 * Returns: a passthrough shader string for copying an input texture to
 *          the output
 *
 * Since: 1.16
 */
gchar *
gst_gl_shader_string_fragment_get_default (GstGLContext * context,
    GstGLSLVersion version, GstGLSLProfile profile)
{
  const gchar *precision =
      gst_gl_shader_string_get_highest_precision (context, version, profile);

  return g_strdup_printf ("%s%s", precision, DEFAULT_FRAGMENT_BODY);
}

/**
 * gst_gl_shader_string_fragment_external_oes_get_default:
 * @context: a #GstGLContext
 * @version: a #GstGLSLVersion
 * @profile: a #GstGLSLProfile
 *
 * Returns: a passthrough shader string for copying an input external-oes
 *          texture to the output
 *
 * Since: 1.16
 */
gchar *
gst_gl_shader_string_fragment_external_oes_get_default (GstGLContext * context,
    GstGLSLVersion version, GstGLSLProfile profile)
{
  const gchar *precision =
      gst_gl_shader_string_get_highest_precision (context, version, profile);

  return g_strdup_printf ("%s%s%s", EXTERNAL_FRAGMENT_HEADER, precision,
      EXTERNAL_FRAGMENT_BODY);
}
