/* GStreamer
 * Copyright (C) 2022 Víctor Jáquez <vjaquez@igalia.com>
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
#include <config.h>
#endif

#include <gst/base/base.h>
#include "gstjpegdecoder.h"

#define MAX_SAMPLE_FACTOR 4     /* JPEG limit on sampling factors */
#define DCT_SIZE          8     /* The basic DCT block is 8x8 samples */

typedef enum
{
  GST_JPEG_DECODER_STATE_GOT_SOI = 1 << 0,
  GST_JPEG_DECODER_STATE_GOT_SOF = 1 << 1,
  GST_JPEG_DECODER_STATE_GOT_SOS = 1 << 2,
  GST_JPEG_DECODER_STATE_GOT_HUF_TABLE = 1 << 3,
  GST_JPEG_DECODER_STATE_GOT_IQ_TABLE = 1 << 4,

  GST_JPEG_DECODER_STATE_VALID_PICTURE = (GST_JPEG_DECODER_STATE_GOT_SOI |
      GST_JPEG_DECODER_STATE_GOT_SOF | GST_JPEG_DECODER_STATE_GOT_SOS),
} GstJpegDecoderState;

struct _GstJpegDecoderPrivate
{
  guint state;
  guint restart_interval;
  GstJpegHuffmanTables huf_tables;
  GstJpegQuantTables quant_tables;
  GstJpegFrameHdr frame_hdr;

  guint8 max_h, max_v;
  gboolean lossless;
};

GST_DEBUG_CATEGORY (gst_jpeg_decoder_debug);
#define GST_CAT_DEFAULT gst_jpeg_decoder_debug

/**
 * GstJpegDecoder:
 *
 * Base class to implement statelesss JPEG decoders
 *
 * Since: 1.22
 */
#define parent_class gst_jpeg_decoder_parent_clas
G_DEFINE_ABSTRACT_TYPE_WITH_CODE (GstJpegDecoder, gst_jpeg_decoder,
    GST_TYPE_VIDEO_DECODER, G_ADD_PRIVATE (GstJpegDecoder);
    GST_DEBUG_CATEGORY_INIT (gst_jpeg_decoder_debug, "jpegdecoder", 0,
        "JPEG Image Decoder"));

static gboolean gst_jpeg_decoder_set_format (GstVideoDecoder * decoder,
    GstVideoCodecState * state);
static GstFlowReturn gst_jpeg_decoder_handle_frame (GstVideoDecoder * decoder,
    GstVideoCodecFrame * frame);
static gboolean gst_jpeg_decoder_stop (GstVideoDecoder * decoder);

static void
gst_jpeg_decoder_class_init (GstJpegDecoderClass * klass)
{
  GstVideoDecoderClass *decoder_class = GST_VIDEO_DECODER_CLASS (klass);

  decoder_class->stop = GST_DEBUG_FUNCPTR (gst_jpeg_decoder_stop);
  decoder_class->set_format = GST_DEBUG_FUNCPTR (gst_jpeg_decoder_set_format);
  decoder_class->handle_frame =
      GST_DEBUG_FUNCPTR (gst_jpeg_decoder_handle_frame);

  gst_type_mark_as_plugin_api (GST_TYPE_JPEG_DECODER, 0);
}

static void
gst_jpeg_decoder_init (GstJpegDecoder * self)
{
  gst_video_decoder_set_packetized (GST_VIDEO_DECODER (self), TRUE);
  gst_video_decoder_set_needs_format (GST_VIDEO_DECODER (self), TRUE);

  self->priv = gst_jpeg_decoder_get_instance_private (self);
}

static gboolean
gst_jpeg_decoder_stop (GstVideoDecoder * decoder)
{
  GstJpegDecoder *self = GST_JPEG_DECODER (decoder);

  g_clear_pointer (&self->input_state, gst_video_codec_state_unref);

  return TRUE;
}

static gboolean
gst_jpeg_decoder_set_format (GstVideoDecoder * decoder,
    GstVideoCodecState * state)
{
  GstJpegDecoder *self = GST_JPEG_DECODER (decoder);

  GST_DEBUG_OBJECT (decoder, "Set format");

  if (self->input_state)
    gst_video_codec_state_unref (self->input_state);

  self->input_state = gst_video_codec_state_ref (state);

  return TRUE;
}

static inline gboolean
valid_state (guint state, guint ref_state)
{
  return (state & ref_state) == ref_state;
}

