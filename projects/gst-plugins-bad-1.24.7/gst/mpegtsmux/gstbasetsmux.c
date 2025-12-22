/*
 * Copyright 2006, 2007, 2008, 2009, 2010 Fluendo S.A.
 *  Authors: Jan Schmidt <jan@fluendo.com>
 *           Kapil Agrawal <kapil@fluendo.com>
 *           Julien Moutte <julien@fluendo.com>
 *
 * Copyright (C) 2011 Jan Schmidt <thaytan@noraisin.net>
 *
 * This library is licensed under 3 different licenses and you
 * can choose to use it under the terms of any one of them. The
 * three licenses are the MPL 1.1, the LGPL and the MIT license.
 *
 * MPL:
 *
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/.
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See the
 * License for the specific language governing rights and limitations
 * under the License.
 *
 * LGPL:
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
 *
 * MIT:
 *
 * Unless otherwise indicated, Source Code is licensed under MIT license.
 * See further explanation attached in License Statement (distributed in the file
 * LICENSE).
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is furnished to do
 * so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 * SPDX-License-Identifier: MPL-1.1 OR MIT OR LGPL-2.0-or-later
 */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <string.h>

#include <gst/tag/tag.h>
#include <gst/video/video.h>
#include <gst/mpegts/mpegts.h>
#include <gst/pbutils/pbutils.h>
#include <gst/videoparsers/gstjpeg2000parse.h>
#include <gst/video/video-color.h>
#include <gst/base/base.h>

#include "gstbasetsmux.h"
#include "gstbasetsmuxaac.h"
#include "gstbasetsmuxttxt.h"
#include "gstbasetsmuxopus.h"
#include "gstbasetsmuxjpeg2000.h"

GST_DEBUG_CATEGORY (gst_base_ts_mux_debug);
#define GST_CAT_DEFAULT gst_base_ts_mux_debug

/* GstBaseTsMuxPad */

G_DEFINE_TYPE (GstBaseTsMuxPad, gst_base_ts_mux_pad, GST_TYPE_AGGREGATOR_PAD);

#define DEFAULT_PAD_STREAM_NUMBER 0

enum
{
  PAD_PROP_0,
  PAD_PROP_STREAM_NUMBER,
};


/* Internals */

static void
gst_base_ts_mux_pad_reset (GstBaseTsMuxPad * pad)
{
  pad->dts = GST_CLOCK_STIME_NONE;
  pad->prog_id = -1;

  if (pad->free_func)
    pad->free_func (pad->prepare_data);
  pad->prepare_data = NULL;
  pad->prepare_func = NULL;
  pad->free_func = NULL;

  if (pad->codec_data)
    gst_buffer_replace (&pad->codec_data, NULL);

  /* reference owned elsewhere */
  pad->stream = NULL;
  pad->prog = NULL;

  if (pad->language) {
    g_free (pad->language);
    pad->language = NULL;
  }

  pad->bitrate = 0;
  pad->max_bitrate = 0;
}

/* GstAggregatorPad implementation */

static GstFlowReturn
gst_base_ts_mux_pad_flush (GstAggregatorPad * agg_pad, GstAggregator * agg)
{
  GList *cur;
  GstBaseTsMux *mux = GST_BASE_TS_MUX (agg);

  /* Send initial segments again after a flush-stop, and also resend the
   * header sections */
  g_mutex_lock (&mux->lock);
  mux->first = TRUE;

  /* output PAT, SI tables */
  tsmux_resend_pat (mux->tsmux);
  tsmux_resend_si (mux->tsmux);

  /* output PMT for each program */
  for (cur = mux->tsmux->programs; cur; cur = cur->next) {
    TsMuxProgram *program = (TsMuxProgram *) cur->data;

    tsmux_resend_pmt (program);
  }
  g_mutex_unlock (&mux->lock);

  return GST_FLOW_OK;
}

/* GObject implementation */

static void
gst_base_ts_mux_pad_dispose (GObject * obj)
{
  GstBaseTsMuxPad *ts_pad = GST_BASE_TS_MUX_PAD (obj);

  gst_base_ts_mux_pad_reset (ts_pad);

  G_OBJECT_CLASS (gst_base_ts_mux_pad_parent_class)->dispose (obj);
}

static void
gst_base_ts_mux_pad_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstBaseTsMuxPad *ts_pad = GST_BASE_TS_MUX_PAD (object);

  switch (prop_id) {
    case PAD_PROP_STREAM_NUMBER:
      ts_pad->stream_number = g_value_get_int (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_base_ts_mux_pad_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstBaseTsMuxPad *ts_pad = GST_BASE_TS_MUX_PAD (object);

  switch (prop_id) {
    case PAD_PROP_STREAM_NUMBER:
      g_value_set_int (value, ts_pad->stream_number);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_base_ts_mux_pad_class_init (GstBaseTsMuxPadClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GstAggregatorPadClass *gstaggpad_class = GST_AGGREGATOR_PAD_CLASS (klass);

  gobject_class->dispose = gst_base_ts_mux_pad_dispose;
  gobject_class->set_property = gst_base_ts_mux_pad_set_property;
  gobject_class->get_property = gst_base_ts_mux_pad_get_property;

  gstaggpad_class->flush = gst_base_ts_mux_pad_flush;

  gst_type_mark_as_plugin_api (GST_TYPE_BASE_TS_MUX, 0);

  /**
   * GstBaseTsMuxPad:stream-number:
   *
   * Set stream number for AVC video stream
   * or AAC audio streams.
   *
   * video stream number is stored in 4 bits
   * audio stream number is stored in 5 bits.
   * See Table 2-22 of ITU-T H222.0 for details on AAC and AVC stream numbers
   *
   * Since: 1.24
   */
  g_object_class_install_property (gobject_class, PAD_PROP_STREAM_NUMBER,
      g_param_spec_int ("stream-number", "stream number",
          "stream number", 0x0, 0x1F, DEFAULT_PAD_STREAM_NUMBER,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

}

static void
gst_base_ts_mux_pad_init (GstBaseTsMuxPad * vaggpad)
{
}

/* GstBaseTsMux */

enum
{
  PROP_0,
  PROP_PROG_MAP,
  PROP_PAT_INTERVAL,
  PROP_PMT_INTERVAL,
  PROP_ALIGNMENT,
  PROP_SI_INTERVAL,
  PROP_BITRATE,
  PROP_PCR_INTERVAL,
  PROP_SCTE_35_PID,
  PROP_SCTE_35_NULL_INTERVAL
};

#define DEFAULT_SCTE_35_PID 0

#define BASETSMUX_DEFAULT_ALIGNMENT    -1

#define CLOCK_BASE 9LL
#define CLOCK_FREQ (CLOCK_BASE * 10000) /* 90 kHz PTS clock */
#define CLOCK_FREQ_SCR (CLOCK_FREQ * 300)       /* 27 MHz SCR clock */
#define TS_MUX_CLOCK_BASE (TSMUX_CLOCK_FREQ * 10 * 360)

#define GSTTIME_TO_MPEGTIME(time) \
    (((time) > 0 ? (gint64) 1 : (gint64) -1) * \
    (gint64) gst_util_uint64_scale (ABS(time), CLOCK_BASE, GST_MSECOND/10))
/* 27 MHz SCR conversions: */
#define MPEG_SYS_TIME_TO_GSTTIME(time) (gst_util_uint64_scale ((time), \
                        GST_USECOND, CLOCK_FREQ_SCR / 1000000))
#define GSTTIME_TO_MPEG_SYS_TIME(time) (gst_util_uint64_scale ((time), \
                        CLOCK_FREQ_SCR / 1000000, GST_USECOND))

#define DEFAULT_PROG_ID	0

static GstStaticPadTemplate gst_base_ts_mux_src_factory =
GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("video/mpegts, "
        "systemstream = (boolean) true, " "packetsize = (int) { 188, 192} ")
    );

typedef struct
{
  GstMapInfo map_info;
  GstBuffer *buffer;
} StreamData;

G_DEFINE_TYPE_WITH_CODE (GstBaseTsMux, gst_base_ts_mux, GST_TYPE_AGGREGATOR,
    gst_mpegts_initialize ());

/* Internals */

/* Takes over the ref on the buffer */
static StreamData *
stream_data_new (GstBuffer * buffer)
{
  StreamData *res = g_new (StreamData, 1);
  res->buffer = buffer;
  gst_buffer_map (buffer, &(res->map_info), GST_MAP_READ);

  return res;
}

static void
stream_data_free (StreamData * data)
{
  if (data) {
    gst_buffer_unmap (data->buffer, &data->map_info);
    gst_buffer_unref (data->buffer);
    g_free (data);
  }
}

#define parent_class gst_base_ts_mux_parent_class

static void
gst_base_ts_mux_set_header_on_caps (GstBaseTsMux * mux)
{
  GstBuffer *buf;
  GstStructure *structure;
  GValue array = { 0 };
  GValue value = { 0 };
  GstCaps *caps;

  caps = gst_pad_get_pad_template_caps (GST_AGGREGATOR_SRC_PAD (mux));

  caps = gst_caps_make_writable (caps);
  structure = gst_caps_get_structure (caps, 0);

  gst_structure_set (structure, "packetsize", G_TYPE_INT, mux->packet_size,
      NULL);

  g_value_init (&array, GST_TYPE_ARRAY);

  GST_LOG_OBJECT (mux, "setting %u packets into streamheader",
      g_queue_get_length (&mux->streamheader));

  while ((buf = GST_BUFFER (g_queue_pop_head (&mux->streamheader)))) {
    g_value_init (&value, GST_TYPE_BUFFER);
    gst_value_take_buffer (&value, buf);
    gst_value_array_append_value (&array, &value);
    g_value_unset (&value);
  }

  gst_structure_set_value (structure, "streamheader", &array);
  gst_aggregator_set_src_caps (GST_AGGREGATOR (mux), caps);
  g_value_unset (&array);
  gst_caps_unref (caps);
}

static gboolean
steal_si_section (GstMpegtsSectionType * type, TsMuxSection * section,
    TsMux * mux)
{
  g_hash_table_insert (mux->si_sections, type, section);

  return TRUE;
}

/* Must be called with mux->lock held */
static void
gst_base_ts_mux_reset (GstBaseTsMux * mux, gboolean alloc)
{
  GstBuffer *buf;
  GstBaseTsMuxClass *klass = GST_BASE_TS_MUX_GET_CLASS (mux);
  GHashTable *si_sections = NULL;
  GList *l;

  mux->first = TRUE;
  mux->last_flow_ret = GST_FLOW_OK;
  mux->last_ts = GST_CLOCK_TIME_NONE;
  mux->is_delta = TRUE;
  mux->is_header = FALSE;

  mux->streamheader_sent = FALSE;
  mux->pending_key_unit_ts = GST_CLOCK_TIME_NONE;
  gst_event_replace (&mux->force_key_unit_event, NULL);

  if (mux->out_adapter)
    gst_adapter_clear (mux->out_adapter);
  mux->output_ts_offset = GST_CLOCK_STIME_NONE;

  if (mux->tsmux) {
    if (mux->tsmux->si_sections)
      si_sections = g_hash_table_ref (mux->tsmux->si_sections);

    tsmux_free (mux->tsmux);
    mux->tsmux = NULL;
  }

  if (mux->programs) {
    g_hash_table_destroy (mux->programs);
  }
  mux->programs = g_hash_table_new (g_direct_hash, g_direct_equal);

  while ((buf = GST_BUFFER (g_queue_pop_head (&mux->streamheader))))
    gst_buffer_unref (buf);

  gst_event_replace (&mux->force_key_unit_event, NULL);
  gst_buffer_replace (&mux->out_buffer, NULL);

  GST_OBJECT_LOCK (mux);

  for (l = GST_ELEMENT (mux)->sinkpads; l; l = l->next) {
    gst_base_ts_mux_pad_reset (GST_BASE_TS_MUX_PAD (l->data));
  }

  GST_OBJECT_UNLOCK (mux);

  if (alloc) {
    g_assert (klass->create_ts_mux);

    mux->tsmux = klass->create_ts_mux (mux);

    /* Preserve user-specified sections across resets */
    if (si_sections)
      g_hash_table_foreach_steal (si_sections, (GHRFunc) steal_si_section,
          mux->tsmux);
  }

  if (si_sections)
    g_hash_table_unref (si_sections);

  mux->last_scte35_event_seqnum = GST_SEQNUM_INVALID;

  if (klass->reset)
    klass->reset (mux);
}

static void
release_buffer_cb (guint8 * data, void *user_data)
{
  stream_data_free ((StreamData *) user_data);
}

