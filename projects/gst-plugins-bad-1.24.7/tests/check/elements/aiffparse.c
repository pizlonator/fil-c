/* GStreamer
 *
 * unit test for aiffparse
 * Copyright (C) 2013 Collabora Ltd.
 *   Author: Matthieu Bouron <matthieu.bouron@collabora.com>
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
#include <string.h>
#include <glib.h>
#include <glib/gstdio.h>

#define DATA_FILENAME "s16be-id3v2.aiff"
#define DATA_SIZE 23254
#define SSND_DATA_OFFSET 68
#define SSND_DATA_SIZE 20480

static GstPad *sinkpad;
static GMainLoop *loop = NULL;
static gboolean have_eos = FALSE;
static gboolean have_tags = FALSE;
static gchar *data = NULL;
static gsize data_size = 0;
static guint64 data_read = 0;
static guint64 data_offset = 0;

static GstStaticPadTemplate sinktemplate =
GST_STATIC_PAD_TEMPLATE ("sink", GST_PAD_SINK, GST_PAD_ALWAYS,
    GST_STATIC_CAPS_ANY);

static void
sink_check_caps (GstPad * pad, GstCaps * caps)
{
  GstCaps *tcaps = gst_caps_new_simple ("audio/x-raw",
      "rate", G_TYPE_INT, 44100,
      "channels", G_TYPE_INT, 2,
      "format", G_TYPE_STRING, "S16BE",
      "layout", G_TYPE_STRING, "interleaved",
      NULL);

  fail_unless (gst_caps_can_intersect (caps, tcaps));
  gst_caps_unref (tcaps);
}

static GstFlowReturn
sink_chain (GstPad * pad, GstObject * parent, GstBuffer * buffer)
{
  GstMapInfo info;

  gst_buffer_map (buffer, &info, GST_MAP_READ);

  fail_unless ((data_offset + info.size) <= data_size);
  fail_unless (memcmp (info.data, data + data_offset, info.size) == 0);

  data_read += info.size;
  data_offset += info.size;

  gst_buffer_unmap (buffer, &info);
  gst_buffer_unref (buffer);

  return GST_FLOW_OK;
}

static gboolean
sink_event (GstPad * pad, GstObject * parent, GstEvent * event)
{
  GST_DEBUG_OBJECT (pad, "Got %s event %p: %" GST_PTR_FORMAT,
      GST_EVENT_TYPE_NAME (event), event, event);

  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_EOS:
      if (loop) {
        while (!g_main_loop_is_running (loop));
      }

      have_eos = TRUE;
      if (loop)
        g_main_loop_quit (loop);
      break;
    case GST_EVENT_CAPS:
    {
      GstCaps *caps;

      gst_event_parse_caps (event, &caps);
      sink_check_caps (pad, caps);
      break;
    }
    case GST_EVENT_TAG:
    {
      int i, ret;
      gchar *buf = NULL;
      GstTagList *aiff_tags = NULL;
      const gchar *tags[][2] = {
        {"title", "Title"}, {"artist", "Artist"},
      };

      gst_event_parse_tag (event, &aiff_tags);
      fail_unless (aiff_tags != NULL);

      have_tags = TRUE;
      for (i = 0; i < sizeof (tags) / sizeof (*tags); i++) {
        buf = NULL;
        if (!gst_tag_list_get_string (aiff_tags, tags[i][0], &buf)) {
          have_tags = FALSE;
          continue;
        }
        ret = g_strcmp0 (buf, tags[i][1]);
        g_free (buf);
        if (ret != 0) {
          have_tags = FALSE;
          continue;
        }
      }

      break;
    }
    default:
      break;
  }

  gst_event_unref (event);

  return TRUE;
}

static GstPad *
create_sink_pad (void)
{
  sinkpad = gst_pad_new_from_static_template (&sinktemplate, "sink");

  gst_pad_set_chain_function (sinkpad, sink_chain);
  gst_pad_set_event_function (sinkpad, sink_event);

  return sinkpad;
}

static void
run_check (gboolean push_mode)
{
  gchar *path;
  GstPad *aiff_srcpad;
  GError *error = NULL;
  GstElement *src, *sep, *aiffparse;

  have_eos = FALSE;
  have_tags = FALSE;
  data_read = 0;
  data_size = 0;
  data_offset = SSND_DATA_OFFSET;
  loop = g_main_loop_new (NULL, FALSE);

  GST_INFO ("%s mode", push_mode ? "Pull" : "Push");

  aiffparse = gst_element_factory_make ("aiffparse", "aiffparse");
  fail_unless (aiffparse != NULL);

  aiff_srcpad = gst_element_get_static_pad (aiffparse, "src");
  fail_unless (aiff_srcpad != NULL);

  src = gst_element_factory_make ("filesrc", "filesrc");
  fail_unless (src != NULL);

  sinkpad = create_sink_pad ();
  fail_unless (sinkpad != NULL);

  if (push_mode) {
    sep = gst_element_factory_make ("queue", "queue");
  } else {
    sep = gst_element_factory_make ("identity", "identity");
  }
  fail_unless (sep != NULL);

  fail_unless (gst_element_link (src, sep));
  fail_unless (gst_element_link (sep, aiffparse));

  fail_unless (gst_pad_link (aiff_srcpad, sinkpad) == GST_PAD_LINK_OK);
  gst_object_unref (aiff_srcpad);

  path = g_build_filename (GST_TEST_FILES_PATH, DATA_FILENAME, NULL);
  GST_LOG ("Reading file '%s'", path);
  g_object_set (src, "location", path, NULL);

  fail_unless (g_file_get_contents (path, &data, &data_size, &error));
  fail_unless (data_size == DATA_SIZE);

  GST_INFO ("Setting to PLAYING");
  gst_pad_set_active (sinkpad, TRUE);
  fail_unless (gst_element_set_state (aiffparse,
          GST_STATE_PLAYING) == GST_STATE_CHANGE_SUCCESS);
  fail_unless (gst_element_set_state (sep,
          GST_STATE_PLAYING) == GST_STATE_CHANGE_SUCCESS);
  fail_unless (gst_element_set_state (src,
          GST_STATE_PLAYING) == GST_STATE_CHANGE_SUCCESS);

  g_main_loop_run (loop);
  fail_unless (have_eos == TRUE);
  fail_unless (data_read == SSND_DATA_SIZE);
  fail_unless (push_mode || (have_tags == TRUE));

  gst_pad_set_active (sinkpad, FALSE);
  gst_element_set_state (aiffparse, GST_STATE_NULL);
  gst_element_set_state (sep, GST_STATE_NULL);
  gst_element_set_state (src, GST_STATE_NULL);

  gst_object_unref (aiffparse);
  gst_object_unref (sep);
  gst_object_unref (src);
  gst_object_unref (sinkpad);
  g_main_loop_unref (loop);
  loop = NULL;
  g_free (path);
  g_free (data);
}

GST_START_TEST (test_pull)
{
  run_check (FALSE);
}

GST_END_TEST;

GST_START_TEST (test_push)
{
  run_check (TRUE);
}

GST_END_TEST;

static Suite *
aiffparse_suite (void)
{
  Suite *s = suite_create ("aiffparse");
  TCase *tc_chain = tcase_create ("general");

  suite_add_tcase (s, tc_chain);
  tcase_add_test (tc_chain, test_pull);
  tcase_add_test (tc_chain, test_push);

  return s;
}

GST_CHECK_MAIN (aiffparse);
