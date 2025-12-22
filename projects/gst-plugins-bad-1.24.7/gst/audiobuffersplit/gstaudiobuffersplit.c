/*
 * GStreamer
 * Copyright (C) 2016 Sebastian Dröge <sebastian@centricular.com>
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

#include "gstaudiobuffersplit.h"

#define GST_CAT_DEFAULT gst_audio_buffer_split_debug
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);

static GstStaticPadTemplate sink_template = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (GST_AUDIO_CAPS_MAKE (GST_AUDIO_FORMATS_ALL)
        ", layout = (string) interleaved")
    );

static GstStaticPadTemplate src_template = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (GST_AUDIO_CAPS_MAKE (GST_AUDIO_FORMATS_ALL)
        ", layout = (string) interleaved")
    );

enum
{
  PROP_0,
  PROP_OUTPUT_BUFFER_DURATION,
  PROP_OUTPUT_BUFFER_SIZE,
  PROP_ALIGNMENT_THRESHOLD,
  PROP_DISCONT_WAIT,
  PROP_STRICT_BUFFER_SIZE,
  PROP_GAPLESS,
  PROP_MAX_SILENCE_TIME,
  LAST_PROP
};

#define DEFAULT_OUTPUT_BUFFER_DURATION_N (1)
#define DEFAULT_OUTPUT_BUFFER_DURATION_D (50)
#define DEFAULT_ALIGNMENT_THRESHOLD   (40 * GST_MSECOND)
#define DEFAULT_DISCONT_WAIT (1 * GST_SECOND)
#define DEFAULT_STRICT_BUFFER_SIZE (FALSE)
#define DEFAULT_GAPLESS (FALSE)
#define DEFAULT_MAX_SILENCE_TIME (0)

#define parent_class gst_audio_buffer_split_parent_class
G_DEFINE_TYPE_WITH_CODE (GstAudioBufferSplit, gst_audio_buffer_split,
    GST_TYPE_ELEMENT, GST_DEBUG_CATEGORY_INIT (gst_audio_buffer_split_debug,
        "audiobuffersplit", 0, "Audio buffer splitter"););
GST_ELEMENT_REGISTER_DEFINE (audiobuffersplit, "audiobuffersplit",
    GST_RANK_NONE, GST_TYPE_AUDIO_BUFFER_SPLIT);

static GstFlowReturn gst_audio_buffer_split_sink_chain (GstPad * pad,
    GstObject * parent, GstBuffer * buffer);
static gboolean gst_audio_buffer_split_sink_event (GstPad * pad,
    GstObject * parent, GstEvent * event);
static gboolean gst_audio_buffer_split_src_query (GstPad * pad,
    GstObject * parent, GstQuery * query);

static void gst_audio_buffer_split_finalize (GObject * object);
static void gst_audio_buffer_split_get_property (GObject * object,
    guint property_id, GValue * value, GParamSpec * pspec);
static void gst_audio_buffer_split_set_property (GObject * object,
    guint property_id, const GValue * value, GParamSpec * pspec);

static GstStateChangeReturn gst_audio_buffer_split_change_state (GstElement *
    element, GstStateChange transition);

static void
gst_audio_buffer_split_class_init (GstAudioBufferSplitClass * klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;
  GstElementClass *gstelement_class = (GstElementClass *) klass;

  gobject_class->set_property = gst_audio_buffer_split_set_property;
  gobject_class->get_property = gst_audio_buffer_split_get_property;
  gobject_class->finalize = gst_audio_buffer_split_finalize;

  g_object_class_install_property (gobject_class, PROP_OUTPUT_BUFFER_DURATION,
      gst_param_spec_fraction ("output-buffer-duration",
          "Output Buffer Duration", "Output block size in seconds", 1, G_MAXINT,
          G_MAXINT, 1, DEFAULT_OUTPUT_BUFFER_DURATION_N,
          DEFAULT_OUTPUT_BUFFER_DURATION_D,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
          GST_PARAM_MUTABLE_READY));

  /**
   * GstAudioBufferSplit:output-buffer-size
   *
   * Allow specifying a buffer size for splitting. Zero by default.
   * Takes precedence over output-buffer-duration when set to a
   * non zero value else will not be in effect.
   *
   * Since: 1.20
   */
  g_object_class_install_property (gobject_class, PROP_OUTPUT_BUFFER_SIZE,
      g_param_spec_uint ("output-buffer-size", "Output buffer size",
          "Output block size in bytes, takes precedence over "
          "buffer duration when set to non zero", 0, G_MAXINT, 0,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
          GST_PARAM_MUTABLE_READY));

  g_object_class_install_property (gobject_class, PROP_ALIGNMENT_THRESHOLD,
      g_param_spec_uint64 ("alignment-threshold", "Alignment Threshold",
          "Timestamp alignment threshold in nanoseconds", 0,
          G_MAXUINT64 - 1, DEFAULT_ALIGNMENT_THRESHOLD,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
          GST_PARAM_MUTABLE_READY));

  g_object_class_install_property (gobject_class, PROP_DISCONT_WAIT,
      g_param_spec_uint64 ("discont-wait", "Discont Wait",
          "Window of time in nanoseconds to wait before "
          "creating a discontinuity", 0,
          G_MAXUINT64 - 1, DEFAULT_DISCONT_WAIT,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
          GST_PARAM_MUTABLE_READY));

  g_object_class_install_property (gobject_class, PROP_STRICT_BUFFER_SIZE,
      g_param_spec_boolean ("strict-buffer-size", "Strict buffer size",
          "Discard the last samples at EOS or discont if they are too "
          "small to fill a buffer", DEFAULT_STRICT_BUFFER_SIZE,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
          GST_PARAM_MUTABLE_READY));

  g_object_class_install_property (gobject_class, PROP_GAPLESS,
      g_param_spec_boolean ("gapless", "Gapless",
          "Insert silence/drop samples instead of creating a discontinuity",
          DEFAULT_GAPLESS,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
          GST_PARAM_MUTABLE_READY));

  g_object_class_install_property (gobject_class, PROP_MAX_SILENCE_TIME,
      g_param_spec_uint64 ("max-silence-time",
          "Maximum time of silence to insert",
          "Do not insert silence in gapless mode if the gap exceeds this "
          "period (in ns) (0 = disabled)",
          0, G_MAXUINT64, DEFAULT_MAX_SILENCE_TIME,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
          GST_PARAM_MUTABLE_READY));

  gst_element_class_set_static_metadata (gstelement_class,
      "Audio Buffer Split", "Audio/Filter",
      "Splits raw audio buffers into equal sized chunks",
      "Sebastian Dröge <sebastian@centricular.com>");

  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&src_template));
  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&sink_template));

  gstelement_class->change_state = gst_audio_buffer_split_change_state;
}

