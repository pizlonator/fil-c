/* GStreamer unit test for libvisual plugin
 *
 * Copyright (C) 2007 Tim-Philipp Müller <tim at centricular net>
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

#include <gst/check/gstcheck.h>

static gboolean
filter_func (GstPluginFeature * feature, gpointer user_data)
{
  return (g_str_has_prefix (GST_OBJECT_NAME (feature), "libvisual_"));
}

static void
test_shutdown_for_factory (const gchar * factory_name)
{
  GstElement *pipeline, *src, *q, *ac, *vis, *cf, *q2, *sink;
  GstCaps *caps;
  guint i;

  pipeline = gst_pipeline_new (NULL);

  src = gst_check_setup_element ("audiotestsrc");
  q = gst_check_setup_element ("queue");
  ac = gst_check_setup_element ("audioconvert");

  GST_INFO ("Using %s", factory_name);
  vis = gst_check_setup_element (factory_name);

  cf = gst_check_setup_element ("capsfilter");
  caps = gst_caps_new_simple ("video/x-raw", "width", G_TYPE_INT, 64,
      "height", G_TYPE_INT, 64, "framerate", GST_TYPE_FRACTION, 15, 1, NULL);
  g_object_set (cf, "caps", caps, NULL);
  gst_caps_unref (caps);

  q2 = gst_check_setup_element ("queue");
  gst_object_set_name (GST_OBJECT (q2), "queue2");
  sink = gst_check_setup_element ("fakesink");

  /* don't want to sync against the clock, the more throughput the better */
  g_object_set (src, "is-live", FALSE, NULL);
  g_object_set (sink, "sync", FALSE, NULL);

  gst_bin_add_many (GST_BIN (pipeline), src, q, ac, vis, cf, q2, sink, NULL);
  fail_if (!gst_element_link_many (src, q, ac, vis, cf, q2, sink, NULL));

  /* now, wait until pipeline is running and then shut it down again; repeat;
   * this makes sure we can shut down cleanly while stuff is going on in the
   * chain function */
  for (i = 0; i < 50; ++i) {
    gst_element_set_state (pipeline, GST_STATE_PAUSED);
    gst_element_get_state (pipeline, NULL, NULL, -1);
    gst_element_set_state (pipeline, GST_STATE_PLAYING);
    g_usleep (100);
    gst_element_set_state (pipeline, GST_STATE_NULL);
  }

  gst_object_unref (pipeline);
}

GST_START_TEST (test_shutdown)
{
  const gchar *factory_to_test;

  factory_to_test = g_getenv ("LIBVISUAL_UNIT_TEST_FACTORY");

  if (factory_to_test == NULL) {
    GList *list, *l;

    list = gst_registry_feature_filter (gst_registry_get (), filter_func,
        FALSE, NULL);

    if (list == NULL) {
      g_print ("No libvisual plugins installed.\n");
      return;
    }
    for (l = list; l != NULL; l = l->next) {
      test_shutdown_for_factory (GST_OBJECT_NAME (l->data));
    }
    gst_plugin_feature_list_free (list);
  } else {
    test_shutdown_for_factory (factory_to_test);
  }
}

GST_END_TEST;

static Suite *
libvisual_suite (void)
{
  Suite *s = suite_create ("libvisual");
  TCase *tc_chain = tcase_create ("general");

  suite_add_tcase (s, tc_chain);

  /* set one manually, since we're currently built as uninst program with
   * the default timeout of 3 seconds, which is way too short */
  tcase_set_timeout (tc_chain, 30);

  tcase_add_test (tc_chain, test_shutdown);

  return s;
}

GST_CHECK_MAIN (libvisual);
