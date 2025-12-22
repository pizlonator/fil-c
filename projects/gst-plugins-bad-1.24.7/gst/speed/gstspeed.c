/* -*- c-basic-offset: 2 -*-
 * GStreamer
 * Copyright (C) 1999-2001 Erik Walthinsen <omega@cse.ogi.edu>
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
 * SECTION:element-speed
 * @title: speed
 *
 * Plays an audio stream at a different speed (by resampling the audio).
 *
 * Do not use this element. Either use the 'pitch' element, or do a seek with
 * a non-1.0 rate parameter, this will have the same effect as using the speed
 * element (but relies on the decoder/demuxer to handle this correctly, also
 * requires a fairly up-to-date gst-plugins-base, as of February 2007).
 *
 * ## Example launch line
 * |[
 * gst-launch-1.0 filesrc location=test.ogg ! decodebin ! audioconvert ! speed speed=1.5 ! audioconvert ! audioresample ! autoaudiosink
 * ]| Plays an .ogg file at 1.5x speed.
 *
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <string.h>
#include <math.h>
#include <gst/gst.h>
#include <gst/audio/audio.h>

#include "gstspeed.h"

GST_DEBUG_CATEGORY_STATIC (speed_debug);
#define GST_CAT_DEFAULT speed_debug

enum
{
  PROP_0,
  PROP_SPEED
};

/* assumption here: sizeof (gfloat) = 4 */
#define GST_SPEED_AUDIO_CAPS \
  "audio/x-raw, " \
    "format = {" GST_AUDIO_NE (F32) ", " GST_AUDIO_NE (S16) "}, " \
    "rate = (int) [ 1, MAX ], " \
    "channels = (int) [ 1, MAX ]"

static GstStaticPadTemplate gst_speed_sink_template =
GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (GST_SPEED_AUDIO_CAPS)
    );

static GstStaticPadTemplate gst_speed_src_template =
GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (GST_SPEED_AUDIO_CAPS)
    );

static void speed_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void speed_get_property (GObject * object, guint prop_id, GValue * value,
    GParamSpec * pspec);

static gboolean speed_parse_caps (GstSpeed * filter, const GstCaps * caps);

static GstFlowReturn speed_chain (GstPad * pad, GstObject * parent,
    GstBuffer * buf);

static GstStateChangeReturn speed_change_state (GstElement * element,
    GstStateChange transition);
static gboolean speed_sink_event (GstPad * pad, GstObject * parent,
    GstEvent * event);
static gboolean speed_src_event (GstPad * pad, GstObject * parent,
    GstEvent * event);

G_DEFINE_TYPE (GstSpeed, gst_speed, GST_TYPE_ELEMENT);
GST_ELEMENT_REGISTER_DEFINE_WITH_CODE (speed, "speed", GST_RANK_NONE,
    GST_TYPE_SPEED, GST_DEBUG_CATEGORY_INIT (speed_debug, "speed", 0,
        "speed element");
    );

static gboolean
speed_setcaps (GstPad * pad, GstCaps * caps)
{
  GstSpeed *filter;
  gboolean ret;

  filter = GST_SPEED (gst_pad_get_parent (pad));

  ret = speed_parse_caps (filter, caps);

  gst_object_unref (filter);

  return ret;
}

static gboolean
speed_parse_caps (GstSpeed * filter, const GstCaps * caps)
{
  g_return_val_if_fail (filter != NULL, FALSE);
  g_return_val_if_fail (caps != NULL, FALSE);

  if (!gst_audio_info_from_caps (&filter->info, caps))
    return FALSE;


  return TRUE;
}

static gboolean
speed_src_event (GstPad * pad, GstObject * parent, GstEvent * event)
{
  GstSpeed *filter;
  gboolean ret = FALSE;

  filter = GST_SPEED (parent);

  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_SEEK:{
      gdouble rate;
      GstFormat format;
      GstSeekFlags flags;
      GstSeekType start_type, stop_type;
      gint64 start, stop;

      gst_event_parse_seek (event, &rate, &format, &flags, &start_type, &start,
          &stop_type, &stop);
      gst_event_unref (event);

      if (format != GST_FORMAT_TIME) {
        GST_DEBUG_OBJECT (filter, "only support seeks in TIME format");
        break;
      }

      if (start_type != GST_SEEK_TYPE_NONE && start != -1) {
        start *= filter->speed;
      }

      if (stop_type != GST_SEEK_TYPE_NONE && stop != -1) {
        stop *= filter->speed;
      }

      event = gst_event_new_seek (rate, format, flags, start_type, start,
          stop_type, stop);

      GST_LOG ("sending seek event: %" GST_PTR_FORMAT,
          gst_event_get_structure (event));

      ret = gst_pad_send_event (GST_PAD_PEER (filter->sinkpad), event);
      break;
    }
    default:
      ret = gst_pad_event_default (pad, parent, event);
      break;
  }

  return ret;
}

