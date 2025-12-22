/*
 * GStreamer
 * Copyright (C) 2015 Vivia Nikolaidou <vivia@toolsonair.com>
 *
 * Based on gstlevel.c:
 * Copyright (C) 2000,2001,2002,2003,2005
 *           Thomas Vander Stichele <thomas at apestaart dot org>
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
 * SECTION:element-videoframe-audiolevel
 * @title: videoframe-audiolevel
 *
 * This element acts like a synchronized audio/video "level". It gathers
 * all audio buffers sent between two video frames, and then sends a message
 * that contains the RMS value of all samples for these buffers.
 *
 * ## Example launch line
 * |[
 * gst-launch-1.0 -m filesrc location="file.mkv" ! decodebin name=d ! "audio/x-raw" ! videoframe-audiolevel name=l ! autoaudiosink d. ! "video/x-raw" ! l. l. ! queue ! autovideosink ]|
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

/* FIXME 2.0: suppress warnings for deprecated API such as GValueArray
 * with newer GLib versions (>= 2.31.0) */
#define GLIB_DISABLE_DEPRECATION_WARNINGS

#include "gstvideoframe-audiolevel.h"
#include <math.h>

#define GST_CAT_DEFAULT gst_videoframe_audiolevel_debug
#if G_BYTE_ORDER == G_LITTLE_ENDIAN
# define FORMATS "{ S8, S16LE, S32LE, F32LE, F64LE }"
#else
# define FORMATS "{ S8, S16BE, S32BE, F32BE, F64BE }"
#endif
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);

static GstStaticPadTemplate audio_sink_template =
GST_STATIC_PAD_TEMPLATE ("asink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (GST_AUDIO_CAPS_MAKE (FORMATS))
    );

static GstStaticPadTemplate audio_src_template =
GST_STATIC_PAD_TEMPLATE ("asrc",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (GST_AUDIO_CAPS_MAKE (FORMATS))
    );

static GstStaticPadTemplate video_sink_template =
GST_STATIC_PAD_TEMPLATE ("vsink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("video/x-raw")
    );

static GstStaticPadTemplate video_src_template =
GST_STATIC_PAD_TEMPLATE ("vsrc",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("video/x-raw")
    );

#define parent_class gst_videoframe_audiolevel_parent_class
G_DEFINE_TYPE (GstVideoFrameAudioLevel, gst_videoframe_audiolevel,
    GST_TYPE_ELEMENT);
GST_ELEMENT_REGISTER_DEFINE (videoframe_audiolevel, "videoframe-audiolevel",
    GST_RANK_NONE, GST_TYPE_VIDEOFRAME_AUDIOLEVEL);

static GstFlowReturn gst_videoframe_audiolevel_asink_chain (GstPad * pad,
    GstObject * parent, GstBuffer * inbuf);
static GstFlowReturn gst_videoframe_audiolevel_vsink_chain (GstPad * pad,
    GstObject * parent, GstBuffer * inbuf);
static gboolean gst_videoframe_audiolevel_asink_event (GstPad * pad,
    GstObject * parent, GstEvent * event);
static gboolean gst_videoframe_audiolevel_vsink_event (GstPad * pad,
    GstObject * parent, GstEvent * event);
static GstIterator *gst_videoframe_audiolevel_iterate_internal_links (GstPad *
    pad, GstObject * parent);

static void gst_videoframe_audiolevel_finalize (GObject * gobject);

static GstStateChangeReturn gst_videoframe_audiolevel_change_state (GstElement *
    element, GstStateChange transition);

