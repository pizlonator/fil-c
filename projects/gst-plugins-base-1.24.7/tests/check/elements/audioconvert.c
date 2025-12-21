/* GStreamer
 *
 * unit test for audioconvert
 *
 * Copyright (C) <2005> Thomas Vander Stichele <thomas at apestaart dot org>
 * Copyright (C) <2007> Tim-Philipp Müller <tim centricular net>
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

#include <gst/check/gstcheck.h>
#include <gst/audio/audio.h>

/* For ease of programming we use globals to keep refs for our floating
 * src and sink pads we create; otherwise we always have to do get_pad,
 * get_peer, and then remove references in every test function */
static GstPad *mysrcpad, *mysinkpad;

#define FORMATS "{ F32LE, F32BE, F64LE, F64BE, " \
                  "S32LE, S32BE, U32LE, U32BE, " \
                  "S24LE, S24BE, U24LE, U24BE, " \
                  "S16LE, S16BE, U16LE, U16BE, " \
                  "S8, U8 } "

#define CONVERT_CAPS_TEMPLATE_STRING    \
  "audio/x-raw, " \
    "format = (string) "FORMATS", " \
    "rate = (int) [ 1, MAX ], " \
    "channels = (int) [ 1, MAX ]"

/* takes over reference for outcaps */
static GstElement *
setup_audioconvert (GstCaps * outcaps, gboolean use_mix_matrix,
    GValue * mix_matrix)
{
  GstPadTemplate *sinktemplate;
  static GstStaticPadTemplate srctemplate = GST_STATIC_PAD_TEMPLATE ("src",
      GST_PAD_SRC,
      GST_PAD_ALWAYS,
      GST_STATIC_CAPS (CONVERT_CAPS_TEMPLATE_STRING)
      );
  GstElement *audioconvert;

  sinktemplate =
      gst_pad_template_new ("sink", GST_PAD_SINK, GST_PAD_ALWAYS, outcaps);

  GST_DEBUG ("setup_audioconvert with caps %" GST_PTR_FORMAT, outcaps);
  audioconvert = gst_check_setup_element ("audioconvert");
  g_object_set (G_OBJECT (audioconvert), "dithering", 0, NULL);
  g_object_set (G_OBJECT (audioconvert), "noise-shaping", 0, NULL);
  if (use_mix_matrix) {
    g_object_set_property (G_OBJECT (audioconvert), "mix-matrix", mix_matrix);
  }
  mysrcpad = gst_check_setup_src_pad (audioconvert, &srctemplate);
  mysinkpad =
      gst_check_setup_sink_pad_from_template (audioconvert, sinktemplate);
  /* this installs a getcaps func that will always return the caps we set
   * later */
  gst_pad_use_fixed_caps (mysinkpad);

  gst_pad_set_active (mysrcpad, TRUE);
  gst_pad_set_active (mysinkpad, TRUE);

  gst_object_unref (sinktemplate);

  return audioconvert;
}

static void
cleanup_audioconvert (GstElement * audioconvert)
{
  GST_DEBUG ("cleanup_audioconvert");

  gst_pad_set_active (mysrcpad, FALSE);
  gst_pad_set_active (mysinkpad, FALSE);
  gst_check_teardown_src_pad (audioconvert);
  gst_check_teardown_sink_pad (audioconvert);
  gst_check_teardown_element (audioconvert);
}

/* returns a newly allocated caps */
static GstCaps *
get_int_caps (guint channels, gint endianness, guint width,
    guint depth, gboolean signedness, GstAudioLayout layout)
{
  GstCaps *caps;
  GstAudioFormat fmt;
  GstAudioInfo info;

  g_assert (channels <= 2);

  GST_DEBUG ("channels:%d, endianness:%d, width:%d, depth:%d, signedness:%d",
      channels, endianness, width, depth, signedness);

  fmt = gst_audio_format_build_integer (signedness, endianness, width, depth);

  gst_audio_info_init (&info);
  gst_audio_info_set_format (&info, fmt, GST_AUDIO_DEF_RATE, channels, NULL);
  info.layout = layout;

  caps = gst_audio_info_to_caps (&info);
  fail_unless (caps != NULL);
  GST_DEBUG ("returning caps %" GST_PTR_FORMAT, caps);

  return caps;
}

static GstAudioFormat
get_float_format (gint endianness, gint width)
{
  if (endianness == G_LITTLE_ENDIAN) {
    if (width == 32)
      return GST_AUDIO_FORMAT_F32LE;
    else
      return GST_AUDIO_FORMAT_F64LE;
  } else {
    if (width == 32)
      return GST_AUDIO_FORMAT_F32BE;
    else
      return GST_AUDIO_FORMAT_F64BE;
  }
}

/* returns a newly allocated caps */
static GstCaps *
get_float_caps (guint channels, gint endianness, guint width,
    GstAudioLayout layout)
{
  GstCaps *caps;
  GstAudioInfo info;

  g_assert (channels <= 2);

  gst_audio_info_init (&info);
  gst_audio_info_set_format (&info, get_float_format (endianness, width),
      GST_AUDIO_DEF_RATE, channels, NULL);
  info.layout = layout;

  caps = gst_audio_info_to_caps (&info);
  fail_unless (caps != NULL);
  GST_DEBUG ("returning caps %" GST_PTR_FORMAT, caps);

  return caps;
}

/* Copied from vorbis; the particular values used don't matter */
static GstAudioChannelPosition channelpositions[][6] = {
  {                             /* Mono */
      GST_AUDIO_CHANNEL_POSITION_MONO},
  {                             /* Stereo */
        GST_AUDIO_CHANNEL_POSITION_FRONT_LEFT,
      GST_AUDIO_CHANNEL_POSITION_FRONT_RIGHT},
  {                             /* Stereo + Centre */
        GST_AUDIO_CHANNEL_POSITION_FRONT_LEFT,
        GST_AUDIO_CHANNEL_POSITION_FRONT_RIGHT,
      GST_AUDIO_CHANNEL_POSITION_FRONT_CENTER},
  {                             /* Quadraphonic */
        GST_AUDIO_CHANNEL_POSITION_FRONT_LEFT,
        GST_AUDIO_CHANNEL_POSITION_FRONT_RIGHT,
        GST_AUDIO_CHANNEL_POSITION_REAR_LEFT,
        GST_AUDIO_CHANNEL_POSITION_REAR_RIGHT,
      },
  {                             /* Stereo + Centre + rear stereo */
        GST_AUDIO_CHANNEL_POSITION_FRONT_LEFT,
        GST_AUDIO_CHANNEL_POSITION_FRONT_RIGHT,
        GST_AUDIO_CHANNEL_POSITION_FRONT_CENTER,
        GST_AUDIO_CHANNEL_POSITION_REAR_LEFT,
        GST_AUDIO_CHANNEL_POSITION_REAR_RIGHT,
      },
  {                             /* Full 5.1 Surround */
        GST_AUDIO_CHANNEL_POSITION_FRONT_LEFT,
        GST_AUDIO_CHANNEL_POSITION_FRONT_RIGHT,
        GST_AUDIO_CHANNEL_POSITION_FRONT_CENTER,
        GST_AUDIO_CHANNEL_POSITION_LFE1,
        GST_AUDIO_CHANNEL_POSITION_REAR_LEFT,
        GST_AUDIO_CHANNEL_POSITION_REAR_RIGHT,
      }
};

/* we get this when recording from a soundcard with lots of input channels */
static GstAudioChannelPosition undefined_positions[][15] = {
  {
      GST_AUDIO_CHANNEL_POSITION_NONE},
  {
        GST_AUDIO_CHANNEL_POSITION_NONE,
      GST_AUDIO_CHANNEL_POSITION_NONE},
  {
        GST_AUDIO_CHANNEL_POSITION_NONE,
        GST_AUDIO_CHANNEL_POSITION_NONE,
      GST_AUDIO_CHANNEL_POSITION_NONE},
  {
        GST_AUDIO_CHANNEL_POSITION_NONE,
        GST_AUDIO_CHANNEL_POSITION_NONE,
        GST_AUDIO_CHANNEL_POSITION_NONE,
      GST_AUDIO_CHANNEL_POSITION_NONE},
  {
        GST_AUDIO_CHANNEL_POSITION_NONE,
        GST_AUDIO_CHANNEL_POSITION_NONE,
        GST_AUDIO_CHANNEL_POSITION_NONE,
        GST_AUDIO_CHANNEL_POSITION_NONE,
      GST_AUDIO_CHANNEL_POSITION_NONE},
  {
        GST_AUDIO_CHANNEL_POSITION_NONE,
        GST_AUDIO_CHANNEL_POSITION_NONE,
        GST_AUDIO_CHANNEL_POSITION_NONE,
        GST_AUDIO_CHANNEL_POSITION_NONE,
        GST_AUDIO_CHANNEL_POSITION_NONE,
      GST_AUDIO_CHANNEL_POSITION_NONE},
  {
        GST_AUDIO_CHANNEL_POSITION_NONE,
        GST_AUDIO_CHANNEL_POSITION_NONE,
        GST_AUDIO_CHANNEL_POSITION_NONE,
        GST_AUDIO_CHANNEL_POSITION_NONE,
        GST_AUDIO_CHANNEL_POSITION_NONE,
        GST_AUDIO_CHANNEL_POSITION_NONE,
      GST_AUDIO_CHANNEL_POSITION_NONE},
  {
        GST_AUDIO_CHANNEL_POSITION_NONE,
        GST_AUDIO_CHANNEL_POSITION_NONE,
        GST_AUDIO_CHANNEL_POSITION_NONE,
        GST_AUDIO_CHANNEL_POSITION_NONE,
        GST_AUDIO_CHANNEL_POSITION_NONE,
        GST_AUDIO_CHANNEL_POSITION_NONE,
        GST_AUDIO_CHANNEL_POSITION_NONE,
      GST_AUDIO_CHANNEL_POSITION_NONE},
  {
        GST_AUDIO_CHANNEL_POSITION_NONE,
        GST_AUDIO_CHANNEL_POSITION_NONE,
        GST_AUDIO_CHANNEL_POSITION_NONE,
        GST_AUDIO_CHANNEL_POSITION_NONE,
        GST_AUDIO_CHANNEL_POSITION_NONE,
        GST_AUDIO_CHANNEL_POSITION_NONE,
        GST_AUDIO_CHANNEL_POSITION_NONE,
        GST_AUDIO_CHANNEL_POSITION_NONE,
      GST_AUDIO_CHANNEL_POSITION_NONE},
  {
        GST_AUDIO_CHANNEL_POSITION_NONE,
        GST_AUDIO_CHANNEL_POSITION_NONE,
        GST_AUDIO_CHANNEL_POSITION_NONE,
        GST_AUDIO_CHANNEL_POSITION_NONE,
        GST_AUDIO_CHANNEL_POSITION_NONE,
        GST_AUDIO_CHANNEL_POSITION_NONE,
        GST_AUDIO_CHANNEL_POSITION_NONE,
        GST_AUDIO_CHANNEL_POSITION_NONE,
        GST_AUDIO_CHANNEL_POSITION_NONE,
      GST_AUDIO_CHANNEL_POSITION_NONE},
  {
        GST_AUDIO_CHANNEL_POSITION_NONE,
        GST_AUDIO_CHANNEL_POSITION_NONE,
        GST_AUDIO_CHANNEL_POSITION_NONE,
        GST_AUDIO_CHANNEL_POSITION_NONE,
        GST_AUDIO_CHANNEL_POSITION_NONE,
        GST_AUDIO_CHANNEL_POSITION_NONE,
        GST_AUDIO_CHANNEL_POSITION_NONE,
        GST_AUDIO_CHANNEL_POSITION_NONE,
        GST_AUDIO_CHANNEL_POSITION_NONE,
        GST_AUDIO_CHANNEL_POSITION_NONE,
      GST_AUDIO_CHANNEL_POSITION_NONE},
  {
        GST_AUDIO_CHANNEL_POSITION_NONE,
        GST_AUDIO_CHANNEL_POSITION_NONE,
        GST_AUDIO_CHANNEL_POSITION_NONE,
        GST_AUDIO_CHANNEL_POSITION_NONE,
        GST_AUDIO_CHANNEL_POSITION_NONE,
        GST_AUDIO_CHANNEL_POSITION_NONE,
        GST_AUDIO_CHANNEL_POSITION_NONE,
        GST_AUDIO_CHANNEL_POSITION_NONE,
        GST_AUDIO_CHANNEL_POSITION_NONE,
        GST_AUDIO_CHANNEL_POSITION_NONE,
        GST_AUDIO_CHANNEL_POSITION_NONE,
      GST_AUDIO_CHANNEL_POSITION_NONE},
  {
        GST_AUDIO_CHANNEL_POSITION_NONE,
        GST_AUDIO_CHANNEL_POSITION_NONE,
        GST_AUDIO_CHANNEL_POSITION_NONE,
        GST_AUDIO_CHANNEL_POSITION_NONE,
        GST_AUDIO_CHANNEL_POSITION_NONE,
        GST_AUDIO_CHANNEL_POSITION_NONE,
        GST_AUDIO_CHANNEL_POSITION_NONE,
        GST_AUDIO_CHANNEL_POSITION_NONE,
        GST_AUDIO_CHANNEL_POSITION_NONE,
        GST_AUDIO_CHANNEL_POSITION_NONE,
        GST_AUDIO_CHANNEL_POSITION_NONE,
        GST_AUDIO_CHANNEL_POSITION_NONE,
      GST_AUDIO_CHANNEL_POSITION_NONE},
  {
        GST_AUDIO_CHANNEL_POSITION_NONE,
        GST_AUDIO_CHANNEL_POSITION_NONE,
        GST_AUDIO_CHANNEL_POSITION_NONE,
        GST_AUDIO_CHANNEL_POSITION_NONE,
        GST_AUDIO_CHANNEL_POSITION_NONE,
        GST_AUDIO_CHANNEL_POSITION_NONE,
        GST_AUDIO_CHANNEL_POSITION_NONE,
        GST_AUDIO_CHANNEL_POSITION_NONE,
        GST_AUDIO_CHANNEL_POSITION_NONE,
        GST_AUDIO_CHANNEL_POSITION_NONE,
        GST_AUDIO_CHANNEL_POSITION_NONE,
        GST_AUDIO_CHANNEL_POSITION_NONE,
        GST_AUDIO_CHANNEL_POSITION_NONE,
      GST_AUDIO_CHANNEL_POSITION_NONE},
  {
        GST_AUDIO_CHANNEL_POSITION_NONE,
        GST_AUDIO_CHANNEL_POSITION_NONE,
        GST_AUDIO_CHANNEL_POSITION_NONE,
        GST_AUDIO_CHANNEL_POSITION_NONE,
        GST_AUDIO_CHANNEL_POSITION_NONE,
        GST_AUDIO_CHANNEL_POSITION_NONE,
        GST_AUDIO_CHANNEL_POSITION_NONE,
        GST_AUDIO_CHANNEL_POSITION_NONE,
        GST_AUDIO_CHANNEL_POSITION_NONE,
        GST_AUDIO_CHANNEL_POSITION_NONE,
        GST_AUDIO_CHANNEL_POSITION_NONE,
        GST_AUDIO_CHANNEL_POSITION_NONE,
        GST_AUDIO_CHANNEL_POSITION_NONE,
        GST_AUDIO_CHANNEL_POSITION_NONE,
      GST_AUDIO_CHANNEL_POSITION_NONE}
};

