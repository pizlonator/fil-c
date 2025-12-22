/* A generic test engine for elements based upon GstAdaptiveDemux
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
#include <gst/check/gsttestclock.h>
#include "adaptive_demux_engine.h"

typedef struct _GstAdaptiveDemuxTestEnginePrivate
{
  GstAdaptiveDemuxTestEngine engine;
  const GstAdaptiveDemuxTestCallbacks *callbacks;
  gpointer user_data;
  guint clock_update_id;
} GstAdaptiveDemuxTestEnginePrivate;


#define GST_TEST_GET_LOCK(d)  (&(((GstAdaptiveDemuxTestEnginePrivate*)(d))->engine.lock))
#define GST_TEST_LOCK(d)      g_mutex_lock (GST_TEST_GET_LOCK (d))
#define GST_TEST_UNLOCK(d)    g_mutex_unlock (GST_TEST_GET_LOCK (d))

static void
adaptive_demux_engine_stream_state_finalize (gpointer data)
{
  GstAdaptiveDemuxTestOutputStream *stream =
      (GstAdaptiveDemuxTestOutputStream *) data;
  g_free (stream->name);
  if (stream->appsink)
    gst_object_unref (stream->appsink);
  if (stream->pad)
    gst_object_unref (stream->pad);
  if (stream->internal_pad)
    gst_object_unref (stream->internal_pad);
  g_slice_free (GstAdaptiveDemuxTestOutputStream, stream);
}

/* get the testOutput entry in testData corresponding to the given AppSink */
static GstAdaptiveDemuxTestOutputStream *
getTestOutputDataByAppsink (GstAdaptiveDemuxTestEnginePrivate * priv,
    GstAppSink * appsink)
{
  guint i;

  for (i = 0; i < priv->engine.output_streams->len; ++i) {
    GstAdaptiveDemuxTestOutputStream *state;
    state = g_ptr_array_index (priv->engine.output_streams, i);
    if (state->appsink == appsink) {
      return state;
    }
  }
  ck_abort_msg ("cannot find appsink %p in the output data", appsink);
  return NULL;
}

/* get the output stream entry in corresponding to the given Pad */
static GstAdaptiveDemuxTestOutputStream *
getTestOutputDataByPad (GstAdaptiveDemuxTestEnginePrivate * priv,
    GstPad * pad, gboolean abort_if_not_found)
{
  guint i;

  for (i = 0; i < priv->engine.output_streams->len; ++i) {
    GstAdaptiveDemuxTestOutputStream *stream;
    stream = g_ptr_array_index (priv->engine.output_streams, i);
    if (stream->internal_pad == pad || stream->pad == pad) {
      return stream;
    }
  }
  if (abort_if_not_found)
    ck_abort_msg ("cannot find pad %p in the output data", pad);
  return NULL;
}

/* get the output stream entry in corresponding to the given Pad */
static GstAdaptiveDemuxTestOutputStream *
getTestOutputDataByName (GstAdaptiveDemuxTestEnginePrivate * priv,
    const gchar * name, gboolean abort_if_not_found)
{
  guint i;

  for (i = 0; i < priv->engine.output_streams->len; ++i) {
    GstAdaptiveDemuxTestOutputStream *stream;
    stream = g_ptr_array_index (priv->engine.output_streams, i);
    if (strstr (stream->name, name) != NULL) {
      return stream;
    }
  }
  if (abort_if_not_found)
    ck_abort_msg ("cannot find pad %s in the output data", name);
  return NULL;
}

/* callback called when AppSink receives data */

/* callback called when AppSink receives data */
static GstFlowReturn
on_appSinkNewSample (GstAppSink * appsink, gpointer user_data)
{
  GstAdaptiveDemuxTestEnginePrivate *priv =
      (GstAdaptiveDemuxTestEnginePrivate *) user_data;
  GstAdaptiveDemuxTestEngine *engine;
  GstAdaptiveDemuxTestOutputStream *testOutputStream = NULL;
  GstSample *sample;
  GstBuffer *buffer;
  gboolean ret = TRUE;

  fail_unless (priv != NULL);
  GST_TEST_LOCK (priv);
  engine = &priv->engine;
  testOutputStream = getTestOutputDataByAppsink (priv, appsink);

  sample = gst_app_sink_pull_sample (appsink);
  fail_unless (sample != NULL);
  buffer = gst_sample_get_buffer (sample);
  fail_unless (buffer != NULL);

  /* call the test callback, if registered */
  if (priv->callbacks->appsink_received_data)
    ret = priv->callbacks->appsink_received_data (engine,
        testOutputStream, buffer, priv->user_data);

  testOutputStream->segment_received_size += gst_buffer_get_size (buffer);

  gst_sample_unref (sample);

  GST_TEST_UNLOCK (priv);

  if (!ret)
    return GST_FLOW_EOS;

  return GST_FLOW_OK;
}

