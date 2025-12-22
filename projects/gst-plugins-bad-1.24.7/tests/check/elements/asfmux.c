/* GStreamer
 *
 * unit test for asfmux
 *
 * Copyright (C) <2008> Thiago Santos <thiagoss@embedded.ufcg.edu.br>
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

/* For ease of programming we use globals to keep refs for our floating
 * src and sink pads we create; otherwise we always have to do get_pad,
 * get_peer, and then remove references in every test function */
static GstPad *mysrcpad, *mysinkpad;

#define AUDIO_CAPS_STRING "audio/x-wma, " \
                        "channels = (int) 2, " \
                        "rate = (int) 8000, " \
                        "wmaversion = (int) 2, " \
                        "block-align = (int) 14, " \
                        "bitrate = (int) 64000"

#define VIDEO_CAPS_STRING "video/x-wmv, " \
                           "width = (int) 384, " \
                           "height = (int) 288, " \
                           "framerate = (fraction) 25/1, " \
                           "wmvversion = (int) 2"

static GstStaticPadTemplate sinktemplate = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("video/x-ms-asf"));
static GstStaticPadTemplate srcvideotemplate = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (VIDEO_CAPS_STRING));
static GstStaticPadTemplate srcaudiotemplate = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (AUDIO_CAPS_STRING));

static GstPad *
setup_src_pad (GstElement * element,
    GstStaticPadTemplate * template, const gchar * sinkname)
{
  GstPad *srcpad, *sinkpad;

  GST_DEBUG_OBJECT (element, "setting up sending pad");
  /* sending pad */
  srcpad = gst_pad_new_from_static_template (template, "src");
  fail_if (srcpad == NULL, "Could not create a srcpad");
  ASSERT_OBJECT_REFCOUNT (srcpad, "srcpad", 1);

  if (!(sinkpad = gst_element_get_static_pad (element, sinkname)))
    sinkpad = gst_element_request_pad_simple (element, sinkname);
  fail_if (sinkpad == NULL, "Could not get sink pad from %s",
      GST_ELEMENT_NAME (element));
  /* references are owned by: 1) us, 2) asfmux, 3) collect pads */
  ASSERT_OBJECT_REFCOUNT (sinkpad, "sinkpad", 3);
  fail_unless (gst_pad_link (srcpad, sinkpad) == GST_PAD_LINK_OK,
      "Could not link source and %s sink pads", GST_ELEMENT_NAME (element));
  gst_object_unref (sinkpad);   /* because we got it higher up */

  /* references are owned by: 1) asfmux, 2) collect pads */
  ASSERT_OBJECT_REFCOUNT (sinkpad, "sinkpad", 2);

  return srcpad;
}

static void
teardown_src_pad (GstElement * element, const gchar * sinkname)
{
  GstPad *srcpad, *sinkpad;
  gchar *padname;

  /* clean up floating src pad */
  padname = g_strdup_printf (sinkname, 1);
  if (!(sinkpad = gst_element_get_static_pad (element, padname)))
    sinkpad = gst_element_request_pad_simple (element, padname);
  g_free (padname);

  fail_if (sinkpad == NULL, "sinkpad is null");

  /* pad refs held by 1) asfmux 2) collectpads and 3) us (through _get) */
  ASSERT_OBJECT_REFCOUNT (sinkpad, "sinkpad", 3);
  fail_unless (gst_pad_is_linked (sinkpad));
  srcpad = gst_pad_get_peer (sinkpad);

  fail_if (srcpad == NULL, "Couldn't get srcpad");
  gst_pad_unlink (srcpad, sinkpad);

  /* after unlinking, pad refs still held by
   * 1) asfmux and 2) collectpads and 3) us (through _get) */
  ASSERT_OBJECT_REFCOUNT (sinkpad, "sinkpad", 3);
  gst_object_unref (sinkpad);
  /* one more ref is held by element itself */

  /* pad refs held by both creator and this function (through _get_peer) */
  ASSERT_OBJECT_REFCOUNT (srcpad, "srcpad", 2);
  gst_object_unref (srcpad);
  gst_object_unref (srcpad);
}

static GstElement *
setup_asfmux (GstStaticPadTemplate * srctemplate, const gchar * sinkname)
{
  GstElement *asfmux;

  GST_DEBUG ("setup_asfmux");
  asfmux = gst_check_setup_element ("asfmux");

  mysrcpad = setup_src_pad (asfmux, srctemplate, sinkname);
  mysinkpad = gst_check_setup_sink_pad (asfmux, &sinktemplate);
  gst_pad_set_active (mysrcpad, TRUE);
  gst_pad_set_active (mysinkpad, TRUE);
  return asfmux;
}

static void
cleanup_asfmux (GstElement * asfmux, const gchar * sinkname)
{
  GST_DEBUG ("cleanup_asfmux");
  gst_element_set_state (asfmux, GST_STATE_NULL);
  gst_pad_set_active (mysrcpad, FALSE);
  gst_pad_set_active (mysinkpad, FALSE);
  teardown_src_pad (asfmux, sinkname);
  gst_check_teardown_sink_pad (asfmux);
  gst_check_teardown_element (asfmux);
}

static void
check_asfmux_pad (GstStaticPadTemplate * srctemplate,
    const gchar * src_caps_string, const gchar * sinkname)
{
  GstElement *asfmux;
  GstBuffer *inbuffer;
  GstCaps *caps;
  GstFlowReturn ret;
  GList *l;

  asfmux = setup_asfmux (srctemplate, sinkname);
  fail_unless (gst_element_set_state (asfmux,
          GST_STATE_PLAYING) == GST_STATE_CHANGE_SUCCESS,
      "could not set to playing");

  inbuffer = gst_buffer_new_and_alloc (1);
  caps = gst_caps_from_string (src_caps_string);
  gst_check_setup_events (mysrcpad, asfmux, caps, GST_FORMAT_TIME);
  gst_caps_unref (caps);
  GST_BUFFER_TIMESTAMP (inbuffer) = 0;
  ASSERT_BUFFER_REFCOUNT (inbuffer, "inbuffer", 1);
  ret = gst_pad_push (mysrcpad, inbuffer);
  fail_unless (ret == GST_FLOW_OK, "Pad push returned: %d", ret);

  cleanup_asfmux (asfmux, sinkname);
  for (l = buffers; l; l = l->next)
    gst_buffer_unref (l->data);
  g_list_free (buffers);
  buffers = NULL;
}

GST_START_TEST (test_video_pad)
{
  check_asfmux_pad (&srcvideotemplate, VIDEO_CAPS_STRING, "video_%u");
}

GST_END_TEST;

GST_START_TEST (test_audio_pad)
{
  check_asfmux_pad (&srcaudiotemplate, AUDIO_CAPS_STRING, "audio_%u");
}

GST_END_TEST;

static Suite *
asfmux_suite (void)
{
  Suite *s = suite_create ("asfmux");
  TCase *tc_chain = tcase_create ("general");
  tcase_add_test (tc_chain, test_video_pad);
  tcase_add_test (tc_chain, test_audio_pad);

  suite_add_tcase (s, tc_chain);

  return s;
}

GST_CHECK_MAIN (asfmux);
