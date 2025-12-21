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

#include <unistd.h>
#include <sys/ioctl.h>

#include <gst/check/gstcheck.h>

static GstPad *mysrcpad;

static GstStaticPadTemplate srctemplate = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("application/x-gst-check")
    );

static GstElement *
setup_multifdsink (void)
{
  GstElement *multifdsink;

  GST_DEBUG ("setup_multifdsink");
  multifdsink = gst_check_setup_element ("multifdsink");
  mysrcpad = gst_check_setup_src_pad (multifdsink, &srctemplate);
  gst_pad_set_active (mysrcpad, TRUE);

  return multifdsink;
}

static void
cleanup_multifdsink (GstElement * multifdsink)
{
  GST_DEBUG ("cleanup_multifdsink");

  gst_check_teardown_src_pad (multifdsink);
  gst_check_teardown_element (multifdsink);
}

static void
wait_bytes_served (GstElement * sink, guint64 bytes)
{
  guint64 bytes_served = 0;

  while (bytes_served != bytes) {
    g_object_get (sink, "bytes-served", &bytes_served, NULL);
  }
}

/* FIXME: possibly racy, since if it would write, we may not get it
 * immediately ? */
#define fail_if_can_read(msg,fd) \
G_STMT_START { \
  long avail; \
\
  fail_if (ioctl (fd, FIONREAD, &avail) < 0, "%s: could not ioctl", msg); \
  fail_if (avail > 0, "%s: has bytes available to read"); \
} G_STMT_END;


GST_START_TEST (test_no_clients)
{
  GstElement *sink;
  GstBuffer *buffer;
  GstCaps *caps;

  sink = setup_multifdsink ();

  ASSERT_SET_STATE (sink, GST_STATE_PLAYING, GST_STATE_CHANGE_ASYNC);

  caps = gst_caps_from_string ("application/x-gst-check");
  buffer = gst_buffer_new_and_alloc (4);
  gst_check_setup_events (mysrcpad, sink, caps, GST_FORMAT_BYTES);
  gst_caps_unref (caps);
  fail_unless (gst_pad_push (mysrcpad, buffer) == GST_FLOW_OK);

  GST_DEBUG ("cleaning up multifdsink");
  ASSERT_SET_STATE (sink, GST_STATE_NULL, GST_STATE_CHANGE_SUCCESS);
  cleanup_multifdsink (sink);
}

GST_END_TEST;

GST_START_TEST (test_add_client)
{
  GstElement *sink;
  GstBuffer *buffer;
  GstCaps *caps;
  int pfd[2];
  gchar data[4];

  sink = setup_multifdsink ();

  fail_if (pipe (pfd) == -1);

  ASSERT_SET_STATE (sink, GST_STATE_PLAYING, GST_STATE_CHANGE_ASYNC);

  /* add the client */
  g_signal_emit_by_name (sink, "add", pfd[1]);

  caps = gst_caps_from_string ("application/x-gst-check");
  ASSERT_CAPS_REFCOUNT (caps, "caps", 1);
  GST_DEBUG ("Created test caps %p %" GST_PTR_FORMAT, caps, caps);
  buffer = gst_buffer_new_and_alloc (4);
  gst_check_setup_events (mysrcpad, sink, caps, GST_FORMAT_BYTES);
  ASSERT_CAPS_REFCOUNT (caps, "caps", 3);
  gst_buffer_fill (buffer, 0, "dead", 4);
  fail_unless (gst_pad_push (mysrcpad, buffer) == GST_FLOW_OK);

  GST_DEBUG ("reading");
  fail_if (read (pfd[0], data, 4) < 4);
  fail_unless (strncmp (data, "dead", 4) == 0);
  wait_bytes_served (sink, 4);

  GST_DEBUG ("cleaning up multifdsink");
  ASSERT_SET_STATE (sink, GST_STATE_NULL, GST_STATE_CHANGE_SUCCESS);
  cleanup_multifdsink (sink);

  ASSERT_CAPS_REFCOUNT (caps, "caps", 1);
  gst_caps_unref (caps);
}

GST_END_TEST;

GST_START_TEST (test_add_client_in_null_state)
{
  GstElement *sink;

  sink = setup_multifdsink ();

  ASSERT_WARNING (g_signal_emit_by_name (sink, "add", 99));

  cleanup_multifdsink (sink);
}

GST_END_TEST;

#define fail_unless_read(msg,fd,size,ref) \
G_STMT_START { \
  char data[size + 1]; \
  int nbytes; \
\
  GST_LOG ("%s: reading %d bytes", msg, size); \
  nbytes = read (fd, data, size); \
  data[size] = 0; \
  GST_LOG ("%s: read %d bytes", msg, nbytes); \
  fail_if (nbytes < size); \
  fail_unless (memcmp (data, ref, size) == 0, \
      "data read '%s' differs from '%s'", data, ref); \
} G_STMT_END;

#define fail_unless_num_handles(sink,num) \
G_STMT_START { \
  gint handles; \
  g_object_get (sink, "num-handles", &handles, NULL); \
  fail_unless (handles == num, \
      "sink has %d handles instead of expected %d", handles, num); \
} G_STMT_END;