static gboolean
decode_huffman_table (GstJpegDecoder * self, GstJpegSegment * seg)
{
  GstJpegDecoderPrivate *priv = self->priv;

  if (!gst_jpeg_segment_parse_huffman_table (seg, &priv->huf_tables)) {
    GST_ERROR_OBJECT (self, "failed to parse Huffman table");
    return FALSE;
  }

  priv->state |= GST_JPEG_DECODER_STATE_GOT_HUF_TABLE;
  return TRUE;
}

static gboolean
decode_quant_table (GstJpegDecoder * self, GstJpegSegment * seg)
{
  GstJpegDecoderPrivate *priv = self->priv;

  if (!gst_jpeg_segment_parse_quantization_table (seg, &priv->quant_tables)) {
    GST_ERROR_OBJECT (self, "failed to parse quantization table");
    return FALSE;
  }

  priv->state |= GST_JPEG_DECODER_STATE_GOT_IQ_TABLE;
  return TRUE;
}

static gboolean
decode_restart_interval (GstJpegDecoder * self, GstJpegSegment * seg)
{
  GstJpegDecoderPrivate *priv = self->priv;

  if (!gst_jpeg_segment_parse_restart_interval (seg, &priv->restart_interval)) {
    GST_ERROR_OBJECT (self, "failed to parse restart interval");
    return FALSE;
  }
  return TRUE;
}

static GstFlowReturn
decode_frame (GstJpegDecoder * self, GstJpegSegment * seg,
    GstVideoCodecFrame * frame)
{
  GstJpegDecoderPrivate *priv = self->priv;
  GstJpegDecoderClass *klass = GST_JPEG_DECODER_GET_CLASS (self);
  GstJpegFrameHdr *frame_hdr = &self->priv->frame_hdr;
  GstFlowReturn ret = GST_FLOW_OK;
  guint i;

  if (!gst_jpeg_segment_parse_frame_header (seg, frame_hdr)) {
    GST_ERROR_OBJECT (self, "failed to parse frame header");
    return GST_FLOW_ERROR;
  }

  /* A.1.1 Dimensions and sampling factors */
  priv->max_h = priv->max_v = 0;
  for (i = 0; i < frame_hdr->num_components; i++) {
    if (frame_hdr->components[i].horizontal_factor >= MAX_SAMPLE_FACTOR
        || frame_hdr->components[i].vertical_factor >= MAX_SAMPLE_FACTOR) {
      ret = GST_FLOW_ERROR;
      GST_ERROR_OBJECT (self, "frame header with bad sampling factor");
      goto beach;
    }

    priv->max_h = MAX (priv->max_h, frame_hdr->components[i].horizontal_factor);
    priv->max_v = MAX (priv->max_v, frame_hdr->components[i].vertical_factor);
  }

  if (priv->max_h == 0 || priv->max_v == 0) {
    ret = GST_FLOW_ERROR;
    GST_ERROR_OBJECT (self, "frame header with bad sampling factor");
    goto beach;
  }

  priv->lossless = seg->marker == GST_JPEG_MARKER_SOF3;

  g_assert (klass->new_picture);
  ret = klass->new_picture (self, frame, seg->marker, &priv->frame_hdr);
  if (ret != GST_FLOW_OK) {
    GST_WARNING_OBJECT (self, "subclass failed to handle new picture");
    goto beach;
  }

  priv->state |= GST_JPEG_DECODER_STATE_GOT_SOF;

beach:
  return ret;
}


static const GstJpegFrameComponent *
get_component (const GstJpegFrameHdr * frame_hdr, guint selector)
{
  guint i;

  for (i = 0; i < frame_hdr->num_components; i++) {
    const GstJpegFrameComponent *fcp = &frame_hdr->components[i];
    if (fcp->identifier == selector)
      return fcp;
  }
  return NULL;
}

