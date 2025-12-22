/* GStreamer
 * Copyright (C) <2008> Thijs Vermeir <thijsvermeir@gmail.com>
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
#include "config.h"
#endif

#include <string.h>

#include "gstsrtenc.h"

GST_DEBUG_CATEGORY_STATIC (srtenc_debug);
#define GST_CAT_DEFAULT srtenc_debug

enum
{
  PROP_0,
  PROP_TIMESTAMP,
  PROP_DURATION
};

static GstStaticPadTemplate src_template = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("application/x-subtitle"));

static GstStaticPadTemplate sink_template = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("text/x-raw, format = { pango-markup, utf8 }"));

static GstFlowReturn gst_srt_enc_chain (GstPad * pad, GstObject * parent,
    GstBuffer * buf);
static gboolean gst_srt_enc_event (GstPad * pad, GstObject * parent,
    GstEvent * event);
static void gst_srt_enc_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);
static void gst_srt_enc_reset (GstSrtEnc * srtenc);
static void gst_srt_enc_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);

#define parent_class gst_srt_enc_parent_class
G_DEFINE_TYPE (GstSrtEnc, gst_srt_enc, GST_TYPE_ELEMENT);
GST_ELEMENT_REGISTER_DEFINE (srtenc, "srtenc", GST_RANK_NONE, GST_TYPE_SRT_ENC);

static void
gst_srt_enc_append_timestamp_to_string (GstClockTime timestamp, GString * str)
{
  guint h, m, s, ms;

  h = timestamp / (3600 * GST_SECOND);

  timestamp -= h * 3600 * GST_SECOND;
  m = timestamp / (60 * GST_SECOND);

  timestamp -= m * 60 * GST_SECOND;
  s = timestamp / GST_SECOND;

  timestamp -= s * GST_SECOND;
  ms = timestamp / GST_MSECOND;

  g_string_append_printf (str, "%.2d:%.2d:%.2d,%.3d", h, m, s, ms);
}

static GstFlowReturn
gst_srt_enc_chain (GstPad * pad, GstObject * parent, GstBuffer * buf)
{
  GstSrtEnc *srtenc = GST_SRT_ENC (parent);
  GstClockTime ts, dur = GST_SECOND;
  GstBuffer *new_buffer;
  GstMapInfo map_info;
  GString *s;
  gsize buf_size;

  gst_object_sync_values (GST_OBJECT (srtenc), GST_BUFFER_PTS (buf));

  ts = GST_BUFFER_PTS (buf) + srtenc->timestamp;
  if (GST_BUFFER_DURATION_IS_VALID (buf))
    dur = GST_BUFFER_DURATION (buf) + srtenc->duration;
  else if (srtenc->duration > 0)
    dur = srtenc->duration;
  else
    dur = GST_SECOND;

  buf_size = gst_buffer_get_size (buf);
  s = g_string_sized_new (10 + 50 + buf_size + 2 + 1);

  /* stanza count */
  g_string_append_printf (s, "%d\n", srtenc->counter++);

  /* start_time --> end_time */
  gst_srt_enc_append_timestamp_to_string (ts, s);
  g_string_append_printf (s, " --> ");
  gst_srt_enc_append_timestamp_to_string (ts + dur, s);
  g_string_append_c (s, '\n');

  /* text */
  if (gst_buffer_map (buf, &map_info, GST_MAP_READ)) {
    g_string_append_len (s, (const gchar *) map_info.data, map_info.size);
    gst_buffer_unmap (buf, &map_info);
  }

  g_string_append (s, "\n\n");

  buf_size = s->len;
  new_buffer = gst_buffer_new_wrapped (g_string_free (s, FALSE), buf_size);

  GST_BUFFER_TIMESTAMP (new_buffer) = GST_BUFFER_TIMESTAMP (buf);
  GST_BUFFER_DURATION (new_buffer) = GST_BUFFER_DURATION (buf);

  gst_buffer_unref (buf);

  return gst_pad_push (srtenc->srcpad, new_buffer);
}