static void
gst_videoframe_audiolevel_class_init (GstVideoFrameAudioLevelClass * klass)
{
  GstElementClass *gstelement_class;
  GObjectClass *gobject_class = (GObjectClass *) klass;

  GST_DEBUG_CATEGORY_INIT (gst_videoframe_audiolevel_debug,
      "videoframe-audiolevel", 0, "Synchronized audio/video level");

  gstelement_class = (GstElementClass *) klass;

  gst_element_class_set_static_metadata (gstelement_class,
      "Video-frame audio level", "Filter/Analyzer/Audio",
      "Synchronized audio/video RMS Level messenger for audio/raw",
      "Vivia Nikolaidou <vivia@toolsonair.com>");

  gobject_class->finalize = gst_videoframe_audiolevel_finalize;
  gstelement_class->change_state = gst_videoframe_audiolevel_change_state;

  gst_element_class_add_static_pad_template (gstelement_class,
      &audio_src_template);
  gst_element_class_add_static_pad_template (gstelement_class,
      &audio_sink_template);

  gst_element_class_add_static_pad_template (gstelement_class,
      &video_src_template);
  gst_element_class_add_static_pad_template (gstelement_class,
      &video_sink_template);
}

static void
gst_videoframe_audiolevel_init (GstVideoFrameAudioLevel * self)
{
  self->asinkpad =
      gst_pad_new_from_static_template (&audio_sink_template, "asink");
  gst_pad_set_chain_function (self->asinkpad,
      GST_DEBUG_FUNCPTR (gst_videoframe_audiolevel_asink_chain));
  gst_pad_set_event_function (self->asinkpad,
      GST_DEBUG_FUNCPTR (gst_videoframe_audiolevel_asink_event));
  gst_pad_set_iterate_internal_links_function (self->asinkpad,
      GST_DEBUG_FUNCPTR (gst_videoframe_audiolevel_iterate_internal_links));
  gst_element_add_pad (GST_ELEMENT (self), self->asinkpad);

  self->vsinkpad =
      gst_pad_new_from_static_template (&video_sink_template, "vsink");
  gst_pad_set_chain_function (self->vsinkpad,
      GST_DEBUG_FUNCPTR (gst_videoframe_audiolevel_vsink_chain));
  gst_pad_set_event_function (self->vsinkpad,
      GST_DEBUG_FUNCPTR (gst_videoframe_audiolevel_vsink_event));
  gst_pad_set_iterate_internal_links_function (self->vsinkpad,
      GST_DEBUG_FUNCPTR (gst_videoframe_audiolevel_iterate_internal_links));
  gst_element_add_pad (GST_ELEMENT (self), self->vsinkpad);

  self->asrcpad =
      gst_pad_new_from_static_template (&audio_src_template, "asrc");
  gst_pad_set_iterate_internal_links_function (self->asrcpad,
      GST_DEBUG_FUNCPTR (gst_videoframe_audiolevel_iterate_internal_links));
  gst_element_add_pad (GST_ELEMENT (self), self->asrcpad);

  self->vsrcpad =
      gst_pad_new_from_static_template (&video_src_template, "vsrc");
  gst_pad_set_iterate_internal_links_function (self->vsrcpad,
      GST_DEBUG_FUNCPTR (gst_videoframe_audiolevel_iterate_internal_links));
  gst_element_add_pad (GST_ELEMENT (self), self->vsrcpad);

  GST_PAD_SET_PROXY_CAPS (self->asinkpad);
  GST_PAD_SET_PROXY_ALLOCATION (self->asinkpad);

  GST_PAD_SET_PROXY_CAPS (self->asrcpad);
  GST_PAD_SET_PROXY_SCHEDULING (self->asrcpad);

  GST_PAD_SET_PROXY_CAPS (self->vsinkpad);
  GST_PAD_SET_PROXY_ALLOCATION (self->vsinkpad);

  GST_PAD_SET_PROXY_CAPS (self->vsrcpad);
  GST_PAD_SET_PROXY_SCHEDULING (self->vsrcpad);

  self->adapter = gst_adapter_new ();

  g_queue_init (&self->vtimeq);
  self->first_time = GST_CLOCK_TIME_NONE;
  self->total_frames = 0;
  /* alignment_threshold and discont_wait should become properties if needed */
  self->alignment_threshold = 40 * GST_MSECOND;
  self->discont_time = GST_CLOCK_TIME_NONE;
  self->next_offset = -1;
  self->discont_wait = 1 * GST_SECOND;

  self->video_eos_flag = FALSE;
  self->audio_flush_flag = FALSE;
  self->shutdown_flag = FALSE;

  g_mutex_init (&self->mutex);
  g_cond_init (&self->cond);
}

