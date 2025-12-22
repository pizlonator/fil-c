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
static GstElement *element;
static GstPad *mysrcpad;
static GstPad *mysinkpad;
/* These are global mainly because they are used from the setup/cleanup
 * fixture functions */
static gulong myprobe;
static GList *mypushedevents;
static GList *myreceivedevents;

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

#define NTP_OFFSET  ((guint64) 1245)
#define TIMESTAMP   ((GstClockTime)42)
#define CSEQ        0x78
#define COMPARE     TRUE
#define NO_COMPARE  FALSE

static GstPadProbeReturn
event_probe (GstPad * pad, GstPadProbeInfo * info, gpointer user_data)
{
  GstEvent *event = GST_PAD_PROBE_INFO_EVENT (info);

  GST_INFO ("got %" GST_PTR_FORMAT, event);
  myreceivedevents = g_list_append (myreceivedevents, gst_event_ref (event));

  return GST_PAD_PROBE_OK;
}

static GstEvent *
create_ntp_offset_event (GstClockTime ntp_offset, gboolean discont)
{
  GstStructure *structure;

  structure =
      gst_structure_new ("GstOnvifTimestamp", "ntp-offset", G_TYPE_UINT64,
      ntp_offset, "discont", G_TYPE_BOOLEAN, discont, NULL);

  return gst_event_new_custom (GST_EVENT_CUSTOM_DOWNSTREAM, structure);
}

static GstEvent *
create_event (GstEventType type)
{
  GstEvent *event = NULL;

  switch (type) {
    case GST_EVENT_CUSTOM_DOWNSTREAM:
      event = gst_event_new_custom (GST_EVENT_CUSTOM_DOWNSTREAM,
          gst_structure_new ("x-app/test", "test-field", G_TYPE_STRING,
              "test-value", NULL));
      break;
    case GST_EVENT_CUSTOM_DOWNSTREAM_OOB:
      event = gst_event_new_custom (GST_EVENT_CUSTOM_DOWNSTREAM_OOB,
          gst_structure_new ("x-app/test", "test-field", G_TYPE_STRING,
              "test-value", NULL));
      break;
    case GST_EVENT_EOS:
      event = gst_event_new_eos ();
      break;
    default:
      g_assert_not_reached ();
      break;
  }

  return event;
}

static void
create_and_push_event (GstEventType type)
{
  GstEvent *event = create_event (type);

  mypushedevents = g_list_append (mypushedevents, event);
  fail_unless (gst_pad_push_event (mysrcpad, event));
}

static void
check_and_clear_events (gint expected, gboolean compare)
{
  GList *p;
  GList *r;

  /* verify that there's as many queued events as expected */
  fail_unless_equals_int (g_list_length (myreceivedevents), expected);

  if (compare) {
    fail_unless_equals_int (expected, g_list_length (mypushedevents));

    /* verify that the events are queued in the expected order */
    r = myreceivedevents;
    p = mypushedevents;

    while (p != NULL) {
      fail_unless_equals_pointer (p->data, r->data);
      p = g_list_next (p);
      r = g_list_next (r);
    }
  }

  g_list_free_full (myreceivedevents, (GDestroyNotify) gst_event_unref);
  myreceivedevents = NULL;
  g_list_free (mypushedevents);
  mypushedevents = NULL;
}

static void
setup (void)
{
  element = gst_check_setup_element ("rtponviftimestamp");

  mysinkpad = gst_check_setup_sink_pad (element, &sinktemplate);
  gst_pad_set_active (mysinkpad, TRUE);

  mysrcpad = gst_check_setup_src_pad (element, &srctemplate);
  gst_pad_set_active (mysrcpad, TRUE);
}

static void
cleanup (void)
{
  gst_check_drop_buffers ();

  gst_pad_set_active (mysrcpad, FALSE);
  gst_check_teardown_src_pad (element);
  mysrcpad = NULL;

  gst_pad_set_active (mysinkpad, FALSE);
  gst_check_teardown_sink_pad (element);
  mysinkpad = NULL;

  gst_check_teardown_element (element);
  element = NULL;

  gst_check_drop_buffers ();
}

static void
setup_with_event (void)
{
  setup ();

  myprobe = gst_pad_add_probe (mysinkpad,
      GST_PAD_PROBE_TYPE_EVENT_DOWNSTREAM, event_probe, NULL, NULL);
  myreceivedevents = NULL;
  mypushedevents = NULL;
}