/* callback called when AppSink receives eos */
static void
on_appSinkEOS (GstAppSink * appsink, gpointer user_data)
{
  GstAdaptiveDemuxTestEnginePrivate *priv =
      (GstAdaptiveDemuxTestEnginePrivate *) user_data;
  GstAdaptiveDemuxTestOutputStream *testOutputStream = NULL;

  fail_unless (priv != NULL);
  GST_TEST_LOCK (priv);
  testOutputStream = getTestOutputDataByAppsink (priv, appsink);

  testOutputStream->total_received_size +=
      testOutputStream->segment_received_size;
  testOutputStream->segment_received_size = 0;

  if (priv->callbacks->appsink_eos)
    priv->callbacks->appsink_eos (&priv->engine,
        testOutputStream, priv->user_data);

  GST_TEST_UNLOCK (priv);
}

static GstPadProbeReturn
on_appsink_event (GstPad * pad, GstPadProbeInfo * info, gpointer data)
{
  GstAdaptiveDemuxTestEnginePrivate *priv =
      (GstAdaptiveDemuxTestEnginePrivate *) data;
  GstAdaptiveDemuxTestOutputStream *stream = NULL;
  GstEvent *event;

  event = GST_PAD_PROBE_INFO_EVENT (info);
  GST_DEBUG ("Received event %" GST_PTR_FORMAT " on pad %" GST_PTR_FORMAT,
      event, pad);

  if (priv->callbacks->appsink_event) {
    GstPad *stream_pad = gst_pad_get_peer (pad);
    fail_unless (stream_pad != NULL);

    GST_TEST_LOCK (priv);
    stream = getTestOutputDataByPad (priv, stream_pad, TRUE);
    GST_TEST_UNLOCK (priv);
    gst_object_unref (stream_pad);
    priv->callbacks->appsink_event (&priv->engine, stream, event,
        priv->user_data);
  }

  return GST_PAD_PROBE_OK;
}


/* callback called when demux sends data to AppSink */
static GstPadProbeReturn
on_demux_sent_data (GstPad * pad, GstPadProbeInfo * info, gpointer data)
{
  GstAdaptiveDemuxTestEnginePrivate *priv =
      (GstAdaptiveDemuxTestEnginePrivate *) data;
  GstAdaptiveDemuxTestOutputStream *stream = NULL;
  GstBuffer *buffer;

  buffer = GST_PAD_PROBE_INFO_BUFFER (info);

  GST_TEST_LOCK (priv);
  stream = getTestOutputDataByPad (priv, pad, TRUE);
  GST_TEST_UNLOCK (priv);

  if (priv->callbacks->demux_sent_data) {
    (*priv->callbacks->demux_sent_data) (&priv->engine,
        stream, buffer, priv->user_data);
  }

  return GST_PAD_PROBE_OK;
}

/* callback called when dash sends event to AppSink */
static GstPadProbeReturn
on_demux_sent_event (GstPad * pad, GstPadProbeInfo * info, gpointer data)
{
  GstAdaptiveDemuxTestEnginePrivate *priv =
      (GstAdaptiveDemuxTestEnginePrivate *) data;
  GstAdaptiveDemuxTestOutputStream *stream = NULL;
  GstEvent *event;

  event = GST_PAD_PROBE_INFO_EVENT (info);

  GST_TEST_LOCK (&priv->engine);

  if (priv->callbacks->demux_sent_event) {
    stream = getTestOutputDataByPad (priv, pad, TRUE);
    (*priv->callbacks->demux_sent_event) (&priv->engine,
        stream, event, priv->user_data);
  }

  GST_TEST_UNLOCK (&priv->engine);
  return GST_PAD_PROBE_OK;
}

