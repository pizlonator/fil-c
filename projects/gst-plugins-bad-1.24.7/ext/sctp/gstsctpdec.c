/*
 * Copyright (c) 2015, Collabora Ltd.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 * list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice, this
 * list of conditions and the following disclaimer in the documentation and/or other
 * materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY
 * OF SUCH DAMAGE.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include "gstsctpdec.h"

#include <gst/sctp/sctpreceivemeta.h>
#include <gst/base/gstdataqueue.h>

#include <stdio.h>
#include <stdlib.h>

GST_DEBUG_CATEGORY_STATIC (gst_sctp_dec_debug_category);
#define GST_CAT_DEFAULT gst_sctp_dec_debug_category

#define gst_sctp_dec_parent_class parent_class
G_DEFINE_TYPE (GstSctpDec, gst_sctp_dec, GST_TYPE_ELEMENT);
GST_ELEMENT_REGISTER_DEFINE (sctpdec, "sctpdec", GST_RANK_NONE,
    GST_TYPE_SCTP_DEC);

static GstStaticPadTemplate sink_template =
GST_STATIC_PAD_TEMPLATE ("sink", GST_PAD_SINK,
    GST_PAD_ALWAYS, GST_STATIC_CAPS ("application/x-sctp"));

static GstStaticPadTemplate src_template =
GST_STATIC_PAD_TEMPLATE ("src_%u", GST_PAD_SRC,
    GST_PAD_SOMETIMES, GST_STATIC_CAPS_ANY);

enum
{
  SIGNAL_RESET_STREAM,
  NUM_SIGNALS
};

static guint signals[NUM_SIGNALS];

enum
{
  PROP_0,

  PROP_GST_SCTP_ASSOCIATION_ID,
  PROP_LOCAL_SCTP_PORT,

  NUM_PROPERTIES
};

static GParamSpec *properties[NUM_PROPERTIES];

#define DEFAULT_GST_SCTP_ASSOCIATION_ID 1
#define DEFAULT_LOCAL_SCTP_PORT 0
#define MAX_SCTP_PORT 65535
#define MAX_GST_SCTP_ASSOCIATION_ID 65535
#define MAX_STREAM_ID 65535

GType gst_sctp_dec_pad_get_type (void);

#define GST_TYPE_SCTP_DEC_PAD (gst_sctp_dec_pad_get_type())
#define GST_SCTP_DEC_PAD(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), GST_TYPE_SCTP_DEC_PAD, GstSctpDecPad))
#define GST_SCTP_DEC_PAD_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST((klass), GST_TYPE_SCTP_DEC_PAD, GstSctpDecPadClass))
#define GST_IS_SCTP_DEC_PAD(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), GST_TYPE_SCTP_DEC_PAD))
#define GST_IS_SCTP_DEC_PAD_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), GST_TYPE_SCTP_DEC_PAD))

typedef struct _GstSctpDecPad GstSctpDecPad;
typedef GstPadClass GstSctpDecPadClass;

struct _GstSctpDecPad
{
  GstPad parent;

  GstDataQueue *packet_queue;
};

G_DEFINE_TYPE (GstSctpDecPad, gst_sctp_dec_pad, GST_TYPE_PAD);

static void
gst_sctp_dec_pad_finalize (GObject * object)
{
  GstSctpDecPad *self = GST_SCTP_DEC_PAD (object);

  gst_object_unref (self->packet_queue);

  G_OBJECT_CLASS (gst_sctp_dec_pad_parent_class)->finalize (object);
}

static gboolean
data_queue_check_full_cb (GstDataQueue * queue, guint visible, guint bytes,
    guint64 time, gpointer user_data)
{
  /* FIXME: Are we full at some point and block? */
  return FALSE;
}

static void
data_queue_empty_cb (GstDataQueue * queue, gpointer user_data)
{
}

static void
data_queue_full_cb (GstDataQueue * queue, gpointer user_data)
{
}

static void
gst_sctp_dec_pad_class_init (GstSctpDecPadClass * klass)
{
  GObjectClass *gobject_class;

  gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->finalize = gst_sctp_dec_pad_finalize;
}

static void
gst_sctp_dec_pad_init (GstSctpDecPad * self)
{
  self->packet_queue = gst_data_queue_new (data_queue_check_full_cb,
      data_queue_full_cb, data_queue_empty_cb, NULL);
}

