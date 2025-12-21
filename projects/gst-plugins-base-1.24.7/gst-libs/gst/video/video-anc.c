/* GStreamer
 * Copyright (C) 2018 Edward Hervey <edward@centricular.com>
 * Copyright (C) 2018 Sebastian Dröge <sebastian@centricular.com>
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

#include <string.h>
#include <gst/base/gstbytereader.h>
#include "video-anc.h"

/**
 * SECTION:gstvideoanc
 * @title: GstVideo Ancillary
 * @short_description: Utilities for Ancillary data, VBI and Closed Caption
 * @private_symbols:
 * - GST_ANCILLARY_META_INFO
 * - GST_ANCILLARY_META_API_TYPE
 * - gst_ancillary_meta_get_info
 * - gst_ancillary_meta_api_get_type
 *
 * A collection of objects and methods to assist with handling Ancillary Data
 * present in Vertical Blanking Interval as well as Closed Caption.
 */

#ifndef GST_DISABLE_GST_DEBUG
#define GST_CAT_DEFAULT ensure_debug_category()
static GstDebugCategory *
ensure_debug_category (void)
{
  static gsize cat_gonce = 0;

  if (g_once_init_enter (&cat_gonce)) {
    gsize cat_done;

    cat_done = (gsize) _gst_debug_category_new ("video-anc", 0,
        "Ancillary data, VBI and CC utilities");

    g_once_init_leave (&cat_gonce, cat_done);
  }

  return (GstDebugCategory *) cat_gonce;
}
#else
#define ensure_debug_category() /* NOOP */
#endif /* GST_DISABLE_GST_DEBUG */

struct _GstVideoVBIParser
{
  GstVideoInfo info;            /* format of the lines provided */
  guint8 *work_data;            /* Converted line in planar 16bit format */
  guint32 work_data_size;       /* Size in bytes of work_data */
  guint offset;                 /* Current offset (in bytes) in work_data */
  gboolean bit16;               /* Data is stored as 16bit if TRUE. Else 8bit(without parity) */
};

G_DEFINE_BOXED_TYPE (GstVideoVBIParser, gst_video_vbi_parser,
    (GBoxedCopyFunc) gst_video_vbi_parser_copy,
    (GBoxedFreeFunc) gst_video_vbi_parser_free);

GstVideoVBIParser *
gst_video_vbi_parser_copy (const GstVideoVBIParser * parser)
{
  GstVideoVBIParser *res;

  g_return_val_if_fail (parser != NULL, NULL);

  res = gst_video_vbi_parser_new (GST_VIDEO_INFO_FORMAT (&parser->info),
      parser->info.width);
  if (res) {
    memcpy (res->work_data, parser->work_data, parser->work_data_size);
  }
  return res;
}

/* See SMPTE S291 */
static GstVideoVBIParserResult
get_ancillary_16 (GstVideoVBIParser * parser, GstVideoAncillary * anc)
{
  gboolean found = FALSE;
  const guint16 *data = (const guint16 *) parser->work_data;

  g_return_val_if_fail (parser != NULL, GST_VIDEO_VBI_PARSER_RESULT_ERROR);
  g_return_val_if_fail (anc != NULL, GST_VIDEO_VBI_PARSER_RESULT_ERROR);

  /* 3 words are needed at least to detect what kind of packet we look at
   *
   * - ADF (SMPTE S291 3.2.1) in case of component ancillary format:
   *       0x000 0x3ff 0x3ff (followed by DID, SDID)
   * - ADF (SMPTE S291 3.2.2) in case of composite ancillary format:
   *       0x3fc DID   SDID
   */
  while (parser->offset + 3 < parser->work_data_size) {
    guint8 DID, SDID, DC;
    guint i = 0, j;
    guint checksum = 0;
    gboolean composite;

    /* Look for ADF */
    if (data[parser->offset] == 0x3fc) {
      /* composite */
      i += 1;
      composite = TRUE;
    } else if (data[parser->offset] == 0x000 &&
        data[parser->offset + 1] == 0x3ff &&
        data[parser->offset + 2] == 0x3ff) {
      /* component */
      i += 3;
      composite = FALSE;
    } else {
      parser->offset += 1;
      continue;
    }

    /* TODO: Might want to check parity bits here but the checksum in
     * the end should really be enough */

    /* 4 words: DID, SDID, DC, [DATA], checksum */
    if (parser->offset + i + 4 >= parser->work_data_size)
      goto not_enough_data;

    /* We have a valid ADF */
    DID = data[parser->offset + i] & 0xff;
    SDID = data[parser->offset + i + 1] & 0xff;
    DC = data[parser->offset + i + 2] & 0xff;
    i += 3;

    /* Check if we have enough room to get the User Data and checksum */
    if (parser->offset + i + DC + 1 >= parser->work_data_size)
      goto not_enough_data;

    /* We found a valid ANC \o/ */
    anc->DID = DID;
    anc->SDID_block_number = SDID;
    anc->data_count = DC;
    memset (anc->data, 0, 256);

    /* FIXME: We assume here the same data format for the user data as for the
     * DID/SDID: 10 bits with parity in the upper 2 bits. In theory some
     * standards could define this differently and even have full 10 bits of
     * user data but there does not seem to be a single such standard after
     * all these years.
     */

    /* i is at the beginning of the user data now */
    for (j = 0; j < anc->data_count; j++)
      anc->data[j] = data[parser->offset + i + j] & 0xff;
    i += DC;

    /* Checksum calculation SMPTE S291 3.2.1 */
    for (j = (composite ? 1 : 3); j < i; j++)
      checksum += data[parser->offset + j] & 0x1ff;
    checksum &= 0x1ff;
    checksum |= (!(checksum >> 8)) << 9;

    if (checksum != (data[parser->offset + i] & 0x3ff)) {
      GST_WARNING ("ADF checksum mismatch: expected 0x%03x, got 0x%03x",
          checksum, (data[parser->offset + i] & 0x3ff));
      parser->offset += 1;
      continue;
    }

    i += 1;

    found = TRUE;
    parser->offset += i;
    break;
  }

  if (found)
    return GST_VIDEO_VBI_PARSER_RESULT_OK;

  return GST_VIDEO_VBI_PARSER_RESULT_DONE;

  /* ERRORS */
not_enough_data:
  {
    GST_WARNING ("ANC requires more User Data than available line size");
    /* Avoid further calls to go in the same error */
    parser->offset = parser->work_data_size;
    return GST_VIDEO_VBI_PARSER_RESULT_ERROR;
  }
}

