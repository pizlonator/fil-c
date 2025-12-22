/* GStreamer JPEG 2000 Parser
 * Copyright (C) <2016-2017> Grok Image Compression Inc.
 *  @author Aaron Boxer <boxerab@gmail.com>
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
#  include "config.h"
#endif

#include "gstvideoparserselements.h"
#include "gstjpeg2000parse.h"
#include <gst/base/base.h>

/* Not used at the moment
static gboolean gst_jpeg2000_parse_is_cinema(guint16 rsiz)   {
	return ((rsiz >= GST_JPEG2000_PARSE_PROFILE_CINEMA_2K) && (rsiz <= GST_JPEG2000_PARSE_PROFILE_CINEMA_S4K));
}
static gboolean gst_jpeg2000_parse_is_storage(guint16 rsiz)   {
	return (rsiz == GST_JPEG2000_PARSE_PROFILE_CINEMA_LTS);
}
*/
static gboolean
gst_jpeg2000_parse_is_broadcast (guint16 rsiz)
{
  return ((rsiz >= GST_JPEG2000_PARSE_PROFILE_BC_SINGLE) &&
      (rsiz <= ((GST_JPEG2000_PARSE_PROFILE_BC_MULTI_R) | (0x000b)))
      && ((rsiz & (~GST_JPEG2000_PARSE_PROFILE_BC_MASK)) == 0));
}

static gboolean
gst_jpeg2000_parse_is_imf (guint16 rsiz)
{
  return ((rsiz >= GST_JPEG2000_PARSE_PROFILE_IMF_2K)
      && (rsiz <= ((GST_JPEG2000_PARSE_PROFILE_IMF_8K_R) | (0x009b))));
}

static gboolean
gst_jpeg2000_parse_is_part_2 (guint16 rsiz)
{
  return (rsiz & GST_JPEG2000_PARSE_PROFILE_PART2);
}



static void
gst_jpeg2000_parse_get_subsampling (guint16 compno,
    GstJPEG2000Sampling sampling, guint8 * dx, guint8 * dy)
{
  *dx = 1;
  *dy = 1;
  if (compno == 1 || compno == 2) {
    if (sampling == GST_JPEG2000_SAMPLING_YBR422) {
      *dx = 2;
    } else if (sampling == GST_JPEG2000_SAMPLING_YBR420) {
      *dx = 2;
      *dy = 2;
    } else if (sampling == GST_JPEG2000_SAMPLING_YBR411) {
      *dx = 4;
      *dy = 1;
    } else if (sampling == GST_JPEG2000_SAMPLING_YBR410) {
      *dx = 4;
      *dy = 4;
    }
  }
}

#define GST_JPEG2000_JP2_SIZE_OF_BOX_ID  	4
#define GST_JPEG2000_JP2_SIZE_OF_BOX_LEN	4
#define GST_JPEG2000_MARKER_SIZE  	4


/* J2C has 8 bytes preceding J2K magic: 4 for size of box, and 4 for fourcc */
#define GST_JPEG2000_PARSE_SIZE_OF_J2C_PREFIX_BYTES (GST_JPEG2000_JP2_SIZE_OF_BOX_LEN +  GST_JPEG2000_JP2_SIZE_OF_BOX_ID)

/* SOC marker plus minimum size of SIZ marker */
#define GST_JPEG2000_PARSE_MIN_FRAME_SIZE (GST_JPEG2000_MARKER_SIZE + GST_JPEG2000_PARSE_SIZE_OF_J2C_PREFIX_BYTES + 36)

#define GST_JPEG2000_PARSE_J2K_MAGIC 0xFF4FFF51
#define GST_JPEG2000_PARSE_J2C_BOX_ID 0x6a703263        /* "jp2c" */

GST_DEBUG_CATEGORY (jpeg2000_parse_debug);
#define GST_CAT_DEFAULT jpeg2000_parse_debug

static GstStaticPadTemplate srctemplate =
    GST_STATIC_PAD_TEMPLATE ("src", GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("image/x-jpc,"
        " width = (int)[1, MAX], height = (int)[1, MAX],"
        GST_JPEG2000_SAMPLING_LIST ","
        GST_JPEG2000_COLORSPACE_LIST ","
        " profile = (int)[0, 49151],"
        " parsed = (boolean) true;"
        "image/x-j2c,"
        " width = (int)[1, MAX], height = (int)[1, MAX],"
        GST_JPEG2000_SAMPLING_LIST ","
        GST_JPEG2000_COLORSPACE_LIST ","
        " profile = (int)[0, 49151],"
        " parsed = (boolean) true ; "
        "image/x-jpc-striped,"
        " width = (int)[1, MAX], height = (int)[1, MAX],"
        GST_JPEG2000_SAMPLING_LIST ","
        GST_JPEG2000_COLORSPACE_LIST ","
        " profile = (int)[0, 49151],"
        " num-stripes = [ 2, MAX ], parsed = (boolean) true;")
    );

