/* GStreamer
 * Copyright (C) 2017 Matthew Waters <matthew@centricular.com>
 * Copyright (C) 2020 Sebastian Dröge <sebastian@centricular.com>
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
 * @symbols:
 * - GstWebRTCDataChannel
 *
 * <https://www.w3.org/TR/webrtc/#rtcdatachannel>
 *
 * Since: 1.18
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "datachannel.h"
#include "webrtc-priv.h"

#define GST_CAT_DEFAULT gst_webrtc_data_channel_debug
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);

#define gst_webrtc_data_channel_parent_class parent_class
G_DEFINE_ABSTRACT_TYPE_WITH_CODE (GstWebRTCDataChannel, gst_webrtc_data_channel,
    G_TYPE_OBJECT, GST_DEBUG_CATEGORY_INIT (gst_webrtc_data_channel_debug,
        "webrtcdatachannel", 0, "webrtcdatachannel"););

enum
{
  SIGNAL_0,
  SIGNAL_ON_OPEN,
  SIGNAL_ON_CLOSE,
  SIGNAL_ON_ERROR,
  SIGNAL_ON_MESSAGE_DATA,
  SIGNAL_ON_MESSAGE_STRING,
  SIGNAL_ON_BUFFERED_AMOUNT_LOW,
  SIGNAL_SEND_DATA,
  SIGNAL_SEND_STRING,
  SIGNAL_CLOSE,
  LAST_SIGNAL,
};

enum
{
  PROP_0,
  PROP_LABEL,
  PROP_ORDERED,
  PROP_MAX_PACKET_LIFETIME,
  PROP_MAX_RETRANSMITS,
  PROP_PROTOCOL,
  PROP_NEGOTIATED,
  PROP_ID,
  PROP_PRIORITY,
  PROP_READY_STATE,
  PROP_BUFFERED_AMOUNT,
  PROP_BUFFERED_AMOUNT_LOW_THRESHOLD,
};

static guint gst_webrtc_data_channel_signals[LAST_SIGNAL] = { 0 };

static void
gst_webrtc_data_channel_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstWebRTCDataChannel *channel = GST_WEBRTC_DATA_CHANNEL (object);

  GST_WEBRTC_DATA_CHANNEL_LOCK (channel);
  switch (prop_id) {
    case PROP_LABEL:
      g_free (channel->label);
      channel->label = g_value_dup_string (value);
      break;
    case PROP_ORDERED:
      channel->ordered = g_value_get_boolean (value);
      break;
    case PROP_MAX_PACKET_LIFETIME:
      channel->max_packet_lifetime = g_value_get_int (value);
      break;
    case PROP_MAX_RETRANSMITS:
      channel->max_retransmits = g_value_get_int (value);
      break;
    case PROP_PROTOCOL:
      g_free (channel->protocol);
      channel->protocol = g_value_dup_string (value);
      break;
    case PROP_NEGOTIATED:
      channel->negotiated = g_value_get_boolean (value);
      break;
    case PROP_ID:
      channel->id = g_value_get_int (value);
      break;
    case PROP_PRIORITY:
      channel->priority = g_value_get_enum (value);
      break;
    case PROP_BUFFERED_AMOUNT_LOW_THRESHOLD:
      channel->buffered_amount_low_threshold = g_value_get_uint64 (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
  GST_WEBRTC_DATA_CHANNEL_UNLOCK (channel);
}

static void
gst_webrtc_data_channel_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstWebRTCDataChannel *channel = GST_WEBRTC_DATA_CHANNEL (object);

  GST_WEBRTC_DATA_CHANNEL_LOCK (channel);
  switch (prop_id) {
    case PROP_LABEL:
      g_value_set_string (value, channel->label);
      break;
    case PROP_ORDERED:
      g_value_set_boolean (value, channel->ordered);
      break;
    case PROP_MAX_PACKET_LIFETIME:
      g_value_set_int (value, channel->max_packet_lifetime);
      break;
    case PROP_MAX_RETRANSMITS:
      g_value_set_int (value, channel->max_retransmits);
      break;
    case PROP_PROTOCOL:
      g_value_set_string (value, channel->protocol);
      break;
    case PROP_NEGOTIATED:
      g_value_set_boolean (value, channel->negotiated);
      break;
    case PROP_ID:
      g_value_set_int (value, channel->id);
      break;
    case PROP_PRIORITY:
      g_value_set_enum (value, channel->priority);
      break;
    case PROP_READY_STATE:
      g_value_set_enum (value, channel->ready_state);
      break;
    case PROP_BUFFERED_AMOUNT:
      g_value_set_uint64 (value, channel->buffered_amount);
      break;
    case PROP_BUFFERED_AMOUNT_LOW_THRESHOLD:
      g_value_set_uint64 (value, channel->buffered_amount_low_threshold);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
  GST_WEBRTC_DATA_CHANNEL_UNLOCK (channel);
}

static void
gst_webrtc_data_channel_finalize (GObject * object)
{
  GstWebRTCDataChannel *channel = GST_WEBRTC_DATA_CHANNEL (object);

  g_free (channel->label);
  channel->label = NULL;

  g_free (channel->protocol);
  channel->protocol = NULL;

  g_mutex_clear (&channel->lock);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
gst_webrtc_data_channel_class_init (GstWebRTCDataChannelClass * klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;

  gobject_class->get_property = gst_webrtc_data_channel_get_property;
  gobject_class->set_property = gst_webrtc_data_channel_set_property;
  gobject_class->finalize = gst_webrtc_data_channel_finalize;

  g_object_class_install_property (gobject_class,
      PROP_LABEL,
      g_param_spec_string ("label",
          "Label", "Data channel label",
          NULL,
          G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class,
      PROP_ORDERED,
      g_param_spec_boolean ("ordered",
          "Ordered", "Using ordered transmission mode",
          FALSE,
          G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class,
      PROP_MAX_PACKET_LIFETIME,
      g_param_spec_int ("max-packet-lifetime",
          "Maximum Packet Lifetime",
          "Maximum number of milliseconds that transmissions and "
          "retransmissions may occur in unreliable mode (-1 = unset)",
          -1, G_MAXUINT16, -1,
          G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class,
      PROP_MAX_RETRANSMITS,
      g_param_spec_int ("max-retransmits",
          "Maximum Retransmits",
          "Maximum number of retransmissions attempted in unreliable mode",
          -1, G_MAXUINT16, 0,
          G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class,
      PROP_PROTOCOL,
      g_param_spec_string ("protocol",
          "Protocol", "Data channel protocol",
          "",
          G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class,
      PROP_NEGOTIATED,
      g_param_spec_boolean ("negotiated",
          "Negotiated",
          "Whether this data channel was negotiated by the application", FALSE,
          G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class,
      PROP_ID,
      g_param_spec_int ("id",
          "ID",
          "ID negotiated by this data channel (-1 = unset)",
          -1, G_MAXUINT16, -1,
          G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class,
      PROP_PRIORITY,
      g_param_spec_enum ("priority",
          "Priority",
          "The priority of data sent using this data channel",
          GST_TYPE_WEBRTC_PRIORITY_TYPE,
          GST_WEBRTC_PRIORITY_TYPE_LOW,
          G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class,
      PROP_READY_STATE,
      g_param_spec_enum ("ready-state",
          "Ready State",
          "The Ready state of this data channel",
          GST_TYPE_WEBRTC_DATA_CHANNEL_STATE,
          GST_WEBRTC_DATA_CHANNEL_STATE_CONNECTING,
          G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class,
      PROP_BUFFERED_AMOUNT,
      g_param_spec_uint64 ("buffered-amount",
          "Buffered Amount",
          "The amount of data in bytes currently buffered",
          0, G_MAXUINT64, 0, G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class,
      PROP_BUFFERED_AMOUNT_LOW_THRESHOLD,
      g_param_spec_uint64 ("buffered-amount-low-threshold",
          "Buffered Amount Low Threshold",
          "The threshold at which the buffered amount is considered low and "
          "the buffered-amount-low signal is emitted",
          0, G_MAXUINT64, 0, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  /**
   * GstWebRTCDataChannel::on-open:
   * @object: the #GstWebRTCDataChannel
   */
  gst_webrtc_data_channel_signals[SIGNAL_ON_OPEN] =
      g_signal_new ("on-open", G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_RUN_LAST, 0, NULL, NULL, NULL, G_TYPE_NONE, 0);

  /**
   * GstWebRTCDataChannel::on-close:
   * @object: the #GstWebRTCDataChannel
   */
  gst_webrtc_data_channel_signals[SIGNAL_ON_CLOSE] =
      g_signal_new ("on-close", G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_RUN_LAST, 0, NULL, NULL, NULL, G_TYPE_NONE, 0);

  /**
   * GstWebRTCDataChannel::on-error:
   * @object: the #GstWebRTCDataChannel
   * @error: the #GError thrown
   */
  gst_webrtc_data_channel_signals[SIGNAL_ON_ERROR] =
      g_signal_new ("on-error", G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_RUN_LAST, 0, NULL, NULL, NULL, G_TYPE_NONE, 1, G_TYPE_ERROR);

  /**
   * GstWebRTCDataChannel::on-message-data:
   * @object: the #GstWebRTCDataChannel
   * @data: (nullable): a #GBytes of the data received
   */
  gst_webrtc_data_channel_signals[SIGNAL_ON_MESSAGE_DATA] =
      g_signal_new ("on-message-data", G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_RUN_LAST, 0, NULL, NULL, NULL, G_TYPE_NONE, 1, G_TYPE_BYTES);

  /**
   * GstWebRTCDataChannel::on-message-string:
   * @object: the #GstWebRTCDataChannel
   * @data: (nullable): the data received as a string
   */
  gst_webrtc_data_channel_signals[SIGNAL_ON_MESSAGE_STRING] =
      g_signal_new ("on-message-string", G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_RUN_LAST, 0, NULL, NULL, NULL, G_TYPE_NONE, 1, G_TYPE_STRING);

  /**
   * GstWebRTCDataChannel::on-buffered-amount-low:
   * @object: the #GstWebRTCDataChannel
   */
  gst_webrtc_data_channel_signals[SIGNAL_ON_BUFFERED_AMOUNT_LOW] =
      g_signal_new ("on-buffered-amount-low", G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_RUN_LAST, 0, NULL, NULL, NULL, G_TYPE_NONE, 0);

  /**
   * GstWebRTCDataChannel::send-data:
   * @object: the #GstWebRTCDataChannel
   * @data: (nullable): a #GBytes with the data
   */
  gst_webrtc_data_channel_signals[SIGNAL_SEND_DATA] =
      g_signal_new_class_handler ("send-data", G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION | G_SIGNAL_DEPRECATED,
      G_CALLBACK (gst_webrtc_data_channel_send_data), NULL, NULL, NULL,
      G_TYPE_NONE, 1, G_TYPE_BYTES);

  /**
   * GstWebRTCDataChannel::send-string:
   * @object: the #GstWebRTCDataChannel
   * @data: (nullable): the data to send as a string
   */
  gst_webrtc_data_channel_signals[SIGNAL_SEND_STRING] =
      g_signal_new_class_handler ("send-string", G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
      G_CALLBACK (gst_webrtc_data_channel_send_string), NULL, NULL, NULL,
      G_TYPE_NONE, 1, G_TYPE_STRING);

  /**
   * GstWebRTCDataChannel::close:
   * @object: the #GstWebRTCDataChannel
   *
   * Close the data channel
   */
  gst_webrtc_data_channel_signals[SIGNAL_CLOSE] =
      g_signal_new_class_handler ("close", G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
      G_CALLBACK (gst_webrtc_data_channel_close), NULL, NULL, NULL,
      G_TYPE_NONE, 0);
}

