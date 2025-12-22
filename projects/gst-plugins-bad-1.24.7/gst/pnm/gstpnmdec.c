/* GStreamer PNM decoder
 * Copyright (C) 2009 Lutz Mueller <lutz@users.sourceforge.net>
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
 * SECTION:element-pnmdec
 * @title: pnmdec
 *
 * Decodes pnm images.
 *
 * ## Example launch line
 * |[
 * gst-launch-1.0 filesrc location=test.pnm ! pnmdec ! videoconvert ! autovideosink
 * ]| The above pipeline reads a pnm file and renders it to the screen.
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gstpnmdec.h"
#include "gstpnmutils.h"

#include <gst/gstutils.h>
#include <gst/video/video.h>
#include <gst/base/gstbytereader.h>
#include <string.h>
#include <stdio.h>

static gboolean gst_pnmdec_start (GstVideoDecoder * decoder);
static GstFlowReturn gst_pnmdec_finish (GstVideoDecoder * decoder);
static gboolean gst_pnmdec_set_format (GstVideoDecoder * decoder,
    GstVideoCodecState * state);
static gboolean gst_pnmdec_stop (GstVideoDecoder * decoder);
static GstFlowReturn gst_pnmdec_parse (GstVideoDecoder * decoder,
    GstVideoCodecFrame * frame, GstAdapter * adapter, gboolean at_eos);
static GstFlowReturn gst_pnmdec_handle_frame (GstVideoDecoder * decoder,
    GstVideoCodecFrame * frame);
static GstFlowReturn
gst_pnmdec_parse_ascii (GstPnmdec * s, const guint8 * b, guint bs);

G_DEFINE_TYPE (GstPnmdec, gst_pnmdec, GST_TYPE_VIDEO_DECODER);
GST_ELEMENT_REGISTER_DEFINE (pnmdec, "pnmdec", GST_RANK_PRIMARY,
    GST_TYPE_PNMDEC);

static GstStaticPadTemplate gst_pnmdec_src_pad_template =
GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (GST_VIDEO_CAPS_MAKE
        ("{ RGB, GRAY8, GRAY16_BE, GRAY16_LE }")));

static GstStaticCaps gst_pnmdec_gray16_caps =
GST_STATIC_CAPS (GST_VIDEO_CAPS_MAKE ("{ GRAY16_BE, GRAY16_LE }"));

static GstStaticPadTemplate gst_pnmdec_sink_pad_template =
GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (MIME_ALL));

GST_DEBUG_CATEGORY (pnmdecoder_debug);
#define GST_CAT_DEFAULT pnmdecoder_debug

static void
gst_pnmdec_class_init (GstPnmdecClass * klass)
{
  GstElementClass *element_class = GST_ELEMENT_CLASS (klass);
  GstVideoDecoderClass *vdec_class = (GstVideoDecoderClass *) klass;

  GST_DEBUG_CATEGORY_INIT (pnmdecoder_debug, "pnmdec", 0, "PNM Video Decoder");

  gst_element_class_add_static_pad_template (element_class,
      &gst_pnmdec_src_pad_template);
  gst_element_class_add_static_pad_template (element_class,
      &gst_pnmdec_sink_pad_template);

  gst_element_class_set_static_metadata (element_class, "PNM image decoder",
      "Codec/Decoder/Image",
      "Decodes images in portable pixmap/graymap/bitmap/anymamp (PNM) format",
      "Lutz Mueller <lutz@users.sourceforge.net>");

  vdec_class->start = gst_pnmdec_start;
  vdec_class->finish = gst_pnmdec_finish;
  vdec_class->stop = gst_pnmdec_stop;
  vdec_class->parse = gst_pnmdec_parse;
  vdec_class->handle_frame = gst_pnmdec_handle_frame;
  vdec_class->set_format = gst_pnmdec_set_format;
}

static void
gst_pnmdec_flush (GstPnmdec * s)
{
  memset (&s->mngr, 0, sizeof (s->mngr));
  s->size = 0;
  s->current_size = 0;
  if (s->buf) {
    gst_buffer_unref (s->buf);
    s->buf = NULL;
  }
}

static void
gst_pnmdec_init (GstPnmdec * s)
{
  /* Initialize decoder */
  s->buf = NULL;
  gst_pnmdec_flush (s);

  gst_video_decoder_set_use_default_pad_acceptcaps (GST_VIDEO_DECODER_CAST
      (s), TRUE);
  GST_PAD_SET_ACCEPT_TEMPLATE (GST_VIDEO_DECODER_SINK_PAD (s));
}

