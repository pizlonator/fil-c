/*
 * GStreamer
 * Copyright (C) 2019 Mathieu Duponchelle <mathieu@centricular.com>
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
 * SECTION:element-line21encoder
 * @title: line21encoder
 *
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <gst/gst.h>
#include <gst/base/base.h>
#include <gst/video/video.h>
#include <string.h>

#include "gstline21enc.h"
#include "io-sim.h"

GST_DEBUG_CATEGORY_STATIC (gst_line_21_encoder_debug);
#define GST_CAT_DEFAULT gst_line_21_encoder_debug

enum
{
  PROP_0,
  PROP_REMOVE_CAPTION_META,
};

/* FIXME: add and test support for PAL resolutions */
#define CAPS "video/x-raw, format={ I420, YUY2, YVYU, UYVY, VYUY }, width=(int)720, height=(int){ 525, 486 }, interlace-mode=interleaved"

static GstStaticPadTemplate sinktemplate = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (CAPS));

static GstStaticPadTemplate srctemplate = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (CAPS));

G_DEFINE_TYPE (GstLine21Encoder, gst_line_21_encoder, GST_TYPE_VIDEO_FILTER);
#define parent_class gst_line_21_encoder_parent_class
GST_ELEMENT_REGISTER_DEFINE (line21encoder, "line21encoder",
    GST_RANK_NONE, GST_TYPE_LINE21ENCODER);

static gboolean gst_line_21_encoder_set_info (GstVideoFilter * filter,
    GstCaps * incaps, GstVideoInfo * in_info,
    GstCaps * outcaps, GstVideoInfo * out_info);
static GstFlowReturn gst_line_21_encoder_transform_ip (GstVideoFilter * filter,
    GstVideoFrame * frame);
