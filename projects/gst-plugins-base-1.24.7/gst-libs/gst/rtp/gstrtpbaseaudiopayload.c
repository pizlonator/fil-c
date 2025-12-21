/* GStreamer
 * Copyright (C) <2006> Philippe Khalaf <philippe.kalaf@collabora.co.uk>
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
 * SECTION:gstrtpbaseaudiopayload
 * @title: GstRTPBaseAudioPayload
 * @short_description: Base class for audio RTP payloader
 *
 * Provides a base class for audio RTP payloaders for frame or sample based
 * audio codecs (constant bitrate)
 *
 * This class derives from GstRTPBasePayload. It can be used for payloading
 * audio codecs. It will only work with constant bitrate codecs. It supports
 * both frame based and sample based codecs. It takes care of packing up the
 * audio data into RTP packets and filling up the headers accordingly. The
 * payloading is done based on the maximum MTU (mtu) and the maximum time per
 * packet (max-ptime). The general idea is to divide large data buffers into
 * smaller RTP packets. The RTP packet size is the minimum of either the MTU,
 * max-ptime (if set) or available data. The RTP packet size is always larger or
 * equal to min-ptime (if set). If min-ptime is not set, any residual data is
 * sent in a last RTP packet. In the case of frame based codecs, the resulting
 * RTP packets always contain full frames.
 *
 * ## Usage
 *
 * To use this base class, your child element needs to call either
 * gst_rtp_base_audio_payload_set_frame_based() or
 * gst_rtp_base_audio_payload_set_sample_based(). This is usually done in the
 * element's `_init()` function. Then, the child element must call either
 * gst_rtp_base_audio_payload_set_frame_options(),
 * gst_rtp_base_audio_payload_set_sample_options() or
 * gst_rtp_base_audio_payload_set_samplebits_options. Since
 * GstRTPBaseAudioPayload derives from GstRTPBasePayload, the child element
 * must set any variables or call/override any functions required by that base
 * class. The child element does not need to override any other functions
 * specific to GstRTPBaseAudioPayload.
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <string.h>
#include <gst/rtp/gstrtpbuffer.h>
#include <gst/base/gstadapter.h>
#include <gst/audio/audio.h>

#include "gstrtpbaseaudiopayload.h"

GST_DEBUG_CATEGORY_STATIC (rtpbaseaudiopayload_debug);
#define GST_CAT_DEFAULT (rtpbaseaudiopayload_debug)

#define DEFAULT_BUFFER_LIST             FALSE

enum
{
  PROP_0,
  PROP_BUFFER_LIST,
  PROP_LAST
};

/* function to convert bytes to a time */
typedef GstClockTime (*GetBytesToTimeFunc) (GstRTPBaseAudioPayload * payload,
    guint64 bytes);
/* function to convert bytes to a RTP time */
typedef guint32 (*GetBytesToRTPTimeFunc) (GstRTPBaseAudioPayload * payload,
    guint64 bytes);
/* function to convert time to bytes */
typedef guint64 (*GetTimeToBytesFunc) (GstRTPBaseAudioPayload * payload,
    GstClockTime time);

struct _GstRTPBaseAudioPayloadPrivate
{
  GetBytesToTimeFunc bytes_to_time;
  GetBytesToRTPTimeFunc bytes_to_rtptime;
  GetTimeToBytesFunc time_to_bytes;

  GstAdapter *adapter;
  guint fragment_size;
  GstClockTime frame_duration_ns;
  gboolean discont;
  guint64 offset;
  GstClockTime last_timestamp;
  guint32 last_rtptime;
  guint align;

  guint cached_mtu;
  guint cached_min_ptime;
  guint cached_max_ptime;
  guint cached_ptime;
  guint cached_min_length;
  guint cached_max_length;
  guint cached_ptime_multiple;
  guint cached_align;
  guint cached_csrc_count;

  gboolean buffer_list;
};

static void gst_rtp_base_audio_payload_finalize (GObject * object);

static void gst_rtp_base_audio_payload_set_property (GObject * object,
    guint prop_id, const GValue * value, GParamSpec * pspec);
static void gst_rtp_base_audio_payload_get_property (GObject * object,
    guint prop_id, GValue * value, GParamSpec * pspec);

/* bytes to time functions */
static GstClockTime
gst_rtp_base_audio_payload_frame_bytes_to_time (GstRTPBaseAudioPayload *
    payload, guint64 bytes);
static GstClockTime
gst_rtp_base_audio_payload_sample_bytes_to_time (GstRTPBaseAudioPayload *
    payload, guint64 bytes);

/* bytes to RTP time functions */
static guint32
gst_rtp_base_audio_payload_frame_bytes_to_rtptime (GstRTPBaseAudioPayload *
    payload, guint64 bytes);
