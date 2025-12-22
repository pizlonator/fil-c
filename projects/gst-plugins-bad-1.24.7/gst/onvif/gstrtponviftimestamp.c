/*
 * gstrtponviftimestamp.h
 *
 * Copyright (C) 2014 Axis Communications AB
 *  Author: Guillaume Desmottes <guillaume.desmottes@collabora.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <gst/rtp/gstrtpbuffer.h>

#include "gstrtponviftimestamp.h"

#define GST_ONVIF_TIMESTAMP_EVENT_NAME "GstOnvifTimestamp"

#define DEFAULT_NTP_OFFSET GST_CLOCK_TIME_NONE
#define DEFAULT_CSEQ 0
#define DEFAULT_SET_E_BIT FALSE
#define DEFAULT_SET_T_BIT FALSE
#define DEFAULT_DROP_OUT_OF_SEGMENT TRUE
#define DEFAULT_USE_REFERENCE_TIMESTAMPS FALSE

GST_DEBUG_CATEGORY_STATIC (rtponviftimestamp_debug);
#define GST_CAT_DEFAULT (rtponviftimestamp_debug)

static GstFlowReturn gst_rtp_onvif_timestamp_chain (GstPad * pad,
    GstObject * parent, GstBuffer * buf);
static GstFlowReturn gst_rtp_onvif_timestamp_chain_list (GstPad * pad,
    GstObject * parent, GstBufferList * list);

static GstFlowReturn handle_and_push_buffer (GstRtpOnvifTimestamp * self,
    GstBuffer * buf);
static GstFlowReturn handle_and_push_buffer_list (GstRtpOnvifTimestamp * self,
    GstBufferList * list);

static GstStaticPadTemplate sink_template_factory =
GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("application/x-rtp")
    );

static GstStaticPadTemplate src_template_factory =
GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("application/x-rtp")
    );

enum
{
  PROP_0,
  PROP_NTP_OFFSET,
  PROP_CSEQ,
  PROP_SET_E_BIT,
  PROP_SET_T_BIT,
  PROP_DROP_OUT_OF_SEGMENT,
  PROP_USE_REFERENCE_TIMESTAMPS
};

/*static guint gst_rtp_onvif_timestamp_signals[LAST_SIGNAL] = { 0 }; */

G_DEFINE_TYPE (GstRtpOnvifTimestamp, gst_rtp_onvif_timestamp, GST_TYPE_ELEMENT);
GST_ELEMENT_REGISTER_DEFINE (rtponviftimestamp, "rtponviftimestamp",
    GST_RANK_NONE, GST_TYPE_RTP_ONVIF_TIMESTAMP);