static void
cleanup_with_event (void)
{
  gst_pad_remove_probe (mysinkpad, myprobe);
  myprobe = 0;
  myreceivedevents = NULL;
  mypushedevents = NULL;

  cleanup ();
}

static void
check_buffer_equal (GstBuffer * buf, GstBuffer * expected)
{
  GstMapInfo info_buf, info_expected;

  fail_if (buf == NULL);
  fail_if (expected == NULL);

  fail_unless (gst_buffer_map (buf, &info_buf, GST_MAP_READ));
  fail_unless (gst_buffer_map (expected, &info_expected, GST_MAP_READ));

  GST_LOG ("buffer: size %" G_GSIZE_FORMAT, info_buf.size);
  GST_LOG ("expected: size %" G_GSIZE_FORMAT, info_expected.size);
  GST_MEMDUMP ("buffer", info_buf.data, info_buf.size);
  GST_MEMDUMP ("expected", info_expected.data, info_expected.size);

  fail_unless_equals_uint64 (info_buf.size, info_expected.size);
  fail_unless_equals_int (memcmp (info_buf.data, info_expected.data,
          info_buf.size), 0);

  gst_buffer_unmap (buf, &info_buf);
  gst_buffer_unmap (expected, &info_expected);
}

/* Create a RTP buffer without the extension */
static GstBuffer *
create_rtp_buffer (GstClockTime timestamp, gboolean clean_point)
{
  GstBuffer *buffer_in;
  GstRTPBuffer rtpbuffer_in = GST_RTP_BUFFER_INIT;

  buffer_in = gst_rtp_buffer_new_allocate (0, 0, 0);
  GST_BUFFER_PTS (buffer_in) = timestamp;

  if (!clean_point)
    GST_BUFFER_FLAG_SET (buffer_in, GST_BUFFER_FLAG_DELTA_UNIT);

  fail_unless (gst_rtp_buffer_map (buffer_in, GST_MAP_READ, &rtpbuffer_in));
  fail_if (gst_rtp_buffer_get_extension (&rtpbuffer_in));
  gst_rtp_buffer_unmap (&rtpbuffer_in);

  return buffer_in;
}

static guint64
convert_to_ntp (GstClockTime t)
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
    gboolean end_contiguous, gboolean discont, guint64 ntp_offset, guint8 cseq,
    gboolean first_buffer)
{
  GstBuffer *buffer_out;
  GstRTPBuffer rtpbuffer_out = GST_RTP_BUFFER_INIT;
  guint8 *data;
  guint8 flags = 0;

  buffer_out = gst_buffer_copy (buffer_in);

  fail_unless (gst_rtp_buffer_map (buffer_out, GST_MAP_READWRITE,
          &rtpbuffer_out));

  /* extension */
  fail_unless (gst_rtp_buffer_set_extension_data (&rtpbuffer_out, 0xABAC, 3));
  fail_unless (gst_rtp_buffer_get_extension (&rtpbuffer_out));
  fail_unless (gst_rtp_buffer_get_extension_data (&rtpbuffer_out, NULL,
          (gpointer) & data, NULL));

  /* NTP timestamp */
  GST_WRITE_UINT64_BE (data, convert_to_ntp (GST_BUFFER_PTS (buffer_in) +
          ntp_offset));

  /* C E D mbz */
  if (first_buffer)
    flags |= (1 << 5);
  if (clean_point)
    flags |= (1 << 7);
  if (end_contiguous)
    flags |= (1 << 6);
  if (discont)
    flags |= (1 << 5);

  GST_WRITE_UINT8 (data + 8, flags);

  /* CSeq */
  GST_WRITE_UINT8 (data + 9, cseq);

  memset (data + 10, 0, 4);

  gst_rtp_buffer_unmap (&rtpbuffer_out);

  return buffer_out;
}