/* callback called when demux receives events from GstFakeSoupHTTPSrc */
static GstPadProbeReturn
on_demuxReceivesEvent (GstPad * pad, GstPadProbeInfo * info, gpointer data)
{
  GstAdaptiveDemuxTestEnginePrivate *priv =
      (GstAdaptiveDemuxTestEnginePrivate *) data;
  GstAdaptiveDemuxTestOutputStream *stream = NULL;
  GstEvent *event;
  const GstSegment *segment;

  event = GST_PAD_PROBE_INFO_EVENT (info);
  GST_DEBUG ("Received event %" GST_PTR_FORMAT " on pad %" GST_PTR_FORMAT,
      event, pad);

  if (GST_EVENT_TYPE (event) == GST_EVENT_SEGMENT) {
    /* a new segment will start arriving
     * update segment_start used by pattern validation
     */
    gst_event_parse_segment (event, &segment);

    GST_TEST_LOCK (priv);
    stream = getTestOutputDataByPad (priv, pad, TRUE);
    stream->total_received_size += stream->segment_received_size;
    stream->segment_received_size = 0;
    stream->segment_start = segment->start;
    GST_TEST_UNLOCK (priv);
  }

  return GST_PAD_PROBE_OK;
}


static void
on_demuxElementAdded (GstBin * demux, GstElement * element, gpointer user_data)
{
  GstAdaptiveDemuxTestEnginePrivate *priv =
      (GstAdaptiveDemuxTestEnginePrivate *) user_data;
  GstAdaptiveDemuxTestOutputStream *stream = NULL;
  GstPad *internal_pad;
  gchar *srcbin_name;

  srcbin_name = GST_ELEMENT_NAME (element);
  GST_TEST_LOCK (priv);

  stream = getTestOutputDataByName (priv, srcbin_name, FALSE);
  if (stream == NULL) {
    /* Pad wasn't exposed yet, create the stream */
    stream = g_slice_new0 (GstAdaptiveDemuxTestOutputStream);
    stream->name = g_strdup (srcbin_name);
    g_ptr_array_add (priv->engine.output_streams, stream);
  }

  /* keep the reference to the internal_pad.
   * We will need it to identify the stream in the on_demuxReceivesEvent callback
   */
  if (stream->internal_pad) {
    gst_pad_remove_probe (stream->internal_pad, stream->internal_pad_probe);
    gst_object_unref (stream->internal_pad);
  }
  internal_pad = gst_element_get_static_pad (element, "src");
  stream->internal_pad_probe =
      gst_pad_add_probe (internal_pad, GST_PAD_PROBE_TYPE_EVENT_DOWNSTREAM,
      (GstPadProbeCallback) on_demuxReceivesEvent, priv, NULL);
  stream->internal_pad = internal_pad;
  GST_TEST_UNLOCK (priv);

}


/* callback called when demux creates a src pad.
 * We will create an AppSink to get the data
 */
