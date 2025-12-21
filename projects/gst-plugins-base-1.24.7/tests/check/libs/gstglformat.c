/* GStreamer
 *
 * unit test for state changes on all elements
 *
 * Copyright (C) <2017> Julien Isorce <julien.isorce@gmail.com>
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

static GstGLDisplay *display;
static GstGLContext *context;

static void
setup (void)
{
  display = gst_gl_display_new ();
  context = gst_gl_context_new (display);
  gst_gl_context_create (context, 0, NULL);
}

static void
teardown (void)
{
  gst_object_unref (context);
  gst_object_unref (display);
}

/* keep in sync with the list in gstglformat.h */
static const struct
{
  GstGLFormat format;
  guint gl_type;
  guint n_bytes;
} formats[] = {
  /* *INDENT-OFF* */
  {GST_GL_LUMINANCE, GL_UNSIGNED_BYTE, 1},
  {GST_GL_ALPHA, GL_UNSIGNED_BYTE, 1},
  {GST_GL_LUMINANCE_ALPHA, GL_UNSIGNED_BYTE, 2},
  {GST_GL_RED, GL_UNSIGNED_BYTE, 1},
  {GST_GL_R8, GL_UNSIGNED_BYTE, 1},
  {GST_GL_RG, GL_UNSIGNED_BYTE, 2},
  {GST_GL_RG8, GL_UNSIGNED_BYTE, 2},
  {GST_GL_RGB, GL_UNSIGNED_BYTE, 3},
  {GST_GL_RGB8, GL_UNSIGNED_BYTE, 3},
  {GST_GL_RGB565, GL_UNSIGNED_SHORT_5_6_5, 2},
  {GST_GL_RGB16, GL_UNSIGNED_SHORT, 6},
  {GST_GL_RGBA, GL_UNSIGNED_BYTE, 4},
  {GST_GL_RGBA8, GL_UNSIGNED_BYTE, 4},
  {GST_GL_RGBA16, GL_UNSIGNED_SHORT, 8},
/*  {GST_GL_DEPTH_COMPONENT16, GL_UNSIGNED_BYTE, 2},
  {GST_GL_DEPTH24_STENCIL8, GL_UNSIGNED_BYTE, 4},*/
  /* *INDENT-ON* */
};

GST_START_TEST (test_format_n_bytes)
{
  int i;

  for (i = 0; i < G_N_ELEMENTS (formats); i++) {
    GST_DEBUG ("idx %i: expected %u, args format 0x%x, gl type 0x%x", i,
        formats[i].n_bytes, formats[i].format, formats[i].gl_type);
    fail_unless_equals_int (formats[i].n_bytes,
        gst_gl_format_type_n_bytes (formats[i].format, formats[i].gl_type));
  }
}

GST_END_TEST;

/* keep in sync with the list in gstglformat.h */
static const struct
{
  GstGLFormat format;
  GstGLFormat unsized_format;
  guint gl_type;
} sized_formats[] = {
  /* *INDENT-OFF* */
  {GST_GL_LUMINANCE, GST_GL_LUMINANCE, GL_UNSIGNED_BYTE},
  {GST_GL_ALPHA, GST_GL_ALPHA, GL_UNSIGNED_BYTE},
  {GST_GL_LUMINANCE_ALPHA, GST_GL_LUMINANCE_ALPHA, GL_UNSIGNED_BYTE},
/*  {GST_GL_R8, GST_GL_RED, GL_UNSIGNED_BYTE}, can be either R8 or RED depending on extensions and GL version */
/*  {GST_GL_R8, GST_GL_R8, GL_UNSIGNED_BYTE}, can be either R8 or RED depending on extensions and GL version */
/*  {GST_GL_RG8, GST_GL_RG, GL_UNSIGNED_BYTE}, can be either RG8 or RG depending on extensions and GL version */
/*  {GST_GL_RG8, GST_GL_RG8, GL_UNSIGNED_BYTE}, can be either RG8 or RG depending on extensions and GL version */
  {GST_GL_RGB8, GST_GL_RGB, GL_UNSIGNED_BYTE},
  {GST_GL_RGB8, GST_GL_RGB8, GL_UNSIGNED_BYTE},
  {GST_GL_RGB565, GST_GL_RGB, GL_UNSIGNED_SHORT_5_6_5},
  {GST_GL_RGB565, GST_GL_RGB565, GL_UNSIGNED_SHORT_5_6_5},
  {GST_GL_RGB16, GST_GL_RGB, GL_UNSIGNED_SHORT},
  {GST_GL_RGB16, GST_GL_RGB16, GL_UNSIGNED_SHORT},
  {GST_GL_RGBA8, GST_GL_RGBA, GL_UNSIGNED_BYTE},
  {GST_GL_RGBA8, GST_GL_RGBA8, GL_UNSIGNED_BYTE},
  {GST_GL_RGBA16, GST_GL_RGBA, GL_UNSIGNED_SHORT},
  {GST_GL_RGBA16, GST_GL_RGBA16, GL_UNSIGNED_SHORT},
/*  {GST_GL_DEPTH_COMPONENT16, GST_GL_DEPTH_COMPONENT16, GL_UNSIGNED_BYTE},
  {GST_GL_DEPTH24_STENCIL8, GST_GL_DEPTH24_STENCIL8, GL_UNSIGNED_BYTE},*/
  /* *INDENT-ON* */
};

