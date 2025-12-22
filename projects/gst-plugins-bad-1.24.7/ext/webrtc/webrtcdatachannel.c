/* GStreamer
 * Copyright (C) 2018 Matthew Waters <matthew@centricular.com>
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
 * SECTION:gstwebrtc-datachannel
 * @short_description: RTCDataChannel object
 * @title: GstWebRTCDataChannel
 * @see_also: #GstWebRTCRTPTransceiver
 *
 * <http://w3c.github.io/webrtc-pc/#dom-rtcsctptransport>
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "webrtcdatachannel.h"
#include <gst/app/gstappsink.h>
#include <gst/app/gstappsrc.h>
#include <gst/base/gstbytereader.h>
#include <gst/base/gstbytewriter.h>
#include <gst/sctp/sctpreceivemeta.h>
#include <gst/sctp/sctpsendmeta.h>

#include "gstwebrtcbin.h"
#include "utils.h"

#define GST_CAT_DEFAULT webrtc_data_channel_debug
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);

static void _close_procedure (WebRTCDataChannel * channel, gpointer user_data);

typedef void (*ChannelTask) (GstWebRTCDataChannel * channel,
    gpointer user_data);

struct task
{
  GstWebRTCBin *webrtcbin;
  GstWebRTCDataChannel *channel;
  ChannelTask func;
  gpointer user_data;
  GDestroyNotify notify;
};

static GstStructure *
_execute_task (GstWebRTCBin * webrtc, struct task *task)
{
  if (task->func)
    task->func (task->channel, task->user_data);

  return NULL;
}

static void
_free_task (struct task *task)
{
  g_object_unref (task->webrtcbin);
  gst_object_unref (task->channel);

  if (task->notify)
    task->notify (task->user_data);
  g_free (task);
}

static void
_channel_enqueue_task (WebRTCDataChannel * channel, ChannelTask func,
    gpointer user_data, GDestroyNotify notify)
{
  GstWebRTCBin *webrtcbin = NULL;
  struct task *task = NULL;

  webrtcbin = g_weak_ref_get (&channel->webrtcbin_weak);
  if (!webrtcbin)
    return;

  task = g_new0 (struct task, 1);

  task->webrtcbin = webrtcbin;
  task->channel = gst_object_ref (channel);
  task->func = func;
  task->user_data = user_data;
  task->notify = notify;

  gst_webrtc_bin_enqueue_task (task->webrtcbin,
      (GstWebRTCBinFunc) _execute_task, task, (GDestroyNotify) _free_task,
      NULL);
}

static void
_channel_store_error (WebRTCDataChannel * channel, GError * error)
{
  GST_WEBRTC_DATA_CHANNEL_LOCK (channel);
  if (error) {
    GST_WARNING_OBJECT (channel, "Error: %s",
        error ? error->message : "Unknown");
    if (!channel->stored_error)
      channel->stored_error = error;
    else
      g_clear_error (&error);
  }
  GST_WEBRTC_DATA_CHANNEL_UNLOCK (channel);
}

struct _WebRTCErrorIgnoreBin
{
  GstBin bin;

  WebRTCDataChannel *data_channel;
};

G_DEFINE_TYPE (WebRTCErrorIgnoreBin, webrtc_error_ignore_bin, GST_TYPE_BIN);

static void
webrtc_error_ignore_bin_handle_message (GstBin * bin, GstMessage * message)
{
  WebRTCErrorIgnoreBin *self = WEBRTC_ERROR_IGNORE_BIN (bin);

  switch (GST_MESSAGE_TYPE (message)) {
    case GST_MESSAGE_ERROR:{
      GError *error = NULL;
      gst_message_parse_error (message, &error, NULL);
      GST_DEBUG_OBJECT (bin, "handling error message from internal element");
      _channel_store_error (self->data_channel, error);
      _channel_enqueue_task (self->data_channel, (ChannelTask) _close_procedure,
          NULL, NULL);
      break;
    }
    default:
      GST_BIN_CLASS (webrtc_error_ignore_bin_parent_class)->handle_message (bin,
          message);
      break;
  }
}

static void
webrtc_error_ignore_bin_class_init (WebRTCErrorIgnoreBinClass * klass)
{
  GstBinClass *bin_class = (GstBinClass *) klass;

  bin_class->handle_message = webrtc_error_ignore_bin_handle_message;
}

static void
webrtc_error_ignore_bin_init (WebRTCErrorIgnoreBin * bin)
{
}

static GstElement *
webrtc_error_ignore_bin_new (WebRTCDataChannel * data_channel,
    GstElement * other)
{
  WebRTCErrorIgnoreBin *self;
  GstPad *pad;

  self = g_object_new (webrtc_error_ignore_bin_get_type (), NULL);
  self->data_channel = data_channel;

  gst_bin_add (GST_BIN (self), other);

  pad = gst_element_get_static_pad (other, "src");
  if (pad) {
    GstPad *ghost_pad = gst_ghost_pad_new ("src", pad);
    gst_element_add_pad (GST_ELEMENT (self), ghost_pad);
    gst_clear_object (&pad);
  }
  pad = gst_element_get_static_pad (other, "sink");
  if (pad) {
    GstPad *ghost_pad = gst_ghost_pad_new ("sink", pad);
    gst_element_add_pad (GST_ELEMENT (self), ghost_pad);
    gst_clear_object (&pad);
  }

  return (GstElement *) self;
}

#define webrtc_data_channel_parent_class parent_class
G_DEFINE_TYPE_WITH_CODE (WebRTCDataChannel, webrtc_data_channel,
    GST_TYPE_WEBRTC_DATA_CHANNEL,
    GST_DEBUG_CATEGORY_INIT (webrtc_data_channel_debug, "webrtcdatachannel", 0,
        "webrtcdatachannel"););

G_LOCK_DEFINE_STATIC (outstanding_channels_lock);
static GList *outstanding_channels;

typedef enum
{
  DATA_CHANNEL_PPID_WEBRTC_CONTROL = 50,
  DATA_CHANNEL_PPID_WEBRTC_STRING = 51,
  DATA_CHANNEL_PPID_WEBRTC_BINARY_PARTIAL = 52, /* deprecated */
  DATA_CHANNEL_PPID_WEBRTC_BINARY = 53,
  DATA_CHANNEL_PPID_WEBRTC_STRING_PARTIAL = 54, /* deprecated */
  DATA_CHANNEL_PPID_WEBRTC_BINARY_EMPTY = 56,
  DATA_CHANNEL_PPID_WEBRTC_STRING_EMPTY = 57,
} DataChannelPPID;