/* See SMPTE S291 */
static GstVideoVBIParserResult
get_ancillary_8 (GstVideoVBIParser * parser, GstVideoAncillary * anc)
{
  gboolean found = FALSE;
  const guint8 *data = parser->work_data;

  g_return_val_if_fail (parser != NULL, GST_VIDEO_VBI_PARSER_RESULT_ERROR);
  g_return_val_if_fail (anc != NULL, GST_VIDEO_VBI_PARSER_RESULT_ERROR);

  /* 3 words are needed at least to detect what kind of packet we look at
   *
   * - ADF (SMPTE S291 3.2.1) in case of component ancillary format:
   *       0x000 0x3ff 0x3ff (followed by DID, SDID)
   * - ADF (SMPTE S291 3.2.2) in case of composite ancillary format:
   *       0x3fc DID   SDID
   */
  while (parser->offset + 3 < parser->work_data_size) {
    guint8 DID, SDID, DC;
    guint i = 0, j;
    gboolean composite;
    guint checksum = 0;

    /* Look for ADF */
    if (data[parser->offset] == 0xfc) {
      /* composite */
      composite = TRUE;
      i += 1;
    } else if (data[parser->offset] == 0x00 &&
        data[parser->offset + 1] == 0xff && data[parser->offset + 2] == 0xff) {
      /* component */
      composite = FALSE;
      i += 3;
    } else {
      parser->offset += 1;
      continue;
    }

    /* 4 words: DID, SDID, DC, [DATA], checksum */
    if (parser->offset + i + 4 >= parser->work_data_size)
      goto not_enough_data;

    /* We have a valid ADF */
    DID = data[parser->offset + i];
    SDID = data[parser->offset + i + 1];
    DC = data[parser->offset + i + 2];
    i += 3;

    /* Check if we have enough room to get the User Data and checksum */
    if (parser->offset + i + DC + 1 >= parser->work_data_size)
      goto not_enough_data;

    /* We found a valid ANC \o/ */
    anc->DID = DID;
    anc->SDID_block_number = SDID;
    anc->data_count = DC;
    memset (anc->data, 0, 256);

    /* i is at the beginning of the user data now */
    for (j = 0; j < anc->data_count; j++)
      anc->data[j] = data[parser->offset + i + j] & 0xff;
    i += DC;

    /* Checksum calculation SMPTE S291 3.2.1 */
    for (j = (composite ? 1 : 3); j < i; j++)
      checksum += data[parser->offset + j];
    checksum &= 0xff;

    if (checksum != data[parser->offset + i]) {
      GST_WARNING ("ADF checksum mismatch: expected 0x%02x, got 0x%02x",
          checksum, data[parser->offset + i]);
      parser->offset += 1;
      continue;
    }

    i += 1;

    found = TRUE;
    parser->offset += i;
    break;
  }

  if (found)
    return GST_VIDEO_VBI_PARSER_RESULT_OK;

  return GST_VIDEO_VBI_PARSER_RESULT_DONE;

  /* ERRORS */
not_enough_data:
  {
    GST_WARNING ("ANC requires more User Data than available line size");
    /* Avoid further calls to go in the same error */
    parser->offset = parser->work_data_size;
    return GST_VIDEO_VBI_PARSER_RESULT_ERROR;
  }
}

/**
 * gst_video_vbi_parser_get_ancillary:
 * @parser: a #GstVideoVBIParser
 * @anc: (out caller-allocates): a #GstVideoAncillary to start the eventual ancillary data
 *
 * Parse the line provided previously by gst_video_vbi_parser_add_line().
 *
 * Since: 1.16
 *
 * Returns: %GST_VIDEO_VBI_PARSER_RESULT_OK if ancillary data was found and
 * @anc was filled. %GST_VIDEO_VBI_PARSER_RESULT_DONE if there wasn't any
 * data.
 */

GstVideoVBIParserResult
gst_video_vbi_parser_get_ancillary (GstVideoVBIParser * parser,
    GstVideoAncillary * anc)
{
  g_return_val_if_fail (parser != NULL, GST_VIDEO_VBI_PARSER_RESULT_ERROR);
  g_return_val_if_fail (anc != NULL, GST_VIDEO_VBI_PARSER_RESULT_ERROR);

  if (parser->bit16)
    return get_ancillary_16 (parser, anc);
  return get_ancillary_8 (parser, anc);
}

/**
 * gst_video_vbi_parser_new:
 * @format: a #GstVideoFormat
 * @pixel_width: The width in pixel to use
 *
 * Create a new #GstVideoVBIParser for the specified @format and @pixel_width.
 *
 * Since: 1.16
 *
 * Returns: (nullable): The new #GstVideoVBIParser or %NULL if the @format and/or @pixel_width
 * is not supported.
 */
GstVideoVBIParser *
gst_video_vbi_parser_new (GstVideoFormat format, guint32 pixel_width)
{
  GstVideoVBIParser *parser;

  g_return_val_if_fail (pixel_width > 0, NULL);

  switch (format) {
    case GST_VIDEO_FORMAT_v210:
      parser = g_new0 (GstVideoVBIParser, 1);
      parser->bit16 = TRUE;
      break;
    case GST_VIDEO_FORMAT_UYVY:
      parser = g_new0 (GstVideoVBIParser, 1);
      parser->bit16 = FALSE;
      break;
    default:
      GST_WARNING ("Format not supported by GstVideoVBIParser");
      return NULL;
  }

  gst_video_info_init (&parser->info);
  if (!gst_video_info_set_format (&parser->info, format, pixel_width, 1)) {
    GST_ERROR ("Could not create GstVideoInfo");
    g_free (parser);
    return NULL;
  }

  /* Allocate the workspace which is going to be 2 * pixel_width big
   *  2 : number of pixels per "component" (we only deal with 4:2:2)
   * We use 1 or 2 bytes per pixel depending on whether we are internally
   * working in 8 or 16bit */
  parser->work_data_size = 2 * pixel_width;
  if (parser->bit16)
    parser->work_data = g_malloc0 (parser->work_data_size * 2);
  else
    parser->work_data = g_malloc0 (parser->work_data_size);
  parser->offset = 0;

  return parser;
}

/**
 * gst_video_vbi_parser_free:
 * @parser: a #GstVideoVBIParser
 *
 * Frees the @parser.
 *
 * Since: 1.16
 */
