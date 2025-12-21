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

#include <gst/gl/gl.h>

#include "gstglsl.h"
#include "gstglsl_private.h"

/**
 * SECTION:gstglsl
 * @title: GstGLSL
 * @short_description: helpers for dealing with OpenGL shaders
 * @see_also: #GstGLSLStage, #GstGLShader
 */

#define GST_CAT_DEFAULT gst_glsl_debug
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);

static void
_init_debug (void)
{
  static gsize _init = 0;

  if (g_once_init_enter (&_init)) {
    GST_DEBUG_CATEGORY_INIT (GST_CAT_DEFAULT, "glsl", 0,
        "OpenGL Shading Language");
    g_once_init_leave (&_init, 1);
  }
}

/**
 * gst_glsl_error_quark:
 *
 * Returns: the quark used for GstGLSL in #GError's
 */
GQuark
gst_glsl_error_quark (void)
{
  return g_quark_from_static_string ("gst-glsl-error-quark");
}

struct glsl_version_string
{
  GstGLSLVersion version;
  const gchar *name;
};

static const struct glsl_version_string glsl_versions[] = {
  /* keep in sync with definition in the header */
  {GST_GLSL_VERSION_100, "100"},
  {GST_GLSL_VERSION_110, "110"},
  {GST_GLSL_VERSION_120, "120"},
  {GST_GLSL_VERSION_130, "130"},
  {GST_GLSL_VERSION_140, "140"},
  {GST_GLSL_VERSION_150, "150"},
  {GST_GLSL_VERSION_300, "300"},
  {GST_GLSL_VERSION_310, "310"},
  {GST_GLSL_VERSION_320, "320"},
  {GST_GLSL_VERSION_330, "330"},
  {GST_GLSL_VERSION_400, "400"},
  {GST_GLSL_VERSION_410, "410"},
  {GST_GLSL_VERSION_420, "420"},
  {GST_GLSL_VERSION_430, "430"},
  {GST_GLSL_VERSION_440, "440"},
  {GST_GLSL_VERSION_450, "450"},
};

struct glsl_profile_string
{
  GstGLSLProfile profile;
  const gchar *name;
};

static const struct glsl_profile_string glsl_profiles[] = {
  /* keep in sync with definition in the header */
  {GST_GLSL_PROFILE_ES, "es"},
  {GST_GLSL_PROFILE_CORE, "core"},
  {GST_GLSL_PROFILE_COMPATIBILITY, "compatibility"},
};

/**
 * gst_glsl_version_to_string:
 * @version: a #GstGLSLVersion
 *
 * Returns: (nullable): the name of @version or %NULL on error
 */
const gchar *
gst_glsl_version_to_string (GstGLSLVersion version)
{
  int i;

  if (version == GST_GLSL_VERSION_NONE)
    return NULL;

  for (i = 0; i < G_N_ELEMENTS (glsl_versions); i++) {
    if (version == glsl_versions[i].version)
      return glsl_versions[i].name;
  }

  return NULL;
}

/**
 * gst_glsl_version_from_string:
 * @string: a GLSL version string
 *
 * Returns: the #GstGLSLVersion of @string or %GST_GLSL_VERSION_NONE on error
 */
GstGLSLVersion
gst_glsl_version_from_string (const gchar * string)
{
  gchar *str;
  int i;

  if (string == NULL)
    return 0;

  str = g_strdup (string);
  str = g_strstrip (str);

  for (i = 0; i < G_N_ELEMENTS (glsl_versions); i++) {
    if (g_strcmp0 (str, glsl_versions[i].name) == 0) {
      g_free (str);
      return glsl_versions[i].version;
    }
  }

  g_free (str);
  return 0;
}

/**
 * gst_glsl_profile_to_string:
 * @profile: a #GstGLSLProfile
 *
 * Returns: (nullable): the name for @profile or %NULL on error
 */
const gchar *
gst_glsl_profile_to_string (GstGLSLProfile profile)
{
  int i;

  if (profile == GST_GLSL_PROFILE_NONE)
    return NULL;

  /* multiple profiles are not allowed */
  if ((profile & (profile - 1)) != 0)
    return NULL;

  for (i = 0; i < G_N_ELEMENTS (glsl_profiles); i++) {
    if (profile == glsl_profiles[i].profile)
      return glsl_profiles[i].name;
  }

  return NULL;
}