typedef enum
{
  CHANNEL_TYPE_RELIABLE = 0x00,
  CHANNEL_TYPE_RELIABLE_UNORDERED = 0x80,
  CHANNEL_TYPE_PARTIAL_RELIABLE_REXMIT = 0x01,
  CHANNEL_TYPE_PARTIAL_RELIABLE_REXMIT_UNORDERED = 0x81,
  CHANNEL_TYPE_PARTIAL_RELIABLE_TIMED = 0x02,
  CHANNEL_TYPE_PARTIAL_RELIABLE_TIMED_UNORDERED = 0x82,
} DataChannelReliabilityType;

typedef enum
{
  CHANNEL_MESSAGE_ACK = 0x02,
  CHANNEL_MESSAGE_OPEN = 0x03,
} DataChannelMessage;

static guint16
priority_type_to_uint (GstWebRTCPriorityType pri)
{
  switch (pri) {
    case GST_WEBRTC_PRIORITY_TYPE_VERY_LOW:
      return 64;
    case GST_WEBRTC_PRIORITY_TYPE_LOW:
      return 192;
    case GST_WEBRTC_PRIORITY_TYPE_MEDIUM:
      return 384;
    case GST_WEBRTC_PRIORITY_TYPE_HIGH:
      return 768;
  }
  g_assert_not_reached ();
  return 0;
}

static GstWebRTCPriorityType
priority_uint_to_type (guint16 val)
{
  if (val <= 128)
    return GST_WEBRTC_PRIORITY_TYPE_VERY_LOW;
  if (val <= 256)
    return GST_WEBRTC_PRIORITY_TYPE_LOW;
  if (val <= 512)
    return GST_WEBRTC_PRIORITY_TYPE_MEDIUM;
  return GST_WEBRTC_PRIORITY_TYPE_HIGH;
}

static GstBuffer *
construct_open_packet (WebRTCDataChannel * channel)
{
  GstByteWriter w;
  gsize label_len = strlen (channel->parent.label);
  gsize proto_len = strlen (channel->parent.protocol);
  gsize size = 12 + label_len + proto_len;
  DataChannelReliabilityType reliability = 0;
  guint32 reliability_param = 0;
  guint16 priority;
  GstBuffer *buf;

/*
 *    0                   1                   2                   3
 *    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 *   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *   |  Message Type |  Channel Type |            Priority           |
 *   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *   |                    Reliability Parameter                      |
 *   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *   |         Label Length          |       Protocol Length         |
 *   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *   \                                                               /
 *   |                             Label                             |
 *   /                                                               \
 *   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *   \                                                               /
 *   |                            Protocol                           |
 *   /                                                               \
 *   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 */

  gst_byte_writer_init_with_size (&w, size, FALSE);

  if (!gst_byte_writer_put_uint8 (&w, (guint8) CHANNEL_MESSAGE_OPEN))
    g_return_val_if_reached (NULL);

  if (!channel->parent.ordered)
    reliability |= 0x80;
  if (channel->parent.max_retransmits != -1) {
    reliability |= 0x01;
    reliability_param = channel->parent.max_retransmits;
  }
  if (channel->parent.max_packet_lifetime != -1) {
    reliability |= 0x02;
    reliability_param = channel->parent.max_packet_lifetime;
  }

  priority = priority_type_to_uint (channel->parent.priority);

  if (!gst_byte_writer_put_uint8 (&w, (guint8) reliability))
    g_return_val_if_reached (NULL);
  if (!gst_byte_writer_put_uint16_be (&w, (guint16) priority))
    g_return_val_if_reached (NULL);
  if (!gst_byte_writer_put_uint32_be (&w, (guint32) reliability_param))
    g_return_val_if_reached (NULL);
  if (!gst_byte_writer_put_uint16_be (&w, (guint16) label_len))
    g_return_val_if_reached (NULL);
  if (!gst_byte_writer_put_uint16_be (&w, (guint16) proto_len))
    g_return_val_if_reached (NULL);
  if (!gst_byte_writer_put_data (&w, (guint8 *) channel->parent.label,
          label_len))
    g_return_val_if_reached (NULL);
  if (!gst_byte_writer_put_data (&w, (guint8 *) channel->parent.protocol,
          proto_len))
    g_return_val_if_reached (NULL);

  buf = gst_byte_writer_reset_and_get_buffer (&w);

  /* send reliable and ordered */
  gst_sctp_buffer_add_send_meta (buf, DATA_CHANNEL_PPID_WEBRTC_CONTROL, TRUE,
      GST_SCTP_SEND_META_PARTIAL_RELIABILITY_NONE, 0);

  return buf;
}

static GstBuffer *
construct_ack_packet (WebRTCDataChannel * channel)
{
  GstByteWriter w;
  GstBuffer *buf;

/*
 *   0                   1                   2                   3
 *   0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 *   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *   |  Message Type |
 *   +-+-+-+-+-+-+-+-+
 */

  gst_byte_writer_init_with_size (&w, 1, FALSE);

  if (!gst_byte_writer_put_uint8 (&w, (guint8) CHANNEL_MESSAGE_ACK))
    g_return_val_if_reached (NULL);

  buf = gst_byte_writer_reset_and_get_buffer (&w);

  /* send reliable and ordered */
  gst_sctp_buffer_add_send_meta (buf, DATA_CHANNEL_PPID_WEBRTC_CONTROL, TRUE,
      GST_SCTP_SEND_META_PARTIAL_RELIABILITY_NONE, 0);

  return buf;
}