/* Must be called with mux->lock held */
static GstFlowReturn
gst_base_ts_mux_create_or_update_stream (GstBaseTsMux * mux,
    GstBaseTsMuxPad * ts_pad, GstCaps * caps)
{
  GstStructure *s;
  guint st = TSMUX_ST_RESERVED;
  const gchar *mt;
  const GValue *value = NULL;
  GstBuffer *codec_data = NULL;
  guint8 opus_channel_config[1 + 2 + 1 + 1 + 255] = { 0, };
  gsize opus_channel_config_len = 0;
  guint16 profile = GST_JPEG2000_PARSE_PROFILE_NONE;
  guint8 main_level = 0;
  guint32 max_rate = 0;
  guint8 color_spec = 0;
  const gchar *stream_format = NULL;
  const char *interlace_mode = NULL;
  gchar *pmt_name;

  GST_DEBUG_OBJECT (ts_pad,
      "%s stream with PID 0x%04x for caps %" GST_PTR_FORMAT,
      ts_pad->stream ? "Recreating" : "Creating", ts_pad->pid, caps);

  s = gst_caps_get_structure (caps, 0);

  mt = gst_structure_get_name (s);
  value = gst_structure_get_value (s, "codec_data");
  if (value != NULL)
    codec_data = gst_value_get_buffer (value);

  g_clear_pointer (&ts_pad->codec_data, gst_buffer_unref);
  ts_pad->prepare_func = NULL;

  stream_format = gst_structure_get_string (s, "stream-format");

  if (strcmp (mt, "video/x-dirac") == 0) {
    st = TSMUX_ST_VIDEO_DIRAC;
  } else if (strcmp (mt, "audio/x-ac3") == 0) {
    st = TSMUX_ST_PS_AUDIO_AC3;
  } else if (strcmp (mt, "audio/x-dts") == 0) {
    st = TSMUX_ST_PS_AUDIO_DTS;
  } else if (strcmp (mt, "audio/x-lpcm") == 0) {
    st = TSMUX_ST_PS_AUDIO_LPCM;
  } else if (strcmp (mt, "video/x-h264") == 0) {
    st = TSMUX_ST_VIDEO_H264;
  } else if (strcmp (mt, "video/x-h265") == 0) {
    st = TSMUX_ST_VIDEO_HEVC;
  } else if (strcmp (mt, "audio/mpeg") == 0) {
    gint mpegversion;

    if (!gst_structure_get_int (s, "mpegversion", &mpegversion)) {
      GST_ERROR_OBJECT (ts_pad, "caps missing mpegversion");
      goto not_negotiated;
    }

    switch (mpegversion) {
      case 1:{
        int mpegaudioversion = 1;       /* Assume mpegaudioversion=1 for backwards compatibility */
        (void) gst_structure_get_int (s, "mpegaudioversion", &mpegaudioversion);

        if (mpegaudioversion == 1)
          st = TSMUX_ST_AUDIO_MPEG1;
        else
          st = TSMUX_ST_AUDIO_MPEG2;
        break;
      }
      case 2:{
        /* mpegversion=2 in GStreamer refers to MPEG-2 Part 7 audio,  */

        st = TSMUX_ST_AUDIO_AAC;

        /* Check the stream format. If raw, make dummy internal codec data from the caps */
        if (g_strcmp0 (stream_format, "raw") == 0) {
          ts_pad->codec_data =
              gst_base_ts_mux_aac_mpeg2_make_codec_data (mux, caps);
          ts_pad->prepare_func = gst_base_ts_mux_prepare_aac_mpeg2;
          if (ts_pad->codec_data == NULL) {
            GST_ERROR_OBJECT (mux, "Invalid or incomplete caps for MPEG-2 AAC");
            goto not_negotiated;
          }
        }
        break;
      }
      case 4:
      {
        st = TSMUX_ST_AUDIO_AAC;

        /* Check the stream format. We need codec_data with RAW streams and mpegversion=4 */
        if (g_strcmp0 (stream_format, "raw") == 0) {
          if (codec_data) {
            GST_DEBUG_OBJECT (ts_pad,
                "we have additional codec data (%" G_GSIZE_FORMAT " bytes)",
                gst_buffer_get_size (codec_data));
            ts_pad->codec_data = gst_buffer_ref (codec_data);
            ts_pad->prepare_func = gst_base_ts_mux_prepare_aac_mpeg4;
          } else {
            ts_pad->codec_data = NULL;
            GST_ERROR_OBJECT (mux, "Need codec_data for raw MPEG-4 AAC");
            goto not_negotiated;
          }
        } else if (codec_data) {
          ts_pad->codec_data = gst_buffer_ref (codec_data);
        } else {
          ts_pad->codec_data = NULL;
        }
        break;
      }
      default:
        GST_WARNING_OBJECT (ts_pad, "unsupported mpegversion %d", mpegversion);
        goto not_negotiated;
    }
  } else if (strcmp (mt, "video/mpeg") == 0) {
    gint mpegversion;

    if (!gst_structure_get_int (s, "mpegversion", &mpegversion)) {
      GST_ERROR_OBJECT (ts_pad, "caps missing mpegversion");
      goto not_negotiated;
    }

    switch (mpegversion) {
      case 1:
        st = TSMUX_ST_VIDEO_MPEG1;
        break;
      case 2:
        st = TSMUX_ST_VIDEO_MPEG2;
        break;
      case 4:
        st = TSMUX_ST_VIDEO_MPEG4;
        break;
      default:
        GST_WARNING_OBJECT (ts_pad, "unsupported mpegversion %d", mpegversion);
        goto not_negotiated;
    }
  } else if (strcmp (mt, "subpicture/x-dvb") == 0) {
    st = TSMUX_ST_PS_DVB_SUBPICTURE;
  } else if (strcmp (mt, "application/x-teletext") == 0) {
    st = TSMUX_ST_PS_TELETEXT;
    /* needs a particularly sized layout */
    ts_pad->prepare_func = gst_base_ts_mux_prepare_teletext;
  } else if (strcmp (mt, "audio/x-opus") == 0) {
    guint8 channels, mapping_family, stream_count, coupled_count;
    guint8 channel_mapping[256];

    if (!gst_codec_utils_opus_parse_caps (caps, NULL, &channels,
            &mapping_family, &stream_count, &coupled_count, channel_mapping)) {
      GST_ERROR_OBJECT (ts_pad, "Incomplete Opus caps");
      goto not_negotiated;
    }

    if (channels <= 2 && mapping_family == 0) {
      opus_channel_config[0] = channels;
      opus_channel_config_len = 1;
    } else if (channels == 2 && mapping_family == 255 && ((stream_count == 1
                && coupled_count == 1) || (stream_count == 2
                && coupled_count == 0))) {
      /* Dual mono */
      opus_channel_config[0] = coupled_count == 0 ? 0x80 : 0x00;
      opus_channel_config_len = 1;
    } else if (channels >= 2 && channels <= 8 && mapping_family == 1) {
      static const guint8 coupled_stream_counts[9] = {
        1, 0, 1, 1, 2, 2, 2, 3, 3
      };
      static const guint8 channel_map_a[8][8] = {
        {0},
        {0, 1},
        {0, 2, 1},
        {0, 1, 2, 3},
        {0, 4, 1, 2, 3},
        {0, 4, 1, 2, 3, 5},
        {0, 4, 1, 2, 3, 5, 6},
        {0, 6, 1, 2, 3, 4, 5, 7},
      };
      static const guint8 channel_map_b[8][8] = {
        {0},
        {0, 1},
        {0, 1, 2},
        {0, 1, 2, 3},
        {0, 1, 2, 3, 4},
        {0, 1, 2, 3, 4, 5},
        {0, 1, 2, 3, 4, 5, 6},
        {0, 1, 2, 3, 4, 5, 6, 7},
      };

      /* Vorbis mapping */
      if (stream_count == channels - coupled_stream_counts[channels] &&
          coupled_count == coupled_stream_counts[channels] &&
          memcmp (channel_mapping, channel_map_a[channels - 1],
              channels) == 0) {
        opus_channel_config[0] = channels;
        opus_channel_config_len = 1;
      } else if (stream_count == channels - coupled_stream_counts[channels] &&
          coupled_count == coupled_stream_counts[channels] &&
          memcmp (channel_mapping, channel_map_b[channels - 1],
              channels) == 0) {
        opus_channel_config[0] = channels | 0x80;
        opus_channel_config_len = 1;
      } else {
        GST_FIXME_OBJECT (ts_pad, "Opus channel mapping not handled");
        goto not_negotiated;
      }
    } else {
      GstBitWriter writer;
      guint i;
      guint n_bits;

      gst_bit_writer_init_with_data (&writer, opus_channel_config,
          sizeof (opus_channel_config), FALSE);
      gst_bit_writer_put_bits_uint8_unchecked (&writer, 0x81, 8);
      gst_bit_writer_put_bits_uint8_unchecked (&writer, channels, 8);
      gst_bit_writer_put_bits_uint8_unchecked (&writer, mapping_family, 8);

      n_bits = g_bit_storage (channels);
      gst_bit_writer_put_bits_uint8_unchecked (&writer, stream_count - 1,
          n_bits);
      n_bits = g_bit_storage (stream_count + 1);
      gst_bit_writer_put_bits_uint8_unchecked (&writer, coupled_count, n_bits);

      n_bits = g_bit_storage (stream_count + coupled_count + 1);
      for (i = 0; i < channels; i++) {
        gst_bit_writer_put_bits_uint8_unchecked (&writer, channel_mapping[i],
            n_bits);
      }

      gst_bit_writer_align_bytes_unchecked (&writer, 0);
      g_assert (writer.bit_size % 8 == 0);

      opus_channel_config_len = writer.bit_size / 8;
    }

    st = TSMUX_ST_PS_OPUS;
    ts_pad->prepare_func = gst_base_ts_mux_prepare_opus;
  } else if (strcmp (mt, "meta/x-klv") == 0) {
    st = TSMUX_ST_PS_KLV;
  } else if (strcmp (mt, "image/x-jpc") == 0) {
    /*
     * See this document for more details on standard:
     *
     * https://www.itu.int/rec/T-REC-H.222.0-201206-S/en
     *  Annex S describes J2K details
     *  Page 104 of this document describes J2k video descriptor
     */

    const GValue *vProfile = gst_structure_get_value (s, "profile");
    const GValue *vMainlevel = gst_structure_get_value (s, "main-level");
    const GValue *vFramerate = gst_structure_get_value (s, "framerate");
    const GValue *vColorimetry = gst_structure_get_value (s, "colorimetry");
    j2k_private_data *private_data;

    /* for now, we relax the condition that profile must exist and equal
     * GST_JPEG2000_PARSE_PROFILE_BC_SINGLE */
    if (vProfile) {
      profile = g_value_get_int (vProfile);
      if (profile != GST_JPEG2000_PARSE_PROFILE_BC_SINGLE) {
        GST_LOG_OBJECT (ts_pad, "Invalid JPEG 2000 profile %d", profile);
        /* goto not_negotiated; */
      }
    }
    /* for now, we will relax the condition that the main level must be present */
    if (vMainlevel) {
      main_level = g_value_get_uint (vMainlevel);
      if (main_level > 11) {
        GST_ERROR_OBJECT (ts_pad, "Invalid main level %d", main_level);
        goto not_negotiated;
      }
      if (main_level >= 6) {
        max_rate = 2 ^ (main_level - 6) * 1600 * 1000000;
      } else {
        switch (main_level) {
          case 0:
          case 1:
          case 2:
          case 3:
            max_rate = 200 * 1000000;
            break;
          case 4:
            max_rate = 400 * 1000000;
            break;
          case 5:
            max_rate = 800 * 1000000;
            break;
          default:
            break;
        }
      }
    } else {
      /* GST_ERROR_OBJECT (ts_pad, "Missing main level");
       * goto not_negotiated; */
    }

    /* We always mux video in J2K-over-MPEG-TS non-interlaced mode */
    private_data = g_new0 (j2k_private_data, 1);
    private_data->interlace = FALSE;
    private_data->den = 0;
    private_data->num = 0;
    private_data->max_bitrate = max_rate;
    private_data->color_spec = 1;
    /* these two fields are not used, since we always mux as non-interlaced */
    private_data->Fic = 1;
    private_data->Fio = 0;

    /* Get Framerate */
    if (vFramerate != NULL) {
      /* Data for ELSM header */
      private_data->num = gst_value_get_fraction_numerator (vFramerate);
      private_data->den = gst_value_get_fraction_denominator (vFramerate);
    }
    /* Get Colorimetry */
    if (vColorimetry) {
      const char *colorimetry = g_value_get_string (vColorimetry);
      color_spec = GST_MPEGTS_JPEG2000_COLORSPEC_SRGB;  /* RGB as default */
      if (g_str_equal (colorimetry, GST_VIDEO_COLORIMETRY_BT601)) {
        color_spec = GST_MPEGTS_JPEG2000_COLORSPEC_REC601;
      } else {
        if (g_str_equal (colorimetry, GST_VIDEO_COLORIMETRY_BT709)
            || g_str_equal (colorimetry, GST_VIDEO_COLORIMETRY_SMPTE240M)) {
          color_spec = GST_MPEGTS_JPEG2000_COLORSPEC_REC709;
        }
      }
      private_data->color_spec = color_spec;
    } else {
      GST_ERROR_OBJECT (ts_pad, "Colorimetry not present in caps");
      g_free (private_data);
      goto not_negotiated;
    }
    st = TSMUX_ST_VIDEO_JP2K;
    ts_pad->prepare_func = gst_base_ts_mux_prepare_jpeg2000;
    ts_pad->prepare_data = private_data;
    ts_pad->free_func = gst_base_ts_mux_free_jpeg2000;
  } else {
    GstBaseTsMuxClass *klass = GST_BASE_TS_MUX_GET_CLASS (mux);

    if (klass->handle_media_type) {
      st = klass->handle_media_type (mux, mt, ts_pad);
    }
  }

  if (st == TSMUX_ST_RESERVED) {
    GST_ERROR_OBJECT (ts_pad, "Failed to determine stream type");
    goto error;
  }

  if (ts_pad->stream && st != ts_pad->stream->stream_type) {
    GST_ELEMENT_ERROR (mux, STREAM, MUX,
        ("Stream type change from %02x to %02x not supported",
            ts_pad->stream->stream_type, st), NULL);
    goto error;
  }

  if (ts_pad->stream == NULL) {
    gint stream_number = DEFAULT_PAD_STREAM_NUMBER;

    g_object_get (ts_pad, "stream-number", &stream_number, NULL);
    ts_pad->stream =
        tsmux_create_stream (mux->tsmux, st, stream_number, ts_pad->pid,
        ts_pad->language, ts_pad->bitrate, ts_pad->max_bitrate);
    if (ts_pad->stream == NULL)
      goto error;
  }

  pmt_name = g_strdup_printf ("PMT_%d", ts_pad->pid);
  if (mux->prog_map && gst_structure_has_field (mux->prog_map, pmt_name)) {
    gst_structure_get_int (mux->prog_map, pmt_name, &ts_pad->stream->pmt_index);
  }
  g_free (pmt_name);

  interlace_mode = gst_structure_get_string (s, "interlace-mode");
  gst_structure_get_int (s, "rate", &ts_pad->stream->audio_sampling);
  gst_structure_get_int (s, "channels", &ts_pad->stream->audio_channels);
  gst_structure_get_int (s, "bitrate", &ts_pad->stream->audio_bitrate);

  /* frame rate */
  gst_structure_get_fraction (s, "framerate", &ts_pad->stream->num,
      &ts_pad->stream->den);

  /* Interlace mode */
  ts_pad->stream->interlace_mode = FALSE;
  if (interlace_mode) {
    ts_pad->stream->interlace_mode =
        g_str_equal (interlace_mode, "interleaved");
  }

  /* Width and Height */
  gst_structure_get_int (s, "width", &ts_pad->stream->horizontal_size);
  gst_structure_get_int (s, "height", &ts_pad->stream->vertical_size);

  ts_pad->stream->color_spec = color_spec;
  ts_pad->stream->max_bitrate = max_rate;
  ts_pad->stream->profile_and_level = profile | main_level;

  memcpy (ts_pad->stream->opus_channel_config, opus_channel_config,
      sizeof (opus_channel_config));
  ts_pad->stream->opus_channel_config_len = opus_channel_config_len;

  tsmux_stream_set_buffer_release_func (ts_pad->stream, release_buffer_cb);

  return GST_FLOW_OK;

  /* ERRORS */
not_negotiated:
  return GST_FLOW_NOT_NEGOTIATED;

error:
  return GST_FLOW_ERROR;
}