static GstStaticPadTemplate sinktemplate =
    GST_STATIC_PAD_TEMPLATE ("sink", GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("image/jp2; image/x-jpc; image/x-j2c; "
        "image/x-jpc-striped"));

#define parent_class gst_jpeg2000_parse_parent_class
G_DEFINE_TYPE (GstJPEG2000Parse, gst_jpeg2000_parse, GST_TYPE_BASE_PARSE);
GST_ELEMENT_REGISTER_DEFINE_WITH_CODE (jpeg2000parse, "jpeg2000parse",
    GST_RANK_PRIMARY, GST_TYPE_JPEG2000_PARSE,
    videoparsers_element_init (plugin));

static gboolean gst_jpeg2000_parse_start (GstBaseParse * parse);
static gboolean gst_jpeg2000_parse_event (GstBaseParse * parse,
    GstEvent * event);
static void gst_jpeg2000_parse_reset (GstBaseParse * parse,
    gboolean hard_reset);
static GstFlowReturn gst_jpeg2000_parse_handle_frame (GstBaseParse * parse,
    GstBaseParseFrame * frame, gint * skipsize);
static GstFlowReturn gst_jpeg2000_parse_pre_push_frame (GstBaseParse * parse,
    GstBaseParseFrame * frame);
static gboolean gst_jpeg2000_parse_set_sink_caps (GstBaseParse * parse,
    GstCaps * caps);
static GstJPEG2000ParseFormats
format_from_media_type (const GstStructure * structure);

static void
gst_jpeg2000_parse_class_init (GstJPEG2000ParseClass * klass)
{
  GstElementClass *gstelement_class = GST_ELEMENT_CLASS (klass);
  GstBaseParseClass *parse_class = GST_BASE_PARSE_CLASS (klass);

  GST_DEBUG_CATEGORY_INIT (jpeg2000_parse_debug, "jpeg2000parse", 0,
      "jpeg 2000 parser");

  gst_element_class_add_static_pad_template (gstelement_class, &srctemplate);
  gst_element_class_add_static_pad_template (gstelement_class, &sinktemplate);
  gst_element_class_set_static_metadata (gstelement_class, "JPEG 2000 parser",
      "Codec/Parser/Video/Image",
      "Parses JPEG 2000 files", "Aaron Boxer <boxerab@gmail.com>");

  /* Override BaseParse vfuncs */
  parse_class->set_sink_caps =
      GST_DEBUG_FUNCPTR (gst_jpeg2000_parse_set_sink_caps);
  parse_class->start = GST_DEBUG_FUNCPTR (gst_jpeg2000_parse_start);
  parse_class->sink_event = GST_DEBUG_FUNCPTR (gst_jpeg2000_parse_event);
  parse_class->handle_frame =
      GST_DEBUG_FUNCPTR (gst_jpeg2000_parse_handle_frame);
  parse_class->pre_push_frame =
      GST_DEBUG_FUNCPTR (gst_jpeg2000_parse_pre_push_frame);
}

static void
gst_jpeg2000_parse_reset (GstBaseParse * parse, gboolean hard_reset)
{
  GstJPEG2000Parse *jpeg2000parse = GST_JPEG2000_PARSE (parse);

  jpeg2000parse->parsed_j2c_box = FALSE;
  jpeg2000parse->frame_size = 0;
  if (hard_reset) {
    jpeg2000parse->width = 0;
    jpeg2000parse->height = 0;
    jpeg2000parse->sampling = GST_JPEG2000_SAMPLING_NONE;
    jpeg2000parse->colorspace = GST_JPEG2000_COLORSPACE_NONE;
    jpeg2000parse->src_codec_format = GST_JPEG2000_PARSE_NO_CODEC;
    jpeg2000parse->sink_codec_format = GST_JPEG2000_PARSE_NO_CODEC;
  }
}

static gboolean
gst_jpeg2000_parse_start (GstBaseParse * parse)
{
  GstJPEG2000Parse *jpeg2000parse = GST_JPEG2000_PARSE (parse);
  GST_DEBUG_OBJECT (jpeg2000parse, "start");
  gst_base_parse_set_min_frame_size (parse, GST_JPEG2000_PARSE_MIN_FRAME_SIZE);
  gst_jpeg2000_parse_reset (parse, TRUE);

  return TRUE;
}


static void
gst_jpeg2000_parse_init (GstJPEG2000Parse * jpeg2000parse)
{
  GST_PAD_SET_ACCEPT_INTERSECT (GST_BASE_PARSE_SINK_PAD (jpeg2000parse));
  GST_PAD_SET_ACCEPT_TEMPLATE (GST_BASE_PARSE_SINK_PAD (jpeg2000parse));
}

