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
static GstPad *mysrcpad, *myshsrcpad, *mysinkpad;

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
    GST_STATIC_CAPS ("application/x-gdp")
    );
static GstStaticPadTemplate srctemplate = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (AUDIO_CAPS_TEMPLATE_STRING)
    );

/* takes over reference for outcaps */
static GstElement *
setup_gdppay (void)
{
  GstElement *gdppay;

  GST_DEBUG ("setup_gdppay");
  gdppay = gst_check_setup_element ("gdppay");
  mysrcpad = gst_check_setup_src_pad (gdppay, &srctemplate);
  mysinkpad = gst_check_setup_sink_pad (gdppay, &sinktemplate);
  gst_pad_set_active (mysrcpad, TRUE);
  gst_pad_set_active (mysinkpad, TRUE);

  return gdppay;
}

static void
cleanup_gdppay (GstElement * gdppay)
{
  GST_DEBUG ("cleanup_gdppay");

  if (mysrcpad)
    gst_pad_set_active (mysrcpad, FALSE);
  if (myshsrcpad)
    gst_pad_set_active (myshsrcpad, FALSE);
  gst_pad_set_active (mysinkpad, FALSE);
  gst_check_teardown_src_pad (gdppay);
  gst_check_teardown_sink_pad (gdppay);
  gst_check_teardown_element (gdppay);
  mysrcpad = NULL;
  myshsrcpad = NULL;
}

static void
check_stream_start_buffer (gint refcount)
{
  GstBuffer *outbuffer;

  fail_if ((outbuffer = (GstBuffer *) buffers->data) == NULL);
  buffers = g_list_remove (buffers, outbuffer);
  ASSERT_BUFFER_REFCOUNT (outbuffer, "outbuffer", refcount);
  gst_buffer_unref (outbuffer);
}

static void
check_caps_buffer (gint refcount, GstCaps * caps)
{
  GstBuffer *outbuffer;
  gchar *caps_string = gst_caps_to_string (caps);
  guint length;

  fail_if ((outbuffer = (GstBuffer *) buffers->data) == NULL);
  buffers = g_list_remove (buffers, outbuffer);
  ASSERT_BUFFER_REFCOUNT (outbuffer, "outbuffer", refcount);
  length = GST_DP_HEADER_LENGTH + (strlen (caps_string) + 1);
  fail_unless_equals_int (gst_buffer_get_size (outbuffer), length);
  gst_buffer_unref (outbuffer);
  g_free (caps_string);
}

static void
check_segment_buffer (gint refcount)
{
  GstBuffer *outbuffer;

  fail_if ((outbuffer = (GstBuffer *) buffers->data) == NULL);
  buffers = g_list_remove (buffers, outbuffer);
  ASSERT_BUFFER_REFCOUNT (outbuffer, "outbuffer", refcount);
  gst_buffer_unref (outbuffer);
}


