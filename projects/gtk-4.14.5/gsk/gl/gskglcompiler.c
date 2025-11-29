/* gskglcompiler.c
 *
 * Copyright 2020 Christian Hergert <chergert@redhat.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "config.h"

#include <gsk/gskdebugprivate.h>
#include <gio/gio.h>
#include <string.h>

#include "gskglcommandqueueprivate.h"
#include "gskglcompilerprivate.h"
#include "gskglprogramprivate.h"

#define SHADER_VERSION_GLES       "100"
#define SHADER_VERSION_GLES3      "300 es"
#define SHADER_VERSION_GL2_LEGACY "110"
#define SHADER_VERSION_GL3_LEGACY "130"
#define SHADER_VERSION_GL3        "150"

struct _GskGLCompiler
{
  GObject parent_instance;

  GskGLDriver *driver;

  GBytes *all_preamble;
  GBytes *fragment_preamble;
  GBytes *vertex_preamble;
  GBytes *fragment_source;
  GBytes *fragment_suffix;
  GBytes *vertex_source;
  GBytes *vertex_suffix;

  GArray *attrib_locations;

  const char *glsl_version;

  guint gl3 : 1;
  guint gles : 1;
  guint gles3 : 1;
  guint legacy : 1;
  guint debug_shaders : 1;
};

typedef struct _GskGLProgramAttrib
{
  const char *name;
  guint location;
} GskGLProgramAttrib;

static GBytes *empty_bytes;

G_DEFINE_TYPE (GskGLCompiler, gsk_gl_compiler, G_TYPE_OBJECT)

static void
gsk_gl_compiler_finalize (GObject *object)
{
  GskGLCompiler *self = (GskGLCompiler *)object;

  g_clear_pointer (&self->all_preamble, g_bytes_unref);
  g_clear_pointer (&self->fragment_preamble, g_bytes_unref);
  g_clear_pointer (&self->vertex_preamble, g_bytes_unref);
  g_clear_pointer (&self->vertex_suffix, g_bytes_unref);
  g_clear_pointer (&self->fragment_source, g_bytes_unref);
  g_clear_pointer (&self->fragment_suffix, g_bytes_unref);
  g_clear_pointer (&self->vertex_source, g_bytes_unref);
  g_clear_pointer (&self->attrib_locations, g_array_unref);
  g_clear_object (&self->driver);

  G_OBJECT_CLASS (gsk_gl_compiler_parent_class)->finalize (object);
}

static void
gsk_gl_compiler_class_init (GskGLCompilerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gsk_gl_compiler_finalize;

  empty_bytes = g_bytes_new (NULL, 0);
}

static void
gsk_gl_compiler_init (GskGLCompiler *self)
{
  self->glsl_version = "150";
  self->attrib_locations = g_array_new (FALSE, FALSE, sizeof (GskGLProgramAttrib));
  self->all_preamble = g_bytes_ref (empty_bytes);
  self->vertex_preamble = g_bytes_ref (empty_bytes);
  self->fragment_preamble = g_bytes_ref (empty_bytes);
  self->vertex_source = g_bytes_ref (empty_bytes);
  self->vertex_suffix = g_bytes_ref (empty_bytes);
  self->fragment_source = g_bytes_ref (empty_bytes);
  self->fragment_suffix = g_bytes_ref (empty_bytes);
}

GskGLCompiler *
gsk_gl_compiler_new (GskGLDriver *driver,
                     gboolean     debug_shaders)
{
  GskGLCompiler *self;
  GdkGLContext *context;

  g_return_val_if_fail (GSK_IS_GL_DRIVER (driver), NULL);
  g_return_val_if_fail (driver->shared_command_queue != NULL, NULL);

  self = g_object_new (GSK_TYPE_GL_COMPILER, NULL);
  self->driver = g_object_ref (driver);
  self->debug_shaders = !!debug_shaders;

  context = gsk_gl_driver_get_context (self->driver);

  if (gdk_gl_context_get_use_es (context))
    {
      int maj, min;

      /* for OpenGL/ES 3.0+, use "300 es" as our shader version */
      gdk_gl_context_get_version (context, &maj, &min);

      if (maj >= 3)
        {
          self->glsl_version = SHADER_VERSION_GLES3;
          self->gles3 = TRUE;
        }
      else
        {
          self->glsl_version = SHADER_VERSION_GLES;
          self->gles = TRUE;
        }
    }
  else if (gdk_gl_context_is_legacy (context))
    {
      int maj, min;

      gdk_gl_context_get_version (context, &maj, &min);

      /* On Windows, legacy contexts can give us a GL 4.x context */
      if (maj >= 3)
        self->glsl_version = SHADER_VERSION_GL3_LEGACY;
      else
        self->glsl_version = SHADER_VERSION_GL2_LEGACY;

      self->legacy = TRUE;
    }
  else
    {
      self->glsl_version = SHADER_VERSION_GL3;
      self->gl3 = TRUE;
    }

  gsk_gl_command_queue_make_current (self->driver->shared_command_queue);

  return g_steal_pointer (&self);
}