static void
gst_webrtc_data_channel_init (GstWebRTCDataChannel * channel)
{
  g_mutex_init (&channel->lock);
}

/**
 * gst_webrtc_data_channel_on_open:
 * @channel: a #GstWebRTCDataChannel
 *
 * Signal that the data channel was opened. Should only be used by subclasses.
 */
void
gst_webrtc_data_channel_on_open (GstWebRTCDataChannel * channel)
{
  g_return_if_fail (GST_IS_WEBRTC_DATA_CHANNEL (channel));

  GST_WEBRTC_DATA_CHANNEL_LOCK (channel);
  if (channel->ready_state == GST_WEBRTC_DATA_CHANNEL_STATE_CLOSING ||
      channel->ready_state == GST_WEBRTC_DATA_CHANNEL_STATE_CLOSED) {
    GST_WEBRTC_DATA_CHANNEL_UNLOCK (channel);
    return;
  }

  if (channel->ready_state != GST_WEBRTC_DATA_CHANNEL_STATE_OPEN) {
    channel->ready_state = GST_WEBRTC_DATA_CHANNEL_STATE_OPEN;
    GST_WEBRTC_DATA_CHANNEL_UNLOCK (channel);
    g_object_notify (G_OBJECT (channel), "ready-state");

    GST_INFO_OBJECT (channel, "We are open and ready for data!");
  } else {
    GST_WEBRTC_DATA_CHANNEL_UNLOCK (channel);
  }

  GST_INFO_OBJECT (channel, "Opened");

  g_signal_emit (channel, gst_webrtc_data_channel_signals[SIGNAL_ON_OPEN], 0,
      NULL);
}