static void gst_sctp_dec_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_sctp_dec_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);
static void gst_sctp_dec_finalize (GObject * object);
static GstStateChangeReturn gst_sctp_dec_change_state (GstElement * element,
    GstStateChange transition);
static GstFlowReturn gst_sctp_dec_packet_chain (GstPad * pad, GstSctpDec * self,
    GstBuffer * buf);
static gboolean gst_sctp_dec_packet_event (GstPad * pad, GstSctpDec * self,
    GstEvent * event);
static void gst_sctp_data_srcpad_loop (GstPad * pad);

static gboolean configure_association (GstSctpDec * self);
static void on_gst_sctp_association_stream_reset (GstSctpAssociation *
    gst_sctp_association, guint16 stream_id, GstSctpDec * self);
static void on_receive (GstSctpAssociation * gst_sctp_association,
    guint8 * buf, gsize length, guint16 stream_id, guint ppid,
    gpointer user_data);
static void stop_srcpad_task (GstPad * pad);
static void stop_all_srcpad_tasks (GstSctpDec * self);
static void sctpdec_cleanup (GstSctpDec * self);
static GstPad *get_pad_for_stream_id (GstSctpDec * self, guint16 stream_id);
static void remove_pad (GstSctpDec * self, GstPad * pad);
static void on_reset_stream (GstSctpDec * self, guint stream_id);