static gboolean
is_valid_pmt_pid (guint16 pmt_pid)
{
  if (pmt_pid < 0x0010 || pmt_pid > 0x1ffe)
    return FALSE;
  return TRUE;
}

/* Must be called with mux->lock held */
static GstFlowReturn
gst_base_ts_mux_create_stream (GstBaseTsMux * mux, GstBaseTsMuxPad * ts_pad,
    GstCaps * caps)
{
  GstFlowReturn ret;

  ret = gst_base_ts_mux_create_or_update_stream (mux, ts_pad, caps);

  if (ret == GST_FLOW_OK) {
    tsmux_program_add_stream (ts_pad->prog, ts_pad->stream);
  }

  return ret;
}

static guint16
get_pmt_pcr_pid (GstBaseTsMux * mux, const gchar * prop_name)
{
  if (mux->prog_map == NULL)
    return 0;
  gint pcr_pid = 0;
  if (!gst_structure_get (mux->prog_map, prop_name, G_TYPE_INT, &pcr_pid, NULL))
    return 0;
  if (pcr_pid < 1 || pcr_pid > G_MAXUINT16)
    return 0;
  return (guint16) pcr_pid;
}

static gchar *
get_pmt_pcr_sink (GstBaseTsMux * mux, const gchar * prop_name)
{
  if (mux->prog_map == NULL)
    return 0;
  gchar *pcr_sink = NULL;
  if (!gst_structure_get (mux->prog_map, prop_name, G_TYPE_STRING, &pcr_sink,
          NULL)) {
    return NULL;
  }
  return pcr_sink;
}

/* Must be called with mux->lock held */
static GstFlowReturn
gst_base_ts_mux_create_pad_stream (GstBaseTsMux * mux, GstPad * pad,
    gboolean allow_no_caps)
{
  GstBaseTsMuxPad *ts_pad = GST_BASE_TS_MUX_PAD (pad);
  gchar *name = NULL;
  gchar *prop_name;
  GstFlowReturn ret = GST_FLOW_OK;
  GstCaps *caps = gst_pad_get_current_caps (pad);

  if (caps == NULL) {
    GST_DEBUG_OBJECT (ts_pad, "Sink pad caps were not set yet");
    /* Try again later once the first buffer is pushed */
    if (allow_no_caps)
      return GST_FLOW_OK;
    return GST_FLOW_NOT_NEGOTIATED;
  }

  if (ts_pad->prog_id == -1) {
    name = GST_PAD_NAME (pad);
    if (mux->prog_map != NULL && gst_structure_has_field (mux->prog_map, name)) {
      gint idx;
      gboolean ret = gst_structure_get_int (mux->prog_map, name, &idx);
      if (!ret) {
        GST_ELEMENT_ERROR (mux, STREAM, MUX,
            ("Reading program map failed. Assuming default"), (NULL));
        idx = DEFAULT_PROG_ID;
      }
      if (idx < 0) {
        GST_DEBUG_OBJECT (mux, "Program number %d associate with pad %s less "
            "than zero; DEFAULT_PROGRAM = %d is used instead",
            idx, name, DEFAULT_PROG_ID);
        idx = DEFAULT_PROG_ID;
      }
      ts_pad->prog_id = idx;
    } else {
      ts_pad->prog_id = DEFAULT_PROG_ID;
    }
  }

  ts_pad->prog =
      (TsMuxProgram *) g_hash_table_lookup (mux->programs,
      GINT_TO_POINTER (ts_pad->prog_id));
  if (ts_pad->prog == NULL) {
    ts_pad->prog = tsmux_program_new (mux->tsmux, ts_pad->prog_id);
    if (ts_pad->prog == NULL)
      goto no_program;
    tsmux_set_pmt_interval (ts_pad->prog, mux->pmt_interval);
    tsmux_program_set_scte35_pid (ts_pad->prog, mux->scte35_pid);
    tsmux_program_set_scte35_interval (ts_pad->prog, mux->scte35_null_interval);
    g_hash_table_insert (mux->programs, GINT_TO_POINTER (ts_pad->prog_id),
        ts_pad->prog);

    /* Check for user-specified PMT PID */
    prop_name = g_strdup_printf ("PMT_%d", ts_pad->prog->pgm_number);
    if (mux->prog_map && gst_structure_has_field (mux->prog_map, prop_name)) {
      guint pmt_pid;

      if (gst_structure_get_uint (mux->prog_map, prop_name, &pmt_pid)) {
        if (is_valid_pmt_pid (pmt_pid)) {
          GST_DEBUG_OBJECT (mux, "User specified pid=%u as PMT for "
              "program (prog_id = %d)", pmt_pid, ts_pad->prog->pgm_number);
          tsmux_program_set_pmt_pid (ts_pad->prog, pmt_pid);
        } else {
          GST_ELEMENT_WARNING (mux, LIBRARY, SETTINGS,
              ("User specified PMT pid %u for program %d is not valid.",
                  pmt_pid, ts_pad->prog->pgm_number), (NULL));
        }
      }
    }
    g_free (prop_name);
  }

  if (ts_pad->stream == NULL) {
    ret = gst_base_ts_mux_create_stream (mux, ts_pad, caps);
    if (ret != GST_FLOW_OK)
      goto no_stream;
  }
  ts_pad->stream->program = ts_pad->prog;

  if (ts_pad->prog->pcr_stream == NULL) {
    /* Take the first stream of the program for the PCR */
    GST_DEBUG_OBJECT (ts_pad,
        "Use stream (pid=%d) from pad as PCR for program (prog_id = %d)",
        ts_pad->pid, ts_pad->prog_id);

    tsmux_program_set_pcr_stream (ts_pad->prog, ts_pad->stream);
  }

  /* Check for user-specified PCR PID */
  prop_name = g_strdup_printf ("PCR_%d", ts_pad->prog->pgm_number);
  guint16 pcr_pid = get_pmt_pcr_pid (mux, prop_name);
  if (pcr_pid) {
    GST_DEBUG_OBJECT (mux, "User specified PID %d as PCR for "
        "program (prog_id = %d)", pcr_pid, ts_pad->prog->pgm_number);
    tsmux_program_set_pcr_pid (ts_pad->prog, pcr_pid);
    goto have_pcr_pid;
  }

  gchar *pcr_sink_name = get_pmt_pcr_sink (mux, prop_name);
  if (!g_strcmp0 (GST_PAD_NAME (pad), pcr_sink_name)) {
    GST_DEBUG_OBJECT (mux, "User specified stream (pid=%d) as PCR for "
        "program (prog_id = %d)", ts_pad->pid, ts_pad->prog->pgm_number);
    tsmux_program_set_pcr_stream (ts_pad->prog, ts_pad->stream);
  }
  g_clear_pointer (&pcr_sink_name, g_free);

have_pcr_pid:
  g_clear_pointer (&prop_name, g_free);
  gst_clear_caps (&caps);

  return ret;

  /* ERRORS */
no_program:
  {
    GST_ELEMENT_ERROR (mux, STREAM, MUX,
        ("Could not create new program"), (NULL));
    gst_clear_caps (&caps);
    return GST_FLOW_ERROR;
  }
no_stream:
  {
    GST_ELEMENT_ERROR (mux, STREAM, MUX,
        ("Could not create handler for stream"), (NULL));
    gst_clear_caps (&caps);
    return ret;
  }
}

/* Must be called with mux->lock held */
static gboolean
gst_base_ts_mux_create_pad_stream_func (GstElement * element, GstPad * pad,
    gpointer user_data)
{
  GstFlowReturn *ret = user_data;

  *ret =
      gst_base_ts_mux_create_pad_stream (GST_BASE_TS_MUX (element), pad, TRUE);

  return *ret == GST_FLOW_OK;
}

/* Must be called with mux->lock held */
static GstFlowReturn
gst_base_ts_mux_create_streams (GstBaseTsMux * mux)
{
  GstFlowReturn ret = GST_FLOW_OK;

  gst_element_foreach_sink_pad (GST_ELEMENT_CAST (mux),
      gst_base_ts_mux_create_pad_stream_func, &ret);

  return ret;
}

static void
new_packet_common_init (GstBaseTsMux * mux, GstBuffer * buf, guint8 * data,
    guint len)
{
  /* Packets should be at least 188 bytes, but check anyway */
  g_assert (len >= 2 || !data);

  if (!mux->streamheader_sent && data) {
    guint pid = ((data[1] & 0x1f) << 8) | data[2];
    /* if it's a PAT or a PMT */
    if (pid == 0x00 || (pid >= TSMUX_START_PMT_PID && pid < TSMUX_START_ES_PID)) {
      GstBuffer *hbuf;

      if (!buf) {
        hbuf = gst_buffer_new_and_alloc (len);
        gst_buffer_fill (hbuf, 0, data, len);
      } else {
        hbuf = gst_buffer_copy (buf);
      }
      GST_LOG_OBJECT (mux,
          "Collecting packet with pid 0x%04x into streamheaders", pid);

      g_queue_push_tail (&mux->streamheader, hbuf);
    } else if (!g_queue_is_empty (&mux->streamheader)) {
      gst_base_ts_mux_set_header_on_caps (mux);
      mux->streamheader_sent = TRUE;
    }
  }

  if (buf) {
    if (mux->is_header) {
      GST_LOG_OBJECT (mux, "marking as header buffer");
      GST_BUFFER_FLAG_SET (buf, GST_BUFFER_FLAG_HEADER);
    }
    if (mux->is_delta) {
      GST_LOG_OBJECT (mux, "marking as delta unit");
      GST_BUFFER_FLAG_SET (buf, GST_BUFFER_FLAG_DELTA_UNIT);
    } else {
      GST_DEBUG_OBJECT (mux, "marking as non-delta unit");
      mux->is_delta = TRUE;
    }
  }
}

static GstFlowReturn
gst_base_ts_mux_push_packets (GstBaseTsMux * mux, gboolean force)
{
  GstSegment *segment =
      &GST_AGGREGATOR_PAD (GST_AGGREGATOR_SRC_PAD (mux))->segment;
  GstBufferList *buffer_list;
  gint align = mux->alignment;
  gint av, packet_size;
  GstFlowReturn flow_ret;
  GstClockTime pts;

  packet_size = mux->packet_size;

  if (align < 0)
    align = mux->automatic_alignment;

  av = gst_adapter_available (mux->out_adapter);
  GST_LOG_OBJECT (mux, "align %d, av %d", align, av);

  if (av == 0)
    return GST_FLOW_OK;

  /* no alignment, just push all available data */
  if (align == 0) {
    buffer_list = gst_adapter_take_buffer_list (mux->out_adapter, av);
    flow_ret = gst_aggregator_finish_buffer_list (GST_AGGREGATOR (mux),
        buffer_list);

    pts = gst_adapter_prev_pts (mux->out_adapter, NULL);
    if (GST_CLOCK_TIME_IS_VALID (pts)
        && (!GST_CLOCK_TIME_IS_VALID (segment->position)
            || segment->position < pts))
      segment->position = pts;

    return flow_ret;
  }

  align *= packet_size;

  if (!force && align > av)
    return GST_FLOW_OK;

  buffer_list = gst_buffer_list_new_sized ((av / align) + 1);

  GST_LOG_OBJECT (mux, "aligning to %d bytes", align);
  while (align <= av) {
    GstBuffer *buf;

    pts = gst_adapter_prev_pts (mux->out_adapter, NULL);
    buf = gst_adapter_take_buffer (mux->out_adapter, align);

    GST_BUFFER_PTS (buf) = pts;

    gst_buffer_list_add (buffer_list, buf);
    av -= align;
  }

  if (av > 0 && force) {
    GstBuffer *buf;
    guint8 *data;
    guint32 header;
    gint dummy;
    GstMapInfo map;

    GST_LOG_OBJECT (mux, "handling %d leftover bytes", av);

    pts = gst_adapter_prev_pts (mux->out_adapter, NULL);
    buf = gst_buffer_new_and_alloc (align);

    GST_BUFFER_PTS (buf) = pts;

    gst_buffer_map (buf, &map, GST_MAP_READ);
    data = map.data;

    gst_adapter_copy (mux->out_adapter, data, 0, av);
    gst_adapter_clear (mux->out_adapter);

    data += av;
    header = GST_READ_UINT32_BE (data - packet_size);

    dummy = (map.size - av) / packet_size;
    GST_LOG_OBJECT (mux, "adding %d null packets", dummy);

    for (; dummy > 0; dummy--) {
      gint offset;

      if (packet_size > GST_BASE_TS_MUX_NORMAL_PACKET_LENGTH) {
        GST_WRITE_UINT32_BE (data, header);
        /* simply increase header a bit and never mind too much */
        header++;
        offset = 4;
      } else {
        offset = 0;
      }
      GST_WRITE_UINT8 (data + offset, TSMUX_SYNC_BYTE);
      /* null packet PID */
      GST_WRITE_UINT16_BE (data + offset + 1, 0x1FFF);
      /* no adaptation field exists | continuity counter undefined */
      GST_WRITE_UINT8 (data + offset + 3, 0x10);
      /* payload */
      memset (data + offset + 4, 0, GST_BASE_TS_MUX_NORMAL_PACKET_LENGTH - 4);
      data += packet_size;
    }

    gst_buffer_unmap (buf, &map);
    gst_buffer_list_add (buffer_list, buf);
  }

  flow_ret =
      gst_aggregator_finish_buffer_list (GST_AGGREGATOR (mux), buffer_list);

  pts = gst_adapter_prev_pts (mux->out_adapter, NULL);
  if (GST_CLOCK_TIME_IS_VALID (pts)
      && (!GST_CLOCK_TIME_IS_VALID (segment->position)
          || segment->position < pts))
    segment->position = pts;

  return flow_ret;
}