GST_START_TEST (test_audio)
{
  GstCaps *caps;
  GstElement *gdppay;
  GstBuffer *inbuffer, *outbuffer;
  gint length;

  gdppay = setup_gdppay ();

  fail_unless (gst_element_set_state (gdppay,
          GST_STATE_PLAYING) == GST_STATE_CHANGE_SUCCESS,
      "could not set to playing");

  /* no buffer should be pushed yet, waiting for caps */
  fail_unless_equals_int (g_list_length (buffers), 0);

  GST_DEBUG ("first buffer");
  inbuffer = gst_buffer_new_and_alloc (4);
  gst_buffer_memset (inbuffer, 0, 0x00, 4);
  caps = gst_caps_from_string (AUDIO_CAPS_STRING);
  gst_check_setup_events (mysrcpad, gdppay, caps, GST_FORMAT_TIME);

  ASSERT_BUFFER_REFCOUNT (inbuffer, "inbuffer", 1);

  /* pushing gives away my reference */
  fail_unless (gst_pad_push (mysrcpad, inbuffer) == GST_FLOW_OK);

  /* we should have four buffers now */
  fail_unless_equals_int (g_list_length (buffers), 4);

  /* first buffer is the stream-start event */
  check_stream_start_buffer (1);

  /* second buffer is the serialized caps */
  check_caps_buffer (1, caps);

  /* third buffer is the serialized new_segment event */
  check_segment_buffer (1);

  /* the fourth buffer is the GDP buffer for our pushed buffer */
  fail_if ((outbuffer = (GstBuffer *) buffers->data) == NULL);
  buffers = g_list_remove (buffers, outbuffer);
  ASSERT_BUFFER_REFCOUNT (outbuffer, "outbuffer", 1);
  length = GST_DP_HEADER_LENGTH + 4;
  fail_unless_equals_int (gst_buffer_get_size (outbuffer), length);
  gst_buffer_unref (outbuffer);

  /* second buffer */
  GST_DEBUG ("second buffer");
  inbuffer = gst_buffer_new_and_alloc (4);
  gst_buffer_memset (inbuffer, 0, 0x00, 4);

  ASSERT_BUFFER_REFCOUNT (inbuffer, "inbuffer", 1);

  /* pushing gives away my reference */
  fail_unless (gst_pad_push (mysrcpad, inbuffer) == GST_FLOW_OK);

  fail_unless_equals_int (g_list_length (buffers), 1);
  fail_if ((outbuffer = (GstBuffer *) buffers->data) == NULL);
  buffers = g_list_remove (buffers, outbuffer);
  ASSERT_BUFFER_REFCOUNT (outbuffer, "outbuffer", 1);

  /* the third output buffer is data */
  length = GST_DP_HEADER_LENGTH + 4;
  fail_unless_equals_int (gst_buffer_get_size (outbuffer), length);
  gst_buffer_unref (outbuffer);

  /* a third buffer without caps set explicitly; should work */
  GST_DEBUG ("Creating third buffer, no caps set");
  inbuffer = gst_buffer_new_and_alloc (4);
  gst_buffer_memset (inbuffer, 0, 0x00, 4);

  ASSERT_BUFFER_REFCOUNT (inbuffer, "inbuffer", 1);

  /* pushing gives away my reference */
  fail_unless (gst_pad_push (mysrcpad, inbuffer) == GST_FLOW_OK);

  fail_unless_equals_int (g_list_length (buffers), 1);
  fail_if ((outbuffer = (GstBuffer *) buffers->data) == NULL);
  buffers = g_list_remove (buffers, outbuffer);
  ASSERT_BUFFER_REFCOUNT (outbuffer, "outbuffer", 1);

  /* the fourth output buffer is data */
  length = GST_DP_HEADER_LENGTH + 4;
  fail_unless_equals_int (gst_buffer_get_size (outbuffer), length);
  gst_buffer_unref (outbuffer);


  fail_unless (gst_element_set_state (gdppay,
          GST_STATE_NULL) == GST_STATE_CHANGE_SUCCESS, "could not set to null");

  gst_caps_unref (caps);
  g_list_foreach (buffers, (GFunc) gst_mini_object_unref, NULL);
  g_list_free (buffers);
  buffers = NULL;
  ASSERT_OBJECT_REFCOUNT (gdppay, "gdppay", 1);
  cleanup_gdppay (gdppay);
}

GST_END_TEST;

static GstStaticPadTemplate shsrctemplate = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("application/x-gst-test-streamheader")
    );


static GstElement *
setup_gdppay_streamheader (void)
{
  GstElement *gdppay;

  GST_DEBUG ("setup_gdppay");
  gdppay = gst_check_setup_element ("gdppay");
  myshsrcpad = gst_check_setup_src_pad (gdppay, &shsrctemplate);
  mysinkpad = gst_check_setup_sink_pad (gdppay, &sinktemplate);
  gst_pad_set_active (myshsrcpad, TRUE);
  gst_pad_set_active (mysinkpad, TRUE);

  return gdppay;
}

/* this test serializes a stream that already has a streamheader of its own.
 * the streamheader should then be serialized and put on the GDP stream's
 * streamheader */