static void
_emit_on_open (WebRTCDataChannel * channel, gpointer user_data)
{
  gst_webrtc_data_channel_on_open (GST_WEBRTC_DATA_CHANNEL (channel));
}

static void
_transport_closed (WebRTCDataChannel * channel)
{
  GError *error;
  gboolean both_sides_closed;

  GST_WEBRTC_DATA_CHANNEL_LOCK (channel);
  error = channel->stored_error;
  channel->stored_error = NULL;

  GST_TRACE_OBJECT (channel, "transport closed, peer closed %u error %p "
      "buffered %" G_GUINT64_FORMAT, channel->peer_closed, error,
      channel->parent.buffered_amount);

  both_sides_closed =
      channel->peer_closed && channel->parent.buffered_amount <= 0;
  if (both_sides_closed || error) {
    channel->peer_closed = FALSE;
  }
  GST_WEBRTC_DATA_CHANNEL_UNLOCK (channel);

  if (error) {
    gst_webrtc_data_channel_on_error (GST_WEBRTC_DATA_CHANNEL (channel), error);
    g_clear_error (&error);
  }
  if (both_sides_closed || error) {
    gst_webrtc_data_channel_on_close (GST_WEBRTC_DATA_CHANNEL (channel));
  }
}

static void
_close_sctp_stream (WebRTCDataChannel * channel, gpointer user_data)
{
  GstPad *pad, *peer;

  GST_INFO_OBJECT (channel, "Closing outgoing SCTP stream %i label \"%s\"",
      channel->parent.id, channel->parent.label);

  pad = gst_element_get_static_pad (channel->src_bin, "src");
  peer = gst_pad_get_peer (pad);
  gst_object_unref (pad);

  if (peer) {
    GstElement *sctpenc = gst_pad_get_parent_element (peer);

    if (sctpenc) {
      GST_TRACE_OBJECT (channel, "removing sctpenc pad %" GST_PTR_FORMAT, peer);
      gst_element_release_request_pad (sctpenc, peer);
      gst_object_unref (sctpenc);
    }
    gst_object_unref (peer);
  }

  _transport_closed (channel);
}

static void
_close_procedure (WebRTCDataChannel * channel, gpointer user_data)
{
  /* https://www.w3.org/TR/webrtc/#data-transport-closing-procedure */
  GST_WEBRTC_DATA_CHANNEL_LOCK (channel);
  if (channel->parent.ready_state == GST_WEBRTC_DATA_CHANNEL_STATE_CLOSED) {
    GST_WEBRTC_DATA_CHANNEL_UNLOCK (channel);
    return;
  } else if (channel->parent.ready_state ==
      GST_WEBRTC_DATA_CHANNEL_STATE_CLOSING) {
    _channel_enqueue_task (channel, (ChannelTask) _transport_closed, NULL,
        NULL);
  } else if (channel->parent.ready_state == GST_WEBRTC_DATA_CHANNEL_STATE_OPEN) {
    channel->parent.ready_state = GST_WEBRTC_DATA_CHANNEL_STATE_CLOSING;
    GST_WEBRTC_DATA_CHANNEL_UNLOCK (channel);
    g_object_notify (G_OBJECT (channel), "ready-state");

    /* Make sure that all data enqueued gets properly sent before data channel is closed. */
    GstFlowReturn ret =
        gst_app_src_end_of_stream (GST_APP_SRC (WEBRTC_DATA_CHANNEL
            (channel)->appsrc));
    if (ret != GST_FLOW_OK) {
      GST_WARNING_OBJECT (channel, "Send end of stream returned %i, %s", ret,
          gst_flow_get_name (ret));
    }
    return;
  }

  GST_WEBRTC_DATA_CHANNEL_UNLOCK (channel);
}

static void
_on_sctp_stream_reset (WebRTCSCTPTransport * sctp, guint stream_id,
    WebRTCDataChannel * channel)
{
  if (channel->parent.id == stream_id) {
    GST_INFO_OBJECT (channel,
        "Received channel close for SCTP stream %i label \"%s\"",
        channel->parent.id, channel->parent.label);

    GST_WEBRTC_DATA_CHANNEL_LOCK (channel);
    channel->peer_closed = TRUE;
    GST_WEBRTC_DATA_CHANNEL_UNLOCK (channel);

    _channel_enqueue_task (channel, (ChannelTask) _close_procedure,
        GUINT_TO_POINTER (stream_id), NULL);
  }
}

static void
webrtc_data_channel_close (GstWebRTCDataChannel * channel)
{
  _close_procedure (WEBRTC_DATA_CHANNEL (channel), NULL);
}

