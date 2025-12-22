/* ATSC Transport Stream muxer
 * Copyright (C) 2019 Mathieu Duponchelle <mathieu@centricular.com>
 *
 * atscmux.c:
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
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gstatscmux.h"

GST_DEBUG_CATEGORY (gst_atsc_mux_debug);
#define GST_CAT_DEFAULT gst_atsc_mux_debug

G_DEFINE_TYPE (GstATSCMux, gst_atsc_mux, GST_TYPE_BASE_TS_MUX);
GST_ELEMENT_REGISTER_DEFINE (atscmux, "atscmux", GST_RANK_PRIMARY,
    gst_atsc_mux_get_type ());

#define parent_class gst_atsc_mux_parent_class
#define ATSCMUX_ST_PS_AUDIO_EAC3 0x87

static GstStaticPadTemplate gst_atsc_mux_src_factory =
GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("video/mpegts, "
        "systemstream = (boolean) true, " "packetsize = (int) 188 ")
    );

static GstStaticPadTemplate gst_atsc_mux_sink_factory =
    GST_STATIC_PAD_TEMPLATE ("sink_%d",
    GST_PAD_SINK,
    GST_PAD_REQUEST,
    GST_STATIC_CAPS ("video/mpeg, "
        "parsed = (boolean) TRUE, "
        "mpegversion = (int) 2, "
        "systemstream = (boolean) false; "
        "video/x-h264,stream-format=(string)byte-stream,"
        "alignment=(string){au, nal}; "
        "audio/x-ac3, framed = (boolean) TRUE;"
        "audio/x-eac3, framed = (boolean) TRUE;"));

/* Internals */