static gboolean
gst_speed_convert (GstSpeed * filter, GstFormat src_format, gint64 src_value,
    GstFormat * dest_format, gint64 * dest_value)
{
  gboolean ret = TRUE;
  guint scale = 1;

  if (src_format == *dest_format) {
    *dest_value = src_value;
    return TRUE;
  }

  switch (src_format) {
    case GST_FORMAT_BYTES:
      switch (*dest_format) {
        case GST_FORMAT_DEFAULT:
          if (GST_AUDIO_INFO_BPF (&filter->info) == 0) {
            ret = FALSE;
            break;
          }
          *dest_value = src_value / GST_AUDIO_INFO_BPF (&filter->info);
          break;
        case GST_FORMAT_TIME:
        {
          gint byterate =
              GST_AUDIO_INFO_BPF (&filter->info) *
              GST_AUDIO_INFO_RATE (&filter->info);

          if (byterate == 0) {
            ret = FALSE;
            break;
          }
          *dest_value = src_value * GST_SECOND / byterate;
          break;
        }
        default:
          ret = FALSE;
      }
      break;
    case GST_FORMAT_DEFAULT:
      switch (*dest_format) {
        case GST_FORMAT_BYTES:
          *dest_value = src_value * GST_AUDIO_INFO_BPF (&filter->info);
          break;
        case GST_FORMAT_TIME:
          if (GST_AUDIO_INFO_RATE (&filter->info) == 0) {
            ret = FALSE;
            break;
          }
          *dest_value =
              src_value * GST_SECOND / GST_AUDIO_INFO_RATE (&filter->info);
          break;
        default:
          ret = FALSE;
      }
      break;
    case GST_FORMAT_TIME:
      switch (*dest_format) {
        case GST_FORMAT_BYTES:
          scale = GST_AUDIO_INFO_BPF (&filter->info);
          /* fallthrough */
        case GST_FORMAT_DEFAULT:
          *dest_value =
              src_value * scale * GST_AUDIO_INFO_RATE (&filter->info) /
              GST_SECOND;
          break;
        default:
          ret = FALSE;
      }
      break;
    default:
      ret = FALSE;
  }

  return ret;

}

