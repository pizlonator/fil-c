/* -*- c-basic-offset: 2 -*-
 * vi:si:et:sw=2:sts=8:ts=8:expandtab
 *
 * GStreamer
 * Copyright (C) 1999-2001 Erik Walthinsen <omega@cse.ogi.edu>
 * Copyright (C) 2005 Andy Wingo <wingo@pobox.com>
 * Copyright (C) 2010 Sebastian Dröge <sebastian.droege@collabora.co.uk>
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
 * SECTION:element-volume
 * @title: volume
 *
 * The volume element changes the volume of the audio data.
 *
 * ## Example launch line
 * |[
 * gst-launch-1.0 -v -m audiotestsrc ! volume volume=0.5 ! level ! fakesink silent=TRUE
 * ]|
 *  This pipeline shows that the level of audiotestsrc has been halved
 * (peak values are around -6 dB and RMS around -9 dB) compared to
 * the same pipeline without the volume element.
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>
#include <gst/gst.h>
#include <gst/base/gstbasetransform.h>
#include <gst/audio/audio.h>
#include <gst/audio/gstaudiofilter.h>

#ifdef HAVE_ORC
#include <orc/orcfunctions.h>
#else
#define orc_memset memset
#endif

#include "gstvolumeorc.h"
#include "gstvolume.h"

/* some defines for audio processing */
/* we map VOLUME_UNITY_INT* to volume = 1.0
 */
#define VOLUME_UNITY_INT8            8  /* internal int for unity 2^(8-5) */
#define VOLUME_UNITY_INT8_BIT_SHIFT  3  /* number of bits to shift for unity */
#define VOLUME_UNITY_INT16           2048       /* internal int for unity 2^(16-5) */
#define VOLUME_UNITY_INT16_BIT_SHIFT 11 /* number of bits to shift for unity */
#define VOLUME_UNITY_INT24           524288     /* internal int for unity 2^(24-5) */
#define VOLUME_UNITY_INT24_BIT_SHIFT 19 /* number of bits to shift for unity */
#define VOLUME_UNITY_INT32           134217728  /* internal int for unity 2^(32-5) */
#define VOLUME_UNITY_INT32_BIT_SHIFT 27
#define VOLUME_MAX_INT8              G_MAXINT8
#define VOLUME_MIN_INT8              G_MININT8
#define VOLUME_MAX_INT16             G_MAXINT16
#define VOLUME_MIN_INT16             G_MININT16
#define VOLUME_MAX_INT24             8388607
#define VOLUME_MIN_INT24             -8388608
#define VOLUME_MAX_INT32             G_MAXINT32
#define VOLUME_MIN_INT32             G_MININT32
#define VOLUME_MAX_INT64             G_MAXINT64
#define VOLUME_MIN_INT64             G_MININT64

#define GST_CAT_DEFAULT gst_volume_debug
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);

/* Filter signals and args */
enum
{
  /* FILL ME */
  LAST_SIGNAL
};

#define DEFAULT_PROP_MUTE       FALSE
#define DEFAULT_PROP_VOLUME     1.0

enum
{
  PROP_0,
  PROP_MUTE,
  PROP_VOLUME,
  PROP_VOLUME_FULL_RANGE
};

#if G_BYTE_ORDER == G_LITTLE_ENDIAN
#define ALLOWED_CAPS \
    GST_AUDIO_CAPS_MAKE ("{ F32LE, F64LE, S8, S16LE, S24LE, S32LE }") \
    ", layout = (string) interleaved"
#else
#define ALLOWED_CAPS \
    GST_AUDIO_CAPS_MAKE ("{ F32BE, F64BE, S8, S16BE, S24BE, S32BE }") \
    ", layout = (string) { interleaved, non-interleaved }"
#endif

#define gst_volume_parent_class parent_class
G_DEFINE_TYPE_WITH_CODE (GstVolume, gst_volume,
    GST_TYPE_AUDIO_FILTER,
    G_IMPLEMENT_INTERFACE (GST_TYPE_STREAM_VOLUME, NULL));
GST_ELEMENT_REGISTER_DEFINE (volume, "volume", GST_RANK_NONE, GST_TYPE_VOLUME);

static void volume_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void volume_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);

static void volume_before_transform (GstBaseTransform * base,
    GstBuffer * buffer);
static GstFlowReturn volume_transform_ip (GstBaseTransform * base,
    GstBuffer * outbuf);
static gboolean volume_stop (GstBaseTransform * base);
static gboolean volume_setup (GstAudioFilter * filter,
    const GstAudioInfo * info);

static void volume_process_double (GstVolume * self, gpointer bytes,
    guint n_bytes);
static void volume_process_controlled_double (GstVolume * self, gpointer bytes,
    gdouble * volume, guint channels, guint n_bytes);
static void volume_process_float (GstVolume * self, gpointer bytes,
    guint n_bytes);