/* For channels > 2, caps have to have channel positions. This adds some simple
 * ones. Only implemented for channels between 1 and 6.
 */
static GstCaps *
get_float_mc_caps (guint channels, gint endianness, guint width,
    GstAudioLayout layout, const GstAudioChannelPosition * position)
{
  GstCaps *caps;
  GstAudioInfo info;

  gst_audio_info_init (&info);

  if (position) {
    gst_audio_info_set_format (&info, get_float_format (endianness, width),
        GST_AUDIO_DEF_RATE, channels, position);
  } else if (channels <= 6) {
    gst_audio_info_set_format (&info, get_float_format (endianness, width),
        GST_AUDIO_DEF_RATE, channels, channelpositions[channels - 1]);
  } else {
    GstAudioChannelPosition pos[64];
    gint i;

    for (i = 0; i < 64; i++)
      pos[i] = GST_AUDIO_CHANNEL_POSITION_NONE;
    gst_audio_info_set_format (&info, get_float_format (endianness, width),
        GST_AUDIO_DEF_RATE, channels, pos);
  }

  info.layout = layout;

  caps = gst_audio_info_to_caps (&info);
  fail_unless (caps != NULL);

  GST_DEBUG ("returning caps %" GST_PTR_FORMAT, caps);

  return caps;
}

static GstCaps *
get_int_mc_caps (guint channels, gint endianness, guint width,
    guint depth, gboolean signedness, GstAudioLayout layout,
    const GstAudioChannelPosition * position)
{
  GstCaps *caps;
  GstAudioFormat fmt;
  GstAudioInfo info;

  fmt = gst_audio_format_build_integer (signedness, endianness, width, depth);

  gst_audio_info_init (&info);

  if (position) {
    gst_audio_info_set_format (&info, fmt, GST_AUDIO_DEF_RATE, channels,
        position);
  } else if (channels <= 6) {
    gst_audio_info_set_format (&info, fmt, GST_AUDIO_DEF_RATE, channels,
        channelpositions[channels - 1]);
  } else {
    GstAudioChannelPosition pos[64];
    gint i;

    for (i = 0; i < 64; i++)
      pos[i] = GST_AUDIO_CHANNEL_POSITION_NONE;
    gst_audio_info_set_format (&info, fmt, GST_AUDIO_DEF_RATE, channels, pos);
  }

  info.layout = layout;

  caps = gst_audio_info_to_caps (&info);
  fail_unless (caps != NULL);

  GST_DEBUG ("returning caps %" GST_PTR_FORMAT, caps);

  return caps;
}

/* eats the refs to the caps */
static void
verify_convert (const gchar * which, void *in, int inlength,
    GstCaps * incaps, void *out, int outlength, GstCaps * outcaps,
    GstFlowReturn expected_flow, gboolean in_place_allowed,
    gboolean use_mix_matrix, GValue * mix_matrix)
{
  GstBuffer *inbuffer, *outbuffer;
  GstElement *audioconvert;
  GstAudioInfo in_info;

  GST_DEBUG ("verifying conversion %s", which);
  GST_DEBUG ("incaps: %" GST_PTR_FORMAT, incaps);
  GST_DEBUG ("outcaps: %" GST_PTR_FORMAT, outcaps);
  ASSERT_CAPS_REFCOUNT (incaps, "incaps", 1);
  ASSERT_CAPS_REFCOUNT (outcaps, "outcaps", 1);
  audioconvert = setup_audioconvert (outcaps, use_mix_matrix, mix_matrix);
  ASSERT_CAPS_REFCOUNT (outcaps, "outcaps", 2);

  fail_unless (gst_element_set_state (audioconvert,
          GST_STATE_PLAYING) == GST_STATE_CHANGE_SUCCESS,
      "could not set to playing");

  gst_check_setup_events (mysrcpad, audioconvert, incaps, GST_FORMAT_TIME);

  GST_DEBUG ("Creating buffer of %d bytes", inlength);
  inbuffer = gst_buffer_new_and_alloc (inlength);
  gst_buffer_fill (inbuffer, 0, in, inlength);
  ASSERT_BUFFER_REFCOUNT (inbuffer, "inbuffer", 1);

  fail_unless (gst_audio_info_from_caps (&in_info, incaps));
  gst_buffer_add_audio_meta (inbuffer, &in_info, inlength / in_info.bpf, NULL);

  if (!in_place_allowed) {
    /* take extra ref to force processing */
    gst_buffer_ref (inbuffer);
    gst_base_transform_set_passthrough (GST_BASE_TRANSFORM (audioconvert),
        FALSE);
  }

  /* pushing gives away my reference ... */
  GST_DEBUG ("push it");
  fail_unless_equals_int (gst_pad_push (mysrcpad, inbuffer), expected_flow);
  GST_DEBUG ("pushed it");

  if (expected_flow != GST_FLOW_OK)
    goto done;

  /* ... and puts a new buffer on the global list */
  fail_unless (g_list_length (buffers) == 1);
  fail_if ((outbuffer = (GstBuffer *) buffers->data) == NULL);

  if (!in_place_allowed) {
    /* release extra ref */
    gst_buffer_unref (inbuffer);
  }

  ASSERT_BUFFER_REFCOUNT (outbuffer, "outbuffer", 1);
  fail_unless_equals_int (gst_buffer_get_size (outbuffer), outlength);

  gst_check_buffer_data (outbuffer, out, outlength);

  /* make sure that the channel positions are not lost */
  {
    GstStructure *in_s, *out_s;
    gint out_chans;
    GstCaps *ccaps;

    in_s = gst_caps_get_structure (incaps, 0);
    ccaps = gst_pad_get_current_caps (mysinkpad);
    out_s = gst_caps_get_structure (ccaps, 0);
    fail_unless (gst_structure_get_int (out_s, "channels", &out_chans));

    /* positions for 1 and 2 channels are implicit if not provided */
    if (out_chans > 2 && gst_structure_has_field (in_s, "channel-mask")) {
      if (!gst_structure_has_field (out_s, "channel-mask")) {
        gst_printerrln ("In  : %" GST_PTR_FORMAT, in_s);
        gst_printerrln ("Out : %" GST_PTR_FORMAT, out_s);
        g_error ("Channel layout got lost somewhere\n");
      }
    }
    gst_caps_unref (ccaps);
  }

  buffers = g_list_remove (buffers, outbuffer);
  gst_buffer_unref (outbuffer);

done:
  fail_unless (gst_element_set_state (audioconvert,
          GST_STATE_NULL) == GST_STATE_CHANGE_SUCCESS, "could not set to null");
  /* cleanup */
  GST_DEBUG ("cleanup audioconvert");
  cleanup_audioconvert (audioconvert);
  GST_DEBUG ("cleanup, unref incaps");
  gst_caps_unref (incaps);
  gst_caps_unref (outcaps);
}


#define RUN_CONVERSION(which, inarray, in_get_caps, outarray, out_get_caps)    \
  verify_convert (which, inarray, sizeof (inarray),                            \
        in_get_caps, outarray, sizeof (outarray), out_get_caps, GST_FLOW_OK,   \
        TRUE, FALSE, &(GValue) G_VALUE_INIT);

#define RUN_CONVERSION_WITH_MATRIX(which, inarray, in_get_caps, outarray, out_get_caps, mix_matrix)    \
  verify_convert (which, inarray, sizeof (inarray),                            \
        in_get_caps, outarray, sizeof (outarray), out_get_caps, GST_FLOW_OK,   \
        TRUE, TRUE, mix_matrix);

#define RUN_CONVERSION_TO_FAIL(which, inarray, in_caps, outarray, out_caps)    \
  verify_convert (which, inarray, sizeof (inarray),                            \
        in_caps, outarray, sizeof (outarray), out_caps,                        \
        GST_FLOW_NOT_NEGOTIATED, TRUE, FALSE, &(GValue) G_VALUE_INIT);

#define RUN_CONVERSION_NOT_INPLACE(which, inarray, in_get_caps, outarray, out_get_caps)    \
  verify_convert (which, inarray, sizeof (inarray),                            \
        in_get_caps, outarray, sizeof (outarray), out_get_caps, GST_FLOW_OK,   \
        FALSE, FALSE, &(GValue) G_VALUE_INIT);

#define INTERLEAVED GST_AUDIO_LAYOUT_INTERLEAVED
#define PLANAR GST_AUDIO_LAYOUT_NON_INTERLEAVED