void
gsk_gl_compiler_bind_attribute (GskGLCompiler *self,
                                const char    *name,
                                guint          location)
{
  GskGLProgramAttrib attrib;

  g_return_if_fail (GSK_IS_GL_COMPILER (self));
  g_return_if_fail (name != NULL);
  g_return_if_fail (location < 32);

  attrib.name = g_intern_string (name);
  attrib.location = location;

  g_array_append_val (self->attrib_locations, attrib);
}

void
gsk_gl_compiler_clear_attributes (GskGLCompiler *self)
{
  g_return_if_fail (GSK_IS_GL_COMPILER (self));

  g_array_set_size (self->attrib_locations, 0);
}

void
gsk_gl_compiler_set_preamble (GskGLCompiler     *self,
                              GskGLCompilerKind  kind,
                              GBytes            *preamble_bytes)
{
  GBytes **loc = NULL;

  g_return_if_fail (GSK_IS_GL_COMPILER (self));
  g_return_if_fail (preamble_bytes != NULL);

  if (kind == GSK_GL_COMPILER_ALL)
    loc = &self->all_preamble;
  else if (kind == GSK_GL_COMPILER_FRAGMENT)
    loc = &self->fragment_preamble;
  else if (kind == GSK_GL_COMPILER_VERTEX)
    loc = &self->vertex_preamble;
  else
    g_return_if_reached ();

  g_assert (loc != NULL);

  if (*loc != preamble_bytes)
    {
      g_clear_pointer (loc, g_bytes_unref);
      *loc = preamble_bytes ? g_bytes_ref (preamble_bytes) : NULL;
    }
}

void
gsk_gl_compiler_set_preamble_from_resource (GskGLCompiler     *self,
                                            GskGLCompilerKind  kind,
                                            const char        *resource_path)
{
  GError *error = NULL;
  GBytes *bytes;

  g_return_if_fail (GSK_IS_GL_COMPILER (self));
  g_return_if_fail (kind == GSK_GL_COMPILER_ALL ||
                    kind == GSK_GL_COMPILER_VERTEX ||
                    kind == GSK_GL_COMPILER_FRAGMENT);
  g_return_if_fail (resource_path != NULL);

  bytes = g_resources_lookup_data (resource_path,
                                   G_RESOURCE_LOOKUP_FLAGS_NONE,
                                   &error);

  if (bytes == NULL)
    g_warning ("Cannot set shader from resource: %s", error->message);
  else
    gsk_gl_compiler_set_preamble (self, kind, bytes);

  g_clear_pointer (&bytes, g_bytes_unref);
  g_clear_error (&error);
}

