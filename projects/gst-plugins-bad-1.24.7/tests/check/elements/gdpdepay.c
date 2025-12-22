/* GStreamer
 *
 * Copyright (C) 2006 Thomas Vander Stichele <thomas at apestaart dot org>
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

#include "config.h"
#include <string.h>

#include <gst/check/gstcheck.h>
#include <gst/audio/audio.h>
#include "../../gst/gdp/dataprotocol.c"

/* For ease of programming we use globals to keep refs for our floating
 * src and sink pads we create; otherwise we always have to do get_pad,
 * get_peer, and then remove references in every test function */
static GstPad *mysrcpad, *mysinkpad, *myshsinkpad;

#define FORMATS "{ S8, "GST_AUDIO_NE(S16)" }"

#define AUDIO_CAPS_TEMPLATE_STRING \
    "audio/x-raw, " \
    "format = (string) "FORMATS", " \
    "rate = (int) [ 1, MAX ], " \
    "channels = (int) [ 1, 8 ]"

#define AUDIO_CAPS_STRING \
    "audio/x-raw, " \
    "format = (string) "GST_AUDIO_NE(S16)", " \
    "rate = (int) 1000, " \
    "channels = (int) 2"

static GstStaticPadTemplate sinktemplate = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (AUDIO_CAPS_TEMPLATE_STRING)
    );
static GstStaticPadTemplate srctemplate = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("application/x-gdp")
    );

/* takes over reference for outcaps */
static GstElement *
setup_gdpdepay (void)
{
  GstElement *gdpdepay;

  GST_DEBUG ("setup_gdpdepay");
  gdpdepay = gst_check_setup_element ("gdpdepay");
  mysrcpad = gst_check_setup_src_pad (gdpdepay, &srctemplate);
  mysinkpad = gst_check_setup_sink_pad (gdpdepay, &sinktemplate);
  gst_pad_set_active (mysrcpad, TRUE);
  gst_pad_set_active (mysinkpad, TRUE);

  return gdpdepay;
}

static void
cleanup_gdpdepay (GstElement * gdpdepay)
{
  GST_DEBUG ("cleanup_gdpdepay");

  gst_pad_set_active (mysrcpad, FALSE);
  if (mysinkpad)
    gst_pad_set_active (mysinkpad, FALSE);
  if (myshsinkpad)
    gst_pad_set_active (myshsinkpad, FALSE);
  gst_check_teardown_src_pad (gdpdepay);
  gst_check_teardown_sink_pad (gdpdepay);
  gst_check_teardown_element (gdpdepay);
  mysinkpad = NULL;
  myshsinkpad = NULL;
}

static void
gdpdepay_push_mem_per_byte (const gchar * reason, GstBuffer * buf, guint nth)
{
  int i;
  GstBuffer *inbuffer;
  GstMapInfo map;
  GstMemory *mem;

  mem = gst_buffer_peek_memory (buf, nth);
  fail_unless (mem != NULL);

  gst_memory_map (mem, &map, GST_MAP_READ);

  for (i = 0; i < map.size; ++i) {
    inbuffer = gst_buffer_new_and_alloc (1);
    gst_buffer_fill (inbuffer, 0, map.data + i, 1);
    fail_unless (gst_pad_push (mysrcpad, inbuffer) == GST_FLOW_OK,
        "%s: failed pushing byte buffer", reason);
  }

  gst_memory_unmap (mem, &map);
}