static void
do_one_buffer_test_apply (gboolean clean_point)
{
  GstBuffer *buffer_in, *buffer_out;

  g_object_set (element, "ntp-offset", NTP_OFFSET, "cseq", 0x12345678,
      "set-e-bit", FALSE, NULL);

  ASSERT_SET_STATE (element, GST_STATE_PLAYING, GST_STATE_CHANGE_SUCCESS);

  buffer_in = create_rtp_buffer (TIMESTAMP, clean_point);
  buffer_out = create_extension_buffer (buffer_in, clean_point, FALSE, FALSE,
      NTP_OFFSET, CSEQ, TRUE);

  /* push initial events */
  gst_check_setup_events (mysrcpad, element, NULL, GST_FORMAT_TIME);

  /* Push buffer */
  fail_unless_equals_int (gst_pad_push (mysrcpad, buffer_in), GST_FLOW_OK);

  check_buffer_equal ((GstBuffer *) buffers->data, buffer_out);
  gst_buffer_unref (buffer_out);

  ASSERT_SET_STATE (element, GST_STATE_NULL, GST_STATE_CHANGE_SUCCESS);
}

static void
do_two_buffers_test_apply (gboolean end_contiguous)
{
  GstBuffer *buffer_in, *buffer_out;
  GList *node;

  g_object_set (element, "ntp-offset", NTP_OFFSET, "cseq", 0x12345678,
      "set-e-bit", TRUE, NULL);

  ASSERT_SET_STATE (element, GST_STATE_PLAYING, GST_STATE_CHANGE_SUCCESS);

  buffer_in = create_rtp_buffer (TIMESTAMP, FALSE);
  buffer_out = create_extension_buffer (buffer_in, FALSE, end_contiguous,
      FALSE, NTP_OFFSET, CSEQ, TRUE);

  /* push initial events */
  gst_check_setup_events (mysrcpad, element, NULL, GST_FORMAT_TIME);

  /* Push buffer */
  fail_unless_equals_int (gst_pad_push (mysrcpad, buffer_in), GST_FLOW_OK);

  /* The buffer hasn't been pushed it as the element is waiting for the next
   * buffer. */
  fail_unless_equals_int (g_list_length (buffers), 0);

  /* push an ntp-offset event to trigger a discontinuty */
  fail_unless (gst_pad_push_event (mysrcpad,
          create_ntp_offset_event (NTP_OFFSET, end_contiguous)));

  /* A second buffer is pushed */
  buffer_in = create_rtp_buffer (TIMESTAMP + 1, FALSE);

  fail_unless_equals_int (gst_pad_push (mysrcpad, buffer_in), GST_FLOW_OK);

  /* The first buffer has now been pushed out */
  fail_unless_equals_int (g_list_length (buffers), 1);

  node = g_list_last (buffers);
  check_buffer_equal ((GstBuffer *) node->data, buffer_out);
  gst_buffer_unref (buffer_out);

  /* Push EOS */
  fail_unless (gst_pad_push_event (mysrcpad, gst_event_new_eos ()));

  /* The second buffer has been pushed out */
  fail_unless_equals_int (g_list_length (buffers), 2);

  /* Last buffer always has the 'E' flag */
  buffer_out = create_extension_buffer (buffer_in, FALSE, TRUE, end_contiguous,
      NTP_OFFSET, CSEQ, FALSE);
  node = g_list_last (buffers);
  check_buffer_equal ((GstBuffer *) node->data, buffer_out);
  gst_buffer_unref (buffer_out);

  ASSERT_SET_STATE (element, GST_STATE_NULL, GST_STATE_CHANGE_SUCCESS);
}

GST_START_TEST (test_apply_clean_point)
{
  do_one_buffer_test_apply (TRUE);
}

GST_END_TEST;

GST_START_TEST (test_apply_no_e_bit)
{
  do_two_buffers_test_apply (FALSE);
}

GST_END_TEST;

GST_START_TEST (test_apply_e_bit)
{
  do_two_buffers_test_apply (TRUE);
}

GST_END_TEST;

