/* GStreamer
 * Copyright (C) 1999,2000 Erik Walthinsen <omega@cse.ogi.edu>
 *                    2005 Wim Taymans <wim@fluendo.com>
 *                    2006 Thomas Vander Stichele <thomas at apestaart dot org>
 *                    2014 Tim-Philipp Müller <tim centricular com>
 *               2015-2017 YouView TV Ltd, Vincent Penquerc'h <vincent.penquerch@collabora.co.uk>
 *
 * gstipcpipelinesink.c:
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
/**
 * SECTION:element-ipcpipelinesink
 * @see_also: #GstIpcPipelineSrc, #GstIpcSlavePipeline
 *
 * Communicates with an ipcpipelinesrc element in another process via a socket.
 *
 * This element, together with ipcpipelinesrc and ipcslavepipeline form a
 * mechanism that allows splitting a single pipeline in different processes.
 * The main use-case for it is a playback pipeline split in two parts, where the
 * first part contains the networking, parsing and demuxing and the second part
 * contains the decoding and display. The intention of this split is to improve
 * security of an application, by letting the networking, parsing and demuxing
 * parts run in a less privileged process than the process that accesses the
 * decoder and display.
 *
 * Once the pipelines in those different processes have been created, the
 * playback can be controlled entirely from the first pipeline, which is the
 * one that contains ipcpipelinesink. We call this pipeline the “master”.
 * All relevant events and queries sent from the application are sent to
 * the master pipeline and messages to the application are sent from the master
 * pipeline. The second pipeline, in the other process, is transparently slaved.
 *
 * ipcpipelinesink can work only in push mode and does not synchronize buffers
 * to the clock. Synchronization is meant to happen either at the real sink at
 * the end of the remote slave pipeline, or not to happen at all, if the
 * pipeline is live.
 *
 * A master pipeline may contain more than one ipcpipelinesink elements, which
 * can be connected either to the same slave pipeline or to different ones.
 *
 * Communication with ipcpipelinesrc on the slave happens via a socket, using a
 * custom protocol. Each buffer, event, query, message or state change is
 * serialized in a "packet" and sent over the socket. The sender then
 * performs a blocking wait for a reply, if a return code is needed.
 *
 * All objects that contain a GstStructure (messages, queries, events) are
 * serialized by serializing the GstStructure to a string
 * (gst_structure_to_string). This implies some limitations, of course.
 * All fields of this structures that are not serializable to strings (ex.
 * object pointers) are ignored, except for some cases where custom
 * serialization may occur (ex error/warning/info messages that contain a
 * GError are serialized differently).
 *
 * Buffers are transported by writing their content directly on the socket.
 * More efficient ways for memory sharing could be implemented in the future.
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include "gstipcpipelineelements.h"
#include "gstipcpipelinesink.h"

static GstStaticPadTemplate sinktemplate = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS_ANY);

GST_DEBUG_CATEGORY_STATIC (gst_ipc_pipeline_sink_debug);
#define GST_CAT_DEFAULT gst_ipc_pipeline_sink_debug

enum
{
  SIGNAL_DISCONNECT,
  /* FILL ME */
  LAST_SIGNAL
};
static guint gst_ipc_pipeline_sink_signals[LAST_SIGNAL] = { 0 };

enum
{
  PROP_0,
  PROP_FDIN,
  PROP_FDOUT,
  PROP_READ_CHUNK_SIZE,
  PROP_ACK_TIME,
};


#define DEFAULT_READ_CHUNK_SIZE 4096
#define DEFAULT_ACK_TIME (10 * G_TIME_SPAN_SECOND)

#define _do_init \
    GST_DEBUG_CATEGORY_INIT (gst_ipc_pipeline_sink_debug, "ipcpipelinesink", 0, "ipcpipelinesink element");
#define gst_ipc_pipeline_sink_parent_class parent_class
G_DEFINE_TYPE_WITH_CODE (GstIpcPipelineSink, gst_ipc_pipeline_sink,
    GST_TYPE_ELEMENT, _do_init);
GST_ELEMENT_REGISTER_DEFINE_WITH_CODE (ipcpipelinesink, "ipcpipelinesink",
    GST_RANK_NONE, GST_TYPE_IPC_PIPELINE_SINK,
    icepipeline_element_init (plugin));