GST_START_TEST (test_streamheader)
{
  GstCaps *caps, *sinkcaps;
  GstElement *gdppay;
  GstBuffer *inbuffer, *outbuffer, *shbuffer;
  gchar *caps_string;
  gint length;
  GstStructure *structure;
  GValue array = { 0 };
  GValue value = { 0 };
  const GValue *sh;
  GArray *shbuffers;

  gdppay = setup_gdppay_streamheader ();

  fail_unless (gst_element_set_state (gdppay,
          GST_STATE_PLAYING) == GST_STATE_CHANGE_SUCCESS,
      "could not set to playing");

  /* no buffer should be pushed yet, still waiting for caps */
  fail_unless_equals_int (g_list_length (buffers), 0);

  GST_DEBUG ("first buffer");
  inbuffer = gst_buffer_new_and_alloc (4);
  gst_buffer_fill (inbuffer, 0, "head", 4);
  caps = gst_caps_from_string ("application/x-gst-test-streamheader");
  structure = gst_caps_get_structure (caps, 0);
  GST_BUFFER_FLAG_SET (inbuffer, GST_BUFFER_FLAG_HEADER);
  g_value_init (&array, GST_TYPE_ARRAY);
  g_value_init (&value, GST_TYPE_BUFFER);
  shbuffer = gst_buffer_copy (inbuffer);
  gst_value_set_buffer (&value, shbuffer);
  gst_buffer_unref (shbuffer);
  gst_value_array_append_value (&array, &value);
  g_value_unset (&value);
  gst_structure_set_value (structure, "streamheader", &array);
  g_value_unset (&array);
  caps_string = gst_caps_to_string (caps);

  gst_check_setup_events (myshsrcpad, gdppay, caps, GST_FORMAT_TIME);
  ASSERT_BUFFER_REFCOUNT (inbuffer, "inbuffer", 1);

  /* pushing gives away my reference */
  fail_unless (gst_pad_push (myshsrcpad, inbuffer) == GST_FLOW_OK);

  /* we should have four buffers now */
  fail_unless_equals_int (g_list_length (buffers), 4);

  /* our sink pad should now have GDP caps with a streamheader that includes
   * GDP wrappings of our streamheader */
  sinkcaps = gst_pad_get_current_caps (mysinkpad);
  structure = gst_caps_get_structure (sinkcaps, 0);
  fail_unless_equals_string ((gchar *) gst_structure_get_name (structure),
      "application/x-gdp");
  fail_unless (gst_structure_has_field (structure, "streamheader"));
  sh = gst_structure_get_value (structure, "streamheader");
  fail_unless (G_VALUE_TYPE (sh) == GST_TYPE_ARRAY);
  shbuffers = g_value_peek_pointer (sh);
  /* a serialized stream-start-id, a serialized new_segment,
   * a serialized caps, and serialization of our
   * incoming streamheader */
  fail_unless_equals_int (shbuffers->len, 4);

  gst_caps_unref (sinkcaps);

  /* first buffer is the stream-start event */
  check_stream_start_buffer (1);

  /* second buffer is the serialized caps;
   * the element also holds a ref to it */
  check_caps_buffer (1, caps);

  /* third buffer is the serialized new_segment event;
   * the element also holds a ref to it */
  check_segment_buffer (1);

  /* the fourth buffer is the GDP buffer for our pushed buffer */
  fail_if ((outbuffer = (GstBuffer *) buffers->data) == NULL);
  buffers = g_list_remove (buffers, outbuffer);
  ASSERT_BUFFER_REFCOUNT (outbuffer, "outbuffer", 1);
  length = GST_DP_HEADER_LENGTH + 4;
  fail_unless_equals_int (gst_buffer_get_size (outbuffer), length);
  gst_buffer_unref (outbuffer);

  /* second buffer */
  GST_DEBUG ("second buffer");
  inbuffer = gst_buffer_new_and_alloc (4);
  gst_buffer_memset (inbuffer, 0, 0x02, 4);

  ASSERT_BUFFER_REFCOUNT (inbuffer, "inbuffer", 1);

  /* pushing gives away my reference */
  fail_unless (gst_pad_push (myshsrcpad, inbuffer) == GST_FLOW_OK);

  fail_unless_equals_int (g_list_length (buffers), 1);
  fail_if ((outbuffer = (GstBuffer *) buffers->data) == NULL);
  buffers = g_list_remove (buffers, outbuffer);
  ASSERT_BUFFER_REFCOUNT (outbuffer, "outbuffer", 1);

  /* the third output buffer is data */
  length = GST_DP_HEADER_LENGTH + 4;
  fail_unless_equals_int (gst_buffer_get_size (outbuffer), length);
  gst_buffer_unref (outbuffer);

  /* a third buffer without caps set explicitly; should work */
  GST_DEBUG ("Creating third buffer, no caps set");
  inbuffer = gst_buffer_new_and_alloc (4);
  gst_buffer_memset (inbuffer, 0, 0x03, 4);

  ASSERT_BUFFER_REFCOUNT (inbuffer, "inbuffer", 1);

  /* pushing gives away my reference */
  fail_unless (gst_pad_push (myshsrcpad, inbuffer) == GST_FLOW_OK);

  fail_unless_equals_int (g_list_length (buffers), 1);
  fail_if ((outbuffer = (GstBuffer *) buffers->data) == NULL);
  buffers = g_list_remove (buffers, outbuffer);
  ASSERT_BUFFER_REFCOUNT (outbuffer, "outbuffer", 1);

  /* the fourth output buffer is data */
  length = GST_DP_HEADER_LENGTH + 4;
  fail_unless_equals_int (gst_buffer_get_size (outbuffer), length);
  gst_buffer_unref (outbuffer);


  fail_unless (gst_element_set_state (gdppay,
          GST_STATE_NULL) == GST_STATE_CHANGE_SUCCESS, "could not set to null");

  gst_caps_unref (caps);
  g_free (caps_string);
  g_list_foreach (buffers, (GFunc) gst_mini_object_unref, NULL);
  g_list_free (buffers);
  buffers = NULL;
  ASSERT_OBJECT_REFCOUNT (gdppay, "gdppay", 1);
  cleanup_gdppay (gdppay);
}

