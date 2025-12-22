/* A set of utility functions that are common between elements
 * based upon GstAdaptiveDemux
 *
 * Copyright (c) <2015> YouView TV Ltd
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

#include <gst/check/gstcheck.h>
#include "adaptive_demux_engine.h"
#include "adaptive_demux_common.h"

#define GST_TEST_HTTP_SRC_NAME            "testhttpsrc"

#define gst_adaptive_demux_test_case_parent_class parent_class

static void gst_adaptive_demux_test_case_dispose (GObject * object);
static void gst_adaptive_demux_test_case_finalize (GObject * object);
static void gst_adaptive_demux_test_case_clear (GstAdaptiveDemuxTestCase *
    testData);

G_DEFINE_TYPE (GstAdaptiveDemuxTestCase, gst_adaptive_demux_test_case,
    G_TYPE_OBJECT);

static void
gst_adaptive_demux_test_case_class_init (GstAdaptiveDemuxTestCaseClass * klass)
{
  GObjectClass *object = G_OBJECT_CLASS (klass);

  object->dispose = gst_adaptive_demux_test_case_dispose;
  object->finalize = gst_adaptive_demux_test_case_finalize;
}

static void
gst_adaptive_demux_test_case_init (GstAdaptiveDemuxTestCase * testData)
{
  testData->output_streams = NULL;
  testData->test_task = NULL;
  g_rec_mutex_init (&testData->test_task_lock);
  g_mutex_init (&testData->test_task_state_lock);
  g_cond_init (&testData->test_task_state_cond);
  gst_adaptive_demux_test_case_clear (testData);
}

static void
gst_adaptive_demux_test_case_clear (GstAdaptiveDemuxTestCase * testData)
{
  if (testData->output_streams) {
    g_list_free (testData->output_streams);
    testData->output_streams = NULL;
  }
  testData->count_of_finished_streams = 0;
  if (testData->test_task) {
    gst_task_stop (testData->test_task);
    gst_task_join (testData->test_task);
    gst_object_unref (testData->test_task);
    testData->test_task = NULL;
  }
  testData->signal_context = NULL;
  testData->test_task_state = TEST_TASK_STATE_NOT_STARTED;
  testData->threshold_for_seek = 0;
  gst_event_replace (&testData->seek_event, NULL);
  testData->signal_context = NULL;
}


static void
gst_adaptive_demux_test_case_dispose (GObject * object)
{
  GstAdaptiveDemuxTestCase *testData = GST_ADAPTIVE_DEMUX_TEST_CASE (object);

  gst_adaptive_demux_test_case_clear (testData);

  GST_CALL_PARENT (G_OBJECT_CLASS, dispose, (object));
}

static void
gst_adaptive_demux_test_case_finalize (GObject * object)
{
  GstAdaptiveDemuxTestCase *testData = GST_ADAPTIVE_DEMUX_TEST_CASE (object);

  g_cond_clear (&testData->test_task_state_cond);
  g_mutex_clear (&testData->test_task_state_lock);
  g_rec_mutex_clear (&testData->test_task_lock);
  if (testData->test_task) {
    gst_task_stop (testData->test_task);
    gst_task_join (testData->test_task);
    gst_object_unref (testData->test_task);
    testData->test_task = NULL;
  }
  if (testData->output_streams) {
    g_list_free (testData->output_streams);
    testData->output_streams = NULL;
  }

  GST_CALL_PARENT (G_OBJECT_CLASS, finalize, (object));
}

/**
 * gst_adaptive_demux_test_case_new:
 *
 * Creates a new #GstAdaptiveDemuxTestCase. Free with g_object_unref().
 *
 * Returns: (transfer full): a new #GstAdaptiveDemuxTestCase
 */
GstAdaptiveDemuxTestCase *
gst_adaptive_demux_test_case_new (void)
{
  return g_object_new (GST_TYPE_ADAPTIVE_DEMUX_TEST_CASE, NULL);
}