/**
 * gst_glsl_profile_from_string:
 * @string: a GLSL version string
 *
 * Returns: the #GstGLSLProfile of @string or %GST_GLSL_PROFILE_NONE on error
 */
GstGLSLProfile
gst_glsl_profile_from_string (const gchar * string)
{
  gchar *str;
  int i;

  if (string == NULL)
    return GST_GLSL_PROFILE_NONE;

  str = g_strdup (string);
  str = g_strstrip (str);

  for (i = 0; i < G_N_ELEMENTS (glsl_profiles); i++) {
    if (g_strcmp0 (str, glsl_profiles[i].name) == 0) {
      g_free (str);
      return glsl_profiles[i].profile;
    }
  }

  g_free (str);
  return GST_GLSL_PROFILE_NONE;
}

static gboolean
_is_valid_version_profile (GstGLSLVersion version, GstGLSLProfile profile)
{
  if (version == GST_GLSL_VERSION_NONE)
    return TRUE;

  /* versions that may not need an explicit profile */
  if (version <= GST_GLSL_VERSION_150 && profile == GST_GLSL_PROFILE_NONE)
    return TRUE;

  /* ES versions require an ES profile */
  if (version == GST_GLSL_VERSION_100 || version == GST_GLSL_VERSION_300
      || version == GST_GLSL_VERSION_310 || version == GST_GLSL_VERSION_320)
    return profile == GST_GLSL_PROFILE_ES;

  /* required profile and no ES profile for normal GL contexts */
  if (version == GST_GLSL_VERSION_150 || version >= GST_GLSL_VERSION_330)
    return profile == GST_GLSL_PROFILE_NONE || profile == GST_GLSL_PROFILE_CORE
        || profile == GST_GLSL_PROFILE_COMPATIBILITY;

  if (version <= GST_GLSL_VERSION_140)
    return profile == GST_GLSL_PROFILE_NONE
        || profile == GST_GLSL_PROFILE_COMPATIBILITY;

  return FALSE;
}

/**
 * gst_glsl_version_profile_to_string:
 * @version: a #GstGLSLVersion
 * @profile: a #GstGLSLVersion
 *
 * Returns: (nullable): the combined GLSL `#version` string for @version and @profile
 */
gchar *
gst_glsl_version_profile_to_string (GstGLSLVersion version,
    GstGLSLProfile profile)
{
  const gchar *version_s, *profile_s;

  if (!_is_valid_version_profile (version, profile))
    return NULL;

  version_s = gst_glsl_version_to_string (version);
  /* no profiles in GL/ES <= 150 */
  if (version <= GST_GLSL_VERSION_140)
    profile_s = NULL;
  else
    profile_s = gst_glsl_profile_to_string (profile);

  if (!version_s)
    return NULL;

  if (profile_s)
    return g_strdup_printf ("%s %s", version_s, profile_s);

  return g_strdup (version_s);
}

static void
_fixup_version_profile (GstGLSLVersion * version, GstGLSLProfile * profile)
{
  if (*version == GST_GLSL_VERSION_100 || *version == GST_GLSL_VERSION_300
      || *version == GST_GLSL_VERSION_310 || *version == GST_GLSL_VERSION_320)
    *profile = GST_GLSL_PROFILE_ES;
  else if (*version <= GST_GLSL_VERSION_140)
    *profile = GST_GLSL_PROFILE_COMPATIBILITY;
  else if (*profile == GST_GLSL_PROFILE_NONE
      && (*version >= GST_GLSL_VERSION_150 || *version >= GST_GLSL_VERSION_330))
    *profile = GST_GLSL_PROFILE_CORE;
}

/* @str points to the start of "#version", "#    version" or "#\tversion", etc */
static const gchar *
_check_valid_version_preprocessor_string (const gchar * str)
{
  gint i = 0;

  if (!str || !str[i])
    return NULL;

  /* there can be whitespace between the '#' and 'version' */
  do {
    i++;
    if (str[i] == '\0' || str[i] == '\n' || str[i] == '\r')
      return NULL;
  } while (g_ascii_isspace (str[i]));
  if (g_strstr_len (&str[i], 7, "version"))
    return &str[i + 7];

  return NULL;
}