static void
gst_sctp_dec_class_init (GstSctpDecClass * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *element_class;

  gobject_class = G_OBJECT_CLASS (klass);
  element_class = GST_ELEMENT_CLASS (klass);

  GST_DEBUG_CATEGORY_INIT (gst_sctp_dec_debug_category,
      "sctpdec", 0, "debug category for sctpdec element");

  gst_element_class_add_pad_template (element_class,
      gst_static_pad_template_get (&src_template));
  gst_element_class_add_pad_template (element_class,
      gst_static_pad_template_get (&sink_template));

  gobject_class->set_property = gst_sctp_dec_set_property;
  gobject_class->get_property = gst_sctp_dec_get_property;
  gobject_class->finalize = gst_sctp_dec_finalize;

  element_class->change_state = GST_DEBUG_FUNCPTR (gst_sctp_dec_change_state);

  klass->on_reset_stream = on_reset_stream;

  properties[PROP_GST_SCTP_ASSOCIATION_ID] =
      g_param_spec_uint ("sctp-association-id",
      "SCTP Association ID",
      "Every encoder/decoder pair should have the same, unique, sctp-association-id. "
      "This value must be set before any pads are requested.",
      0, MAX_GST_SCTP_ASSOCIATION_ID, DEFAULT_GST_SCTP_ASSOCIATION_ID,
      G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  properties[PROP_LOCAL_SCTP_PORT] =
      g_param_spec_uint ("local-sctp-port",
      "Local SCTP port",
      "Local sctp port for the sctp association. The remote port is configured via the "
      "GstSctpEnc element.",
      0, MAX_SCTP_PORT, DEFAULT_LOCAL_SCTP_PORT,
      G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (gobject_class, NUM_PROPERTIES, properties);

  signals[SIGNAL_RESET_STREAM] = g_signal_new ("reset-stream",
      G_TYPE_FROM_CLASS (gobject_class), G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
      G_STRUCT_OFFSET (GstSctpDecClass, on_reset_stream), NULL, NULL,
      NULL, G_TYPE_NONE, 1, G_TYPE_UINT);

  gst_element_class_set_static_metadata (element_class,
      "SCTP Decoder",
      "Decoder/Network/SCTP",
      "Decodes packets with SCTP",
      "George Kiagiadakis <george.kiagiadakis@collabora.com>");
}

static void
gst_sctp_dec_init (GstSctpDec * self)
{
  self->sctp_association_id = DEFAULT_GST_SCTP_ASSOCIATION_ID;
  self->local_sctp_port = DEFAULT_LOCAL_SCTP_PORT;

  self->flow_combiner = gst_flow_combiner_new ();

  self->sink_pad = gst_pad_new_from_static_template (&sink_template, "sink");
  gst_pad_set_chain_function (self->sink_pad,
      GST_DEBUG_FUNCPTR ((GstPadChainFunction) gst_sctp_dec_packet_chain));
  gst_pad_set_event_function (self->sink_pad,
      GST_DEBUG_FUNCPTR ((GstPadEventFunction) gst_sctp_dec_packet_event));

  gst_element_add_pad (GST_ELEMENT (self), self->sink_pad);
}

static void
gst_sctp_dec_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstSctpDec *self = GST_SCTP_DEC (object);

  switch (prop_id) {
    case PROP_GST_SCTP_ASSOCIATION_ID:
      self->sctp_association_id = g_value_get_uint (value);
      break;
    case PROP_LOCAL_SCTP_PORT:
      self->local_sctp_port = g_value_get_uint (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (self, prop_id, pspec);
      break;
  }
}

static void
gst_sctp_dec_get_property (GObject * object, guint prop_id, GValue * value,
    GParamSpec * pspec)
{
  GstSctpDec *self = GST_SCTP_DEC (object);

  switch (prop_id) {
    case PROP_GST_SCTP_ASSOCIATION_ID:
      g_value_set_uint (value, self->sctp_association_id);
      break;
    case PROP_LOCAL_SCTP_PORT:
      g_value_set_uint (value, self->local_sctp_port);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (self, prop_id, pspec);
      break;
  }
}

static void
gst_sctp_dec_finalize (GObject * object)
{
  GstSctpDec *self = GST_SCTP_DEC (object);

  gst_flow_combiner_free (self->flow_combiner);
  self->flow_combiner = NULL;

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static GstStateChangeReturn
gst_sctp_dec_change_state (GstElement * element, GstStateChange transition)
{
  GstSctpDec *self = GST_SCTP_DEC (element);
  GstStateChangeReturn ret = GST_STATE_CHANGE_SUCCESS;

  switch (transition) {
    case GST_STATE_CHANGE_READY_TO_PAUSED:
      gst_flow_combiner_reset (self->flow_combiner);
      if (!configure_association (self))
        ret = GST_STATE_CHANGE_FAILURE;
      break;
    case GST_STATE_CHANGE_PAUSED_TO_READY:
      stop_all_srcpad_tasks (self);
      break;
    default:
      break;
  }

  if (ret != GST_STATE_CHANGE_FAILURE)
    ret = GST_ELEMENT_CLASS (parent_class)->change_state (element, transition);

  switch (transition) {
    case GST_STATE_CHANGE_PAUSED_TO_READY:
      sctpdec_cleanup (self);
      gst_flow_combiner_reset (self->flow_combiner);
      break;
    default:
      break;
  }

  return ret;
}

static GstFlowReturn
gst_sctp_dec_packet_chain (GstPad * pad, GstSctpDec * self, GstBuffer * buf)
{
  GstFlowReturn flow_ret;
  GstMapInfo map;

  GST_DEBUG_OBJECT (self, "Processing received buffer %" GST_PTR_FORMAT, buf);

  if (!gst_buffer_map (buf, &map, GST_MAP_READ)) {
    GST_ERROR_OBJECT (self, "Could not map GstBuffer");
    gst_buffer_unref (buf);
    return GST_FLOW_ERROR;
  }

  gst_sctp_association_incoming_packet (self->sctp_association,
      (const guint8 *) map.data, (guint32) map.size);
  gst_buffer_unmap (buf, &map);
  gst_buffer_unref (buf);

  GST_OBJECT_LOCK (self);
  /* This gets the last combined flow return from all source pads */
  flow_ret = gst_flow_combiner_update_flow (self->flow_combiner, GST_FLOW_OK);
  GST_OBJECT_UNLOCK (self);

  if (flow_ret != GST_FLOW_OK) {
    GST_DEBUG_OBJECT (self, "Returning %s", gst_flow_get_name (flow_ret));
  }

  return flow_ret;
}

static void
flush_srcpad (const GValue * item, gpointer user_data)
{
  GstSctpDecPad *sctpdec_pad = g_value_get_object (item);
  gboolean flush = GPOINTER_TO_INT (user_data);

  if (flush) {
    gst_data_queue_set_flushing (sctpdec_pad->packet_queue, TRUE);
    gst_data_queue_flush (sctpdec_pad->packet_queue);
  } else {
    gst_data_queue_set_flushing (sctpdec_pad->packet_queue, FALSE);
    gst_pad_start_task (GST_PAD (sctpdec_pad),
        (GstTaskFunction) gst_sctp_data_srcpad_loop, sctpdec_pad, NULL);
  }
}

static gboolean
gst_sctp_dec_packet_event (GstPad * pad, GstSctpDec * self, GstEvent * event)
{
  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_STREAM_START:
    case GST_EVENT_CAPS:
      /* We create our own stream-start events and the caps event does not
       * make sense */
      gst_event_unref (event);
      return TRUE;
    case GST_EVENT_EOS:
      /* Drop this, we're never EOS until shut down */
      gst_event_unref (event);
      return TRUE;
    case GST_EVENT_FLUSH_START:{
      GstIterator *it;

      it = gst_element_iterate_src_pads (GST_ELEMENT (self));
      while (gst_iterator_foreach (it, flush_srcpad,
              GINT_TO_POINTER (TRUE)) == GST_ITERATOR_RESYNC)
        gst_iterator_resync (it);
      gst_iterator_free (it);

      return gst_pad_event_default (pad, GST_OBJECT (self), event);
    }
    case GST_EVENT_FLUSH_STOP:{
      GstIterator *it;

      it = gst_element_iterate_src_pads (GST_ELEMENT (self));
      while (gst_iterator_foreach (it, flush_srcpad,
              GINT_TO_POINTER (FALSE)) == GST_ITERATOR_RESYNC)
        gst_iterator_resync (it);
      gst_iterator_free (it);

      return gst_pad_event_default (pad, GST_OBJECT (self), event);
    }
    default:
      return gst_pad_event_default (pad, GST_OBJECT (self), event);
  }
}

static void
gst_sctp_data_srcpad_loop (GstPad * pad)
{
  GstSctpDec *self;
  GstSctpDecPad *sctpdec_pad = GST_SCTP_DEC_PAD (pad);
  GstDataQueueItem *item;

  self = GST_SCTP_DEC (gst_pad_get_parent (pad));

  if (gst_data_queue_pop (sctpdec_pad->packet_queue, &item)) {
    GstBuffer *buffer;
    GstFlowReturn flow_ret;

    buffer = GST_BUFFER (item->object);
    GST_DEBUG_OBJECT (pad, "Forwarding buffer %" GST_PTR_FORMAT, buffer);

    flow_ret = gst_pad_push (pad, buffer);
    item->object = NULL;

    GST_OBJECT_LOCK (self);
    gst_flow_combiner_update_pad_flow (self->flow_combiner, pad, flow_ret);
    GST_OBJECT_UNLOCK (self);

    if (G_UNLIKELY (flow_ret == GST_FLOW_FLUSHING
            || flow_ret == GST_FLOW_NOT_LINKED) || flow_ret == GST_FLOW_EOS) {
      GST_DEBUG_OBJECT (pad, "Push failed on packet source pad. Error: %s",
          gst_flow_get_name (flow_ret));
    } else if (G_UNLIKELY (flow_ret != GST_FLOW_OK)) {
      GST_ERROR_OBJECT (pad, "Push failed on packet source pad. Error: %s",
          gst_flow_get_name (flow_ret));
    }

    if (G_UNLIKELY (flow_ret != GST_FLOW_OK)) {
      GST_DEBUG_OBJECT (pad, "Pausing task because of an error");
      gst_data_queue_set_flushing (sctpdec_pad->packet_queue, TRUE);
      gst_data_queue_flush (sctpdec_pad->packet_queue);
      gst_pad_pause_task (pad);
    }

    item->destroy (item);
  } else {
    GST_OBJECT_LOCK (self);
    gst_flow_combiner_update_pad_flow (self->flow_combiner, pad,
        GST_FLOW_FLUSHING);
    GST_OBJECT_UNLOCK (self);

    GST_DEBUG_OBJECT (pad, "Pausing task because we're flushing");
    gst_pad_pause_task (pad);
  }

  gst_object_unref (self);
}

static gboolean
configure_association (GstSctpDec * self)
{
  gint state;

  self->sctp_association = gst_sctp_association_get (self->sctp_association_id);

  g_object_get (self->sctp_association, "state", &state, NULL);

  if (state != GST_SCTP_ASSOCIATION_STATE_NEW) {
    GST_WARNING_OBJECT (self,
        "Could not configure SCTP association. Association already in use!");
    g_object_unref (self->sctp_association);
    self->sctp_association = NULL;
    goto error;
  }

  self->signal_handler_stream_reset =
      g_signal_connect_object (self->sctp_association, "stream-reset",
      G_CALLBACK (on_gst_sctp_association_stream_reset), self, 0);

  g_object_bind_property (self, "local-sctp-port", self->sctp_association,
      "local-port", G_BINDING_SYNC_CREATE);

  gst_sctp_association_set_on_packet_received (self->sctp_association,
      on_receive, gst_object_ref (self), gst_object_unref);

  return TRUE;
error:
  return FALSE;
}

static gboolean
gst_sctp_dec_src_event (GstPad * pad, GstSctpDec * self, GstEvent * event)
{
  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_RECONFIGURE:
    case GST_EVENT_FLUSH_STOP:{
      GstSctpDecPad *sctpdec_pad = GST_SCTP_DEC_PAD (pad);

      /* Unflush and start task again */
      gst_data_queue_set_flushing (sctpdec_pad->packet_queue, FALSE);
      gst_pad_start_task (pad, (GstTaskFunction) gst_sctp_data_srcpad_loop, pad,
          NULL);

      return gst_pad_event_default (pad, GST_OBJECT (self), event);
    }
    case GST_EVENT_FLUSH_START:{
      GstSctpDecPad *sctpdec_pad = GST_SCTP_DEC_PAD (pad);

      gst_data_queue_set_flushing (sctpdec_pad->packet_queue, TRUE);
      gst_data_queue_flush (sctpdec_pad->packet_queue);

      return gst_pad_event_default (pad, GST_OBJECT (self), event);
    }
    default:
      return gst_pad_event_default (pad, GST_OBJECT (self), event);
  }
}

static gboolean
copy_sticky_events (GstPad * pad, GstEvent ** event, gpointer user_data)
{
  GstPad *new_pad = user_data;

  if (GST_EVENT_TYPE (*event) != GST_EVENT_CAPS
      && GST_EVENT_TYPE (*event) != GST_EVENT_STREAM_START)
    gst_pad_store_sticky_event (new_pad, *event);

  return TRUE;
}

static GstPad *
get_pad_for_stream_id (GstSctpDec * self, guint16 stream_id)
{
  GstPad *new_pad = NULL;
  gint state;
  gchar *pad_name, *pad_stream_id;
  GstPadTemplate *template;

  pad_name = g_strdup_printf ("src_%hu", stream_id);
  new_pad = gst_element_get_static_pad (GST_ELEMENT (self), pad_name);
  if (new_pad) {
    g_free (pad_name);
    return new_pad;
  }

  g_object_get (self->sctp_association, "state", &state, NULL);

  if (state != GST_SCTP_ASSOCIATION_STATE_CONNECTED) {
    GST_ERROR_OBJECT (self,
        "The SCTP association must be established before a new stream can be created");
    return NULL;
  }

  GST_DEBUG_OBJECT (self, "Creating new pad for stream id %u", stream_id);

  if (stream_id > MAX_STREAM_ID)
    return NULL;

  template = gst_static_pad_template_get (&src_template);
  new_pad = g_object_new (GST_TYPE_SCTP_DEC_PAD, "name", pad_name,
      "direction", template->direction, "template", template, NULL);
  g_free (pad_name);
  gst_clear_object (&template);

  gst_pad_set_event_function (new_pad,
      GST_DEBUG_FUNCPTR ((GstPadEventFunction) gst_sctp_dec_src_event));

  if (!gst_pad_set_active (new_pad, TRUE))
    goto error_cleanup;

  pad_stream_id =
      gst_pad_create_stream_id_printf (new_pad, GST_ELEMENT (self), "%hu",
      stream_id);
  gst_pad_push_event (new_pad, gst_event_new_stream_start (pad_stream_id));
  g_free (pad_stream_id);
  gst_pad_sticky_events_foreach (self->sink_pad, copy_sticky_events, new_pad);

  if (!gst_element_add_pad (GST_ELEMENT (self), new_pad))
    goto error_add;

  GST_OBJECT_LOCK (self);
  gst_flow_combiner_add_pad (self->flow_combiner, new_pad);
  GST_OBJECT_UNLOCK (self);

  gst_pad_start_task (new_pad, (GstTaskFunction) gst_sctp_data_srcpad_loop,
      new_pad, NULL);

  gst_object_ref (new_pad);

  return new_pad;
error_add:
  gst_pad_set_active (new_pad, FALSE);
error_cleanup:
  gst_object_unref (new_pad);
  return NULL;
}

static void
remove_pad (GstSctpDec * self, GstPad * pad)
{
  stop_srcpad_task (pad);
  GST_PAD_STREAM_LOCK (pad);
  gst_pad_set_active (pad, FALSE);
  if (gst_object_has_as_parent (GST_OBJECT (pad), GST_OBJECT (self)))
    gst_element_remove_pad (GST_ELEMENT (self), pad);
  GST_PAD_STREAM_UNLOCK (pad);
  GST_OBJECT_LOCK (self);
  gst_flow_combiner_remove_pad (self->flow_combiner, pad);
  GST_OBJECT_UNLOCK (self);
}

static void
on_gst_sctp_association_stream_reset (GstSctpAssociation * gst_sctp_association,
    guint16 stream_id, GstSctpDec * self)
{
  gchar *pad_name;
  GstPad *srcpad;

  GST_DEBUG_OBJECT (self, "Stream %u reset", stream_id);

  pad_name = g_strdup_printf ("src_%hu", stream_id);
  srcpad = gst_element_get_static_pad (GST_ELEMENT (self), pad_name);
  g_free (pad_name);
  if (!srcpad) {
    /* This can happen if a stream is created but the peer never sends any data.
     * We still need to signal the reset by removing the relevant pad.  To do
     * that, we need to add the relevant pad first. */
    srcpad = get_pad_for_stream_id (self, stream_id);
    if (!srcpad) {
      GST_WARNING_OBJECT (self, "Reset called on stream without a srcpad");
      return;
    }
  }
  remove_pad (self, srcpad);
  gst_object_unref (srcpad);
}

static void
data_queue_item_free (GstDataQueueItem * item)
{
  if (item->object)
    gst_mini_object_unref (item->object);
  g_free (item);
}

static void
on_receive (GstSctpAssociation * sctp_association, guint8 * buf,
    gsize length, guint16 stream_id, guint ppid, gpointer user_data)
{
  GstSctpDec *self = user_data;
  GstSctpDecPad *sctpdec_pad;
  GstPad *src_pad;
  GstDataQueueItem *item;
  GstBuffer *gstbuf;

  src_pad = get_pad_for_stream_id (self, stream_id);
  g_assert (src_pad);

  GST_DEBUG_OBJECT (src_pad,
      "Received incoming packet of size %" G_GSIZE_FORMAT
      " with stream id %u ppid %u", length, stream_id, ppid);

  sctpdec_pad = GST_SCTP_DEC_PAD (src_pad);
  gstbuf =
      gst_buffer_new_wrapped_full (0, buf, length, 0, length, buf,
      (GDestroyNotify) usrsctp_freedumpbuffer);
  gst_sctp_buffer_add_receive_meta (gstbuf, ppid);

  item = g_new0 (GstDataQueueItem, 1);
  item->object = GST_MINI_OBJECT (gstbuf);
  item->size = length;
  item->visible = TRUE;
  item->destroy = (GDestroyNotify) data_queue_item_free;
  if (!gst_data_queue_push (sctpdec_pad->packet_queue, item)) {
    item->destroy (item);
    GST_DEBUG_OBJECT (src_pad, "Failed to push item because we're flushing");
  }

  gst_object_unref (src_pad);
}

static void
stop_srcpad_task (GstPad * pad)
{
  GstSctpDecPad *sctpdec_pad = GST_SCTP_DEC_PAD (pad);

  gst_data_queue_set_flushing (sctpdec_pad->packet_queue, TRUE);
  gst_data_queue_flush (sctpdec_pad->packet_queue);
  gst_pad_stop_task (pad);
}

static void
remove_pad_it (const GValue * item, gpointer user_data)
{
  GstPad *pad = g_value_get_object (item);
  GstSctpDec *self = user_data;

  remove_pad (self, pad);
}

static void
stop_all_srcpad_tasks (GstSctpDec * self)
{
  GstIterator *it;

  it = gst_element_iterate_src_pads (GST_ELEMENT (self));
  while (gst_iterator_foreach (it, remove_pad_it, self) == GST_ITERATOR_RESYNC)
    gst_iterator_resync (it);
  gst_iterator_free (it);
}

static void
sctpdec_cleanup (GstSctpDec * self)
{
  if (self->sctp_association) {
    gst_sctp_association_set_on_packet_received (self->sctp_association, NULL,
        NULL, NULL);
    g_signal_handler_disconnect (self->sctp_association,
        self->signal_handler_stream_reset);
    gst_sctp_association_force_close (self->sctp_association);
    g_object_unref (self->sctp_association);
    self->sctp_association = NULL;
  }
}

static void
on_reset_stream (GstSctpDec * self, guint stream_id)
{
  if (self->sctp_association) {
    gst_sctp_association_reset_stream (self->sctp_association, stream_id);
    on_gst_sctp_association_stream_reset (self->sctp_association, stream_id,
        self);
  }
}