static GstFlowReturn
gst_pnmdec_negotiate (GstVideoDecoder * decoder)
{
  GstPnmdec *pnmdec = (GstPnmdec *) decoder;
  GstVideoFormat fmt = GST_VIDEO_FORMAT_UNKNOWN;
  GstVideoCodecState *output_state;

  switch (pnmdec->mngr.info.type) {
    case GST_PNM_TYPE_BITMAP:
      if (pnmdec->mngr.info.encoding == GST_PNM_ENCODING_ASCII) {
        return GST_FLOW_ERROR;
      }
      pnmdec->size = pnmdec->mngr.info.width * pnmdec->mngr.info.height * 1;
      fmt = GST_VIDEO_FORMAT_GRAY8;
      break;
    case GST_PNM_TYPE_GRAYMAP:
      if (pnmdec->mngr.info.max > 255) {
        GstCaps *gray16_caps = gst_static_caps_get (&gst_pnmdec_gray16_caps);
        GstCaps *peercaps;
        GstStructure *peerstruct;
        const gchar *fmtstr;

        pnmdec->size = pnmdec->mngr.info.width * pnmdec->mngr.info.height * 2;
        /* perform some basic negotiation to resolve which endianness,
         * if any, is supported by the component downstream. Query
         * the peer caps, intersecting with our preferred caps
         */
        peercaps =
            gst_pad_peer_query_caps (GST_VIDEO_DECODER_SRC_PAD (decoder),
            gray16_caps);
        gst_caps_unref (gray16_caps);

        GST_DEBUG ("Received caps from peer: %" GST_PTR_FORMAT, peercaps);
        if (gst_caps_is_empty (peercaps)) {
          gst_caps_unref (peercaps);
          return FALSE;
        }

        if (!gst_caps_is_fixed (peercaps))
          peercaps = gst_caps_fixate (peercaps);

        peerstruct = gst_caps_get_structure (peercaps, 0);
        fmtstr = gst_structure_get_string (peerstruct, "format");
        if (fmtstr) {
          if (g_str_equal (fmtstr, "GRAY16_BE")) {
            fmt = GST_VIDEO_FORMAT_GRAY16_BE;
          } else if (g_str_equal (fmtstr, "GRAY16_LE")) {
            fmt = GST_VIDEO_FORMAT_GRAY16_LE;
          }
        }
        gst_caps_unref (peercaps);
      } else {
        pnmdec->size = pnmdec->mngr.info.width * pnmdec->mngr.info.height * 1;
        fmt = GST_VIDEO_FORMAT_GRAY8;
      }
      break;
    case GST_PNM_TYPE_PIXMAP:
      pnmdec->size = pnmdec->mngr.info.width * pnmdec->mngr.info.height * 3;
      fmt = GST_VIDEO_FORMAT_RGB;
      break;
  }

  if (fmt == GST_VIDEO_FORMAT_UNKNOWN)
    return GST_FLOW_NOT_NEGOTIATED;

  pnmdec->out_format = fmt;

  output_state =
      gst_video_decoder_set_output_state (decoder, fmt,
      pnmdec->mngr.info.width, pnmdec->mngr.info.height, pnmdec->input_state);
  gst_video_codec_state_unref (output_state);

  if (gst_video_decoder_negotiate (decoder) == FALSE)
    return GST_FLOW_NOT_NEGOTIATED;

  return GST_FLOW_OK;
}

static gboolean
gst_pnmdec_set_format (GstVideoDecoder * decoder, GstVideoCodecState * state)
{
  GstPnmdec *pnmdec = (GstPnmdec *) decoder;

  gst_pnmdec_negotiate (decoder);

  if (pnmdec->input_state)
    gst_video_codec_state_unref (pnmdec->input_state);
  pnmdec->input_state = gst_video_codec_state_ref (state);

  return TRUE;
}

static gboolean
gst_pnmdec_stop (GstVideoDecoder * decoder)
{
  GstPnmdec *pnmdec = (GstPnmdec *) decoder;

  if (pnmdec->input_state) {
    gst_video_codec_state_unref (pnmdec->input_state);
    pnmdec->input_state = NULL;
  }

  if (pnmdec->buf) {
    gst_buffer_unref (pnmdec->buf);
    pnmdec->buf = NULL;
  }
  return TRUE;
}

