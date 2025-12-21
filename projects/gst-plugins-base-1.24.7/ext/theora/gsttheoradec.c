/* GStreamer
 * Copyright (C) 2004 Benjamin Otte <in7y118@public.uni-hamburg.de>
 * Copyright (c) 2012 Collabora Ltd.
 *	Author : Edward Hervey <edward@collabora.com>
 *      Author : Mark Nauwelaerts <mark.nauwelaerts@collabora.co.uk>
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
 * SECTION:element-theoradec
 * @title: theoradec
 * @see_also: theoraenc, oggdemux
 *
 * This element decodes theora streams into raw video
 * [Theora](http://www.theora.org/) is a royalty-free
 * video codec maintained by the [Xiph.org Foundation](http://www.xiph.org/),
 * based on the VP3 codec.
 *
 * ## Example pipeline
 * |[
 * gst-launch-1.0 -v filesrc location=videotestsrc.ogg ! oggdemux ! theoradec ! videoconvert ! videoscale ! autovideosink
 * ]|
 *  This example pipeline will decode an ogg stream and decodes the theora video in it.
 * Refer to the theoraenc example to create the ogg file.
 *
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include "gsttheoradec.h"
#include <gst/tag/tag.h>
#include <gst/video/video.h>
#include <gst/video/gstvideometa.h>
#include <gst/video/gstvideopool.h>

#define GST_CAT_DEFAULT theoradec_debug
GST_DEBUG_CATEGORY_STATIC (theoradec_debug);
GST_DEBUG_CATEGORY_STATIC (CAT_PERFORMANCE);

#define THEORA_DEF_TELEMETRY_MV 0
#define THEORA_DEF_TELEMETRY_MBMODE 0
#define THEORA_DEF_TELEMETRY_QI 0
#define THEORA_DEF_TELEMETRY_BITS 0

/* This was removed from the base class, this is used as a
   temporary return to signal the need to call _drop_frame,
   and does not leave theoraenc. */
#define GST_CUSTOM_FLOW_DROP GST_FLOW_CUSTOM_SUCCESS_1

enum
{
  PROP_0,
  PROP_TELEMETRY_MV,
  PROP_TELEMETRY_MBMODE,
  PROP_TELEMETRY_QI,
  PROP_TELEMETRY_BITS
};

static GstStaticPadTemplate theora_dec_src_factory =
GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("video/x-raw, "
        "format = (string) { I420, Y42B, Y444 }, "
        "framerate = (fraction) [0/1, MAX], "
        "width = (int) [ 1, MAX ], " "height = (int) [ 1, MAX ]")
    );

static GstStaticPadTemplate theora_dec_sink_factory =
GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("video/x-theora")
    );

#define gst_theora_dec_parent_class parent_class
G_DEFINE_TYPE (GstTheoraDec, gst_theora_dec, GST_TYPE_VIDEO_DECODER);
GST_ELEMENT_REGISTER_DEFINE (theoradec, "theoradec",
    GST_RANK_PRIMARY, GST_TYPE_THEORA_DEC);

static void theora_dec_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);
static void theora_dec_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);

static gboolean theora_dec_start (GstVideoDecoder * decoder);
static gboolean theora_dec_stop (GstVideoDecoder * decoder);
static gboolean theora_dec_set_format (GstVideoDecoder * decoder,
    GstVideoCodecState * state);
static gboolean theora_dec_flush (GstVideoDecoder * decoder);
static GstFlowReturn theora_dec_parse (GstVideoDecoder * decoder,
    GstVideoCodecFrame * frame, GstAdapter * adapter, gboolean at_eos);
static GstFlowReturn theora_dec_handle_frame (GstVideoDecoder * decoder,
    GstVideoCodecFrame * frame);

static GstFlowReturn theora_dec_decode_buffer (GstTheoraDec * dec,
    GstBuffer * buf, GstVideoCodecFrame * frame);

static gboolean
gst_theora_dec_ctl_is_supported (int req)
{
  /* should return TH_EFAULT or TH_EINVAL if supported, and TH_EIMPL if not */
  return (th_decode_ctl (NULL, req, NULL, 0) != TH_EIMPL);
}