static GstFlowReturn
_parse_control_packet (WebRTCDataChannel * channel, guint8 * data,
    gsize size, GError ** error)
{
  GstByteReader r;
  guint8 message_type;
  gchar *label = NULL;
  gchar *proto = NULL;

  if (!data)
    g_return_val_if_reached (GST_FLOW_ERROR);
  if (size < 1)
    g_return_val_if_reached (GST_FLOW_ERROR);

  gst_byte_reader_init (&r, data, size);

  if (!gst_byte_reader_get_uint8 (&r, &message_type))
    g_return_val_if_reached (GST_FLOW_ERROR);

  if (message_type == CHANNEL_MESSAGE_ACK) {
    /* all good */
    GST_INFO_OBJECT (channel, "Received channel ack");
    return GST_FLOW_OK;
  } else if (message_type == CHANNEL_MESSAGE_OPEN) {
    guint8 reliability;
    guint32 reliability_param;
    guint16 priority, label_len, proto_len;
    const guint8 *src;
    GstBuffer *buffer;
    GstFlowReturn ret;

    GST_INFO_OBJECT (channel, "Received channel open");

    if (channel->parent.negotiated) {
      g_set_error (error, GST_WEBRTC_ERROR,
          GST_WEBRTC_ERROR_DATA_CHANNEL_FAILURE,
          "Data channel was signalled as negotiated already");
      g_return_val_if_reached (GST_FLOW_ERROR);
    }

    if (channel->opened)
      return GST_FLOW_OK;

    if (!gst_byte_reader_get_uint8 (&r, &reliability))
      goto parse_error;
    if (!gst_byte_reader_get_uint16_be (&r, &priority))
      goto parse_error;
    if (!gst_byte_reader_get_uint32_be (&r, &reliability_param))
      goto parse_error;
    if (!gst_byte_reader_get_uint16_be (&r, &label_len))
      goto parse_error;
    if (!gst_byte_reader_get_uint16_be (&r, &proto_len))
      goto parse_error;

    label = g_new0 (gchar, (gsize) label_len + 1);
    proto = g_new0 (gchar, (gsize) proto_len + 1);

    if (!gst_byte_reader_get_data (&r, label_len, &src))
      goto parse_error;
    memcpy (label, src, label_len);
    label[label_len] = '\0';
    if (!gst_byte_reader_get_data (&r, proto_len, &src))
      goto parse_error;
    memcpy (proto, src, proto_len);
    proto[proto_len] = '\0';

    g_free (channel->parent.label);
    channel->parent.label = label;
    g_free (channel->parent.protocol);
    channel->parent.protocol = proto;
    channel->parent.priority = priority_uint_to_type (priority);
    channel->parent.ordered = !(reliability & 0x80);
    if (reliability & 0x01) {
      channel->parent.max_retransmits = reliability_param;
      channel->parent.max_packet_lifetime = -1;
    } else if (reliability & 0x02) {
      channel->parent.max_retransmits = -1;
      channel->parent.max_packet_lifetime = reliability_param;
    } else {
      channel->parent.max_retransmits = -1;
      channel->parent.max_packet_lifetime = -1;
    }
    channel->opened = TRUE;

    GST_INFO_OBJECT (channel, "Received channel open for SCTP stream %i "
        "label \"%s\" protocol %s ordered %s", channel->parent.id,
        channel->parent.label, channel->parent.protocol,
        channel->parent.ordered ? "true" : "false");

    _channel_enqueue_task (channel, (ChannelTask) _emit_on_open, NULL, NULL);

    GST_INFO_OBJECT (channel, "Sending channel ack");
    buffer = construct_ack_packet (channel);

    GST_WEBRTC_DATA_CHANNEL_LOCK (channel);
    channel->parent.buffered_amount += gst_buffer_get_size (buffer);
    GST_WEBRTC_DATA_CHANNEL_UNLOCK (channel);

    ret = gst_app_src_push_buffer (GST_APP_SRC (channel->appsrc), buffer);
    if (ret != GST_FLOW_OK) {
      g_set_error (error, GST_WEBRTC_ERROR,
          GST_WEBRTC_ERROR_DATA_CHANNEL_FAILURE, "Could not send ack packet");
      GST_WARNING_OBJECT (channel, "push returned %i, %s", ret,
          gst_flow_get_name (ret));
      return ret;
    }

    return ret;
  } else {
    g_set_error (error, GST_WEBRTC_ERROR,
        GST_WEBRTC_ERROR_DATA_CHANNEL_FAILURE,
        "Unknown message type in control protocol");
    return GST_FLOW_ERROR;
  }

parse_error:
  {
    g_free (label);
    g_free (proto);
    g_set_error (error, GST_WEBRTC_ERROR,
        GST_WEBRTC_ERROR_DATA_CHANNEL_FAILURE, "Failed to parse packet");
    g_return_val_if_reached (GST_FLOW_ERROR);
  }
}

static void
on_sink_eos (GstAppSink * sink, gpointer user_data)
{
}

struct map_info
{
  GstBuffer *buffer;
  GstMapInfo map_info;
};

static void
buffer_unmap_and_unref (struct map_info *info)
{
  gst_buffer_unmap (info->buffer, &info->map_info);
  gst_buffer_unref (info->buffer);
  g_free (info);
}

static void
_emit_have_data (WebRTCDataChannel * channel, GBytes * data)
{
  gst_webrtc_data_channel_on_message_data (GST_WEBRTC_DATA_CHANNEL (channel),
      data);
}

static void
_emit_have_string (GstWebRTCDataChannel * channel, gchar * str)
{
  gst_webrtc_data_channel_on_message_string (GST_WEBRTC_DATA_CHANNEL (channel),
      str);
}