GST_START_TEST (test_int16)
{
  /* stereo to mono */
  {
    gint16 in_i[] = { 16384, -256, 1024, 1024 };
    gint16 in_p[] = { 16384, 1024, -256, 1024 };
    gint16 out[] = { 8064, 1024 };

    RUN_CONVERSION ("int16 stereo to mono interleaved",
        in_i, get_int_caps (2, G_BYTE_ORDER, 16, 16, TRUE, INTERLEAVED),
        out, get_int_caps (1, G_BYTE_ORDER, 16, 16, TRUE, INTERLEAVED));
    RUN_CONVERSION ("int16 stereo to mono planar",
        in_p, get_int_caps (2, G_BYTE_ORDER, 16, 16, TRUE, PLANAR),
        out, get_int_caps (1, G_BYTE_ORDER, 16, 16, TRUE, PLANAR));
  }
  /* mono to stereo */
  {
    gint16 in[] = { 512, 1024 };
    gint16 out_i[] = { 512, 512, 1024, 1024 };
    gint16 out_p[] = { 512, 1024, 512, 1024 };

    RUN_CONVERSION ("int16 mono to stereo interleaved",
        in, get_int_caps (1, G_BYTE_ORDER, 16, 16, TRUE, INTERLEAVED),
        out_i, get_int_caps (2, G_BYTE_ORDER, 16, 16, TRUE, INTERLEAVED));
    RUN_CONVERSION ("int16 mono to stereo planar",
        in, get_int_caps (1, G_BYTE_ORDER, 16, 16, TRUE, PLANAR),
        out_p, get_int_caps (2, G_BYTE_ORDER, 16, 16, TRUE, PLANAR));
  }
  /* signed -> unsigned */
  {
    gint16 in[] = { 0, -32767, 32767, -32768 };
    guint16 out[] = { 32768, 1, 65535, 0 };

    RUN_CONVERSION ("int16 signed to unsigned interleaved",
        in, get_int_caps (1, G_BYTE_ORDER, 16, 16, TRUE, INTERLEAVED),
        out, get_int_caps (1, G_BYTE_ORDER, 16, 16, FALSE, INTERLEAVED));
    RUN_CONVERSION ("int16 unsigned to signed interleaved",
        out, get_int_caps (1, G_BYTE_ORDER, 16, 16, FALSE, INTERLEAVED),
        in, get_int_caps (1, G_BYTE_ORDER, 16, 16, TRUE, INTERLEAVED));

    RUN_CONVERSION ("int16 signed to unsigned planar",
        in, get_int_caps (1, G_BYTE_ORDER, 16, 16, TRUE, PLANAR),
        out, get_int_caps (1, G_BYTE_ORDER, 16, 16, FALSE, PLANAR));
    RUN_CONVERSION ("int16 unsigned to signed planar",
        out, get_int_caps (1, G_BYTE_ORDER, 16, 16, FALSE, PLANAR),
        in, get_int_caps (1, G_BYTE_ORDER, 16, 16, TRUE, PLANAR));
  }
}

GST_END_TEST;


GST_START_TEST (test_float32)
{
  /* stereo to mono */
  {
    gfloat in_i[] = { 0.6, -0.0078125, 0.03125, 0.03125 };
    gfloat in_p[] = { 0.6, 0.03125, -0.0078125, 0.03125 };
    gfloat out[] = { 0.29609375, 0.03125 };

    RUN_CONVERSION ("float32 stereo to mono interleaved",
        in_i, get_float_caps (2, G_BYTE_ORDER, 32, INTERLEAVED),
        out, get_float_caps (1, G_BYTE_ORDER, 32, INTERLEAVED));
    RUN_CONVERSION ("float32 stereo to mono planar",
        in_p, get_float_caps (2, G_BYTE_ORDER, 32, PLANAR),
        out, get_float_caps (1, G_BYTE_ORDER, 32, PLANAR));
  }
  /* mono to stereo */
  {
    gfloat in[] = { 0.015625, 0.03125 };
    gfloat out_i[] = { 0.015625, 0.015625, 0.03125, 0.03125 };
    gfloat out_p[] = { 0.015625, 0.03125, 0.015625, 0.03125 };

    RUN_CONVERSION ("float32 mono to stereo interleaved",
        in, get_float_caps (1, G_BYTE_ORDER, 32, INTERLEAVED),
        out_i, get_float_caps (2, G_BYTE_ORDER, 32, INTERLEAVED));
    RUN_CONVERSION ("float32 mono to stereo planar",
        in, get_float_caps (1, G_BYTE_ORDER, 32, PLANAR),
        out_p, get_float_caps (2, G_BYTE_ORDER, 32, PLANAR));
  }
}

GST_END_TEST;


GST_START_TEST (test_int_conversion)
{
  /* 8 <-> 16 signed */
  /* NOTE: if audioconvert was doing dithering we'd have a problem */
  {
    gint8 in[] = { 0, 1, 2, 127, -127 };
    gint16 out[] = { 0, 256, 512, 32512, -32512 };

    RUN_CONVERSION ("int 8bit to 16bit signed",
        in, get_int_caps (1, G_BYTE_ORDER, 8, 8, TRUE, INTERLEAVED),
        out, get_int_caps (1, G_BYTE_ORDER, 16, 16, TRUE, INTERLEAVED)
        );
    RUN_CONVERSION ("int 16bit signed to 8bit",
        out, get_int_caps (1, G_BYTE_ORDER, 16, 16, TRUE, INTERLEAVED),
        in, get_int_caps (1, G_BYTE_ORDER, 8, 8, TRUE, INTERLEAVED)
        );
  }
  /* 16 -> 8 signed */
  {
    gint16 in[] = { 0, 127, 128, 256, 256 + 127, 256 + 128 };
    gint8 out[] = { 0, 0, 1, 1, 1, 2 };

    RUN_CONVERSION ("16 bit to 8 signed",
        in, get_int_caps (1, G_BYTE_ORDER, 16, 16, TRUE, INTERLEAVED),
        out, get_int_caps (1, G_BYTE_ORDER, 8, 8, TRUE, INTERLEAVED)
        );
  }
  /* 8 unsigned <-> 16 signed */
  /* NOTE: if audioconvert was doing dithering we'd have a problem */
  {
    guint8 in[] = { 128, 129, 130, 255, 1 };
    gint16 out[] = { 0, 256, 512, 32512, -32512 };
    GstCaps *incaps, *outcaps;

    /* exploded for easier valgrinding */
    incaps = get_int_caps (1, G_BYTE_ORDER, 8, 8, FALSE, INTERLEAVED);
    outcaps = get_int_caps (1, G_BYTE_ORDER, 16, 16, TRUE, INTERLEAVED);
    GST_DEBUG ("incaps: %" GST_PTR_FORMAT, incaps);
    GST_DEBUG ("outcaps: %" GST_PTR_FORMAT, outcaps);
    RUN_CONVERSION ("8 unsigned to 16 signed", in, incaps, out, outcaps);
    RUN_CONVERSION ("16 signed to 8 unsigned", out, get_int_caps (1,
            G_BYTE_ORDER, 16, 16, TRUE, INTERLEAVED), in, get_int_caps (1,
            G_BYTE_ORDER, 8, 8, FALSE, INTERLEAVED)
        );
  }
  /* 8 <-> 24 signed */
  /* NOTE: if audioconvert was doing dithering we'd have a problem */
  {
    gint8 in[] = { 0, 1, 127 };
    guint8 out[] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x7f };
    /* out has the bytes in little-endian, so that's how they should be
     * interpreted during conversion */

    RUN_CONVERSION ("8 to 24 signed", in, get_int_caps (1, G_BYTE_ORDER, 8, 8,
            TRUE, INTERLEAVED), out, get_int_caps (1, G_LITTLE_ENDIAN, 24, 24,
            TRUE, INTERLEAVED)
        );
    RUN_CONVERSION ("24 signed to 8", out, get_int_caps (1, G_LITTLE_ENDIAN, 24,
            24, TRUE, INTERLEAVED), in, get_int_caps (1, G_BYTE_ORDER, 8, 8,
            TRUE, INTERLEAVED)
        );
  }

  /* 16 bit signed <-> unsigned */
  {
    gint16 in[] = { 0, 128, -128 };
    guint16 out[] = { 32768, 32896, 32640 };
    RUN_CONVERSION ("16 signed to 16 unsigned",
        in, get_int_caps (1, G_BYTE_ORDER, 16, 16, TRUE, INTERLEAVED),
        out, get_int_caps (1, G_BYTE_ORDER, 16, 16, FALSE, INTERLEAVED)
        );
    RUN_CONVERSION ("16 unsigned to 16 signed",
        out, get_int_caps (1, G_BYTE_ORDER, 16, 16, FALSE, INTERLEAVED),
        in, get_int_caps (1, G_BYTE_ORDER, 16, 16, TRUE, INTERLEAVED)
        );
  }

  /* 32 bit signed -> 16 bit signed for rounding check */
  /* NOTE: if audioconvert was doing dithering we'd have a problem */
  {
    gint32 in[] = { 0, G_MININT32, G_MAXINT32,
      (32 << 16), (32 << 16) + (1 << 15), (32 << 16) - (1 << 15),
      (32 << 16) + (2 << 15), (32 << 16) - (2 << 15),
      (-(32 << 16)) + (1 << 15), (-(32 << 16)) - (1 << 15),
      (-(32 << 16)) + (2 << 15), (-(32 << 16)) - (2 << 15),
      (-(32 << 16))
    };
    gint16 out[] = { 0, G_MININT16, G_MAXINT16,
      32, 33, 32,
      33, 31,
      -31, -32,
      -31, -33,
      -32
    };
    RUN_CONVERSION ("32 signed to 16 signed for rounding",
        in, get_int_caps (1, G_BYTE_ORDER, 32, 32, TRUE, INTERLEAVED),
        out, get_int_caps (1, G_BYTE_ORDER, 16, 16, TRUE, INTERLEAVED)
        );
  }

  /* 32 bit signed -> 16 bit unsigned for rounding check */
  /* NOTE: if audioconvert was doing dithering we'd have a problem */
  {
    gint32 in[] = { 0, G_MININT32, G_MAXINT32,
      (32 << 16), (32 << 16) + (1 << 15), (32 << 16) - (1 << 15),
      (32 << 16) + (2 << 15), (32 << 16) - (2 << 15),
      (-(32 << 16)) + (1 << 15), (-(32 << 16)) - (1 << 15),
      (-(32 << 16)) + (2 << 15), (-(32 << 16)) - (2 << 15),
      (-(32 << 16))
    };
    guint16 out[] = { (1 << 15), 0, G_MAXUINT16,
      (1 << 15) + 32, (1 << 15) + 33, (1 << 15) + 32,
      (1 << 15) + 33, (1 << 15) + 31,
      (1 << 15) - 31, (1 << 15) - 32,
      (1 << 15) - 31, (1 << 15) - 33,
      (1 << 15) - 32
    };
    RUN_CONVERSION ("32 signed to 16 unsigned for rounding",
        in, get_int_caps (1, G_BYTE_ORDER, 32, 32, TRUE, INTERLEAVED),
        out, get_int_caps (1, G_BYTE_ORDER, 16, 16, FALSE, INTERLEAVED)
        );
  }
}

GST_END_TEST;

GST_START_TEST (test_float_conversion)
{
  /* 64-bit float <-> 32-bit float */
  {
    gdouble in[] = { 0.0, 1.0, -1.0, 0.5, -0.5 };
    gfloat out[] = { 0.0, 1.0, -1.0, 0.5, -0.5 };

    RUN_CONVERSION ("64 float to 32 float",
        in, get_float_caps (1, G_BYTE_ORDER, 64, INTERLEAVED),
        out, get_float_caps (1, G_BYTE_ORDER, 32, INTERLEAVED));

    RUN_CONVERSION ("32 float to 64 float",
        out, get_float_caps (1, G_BYTE_ORDER, 32, INTERLEAVED),
        in, get_float_caps (1, G_BYTE_ORDER, 64, INTERLEAVED));
  }

  /* 32-bit float little endian <-> big endian */
  {
    gfloat le[] = { GFLOAT_TO_LE (0.0), GFLOAT_TO_LE (1.0), GFLOAT_TO_LE (-1.0),
      GFLOAT_TO_LE (0.5), GFLOAT_TO_LE (-0.5)
    };
    gfloat be[] = { GFLOAT_TO_BE (0.0), GFLOAT_TO_BE (1.0), GFLOAT_TO_BE (-1.0),
      GFLOAT_TO_BE (0.5), GFLOAT_TO_BE (-0.5)
    };

    RUN_CONVERSION ("32 float LE to BE",
        le, get_float_caps (1, G_LITTLE_ENDIAN, 32, INTERLEAVED),
        be, get_float_caps (1, G_BIG_ENDIAN, 32, INTERLEAVED));

    RUN_CONVERSION ("32 float BE to LE",
        be, get_float_caps (1, G_BIG_ENDIAN, 32, INTERLEAVED),
        le, get_float_caps (1, G_LITTLE_ENDIAN, 32, INTERLEAVED));
  }

  /* 64-bit float little endian <-> big endian */
  {
    gdouble le[] =
        { GDOUBLE_TO_LE (0.0), GDOUBLE_TO_LE (1.0), GDOUBLE_TO_LE (-1.0),
      GDOUBLE_TO_LE (0.5), GDOUBLE_TO_LE (-0.5)
    };
    gdouble be[] =
        { GDOUBLE_TO_BE (0.0), GDOUBLE_TO_BE (1.0), GDOUBLE_TO_BE (-1.0),
      GDOUBLE_TO_BE (0.5), GDOUBLE_TO_BE (-0.5)
    };

    RUN_CONVERSION ("64 float LE to BE",
        le, get_float_caps (1, G_LITTLE_ENDIAN, 64, INTERLEAVED),
        be, get_float_caps (1, G_BIG_ENDIAN, 64, INTERLEAVED));

    RUN_CONVERSION ("64 float BE to LE",
        be, get_float_caps (1, G_BIG_ENDIAN, 64, INTERLEAVED),
        le, get_float_caps (1, G_LITTLE_ENDIAN, 64, INTERLEAVED));
  }
}

