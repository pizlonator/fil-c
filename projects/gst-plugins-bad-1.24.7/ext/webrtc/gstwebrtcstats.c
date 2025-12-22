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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

/* for GValueArray... */
#define GLIB_DISABLE_DEPRECATION_WARNINGS

#include "gstwebrtcstats.h"
#include "gstwebrtcbin.h"
#include "transportstream.h"
#include "transportreceivebin.h"
#include "utils.h"
#include "webrtctransceiver.h"

#include <stdlib.h>

#define GST_CAT_DEFAULT gst_webrtc_stats_debug
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);

static void
_init_debug (void)
{
  static gsize _init = 0;

  if (g_once_init_enter (&_init)) {
    GST_DEBUG_CATEGORY_INIT (gst_webrtc_stats_debug, "webrtcstats", 0,
        "webrtcstats");
    g_once_init_leave (&_init, 1);
  }
}

static double
monotonic_time_as_double_milliseconds (void)
{
  return g_get_monotonic_time () / 1000.0;
}

static void
_set_base_stats (GstStructure * s, GstWebRTCStatsType type, double ts,
    const char *id)
{
  const gchar *name = _enum_value_to_string (GST_TYPE_WEBRTC_STATS_TYPE,
      type);

  g_return_if_fail (name != NULL);

  gst_structure_set_name (s, name);
  gst_structure_set (s, "type", GST_TYPE_WEBRTC_STATS_TYPE, type, "timestamp",
      G_TYPE_DOUBLE, ts, "id", G_TYPE_STRING, id, NULL);
}

static GstStructure *
_get_peer_connection_stats (GstWebRTCBin * webrtc)
{
  guint opened;
  guint closed;
  GstStructure *s = gst_structure_new_empty ("peer-connection-stats");

  gst_webrtc_bin_get_peer_connection_stats (webrtc, &opened, &closed);

  gst_structure_set (s, "data-channels-opened", G_TYPE_UINT, opened,
      "data-channels-closed", G_TYPE_UINT, closed, "data-channels-requested",
      G_TYPE_UINT, 0, "data-channels-accepted", G_TYPE_UINT, 0, NULL);

  return s;
}

static void
_gst_structure_take_structure (GstStructure * s, const char *fieldname,
    GstStructure ** value_s)
{
  GValue v = G_VALUE_INIT;

  g_return_if_fail (GST_IS_STRUCTURE (*value_s));

  g_value_init (&v, GST_TYPE_STRUCTURE);
  g_value_take_boxed (&v, *value_s);

  gst_structure_take_value (s, fieldname, &v);

  *value_s = NULL;
}

#define CLOCK_RATE_VALUE_TO_SECONDS(v,r) ((double) v / (double) clock_rate)
#define FIXED_16_16_TO_DOUBLE(v) ((double) ((v & 0xffff0000) >> 16) + ((v & 0xffff) / 65536.0))
#define FIXED_32_32_TO_DOUBLE(v) ((double) ((v & G_GUINT64_CONSTANT (0xffffffff00000000)) >> 32) + ((v & G_GUINT64_CONSTANT (0xffffffff)) / 4294967296.0))