GST_START_TEST (test_flushing)
{
  GstBuffer *buffer;

  /* set the e-bit, so the element use caching */
  g_object_set (element, "set-e-bit", TRUE, NULL);
  /* set the ntp-offset, since no one will provide a clock */
  g_object_set (element, "ntp-offset", NTP_OFFSET, NULL);

  ASSERT_SET_STATE (element, GST_STATE_PLAYING, GST_STATE_CHANGE_SUCCESS);
  gst_check_setup_events (mysrcpad, element, NULL, GST_FORMAT_TIME);

  /* create and push the first buffer */
  buffer = create_rtp_buffer (TIMESTAMP, TRUE);
  fail_unless_equals_int (gst_pad_push (mysrcpad, buffer), GST_FLOW_OK);

  /* no buffers should have made it through */
  fail_unless_equals_int (g_list_length (buffers), 0);

  /* flush the element */
  fail_unless (gst_pad_push_event (mysrcpad, gst_event_new_flush_start ()));
  fail_unless (gst_pad_push_event (mysrcpad, gst_event_new_flush_stop (FALSE)));

  /* resend events */
  gst_check_setup_events (mysrcpad, element, NULL, GST_FORMAT_TIME);

  /* create and push a second buffer */
  buffer = create_rtp_buffer (TIMESTAMP + 1, TRUE);
  fail_unless_equals_int (gst_pad_push (mysrcpad, buffer), GST_FLOW_OK);

  /* still no buffers should have made it through (the first one should have
   * been dropped during flushing) */
  fail_unless_equals_int (g_list_length (buffers), 0);

  ASSERT_SET_STATE (element, GST_STATE_NULL, GST_STATE_CHANGE_SUCCESS);
}

GST_END_TEST;

GST_START_TEST (test_reusable_element_no_e_bit)
{
  GstBuffer *buffer;

  /* set the ntp-offset, since no one will provide a clock */
  g_object_set (element, "ntp-offset", NTP_OFFSET, NULL);

  ASSERT_SET_STATE (element, GST_STATE_PLAYING, GST_STATE_CHANGE_SUCCESS);
  gst_check_setup_events (mysrcpad, element, NULL, GST_FORMAT_TIME);

  /* create and push the first buffer */
  buffer = create_rtp_buffer (TIMESTAMP, TRUE);
  fail_unless_equals_int (gst_pad_push (mysrcpad, buffer), GST_FLOW_OK);

  /* create and push a second buffer */
  buffer = create_rtp_buffer (TIMESTAMP + 1, TRUE);
  fail_unless_equals_int (gst_pad_push (mysrcpad, buffer), GST_FLOW_OK);

  /* create and push a third buffer */
  buffer = create_rtp_buffer (TIMESTAMP + 2, TRUE);
  fail_unless_equals_int (gst_pad_push (mysrcpad, buffer), GST_FLOW_OK);

  ASSERT_SET_STATE (element, GST_STATE_NULL, GST_STATE_CHANGE_SUCCESS);

  fail_unless_equals_int (g_list_length (buffers), 3);

  ASSERT_SET_STATE (element, GST_STATE_PLAYING, GST_STATE_CHANGE_SUCCESS);
  gst_check_setup_events (mysrcpad, element, NULL, GST_FORMAT_TIME);

  /* create and push the first buffer */
  buffer = create_rtp_buffer (TIMESTAMP, TRUE);
  fail_unless_equals_int (gst_pad_push (mysrcpad, buffer), GST_FLOW_OK);

  /* create and push a second buffer */
  buffer = create_rtp_buffer (TIMESTAMP + 1, TRUE);
  fail_unless_equals_int (gst_pad_push (mysrcpad, buffer), GST_FLOW_OK);

  /* create and push a third buffer */
  buffer = create_rtp_buffer (TIMESTAMP + 2, TRUE);
  fail_unless_equals_int (gst_pad_push (mysrcpad, buffer), GST_FLOW_OK);

  ASSERT_SET_STATE (element, GST_STATE_NULL, GST_STATE_CHANGE_SUCCESS);

  fail_unless_equals_int (g_list_length (buffers), 6);
}

GST_END_TEST;