GST_END_TEST;


GST_START_TEST (test_first_no_caps)
{
  GstElement *gdppay;
  GstBuffer *inbuffer;

  gdppay = setup_gdppay ();

  fail_unless (gst_element_set_state (gdppay,
          GST_STATE_PLAYING) == GST_STATE_CHANGE_SUCCESS,
      "could not set to playing");

  gst_check_setup_events (mysrcpad, gdppay, NULL, GST_FORMAT_TIME);

  GST_DEBUG ("first buffer");
  inbuffer = gst_buffer_new_and_alloc (4);
  gst_buffer_memset (inbuffer, 0, 0x01, 4);
  ASSERT_BUFFER_REFCOUNT (inbuffer, "inbuffer", 1);

  /* pushing should trigger an error */
  fail_unless (gst_pad_push (mysrcpad, inbuffer) == GST_FLOW_NOT_NEGOTIATED);

  fail_unless_equals_int (g_list_length (buffers), 0);

  fail_unless (gst_element_set_state (gdppay,
          GST_STATE_NULL) == GST_STATE_CHANGE_SUCCESS, "could not set to null");

  g_list_foreach (buffers, (GFunc) gst_mini_object_unref, NULL);
  g_list_free (buffers);
  buffers = NULL;
  ASSERT_OBJECT_REFCOUNT (gdppay, "gdppay", 1);
  cleanup_gdppay (gdppay);
}

GST_END_TEST;

/* element should still work if no new_segment is sent before the first
 * buffer */
GST_START_TEST (test_first_no_new_segment)
{
  GstElement *gdppay;
  GstBuffer *inbuffer;
  GstCaps *caps;

  gdppay = setup_gdppay ();

  fail_unless (gst_element_set_state (gdppay,
          GST_STATE_PLAYING) == GST_STATE_CHANGE_SUCCESS,
      "could not set to playing");

  GST_DEBUG ("first buffer");
  inbuffer = gst_buffer_new_and_alloc (4);
  gst_buffer_memset (inbuffer, 0, 0x01, 4);
  caps = gst_caps_from_string (AUDIO_CAPS_STRING);
  gst_check_setup_events (mysrcpad, gdppay, caps, GST_FORMAT_TIME);
  gst_caps_unref (caps);

  ASSERT_BUFFER_REFCOUNT (inbuffer, "inbuffer", 1);

  /* pushing gives away my reference */
  fail_unless (gst_pad_push (mysrcpad, inbuffer) == GST_FLOW_OK);

  /* we should have three buffers now;
   * one for the stream-start, one for an "invented" new segment,
   * one for GDP caps, and one with our buffer */
  fail_unless_equals_int (g_list_length (buffers), 4);

  fail_unless (gst_element_set_state (gdppay,
          GST_STATE_NULL) == GST_STATE_CHANGE_SUCCESS, "could not set to null");

  g_list_foreach (buffers, (GFunc) gst_mini_object_unref, NULL);
  g_list_free (buffers);
  buffers = NULL;
  ASSERT_OBJECT_REFCOUNT (gdppay, "gdppay", 1);
  cleanup_gdppay (gdppay);
}