/* https://www.w3.org/TR/webrtc-stats/#remoteinboundrtpstats-dict* */
static gboolean
_get_stats_from_remote_rtp_source_stats (GstWebRTCBin * webrtc,
    TransportStream * stream, const GstStructure * source_stats,
    guint ssrc, guint clock_rate, const gchar * codec_id, const gchar * kind,
    const gchar * transport_id, GstStructure * s)
{
  gboolean have_rb = FALSE, internal = FALSE;
  int lost;
  GstStructure *r_in;
  gchar *r_in_id, *out_id;
  guint32 rtt;
  guint fraction_lost, jitter;
  double ts;

  gst_structure_get_double (s, "timestamp", &ts);
  gst_structure_get (source_stats, "internal", G_TYPE_BOOLEAN, &internal,
      "have-rb", G_TYPE_BOOLEAN, &have_rb, NULL);

  /* This isn't what we're looking for */
  if (internal == TRUE || have_rb == FALSE)
    return FALSE;

  r_in_id = g_strdup_printf ("rtp-remote-inbound-stream-stats_%u", ssrc);
  out_id = g_strdup_printf ("rtp-outbound-stream-stats_%u", ssrc);

  r_in = gst_structure_new_empty (r_in_id);
  _set_base_stats (r_in, GST_WEBRTC_STATS_REMOTE_INBOUND_RTP, ts, r_in_id);

  /* RTCRtpStreamStats */
  gst_structure_set (r_in, "local-id", G_TYPE_STRING, out_id, NULL);
  gst_structure_set (r_in, "ssrc", G_TYPE_UINT, ssrc, NULL);
  gst_structure_set (r_in, "codec-id", G_TYPE_STRING, codec_id, NULL);
  gst_structure_set (r_in, "transport-id", G_TYPE_STRING, transport_id, NULL);
  if (kind)
    gst_structure_set (r_in, "kind", G_TYPE_STRING, kind, NULL);

  /* RTCReceivedRtpStreamStats */

  if (gst_structure_get_int (source_stats, "rb-packetslost", &lost))
    gst_structure_set (r_in, "packets-lost", G_TYPE_INT64, (gint64) lost, NULL);

  if (clock_rate && gst_structure_get_uint (source_stats, "rb-jitter", &jitter))
    gst_structure_set (r_in, "jitter", G_TYPE_DOUBLE,
        CLOCK_RATE_VALUE_TO_SECONDS (jitter, clock_rate), NULL);

  /* RTCReceivedRtpStreamStats:

     unsigned long long  packetsReceived;
     unsigned long      packetsDiscarded;
     unsigned long      packetsRepaired;
     unsigned long      burstPacketsLost;
     unsigned long      burstPacketsDiscarded;
     unsigned long      burstLossCount;
     unsigned long      burstDiscardCount;
     double             burstLossRate;
     double             burstDiscardRate;
     double             gapLossRate;
     double             gapDiscardRate;

     Can't be implemented frame re-assembly happens after rtpbin:

     unsigned long        framesDropped;
     unsigned long        partialFramesLost;
     unsigned long        fullFramesLost;
   */

  /* RTCRemoteInboundRTPStreamStats */

  if (gst_structure_get_uint (source_stats, "rb-fractionlost", &fraction_lost))
    gst_structure_set (r_in, "fraction-lost", G_TYPE_DOUBLE,
        (double) fraction_lost / 256.0, NULL);

  if (gst_structure_get_uint (source_stats, "rb-round-trip", &rtt)) {
    /* 16.16 fixed point to double */
    double val = FIXED_16_16_TO_DOUBLE (rtt);
    gst_structure_set (r_in, "round-trip-time", G_TYPE_DOUBLE, val, NULL);
  }

  /* RTCRemoteInboundRTPStreamStats:

     To be added:

     DOMString            localId;
     double               totalRoundTripTime;
     unsigned long long   reportsReceived;
     unsigned long long   roundTripTimeMeasurements;
   */

  gst_structure_set (r_in, "gst-rtpsource-stats", GST_TYPE_STRUCTURE,
      source_stats, NULL);

  _gst_structure_take_structure (s, r_in_id, &r_in);

  g_free (r_in_id);
  g_free (out_id);

  return TRUE;
}

/* https://www.w3.org/TR/webrtc-stats/#inboundrtpstats-dict*
   https://www.w3.org/TR/webrtc-stats/#outboundrtpstats-dict* */
