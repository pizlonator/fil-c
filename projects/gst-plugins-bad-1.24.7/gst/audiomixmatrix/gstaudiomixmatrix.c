/*
 * GStreamer
 * Copyright (C) 2017 Vivia Nikolaidou <vivia@toolsonair.com>
 *
 * gstaudiomixmatrix.c
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
 * SECTION:element-audiomixmatrix
 * @title: audiomixmatrix
 * @short_description: Transform input/output channels according to a matrix
 *
 * This element transforms a given number of input channels into a given
 * number of output channels according to a given transformation matrix. The
 * matrix coefficients must be between -1 and 1: the number of rows is equal
 * to the number of output channels and the number of columns is equal to the
 * number of input channels. In the first-channels mode, input/output channels
 * are automatically negotiated and the transformation matrix is a truncated
 * identity matrix.
 *
 * ## Example matrix generation code
 * To generate the matrix using code:
 *
 * |[
 * GValue v = G_VALUE_INIT;
 * GValue v2 = G_VALUE_INIT;
 * GValue v3 = G_VALUE_INIT;
 *
 * g_value_init (&v2, GST_TYPE_ARRAY);
 * g_value_init (&v3, G_TYPE_DOUBLE);
 * g_value_set_double (&v3, 1);
 * gst_value_array_append_value (&v2, &v3);
 * g_value_unset (&v3);
 * [ Repeat for as many double as your input channels - unset and reinit v3 ]
 * g_value_init (&v, GST_TYPE_ARRAY);
 * gst_value_array_append_value (&v, &v2);
 * g_value_unset (&v2);
 * [ Repeat for as many v2's as your output channels - unset and reinit v2]
 * g_object_set_property (G_OBJECT (audiomixmatrix), "matrix", &v);
 * g_value_unset (&v);
 * ]|
 *
 * ## Example launch line
 * |[
 * gst-launch-1.0 audiotestsrc ! audio/x-raw,channels=4 ! audiomixmatrix in-channels=4 out-channels=2 channel-mask=-1 matrix="<<(double)1, (double)0, (double)0, (double)0>, <0.0, 1.0, 0.0, 0.0>>" ! audio/x-raw,channels=2 ! autoaudiosink
 * ]|
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gstaudiomixmatrix.h"

#include <gst/gst.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

GST_DEBUG_CATEGORY_STATIC (audiomixmatrix_debug);
#define GST_CAT_DEFAULT audiomixmatrix_debug

/* GstAudioMixMatrix properties */
enum
{
  PROP_0,
  PROP_IN_CHANNELS,
  PROP_OUT_CHANNELS,
  PROP_MATRIX,
  PROP_CHANNEL_MASK,
  PROP_MODE
};

GType
gst_audio_mix_matrix_mode_get_type (void)
{
  static GType gst_audio_mix_matrix_mode_type = 0;
  static const GEnumValue gst_audio_mix_matrix_mode[] = {
    {GST_AUDIO_MIX_MATRIX_MODE_MANUAL,
          "Manual mode: please specify input/output channels and transformation matrix",
        "manual"},
    {GST_AUDIO_MIX_MATRIX_MODE_FIRST_CHANNELS,
          "First channels mode: input/output channels are auto-negotiated, transformation matrix is a truncated identity matrix",
        "first-channels"},
    {0, NULL, NULL}

  };

  if (!gst_audio_mix_matrix_mode_type) {
    gst_audio_mix_matrix_mode_type =
        g_enum_register_static ("GstAudioMixMatrixModeType",
        gst_audio_mix_matrix_mode);
  }
  return gst_audio_mix_matrix_mode_type;
}

static GstStaticPadTemplate gst_audio_mix_matrix_src_template =
GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS
    ("audio/x-raw, channels = [1, max], layout = (string) interleaved, format = (string) {"
        GST_AUDIO_NE (F32) "," GST_AUDIO_NE (F64) "," GST_AUDIO_NE (S16) ","
        GST_AUDIO_NE (S32) "}")
    );

static GstStaticPadTemplate gst_audio_mix_matrix_sink_template =
GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS
    ("audio/x-raw, channels = [1, max], layout = (string) interleaved, format = (string) {"
        GST_AUDIO_NE (F32) "," GST_AUDIO_NE (F64) "," GST_AUDIO_NE (S16) ","
        GST_AUDIO_NE (S32) "}")
    );

