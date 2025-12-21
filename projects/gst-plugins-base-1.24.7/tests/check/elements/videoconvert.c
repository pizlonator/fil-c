/* GStreamer
 * unit test for videoconvert
 *
 * Copyright (C) 2006-2012 Tim-Philipp Müller <tim centricular net>
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
# include <config.h>
#endif

#ifdef HAVE_VALGRIND
# include <valgrind/valgrind.h>
#endif

#include <gst/check/gstcheck.h>
#include <gst/check/gstharness.h>
#include <gst/video/video.h>

static guint
get_num_formats (void)
{
  guint i = 2;

  while (gst_video_format_to_string ((GstVideoFormat) i) != NULL)
    ++i;

  return i;
}

static void
check_pad_template (GstPadTemplate * tmpl)
{
  const GValue *list_val, *fmt_val;
  GstStructure *s;
  gboolean *formats_supported;
  GstCaps *caps;
  guint i, num_formats;

  num_formats = get_num_formats ();
  formats_supported = g_new0 (gboolean, num_formats);

  caps = gst_pad_template_get_caps (tmpl);

  /* If this fails, we need to update this unit test */
  fail_unless_equals_int (gst_caps_get_size (caps), 2);
  /* Remove the ANY caps features structure */
  caps = gst_caps_truncate (caps);
  s = gst_caps_get_structure (caps, 0);

  fail_unless (gst_structure_has_name (s, "video/x-raw"));

  list_val = gst_structure_get_value (s, "format");
  fail_unless (list_val != NULL);
  /* If this fails, we need to update this unit test */
  fail_unless (GST_VALUE_HOLDS_LIST (list_val));

  for (i = 0; i < gst_value_list_get_size (list_val); ++i) {
    GstVideoFormat fmt;
    const gchar *fmt_str;

    fmt_val = gst_value_list_get_value (list_val, i);
    fail_unless (G_VALUE_HOLDS_STRING (fmt_val));
    fmt_str = g_value_get_string (fmt_val);
    GST_LOG ("format string: '%s'", fmt_str);
    fmt = gst_video_format_from_string (fmt_str);
    fail_unless (fmt != GST_VIDEO_FORMAT_UNKNOWN);
    formats_supported[(guint) fmt] = TRUE;
  }

  gst_caps_unref (caps);

  for (i = 2; i < num_formats; ++i) {
    if (i == GST_VIDEO_FORMAT_DMA_DRM)
      continue;

    if (!formats_supported[i]) {
      g_error ("videoconvert doesn't support format '%s'",
          gst_video_format_to_string ((GstVideoFormat) i));
    }
  }

  g_free (formats_supported);
}

GST_START_TEST (test_template_formats)
{
  GstElementFactory *f;
  GstPadTemplate *t;
  const GList *pad_templates;

  f = gst_element_factory_find ("videoconvert");
  fail_unless (f != NULL);

  pad_templates = gst_element_factory_get_static_pad_templates (f);
  fail_unless_equals_int (g_list_length ((GList *) pad_templates), 2);

  t = gst_static_pad_template_get (pad_templates->data);
  check_pad_template (GST_PAD_TEMPLATE (t));
  gst_object_unref (t);
  t = gst_static_pad_template_get (pad_templates->next->data);
  check_pad_template (GST_PAD_TEMPLATE (t));
  gst_object_unref (t);

  gst_object_unref (f);
}

GST_END_TEST;

GST_START_TEST (test_negotiate_alternate)
{
  GstHarness *h;
  GstBuffer *buffer;
  GstMapInfo map;

  h = gst_harness_new ("videoconvert");

  buffer = gst_buffer_new_and_alloc (4);
  gst_buffer_map (buffer, &map, GST_MAP_WRITE);
  map.data[0] = 0x0;
  map.data[1] = 0x0;
  map.data[2] = 0x0;
  map.data[3] = 0x0;
  gst_buffer_unmap (buffer, &map);

  gst_harness_set_sink_caps_str (h,
      "video/x-raw,interlace-mode=alternate,width=1,height=1,format=AYUV");
  gst_harness_set_src_caps_str (h,
      "video/x-raw(format:Interlaced),interlace-mode=alternate,width=1,height=1,format=ARGB");
  fail_unless_equals_int (gst_harness_push (h, gst_buffer_ref (buffer)),
      GST_FLOW_NOT_NEGOTIATED);

  gst_harness_set_sink_caps_str (h,
      "video/x-raw(format:Interlaced),interlace-mode=alternate,width=1,height=1,format=AYUV");
  gst_harness_set_src_caps_str (h,
      "video/x-raw,interlace-mode=alternate,width=1,height=1,format=ARGB");
  fail_unless_equals_int (gst_harness_push (h, gst_buffer_ref (buffer)),
      GST_FLOW_NOT_NEGOTIATED);

  gst_harness_set_sink_caps_str (h,
      "video/x-raw(format:Interlaced),interlace-mode=alternate,width=1,height=1,format=AYUV");
  gst_harness_set_src_caps_str (h,
      "video/x-raw(format:Interlaced),interlace-mode=alternate,width=1,height=1,format=ARGB");
  fail_unless_equals_int (gst_harness_push (h, buffer), GST_FLOW_OK);

  gst_harness_teardown (h);
}

GST_END_TEST;

static Suite *
videoconvert_suite (void)
{
  Suite *s = suite_create ("videoconvert");
  TCase *tc_chain = tcase_create ("general");

  suite_add_tcase (s, tc_chain);

  tcase_add_test (tc_chain, test_template_formats);
  tcase_add_test (tc_chain, test_negotiate_alternate);

  return s;
}

GST_CHECK_MAIN (videoconvert);