/**
 * gst_glsl_version_profile_from_string:
 * @string: a valid GLSL `#version` string
 * @version_ret: (out): resulting #GstGLSLVersion
 * @profile_ret: (out): resulting #GstGLSLVersion
 *
 * Note: this function expects either a `#version` GLSL preprocesser directive
 * or a valid GLSL version and/or profile.
 *
 * Returns: TRUE if a valid `#version` string was found, FALSE otherwise
 */
gboolean
gst_glsl_version_profile_from_string (const gchar * string,
    GstGLSLVersion * version_ret, GstGLSLProfile * profile_ret)
{
  gchar *str, *version_s, *profile_s;
  GstGLSLVersion version = GST_GLSL_VERSION_NONE;
  GstGLSLProfile profile = GST_GLSL_PROFILE_NONE;
  gint i;

  _init_debug ();

  if (!string)
    goto error;

  str = g_strdup (string);
  version_s = g_strstrip (str);

  /* skip possible #version prefix */
  if (str[0] == '#') {
    if (!(version_s =
            (gchar *) _check_valid_version_preprocessor_string (version_s))) {
      GST_WARNING ("Invalid preprocesser directive detected");
      g_free (str);
      goto error;
    }
  }

  version_s = g_strstrip (version_s);

  i = 0;
  while (version_s && version_s[i] != '\0' && g_ascii_isdigit (version_s[i]))
    i++;
  /* wrong version length */
  if (i != 3) {
    GST_WARNING ("version number has the wrong number of digits: %s",
        version_s);
    g_free (str);
    goto error;
  }

  if (version_s[i] != 0) {
    version_s[i] = '\0';
    i++;
    profile_s = &version_s[i];
    profile_s = g_strstrip (profile_s);

    profile = gst_glsl_profile_from_string (profile_s);
  }
  version = gst_glsl_version_from_string (version_s);
  g_free (str);

  /* check whether the parsed data is valid */
  if (!version) {
    GST_WARNING ("Could not map the version number to a valid GLSL version:");
    goto error;
  }
  if (!_is_valid_version_profile (version, profile)) {
    GST_WARNING ("Invalid version/profile combination specified: %s %s",
        gst_glsl_version_to_string (version),
        gst_glsl_profile_to_string (profile));
    goto error;
  }
  /* got a profile when none was expected */
  if (version <= GST_GLSL_VERSION_140 && profile != GST_GLSL_PROFILE_NONE) {
    GST_WARNING
        ("Found a profile (%s) with a version (%s) that does not support "
        "profiles", gst_glsl_version_to_string (version),
        gst_glsl_profile_to_string (profile));
    goto error;
  }

  _fixup_version_profile (&version, &profile);

  if (profile_ret)
    *profile_ret = profile;
  if (version_ret)
    *version_ret = version;

  return TRUE;

error:
  {
    if (profile_ret)
      *profile_ret = GST_GLSL_PROFILE_NONE;
    if (version_ret)
      *version_ret = GST_GLSL_VERSION_NONE;
    return FALSE;
  }
}

/* returns the pointer in @str to the #version declaration */
const gchar *
_gst_glsl_shader_string_find_version (const gchar * str)
{
  gboolean sl_comment = FALSE;
  gboolean ml_comment = FALSE;
  gboolean newline = TRUE;
  gint i = 0;

  _init_debug ();

  /* search for #version while allowing for preceding comments/whitespace as
   * permitted by the GLSL specification */
  while (str && str[i] != '\0' && i < 1024) {
    if (str[i] == '\n' || str[i] == '\r') {
      newline = TRUE;
      sl_comment = FALSE;
      i++;
      continue;
    }

    if (g_ascii_isspace (str[i]))
      goto next;

    if (sl_comment)
      goto next;

    if (ml_comment) {
      if (g_strstr_len (&str[i], 2, "*/")) {
        ml_comment = FALSE;
        i++;
      }
      goto next;
    }

    if (g_strstr_len (&str[i], 2, "//")) {
      sl_comment = TRUE;
      i++;
      goto next;
    }

    if (g_strstr_len (&str[i], 2, "/*")) {
      ml_comment = TRUE;
      i++;
      goto next;
    }

    if (str[i] == '#') {
      if (newline && _check_valid_version_preprocessor_string (&str[i])) {
        GST_DEBUG ("found #version declaration at index %i", i);
        return &str[i];
      }
      break;
    }

  next:
    newline = FALSE;
    i++;
  }

  GST_DEBUG ("no #version declaration found in the first 1K");

  return NULL;
}