GstAdaptiveDemuxTestExpectedOutput *
gst_adaptive_demux_test_find_test_data_by_stream (GstAdaptiveDemuxTestCase *
    testData, GstAdaptiveDemuxTestOutputStream * stream, guint * index)
{
  gchar *pad_name;
  GstAdaptiveDemuxTestExpectedOutput *ret = NULL;
  guint count = 0;
  GList *walk;

  pad_name = gst_pad_get_name (stream->pad);
  fail_unless (pad_name != NULL);
  for (walk = testData->output_streams; walk; walk = g_list_next (walk)) {
    GstAdaptiveDemuxTestExpectedOutput *td = walk->data;
    if (strcmp (td->name, pad_name) == 0) {
      ret = td;
      if (index)
        *index = count;
    }
    ++count;
  }
  g_free (pad_name);
  return ret;
}

/* function to validate data received by AppSink */
gboolean
gst_adaptive_demux_test_check_received_data (GstAdaptiveDemuxTestEngine *
    engine, GstAdaptiveDemuxTestOutputStream * stream, GstBuffer * buffer,
    gpointer user_data)
{
  GstMapInfo info;
  guint pattern;
  guint64 streamOffset;
  GstAdaptiveDemuxTestCase *testData = GST_ADAPTIVE_DEMUX_TEST_CASE (user_data);
  GstAdaptiveDemuxTestExpectedOutput *testOutputStreamData;
  guint64 i;

  fail_unless (stream != NULL);
  fail_unless (engine->pipeline != NULL);
  testOutputStreamData =
      gst_adaptive_demux_test_find_test_data_by_stream (testData, stream, NULL);
  fail_unless (testOutputStreamData != NULL);

  GST_DEBUG
      ("total_received_size=%" G_GUINT64_FORMAT
      " segment_received_size = %" G_GUINT64_FORMAT
      " buffer_size=%" G_GUINT64_FORMAT
      " expected_size=%" G_GUINT64_FORMAT
      " segment_start = %" G_GUINT64_FORMAT,
      stream->total_received_size,
      stream->segment_received_size,
      (guint64) gst_buffer_get_size (buffer),
      testOutputStreamData->expected_size, stream->segment_start);

  /* Only verify after seeking */
  if (testData->seek_event && testData->seeked)
    fail_unless (stream->total_received_size +
        stream->segment_received_size +
        gst_buffer_get_size (buffer) <= testOutputStreamData->expected_size,
        "Received unexpected data, please check what segments are being downloaded");

  streamOffset = stream->segment_start + stream->segment_received_size;
  if (testOutputStreamData->expected_data) {
    gsize size = gst_buffer_get_size (buffer);
    if (gst_buffer_memcmp (buffer, 0,
            &testOutputStreamData->expected_data[streamOffset], size) == 0) {
      return TRUE;
    }
    /* If buffers do not match, fall back to a slower byte-based check
       so that the test can output the position where the received data
       diverges from expected_data
     */
  }

  gst_buffer_map (buffer, &info, GST_MAP_READ);

  pattern = streamOffset - streamOffset % sizeof (pattern);
  for (i = 0; i != info.size; ++i) {
    guint received = info.data[i];
    guint expected;

    if (testOutputStreamData->expected_data) {
      fail_unless (streamOffset + i < testOutputStreamData->expected_size);
      expected = testOutputStreamData->expected_data[streamOffset + i];
    } else {
      gchar pattern_byte_to_read;

      pattern_byte_to_read = (streamOffset + i) % sizeof (pattern);
      if (pattern_byte_to_read == 0) {
        pattern = streamOffset + i;
      }

      expected = (pattern >> (pattern_byte_to_read * 8)) & 0xFF;
#if 0
      GST_DEBUG
          ("received '0x%02x' expected '0x%02x' offset %" G_GUINT64_FORMAT
          " pattern=%08x byte_to_read=%d",
          received, expected, i, pattern, pattern_byte_to_read);
#endif
    }

    fail_unless (received == expected,
        "output validation failed: received '0x%02x' expected '0x%02x' byte %"
        G_GUINT64_FORMAT " offset=%" G_GUINT64_FORMAT "\n", received, expected,
        i, streamOffset);
  }

  gst_buffer_unmap (buffer, &info);
  return TRUE;
}

/* AppSink EOS callback.
 * To be used by tests that don't expect AppSink to receive EOS.
 */