GST_START_TEST (test_reusable_element_e_bit)
{
  GstBuffer *buffer;

  /* set the e-bit, so the element use caching */
  g_object_set (element, "set-e-bit", TRUE, NULL);
  /* set the ntp-offset, since no one will provide a clock */
  g_object_set (element, "ntp-offset", NTP_OFFSET, NULL);

  ASSERT_SET_STATE (element, GST_STATE_PLAYING, GST_STATE_CHANGE_SUCCESS);
  gst_check_setup_events (mysrcpad, element, NULL, GST_FORMAT_TIME);

  /* create and push the first buffer */
  buffer = create_rtp_buffer (TIMESTAMP, TRUE);
  fail_unless_equals_int (gst_pad_push (mysrcpad, buffer), GST_FLOW_OK);

  /* create and push a second buffer */
  buffer = create_rtp_buffer (TIMESTAMP + 1, TRUE);
  fail_unless_equals_int (gst_pad_push (mysrcpad, buffer), GST_FLOW_OK);

  /* create and push a third buffer */
  buffer = create_rtp_buffer (TIMESTAMP + 2, TRUE);
  fail_unless_equals_int (gst_pad_push (mysrcpad, buffer), GST_FLOW_OK);

  ASSERT_SET_STATE (element, GST_STATE_NULL, GST_STATE_CHANGE_SUCCESS);

  fail_unless_equals_int (g_list_length (buffers), 2);

  ASSERT_SET_STATE (element, GST_STATE_PLAYING, GST_STATE_CHANGE_SUCCESS);
  gst_check_setup_events (mysrcpad, element, NULL, GST_FORMAT_TIME);

  /* create and push the first buffer */
  buffer = create_rtp_buffer (TIMESTAMP, TRUE);
  fail_unless_equals_int (gst_pad_push (mysrcpad, buffer), GST_FLOW_OK);

  /* create and push a second buffer */
  buffer = create_rtp_buffer (TIMESTAMP + 1, TRUE);
  fail_unless_equals_int (gst_pad_push (mysrcpad, buffer), GST_FLOW_OK);

  /* create and push a third buffer */
  buffer = create_rtp_buffer (TIMESTAMP + 2, TRUE);
  fail_unless_equals_int (gst_pad_push (mysrcpad, buffer), GST_FLOW_OK);

  ASSERT_SET_STATE (element, GST_STATE_NULL, GST_STATE_CHANGE_SUCCESS);

  fail_unless_equals_int (g_list_length (buffers), 4);
}

GST_END_TEST;

GST_START_TEST (test_ntp_offset_event)
{
  GstBuffer *buffer_in, *buffer1_out, *buffer2_out;
  GList *node;

  /* set the e-bit, so the element use caching */
  g_object_set (element, "set-e-bit", TRUE, NULL);

  ASSERT_SET_STATE (element, GST_STATE_PLAYING, GST_STATE_CHANGE_SUCCESS);
  gst_check_setup_events (mysrcpad, element, NULL, GST_FORMAT_TIME);

  /* push an ntp-offset event */
  fail_unless (gst_pad_push_event (mysrcpad,
          create_ntp_offset_event (NTP_OFFSET, TRUE)));

  /* create and push the first buffer */
  buffer_in = create_rtp_buffer (TIMESTAMP, TRUE);
  buffer1_out = create_extension_buffer (buffer_in, TRUE, TRUE, FALSE,
      NTP_OFFSET, 0, TRUE);
  fail_unless_equals_int (gst_pad_push (mysrcpad, buffer_in), GST_FLOW_OK);

  /* push a new ntp offset */
  fail_unless (gst_pad_push_event (mysrcpad,
          create_ntp_offset_event (2 * NTP_OFFSET, TRUE)));

  /* create and push a second buffer (last) */
  buffer_in = create_rtp_buffer (TIMESTAMP + 1, TRUE);
  buffer2_out = create_extension_buffer (buffer_in, TRUE, TRUE, TRUE,
      2 * NTP_OFFSET, 0, FALSE);
  fail_unless_equals_int (gst_pad_push (mysrcpad, buffer_in), GST_FLOW_OK);

  /* the first buffer should have been pushed now */
  fail_unless_equals_int (g_list_length (buffers), 1);
  node = g_list_last (buffers);
  check_buffer_equal ((GstBuffer *) node->data, buffer1_out);
  gst_buffer_unref (buffer1_out);

  /* push EOS */
  fail_unless (gst_pad_push_event (mysrcpad, gst_event_new_eos ()));

  /* the second buffer has now been pushed */
  fail_unless_equals_int (g_list_length (buffers), 2);
  node = g_list_last (buffers);
  check_buffer_equal ((GstBuffer *) node->data, buffer2_out);
  gst_buffer_unref (buffer2_out);

  ASSERT_SET_STATE (element, GST_STATE_NULL, GST_STATE_CHANGE_SUCCESS);
}

GST_END_TEST;

