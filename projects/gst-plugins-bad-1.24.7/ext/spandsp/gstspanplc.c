/*
 * (C) 2011 Collabora Ltd.
 *  Contact: Youness Alaoui <youness.alaoui@collabora.co.uk>
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
 * SECTION:element-spanplc
 * @title: spanplc
 *
 * The spanplc (Packet Loss Concealment) element provides a synthetic
 * fill-in signal, to minimise the audible effect of lost packets in
 * VoIP applications
 *
 */


#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "gstspanplc.h"

#include <gst/audio/audio.h>

G_DEFINE_TYPE (GstSpanPlc, gst_span_plc, GST_TYPE_ELEMENT);
GST_ELEMENT_REGISTER_DEFINE (spanplc, "spanplc", GST_RANK_PRIMARY,
    GST_TYPE_SPAN_PLC);

GST_DEBUG_CATEGORY_STATIC (gst_span_plc_debug);
#define GST_CAT_DEFAULT gst_span_plc_debug

enum
{
  PROP_0,
  PROP_STATS,
};

static GstStaticPadTemplate src_factory = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("audio/x-raw, format=\"" GST_AUDIO_NE (S16) "\", "
        "rate = " GST_AUDIO_RATE_RANGE " , channels = (int) 1")
    );

static GstStaticPadTemplate sink_factory = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("audio/x-raw, format=\"" GST_AUDIO_NE (S16) "\", "
        "rate = " GST_AUDIO_RATE_RANGE " , channels = (int) 1")
    );

static void gst_span_plc_dispose (GObject * object);

static GstStateChangeReturn gst_span_plc_change_state (GstElement * element,
    GstStateChange transition);
static void gst_span_plc_setcaps_sink (GstSpanPlc * plc, GstCaps * caps);
static GstFlowReturn gst_span_plc_chain (GstPad * pad, GstObject * parent,
    GstBuffer * buf);
static gboolean gst_span_plc_event_sink (GstPad * pad, GstObject * parent,
    GstEvent * event);

static GstStructure *
gst_span_plc_create_stats (GstSpanPlc * self)
{
  GstStructure *s = NULL;

  GST_OBJECT_LOCK (self);
  if (self->plc_state) {
    s = gst_structure_new ("application/x-spanplc-stats",
        "num-pushed", G_TYPE_UINT64, self->num_pushed,
        "num-gap", G_TYPE_UINT64, self->num_gap,
        "plc-num-samples", G_TYPE_UINT64, self->plc_num_samples,
        "plc-duration", G_TYPE_UINT64, self->plc_duration,
        "pitch", G_TYPE_INT, self->plc_state->pitch,
        "pitch-offset", G_TYPE_INT, self->plc_state->pitch_offset, NULL);
  }
  GST_OBJECT_UNLOCK (self);

  return s;
}

