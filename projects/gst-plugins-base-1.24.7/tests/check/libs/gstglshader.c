/* GStreamer
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
#  include "config.h"
#endif

#include <gst/check/gstcheck.h>

#include <gst/gl/gl.h>

#include <stdio.h>

static GstGLDisplay *display;
static GstGLContext *context;

static void
setup (void)
{
  GError *error = NULL;

  display = gst_gl_display_new ();
  context = gst_gl_context_new (display);

  gst_gl_context_create (context, NULL, &error);

  fail_if (error != NULL, "Error creating context: %s\n",
      error ? error->message : "Unknown Error");
}

static void
teardown (void)
{
  gst_object_unref (display);
  gst_object_unref (context);
}

static void
_test_compile_attach_gl (GstGLContext * context, gpointer data)
{
  GstGLShader *shader;
  GstGLSLStage *vert;
  GError *error = NULL;

  shader = gst_gl_shader_new (context);
  vert = gst_glsl_stage_new_default_vertex (context);

  fail_unless (gst_gl_shader_compile_attach_stage (shader, vert, &error));

  gst_object_unref (shader);
}

GST_START_TEST (test_compile_attach)
{
  gst_gl_context_thread_add (context,
      (GstGLContextThreadFunc) _test_compile_attach_gl, NULL);
}

GST_END_TEST;

static void
_test_separate_compile_attach_gl (GstGLContext * context, gpointer data)
{
  GstGLShader *shader;
  GstGLSLStage *vert;
  GError *error = NULL;

  shader = gst_gl_shader_new (context);
  vert = gst_glsl_stage_new_default_vertex (context);

  fail_unless (gst_glsl_stage_compile (vert, &error));
  fail_unless (gst_gl_shader_attach (shader, vert));
  fail_unless (gst_gl_shader_attach (shader, vert));

  gst_object_unref (shader);
}

GST_START_TEST (test_separate_compile_attach)
{
  gst_gl_context_thread_add (context,
      (GstGLContextThreadFunc) _test_separate_compile_attach_gl, NULL);
}

GST_END_TEST;

static void
_test_detach_gl (GstGLContext * context, gpointer data)
{
  GstGLShader *shader;
  GstGLSLStage *vert;
  GError *error = NULL;

  shader = gst_gl_shader_new (context);
  vert = gst_glsl_stage_new_default_vertex (context);

  fail_unless (gst_glsl_stage_compile (vert, &error));
  fail_unless (gst_gl_shader_attach (shader, vert));
  gst_gl_shader_detach (shader, vert);

  gst_object_unref (shader);
}

GST_START_TEST (test_detach)
{
  gst_gl_context_thread_add (context, (GstGLContextThreadFunc) _test_detach_gl,
      NULL);
}

GST_END_TEST;

static void
_test_link_gl (GstGLContext * context, gpointer data)
{
  GstGLShader *shader;
  GstGLSLStage *vert, *frag;
  GError *error = NULL;

  shader = gst_gl_shader_new (context);
  vert = gst_glsl_stage_new_default_vertex (context);
  frag = gst_glsl_stage_new_default_fragment (context);

  fail_unless (gst_gl_shader_compile_attach_stage (shader, vert, &error));
  fail_unless (gst_gl_shader_compile_attach_stage (shader, frag, &error));
  fail_unless (gst_gl_shader_link (shader, &error));
  fail_unless (gst_gl_shader_is_linked (shader));

  gst_object_unref (shader);
}

GST_START_TEST (test_link)
{
  gst_gl_context_thread_add (context, (GstGLContextThreadFunc) _test_link_gl,
      NULL);
}

GST_END_TEST;

static void
_test_default_shader_gl (GstGLContext * context, gpointer data)
{
  GstGLShader *shader;
  GError *error = NULL;

  shader = gst_gl_shader_new_default (context, &error);
  fail_unless (shader != NULL);
  fail_unless (error == NULL);

  gst_gl_shader_use (shader);
  gst_gl_context_clear_shader (context);

  gst_object_unref (shader);
}

GST_START_TEST (test_default_shader)
{
  gst_gl_context_thread_add (context,
      (GstGLContextThreadFunc) _test_default_shader_gl, NULL);
}

GST_END_TEST;

static void
_test_get_attribute_location_gl (GstGLContext * context, gpointer data)
{
  GstGLShader *shader;
  GError *error = NULL;
  gint loc;

  shader = gst_gl_shader_new_default (context, &error);

  gst_gl_shader_use (shader);

  loc = gst_gl_shader_get_attribute_location (shader, "a_position");
  fail_unless (loc != -1);
  loc = gst_gl_shader_get_attribute_location (shader, "a_texcoord");
  fail_unless (loc != -1);
  loc = gst_gl_shader_get_attribute_location (shader, "unused_value_1928374");
  fail_unless (loc == -1);

  gst_object_unref (shader);
}

GST_START_TEST (test_get_attribute_location)
{
  gst_gl_context_thread_add (context,
      (GstGLContextThreadFunc) _test_get_attribute_location_gl, NULL);
}

GST_END_TEST;

static Suite *
gst_gl_shader_suite (void)
{
  Suite *s = suite_create ("GstGLShader");
  TCase *tc_chain = tcase_create ("glshader");

  suite_add_tcase (s, tc_chain);
  tcase_add_checked_fixture (tc_chain, setup, teardown);
  tcase_add_test (tc_chain, test_compile_attach);
  tcase_add_test (tc_chain, test_separate_compile_attach);
  tcase_add_test (tc_chain, test_detach);
  tcase_add_test (tc_chain, test_link);
  tcase_add_test (tc_chain, test_default_shader);
  tcase_add_test (tc_chain, test_get_attribute_location);

  return s;
}

GST_CHECK_MAIN (gst_gl_shader);