GST_START_TEST (test_serialized_events)
{
  GstBuffer *buffer;

  /* we want the e-bit set so that buffers are cached */
  g_object_set (element, "set-e-bit", TRUE, NULL);
  g_object_set (element, "ntp-offset", NTP_OFFSET, NULL);

  ASSERT_SET_STATE (element, GST_STATE_PLAYING, GST_STATE_CHANGE_SUCCESS);

  /* send intitial events (stream-start and segment) */
  gst_check_setup_events (mysrcpad, element, NULL, GST_FORMAT_TIME);
  check_and_clear_events (2, NO_COMPARE);

  /* events received while no buffer is cached should be forwarded */
  create_and_push_event (GST_EVENT_CUSTOM_DOWNSTREAM);
  check_and_clear_events (1, NO_COMPARE);

  /* create and push the first buffer, which should be cached */
  buffer = create_rtp_buffer (TIMESTAMP, TRUE);
  fail_unless_equals_int (gst_pad_push (mysrcpad, buffer), GST_FLOW_OK);
  fail_unless_equals_int (g_list_length (buffers), 0);
  /* serialized events should be queued when there's a buffer cached */
  create_and_push_event (GST_EVENT_CUSTOM_DOWNSTREAM);
  fail_unless_equals_int (g_list_length (myreceivedevents), 0);
  /* there's still a buffer cached... */
  create_and_push_event (GST_EVENT_CUSTOM_DOWNSTREAM);
  fail_unless_equals_int (g_list_length (myreceivedevents), 0);

  /* receiving a new buffer should let the first through, along with the
   * queued serialized events  */
  buffer = create_rtp_buffer (TIMESTAMP, TRUE);
  fail_unless_equals_int (gst_pad_push (mysrcpad, buffer), GST_FLOW_OK);
  fail_unless_equals_int (g_list_length (buffers), 1);
  check_and_clear_events (2, COMPARE);

  /* there's still a buffer cached, a new serialized event should be quueud */
  create_and_push_event (GST_EVENT_CUSTOM_DOWNSTREAM);
  fail_unless_equals_int (g_list_length (myreceivedevents), 0);

  /* when receiving an EOS cached buffer and queued events should be forwarded */
  create_and_push_event (GST_EVENT_EOS);
  check_and_clear_events (2, COMPARE);
}

GST_END_TEST;

GST_START_TEST (test_non_serialized_events)
{
  GstEvent *event;
  GstBuffer *buffer;

  /* we want the e-bit set so that buffers are cached */
  g_object_set (element, "set-e-bit", TRUE, NULL);
  g_object_set (element, "ntp-offset", NTP_OFFSET, NULL);

  ASSERT_SET_STATE (element, GST_STATE_PLAYING, GST_STATE_CHANGE_SUCCESS);

  /* send intitial events (stream-start and segment) */
  gst_check_setup_events (mysrcpad, element, NULL, GST_FORMAT_TIME);
  fail_unless_equals_int (g_list_length (myreceivedevents), 2);
  check_and_clear_events (2, NO_COMPARE);

  /* events received while no buffer is cached should be forwarded */
  create_and_push_event (GST_EVENT_CUSTOM_DOWNSTREAM_OOB);
  check_and_clear_events (1, COMPARE);

  /* create and push the first buffer, which should be cached */
  buffer = create_rtp_buffer (TIMESTAMP, TRUE);
  fail_unless_equals_int (gst_pad_push (mysrcpad, buffer), GST_FLOW_OK);
  fail_unless_equals_int (g_list_length (buffers), 0);
  /* non-serialized events should be forwarded regardless of whether
   * there is a cached buffer */
  create_and_push_event (GST_EVENT_CUSTOM_DOWNSTREAM_OOB);
  check_and_clear_events (1, COMPARE);

  /* there's still a buffer cached, push a serialized event and make sure
   * it's queued */
  create_and_push_event (GST_EVENT_CUSTOM_DOWNSTREAM);
  fail_unless_equals_int (g_list_length (myreceivedevents), 0);
  /* non-serialized events should be forwarded regardless of whether there
   * are serialized events queued, thus the g_list_prepend below */
  event = create_event (GST_EVENT_CUSTOM_DOWNSTREAM_OOB);
  mypushedevents = g_list_prepend (mypushedevents, event);
  fail_unless (gst_pad_push_event (mysrcpad, event));
  fail_unless_equals_int (g_list_length (myreceivedevents), 1);

  /* when receiving an EOS cached buffer and queued events should be forwarded */
  create_and_push_event (GST_EVENT_EOS);
  fail_unless_equals_int (g_list_length (buffers), 1);
  check_and_clear_events (3, COMPARE);
}

