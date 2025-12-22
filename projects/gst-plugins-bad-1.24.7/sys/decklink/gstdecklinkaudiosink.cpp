/* GStreamer
 * Copyright (C) 2011 David Schleef <ds@entropywave.com>
 * Copyright (C) 2014 Sebastian Dröge <sebastian@centricular.com>
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
 * Free Software Foundation, Inc., 51 Franklin Street, Suite 500,
 * Boston, MA 02110-1335, USA.
 */
/**
 * SECTION:element-decklinkaudiosink
 * @short_description: Outputs Audio to a BlackMagic DeckLink Device
 * @see_also: decklinkvideosink
 *
 * Playout Video and Audio to a BlackMagic DeckLink Device. Can only be used
 * in conjunction with decklinkvideosink.
 *
 * ## Sample pipeline
 * |[
 * gst-launch-1.0 \
 *   videotestsrc ! decklinkvideosink device-number=0 mode=1080p25 \
 *   audiotestsrc ! decklinkaudiosink device-number=0
 * ]|
 * Playout a 1080p25 test-video with a test-audio signal to the SDI-Out of Card 0.
 * Devices are numbered starting with 0.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gstdecklinkaudiosink.h"
#include "gstdecklinkvideosink.h"
#include <string.h>

GST_DEBUG_CATEGORY_STATIC (gst_decklink_audio_sink_debug);
#define GST_CAT_DEFAULT gst_decklink_audio_sink_debug

#define DEFAULT_DEVICE_NUMBER (0)
#define DEFAULT_ALIGNMENT_THRESHOLD   (40 * GST_MSECOND)
#define DEFAULT_DISCONT_WAIT (1 * GST_SECOND)
// Microseconds for audiobasesink compatibility...
#define DEFAULT_BUFFER_TIME (50 * GST_MSECOND / 1000)

#define DEFAULT_PERSISTENT_ID (-1)

enum
{
  PROP_0,
  PROP_DEVICE_NUMBER,
  PROP_HW_SERIAL_NUMBER,
  PROP_ALIGNMENT_THRESHOLD,
  PROP_DISCONT_WAIT,
  PROP_BUFFER_TIME,
  PROP_PERSISTENT_ID
};

static void gst_decklink_audio_sink_set_property (GObject * object,
    guint property_id, const GValue * value, GParamSpec * pspec);
static void gst_decklink_audio_sink_get_property (GObject * object,
    guint property_id, GValue * value, GParamSpec * pspec);
static void gst_decklink_audio_sink_finalize (GObject * object);

static GstStateChangeReturn
gst_decklink_audio_sink_change_state (GstElement * element,
    GstStateChange transition);
static GstClock *gst_decklink_audio_sink_provide_clock (GstElement * element);

static GstCaps *gst_decklink_audio_sink_get_caps (GstBaseSink * bsink,
    GstCaps * filter);
static gboolean gst_decklink_audio_sink_set_caps (GstBaseSink * bsink,
    GstCaps * caps);
static GstFlowReturn gst_decklink_audio_sink_render (GstBaseSink * bsink,
    GstBuffer * buffer);
static gboolean gst_decklink_audio_sink_open (GstBaseSink * bsink);
static gboolean gst_decklink_audio_sink_close (GstBaseSink * bsink);
static gboolean gst_decklink_audio_sink_stop (GstDecklinkAudioSink * self);
static gboolean gst_decklink_audio_sink_unlock_stop (GstBaseSink * bsink);
static void gst_decklink_audio_sink_get_times (GstBaseSink * bsink,
    GstBuffer * buffer, GstClockTime * start, GstClockTime * end);
static gboolean gst_decklink_audio_sink_query (GstBaseSink * bsink,
    GstQuery * query);
static gboolean gst_decklink_audio_sink_event (GstBaseSink * bsink,
    GstEvent * event);

static GstStaticPadTemplate sink_template = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS
    ("audio/x-raw, format={S16LE,S32LE}, channels={2, 8, 16}, rate=48000, "
        "layout=interleaved")
    );

#define parent_class gst_decklink_audio_sink_parent_class
G_DEFINE_TYPE (GstDecklinkAudioSink, gst_decklink_audio_sink,
    GST_TYPE_BASE_SINK);
GST_ELEMENT_REGISTER_DEFINE_WITH_CODE (decklinkaudiosink, "decklinkaudiosink", GST_RANK_NONE,
    GST_TYPE_DECKLINK_AUDIO_SINK, decklink_element_init (plugin));

static void
gst_decklink_audio_sink_class_init (GstDecklinkAudioSinkClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GstElementClass *element_class = GST_ELEMENT_CLASS (klass);
  GstBaseSinkClass *basesink_class = GST_BASE_SINK_CLASS (klass);

  gobject_class->set_property = gst_decklink_audio_sink_set_property;
  gobject_class->get_property = gst_decklink_audio_sink_get_property;
  gobject_class->finalize = gst_decklink_audio_sink_finalize;

  element_class->change_state =
      GST_DEBUG_FUNCPTR (gst_decklink_audio_sink_change_state);
  element_class->provide_clock =
      GST_DEBUG_FUNCPTR (gst_decklink_audio_sink_provide_clock);

  basesink_class->get_caps =
      GST_DEBUG_FUNCPTR (gst_decklink_audio_sink_get_caps);
  basesink_class->set_caps =
      GST_DEBUG_FUNCPTR (gst_decklink_audio_sink_set_caps);
  basesink_class->render = GST_DEBUG_FUNCPTR (gst_decklink_audio_sink_render);
  // FIXME: These are misnamed in basesink!
  basesink_class->start = GST_DEBUG_FUNCPTR (gst_decklink_audio_sink_open);
  basesink_class->stop = GST_DEBUG_FUNCPTR (gst_decklink_audio_sink_close);
  basesink_class->unlock_stop =
      GST_DEBUG_FUNCPTR (gst_decklink_audio_sink_unlock_stop);
  basesink_class->get_times =
      GST_DEBUG_FUNCPTR (gst_decklink_audio_sink_get_times);
  basesink_class->query = GST_DEBUG_FUNCPTR (gst_decklink_audio_sink_query);
  basesink_class->event = GST_DEBUG_FUNCPTR (gst_decklink_audio_sink_event);

  g_object_class_install_property (gobject_class, PROP_DEVICE_NUMBER,
      g_param_spec_int ("device-number", "Device number",
          "Output device instance to use", 0, G_MAXINT, DEFAULT_DEVICE_NUMBER,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
              G_PARAM_CONSTRUCT)));

    /**
   * GstDecklinkAudioSink:persistent-id
   *
   * Decklink device to use. Higher priority than "device-number".
   * BMDDeckLinkPersistentID is a device speciﬁc, 32-bit unique identiﬁer.
   * It is stable even when the device is plugged in a diﬀerent connector,
   * across reboots, and when plugged into diﬀerent computers.
   *
   * Since: 1.22
   */
  g_object_class_install_property (gobject_class, PROP_PERSISTENT_ID,
      g_param_spec_int64 ("persistent-id", "Persistent id",
          "Output device instance to use. Higher priority than \"device-number\".",
          DEFAULT_PERSISTENT_ID, G_MAXINT64, DEFAULT_PERSISTENT_ID,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
              G_PARAM_CONSTRUCT)));

  g_object_class_install_property (gobject_class, PROP_HW_SERIAL_NUMBER,
      g_param_spec_string ("hw-serial-number", "Hardware serial number",
          "The serial number (hardware ID) of the Decklink card",
          NULL, (GParamFlags) (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS)));

  g_object_class_install_property (gobject_class, PROP_ALIGNMENT_THRESHOLD,
      g_param_spec_uint64 ("alignment-threshold", "Alignment Threshold",
          "Timestamp alignment threshold in nanoseconds", 0,
          G_MAXUINT64 - 1, DEFAULT_ALIGNMENT_THRESHOLD,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
              GST_PARAM_MUTABLE_READY)));

  g_object_class_install_property (gobject_class, PROP_DISCONT_WAIT,
      g_param_spec_uint64 ("discont-wait", "Discont Wait",
          "Window of time in nanoseconds to wait before "
          "creating a discontinuity", 0,
          G_MAXUINT64 - 1, DEFAULT_DISCONT_WAIT,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
              GST_PARAM_MUTABLE_READY)));

  g_object_class_install_property (gobject_class, PROP_BUFFER_TIME,
      g_param_spec_uint64 ("buffer-time", "Buffer Time",
          "Size of audio buffer in microseconds, this is the minimum latency that the sink reports",
          0, G_MAXUINT64, DEFAULT_BUFFER_TIME,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
              GST_PARAM_MUTABLE_READY)));

  gst_element_class_add_static_pad_template (element_class, &sink_template);

  gst_element_class_set_static_metadata (element_class, "Decklink Audio Sink",
      "Audio/Sink/Hardware", "Decklink Sink",
      "David Schleef <ds@entropywave.com>, "
      "Sebastian Dröge <sebastian@centricular.com>");

  GST_DEBUG_CATEGORY_INIT (gst_decklink_audio_sink_debug, "decklinkaudiosink",
      0, "debug category for decklinkaudiosink element");
}