static GstFlowReturn
gst_base_ts_mux_collect_packet (GstBaseTsMux * mux, GstBuffer * buf)
{
  GST_LOG_OBJECT (mux, "collecting packet size %" G_GSIZE_FORMAT,
      gst_buffer_get_size (buf));
  gst_adapter_push (mux->out_adapter, buf);

  return GST_FLOW_OK;
}

static GstEvent *
check_pending_key_unit_event (GstEvent * pending_event, GstSegment * segment,
    GstClockTime timestamp, guint flags, GstClockTime pending_key_unit_ts)
{
  GstClockTime running_time, stream_time;
  gboolean all_headers;
  guint count;
  GstEvent *event = NULL;

  g_assert (segment != NULL);

  if (pending_event == NULL)
    goto out;

  if (GST_CLOCK_TIME_IS_VALID (pending_key_unit_ts) &&
      timestamp == GST_CLOCK_TIME_NONE)
    goto out;

  running_time = timestamp;

  GST_INFO ("now %" GST_TIME_FORMAT " wanted %" GST_TIME_FORMAT,
      GST_TIME_ARGS (running_time), GST_TIME_ARGS (pending_key_unit_ts));
  if (GST_CLOCK_TIME_IS_VALID (pending_key_unit_ts) &&
      running_time < pending_key_unit_ts)
    goto out;

  if (flags & GST_BUFFER_FLAG_DELTA_UNIT) {
    GST_INFO ("pending force key unit, waiting for keyframe");
    goto out;
  }

  stream_time = gst_segment_to_stream_time (segment,
      GST_FORMAT_TIME, timestamp);

  if (GST_EVENT_TYPE (pending_event) == GST_EVENT_CUSTOM_DOWNSTREAM) {
    gst_video_event_parse_downstream_force_key_unit (pending_event,
        NULL, NULL, NULL, &all_headers, &count);
  } else {
    gst_video_event_parse_upstream_force_key_unit (pending_event, NULL,
        &all_headers, &count);
  }

  event =
      gst_video_event_new_downstream_force_key_unit (timestamp, stream_time,
      running_time, all_headers, count);
  gst_event_set_seqnum (event, gst_event_get_seqnum (pending_event));

out:
  return event;
}

/* Called when the TsMux has prepared a packet for output. Return FALSE
 * on error */
static gboolean
new_packet_cb (GstBuffer * buf, void *user_data, gint64 new_pcr)
{
  GstBaseTsMux *mux = (GstBaseTsMux *) user_data;
  GstAggregator *agg = GST_AGGREGATOR (mux);
  GstBaseTsMuxClass *klass = GST_BASE_TS_MUX_GET_CLASS (mux);
  GstMapInfo map;
  GstSegment *agg_segment = &GST_AGGREGATOR_PAD (agg->srcpad)->segment;

  g_assert (klass->output_packet);

  gst_buffer_map (buf, &map, GST_MAP_READWRITE);

  if (!GST_CLOCK_TIME_IS_VALID (GST_BUFFER_PTS (buf))) {
    /* tsmux isn't generating timestamps. Use the input times */
    GST_BUFFER_PTS (buf) = mux->last_ts;
  }

  if (GST_CLOCK_TIME_IS_VALID (GST_BUFFER_PTS (buf))) {
    if (!GST_CLOCK_STIME_IS_VALID (mux->output_ts_offset)) {
      GstClockTime output_start_time = agg_segment->position;
      if (agg_segment->position == -1
          || agg_segment->position < agg_segment->start) {
        output_start_time = agg_segment->start;
      }

      mux->output_ts_offset =
          GST_CLOCK_DIFF (GST_BUFFER_PTS (buf), output_start_time);

      GST_DEBUG_OBJECT (mux, "New output ts offset %" GST_STIME_FORMAT,
          GST_STIME_ARGS (mux->output_ts_offset));
    }

    GST_BUFFER_PTS (buf) += mux->output_ts_offset;

    agg_segment->position = GST_BUFFER_PTS (buf);
  } else if (agg_segment->position == -1
      || agg_segment->position < agg_segment->start) {
    GST_BUFFER_PTS (buf) = agg_segment->start;
  } else {
    GST_BUFFER_PTS (buf) = agg_segment->position;
  }

  /* do common init (flags and streamheaders) */
  new_packet_common_init (mux, buf, map.data, map.size);

  gst_buffer_unmap (buf, &map);

  return klass->output_packet (mux, buf, new_pcr);
}

/* called when TsMux needs new packet to write into */
static void
alloc_packet_cb (GstBuffer ** buf, void *user_data)
{
  GstBaseTsMux *mux = (GstBaseTsMux *) user_data;
  GstBaseTsMuxClass *klass = GST_BASE_TS_MUX_GET_CLASS (mux);

  g_assert (klass->allocate_packet);

  klass->allocate_packet (mux, buf);
}

static GstFlowReturn
gst_base_ts_mux_aggregate_buffer (GstBaseTsMux * mux,
    GstAggregatorPad * agg_pad, GstBuffer * buf)
{
  GstFlowReturn ret = GST_FLOW_OK;
  GstBaseTsMuxPad *best = GST_BASE_TS_MUX_PAD (agg_pad);
  TsMuxProgram *prog;
  gint64 pts = GST_CLOCK_STIME_NONE;
  gint64 dts = GST_CLOCK_STIME_NONE;
  gboolean delta = TRUE, header = FALSE;
  StreamData *stream_data;
  GstMpegtsSection *scte_section = NULL;

  GST_DEBUG_OBJECT (mux, "Pads collected");

  if (buf && gst_buffer_get_size (buf) == 0
      && GST_BUFFER_FLAG_IS_SET (buf, GST_BUFFER_FLAG_GAP)) {
    gst_buffer_unref (buf);
    return GST_FLOW_OK;
  }

  g_mutex_lock (&mux->lock);
  if (G_UNLIKELY (mux->first)) {
    ret = gst_base_ts_mux_create_streams (mux);
    if (G_UNLIKELY (ret != GST_FLOW_OK)) {
      if (buf)
        gst_buffer_unref (buf);
      g_mutex_unlock (&mux->lock);
      return ret;
    }

    mux->first = FALSE;
  }

  prog = best->prog;
  if (prog == NULL) {
    GList *cur;

    ret = gst_base_ts_mux_create_pad_stream (mux, GST_PAD (best), FALSE);
    if (G_UNLIKELY (ret != GST_FLOW_OK)) {
      if (buf)
        gst_buffer_unref (buf);
      g_mutex_unlock (&mux->lock);
      return ret;
    }
    tsmux_resend_pat (mux->tsmux);
    tsmux_resend_si (mux->tsmux);
    prog = best->prog;
    g_assert_nonnull (prog);

    /* output PMT for each program */
    for (cur = mux->tsmux->programs; cur; cur = cur->next) {
      TsMuxProgram *program = (TsMuxProgram *) cur->data;

      tsmux_resend_pmt (program);
    }
  }

  g_assert (buf != NULL);

  if (best->prepare_func) {
    GstBuffer *tmp;

    tmp = best->prepare_func (buf, best, mux);
    g_assert (tmp);
    gst_buffer_unref (buf);
    buf = tmp;
  }

  if (mux->force_key_unit_event != NULL && best->stream->is_video_stream) {
    GstEvent *event;

    g_mutex_unlock (&mux->lock);
    event = check_pending_key_unit_event (mux->force_key_unit_event,
        &agg_pad->segment, GST_BUFFER_PTS (buf),
        GST_BUFFER_FLAGS (buf), mux->pending_key_unit_ts);
    if (event) {
      GstClockTime running_time;
      guint count;
      GList *cur;

      mux->pending_key_unit_ts = GST_CLOCK_TIME_NONE;
      gst_event_replace (&mux->force_key_unit_event, NULL);

      gst_video_event_parse_downstream_force_key_unit (event,
          NULL, NULL, &running_time, NULL, &count);

      GST_INFO_OBJECT (mux, "pushing downstream force-key-unit event %d "
          "%" GST_TIME_FORMAT " count %d", gst_event_get_seqnum (event),
          GST_TIME_ARGS (running_time), count);
      gst_pad_push_event (GST_AGGREGATOR_SRC_PAD (mux), event);

      g_mutex_lock (&mux->lock);
      /* output PAT, SI tables */
      tsmux_resend_pat (mux->tsmux);
      tsmux_resend_si (mux->tsmux);

      /* output PMT for each program */
      for (cur = mux->tsmux->programs; cur; cur = cur->next) {
        TsMuxProgram *program = (TsMuxProgram *) cur->data;

        tsmux_resend_pmt (program);
      }
    } else {
      g_mutex_lock (&mux->lock);
    }
  }

  if (!prog->pcr_pid && G_UNLIKELY (prog->pcr_stream == NULL)) {
    /* Take the first data stream for the PCR */
    GST_DEBUG_OBJECT (best,
        "Use stream (pid=%d) from pad as PCR for program (prog_id = %d)",
        best->pid, best->prog_id);

    /* Set the chosen PCR stream */
    tsmux_program_set_pcr_stream (prog, best->stream);
  }

  GST_DEBUG_OBJECT (best, "Chose stream for output (PID: 0x%04x)", best->pid);

  GST_OBJECT_LOCK (mux);
  scte_section = mux->pending_scte35_section;
  mux->pending_scte35_section = NULL;
  GST_OBJECT_UNLOCK (mux);
  if (G_UNLIKELY (scte_section)) {
    GST_DEBUG_OBJECT (mux, "Sending pending SCTE section");
    if (!tsmux_send_section (mux->tsmux, scte_section))
      GST_ERROR_OBJECT (mux, "Error sending SCTE section !");
  }

  if (GST_CLOCK_TIME_IS_VALID (GST_BUFFER_PTS (buf))) {
    pts = GSTTIME_TO_MPEGTIME (GST_BUFFER_PTS (buf));
    GST_DEBUG_OBJECT (mux, "Buffer has PTS  %" GST_TIME_FORMAT " pts %"
        G_GINT64_FORMAT "%s", GST_TIME_ARGS (GST_BUFFER_PTS (buf)), pts,
        !GST_BUFFER_FLAG_IS_SET (buf,
            GST_BUFFER_FLAG_DELTA_UNIT) ? " (keyframe)" : "");
  }

  if (GST_CLOCK_STIME_IS_VALID (best->dts)) {
    dts = GSTTIME_TO_MPEGTIME (best->dts);
    GST_DEBUG_OBJECT (mux, "Buffer has DTS %" GST_STIME_FORMAT " dts %"
        G_GINT64_FORMAT, GST_STIME_ARGS (best->dts), dts);
  }

  /* should not have a DTS without PTS */
  if (!GST_CLOCK_STIME_IS_VALID (pts) && GST_CLOCK_STIME_IS_VALID (dts)) {
    GST_DEBUG_OBJECT (mux, "using DTS for unknown PTS");
    pts = dts;
  }

  if (best->stream->is_video_stream) {
    delta = GST_BUFFER_FLAG_IS_SET (buf, GST_BUFFER_FLAG_DELTA_UNIT);
    header = GST_BUFFER_FLAG_IS_SET (buf, GST_BUFFER_FLAG_HEADER);
  }

  if (best->stream->is_meta && gst_buffer_get_size (buf) > (G_MAXUINT16 - 3)) {
    GST_WARNING_OBJECT (mux, "KLV meta unit too big, splitting not supported");

    gst_buffer_unref (buf);
    g_mutex_unlock (&mux->lock);
    return GST_FLOW_OK;
  }

  GST_DEBUG_OBJECT (mux, "delta: %d", delta);

  if (gst_buffer_get_size (buf) > 0) {
    stream_data = stream_data_new (buf);
    tsmux_stream_add_data (best->stream, stream_data->map_info.data,
        stream_data->map_info.size, stream_data, pts, dts, !delta);
  }

  /* outgoing ts follows ts of PCR program stream */
  if (prog->pcr_stream == best->stream) {
    /* prefer DTS if present for PCR as it should be monotone */
    mux->last_ts =
        GST_CLOCK_TIME_IS_VALID (GST_BUFFER_DTS (buf)) ?
        GST_BUFFER_DTS (buf) : GST_BUFFER_PTS (buf);
  }

  mux->is_delta = delta;
  mux->is_header = header;
  while (tsmux_stream_bytes_in_buffer (best->stream) > 0) {
    if (!tsmux_write_stream_packet (mux->tsmux, best->stream)) {
      /* Failed writing data for some reason. Set appropriate error */
      GST_DEBUG_OBJECT (mux, "Failed to write data packet");
      GST_ELEMENT_ERROR (mux, STREAM, MUX,
          ("Failed writing output data to stream %04x", best->stream->id),
          (NULL));
      goto write_fail;
    }
  }
  g_mutex_unlock (&mux->lock);
  /* flush packet cache */
  return gst_base_ts_mux_push_packets (mux, FALSE);

  /* ERRORS */
write_fail:
  {
    return mux->last_flow_ret;
  }
}