static GstFlowReturn
decode_scan (GstJpegDecoder * self, GstJpegSegment * seg)
{
  GstJpegDecoderPrivate *priv = self->priv;
  GstJpegDecoderClass *klass = GST_JPEG_DECODER_GET_CLASS (self);
  GstJpegDecoderScan scan;
  GstJpegScanHdr scan_hdr;
  GstFlowReturn ret;
  const guint8 *data;
  guint size, scan_hdr_size;
  guint64 mcus_per_row, mcu_rows_in_scan;
  guint blocksize = priv->lossless ? 1 : DCT_SIZE;

  /* E.2.3 Control procedure for decoding a scan */
  if (!valid_state (priv->state, GST_JPEG_DECODER_STATE_GOT_SOF))
    return GST_FLOW_OK;         /* ignore segment */

  if (!gst_jpeg_segment_parse_scan_header (seg, &scan_hdr)) {
    GST_ERROR_OBJECT (self, "failed to parse scan header");
    return GST_FLOW_ERROR;
  }

  if (!valid_state (priv->state, GST_JPEG_DECODER_STATE_GOT_HUF_TABLE))
    gst_jpeg_get_default_huffman_tables (&priv->huf_tables);

  if (!valid_state (priv->state, GST_JPEG_DECODER_STATE_GOT_IQ_TABLE))
    gst_jpeg_get_default_quantization_tables (&priv->quant_tables);

  /* Non-interleaved */
  if (scan_hdr.num_components == 1) {
    const guint cs = scan_hdr.components[0].component_selector;
    const GstJpegFrameComponent *fc = get_component (&priv->frame_hdr, cs);

    if (!fc || fc->horizontal_factor == 0 || fc->vertical_factor == 0) {
      GST_ERROR_OBJECT (self, "failed to validate frame component %u", cs);
      return GST_FLOW_ERROR;
    }

    mcus_per_row = gst_util_uint64_scale_int_ceil (priv->frame_hdr.width,
        fc->horizontal_factor, priv->max_h * blocksize);
    mcu_rows_in_scan = gst_util_uint64_scale_int_ceil (priv->frame_hdr.height,
        fc->vertical_factor, priv->max_v * blocksize);
  } else {
    mcus_per_row = gst_util_uint64_scale_int_ceil (priv->frame_hdr.width, 1,
        priv->max_h * blocksize);
    mcu_rows_in_scan =
        gst_util_uint64_scale_int_ceil (priv->frame_hdr.height, 1,
        priv->max_v * blocksize);
  }

  scan_hdr_size = (seg->data[seg->offset] << 8) | seg->data[seg->offset + 1];
  size = seg->size - scan_hdr_size;
  data = seg->data + seg->offset + scan_hdr_size;

  if (size <= 0)
    return GST_FLOW_ERROR;

  /* *INDENT-OFF* */
  scan = (GstJpegDecoderScan) {
    .scan_hdr = &scan_hdr,
    .huffman_tables = &priv->huf_tables,
    .quantization_tables = &priv->quant_tables,
    .restart_interval = priv->restart_interval,
    .mcus_per_row = mcus_per_row,
    .mcu_rows_in_scan = mcu_rows_in_scan,
  };
  /* *INDENT-ON* */

  g_assert (klass->decode_scan);
  ret = klass->decode_scan (self, &scan, data, size);

  if (ret == GST_FLOW_OK)
    priv->state |= GST_JPEG_DECODER_STATE_GOT_SOS;

  return ret;
}

#ifndef GST_DISABLE_GST_DEBUG
static const char *
_get_marker_name (guint marker)
{
#define MARKERS(V)                              \
  V (SOF0)                                      \
    V (SOF1)                                    \
    V (SOF2)                                    \
    V (SOF3)                                    \
    V (SOF5)                                    \
    V (SOF6)                                    \
    V (SOF7)                                    \
    V (SOF9)                                    \
    V (SOF10)                                   \
    V (SOF11)                                   \
    V (SOF13)                                   \
    V (SOF14)                                   \
    V (SOF15)                                   \
    V (DHT)                                     \
    V (DAC)                                     \
    V (RST0)                                    \
    V (RST1)                                    \
    V (RST2)                                    \
    V (RST3)                                    \
    V (RST4)                                    \
    V (RST5)                                    \
    V (RST6)                                    \
    V (RST7)                                    \
    V (SOI)                                     \
    V (EOI)                                     \
    V (SOS)                                     \
    V (DQT)                                     \
    V (DNL)                                     \
    V (DRI)                                     \
    V (APP0)                                    \
    V (APP1)                                    \
    V (APP2)                                    \
    V (APP3)                                    \
    V (APP4)                                    \
    V (APP5)                                    \
    V (APP6)                                    \
    V (APP7)                                    \
    V (APP8)                                    \
    V (APP9)                                    \
    V (APP10)                                   \
    V (APP11)                                   \
    V (APP12)                                   \
    V (APP13)                                   \
    V (APP14)                                   \
    V (APP15)                                   \
    V (COM)
#define CASE(marker) case G_PASTE(GST_JPEG_MARKER_, marker): return G_STRINGIFY (marker);
  switch (marker) {
      MARKERS (CASE)
    default:
      return "Unknown";
  }
#undef CASE
#undef MARKERS
}
#endif