static void
gst_decklink_audio_sink_init (GstDecklinkAudioSink * self)
{
  self->device_number = DEFAULT_DEVICE_NUMBER;
  self->stream_align =
      gst_audio_stream_align_new (48000, DEFAULT_ALIGNMENT_THRESHOLD,
      DEFAULT_DISCONT_WAIT);
  self->buffer_time = DEFAULT_BUFFER_TIME * 1000;

  self->persistent_id = DEFAULT_PERSISTENT_ID;

  gst_base_sink_set_max_lateness (GST_BASE_SINK_CAST (self), 20 * GST_MSECOND);
}

void
gst_decklink_audio_sink_set_property (GObject * object, guint property_id,
    const GValue * value, GParamSpec * pspec)
{
  GstDecklinkAudioSink *self = GST_DECKLINK_AUDIO_SINK_CAST (object);

  switch (property_id) {
    case PROP_DEVICE_NUMBER:
      self->device_number = g_value_get_int (value);
      break;
    case PROP_ALIGNMENT_THRESHOLD:
      GST_OBJECT_LOCK (self);
      gst_audio_stream_align_set_alignment_threshold (self->stream_align,
          g_value_get_uint64 (value));
      GST_OBJECT_UNLOCK (self);
      break;
    case PROP_DISCONT_WAIT:
      GST_OBJECT_LOCK (self);
      gst_audio_stream_align_set_discont_wait (self->stream_align,
          g_value_get_uint64 (value));
      GST_OBJECT_UNLOCK (self);
      break;
    case PROP_BUFFER_TIME:
      GST_OBJECT_LOCK (self);
      self->buffer_time = g_value_get_uint64 (value) * 1000;
      GST_OBJECT_UNLOCK (self);
      break;
    case PROP_PERSISTENT_ID:
      self->persistent_id = g_value_get_int64 (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

void
gst_decklink_audio_sink_get_property (GObject * object, guint property_id,
    GValue * value, GParamSpec * pspec)
{
  GstDecklinkAudioSink *self = GST_DECKLINK_AUDIO_SINK_CAST (object);

  switch (property_id) {
    case PROP_DEVICE_NUMBER:
      g_value_set_int (value, self->device_number);
      break;
    case PROP_HW_SERIAL_NUMBER:
      if (self->output)
        g_value_set_string (value, self->output->hw_serial_number);
      else
        g_value_set_string (value, NULL);
      break;
    case PROP_ALIGNMENT_THRESHOLD:
      GST_OBJECT_LOCK (self);
      g_value_set_uint64 (value,
          gst_audio_stream_align_get_alignment_threshold (self->stream_align));
      GST_OBJECT_UNLOCK (self);
      break;
    case PROP_DISCONT_WAIT:
      GST_OBJECT_LOCK (self);
      g_value_set_uint64 (value,
          gst_audio_stream_align_get_discont_wait (self->stream_align));
      GST_OBJECT_UNLOCK (self);
      break;
    case PROP_BUFFER_TIME:
      GST_OBJECT_LOCK (self);
      g_value_set_uint64 (value, self->buffer_time / 1000);
      GST_OBJECT_UNLOCK (self);
      break;
    case PROP_PERSISTENT_ID:
      g_value_set_int64 (value, self->persistent_id);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

void
gst_decklink_audio_sink_finalize (GObject * object)
{
  GstDecklinkAudioSink *self = GST_DECKLINK_AUDIO_SINK_CAST (object);

  if (self->stream_align) {
    gst_audio_stream_align_free (self->stream_align);
    self->stream_align = NULL;
  }

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static gboolean
gst_decklink_audio_sink_set_caps (GstBaseSink * bsink, GstCaps * caps)
{
  GstDecklinkAudioSink *self = GST_DECKLINK_AUDIO_SINK_CAST (bsink);
  HRESULT ret;
  BMDAudioSampleType sample_depth;
  GstAudioInfo info;

  GST_DEBUG_OBJECT (self, "Setting caps %" GST_PTR_FORMAT, caps);

  if (!gst_audio_info_from_caps (&info, caps))
    return FALSE;

  if (self->output->audio_enabled
      && (self->info.finfo->format != info.finfo->format
          || self->info.channels != info.channels)) {
    GST_ERROR_OBJECT (self, "Reconfiguration not supported");
    return FALSE;
  } else if (self->output->audio_enabled) {
    return TRUE;
  }

  if (info.finfo->format == GST_AUDIO_FORMAT_S16LE) {
    sample_depth = bmdAudioSampleType16bitInteger;
  } else {
    sample_depth = bmdAudioSampleType32bitInteger;
  }

  g_mutex_lock (&self->output->lock);
  ret = self->output->output->EnableAudioOutput (bmdAudioSampleRate48kHz,
      sample_depth, info.channels, bmdAudioOutputStreamContinuous);
  if (ret != S_OK) {
    g_mutex_unlock (&self->output->lock);
    GST_WARNING_OBJECT (self, "Failed to enable audio output 0x%08lx",
        (unsigned long) ret);
    return FALSE;
  }

  self->output->audio_enabled = TRUE;
  self->info = info;

  if (self->output->start_scheduled_playback && self->output->videosink)
    self->output->start_scheduled_playback (self->output->videosink);
  g_mutex_unlock (&self->output->lock);

  // Create a new resampler as needed
  if (self->resampler)
    gst_audio_resampler_free (self->resampler);
  self->resampler = NULL;

  return TRUE;
}

static GstCaps *
gst_decklink_audio_sink_get_caps (GstBaseSink * bsink, GstCaps * filter)
{
  GstDecklinkAudioSink *self = GST_DECKLINK_AUDIO_SINK_CAST (bsink);
  GstCaps *caps;

  if ((caps = gst_pad_get_current_caps (GST_BASE_SINK_PAD (bsink))))
    return caps;

  caps = gst_pad_get_pad_template_caps (GST_BASE_SINK_PAD (bsink));

  GST_OBJECT_LOCK (self);
  if (self->output && self->output->attributes) {
    int64_t max_channels = 0;
    HRESULT ret;
    GstStructure *s;
    GValue arr = G_VALUE_INIT;
    GValue v = G_VALUE_INIT;

    ret =
        self->output->attributes->GetInt (BMDDeckLinkMaximumAudioChannels,
        &max_channels);
    /* 2 should always be supported */
    if (ret != S_OK) {
      max_channels = 2;
    }

    caps = gst_caps_make_writable (caps);
    s = gst_caps_get_structure (caps, 0);

    g_value_init (&arr, GST_TYPE_LIST);
    g_value_init (&v, G_TYPE_INT);
    if (max_channels >= 16) {
      g_value_set_int (&v, 16);
      gst_value_list_append_value (&arr, &v);
    }
    if (max_channels >= 8) {
      g_value_set_int (&v, 8);
      gst_value_list_append_value (&arr, &v);
    }
    g_value_set_int (&v, 2);
    gst_value_list_append_value (&arr, &v);

    gst_structure_set_value (s, "channels", &arr);
    g_value_unset (&v);
    g_value_unset (&arr);
  }
  GST_OBJECT_UNLOCK (self);

  if (filter) {
    GstCaps *intersection =
        gst_caps_intersect_full (filter, caps, GST_CAPS_INTERSECT_FIRST);
    gst_caps_unref (caps);
    caps = intersection;
  }

  return caps;
}

static gboolean
gst_decklink_audio_sink_query (GstBaseSink * bsink, GstQuery * query)
{
  GstDecklinkAudioSink *self = GST_DECKLINK_AUDIO_SINK (bsink);
  gboolean res = FALSE;

  switch (GST_QUERY_TYPE (query)) {
    case GST_QUERY_LATENCY:
    {
      gboolean live, us_live;
      GstClockTime min_l, max_l;

      GST_DEBUG_OBJECT (self, "latency query");

      /* ask parent first, it will do an upstream query for us. */
      if ((res =
              gst_base_sink_query_latency (GST_BASE_SINK_CAST (self), &live,
                  &us_live, &min_l, &max_l))) {
        GstClockTime base_latency, min_latency, max_latency;

        /* we and upstream are both live, adjust the min_latency */
        if (live && us_live) {
          GST_OBJECT_LOCK (self);
          if (!self->info.rate) {
            GST_OBJECT_UNLOCK (self);

            GST_DEBUG_OBJECT (self,
                "we are not negotiated, can't report latency yet");
            res = FALSE;
            goto done;
          }

          base_latency = self->buffer_time * 1000;
          GST_OBJECT_UNLOCK (self);

          /* we cannot go lower than the buffer size and the min peer latency */
          min_latency = base_latency + min_l;
          /* the max latency is the max of the peer, we can delay an infinite
           * amount of time. */
          max_latency =
              (max_l ==
              GST_CLOCK_TIME_NONE) ? GST_CLOCK_TIME_NONE : (base_latency +
              max_l);

          GST_DEBUG_OBJECT (self,
              "peer min %" GST_TIME_FORMAT ", our min latency: %"
              GST_TIME_FORMAT, GST_TIME_ARGS (min_l),
              GST_TIME_ARGS (min_latency));
          GST_DEBUG_OBJECT (self,
              "peer max %" GST_TIME_FORMAT ", our max latency: %"
              GST_TIME_FORMAT, GST_TIME_ARGS (max_l),
              GST_TIME_ARGS (max_latency));
        } else {
          GST_DEBUG_OBJECT (self,
              "peer or we are not live, don't care about latency");
          min_latency = min_l;
          max_latency = max_l;
        }
        gst_query_set_latency (query, live, min_latency, max_latency);
      }
      break;
    }
    default:
      res = GST_BASE_SINK_CLASS (parent_class)->query (bsink, query);
      break;
  }

done:
  return res;
}

static gboolean
gst_decklink_audio_sink_event (GstBaseSink * bsink, GstEvent * event)
{
  GstDecklinkAudioSink *self = GST_DECKLINK_AUDIO_SINK_CAST (bsink);

  if (GST_EVENT_TYPE (event) == GST_EVENT_SEGMENT) {
    const GstSegment *new_segment;

    gst_event_parse_segment (event, &new_segment);

    if (ABS (new_segment->rate) != 1.0) {
      guint out_rate = self->info.rate / ABS (new_segment->rate);

      if (self->resampler && (self->resampler_out_rate != out_rate
              || self->resampler_in_rate != (guint) self->info.rate))
        gst_audio_resampler_update (self->resampler, self->info.rate, out_rate,
            NULL);
      else if (!self->resampler)
        self->resampler =
            gst_audio_resampler_new (GST_AUDIO_RESAMPLER_METHOD_LINEAR,
            GST_AUDIO_RESAMPLER_FLAG_NONE, self->info.finfo->format,
            self->info.channels, self->info.rate, out_rate, NULL);

      self->resampler_in_rate = self->info.rate;
      self->resampler_out_rate = out_rate;
    } else if (self->resampler) {
      gst_audio_resampler_free (self->resampler);
      self->resampler = NULL;
    }

    if (new_segment->rate < 0)
      gst_audio_stream_align_set_rate (self->stream_align, -48000);
  }

  return GST_BASE_SINK_CLASS (parent_class)->event (bsink, event);
}

static GstFlowReturn
gst_decklink_audio_sink_render (GstBaseSink * bsink, GstBuffer * buffer)
{
  GstDecklinkAudioSink *self = GST_DECKLINK_AUDIO_SINK_CAST (bsink);
  GstDecklinkVideoSink *video_sink;
  GstFlowReturn flow_ret;
  HRESULT ret;
  GstClockTime timestamp, duration;
  GstClockTime running_time, running_time_duration;
  GstClockTime schedule_time, schedule_time_duration;
  GstClockTime latency, render_delay;
  GstClockTimeDiff ts_offset;
  GstMapInfo map_info;
  const guint8 *data;
  gsize len, written_all;
  gboolean discont;

  GST_DEBUG_OBJECT (self, "Rendering buffer %p", buffer);

  // FIXME: Handle no timestamps
  if (!GST_BUFFER_TIMESTAMP_IS_VALID (buffer)) {
    return GST_FLOW_ERROR;
  }

  if (GST_BASE_SINK_CAST (self)->flushing) {
    return GST_FLOW_FLUSHING;
  }
  // If we're called before output is actually started, start pre-rolling
  if (!self->output->started) {
    self->output->output->BeginAudioPreroll ();
  }

  video_sink =
      GST_DECKLINK_VIDEO_SINK (gst_object_ref (self->output->videosink));

  timestamp = GST_BUFFER_TIMESTAMP (buffer);
  duration = GST_BUFFER_DURATION (buffer);
  discont = gst_audio_stream_align_process (self->stream_align,
      GST_BUFFER_IS_DISCONT (buffer), timestamp,
      gst_buffer_get_size (buffer) / self->info.bpf, &timestamp, &duration,
      NULL);

  if (discont && self->resampler)
    gst_audio_resampler_reset (self->resampler);

  if (GST_BASE_SINK_CAST (self)->segment.rate < 0.0) {
    GstMapInfo out_map;
    gint out_frames = gst_buffer_get_size (buffer) / self->info.bpf;

    buffer = gst_buffer_make_writable (gst_buffer_ref (buffer));

    gst_buffer_map (buffer, &out_map, GST_MAP_READWRITE);
    if (self->info.finfo->format == GST_AUDIO_FORMAT_S16) {
      gint16 *swap_data = (gint16 *) out_map.data;
      gint16 *swap_data_end =
          swap_data + (out_frames - 1) * self->info.channels;
      gint16 swap_tmp[16];

      while (out_frames > 0) {
        memcpy (&swap_tmp, swap_data, self->info.bpf);
        memcpy (swap_data, swap_data_end, self->info.bpf);
        memcpy (swap_data_end, &swap_tmp, self->info.bpf);

        swap_data += self->info.channels;
        swap_data_end -= self->info.channels;

        out_frames -= 2;
      }
    } else {
      gint32 *swap_data = (gint32 *) out_map.data;
      gint32 *swap_data_end =
          swap_data + (out_frames - 1) * self->info.channels;
      gint32 swap_tmp[16];

      while (out_frames > 0) {
        memcpy (&swap_tmp, swap_data, self->info.bpf);
        memcpy (swap_data, swap_data_end, self->info.bpf);
        memcpy (swap_data_end, &swap_tmp, self->info.bpf);

        swap_data += self->info.channels;
        swap_data_end -= self->info.channels;

        out_frames -= 2;
      }
    }
    gst_buffer_unmap (buffer, &out_map);
  } else {
    gst_buffer_ref (buffer);
  }

  if (self->resampler) {
    gint in_frames = gst_buffer_get_size (buffer) / self->info.bpf;
    gint out_frames =
        gst_audio_resampler_get_out_frames (self->resampler, in_frames);
    GstBuffer *out_buf = gst_buffer_new_and_alloc (out_frames * self->info.bpf);
    GstMapInfo out_map;

    gst_buffer_map (buffer, &map_info, GST_MAP_READ);
    gst_buffer_map (out_buf, &out_map, GST_MAP_READWRITE);

    gst_audio_resampler_resample (self->resampler, (gpointer *) & map_info.data,
        in_frames, (gpointer *) & out_map.data, out_frames);

    gst_buffer_unmap (out_buf, &out_map);
    gst_buffer_unmap (buffer, &map_info);
    buffer = out_buf;
  }

  gst_buffer_map (buffer, &map_info, GST_MAP_READ);
  data = map_info.data;
  len = map_info.size / self->info.bpf;
  written_all = 0;

  do {
    GstClockTime timestamp_now =
        timestamp + gst_util_uint64_scale (written_all, GST_SECOND,
        self->info.rate);
    guint32 buffered_samples;
    GstClockTime buffered_time;
    guint32 written = 0;
    GstClock *clock;
    GstClockTimeDiff clock_ahead;

    if (GST_BASE_SINK_CAST (self)->flushing) {
      flow_ret = GST_FLOW_FLUSHING;
      break;
    }

    running_time =
        gst_segment_to_running_time (&GST_BASE_SINK_CAST (self)->segment,
        GST_FORMAT_TIME, timestamp_now);
    running_time_duration =
        gst_segment_to_running_time (&GST_BASE_SINK_CAST (self)->segment,
        GST_FORMAT_TIME, timestamp_now + duration) - running_time;

    /* See gst_base_sink_adjust_time() */
    latency = gst_base_sink_get_latency (bsink);
    render_delay = gst_base_sink_get_render_delay (bsink);
    ts_offset = gst_base_sink_get_ts_offset (bsink);
    running_time += latency;

    if (ts_offset < 0) {
      ts_offset = -ts_offset;
      if ((GstClockTime) ts_offset < running_time)
        running_time -= ts_offset;
      else
        running_time = 0;
    } else {
      running_time += ts_offset;
    }

    if (running_time > render_delay)
      running_time -= render_delay;
    else
      running_time = 0;

    clock = gst_element_get_clock (GST_ELEMENT_CAST (self));
    clock_ahead = 0;
    if (clock) {
      GstClockTime clock_now = gst_clock_get_time (clock);
      GstClockTime base_time =
          gst_element_get_base_time (GST_ELEMENT_CAST (self));
      gst_object_unref (clock);
      clock = NULL;

      if (clock_now != GST_CLOCK_TIME_NONE && base_time != GST_CLOCK_TIME_NONE) {
        GST_DEBUG_OBJECT (self,
            "Clock time %" GST_TIME_FORMAT ", base time %" GST_TIME_FORMAT
            ", target running time %" GST_TIME_FORMAT,
            GST_TIME_ARGS (clock_now), GST_TIME_ARGS (base_time),
            GST_TIME_ARGS (running_time));
        if (clock_now > base_time)
          clock_now -= base_time;
        else
          clock_now = 0;

        clock_ahead = running_time - clock_now;
      }
    }

    GST_DEBUG_OBJECT (self,
        "Ahead %" GST_STIME_FORMAT " of the clock running time",
        GST_STIME_ARGS (clock_ahead));

    if (self->output->
        output->GetBufferedAudioSampleFrameCount (&buffered_samples) != S_OK)
      buffered_samples = 0;

    buffered_time =
        gst_util_uint64_scale (buffered_samples, GST_SECOND, self->info.rate);
    buffered_time /= ABS (GST_BASE_SINK_CAST (self)->segment.rate);
    GST_DEBUG_OBJECT (self,
        "Buffered %" GST_TIME_FORMAT " in the driver (%u samples)",
        GST_TIME_ARGS (buffered_time), buffered_samples);

    {
      GstClockTimeDiff buffered_ahead_of_clock_ahead =
          GST_CLOCK_DIFF (clock_ahead, buffered_time);

      GST_DEBUG_OBJECT (self, "driver is %" GST_STIME_FORMAT " ahead of the "
          "expected clock", GST_STIME_ARGS (buffered_ahead_of_clock_ahead));
      /* we don't want to store too much data in the driver as decklink
       * doesn't seem to actually use our provided timestamps to perform its
       * own synchronisation. It seems to count samples instead. */
      /* FIXME: do we need to split buffers? */
      if (buffered_ahead_of_clock_ahead > 0 &&
          buffered_ahead_of_clock_ahead >
          gst_base_sink_get_max_lateness (bsink)) {
        GST_DEBUG_OBJECT (self,
            "Dropping buffer that is %" GST_STIME_FORMAT " too late",
            GST_STIME_ARGS (buffered_ahead_of_clock_ahead));
        if (self->resampler)
          gst_audio_resampler_reset (self->resampler);
        flow_ret = GST_FLOW_OK;
        break;
      }
    }

    // We start waiting once we have more than buffer-time buffered
    if (((GstClockTime) clock_ahead) > self->buffer_time) {
      GstClockReturn clock_ret;
      GstClockTime wait_time = running_time;

      GST_DEBUG_OBJECT (self,
          "Buffered enough, wait for preroll or the clock or flushing. "
          "Configured buffer time: %" GST_TIME_FORMAT,
          GST_TIME_ARGS (self->buffer_time));

      if (wait_time < self->buffer_time)
        wait_time = 0;
      else
        wait_time -= self->buffer_time;

      flow_ret =
          gst_base_sink_do_preroll (GST_BASE_SINK_CAST (self),
          GST_MINI_OBJECT_CAST (buffer));
      if (flow_ret != GST_FLOW_OK)
        break;

      clock_ret =
          gst_base_sink_wait_clock (GST_BASE_SINK_CAST (self), wait_time, NULL);
      if (GST_BASE_SINK_CAST (self)->flushing) {
        flow_ret = GST_FLOW_FLUSHING;
        break;
      }
      // Rerun the whole loop again
      if (clock_ret == GST_CLOCK_UNSCHEDULED)
        continue;
    }

    schedule_time = running_time;
    schedule_time_duration = running_time_duration;

    gst_decklink_video_sink_convert_to_internal_clock (video_sink,
        &schedule_time, &schedule_time_duration);

    GST_LOG_OBJECT (self, "Scheduling audio samples at %" GST_TIME_FORMAT
        " with duration %" GST_TIME_FORMAT, GST_TIME_ARGS (schedule_time),
        GST_TIME_ARGS (schedule_time_duration));

    ret = self->output->output->ScheduleAudioSamples ((void *) data, len,
        schedule_time, GST_SECOND, &written);
    if (ret != S_OK) {
      bool is_running = true;
      self->output->output->IsScheduledPlaybackRunning (&is_running);

      if (is_running && !GST_BASE_SINK_CAST (self)->flushing
          && self->output->started) {
        GST_ELEMENT_ERROR (self, STREAM, FAILED, (NULL),
            ("Failed to schedule frame: 0x%08lx", (unsigned long) ret));
        flow_ret = GST_FLOW_ERROR;
        break;
      } else {
        // Ignore the error and go out of the loop here, we're shutting down
        // or are not started yet and there's nothing we can do at this point
        GST_INFO_OBJECT (self,
            "Ignoring scheduling error 0x%08x because we're not started yet"
            " or not anymore", (guint) ret);
        flow_ret = GST_FLOW_OK;
        break;
      }
    }

    len -= written;
    data += written * self->info.bpf;
    if (self->resampler)
      written_all += written * ABS (GST_BASE_SINK_CAST (self)->segment.rate);
    else
      written_all += written;

    flow_ret = GST_FLOW_OK;
  } while (len > 0);

  gst_buffer_unmap (buffer, &map_info);
  gst_buffer_unref (buffer);

  GST_DEBUG_OBJECT (self, "Returning %s", gst_flow_get_name (flow_ret));

  return flow_ret;
}

static gboolean
gst_decklink_audio_sink_open (GstBaseSink * bsink)
{
  GstDecklinkAudioSink *self = GST_DECKLINK_AUDIO_SINK_CAST (bsink);

  GST_DEBUG_OBJECT (self, "Starting");

  self->output =
      gst_decklink_acquire_nth_output (self->device_number, self->persistent_id,
      GST_ELEMENT_CAST (self), TRUE);
  if (!self->output) {
    GST_ERROR_OBJECT (self, "Failed to acquire output");
    return FALSE;
  }

  g_object_notify (G_OBJECT (self), "hw-serial-number");

  return TRUE;
}

static gboolean
gst_decklink_audio_sink_close (GstBaseSink * bsink)
{
  GstDecklinkAudioSink *self = GST_DECKLINK_AUDIO_SINK_CAST (bsink);

  GST_DEBUG_OBJECT (self, "Closing");

  if (self->output) {
    g_mutex_lock (&self->output->lock);
    self->output->mode = NULL;
    self->output->audio_enabled = FALSE;
    if (self->output->start_scheduled_playback && self->output->videosink)
      self->output->start_scheduled_playback (self->output->videosink);
    g_mutex_unlock (&self->output->lock);

    self->output->output->DisableAudioOutput ();
    gst_decklink_release_nth_output (self->device_number, self->persistent_id,
        GST_ELEMENT_CAST (self), TRUE);
    self->output = NULL;
  }

  return TRUE;
}

static gboolean
gst_decklink_audio_sink_stop (GstDecklinkAudioSink * self)
{
  GST_DEBUG_OBJECT (self, "Stopping");

  if (self->output && self->output->audio_enabled) {
    g_mutex_lock (&self->output->lock);
    self->output->audio_enabled = FALSE;
    g_mutex_unlock (&self->output->lock);

    self->output->output->DisableAudioOutput ();
  }

  if (self->resampler) {
    gst_audio_resampler_free (self->resampler);
    self->resampler = NULL;
  }

  return TRUE;
}

static gboolean
gst_decklink_audio_sink_unlock_stop (GstBaseSink * bsink)
{
  GstDecklinkAudioSink *self = GST_DECKLINK_AUDIO_SINK (bsink);

  if (self->output) {
    self->output->output->FlushBufferedAudioSamples ();
  }

  return TRUE;
}

static void
gst_decklink_audio_sink_get_times (GstBaseSink * bsink, GstBuffer * buffer,
    GstClockTime * start, GstClockTime * end)
{
  /* our clock sync is a bit too much for the base class to handle so
   * we implement it ourselves. */
  *start = GST_CLOCK_TIME_NONE;
  *end = GST_CLOCK_TIME_NONE;
}

static GstStateChangeReturn
gst_decklink_audio_sink_change_state (GstElement * element,
    GstStateChange transition)
{
  GstDecklinkAudioSink *self = GST_DECKLINK_AUDIO_SINK_CAST (element);
  GstStateChangeReturn ret = GST_STATE_CHANGE_SUCCESS;

  switch (transition) {
    case GST_STATE_CHANGE_READY_TO_PAUSED:
      GST_OBJECT_LOCK (self);
      gst_audio_stream_align_mark_discont (self->stream_align);
      GST_OBJECT_UNLOCK (self);

      g_mutex_lock (&self->output->lock);
      if (self->output->start_scheduled_playback)
        self->output->start_scheduled_playback (self->output->videosink);
      g_mutex_unlock (&self->output->lock);
      break;
    case GST_STATE_CHANGE_PAUSED_TO_READY:
      gst_decklink_audio_sink_stop (self);
      break;
    default:
      break;
  }

  ret = GST_ELEMENT_CLASS (parent_class)->change_state (element, transition);
  if (ret == GST_STATE_CHANGE_FAILURE)
    return ret;

  switch (transition) {
    default:
      break;
  }

  return ret;
}

static GstClock *
gst_decklink_audio_sink_provide_clock (GstElement * element)
{
  GstDecklinkAudioSink *self = GST_DECKLINK_AUDIO_SINK_CAST (element);

  if (!self->output)
    return NULL;

  return GST_CLOCK_CAST (gst_object_ref (self->output->clock));
}