static GstFlowReturn
_data_channel_have_sample (WebRTCDataChannel * channel, GstSample * sample,
    GError ** error)
{
  GstSctpReceiveMeta *receive;
  GstBuffer *buffer;
  GstFlowReturn ret = GST_FLOW_OK;

  GST_LOG_OBJECT (channel, "Received sample %" GST_PTR_FORMAT, sample);

  g_return_val_if_fail (channel->sctp_transport != NULL, GST_FLOW_ERROR);

  buffer = gst_sample_get_buffer (sample);
  if (!buffer) {
    g_set_error (error, GST_WEBRTC_ERROR,
        GST_WEBRTC_ERROR_DATA_CHANNEL_FAILURE, "No buffer to handle");
    return GST_FLOW_ERROR;
  }
  receive = gst_sctp_buffer_get_receive_meta (buffer);
  if (!receive) {
    g_set_error (error, GST_WEBRTC_ERROR,
        GST_WEBRTC_ERROR_DATA_CHANNEL_FAILURE,
        "No SCTP Receive meta on the buffer");
    return GST_FLOW_ERROR;
  }

  switch (receive->ppid) {
    case DATA_CHANNEL_PPID_WEBRTC_CONTROL:{
      GstMapInfo info = GST_MAP_INFO_INIT;
      if (!gst_buffer_map (buffer, &info, GST_MAP_READ)) {
        g_set_error (error, GST_WEBRTC_ERROR,
            GST_WEBRTC_ERROR_DATA_CHANNEL_FAILURE,
            "Failed to map received buffer");
        ret = GST_FLOW_ERROR;
      } else {
        ret = _parse_control_packet (channel, info.data, info.size, error);
        gst_buffer_unmap (buffer, &info);
      }
      break;
    }
    case DATA_CHANNEL_PPID_WEBRTC_STRING:
    case DATA_CHANNEL_PPID_WEBRTC_STRING_PARTIAL:{
      GstMapInfo info = GST_MAP_INFO_INIT;
      if (!gst_buffer_map (buffer, &info, GST_MAP_READ)) {
        g_set_error (error, GST_WEBRTC_ERROR,
            GST_WEBRTC_ERROR_DATA_CHANNEL_FAILURE,
            "Failed to map received buffer");
        ret = GST_FLOW_ERROR;
      } else {
        gchar *str = g_strndup ((gchar *) info.data, info.size);
        _channel_enqueue_task (channel, (ChannelTask) _emit_have_string, str,
            g_free);
        gst_buffer_unmap (buffer, &info);
      }
      break;
    }
    case DATA_CHANNEL_PPID_WEBRTC_BINARY:
    case DATA_CHANNEL_PPID_WEBRTC_BINARY_PARTIAL:{
      struct map_info *info = g_new0 (struct map_info, 1);
      if (!gst_buffer_map (buffer, &info->map_info, GST_MAP_READ)) {
        g_set_error (error, GST_WEBRTC_ERROR,
            GST_WEBRTC_ERROR_DATA_CHANNEL_FAILURE,
            "Failed to map received buffer");
        ret = GST_FLOW_ERROR;
      } else {
        GBytes *data = g_bytes_new_with_free_func (info->map_info.data,
            info->map_info.size, (GDestroyNotify) buffer_unmap_and_unref, info);
        info->buffer = gst_buffer_ref (buffer);
        _channel_enqueue_task (channel, (ChannelTask) _emit_have_data, data,
            (GDestroyNotify) g_bytes_unref);
      }
      break;
    }
    case DATA_CHANNEL_PPID_WEBRTC_BINARY_EMPTY:
      _channel_enqueue_task (channel, (ChannelTask) _emit_have_data, NULL,
          NULL);
      break;
    case DATA_CHANNEL_PPID_WEBRTC_STRING_EMPTY:
      _channel_enqueue_task (channel, (ChannelTask) _emit_have_string, NULL,
          NULL);
      break;
    default:
      g_set_error (error, GST_WEBRTC_ERROR,
          GST_WEBRTC_ERROR_DATA_CHANNEL_FAILURE,
          "Unknown SCTP PPID %u received", receive->ppid);
      ret = GST_FLOW_ERROR;
      break;
  }

  return ret;
}

static GstFlowReturn
on_sink_preroll (GstAppSink * sink, gpointer user_data)
{
  WebRTCDataChannel *channel = user_data;
  GstSample *sample = gst_app_sink_pull_preroll (sink);
  GstFlowReturn ret;

  if (sample) {
    /* This sample also seems to be provided by the sample callback
       ret = _data_channel_have_sample (channel, sample); */
    ret = GST_FLOW_OK;
    gst_sample_unref (sample);
  } else if (gst_app_sink_is_eos (sink)) {
    ret = GST_FLOW_EOS;
  } else {
    ret = GST_FLOW_ERROR;
  }

  if (ret != GST_FLOW_OK) {
    _channel_enqueue_task (channel, (ChannelTask) _close_procedure, NULL, NULL);
  }

  return ret;
}

static GstFlowReturn
on_sink_sample (GstAppSink * sink, gpointer user_data)
{
  WebRTCDataChannel *channel = user_data;
  GstSample *sample = gst_app_sink_pull_sample (sink);
  GstFlowReturn ret;
  GError *error = NULL;

  if (sample) {
    ret = _data_channel_have_sample (channel, sample, &error);
    gst_sample_unref (sample);
  } else if (gst_app_sink_is_eos (sink)) {
    ret = GST_FLOW_EOS;
  } else {
    ret = GST_FLOW_ERROR;
  }

  if (error)
    _channel_store_error (channel, error);

  if (ret != GST_FLOW_OK) {
    _channel_enqueue_task (channel, (ChannelTask) _close_procedure, NULL, NULL);
  }

  return ret;
}

static GstAppSinkCallbacks sink_callbacks = {
  on_sink_eos,
  on_sink_preroll,
  on_sink_sample,
};

void
webrtc_data_channel_start_negotiation (WebRTCDataChannel * channel)
{
  GstBuffer *buffer;

  g_return_if_fail (!channel->parent.negotiated);
  g_return_if_fail (channel->parent.id != -1);
  g_return_if_fail (channel->sctp_transport != NULL);

  buffer = construct_open_packet (channel);

  GST_INFO_OBJECT (channel, "Sending channel open for SCTP stream %i "
      "label \"%s\" protocol %s ordered %s", channel->parent.id,
      channel->parent.label, channel->parent.protocol,
      channel->parent.ordered ? "true" : "false");

  GST_WEBRTC_DATA_CHANNEL_LOCK (channel);
  channel->parent.buffered_amount += gst_buffer_get_size (buffer);
  GST_WEBRTC_DATA_CHANNEL_UNLOCK (channel);
  g_object_notify (G_OBJECT (&channel->parent), "buffered-amount");

  if (gst_app_src_push_buffer (GST_APP_SRC (channel->appsrc),
          buffer) == GST_FLOW_OK) {
    channel->opened = TRUE;
    _channel_enqueue_task (channel, (ChannelTask) _emit_on_open, NULL, NULL);
  } else {
    GError *error = NULL;
    g_set_error (&error, GST_WEBRTC_ERROR,
        GST_WEBRTC_ERROR_DATA_CHANNEL_FAILURE,
        "Failed to send DCEP open packet");
    _channel_store_error (channel, error);
    _channel_enqueue_task (channel, (ChannelTask) _close_procedure, NULL, NULL);
  }
}

