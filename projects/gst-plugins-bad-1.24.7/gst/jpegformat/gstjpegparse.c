/* GStreamer
 *
 * jpegparse: a parser for JPEG streams
 *
 * Copyright (C) <2009> Arnout Vandecappelle (Essensium/Mind) <arnout@mind.be>
 *               <2022> Víctor Manuel Jáquez Leal <vjaquez@igalia.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

/**
 * SECTION:element-jpegparse
 * @title: jpegparse
 * @short_description: JPEG parser
 *
 * Parses a JPEG stream into JPEG images.  It looks for EOI boundaries to
 * split a continuous stream into single-frame buffers. Also reads the
 * image header searching for image properties such as width and height
 * among others. Jpegparse can also extract metadata (e.g. xmp).
 *
 * ## Example launch line
 * |[
 * gst-launch-1.0 -v souphttpsrc location=... ! jpegparse ! matroskamux ! filesink location=...
 * ]|
 * The above pipeline fetches a motion JPEG stream from an IP camera over
 * HTTP and stores it in a matroska file.
 *
 */
/* FIXME: output plain JFIF APP marker only. This provides best code reuse.
 * JPEG decoders would not need to handle this part anymore. Also when remuxing
 * (... ! jpegparse ! ... ! jifmux ! ...) metadata consolidation would be
 * easier.
 */

/* TODO:
 *  + APP2 -- ICC color profile
 *  + APP3 -- meta (same as exif)
 *  + APP12 -- Photoshop Save for Web: Ducky / Picture info
 *  + APP13 -- Adobe IRB
 *  + check for interlaced mjpeg
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>
#include <gst/base/gstbytereader.h>
#include <gst/codecparsers/gstjpegparser.h>
#include <gst/tag/tag.h>

#include "gstjpegparse.h"

enum ParserState
{
  GST_JPEG_PARSER_STATE_GOT_SOI = 1 << 0,
  GST_JPEG_PARSER_STATE_GOT_SOF = 1 << 1,
  GST_JPEG_PARSER_STATE_GOT_SOS = 1 << 2,
  GST_JPEG_PARSER_STATE_GOT_JFIF = 1 << 3,
  GST_JPEG_PARSER_STATE_GOT_ADOBE = 1 << 4,

  GST_JPEG_PARSER_STATE_VALID_PICTURE = (GST_JPEG_PARSER_STATE_GOT_SOI |
      GST_JPEG_PARSER_STATE_GOT_SOF | GST_JPEG_PARSER_STATE_GOT_SOS),
};

static GstStaticPadTemplate gst_jpeg_parse_src_pad_template =
GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("image/jpeg, "
        "framerate = (fraction) [ 0/1, MAX ], " "parsed = (boolean) true")
    );

static GstStaticPadTemplate gst_jpeg_parse_sink_pad_template =
GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("image/jpeg")
    );

GST_DEBUG_CATEGORY_STATIC (jpeg_parse_debug);
#define GST_CAT_DEFAULT jpeg_parse_debug

static GstFlowReturn
gst_jpeg_parse_handle_frame (GstBaseParse * bparse, GstBaseParseFrame * frame,
    gint * skipsize);
static gboolean gst_jpeg_parse_set_sink_caps (GstBaseParse * parse,
    GstCaps * caps);
static gboolean gst_jpeg_parse_sink_event (GstBaseParse * parse,
    GstEvent * event);
static gboolean gst_jpeg_parse_start (GstBaseParse * parse);
static gboolean gst_jpeg_parse_stop (GstBaseParse * parse);

#define gst_jpeg_parse_parent_class parent_class
G_DEFINE_TYPE (GstJpegParse, gst_jpeg_parse, GST_TYPE_BASE_PARSE);
GST_ELEMENT_REGISTER_DEFINE (jpegparse, "jpegparse", GST_RANK_PRIMARY,
    GST_TYPE_JPEG_PARSE);

enum GstJPEGColorspace
{
  GST_JPEG_COLORSPACE_NONE,
  GST_JPEG_COLORSPACE_RGB,
  GST_JPEG_COLORSPACE_YUV,
  GST_JPEG_COLORSPACE_GRAY,
  GST_JPEG_COLORSPACE_CMYK,
  GST_JPEG_COLORSPACE_YCCK,
};

static const gchar *gst_jpeg_colorspace_strings[] = {
  [GST_JPEG_COLORSPACE_NONE] = NULL,
  [GST_JPEG_COLORSPACE_RGB] = "sRGB",
  [GST_JPEG_COLORSPACE_YUV] = "sYUV",
  [GST_JPEG_COLORSPACE_GRAY] = "GRAY",
  [GST_JPEG_COLORSPACE_CMYK] = "CMYK",
  [GST_JPEG_COLORSPACE_YCCK] = "YCCK",
};

enum GstJPEGSampling
{
  GST_JPEG_SAMPLING_NONE,
  GST_JPEG_SAMPLING_RGB,
  GST_JPEG_SAMPLING_BGR,
  GST_JPEG_SAMPLING_YBR444,
  GST_JPEG_SAMPLING_YBR422,
  GST_JPEG_SAMPLING_YBR420,
  GST_JPEG_SAMPLING_YBR440,
  GST_JPEG_SAMPLING_YBR410,
  GST_JPEG_SAMPLING_YBR411,
  GST_JPEG_SAMPLING_GRAYSCALE,
};

static const gchar *gst_jpeg_sampling_strings[] = {
  [GST_JPEG_SAMPLING_NONE] = NULL,
  [GST_JPEG_SAMPLING_RGB] = "RGB",
  [GST_JPEG_SAMPLING_BGR] = "BGR",
  [GST_JPEG_SAMPLING_YBR444] = "YCbCr-4:4:4",
  [GST_JPEG_SAMPLING_YBR422] = "YCbCr-4:2:2",
  [GST_JPEG_SAMPLING_YBR420] = "YCbCr-4:2:0",
  [GST_JPEG_SAMPLING_YBR440] = "YCbCr-4:4:0",
  [GST_JPEG_SAMPLING_YBR410] = "YCbCr-4:1:0",
  [GST_JPEG_SAMPLING_YBR411] = "YCbCr-4:1:1",
  [GST_JPEG_SAMPLING_GRAYSCALE] = "GRAYSCALE",
};

static void
gst_jpeg_parse_class_init (GstJpegParseClass * klass)
{
  GstBaseParseClass *gstbaseparse_class;
  GstElementClass *gstelement_class;

  gstbaseparse_class = (GstBaseParseClass *) klass;
  gstelement_class = (GstElementClass *) klass;

  gstbaseparse_class->start = gst_jpeg_parse_start;
  gstbaseparse_class->stop = gst_jpeg_parse_stop;
  gstbaseparse_class->set_sink_caps = gst_jpeg_parse_set_sink_caps;
  gstbaseparse_class->sink_event = gst_jpeg_parse_sink_event;
  gstbaseparse_class->handle_frame = gst_jpeg_parse_handle_frame;

  gst_element_class_add_static_pad_template (gstelement_class,
      &gst_jpeg_parse_src_pad_template);
  gst_element_class_add_static_pad_template (gstelement_class,
      &gst_jpeg_parse_sink_pad_template);

  gst_element_class_set_static_metadata (gstelement_class,
      "JPEG stream parser",
      "Codec/Parser/Image",
      "Parse JPEG images into single-frame buffers",
      "Víctor Jáquez <vjaquez@igalia.com>");

  GST_DEBUG_CATEGORY_INIT (jpeg_parse_debug, "jpegparse", 0, "JPEG parser");
}

static void
gst_jpeg_parse_init (GstJpegParse * parse)
{
  parse->sof = -1;
}

static void
parse_avid (GstJpegParse * parse, const guint8 * data, guint16 len)
{
  parse->avid = 1;
  if (len > 14 && data[12] == 1)        /* 1 - NTSC */
    parse->field_order = GST_VIDEO_FIELD_ORDER_BOTTOM_FIELD_FIRST;
  if (len > 14 && data[12] == 2)        /* 2 - PAL */
    parse->field_order = GST_VIDEO_FIELD_ORDER_TOP_FIELD_FIRST;
  GST_INFO_OBJECT (parse, "AVID: %s",
      gst_video_field_order_to_string (parse->field_order));
}