static void
_get_stats_from_rtp_source_stats (GstWebRTCBin * webrtc,
    TransportStream * stream, const GstStructure * source_stats,
    const gchar * codec_id, const gchar * kind, const gchar * transport_id,
    GstStructure * s)
{
  guint ssrc, fir, pli, nack, jitter;
  int clock_rate;
  guint64 packets, bytes;
  gboolean internal;
  double ts;

  gst_structure_get_double (s, "timestamp", &ts);
  gst_structure_get (source_stats, "ssrc", G_TYPE_UINT, &ssrc, "clock-rate",
      G_TYPE_INT, &clock_rate, "internal", G_TYPE_BOOLEAN, &internal, NULL);

  if (internal) {
    GstStructure *out;
    gchar *out_id, *r_in_id;

    out_id = g_strdup_printf ("rtp-outbound-stream-stats_%u", ssrc);

    out = gst_structure_new_empty (out_id);
    _set_base_stats (out, GST_WEBRTC_STATS_OUTBOUND_RTP, ts, out_id);

    /* RTCStreamStats */
    gst_structure_set (out, "ssrc", G_TYPE_UINT, ssrc, NULL);
    gst_structure_set (out, "codec-id", G_TYPE_STRING, codec_id, NULL);
    gst_structure_set (out, "transport-id", G_TYPE_STRING, transport_id, NULL);
    if (kind)
      gst_structure_set (out, "kind", G_TYPE_STRING, kind, NULL);

    /* RTCSentRtpStreamStats  */
    if (gst_structure_get_uint64 (source_stats, "octets-sent", &bytes))
      gst_structure_set (out, "bytes-sent", G_TYPE_UINT64, bytes, NULL);
    if (gst_structure_get_uint64 (source_stats, "packets-sent", &packets))
      gst_structure_set (out, "packets-sent", G_TYPE_UINT64, packets, NULL);

    /* RTCOutboundRTPStreamStats */

    if (gst_structure_get_uint (source_stats, "recv-fir-count", &fir))
      gst_structure_set (out, "fir-count", G_TYPE_UINT, fir, NULL);
    if (gst_structure_get_uint (source_stats, "recv-pli-count", &pli))
      gst_structure_set (out, "pli-count", G_TYPE_UINT, pli, NULL);
    if (gst_structure_get_uint (source_stats, "recv-nack-count", &nack))
      gst_structure_set (out, "nack-count", G_TYPE_UINT, nack, NULL);
    /* XXX: mediaType, trackId, sliCount, qpSum */

    r_in_id = g_strdup_printf ("rtp-remote-inbound-stream-stats_%u", ssrc);
    if (gst_structure_has_field (s, r_in_id))
      gst_structure_set (out, "remote-id", G_TYPE_STRING, r_in_id, NULL);
    g_free (r_in_id);

    /*  RTCOutboundRTPStreamStats:

       To be added:

       unsigned long        sliCount;
       unsigned long        rtxSsrc;
       DOMString            mediaSourceId;
       DOMString            senderId;
       DOMString            remoteId;
       DOMString            rid;
       DOMHighResTimeStamp  lastPacketSentTimestamp;
       unsigned long long   headerBytesSent;
       unsigned long        packetsDiscardedOnSend;
       unsigned long long   bytesDiscardedOnSend;
       unsigned long        fecPacketsSent;
       unsigned long long   retransmittedPacketsSent;
       unsigned long long   retransmittedBytesSent;
       double               averageRtcpInterval;
       record<USVString, unsigned long long> perDscpPacketsSent;

       Not relevant because webrtcbin doesn't encode:

       double               targetBitrate;
       unsigned long long   totalEncodedBytesTarget;
       unsigned long        frameWidth;
       unsigned long        frameHeight;
       unsigned long        frameBitDepth;
       double               framesPerSecond;
       unsigned long        framesSent;
       unsigned long        hugeFramesSent;
       unsigned long        framesEncoded;
       unsigned long        keyFramesEncoded;
       unsigned long        framesDiscardedOnSend;
       unsigned long long   qpSum;
       unsigned long long   totalSamplesSent;
       unsigned long long   samplesEncodedWithSilk;
       unsigned long long   samplesEncodedWithCelt;
       boolean              voiceActivityFlag;
       double               totalEncodeTime;
       double               totalPacketSendDelay;
       RTCQualityLimitationReason                 qualityLimitationReason;
       record<DOMString, double> qualityLimitationDurations;
       unsigned long        qualityLimitationResolutionChanges;
       DOMString            encoderImplementation;
     */

    /* Store the raw stats from GStreamer into the structure for advanced
     * information.
     */
    gst_structure_set (out, "gst-rtpsource-stats", GST_TYPE_STRUCTURE,
        source_stats, NULL);

    _gst_structure_take_structure (s, out_id, &out);

    g_free (out_id);
  } else {
    GstStructure *in, *r_out;
    gchar *r_out_id, *in_id;
    gboolean have_sr = FALSE;
    GstStructure *jb_stats = NULL;
    guint i;
    guint64 jb_lost, duplicates, late, rtx_success;

    gst_structure_get (source_stats, "have-sr", G_TYPE_BOOLEAN, &have_sr, NULL);

    for (i = 0; i < stream->ssrcmap->len; i++) {
      SsrcMapItem *item = g_ptr_array_index (stream->ssrcmap, i);

      if (item->direction == GST_WEBRTC_RTP_TRANSCEIVER_DIRECTION_RECVONLY
          && item->ssrc == ssrc) {
        GObject *jb = g_weak_ref_get (&item->rtpjitterbuffer);

        if (jb) {
          g_object_get (jb, "stats", &jb_stats, NULL);
          g_object_unref (jb);
        }
        break;
      }
    }

    if (jb_stats)
      gst_structure_get (jb_stats, "num-lost", G_TYPE_UINT64, &jb_lost,
          "num-duplicates", G_TYPE_UINT64, &duplicates, "num-late",
          G_TYPE_UINT64, &late, "rtx-success-count", G_TYPE_UINT64,
          &rtx_success, NULL);

    in_id = g_strdup_printf ("rtp-inbound-stream-stats_%u", ssrc);
    r_out_id = g_strdup_printf ("rtp-remote-outbound-stream-stats_%u", ssrc);

    in = gst_structure_new_empty (in_id);
    _set_base_stats (in, GST_WEBRTC_STATS_INBOUND_RTP, ts, in_id);

    /* RTCRtpStreamStats */
    gst_structure_set (in, "ssrc", G_TYPE_UINT, ssrc, NULL);
    gst_structure_set (in, "codec-id", G_TYPE_STRING, codec_id, NULL);
    gst_structure_set (in, "transport-id", G_TYPE_STRING, transport_id, NULL);
    if (kind)
      gst_structure_set (in, "kind", G_TYPE_STRING, kind, NULL);

    /* RTCReceivedRtpStreamStats */

    if (gst_structure_get_uint64 (source_stats, "packets-received", &packets))
      gst_structure_set (in, "packets-received", G_TYPE_UINT64, packets, NULL);
    if (jb_stats) {
      gint64 packets_lost = jb_lost > G_MAXINT64 ?
          G_MAXINT64 : (gint64) jb_lost;
      gst_structure_set (in, "packets-lost", G_TYPE_INT64, packets_lost, NULL);
    }
    if (gst_structure_get_uint (source_stats, "jitter", &jitter))
      gst_structure_set (in, "jitter", G_TYPE_DOUBLE,
          CLOCK_RATE_VALUE_TO_SECONDS (jitter, clock_rate), NULL);

    if (jb_stats)
      gst_structure_set (in, "packets-discarded", G_TYPE_UINT64, late,
          "packets-repaired", G_TYPE_UINT64, rtx_success, NULL);

    /*
       RTCReceivedRtpStreamStats

       To be added:

       unsigned long long   burstPacketsLost;
       unsigned long long   burstPacketsDiscarded;
       unsigned long        burstLossCount;
       unsigned long        burstDiscardCount;
       double               burstLossRate;
       double               burstDiscardRate;
       double               gapLossRate;
       double               gapDiscardRate;

       Not relevant because webrtcbin doesn't decode:

       unsigned long        framesDropped;
       unsigned long        partialFramesLost;
       unsigned long        fullFramesLost;
     */

    /* RTCInboundRtpStreamStats */
    gst_structure_set (in, "remote-id", G_TYPE_STRING, r_out_id, NULL);

    if (gst_structure_get_uint64 (source_stats, "octets-received", &bytes))
      gst_structure_set (in, "bytes-received", G_TYPE_UINT64, bytes, NULL);

    if (gst_structure_get_uint (source_stats, "sent-fir-count", &fir))
      gst_structure_set (in, "fir-count", G_TYPE_UINT, fir, NULL);
    if (gst_structure_get_uint (source_stats, "sent-pli-count", &pli))
      gst_structure_set (in, "pli-count", G_TYPE_UINT, pli, NULL);
    if (gst_structure_get_uint (source_stats, "sent-nack-count", &nack))
      gst_structure_set (in, "nack-count", G_TYPE_UINT, nack, NULL);
    if (jb_stats)
      gst_structure_set (in, "packets-duplicated", G_TYPE_UINT64, duplicates,
          NULL);

    /* RTCInboundRtpStreamStats:

       To be added:

       required DOMString   receiverId;
       double               averageRtcpInterval;
       unsigned long long   headerBytesReceived;
       unsigned long long   fecPacketsReceived;
       unsigned long long   fecPacketsDiscarded;
       unsigned long long   bytesReceived;
       unsigned long long   packetsFailedDecryption;
       record<USVString, unsigned long long> perDscpPacketsReceived;
       unsigned long        nackCount;
       unsigned long        firCount;
       unsigned long        pliCount;
       unsigned long        sliCount;
       double               jitterBufferDelay;

       Not relevant because webrtcbin doesn't decode or depayload:
       unsigned long        framesDecoded;
       unsigned long        keyFramesDecoded;
       unsigned long        frameWidth;
       unsigned long        frameHeight;
       unsigned long        frameBitDepth;
       double               framesPerSecond;
       unsigned long long   qpSum;
       double               totalDecodeTime;
       double               totalInterFrameDelay;
       double               totalSquaredInterFrameDelay;
       boolean              voiceActivityFlag;
       DOMHighResTimeStamp  lastPacketReceivedTimestamp;
       double               totalProcessingDelay;
       DOMHighResTimeStamp  estimatedPlayoutTimestamp;
       unsigned long long   jitterBufferEmittedCount;
       unsigned long long   totalSamplesReceived;
       unsigned long long   totalSamplesDecoded;
       unsigned long long   samplesDecodedWithSilk;
       unsigned long long   samplesDecodedWithCelt;
       unsigned long long   concealedSamples;
       unsigned long long   silentConcealedSamples;
       unsigned long long   concealmentEvents;
       unsigned long long   insertedSamplesForDeceleration;
       unsigned long long   removedSamplesForAcceleration;
       double               audioLevel;
       double               totalAudioEnergy;
       double               totalSamplesDuration;
       unsigned long        framesReceived;
       DOMString            decoderImplementation;
     */

    r_out = gst_structure_new_empty (r_out_id);
    _set_base_stats (r_out, GST_WEBRTC_STATS_REMOTE_OUTBOUND_RTP, ts, r_out_id);
    /* RTCStreamStats */
    gst_structure_set (r_out, "ssrc", G_TYPE_UINT, ssrc, NULL);
    gst_structure_set (r_out, "codec-id", G_TYPE_STRING, codec_id, NULL);
    gst_structure_set (r_out, "transport-id", G_TYPE_STRING, transport_id,
        NULL);
    /* XXX: mediaType, trackId */

    /* RTCSentRtpStreamStats */

    if (have_sr) {
      guint sr_bytes, sr_packets;

      if (gst_structure_get_uint (source_stats, "sr-octet-count", &sr_bytes))
        gst_structure_set (r_out, "bytes-sent", G_TYPE_UINT, sr_bytes, NULL);
      if (gst_structure_get_uint (source_stats, "sr-packet-count", &sr_packets))
        gst_structure_set (r_out, "packets-sent", G_TYPE_UINT, sr_packets,
            NULL);
    }

    /* RTCSentRtpStreamStats:

       To be added:

       unsigned long        rtxSsrc;
       DOMString            mediaSourceId;
       DOMString            senderId;
       DOMString            remoteId;
       DOMString            rid;
       DOMHighResTimeStamp  lastPacketSentTimestamp;
       unsigned long long   headerBytesSent;
       unsigned long        packetsDiscardedOnSend;
       unsigned long long   bytesDiscardedOnSend;
       unsigned long        fecPacketsSent;
       unsigned long long   retransmittedPacketsSent;
       unsigned long long   retransmittedBytesSent;
       double               averageRtcpInterval;
       unsigned long        sliCount;

       Can't be implemented because we don't decode:

       double               targetBitrate;
       unsigned long long   totalEncodedBytesTarget;
       unsigned long        frameWidth;
       unsigned long        frameHeight;
       unsigned long        frameBitDepth;
       double               framesPerSecond;
       unsigned long        framesSent;
       unsigned long        hugeFramesSent;
       unsigned long        framesEncoded;
       unsigned long        keyFramesEncoded;
       unsigned long        framesDiscardedOnSend;
       unsigned long long   qpSum;
       unsigned long long   totalSamplesSent;
       unsigned long long   samplesEncodedWithSilk;
       unsigned long long   samplesEncodedWithCelt;
       boolean              voiceActivityFlag;
       double               totalEncodeTime;
       double               totalPacketSendDelay;
       RTCQualityLimitationReason                 qualityLimitationReason;
       record<DOMString, double> qualityLimitationDurations;
       unsigned long        qualityLimitationResolutionChanges;
       record<USVString, unsigned long long> perDscpPacketsSent;
       DOMString            encoderImplementation;
     */

    /* RTCRemoteOutboundRtpStreamStats */

    if (have_sr) {
      guint64 ntptime;
      if (gst_structure_get_uint64 (source_stats, "sr-ntptime", &ntptime)) {
        /* 16.16 fixed point to double */
        double val = FIXED_32_32_TO_DOUBLE (ntptime);
        gst_structure_set (r_out, "remote-timestamp", G_TYPE_DOUBLE, val, NULL);
      }
    } else {
      /* default values */
      gst_structure_set (r_out, "remote-timestamp", G_TYPE_DOUBLE, 0.0, NULL);
    }

    gst_structure_set (r_out, "local-id", G_TYPE_STRING, in_id, NULL);

    /* To be added:
       reportsSent
     */

    /* Store the raw stats from GStreamer into the structure for advanced
     * information.
     */
    if (jb_stats)
      _gst_structure_take_structure (in, "gst-rtpjitterbuffer-stats",
          &jb_stats);

    gst_structure_set (in, "gst-rtpsource-stats", GST_TYPE_STRUCTURE,
        source_stats, NULL);

    _gst_structure_take_structure (s, in_id, &in);
    _gst_structure_take_structure (s, r_out_id, &r_out);

    g_free (in_id);
    g_free (r_out_id);
  }
}