static GstFlowReturn
gst_pnmdec_parse_ascii (GstPnmdec * s, const guint8 * b, guint bs)
{
  GScanner *scanner;
  guint i = 0;
  guint target, last_val = 0;
  GstMapInfo map;
  guint8 *outdata;

  if (!s->buf)
    return GST_FLOW_OK;

  target = s->size - s->current_size;

  gst_buffer_map (s->buf, &map, GST_MAP_WRITE);

  if (bs) {
    GST_MEMDUMP_OBJECT (s, "Starting parse:", b, MIN (16, bs));
  }

  /* leave the number of bytes already parsed */
  outdata = map.data + s->current_size;

  if (s->have_last_val) {
    while (bs && *b >= '0' && *b <= '9') {
      s->last_val = 10 * s->last_val + *b - '0';
      b++;
      if (!--bs) {
        goto drop_error;
      }
    }
    if (s->last_val > s->mngr.info.max) {
      GST_DEBUG_OBJECT (s, "Corrupt ASCII encoded PNM file.");
      goto drop_error;
    }

    GST_LOG_OBJECT (s, "Collected partial value from previous parse - %u",
        s->last_val);
    if (s->mngr.info.max > 255) {
      if (i + 1 >= target) {
        GST_DEBUG_OBJECT (s, "PNM file contains too much data.");
        goto drop_error;
      }
      if (s->out_format == GST_VIDEO_FORMAT_GRAY16_BE)
        GST_WRITE_UINT16_BE (outdata + i, s->last_val);
      else
        GST_WRITE_UINT16_LE (outdata + i, s->last_val);
      i += 2;
    } else {
      outdata[i++] = s->last_val;
    }
    last_val = s->last_val;
    s->have_last_val = FALSE;
  }

  /* Might be no data if we're at EOS */
  if (!bs)
    goto done;

  scanner = g_scanner_new (NULL);
  g_scanner_input_text (scanner, (gchar *) b, bs);
  while (!g_scanner_eof (scanner)) {
    switch (g_scanner_get_next_token (scanner)) {
      case G_TOKEN_INT:
        if (s->mngr.info.max > 255) {
          if (i + 1 >= target) {
            GST_DEBUG_OBJECT (s,
                "PNM file contains too much data after line %u col %u.",
                scanner->line, scanner->position);
            g_scanner_destroy (scanner);
            goto done;          // drop_error;
          }
          if (s->out_format == GST_VIDEO_FORMAT_GRAY16_BE)
            GST_WRITE_UINT16_BE (outdata + i, scanner->value.v_int);
          else
            GST_WRITE_UINT16_LE (outdata + i, scanner->value.v_int);
          i += 2;
        } else {
          if (i == target) {
            GST_DEBUG_OBJECT (s,
                "PNM file contains too much data after line %u col %u.",
                scanner->line, scanner->position);
            g_scanner_destroy (scanner);
            goto drop_error;
          }
          outdata[i++] = scanner->value.v_int;
        }
        last_val = scanner->value.v_int;
        break;
      default:
        /* Should we care? */ ;
    }
  }
  g_scanner_destroy (scanner);

  /* If we didn't get the whole image, handle the last byte with care. */
  if (i && i < target && b[bs - 1] >= '0' && b[bs - 1] <= '9') {
    s->last_val = last_val;
    s->have_last_val = TRUE;
    if (s->mngr.info.max > 255)
      i -= 2;
    else
      i--;
    GST_LOG_OBJECT (s, "Stored last value %u for next parse cycle",
        s->last_val);
  }

done:
  /* Update the number of output bytes parsed in this scan */
  s->current_size += i;
  GST_LOG_OBJECT (s, "Parsed %u bytes, now have %u bytes of %u output",
      i, s->current_size, s->size);
  gst_buffer_unmap (s->buf, &map);

  return GST_FLOW_OK;

drop_error:
  gst_buffer_unmap (s->buf, &map);

  return GST_FLOW_ERROR;
}