void
gst_video_vbi_parser_free (GstVideoVBIParser * parser)
{
  g_return_if_fail (parser != NULL);

  g_free (parser->work_data);
  g_free (parser);
}

static void
convert_line_from_uyvy (GstVideoVBIParser * parser, const guint8 * data)
{
  guint i;
  guint8 *y = parser->work_data;

  /* Data is stored differently in SD, making no distinction between Y and UV */
  if (parser->info.width < 1280) {
    for (i = 0; i < parser->info.width - 3; i += 4) {
      *y++ = data[(i / 4) * 4 + 0];
      *y++ = data[(i / 4) * 4 + 1];
      *y++ = data[(i / 4) * 4 + 2];
      *y++ = data[(i / 4) * 4 + 3];
    }
  } else {
    guint8 *uv = y + parser->info.width;

    for (i = 0; i < parser->info.width - 3; i += 4) {
      *uv++ = data[(i / 4) * 4 + 0];
      *y++ = data[(i / 4) * 4 + 1];
      *uv++ = data[(i / 4) * 4 + 2];
      *y++ = data[(i / 4) * 4 + 3];
    }
  }
  GST_MEMDUMP ("Converted line", parser->work_data, 128);
}

static void
gst_info_dump_mem16_line (gchar * linebuf, gsize linebuf_size,
    const guint16 * mem, gsize mem_offset, gsize mem_size)
{
  gchar hexstr[50], digitstr[6];

  if (mem_size > 8)
    mem_size = 8;

  hexstr[0] = '\0';

  if (mem != NULL) {
    guint i = 0;

    mem += mem_offset;
    while (i < mem_size) {
      g_snprintf (digitstr, sizeof (digitstr), "%04x ", mem[i]);
      g_strlcat (hexstr, digitstr, sizeof (hexstr));
      ++i;
    }
  }

  g_snprintf (linebuf, linebuf_size, "%08x: %-48.48s",
      (guint) mem_offset, hexstr);
}

static void
convert_line_from_v210 (GstVideoVBIParser * parser, const guint8 * data)
{
  guint i;
  guint16 *y = (guint16 *) parser->work_data;
  guint32 a, b, c, d;

  /* Data is stored differently in SD, making no distinction between Y and UV */
  if (parser->info.width < 1280) {
    /* Convert the line */
    for (i = 0; i < parser->info.width - 5; i += 6) {
      a = GST_READ_UINT32_LE (data + (i / 6) * 16 + 0);
      b = GST_READ_UINT32_LE (data + (i / 6) * 16 + 4);
      c = GST_READ_UINT32_LE (data + (i / 6) * 16 + 8);
      d = GST_READ_UINT32_LE (data + (i / 6) * 16 + 12);

      *y++ = (a >> 0) & 0x3ff;
      *y++ = (a >> 10) & 0x3ff;
      *y++ = (a >> 20) & 0x3ff;
      *y++ = (b >> 0) & 0x3ff;

      *y++ = (b >> 10) & 0x3ff;
      *y++ = (b >> 20) & 0x3ff;
      *y++ = (c >> 0) & 0x3ff;
      *y++ = (c >> 10) & 0x3ff;

      *y++ = (c >> 20) & 0x3ff;
      *y++ = (d >> 0) & 0x3ff;
      *y++ = (d >> 10) & 0x3ff;
      *y++ = (d >> 20) & 0x3ff;
    }
  } else {
    guint16 *uv = y + parser->info.width;

    /* Convert the line */
    for (i = 0; i < parser->info.width - 5; i += 6) {
      a = GST_READ_UINT32_LE (data + (i / 6) * 16 + 0);
      b = GST_READ_UINT32_LE (data + (i / 6) * 16 + 4);
      c = GST_READ_UINT32_LE (data + (i / 6) * 16 + 8);
      d = GST_READ_UINT32_LE (data + (i / 6) * 16 + 12);

      *uv++ = (a >> 0) & 0x3ff;
      *y++ = (a >> 10) & 0x3ff;
      *uv++ = (a >> 20) & 0x3ff;
      *y++ = (b >> 0) & 0x3ff;

      *uv++ = (b >> 10) & 0x3ff;
      *y++ = (b >> 20) & 0x3ff;
      *uv++ = (c >> 0) & 0x3ff;
      *y++ = (c >> 10) & 0x3ff;

      *uv++ = (c >> 20) & 0x3ff;
      *y++ = (d >> 0) & 0x3ff;
      *uv++ = (d >> 10) & 0x3ff;
      *y++ = (d >> 20) & 0x3ff;
    }
  }

  if (0) {
    guint off = 0;
    gsize length = parser->info.width * 2;

    GST_TRACE ("--------"
        "-------------------------------------------------------------------");

    while (off < length) {
      gchar buf[128];

      /* gst_info_dump_mem_line will process 16 bytes (8 16bit chunks) at most */
      gst_info_dump_mem16_line (buf, sizeof (buf),
          (guint16 *) parser->work_data, off, length - off);
      GST_TRACE ("%s", buf);
      off += 8;
    }
    GST_TRACE ("--------"
        "-------------------------------------------------------------------");
  }
}

/**
 * gst_video_vbi_parser_add_line:
 * @parser: a #GstVideoVBIParser
 * @data: (array) (transfer none): The line of data to parse
 *
 * Provide a new line of data to the @parser. Call gst_video_vbi_parser_get_ancillary()
 * to get the Ancillary data that might be present on that line.
 *
 * Since: 1.16
 */
void
gst_video_vbi_parser_add_line (GstVideoVBIParser * parser, const guint8 * data)
{
  g_return_if_fail (parser != NULL);
  g_return_if_fail (data != NULL);

  /* Reset offset */
  parser->offset = 0;

  switch (GST_VIDEO_INFO_FORMAT (&parser->info)) {
    case GST_VIDEO_FORMAT_v210:
      convert_line_from_v210 (parser, data);
      break;
    case GST_VIDEO_FORMAT_UYVY:
      convert_line_from_uyvy (parser, data);
      break;
    default:
      GST_ERROR ("UNSUPPORTED FORMAT !");
      g_assert_not_reached ();
      break;
  }
}

struct _GstVideoVBIEncoder
{
  GstVideoInfo info;            /* format of the lines provided */
  guint8 *work_data;            /* Converted line in planar 16bit format */
  guint32 work_data_size;       /* Size in bytes of work_data */
  guint offset;                 /* Current offset (in bytes) in work_data */
  gboolean bit16;               /* Data is stored as 16bit if TRUE. Else 8bit(without parity) */
};

