/* GStreamer
 * Copyright (C) 1999,2000 Erik Walthinsen <omega@cse.ogi.edu>
 *                    2000 Wim Taymans <wim@fluendo.com>
 *                    2006 Thomas Vander Stichele <thomas at apestaart dot org>
 *               2015-2017 YouView TV Ltd, Vincent Penquerc'h <vincent.penquerch@collabora.co.uk>
 *
 * gstipcpipelinesrc.c:
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
 * SECTION:element-ipcpipelinesrc
 * @see_also: #GstIpcPipelineSink, #GstIpcSlavePipeline
 *
 * Communicates with an ipcpipelinesink element in another process via a socket.
 *
 * The ipcpipelinesrc element allows 2-way communication with an ipcpipelinesink
 * element on another process/pipeline. It is meant to run inside an
 * interslavepipeline and when paired with an ipcpipelinesink, it will slave its
 * whole parent pipeline to the "master" one, which contains the ipcpipelinesink.
 *
 * For more details about this mechanism and its uses, see the documentation
 * of the ipcpipelinesink element.
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include "gstipcpipelineelements.h"
#include "gstipcpipelinesrc.h"

static GstStaticPadTemplate srctemplate = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS_ANY);

GST_DEBUG_CATEGORY_STATIC (gst_ipc_pipeline_src_debug);
#define GST_CAT_DEFAULT gst_ipc_pipeline_src_debug

enum
{
  /* FILL ME */
  SIGNAL_FORWARD_MESSAGE,
  SIGNAL_DISCONNECT,
  LAST_SIGNAL
};
static guint gst_ipc_pipeline_src_signals[LAST_SIGNAL] = { 0 };

enum
{
  PROP_0,
  PROP_FDIN,
  PROP_FDOUT,
  PROP_READ_CHUNK_SIZE,
  PROP_ACK_TIME,
  PROP_LAST,
};

static GQuark QUARK_UPSTREAM;

#define DEFAULT_READ_CHUNK_SIZE 65536
#define DEFAULT_ACK_TIME (10 * G_TIME_SPAN_SECOND)

#define _do_init \
    GST_DEBUG_CATEGORY_INIT (gst_ipc_pipeline_src_debug, "ipcpipelinesrc", 0, "ipcpipelinesrc element");
#define gst_ipc_pipeline_src_parent_class parent_class
G_DEFINE_TYPE_WITH_CODE (GstIpcPipelineSrc, gst_ipc_pipeline_src,
    GST_TYPE_ELEMENT, _do_init);
GST_ELEMENT_REGISTER_DEFINE_WITH_CODE (ipcpipelinesrc, "ipcpipelinesrc",
    GST_RANK_NONE, GST_TYPE_IPC_PIPELINE_SRC,
    icepipeline_element_init (plugin));

static void gst_ipc_pipeline_src_finalize (GObject * object);
static void gst_ipc_pipeline_src_dispose (GObject * object);
static void gst_ipc_pipeline_src_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_ipc_pipeline_src_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);

static void gst_ipc_pipeline_src_cancel_queued (GstIpcPipelineSrc * src);

static gboolean gst_ipc_pipeline_src_start_reader_thread (GstIpcPipelineSrc *
    src);
static void gst_ipc_pipeline_src_stop_reader_thread (GstIpcPipelineSrc * src);

static gboolean gst_ipc_pipeline_src_activate_mode (GstPad * pad,
    GstObject * parent, GstPadMode mode, gboolean active);
static gboolean gst_ipc_pipeline_src_srcpad_event (GstPad * pad,
    GstObject * parent, GstEvent * event);
static gboolean gst_ipc_pipeline_src_srcpad_query (GstPad * pad,
    GstObject * parent, GstQuery * query);
static void gst_ipc_pipeline_src_loop (GstIpcPipelineSrc * src);

static gboolean gst_ipc_pipeline_src_send_event (GstElement * element,
    GstEvent * event);
static gboolean gst_ipc_pipeline_src_query (GstElement * element,
    GstQuery * query);
static GstStateChangeReturn gst_ipc_pipeline_src_change_state (GstElement *
    element, GstStateChange transition);

static gboolean gst_ipc_pipeline_src_forward_message (GstIpcPipelineSrc * src,
    GstMessage * msg);
static void gst_ipc_pipeline_src_disconnect (GstIpcPipelineSrc * src);