static void volume_process_controlled_float (GstVolume * self, gpointer bytes,
    gdouble * volume, guint channels, guint n_bytes);
static void volume_process_int32 (GstVolume * self, gpointer bytes,
    guint n_bytes);
static void volume_process_int32_clamp (GstVolume * self, gpointer bytes,
    guint n_bytes);
static void volume_process_int32_via_double_with_clamp (GstVolume * self,
    gpointer bytes, guint n_bytes);
static void volume_process_controlled_int32_clamp (GstVolume * self,
    gpointer bytes, gdouble * volume, guint channels, guint n_bytes);
static void volume_process_int24 (GstVolume * self, gpointer bytes,
    guint n_bytes);
static void volume_process_int24_clamp (GstVolume * self, gpointer bytes,
    guint n_bytes);
static void volume_process_int24_via_float_with_clamp (GstVolume * self,
    gpointer bytes, guint n_bytes);
static void volume_process_controlled_int24_clamp (GstVolume * self,
    gpointer bytes, gdouble * volume, guint channels, guint n_bytes);
static void volume_process_int16 (GstVolume * self, gpointer bytes,
    guint n_bytes);
static void volume_process_int16_clamp (GstVolume * self, gpointer bytes,
    guint n_bytes);
static void volume_process_int16_via_float_with_clamp (GstVolume * self,
    gpointer bytes, guint n_bytes);
static void volume_process_controlled_int16_clamp (GstVolume * self,
    gpointer bytes, gdouble * volume, guint channels, guint n_bytes);
static void volume_process_int8 (GstVolume * self, gpointer bytes,
    guint n_bytes);
static void volume_process_int8_clamp (GstVolume * self, gpointer bytes,
    guint n_bytes);
static void volume_process_int8_via_float_with_clamp (GstVolume * self,
    gpointer bytes, guint n_bytes);
static void volume_process_controlled_int8_clamp (GstVolume * self,
    gpointer bytes, gdouble * volume, guint channels, guint n_bytes);


/* helper functions */

static gboolean
volume_choose_func (GstVolume * self, const GstAudioInfo * info)
{
  GstAudioFormat format;

  self->process = NULL;
  self->process_controlled = NULL;

  format = GST_AUDIO_INFO_FORMAT (info);

  if (format == GST_AUDIO_FORMAT_UNKNOWN)
    return FALSE;

  switch (format) {
    case GST_AUDIO_FORMAT_S32:
      /* only clamp if the gain is greater than 1.0 */
      if (self->current_vol_i32 > VOLUME_UNITY_INT32) {
        /* Fixed-point multiplication only supports small subset of volumes */
        if (self->current_vol_i32 > VOLUME_MAX_INT32) {
          self->process = volume_process_int32_via_double_with_clamp;
        } else {
          self->process = volume_process_int32_clamp;
        }
      } else {
        self->process = volume_process_int32;
      }
      self->process_controlled = volume_process_controlled_int32_clamp;
      break;
    case GST_AUDIO_FORMAT_S24:
      /* only clamp if the gain is greater than 1.0 */
      if (self->current_vol_i24 > VOLUME_UNITY_INT24) {
        /* Fixed-point multiplication only supports small subset of volumes */
        if (self->current_vol_i24 > VOLUME_MAX_INT24) {
          self->process = volume_process_int24_via_float_with_clamp;
        } else {
          self->process = volume_process_int24_clamp;
        }
      } else {
        self->process = volume_process_int24;
      }
      self->process_controlled = volume_process_controlled_int24_clamp;
      break;
    case GST_AUDIO_FORMAT_S16:
      /* only clamp if the gain is greater than 1.0 */
      if (self->current_vol_i16 > VOLUME_UNITY_INT16) {
        /* Fixed-point multiplication only supports small subset of volumes */
        if (self->current_vol_i16 > VOLUME_MAX_INT16) {
          self->process = volume_process_int16_via_float_with_clamp;
        } else {
          self->process = volume_process_int16_clamp;
        }
      } else {
        self->process = volume_process_int16;
      }
      self->process_controlled = volume_process_controlled_int16_clamp;
      break;
    case GST_AUDIO_FORMAT_S8:
      /* only clamp if the gain is greater than 1.0 */
      if (self->current_vol_i8 > VOLUME_UNITY_INT8) {
        /* Fixed-point multiplication only supports small subset of volumes */
        if (self->current_vol_i8 > VOLUME_MAX_INT8) {
          self->process = volume_process_int8_via_float_with_clamp;
        } else {
          self->process = volume_process_int8_clamp;
        }
      } else {
        self->process = volume_process_int8;
      }
      self->process_controlled = volume_process_controlled_int8_clamp;
      break;
    case GST_AUDIO_FORMAT_F32:
      self->process = volume_process_float;
      self->process_controlled = volume_process_controlled_float;
      break;
    case GST_AUDIO_FORMAT_F64:
      self->process = volume_process_double;
      self->process_controlled = volume_process_controlled_double;
      break;
    default:
      break;
  }

  return (self->process != NULL);
}

