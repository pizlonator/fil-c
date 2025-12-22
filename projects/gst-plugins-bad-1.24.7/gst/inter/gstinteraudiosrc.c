/* GStreamer
 * Copyright (C) 2011 David A. Schleef <ds@schleef.org>
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
 * SECTION:element-interaudiosrc
 * @title: gstinteraudiosrc
 *
 * The interaudiosrc element is an audio source element.  It is used
 * in connection with a interaudiosink element in a different pipeline.
 *
 * ## Example launch line
 * |[
 * gst-launch-1.0 -v interaudiosrc ! queue ! autoaudiosink
 * ]|
 *
 * The interaudiosrc element cannot be used effectively with gst-launch-1.0,
 * as it requires a second pipeline in the application to send audio.
 * See the gstintertest.c example in the gst-plugins-bad source code for
 * more details.
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gstinteraudiosrc.h"

#include <gst/gst.h>
#include <gst/base/gstbasesrc.h>
#include <gst/audio/audio.h>

#include <string.h>

GST_DEBUG_CATEGORY_STATIC (gst_inter_audio_src_debug_category);
#define GST_CAT_DEFAULT gst_inter_audio_src_debug_category

/* prototypes */
static void gst_inter_audio_src_set_property (GObject * object,
    guint property_id, const GValue * value, GParamSpec * pspec);
static void gst_inter_audio_src_get_property (GObject * object,
    guint property_id, GValue * value, GParamSpec * pspec);
static void gst_inter_audio_src_finalize (GObject * object);

static GstCaps *gst_inter_audio_src_get_caps (GstBaseSrc * src,
    GstCaps * filter);
static gboolean gst_inter_audio_src_set_caps (GstBaseSrc * src, GstCaps * caps);
static gboolean gst_inter_audio_src_start (GstBaseSrc * src);
static gboolean gst_inter_audio_src_stop (GstBaseSrc * src);
static void
gst_inter_audio_src_get_times (GstBaseSrc * src, GstBuffer * buffer,
    GstClockTime * start, GstClockTime * end);
static GstFlowReturn
gst_inter_audio_src_create (GstBaseSrc * src, guint64 offset, guint size,
    GstBuffer ** buf);
static gboolean gst_inter_audio_src_query (GstBaseSrc * src, GstQuery * query);
static GstCaps *gst_inter_audio_src_fixate (GstBaseSrc * src, GstCaps * caps);

enum
{
  PROP_0,
  PROP_CHANNEL,
  PROP_BUFFER_TIME,
  PROP_LATENCY_TIME,
  PROP_PERIOD_TIME
};

#define DEFAULT_CHANNEL ("default")

/* pad templates */
static GstStaticPadTemplate gst_inter_audio_src_src_template =
GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (GST_AUDIO_CAPS_MAKE (GST_AUDIO_FORMATS_ALL)
        ", layout = (string) interleaved")
    );


/* class initialization */
#define parent_class gst_inter_audio_src_parent_class
G_DEFINE_TYPE (GstInterAudioSrc, gst_inter_audio_src, GST_TYPE_BASE_SRC);
GST_ELEMENT_REGISTER_DEFINE (interaudiosrc, "interaudiosrc",
    GST_RANK_NONE, GST_TYPE_INTER_AUDIO_SRC);