static gboolean
speed_src_query (GstPad * pad, GstObject * parent, GstQuery * query)
{
  gboolean ret = TRUE;
  GstSpeed *filter;

  filter = GST_SPEED (parent);

  switch (GST_QUERY_TYPE (query)) {
    case GST_QUERY_POSITION:
    {
      GstFormat format;
      GstFormat rformat = GST_FORMAT_TIME;
      gint64 cur;
      GstFormat conv_format = GST_FORMAT_TIME;

      /* save requested format */
      gst_query_parse_position (query, &format, NULL);

      /* query peer for current position in time */
      gst_query_set_position (query, GST_FORMAT_TIME, -1);

      if (!gst_pad_peer_query_position (filter->sinkpad, rformat, &cur)) {
        GST_LOG_OBJECT (filter, "TIME query on peer pad failed, trying BYTES");
        rformat = GST_FORMAT_BYTES;
        if (!gst_pad_peer_query_position (filter->sinkpad, rformat, &cur)) {
          GST_LOG_OBJECT (filter, "BYTES query on peer pad failed too");
          goto error;
        }
      }

      if (rformat == GST_FORMAT_BYTES)
        GST_LOG_OBJECT (filter,
            "peer pad returned current=%" G_GINT64_FORMAT " bytes", cur);
      else if (rformat == GST_FORMAT_TIME)
        GST_LOG_OBJECT (filter, "peer pad returned time=%" G_GINT64_FORMAT,
            cur);

      /* convert to time format */
      if (!gst_speed_convert (filter, rformat, cur, &conv_format, &cur)) {
        ret = FALSE;
        break;
      }

      /* adjust for speed factor */
      cur /= filter->speed;

      /* convert to time format */
      if (!gst_speed_convert (filter, conv_format, cur, &format, &cur)) {
        ret = FALSE;
        break;
      }
      gst_query_set_position (query, format, cur);

      GST_LOG_OBJECT (filter,
          "position query: we return %" G_GUINT64_FORMAT " (format %u)", cur,
          format);

      break;
    }
    case GST_QUERY_DURATION:
    {
      GstFormat format;
      GstFormat rformat = GST_FORMAT_TIME;
      gint64 end;
      GstFormat conv_format = GST_FORMAT_TIME;

      /* save requested format */
      gst_query_parse_duration (query, &format, NULL);

      /* query peer for total length in time */
      gst_query_set_duration (query, GST_FORMAT_TIME, -1);

      if (!gst_pad_peer_query_duration (filter->sinkpad, rformat, &end)) {
        GST_LOG_OBJECT (filter, "TIME query on peer pad failed, trying BYTES");
        rformat = GST_FORMAT_BYTES;
        if (!gst_pad_peer_query_duration (filter->sinkpad, rformat, &end)) {
          GST_LOG_OBJECT (filter, "BYTES query on peer pad failed too");
          goto error;
        }
      }

      if (rformat == GST_FORMAT_BYTES)
        GST_LOG_OBJECT (filter,
            "peer pad returned total=%" G_GINT64_FORMAT " bytes", end);
      else if (rformat == GST_FORMAT_TIME)
        GST_LOG_OBJECT (filter, "peer pad returned time=%" G_GINT64_FORMAT,
            end);

      /* convert to time format */
      if (!gst_speed_convert (filter, rformat, end, &conv_format, &end)) {
        ret = FALSE;
        break;
      }

      /* adjust for speed factor */
      end /= filter->speed;

      /* convert to time format */
      if (!gst_speed_convert (filter, conv_format, end, &format, &end)) {
        ret = FALSE;
        break;
      }

      gst_query_set_duration (query, format, end);

      GST_LOG_OBJECT (filter,
          "duration query: we return %" G_GUINT64_FORMAT " (format %u)", end,
          format);

      break;
    }
    default:
      ret = FALSE;
      break;
  }

  return ret;

error:

  gst_object_unref (filter);
  GST_DEBUG ("error handling query");
  return FALSE;
}

static void
gst_speed_class_init (GstSpeedClass * klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;
  GstElementClass *gstelement_class = (GstElementClass *) klass;

  gobject_class->set_property = speed_set_property;
  gobject_class->get_property = speed_get_property;
  gstelement_class->change_state = speed_change_state;

  g_object_class_install_property (G_OBJECT_CLASS (klass), PROP_SPEED,
      g_param_spec_float ("speed", "speed", "speed",
          0.1f, 40.0, 1.0,
          G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS));

  gst_element_class_set_static_metadata (gstelement_class, "Speed",
      "Filter/Effect/Audio",
      "Set speed/pitch on audio/raw streams (resampler)",
      "Andy Wingo <apwingo@eos.ncsu.edu>, "
      "Tim-Philipp Müller <tim@centricular.net>");

  gst_element_class_add_static_pad_template (gstelement_class,
      &gst_speed_src_template);
  gst_element_class_add_static_pad_template (gstelement_class,
      &gst_speed_sink_template);
}

static void
gst_speed_init (GstSpeed * filter)
{
  filter->sinkpad =
      gst_pad_new_from_static_template (&gst_speed_sink_template, "sink");
  gst_pad_set_chain_function (filter->sinkpad, speed_chain);
  gst_element_add_pad (GST_ELEMENT (filter), filter->sinkpad);
  gst_pad_set_event_function (filter->sinkpad, speed_sink_event);
  GST_PAD_SET_PROXY_CAPS (filter->sinkpad);

  filter->srcpad =
      gst_pad_new_from_static_template (&gst_speed_src_template, "src");
  gst_pad_set_query_function (filter->srcpad, speed_src_query);
  gst_element_add_pad (GST_ELEMENT (filter), filter->srcpad);
  gst_pad_set_event_function (filter->srcpad, speed_src_event);
  GST_PAD_SET_PROXY_CAPS (filter->srcpad);

  filter->offset = 0;
  filter->timestamp = 0;
}