static gboolean
volume_update_volume (GstVolume * self, const GstAudioInfo * info,
    gdouble volume, gboolean mute)
{
  gboolean passthrough;
  gboolean res;

  GST_DEBUG_OBJECT (self, "configure mute %d, volume %f", mute, volume);

  if (mute) {
    self->current_mute = TRUE;
    self->current_volume = 0.0;

    self->current_vol_i8 = 0;
    self->current_vol_i16 = 0;
    self->current_vol_i24 = 0;
    self->current_vol_i32 = 0;

    passthrough = FALSE;
  } else {
    self->current_mute = FALSE;
    self->current_volume = volume;

    gdouble current_fp_vol_i8 = (gdouble) volume * (gdouble) VOLUME_UNITY_INT8;
    gdouble current_fp_vol_i16 =
        (gdouble) volume * (gdouble) VOLUME_UNITY_INT16;
    gdouble current_fp_vol_i24 =
        (gdouble) volume * (gdouble) VOLUME_UNITY_INT24;
    gdouble current_fp_vol_i32 =
        (gdouble) volume * (gdouble) VOLUME_UNITY_INT32;

    /* Perform "saturating" FP->int conversion.
     * We want to be able to tell when current_fp_vol_i32 is *larger* than
     * VOLUME_MAX_INT32, *and* don't have UB on FP->int cast overflow,
     * `2 * VOLUME_MAX_INT32` achieves both of these points.
     */
    self->current_vol_i8 = (glong) (CLAMP (current_fp_vol_i8, (gdouble) 0,
            (gdouble) 2 * VOLUME_MAX_INT32));
    self->current_vol_i16 = (glong) (CLAMP (current_fp_vol_i16, (gdouble) 0,
            (gdouble) 2 * VOLUME_MAX_INT32));
    self->current_vol_i24 = (glong) (CLAMP (current_fp_vol_i24, (gdouble) 0,
            (gdouble) 2 * VOLUME_MAX_INT32));
    self->current_vol_i32 = (glong) (CLAMP (current_fp_vol_i32, (gdouble) 0,
            (gdouble) 2 * VOLUME_MAX_INT32));

    switch (GST_AUDIO_INFO_FORMAT (info)) {
      case GST_AUDIO_FORMAT_F32:
      case GST_AUDIO_FORMAT_F64:
        passthrough = (self->current_volume == 1.0);
        break;
      case GST_AUDIO_FORMAT_S8:
        passthrough = (self->current_vol_i8 == VOLUME_UNITY_INT8);
        break;
      case GST_AUDIO_FORMAT_S16:
        passthrough = (self->current_vol_i16 == VOLUME_UNITY_INT16);
        break;
      case GST_AUDIO_FORMAT_S24:
        passthrough = (self->current_vol_i24 == VOLUME_UNITY_INT24);
        break;
      case GST_AUDIO_FORMAT_S32:
        passthrough = (self->current_vol_i32 == VOLUME_UNITY_INT32);
        break;
      default:
        passthrough = FALSE;
        break;
    }
  }

  /* If a controller is used, never use passthrough mode
   * because the property can change from 1.0 to something
   * else in the middle of a buffer.
   */
  passthrough &= !gst_object_has_active_control_bindings (GST_OBJECT (self));

  GST_DEBUG_OBJECT (self, "set passthrough %d", passthrough);

  gst_base_transform_set_passthrough (GST_BASE_TRANSFORM (self), passthrough);

  res = self->negotiated = volume_choose_func (self, info);

  return res;
}

/* Element class */