G_DEFINE_BOXED_TYPE (GstVideoVBIEncoder, gst_video_vbi_encoder,
    (GBoxedCopyFunc) gst_video_vbi_encoder_copy,
    (GBoxedFreeFunc) gst_video_vbi_encoder_free);

GstVideoVBIEncoder *
gst_video_vbi_encoder_copy (const GstVideoVBIEncoder * encoder)
{
  GstVideoVBIEncoder *res;

  g_return_val_if_fail (encoder != NULL, NULL);

  res = gst_video_vbi_encoder_new (GST_VIDEO_INFO_FORMAT (&encoder->info),
      encoder->info.width);
  if (res) {
    memcpy (res->work_data, encoder->work_data, encoder->work_data_size);
  }
  return res;
}

/**
 * gst_video_vbi_encoder_free:
 * @encoder: a #GstVideoVBIEncoder
 *
 * Frees the @encoder.
 *
 * Since: 1.16
 */
void
gst_video_vbi_encoder_free (GstVideoVBIEncoder * encoder)
{
  g_return_if_fail (encoder != NULL);

  g_free (encoder->work_data);
  g_free (encoder);
}

/**
 * gst_video_vbi_encoder_new:
 * @format: a #GstVideoFormat
 * @pixel_width: The width in pixel to use
 *
 * Create a new #GstVideoVBIEncoder for the specified @format and @pixel_width.
 *
 * Since: 1.16
 *
 * Returns: (nullable): The new #GstVideoVBIEncoder or %NULL if the @format and/or @pixel_width
 * is not supported.
 */
GstVideoVBIEncoder *
gst_video_vbi_encoder_new (GstVideoFormat format, guint32 pixel_width)
{
  GstVideoVBIEncoder *encoder;

  g_return_val_if_fail (pixel_width > 0, NULL);

  switch (format) {
    case GST_VIDEO_FORMAT_v210:
      encoder = g_new0 (GstVideoVBIEncoder, 1);
      encoder->bit16 = TRUE;
      break;
    case GST_VIDEO_FORMAT_UYVY:
      encoder = g_new0 (GstVideoVBIEncoder, 1);
      encoder->bit16 = FALSE;
      break;
    default:
      GST_WARNING ("Format not supported by GstVideoVBIEncoder");
      return NULL;
  }

  gst_video_info_init (&encoder->info);
  if (!gst_video_info_set_format (&encoder->info, format, pixel_width, 1)) {
    GST_ERROR ("Could not create GstVideoInfo");
    g_free (encoder);
    return NULL;
  }

  /* Allocate the workspace which is going to be 2 * pixel_width big
   *  2 : number of pixels per "component" (we only deal with 4:2:2)
   * We use 1 or 2 bytes per pixel depending on whether we are internally
   * working in 8 or 16bit */
  encoder->work_data_size = 2 * pixel_width;
  if (encoder->bit16)
    encoder->work_data = g_malloc0 (encoder->work_data_size * 2);
  else
    encoder->work_data = g_malloc0 (encoder->work_data_size);
  encoder->offset = 0;

  return encoder;
}

#if G_GNUC_CHECK_VERSION(3,4)
static inline guint
parity (guint8 x)
{
  return __builtin_parity (x);
}
#else
static guint
parity (guint8 x)
{
  guint count = 0;

  while (x) {
    count += x & 1;
    x >>= 1;
  }

  return count & 1;
}
#endif

/* Odd/even parity in the upper two bits */
#define SET_WITH_PARITY(buf, val) G_STMT_START { \
  *(buf) = val; \
    if (parity (val)) \
      *(buf) |= 0x100; \
    else \
      *(buf) |= 0x200; \
} G_STMT_END;

/**
 * gst_video_vbi_encoder_add_ancillary:
 * @encoder: a #GstVideoVBIEncoder
 * @composite: %TRUE if composite ADF should be created, component otherwise
 * @DID: The Data Identifier
 * @SDID_block_number: The Secondary Data Identifier (if type 2) or the Data
 *                     Block Number (if type 1)
 * @data_count: The amount of data (in bytes) in @data (max 255 bytes)
 * @data: (array length=data_count): The user data content of the Ancillary packet.
 *    Does not contain the ADF, DID, SDID nor CS.
 *
 * Stores Video Ancillary data, according to SMPTE-291M specification.
 *
 * Note that the contents of the data are always read as 8bit data (i.e. do not contain
 * the parity check bits).
 *
 * Since: 1.16
 *
 * Returns: %TRUE if enough space was left in the current line, %FALSE
 *          otherwise.
 */
gboolean
gst_video_vbi_encoder_add_ancillary (GstVideoVBIEncoder * encoder,
    gboolean composite, guint8 DID, guint8 SDID_block_number,
    const guint8 * data, guint data_count)
{
  g_return_val_if_fail (encoder != NULL, FALSE);
  g_return_val_if_fail (data != NULL, FALSE);
  g_return_val_if_fail (data_count < 256, FALSE);

  /* Doesn't fit into this line anymore */
  if (encoder->offset + data_count + (composite ? 5 : 7) >
      encoder->work_data_size)
    return FALSE;

  if (encoder->bit16) {
    guint16 *work_data = ((guint16 *) encoder->work_data) + encoder->offset;
    guint i = 0, j;
    guint checksum = 0;

    /* Write ADF */
    if (composite) {
      work_data[i] = 0x3fc;
      i += 1;
    } else {
      work_data[i] = 0x000;
      work_data[i + 1] = 0x3ff;
      work_data[i + 2] = 0x3ff;
      i += 3;
    }

    SET_WITH_PARITY (&work_data[i], DID);
    SET_WITH_PARITY (&work_data[i + 1], SDID_block_number);
    SET_WITH_PARITY (&work_data[i + 2], data_count);
    i += 3;

    for (j = 0; j < data_count; j++)
      SET_WITH_PARITY (&work_data[i + j], data[j]);
    i += data_count;

    for (j = (composite ? 1 : 3); j < i; j++)
      checksum += work_data[j];
    checksum &= 0x1ff;
    checksum |= (!(checksum >> 8)) << 9;

    work_data[i] = checksum;
    i += 1;

    encoder->offset += i;
  } else {
    guint8 *work_data = ((guint8 *) encoder->work_data) + encoder->offset;
    guint i = 0, j;
    guint checksum = 0;

    /* Write ADF */
    if (composite) {
      work_data[i] = 0xfc;
      i += 1;
    } else {
      work_data[i] = 0x00;
      work_data[i + 1] = 0xff;
      work_data[i + 2] = 0xff;
      i += 3;
    }

    work_data[i] = DID;
    work_data[i + 1] = SDID_block_number;
    work_data[i + 2] = data_count;
    i += 3;

    for (j = 0; j < data_count; j++)
      work_data[i + j] = data[j];
    i += data_count;

    for (j = (composite ? 1 : 3); j < i; j++)
      checksum += work_data[j];
    checksum &= 0xff;

    work_data[i] = checksum;
    i += 1;

    encoder->offset += i;
  }

  return TRUE;
}