static void
on_demuxNewPad (GstElement * demux, GstPad * pad, gpointer user_data)
{
  GstAdaptiveDemuxTestEnginePrivate *priv =
      (GstAdaptiveDemuxTestEnginePrivate *) user_data;
  GstElement *pipeline;
  GstElement *sink;
  gboolean ret;
  gchar *name;
  GstPad *appsink_pad;
  GstAppSinkCallbacks appSinkCallbacks;
  GstAdaptiveDemuxTestOutputStream *stream;
  GObjectClass *gobject_class;

  fail_unless (priv != NULL);
  name = gst_pad_get_name (pad);

  GST_DEBUG ("demux created pad %p", pad);

  stream = getTestOutputDataByName (priv, name, FALSE);
  if (stream == NULL) {
    stream = g_slice_new0 (GstAdaptiveDemuxTestOutputStream);
    stream->name = g_strdup (name);
    g_ptr_array_add (priv->engine.output_streams, stream);
  }

  sink = gst_element_factory_make ("appsink", name);
  g_free (name);
  fail_unless (sink != NULL);

  GST_TEST_LOCK (priv);

  /* register the AppSink pointer in the test output data */
  gst_object_ref (sink);
  stream->appsink = GST_APP_SINK (sink);

  appSinkCallbacks.eos = on_appSinkEOS;
  appSinkCallbacks.new_preroll = NULL;
  appSinkCallbacks.new_sample = on_appSinkNewSample;

  gst_app_sink_set_callbacks (GST_APP_SINK (sink), &appSinkCallbacks, priv,
      NULL);
  appsink_pad = gst_element_get_static_pad (sink, "sink");
  gst_pad_add_probe (appsink_pad,
      GST_PAD_PROBE_TYPE_EVENT_DOWNSTREAM | GST_PAD_PROBE_TYPE_EVENT_FLUSH,
      (GstPadProbeCallback) on_appsink_event, priv, NULL);
  gst_object_unref (appsink_pad);

  gst_pad_add_probe (pad, GST_PAD_PROBE_TYPE_BUFFER,
      (GstPadProbeCallback) on_demux_sent_data, priv, NULL);
  gst_pad_add_probe (pad, GST_PAD_PROBE_TYPE_EVENT_DOWNSTREAM |
      GST_PAD_PROBE_TYPE_EVENT_FLUSH,
      (GstPadProbeCallback) on_demux_sent_event, priv, NULL);
  gobject_class = G_OBJECT_GET_CLASS (sink);
  if (g_object_class_find_property (gobject_class, "sync")) {
    GST_DEBUG ("Setting sync=FALSE on AppSink");
    g_object_set (G_OBJECT (sink), "sync", FALSE, NULL);
  }
  stream->pad = gst_object_ref (pad);

  GST_TEST_UNLOCK (priv);

  pipeline = GST_ELEMENT (gst_element_get_parent (demux));
  fail_unless (pipeline != NULL);
  ret = gst_bin_add (GST_BIN (pipeline), sink);
  fail_unless_equals_int (ret, TRUE);
  gst_object_unref (pipeline);
  ret = gst_element_link (demux, sink);
  fail_unless_equals_int (ret, TRUE);
  ret = gst_element_sync_state_with_parent (sink);
  fail_unless_equals_int (ret, TRUE);
  GST_TEST_LOCK (priv);
  if (priv->callbacks->demux_pad_added) {
    priv->callbacks->demux_pad_added (&priv->engine, stream, priv->user_data);
  }
  GST_TEST_UNLOCK (priv);
}

/* callback called when demux removes a src pad.
 * We remove the AppSink associated with this pad
 */
static void
on_demuxPadRemoved (GstElement * demux, GstPad * pad, gpointer user_data)
{
  GstAdaptiveDemuxTestEnginePrivate *priv =
      (GstAdaptiveDemuxTestEnginePrivate *) user_data;
  GstAdaptiveDemuxTestOutputStream *stream = NULL;
  GstStateChangeReturn ret;
  GstState currentState, pending;
  GstElement *appSink;

  fail_unless (priv != NULL);

  GST_DEBUG ("Pad removed: %" GST_PTR_FORMAT, pad);

  GST_TEST_LOCK (priv);
  stream = getTestOutputDataByPad (priv, pad, TRUE);
  if (priv->callbacks->demux_pad_removed) {
    priv->callbacks->demux_pad_removed (&priv->engine, stream, priv->user_data);
  }
  fail_unless (stream->appsink != NULL);
  if (stream->internal_pad) {
    gst_object_unref (stream->internal_pad);
    stream->internal_pad = NULL;
  }
  appSink = GST_ELEMENT (stream->appsink);
  ret = gst_element_get_state (appSink, &currentState, &pending, 0);
  if ((ret == GST_STATE_CHANGE_SUCCESS && currentState == GST_STATE_PLAYING)
      || (ret == GST_STATE_CHANGE_ASYNC && pending == GST_STATE_PLAYING)) {
    GST_DEBUG ("Changing AppSink element to PAUSED");
    gst_element_set_state (appSink, GST_STATE_PAUSED);
  }
  GST_TEST_UNLOCK (priv);
}

/* callback called when main_loop detects an error message
 * We will signal main loop to quit
 */