static GstStateChangeReturn
gst_videoframe_audiolevel_change_state (GstElement * element,
    GstStateChange transition)
{
  GstStateChangeReturn ret;
  GstVideoFrameAudioLevel *self = GST_VIDEOFRAME_AUDIOLEVEL (element);

  switch (transition) {
    case GST_STATE_CHANGE_PAUSED_TO_READY:
      g_mutex_lock (&self->mutex);
      self->shutdown_flag = TRUE;
      g_cond_signal (&self->cond);
      g_mutex_unlock (&self->mutex);
      break;
    case GST_STATE_CHANGE_READY_TO_PAUSED:
      g_mutex_lock (&self->mutex);
      self->shutdown_flag = FALSE;
      self->video_eos_flag = FALSE;
      self->audio_flush_flag = FALSE;
      g_mutex_unlock (&self->mutex);
    default:
      break;
  }

  ret = GST_ELEMENT_CLASS (parent_class)->change_state (element, transition);

  switch (transition) {
    case GST_STATE_CHANGE_PAUSED_TO_READY:
      g_mutex_lock (&self->mutex);
      self->first_time = GST_CLOCK_TIME_NONE;
      self->total_frames = 0;
      gst_segment_init (&self->asegment, GST_FORMAT_UNDEFINED);
      gst_segment_init (&self->vsegment, GST_FORMAT_UNDEFINED);
      self->vsegment.position = GST_CLOCK_TIME_NONE;
      gst_adapter_clear (self->adapter);
      g_queue_foreach (&self->vtimeq, (GFunc) g_free, NULL);
      g_queue_clear (&self->vtimeq);
      if (self->CS) {
        g_free (self->CS);
        self->CS = NULL;
      }
      g_mutex_unlock (&self->mutex);
      break;
    default:
      break;
  }

  return ret;
}

static void
gst_videoframe_audiolevel_finalize (GObject * object)
{
  GstVideoFrameAudioLevel *self = GST_VIDEOFRAME_AUDIOLEVEL (object);

  if (self->adapter) {
    g_object_unref (self->adapter);
    self->adapter = NULL;
  }
  g_queue_foreach (&self->vtimeq, (GFunc) g_free, NULL);
  g_queue_clear (&self->vtimeq);
  self->first_time = GST_CLOCK_TIME_NONE;
  self->total_frames = 0;
  if (self->CS) {
    g_free (self->CS);
    self->CS = NULL;
  }

  g_mutex_clear (&self->mutex);
  g_cond_clear (&self->cond);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

#define DEFINE_INT_LEVEL_CALCULATOR(TYPE, RESOLUTION)                         \
static void inline                                                            \
gst_videoframe_audiolevel_calculate_##TYPE (gpointer data, guint num, guint channels,        \
                            gdouble *NCS)                                     \
{                                                                             \
  TYPE * in = (TYPE *)data;                                                   \
  register guint j;                                                           \
  gdouble squaresum = 0.0;           /* square sum of the input samples */    \
  register gdouble square = 0.0;     /* Square */                             \
  gdouble normalizer;                /* divisor to get a [-1.0, 1.0] range */ \
                                                                              \
  /* *NCS = 0.0; Normalized Cumulative Square */                              \
                                                                              \
  for (j = 0; j < num; j += channels) {                                       \
    square = ((gdouble) in[j]) * in[j];                                       \
    squaresum += square;                                                      \
  }                                                                           \
                                                                              \
  normalizer = (gdouble) (G_GINT64_CONSTANT(1) << (RESOLUTION * 2));          \
  *NCS = squaresum / normalizer;                                              \
}