static void gst_ipc_pipeline_sink_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_ipc_pipeline_sink_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);
static void gst_ipc_pipeline_sink_dispose (GObject * obj);
static void gst_ipc_pipeline_sink_finalize (GObject * obj);
static gboolean gst_ipc_pipeline_sink_start_reader_thread (GstIpcPipelineSink *
    sink);
static void gst_ipc_pipeline_sink_stop_reader_thread (GstIpcPipelineSink *
    sink);

static GstStateChangeReturn gst_ipc_pipeline_sink_change_state (GstElement *
    element, GstStateChange transition);

static GstFlowReturn gst_ipc_pipeline_sink_chain (GstPad * pad,
    GstObject * parent, GstBuffer * buffer);
static gboolean gst_ipc_pipeline_sink_event (GstPad * pad, GstObject * parent,
    GstEvent * event);
static gboolean gst_ipc_pipeline_sink_element_query (GstElement * element,
    GstQuery * query);
static gboolean gst_ipc_pipeline_sink_send_event (GstElement * element,
    GstEvent * event);
static gboolean gst_ipc_pipeline_sink_query (GstPad * pad, GstObject * parent,
    GstQuery * query);
static gboolean gst_ipc_pipeline_sink_pad_activate_mode (GstPad * pad,
    GstObject * parent, GstPadMode mode, gboolean active);


static void gst_ipc_pipeline_sink_disconnect (GstIpcPipelineSink * sink);
static void pusher (gpointer data, gpointer user_data);