/**
 * gst_glsl_string_get_version_profile:
 * @s: string to search for a valid `#version` string
 * @version: (out): resulting #GstGLSLVersion
 * @profile: (out): resulting #GstGLSLProfile
 *
 * Note: this function first searches the first 1 kilobytes for a `#version`
 * preprocessor directive and then executes gst_glsl_version_profile_from_string().
 *
 * Returns: TRUE if a valid `#version` string was found, FALSE otherwise
 */
gboolean
gst_glsl_string_get_version_profile (const gchar * s, GstGLSLVersion * version,
    GstGLSLProfile * profile)
{
  const gchar *version_profile_s;

  version_profile_s = _gst_glsl_shader_string_find_version (s);
  if (!version_profile_s)
    goto error;

  if (!gst_glsl_version_profile_from_string (version_profile_s, version,
          profile))
    goto error;

  return TRUE;

error:
  {
    if (version)
      *version = GST_GLSL_VERSION_NONE;
    if (profile)
      *profile = GST_GLSL_PROFILE_NONE;
    return FALSE;
  }
}

/**
 * gst_gl_version_to_glsl_version:
 * @gl_api: the #GstGLAPI
 * @maj: the major GL version
 * @min: the minor GL version
 *
 * Returns: The minimum supported #GstGLSLVersion available for @gl_api, @maj and @min
 */
GstGLSLVersion
gst_gl_version_to_glsl_version (GstGLAPI gl_api, gint maj, gint min)
{
  g_return_val_if_fail (gl_api != GST_GL_API_NONE, 0);

  _init_debug ();

  if (gl_api & GST_GL_API_GLES2) {
    if (maj == 2 && min == 0)
      return 100;

    if (maj == 3 && min >= 0 && min <= 2)
      return maj * 100 + min * 10;

    GST_WARNING ("unknown GLES version");
    return 0;
  }

  /* versions match for >= 3.3 */
  if (gl_api & (GST_GL_API_OPENGL3 | GST_GL_API_OPENGL)) {
    if (maj > 3 || (maj == 3 && min >= 3))
      return maj * 100 + min * 10;

    if (maj == 3 && min == 2)
      return 150;
    if (maj == 3 && min == 1)
      return 140;
    if (maj == 3 && min == 0)
      return 130;
    if (maj == 2 && min == 1)
      return 120;
    if (maj == 2 && min == 0)
      return 110;

    GST_WARNING ("unknown GL version");
    return 0;
  }

  GST_WARNING ("unknown GL API");
  return 0;
}

/**
 * gst_gl_context_supports_glsl_profile_version:
 * @context: a #GstGLContext
 * @version: a #GstGLSLVersion
 * @profile: a #GstGLSLProfile
 *
 * Returns: Whether @context supports the combination of @version with @profile
 */