/* from the given two data buffers, create two streamheader buffers and
 * some caps that match it, and store them in the given pointers
 * returns  one ref to each of the buffers and the caps */
static void
gst_multifdsink_create_streamheader (const gchar * data1,
    const gchar * data2, GstBuffer ** hbuf1, GstBuffer ** hbuf2,
    GstCaps ** caps)
{
  GstBuffer *buf;
  GValue array = { 0 };
  GValue value = { 0 };
  GstStructure *structure;
  guint size1 = strlen (data1);
  guint size2 = strlen (data2);

  fail_if (hbuf1 == NULL);
  fail_if (hbuf2 == NULL);
  fail_if (caps == NULL);

  /* create caps with streamheader, set the caps, and push the HEADER
   * buffers */
  *hbuf1 = gst_buffer_new_and_alloc (size1);
  GST_BUFFER_FLAG_SET (*hbuf1, GST_BUFFER_FLAG_HEADER);
  gst_buffer_fill (*hbuf1, 0, data1, size1);
  *hbuf2 = gst_buffer_new_and_alloc (size2);
  GST_BUFFER_FLAG_SET (*hbuf2, GST_BUFFER_FLAG_HEADER);
  gst_buffer_fill (*hbuf2, 0, data2, size2);

  g_value_init (&array, GST_TYPE_ARRAY);

  g_value_init (&value, GST_TYPE_BUFFER);
  /* we take a copy, set it on the array (which refs it), then unref our copy */
  buf = gst_buffer_copy (*hbuf1);
  gst_value_set_buffer (&value, buf);
  ASSERT_BUFFER_REFCOUNT (buf, "copied buffer", 2);
  gst_buffer_unref (buf);
  gst_value_array_append_value (&array, &value);
  g_value_unset (&value);

  g_value_init (&value, GST_TYPE_BUFFER);
  buf = gst_buffer_copy (*hbuf2);
  gst_value_set_buffer (&value, buf);
  ASSERT_BUFFER_REFCOUNT (buf, "copied buffer", 2);
  gst_buffer_unref (buf);
  gst_value_array_append_value (&array, &value);
  g_value_unset (&value);

  *caps = gst_caps_from_string ("application/x-gst-check");
  structure = gst_caps_get_structure (*caps, 0);

  gst_structure_set_value (structure, "streamheader", &array);
  g_value_unset (&array);
  ASSERT_CAPS_REFCOUNT (*caps, "streamheader caps", 1);

  /* we want to keep them around for the tests */
  gst_buffer_ref (*hbuf1);
  gst_buffer_ref (*hbuf2);

  GST_DEBUG ("created streamheader caps %p %" GST_PTR_FORMAT, *caps, *caps);
}


/* this test:
 * - adds a first client
 * - sets streamheader caps on the pad
 * - pushes the HEADER buffers
 * - pushes a buffer
 * - verifies that the client received all the data correctly, and did not
 *   get multiple copies of the streamheader
 * - adds a second client
 * - verifies that this second client receives the streamheader caps too, plus
 * - the new buffer
 */
GST_START_TEST (test_streamheader)
{
  GstElement *sink;
  GstBuffer *hbuf1, *hbuf2, *buf;
  GstCaps *caps;
  int pfd1[2], pfd2[2];

  sink = setup_multifdsink ();

  fail_if (pipe (pfd1) == -1);
  fail_if (pipe (pfd2) == -1);

  ASSERT_SET_STATE (sink, GST_STATE_PLAYING, GST_STATE_CHANGE_ASYNC);

  /* add the first client */
  fail_unless_num_handles (sink, 0);
  g_signal_emit_by_name (sink, "add", pfd1[1]);
  fail_unless_num_handles (sink, 1);

  /* create caps with streamheader, set the caps, and push the HEADER
   * buffers */
  gst_multifdsink_create_streamheader ("babe", "deadbeef", &hbuf1, &hbuf2,
      &caps);
  ASSERT_CAPS_REFCOUNT (caps, "caps", 1);
  gst_check_setup_events (mysrcpad, sink, caps, GST_FORMAT_BYTES);
  /* one is ours, two from set_caps */
  ASSERT_CAPS_REFCOUNT (caps, "caps", 3);

  fail_unless (gst_pad_push (mysrcpad, hbuf1) == GST_FLOW_OK);
  fail_unless (gst_pad_push (mysrcpad, hbuf2) == GST_FLOW_OK);

  //FIXME:
  //fail_if_can_read ("first client", pfd1[0]);

  /* push a non-HEADER buffer, this should trigger the client receiving the
   * first three buffers */
  buf = gst_buffer_new_and_alloc (4);
  gst_buffer_fill (buf, 0, "f00d", 4);
  gst_pad_push (mysrcpad, buf);

  fail_unless_read ("first client", pfd1[0], 4, "babe");
  fail_unless_read ("first client", pfd1[0], 8, "deadbeef");
  fail_unless_read ("first client", pfd1[0], 4, "f00d");
  wait_bytes_served (sink, 16);

  /* now add the second client */
  g_signal_emit_by_name (sink, "add", pfd2[1]);
  fail_unless_num_handles (sink, 2);
  //FIXME:
  //fail_if_can_read ("second client", pfd2[0]);

  /* now push another buffer, which will trigger streamheader for second
   * client */
  buf = gst_buffer_new_and_alloc (4);
  gst_buffer_fill (buf, 0, "deaf", 4);
  gst_pad_push (mysrcpad, buf);

  fail_unless_read ("first client", pfd1[0], 4, "deaf");

  fail_unless_read ("second client", pfd2[0], 4, "babe");
  fail_unless_read ("second client", pfd2[0], 8, "deadbeef");
  /* we missed the f00d buffer */
  fail_unless_read ("second client", pfd2[0], 4, "deaf");
  wait_bytes_served (sink, 36);

  GST_DEBUG ("cleaning up multifdsink");

  fail_unless_num_handles (sink, 2);
  g_signal_emit_by_name (sink, "remove", pfd1[1]);
  fail_unless_num_handles (sink, 1);
  g_signal_emit_by_name (sink, "remove", pfd2[1]);
  fail_unless_num_handles (sink, 0);

  ASSERT_SET_STATE (sink, GST_STATE_NULL, GST_STATE_CHANGE_SUCCESS);
  cleanup_multifdsink (sink);

  ASSERT_BUFFER_REFCOUNT (hbuf1, "hbuf1", 1);
  ASSERT_BUFFER_REFCOUNT (hbuf2, "hbuf2", 1);
  gst_buffer_unref (hbuf1);
  gst_buffer_unref (hbuf2);

  ASSERT_CAPS_REFCOUNT (caps, "caps", 1);
  gst_caps_unref (caps);
}