static void
convert_line_to_v210 (GstVideoVBIEncoder * encoder, guint8 * data)
{
  guint i;
  const guint16 *y = (const guint16 *) encoder->work_data;
  guint32 a, b, c, d;

  /* Data is stored differently in SD, making no distinction between Y and UV */
  if (encoder->info.width < 1280) {
    /* Convert the line */
    for (i = 0; i < encoder->info.width - 5; i += 6) {
      a = ((y[0] & 0x3ff) << 0)
          | ((y[1] & 0x3ff) << 10)
          | ((y[2] & 0x3ff) << 20);
      y += 3;

      b = ((y[0] & 0x3ff) << 0)
          | ((y[1] & 0x3ff) << 10)
          | ((y[2] & 0x3ff) << 20);
      y += 3;

      c = ((y[0] & 0x3ff) << 0)
          | ((y[1] & 0x3ff) << 10)
          | ((y[2] & 0x3ff) << 20);
      y += 3;

      d = ((y[0] & 0x3ff) << 0)
          | ((y[1] & 0x3ff) << 10)
          | ((y[2] & 0x3ff) << 20);
      y += 3;

      GST_WRITE_UINT32_LE (data + (i / 6) * 16 + 0, a);
      GST_WRITE_UINT32_LE (data + (i / 6) * 16 + 4, b);
      GST_WRITE_UINT32_LE (data + (i / 6) * 16 + 8, c);
      GST_WRITE_UINT32_LE (data + (i / 6) * 16 + 12, d);
    }
  } else {
    const guint16 *uv = y + encoder->info.width;

    /* Convert the line */
    for (i = 0; i < encoder->info.width - 5; i += 6) {
      a = ((uv[0] & 0x3ff) << 0)
          | ((y[0] & 0x3ff) << 10)
          | ((uv[1] & 0x3ff) << 20);
      uv += 2;
      y++;

      b = ((y[0] & 0x3ff) << 0)
          | ((uv[0] & 0x3ff) << 10)
          | ((y[1] & 0x3ff) << 20);
      y += 2;
      uv++;

      c = ((uv[0] & 0x3ff) << 0)
          | ((y[0] & 0x3ff) << 10)
          | ((uv[1] & 0x3ff) << 20);
      uv += 2;
      y++;

      d = ((y[0] & 0x3ff) << 0)
          | ((uv[0] & 0x3ff) << 10)
          | ((y[1] & 0x3ff) << 20);
      y += 2;
      uv++;

      GST_WRITE_UINT32_LE (data + (i / 6) * 16 + 0, a);
      GST_WRITE_UINT32_LE (data + (i / 6) * 16 + 4, b);
      GST_WRITE_UINT32_LE (data + (i / 6) * 16 + 8, c);
      GST_WRITE_UINT32_LE (data + (i / 6) * 16 + 12, d);
    }
  }
}

static void
convert_line_to_uyvy (GstVideoVBIEncoder * encoder, guint8 * data)
{
  guint i;
  const guint8 *y = encoder->work_data;

  /* Data is stored differently in SD, making no distinction between Y and UV */
  if (encoder->info.width < 1280) {
    for (i = 0; i < encoder->info.width - 3; i += 4) {
      data[(i / 4) * 4 + 0] = *y++;
      data[(i / 4) * 4 + 1] = *y++;
      data[(i / 4) * 4 + 2] = *y++;
      data[(i / 4) * 4 + 3] = *y++;
    }
  } else {
    const guint8 *uv = y + encoder->info.width;

    for (i = 0; i < encoder->info.width - 3; i += 4) {
      data[(i / 4) * 4 + 0] = *uv++;
      data[(i / 4) * 4 + 1] = *y++;
      data[(i / 4) * 4 + 2] = *uv++;
      data[(i / 4) * 4 + 3] = *y++;
    }
  }
}

void
gst_video_vbi_encoder_write_line (GstVideoVBIEncoder * encoder, guint8 * data)
{
  g_return_if_fail (encoder != NULL);
  g_return_if_fail (data != NULL);

  /* nothing to write? just exit early */
  if (!encoder->offset)
    return;

  switch (GST_VIDEO_INFO_FORMAT (&encoder->info)) {
    case GST_VIDEO_FORMAT_v210:
      convert_line_to_v210 (encoder, data);
      break;
    case GST_VIDEO_FORMAT_UYVY:
      convert_line_to_uyvy (encoder, data);
      break;
    default:
      GST_ERROR ("UNSUPPORTED FORMAT !");
      g_assert_not_reached ();
      break;
  }

  encoder->offset = 0;
  memset (encoder->work_data, 0,
      encoder->work_data_size * (encoder->bit16 ? 2 : 1));
}

/* Closed Caption Meta implementation *******************************************/

GType
gst_video_caption_meta_api_get_type (void)
{
  static GType type = 0;

  if (g_once_init_enter (&type)) {
    static const gchar *tags[] = { NULL };
    GType _type = gst_meta_api_type_register ("GstVideoCaptionMetaAPI", tags);
    GST_INFO ("registering");
    g_once_init_leave (&type, _type);
  }
  return type;
}


static gboolean
gst_video_caption_meta_transform (GstBuffer * dest, GstMeta * meta,
    GstBuffer * buffer, GQuark type, gpointer data)
{
  GstVideoCaptionMeta *dmeta, *smeta;

  /* We always copy over the caption meta */
  smeta = (GstVideoCaptionMeta *) meta;

  GST_DEBUG ("copy caption metadata");
  dmeta =
      gst_buffer_add_video_caption_meta (dest, smeta->caption_type,
      smeta->data, smeta->size);
  if (!dmeta)
    return FALSE;

  return TRUE;
}

static gboolean
gst_video_caption_meta_init (GstMeta * meta, gpointer params,
    GstBuffer * buffer)
{
  GstVideoCaptionMeta *emeta = (GstVideoCaptionMeta *) meta;

  emeta->caption_type = GST_VIDEO_CAPTION_TYPE_UNKNOWN;
  emeta->data = NULL;
  emeta->size = 0;

  return TRUE;
}