static void
on_ErrorMessageOnBus (GstBus * bus, GstMessage * msg, gpointer user_data)
{
  GstAdaptiveDemuxTestEnginePrivate *priv =
      (GstAdaptiveDemuxTestEnginePrivate *) user_data;
  GError *err = NULL;
  gchar *dbg_info = NULL;

  gst_message_parse_error (msg, &err, &dbg_info);
  GST_DEBUG ("ERROR from element %s: '%s'. Debugging info: %s",
      GST_OBJECT_NAME (msg->src), err->message, (dbg_info) ? dbg_info : "none");
  g_error_free (err);
  g_free (dbg_info);

  GST_TEST_LOCK (priv);

  fail_unless (priv->callbacks->bus_error_message,
      "unexpected error detected on bus");

  priv->callbacks->bus_error_message (&priv->engine, msg, priv->user_data);

  GST_TEST_UNLOCK (priv);
}

static gboolean
gst_adaptive_demux_update_test_clock (gpointer user_data)
{
  GstAdaptiveDemuxTestEnginePrivate *priv =
      (GstAdaptiveDemuxTestEnginePrivate *) user_data;
  GstClockID id;
  GstClockTime next_entry;
  GstTestClock *clock = GST_TEST_CLOCK (priv->engine.clock);

  fail_unless (clock != NULL);
  next_entry = gst_test_clock_get_next_entry_time (clock);
  if (next_entry != GST_CLOCK_TIME_NONE) {
    /* tests that do not want the manifest to update will set the update period
     * to a big value, eg 500s. The manifest update task will register an alarm
     * for that value.
     * We do not want the clock to jump to that. If it does, the manifest update
     * task will keep scheduling and use all the cpu power, starving the other
     * threads.
     * Usually the test require the clock to update with approx 3s, so we will
     * allow only updates smaller than 100s
     */
    GstClockTime curr_time = gst_clock_get_time (GST_CLOCK (clock));
    if (next_entry - curr_time < 100 * GST_SECOND) {
      gst_test_clock_set_time (clock, next_entry);
      id = gst_test_clock_process_next_clock_id (clock);
      fail_unless (id != NULL);
      gst_clock_id_unref (id);
    }
  }
  return TRUE;
}

static gboolean
start_pipeline_playing (gpointer user_data)
{
  GstAdaptiveDemuxTestEnginePrivate *priv =
      (GstAdaptiveDemuxTestEnginePrivate *) user_data;
  GstStateChangeReturn stateChange;

  GST_DEBUG ("Moving pipeline to PLAYING state");
  stateChange =
      gst_element_set_state (priv->engine.pipeline, GST_STATE_PLAYING);
  fail_unless (stateChange != GST_STATE_CHANGE_FAILURE);
  GST_DEBUG ("PLAYING stateChange = %d", stateChange);
  return FALSE;
}

/*
 * Create a demux element, run a test using the input data and check
 * the output data
 */