void
gst_adaptive_demux_test_unexpected_eos (GstAdaptiveDemuxTestEngine *
    engine, GstAdaptiveDemuxTestOutputStream * stream, gpointer user_data)
{
  fail_if (TRUE);
}

/* AppSink EOS callback.
 * To be used by tests that expect AppSink to receive EOS.
 * Will check total size of data received by AppSink.
 */
void
gst_adaptive_demux_test_check_size_of_received_data (GstAdaptiveDemuxTestEngine
    * engine, GstAdaptiveDemuxTestOutputStream * stream, gpointer user_data)
{
  GstAdaptiveDemuxTestCase *testData = GST_ADAPTIVE_DEMUX_TEST_CASE (user_data);
  GstAdaptiveDemuxTestExpectedOutput *testOutputStreamData;

  testOutputStreamData =
      gst_adaptive_demux_test_find_test_data_by_stream (testData, stream, NULL);
  fail_unless (testOutputStreamData != NULL);

  fail_unless (stream->total_received_size ==
      testOutputStreamData->expected_size,
      "size validation failed, expected %d received %d",
      testOutputStreamData->expected_size, stream->total_received_size);
  testData->count_of_finished_streams++;
  if (testData->count_of_finished_streams ==
      g_list_length (testData->output_streams)) {
    g_main_loop_quit (engine->loop);
  }
}

typedef struct _SeekTaskContext
{
  GstElement *pipeline;
  GstTask *task;
  GstEvent *seek_event;
} SeekTaskContext;

/* function to generate a seek event. Will be run in a separate thread */
static void
testSeekTaskDoSeek (gpointer user_data)
{
  SeekTaskContext *context = (SeekTaskContext *) user_data;
  GstTask *task;

  GST_DEBUG ("testSeekTaskDoSeek calling seek");

  fail_unless (GST_IS_EVENT (context->seek_event));
  fail_unless (GST_EVENT_TYPE (context->seek_event) == GST_EVENT_SEEK);

  if (!gst_element_send_event (GST_ELEMENT (context->pipeline),
          context->seek_event))
    fail ("Seek failed!\n");
  GST_DEBUG ("seek ok");
  task = context->task;
  g_slice_free (SeekTaskContext, context);
  gst_task_stop (task);
}

/* function to be called during seek test when demux sends data to AppSink
 * It monitors the data sent and after a while will generate a seek request.
 */
static gboolean
testSeekAdaptiveDemuxSendsData (GstAdaptiveDemuxTestEngine * engine,
    GstAdaptiveDemuxTestOutputStream * stream,
    GstBuffer * buffer, gpointer user_data)
{
  GstAdaptiveDemuxTestCase *testData = GST_ADAPTIVE_DEMUX_TEST_CASE (user_data);
  SeekTaskContext *seekContext;
  GstAdaptiveDemuxTestExpectedOutput *testOutputStreamData;
  guint index = 0;

  testOutputStreamData =
      gst_adaptive_demux_test_find_test_data_by_stream (testData, stream,
      &index);
  fail_unless (testOutputStreamData != NULL);
  /* first entry in testData->output_streams is the
     PAD on which to perform the seek */
  if (index == 0 &&
      testData->test_task == NULL &&
      (stream->total_received_size + stream->segment_received_size) >=
      testData->threshold_for_seek) {
    GstSeekFlags seek_flags;

    testData->threshold_for_seek =
        stream->total_received_size + stream->segment_received_size;

    gst_event_parse_seek (testData->seek_event, NULL, NULL, &seek_flags, NULL,
        NULL, NULL, NULL);
    if (seek_flags & GST_SEEK_FLAG_FLUSH)
      testOutputStreamData->expected_size += testData->threshold_for_seek;

    GST_DEBUG ("starting seek task");

    g_mutex_lock (&testData->test_task_state_lock);
    testData->test_task_state =
        TEST_TASK_STATE_WAITING_FOR_TESTSRC_STATE_CHANGE;
    g_mutex_unlock (&testData->test_task_state_lock);

    seekContext = g_slice_new (SeekTaskContext);
    seekContext->pipeline = engine->pipeline;
    seekContext->seek_event = gst_event_ref (testData->seek_event);
    testData->test_task = seekContext->task =
        gst_task_new ((GstTaskFunction) testSeekTaskDoSeek, seekContext, NULL);
    gst_task_set_lock (testData->test_task, &testData->test_task_lock);
    gst_task_start (testData->test_task);

    GST_DEBUG ("seek task started");

    if (seek_flags & GST_SEEK_FLAG_FLUSH) {
      g_mutex_lock (&testData->test_task_state_lock);

      GST_DEBUG ("waiting for seek task to change state on testsrc");

      /* wait for test_task to run, send a flush start event to AppSink
       * and change the testhttpsrc element state from PLAYING to PAUSED
       */
      while (testData->test_task_state ==
          TEST_TASK_STATE_WAITING_FOR_TESTSRC_STATE_CHANGE) {
        g_cond_wait (&testData->test_task_state_cond,
            &testData->test_task_state_lock);
      }
      testData->seeked = TRUE;
      g_mutex_unlock (&testData->test_task_state_lock);
      /* we can continue now, but this buffer will be rejected by AppSink
       * because it is in flushing mode
       */
      GST_DEBUG ("seek task changed state on testsrc, resuming");
    }
  }

  return TRUE;
}

