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

#ifndef __GST_ADAPTIVE_DEMUX_TEST_ENGINE_H__
#define __GST_ADAPTIVE_DEMUX_TEST_ENGINE_H__

#include <gst/gst.h>
#include <gst/app/gstappsink.h>
#include "test_http_src.h"

G_BEGIN_DECLS

typedef struct _GstAdaptiveDemuxTestEngine GstAdaptiveDemuxTestEngine;

typedef struct _GstAdaptiveDemuxTestOutputStream {
  gchar *name;

  /* the GstAppSink element getting the data for this stream */
  GstAppSink *appsink;
  GstPad *pad;
  /* the internal pad of adaptivedemux element used to send data to the GstAppSink element */
  GstPad *internal_pad;
  gulong internal_pad_probe;
  /* current segment start offset */
  guint64 segment_start;
  /* the size received so far on this segment */
  guint64 segment_received_size;
  /* the total size received so far on this stream, excluding current segment */
  guint64 total_received_size;
} GstAdaptiveDemuxTestOutputStream;

/* GstAdaptiveDemuxTestCallbacks: contains various callbacks that can
 * be registered by a test. Not all callbacks needs to be configured
 * by a test. A callback that is not required by a test must be set
 * to NULL.
 */
typedef struct _GstAdaptiveDemuxTestCallbacks
{
  /**
   * pre_test: called before starting the pipeline
   * @engine: #GstAdaptiveDemuxTestEngine
   * @user_data: the user_data passed to gst_adaptive_demux_test_run()
   */
  void (*pre_test) (GstAdaptiveDemuxTestEngine *engine, gpointer user_data);
  
  /**
   * post_test: called after stopping the pipeline.
   * @engine: #GstAdaptiveDemuxTestEngine
   * @user_data: the user_data passed to gst_adaptive_demux_test_run()
   */
  void (*post_test) (GstAdaptiveDemuxTestEngine *engine, gpointer user_data);

  /**
   * appsink_received_data: called each time AppSink receives data
   * @engine: #GstAdaptiveDemuxTestEngine
   * @stream: #GstAdaptiveDemuxTestOutputStream
   * @buffer: the #GstBuffer that was received by #GstAppSink
   * @user_data: the user_data passed to gst_adaptive_demux_test_run()
   * Returns: %TRUE to continue processing, %FALSE to cause EOS
   *
   * Can be used by a test to perform additional operations (eg validate
   * output data)
   */
  gboolean (*appsink_received_data) (GstAdaptiveDemuxTestEngine *engine,
      GstAdaptiveDemuxTestOutputStream * stream,
      GstBuffer * buffer, gpointer user_data);

  /**
   * appsink_eos: called each time AppSink receives eos
   * @engine: #GstAdaptiveDemuxTestEngine
   * @stream: #GstAdaptiveDemuxTestOutputStream
   * @user_data: the user_data passed to gst_adaptive_demux_test_run()
   *
   * Can be used by a test to perform additional operations (eg validate
   * output data)
   */
  void (*appsink_eos) (GstAdaptiveDemuxTestEngine *engine,
      GstAdaptiveDemuxTestOutputStream * stream, gpointer user_data);

  /**
   * appsink_event: called when an event is received by appsink
   * @engine: #GstAdaptiveDemuxTestEngine
   * @stream: #GstAdaptiveDemuxTestOutputStream
   * @event: the #GstEvent that was pushed in the demuxer pad
   * @user_data: the user_data passed to gst_adaptive_demux_test_run()
   *
   * Can be used by a test to do some checks on the events
   */
  void (*appsink_event) (GstAdaptiveDemuxTestEngine *engine,
      GstAdaptiveDemuxTestOutputStream * stream,
      GstEvent * event, gpointer user_data);

  /**
   * demux_pad_added: called each time the demux creates a new pad
   * @engine: #GstAdaptiveDemuxTestEngine
   * @stream: the #GstAdaptiveDemuxTestOutputStream that has been created
   * @user_data: the user_data passed to gst_adaptive_demux_test_run()
   */
  void (*demux_pad_added) (GstAdaptiveDemuxTestEngine * engine,
      GstAdaptiveDemuxTestOutputStream * stream, gpointer user_data);

  /**
   * demux_pad_removed: called each time the demux removes a pad
   * @engine: #GstAdaptiveDemuxTestEngine
   * @stream: the #GstAdaptiveDemuxTestOutputStream that will no longer
   * be used
   * @user_data: the user_data passed to gst_adaptive_demux_test_run()
   */
  void (*demux_pad_removed) (GstAdaptiveDemuxTestEngine * engine,
      GstAdaptiveDemuxTestOutputStream * stream, gpointer user_data);

  /**
   * demux_sent_data: called each time the demux sends data to AppSink
   * @engine: #GstAdaptiveDemuxTestEngine
   * @stream: #GstAdaptiveDemuxTestOutputStream
   * @buffer: the #GstBuffer that was sent by demux
   * @user_data: the user_data passed to gst_adaptive_demux_test_run()
   */
  gboolean (*demux_sent_data) (GstAdaptiveDemuxTestEngine *engine,
      GstAdaptiveDemuxTestOutputStream * stream,
      GstBuffer * buffer, gpointer user_data);

  /**
   * demux_sent_event: called each time the demux sends event to AppSink
   * @engine: #GstAdaptiveDemuxTestEngine
   * @stream: #GstAdaptiveDemuxTestOutputStream
   * @event: the #GstEvent that was sent by demux
   * @user_data: the user_data passed to gst_adaptive_demux_test_run()
   */
  gboolean (*demux_sent_event) (GstAdaptiveDemuxTestEngine *engine,
      GstAdaptiveDemuxTestOutputStream * stream,
      GstEvent * event, gpointer user_data);

  /**
   * bus_error_message: called if an error is posted to the bus
   * @engine: #GstAdaptiveDemuxTestEngine
   * @msg: the #GstMessage that contains the error
   * @user_data: the user_data passed to gst_adaptive_demux_test_run()
   *
   * The callback can decide if this error is expected, or to fail
   * the test
   */
  void (*bus_error_message)(GstAdaptiveDemuxTestEngine *engine,
      GstMessage * msg, gpointer user_data);
} GstAdaptiveDemuxTestCallbacks;