GST_END_TEST;

/* this tests changing of streamheaders
 * - set streamheader caps on the pad
 * - pushes the HEADER buffers
 * - pushes a buffer
 * - add a first client
 * - verifies that this first client receives the first streamheader caps,
 *   plus a new buffer
 * - change streamheader caps
 * - verify that the first client receives the new streamheader buffers as well
 */
GST_START_TEST (test_change_streamheader)
{
  GstElement *sink;
  GstBuffer *hbuf1, *hbuf2, *buf;
  GstCaps *caps;
  int pfd1[2], pfd2[2];

  sink = setup_multifdsink ();

  fail_if (pipe (pfd1) == -1);
  fail_if (pipe (pfd2) == -1);

  ASSERT_SET_STATE (sink, GST_STATE_PLAYING, GST_STATE_CHANGE_ASYNC);

  /* create caps with streamheader, set the caps, and push the HEADER
   * buffers */
  gst_multifdsink_create_streamheader ("first", "header", &hbuf1, &hbuf2,
      &caps);
  ASSERT_CAPS_REFCOUNT (caps, "caps", 1);
  gst_check_setup_events (mysrcpad, sink, caps, GST_FORMAT_BYTES);
  /* one is ours, two from set_caps */
  ASSERT_CAPS_REFCOUNT (caps, "caps", 3);

  /* one to hold for the test and one to give away */
  ASSERT_BUFFER_REFCOUNT (hbuf1, "hbuf1", 2);
  ASSERT_BUFFER_REFCOUNT (hbuf2, "hbuf2", 2);

  fail_unless (gst_pad_push (mysrcpad, hbuf1) == GST_FLOW_OK);
  fail_unless (gst_pad_push (mysrcpad, hbuf2) == GST_FLOW_OK);

  /* add the first client */
  g_signal_emit_by_name (sink, "add", pfd1[1]);

  /* verify this hasn't triggered a write yet */
  /* FIXME: possibly racy, since if it would write, we may not get it
   * immediately ? */
  //fail_if_can_read ("first client, no buffer", pfd1[0]);

  /* now push a buffer and read */
  buf = gst_buffer_new_and_alloc (4);
  gst_buffer_fill (buf, 0, "f00d", 4);
  gst_pad_push (mysrcpad, buf);

  fail_unless_read ("change: first client", pfd1[0], 5, "first");
  fail_unless_read ("change: first client", pfd1[0], 6, "header");
  fail_unless_read ("change: first client", pfd1[0], 4, "f00d");
  //wait_bytes_served (sink, 16);

  /* now add the second client */
  g_signal_emit_by_name (sink, "add", pfd2[1]);
  //fail_if_can_read ("second client, no buffer", pfd2[0]);

  /* change the streamheader */

  /* only we have a reference to the streamheaders now */
  ASSERT_BUFFER_REFCOUNT (hbuf1, "hbuf1", 1);
  ASSERT_BUFFER_REFCOUNT (hbuf2, "hbuf2", 1);
  gst_buffer_unref (hbuf1);
  gst_buffer_unref (hbuf2);

  /* drop our ref to the previous caps */
  gst_caps_unref (caps);

  gst_multifdsink_create_streamheader ("second", "header", &hbuf1, &hbuf2,
      &caps);
  gst_check_setup_events (mysrcpad, sink, caps, GST_FORMAT_BYTES);
  /* one to hold for the test and one to give away */
  ASSERT_BUFFER_REFCOUNT (hbuf1, "hbuf1", 2);
  ASSERT_BUFFER_REFCOUNT (hbuf2, "hbuf2", 2);

  fail_unless (gst_pad_push (mysrcpad, hbuf1) == GST_FLOW_OK);
  fail_unless (gst_pad_push (mysrcpad, hbuf2) == GST_FLOW_OK);

  /* verify neither client has new data available to read */
  //fail_if_can_read ("first client, changed streamheader", pfd1[0]);
  //fail_if_can_read ("second client, changed streamheader", pfd2[0]);

  /* now push another buffer, which will trigger streamheader for second
   * client, but should also send new streamheaders to first client */
  buf = gst_buffer_new_and_alloc (8);
  gst_buffer_fill (buf, 0, "deadbabe", 8);
  gst_pad_push (mysrcpad, buf);

  fail_unless_read ("first client", pfd1[0], 6, "second");
  fail_unless_read ("first client", pfd1[0], 6, "header");
  fail_unless_read ("first client", pfd1[0], 8, "deadbabe");

  /* new streamheader data */
  fail_unless_read ("second client", pfd2[0], 6, "second");
  fail_unless_read ("second client", pfd2[0], 6, "header");
  /* we missed the f00d buffer */
  fail_unless_read ("second client", pfd2[0], 8, "deadbabe");
  //wait_bytes_served (sink, 36);

  GST_DEBUG ("cleaning up multifdsink");
  g_signal_emit_by_name (sink, "remove", pfd1[1]);
  g_signal_emit_by_name (sink, "remove", pfd2[1]);
  ASSERT_SET_STATE (sink, GST_STATE_NULL, GST_STATE_CHANGE_SUCCESS);

  /* setting to NULL should have cleared the streamheader */
  ASSERT_BUFFER_REFCOUNT (hbuf1, "hbuf1", 1);
  ASSERT_BUFFER_REFCOUNT (hbuf2, "hbuf2", 1);
  gst_buffer_unref (hbuf1);
  gst_buffer_unref (hbuf2);
  cleanup_multifdsink (sink);

  ASSERT_CAPS_REFCOUNT (caps, "caps", 1);
  gst_caps_unref (caps);
}