static void
gst_volume_dispose (GObject * object)
{
  GstVolume *volume = GST_VOLUME (object);

  if (volume->tracklist) {
    if (volume->tracklist->data)
      g_object_unref (volume->tracklist->data);
    g_list_free (volume->tracklist);
    volume->tracklist = NULL;
  }

  G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
gst_volume_class_init (GstVolumeClass * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *element_class;
  GstBaseTransformClass *trans_class;
  GstAudioFilterClass *filter_class;
  GstCaps *caps;

  gobject_class = (GObjectClass *) klass;
  element_class = (GstElementClass *) klass;
  trans_class = (GstBaseTransformClass *) klass;
  filter_class = (GstAudioFilterClass *) (klass);

  gobject_class->set_property = volume_set_property;
  gobject_class->get_property = volume_get_property;
  gobject_class->dispose = gst_volume_dispose;

  g_object_class_install_property (gobject_class, PROP_MUTE,
      g_param_spec_boolean ("mute", "Mute", "mute channel",
          DEFAULT_PROP_MUTE,
          G_PARAM_READWRITE | GST_PARAM_CONTROLLABLE | G_PARAM_STATIC_STRINGS));

  /**
   * GstVolume:volume-full-range:
   *
   * The volume-full-range factor is a range from 0.0 to G_MAXDOUBLE so
   * it is *very* inclusive, e.g. gain of +48 dB is very reasonable. This
   * property allows setting higher value than the #GstVolume:volume property.
   *
   * Since: 1.24
   */
  /* This property has been exposed as another property to keep the backward compatibility
   * when using #GstDirectControlBinding with GstDirectControlBinding:absolute set to FALSE. */
  g_object_class_install_property (gobject_class, PROP_VOLUME_FULL_RANGE,
      g_param_spec_double ("volume-full-range", "Volume",
          "volume factor with a full range of values, 1.0=100%", 0.0,
          G_MAXDOUBLE, DEFAULT_PROP_VOLUME,
          G_PARAM_READWRITE | GST_PARAM_CONTROLLABLE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_VOLUME,
      g_param_spec_double ("volume", "Volume", "volume factor, 1.0=100%",
          0.0, 10.0, DEFAULT_PROP_VOLUME,
          G_PARAM_READWRITE | GST_PARAM_CONTROLLABLE | G_PARAM_STATIC_STRINGS));

  gst_element_class_set_static_metadata (element_class, "Volume",
      "Filter/Effect/Audio",
      "Set volume on audio/raw streams", "Andy Wingo <wingo@pobox.com>");

  caps = gst_caps_from_string (ALLOWED_CAPS);
  gst_audio_filter_class_add_pad_templates (filter_class, caps);
  gst_caps_unref (caps);

  trans_class->before_transform = GST_DEBUG_FUNCPTR (volume_before_transform);
  trans_class->transform_ip = GST_DEBUG_FUNCPTR (volume_transform_ip);
  trans_class->stop = GST_DEBUG_FUNCPTR (volume_stop);
  trans_class->transform_ip_on_passthrough = FALSE;

  filter_class->setup = GST_DEBUG_FUNCPTR (volume_setup);
}

static void
gst_volume_init (GstVolume * self)
{
  self->mute = DEFAULT_PROP_MUTE;
  self->volume = DEFAULT_PROP_VOLUME;

  self->tracklist = NULL;
  self->negotiated = FALSE;

  gst_base_transform_set_gap_aware (GST_BASE_TRANSFORM (self), TRUE);
}

static void
volume_process_double (GstVolume * self, gpointer bytes, guint n_bytes)
{
  gdouble *data = (gdouble *) bytes;
  guint num_samples = n_bytes / sizeof (gdouble);

  volume_orc_scalarmultiply_f64_ns (data, self->current_volume, num_samples);
}

static void
volume_process_controlled_double (GstVolume * self, gpointer bytes,
    gdouble * volume, guint channels, guint n_bytes)
{
  gdouble *data = (gdouble *) bytes;
  guint num_samples = n_bytes / (sizeof (gdouble) * channels);
  guint i, j;
  gdouble vol;

  if (channels == 1) {
    volume_orc_process_controlled_f64_1ch (data, volume, num_samples);
  } else {
    for (i = 0; i < num_samples; i++) {
      vol = *volume++;
      for (j = 0; j < channels; j++) {
        *data++ *= vol;
      }
    }
  }
}

static void
volume_process_float (GstVolume * self, gpointer bytes, guint n_bytes)
{
  gfloat *data = (gfloat *) bytes;
  guint num_samples = n_bytes / sizeof (gfloat);

  volume_orc_scalarmultiply_f32_ns (data, self->current_volume, num_samples);
}

static void
volume_process_controlled_float (GstVolume * self, gpointer bytes,
    gdouble * volume, guint channels, guint n_bytes)
{
  gfloat *data = (gfloat *) bytes;
  guint num_samples = n_bytes / (sizeof (gfloat) * channels);
  guint i, j;
  gdouble vol;

  if (channels == 1) {
    volume_orc_process_controlled_f32_1ch (data, volume, num_samples);
  } else if (channels == 2) {
    volume_orc_process_controlled_f32_2ch (data, volume, num_samples);
  } else {
    for (i = 0; i < num_samples; i++) {
      vol = *volume++;
      for (j = 0; j < channels; j++) {
        *data++ *= vol;
      }
    }
  }
}

static void
volume_process_int32 (GstVolume * self, gpointer bytes, guint n_bytes)
{
  gint32 *data = (gint32 *) bytes;
  guint num_samples = n_bytes / sizeof (gint);

  /* hard coded in volume.orc */
  g_assert (VOLUME_UNITY_INT32_BIT_SHIFT == 27);
  volume_orc_process_int32 (data, self->current_vol_i32, num_samples);
}

static void
volume_process_int32_clamp (GstVolume * self, gpointer bytes, guint n_bytes)
{
  gint32 *data = (gint32 *) bytes;
  guint num_samples = n_bytes / sizeof (gint);

  /* hard coded in volume.orc */
  g_assert (VOLUME_UNITY_INT32_BIT_SHIFT == 27);

  volume_orc_process_int32_clamp (data, self->current_vol_i32, num_samples);
}

// TODO: Add ORC implementation for this
static void
volume_process_int32_via_double_with_clamp (GstVolume * self,
    gpointer bytes, guint n_bytes)
{
  gint32 *data = (gint32 *) bytes;
  guint i;
  guint num_samples = n_bytes / sizeof (gint32);
  /* Cast between the 32-bit integer and 32-bit floating point is lossy,
   * but not between 64-bit floating point, so use it instead. */
  /* WARNING: (gint32)((gfloat)VOLUME_MAX_INT32)) is UB! */
  gdouble vol = self->current_volume;
  gdouble val;

  for (i = 0; i < num_samples; i++) {
    val = *data * vol;
    *data++ = (gint32) CLAMP (val, VOLUME_MIN_INT32, VOLUME_MAX_INT32);
  }
}

static void
volume_process_controlled_int32_clamp (GstVolume * self, gpointer bytes,
    gdouble * volume, guint channels, guint n_bytes)
{
  gint32 *data = (gint32 *) bytes;
  guint i, j;
  guint num_samples = n_bytes / (sizeof (gint32) * channels);
  gdouble vol, val;

  if (channels == 1) {
    volume_orc_process_controlled_int32_1ch (data, volume, num_samples);
  } else {
    for (i = 0; i < num_samples; i++) {
      vol = *volume++;
      for (j = 0; j < channels; j++) {
        val = *data * vol;
        *data++ = (gint32) CLAMP (val, VOLUME_MIN_INT32, VOLUME_MAX_INT32);
      }
    }
  }
}

#if (G_BYTE_ORDER == G_LITTLE_ENDIAN)
/* NOTE: does not sign-extend, result is an 24-bit unsigned integer! */
#define get_unaligned_i24(_x) ( (((guint8*)_x)[0]) | ((((guint8*)_x)[1]) << 8) | ((((gint8*)_x)[2]) << 16) )

#define write_unaligned_u24(_x,samp) \
G_STMT_START { \
  *(_x)++ = samp & 0xFF; \
  *(_x)++ = (samp >> 8) & 0xFF; \
  *(_x)++ = (samp >> 16) & 0xFF; \
} G_STMT_END

#else /* BIG ENDIAN */
/* NOTE: does not sign-extend, result is an 24-bit unsigned integer! */
#define get_unaligned_i24(_x) ( (((guint8*)_x)[2]) | ((((guint8*)_x)[1]) << 8) | ((((gint8*)_x)[0]) << 16) )
#define write_unaligned_u24(_x,samp) \
G_STMT_START { \
  *(_x)++ = (samp >> 16) & 0xFF; \
  *(_x)++ = (samp >> 8) & 0xFF; \
  *(_x)++ = samp & 0xFF; \
} G_STMT_END
#endif

#define sign_extend_i24(_x)                                                    \
  (((gint32)(((guint32)(_x)) << (32 - 24))) >> (32 - 24))

static void
volume_process_int24 (GstVolume * self, gpointer bytes, guint n_bytes)
{
  gint8 *data = (gint8 *) bytes;        /* treat the data as a byte stream */
  guint i, num_samples;
  guint32 samp;
  gint64 val;

  num_samples = n_bytes / (sizeof (gint8) * 3);
  for (i = 0; i < num_samples; i++) {
    samp = get_unaligned_i24 (data);

    val = (gint32) samp;
    val =
        (((gint64) self->current_vol_i24 *
            val) >> VOLUME_UNITY_INT24_BIT_SHIFT);
    samp = (guint32) val;

    /* write the value back into the stream */
    write_unaligned_u24 (data, samp);
  }
}

static void
volume_process_int24_clamp (GstVolume * self, gpointer bytes, guint n_bytes)
{
  gint8 *data = (gint8 *) bytes;        /* treat the data as a byte stream */
  guint i, num_samples;
  guint32 samp;
  gint64 val;

  num_samples = n_bytes / (sizeof (gint8) * 3);
  for (i = 0; i < num_samples; i++) {
    samp = get_unaligned_i24 (data);

    val = (gint32) samp;
    val =
        (((gint64) self->current_vol_i24 *
            val) >> VOLUME_UNITY_INT24_BIT_SHIFT);
    samp = (guint32) CLAMP (val, VOLUME_MIN_INT24, VOLUME_MAX_INT24);

    /* write the value back into the stream */
    write_unaligned_u24 (data, samp);
  }
}

static void
volume_process_int24_via_float_with_clamp (GstVolume * self,
    gpointer bytes, guint n_bytes)
{
  gint8 *data = (gint8 *) bytes;        /* treat the data as a byte stream */
  guint i, num_samples;
  guint32 samp;
  gfloat vol = self->current_volume;
  gfloat val;

  num_samples = n_bytes / (sizeof (gint8) * 3);
  for (i = 0; i < num_samples; i++) {
    samp = get_unaligned_i24 (data);    /* NOT SIGN-EXTENDED!!! */

    val = (gfloat) (sign_extend_i24 (samp));
    val *= vol;
    /* NOTE: we *MUST* first cast FP to int, and only then to unsigned. */
    samp = (guint32) ((gint32) CLAMP (val, VOLUME_MIN_INT24, VOLUME_MAX_INT24));

    /* write the value back into the stream */
    write_unaligned_u24 (data, samp);
  }
}

static void
volume_process_controlled_int24_clamp (GstVolume * self, gpointer bytes,
    gdouble * volume, guint channels, guint n_bytes)
{
  gint8 *data = (gint8 *) bytes;        /* treat the data as a byte stream */
  guint i, j;
  guint num_samples = n_bytes / (sizeof (gint8) * 3 * channels);
  gdouble vol, val;

  for (i = 0; i < num_samples; i++) {
    vol = *volume++;
    for (j = 0; j < channels; j++) {
      val = get_unaligned_i24 (data) * vol;
      val = CLAMP (val, VOLUME_MIN_INT24, VOLUME_MAX_INT24);
      write_unaligned_u24 (data, (gint32) val);
    }
  }
}

static void
volume_process_int16 (GstVolume * self, gpointer bytes, guint n_bytes)
{
  gint16 *data = (gint16 *) bytes;
  guint num_samples = n_bytes / sizeof (gint16);

  /* hard coded in volume.orc */
  g_assert (VOLUME_UNITY_INT16_BIT_SHIFT == 11);

  volume_orc_process_int16 (data, self->current_vol_i16, num_samples);
}

static void
volume_process_int16_clamp (GstVolume * self, gpointer bytes, guint n_bytes)
{
  gint16 *data = (gint16 *) bytes;
  guint num_samples = n_bytes / sizeof (gint16);

  /* hard coded in volume.orc */
  g_assert (VOLUME_UNITY_INT16_BIT_SHIFT == 11);

  volume_orc_process_int16_clamp (data, self->current_vol_i16, num_samples);
}

// TODO: Add ORC implementation for this
static void
volume_process_int16_via_float_with_clamp (GstVolume * self,
    gpointer bytes, guint n_bytes)
{
  gint16 *data = (gint16 *) bytes;
  guint i;
  guint num_samples = n_bytes / sizeof (gint16);
  gfloat vol = self->current_volume;
  gfloat val;

  for (i = 0; i < num_samples; i++) {
    val = *data * vol;
    *data++ = (gint16) CLAMP (val, VOLUME_MIN_INT16, VOLUME_MAX_INT16);
  }
}

static void
volume_process_controlled_int16_clamp (GstVolume * self, gpointer bytes,
    gdouble * volume, guint channels, guint n_bytes)
{
  gint16 *data = (gint16 *) bytes;
  guint i, j;
  guint num_samples = n_bytes / (sizeof (gint16) * channels);
  gdouble vol, val;

  if (channels == 1) {
    volume_orc_process_controlled_int16_1ch (data, volume, num_samples);
  } else if (channels == 2) {
    volume_orc_process_controlled_int16_2ch (data, volume, num_samples);
  } else {
    for (i = 0; i < num_samples; i++) {
      vol = *volume++;
      for (j = 0; j < channels; j++) {
        val = *data * vol;
        *data++ = (gint16) CLAMP (val, VOLUME_MIN_INT16, VOLUME_MAX_INT16);
      }
    }
  }
}

static void
volume_process_int8 (GstVolume * self, gpointer bytes, guint n_bytes)
{
  gint8 *data = (gint8 *) bytes;
  guint num_samples = n_bytes / sizeof (gint8);

  /* hard coded in volume.orc */
  g_assert (VOLUME_UNITY_INT8_BIT_SHIFT == 3);

  volume_orc_process_int8 (data, self->current_vol_i8, num_samples);
}

static void
volume_process_int8_clamp (GstVolume * self, gpointer bytes, guint n_bytes)
{
  gint8 *data = (gint8 *) bytes;
  guint num_samples = n_bytes / sizeof (gint8);

  /* hard coded in volume.orc */
  g_assert (VOLUME_UNITY_INT8_BIT_SHIFT == 3);

  volume_orc_process_int8_clamp (data, self->current_vol_i8, num_samples);
}

// TODO: Add ORC implementation for this
static void
volume_process_int8_via_float_with_clamp (GstVolume * self,
    gpointer bytes, guint n_bytes)
{
  gint8 *data = (gint8 *) bytes;
  guint i;
  guint num_samples = n_bytes / sizeof (gint8);
  gfloat vol = self->current_volume;
  gfloat val;

  for (i = 0; i < num_samples; i++) {
    val = *data;
    val *= vol;
    *data++ = (gint8) CLAMP (val, VOLUME_MIN_INT8, VOLUME_MAX_INT8);
  }
}

static void
volume_process_controlled_int8_clamp (GstVolume * self, gpointer bytes,
    gdouble * volume, guint channels, guint n_bytes)
{
  gint8 *data = (gint8 *) bytes;
  guint i, j;
  guint num_samples = n_bytes / (sizeof (gint8) * channels);
  gdouble val, vol;

  if (channels == 1) {
    volume_orc_process_controlled_int8_1ch (data, volume, num_samples);
  } else if (channels == 2) {
    volume_orc_process_controlled_int8_2ch (data, volume, num_samples);
  } else {
    for (i = 0; i < num_samples; i++) {
      vol = *volume++;
      for (j = 0; j < channels; j++) {
        val = *data * vol;
        *data++ = (gint8) CLAMP (val, VOLUME_MIN_INT8, VOLUME_MAX_INT8);
      }
    }
  }
}

/* GstBaseTransform vmethod implementations */

/* get notified of caps and plug in the correct process function */
static gboolean
volume_setup (GstAudioFilter * filter, const GstAudioInfo * info)
{
  gboolean res;
  GstVolume *self = GST_VOLUME (filter);
  gdouble volume;
  gboolean mute;

  GST_OBJECT_LOCK (self);
  volume = self->volume;
  mute = self->mute;
  GST_OBJECT_UNLOCK (self);

  res = volume_update_volume (self, info, volume, mute);
  if (!res) {
    GST_ELEMENT_ERROR (self, CORE, NEGOTIATION,
        ("Invalid incoming format"), (NULL));
  }
  self->negotiated = res;

  return res;
}

static gboolean
volume_stop (GstBaseTransform * base)
{
  GstVolume *self = GST_VOLUME (base);

  g_free (self->volumes);
  self->volumes = NULL;
  self->volumes_count = 0;

  g_free (self->mutes);
  self->mutes = NULL;
  self->mutes_count = 0;

  return GST_CALL_PARENT_WITH_DEFAULT (GST_BASE_TRANSFORM_CLASS, stop, (base),
      TRUE);
}

static void
volume_before_transform (GstBaseTransform * base, GstBuffer * buffer)
{
  GstClockTime timestamp;
  GstVolume *self = GST_VOLUME (base);
  gdouble volume;
  gboolean mute;

  timestamp = GST_BUFFER_TIMESTAMP (buffer);
  timestamp =
      gst_segment_to_stream_time (&base->segment, GST_FORMAT_TIME, timestamp);

  GST_DEBUG_OBJECT (base, "sync to %" GST_TIME_FORMAT,
      GST_TIME_ARGS (timestamp));

  if (GST_CLOCK_TIME_IS_VALID (timestamp))
    gst_object_sync_values (GST_OBJECT (self), timestamp);

  /* get latest values */
  GST_OBJECT_LOCK (self);
  volume = self->volume;
  mute = self->mute;
  GST_OBJECT_UNLOCK (self);

  if ((volume != self->current_volume) || (mute != self->current_mute)) {
    /* the volume or mute was updated, update our internal state before
     * we continue processing. */
    volume_update_volume (self, GST_AUDIO_FILTER_INFO (self), volume, mute);
  }
}

/* call the plugged-in process function for this instance
 * needs to be done with this indirection since volume_transform is
 * a class-global method
 */
static GstFlowReturn
volume_transform_ip (GstBaseTransform * base, GstBuffer * outbuf)
{
  GstAudioFilter *filter = GST_AUDIO_FILTER_CAST (base);
  GstVolume *self = GST_VOLUME (base);
  GstMapInfo map;
  GstClockTime ts;

  if (G_UNLIKELY (!self->negotiated))
    goto not_negotiated;

  /* don't process data with GAP */
  if (GST_BUFFER_FLAG_IS_SET (outbuf, GST_BUFFER_FLAG_GAP))
    return GST_FLOW_OK;

  gst_buffer_map (outbuf, &map, GST_MAP_READWRITE);
  ts = GST_BUFFER_TIMESTAMP (outbuf);
  ts = gst_segment_to_stream_time (&base->segment, GST_FORMAT_TIME, ts);

  if (GST_CLOCK_TIME_IS_VALID (ts)) {
    GstControlBinding *mute_cb, *volume_cb;

    mute_cb = gst_object_get_control_binding (GST_OBJECT (self), "mute");
    volume_cb = gst_object_get_control_binding (GST_OBJECT (self), "volume");

    if (mute_cb || (volume_cb && !self->current_mute)) {
      gint rate = GST_AUDIO_INFO_RATE (&filter->info);
      gint width = GST_AUDIO_FORMAT_INFO_WIDTH (filter->info.finfo) / 8;
      gint channels = GST_AUDIO_INFO_CHANNELS (&filter->info);
      guint nsamples = map.size / (width * channels);
      GstClockTime interval = gst_util_uint64_scale_int (1, GST_SECOND, rate);
      gboolean have_mutes = FALSE;
      gboolean have_volumes = FALSE;

      if (self->mutes_count < nsamples && mute_cb) {
        self->mutes = g_realloc (self->mutes, sizeof (gboolean) * nsamples);
        self->mutes_count = nsamples;
      }

      if (self->volumes_count < nsamples) {
        self->volumes = g_realloc (self->volumes, sizeof (gdouble) * nsamples);
        self->volumes_count = nsamples;
      }

      if (volume_cb && self->volumes) {
        have_volumes =
            gst_control_binding_get_value_array (volume_cb, ts, interval,
            nsamples, (gpointer) self->volumes);
        gst_object_replace ((GstObject **) & volume_cb, NULL);
      }
      if (!have_volumes) {
        volume_orc_memset_f64 (self->volumes, self->current_volume, nsamples);
      }

      if (mute_cb && self->mutes) {
        have_mutes = gst_control_binding_get_value_array (mute_cb, ts, interval,
            nsamples, (gpointer) self->mutes);
        gst_object_replace ((GstObject **) & mute_cb, NULL);
      }
      if (have_mutes) {
        volume_orc_prepare_volumes (self->volumes, self->mutes, nsamples);
      } else {
        g_free (self->mutes);
        self->mutes = NULL;
        self->mutes_count = 0;
      }

      self->process_controlled (self, map.data, self->volumes, channels,
          map.size);

      goto done;
    } else if (volume_cb) {
      gst_object_unref (volume_cb);
    }
  }

  if (self->current_volume == 0.0 || self->current_mute) {
    orc_memset (map.data, 0, map.size);
    GST_BUFFER_FLAG_SET (outbuf, GST_BUFFER_FLAG_GAP);
  } else if (self->current_volume != 1.0) {
    self->process (self, map.data, map.size);
  }

done:
  gst_buffer_unmap (outbuf, &map);

  return GST_FLOW_OK;

  /* ERRORS */
not_negotiated:
  {
    GST_ELEMENT_ERROR (self, CORE, NEGOTIATION,
        ("No format was negotiated"), (NULL));
    return GST_FLOW_NOT_NEGOTIATED;
  }
}

static void
volume_set_property (GObject * object, guint prop_id, const GValue * value,
    GParamSpec * pspec)
{
  GstVolume *self = GST_VOLUME (object);

  switch (prop_id) {
    case PROP_MUTE:
      GST_OBJECT_LOCK (self);
      self->mute = g_value_get_boolean (value);
      GST_OBJECT_UNLOCK (self);
      break;
    case PROP_VOLUME:
      GST_OBJECT_LOCK (self);
      self->volume = g_value_get_double (value);
      GST_OBJECT_UNLOCK (self);
      break;
    case PROP_VOLUME_FULL_RANGE:
      GST_OBJECT_LOCK (self);
      self->volume = g_value_get_double (value);
      GST_OBJECT_UNLOCK (self);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
volume_get_property (GObject * object, guint prop_id, GValue * value,
    GParamSpec * pspec)
{
  GstVolume *self = GST_VOLUME (object);

  switch (prop_id) {
    case PROP_MUTE:
      GST_OBJECT_LOCK (self);
      g_value_set_boolean (value, self->mute);
      GST_OBJECT_UNLOCK (self);
      break;
    case PROP_VOLUME:
      GST_OBJECT_LOCK (self);
      if (self->volume > 10.0) {
        GST_WARNING_OBJECT (object,
            "Volume is greater than its max value 10.0, reporting as 10.0");
        g_value_set_double (value, 10.0);
      } else {
        g_value_set_double (value, self->volume);
      }
      GST_OBJECT_UNLOCK (self);
      break;
    case PROP_VOLUME_FULL_RANGE:
      GST_OBJECT_LOCK (self);
      g_value_set_double (value, self->volume);
      GST_OBJECT_UNLOCK (self);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static gboolean
plugin_init (GstPlugin * plugin)
{
  GST_DEBUG_CATEGORY_INIT (GST_CAT_DEFAULT, "volume", 0, "Volume gain");

  return GST_ELEMENT_REGISTER (volume, plugin);
}

GST_PLUGIN_DEFINE (GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    volume,
    "plugin for controlling audio volume",
    plugin_init, VERSION, GST_LICENSE, GST_PACKAGE_NAME, GST_PACKAGE_ORIGIN);