GST_END_TEST;

GST_START_TEST (test_crc)
{
  GstCaps *caps;
  GstElement *gdppay;
  GstBuffer *inbuffer, *outbuffer;
  gint length;
  GstMapInfo map;
  guint16 crc_calculated, crc_read;

  gdppay = setup_gdppay ();
  g_object_set (gdppay, "crc-header", TRUE, NULL);

  fail_unless (gst_element_set_state (gdppay,
          GST_STATE_PLAYING) == GST_STATE_CHANGE_SUCCESS,
      "could not set to playing");

  /* no buffer should be pushed yet, waiting for caps */
  fail_unless_equals_int (g_list_length (buffers), 0);

  GST_DEBUG ("first buffer");
  inbuffer = gst_buffer_new_and_alloc (4);
  gst_buffer_memset (inbuffer, 0, g_random_int () & 0xff, 4);
  caps = gst_caps_from_string (AUDIO_CAPS_STRING);
  gst_check_setup_events (mysrcpad, gdppay, caps, GST_FORMAT_TIME);

  ASSERT_BUFFER_REFCOUNT (inbuffer, "inbuffer", 1);

  /* pushing gives away my reference */
  fail_unless (gst_pad_push (mysrcpad, inbuffer) == GST_FLOW_OK);

  /* we should have four buffers now */
  fail_unless_equals_int (g_list_length (buffers), 4);

  /* first buffer is the stream-start event */
  check_stream_start_buffer (1);

  /* second buffer is the serialized caps;
   * the element also holds a ref to it */
  check_caps_buffer (1, caps);

  /* third buffer is the serialized new_segment event */
  fail_if ((outbuffer = (GstBuffer *) buffers->data) == NULL);
  buffers = g_list_remove (buffers, outbuffer);
  ASSERT_BUFFER_REFCOUNT (outbuffer, "outbuffer", 1);

  /* verify the header checksum */
  /* CRC's start at 58 in the header */
  outbuffer = gst_buffer_make_writable (outbuffer);
  gst_buffer_map (outbuffer, &map, GST_MAP_READWRITE);
  crc_calculated = gst_dp_crc (map.data, 58);
  crc_read = GST_READ_UINT16_BE (map.data + 58);
  fail_unless_equals_int (crc_calculated, crc_read);

  /* change a byte in the header and verify that the checksum now fails */
  map.data[0] = 0xff;
  crc_calculated = gst_dp_crc (map.data, 58);
  fail_if (crc_calculated == crc_read,
      "Introducing a byte error in the header should make the checksum fail");

  gst_buffer_unmap (outbuffer, &map);
  gst_buffer_unref (outbuffer);

  /* the fourth buffer is the GDP buffer for our pushed buffer */
  fail_if ((outbuffer = (GstBuffer *) buffers->data) == NULL);
  buffers = g_list_remove (buffers, outbuffer);
  ASSERT_BUFFER_REFCOUNT (outbuffer, "outbuffer", 1);
  length = GST_DP_HEADER_LENGTH + 4;
  fail_unless_equals_int (gst_buffer_get_size (outbuffer), length);
  gst_buffer_unref (outbuffer);

  fail_unless (gst_element_set_state (gdppay,
          GST_STATE_NULL) == GST_STATE_CHANGE_SUCCESS, "could not set to null");

  gst_caps_unref (caps);
  g_list_foreach (buffers, (GFunc) gst_mini_object_unref, NULL);
  g_list_free (buffers);
  buffers = NULL;
  ASSERT_OBJECT_REFCOUNT (gdppay, "gdppay", 1);
  cleanup_gdppay (gdppay);
}

GST_END_TEST;


static Suite *
gdppay_suite (void)
{
  Suite *s = suite_create ("gdppay");
  TCase *tc_chain = tcase_create ("general");

  suite_add_tcase (s, tc_chain);
  tcase_add_test (tc_chain, test_audio);
  tcase_add_test (tc_chain, test_first_no_caps);
  tcase_add_test (tc_chain, test_first_no_new_segment);
  tcase_add_test (tc_chain, test_streamheader);
  tcase_add_test (tc_chain, test_crc);

  return s;
}

GST_CHECK_MAIN (gdppay);