void
gsk_gl_compiler_set_source (GskGLCompiler     *self,
                            GskGLCompilerKind  kind,
                            GBytes            *source_bytes)
{
  GBytes **loc = NULL;

  g_return_if_fail (GSK_IS_GL_COMPILER (self));
  g_return_if_fail (kind == GSK_GL_COMPILER_ALL ||
                    kind == GSK_GL_COMPILER_VERTEX ||
                    kind == GSK_GL_COMPILER_FRAGMENT);

  if (source_bytes == NULL)
    source_bytes = empty_bytes;

  /* If kind is ALL, then we need to split the fragment and
   * vertex shaders from the bytes and assign them individually.
   * This safely scans for FRAGMENT_SHADER and VERTEX_SHADER as
   * specified within the GLSL resources. Some care is taken to
   * use GBytes which reference the original bytes instead of
   * copying them.
   */
  if (kind == GSK_GL_COMPILER_ALL)
    {
      gsize len = 0;
      const char *source;
      const char *vertex_shader_start;
      const char *fragment_shader_start;
      const char *endpos;
      GBytes *fragment_bytes;
      GBytes *vertex_bytes;

      g_clear_pointer (&self->fragment_source, g_bytes_unref);
      g_clear_pointer (&self->vertex_source, g_bytes_unref);

      source = g_bytes_get_data (source_bytes, &len);
      endpos = source + len;
      vertex_shader_start = g_strstr_len (source, len, "VERTEX_SHADER");
      fragment_shader_start = g_strstr_len (source, len, "FRAGMENT_SHADER");

      if (vertex_shader_start == NULL)
        {
          g_warning ("Failed to locate VERTEX_SHADER in shader source");
          return;
        }

      if (fragment_shader_start == NULL)
        {
          g_warning ("Failed to locate FRAGMENT_SHADER in shader source");
          return;
        }

      if (vertex_shader_start > fragment_shader_start)
        {
          g_warning ("VERTEX_SHADER must come before FRAGMENT_SHADER");
          return;
        }

      /* Locate next newlines */
      while (vertex_shader_start < endpos && vertex_shader_start[0] != '\n')
        vertex_shader_start++;
      while (fragment_shader_start < endpos && fragment_shader_start[0] != '\n')
        fragment_shader_start++;

      vertex_bytes = g_bytes_new_from_bytes (source_bytes,
                                             vertex_shader_start - source,
                                             fragment_shader_start - vertex_shader_start);
      fragment_bytes = g_bytes_new_from_bytes (source_bytes,
                                               fragment_shader_start - source,
                                               endpos - fragment_shader_start);

      gsk_gl_compiler_set_source (self, GSK_GL_COMPILER_VERTEX, vertex_bytes);
      gsk_gl_compiler_set_source (self, GSK_GL_COMPILER_FRAGMENT, fragment_bytes);

      g_bytes_unref (fragment_bytes);
      g_bytes_unref (vertex_bytes);

      return;
    }

  if (kind == GSK_GL_COMPILER_FRAGMENT)
    loc = &self->fragment_source;
  else if (kind == GSK_GL_COMPILER_VERTEX)
    loc = &self->vertex_source;
  else
    g_return_if_reached ();

  if (*loc != source_bytes)
    {
      g_clear_pointer (loc, g_bytes_unref);
      *loc = g_bytes_ref (source_bytes);
    }
}

void
gsk_gl_compiler_set_source_from_resource (GskGLCompiler     *self,
                                          GskGLCompilerKind  kind,
                                          const char        *resource_path)
{
  GError *error = NULL;
  GBytes *bytes;

  g_return_if_fail (GSK_IS_GL_COMPILER (self));
  g_return_if_fail (kind == GSK_GL_COMPILER_ALL ||
                    kind == GSK_GL_COMPILER_VERTEX ||
                    kind == GSK_GL_COMPILER_FRAGMENT);
  g_return_if_fail (resource_path != NULL);

  bytes = g_resources_lookup_data (resource_path,
                                   G_RESOURCE_LOOKUP_FLAGS_NONE,
                                   &error);

  if (bytes == NULL)
    g_warning ("Cannot set shader from resource: %s", error->message);
  else
    gsk_gl_compiler_set_source (self, kind, bytes);

  g_clear_pointer (&bytes, g_bytes_unref);
  g_clear_error (&error);
}

void
gsk_gl_compiler_set_suffix (GskGLCompiler     *self,
                            GskGLCompilerKind  kind,
                            GBytes            *suffix_bytes)
{
  GBytes **loc;

  g_return_if_fail (GSK_IS_GL_COMPILER (self));
  g_return_if_fail (kind == GSK_GL_COMPILER_VERTEX ||
                    kind == GSK_GL_COMPILER_FRAGMENT);
  g_return_if_fail (suffix_bytes != NULL);

  if (suffix_bytes == NULL)
    suffix_bytes = empty_bytes;

  if (kind == GSK_GL_COMPILER_FRAGMENT)
    loc = &self->fragment_suffix;
  else if (kind == GSK_GL_COMPILER_VERTEX)
    loc = &self->vertex_suffix;
  else
    g_return_if_reached ();

  if (*loc != suffix_bytes)
    {
      g_clear_pointer (loc, g_bytes_unref);
      *loc = g_bytes_ref (suffix_bytes);
    }
}