static void gst_audio_mix_matrix_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_audio_mix_matrix_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);
static void gst_audio_mix_matrix_dispose (GObject * object);
static gboolean gst_audio_mix_matrix_get_unit_size (GstBaseTransform * trans,
    GstCaps * caps, gsize * size);
static gboolean gst_audio_mix_matrix_set_caps (GstBaseTransform * trans,
    GstCaps * incaps, GstCaps * outcaps);
static GstFlowReturn gst_audio_mix_matrix_transform (GstBaseTransform * trans,
    GstBuffer * inbuf, GstBuffer * outbuf);
static GstCaps *gst_audio_mix_matrix_transform_caps (GstBaseTransform * trans,
    GstPadDirection direction, GstCaps * caps, GstCaps * filter);
static GstCaps *gst_audio_mix_matrix_fixate_caps (GstBaseTransform * trans,
    GstPadDirection direction, GstCaps * caps, GstCaps * othercaps);
static GstStateChangeReturn gst_audio_mix_matrix_change_state (GstElement *
    element, GstStateChange transition);

G_DEFINE_TYPE (GstAudioMixMatrix, gst_audio_mix_matrix,
    GST_TYPE_BASE_TRANSFORM);
GST_ELEMENT_REGISTER_DEFINE (audiomixmatrix, "audiomixmatrix", GST_RANK_NONE,
    GST_TYPE_AUDIO_MIX_MATRIX);