GST_END_TEST;

GST_START_TEST (test_int_float_conversion)
{
  /* 32 float <-> 16 signed */
  /* NOTE: if audioconvert was doing dithering we'd have a problem */
  {
    gfloat in_le[] =
        { GFLOAT_TO_LE (0.0), GFLOAT_TO_LE (1.0), GFLOAT_TO_LE (-1.0),
      GFLOAT_TO_LE (0.5), GFLOAT_TO_LE (-0.5), GFLOAT_TO_LE (1.1),
      GFLOAT_TO_LE (-1.1)
    };
    gfloat in_be[] =
        { GFLOAT_TO_BE (0.0), GFLOAT_TO_BE (1.0), GFLOAT_TO_BE (-1.0),
      GFLOAT_TO_BE (0.5), GFLOAT_TO_BE (-0.5), GFLOAT_TO_BE (1.1),
      GFLOAT_TO_BE (-1.1)
    };
    gint16 out[] = { 0, 32767, -32768, 16384, -16384, 32767, -32768 };

    /* only one direction conversion, the other direction does
     * not produce exactly the same as the input due to floating
     * point rounding errors etc. */
    RUN_CONVERSION ("32 float le to 16 signed",
        in_le, get_float_caps (1, G_LITTLE_ENDIAN, 32, INTERLEAVED),
        out, get_int_caps (1, G_BYTE_ORDER, 16, 16, TRUE, INTERLEAVED));
    RUN_CONVERSION ("32 float be to 16 signed",
        in_be, get_float_caps (1, G_BIG_ENDIAN, 32, INTERLEAVED),
        out, get_int_caps (1, G_BYTE_ORDER, 16, 16, TRUE, INTERLEAVED));
  }

  {
    gint16 in[] = { 0, -32768, 16384, -16384 };
    gfloat out[] = { 0.0, -1.0, 0.5, -0.5 };

    RUN_CONVERSION ("16 signed to 32 float",
        in, get_int_caps (1, G_BYTE_ORDER, 16, 16, TRUE, INTERLEAVED),
        out, get_float_caps (1, G_BYTE_ORDER, 32, INTERLEAVED));
  }

  /* 64 float <-> 16 signed */
  /* NOTE: if audioconvert was doing dithering we'd have a problem */
  {
    gdouble in_le[] =
        { GDOUBLE_TO_LE (0.0), GDOUBLE_TO_LE (1.0), GDOUBLE_TO_LE (-1.0),
      GDOUBLE_TO_LE (0.5), GDOUBLE_TO_LE (-0.5), GDOUBLE_TO_LE (1.1),
      GDOUBLE_TO_LE (-1.1)
    };
    gdouble in_be[] =
        { GDOUBLE_TO_BE (0.0), GDOUBLE_TO_BE (1.0), GDOUBLE_TO_BE (-1.0),
      GDOUBLE_TO_BE (0.5), GDOUBLE_TO_BE (-0.5), GDOUBLE_TO_BE (1.1),
      GDOUBLE_TO_BE (-1.1)
    };
    gint16 out[] = { 0, 32767, -32768, 16384, -16384, 32767, -32768 };

    /* only one direction conversion, the other direction does
     * not produce exactly the same as the input due to floating
     * point rounding errors etc. */
    RUN_CONVERSION ("64 float LE to 16 signed",
        in_le, get_float_caps (1, G_LITTLE_ENDIAN, 64, INTERLEAVED),
        out, get_int_caps (1, G_BYTE_ORDER, 16, 16, TRUE, INTERLEAVED));
    RUN_CONVERSION ("64 float BE to 16 signed",
        in_be, get_float_caps (1, G_BIG_ENDIAN, 64, INTERLEAVED),
        out, get_int_caps (1, G_BYTE_ORDER, 16, 16, TRUE, INTERLEAVED));
  }
  {
    gint16 in[] = { 0, -32768, 16384, -16384 };
    gdouble out[] = { 0.0,
      (gdouble) (-(G_GINT64_CONSTANT (32768) << 16)) / 2147483648.0,    /* ~ -1.0 */
      (gdouble) (16384L << 16) / 2147483648.0,  /* ~  0.5 */
      (gdouble) (-(16384L << 16)) / 2147483648.0,       /* ~ -0.5 */
    };

    RUN_CONVERSION ("16 signed to 64 float",
        in, get_int_caps (1, G_BYTE_ORDER, 16, 16, TRUE, INTERLEAVED),
        out, get_float_caps (1, G_BYTE_ORDER, 64, INTERLEAVED));
  }
  {
    gint32 in[] =
        { 0, (-(G_GINT64_CONSTANT (1) << 31)), (1L << 30), (-(1L << 30)) };
    gdouble out[] = { 0.0,
      (gdouble) (-(G_GINT64_CONSTANT (1) << 31)) / 2147483648.0,        /* ~ -1.0 */
      (gdouble) (1L << 30) / 2147483648.0,      /* ~  0.5 */
      (gdouble) (-(1L << 30)) / 2147483648.0,   /* ~ -0.5 */
    };

    RUN_CONVERSION ("32 signed to 64 float",
        in, get_int_caps (1, G_BYTE_ORDER, 32, 32, TRUE, INTERLEAVED),
        out, get_float_caps (1, G_BYTE_ORDER, 64, INTERLEAVED));
  }
}

GST_END_TEST;

GST_START_TEST (test_multichannel_conversion)
{
  {
    gfloat in[] = { 0.0, 0.0, 0.0, 0.0, 0.0, 0.0 };
    gfloat out[] = { 0.0, 0.0 };

    RUN_CONVERSION ("3 channels to 1", in, get_float_mc_caps (3,
            G_BYTE_ORDER, 32, INTERLEAVED, NULL), out, get_float_caps (1,
            G_BYTE_ORDER, 32, INTERLEAVED));
    RUN_CONVERSION ("1 channels to 3", out, get_float_caps (1, G_BYTE_ORDER,
            32, INTERLEAVED), in, get_float_mc_caps (3, G_BYTE_ORDER, 32,
            INTERLEAVED, NULL));
  }

  {
    gint16 in[] = { 0, 0, 0, 0, 0, 0 };
    gint16 out[] = { 0, 0 };

    RUN_CONVERSION ("3 channels to 1", in, get_int_mc_caps (3,
            G_BYTE_ORDER, 16, 16, TRUE, INTERLEAVED, NULL), out,
        get_int_caps (1, G_BYTE_ORDER, 16, 16, TRUE, INTERLEAVED));
    RUN_CONVERSION ("1 channels to 3", out, get_int_caps (1, G_BYTE_ORDER, 16,
            16, TRUE, INTERLEAVED), in, get_int_mc_caps (3, G_BYTE_ORDER, 16,
            16, TRUE, INTERLEAVED, NULL));
  }

  {
    gint16 in[] = { 1, 2 };
    gint16 out[] = { 1, 1, 2, 2 };
    GstAudioChannelPosition in_layout[1] = { GST_AUDIO_CHANNEL_POSITION_MONO };
    GstAudioChannelPosition out_layout[2] =
        { GST_AUDIO_CHANNEL_POSITION_FRONT_LEFT,
      GST_AUDIO_CHANNEL_POSITION_FRONT_RIGHT
    };
    GstCaps *in_caps =
        get_int_mc_caps (1, G_BYTE_ORDER, 16, 16, TRUE, INTERLEAVED, in_layout);
    GstCaps *out_caps = get_int_mc_caps (2, G_BYTE_ORDER, 16, 16, TRUE,
        INTERLEAVED, out_layout);

    RUN_CONVERSION ("1 channels to 2 with standard layout", in,
        in_caps, out, out_caps);
  }

  {
    gint16 in[] = { 1, 2 };
    gint16 out[] = { 1, 1, 2, 2 };
    GstCaps *in_caps =
        get_int_caps (1, G_BYTE_ORDER, 16, 16, TRUE, INTERLEAVED);
    GstCaps *out_caps =
        get_int_caps (2, G_BYTE_ORDER, 16, 16, TRUE, INTERLEAVED);

    RUN_CONVERSION ("1 channels to 2 with standard layout and no positions set",
        in, gst_caps_copy (in_caps), out, gst_caps_copy (out_caps));

    RUN_CONVERSION ("2 channels to 1 with standard layout and no positions set",
        out, out_caps, in, in_caps);
  }

  {
    gint16 in[] = { 1, 2 };
    gint16 out[] = { 1, 0, 2, 0 };
    GstAudioChannelPosition in_layout[1] =
        { GST_AUDIO_CHANNEL_POSITION_FRONT_LEFT };
    GstAudioChannelPosition out_layout[2] =
        { GST_AUDIO_CHANNEL_POSITION_FRONT_LEFT,
      GST_AUDIO_CHANNEL_POSITION_FRONT_RIGHT
    };
    GstCaps *in_caps =
        get_int_mc_caps (1, G_BYTE_ORDER, 16, 16, TRUE, INTERLEAVED, in_layout);
    GstCaps *out_caps = get_int_mc_caps (2, G_BYTE_ORDER, 16, 16, TRUE,
        INTERLEAVED, out_layout);

    RUN_CONVERSION ("1 channels to 2 with non-standard layout", in,
        in_caps, out, out_caps);
  }

  {
    gint16 in[] = { 1, 2, 3, 4 };
    gint16 out[] = { 2, 4 };
    GstAudioChannelPosition in_layout[2] =
        { GST_AUDIO_CHANNEL_POSITION_FRONT_LEFT,
      GST_AUDIO_CHANNEL_POSITION_FRONT_RIGHT
    };
    GstAudioChannelPosition out_layout[1] =
        { GST_AUDIO_CHANNEL_POSITION_FRONT_CENTER };
    GstCaps *in_caps =
        get_int_mc_caps (2, G_BYTE_ORDER, 16, 16, TRUE, INTERLEAVED, in_layout);
    GstCaps *out_caps = get_int_mc_caps (1, G_BYTE_ORDER, 16, 16, TRUE,
        INTERLEAVED, out_layout);

    RUN_CONVERSION ("2 channels to 1 with non-standard layout", in,
        in_caps, out, out_caps);
  }

  {
    gint16 in[] = { 1, 2, 3, 4 };
    gint16 out[] = { 2, 4 };
    GstAudioChannelPosition in_layout[2] =
        { GST_AUDIO_CHANNEL_POSITION_FRONT_LEFT,
      GST_AUDIO_CHANNEL_POSITION_FRONT_RIGHT
    };
    GstAudioChannelPosition out_layout[1] = { GST_AUDIO_CHANNEL_POSITION_MONO };
    GstCaps *in_caps =
        get_int_mc_caps (2, G_BYTE_ORDER, 16, 16, TRUE, INTERLEAVED, in_layout);
    GstCaps *out_caps = get_int_mc_caps (1, G_BYTE_ORDER, 16, 16, TRUE,
        INTERLEAVED, out_layout);

    RUN_CONVERSION ("2 channels to 1 with standard layout", in,
        in_caps, out, out_caps);
  }

  {
    gint16 in[] = { 1, 2, 3, 4 };
    gint16 out[] = { 1, 3 };
    GstAudioChannelPosition in_layout[2] =
        { GST_AUDIO_CHANNEL_POSITION_FRONT_CENTER,
      GST_AUDIO_CHANNEL_POSITION_REAR_CENTER
    };
    GstAudioChannelPosition out_layout[1] = { GST_AUDIO_CHANNEL_POSITION_MONO };
    GstCaps *in_caps =
        get_int_mc_caps (2, G_BYTE_ORDER, 16, 16, TRUE, INTERLEAVED, in_layout);
    GstCaps *out_caps = get_int_mc_caps (1, G_BYTE_ORDER, 16, 16, TRUE,
        INTERLEAVED, out_layout);

    RUN_CONVERSION ("2 channels to 1 with non-standard layout", in,
        in_caps, out, out_caps);
  }

  {
    gint16 in[] = { 1, 2, 3, 4 };
    gint16 out[] = { 1, 3 };
    GstAudioChannelPosition in_layout[2] =
        { GST_AUDIO_CHANNEL_POSITION_FRONT_CENTER,
      GST_AUDIO_CHANNEL_POSITION_REAR_LEFT
    };
    GstAudioChannelPosition out_layout[1] = { GST_AUDIO_CHANNEL_POSITION_MONO };
    GstCaps *in_caps =
        get_int_mc_caps (2, G_BYTE_ORDER, 16, 16, TRUE, INTERLEAVED, in_layout);
    GstCaps *out_caps = get_int_mc_caps (1, G_BYTE_ORDER, 16, 16, TRUE,
        INTERLEAVED, out_layout);

    RUN_CONVERSION ("2 channels to 1 with non-standard layout", in,
        in_caps, out, out_caps);
  }
  {
    gint16 in[] = { 4, 5, 4, 2, 2, 1 };
    gint16 out[] = { 3, 3 };
    GstCaps *in_caps =
        get_int_mc_caps (6, G_BYTE_ORDER, 16, 16, TRUE, INTERLEAVED, NULL);
    GstCaps *out_caps =
        get_int_caps (2, G_BYTE_ORDER, 16, 16, TRUE, INTERLEAVED);

    RUN_CONVERSION ("5.1 to 2 channels", in, in_caps, out, out_caps);
  }
  {
    gint16 in[] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
    gint16 out[] = { 0, 0 };
    GstAudioChannelPosition in_layout[11] = {
      GST_AUDIO_CHANNEL_POSITION_FRONT_LEFT,
      GST_AUDIO_CHANNEL_POSITION_FRONT_RIGHT,
      GST_AUDIO_CHANNEL_POSITION_FRONT_CENTER,
      GST_AUDIO_CHANNEL_POSITION_LFE1,
      GST_AUDIO_CHANNEL_POSITION_REAR_LEFT,
      GST_AUDIO_CHANNEL_POSITION_REAR_RIGHT,
      GST_AUDIO_CHANNEL_POSITION_FRONT_LEFT_OF_CENTER,
      GST_AUDIO_CHANNEL_POSITION_FRONT_RIGHT_OF_CENTER,
      GST_AUDIO_CHANNEL_POSITION_REAR_CENTER,
      GST_AUDIO_CHANNEL_POSITION_SIDE_LEFT,
      GST_AUDIO_CHANNEL_POSITION_SIDE_RIGHT,
    };
    GstCaps *in_caps = get_int_mc_caps (11, G_BYTE_ORDER, 16, 16, TRUE,
        INTERLEAVED, in_layout);
    GstCaps *out_caps =
        get_int_mc_caps (2, G_BYTE_ORDER, 16, 16, TRUE, INTERLEAVED, NULL);

    RUN_CONVERSION ("11 channels to 2", in, in_caps, out, out_caps);
  }
  {
    gint16 in[] = { 0, 0 };
    gint16 out[] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
    GstAudioChannelPosition out_layout[11] = {
      GST_AUDIO_CHANNEL_POSITION_FRONT_LEFT,
      GST_AUDIO_CHANNEL_POSITION_FRONT_RIGHT,
      GST_AUDIO_CHANNEL_POSITION_FRONT_CENTER,
      GST_AUDIO_CHANNEL_POSITION_LFE1,
      GST_AUDIO_CHANNEL_POSITION_REAR_LEFT,
      GST_AUDIO_CHANNEL_POSITION_REAR_RIGHT,
      GST_AUDIO_CHANNEL_POSITION_FRONT_LEFT_OF_CENTER,
      GST_AUDIO_CHANNEL_POSITION_FRONT_RIGHT_OF_CENTER,
      GST_AUDIO_CHANNEL_POSITION_REAR_CENTER,
      GST_AUDIO_CHANNEL_POSITION_SIDE_LEFT,
      GST_AUDIO_CHANNEL_POSITION_SIDE_RIGHT,
    };
    GstCaps *in_caps =
        get_int_mc_caps (2, G_BYTE_ORDER, 16, 16, TRUE, INTERLEAVED, NULL);
    GstCaps *out_caps = get_int_mc_caps (11, G_BYTE_ORDER, 16, 16, TRUE,
        INTERLEAVED, out_layout);

    RUN_CONVERSION ("2 channels to 11", in, in_caps, out, out_caps);
  }

}

