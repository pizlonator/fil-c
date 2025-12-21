/* GStreamer
 *
 * Copyright (C) 2018 Matthew Waters <matthew@centricular.com>
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

GST_START_TEST (test_same_version)
{
  fail_unless_equals_int (GST_GL_CHECK_GL_VERSION (2, 2, 2, 2), TRUE);
}

GST_END_TEST;

GST_START_TEST (test_greater_major_version)
{
  fail_unless_equals_int (GST_GL_CHECK_GL_VERSION (3, 2, 2, 2), TRUE);
}

GST_END_TEST;

GST_START_TEST (test_greater_minor_version)
{
  fail_unless_equals_int (GST_GL_CHECK_GL_VERSION (2, 3, 2, 2), TRUE);
}

GST_END_TEST;

GST_START_TEST (test_greater_major_minor_version)
{
  fail_unless_equals_int (GST_GL_CHECK_GL_VERSION (3, 3, 2, 2), TRUE);
}

GST_END_TEST;

GST_START_TEST (test_lesser_major_version)
{
  fail_unless_equals_int (GST_GL_CHECK_GL_VERSION (2, 2, 3, 2), FALSE);
}

GST_END_TEST;

GST_START_TEST (test_lesser_minor_version)
{
  fail_unless_equals_int (GST_GL_CHECK_GL_VERSION (2, 2, 2, 3), FALSE);
}

GST_END_TEST;

GST_START_TEST (test_lesser_major_minor_version)
{
  fail_unless_equals_int (GST_GL_CHECK_GL_VERSION (2, 2, 3, 3), FALSE);
}

GST_END_TEST;

static const gchar *dummy_extensions = "start middle end";

GST_START_TEST (test_extension_start)
{
  fail_unless_equals_int (gst_gl_check_extension ("start", dummy_extensions),
      TRUE);
}

GST_END_TEST;

GST_START_TEST (test_extension_middle)
{
  fail_unless_equals_int (gst_gl_check_extension ("middle", dummy_extensions),
      TRUE);
}

GST_END_TEST;

GST_START_TEST (test_extension_end)
{
  fail_unless_equals_int (gst_gl_check_extension ("end", dummy_extensions),
      TRUE);
}

GST_END_TEST;

GST_START_TEST (test_extension_non_existent)
{
  fail_unless_equals_int (gst_gl_check_extension ("ZZZZZZ", dummy_extensions),
      FALSE);
}

GST_END_TEST;

GST_START_TEST (test_extension_non_existent_start)
{
  fail_unless_equals_int (gst_gl_check_extension ("start1", dummy_extensions),
      FALSE);
}

GST_END_TEST;

GST_START_TEST (test_extension_non_existent_middle)
{
  fail_unless_equals_int (gst_gl_check_extension ("middle1", dummy_extensions),
      FALSE);
}

GST_END_TEST;

GST_START_TEST (test_extension_non_existent_end)
{
  fail_unless_equals_int (gst_gl_check_extension ("1end", dummy_extensions),
      FALSE);
}

GST_END_TEST;

static Suite *
gst_gl_format_suite (void)
{
  Suite *s = suite_create ("Gst GL Feature");
  TCase *tc_chain = tcase_create ("general");

  suite_add_tcase (s, tc_chain);
  tcase_add_test (tc_chain, test_same_version);
  tcase_add_test (tc_chain, test_greater_major_version);
  tcase_add_test (tc_chain, test_greater_minor_version);
  tcase_add_test (tc_chain, test_greater_major_minor_version);
  tcase_add_test (tc_chain, test_lesser_major_version);
  tcase_add_test (tc_chain, test_lesser_minor_version);
  tcase_add_test (tc_chain, test_lesser_major_minor_version);
  tcase_add_test (tc_chain, test_extension_start);
  tcase_add_test (tc_chain, test_extension_middle);
  tcase_add_test (tc_chain, test_extension_end);
  tcase_add_test (tc_chain, test_extension_non_existent);
  tcase_add_test (tc_chain, test_extension_non_existent_start);
  tcase_add_test (tc_chain, test_extension_non_existent_middle);
  tcase_add_test (tc_chain, test_extension_non_existent_end);

  return s;
}

GST_CHECK_MAIN (gst_gl_format);