/* GstElement implementation */
static gboolean
gst_base_ts_mux_has_pad_with_pid (GstBaseTsMux * mux, guint16 pid)
{
  GList *l;
  gboolean res = FALSE;

  GST_OBJECT_LOCK (mux);

  for (l = GST_ELEMENT_CAST (mux)->sinkpads; l; l = l->next) {
    GstBaseTsMuxPad *tpad = GST_BASE_TS_MUX_PAD (l->data);

    if (tpad->pid == pid) {
      res = TRUE;
      break;
    }
  }

  GST_OBJECT_UNLOCK (mux);
  return res;
}

static GstPad *
gst_base_ts_mux_request_new_pad (GstElement * element, GstPadTemplate * templ,
    const gchar * name, const GstCaps * caps)
{
  GstBaseTsMux *mux = GST_BASE_TS_MUX (element);
  gint pid = -1;
  GstPad *pad = NULL;
  gchar *free_name = NULL;

  g_mutex_lock (&mux->lock);
  if (name != NULL && sscanf (name, "sink_%d", &pid) == 1) {
    if (tsmux_find_stream (mux->tsmux, pid)) {
      g_mutex_unlock (&mux->lock);
      goto stream_exists;
    }
    /* Make sure we don't use reserved PID.
     * FIXME : This should be extended to other variants (ex: ATSC) reserved PID */
    if (pid < TSMUX_START_ES_PID)
      goto invalid_stream_pid;
  } else {
    do {
      pid = tsmux_get_new_pid (mux->tsmux);
    } while (gst_base_ts_mux_has_pad_with_pid (mux, pid));

    /* Name the pad correctly after the selected pid */
    name = free_name = g_strdup_printf ("sink_%d", pid);
  }
  g_mutex_unlock (&mux->lock);

  pad = (GstPad *)
      GST_ELEMENT_CLASS (parent_class)->request_new_pad (element,
      templ, name, caps);

  gst_base_ts_mux_pad_reset (GST_BASE_TS_MUX_PAD (pad));
  GST_BASE_TS_MUX_PAD (pad)->pid = pid;

  g_free (free_name);

  return pad;

  /* ERRORS */
stream_exists:
  {
    GST_ELEMENT_ERROR (element, STREAM, MUX, ("Duplicate PID requested"),
        (NULL));
    return NULL;
  }

invalid_stream_pid:
  {
    GST_ELEMENT_ERROR (element, STREAM, MUX,
        ("Invalid Elementary stream PID (0x%02u < 0x40)", pid), (NULL));
    return NULL;
  }
}

static void
gst_base_ts_mux_release_pad (GstElement * element, GstPad * pad)
{
  GstBaseTsMux *mux = GST_BASE_TS_MUX (element);

  g_mutex_lock (&mux->lock);
  if (mux->tsmux) {
    GList *cur;
    GstBaseTsMuxPad *ts_pad = GST_BASE_TS_MUX_PAD (pad);
    gint pid = ts_pad->pid;

    if (ts_pad->prog) {
      if (ts_pad->prog->pcr_stream == ts_pad->stream) {
        tsmux_program_set_pcr_stream (ts_pad->prog, NULL);
      }
      if (tsmux_remove_stream (mux->tsmux, pid, ts_pad->prog)) {
        g_hash_table_remove (mux->programs, GINT_TO_POINTER (ts_pad->prog_id));
      }
    }

    tsmux_resend_pat (mux->tsmux);
    tsmux_resend_si (mux->tsmux);

    /* output PMT for each program */
    for (cur = mux->tsmux->programs; cur; cur = cur->next) {
      TsMuxProgram *program = (TsMuxProgram *) cur->data;

      tsmux_resend_pmt (program);
    }
  }
  g_mutex_unlock (&mux->lock);

  GST_ELEMENT_CLASS (parent_class)->release_pad (element, pad);
}

/* GstAggregator implementation */

static void
request_keyframe (GstBaseTsMux * mux, GstClockTime running_time)
{
  GList *l;
  GST_OBJECT_LOCK (mux);

  for (l = GST_ELEMENT_CAST (mux)->sinkpads; l; l = l->next) {
    gst_pad_push_event (GST_PAD (l->data),
        gst_video_event_new_upstream_force_key_unit (running_time, TRUE, 0));
  }

  GST_OBJECT_UNLOCK (mux);
}

static const guint32 crc_tab[256] = {
  0x00000000, 0x04c11db7, 0x09823b6e, 0x0d4326d9, 0x130476dc, 0x17c56b6b,
  0x1a864db2, 0x1e475005, 0x2608edb8, 0x22c9f00f, 0x2f8ad6d6, 0x2b4bcb61,
  0x350c9b64, 0x31cd86d3, 0x3c8ea00a, 0x384fbdbd, 0x4c11db70, 0x48d0c6c7,
  0x4593e01e, 0x4152fda9, 0x5f15adac, 0x5bd4b01b, 0x569796c2, 0x52568b75,
  0x6a1936c8, 0x6ed82b7f, 0x639b0da6, 0x675a1011, 0x791d4014, 0x7ddc5da3,
  0x709f7b7a, 0x745e66cd, 0x9823b6e0, 0x9ce2ab57, 0x91a18d8e, 0x95609039,
  0x8b27c03c, 0x8fe6dd8b, 0x82a5fb52, 0x8664e6e5, 0xbe2b5b58, 0xbaea46ef,
  0xb7a96036, 0xb3687d81, 0xad2f2d84, 0xa9ee3033, 0xa4ad16ea, 0xa06c0b5d,
  0xd4326d90, 0xd0f37027, 0xddb056fe, 0xd9714b49, 0xc7361b4c, 0xc3f706fb,
  0xceb42022, 0xca753d95, 0xf23a8028, 0xf6fb9d9f, 0xfbb8bb46, 0xff79a6f1,
  0xe13ef6f4, 0xe5ffeb43, 0xe8bccd9a, 0xec7dd02d, 0x34867077, 0x30476dc0,
  0x3d044b19, 0x39c556ae, 0x278206ab, 0x23431b1c, 0x2e003dc5, 0x2ac12072,
  0x128e9dcf, 0x164f8078, 0x1b0ca6a1, 0x1fcdbb16, 0x018aeb13, 0x054bf6a4,
  0x0808d07d, 0x0cc9cdca, 0x7897ab07, 0x7c56b6b0, 0x71159069, 0x75d48dde,
  0x6b93dddb, 0x6f52c06c, 0x6211e6b5, 0x66d0fb02, 0x5e9f46bf, 0x5a5e5b08,
  0x571d7dd1, 0x53dc6066, 0x4d9b3063, 0x495a2dd4, 0x44190b0d, 0x40d816ba,
  0xaca5c697, 0xa864db20, 0xa527fdf9, 0xa1e6e04e, 0xbfa1b04b, 0xbb60adfc,
  0xb6238b25, 0xb2e29692, 0x8aad2b2f, 0x8e6c3698, 0x832f1041, 0x87ee0df6,
  0x99a95df3, 0x9d684044, 0x902b669d, 0x94ea7b2a, 0xe0b41de7, 0xe4750050,
  0xe9362689, 0xedf73b3e, 0xf3b06b3b, 0xf771768c, 0xfa325055, 0xfef34de2,
  0xc6bcf05f, 0xc27dede8, 0xcf3ecb31, 0xcbffd686, 0xd5b88683, 0xd1799b34,
  0xdc3abded, 0xd8fba05a, 0x690ce0ee, 0x6dcdfd59, 0x608edb80, 0x644fc637,
  0x7a089632, 0x7ec98b85, 0x738aad5c, 0x774bb0eb, 0x4f040d56, 0x4bc510e1,
  0x46863638, 0x42472b8f, 0x5c007b8a, 0x58c1663d, 0x558240e4, 0x51435d53,
  0x251d3b9e, 0x21dc2629, 0x2c9f00f0, 0x285e1d47, 0x36194d42, 0x32d850f5,
  0x3f9b762c, 0x3b5a6b9b, 0x0315d626, 0x07d4cb91, 0x0a97ed48, 0x0e56f0ff,
  0x1011a0fa, 0x14d0bd4d, 0x19939b94, 0x1d528623, 0xf12f560e, 0xf5ee4bb9,
  0xf8ad6d60, 0xfc6c70d7, 0xe22b20d2, 0xe6ea3d65, 0xeba91bbc, 0xef68060b,
  0xd727bbb6, 0xd3e6a601, 0xdea580d8, 0xda649d6f, 0xc423cd6a, 0xc0e2d0dd,
  0xcda1f604, 0xc960ebb3, 0xbd3e8d7e, 0xb9ff90c9, 0xb4bcb610, 0xb07daba7,
  0xae3afba2, 0xaafbe615, 0xa7b8c0cc, 0xa379dd7b, 0x9b3660c6, 0x9ff77d71,
  0x92b45ba8, 0x9675461f, 0x8832161a, 0x8cf30bad, 0x81b02d74, 0x857130c3,
  0x5d8a9099, 0x594b8d2e, 0x5408abf7, 0x50c9b640, 0x4e8ee645, 0x4a4ffbf2,
  0x470cdd2b, 0x43cdc09c, 0x7b827d21, 0x7f436096, 0x7200464f, 0x76c15bf8,
  0x68860bfd, 0x6c47164a, 0x61043093, 0x65c52d24, 0x119b4be9, 0x155a565e,
  0x18197087, 0x1cd86d30, 0x029f3d35, 0x065e2082, 0x0b1d065b, 0x0fdc1bec,
  0x3793a651, 0x3352bbe6, 0x3e119d3f, 0x3ad08088, 0x2497d08d, 0x2056cd3a,
  0x2d15ebe3, 0x29d4f654, 0xc5a92679, 0xc1683bce, 0xcc2b1d17, 0xc8ea00a0,
  0xd6ad50a5, 0xd26c4d12, 0xdf2f6bcb, 0xdbee767c, 0xe3a1cbc1, 0xe760d676,
  0xea23f0af, 0xeee2ed18, 0xf0a5bd1d, 0xf464a0aa, 0xf9278673, 0xfde69bc4,
  0x89b8fd09, 0x8d79e0be, 0x803ac667, 0x84fbdbd0, 0x9abc8bd5, 0x9e7d9662,
  0x933eb0bb, 0x97ffad0c, 0xafb010b1, 0xab710d06, 0xa6322bdf, 0xa2f33668,
  0xbcb4666d, 0xb8757bda, 0xb5365d03, 0xb1f740b4
};

static guint32
_calc_crc32 (const guint8 * data, guint datalen)
{
  gint i;
  guint32 crc = 0xffffffff;

  for (i = 0; i < datalen; i++) {
    crc = (crc << 8) ^ crc_tab[((crc >> 24) ^ *data++) & 0xff];
  }
  return crc;
}

#define MPEGTIME_TO_GSTTIME(t) ((t) * (guint64)100000 / 9)

static GstMpegtsSCTESpliceEvent *
copy_splice (GstMpegtsSCTESpliceEvent * splice)
{
  return g_boxed_copy (GST_TYPE_MPEGTS_SCTE_SPLICE_EVENT, splice);
}

static void
free_splice (GstMpegtsSCTESpliceEvent * splice)
{
  g_boxed_free (GST_TYPE_MPEGTS_SCTE_SPLICE_EVENT, splice);
}

/* FIXME: get rid of this when depending on glib >= 2.62 */

static GPtrArray *
_g_ptr_array_copy (GPtrArray * array,
    GCopyFunc func, GFreeFunc free_func, gpointer user_data)
{
  GPtrArray *new_array;

  g_return_val_if_fail (array != NULL, NULL);

  new_array = g_ptr_array_new_with_free_func (free_func);

  g_ptr_array_set_size (new_array, array->len);

  if (func != NULL) {
    guint i;

    for (i = 0; i < array->len; i++)
      new_array->pdata[i] = func (array->pdata[i], user_data);
  } else if (array->len > 0) {
    memcpy (new_array->pdata, array->pdata,
        array->len * sizeof (*array->pdata));
  }

  new_array->len = array->len;

  return new_array;
}

static GstMpegtsSCTESIT *
deep_copy_sit (const GstMpegtsSCTESIT * sit)
{
  GstMpegtsSCTESIT *sit_copy = g_boxed_copy (GST_TYPE_MPEGTS_SCTE_SIT, sit);
  GPtrArray *splices_copy =
      _g_ptr_array_copy (sit_copy->splices, (GCopyFunc) copy_splice,
      (GFreeFunc) free_splice, NULL);

  g_ptr_array_unref (sit_copy->splices);
  sit_copy->splices = splices_copy;

  return sit_copy;
}