GST_END_TEST;

GST_START_TEST (test_passthrough)
{
  /* int 8 bit */
  {
    gint8 data[] = { 0, 1, 2, 127, -127 };

    RUN_CONVERSION ("int 8 bit passthrough",
        data, get_int_caps (1, G_BYTE_ORDER, 8, 8, TRUE, INTERLEAVED),
        data, get_int_caps (1, G_BYTE_ORDER, 8, 8, TRUE, INTERLEAVED)
        );
    RUN_CONVERSION_NOT_INPLACE ("int 8 bit passthrough",
        data, get_int_caps (1, G_BYTE_ORDER, 8, 8, TRUE, INTERLEAVED),
        data, get_int_caps (1, G_BYTE_ORDER, 8, 8, TRUE, INTERLEAVED)
        );
  }
  /* int 16 bit signed */
  {
    gint16 data[] = { 0, 256, 512, 32512, -32512 };

    RUN_CONVERSION ("int 16 bit signed passthrough",
        data, get_int_caps (1, G_BYTE_ORDER, 16, 16, TRUE, INTERLEAVED),
        data, get_int_caps (1, G_BYTE_ORDER, 16, 16, TRUE, INTERLEAVED)
        );
    RUN_CONVERSION_NOT_INPLACE ("int 16 bit signed passthrough",
        data, get_int_caps (1, G_BYTE_ORDER, 16, 16, TRUE, INTERLEAVED),
        data, get_int_caps (1, G_BYTE_ORDER, 16, 16, TRUE, INTERLEAVED)
        );
  }
  /* int 32 bit signed */
  {
    gint32 data[] = { 0, G_MININT32, G_MAXINT32,
      (32 << 16), (32 << 16) + (1 << 15), (32 << 16) - (1 << 15),
      (32 << 16) + (2 << 15), (32 << 16) - (2 << 15),
      (-(32 << 16)) + (1 << 15), (-(32 << 16)) - (1 << 15),
      (-(32 << 16)) + (2 << 15), (-(32 << 16)) - (2 << 15),
      (-(32 << 16))
    };
    RUN_CONVERSION ("int 32 bit signed passthrough",
        data, get_int_caps (1, G_BYTE_ORDER, 32, 32, TRUE, INTERLEAVED),
        data, get_int_caps (1, G_BYTE_ORDER, 32, 32, TRUE, INTERLEAVED)
        );
    RUN_CONVERSION_NOT_INPLACE ("int 32 bit signed passthrough",
        data, get_int_caps (1, G_BYTE_ORDER, 32, 32, TRUE, INTERLEAVED),
        data, get_int_caps (1, G_BYTE_ORDER, 32, 32, TRUE, INTERLEAVED)
        );
  }

  /* int 16 bit signed stereo */
  {
    gint16 data[] = { 0, 0, 1, 1, 2, 2, 3, 3 };

    RUN_CONVERSION ("int 16 bit signed 2 channel passthrough",
        data, get_int_caps (2, G_BYTE_ORDER, 16, 16, TRUE, INTERLEAVED),
        data, get_int_caps (2, G_BYTE_ORDER, 16, 16, TRUE, INTERLEAVED));
    RUN_CONVERSION_NOT_INPLACE ("int 16 bit signed 2 channel passthrough",
        data, get_int_caps (2, G_BYTE_ORDER, 16, 16, TRUE, INTERLEAVED),
        data, get_int_caps (2, G_BYTE_ORDER, 16, 16, TRUE, INTERLEAVED));
  }
}

GST_END_TEST;

GST_START_TEST (test_caps_negotiation)
{
  GstElement *src, *ac1, *ac2, *ac3, *sink;
  GstElement *pipeline;
  GstPad *ac3_src;
  GstCaps *caps1, *caps2;

  pipeline = gst_pipeline_new ("test");

  /* create elements */
  src = gst_element_factory_make ("audiotestsrc", "src");
  ac1 = gst_element_factory_make ("audioconvert", "ac1");
  ac2 = gst_element_factory_make ("audioconvert", "ac2");
  ac3 = gst_element_factory_make ("audioconvert", "ac3");
  sink = gst_element_factory_make ("fakesink", "sink");
  ac3_src = gst_element_get_static_pad (ac3, "src");

  /* test with 2 audioconvert elements */
  gst_bin_add_many (GST_BIN (pipeline), src, ac1, ac3, sink, NULL);
  gst_element_link_many (src, ac1, ac3, sink, NULL);

  /* Set to PAUSED and wait for PREROLL */
  fail_if (gst_element_set_state (pipeline, GST_STATE_PAUSED) ==
      GST_STATE_CHANGE_FAILURE, "Failed to set test pipeline to PAUSED");
  fail_if (gst_element_get_state (pipeline, NULL, NULL, GST_CLOCK_TIME_NONE) !=
      GST_STATE_CHANGE_SUCCESS, "Failed to set test pipeline to PAUSED");

  caps1 = gst_pad_query_caps (ac3_src, NULL);
  fail_if (caps1 == NULL, "gst_pad_query_caps returned NULL");
  GST_DEBUG ("Caps size 1 : %d", gst_caps_get_size (caps1));

  fail_if (gst_element_set_state (pipeline, GST_STATE_READY) ==
      GST_STATE_CHANGE_FAILURE, "Failed to set test pipeline back to READY");
  fail_if (gst_element_get_state (pipeline, NULL, NULL, GST_CLOCK_TIME_NONE) !=
      GST_STATE_CHANGE_SUCCESS, "Failed to set test pipeline back to READY");

  /* test with 3 audioconvert elements */
  gst_element_unlink (ac1, ac3);
  gst_bin_add (GST_BIN (pipeline), ac2);
  gst_element_link_many (ac1, ac2, ac3, NULL);

  fail_if (gst_element_set_state (pipeline, GST_STATE_PAUSED) ==
      GST_STATE_CHANGE_FAILURE, "Failed to set test pipeline back to PAUSED");
  fail_if (gst_element_get_state (pipeline, NULL, NULL, GST_CLOCK_TIME_NONE) !=
      GST_STATE_CHANGE_SUCCESS, "Failed to set test pipeline back to PAUSED");

  caps2 = gst_pad_query_caps (ac3_src, NULL);

  fail_if (caps2 == NULL, "gst_pad_query_caps returned NULL");
  GST_DEBUG ("Caps size 2 : %d", gst_caps_get_size (caps2));
  fail_unless (gst_caps_get_size (caps1) == gst_caps_get_size (caps2));

  gst_caps_unref (caps1);
  gst_caps_unref (caps2);

  fail_if (gst_element_set_state (pipeline, GST_STATE_NULL) ==
      GST_STATE_CHANGE_FAILURE, "Failed to set test pipeline back to NULL");
  fail_if (gst_element_get_state (pipeline, NULL, NULL, GST_CLOCK_TIME_NONE) !=
      GST_STATE_CHANGE_SUCCESS, "Failed to set test pipeline back to NULL");

  gst_object_unref (ac3_src);
  gst_object_unref (pipeline);
}

GST_END_TEST;