/* https://www.w3.org/TR/webrtc-stats/#icecandidate-dict* */
static gchar *
_get_stats_from_ice_candidates (GstWebRTCBin * webrtc,
    GstWebRTCICECandidateStats * can, const gchar * transport_id,
    const gchar * candidate_tag, GstStructure * s)
{
  GstStructure *stats;
  GstWebRTCStatsType type;
  gchar *id;
  double ts;

  gst_structure_get_double (s, "timestamp", &ts);

  id = g_strdup_printf ("ice-candidate-%s_%u_%s_%u", candidate_tag,
      can->stream_id, can->ipaddr, can->port);
  stats = gst_structure_new_empty (id);

  if (g_str_equal (candidate_tag, "local")) {
    type = GST_WEBRTC_STATS_LOCAL_CANDIDATE;
  } else if (g_str_equal (candidate_tag, "remote")) {
    type = GST_WEBRTC_STATS_REMOTE_CANDIDATE;
  } else {
    GST_WARNING_OBJECT (webrtc, "Invalid ice candidate tag: %s", candidate_tag);
    return NULL;
  }
  _set_base_stats (stats, type, ts, id);

  /* RTCIceCandidateStats
     DOMString           transportId;
     DOMString           address;
     long                port;
     DOMString           protocol;
     RTCIceCandidateType candidateType;
     long                priority;
     DOMString           url;
     DOMString           relayProtocol;
   */

  if (transport_id)
    gst_structure_set (stats, "transport-id", G_TYPE_STRING, transport_id,
        NULL);
  gst_structure_set (stats, "address", G_TYPE_STRING, can->ipaddr, NULL);
  gst_structure_set (stats, "port", G_TYPE_UINT, can->port, NULL);
  gst_structure_set (stats, "candidate-type", G_TYPE_STRING, can->type, NULL);
  gst_structure_set (stats, "priority", G_TYPE_UINT, can->prio, NULL);
  gst_structure_set (stats, "protocol", G_TYPE_STRING, can->proto, NULL);
  if (can->relay_proto)
    gst_structure_set (stats, "relay-protocol", G_TYPE_STRING, can->relay_proto,
        NULL);
  if (can->url)
    gst_structure_set (stats, "url", G_TYPE_STRING, can->url, NULL);

  gst_structure_set (s, id, GST_TYPE_STRUCTURE, stats, NULL);
  gst_structure_free (stats);

  return id;
}

