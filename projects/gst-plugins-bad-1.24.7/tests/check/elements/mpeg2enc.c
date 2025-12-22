/* GStreamer
 *
 * unit test for mpeg2enc
 *
 * Copyright (C) <2006> Mark Nauwelaerts <manauw@skynet.be>
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

/* For ease of programming we use globals to keep refs for our floating
 * src and sink pads we create; otherwise we always have to do get_pad,
 * get_peer, and then remove references in every test function */
static GstPad *mysrcpad, *mysinkpad;

#define VIDEO_CAPS_STRING "video/x-raw, " \
                           "format = (string) I420, " \
                           "width = (int) 384, " \
                           "height = (int) 288, " \
                           "framerate = (fraction) 25/1"

#define MPEG_CAPS_STRING "video/mpeg, " \
                           "mpegversion = (int) { 1, 2 }, " \
                           "systemstream = (bool) false, " \
                           "height = (int) 288, " \
                           "framerate = (fraction) 25/1"

static GstStaticPadTemplate sinktemplate = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (MPEG_CAPS_STRING));

static GstStaticPadTemplate srctemplate = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (VIDEO_CAPS_STRING));


/* some global vars, makes it easy as for the ones above */
static GMutex mpeg2enc_mutex;
static GCond mpeg2enc_cond;
static gboolean arrived_eos;

static gboolean
test_sink_event (GstPad * pad, GstObject * parent, GstEvent * event)
{

  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_EOS:
      g_mutex_lock (&mpeg2enc_mutex);
      arrived_eos = TRUE;
      g_cond_signal (&mpeg2enc_cond);
      g_mutex_unlock (&mpeg2enc_mutex);
      break;
    default:
      break;
  }

  return gst_pad_event_default (pad, parent, event);
}

static GstElement *
setup_mpeg2enc (void)
{
  GstElement *mpeg2enc;

  GST_DEBUG ("setup_mpeg2enc");
  mpeg2enc = gst_check_setup_element ("mpeg2enc");
  mysrcpad = gst_check_setup_src_pad (mpeg2enc, &srctemplate);
  mysinkpad = gst_check_setup_sink_pad (mpeg2enc, &sinktemplate);
  gst_pad_set_active (mysrcpad, TRUE);
  gst_pad_set_active (mysinkpad, TRUE);

  /* need to know when we are eos */
  gst_pad_set_event_function (mysinkpad, test_sink_event);

  /* and notify the test run */
  g_mutex_init (&mpeg2enc_mutex);
  g_cond_init (&mpeg2enc_cond);

  return mpeg2enc;
}

static void
cleanup_mpeg2enc (GstElement * mpeg2enc)
{
  GST_DEBUG ("cleanup_mpeg2enc");
  gst_element_set_state (mpeg2enc, GST_STATE_NULL);

  gst_pad_set_active (mysrcpad, FALSE);
  gst_pad_set_active (mysinkpad, FALSE);
  gst_check_teardown_src_pad (mpeg2enc);
  gst_check_teardown_sink_pad (mpeg2enc);
  gst_check_teardown_element (mpeg2enc);

  g_mutex_clear (&mpeg2enc_mutex);
  g_cond_clear (&mpeg2enc_cond);
}

GST_START_TEST (test_video_pad)
{
  GstElement *mpeg2enc;
  GstBuffer *inbuffer, *outbuffer;
  GstCaps *caps;
  int i, num_buffers;
  guint8 data0[] = { 0x00, 0x00, 0x01, 0xb3 };


  mpeg2enc = setup_mpeg2enc ();
  fail_unless (gst_element_set_state (mpeg2enc,
          GST_STATE_PLAYING) == GST_STATE_CHANGE_SUCCESS,
      "could not set to playing");

  caps = gst_caps_from_string (VIDEO_CAPS_STRING);
  gst_check_setup_events_with_stream_id (mysrcpad, mpeg2enc, caps,
      GST_FORMAT_TIME, "/test/mpeg2enc");
  gst_caps_unref (caps);

  /* corresponds to I420 buffer for the size mentioned in the caps */
  inbuffer = gst_buffer_new_and_alloc (384 * 288 * 3 / 2);
  /* makes valgrind's memcheck happier */
  gst_buffer_memset (inbuffer, 0, 0, -1);
  GST_BUFFER_TIMESTAMP (inbuffer) = 0;
  ASSERT_BUFFER_REFCOUNT (inbuffer, "inbuffer", 1);
  fail_unless (gst_pad_push (mysrcpad, inbuffer) == GST_FLOW_OK);

  /* need to force eos and state change to make sure the encoding task ends */
  fail_unless (gst_pad_push_event (mysrcpad, gst_event_new_eos ()) == TRUE);
  /* need to wait a bit to make sure mpeg2enc task digested all this */
  g_mutex_lock (&mpeg2enc_mutex);
  while (!arrived_eos)
    g_cond_wait (&mpeg2enc_cond, &mpeg2enc_mutex);
  g_mutex_unlock (&mpeg2enc_mutex);

  num_buffers = g_list_length (buffers);
  /* well, we do not really know much with mpeg, but at least something ... */
  fail_unless (num_buffers >= 1);

  /* clean up buffers */
  for (i = 0; i < num_buffers; ++i) {
    outbuffer = GST_BUFFER (buffers->data);
    fail_if (outbuffer == NULL);

    switch (i) {
      case 0:
        fail_unless (gst_buffer_get_size (outbuffer) >= sizeof (data0));
        fail_unless (gst_buffer_memcmp (outbuffer, 0, data0,
                sizeof (data0)) == 0);
        break;
      default:
        break;
    }
    buffers = g_list_remove (buffers, outbuffer);

    ASSERT_BUFFER_REFCOUNT (outbuffer, "outbuffer", 1);
    gst_buffer_unref (outbuffer);
    outbuffer = NULL;
  }

  cleanup_mpeg2enc (mpeg2enc);
  g_list_free (buffers);
  buffers = NULL;
}

GST_END_TEST;

static Suite *
mpeg2enc_suite (void)
{
  Suite *s = suite_create ("mpeg2enc");
  TCase *tc_chain = tcase_create ("general");

  suite_add_tcase (s, tc_chain);
  tcase_add_test (tc_chain, test_video_pad);

  return s;
}

GST_CHECK_MAIN (mpeg2enc);