static GstFlowReturn
gst_pnmdec_handle_frame (GstVideoDecoder * decoder, GstVideoCodecFrame * frame)
{
  GstPnmdec *s = (GstPnmdec *) decoder;
  GstMapInfo imap, omap;
  guint i_rowstride;
  guint o_rowstride;
  GstFlowReturn r = GST_FLOW_OK;
  gint bytes, i, total_bytes = 0;

  r = gst_video_decoder_allocate_output_frame (decoder, frame);
  if (r != GST_FLOW_OK) {
    gst_video_decoder_drop_frame (GST_VIDEO_DECODER (s), frame);
    goto out;
  }

  if (s->mngr.info.encoding == GST_PNM_ENCODING_ASCII) {
    /* In case of ASCII parsed data is stored in buf, so input needs to be
       taken from here for frame processing */
    gst_buffer_map (s->buf, &imap, GST_MAP_READ);
  } else {
    gst_buffer_map (frame->input_buffer, &imap, GST_MAP_READ);
  }
  gst_buffer_map (frame->output_buffer, &omap, GST_MAP_WRITE);

  gst_buffer_copy_into (frame->output_buffer, frame->input_buffer,
      GST_BUFFER_COPY_METADATA, 0, 0);

  if (s->mngr.info.type == GST_PNM_TYPE_BITMAP) {
    bytes = (s->mngr.info.width * s->mngr.info.height + 7) / 8;
    for (i = 0; i < bytes; i++) {
      omap.data[i * 8] = (imap.data[i] & 0x80) ? 0 : 255;
      omap.data[i * 8 + 1] = (imap.data[i] & 0x40) ? 0 : 255;
      omap.data[i * 8 + 2] = (imap.data[i] & 0x20) ? 0 : 255;
      omap.data[i * 8 + 3] = (imap.data[i] & 0x10) ? 0 : 255;
      omap.data[i * 8 + 4] = (imap.data[i] & 0x08) ? 0 : 255;
      omap.data[i * 8 + 5] = (imap.data[i] & 0x04) ? 0 : 255;
      omap.data[i * 8 + 6] = (imap.data[i] & 0x02) ? 0 : 255;
      omap.data[i * 8 + 7] = (imap.data[i] & 0x01) ? 0 : 255;
    }
    total_bytes = bytes * 8;
  } else
    /* Need to convert from PNM rowstride to GStreamer rowstride */
  if (s->mngr.info.width % 4 != 0) {
    if (s->mngr.info.type == GST_PNM_TYPE_PIXMAP) {
      i_rowstride = 3 * s->mngr.info.width;
      o_rowstride = GST_ROUND_UP_4 (i_rowstride);
    } else {
      if (s->mngr.info.max > 255)
        i_rowstride = s->mngr.info.width * 2;
      else
        i_rowstride = s->mngr.info.width;
      o_rowstride = GST_ROUND_UP_4 (i_rowstride);
    }

    for (i = 0; i < s->mngr.info.height; i++)
      memcpy (omap.data + i * o_rowstride, imap.data + i * i_rowstride,
          i_rowstride);
    total_bytes = o_rowstride * s->mngr.info.height;
  } else {
    memcpy (omap.data, imap.data, s->size);
    total_bytes = s->size;
  }

  if (s->mngr.info.type != GST_PNM_TYPE_BITMAP) {
    if (s->mngr.info.max > 255 && s->mngr.info.max < 65535) {
      /* Convert the pixels from 0 - max range to 0 - 65535 range
       * and appropriate endianness (input is always BE) */
      guint8 *data = omap.data;
      gint max = s->mngr.info.max;
      if (s->out_format == GST_VIDEO_FORMAT_GRAY16_BE) {
        for (i = 0; i < total_bytes; i += 2) {
          /* Clamp to 65535 */
          guint16 val = GST_READ_UINT16_BE (data + i);
          val = (val > max) ? 65535 : 65535 * val / max;
          GST_WRITE_UINT16_BE (data + i, val);
        }
      } else {
        for (i = 0; i < total_bytes; i += 2) {
          /* Clamp to 65535 */
          guint16 val = GST_READ_UINT16_BE (data + i);
          val = (val > max) ? 65535 : 65535 * val / max;
          GST_WRITE_UINT16_LE (data + i, val);
        }
      }
    } else if (s->mngr.info.max < 255) {
      /* Convert the pixels from 0 - max range to 0 - 255 range */
      gint max = s->mngr.info.max;
      for (i = 0; i < total_bytes; i++) {
        if (omap.data[i] <= max) {
          omap.data[i] = 255 * omap.data[i] / max;
        } else {
          /* This is an error case, wherein value in the data stream is
             more than max. Clamp such values to 255 */
          omap.data[i] = 255;
        }
      }
    }
  }

  if (s->mngr.info.encoding == GST_PNM_ENCODING_ASCII) {
    gst_buffer_unmap (s->buf, &imap);
  } else {
    gst_buffer_unmap (frame->input_buffer, &imap);
  }
  gst_buffer_unmap (frame->output_buffer, &omap);

  s->current_size = 0;

  r = gst_video_decoder_finish_frame (GST_VIDEO_DECODER (s), frame);

out:
  gst_pnmdec_flush (s);

  return r;
}