GST_START_TEST (test_audio_per_byte)
{
  GstCaps *caps;
  GstPad *srcpad;
  GstElement *gdpdepay;
  GstBuffer *buffer, *outbuffer;
  GstEvent *event;
  GstSegment segment;

  gdpdepay = setup_gdpdepay ();
  srcpad = gst_element_get_static_pad (gdpdepay, "src");

  fail_unless (gst_element_set_state (gdpdepay,
          GST_STATE_PLAYING) == GST_STATE_CHANGE_SUCCESS,
      "could not set to playing");

  caps = gst_pad_query_caps (srcpad, NULL);
  fail_unless (gst_caps_is_any (caps));
  gst_caps_unref (caps);
  fail_if (gst_pad_get_current_caps (srcpad));

  caps = gst_caps_new_empty_simple ("application/x-gdp");
  gst_check_setup_events (mysrcpad, gdpdepay, caps, GST_FORMAT_BYTES);
  gst_caps_unref (caps);

  /* send stream-start event */
  event = gst_event_new_stream_start ("s-s-id-1234");
  buffer = gst_dp_payload_event (event, 0);
  gst_event_unref (event);
  fail_unless (buffer != NULL);
  gdpdepay_push_mem_per_byte ("stream-start header", buffer, 0);
  fail_unless_equals_int (g_list_length (buffers), 0);
  gdpdepay_push_mem_per_byte ("stream-start payload", buffer, 1);
  fail_unless_equals_int (g_list_length (buffers), 0);
  gst_buffer_unref (buffer);

  /* create caps and buffer packets and push them */
  caps = gst_caps_from_string (AUDIO_CAPS_STRING);
  buffer = gst_dp_payload_caps (caps, 0);
  gst_caps_unref (caps);
  fail_unless (buffer != NULL);
  gdpdepay_push_mem_per_byte ("caps header", buffer, 0);
  fail_unless_equals_int (g_list_length (buffers), 0);
  gdpdepay_push_mem_per_byte ("caps payload", buffer, 1);
  fail_unless_equals_int (g_list_length (buffers), 0);
  caps = gst_pad_query_caps (srcpad, NULL);
  fail_if (gst_caps_is_any (caps));
  gst_caps_unref (caps);
  gst_buffer_unref (buffer);

  /* send segment */
  gst_segment_init (&segment, GST_FORMAT_TIME);
  event = gst_event_new_segment (&segment);
  buffer = gst_dp_payload_event (event, 0);
  gst_event_unref (event);
  fail_unless (buffer != NULL);
  gdpdepay_push_mem_per_byte ("segment header", buffer, 0);
  fail_unless_equals_int (g_list_length (buffers), 0);
  gdpdepay_push_mem_per_byte ("segment payload", buffer, 1);
  fail_unless_equals_int (g_list_length (buffers), 0);
  gst_buffer_unref (buffer);

  buffer = gst_buffer_new_and_alloc (4);
  gst_buffer_fill (buffer, 0, "f00d", 4);
  GST_BUFFER_TIMESTAMP (buffer) = GST_SECOND;
  GST_BUFFER_DURATION (buffer) = GST_SECOND / 10;
  outbuffer = gst_dp_payload_buffer (buffer, 0);
  gst_buffer_unref (buffer);
  fail_unless (outbuffer != NULL);
  gdpdepay_push_mem_per_byte ("buffer header", outbuffer, 0);
  fail_unless_equals_int (g_list_length (buffers), 0);
  gdpdepay_push_mem_per_byte ("buffer payload", outbuffer, 1);
  gst_buffer_unref (outbuffer);

  fail_unless_equals_int (g_list_length (buffers), 1);
  fail_if ((outbuffer = (GstBuffer *) buffers->data) == NULL);
  fail_unless_equals_uint64 (GST_BUFFER_TIMESTAMP (outbuffer), GST_SECOND);
  fail_unless_equals_uint64 (GST_BUFFER_DURATION (outbuffer), GST_SECOND / 10);

  buffers = g_list_remove (buffers, outbuffer);
  gst_buffer_unref (outbuffer);

  fail_unless (gst_element_set_state (gdpdepay,
          GST_STATE_NULL) == GST_STATE_CHANGE_SUCCESS, "could not set to null");

  ASSERT_OBJECT_REFCOUNT (gdpdepay, "gdpdepay", 1);
  g_list_foreach (buffers, (GFunc) gst_mini_object_unref, NULL);
  g_list_free (buffers);
  buffers = NULL;
  gst_object_unref (srcpad);
  cleanup_gdpdepay (gdpdepay);
}

GST_END_TEST;