/* https://www.w3.org/TR/webrtc-stats/#candidatepair-dict* */
static gchar *
_get_stats_from_ice_transport (GstWebRTCBin * webrtc,
    GstWebRTCICETransport * transport, GstWebRTCICEStream * stream,
    const GstStructure * twcc_stats, const gchar * transport_id,
    GstStructure * s)
{
  GstStructure *stats;
  gchar *id;
  gchar *local_cand_id = NULL, *remote_cand_id = NULL;
  double ts;
  GstWebRTCICECandidateStats *local_cand = NULL, *remote_cand = NULL;

  gst_structure_get_double (s, "timestamp", &ts);

  id = g_strdup_printf ("ice-candidate-pair_%s", GST_OBJECT_NAME (transport));
  stats = gst_structure_new_empty (id);
  _set_base_stats (stats, GST_WEBRTC_STATS_CANDIDATE_PAIR, ts, id);

  /* RTCIceCandidatePairStats
     DOMString                     transportId;
     DOMString                     localCandidateId;
     DOMString                     remoteCandidateId;

     XXX: To be added:

     RTCStatsIceCandidatePairState state;
     boolean                       nominated;
     unsigned long                 packetsSent;
     unsigned long                 packetsReceived;
     unsigned long long            bytesSent;
     unsigned long long            bytesReceived;
     DOMHighResTimeStamp           lastPacketSentTimestamp;
     DOMHighResTimeStamp           lastPacketReceivedTimestamp;
     DOMHighResTimeStamp           firstRequestTimestamp;
     DOMHighResTimeStamp           lastRequestTimestamp;
     DOMHighResTimeStamp           lastResponseTimestamp;
     double                        totalRoundTripTime;
     double                        currentRoundTripTime;
     double                        availableOutgoingBitrate;
     double                        availableIncomingBitrate;
     unsigned long                 circuitBreakerTriggerCount;
     unsigned long long            requestsReceived;
     unsigned long long            requestsSent;
     unsigned long long            responsesReceived;
     unsigned long long            responsesSent;
     unsigned long long            retransmissionsReceived;
     unsigned long long            retransmissionsSent;
     unsigned long long            consentRequestsSent;
     DOMHighResTimeStamp           consentExpiredTimestamp;
     unsigned long                 packetsDiscardedOnSend;
     unsigned long long            bytesDiscardedOnSend;
     unsigned long long            requestBytesSent;
     unsigned long long            consentRequestBytesSent;
     unsigned long long            responseBytesSent;
   */

  if (gst_webrtc_ice_get_selected_pair (webrtc->priv->ice, stream,
          &local_cand, &remote_cand)) {
    local_cand_id =
        _get_stats_from_ice_candidates (webrtc, local_cand, transport_id,
        "local", s);
    remote_cand_id =
        _get_stats_from_ice_candidates (webrtc, remote_cand, transport_id,
        "remote", s);

    gst_structure_set (stats, "local-candidate-id", G_TYPE_STRING,
        local_cand_id, NULL);
    gst_structure_set (stats, "remote-candidate-id", G_TYPE_STRING,
        remote_cand_id, NULL);
  } else
    GST_INFO_OBJECT (webrtc,
        "No selected ICE candidate pair was found for transport %s",
        GST_OBJECT_NAME (transport));

  /* XXX: these stats are at the rtp session level but there isn't a specific
   * stats structure for that. The RTCIceCandidatePairStats is the closest with
   * the 'availableIncomingBitrate' and 'availableOutgoingBitrate' fields
   */
  if (twcc_stats)
    gst_structure_set (stats, "gst-twcc-stats", GST_TYPE_STRUCTURE, twcc_stats,
        NULL);

  gst_structure_set (s, id, GST_TYPE_STRUCTURE, stats, NULL);

  g_free (local_cand_id);
  g_free (remote_cand_id);

  gst_webrtc_ice_candidate_stats_free (local_cand);
  gst_webrtc_ice_candidate_stats_free (remote_cand);

  gst_structure_free (stats);

  return id;
}