static void
gst_audio_mix_matrix_class_init (GstAudioMixMatrixClass * klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;
  GstElementClass *element_class = (GstElementClass *) klass;
  GstBaseTransformClass *trans_class = (GstBaseTransformClass *) klass;

  GST_DEBUG_CATEGORY_INIT (audiomixmatrix_debug, "audiomixmatrix", 0,
      "audiomixmatrix");
  gst_element_class_set_static_metadata (element_class, "Matrix audio mix",
      "Filter/Audio",
      "Mixes a number of input channels into a number of output channels according to a transformation matrix",
      "Vivia Nikolaidou <vivia@toolsonair.com>");

  gobject_class->set_property = gst_audio_mix_matrix_set_property;
  gobject_class->get_property = gst_audio_mix_matrix_get_property;
  gobject_class->dispose = gst_audio_mix_matrix_dispose;

  g_object_class_install_property (gobject_class, PROP_IN_CHANNELS,
      g_param_spec_uint ("in-channels", "Input audio channels",
          "How many audio channels we have on the input side",
          0, 64, 0, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_OUT_CHANNELS,
      g_param_spec_uint ("out-channels", "Output audio channels",
          "How many audio channels we have on the output side",
          0, 64, 0, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_MATRIX,
      gst_param_spec_array ("matrix",
          "Input/output channel matrix",
          "Transformation matrix for input/output channels",
          gst_param_spec_array ("matrix-in1", "rows", "rows",
              g_param_spec_double ("matrix-in2", "cols", "cols",
                  -1, 1, 0,
                  G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS),
              G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS),
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_CHANNEL_MASK,
      g_param_spec_uint64 ("channel-mask",
          "Output channel mask",
          "Output channel mask (-1 means \"default for these channels\")",
          0, G_MAXUINT64, 0, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_MODE,
      g_param_spec_enum ("mode",
          "Channel/matrix mode",
          "Whether to auto-negotiate input/output channels and matrix",
          GST_TYPE_AUDIO_MIX_MATRIX_MODE,
          GST_AUDIO_MIX_MATRIX_MODE_MANUAL,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  gst_element_class_add_pad_template (element_class,
      gst_static_pad_template_get (&gst_audio_mix_matrix_sink_template));
  gst_element_class_add_pad_template (element_class,
      gst_static_pad_template_get (&gst_audio_mix_matrix_src_template));

  trans_class->get_unit_size =
      GST_DEBUG_FUNCPTR (gst_audio_mix_matrix_get_unit_size);
  trans_class->set_caps = GST_DEBUG_FUNCPTR (gst_audio_mix_matrix_set_caps);
  trans_class->transform = GST_DEBUG_FUNCPTR (gst_audio_mix_matrix_transform);
  trans_class->transform_caps =
      GST_DEBUG_FUNCPTR (gst_audio_mix_matrix_transform_caps);
  trans_class->fixate_caps =
      GST_DEBUG_FUNCPTR (gst_audio_mix_matrix_fixate_caps);

  element_class->change_state =
      GST_DEBUG_FUNCPTR (gst_audio_mix_matrix_change_state);

  gst_type_mark_as_plugin_api (GST_TYPE_AUDIO_MIX_MATRIX_MODE, 0);
}

static void
gst_audio_mix_matrix_init (GstAudioMixMatrix * self)
{
  self->in_channels = 0;
  self->out_channels = 0;
  self->matrix = NULL;
  self->channel_mask = 0;
  self->s16_conv_matrix = NULL;
  self->s32_conv_matrix = NULL;
  self->mode = GST_AUDIO_MIX_MATRIX_MODE_MANUAL;
}

static void
gst_audio_mix_matrix_dispose (GObject * object)
{
  GstAudioMixMatrix *self = GST_AUDIO_MIX_MATRIX (object);

  if (self->matrix) {
    g_free (self->matrix);
    self->matrix = NULL;
  }

  G_OBJECT_CLASS (gst_audio_mix_matrix_parent_class)->dispose (object);
}

static void
gst_audio_mix_matrix_convert_s16_matrix (GstAudioMixMatrix * self)
{
  gint i;

  /* converted bits - input bits - sign - bits needed for channel */
  self->shift_bytes = 32 - 16 - 1 - ceil (log (self->in_channels) / log (2));

  if (self->s16_conv_matrix)
    g_free (self->s16_conv_matrix);
  self->s16_conv_matrix =
      g_new (gint32, self->in_channels * self->out_channels);
  for (i = 0; i < self->in_channels * self->out_channels; i++) {
    self->s16_conv_matrix[i] =
        (gint32) ((self->matrix[i]) * (1 << self->shift_bytes));
  }
}

static void
gst_audio_mix_matrix_convert_s32_matrix (GstAudioMixMatrix * self)
{
  gint i;

  /* converted bits - input bits - sign - bits needed for channel */
  self->shift_bytes = 64 - 32 - 1 - (gint) (log (self->in_channels) / log (2));

  if (self->s32_conv_matrix)
    g_free (self->s32_conv_matrix);
  self->s32_conv_matrix =
      g_new (gint64, self->in_channels * self->out_channels);
  for (i = 0; i < self->in_channels * self->out_channels; i++) {
    self->s32_conv_matrix[i] =
        (gint64) ((self->matrix[i]) * (1 << self->shift_bytes));
  }
}


static void
gst_audio_mix_matrix_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstAudioMixMatrix *self = GST_AUDIO_MIX_MATRIX (object);

  switch (prop_id) {
    case PROP_IN_CHANNELS:
      self->in_channels = g_value_get_uint (value);
      if (self->matrix) {
        gst_audio_mix_matrix_convert_s16_matrix (self);
        gst_audio_mix_matrix_convert_s32_matrix (self);
      }
      break;
    case PROP_OUT_CHANNELS:
      self->out_channels = g_value_get_uint (value);
      if (self->matrix) {
        gst_audio_mix_matrix_convert_s16_matrix (self);
        gst_audio_mix_matrix_convert_s32_matrix (self);
      }
      break;
    case PROP_MATRIX:{
      gint in, out;

      if (self->matrix)
        g_free (self->matrix);
      self->matrix = g_new (gdouble, self->in_channels * self->out_channels);

      g_return_if_fail (gst_value_array_get_size (value) == self->out_channels);
      for (out = 0; out < self->out_channels; out++) {
        const GValue *row = gst_value_array_get_value (value, out);
        g_return_if_fail (gst_value_array_get_size (row) == self->in_channels);
        for (in = 0; in < self->in_channels; in++) {
          const GValue *itm;
          gdouble coefficient;

          itm = gst_value_array_get_value (row, in);
          g_return_if_fail (G_VALUE_HOLDS_DOUBLE (itm));
          coefficient = g_value_get_double (itm);
          self->matrix[out * self->in_channels + in] = coefficient;
        }
      }
      gst_audio_mix_matrix_convert_s16_matrix (self);
      gst_audio_mix_matrix_convert_s32_matrix (self);
      break;
    }
    case PROP_CHANNEL_MASK:
      self->channel_mask = g_value_get_uint64 (value);
      break;
    case PROP_MODE:
      self->mode = g_value_get_enum (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_audio_mix_matrix_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstAudioMixMatrix *self = GST_AUDIO_MIX_MATRIX (object);

  switch (prop_id) {
    case PROP_IN_CHANNELS:
      g_value_set_uint (value, self->in_channels);
      break;
    case PROP_OUT_CHANNELS:
      g_value_set_uint (value, self->out_channels);
      break;
    case PROP_MATRIX:{
      gint in, out;

      if (self->matrix == NULL)
        break;

      for (out = 0; out < self->out_channels; out++) {
        GValue row = G_VALUE_INIT;
        g_value_init (&row, GST_TYPE_ARRAY);
        for (in = 0; in < self->in_channels; in++) {
          GValue itm = G_VALUE_INIT;
          g_value_init (&itm, G_TYPE_DOUBLE);
          g_value_set_double (&itm, self->matrix[out * self->in_channels + in]);
          gst_value_array_append_value (&row, &itm);
          g_value_unset (&itm);
        }
        gst_value_array_append_value (value, &row);
        g_value_unset (&row);
      }
      break;
    }
    case PROP_CHANNEL_MASK:
      g_value_set_uint64 (value, self->channel_mask);
      break;
    case PROP_MODE:
      g_value_set_enum (value, self->mode);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static GstStateChangeReturn
gst_audio_mix_matrix_change_state (GstElement * element,
    GstStateChange transition)
{
  GstAudioMixMatrix *self = GST_AUDIO_MIX_MATRIX (element);
  GstStateChangeReturn s;

  s = GST_ELEMENT_CLASS (gst_audio_mix_matrix_parent_class)->change_state
      (element, transition);

  if (transition == GST_STATE_CHANGE_PAUSED_TO_READY) {
    if (self->s16_conv_matrix) {
      g_free (self->s16_conv_matrix);
      self->s16_conv_matrix = NULL;
    }

    if (self->s32_conv_matrix) {
      g_free (self->s32_conv_matrix);
      self->s32_conv_matrix = NULL;
    }
  }

  return s;
}


static GstFlowReturn
gst_audio_mix_matrix_transform (GstBaseTransform * vfilter,
    GstBuffer * inbuf, GstBuffer * outbuf)
{
  GstMapInfo inmap, outmap;
  GstAudioMixMatrix *self = GST_AUDIO_MIX_MATRIX (vfilter);
  gint in, out, sample;
  guint inchannels = self->in_channels;
  guint outchannels = self->out_channels;
  gdouble *matrix = self->matrix;

  if (!gst_buffer_map (inbuf, &inmap, GST_MAP_READ)) {
    return GST_FLOW_ERROR;
  }
  if (!gst_buffer_map (outbuf, &outmap, GST_MAP_WRITE)) {
    gst_buffer_unmap (inbuf, &inmap);
    return GST_FLOW_ERROR;
  }

  switch (self->format) {
    case GST_AUDIO_FORMAT_F32LE:
    case GST_AUDIO_FORMAT_F32BE:{
      const gfloat *inarray;
      gfloat *outarray;
      guint n_samples = outmap.size / (sizeof (gfloat) * outchannels);

      inarray = (gfloat *) inmap.data;
      outarray = (gfloat *) outmap.data;

      for (sample = 0; sample < n_samples; sample++) {
        for (out = 0; out < outchannels; out++) {
          gfloat outval = 0;
          for (in = 0; in < inchannels; in++) {
            outval +=
                inarray[sample * inchannels +
                in] * matrix[out * inchannels + in];
          }
          outarray[sample * outchannels + out] = outval;
        }
      }
      break;
    }
    case GST_AUDIO_FORMAT_F64LE:
    case GST_AUDIO_FORMAT_F64BE:{
      const gdouble *inarray;
      gdouble *outarray;
      guint n_samples = outmap.size / (sizeof (gdouble) * outchannels);

      inarray = (gdouble *) inmap.data;
      outarray = (gdouble *) outmap.data;

      for (sample = 0; sample < n_samples; sample++) {
        for (out = 0; out < outchannels; out++) {
          gdouble outval = 0;
          for (in = 0; in < inchannels; in++) {
            outval +=
                inarray[sample * inchannels +
                in] * matrix[out * inchannels + in];
          }
          outarray[sample * outchannels + out] = outval;
        }
      }
      break;
    }
    case GST_AUDIO_FORMAT_S16LE:
    case GST_AUDIO_FORMAT_S16BE:{
      const gint16 *inarray;
      gint16 *outarray;
      guint n_samples = outmap.size / (sizeof (gint16) * outchannels);
      guint n = self->shift_bytes;
      gint32 *conv_matrix = self->s16_conv_matrix;

      inarray = (gint16 *) inmap.data;
      outarray = (gint16 *) outmap.data;

      for (sample = 0; sample < n_samples; sample++) {
        for (out = 0; out < outchannels; out++) {
          gint32 outval = 0;
          for (in = 0; in < inchannels; in++) {
            outval += (gint32) (inarray[sample * inchannels + in] *
                conv_matrix[out * inchannels + in]);
          }
          outarray[sample * outchannels + out] = (gint16) (outval >> n);
        }
      }
      break;
    }
    case GST_AUDIO_FORMAT_S32LE:
    case GST_AUDIO_FORMAT_S32BE:{
      const gint32 *inarray;
      gint32 *outarray;
      guint n_samples = outmap.size / (sizeof (gint32) * outchannels);
      guint n = self->shift_bytes;
      gint64 *conv_matrix = self->s32_conv_matrix;

      inarray = (gint32 *) inmap.data;
      outarray = (gint32 *) outmap.data;

      for (sample = 0; sample < n_samples; sample++) {
        for (out = 0; out < outchannels; out++) {
          gint64 outval = 0;
          for (in = 0; in < inchannels; in++) {
            outval += (gint64) (inarray[sample * inchannels + in] *
                conv_matrix[out * inchannels + in]);
          }
          outarray[sample * outchannels + out] = (gint32) (outval >> n);
        }
      }
      break;
    }
    default:
      gst_buffer_unmap (inbuf, &inmap);
      gst_buffer_unmap (outbuf, &outmap);
      return GST_FLOW_NOT_SUPPORTED;

  }

  gst_buffer_unmap (inbuf, &inmap);
  gst_buffer_unmap (outbuf, &outmap);
  return GST_FLOW_OK;
}

static gboolean
gst_audio_mix_matrix_get_unit_size (GstBaseTransform * trans,
    GstCaps * caps, gsize * size)
{
  GstAudioInfo info;

  if (!gst_audio_info_from_caps (&info, caps))
    return FALSE;

  *size = GST_AUDIO_INFO_BPF (&info);

  return TRUE;
}

static gboolean
gst_audio_mix_matrix_set_caps (GstBaseTransform * trans, GstCaps * incaps,
    GstCaps * outcaps)
{
  GstAudioMixMatrix *self = GST_AUDIO_MIX_MATRIX (trans);
  GstAudioInfo info, out_info;

  if (!gst_audio_info_from_caps (&info, incaps))
    return FALSE;

  if (!gst_audio_info_from_caps (&out_info, outcaps))
    return FALSE;

  self->format = info.finfo->format;

  if (self->mode == GST_AUDIO_MIX_MATRIX_MODE_FIRST_CHANNELS) {
    gint in, out;

    self->in_channels = info.channels;
    self->out_channels = out_info.channels;

    self->matrix = g_new (gdouble, self->in_channels * self->out_channels);

    for (out = 0; out < self->out_channels; out++) {
      for (in = 0; in < self->in_channels; in++) {
        self->matrix[out * self->in_channels + in] = (out == in);
      }
    }
  } else if (!self->matrix || info.channels != self->in_channels ||
      out_info.channels != self->out_channels) {
    GST_ELEMENT_ERROR (self, LIBRARY, SETTINGS,
        ("Erroneous matrix detected"),
        ("Please enter a matrix with the correct input and output channels"));
    return FALSE;
  }

  switch (self->format) {
    case GST_AUDIO_FORMAT_S16LE:
    case GST_AUDIO_FORMAT_S16BE:{
      gst_audio_mix_matrix_convert_s16_matrix (self);
      break;
    }
    case GST_AUDIO_FORMAT_S32LE:
    case GST_AUDIO_FORMAT_S32BE:{
      gst_audio_mix_matrix_convert_s32_matrix (self);
      break;
    }
    default:
      break;
  }
  return TRUE;
}

static GstCaps *
gst_audio_mix_matrix_fixate_caps (GstBaseTransform * trans,
    GstPadDirection direction, GstCaps * caps, GstCaps * othercaps)
{
  GstAudioMixMatrix *self = GST_AUDIO_MIX_MATRIX (trans);
  GstStructure *s, *s2;
  guint capssize = gst_caps_get_size (othercaps);
  gint i;
  gint channels;

  if (self->mode == GST_AUDIO_MIX_MATRIX_MODE_FIRST_CHANNELS) {
    s2 = gst_caps_get_structure (caps, 0);

    /* Try to keep channel configuration as much as possible */
    if (gst_structure_get_int (s2, "channels", &channels)) {
      gint mindiff = -1;
      othercaps = gst_caps_make_writable (othercaps);
      for (i = 0; i < capssize; i++) {
        s = gst_caps_get_structure (othercaps, i);
        if (!gst_structure_has_field (s, "channels")) {
          mindiff = 0;
          gst_structure_set (s, "channels", G_TYPE_INT, channels, NULL);
        } else {
          gint outchannels;
          gint diff;

          gst_structure_fixate_field_nearest_int (s, "channels", channels);
          if (gst_structure_get_int (s, "channels", &outchannels)) {
            diff = ABS (channels - outchannels);
            if (mindiff < 0 || diff < mindiff)
              mindiff = diff;
          }
        }
      }

      if (mindiff >= 0) {
        for (i = 0; i < capssize; i++) {
          gint outchannels, diff;
          s = gst_caps_get_structure (othercaps, i);
          if (gst_structure_get_int (s, "channels", &outchannels)) {
            diff = ABS (channels - outchannels);
            if (diff > mindiff) {
              gst_caps_remove_structure (othercaps, i--);
              capssize--;
            }
          }
        }
      }
    }
  }

  if (gst_caps_is_empty (othercaps))
    return othercaps;

  othercaps =
      GST_BASE_TRANSFORM_CLASS (gst_audio_mix_matrix_parent_class)->fixate_caps
      (trans, direction, caps, othercaps);

  s = gst_caps_get_structure (othercaps, 0);

  if (!gst_structure_has_field (s, "channel-mask")) {
    if (self->mode == GST_AUDIO_MIX_MATRIX_MODE_FIRST_CHANNELS ||
        self->channel_mask == -1) {
      gint channels;

      g_assert (gst_structure_get_int (s, "channels", &channels));
      gst_structure_set (s, "channel-mask", GST_TYPE_BITMASK,
          gst_audio_channel_get_fallback_mask (channels), NULL);
    } else {
      gst_structure_set (s, "channel-mask", GST_TYPE_BITMASK,
          self->channel_mask, NULL);
    }
  }

  return othercaps;

}

static GstCaps *
gst_audio_mix_matrix_transform_caps (GstBaseTransform * trans,
    GstPadDirection direction, GstCaps * caps, GstCaps * filter)
{
  GstAudioMixMatrix *self = GST_AUDIO_MIX_MATRIX (trans);
  GstCaps *outcaps = gst_caps_copy (caps);
  GstCaps *ret;
  GstStructure *s;
  gint i;
  guint capssize = gst_caps_get_size (outcaps);

  if (self->mode == GST_AUDIO_MIX_MATRIX_MODE_FIRST_CHANNELS) {
    for (i = 0; i < capssize; i++) {
      s = gst_caps_get_structure (outcaps, i);
      if (gst_structure_has_field (s, "channels")) {
        gst_structure_remove_field (s, "channels");
      }
      if (gst_structure_has_field (s, "channel-mask")) {
        gst_structure_remove_field (s, "channel-mask");
      }
    }
    goto beach;
  }

  if (self->in_channels == 0 || self->out_channels == 0 || self->matrix == NULL) {
    /* Not dispatching element error because we return empty caps anyway and
     * we should let it fail to link. Additionally, the element error would be
     * printed as WARN, so a possible gst-launch pipeline would appear to
     * hang. */
    GST_ERROR_OBJECT (self, "Invalid settings detected in manual mode. "
        "Please specify in-channels, out-channels and matrix.");
    return gst_caps_new_empty ();
  }

  if (self->in_channels == self->out_channels) {
    goto beach;
  }

  for (i = 0; i < capssize; i++) {
    s = gst_caps_get_structure (outcaps, i);
    if (direction == GST_PAD_SRC) {
      gst_structure_set (s, "channels", G_TYPE_INT, self->in_channels, NULL);
      gst_structure_remove_field (s, "channel-mask");
    } else if (direction == GST_PAD_SINK) {
      gst_structure_set (s, "channels", G_TYPE_INT, self->out_channels,
          "channel-mask", GST_TYPE_BITMASK, self->channel_mask, NULL);
    } else {
      g_assert_not_reached ();
    }
  }

beach:
  if (filter) {
    ret = gst_caps_intersect_full (filter, outcaps, GST_CAPS_INTERSECT_FIRST);
    gst_caps_unref (outcaps);
  } else {
    ret = outcaps;
  }
  return ret;
}

static gboolean
plugin_init (GstPlugin * plugin)
{
  return GST_ELEMENT_REGISTER (audiomixmatrix, plugin);
}

GST_PLUGIN_DEFINE (GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    audiomixmatrix,
    "Audio matrix mix",
    plugin_init, VERSION, GST_LICENSE, GST_PACKAGE_NAME, GST_PACKAGE_ORIGIN);
