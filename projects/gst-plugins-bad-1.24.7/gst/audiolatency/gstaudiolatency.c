/* Audio latency measurement plugin
 * Copyright (C) 2018 Centricular Ltd.
 *   Author: Nirbheek Chauhan <nirbheek@centricular.com>
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
 * SECTION:element-audiolatency
 * @title: audiolatency
 *
 * Measures the audio latency between the source pad and the sink pad by
 * outputting period ticks on the source pad and measuring how long they take to
 * arrive on the sink pad.
 *
 * The ticks have a period of 1 second, so this element can only measure
 * latencies smaller than that.
 *
 * ## Example pipeline
 * |[
 * gst-launch-1.0 -v autoaudiosrc ! audiolatency print-latency=true ! autoaudiosink
 * ]| Continuously print the latency of the audio output and the audio capture
 *
 * In this case, you must ensure that the audio output is captured by the audio
 * source. The simplest way is to use a standard 3.5mm male-to-male audio cable
 * to connect line-out to line-in, or speaker-out to mic-in, etc.
 *
 * Capturing speaker output with a microphone should also work, as long as the
 * ambient noise level is low enough. You may have to adjust the microphone gain
 * to ensure that the volume is loud enough to be detected by the element, and
 * at the same time that it's not so loud that it picks up ambient noise.
 *
 * For programmatic use, instead of using 'print-stats', you should read the
 * 'last-latency' and 'average-latency' properties at most once a second, or
 * parse the "latency" element message, which contains the "last-latency" and
 * "average-latency" fields in the GstStructure.
 *
 * The average latency is a running average of the last 5 measurements.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gstaudiolatency.h"

#define AUDIOLATENCY_CAPS "audio/x-raw, " \
    "format = (string) F32LE, " \
    "layout = (string) interleaved, " \
    "rate = (int) [ 1, MAX ], " \
    "channels = (int) [ 1, MAX ]"

GST_DEBUG_CATEGORY_STATIC (gst_audiolatency_debug);
#define GST_CAT_DEFAULT gst_audiolatency_debug

static GstStaticPadTemplate src_template = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (AUDIOLATENCY_CAPS)
    );

static GstStaticPadTemplate sink_template = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (AUDIOLATENCY_CAPS)
    );

#define gst_audiolatency_parent_class parent_class
G_DEFINE_TYPE_WITH_CODE (GstAudioLatency, gst_audiolatency, GST_TYPE_BIN,
    GST_DEBUG_CATEGORY_INIT (gst_audiolatency_debug, "audiolatency", 0,
        "audiolatency"););
GST_ELEMENT_REGISTER_DEFINE (audiolatency, "audiolatency", GST_RANK_PRIMARY,
    GST_TYPE_AUDIOLATENCY);

#define DEFAULT_PRINT_LATENCY   FALSE
#define DEFAULT_SAMPLES_PER_BUFFER 240

enum
{
  PROP_0,
  PROP_PRINT_LATENCY,
  PROP_LAST_LATENCY,
  PROP_AVERAGE_LATENCY,
  PROP_SAMPLES_PER_BUFFER,
};

static gint64 gst_audiolatency_get_latency (GstAudioLatency * self);
static gint64 gst_audiolatency_get_average_latency (GstAudioLatency * self);
static GstFlowReturn gst_audiolatency_sink_chain (GstPad * pad,
    GstObject * parent, GstBuffer * buffer);
static gboolean gst_audiolatency_sink_event (GstPad * pad,
    GstObject * parent, GstEvent * event);
static GstPadProbeReturn gst_audiolatency_src_probe (GstPad * pad,
    GstPadProbeInfo * info, gpointer user_data);

static void
gst_audiolatency_get_property (GObject * object,
    guint prop_id, GValue * value, GParamSpec * pspec)
{
  GstAudioLatency *self = GST_AUDIOLATENCY (object);

  switch (prop_id) {
    case PROP_PRINT_LATENCY:
      g_value_set_boolean (value, self->print_latency);
      break;
    case PROP_LAST_LATENCY:
      g_value_set_int64 (value, gst_audiolatency_get_latency (self));
      break;
    case PROP_AVERAGE_LATENCY:
      g_value_set_int64 (value, gst_audiolatency_get_average_latency (self));
      break;
    case PROP_SAMPLES_PER_BUFFER:
      g_value_set_int (value, self->samples_per_buffer);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_audiolatency_set_property (GObject * object,
    guint prop_id, const GValue * value, GParamSpec * pspec)
{
  GstAudioLatency *self = GST_AUDIOLATENCY (object);

  switch (prop_id) {
    case PROP_PRINT_LATENCY:
      self->print_latency = g_value_get_boolean (value);
      break;
    case PROP_SAMPLES_PER_BUFFER:
      self->samples_per_buffer = g_value_get_int (value);
      g_object_set (self->audiosrc,
          "samplesperbuffer", self->samples_per_buffer, NULL);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_audiolatency_class_init (GstAudioLatencyClass * klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;
  GstElementClass *gstelement_class = (GstElementClass *) klass;

  gobject_class->get_property = gst_audiolatency_get_property;
  gobject_class->set_property = gst_audiolatency_set_property;

  g_object_class_install_property (gobject_class, PROP_PRINT_LATENCY,
      g_param_spec_boolean ("print-latency", "Print latencies",
          "Print the measured latencies on stdout",
          DEFAULT_PRINT_LATENCY, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_LAST_LATENCY,
      g_param_spec_int64 ("last-latency", "Last measured latency",
          "The last latency that was measured, in microseconds", 0,
          G_USEC_PER_SEC, 0, G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_AVERAGE_LATENCY,
      g_param_spec_int64 ("average-latency", "Running average latency",
          "The running average latency, in microseconds", 0,
          G_USEC_PER_SEC, 0, G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  /**
   * GstAudioLatency:samplesperbuffer:
   *
   * The number of audio samples in each outgoing buffer.
   * See also #GstAudioTestSrc:samplesperbuffer
   *
   * Since: 1.20
   */
  g_object_class_install_property (gobject_class, PROP_SAMPLES_PER_BUFFER,
      g_param_spec_int ("samplesperbuffer", "Samples per buffer",
          "Number of samples in each outgoing buffer",
          1, G_MAXINT, DEFAULT_SAMPLES_PER_BUFFER,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  gst_element_class_add_static_pad_template (gstelement_class, &src_template);
  gst_element_class_add_static_pad_template (gstelement_class, &sink_template);

  gst_element_class_set_static_metadata (gstelement_class, "AudioLatency",
      "Audio/Util",
      "Measures the audio latency between the source and the sink",
      "Nirbheek Chauhan <nirbheek@centricular.com>");
}

static void
gst_audiolatency_init (GstAudioLatency * self)
{
  GstPad *srcpad;
  GstPadTemplate *templ;

  self->send_pts = 0;
  self->recv_pts = 0;
  self->print_latency = DEFAULT_PRINT_LATENCY;
  self->samples_per_buffer = DEFAULT_SAMPLES_PER_BUFFER;

  /* Setup sinkpad */
  self->sinkpad = gst_pad_new_from_static_template (&sink_template, "sink");
  gst_pad_set_chain_function (self->sinkpad,
      GST_DEBUG_FUNCPTR (gst_audiolatency_sink_chain));
  gst_pad_set_event_function (self->sinkpad,
      GST_DEBUG_FUNCPTR (gst_audiolatency_sink_event));

  gst_element_add_pad (GST_ELEMENT (self), self->sinkpad);

  /* Setup srcpad */
  self->audiosrc = gst_element_factory_make ("audiotestsrc", NULL);
  g_object_set (self->audiosrc, "wave", 8, "samplesperbuffer",
      DEFAULT_SAMPLES_PER_BUFFER, "is-live", TRUE, NULL);
  gst_bin_add (GST_BIN (self), self->audiosrc);

  templ = gst_static_pad_template_get (&src_template);
  srcpad = gst_element_get_static_pad (self->audiosrc, "src");
  gst_pad_add_probe (srcpad,
      GST_PAD_PROBE_TYPE_BUFFER | GST_PAD_PROBE_TYPE_QUERY_UPSTREAM |
      GST_PAD_PROBE_TYPE_EVENT_UPSTREAM,
      (GstPadProbeCallback) gst_audiolatency_src_probe, self, NULL);

  self->srcpad = gst_ghost_pad_new_from_template ("src", srcpad, templ);
  gst_element_add_pad (GST_ELEMENT (self), self->srcpad);
  gst_object_unref (srcpad);
  gst_object_unref (templ);

  GST_LOG_OBJECT (self, "Initialized audiolatency");
}

static gint64
gst_audiolatency_get_latency (GstAudioLatency * self)
{
  gint64 last_latency;
  gint last_latency_idx;

  GST_OBJECT_LOCK (self);
  /* Decrement index, with wrap-around */
  last_latency_idx = self->next_latency_idx - 1;
  if (last_latency_idx < 0)
    last_latency_idx = GST_AUDIOLATENCY_NUM_LATENCIES - 1;

  last_latency = self->latencies[last_latency_idx];
  GST_OBJECT_UNLOCK (self);

  return last_latency;
}

static gint64
gst_audiolatency_get_average_latency_unlocked (GstAudioLatency * self)
{
  int ii, n = 0;
  gint64 average = 0;

  for (ii = 0; ii < GST_AUDIOLATENCY_NUM_LATENCIES; ii++) {
    if (G_LIKELY (self->latencies[ii] > 0))
      n += 1;
    average += self->latencies[ii];
  }

  return average / MAX (n, 1);
}

static gint64
gst_audiolatency_get_average_latency (GstAudioLatency * self)
{
  gint64 average;

  GST_OBJECT_LOCK (self);
  average = gst_audiolatency_get_average_latency_unlocked (self);
  GST_OBJECT_UNLOCK (self);

  return average;
}

static void
gst_audiolatency_set_latency (GstAudioLatency * self, gint64 latency)
{
  gint64 avg_latency;

  GST_OBJECT_LOCK (self);
  self->latencies[self->next_latency_idx] = latency;

  /* Increment index, with wrap-around */
  self->next_latency_idx += 1;
  if (self->next_latency_idx > GST_AUDIOLATENCY_NUM_LATENCIES - 1)
    self->next_latency_idx = 0;

  avg_latency = gst_audiolatency_get_average_latency_unlocked (self);

  if (self->print_latency)
    g_print ("last latency: %" G_GINT64_FORMAT "ms, running average: %"
        G_GINT64_FORMAT "ms\n", latency / 1000, avg_latency / 1000);
  GST_OBJECT_UNLOCK (self);

  /* Post an element message about it */
  gst_element_post_message (GST_ELEMENT (self),
      gst_message_new_element (GST_OBJECT (self),
          gst_structure_new ("latency", "last-latency", G_TYPE_INT64, latency,
              "average-latency", G_TYPE_INT64, avg_latency, NULL)));
}

static gint64
buffer_has_wave (GstBuffer * buffer, GstPad * pad)
{
  const GstStructure *s;
  GstCaps *caps;
  GstMapInfo minfo;
  guint64 duration;
  gint64 offset;
  gint ii, channels, fsize, rate;
  gfloat *fdata;
  gboolean ret;
  GstMemory *memory = NULL;

  switch (gst_buffer_n_memory (buffer)) {
    case 0:
      GST_WARNING_OBJECT (pad, "buffer %" GST_PTR_FORMAT "has no memory?",
          buffer);
      return -1;
    case 1:
      memory = gst_buffer_peek_memory (buffer, 0);
      ret = gst_memory_map (memory, &minfo, GST_MAP_READ);
      break;
    default:
      ret = gst_buffer_map (buffer, &minfo, GST_MAP_READ);
  }

  if (!ret) {
    GST_WARNING_OBJECT (pad, "failed to map buffer %" GST_PTR_FORMAT, buffer);
    return -1;
  }

  caps = gst_pad_get_current_caps (pad);
  s = gst_caps_get_structure (caps, 0);
  /* channels and rate are required in caps, so will always be present */
  gst_structure_get_int (s, "channels", &channels);
  gst_structure_get_int (s, "rate", &rate);
  gst_caps_unref (caps);

  fdata = (gfloat *) minfo.data;
  fsize = minfo.size / sizeof (gfloat);

  offset = -1;
  if (GST_BUFFER_DURATION_IS_VALID (buffer)) {
    duration = GST_BUFFER_DURATION (buffer);
  } else {
    /* Cannot do a rounding-accurate duration calculation here because in the
     * case when the duration is invalid, the pts might also be invalid */
    duration = gst_util_uint64_scale_int_round (GST_SECOND, fsize / channels,
        rate);
    GST_LOG_OBJECT (pad, "buffer duration is invalid, calculated likely "
        "duration as %" G_GINT64_FORMAT "us", duration / GST_USECOND);
  }

  /* Read only one channel */
  for (ii = 1; ii < fsize; ii += channels) {
    if (ABS (fdata[ii]) > 0.7) {
      /* The waveform probably starts somewhere inside the buffer,
       * so get the offset in nanoseconds from the buffer pts */
      offset = gst_util_uint64_scale_int_round (duration, ii, fsize);
      break;
    }
  }

  if (memory)
    gst_memory_unmap (memory, &minfo);
  else
    gst_buffer_unmap (buffer, &minfo);

  /* Return offset in microseconds */
  return (offset > 0) ? offset / 1000 : -1;
}

static GstPadProbeReturn
gst_audiolatency_src_probe_buffer (GstPad * pad, GstPadProbeInfo * info,
    gpointer user_data)
{
  GstAudioLatency *self = user_data;
  GstBuffer *buffer;
  gint64 pts, offset;

  if (!(info->type & GST_PAD_PROBE_TYPE_BUFFER))
    goto out;

  if (GST_STATE (self) != GST_STATE_PLAYING)
    goto out;

  GST_TRACE ("audiotestsrc pushed out a buffer");

  pts = g_get_monotonic_time ();
  /* Ticks are once a second, so once we send something, we can skip
   * checking ~1sec of buffers till the next one. */
  if (self->send_pts > 0 && pts - self->send_pts <= 950 * 1000)
    goto out;

  /* Check if buffer contains a waveform */
  buffer = gst_pad_probe_info_get_buffer (info);
  offset = buffer_has_wave (buffer, pad);
  if (offset < 0)
    goto out;

  pts -= offset;
  {
    gint64 after = 0;
    if (self->send_pts > 0)
      after = (pts - self->send_pts) / 1000;
    GST_INFO ("send pts: %" G_GINT64_FORMAT "us (after %" G_GINT64_FORMAT
        "ms, offset %" G_GINT64_FORMAT "ms)", pts, after, offset / 1000);
  }

  self->send_pts = pts + offset;

out:
  return GST_PAD_PROBE_OK;
}

static GstPadProbeReturn
gst_audiolatency_src_probe (GstPad * pad, GstPadProbeInfo * info,
    gpointer user_data)
{
  GstAudioLatency *self = user_data;

  if (info->type & GST_PAD_PROBE_TYPE_BUFFER) {
    return gst_audiolatency_src_probe_buffer (pad, info, user_data);
  } else if (info->type & GST_PAD_PROBE_TYPE_QUERY_UPSTREAM) {
    GstQuery *query = gst_pad_probe_info_get_query (info);

    /* Forward latency query to the upstream sinkpad */
    if (GST_QUERY_TYPE (query) == GST_QUERY_LATENCY) {
      gboolean res = gst_pad_peer_query (self->sinkpad, query);
      GST_LOG_OBJECT (self,
          "Forwarded latency query to sinkpad. Result %d %" GST_PTR_FORMAT, res,
          query);
      return res ? GST_PAD_PROBE_HANDLED : GST_PAD_PROBE_DROP;
    }
  } else if (info->type & GST_PAD_PROBE_TYPE_EVENT_UPSTREAM) {
    GstEvent *event = gst_pad_probe_info_get_event (info);

    if (GST_EVENT_TYPE (event) == GST_EVENT_LATENCY) {
      gboolean res = gst_pad_push_event (self->sinkpad, event);

      GST_LOG_OBJECT (self,
          "Forwarded latency event to sinkpad. Result %d %" GST_PTR_FORMAT, res,
          event);
      if (!res) {
        /* This doesn't actually do anything - pad probe handling ignores
         * it, but maybe one day */
        GST_PAD_PROBE_INFO_FLOW_RETURN (info) = GST_FLOW_ERROR;
      }
      return GST_PAD_PROBE_HANDLED;
    }
  }

  return GST_PAD_PROBE_OK;
}

static GstFlowReturn
gst_audiolatency_sink_chain (GstPad * pad, GstObject * parent,
    GstBuffer * buffer)
{
  GstAudioLatency *self = GST_AUDIOLATENCY (parent);
  gint64 latency, offset, pts;

  /* Ignore buffers till something gets sent out by us. Fixes a bug where we'd
   * start out by printing one garbage latency value on Windows. */
  if (self->send_pts == 0)
    goto out;

  GST_TRACE_OBJECT (pad, "Got buffer %p", buffer);

  pts = g_get_monotonic_time ();
  /* Ticks are once a second, so once we receive something, we can skip
   * checking ~1sec of buffers till the next one. This way we also don't count
   * the same tick twice for latency measurement. */
  if (self->recv_pts > 0 && pts - self->recv_pts <= 950 * 1000)
    goto out;

  offset = buffer_has_wave (buffer, pad);
  if (offset < 0)
    goto out;

  self->recv_pts = pts + offset;
  latency = (self->recv_pts - self->send_pts);
  gst_audiolatency_set_latency (self, latency);

  GST_INFO ("recv pts: %" G_GINT64_FORMAT "us, latency: %" G_GINT64_FORMAT
      "ms, offset: %" G_GINT64_FORMAT "ms", self->recv_pts, latency / 1000,
      offset / 1000);

out:
  gst_buffer_unref (buffer);
  return GST_FLOW_OK;
}

static gboolean
gst_audiolatency_sink_event (GstPad * pad, GstObject * parent, GstEvent * event)
{
  switch (GST_EVENT_TYPE (event)) {
      /* Drop below events. audiotestsrc will push its own event */
    case GST_EVENT_STREAM_START:
    case GST_EVENT_CAPS:
    case GST_EVENT_SEGMENT:
      gst_event_unref (event);
      return TRUE;
    default:
      break;
  }

  return gst_pad_event_default (pad, parent, event);
}

/* Element registration */
static gboolean
plugin_init (GstPlugin * plugin)
{
  return GST_ELEMENT_REGISTER (audiolatency, plugin);
}

GST_PLUGIN_DEFINE (GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    audiolatency,
    "A plugin to measure audio latency",
    plugin_init, VERSION, "LGPL", GST_PACKAGE_NAME, GST_PACKAGE_ORIGIN)
