/*
 * GStreamer
 * Copyright (C) 2011 Stefan Sauer <ensonic@users.sf.net>
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
/*
 * Freeverb
 *
 * Written by Jezar at Dreampoint, June 2000
 * http://www.dreampoint.co.uk
 * This code is public domain
 *
 * Translated to C by Peter Hanappe, Mai 2001
 * Transformed into a GStreamer plugin by Stefan Sauer, Nov 2011
 */

/**
 * SECTION:element-freeverb
 * @title: freeverb
 *
 * Reverberation/room effect.
 *
 * ## Example launch line
 * |[
 * gst-launch-1.0 audiotestsrc wave=saw ! freeverb ! autoaudiosink
 * gst-launch-1.0 filesrc location="melo1.ogg" ! decodebin ! audioconvert ! freeverb ! autoaudiosink
 * ]|
 *
 */

/* FIXME:
 * - add mono-to-mono, then we might also need stereo-to-mono ?
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include <gst/gst.h>
#include <gst/base/gstbasetransform.h>

#include "gstfreeverb.h"

#define GST_CAT_DEFAULT gst_freeverb_debug
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);

enum
{
  PROP_0,
  PROP_ROOM_SIZE,
  PROP_DAMPING,
  PROP_PAN_WIDTH,
  PROP_LEVEL
};

static GstStaticPadTemplate sink_template = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("audio/x-raw, "
        "format = (string) { " GST_AUDIO_NE (F32) ", " GST_AUDIO_NE (S16) "}, "
        "rate = (int) [ 1, MAX ], " "channels = (int) [ 1, 2 ], "
        "layout = (string) interleaved")
    );

static GstStaticPadTemplate src_template = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("audio/x-raw, "
        "format = (string) { " GST_AUDIO_NE (F32) ", " GST_AUDIO_NE (S16) "}, "
        "rate = (int) [ 1, MAX ], " "channels = (int) 2, "
        "layout = (string) interleaved")
    );

static void gst_freeverb_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_freeverb_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);

static void gst_freeverb_finalize (GObject * object);

static gboolean gst_freeverb_get_unit_size (GstBaseTransform * base,
    GstCaps * caps, gsize * size);
static GstCaps *gst_freeverb_transform_caps (GstBaseTransform * base,
    GstPadDirection direction, GstCaps * caps, GstCaps * filter);
static gboolean gst_freeverb_set_caps (GstBaseTransform * base,
    GstCaps * incaps, GstCaps * outcaps);

static GstFlowReturn gst_freeverb_transform (GstBaseTransform * base,
    GstBuffer * inbuf, GstBuffer * outbuf);

static gboolean gst_freeverb_transform_m2s_int (GstFreeverb * filter,
    gint16 * idata, gint16 * odata, guint num_samples);
static gboolean gst_freeverb_transform_s2s_int (GstFreeverb * filter,
    gint16 * idata, gint16 * odata, guint num_samples);
static gboolean gst_freeverb_transform_m2s_float (GstFreeverb * filter,
    gfloat * idata, gfloat * odata, guint num_samples);
static gboolean gst_freeverb_transform_s2s_float (GstFreeverb * filter,
    gfloat * idata, gfloat * odata, guint num_samples);


/* Table with processing functions: [channels][format] */
static const GstFreeverbProcessFunc process_functions[2][2] = {
  {
        (GstFreeverbProcessFunc) gst_freeverb_transform_m2s_int,
        (GstFreeverbProcessFunc) gst_freeverb_transform_m2s_float,
      },
  {
        (GstFreeverbProcessFunc) gst_freeverb_transform_s2s_int,
        (GstFreeverbProcessFunc) gst_freeverb_transform_s2s_float,
      }
};

/***************************************************************
 *
 *                           REVERB
 */

/* Denormalising:
 *
 * Another method fixes the problem cheaper: Use a small DC-offset in
 * the filter calculations.  Now the signals converge not against 0,
 * but against the offset.  The constant offset is invisible from the
 * outside world (i.e. it does not appear at the output.  There is a
 * very small turn-on transient response, which should not cause
 * problems.
 */

//#define DC_OFFSET 0
#define DC_OFFSET 1e-8
//#define DC_OFFSET 0.001f

/* all pass filter */

