/* GStreamer zxing element unit test
 *
 * Copyright (C) 2019 Stéphane Cerveau  <scerveau@collabora.com>
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

#include <gst/check/gstcheck.h>

static GstElement *
setup_pipeline (const gchar * in_format)
{
  GstElement *pipeline;
  gchar *path;
  gchar *pipeline_str;

  path = g_build_filename (GST_TEST_FILES_PATH, "barcode.png", NULL);
  GST_LOG ("reading file '%s'", path);

  pipeline_str =
      g_strdup_printf ("filesrc location=%s ! "
      "pngdec ! videoconvert ! video/x-raw,format=%s ! zxing name=zxing"
      " ! fakesink", path, in_format);
  GST_LOG ("Running pipeline: %s", pipeline_str);
  pipeline = gst_parse_launch (pipeline_str, NULL);
  fail_unless (pipeline != NULL);
  g_free (pipeline_str);
  g_free (path);

  return pipeline;
}

static GstMessage *
get_zxing_msg_until_eos (GstElement * pipeline)
{
  GstMessage *zxing_msg = NULL;

  do {
    GstMessage *msg;

    msg = gst_bus_timed_pop_filtered (GST_ELEMENT_BUS (pipeline), -1,
        GST_MESSAGE_ELEMENT | GST_MESSAGE_EOS | GST_MESSAGE_ERROR);

    GST_INFO ("message: %" GST_PTR_FORMAT, msg);

    fail_if (GST_MESSAGE_TYPE (msg) == GST_MESSAGE_ERROR);

    if (GST_MESSAGE_TYPE (msg) == GST_MESSAGE_EOS) {
      gst_message_unref (msg);
      break;
    }

    if (!g_strcmp0 (GST_OBJECT_NAME (GST_MESSAGE_SRC (msg)), "zxing")
        && zxing_msg == NULL) {
      zxing_msg = msg;
    } else {
      gst_message_unref (msg);
    }
  } while (1);
  return zxing_msg;
}

GST_START_TEST (test_still_image)
{
  GstMessage *zxing_msg;
  const GstStructure *s;
  GstElement *pipeline;
  const gchar *type, *symbol;

  pipeline = setup_pipeline ("ARGB");

  fail_unless_equals_int (gst_element_set_state (pipeline, GST_STATE_PLAYING),
      GST_STATE_CHANGE_ASYNC);

  zxing_msg = get_zxing_msg_until_eos (pipeline);
  fail_unless (zxing_msg != NULL);

  s = gst_message_get_structure (zxing_msg);
  fail_unless (s != NULL);

  fail_unless (gst_structure_has_name (s, "barcode"));
  fail_unless (gst_structure_has_field (s, "timestamp"));
  fail_unless (gst_structure_has_field (s, "stream-time"));
  fail_unless (gst_structure_has_field (s, "running-time"));
  fail_unless (gst_structure_has_field (s, "type"));
  fail_unless (gst_structure_has_field (s, "symbol"));
  type = gst_structure_get_string (s, "type");
  fail_unless_equals_string (type, "EAN-13");
  symbol = gst_structure_get_string (s, "symbol");
  fail_unless_equals_string (symbol, "9876543210128");

  fail_if (gst_structure_has_field (s, "frame"));

  fail_unless_equals_int (gst_element_set_state (pipeline, GST_STATE_NULL),
      GST_STATE_CHANGE_SUCCESS);

  gst_object_unref (pipeline);
  gst_message_unref (zxing_msg);
}

GST_END_TEST;

GST_START_TEST (test_still_image_with_sample)
{
  GstMessage *zxing_msg = NULL;
  const GstStructure *s;
  GstElement *pipeline;
  GstSample *sample;

  pipeline = setup_pipeline ("ARGB");
  gst_child_proxy_set ((GstChildProxy *) pipeline, "zxing::attach-frame", TRUE,
      NULL);

  fail_unless_equals_int (gst_element_set_state (pipeline, GST_STATE_PLAYING),
      GST_STATE_CHANGE_ASYNC);

  zxing_msg = get_zxing_msg_until_eos (pipeline);
  fail_unless (zxing_msg != NULL);

  s = gst_message_get_structure (zxing_msg);
  fail_unless (s != NULL);

  fail_unless (gst_structure_get (s, "frame", GST_TYPE_SAMPLE, &sample, NULL));
  fail_unless (gst_sample_get_buffer (sample));
  fail_unless (gst_sample_get_caps (sample));
  gst_sample_unref (sample);

  fail_unless_equals_int (gst_element_set_state (pipeline, GST_STATE_NULL),
      GST_STATE_CHANGE_SUCCESS);

  gst_object_unref (pipeline);
  gst_message_unref (zxing_msg);
}

GST_END_TEST;

static Suite *
zxing_suite (void)
{
  Suite *s = suite_create ("zxing");
  TCase *tc_chain = tcase_create ("general");

  suite_add_tcase (s, tc_chain);

  if (!gst_registry_check_feature_version (gst_registry_get (), "pngdec", 0, 10,
          25)) {
    GST_INFO ("Skipping test, pngdec either not available or too old");
  } else {
    tcase_add_test (tc_chain, test_still_image);
    tcase_add_test (tc_chain, test_still_image_with_sample);
  }

  return s;
}

GST_CHECK_MAIN (zxing);