static void
gst_ipc_pipeline_src_class_init (GstIpcPipelineSrcClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GstElementClass *gstelement_class = GST_ELEMENT_CLASS (klass);

  QUARK_UPSTREAM = g_quark_from_static_string ("ipcpipeline-upstream");

  gobject_class->dispose = gst_ipc_pipeline_src_dispose;
  gobject_class->finalize = gst_ipc_pipeline_src_finalize;

  gobject_class->set_property = gst_ipc_pipeline_src_set_property;
  gobject_class->get_property = gst_ipc_pipeline_src_get_property;

  gstelement_class->send_event =
      GST_DEBUG_FUNCPTR (gst_ipc_pipeline_src_send_event);
  gstelement_class->query = GST_DEBUG_FUNCPTR (gst_ipc_pipeline_src_query);
  gstelement_class->change_state =
      GST_DEBUG_FUNCPTR (gst_ipc_pipeline_src_change_state);

  klass->forward_message =
      GST_DEBUG_FUNCPTR (gst_ipc_pipeline_src_forward_message);
  klass->disconnect = GST_DEBUG_FUNCPTR (gst_ipc_pipeline_src_disconnect);

  GST_DEBUG_REGISTER_FUNCPTR (gst_ipc_pipeline_src_activate_mode);
  GST_DEBUG_REGISTER_FUNCPTR (gst_ipc_pipeline_src_srcpad_event);
  GST_DEBUG_REGISTER_FUNCPTR (gst_ipc_pipeline_src_srcpad_query);

  g_object_class_install_property (gobject_class, PROP_FDIN,
      g_param_spec_int ("fdin", "Input file descriptor",
          "File descriptor to read data from",
          -1, 0xffff, -1, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_FDOUT,
      g_param_spec_int ("fdout", "Output file descriptor",
          "File descriptor to write data through",
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

  gst_ipc_pipeline_src_signals[SIGNAL_FORWARD_MESSAGE] =
      g_signal_new ("forward-message", G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
      G_STRUCT_OFFSET (GstIpcPipelineSrcClass, forward_message), NULL, NULL,
      NULL, G_TYPE_BOOLEAN, 1, GST_TYPE_MESSAGE);

  gst_ipc_pipeline_src_signals[SIGNAL_DISCONNECT] =
      g_signal_new ("disconnect", G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
      G_STRUCT_OFFSET (GstIpcPipelineSrcClass, disconnect), NULL, NULL, NULL,
      G_TYPE_NONE, 0);

  gst_element_class_set_static_metadata (gstelement_class,
      "Inter-process Pipeline Source",
      "Source",
      "Continues a split pipeline from another process",
      "Vincent Penquerc'h <vincent.penquerch@collabora.co.uk>");
  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&srctemplate));
}

static void
gst_ipc_pipeline_src_init (GstIpcPipelineSrc * src)
{
  GST_OBJECT_FLAG_SET (src, GST_ELEMENT_FLAG_SOURCE);

  gst_ipc_pipeline_comm_init (&src->comm, GST_ELEMENT (src));
  src->comm.read_chunk_size = DEFAULT_READ_CHUNK_SIZE;
  src->comm.ack_time = DEFAULT_ACK_TIME;
  src->flushing = TRUE;
  src->last_ret = GST_FLOW_FLUSHING;
  src->queued = NULL;
  g_cond_init (&src->create_cond);

  src->srcpad = gst_pad_new_from_static_template (&srctemplate, "src");
  gst_pad_set_activatemode_function (src->srcpad,
      gst_ipc_pipeline_src_activate_mode);
  gst_pad_set_event_function (src->srcpad, gst_ipc_pipeline_src_srcpad_event);
  gst_pad_set_query_function (src->srcpad, gst_ipc_pipeline_src_srcpad_query);
  gst_element_add_pad (GST_ELEMENT (src), src->srcpad);

  gst_ipc_pipeline_src_start_reader_thread (src);
}

static void
gst_ipc_pipeline_src_dispose (GObject * object)
{
  GstIpcPipelineSrc *src = GST_IPC_PIPELINE_SRC (object);

  gst_ipc_pipeline_src_stop_reader_thread (src);
  gst_ipc_pipeline_src_cancel_queued (src);
  gst_ipc_pipeline_comm_cancel (&src->comm, TRUE);

  G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
gst_ipc_pipeline_src_finalize (GObject * object)
{
  GstIpcPipelineSrc *src = GST_IPC_PIPELINE_SRC (object);

  gst_ipc_pipeline_comm_clear (&src->comm);
  g_cond_clear (&src->create_cond);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
gst_ipc_pipeline_src_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstIpcPipelineSrc *src = GST_IPC_PIPELINE_SRC (object);

  switch (prop_id) {
    case PROP_FDIN:
      src->comm.fdin = g_value_get_int (value);
      break;
    case PROP_FDOUT:
      src->comm.fdout = g_value_get_int (value);
      break;
    case PROP_READ_CHUNK_SIZE:
      src->comm.read_chunk_size = g_value_get_uint (value);
      break;
    case PROP_ACK_TIME:
      src->comm.ack_time = g_value_get_uint64 (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_ipc_pipeline_src_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstIpcPipelineSrc *src = GST_IPC_PIPELINE_SRC (object);

  g_return_if_fail (GST_IS_IPC_PIPELINE_SRC (object));

  switch (prop_id) {
    case PROP_FDIN:
      g_value_set_int (value, src->comm.fdin);
      break;
    case PROP_FDOUT:
      g_value_set_int (value, src->comm.fdout);
      break;
    case PROP_READ_CHUNK_SIZE:
      g_value_set_uint (value, src->comm.read_chunk_size);
      break;
    case PROP_ACK_TIME:
      g_value_set_uint64 (value, src->comm.ack_time);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_ipc_pipeline_src_log_queue (GstIpcPipelineSrc * src)
{
  GList *queued;
  guint n;

  queued = src->queued;
  n = 0;
  GST_LOG_OBJECT (src, "There are %u objects in the queue",
      g_list_length (queued));
  while (queued) {
    void *object = queued->data;
    if (GST_IS_EVENT (object)) {
      GST_LOG_OBJECT (src, "  #%u: %s event", n, GST_EVENT_TYPE_NAME (object));
    } else if (GST_IS_QUERY (object)) {
      GST_LOG_OBJECT (src, "  #%u: %s query", n, GST_QUERY_TYPE_NAME (object));
    } else if (GST_IS_BUFFER (object)) {
      GST_LOG_OBJECT (src, "  #%u: %" G_GSIZE_FORMAT " bytes buffer", n,
          gst_buffer_get_size (object));
    } else {
      GST_LOG_OBJECT (src, "  #%u: unknown item in queue", n);
    }
    queued = queued->next;
    ++n;
  }
}

static void
gst_ipc_pipeline_src_cancel_queued (GstIpcPipelineSrc * src)
{
  GList *queued;
  guint32 id;

  g_mutex_lock (&src->comm.mutex);
  queued = src->queued;
  src->queued = NULL;
  g_cond_broadcast (&src->create_cond);
  g_mutex_unlock (&src->comm.mutex);

  while (queued) {
    void *object = queued->data;

    id = GPOINTER_TO_INT (gst_mini_object_get_qdata (GST_MINI_OBJECT (object),
            QUARK_ID));

    queued = g_list_delete_link (queued, queued);
    if (GST_IS_EVENT (object)) {
      GstEvent *event = GST_EVENT (object);
      GST_DEBUG_OBJECT (src, "Cancelling queued event: %" GST_PTR_FORMAT,
          event);
      gst_ipc_pipeline_comm_write_boolean_ack_to_fd (&src->comm, id, FALSE);
      gst_event_unref (event);
    } else if (GST_IS_BUFFER (object)) {
      GstBuffer *buffer = GST_BUFFER (object);
      GST_DEBUG_OBJECT (src, "Cancelling queued buffer: %" GST_PTR_FORMAT,
          buffer);
      gst_ipc_pipeline_comm_write_flow_ack_to_fd (&src->comm, id,
          GST_FLOW_FLUSHING);
      gst_buffer_unref (buffer);
    } else if (GST_IS_QUERY (object)) {
      GstQuery *query = GST_QUERY (object);
      GST_DEBUG_OBJECT (src, "Cancelling queued query: %" GST_PTR_FORMAT,
          query);
      gst_ipc_pipeline_comm_write_query_result_to_fd (&src->comm, id, FALSE,
          query);
      gst_query_unref (query);
    }
  }

}

static void
gst_ipc_pipeline_src_start_loop (GstIpcPipelineSrc * src)
{
  g_mutex_lock (&src->comm.mutex);
  src->flushing = FALSE;
  src->last_ret = GST_FLOW_OK;
  g_mutex_unlock (&src->comm.mutex);

  gst_pad_start_task (src->srcpad, (GstTaskFunction) gst_ipc_pipeline_src_loop,
      src, NULL);
}

static void
gst_ipc_pipeline_src_stop_loop (GstIpcPipelineSrc * src, gboolean stop)
{
  g_mutex_lock (&src->comm.mutex);
  src->flushing = TRUE;
  g_cond_broadcast (&src->create_cond);
  g_mutex_unlock (&src->comm.mutex);

  if (stop)
    gst_pad_stop_task (src->srcpad);
}

static gboolean
gst_ipc_pipeline_src_activate_mode (GstPad * pad, GstObject * parent,
    GstPadMode mode, gboolean active)
{
  GstIpcPipelineSrc *src = GST_IPC_PIPELINE_SRC (parent);

  switch (mode) {
    case GST_PAD_MODE_PUSH:
      GST_DEBUG_OBJECT (pad, "%s in push mode", active ? "activating" :
          "deactivating");
      if (active) {
        gst_ipc_pipeline_src_start_loop (src);
      } else {
        gst_ipc_pipeline_src_stop_loop (src, TRUE);
        gst_ipc_pipeline_comm_cancel (&src->comm, FALSE);
      }
      return TRUE;
    default:
      GST_DEBUG_OBJECT (pad, "unsupported activation mode");
      return FALSE;
  }
}

static gboolean
gst_ipc_pipeline_src_srcpad_event (GstPad * pad, GstObject * parent,
    GstEvent * event)
{
  GstIpcPipelineSrc *src = GST_IPC_PIPELINE_SRC (parent);
  gboolean ret;

  GST_DEBUG_OBJECT (src, "Got upstream event %s", GST_EVENT_TYPE_NAME (event));

  ret = gst_ipc_pipeline_comm_write_event_to_fd (&src->comm, TRUE, event);
  gst_event_unref (event);

  GST_DEBUG_OBJECT (src, "Returning event result: %d", ret);
  return ret;
}

static gboolean
gst_ipc_pipeline_src_srcpad_query (GstPad * pad, GstObject * parent,
    GstQuery * query)
{
  GstIpcPipelineSrc *src = GST_IPC_PIPELINE_SRC (parent);
  gboolean ret;

  /* answer some queries that do not make sense to be forwarded */
  switch (GST_QUERY_TYPE (query)) {
    case GST_QUERY_LATENCY:
      return TRUE;
    case GST_QUERY_CONTEXT:
      return FALSE;
    case GST_QUERY_CAPS:
    {
      /* caps queries occur even while linking the pipeline.
       * It is possible that the ipcpipelinesink may not be connected at this
       * point, so let's avoid a couple of errors... */
      GstState state;
      GST_OBJECT_LOCK (src);
      state = GST_STATE (src);
      GST_OBJECT_UNLOCK (src);
      if (state == GST_STATE_NULL)
        return FALSE;
    }
    default:
      break;
  }

  GST_DEBUG_OBJECT (src, "Got upstream query %s: %" GST_PTR_FORMAT,
      GST_QUERY_TYPE_NAME (query), query);

  ret = gst_ipc_pipeline_comm_write_query_to_fd (&src->comm, TRUE, query);

  GST_DEBUG_OBJECT (src, "Returning query result: %d, %" GST_PTR_FORMAT,
      ret, query);
  return ret;
}

static void
gst_ipc_pipeline_src_loop (GstIpcPipelineSrc * src)
{
  gpointer object;
  guint32 id;
  gboolean ok;
  GstFlowReturn ret = GST_FLOW_OK;

  g_mutex_lock (&src->comm.mutex);

  while (!src->queued && !src->flushing)
    g_cond_wait (&src->create_cond, &src->comm.mutex);

  if (src->flushing)
    goto out;

  object = src->queued->data;
  src->queued = g_list_delete_link (src->queued, src->queued);
  g_mutex_unlock (&src->comm.mutex);

  id = GPOINTER_TO_INT (gst_mini_object_get_qdata (GST_MINI_OBJECT (object),
          QUARK_ID));

  if (GST_IS_BUFFER (object)) {
    GstBuffer *buf = GST_BUFFER (object);
    GST_DEBUG_OBJECT (src, "Pushing queued buffer: %" GST_PTR_FORMAT, buf);
    ret = gst_pad_push (src->srcpad, buf);
    GST_DEBUG_OBJECT (src, "pushed id %u, ret: %s", id,
        gst_flow_get_name (ret));
    gst_ipc_pipeline_comm_write_flow_ack_to_fd (&src->comm, id, ret);
  } else if (GST_IS_EVENT (object)) {
    GstEvent *event = GST_EVENT (object);
    GST_DEBUG_OBJECT (src, "Pushing queued event: %" GST_PTR_FORMAT, event);
    ok = gst_pad_push_event (src->srcpad, event);
    gst_ipc_pipeline_comm_write_boolean_ack_to_fd (&src->comm, id, ok);
  } else if (GST_IS_QUERY (object)) {
    GstQuery *query = GST_QUERY (object);
    GST_DEBUG_OBJECT (src, "Pushing queued query: %" GST_PTR_FORMAT, query);
    ok = gst_pad_peer_query (src->srcpad, query);
    gst_ipc_pipeline_comm_write_query_result_to_fd (&src->comm, id, ok, query);
    gst_query_unref (query);
  } else {
    GST_WARNING_OBJECT (src, "Unknown data type queued");
  }

  g_mutex_lock (&src->comm.mutex);
  if (!src->queued)
    g_cond_broadcast (&src->create_cond);
out:
  if (src->flushing)
    ret = GST_FLOW_FLUSHING;
  if (ret != GST_FLOW_OK)
    src->last_ret = ret;
  g_mutex_unlock (&src->comm.mutex);

  if (ret == GST_FLOW_FLUSHING) {
    gst_ipc_pipeline_src_cancel_queued (src);
    gst_pad_pause_task (src->srcpad);
  }
}

static gboolean
gst_ipc_pipeline_src_send_event (GstElement * element, GstEvent * event)
{
  GstIpcPipelineSrc *src = GST_IPC_PIPELINE_SRC (element);
  return gst_pad_push_event (src->srcpad, event);
}

static gboolean
gst_ipc_pipeline_src_query (GstElement * element, GstQuery * query)
{
  GstIpcPipelineSrc *src = GST_IPC_PIPELINE_SRC (element);
  return gst_pad_query (src->srcpad, query);
}

static GstElement *
find_pipeline (GstElement * element)
{
  GstElement *pipeline = element;
  while (GST_ELEMENT_PARENT (pipeline)) {
    pipeline = GST_ELEMENT_PARENT (pipeline);
    if (GST_IS_PIPELINE (pipeline))
      break;
  }
  if (!pipeline || !GST_IS_PIPELINE (pipeline)) {
    pipeline = NULL;
  }
  return pipeline;
}

static gboolean
gst_ipc_pipeline_src_forward_message (GstIpcPipelineSrc * src, GstMessage * msg)
{
  gboolean skip = FALSE;

  GST_DEBUG_OBJECT (src, "Message to forward: %" GST_PTR_FORMAT, msg);

  switch (GST_MESSAGE_TYPE (msg)) {
    case GST_MESSAGE_STATE_CHANGED:
    {
      GstState old, new, pending;
      GstElement *pipeline = find_pipeline (GST_ELEMENT (src));

      gst_message_parse_state_changed (msg, &old, &new, &pending);

      if (GST_MESSAGE_SRC (msg) == GST_OBJECT (pipeline) &&
          old == new && new == pending) {
        GST_DEBUG_OBJECT (src, "Detected lost state, notifying master");
        gst_ipc_pipeline_comm_write_state_lost_to_fd (&src->comm);
      }
      /* fall through & skip */
    }
    case GST_MESSAGE_ASYNC_START:
    case GST_MESSAGE_CLOCK_PROVIDE:
    case GST_MESSAGE_CLOCK_LOST:
    case GST_MESSAGE_NEW_CLOCK:
    case GST_MESSAGE_STREAM_STATUS:
    case GST_MESSAGE_NEED_CONTEXT:
    case GST_MESSAGE_HAVE_CONTEXT:
    case GST_MESSAGE_STRUCTURE_CHANGE:
      skip = TRUE;
      break;
    case GST_MESSAGE_RESET_TIME:
    {
      GQuark ipcpipelinesrc_posted = g_quark_from_static_string
          ("gstinterslavepipeline-message-already-posted");

      skip = GPOINTER_TO_INT (gst_mini_object_get_qdata (GST_MINI_OBJECT (msg),
              ipcpipelinesrc_posted));
      if (!skip) {
        gst_mini_object_set_qdata (GST_MINI_OBJECT (msg), ipcpipelinesrc_posted,
            GUINT_TO_POINTER (1), NULL);
      }
      break;
    }
    case GST_MESSAGE_ERROR:
    {
      GError *error = NULL;

      /* skip forwarding a RESOURCE/WRITE error message that originated from
       * ipcpipelinesrc; we post this error when writing to the comm fd fails,
       * so if we try to forward it here, we will likely get another one posted
       * immediately and end up doing an endless loop */
      gst_message_parse_error (msg, &error, NULL);
      skip = (GST_MESSAGE_SRC (msg) == GST_OBJECT_CAST (src)
          && error->domain == gst_resource_error_quark ()
          && error->code == GST_RESOURCE_ERROR_WRITE);
      g_error_free (error);
      break;
    }
    default:
      break;
  }

  if (skip) {
    GST_DEBUG_OBJECT (src, "message will not be forwarded");
    return TRUE;
  }

  return gst_ipc_pipeline_comm_write_message_to_fd (&src->comm, msg);
}

static void
on_buffer (guint32 id, GstBuffer * buffer, gpointer user_data)
{
  GstIpcPipelineSrc *src = GST_IPC_PIPELINE_SRC (user_data);
  GST_DEBUG_OBJECT (src, "Got buffer id %u, queueing: %" GST_PTR_FORMAT, id,
      buffer);
  g_mutex_lock (&src->comm.mutex);
  if (!GST_PAD_IS_ACTIVE (src->srcpad) || src->flushing) {
    g_mutex_unlock (&src->comm.mutex);
    GST_INFO_OBJECT (src, "We're not started or flushing, buffer ignored");
    gst_ipc_pipeline_comm_write_flow_ack_to_fd (&src->comm, id,
        GST_FLOW_FLUSHING);
    gst_buffer_unref (buffer);
    return;
  }
  if (src->last_ret != GST_FLOW_OK) {
    GstFlowReturn last_ret = src->last_ret;
    g_mutex_unlock (&src->comm.mutex);
    GST_DEBUG_OBJECT (src, "Last flow was %s, rejecting buffer",
        gst_flow_get_name (last_ret));
    gst_ipc_pipeline_comm_write_flow_ack_to_fd (&src->comm, id, last_ret);
    gst_buffer_unref (buffer);
    return;
  }
  src->queued = g_list_append (src->queued, buffer);    /* keep the ref */
  gst_ipc_pipeline_src_log_queue (src);
  g_cond_broadcast (&src->create_cond);
  g_mutex_unlock (&src->comm.mutex);
}

static void
do_oob_event (GstElement * element, gpointer user_data)
{
  GstIpcPipelineSrc *src = GST_IPC_PIPELINE_SRC (element);
  GstEvent *event = user_data;
  gboolean ret, upstream;
  guint32 id;

  id = GPOINTER_TO_INT (gst_mini_object_get_qdata (GST_MINI_OBJECT
          (event), QUARK_ID));
  upstream = GPOINTER_TO_INT (gst_mini_object_get_qdata (GST_MINI_OBJECT
          (event), QUARK_UPSTREAM));

  if (upstream) {
    GstElement *pipeline;
    gboolean ok = FALSE;

    if (!(pipeline = find_pipeline (element))) {
      GST_ERROR_OBJECT (src, "No pipeline found");
      gst_ipc_pipeline_comm_write_boolean_ack_to_fd (&src->comm, id, ok);
    } else {
      GST_DEBUG_OBJECT (src, "Posting upstream event on pipeline: %"
          GST_PTR_FORMAT, event);
      ok = gst_element_send_event (pipeline, gst_event_ref (event));
      gst_ipc_pipeline_comm_write_boolean_ack_to_fd (&src->comm, id, ok);
    }
  } else {
    GST_DEBUG_OBJECT (src, "Pushing event async: %" GST_PTR_FORMAT, event);
    ret = gst_element_send_event (element, gst_event_ref (event));
    GST_DEBUG_OBJECT (src, "Event pushed, return %d", ret);
    gst_ipc_pipeline_comm_write_boolean_ack_to_fd (&src->comm, id, ret);
  }
}

static void
on_event (guint32 id, GstEvent * event, gboolean upstream, gpointer user_data)
{
  GstIpcPipelineSrc *src = GST_IPC_PIPELINE_SRC (user_data);
  GstFlowReturn last_ret = GST_FLOW_OK;

  GST_DEBUG_OBJECT (src, "Got event id %u, queueing: %" GST_PTR_FORMAT, id,
      event);

  gst_mini_object_set_qdata (GST_MINI_OBJECT (event), QUARK_UPSTREAM,
      GINT_TO_POINTER (upstream), NULL);

  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_FLUSH_START:
      gst_ipc_pipeline_src_stop_loop (src, FALSE);
      break;
    case GST_EVENT_FLUSH_STOP:
      gst_ipc_pipeline_src_start_loop (src);
      break;
    default:
      g_mutex_lock (&src->comm.mutex);
      last_ret = src->last_ret;
      g_mutex_unlock (&src->comm.mutex);
      break;
  }

  if (GST_EVENT_IS_SERIALIZED (event) && !upstream) {
    if (last_ret != GST_FLOW_OK) {
      GST_DEBUG_OBJECT (src, "Last flow was %s, rejecting event",
          gst_flow_get_name (last_ret));
      gst_event_unref (event);
      gst_ipc_pipeline_comm_write_boolean_ack_to_fd (&src->comm, id, FALSE);
    } else {
      GST_DEBUG_OBJECT (src, "This is a serialized event, adding to queue %p",
          src->queued);
      g_mutex_lock (&src->comm.mutex);
      src->queued = g_list_append (src->queued, event); /* keep the ref */
      gst_ipc_pipeline_src_log_queue (src);
      g_cond_broadcast (&src->create_cond);
      g_mutex_unlock (&src->comm.mutex);
    }
  } else {
    if (last_ret != GST_FLOW_OK && !upstream) {
      GST_DEBUG_OBJECT (src, "Last flow was %s, rejecting event",
          gst_flow_get_name (last_ret));
      gst_ipc_pipeline_comm_write_boolean_ack_to_fd (&src->comm, id, FALSE);
      gst_event_unref (event);
    } else {
      GST_DEBUG_OBJECT (src,
          "This is not a serialized event, pushing in a thread");
      gst_element_call_async (GST_ELEMENT (src), do_oob_event, event,
          (GDestroyNotify) gst_event_unref);
    }
  }
}

static void
do_oob_query (GstElement * element, gpointer user_data)
{
  GstIpcPipelineSrc *src = GST_IPC_PIPELINE_SRC (element);
  GstQuery *query = GST_QUERY (user_data);
  guint32 id;
  gboolean upstream;
  gboolean ret;

  id = GPOINTER_TO_INT (gst_mini_object_get_qdata (GST_MINI_OBJECT
          (query), QUARK_ID));
  upstream = GPOINTER_TO_INT (gst_mini_object_get_qdata (GST_MINI_OBJECT
          (query), QUARK_UPSTREAM));

  if (upstream) {
    GstElement *pipeline;

    if (!(pipeline = find_pipeline (element))) {
      GST_ERROR_OBJECT (src, "No pipeline found");
      ret = FALSE;
    } else {
      GST_DEBUG_OBJECT (src, "Posting query on pipeline: %" GST_PTR_FORMAT,
          query);
      ret = gst_element_query (pipeline, query);
    }
  } else {
    GST_DEBUG_OBJECT (src, "Pushing query async: %" GST_PTR_FORMAT, query);
    ret = gst_pad_peer_query (src->srcpad, query);
    GST_DEBUG_OBJECT (src, "Query pushed, return %d", ret);
  }
  gst_ipc_pipeline_comm_write_query_result_to_fd (&src->comm, id, ret, query);
}

static void
on_query (guint32 id, GstQuery * query, gboolean upstream, gpointer user_data)
{
  GstIpcPipelineSrc *src = GST_IPC_PIPELINE_SRC (user_data);

  GST_DEBUG_OBJECT (src, "Got query id %u, queueing: %" GST_PTR_FORMAT, id,
      query);

  if (GST_QUERY_IS_SERIALIZED (query) && !upstream) {
    g_mutex_lock (&src->comm.mutex);
    src->queued = g_list_append (src->queued, query);   /* keep the ref */
    gst_ipc_pipeline_src_log_queue (src);
    g_cond_broadcast (&src->create_cond);
    g_mutex_unlock (&src->comm.mutex);
  } else {
    gst_mini_object_set_qdata (GST_MINI_OBJECT (query), QUARK_UPSTREAM,
        GINT_TO_POINTER (upstream), NULL);
    gst_element_call_async (GST_ELEMENT (src), do_oob_query, query,
        (GDestroyNotify) gst_query_unref);
  }
}

struct StateChangeData
{
  guint32 id;
  GstStateChange transition;
};

static void
do_state_change (GstElement * element, gpointer data)
{
  GstIpcPipelineSrc *src = GST_IPC_PIPELINE_SRC (element);
  GstElement *pipeline;
  GstStateChangeReturn ret = GST_STATE_CHANGE_FAILURE;
  GstState state, pending, effective;
  struct StateChangeData *d = data;
  guint32 id = d->id;
  GstStateChange transition = d->transition;
  gboolean down;

  GST_DEBUG_OBJECT (src, "Doing state change id %u, %s -> %s", id,
      gst_element_state_get_name (GST_STATE_TRANSITION_CURRENT (transition)),
      gst_element_state_get_name (GST_STATE_TRANSITION_NEXT (transition)));

  if (!(pipeline = find_pipeline (element))) {
    GST_ERROR_OBJECT (src, "No pipeline found");
    ret = GST_STATE_CHANGE_FAILURE;
    goto done_nolock;
  }

  down = (GST_STATE_TRANSITION_CURRENT (transition) >=
      GST_STATE_TRANSITION_NEXT (transition));

  GST_STATE_LOCK (pipeline);
  ret = gst_element_get_state (pipeline, &state, &pending, 0);

  /* if we are pending a state change, count the pending state as
   * the current one */
  effective = pending == GST_STATE_VOID_PENDING ? state : pending;

  GST_DEBUG_OBJECT (src, "Current element state: ret:%s state:%s pending:%s "
      "effective:%s", gst_element_state_change_return_get_name (ret),
      gst_element_state_get_name (state),
      gst_element_state_get_name (pending),
      gst_element_state_get_name (effective));

  if ((GST_STATE_TRANSITION_NEXT (transition) <= effective && !down) ||
      (GST_STATE_TRANSITION_NEXT (transition) > effective && down)) {
    /* if the request was to transition to a state that we have already
     * transitioned to in the same direction, then we just silently return */
    GST_DEBUG_OBJECT (src, "State transition to %s is unnecessary",
        gst_element_state_get_name (GST_STATE_TRANSITION_NEXT (transition)));
    /* make sure we return SUCCESS if the transition is to NULL or READY,
     * even if our current ret is ASYNC for example; also, make sure not
     * to return FAILURE, since our state is already committed */
    if (GST_STATE_TRANSITION_NEXT (transition) <= GST_STATE_READY ||
        ret == GST_STATE_CHANGE_FAILURE) {
      ret = GST_STATE_CHANGE_SUCCESS;
    }
  } else if (ret != GST_STATE_CHANGE_FAILURE || down) {
    /* if the request was to transition to a state that we haven't already
     * transitioned to in the same direction, then we need to request a state
     * change in the pipeline, *unless* we are going upwards and the last ret
     * was FAILURE, in which case we should just return FAILURE and stop.
     * We don't stop a downwards state change though in case of FAILURE, since
     * we need to be able to bring the pipeline down to NULL. Note that
     * GST_MESSAGE_ERROR will cause ret to be GST_STATE_CHANGE_FAILURE */
    ret = gst_element_set_state (pipeline,
        GST_STATE_TRANSITION_NEXT (transition));
    GST_DEBUG_OBJECT (src, "gst_element_set_state returned %s",
        gst_element_state_change_return_get_name (ret));
  }

  GST_STATE_UNLOCK (pipeline);

done_nolock:
  GST_DEBUG_OBJECT (src, "sending state change ack, ret = %s",
      gst_element_state_change_return_get_name (ret));
  gst_ipc_pipeline_comm_write_state_change_ack_to_fd (&src->comm, id, ret);
}

static void
on_state_change (guint32 id, GstStateChange transition, gpointer user_data)
{
  struct StateChangeData *d;
  GstElement *ipcpipelinesrc = GST_ELEMENT (user_data);

  GST_DEBUG_OBJECT (ipcpipelinesrc, "Got state change id %u, %s -> %s", id,
      gst_element_state_get_name (GST_STATE_TRANSITION_CURRENT (transition)),
      gst_element_state_get_name (GST_STATE_TRANSITION_NEXT (transition)));

  d = g_new (struct StateChangeData, 1);
  d->id = id;
  d->transition = transition;

  gst_element_call_async (ipcpipelinesrc, do_state_change, d, g_free);
}

static void
on_message (guint32 id, GstMessage * message, gpointer user_data)
{
  GstIpcPipelineSrc *src = GST_IPC_PIPELINE_SRC (user_data);

  GST_ERROR_OBJECT (src, "Got message id %u, not supposed to: %" GST_PTR_FORMAT,
      id, message);
  gst_message_unref (message);
}

static gboolean
gst_ipc_pipeline_src_start_reader_thread (GstIpcPipelineSrc * src)
{
  if (!gst_ipc_pipeline_comm_start_reader_thread (&src->comm, on_buffer,
          on_event, on_query, on_state_change, NULL, on_message, src)) {
    GST_ERROR_OBJECT (src, "Failed to start reader thread");
    return FALSE;
  }
  return TRUE;
}

static void
gst_ipc_pipeline_src_stop_reader_thread (GstIpcPipelineSrc * src)
{
  gst_ipc_pipeline_comm_stop_reader_thread (&src->comm);
}

static void
gst_ipc_pipeline_src_disconnect (GstIpcPipelineSrc * src)
{
  GST_DEBUG_OBJECT (src, "Disconnecting");
  gst_ipc_pipeline_src_stop_reader_thread (src);
  src->comm.fdin = -1;
  src->comm.fdout = -1;
  gst_ipc_pipeline_comm_cancel (&src->comm, FALSE);
  gst_ipc_pipeline_src_start_reader_thread (src);
}

static GstStateChangeReturn
gst_ipc_pipeline_src_change_state (GstElement * element,
    GstStateChange transition)
{
  GstIpcPipelineSrc *src = GST_IPC_PIPELINE_SRC (element);

  switch (transition) {
    case GST_STATE_CHANGE_NULL_TO_READY:
      if (src->comm.fdin < 0) {
        GST_ERROR_OBJECT (element, "Invalid fdin: %d", src->comm.fdin);
        return GST_STATE_CHANGE_FAILURE;
      }
      if (src->comm.fdout < 0) {
        GST_ERROR_OBJECT (element, "Invalid fdout: %d", src->comm.fdout);
        return GST_STATE_CHANGE_FAILURE;
      }
      if (!src->comm.reader_thread) {
        GST_ERROR_OBJECT (element, "Failed to start reader thread");
        return GST_STATE_CHANGE_FAILURE;
      }
      break;
    default:
      break;
  }
  return GST_ELEMENT_CLASS (parent_class)->change_state (element, transition);
}