static inline guint
speed_chain_int16 (GstSpeed * filter, GstBuffer * in_buf, GstBuffer * out_buf,
    guint c, guint in_samples)
{
  gint16 *in_data, *out_data;
  gfloat interp, lower, i_float;
  guint i, j;
  GstMapInfo in_info, out_info;

  gst_buffer_map (in_buf, &in_info, GST_MAP_READ);
  gst_buffer_map (out_buf, &out_info, GST_MAP_WRITE);

  in_data = (gint16 *) in_info.data + c;
  out_data = (gint16 *) out_info.data + c;

  lower = in_data[0];
  i_float = 0.5 * (filter->speed - 1.0);
  i = (guint) ceil (i_float);
  j = 0;

  while (i < in_samples) {
    interp = i_float - floor (i_float);

    out_data[j * GST_AUDIO_INFO_CHANNELS (&filter->info)] =
        lower * (1 - interp) +
        in_data[i * GST_AUDIO_INFO_CHANNELS (&filter->info)] * interp;

    lower = in_data[i * GST_AUDIO_INFO_CHANNELS (&filter->info)];

    i_float += filter->speed;
    i = (guint) ceil (i_float);

    ++j;
  }

  gst_buffer_unmap (in_buf, &in_info);
  gst_buffer_unmap (out_buf, &out_info);
  return j;
}

static inline guint
speed_chain_float32 (GstSpeed * filter, GstBuffer * in_buf, GstBuffer * out_buf,
    guint c, guint in_samples)
{
  gfloat *in_data, *out_data;
  gfloat interp, lower, i_float;
  guint i, j;
  GstMapInfo in_info, out_info;

  gst_buffer_map (in_buf, &in_info, GST_MAP_WRITE);
  gst_buffer_map (out_buf, &out_info, GST_MAP_WRITE);

  in_data = (gfloat *) in_info.data + c;
  out_data = (gfloat *) out_info.data + c;

  lower = in_data[0];
  i_float = 0.5 * (filter->speed - 1.0);
  i = (guint) ceil (i_float);
  j = 0;

  while (i < in_samples) {
    interp = i_float - floor (i_float);

    out_data[j * GST_AUDIO_INFO_CHANNELS (&filter->info)] =
        lower * (1 - interp) +
        in_data[i * GST_AUDIO_INFO_CHANNELS (&filter->info)] * interp;

    lower = in_data[i * GST_AUDIO_INFO_CHANNELS (&filter->info)];

    i_float += filter->speed;
    i = (guint) ceil (i_float);

    ++j;
  }
  gst_buffer_unmap (in_buf, &in_info);
  gst_buffer_unmap (out_buf, &out_info);
  return j;
}


static gboolean
speed_sink_event (GstPad * pad, GstObject * parent, GstEvent * event)
{
  GstSpeed *filter = GST_SPEED (parent);
  gboolean ret = FALSE;

  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_SEGMENT:{
      gdouble rate;
      GstFormat format;
      gint64 start_value, stop_value;
      const GstSegment *segment;
      GstSegment seg;

      gst_event_parse_segment (event, &segment);

      rate = segment->rate;
      format = segment->format;
      start_value = segment->start;
      stop_value = segment->stop;

      gst_event_unref (event);

      if (format != GST_FORMAT_TIME) {
        GST_WARNING_OBJECT (filter, "newsegment event not in TIME format!");
        break;
      }

      g_assert (filter->speed > 0);

      if (start_value >= 0)
        start_value /= filter->speed;
      if (stop_value >= 0)
        stop_value /= filter->speed;

      /* this would only really be correct if we clipped incoming data */
      filter->timestamp = start_value;

      /* set to NONE so it gets reset later based on the timestamp when we have
       * the samplerate */
      filter->offset = GST_BUFFER_OFFSET_NONE;

      gst_segment_init (&seg, GST_FORMAT_TIME);
      seg.rate = rate;
      seg.start = start_value;
      seg.stop = stop_value;
      seg.time = segment->time;
      ret = gst_pad_push_event (filter->srcpad, gst_event_new_segment (&seg));

      break;
    }
    case GST_EVENT_CAPS:
    {
      GstCaps *caps;

      gst_event_parse_caps (event, &caps);
      ret = speed_setcaps (pad, caps);
      if (!ret) {
        gst_event_unref (event);
        return ret;
      }
    }
      /* Fallthrough so that the caps event gets forwarded */
    default:
      ret = gst_pad_event_default (pad, parent, event);
      break;
  }
  return ret;
}