/* structure containing all data used by a test
 * Any callback defined by a test will receive this as first parameter
 */
struct _GstAdaptiveDemuxTestEngine
{
  GstElement *pipeline;
  GstClock *clock;
  GstElement *demux;
  GstElement *manifest_source;
  GMainLoop *loop;
  GPtrArray *output_streams; /* GPtrArray<GstAdaptiveDemuxTestOutputStream> */
  /* mutex to lock accesses to this structure when data is shared 
   * between threads */
  GMutex lock;
};

/**
 * gst_adaptive_demux_test_run:
 * @element_name: The name of the demux element (e.g. "dashdemux")
 * @manifest_uri: The URI of the manifest to load
 * @callbacks: The callbacks to use while the test is in operating
 * @user_data: Opaque pointer that is passed to every callback
 *
 * Creates a pipeline with the specified demux element in it,
 * connect a testhttpsrc element to this demux element and
 * request manifest_uri. When the demux element adds a new
 * pad, the engine will create an AppSink element and attach
 * it to this pad. 
 *
 * Information about these pads is collected in 
 * GstAdaptiveDemuxTestEngine::output_streams
 */
void gst_adaptive_demux_test_run (const gchar * element_name,
    const gchar * manifest_uri,
    const GstAdaptiveDemuxTestCallbacks * callbacks,
    gpointer user_data);

G_END_DECLS
#endif /* __GST_ADAPTIVE_DEMUX_TEST_ENGINE_H__ */