gboolean
gst_gl_context_supports_glsl_profile_version (GstGLContext * context,
    GstGLSLVersion version, GstGLSLProfile profile)
{
  g_return_val_if_fail (GST_IS_GL_CONTEXT (context), FALSE);

  if (!_is_valid_version_profile (version, profile))
    return FALSE;

  if (profile != GST_GLSL_PROFILE_NONE) {
    if (gst_gl_context_check_gl_version (context, GST_GL_API_GLES2, 2, 0)) {
      if ((profile & GST_GLSL_PROFILE_ES) == 0)
        return FALSE;
    } else if ((gst_gl_context_get_gl_api (context) & GST_GL_API_OPENGL) != 0) {
      if ((profile & GST_GLSL_PROFILE_COMPATIBILITY) == 0)
        return FALSE;
    } else if ((gst_gl_context_get_gl_api (context) & GST_GL_API_OPENGL3) != 0) {
      /* GL_ARB_es2_compatibility is required for GL3 contexts */
      if ((profile & (GST_GLSL_PROFILE_CORE | GST_GLSL_PROFILE_ES)) == 0)
        return FALSE;
    } else {
      g_assert_not_reached ();
    }
  }

  if (version != GST_GLSL_VERSION_NONE) {
    GstGLAPI gl_api;
    gint maj, min, glsl_version;

    if (gst_gl_context_check_gl_version (context, GST_GL_API_GLES2, 3, 1)) {
      if (version > GST_GLSL_VERSION_310)
        return FALSE;
    } else if (gst_gl_context_check_gl_version (context, GST_GL_API_GLES2, 3,
            0)) {
      if (version > GST_GLSL_VERSION_300)
        return FALSE;
    } else if (gst_gl_context_check_gl_version (context, GST_GL_API_GLES2, 2,
            0)) {
      if (version > GST_GLSL_VERSION_100)
        return FALSE;
    }

    gl_api = gst_gl_context_get_gl_api (context);
    gst_gl_context_get_gl_version (context, &maj, &min);
    glsl_version = gst_gl_version_to_glsl_version (gl_api, maj, min);
    if (version > glsl_version)
      return FALSE;

    if (gst_gl_context_check_gl_version (context, GST_GL_API_OPENGL3, 1, 0))
      /* GL_ARB_es2_compatibility is required for GL3 contexts */
      if (version < GST_GLSL_VERSION_150 && version != GST_GLSL_VERSION_100)
        return FALSE;

    if (gst_gl_context_check_gl_version (context, GST_GL_API_OPENGL, 1, 0)
        && version < GST_GLSL_VERSION_110)
      return FALSE;
  }

  return TRUE;
}

gboolean
_gst_glsl_funcs_fill (GstGLSLFuncs * vtable, GstGLContext * context)
{
  GstGLFuncs *gl = context->gl_vtable;

  if (vtable->initialized)
    return TRUE;

  if (gl->CreateProgram) {
    vtable->CreateProgram = gl->CreateProgram;
    vtable->DeleteProgram = gl->DeleteProgram;
    vtable->UseProgram = gl->UseProgram;

    vtable->CreateShader = gl->CreateShader;
    vtable->DeleteShader = gl->DeleteShader;
    vtable->AttachShader = gl->AttachShader;
    vtable->DetachShader = gl->DetachShader;

    vtable->GetAttachedShaders = gl->GetAttachedShaders;

    vtable->GetShaderInfoLog = gl->GetShaderInfoLog;
    vtable->GetShaderiv = gl->GetShaderiv;
    vtable->GetProgramInfoLog = gl->GetProgramInfoLog;
    vtable->GetProgramiv = gl->GetProgramiv;
  } else if (gl->CreateProgramObject) {
    vtable->CreateProgram = gl->CreateProgramObject;
    vtable->DeleteProgram = gl->DeleteObject;
    vtable->UseProgram = gl->UseProgramObject;

    vtable->CreateShader = gl->CreateShaderObject;
    vtable->DeleteShader = gl->DeleteObject;
    vtable->AttachShader = gl->AttachObject;
    vtable->DetachShader = gl->DetachObject;

    vtable->GetAttachedShaders = gl->GetAttachedObjects;

    vtable->GetShaderInfoLog = gl->GetInfoLog;
    vtable->GetShaderiv = gl->GetObjectParameteriv;
    vtable->GetProgramInfoLog = gl->GetInfoLog;
    vtable->GetProgramiv = gl->GetObjectParameteriv;
  } else {
    vtable->initialized = FALSE;
    return FALSE;
  }

  vtable->initialized = TRUE;
  return TRUE;
}