static GstFlowReturn
speed_chain (GstPad * pad, GstObject * parent, GstBuffer * in_buf)
{
  GstBuffer *out_buf;
  GstSpeed *filter = GST_SPEED (parent);
  guint c, in_samples, out_samples, out_size;
  GstFlowReturn flow;
  gsize size;

  if (G_UNLIKELY (filter->offset == GST_BUFFER_OFFSET_NONE)) {
    filter->offset = gst_util_uint64_scale_int (filter->timestamp,
        GST_AUDIO_INFO_RATE (&filter->info), GST_SECOND);
  }

  /* buffersize has to be aligned to a frame */
  out_size = ceil ((gfloat) gst_buffer_get_size (in_buf) / filter->speed);
  out_size = ((out_size + GST_AUDIO_INFO_BPF (&filter->info) - 1) /
      GST_AUDIO_INFO_BPF (&filter->info)) * GST_AUDIO_INFO_BPF (&filter->info);

  out_buf = gst_buffer_new_and_alloc (out_size);

  in_samples = gst_buffer_get_size (in_buf) /
      GST_AUDIO_INFO_BPF (&filter->info);

  out_samples = 0;

  for (c = 0; c < GST_AUDIO_INFO_CHANNELS (&filter->info); ++c) {
    if (GST_AUDIO_INFO_IS_INTEGER (&filter->info))
      out_samples = speed_chain_int16 (filter, in_buf, out_buf, c, in_samples);
    else
      out_samples =
          speed_chain_float32 (filter, in_buf, out_buf, c, in_samples);
  }

  size = out_samples * GST_AUDIO_INFO_BPF (&filter->info);
  gst_buffer_set_size (out_buf, size);

  GST_BUFFER_OFFSET (out_buf) = filter->offset;
  GST_BUFFER_TIMESTAMP (out_buf) = filter->timestamp;

  filter->offset += size / GST_AUDIO_INFO_BPF (&filter->info);
  filter->timestamp = gst_util_uint64_scale_int (filter->offset, GST_SECOND,
      GST_AUDIO_INFO_RATE (&filter->info));

  /* make sure it's at least nominally a perfect stream */
  GST_BUFFER_DURATION (out_buf) =
      filter->timestamp - GST_BUFFER_TIMESTAMP (out_buf);
  flow = gst_pad_push (filter->srcpad, out_buf);

  if (G_UNLIKELY (flow != GST_FLOW_OK))
    GST_DEBUG_OBJECT (filter, "flow: %s", gst_flow_get_name (flow));

  gst_buffer_unref (in_buf);
  return flow;
}

static void
speed_set_property (GObject * object, guint prop_id, const GValue * value,
    GParamSpec * pspec)
{
  GstSpeed *filter = GST_SPEED (object);

  switch (prop_id) {
    case PROP_SPEED:
      filter->speed = g_value_get_float (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }

}

static void
speed_get_property (GObject * object, guint prop_id, GValue * value,
    GParamSpec * pspec)
{
  GstSpeed *filter = GST_SPEED (object);

  switch (prop_id) {
    case PROP_SPEED:
      g_value_set_float (value, filter->speed);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }

}

static GstStateChangeReturn
speed_change_state (GstElement * element, GstStateChange transition)
{
  GstSpeed *speed = GST_SPEED (element);

  switch (transition) {
    case GST_STATE_CHANGE_READY_TO_PAUSED:
      speed->offset = GST_BUFFER_OFFSET_NONE;
      speed->timestamp = 0;
      gst_audio_info_init (&speed->info);
      break;
    default:
      break;
  }

  return GST_ELEMENT_CLASS (gst_speed_parent_class)->change_state (element,
      transition);
}

static gboolean
plugin_init (GstPlugin * plugin)
{
  return GST_ELEMENT_REGISTER (speed, plugin);
}

GST_PLUGIN_DEFINE (GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    speed,
    "Set speed/pitch on audio/raw streams (resampler)",
    plugin_init, VERSION, GST_LICENSE, GST_PACKAGE_NAME, GST_PACKAGE_ORIGIN)