static void
gst_video_caption_meta_free (GstMeta * meta, GstBuffer * buffer)
{
  GstVideoCaptionMeta *emeta = (GstVideoCaptionMeta *) meta;

  g_free (emeta->data);
}

const GstMetaInfo *
gst_video_caption_meta_get_info (void)
{
  static const GstMetaInfo *meta_info = NULL;

  if (g_once_init_enter ((GstMetaInfo **) & meta_info)) {
    const GstMetaInfo *mi = gst_meta_register (GST_VIDEO_CAPTION_META_API_TYPE,
        "GstVideoCaptionMeta",
        sizeof (GstVideoCaptionMeta),
        gst_video_caption_meta_init,
        gst_video_caption_meta_free,
        gst_video_caption_meta_transform);
    g_once_init_leave ((GstMetaInfo **) & meta_info, (GstMetaInfo *) mi);
  }
  return meta_info;
}

/**
 * gst_buffer_add_video_caption_meta:
 * @buffer: a #GstBuffer
 * @caption_type: The type of Closed Caption to add
 * @data: (array length=size) (transfer none): The Closed Caption data
 * @size: The size of @data in bytes
 *
 * Attaches #GstVideoCaptionMeta metadata to @buffer with the given
 * parameters.
 *
 * Returns: (transfer none): the #GstVideoCaptionMeta on @buffer.
 *
 * Since: 1.16
 */
GstVideoCaptionMeta *
gst_buffer_add_video_caption_meta (GstBuffer * buffer,
    GstVideoCaptionType caption_type, const guint8 * data, gsize size)
{
  GstVideoCaptionMeta *meta;

  g_return_val_if_fail (GST_IS_BUFFER (buffer), NULL);
  g_return_val_if_fail (data != NULL, NULL);
  g_return_val_if_fail (size > 0, NULL);

  switch (caption_type) {
    case GST_VIDEO_CAPTION_TYPE_CEA608_RAW:
    case GST_VIDEO_CAPTION_TYPE_CEA608_S334_1A:
    case GST_VIDEO_CAPTION_TYPE_CEA708_RAW:
    case GST_VIDEO_CAPTION_TYPE_CEA708_CDP:
      break;
    default:
      GST_ERROR ("Unknown caption type !");
      return NULL;
  }
  /* FIXME : Add checks for content ? */

  meta = (GstVideoCaptionMeta *) gst_buffer_add_meta (buffer,
      GST_VIDEO_CAPTION_META_INFO, NULL);
  g_return_val_if_fail (meta != NULL, NULL);

  meta->caption_type = caption_type;
  meta->data = g_memdup2 (data, size);
  meta->size = size;

  return meta;
}

/**
 * gst_video_caption_type_from_caps:
 * @caps: Fixed #GstCaps to parse
 *
 * Parses fixed Closed Caption #GstCaps and returns the corresponding caption
 * type, or %GST_VIDEO_CAPTION_TYPE_UNKNOWN.
 *
 * Returns: #GstVideoCaptionType.
 *
 * Since: 1.16
 */
GstVideoCaptionType
gst_video_caption_type_from_caps (const GstCaps * caps)
{
  const GstStructure *s;
  const gchar *format;

  g_return_val_if_fail (gst_caps_is_fixed (caps),
      GST_VIDEO_CAPTION_TYPE_UNKNOWN);

  s = gst_caps_get_structure (caps, 0);
  g_return_val_if_fail (s != NULL, GST_VIDEO_CAPTION_TYPE_UNKNOWN);

  format = gst_structure_get_string (s, "format");
  if (gst_structure_has_name (s, "closedcaption/x-cea-608")) {
    if (g_strcmp0 (format, "raw") == 0) {
      return GST_VIDEO_CAPTION_TYPE_CEA608_RAW;
    } else if (g_strcmp0 (format, "s334-1a") == 0) {
      return GST_VIDEO_CAPTION_TYPE_CEA608_S334_1A;
    }
  } else if (gst_structure_has_name (s, "closedcaption/x-cea-708")) {
    if (g_strcmp0 (format, "cc_data") == 0) {
      return GST_VIDEO_CAPTION_TYPE_CEA708_RAW;
    } else if (g_strcmp0 (format, "cdp") == 0) {
      return GST_VIDEO_CAPTION_TYPE_CEA708_CDP;
    }
  }
  return GST_VIDEO_CAPTION_TYPE_UNKNOWN;
}

/**
 * gst_video_caption_type_to_caps:
 * @type: #GstVideoCaptionType
 *
 * Creates new caps corresponding to @type.
 *
 * Returns: (transfer full): new #GstCaps
 *
 * Since: 1.16
 */
GstCaps *
gst_video_caption_type_to_caps (GstVideoCaptionType type)
{
  GstCaps *caption_caps;

  g_return_val_if_fail (type != GST_VIDEO_CAPTION_TYPE_UNKNOWN, NULL);

  switch (type) {
    case GST_VIDEO_CAPTION_TYPE_CEA608_RAW:
      caption_caps = gst_caps_new_simple ("closedcaption/x-cea-608",
          "format", G_TYPE_STRING, "raw", NULL);
      break;
    case GST_VIDEO_CAPTION_TYPE_CEA608_S334_1A:
      caption_caps = gst_caps_new_simple ("closedcaption/x-cea-608",
          "format", G_TYPE_STRING, "s334-1a", NULL);
      break;
    case GST_VIDEO_CAPTION_TYPE_CEA708_RAW:
      caption_caps = gst_caps_new_simple ("closedcaption/x-cea-708",
          "format", G_TYPE_STRING, "cc_data", NULL);
      break;
    case GST_VIDEO_CAPTION_TYPE_CEA708_CDP:
      caption_caps = gst_caps_new_simple ("closedcaption/x-cea-708",
          "format", G_TYPE_STRING, "cdp", NULL);
      break;
    default:
      g_return_val_if_reached (NULL);
      break;
  }

  return caption_caps;
}

/* Ancillary Meta Implementation */

GType
gst_ancillary_meta_api_get_type (void)
{
  static GType type = 0;

  if (g_once_init_enter (&type)) {
    static const gchar *tags[] = { NULL };
    GType _type = gst_meta_api_type_register ("GstAncillaryMetaAPI", tags);
    GST_INFO ("registering");
    g_once_init_leave (&type, _type);
  }
  return type;
}