static gchar *
_mangle_external_image_extension (const gchar * str, GstGLContext * context,
    GstGLTextureTarget from, GstGLTextureTarget to, GstGLSLVersion version,
    GstGLSLProfile profile)
{
  GST_DEBUG ("is oes? %d, profile == ES? %d, version >= 300? %d, "
      "have essl3? %d", to == GST_GL_TEXTURE_TARGET_EXTERNAL_OES,
      profile == GST_GLSL_PROFILE_ES, version >= GST_GLSL_VERSION_300,
      gst_gl_context_check_feature (context,
          "GL_OES_EGL_image_external_essl3"));

  /* replace GL_OES_EGL_image_external with GL_OES_EGL_image_external_essl3 where supported */
  if (to == GST_GL_TEXTURE_TARGET_EXTERNAL_OES && profile == GST_GLSL_PROFILE_ES
      && version >= GST_GLSL_VERSION_300) {
    if (gst_gl_context_check_feature (context,
            "GL_OES_EGL_image_external_essl3")) {
      GRegex *regex = g_regex_new (
          /* '#extension ' with optional spacing */
          "(#[ \\t]*extension[ \\t]+)"
          /* what we're looking to replace */
          "GL_OES_EGL_image_external"
          /* ':' with optional spacing */
          "([ \\t]*:[ \\t]*"
          /* some word like require, disable, etc followed by spacing and a newline */
          "\\S+[ \\t]*\\R)",
          0, 0, NULL);
      gchar *tmp = g_regex_replace (regex, str, -1, 0,
          "\\1GL_OES_EGL_image_external_essl3\\2", 0, NULL);
      g_regex_unref (regex);
      return tmp;
    } else {
      GST_FIXME ("Undefined situation detected. GLES3 supported but "
          "GL_OES_EGL_image_external_essl3 not supported.  Falling back to the "
          "older GL_OES_EGL_image_external extension");
      return g_strdup (str);
    }
  } else {
    return g_strdup (str);
  }
}

static gchar *
_mangle_texture_access (const gchar * str, GstGLContext * context,
    GstGLTextureTarget from, GstGLTextureTarget to, GstGLSLVersion version,
    GstGLSLProfile profile)
{
  const gchar *from_str = NULL, *to_str = NULL;
  gchar *ret, *tmp;
  gchar *regex_find;
  GRegex *regex;

  if (from == GST_GL_TEXTURE_TARGET_2D)
    from_str = "texture2D";
  if (from == GST_GL_TEXTURE_TARGET_RECTANGLE)
    from_str = "texture2DRect";
  if (from == GST_GL_TEXTURE_TARGET_EXTERNAL_OES)
    from_str = "texture2D";

  /* GL3 || gles3 but not when external-oes unless the image_external_essl3 extension is supported */
  if (profile == GST_GLSL_PROFILE_CORE || (profile == GST_GLSL_PROFILE_ES
          && version >= GST_GLSL_VERSION_300
          && (to != GST_GL_TEXTURE_TARGET_EXTERNAL_OES
              || gst_gl_context_check_feature (context,
                  "GL_OES_EGL_image_external_essl3")))) {
    to_str = "texture";
  } else {
    if (to == GST_GL_TEXTURE_TARGET_2D)
      to_str = "texture2D";
    if (to == GST_GL_TEXTURE_TARGET_RECTANGLE)
      to_str = "texture2DRect";
    if (to == GST_GL_TEXTURE_TARGET_EXTERNAL_OES)
      to_str = "texture2D";
  }

  /* followed by any amount of whitespace then a bracket */
  regex_find = g_strdup_printf ("%s(?=\\s*\\()", from_str);
  regex = g_regex_new (regex_find, 0, 0, NULL);
  tmp = g_regex_replace_literal (regex, str, -1, 0, to_str, 0, NULL);
  g_free (regex_find);
  g_regex_unref (regex);

  if (tmp) {
    ret = tmp;
  } else {
    GST_FIXME ("Couldn't mangle texture access successfully from %s to %s",
        from_str, to_str);
    ret = g_strdup (str);
  }

  return ret;
}

static gchar *
_mangle_sampler_type (const gchar * str, GstGLTextureTarget from,
    GstGLTextureTarget to)
{
  const gchar *from_str = NULL, *to_str = NULL;
  gchar *ret, *tmp;
  gchar *regex_find;
  GRegex *regex;

  if (from == GST_GL_TEXTURE_TARGET_2D)
    from_str = "sampler2D";
  if (from == GST_GL_TEXTURE_TARGET_RECTANGLE)
    from_str = "sampler2DRect";
  if (from == GST_GL_TEXTURE_TARGET_EXTERNAL_OES)
    from_str = "samplerExternalOES";

  if (to == GST_GL_TEXTURE_TARGET_2D)
    to_str = "sampler2D";
  if (to == GST_GL_TEXTURE_TARGET_RECTANGLE)
    to_str = "sampler2DRect";
  if (to == GST_GL_TEXTURE_TARGET_EXTERNAL_OES)
    to_str = "samplerExternalOES";

  /* followed by some whitespace  */
  regex_find = g_strdup_printf ("%s(?=\\s)", from_str);
  regex = g_regex_new (regex_find, 0, 0, NULL);
  tmp = g_regex_replace_literal (regex, str, -1, 0, to_str, 0, NULL);
  g_free (regex_find);
  g_regex_unref (regex);

  if (tmp) {
    ret = tmp;
  } else {
    GST_FIXME ("Couldn't mangle sampler type successfully from %s to %s",
        from_str, to_str);
    ret = g_strdup (str);
  }

  return ret;
}

