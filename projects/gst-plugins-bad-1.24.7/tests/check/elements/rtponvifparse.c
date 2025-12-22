/*
 * onviftimestamp.c
 *
 * Copyright (C) 2014 Axis Communications AB
 *  Author: Guillaume Desmottes <guillaume.desmottes@collabora.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */

#include <gst/check/gstcheck.h>
#include <gst/rtp/gstrtpbuffer.h>

/* For ease of programming we use globals to keep refs for our floating
 * src and sink pads we create; otherwise we always have to do get_pad,
 * get_peer, and then remove references in every test function */
static GstPad *mysrcpad, *mysinkpad;

static GstStaticPadTemplate sinktemplate = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("application/x-rtp")
    );
static GstStaticPadTemplate srctemplate = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("application/x-rtp")
    );

#define NTP_OFFSET (guint64) 1245
#define TIMESTAMP 42

static void
setup_element (GstElement * element)
{
  mysrcpad = gst_check_setup_src_pad (element, &srctemplate);
  mysinkpad = gst_check_setup_sink_pad (element, &sinktemplate);
  gst_pad_set_active (mysrcpad, TRUE);
  gst_pad_set_active (mysinkpad, TRUE);

  fail_unless (gst_element_set_state (element,
          GST_STATE_PLAYING) == GST_STATE_CHANGE_SUCCESS,
      "could not set to playing");
}

static void
cleanup_element (GstElement * element)
{
  fail_unless (gst_element_set_state (element,
          GST_STATE_NULL) == GST_STATE_CHANGE_SUCCESS, "could not set to null");

  gst_pad_set_active (mysrcpad, FALSE);
  if (mysinkpad)
    gst_pad_set_active (mysinkpad, FALSE);
  gst_check_teardown_src_pad (element);
  gst_check_teardown_sink_pad (element);
  gst_check_teardown_element (element);
  mysrcpad = NULL;
  mysinkpad = NULL;
}

static guint64
convert_to_ntp (guint64 t)
{
  guint64 ntptime;

  /* convert to NTP time. upper 32 bits should contain the seconds
   * and the lower 32 bits, the fractions of a second. */
  ntptime = gst_util_uint64_scale (t, (G_GINT64_CONSTANT (1) << 32),
      GST_SECOND);

  return ntptime;
}

/* Create a copy of @buffer_in having the RTP extension */
static GstBuffer *
create_extension_buffer (GstBuffer * buffer_in, gboolean clean_point,
    gboolean end_contiguous, gboolean discont)
{
  GstBuffer *buffer_out;
  GstRTPBuffer rtpbuffer_out = GST_RTP_BUFFER_INIT;
  guint8 *data;
  guint8 flags = 0;

  buffer_out = gst_buffer_copy (buffer_in);

  fail_unless (gst_rtp_buffer_map (buffer_out, GST_MAP_READWRITE,
          &rtpbuffer_out));

  /* extension */
  gst_rtp_buffer_set_extension_data (&rtpbuffer_out, 0xABAC, 3);
  fail_unless (gst_rtp_buffer_get_extension (&rtpbuffer_out));
  gst_rtp_buffer_get_extension_data (&rtpbuffer_out, NULL, (gpointer) & data,
      NULL);

  /* NTP timestamp */
  GST_WRITE_UINT64_BE (data, convert_to_ntp (buffer_in->pts + NTP_OFFSET));

  /* C E D mbz */
  if (clean_point)
    flags |= (1 << 7);
  if (end_contiguous)
    flags |= (1 << 6);
  if (discont)
    flags |= (1 << 5);

  GST_WRITE_UINT8 (data + 8, flags);

  /* CSeq */
  GST_WRITE_UINT8 (data + 9, 0x78);

  memset (data + 10, 0, 4);

  gst_rtp_buffer_unmap (&rtpbuffer_out);

  return buffer_out;
}

static GstElement *
setup_rtponvifparse (gboolean set_e_bit)
{
  GstElement *parse;

  GST_DEBUG ("setup_rtponvifparse");
  parse = gst_check_setup_element ("rtponvifparse");

  setup_element (parse);

  return parse;
}

static void
cleanup_rtponvifparse (GstElement * parse)
{
  GST_DEBUG ("cleanup_rtponvifparse");

  cleanup_element (parse);
}

static void
test_parse (gboolean clean_point, gboolean discont)
{
  GstElement *parse;
  GstBuffer *rtp, *buf;
  GstSegment segment;

  parse = setup_rtponvifparse (FALSE);

  rtp = gst_rtp_buffer_new_allocate (4, 0, 0);
  buf = create_extension_buffer (rtp, clean_point, FALSE, discont);
  gst_buffer_unref (rtp);

  /* stream start */
  fail_unless (gst_pad_push_event (mysrcpad,
          gst_event_new_stream_start ("test")));

  /* Push a segment */
  gst_segment_init (&segment, GST_FORMAT_TIME);
  fail_unless (gst_pad_push_event (mysrcpad, gst_event_new_segment (&segment)));

  /* Push buffer */
  fail_unless (gst_pad_push (mysrcpad, buf) == GST_FLOW_OK,
      "failed pushing buffer");

  g_assert_cmpuint (g_list_length (buffers), ==, 1);
  buf = buffers->data;

  if (clean_point)
    g_assert (!GST_BUFFER_FLAG_IS_SET (buf, GST_BUFFER_FLAG_DELTA_UNIT));
  else
    g_assert (GST_BUFFER_FLAG_IS_SET (buf, GST_BUFFER_FLAG_DELTA_UNIT));

  if (discont)
    g_assert (GST_BUFFER_IS_DISCONT (buf));
  else
    g_assert (!GST_BUFFER_IS_DISCONT (buf));

  g_list_foreach (buffers, (GFunc) gst_mini_object_unref, NULL);
  g_list_free (buffers);
  buffers = NULL;

  ASSERT_OBJECT_REFCOUNT (parse, "rtponvifparse", 1);
  cleanup_rtponvifparse (parse);
}

GST_START_TEST (test_parse_no_flag)
{
  test_parse (FALSE, FALSE);
}

GST_END_TEST;

GST_START_TEST (test_parse_clean_point)
{
  test_parse (TRUE, FALSE);
}

GST_END_TEST;

GST_START_TEST (test_parse_discont)
{
  test_parse (FALSE, TRUE);
}

GST_END_TEST;

static Suite *
onviftimestamp_suite (void)
{
  Suite *s = suite_create ("onviftimestamp");
  TCase *tc_chain;

  tc_chain = tcase_create ("parse");
  suite_add_tcase (s, tc_chain);
  tcase_add_test (tc_chain, test_parse_no_flag);
  tcase_add_test (tc_chain, test_parse_clean_point);
  tcase_add_test (tc_chain, test_parse_discont);

  return s;
}

GST_CHECK_MAIN (onviftimestamp);
