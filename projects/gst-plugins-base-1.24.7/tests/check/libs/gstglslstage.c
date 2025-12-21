/* GStreamer
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
#  include "config.h"
#endif

#include <gst/check/gstcheck.h>

#include <gst/gl/gl.h>
#include <gst/gl/gstglfuncs.h>

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

GST_START_TEST (test_default_vertex)
{
  GstGLSLStage *stage;
  GError *error = NULL;

  stage = gst_glsl_stage_new_default_vertex (context);
  fail_unless (stage != NULL);
  fail_unless (GL_VERTEX_SHADER == gst_glsl_stage_get_shader_type (stage));

  fail_unless (gst_glsl_stage_compile (stage, &error));

  gst_object_unref (stage);
}

GST_END_TEST;

static void
create_frag_shader (GstGLContext * context, GstGLSLStage ** stage)
{
  *stage = gst_glsl_stage_new_default_fragment (context);
}

GST_START_TEST (test_default_fragment)
{
  GstGLSLStage *stage;
  GError *error = NULL;

  gst_gl_context_thread_add (context,
      (GstGLContextThreadFunc) create_frag_shader, &stage);

  fail_unless (stage != NULL);
  fail_unless (GL_FRAGMENT_SHADER == gst_glsl_stage_get_shader_type (stage));

  fail_unless (gst_glsl_stage_compile (stage, &error));

  gst_object_unref (stage);
}

GST_END_TEST;

static Suite *
gst_gl_upload_suite (void)
{
  Suite *s = suite_create ("GstGLSL");
  TCase *tc_chain = tcase_create ("glsl");

  suite_add_tcase (s, tc_chain);
  tcase_add_checked_fixture (tc_chain, setup, teardown);
  tcase_add_test (tc_chain, test_default_vertex);
  tcase_add_test (tc_chain, test_default_fragment);

  return s;
}

GST_CHECK_MAIN (gst_gl_upload);
