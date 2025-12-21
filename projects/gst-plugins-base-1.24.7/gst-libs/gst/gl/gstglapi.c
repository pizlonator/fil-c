/*
 * GStreamer
 * Copyright (C) 2012 Matthew Waters <ystreet00@gmail.com>
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

/**
 * SECTION:gstglapi
 * @title: GstGLAPI
 * @short_description: OpenGL API specific functionality
 * @see_also: #GstGLDisplay, #GstGLContext
 *
 * Provides some helper API for dealing with OpenGL API's and platforms
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gstglapi.h"

/**
 * GstGLFuncs:
 *
 * Structure containing function pointers to OpenGL functions.
 *
 * Each field is named exactly the same as the OpenGL function without the
 * `gl` prefix.
 */

/**
 * gst_gl_api_to_string:
 * @api: a #GstGLAPI to stringify
 *
 * Returns: A space separated string of the OpenGL api's enabled in @api
 */
gchar *
gst_gl_api_to_string (GstGLAPI api)
{
  GString *str = NULL;
  gchar *ret;

  if (api == GST_GL_API_NONE) {
    str = g_string_new ("none");
    goto out;
  } else if (api == GST_GL_API_ANY) {
    str = g_string_new ("any");
    goto out;
  }

  if (api & GST_GL_API_OPENGL) {
    str = g_string_new (GST_GL_API_OPENGL_NAME);
  }
  if (api & GST_GL_API_OPENGL3) {
    if (str) {
      g_string_append (str, " " GST_GL_API_OPENGL3_NAME);
    } else {
      str = g_string_new (GST_GL_API_OPENGL3_NAME);
    }
  }
  if (api & GST_GL_API_GLES1) {
    if (str) {
      g_string_append (str, " " GST_GL_API_GLES1_NAME);
    } else {
      str = g_string_new (GST_GL_API_GLES1_NAME);
    }
  }
  if (api & GST_GL_API_GLES2) {
    if (str) {
      g_string_append (str, " " GST_GL_API_GLES2_NAME);
    } else {
      str = g_string_new (GST_GL_API_GLES2_NAME);
    }
  }

out:
  if (!str)
    str = g_string_new ("unknown");

  ret = g_string_free (str, FALSE);
  return ret;
}

/**
 * gst_gl_api_from_string:
 * @api_s: a space separated string of OpenGL apis
 *
 * Returns: The #GstGLAPI represented by @api_s
 */
GstGLAPI
gst_gl_api_from_string (const gchar * apis_s)
{
  GstGLAPI ret = GST_GL_API_NONE;
  gchar *apis = (gchar *) apis_s;

  if (!apis || apis[0] == '\0' || g_strcmp0 (apis_s, "any") == 0) {
    ret = GST_GL_API_ANY;
  } else if (g_strcmp0 (apis_s, "none") == 0) {
    ret = GST_GL_API_NONE;
  } else {
    while (apis) {
      if (apis[0] == '\0') {
        break;
      } else if (apis[0] == ' ' || apis[0] == ',') {
        apis = &apis[1];
      } else if (g_strstr_len (apis, 7, GST_GL_API_OPENGL3_NAME)) {
        ret |= GST_GL_API_OPENGL3;
        apis = &apis[7];
      } else if (g_strstr_len (apis, 6, GST_GL_API_OPENGL_NAME)) {
        ret |= GST_GL_API_OPENGL;
        apis = &apis[6];
      } else if (g_strstr_len (apis, 5, GST_GL_API_GLES1_NAME)) {
        ret |= GST_GL_API_GLES1;
        apis = &apis[5];
      } else if (g_strstr_len (apis, 5, GST_GL_API_GLES2_NAME)) {
        ret |= GST_GL_API_GLES2;
        apis = &apis[5];
      } else {
        GST_ERROR ("Error parsing \'%s\'", apis);
        break;
      }
    }
  }

  return ret;
}

/**
 * gst_gl_platform_to_string:
 * @platform: a #GstGLPlatform to stringify
 *
 * Returns: A space separated string of the OpenGL platforms enabled in @platform
 */
gchar *
gst_gl_platform_to_string (GstGLPlatform platform)
{
  GString *str = NULL;
  gboolean first_set = FALSE;
  gchar *ret;

  if (platform == GST_GL_PLATFORM_NONE) {
    str = g_string_new ("none");
    goto out;
  } else if (platform == GST_GL_PLATFORM_ANY) {
    str = g_string_new ("any");
    goto out;
  }

  str = g_string_new ("");

#define ADD_PLATFORM(flag,str__) \
  if (platform & flag) { \
    if (first_set) \
      g_string_append_c (str, ' '); \
    str = g_string_append (str, str__); \
    first_set = TRUE; \
  }
  ADD_PLATFORM (GST_GL_PLATFORM_GLX, "glx");
  ADD_PLATFORM (GST_GL_PLATFORM_EGL, "egl");
  ADD_PLATFORM (GST_GL_PLATFORM_WGL, "wgl");
  ADD_PLATFORM (GST_GL_PLATFORM_CGL, "cgl");
  ADD_PLATFORM (GST_GL_PLATFORM_EAGL, "eagl");

#undef ADD_PLATFORM

out:
  if (g_strcmp0 (str->str, "") == 0)
    str = g_string_append (str, "unknown");

  ret = g_string_free (str, FALSE);
  return ret;
}

/**
 * gst_gl_platform_from_string:
 * @platform_s: a space separated string of OpenGL platformss
 *
 * Returns: The #GstGLPlatform represented by @platform_s
 */
GstGLPlatform
gst_gl_platform_from_string (const gchar * platform_s)
{
  GstGLPlatform ret = GST_GL_PLATFORM_NONE;
  gchar *platform = (gchar *) platform_s;

  if (!platform || platform[0] == '\0' || g_strcmp0 (platform_s, "any") == 0) {
    ret = GST_GL_PLATFORM_ANY;
  } else if (g_strcmp0 (platform_s, "none") == 0) {
    ret = GST_GL_PLATFORM_NONE;
  } else {
    while (platform) {
      if (platform[0] == '\0') {
        break;
      } else if (platform[0] == ' ' || platform[0] == ',') {
        platform = &platform[1];
      } else if (g_strstr_len (platform, 3, "glx")) {
        ret |= GST_GL_PLATFORM_GLX;
        platform = &platform[3];
      } else if (g_strstr_len (platform, 3, "egl")) {
        ret |= GST_GL_PLATFORM_EGL;
        platform = &platform[3];
      } else if (g_strstr_len (platform, 3, "wgl")) {
        ret |= GST_GL_PLATFORM_WGL;
        platform = &platform[3];
      } else if (g_strstr_len (platform, 3, "cgl")) {
        ret |= GST_GL_PLATFORM_CGL;
        platform = &platform[3];
      } else if (g_strstr_len (platform, 4, "eagl")) {
        ret |= GST_GL_PLATFORM_EAGL;
        platform = &platform[4];
      } else {
        GST_ERROR ("Error parsing \'%s\'", platform);
        break;
      }
    }
  }

  return ret;
}