static gboolean
gst_srt_enc_event (GstPad * pad, GstObject * parent, GstEvent * event)
{
  GstSrtEnc *srtenc = GST_SRT_ENC (parent);
  gboolean ret;

  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_CAPS:
    {
      GstCaps *caps;

      caps = gst_static_pad_template_get_caps (&src_template);
      gst_pad_set_caps (srtenc->srcpad, caps);
      gst_caps_unref (caps);
      gst_event_unref (event);
      ret = TRUE;
      break;
    }
    default:
      ret = gst_pad_event_default (pad, parent, event);
      break;
  }

  return ret;
}

static void
gst_srt_enc_reset (GstSrtEnc * srtenc)
{
  srtenc->counter = 1;
}

static GstStateChangeReturn
gst_srt_enc_change_state (GstElement * element, GstStateChange transition)
{
  GstStateChangeReturn ret;
  GstSrtEnc *srtenc = GST_SRT_ENC (element);

  ret = GST_ELEMENT_CLASS (parent_class)->change_state (element, transition);
  if (ret == GST_STATE_CHANGE_FAILURE)
    return ret;

  switch (transition) {
    case GST_STATE_CHANGE_PAUSED_TO_READY:
      gst_srt_enc_reset (srtenc);
      break;
    default:
      break;
  }

  return ret;
}

static void
gst_srt_enc_get_property (GObject * object,
    guint prop_id, GValue * value, GParamSpec * pspec)
{
  GstSrtEnc *srtenc;

  srtenc = GST_SRT_ENC (object);

  switch (prop_id) {
    case PROP_TIMESTAMP:
      g_value_set_int64 (value, srtenc->timestamp);
      break;
    case PROP_DURATION:
      g_value_set_int64 (value, srtenc->duration);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_srt_enc_set_property (GObject * object,
    guint prop_id, const GValue * value, GParamSpec * pspec)
{

  GstSrtEnc *srtenc;

  srtenc = GST_SRT_ENC (object);

  switch (prop_id) {
    case PROP_TIMESTAMP:
      srtenc->timestamp = g_value_get_int64 (value);
      break;
    case PROP_DURATION:
      srtenc->duration = g_value_get_int64 (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_srt_enc_class_init (GstSrtEncClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GstElementClass *element_class = GST_ELEMENT_CLASS (klass);

  gobject_class->set_property = GST_DEBUG_FUNCPTR (gst_srt_enc_set_property);
  gobject_class->get_property = GST_DEBUG_FUNCPTR (gst_srt_enc_get_property);

  element_class->change_state = GST_DEBUG_FUNCPTR (gst_srt_enc_change_state);

  g_object_class_install_property (gobject_class, PROP_TIMESTAMP,
      g_param_spec_int64 ("timestamp", "Offset for the starttime",
          "Offset for the starttime for the subtitles", G_MININT64, G_MAXINT64,
          0,
          G_PARAM_READWRITE | GST_PARAM_CONTROLLABLE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_DURATION,
      g_param_spec_int64 ("duration", "Offset for the duration",
          "Offset for the duration of the subtitles", G_MININT64, G_MAXINT64,
          0,
          G_PARAM_READWRITE | GST_PARAM_CONTROLLABLE | G_PARAM_STATIC_STRINGS));

  gst_element_class_add_static_pad_template (element_class, &sink_template);
  gst_element_class_add_static_pad_template (element_class, &src_template);

  gst_element_class_set_static_metadata (element_class,
      "Srt encoder", "Codec/Encoder/Subtitle",
      "Srt subtitle encoder", "Thijs Vermeir <thijsvermeir@gmail.com>");

  GST_DEBUG_CATEGORY_INIT (srtenc_debug, "srtenc", 0,
      "SubRip subtitle encoder");
}

static void
gst_srt_enc_init (GstSrtEnc * srtenc)
{
  gst_srt_enc_reset (srtenc);

  srtenc->srcpad = gst_pad_new_from_static_template (&src_template, "src");
  gst_element_add_pad (GST_ELEMENT (srtenc), srtenc->srcpad);

  srtenc->sinkpad = gst_pad_new_from_static_template (&sink_template, "sink");
  gst_pad_set_chain_function (srtenc->sinkpad,
      GST_DEBUG_FUNCPTR (gst_srt_enc_chain));
  gst_pad_set_event_function (srtenc->sinkpad,
      GST_DEBUG_FUNCPTR (gst_srt_enc_event));
  gst_element_add_pad (GST_ELEMENT (srtenc), srtenc->sinkpad);
}