static gchar *
_mangle_varying_attribute (const gchar * str, guint shader_type,
    GstGLSLVersion version, GstGLSLProfile profile)
{
  if (shader_type == GL_VERTEX_SHADER) {
    if (profile == GST_GLSL_PROFILE_CORE || (profile == GST_GLSL_PROFILE_ES
            && version >= GST_GLSL_VERSION_300)) {
      gchar *tmp, *tmp2;
      GRegex *regex;

      /* followed by some whitespace  */
      regex = g_regex_new ("varying(?=\\s)", 0, 0, NULL);
      tmp = g_regex_replace_literal (regex, str, -1, 0, "out", 0, NULL);
      g_regex_unref (regex);

      /* followed by some whitespace  */
      regex = g_regex_new ("attribute(?=\\s)", 0, 0, NULL);
      tmp2 = g_regex_replace_literal (regex, tmp, -1, 0, "in", 0, NULL);
      g_regex_unref (regex);

      g_free (tmp);
      return tmp2;
    }
  } else if (shader_type == GL_FRAGMENT_SHADER) {
    if (profile == GST_GLSL_PROFILE_CORE || (profile == GST_GLSL_PROFILE_ES
            && version >= GST_GLSL_VERSION_300)) {
      gchar *tmp;
      GRegex *regex;

      /* followed by some whitespace  */
      regex = g_regex_new ("varying(?=\\s)", 0, 0, NULL);
      tmp = g_regex_replace_literal (regex, str, -1, 0, "in", 0, NULL);
      g_regex_unref (regex);

      return tmp;
    }
  }
  return g_strdup (str);
}

static gchar *
_mangle_frag_color_data (const gchar * str)
{
  GRegex *regex;
  gchar *ret, *tmp;

  regex = g_regex_new ("gl_FragColor", 0, 0, NULL);
  ret = g_regex_replace_literal (regex, str, -1, 0, "fragColor", 0, NULL);
  g_regex_unref (regex);

  tmp = ret;
  /* search and replace 'gl_FragData[NUM]' into fragColor_NUM */
  regex = g_regex_new ("gl_FragData\\[(\\d+)\\]", 0, 0, NULL);
  ret = g_regex_replace (regex, tmp, -1, 0, "fragColor_\\1", 0, NULL);
  g_regex_unref (regex);
  g_free (tmp);

  return ret;
}

static void
_mangle_version_profile_from_gl_api (GstGLContext * context,
    GstGLTextureTarget from, GstGLTextureTarget to, GstGLSLVersion * version,
    GstGLSLProfile * profile)
{
  GstGLAPI gl_api;
  gint gl_major, gl_minor;

  gl_api = gst_gl_context_get_gl_api (context);
  gst_gl_context_get_gl_version (context, &gl_major, &gl_minor);

  *version = GST_GLSL_VERSION_NONE;
  *profile = GST_GLSL_PROFILE_NONE;

  if (gl_api & GST_GL_API_OPENGL3) {
    if (gl_major > 3 || gl_minor >= 3) {
      *version = GST_GLSL_VERSION_330;
      *profile = GST_GLSL_PROFILE_CORE;
    } else {
      *version = GST_GLSL_VERSION_150;
      *profile = GST_GLSL_PROFILE_NONE;
    }
  } else if (gl_api & GST_GL_API_GLES2) {
    /* We don't know which texture function to use if we have GLES3 and
     * don't have the essl3 extension */
    if (gl_major >= 3 && (to != GST_GL_TEXTURE_TARGET_EXTERNAL_OES
            || gst_gl_context_check_feature (context,
                "GL_OES_EGL_image_external_essl3"))) {
      *version = GST_GLSL_VERSION_300;
      *profile = GST_GLSL_PROFILE_ES;
    } else if (gl_major >= 2) {
      *version = GST_GLSL_VERSION_100;
      *profile = GST_GLSL_PROFILE_ES;
    }
  } else if (gl_api & GST_GL_API_OPENGL) {
    /* We only use GST_GL_API_OPENGL3 from 3.1, but nothing prevents us from
     * using 3.0/1.30 GLSSL with GL2 API */
    if (gl_major >= 3)
      *version = GST_GLSL_VERSION_130;
    else
      *version = GST_GLSL_VERSION_110;
    *profile = GST_GLSL_PROFILE_COMPATIBILITY;
  }
}