void
gsk_gl_compiler_set_suffix_from_resource (GskGLCompiler     *self,
                                          GskGLCompilerKind  kind,
                                          const char        *resource_path)
{
  GError *error = NULL;
  GBytes *bytes;

  g_return_if_fail (GSK_IS_GL_COMPILER (self));
  g_return_if_fail (kind == GSK_GL_COMPILER_VERTEX ||
                    kind == GSK_GL_COMPILER_FRAGMENT);
  g_return_if_fail (resource_path != NULL);

  bytes = g_resources_lookup_data (resource_path,
                                   G_RESOURCE_LOOKUP_FLAGS_NONE,
                                   &error);

  if (bytes == NULL)
    g_warning ("Cannot set suffix from resource: %s", error->message);
  else
    gsk_gl_compiler_set_suffix (self, kind, bytes);

  g_clear_pointer (&bytes, g_bytes_unref);
  g_clear_error (&error);
}

static void
prepend_line_numbers (char    *code,
                      GString *s)
{
  char *p;
  int line;

  p = code;
  line = 1;
  while (*p)
    {
      char *end = strchr (p, '\n');
      if (end)
        end = end + 1; /* Include newline */
      else
        end = p + strlen (p);

      g_string_append_printf (s, "%3d| ", line++);
      g_string_append_len (s, p, end - p);

      p = end;
    }
}

static gboolean
check_shader_error (int      shader_id,
                    GError **error)
{
  GLint status;
  GLint log_len;
  GLint code_len;
  char *buffer;
  char *code;
  GString *s;

  glGetShaderiv (shader_id, GL_COMPILE_STATUS, &status);

  if G_LIKELY (status == GL_TRUE)
    return TRUE;

  glGetShaderiv (shader_id, GL_INFO_LOG_LENGTH, &log_len);
  buffer = g_malloc0 (log_len + 1);
  glGetShaderInfoLog (shader_id, log_len, NULL, buffer);

  glGetShaderiv (shader_id, GL_SHADER_SOURCE_LENGTH, &code_len);
  code = g_malloc0 (code_len + 1);
  glGetShaderSource (shader_id, code_len, NULL, code);

  s = g_string_new ("");
  prepend_line_numbers (code, s);

  g_set_error (error,
               GDK_GL_ERROR,
               GDK_GL_ERROR_COMPILATION_FAILED,
               "Compilation failure in shader.\n"
               "Source Code: %s\n"
               "\n"
               "Error Message:\n"
               "%s\n"
               "\n",
               s->str,
               buffer);

  g_string_free (s, TRUE);
  g_free (buffer);
  g_free (code);

  return FALSE;
}

static void
print_shader_info (const char *prefix,
                   int         shader_id,
                   const char *name)
{
  if (GSK_DEBUG_CHECK(SHADERS))
    {
      int code_len;

      glGetShaderiv (shader_id, GL_SHADER_SOURCE_LENGTH, &code_len);

      if (code_len > 0)
        {
          char *code;
          GString *s;

          code = g_malloc0 (code_len + 1);
          glGetShaderSource (shader_id, code_len, NULL, code);

          s = g_string_new (NULL);
          prepend_line_numbers (code, s);

          g_message ("%s %d, %s:\n%s",
                     prefix, shader_id,
                     name ? name : "unnamed",
                     s->str);
          g_string_free (s,  TRUE);
          g_free (code);
        }
    }
}

static const char *
get_shader_string (GBytes *bytes)
{
  /* 0 length bytes will give us NULL back */
  const char *str = g_bytes_get_data (bytes, NULL);
  return str ? str : "";
}