static void
gst_atsc_mux_stream_get_es_descrs (TsMuxStream * stream,
    GstMpegtsPMTStream * pmt_stream, gpointer user_data)
{
  GstMpegtsDescriptor *descriptor;

  if (stream->stream_type == ATSCMUX_ST_PS_AUDIO_EAC3) {
    guint8 add_info[4];
    guint8 *pos;

    pos = add_info;

    /* audio_stream_descriptor () | ATSC A/52-2018 Annex G
     *
     * descriptor_tag     8 uimsbf
     * descriptor_length  8 uimsbf
     * reserved           1 '1'
     * bsid_flag          1 bslbf
     * mainid_flag        1 bslbf
     * asvc_flag          1 bslbf
     * mixinfoexists      1 bslbf
     * substream1_flag    1 bslbf
     * substream2_flag    1 bslbf
     * substream3_flag    1 bslbf
     * reserved           1 '1'
     * full_service_flag  1 bslbf
     * audio_service_type 3 uimsbf
     * number_of_channels 3 uimsbf
     * [...]
     */

    *pos++ = 0xCC;
    *pos++ = 2;

    /* 1 bit reserved, all other flags unset */
    *pos++ = 0x80;

    /* 1 bit reserved,
     * 1 bit set for full_service_flag,
     * 3 bits hardcoded audio_service_type "Complete Main",
     * 3 bits number_of_channels
     */
    switch (stream->audio_channels) {
      case 1:
        *pos++ = 0xC0;          /* Mono */
        break;
      case 2:
        *pos++ = 0xC0 | 0x2;    /* 2-channel (stereo) */
        break;
      case 3:
      case 4:
      case 5:
        *pos++ = 0xC0 | 0x4;    /* Multichannel audio (> 2 channels; <= 3/2 + LFE channels) */
        break;
      case 6:
      default:
        *pos++ = 0xC0 | 0x5;    /* Multichannel audio(> 3/2 + LFE channels) */
    }

    descriptor = gst_mpegts_descriptor_from_registration ("EAC3", add_info, 4);
    g_ptr_array_add (pmt_stream->descriptors, descriptor);

    descriptor =
        gst_mpegts_descriptor_from_custom (GST_MTS_DESC_ATSC_EAC3, add_info, 4);
    g_ptr_array_add (pmt_stream->descriptors, descriptor);
  } else if (stream->stream_type == TSMUX_ST_PS_AUDIO_AC3) {
    int wr_size = 0;
    guint8 *add_info = NULL;
    guint8 data;
    guint bitrate;
    gboolean has_language;
    guint bitrates[20][2] = {
      {32000, 0x00}
      ,
      {40000, 0x01}
      ,
      {48000, 0x02}
      ,
      {56000, 0x03}
      ,
      {64000, 0x04}
      ,
      {80000, 0x05}
      ,
      {96000, 0x06}
      ,
      {112000, 0x07}
      ,
      {128000, 0x08}
      ,
      {160000, 0x09}
      ,
      {192000, 0x0A}
      ,
      {224000, 0x0B}
      ,
      {256000, 0x0C}
      ,
      {320000, 0x0D}
      ,
      {384000, 0x0E}
      ,
      {448000, 0x0F}
      ,
      {512000, 0x10}
      ,
      {576000, 0x11}
      ,
      {640000, 0x12}
    };
    gint i;
    guint bitrate_code = 0x12;
    GstByteWriter writer;
    gst_byte_writer_init_with_size (&writer, 7, FALSE);

    /* audio_stream_descriptor () | ATSC A/52-2001 Annex A
     *
     * descriptor_tag       8 uimsbf
     * descriptor_length    8 uimsbf
     * sample_rate_code     3 bslbf
     * bsid                 5 bslbf
     * bit_rate_code        6 bslbf
     * surround_mode        2 bslbf
     * bsmod                3 bslbf
     * num_channels         4 bslbf
     * full_svc             1 bslbf
     * langcod              8 bslbf
     * mainid               3 uimsbf
     * priority             2 bslbf
     * reserved             3 '111'
     * textlen              7 uimsbf
     * text_code            1 bslbf
     * text                 8*textlen bslbf
     * language_flag        1 bslbf
     * language_flag_2      1 bslbf
     * reserved             6 '111111'
     * language if flag     3*8 uimbsf
     * language_2 if flag_2 3*8 uimsbf
     */

    /* 3 bits sample_rate_code, 5 bits hardcoded bsid (default ver 8) */
    switch (stream->audio_sampling) {
      case 48000:
        data = 0x08;
        break;
      case 44100:
        data = 0x28;
        break;
      case 32000:
        data = 0x48;
        break;
      default:
        data = 0xE8;
        break;                  /* 48, 44.1 or 32 Khz */
    }
    gst_byte_writer_put_uint8 (&writer, data);

    /* 1 bit bit_rate_limit, 5 bits bit_rate_code, 2 bits suround_mode */
    bitrate = MAX (stream->audio_bitrate, stream->max_bitrate);
    for (i = 0; i < G_N_ELEMENTS (bitrates); i++) {
      if (bitrate < bitrates[i][0]) {
        break;
      }
      bitrate_code = bitrates[i][1];
    }
    data = bitrate_code << 2;
    data |= 0x80;               /* This is a maximum bitrate */
    gst_byte_writer_put_uint8 (&writer, data);

    /* 3 bits bsmod, 4 bits num_channels, 1 bit full_svc */
    switch (stream->audio_channels) {
      case 1:
        data = 0x01 << 1;
        break;                  /* 1/0 */
      case 2:
        data = 0x02 << 1;
        break;                  /* 2/0 */
      case 3:
        data = 0x0A << 1;
        break;                  /* <= 3 */
      case 4:
        data = 0x0B << 1;
        break;                  /* <= 4 */
      case 5:
        data = 0x0C << 1;
        break;                  /* <= 5 */
      case 6:
      default:
        data = 0x0D << 1;
        break;                  /* <= 6 */
    }
    data |= 0x01;               /* full_svc is hardcoded to 1 for now */
    gst_byte_writer_put_uint8 (&writer, data);

    /* deprecated langcod */
    data = 0xff;
    gst_byte_writer_put_uint8 (&writer, data);
    /* langcod2 skipped because num_channels > 0 (no dual mono) */
    /* 3 bits mainid, 2 bits priority, 3 bits reserved */
    data = 0x0f;
    gst_byte_writer_put_uint8 (&writer, data);
    /* 7 bits textlen, 1 bit text_code */
    data = 0x00;
    gst_byte_writer_put_uint8 (&writer, data);
    /* no text provided, jumping directly to language */

    has_language = (stream->language[0] != '\0');
    if (has_language) {
      data = 0xbf;
      gst_byte_writer_put_uint8 (&writer, data);
      gst_byte_writer_put_data (&writer, (guint8 *) stream->language, 3);
    } else {
      data = 0x3f;
      gst_byte_writer_put_uint8 (&writer, data);
    }

    descriptor = gst_mpegts_descriptor_from_registration ("AC-3", NULL, 0);
    g_ptr_array_add (pmt_stream->descriptors, descriptor);

    wr_size = gst_byte_writer_get_size (&writer);
    add_info = gst_byte_writer_reset_and_get_data (&writer);

    descriptor =
        gst_mpegts_descriptor_from_custom (GST_MTS_DESC_AC3_AUDIO_STREAM,
        add_info, wr_size);
    g_ptr_array_add (pmt_stream->descriptors, descriptor);
  } else {
    tsmux_stream_default_get_es_descrs (stream, pmt_stream);
  }
}