static void
testSeekAdaptiveAppSinkEvent (GstAdaptiveDemuxTestEngine * engine,
    GstAdaptiveDemuxTestOutputStream * stream,
    GstEvent * event, gpointer user_data)
{
  GstAdaptiveDemuxTestCase *testData = GST_ADAPTIVE_DEMUX_TEST_CASE (user_data);
  GstAdaptiveDemuxTestExpectedOutput *testOutputStreamData;
  guint index = 0;

  testOutputStreamData =
      gst_adaptive_demux_test_find_test_data_by_stream (testData, stream,
      &index);
  fail_unless (testOutputStreamData != NULL);

  if (testData->seek_event && GST_EVENT_TYPE (event) == GST_EVENT_SEGMENT
      && testOutputStreamData->post_seek_segment.format != GST_FORMAT_UNDEFINED
      && gst_event_get_seqnum (event) ==
      gst_event_get_seqnum (testData->seek_event)) {
    const GstSegment *seek_segment;


    gst_event_parse_segment (event, &seek_segment);
    fail_unless (seek_segment->format ==
        testOutputStreamData->post_seek_segment.format);
    fail_unless (seek_segment->rate ==
        testOutputStreamData->post_seek_segment.rate);
    fail_unless (seek_segment->start ==
        testOutputStreamData->post_seek_segment.start);
    fail_unless (seek_segment->stop ==
        testOutputStreamData->post_seek_segment.stop);
    fail_unless (seek_segment->base ==
        testOutputStreamData->post_seek_segment.base);
    fail_unless (seek_segment->time ==
        testOutputStreamData->post_seek_segment.time);

    testOutputStreamData->segment_verification_needed = FALSE;
  }
}

/* callback called when main_loop detects a state changed event */
static void
testSeekOnStateChanged (GstBus * bus, GstMessage * msg, gpointer user_data)
{
  GstAdaptiveDemuxTestCase *testData = GST_ADAPTIVE_DEMUX_TEST_CASE (user_data);
  GstState old_state, new_state;
  const char *srcName = GST_OBJECT_NAME (msg->src);

  gst_message_parse_state_changed (msg, &old_state, &new_state, NULL);
  GST_DEBUG ("Element %s changed state from %s to %s",
      GST_OBJECT_NAME (msg->src),
      gst_element_state_get_name (old_state),
      gst_element_state_get_name (new_state));

  if (strstr (srcName, "srcbin") == srcName &&
      old_state == GST_STATE_PLAYING && new_state == GST_STATE_PAUSED) {
    g_mutex_lock (&testData->test_task_state_lock);
    if (testData->test_task_state ==
        TEST_TASK_STATE_WAITING_FOR_TESTSRC_STATE_CHANGE) {
      GST_DEBUG ("changing test_task_state");
      testData->test_task_state = TEST_TASK_STATE_EXITING;
      g_cond_signal (&testData->test_task_state_cond);
    }
    g_mutex_unlock (&testData->test_task_state_lock);
  }
}