static gboolean
gst_jpeg_parse_set_sink_caps (GstBaseParse * bparse, GstCaps * caps)
{
  GstJpegParse *parse = GST_JPEG_PARSE_CAST (bparse);
  GstStructure *s = gst_caps_get_structure (caps, 0);
  const GValue *codec_data;
  const char *interlace_mode, *field_order;

  GST_DEBUG_OBJECT (parse, "get sink caps %" GST_PTR_FORMAT, caps);

  gst_structure_get_fraction (s, "framerate",
      &parse->framerate_numerator, &parse->framerate_denominator);

  gst_structure_get_int (s, "height", &parse->orig_height);
  gst_structure_get_int (s, "width", &parse->orig_width);

  gst_structure_get_fraction (s, "pixel-aspect-ration", &parse->par_num,
      &parse->par_den);

  codec_data = gst_structure_get_value (s, "codec_data");
  if (codec_data && G_VALUE_TYPE (codec_data) == GST_TYPE_BUFFER) {
    GstMapInfo map;

    gst_clear_buffer (&parse->codec_data);

    parse->codec_data = GST_BUFFER (g_value_dup_boxed (codec_data));
    if (gst_buffer_map (parse->codec_data, &map, GST_MAP_READ)) {
      if (map.size > 8 && map.data[0] == 0x2c && map.data[4] == 0x18)
        parse_avid (parse, map.data, map.size);
      gst_buffer_unmap (parse->codec_data, &map);
    }
  }

  interlace_mode = gst_structure_get_string (s, "interlace-mode");
  if (interlace_mode) {
    parse->interlace_mode =
        gst_video_interlace_mode_from_string (interlace_mode);
  }

  if (parse->interlace_mode != GST_VIDEO_INTERLACE_MODE_PROGRESSIVE) {
    field_order = gst_structure_get_string (s, "field-order");
    if (field_order)
      parse->field_order = gst_video_field_order_from_string (field_order);
  }

  g_clear_pointer (&parse->colorimetry, g_free);
  parse->colorimetry = g_strdup (gst_structure_get_string (s, "colorimetry"));

  return TRUE;
}

