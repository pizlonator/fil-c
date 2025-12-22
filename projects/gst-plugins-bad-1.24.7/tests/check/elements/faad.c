/* GStreamer
 *
 * unit test for faad
 *
 * Copyright (C) <2009> Mark Nauwelaerts <mnauw@users.sf.net>
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

#include <unistd.h>

#include <gst/check/gstcheck.h>
#include <gst/audio/audio.h>

/* For ease of programming we use globals to keep refs for our floating
 * src and sink pads we create; otherwise we always have to do get_pad,
 * get_peer, and then remove references in every test function */
static GstPad *mysrcpad, *mysinkpad;

#define AUDIO_CAPS_STRING "audio/x-raw, " \
                           "format = (string) " GST_AUDIO_NE (S16) ", " \
                           "rate = (int) 48000, " \
                           "channels = (int) 2, " \
                           "channel-mask = (bitmask) 3"

#define AAC_CAPS_STRING "audio/mpeg, " \
                          "mpegversion = (int) 4, " \
                          "rate = (int) 48000, " \
                          "channels = (int) 2, " \
                          "framed = (boolean) true "

static GstStaticPadTemplate sinktemplate = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (AUDIO_CAPS_STRING));

static GstStaticPadTemplate srctemplate = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (AAC_CAPS_STRING));


static GstElement *
setup_faad (void)
{
  GstElement *faad;

  GST_DEBUG ("setup_faad");
  faad = gst_check_setup_element ("faad");
  mysrcpad = gst_check_setup_src_pad (faad, &srctemplate);
  mysinkpad = gst_check_setup_sink_pad (faad, &sinktemplate);
  gst_pad_set_active (mysrcpad, TRUE);
  gst_pad_set_active (mysinkpad, TRUE);

  return faad;
}

static void
cleanup_faad (GstElement * faad)
{
  GST_DEBUG ("cleanup_faad");
  gst_element_set_state (faad, GST_STATE_NULL);

  gst_pad_set_active (mysrcpad, FALSE);
  gst_pad_set_active (mysinkpad, FALSE);
  gst_check_teardown_src_pad (faad);
  gst_check_teardown_sink_pad (faad);
  gst_check_teardown_element (faad);
}

static void
do_test (GstBuffer * inbuffer, GstCaps * caps)
{
  GstElement *faad;
  GstBuffer *outbuffer;
  gint i, num_buffers;
  const gint nbuffers = 2;

  faad = setup_faad ();
  fail_unless (gst_element_set_state (faad,
          GST_STATE_PLAYING) == GST_STATE_CHANGE_SUCCESS,
      "could not set to playing");

  GST_BUFFER_TIMESTAMP (inbuffer) = 0;
  ASSERT_BUFFER_REFCOUNT (inbuffer, "inbuffer", 1);

  gst_check_setup_events (mysrcpad, faad, caps, GST_FORMAT_TIME);

  /* need to push twice to get faad output */
  gst_buffer_ref (inbuffer);
  fail_unless (gst_pad_push (mysrcpad, inbuffer) == GST_FLOW_OK);
  fail_unless (gst_pad_push (mysrcpad, inbuffer) == GST_FLOW_OK);

  /* send eos to have all flushed if needed */
  fail_unless (gst_pad_push_event (mysrcpad, gst_event_new_eos ()) == TRUE);

  num_buffers = g_list_length (buffers);
  fail_unless (num_buffers >= nbuffers - 1);

  /* clean up buffers */
  for (i = 0; i < num_buffers; ++i) {
    gint size;

    outbuffer = GST_BUFFER (buffers->data);
    fail_if (outbuffer == NULL);

    size = gst_buffer_get_size (outbuffer);

    /* 2 16-bit channels */
    fail_unless (size == 1024 * 2 * 2);

    buffers = g_list_remove (buffers, outbuffer);

    ASSERT_BUFFER_REFCOUNT (outbuffer, "outbuffer", 1);
    gst_buffer_unref (outbuffer);
    outbuffer = NULL;
  }

  cleanup_faad (faad);
  g_list_free (buffers);
  buffers = NULL;
}

static guint8 raw_data_block[] = {
  0x21, 0x1b, 0x80, 0x00, 0x7d, 0xe0, 0x00, 0x3e, 0xf1, 0xe7
};

static guint8 adts_header[] = {
  0xff, 0xf8, 0x4c, 0x80, 0x02, 0x7f, 0xfc, 0x04, 0x40
};

static guint8 codec_data[] = {
  0x11, 0x90
};

GST_START_TEST (test_adts)
{
  gint size;
  GstBuffer *buf, *header_buf;
  GstCaps *caps;

  size = sizeof (adts_header);
  header_buf = gst_buffer_new_and_alloc (size);
  gst_buffer_fill (header_buf, 0, adts_header, size);

  size = sizeof (raw_data_block);
  buf = gst_buffer_new_and_alloc (size);
  gst_buffer_fill (buf, 0, raw_data_block, size);

  buf = gst_buffer_append (header_buf, buf);
  caps = gst_caps_from_string (AAC_CAPS_STRING);
  gst_caps_set_simple (caps, "stream-format", G_TYPE_STRING, "adts", NULL);
  do_test (buf, caps);
  gst_caps_unref (caps);
}

GST_END_TEST;

GST_START_TEST (test_raw)
{
  gint size;
  GstBuffer *buf, *codec_buf;
  GstCaps *caps;

  size = sizeof (codec_data);
  codec_buf = gst_buffer_new_and_alloc (size);
  gst_buffer_fill (codec_buf, 0, codec_data, size);

  size = sizeof (raw_data_block);
  buf = gst_buffer_new_and_alloc (size);
  gst_buffer_fill (buf, 0, raw_data_block, size);
  caps = gst_caps_from_string (AAC_CAPS_STRING);
  gst_caps_set_simple (caps, "stream-format", G_TYPE_STRING, "raw",
      "codec_data", GST_TYPE_BUFFER, codec_buf, NULL);
  gst_buffer_unref (codec_buf);

  do_test (buf, caps);
  gst_caps_unref (caps);
}

GST_END_TEST;

static Suite *
faad_suite (void)
{
  Suite *s = suite_create ("faad");
  TCase *tc_chain = tcase_create ("general");

  suite_add_tcase (s, tc_chain);
  tcase_add_test (tc_chain, test_adts);
  tcase_add_test (tc_chain, test_raw);

  return s;
}

GST_CHECK_MAIN (faad);
