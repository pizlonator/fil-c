/* GStreamer
 *
 * Copyright (C) 2014 Matthew Waters <matthew@centricular.com>
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
#  include "config.h"
#endif

#include <gst/check/gstcheck.h>

#include <gst/gl/gl.h>

/* *INDENT-OFF* */
static struct
{
  GstGLAPI api;
  const gchar *str;
} api_strings[] = {
  {GST_GL_API_OPENGL, "opengl"},
  {GST_GL_API_OPENGL3, "opengl3"},
  {GST_GL_API_GLES1, "gles1"},
  {GST_GL_API_GLES2, "gles2"},
  {GST_GL_API_ANY, "any"},
  {GST_GL_API_NONE, "none"},
};

static struct
{
  GstGLAPI api;
  const gchar *str;
} from_api_strings[] = {
  {GST_GL_API_ANY, ""},
  {GST_GL_API_ANY, NULL},
  {GST_GL_API_NONE, "invalid-api"},
};
/* *INDENT-ON* */

GST_START_TEST (gl_api_serialization)
{
  int i;

  for (i = 0; i < G_N_ELEMENTS (api_strings); i++) {
    gchar *new_str;

    new_str = gst_gl_api_to_string (api_strings[i].api);
    GST_DEBUG ("\'%s\' ?= \'%s\'", new_str, api_strings[i].str);
    fail_unless_equals_int (0, g_strcmp0 (new_str, api_strings[i].str));
    fail_unless_equals_int (gst_gl_api_from_string (api_strings[i].str),
        api_strings[i].api);
    g_free (new_str);
  }

  for (i = 0; i < G_N_ELEMENTS (from_api_strings); i++) {
    fail_unless_equals_int (gst_gl_api_from_string (from_api_strings[i].str),
        from_api_strings[i].api);
  }
}

GST_END_TEST;

/* *INDENT-OFF* */
static struct
{
  GstGLPlatform platform;
  const gchar *str;
} platform_strings[] = {
  {GST_GL_PLATFORM_GLX, "glx"},
  {GST_GL_PLATFORM_EGL, "egl"},
  {GST_GL_PLATFORM_WGL, "wgl"},
  {GST_GL_PLATFORM_CGL, "cgl"},
  {GST_GL_PLATFORM_EAGL, "eagl"},
  {GST_GL_PLATFORM_ANY, "any"},
  {GST_GL_PLATFORM_NONE, "none"},
};

static struct
{
  GstGLPlatform platform;
  const gchar *str;
} from_platform_strings[] = {
  {GST_GL_PLATFORM_ANY, ""},
  {GST_GL_PLATFORM_ANY, NULL},
  {GST_GL_PLATFORM_NONE, "invalid-platform"},
};
/* *INDENT-ON* */

GST_START_TEST (gl_platform_serialization)
{
  int i;

  for (i = 0; i < G_N_ELEMENTS (platform_strings); i++) {
    gchar *new_str;

    new_str = gst_gl_platform_to_string (platform_strings[i].platform);
    GST_DEBUG ("\'%s\' ?= \'%s\'", new_str, platform_strings[i].str);
    fail_unless_equals_int (0, g_strcmp0 (new_str, platform_strings[i].str));
    fail_unless_equals_int (gst_gl_platform_from_string (platform_strings
            [i].str), platform_strings[i].platform);
    g_free (new_str);
  }

  for (i = 0; i < G_N_ELEMENTS (from_platform_strings); i++) {
    fail_unless_equals_int (gst_gl_platform_from_string (from_platform_strings
            [i].str), from_platform_strings[i].platform);
  }
}

GST_END_TEST;

static Suite *
gst_gl_color_convert_suite (void)
{
  Suite *s = suite_create ("GstGLAPI");
  TCase *tc_chain = tcase_create ("api");

  suite_add_tcase (s, tc_chain);
  tcase_add_test (tc_chain, gl_platform_serialization);
  tcase_add_test (tc_chain, gl_api_serialization);

  return s;
}

GST_CHECK_MAIN (gst_gl_color_convert);