/* https://www.w3.org/TR/webrtc-stats/#dom-rtctransportstats */
static gchar *
_get_stats_from_dtls_transport (GstWebRTCBin * webrtc,
    GstWebRTCDTLSTransport * transport, GstWebRTCICEStream * stream,
    const GstStructure * twcc_stats, GstStructure * s)
{
  GstStructure *stats;
  gchar *id;
  double ts;
  gchar *ice_id;

  gst_structure_get_double (s, "timestamp", &ts);

  id = g_strdup_printf ("transport-stats_%s", GST_OBJECT_NAME (transport));
  stats = gst_structure_new_empty (id);
  _set_base_stats (stats, GST_WEBRTC_STATS_TRANSPORT, ts, id);

/* XXX: RTCTransportStats
    unsigned long         packetsSent;
    unsigned long         packetsReceived;
    unsigned long long    bytesSent;
    unsigned long long    bytesReceived;
    DOMString             rtcpTransportStatsId;
    RTCIceRole            iceRole;
    RTCDtlsTransportState dtlsState;
    DOMString             selectedCandidatePairId;
    DOMString             localCertificateId;
    DOMString             remoteCertificateId;
*/

/* XXX: RTCCertificateStats
    DOMString fingerprint;
    DOMString fingerprintAlgorithm;
    DOMString base64Certificate;
    DOMString issuerCertificateId;
*/

  ice_id =
      _get_stats_from_ice_transport (webrtc, transport->transport, stream,
      twcc_stats, id, s);
  if (ice_id) {
    gst_structure_set (stats, "selected-candidate-pair-id", G_TYPE_STRING,
        ice_id, NULL);
    g_free (ice_id);
  }

  gst_structure_set (s, id, GST_TYPE_STRUCTURE, stats, NULL);
  gst_structure_free (stats);

  return id;
}