/*
 * Issue a seek request after media segment has started to be downloaded
 * on the first pad listed in GstAdaptiveDemuxTestOutputStreamData and the
 * first chunk of at least one byte has already arrived in AppSink
 */
static void
testSeekPreTestCallback (GstAdaptiveDemuxTestEngine * engine,
    gpointer user_data)
{
  GstAdaptiveDemuxTestCase *testData = GST_ADAPTIVE_DEMUX_TEST_CASE (user_data);
  GstBus *bus;

  /* register a callback to listen for state change events */
  bus = gst_pipeline_get_bus (GST_PIPELINE (engine->pipeline));
  gst_bus_add_signal_watch (bus);
  g_signal_connect (bus, "message::state-changed",
      G_CALLBACK (testSeekOnStateChanged), testData);
  gst_object_unref (bus);
}

static void
testSeekPostTestCallback (GstAdaptiveDemuxTestEngine * engine,
    gpointer user_data)
{
  GstBus *bus;
  GList *walk;

  GstAdaptiveDemuxTestCase *testData = GST_ADAPTIVE_DEMUX_TEST_CASE (user_data);
  for (walk = testData->output_streams; walk; walk = g_list_next (walk)) {
    GstAdaptiveDemuxTestExpectedOutput *td = walk->data;

    fail_if (td->segment_verification_needed);
  }

  bus = gst_pipeline_get_bus (GST_PIPELINE (engine->pipeline));
  gst_bus_remove_signal_watch (bus);
  gst_object_unref (bus);
}

/* function to check total size of data received by AppSink
 * will be called when AppSink receives eos.
 */
void gst_adaptive_demux_test_download_error_size_of_received_data
    (GstAdaptiveDemuxTestEngine * engine,
    GstAdaptiveDemuxTestOutputStream * stream, gpointer user_data)
{
  GstAdaptiveDemuxTestCase *testData = GST_ADAPTIVE_DEMUX_TEST_CASE (user_data);
  GstAdaptiveDemuxTestExpectedOutput *testOutputStreamData;

  testOutputStreamData =
      gst_adaptive_demux_test_find_test_data_by_stream (testData, stream, NULL);
  fail_unless (testOutputStreamData != NULL);
  /* expect to receive more than 0 */
  fail_unless (stream->total_received_size > 0,
      "size validation failed for %s, expected > 0, received %d",
      testOutputStreamData->name, stream->total_received_size);

  /* expect to receive less than file size */
  fail_unless (stream->total_received_size <
      testOutputStreamData->expected_size,
      "size validation failed for %s, expected < %d received %d",
      testOutputStreamData->name, testOutputStreamData->expected_size,
      stream->total_received_size);
  if (testData->count_of_finished_streams ==
      g_list_length (testData->output_streams)) {
    g_main_loop_quit (engine->loop);
  }
}

void
gst_adaptive_demux_test_seek (const gchar * element_name,
    const gchar * manifest_uri, GstAdaptiveDemuxTestCase * testData)
{
  GstAdaptiveDemuxTestCallbacks cb = { 0 };
  cb.appsink_received_data = gst_adaptive_demux_test_check_received_data;
  cb.appsink_eos = gst_adaptive_demux_test_check_size_of_received_data;
  cb.appsink_event = testSeekAdaptiveAppSinkEvent;
  cb.pre_test = testSeekPreTestCallback;
  cb.post_test = testSeekPostTestCallback;
  cb.demux_sent_data = testSeekAdaptiveDemuxSendsData;
  gst_adaptive_demux_test_run (element_name, manifest_uri, &cb, testData);
  /* the call to g_object_unref of testData will clean up the seek task */
}

void
gst_adaptive_demux_test_setup (void)
{
  GstRegistry *registry;
  gboolean ret;

  registry = gst_registry_get ();
  ret = gst_test_http_src_register_plugin (registry, GST_TEST_HTTP_SRC_NAME);
  fail_unless (ret);
}

void
gst_adaptive_demux_test_teardown (void)
{
  gst_test_http_src_install_callbacks (NULL, NULL);
  gst_test_http_src_set_default_blocksize (0);
}