GST_START_TEST (test_audio_in_one_buffer)
{
  GstCaps *caps;
  GstPad *srcpad;
  GstElement *gdpdepay;
  GstBuffer *buffer, *inbuffer;
  GstBuffer *caps_buf, *streamstart_buf, *segment_buf, *data_buf;
  GstEvent *event;
  GstSegment segment;

  gdpdepay = setup_gdpdepay ();
  srcpad = gst_element_get_static_pad (gdpdepay, "src");

  fail_unless (gst_element_set_state (gdpdepay,
          GST_STATE_PLAYING) == GST_STATE_CHANGE_SUCCESS,
      "could not set to playing");

  /* make sure no caps are set yet */
  caps = gst_pad_query_caps (srcpad, NULL);
  fail_unless (gst_caps_is_any (caps));
  gst_caps_unref (caps);
  fail_if (gst_pad_get_current_caps (srcpad));

  caps = gst_caps_new_empty_simple ("application/x-gdp");
  gst_check_setup_events (mysrcpad, gdpdepay, caps, GST_FORMAT_BYTES);
  gst_caps_unref (caps);

  /* create stream-start event */
  event = gst_event_new_stream_start ("s-s-id-1234");
  streamstart_buf = gst_dp_payload_event (event, 0);
  gst_event_unref (event);

  /* create caps and buffer packets and push them as one buffer */
  caps = gst_caps_from_string (AUDIO_CAPS_STRING);
  caps_buf = gst_dp_payload_caps (caps, 0);
  gst_caps_unref (caps);

  /* create segment */
  gst_segment_init (&segment, GST_FORMAT_TIME);
  event = gst_event_new_segment (&segment);
  segment_buf = gst_dp_payload_event (event, 0);
  gst_event_unref (event);

  buffer = gst_buffer_new_and_alloc (4);
  gst_buffer_fill (buffer, 0, "f00d", 4);
  data_buf = gst_dp_payload_buffer (buffer, 0);
  gst_buffer_unref (buffer);

  inbuffer = gst_buffer_append (streamstart_buf, caps_buf);
  inbuffer = gst_buffer_append (inbuffer, segment_buf);
  inbuffer = gst_buffer_append (inbuffer, data_buf);

  /* now push it */
  gst_pad_push (mysrcpad, inbuffer);

  /* the buffer is still queued */
  fail_unless_equals_int (g_list_length (buffers), 1);

  fail_unless (gst_element_set_state (gdpdepay,
          GST_STATE_NULL) == GST_STATE_CHANGE_SUCCESS, "could not set to null");

  gst_object_unref (srcpad);
  g_list_foreach (buffers, (GFunc) gst_mini_object_unref, NULL);
  g_list_free (buffers);
  buffers = NULL;
  ASSERT_OBJECT_REFCOUNT (gdpdepay, "gdpdepay", 1);
  cleanup_gdpdepay (gdpdepay);
}

GST_END_TEST;

static GstStaticPadTemplate shsinktemplate = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("application/x-gst-test-streamheader")
    );

static GstElement *
setup_gdpdepay_streamheader (void)
{
  GstElement *gdpdepay;

  GST_DEBUG ("setup_gdpdepay");
  gdpdepay = gst_check_setup_element ("gdpdepay");
  mysrcpad = gst_check_setup_src_pad (gdpdepay, &srctemplate);
  myshsinkpad = gst_check_setup_sink_pad (gdpdepay, &shsinktemplate);
  gst_pad_set_active (mysrcpad, TRUE);
  gst_pad_set_active (myshsinkpad, TRUE);

  return gdpdepay;
}

/* this tests deserialization of a GDP stream where the serialized caps
 * have a streamheader set */
