/* GStreamer
 * Copyright (C) 2017 Matthew Waters <matthew@centricular.com>
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
 * SECTION:gstwebrtc-transceiver
 * @short_description: RTCRtpTransceiver object
 * @title: GstWebRTCRTPTransceiver
 * @see_also: #GstWebRTCRTPSender, #GstWebRTCRTPReceiver
 * @symbols:
 * - GstWebRTCRTPTransceiver
 *
 * <https://www.w3.org/TR/webrtc/#rtcrtptransceiver-interface>
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "rtptransceiver.h"

#include "webrtc-priv.h"

#define GST_CAT_DEFAULT gst_webrtc_rtp_transceiver_debug
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);

#define gst_webrtc_rtp_transceiver_parent_class parent_class
G_DEFINE_ABSTRACT_TYPE_WITH_CODE (GstWebRTCRTPTransceiver,
    gst_webrtc_rtp_transceiver, GST_TYPE_OBJECT,
    GST_DEBUG_CATEGORY_INIT (gst_webrtc_rtp_transceiver_debug,
        "webrtcrtptransceiver", 0, "webrtcrtptransceiver");
    );

enum
{
  SIGNAL_0,
  LAST_SIGNAL,
};

enum
{
  PROP_0,
  PROP_SENDER,
  PROP_RECEIVER,
  PROP_DIRECTION,
  PROP_MLINE,
  PROP_MID,
  PROP_CURRENT_DIRECTION,
  PROP_KIND,
  PROP_CODEC_PREFERENCES,
  PROP_STOPPED,                 // FIXME
};

//static guint gst_webrtc_rtp_transceiver_signals[LAST_SIGNAL] = { 0 };

static void
gst_webrtc_rtp_transceiver_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstWebRTCRTPTransceiver *webrtc = GST_WEBRTC_RTP_TRANSCEIVER (object);

  switch (prop_id) {
    case PROP_SENDER:
      webrtc->sender = g_value_dup_object (value);
      break;
    case PROP_RECEIVER:
      webrtc->receiver = g_value_dup_object (value);
      break;
    case PROP_MLINE:
      webrtc->mline = g_value_get_uint (value);
      break;
    case PROP_DIRECTION:
      GST_OBJECT_LOCK (webrtc);
      webrtc->direction = g_value_get_enum (value);
      GST_OBJECT_UNLOCK (webrtc);
      break;
    case PROP_CODEC_PREFERENCES:
      GST_OBJECT_LOCK (webrtc);
      gst_caps_replace (&webrtc->codec_preferences, g_value_get_boxed (value));
      GST_OBJECT_UNLOCK (webrtc);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_webrtc_rtp_transceiver_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstWebRTCRTPTransceiver *webrtc = GST_WEBRTC_RTP_TRANSCEIVER (object);

  switch (prop_id) {
    case PROP_MID:
      g_value_set_string (value, webrtc->mid);
      break;
    case PROP_SENDER:
      g_value_set_object (value, webrtc->sender);
      break;
    case PROP_RECEIVER:
      g_value_set_object (value, webrtc->receiver);
      break;
    case PROP_MLINE:
      g_value_set_uint (value, webrtc->mline);
      break;
    case PROP_DIRECTION:
      GST_OBJECT_LOCK (webrtc);
      g_value_set_enum (value, webrtc->direction);
      GST_OBJECT_UNLOCK (webrtc);
      break;
    case PROP_CURRENT_DIRECTION:
      g_value_set_enum (value, webrtc->current_direction);
      break;
    case PROP_KIND:
      g_value_set_enum (value, webrtc->kind);
      break;
    case PROP_CODEC_PREFERENCES:
      GST_OBJECT_LOCK (webrtc);
      gst_value_set_caps (value, webrtc->codec_preferences);
      GST_OBJECT_UNLOCK (webrtc);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_webrtc_rtp_transceiver_constructed (GObject * object)
{
  GstWebRTCRTPTransceiver *webrtc = GST_WEBRTC_RTP_TRANSCEIVER (object);

  gst_object_set_parent (GST_OBJECT (webrtc->sender), GST_OBJECT (webrtc));
  gst_object_set_parent (GST_OBJECT (webrtc->receiver), GST_OBJECT (webrtc));

  G_OBJECT_CLASS (parent_class)->constructed (object);
}

static void
gst_webrtc_rtp_transceiver_dispose (GObject * object)
{
  GstWebRTCRTPTransceiver *webrtc = GST_WEBRTC_RTP_TRANSCEIVER (object);

  if (webrtc->sender) {
    GST_OBJECT_PARENT (webrtc->sender) = NULL;
    gst_object_unref (webrtc->sender);
  }
  webrtc->sender = NULL;
  if (webrtc->receiver) {
    GST_OBJECT_PARENT (webrtc->receiver) = NULL;
    gst_object_unref (webrtc->receiver);
  }
  webrtc->receiver = NULL;

  G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
gst_webrtc_rtp_transceiver_finalize (GObject * object)
{
  GstWebRTCRTPTransceiver *webrtc = GST_WEBRTC_RTP_TRANSCEIVER (object);

  g_free (webrtc->mid);
  if (webrtc->codec_preferences)
    gst_caps_unref (webrtc->codec_preferences);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
gst_webrtc_rtp_transceiver_class_init (GstWebRTCRTPTransceiverClass * klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;

  gobject_class->get_property = gst_webrtc_rtp_transceiver_get_property;
  gobject_class->set_property = gst_webrtc_rtp_transceiver_set_property;
  gobject_class->constructed = gst_webrtc_rtp_transceiver_constructed;
  gobject_class->dispose = gst_webrtc_rtp_transceiver_dispose;
  gobject_class->finalize = gst_webrtc_rtp_transceiver_finalize;

  g_object_class_install_property (gobject_class,
      PROP_SENDER,
      g_param_spec_object ("sender", "Sender",
          "The RTP sender for this transceiver",
          GST_TYPE_WEBRTC_RTP_SENDER,
          G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class,
      PROP_RECEIVER,
      g_param_spec_object ("receiver", "Receiver",
          "The RTP receiver for this transceiver",
          GST_TYPE_WEBRTC_RTP_RECEIVER,
          G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class,
      PROP_MLINE,
      g_param_spec_uint ("mlineindex", "Media Line Index",
          "Index in the SDP of the Media",
          0, G_MAXUINT, 0,
          G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  /**
   * GstWebRTCRTPTransceiver:direction:
   *
   * Direction of the transceiver.
   *
   * Since: 1.18
   **/
  g_object_class_install_property (gobject_class,
      PROP_DIRECTION,
      g_param_spec_enum ("direction", "Direction",
          "Transceiver direction",
          GST_TYPE_WEBRTC_RTP_TRANSCEIVER_DIRECTION,
          GST_WEBRTC_RTP_TRANSCEIVER_DIRECTION_NONE,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  /**
   * GstWebRTCRTPTransceiver:mid:
   *
   * The media ID of the m-line associated with this transceiver. This
   * association is established, when possible, whenever either a
   * local or remote description is applied. This field is null if
   * neither a local or remote description has been applied, or if its
   * associated m-line is rejected by either a remote offer or any
   * answer.
   *
   * Since: 1.20
   */
  g_object_class_install_property (gobject_class,
      PROP_MID,
      g_param_spec_string ("mid", "Media ID",
          "The media ID of the m-line associated with this transceiver. This "
          " association is established, when possible, whenever either a local"
          " or remote description is applied. This field is null if neither a"
          " local or remote description has been applied, or if its associated"
          " m-line is rejected by either a remote offer or any answer.",
          NULL, G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  /**
   * GstWebRTCRTPTransceiver:current-direction:
   *
   * The transceiver's current directionality, or none if the
   * transceiver is stopped or has never participated in an exchange
   * of offers and answers. To change the transceiver's
   * directionality, set the value of the direction property.
   *
   * Since: 1.20
   **/
  g_object_class_install_property (gobject_class,
      PROP_DIRECTION,
      g_param_spec_enum ("current-direction", "Current Direction",
          "Transceiver current direction",
          GST_TYPE_WEBRTC_RTP_TRANSCEIVER_DIRECTION,
          GST_WEBRTC_RTP_TRANSCEIVER_DIRECTION_NONE,
          G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  /**
   * GstWebRTCRTPTransceiver:kind:
   *
   * The kind of media this transceiver transports
   *
   * Since: 1.20
   **/
  g_object_class_install_property (gobject_class,
      PROP_KIND,
      g_param_spec_enum ("kind", "Media Kind",
          "Kind of media this transceiver transports",
          GST_TYPE_WEBRTC_KIND, GST_WEBRTC_KIND_UNKNOWN,
          G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  /**
   * GstWebRTCRTPTransceiver:codec-preferences:
   *
   * Caps representing the codec preferences.
   *
   * Since: 1.20
   **/
  g_object_class_install_property (gobject_class,
      PROP_CODEC_PREFERENCES,
      g_param_spec_boxed ("codec-preferences", "Codec Preferences",
          "Caps representing the codec preferences.",
          GST_TYPE_CAPS, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
}

static void
gst_webrtc_rtp_transceiver_init (GstWebRTCRTPTransceiver * webrtc)
{
  webrtc->direction = GST_WEBRTC_RTP_TRANSCEIVER_DIRECTION_NONE;
}