typedef struct _freeverb_allpass
{
  gfloat feedback;
  gfloat *buffer;
  gint bufsize;
  gint bufidx;
} freeverb_allpass;

static void
freeverb_allpass_setbuffer (freeverb_allpass * allpass, gint size)
{
  allpass->bufidx = 0;
  allpass->buffer = g_new (gfloat, size);
  allpass->bufsize = size;
}

static void
freeverb_allpass_release (freeverb_allpass * allpass)
{
  g_free (allpass->buffer);
}

static void
freeverb_allpass_init (freeverb_allpass * allpass)
{
  gint i, len = allpass->bufsize;
  gfloat *buf = allpass->buffer;

  for (i = 0; i < len; i++) {
    buf[i] = (gfloat) DC_OFFSET;        /* this is not 100 % correct. */
  }
}

static void
freeverb_allpass_setfeedback (freeverb_allpass * allpass, gfloat val)
{
  allpass->feedback = val;
}

/*
static gfloat
freeverb_allpass_getfeedback(freeverb_allpass* allpass)
{
  return allpass->feedback;
}*/

#define freeverb_allpass_process(_allpass, _input_1) \
{ \
  gfloat output; \
  gfloat bufout; \
  bufout = _allpass.buffer[_allpass.bufidx]; \
  output = bufout-_input_1; \
  _allpass.buffer[_allpass.bufidx] = _input_1 + (bufout * _allpass.feedback); \
  if (++_allpass.bufidx >= _allpass.bufsize) { \
    _allpass.bufidx = 0; \
  } \
  _input_1 = output; \
}

/* comb filter */

typedef struct _freeverb_comb
{
  gfloat feedback;
  gfloat filterstore;
  gfloat damp1;
  gfloat damp2;
  gfloat *buffer;
  gint bufsize;
  gint bufidx;
} freeverb_comb;

static void
freeverb_comb_setbuffer (freeverb_comb * comb, gint size)
{
  comb->filterstore = 0;
  comb->bufidx = 0;
  comb->buffer = g_new (gfloat, size);
  comb->bufsize = size;
}

static void
freeverb_comb_release (freeverb_comb * comb)
{
  g_free (comb->buffer);
}

static void
freeverb_comb_init (freeverb_comb * comb)
{
  gint i, len = comb->bufsize;
  gfloat *buf = comb->buffer;

  for (i = 0; i < len; i++) {
    buf[i] = (gfloat) DC_OFFSET;        /* This is not 100 % correct. */
  }
}

static void
freeverb_comb_setdamp (freeverb_comb * comb, gfloat val)
{
  comb->damp1 = val;
  comb->damp2 = 1 - val;
}

/*
static gfloat
freeverb_comb_getdamp(freeverb_comb* comb)
{
  return comb->damp1;
}*/

static void
freeverb_comb_setfeedback (freeverb_comb * comb, gfloat val)
{
  comb->feedback = val;
}

/*
static gfloat
freeverb_comb_getfeedback(freeverb_comb* comb)
{
  return comb->feedback;
}*/

#define freeverb_comb_process(_comb, _input_1, _output) \
{ \
  gfloat _tmp = _comb.buffer[_comb.bufidx]; \
  _comb.filterstore = (_tmp * _comb.damp2) + (_comb.filterstore * _comb.damp1); \
  _comb.buffer[_comb.bufidx] = _input_1 + (_comb.filterstore * _comb.feedback); \
  if (++_comb.bufidx >= _comb.bufsize) { \
    _comb.bufidx = 0; \
  } \
  _output += _tmp; \
}

#define numcombs 8
#define numallpasses 4
#define	fixedgain 0.015f
#define scalewet 1.0f
#define scaledry 1.0f
#define scaledamp 1.0f
#define scaleroom 0.28f
#define offsetroom 0.7f
#define stereospread 23

/* These values assume 44.1KHz sample rate
 * they will need scaling for 96KHz (or other) sample rates.
 * The values were obtained by listening tests.
 */
