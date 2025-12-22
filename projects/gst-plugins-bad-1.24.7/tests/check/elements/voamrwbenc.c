/* GStreamer
 *
 * unit test for voamrwbenc
 *
 * Copyright (C) <2011> Mark Nauwelaerts <mark.nauwelaerts@collabora.co.uk>
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

#include <gst/audio/audio-format.h>
#include <gst/check/gstcheck.h>

/* For ease of programming we use globals to keep refs for our floating
 * src and sink pads we create; otherwise we always have to do get_pad,
 * get_peer, and then remove references in every test function */
static GstPad *mysrcpad, *mysinkpad;

#define AUDIO_CAPS_STRING "audio/x-raw, " \
                          "format = (string) " GST_AUDIO_NE (S16) ", " \
                          "layout = (string) interleaved, " \
                          "rate = (int) 16000, " \
                          "channels = (int) 1 "


#define AMRWB_CAPS_STRING "audio/AMR-WB"


static GstStaticPadTemplate sinktemplate = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (AMRWB_CAPS_STRING));


static GstStaticPadTemplate srctemplate = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (AUDIO_CAPS_STRING));


static GstElement *
setup_voamrwbenc (void)
{
  GstElement *voamrwbenc;

  GST_DEBUG ("setup_voamrwbenc");
  voamrwbenc = gst_check_setup_element ("voamrwbenc");
  /* ensure mode as expected */
  g_object_set (voamrwbenc, "band-mode", 0, NULL);
  mysrcpad = gst_check_setup_src_pad (voamrwbenc, &srctemplate);
  mysinkpad = gst_check_setup_sink_pad (voamrwbenc, &sinktemplate);
  gst_pad_set_active (mysrcpad, TRUE);
  gst_pad_set_active (mysinkpad, TRUE);

  return voamrwbenc;
}

static void
cleanup_voamrwbenc (GstElement * voamrwbenc)
{
  GST_DEBUG ("cleanup_aacenc");
  gst_element_set_state (voamrwbenc, GST_STATE_NULL);

  gst_pad_set_active (mysrcpad, FALSE);
  gst_pad_set_active (mysinkpad, FALSE);
  gst_check_teardown_src_pad (voamrwbenc);
  gst_check_teardown_sink_pad (voamrwbenc);
  gst_check_teardown_element (voamrwbenc);
}

static void
do_test (gsize sample_count)
{
  GstElement *voamrwbenc;
  GstBuffer *inbuffer, *outbuffer;
  GstCaps *caps;
  gint i, num_buffers;
  const gsize buffer_size = sample_count * 2;
  const gsize nbuffers = (sample_count + 319) / 320;

  voamrwbenc = setup_voamrwbenc ();
  fail_unless (gst_element_set_state (voamrwbenc,
          GST_STATE_PLAYING) == GST_STATE_CHANGE_SUCCESS,
      "could not set to playing");

  /* corresponds to audio buffer mentioned in the caps */
  inbuffer = gst_buffer_new_allocate (NULL, buffer_size, NULL);
  /* makes valgrind's memcheck happier */
  gst_buffer_memset (inbuffer, 0, 0, buffer_size);
  caps = gst_caps_from_string (AUDIO_CAPS_STRING);

  gst_check_setup_events (mysrcpad, voamrwbenc, caps, GST_FORMAT_TIME);
  gst_caps_unref (caps);
  GST_BUFFER_TIMESTAMP (inbuffer) = 0;
  ASSERT_BUFFER_REFCOUNT (inbuffer, "inbuffer", 1);
  fail_unless (gst_pad_push (mysrcpad, inbuffer) == GST_FLOW_OK);

  /* send eos to have all flushed if needed */
  fail_unless (gst_pad_push_event (mysrcpad, gst_event_new_eos ()) == TRUE);

  num_buffers = g_list_length (buffers);
  fail_unless_equals_int (num_buffers, nbuffers);

  /* clean up buffers */
  for (i = 0; i < num_buffers; ++i) {
    GstMapInfo map;
    gsize size;
    guint8 *data;
    GstClockTime time, dur;

    outbuffer = GST_BUFFER (buffers->data);
    fail_if (outbuffer == NULL);

    gst_buffer_map (outbuffer, &map, GST_MAP_READ);
    data = map.data;
    size = map.size;

    /* at least for mode 0 */
    fail_unless (size == 18);
    fail_unless ((data[0] & 0x83) == 0);
    fail_unless (((data[0] >> 3) & 0xF) == 0);

    time = GST_BUFFER_TIMESTAMP (outbuffer);
    dur = GST_BUFFER_DURATION (outbuffer);
    fail_unless (time == 20 * GST_MSECOND * i);
    fail_unless (dur == 20 * GST_MSECOND);
    gst_buffer_unmap (outbuffer, &map);

    buffers = g_list_remove (buffers, outbuffer);

    ASSERT_BUFFER_REFCOUNT (outbuffer, "outbuffer", 1);
    gst_buffer_unref (outbuffer);
    outbuffer = NULL;
  }

  cleanup_voamrwbenc (voamrwbenc);
  g_list_free (buffers);
  buffers = NULL;
}

GST_START_TEST (test_enc_aligned)
{
  do_test (3200);
}

GST_END_TEST;

GST_START_TEST (test_enc_minus_one)
{
  do_test (3199);
}

GST_END_TEST;

GST_START_TEST (test_enc_plus_one)
{
  do_test (3201);
}

GST_END_TEST;

static Suite *
voamrwbenc_suite (void)
{
  Suite *s = suite_create ("voamrwbenc");
  TCase *tc_chain = tcase_create ("general");

  suite_add_tcase (s, tc_chain);
  tcase_add_test (tc_chain, test_enc_aligned);
  tcase_add_test (tc_chain, test_enc_minus_one);
  tcase_add_test (tc_chain, test_enc_plus_one);

  return s;
}

GST_CHECK_MAIN (voamrwbenc);