GST_START_TEST (test_convert_undefined_multichannel)
{
  /* (A) CONVERSION FROM 'WORSE' TO 'BETTER' FORMAT */

  /* 1 channel, NONE positions, int8 => int16 */
  {
    guint16 out[] = { 0x2000 };
    guint8 in[] = { 0x20 };
    GstCaps *out_caps = get_int_mc_caps (1, G_BYTE_ORDER, 16, 16, FALSE,
        INTERLEAVED, undefined_positions[1 - 1]);
    GstCaps *in_caps = get_int_mc_caps (1, G_BYTE_ORDER, 8, 8, FALSE,
        INTERLEAVED, undefined_positions[1 - 1]);

    RUN_CONVERSION ("1 channel, undefined layout, identity conversion, "
        "int8 => int16", in, in_caps, out, out_caps);
  }

  /* 2 channels, NONE positions, int8 => int16 */
  {
    guint16 out[] = { 0x8000, 0x2000 };
    guint8 in[] = { 0x80, 0x20 };
    GstCaps *out_caps = get_int_mc_caps (2, G_BYTE_ORDER, 16, 16, FALSE,
        INTERLEAVED, undefined_positions[2 - 1]);
    GstCaps *in_caps = get_int_mc_caps (2, G_BYTE_ORDER, 8, 8, FALSE,
        INTERLEAVED, undefined_positions[2 - 1]);

    RUN_CONVERSION ("2 channels, undefined layout, identity conversion, "
        "int8 => int16", in, in_caps, out, out_caps);
  }

  /* 6 channels, NONE positions, int8 => int16 */
  {
    guint16 out[] = { 0x0000, 0x2000, 0x8000, 0x2000, 0x0000, 0xff00 };
    guint8 in[] = { 0x00, 0x20, 0x80, 0x20, 0x00, 0xff };
    GstCaps *out_caps = get_int_mc_caps (6, G_BYTE_ORDER, 16, 16, FALSE,
        INTERLEAVED, undefined_positions[6 - 1]);
    GstCaps *in_caps = get_int_mc_caps (6, G_BYTE_ORDER, 8, 8, FALSE,
        INTERLEAVED, undefined_positions[6 - 1]);

    RUN_CONVERSION ("6 channels, undefined layout, identity conversion, "
        "int8 => int16", in, in_caps, out, out_caps);
  }

  /* 9 channels, NONE positions, int8 => int16 */
  {
    guint16 out[] = { 0x0000, 0xff00, 0x0000, 0x2000, 0x8000, 0x2000,
      0x0000, 0xff00, 0x0000
    };
    guint8 in[] = { 0x00, 0xff, 0x00, 0x20, 0x80, 0x20, 0x00, 0xff, 0x00 };
    GstCaps *out_caps = get_int_mc_caps (9, G_BYTE_ORDER, 16, 16, FALSE,
        INTERLEAVED, undefined_positions[9 - 1]);
    GstCaps *in_caps = get_int_mc_caps (9, G_BYTE_ORDER, 8, 8, FALSE,
        INTERLEAVED, undefined_positions[9 - 1]);

    RUN_CONVERSION ("9 channels, undefined layout, identity conversion, "
        "int8 => int16", in, in_caps, out, out_caps);
  }

  /* 15 channels, NONE positions, int8 => int16 */
  {
    guint16 out[] =
        { 0x0000, 0xff00, 0x0000, 0x2000, 0x8000, 0x2000, 0x0000, 0xff00,
      0x0000, 0xff00, 0x0000, 0x2000, 0x8000, 0x2000, 0x0000
    };
    guint8 in[] =
        { 0x00, 0xff, 0x00, 0x20, 0x80, 0x20, 0x00, 0xff, 0x00, 0xff, 0x00,
      0x20, 0x80, 0x20, 0x00
    };
    GstCaps *out_caps = get_int_mc_caps (15, G_BYTE_ORDER, 16, 16, FALSE,
        INTERLEAVED, undefined_positions[15 - 1]);
    GstCaps *in_caps = get_int_mc_caps (15, G_BYTE_ORDER, 8, 8, FALSE,
        INTERLEAVED, undefined_positions[15 - 1]);

    RUN_CONVERSION ("15 channels, undefined layout, identity conversion, "
        "int8 => int16", in, in_caps, out, out_caps);
  }

  /* (B) CONVERSION FROM 'BETTER' TO 'WORSE' FORMAT */

  /* 1 channel, NONE positions, int16 => int8 */
  {
    guint16 in[] = { 0x2000 };
    guint8 out[] = { 0x20 };
    GstCaps *in_caps = get_int_mc_caps (1, G_BYTE_ORDER, 16, 16, FALSE,
        INTERLEAVED, undefined_positions[1 - 1]);
    GstCaps *out_caps = get_int_mc_caps (1, G_BYTE_ORDER, 8, 8, FALSE,
        INTERLEAVED, undefined_positions[1 - 1]);

    RUN_CONVERSION ("1 channel, undefined layout, identity conversion, "
        "int16 => int8", in, in_caps, out, out_caps);
  }

  /* 2 channels, NONE positions, int16 => int8 */
  {
    guint16 in[] = { 0x8000, 0x2000 };
    guint8 out[] = { 0x80, 0x20 };
    GstCaps *in_caps = get_int_mc_caps (2, G_BYTE_ORDER, 16, 16, FALSE,
        INTERLEAVED, undefined_positions[2 - 1]);
    GstCaps *out_caps = get_int_mc_caps (2, G_BYTE_ORDER, 8, 8, FALSE,
        INTERLEAVED, undefined_positions[2 - 1]);

    RUN_CONVERSION ("2 channels, undefined layout, identity conversion, "
        "int16 => int8", in, in_caps, out, out_caps);
  }

  /* 6 channels, NONE positions, int16 => int8 */
  {
    guint16 in[] = { 0x0000, 0x2000, 0x8000, 0x2000, 0x0000, 0xff00 };
    guint8 out[] = { 0x00, 0x20, 0x80, 0x20, 0x00, 0xff };
    GstCaps *in_caps = get_int_mc_caps (6, G_BYTE_ORDER, 16, 16, FALSE,
        INTERLEAVED, undefined_positions[6 - 1]);
    GstCaps *out_caps = get_int_mc_caps (6, G_BYTE_ORDER, 8, 8, FALSE,
        INTERLEAVED, undefined_positions[6 - 1]);

    RUN_CONVERSION ("6 channels, undefined layout, identity conversion, "
        "int16 => int8", in, in_caps, out, out_caps);
  }

  /* 9 channels, NONE positions, int16 => int8 */
  {
    guint16 in[] = { 0x0000, 0xff00, 0x0000, 0x2000, 0x8000, 0x2000,
      0x0000, 0xff00, 0x0000
    };
    guint8 out[] = { 0x00, 0xff, 0x00, 0x20, 0x80, 0x20, 0x00, 0xff, 0x00 };
    GstCaps *in_caps = get_int_mc_caps (9, G_BYTE_ORDER, 16, 16, FALSE,
        INTERLEAVED, undefined_positions[9 - 1]);
    GstCaps *out_caps = get_int_mc_caps (9, G_BYTE_ORDER, 8, 8, FALSE,
        INTERLEAVED, undefined_positions[9 - 1]);

    RUN_CONVERSION ("9 channels, undefined layout, identity conversion, "
        "int16 => int8", in, in_caps, out, out_caps);
  }

  /* 15 channels, NONE positions, int16 => int8 */
  {
    guint16 in[] = { 0x0000, 0xff00, 0x0000, 0x2000, 0x8000, 0x2000,
      0x0000, 0xff00, 0x0000, 0xff00, 0x0000, 0x2000, 0x8000, 0x2000,
      0x0000
    };
    guint8 out[] =
        { 0x00, 0xff, 0x00, 0x20, 0x80, 0x20, 0x00, 0xff, 0x00, 0xff, 0x00,
      0x20, 0x80, 0x20, 0x00
    };
    GstCaps *in_caps = get_int_mc_caps (15, G_BYTE_ORDER, 16, 16, FALSE,
        INTERLEAVED, undefined_positions[15 - 1]);
    GstCaps *out_caps = get_int_mc_caps (15, G_BYTE_ORDER, 8, 8, FALSE,
        INTERLEAVED, undefined_positions[15 - 1]);

    RUN_CONVERSION ("15 channels, undefined layout, identity conversion, "
        "int16 => int8", in, in_caps, out, out_caps);
  }


  /* (C) NO CONVERSION, SAME FORMAT */

  /* 1 channel, NONE positions, int16 => int16 */
  {
    guint16 in[] = { 0x2000 };
    guint16 out[] = { 0x2000 };
    GstCaps *in_caps = get_int_mc_caps (1, G_BYTE_ORDER, 16, 16, FALSE,
        INTERLEAVED, undefined_positions[1 - 1]);
    GstCaps *out_caps = get_int_mc_caps (1, G_BYTE_ORDER, 16, 16, FALSE,
        INTERLEAVED, undefined_positions[1 - 1]);

    RUN_CONVERSION ("1 channel, undefined layout, identity conversion, "
        "int16 => int16", in, in_caps, out, out_caps);
  }

  /* 2 channels, NONE positions, int16 => int16 */
  {
    guint16 in[] = { 0x8000, 0x2000 };
    guint16 out[] = { 0x8000, 0x2000 };
    GstCaps *in_caps = get_int_mc_caps (2, G_BYTE_ORDER, 16, 16, FALSE,
        INTERLEAVED, undefined_positions[2 - 1]);
    GstCaps *out_caps = get_int_mc_caps (2, G_BYTE_ORDER, 16, 16, FALSE,
        INTERLEAVED, undefined_positions[2 - 1]);

    RUN_CONVERSION ("2 channels, undefined layout, identity conversion, "
        "int16 => int16", in, in_caps, out, out_caps);
  }

  /* 6 channels, NONE positions, int16 => int16 */
  {
    guint16 in[] = { 0x0000, 0x2000, 0x8000, 0x2000, 0x0000, 0xff00 };
    guint16 out[] = { 0x0000, 0x2000, 0x8000, 0x2000, 0x0000, 0xff00 };
    GstCaps *in_caps = get_int_mc_caps (6, G_BYTE_ORDER, 16, 16, FALSE,
        INTERLEAVED, undefined_positions[6 - 1]);
    GstCaps *out_caps = get_int_mc_caps (6, G_BYTE_ORDER, 16, 16, FALSE,
        INTERLEAVED, undefined_positions[6 - 1]);

    RUN_CONVERSION ("6 channels, undefined layout, identity conversion, "
        "int16 => int16", in, in_caps, out, out_caps);
  }

  /* 9 channels, NONE positions, int16 => int16 */
  {
    guint16 in[] = { 0x0000, 0xff00, 0x0000, 0x2000, 0x8000, 0x2000,
      0x0000, 0xff00, 0x0000
    };
    guint16 out[] = { 0x0000, 0xff00, 0x0000, 0x2000, 0x8000, 0x2000,
      0x0000, 0xff00, 0x0000
    };
    GstCaps *in_caps = get_int_mc_caps (9, G_BYTE_ORDER, 16, 16, FALSE,
        INTERLEAVED, undefined_positions[9 - 1]);
    GstCaps *out_caps = get_int_mc_caps (9, G_BYTE_ORDER, 16, 16, FALSE,
        INTERLEAVED, undefined_positions[9 - 1]);

    RUN_CONVERSION ("9 channels, undefined layout, identity conversion, "
        "int16 => int16", in, in_caps, out, out_caps);
  }

  /* 15 channels, NONE positions, int16 => int16 */
  {
    guint16 in[] = { 0x0000, 0xff00, 0x0000, 0x2000, 0x8000, 0x2000,
      0x0000, 0xff00, 0x0000, 0xff00, 0x0000, 0x2000, 0x8000, 0x2000,
      0x0000
    };
    guint16 out[] = { 0x0000, 0xff00, 0x0000, 0x2000, 0x8000, 0x2000,
      0x0000, 0xff00, 0x0000, 0xff00, 0x0000, 0x2000, 0x8000, 0x2000,
      0x0000
    };
    GstCaps *in_caps = get_int_mc_caps (15, G_BYTE_ORDER, 16, 16, FALSE,
        INTERLEAVED, undefined_positions[15 - 1]);
    GstCaps *out_caps = get_int_mc_caps (15, G_BYTE_ORDER, 16, 16, FALSE,
        INTERLEAVED, undefined_positions[15 - 1]);

    RUN_CONVERSION ("15 channels, undefined layout, identity conversion, "
        "int16 => int16", in, in_caps, out, out_caps);
  }


  /* (C) int16 => float */

  /* 9 channels, NONE positions, int16 => float */
  {
    guint16 in[] = { 0x0000, 0x8000, 0x0000, 0x8000, 0x8000, 0x8000,
      0x0000, 0x8000, 0x0000
    };
    gfloat out[] = { -1.0, 0.0, -1.0, 0.0, 0.0, 0.0, -1.0, 0.0, -1.0 };
    GstCaps *in_caps = get_int_mc_caps (9, G_BYTE_ORDER, 16, 16, FALSE,
        INTERLEAVED, undefined_positions[9 - 1]);
    GstCaps *out_caps = get_float_mc_caps (9, G_BYTE_ORDER, 32, INTERLEAVED,
        undefined_positions[9 - 1]);

    RUN_CONVERSION ("9 channels, undefined layout, identity conversion, "
        "int16 => float", in, in_caps, out, out_caps);
  }

  /* 15 channels, NONE positions, int16 => float */
  {
    guint16 in[] = { 0x0000, 0x8000, 0x0000, 0x8000, 0x8000, 0x8000,
      0x0000, 0x8000, 0x0000, 0x8000, 0x0000, 0x8000, 0x8000, 0x8000,
      0x0000
    };
    gfloat out[] =
        { -1.0, 0.0, -1.0, 0.0, 0.0, 0.0, -1.0, 0.0, -1.0, 0.0, -1.0, 0.0, 0.0,
      0.0, -1.0
    };
    GstCaps *in_caps = get_int_mc_caps (15, G_BYTE_ORDER, 16, 16, FALSE,
        INTERLEAVED, undefined_positions[15 - 1]);
    GstCaps *out_caps = get_float_mc_caps (15, G_BYTE_ORDER, 32, INTERLEAVED,
        undefined_positions[15 - 1]);

    RUN_CONVERSION ("15 channels, undefined layout, identity conversion, "
        "int16 => float", in, in_caps, out, out_caps);
  }


  /* 9 channels, NONE positions, int16 => float (same as above, but no
   * position on output caps to see if audioconvert transforms correctly) */
  {
    guint16 in[] = { 0x0000, 0x8000, 0x0000, 0x8000, 0x8000, 0x8000,
      0x0000, 0x8000, 0x0000
    };
    gfloat out[] = { -1.0, 0.0, -1.0, 0.0, 0.0, 0.0, -1.0, 0.0, -1.0 };
    GstCaps *in_caps = get_int_mc_caps (9, G_BYTE_ORDER, 16, 16, FALSE,
        INTERLEAVED, undefined_positions[9 - 1]);
    GstCaps *out_caps = get_float_mc_caps (9, G_BYTE_ORDER, 32, INTERLEAVED,
        undefined_positions[9 - 1]);

    gst_structure_remove_field (gst_caps_get_structure (out_caps, 0),
        "channel-mask");

    RUN_CONVERSION ("9 channels, undefined layout, identity conversion, "
        "int16 => float", in, in_caps, out, out_caps);
  }

  /* 15 channels, NONE positions, int16 => float (same as above, but no
   * position on output caps to see if audioconvert transforms correctly) */
  {
    guint16 in[] = { 0x0000, 0x8000, 0x0000, 0x8000, 0x8000, 0x8000,
      0x0000, 0x8000, 0x0000, 0x8000, 0x0000, 0x8000, 0x8000, 0x8000,
      0x0000
    };
    gfloat out[] =
        { -1.0, 0.0, -1.0, 0.0, 0.0, 0.0, -1.0, 0.0, -1.0, 0.0, -1.0, 0.0, 0.0,
      0.0, -1.0
    };
    GstCaps *in_caps = get_int_mc_caps (15, G_BYTE_ORDER, 16, 16, FALSE,
        INTERLEAVED, undefined_positions[15 - 1]);
    GstCaps *out_caps = get_float_mc_caps (15, G_BYTE_ORDER, 32, INTERLEAVED,
        undefined_positions[15 - 1]);

    gst_structure_remove_field (gst_caps_get_structure (out_caps, 0),
        "channel-mask");

    RUN_CONVERSION ("15 channels, undefined layout, identity conversion, "
        "int16 => float", in, in_caps, out, out_caps);
  }

  /* 8 channels, NONE positions => 2 channels: should fail, no mixing allowed */
  {
    guint16 in[] = { 0, 0, 0, 0, 0, 0, 0, 0 };
    gfloat out[] = { -1.0, -1.0 };
    GstCaps *in_caps = get_int_mc_caps (8, G_BYTE_ORDER, 16, 16, FALSE,
        INTERLEAVED, undefined_positions[8 - 1]);
    GstCaps *out_caps = get_float_mc_caps (2, G_BYTE_ORDER, 32, INTERLEAVED,
        NULL);

    RUN_CONVERSION_TO_FAIL ("8 channels with layout => 2 channels",
        in, in_caps, out, out_caps);
  }

  /* 8 channels, with positions => 2 channels (makes sure channel-position
   * fields are removed properly in some cases in ::transform_caps, so we
   * don't up with caps with 2 channels and 8 channel positions) */
  {
    GstAudioChannelPosition layout8ch[] = {
      GST_AUDIO_CHANNEL_POSITION_FRONT_LEFT,
      GST_AUDIO_CHANNEL_POSITION_FRONT_RIGHT,
      GST_AUDIO_CHANNEL_POSITION_FRONT_CENTER,
      GST_AUDIO_CHANNEL_POSITION_LFE1,
      GST_AUDIO_CHANNEL_POSITION_REAR_LEFT,
      GST_AUDIO_CHANNEL_POSITION_REAR_RIGHT,
      GST_AUDIO_CHANNEL_POSITION_SIDE_LEFT,
      GST_AUDIO_CHANNEL_POSITION_SIDE_RIGHT
    };
    gint16 in[] = { 0, 0, 0, 0, 0, 0, 0, 0 };
    gint16 out[] = { 0, 0 };
    GstCaps *in_caps =
        get_int_mc_caps (8, G_BYTE_ORDER, 16, 16, TRUE, INTERLEAVED, layout8ch);
    GstCaps *out_caps = get_int_mc_caps (2, G_BYTE_ORDER, 16, 16, TRUE,
        INTERLEAVED, NULL);

    RUN_CONVERSION ("8 channels with layout => 2 channels",
        in, in_caps, out, out_caps);
  }

  /* 9 channels, NONE positions => 2 channels, with empty mix-matrix */
  {
    guint16 in[] =
        { 0, 0, 0x8000, 0x8000, 0x8000, 0x8000, 0x8000, 0x8000, 0x8000 };
    gfloat out[] = { -1.0, -1.0 };
    GstCaps *in_caps = get_int_mc_caps (9, G_BYTE_ORDER, 16, 16, FALSE,
        INTERLEAVED, undefined_positions[9 - 1]);
    GstCaps *out_caps = get_float_mc_caps (2, G_BYTE_ORDER, 32, INTERLEAVED,
        NULL);
    GValue empty_mix_matrix = G_VALUE_INIT;
    g_value_init (&empty_mix_matrix, GST_TYPE_ARRAY);

    RUN_CONVERSION_WITH_MATRIX ("9 channels, undefined layout => 2 channels",
        in, in_caps, out, out_caps, &empty_mix_matrix);
    g_value_unset (&empty_mix_matrix);
  }

  /* 9 channels, NONE positions => 2 channels, with specified mix-matrix */
  {
    guint16 in[] =
        { 0, 0, 0x8000, 0x8000, 0x8000, 0x8000, 0x8000, 0x8000, 0x8000 };
    gfloat out[] = { -1.0, -1.0 };
    GstCaps *in_caps = get_int_mc_caps (9, G_BYTE_ORDER, 16, 16, FALSE,
        INTERLEAVED, undefined_positions[9 - 1]);
    GstCaps *out_caps = get_float_mc_caps (2, G_BYTE_ORDER, 32, INTERLEAVED,
        NULL);
    GValue mix_matrix = G_VALUE_INIT;
    GValue row = G_VALUE_INIT;
    GValue value = G_VALUE_INIT;
    g_value_init (&mix_matrix, GST_TYPE_ARRAY);
    for (int j = 0; j < 2; j++) {
      g_value_init (&row, GST_TYPE_ARRAY);
      for (int i = 0; i < 9; i++) {
        g_value_init (&value, G_TYPE_FLOAT);
        g_value_set_float (&value, i == j && i < 2 ? 1 : 0);
        gst_value_array_append_value (&row, &value);
        g_value_unset (&value);
      }
      gst_value_array_append_value (&mix_matrix, &row);
      g_value_unset (&row);
    }

    RUN_CONVERSION_WITH_MATRIX ("9 channels, undefined layout => 2 channels",
        in, in_caps, out, out_caps, &mix_matrix);
    g_value_unset (&mix_matrix);
  }
}