static inline gboolean
valid_state (guint state, guint ref_state)
{
  return (state & ref_state) == ref_state;
}

/* https://zpl.fi/chroma-subsampling-and-jpeg-sampling-factors/ */
/* *INDENT-OFF* */
static const struct
{
  gint h[3];
  gint v[3];
  enum GstJPEGSampling sampling;
} subsampling_map[] = {
  {{1, 1, 1}, {1, 1, 1}, GST_JPEG_SAMPLING_YBR444},
  {{2, 2, 2}, {1, 1, 1}, GST_JPEG_SAMPLING_YBR444},
  {{3, 3, 3}, {1, 1, 1}, GST_JPEG_SAMPLING_YBR444},
  {{1, 1, 1}, {2, 2, 2}, GST_JPEG_SAMPLING_YBR444},
  {{1, 1, 1}, {3, 3, 3}, GST_JPEG_SAMPLING_YBR444},
  {{1, 1, 1}, {2, 1, 1}, GST_JPEG_SAMPLING_YBR440},
  {{2, 2, 2}, {2, 1, 1}, GST_JPEG_SAMPLING_YBR440},
  {{1, 1, 1}, {4, 2, 2}, GST_JPEG_SAMPLING_YBR440},
  {{2, 1, 1}, {1, 1, 1}, GST_JPEG_SAMPLING_YBR422},
  {{2, 1, 1}, {2, 2, 2}, GST_JPEG_SAMPLING_YBR422},
  {{4, 2, 2}, {1, 1, 1}, GST_JPEG_SAMPLING_YBR422},
  {{2, 1, 1}, {2, 1, 1}, GST_JPEG_SAMPLING_YBR420},
  {{4, 1, 1}, {1, 1, 1}, GST_JPEG_SAMPLING_YBR411},
  {{4, 1, 1}, {2, 1, 1}, GST_JPEG_SAMPLING_YBR410},
};
/* *INDENT-ON* */

static guint16
yuv_sampling (GstJpegFrameHdr * frame_hdr)
{
  int i, h0, h1, h2, v0, v1, v2;

  g_return_val_if_fail (frame_hdr->num_components == 3, GST_JPEG_SAMPLING_NONE);

  h0 = frame_hdr->components[0].horizontal_factor;
  h1 = frame_hdr->components[1].horizontal_factor;
  h2 = frame_hdr->components[2].horizontal_factor;
  v0 = frame_hdr->components[0].vertical_factor;
  v1 = frame_hdr->components[1].vertical_factor;
  v2 = frame_hdr->components[2].vertical_factor;

  for (i = 0; i < G_N_ELEMENTS (subsampling_map); i++) {
    if (subsampling_map[i].h[0] == h0
        && subsampling_map[i].h[1] == h1 && subsampling_map[i].h[2] == h2
        && subsampling_map[i].v[0] == v0
        && subsampling_map[i].v[1] == v1 && subsampling_map[i].v[2] == v2)
      return subsampling_map[i].sampling;
  }

  return GST_JPEG_SAMPLING_NONE;
}

static const gchar *
colorspace_to_string (enum GstJPEGColorspace colorspace)
{
  return gst_jpeg_colorspace_strings[colorspace];
}