static void
_get_sctp_reliability (WebRTCDataChannel * channel,
    GstSctpSendMetaPartiallyReliability * reliability, guint * rel_param)
{
  if (channel->parent.max_retransmits != -1) {
    *reliability = GST_SCTP_SEND_META_PARTIAL_RELIABILITY_RTX;
    *rel_param = channel->parent.max_retransmits;
  } else if (channel->parent.max_packet_lifetime != -1) {
    *reliability = GST_SCTP_SEND_META_PARTIAL_RELIABILITY_TTL;
    *rel_param = channel->parent.max_packet_lifetime;
  } else {
    *reliability = GST_SCTP_SEND_META_PARTIAL_RELIABILITY_NONE;
    *rel_param = 0;
  }
}

static gboolean
_is_within_max_message_size (WebRTCDataChannel * channel, gsize size)
{
  return size <= channel->sctp_transport->max_message_size;
}

static gboolean
webrtc_data_channel_send_data (GstWebRTCDataChannel * base_channel,
    GBytes * bytes, GError ** error)
{
  WebRTCDataChannel *channel = WEBRTC_DATA_CHANNEL (base_channel);
  GstSctpSendMetaPartiallyReliability reliability;
  guint rel_param;
  guint32 ppid;
  GstBuffer *buffer;
  gsize size = 0;
  GstFlowReturn ret;

  if (!bytes) {
    buffer = gst_buffer_new ();
    ppid = DATA_CHANNEL_PPID_WEBRTC_BINARY_EMPTY;
  } else {
    guint8 *data;

    data = (guint8 *) g_bytes_get_data (bytes, &size);
    g_return_val_if_fail (data != NULL, FALSE);
    if (!_is_within_max_message_size (channel, size)) {
      g_set_error (error, GST_WEBRTC_ERROR, GST_WEBRTC_ERROR_TYPE_ERROR,
          "Requested to send data that is too large");
      return FALSE;
    }

    buffer = gst_buffer_new_wrapped_full (GST_MEMORY_FLAG_READONLY, data, size,
        0, size, g_bytes_ref (bytes), (GDestroyNotify) g_bytes_unref);
    ppid = DATA_CHANNEL_PPID_WEBRTC_BINARY;
  }

  _get_sctp_reliability (channel, &reliability, &rel_param);
  gst_sctp_buffer_add_send_meta (buffer, ppid, channel->parent.ordered,
      reliability, rel_param);

  GST_LOG_OBJECT (channel, "Sending data using buffer %" GST_PTR_FORMAT,
      buffer);

  GST_WEBRTC_DATA_CHANNEL_LOCK (channel);
  if (channel->parent.ready_state == GST_WEBRTC_DATA_CHANNEL_STATE_OPEN) {
    channel->parent.buffered_amount += size;
  } else {
    GST_WEBRTC_DATA_CHANNEL_UNLOCK (channel);
    g_set_error (error, GST_WEBRTC_ERROR,
        GST_WEBRTC_ERROR_INVALID_STATE, "channel is not open");
    gst_buffer_unref (buffer);
    return FALSE;
  }
  GST_WEBRTC_DATA_CHANNEL_UNLOCK (channel);

  ret = gst_app_src_push_buffer (GST_APP_SRC (channel->appsrc), buffer);
  if (ret == GST_FLOW_OK) {
    g_object_notify (G_OBJECT (&channel->parent), "buffered-amount");
  } else {
    g_set_error (error, GST_WEBRTC_ERROR,
        GST_WEBRTC_ERROR_DATA_CHANNEL_FAILURE, "Failed to send data");
    GST_WARNING_OBJECT (channel, "push returned %i, %s", ret,
        gst_flow_get_name (ret));

    GST_WEBRTC_DATA_CHANNEL_LOCK (channel);
    channel->parent.buffered_amount -= size;
    GST_WEBRTC_DATA_CHANNEL_UNLOCK (channel);

    _channel_enqueue_task (channel, (ChannelTask) _close_procedure, NULL, NULL);
    return FALSE;
  }

  return TRUE;
}