GST_START_TEST (test_sized_from_unsized)
{
  int i;

  for (i = 0; i < G_N_ELEMENTS (sized_formats); i++) {
    GST_DEBUG ("idx %i: expected 0x%x, args format 0x%x, gl type 0x%x", i,
        sized_formats[i].format, sized_formats[i].unsized_format,
        sized_formats[i].gl_type);
    fail_unless_equals_int (sized_formats[i].format,
        gst_gl_sized_gl_format_from_gl_format_type (context,
            sized_formats[i].unsized_format, sized_formats[i].gl_type));
  }
}

GST_END_TEST;

/* keep in sync with the list in gstglformat.h */
static const struct
{
  GstGLFormat unsized_format;
  guint gl_type;
  GstGLFormat format;
} unsized_formats[] = {
  /* *INDENT-OFF* */
  {GST_GL_LUMINANCE, GL_UNSIGNED_BYTE, GST_GL_LUMINANCE},
  {GST_GL_ALPHA, GL_UNSIGNED_BYTE, GST_GL_ALPHA},
  {GST_GL_LUMINANCE_ALPHA, GL_UNSIGNED_BYTE, GST_GL_LUMINANCE_ALPHA},
  {GST_GL_RED, GL_UNSIGNED_BYTE, GST_GL_RED},
  {GST_GL_RED, GL_UNSIGNED_BYTE, GST_GL_R8},
  {GST_GL_RG, GL_UNSIGNED_BYTE, GST_GL_RG},
  {GST_GL_RG, GL_UNSIGNED_BYTE, GST_GL_RG8},
  {GST_GL_RGB, GL_UNSIGNED_BYTE, GST_GL_RGB},
  {GST_GL_RGB, GL_UNSIGNED_BYTE, GST_GL_RGB8},
  {GST_GL_RGB, GL_UNSIGNED_SHORT_5_6_5, GST_GL_RGB565},
  {GST_GL_RGB, GL_UNSIGNED_SHORT, GST_GL_RGB16},
  {GST_GL_RGBA, GL_UNSIGNED_BYTE, GST_GL_RGBA},
  {GST_GL_RGBA, GL_UNSIGNED_BYTE, GST_GL_RGBA8},
  {GST_GL_RGBA, GL_UNSIGNED_SHORT, GST_GL_RGBA16},
/*  {GST_GL_DEPTH_COMPONENT16, GL_UNSIGNED_BYTE, GST_GL_DEPTH_COMPONENT16},
  {GST_GL_DEPTH24_STENCIL8, GL_UNSIGNED_BYTE, GST_GL_DEPTH24_STENCIL8},*/
  /* *INDENT-ON* */
};

GST_START_TEST (test_unsized_from_sized)
{
  int i;

  for (i = 0; i < G_N_ELEMENTS (unsized_formats); i++) {
    GstGLFormat unsized_format;
    guint gl_type;

    GST_DEBUG ("idx %i: expected 0x%x 0x%x, args format 0x%x", i,
        unsized_formats[i].unsized_format, unsized_formats[i].gl_type,
        unsized_formats[i].format);
    gst_gl_format_type_from_sized_gl_format (unsized_formats[i].format,
        &unsized_format, &gl_type);

    fail_unless_equals_int (unsized_formats[i].unsized_format, unsized_format);
    fail_unless_equals_int (unsized_formats[i].gl_type, gl_type);
  }
}

GST_END_TEST;

static GstGLTextureTarget texture_targets[] = {
  GST_GL_TEXTURE_TARGET_2D,
  GST_GL_TEXTURE_TARGET_RECTANGLE,
  GST_GL_TEXTURE_TARGET_EXTERNAL_OES,
};

GST_START_TEST (test_texture_target_strings)
{
  int i;

  for (i = 0; i < G_N_ELEMENTS (texture_targets); i++) {
    GstGLTextureTarget res;
    const gchar *str;

    str = gst_gl_texture_target_to_string (texture_targets[i]);
    res = gst_gl_texture_target_from_string (str);

    GST_DEBUG ("from %u to \'%s\' to %u", texture_targets[i], str, res);

    fail_unless_equals_int (texture_targets[i], res);
  }
}

GST_END_TEST;

GST_START_TEST (test_texture_target_gl)
{
  int i;

  for (i = 0; i < G_N_ELEMENTS (texture_targets); i++) {
    GstGLTextureTarget res;
    guint gl;

    gl = gst_gl_texture_target_to_gl (texture_targets[i]);
    res = gst_gl_texture_target_from_gl (gl);

    GST_DEBUG ("from %u to 0x%x to %u", texture_targets[i], gl, res);

    fail_unless_equals_int (texture_targets[i], res);
  }
}

GST_END_TEST;

static Suite *
gst_gl_format_suite (void)
{
  Suite *s = suite_create ("Gst GL Formats");
  TCase *tc_chain = tcase_create ("general");

  suite_add_tcase (s, tc_chain);
  tcase_add_checked_fixture (tc_chain, setup, teardown);
  tcase_add_test (tc_chain, test_format_n_bytes);
  tcase_add_test (tc_chain, test_sized_from_unsized);
  tcase_add_test (tc_chain, test_unsized_from_sized);
  tcase_add_test (tc_chain, test_texture_target_strings);
  tcase_add_test (tc_chain, test_texture_target_gl);

  return s;
}

GST_CHECK_MAIN (gst_gl_format);