static guint32
gst_rtp_base_audio_payload_sample_bytes_to_rtptime (GstRTPBaseAudioPayload *
    payload, guint64 bytes);

/* time to bytes functions */
static guint64
gst_rtp_base_audio_payload_frame_time_to_bytes (GstRTPBaseAudioPayload *
    payload, GstClockTime time);
static guint64
gst_rtp_base_audio_payload_sample_time_to_bytes (GstRTPBaseAudioPayload *
    payload, GstClockTime time);

static GstFlowReturn gst_rtp_base_audio_payload_handle_buffer (GstRTPBasePayload
    * payload, GstBuffer * buffer);
static GstStateChangeReturn gst_rtp_base_payload_audio_change_state (GstElement
    * element, GstStateChange transition);
static gboolean gst_rtp_base_payload_audio_sink_event (GstRTPBasePayload
    * payload, GstEvent * event);

/* cached quark to avoid contention on the global quark table lock */
#define META_TAG_AUDIO meta_tag_audio_quark
static GQuark meta_tag_audio_quark;

#define gst_rtp_base_audio_payload_parent_class parent_class
G_DEFINE_TYPE_WITH_PRIVATE (GstRTPBaseAudioPayload, gst_rtp_base_audio_payload,
    GST_TYPE_RTP_BASE_PAYLOAD);

