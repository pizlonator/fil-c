/* GStreamer unit tests for streamsynchronizer
 *
 * Copyright (C) 2012 Edward Hervey <edward@collabora.com>, Collabora Ltd
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
# include <config.h>
#endif

#define GLIB_DISABLE_DEPRECATION_WARNINGS

#undef GST_CAT_DEFAULT
#include <gst/check/gstcheck.h>

static GstStaticPadTemplate mysinktemplate = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS_ANY);
static GstStaticPadTemplate mysrctemplate = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS_ANY);

typedef struct
{
  GstPad *pad;
  GList *to_push;
} MyPushInfo;

GMutex push_mutex;
GCond push_cond;

static GstPad *
get_other_pad (GstPad * pad)
{
  GstIterator *it;
  GValue item = G_VALUE_INIT;
  GstPad *otherpad;

  it = gst_pad_iterate_internal_links (pad);
  fail_unless (gst_iterator_next (it, &item) == GST_ITERATOR_OK);
  otherpad = g_value_dup_object (&item);
  g_value_unset (&item);
  gst_iterator_free (it);

  return otherpad;
}

static GstFlowReturn
my_sink_chain (GstPad * pad, GstObject * parent, GstBuffer * buf)
{
  GList **expected = GST_PAD_ELEMENT_PRIVATE (pad);
  GList *next;
  GstBuffer *exp;

  fail_if (*expected == NULL,
      "streamsynchronizer pushed a buffer/event but we didn't expect any");

  next = (*expected)->next;

  fail_if (GST_IS_EVENT ((*expected)->data),
      "Expected an event (%s) but got a buffer instead",
      GST_EVENT_TYPE_NAME (GST_EVENT ((*expected)->data)));

  exp = GST_BUFFER ((*expected)->data);

  fail_unless_equals_uint64 (GST_BUFFER_TIMESTAMP (buf),
      GST_BUFFER_TIMESTAMP (exp));

  GST_DEBUG ("Properly received expected buffer");
  gst_buffer_unref (exp);
  gst_buffer_unref (buf);

  g_list_free1 (*expected);
  *expected = next;

  /* When done signal main thread */
  if (next == NULL) {
    g_mutex_lock (&push_mutex);
    g_cond_signal (&push_cond);
    g_mutex_unlock (&push_mutex);
  }

  return GST_FLOW_OK;
}

static gboolean
my_sink_event (GstPad * pad, GstObject * parent, GstEvent * event)
{
  GList **expected = GST_PAD_ELEMENT_PRIVATE (pad);
  GList *next;
  GstEvent *exp;

  fail_if (*expected == NULL,
      "streamsynchronizer pushed a buffer/event but we didn't expect any");

  next = (*expected)->next;

  fail_unless (GST_IS_EVENT ((*expected)->data),
      "We were not expecting an event (But got an event of type %s)",
      GST_EVENT_TYPE_NAME (event));
  exp = GST_EVENT ((*expected)->data);
  fail_unless (GST_EVENT_TYPE (event) == GST_EVENT_TYPE (exp),
      "Got event of type %s but expected of type %s",
      GST_EVENT_TYPE_NAME (event), GST_EVENT_TYPE_NAME (exp));
  fail_unless_equals_int (GST_EVENT_SEQNUM (event), GST_EVENT_SEQNUM (exp));
  /* FIXME : Check more types */

  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_SEGMENT:
    {
      const GstSegment *recvseg, *expectseg;

      /* Compare segment values */
      gst_event_parse_segment (event, &recvseg);
      gst_event_parse_segment (exp, &expectseg);

      fail_unless_equals_int (recvseg->format, expectseg->format);
      fail_unless_equals_uint64 (recvseg->base, expectseg->base);
      fail_unless_equals_uint64 (recvseg->offset, expectseg->offset);
      fail_unless_equals_uint64 (recvseg->start, expectseg->start);
      fail_unless_equals_uint64 (recvseg->stop, expectseg->stop);
      fail_unless_equals_uint64 (recvseg->time, expectseg->time);
      fail_unless_equals_uint64 (recvseg->position, expectseg->position);
      fail_unless_equals_uint64 (recvseg->duration, expectseg->duration);
    }
      break;
    default:
      break;
  }

  GST_DEBUG ("Properly received expected event %s", GST_EVENT_TYPE_NAME (exp));

  gst_event_unref (exp);
  gst_event_unref (event);

  g_list_free1 (*expected);
  *expected = next;

  /* When done signal main thread */
  if (next == NULL) {
    g_mutex_lock (&push_mutex);
    g_cond_signal (&push_cond);
    g_mutex_unlock (&push_mutex);
  }

  return TRUE;
}

static gpointer
my_push_thread (MyPushInfo * pushinfo)
{
  GList *tmp;

  /* FIXME : Do this in a thread */
  for (tmp = pushinfo->to_push; tmp; tmp = tmp->next) {
    if (GST_IS_EVENT (tmp->data))
      gst_pad_push_event (pushinfo->pad, GST_EVENT (tmp->data));
    else
      gst_pad_push (pushinfo->pad, GST_BUFFER (tmp->data));
  }

  GST_INFO ("leaving thread");
  return NULL;
}