/* Takes ownership of @section.
 *
 * This function is a bit complex because the SCTE sections can
 * have various origins:
 *
 * * Sections created by the application with the gst_mpegts_scte_*_new()
 *   API. The splice times / durations contained by these are expressed
 *   in the GStreamer running time domain, and must be translated to
 *   our local PES time domain. In this case, we will packetize the section
 *   ourselves.
 *
 * * Sections passed through from tsdemux: this case is complicated as
 *   splice times in the incoming stream may be encrypted, with pts_adjustment
 *   being the only timing field guaranteed *not* to be encrypted. In this
 *   case, the original binary data (section->data) will be reinjected as is
 *   in the output stream, with pts_adjustment adjusted. tsdemux provides us
 *   with the pts_offset it introduces, the difference between the original
 *   PES PTSs and the running times it outputs.
 *
 * Additionally, in either of these cases when the splice times aren't encrypted
 * we want to make use of those to request keyframes. For the passthrough case,
 * as the splice times are left untouched tsdemux provides us with the running
 * times the section originally referred to. We cannot calculate it locally
 * because we would need to have access to the information that the timestamps
 * in the original PES domain have wrapped around, and how many times they have
 * done so. While we could probably make educated guesses, tsdemux (more specifically
 * mpegtspacketizer) already keeps track of that, and it seemed more logical to
 * perform the calculation there and forward it alongside the downstream events.
 *
 * Finally, while we can't request keyframes at splice points in the encrypted
 * case, if the input stream was compliant in that regard and no reencoding took
 * place the splice times will still match with valid splice points, it is up
 * to the application to ensure that that is the case.
 */
static void
handle_scte35_section (GstBaseTsMux * mux, GstEvent * event,
    GstMpegtsSection * section, guint64 mpeg_pts_offset,
    GstStructure * rtime_map)
{
  GstMpegtsSCTESIT *sit;
  guint i;
  gboolean forward = TRUE;
  guint64 pts_adjust;
  guint8 *section_data;
  guint8 *crc;
  gboolean translate = FALSE;

  sit = (GstMpegtsSCTESIT *) gst_mpegts_section_get_scte_sit (section);

  /* When the application injects manually constructed splice events,
   * their time domain is the GStreamer running time, we receive them
   * unpacketized and translate the fields in the SIT to local PTS.
   *
   * We make a copy of the SIT in order to make sure we can rewrite it.
   */
  if (sit->is_running_time) {
    sit = deep_copy_sit (sit);
    translate = TRUE;
  }

  switch (sit->splice_command_type) {
    case GST_MTS_SCTE_SPLICE_COMMAND_NULL:
      /* We implement heartbeating ourselves */
      forward = FALSE;
      break;
    case GST_MTS_SCTE_SPLICE_COMMAND_SCHEDULE:
      /* No need to request keyframes at this point, splice_insert
       * messages will precede the future splice points and we
       * can request keyframes then. Only translate if needed.
       */
      if (translate) {
        for (i = 0; i < sit->splices->len; i++) {
          GstMpegtsSCTESpliceEvent *sevent =
              g_ptr_array_index (sit->splices, i);

          if (sevent->program_splice_time_specified)
            sevent->program_splice_time =
                GSTTIME_TO_MPEGTIME (sevent->program_splice_time) +
                TS_MUX_CLOCK_BASE;

          if (sevent->duration_flag)
            sevent->break_duration =
                GSTTIME_TO_MPEGTIME (sevent->break_duration);
        }
      }
      break;
    case GST_MTS_SCTE_SPLICE_COMMAND_INSERT:
      /* We want keyframes at splice points */
      if (sit->fully_parsed && (rtime_map || translate)) {

        for (i = 0; i < sit->splices->len; i++) {
          guint64 running_time = GST_CLOCK_TIME_NONE;

          GstMpegtsSCTESpliceEvent *sevent =
              g_ptr_array_index (sit->splices, i);
          if (sevent->program_splice_time_specified) {
            if (rtime_map) {
              gchar *field_name = g_strdup_printf ("event-%u-splice-time",
                  sevent->splice_event_id);
              if (gst_structure_get_uint64 (rtime_map, field_name,
                      &running_time)) {
                GST_DEBUG_OBJECT (mux,
                    "Requesting keyframe for splice point at %" GST_TIME_FORMAT,
                    GST_TIME_ARGS (running_time));
                request_keyframe (mux, running_time);
              }
              g_free (field_name);
            } else {
              g_assert (translate == TRUE);
              running_time = sevent->program_splice_time;
              GST_DEBUG_OBJECT (mux,
                  "Requesting keyframe for splice point at %" GST_TIME_FORMAT,
                  GST_TIME_ARGS (running_time));
              request_keyframe (mux, running_time);
              sevent->program_splice_time =
                  GSTTIME_TO_MPEGTIME (running_time) + TS_MUX_CLOCK_BASE;
            }
          } else {
            GST_DEBUG_OBJECT (mux,
                "Requesting keyframe for immediate splice point");
            request_keyframe (mux, GST_CLOCK_TIME_NONE);
          }

          if (sevent->duration_flag) {
            if (translate) {
              sevent->break_duration =
                  GSTTIME_TO_MPEGTIME (sevent->break_duration);
            }

            /* Even if auto_return is FALSE, when a break_duration is specified it
             * is intended as a redundancy mechanism in case the follow-up
             * splice insert goes missing.
             *
             * Schedule a keyframe at that point (if we can calculate its position
             * accurately).
             */
            if (GST_CLOCK_TIME_IS_VALID (running_time)) {
              running_time += MPEGTIME_TO_GSTTIME (sevent->break_duration);
              GST_DEBUG_OBJECT (mux,
                  "Requesting keyframe for end of break at %" GST_TIME_FORMAT,
                  GST_TIME_ARGS (running_time));
              request_keyframe (mux, running_time);
            }
          }
        }
      }
      break;
    case GST_MTS_SCTE_SPLICE_COMMAND_TIME:{
      /* Adjust timestamps and potentially request keyframes */
      gboolean do_request_keyframes = FALSE;

      /* TODO: we can probably be a little more fine-tuned about determining
       * whether a keyframe is actually needed, but this at least takes care
       * of the requirement in 10.3.4 that a keyframe should not be created
       * when the signal contains only a time_descriptor.
       */
      if (sit->fully_parsed && (rtime_map || translate)) {
        for (i = 0; i < sit->descriptors->len; i++) {
          GstMpegtsDescriptor *descriptor =
              g_ptr_array_index (sit->descriptors, i);

          switch (descriptor->tag) {
            case GST_MTS_SCTE_DESC_AVAIL:
            case GST_MTS_SCTE_DESC_DTMF:
            case GST_MTS_SCTE_DESC_SEGMENTATION:
              do_request_keyframes = TRUE;
              break;
            case GST_MTS_SCTE_DESC_TIME:
            case GST_MTS_SCTE_DESC_AUDIO:
              break;
          }

          if (do_request_keyframes)
            break;
        }

        if (sit->splice_time_specified) {
          GstClockTime running_time = GST_CLOCK_TIME_NONE;

          if (rtime_map) {
            if (do_request_keyframes
                && gst_structure_get_uint64 (rtime_map, "splice-time",
                    &running_time)) {
              GST_DEBUG_OBJECT (mux,
                  "Requesting keyframe for time signal at %" GST_TIME_FORMAT,
                  GST_TIME_ARGS (running_time));
              request_keyframe (mux, running_time);
            }
          } else {
            g_assert (translate);
            running_time = sit->splice_time;
            sit->splice_time =
                GSTTIME_TO_MPEGTIME (running_time) + TS_MUX_CLOCK_BASE;
            if (do_request_keyframes) {
              GST_DEBUG_OBJECT (mux,
                  "Requesting keyframe for time signal at %" GST_TIME_FORMAT,
                  GST_TIME_ARGS (running_time));
              request_keyframe (mux, running_time);
            }
          }
        } else if (do_request_keyframes) {
          GST_DEBUG_OBJECT (mux,
              "Requesting keyframe for immediate time signal");
          request_keyframe (mux, GST_CLOCK_TIME_NONE);
        }
      }
      break;
    }
    case GST_MTS_SCTE_SPLICE_COMMAND_BANDWIDTH:
    case GST_MTS_SCTE_SPLICE_COMMAND_PRIVATE:
      /* Just let those go through untouched, none of our business */
      break;
    default:
      break;
  }

  if (!forward) {
    gst_mpegts_section_unref (section);
    return;
  }

  if (!translate) {
    g_assert (section->data);
    /* Calculate the final adjustment, as a sum of:
     * - The adjustment in the original packet
     * - The offset introduced between the original local PTS
     *   and the GStreamer PTS output by tsdemux
     * - Our own 1-hour offset
     */
    pts_adjust = sit->pts_adjustment + mpeg_pts_offset + TS_MUX_CLOCK_BASE;

    /* Account for offsets potentially introduced between the demuxer and us */
    pts_adjust +=
        GSTTIME_TO_MPEGTIME (gst_event_get_running_time_offset (event));

    pts_adjust &= 0x1ffffffff;
    section_data = g_memdup2 (section->data, section->section_length);
    section_data[4] |= pts_adjust >> 32;
    section_data[5] = pts_adjust >> 24;
    section_data[6] = pts_adjust >> 16;
    section_data[7] = pts_adjust >> 8;
    section_data[8] = pts_adjust;

    /* Now rewrite our checksum */
    crc = section_data + section->section_length - 4;
    GST_WRITE_UINT32_BE (crc, _calc_crc32 (section_data, crc - section_data));

    GST_OBJECT_LOCK (mux);
    GST_DEBUG_OBJECT (mux, "Storing SCTE section");
    if (mux->pending_scte35_section)
      gst_mpegts_section_unref (mux->pending_scte35_section);
    mux->pending_scte35_section =
        gst_mpegts_section_new (mux->scte35_pid, section_data,
        section->section_length);
    GST_OBJECT_UNLOCK (mux);

    gst_mpegts_section_unref (section);
  } else {
    GST_OBJECT_LOCK (mux);
    GST_DEBUG_OBJECT (mux, "Storing SCTE section");
    gst_mpegts_section_unref (section);
    if (mux->pending_scte35_section)
      gst_mpegts_section_unref (mux->pending_scte35_section);
    mux->pending_scte35_section =
        gst_mpegts_section_from_scte_sit (sit, mux->scte35_pid);;
    GST_OBJECT_UNLOCK (mux);
  }
}

static gboolean
gst_base_ts_mux_send_event (GstElement * element, GstEvent * event)
{
  GstMpegtsSection *section;
  GstBaseTsMux *mux = GST_BASE_TS_MUX (element);

  section = gst_event_parse_mpegts_section (event);

  if (section) {
    GST_DEBUG ("Received event with mpegts section");

    if (section->section_type == GST_MPEGTS_SECTION_SCTE_SIT) {
      handle_scte35_section (mux, event, section, 0, NULL);
    } else {
      g_mutex_lock (&mux->lock);
      /* TODO: Check that the section type is supported */
      tsmux_add_mpegts_si_section (mux->tsmux, section);
      g_mutex_unlock (&mux->lock);
    }

    gst_event_unref (event);

    return TRUE;
  }

  return GST_ELEMENT_CLASS (parent_class)->send_event (element, event);
}

/* Must be called with mux->lock held */
static void
gst_base_ts_mux_resend_all_pmts (GstBaseTsMux * mux)
{
  GList *cur;

  /* output PMT for each program */
  for (cur = mux->tsmux->programs; cur; cur = cur->next) {
    TsMuxProgram *program = (TsMuxProgram *) cur->data;

    program->pmt_changed = TRUE;
    tsmux_resend_pmt (program);
  }
}

/* GstAggregator implementation */