static gboolean
webrtc_data_channel_send_string (GstWebRTCDataChannel * base_channel,
    const gchar * str, GError ** error)
{
  WebRTCDataChannel *channel = WEBRTC_DATA_CHANNEL (base_channel);
  GstSctpSendMetaPartiallyReliability reliability;
  guint rel_param;
  guint32 ppid;
  GstBuffer *buffer;
  gsize size = 0;
  GstFlowReturn ret;

  if (!channel->parent.negotiated)
    g_return_val_if_fail (channel->opened, FALSE);
  g_return_val_if_fail (channel->sctp_transport != NULL, FALSE);

  if (!str) {
    buffer = gst_buffer_new ();
    ppid = DATA_CHANNEL_PPID_WEBRTC_STRING_EMPTY;
  } else {
    gchar *str_copy;
    size = strlen (str);

    if (!_is_within_max_message_size (channel, size)) {
      g_set_error (error, GST_WEBRTC_ERROR, GST_WEBRTC_ERROR_TYPE_ERROR,
          "Requested to send a string that is too large");
      return FALSE;
    }

    str_copy = g_strdup (str);
    buffer =
        gst_buffer_new_wrapped_full (GST_MEMORY_FLAG_READONLY, str_copy,
        size, 0, size, str_copy, g_free);
    ppid = DATA_CHANNEL_PPID_WEBRTC_STRING;
  }

  _get_sctp_reliability (channel, &reliability, &rel_param);
  gst_sctp_buffer_add_send_meta (buffer, ppid, channel->parent.ordered,
      reliability, rel_param);

  GST_TRACE_OBJECT (channel, "Sending string using buffer %" GST_PTR_FORMAT,
      buffer);

  GST_WEBRTC_DATA_CHANNEL_LOCK (channel);
  if (channel->parent.ready_state == GST_WEBRTC_DATA_CHANNEL_STATE_OPEN) {
    channel->parent.buffered_amount += size;
  } else {
    GST_WEBRTC_DATA_CHANNEL_UNLOCK (channel);
    g_set_error (error, GST_WEBRTC_ERROR,
        GST_WEBRTC_ERROR_INVALID_STATE, "channel is not open");
    gst_buffer_unref (buffer);
    return FALSE;
  }
  GST_WEBRTC_DATA_CHANNEL_UNLOCK (channel);

  ret = gst_app_src_push_buffer (GST_APP_SRC (channel->appsrc), buffer);
  if (ret == GST_FLOW_OK) {
    g_object_notify (G_OBJECT (&channel->parent), "buffered-amount");
  } else {
    g_set_error (error, GST_WEBRTC_ERROR,
        GST_WEBRTC_ERROR_DATA_CHANNEL_FAILURE, "Failed to send string");

    GST_WEBRTC_DATA_CHANNEL_LOCK (channel);
    channel->parent.buffered_amount -= size;
    GST_WEBRTC_DATA_CHANNEL_UNLOCK (channel);

    _channel_enqueue_task (channel, (ChannelTask) _close_procedure, NULL, NULL);
    return FALSE;
  }

  return TRUE;
}

static void
_on_sctp_notify_state_unlocked (GObject * sctp_transport,
    WebRTCDataChannel * channel)
{
  GstWebRTCSCTPTransportState state;

  g_object_get (sctp_transport, "state", &state, NULL);
  if (state == GST_WEBRTC_SCTP_TRANSPORT_STATE_CONNECTED) {
    if (channel->parent.negotiated)
      _channel_enqueue_task (channel, (ChannelTask) _emit_on_open, NULL, NULL);
  }
}

static WebRTCDataChannel *
ensure_channel_alive (WebRTCDataChannel * channel)
{
  /* ghetto impl of, does the channel still exist?.
   * Needed because g_signal_handler_disconnect*() will not disconnect any
   * running functions and _finalize() implementation can complete and
   * invalidate channel */
  G_LOCK (outstanding_channels_lock);
  if (g_list_find (outstanding_channels, channel)) {
    g_object_ref (channel);
  } else {
    G_UNLOCK (outstanding_channels_lock);
    return NULL;
  }
  G_UNLOCK (outstanding_channels_lock);

  return channel;
}

static void
_on_sctp_notify_state (GObject * sctp_transport, GParamSpec * pspec,
    WebRTCDataChannel * channel)
{
  if (!(channel = ensure_channel_alive (channel)))
    return;

  GST_WEBRTC_DATA_CHANNEL_LOCK (channel);
  _on_sctp_notify_state_unlocked (sctp_transport, channel);
  GST_WEBRTC_DATA_CHANNEL_UNLOCK (channel);

  g_object_unref (channel);
}

static void
_emit_low_threshold (WebRTCDataChannel * channel, gpointer user_data)
{
  gst_webrtc_data_channel_on_buffered_amount_low (GST_WEBRTC_DATA_CHANNEL
      (channel));
}

static GstPadProbeReturn
on_appsrc_data (GstPad * pad, GstPadProbeInfo * info, gpointer user_data)
{
  WebRTCDataChannel *channel = user_data;
  guint64 prev_amount;
  guint64 size = 0;

  if (GST_PAD_PROBE_INFO_TYPE (info) & (GST_PAD_PROBE_TYPE_BUFFER)) {
    GstBuffer *buffer = GST_PAD_PROBE_INFO_BUFFER (info);
    size = gst_buffer_get_size (buffer);
  } else if (GST_PAD_PROBE_INFO_TYPE (info) & GST_PAD_PROBE_TYPE_BUFFER_LIST) {
    GstBufferList *list = GST_PAD_PROBE_INFO_BUFFER_LIST (info);
    size = gst_buffer_list_calculate_size (list);
  } else if (GST_PAD_PROBE_INFO_TYPE (info) &
      GST_PAD_PROBE_TYPE_EVENT_DOWNSTREAM) {
    GstEvent *event = GST_PAD_PROBE_INFO_EVENT (info);
    if (GST_EVENT_TYPE (event) == GST_EVENT_EOS
        && channel->parent.ready_state ==
        GST_WEBRTC_DATA_CHANNEL_STATE_CLOSING) {
      _channel_enqueue_task (channel, (ChannelTask) _close_sctp_stream, NULL,
          NULL);
      return GST_PAD_PROBE_DROP;
    }
  }

  if (size > 0) {
    GST_WEBRTC_DATA_CHANNEL_LOCK (channel);
    prev_amount = channel->parent.buffered_amount;
    channel->parent.buffered_amount -= size;
    GST_TRACE_OBJECT (channel, "checking low-threshold: prev %"
        G_GUINT64_FORMAT " low-threshold %" G_GUINT64_FORMAT " buffered %"
        G_GUINT64_FORMAT, prev_amount,
        channel->parent.buffered_amount_low_threshold,
        channel->parent.buffered_amount);
    if (prev_amount >= channel->parent.buffered_amount_low_threshold
        && channel->parent.buffered_amount <=
        channel->parent.buffered_amount_low_threshold) {
      _channel_enqueue_task (channel, (ChannelTask) _emit_low_threshold, NULL,
          NULL);
    }

    GST_WEBRTC_DATA_CHANNEL_UNLOCK (channel);
    g_object_notify (G_OBJECT (&channel->parent), "buffered-amount");
  }

  return GST_PAD_PROBE_OK;
}