GST_END_TEST;

#define SIMPLE_CAPS_TEMPLATE_STRING    \
    "audio/x-raw, " \
    "format = (string) {S8, S16LE, S24LE, S32LE}, " \
    "rate = (int) [ 1, MAX ], " \
    "channels = (int) [ 1, MAX ]"

GST_START_TEST (test_preserve_width)
{
  static GstStaticPadTemplate sinktemplate = GST_STATIC_PAD_TEMPLATE ("sink",
      GST_PAD_SINK,
      GST_PAD_ALWAYS,
      GST_STATIC_CAPS (SIMPLE_CAPS_TEMPLATE_STRING)
      );
  static GstStaticPadTemplate srctemplate = GST_STATIC_PAD_TEMPLATE ("src",
      GST_PAD_SRC,
      GST_PAD_ALWAYS,
      GST_STATIC_CAPS (CONVERT_CAPS_TEMPLATE_STRING)
      );
  static const struct _test_formats
  {
    int width;
    const gchar *outf;
  } test_formats[] = { {
      8, "S8"}, {
      16, "S16LE"}, {
      24, "S24LE"}, {
      32, "S32LE"}, {
      0, NULL}
  };

  gint i;
  GstStructure *structure;
  GstElement *audioconvert;
  GstCaps *incaps, *convert_outcaps;

  audioconvert = gst_check_setup_element ("audioconvert");
  mysrcpad = gst_check_setup_src_pad (audioconvert, &srctemplate);
  mysinkpad = gst_check_setup_sink_pad (audioconvert, &sinktemplate);

  gst_pad_set_active (mysrcpad, TRUE);
  gst_pad_set_active (mysinkpad, TRUE);

  fail_unless (gst_element_set_state (audioconvert,
          GST_STATE_PLAYING) == GST_STATE_CHANGE_SUCCESS,
      "could not set to playing");

  for (i = 0; test_formats[i].width; i++) {
    gint width = test_formats[i].width;
    incaps = get_int_caps (1, G_BIG_ENDIAN, width, width, TRUE, INTERLEAVED);
    gst_pad_set_caps (mysrcpad, incaps);

    convert_outcaps = gst_pad_get_current_caps (mysinkpad);
    structure = gst_caps_get_structure (convert_outcaps, 0);
    fail_unless_equals_string (gst_structure_get_string (structure, "format"),
        test_formats[i].outf);

    gst_caps_unref (convert_outcaps);
    gst_caps_unref (incaps);
  }

  cleanup_audioconvert (audioconvert);
}

GST_END_TEST;

GST_START_TEST (test_gap_buffers)
{
  GstBuffer *inbuffer, *outbuffer;
  GstElement *audioconvert;
  GstCaps *caps = get_int_caps (1, G_BYTE_ORDER, 16, 16, TRUE, INTERLEAVED);
  gint16 data[] = { 0, 0, 0, 0 };
  gsize data_len = sizeof (data);
  gint i;

  audioconvert = setup_audioconvert (caps, FALSE, &(GValue) G_VALUE_INIT);

  fail_unless (gst_element_set_state (audioconvert,
          GST_STATE_PLAYING) == GST_STATE_CHANGE_SUCCESS,
      "could not set to playing");

  gst_check_setup_events (mysrcpad, audioconvert, caps, GST_FORMAT_TIME);

  inbuffer = gst_buffer_new_and_alloc (data_len);
  gst_buffer_fill (inbuffer, 0, data, data_len);
  GST_BUFFER_FLAG_SET (inbuffer, GST_BUFFER_FLAG_GAP);

  for (i = 0; i < 2; i++) {
    gst_base_transform_set_passthrough (GST_BASE_TRANSFORM (audioconvert),
        (i == 0));

    gst_pad_push (mysrcpad, inbuffer);

    fail_unless (g_list_length (buffers) == 1);
    fail_if ((outbuffer = (GstBuffer *) buffers->data) == NULL);

    fail_unless (GST_BUFFER_FLAG_IS_SET (outbuffer, GST_BUFFER_FLAG_GAP));

    buffers = g_list_remove (buffers, outbuffer);
  }

  GST_DEBUG ("cleanup, unref buffers");
  gst_buffer_unref (outbuffer);
  cleanup_audioconvert (audioconvert);
  GST_DEBUG ("cleanup, unref caps");
  gst_caps_unref (caps);
}