GST_START_TEST (test_basic)
{
  GstElement *synchr;
  GstPad *sinkpad, *srcpad;
  GstPad *mysrcpad, *mysinkpad;
  GList *to_push = NULL, *expected = NULL;
  GstEvent *event;
  GstBuffer *buf;
  GThread *thread;
  MyPushInfo pushinfo;
  guint i;
  GstSegment segment;
  guint32 seqnum;

  synchr = gst_element_factory_make ("streamsynchronizer", NULL);

  /* Get sinkpad/srcpad */
  sinkpad = gst_element_request_pad_simple (synchr, "sink_%u");
  fail_unless (sinkpad != NULL);
  srcpad = get_other_pad (sinkpad);
  fail_unless (srcpad != NULL);

  gst_element_set_state (synchr, GST_STATE_PLAYING);

  mysrcpad = gst_pad_new_from_static_template (&mysrctemplate, "src");
  fail_if (mysrcpad == NULL);
  fail_unless (gst_pad_link (mysrcpad, sinkpad) == GST_PAD_LINK_OK);
  fail_unless (gst_pad_set_active (mysrcpad, TRUE));

  mysinkpad = gst_pad_new_from_static_template (&mysinktemplate, "sink");
  gst_pad_set_chain_function (mysinkpad, my_sink_chain);
  gst_pad_set_event_function (mysinkpad, my_sink_event);
  fail_if (mysinkpad == NULL);
  fail_unless (gst_pad_link (srcpad, mysinkpad) == GST_PAD_LINK_OK);
  fail_unless (gst_pad_set_active (mysinkpad, TRUE));
  GST_PAD_ELEMENT_PRIVATE (mysinkpad) = &expected;

  /* Start with a stream START and a new segment */
  event = gst_event_new_stream_start ("lala");
  to_push = g_list_append (to_push, event);
  expected = g_list_append (expected, gst_event_ref (event));

  gst_segment_init (&segment, GST_FORMAT_TIME);
  event = gst_event_new_segment (&segment);
  to_push = g_list_append (to_push, event);
  expected = g_list_append (expected, gst_event_ref (event));

  /* Then 10 buffers */
  for (i = 0; i < 10; i++) {
    buf = gst_buffer_new ();
    GST_BUFFER_TIMESTAMP (buf) = i * GST_SECOND;
    GST_BUFFER_DURATION (buf) = GST_SECOND;
    to_push = g_list_append (to_push, buf);
    expected = g_list_append (expected, gst_buffer_ref (buf));
  }

  /* Then a new stream start */
  event = gst_event_new_stream_start ("lala again");
  to_push = g_list_append (to_push, event);
  expected = g_list_append (expected, gst_event_ref (event));

  /* This newsegment will be updated */
  gst_segment_init (&segment, GST_FORMAT_TIME);
  event = gst_event_new_segment (&segment);
  seqnum = gst_event_get_seqnum (event);
  to_push = g_list_append (to_push, event);
  /* The received segment's base should be updated by streamsynchronizer to
   * take into account the amount of data played before (i.e. 10s) */
  segment.base = 10 * GST_SECOND;
  event = gst_event_new_segment (&segment);
  gst_event_set_seqnum (event, seqnum);
  expected = g_list_append (expected, event);

  /* Then 10 buffers */
  for (i = 0; i < 10; i++) {
    buf = gst_buffer_new ();
    GST_BUFFER_TIMESTAMP (buf) = i * GST_SECOND;
    GST_BUFFER_DURATION (buf) = GST_SECOND;
    to_push = g_list_append (to_push, buf);
    expected = g_list_append (expected, gst_buffer_ref (buf));
  }

  g_mutex_init (&push_mutex);
  g_cond_init (&push_cond);

  pushinfo.pad = mysrcpad;
  pushinfo.to_push = to_push;
  g_mutex_lock (&push_mutex);
  thread = g_thread_new ("pushthread", (GThreadFunc) my_push_thread, &pushinfo);
  fail_unless (thread != NULL);

  g_cond_wait (&push_cond, &push_mutex);
  g_mutex_unlock (&push_mutex);

  fail_if (expected != NULL);

  /* wait for thread to exit before freeing things */
  g_thread_join (thread);

  /* Cleanup */
  g_list_free (to_push);
  gst_element_release_request_pad (synchr, sinkpad);
  gst_object_unref (srcpad);
  gst_object_unref (sinkpad);
  gst_object_unref (mysinkpad);
  gst_object_unref (mysrcpad);
  gst_element_set_state (synchr, GST_STATE_NULL);
  gst_object_unref (synchr);
}

GST_END_TEST;

static Suite *
streamsynchronizer_suite (void)
{
  Suite *s = suite_create ("streamsynchronizer");
  TCase *tc_chain = tcase_create ("general");

  suite_add_tcase (s, tc_chain);
  tcase_add_test (tc_chain, test_basic);

  return s;
}

GST_CHECK_MAIN (streamsynchronizer);