/**
 * gst_webrtc_data_channel_on_close:
 * @channel: a #GstWebRTCDataChannel
 *
 * Signal that the data channel was closed. Should only be used by subclasses.
 */
void
gst_webrtc_data_channel_on_close (GstWebRTCDataChannel * channel)
{
  g_return_if_fail (GST_IS_WEBRTC_DATA_CHANNEL (channel));

  GST_INFO_OBJECT (channel, "Closed");

  GST_WEBRTC_DATA_CHANNEL_LOCK (channel);
  if (channel->ready_state == GST_WEBRTC_DATA_CHANNEL_STATE_CLOSED) {
    GST_WEBRTC_DATA_CHANNEL_UNLOCK (channel);
    return;
  }

  channel->ready_state = GST_WEBRTC_DATA_CHANNEL_STATE_CLOSED;
  GST_WEBRTC_DATA_CHANNEL_UNLOCK (channel);

  g_object_notify (G_OBJECT (channel), "ready-state");
  GST_INFO_OBJECT (channel, "We are closed for data");

  g_signal_emit (channel, gst_webrtc_data_channel_signals[SIGNAL_ON_CLOSE], 0,
      NULL);
}

/**
 * gst_webrtc_data_channel_on_error:
 * @channel: a #GstWebRTCDataChannel
 * @error: (transfer full): a #GError
 *
 * Signal that the data channel had an error. Should only be used by subclasses.
 */