static gboolean
gst_ancillary_meta_init (GstMeta * meta, gpointer params, GstBuffer * buffer)
{
  GstAncillaryMeta *ameta = (GstAncillaryMeta *) meta;

  /* Set sensible default values */
  ameta->field = GST_ANCILLARY_META_FIELD_PROGRESSIVE;
  ameta->c_not_y_channel = 0;
  ameta->line = 0x7ff;
  ameta->offset = 0xfff;

  ameta->DID = ameta->SDID_block_number = ameta->data_count = 0;
  ameta->data = NULL;
  ameta->checksum = 0;

  return TRUE;
}

static void
gst_ancillary_meta_free (GstMeta * meta, GstBuffer * buffer)
{
  GstAncillaryMeta *ameta = (GstAncillaryMeta *) meta;

  g_free (ameta->data);
}

static gboolean
gst_ancillary_meta_transform (GstBuffer * dest, GstMeta * meta,
    GstBuffer * buffer, GQuark type, gpointer data)
{
  GstAncillaryMeta *dmeta, *smeta;

  /* We always copy over the ancillary meta */
  smeta = (GstAncillaryMeta *) meta;

  dmeta =
      (GstAncillaryMeta *) gst_buffer_add_meta (dest, GST_ANCILLARY_META_INFO,
      NULL);

  dmeta->field = smeta->field;
  dmeta->c_not_y_channel = smeta->c_not_y_channel;
  dmeta->line = smeta->line;
  dmeta->offset = smeta->offset;
  dmeta->DID = smeta->DID;
  dmeta->SDID_block_number = smeta->SDID_block_number;
  dmeta->data_count = smeta->data_count;
  dmeta->data = g_memdup2 (smeta->data, (smeta->data_count & 0xff) * 2);
  dmeta->checksum = smeta->checksum;

  return TRUE;
}

const GstMetaInfo *
gst_ancillary_meta_get_info (void)
{
  static const GstMetaInfo *meta_info = NULL;

  if (g_once_init_enter ((GstMetaInfo **) & meta_info)) {
    const GstMetaInfo *mi = gst_meta_register (GST_ANCILLARY_META_API_TYPE,
        "GstAncillaryMeta",
        sizeof (GstAncillaryMeta),
        gst_ancillary_meta_init,
        gst_ancillary_meta_free,
        gst_ancillary_meta_transform);
    g_once_init_leave ((GstMetaInfo **) & meta_info, (GstMetaInfo *) mi);
  }
  return meta_info;

}

/**
 * gst_buffer_add_ancillary_meta:
 * @buffer: A #GstBuffer
 *
 * Adds a new #GstAncillaryMeta to the @buffer. The caller is responsible for setting the appropriate
 * fields.
 *
 * Since: 1.24
 *
 * Returns: (transfer none): A new #GstAncillaryMeta, or %NULL if an error happened.
 */

GstAncillaryMeta *
gst_buffer_add_ancillary_meta (GstBuffer * buffer)
{
  GstAncillaryMeta *meta;

  meta =
      (GstAncillaryMeta *) gst_buffer_add_meta (buffer, GST_ANCILLARY_META_INFO,
      NULL);
  g_assert (meta != NULL);

  return meta;
}

/* Active Format Description (AFD) Meta implementation */

GType
gst_video_afd_meta_api_get_type (void)
{
  static GType type = 0;

  if (g_once_init_enter (&type)) {
    static const gchar *tags[] = {
      GST_META_TAG_VIDEO_SIZE_STR,
      GST_META_TAG_VIDEO_ORIENTATION_STR,
      GST_META_TAG_VIDEO_STR,
      NULL
    };
    GType _type = gst_meta_api_type_register ("GstVideoAFDMetaAPI", tags);
    g_once_init_leave (&type, _type);
  }
  return type;
}

static gboolean
gst_video_afd_meta_init (GstMeta * meta, gpointer params, GstBuffer * buffer)
{
  GstVideoAFDMeta *emeta = (GstVideoAFDMeta *) meta;

  emeta->field = 0;
  emeta->spec = GST_VIDEO_AFD_SPEC_ATSC_A53;
  emeta->afd = GST_VIDEO_AFD_UNAVAILABLE;

  return TRUE;
}

static gboolean
gst_video_afd_meta_transform (GstBuffer * dest, GstMeta * meta,
    GstBuffer * buffer, GQuark type, gpointer data)
{
  GstVideoAFDMeta *smeta = (GstVideoAFDMeta *) meta;

  if (GST_META_TRANSFORM_IS_COPY (type)) {
    GST_DEBUG ("copy AFD metadata");
    gst_buffer_add_video_afd_meta (dest, smeta->field, smeta->spec, smeta->afd);
    return TRUE;
  } else if (GST_VIDEO_META_TRANSFORM_IS_SCALE (type)) {
    GstVideoMetaTransform *trans = data;
    gdouble diff;
    gint ow, oh, nw, nh;
    gint opn, opd, npn, npd;

    ow = GST_VIDEO_INFO_WIDTH (trans->in_info);
    nw = GST_VIDEO_INFO_WIDTH (trans->out_info);
    oh = GST_VIDEO_INFO_HEIGHT (trans->in_info);
    nh = GST_VIDEO_INFO_HEIGHT (trans->out_info);
    opn = GST_VIDEO_INFO_PAR_N (trans->in_info);
    opd = GST_VIDEO_INFO_PAR_D (trans->in_info);
    npn = GST_VIDEO_INFO_PAR_N (trans->out_info);
    npd = GST_VIDEO_INFO_PAR_D (trans->out_info);

    /* if the aspect ratio stays the same we can copy the meta, otherwise
     * we can't know if the aspect ratio was changed or black borders were
     * introduced. Both would invalidate the AFD meta */

    diff =
        ABS (((gdouble) ow / (gdouble) oh) * ((gdouble) opn / (gdouble) opd) -
        ((gdouble) nw / (gdouble) nh) * ((gdouble) npn / (gdouble) npd));
    if (diff < 0.0001) {
      GST_DEBUG ("copying AFD metadata, aspect ratio did not change");
      gst_buffer_add_video_afd_meta (dest, smeta->field, smeta->spec,
          smeta->afd);
      return TRUE;
    } else {
      return FALSE;
    }
  } else {
    /* return FALSE, if transform type is not supported */
    return FALSE;
  }

}