static void
gst_audio_buffer_split_init (GstAudioBufferSplit * self)
{
  self->sinkpad = gst_pad_new_from_static_template (&sink_template, "sink");
  gst_pad_set_chain_function (self->sinkpad,
      GST_DEBUG_FUNCPTR (gst_audio_buffer_split_sink_chain));
  gst_pad_set_event_function (self->sinkpad,
      GST_DEBUG_FUNCPTR (gst_audio_buffer_split_sink_event));
  GST_PAD_SET_PROXY_CAPS (self->sinkpad);
  gst_element_add_pad (GST_ELEMENT (self), self->sinkpad);

  self->srcpad = gst_pad_new_from_static_template (&src_template, "src");
  gst_pad_set_query_function (self->srcpad,
      GST_DEBUG_FUNCPTR (gst_audio_buffer_split_src_query));
  GST_PAD_SET_PROXY_CAPS (self->srcpad);
  gst_pad_use_fixed_caps (self->srcpad);
  gst_element_add_pad (GST_ELEMENT (self), self->srcpad);

  self->output_buffer_duration_n = DEFAULT_OUTPUT_BUFFER_DURATION_N;
  self->output_buffer_duration_d = DEFAULT_OUTPUT_BUFFER_DURATION_D;
  self->strict_buffer_size = DEFAULT_STRICT_BUFFER_SIZE;
  self->gapless = DEFAULT_GAPLESS;
  self->output_buffer_size = 0;

  self->adapter = gst_adapter_new ();

  self->stream_align =
      gst_audio_stream_align_new (48000, DEFAULT_ALIGNMENT_THRESHOLD,
      DEFAULT_DISCONT_WAIT);
}

