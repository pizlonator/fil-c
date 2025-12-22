/* GStreamer
 * Copyright (C) 2016 Iskratel d.o.o.
 *   Author: Okrslar Ales <okrslar@iskratel.si>
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

#include "gsttonegeneratesrc.h"

#undef IT_DBG

GST_DEBUG_CATEGORY_STATIC (tone_generate_src_debug);
#define GST_CAT_DEFAULT tone_generate_src_debug

#define DEFAULT_SAMPLES_PER_BUFFER   1024
#define DEFAULT_FREQ                 0
#define DEFAULT_VOLUME               0
#define DEFAULT_ON_TIME              1000
#define DEFAULT_OFF_TIME             1000
#define DEFAULT_REPEAT               FALSE

enum
{
  PROP_0,
  PROP_SAMPLES_PER_BUFFER,
  PROP_FREQ,
  PROP_VOLUME,
  PROP_FREQ2,
  PROP_VOLUME2,
  PROP_ON_TIME,
  PROP_OFF_TIME,
  PROP_ON_TIME2,
  PROP_OFF_TIME2,
  PROP_REPEAT,
  PROP_LAST
};

static GstStaticPadTemplate gst_tone_generate_src_src_template =
GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("audio/x-raw, "
        "format = (string) " GST_AUDIO_NE (S16) ", "
        "layout = (string) interleaved, " "rate = (int) 8000, " "channels = 1")
    );

#define gst_tone_generate_src_parent_class parent_class
G_DEFINE_TYPE_WITH_CODE (GstToneGenerateSrc, gst_tone_generate_src,
    GST_TYPE_PUSH_SRC, GST_DEBUG_CATEGORY_INIT (tone_generate_src_debug,
        "tonegeneratesrc", 0, "Telephony Tone Test Source");
    );

GST_ELEMENT_REGISTER_DEFINE (tonegeneratesrc, "tonegeneratesrc", GST_RANK_NONE,
    GST_TYPE_TONE_GENERATE_SRC);
static void gst_tone_generate_src_finalize (GObject * object);
static void gst_tone_generate_src_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_tone_generate_src_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);
static gboolean gst_tone_generate_src_start (GstBaseSrc * basesrc);
static gboolean gst_tone_generate_src_stop (GstBaseSrc * basesrc);
static GstFlowReturn gst_tone_generate_src_fill (GstPushSrc * basesrc,
    GstBuffer * buffer);

static void
gst_tone_generate_src_class_init (GstToneGenerateSrcClass * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *gstelement_class;
  GstBaseSrcClass *gstbasesrc_class;
  GstPushSrcClass *gstpushsrc_class;

  gobject_class = (GObjectClass *) klass;
  gstelement_class = (GstElementClass *) klass;
  gstbasesrc_class = (GstBaseSrcClass *) klass;
  gstpushsrc_class = (GstPushSrcClass *) klass;

  gobject_class->set_property = gst_tone_generate_src_set_property;
  gobject_class->get_property = gst_tone_generate_src_get_property;
  gobject_class->finalize = gst_tone_generate_src_finalize;

  g_object_class_install_property (gobject_class, PROP_SAMPLES_PER_BUFFER,
      g_param_spec_int ("samplesperbuffer", "Samples per buffer",
          "Number of samples in each outgoing buffer",
          1, G_MAXINT, DEFAULT_SAMPLES_PER_BUFFER,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_FREQ,
      g_param_spec_int ("freq", "Frequency", "Frequency of test signal",
          0, 20000, DEFAULT_FREQ, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_VOLUME,
      g_param_spec_int ("volume", "Volume",
          "Volume of first signal",
          -50, 0, DEFAULT_VOLUME, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_FREQ2,
      g_param_spec_int ("freq2", "Second Frequency",
          "Frequency of second telephony tone component",
          0, 20000, DEFAULT_FREQ, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_VOLUME2,
      g_param_spec_int ("volume2", "Volume2",
          "Volume of second tone signal",
          -50, 0, DEFAULT_VOLUME, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_ON_TIME,
      g_param_spec_int ("on-time", "Signal ON time first period",
          "Time of the first period  when the tone signal is present", 1,
          G_MAXINT, DEFAULT_ON_TIME, G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class, PROP_OFF_TIME,
      g_param_spec_int ("off-time", "Signal OFF time first period ",
          "Time of the first period  when the tone signal is off", 0, G_MAXINT,
          DEFAULT_OFF_TIME, G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class, PROP_ON_TIME2,
      g_param_spec_int ("on-time2", "Signal ON time second period",
          "Time of the second period  when the tone signal is present", 1,
          G_MAXINT, DEFAULT_ON_TIME, G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class, PROP_OFF_TIME2,
      g_param_spec_int ("off-time2", "Signal OFF time first period ",
          "Time of the second period  when the tone signal is off", 0, G_MAXINT,
          DEFAULT_ON_TIME, G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class, PROP_REPEAT,
      g_param_spec_boolean ("repeat", "Repeat the specified tone period ",
          "Whether to repeat specified tone indefinitely", DEFAULT_REPEAT,
          G_PARAM_READWRITE));

  gst_element_class_add_static_pad_template (gstelement_class,
      &gst_tone_generate_src_src_template);

  gst_element_class_set_static_metadata (gstelement_class,
      "Telephony Tone  Generator source", "Source/Audio",
      "Creates telephony signals of given frequency, volume, cadence",
      "Iskratel <www.iskratel.com>");

  gstbasesrc_class->start = GST_DEBUG_FUNCPTR (gst_tone_generate_src_start);
  gstbasesrc_class->stop = GST_DEBUG_FUNCPTR (gst_tone_generate_src_stop);
  gstpushsrc_class->fill = GST_DEBUG_FUNCPTR (gst_tone_generate_src_fill);
}

static void
gst_tone_generate_src_init (GstToneGenerateSrc * src)
{
  src->volume = DEFAULT_VOLUME;
  src->freq = DEFAULT_FREQ;
  src->on_time = DEFAULT_ON_TIME;
  src->off_time = DEFAULT_OFF_TIME;
  src->volume2 = DEFAULT_VOLUME;
  src->freq2 = DEFAULT_FREQ;
  src->on_time2 = DEFAULT_ON_TIME;
  src->off_time2 = DEFAULT_OFF_TIME;
  src->repeat = DEFAULT_REPEAT;

  gst_base_src_set_format (GST_BASE_SRC (src), GST_FORMAT_TIME);

  src->samples_per_buffer = DEFAULT_SAMPLES_PER_BUFFER;
  gst_base_src_set_blocksize (GST_BASE_SRC (src), 2 * src->samples_per_buffer);
}

static void
gst_tone_generate_src_finalize (GObject * object)
{
  GstToneGenerateSrc *src = GST_TONE_GENERATE_SRC (object);

  if (src->tone_desc) {
    tone_gen_descriptor_free (src->tone_desc);
    src->tone_desc = NULL;
  }

  if (src->tone_state) {
    tone_gen_free (src->tone_state);
    src->tone_state = NULL;
  }

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static gboolean
gst_tone_generate_src_start (GstBaseSrc * basesrc)
{
  GstToneGenerateSrc *src = GST_TONE_GENERATE_SRC (basesrc);

  GST_OBJECT_LOCK (src);
  src->properties_changed = FALSE;
  GST_OBJECT_UNLOCK (src);

  src->next_sample = 0;
  src->next_time = 0;

  return TRUE;
}

static gboolean
gst_tone_generate_src_stop (GstBaseSrc * basesrc)
{
  GstToneGenerateSrc *src = GST_TONE_GENERATE_SRC (basesrc);

  GST_OBJECT_LOCK (src);
  if (src->tone_desc) {
    tone_gen_descriptor_free (src->tone_desc);
    src->tone_desc = NULL;
  }

  if (src->tone_state) {
    tone_gen_free (src->tone_state);
    src->tone_state = NULL;
  }
  src->properties_changed = FALSE;
  GST_OBJECT_UNLOCK (src);

  return TRUE;
}

static GstFlowReturn
gst_tone_generate_src_fill (GstPushSrc * basesrc, GstBuffer * buffer)
{
  GstToneGenerateSrc *src;
  GstClockTime next_time;
  gint64 next_sample;
  gint bytes, samples;
  GstMapInfo map;
  const gint samplerate = 8000, bpf = 2;

  src = GST_TONE_GENERATE_SRC (basesrc);

  bytes = gst_buffer_get_size (buffer);
  samples = bytes / bpf;

  /* calculate full buffer */
  next_sample = src->next_sample + samples;

  next_time = gst_util_uint64_scale_int (next_sample, GST_SECOND, samplerate);

  GST_LOG_OBJECT (src, "samplerate %d", samplerate);
  GST_LOG_OBJECT (src, "next_sample %" G_GINT64_FORMAT ", ts %" GST_TIME_FORMAT,
      next_sample, GST_TIME_ARGS (next_time));

  GST_BUFFER_OFFSET (buffer) = src->next_sample;
  GST_BUFFER_OFFSET_END (buffer) = next_sample;
  GST_BUFFER_TIMESTAMP (buffer) = src->next_time;
  GST_BUFFER_DURATION (buffer) = next_time - src->next_time;

  gst_object_sync_values (GST_OBJECT (src), GST_BUFFER_TIMESTAMP (buffer));

  src->next_time = next_time;
  src->next_sample = next_sample;

  GST_LOG_OBJECT (src, "generating %u samples at ts %" GST_TIME_FORMAT,
      samples, GST_TIME_ARGS (GST_BUFFER_TIMESTAMP (buffer)));

  gst_buffer_map (buffer, &map, GST_MAP_WRITE);

  GST_OBJECT_LOCK (src);
  if (!src->tone_state || src->properties_changed) {
    src->tone_desc = tone_gen_descriptor_init (src->tone_desc,
        src->freq,
        src->volume,
        src->freq2,
        src->volume2,
        src->on_time,
        src->off_time, src->on_time2, src->off_time2, src->repeat);

    src->tone_state = tone_gen_init (src->tone_state, src->tone_desc);
    src->properties_changed = FALSE;
  }

  tone_gen (src->tone_state, (int16_t *) map.data, samples);
  GST_OBJECT_UNLOCK (src);

  gst_buffer_unmap (buffer, &map);

  return GST_FLOW_OK;
}