/* https://entropymine.wordpress.com/2018/10/22/how-is-a-jpeg-images-color-type-determined/ */
/* T-REC-T.872-201206  6.1 Colour encodings and associated values to define white and black */
static gboolean
gst_jpeg_parse_sof (GstJpegParse * parse, GstJpegSegment * seg)
{
  GstJpegFrameHdr hdr = { 0, };
  guint colorspace;
  guint sampling;

  if (!gst_jpeg_segment_parse_frame_header (seg, &hdr)) {
    return FALSE;
  }

  colorspace = GST_JPEG_COLORSPACE_NONE;
  sampling = GST_JPEG_SAMPLING_NONE;

  switch (hdr.num_components) {
    case 1:
      colorspace = GST_JPEG_COLORSPACE_GRAY;
      sampling = GST_JPEG_SAMPLING_GRAYSCALE;
      break;
    case 3:
      if (valid_state (parse->state, GST_JPEG_PARSER_STATE_GOT_JFIF)) {
        colorspace = GST_JPEG_COLORSPACE_YUV;
        sampling = yuv_sampling (&hdr);
      } else {
        if (valid_state (parse->state, GST_JPEG_PARSER_STATE_GOT_ADOBE)) {
          if (parse->adobe_transform == 0) {
            colorspace = GST_JPEG_COLORSPACE_RGB;
            sampling = GST_JPEG_SAMPLING_RGB;
          } else if (parse->adobe_transform == 1) {
            colorspace = GST_JPEG_COLORSPACE_YUV;;
            sampling = yuv_sampling (&hdr);
          } else {
            GST_DEBUG_OBJECT (parse, "Unknown Adobe color transform code");
            colorspace = GST_JPEG_COLORSPACE_YUV;;
            sampling = yuv_sampling (&hdr);
          }
        } else {
          int cid0, cid1, cid2;

          cid0 = hdr.components[0].identifier;
          cid1 = hdr.components[1].identifier;
          cid2 = hdr.components[2].identifier;

          if (cid0 == 1 && cid1 == 2 && cid2 == 3) {
            colorspace = GST_JPEG_COLORSPACE_YUV;
            sampling = yuv_sampling (&hdr);
          } else if (cid0 == 'R' && cid1 == 'G' && cid2 == 'B') {
            colorspace = GST_JPEG_COLORSPACE_RGB;
            sampling = GST_JPEG_SAMPLING_RGB;
          } else {
            GST_DEBUG_OBJECT (parse, "Unrecognized component IDs");
            colorspace = GST_JPEG_COLORSPACE_YUV;
            sampling = yuv_sampling (&hdr);
          }
        }
      }
      break;
    case 4:
      if (valid_state (parse->state, GST_JPEG_PARSER_STATE_GOT_ADOBE)) {
        if (parse->adobe_transform == 0) {
          colorspace = GST_JPEG_COLORSPACE_CMYK;
        } else if (parse->adobe_transform == 2) {
          colorspace = GST_JPEG_COLORSPACE_YCCK;
        } else {
          GST_DEBUG_OBJECT (parse, "Unknown Adobe color transform code");
          colorspace = GST_JPEG_COLORSPACE_YCCK;
        }
      } else {
        colorspace = GST_JPEG_COLORSPACE_CMYK;
      }
      break;
    default:
      GST_WARNING_OBJECT (parse, "Unknown color space");
      break;
  }

  if (hdr.width != parse->width || hdr.height != parse->height
      || colorspace != parse->colorspace || sampling != parse->sampling) {
    parse->width = hdr.width;
    parse->height = hdr.height;
    parse->colorspace = colorspace;
    parse->sampling = sampling;

    if (parse->first_picture && !parse->multiscope) {
      if (parse->orig_height > 0
          && parse->height < ((parse->orig_height * 3) / 4)) {
        parse->interlace_mode = GST_VIDEO_INTERLACE_MODE_INTERLEAVED;
      } else if (parse->avid) {
        /* if no container info, let's suppose it doubles its height */
        if (parse->orig_height == 0)
          parse->orig_height = 2 * hdr.height;
        parse->interlace_mode = GST_VIDEO_INTERLACE_MODE_INTERLEAVED;
      }
    }

    parse->first_picture = FALSE;
    parse->renegotiate = TRUE;
  }

  GST_INFO_OBJECT (parse, "SOF [%dx%d] %d comp - %s", parse->width,
      parse->height, hdr.num_components,
      GST_STR_NULL (colorspace_to_string (parse->colorspace)));
  return TRUE;
}

static inline GstTagList *
get_tag_list (GstJpegParse * parse)
{
  if (!parse->tags)
    parse->tags = gst_tag_list_new_empty ();
  return parse->tags;
}