DEFINE_INT_LEVEL_CALCULATOR (gint32, 31);
DEFINE_INT_LEVEL_CALCULATOR (gint16, 15);
DEFINE_INT_LEVEL_CALCULATOR (gint8, 7);

#define DEFINE_FLOAT_LEVEL_CALCULATOR(TYPE)                                   \
static void inline                                                            \
gst_videoframe_audiolevel_calculate_##TYPE (gpointer data, guint num, guint channels,        \
                            gdouble *NCS)                                     \
{                                                                             \
  TYPE * in = (TYPE *)data;                                                   \
  register guint j;                                                           \
  gdouble squaresum = 0.0;           /* square sum of the input samples */    \
  register gdouble square = 0.0;     /* Square */                             \
                                                                              \
  /* *NCS = 0.0; Normalized Cumulative Square */                              \
                                                                              \
  for (j = 0; j < num; j += channels) {                                       \
    square = ((gdouble) in[j]) * in[j];                                       \
    squaresum += square;                                                      \
  }                                                                           \
                                                                              \
  *NCS = squaresum;                                                           \
}

DEFINE_FLOAT_LEVEL_CALCULATOR (gfloat);
DEFINE_FLOAT_LEVEL_CALCULATOR (gdouble);

static gboolean
gst_videoframe_audiolevel_vsink_event (GstPad * pad, GstObject * parent,
    GstEvent * event)
{
  GstVideoFrameAudioLevel *self = GST_VIDEOFRAME_AUDIOLEVEL (parent);
  GST_LOG_OBJECT (pad, "Got %s event", GST_EVENT_TYPE_NAME (event));

  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_SEGMENT:
      g_mutex_lock (&self->mutex);
      g_queue_foreach (&self->vtimeq, (GFunc) g_free, NULL);
      g_queue_clear (&self->vtimeq);
      g_mutex_unlock (&self->mutex);
      gst_event_copy_segment (event, &self->vsegment);
      if (self->vsegment.format != GST_FORMAT_TIME)
        return FALSE;
      self->vsegment.position = GST_CLOCK_TIME_NONE;
      break;
    case GST_EVENT_GAP:
      return TRUE;
    case GST_EVENT_EOS:
      g_mutex_lock (&self->mutex);
      self->video_eos_flag = TRUE;
      g_cond_signal (&self->cond);
      g_mutex_unlock (&self->mutex);
      break;
    case GST_EVENT_FLUSH_STOP:
      g_mutex_lock (&self->mutex);
      g_queue_foreach (&self->vtimeq, (GFunc) g_free, NULL);
      g_queue_clear (&self->vtimeq);
      gst_segment_init (&self->vsegment, GST_FORMAT_UNDEFINED);
      g_cond_signal (&self->cond);
      g_mutex_unlock (&self->mutex);
      self->vsegment.position = GST_CLOCK_TIME_NONE;
      break;
    default:
      break;
  }
  return gst_pad_event_default (pad, parent, event);
}