GST_END_TEST;

static void
do_ntp_time (GstClockTime buffer_time, gint segment_start, gint segment_base)
{
  GstSegment segment;
  GstBuffer *buffer;
  GstRTPBuffer rtpbuffer = GST_RTP_BUFFER_INIT;
  guint8 *data;
  guint64 expected_ntp_time;
  guint64 timestamp;

  /* create a segment that controls the behavior
   * by changing segment.start and segment.base we affect the stream time and
   * running time respectively */
  gst_segment_init (&segment, GST_FORMAT_TIME);
  segment.start = segment_start;
  segment.base = segment_base;
  gst_pad_push_event (mysrcpad, gst_event_new_segment (&segment));

  expected_ntp_time = gst_segment_to_stream_time (&segment, GST_FORMAT_TIME,
      buffer_time);
  expected_ntp_time += NTP_OFFSET;
  expected_ntp_time = gst_util_uint64_scale (expected_ntp_time,
      (G_GINT64_CONSTANT (1) << 32), GST_SECOND);

  buffer = create_rtp_buffer (buffer_time, FALSE);
  fail_unless_equals_int (gst_pad_push (mysrcpad, buffer), GST_FLOW_OK);
  fail_unless_equals_int (g_list_length (buffers), 1);

  buffer = g_list_last (buffers)->data;

  /* get the extension header */
  fail_unless (gst_rtp_buffer_map (buffer, GST_MAP_READWRITE, &rtpbuffer));
  fail_unless (gst_rtp_buffer_get_extension_data (&rtpbuffer, NULL,
          (gpointer) & data, NULL));

  /* ...and read the NTP timestamp and verify that it's the expected one */
  timestamp = GST_READ_UINT64_BE (data);
  fail_unless_equals_uint64 (timestamp, expected_ntp_time);

  gst_rtp_buffer_unmap (&rtpbuffer);
  gst_check_drop_buffers ();
}

GST_START_TEST (test_ntp_time)
{
  /* we do not need buffer caching, so do not set the e-bit */
  g_object_set (element, "set-e-bit", FALSE, NULL);
  /* set an ntp offset suitable for testing */
  g_object_set (element, "ntp-offset", NTP_OFFSET, NULL);

  ASSERT_SET_STATE (element, GST_STATE_PLAYING, GST_STATE_CHANGE_SUCCESS);

  /* push initial events */
  gst_check_setup_events (mysrcpad, element, NULL, GST_FORMAT_TIME);

  /* first test with a "clean" segment */
  do_ntp_time (GST_MSECOND, 0, 0);
  do_ntp_time (GST_SECOND + GST_MSECOND, 0, 0);

  /* verify that changing the running time does not affect the ntp time stamps  */
  do_ntp_time (GST_MSECOND, 0, GST_SECOND);
  do_ntp_time (GST_SECOND + GST_MSECOND, 0, GST_SECOND);

  /* changing the segment.start affects the stream time, verify that the element
   * handles it correctly */
  do_ntp_time (GST_MSECOND, GST_MSECOND / 2, 0);
  do_ntp_time (GST_SECOND + GST_MSECOND, GST_MSECOND / 2, 0);

  /* and finally change both of them and verify that all's fine */
  do_ntp_time (GST_MSECOND, GST_MSECOND / 2, GST_SECOND);
  do_ntp_time (GST_SECOND + GST_MSECOND, GST_MSECOND / 2, GST_SECOND);
}

GST_END_TEST;