GST_END_TEST;

static GstBuffer *
gst_new_buffer (int i)
{
  GstMapInfo info;
  gchar *data;

  GstBuffer *buffer = gst_buffer_new_and_alloc (16);

  /* copy some id */
  g_assert (gst_buffer_map (buffer, &info, GST_MAP_WRITE));
  data = (gchar *) info.data;
  g_snprintf (data, 16, "deadbee%08x", i);
  gst_buffer_unmap (buffer, &info);

  return buffer;
}


/* keep 100 bytes and burst 80 bytes to clients */
GST_START_TEST (test_burst_client_bytes)
{
  GstElement *sink;
  GstCaps *caps;
  int pfd1[2];
  int pfd2[2];
  int pfd3[2];
  gint i;
  guint buffers_queued;

  sink = setup_multifdsink ();
  /* make sure we keep at least 100 bytes at all times */
  g_object_set (sink, "bytes-min", 100, NULL);
  g_object_set (sink, "sync-method", 3, NULL);  /* 3 = burst */
  g_object_set (sink, "burst-format", GST_FORMAT_BYTES, NULL);
  g_object_set (sink, "burst-value", (guint64) 80, NULL);

  fail_if (pipe (pfd1) == -1);
  fail_if (pipe (pfd2) == -1);
  fail_if (pipe (pfd3) == -1);

  ASSERT_SET_STATE (sink, GST_STATE_PLAYING, GST_STATE_CHANGE_ASYNC);

  caps = gst_caps_from_string ("application/x-gst-check");
  gst_check_setup_events (mysrcpad, sink, caps, GST_FORMAT_BYTES);
  GST_DEBUG ("Created test caps %p %" GST_PTR_FORMAT, caps, caps);

  /* push buffers in, 9 * 16 bytes = 144 bytes */
  for (i = 0; i < 9; i++) {
    GstBuffer *buffer = gst_new_buffer (i);

    fail_unless (gst_pad_push (mysrcpad, buffer) == GST_FLOW_OK);
  }

  /* check that at least 7 buffers (112 bytes) are in the queue */
  g_object_get (sink, "buffers-queued", &buffers_queued, NULL);
  fail_if (buffers_queued != 7);

  /* now add the clients */
  fail_unless_num_handles (sink, 0);
  g_signal_emit_by_name (sink, "add", pfd1[1]);
  fail_unless_num_handles (sink, 1);
  g_signal_emit_by_name (sink, "add_full", pfd2[1], 3,
      GST_FORMAT_BYTES, (guint64) 50, GST_FORMAT_BYTES, (guint64) 200);
  g_signal_emit_by_name (sink, "add_full", pfd3[1], 3,
      GST_FORMAT_BYTES, (guint64) 50, GST_FORMAT_BYTES, (guint64) 50);
  fail_unless_num_handles (sink, 3);

  /* push last buffer to make client fds ready for reading */
  for (i = 9; i < 10; i++) {
    GstBuffer *buffer = gst_new_buffer (i);

    fail_unless (gst_pad_push (mysrcpad, buffer) == GST_FLOW_OK);
  }

  /* now we should only read the last 5 buffers (5 * 16 = 80 bytes) */
  GST_DEBUG ("Reading from client 1");
  fail_unless_read ("client 1", pfd1[0], 16, "deadbee00000005");
  fail_unless_read ("client 1", pfd1[0], 16, "deadbee00000006");
  fail_unless_read ("client 1", pfd1[0], 16, "deadbee00000007");
  fail_unless_read ("client 1", pfd1[0], 16, "deadbee00000008");
  fail_unless_read ("client 1", pfd1[0], 16, "deadbee00000009");

  /* second client only bursts 50 bytes = 4 buffers (we get 4 buffers since
   * the max allows it) */
  GST_DEBUG ("Reading from client 2");
  fail_unless_read ("client 2", pfd2[0], 16, "deadbee00000006");
  fail_unless_read ("client 2", pfd2[0], 16, "deadbee00000007");
  fail_unless_read ("client 2", pfd2[0], 16, "deadbee00000008");
  fail_unless_read ("client 2", pfd2[0], 16, "deadbee00000009");

  /* third client only bursts 50 bytes = 4 buffers, we can't send
   * more than 50 bytes so we only get 3 buffers (48 bytes). */
  GST_DEBUG ("Reading from client 3");
  fail_unless_read ("client 3", pfd3[0], 16, "deadbee00000007");
  fail_unless_read ("client 3", pfd3[0], 16, "deadbee00000008");
  fail_unless_read ("client 3", pfd3[0], 16, "deadbee00000009");

  GST_DEBUG ("cleaning up multifdsink");
  ASSERT_SET_STATE (sink, GST_STATE_NULL, GST_STATE_CHANGE_SUCCESS);
  cleanup_multifdsink (sink);

  ASSERT_CAPS_REFCOUNT (caps, "caps", 1);
  gst_caps_unref (caps);
}