static gboolean
gst_base_ts_mux_sink_event (GstAggregator * agg, GstAggregatorPad * agg_pad,
    GstEvent * event)
{
  GstAggregatorClass *agg_class = GST_AGGREGATOR_CLASS (parent_class);
  GstBaseTsMux *mux = GST_BASE_TS_MUX (agg);
  GstBaseTsMuxPad *ts_pad = GST_BASE_TS_MUX_PAD (agg_pad);
  gboolean res = FALSE;
  gboolean forward = TRUE;

  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_CAPS:
    {
      GstCaps *caps;
      GstFlowReturn ret;

      g_mutex_lock (&mux->lock);
      if (ts_pad->stream == NULL) {
        g_mutex_unlock (&mux->lock);
        break;
      }

      forward = FALSE;

      gst_event_parse_caps (event, &caps);
      if (!caps || !gst_caps_is_fixed (caps)) {
        g_mutex_unlock (&mux->lock);
        break;
      }

      ret = gst_base_ts_mux_create_or_update_stream (mux, ts_pad, caps);
      if (ret != GST_FLOW_OK) {
        g_mutex_unlock (&mux->lock);
        break;
      }

      mux->tsmux->pat_changed = TRUE;
      mux->tsmux->si_changed = TRUE;
      tsmux_resend_pat (mux->tsmux);
      tsmux_resend_si (mux->tsmux);
      gst_base_ts_mux_resend_all_pmts (mux);

      g_mutex_unlock (&mux->lock);

      res = TRUE;
      break;
    }
    case GST_EVENT_CUSTOM_DOWNSTREAM:
    {
      GstClockTime timestamp, stream_time, running_time;
      gboolean all_headers;
      guint count;
      const GstStructure *s;

      s = gst_event_get_structure (event);

      if (gst_structure_has_name (s, "scte-sit") && mux->scte35_pid != 0) {

        /* When operating downstream of tsdemux, tsdemux will send out events
         * on all its source pads for each splice table it encounters. If we
         * are remuxing multiple streams it has demuxed, this means we could
         * unnecessarily repeat the same table multiple times, we avoid that
         * by deduplicating thanks to the event sequm
         */
        if (gst_event_get_seqnum (event) != mux->last_scte35_event_seqnum) {
          GstMpegtsSection *section;

          gst_structure_get (s, "section", GST_TYPE_MPEGTS_SECTION, &section,
              NULL);
          if (section) {
            guint64 mpeg_pts_offset = 0;
            GstStructure *rtime_map = NULL;

            gst_structure_get (s, "running-time-map", GST_TYPE_STRUCTURE,
                &rtime_map, NULL);
            gst_structure_get_uint64 (s, "mpeg-pts-offset", &mpeg_pts_offset);

            handle_scte35_section (mux, event, section, mpeg_pts_offset,
                rtime_map);
            if (rtime_map)
              gst_structure_free (rtime_map);
            mux->last_scte35_event_seqnum = gst_event_get_seqnum (event);
          } else {
            GST_WARNING_OBJECT (ts_pad,
                "Ignoring scte-sit event without a section");
          }
        } else {
          GST_DEBUG_OBJECT (ts_pad, "Ignoring duplicate scte-sit event");
        }
        res = TRUE;
        forward = FALSE;
        goto out;
      }

      if (!gst_video_event_is_force_key_unit (event))
        goto out;

      res = TRUE;
      forward = FALSE;

      gst_video_event_parse_downstream_force_key_unit (event,
          &timestamp, &stream_time, &running_time, &all_headers, &count);
      GST_INFO_OBJECT (ts_pad, "have downstream force-key-unit event, "
          "seqnum %d, running-time %" GST_TIME_FORMAT " count %d",
          gst_event_get_seqnum (event), GST_TIME_ARGS (running_time), count);

      if (mux->force_key_unit_event != NULL) {
        GST_INFO_OBJECT (mux, "skipping downstream force key unit event "
            "as an upstream force key unit is already queued");
        goto out;
      }

      if (!all_headers)
        goto out;

      mux->pending_key_unit_ts = running_time;
      gst_event_replace (&mux->force_key_unit_event, event);
      break;
    }
    case GST_EVENT_TAG:{
      GstTagList *list;
      gchar *lang = NULL;
      guint bitrate = 0;
      guint max_bitrate = 0;

      GST_DEBUG_OBJECT (mux, "received tag event");
      gst_event_parse_tag (event, &list);

      /* MPEG wants ISO 639-2T code, taglist most likely contains 639-1 */
      if (gst_tag_list_get_string (list, GST_TAG_LANGUAGE_CODE, &lang)) {
        const gchar *lang_code;

        lang_code = gst_tag_get_language_code_iso_639_2T (lang);
        if (lang_code) {

          g_mutex_lock (&mux->lock);
          if (g_strcmp0 (ts_pad->language, lang_code) != 0) {
            GST_DEBUG_OBJECT (ts_pad, "Setting language to '%s'", lang_code);

            g_free (ts_pad->language);
            ts_pad->language = g_strdup (lang_code);
            if (ts_pad->stream) {
              strncpy (ts_pad->stream->language, lang_code, 3);
              ts_pad->stream->language[3] = 0;
              gst_base_ts_mux_resend_all_pmts (mux);
            }
          }
          g_mutex_unlock (&mux->lock);
        } else {
          GST_WARNING_OBJECT (ts_pad, "Did not get language code for '%s'",
              lang);
        }
        g_free (lang);
      }

      if (gst_tag_list_get_uint (list, GST_TAG_BITRATE, &bitrate)) {
        ts_pad->bitrate = bitrate;
      }

      if (gst_tag_list_get_uint (list, GST_TAG_MAXIMUM_BITRATE, &max_bitrate)) {
        ts_pad->max_bitrate = bitrate;
      }

      /* handled this, don't want collectpads to forward it downstream */
      res = TRUE;
      forward = gst_tag_list_get_scope (list) == GST_TAG_SCOPE_GLOBAL;
      break;
    }
    case GST_EVENT_STREAM_START:{
      GstStreamFlags flags;

      gst_event_parse_stream_flags (event, &flags);

      /* Don't wait for data on sparse inputs like metadata streams */
      /*
         if ((flags & GST_STREAM_FLAG_SPARSE)) {
         GST_COLLECT_PADS_STATE_UNSET (data, GST_COLLECT_PADS_STATE_LOCKED);
         gst_collect_pads_set_waiting (pads, data, FALSE);
         GST_COLLECT_PADS_STATE_SET (data, GST_COLLECT_PADS_STATE_LOCKED);
         }
       */
      break;
    }
    default:
      break;
  }

out:
  if (!forward)
    gst_event_unref (event);
  else
    res = agg_class->sink_event (agg, agg_pad, event);

  return res;
}

static gboolean
gst_base_ts_mux_src_event (GstAggregator * agg, GstEvent * event)
{
  GstAggregatorClass *agg_class = GST_AGGREGATOR_CLASS (parent_class);
  GstBaseTsMux *mux = GST_BASE_TS_MUX (agg);
  gboolean res = TRUE, forward = TRUE;

  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_CUSTOM_UPSTREAM:
    {
      GstIterator *iter;
      GValue sinkpad_value = G_VALUE_INIT;
      GstClockTime running_time;
      gboolean all_headers, done = FALSE;
      guint count;

      if (!gst_video_event_is_force_key_unit (event))
        break;

      forward = FALSE;

      gst_video_event_parse_upstream_force_key_unit (event,
          &running_time, &all_headers, &count);

      GST_INFO_OBJECT (mux, "received upstream force-key-unit event, "
          "seqnum %d running_time %" GST_TIME_FORMAT " all_headers %d count %d",
          gst_event_get_seqnum (event), GST_TIME_ARGS (running_time),
          all_headers, count);

      if (!all_headers)
        break;

      mux->pending_key_unit_ts = running_time;
      gst_event_replace (&mux->force_key_unit_event, event);

      iter = gst_element_iterate_sink_pads (GST_ELEMENT_CAST (mux));

      while (!done) {
        switch (gst_iterator_next (iter, &sinkpad_value)) {
          case GST_ITERATOR_OK:{
            GstPad *sinkpad = g_value_get_object (&sinkpad_value);
            gboolean tmp;

            GST_INFO_OBJECT (GST_AGGREGATOR_SRC_PAD (agg), "forwarding");
            tmp = gst_pad_push_event (sinkpad, gst_event_ref (event));
            GST_INFO_OBJECT (mux, "result %d", tmp);
            /* succeed if at least one pad succeeds */
            res |= tmp;
            break;
          }
          case GST_ITERATOR_DONE:
            done = TRUE;
            break;
          case GST_ITERATOR_RESYNC:
            gst_iterator_resync (iter);
            break;
          case GST_ITERATOR_ERROR:
            g_assert_not_reached ();
            break;
        }
        g_value_reset (&sinkpad_value);
      }
      g_value_unset (&sinkpad_value);
      gst_iterator_free (iter);
      break;
    }
    default:
      break;
  }

  if (forward)
    res = agg_class->src_event (agg, event);
  else
    gst_event_unref (event);

  return res;
}

static GstBuffer *
gst_base_ts_mux_clip (GstAggregator * agg,
    GstAggregatorPad * agg_pad, GstBuffer * buf)
{
  GstBaseTsMuxPad *pad = GST_BASE_TS_MUX_PAD (agg_pad);
  GstClockTime time;
  GstBuffer *ret;

  ret = buf;

  /* PTS */
  time = GST_BUFFER_PTS (buf);

  /* invalid left alone and passed */
  if (G_LIKELY (GST_CLOCK_TIME_IS_VALID (time))) {
    time =
        gst_segment_to_running_time (&agg_pad->segment, GST_FORMAT_TIME, time);
    if (G_UNLIKELY (!GST_CLOCK_TIME_IS_VALID (time))) {
      GST_DEBUG_OBJECT (pad, "clipping buffer on pad outside segment");
      gst_buffer_unref (buf);
      ret = NULL;
      goto beach;
    } else {
      GST_LOG_OBJECT (pad, "buffer pts %" GST_TIME_FORMAT " ->  %"
          GST_TIME_FORMAT " running time",
          GST_TIME_ARGS (GST_BUFFER_PTS (buf)), GST_TIME_ARGS (time));
      buf = ret = gst_buffer_make_writable (buf);
      GST_BUFFER_PTS (ret) = time;
    }
  }

  /* DTS */
  time = GST_BUFFER_DTS (buf);

  /* invalid left alone and passed */
  if (G_LIKELY (GST_CLOCK_TIME_IS_VALID (time))) {
    gint sign;
    gint64 dts;

    sign = gst_segment_to_running_time_full (&agg_pad->segment, GST_FORMAT_TIME,
        time, &time);

    if (sign > 0)
      dts = (gint64) time;
    else
      dts = -((gint64) time);

    GST_LOG_OBJECT (pad, "buffer dts %" GST_TIME_FORMAT " -> %"
        GST_STIME_FORMAT " running time", GST_TIME_ARGS (GST_BUFFER_DTS (buf)),
        GST_STIME_ARGS (dts));

    if (GST_CLOCK_STIME_IS_VALID (pad->dts) && dts < pad->dts) {
      /* Ignore DTS going backward */
      GST_WARNING_OBJECT (pad, "ignoring DTS going backward");
      dts = pad->dts;
    }

    ret = gst_buffer_make_writable (buf);
    if (sign > 0)
      GST_BUFFER_DTS (ret) = time;
    else
      GST_BUFFER_DTS (ret) = GST_CLOCK_TIME_NONE;

    pad->dts = dts;
  } else {
    pad->dts = GST_CLOCK_STIME_NONE;
  }

beach:
  return ret;
}

static GstBaseTsMuxPad *
gst_base_ts_mux_find_best_pad (GstAggregator * aggregator)
{
  GstBaseTsMuxPad *best = NULL;
  GstClockTime best_ts = GST_CLOCK_TIME_NONE;
  GList *l;

  GST_OBJECT_LOCK (aggregator);

  for (l = GST_ELEMENT_CAST (aggregator)->sinkpads; l; l = l->next) {
    GstBaseTsMuxPad *tpad = GST_BASE_TS_MUX_PAD (l->data);
    GstAggregatorPad *apad = GST_AGGREGATOR_PAD_CAST (tpad);
    GstBuffer *buffer;

    buffer = gst_aggregator_pad_peek_buffer (apad);
    if (!buffer)
      continue;
    if (best_ts == GST_CLOCK_TIME_NONE) {
      best = tpad;
      best_ts = GST_BUFFER_DTS_OR_PTS (buffer);
    } else if (GST_BUFFER_DTS_OR_PTS (buffer) != GST_CLOCK_TIME_NONE) {
      GstClockTime t = GST_BUFFER_DTS_OR_PTS (buffer);
      if (t < best_ts) {
        best = tpad;
        best_ts = t;
      }
    }
    gst_buffer_unref (buffer);
  }

  if (best)
    gst_object_ref (best);

  GST_OBJECT_UNLOCK (aggregator);

  GST_DEBUG_OBJECT (aggregator,
      "Best pad found with %" GST_TIME_FORMAT ": %" GST_PTR_FORMAT,
      GST_TIME_ARGS (best_ts), best);

  return best;
}

static gboolean
gst_base_ts_mux_are_all_pads_eos (GstBaseTsMux * mux)
{
  GList *l;
  gboolean ret = TRUE;

  GST_OBJECT_LOCK (mux);

  for (l = GST_ELEMENT_CAST (mux)->sinkpads; l; l = l->next) {
    GstBaseTsMuxPad *pad = GST_BASE_TS_MUX_PAD (l->data);

    if (!gst_aggregator_pad_is_eos (GST_AGGREGATOR_PAD (pad))) {
      ret = FALSE;
      break;
    }
  }

  GST_OBJECT_UNLOCK (mux);

  return ret;
}


static GstFlowReturn
gst_base_ts_mux_aggregate (GstAggregator * agg, gboolean timeout)
{
  GstBaseTsMux *mux = GST_BASE_TS_MUX (agg);
  GstFlowReturn ret = GST_FLOW_OK;
  GstBaseTsMuxPad *best = gst_base_ts_mux_find_best_pad (agg);
  GstCaps *caps;

  /* set caps on the srcpad if no caps were set yet */
  if (!(caps = gst_pad_get_current_caps (agg->srcpad))) {
    GstStructure *structure;

    caps = gst_pad_get_pad_template_caps (GST_AGGREGATOR_SRC_PAD (mux));
    caps = gst_caps_make_writable (caps);
    structure = gst_caps_get_structure (caps, 0);
    gst_structure_set (structure, "packetsize", G_TYPE_INT, mux->packet_size,
        NULL);

    gst_aggregator_set_src_caps (GST_AGGREGATOR (mux), caps);
  }
  gst_caps_unref (caps);

  if (best) {
    GstBuffer *buffer;

    buffer = gst_aggregator_pad_pop_buffer (GST_AGGREGATOR_PAD (best));
    if (!buffer) {
      /* We might have gotten a flush event after we picked the pad */
      goto done;
    }

    ret =
        gst_base_ts_mux_aggregate_buffer (GST_BASE_TS_MUX (agg),
        GST_AGGREGATOR_PAD (best), buffer);

    gst_object_unref (best);

    if (ret != GST_FLOW_OK)
      goto done;
  }

  if (gst_base_ts_mux_are_all_pads_eos (mux)) {
    GstBaseTsMuxClass *klass = GST_BASE_TS_MUX_GET_CLASS (mux);
    /* drain some possibly cached data */
    if (klass->drain)
      klass->drain (mux);
    gst_base_ts_mux_push_packets (mux, TRUE);

    ret = GST_FLOW_EOS;
  }

done:
  return ret;
}