static void
gst_theora_dec_class_init (GstTheoraDecClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GstElementClass *element_class = GST_ELEMENT_CLASS (klass);
  GstVideoDecoderClass *video_decoder_class = GST_VIDEO_DECODER_CLASS (klass);

  gobject_class->set_property = theora_dec_set_property;
  gobject_class->get_property = theora_dec_get_property;

  if (gst_theora_dec_ctl_is_supported (TH_DECCTL_SET_TELEMETRY_MV)) {
    g_object_class_install_property (gobject_class, PROP_TELEMETRY_MV,
        g_param_spec_int ("visualize-motion-vectors",
            "Visualize motion vectors",
            "Show motion vector selection overlaid on image. "
            "Value gives a mask for motion vector (MV) modes to show",
            0, 0xffff, THEORA_DEF_TELEMETRY_MV,
            G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  }

  if (gst_theora_dec_ctl_is_supported (TH_DECCTL_SET_TELEMETRY_MBMODE)) {
    g_object_class_install_property (gobject_class, PROP_TELEMETRY_MBMODE,
        g_param_spec_int ("visualize-macroblock-modes",
            "Visualize macroblock modes",
            "Show macroblock mode selection overlaid on image. "
            "Value gives a mask for macroblock (MB) modes to show",
            0, 0xffff, THEORA_DEF_TELEMETRY_MBMODE,
            G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  }

  if (gst_theora_dec_ctl_is_supported (TH_DECCTL_SET_TELEMETRY_QI)) {
    g_object_class_install_property (gobject_class, PROP_TELEMETRY_QI,
        g_param_spec_int ("visualize-quantization-modes",
            "Visualize adaptive quantization modes",
            "Show adaptive quantization mode selection overlaid on image. "
            "Value gives a mask for quantization (QI) modes to show",
            0, 0xffff, THEORA_DEF_TELEMETRY_QI,
            G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  }

  if (gst_theora_dec_ctl_is_supported (TH_DECCTL_SET_TELEMETRY_BITS)) {
    /* FIXME: make this a boolean instead? The value scales the bars so
     * they're less wide. Default is to use full width, and anything else
     * doesn't seem particularly useful, since the smaller bars just disappear
     * then (they almost disappear for a value of 2 already). */
    g_object_class_install_property (gobject_class, PROP_TELEMETRY_BITS,
        g_param_spec_int ("visualize-bit-usage",
            "Visualize bitstream usage breakdown",
            "Sets the bitstream breakdown visualization mode. "
            "Values influence the width of the bit usage bars to show",
            0, 0xff, THEORA_DEF_TELEMETRY_BITS,
            G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  }

  gst_element_class_add_static_pad_template (element_class,
      &theora_dec_src_factory);
  gst_element_class_add_static_pad_template (element_class,
      &theora_dec_sink_factory);
  gst_element_class_set_static_metadata (element_class, "Theora video decoder",
      "Codec/Decoder/Video", "decode raw theora streams to raw YUV video",
      "Benjamin Otte <otte@gnome.org>, Wim Taymans <wim@fluendo.com>");

  video_decoder_class->start = GST_DEBUG_FUNCPTR (theora_dec_start);
  video_decoder_class->stop = GST_DEBUG_FUNCPTR (theora_dec_stop);
  video_decoder_class->flush = GST_DEBUG_FUNCPTR (theora_dec_flush);
  video_decoder_class->set_format = GST_DEBUG_FUNCPTR (theora_dec_set_format);
  video_decoder_class->parse = GST_DEBUG_FUNCPTR (theora_dec_parse);
  video_decoder_class->handle_frame =
      GST_DEBUG_FUNCPTR (theora_dec_handle_frame);

  GST_DEBUG_CATEGORY_INIT (theoradec_debug, "theoradec", 0, "Theora decoder");
  GST_DEBUG_CATEGORY_GET (CAT_PERFORMANCE, "GST_PERFORMANCE");
}

static void
gst_theora_dec_init (GstTheoraDec * dec)
{
  dec->telemetry_mv = THEORA_DEF_TELEMETRY_MV;
  dec->telemetry_mbmode = THEORA_DEF_TELEMETRY_MBMODE;
  dec->telemetry_qi = THEORA_DEF_TELEMETRY_QI;
  dec->telemetry_bits = THEORA_DEF_TELEMETRY_BITS;

  /* input is packetized,
   * but is not marked that way so data gets parsed and keyframes marked */
  gst_video_decoder_set_packetized (GST_VIDEO_DECODER (dec), FALSE);
  gst_video_decoder_set_needs_format (GST_VIDEO_DECODER (dec), TRUE);

  gst_video_decoder_set_use_default_pad_acceptcaps (GST_VIDEO_DECODER_CAST
      (dec), TRUE);
  GST_PAD_SET_ACCEPT_TEMPLATE (GST_VIDEO_DECODER_SINK_PAD (dec));
}

static gboolean
theora_dec_start (GstVideoDecoder * decoder)
{
  GstTheoraDec *dec = GST_THEORA_DEC (decoder);

  GST_DEBUG_OBJECT (dec, "start");
  GST_DEBUG_OBJECT (dec, "Setting have_header to FALSE");
  dec->have_header = FALSE;

  return TRUE;
}

static gboolean
theora_dec_stop (GstVideoDecoder * decoder)
{
  GstTheoraDec *dec = GST_THEORA_DEC (decoder);

  GST_DEBUG_OBJECT (dec, "stop");

  th_info_clear (&dec->info);
  th_comment_clear (&dec->comment);
  if (dec->setup) {
    th_setup_free (dec->setup);
    dec->setup = NULL;
  }
  if (dec->decoder) {
    th_decode_free (dec->decoder);
    dec->decoder = NULL;
  }

  if (dec->input_state) {
    gst_video_codec_state_unref (dec->input_state);
    dec->input_state = NULL;
  }
  if (dec->output_state) {
    gst_video_codec_state_unref (dec->output_state);
    dec->output_state = NULL;
  }

  return TRUE;
}

static gboolean
theora_dec_flush (GstVideoDecoder * decoder)
{
  GstTheoraDec *dec = GST_THEORA_DEC (decoder);

  dec->need_keyframe = TRUE;

  return TRUE;
}

static GstFlowReturn
theora_dec_parse (GstVideoDecoder * decoder,
    GstVideoCodecFrame * frame, GstAdapter * adapter, gboolean at_eos)
{
  gint av;
  const guint8 *data;

  av = gst_adapter_available (adapter);

  if (av > 0) {
    data = gst_adapter_map (adapter, 1);
    /* check for keyframe; must not be header packet (0x80 | 0x40) */
    if (!(data[0] & 0xc0)) {
      GST_VIDEO_CODEC_FRAME_SET_SYNC_POINT (frame);
      GST_LOG_OBJECT (decoder, "Found keyframe");
    }
    gst_adapter_unmap (adapter);
  }

  /* and pass along all */
  gst_video_decoder_add_to_frame (decoder, av);
  return gst_video_decoder_have_frame (decoder);
}


static gboolean
theora_dec_set_format (GstVideoDecoder * bdec, GstVideoCodecState * state)
{
  GstTheoraDec *dec;

  dec = GST_THEORA_DEC (bdec);

  /* Keep a copy of the input state */
  if (dec->input_state)
    gst_video_codec_state_unref (dec->input_state);
  dec->input_state = gst_video_codec_state_ref (state);

  /* FIXME : Interesting, we always accept any kind of caps ? */
  if (state->codec_data) {
    GstBuffer *buffer;
    GstMapInfo minfo;
    guint8 *data;
    guint size;
    guint offset;

    buffer = state->codec_data;
    gst_buffer_map (buffer, &minfo, GST_MAP_READ);

    offset = 0;
    size = minfo.size;
    data = (guint8 *) minfo.data;

    while (size > 2) {
      guint psize;
      GstBuffer *buf;

      psize = (data[0] << 8) | data[1];
      /* skip header */
      data += 2;
      size -= 2;
      offset += 2;

      /* make sure we don't read too much */
      psize = MIN (psize, size);

      buf = gst_buffer_copy_region (buffer, GST_BUFFER_COPY_ALL, offset, psize);

      /* first buffer is a discont buffer */
      if (offset == 2)
        GST_BUFFER_FLAG_SET (buf, GST_BUFFER_FLAG_DISCONT);

      /* now feed it to the decoder we can ignore the error */
      theora_dec_decode_buffer (dec, buf, NULL);
      gst_buffer_unref (buf);

      /* skip the data */
      size -= psize;
      data += psize;
      offset += psize;
    }

    gst_buffer_unmap (buffer, &minfo);
  }

  GST_DEBUG_OBJECT (dec, "Done");

  return TRUE;
}

static GstFlowReturn
theora_handle_comment_packet (GstTheoraDec * dec, ogg_packet * packet)
{
  gchar *encoder = NULL;
  GstTagList *list;

  GST_DEBUG_OBJECT (dec, "parsing comment packet");

  list =
      gst_tag_list_from_vorbiscomment (packet->packet, packet->bytes,
      (guint8 *) "\201theora", 7, &encoder);

  if (!list) {
    GST_ERROR_OBJECT (dec, "couldn't decode comments");
    list = gst_tag_list_new_empty ();
  }
  if (encoder) {
    gst_tag_list_add (list, GST_TAG_MERGE_REPLACE,
        GST_TAG_ENCODER, encoder, NULL);
    g_free (encoder);
  }
  gst_tag_list_add (list, GST_TAG_MERGE_REPLACE,
      GST_TAG_ENCODER_VERSION, dec->info.version_major,
      GST_TAG_VIDEO_CODEC, "Theora", NULL);

  if (dec->info.target_bitrate > 0) {
    gst_tag_list_add (list, GST_TAG_MERGE_REPLACE,
        GST_TAG_BITRATE, dec->info.target_bitrate,
        GST_TAG_NOMINAL_BITRATE, dec->info.target_bitrate, NULL);
  }

  gst_video_decoder_merge_tags (GST_VIDEO_DECODER (dec),
      list, GST_TAG_MERGE_REPLACE);

  gst_tag_list_unref (list);

  return GST_FLOW_OK;
}

static GstFlowReturn
theora_handle_type_packet (GstTheoraDec * dec)
{
  gint par_num, par_den;
  GstFlowReturn ret = GST_FLOW_OK;
  GstVideoCodecState *state;
  GstVideoFormat fmt;
  gint width, height;

  GST_DEBUG_OBJECT (dec, "fps %d/%d, PAR %d/%d",
      dec->info.fps_numerator, dec->info.fps_denominator,
      dec->info.aspect_numerator, dec->info.aspect_denominator);

  /* calculate par
   * the info.aspect_* values reflect PAR;
   * 0:x and x:0 are allowed and can be interpreted as 1:1.
   */
  par_num = GST_VIDEO_INFO_PAR_N (&dec->input_state->info);
  par_den = GST_VIDEO_INFO_PAR_D (&dec->input_state->info);

  /* If we have a default PAR, see if the decoder specified a different one */
  if (par_num == 1 && par_den == 1 &&
      (dec->info.aspect_numerator != 0 && dec->info.aspect_denominator != 0)) {
    par_num = dec->info.aspect_numerator;
    par_den = dec->info.aspect_denominator;
  }
  /* theora has:
   *
   *  width/height : dimension of the encoded frame
   *  pic_width/pic_height : dimension of the visible part
   *  pic_x/pic_y : offset in encoded frame where visible part starts
   */
  GST_DEBUG_OBJECT (dec, "dimension %dx%d, PAR %d/%d", dec->info.pic_width,
      dec->info.pic_height, par_num, par_den);
  GST_DEBUG_OBJECT (dec, "frame dimension %dx%d, offset %d:%d",
      dec->info.pic_width, dec->info.pic_height,
      dec->info.pic_x, dec->info.pic_y);

  switch (dec->info.pixel_fmt) {
    case TH_PF_420:
      fmt = GST_VIDEO_FORMAT_I420;
      break;
    case TH_PF_422:
      fmt = GST_VIDEO_FORMAT_Y42B;
      break;
    case TH_PF_444:
      fmt = GST_VIDEO_FORMAT_Y444;
      break;
    default:
      goto unsupported_format;
  }

  width = dec->info.pic_width;
  height = dec->info.pic_height;

  /* Ensure correct offsets in chroma for formats that need it
   * by rounding the offset. libtheora will add proper pixels,
   * so no need to handle them ourselves. */
  if (dec->info.pic_x & 1 && dec->info.pixel_fmt != TH_PF_444) {
    width++;
  }
  if (dec->info.pic_y & 1 && dec->info.pixel_fmt == TH_PF_420) {
    height++;
  }

  GST_DEBUG_OBJECT (dec, "after fixup frame dimension %dx%d, offset %d:%d",
      width, height, dec->info.pic_x, dec->info.pic_y);

  if (width == 0 || height == 0)
    goto invalid_dimensions;

  /* done */
  dec->decoder = th_decode_alloc (&dec->info, dec->setup);

  if (dec->telemetry_mv &&
      th_decode_ctl (dec->decoder, TH_DECCTL_SET_TELEMETRY_MV,
          &dec->telemetry_mv, sizeof (dec->telemetry_mv)) != TH_EIMPL) {
    GST_WARNING_OBJECT (dec, "Could not enable MV visualisation");
  }
  if (dec->telemetry_mbmode &&
      th_decode_ctl (dec->decoder, TH_DECCTL_SET_TELEMETRY_MBMODE,
          &dec->telemetry_mbmode, sizeof (dec->telemetry_mbmode)) != TH_EIMPL) {
    GST_WARNING_OBJECT (dec, "Could not enable MB mode visualisation");
  }
  if (dec->telemetry_qi &&
      th_decode_ctl (dec->decoder, TH_DECCTL_SET_TELEMETRY_QI,
          &dec->telemetry_qi, sizeof (dec->telemetry_qi)) != TH_EIMPL) {
    GST_WARNING_OBJECT (dec, "Could not enable QI mode visualisation");
  }
  if (dec->telemetry_bits &&
      th_decode_ctl (dec->decoder, TH_DECCTL_SET_TELEMETRY_BITS,
          &dec->telemetry_bits, sizeof (dec->telemetry_bits)) != TH_EIMPL) {
    GST_WARNING_OBJECT (dec, "Could not enable BITS mode visualisation");
  }

  /* Create the output state */
  if (dec->output_state)
    gst_video_codec_state_unref (dec->output_state);
  dec->output_state = state =
      gst_video_decoder_set_output_state (GST_VIDEO_DECODER (dec), fmt,
      width, height, dec->input_state);

  /* FIXME : Do we still need to set fps/par now that we pass the reference input stream ? */
  state->info.fps_n = dec->info.fps_numerator;
  state->info.fps_d = dec->info.fps_denominator;
  state->info.par_n = par_num;
  state->info.par_d = par_den;

  /* these values are for all versions of the colorspace specified in the
   * theora info */
  state->info.chroma_site = GST_VIDEO_CHROMA_SITE_JPEG;
  state->info.colorimetry.range = GST_VIDEO_COLOR_RANGE_16_235;
  state->info.colorimetry.matrix = GST_VIDEO_COLOR_MATRIX_BT601;
  state->info.colorimetry.transfer = GST_VIDEO_TRANSFER_BT709;
  switch (dec->info.colorspace) {
    case TH_CS_ITU_REC_470M:
      state->info.colorimetry.primaries = GST_VIDEO_COLOR_PRIMARIES_BT470M;
      break;
    case TH_CS_ITU_REC_470BG:
      state->info.colorimetry.primaries = GST_VIDEO_COLOR_PRIMARIES_BT470BG;
      break;
    default:
      state->info.colorimetry.primaries = GST_VIDEO_COLOR_PRIMARIES_UNKNOWN;
      break;
  }

  if (!gst_video_decoder_negotiate (GST_VIDEO_DECODER (dec)))
    goto not_negotiated;

  dec->have_header = TRUE;

  return ret;

  /* ERRORS */
unsupported_format:
  {
    GST_ERROR_OBJECT (dec, "Invalid pixel format %d", dec->info.pixel_fmt);
    return GST_FLOW_ERROR;
  }

not_negotiated:
  {
    GST_ERROR_OBJECT (dec, "Failed to negotiate");
    return GST_FLOW_NOT_NEGOTIATED;
  }

invalid_dimensions:
  {
    GST_ERROR_OBJECT (dec, "Invalid dimensions (width:%d, height:%d)",
        width, height);
    return GST_FLOW_ERROR;
  }
}

static GstFlowReturn
theora_handle_header_packet (GstTheoraDec * dec, ogg_packet * packet)
{
  GstFlowReturn res;
  int ret;

  GST_DEBUG_OBJECT (dec, "parsing header packet");

  ret = th_decode_headerin (&dec->info, &dec->comment, &dec->setup, packet);
  if (ret < 0)
    goto header_read_error;

  switch (packet->packet[0]) {
    case 0x81:
      res = theora_handle_comment_packet (dec, packet);
      break;
    case 0x82:
      res = theora_handle_type_packet (dec);
      break;
    default:
      /* ignore */
      GST_WARNING_OBJECT (dec, "unknown theora header packet found");
    case 0x80:
      /* nothing special, this is the identification header */
      res = GST_FLOW_OK;
      break;
  }
  return res;

  /* ERRORS */
header_read_error:
  {
    GST_ELEMENT_ERROR (GST_ELEMENT (dec), STREAM, DECODE,
        (NULL), ("couldn't read header packet"));
    return GST_FLOW_ERROR;
  }
}

#define MIN_NUM_HEADERS 3
static GstFlowReturn
theoradec_handle_header_caps (GstTheoraDec * dec)
{
  GstFlowReturn result = GST_CUSTOM_FLOW_DROP;
  GstCaps *caps;
  GstStructure *s = NULL;
  const GValue *array = NULL;

  GST_DEBUG_OBJECT (dec, "Looking for Theora headers in caps");
  caps = gst_pad_get_current_caps (GST_VIDEO_DECODER_SINK_PAD (dec));
  if (caps)
    s = gst_caps_get_structure (caps, 0);
  if (s)
    array = gst_structure_get_value (s, "streamheader");

  if (caps)
    gst_caps_unref (caps);

  if (array && (gst_value_array_get_size (array) >= MIN_NUM_HEADERS)) {
    const GValue *value = NULL;
    GstBuffer *buf = NULL;
    gint i = 0;

    while (result == GST_CUSTOM_FLOW_DROP
        && i < gst_value_array_get_size (array)) {
      value = gst_value_array_get_value (array, i);
      buf = gst_value_get_buffer (value);
      if (!buf)
        goto null_buffer;
      GST_LOG_OBJECT (dec, "Submitting header packet");
      result = theora_dec_decode_buffer (dec, buf, NULL);
      i++;
    }
  } else
    goto array_error;

done:
  return (result !=
      GST_CUSTOM_FLOW_DROP ? GST_FLOW_NOT_NEGOTIATED : GST_FLOW_OK);

  /* ERRORS */
array_error:
  {
    GST_WARNING_OBJECT (dec, "streamheader array not found");
    result = GST_FLOW_ERROR;
    goto done;
  }
null_buffer:
  {
    GST_WARNING_OBJECT (dec, "streamheader with null buffer received");
    result = GST_FLOW_ERROR;
    goto done;
  }
}

/* Allocate output frame and copy image data */
static GstFlowReturn
theora_handle_image (GstTheoraDec * dec, th_ycbcr_buffer buf,
    GstVideoCodecFrame * frame)
{
  GstVideoDecoder *decoder = GST_VIDEO_DECODER (dec);
  gint width, height, stride;
  GstFlowReturn result;
  gint i, comp;
  guint8 *dest, *src;
  GstVideoFrame vframe;
  gint pic_width, pic_height;
  gint offset_x, offset_y;

  result = gst_video_decoder_allocate_output_frame (decoder, frame);

  if (G_UNLIKELY (result != GST_FLOW_OK)) {
    GST_DEBUG_OBJECT (dec, "could not get buffer, reason: %s",
        gst_flow_get_name (result));
    return result;
  }

  /* we need to crop the hard way */
  offset_x = dec->info.pic_x;
  offset_y = dec->info.pic_y;
  pic_width = dec->info.pic_width;
  pic_height = dec->info.pic_height;
  /* Ensure correct offsets in chroma for formats that need it
   * by rounding the offset. libtheora will add proper pixels,
   * so no need to handle them ourselves. */
  if (offset_x & 1 && dec->info.pixel_fmt != TH_PF_444)
    offset_x--;
  if (offset_y & 1 && dec->info.pixel_fmt == TH_PF_420)
    offset_y--;

  /* if only libtheora would allow us to give it a destination frame */
  GST_CAT_TRACE_OBJECT (CAT_PERFORMANCE, dec,
      "doing unavoidable video frame copy");

  if (G_UNLIKELY (!gst_video_frame_map (&vframe, &dec->output_state->info,
              frame->output_buffer, GST_MAP_WRITE)))
    goto invalid_frame;

  for (comp = 0; comp < 3; comp++) {
    width =
        GST_VIDEO_FORMAT_INFO_SCALE_WIDTH (vframe.info.finfo, comp, pic_width);
    height =
        GST_VIDEO_FORMAT_INFO_SCALE_HEIGHT (vframe.info.finfo, comp,
        pic_height);
    stride = GST_VIDEO_FRAME_COMP_STRIDE (&vframe, comp);
    dest = GST_VIDEO_FRAME_COMP_DATA (&vframe, comp);

    src = buf[comp].data;
    src += ((height == pic_height) ? offset_y : offset_y / 2)
        * buf[comp].stride;
    src += (width == pic_width) ? offset_x : offset_x / 2;

    for (i = 0; i < height; i++) {
      memcpy (dest, src, width);

      dest += stride;
      src += buf[comp].stride;
    }
  }
  gst_video_frame_unmap (&vframe);

  return GST_FLOW_OK;
invalid_frame:
  {
    GST_DEBUG_OBJECT (dec, "could not map video frame");
    return GST_FLOW_ERROR;
  }
}

static GstFlowReturn
theora_handle_data_packet (GstTheoraDec * dec, ogg_packet * packet,
    GstVideoCodecFrame * frame)
{
  /* normal data packet */
  th_ycbcr_buffer buf;
  gboolean keyframe;
  GstFlowReturn result;
  ogg_int64_t gp;

  if (G_UNLIKELY (!dec->have_header)) {
    result = theoradec_handle_header_caps (dec);
    if (result != GST_FLOW_OK)
      goto not_initialized;
  }

  /* the second most significant bit of the first data byte is cleared
   * for keyframes. We can only check it if it's not a zero-length packet. */
  keyframe = packet->bytes && ((packet->packet[0] & 0x40) == 0);
  if (G_UNLIKELY (keyframe)) {
    GST_DEBUG_OBJECT (dec, "we have a keyframe");
    dec->need_keyframe = FALSE;
  } else if (G_UNLIKELY (dec->need_keyframe)) {
    goto dropping;
  }

  GST_DEBUG_OBJECT (dec, "parsing data packet");

  /* this does the decoding */
  if (G_UNLIKELY (th_decode_packetin (dec->decoder, packet, &gp) < 0))
    goto decode_error;

  if (frame &&
      (gst_video_decoder_get_max_decode_time (GST_VIDEO_DECODER (dec),
              frame) < 0))
    goto dropping_qos;

  /* this does postprocessing and set up the decoded frame
   * pointers in our yuv variable */
  if (G_UNLIKELY (th_decode_ycbcr_out (dec->decoder, buf) < 0))
    goto no_yuv;

  if (G_UNLIKELY ((buf[0].width != dec->info.frame_width)
          || (buf[0].height != dec->info.frame_height)))
    goto wrong_dimensions;

  result = theora_handle_image (dec, buf, frame);

  return result;

  /* ERRORS */
not_initialized:
  {
    GST_ELEMENT_ERROR (GST_ELEMENT (dec), STREAM, DECODE,
        (NULL), ("no header sent yet"));
    return GST_FLOW_ERROR;
  }
dropping:
  {
    GST_WARNING_OBJECT (dec, "dropping frame because we need a keyframe");
    return GST_CUSTOM_FLOW_DROP;
  }
dropping_qos:
  {
    GST_WARNING_OBJECT (dec, "dropping frame because of QoS");
    return GST_CUSTOM_FLOW_DROP;
  }
decode_error:
  {
    GST_ELEMENT_ERROR (GST_ELEMENT (dec), STREAM, DECODE,
        (NULL), ("theora decoder did not decode data packet"));
    return GST_FLOW_ERROR;
  }
no_yuv:
  {
    GST_ELEMENT_ERROR (GST_ELEMENT (dec), STREAM, DECODE,
        (NULL), ("couldn't read out YUV image"));
    return GST_FLOW_ERROR;
  }
wrong_dimensions:
  {
    GST_ELEMENT_ERROR (GST_ELEMENT (dec), STREAM, FORMAT,
        (NULL), ("dimensions of image do not match header"));
    return GST_FLOW_ERROR;
  }
}

static GstFlowReturn
theora_dec_decode_buffer (GstTheoraDec * dec, GstBuffer * buf,
    GstVideoCodecFrame * frame)
{
  ogg_packet packet;
  GstFlowReturn result = GST_FLOW_OK;
  GstMapInfo minfo;

  /* make ogg_packet out of the buffer */
  gst_buffer_map (buf, &minfo, GST_MAP_READ);
  packet.packet = minfo.data;
  packet.bytes = minfo.size;
  packet.granulepos = -1;
  packet.packetno = 0;          /* we don't really care */
  packet.b_o_s = dec->have_header ? 0 : 1;
  /* EOS does not matter for the decoder */
  packet.e_o_s = 0;

  GST_LOG_OBJECT (dec, "decode buffer of size %ld", packet.bytes);

  GST_DEBUG_OBJECT (dec, "header=%02x", packet.bytes ? packet.packet[0] : -1);

  /* switch depending on packet type. A zero byte packet is always a data
   * packet; we don't dereference it in that case. */
  if (packet.bytes && packet.packet[0] & 0x80) {
    /* header packets are not meant to be displayed - return FLOW_DROP */
    if (dec->have_header) {
      GST_WARNING_OBJECT (GST_OBJECT (dec), "Ignoring header");
      result = GST_CUSTOM_FLOW_DROP;
      goto done;
    }
    if ((result = theora_handle_header_packet (dec, &packet)) != GST_FLOW_OK)
      goto done;
    result = GST_CUSTOM_FLOW_DROP;
  } else {
    result = theora_handle_data_packet (dec, &packet, frame);
  }

done:
  gst_buffer_unmap (buf, &minfo);

  return result;
}

static GstFlowReturn
theora_dec_handle_frame (GstVideoDecoder * bdec, GstVideoCodecFrame * frame)
{
  GstTheoraDec *dec;
  GstFlowReturn res;

  dec = GST_THEORA_DEC (bdec);

  res = theora_dec_decode_buffer (dec, frame->input_buffer, frame);
  switch (res) {
    case GST_FLOW_OK:
      res = gst_video_decoder_finish_frame (bdec, frame);
      break;
    case GST_CUSTOM_FLOW_DROP:
      res = gst_video_decoder_drop_frame (bdec, frame);
      break;
    default:
      gst_video_codec_frame_unref (frame);
      break;
  }

  return res;
}

static void
theora_dec_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstTheoraDec *dec = GST_THEORA_DEC (object);

  switch (prop_id) {
    case PROP_TELEMETRY_MV:
      dec->telemetry_mv = g_value_get_int (value);
      break;
    case PROP_TELEMETRY_MBMODE:
      dec->telemetry_mbmode = g_value_get_int (value);
      break;
    case PROP_TELEMETRY_QI:
      dec->telemetry_qi = g_value_get_int (value);
      break;
    case PROP_TELEMETRY_BITS:
      dec->telemetry_bits = g_value_get_int (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
theora_dec_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstTheoraDec *dec = GST_THEORA_DEC (object);

  switch (prop_id) {
    case PROP_TELEMETRY_MV:
      g_value_set_int (value, dec->telemetry_mv);
      break;
    case PROP_TELEMETRY_MBMODE:
      g_value_set_int (value, dec->telemetry_mbmode);
      break;
    case PROP_TELEMETRY_QI:
      g_value_set_int (value, dec->telemetry_qi);
      break;
    case PROP_TELEMETRY_BITS:
      g_value_set_int (value, dec->telemetry_bits);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}