static void
gst_webrtc_data_channel_constructed (GObject * object)
{
  WebRTCDataChannel *channel;
  GstPad *pad;
  GstCaps *caps;

  G_OBJECT_CLASS (parent_class)->constructed (object);

  channel = WEBRTC_DATA_CHANNEL (object);
  GST_DEBUG ("New channel %p constructed", channel);

  caps = gst_caps_new_any ();

  channel->appsrc = gst_element_factory_make ("appsrc", NULL);
  gst_object_ref_sink (channel->appsrc);
  pad = gst_element_get_static_pad (channel->appsrc, "src");

  channel->src_probe = gst_pad_add_probe (pad, GST_PAD_PROBE_TYPE_DATA_BOTH,
      (GstPadProbeCallback) on_appsrc_data, channel, NULL);

  channel->src_bin = webrtc_error_ignore_bin_new (channel, channel->appsrc);

  channel->appsink = gst_element_factory_make ("appsink", NULL);
  gst_object_ref_sink (channel->appsink);
  g_object_set (channel->appsink, "sync", FALSE, "async", FALSE, "caps", caps,
      NULL);
  gst_app_sink_set_callbacks (GST_APP_SINK (channel->appsink), &sink_callbacks,
      channel, NULL);

  channel->sink_bin = webrtc_error_ignore_bin_new (channel, channel->appsink);

  gst_object_unref (pad);
  gst_caps_unref (caps);
}

static void
gst_webrtc_data_channel_dispose (GObject * object)
{
  G_LOCK (outstanding_channels_lock);
  outstanding_channels = g_list_remove (outstanding_channels, object);
  G_UNLOCK (outstanding_channels_lock);

  G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
gst_webrtc_data_channel_finalize (GObject * object)
{
  WebRTCDataChannel *channel = WEBRTC_DATA_CHANNEL (object);

  if (channel->src_probe) {
    GstPad *pad = gst_element_get_static_pad (channel->appsrc, "src");
    gst_pad_remove_probe (pad, channel->src_probe);
    gst_object_unref (pad);
    channel->src_probe = 0;
  }

  if (channel->sctp_transport)
    g_signal_handlers_disconnect_by_data (channel->sctp_transport, channel);
  g_clear_object (&channel->sctp_transport);

  g_clear_object (&channel->appsrc);
  g_clear_object (&channel->appsink);

  g_weak_ref_clear (&channel->webrtcbin_weak);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
webrtc_data_channel_class_init (WebRTCDataChannelClass * klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;
  GstWebRTCDataChannelClass *channel_class =
      (GstWebRTCDataChannelClass *) klass;

  gobject_class->constructed = gst_webrtc_data_channel_constructed;
  gobject_class->dispose = gst_webrtc_data_channel_dispose;
  gobject_class->finalize = gst_webrtc_data_channel_finalize;

  channel_class->send_data = webrtc_data_channel_send_data;
  channel_class->send_string = webrtc_data_channel_send_string;
  channel_class->close = webrtc_data_channel_close;
}

static void
webrtc_data_channel_init (WebRTCDataChannel * channel)
{
  G_LOCK (outstanding_channels_lock);
  outstanding_channels = g_list_prepend (outstanding_channels, channel);
  G_UNLOCK (outstanding_channels_lock);

  g_weak_ref_init (&channel->webrtcbin_weak, NULL);
}

static void
_data_channel_set_sctp_transport (WebRTCDataChannel * channel,
    WebRTCSCTPTransport * sctp)
{
  g_return_if_fail (GST_IS_WEBRTC_DATA_CHANNEL (channel));
  g_return_if_fail (GST_IS_WEBRTC_SCTP_TRANSPORT (sctp));

  GST_WEBRTC_DATA_CHANNEL_LOCK (channel);
  if (channel->sctp_transport)
    g_signal_handlers_disconnect_by_data (channel->sctp_transport, channel);
  GST_TRACE_OBJECT (channel, "set sctp %p", sctp);

  gst_object_replace ((GstObject **) & channel->sctp_transport,
      GST_OBJECT (sctp));

  if (sctp) {
    g_signal_connect (sctp, "stream-reset", G_CALLBACK (_on_sctp_stream_reset),
        channel);
    g_signal_connect (sctp, "notify::state", G_CALLBACK (_on_sctp_notify_state),
        channel);
  }
  GST_WEBRTC_DATA_CHANNEL_UNLOCK (channel);
}

void
webrtc_data_channel_link_to_sctp (WebRTCDataChannel * channel,
    WebRTCSCTPTransport * sctp_transport)
{
  if (sctp_transport && !channel->sctp_transport) {
    gint id;

    g_object_get (channel, "id", &id, NULL);

    if (sctp_transport->association_established && id != -1) {
      gchar *pad_name;

      _data_channel_set_sctp_transport (channel, sctp_transport);
      pad_name = g_strdup_printf ("sink_%u", id);
      if (!gst_element_link_pads (channel->src_bin, "src",
              channel->sctp_transport->sctpenc, pad_name))
        g_warn_if_reached ();
      g_free (pad_name);

      _on_sctp_notify_state_unlocked (G_OBJECT (sctp_transport), channel);
    }
  }
}

void
webrtc_data_channel_set_webrtcbin (WebRTCDataChannel * channel,
    GstWebRTCBin * webrtcbin)
{
  g_weak_ref_set (&channel->webrtcbin_weak, webrtcbin);
}
