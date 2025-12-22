/* GStreamer unit tests for the neonhttpsrc element
 * Copyright (C) 2006-2007 Tim-Philipp Müller <tim centricular net>
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
# include "config.h"
#endif

#include <gst/check/gstcheck.h>

static void
handoff_cb (GstElement * fakesink, GstBuffer * buf, GstPad * pad,
    GstBuffer ** p_outbuf)
{
  GST_LOG ("handoff, buf = %p", buf);
  if (*p_outbuf == NULL)
    *p_outbuf = gst_buffer_ref (buf);
}

GST_START_TEST (test_first_buffer_has_offset)
{
  GstStateChangeReturn ret;
  GstElement *pipe, *src, *sink;
  GstBuffer *buf = NULL;
  gchar **cookies;

  pipe = gst_pipeline_new (NULL);

  src = gst_element_factory_make ("neonhttpsrc", NULL);
  fail_unless (src != NULL);

  sink = gst_element_factory_make ("fakesink", NULL);
  fail_unless (sink != NULL);

  gst_bin_add (GST_BIN (pipe), src);
  gst_bin_add (GST_BIN (pipe), sink);
  fail_unless (gst_element_link (src, sink));

  g_object_set (src, "location", "http://gstreamer.freedesktop.org/", NULL);
  g_object_set (src, "automatic-redirect", TRUE, NULL);

  /* set some cookies (shouldn't hurt) */
  cookies = g_strsplit ("foo=1234,bar=9871615348162523726337x99FB", ",", -1);
  g_object_set (src, "cookies", cookies, NULL);
  g_strfreev (cookies);

  g_object_set (sink, "signal-handoffs", TRUE, NULL);
  g_signal_connect (sink, "preroll-handoff", G_CALLBACK (handoff_cb), &buf);

  ret = gst_element_set_state (pipe, GST_STATE_PAUSED);
  if (ret != GST_STATE_CHANGE_ASYNC) {
    GST_DEBUG ("failed to start up neon http src, ret = %d", ret);
    goto done;
  }

  /* don't wait for more than 10 seconds */
  ret = gst_element_get_state (pipe, NULL, NULL, 10 * GST_SECOND);
  GST_LOG ("ret = %u", ret);

  if (buf == NULL) {
    /* we want to test the buffer offset, nothing else; if there's a failure
     * it might be for lots of reasons (no network connection, whatever), we're
     * not interested in those */
    GST_DEBUG ("didn't manage to get data within 10 seconds, skipping test");
    goto done;
  }

  GST_DEBUG ("buffer offset = %" G_GUINT64_FORMAT, GST_BUFFER_OFFSET (buf));

  /* first buffer should have a 0 offset */
  fail_unless (GST_BUFFER_OFFSET (buf) == 0);
  gst_buffer_unref (buf);

done:

  gst_element_set_state (pipe, GST_STATE_NULL);
  gst_object_unref (pipe);
}

GST_END_TEST;

GST_START_TEST (test_icy_stream)
{
  GstElement *pipe, *src, *sink;
  GstMessage *msg;

  pipe = gst_pipeline_new (NULL);

  src = gst_element_factory_make ("neonhttpsrc", NULL);
  fail_unless (src != NULL);

  sink = gst_element_factory_make ("fakesink", NULL);
  fail_unless (sink != NULL);

  gst_bin_add (GST_BIN (pipe), src);
  gst_bin_add (GST_BIN (pipe), sink);
  fail_unless (gst_element_link (src, sink));

  /* First try Virgin Radio Ogg stream, to see if there's connectivity and all
   * (which is an attempt to work around the completely horrid error reporting
   * and that we can't distinguish different types of failures here).
   * Note that neonhttpsrc does the whole connect + session initiation all in
   * the state change function. */

  g_object_set (src, "location", "http://ogg2.smgradio.com/vr32.ogg", NULL);
  g_object_set (src, "automatic-redirect", FALSE, NULL);
  g_object_set (src, "num-buffers", 1, NULL);
  gst_element_set_state (pipe, GST_STATE_PLAYING);

  msg = gst_bus_poll (GST_ELEMENT_BUS (pipe),
      GST_MESSAGE_EOS | GST_MESSAGE_ERROR, -1);
  if (GST_MESSAGE_TYPE (msg) == GST_MESSAGE_ERROR) {
    GST_INFO ("looks like there's no net connectivity or sgmradio.com is "
        "down. In any case, let's just skip this test");
    gst_message_unref (msg);
    goto done;
  }
  gst_message_unref (msg);
  msg = NULL;
  gst_element_set_state (pipe, GST_STATE_NULL);

  /* Now, if the ogg stream works, the mp3 shoutcast stream should work as
   * well (time will tell if that's true) */

  /* Virgin Radio 32kbps mp3 shoutcast stream */
  g_object_set (src, "location", "http://mp3-vr-32.smgradio.com:80/", NULL);
  g_object_set (src, "automatic-redirect", FALSE, NULL);

  /* g_object_set (src, "neon-http-debug", TRUE, NULL); */

  /* EOS after the first buffer */
  g_object_set (src, "num-buffers", 1, NULL);

  gst_element_set_state (pipe, GST_STATE_PLAYING);
  msg = gst_bus_poll (GST_ELEMENT_BUS (pipe),
      GST_MESSAGE_EOS | GST_MESSAGE_ERROR, -1);

  if (GST_MESSAGE_TYPE (msg) == GST_MESSAGE_EOS) {
    GST_DEBUG ("success, we're done here");
    gst_message_unref (msg);
    goto done;
  }

  {
    GError *err = NULL;

    gst_message_parse_error (msg, &err, NULL);
    gst_message_unref (msg);
    g_error ("Error with ICY mp3 shoutcast stream: %s", err->message);
    g_error_free (err);
  }

done:

  gst_element_set_state (pipe, GST_STATE_NULL);
  gst_object_unref (pipe);
}

GST_END_TEST;

static Suite *
neonhttpsrc_suite (void)
{
  Suite *s = suite_create ("neonhttpsrc");
  TCase *tc_chain = tcase_create ("general");

  suite_add_tcase (s, tc_chain);
  tcase_set_timeout (tc_chain, 5);
  tcase_add_test (tc_chain, test_first_buffer_has_offset);
  tcase_add_test (tc_chain, test_icy_stream);

  return s;
}

GST_CHECK_MAIN (neonhttpsrc);