/* https://www.w3.org/TR/webrtc-stats/#codec-dict* */
static gboolean
_get_codec_stats_from_pad (GstWebRTCBin * webrtc, GstPad * pad,
    GstStructure * s, gchar ** out_id, guint * out_ssrc, guint * out_clock_rate)
{
  GstWebRTCBinPad *wpad = GST_WEBRTC_BIN_PAD (pad);
  GstStructure *stats;
  GstCaps *caps = NULL;
  gchar *id;
  double ts;
  guint ssrc = 0;
  gint clock_rate = 0;
  gboolean has_caps_ssrc = FALSE;

  gst_structure_get_double (s, "timestamp", &ts);

  stats = gst_structure_new_empty ("unused");
  id = g_strdup_printf ("codec-stats-%s", GST_OBJECT_NAME (pad));
  _set_base_stats (stats, GST_WEBRTC_STATS_CODEC, ts, id);

  if (wpad->received_caps)
    caps = gst_caps_ref (wpad->received_caps);
  else
    caps = gst_pad_get_current_caps (pad);

  GST_DEBUG_OBJECT (pad, "Pad caps are: %" GST_PTR_FORMAT, caps);
  if (caps && gst_caps_is_fixed (caps)) {
    GstStructure *caps_s = gst_caps_get_structure (caps, 0);
    gint pt;
    const gchar *encoding_name, *media, *encoding_params;
    GstSDPMedia sdp_media = { 0 };
    guint channels = 0;

    if (gst_structure_get_int (caps_s, "payload", &pt))
      gst_structure_set (stats, "payload-type", G_TYPE_UINT, pt, NULL);

    if (gst_structure_get_int (caps_s, "clock-rate", &clock_rate))
      gst_structure_set (stats, "clock-rate", G_TYPE_UINT, clock_rate, NULL);

    if (gst_structure_get_uint (caps_s, "ssrc", &ssrc)) {
      gst_structure_set (stats, "ssrc", G_TYPE_UINT, ssrc, NULL);
      has_caps_ssrc = TRUE;
    }

    media = gst_structure_get_string (caps_s, "media");
    encoding_name = gst_structure_get_string (caps_s, "encoding-name");
    encoding_params = gst_structure_get_string (caps_s, "encoding-params");

    if (media || encoding_name) {
      gchar *mime_type;

      mime_type = g_strdup_printf ("%s/%s", media ? media : "",
          encoding_name ? encoding_name : "");
      gst_structure_set (stats, "mime-type", G_TYPE_STRING, mime_type, NULL);
      g_free (mime_type);
    }

    if (encoding_params)
      channels = atoi (encoding_params);
    if (channels)
      gst_structure_set (stats, "channels", G_TYPE_UINT, channels, NULL);

    if (gst_pad_get_direction (pad) == GST_PAD_SRC)
      gst_structure_set (stats, "codec-type", G_TYPE_STRING, "decode", NULL);
    else
      gst_structure_set (stats, "codec-type", G_TYPE_STRING, "encode", NULL);

    gst_sdp_media_init (&sdp_media);
    if (gst_sdp_media_set_media_from_caps (caps, &sdp_media) == GST_SDP_OK) {
      const gchar *fmtp = gst_sdp_media_get_attribute_val (&sdp_media, "fmtp");

      if (fmtp) {
        gst_structure_set (stats, "sdp-fmtp-line", G_TYPE_STRING, fmtp, NULL);
      }
    }
    gst_sdp_media_uninit (&sdp_media);

    /* FIXME: transportId */
  }

  if (caps)
    gst_caps_unref (caps);

  gst_structure_set (s, id, GST_TYPE_STRUCTURE, stats, NULL);
  gst_structure_free (stats);

  if (out_id)
    *out_id = id;
  else
    g_free (id);

  if (out_ssrc)
    *out_ssrc = ssrc;

  if (out_clock_rate)
    *out_clock_rate = clock_rate;

  return has_caps_ssrc;
}

struct transport_stream_stats
{
  GstWebRTCBin *webrtc;
  TransportStream *stream;
  char *transport_id;
  char *codec_id;
  const char *kind;
  guint clock_rate;
  GValueArray *source_stats;
  GstStructure *s;
};

static gboolean
webrtc_stats_get_from_transport (SsrcMapItem * entry,
    struct transport_stream_stats *ts_stats)
{
  double ts;
  int i;

  gst_structure_get_double (ts_stats->s, "timestamp", &ts);

  /* construct stats objects */
  for (i = 0; i < ts_stats->source_stats->n_values; i++) {
    const GstStructure *stats;
    const GValue *val = g_value_array_get_nth (ts_stats->source_stats, i);
    guint stats_ssrc = 0;

    stats = gst_value_get_structure (val);

    /* skip foreign sources */
    if (gst_structure_get_uint (stats, "ssrc", &stats_ssrc) &&
        entry->ssrc == stats_ssrc)
      _get_stats_from_rtp_source_stats (ts_stats->webrtc, ts_stats->stream,
          stats, ts_stats->codec_id, ts_stats->kind, ts_stats->transport_id,
          ts_stats->s);
    else if (gst_structure_get_uint (stats, "rb-ssrc", &stats_ssrc)
        && entry->ssrc == stats_ssrc)
      _get_stats_from_remote_rtp_source_stats (ts_stats->webrtc,
          ts_stats->stream, stats, entry->ssrc, ts_stats->clock_rate,
          ts_stats->codec_id, ts_stats->kind, ts_stats->transport_id,
          ts_stats->s);
  }