GST_END_TEST;

/* keep 100 bytes and burst 80 bytes to clients */
GST_START_TEST (test_burst_client_bytes_keyframe)
{
  GstElement *sink;
  GstCaps *caps;
  int pfd1[2];
  int pfd2[2];
  int pfd3[2];
  gint i;
  guint buffers_queued;

  sink = setup_multifdsink ();
  /* make sure we keep at least 100 bytes at all times */
  g_object_set (sink, "bytes-min", 100, NULL);
  g_object_set (sink, "sync-method", 4, NULL);  /* 4 = burst_keyframe */
  g_object_set (sink, "burst-format", GST_FORMAT_BYTES, NULL);
  g_object_set (sink, "burst-value", (guint64) 80, NULL);

  fail_if (pipe (pfd1) == -1);
  fail_if (pipe (pfd2) == -1);
  fail_if (pipe (pfd3) == -1);

  ASSERT_SET_STATE (sink, GST_STATE_PLAYING, GST_STATE_CHANGE_ASYNC);

  caps = gst_caps_from_string ("application/x-gst-check");
  gst_check_setup_events (mysrcpad, sink, caps, GST_FORMAT_BYTES);
  GST_DEBUG ("Created test caps %p %" GST_PTR_FORMAT, caps, caps);

  /* push buffers in, 9 * 16 bytes = 144 bytes */
  for (i = 0; i < 9; i++) {
    GstBuffer *buffer = gst_new_buffer (i);

    /* mark most buffers as delta */
    if (i != 0 && i != 4 && i != 8)
      GST_BUFFER_FLAG_SET (buffer, GST_BUFFER_FLAG_DELTA_UNIT);

    fail_unless (gst_pad_push (mysrcpad, buffer) == GST_FLOW_OK);
  }

  /* check that at least 7 buffers (112 bytes) are in the queue */
  g_object_get (sink, "buffers-queued", &buffers_queued, NULL);
  fail_if (buffers_queued != 7);

  /* now add the clients */
  g_signal_emit_by_name (sink, "add", pfd1[1]);
  g_signal_emit_by_name (sink, "add_full", pfd2[1],
      4, GST_FORMAT_BYTES, (guint64) 50, GST_FORMAT_BYTES, (guint64) 90);
  g_signal_emit_by_name (sink, "add_full", pfd3[1],
      4, GST_FORMAT_BYTES, (guint64) 50, GST_FORMAT_BYTES, (guint64) 50);

  /* push last buffer to make client fds ready for reading */
  for (i = 9; i < 10; i++) {
    GstBuffer *buffer = gst_new_buffer (i);

    GST_BUFFER_FLAG_SET (buffer, GST_BUFFER_FLAG_DELTA_UNIT);

    fail_unless (gst_pad_push (mysrcpad, buffer) == GST_FLOW_OK);
  }

  /* now we should only read the last 6 buffers (min 5 * 16 = 80 bytes),
   * keyframe at buffer 4 */
  GST_DEBUG ("Reading from client 1");
  fail_unless_read ("client 1", pfd1[0], 16, "deadbee00000004");
  fail_unless_read ("client 1", pfd1[0], 16, "deadbee00000005");
  fail_unless_read ("client 1", pfd1[0], 16, "deadbee00000006");
  fail_unless_read ("client 1", pfd1[0], 16, "deadbee00000007");
  fail_unless_read ("client 1", pfd1[0], 16, "deadbee00000008");
  fail_unless_read ("client 1", pfd1[0], 16, "deadbee00000009");

  /* second client only bursts 50 bytes = 4 buffers, there is
   * no keyframe above min and below max, so get one below min */
  GST_DEBUG ("Reading from client 2");
  fail_unless_read ("client 2", pfd2[0], 16, "deadbee00000008");
  fail_unless_read ("client 2", pfd2[0], 16, "deadbee00000009");

  /* third client only bursts 50 bytes = 4 buffers, we can't send
   * more than 50 bytes so we only get 2 buffers (32 bytes). */
  GST_DEBUG ("Reading from client 3");
  fail_unless_read ("client 3", pfd3[0], 16, "deadbee00000008");
  fail_unless_read ("client 3", pfd3[0], 16, "deadbee00000009");

  GST_DEBUG ("cleaning up multifdsink");
  ASSERT_SET_STATE (sink, GST_STATE_NULL, GST_STATE_CHANGE_SUCCESS);
  cleanup_multifdsink (sink);

  ASSERT_CAPS_REFCOUNT (caps, "caps", 1);
  gst_caps_unref (caps);
}