static void
gst_inter_audio_src_class_init (GstInterAudioSrcClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GstElementClass *element_class = GST_ELEMENT_CLASS (klass);
  GstBaseSrcClass *base_src_class = GST_BASE_SRC_CLASS (klass);

  GST_DEBUG_CATEGORY_INIT (gst_inter_audio_src_debug_category, "interaudiosrc",
      0, "debug category for interaudiosrc element");

  gst_element_class_add_static_pad_template (element_class,
      &gst_inter_audio_src_src_template);

  gst_element_class_set_static_metadata (element_class,
      "Internal audio source",
      "Source/Audio",
      "Virtual audio source for internal process communication",
      "David Schleef <ds@schleef.org>");

  gobject_class->set_property = gst_inter_audio_src_set_property;
  gobject_class->get_property = gst_inter_audio_src_get_property;
  gobject_class->finalize = gst_inter_audio_src_finalize;
  base_src_class->get_caps = GST_DEBUG_FUNCPTR (gst_inter_audio_src_get_caps);
  base_src_class->set_caps = GST_DEBUG_FUNCPTR (gst_inter_audio_src_set_caps);
  base_src_class->start = GST_DEBUG_FUNCPTR (gst_inter_audio_src_start);
  base_src_class->stop = GST_DEBUG_FUNCPTR (gst_inter_audio_src_stop);
  base_src_class->get_times = GST_DEBUG_FUNCPTR (gst_inter_audio_src_get_times);
  base_src_class->create = GST_DEBUG_FUNCPTR (gst_inter_audio_src_create);
  base_src_class->query = GST_DEBUG_FUNCPTR (gst_inter_audio_src_query);
  base_src_class->fixate = GST_DEBUG_FUNCPTR (gst_inter_audio_src_fixate);

  g_object_class_install_property (gobject_class, PROP_CHANNEL,
      g_param_spec_string ("channel", "Channel",
          "Channel name to match inter src and sink elements",
          DEFAULT_CHANNEL, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_BUFFER_TIME,
      g_param_spec_uint64 ("buffer-time", "Buffer Time",
          "Size of audio buffer", 1, G_MAXUINT64, DEFAULT_AUDIO_BUFFER_TIME,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_LATENCY_TIME,
      g_param_spec_uint64 ("latency-time", "Latency Time",
          "Latency as reported by the source",
          1, G_MAXUINT64, DEFAULT_AUDIO_LATENCY_TIME,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_PERIOD_TIME,
      g_param_spec_uint64 ("period-time", "Period Time",
          "The minimum amount of data to read in each iteration",
          1, G_MAXUINT64, DEFAULT_AUDIO_PERIOD_TIME,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
}

static void
gst_inter_audio_src_init (GstInterAudioSrc * interaudiosrc)
{
  gst_base_src_set_format (GST_BASE_SRC (interaudiosrc), GST_FORMAT_TIME);
  gst_base_src_set_live (GST_BASE_SRC (interaudiosrc), TRUE);
  gst_base_src_set_blocksize (GST_BASE_SRC (interaudiosrc), -1);

  interaudiosrc->channel = g_strdup (DEFAULT_CHANNEL);
  interaudiosrc->buffer_time = DEFAULT_AUDIO_BUFFER_TIME;
  interaudiosrc->latency_time = DEFAULT_AUDIO_LATENCY_TIME;
  interaudiosrc->period_time = DEFAULT_AUDIO_PERIOD_TIME;
}

void
gst_inter_audio_src_set_property (GObject * object, guint property_id,
    const GValue * value, GParamSpec * pspec)
{
  GstInterAudioSrc *interaudiosrc = GST_INTER_AUDIO_SRC (object);

  switch (property_id) {
    case PROP_CHANNEL:
      g_free (interaudiosrc->channel);
      interaudiosrc->channel = g_value_dup_string (value);
      break;
    case PROP_BUFFER_TIME:
      interaudiosrc->buffer_time = g_value_get_uint64 (value);
      break;
    case PROP_LATENCY_TIME:
      interaudiosrc->latency_time = g_value_get_uint64 (value);
      break;
    case PROP_PERIOD_TIME:
      interaudiosrc->period_time = g_value_get_uint64 (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

void
gst_inter_audio_src_get_property (GObject * object, guint property_id,
    GValue * value, GParamSpec * pspec)
{
  GstInterAudioSrc *interaudiosrc = GST_INTER_AUDIO_SRC (object);

  switch (property_id) {
    case PROP_CHANNEL:
      g_value_set_string (value, interaudiosrc->channel);
      break;
    case PROP_BUFFER_TIME:
      g_value_set_uint64 (value, interaudiosrc->buffer_time);
      break;
    case PROP_LATENCY_TIME:
      g_value_set_uint64 (value, interaudiosrc->latency_time);
      break;
    case PROP_PERIOD_TIME:
      g_value_set_uint64 (value, interaudiosrc->period_time);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

void
gst_inter_audio_src_finalize (GObject * object)
{
  GstInterAudioSrc *interaudiosrc = GST_INTER_AUDIO_SRC (object);

  /* clean up object here */
  g_free (interaudiosrc->channel);

  G_OBJECT_CLASS (gst_inter_audio_src_parent_class)->finalize (object);
}

static GstCaps *
gst_inter_audio_src_get_caps (GstBaseSrc * src, GstCaps * filter)
{
  GstInterAudioSrc *interaudiosrc = GST_INTER_AUDIO_SRC (src);
  GstCaps *caps;

  GST_DEBUG_OBJECT (interaudiosrc, "get_caps");

  if (!interaudiosrc->surface)
    return GST_BASE_SRC_CLASS (parent_class)->get_caps (src, filter);

  g_mutex_lock (&interaudiosrc->surface->mutex);
  if (interaudiosrc->surface->audio_info.finfo) {
    caps = gst_audio_info_to_caps (&interaudiosrc->surface->audio_info);
    if (filter) {
      GstCaps *tmp;

      tmp = gst_caps_intersect_full (filter, caps, GST_CAPS_INTERSECT_FIRST);
      gst_caps_unref (caps);
      caps = tmp;
    }
  } else {
    caps = NULL;
  }
  g_mutex_unlock (&interaudiosrc->surface->mutex);

  if (caps)
    return caps;
  else
    return GST_BASE_SRC_CLASS (parent_class)->get_caps (src, filter);
}

static gboolean
gst_inter_audio_src_set_caps (GstBaseSrc * src, GstCaps * caps)
{
  GstInterAudioSrc *interaudiosrc = GST_INTER_AUDIO_SRC (src);

  GST_DEBUG_OBJECT (interaudiosrc, "set_caps");

  if (!gst_audio_info_from_caps (&interaudiosrc->info, caps)) {
    GST_ERROR_OBJECT (src, "Failed to parse caps %" GST_PTR_FORMAT, caps);
    return FALSE;
  }

  return TRUE;
}

static gboolean
gst_inter_audio_src_start (GstBaseSrc * src)
{
  GstInterAudioSrc *interaudiosrc = GST_INTER_AUDIO_SRC (src);

  GST_DEBUG_OBJECT (interaudiosrc, "start");

  interaudiosrc->surface = gst_inter_surface_get (interaudiosrc->channel);
  interaudiosrc->timestamp_offset = 0;
  interaudiosrc->n_samples = 0;

  g_mutex_lock (&interaudiosrc->surface->mutex);
  interaudiosrc->surface->audio_buffer_time = interaudiosrc->buffer_time;
  interaudiosrc->surface->audio_latency_time = interaudiosrc->latency_time;
  interaudiosrc->surface->audio_period_time = interaudiosrc->period_time;
  g_mutex_unlock (&interaudiosrc->surface->mutex);

  return TRUE;
}

static gboolean
gst_inter_audio_src_stop (GstBaseSrc * src)
{
  GstInterAudioSrc *interaudiosrc = GST_INTER_AUDIO_SRC (src);

  GST_DEBUG_OBJECT (interaudiosrc, "stop");

  gst_inter_surface_unref (interaudiosrc->surface);
  interaudiosrc->surface = NULL;

  return TRUE;
}

static void
gst_inter_audio_src_get_times (GstBaseSrc * src, GstBuffer * buffer,
    GstClockTime * start, GstClockTime * end)
{
  GstInterAudioSrc *interaudiosrc = GST_INTER_AUDIO_SRC (src);

  GST_DEBUG_OBJECT (src, "get_times");

  /* for live sources, sync on the timestamp of the buffer */
  if (gst_base_src_is_live (src)) {
    if (GST_BUFFER_TIMESTAMP_IS_VALID (buffer)) {
      *start = GST_BUFFER_TIMESTAMP (buffer);
      if (GST_BUFFER_DURATION_IS_VALID (buffer)) {
        *end = *start + GST_BUFFER_DURATION (buffer);
      } else {
        if (interaudiosrc->info.rate > 0) {
          *end = *start +
              gst_util_uint64_scale_int (gst_buffer_get_size (buffer),
              GST_SECOND, interaudiosrc->info.rate * interaudiosrc->info.bpf);
        }
      }
    }
  }
}

static GstFlowReturn
gst_inter_audio_src_create (GstBaseSrc * src, guint64 offset, guint size,
    GstBuffer ** buf)
{
  GstInterAudioSrc *interaudiosrc = GST_INTER_AUDIO_SRC (src);
  GstCaps *caps;
  GstBuffer *buffer;
  guint n, bpf;
  guint64 period_time;
  guint64 period_samples;

  GST_DEBUG_OBJECT (interaudiosrc, "create");

  buffer = NULL;
  caps = NULL;

  g_mutex_lock (&interaudiosrc->surface->mutex);
  if (interaudiosrc->surface->audio_info.finfo) {
    if (!gst_audio_info_is_equal (&interaudiosrc->surface->audio_info,
            &interaudiosrc->info)) {
      caps = gst_audio_info_to_caps (&interaudiosrc->surface->audio_info);
      interaudiosrc->timestamp_offset +=
          gst_util_uint64_scale (interaudiosrc->n_samples, GST_SECOND,
          interaudiosrc->info.rate);
      interaudiosrc->n_samples = 0;
    }
  }

  bpf = interaudiosrc->surface->audio_info.bpf;
  period_time = interaudiosrc->surface->audio_period_time;
  period_samples =
      gst_util_uint64_scale (period_time, interaudiosrc->info.rate, GST_SECOND);

  if (bpf > 0)
    n = gst_adapter_available (interaudiosrc->surface->audio_adapter) / bpf;
  else
    n = 0;

  if (n > period_samples)
    n = period_samples;
  if (n > 0) {
    buffer = gst_adapter_take_buffer (interaudiosrc->surface->audio_adapter,
        n * bpf);
  } else {
    buffer = gst_buffer_new ();
    GST_BUFFER_FLAG_SET (buffer, GST_BUFFER_FLAG_GAP);
  }
  g_mutex_unlock (&interaudiosrc->surface->mutex);

  if (caps) {
    gboolean ret = gst_base_src_set_caps (src, caps);
    gst_caps_unref (caps);
    if (!ret) {
      GST_ERROR_OBJECT (src, "Failed to set caps %" GST_PTR_FORMAT, caps);
      if (buffer)
        gst_buffer_unref (buffer);
      return GST_FLOW_NOT_NEGOTIATED;
    }
  }

  buffer = gst_buffer_make_writable (buffer);

  bpf = interaudiosrc->info.bpf;
  if (n < period_samples) {
    GstMapInfo map;
    GstMemory *mem;

    GST_DEBUG_OBJECT (interaudiosrc,
        "creating %" G_GUINT64_FORMAT " samples of silence",
        period_samples - n);
    mem = gst_allocator_alloc (NULL, (period_samples - n) * bpf, NULL);
    if (gst_memory_map (mem, &map, GST_MAP_WRITE)) {
      gst_audio_format_info_fill_silence (interaudiosrc->info.finfo, map.data,
          map.size);
      gst_memory_unmap (mem, &map);
    }
    gst_buffer_prepend_memory (buffer, mem);
  }
  n = period_samples;

  /* audioconvert expects an audio meta for planar layout audio inputs. */
  if (GST_AUDIO_INFO_LAYOUT (&interaudiosrc->info) ==
      GST_AUDIO_LAYOUT_NON_INTERLEAVED) {
    gst_buffer_add_audio_meta (buffer, &interaudiosrc->info, n, NULL);
  }

  GST_BUFFER_OFFSET (buffer) = interaudiosrc->n_samples;
  GST_BUFFER_OFFSET_END (buffer) = interaudiosrc->n_samples + n;
  GST_BUFFER_DTS (buffer) = GST_CLOCK_TIME_NONE;
  GST_BUFFER_PTS (buffer) = interaudiosrc->timestamp_offset +
      gst_util_uint64_scale (interaudiosrc->n_samples, GST_SECOND,
      interaudiosrc->info.rate);
  GST_DEBUG_OBJECT (interaudiosrc, "create ts %" GST_TIME_FORMAT,
      GST_TIME_ARGS (GST_BUFFER_TIMESTAMP (buffer)));
  GST_BUFFER_DURATION (buffer) = interaudiosrc->timestamp_offset +
      gst_util_uint64_scale (interaudiosrc->n_samples + n, GST_SECOND,
      interaudiosrc->info.rate) - GST_BUFFER_TIMESTAMP (buffer);
  GST_BUFFER_FLAG_UNSET (buffer, GST_BUFFER_FLAG_DISCONT);
  if (interaudiosrc->n_samples == 0) {
    GST_BUFFER_FLAG_SET (buffer, GST_BUFFER_FLAG_DISCONT);
  }
  interaudiosrc->n_samples += n;

  *buf = buffer;

  return GST_FLOW_OK;
}

static gboolean
gst_inter_audio_src_query (GstBaseSrc * src, GstQuery * query)
{
  GstInterAudioSrc *interaudiosrc = GST_INTER_AUDIO_SRC (src);
  gboolean ret;

  GST_DEBUG_OBJECT (src, "query");

  switch (GST_QUERY_TYPE (query)) {
    case GST_QUERY_LATENCY:{
      GstClockTime min_latency, max_latency;

      min_latency = interaudiosrc->latency_time;
      max_latency = interaudiosrc->buffer_time;

      GST_DEBUG_OBJECT (src,
          "report latency min %" GST_TIME_FORMAT " max %" GST_TIME_FORMAT,
          GST_TIME_ARGS (min_latency), GST_TIME_ARGS (max_latency));

      gst_query_set_latency (query,
          gst_base_src_is_live (src), min_latency, max_latency);

      ret = TRUE;
      break;
    }
    default:
      ret = GST_BASE_SRC_CLASS (gst_inter_audio_src_parent_class)->query (src,
          query);
      break;
  }

  return ret;
}

static GstCaps *
gst_inter_audio_src_fixate (GstBaseSrc * src, GstCaps * caps)
{
  GstStructure *structure;

  GST_DEBUG_OBJECT (src, "fixate");

  caps = gst_caps_make_writable (caps);
  caps = gst_caps_truncate (caps);

  structure = gst_caps_get_structure (caps, 0);

  gst_structure_fixate_field_string (structure, "format", GST_AUDIO_NE (S16));
  gst_structure_fixate_field_nearest_int (structure, "channels", 2);
  gst_structure_fixate_field_nearest_int (structure, "rate", 48000);
  gst_structure_fixate_field_string (structure, "layout", "interleaved");

  return caps;
}