static void
gst_ipc_pipeline_sink_class_init (GstIpcPipelineSinkClass * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *gstelement_class;

  gobject_class = G_OBJECT_CLASS (klass);
  gstelement_class = GST_ELEMENT_CLASS (klass);

  gobject_class->set_property = gst_ipc_pipeline_sink_set_property;
  gobject_class->get_property = gst_ipc_pipeline_sink_get_property;
  gobject_class->dispose = gst_ipc_pipeline_sink_dispose;
  gobject_class->finalize = gst_ipc_pipeline_sink_finalize;

  g_object_class_install_property (gobject_class, PROP_FDIN,
      g_param_spec_int ("fdin", "Input file descriptor",
          "File descriptor to received data from",
          -1, 0xffff, -1, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_FDOUT,
      g_param_spec_int ("fdout", "Output file descriptor",
          "File descriptor to send data through",
          -1, 0xffff, -1, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_READ_CHUNK_SIZE,
      g_param_spec_uint ("read-chunk-size", "Read chunk size",
          "Read chunk size",
          1, 1 << 24, DEFAULT_READ_CHUNK_SIZE,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_ACK_TIME,
      g_param_spec_uint64 ("ack-time", "Ack time",
          "Maximum time to wait for a response to a message",
          0, G_MAXUINT64, DEFAULT_ACK_TIME,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  gst_ipc_pipeline_sink_signals[SIGNAL_DISCONNECT] =
      g_signal_new ("disconnect",
      G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
      G_STRUCT_OFFSET (GstIpcPipelineSinkClass, disconnect),
      NULL, NULL, NULL, G_TYPE_NONE, 0);

  gst_element_class_set_static_metadata (gstelement_class,
      "Inter-process Pipeline Sink",
      "Sink",
      "Allows splitting and continuing a pipeline in another process",
      "Vincent Penquerc'h <vincent.penquerch@collabora.co.uk>");
  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&sinktemplate));

  gstelement_class->change_state =
      GST_DEBUG_FUNCPTR (gst_ipc_pipeline_sink_change_state);
  gstelement_class->query =
      GST_DEBUG_FUNCPTR (gst_ipc_pipeline_sink_element_query);
  gstelement_class->send_event =
      GST_DEBUG_FUNCPTR (gst_ipc_pipeline_sink_send_event);

  klass->disconnect = GST_DEBUG_FUNCPTR (gst_ipc_pipeline_sink_disconnect);
}

static void
gst_ipc_pipeline_sink_init (GstIpcPipelineSink * sink)
{
  GstPadTemplate *pad_template;

  GST_OBJECT_FLAG_SET (sink, GST_ELEMENT_FLAG_SINK);

  gst_ipc_pipeline_comm_init (&sink->comm, GST_ELEMENT (sink));
  sink->comm.read_chunk_size = DEFAULT_READ_CHUNK_SIZE;
  sink->comm.ack_time = DEFAULT_ACK_TIME;
  sink->comm.fdin = -1;
  sink->comm.fdout = -1;
  sink->threads = g_thread_pool_new (pusher, sink, -1, FALSE, NULL);
  gst_ipc_pipeline_sink_start_reader_thread (sink);

  pad_template =
      gst_element_class_get_pad_template (GST_ELEMENT_GET_CLASS (sink), "sink");
  g_return_if_fail (pad_template != NULL);

  sink->sinkpad = gst_pad_new_from_template (pad_template, "sink");

  gst_pad_set_activatemode_function (sink->sinkpad,
      gst_ipc_pipeline_sink_pad_activate_mode);
  gst_pad_set_query_function (sink->sinkpad, gst_ipc_pipeline_sink_query);
  gst_pad_set_event_function (sink->sinkpad, gst_ipc_pipeline_sink_event);
  gst_pad_set_chain_function (sink->sinkpad, gst_ipc_pipeline_sink_chain);
  gst_element_add_pad (GST_ELEMENT_CAST (sink), sink->sinkpad);

}

static void
gst_ipc_pipeline_sink_dispose (GObject * obj)
{
  GstIpcPipelineSink *sink = GST_IPC_PIPELINE_SINK (obj);

  gst_ipc_pipeline_sink_stop_reader_thread (sink);
  gst_ipc_pipeline_comm_cancel (&sink->comm, TRUE);

  G_OBJECT_CLASS (parent_class)->dispose (obj);
}

static void
gst_ipc_pipeline_sink_finalize (GObject * obj)
{
  GstIpcPipelineSink *sink = GST_IPC_PIPELINE_SINK (obj);

  gst_ipc_pipeline_comm_clear (&sink->comm);
  g_thread_pool_free (sink->threads, TRUE, TRUE);

  G_OBJECT_CLASS (parent_class)->finalize (obj);
}

static void
gst_ipc_pipeline_sink_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstIpcPipelineSink *sink = GST_IPC_PIPELINE_SINK (object);

  switch (prop_id) {
    case PROP_FDIN:
      sink->comm.fdin = g_value_get_int (value);
      break;
    case PROP_FDOUT:
      sink->comm.fdout = g_value_get_int (value);
      break;
    case PROP_READ_CHUNK_SIZE:
      sink->comm.read_chunk_size = g_value_get_uint (value);
      break;
    case PROP_ACK_TIME:
      sink->comm.ack_time = g_value_get_uint64 (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_ipc_pipeline_sink_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstIpcPipelineSink *sink = GST_IPC_PIPELINE_SINK (object);

  switch (prop_id) {
    case PROP_FDIN:
      g_value_set_int (value, sink->comm.fdin);
      break;
    case PROP_FDOUT:
      g_value_set_int (value, sink->comm.fdout);
      break;
    case PROP_READ_CHUNK_SIZE:
      g_value_set_uint (value, sink->comm.read_chunk_size);
      break;
    case PROP_ACK_TIME:
      g_value_set_uint64 (value, sink->comm.ack_time);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static gboolean
gst_ipc_pipeline_sink_event (GstPad * pad, GstObject * parent, GstEvent * event)
{
  GstIpcPipelineSink *sink = GST_IPC_PIPELINE_SINK (parent);
  gboolean ret;

  GST_DEBUG_OBJECT (sink, "received event %p of type %s (%d)",
      event, gst_event_type_get_name (event->type), event->type);

  ret = gst_ipc_pipeline_comm_write_event_to_fd (&sink->comm, FALSE, event);
  gst_event_unref (event);
  return ret;
}

static GstFlowReturn
gst_ipc_pipeline_sink_chain (GstPad * pad, GstObject * parent,
    GstBuffer * buffer)
{
  GstIpcPipelineSink *sink = GST_IPC_PIPELINE_SINK (parent);
  GstFlowReturn ret;

  GST_DEBUG_OBJECT (sink, "Rendering buffer %" GST_PTR_FORMAT, buffer);

  ret = gst_ipc_pipeline_comm_write_buffer_to_fd (&sink->comm, buffer);
  if (ret != GST_FLOW_OK)
    GST_DEBUG_OBJECT (sink, "Peer result was %s", gst_flow_get_name (ret));

  gst_buffer_unref (buffer);
  return ret;
}

static gboolean
gst_ipc_pipeline_sink_query (GstPad * pad, GstObject * parent, GstQuery * query)
{
  GstIpcPipelineSink *sink = GST_IPC_PIPELINE_SINK (parent);
  gboolean ret;

  GST_DEBUG_OBJECT (sink, "Got query %s: %" GST_PTR_FORMAT,
      GST_QUERY_TYPE_NAME (query), query);

  switch (GST_QUERY_TYPE (query)) {
    case GST_QUERY_ALLOCATION:
      GST_DEBUG_OBJECT (sink, "Rejecting ALLOCATION query");
      return FALSE;
    case GST_QUERY_CAPS:
    {
      /* caps queries occur even while linking the pipeline.
       * It is possible that the ipcpipelinesrc may not be connected at this
       * point, so let's avoid a couple of errors... */
      GstState state;
      GST_OBJECT_LOCK (sink);
      state = GST_STATE (sink);
      GST_OBJECT_UNLOCK (sink);
      if (state == GST_STATE_NULL)
        return FALSE;
    }
    default:
      break;
  }
  ret = gst_ipc_pipeline_comm_write_query_to_fd (&sink->comm, FALSE, query);

  return ret;
}

static gboolean
gst_ipc_pipeline_sink_element_query (GstElement * element, GstQuery * query)
{
  GstIpcPipelineSink *sink = GST_IPC_PIPELINE_SINK (element);
  gboolean ret;

  GST_DEBUG_OBJECT (sink, "Got element query %s: %" GST_PTR_FORMAT,
      GST_QUERY_TYPE_NAME (query), query);

  ret = gst_ipc_pipeline_comm_write_query_to_fd (&sink->comm, TRUE, query);
  GST_DEBUG_OBJECT (sink, "Got query reply: %d: %" GST_PTR_FORMAT, ret, query);
  return ret;
}

static gboolean
gst_ipc_pipeline_sink_send_event (GstElement * element, GstEvent * event)
{
  GstIpcPipelineSink *sink = GST_IPC_PIPELINE_SINK (element);
  gboolean ret;

  GST_DEBUG_OBJECT (sink, "Got element event %s: %" GST_PTR_FORMAT,
      GST_EVENT_TYPE_NAME (event), event);

  ret = gst_ipc_pipeline_comm_write_event_to_fd (&sink->comm, TRUE, event);
  GST_DEBUG_OBJECT (sink, "Got event reply: %d: %" GST_PTR_FORMAT, ret, event);

  gst_event_unref (event);
  return ret;
}


static gboolean
gst_ipc_pipeline_sink_pad_activate_mode (GstPad * pad,
    GstObject * parent, GstPadMode mode, gboolean active)
{
  if (mode == GST_PAD_MODE_PULL)
    return FALSE;
  return TRUE;
}

static void
on_buffer (guint32 id, GstBuffer * buffer, gpointer user_data)
{
  GstIpcPipelineSink *sink = GST_IPC_PIPELINE_SINK (user_data);
  GST_ERROR_OBJECT (sink,
      "Got buffer id %u! I never knew buffers could go upstream...", id);
  gst_buffer_unref (buffer);
}

static void
pusher (gpointer data, gpointer user_data)
{
  GstIpcPipelineSink *sink = user_data;
  gboolean ret;
  guint32 id;

  id = GPOINTER_TO_INT (gst_mini_object_get_qdata (GST_MINI_OBJECT (data),
          QUARK_ID));

  if (GST_IS_EVENT (data)) {
    GstEvent *event = GST_EVENT (data);
    GST_DEBUG_OBJECT (sink, "Pushing event async: %" GST_PTR_FORMAT, event);
    ret = gst_pad_push_event (sink->sinkpad, event);
    GST_DEBUG_OBJECT (sink, "Event pushed, return %d", ret);
    gst_ipc_pipeline_comm_write_boolean_ack_to_fd (&sink->comm, id, ret);
  } else if (GST_IS_QUERY (data)) {
    GstQuery *query = GST_QUERY (data);
    GST_DEBUG_OBJECT (sink, "Pushing query async: %" GST_PTR_FORMAT, query);
    ret = gst_pad_peer_query (sink->sinkpad, query);
    GST_DEBUG_OBJECT (sink, "Query pushed, return %d", ret);
    gst_ipc_pipeline_comm_write_query_result_to_fd (&sink->comm, id, ret,
        query);
    gst_query_unref (query);
  } else {
    GST_ERROR_OBJECT (sink, "Unsupported object type");
  }
  gst_object_unref (sink);
}

static void
on_event (guint32 id, GstEvent * event, gboolean upstream, gpointer user_data)
{
  GstIpcPipelineSink *sink = GST_IPC_PIPELINE_SINK (user_data);

  if (!upstream) {
    GST_ERROR_OBJECT (sink, "Got downstream event id %u! Not supposed to...",
        id);
    gst_ipc_pipeline_comm_write_boolean_ack_to_fd (&sink->comm, id, FALSE);
    gst_event_unref (event);
    return;
  }

  GST_DEBUG_OBJECT (sink, "Got event id %u: %" GST_PTR_FORMAT, id, event);
  gst_object_ref (sink);
  g_thread_pool_push (sink->threads, event, NULL);
}

static void
on_query (guint32 id, GstQuery * query, gboolean upstream, gpointer user_data)
{
  GstIpcPipelineSink *sink = GST_IPC_PIPELINE_SINK (user_data);

  if (!upstream) {
    GST_ERROR_OBJECT (sink, "Got downstream query id %u! Not supposed to...",
        id);
    gst_ipc_pipeline_comm_write_query_result_to_fd (&sink->comm, id, FALSE,
        query);
    gst_query_unref (query);
    return;
  }

  GST_DEBUG_OBJECT (sink, "Got query id %u: %" GST_PTR_FORMAT, id, query);
  gst_object_ref (sink);
  g_thread_pool_push (sink->threads, query, NULL);
}

static void
on_state_change (guint32 id, GstStateChange transition, gpointer user_data)
{
  GstIpcPipelineSink *sink = GST_IPC_PIPELINE_SINK (user_data);
  GST_ERROR_OBJECT (sink, "Got state change id %u! Not supposed to...", id);
}

static void
on_state_lost (gpointer user_data)
{
  GstIpcPipelineSink *sink = GST_IPC_PIPELINE_SINK (user_data);

  GST_DEBUG_OBJECT (sink, "Got state lost notification, losing state");

  GST_OBJECT_LOCK (sink);
  sink->pass_next_async_done = TRUE;
  GST_OBJECT_UNLOCK (sink);

  gst_element_lost_state (GST_ELEMENT (sink));
}

static void
do_async_done (GstElement * element, gpointer user_data)
{
  GstIpcPipelineSink *sink = GST_IPC_PIPELINE_SINK (element);
  GstMessage *message = user_data;

  GST_STATE_LOCK (sink);
  GST_OBJECT_LOCK (sink);
  if (sink->pass_next_async_done) {
    sink->pass_next_async_done = FALSE;
    GST_OBJECT_UNLOCK (sink);
    gst_element_continue_state (element, GST_STATE_CHANGE_SUCCESS);
    GST_STATE_UNLOCK (sink);
    gst_element_post_message (element, gst_message_ref (message));

  } else {
    GST_OBJECT_UNLOCK (sink);
    GST_STATE_UNLOCK (sink);
  }
}

static void
on_message (guint32 id, GstMessage * message, gpointer user_data)
{
  GstIpcPipelineSink *sink = GST_IPC_PIPELINE_SINK (user_data);

  GST_DEBUG_OBJECT (sink, "Got message id %u: %" GST_PTR_FORMAT, id, message);

  switch (GST_MESSAGE_TYPE (message)) {
    case GST_MESSAGE_ASYNC_DONE:
      GST_OBJECT_LOCK (sink);
      if (sink->pass_next_async_done) {
        GST_OBJECT_UNLOCK (sink);
        gst_element_call_async (GST_ELEMENT (sink), do_async_done,
            message, (GDestroyNotify) gst_message_unref);
      } else {
        GST_OBJECT_UNLOCK (sink);
        gst_message_unref (message);
      }
      return;
    default:
      break;
  }

  gst_element_post_message (GST_ELEMENT (sink), message);
}

static gboolean
gst_ipc_pipeline_sink_start_reader_thread (GstIpcPipelineSink * sink)
{
  if (!gst_ipc_pipeline_comm_start_reader_thread (&sink->comm, on_buffer,
          on_event, on_query, on_state_change, on_state_lost, on_message,
          sink)) {
    GST_ERROR_OBJECT (sink, "Failed to start reader thread");
    return FALSE;
  }
  return TRUE;
}

static void
gst_ipc_pipeline_sink_stop_reader_thread (GstIpcPipelineSink * sink)
{
  gst_ipc_pipeline_comm_stop_reader_thread (&sink->comm);
}


static void
gst_ipc_pipeline_sink_disconnect (GstIpcPipelineSink * sink)
{
  GST_DEBUG_OBJECT (sink, "Disconnecting");
  gst_ipc_pipeline_sink_stop_reader_thread (sink);
  sink->comm.fdin = -1;
  sink->comm.fdout = -1;
  gst_ipc_pipeline_comm_cancel (&sink->comm, FALSE);
  gst_ipc_pipeline_sink_start_reader_thread (sink);
}

static GstStateChangeReturn
gst_ipc_pipeline_sink_change_state (GstElement * element,
    GstStateChange transition)
{
  GstIpcPipelineSink *sink = GST_IPC_PIPELINE_SINK (element);
  GstStateChangeReturn ret = GST_STATE_CHANGE_SUCCESS;
  GstStateChangeReturn peer_ret = GST_STATE_CHANGE_SUCCESS;
  gboolean async = FALSE;
  gboolean down = FALSE;

  GST_DEBUG_OBJECT (sink, "Got state change request: %s -> %s",
      gst_element_state_get_name (GST_STATE_TRANSITION_CURRENT (transition)),
      gst_element_state_get_name (GST_STATE_TRANSITION_NEXT (transition)));

  switch (transition) {
    case GST_STATE_CHANGE_NULL_TO_READY:
      if (sink->comm.fdin < 0) {
        GST_ERROR_OBJECT (element, "Invalid fdin: %d", sink->comm.fdin);
        return GST_STATE_CHANGE_FAILURE;
      }
      if (sink->comm.fdout < 0) {
        GST_ERROR_OBJECT (element, "Invalid fdout: %d", sink->comm.fdout);
        return GST_STATE_CHANGE_FAILURE;
      }
      if (!sink->comm.reader_thread) {
        GST_ERROR_OBJECT (element, "Failed to start reader thread");
        return GST_STATE_CHANGE_FAILURE;
      }
      break;
    case GST_STATE_CHANGE_READY_TO_PAUSED:
    case GST_STATE_CHANGE_PAUSED_TO_PLAYING:
    case GST_STATE_CHANGE_PLAYING_TO_PAUSED:
      /* In these transitions, it is possible that the peer returns ASYNC.
       * We don't know that in advance, but we post async-start anyway because
       * it needs to be delivered *before* async-done, and async-done may
       * arrive at any point in time after we've set the state of the peer.
       * In case the peer doesn't return ASYNC, we just post async-done
       * ourselves and the parent GstBin takes care of matching and deleting
       * them, so the app never gets any of these. */
      async = TRUE;
      break;
    default:
      break;
  }

  /* downwards state change */
  down = (GST_STATE_TRANSITION_CURRENT (transition) >=
      GST_STATE_TRANSITION_NEXT (transition));

  if (async) {
    GST_DEBUG_OBJECT (sink,
        "Posting async-start for %s, will need state-change-done",
        gst_element_state_get_name (GST_STATE_TRANSITION_NEXT (transition)));

    gst_element_post_message (GST_ELEMENT (sink),
        gst_message_new_async_start (GST_OBJECT (sink)));

    GST_OBJECT_LOCK (sink);
    sink->pass_next_async_done = TRUE;
    GST_OBJECT_UNLOCK (sink);
  }

  /* change the state of the peer first */
  /* If the fd out is -1, we do not actually call the peer. This will happen
     when we explicitly disconnected, and in that case we want to be able
     to bring the element down to NULL, so it can be restarted with a new
     slave pipeline. */
  if (sink->comm.fdout >= 0) {
    GST_DEBUG_OBJECT (sink, "Calling peer with state change");
    peer_ret = gst_ipc_pipeline_comm_write_state_change_to_fd (&sink->comm,
        transition);
    if (peer_ret == GST_STATE_CHANGE_FAILURE && down) {
      GST_WARNING_OBJECT (sink, "Peer returned state change failure, "
          "but ignoring because we are going down");
      peer_ret = GST_STATE_CHANGE_SUCCESS;
    }
  } else {
    if (down) {
      GST_WARNING_OBJECT (sink, "Not calling peer (fdout %d)",
          sink->comm.fdout);
      peer_ret = GST_STATE_CHANGE_SUCCESS;
    } else {
      GST_ERROR_OBJECT (sink, "Not calling peer (fdout %d) and failing",
          sink->comm.fdout);
      peer_ret = GST_STATE_CHANGE_FAILURE;
    }
  }

  /* chain up to the parent class to change our state, if the peer succeeded */
  if (peer_ret != GST_STATE_CHANGE_FAILURE) {
    ret = GST_ELEMENT_CLASS (parent_class)->change_state (element, transition);

    if (G_UNLIKELY (ret == GST_STATE_CHANGE_FAILURE && down)) {
      GST_WARNING_OBJECT (sink, "Parent returned state change failure, "
          "but ignoring because we are going down");
      ret = GST_STATE_CHANGE_SUCCESS;
    }
  }

  GST_DEBUG_OBJECT (sink, "For %s -> %s: Peer ret: %s, parent ret: %s",
      gst_element_state_get_name (GST_STATE_TRANSITION_CURRENT (transition)),
      gst_element_state_get_name (GST_STATE_TRANSITION_NEXT (transition)),
      gst_element_state_change_return_get_name (peer_ret),
      gst_element_state_change_return_get_name (ret));

  /* now interpret the return codes */
  if (async && peer_ret != GST_STATE_CHANGE_ASYNC) {
    GST_DEBUG_OBJECT (sink, "Posting async-done for %s; peer wasn't ASYNC",
        gst_element_state_get_name (GST_STATE_TRANSITION_NEXT (transition)));

    GST_OBJECT_LOCK (sink);
    sink->pass_next_async_done = FALSE;
    GST_OBJECT_UNLOCK (sink);

    gst_element_post_message (GST_ELEMENT (sink),
        gst_message_new_async_done (GST_OBJECT (sink), GST_CLOCK_TIME_NONE));
  } else if (G_UNLIKELY (!async && peer_ret == GST_STATE_CHANGE_ASYNC)) {
    GST_WARNING_OBJECT (sink, "Transition not async but peer returned ASYNC");
    peer_ret = GST_STATE_CHANGE_SUCCESS;
  }

  if (peer_ret == GST_STATE_CHANGE_FAILURE || ret == GST_STATE_CHANGE_FAILURE) {
    if (peer_ret != GST_STATE_CHANGE_FAILURE && sink->comm.fdout >= 0) {
      /* only the parent's ret was FAILURE - revert remote changes */
      GST_DEBUG_OBJECT (sink, "Reverting remote state change because parent "
          "returned failure");
      gst_ipc_pipeline_comm_write_state_change_to_fd (&sink->comm,
          GST_STATE_TRANSITION (GST_STATE_TRANSITION_NEXT (transition),
              GST_STATE_TRANSITION_CURRENT (transition)));
    }
    return GST_STATE_CHANGE_FAILURE;
  }

  /* the parent's (GstElement) state change func won't return ASYNC or
   * NO_PREROLL, so unless it has returned FAILURE, which we have caught above,
   * we are not interested in its return code... just return the peer's */
  return peer_ret;
}
