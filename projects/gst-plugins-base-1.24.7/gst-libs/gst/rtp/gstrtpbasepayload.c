/* GStreamer
 * Copyright (C) <2005> Wim Taymans <wim@fluendo.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more
 */

/**
 * SECTION:gstrtpbasepayload
 * @title: GstRTPBasePayload
 * @short_description: Base class for RTP payloader
 *
 * Provides a base class for RTP payloaders
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>

#include <gst/rtp/gstrtpbuffer.h>

#include "gstrtpbasepayload.h"
#include "gstrtpmeta.h"
#include "gstrtphdrext.h"

GST_DEBUG_CATEGORY_STATIC (rtpbasepayload_debug);
#define GST_CAT_DEFAULT (rtpbasepayload_debug)

struct _GstRTPBasePayloadPrivate
{
  gboolean ts_offset_random;
  gboolean seqnum_offset_random;
  gboolean ssrc_random;
  guint16 next_seqnum;
  gboolean perfect_rtptime;
  gint notified_first_timestamp;

  gboolean pt_set;

  gboolean source_info;
  GstBuffer *input_meta_buffer;

  guint64 base_offset;
  gint64 base_rtime;
  guint64 base_rtime_hz;
  guint64 running_time;
  gboolean scale_rtptime;
  gboolean auto_hdr_ext;

  gint64 prop_max_ptime;
  gint64 caps_max_ptime;

  gboolean onvif_no_rate_control;

  gboolean negotiated;

  /* We need to know whether negotiate was called in order to decide
   * whether we should store the input buffer as input meta in case
   * negotiate() gets called from the subclass' handle_buffer() implementation,
   * as negotiate() is where we instantiate header extensions.
   */
  gboolean negotiate_called;

  gboolean delay_segment;
  GstEvent *pending_segment;

  GstCaps *subclass_srccaps;
  GstCaps *sinkcaps;

  /* array of GstRTPHeaderExtension's * */
  GPtrArray *header_exts;
};

/* RTPBasePayload signals and args */
enum
{
  SIGNAL_0,
  SIGNAL_REQUEST_EXTENSION,
  SIGNAL_ADD_EXTENSION,
  SIGNAL_CLEAR_EXTENSIONS,
  LAST_SIGNAL
};

static guint gst_rtp_base_payload_signals[LAST_SIGNAL] = { 0 };

/* FIXME 0.11, a better default is the Ethernet MTU of
 * 1500 - sizeof(headers) as pointed out by marcelm in IRC:
 * So an Ethernet MTU of 1500, minus 60 for the max IP, minus 8 for UDP, gives
 * 1432 bytes or so.  And that should be adjusted downward further for other
 * encapsulations like PPPoE, so 1400 at most.
 */
#define DEFAULT_MTU                     1400
#define DEFAULT_PT                      96
#define DEFAULT_SSRC                    -1
#define DEFAULT_TIMESTAMP_OFFSET        -1
#define DEFAULT_SEQNUM_OFFSET           -1
#define DEFAULT_MAX_PTIME               -1
#define DEFAULT_MIN_PTIME               0
#define DEFAULT_PERFECT_RTPTIME         TRUE
#define DEFAULT_PTIME_MULTIPLE          0
#define DEFAULT_RUNNING_TIME            GST_CLOCK_TIME_NONE
#define DEFAULT_SOURCE_INFO             FALSE
#define DEFAULT_ONVIF_NO_RATE_CONTROL   FALSE
#define DEFAULT_SCALE_RTPTIME           TRUE
#define DEFAULT_AUTO_HEADER_EXTENSION   TRUE

#define RTP_HEADER_EXT_ONE_BYTE_MAX_SIZE 16
#define RTP_HEADER_EXT_TWO_BYTE_MAX_SIZE 255
#define RTP_HEADER_EXT_ONE_BYTE_MAX_ID 14
#define RTP_HEADER_EXT_TWO_BYTE_MAX_ID 255

enum
{
  PROP_0,
  PROP_MTU,
  PROP_PT,
  PROP_SSRC,
  PROP_TIMESTAMP_OFFSET,
  PROP_SEQNUM_OFFSET,
  PROP_MAX_PTIME,
  PROP_MIN_PTIME,
  PROP_TIMESTAMP,
  PROP_SEQNUM,
  PROP_PERFECT_RTPTIME,
  PROP_PTIME_MULTIPLE,
  PROP_STATS,
  PROP_SOURCE_INFO,
  PROP_ONVIF_NO_RATE_CONTROL,
  PROP_SCALE_RTPTIME,
  PROP_AUTO_HEADER_EXTENSION,
  PROP_EXTENSIONS,
  PROP_LAST
};

static GParamSpec *gst_rtp_base_payload_extensions_pspec;

static void gst_rtp_base_payload_class_init (GstRTPBasePayloadClass * klass);
static void gst_rtp_base_payload_init (GstRTPBasePayload * rtpbasepayload,
    gpointer g_class);
static void gst_rtp_base_payload_finalize (GObject * object);

static GstCaps *gst_rtp_base_payload_getcaps_default (GstRTPBasePayload *
    rtpbasepayload, GstPad * pad, GstCaps * filter);

static gboolean gst_rtp_base_payload_sink_event_default (GstRTPBasePayload *
    rtpbasepayload, GstEvent * event);
static gboolean gst_rtp_base_payload_sink_event (GstPad * pad,
    GstObject * parent, GstEvent * event);
static gboolean gst_rtp_base_payload_src_event_default (GstRTPBasePayload *
    rtpbasepayload, GstEvent * event);
static gboolean gst_rtp_base_payload_src_event (GstPad * pad,
    GstObject * parent, GstEvent * event);
static gboolean gst_rtp_base_payload_query_default (GstRTPBasePayload *
    rtpbasepayload, GstPad * pad, GstQuery * query);
static gboolean gst_rtp_base_payload_query (GstPad * pad, GstObject * parent,
    GstQuery * query);
static GstFlowReturn gst_rtp_base_payload_chain (GstPad * pad,
    GstObject * parent, GstBuffer * buffer);

static void gst_rtp_base_payload_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_rtp_base_payload_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);

static GstStateChangeReturn gst_rtp_base_payload_change_state (GstElement *
    element, GstStateChange transition);

static gboolean gst_rtp_base_payload_negotiate (GstRTPBasePayload * payload);

static void gst_rtp_base_payload_add_extension (GstRTPBasePayload * payload,
    GstRTPHeaderExtension * ext);
static void gst_rtp_base_payload_clear_extensions (GstRTPBasePayload * payload);
static void gst_rtp_base_payload_get_extensions (GstRTPBasePayload * payload,
    GValue * out_value);

static GstElementClass *parent_class = NULL;
static gint private_offset = 0;

GType
gst_rtp_base_payload_get_type (void)
{
  static GType rtpbasepayload_type = 0;

  if (g_once_init_enter ((gsize *) & rtpbasepayload_type)) {
    static const GTypeInfo rtpbasepayload_info = {
      sizeof (GstRTPBasePayloadClass),
      NULL,
      NULL,
      (GClassInitFunc) gst_rtp_base_payload_class_init,
      NULL,
      NULL,
      sizeof (GstRTPBasePayload),
      0,
      (GInstanceInitFunc) gst_rtp_base_payload_init,
    };
    GType _type;

    _type = g_type_register_static (GST_TYPE_ELEMENT, "GstRTPBasePayload",
        &rtpbasepayload_info, G_TYPE_FLAG_ABSTRACT);

    private_offset =
        g_type_add_instance_private (_type, sizeof (GstRTPBasePayloadPrivate));

    g_once_init_leave ((gsize *) & rtpbasepayload_type, _type);
  }
  return rtpbasepayload_type;
}

static inline GstRTPBasePayloadPrivate *
gst_rtp_base_payload_get_instance_private (GstRTPBasePayload * self)
{
  return (G_STRUCT_MEMBER_P (self, private_offset));
}

static GstRTPHeaderExtension *
gst_rtp_base_payload_request_extension_default (GstRTPBasePayload * payload,
    guint ext_id, const gchar * uri)
{
  GstRTPHeaderExtension *ext = NULL;

  if (!payload->priv->auto_hdr_ext)
    return NULL;

  ext = gst_rtp_header_extension_create_from_uri (uri);
  if (ext) {
    GST_DEBUG_OBJECT (payload,
        "Automatically enabled extension %s for uri \'%s\'",
        GST_ELEMENT_NAME (ext), uri);

    gst_rtp_header_extension_set_id (ext, ext_id);
  } else {
    GST_DEBUG_OBJECT (payload,
        "Didn't find any extension implementing uri \'%s\'", uri);
  }

  return ext;
}

static gboolean
extension_accumulator (GSignalInvocationHint * ihint,
    GValue * return_accu, const GValue * handler_return, gpointer data)
{
  gpointer ext;

  /* Call default handler if user callback didn't create the extension */
  ext = g_value_get_object (handler_return);
  if (!ext)
    return TRUE;

  g_value_set_object (return_accu, ext);
  return FALSE;
}