gchar *
_gst_glsl_mangle_shader (const gchar * str, guint shader_type,
    GstGLTextureTarget from, GstGLTextureTarget to, GstGLContext * context,
    GstGLSLVersion * version, GstGLSLProfile * profile)
{
  gchar *tmp, *tmp2;

  _init_debug ();

  g_return_val_if_fail (GST_IS_GL_CONTEXT (context), NULL);

  _mangle_version_profile_from_gl_api (context, from, to, version, profile);
  tmp2 =
      _mangle_external_image_extension (str, context, from, to, *version,
      *profile);
  tmp = _mangle_texture_access (tmp2, context, from, to, *version, *profile);
  g_free (tmp2);
  tmp2 = _mangle_sampler_type (tmp, from, to);
  g_free (tmp);
  tmp = _mangle_varying_attribute (tmp2, shader_type, *version, *profile);
  g_free (tmp2);
  if (shader_type == GL_FRAGMENT_SHADER) {
    if ((*profile == GST_GLSL_PROFILE_ES && *version >= GST_GLSL_VERSION_300)
        || (*profile == GST_GLSL_PROFILE_CORE
            && *version >= GST_GLSL_VERSION_150)) {
      tmp2 = _mangle_frag_color_data (tmp);
      g_free (tmp);
      tmp = tmp2;
    }
  }
  return tmp;
}

/**
 * gst_gl_context_supports_precision:
 * @context: a #GstGLContext
 * @version: a #GstGLSLVersion
 * @profile: a #GstGLSLProfile
 *
 * Returns: whether @context supports the 'precision' specifier in GLSL shaders
 *
 * Since: 1.16
 */
gboolean
gst_gl_context_supports_precision (GstGLContext * context,
    GstGLSLVersion version, GstGLSLProfile profile)
{
  gboolean es2 = FALSE;

  g_return_val_if_fail (GST_IS_GL_CONTEXT (context), FALSE);

  if ((profile & GST_GLSL_PROFILE_ES) == 0)
    return FALSE;

  es2 = gst_gl_context_check_gl_version (context, GST_GL_API_GLES2, 2, 0)
      || gst_gl_context_check_feature (context, "GL_ARB_ES2_compatibility");

  return es2 && context->gl_vtable->GetShaderPrecisionFormat;
}

/**
 * gst_gl_context_supports_precision_highp:
 * @context: a #GstGLContext
 * @version: a #GstGLSLVersion
 * @profile: a #GstGLSLProfile
 *
 * Returns: whether @context supports the 'precision highp' specifier in GLSL shaders
 *
 * Since: 1.16
 */
gboolean
gst_gl_context_supports_precision_highp (GstGLContext * context,
    GstGLSLVersion version, GstGLSLProfile profile)
{
  gint v_range[2] = { 0, }
  , v_precision = 0;
  gint f_range[2] = { 0, }
  , f_precision = 0;

  g_return_val_if_fail (GST_IS_GL_CONTEXT (context), FALSE);

  if (!gst_gl_context_supports_precision (context, version, profile))
    return FALSE;

  context->gl_vtable->GetShaderPrecisionFormat (GL_VERTEX_SHADER, GL_HIGH_FLOAT,
      v_range, &v_precision);
  context->gl_vtable->GetShaderPrecisionFormat (GL_FRAGMENT_SHADER,
      GL_HIGH_FLOAT, f_range, &f_precision);

  return v_range[0] != 0 && v_range[1] != 0 && v_precision != 0 &&
      f_range[0] != 0 && f_range[1] != 0 && f_precision != 0;
}