static GstFlowReturn
gst_jpeg_decoder_handle_frame (GstVideoDecoder * decoder,
    GstVideoCodecFrame * frame)
{
  GstJpegDecoder *self = GST_JPEG_DECODER (decoder);
  GstJpegDecoderPrivate *priv = self->priv;
  GstJpegDecoderClass *klass = GST_JPEG_DECODER_GET_CLASS (self);
  GstBuffer *in_buf = frame->input_buffer;
  GstFlowReturn ret = GST_FLOW_OK;
  GstMapInfo map;
  GstJpegMarker marker;
  GstJpegSegment seg;
  guint offset = 0;

  GST_LOG_OBJECT (self, "handle frame %" GST_PTR_FORMAT, in_buf);

  if (!gst_buffer_map (in_buf, &map, GST_MAP_READ)) {
    GST_ERROR_OBJECT (self, "Cannot map input buffer");
    ret = GST_FLOW_ERROR;
    goto error;
  }

  priv->state = 0;

  /* E.2.1 Control procedure for decoding compressed image data */
  while (offset < map.size) {
    if (!gst_jpeg_parse (&seg, map.data, map.size, offset))
      goto unmap_and_error;

    offset = seg.offset + seg.size;
    marker = seg.marker;

    if (!valid_state (priv->state, GST_JPEG_DECODER_STATE_GOT_SOI)
        && marker != GST_JPEG_MARKER_SOI)
      goto unmap_and_error;

    GST_LOG_OBJECT (self, "marker %s: %" G_GSIZE_FORMAT,
        _get_marker_name (marker), seg.size);

    switch (marker) {

        /* Start of Image */
      case GST_JPEG_MARKER_SOI:
        priv->state |= GST_JPEG_DECODER_STATE_GOT_SOI;
        priv->restart_interval = 0;
        break;

        /* End of Image */
      case GST_JPEG_MARKER_EOI:
        if (!valid_state (priv->state, GST_JPEG_DECODER_STATE_VALID_PICTURE))
          goto unmap_and_error;

        g_assert (klass->end_picture);
        ret = klass->end_picture (self);
        if (ret != GST_FLOW_OK)
          goto unmap_and_error;

        priv->state = 0;

        gst_buffer_unmap (in_buf, &map);

        GST_VIDEO_CODEC_FRAME_SET_SYNC_POINT (frame);

        g_assert (klass->output_picture);
        return klass->output_picture (self, frame);

        /* Start of Scan */
      case GST_JPEG_MARKER_SOS:{
        /* get whole scan + ECSs, with RSTi */
        GstJpegSegment seg_scan;

        for (;;) {
          if (!gst_jpeg_parse (&seg_scan, map.data, map.size, offset))
            goto unmap_and_error;

          if (seg_scan.marker < GST_JPEG_MARKER_RST_MIN
              || seg_scan.marker > GST_JPEG_MARKER_RST_MAX)
            break;

          offset = seg_scan.offset + seg_scan.size;
        }

        offset = seg_scan.offset - 2;
        seg.size = offset - seg.offset;

        ret = decode_scan (self, &seg);
        if (ret != GST_FLOW_OK)
          goto unmap_and_error;

        break;
      }

        /* Interpret markers */
      case GST_JPEG_MARKER_DAC:
        /* FIXME: Annex D - Arithmetic coding */
        GST_FIXME_OBJECT (self, "Arithmetic coding mode unsupported");
        goto unmap_and_error;
      case GST_JPEG_MARKER_DHT:
        if (!decode_huffman_table (self, &seg)) {
          ret = GST_FLOW_ERROR;
          goto unmap_and_error;
        }
        break;
      case GST_JPEG_MARKER_DQT:
        if (!decode_quant_table (self, &seg)) {
          ret = GST_FLOW_ERROR;
          goto unmap_and_error;
        }
        break;

      case GST_JPEG_MARKER_DRI:
        if (!decode_restart_interval (self, &seg)) {
          GST_WARNING_OBJECT (self, "Fail to decode restart interval");
          goto unmap_and_error;
        }
        break;
      case GST_JPEG_MARKER_DNL:
        break;
      default:
        /* SOFn (Start Of Frame) */
        if (marker >= GST_JPEG_MARKER_SOF_MIN &&
            marker <= GST_JPEG_MARKER_SOF_MAX) {
          ret = decode_frame (self, &seg, frame);
          if (ret != GST_FLOW_OK)
            goto unmap_and_error;
        }
        break;
    }
  }

  ret = GST_FLOW_ERROR;

unmap_and_error:
  {
    gst_buffer_unmap (in_buf, &map);
    goto error;
  }

error:
  {
    if (ret == GST_FLOW_ERROR) {
      GST_VIDEO_DECODER_ERROR (self, 1, STREAM, DECODE,
          ("Failed to decode data"), (NULL), ret);
    }

    gst_video_decoder_drop_frame (decoder, frame);

    return ret;
  }
}