static gboolean
gst_jpeg_parse_app0 (GstJpegParse * parse, GstJpegSegment * seg)
{
  GstByteReader reader;
  guint16 xd, yd;
  guint8 unit, xt, yt;
  guint32 id;

  if (seg->size < 6)            /* less than 6 means no id string */
    return FALSE;

  gst_byte_reader_init (&reader, seg->data + seg->offset, seg->size);
  gst_byte_reader_skip_unchecked (&reader, 2);

#if G_BYTE_ORDER == G_LITTLE_ENDIAN
  if (!gst_byte_reader_get_uint32_le (&reader, &id))
    return FALSE;
#else
  if (!gst_byte_reader_get_uint32_be (&reader, &id))
    return FALSE;
#endif

  if (!valid_state (parse->state, GST_JPEG_PARSER_STATE_GOT_JFIF)
      && GST_MAKE_FOURCC ('J', 'F', 'I', 'F') == id) {

    parse->state |= GST_JPEG_PARSER_STATE_GOT_JFIF;

    /* trailing zero-byte */
    gst_byte_reader_skip_unchecked (&reader, 1);

    /* version */
    gst_byte_reader_skip_unchecked (&reader, 2);

    /* units */
    if (!gst_byte_reader_get_uint8 (&reader, &unit))
      return FALSE;

    /* x density */
#if G_BYTE_ORDER == G_LITTLE_ENDIAN
    if (!gst_byte_reader_get_uint16_le (&reader, &xd))
      return FALSE;
    /* y density */
    if (!gst_byte_reader_get_uint16_le (&reader, &yd))
      return FALSE;
#else
    if (!gst_byte_reader_get_uint16_be (&reader, &xd))
      return FALSE;
    /* y density */
    if (!gst_byte_reader_get_uint16_be (&reader, &yd))
      return FALSE;
#endif

    /* x thumbnail */
    if (!gst_byte_reader_get_uint8 (&reader, &xt))
      return FALSE;
    /* y thumbnail */
    if (!gst_byte_reader_get_uint8 (&reader, &yt))
      return FALSE;

    if (unit == 0) {
      /* no units, X and Y specify the pixel aspect ratio */
      if (parse->par_num != xd || parse->par_den != yd) {
        parse->renegotiate = TRUE;
        parse->par_num = xd;
        parse->par_den = yd;
      }
    } else if (unit == 1 || unit == 2) {
      /* tag pixel per inches */
      double hppi = xd, vppi = yd;

      /* cm to in */
      if (unit == 2) {
        hppi *= 2.54;
        vppi *= 2.54;
      }

      gst_tag_register_musicbrainz_tags ();
      gst_tag_list_add (get_tag_list (parse), GST_TAG_MERGE_REPLACE,
          GST_TAG_IMAGE_HORIZONTAL_PPI, hppi, GST_TAG_IMAGE_VERTICAL_PPI, vppi,
          NULL);
    }

    if (xt > 0 && yt > 0)
      GST_FIXME_OBJECT (parse, "embedded thumbnail ignored");

    return TRUE;
  }

  /* JFIF  Extension  */
  if (GST_MAKE_FOURCC ('J', 'F', 'X', 'X') == id) {
    if (!valid_state (parse->state, GST_JPEG_PARSER_STATE_GOT_JFIF))
      return FALSE;

    return TRUE;
  }

  /* https://exiftool.org/TagNames/JPEG.html#AVI1 */
  if (GST_MAKE_FOURCC ('A', 'V', 'I', '1') == id) {
    /* polarity */
    if (!gst_byte_reader_get_uint8 (&reader, &unit))
      return FALSE;

    parse->avid = (unit > 0);   /* otherwise is not interleaved */

    /* TODO: update caps for interlaced MJPEG */
    GST_DEBUG_OBJECT (parse, "MJPEG interleaved field: %s", unit == 0 ?
        "not interleaved" : unit % 2 ? "Odd" : "Even");

    return TRUE;
  }

  GST_MEMDUMP_OBJECT (parse, "Unhandled app0", seg->data + seg->offset,
      seg->size);

  return TRUE;
}

/* *INDENT-OFF* */
static const struct
{
  const gchar *suffix;
  GstTagList *(*tag_func) (GstBuffer * buff);
} TagMap[] = {
  {"Exif", gst_tag_list_from_exif_buffer_with_tiff_header},
  {"http://ns.adobe.com/xap/1.0/", gst_tag_list_from_xmp_buffer},
};
/* *INDENT-ON* */

static gboolean
gst_jpeg_parse_app1 (GstJpegParse * parse, GstJpegSegment * seg)
{
  GstByteReader reader;
  GstBuffer *buf;
  guint16 size = 0;
  const gchar *id_str;
  const guint8 *data;
  gint i;

  if (seg->size < 6)            /* less than 6 means no id string */
    return FALSE;

  gst_byte_reader_init (&reader, seg->data + seg->offset, seg->size);
  gst_byte_reader_skip_unchecked (&reader, 2);

  if (!gst_byte_reader_get_string_utf8 (&reader, &id_str))
    return FALSE;

  for (i = 0; i < G_N_ELEMENTS (TagMap); i++) {
    if (!g_str_has_suffix (id_str, TagMap[i].suffix))
      continue;

    /* skip NUL only for Exif */
    if (i == 0) {
      if (!gst_byte_reader_skip (&reader, 1))
        return FALSE;
    }

    size = gst_byte_reader_get_remaining (&reader);

    if (!gst_byte_reader_get_data (&reader, size, &data))
      return FALSE;

    buf = gst_buffer_new_wrapped_full (GST_MEMORY_FLAG_READONLY,
        (gpointer) data, size, 0, size, NULL, NULL);

    if (buf) {
      GstTagList *tags;

      tags = TagMap[i].tag_func (buf);
      gst_buffer_unref (buf);

      if (tags) {
        GST_LOG_OBJECT (parse, "parsed marker %x: '%s' %" GST_PTR_FORMAT,
            GST_JPEG_MARKER_APP1, id_str, tags);
        gst_tag_list_insert (get_tag_list (parse), tags, GST_TAG_MERGE_REPLACE);
        gst_tag_list_unref (tags);
      } else {
        GST_INFO_OBJECT (parse, "failed to parse %s: %s", id_str, data);
        return FALSE;
      }
    }

    return TRUE;
  }

  GST_MEMDUMP_OBJECT (parse, "Unhandled app1", seg->data + seg->offset,
      seg->size);

  return TRUE;
}

static gboolean
gst_jpeg_parse_app14 (GstJpegParse * parse, GstJpegSegment * seg)
{
  GstByteReader reader;
  const gchar *id_str;
  guint8 transform;

  if (seg->size < 6)            /* less than 6 means no id string */
    return FALSE;

  gst_byte_reader_init (&reader, seg->data + seg->offset, seg->size);
  gst_byte_reader_skip_unchecked (&reader, 2);

  if (!gst_byte_reader_get_string_utf8 (&reader, &id_str))
    return FALSE;

  if (!g_str_has_prefix (id_str, "Adobe")) {
    GST_DEBUG_OBJECT (parse, "Unhandled app14: %s", id_str);
    return TRUE;
  }

  /* skip version and flags */
  if (!gst_byte_reader_skip (&reader, 6))
    return FALSE;

  parse->state |= GST_JPEG_PARSER_STATE_GOT_ADOBE;

  /* transform bit might not exist  */
  if (!gst_byte_reader_get_uint8 (&reader, &transform))
    return TRUE;

  parse->adobe_transform = transform;
  return TRUE;
}

