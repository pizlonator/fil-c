/* GStreamer zbar element unit test
 *
 * Copyright (C) 2010 Tim-Philipp Müller  <tim centricular net>
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
setup_pipeline (void)
{
  GstElement *pipeline, *src, *dec, *csp, *zbar, *sink;
  gchar *path;

  pipeline = gst_pipeline_new ("pipeline");

  src = gst_element_factory_make ("filesrc", NULL);
  dec = gst_element_factory_make ("pngdec", NULL);
  csp = gst_element_factory_make ("videoconvert", NULL);
  zbar = gst_element_factory_make ("zbar", "zbar");
  sink = gst_element_factory_make ("fakesink", NULL);

  path = g_build_filename (GST_TEST_FILES_PATH, "barcode.png", NULL);
  GST_LOG ("reading file '%s'", path);
  g_object_set (src, "location", path, NULL);
  g_free (path);

  gst_bin_add_many (GST_BIN (pipeline), src, dec, csp, zbar, sink, NULL);
  fail_unless (gst_element_link_many (src, dec, csp, zbar, sink, NULL));

  return pipeline;
}

static GstMessage *
get_zbar_msg_until_eos (GstElement * pipeline)
{
  GstMessage *zbar_msg = NULL;

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

    if (!g_strcmp0 (GST_OBJECT_NAME (GST_MESSAGE_SRC (msg)), "zbar")
        && zbar_msg == NULL) {
      zbar_msg = msg;
    } else {
      gst_message_unref (msg);
    }
  } while (1);
  return zbar_msg;
}


GST_START_TEST (test_still_image)
{
  GstMessage *zbar_msg;
  const GstStructure *s;
  GstElement *pipeline;
  const gchar *type, *symbol;
  int qual;

  pipeline = setup_pipeline ();

  fail_unless_equals_int (gst_element_set_state (pipeline, GST_STATE_PLAYING),
      GST_STATE_CHANGE_ASYNC);

  zbar_msg = get_zbar_msg_until_eos (pipeline);
  fail_unless (zbar_msg != NULL);

  s = gst_message_get_structure (zbar_msg);
  fail_unless (s != NULL);

  fail_unless (gst_structure_has_name (s, "barcode"));
  fail_unless (gst_structure_has_field (s, "timestamp"));
  fail_unless (gst_structure_has_field (s, "type"));
  fail_unless (gst_structure_has_field (s, "symbol"));
  fail_unless (gst_structure_has_field (s, "quality"));
  fail_unless (gst_structure_get_int (s, "quality", &qual));
  fail_unless (qual >= 90);
  type = gst_structure_get_string (s, "type");
  fail_unless_equals_string (type, "EAN-13");
  symbol = gst_structure_get_string (s, "symbol");
  fail_unless_equals_string (symbol, "9876543210128");

  fail_if (gst_structure_has_field (s, "frame"));

  fail_unless_equals_int (gst_element_set_state (pipeline, GST_STATE_NULL),
      GST_STATE_CHANGE_SUCCESS);

  gst_object_unref (pipeline);
  gst_message_unref (zbar_msg);
}

GST_END_TEST;

GST_START_TEST (test_still_image_with_sample)
{
  GstMessage *zbar_msg = NULL;
  const GstStructure *s;
  GstElement *pipeline;
  GstSample *sample;

  pipeline = setup_pipeline ();
  gst_child_proxy_set ((GstChildProxy *) pipeline, "zbar::attach-frame", TRUE,
      NULL);


  fail_unless_equals_int (gst_element_set_state (pipeline, GST_STATE_PLAYING),
      GST_STATE_CHANGE_ASYNC);

  zbar_msg = get_zbar_msg_until_eos (pipeline);
  fail_unless (zbar_msg != NULL);

  s = gst_message_get_structure (zbar_msg);
  fail_unless (s != NULL);

  fail_unless (gst_structure_get (s, "frame", GST_TYPE_SAMPLE, &sample, NULL));
  fail_unless (gst_sample_get_buffer (sample));
  fail_unless (gst_sample_get_caps (sample));
  gst_sample_unref (sample);

  fail_unless_equals_int (gst_element_set_state (pipeline, GST_STATE_NULL),
      GST_STATE_CHANGE_SUCCESS);

  gst_object_unref (pipeline);
  gst_message_unref (zbar_msg);
}

GST_END_TEST;

static Suite *
zbar_suite (void)
{
  Suite *s = suite_create ("zbar");
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

GST_CHECK_MAIN (zbar);