static void gst_line_21_encoder_set_property (GObject * self, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_line_21_encoder_get_property (GObject * self, guint prop_id,
    GValue * value, GParamSpec * pspec);


static void
gst_line_21_encoder_class_init (GstLine21EncoderClass * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *gstelement_class;
  GstVideoFilterClass *filter_class;

  gobject_class = (GObjectClass *) klass;
  gstelement_class = (GstElementClass *) klass;
  filter_class = (GstVideoFilterClass *) klass;

  gobject_class->set_property = gst_line_21_encoder_set_property;
  gobject_class->get_property = gst_line_21_encoder_get_property;

  /**
   * line21encoder:remove-caption-meta
   *
   * Selects whether the encoded #GstVideoCaptionMeta should be removed from
   * the outgoing video buffers or whether it should be kept.
   *
   * Since: 1.20
   */
  g_object_class_install_property (G_OBJECT_CLASS (klass),
      PROP_REMOVE_CAPTION_META, g_param_spec_boolean ("remove-caption-meta",
          "Remove Caption Meta",
          "Remove encoded caption meta from outgoing video buffers", FALSE,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));


  gst_element_class_set_static_metadata (gstelement_class,
      "Line 21 CC Encoder",
      "Filter/Video/ClosedCaption",
      "Inject line21 CC in SD video streams",
      "Mathieu Duponchelle <mathieu@centricular.com>");

  gst_element_class_add_static_pad_template (gstelement_class, &sinktemplate);
  gst_element_class_add_static_pad_template (gstelement_class, &srctemplate);

  filter_class->set_info = gst_line_21_encoder_set_info;
  filter_class->transform_frame_ip = gst_line_21_encoder_transform_ip;

  GST_DEBUG_CATEGORY_INIT (gst_line_21_encoder_debug, "line21encoder",
      0, "Line 21 CC Encoder");
  vbi_initialize_gst_debug ();
}

static void
gst_line_21_encoder_init (GstLine21Encoder * filter)
{
}

static void
gst_line_21_encoder_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstLine21Encoder *enc = GST_LINE21ENCODER (object);

  switch (prop_id) {
    case PROP_REMOVE_CAPTION_META:
      enc->remove_caption_meta = g_value_get_boolean (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_line_21_encoder_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstLine21Encoder *enc = GST_LINE21ENCODER (object);

  switch (prop_id) {
    case PROP_REMOVE_CAPTION_META:
      g_value_set_boolean (value, enc->remove_caption_meta);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static vbi_pixfmt
vbi_pixfmt_from_gst_video_format (GstVideoFormat format)
{
  switch (format) {
    case GST_VIDEO_FORMAT_I420:
      return VBI_PIXFMT_YUV420;
    case GST_VIDEO_FORMAT_YUY2:
      return VBI_PIXFMT_YUYV;
    case GST_VIDEO_FORMAT_YVYU:
      return VBI_PIXFMT_YVYU;
    case GST_VIDEO_FORMAT_UYVY:
      return VBI_PIXFMT_UYVY;
    case GST_VIDEO_FORMAT_VYUY:
      return VBI_PIXFMT_VYUY;
    default:
      g_assert_not_reached ();
      return (vbi_pixfmt) 0;
  }
#undef NATIVE_VBI_FMT
}

static gboolean
gst_line_21_encoder_set_info (GstVideoFilter * filter,
    GstCaps * incaps, GstVideoInfo * in_info,
    GstCaps * outcaps, GstVideoInfo * out_info)
{
  GstLine21Encoder *self = GST_LINE21ENCODER (filter);

  self->info = *in_info;

  /*
   * Set up blank / black / white levels fit for NTSC, no actual relation
   * with the height of the video
   */
  self->sp.scanning = 525;
  /* The pixel format */
  self->sp.sampling_format =
      vbi_pixfmt_from_gst_video_format (GST_VIDEO_INFO_FORMAT (&self->info));
  /* Sampling rate. For BT.601 it's 13.5MHz */
  self->sp.sampling_rate = 13.5e6;
  /* Stride */
  self->sp.bytes_per_line = GST_VIDEO_INFO_COMP_STRIDE (&self->info, 0);
  /* Horizontal offset of the VBI image */
  self->sp.offset = 122;

  /* FIXME: magic numbers */
  self->sp.start[0] = 21;
  self->sp.count[0] = 1;
  self->sp.start[1] = 284;
  self->sp.count[1] = 1;

  self->sp.interlaced = FALSE;
  self->sp.synchronous = TRUE;

  return TRUE;
}

#define MAX_CDP_PACKET_LEN 256
#define MAX_CEA608_LEN 32

/* Converts CDP into raw CEA708 cc_data, taken from ccconverter */
static guint
convert_cea708_cdp_cea708_cc_data_internal (GstLine21Encoder * self,
    const guint8 * cdp, guint cdp_len, guint8 cc_data[256])
{
  GstByteReader br;
  guint16 u16;
  guint8 u8;
  guint8 flags;
  guint len = 0;

  /* Header + footer length */
  if (cdp_len < 11) {
    GST_WARNING_OBJECT (self, "cdp packet too short (%u). expected at "
        "least %u", cdp_len, 11);
    return 0;
  }

  gst_byte_reader_init (&br, cdp, cdp_len);
  u16 = gst_byte_reader_get_uint16_be_unchecked (&br);
  if (u16 != 0x9669) {
    GST_WARNING_OBJECT (self, "cdp packet does not have initial magic bytes "
        "of 0x9669");
    return 0;
  }

  u8 = gst_byte_reader_get_uint8_unchecked (&br);
  if (u8 != cdp_len) {
    GST_WARNING_OBJECT (self, "cdp packet length (%u) does not match passed "
        "in value (%u)", u8, cdp_len);
    return 0;
  }

  gst_byte_reader_skip_unchecked (&br, 1);

  flags = gst_byte_reader_get_uint8_unchecked (&br);
  /* No cc_data? */
  if ((flags & 0x40) == 0) {
    GST_DEBUG_OBJECT (self, "cdp packet does have any cc_data");
    return 0;
  }

  /* cdp_hdr_sequence_cntr */
  gst_byte_reader_skip_unchecked (&br, 2);

  /* time_code_present */
  if (flags & 0x80) {
    if (gst_byte_reader_get_remaining (&br) < 5) {
      GST_WARNING_OBJECT (self, "cdp packet does not have enough data to "
          "contain a timecode (%u). Need at least 5 bytes",
          gst_byte_reader_get_remaining (&br));
      return 0;
    }

    gst_byte_reader_skip_unchecked (&br, 5);
  }

  /* ccdata_present */
  if (flags & 0x40) {
    guint8 cc_count;

    if (gst_byte_reader_get_remaining (&br) < 2) {
      GST_WARNING_OBJECT (self, "not enough data to contain valid cc_data");
      return 0;
    }
    u8 = gst_byte_reader_get_uint8_unchecked (&br);
    if (u8 != 0x72) {
      GST_WARNING_OBJECT (self, "missing cc_data start code of 0x72, "
          "found 0x%02x", u8);
      return 0;
    }

    cc_count = gst_byte_reader_get_uint8_unchecked (&br);
    if ((cc_count & 0xe0) != 0xe0) {
      GST_WARNING_OBJECT (self, "reserved bits are not 0xe0, found 0x%02x", u8);
      return 0;
    }
    cc_count &= 0x1f;

    len = 3 * cc_count;
    if (gst_byte_reader_get_remaining (&br) < len)
      return 0;

    memcpy (cc_data, gst_byte_reader_get_data_unchecked (&br, len), len);
  }

  /* skip everything else we don't care about */
  return len;
}

#define VAL_OR_0(v) ((v) ? (*(v)) : 0)

static guint
compact_cc_data (guint8 * cc_data, guint cc_data_len)
{
  gboolean started_ccp = FALSE;
  guint out_len = 0;
  guint i;

  if (cc_data_len % 3 != 0) {
    GST_WARNING ("Invalid cc_data buffer size");
    cc_data_len = cc_data_len - (cc_data_len % 3);
  }

  for (i = 0; i < cc_data_len / 3; i++) {
    gboolean cc_valid = (cc_data[i * 3] & 0x04) == 0x04;
    guint8 cc_type = cc_data[i * 3] & 0x03;

    if (!started_ccp && (cc_type == 0x00 || cc_type == 0x01)) {
      if (cc_valid) {
        /* copy over valid 608 data */
        cc_data[out_len++] = cc_data[i * 3];
        cc_data[out_len++] = cc_data[i * 3 + 1];
        cc_data[out_len++] = cc_data[i * 3 + 2];
      }
      continue;
    }

    if (cc_type & 0x10)
      started_ccp = TRUE;

    if (!cc_valid)
      continue;

    if (cc_type == 0x00 || cc_type == 0x01) {
      GST_WARNING ("Invalid cc_data.  cea608 bytes after cea708");
      return 0;
    }

    cc_data[out_len++] = cc_data[i * 3];
    cc_data[out_len++] = cc_data[i * 3 + 1];
    cc_data[out_len++] = cc_data[i * 3 + 2];
  }

  GST_LOG ("compacted cc_data from %u to %u", cc_data_len, out_len);

  return out_len;
}

static gint
cc_data_extract_cea608 (guint8 * cc_data, guint cc_data_len,
    guint8 * cea608_field1, guint * cea608_field1_len,
    guint8 * cea608_field2, guint * cea608_field2_len)
{
  guint i, field_1_len = 0, field_2_len = 0;

  if (cea608_field1_len) {
    field_1_len = *cea608_field1_len;
    *cea608_field1_len = 0;
  }
  if (cea608_field2_len) {
    field_2_len = *cea608_field2_len;
    *cea608_field2_len = 0;
  }

  if (cc_data_len % 3 != 0) {
    GST_WARNING ("Invalid cc_data buffer size %u. Truncating to a multiple "
        "of 3", cc_data_len);
    cc_data_len = cc_data_len - (cc_data_len % 3);
  }

  for (i = 0; i < cc_data_len / 3; i++) {
    gboolean cc_valid = (cc_data[i * 3] & 0x04) == 0x04;
    guint8 cc_type = cc_data[i * 3] & 0x03;

    GST_TRACE ("0x%02x 0x%02x 0x%02x, valid: %u, type: 0b%u%u",
        cc_data[i * 3 + 0], cc_data[i * 3 + 1], cc_data[i * 3 + 2], cc_valid,
        cc_type & 0x2, cc_type & 0x1);

    if (cc_type == 0x00) {
      if (!cc_valid)
        continue;

      if (cea608_field1 && cea608_field1_len) {
        if (*cea608_field1_len + 2 > field_1_len) {
          GST_WARNING ("Too many cea608 input bytes %u for field 1",
              *cea608_field1_len + 2);
          return -1;
        }
        cea608_field1[(*cea608_field1_len)++] = cc_data[i * 3 + 1];
        cea608_field1[(*cea608_field1_len)++] = cc_data[i * 3 + 2];
      }
    } else if (cc_type == 0x01) {
      if (!cc_valid)
        continue;

      if (cea608_field2 && cea608_field2_len) {
        if (*cea608_field2_len + 2 > field_2_len) {
          GST_WARNING ("Too many cea608 input bytes %u for field 2",
              *cea608_field2_len + 2);
          return -1;
        }
        cea608_field2[(*cea608_field2_len)++] = cc_data[i * 3 + 1];
        cea608_field2[(*cea608_field2_len)++] = cc_data[i * 3 + 2];
      }
    } else {
      /* all cea608 packets must be at the beginning of a cc_data */
      break;
    }
  }

  g_assert_cmpint (i * 3, <=, cc_data_len);

  GST_LOG ("Extracted cea608-1 of length %u and cea608-2 of length %u",
      VAL_OR_0 (cea608_field1_len), VAL_OR_0 (cea608_field2_len));

  return i * 3;
}

static GstFlowReturn
gst_line_21_encoder_transform_ip (GstVideoFilter * filter,
    GstVideoFrame * frame)
{
  GstLine21Encoder *self = GST_LINE21ENCODER (filter);
  GstVideoCaptionMeta *cc_meta;
  guint8 *buf;
  vbi_sliced sliced[2];
  gpointer iter = NULL;
  GstFlowReturn ret = GST_FLOW_ERROR;
  guint offset;

  sliced[0].id = VBI_SLICED_CAPTION_525_F1;
  sliced[0].line = self->sp.start[0];
  sliced[1].id = VBI_SLICED_CAPTION_525_F2;
  sliced[1].line = self->sp.start[1];

  sliced[0].data[0] = 0x80;
  sliced[0].data[1] = 0x80;
  sliced[1].data[0] = 0x80;
  sliced[1].data[1] = 0x80;

  /* We loop over caption metas until we find the first CEA608 meta */
  while ((cc_meta = (GstVideoCaptionMeta *)
          gst_buffer_iterate_meta_filtered (frame->buffer, &iter,
              GST_VIDEO_CAPTION_META_API_TYPE))) {
    guint n = cc_meta->size;
    guint i;

    if (cc_meta->caption_type == GST_VIDEO_CAPTION_TYPE_CEA708_CDP) {
      guint8 cc_data[MAX_CDP_PACKET_LEN];
      guint8 cea608_field1[MAX_CEA608_LEN];
      guint8 cea608_field2[MAX_CEA608_LEN];
      guint cea608_field1_len = MAX_CEA608_LEN;
      guint cea608_field2_len = MAX_CEA608_LEN;
      guint cc_data_len = 0;

      cc_data_len =
          convert_cea708_cdp_cea708_cc_data_internal (self, cc_meta->data,
          cc_meta->size, cc_data);

      cc_data_len = compact_cc_data (cc_data, cc_data_len);

      cc_data_extract_cea608 (cc_data, cc_data_len,
          cea608_field1, &cea608_field1_len, cea608_field2, &cea608_field2_len);

      if (cea608_field1_len == 2) {
        sliced[0].data[0] = cea608_field1[0];
        sliced[0].data[1] = cea608_field1[1];
      }

      if (cea608_field2_len == 2) {
        sliced[1].data[0] = cea608_field2[0];
        sliced[1].data[1] = cea608_field2[1];
      }

      break;
    } else if (cc_meta->caption_type == GST_VIDEO_CAPTION_TYPE_CEA608_S334_1A) {
      if (n % 3 != 0) {
        GST_ERROR_OBJECT (filter, "Invalid S334-1A CEA608 buffer size");
        goto done;
      }

      n /= 3;

      if (n >= 3) {
        GST_ERROR_OBJECT (filter, "Too many S334-1A CEA608 triplets %u", n);
        goto done;
      }

      for (i = 0; i < n; i++) {
        if (cc_meta->data[i * 3] & 0x80) {
          sliced[0].data[0] = cc_meta->data[i * 3 + 1];
          sliced[0].data[1] = cc_meta->data[i * 3 + 2];
        } else {
          sliced[1].data[0] = cc_meta->data[i * 3 + 1];
          sliced[1].data[1] = cc_meta->data[i * 3 + 2];
        }
      }

      break;
    }
  }

  /* We've encoded this meta, it can now be removed if required */
  if (cc_meta && self->remove_caption_meta)
    gst_buffer_remove_meta (frame->buffer, (GstMeta *) cc_meta);

  /* When dealing with standard NTSC resolution, field 1 goes at line 21,
   * when dealing with a reduced height the image has 3 VBI lines at the
   * top and 3 at the bottom, and field 1 goes at line 1 */
  offset = self->info.height == 525 ? 21 : 1;

  buf =
      (guint8 *) GST_VIDEO_FRAME_PLANE_DATA (frame,
      0) + offset * GST_VIDEO_INFO_COMP_STRIDE (&self->info, 0);

  if (!vbi_raw_video_image (buf, GST_VIDEO_INFO_COMP_STRIDE (&self->info,
              0) * 2, &self->sp, 0, 0, 0, 0x000000FF, 0, sliced, 2)) {
    GST_ERROR_OBJECT (filter, "Failed to encode CC data");
    goto done;
  }

  ret = GST_FLOW_OK;

done:
  return ret;
}