static GstFlowReturn
gst_pnmdec_parse (GstVideoDecoder * decoder, GstVideoCodecFrame * frame,
    GstAdapter * adapter, gboolean at_eos)
{
  gsize size;
  GstPnmdec *s = GST_PNMDEC (decoder);
  GstFlowReturn r = GST_FLOW_OK;
  guint offset = 0;
  const guint8 *raw_data = NULL;

  GST_VIDEO_CODEC_FRAME_SET_SYNC_POINT (frame);

  size = gst_adapter_available (adapter);
  if (size > 0)
    raw_data = gst_adapter_map (adapter, size);

  GST_LOG_OBJECT (s, "Entering parse with %" G_GSIZE_FORMAT " bytes. at_eos %d",
      size, at_eos);

  if (s->mngr.info.fields != GST_PNM_INFO_FIELDS_ALL) {
    GstPnmInfoMngrResult res;

    if (size < 8)
      goto need_more_data;

    res = gst_pnm_info_mngr_scan (&s->mngr, raw_data, size);

    switch (res) {
      case GST_PNM_INFO_MNGR_RESULT_FAILED:
        r = GST_FLOW_ERROR;
        goto out;
      case GST_PNM_INFO_MNGR_RESULT_READING:
        r = GST_FLOW_OK;
        goto out;
      case GST_PNM_INFO_MNGR_RESULT_FINISHED:

        r = gst_pnmdec_negotiate (decoder);
        if (r != GST_FLOW_OK)
          goto out;

        if (s->mngr.info.encoding == GST_PNM_ENCODING_ASCII) {
          /* It is not possible to know the size of input ascii data to parse.
             So we have to parse and know the number of pixels parsed and
             then finally decide when we have full frame */
          GST_DEBUG_OBJECT (s, "Allocating output frame of size %u", s->size);
          s->buf = gst_buffer_new_and_alloc (s->size);
        }
        offset = s->mngr.data_offset;
        gst_adapter_flush (adapter, offset);
        size = size - offset;
    }
  }

  if (s->mngr.info.encoding == GST_PNM_ENCODING_ASCII) {
    /* Parse ASCII data and populate s->current_size with the number of
       bytes actually parsed from the input data */
    GST_DEBUG_OBJECT (s, "Parsing %u bytes at offset %u", (guint) size, offset);
    r = gst_pnmdec_parse_ascii (s, raw_data + offset, size);
  } else {
    /* Bitmap Contains 8 pixels in a byte */
    if (s->mngr.info.type == GST_PNM_TYPE_BITMAP)
      s->current_size += (size * 8);
    else
      s->current_size += size;
  }

  gst_video_decoder_add_to_frame (decoder, size);
  if (s->size <= s->current_size) {
    goto have_full_frame;
  }

need_more_data:
  return GST_VIDEO_DECODER_FLOW_NEED_DATA;

have_full_frame:
  return gst_video_decoder_have_frame (decoder);

out:
  return r;
}

static gboolean
gst_pnmdec_start (GstVideoDecoder * decoder)
{
  GstPnmdec *pnmdec = (GstPnmdec *) decoder;
  gst_video_decoder_set_packetized (GST_VIDEO_DECODER (pnmdec), FALSE);
  gst_pnmdec_flush (pnmdec);
  return TRUE;
}

static GstFlowReturn
gst_pnmdec_finish (GstVideoDecoder * decoder)
{
  GstPnmdec *s = (GstPnmdec *) decoder;

  GST_LOG_OBJECT (s, "finishing");

  if (s->mngr.info.encoding == GST_PNM_ENCODING_ASCII) {
    /* One last go at outputting any final value */
    gst_pnmdec_parse_ascii (s, 0, 0);
    if (s->size && s->size <= s->current_size) {
      return gst_video_decoder_have_frame (decoder);
    }
  }

  return GST_FLOW_OK;
}