static void
gst_tone_generate_src_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstToneGenerateSrc *src = GST_TONE_GENERATE_SRC (object);

  switch (prop_id) {
    case PROP_SAMPLES_PER_BUFFER:
      src->samples_per_buffer = g_value_get_int (value);
      gst_base_src_set_blocksize (GST_BASE_SRC_CAST (src),
          2 * src->samples_per_buffer);
      break;
    case PROP_FREQ:
      GST_OBJECT_LOCK (src);
      src->freq = g_value_get_int (value);
      src->properties_changed = TRUE;
      GST_OBJECT_UNLOCK (src);
      break;
    case PROP_VOLUME:
      GST_OBJECT_LOCK (src);
      src->volume = g_value_get_int (value);
      src->properties_changed = TRUE;
      GST_OBJECT_UNLOCK (src);
      break;
    case PROP_FREQ2:
      GST_OBJECT_LOCK (src);
      src->freq2 = g_value_get_int (value);
      src->properties_changed = TRUE;
      GST_OBJECT_UNLOCK (src);
      break;
    case PROP_VOLUME2:
      GST_OBJECT_LOCK (src);
      src->volume2 = g_value_get_int (value);
      src->properties_changed = TRUE;
      GST_OBJECT_UNLOCK (src);
      break;
    case PROP_ON_TIME:
      GST_OBJECT_LOCK (src);
      src->on_time = g_value_get_int (value);
      src->properties_changed = TRUE;
      GST_OBJECT_UNLOCK (src);
      break;
    case PROP_ON_TIME2:
      GST_OBJECT_LOCK (src);
      src->on_time2 = g_value_get_int (value);
      src->properties_changed = TRUE;
      GST_OBJECT_UNLOCK (src);
      break;
    case PROP_OFF_TIME:
      GST_OBJECT_LOCK (src);
      src->off_time = g_value_get_int (value);
      src->properties_changed = TRUE;
      GST_OBJECT_UNLOCK (src);
      break;
    case PROP_OFF_TIME2:
      GST_OBJECT_LOCK (src);
      src->off_time2 = g_value_get_int (value);
      src->properties_changed = TRUE;
      GST_OBJECT_UNLOCK (src);
      break;
    case PROP_REPEAT:
      GST_OBJECT_LOCK (src);
      src->repeat = g_value_get_boolean (value);
      src->properties_changed = TRUE;
      GST_OBJECT_UNLOCK (src);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_tone_generate_src_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstToneGenerateSrc *src = GST_TONE_GENERATE_SRC (object);

  switch (prop_id) {
    case PROP_SAMPLES_PER_BUFFER:
      g_value_set_int (value, src->samples_per_buffer);
      break;
    case PROP_FREQ:
      g_value_set_int (value, src->freq);
      break;
    case PROP_VOLUME:
      g_value_set_int (value, src->volume);
      break;
    case PROP_FREQ2:
      g_value_set_int (value, src->freq2);
      break;
    case PROP_VOLUME2:
      g_value_set_int (value, src->volume2);
      break;
    case PROP_ON_TIME:
      g_value_set_int (value, src->on_time);
      break;
    case PROP_OFF_TIME:
      g_value_set_int (value, src->off_time);
      break;
    case PROP_ON_TIME2:
      g_value_set_int (value, src->on_time2);
      break;
    case PROP_OFF_TIME2:
      g_value_set_int (value, src->off_time2);
      break;
    case PROP_REPEAT:
      g_value_set_boolean (value, src->repeat);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}