static void
gst_span_plc_get_property (GObject * object, guint prop_id, GValue * value,
    GParamSpec * pspec)
{
  GstSpanPlc *self = GST_SPAN_PLC (object);

  switch (prop_id) {
    case PROP_STATS:
      g_value_take_boxed (value, gst_span_plc_create_stats (self));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

/* initialize the plugin's class */
static void
gst_span_plc_class_init (GstSpanPlcClass * klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;
  GstElementClass *gstelement_class = (GstElementClass *) klass;

  gst_element_class_add_static_pad_template (gstelement_class, &src_factory);
  gst_element_class_add_static_pad_template (gstelement_class, &sink_factory);

  gst_element_class_set_static_metadata (gstelement_class, "SpanDSP PLC",
      "Filter/Effect/Audio",
      "Adds packet loss concealment to audio",
      "Youness Alaoui <youness.alaoui@collabora.co.uk>");

  gobject_class->get_property = gst_span_plc_get_property;
  gobject_class->dispose = gst_span_plc_dispose;

  gstelement_class->change_state =
      GST_DEBUG_FUNCPTR (gst_span_plc_change_state);

  /**
   * GstSpanPlc:stats:
   *
   * Various decoder statistics. This property returns a GstStructure
   * with name application/x-spanplc-stats with the following fields:
   *
   * * #guint64 `num-pushed`: the number of packets pushed out.
   * * #guint64 `num-gap`: the number of gap packets received.
   * * #guint64 `plc-num-samples`: the number of samples generated using PLC
   * * #guint64 `plc-duration`: the total duration, in ns, of samples generated using PLC
   * * #guint `pitch`: pitch estimate, in Hz
   * * #guint `pitch-offset`: current offset in pitch period, in Hz
   *
   * Since: 1.18
   */
  g_object_class_install_property (gobject_class, PROP_STATS,
      g_param_spec_boxed ("stats", "Statistics",
          "Various statistics", GST_TYPE_STRUCTURE,
          G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  GST_DEBUG_CATEGORY_INIT (gst_span_plc_debug, "spanplc",
      0, "spanDSP's packet loss concealment");

}

static void
gst_span_plc_init (GstSpanPlc * plc)
{
  GST_DEBUG_OBJECT (plc, "init");

  plc->srcpad = gst_pad_new_from_static_template (&src_factory, "src");
  plc->sinkpad = gst_pad_new_from_static_template (&sink_factory, "sink");

  gst_pad_set_chain_function (plc->sinkpad,
      GST_DEBUG_FUNCPTR (gst_span_plc_chain));
  gst_pad_set_event_function (plc->sinkpad,
      GST_DEBUG_FUNCPTR (gst_span_plc_event_sink));

  gst_element_add_pad (GST_ELEMENT (plc), plc->srcpad);
  gst_element_add_pad (GST_ELEMENT (plc), plc->sinkpad);

  plc->plc_state = NULL;

  GST_DEBUG_OBJECT (plc, "init complete");
}

static void
gst_span_plc_dispose (GObject * object)
{
  GstSpanPlc *plc = GST_SPAN_PLC (object);

  if (plc->plc_state)
    plc_free (plc->plc_state);
  plc->plc_state = NULL;

  G_OBJECT_CLASS (gst_span_plc_parent_class)->dispose (object);
}

static void
gst_span_plc_flush (GstSpanPlc * plc, gboolean renew)
{
  if (plc->plc_state)
    plc_free (plc->plc_state);
  if (renew)
    plc->plc_state = plc_init (NULL);
  else
    plc->plc_state = NULL;
}

static GstStateChangeReturn
gst_span_plc_change_state (GstElement * element, GstStateChange transition)
{
  GstSpanPlc *plc = GST_SPAN_PLC (element);
  GstStateChangeReturn ret;

  switch (transition) {
    case GST_STATE_CHANGE_NULL_TO_READY:
      gst_span_plc_flush (plc, TRUE);
      break;
    default:
      break;
  }

  ret = GST_ELEMENT_CLASS (gst_span_plc_parent_class)->change_state (element,
      transition);

  switch (transition) {
    case GST_STATE_CHANGE_PAUSED_TO_READY:
      GST_OBJECT_LOCK (plc);
      plc->num_pushed = 0;
      plc->num_gap = 0;
      plc->plc_num_samples = 0;
      plc->plc_duration = 0;
      GST_OBJECT_UNLOCK (plc);
      break;
    case GST_STATE_CHANGE_READY_TO_NULL:
      gst_span_plc_flush (plc, FALSE);
    default:
      break;
  }

  return ret;
}

static void
gst_span_plc_setcaps_sink (GstSpanPlc * plc, GstCaps * caps)
{
  GstStructure *s = NULL;
  gint sample_rate;

  s = gst_caps_get_structure (caps, 0);
  if (!s)
    return;

  gst_structure_get_int (s, "rate", &sample_rate);
  if (sample_rate != plc->sample_rate) {
    GST_DEBUG_OBJECT (plc, "setcaps: got sample rate : %d", sample_rate);
    plc->sample_rate = sample_rate;
    gst_span_plc_flush (plc, TRUE);
  }
}

static GstFlowReturn
gst_span_plc_chain (GstPad * pad, GstObject * parent, GstBuffer * buffer)
{
  GstSpanPlc *plc = GST_SPAN_PLC (parent);
  GstMapInfo map;

  buffer = gst_buffer_make_writable (buffer);
  gst_buffer_map (buffer, &map, GST_MAP_READWRITE);
  plc_rx (plc->plc_state, (int16_t *) map.data, map.size / 2);
  gst_buffer_unmap (buffer, &map);

  GST_OBJECT_LOCK (plc);
  plc->num_pushed++;
  GST_OBJECT_UNLOCK (plc);

  return gst_pad_push (plc->srcpad, buffer);
}

static void
gst_span_plc_send_fillin (GstSpanPlc * plc, GstClockTime timestamp,
    GstClockTime duration)
{
  guint buf_size;
  GstBuffer *buffer = NULL;
  GstMapInfo map;
  gint num_samples;

  buf_size = ((float) duration / GST_SECOND) * plc->sample_rate;
  buf_size *= sizeof (guint16);
  buffer = gst_buffer_new_and_alloc (buf_size);
  GST_DEBUG_OBJECT (plc, "Missing packet of %" GST_TIME_FORMAT
      " == %d bytes", GST_TIME_ARGS (duration), buf_size);
  gst_buffer_map (buffer, &map, GST_MAP_READWRITE);
  num_samples = plc_fillin (plc->plc_state, (int16_t *) map.data, map.size / 2);
  gst_buffer_unmap (buffer, &map);
  GST_BUFFER_PTS (buffer) = timestamp;
  GST_BUFFER_DURATION (buffer) = duration;

  GST_OBJECT_LOCK (plc);
  plc->num_gap++;
  plc->num_pushed++;
  plc->plc_num_samples += num_samples;
  plc->plc_duration += duration;
  GST_OBJECT_UNLOCK (plc);

  gst_pad_push (plc->srcpad, buffer);
}

static gboolean
gst_span_plc_event_sink (GstPad * pad, GstObject * parent, GstEvent * event)
{
  GstSpanPlc *plc = GST_SPAN_PLC (parent);

  GST_DEBUG_OBJECT (plc, "received event %s", GST_EVENT_TYPE_NAME (event));

  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_CAPS:
    {
      GstCaps *caps;
      gst_event_parse_caps (event, &caps);
      gst_span_plc_setcaps_sink (plc, caps);
      break;
    }
    case GST_EVENT_GAP:
    {
      GstClockTime timestamp;
      GstClockTime duration;

      gst_event_parse_gap (event, &timestamp, &duration);
      gst_span_plc_send_fillin (plc, timestamp, duration);
      gst_event_unref (event);
      return TRUE;
    }
    case GST_EVENT_FLUSH_STOP:
      gst_span_plc_flush (plc, TRUE);
      break;
    default:
      break;
  }

  return gst_pad_push_event (plc->srcpad, event);
}