static gboolean
gst_jpeg2000_parse_set_sink_caps (GstBaseParse * parse, GstCaps * caps)
{
  GstJPEG2000Parse *jpeg2000parse = GST_JPEG2000_PARSE (parse);
  GstStructure *caps_struct = gst_caps_get_structure (caps, 0);

  gst_jpeg2000_parse_reset (parse, TRUE);
  jpeg2000parse->sink_codec_format = format_from_media_type (caps_struct);

  return TRUE;
}

static gboolean
gst_jpeg2000_parse_event (GstBaseParse * parse, GstEvent * event)
{
  gboolean res;
  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_FLUSH_STOP:
      gst_base_parse_set_min_frame_size (parse,
          GST_JPEG2000_PARSE_MIN_FRAME_SIZE);
      res = GST_BASE_PARSE_CLASS (parent_class)->sink_event (parse, event);
      break;
    default:
      res = GST_BASE_PARSE_CLASS (parent_class)->sink_event (parse, event);
      break;
  }
  return res;
}

static GstJPEG2000ParseFormats
format_from_media_type (const GstStructure * structure)
{
  const char *media_type = gst_structure_get_name (structure);
  if (!strcmp (media_type, "image/x-j2c"))
    return GST_JPEG2000_PARSE_J2C;
  if (!strcmp (media_type, "image/x-jpc"))
    return GST_JPEG2000_PARSE_JPC;
  if (!strcmp (media_type, "image/jp2"))
    return GST_JPEG2000_PARSE_JP2;
  return GST_JPEG2000_PARSE_NO_CODEC;
}

/* check downstream caps to configure media type */
static gboolean
gst_jpeg2000_parse_negotiate (GstJPEG2000Parse * parse, GstCaps * in_caps)
{
  GstCaps *caps;
  guint codec_format = GST_JPEG2000_PARSE_NO_CODEC;

  if (in_caps != NULL && !gst_caps_is_fixed (in_caps))
    return FALSE;

  caps = gst_pad_get_allowed_caps (GST_BASE_PARSE_SRC_PAD (parse));
  GST_DEBUG_OBJECT (parse, "allowed caps: %" GST_PTR_FORMAT, caps);

  /* concentrate on leading structure, since decodebin parser
   * capsfilter always includes parser template caps */
  if (caps) {
    caps = gst_caps_truncate (caps);
    GST_DEBUG_OBJECT (parse, "negotiating with caps: %" GST_PTR_FORMAT, caps);
  }

  if (in_caps && caps) {
    if (gst_caps_can_intersect (in_caps, caps)) {
      GST_DEBUG_OBJECT (parse, "downstream accepts upstream caps");
      codec_format =
          format_from_media_type (gst_caps_get_structure (in_caps, 0));
      gst_caps_unref (caps);
      caps = NULL;
    }
  }

  if (caps && !gst_caps_is_empty (caps)) {
    /* fixate to avoid ambiguity with lists when parsing */
    caps = gst_caps_fixate (caps);
    codec_format = format_from_media_type (gst_caps_get_structure (caps, 0));
  }
  if (caps)
    gst_caps_unref (caps);

  GST_DEBUG_OBJECT (parse, "selected codec format %d", codec_format);
  parse->src_codec_format = codec_format;

  return codec_format != GST_JPEG2000_PARSE_NO_CODEC;
}

static const char *
media_type_from_codec_format (GstJPEG2000ParseFormats f)
{
  switch (f) {
    case GST_JPEG2000_PARSE_J2C:
      return "image/x-j2c";
    case GST_JPEG2000_PARSE_JP2:
      return "image/jp2";
    case GST_JPEG2000_PARSE_JPC:
      return "image/x-jpc";
    default:
      g_assert_not_reached ();
      return "invalid/x-invalid";
  }
}