GST_END_TEST;

/* keep 100 bytes and burst 80 bytes to clients */
GST_START_TEST (test_burst_client_bytes_with_keyframe)
{
  GstElement *sink;
  GstCaps *caps;
  int pfd1[2];
  int pfd2[2];
  int pfd3[2];
  gint i;
  guint buffers_queued;

  sink = setup_multifdsink ();
  /* make sure we keep at least 100 bytes at all times */
  g_object_set (sink, "bytes-min", 100, NULL);
  g_object_set (sink, "sync-method", 5, NULL);  /* 5 = burst_with_keyframe */
  g_object_set (sink, "burst-format", GST_FORMAT_BYTES, NULL);
  g_object_set (sink, "burst-value", (guint64) 80, NULL);

  fail_if (pipe (pfd1) == -1);
  fail_if (pipe (pfd2) == -1);
  fail_if (pipe (pfd3) == -1);

  ASSERT_SET_STATE (sink, GST_STATE_PLAYING, GST_STATE_CHANGE_ASYNC);

  caps = gst_caps_from_string ("application/x-gst-check");
  gst_check_setup_events (mysrcpad, sink, caps, GST_FORMAT_BYTES);
  GST_DEBUG ("Created test caps %p %" GST_PTR_FORMAT, caps, caps);

  /* push buffers in, 9 * 16 bytes = 144 bytes */
  for (i = 0; i < 9; i++) {
    GstBuffer *buffer = gst_new_buffer (i);

    /* mark most buffers as delta */
    if (i != 0 && i != 4 && i != 8)
      GST_BUFFER_FLAG_SET (buffer, GST_BUFFER_FLAG_DELTA_UNIT);

    fail_unless (gst_pad_push (mysrcpad, buffer) == GST_FLOW_OK);
  }

  /* check that at least 7 buffers (112 bytes) are in the queue */
  g_object_get (sink, "buffers-queued", &buffers_queued, NULL);
  fail_if (buffers_queued != 7);

  /* now add the clients */
  g_signal_emit_by_name (sink, "add", pfd1[1]);
  g_signal_emit_by_name (sink, "add_full", pfd2[1],
      5, GST_FORMAT_BYTES, (guint64) 50, GST_FORMAT_BYTES, (guint64) 90);
  g_signal_emit_by_name (sink, "add_full", pfd3[1],
      5, GST_FORMAT_BYTES, (guint64) 50, GST_FORMAT_BYTES, (guint64) 50);

  /* push last buffer to make client fds ready for reading */
  for (i = 9; i < 10; i++) {
    GstBuffer *buffer = gst_new_buffer (i);

    GST_BUFFER_FLAG_SET (buffer, GST_BUFFER_FLAG_DELTA_UNIT);

    fail_unless (gst_pad_push (mysrcpad, buffer) == GST_FLOW_OK);
  }

  /* now we should only read the last 6 buffers (min 5 * 16 = 80 bytes),
   * keyframe at buffer 4 */
  GST_DEBUG ("Reading from client 1");
  fail_unless_read ("client 1", pfd1[0], 16, "deadbee00000004");
  fail_unless_read ("client 1", pfd1[0], 16, "deadbee00000005");
  fail_unless_read ("client 1", pfd1[0], 16, "deadbee00000006");
  fail_unless_read ("client 1", pfd1[0], 16, "deadbee00000007");
  fail_unless_read ("client 1", pfd1[0], 16, "deadbee00000008");
  fail_unless_read ("client 1", pfd1[0], 16, "deadbee00000009");

  /* second client only bursts 50 bytes = 4 buffers, there is
   * no keyframe above min and below max, so send min */
  GST_DEBUG ("Reading from client 2");
  fail_unless_read ("client 2", pfd2[0], 16, "deadbee00000006");
  fail_unless_read ("client 2", pfd2[0], 16, "deadbee00000007");
  fail_unless_read ("client 2", pfd2[0], 16, "deadbee00000008");
  fail_unless_read ("client 2", pfd2[0], 16, "deadbee00000009");

  /* third client only bursts 50 bytes = 4 buffers, we can't send
   * more than 50 bytes so we only get 3 buffers (48 bytes). */
  GST_DEBUG ("Reading from client 3");
  fail_unless_read ("client 3", pfd3[0], 16, "deadbee00000007");
  fail_unless_read ("client 3", pfd3[0], 16, "deadbee00000008");
  fail_unless_read ("client 3", pfd3[0], 16, "deadbee00000009");

  GST_DEBUG ("cleaning up multifdsink");
  ASSERT_SET_STATE (sink, GST_STATE_NULL, GST_STATE_CHANGE_SUCCESS);
  cleanup_multifdsink (sink);

  ASSERT_CAPS_REFCOUNT (caps, "caps", 1);
  gst_caps_unref (caps);
}