static void
gst_rtp_base_audio_payload_class_init (GstRTPBaseAudioPayloadClass * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *gstelement_class;
  GstRTPBasePayloadClass *gstrtpbasepayload_class;

  meta_tag_audio_quark = g_quark_from_static_string (GST_META_TAG_AUDIO_STR);

  gobject_class = (GObjectClass *) klass;
  gstelement_class = (GstElementClass *) klass;
  gstrtpbasepayload_class = (GstRTPBasePayloadClass *) klass;

  gobject_class->finalize = gst_rtp_base_audio_payload_finalize;
  gobject_class->set_property = gst_rtp_base_audio_payload_set_property;
  gobject_class->get_property = gst_rtp_base_audio_payload_get_property;

  g_object_class_install_property (G_OBJECT_CLASS (klass), PROP_BUFFER_LIST,
      g_param_spec_boolean ("buffer-list", "Buffer List",
          "Use Buffer Lists",
          DEFAULT_BUFFER_LIST, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  gstelement_class->change_state =
      GST_DEBUG_FUNCPTR (gst_rtp_base_payload_audio_change_state);

  gstrtpbasepayload_class->handle_buffer =
      GST_DEBUG_FUNCPTR (gst_rtp_base_audio_payload_handle_buffer);
  gstrtpbasepayload_class->sink_event =
      GST_DEBUG_FUNCPTR (gst_rtp_base_payload_audio_sink_event);

  GST_DEBUG_CATEGORY_INIT (rtpbaseaudiopayload_debug, "rtpbaseaudiopayload", 0,
      "base audio RTP payloader");
}

static void
gst_rtp_base_audio_payload_init (GstRTPBaseAudioPayload * payload)
{
  payload->priv = gst_rtp_base_audio_payload_get_instance_private (payload);

  /* these need to be set by child object if frame based */
  payload->frame_size = 0;
  payload->frame_duration = 0;

  /* these need to be set by child object if sample based */
  payload->sample_size = 0;

  payload->priv->adapter = gst_adapter_new ();

  payload->priv->buffer_list = DEFAULT_BUFFER_LIST;
}

static void
gst_rtp_base_audio_payload_finalize (GObject * object)
{
  GstRTPBaseAudioPayload *payload;

  payload = GST_RTP_BASE_AUDIO_PAYLOAD (object);

  g_object_unref (payload->priv->adapter);

  GST_CALL_PARENT (G_OBJECT_CLASS, finalize, (object));
}

static void
gst_rtp_base_audio_payload_set_property (GObject * object,
    guint prop_id, const GValue * value, GParamSpec * pspec)
{
  GstRTPBaseAudioPayload *payload;

  payload = GST_RTP_BASE_AUDIO_PAYLOAD (object);

  switch (prop_id) {
    case PROP_BUFFER_LIST:
#if 0
      payload->priv->buffer_list = g_value_get_boolean (value);
#endif
      payload->priv->buffer_list = FALSE;
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_rtp_base_audio_payload_get_property (GObject * object,
    guint prop_id, GValue * value, GParamSpec * pspec)
{
  GstRTPBaseAudioPayload *payload;

  payload = GST_RTP_BASE_AUDIO_PAYLOAD (object);

  switch (prop_id) {
    case PROP_BUFFER_LIST:
      g_value_set_boolean (value, payload->priv->buffer_list);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

/**
 * gst_rtp_base_audio_payload_set_frame_based:
 * @rtpbaseaudiopayload: a pointer to the element.
 *
 * Tells #GstRTPBaseAudioPayload that the child element is for a frame based
 * audio codec
 */
void
gst_rtp_base_audio_payload_set_frame_based (GstRTPBaseAudioPayload *
    rtpbaseaudiopayload)
{
  g_return_if_fail (rtpbaseaudiopayload != NULL);
  g_return_if_fail (rtpbaseaudiopayload->priv->time_to_bytes == NULL);
  g_return_if_fail (rtpbaseaudiopayload->priv->bytes_to_time == NULL);
  g_return_if_fail (rtpbaseaudiopayload->priv->bytes_to_rtptime == NULL);

  rtpbaseaudiopayload->priv->bytes_to_time =
      gst_rtp_base_audio_payload_frame_bytes_to_time;
  rtpbaseaudiopayload->priv->bytes_to_rtptime =
      gst_rtp_base_audio_payload_frame_bytes_to_rtptime;
  rtpbaseaudiopayload->priv->time_to_bytes =
      gst_rtp_base_audio_payload_frame_time_to_bytes;
}

/**
 * gst_rtp_base_audio_payload_set_sample_based:
 * @rtpbaseaudiopayload: a pointer to the element.
 *
 * Tells #GstRTPBaseAudioPayload that the child element is for a sample based
 * audio codec
 */
void
gst_rtp_base_audio_payload_set_sample_based (GstRTPBaseAudioPayload *
    rtpbaseaudiopayload)
{
  g_return_if_fail (rtpbaseaudiopayload != NULL);
  g_return_if_fail (rtpbaseaudiopayload->priv->time_to_bytes == NULL);
  g_return_if_fail (rtpbaseaudiopayload->priv->bytes_to_time == NULL);
  g_return_if_fail (rtpbaseaudiopayload->priv->bytes_to_rtptime == NULL);

  rtpbaseaudiopayload->priv->bytes_to_time =
      gst_rtp_base_audio_payload_sample_bytes_to_time;
  rtpbaseaudiopayload->priv->bytes_to_rtptime =
      gst_rtp_base_audio_payload_sample_bytes_to_rtptime;
  rtpbaseaudiopayload->priv->time_to_bytes =
      gst_rtp_base_audio_payload_sample_time_to_bytes;
}

/**
 * gst_rtp_base_audio_payload_set_frame_options:
 * @rtpbaseaudiopayload: a pointer to the element.
 * @frame_duration: The duraction of an audio frame in milliseconds.
 * @frame_size: The size of an audio frame in bytes.
 *
 * Sets the options for frame based audio codecs.
 *
 */
void
gst_rtp_base_audio_payload_set_frame_options (GstRTPBaseAudioPayload
    * rtpbaseaudiopayload, gint frame_duration, gint frame_size)
{
  GstRTPBaseAudioPayloadPrivate *priv;

  g_return_if_fail (rtpbaseaudiopayload != NULL);

  priv = rtpbaseaudiopayload->priv;

  rtpbaseaudiopayload->frame_duration = frame_duration;
  priv->frame_duration_ns = frame_duration * GST_MSECOND;
  rtpbaseaudiopayload->frame_size = frame_size;
  priv->align = frame_size;

  gst_adapter_clear (priv->adapter);

  GST_DEBUG_OBJECT (rtpbaseaudiopayload, "frame set to %d ms and size %d",
      frame_duration, frame_size);
}

/**
 * gst_rtp_base_audio_payload_set_sample_options:
 * @rtpbaseaudiopayload: a pointer to the element.
 * @sample_size: Size per sample in bytes.
 *
 * Sets the options for sample based audio codecs.
 */
void
gst_rtp_base_audio_payload_set_sample_options (GstRTPBaseAudioPayload
    * rtpbaseaudiopayload, gint sample_size)
{
  g_return_if_fail (rtpbaseaudiopayload != NULL);

  /* sample_size is in bits internally */
  gst_rtp_base_audio_payload_set_samplebits_options (rtpbaseaudiopayload,
      sample_size * 8);
}

/**
 * gst_rtp_base_audio_payload_set_samplebits_options:
 * @rtpbaseaudiopayload: a pointer to the element.
 * @sample_size: Size per sample in bits.
 *
 * Sets the options for sample based audio codecs.
 */
void
gst_rtp_base_audio_payload_set_samplebits_options (GstRTPBaseAudioPayload
    * rtpbaseaudiopayload, gint sample_size)
{
  guint fragment_size;
  GstRTPBaseAudioPayloadPrivate *priv;

  g_return_if_fail (rtpbaseaudiopayload != NULL);

  priv = rtpbaseaudiopayload->priv;

  rtpbaseaudiopayload->sample_size = sample_size;

  /* sample_size is in bits and is converted into multiple bytes */
  fragment_size = sample_size;
  while ((fragment_size % 8) != 0)
    fragment_size += fragment_size;
  priv->fragment_size = fragment_size / 8;
  priv->align = priv->fragment_size;

  gst_adapter_clear (priv->adapter);

  GST_DEBUG_OBJECT (rtpbaseaudiopayload,
      "Samplebits set to sample size %d bits", sample_size);
}

static void
gst_rtp_base_audio_payload_set_meta (GstRTPBaseAudioPayload * payload,
    GstBuffer * buffer, guint payload_len, GstClockTime timestamp)
{
  GstRTPBasePayload *basepayload;
  GstRTPBaseAudioPayloadPrivate *priv;
  GstRTPBuffer rtp = { NULL };

  basepayload = GST_RTP_BASE_PAYLOAD_CAST (payload);
  priv = payload->priv;

  /* set payload type */
  gst_rtp_buffer_map (buffer, GST_MAP_WRITE, &rtp);
  gst_rtp_buffer_set_payload_type (&rtp, basepayload->pt);
  /* set marker bit for disconts */
  if (priv->discont) {
    GST_DEBUG_OBJECT (payload, "Setting marker and DISCONT");
    gst_rtp_buffer_set_marker (&rtp, TRUE);
    GST_BUFFER_FLAG_SET (buffer, GST_BUFFER_FLAG_DISCONT);
    priv->discont = FALSE;
  }
  gst_rtp_buffer_unmap (&rtp);

  GST_BUFFER_PTS (buffer) = timestamp;

  /* get the offset in RTP time */
  GST_BUFFER_OFFSET (buffer) = priv->bytes_to_rtptime (payload, priv->offset);

  priv->offset += payload_len;

  /* Set the duration from the size */
  GST_BUFFER_DURATION (buffer) = priv->bytes_to_time (payload, payload_len);

  /* remember the last rtptime/timestamp pair. We will use this to realign our
   * RTP timestamp after a buffer discont */
  priv->last_rtptime = GST_BUFFER_OFFSET (buffer);
  priv->last_timestamp = timestamp;
}

/**
 * gst_rtp_base_audio_payload_push:
 * @baseaudiopayload: a #GstRTPBasePayload
 * @data: (array length=payload_len): data to set as payload
 * @payload_len: length of payload
 * @timestamp: a #GstClockTime
 *
 * Create an RTP buffer and store @payload_len bytes of @data as the
 * payload. Set the timestamp on the new buffer to @timestamp before pushing
 * the buffer downstream.
 *
 * Returns: a #GstFlowReturn
 */
GstFlowReturn
gst_rtp_base_audio_payload_push (GstRTPBaseAudioPayload * baseaudiopayload,
    const guint8 * data, guint payload_len, GstClockTime timestamp)
{
  GstRTPBasePayload *basepayload;
  GstBuffer *outbuf;
  guint8 *payload;
  GstFlowReturn ret;
  GstRTPBuffer rtp = { NULL };

  basepayload = GST_RTP_BASE_PAYLOAD (baseaudiopayload);

  GST_DEBUG_OBJECT (baseaudiopayload, "Pushing %d bytes ts %" GST_TIME_FORMAT,
      payload_len, GST_TIME_ARGS (timestamp));

  /* create buffer to hold the payload */
  outbuf = gst_rtp_base_payload_allocate_output_buffer (basepayload,
      payload_len, 0, 0);

  /* copy payload */
  gst_rtp_buffer_map (outbuf, GST_MAP_WRITE, &rtp);
  payload = gst_rtp_buffer_get_payload (&rtp);
  memcpy (payload, data, payload_len);
  gst_rtp_buffer_unmap (&rtp);

  /* set metadata */
  gst_rtp_base_audio_payload_set_meta (baseaudiopayload, outbuf, payload_len,
      timestamp);

  ret = gst_rtp_base_payload_push (basepayload, outbuf);

  return ret;
}

typedef struct
{
  GstRTPBaseAudioPayload *pay;
  GstBuffer *outbuf;
} CopyMetaData;

static gboolean
foreach_metadata (GstBuffer * inbuf, GstMeta ** meta, gpointer user_data)
{
  CopyMetaData *data = user_data;
  GstRTPBaseAudioPayload *pay = data->pay;
  GstBuffer *outbuf = data->outbuf;
  const GstMetaInfo *info = (*meta)->info;
  const gchar *const *tags = gst_meta_api_type_get_tags (info->api);

  if (info->transform_func && (!tags || (g_strv_length ((gchar **) tags) == 1
              && gst_meta_api_type_has_tag (info->api, META_TAG_AUDIO)))) {
    GstMetaTransformCopy copy_data = { FALSE, 0, -1 };
    GST_DEBUG_OBJECT (pay, "copy metadata %s", g_type_name (info->api));
    /* simply copy then */
    info->transform_func (outbuf, *meta, inbuf,
        _gst_meta_transform_copy, &copy_data);
  } else {
    GST_DEBUG_OBJECT (pay, "not copying metadata %s", g_type_name (info->api));
  }

  return TRUE;
}

static GstFlowReturn
gst_rtp_base_audio_payload_push_buffer (GstRTPBaseAudioPayload *
    baseaudiopayload, GstBuffer * buffer, GstClockTime timestamp)
{
  GstRTPBasePayload *basepayload;
  GstRTPBaseAudioPayloadPrivate *priv;
  GstBuffer *outbuf;
  guint payload_len;
  GstFlowReturn ret;

  priv = baseaudiopayload->priv;
  basepayload = GST_RTP_BASE_PAYLOAD (baseaudiopayload);

  payload_len = gst_buffer_get_size (buffer);

  GST_DEBUG_OBJECT (baseaudiopayload, "Pushing %d bytes ts %" GST_TIME_FORMAT,
      payload_len, GST_TIME_ARGS (timestamp));

  /* create just the RTP header buffer */
  outbuf = gst_rtp_base_payload_allocate_output_buffer (basepayload, 0, 0, 0);

  /* set metadata */
  gst_rtp_base_audio_payload_set_meta (baseaudiopayload, outbuf, payload_len,
      timestamp);

  if (priv->buffer_list) {
    GstBufferList *list;
    guint i, len;

    list = gst_buffer_list_new ();
    len = gst_buffer_list_length (list);

    for (i = 0; i < len; i++) {
      /* FIXME */
      g_warning ("bufferlist not implemented");
      gst_buffer_list_add (list, outbuf);
      gst_buffer_list_add (list, buffer);
    }

    GST_DEBUG_OBJECT (baseaudiopayload, "Pushing list %p", list);
    ret = gst_rtp_base_payload_push_list (basepayload, list);
  } else {
    CopyMetaData data;

    /* copy payload */
    data.pay = baseaudiopayload;
    data.outbuf = outbuf;
    gst_buffer_foreach_meta (buffer, foreach_metadata, &data);
    outbuf = gst_buffer_append (outbuf, buffer);

    GST_DEBUG_OBJECT (baseaudiopayload, "Pushing buffer %p", outbuf);
    ret = gst_rtp_base_payload_push (basepayload, outbuf);
  }

  return ret;
}

/**
 * gst_rtp_base_audio_payload_flush:
 * @baseaudiopayload: a #GstRTPBasePayload
 * @payload_len: length of payload
 * @timestamp: a #GstClockTime
 *
 * Create an RTP buffer and store @payload_len bytes of the adapter as the
 * payload. Set the timestamp on the new buffer to @timestamp before pushing
 * the buffer downstream.
 *
 * If @payload_len is -1, all pending bytes will be flushed. If @timestamp is
 * -1, the timestamp will be calculated automatically.
 *
 * Returns: a #GstFlowReturn
 */
GstFlowReturn
gst_rtp_base_audio_payload_flush (GstRTPBaseAudioPayload * baseaudiopayload,
    guint payload_len, GstClockTime timestamp)
{
  GstRTPBasePayload *basepayload;
  GstRTPBaseAudioPayloadPrivate *priv;
  GstBuffer *outbuf;
  GstFlowReturn ret;
  GstAdapter *adapter;
  guint64 distance;

  priv = baseaudiopayload->priv;
  adapter = priv->adapter;

  basepayload = GST_RTP_BASE_PAYLOAD (baseaudiopayload);

  if (payload_len == -1)
    payload_len = gst_adapter_available (adapter);

  /* nothing to do, just return */
  if (payload_len == 0)
    return GST_FLOW_OK;

  if (timestamp == -1) {
    /* calculate the timestamp */
    timestamp = gst_adapter_prev_pts (adapter, &distance);

    GST_LOG_OBJECT (baseaudiopayload,
        "last timestamp %" GST_TIME_FORMAT ", distance %" G_GUINT64_FORMAT,
        GST_TIME_ARGS (timestamp), distance);

    if (GST_CLOCK_TIME_IS_VALID (timestamp) && distance > 0) {
      /* convert the number of bytes since the last timestamp to time and add to
       * the last seen timestamp */
      timestamp += priv->bytes_to_time (baseaudiopayload, distance);
    }
  }

  GST_DEBUG_OBJECT (baseaudiopayload, "Pushing %d bytes ts %" GST_TIME_FORMAT,
      payload_len, GST_TIME_ARGS (timestamp));

  if (priv->buffer_list && gst_adapter_available_fast (adapter) >= payload_len) {
    GstBuffer *buffer;
    /* we can quickly take a buffer out of the adapter without having to copy
     * anything. */
    buffer = gst_adapter_take_buffer (adapter, payload_len);

    ret =
        gst_rtp_base_audio_payload_push_buffer (baseaudiopayload, buffer,
        timestamp);
  } else {
    GstBuffer *paybuf;
    CopyMetaData data;


    /* create buffer to hold the payload */
    outbuf = gst_rtp_base_payload_allocate_output_buffer (basepayload, 0, 0, 0);

    paybuf = gst_adapter_take_buffer_fast (adapter, payload_len);

    data.pay = baseaudiopayload;
    data.outbuf = outbuf;
    gst_buffer_foreach_meta (paybuf, foreach_metadata, &data);
    outbuf = gst_buffer_append (outbuf, paybuf);

    /* set metadata */
    gst_rtp_base_audio_payload_set_meta (baseaudiopayload, outbuf, payload_len,
        timestamp);

    ret = gst_rtp_base_payload_push (basepayload, outbuf);
  }

  return ret;
}

#define ALIGN_DOWN(val,len) ((val) - ((val) % (len)))

/* calculate the min and max length of a packet. This depends on the configured
 * mtu and min/max_ptime values. We cache those so that we don't have to redo
 * all the calculations */
static gboolean
gst_rtp_base_audio_payload_get_lengths (GstRTPBasePayload * basepayload,
    guint csrc_count, guint * min_payload_len, guint * max_payload_len,
    guint * align)
{
  GstRTPBaseAudioPayload *payload;
  GstRTPBaseAudioPayloadPrivate *priv;
  guint max_mtu, mtu;
  guint maxptime_octets;
  guint minptime_octets;
  guint ptime_mult_octets;

  payload = GST_RTP_BASE_AUDIO_PAYLOAD_CAST (basepayload);
  priv = payload->priv;

  if (priv->align == 0)
    return FALSE;

  mtu = GST_RTP_BASE_PAYLOAD_MTU (payload);

  /* check cached values. Since csrc_count may vary for each packet, we only
   * check whether the new value exceeds the cached value and thus result in
   * smaller payload. */
  if (G_LIKELY (priv->cached_mtu == mtu
          && priv->cached_ptime_multiple ==
          basepayload->ptime_multiple
          && priv->cached_ptime == basepayload->ptime
          && priv->cached_max_ptime == basepayload->max_ptime
          && priv->cached_min_ptime == basepayload->min_ptime
          && priv->cached_csrc_count >= csrc_count)) {
    /* if nothing changed, return cached values */
    *min_payload_len = priv->cached_min_length;
    *max_payload_len = priv->cached_max_length;
    *align = priv->cached_align;
    return TRUE;
  }

  ptime_mult_octets = priv->time_to_bytes (payload,
      basepayload->ptime_multiple);
  *align = ALIGN_DOWN (MAX (priv->align, ptime_mult_octets), priv->align);

  /* ptime max */
  if (basepayload->max_ptime != -1) {
    maxptime_octets = priv->time_to_bytes (payload, basepayload->max_ptime);
  } else {
    maxptime_octets = G_MAXUINT;
  }
  /* MTU max */
  max_mtu = gst_rtp_buffer_calc_payload_len (mtu, 0, csrc_count);
  /* round down to alignment */
  max_mtu = ALIGN_DOWN (max_mtu, *align);

  /* combine max ptime and max payload length */
  *max_payload_len = MIN (max_mtu, maxptime_octets);

  /* min number of bytes based on a given ptime */
  minptime_octets = priv->time_to_bytes (payload, basepayload->min_ptime);
  /* must be at least one frame size */
  *min_payload_len = MAX (minptime_octets, *align);

  if (*min_payload_len > *max_payload_len)
    *min_payload_len = *max_payload_len;

  /* If the ptime is specified in the caps, tried to adhere to it exactly */
  if (basepayload->ptime) {
    guint ptime_in_bytes = priv->time_to_bytes (payload,
        basepayload->ptime);

    /* clip to computed min and max lengths */
    ptime_in_bytes = MAX (*min_payload_len, ptime_in_bytes);
    ptime_in_bytes = MIN (*max_payload_len, ptime_in_bytes);

    *min_payload_len = *max_payload_len = ptime_in_bytes;
  }

  /* cache values */
  priv->cached_mtu = mtu;
  priv->cached_ptime = basepayload->ptime;
  priv->cached_min_ptime = basepayload->min_ptime;
  priv->cached_max_ptime = basepayload->max_ptime;
  priv->cached_ptime_multiple = basepayload->ptime_multiple;
  priv->cached_min_length = *min_payload_len;
  priv->cached_max_length = *max_payload_len;
  priv->cached_align = *align;
  priv->cached_csrc_count = csrc_count;

  return TRUE;
}

/* frame conversions functions */
static GstClockTime
gst_rtp_base_audio_payload_frame_bytes_to_time (GstRTPBaseAudioPayload *
    payload, guint64 bytes)
{
  guint64 framecount;

  framecount = bytes / payload->frame_size;
  if (G_UNLIKELY (bytes % payload->frame_size))
    framecount++;

  return framecount * payload->priv->frame_duration_ns;
}

static guint32
gst_rtp_base_audio_payload_frame_bytes_to_rtptime (GstRTPBaseAudioPayload *
    payload, guint64 bytes)
{
  guint64 framecount;
  guint64 time;

  framecount = bytes / payload->frame_size;
  if (G_UNLIKELY (bytes % payload->frame_size))
    framecount++;

  time = framecount * payload->priv->frame_duration_ns;

  return gst_util_uint64_scale_int (time,
      GST_RTP_BASE_PAYLOAD (payload)->clock_rate, GST_SECOND);
}

static guint64
gst_rtp_base_audio_payload_frame_time_to_bytes (GstRTPBaseAudioPayload *
    payload, GstClockTime time)
{
  return gst_util_uint64_scale (time, payload->frame_size,
      payload->priv->frame_duration_ns);
}

/* sample conversion functions */
static GstClockTime
gst_rtp_base_audio_payload_sample_bytes_to_time (GstRTPBaseAudioPayload *
    payload, guint64 bytes)
{
  guint64 rtptime;

  /* avoid division when we can */
  if (G_LIKELY (payload->sample_size != 8))
    rtptime = gst_util_uint64_scale_int (bytes, 8, payload->sample_size);
  else
    rtptime = bytes;

  return gst_util_uint64_scale_int (rtptime, GST_SECOND,
      GST_RTP_BASE_PAYLOAD (payload)->clock_rate);
}

static guint32
gst_rtp_base_audio_payload_sample_bytes_to_rtptime (GstRTPBaseAudioPayload *
    payload, guint64 bytes)
{
  /* avoid division when we can */
  if (G_LIKELY (payload->sample_size != 8))
    return gst_util_uint64_scale_int (bytes, 8, payload->sample_size);
  else
    return bytes;
}

static guint64
gst_rtp_base_audio_payload_sample_time_to_bytes (GstRTPBaseAudioPayload *
    payload, guint64 time)
{
  guint64 samples;

  samples = gst_util_uint64_scale_int (time,
      GST_RTP_BASE_PAYLOAD (payload)->clock_rate, GST_SECOND);

  /* avoid multiplication when we can */
  if (G_LIKELY (payload->sample_size != 8))
    return gst_util_uint64_scale_int (samples, payload->sample_size, 8);
  else
    return samples;
}

static GstFlowReturn
gst_rtp_base_audio_payload_handle_buffer (GstRTPBasePayload *
    basepayload, GstBuffer * buffer)
{
  GstRTPBaseAudioPayload *payload;
  GstRTPBaseAudioPayloadPrivate *priv;
  guint payload_len;
  GstFlowReturn ret;
  guint available;
  guint min_payload_len;
  guint max_payload_len;
  guint align;
  guint size;
  gboolean discont;
  GstClockTime timestamp;

  ret = GST_FLOW_OK;

  payload = GST_RTP_BASE_AUDIO_PAYLOAD_CAST (basepayload);
  priv = payload->priv;

  timestamp = GST_BUFFER_PTS (buffer);
  discont = GST_BUFFER_IS_DISCONT (buffer);
  if (discont) {

    GST_DEBUG_OBJECT (payload, "Got DISCONT");
    /* flush everything out of the adapter, mark DISCONT */
    ret = gst_rtp_base_audio_payload_flush (payload, -1, -1);
    priv->discont = TRUE;

    /* get the distance between the timestamp gap and produce the same gap in
     * the RTP timestamps */
    if (priv->last_timestamp != -1 && timestamp != -1) {
      /* we had a last timestamp, compare it to the new timestamp and update the
       * offset counter for RTP timestamps. The effect is that we will produce
       * output buffers containing the same RTP timestamp gap as the gap
       * between the GST timestamps. */
      if (timestamp > priv->last_timestamp) {
        GstClockTime diff;
        guint64 bytes;
        /* we're only going to apply a positive gap, otherwise we let the marker
         * bit do its thing. simply convert to bytes and add the current
         * offset */
        diff = timestamp - priv->last_timestamp;
        bytes = priv->time_to_bytes (payload, diff);
        priv->offset += bytes;

        GST_DEBUG_OBJECT (payload,
            "elapsed time %" GST_TIME_FORMAT ", bytes %" G_GUINT64_FORMAT
            ", new offset %" G_GUINT64_FORMAT, GST_TIME_ARGS (diff), bytes,
            priv->offset);
      }
    }
  }

  if (!gst_rtp_base_audio_payload_get_lengths (basepayload,
          gst_rtp_base_payload_get_source_count (basepayload, buffer),
          &min_payload_len, &max_payload_len, &align))
    goto config_error;

  GST_DEBUG_OBJECT (payload,
      "Calculated min_payload_len %u and max_payload_len %u",
      min_payload_len, max_payload_len);

  size = gst_buffer_get_size (buffer);

  /* shortcut, we don't need to use the adapter when the packet can be pushed
   * through directly. */
  available = gst_adapter_available (priv->adapter);

  GST_DEBUG_OBJECT (payload, "got buffer size %u, available %u",
      size, available);

  if (available == 0 && (size >= min_payload_len && size <= max_payload_len) &&
      (size % align == 0)) {
    /* If buffer fits on an RTP packet, let's just push it through
     * this will check against max_ptime and max_mtu */
    GST_DEBUG_OBJECT (payload, "Fast packet push");
    ret = gst_rtp_base_audio_payload_push_buffer (payload, buffer, timestamp);
  } else {
    /* push the buffer in the adapter */
    gst_adapter_push (priv->adapter, buffer);
    available += size;

    GST_DEBUG_OBJECT (payload, "available now %u", available);

    /* as long as we have full frames */
    /* TODO: Use buffer lists here */
    while (available >= min_payload_len) {
      /* get multiple of alignment */
      payload_len = MIN (max_payload_len, available);
      payload_len = ALIGN_DOWN (payload_len, align);

      /* and flush out the bytes from the adapter, automatically set the
       * timestamp. */
      ret = gst_rtp_base_audio_payload_flush (payload, payload_len, -1);

      available -= payload_len;
      GST_DEBUG_OBJECT (payload, "available after push %u", available);
    }
  }
  return ret;

  /* ERRORS */
config_error:
  {
    GST_ELEMENT_ERROR (payload, STREAM, NOT_IMPLEMENTED, (NULL),
        ("subclass did not configure us properly"));
    gst_buffer_unref (buffer);
    return GST_FLOW_ERROR;
  }
}

static GstStateChangeReturn
gst_rtp_base_payload_audio_change_state (GstElement * element,
    GstStateChange transition)
{
  GstRTPBaseAudioPayload *rtpbasepayload;
  GstStateChangeReturn ret;

  rtpbasepayload = GST_RTP_BASE_AUDIO_PAYLOAD (element);

  switch (transition) {
    case GST_STATE_CHANGE_READY_TO_PAUSED:
      rtpbasepayload->priv->cached_mtu = -1;
      rtpbasepayload->priv->last_rtptime = -1;
      rtpbasepayload->priv->last_timestamp = -1;
      break;
    default:
      break;
  }

  ret = GST_ELEMENT_CLASS (parent_class)->change_state (element, transition);

  switch (transition) {
    case GST_STATE_CHANGE_PAUSED_TO_READY:
      gst_adapter_clear (rtpbasepayload->priv->adapter);
      break;
    default:
      break;
  }

  return ret;
}

static gboolean
gst_rtp_base_payload_audio_sink_event (GstRTPBasePayload * basep,
    GstEvent * event)
{
  GstRTPBaseAudioPayload *payload;
  gboolean res = FALSE;

  payload = GST_RTP_BASE_AUDIO_PAYLOAD (basep);

  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_EOS:
      /* flush remaining bytes in the adapter */
      gst_rtp_base_audio_payload_flush (payload, -1, -1);
      break;
    case GST_EVENT_FLUSH_STOP:
      gst_adapter_clear (payload->priv->adapter);
      break;
    default:
      break;
  }

  /* let parent handle the remainder of the event */
  res = GST_RTP_BASE_PAYLOAD_CLASS (parent_class)->sink_event (basep, event);

  return res;
}

/**
 * gst_rtp_base_audio_payload_get_adapter:
 * @rtpbaseaudiopayload: a #GstRTPBaseAudioPayload
 *
 * Gets the internal adapter used by the depayloader.
 *
 * Returns: (transfer full): a #GstAdapter.
 */
GstAdapter *
gst_rtp_base_audio_payload_get_adapter (GstRTPBaseAudioPayload
    * rtpbaseaudiopayload)
{
  GstAdapter *adapter;

  if ((adapter = rtpbaseaudiopayload->priv->adapter))
    g_object_ref (adapter);

  return adapter;
}