void
gst_webrtc_data_channel_on_error (GstWebRTCDataChannel * channel,
    GError * error)
{
  g_return_if_fail (GST_IS_WEBRTC_DATA_CHANNEL (channel));
  g_return_if_fail (error != NULL);

  GST_WARNING_OBJECT (channel, "Error: %s", GST_STR_NULL (error->message));

  g_signal_emit (channel, gst_webrtc_data_channel_signals[SIGNAL_ON_ERROR], 0,
      error);
}

/**
 * gst_webrtc_data_channel_on_message_data:
 * @channel: a #GstWebRTCDataChannel
 * @data: (nullable): a #GBytes or %NULL
 *
 * Signal that the data channel received a data message. Should only be used by subclasses.
 */
void
gst_webrtc_data_channel_on_message_data (GstWebRTCDataChannel * channel,
    GBytes * data)
{
  g_return_if_fail (GST_IS_WEBRTC_DATA_CHANNEL (channel));

  GST_LOG_OBJECT (channel, "Have data %p", data);
  g_signal_emit (channel,
      gst_webrtc_data_channel_signals[SIGNAL_ON_MESSAGE_DATA], 0, data);
}

/**
 * gst_webrtc_data_channel_on_message_string:
 * @channel: a #GstWebRTCDataChannel
 * @str: (nullable): a string or %NULL
 *
 * Signal that the data channel received a string message. Should only be used by subclasses.
 */
void
gst_webrtc_data_channel_on_message_string (GstWebRTCDataChannel * channel,
    const gchar * str)
{
  g_return_if_fail (GST_IS_WEBRTC_DATA_CHANNEL (channel));

  GST_LOG_OBJECT (channel, "Have string %p", str);
  g_signal_emit (channel,
      gst_webrtc_data_channel_signals[SIGNAL_ON_MESSAGE_STRING], 0, str);
}