GST_END_TEST;

/* Check that we can get data when multifdsink is configured in next-keyframe
 * mode */
GST_START_TEST (test_client_next_keyframe)
{
  GstElement *sink;
  GstCaps *caps;
  int pfd1[2];
  gint i;

  sink = setup_multifdsink ();
  g_object_set (sink, "sync-method", 1, NULL);  /* 1 = next-keyframe */

  fail_if (pipe (pfd1) == -1);

  ASSERT_SET_STATE (sink, GST_STATE_PLAYING, GST_STATE_CHANGE_ASYNC);

  caps = gst_caps_from_string ("application/x-gst-check");
  gst_check_setup_events (mysrcpad, sink, caps, GST_FORMAT_BYTES);
  GST_DEBUG ("Created test caps %p %" GST_PTR_FORMAT, caps, caps);

  /* now add our client */
  g_signal_emit_by_name (sink, "add", pfd1[1]);

  /* push buffers in: keyframe, then non-keyframe */
  for (i = 0; i < 2; i++) {
    GstBuffer *buffer = gst_new_buffer (i);
    if (i > 0)
      GST_BUFFER_FLAG_SET (buffer, GST_BUFFER_FLAG_DELTA_UNIT);

    fail_unless (gst_pad_push (mysrcpad, buffer) == GST_FLOW_OK);
  }

  /* now we should be able to read some data */
  GST_DEBUG ("Reading from client 1");
  fail_unless_read ("client 1", pfd1[0], 16, "deadbee00000000");
  fail_unless_read ("client 1", pfd1[0], 16, "deadbee00000001");

  GST_DEBUG ("cleaning up multifdsink");
  ASSERT_SET_STATE (sink, GST_STATE_NULL, GST_STATE_CHANGE_SUCCESS);
  cleanup_multifdsink (sink);

  ASSERT_CAPS_REFCOUNT (caps, "caps", 1);
  gst_caps_unref (caps);
}

GST_END_TEST;

/* number of 16-byte chunks.
 * should be bigger than any OS pipe buffer, hopefully */
#define BIG_BUFFER_MULT (16 * 1024)

static GstBuffer *
gst_new_buffer_big (int i)
{
  GstMapInfo info;
  gchar *data;
  gint j;

  GstBuffer *buffer = gst_buffer_new_and_alloc (16 * BIG_BUFFER_MULT);

  /* copy some id */
  g_assert (gst_buffer_map (buffer, &info, GST_MAP_WRITE));
  data = (gchar *) info.data;
  for (j = 0; j < BIG_BUFFER_MULT; j++) {
    g_snprintf (data + 16 * j, 16, "deadbee%08x", i);
  }
  gst_buffer_unmap (buffer, &info);

  return buffer;
}

#define fail_unless_read_big(msg,fd,i) \
G_STMT_START { \
  char ref[16]; \
  int j; \
  g_snprintf (ref, 16, "deadbee%08x", i); \
  for (j = 0; j < BIG_BUFFER_MULT; j++) { \
    fail_unless_read (msg, fd, 16, ref); \
  } \
} G_STMT_END;

#define fail_unless_eof(msg,fd) \
G_STMT_START { \
  char data; \
  int nbytes; \
\
  GST_LOG ("%s: checking for EOF", msg); \
  nbytes = read (fd, &data, 1); \
  GST_LOG ("%s: read %d bytes", msg, nbytes); \
  fail_if (nbytes != 0, "%s: not at EOF (%d)", msg, nbytes); \
} G_STMT_END;

static gint
get_buffers_queued (GstElement * sink)
{
  gint buffers;
  g_object_get (sink, "buffers-queued", &buffers, NULL);
  return buffers;
}