GST_START_TEST (test_reference_ts)
{
  GstCaps *ref_ts_id = gst_caps_new_empty_simple ("timestamp/x-unix");
  GstSegment segment;
  GstBuffer *buffer;
  guint64 timestamp;
  GstRTPBuffer rtpbuffer = GST_RTP_BUFFER_INIT;
  guint8 *data;
  guint64 expected_ntp_time;

  /* configure element to use references timestamps */
  g_object_set (element, "use-reference-timestamps", TRUE, NULL);

  ASSERT_SET_STATE (element, GST_STATE_PLAYING, GST_STATE_CHANGE_SUCCESS);

  /* push initial events */
  gst_check_setup_events (mysrcpad, element, NULL, GST_FORMAT_TIME);

  /* a suitable segment */
  gst_segment_init (&segment, GST_FORMAT_TIME);
  segment.start = 0;
  segment.base = 0;
  gst_pad_push_event (mysrcpad, gst_event_new_segment (&segment));

  /* create a buffer with PTS 0, which should result in both stream time and
   * running time becoming 0 with the segment pushed above */
  buffer = create_rtp_buffer (0, FALSE);

  /* add a reference timestamp to the buffer and push it to the element */
  timestamp = 42;
  ck_assert_ptr_ne (gst_buffer_add_reference_timestamp_meta (buffer,
          ref_ts_id, timestamp, GST_CLOCK_TIME_NONE), NULL);

  /* the timestamp in the extension header is relative to the NTP epoch, so
   * adjust the expected timestamp for the difference between unix and ntp
   * epochs */
  expected_ntp_time =
      gst_util_uint64_scale (timestamp +
      G_GUINT64_CONSTANT (2208988800) * GST_SECOND,
      (G_GINT64_CONSTANT (1) << 32), GST_SECOND);

  fail_unless_equals_int (gst_pad_push (mysrcpad, buffer), GST_FLOW_OK);
  fail_unless_equals_int (g_list_length (buffers), 1);
  buffer = g_list_last (buffers)->data;

  /* get the extension header */
  fail_unless (gst_rtp_buffer_map (buffer, GST_MAP_READWRITE, &rtpbuffer));
  fail_unless (gst_rtp_buffer_get_extension_data (&rtpbuffer, NULL,
          (gpointer) & data, NULL));

  /* read the NTP timestamp and verify that it's the expected one, i.e. derived
   * from the reference timestamp */
  timestamp = GST_READ_UINT64_BE (data);
  fail_unless_equals_uint64 (timestamp, expected_ntp_time);

  gst_rtp_buffer_unmap (&rtpbuffer);
  gst_check_drop_buffers ();
  gst_caps_unref (ref_ts_id);
}

GST_END_TEST;

GST_START_TEST (test_reference_ts_not_present)
{
  GstBuffer *buffer;

  /* configure element to use references timestamps */
  g_object_set (element, "use-reference-timestamps", TRUE, NULL);

  ASSERT_SET_STATE (element, GST_STATE_PLAYING, GST_STATE_CHANGE_SUCCESS);

  /* push initial events */
  gst_check_setup_events (mysrcpad, element, NULL, GST_FORMAT_TIME);

  /* create a buffer without reference timestamp, push it and verify that
   * GST_FLOW_ERROR is returned */
  buffer = create_rtp_buffer (0, FALSE);
  fail_unless_equals_int (gst_pad_push (mysrcpad, buffer), GST_FLOW_ERROR);

  gst_check_drop_buffers ();
}

GST_END_TEST;

static Suite *
onviftimestamp_suite (void)
{
  Suite *s = suite_create ("onviftimestamp");
  TCase *tc_general, *tc_events;

  tc_general = tcase_create ("general");
  suite_add_tcase (s, tc_general);
  tcase_add_checked_fixture (tc_general, setup, cleanup);

  tcase_add_test (tc_general, test_apply_clean_point);
  tcase_add_test (tc_general, test_apply_no_e_bit);
  tcase_add_test (tc_general, test_apply_e_bit);
  tcase_add_test (tc_general, test_flushing);
  tcase_add_test (tc_general, test_reusable_element_no_e_bit);
  tcase_add_test (tc_general, test_reusable_element_e_bit);
  tcase_add_test (tc_general, test_ntp_offset_event);
  tcase_add_test (tc_general, test_ntp_time);

  tcase_add_test (tc_general, test_reference_ts);
  tcase_add_test (tc_general, test_reference_ts_not_present);

  tc_events = tcase_create ("events");
  suite_add_tcase (s, tc_events);
  tcase_add_checked_fixture (tc_events, setup_with_event, cleanup_with_event);

  tcase_add_test (tc_events, test_serialized_events);
  tcase_add_test (tc_events, test_non_serialized_events);

  return s;
}

GST_CHECK_MAIN (onviftimestamp);