/**
 * gst_webrtc_data_channel_on_buffered_amount_low:
 * @channel: a #GstWebRTCDataChannel
 *
 * Signal that the data channel reached a low buffered amount. Should only be used by subclasses.
 */
void
gst_webrtc_data_channel_on_buffered_amount_low (GstWebRTCDataChannel * channel)
{
  g_return_if_fail (GST_IS_WEBRTC_DATA_CHANNEL (channel));

  GST_LOG_OBJECT (channel, "Low threshold reached");
  g_signal_emit (channel,
      gst_webrtc_data_channel_signals[SIGNAL_ON_BUFFERED_AMOUNT_LOW], 0);
}

/**
 * gst_webrtc_data_channel_send_data:
 * @channel: a #GstWebRTCDataChannel
 * @data: (nullable): a #GBytes or %NULL
 *
 * Send @data as a data message over @channel.
 */
void
gst_webrtc_data_channel_send_data (GstWebRTCDataChannel * channel,
    GBytes * data)
{
  GstWebRTCDataChannelClass *klass;

  g_return_if_fail (GST_IS_WEBRTC_DATA_CHANNEL (channel));

  klass = GST_WEBRTC_DATA_CHANNEL_GET_CLASS (channel);
  (void) klass->send_data (channel, data, NULL);
}

/**
 * gst_webrtc_data_channel_send_data_full:
 * @channel: a #GstWebRTCDataChannel
 * @data: (nullable): a #GBytes or %NULL
 * @error: (nullable): location to a #GError or %NULL
 *
 * Send @data as a data message over @channel.
 *
 * Returns: TRUE if @channel is open and data could be queued
 *
 * Since: 1.22
 */
gboolean
gst_webrtc_data_channel_send_data_full (GstWebRTCDataChannel * channel,
    GBytes * data, GError ** error)
{
  GstWebRTCDataChannelClass *klass;

  g_return_val_if_fail (GST_IS_WEBRTC_DATA_CHANNEL (channel), FALSE);

  klass = GST_WEBRTC_DATA_CHANNEL_GET_CLASS (channel);
  return klass->send_data (channel, data, error);
}

/**
 * gst_webrtc_data_channel_send_string:
 * @channel: a #GstWebRTCDataChannel
 * @str: (nullable): a string or %NULL
 *
 * Send @str as a string message over @channel.
 */
void
gst_webrtc_data_channel_send_string (GstWebRTCDataChannel * channel,
    const gchar * str)
{
  GstWebRTCDataChannelClass *klass;

  g_return_if_fail (GST_IS_WEBRTC_DATA_CHANNEL (channel));

  klass = GST_WEBRTC_DATA_CHANNEL_GET_CLASS (channel);
  (void) klass->send_string (channel, str, NULL);
}

/**
 * gst_webrtc_data_channel_send_string_full:
 * @channel: a #GstWebRTCDataChannel
 * @str: (nullable): a string or %NULL
 *
 * Send @str as a string message over @channel.
 *
 * Returns: TRUE if @channel is open and data could be queued
 *
 * Since: 1.22
 */
gboolean
gst_webrtc_data_channel_send_string_full (GstWebRTCDataChannel * channel,
    const gchar * str, GError ** error)
{
  GstWebRTCDataChannelClass *klass;

  g_return_val_if_fail (GST_IS_WEBRTC_DATA_CHANNEL (channel), FALSE);

  klass = GST_WEBRTC_DATA_CHANNEL_GET_CLASS (channel);
  return klass->send_string (channel, str, error);
}

/**
 * gst_webrtc_data_channel_close:
 * @channel: a #GstWebRTCDataChannel
 *
 * Close the @channel.
 */
void
gst_webrtc_data_channel_close (GstWebRTCDataChannel * channel)
{
  GstWebRTCDataChannelClass *klass;

  g_return_if_fail (GST_IS_WEBRTC_DATA_CHANNEL (channel));

  klass = GST_WEBRTC_DATA_CHANNEL_GET_CLASS (channel);
  klass->close (channel);
}