#define combtuningL1 1116
#define combtuningR1 (1116 + stereospread)
#define combtuningL2 1188
#define combtuningR2 (1188 + stereospread)
#define combtuningL3 1277
#define combtuningR3 (1277 + stereospread)
#define combtuningL4 1356
#define combtuningR4 (1356 + stereospread)
#define combtuningL5 1422
#define combtuningR5 (1422 + stereospread)
#define combtuningL6 1491
#define combtuningR6 (1491 + stereospread)
#define combtuningL7 1557
#define combtuningR7 (1557 + stereospread)
#define combtuningL8 1617
#define combtuningR8 (1617 + stereospread)
#define allpasstuningL1 556
#define allpasstuningR1 (556 + stereospread)
#define allpasstuningL2 441
#define allpasstuningR2 (441 + stereospread)
#define allpasstuningL3 341
#define allpasstuningR3 (341 + stereospread)
#define allpasstuningL4 225
#define allpasstuningR4 (225 + stereospread)

struct _GstFreeverbPrivate
{
  gfloat roomsize;
  gfloat damp;
  gfloat wet, wet1, wet2, dry;
  gfloat width;
  gfloat gain;
  /*
     The following are all declared inline
     to remove the need for dynamic allocation
     with its subsequent error-checking messiness
   */
  /* Comb filters */
  freeverb_comb combL[numcombs];
  freeverb_comb combR[numcombs];
  /* Allpass filters */
  freeverb_allpass allpassL[numallpasses];
  freeverb_allpass allpassR[numallpasses];
};

G_DEFINE_TYPE_WITH_CODE (GstFreeverb, gst_freeverb, GST_TYPE_BASE_TRANSFORM,
    G_ADD_PRIVATE (GstFreeverb)
    G_IMPLEMENT_INTERFACE (GST_TYPE_PRESET, NULL));
GST_ELEMENT_REGISTER_DEFINE (freeverb, "freeverb",
    GST_RANK_NONE, GST_TYPE_FREEVERB);

static void
freeverb_revmodel_init (GstFreeverb * filter)
{
  GstFreeverbPrivate *priv = filter->priv;
  gint i;

  for (i = 0; i < numcombs; i++) {
    freeverb_comb_init (&priv->combL[i]);
    freeverb_comb_init (&priv->combR[i]);
  }
  for (i = 0; i < numallpasses; i++) {
    freeverb_allpass_init (&priv->allpassL[i]);
    freeverb_allpass_init (&priv->allpassR[i]);
  }
}

static void
freeverb_revmodel_free (GstFreeverb * filter)
{
  GstFreeverbPrivate *priv = filter->priv;
  gint i;

  for (i = 0; i < numcombs; i++) {
    freeverb_comb_release (&priv->combL[i]);
    freeverb_comb_release (&priv->combR[i]);
  }
  for (i = 0; i < numallpasses; i++) {
    freeverb_allpass_release (&priv->allpassL[i]);
    freeverb_allpass_release (&priv->allpassR[i]);
  }
}

/* GObject vmethod implementations */