static TsMuxStream *
gst_atsc_mux_create_new_stream (guint16 new_pid, TsMuxStreamType stream_type,
    guint stream_number, gpointer user_data)
{
  TsMuxStream *ret = tsmux_stream_new (new_pid, stream_type, stream_number);

  if (stream_type == ATSCMUX_ST_PS_AUDIO_EAC3) {
    ret->id = 0xBD;
    ret->pi.flags |= TSMUX_PACKET_FLAG_PES_FULL_HEADER;
    ret->is_audio = TRUE;
  } else if (stream_type == TSMUX_ST_PS_AUDIO_AC3) {
    ret->id = 0xBD;
    ret->id_extended = 0;
  }

  tsmux_stream_set_get_es_descriptors_func (ret,
      gst_atsc_mux_stream_get_es_descrs, user_data);

  return ret;
}

/* GstBaseTsMux implementation */

static TsMux *
gst_atsc_mux_create_ts_mux (GstBaseTsMux * mpegtsmux)
{
  TsMux *ret = ((GstBaseTsMuxClass *) parent_class)->create_ts_mux (mpegtsmux);
  GstMpegtsAtscMGT *mgt;
  GstMpegtsAtscSTT *stt;
  GstMpegtsAtscRRT *rrt;
  GstMpegtsSection *section;

  mgt = gst_mpegts_atsc_mgt_new ();
  section = gst_mpegts_section_from_atsc_mgt (mgt);
  tsmux_add_mpegts_si_section (ret, section);

  stt = gst_mpegts_atsc_stt_new ();
  section = gst_mpegts_section_from_atsc_stt (stt);
  tsmux_add_mpegts_si_section (ret, section);

  rrt = gst_mpegts_atsc_rrt_new ();
  section = gst_mpegts_section_from_atsc_rrt (rrt);
  tsmux_add_mpegts_si_section (ret, section);

  tsmux_set_new_stream_func (ret, gst_atsc_mux_create_new_stream, mpegtsmux);

  return ret;
}

static guint
gst_atsc_mux_handle_media_type (GstBaseTsMux * mux, const gchar * media_type,
    GstBaseTsMuxPad * pad)
{
  guint ret = TSMUX_ST_RESERVED;

  if (!g_strcmp0 (media_type, "audio/x-eac3")) {
    ret = ATSCMUX_ST_PS_AUDIO_EAC3;
  }

  return ret;
}

static void
gst_atsc_mux_class_init (GstATSCMuxClass * klass)
{
  GstElementClass *gstelement_class = GST_ELEMENT_CLASS (klass);
  GstBaseTsMuxClass *mpegtsmux_class = (GstBaseTsMuxClass *) klass;

  GST_DEBUG_CATEGORY_INIT (gst_atsc_mux_debug, "atscmux", 0, "ATSC muxer");

  gst_element_class_set_static_metadata (gstelement_class,
      "ATSC Transport Stream Muxer", "Codec/Muxer",
      "Multiplexes media streams into an ATSC-compliant Transport Stream",
      "Mathieu Duponchelle <mathieu@centricular.com>");

  mpegtsmux_class->create_ts_mux = gst_atsc_mux_create_ts_mux;
  mpegtsmux_class->handle_media_type = gst_atsc_mux_handle_media_type;

  gst_element_class_add_static_pad_template_with_gtype (gstelement_class,
      &gst_atsc_mux_sink_factory, GST_TYPE_BASE_TS_MUX_PAD);

  gst_element_class_add_static_pad_template_with_gtype (gstelement_class,
      &gst_atsc_mux_src_factory, GST_TYPE_AGGREGATOR_PAD);
}

static void
gst_atsc_mux_init (GstATSCMux * mux)
{
}