static void
gst_rtp_onvif_timestamp_get_property (GObject * object,
    guint prop_id, GValue * value, GParamSpec * pspec)
{
  GstRtpOnvifTimestamp *self = GST_RTP_ONVIF_TIMESTAMP (object);

  switch (prop_id) {
    case PROP_NTP_OFFSET:
      g_value_set_uint64 (value, self->prop_ntp_offset);
      break;
    case PROP_CSEQ:
      g_value_set_uint (value, self->prop_cseq);
      break;
    case PROP_SET_E_BIT:
      g_value_set_boolean (value, self->prop_set_e_bit);
      break;
    case PROP_SET_T_BIT:
      g_value_set_boolean (value, self->prop_set_t_bit);
      break;
    case PROP_DROP_OUT_OF_SEGMENT:
      g_value_set_boolean (value, self->prop_drop_out_of_segment);
      break;
    case PROP_USE_REFERENCE_TIMESTAMPS:
      g_value_set_boolean (value, self->prop_use_reference_timestamps);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_rtp_onvif_timestamp_set_property (GObject * object,
    guint prop_id, const GValue * value, GParamSpec * pspec)
{
  GstRtpOnvifTimestamp *self = GST_RTP_ONVIF_TIMESTAMP (object);

  switch (prop_id) {
    case PROP_NTP_OFFSET:
      self->prop_ntp_offset = g_value_get_uint64 (value);
      break;
    case PROP_CSEQ:
      self->prop_cseq = g_value_get_uint (value);
      break;
    case PROP_SET_E_BIT:
      self->prop_set_e_bit = g_value_get_boolean (value);
      break;
    case PROP_SET_T_BIT:
      self->prop_set_t_bit = g_value_get_boolean (value);
      break;
    case PROP_DROP_OUT_OF_SEGMENT:
      self->prop_drop_out_of_segment = g_value_get_boolean (value);
      break;
    case PROP_USE_REFERENCE_TIMESTAMPS:
      self->prop_use_reference_timestamps = g_value_get_boolean (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

/* send cached buffer or list, and events, if present */
static GstFlowReturn
send_cached_buffer_and_events (GstRtpOnvifTimestamp * self)
{
  GstFlowReturn ret = GST_FLOW_OK;

  g_assert (!(self->buffer && self->list));

  if (self->buffer) {
    GST_DEBUG_OBJECT (self, "pushing %" GST_PTR_FORMAT, self->buffer);
    ret = handle_and_push_buffer (self, self->buffer);
    self->buffer = NULL;
  }
  if (self->list) {
    GST_DEBUG_OBJECT (self, "pushing %" GST_PTR_FORMAT, self->list);
    ret = handle_and_push_buffer_list (self, self->list);
    self->list = NULL;
  }

  if (ret != GST_FLOW_OK)
    goto out;

  while (!g_queue_is_empty (self->event_queue)) {
    GstEvent *event;

    event = GST_EVENT_CAST (g_queue_pop_head (self->event_queue));
    GST_LOG_OBJECT (self->sinkpad, "sending %" GST_PTR_FORMAT, event);
    (void) gst_pad_send_event (self->sinkpad, event);
  }

out:
  return ret;
}

static void
purge_cached_buffer_and_events (GstRtpOnvifTimestamp * self)
{
  g_assert (!(self->buffer && self->list));

  if (self->buffer) {
    GST_DEBUG_OBJECT (self, "purging %" GST_PTR_FORMAT, self->buffer);
    gst_buffer_unref (self->buffer);
    self->buffer = NULL;
  }
  if (self->list) {
    GST_DEBUG_OBJECT (self, "purging %" GST_PTR_FORMAT, self->list);
    gst_buffer_list_unref (self->list);
    self->list = NULL;
  }

  while (!g_queue_is_empty (self->event_queue)) {
    GstEvent *event;

    event = GST_EVENT_CAST (g_queue_pop_head (self->event_queue));
    gst_event_unref (event);
  }
}

static GstStateChangeReturn
gst_rtp_onvif_timestamp_change_state (GstElement * element,
    GstStateChange transition)
{
  GstRtpOnvifTimestamp *self = GST_RTP_ONVIF_TIMESTAMP (element);
  GstStateChangeReturn ret;

  switch (transition) {
    case GST_STATE_CHANGE_READY_TO_PAUSED:
      if (self->prop_use_reference_timestamps &&
          self->prop_ntp_offset != DEFAULT_NTP_OFFSET) {
        GST_WARNING_OBJECT (self, "ntp-offset should not be set if reference "
            "timestamps are used");
        self->ntp_offset = DEFAULT_NTP_OFFSET;
      } else if (self->prop_use_reference_timestamps) {
        GST_DEBUG_OBJECT (self, "using reference timestamp meta");
      } else {
        self->ntp_offset = self->prop_ntp_offset;
        GST_DEBUG_OBJECT (self, "ntp-offset: %" GST_TIME_FORMAT,
            GST_TIME_ARGS (self->ntp_offset));
      }
      self->set_d_bit = TRUE;
      self->set_e_bit = FALSE;
      self->set_t_bit = FALSE;
      break;
    default:
      break;
  }

  ret = GST_ELEMENT_CLASS (gst_rtp_onvif_timestamp_parent_class)->change_state
      (element, transition);

  if (ret == GST_STATE_CHANGE_FAILURE)
    return ret;

  switch (transition) {
    case GST_STATE_CHANGE_PAUSED_TO_READY:
      purge_cached_buffer_and_events (self);
      gst_segment_init (&self->segment, GST_FORMAT_UNDEFINED);
      break;
    default:
      break;
  }

  return ret;
}

static void
gst_rtp_onvif_timestamp_finalize (GObject * object)
{
  GstRtpOnvifTimestamp *self = GST_RTP_ONVIF_TIMESTAMP (object);

  g_queue_free (self->event_queue);
  gst_caps_replace (&self->reference_timestamp_id, NULL);

  G_OBJECT_CLASS (gst_rtp_onvif_timestamp_parent_class)->finalize (object);
}

static void
gst_rtp_onvif_timestamp_class_init (GstRtpOnvifTimestampClass * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *gstelement_class;

  gobject_class = G_OBJECT_CLASS (klass);
  gstelement_class = GST_ELEMENT_CLASS (klass);

  gobject_class->get_property = gst_rtp_onvif_timestamp_get_property;
  gobject_class->set_property = gst_rtp_onvif_timestamp_set_property;
  gobject_class->finalize = gst_rtp_onvif_timestamp_finalize;

  g_object_class_install_property (gobject_class, PROP_NTP_OFFSET,
      g_param_spec_uint64 ("ntp-offset", "NTP offset",
          "Offset between the pipeline running time and the absolute UTC time, "
          "in nano-seconds since 1900 (-1 for automatic computation)",
          0, G_MAXUINT64,
          DEFAULT_NTP_OFFSET, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_CSEQ,
      g_param_spec_uint ("cseq", "CSeq",
          "The RTSP CSeq which initiated the playback",
          0, G_MAXUINT32,
          DEFAULT_CSEQ, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_SET_E_BIT,
      g_param_spec_boolean ("set-e-bit", "Set 'E' bit",
          "If the element should set the 'E' bit as defined in the ONVIF RTP "
          "extension. This increases latency by one packet",
          DEFAULT_SET_E_BIT, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_SET_T_BIT,
      g_param_spec_boolean ("set-t-bit", "Set 'T' bit",
          "If the element should set the 'T' bit as defined in the ONVIF RTP "
          "extension. This increases latency by one packet",
          DEFAULT_SET_T_BIT, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_DROP_OUT_OF_SEGMENT,
      g_param_spec_boolean ("drop-out-of-segment", "Drop out of segment",
          "Whether the element should drop buffers that fall outside the segment, "
          "not part of the specification but allows full reverse playback.",
          DEFAULT_DROP_OUT_OF_SEGMENT,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  /**
   * GstRtpOnvifTimestamp:use-reference-timestamps:
   *
   * Whether to obtain timestamps from reference timestamp meta instead of using
   * the ntp-offset method. If enabled then timestamps are expected to be
   * attached to the buffers, and in that case ntp-offset should not be
   * configured.
   *
   * Default value is FALSE, meaning that the ntp-offset property is used.
   * If neither is set then the element calculates an ntp-offset.
   *
   * Since: 1.22
   */
  g_object_class_install_property (gobject_class, PROP_USE_REFERENCE_TIMESTAMPS,
      g_param_spec_boolean ("use-reference-timestamps",
          "Use reference timestamps",
          "Whether the element should use reference UTC timestamps from the "
          "buffers instead of using the ntp-offset mechanism.",
          DEFAULT_USE_REFERENCE_TIMESTAMPS,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
          GST_PARAM_MUTABLE_READY));

  /* register pads */
  gst_element_class_add_static_pad_template (gstelement_class,
      &sink_template_factory);
  gst_element_class_add_static_pad_template (gstelement_class,
      &src_template_factory);

  gst_element_class_set_static_metadata (gstelement_class,
      "ONVIF NTP timestamps RTP extension", "Effect/RTP",
      "Add absolute timestamps and flags of recorded data in a playback "
      "session", "Guillaume Desmottes <guillaume.desmottes@collabora.com>");

  gstelement_class->change_state =
      GST_DEBUG_FUNCPTR (gst_rtp_onvif_timestamp_change_state);

  GST_DEBUG_CATEGORY_INIT (rtponviftimestamp_debug, "rtponviftimestamp",
      0, "ONVIF NTP timestamps RTP extension");
}

static gboolean
parse_event_ntp_offset (GstRtpOnvifTimestamp * self, GstEvent * event,
    GstClockTime * offset, gboolean * discont)
{
  const GstStructure *structure = gst_event_get_structure (event);
  GstClockTime event_offset;
  gboolean event_discont;

  if (!gst_structure_get_clock_time (structure, "ntp-offset", &event_offset)) {
    GST_ERROR_OBJECT (self, "no ntp-offset in %" GST_PTR_FORMAT, event);
    return FALSE;
  }
  if (!gst_structure_get_boolean (structure, "discont", &event_discont)) {
    GST_ERROR_OBJECT (self, "no discontinue in %" GST_PTR_FORMAT, event);
    return FALSE;
  }

  if (offset)
    *offset = event_offset;

  if (discont)
    *discont = event_discont;

  return TRUE;
}

static gboolean
gst_rtp_onvif_timestamp_sink_event (GstPad * pad, GstObject * parent,
    GstEvent * event)
{
  GstRtpOnvifTimestamp *self = GST_RTP_ONVIF_TIMESTAMP (parent);
  gboolean drop = FALSE;
  gboolean ret = TRUE;

  GST_DEBUG_OBJECT (pad, "handling event %s", GST_EVENT_TYPE_NAME (event));

  /* handle serialized events, which, should not be enqueued */
  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_CUSTOM_DOWNSTREAM:
      /* if the "set-e-bit" property is set, an offset event might mark the
       * stream as discontinued. We need to check if the currently cached buffer
       * or buffer list needs the e-bit before it's pushed */
      if ((self->buffer != NULL || self->list != NULL) && self->prop_set_e_bit
          && gst_event_has_name (event, GST_ONVIF_TIMESTAMP_EVENT_NAME)) {
        gboolean discont;
        if (parse_event_ntp_offset (self, event, NULL, &discont)) {
          GST_DEBUG_OBJECT (self, "stream %s discontinued",
              (discont ? "is" : "is not"));
          self->set_e_bit = discont;
        } else {
          drop = TRUE;
          ret = FALSE;
          goto out;
        }
      }
      break;
    case GST_EVENT_EOS:
    {
      GstFlowReturn res;

      /* Push pending buffers, if any */
      self->set_e_bit = TRUE;
      if (self->prop_set_t_bit)
        self->set_t_bit = TRUE;
      res = send_cached_buffer_and_events (self);
      if (res != GST_FLOW_OK) {
        drop = TRUE;
        ret = FALSE;
        goto out;
      }
      break;
    }
    case GST_EVENT_FLUSH_STOP:
      purge_cached_buffer_and_events (self);
      self->set_d_bit = TRUE;
      self->set_e_bit = FALSE;
      self->set_t_bit = FALSE;
      gst_segment_init (&self->segment, GST_FORMAT_UNDEFINED);
      break;
    default:
      break;
  }

  /* enqueue serialized events if there is a cached buffer */
  if (GST_EVENT_IS_SERIALIZED (event) && (self->buffer || self->list)) {
    GST_WARNING ("enqueueing serialized event");
    g_queue_push_tail (self->event_queue, event);
    event = NULL;
    goto out;
  }

  /* handle rest of the events */
  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_CUSTOM_DOWNSTREAM:
      /* update the ntp-offset after any cached buffer/buffer list has been
       * pushed. the d-bit of the next buffer/buffer list should be set if
       * the stream is discontinued */
      if (gst_event_has_name (event, GST_ONVIF_TIMESTAMP_EVENT_NAME)) {
        GstClockTime offset;
        gboolean discont;
        if (parse_event_ntp_offset (self, event, &offset, &discont)) {
          GST_DEBUG_OBJECT (self, "new ntp-offset: %" GST_TIME_FORMAT
              ", stream %s discontinued", GST_TIME_ARGS (offset),
              (discont ? "is" : "is not"));
          self->ntp_offset = offset;
          self->set_d_bit = discont;
        } else {
          ret = FALSE;
        }
        drop = TRUE;
      }
      break;
    case GST_EVENT_SEGMENT:
      gst_event_copy_segment (event, &self->segment);
      break;
    default:
      break;
  }

out:
  if (drop)
    gst_event_unref (event);
  else if (event)
    ret = gst_pad_event_default (pad, parent, event);

  return ret;
}

static void
gst_rtp_onvif_timestamp_init (GstRtpOnvifTimestamp * self)
{
  self->sinkpad =
      gst_pad_new_from_static_template (&sink_template_factory, "sink");
  gst_pad_set_chain_function (self->sinkpad, gst_rtp_onvif_timestamp_chain);
  gst_pad_set_chain_list_function (self->sinkpad,
      gst_rtp_onvif_timestamp_chain_list);
  gst_pad_set_event_function (self->sinkpad,
      gst_rtp_onvif_timestamp_sink_event);
  gst_element_add_pad (GST_ELEMENT (self), self->sinkpad);
  GST_PAD_SET_PROXY_CAPS (self->sinkpad);
  GST_PAD_SET_PROXY_ALLOCATION (self->sinkpad);

  self->srcpad =
      gst_pad_new_from_static_template (&src_template_factory, "src");
  gst_element_add_pad (GST_ELEMENT (self), self->srcpad);

  self->prop_use_reference_timestamps = DEFAULT_USE_REFERENCE_TIMESTAMPS;
  self->reference_timestamp_id = gst_caps_new_empty_simple ("timestamp/x-unix");

  self->prop_ntp_offset = DEFAULT_NTP_OFFSET;
  self->prop_set_e_bit = DEFAULT_SET_E_BIT;
  self->prop_set_t_bit = DEFAULT_SET_T_BIT;
  self->prop_drop_out_of_segment = DEFAULT_DROP_OUT_OF_SEGMENT;

  gst_segment_init (&self->segment, GST_FORMAT_UNDEFINED);

  self->event_queue = g_queue_new ();
  self->buffer = NULL;
  self->list = NULL;
}

#define EXTENSION_ID 0xABAC
#define EXTENSION_SIZE 3

static guint64
get_utc_from_reference_timestamp (GstRtpOnvifTimestamp * self, GstBuffer * buf)
{
  GstReferenceTimestampMeta *meta;
  GstClockTime time;

  meta = gst_buffer_get_reference_timestamp_meta (buf,
      self->reference_timestamp_id);
  if (meta != NULL) {
    /* the reference timestamp is expressed in unix times so add the difference
     * between unix and ntp epochs */
    time = meta->timestamp + G_GUINT64_CONSTANT (2208988800) * GST_SECOND;
    GST_TRACE_OBJECT (self, "UTC reference timestamp found: %" GST_TIME_FORMAT,
        GST_TIME_ARGS (time));
  } else {
    GST_ERROR_OBJECT (self, "UTC reference timestamp not found");
    time = GST_CLOCK_TIME_NONE;
  }

  return time;
}

static guint64
get_utc_from_offset (GstRtpOnvifTimestamp * self, GstBuffer * buf)
{
  guint64 time = GST_CLOCK_TIME_NONE;

  if (GST_BUFFER_PTS_IS_VALID (buf)) {
    if (gst_segment_to_stream_time_full (&self->segment, GST_FORMAT_TIME,
            GST_BUFFER_PTS (buf), &time) < 0) {
      time = GST_CLOCK_TIME_NONE;
    }
  } else if (GST_BUFFER_DTS_IS_VALID (buf)) {
    if (gst_segment_to_stream_time_full (&self->segment, GST_FORMAT_TIME,
            GST_BUFFER_DTS (buf), &time) < 0) {
      time = GST_CLOCK_TIME_NONE;
    }
  } else {
    g_assert_not_reached ();
  }

  /* add the offset (in seconds) */
  if (time != GST_CLOCK_TIME_NONE) {
    time += self->ntp_offset;
  }

  return time;
}

static gboolean
handle_buffer (GstRtpOnvifTimestamp * self, GstBuffer * buf, gboolean last)
{
  GstRTPBuffer rtp = GST_RTP_BUFFER_INIT;
  guint8 *data;
  guint16 bits;
  guint wordlen;
  guint64 time;
  guint8 field = 0;

  if (!self->prop_use_reference_timestamps &&
      !GST_CLOCK_TIME_IS_VALID (self->ntp_offset)) {
    GstClock *clock = gst_element_get_clock (GST_ELEMENT (self));

    if (clock) {
      GstClockTime clock_time = gst_clock_get_time (clock);
      guint64 real_time = g_get_real_time ();
      GstClockTime running_time = clock_time -
          gst_element_get_base_time (GST_ELEMENT (self));

      /* convert microseconds to nanoseconds */
      real_time *= 1000;

      /* add constant to convert from 1970 based time to 1900 based time */
      real_time += (G_GUINT64_CONSTANT (2208988800) * GST_SECOND);

      self->ntp_offset = real_time - running_time;

      GST_DEBUG_OBJECT (self, "new ntp-offset: %" GST_TIME_FORMAT,
          GST_TIME_ARGS (self->ntp_offset));

      gst_object_unref (clock);
    } else {
      GST_ELEMENT_ERROR (self, STREAM, FAILED, ("No ntp-offset present"),
          ("Can not guess ntp-offset with no clock."));
      /* Received a buffer in PAUSED, so we can't guess the match
       * between the running time and the NTP clock yet.
       */
      return FALSE;
    }
  }

  if (self->segment.format != GST_FORMAT_TIME) {
    GST_ELEMENT_ERROR (self, STREAM, FAILED,
        ("did not receive a time segment yet"), (NULL));
    return FALSE;
  }

  if (!gst_rtp_buffer_map (buf, GST_MAP_READWRITE, &rtp)) {
    GST_ELEMENT_ERROR (self, STREAM, FAILED,
        ("Failed to map RTP buffer"), (NULL));
    return FALSE;
  }

  if (!gst_rtp_buffer_set_extension_data (&rtp, EXTENSION_ID, EXTENSION_SIZE)) {
    GST_ELEMENT_ERROR (self, STREAM, FAILED, ("Failed to set extension data"),
        (NULL));
    gst_rtp_buffer_unmap (&rtp);
    return FALSE;
  }

  if (!gst_rtp_buffer_get_extension_data (&rtp, &bits, (gpointer) & data,
          &wordlen)) {
    GST_ELEMENT_ERROR (self, STREAM, FAILED, ("Failed to get extension data"),
        (NULL));
    gst_rtp_buffer_unmap (&rtp);
    return FALSE;
  }

  if (self->prop_use_reference_timestamps) {
    time = get_utc_from_reference_timestamp (self, buf);
    if (time == GST_CLOCK_TIME_NONE) {
      gst_rtp_buffer_unmap (&rtp);
      return FALSE;
    }
  } else if (GST_BUFFER_PTS_IS_VALID (buf) || GST_BUFFER_DTS_IS_VALID (buf)) {
    time = get_utc_from_offset (self, buf);
  } else {
    GST_INFO_OBJECT (self,
        "Buffer doesn't contain any valid DTS or PTS timestamp");
    goto done;
  }

  if (self->prop_drop_out_of_segment && !GST_CLOCK_TIME_IS_VALID (time)) {
    GST_ERROR_OBJECT (self, "Failed to get stream time");
    gst_rtp_buffer_unmap (&rtp);
    return FALSE;
  }

  /* convert to NTP time. upper 32 bits should contain the seconds
   * and the lower 32 bits, the fractions of a second. */
  if (GST_CLOCK_TIME_IS_VALID (time)) {
    time = gst_util_uint64_scale (time, (G_GINT64_CONSTANT (1) << 32),
        GST_SECOND);
  }

  GST_DEBUG_OBJECT (self, "timestamp: %" G_GUINT64_FORMAT, time);

  GST_WRITE_UINT64_BE (data, time);

  /* The next byte is composed of: C E D T mbz (4 bits) */

  /* Set C if the buffer does *not* have the DELTA_UNIT flag as it means
   * that's a key frame (or 'clean point'). */
  if (!GST_BUFFER_FLAG_IS_SET (buf, GST_BUFFER_FLAG_DELTA_UNIT)) {
    GST_DEBUG_OBJECT (self, "set C flag");
    field |= (1 << 7);
  }

  /* Set E if this the last buffer of a contiguous section of recording */
  if (last && self->set_e_bit) {
    GST_DEBUG_OBJECT (self, "set E flag");
    field |= (1 << 6);
    self->set_e_bit = FALSE;
  }

  /* Set D if the buffer has the DISCONT flag */
  if (self->set_d_bit || GST_BUFFER_FLAG_IS_SET (buf, GST_BUFFER_FLAG_DISCONT)) {
    GST_DEBUG_OBJECT (self, "set D flag");
    field |= (1 << 5);
    self->set_d_bit = FALSE;
  }

  /* Set T if we have received EOS */
  if (last && self->set_t_bit) {
    GST_DEBUG_OBJECT (self, "set T flag");
    field |= (1 << 4);
    self->set_t_bit = FALSE;
  }

  GST_WRITE_UINT8 (data + 8, field);

  /* CSeq (low-order byte) */
  GST_WRITE_UINT8 (data + 9, (guchar) self->prop_cseq);

  memset (data + 10, 0, 3);

done:
  gst_rtp_buffer_unmap (&rtp);
  return TRUE;
}

/* @buf: (transfer full) */
static GstFlowReturn
handle_and_push_buffer (GstRtpOnvifTimestamp * self, GstBuffer * buf)
{
  if (!handle_buffer (self, buf, TRUE)) {
    gst_buffer_unref (buf);
    return GST_FLOW_ERROR;
  }

  return gst_pad_push (self->srcpad, buf);
}

static GstFlowReturn
gst_rtp_onvif_timestamp_chain (GstPad * pad, GstObject * parent,
    GstBuffer * buf)
{
  GstRtpOnvifTimestamp *self = GST_RTP_ONVIF_TIMESTAMP (parent);
  GstFlowReturn result = GST_FLOW_OK;

  if (!self->prop_set_e_bit && !self->prop_set_t_bit) {
    /* Modify and push this buffer right away */
    return handle_and_push_buffer (self, buf);
  }

  /* send any previously cached item(s), this leaves an empty queue */
  result = send_cached_buffer_and_events (self);

  /* enqueue the new item, as the only item in the queue */
  self->buffer = buf;
  return result;
}

static gboolean
do_handle_buffer (GstBuffer ** buffer, guint idx, GstRtpOnvifTimestamp * self)
{
  return handle_buffer (self, *buffer, idx + 1 == self->current_list_size);
}

/* @buf: (transfer full) */
static GstFlowReturn
handle_and_push_buffer_list (GstRtpOnvifTimestamp * self, GstBufferList * list)
{
  self->current_list_size = gst_buffer_list_length (list);

  if (!gst_buffer_list_foreach (list, (GstBufferListFunc) do_handle_buffer,
          self)) {
    gst_buffer_list_unref (list);
    return GST_FLOW_ERROR;
  }

  return gst_pad_push_list (self->srcpad, list);
}

/* gst_pad_chain_list_default() refs the buffer when passing it to the chain
 * function, making it not writable. We implement our own chain_list function
 * to avoid having to copy each buffer. */
static GstFlowReturn
gst_rtp_onvif_timestamp_chain_list (GstPad * pad, GstObject * parent,
    GstBufferList * list)
{
  GstRtpOnvifTimestamp *self = GST_RTP_ONVIF_TIMESTAMP (parent);
  GstFlowReturn result = GST_FLOW_OK;

  if (!self->prop_set_e_bit && !self->prop_set_t_bit) {
    return handle_and_push_buffer_list (self, list);
  }

  /* send any previously cached item(s), this leaves an empty queue */
  result = send_cached_buffer_and_events (self);

  /* enqueue the new item, as the only item in the queue */
  self->list = list;
  return result;
}