static gboolean
gst_videoframe_audiolevel_asink_event (GstPad * pad, GstObject * parent,
    GstEvent * event)
{
  GstVideoFrameAudioLevel *self = GST_VIDEOFRAME_AUDIOLEVEL (parent);
  GST_LOG_OBJECT (pad, "Got %s event", GST_EVENT_TYPE_NAME (event));

  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_SEGMENT:
      self->first_time = GST_CLOCK_TIME_NONE;
      self->total_frames = 0;
      gst_adapter_clear (self->adapter);
      gst_event_copy_segment (event, &self->asegment);
      if (self->asegment.format != GST_FORMAT_TIME)
        return FALSE;
      break;
    case GST_EVENT_FLUSH_START:
      g_mutex_lock (&self->mutex);
      self->audio_flush_flag = TRUE;
      g_cond_signal (&self->cond);
      g_mutex_unlock (&self->mutex);
      break;
    case GST_EVENT_FLUSH_STOP:
      self->audio_flush_flag = FALSE;
      self->total_frames = 0;
      self->first_time = GST_CLOCK_TIME_NONE;
      gst_adapter_clear (self->adapter);
      gst_segment_init (&self->asegment, GST_FORMAT_UNDEFINED);
      break;
    case GST_EVENT_CAPS:{
      GstCaps *caps;
      gint channels;
      gst_event_parse_caps (event, &caps);
      GST_DEBUG_OBJECT (self, "Got caps %" GST_PTR_FORMAT, caps);
      if (!gst_audio_info_from_caps (&self->ainfo, caps))
        return FALSE;
      switch (GST_AUDIO_INFO_FORMAT (&self->ainfo)) {
        case GST_AUDIO_FORMAT_S8:
          self->process = gst_videoframe_audiolevel_calculate_gint8;
          break;
        case GST_AUDIO_FORMAT_S16:
          self->process = gst_videoframe_audiolevel_calculate_gint16;
          break;
        case GST_AUDIO_FORMAT_S32:
          self->process = gst_videoframe_audiolevel_calculate_gint32;
          break;
        case GST_AUDIO_FORMAT_F32:
          self->process = gst_videoframe_audiolevel_calculate_gfloat;
          break;
        case GST_AUDIO_FORMAT_F64:
          self->process = gst_videoframe_audiolevel_calculate_gdouble;
          break;
        default:
          self->process = NULL;
          break;
      }
      gst_adapter_clear (self->adapter);
      channels = GST_AUDIO_INFO_CHANNELS (&self->ainfo);
      self->first_time = GST_CLOCK_TIME_NONE;
      self->total_frames = 0;
      if (self->CS)
        g_free (self->CS);
      self->CS = g_new0 (gdouble, channels);
      break;
    }
    default:
      break;
  }

  return gst_pad_event_default (pad, parent, event);
}

static GstMessage *
update_rms_from_buffer (GstVideoFrameAudioLevel * self, GstBuffer * inbuf)
{
  GstMapInfo map;
  guint8 *in_data;
  gsize in_size;
  gdouble CS;
  guint i;
  guint num_frames, frames;
  guint num_int_samples = 0;    /* number of interleaved samples
                                 * ie. total count for all channels combined */
  gint channels, rate, bps;
  GValue v = G_VALUE_INIT;
  GValue va = G_VALUE_INIT;
  GValueArray *a;
  GstStructure *s;
  GstMessage *msg;
  GstClockTime duration, running_time;

  channels = GST_AUDIO_INFO_CHANNELS (&self->ainfo);
  bps = GST_AUDIO_INFO_BPS (&self->ainfo);
  rate = GST_AUDIO_INFO_RATE (&self->ainfo);

  gst_buffer_map (inbuf, &map, GST_MAP_READ);
  in_data = map.data;
  in_size = map.size;

  num_int_samples = in_size / bps;

  GST_LOG_OBJECT (self, "analyzing %u sample frames at ts %" GST_TIME_FORMAT,
      num_int_samples, GST_TIME_ARGS (GST_BUFFER_TIMESTAMP (inbuf)));

  g_return_val_if_fail (num_int_samples % channels == 0, NULL);

  num_frames = num_int_samples / channels;
  frames = num_frames;
  duration = GST_FRAMES_TO_CLOCK_TIME (frames, rate);
  if (num_frames > 0) {
    for (i = 0; i < channels; ++i) {
      self->process (in_data + (bps * i), num_int_samples, channels, &CS);
      GST_LOG_OBJECT (self,
          "[%d]: cumulative squares %lf, over %d samples/%d channels",
          i, CS, num_int_samples, channels);
      self->CS[i] += CS;
    }
    in_data += num_frames * bps;

    self->total_frames += num_frames;
  }
  running_time =
      self->first_time + gst_util_uint64_scale (self->total_frames, GST_SECOND,
      rate);

  a = g_value_array_new (channels);
  s = gst_structure_new ("videoframe-audiolevel", "running-time", G_TYPE_UINT64,
      running_time, "duration", G_TYPE_UINT64, duration, NULL);

  g_value_init (&v, G_TYPE_DOUBLE);
  g_value_init (&va, G_TYPE_VALUE_ARRAY);
  for (i = 0; i < channels; i++) {
    gdouble rms;
    if (frames == 0 || self->CS[i] == 0) {
      rms = 0;                  /* empty buffer */
    } else {
      rms = sqrt (self->CS[i] / frames);
    }
    self->CS[i] = 0.0;
    g_value_set_double (&v, rms);
    g_value_array_append (a, &v);
  }
  g_value_take_boxed (&va, a);
  gst_structure_take_value (s, "rms", &va);
  msg = gst_message_new_element (GST_OBJECT (self), s);

  gst_buffer_unmap (inbuf, &map);

  return msg;
}