static inline gchar *
get_utf8_from_data (const guint8 * data, guint16 size)
{
  const gchar *env_vars[] = { "GST_JPEG_TAG_ENCODING",
    "GST_TAG_ENCODING", NULL
  };
  const char *str = (gchar *) data;
  char *ret;

  ret = gst_tag_freeform_string_to_utf8 (str, size, env_vars);
  if (!ret)
    GST_MEMDUMP ("non-parsed marker data", data, size);

  return ret;
}

/* read comment and post as tag */
static inline gboolean
gst_jpeg_parse_com (GstJpegParse * parse, GstJpegSegment * seg)
{
  GstByteReader reader;
  const guint8 *data = NULL;
  guint16 size;
  const gchar *buf;

  gst_byte_reader_init (&reader, seg->data + seg->offset, seg->size);
  gst_byte_reader_skip_unchecked (&reader, 2);

  size = gst_byte_reader_get_remaining (&reader);
  if (!gst_byte_reader_get_data (&reader, size, &data))
    return FALSE;

  buf = (const gchar *) data;
  /* buggy avid, it puts EOI only at every 10th frame */
  if (g_str_has_prefix (buf, "AVID")) {
    parse_avid (parse, data, size);
  } else if (g_str_has_prefix (buf, "MULTISCOPE II")) {
    parse->par_num = 1;
    parse->par_den = 2;
    parse->multiscope = TRUE;
  } else {
    gchar *comment;

    comment = get_utf8_from_data (data, size);
    if (!comment)
      return FALSE;

    GST_INFO_OBJECT (parse, "comment found: %s", comment);
    gst_tag_list_add (get_tag_list (parse), GST_TAG_MERGE_REPLACE,
        GST_TAG_COMMENT, comment, NULL);
    g_free (comment);
  }

  return TRUE;
}

/* reset per image */
static void
gst_jpeg_parse_reset (GstJpegParse * parse)
{
  parse->last_offset = 0;
  parse->state = 0;
  parse->adobe_transform = 0;
  parse->field = 0;

  if (parse->tags) {
    gst_tag_list_unref (parse->tags);
    parse->tags = NULL;
  }
}

static const gchar *
sampling_to_string (enum GstJPEGSampling sampling)
{
  return gst_jpeg_sampling_strings[sampling];
}

static gboolean
gst_jpeg_parse_set_new_caps (GstJpegParse * parse)
{
  GstCaps *caps;
  GstEvent *event;
  gboolean res;

  if (!parse->renegotiate)
    return TRUE;

  caps = gst_caps_new_simple ("image/jpeg", "parsed", G_TYPE_BOOLEAN, TRUE,
      NULL);

  if (parse->width > 0)
    gst_caps_set_simple (caps, "width", G_TYPE_INT, parse->width, NULL);
  if (parse->orig_height > 0 && parse->orig_height > parse->height)
    gst_caps_set_simple (caps, "height", G_TYPE_INT, parse->orig_height, NULL);
  else if (parse->height > 0)
    gst_caps_set_simple (caps, "height", G_TYPE_INT, parse->height, NULL);
  if (parse->sof >= 0)
    gst_caps_set_simple (caps, "sof-marker", G_TYPE_INT, parse->sof, NULL);
  if (parse->colorspace != GST_JPEG_COLORSPACE_NONE) {
    gst_caps_set_simple (caps, "colorspace", G_TYPE_STRING,
        colorspace_to_string (parse->colorspace), NULL);
  }
  if (parse->sampling != GST_JPEG_SAMPLING_NONE) {
    gst_caps_set_simple (caps, "sampling", G_TYPE_STRING,
        sampling_to_string (parse->sampling), NULL);
  }

  if (parse->colorimetry) {
    gst_caps_set_simple (caps, "colorimetry", G_TYPE_STRING, parse->colorimetry,
        NULL);
  }

  gst_caps_set_simple (caps, "interlace-mode", G_TYPE_STRING,
      gst_video_interlace_mode_to_string (parse->interlace_mode), NULL);

  if (parse->interlace_mode == GST_VIDEO_INTERLACE_MODE_INTERLEAVED) {
    gst_caps_set_simple (caps, "field-order", G_TYPE_STRING,
        gst_video_field_order_to_string (parse->field_order), NULL);
  }

  gst_caps_set_simple (caps, "framerate", GST_TYPE_FRACTION,
      parse->framerate_numerator, parse->framerate_denominator, NULL);

  if (parse->par_num > 0 && parse->par_den > 0) {
    gst_caps_set_simple (caps, "pixel-aspect-ratio", GST_TYPE_FRACTION,
        parse->par_num, parse->par_den, NULL);
  }

  if (parse->codec_data) {
    gst_caps_set_simple (caps, "codec_data", GST_TYPE_BUFFER, parse->codec_data,
        NULL);
  }

  parse->renegotiate = FALSE;

  GST_DEBUG_OBJECT (parse,
      "setting downstream caps on %s:%s to %" GST_PTR_FORMAT,
      GST_DEBUG_PAD_NAME (GST_BASE_PARSE_SRC_PAD (parse)), caps);

  event = gst_event_new_caps (caps);
  res = gst_pad_push_event (GST_BASE_PARSE_SRC_PAD (parse), event);
  gst_caps_unref (caps);

  return res;
}

