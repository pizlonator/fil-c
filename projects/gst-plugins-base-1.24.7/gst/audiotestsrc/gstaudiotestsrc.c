/* GStreamer
 * Copyright (C) 2005 Stefan Kost <ensonic@users.sf.net>
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
 * SECTION:element-audiotestsrc
 * @title: audiotestsrc
 *
 * AudioTestSrc can be used to generate basic audio signals. It support several
 * different waveforms and allows to set the base frequency and volume. Some
 * waveforms might use additional properties.
 *
 * Waveform specific notes:
 *
 * ## Gaussian white noise
 * 
 * This waveform produces white (zero mean) Gaussian noise.
 * Volume sets the standard deviation of the noise in units of the range
 * of values of the sample type, e.g. volume=0.1 produces noise with a
 * standard deviation of 0.1*32767=3277 with 16-bit integer samples,
 * or 0.1*1.0=0.1 with floating-point samples.
 *     
 * ## Ticks
 *     
 * This waveform is special in that it does not produce one continuous
 * signal. Instead, it produces finite-length sine wave pulses (the "ticks").
 * It is useful for detecting time shifts between audio signal, for example
 * between RTSP audio clients that shall play synchronized. It is also useful
 * for generating a signal that feeds the trigger of an oscilloscope.
 *
 * To further help with oscilloscope triggering and time offset detection,
 * the waveform can apply a different volume to every Nth tick (this is then
 * called the "marker tick"). For instance, one could generate a tick every
 * 100ms, and make every 20th tick a marker tick (meaning that every 2 seconds
 * there is a marker tick). This is useful for detecting large time offsets
 * while still frequently triggering an oscilloscope.
 *
 * Also, a "ramp" can be applied to the begin & end of ticks. The sudden
 * start of the sine tick is a discontinuity, even if the sine wave starts
 * at 0. The* resulting artifacts can often make it more difficult to use the
 * ticks for an oscilloscope's trigger. To that end, an initial "ramp" can
 * be applied. The first few samples are modulated by a cubic function to
 * reduce the impact of the discontinuity, resulting in smaller artifacts.
 * The number of samples equals floor(samplerate / sine-wave-frequency).
 * Example: with a sample rate of 48 kHz and a sine wave frequency of 10 kHz,
 * the first 4 samples are modulated by the cubic function.
 *
 * ## Example launch line
 * |[
 * gst-launch-1.0 audiotestsrc ! audioconvert ! autoaudiosink
 * ]|
 *  This pipeline produces a sine with default frequency, 440 Hz, and the
 * default volume, 0.8 (relative to a maximum 1.0).
 * |[
 * gst-launch-1.0 audiotestsrc wave=2 freq=200 ! tee name=t ! queue ! audioconvert ! \
 *     autoaudiosink t. ! queue ! audioconvert ! libvisual_lv_scope ! videoconvert ! autovideosink
 * ]|
 *  In this example a saw wave is generated. The wave is shown using a
 * scope visualizer from libvisual, allowing you to visually verify that
 * the saw wave is correct.
 *
 * |[
 * gst-launch-1.0 audiotestsrc wave=ticks apply-tick-ramp=true tick-interval=100000000 \
 *     freq=10000 volume=0.4 marker-tick-period=10 sine-periods-per-tick=20 ! autoaudiosink
 * ]| This pipeline produces a series of 10 kHz sine wave ticks. Each tick is
 * 20 sine wave periods long, ticks occur every 100 ms and have a volume of
 * 0.4. Every 10th tick is a marker tick and has the default marker tick volume
 * of 1.0. The beginning and end of the ticks are modulated with the ramp.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "gstaudiotestsrc.h"


#define M_PI_M2 ( G_PI + G_PI )

GST_DEBUG_CATEGORY_STATIC (audio_test_src_debug);
#define GST_CAT_DEFAULT audio_test_src_debug

#define DEFAULT_SAMPLES_PER_BUFFER      1024
#define DEFAULT_WAVE                    GST_AUDIO_TEST_SRC_WAVE_SINE
#define DEFAULT_FREQ                    440.0
#define DEFAULT_VOLUME                  0.8
#define DEFAULT_IS_LIVE                 FALSE
#define DEFAULT_TIMESTAMP_OFFSET        G_GINT64_CONSTANT (0)
#define DEFAULT_SINE_PERIODS_PER_TICK   10
#define DEFAULT_TIME_BETWEEN_TICKS      GST_SECOND
#define DEFAULT_MARKER_TICK_PERIOD    0
#define DEFAULT_MARKER_TICK_VOLUME      1.0
#define DEFAULT_APPLY_TICK_RAMP         FALSE
#define DEFAULT_CAN_ACTIVATE_PUSH       TRUE
#define DEFAULT_CAN_ACTIVATE_PULL       FALSE

enum
{
  PROP_0,
  PROP_SAMPLES_PER_BUFFER,
  PROP_WAVE,
  PROP_FREQ,
  PROP_VOLUME,
  PROP_IS_LIVE,
  PROP_TIMESTAMP_OFFSET,
  PROP_SINE_PERIODS_PER_TICK,
  PROP_TICK_INTERVAL,
  PROP_MARKER_TICK_PERIOD,
  PROP_MARKER_TICK_VOLUME,
  PROP_APPLY_TICK_RAMP,
  PROP_CAN_ACTIVATE_PUSH,
  PROP_CAN_ACTIVATE_PULL
};

#define FORMAT_STR  " { S16LE, S16BE, U16LE, U16BE, " \
    "S24_32LE, S24_32BE, U24_32LE, U24_32BE, " \
    "S32LE, S32BE, U32LE, U32BE, " \
    "S24LE, S24BE, U24LE, U24BE, " \
    "S20LE, S20BE, U20LE, U20BE, " \
    "S18LE, S18BE, U18LE, U18BE, " \
    "F32LE, F32BE, F64LE, F64BE, " \
    "S8, U8 }"

#define DEFAULT_FORMAT_STR GST_AUDIO_NE ("S16")

static GstStaticPadTemplate gst_audio_test_src_src_template =
GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("audio/x-raw, "
        "format = (string) " FORMAT_STR ", "
        "layout = (string) { interleaved, non-interleaved }, "
        "rate = " GST_AUDIO_RATE_RANGE ", "
        "channels = " GST_AUDIO_CHANNELS_RANGE)
    );

#define gst_audio_test_src_parent_class parent_class
G_DEFINE_TYPE (GstAudioTestSrc, gst_audio_test_src, GST_TYPE_BASE_SRC);
GST_ELEMENT_REGISTER_DEFINE_WITH_CODE (audiotestsrc, "audiotestsrc",
    GST_RANK_NONE, GST_TYPE_AUDIO_TEST_SRC,
    GST_DEBUG_CATEGORY_INIT (audio_test_src_debug, "audiotestsrc", 0,
        "Audio Test Source"));

#define GST_TYPE_AUDIO_TEST_SRC_WAVE (gst_audiostestsrc_wave_get_type())
static GType
gst_audiostestsrc_wave_get_type (void)
{
  static GType audiostestsrc_wave_type = 0;
  static const GEnumValue audiostestsrc_waves[] = {
    {GST_AUDIO_TEST_SRC_WAVE_SINE, "Sine", "sine"},
    {GST_AUDIO_TEST_SRC_WAVE_SQUARE, "Square", "square"},
    {GST_AUDIO_TEST_SRC_WAVE_SAW, "Saw", "saw"},
    {GST_AUDIO_TEST_SRC_WAVE_TRIANGLE, "Triangle", "triangle"},
    {GST_AUDIO_TEST_SRC_WAVE_SILENCE, "Silence", "silence"},
    {GST_AUDIO_TEST_SRC_WAVE_WHITE_NOISE, "White uniform noise", "white-noise"},
    {GST_AUDIO_TEST_SRC_WAVE_PINK_NOISE, "Pink noise", "pink-noise"},
    {GST_AUDIO_TEST_SRC_WAVE_SINE_TAB, "Sine table", "sine-table"},
    {GST_AUDIO_TEST_SRC_WAVE_TICKS, "Periodic Ticks", "ticks"},
    {GST_AUDIO_TEST_SRC_WAVE_GAUSSIAN_WHITE_NOISE, "White Gaussian noise",
        "gaussian-noise"},
    {GST_AUDIO_TEST_SRC_WAVE_RED_NOISE, "Red (brownian) noise", "red-noise"},
    {GST_AUDIO_TEST_SRC_WAVE_BLUE_NOISE, "Blue noise", "blue-noise"},
    {GST_AUDIO_TEST_SRC_WAVE_VIOLET_NOISE, "Violet noise", "violet-noise"},
    {0, NULL, NULL},
  };

  if (G_UNLIKELY (audiostestsrc_wave_type == 0)) {
    audiostestsrc_wave_type = g_enum_register_static ("GstAudioTestSrcWave",
        audiostestsrc_waves);
  }
  return audiostestsrc_wave_type;
}

static void gst_audio_test_src_finalize (GObject * object);

static void gst_audio_test_src_set_property (GObject * object,
    guint prop_id, const GValue * value, GParamSpec * pspec);
static void gst_audio_test_src_get_property (GObject * object,
    guint prop_id, GValue * value, GParamSpec * pspec);

static gboolean gst_audio_test_src_setcaps (GstBaseSrc * basesrc,
    GstCaps * caps);
static GstCaps *gst_audio_test_src_fixate (GstBaseSrc * bsrc, GstCaps * caps);

static gboolean gst_audio_test_src_is_seekable (GstBaseSrc * basesrc);
static gboolean gst_audio_test_src_do_seek (GstBaseSrc * basesrc,
    GstSegment * segment);
static gboolean gst_audio_test_src_query (GstBaseSrc * basesrc,
    GstQuery * query);

static void gst_audio_test_src_change_wave (GstAudioTestSrc * src);

static void gst_audio_test_src_get_times (GstBaseSrc * basesrc,
    GstBuffer * buffer, GstClockTime * start, GstClockTime * end);
static gboolean gst_audio_test_src_start (GstBaseSrc * basesrc);
static gboolean gst_audio_test_src_stop (GstBaseSrc * basesrc);
static GstFlowReturn gst_audio_test_src_fill (GstBaseSrc * basesrc,
    guint64 offset, guint length, GstBuffer * buffer);

static void
gst_audio_test_src_class_init (GstAudioTestSrcClass * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *gstelement_class;
  GstBaseSrcClass *gstbasesrc_class;

  gobject_class = (GObjectClass *) klass;
  gstelement_class = (GstElementClass *) klass;
  gstbasesrc_class = (GstBaseSrcClass *) klass;

  gobject_class->set_property = gst_audio_test_src_set_property;
  gobject_class->get_property = gst_audio_test_src_get_property;
  gobject_class->finalize = gst_audio_test_src_finalize;

  g_object_class_install_property (gobject_class, PROP_SAMPLES_PER_BUFFER,
      g_param_spec_int ("samplesperbuffer", "Samples per buffer",
          "Number of samples in each outgoing buffer",
          1, G_MAXINT, DEFAULT_SAMPLES_PER_BUFFER,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_WAVE,
      g_param_spec_enum ("wave", "Waveform", "Oscillator waveform",
          GST_TYPE_AUDIO_TEST_SRC_WAVE, GST_AUDIO_TEST_SRC_WAVE_SINE,
          G_PARAM_READWRITE | GST_PARAM_CONTROLLABLE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_FREQ,
      g_param_spec_double ("freq", "Frequency", "Frequency of test signal. "
          "The sample rate needs to be at least 2 times higher.",
          0.0, (gdouble) G_MAXINT / 2, DEFAULT_FREQ,
          G_PARAM_READWRITE | GST_PARAM_CONTROLLABLE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_VOLUME,
      g_param_spec_double ("volume", "Volume", "Volume of test signal", 0.0,
          1.0, DEFAULT_VOLUME,
          G_PARAM_READWRITE | GST_PARAM_CONTROLLABLE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_IS_LIVE,
      g_param_spec_boolean ("is-live", "Is Live",
          "Whether to act as a live source", DEFAULT_IS_LIVE,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (G_OBJECT_CLASS (klass),
      PROP_TIMESTAMP_OFFSET, g_param_spec_int64 ("timestamp-offset",
          "Timestamp offset",
          "An offset added to timestamps set on buffers (in ns)", G_MININT64,
          G_MAXINT64, DEFAULT_TIMESTAMP_OFFSET,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_SINE_PERIODS_PER_TICK,
      g_param_spec_uint ("sine-periods-per-tick", "Sine periods per tick",
          "Number of sine wave periods in one tick. Only used if wave = ticks.",
          1, G_MAXUINT, DEFAULT_SINE_PERIODS_PER_TICK,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_TICK_INTERVAL,
      g_param_spec_uint64 ("tick-interval", "Time between ticks",
          "Distance between start of current and start of next tick, in nanoseconds.",
          1, G_MAXUINT64, DEFAULT_TIME_BETWEEN_TICKS,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_MARKER_TICK_PERIOD,
      g_param_spec_uint ("marker-tick-period", "Marker tick period",
          "Make every Nth tick a marker tick (= a tick with different volume). "
          "Only used if wave = ticks. 0 = no marker ticks.",
          0, G_MAXUINT, DEFAULT_MARKER_TICK_PERIOD,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_MARKER_TICK_VOLUME,
      g_param_spec_double ("marker-tick-volume", "Marker tick volume",
          "Volume of marker ticks. Only used if wave = ticks and"
          "marker-tick-period is set to a nonzero value.",
          0.0, 1.0, DEFAULT_MARKER_TICK_VOLUME,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_APPLY_TICK_RAMP,
      g_param_spec_boolean ("apply-tick-ramp", "Apply tick ramp",
          "Apply ramp to tick samples", DEFAULT_APPLY_TICK_RAMP,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_CAN_ACTIVATE_PUSH,
      g_param_spec_boolean ("can-activate-push", "Can activate push",
          "Can activate in push mode", DEFAULT_CAN_ACTIVATE_PUSH,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_CAN_ACTIVATE_PULL,
      g_param_spec_boolean ("can-activate-pull", "Can activate pull",
          "Can activate in pull mode", DEFAULT_CAN_ACTIVATE_PULL,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  gst_element_class_add_static_pad_template (gstelement_class,
      &gst_audio_test_src_src_template);
  gst_element_class_set_static_metadata (gstelement_class, "Audio test source",
      "Source/Audio",
      "Creates audio test signals of given frequency and volume",
      "Stefan Kost <ensonic@users.sf.net>");

  gstbasesrc_class->set_caps = GST_DEBUG_FUNCPTR (gst_audio_test_src_setcaps);
  gstbasesrc_class->fixate = GST_DEBUG_FUNCPTR (gst_audio_test_src_fixate);
  gstbasesrc_class->is_seekable =
      GST_DEBUG_FUNCPTR (gst_audio_test_src_is_seekable);
  gstbasesrc_class->do_seek = GST_DEBUG_FUNCPTR (gst_audio_test_src_do_seek);
  gstbasesrc_class->query = GST_DEBUG_FUNCPTR (gst_audio_test_src_query);
  gstbasesrc_class->get_times =
      GST_DEBUG_FUNCPTR (gst_audio_test_src_get_times);
  gstbasesrc_class->start = GST_DEBUG_FUNCPTR (gst_audio_test_src_start);
  gstbasesrc_class->stop = GST_DEBUG_FUNCPTR (gst_audio_test_src_stop);
  gstbasesrc_class->fill = GST_DEBUG_FUNCPTR (gst_audio_test_src_fill);

  gst_type_mark_as_plugin_api (GST_TYPE_AUDIO_TEST_SRC_WAVE, 0);
}

static void
gst_audio_test_src_init (GstAudioTestSrc * src)
{
  src->volume = DEFAULT_VOLUME;
  src->freq = DEFAULT_FREQ;

  /* we operate in time */
  gst_base_src_set_format (GST_BASE_SRC (src), GST_FORMAT_TIME);
  gst_base_src_set_live (GST_BASE_SRC (src), DEFAULT_IS_LIVE);

  src->samples_per_buffer = DEFAULT_SAMPLES_PER_BUFFER;
  src->generate_samples_per_buffer = src->samples_per_buffer;
  src->timestamp_offset = DEFAULT_TIMESTAMP_OFFSET;
  src->can_activate_pull = DEFAULT_CAN_ACTIVATE_PULL;

  src->sine_periods_per_tick = DEFAULT_SINE_PERIODS_PER_TICK;
  src->tick_interval = DEFAULT_TIME_BETWEEN_TICKS;
  src->marker_tick_period = DEFAULT_MARKER_TICK_PERIOD;
  src->marker_tick_volume = DEFAULT_MARKER_TICK_VOLUME;
  src->apply_tick_ramp = DEFAULT_APPLY_TICK_RAMP;

  src->gen = NULL;

  src->wave = DEFAULT_WAVE;
  gst_base_src_set_blocksize (GST_BASE_SRC (src), -1);
}