static gint
get_num_handles (GstElement * sink)
{
  gint handles;
  g_object_get (sink, "num-handles", &handles, NULL);
  return handles;
}

/* test kicking out clients */
GST_START_TEST (test_client_kick)
{
  GstElement *sink;
  GstCaps *caps;
  int pfd1[2];
  int pfd2[2];
  int pfd3[2];
  gint i, initial_buffers = 3, num_buffers = 0;

  sink = setup_multifdsink ();
  g_object_set (sink, "units-max", (gint64) initial_buffers, NULL);

  fail_if (pipe (pfd1) == -1);
  fail_if (pipe (pfd2) == -1);
  fail_if (pipe (pfd3) == -1);

  ASSERT_SET_STATE (sink, GST_STATE_PLAYING, GST_STATE_CHANGE_ASYNC);

  caps = gst_caps_from_string ("application/x-gst-check");
  gst_check_setup_events (mysrcpad, sink, caps, GST_FORMAT_BYTES);
  GST_DEBUG ("Created test caps %p %" GST_PTR_FORMAT, caps, caps);

  /* add the clients */
  g_signal_emit_by_name (sink, "add", pfd1[1]);
  g_signal_emit_by_name (sink, "add", pfd2[1]);
  g_signal_emit_by_name (sink, "add", pfd3[1]);

  /* push initial buffers in */
  for (i = 0; i < initial_buffers; i++) {
    GstBuffer *buffer = gst_new_buffer_big (i);
    fail_unless (gst_pad_push (mysrcpad, buffer) == GST_FLOW_OK);
    num_buffers++;
    GST_DEBUG ("Pushed buffer #%d; %d buffers queued", i,
        get_buffers_queued (sink));
  }

  /* check initial state */
  fail_unless_num_handles (sink, 3);

  for (i = 0; i < initial_buffers; i++) {
    fail_unless_read_big ("client 1", pfd1[0], i);
    fail_unless_read_big ("client 3", pfd3[0], i);
    GST_DEBUG ("Read buffer #%d", i);
  }

  /* check that all 3 clients still exist */
  fail_unless_num_handles (sink, 3);

  /* now push buffers until client 2 gets kicked.
   * we don't know how much to push because both the element itself
   * and the OS pipes have internal buffering of unknown size */
  for (i = initial_buffers; get_num_handles (sink) == 3; i++) {
    GstBuffer *buffer = gst_new_buffer_big (i);
    fail_unless (gst_pad_push (mysrcpad, buffer) == GST_FLOW_OK);
    num_buffers++;
    GST_DEBUG ("Pushed buffer #%d; %d buffers queued", i,
        get_buffers_queued (sink));
  }

  /* check that 2 clients remain */
  fail_unless_num_handles (sink, 2);

  /* read the data we've pushed until now */
  for (i = initial_buffers; i < num_buffers; i++) {
    fail_unless_read_big ("client 1", pfd1[0], i);
    fail_unless_read_big ("client 3", pfd3[0], i);
    GST_DEBUG ("Read buffer #%d", i);
  }

  GST_DEBUG ("cleaning up multifdsink");
  g_signal_emit_by_name (sink, "remove", pfd1[1]);
  g_signal_emit_by_name (sink, "remove", pfd3[1]);

  fail_unless (close (pfd1[1]) == 0);
  fail_unless (close (pfd3[1]) == 0);
  fail_unless_eof ("client 1", pfd1[0]);
  fail_unless_eof ("client 3", pfd3[0]);

  ASSERT_SET_STATE (sink, GST_STATE_NULL, GST_STATE_CHANGE_SUCCESS);
  cleanup_multifdsink (sink);

  ASSERT_CAPS_REFCOUNT (caps, "caps", 1);
  gst_caps_unref (caps);
}

GST_END_TEST;

/* FIXME: add test simulating chained oggs where:
 * sync-method is burst-on-connect
 * (when multifdsink actually does burst-on-connect based on byte size, not
   "last keyframe" which any frame for audio :))
 * an old client still needs to read from before the new streamheaders
 * a new client gets the new streamheaders
 */
static Suite *
multifdsink_suite (void)
{
  Suite *s = suite_create ("multifdsink");
  TCase *tc_chain = tcase_create ("general");

  suite_add_tcase (s, tc_chain);
  tcase_add_test (tc_chain, test_no_clients);
  tcase_add_test (tc_chain, test_add_client);
  tcase_add_test (tc_chain, test_add_client_in_null_state);
  tcase_add_test (tc_chain, test_streamheader);
  tcase_add_test (tc_chain, test_change_streamheader);
  tcase_add_test (tc_chain, test_burst_client_bytes);
  tcase_add_test (tc_chain, test_burst_client_bytes_keyframe);
  tcase_add_test (tc_chain, test_burst_client_bytes_with_keyframe);
  tcase_add_test (tc_chain, test_client_next_keyframe);
  tcase_add_test (tc_chain, test_client_kick);

  return s;
}

GST_CHECK_MAIN (multifdsink);