static GstFlowReturn
gst_jpeg_parse_finish_frame (GstJpegParse * parse, GstBaseParseFrame * frame,
    gint size)
{
  GstBaseParse *bparse = GST_BASE_PARSE (parse);
  GstFlowReturn ret;

  if (parse->tags)
    gst_base_parse_merge_tags (bparse, parse->tags, GST_TAG_MERGE_REPLACE);

  if (!gst_jpeg_parse_set_new_caps (parse))
    return GST_FLOW_ERROR;

  if (!valid_state (parse->state, GST_JPEG_PARSER_STATE_VALID_PICTURE)) {
    /* this validation breaks unit tests */
    /* frame->flags |= GST_BASE_PARSE_FRAME_FLAG_DROP; */
    GST_WARNING_OBJECT (parse, "Potentially invalid picture");
  }

  GST_TRACE_OBJECT (parse, "Finish frame %" GST_PTR_FORMAT, frame->buffer);
  ret = gst_base_parse_finish_frame (bparse, frame, size);

  gst_jpeg_parse_reset (parse);

  return ret;
}

static GstFlowReturn
gst_jpeg_parse_handle_frame (GstBaseParse * bparse, GstBaseParseFrame * frame,
    gint * skipsize)
{
  GstJpegParse *parse = GST_JPEG_PARSE_CAST (bparse);
  GstMapInfo mapinfo;
  GstJpegMarker marker;
  GstJpegSegment seg;
  guint offset;

  GST_TRACE_OBJECT (parse, "frame %" GST_PTR_FORMAT, frame->buffer);

  if (!gst_buffer_map (frame->buffer, &mapinfo, GST_MAP_READ))
    return GST_FLOW_ERROR;

  offset = parse->last_offset;
  if (offset > 0)
    offset -= 1;                /* it migth be in the middle marker */

  while (offset < mapinfo.size) {
    if (!gst_jpeg_parse (&seg, mapinfo.data, mapinfo.size, offset)) {
      if (!valid_state (parse->state, GST_JPEG_PARSER_STATE_GOT_SOI)) {
        /* Skip any garbage until SOI */
        *skipsize = mapinfo.size;
        GST_INFO_OBJECT (parse, "skipping %d bytes", *skipsize);
      } else {
        /* Accept anything after SOI */
        parse->last_offset = mapinfo.size;
      }
      goto beach;
    }

    offset = seg.offset;
    marker = seg.marker;

    if (!valid_state (parse->state, GST_JPEG_PARSER_STATE_GOT_SOI)
        && marker != GST_JPEG_MARKER_SOI)
      continue;

    /* check if the whole segment is available */
    if (offset + seg.size > mapinfo.size) {
      GST_DEBUG_OBJECT (parse, "incomplete segment: %x [offset %d]", marker,
          offset);
      parse->last_offset = offset - 2;
      goto beach;
    }

    offset += seg.size;

    GST_LOG_OBJECT (parse, "marker found: %x [offset %d / size %"
        G_GSSIZE_FORMAT "]", marker, seg.offset, seg.size);

    switch (marker) {
      case GST_JPEG_MARKER_SOI:
        /* This means that new SOI comes without an previous EOI. */
        if (offset > 2
            && (parse->interlace_mode == GST_VIDEO_INTERLACE_MODE_PROGRESSIVE
                || parse->field == 0)) {
          /* If already some data segment parsed, push it as a frame. */
          if (valid_state (parse->state, GST_JPEG_PARSER_STATE_GOT_SOS)) {
            gst_buffer_unmap (frame->buffer, &mapinfo);

            frame->out_buffer = gst_buffer_copy_region (frame->buffer,
                GST_BUFFER_COPY_ALL, 0, seg.offset - 2);
            GST_MINI_OBJECT_FLAGS (frame->out_buffer) |=
                GST_BUFFER_FLAG_CORRUPTED;

            GST_WARNING_OBJECT (parse, "Push a frame without EOI, size %d",
                seg.offset - 2);
            return gst_jpeg_parse_finish_frame (parse, frame, seg.offset - 2);
          }

          gst_jpeg_parse_reset (parse);
          parse->state |= GST_JPEG_PARSER_STATE_GOT_SOI;
          /* unset tags */
          gst_base_parse_merge_tags (bparse, NULL, GST_TAG_MERGE_UNDEFINED);

          *skipsize = offset - 2;
          GST_DEBUG_OBJECT (parse, "skipping %d bytes before SOI", *skipsize);
          parse->last_offset = 2;
          goto beach;
        }

        /* unset tags */
        gst_base_parse_merge_tags (bparse, NULL, GST_TAG_MERGE_UNDEFINED);
        parse->state |= GST_JPEG_PARSER_STATE_GOT_SOI;
        break;
      case GST_JPEG_MARKER_EOI:
        if (parse->interlace_mode == GST_VIDEO_INTERLACE_MODE_PROGRESSIVE
            || parse->field == 1) {
          gst_buffer_unmap (frame->buffer, &mapinfo);
          return gst_jpeg_parse_finish_frame (parse, frame, seg.offset);
        } else if (parse->interlace_mode == GST_VIDEO_INTERLACE_MODE_INTERLEAVED
            && parse->field == 0) {
          parse->field = 1;
          parse->state = 0;
        }
        break;
      case GST_JPEG_MARKER_SOS:
        if (!valid_state (parse->state, GST_JPEG_PARSER_STATE_GOT_SOF))
          GST_WARNING_OBJECT (parse, "SOS marker without SOF one");
        parse->state |= GST_JPEG_PARSER_STATE_GOT_SOS;
        break;
      case GST_JPEG_MARKER_COM:
        if (!gst_jpeg_parse_com (parse, &seg))
          GST_WARNING_OBJECT (parse, "Failed to parse com segment");
        break;
      case GST_JPEG_MARKER_APP0:
        if (!gst_jpeg_parse_app0 (parse, &seg))
          GST_WARNING_OBJECT (parse, "Failed to parse app0 segment");
        break;
      case GST_JPEG_MARKER_APP1:
        if (!gst_jpeg_parse_app1 (parse, &seg))
          GST_WARNING_OBJECT (parse, "Failed to parse app1 segment");
        break;
      case GST_JPEG_MARKER_APP14:
        if (!gst_jpeg_parse_app14 (parse, &seg))
          GST_WARNING_OBJECT (parse, "Failed to parse app14 segment");
        break;
      case GST_JPEG_MARKER_DHT:
      case GST_JPEG_MARKER_DAC:
        /* to avoid break the below SOF interval */
        break;
      default:
        /* SOFn segments */
        if (marker >= GST_JPEG_MARKER_SOF_MIN &&
            marker <= GST_JPEG_MARKER_SOF_MAX) {
          if (!valid_state (parse->state, GST_JPEG_PARSER_STATE_GOT_SOF)
              && gst_jpeg_parse_sof (parse, &seg)) {
            gint8 sof;

            parse->state |= GST_JPEG_PARSER_STATE_GOT_SOF;
            sof = marker - 0xc0;
            if (parse->sof != sof) {
              parse->sof = sof;
              parse->renegotiate = TRUE;
            }
          } else {
            GST_ELEMENT_ERROR (parse, STREAM, FORMAT,
                ("Invalid data"), ("Duplicated or bad SOF marker"));
            gst_buffer_unmap (frame->buffer, &mapinfo);
            gst_jpeg_parse_reset (parse);
            return GST_FLOW_ERROR;
          }
        }
        break;
    }
  }

  parse->last_offset = offset;

beach:
  gst_buffer_unmap (frame->buffer, &mapinfo);
  return GST_FLOW_OK;
}