static void
gst_audio_test_src_finalize (GObject * object)
{
  GstAudioTestSrc *src = GST_AUDIO_TEST_SRC (object);

  if (src->gen)
    g_rand_free (src->gen);
  src->gen = NULL;
  g_free (src->tmp);
  src->tmp = NULL;
  src->tmpsize = 0;

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static GstCaps *
gst_audio_test_src_fixate (GstBaseSrc * bsrc, GstCaps * caps)
{
  GstAudioTestSrc *src = GST_AUDIO_TEST_SRC (bsrc);
  GstStructure *structure;
  gint channels, rate;

  caps = gst_caps_make_writable (caps);

  structure = gst_caps_get_structure (caps, 0);

  GST_DEBUG_OBJECT (src, "fixating samplerate to %d", GST_AUDIO_DEF_RATE);

  rate = MAX (GST_AUDIO_DEF_RATE, src->freq * 2);
  gst_structure_fixate_field_nearest_int (structure, "rate", rate);

  gst_structure_fixate_field_string (structure, "format", DEFAULT_FORMAT_STR);

  gst_structure_fixate_field_string (structure, "layout", "interleaved");

  /* fixate to mono unless downstream requires stereo, for backwards compat */
  gst_structure_fixate_field_nearest_int (structure, "channels", 1);

  if (gst_structure_get_int (structure, "channels", &channels) && channels > 2) {
    if (!gst_structure_has_field_typed (structure, "channel-mask",
            GST_TYPE_BITMASK)) {
      guint64 mask = gst_audio_channel_get_fallback_mask (channels);
      gst_structure_set (structure, "channel-mask", GST_TYPE_BITMASK, mask,
          NULL);
    }
  }

  caps = GST_BASE_SRC_CLASS (parent_class)->fixate (bsrc, caps);

  return caps;
}

static gboolean
gst_audio_test_src_setcaps (GstBaseSrc * basesrc, GstCaps * caps)
{
  GstAudioTestSrc *src = GST_AUDIO_TEST_SRC (basesrc);
  GstAudioInfo info;

  if (!gst_audio_info_from_caps (&info, caps))
    goto invalid_caps;

  GST_DEBUG_OBJECT (src, "negotiated to caps %" GST_PTR_FORMAT, caps);

  src->info = info;

  gst_base_src_set_blocksize (basesrc,
      GST_AUDIO_INFO_BPF (&info) * src->samples_per_buffer);
  gst_audio_test_src_change_wave (src);

  return TRUE;

  /* ERROR */
invalid_caps:
  {
    GST_ERROR_OBJECT (basesrc, "received invalid caps");
    return FALSE;
  }
}

static gboolean
gst_audio_test_src_query (GstBaseSrc * basesrc, GstQuery * query)
{
  GstAudioTestSrc *src = GST_AUDIO_TEST_SRC (basesrc);
  gboolean res = FALSE;

  switch (GST_QUERY_TYPE (query)) {
    case GST_QUERY_CONVERT:
    {
      GstFormat src_fmt, dest_fmt;
      gint64 src_val, dest_val;

      gst_query_parse_convert (query, &src_fmt, &src_val, &dest_fmt, &dest_val);

      if (!gst_audio_info_convert (&src->info, src_fmt, src_val, dest_fmt,
              &dest_val))
        goto error;

      gst_query_set_convert (query, src_fmt, src_val, dest_fmt, dest_val);
      res = TRUE;
      break;
    }
    case GST_QUERY_SCHEDULING:
    {
      /* if we can operate in pull mode */
      gst_query_set_scheduling (query, GST_SCHEDULING_FLAG_SEEKABLE, 1, -1, 0);
      gst_query_add_scheduling_mode (query, GST_PAD_MODE_PUSH);
      if (src->can_activate_pull)
        gst_query_add_scheduling_mode (query, GST_PAD_MODE_PULL);

      res = TRUE;
      break;
    }
    case GST_QUERY_LATENCY:
    {
      if (src->info.rate > 0) {
        GstClockTime latency;

        latency =
            gst_util_uint64_scale (src->generate_samples_per_buffer, GST_SECOND,
            src->info.rate);
        gst_query_set_latency (query,
            gst_base_src_is_live (GST_BASE_SRC_CAST (src)), latency,
            GST_CLOCK_TIME_NONE);
        GST_DEBUG_OBJECT (src, "Reporting latency of %" GST_TIME_FORMAT,
            GST_TIME_ARGS (latency));
        res = TRUE;
      }
      break;
    }
    default:
      res = GST_BASE_SRC_CLASS (parent_class)->query (basesrc, query);
      break;
  }

  return res;
  /* ERROR */
error:
  {
    GST_DEBUG_OBJECT (src, "query failed");
    return FALSE;
  }
}

#define DEFINE_SINE(type,scale) \
static void \
gst_audio_test_src_create_sine_##type (GstAudioTestSrc * src, g##type * samples) \
{ \
  gint i, c, channels, channel_step, sample_step; \
  gdouble step, amp; \
  g##type *ptr; \
  \
  channels = GST_AUDIO_INFO_CHANNELS (&src->info); \
  if (GST_AUDIO_INFO_LAYOUT (&src->info) == GST_AUDIO_LAYOUT_INTERLEAVED) { \
    channel_step = 1; \
    sample_step = channels; \
  } else { \
    channel_step = src->generate_samples_per_buffer; \
    sample_step = 1; \
  } \
  step = M_PI_M2 * src->freq / GST_AUDIO_INFO_RATE (&src->info); \
  amp = src->volume * scale; \
  \
  for (i = 0; i < src->generate_samples_per_buffer; i++) { \
    src->accumulator += step; \
    if (src->accumulator >= M_PI_M2) \
      src->accumulator -= M_PI_M2; \
    \
    ptr = samples; \
    for (c = 0; c < channels; ++c) { \
      *ptr = (g##type) (sin (src->accumulator) * amp); \
      ptr += channel_step; \
    } \
    samples += sample_step; \
  } \
}

DEFINE_SINE (int16, 32767.0);
DEFINE_SINE (int32, 2147483647.0);
DEFINE_SINE (float, 1.0);
DEFINE_SINE (double, 1.0);

static const ProcessFunc sine_funcs[] = {
  (ProcessFunc) gst_audio_test_src_create_sine_int16,
  (ProcessFunc) gst_audio_test_src_create_sine_int32,
  (ProcessFunc) gst_audio_test_src_create_sine_float,
  (ProcessFunc) gst_audio_test_src_create_sine_double
};

#define DEFINE_SQUARE(type,scale) \
static void \
gst_audio_test_src_create_square_##type (GstAudioTestSrc * src, g##type * samples) \
{ \
  gint i, c, channels, channel_step, sample_step; \
  gdouble step, amp; \
  g##type *ptr; \
  \
  channels = GST_AUDIO_INFO_CHANNELS (&src->info); \
  if (GST_AUDIO_INFO_LAYOUT (&src->info) == GST_AUDIO_LAYOUT_INTERLEAVED) { \
    channel_step = 1; \
    sample_step = channels; \
  } else { \
    channel_step = src->generate_samples_per_buffer; \
    sample_step = 1; \
  } \
  step = M_PI_M2 * src->freq / GST_AUDIO_INFO_RATE (&src->info); \
  amp = src->volume * scale; \
  \
  for (i = 0; i < src->generate_samples_per_buffer; i++) { \
    src->accumulator += step; \
    if (src->accumulator >= M_PI_M2) \
      src->accumulator -= M_PI_M2; \
    \
    ptr = samples; \
    for (c = 0; c < channels; ++c) { \
      *ptr = (g##type) ((src->accumulator < G_PI) ? amp : -amp); \
      ptr += channel_step; \
    } \
    samples += sample_step; \
  } \
}

DEFINE_SQUARE (int16, 32767.0);
DEFINE_SQUARE (int32, 2147483647.0);
DEFINE_SQUARE (float, 1.0);
DEFINE_SQUARE (double, 1.0);

static const ProcessFunc square_funcs[] = {
  (ProcessFunc) gst_audio_test_src_create_square_int16,
  (ProcessFunc) gst_audio_test_src_create_square_int32,
  (ProcessFunc) gst_audio_test_src_create_square_float,
  (ProcessFunc) gst_audio_test_src_create_square_double
};

#define DEFINE_SAW(type,scale) \
static void \
gst_audio_test_src_create_saw_##type (GstAudioTestSrc * src, g##type * samples) \
{ \
  gint i, c, channels, channel_step, sample_step; \
  gdouble step, amp; \
  g##type *ptr; \
  \
  channels = GST_AUDIO_INFO_CHANNELS (&src->info); \
  if (GST_AUDIO_INFO_LAYOUT (&src->info) == GST_AUDIO_LAYOUT_INTERLEAVED) { \
    channel_step = 1; \
    sample_step = channels; \
  } else { \
    channel_step = src->generate_samples_per_buffer; \
    sample_step = 1; \
  } \
  step = M_PI_M2 * src->freq / GST_AUDIO_INFO_RATE (&src->info); \
  amp = (src->volume * scale) / G_PI; \
  \
  for (i = 0; i < src->generate_samples_per_buffer; i++) { \
    src->accumulator += step; \
    if (src->accumulator >= M_PI_M2) \
      src->accumulator -= M_PI_M2; \
    \
    ptr = samples; \
    if (src->accumulator < G_PI) { \
      for (c = 0; c < channels; ++c) { \
        *ptr = (g##type) (src->accumulator * amp); \
        ptr += channel_step; \
      } \
    } else { \
      for (c = 0; c < channels; ++c) { \
        *ptr = (g##type) ((M_PI_M2 - src->accumulator) * -amp); \
        ptr += channel_step; \
      } \
    } \
    samples += sample_step; \
  } \
}

DEFINE_SAW (int16, 32767.0);
DEFINE_SAW (int32, 2147483647.0);
DEFINE_SAW (float, 1.0);
DEFINE_SAW (double, 1.0);

static const ProcessFunc saw_funcs[] = {
  (ProcessFunc) gst_audio_test_src_create_saw_int16,
  (ProcessFunc) gst_audio_test_src_create_saw_int32,
  (ProcessFunc) gst_audio_test_src_create_saw_float,
  (ProcessFunc) gst_audio_test_src_create_saw_double
};

#define DEFINE_TRIANGLE(type,scale) \
static void \
gst_audio_test_src_create_triangle_##type (GstAudioTestSrc * src, g##type * samples) \
{ \
  gint i, c, channels, channel_step, sample_step; \
  gdouble step, amp; \
  g##type *ptr; \
  \
  channels = GST_AUDIO_INFO_CHANNELS (&src->info); \
  if (GST_AUDIO_INFO_LAYOUT (&src->info) == GST_AUDIO_LAYOUT_INTERLEAVED) { \
    channel_step = 1; \
    sample_step = channels; \
  } else { \
    channel_step = src->generate_samples_per_buffer; \
    sample_step = 1; \
  } \
  step = M_PI_M2 * src->freq / GST_AUDIO_INFO_RATE (&src->info); \
  amp = (src->volume * scale) / G_PI_2; \
  \
  for (i = 0; i < src->generate_samples_per_buffer; i++) { \
    src->accumulator += step; \
    if (src->accumulator >= M_PI_M2) \
      src->accumulator -= M_PI_M2; \
    \
    ptr = samples; \
    if (src->accumulator < (G_PI_2)) { \
      for (c = 0; c < channels; ++c) { \
        *ptr = (g##type) (src->accumulator * amp); \
        ptr += channel_step; \
      } \
    } else if (src->accumulator < (G_PI * 1.5)) { \
      for (c = 0; c < channels; ++c) { \
        *ptr = (g##type) ((src->accumulator - G_PI) * -amp); \
        ptr += channel_step; \
      } \
    } else { \
      for (c = 0; c < channels; ++c) { \
        *ptr = (g##type) ((M_PI_M2 - src->accumulator) * -amp); \
        ptr += channel_step; \
      } \
    } \
    samples += sample_step; \
  } \
}

DEFINE_TRIANGLE (int16, 32767.0);
DEFINE_TRIANGLE (int32, 2147483647.0);
DEFINE_TRIANGLE (float, 1.0);
DEFINE_TRIANGLE (double, 1.0);

static const ProcessFunc triangle_funcs[] = {
  (ProcessFunc) gst_audio_test_src_create_triangle_int16,
  (ProcessFunc) gst_audio_test_src_create_triangle_int32,
  (ProcessFunc) gst_audio_test_src_create_triangle_float,
  (ProcessFunc) gst_audio_test_src_create_triangle_double
};

#define DEFINE_SILENCE(type) \
static void \
gst_audio_test_src_create_silence_##type (GstAudioTestSrc * src, g##type * samples) \
{ \
  memset (samples, 0, src->generate_samples_per_buffer * sizeof (g##type) * src->info.channels); \
}

DEFINE_SILENCE (int16);
DEFINE_SILENCE (int32);
DEFINE_SILENCE (float);
DEFINE_SILENCE (double);

static const ProcessFunc silence_funcs[] = {
  (ProcessFunc) gst_audio_test_src_create_silence_int16,
  (ProcessFunc) gst_audio_test_src_create_silence_int32,
  (ProcessFunc) gst_audio_test_src_create_silence_float,
  (ProcessFunc) gst_audio_test_src_create_silence_double
};

#define DEFINE_WHITE_NOISE(type,scale) \
static void \
gst_audio_test_src_create_white_noise_##type (GstAudioTestSrc * src, g##type * samples) \
{ \
  gint i, c, channel_step, sample_step; \
  g##type *ptr; \
  gdouble amp = (src->volume * scale); \
  gint channels = GST_AUDIO_INFO_CHANNELS (&src->info); \
  \
  if (GST_AUDIO_INFO_LAYOUT (&src->info) == GST_AUDIO_LAYOUT_INTERLEAVED) { \
    channel_step = 1; \
    sample_step = channels; \
  } else { \
    channel_step = src->generate_samples_per_buffer; \
    sample_step = 1; \
  } \
  \
  for (i = 0; i < src->generate_samples_per_buffer; i++) { \
    ptr = samples; \
    for (c = 0; c < channels; ++c) { \
      *ptr = (g##type) (amp * g_rand_double_range (src->gen, -1.0, 1.0)); \
      ptr += channel_step; \
    } \
    samples += sample_step; \
  } \
}

DEFINE_WHITE_NOISE (int16, 32767.0);
DEFINE_WHITE_NOISE (int32, 2147483647.0);
DEFINE_WHITE_NOISE (float, 1.0);
DEFINE_WHITE_NOISE (double, 1.0);

static const ProcessFunc white_noise_funcs[] = {
  (ProcessFunc) gst_audio_test_src_create_white_noise_int16,
  (ProcessFunc) gst_audio_test_src_create_white_noise_int32,
  (ProcessFunc) gst_audio_test_src_create_white_noise_float,
  (ProcessFunc) gst_audio_test_src_create_white_noise_double
};

/* pink noise calculation is based on
 * http://www.firstpr.com.au/dsp/pink-noise/phil_burk_19990905_patest_pink.c
 * which has been released under public domain
 * Many thanks Phil!
 */
static void
gst_audio_test_src_init_pink_noise (GstAudioTestSrc * src)
{
  gint i;
  gint num_rows = 12;           /* arbitrary: 1 .. PINK_MAX_RANDOM_ROWS */
  glong pmax;

  src->pink.index = 0;
  src->pink.index_mask = (1 << num_rows) - 1;
  /* calculate maximum possible signed random value.
   * Extra 1 for white noise always added. */
  pmax = (num_rows + 1) * (1 << (PINK_RANDOM_BITS - 1));
  src->pink.scalar = 1.0f / pmax;
  /* Initialize rows. */
  for (i = 0; i < num_rows; i++)
    src->pink.rows[i] = 0;
  src->pink.running_sum = 0;
}

/* Generate Pink noise values between -1.0 and +1.0 */
static gdouble
gst_audio_test_src_generate_pink_noise_value (GstAudioTestSrc * src)
{
  GstPinkNoise *pink = &src->pink;
  glong new_random;
  glong sum;

  /* Increment and mask index. */
  pink->index = (pink->index + 1) & pink->index_mask;

  /* If index is zero, don't update any random values. */
  if (pink->index != 0) {
    /* Determine how many trailing zeros in PinkIndex. */
    /* This algorithm will hang if n==0 so test first. */
    gint num_zeros = 0;
    gint n = pink->index;

    while ((n & 1) == 0) {
      n = n >> 1;
      num_zeros++;
    }

    /* Replace the indexed ROWS random value.
     * Subtract and add back to RunningSum instead of adding all the random
     * values together. Only one changes each time.
     */
    pink->running_sum -= pink->rows[num_zeros];
    new_random = 32768.0 - (65536.0 * (gulong) g_rand_int (src->gen)
        / (G_MAXUINT32 + 1.0));
    pink->running_sum += new_random;
    pink->rows[num_zeros] = new_random;
  }

  /* Add extra white noise value. */
  new_random = 32768.0 - (65536.0 * (gulong) g_rand_int (src->gen)
      / (G_MAXUINT32 + 1.0));
  sum = pink->running_sum + new_random;

  /* Scale to range of -1.0 to 0.9999. */
  return (pink->scalar * sum);
}

#define DEFINE_PINK(type, scale) \
static void \
gst_audio_test_src_create_pink_noise_##type (GstAudioTestSrc * src, g##type * samples) \
{ \
  gint i, c, channels, channel_step, sample_step; \
  gdouble amp; \
  g##type *ptr; \
  \
  amp = src->volume * scale; \
  channels = GST_AUDIO_INFO_CHANNELS (&src->info); \
  if (GST_AUDIO_INFO_LAYOUT (&src->info) == GST_AUDIO_LAYOUT_INTERLEAVED) { \
    channel_step = 1; \
    sample_step = channels; \
  } else { \
    channel_step = src->generate_samples_per_buffer; \
    sample_step = 1; \
  } \
  \
  for (i = 0; i < src->generate_samples_per_buffer; i++) { \
    ptr = samples; \
    for (c = 0; c < channels; ++c) { \
      *ptr = (g##type) (gst_audio_test_src_generate_pink_noise_value (src) * amp); \
      ptr += channel_step; \
    } \
    samples += sample_step; \
  } \
}

DEFINE_PINK (int16, 32767.0);
DEFINE_PINK (int32, 2147483647.0);
DEFINE_PINK (float, 1.0);
DEFINE_PINK (double, 1.0);

static const ProcessFunc pink_noise_funcs[] = {
  (ProcessFunc) gst_audio_test_src_create_pink_noise_int16,
  (ProcessFunc) gst_audio_test_src_create_pink_noise_int32,
  (ProcessFunc) gst_audio_test_src_create_pink_noise_float,
  (ProcessFunc) gst_audio_test_src_create_pink_noise_double
};

static void
gst_audio_test_src_init_sine_table (GstAudioTestSrc * src, gboolean use_volume)
{
  gint i;
  gdouble ang = 0.0;
  gdouble step = M_PI_M2 / 1024.0;
  gdouble amp = use_volume ? src->volume : 1.0;

  for (i = 0; i < 1024; i++) {
    src->wave_table[i] = sin (ang) * amp;
    ang += step;
  }
}

#define DEFINE_SINE_TABLE(type,scale) \
static void \
gst_audio_test_src_create_sine_table_##type (GstAudioTestSrc * src, g##type * samples) \
{ \
  gint i, c, channels, channel_step, sample_step; \
  gdouble step, scl; \
  g##type *ptr; \
  \
  channels = GST_AUDIO_INFO_CHANNELS (&src->info); \
  if (GST_AUDIO_INFO_LAYOUT (&src->info) == GST_AUDIO_LAYOUT_INTERLEAVED) { \
    channel_step = 1; \
    sample_step = channels; \
  } else { \
    channel_step = src->generate_samples_per_buffer; \
    sample_step = 1; \
  } \
  step = M_PI_M2 * src->freq / GST_AUDIO_INFO_RATE (&src->info); \
  scl = 1024.0 / M_PI_M2; \
  \
  for (i = 0; i < src->generate_samples_per_buffer; i++) { \
    src->accumulator += step; \
    if (src->accumulator >= M_PI_M2) \
      src->accumulator -= M_PI_M2; \
    \
    ptr = samples; \
    for (c = 0; c < channels; ++c) { \
      *ptr = (g##type) scale * src->wave_table[(gint) (src->accumulator * scl)]; \
      ptr += channel_step; \
    } \
    samples += sample_step; \
  } \
}

DEFINE_SINE_TABLE (int16, 32767.0);
DEFINE_SINE_TABLE (int32, 2147483647.0);
DEFINE_SINE_TABLE (float, 1.0);
DEFINE_SINE_TABLE (double, 1.0);

static const ProcessFunc sine_table_funcs[] = {
  (ProcessFunc) gst_audio_test_src_create_sine_table_int16,
  (ProcessFunc) gst_audio_test_src_create_sine_table_int32,
  (ProcessFunc) gst_audio_test_src_create_sine_table_float,
  (ProcessFunc) gst_audio_test_src_create_sine_table_double
};

static inline gdouble
calc_scaled_tick_volume (GstAudioTestSrc * src, gdouble scale)
{
  gdouble vol;
  vol = ((src->marker_tick_period > 0)
      && ((src->tick_counter % src->marker_tick_period) == 0))
      ? src->marker_tick_volume : src->volume;
  return vol * scale;
}


#define DEFINE_TICKS(type,scale) \
static void \
gst_audio_test_src_create_tick_##type (GstAudioTestSrc * src, g##type * samples) \
{ \
  gint i, c, channels, samplerate, samplemod, channel_step, sample_step; \
  gint num_nonzero_samples, num_ramp_samples, end_ramp_offset; \
  gdouble step, scl; \
  gdouble volscale; \
  g##type *ptr; \
  \
  volscale = calc_scaled_tick_volume (src, scale); \
  channels = GST_AUDIO_INFO_CHANNELS (&src->info); \
  if (GST_AUDIO_INFO_LAYOUT (&src->info) == GST_AUDIO_LAYOUT_INTERLEAVED) { \
    channel_step = 1; \
    sample_step = channels; \
  } else { \
    channel_step = src->generate_samples_per_buffer; \
    sample_step = 1; \
  } \
  samplerate = GST_AUDIO_INFO_RATE (&src->info); \
  step = M_PI_M2 * src->freq / samplerate; \
  num_nonzero_samples = samplerate * src->sine_periods_per_tick / src->freq; \
  scl = 1024.0 / M_PI_M2; \
  num_ramp_samples = src->apply_tick_ramp ? (samplerate / src->freq) : 0; \
  end_ramp_offset = num_nonzero_samples - num_ramp_samples; \
  \
  for (i = 0; i < src->generate_samples_per_buffer; i++) { \
    samplemod = (src->next_sample + i)%src->samples_between_ticks; \
    \
    ptr = samples; \
    if (samplemod == 0) { \
      src->accumulator = 0; \
      src->tick_counter++; \
      volscale = calc_scaled_tick_volume (src, scale); \
      for (c = 0; c < channels; ++c) { \
        *ptr = 0; \
        ptr += channel_step; \
      } \
    } else if (samplemod < num_nonzero_samples)  { \
      gdouble ramp; \
      if (num_ramp_samples > 0) { \
        ramp = \
            (samplemod < num_ramp_samples) ? (((gdouble)samplemod) / num_ramp_samples) : \
            (samplemod >= end_ramp_offset) ? (((gdouble)(num_nonzero_samples - samplemod)) / num_ramp_samples) \
            : 1.0; \
        if (ramp > 1.0) \
          ramp = 1.0; \
        ramp *= ramp * ramp; \
      } else \
        ramp = 1.0; \
      \
      for (c = 0; c < channels; ++c) { \
        *ptr = \
            (g##type) volscale * ramp * src->wave_table[(gint) (src->accumulator * scl)]; \
        ptr += channel_step; \
      } \
    } else { \
      for (c = 0; c < channels; ++c) { \
        *ptr = 0; \
        ptr += channel_step; \
      } \
    } \
    \
    src->accumulator += step; \
    if (src->accumulator >= M_PI_M2) \
      src->accumulator -= M_PI_M2; \
    \
    samples += sample_step; \
  } \
}

DEFINE_TICKS (int16, 32767.0);
DEFINE_TICKS (int32, 2147483647.0);
DEFINE_TICKS (float, 1.0);
DEFINE_TICKS (double, 1.0);

static const ProcessFunc tick_funcs[] = {
  (ProcessFunc) gst_audio_test_src_create_tick_int16,
  (ProcessFunc) gst_audio_test_src_create_tick_int32,
  (ProcessFunc) gst_audio_test_src_create_tick_float,
  (ProcessFunc) gst_audio_test_src_create_tick_double
};

/* Gaussian white noise using Box-Muller algorithm.  unit variance
 * normally-distributed random numbers are generated in pairs as the real
 * and imaginary parts of a complex random variable with
 * uniformly-distributed argument and \chi^{2}-distributed modulus.
 */

#define DEFINE_GAUSSIAN_WHITE_NOISE(type,scale) \
static void \
gst_audio_test_src_create_gaussian_white_noise_##type (GstAudioTestSrc * src, g##type * samples) \
{ \
  gint i, c, channel_step, sample_step; \
  g##type *ptr; \
  gdouble amp = (src->volume * scale); \
  gint channels = GST_AUDIO_INFO_CHANNELS (&src->info); \
  \
  if (GST_AUDIO_INFO_LAYOUT (&src->info) == GST_AUDIO_LAYOUT_INTERLEAVED) { \
    channel_step = 1; \
    sample_step = channels; \
  } else { \
    channel_step = src->generate_samples_per_buffer; \
    sample_step = 1; \
  } \
  \
  for (i = 0; i < src->generate_samples_per_buffer; i++) { \
    ptr = samples; \
    for (c = 0; c < channels; ++c) { \
      gdouble mag = sqrt (-2 * log (1.0 - g_rand_double (src->gen))); \
      gdouble phs = g_rand_double_range (src->gen, 0.0, M_PI_M2); \
      \
      *ptr = (g##type) (amp * mag * cos (phs)); \
      ptr += channel_step; \
      if (++c >= channels) \
        break; \
      *ptr = (g##type) (amp * mag * sin (phs)); \
      ptr += channel_step; \
    } \
    samples += sample_step; \
  } \
}

DEFINE_GAUSSIAN_WHITE_NOISE (int16, 32767.0);
DEFINE_GAUSSIAN_WHITE_NOISE (int32, 2147483647.0);
DEFINE_GAUSSIAN_WHITE_NOISE (float, 1.0);
DEFINE_GAUSSIAN_WHITE_NOISE (double, 1.0);

static const ProcessFunc gaussian_white_noise_funcs[] = {
  (ProcessFunc) gst_audio_test_src_create_gaussian_white_noise_int16,
  (ProcessFunc) gst_audio_test_src_create_gaussian_white_noise_int32,
  (ProcessFunc) gst_audio_test_src_create_gaussian_white_noise_float,
  (ProcessFunc) gst_audio_test_src_create_gaussian_white_noise_double
};

/* Brownian (Red) Noise: noise where the power density decreases by 6 dB per
 * octave with increasing frequency
 *
 * taken from http://vellocet.com/dsp/noise/VRand.html
 * by Andrew Simper of Vellocet (andy@vellocet.com)
 */

#define DEFINE_RED_NOISE(type,scale) \
static void \
gst_audio_test_src_create_red_noise_##type (GstAudioTestSrc * src, g##type * samples) \
{ \
  gint i, c, channel_step, sample_step; \
  g##type *ptr; \
  gdouble amp = (src->volume * scale); \
  gdouble state = src->red.state; \
  gint channels = GST_AUDIO_INFO_CHANNELS (&src->info); \
  \
  if (GST_AUDIO_INFO_LAYOUT (&src->info) == GST_AUDIO_LAYOUT_INTERLEAVED) { \
    channel_step = 1; \
    sample_step = channels; \
  } else { \
    channel_step = src->generate_samples_per_buffer; \
    sample_step = 1; \
  } \
  \
  for (i = 0; i < src->generate_samples_per_buffer; i++) { \
    ptr = samples; \
    for (c = 0; c < channels; ++c) { \
      while (TRUE) { \
        gdouble r = g_rand_double_range (src->gen, -1.0, 1.0); \
        state += r; \
        if (state < -8.0f || state > 8.0f) state -= r; \
        else break; \
      } \
      *ptr = (g##type) (amp * state * 0.0625f); /* /16.0 */ \
      ptr += channel_step; \
    } \
    samples += sample_step; \
  } \
  src->red.state = state; \
}

DEFINE_RED_NOISE (int16, 32767.0);
DEFINE_RED_NOISE (int32, 2147483647.0);
DEFINE_RED_NOISE (float, 1.0);
DEFINE_RED_NOISE (double, 1.0);

static const ProcessFunc red_noise_funcs[] = {
  (ProcessFunc) gst_audio_test_src_create_red_noise_int16,
  (ProcessFunc) gst_audio_test_src_create_red_noise_int32,
  (ProcessFunc) gst_audio_test_src_create_red_noise_float,
  (ProcessFunc) gst_audio_test_src_create_red_noise_double
};

/* Blue Noise: apply spectral inversion to pink noise */

#define DEFINE_BLUE_NOISE(type) \
static void \
gst_audio_test_src_create_blue_noise_##type (GstAudioTestSrc * src, g##type * samples) \
{ \
  gint i, c, channel_step, sample_step; \
  static gdouble flip=1.0; \
  gint channels = GST_AUDIO_INFO_CHANNELS (&src->info); \
  g##type *ptr; \
  \
  if (GST_AUDIO_INFO_LAYOUT (&src->info) == GST_AUDIO_LAYOUT_INTERLEAVED) { \
    channel_step = 1; \
    sample_step = channels; \
  } else { \
    channel_step = src->generate_samples_per_buffer; \
    sample_step = 1; \
  } \
  \
  gst_audio_test_src_create_pink_noise_##type (src, samples); \
  for (i = 0; i < src->generate_samples_per_buffer; i++) { \
    ptr = samples; \
    for (c = 0; c < channels; ++c) { \
      *ptr *= flip; \
      ptr += channel_step; \
    } \
    flip *= -1.0; \
    samples += sample_step; \
  } \
}

DEFINE_BLUE_NOISE (int16);
DEFINE_BLUE_NOISE (int32);
DEFINE_BLUE_NOISE (float);
DEFINE_BLUE_NOISE (double);

static const ProcessFunc blue_noise_funcs[] = {
  (ProcessFunc) gst_audio_test_src_create_blue_noise_int16,
  (ProcessFunc) gst_audio_test_src_create_blue_noise_int32,
  (ProcessFunc) gst_audio_test_src_create_blue_noise_float,
  (ProcessFunc) gst_audio_test_src_create_blue_noise_double
};


/* Violet Noise: apply spectral inversion to red noise */

#define DEFINE_VIOLET_NOISE(type) \
static void \
gst_audio_test_src_create_violet_noise_##type (GstAudioTestSrc * src, g##type * samples) \
{ \
  gint i, c, channel_step, sample_step; \
  static gdouble flip=1.0; \
  gint channels = GST_AUDIO_INFO_CHANNELS (&src->info); \
  g##type *ptr; \
  \
  if (GST_AUDIO_INFO_LAYOUT (&src->info) == GST_AUDIO_LAYOUT_INTERLEAVED) { \
    channel_step = 1; \
    sample_step = channels; \
  } else { \
    channel_step = src->generate_samples_per_buffer; \
    sample_step = 1; \
  } \
  \
  gst_audio_test_src_create_red_noise_##type (src, samples); \
  for (i = 0; i < src->generate_samples_per_buffer; i++) { \
    ptr = samples; \
    for (c = 0; c < channels; ++c) { \
      *ptr *= flip; \
      ptr += channel_step; \
    } \
    flip *= -1.0; \
    samples += sample_step; \
  } \
}

DEFINE_VIOLET_NOISE (int16);
DEFINE_VIOLET_NOISE (int32);
DEFINE_VIOLET_NOISE (float);
DEFINE_VIOLET_NOISE (double);

static const ProcessFunc violet_noise_funcs[] = {
  (ProcessFunc) gst_audio_test_src_create_violet_noise_int16,
  (ProcessFunc) gst_audio_test_src_create_violet_noise_int32,
  (ProcessFunc) gst_audio_test_src_create_violet_noise_float,
  (ProcessFunc) gst_audio_test_src_create_violet_noise_double
};


/*
 * gst_audio_test_src_change_wave:
 * Assign function pointer of wave generator.
 */
static void
gst_audio_test_src_change_wave (GstAudioTestSrc * src)
{
  gint idx;

  src->pack_func = NULL;
  src->process = NULL;

  /* not negotiated yet? */
  if (src->info.finfo == NULL)
    return;

  switch (GST_AUDIO_FORMAT_INFO_FORMAT (src->info.finfo)) {
    case GST_AUDIO_FORMAT_S16:
      idx = 0;
      break;
    case GST_AUDIO_FORMAT_S32:
      idx = 1;
      break;
    case GST_AUDIO_FORMAT_F32:
      idx = 2;
      break;
    case GST_AUDIO_FORMAT_F64:
      idx = 3;
      break;
    default:
      /* special format */
      switch (src->info.finfo->unpack_format) {
        case GST_AUDIO_FORMAT_S32:
          idx = 1;
          src->pack_func = src->info.finfo->pack_func;
          src->pack_size = sizeof (gint32);
          break;
        case GST_AUDIO_FORMAT_F64:
          idx = 3;
          src->pack_func = src->info.finfo->pack_func;
          src->pack_size = sizeof (gdouble);
          break;
        default:
          g_assert_not_reached ();
          return;
      }
  }

  switch (src->wave) {
    case GST_AUDIO_TEST_SRC_WAVE_SINE:
      src->process = sine_funcs[idx];
      break;
    case GST_AUDIO_TEST_SRC_WAVE_SQUARE:
      src->process = square_funcs[idx];
      break;
    case GST_AUDIO_TEST_SRC_WAVE_SAW:
      src->process = saw_funcs[idx];
      break;
    case GST_AUDIO_TEST_SRC_WAVE_TRIANGLE:
      src->process = triangle_funcs[idx];
      break;
    case GST_AUDIO_TEST_SRC_WAVE_SILENCE:
      src->process = silence_funcs[idx];
      break;
    case GST_AUDIO_TEST_SRC_WAVE_WHITE_NOISE:
      if (!(src->gen))
        src->gen = g_rand_new ();
      src->process = white_noise_funcs[idx];
      break;
    case GST_AUDIO_TEST_SRC_WAVE_PINK_NOISE:
      if (!(src->gen))
        src->gen = g_rand_new ();
      gst_audio_test_src_init_pink_noise (src);
      src->process = pink_noise_funcs[idx];
      break;
    case GST_AUDIO_TEST_SRC_WAVE_SINE_TAB:
      gst_audio_test_src_init_sine_table (src, TRUE);
      src->process = sine_table_funcs[idx];
      break;
    case GST_AUDIO_TEST_SRC_WAVE_TICKS:
      gst_audio_test_src_init_sine_table (src, FALSE);
      src->process = tick_funcs[idx];
      src->samples_between_ticks =
          gst_util_uint64_scale_int (src->tick_interval,
          GST_AUDIO_INFO_RATE (&(src->info)), GST_SECOND);
      break;
    case GST_AUDIO_TEST_SRC_WAVE_GAUSSIAN_WHITE_NOISE:
      if (!(src->gen))
        src->gen = g_rand_new ();
      src->process = gaussian_white_noise_funcs[idx];
      break;
    case GST_AUDIO_TEST_SRC_WAVE_RED_NOISE:
      if (!(src->gen))
        src->gen = g_rand_new ();
      src->red.state = 0.0;
      src->process = red_noise_funcs[idx];
      break;
    case GST_AUDIO_TEST_SRC_WAVE_BLUE_NOISE:
      if (!(src->gen))
        src->gen = g_rand_new ();
      gst_audio_test_src_init_pink_noise (src);
      src->process = blue_noise_funcs[idx];
      break;
    case GST_AUDIO_TEST_SRC_WAVE_VIOLET_NOISE:
      if (!(src->gen))
        src->gen = g_rand_new ();
      src->red.state = 0.0;
      src->process = violet_noise_funcs[idx];
      break;
    default:
      GST_ERROR ("invalid wave-form");
      break;
  }
}

/*
 * gst_audio_test_src_change_volume:
 * Recalc wave tables for precalculated waves.
 */
static void
gst_audio_test_src_change_volume (GstAudioTestSrc * src)
{
  switch (src->wave) {
    case GST_AUDIO_TEST_SRC_WAVE_SINE_TAB:
      gst_audio_test_src_init_sine_table (src, TRUE);
      break;
    default:
      break;
  }
}

static void
gst_audio_test_src_get_times (GstBaseSrc * basesrc, GstBuffer * buffer,
    GstClockTime * start, GstClockTime * end)
{
  /* for live sources, sync on the timestamp of the buffer */
  if (gst_base_src_is_live (basesrc)) {
    GstClockTime timestamp = GST_BUFFER_TIMESTAMP (buffer);

    if (GST_CLOCK_TIME_IS_VALID (timestamp)) {
      /* get duration to calculate end time */
      GstClockTime duration = GST_BUFFER_DURATION (buffer);

      if (GST_CLOCK_TIME_IS_VALID (duration)) {
        *end = timestamp + duration;
      }
      *start = timestamp;
    }
  } else {
    *start = -1;
    *end = -1;
  }
}

static gboolean
gst_audio_test_src_start (GstBaseSrc * basesrc)
{
  GstAudioTestSrc *src = GST_AUDIO_TEST_SRC (basesrc);

  src->next_sample = 0;
  src->next_byte = 0;
  src->next_time = 0;
  src->check_seek_stop = FALSE;
  src->eos_reached = FALSE;
  src->tags_pushed = FALSE;
  src->accumulator = 0;
  src->tick_counter = 0;

  return TRUE;
}

static gboolean
gst_audio_test_src_stop (GstBaseSrc * basesrc)
{
  return TRUE;
}

/* seek to time, will be called when we operate in push mode. In pull mode we
 * get the requested byte offset. */
static gboolean
gst_audio_test_src_do_seek (GstBaseSrc * basesrc, GstSegment * segment)
{
  GstAudioTestSrc *src = GST_AUDIO_TEST_SRC (basesrc);
  GstClockTime time;
  gint samplerate, bpf;
  gint64 next_sample;

  GST_DEBUG_OBJECT (src, "seeking %" GST_SEGMENT_FORMAT, segment);

  time = segment->position;
  src->reverse = (segment->rate < 0.0);

  samplerate = GST_AUDIO_INFO_RATE (&src->info);
  bpf = GST_AUDIO_INFO_BPF (&src->info);

  /* now move to the time indicated, don't seek to the sample *after* the time */
  next_sample = gst_util_uint64_scale_int (time, samplerate, GST_SECOND);
  src->next_byte = next_sample * bpf;
  if (samplerate == 0)
    src->next_time = 0;
  else
    src->next_time =
        gst_util_uint64_scale_round (next_sample, GST_SECOND, samplerate);

  GST_DEBUG_OBJECT (src, "seeking next_sample=%" G_GINT64_FORMAT
      " next_time=%" GST_TIME_FORMAT, next_sample,
      GST_TIME_ARGS (src->next_time));

  g_assert (src->next_time <= time);

  src->next_sample = next_sample;

  if (segment->rate > 0 && GST_CLOCK_TIME_IS_VALID (segment->stop)) {
    time = segment->stop;
    src->sample_stop =
        gst_util_uint64_scale_round (time, samplerate, GST_SECOND);
    src->check_seek_stop = TRUE;
  } else if (segment->rate < 0) {
    time = segment->start;
    src->sample_stop =
        gst_util_uint64_scale_round (time, samplerate, GST_SECOND);
    src->check_seek_stop = TRUE;
  } else {
    src->check_seek_stop = FALSE;
  }
  src->eos_reached = FALSE;

  return TRUE;
}

static gboolean
gst_audio_test_src_is_seekable (GstBaseSrc * basesrc)
{
  /* we're seekable... */
  return TRUE;
}

static GstFlowReturn
gst_audio_test_src_fill (GstBaseSrc * basesrc, guint64 offset,
    guint length, GstBuffer * buffer)
{
  GstAudioTestSrc *src;
  GstClockTime next_time;
  gint64 next_sample, next_byte;
  gint bytes, samples;
  GstElementClass *eclass;
  GstMapInfo map;
  gint samplerate, bpf;

  src = GST_AUDIO_TEST_SRC (basesrc);

  /* example for tagging generated data */
  if (!src->tags_pushed) {
    GstTagList *taglist;

    taglist = gst_tag_list_new (GST_TAG_DESCRIPTION, "audiotest wave", NULL);

    eclass = GST_ELEMENT_CLASS (parent_class);
    if (eclass->send_event)
      eclass->send_event (GST_ELEMENT_CAST (basesrc),
          gst_event_new_tag (taglist));
    else
      gst_tag_list_unref (taglist);
    src->tags_pushed = TRUE;
  }

  if (src->eos_reached) {
    GST_INFO_OBJECT (src, "eos");
    return GST_FLOW_EOS;
  }

  samplerate = GST_AUDIO_INFO_RATE (&src->info);
  bpf = GST_AUDIO_INFO_BPF (&src->info);

  /* if no length was given, use our default length in samples otherwise convert
   * the length in bytes to samples. */
  if (length == -1)
    samples = src->samples_per_buffer;
  else
    samples = length / bpf;

  /* if no offset was given, use our next logical byte */
  if (offset == -1)
    offset = src->next_byte;

  /* now see if we are at the byteoffset we think we are */
  if (offset != src->next_byte) {
    GST_DEBUG_OBJECT (src, "seek to new offset %" G_GUINT64_FORMAT, offset);
    /* we have a discont in the expected sample offset, do a 'seek' */
    src->next_sample = offset / bpf;
    src->next_time =
        gst_util_uint64_scale_int (src->next_sample, GST_SECOND, samplerate);
    src->next_byte = offset;
  }

  /* check for eos */
  if (src->check_seek_stop && !src->reverse &&
      (src->sample_stop > src->next_sample) &&
      (src->sample_stop < src->next_sample + samples)
      ) {
    /* calculate only partial buffer */
    src->generate_samples_per_buffer = src->sample_stop - src->next_sample;
    next_sample = src->sample_stop;
    src->eos_reached = TRUE;
  } else if (src->check_seek_stop && src->reverse &&
      (src->sample_stop >= (src->next_sample - samples))
      ) {
    /* calculate only partial buffer */
    src->generate_samples_per_buffer = src->next_sample - src->sample_stop;
    next_sample = src->sample_stop;
    src->eos_reached = TRUE;
  } else {
    /* calculate full buffer */
    src->generate_samples_per_buffer = samples;
    next_sample = src->next_sample + (src->reverse ? (-samples) : samples);
  }

  bytes = src->generate_samples_per_buffer * bpf;

  next_byte = src->next_byte + (src->reverse ? (-bytes) : bytes);
  next_time = gst_util_uint64_scale_int (next_sample, GST_SECOND, samplerate);

  GST_LOG_OBJECT (src, "samplerate %d", samplerate);
  GST_LOG_OBJECT (src, "next_sample %" G_GINT64_FORMAT ", ts %" GST_TIME_FORMAT,
      next_sample, GST_TIME_ARGS (next_time));

  gst_buffer_set_size (buffer, bytes);

  GST_BUFFER_OFFSET (buffer) = src->next_sample;
  GST_BUFFER_OFFSET_END (buffer) = next_sample;
  if (!src->reverse) {
    GST_BUFFER_TIMESTAMP (buffer) = src->timestamp_offset + src->next_time;
    GST_BUFFER_DURATION (buffer) = next_time - src->next_time;
  } else {
    GST_BUFFER_TIMESTAMP (buffer) = src->timestamp_offset + next_time;
    GST_BUFFER_DURATION (buffer) = src->next_time - next_time;
  }

  gst_object_sync_values (GST_OBJECT (src), GST_BUFFER_TIMESTAMP (buffer));

  src->next_time = next_time;
  src->next_sample = next_sample;
  src->next_byte = next_byte;

  GST_LOG_OBJECT (src, "generating %u samples at ts %" GST_TIME_FORMAT,
      src->generate_samples_per_buffer,
      GST_TIME_ARGS (GST_BUFFER_TIMESTAMP (buffer)));

  gst_buffer_map (buffer, &map, GST_MAP_WRITE);
  if (src->pack_func) {
    gsize tmpsize;

    tmpsize =
        src->generate_samples_per_buffer * GST_AUDIO_INFO_CHANNELS (&src->info)
        * src->pack_size;

    if (tmpsize > src->tmpsize) {
      src->tmp = g_realloc (src->tmp, tmpsize);
      src->tmpsize = tmpsize;
    }
    src->process (src, src->tmp);
    src->pack_func (src->info.finfo, 0, src->tmp, map.data,
        src->generate_samples_per_buffer *
        GST_AUDIO_INFO_CHANNELS (&src->info));
  } else {
    src->process (src, map.data);
  }
  gst_buffer_unmap (buffer, &map);

  if (G_UNLIKELY ((src->wave == GST_AUDIO_TEST_SRC_WAVE_SILENCE)
          || (src->volume == 0.0))) {
    GST_BUFFER_FLAG_SET (buffer, GST_BUFFER_FLAG_GAP);
  }

  if (GST_AUDIO_INFO_LAYOUT (&src->info) == GST_AUDIO_LAYOUT_NON_INTERLEAVED) {
    gst_buffer_add_audio_meta (buffer, &src->info,
        src->generate_samples_per_buffer, NULL);
  }

  return GST_FLOW_OK;
}

static void
gst_audio_test_src_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstAudioTestSrc *src = GST_AUDIO_TEST_SRC (object);

  switch (prop_id) {
    case PROP_SAMPLES_PER_BUFFER:
      src->samples_per_buffer = g_value_get_int (value);
      gst_base_src_set_blocksize (GST_BASE_SRC_CAST (src),
          GST_AUDIO_INFO_BPF (&src->info) * src->samples_per_buffer);
      break;
    case PROP_WAVE:
      src->wave = g_value_get_enum (value);
      gst_audio_test_src_change_wave (src);
      break;
    case PROP_FREQ:
      src->freq = g_value_get_double (value);
      break;
    case PROP_VOLUME:
      src->volume = g_value_get_double (value);
      gst_audio_test_src_change_volume (src);
      break;
    case PROP_IS_LIVE:
      gst_base_src_set_live (GST_BASE_SRC (src), g_value_get_boolean (value));
      break;
    case PROP_TIMESTAMP_OFFSET:
      src->timestamp_offset = g_value_get_int64 (value);
      break;
    case PROP_SINE_PERIODS_PER_TICK:
      src->sine_periods_per_tick = g_value_get_uint (value);
      break;
    case PROP_TICK_INTERVAL:
      src->tick_interval = g_value_get_uint64 (value);
      break;
    case PROP_MARKER_TICK_PERIOD:
      src->marker_tick_period = g_value_get_uint (value);
      break;
    case PROP_MARKER_TICK_VOLUME:
      src->marker_tick_volume = g_value_get_double (value);
      break;
    case PROP_APPLY_TICK_RAMP:
      src->apply_tick_ramp = g_value_get_boolean (value);
      break;
    case PROP_CAN_ACTIVATE_PUSH:
      GST_BASE_SRC (src)->can_activate_push = g_value_get_boolean (value);
      break;
    case PROP_CAN_ACTIVATE_PULL:
      src->can_activate_pull = g_value_get_boolean (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_audio_test_src_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstAudioTestSrc *src = GST_AUDIO_TEST_SRC (object);

  switch (prop_id) {
    case PROP_SAMPLES_PER_BUFFER:
      g_value_set_int (value, src->samples_per_buffer);
      break;
    case PROP_WAVE:
      g_value_set_enum (value, src->wave);
      break;
    case PROP_FREQ:
      g_value_set_double (value, src->freq);
      break;
    case PROP_VOLUME:
      g_value_set_double (value, src->volume);
      break;
    case PROP_IS_LIVE:
      g_value_set_boolean (value, gst_base_src_is_live (GST_BASE_SRC (src)));
      break;
    case PROP_TIMESTAMP_OFFSET:
      g_value_set_int64 (value, src->timestamp_offset);
      break;
    case PROP_SINE_PERIODS_PER_TICK:
      g_value_set_uint (value, src->sine_periods_per_tick);
      break;
    case PROP_TICK_INTERVAL:
      g_value_set_uint64 (value, src->tick_interval);
      break;
    case PROP_MARKER_TICK_PERIOD:
      g_value_set_uint (value, src->marker_tick_period);
      break;
    case PROP_MARKER_TICK_VOLUME:
      g_value_set_double (value, src->marker_tick_volume);
      break;
    case PROP_APPLY_TICK_RAMP:
      g_value_set_boolean (value, src->apply_tick_ramp);
      break;
    case PROP_CAN_ACTIVATE_PUSH:
      g_value_set_boolean (value, GST_BASE_SRC (src)->can_activate_push);
      break;
    case PROP_CAN_ACTIVATE_PULL:
      g_value_set_boolean (value, src->can_activate_pull);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static gboolean
plugin_init (GstPlugin * plugin)
{
  return GST_ELEMENT_REGISTER (audiotestsrc, plugin);
}

GST_PLUGIN_DEFINE (GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    audiotestsrc,
    "Creates audio test signals of given frequency and volume",
    plugin_init, VERSION, "LGPL", GST_PACKAGE_NAME, GST_PACKAGE_ORIGIN);