GST_END_TEST;

GST_START_TEST (test_layout_conversion)
{
  /* just layout conversion */
  {
    gint16 in[] = { 123, 123, 1024, 1024 };
    gint16 out[] = { 123, 1024, 123, 1024 };

    RUN_CONVERSION ("int16 interleaved -> planar",
        in, get_int_caps (2, G_BYTE_ORDER, 16, 16, TRUE, INTERLEAVED),
        out, get_int_caps (2, G_BYTE_ORDER, 16, 16, TRUE, PLANAR));
    RUN_CONVERSION ("int16 interleaved -> planar",
        in, get_int_caps (2, G_BYTE_ORDER, 16, 16, TRUE, PLANAR),
        out, get_int_caps (2, G_BYTE_ORDER, 16, 16, TRUE, INTERLEAVED));
  }
  /* int16 stereo to mono with layout conversion */
  {
    gint16 in_i[] = { 16384, -256, 1024, 1024 };
    gint16 in_p[] = { 16384, 1024, -256, 1024 };
    gint16 out[] = { 8064, 1024 };

    RUN_CONVERSION ("int16 stereo to mono / interleaved -> planar",
        in_i, get_int_caps (2, G_BYTE_ORDER, 16, 16, TRUE, INTERLEAVED),
        out, get_int_caps (1, G_BYTE_ORDER, 16, 16, TRUE, PLANAR));
    RUN_CONVERSION ("int16 stereo to mono / planar -> interleaved",
        in_p, get_int_caps (2, G_BYTE_ORDER, 16, 16, TRUE, PLANAR),
        out, get_int_caps (1, G_BYTE_ORDER, 16, 16, TRUE, INTERLEAVED));
  }
  /* int16 mono to stereo with layout conversion */
  {
    gint16 in[] = { 512, 1024 };
    gint16 out_i[] = { 512, 512, 1024, 1024 };
    gint16 out_p[] = { 512, 1024, 512, 1024 };

    RUN_CONVERSION ("int16 mono to stereo / planar -> interleaved",
        in, get_int_caps (1, G_BYTE_ORDER, 16, 16, TRUE, PLANAR),
        out_i, get_int_caps (2, G_BYTE_ORDER, 16, 16, TRUE, INTERLEAVED));
    RUN_CONVERSION ("int16 mono to stereo / interleaved -> planar",
        in, get_int_caps (1, G_BYTE_ORDER, 16, 16, TRUE, INTERLEAVED),
        out_p, get_int_caps (2, G_BYTE_ORDER, 16, 16, TRUE, PLANAR));
  }
  /* change format with layout conversion */
  {
    gint16 in_p[] = { 0, 32767, -32767, -32768 };
    gint16 in_i[] = { 0, -32767, 32767, -32768 };
    guint16 out_p[] = { 32768, 65535, 1, 0 };
    guint16 out_i[] = { 32768, 1, 65535, 0 };

    RUN_CONVERSION ("int16 signed -> unsigned / planar -> interleaved",
        in_p, get_int_caps (2, G_BYTE_ORDER, 16, 16, TRUE, PLANAR),
        out_i, get_int_caps (2, G_BYTE_ORDER, 16, 16, FALSE, INTERLEAVED));
    RUN_CONVERSION ("int16 signed -> unsigned / interleaved -> planar",
        in_i, get_int_caps (2, G_BYTE_ORDER, 16, 16, TRUE, INTERLEAVED),
        out_p, get_int_caps (2, G_BYTE_ORDER, 16, 16, FALSE, PLANAR));

    RUN_CONVERSION ("int16 unsigned -> signed / planar -> interleaved",
        out_p, get_int_caps (2, G_BYTE_ORDER, 16, 16, FALSE, PLANAR),
        in_i, get_int_caps (2, G_BYTE_ORDER, 16, 16, TRUE, INTERLEAVED));
    RUN_CONVERSION ("int16 unsigned -> signed / interleaved -> planar",
        out_i, get_int_caps (2, G_BYTE_ORDER, 16, 16, FALSE, INTERLEAVED),
        in_p, get_int_caps (2, G_BYTE_ORDER, 16, 16, TRUE, PLANAR));
  }
  /* channel mixing with layout conversion */
  {
    gint16 in_i[] = { 4, 5, 4, 2, 2, 1, 8, 10, 8, 4, 4, 2 };
    gint16 in_p[] = { 4, 8, 5, 10, 4, 8, 2, 4, 2, 4, 1, 2 };
    gint16 out_i[] = { 3, 3, 6, 6 };
    gint16 out_p[] = { 3, 6, 3, 6 };

    RUN_CONVERSION ("5.1 to 2 channels / interleaved -> planar", in_i,
        get_int_mc_caps (6, G_BYTE_ORDER, 16, 16, TRUE, INTERLEAVED, NULL),
        out_p, get_int_caps (2, G_BYTE_ORDER, 16, 16, TRUE, PLANAR));

    RUN_CONVERSION ("5.1 to 2 channels / planar -> interleaved", in_p,
        get_int_mc_caps (6, G_BYTE_ORDER, 16, 16, TRUE, PLANAR, NULL),
        out_i, get_int_caps (2, G_BYTE_ORDER, 16, 16, TRUE, INTERLEAVED));
  }
  /* change format + channels + layout */
  {
    guint8 in_i[] = {
      0x00, 0x04, 0x00, 0x00, 0x05, 0x00, 0x00, 0x04, 0x00,
      0x00, 0x02, 0x00, 0x00, 0x02, 0x00, 0x00, 0x01, 0x00,
      0x00, 0x08, 0x00, 0x00, 0x0a, 0x00, 0x00, 0x08, 0x00,
      0x00, 0x04, 0x00, 0x00, 0x04, 0x00, 0x00, 0x02, 0x00
    };
    guint8 in_p[] = {
      0x00, 0x04, 0x00, 0x00, 0x08, 0x00, 0x00, 0x05, 0x00,
      0x00, 0x0a, 0x00, 0x00, 0x04, 0x00, 0x00, 0x08, 0x00,
      0x00, 0x02, 0x00, 0x00, 0x04, 0x00, 0x00, 0x02, 0x00,
      0x00, 0x04, 0x00, 0x00, 0x01, 0x00, 0x00, 0x02, 0x00
    };
    gint16 out_i[] = { 3, 3, 6, 6 };
    gint16 out_p[] = { 3, 6, 3, 6 };

    RUN_CONVERSION ("5.1 to 2 channels / S24LE interleaved -> S16 planar", in_i,
        get_int_mc_caps (6, G_LITTLE_ENDIAN, 24, 24, TRUE, INTERLEAVED, NULL),
        out_p, get_int_caps (2, G_BYTE_ORDER, 16, 16, TRUE, PLANAR));
    RUN_CONVERSION ("5.1 to 2 channels / S24LE planar -> S16 interleaved", in_p,
        get_int_mc_caps (6, G_LITTLE_ENDIAN, 24, 24, TRUE, PLANAR, NULL),
        out_i, get_int_caps (2, G_BYTE_ORDER, 16, 16, TRUE, INTERLEAVED));
  }
}

GST_END_TEST;

GST_START_TEST (test_layout_conv_fixate_caps)
{
  GstCaps *incaps, *outcaps;
  GstElement *audioconvert;
  GstCaps *caps;
  GstStructure *s;

  incaps = gst_caps_from_string ("audio/x-raw, "
      "format = (string) F32LE, "
      "layout = (string) interleaved, "
      "rate = (int) 44100, " "channels = (int) 1");
  outcaps = gst_caps_from_string ("audio/x-raw, "
      "format = (string) S16LE, "
      "layout = (string) interleaved, "
      "rate = (int) [ 1, MAX ], "
      "channels = (int) 2;"
      "audio/x-raw, "
      "format = (string) { F32LE, F32BE, F64LE, F64BE }, "
      "layout = (string) non-interleaved, "
      "rate = (int) [ 1, MAX ], " "channels = (int) [1, 8]");

  audioconvert = setup_audioconvert (outcaps, FALSE, &(GValue) G_VALUE_INIT);

  fail_unless (gst_element_set_state (audioconvert,
          GST_STATE_PLAYING) == GST_STATE_CHANGE_SUCCESS,
      "could not set to playing");

  gst_check_setup_events (mysrcpad, audioconvert, incaps, GST_FORMAT_TIME);

  caps = gst_pad_get_current_caps (mysinkpad);
  s = gst_caps_get_structure (caps, 0);
  assert_equals_string (gst_structure_get_string (s, "format"), "F32LE");
  assert_equals_string (gst_structure_get_string (s, "layout"),
      "non-interleaved");
  gst_clear_caps (&caps);

  fail_unless (gst_element_set_state (audioconvert,
          GST_STATE_NULL) == GST_STATE_CHANGE_SUCCESS, "could not set to null");
  /* cleanup */
  GST_DEBUG ("cleanup audioconvert");
  cleanup_audioconvert (audioconvert);
  GST_DEBUG ("cleanup, unref incaps");
  gst_caps_unref (incaps);
  gst_caps_unref (outcaps);
}

GST_END_TEST;

GST_START_TEST (test_96_channels_conversion)
{
  GstCaps *incaps, *outcaps;
  GstElement *audioconvert;
  GstCaps *caps;
  GstStructure *s;

  incaps = gst_caps_from_string ("audio/x-raw, "
      "format = (string) F32LE, "
      "layout = (string) interleaved, "
      "rate = (int) 44100, "
      "channels = (int) 96, " "channel-mask = (bitmask) 0");
  outcaps = gst_caps_from_string ("audio/x-raw, "
      "format = (string) F64LE, " "layout = (string) non-interleaved");

  audioconvert = setup_audioconvert (outcaps, FALSE, &(GValue) G_VALUE_INIT);

  fail_unless (gst_element_set_state (audioconvert,
          GST_STATE_PLAYING) == GST_STATE_CHANGE_SUCCESS,
      "could not set to playing");

  gst_check_setup_events (mysrcpad, audioconvert, incaps, GST_FORMAT_TIME);

  caps = gst_pad_get_current_caps (mysinkpad);
  s = gst_caps_get_structure (caps, 0);
  assert_equals_string (gst_structure_get_string (s, "format"), "F64LE");
  assert_equals_string (gst_structure_get_string (s, "layout"),
      "non-interleaved");
  gst_clear_caps (&caps);

  fail_unless (gst_element_set_state (audioconvert,
          GST_STATE_NULL) == GST_STATE_CHANGE_SUCCESS, "could not set to null");
  /* cleanup */
  GST_DEBUG ("cleanup audioconvert");
  cleanup_audioconvert (audioconvert);
  GST_DEBUG ("cleanup, unref incaps");
  gst_caps_unref (incaps);
  gst_caps_unref (outcaps);
}

GST_END_TEST;


static Suite *
audioconvert_suite (void)
{
  Suite *s = suite_create ("audioconvert");
  TCase *tc_chain = tcase_create ("general");

  suite_add_tcase (s, tc_chain);
  tcase_add_test (tc_chain, test_int16);
  tcase_add_test (tc_chain, test_float32);
  tcase_add_test (tc_chain, test_int_conversion);
  tcase_add_test (tc_chain, test_float_conversion);
  tcase_add_test (tc_chain, test_int_float_conversion);
  tcase_add_test (tc_chain, test_multichannel_conversion);
  tcase_add_test (tc_chain, test_passthrough);
  tcase_add_test (tc_chain, test_caps_negotiation);
  tcase_add_test (tc_chain, test_convert_undefined_multichannel);
  tcase_add_test (tc_chain, test_preserve_width);
  tcase_add_test (tc_chain, test_gap_buffers);
  tcase_add_test (tc_chain, test_layout_conversion);
  tcase_add_test (tc_chain, test_layout_conv_fixate_caps);
  tcase_add_test (tc_chain, test_96_channels_conversion);

  return s;
}

GST_CHECK_MAIN (audioconvert);