static GstFlowReturn
gst_videoframe_audiolevel_vsink_chain (GstPad * pad, GstObject * parent,
    GstBuffer * inbuf)
{
  GstClockTime timestamp;
  GstVideoFrameAudioLevel *self = GST_VIDEOFRAME_AUDIOLEVEL (parent);
  GstClockTime duration;
  GstClockTime *ptrtime = g_new (GstClockTime, 1);

  timestamp = GST_BUFFER_TIMESTAMP (inbuf);
  *ptrtime =
      gst_segment_to_running_time (&self->vsegment, GST_FORMAT_TIME, timestamp);
  g_mutex_lock (&self->mutex);
  self->vsegment.position = timestamp;
  duration = GST_BUFFER_DURATION (inbuf);
  if (duration != GST_CLOCK_TIME_NONE)
    self->vsegment.position += duration;
  g_queue_push_tail (&self->vtimeq, ptrtime);
  g_cond_signal (&self->cond);
  GST_DEBUG_OBJECT (pad, "Pushed a frame");
  g_mutex_unlock (&self->mutex);
  return gst_pad_push (self->vsrcpad, inbuf);
}

static GstFlowReturn
gst_videoframe_audiolevel_asink_chain (GstPad * pad, GstObject * parent,
    GstBuffer * inbuf)
{
  GstClockTime timestamp, cur_time;
  GstVideoFrameAudioLevel *self = GST_VIDEOFRAME_AUDIOLEVEL (parent);
  GstBuffer *buf;
  gsize inbuf_size;
  guint64 start_offset, end_offset;
  GstClockTime running_time;
  gint rate, bpf;
  gboolean discont = FALSE;

  timestamp = GST_BUFFER_TIMESTAMP (inbuf);
  running_time =
      gst_segment_to_running_time (&self->asegment, GST_FORMAT_TIME, timestamp);

  rate = GST_AUDIO_INFO_RATE (&self->ainfo);
  bpf = GST_AUDIO_INFO_BPF (&self->ainfo);
  start_offset = gst_util_uint64_scale (timestamp, rate, GST_SECOND);
  inbuf_size = gst_buffer_get_size (inbuf);
  end_offset = start_offset + inbuf_size / bpf;

  g_mutex_lock (&self->mutex);

  if (GST_BUFFER_IS_DISCONT (inbuf)
      || GST_BUFFER_FLAG_IS_SET (inbuf, GST_BUFFER_FLAG_RESYNC)
      || self->first_time == GST_CLOCK_TIME_NONE) {
    discont = TRUE;
  } else {
    guint64 diff, max_sample_diff;

    /* Check discont, based on audiobasesink */
    if (start_offset <= self->next_offset)
      diff = self->next_offset - start_offset;
    else
      diff = start_offset - self->next_offset;

    max_sample_diff =
        gst_util_uint64_scale_int (self->alignment_threshold, rate, GST_SECOND);

    /* Discont! */
    if (G_UNLIKELY (diff >= max_sample_diff)) {
      if (self->discont_wait > 0) {
        if (self->discont_time == GST_CLOCK_TIME_NONE) {
          self->discont_time = timestamp;
        } else if (timestamp - self->discont_time >= self->discont_wait) {
          discont = TRUE;
          self->discont_time = GST_CLOCK_TIME_NONE;
        }
      } else {
        discont = TRUE;
      }
    } else if (G_UNLIKELY (self->discont_time != GST_CLOCK_TIME_NONE)) {
      /* we have had a discont, but are now back on track! */
      self->discont_time = GST_CLOCK_TIME_NONE;
    }
  }

  if (discont) {
    /* Have discont, need resync */
    if (self->next_offset != -1)
      GST_INFO_OBJECT (pad, "Have discont. Expected %"
          G_GUINT64_FORMAT ", got %" G_GUINT64_FORMAT,
          self->next_offset, start_offset);
    self->total_frames = 0;
    self->first_time = running_time;
    self->next_offset = end_offset;
  } else {
    self->next_offset += inbuf_size / bpf;
  }

  gst_adapter_push (self->adapter, gst_buffer_ref (inbuf));

  GST_DEBUG_OBJECT (self, "Queue length %i",
      g_queue_get_length (&self->vtimeq));

  while (TRUE) {
    GstClockTime *vt0, *vt1;
    GstClockTime vtemp;
    GstMessage *msg;
    gsize bytes, available_bytes;

    vtemp = GST_CLOCK_TIME_NONE;

    while (!(g_queue_get_length (&self->vtimeq) >= 2 || self->video_eos_flag
            || self->audio_flush_flag || self->shutdown_flag))
      g_cond_wait (&self->cond, &self->mutex);

    if (self->audio_flush_flag || self->shutdown_flag) {
      g_mutex_unlock (&self->mutex);
      gst_buffer_unref (inbuf);
      return GST_FLOW_FLUSHING;
    } else if (self->video_eos_flag) {
      GST_DEBUG_OBJECT (self, "Video EOS flag alert");
      /* nothing to do here if queue is empty */
      if (g_queue_get_length (&self->vtimeq) == 0)
        break;

      if (g_queue_get_length (&self->vtimeq) < 2) {
        vtemp = self->vsegment.position;
      } else if (self->vsegment.position == GST_CLOCK_TIME_NONE) {
        /* g_queue_get_length is surely >= 2 at this point
         * so the adapter isn't empty */
        buf =
            gst_adapter_take_buffer (self->adapter,
            gst_adapter_available (self->adapter));
        if (buf != NULL) {
          GstMessage *msg;
          msg = update_rms_from_buffer (self, buf);
          g_mutex_unlock (&self->mutex);
          gst_element_post_message (GST_ELEMENT (self), msg);
          gst_buffer_unref (buf);
          g_mutex_lock (&self->mutex);  /* we unlock again later */
        }
        break;
      }
    } else if (g_queue_get_length (&self->vtimeq) < 2) {
      continue;
    }

    vt0 = g_queue_pop_head (&self->vtimeq);
    if (vtemp == GST_CLOCK_TIME_NONE)
      vt1 = g_queue_peek_head (&self->vtimeq);
    else
      vt1 = &vtemp;

    cur_time =
        self->first_time + gst_util_uint64_scale (self->total_frames,
        GST_SECOND, rate);
    GST_DEBUG_OBJECT (self,
        "Processing: current time is %" GST_TIME_FORMAT,
        GST_TIME_ARGS (cur_time));
    GST_DEBUG_OBJECT (self, "Total frames is %i with a rate of %d",
        self->total_frames, rate);
    GST_DEBUG_OBJECT (self, "Start time is %" GST_TIME_FORMAT,
        GST_TIME_ARGS (self->first_time));
    GST_DEBUG_OBJECT (self, "Time on top is %" GST_TIME_FORMAT,
        GST_TIME_ARGS (*vt0));

    if (cur_time < *vt0) {
      guint num_frames =
          gst_util_uint64_scale (*vt0 - cur_time, rate, GST_SECOND);
      bytes = num_frames * GST_AUDIO_INFO_BPF (&self->ainfo);
      available_bytes = gst_adapter_available (self->adapter);
      if (available_bytes == 0) {
        g_queue_push_head (&self->vtimeq, vt0);
        break;
      }
      if (bytes == 0) {
        cur_time = *vt0;
      } else {
        GST_DEBUG_OBJECT (self,
            "Flushed %" G_GSIZE_FORMAT " out of %" G_GSIZE_FORMAT " bytes",
            bytes, available_bytes);
        gst_adapter_flush (self->adapter, MIN (bytes, available_bytes));
        self->total_frames += num_frames;
        if (available_bytes <= bytes) {
          g_queue_push_head (&self->vtimeq, vt0);
          break;
        }
        cur_time =
            self->first_time + gst_util_uint64_scale (self->total_frames,
            GST_SECOND, rate);
      }
    }
    if (*vt1 > cur_time) {
      bytes =
          GST_AUDIO_INFO_BPF (&self->ainfo) * gst_util_uint64_scale (*vt1 -
          cur_time, rate, GST_SECOND);
    } else {
      bytes = 0;                /* We just need to discard vt0 */
    }
    available_bytes = gst_adapter_available (self->adapter);
    GST_DEBUG_OBJECT (self,
        "Adapter contains %" G_GSIZE_FORMAT " out of %" G_GSIZE_FORMAT " bytes",
        available_bytes, bytes);

    if (available_bytes < bytes) {
      g_queue_push_head (&self->vtimeq, vt0);
      goto done;
    }

    if (bytes > 0) {
      buf = gst_adapter_take_buffer (self->adapter, bytes);
      g_assert (buf != NULL);
    } else {
      /* Just an empty buffer */
      buf = gst_buffer_new ();
    }
    msg = update_rms_from_buffer (self, buf);
    g_mutex_unlock (&self->mutex);
    gst_element_post_message (GST_ELEMENT (self), msg);
    g_mutex_lock (&self->mutex);

    gst_buffer_unref (buf);
    g_free (vt0);
    if (available_bytes == bytes)
      break;
  }
done:
  g_mutex_unlock (&self->mutex);
  return gst_pad_push (self->asrcpad, inbuf);
}