const GstMetaInfo *
gst_video_afd_meta_get_info (void)
{
  static const GstMetaInfo *meta_info = NULL;

  if (g_once_init_enter ((GstMetaInfo **) & meta_info)) {
    const GstMetaInfo *mi = gst_meta_register (GST_VIDEO_AFD_META_API_TYPE,
        "GstVideoAFDMeta",
        sizeof (GstVideoAFDMeta),
        gst_video_afd_meta_init,
        NULL,
        gst_video_afd_meta_transform);
    g_once_init_leave ((GstMetaInfo **) & meta_info, (GstMetaInfo *) mi);
  }
  return meta_info;
}

/**
 * gst_buffer_add_video_afd_meta:
 * @buffer: a #GstBuffer
 * @field: 0 for progressive or field 1 and 1 for field 2
 * @spec: #GstVideoAFDSpec that applies to AFD value
 * @afd: #GstVideoAFDValue AFD enumeration
 *
 * Attaches #GstVideoAFDMeta metadata to @buffer with the given
 * parameters.
 *
 * Returns: (transfer none): the #GstVideoAFDMeta on @buffer.
 *
 * Since: 1.18
 */
GstVideoAFDMeta *
gst_buffer_add_video_afd_meta (GstBuffer * buffer, guint8 field,
    GstVideoAFDSpec spec, GstVideoAFDValue afd)
{
  GstVideoAFDMeta *meta;
  gint8 afd_data = (gint8) afd;
  g_return_val_if_fail (GST_IS_BUFFER (buffer), NULL);
  g_return_val_if_fail (field <= 1, NULL);
  g_return_val_if_fail ((guint8) spec <= 2, NULL);
  /* AFD is stored in a nybble */
  g_return_val_if_fail (afd_data <= 0xF, NULL);
  /* reserved values for all specifications */
  g_return_val_if_fail (afd_data != 1 && (afd_data < 5 || afd_data > 7)
      && afd_data != 12, NULL);
  /* reserved for DVB/ETSI */
  g_return_val_if_fail ((spec != GST_VIDEO_AFD_SPEC_DVB_ETSI)
      || (afd_data != 0), NULL);

  meta = (GstVideoAFDMeta *) gst_buffer_add_meta (buffer,
      GST_VIDEO_AFD_META_INFO, NULL);
  g_assert (meta != NULL);

  meta->field = field;
  meta->spec = spec;
  meta->afd = afd;

  return meta;
}

/* Bar Meta implementation */

GType
gst_video_bar_meta_api_get_type (void)
{
  static GType type = 0;

  if (g_once_init_enter (&type)) {
    static const gchar *tags[] = {
      GST_META_TAG_VIDEO_SIZE_STR,
      GST_META_TAG_VIDEO_ORIENTATION_STR,
      GST_META_TAG_VIDEO_STR,
      NULL
    };
    GType _type = gst_meta_api_type_register ("GstVideoBarMetaAPI", tags);
    g_once_init_leave (&type, _type);
  }
  return type;
}

static gboolean
gst_video_bar_meta_init (GstMeta * meta, gpointer params, GstBuffer * buffer)
{
  GstVideoBarMeta *emeta = (GstVideoBarMeta *) meta;

  emeta->field = 0;
  emeta->is_letterbox = FALSE;
  emeta->bar_data1 = 0;
  emeta->bar_data2 = 0;

  return TRUE;
}

static gboolean
gst_video_bar_meta_transform (GstBuffer * dest, GstMeta * meta,
    GstBuffer * buffer, GQuark type, gpointer data)
{
  GstVideoBarMeta *smeta = (GstVideoBarMeta *) meta;

  if (GST_META_TRANSFORM_IS_COPY (type)) {
    GST_DEBUG ("copy Bar metadata");
    gst_buffer_add_video_bar_meta (dest, smeta->field, smeta->is_letterbox,
        smeta->bar_data1, smeta->bar_data2);
    return TRUE;
  } else {
    /* return FALSE, if transform type is not supported */
    return FALSE;
  }
}

const GstMetaInfo *
gst_video_bar_meta_get_info (void)
{
  static const GstMetaInfo *meta_info = NULL;

  if (g_once_init_enter ((GstMetaInfo **) & meta_info)) {
    const GstMetaInfo *mi = gst_meta_register (GST_VIDEO_BAR_META_API_TYPE,
        "GstVideoBarMeta",
        sizeof (GstVideoBarMeta),
        gst_video_bar_meta_init,
        NULL,
        gst_video_bar_meta_transform);
    g_once_init_leave ((GstMetaInfo **) & meta_info, (GstMetaInfo *) mi);
  }
  return meta_info;
}

/**
 * gst_buffer_add_video_bar_meta:
 * @buffer: a #GstBuffer
 * @field: 0 for progressive or field 1 and 1 for field 2
 * @is_letterbox: if true then bar data specifies letterbox, otherwise pillarbox
 * @bar_data1: If @is_letterbox is true, then the value specifies the
 *      last line of a horizontal letterbox bar area at top of reconstructed frame.
 *      Otherwise, it specifies the last horizontal luminance sample of a vertical pillarbox
 *      bar area at the left side of the reconstructed frame
 * @bar_data2: If @is_letterbox is true, then the value specifies the
 *      first line of a horizontal letterbox bar area at bottom of reconstructed frame.
 *      Otherwise, it specifies the first horizontal
 *      luminance sample of a vertical pillarbox bar area at the right side of the reconstructed frame.
 *
 * Attaches #GstVideoBarMeta metadata to @buffer with the given
 * parameters.
 *
 * Returns: (transfer none): the #GstVideoBarMeta on @buffer.
 *
 * See Table 6.11 Bar Data Syntax
 *
 * https://www.atsc.org/wp-content/uploads/2015/03/a_53-Part-4-2009.pdf
 *
 * Since: 1.18
 */
GstVideoBarMeta *
gst_buffer_add_video_bar_meta (GstBuffer * buffer, guint8 field,
    gboolean is_letterbox, guint bar_data1, guint bar_data2)
{
  GstVideoBarMeta *meta;

  g_return_val_if_fail (GST_IS_BUFFER (buffer), NULL);
  g_return_val_if_fail (field <= 1, NULL);

  meta = (GstVideoBarMeta *) gst_buffer_add_meta (buffer,
      GST_VIDEO_BAR_META_INFO, NULL);
  g_assert (meta != NULL);

  meta->field = field;
  meta->is_letterbox = is_letterbox;
  meta->bar_data1 = bar_data1;
  meta->bar_data2 = bar_data2;
  return meta;
}