static GstFlowReturn
gst_jpeg2000_parse_handle_frame (GstBaseParse * parse,
    GstBaseParseFrame * frame, gint * skipsize)
{
  GstJPEG2000Parse *jpeg2000parse = GST_JPEG2000_PARSE (parse);
  GstMapInfo map;
  GstByteReader reader;
  GstFlowReturn ret = GST_FLOW_OK;
  guint eoc_offset = 0;
  GstCaps *current_caps = NULL;
  GstStructure *current_caps_struct = NULL;
  GstJPEG2000Colorspace colorspace = GST_JPEG2000_COLORSPACE_NONE;
  guint x0 = 0, y0 = 0, x1 = 0, y1 = 0;
  guint width = 0, height = 0;
  guint i;
  guint8 dx[GST_JPEG2000_PARSE_MAX_SUPPORTED_COMPONENTS];
  guint8 dy[GST_JPEG2000_PARSE_MAX_SUPPORTED_COMPONENTS];
  guint16 numcomps = 0;
  guint16 capabilities = 0;
  guint16 profile = 0;
  gboolean validate_main_level = FALSE;
  guint8 main_level = 0;
  guint8 sub_level = 0;
  guint16 compno = 0;
  GstJPEG2000Sampling parsed_sampling = GST_JPEG2000_SAMPLING_NONE;
  const gchar *sink_sampling_string = NULL;
  GstJPEG2000Sampling sink_sampling = GST_JPEG2000_SAMPLING_NONE;
  GstJPEG2000Sampling source_sampling = GST_JPEG2000_SAMPLING_NONE;
  guint num_prefix_bytes = 0;   /* number of bytes to skip before actual code stream */
  GstCaps *src_caps = NULL;
  guint eoc_frame_size = 0;
  gint num_stripes = 1;
  gint stripe_height = 0;

  for (i = 0; i < GST_JPEG2000_PARSE_MAX_SUPPORTED_COMPONENTS; ++i) {
    dx[i] = 1;
    dy[i] = 1;
  }

  if (!gst_buffer_map (frame->buffer, &map, GST_MAP_READ)) {
    GST_ERROR_OBJECT (jpeg2000parse, "Unable to map buffer");
    return GST_FLOW_ERROR;
  }
  gst_byte_reader_init (&reader, map.data, map.size);
  current_caps = gst_pad_get_current_caps (GST_BASE_PARSE_SINK_PAD (parse));

  /* Parse J2C box */
  if (!jpeg2000parse->parsed_j2c_box) {
    gboolean has_j2c_box = FALSE;
    gboolean is_j2c_src;
    guint j2c_box_id_offset = -1;
    guint magic_offset = -1;

    /* Look for magic. If not found, get more data */
    magic_offset = gst_byte_reader_masked_scan_uint32_peek (&reader, 0xffffffff,
        GST_JPEG2000_PARSE_J2K_MAGIC, 0,
        gst_byte_reader_get_remaining (&reader), NULL);
    if (magic_offset == -1)
      goto beach;
    GST_DEBUG_OBJECT (jpeg2000parse, "Found magic at offset = %d",
        magic_offset);

    if (magic_offset > 0) {
      j2c_box_id_offset =
          gst_byte_reader_masked_scan_uint32_peek (&reader, 0xffffffff,
          GST_JPEG2000_PARSE_J2C_BOX_ID, 0, magic_offset, NULL);
      has_j2c_box = j2c_box_id_offset != -1;
      /* sanity check on box id offset */
      if (has_j2c_box) {
        if (j2c_box_id_offset + GST_JPEG2000_JP2_SIZE_OF_BOX_ID != magic_offset
            || j2c_box_id_offset < GST_JPEG2000_JP2_SIZE_OF_BOX_LEN) {
          GST_ELEMENT_ERROR (jpeg2000parse, STREAM, DECODE, NULL,
              ("Corrupt contiguous code stream box for j2c stream"));
          ret = GST_FLOW_ERROR;
          goto beach;
        }
        /* read the box length */
        if (!gst_byte_reader_skip (&reader,
                j2c_box_id_offset - GST_JPEG2000_JP2_SIZE_OF_BOX_LEN))
          goto beach;
        if (!gst_byte_reader_get_uint32_be (&reader,
                &jpeg2000parse->frame_size))
          goto beach;
      }
    }
    jpeg2000parse->parsed_j2c_box = TRUE;

    /* determine downstream j2k format */
    if (jpeg2000parse->src_codec_format == GST_JPEG2000_PARSE_NO_CODEC) {
      if (!gst_jpeg2000_parse_negotiate (jpeg2000parse, current_caps)) {
        ret = GST_FLOW_NOT_NEGOTIATED;
        goto beach;
      }
    }

    /* treat JP2 as J2C */
    if (jpeg2000parse->src_codec_format == GST_JPEG2000_PARSE_JP2)
      jpeg2000parse->src_codec_format = GST_JPEG2000_PARSE_J2C;
    is_j2c_src = jpeg2000parse->src_codec_format == GST_JPEG2000_PARSE_J2C;
    /* we can't convert JPC to any other format */
    if (!has_j2c_box && is_j2c_src) {
      ret = GST_FLOW_NOT_NEGOTIATED;
      goto beach;
    }
    /* sanity check on sink caps */
    if (jpeg2000parse->sink_codec_format > GST_JPEG2000_PARSE_J2C
        && !has_j2c_box) {
      GST_ELEMENT_ERROR (jpeg2000parse, STREAM, DECODE, NULL,
          ("Expected J2C box but found none."));
      ret = GST_FLOW_ERROR;
      goto beach;
    }
    /* adjust frame size for JPC src caps */
    if (jpeg2000parse->frame_size &&
        jpeg2000parse->src_codec_format == GST_JPEG2000_PARSE_JPC) {
      jpeg2000parse->frame_size -=
          GST_JPEG2000_JP2_SIZE_OF_BOX_LEN + GST_JPEG2000_JP2_SIZE_OF_BOX_ID;
    }
    /* see if we need to skip any bytes at beginning of frame */
    *skipsize = magic_offset;
    if (is_j2c_src)
      *skipsize -= GST_JPEG2000_PARSE_SIZE_OF_J2C_PREFIX_BYTES;
    if (*skipsize > 0)
      goto beach;
    /* reset reader to beginning of buffer */
    gst_byte_reader_set_pos (&reader, 0);
  }

  /* we keep prefix bytes but skip them in order
   * to process the rest of the frame */
  /* magic prefix */
  num_prefix_bytes = GST_JPEG2000_MARKER_SIZE;
  /* J2C box prefix */
  if (jpeg2000parse->src_codec_format == GST_JPEG2000_PARSE_J2C) {
    num_prefix_bytes +=
        GST_JPEG2000_JP2_SIZE_OF_BOX_LEN + GST_JPEG2000_JP2_SIZE_OF_BOX_ID;
  }
  /* bail out if not enough data for code stream */
  if (jpeg2000parse->frame_size &&
      (gst_byte_reader_get_size (&reader) < jpeg2000parse->frame_size))
    goto beach;

  /* skip prefix and 2 bytes for marker size */
  if (!gst_byte_reader_skip (&reader, num_prefix_bytes + 2))
    goto beach;

  if (!gst_byte_reader_get_uint16_be (&reader, &capabilities))
    goto beach;

  profile = capabilities & GST_JPEG2000_PARSE_PROFILE_MASK;
  if (!gst_jpeg2000_parse_is_part_2 (capabilities)) {
    if ((profile > GST_JPEG2000_PARSE_PROFILE_CINEMA_LTS)
        && !gst_jpeg2000_parse_is_broadcast (profile)
        && !gst_jpeg2000_parse_is_imf (profile)) {
      GST_ELEMENT_ERROR (jpeg2000parse, STREAM, DECODE, NULL,
          ("Unrecognized JPEG 2000 profile %d", profile));
      ret = GST_FLOW_ERROR;
      goto beach;
    }
    if (gst_jpeg2000_parse_is_broadcast (profile)) {
      main_level = capabilities & 0xF;
      validate_main_level = TRUE;
    } else if (gst_jpeg2000_parse_is_imf (profile)) {
      main_level = capabilities & 0xF;
      validate_main_level = TRUE;
      sub_level = (capabilities >> 4) & 0xF;
      if (sub_level > 9) {
        GST_ELEMENT_ERROR (jpeg2000parse, STREAM, DECODE, NULL,
            ("Sub level %d is invalid", sub_level));
        ret = GST_FLOW_ERROR;
        goto beach;
      }
    }
    if (validate_main_level && main_level > 11) {
      GST_ELEMENT_ERROR (jpeg2000parse, STREAM, DECODE, NULL,
          ("Main level %d is invalid", main_level));
      ret = GST_FLOW_ERROR;
      goto beach;

    }
  }


  if (!gst_byte_reader_get_uint32_be (&reader, &x1))
    goto beach;

  if (!gst_byte_reader_get_uint32_be (&reader, &y1))
    goto beach;

  if (!gst_byte_reader_get_uint32_be (&reader, &x0))
    goto beach;

  if (!gst_byte_reader_get_uint32_be (&reader, &y0))
    goto beach;

  /* sanity check on image dimensions */
  if (x1 < x0 || y1 < y0) {
    GST_ELEMENT_ERROR (jpeg2000parse, STREAM, DECODE, NULL,
        ("Nonsensical image dimensions %d,%d,%d,%d", x0, y0, x1, y1));
    ret = GST_FLOW_ERROR;
    goto beach;
  }

  width = x1 - x0;
  height = y1 - y0;

  GST_DEBUG_OBJECT (jpeg2000parse, "Parsed image dimensions %d,%d", width,
      height);

  /* skip tile dimensions */
  if (!gst_byte_reader_skip (&reader, 4 * 4))
    goto beach;

  /* read number of components */
  if (!gst_byte_reader_get_uint16_be (&reader, &numcomps))
    goto beach;

  if (numcomps == 0 || numcomps > GST_JPEG2000_PARSE_MAX_SUPPORTED_COMPONENTS) {
    GST_ELEMENT_ERROR (jpeg2000parse, STREAM, DECODE, NULL,
        ("Unsupported number of components %d", numcomps));
    ret = GST_FLOW_NOT_NEGOTIATED;
    goto beach;
  }
  if (current_caps) {
    const gchar *colorspace_string = NULL;
    current_caps_struct = gst_caps_get_structure (current_caps, 0);
    if (!current_caps_struct) {
      GST_ERROR_OBJECT (jpeg2000parse,
          "Unable to get structure of current caps struct");
      ret = GST_FLOW_NOT_NEGOTIATED;
      goto beach;
    }

    colorspace_string = gst_structure_get_string
        (current_caps_struct, "colorspace");
    if (colorspace_string)
      colorspace = gst_jpeg2000_colorspace_from_string (colorspace_string);
    sink_sampling_string = gst_structure_get_string
        (current_caps_struct, "sampling");
    if (sink_sampling_string)
      sink_sampling = gst_jpeg2000_sampling_from_string (sink_sampling_string);

  }

  if (colorspace == GST_JPEG2000_COLORSPACE_NONE) {
    /* guess color space based on number of components       */
    if (numcomps == 0 || numcomps > 4) {
      GST_ERROR_OBJECT (jpeg2000parse,
          "Unable to guess color space from number of components %d", numcomps);
      ret = GST_FLOW_NOT_NEGOTIATED;
      goto beach;
    }
    colorspace =
        (numcomps >=
        3) ? GST_JPEG2000_COLORSPACE_RGB : GST_JPEG2000_COLORSPACE_GRAY;
    if (numcomps == 4) {
      GST_WARNING_OBJECT (jpeg2000parse, "No caps available: assuming RGBA");
    } else if (numcomps == 3) {
      GST_WARNING_OBJECT (jpeg2000parse, "No caps available: assuming RGB");
    } else if (numcomps == 2) {
      GST_WARNING_OBJECT (jpeg2000parse,
          "No caps available: assuming grayscale with alpha");
    }

  }

  for (compno = 0; compno < numcomps; ++compno) {

    /* skip Ssiz (precision and signed/unsigned bit )  */
    if (!gst_byte_reader_skip (&reader, 1))
      goto beach;

    if (!gst_byte_reader_get_uint8 (&reader, dx + compno))
      goto beach;

    if (!gst_byte_reader_get_uint8 (&reader, dy + compno))
      goto beach;

    GST_DEBUG_OBJECT (jpeg2000parse,
        "Parsed sub-sampling %d,%d for component %d", dx[compno], dy[compno],
        compno);
  }

  /*** sanity check on sub-sampling *****/
  if (dx[0] != 1 || dy[0] != 1) {
    GST_WARNING_OBJECT (jpeg2000parse, "Sub-sampled luma channel");
  }
  if (dx[1] != dx[2] || dy[1] != dy[2]) {
    GST_WARNING_OBJECT (jpeg2000parse,
        "Chroma channel sub-sampling factors are not equal");
  }
  for (compno = 0; compno < numcomps; ++compno) {
    if (colorspace != GST_JPEG2000_COLORSPACE_NONE
        && (colorspace != GST_JPEG2000_COLORSPACE_YUV)
        && (dx[compno] > 1 || dy[compno] > 1)) {
      GST_WARNING_OBJECT (jpeg2000parse,
          "Sub-sampled RGB or monochrome color spaces");
    }
    if (sink_sampling != GST_JPEG2000_SAMPLING_NONE) {
      guint8 dx_caps, dy_caps;
      gst_jpeg2000_parse_get_subsampling (compno, sink_sampling, &dx_caps,
          &dy_caps);
      if (dx_caps != dx[compno] || dy_caps != dy[compno]) {
        GstJPEG2000Colorspace inferred_colorspace =
            GST_JPEG2000_COLORSPACE_NONE;
        GST_WARNING_OBJECT (jpeg2000parse,
            "Sink caps sub-sampling %d,%d for channel %d does not match stream sub-sampling %d,%d",
            dx_caps, dy_caps, compno, dx[compno], dy[compno]);
        /* try to guess correct color space */
        if (gst_jpeg2000_sampling_is_mono (sink_sampling))
          inferred_colorspace = GST_JPEG2000_COLORSPACE_GRAY;
        else if (gst_jpeg2000_sampling_is_rgb (sink_sampling))
          inferred_colorspace = GST_JPEG2000_COLORSPACE_RGB;
        else if (gst_jpeg2000_sampling_is_yuv (sink_sampling))
          inferred_colorspace = GST_JPEG2000_COLORSPACE_YUV;
        else if (colorspace)
          inferred_colorspace = colorspace;
        if (inferred_colorspace != GST_JPEG2000_COLORSPACE_NONE) {
          sink_sampling = GST_JPEG2000_SAMPLING_NONE;
          colorspace = inferred_colorspace;
          break;
        } else {
          /* unrecognized sink_sampling and no colorspace */
          GST_ERROR_OBJECT (jpeg2000parse,
              "Unrecognized sink sampling field and no sink colorspace field");
          ret = GST_FLOW_NOT_NEGOTIATED;
          goto beach;
        }
      }
    }
  }
  /*************************************/

  /* if colorspace is present, we can work out the parsed_sampling field */
  if (colorspace != GST_JPEG2000_COLORSPACE_NONE) {
    if (colorspace == GST_JPEG2000_COLORSPACE_YUV) {
      if (numcomps == 4) {
        guint i;
        parsed_sampling = GST_JPEG2000_SAMPLING_YBRA4444_EXT;
        for (i = 0; i < 4; ++i) {
          if (dx[i] > 1 || dy[i] > 1) {
            GST_WARNING_OBJECT (jpeg2000parse, "Sub-sampled YUVA images");
          }
        }
      } else if (numcomps == 3) {
        /* use sub-sampling from U chroma channel */
        if (dx[1] == 1 && dy[1] == 1) {
          parsed_sampling = GST_JPEG2000_SAMPLING_YBR444;
        } else if (dx[1] == 2 && dy[1] == 2) {
          parsed_sampling = GST_JPEG2000_SAMPLING_YBR420;
        } else if (dx[1] == 4 && dy[1] == 1) {
          parsed_sampling = GST_JPEG2000_SAMPLING_YBR411;
        } else if (dx[1] == 4 && dy[1] == 4) {
          parsed_sampling = GST_JPEG2000_SAMPLING_YBR410;
        } else if (dx[1] == 2 && dy[1] == 1) {
          parsed_sampling = GST_JPEG2000_SAMPLING_YBR422;
        } else {
          GST_WARNING_OBJECT (jpeg2000parse,
              "Unsupported sub-sampling factors %d,%d", dx[1], dy[1]);
          /* best effort */
          parsed_sampling = GST_JPEG2000_SAMPLING_YBR444;
        }
      }
    } else if (colorspace == GST_JPEG2000_COLORSPACE_GRAY) {
      parsed_sampling = GST_JPEG2000_SAMPLING_GRAYSCALE;
    } else {
      parsed_sampling =
          (numcomps ==
          4) ? GST_JPEG2000_SAMPLING_RGBA : GST_JPEG2000_SAMPLING_RGB;
    }
  } else {
    if (gst_jpeg2000_sampling_is_mono (sink_sampling)) {
      colorspace = GST_JPEG2000_COLORSPACE_GRAY;
    } else if (gst_jpeg2000_sampling_is_rgb (sink_sampling)) {
      colorspace = GST_JPEG2000_COLORSPACE_RGB;
    } else {
      /* best effort */
      colorspace = GST_JPEG2000_COLORSPACE_YUV;
    }
  }

  /* use caps height if in sub-frame mode, as encoded frame height will be
   * strictly less than full frame height */
  if (current_caps_struct &&
      gst_structure_has_name (current_caps_struct, "image/x-jpc-striped")) {
    gint h;

    if (!gst_structure_get_int (current_caps_struct, "num-stripes",
            &num_stripes) || num_stripes < 2) {
      GST_ELEMENT_ERROR (parse, STREAM, FORMAT, (NULL),
          ("Striped JPEG 2000 is missing the stripe count"));
      ret = GST_FLOW_ERROR;
      goto beach;
    }

    if (!gst_structure_get_int (current_caps_struct, "stripe-height",
            &stripe_height)) {
      stripe_height = height;
    } else if (stripe_height != height &&
        !GST_BUFFER_FLAG_IS_SET (frame->buffer, GST_BUFFER_FLAG_MARKER)) {
      GST_WARNING_OBJECT (parse,
          "Only the last stripe is expected to be different"
          " from the stripe height (%d != %u)", height, stripe_height);
    }

    gst_structure_get_int (current_caps_struct, "height", &h);
    height = h;
  }

  /* now we can set the source caps, if something has changed */
  source_sampling =
      sink_sampling !=
      GST_JPEG2000_SAMPLING_NONE ? sink_sampling : parsed_sampling;
  if (width != jpeg2000parse->width || height != jpeg2000parse->height
      || jpeg2000parse->sampling != source_sampling
      || jpeg2000parse->colorspace != colorspace) {
    gint fr_num = 0, fr_denom = 0;

    src_caps =
        gst_caps_new_simple (num_stripes > 1 ? "image/x-jpc-striped" :
        media_type_from_codec_format (jpeg2000parse->src_codec_format),
        "width", G_TYPE_INT, width,
        "height", G_TYPE_INT, height,
        "colorspace", G_TYPE_STRING,
        gst_jpeg2000_colorspace_to_string (colorspace), "sampling",
        G_TYPE_STRING, gst_jpeg2000_sampling_to_string (source_sampling),
        "profile", G_TYPE_INT, profile, "parsed", G_TYPE_BOOLEAN, TRUE, NULL);

    if (num_stripes > 1)
      gst_caps_set_simple (src_caps, "num-stripes", G_TYPE_INT, num_stripes,
          "stripe_height", G_TYPE_INT, stripe_height, NULL);

    if (gst_jpeg2000_parse_is_broadcast (capabilities)
        || gst_jpeg2000_parse_is_imf (capabilities)) {
      gst_caps_set_simple (src_caps, "main-level", G_TYPE_INT, main_level,
          NULL);
      if (gst_jpeg2000_parse_is_imf (capabilities)) {
        gst_caps_set_simple (src_caps, "sub-level", G_TYPE_INT, sub_level,
            NULL);
      }
    }

    if (current_caps_struct) {
      const gchar *caps_string;

      caps_string = gst_structure_get_string
          (current_caps_struct, "colorimetry");
      if (caps_string) {
        gst_caps_set_simple (src_caps, "colorimetry", G_TYPE_STRING,
            caps_string, NULL);
      }
      caps_string = gst_structure_get_string
          (current_caps_struct, "interlace-mode");
      if (caps_string) {
        gst_caps_set_simple (src_caps, "interlace-mode", G_TYPE_STRING,
            caps_string, NULL);
      }
      caps_string = gst_structure_get_string
          (current_caps_struct, "field-order");
      if (caps_string) {
        gst_caps_set_simple (src_caps, "field-order", G_TYPE_STRING,
            caps_string, NULL);
      }
      caps_string = gst_structure_get_string
          (current_caps_struct, "multiview-mode");
      if (caps_string) {
        gst_caps_set_simple (src_caps, "multiview-mode", G_TYPE_STRING,
            caps_string, NULL);
      }
      caps_string = gst_structure_get_string
          (current_caps_struct, "chroma-site");
      if (caps_string) {
        gst_caps_set_simple (src_caps, "chroma-site", G_TYPE_STRING,
            caps_string, NULL);
      }
      if (gst_structure_get_fraction (current_caps_struct, "framerate", &fr_num,
              &fr_denom)) {
        gst_caps_set_simple (src_caps, "framerate", GST_TYPE_FRACTION, fr_num,
            fr_denom, NULL);
      } else {
        GST_WARNING_OBJECT (jpeg2000parse, "No framerate set");
      }
    }

    if (!gst_pad_set_caps (GST_BASE_PARSE_SRC_PAD (parse), src_caps)) {
      GST_ERROR_OBJECT (jpeg2000parse, "Unable to set source caps");
      ret = GST_FLOW_NOT_NEGOTIATED;
      gst_caps_unref (src_caps);
      goto beach;
    }
    gst_caps_unref (src_caps);
    jpeg2000parse->width = width;
    jpeg2000parse->height = height;
    jpeg2000parse->sampling = source_sampling;
    jpeg2000parse->colorspace = colorspace;
  }
  /*************************************************/

  /* look for EOC to mark frame end */
  /* look for EOC end of codestream marker  */
  eoc_offset = gst_byte_reader_masked_scan_uint32 (&reader, 0x0000ffff,
      0xFFD9, 0, gst_byte_reader_get_remaining (&reader));
  if (eoc_offset == -1)
    goto beach;

  /* add 4 for eoc marker and eoc marker size */
  eoc_frame_size = gst_byte_reader_get_pos (&reader) + eoc_offset + 4;
  GST_DEBUG_OBJECT (jpeg2000parse,
      "Found EOC at offset = %d, frame size = %d", eoc_offset, eoc_frame_size);

  /* bail out if not enough data for frame */
  if (gst_byte_reader_get_size (&reader) < eoc_frame_size)
    goto beach;

  if (jpeg2000parse->frame_size && jpeg2000parse->frame_size != eoc_frame_size) {
    GST_WARNING_OBJECT (jpeg2000parse,
        "Frame size %d from contiguous code size does not equal frame size %d signaled by eoc",
        jpeg2000parse->frame_size, eoc_frame_size);
  }
  jpeg2000parse->frame_size = eoc_frame_size;


  /* clean up and finish frame */
  if (current_caps)
    gst_caps_unref (current_caps);
  gst_buffer_unmap (frame->buffer, &map);
  ret = gst_base_parse_finish_frame (parse, frame, jpeg2000parse->frame_size);
  if (ret != GST_FLOW_OK)
    gst_jpeg2000_parse_reset (parse, TRUE);
  return ret;

beach:
  if (current_caps)
    gst_caps_unref (current_caps);
  gst_buffer_unmap (frame->buffer, &map);
  if (ret != GST_FLOW_OK)
    gst_jpeg2000_parse_reset (parse, TRUE);
  return ret;
}

static GstFlowReturn
gst_jpeg2000_parse_pre_push_frame (GstBaseParse * parse,
    GstBaseParseFrame * frame)
{
  gst_jpeg2000_parse_reset (parse, FALSE);
  return GST_FLOW_OK;

}