void
gst_adaptive_demux_test_run (const gchar * element_name,
    const gchar * manifest_uri,
    const GstAdaptiveDemuxTestCallbacks * callbacks, gpointer user_data)
{
  GstBus *bus;
  GstElement *demux;
  GstElement *manifest_source;
  gboolean ret;
  GstStateChangeReturn stateChange;
  GstAdaptiveDemuxTestEnginePrivate *priv;

  priv = g_slice_new0 (GstAdaptiveDemuxTestEnginePrivate);
  priv->engine.output_streams =
      g_ptr_array_new_with_free_func
      (adaptive_demux_engine_stream_state_finalize);
  g_mutex_init (&priv->engine.lock);
  priv->callbacks = callbacks;
  priv->user_data = user_data;
  priv->engine.loop = g_main_loop_new (NULL, TRUE);
  fail_unless (priv->engine.loop != NULL);
  GST_TEST_LOCK (priv);
  priv->engine.pipeline = gst_pipeline_new ("pipeline");
  fail_unless (priv->engine.pipeline != NULL);
  GST_DEBUG ("created pipeline %" GST_PTR_FORMAT, priv->engine.pipeline);

  /* register a callback to listen for error messages */
  bus = gst_pipeline_get_bus (GST_PIPELINE (priv->engine.pipeline));
  gst_bus_add_signal_watch_full (bus, G_PRIORITY_HIGH);
  g_signal_connect (bus, "message::error",
      G_CALLBACK (on_ErrorMessageOnBus), priv);

  manifest_source =
      gst_element_make_from_uri (GST_URI_SRC, manifest_uri, NULL, NULL);
  fail_unless (manifest_source != NULL);
  priv->engine.manifest_source = manifest_source;

  demux = gst_check_setup_element (element_name);
  fail_unless (demux != NULL);
  priv->engine.demux = demux;
  GST_DEBUG ("created demux %" GST_PTR_FORMAT, demux);

  g_signal_connect (demux, "element-added", G_CALLBACK (on_demuxElementAdded),
      priv);
  g_signal_connect (demux, "pad-added", G_CALLBACK (on_demuxNewPad), priv);
  g_signal_connect (demux, "pad-removed",
      G_CALLBACK (on_demuxPadRemoved), priv);

  gst_bin_add_many (GST_BIN (priv->engine.pipeline), manifest_source, demux,
      NULL);
  ASSERT_OBJECT_REFCOUNT (manifest_source, element_name, 1);
  ASSERT_OBJECT_REFCOUNT (demux, element_name, 1);

  ret = gst_element_link (manifest_source, demux);
  fail_unless_equals_int (ret, TRUE);

  priv->engine.clock = gst_system_clock_obtain ();
  if (GST_IS_TEST_CLOCK (priv->engine.clock)) {
    /*
     * live tests will want to manipulate the clock, so they will register a
     * gst_test_clock as the system clock.
     * The on demand tests do not care about the clock, so they will let the
     * system clock to the default one.
     * If a gst_test_clock was installed as system clock, we register a
     * periodic callback to update its value.
     */
    priv->clock_update_id =
        g_timeout_add (100, gst_adaptive_demux_update_test_clock, priv);
  }

  /* call a test callback before we start the pipeline */
  if (callbacks->pre_test)
    (*callbacks->pre_test) (&priv->engine, priv->user_data);

  GST_TEST_UNLOCK (priv);

  GST_DEBUG ("Starting pipeline");
  stateChange = gst_element_set_state (priv->engine.pipeline, GST_STATE_PAUSED);
  fail_unless (stateChange != GST_STATE_CHANGE_FAILURE);
  /* wait for completion of the move to PAUSED */
  stateChange = gst_element_get_state (priv->engine.pipeline, NULL, NULL,
      GST_CLOCK_TIME_NONE);
  fail_unless (stateChange != GST_STATE_CHANGE_FAILURE);

  g_idle_add ((GSourceFunc) start_pipeline_playing, priv);

  /* block until a callback calls g_main_loop_quit (engine.loop) */
  GST_DEBUG ("main thread waiting for streams to finish");
  g_main_loop_run (priv->engine.loop);
  GST_DEBUG ("main thread finished. Stopping pipeline");

  /* no need to use gst_element_get_state as the move the GST_STATE_NULL
     is always synchronous */
  stateChange = gst_element_set_state (priv->engine.pipeline, GST_STATE_NULL);
  fail_unless (stateChange != GST_STATE_CHANGE_FAILURE);

  GST_TEST_LOCK (priv);

  /* call a test callback after the stop of the pipeline */
  if (callbacks->post_test)
    (*callbacks->post_test) (&priv->engine, priv->user_data);

  g_signal_handlers_disconnect_by_func (bus,
      G_CALLBACK (on_ErrorMessageOnBus), priv);
  gst_bus_remove_signal_watch (bus);
  g_signal_handlers_disconnect_by_func (demux, G_CALLBACK (on_demuxNewPad),
      priv);
  g_signal_handlers_disconnect_by_func (demux, G_CALLBACK (on_demuxPadRemoved),
      priv);

  GST_DEBUG ("main thread pipeline stopped");
  if (priv->clock_update_id != 0)
    g_source_remove (priv->clock_update_id);
  gst_object_unref (priv->engine.clock);
  gst_object_unref (priv->engine.pipeline);
  priv->engine.pipeline = NULL;
  g_main_loop_unref (priv->engine.loop);
  g_ptr_array_unref (priv->engine.output_streams);
  gst_object_unref (bus);

  GST_TEST_UNLOCK (priv);
  g_mutex_clear (&priv->engine.lock);
  g_slice_free (GstAdaptiveDemuxTestEnginePrivate, priv);
}