static void
gst_freeverb_class_init (GstFreeverbClass * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *element_class;

  GST_DEBUG_CATEGORY_INIT (gst_freeverb_debug, "freeverb", 0,
      "freeverb element");

  gobject_class = (GObjectClass *) klass;
  element_class = (GstElementClass *) klass;

  gobject_class->set_property = gst_freeverb_set_property;
  gobject_class->get_property = gst_freeverb_get_property;
  gobject_class->finalize = gst_freeverb_finalize;

  g_object_class_install_property (gobject_class, PROP_ROOM_SIZE,
      g_param_spec_float ("room-size", "Room size",
          "Size of the simulated room", 0.0, 1.0, 0.5,
          G_PARAM_CONSTRUCT | G_PARAM_READWRITE | GST_PARAM_CONTROLLABLE |
          G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_DAMPING,
      g_param_spec_float ("damping", "Damping", "Damping of high frequencies",
          0.0, 1.0, 0.2f,
          G_PARAM_CONSTRUCT | G_PARAM_READWRITE | GST_PARAM_CONTROLLABLE |
          G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_PAN_WIDTH,
      g_param_spec_float ("width", "Width", "Stereo panorama width", 0.0, 1.0,
          1.0,
          G_PARAM_CONSTRUCT | G_PARAM_READWRITE | GST_PARAM_CONTROLLABLE |
          G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_LEVEL,
      g_param_spec_float ("level", "Level", "dry/wet level", 0.0, 1.0, 0.5,
          G_PARAM_CONSTRUCT | G_PARAM_READWRITE | GST_PARAM_CONTROLLABLE |
          G_PARAM_STATIC_STRINGS));

  gst_element_class_set_static_metadata (element_class,
      "Reverberation/room effect", "Filter/Effect/Audio",
      "Add reverberation to audio streams",
      "Stefan Sauer <ensonic@users.sf.net>");

  gst_element_class_add_static_pad_template (element_class, &src_template);
  gst_element_class_add_static_pad_template (element_class, &sink_template);

  GST_BASE_TRANSFORM_CLASS (klass)->get_unit_size =
      GST_DEBUG_FUNCPTR (gst_freeverb_get_unit_size);
  GST_BASE_TRANSFORM_CLASS (klass)->transform_caps =
      GST_DEBUG_FUNCPTR (gst_freeverb_transform_caps);
  GST_BASE_TRANSFORM_CLASS (klass)->set_caps =
      GST_DEBUG_FUNCPTR (gst_freeverb_set_caps);
  GST_BASE_TRANSFORM_CLASS (klass)->transform =
      GST_DEBUG_FUNCPTR (gst_freeverb_transform);
}

static void
gst_freeverb_init (GstFreeverb * filter)
{
  filter->priv = gst_freeverb_get_instance_private (filter);

  gst_audio_info_init (&filter->info);
  filter->process = NULL;

  gst_base_transform_set_gap_aware (GST_BASE_TRANSFORM (filter), TRUE);

  freeverb_revmodel_init (filter);
}

static void
gst_freeverb_finalize (GObject * object)
{
  GstFreeverb *filter = GST_FREEVERB (object);

  freeverb_revmodel_free (filter);

  G_OBJECT_CLASS (gst_freeverb_parent_class)->finalize (object);
}

static gboolean
gst_freeverb_set_process_function (GstFreeverb * filter, GstAudioInfo * info)
{
  gint channel_index, format_index;
  const GstAudioFormatInfo *finfo = info->finfo;

  /* set processing function */
  channel_index = GST_AUDIO_INFO_CHANNELS (info) - 1;
  if (channel_index > 1 || channel_index < 0) {
    filter->process = NULL;
    return FALSE;
  }

  format_index = GST_AUDIO_FORMAT_INFO_IS_FLOAT (finfo) ? 1 : 0;

  filter->process = process_functions[channel_index][format_index];
  return TRUE;
}

static void
gst_freeverb_init_rev_model (GstFreeverb * filter)
{
  gfloat srfactor = GST_AUDIO_INFO_RATE (&filter->info) / 44100.0f;
  GstFreeverbPrivate *priv = filter->priv;

  freeverb_revmodel_free (filter);

  priv->gain = fixedgain;

  freeverb_comb_setbuffer (&priv->combL[0], combtuningL1 * srfactor);
  freeverb_comb_setbuffer (&priv->combR[0], combtuningR1 * srfactor);
  freeverb_comb_setbuffer (&priv->combL[1], combtuningL2 * srfactor);
  freeverb_comb_setbuffer (&priv->combR[1], combtuningR2 * srfactor);
  freeverb_comb_setbuffer (&priv->combL[2], combtuningL3 * srfactor);
  freeverb_comb_setbuffer (&priv->combR[2], combtuningR3 * srfactor);
  freeverb_comb_setbuffer (&priv->combL[3], combtuningL4 * srfactor);
  freeverb_comb_setbuffer (&priv->combR[3], combtuningR4 * srfactor);
  freeverb_comb_setbuffer (&priv->combL[4], combtuningL5 * srfactor);
  freeverb_comb_setbuffer (&priv->combR[4], combtuningR5 * srfactor);
  freeverb_comb_setbuffer (&priv->combL[5], combtuningL6 * srfactor);
  freeverb_comb_setbuffer (&priv->combR[5], combtuningR6 * srfactor);
  freeverb_comb_setbuffer (&priv->combL[6], combtuningL7 * srfactor);
  freeverb_comb_setbuffer (&priv->combR[6], combtuningR7 * srfactor);
  freeverb_comb_setbuffer (&priv->combL[7], combtuningL8 * srfactor);
  freeverb_comb_setbuffer (&priv->combR[7], combtuningR8 * srfactor);
  freeverb_allpass_setbuffer (&priv->allpassL[0], allpasstuningL1 * srfactor);
  freeverb_allpass_setbuffer (&priv->allpassR[0], allpasstuningR1 * srfactor);
  freeverb_allpass_setbuffer (&priv->allpassL[1], allpasstuningL2 * srfactor);
  freeverb_allpass_setbuffer (&priv->allpassR[1], allpasstuningR2 * srfactor);
  freeverb_allpass_setbuffer (&priv->allpassL[2], allpasstuningL3 * srfactor);
  freeverb_allpass_setbuffer (&priv->allpassR[2], allpasstuningR3 * srfactor);
  freeverb_allpass_setbuffer (&priv->allpassL[3], allpasstuningL4 * srfactor);
  freeverb_allpass_setbuffer (&priv->allpassR[3], allpasstuningR4 * srfactor);

  /* clear buffers */
  freeverb_revmodel_init (filter);

  /* set default values */
  freeverb_allpass_setfeedback (&priv->allpassL[0], 0.5f);
  freeverb_allpass_setfeedback (&priv->allpassR[0], 0.5f);
  freeverb_allpass_setfeedback (&priv->allpassL[1], 0.5f);
  freeverb_allpass_setfeedback (&priv->allpassR[1], 0.5f);
  freeverb_allpass_setfeedback (&priv->allpassL[2], 0.5f);
  freeverb_allpass_setfeedback (&priv->allpassR[2], 0.5f);
  freeverb_allpass_setfeedback (&priv->allpassL[3], 0.5f);
  freeverb_allpass_setfeedback (&priv->allpassR[3], 0.5f);
}

static void
gst_freeverb_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstFreeverb *filter = GST_FREEVERB (object);
  GstFreeverbPrivate *priv = filter->priv;
  gint i;

  switch (prop_id) {
    case PROP_ROOM_SIZE:
      filter->room_size = g_value_get_float (value);
      priv->roomsize = (filter->room_size * scaleroom) + offsetroom;
      for (i = 0; i < numcombs; i++) {
        freeverb_comb_setfeedback (&priv->combL[i], priv->roomsize);
        freeverb_comb_setfeedback (&priv->combR[i], priv->roomsize);
      }
      break;
    case PROP_DAMPING:
      filter->damping = g_value_get_float (value);
      priv->damp = filter->damping * scaledamp;
      for (i = 0; i < numcombs; i++) {
        freeverb_comb_setdamp (&priv->combL[i], priv->damp);
        freeverb_comb_setdamp (&priv->combR[i], priv->damp);
      }
      break;
    case PROP_PAN_WIDTH:
      filter->pan_width = g_value_get_float (value);
      priv->width = filter->pan_width;
      priv->wet1 = priv->wet * (priv->width / 2.0f + 0.5f);
      priv->wet2 = priv->wet * ((1.0f - priv->width) / 2.0f);
      break;
    case PROP_LEVEL:
      filter->level = g_value_get_float (value);
      priv->wet = filter->level * scalewet;
      priv->dry = (1.0 - filter->level) * scaledry;
      priv->wet1 = priv->wet * (priv->width / 2.0f + 0.5f);
      priv->wet2 = priv->wet * ((1.0f - priv->width) / 2.0f);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_freeverb_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstFreeverb *filter = GST_FREEVERB (object);

  switch (prop_id) {
    case PROP_ROOM_SIZE:
      g_value_set_float (value, filter->room_size);
      break;
    case PROP_DAMPING:
      g_value_set_float (value, filter->damping);
      break;
    case PROP_PAN_WIDTH:
      g_value_set_float (value, filter->pan_width);
      break;
    case PROP_LEVEL:
      g_value_set_float (value, filter->level);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

/* GstBaseTransform vmethod implementations */

static gboolean
gst_freeverb_get_unit_size (GstBaseTransform * base, GstCaps * caps,
    gsize * size)
{
  GstAudioInfo info;

  g_assert (size);

  if (!gst_audio_info_from_caps (&info, caps))
    return FALSE;

  *size = GST_AUDIO_INFO_BPF (&info);

  GST_INFO_OBJECT (base, "unit size: %" G_GSIZE_FORMAT, *size);

  return TRUE;
}

static GstCaps *
gst_freeverb_transform_caps (GstBaseTransform * base,
    GstPadDirection direction, GstCaps * caps, GstCaps * filter)
{
  GstCaps *res;
  GstStructure *structure;
  gint i;

  /* replace the channel property with our range. */
  res = gst_caps_copy (caps);
  for (i = 0; i < gst_caps_get_size (res); i++) {
    structure = gst_caps_get_structure (res, i);
    if (direction == GST_PAD_SRC) {
      GST_INFO_OBJECT (base, "[%d] allow 1-2 channels", i);
      gst_structure_set (structure, "channels", GST_TYPE_INT_RANGE, 1, 2, NULL);
    } else {
      GST_INFO_OBJECT (base, "[%d] allow 2 channels", i);
      gst_structure_set (structure, "channels", G_TYPE_INT, 2, NULL);
    }
    gst_structure_remove_field (structure, "channel-mask");
  }
  GST_DEBUG_OBJECT (base, "transformed %" GST_PTR_FORMAT, res);

  if (filter) {
    GstCaps *intersection;

    GST_DEBUG_OBJECT (base, "Using filter caps %" GST_PTR_FORMAT, filter);
    intersection =
        gst_caps_intersect_full (filter, res, GST_CAPS_INTERSECT_FIRST);
    gst_caps_unref (res);
    res = intersection;
    GST_DEBUG_OBJECT (base, "Intersection %" GST_PTR_FORMAT, res);
  }

  return res;
}

static gboolean
gst_freeverb_set_caps (GstBaseTransform * base, GstCaps * incaps,
    GstCaps * outcaps)
{
  GstFreeverb *filter = GST_FREEVERB (base);
  GstAudioInfo info;

  /*GST_INFO ("incaps are %" GST_PTR_FORMAT, incaps); */
  if (!gst_audio_info_from_caps (&info, incaps))
    goto no_format;

  GST_DEBUG ("try to process %d input with %d channels",
      GST_AUDIO_INFO_FORMAT (&info), GST_AUDIO_INFO_CHANNELS (&info));

  if (!gst_freeverb_set_process_function (filter, &info))
    goto no_format;

  filter->info = info;

  gst_freeverb_init_rev_model (filter);
  filter->drained = FALSE;
  GST_INFO_OBJECT (base, "model configured");

  return TRUE;

no_format:
  {
    GST_DEBUG ("invalid caps");
    return FALSE;
  }
}

static gboolean
gst_freeverb_transform_m2s_int (GstFreeverb * filter,
    gint16 * idata, gint16 * odata, guint num_samples)
{
  GstFreeverbPrivate *priv = filter->priv;
  gint i, k;
  gfloat out_l1, out_r1, input_1;
  gfloat out_l2, out_r2, input_2;
  gboolean drained = TRUE;

  for (k = 0; k < num_samples; k++) {
    out_l1 = out_r1 = 0.0;

    /* The original Freeverb code expects a stereo signal and 'input_1'
     * is set to the sum of the left and right input_1 sample. Since
     * this code works on a mono signal, 'input_1' is set to twice the
     * input_1 sample. */
    input_2 = (gfloat) * idata++;
    input_1 = (2.0f * input_2 + DC_OFFSET) * priv->gain;

    /* Accumulate comb filters in parallel */
    for (i = 0; i < numcombs; i++) {
      freeverb_comb_process (priv->combL[i], input_1, out_l1);
      freeverb_comb_process (priv->combR[i], input_1, out_r1);
    }
    /* Feed through allpasses in series */
    for (i = 0; i < numallpasses; i++) {
      freeverb_allpass_process (priv->allpassL[i], out_l1);
      freeverb_allpass_process (priv->allpassR[i], out_r1);
    }

    /* Remove the DC offset */
    out_l1 -= (gfloat) DC_OFFSET;
    out_r1 -= (gfloat) DC_OFFSET;

    /* Calculate output */
    out_l2 = out_l1 * priv->wet1 + out_r1 * priv->wet2 + input_2 * priv->dry;
    out_r2 = out_r1 * priv->wet1 + out_l1 * priv->wet2 + input_2 * priv->dry;
    out_l2 = CLAMP (out_l2, G_MININT16, G_MAXINT16);
    out_r2 = CLAMP (out_r2, G_MININT16, G_MAXINT16);
    *odata++ = (gint16) out_l2;
    *odata++ = (gint16) out_r2;

    if (abs ((gint16) out_l2) > 0 || abs ((gint16) out_r2) > 0)
      drained = FALSE;
  }
  return drained;
}

static gboolean
gst_freeverb_transform_s2s_int (GstFreeverb * filter,
    gint16 * idata, gint16 * odata, guint num_samples)
{
  GstFreeverbPrivate *priv = filter->priv;
  gint i, k;
  gfloat out_l1, out_r1, input_1l, input_1r;
  gfloat out_l2, out_r2, input_2l, input_2r;
  gboolean drained = TRUE;

  for (k = 0; k < num_samples; k++) {
    out_l1 = out_r1 = 0.0;

    input_2l = (gfloat) * idata++;
    input_2r = (gfloat) * idata++;
    input_1l = (input_2l + DC_OFFSET) * priv->gain;
    input_1r = (input_2r + DC_OFFSET) * priv->gain;

    /* Accumulate comb filters in parallel */
    for (i = 0; i < numcombs; i++) {
      freeverb_comb_process (priv->combL[i], input_1l, out_l1);
      freeverb_comb_process (priv->combR[i], input_1r, out_r1);
    }
    /* Feed through allpasses in series */
    for (i = 0; i < numallpasses; i++) {
      freeverb_allpass_process (priv->allpassL[i], out_l1);
      freeverb_allpass_process (priv->allpassR[i], out_r1);
    }

    /* Remove the DC offset */
    out_l1 -= (gfloat) DC_OFFSET;
    out_r1 -= (gfloat) DC_OFFSET;

    /* Calculate output */
    out_l2 = out_l1 * priv->wet1 + out_r1 * priv->wet2 + input_2l * priv->dry;
    out_r2 = out_r1 * priv->wet1 + out_l1 * priv->wet2 + input_2r * priv->dry;
    out_l2 = CLAMP (out_l2, G_MININT16, G_MAXINT16);
    out_r2 = CLAMP (out_r2, G_MININT16, G_MAXINT16);
    *odata++ = (gint16) out_l2;
    *odata++ = (gint16) out_r2;

    if (abs ((gint16) out_l2) > 0 || abs ((gint16) out_r2) > 0)
      drained = FALSE;
  }
  return drained;
}

static gboolean
gst_freeverb_transform_m2s_float (GstFreeverb * filter,
    gfloat * idata, gfloat * odata, guint num_samples)
{
  GstFreeverbPrivate *priv = filter->priv;
  gint i, k;
  gfloat out_l1, out_r1, input_1;
  gfloat out_l2, out_r2, input_2;
  gboolean drained = TRUE;

  for (k = 0; k < num_samples; k++) {
    out_l1 = out_r1 = 0.0;

    /* The original Freeverb code expects a stereo signal and 'input_1'
     * is set to the sum of the left and right input_1 sample. Since
     * this code works on a mono signal, 'input_1' is set to twice the
     * input_1 sample. */
    input_2 = *idata++;
    input_1 = (2.0f * input_2 + DC_OFFSET) * priv->gain;

    /* Accumulate comb filters in parallel */
    for (i = 0; i < numcombs; i++) {
      freeverb_comb_process (priv->combL[i], input_1, out_l1);
      freeverb_comb_process (priv->combR[i], input_1, out_r1);
    }
    /* Feed through allpasses in series */
    for (i = 0; i < numallpasses; i++) {
      freeverb_allpass_process (priv->allpassL[i], out_l1);
      freeverb_allpass_process (priv->allpassR[i], out_r1);
    }

    /* Remove the DC offset */
    out_l1 -= (gfloat) DC_OFFSET;
    out_r1 -= (gfloat) DC_OFFSET;

    /* Calculate output */
    out_l2 = out_l1 * priv->wet1 + out_r1 * priv->wet2 + input_2 * priv->dry;
    out_r2 = out_r1 * priv->wet1 + out_l1 * priv->wet2 + input_2 * priv->dry;
    *odata++ = out_l2;
    *odata++ = out_r2;

    if (fabs (out_l2) > 0 || fabs (out_r2) > 0)
      drained = FALSE;
  }
  return drained;
}

static gboolean
gst_freeverb_transform_s2s_float (GstFreeverb * filter,
    gfloat * idata, gfloat * odata, guint num_samples)
{
  GstFreeverbPrivate *priv = filter->priv;
  gint i, k;
  gfloat out_l1, out_r1, input_1l, input_1r;
  gfloat out_l2, out_r2, input_2l, input_2r;
  gboolean drained = TRUE;

  for (k = 0; k < num_samples; k++) {
    out_l1 = out_r1 = 0.0;

    input_2l = *idata++;
    input_2r = *idata++;
    input_1l = (input_2l + DC_OFFSET) * priv->gain;
    input_1r = (input_2r + DC_OFFSET) * priv->gain;

    /* Accumulate comb filters in parallel */
    for (i = 0; i < numcombs; i++) {
      freeverb_comb_process (priv->combL[i], input_1l, out_l1);
      freeverb_comb_process (priv->combR[i], input_1r, out_r1);
    }
    /* Feed through allpasses in series */
    for (i = 0; i < numallpasses; i++) {
      freeverb_allpass_process (priv->allpassL[i], out_l1);
      freeverb_allpass_process (priv->allpassR[i], out_r1);
    }

    /* Remove the DC offset */
    out_l1 -= (gfloat) DC_OFFSET;
    out_r1 -= (gfloat) DC_OFFSET;

    /* Calculate output */
    out_l2 = out_l1 * priv->wet1 + out_r1 * priv->wet2 + input_2l * priv->dry;
    out_r2 = out_r1 * priv->wet1 + out_l1 * priv->wet2 + input_2r * priv->dry;
    *odata++ = out_l2;
    *odata++ = out_r2;

    if (fabs (out_l2) > 0 || fabs (out_r2) > 0)
      drained = FALSE;
  }
  return drained;
}

/* this function does the actual processing
 */
static GstFlowReturn
gst_freeverb_transform (GstBaseTransform * base, GstBuffer * inbuf,
    GstBuffer * outbuf)
{
  GstFreeverb *filter = GST_FREEVERB (base);
  guint num_samples;
  GstClockTime timestamp;
  GstMapInfo inmap, outmap;

  timestamp = GST_BUFFER_TIMESTAMP (inbuf);
  timestamp =
      gst_segment_to_stream_time (&base->segment, GST_FORMAT_TIME, timestamp);

  gst_buffer_map (inbuf, &inmap, GST_MAP_READ);
  gst_buffer_map (outbuf, &outmap, GST_MAP_WRITE);
  num_samples = outmap.size / (2 * GST_AUDIO_INFO_BPS (&filter->info));

  GST_DEBUG_OBJECT (filter, "processing %u samples at %" GST_TIME_FORMAT,
      num_samples, GST_TIME_ARGS (timestamp));

  if (GST_CLOCK_TIME_IS_VALID (timestamp))
    gst_object_sync_values (GST_OBJECT (filter), timestamp);

  if (G_UNLIKELY (GST_BUFFER_FLAG_IS_SET (inbuf, GST_BUFFER_FLAG_DISCONT))) {
    filter->drained = FALSE;
  }
  if (G_UNLIKELY (GST_BUFFER_FLAG_IS_SET (inbuf, GST_BUFFER_FLAG_GAP))) {
    if (filter->drained) {
      memset (outmap.data, 0, outmap.size);
    }
  } else {
    filter->drained = FALSE;
  }

  if (!filter->drained) {
    filter->drained =
        filter->process (filter, inmap.data, outmap.data, num_samples);
  }

  if (filter->drained) {
    GST_BUFFER_FLAG_SET (outbuf, GST_BUFFER_FLAG_GAP);
  }

  gst_buffer_unmap (inbuf, &inmap);
  gst_buffer_unmap (outbuf, &outmap);

  return GST_FLOW_OK;
}


static gboolean
plugin_init (GstPlugin * plugin)
{
  return GST_ELEMENT_REGISTER (freeverb, plugin);
}

GST_PLUGIN_DEFINE (GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    freeverb,
    "Reverberation/room effect",
    plugin_init, VERSION, "LGPL", GST_PACKAGE_NAME, GST_PACKAGE_ORIGIN)
