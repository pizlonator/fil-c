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

#ifndef __GST_GLSL_PRIVATE_H__
#define __GST_GLSL_PRIVATE_H__

#include <gst/gl/gstgl_fwd.h>
#include <gst/gl/gstglfuncs.h>

G_BEGIN_DECLS

#ifndef GL_COMPILE_STATUS
#define GL_COMPILE_STATUS             0x8B81
#endif
#ifndef GLhandleARB
#define GLhandleARB GLuint
#endif

typedef struct _GstGLSLFuncs
{
  gboolean initialized;

  GLuint (GSTGLAPI *CreateProgram) (void);
  void (GSTGLAPI *DeleteProgram) (GLuint program);
  void (GSTGLAPI *UseProgram) (GLuint program);
  void (GSTGLAPI *GetAttachedShaders) (GLuint program, GLsizei maxcount,
      GLsizei * count, GLuint * shaders);

  GLuint (GSTGLAPI *CreateShader) (GLenum shaderType);
  void (GSTGLAPI *DeleteShader) (GLuint shader);
  void (GSTGLAPI *AttachShader) (GLuint program, GLuint shader);
  void (GSTGLAPI *DetachShader) (GLuint program, GLuint shader);

  void (GSTGLAPI *GetShaderiv) (GLuint program, GLenum pname, GLint * params);
  void (GSTGLAPI *GetProgramiv) (GLuint program, GLenum pname, GLint * params);
  void (GSTGLAPI *GetShaderInfoLog) (GLuint shader, GLsizei maxLength,
      GLsizei * length, char *log);
  void (GSTGLAPI *GetProgramInfoLog) (GLuint shader, GLsizei maxLength,
      GLsizei * length, char *log);
} GstGLSLFuncs;

G_GNUC_INTERNAL gboolean _gst_glsl_funcs_fill (GstGLSLFuncs * vtable, GstGLContext * context);
G_GNUC_INTERNAL const gchar * _gst_glsl_shader_string_find_version (const gchar * str);

G_GNUC_INTERNAL gchar *
_gst_glsl_mangle_shader (const gchar * str, guint shader_type, GstGLTextureTarget from,
    GstGLTextureTarget to, GstGLContext * context, GstGLSLVersion * version, GstGLSLProfile * profile);

G_END_DECLS

#endif /* __GST_GLSL_PRIVATE_H__ */