static void
gst_rtp_base_payload_class_init (GstRTPBasePayloadClass * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *gstelement_class;

  gobject_class = (GObjectClass *) klass;
  gstelement_class = (GstElementClass *) klass;

  if (private_offset != 0)
    g_type_class_adjust_private_offset (klass, &private_offset);

  parent_class = g_type_class_peek_parent (klass);

  gobject_class->finalize = gst_rtp_base_payload_finalize;

  gobject_class->set_property = gst_rtp_base_payload_set_property;
  gobject_class->get_property = gst_rtp_base_payload_get_property;

  g_object_class_install_property (G_OBJECT_CLASS (klass), PROP_MTU,
      g_param_spec_uint ("mtu", "MTU",
          "Maximum size of one packet",
          28, G_MAXUINT, DEFAULT_MTU,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (G_OBJECT_CLASS (klass), PROP_PT,
      g_param_spec_uint ("pt", "payload type",
          "The payload type of the packets", 0, 0x7f, DEFAULT_PT,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (G_OBJECT_CLASS (klass), PROP_SSRC,
      g_param_spec_uint ("ssrc", "SSRC",
          "The SSRC of the packets (default == random)", 0, G_MAXUINT32,
          DEFAULT_SSRC, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (G_OBJECT_CLASS (klass),
      PROP_TIMESTAMP_OFFSET, g_param_spec_uint ("timestamp-offset",
          "Timestamp Offset",
          "Offset to add to all outgoing timestamps (default = random)", 0,
          G_MAXUINT32, DEFAULT_TIMESTAMP_OFFSET,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (G_OBJECT_CLASS (klass), PROP_SEQNUM_OFFSET,
      g_param_spec_int ("seqnum-offset", "Sequence number Offset",
          "Offset to add to all outgoing seqnum (-1 = random)", -1, G_MAXUINT16,
          DEFAULT_SEQNUM_OFFSET, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (G_OBJECT_CLASS (klass), PROP_MAX_PTIME,
      g_param_spec_int64 ("max-ptime", "Max packet time",
          "Maximum duration of the packet data in ns (-1 = unlimited up to MTU)",
          -1, G_MAXINT64, DEFAULT_MAX_PTIME,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  /**
   * GstRTPBasePayload:min-ptime:
   *
   * Minimum duration of the packet data in ns (can't go above MTU)
   **/
  g_object_class_install_property (G_OBJECT_CLASS (klass), PROP_MIN_PTIME,
      g_param_spec_int64 ("min-ptime", "Min packet time",
          "Minimum duration of the packet data in ns (can't go above MTU)",
          0, G_MAXINT64, DEFAULT_MIN_PTIME,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (G_OBJECT_CLASS (klass), PROP_TIMESTAMP,
      g_param_spec_uint ("timestamp", "Timestamp",
          "The RTP timestamp of the last processed packet",
          0, G_MAXUINT32, 0, G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (G_OBJECT_CLASS (klass), PROP_SEQNUM,
      g_param_spec_uint ("seqnum", "Sequence number",
          "The RTP sequence number of the last processed packet",
          0, G_MAXUINT16, 0, G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  /**
   * GstRTPBasePayload:perfect-rtptime:
   *
   * Try to use the offset fields to generate perfect RTP timestamps. When this
   * option is disabled, RTP timestamps are generated from GST_BUFFER_PTS of
   * each payloaded buffer. The PTSes of buffers may not necessarily increment
   * with the amount of data in each input buffer, consider e.g. the case where
   * the buffer arrives from a network which means that the PTS is unrelated to
   * the amount of data. Because the RTP timestamps are generated from
   * GST_BUFFER_PTS this can result in RTP timestamps that also don't increment
   * with the amount of data in the payloaded packet. To circumvent this it is
   * possible to set the perfect rtptime option enabled. When this option is
   * enabled the payloader will increment the RTP timestamps based on
   * GST_BUFFER_OFFSET which relates to the amount of data in each packet
   * rather than the GST_BUFFER_PTS of each buffer and therefore the RTP
   * timestamps will more closely correlate with the amount of data in each
   * buffer. Currently GstRTPBasePayload is limited to handling perfect RTP
   * timestamps for audio streams.
   */
  g_object_class_install_property (G_OBJECT_CLASS (klass), PROP_PERFECT_RTPTIME,
      g_param_spec_boolean ("perfect-rtptime", "Perfect RTP Time",
          "Generate perfect RTP timestamps when possible",
          DEFAULT_PERFECT_RTPTIME, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  /**
   * GstRTPBasePayload:ptime-multiple:
   *
   * Force buffers to be multiples of this duration in ns (0 disables)
   **/
  g_object_class_install_property (G_OBJECT_CLASS (klass), PROP_PTIME_MULTIPLE,
      g_param_spec_int64 ("ptime-multiple", "Packet time multiple",
          "Force buffers to be multiples of this duration in ns (0 disables)",
          0, G_MAXINT64, DEFAULT_PTIME_MULTIPLE,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  /**
   * GstRTPBasePayload:stats:
   *
   * Various payloader statistics retrieved atomically (and are therefore
   * synchroized with each other), these can be used e.g. to generate an
   * RTP-Info header. This property return a GstStructure named
   * application/x-rtp-payload-stats containing the following fields relating to
   * the last processed buffer and current state of the stream being payloaded:
   *
   *   * `clock-rate` :#G_TYPE_UINT, clock-rate of the stream
   *   * `running-time` :#G_TYPE_UINT64, running time
   *   * `seqnum` :#G_TYPE_UINT, sequence number, same as #GstRTPBasePayload:seqnum
   *   * `timestamp` :#G_TYPE_UINT, RTP timestamp, same as #GstRTPBasePayload:timestamp
   *   * `ssrc` :#G_TYPE_UINT, The SSRC in use
   *   * `pt` :#G_TYPE_UINT, The Payload type in use, same as #GstRTPBasePayload:pt
   *   * `seqnum-offset` :#G_TYPE_UINT, The current offset added to the seqnum
   *   * `timestamp-offset` :#G_TYPE_UINT, The current offset added to the timestamp
   **/
  g_object_class_install_property (G_OBJECT_CLASS (klass), PROP_STATS,
      g_param_spec_boxed ("stats", "Statistics", "Various statistics",
          GST_TYPE_STRUCTURE, G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  /**
   * GstRTPBasePayload:source-info:
   *
   * Enable writing the CSRC field in allocated RTP header based on RTP source
   * information found in the input buffer's #GstRTPSourceMeta.
   *
   * Since: 1.16
   **/
  g_object_class_install_property (gobject_class, PROP_SOURCE_INFO,
      g_param_spec_boolean ("source-info", "RTP source information",
          "Write CSRC based on buffer meta RTP source information",
          DEFAULT_SOURCE_INFO, G_PARAM_READWRITE));

  /**
   * GstRTPBasePayload:onvif-no-rate-control:
   *
   * Make the payloader timestamp packets according to the Rate-Control=no
   * behaviour specified in the ONVIF replay spec.
   *
   * Since: 1.16
   */
  g_object_class_install_property (G_OBJECT_CLASS (klass),
      PROP_ONVIF_NO_RATE_CONTROL, g_param_spec_boolean ("onvif-no-rate-control",
          "ONVIF no rate control",
          "Enable ONVIF Rate-Control=no timestamping mode",
          DEFAULT_ONVIF_NO_RATE_CONTROL,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  /**
   * GstRTPBasePayload:scale-rtptime:
   *
   * Make the RTP packets' timestamps be scaled with the segment's rate
   * (corresponding to RTSP speed parameter). Disabling this property means
   * the timestamps will not be affected by the set delivery speed (RTSP speed).
   *
   * Example: A server wants to allow streaming a recorded video in double
   * speed but still have the timestamps correspond to the position in the
   * video. This is achieved by the client setting RTSP Speed to 2 while the
   * server has this property disabled.
   *
   * Since: 1.18
   */
  g_object_class_install_property (G_OBJECT_CLASS (klass), PROP_SCALE_RTPTIME,
      g_param_spec_boolean ("scale-rtptime", "Scale RTP time",
          "Whether the RTP timestamp should be scaled with the rate (speed)",
          DEFAULT_SCALE_RTPTIME, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  /**
   * GstRTPBasePayload:auto-header-extension:
   *
   * If enabled, the payloader will automatically try to enable all the
   * RTP header extensions provided in the src caps, saving the application
   * the need to handle these extensions manually using the
   * GstRTPBasePayload::request-extension: signal.
   *
   * Since: 1.20
   */
  g_object_class_install_property (G_OBJECT_CLASS (klass),
      PROP_AUTO_HEADER_EXTENSION, g_param_spec_boolean ("auto-header-extension",
          "Automatic RTP header extension",
          "Whether RTP header extensions should be automatically enabled, if an implementation is available",
          DEFAULT_AUTO_HEADER_EXTENSION,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  gst_rtp_base_payload_extensions_pspec = gst_param_spec_array ("extensions",
      "RTP header extensions",
      "A list of already enabled RTP header extensions",
      g_param_spec_object ("extension", "RTP header extension",
          "An already enabled RTP extension", GST_TYPE_RTP_HEADER_EXTENSION,
          G_PARAM_READABLE | G_PARAM_STATIC_STRINGS),
      G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  /**
   * GstRTPBasePayload:extensions:
   *
   * A list of already enabled RTP header extensions. This may be useful for finding
   * out which extensions are already enabled (with add-extension signal) and picking a non-conflicting
   * ID for a new extension that needs to be added on top of the existing ones.
   *
   * Note that the value returned by reading this property is not dynamically updated when the set of
   * enabled extensions changes by any of existing action signals. Rather, it represents the current state
   * at the time the property is read.
   *
   * Dynamic updates of this property can be received by subscribing to its corresponding "notify" signal, i.e.
   * "notify::extensions".
   *
   * Since: 1.24
   */
  g_object_class_install_property (G_OBJECT_CLASS (klass),
      PROP_EXTENSIONS, gst_rtp_base_payload_extensions_pspec);

  /**
   * GstRTPBasePayload::add-extension:
   * @object: the #GstRTPBasePayload
   * @ext: (transfer full): the #GstRTPHeaderExtension
   *
   * Add @ext as an extension for writing part of an RTP header extension onto
   * outgoing RTP packets.
   *
   * Since: 1.20
   */
  gst_rtp_base_payload_signals[SIGNAL_ADD_EXTENSION] =
      g_signal_new_class_handler ("add-extension", G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
      G_CALLBACK (gst_rtp_base_payload_add_extension), NULL, NULL, NULL,
      G_TYPE_NONE, 1, GST_TYPE_RTP_HEADER_EXTENSION);

  /**
   * GstRTPBasePayload::request-extension:
   * @object: the #GstRTPBasePayload
   * @ext_id: the extension id being requested
   * @ext_uri: the extension URI being requested
   *
   * The returned @ext must be configured with the correct @ext_id and with the
   * necessary attributes as required by the extension implementation.
   *
   * Returns: (transfer full) (nullable): the #GstRTPHeaderExtension for @ext_id, or %NULL
   *
   * Since: 1.20
   */
  gst_rtp_base_payload_signals[SIGNAL_REQUEST_EXTENSION] =
      g_signal_new_class_handler ("request-extension",
      G_TYPE_FROM_CLASS (klass), G_SIGNAL_RUN_LAST,
      G_CALLBACK (gst_rtp_base_payload_request_extension_default),
      extension_accumulator, NULL, NULL,
      GST_TYPE_RTP_HEADER_EXTENSION, 2, G_TYPE_UINT, G_TYPE_STRING);

  /**
   * GstRTPBasePayload::clear-extensions:
   * @object: the #GstRTPBasePayload
   *
   * Clear all RTP header extensions used by this payloader.
   *
   * Since: 1.20
   */
  gst_rtp_base_payload_signals[SIGNAL_CLEAR_EXTENSIONS] =
      g_signal_new_class_handler ("clear-extensions", G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
      G_CALLBACK (gst_rtp_base_payload_clear_extensions), NULL, NULL, NULL,
      G_TYPE_NONE, 0);

  gstelement_class->change_state = gst_rtp_base_payload_change_state;

  klass->get_caps = gst_rtp_base_payload_getcaps_default;
  klass->sink_event = gst_rtp_base_payload_sink_event_default;
  klass->src_event = gst_rtp_base_payload_src_event_default;
  klass->query = gst_rtp_base_payload_query_default;

  GST_DEBUG_CATEGORY_INIT (rtpbasepayload_debug, "rtpbasepayload", 0,
      "Base class for RTP Payloaders");
}

static void
gst_rtp_base_payload_init (GstRTPBasePayload * rtpbasepayload, gpointer g_class)
{
  GstPadTemplate *templ;
  GstRTPBasePayloadPrivate *priv;

  rtpbasepayload->priv = priv =
      gst_rtp_base_payload_get_instance_private (rtpbasepayload);

  templ =
      gst_element_class_get_pad_template (GST_ELEMENT_CLASS (g_class), "src");
  g_return_if_fail (templ != NULL);

  rtpbasepayload->srcpad = gst_pad_new_from_template (templ, "src");
  gst_pad_set_event_function (rtpbasepayload->srcpad,
      gst_rtp_base_payload_src_event);
  gst_element_add_pad (GST_ELEMENT (rtpbasepayload), rtpbasepayload->srcpad);

  templ =
      gst_element_class_get_pad_template (GST_ELEMENT_CLASS (g_class), "sink");
  g_return_if_fail (templ != NULL);

  rtpbasepayload->sinkpad = gst_pad_new_from_template (templ, "sink");
  gst_pad_set_chain_function (rtpbasepayload->sinkpad,
      gst_rtp_base_payload_chain);
  gst_pad_set_event_function (rtpbasepayload->sinkpad,
      gst_rtp_base_payload_sink_event);
  gst_pad_set_query_function (rtpbasepayload->sinkpad,
      gst_rtp_base_payload_query);
  gst_element_add_pad (GST_ELEMENT (rtpbasepayload), rtpbasepayload->sinkpad);

  rtpbasepayload->mtu = DEFAULT_MTU;
  rtpbasepayload->pt = DEFAULT_PT;
  rtpbasepayload->seqnum_offset = DEFAULT_SEQNUM_OFFSET;
  rtpbasepayload->ssrc = DEFAULT_SSRC;
  rtpbasepayload->ts_offset = DEFAULT_TIMESTAMP_OFFSET;
  priv->running_time = DEFAULT_RUNNING_TIME;
  priv->seqnum_offset_random = (rtpbasepayload->seqnum_offset == -1);
  priv->ts_offset_random = (rtpbasepayload->ts_offset == -1);
  priv->ssrc_random = (rtpbasepayload->ssrc == -1);
  priv->pt_set = FALSE;
  priv->source_info = DEFAULT_SOURCE_INFO;

  rtpbasepayload->max_ptime = DEFAULT_MAX_PTIME;
  rtpbasepayload->min_ptime = DEFAULT_MIN_PTIME;
  rtpbasepayload->priv->perfect_rtptime = DEFAULT_PERFECT_RTPTIME;
  rtpbasepayload->ptime_multiple = DEFAULT_PTIME_MULTIPLE;
  rtpbasepayload->priv->base_offset = GST_BUFFER_OFFSET_NONE;
  rtpbasepayload->priv->base_rtime_hz = GST_BUFFER_OFFSET_NONE;
  rtpbasepayload->priv->onvif_no_rate_control = DEFAULT_ONVIF_NO_RATE_CONTROL;
  rtpbasepayload->priv->scale_rtptime = DEFAULT_SCALE_RTPTIME;
  rtpbasepayload->priv->auto_hdr_ext = DEFAULT_AUTO_HEADER_EXTENSION;

  rtpbasepayload->media = NULL;
  rtpbasepayload->encoding_name = NULL;

  rtpbasepayload->clock_rate = 0;

  rtpbasepayload->priv->caps_max_ptime = DEFAULT_MAX_PTIME;
  rtpbasepayload->priv->prop_max_ptime = DEFAULT_MAX_PTIME;
  rtpbasepayload->priv->header_exts =
      g_ptr_array_new_with_free_func ((GDestroyNotify) gst_object_unref);
}

static void
gst_rtp_base_payload_finalize (GObject * object)
{
  GstRTPBasePayload *rtpbasepayload;

  rtpbasepayload = GST_RTP_BASE_PAYLOAD (object);

  g_free (rtpbasepayload->media);
  rtpbasepayload->media = NULL;
  g_free (rtpbasepayload->encoding_name);
  rtpbasepayload->encoding_name = NULL;

  gst_caps_replace (&rtpbasepayload->priv->subclass_srccaps, NULL);
  gst_caps_replace (&rtpbasepayload->priv->sinkcaps, NULL);

  g_ptr_array_unref (rtpbasepayload->priv->header_exts);
  rtpbasepayload->priv->header_exts = NULL;

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static GstCaps *
gst_rtp_base_payload_getcaps_default (GstRTPBasePayload * rtpbasepayload,
    GstPad * pad, GstCaps * filter)
{
  GstCaps *caps;

  caps = GST_PAD_TEMPLATE_CAPS (GST_PAD_PAD_TEMPLATE (pad));
  GST_DEBUG_OBJECT (pad,
      "using pad template %p with caps %p %" GST_PTR_FORMAT,
      GST_PAD_PAD_TEMPLATE (pad), caps, caps);

  if (filter)
    caps = gst_caps_intersect_full (filter, caps, GST_CAPS_INTERSECT_FIRST);
  else
    caps = gst_caps_ref (caps);

  return caps;
}

static gboolean
gst_rtp_base_payload_sink_event_default (GstRTPBasePayload * rtpbasepayload,
    GstEvent * event)
{
  GstObject *parent = GST_OBJECT_CAST (rtpbasepayload);
  gboolean res = FALSE;

  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_FLUSH_START:
      res = gst_pad_event_default (rtpbasepayload->sinkpad, parent, event);
      break;
    case GST_EVENT_FLUSH_STOP:
      res = gst_pad_event_default (rtpbasepayload->sinkpad, parent, event);
      gst_segment_init (&rtpbasepayload->segment, GST_FORMAT_UNDEFINED);
      gst_event_replace (&rtpbasepayload->priv->pending_segment, NULL);
      break;
    case GST_EVENT_CAPS:
    {
      GstRTPBasePayloadClass *rtpbasepayload_class;
      GstCaps *caps;

      gst_event_parse_caps (event, &caps);
      GST_DEBUG_OBJECT (rtpbasepayload, "setting caps %" GST_PTR_FORMAT, caps);

      gst_caps_replace (&rtpbasepayload->priv->sinkcaps, caps);

      rtpbasepayload_class = GST_RTP_BASE_PAYLOAD_GET_CLASS (rtpbasepayload);
      if (rtpbasepayload_class->set_caps)
        res = rtpbasepayload_class->set_caps (rtpbasepayload, caps);
      else
        res = gst_rtp_base_payload_negotiate (rtpbasepayload);

      rtpbasepayload->priv->negotiated = res;

      gst_event_unref (event);
      break;
    }
    case GST_EVENT_SEGMENT:
    {
      GstSegment *segment;

      segment = &rtpbasepayload->segment;
      gst_event_copy_segment (event, segment);

      rtpbasepayload->priv->base_offset = GST_BUFFER_OFFSET_NONE;

      GST_DEBUG_OBJECT (rtpbasepayload,
          "configured SEGMENT %" GST_SEGMENT_FORMAT, segment);
      if (rtpbasepayload->priv->delay_segment) {
        gst_event_replace (&rtpbasepayload->priv->pending_segment, event);
        gst_event_unref (event);
        res = TRUE;
      } else {
        res = gst_pad_event_default (rtpbasepayload->sinkpad, parent, event);
      }
      break;
    }
    case GST_EVENT_GAP:
    {
      if (G_UNLIKELY (rtpbasepayload->priv->pending_segment)) {
        gst_pad_push_event (rtpbasepayload->srcpad,
            rtpbasepayload->priv->pending_segment);
        rtpbasepayload->priv->pending_segment = FALSE;
        rtpbasepayload->priv->delay_segment = FALSE;
      }
      res = gst_pad_event_default (rtpbasepayload->sinkpad, parent, event);
      break;
    }
    default:
      res = gst_pad_event_default (rtpbasepayload->sinkpad, parent, event);
      break;
  }
  return res;
}

static gboolean
gst_rtp_base_payload_sink_event (GstPad * pad, GstObject * parent,
    GstEvent * event)
{
  GstRTPBasePayload *rtpbasepayload;
  GstRTPBasePayloadClass *rtpbasepayload_class;
  gboolean res = FALSE;

  rtpbasepayload = GST_RTP_BASE_PAYLOAD (parent);
  rtpbasepayload_class = GST_RTP_BASE_PAYLOAD_GET_CLASS (rtpbasepayload);

  if (rtpbasepayload_class->sink_event)
    res = rtpbasepayload_class->sink_event (rtpbasepayload, event);
  else
    gst_event_unref (event);

  return res;
}

static gboolean
gst_rtp_base_payload_src_event_default (GstRTPBasePayload * rtpbasepayload,
    GstEvent * event)
{
  GstObject *parent = GST_OBJECT_CAST (rtpbasepayload);
  gboolean res = TRUE, forward = TRUE;

  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_CUSTOM_UPSTREAM:
    {
      const GstStructure *s = gst_event_get_structure (event);

      if (gst_structure_has_name (s, "GstRTPCollision")) {
        guint ssrc = 0;

        if (!gst_structure_get_uint (s, "ssrc", &ssrc))
          ssrc = -1;

        GST_DEBUG_OBJECT (rtpbasepayload, "collided ssrc: %" G_GUINT32_FORMAT,
            ssrc);

        /* choose another ssrc for our stream */
        if (ssrc == rtpbasepayload->current_ssrc) {
          GstCaps *caps;
          guint suggested_ssrc = 0;

          if (gst_structure_get_uint (s, "suggested-ssrc", &suggested_ssrc))
            rtpbasepayload->current_ssrc = suggested_ssrc;

          while (ssrc == rtpbasepayload->current_ssrc)
            rtpbasepayload->current_ssrc = g_random_int ();

          caps = gst_pad_get_current_caps (rtpbasepayload->srcpad);
          if (caps) {
            caps = gst_caps_make_writable (caps);
            gst_caps_set_simple (caps,
                "ssrc", G_TYPE_UINT, rtpbasepayload->current_ssrc, NULL);
            res = gst_pad_set_caps (rtpbasepayload->srcpad, caps);
            gst_caps_unref (caps);
          }

          /* the event was for us */
          forward = FALSE;
        }
      }
      break;
    }
    default:
      break;
  }

  if (forward)
    res = gst_pad_event_default (rtpbasepayload->srcpad, parent, event);
  else
    gst_event_unref (event);

  return res;
}

static gboolean
gst_rtp_base_payload_src_event (GstPad * pad, GstObject * parent,
    GstEvent * event)
{
  GstRTPBasePayload *rtpbasepayload;
  GstRTPBasePayloadClass *rtpbasepayload_class;
  gboolean res = FALSE;

  rtpbasepayload = GST_RTP_BASE_PAYLOAD (parent);
  rtpbasepayload_class = GST_RTP_BASE_PAYLOAD_GET_CLASS (rtpbasepayload);

  if (rtpbasepayload_class->src_event)
    res = rtpbasepayload_class->src_event (rtpbasepayload, event);
  else
    gst_event_unref (event);

  return res;
}


static gboolean
gst_rtp_base_payload_query_default (GstRTPBasePayload * rtpbasepayload,
    GstPad * pad, GstQuery * query)
{
  gboolean res = FALSE;

  switch (GST_QUERY_TYPE (query)) {
    case GST_QUERY_CAPS:
    {
      GstRTPBasePayloadClass *rtpbasepayload_class;
      GstCaps *filter, *caps;

      gst_query_parse_caps (query, &filter);
      GST_DEBUG_OBJECT (rtpbasepayload, "getting caps with filter %"
          GST_PTR_FORMAT, filter);

      rtpbasepayload_class = GST_RTP_BASE_PAYLOAD_GET_CLASS (rtpbasepayload);
      if (rtpbasepayload_class->get_caps) {
        caps = rtpbasepayload_class->get_caps (rtpbasepayload, pad, filter);
        gst_query_set_caps_result (query, caps);
        gst_caps_unref (caps);
        res = TRUE;
      }
      break;
    }
    default:
      res =
          gst_pad_query_default (pad, GST_OBJECT_CAST (rtpbasepayload), query);
      break;
  }
  return res;
}

static gboolean
gst_rtp_base_payload_query (GstPad * pad, GstObject * parent, GstQuery * query)
{
  GstRTPBasePayload *rtpbasepayload;
  GstRTPBasePayloadClass *rtpbasepayload_class;
  gboolean res = FALSE;

  rtpbasepayload = GST_RTP_BASE_PAYLOAD (parent);
  rtpbasepayload_class = GST_RTP_BASE_PAYLOAD_GET_CLASS (rtpbasepayload);

  if (rtpbasepayload_class->query)
    res = rtpbasepayload_class->query (rtpbasepayload, pad, query);

  return res;
}

static GstFlowReturn
gst_rtp_base_payload_chain (GstPad * pad, GstObject * parent,
    GstBuffer * buffer)
{
  GstRTPBasePayload *rtpbasepayload;
  GstRTPBasePayloadClass *rtpbasepayload_class;
  GstFlowReturn ret;

  rtpbasepayload = GST_RTP_BASE_PAYLOAD (parent);
  rtpbasepayload_class = GST_RTP_BASE_PAYLOAD_GET_CLASS (rtpbasepayload);

  if (!rtpbasepayload_class->handle_buffer)
    goto no_function;

  if (!rtpbasepayload->priv->negotiated)
    goto not_negotiated;

  if (rtpbasepayload->priv->source_info
      || rtpbasepayload->priv->header_exts->len > 0
      || !rtpbasepayload->priv->negotiate_called) {
    /* Save a copy of meta (instead of taking an extra reference before
     * handle_buffer) to make the meta available when allocating a output
     * buffer. */
    rtpbasepayload->priv->input_meta_buffer = gst_buffer_new ();
    gst_buffer_copy_into (rtpbasepayload->priv->input_meta_buffer, buffer,
        GST_BUFFER_COPY_METADATA, 0, -1);
  }

  if (gst_pad_check_reconfigure (GST_RTP_BASE_PAYLOAD_SRCPAD (rtpbasepayload))) {
    if (!gst_rtp_base_payload_negotiate (rtpbasepayload)) {
      gst_pad_mark_reconfigure (GST_RTP_BASE_PAYLOAD_SRCPAD (rtpbasepayload));
      if (GST_PAD_IS_FLUSHING (GST_RTP_BASE_PAYLOAD_SRCPAD (rtpbasepayload))) {
        goto flushing;
      } else {
        goto negotiate_failed;
      }
    }
  }

  ret = rtpbasepayload_class->handle_buffer (rtpbasepayload, buffer);

  gst_buffer_replace (&rtpbasepayload->priv->input_meta_buffer, NULL);

  return ret;

  /* ERRORS */
no_function:
  {
    GST_ELEMENT_ERROR (rtpbasepayload, STREAM, NOT_IMPLEMENTED, (NULL),
        ("subclass did not implement handle_buffer function"));
    gst_buffer_unref (buffer);
    return GST_FLOW_ERROR;
  }
not_negotiated:
  {
    GST_ELEMENT_ERROR (rtpbasepayload, CORE, NEGOTIATION, (NULL),
        ("No input format was negotiated, i.e. no caps event was received. "
            "Perhaps you need a parser or typefind element before the payloader"));
    gst_buffer_unref (buffer);
    return GST_FLOW_NOT_NEGOTIATED;
  }
negotiate_failed:
  {
    GST_DEBUG_OBJECT (rtpbasepayload, "Not negotiated");
    gst_buffer_unref (buffer);
    return GST_FLOW_NOT_NEGOTIATED;
  }
flushing:
  {
    GST_DEBUG_OBJECT (rtpbasepayload, "we are flushing");
    gst_buffer_unref (buffer);
    return GST_FLOW_FLUSHING;
  }
}

/**
 * gst_rtp_base_payload_set_options:
 * @payload: a #GstRTPBasePayload
 * @media: the media type (typically "audio" or "video")
 * @dynamic: if the payload type is dynamic
 * @encoding_name: the encoding name
 * @clock_rate: the clock rate of the media
 *
 * Set the rtp options of the payloader. These options will be set in the caps
 * of the payloader. Subclasses must call this method before calling
 * gst_rtp_base_payload_push() or gst_rtp_base_payload_set_outcaps().
 */
void
gst_rtp_base_payload_set_options (GstRTPBasePayload * payload,
    const gchar * media, gboolean dynamic, const gchar * encoding_name,
    guint32 clock_rate)
{
  g_return_if_fail (payload != NULL);
  g_return_if_fail (clock_rate != 0);

  g_free (payload->media);
  payload->media = g_strdup (media);
  payload->dynamic = dynamic;
  g_free (payload->encoding_name);
  payload->encoding_name = g_strdup (encoding_name);
  payload->clock_rate = clock_rate;
}

static gboolean
copy_fixed (GQuark field_id, const GValue * value, GstStructure * dest)
{
  if (gst_value_is_fixed (value)) {
    gst_structure_id_set_value (dest, field_id, value);
  }
  return TRUE;
}

static void
update_max_ptime (GstRTPBasePayload * rtpbasepayload)
{
  if (rtpbasepayload->priv->caps_max_ptime != -1 &&
      rtpbasepayload->priv->prop_max_ptime != -1)
    rtpbasepayload->max_ptime = MIN (rtpbasepayload->priv->caps_max_ptime,
        rtpbasepayload->priv->prop_max_ptime);
  else if (rtpbasepayload->priv->caps_max_ptime != -1)
    rtpbasepayload->max_ptime = rtpbasepayload->priv->caps_max_ptime;
  else if (rtpbasepayload->priv->prop_max_ptime != -1)
    rtpbasepayload->max_ptime = rtpbasepayload->priv->prop_max_ptime;
  else
    rtpbasepayload->max_ptime = DEFAULT_MAX_PTIME;
}

static gboolean
_set_caps (GQuark field_id, const GValue * value, GstCaps * caps)
{
  gst_caps_set_value (caps, g_quark_to_string (field_id), value);

  return TRUE;
}

/**
 * gst_rtp_base_payload_set_outcaps_structure:
 * @payload: a #GstRTPBasePayload
 * @s: (nullable): a #GstStructure with the caps fields
 *
 * Configure the output caps with the optional fields.
 *
 * Returns: %TRUE if the caps could be set.
 *
 * Since: 1.20
 */
gboolean
gst_rtp_base_payload_set_outcaps_structure (GstRTPBasePayload * payload,
    GstStructure * s)
{
  GstCaps *srccaps;

  /* fill in the defaults, their properties cannot be negotiated. */
  srccaps = gst_caps_new_simple ("application/x-rtp",
      "media", G_TYPE_STRING, payload->media,
      "clock-rate", G_TYPE_INT, payload->clock_rate,
      "encoding-name", G_TYPE_STRING, payload->encoding_name, NULL);

  GST_DEBUG_OBJECT (payload, "defaults: %" GST_PTR_FORMAT, srccaps);

  if (s && gst_structure_n_fields (s) > 0) {
    gst_structure_foreach (s, (GstStructureForeachFunc) _set_caps, srccaps);

    GST_DEBUG_OBJECT (payload, "custom added: %" GST_PTR_FORMAT, srccaps);
  }

  gst_caps_replace (&payload->priv->subclass_srccaps, srccaps);
  gst_caps_unref (srccaps);

  return gst_rtp_base_payload_negotiate (payload);
}

/**
 * gst_rtp_base_payload_set_outcaps:
 * @payload: a #GstRTPBasePayload
 * @fieldname: the first field name or %NULL
 * @...: field values
 *
 * Configure the output caps with the optional parameters.
 *
 * Variable arguments should be in the form field name, field type
 * (as a GType), value(s).  The last variable argument should be NULL.
 *
 * Returns: %TRUE if the caps could be set.
 */
gboolean
gst_rtp_base_payload_set_outcaps (GstRTPBasePayload * payload,
    const gchar * fieldname, ...)
{
  gboolean result;
  GstStructure *s = NULL;

  if (fieldname) {
    va_list varargs;

    s = gst_structure_new_empty ("unused");

    /* override with custom properties */
    va_start (varargs, fieldname);
    gst_structure_set_valist (s, fieldname, varargs);
    va_end (varargs);
  }

  result = gst_rtp_base_payload_set_outcaps_structure (payload, s);

  gst_clear_structure (&s);

  return result;
}

static void
add_and_ref_item (GstRTPHeaderExtension * ext, GPtrArray * ret)
{
  g_ptr_array_add (ret, gst_object_ref (ext));
}

static void
remove_item_from (GstRTPHeaderExtension * ext, GPtrArray * ret)
{
  g_ptr_array_remove_fast (ret, ext);
}

static void
add_item_to (GstRTPHeaderExtension * ext, GPtrArray * ret)
{
  g_ptr_array_add (ret, ext);
}

static void
add_header_ext_to_caps (GstRTPHeaderExtension * ext, GstCaps * caps)
{
  if (!gst_rtp_header_extension_set_caps_from_attributes (ext, caps)) {
    GST_WARNING ("Failed to set caps from rtp header extension");
  }
}

static gboolean
gst_rtp_base_payload_negotiate (GstRTPBasePayload * payload)
{
  GstCaps *templ, *peercaps, *srccaps;
  GstStructure *s, *d;
  gboolean res = TRUE;

  payload->priv->caps_max_ptime = DEFAULT_MAX_PTIME;
  payload->ptime = 0;

  gst_pad_check_reconfigure (payload->srcpad);

  templ = gst_pad_get_pad_template_caps (payload->srcpad);

  if (payload->priv->subclass_srccaps) {
    GstCaps *tmp = gst_caps_intersect (payload->priv->subclass_srccaps,
        templ);
    gst_caps_unref (templ);
    templ = tmp;
  }

  peercaps = gst_pad_peer_query_caps (payload->srcpad, templ);

  if (peercaps == NULL) {
    /* no peer caps, just add the other properties */

    srccaps = gst_caps_copy (templ);
    gst_caps_set_simple (srccaps,
        "payload", G_TYPE_INT, GST_RTP_BASE_PAYLOAD_PT (payload),
        "ssrc", G_TYPE_UINT, payload->current_ssrc,
        "timestamp-offset", G_TYPE_UINT, payload->ts_base,
        "seqnum-offset", G_TYPE_UINT, payload->seqnum_base, NULL);

    GST_DEBUG_OBJECT (payload, "no peer caps: %" GST_PTR_FORMAT, srccaps);
  } else {
    GstCaps *temp;
    const GValue *value;
    gboolean have_pt = FALSE;
    gboolean have_ts_offset = FALSE;
    gboolean have_seqnum_offset = FALSE;
    guint max_ptime, ptime;

    /* peer provides caps we can use to fixate. They are already intersected
     * with our srccaps, just make them writable */
    temp = gst_caps_make_writable (peercaps);
    peercaps = NULL;

    if (gst_caps_is_empty (temp)) {
      gst_caps_unref (temp);
      gst_caps_unref (templ);
      res = FALSE;
      goto out;
    }

    /* We prefer the pt, timestamp-offset, seqnum-offset from the
     * property (if set), or any previously configured value over what
     * downstream prefers. Only if downstream can't accept that, or the
     * properties were not set, we fall back to choosing downstream's
     * preferred value
     *
     * For ssrc we prefer any value downstream suggests, otherwise
     * the property value or as a last resort a random value.
     * This difference for ssrc is implemented for retaining backwards
     * compatibility with changing rtpsession's internal-ssrc property.
     *
     * FIXME 2.0: All these properties should go away and be negotiated
     * via caps only!
     */

    /* try to use the previously set pt, or the one from the property */
    if (payload->priv->pt_set || gst_pad_has_current_caps (payload->srcpad)) {
      GstCaps *probe_caps = gst_caps_copy (templ);
      GstCaps *intersection;

      gst_caps_set_simple (probe_caps, "payload", G_TYPE_INT,
          GST_RTP_BASE_PAYLOAD_PT (payload), NULL);
      intersection = gst_caps_intersect (probe_caps, temp);

      if (!gst_caps_is_empty (intersection)) {
        GST_LOG_OBJECT (payload, "Using selected pt %d",
            GST_RTP_BASE_PAYLOAD_PT (payload));
        have_pt = TRUE;
        gst_caps_unref (temp);
        temp = intersection;
      } else {
        GST_WARNING_OBJECT (payload, "Can't use selected pt %d",
            GST_RTP_BASE_PAYLOAD_PT (payload));
        gst_caps_unref (intersection);
      }
      gst_caps_unref (probe_caps);
    }

    /* If we got no pt above, select one now */
    if (!have_pt) {
      gint pt;

      /* get first structure */
      s = gst_caps_get_structure (temp, 0);

      if (gst_structure_get_int (s, "payload", &pt)) {
        /* use peer pt */
        GST_RTP_BASE_PAYLOAD_PT (payload) = pt;
        GST_LOG_OBJECT (payload, "using peer pt %d", pt);
      } else {
        if (gst_structure_has_field (s, "payload")) {
          /* can only fixate if there is a field */
          gst_structure_fixate_field_nearest_int (s, "payload",
              GST_RTP_BASE_PAYLOAD_PT (payload));
          gst_structure_get_int (s, "payload", &pt);
          GST_RTP_BASE_PAYLOAD_PT (payload) = pt;
          GST_LOG_OBJECT (payload, "using peer pt %d", pt);
        } else {
          /* no pt field, use the internal pt */
          pt = GST_RTP_BASE_PAYLOAD_PT (payload);
          gst_structure_set (s, "payload", G_TYPE_INT, pt, NULL);
          GST_LOG_OBJECT (payload, "using internal pt %d", pt);
        }
      }
      s = NULL;
    }

    /* If we got no ssrc above, select one now */
    /* get first structure */
    s = gst_caps_get_structure (temp, 0);

    if (gst_structure_has_field_typed (s, "ssrc", G_TYPE_UINT)) {
      value = gst_structure_get_value (s, "ssrc");
      payload->current_ssrc = g_value_get_uint (value);
      GST_LOG_OBJECT (payload, "using peer ssrc %08x", payload->current_ssrc);
    } else {
      /* FIXME, fixate_nearest_uint would be even better but we
       * don't support uint ranges so how likely is it that anybody
       * uses a list of possible ssrcs */
      gst_structure_set (s, "ssrc", G_TYPE_UINT, payload->current_ssrc, NULL);
      GST_LOG_OBJECT (payload, "using internal ssrc %08x",
          payload->current_ssrc);
    }
    s = NULL;

    /* try to select the previously used timestamp-offset, or the one from the property */
    if (!payload->priv->ts_offset_random
        || gst_pad_has_current_caps (payload->srcpad)) {
      GstCaps *probe_caps = gst_caps_copy (templ);
      GstCaps *intersection;

      gst_caps_set_simple (probe_caps, "timestamp-offset", G_TYPE_UINT,
          payload->ts_base, NULL);
      intersection = gst_caps_intersect (probe_caps, temp);

      if (!gst_caps_is_empty (intersection)) {
        GST_LOG_OBJECT (payload, "Using selected timestamp-offset %u",
            payload->ts_base);
        gst_caps_unref (temp);
        temp = intersection;
        have_ts_offset = TRUE;
      } else {
        GST_WARNING_OBJECT (payload, "Can't use selected timestamp-offset %u",
            payload->ts_base);
        gst_caps_unref (intersection);
      }
      gst_caps_unref (probe_caps);
    }

    /* If we got no timestamp-offset above, select one now */
    if (!have_ts_offset) {
      /* get first structure */
      s = gst_caps_get_structure (temp, 0);

      if (gst_structure_has_field_typed (s, "timestamp-offset", G_TYPE_UINT)) {
        value = gst_structure_get_value (s, "timestamp-offset");
        payload->ts_base = g_value_get_uint (value);
        GST_LOG_OBJECT (payload, "using peer timestamp-offset %u",
            payload->ts_base);
      } else {
        /* FIXME, fixate_nearest_uint would be even better but we
         * don't support uint ranges so how likely is it that anybody
         * uses a list of possible timestamp-offsets */
        gst_structure_set (s, "timestamp-offset", G_TYPE_UINT, payload->ts_base,
            NULL);
        GST_LOG_OBJECT (payload, "using internal timestamp-offset %u",
            payload->ts_base);
      }
      s = NULL;
    }

    /* try to select the previously used seqnum-offset, or the one from the property */
    if (!payload->priv->seqnum_offset_random
        || gst_pad_has_current_caps (payload->srcpad)) {
      GstCaps *probe_caps = gst_caps_copy (templ);
      GstCaps *intersection;

      gst_caps_set_simple (probe_caps, "seqnum-offset", G_TYPE_UINT,
          payload->seqnum_base, NULL);
      intersection = gst_caps_intersect (probe_caps, temp);

      if (!gst_caps_is_empty (intersection)) {
        GST_LOG_OBJECT (payload, "Using selected seqnum-offset %u",
            payload->seqnum_base);
        gst_caps_unref (temp);
        temp = intersection;
        have_seqnum_offset = TRUE;
      } else {
        GST_WARNING_OBJECT (payload, "Can't use selected seqnum-offset %u",
            payload->seqnum_base);
        gst_caps_unref (intersection);
      }
      gst_caps_unref (probe_caps);
    }

    /* If we got no seqnum-offset above, select one now */
    if (!have_seqnum_offset) {
      /* get first structure */
      s = gst_caps_get_structure (temp, 0);

      if (gst_structure_has_field_typed (s, "seqnum-offset", G_TYPE_UINT)) {
        value = gst_structure_get_value (s, "seqnum-offset");
        payload->seqnum_base = g_value_get_uint (value);
        GST_LOG_OBJECT (payload, "using peer seqnum-offset %u",
            payload->seqnum_base);
        payload->priv->next_seqnum = payload->seqnum_base;
        payload->seqnum = payload->seqnum_base;
        payload->priv->seqnum_offset_random = FALSE;
      } else {
        /* FIXME, fixate_nearest_uint would be even better but we
         * don't support uint ranges so how likely is it that anybody
         * uses a list of possible seqnum-offsets */
        gst_structure_set (s, "seqnum-offset", G_TYPE_UINT,
            payload->seqnum_base, NULL);
        GST_LOG_OBJECT (payload, "using internal seqnum-offset %u",
            payload->seqnum_base);
      }

      s = NULL;
    }

    /* now fixate, start by taking the first caps */
    temp = gst_caps_truncate (temp);

    /* get first structure */
    s = gst_caps_get_structure (temp, 0);

    if (gst_structure_get_uint (s, "maxptime", &max_ptime))
      payload->priv->caps_max_ptime = max_ptime * GST_MSECOND;

    if (gst_structure_get_uint (s, "ptime", &ptime))
      payload->ptime = ptime * GST_MSECOND;

    /* make the target caps by copying over all the fixed fields, removing the
     * unfixed fields. */
    srccaps = gst_caps_new_empty_simple (gst_structure_get_name (s));
    d = gst_caps_get_structure (srccaps, 0);

    gst_structure_foreach (s, (GstStructureForeachFunc) copy_fixed, d);

    gst_caps_unref (temp);

    GST_DEBUG_OBJECT (payload, "with peer caps: %" GST_PTR_FORMAT, srccaps);
  }

  if (payload->priv->sinkcaps != NULL) {
    s = gst_caps_get_structure (payload->priv->sinkcaps, 0);
    if (g_str_has_prefix (gst_structure_get_name (s), "video")) {
      gboolean has_framerate;
      gint num, denom;

      GST_DEBUG_OBJECT (payload, "video caps: %" GST_PTR_FORMAT,
          payload->priv->sinkcaps);

      has_framerate = gst_structure_get_fraction (s, "framerate", &num, &denom);
      if (has_framerate && num == 0 && denom == 1) {
        has_framerate =
            gst_structure_get_fraction (s, "max-framerate", &num, &denom);
      }

      if (has_framerate) {
        gchar str[G_ASCII_DTOSTR_BUF_SIZE];
        gdouble framerate;

        gst_util_fraction_to_double (num, denom, &framerate);
        g_ascii_dtostr (str, G_ASCII_DTOSTR_BUF_SIZE, framerate);
        d = gst_caps_get_structure (srccaps, 0);
        gst_structure_set (d, "a-framerate", G_TYPE_STRING, str, NULL);
      }

      GST_DEBUG_OBJECT (payload, "with video caps: %" GST_PTR_FORMAT, srccaps);
    }
  }

  update_max_ptime (payload);

  {
    /* try to find header extension implementations for the list in the
     * caps */
    GstStructure *s = gst_caps_get_structure (srccaps, 0);
    guint i, j, n_fields = gst_structure_n_fields (s);
    GPtrArray *header_exts = g_ptr_array_new_with_free_func (gst_object_unref);
    GPtrArray *to_add = g_ptr_array_new ();
    GPtrArray *to_remove = g_ptr_array_new ();

    GST_OBJECT_LOCK (payload);
    g_ptr_array_foreach (payload->priv->header_exts,
        (GFunc) add_and_ref_item, header_exts);
    GST_OBJECT_UNLOCK (payload);

    for (i = 0; i < n_fields; i++) {
      const gchar *field_name = gst_structure_nth_field_name (s, i);
      if (g_str_has_prefix (field_name, "extmap-")) {
        const GValue *val;
        const gchar *uri = NULL;
        gchar *nptr;
        guint ext_id;
        GstRTPHeaderExtension *ext = NULL;

        errno = 0;
        ext_id = g_ascii_strtoull (&field_name[strlen ("extmap-")], &nptr, 10);
        if (errno != 0 || (ext_id == 0 && field_name == nptr)) {
          GST_WARNING_OBJECT (payload, "could not parse id from %s",
              field_name);
          res = FALSE;
          goto ext_out;
        }

        val = gst_structure_get_value (s, field_name);
        if (G_VALUE_HOLDS_STRING (val)) {
          uri = g_value_get_string (val);
        } else if (GST_VALUE_HOLDS_ARRAY (val)) {
          /* the uri is the second value in the array */
          const GValue *str = gst_value_array_get_value (val, 1);
          if (G_VALUE_HOLDS_STRING (str)) {
            uri = g_value_get_string (str);
          }
        }

        if (!uri) {
          GST_WARNING_OBJECT (payload, "could not get extmap uri for "
              "field %s", field_name);
          res = FALSE;
          goto ext_out;
        }

        /* try to find if this extension mapping already exists */
        for (j = 0; j < header_exts->len; j++) {
          ext = g_ptr_array_index (header_exts, j);
          if (gst_rtp_header_extension_get_id (ext) == ext_id) {
            if (g_strcmp0 (uri, gst_rtp_header_extension_get_uri (ext)) == 0) {
              /* still matching, we're good, set attributes from caps in case
               * the caps have been updated */
              if (!gst_rtp_header_extension_set_attributes_from_caps (ext,
                      srccaps)) {
                GST_WARNING_OBJECT (payload,
                    "Failed to configure rtp header " "extension %"
                    GST_PTR_FORMAT " attributes from caps %" GST_PTR_FORMAT,
                    ext, srccaps);
                res = FALSE;
                goto ext_out;
              }
              break;
            } else {
              GST_DEBUG_OBJECT (payload, "extension id %u"
                  "was replaced with a different extension uri "
                  "original:\'%s' vs \'%s\'", ext_id,
                  gst_rtp_header_extension_get_uri (ext), uri);
              g_ptr_array_add (to_remove, ext);
              ext = NULL;
              break;
            }
          } else {
            ext = NULL;
          }
        }

        /* if no extension, attempt to request one */
        if (!ext) {
          GST_DEBUG_OBJECT (payload, "requesting extension for id %u"
              " and uri %s", ext_id, uri);
          g_signal_emit (payload,
              gst_rtp_base_payload_signals[SIGNAL_REQUEST_EXTENSION], 0,
              ext_id, uri, &ext);
          GST_DEBUG_OBJECT (payload, "request returned extension %p \'%s\' "
              "for id %u and uri %s", ext,
              ext ? GST_OBJECT_NAME (ext) : "", ext_id, uri);

          /* We require caller to set the appropriate extension if it's required */
          if (ext && gst_rtp_header_extension_get_id (ext) != ext_id) {
            g_warning ("\'request-extension\' signal provided an rtp header "
                "extension for uri \'%s\' that does not match the requested "
                "extension id %u", uri, ext_id);
            gst_clear_object (&ext);
          }

          if (ext && !gst_rtp_header_extension_set_attributes_from_caps (ext,
                  srccaps)) {
            GST_WARNING_OBJECT (payload,
                "Failed to configure rtp header " "extension %"
                GST_PTR_FORMAT " attributes from caps %" GST_PTR_FORMAT,
                ext, srccaps);
            res = FALSE;
            g_clear_object (&ext);
            goto ext_out;
          }

          if (ext) {
            g_ptr_array_add (to_add, ext);
          }
        }
      }
    }

    GST_OBJECT_LOCK (payload);
    g_ptr_array_foreach (to_remove, (GFunc) remove_item_from,
        payload->priv->header_exts);
    g_ptr_array_foreach (to_add, (GFunc) add_item_to,
        payload->priv->header_exts);
    /* let extensions update their internal state from sinkcaps */
    if (payload->priv->sinkcaps) {
      gint i;

      for (i = 0; i < payload->priv->header_exts->len; i++) {
        GstRTPHeaderExtension *ext;

        ext = g_ptr_array_index (payload->priv->header_exts, i);
        if (!gst_rtp_header_extension_set_non_rtp_sink_caps (ext,
                payload->priv->sinkcaps)) {
          GST_WARNING_OBJECT (payload,
              "Failed to update rtp header extension (%s) from sink caps",
              GST_OBJECT_NAME (ext));
          res = FALSE;
          GST_OBJECT_UNLOCK (payload);
          goto ext_out;
        }
      }
    }
    /* add extension information to srccaps */
    g_ptr_array_foreach (payload->priv->header_exts,
        (GFunc) add_header_ext_to_caps, srccaps);
    GST_OBJECT_UNLOCK (payload);

    g_object_notify_by_pspec (G_OBJECT (payload),
        gst_rtp_base_payload_extensions_pspec);

  ext_out:
    g_ptr_array_unref (to_add);
    g_ptr_array_unref (to_remove);
    g_ptr_array_unref (header_exts);
  }

  GST_DEBUG_OBJECT (payload, "configuring caps %" GST_PTR_FORMAT, srccaps);

  if (res)
    res = gst_pad_set_caps (GST_RTP_BASE_PAYLOAD_SRCPAD (payload), srccaps);
  gst_caps_unref (srccaps);
  gst_caps_unref (templ);

out:
  payload->priv->negotiate_called = TRUE;

  if (!res)
    gst_pad_mark_reconfigure (GST_RTP_BASE_PAYLOAD_SRCPAD (payload));

  return res;
}

/**
 * gst_rtp_base_payload_is_filled:
 * @payload: a #GstRTPBasePayload
 * @size: the size of the packet
 * @duration: the duration of the packet
 *
 * Check if the packet with @size and @duration would exceed the configured
 * maximum size.
 *
 * Returns: %TRUE if the packet of @size and @duration would exceed the
 * configured MTU or max_ptime.
 */
gboolean
gst_rtp_base_payload_is_filled (GstRTPBasePayload * payload,
    guint size, GstClockTime duration)
{
  if (size > payload->mtu)
    return TRUE;

  if (payload->max_ptime != -1 && duration >= payload->max_ptime)
    return TRUE;

  return FALSE;
}

typedef struct
{
  GstRTPBasePayload *payload;
  guint32 ssrc;
  guint16 seqnum;
  guint8 pt;
  GstClockTime dts;
  GstClockTime pts;
  guint64 offset;
  guint32 rtptime;
} HeaderData;

static gboolean
find_timestamp (GstBuffer ** buffer, guint idx, gpointer user_data)
{
  HeaderData *data = user_data;
  data->dts = GST_BUFFER_DTS (*buffer);
  data->pts = GST_BUFFER_PTS (*buffer);
  data->offset = GST_BUFFER_OFFSET (*buffer);

  /* stop when we find a timestamp. We take whatever offset is associated with
   * the timestamp (if any) to do perfect timestamps when we need to. */
  if (data->pts != -1)
    return FALSE;
  else
    return TRUE;
}

static void
gst_rtp_base_payload_add_extension (GstRTPBasePayload * payload,
    GstRTPHeaderExtension * ext)
{
  g_return_if_fail (GST_IS_RTP_HEADER_EXTENSION (ext));
  g_return_if_fail (gst_rtp_header_extension_get_id (ext) > 0);

  /* XXX: check for duplicate ids? */
  GST_OBJECT_LOCK (payload);
  g_ptr_array_add (payload->priv->header_exts, gst_object_ref (ext));
  gst_pad_mark_reconfigure (GST_RTP_BASE_PAYLOAD_SRCPAD (payload));
  GST_OBJECT_UNLOCK (payload);

  g_object_notify_by_pspec (G_OBJECT (payload),
      gst_rtp_base_payload_extensions_pspec);
}

static void
gst_rtp_base_payload_clear_extensions (GstRTPBasePayload * payload)
{
  GST_OBJECT_LOCK (payload);
  g_ptr_array_set_size (payload->priv->header_exts, 0);
  GST_OBJECT_UNLOCK (payload);

  g_object_notify_by_pspec (G_OBJECT (payload),
      gst_rtp_base_payload_extensions_pspec);
}

static void
gst_rtp_base_payload_get_extensions (GstRTPBasePayload * payload,
    GValue * out_value)
{
  GPtrArray *extensions;
  guint i;

  GST_OBJECT_LOCK (payload);
  extensions = payload->priv->header_exts;

  for (i = 0; i < extensions->len; ++i) {
    GValue value = G_VALUE_INIT;
    g_value_init (&value, GST_TYPE_RTP_HEADER_EXTENSION);

    g_value_set_object (&value, g_ptr_array_index (extensions, i));

    gst_value_array_append_and_take_value (out_value, &value);
  }

  GST_OBJECT_UNLOCK (payload);
}

typedef struct
{
  GstRTPBasePayload *payload;
  GstRTPHeaderExtensionFlags flags;
  GstBuffer *output;
  guint8 *data;
  gsize allocated_size;
  gsize written_size;
  gsize hdr_unit_size;
  gboolean abort;
} HeaderExt;

static void
determine_header_extension_flags_size (GstRTPHeaderExtension * ext,
    gpointer user_data)
{
  HeaderExt *hdr = user_data;
  guint ext_id;
  gsize max_size;

  hdr->flags &= gst_rtp_header_extension_get_supported_flags (ext);
  max_size =
      gst_rtp_header_extension_get_max_size (ext,
      hdr->payload->priv->input_meta_buffer);

  if (max_size > RTP_HEADER_EXT_ONE_BYTE_MAX_SIZE)
    hdr->flags &= ~GST_RTP_HEADER_EXTENSION_ONE_BYTE;
  if (max_size > RTP_HEADER_EXT_TWO_BYTE_MAX_SIZE)
    hdr->flags &= ~GST_RTP_HEADER_EXTENSION_TWO_BYTE;

  ext_id = gst_rtp_header_extension_get_id (ext);
  if (ext_id > RTP_HEADER_EXT_ONE_BYTE_MAX_ID)
    hdr->flags &= ~GST_RTP_HEADER_EXTENSION_ONE_BYTE;
  if (ext_id > RTP_HEADER_EXT_TWO_BYTE_MAX_ID)
    hdr->flags &= ~GST_RTP_HEADER_EXTENSION_TWO_BYTE;

  hdr->allocated_size += max_size;
}

static void
write_header_extension (GstRTPHeaderExtension * ext, gpointer user_data)
{
  HeaderExt *hdr = user_data;
  gsize remaining =
      hdr->allocated_size - hdr->written_size - hdr->hdr_unit_size;
  gsize offset = hdr->written_size + hdr->hdr_unit_size;
  gssize written;
  guint ext_id;

  if (hdr->abort)
    return;

  written = gst_rtp_header_extension_write (ext,
      hdr->payload->priv->input_meta_buffer, hdr->flags, hdr->output,
      &hdr->data[offset], remaining);

  GST_TRACE_OBJECT (hdr->payload, "extension %" GST_PTR_FORMAT " wrote %"
      G_GSIZE_FORMAT, ext, written);

  if (written == 0) {
    /* extension wrote no data */
    return;
  } else if (written < 0) {
    GST_WARNING_OBJECT (hdr->payload, "%s failed to write extension data",
        GST_OBJECT_NAME (ext));
    goto error;
  } else if (written > remaining) {
    /* wrote too much! */
    g_error ("Overflow detected writing rtp header extensions. One of the "
        "instances likely did not report a large enough maximum size. "
        "Memory corruption has occured. Aborting");
    goto error;
  }

  ext_id = gst_rtp_header_extension_get_id (ext);

  /* move to the beginning of the extension header */
  offset -= hdr->hdr_unit_size;

  /* write extension header */
  if (hdr->flags & GST_RTP_HEADER_EXTENSION_ONE_BYTE) {
    if (written > RTP_HEADER_EXT_ONE_BYTE_MAX_SIZE) {
      g_critical ("Amount of data written by %s is larger than allowed with "
          "a one byte header.", GST_OBJECT_NAME (ext));
      goto error;
    }

    hdr->data[offset] = ((ext_id & 0x0F) << 4) | ((written - 1) & 0x0F);
  } else if (hdr->flags & GST_RTP_HEADER_EXTENSION_TWO_BYTE) {
    if (written > RTP_HEADER_EXT_TWO_BYTE_MAX_SIZE) {
      g_critical ("Amount of data written by %s is larger than allowed with "
          "a two byte header.", GST_OBJECT_NAME (ext));
      goto error;
    }

    hdr->data[offset] = ext_id & 0xFF;
    hdr->data[offset + 1] = written & 0xFF;
  } else {
    g_critical ("Don't know how to write extension data with flags 0x%x!",
        hdr->flags);
    goto error;
  }

  hdr->written_size += written + hdr->hdr_unit_size;

  return;

error:
  hdr->abort = TRUE;
  return;
}

static gboolean
set_headers (GstBuffer ** buffer, guint idx, gpointer user_data)
{
  HeaderData *data = user_data;
  HeaderExt hdrext = { NULL, };
  GstRTPBuffer rtp = { NULL, };

  if (!gst_rtp_buffer_map (*buffer, GST_MAP_READWRITE, &rtp))
    goto map_failed;

  gst_rtp_buffer_set_ssrc (&rtp, data->ssrc);
  gst_rtp_buffer_set_payload_type (&rtp, data->pt);
  gst_rtp_buffer_set_seq (&rtp, data->seqnum);
  gst_rtp_buffer_set_timestamp (&rtp, data->rtptime);

  GST_OBJECT_LOCK (data->payload);
  if (data->payload->priv->header_exts->len > 0
      && data->payload->priv->input_meta_buffer) {
    guint wordlen;
    gsize extlen;
    guint16 bit_pattern;

    /* write header extensions */
    hdrext.payload = data->payload;
    hdrext.output = *buffer;
    /* XXX: pre-calculate these flags and sizes? */
    hdrext.flags =
        GST_RTP_HEADER_EXTENSION_ONE_BYTE | GST_RTP_HEADER_EXTENSION_TWO_BYTE;
    g_ptr_array_foreach (data->payload->priv->header_exts,
        (GFunc) determine_header_extension_flags_size, &hdrext);
    hdrext.hdr_unit_size = 0;
    if (hdrext.flags & GST_RTP_HEADER_EXTENSION_ONE_BYTE) {
      /* prefer the one byte header */
      hdrext.hdr_unit_size = 1;
      /* TODO: support mixed size writing modes, i.e. RFC8285 */
      hdrext.flags &= ~GST_RTP_HEADER_EXTENSION_TWO_BYTE;
      bit_pattern = 0xBEDE;
    } else if (hdrext.flags & GST_RTP_HEADER_EXTENSION_TWO_BYTE) {
      hdrext.hdr_unit_size = 2;
      bit_pattern = 0x1000;
    } else {
      goto unsupported_flags;
    }

    extlen =
        hdrext.hdr_unit_size * data->payload->priv->header_exts->len +
        hdrext.allocated_size;
    wordlen = extlen / 4 + ((extlen % 4) ? 1 : 0);

    /* XXX: do we need to add to any existing extension data instead of
     * overwriting everything? */
    gst_rtp_buffer_set_extension_data (&rtp, bit_pattern, wordlen);
    gst_rtp_buffer_get_extension_data (&rtp, NULL, (gpointer) & hdrext.data,
        &wordlen);

    /* from 32-bit words to bytes */
    hdrext.allocated_size = wordlen * 4;

    g_ptr_array_foreach (data->payload->priv->header_exts,
        (GFunc) write_header_extension, &hdrext);

    if (hdrext.written_size > 0) {
      wordlen = hdrext.written_size / 4 + ((hdrext.written_size % 4) ? 1 : 0);

      /* zero-fill the hdrext padding bytes */
      memset (&hdrext.data[hdrext.written_size], 0,
          wordlen * 4 - hdrext.written_size);

      gst_rtp_buffer_set_extension_data (&rtp, bit_pattern, wordlen);
    } else {
      gst_rtp_buffer_remove_extension_data (&rtp);
    }
  }
  GST_OBJECT_UNLOCK (data->payload);
  gst_rtp_buffer_unmap (&rtp);

  /* increment the seqnum for each buffer */
  data->seqnum++;

  return TRUE;
  /* ERRORS */
map_failed:
  {
    GST_ERROR ("failed to map buffer %p", *buffer);
    return FALSE;
  }

unsupported_flags:
  {
    GST_OBJECT_UNLOCK (data->payload);
    gst_rtp_buffer_unmap (&rtp);
    GST_ERROR ("Cannot add rtp header extensions with mixed header types");
    return FALSE;
  }
}

static gboolean
foreach_metadata_drop (GstBuffer * buffer, GstMeta ** meta, gpointer user_data)
{
  GType drop_api_type = (GType) user_data;
  const GstMetaInfo *info = (*meta)->info;

  if (info->api == drop_api_type)
    *meta = NULL;

  return TRUE;
}

static gboolean
filter_meta (GstBuffer ** buffer, guint idx, gpointer user_data)
{
  return gst_buffer_foreach_meta (*buffer, foreach_metadata_drop,
      (gpointer) GST_RTP_SOURCE_META_API_TYPE);
}

/* Updates the SSRC, payload type, seqnum and timestamp of the RTP buffer
 * before the buffer is pushed. */
static GstFlowReturn
gst_rtp_base_payload_prepare_push (GstRTPBasePayload * payload,
    gpointer obj, gboolean is_list)
{
  GstRTPBasePayloadPrivate *priv;
  HeaderData data;

  if (payload->clock_rate == 0)
    goto no_rate;

  priv = payload->priv;

  /* update first, so that the property is set to the last
   * seqnum pushed */
  payload->seqnum = priv->next_seqnum;

  /* fill in the fields we want to set on all headers */
  data.payload = payload;
  data.seqnum = payload->seqnum;
  data.ssrc = payload->current_ssrc;
  data.pt = payload->pt;

  /* find the first buffer with a timestamp */
  if (is_list) {
    data.dts = -1;
    data.pts = -1;
    data.offset = GST_BUFFER_OFFSET_NONE;
    gst_buffer_list_foreach (GST_BUFFER_LIST_CAST (obj), find_timestamp, &data);
  } else {
    data.dts = GST_BUFFER_DTS (GST_BUFFER_CAST (obj));
    data.pts = GST_BUFFER_PTS (GST_BUFFER_CAST (obj));
    data.offset = GST_BUFFER_OFFSET (GST_BUFFER_CAST (obj));
  }

  /* convert to RTP time */
  if (priv->perfect_rtptime && data.offset != GST_BUFFER_OFFSET_NONE &&
      priv->base_offset != GST_BUFFER_OFFSET_NONE) {
    /* generate perfect RTP time by adding together the base timestamp, the
     * running time of the first buffer and difference between the offset of the
     * first buffer and the offset of the current buffer. */
    guint64 offset = data.offset - priv->base_offset;
    data.rtptime = payload->ts_base + priv->base_rtime_hz + offset;

    GST_LOG_OBJECT (payload,
        "Using offset %" G_GUINT64_FORMAT " for RTP timestamp", data.offset);

    /* store buffer's running time */
    GST_LOG_OBJECT (payload,
        "setting running-time to %" G_GUINT64_FORMAT,
        data.offset - priv->base_offset);
    priv->running_time = priv->base_rtime + data.offset - priv->base_offset;
  } else if (GST_CLOCK_TIME_IS_VALID (data.pts)) {
    guint64 rtime_ns;
    guint64 rtime_hz;

    /* no offset, use the gstreamer pts */
    if (priv->onvif_no_rate_control || !priv->scale_rtptime)
      rtime_ns = gst_segment_to_stream_time (&payload->segment,
          GST_FORMAT_TIME, data.pts);
    else
      rtime_ns =
          gst_segment_to_running_time (&payload->segment, GST_FORMAT_TIME,
          data.pts);

    if (!GST_CLOCK_TIME_IS_VALID (rtime_ns)) {
      GST_LOG_OBJECT (payload, "Clipped pts, using base RTP timestamp");
      rtime_hz = 0;
    } else {
      GST_LOG_OBJECT (payload,
          "Using running_time %" GST_TIME_FORMAT " for RTP timestamp",
          GST_TIME_ARGS (rtime_ns));
      rtime_hz =
          gst_util_uint64_scale_int (rtime_ns, payload->clock_rate, GST_SECOND);
      priv->base_offset = data.offset;
      priv->base_rtime_hz = rtime_hz;
    }

    /* add running_time in clock-rate units to the base timestamp */
    data.rtptime = payload->ts_base + rtime_hz;

    /* store buffer's running time */
    if (priv->perfect_rtptime) {
      GST_LOG_OBJECT (payload,
          "setting running-time to %" G_GUINT64_FORMAT, rtime_hz);
      priv->running_time = rtime_hz;
    } else {
      GST_LOG_OBJECT (payload,
          "setting running-time to %" GST_TIME_FORMAT,
          GST_TIME_ARGS (rtime_ns));
      priv->running_time = rtime_ns;
    }
  } else {
    GST_LOG_OBJECT (payload,
        "Using previous RTP timestamp %" G_GUINT32_FORMAT, payload->timestamp);
    /* no timestamp to convert, take previous timestamp */
    data.rtptime = payload->timestamp;
  }

  /* set ssrc, payload type, seq number, caps and rtptime */
  /* remove unwanted meta */
  if (is_list) {
    gst_buffer_list_foreach (GST_BUFFER_LIST_CAST (obj), set_headers, &data);
    gst_buffer_list_foreach (GST_BUFFER_LIST_CAST (obj), filter_meta, NULL);
    /* sequence number has increased more if this was a buffer list */
    payload->seqnum = data.seqnum - 1;
  } else {
    GstBuffer *buf = GST_BUFFER_CAST (obj);
    set_headers (&buf, 0, &data);
    filter_meta (&buf, 0, NULL);
  }

  priv->next_seqnum = data.seqnum;
  payload->timestamp = data.rtptime;

  GST_LOG_OBJECT (payload, "Preparing to push %s with size %"
      G_GSIZE_FORMAT ", seq=%d, rtptime=%u, pts %" GST_TIME_FORMAT,
      (is_list) ? "list" : "packet",
      (is_list) ? gst_buffer_list_length (GST_BUFFER_LIST_CAST (obj)) :
      gst_buffer_get_size (GST_BUFFER (obj)),
      payload->seqnum, data.rtptime, GST_TIME_ARGS (data.pts));

  if (g_atomic_int_compare_and_exchange (&payload->priv->
          notified_first_timestamp, 1, 0)) {
    g_object_notify (G_OBJECT (payload), "timestamp");
    g_object_notify (G_OBJECT (payload), "seqnum");
  }

  return GST_FLOW_OK;

  /* ERRORS */
no_rate:
  {
    GST_ELEMENT_ERROR (payload, STREAM, NOT_IMPLEMENTED, (NULL),
        ("subclass did not specify clock-rate"));
    return GST_FLOW_ERROR;
  }
}

/**
 * gst_rtp_base_payload_push_list:
 * @payload: a #GstRTPBasePayload
 * @list: (transfer full): a #GstBufferList
 *
 * Push @list to the peer element of the payloader. The SSRC, payload type,
 * seqnum and timestamp of the RTP buffer will be updated first.
 *
 * This function takes ownership of @list.
 *
 * Returns: a #GstFlowReturn.
 */
GstFlowReturn
gst_rtp_base_payload_push_list (GstRTPBasePayload * payload,
    GstBufferList * list)
{
  GstFlowReturn res;

  res = gst_rtp_base_payload_prepare_push (payload, list, TRUE);

  if (G_LIKELY (res == GST_FLOW_OK)) {
    if (G_UNLIKELY (payload->priv->pending_segment)) {
      gst_pad_push_event (payload->srcpad, payload->priv->pending_segment);
      payload->priv->pending_segment = FALSE;
      payload->priv->delay_segment = FALSE;
    }
    res = gst_pad_push_list (payload->srcpad, list);
  } else {
    gst_buffer_list_unref (list);
  }

  return res;
}

/**
 * gst_rtp_base_payload_push:
 * @payload: a #GstRTPBasePayload
 * @buffer: (transfer full): a #GstBuffer
 *
 * Push @buffer to the peer element of the payloader. The SSRC, payload type,
 * seqnum and timestamp of the RTP buffer will be updated first.
 *
 * This function takes ownership of @buffer.
 *
 * Returns: a #GstFlowReturn.
 */
GstFlowReturn
gst_rtp_base_payload_push (GstRTPBasePayload * payload, GstBuffer * buffer)
{
  GstFlowReturn res;

  res = gst_rtp_base_payload_prepare_push (payload, buffer, FALSE);

  if (G_LIKELY (res == GST_FLOW_OK)) {
    if (G_UNLIKELY (payload->priv->pending_segment)) {
      gst_pad_push_event (payload->srcpad, payload->priv->pending_segment);
      payload->priv->pending_segment = FALSE;
      payload->priv->delay_segment = FALSE;
    }
    res = gst_pad_push (payload->srcpad, buffer);
  } else {
    gst_buffer_unref (buffer);
  }

  return res;
}

/**
 * gst_rtp_base_payload_allocate_output_buffer:
 * @payload: a #GstRTPBasePayload
 * @payload_len: the length of the payload
 * @pad_len: the amount of padding
 * @csrc_count: the minimum number of CSRC entries
 *
 * Allocate a new #GstBuffer with enough data to hold an RTP packet with
 * minimum @csrc_count CSRCs, a payload length of @payload_len and padding of
 * @pad_len. If @payload has #GstRTPBasePayload:source-info %TRUE additional
 * CSRCs may be allocated and filled with RTP source information.
 *
 * Returns: A newly allocated buffer that can hold an RTP packet with given
 * parameters.
 *
 * Since: 1.16
 */
GstBuffer *
gst_rtp_base_payload_allocate_output_buffer (GstRTPBasePayload * payload,
    guint payload_len, guint8 pad_len, guint8 csrc_count)
{
  GstBuffer *buffer = NULL;

  if (payload->priv->input_meta_buffer != NULL) {
    GstRTPSourceMeta *meta =
        gst_buffer_get_rtp_source_meta (payload->priv->input_meta_buffer);
    if (meta != NULL) {
      guint total_csrc_count, idx, i;
      GstRTPBuffer rtp = GST_RTP_BUFFER_INIT;

      total_csrc_count = csrc_count + meta->csrc_count +
          (meta->ssrc_valid ? 1 : 0);
      total_csrc_count = MIN (total_csrc_count, 15);
      buffer = gst_rtp_buffer_new_allocate (payload_len, pad_len,
          total_csrc_count);

      gst_rtp_buffer_map (buffer, GST_MAP_READWRITE, &rtp);

      /* Skip CSRC fields requested by derived class and fill CSRCs from meta.
       * Finally append the SSRC as a new CSRC. */
      idx = csrc_count;
      for (i = 0; i < meta->csrc_count && idx < 15; i++, idx++)
        gst_rtp_buffer_set_csrc (&rtp, idx, meta->csrc[i]);
      if (meta->ssrc_valid && idx < 15)
        gst_rtp_buffer_set_csrc (&rtp, idx, meta->ssrc);

      gst_rtp_buffer_unmap (&rtp);
    }
  }

  if (buffer == NULL)
    buffer = gst_rtp_buffer_new_allocate (payload_len, pad_len, csrc_count);

  return buffer;
}

static GstStructure *
gst_rtp_base_payload_create_stats (GstRTPBasePayload * rtpbasepayload)
{
  GstRTPBasePayloadPrivate *priv;
  GstStructure *s;

  priv = rtpbasepayload->priv;

  s = gst_structure_new ("application/x-rtp-payload-stats",
      "clock-rate", G_TYPE_UINT, (guint) rtpbasepayload->clock_rate,
      "running-time", G_TYPE_UINT64, priv->running_time,
      "seqnum", G_TYPE_UINT, (guint) rtpbasepayload->seqnum,
      "timestamp", G_TYPE_UINT, (guint) rtpbasepayload->timestamp,
      "ssrc", G_TYPE_UINT, rtpbasepayload->current_ssrc,
      "pt", G_TYPE_UINT, rtpbasepayload->pt,
      "seqnum-offset", G_TYPE_UINT, (guint) rtpbasepayload->seqnum_base,
      "timestamp-offset", G_TYPE_UINT, (guint) rtpbasepayload->ts_base, NULL);

  return s;
}

static void
gst_rtp_base_payload_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstRTPBasePayload *rtpbasepayload;
  GstRTPBasePayloadPrivate *priv;
  gint64 val;

  rtpbasepayload = GST_RTP_BASE_PAYLOAD (object);
  priv = rtpbasepayload->priv;

  switch (prop_id) {
    case PROP_MTU:
      rtpbasepayload->mtu = g_value_get_uint (value);
      break;
    case PROP_PT:
      rtpbasepayload->pt = g_value_get_uint (value);
      priv->pt_set = TRUE;
      break;
    case PROP_SSRC:
      val = g_value_get_uint (value);
      rtpbasepayload->ssrc = val;
      priv->ssrc_random = FALSE;
      break;
    case PROP_TIMESTAMP_OFFSET:
      val = g_value_get_uint (value);
      rtpbasepayload->ts_offset = val;
      priv->ts_offset_random = FALSE;
      break;
    case PROP_SEQNUM_OFFSET:
      val = g_value_get_int (value);
      rtpbasepayload->seqnum_offset = val;
      priv->seqnum_offset_random = (val == -1);
      GST_DEBUG_OBJECT (rtpbasepayload, "seqnum offset 0x%04x, random %d",
          rtpbasepayload->seqnum_offset, priv->seqnum_offset_random);
      break;
    case PROP_MAX_PTIME:
      rtpbasepayload->priv->prop_max_ptime = g_value_get_int64 (value);
      update_max_ptime (rtpbasepayload);
      break;
    case PROP_MIN_PTIME:
      rtpbasepayload->min_ptime = g_value_get_int64 (value);
      break;
    case PROP_PERFECT_RTPTIME:
      priv->perfect_rtptime = g_value_get_boolean (value);
      break;
    case PROP_PTIME_MULTIPLE:
      rtpbasepayload->ptime_multiple = g_value_get_int64 (value);
      break;
    case PROP_SOURCE_INFO:
      gst_rtp_base_payload_set_source_info_enabled (rtpbasepayload,
          g_value_get_boolean (value));
      break;
    case PROP_ONVIF_NO_RATE_CONTROL:
      priv->onvif_no_rate_control = g_value_get_boolean (value);
      break;
    case PROP_SCALE_RTPTIME:
      priv->scale_rtptime = g_value_get_boolean (value);
      break;
    case PROP_AUTO_HEADER_EXTENSION:
      priv->auto_hdr_ext = g_value_get_boolean (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_rtp_base_payload_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstRTPBasePayload *rtpbasepayload;
  GstRTPBasePayloadPrivate *priv;

  rtpbasepayload = GST_RTP_BASE_PAYLOAD (object);
  priv = rtpbasepayload->priv;

  switch (prop_id) {
    case PROP_MTU:
      g_value_set_uint (value, rtpbasepayload->mtu);
      break;
    case PROP_PT:
      g_value_set_uint (value, rtpbasepayload->pt);
      break;
    case PROP_SSRC:
      if (priv->ssrc_random)
        g_value_set_uint (value, -1);
      else
        g_value_set_uint (value, rtpbasepayload->ssrc);
      break;
    case PROP_TIMESTAMP_OFFSET:
      if (priv->ts_offset_random)
        g_value_set_uint (value, -1);
      else
        g_value_set_uint (value, (guint32) rtpbasepayload->ts_offset);
      break;
    case PROP_SEQNUM_OFFSET:
      if (priv->seqnum_offset_random)
        g_value_set_int (value, -1);
      else
        g_value_set_int (value, (guint16) rtpbasepayload->seqnum_offset);
      break;
    case PROP_MAX_PTIME:
      g_value_set_int64 (value, rtpbasepayload->max_ptime);
      break;
    case PROP_MIN_PTIME:
      g_value_set_int64 (value, rtpbasepayload->min_ptime);
      break;
    case PROP_TIMESTAMP:
      g_value_set_uint (value, rtpbasepayload->timestamp);
      break;
    case PROP_SEQNUM:
      g_value_set_uint (value, rtpbasepayload->seqnum);
      break;
    case PROP_PERFECT_RTPTIME:
      g_value_set_boolean (value, priv->perfect_rtptime);
      break;
    case PROP_PTIME_MULTIPLE:
      g_value_set_int64 (value, rtpbasepayload->ptime_multiple);
      break;
    case PROP_STATS:
      g_value_take_boxed (value,
          gst_rtp_base_payload_create_stats (rtpbasepayload));
      break;
    case PROP_SOURCE_INFO:
      g_value_set_boolean (value,
          gst_rtp_base_payload_is_source_info_enabled (rtpbasepayload));
      break;
    case PROP_ONVIF_NO_RATE_CONTROL:
      g_value_set_boolean (value, priv->onvif_no_rate_control);
      break;
    case PROP_SCALE_RTPTIME:
      g_value_set_boolean (value, priv->scale_rtptime);
      break;
    case PROP_AUTO_HEADER_EXTENSION:
      g_value_set_boolean (value, priv->auto_hdr_ext);
      break;
    case PROP_EXTENSIONS:
      gst_rtp_base_payload_get_extensions (rtpbasepayload, value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static GstStateChangeReturn
gst_rtp_base_payload_change_state (GstElement * element,
    GstStateChange transition)
{
  GstRTPBasePayload *rtpbasepayload;
  GstRTPBasePayloadPrivate *priv;
  GstStateChangeReturn ret;

  rtpbasepayload = GST_RTP_BASE_PAYLOAD (element);
  priv = rtpbasepayload->priv;

  switch (transition) {
    case GST_STATE_CHANGE_NULL_TO_READY:
      break;
    case GST_STATE_CHANGE_READY_TO_PAUSED:
      gst_segment_init (&rtpbasepayload->segment, GST_FORMAT_UNDEFINED);
      rtpbasepayload->priv->delay_segment = TRUE;
      gst_event_replace (&rtpbasepayload->priv->pending_segment, NULL);

      if (priv->seqnum_offset_random)
        rtpbasepayload->seqnum_base = g_random_int_range (0, G_MAXINT16);
      else
        rtpbasepayload->seqnum_base = rtpbasepayload->seqnum_offset;
      priv->next_seqnum = rtpbasepayload->seqnum_base;
      rtpbasepayload->seqnum = rtpbasepayload->seqnum_base;

      if (priv->ssrc_random)
        rtpbasepayload->current_ssrc = g_random_int ();
      else
        rtpbasepayload->current_ssrc = rtpbasepayload->ssrc;

      if (priv->ts_offset_random)
        rtpbasepayload->ts_base = g_random_int ();
      else
        rtpbasepayload->ts_base = rtpbasepayload->ts_offset;
      rtpbasepayload->timestamp = rtpbasepayload->ts_base;
      priv->running_time = DEFAULT_RUNNING_TIME;
      g_atomic_int_set (&rtpbasepayload->priv->notified_first_timestamp, 1);
      priv->base_offset = GST_BUFFER_OFFSET_NONE;
      priv->negotiated = FALSE;
      priv->negotiate_called = FALSE;
      gst_caps_replace (&rtpbasepayload->priv->subclass_srccaps, NULL);
      gst_caps_replace (&rtpbasepayload->priv->sinkcaps, NULL);
      break;
    default:
      break;
  }

  ret = GST_ELEMENT_CLASS (parent_class)->change_state (element, transition);

  switch (transition) {
    case GST_STATE_CHANGE_PLAYING_TO_PAUSED:
      g_atomic_int_set (&rtpbasepayload->priv->notified_first_timestamp, 1);
      break;
    case GST_STATE_CHANGE_PAUSED_TO_READY:
      gst_event_replace (&rtpbasepayload->priv->pending_segment, NULL);
      break;
    default:
      break;
  }
  return ret;
}

/**
 * gst_rtp_base_payload_set_source_info_enabled:
 * @payload: a #GstRTPBasePayload
 * @enable: whether to add contributing sources to RTP packets
 *
 * Enable or disable adding contributing sources to RTP packets from
 * #GstRTPSourceMeta.
 *
 * Since: 1.16
 **/
void
gst_rtp_base_payload_set_source_info_enabled (GstRTPBasePayload * payload,
    gboolean enable)
{
  payload->priv->source_info = enable;
}

/**
 * gst_rtp_base_payload_is_source_info_enabled:
 * @payload: a #GstRTPBasePayload
 *
 * Queries whether the payloader will add contributing sources (CSRCs) to the
 * RTP header from #GstRTPSourceMeta.
 *
 * Returns: %TRUE if source-info is enabled.
 *
 * Since: 1.16
 **/
gboolean
gst_rtp_base_payload_is_source_info_enabled (GstRTPBasePayload * payload)
{
  return payload->priv->source_info;
}


/**
 * gst_rtp_base_payload_get_source_count:
 * @payload: a #GstRTPBasePayload
 * @buffer: (transfer none): a #GstBuffer, typically the buffer to payload
 *
 * Count the total number of RTP sources found in the meta of @buffer, which
 * will be automically added by gst_rtp_base_payload_allocate_output_buffer().
 * If #GstRTPBasePayload:source-info is %FALSE the count will be 0.
 *
 * Returns: The number of sources.
 *
 * Since: 1.16
 **/
guint
gst_rtp_base_payload_get_source_count (GstRTPBasePayload * payload,
    GstBuffer * buffer)
{
  guint count = 0;

  g_return_val_if_fail (buffer != NULL, 0);

  if (gst_rtp_base_payload_is_source_info_enabled (payload)) {
    GstRTPSourceMeta *meta = gst_buffer_get_rtp_source_meta (buffer);
    if (meta != NULL)
      count = gst_rtp_source_meta_get_source_count (meta);
  }

  return count;
}