static gboolean
gst_base_ts_mux_start (GstAggregator * agg)
{
  GstBaseTsMux *mux = GST_BASE_TS_MUX (agg);

  g_mutex_lock (&mux->lock);
  gst_base_ts_mux_reset (mux, TRUE);
  g_mutex_unlock (&mux->lock);

  return TRUE;
}

static gboolean
gst_base_ts_mux_stop (GstAggregator * agg)
{
  GstBaseTsMux *mux = GST_BASE_TS_MUX (agg);

  g_mutex_lock (&mux->lock);
  gst_base_ts_mux_reset (GST_BASE_TS_MUX (agg), TRUE);
  g_mutex_unlock (&mux->lock);

  return TRUE;
}

/* GObject implementation */

static void
gst_base_ts_mux_dispose (GObject * object)
{
  GstBaseTsMux *mux = GST_BASE_TS_MUX (object);

  g_mutex_lock (&mux->lock);
  gst_base_ts_mux_reset (mux, FALSE);

  if (mux->out_adapter) {
    g_object_unref (mux->out_adapter);
    mux->out_adapter = NULL;
  }
  if (mux->prog_map) {
    gst_structure_free (mux->prog_map);
    mux->prog_map = NULL;
  }
  if (mux->programs) {
    g_hash_table_destroy (mux->programs);
    mux->programs = NULL;
  }
  g_mutex_unlock (&mux->lock);
  GST_CALL_PARENT (G_OBJECT_CLASS, dispose, (object));
}

static void
gst_base_ts_mux_finalize (GObject * object)
{
  GstBaseTsMux *mux = GST_BASE_TS_MUX (object);

  g_mutex_clear (&mux->lock);
  GST_CALL_PARENT (G_OBJECT_CLASS, finalize, (object));
}

static void
gst_base_ts_mux_constructed (GObject * object)
{
  GstBaseTsMux *mux = GST_BASE_TS_MUX (object);

  /* initial state */
  g_mutex_lock (&mux->lock);
  gst_base_ts_mux_reset (mux, TRUE);
  g_mutex_unlock (&mux->lock);
}

static void
gst_base_ts_mux_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstBaseTsMux *mux = GST_BASE_TS_MUX (object);
  GList *l;

  switch (prop_id) {
    case PROP_PROG_MAP:
    {
      const GstStructure *s = gst_value_get_structure (value);
      if (mux->prog_map) {
        gst_structure_free (mux->prog_map);
      }
      if (s)
        mux->prog_map = gst_structure_copy (s);
      else
        mux->prog_map = NULL;
      break;
    }
    case PROP_PAT_INTERVAL:
      mux->pat_interval = g_value_get_uint (value);
      g_mutex_lock (&mux->lock);
      if (mux->tsmux)
        tsmux_set_pat_interval (mux->tsmux, mux->pat_interval);
      g_mutex_unlock (&mux->lock);
      break;
    case PROP_PMT_INTERVAL:
      mux->pmt_interval = g_value_get_uint (value);
      GST_OBJECT_LOCK (mux);
      for (l = GST_ELEMENT_CAST (mux)->sinkpads; l; l = l->next) {
        GstBaseTsMuxPad *ts_pad = GST_BASE_TS_MUX_PAD (l->data);

        g_mutex_lock (&mux->lock);
        tsmux_set_pmt_interval (ts_pad->prog, mux->pmt_interval);
        g_mutex_unlock (&mux->lock);
      }
      GST_OBJECT_UNLOCK (mux);
      break;
    case PROP_ALIGNMENT:
      mux->alignment = g_value_get_int (value);
      break;
    case PROP_SI_INTERVAL:
      mux->si_interval = g_value_get_uint (value);
      g_mutex_lock (&mux->lock);
      tsmux_set_si_interval (mux->tsmux, mux->si_interval);
      g_mutex_unlock (&mux->lock);
      break;
    case PROP_BITRATE:
      mux->bitrate = g_value_get_uint64 (value);
      g_mutex_lock (&mux->lock);
      if (mux->tsmux)
        tsmux_set_bitrate (mux->tsmux, mux->bitrate);
      g_mutex_unlock (&mux->lock);
      break;
    case PROP_PCR_INTERVAL:
      mux->pcr_interval = g_value_get_uint (value);
      g_mutex_lock (&mux->lock);
      if (mux->tsmux)
        tsmux_set_pcr_interval (mux->tsmux, mux->pcr_interval);
      g_mutex_unlock (&mux->lock);
      break;
    case PROP_SCTE_35_PID:
      mux->scte35_pid = g_value_get_uint (value);
      break;
    case PROP_SCTE_35_NULL_INTERVAL:
      mux->scte35_null_interval = g_value_get_uint (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_base_ts_mux_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstBaseTsMux *mux = GST_BASE_TS_MUX (object);

  switch (prop_id) {
    case PROP_PROG_MAP:
      gst_value_set_structure (value, mux->prog_map);
      break;
    case PROP_PAT_INTERVAL:
      g_value_set_uint (value, mux->pat_interval);
      break;
    case PROP_PMT_INTERVAL:
      g_value_set_uint (value, mux->pmt_interval);
      break;
    case PROP_ALIGNMENT:
      g_value_set_int (value, mux->alignment);
      break;
    case PROP_SI_INTERVAL:
      g_value_set_uint (value, mux->si_interval);
      break;
    case PROP_BITRATE:
      g_value_set_uint64 (value, mux->bitrate);
      break;
    case PROP_PCR_INTERVAL:
      g_value_set_uint (value, mux->pcr_interval);
      break;
    case PROP_SCTE_35_PID:
      g_value_set_uint (value, mux->scte35_pid);
      break;
    case PROP_SCTE_35_NULL_INTERVAL:
      g_value_set_uint (value, mux->scte35_null_interval);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

/* Default vmethods implementation */

static TsMux *
gst_base_ts_mux_default_create_ts_mux (GstBaseTsMux * mux)
{
  TsMux *tsmux = tsmux_new ();
  tsmux_set_write_func (tsmux, new_packet_cb, mux);
  tsmux_set_alloc_func (tsmux, alloc_packet_cb, mux);
  tsmux_set_pat_interval (tsmux, mux->pat_interval);
  tsmux_set_si_interval (tsmux, mux->si_interval);
  tsmux_set_bitrate (tsmux, mux->bitrate);
  tsmux_set_pcr_interval (tsmux, mux->pcr_interval);

  return tsmux;
}

static void
gst_base_ts_mux_default_allocate_packet (GstBaseTsMux * mux,
    GstBuffer ** buffer)
{
  GstBuffer *buf;

  buf = gst_buffer_new_and_alloc (mux->packet_size);

  *buffer = buf;
}

static gboolean
gst_base_ts_mux_default_output_packet (GstBaseTsMux * mux, GstBuffer * buffer,
    gint64 new_pcr)
{
  gst_base_ts_mux_collect_packet (mux, buffer);

  return TRUE;
}

/* Subclass API */

void
gst_base_ts_mux_set_packet_size (GstBaseTsMux * mux, gsize size)
{
  mux->packet_size = size;
}

void
gst_base_ts_mux_set_automatic_alignment (GstBaseTsMux * mux, gsize alignment)
{
  mux->automatic_alignment = alignment;
}

static void
gst_base_ts_mux_class_init (GstBaseTsMuxClass * klass)
{
  GstElementClass *gstelement_class = GST_ELEMENT_CLASS (klass);
  GstAggregatorClass *gstagg_class = GST_AGGREGATOR_CLASS (klass);
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  GST_DEBUG_CATEGORY_INIT (gst_base_ts_mux_debug, "basetsmux", 0,
      "MPEG Transport Stream muxer");

  gst_element_class_set_static_metadata (gstelement_class,
      "MPEG Transport Stream Muxer", "Codec/Muxer",
      "Multiplexes media streams into an MPEG Transport Stream",
      "Fluendo <contact@fluendo.com>");

  gobject_class->set_property =
      GST_DEBUG_FUNCPTR (gst_base_ts_mux_set_property);
  gobject_class->get_property =
      GST_DEBUG_FUNCPTR (gst_base_ts_mux_get_property);
  gobject_class->dispose = gst_base_ts_mux_dispose;
  gobject_class->finalize = gst_base_ts_mux_finalize;
  gobject_class->constructed = gst_base_ts_mux_constructed;

  gstelement_class->request_new_pad = gst_base_ts_mux_request_new_pad;
  gstelement_class->release_pad = gst_base_ts_mux_release_pad;
  gstelement_class->send_event = gst_base_ts_mux_send_event;

  gstagg_class->negotiate = NULL;
  gstagg_class->aggregate = gst_base_ts_mux_aggregate;
  gstagg_class->clip = gst_base_ts_mux_clip;
  gstagg_class->sink_event = gst_base_ts_mux_sink_event;
  gstagg_class->src_event = gst_base_ts_mux_src_event;
  gstagg_class->start = gst_base_ts_mux_start;
  gstagg_class->stop = gst_base_ts_mux_stop;
  gstagg_class->get_next_time = gst_aggregator_simple_get_next_time;

  klass->create_ts_mux = gst_base_ts_mux_default_create_ts_mux;
  klass->allocate_packet = gst_base_ts_mux_default_allocate_packet;
  klass->output_packet = gst_base_ts_mux_default_output_packet;

  g_object_class_install_property (G_OBJECT_CLASS (klass), PROP_PROG_MAP,
      g_param_spec_boxed ("prog-map", "Program map",
          "A GstStructure specifies the mapping from elementary streams to programs",
          GST_TYPE_STRUCTURE,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  g_object_class_install_property (G_OBJECT_CLASS (klass), PROP_PAT_INTERVAL,
      g_param_spec_uint ("pat-interval", "PAT interval",
          "Set the interval (in ticks of the 90kHz clock) for writing out the PAT table",
          1, G_MAXUINT, TSMUX_DEFAULT_PAT_INTERVAL,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  g_object_class_install_property (G_OBJECT_CLASS (klass), PROP_PMT_INTERVAL,
      g_param_spec_uint ("pmt-interval", "PMT interval",
          "Set the interval (in ticks of the 90kHz clock) for writing out the PMT table",
          1, G_MAXUINT, TSMUX_DEFAULT_PMT_INTERVAL,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  g_object_class_install_property (G_OBJECT_CLASS (klass), PROP_ALIGNMENT,
      g_param_spec_int ("alignment", "packet alignment",
          "Number of packets per buffer (padded with dummy packets on EOS) "
          "(-1 = auto, 0 = all available packets, 7 for UDP streaming)",
          -1, G_MAXINT, BASETSMUX_DEFAULT_ALIGNMENT,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  g_object_class_install_property (G_OBJECT_CLASS (klass), PROP_SI_INTERVAL,
      g_param_spec_uint ("si-interval", "SI interval",
          "Set the interval (in ticks of the 90kHz clock) for writing out the Service"
          "Information tables", 1, G_MAXUINT, TSMUX_DEFAULT_SI_INTERVAL,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  g_object_class_install_property (G_OBJECT_CLASS (klass), PROP_BITRATE,
      g_param_spec_uint64 ("bitrate", "Bitrate (in bits per second)",
          "Set the target bitrate, will insert null packets as padding "
          " to achieve multiplex-wide constant bitrate (0 means no padding)",
          0, G_MAXUINT64, TSMUX_DEFAULT_BITRATE,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  g_object_class_install_property (G_OBJECT_CLASS (klass), PROP_PCR_INTERVAL,
      g_param_spec_uint ("pcr-interval", "PCR interval",
          "Set the interval (in ticks of the 90kHz clock) for writing PCR",
          1, G_MAXUINT, TSMUX_DEFAULT_PCR_INTERVAL,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  g_object_class_install_property (G_OBJECT_CLASS (klass), PROP_SCTE_35_PID,
      g_param_spec_uint ("scte-35-pid", "SCTE-35 PID",
          "PID to use for inserting SCTE-35 packets (0: unused)",
          0, G_MAXUINT, DEFAULT_SCTE_35_PID,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  g_object_class_install_property (G_OBJECT_CLASS (klass),
      PROP_SCTE_35_NULL_INTERVAL, g_param_spec_uint ("scte-35-null-interval",
          "SCTE-35 NULL packet interval",
          "Set the interval (in ticks of the 90kHz clock) for writing SCTE-35 NULL (heartbeat) packets."
          " (only valid if scte-35-pid is different from 0)", 1, G_MAXUINT,
          TSMUX_DEFAULT_SCTE_35_NULL_INTERVAL,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  gst_element_class_add_static_pad_template_with_gtype (gstelement_class,
      &gst_base_ts_mux_src_factory, GST_TYPE_AGGREGATOR_PAD);

  gst_type_mark_as_plugin_api (GST_TYPE_BASE_TS_MUX_PAD, 0);
}

static void
gst_base_ts_mux_init (GstBaseTsMux * mux)
{
  mux->out_adapter = gst_adapter_new ();

  /* properties */
  mux->pat_interval = TSMUX_DEFAULT_PAT_INTERVAL;
  mux->pmt_interval = TSMUX_DEFAULT_PMT_INTERVAL;
  mux->si_interval = TSMUX_DEFAULT_SI_INTERVAL;
  mux->pcr_interval = TSMUX_DEFAULT_PCR_INTERVAL;
  mux->prog_map = NULL;
  mux->alignment = BASETSMUX_DEFAULT_ALIGNMENT;
  mux->bitrate = TSMUX_DEFAULT_BITRATE;
  mux->scte35_pid = DEFAULT_SCTE_35_PID;
  mux->scte35_null_interval = TSMUX_DEFAULT_SCTE_35_NULL_INTERVAL;

  mux->packet_size = GST_BASE_TS_MUX_NORMAL_PACKET_LENGTH;
  mux->automatic_alignment = 0;

  g_mutex_init (&mux->lock);
}