GST_START_TEST (test_streamheader)
{
  GstCaps *caps;
  GstPad *srcpad;
  GstElement *gdpdepay;
  GstBuffer *buffer, *inbuffer, *outbuffer, *shbuffer;
  GstBuffer *caps_buf, *ss_buf, *segment_buf, *data_buf;
  GstEvent *event;
  GstSegment segment;
  GstStructure *structure;
  GValue array = { 0 };
  GValue value = { 0 };

  gdpdepay = setup_gdpdepay_streamheader ();
  srcpad = gst_element_get_static_pad (gdpdepay, "src");
  ASSERT_OBJECT_REFCOUNT (gdpdepay, "gdpdepay", 1);

  fail_unless (gst_element_set_state (gdpdepay,
          GST_STATE_PLAYING) == GST_STATE_CHANGE_SUCCESS,
      "could not set to playing");

  /* make sure no caps are set yet */
  caps = gst_pad_query_caps (srcpad, NULL);
  fail_unless (gst_caps_is_any (caps));
  gst_caps_unref (caps);
  fail_if (gst_pad_get_current_caps (srcpad));

  caps = gst_caps_new_empty_simple ("application/x-gdp");
  gst_check_setup_events (mysrcpad, gdpdepay, caps, GST_FORMAT_BYTES);
  gst_caps_unref (caps);

  /* create a streamheader buffer and the caps containing it */
  caps = gst_caps_from_string ("application/x-gst-test-streamheader");
  structure = gst_caps_get_structure (caps, 0);
  buffer = gst_buffer_new_and_alloc (4);
  gst_buffer_fill (buffer, 0, "f00d", 4);
  GST_BUFFER_FLAG_SET (buffer, GST_BUFFER_FLAG_HEADER);
  g_value_init (&array, GST_TYPE_ARRAY);
  g_value_init (&value, GST_TYPE_BUFFER);
  shbuffer = gst_buffer_copy (buffer);
  gst_value_set_buffer (&value, shbuffer);
  gst_buffer_unref (shbuffer);
  gst_value_array_append_value (&array, &value);
  g_value_unset (&value);
  gst_structure_set_value (structure, "streamheader", &array);
  g_value_unset (&array);


  /* basic events */
  /* create stream-start event */
  event = gst_event_new_stream_start ("s-s-id-1234");
  ss_buf = gst_dp_payload_event (event, 0);
  gst_event_unref (event);

  /* create segment */
  gst_segment_init (&segment, GST_FORMAT_TIME);
  event = gst_event_new_segment (&segment);
  segment_buf = gst_dp_payload_event (event, 0);
  gst_event_unref (event);

  /* create GDP packets for the caps and the buffer, and put them in one
   * GDP buffer */
  caps_buf = gst_dp_payload_caps (caps, 0);
  gst_caps_unref (caps);

  data_buf = gst_dp_payload_buffer (buffer, 0);
  gst_buffer_unref (buffer);

  inbuffer = gst_buffer_append (ss_buf, caps_buf);
  inbuffer = gst_buffer_append (inbuffer, segment_buf);
  inbuffer = gst_buffer_append (inbuffer, data_buf);

  /* now push it */
  ASSERT_BUFFER_REFCOUNT (inbuffer, "inbuffer", 1);
  gst_pad_push (mysrcpad, inbuffer);

  /* our only output buffer is the streamheader buffer */
  fail_unless_equals_int (g_list_length (buffers), 1);
  fail_if ((outbuffer = (GstBuffer *) buffers->data) == NULL);
  buffers = g_list_remove (buffers, outbuffer);
  ASSERT_BUFFER_REFCOUNT (outbuffer, "outbuffer", 1);
  fail_unless (GST_BUFFER_FLAG_IS_SET (outbuffer, GST_BUFFER_FLAG_HEADER));

  /* FIXME: get streamheader, compare data with buffer */
  gst_buffer_unref (outbuffer);

  /* clean up */
  fail_unless (gst_element_set_state (gdpdepay,
          GST_STATE_NULL) == GST_STATE_CHANGE_SUCCESS, "could not set to null");

  gst_object_unref (srcpad);
  g_list_foreach (buffers, (GFunc) gst_mini_object_unref, NULL);
  g_list_free (buffers);
  buffers = NULL;
  ASSERT_OBJECT_REFCOUNT (gdpdepay, "gdpdepay", 1);
  cleanup_gdpdepay (gdpdepay);
}

GST_END_TEST;

static Suite *
gdpdepay_suite (void)
{
  Suite *s = suite_create ("gdpdepay");
  TCase *tc_chain = tcase_create ("general");

  suite_add_tcase (s, tc_chain);
  tcase_add_test (tc_chain, test_audio_per_byte);
  tcase_add_test (tc_chain, test_audio_in_one_buffer);
  tcase_add_test (tc_chain, test_streamheader);

  return s;
}

GST_CHECK_MAIN (gdpdepay);