static gboolean
gst_jpeg_parse_sink_event (GstBaseParse * bparse, GstEvent * event)
{
  GstJpegParse *parse = GST_JPEG_PARSE_CAST (bparse);

  GST_DEBUG_OBJECT (parse, "event : %s", GST_EVENT_TYPE_NAME (event));

  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_FLUSH_STOP:
      gst_jpeg_parse_reset (parse);
      break;
    default:
      break;
  }

  return GST_BASE_PARSE_CLASS (parent_class)->sink_event (bparse, event);
}

static gboolean
gst_jpeg_parse_start (GstBaseParse * bparse)
{
  GstJpegParse *parse = GST_JPEG_PARSE_CAST (bparse);

  parse->framerate_numerator = 0;
  parse->framerate_denominator = 1;

  parse->first_picture = TRUE;
  parse->renegotiate = TRUE;

  parse->par_num = parse->par_den = 1;

  parse->interlace_mode = GST_VIDEO_INTERLACE_MODE_PROGRESSIVE;
  parse->field_order = GST_VIDEO_FIELD_ORDER_TOP_FIELD_FIRST;

  gst_jpeg_parse_reset (parse);

  gst_base_parse_set_min_frame_size (bparse, 2);

  return TRUE;
}

static gboolean
gst_jpeg_parse_stop (GstBaseParse * bparse)
{
  GstJpegParse *parse = GST_JPEG_PARSE_CAST (bparse);

  if (parse->tags) {
    gst_tag_list_unref (parse->tags);
    parse->tags = NULL;
  }
  gst_clear_buffer (&parse->codec_data);
  gst_clear_caps (&parse->prev_caps);
  g_clear_pointer (&parse->colorimetry, g_free);

  return TRUE;
}