  /* we want to look at all the entries */
  return FALSE;
}

static gboolean
_get_stats_from_pad (GstWebRTCBin * webrtc, GstPad * pad, GstStructure * s)
{
  GstWebRTCBinPad *wpad = GST_WEBRTC_BIN_PAD (pad);
  struct transport_stream_stats ts_stats = { NULL, };
  guint ssrc, clock_rate;
  GObject *rtp_session;
  GObject *gst_rtp_session;
  GstStructure *rtp_stats, *twcc_stats;
  GstWebRTCKind kind;

  _get_codec_stats_from_pad (webrtc, pad, s, &ts_stats.codec_id, &ssrc,
      &clock_rate);

  if (!wpad->trans)
    goto out;

  g_object_get (wpad->trans, "kind", &kind, NULL);
  switch (kind) {
    case GST_WEBRTC_KIND_AUDIO:
      ts_stats.kind = "audio";
      break;
    case GST_WEBRTC_KIND_VIDEO:
      ts_stats.kind = "video";
      break;
    case GST_WEBRTC_KIND_UNKNOWN:
      ts_stats.kind = NULL;
      break;
  };

  ts_stats.stream = WEBRTC_TRANSCEIVER (wpad->trans)->stream;
  if (!ts_stats.stream)
    goto out;

  if (wpad->trans->mline == G_MAXUINT)
    goto out;

  if (!ts_stats.stream->transport)
    goto out;

  g_signal_emit_by_name (webrtc->rtpbin, "get-internal-session",
      ts_stats.stream->session_id, &rtp_session);
  g_object_get (rtp_session, "stats", &rtp_stats, NULL);
  g_signal_emit_by_name (webrtc->rtpbin, "get-session",
      ts_stats.stream->session_id, &gst_rtp_session);
  g_object_get (gst_rtp_session, "twcc-stats", &twcc_stats, NULL);

  gst_structure_get (rtp_stats, "source-stats", G_TYPE_VALUE_ARRAY,
      &ts_stats.source_stats, NULL);

  ts_stats.transport_id =
      _get_stats_from_dtls_transport (webrtc, ts_stats.stream->transport,
      GST_WEBRTC_ICE_STREAM (ts_stats.stream->stream), twcc_stats, s);

  GST_DEBUG_OBJECT (webrtc, "retrieving rtp stream stats from transport %"
      GST_PTR_FORMAT " rtp session %" GST_PTR_FORMAT " with %u rtp sources, "
      "transport %" GST_PTR_FORMAT, ts_stats.stream, rtp_session,
      ts_stats.source_stats->n_values, ts_stats.stream->transport);

  ts_stats.s = s;
  ts_stats.clock_rate = clock_rate;

  transport_stream_find_ssrc_map_item (ts_stats.stream, &ts_stats,
      (FindSsrcMapFunc) webrtc_stats_get_from_transport);

  g_clear_object (&rtp_session);
  g_clear_object (&gst_rtp_session);
  gst_clear_structure (&rtp_stats);
  gst_clear_structure (&twcc_stats);
  g_value_array_free (ts_stats.source_stats);
  ts_stats.source_stats = NULL;
  g_clear_pointer (&ts_stats.transport_id, g_free);

out:
  g_clear_pointer (&ts_stats.codec_id, g_free);
  return TRUE;
}

GstStructure *
gst_webrtc_bin_create_stats (GstWebRTCBin * webrtc, GstPad * pad)
{
  GstStructure *s = gst_structure_new_empty ("application/x-webrtc-stats");
  double ts = monotonic_time_as_double_milliseconds ();
  GstStructure *pc_stats;

  _init_debug ();

  gst_structure_set (s, "timestamp", G_TYPE_DOUBLE, ts, NULL);

  /* FIXME: better unique IDs */
  /* FIXME: rate limitting stat updates? */
  /* FIXME: all stats need to be kept forever */

  GST_DEBUG_OBJECT (webrtc, "updating stats at time %f", ts);

  if ((pc_stats = _get_peer_connection_stats (webrtc))) {
    const gchar *id = "peer-connection-stats";
    _set_base_stats (pc_stats, GST_WEBRTC_STATS_PEER_CONNECTION, ts, id);
    gst_structure_set (s, id, GST_TYPE_STRUCTURE, pc_stats, NULL);
    gst_structure_free (pc_stats);
  }

  if (pad)
    _get_stats_from_pad (webrtc, pad, s);
  else
    gst_element_foreach_pad (GST_ELEMENT (webrtc),
        (GstElementForeachPadFunc) _get_stats_from_pad, s);

  gst_structure_remove_field (s, "timestamp");

  return s;
}