GskGLProgram *
gsk_gl_compiler_compile (GskGLCompiler  *self,
                         const char     *name,
                         const char     *clip,
                         GError        **error)
{
  char version[32];
  const char *debug = "";
  const char *legacy = "";
  const char *gl3 = "";
  const char *gles = "";
  const char *gles3 = "";
  int program_id;
  int vertex_id;
  int fragment_id;
  int status;

  g_return_val_if_fail (GSK_IS_GL_COMPILER (self), NULL);
  g_return_val_if_fail (self->all_preamble != NULL, NULL);
  g_return_val_if_fail (self->fragment_preamble != NULL, NULL);
  g_return_val_if_fail (self->vertex_preamble != NULL, NULL);
  g_return_val_if_fail (self->fragment_source != NULL, NULL);
  g_return_val_if_fail (self->vertex_source != NULL, NULL);
  g_return_val_if_fail (self->driver != NULL, NULL);

  gsk_gl_command_queue_make_current (self->driver->command_queue);

  g_snprintf (version, sizeof version, "#version %s\n", self->glsl_version);

  if (self->debug_shaders)
    debug = "#define GSK_DEBUG 1\n";

  if (self->legacy)
    legacy = "#define GSK_LEGACY 1\n";

  if (self->gles)
    gles = "#define GSK_GLES 1\n";

  if (self->gles3)
    gles3 = "#define GSK_GLES3 1\n";

  if (self->gl3)
    gl3 = "#define GSK_GL3 1\n";

  vertex_id = glCreateShader (GL_VERTEX_SHADER);
  glShaderSource (vertex_id,
                  11,
                  (const char *[]) {
                    version, debug, legacy, gl3, gles, gles3, clip,
                    get_shader_string (self->all_preamble),
                    get_shader_string (self->vertex_preamble),
                    get_shader_string (self->vertex_source),
                    get_shader_string (self->vertex_suffix),
                  },
                  (int[]) {
                    strlen (version),
                    strlen (debug),
                    strlen (legacy),
                    strlen (gl3),
                    strlen (gles),
                    strlen (gles3),
                    strlen (clip),
                    g_bytes_get_size (self->all_preamble),
                    g_bytes_get_size (self->vertex_preamble),
                    g_bytes_get_size (self->vertex_source),
                    g_bytes_get_size (self->vertex_suffix),
                  });
  glCompileShader (vertex_id);

  if (!check_shader_error (vertex_id, error))
    {
      glDeleteShader (vertex_id);
      return NULL;
    }

  print_shader_info ("Vertex shader", vertex_id, name);

  fragment_id = glCreateShader (GL_FRAGMENT_SHADER);
  glShaderSource (fragment_id,
                  11,
                  (const char *[]) {
                    version, debug, legacy, gl3, gles, gles3, clip,
                    get_shader_string (self->all_preamble),
                    get_shader_string (self->fragment_preamble),
                    get_shader_string (self->fragment_source),
                    get_shader_string (self->fragment_suffix),
                  },
                  (int[]) {
                    strlen (version),
                    strlen (debug),
                    strlen (legacy),
                    strlen (gl3),
                    strlen (gles),
                    strlen (gles3),
                    strlen (clip),
                    g_bytes_get_size (self->all_preamble),
                    g_bytes_get_size (self->fragment_preamble),
                    g_bytes_get_size (self->fragment_source),
                    g_bytes_get_size (self->fragment_suffix),
                  });
  glCompileShader (fragment_id);

  if (!check_shader_error (fragment_id, error))
    {
      glDeleteShader (vertex_id);
      glDeleteShader (fragment_id);
      return NULL;
    }

  print_shader_info ("Fragment shader", fragment_id, name);

  program_id = glCreateProgram ();
  glAttachShader (program_id, vertex_id);
  glAttachShader (program_id, fragment_id);

  for (guint i = 0; i < self->attrib_locations->len; i++)
    {
      const GskGLProgramAttrib *attrib;

      attrib = &g_array_index (self->attrib_locations, GskGLProgramAttrib, i);
      glBindAttribLocation (program_id, attrib->location, attrib->name);
    }

  glLinkProgram (program_id);

  glGetProgramiv (program_id, GL_LINK_STATUS, &status);

  glDetachShader (program_id, vertex_id);
  glDeleteShader (vertex_id);

  glDetachShader (program_id, fragment_id);
  glDeleteShader (fragment_id);

  if (status == GL_FALSE)
    {
      char *buffer = NULL;
      int log_len = 0;

      glGetProgramiv (program_id, GL_INFO_LOG_LENGTH, &log_len);

      if (log_len > 0)
        {
          /* log_len includes NULL */
          buffer = g_malloc0 (log_len);
          glGetProgramInfoLog (program_id, log_len, NULL, buffer);
        }

      g_warning ("Linking failure in shader:\n%s",
                 buffer ? buffer : "");

      g_set_error (error,
                   GDK_GL_ERROR,
                   GDK_GL_ERROR_LINK_FAILED,
                   "Linking failure in shader: %s",
                   buffer ? buffer : "");

      g_free (buffer);

      glDeleteProgram (program_id);

      return NULL;
    }

  return gsk_gl_program_new (self->driver, name, program_id);
}