static GstIterator *
gst_videoframe_audiolevel_iterate_internal_links (GstPad * pad,
    GstObject * parent)
{
  GstIterator *it = NULL;
  GstPad *opad;
  GValue val = { 0, };
  GstVideoFrameAudioLevel *self = GST_VIDEOFRAME_AUDIOLEVEL (parent);

  if (self->asinkpad == pad)
    opad = gst_object_ref (self->asrcpad);
  else if (self->asrcpad == pad)
    opad = gst_object_ref (self->asinkpad);
  else if (self->vsinkpad == pad)
    opad = gst_object_ref (self->vsrcpad);
  else if (self->vsrcpad == pad)
    opad = gst_object_ref (self->vsinkpad);
  else
    goto out;

  g_value_init (&val, GST_TYPE_PAD);
  g_value_set_object (&val, opad);
  it = gst_iterator_new_single (GST_TYPE_PAD, &val);
  g_value_unset (&val);

  gst_object_unref (opad);

out:
  return it;
}

static gboolean
gst_videoframe_audiolevel_plugin_init (GstPlugin * plugin)
{
  return GST_ELEMENT_REGISTER (videoframe_audiolevel, plugin);
}

GST_PLUGIN_DEFINE (GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    videoframe_audiolevel,
    "Video frame-synchronized audio level",
    gst_videoframe_audiolevel_plugin_init, VERSION, GST_LICENSE,
    GST_PACKAGE_NAME, GST_PACKAGE_ORIGIN);