static void
gst_audio_buffer_split_finalize (GObject * object)
{
  GstAudioBufferSplit *self = GST_AUDIO_BUFFER_SPLIT (object);

  if (self->adapter) {
    gst_object_unref (self->adapter);
    self->adapter = NULL;
  }

  if (self->stream_align) {
    gst_audio_stream_align_free (self->stream_align);
    self->stream_align = NULL;
  }

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static gboolean
gst_audio_buffer_split_update_samples_per_buffer (GstAudioBufferSplit * self)
{
  gboolean ret = TRUE;

  GST_OBJECT_LOCK (self);

  /* For a later time */
  if (!self->info.finfo
      || GST_AUDIO_INFO_FORMAT (&self->info) == GST_AUDIO_FORMAT_UNKNOWN) {
    self->samples_per_buffer = 0;
    goto out;
  }

  if (self->output_buffer_size) {
    self->output_buffer_duration_n =
        self->output_buffer_size / GST_AUDIO_INFO_BPF (&self->info);
    self->output_buffer_duration_d = GST_AUDIO_INFO_RATE (&self->info);
  }

  self->samples_per_buffer =
      (((guint64) GST_AUDIO_INFO_RATE (&self->info)) *
      self->output_buffer_duration_n) / self->output_buffer_duration_d;
  if (self->samples_per_buffer == 0) {
    ret = FALSE;
    goto out;
  }

  self->error_per_buffer =
      (((guint64) GST_AUDIO_INFO_RATE (&self->info)) *
      self->output_buffer_duration_n) % self->output_buffer_duration_d;
  self->accumulated_error = 0;

  GST_DEBUG_OBJECT (self, "Buffer duration: %u/%u",
      self->output_buffer_duration_n, self->output_buffer_duration_d);
  GST_DEBUG_OBJECT (self, "Samples per buffer: %u (error: %u/%u)",
      self->samples_per_buffer, self->error_per_buffer,
      self->output_buffer_duration_d);
out:
  GST_OBJECT_UNLOCK (self);

  return ret;
}

static void
gst_audio_buffer_split_set_property (GObject * object, guint property_id,
    const GValue * value, GParamSpec * pspec)
{
  GstAudioBufferSplit *self = GST_AUDIO_BUFFER_SPLIT (object);

  switch (property_id) {
    case PROP_OUTPUT_BUFFER_DURATION:
      self->output_buffer_duration_n = gst_value_get_fraction_numerator (value);
      self->output_buffer_duration_d =
          gst_value_get_fraction_denominator (value);
      gst_audio_buffer_split_update_samples_per_buffer (self);
      break;
    case PROP_OUTPUT_BUFFER_SIZE:
      self->output_buffer_size = g_value_get_uint (value);
      gst_audio_buffer_split_update_samples_per_buffer (self);
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
    case PROP_STRICT_BUFFER_SIZE:
      self->strict_buffer_size = g_value_get_boolean (value);
      break;
    case PROP_GAPLESS:
      self->gapless = g_value_get_boolean (value);
      break;
    case PROP_MAX_SILENCE_TIME:
      self->max_silence_time = g_value_get_uint64 (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

static void
gst_audio_buffer_split_get_property (GObject * object, guint property_id,
    GValue * value, GParamSpec * pspec)
{
  GstAudioBufferSplit *self = GST_AUDIO_BUFFER_SPLIT (object);

  switch (property_id) {
    case PROP_OUTPUT_BUFFER_DURATION:
      gst_value_set_fraction (value, self->output_buffer_duration_n,
          self->output_buffer_duration_d);
      break;
    case PROP_OUTPUT_BUFFER_SIZE:
      g_value_set_uint (value, self->output_buffer_size);
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
    case PROP_STRICT_BUFFER_SIZE:
      g_value_set_boolean (value, self->strict_buffer_size);
      break;
    case PROP_GAPLESS:
      g_value_set_boolean (value, self->gapless);
      break;
    case PROP_MAX_SILENCE_TIME:
      g_value_set_uint64 (value, self->max_silence_time);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

static GstStateChangeReturn
gst_audio_buffer_split_change_state (GstElement * element,
    GstStateChange transition)
{
  GstAudioBufferSplit *self = GST_AUDIO_BUFFER_SPLIT (element);
  GstStateChangeReturn state_ret;

  switch (transition) {
    case GST_STATE_CHANGE_READY_TO_PAUSED:
      gst_audio_info_init (&self->info);
      gst_segment_init (&self->in_segment, GST_FORMAT_TIME);
      gst_segment_init (&self->out_segment, GST_FORMAT_UNDEFINED);
      self->segment_pending = FALSE;
      GST_OBJECT_LOCK (self);
      gst_audio_stream_align_mark_discont (self->stream_align);
      GST_OBJECT_UNLOCK (self);
      self->current_offset = -1;
      self->accumulated_error = 0;
      self->samples_per_buffer = 0;
      break;
    default:
      break;
  }

  state_ret =
      GST_ELEMENT_CLASS (gst_audio_buffer_split_parent_class)->change_state
      (element, transition);
  if (state_ret == GST_STATE_CHANGE_FAILURE)
    return state_ret;

  switch (transition) {
    case GST_STATE_CHANGE_PAUSED_TO_READY:
      gst_adapter_clear (self->adapter);
      GST_OBJECT_LOCK (self);
      gst_audio_stream_align_mark_discont (self->stream_align);
      GST_OBJECT_UNLOCK (self);
      break;
    default:
      break;
  }

  return state_ret;
}

static GstFlowReturn
gst_audio_buffer_split_output (GstAudioBufferSplit * self, gboolean force,
    gint rate, gint bpf, guint samples_per_buffer)
{
  gint size, avail;
  GstFlowReturn ret = GST_FLOW_OK;
  GstClockTime resync_pts;

  resync_pts = self->resync_pts;
  size = samples_per_buffer * bpf;

  /* If we accumulated enough error for one sample, include one
   * more sample in this buffer. Accumulated error is updated below */
  if (self->error_per_buffer + self->accumulated_error >=
      self->output_buffer_duration_d)
    size += bpf;

  while ((avail = gst_adapter_available (self->adapter)) >= size || (force
          && avail > 0)) {
    GstBuffer *buffer;
    GstClockTime resync_time_diff;

    size = MIN (size, avail);
    buffer = gst_adapter_take_buffer (self->adapter, size);
    buffer = gst_buffer_make_writable (buffer);

    /* After a reset we have to set the discont flag */
    if (self->current_offset == 0)
      GST_BUFFER_FLAG_SET (buffer,
          GST_BUFFER_FLAG_DISCONT | GST_BUFFER_FLAG_RESYNC);
    else
      GST_BUFFER_FLAG_UNSET (buffer,
          GST_BUFFER_FLAG_DISCONT | GST_BUFFER_FLAG_RESYNC);

    resync_time_diff =
        gst_util_uint64_scale (self->current_offset, GST_SECOND, rate);
    if (self->out_segment.rate < 0.0) {
      if (resync_pts > resync_time_diff)
        GST_BUFFER_PTS (buffer) = resync_pts - resync_time_diff;
      else
        GST_BUFFER_PTS (buffer) = 0;
      GST_BUFFER_DURATION (buffer) =
          gst_util_uint64_scale (size / bpf, GST_SECOND, rate);

      self->current_offset += size / bpf;
    } else {
      GST_BUFFER_PTS (buffer) = resync_pts + resync_time_diff;
      self->current_offset += size / bpf;
      resync_time_diff =
          gst_util_uint64_scale (self->current_offset, GST_SECOND, rate);
      GST_BUFFER_DURATION (buffer) =
          resync_time_diff - (GST_BUFFER_PTS (buffer) - resync_pts);
    }

    GST_BUFFER_OFFSET (buffer) = GST_BUFFER_OFFSET_NONE;
    GST_BUFFER_OFFSET_END (buffer) = GST_BUFFER_OFFSET_NONE;

    self->accumulated_error =
        (self->accumulated_error +
        self->error_per_buffer) % self->output_buffer_duration_d;

    GST_LOG_OBJECT (self,
        "Outputting buffer at running time %" GST_TIME_FORMAT
        " with timestamp %" GST_TIME_FORMAT " with duration %" GST_TIME_FORMAT
        " (%u samples)",
        GST_TIME_ARGS (gst_segment_to_running_time (&self->out_segment,
                GST_FORMAT_TIME, GST_BUFFER_PTS (buffer))),
        GST_TIME_ARGS (GST_BUFFER_PTS (buffer)),
        GST_TIME_ARGS (GST_BUFFER_DURATION (buffer)), size / bpf);

    ret = gst_pad_push (self->srcpad, buffer);
    if (ret != GST_FLOW_OK)
      break;

    /* Update the size based on the accumulated error we have now after
     * taking out a buffer. Same code as above */
    size = samples_per_buffer * bpf;
    if (self->error_per_buffer + self->accumulated_error >=
        self->output_buffer_duration_d)
      size += bpf;
  }

  return ret;
}

static GstFlowReturn
gst_audio_buffer_split_handle_discont (GstAudioBufferSplit * self,
    GstBuffer * buffer, GstAudioFormat format, gint rate, gint bpf,
    guint samples_per_buffer)
{
  gboolean discont;
  GstFlowReturn ret = GST_FLOW_OK;
  guint avail = gst_adapter_available (self->adapter);
  guint avail_samples = avail / bpf;
  guint64 new_offset;
  GstClockTime input_rt, current_rt;
  GstClockTime input_duration;
  GstClockTime current_rt_end;

  input_rt =
      gst_segment_to_running_time (&self->in_segment, GST_FORMAT_TIME,
      GST_BUFFER_PTS (buffer));
  input_duration =
      (gst_buffer_get_size (buffer) / bpf) / ABS (self->in_segment.rate);

  GST_OBJECT_LOCK (self);

  if (self->in_segment.rate < 0) {
    discont = FALSE;
  } else {
    discont = GST_BUFFER_IS_DISCONT (buffer)
        || GST_BUFFER_FLAG_IS_SET (buffer, GST_BUFFER_FLAG_RESYNC);
  }

  /* If the segment rate is changing this is a discontinuity */
  discont = discont || (self->out_segment.format != GST_FORMAT_UNDEFINED
      && self->in_segment.rate != self->out_segment.rate);

  /* If this is the very first buffer we also have a discontinuity */
  discont = discont || self->current_offset == -1;

  discont =
      gst_audio_stream_align_process (self->stream_align,
      discont, input_rt, input_duration, NULL, NULL, NULL);
  GST_OBJECT_UNLOCK (self);

  if (!discont)
    return ret;

  /* Reset */
  self->drop_samples = 0;

  if (self->in_segment.rate < 0.0) {
    current_rt =
        self->resync_rt - gst_util_uint64_scale (self->current_offset +
        avail_samples, GST_SECOND, rate * ABS (self->in_segment.rate));
    current_rt_end =
        self->resync_rt - gst_util_uint64_scale (self->current_offset,
        GST_SECOND, rate * ABS (self->in_segment.rate));
  } else {
    current_rt =
        self->resync_rt + gst_util_uint64_scale (self->current_offset,
        GST_SECOND, rate * self->in_segment.rate);
    current_rt_end =
        self->resync_rt + gst_util_uint64_scale (self->current_offset +
        avail_samples, GST_SECOND, rate * self->in_segment.rate);
  }

  if (self->gapless && self->current_offset != -1) {
    GST_DEBUG_OBJECT (self,
        "Got discont in gapless mode: Current running time %" GST_TIME_FORMAT
        ", current end running time %" GST_TIME_FORMAT
        ", running time after discont %" GST_TIME_FORMAT,
        GST_TIME_ARGS (current_rt),
        GST_TIME_ARGS (current_rt_end), GST_TIME_ARGS (input_rt));

    new_offset =
        gst_util_uint64_scale (input_rt - self->resync_rt,
        rate * ABS (self->in_segment.rate), GST_SECOND);
    if (input_rt < self->resync_rt) {
      guint64 drop_samples;

      new_offset =
          gst_util_uint64_scale (self->resync_rt -
          input_rt, rate * ABS (self->in_segment.rate), GST_SECOND);
      drop_samples = self->current_offset + avail_samples + new_offset;

      GST_DEBUG_OBJECT (self,
          "Dropping %" G_GUINT64_FORMAT " samples (%" GST_TIME_FORMAT ")",
          drop_samples, GST_TIME_ARGS (gst_util_uint64_scale (drop_samples,
                  GST_SECOND, rate)));
      self->drop_samples = drop_samples;
      discont = FALSE;
    } else if (new_offset > self->current_offset + avail_samples) {
      guint64 silence_samples =
          new_offset - (self->current_offset + avail_samples);
      const GstAudioFormatInfo *info = gst_audio_format_get_info (format);
      GstClockTime silence_time =
          gst_util_uint64_scale (silence_samples, GST_SECOND, rate);

      if (silence_time > self->max_silence_time && self->max_silence_time != 0) {
        GST_DEBUG_OBJECT (self,
            "Not inserting %" G_GUINT64_FORMAT " samples of silence (%"
            GST_TIME_FORMAT " exceeds maximum %" GST_TIME_FORMAT ")",
            silence_samples, GST_TIME_ARGS (silence_time),
            GST_TIME_ARGS (self->max_silence_time));
      } else {
        GST_DEBUG_OBJECT (self,
            "Inserting %" G_GUINT64_FORMAT " samples of silence (%"
            GST_TIME_FORMAT ")", silence_samples, GST_TIME_ARGS (silence_time));

        /* Insert silence buffers to fill the gap in 1s chunks */
        while (silence_samples > 0) {
          guint n_samples = MIN (silence_samples, rate);
          GstBuffer *silence;
          GstMapInfo map;

          silence = gst_buffer_new_and_alloc (n_samples * bpf);
          GST_BUFFER_FLAG_SET (silence, GST_BUFFER_FLAG_GAP);
          gst_buffer_map (silence, &map, GST_MAP_WRITE);
          gst_audio_format_info_fill_silence (info, map.data, map.size);
          gst_buffer_unmap (silence, &map);

          gst_adapter_push (self->adapter, silence);
          ret =
              gst_audio_buffer_split_output (self, FALSE, rate, bpf,
              samples_per_buffer);
          if (ret != GST_FLOW_OK)
            return ret;

          silence_samples -= n_samples;
        }
        discont = FALSE;
      }
    } else if (new_offset < self->current_offset + avail_samples) {
      guint64 drop_samples = self->current_offset + avail_samples - new_offset;

      GST_DEBUG_OBJECT (self,
          "Dropping %" G_GUINT64_FORMAT " samples (%" GST_TIME_FORMAT ")",
          drop_samples, GST_TIME_ARGS (gst_util_uint64_scale (drop_samples,
                  GST_SECOND, rate)));
      self->drop_samples = drop_samples;
      discont = FALSE;
    }
  }

  if (discont) {
    /* We might end up in here also in gapless mode, if the above code decided
     * that no silence is to be inserted, because e.g. the gap is too big */
    GST_DEBUG_OBJECT (self,
        "Got %s: Current running time %" GST_TIME_FORMAT
        ", current end running time %" GST_TIME_FORMAT
        ", running time after discont %" GST_TIME_FORMAT,
        self->current_offset == -1 ? "first buffer" : "discont",
        GST_TIME_ARGS (current_rt),
        GST_TIME_ARGS (current_rt_end), GST_TIME_ARGS (input_rt));

    if (self->strict_buffer_size) {
      gst_adapter_clear (self->adapter);
      ret = GST_FLOW_OK;
    } else {
      ret =
          gst_audio_buffer_split_output (self, TRUE, rate, bpf,
          samples_per_buffer);
    }

    self->current_offset = 0;
    self->accumulated_error = 0;
    self->resync_pts = GST_BUFFER_PTS (buffer);
    self->resync_rt = input_rt;

    if (self->segment_pending) {
      GstEvent *event;

      self->out_segment = self->in_segment;
      GST_DEBUG_OBJECT (self, "Updating output segment %" GST_SEGMENT_FORMAT,
          &self->out_segment);
      event = gst_event_new_segment (&self->out_segment);
      gst_event_set_seqnum (event, self->segment_seqnum);
      gst_pad_push_event (self->srcpad, event);
      self->segment_pending = FALSE;
    }
  }

  return ret;
}

static GstBuffer *
gst_audio_buffer_split_clip_buffer_start_for_gapless (GstAudioBufferSplit *
    self, GstBuffer * buffer, gint rate, gint bpf)
{
  guint nsamples;

  if (!self->gapless || self->drop_samples == 0)
    return buffer;

  nsamples = gst_buffer_get_size (buffer) / bpf;

  GST_DEBUG_OBJECT (self, "Have to drop %" G_GUINT64_FORMAT
      " samples, got %u samples", self->drop_samples, nsamples);

  if (nsamples <= self->drop_samples) {
    gst_buffer_unref (buffer);
    self->drop_samples -= nsamples;
    return NULL;
  }

  if (self->out_segment.rate < 0.0) {
    buffer =
        gst_audio_buffer_truncate (buffer, bpf, 0,
        nsamples - self->drop_samples);
    self->drop_samples = 0;
    return buffer;
  } else {
    buffer = gst_audio_buffer_truncate (buffer, bpf, self->drop_samples, -1);
    self->drop_samples = 0;
    return buffer;
  }

  return buffer;
}

static GstFlowReturn
gst_audio_buffer_split_sink_chain (GstPad * pad, GstObject * parent,
    GstBuffer * buffer)
{
  GstAudioBufferSplit *self = GST_AUDIO_BUFFER_SPLIT (parent);
  GstFlowReturn ret;
  GstAudioFormat format;
  gint rate, bpf, samples_per_buffer;

  GST_OBJECT_LOCK (self);
  format =
      self->info.
      finfo ? GST_AUDIO_INFO_FORMAT (&self->info) : GST_AUDIO_FORMAT_UNKNOWN;
  rate = GST_AUDIO_INFO_RATE (&self->info);
  bpf = GST_AUDIO_INFO_BPF (&self->info);
  samples_per_buffer = self->samples_per_buffer;
  GST_OBJECT_UNLOCK (self);

  GST_LOG_OBJECT (self,
      "Processing buffer at running time %" GST_TIME_FORMAT
      " with timestamp %" GST_TIME_FORMAT " with duration %" GST_TIME_FORMAT
      " (%u samples)",
      GST_TIME_ARGS (gst_segment_to_running_time (&self->in_segment,
              GST_FORMAT_TIME, GST_BUFFER_PTS (buffer))),
      GST_TIME_ARGS (GST_BUFFER_PTS (buffer)),
      GST_TIME_ARGS (GST_BUFFER_DURATION (buffer)),
      (guint) (gst_buffer_get_size (buffer) / bpf));

  if (format == GST_AUDIO_FORMAT_UNKNOWN || samples_per_buffer == 0) {
    gst_buffer_unref (buffer);
    return GST_FLOW_NOT_NEGOTIATED;
  }

  buffer = gst_audio_buffer_clip (buffer, &self->in_segment, rate, bpf);
  if (!buffer)
    return GST_FLOW_OK;

  ret =
      gst_audio_buffer_split_handle_discont (self, buffer, format, rate, bpf,
      samples_per_buffer);
  if (ret != GST_FLOW_OK) {
    gst_buffer_unref (buffer);
    return ret;
  }

  buffer =
      gst_audio_buffer_split_clip_buffer_start_for_gapless (self, buffer, rate,
      bpf);
  if (!buffer)
    return GST_FLOW_OK;

  gst_adapter_push (self->adapter, buffer);

  return gst_audio_buffer_split_output (self, FALSE, rate, bpf,
      samples_per_buffer);
}

static gboolean
gst_audio_buffer_split_sink_event (GstPad * pad, GstObject * parent,
    GstEvent * event)
{
  GstAudioBufferSplit *self = GST_AUDIO_BUFFER_SPLIT (parent);
  gboolean ret = FALSE;

  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_CAPS:{
      GstCaps *caps;
      GstAudioInfo info;

      gst_event_parse_caps (event, &caps);

      ret = gst_audio_info_from_caps (&info, caps);
      if (ret) {
        GST_DEBUG_OBJECT (self, "Got caps %" GST_PTR_FORMAT, caps);

        if (!gst_audio_info_is_equal (&info, &self->info)) {
          if (self->strict_buffer_size) {
            gst_adapter_clear (self->adapter);
          } else {
            GstAudioFormat format;
            gint rate, bpf, samples_per_buffer;

            GST_OBJECT_LOCK (self);
            format =
                self->info.finfo ? GST_AUDIO_INFO_FORMAT (&self->info) :
                GST_AUDIO_FORMAT_UNKNOWN;
            rate = GST_AUDIO_INFO_RATE (&self->info);
            bpf = GST_AUDIO_INFO_BPF (&self->info);
            samples_per_buffer = self->samples_per_buffer;
            GST_OBJECT_UNLOCK (self);

            if (format != GST_AUDIO_FORMAT_UNKNOWN && samples_per_buffer != 0)
              gst_audio_buffer_split_output (self, TRUE, rate, bpf,
                  samples_per_buffer);
          }
        }
        self->info = info;
        GST_OBJECT_LOCK (self);
        gst_audio_stream_align_set_rate (self->stream_align, self->info.rate);
        GST_OBJECT_UNLOCK (self);
        ret = gst_audio_buffer_split_update_samples_per_buffer (self);
      } else {
        ret = FALSE;
      }

      if (ret)
        ret = gst_pad_event_default (pad, parent, event);
      else
        gst_event_unref (event);

      break;
    }
    case GST_EVENT_FLUSH_STOP:
      gst_segment_init (&self->in_segment, GST_FORMAT_TIME);
      gst_segment_init (&self->out_segment, GST_FORMAT_UNDEFINED);
      self->segment_pending = FALSE;
      GST_OBJECT_LOCK (self);
      gst_audio_stream_align_mark_discont (self->stream_align);
      GST_OBJECT_UNLOCK (self);
      self->current_offset = -1;
      self->accumulated_error = 0;
      gst_adapter_clear (self->adapter);
      ret = gst_pad_event_default (pad, parent, event);
      break;
    case GST_EVENT_SEGMENT:
      gst_event_copy_segment (event, &self->in_segment);
      if (self->in_segment.format != GST_FORMAT_TIME) {
        gst_event_unref (event);
        ret = FALSE;
      } else {
        GST_DEBUG_OBJECT (self,
            "Received new input segment %" GST_SEGMENT_FORMAT,
            &self->in_segment);
        self->segment_pending = TRUE;
        self->segment_seqnum = gst_event_get_seqnum (event);
        gst_event_unref (event);
        ret = TRUE;
      }
      break;
    case GST_EVENT_EOS:
      if (self->strict_buffer_size) {
        gst_adapter_clear (self->adapter);
      } else {
        GstAudioFormat format;
        gint rate, bpf, samples_per_buffer;

        GST_OBJECT_LOCK (self);
        format =
            self->info.finfo ? GST_AUDIO_INFO_FORMAT (&self->info) :
            GST_AUDIO_FORMAT_UNKNOWN;
        rate = GST_AUDIO_INFO_RATE (&self->info);
        bpf = GST_AUDIO_INFO_BPF (&self->info);
        samples_per_buffer = self->samples_per_buffer;
        GST_OBJECT_UNLOCK (self);

        if (format != GST_AUDIO_FORMAT_UNKNOWN && samples_per_buffer != 0)
          gst_audio_buffer_split_output (self, TRUE, rate, bpf,
              samples_per_buffer);
      }
      ret = gst_pad_event_default (pad, parent, event);
      break;
    default:
      ret = gst_pad_event_default (pad, parent, event);
      break;
  }

  return ret;
}

static gboolean
gst_audio_buffer_split_src_query (GstPad * pad,
    GstObject * parent, GstQuery * query)
{
  GstAudioBufferSplit *self = GST_AUDIO_BUFFER_SPLIT (parent);
  gboolean ret = FALSE;

  switch (GST_QUERY_TYPE (query)) {
    case GST_QUERY_LATENCY:{
      if ((ret = gst_pad_peer_query (self->sinkpad, query))) {
        GstClockTime latency;
        GstClockTime min, max;
        gboolean live;

        gst_query_parse_latency (query, &live, &min, &max);

        GST_DEBUG_OBJECT (self, "Peer latency: min %"
            GST_TIME_FORMAT " max %" GST_TIME_FORMAT,
            GST_TIME_ARGS (min), GST_TIME_ARGS (max));

        latency =
            gst_util_uint64_scale (GST_SECOND, self->output_buffer_duration_n,
            self->output_buffer_duration_d);

        GST_DEBUG_OBJECT (self, "Our latency: min %" GST_TIME_FORMAT
            ", max %" GST_TIME_FORMAT,
            GST_TIME_ARGS (latency), GST_TIME_ARGS (latency));

        min += latency;
        if (max != GST_CLOCK_TIME_NONE)
          max += latency;

        GST_DEBUG_OBJECT (self, "Calculated total latency : min %"
            GST_TIME_FORMAT " max %" GST_TIME_FORMAT,
            GST_TIME_ARGS (min), GST_TIME_ARGS (max));

        gst_query_set_latency (query, live, min, max);
      }

      break;
    }
    default:
      ret = gst_pad_query_default (pad, parent, query);
      break;
  }

  return ret;
}

static gboolean
plugin_init (GstPlugin * plugin)
{
  return GST_ELEMENT_REGISTER (audiobuffersplit, plugin);
}

GST_PLUGIN_DEFINE (GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    audiobuffersplit,
    "Audio buffer splitter",
    plugin_init, VERSION, "LGPL", PACKAGE_NAME, GST_PACKAGE_ORIGIN)
