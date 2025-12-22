/*
 * ISO File Format parsing library
 *
 * gstisoff.h
 *
 * Copyright (C) 2015 Samsung Electronics. All rights reserved.
 *   Author: Thiago Santos <thiagoss@osg.samsung.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library (COPYING); if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gstisoff.h"
#include <gst/base/gstbytereader.h>

#include <string.h>

GST_DEBUG_CATEGORY_STATIC (gst_isoff_debug);
#define GST_CAT_DEFAULT gst_isoff_debug

static gboolean initialized = FALSE;

#define INITIALIZE_DEBUG_CATEGORY \
  if (!initialized) { \
  GST_DEBUG_CATEGORY_INIT (gst_isoff_debug, "isoff", 0, \
      "ISO File Format parsing library"); \
    initialized = TRUE; \
  }

static const guint8 tfrf_uuid[] = {
  0xd4, 0x80, 0x7e, 0xf2, 0xca, 0x39, 0x46, 0x95,
  0x8e, 0x54, 0x26, 0xcb, 0x9e, 0x46, 0xa7, 0x9f
};

static const guint8 tfxd_uuid[] = {
  0x6d, 0x1d, 0x9b, 0x05, 0x42, 0xd5, 0x44, 0xe6,
  0x80, 0xe2, 0x14, 0x1d, 0xaf, 0xf7, 0x57, 0xb2
};

/* gst_isoff_parse_box_header:
 * @reader:
 * @type: type that was found at the current position
 * @extended_type: (allow-none): extended type if type=='uuid'
 * @header_size: (allow-none): size of the box header (type, extended type and size)
 * @size: size of the complete box including type, extended type and size
 *
 * Advances the byte reader to the start of the box content. To skip
 * over the complete box, skip size - header_size bytes.
 *
 * Returns: TRUE if a box header could be parsed, FALSE if more data is needed
 */
gboolean
gst_isoff_parse_box_header (GstByteReader * reader, guint32 * type,
    guint8 extended_type[16], guint * header_size, guint64 * size)
{
  guint header_start_offset;
  guint32 size_field;

  INITIALIZE_DEBUG_CATEGORY;
  header_start_offset = gst_byte_reader_get_pos (reader);

  if (gst_byte_reader_get_remaining (reader) < 8)
    goto not_enough_data;

  size_field = gst_byte_reader_get_uint32_be_unchecked (reader);
  *type = gst_byte_reader_get_uint32_le_unchecked (reader);

  if (size_field == 1) {
    if (gst_byte_reader_get_remaining (reader) < 8)
      goto not_enough_data;
    *size = gst_byte_reader_get_uint64_be_unchecked (reader);
  } else {
    *size = size_field;
  }

  if (*type == GST_ISOFF_FOURCC_UUID) {
    if (gst_byte_reader_get_remaining (reader) < 16)
      goto not_enough_data;

    if (extended_type)
      memcpy (extended_type, gst_byte_reader_get_data_unchecked (reader, 16),
          16);
  }

  if (header_size)
    *header_size = gst_byte_reader_get_pos (reader) - header_start_offset;

  return TRUE;

not_enough_data:
  gst_byte_reader_set_pos (reader, header_start_offset);
  return FALSE;
}

static void
gst_isoff_trun_box_clear (GstTrunBox * trun)
{
  if (trun->samples)
    g_array_free (trun->samples, TRUE);
}

static void
gst_isoff_tfrf_box_free (GstTfrfBox * tfrf)
{
  if (tfrf->entries)
    g_array_free (tfrf->entries, TRUE);

  g_free (tfrf);
}

static void
gst_isoff_traf_box_clear (GstTrafBox * traf)
{
  if (traf->trun)
    g_array_free (traf->trun, TRUE);

  if (traf->tfrf)
    gst_isoff_tfrf_box_free (traf->tfrf);

  g_free (traf->tfxd);
  traf->trun = NULL;
  traf->tfrf = NULL;
  traf->tfxd = NULL;
}

static gboolean
gst_isoff_mfhd_box_parse (GstMfhdBox * mfhd, GstByteReader * reader)
{
  guint8 version;
  guint32 flags;

  if (gst_byte_reader_get_remaining (reader) != 8)
    return FALSE;

  version = gst_byte_reader_get_uint8_unchecked (reader);
  if (version != 0)
    return FALSE;

  flags = gst_byte_reader_get_uint24_be_unchecked (reader);
  if (flags != 0)
    return FALSE;

  mfhd->sequence_number = gst_byte_reader_get_uint32_be_unchecked (reader);

  return TRUE;
}

static gboolean
gst_isoff_tfhd_box_parse (GstTfhdBox * tfhd, GstByteReader * reader)
{
  memset (tfhd, 0, sizeof (*tfhd));

  if (gst_byte_reader_get_remaining (reader) < 4)
    return FALSE;

  tfhd->version = gst_byte_reader_get_uint8_unchecked (reader);
  if (tfhd->version != 0)
    return FALSE;

  tfhd->flags = gst_byte_reader_get_uint24_be_unchecked (reader);

  if (!gst_byte_reader_get_uint32_be (reader, &tfhd->track_id))
    return FALSE;

  if ((tfhd->flags & GST_TFHD_FLAGS_BASE_DATA_OFFSET_PRESENT) &&
      !gst_byte_reader_get_uint64_be (reader, &tfhd->base_data_offset))
    return FALSE;

  if ((tfhd->flags & GST_TFHD_FLAGS_SAMPLE_DESCRIPTION_INDEX_PRESENT) &&
      !gst_byte_reader_get_uint32_be (reader, &tfhd->sample_description_index))
    return FALSE;

  if ((tfhd->flags & GST_TFHD_FLAGS_DEFAULT_SAMPLE_DURATION_PRESENT) &&
      !gst_byte_reader_get_uint32_be (reader, &tfhd->default_sample_duration))
    return FALSE;

  if ((tfhd->flags & GST_TFHD_FLAGS_DEFAULT_SAMPLE_SIZE_PRESENT) &&
      !gst_byte_reader_get_uint32_be (reader, &tfhd->default_sample_size))
    return FALSE;

  if ((tfhd->flags & GST_TFHD_FLAGS_DEFAULT_SAMPLE_FLAGS_PRESENT) &&
      !gst_byte_reader_get_uint32_be (reader, &tfhd->default_sample_flags))
    return FALSE;

  return TRUE;
}

static gboolean
gst_isoff_trun_box_parse (GstTrunBox * trun, GstByteReader * reader)
{
  gint i;

  memset (trun, 0, sizeof (*trun));

  if (gst_byte_reader_get_remaining (reader) < 4)
    return FALSE;

  trun->version = gst_byte_reader_get_uint8_unchecked (reader);
  if (trun->version != 0 && trun->version != 1)
    return FALSE;

  trun->flags = gst_byte_reader_get_uint24_be_unchecked (reader);

  if (!gst_byte_reader_get_uint32_be (reader, &trun->sample_count))
    return FALSE;

  trun->samples =
      g_array_sized_new (FALSE, FALSE, sizeof (GstTrunSample),
      trun->sample_count);

  if ((trun->flags & GST_TRUN_FLAGS_DATA_OFFSET_PRESENT) &&
      !gst_byte_reader_get_uint32_be (reader, (guint32 *) & trun->data_offset))
    return FALSE;

  if ((trun->flags & GST_TRUN_FLAGS_FIRST_SAMPLE_FLAGS_PRESENT) &&
      !gst_byte_reader_get_uint32_be (reader, &trun->first_sample_flags))
    return FALSE;

  for (i = 0; i < trun->sample_count; i++) {
    GstTrunSample sample = { 0, };

    if ((trun->flags & GST_TRUN_FLAGS_SAMPLE_DURATION_PRESENT) &&
        !gst_byte_reader_get_uint32_be (reader, &sample.sample_duration))
      goto error;

    if ((trun->flags & GST_TRUN_FLAGS_SAMPLE_SIZE_PRESENT) &&
        !gst_byte_reader_get_uint32_be (reader, &sample.sample_size))
      goto error;

    if ((trun->flags & GST_TRUN_FLAGS_SAMPLE_FLAGS_PRESENT) &&
        !gst_byte_reader_get_uint32_be (reader, &sample.sample_flags))
      goto error;

    if ((trun->flags & GST_TRUN_FLAGS_SAMPLE_COMPOSITION_TIME_OFFSETS_PRESENT)
        && !gst_byte_reader_get_uint32_be (reader,
            &sample.sample_composition_time_offset.u))
      goto error;

    g_array_append_val (trun->samples, sample);
  }

  return TRUE;

error:
  gst_isoff_trun_box_clear (trun);
  return FALSE;
}

static gboolean
gst_isoff_tfdt_box_parse (GstTfdtBox * tfdt, GstByteReader * reader)
{
  gint8 version;

  memset (tfdt, 0, sizeof (*tfdt));

  if (gst_byte_reader_get_remaining (reader) < 4)
    return FALSE;

  version = gst_byte_reader_get_uint8_unchecked (reader);

  if (!gst_byte_reader_skip (reader, 3))
    return FALSE;

  if (version == 1) {
    if (!gst_byte_reader_get_uint64_be (reader, &tfdt->decode_time))
      return FALSE;
  } else {
    guint32 dec_time = 0;
    if (!gst_byte_reader_get_uint32_be (reader, &dec_time))
      return FALSE;
    tfdt->decode_time = dec_time;
  }

  return TRUE;
}

static gboolean
gst_isoff_tfxd_box_parse (GstTfxdBox * tfxd, GstByteReader * reader)
{
  guint8 version;
  guint32 flags = 0;
  guint64 absolute_time = 0;
  guint64 absolute_duration = 0;

  memset (tfxd, 0, sizeof (*tfxd));

  if (gst_byte_reader_get_remaining (reader) < 4)
    return FALSE;

  if (!gst_byte_reader_get_uint8 (reader, &version)) {
    GST_ERROR ("Error getting box's version field");
    return FALSE;
  }

  if (!gst_byte_reader_get_uint24_be (reader, &flags)) {
    GST_ERROR ("Error getting box's flags field");
    return FALSE;
  }

  tfxd->version = version;
  tfxd->flags = flags;

  if (gst_byte_reader_get_remaining (reader) < ((version & 0x01) ? 16 : 8))
    return FALSE;

  if (version & 0x01) {
    gst_byte_reader_get_uint64_be (reader, &absolute_time);
    gst_byte_reader_get_uint64_be (reader, &absolute_duration);
  } else {
    guint32 time = 0;
    guint32 duration = 0;
    gst_byte_reader_get_uint32_be (reader, &time);
    gst_byte_reader_get_uint32_be (reader, &duration);
    absolute_time = time;
    absolute_duration = duration;
  }

  tfxd->time = absolute_time;
  tfxd->duration = absolute_duration;

  return TRUE;
}

static gboolean
gst_isoff_tfrf_box_parse (GstTfrfBox * tfrf, GstByteReader * reader)
{
  guint8 version;
  guint32 flags = 0;
  guint8 fragment_count = 0;
  guint8 index = 0;

  memset (tfrf, 0, sizeof (*tfrf));

  if (gst_byte_reader_get_remaining (reader) < 4)
    return FALSE;

  if (!gst_byte_reader_get_uint8 (reader, &version)) {
    GST_ERROR ("Error getting box's version field");
    return FALSE;
  }

  if (!gst_byte_reader_get_uint24_be (reader, &flags)) {
    GST_ERROR ("Error getting box's flags field");
    return FALSE;
  }

  tfrf->version = version;
  tfrf->flags = flags;

  if (!gst_byte_reader_get_uint8 (reader, &fragment_count))
    return FALSE;

  tfrf->entries_count = fragment_count;
  tfrf->entries =
      g_array_sized_new (FALSE, FALSE, sizeof (GstTfrfBoxEntry),
      tfrf->entries_count);

  for (index = 0; index < fragment_count; index++) {
    GstTfrfBoxEntry entry = { 0, };
    guint64 absolute_time = 0;
    guint64 absolute_duration = 0;
    if (gst_byte_reader_get_remaining (reader) < ((version & 0x01) ? 16 : 8))
      return FALSE;

    if (version & 0x01) {
      if (!gst_byte_reader_get_uint64_be (reader, &absolute_time) ||
          !gst_byte_reader_get_uint64_be (reader, &absolute_duration)) {
        return FALSE;
      }
    } else {
      guint32 time = 0;
      guint32 duration = 0;
      if (!gst_byte_reader_get_uint32_be (reader, &time) ||
          !gst_byte_reader_get_uint32_be (reader, &duration)) {
        return FALSE;
      }
      absolute_time = time;
      absolute_duration = duration;
    }
    entry.time = absolute_time;
    entry.duration = absolute_duration;

    g_array_append_val (tfrf->entries, entry);
  }

  return TRUE;
}

static gboolean
gst_isoff_traf_box_parse (GstTrafBox * traf, GstByteReader * reader)
{
  gboolean had_tfhd = FALSE;

  memset (traf, 0, sizeof (*traf));
  traf->trun = g_array_new (FALSE, FALSE, sizeof (GstTrunBox));
  g_array_set_clear_func (traf->trun,
      (GDestroyNotify) gst_isoff_trun_box_clear);

  traf->tfdt.decode_time = GST_CLOCK_TIME_NONE;

  while (gst_byte_reader_get_remaining (reader) > 0) {
    guint32 fourcc;
    guint header_size;
    guint64 size;
    GstByteReader sub_reader;
    guint8 extended_type[16] = { 0, };

    if (!gst_isoff_parse_box_header (reader, &fourcc, extended_type,
            &header_size, &size))
      goto error;
    if (gst_byte_reader_get_remaining (reader) < size - header_size)
      goto error;

    switch (fourcc) {
      case GST_ISOFF_FOURCC_TFHD:{
        gst_byte_reader_get_sub_reader (reader, &sub_reader,
            size - header_size);
        if (!gst_isoff_tfhd_box_parse (&traf->tfhd, &sub_reader))
          goto error;
        had_tfhd = TRUE;
        break;
      }
      case GST_ISOFF_FOURCC_TFDT:{
        gst_byte_reader_get_sub_reader (reader, &sub_reader,
            size - header_size);
        if (!gst_isoff_tfdt_box_parse (&traf->tfdt, &sub_reader))
          goto error;
        break;
      }
      case GST_ISOFF_FOURCC_TRUN:{
        GstTrunBox trun;

        gst_byte_reader_get_sub_reader (reader, &sub_reader,
            size - header_size);
        if (!gst_isoff_trun_box_parse (&trun, &sub_reader))
          goto error;

        g_array_append_val (traf->trun, trun);
        break;
      }
      case GST_ISOFF_FOURCC_UUID:{
        /* smooth-streaming specific */
        if (memcmp (extended_type, tfrf_uuid, 16) == 0) {
          traf->tfrf = g_new0 (GstTfrfBox, 1);
          gst_byte_reader_get_sub_reader (reader, &sub_reader,
              size - header_size);

          if (!gst_isoff_tfrf_box_parse (traf->tfrf, &sub_reader))
            goto error;
        } else if (memcmp (extended_type, tfxd_uuid, 16) == 0) {
          traf->tfxd = g_new0 (GstTfxdBox, 1);
          gst_byte_reader_get_sub_reader (reader, &sub_reader,
              size - header_size);

          if (!gst_isoff_tfxd_box_parse (traf->tfxd, &sub_reader))
            goto error;
        } else {
          gst_byte_reader_skip (reader, size - header_size);
        }
        break;
      }
      default:
        gst_byte_reader_skip (reader, size - header_size);
        break;
    }
  }

  if (!had_tfhd)
    goto error;

  return TRUE;

error:
  gst_isoff_traf_box_clear (traf);

  return FALSE;
}

GstMoofBox *
gst_isoff_moof_box_parse (GstByteReader * reader)
{
  GstMoofBox *moof;
  gboolean had_mfhd = FALSE;
  GstByteReader sub_reader;


  INITIALIZE_DEBUG_CATEGORY;
  moof = g_new0 (GstMoofBox, 1);
  moof->traf = g_array_new (FALSE, FALSE, sizeof (GstTrafBox));
  g_array_set_clear_func (moof->traf,
      (GDestroyNotify) gst_isoff_traf_box_clear);

  while (gst_byte_reader_get_remaining (reader) > 0) {
    guint32 fourcc;
    guint header_size;
    guint64 size;

    if (!gst_isoff_parse_box_header (reader, &fourcc, NULL, &header_size,
            &size))
      goto error;
    if (gst_byte_reader_get_remaining (reader) < size - header_size)
      goto error;

    switch (fourcc) {
      case GST_ISOFF_FOURCC_MFHD:{
        gst_byte_reader_get_sub_reader (reader, &sub_reader,
            size - header_size);
        if (!gst_isoff_mfhd_box_parse (&moof->mfhd, &sub_reader))
          goto error;
        had_mfhd = TRUE;
        break;
      }
      case GST_ISOFF_FOURCC_TRAF:{
        GstTrafBox traf;

        gst_byte_reader_get_sub_reader (reader, &sub_reader,
            size - header_size);
        if (!gst_isoff_traf_box_parse (&traf, &sub_reader))
          goto error;

        g_array_append_val (moof->traf, traf);
        break;
      }
      default:
        gst_byte_reader_skip (reader, size - header_size);
        break;
    }
  }

  if (!had_mfhd)
    goto error;

  return moof;

error:
  gst_isoff_moof_box_free (moof);
  return NULL;
}

void
gst_isoff_moof_box_free (GstMoofBox * moof)
{
  g_array_free (moof->traf, TRUE);
  g_free (moof);
}

static gboolean
gst_isoff_mdhd_box_parse (GstMdhdBox * mdhd, GstByteReader * reader)
{
  guint8 version;

  memset (mdhd, 0, sizeof (*mdhd));

  if (gst_byte_reader_get_remaining (reader) < 4)
    return FALSE;

  version = gst_byte_reader_get_uint8_unchecked (reader);

  if (!gst_byte_reader_skip (reader, 3))
    return FALSE;

  /* skip {creation, modification}_time, we don't have interest */
  if (version == 1) {
    if (!gst_byte_reader_skip (reader, 16))
      return FALSE;
  } else {
    if (!gst_byte_reader_skip (reader, 8))
      return FALSE;
  }

  if (!gst_byte_reader_get_uint32_be (reader, &mdhd->timescale))
    return FALSE;

  return TRUE;
}

static gboolean
gst_isoff_hdlr_box_parse (GstHdlrBox * hdlr, GstByteReader * reader)
{
  memset (hdlr, 0, sizeof (*hdlr));

  if (gst_byte_reader_get_remaining (reader) < 4)
    return FALSE;

  /* version & flag */
  if (!gst_byte_reader_skip (reader, 4))
    return FALSE;

  /* pre_defined = 0 */
  if (!gst_byte_reader_skip (reader, 4))
    return FALSE;

  if (!gst_byte_reader_get_uint32_le (reader, &hdlr->handler_type))
    return FALSE;

  return TRUE;
}

static gboolean
gst_isoff_mdia_box_parse (GstMdiaBox * mdia, GstByteReader * reader)
{
  gboolean had_mdhd = FALSE, had_hdlr = FALSE;
  while (gst_byte_reader_get_remaining (reader) > 0) {
    guint32 fourcc;
    guint header_size;
    guint64 size;
    GstByteReader sub_reader;

    if (!gst_isoff_parse_box_header (reader, &fourcc, NULL, &header_size,
            &size))
      return FALSE;
    if (gst_byte_reader_get_remaining (reader) < size - header_size)
      return FALSE;

    switch (fourcc) {
      case GST_ISOFF_FOURCC_MDHD:{
        gst_byte_reader_get_sub_reader (reader, &sub_reader,
            size - header_size);
        if (!gst_isoff_mdhd_box_parse (&mdia->mdhd, &sub_reader))
          return FALSE;

        had_mdhd = TRUE;
        break;
      }
      case GST_ISOFF_FOURCC_HDLR:{
        gst_byte_reader_get_sub_reader (reader, &sub_reader,
            size - header_size);
        if (!gst_isoff_hdlr_box_parse (&mdia->hdlr, &sub_reader))
          return FALSE;

        had_hdlr = TRUE;
        break;
      }
      default:
        gst_byte_reader_skip (reader, size - header_size);
        break;
    }
  }

  if (!had_mdhd || !had_hdlr)
    return FALSE;

  return TRUE;
}

static gboolean
gst_isoff_tkhd_box_parse (GstTkhdBox * tkhd, GstByteReader * reader)
{
  guint8 version;

  memset (tkhd, 0, sizeof (*tkhd));

  if (gst_byte_reader_get_remaining (reader) < 4)
    return FALSE;

  if (!gst_byte_reader_get_uint8 (reader, &version))
    return FALSE;

  if (!gst_byte_reader_skip (reader, 3))
    return FALSE;

  /* skip {creation, modification}_time, we don't have interest */
  if (version == 1) {
    if (!gst_byte_reader_skip (reader, 16))
      return FALSE;
  } else {
    if (!gst_byte_reader_skip (reader, 8))
      return FALSE;
  }

  if (!gst_byte_reader_get_uint32_be (reader, &tkhd->track_id))
    return FALSE;

  return TRUE;
}

static gboolean
gst_isoff_trak_box_parse (GstTrakBox * trak, GstByteReader * reader)
{
  gboolean had_mdia = FALSE, had_tkhd = FALSE;
  while (gst_byte_reader_get_remaining (reader) > 0) {
    guint32 fourcc;
    guint header_size;
    guint64 size;
    GstByteReader sub_reader;

    if (!gst_isoff_parse_box_header (reader, &fourcc, NULL, &header_size,
            &size))
      return FALSE;
    if (gst_byte_reader_get_remaining (reader) < size - header_size)
      return FALSE;

    switch (fourcc) {
      case GST_ISOFF_FOURCC_MDIA:{
        gst_byte_reader_get_sub_reader (reader, &sub_reader,
            size - header_size);
        if (!gst_isoff_mdia_box_parse (&trak->mdia, &sub_reader))
          return FALSE;

        had_mdia = TRUE;
        break;
      }
      case GST_ISOFF_FOURCC_TKHD:{
        gst_byte_reader_get_sub_reader (reader, &sub_reader,
            size - header_size);
        if (!gst_isoff_tkhd_box_parse (&trak->tkhd, &sub_reader))
          return FALSE;

        had_tkhd = TRUE;
        break;
      }
      default:
        gst_byte_reader_skip (reader, size - header_size);
        break;
    }
  }

  if (!had_tkhd || !had_mdia)
    return FALSE;

  return TRUE;
}

GstMoovBox *
gst_isoff_moov_box_parse (GstByteReader * reader)
{
  GstMoovBox *moov;
  gboolean had_trak = FALSE;
  moov = g_new0 (GstMoovBox, 1);
  moov->trak = g_array_new (FALSE, FALSE, sizeof (GstTrakBox));

  while (gst_byte_reader_get_remaining (reader) > 0) {
    guint32 fourcc;
    guint header_size;
    guint64 size;

    if (!gst_isoff_parse_box_header (reader, &fourcc, NULL, &header_size,
            &size))
      goto error;
    if (gst_byte_reader_get_remaining (reader) < size - header_size)
      goto error;

    switch (fourcc) {
      case GST_ISOFF_FOURCC_TRAK:{
        GstByteReader sub_reader;
        GstTrakBox trak;

        gst_byte_reader_get_sub_reader (reader, &sub_reader,
            size - header_size);
        if (!gst_isoff_trak_box_parse (&trak, &sub_reader))
          goto error;

        had_trak = TRUE;
        g_array_append_val (moov->trak, trak);
        break;
      }
      default:
        gst_byte_reader_skip (reader, size - header_size);
        break;
    }
  }

  if (!had_trak)
    goto error;

  return moov;

error:
  gst_isoff_moov_box_free (moov);
  return NULL;
}

void
gst_isoff_moov_box_free (GstMoovBox * moov)
{
  g_array_free (moov->trak, TRUE);
  g_free (moov);
}

void
gst_isoff_sidx_parser_init (GstSidxParser * parser)
{
  parser->status = GST_ISOFF_SIDX_PARSER_INIT;
  parser->cumulative_entry_size = 0;
  parser->sidx.entries = NULL;
  parser->sidx.entries_count = 0;
}

void
gst_isoff_sidx_parser_clear (GstSidxParser * parser)
{
  g_free (parser->sidx.entries);
  memset (parser, 0, sizeof (*parser));

  gst_isoff_sidx_parser_init (parser);
}

static void
gst_isoff_parse_sidx_entry (GstSidxBoxEntry * entry, GstByteReader * reader)
{
  guint32 aux;

  aux = gst_byte_reader_get_uint32_be_unchecked (reader);
  entry->ref_type = aux >> 31;
  entry->size = aux & 0x7FFFFFFF;
  entry->duration = gst_byte_reader_get_uint32_be_unchecked (reader);
  aux = gst_byte_reader_get_uint32_be_unchecked (reader);
  entry->starts_with_sap = aux >> 31;
  entry->sap_type = ((aux >> 28) & 0x7);
  entry->sap_delta_time = aux & 0xFFFFFFF;
}

GstIsoffParserResult
gst_isoff_sidx_parser_parse (GstSidxParser * parser,
    GstByteReader * reader, guint * consumed)
{
  GstIsoffParserResult res = GST_ISOFF_PARSER_OK;
  gsize remaining;

  INITIALIZE_DEBUG_CATEGORY;
  switch (parser->status) {
    case GST_ISOFF_SIDX_PARSER_INIT:
      /* Try again once we have enough data for the FullBox header */
      if (gst_byte_reader_get_remaining (reader) < 4) {
        gst_byte_reader_set_pos (reader, 0);
        break;
      }
      parser->sidx.version = gst_byte_reader_get_uint8_unchecked (reader);
      parser->sidx.flags = gst_byte_reader_get_uint24_le_unchecked (reader);

      parser->status = GST_ISOFF_SIDX_PARSER_HEADER;

    case GST_ISOFF_SIDX_PARSER_HEADER:
      remaining = gst_byte_reader_get_remaining (reader);
      if (remaining < 12 + (parser->sidx.version == 0 ? 8 : 16)) {
        break;
      }

      parser->sidx.ref_id = gst_byte_reader_get_uint32_be_unchecked (reader);
      parser->sidx.timescale = gst_byte_reader_get_uint32_be_unchecked (reader);
      if (parser->sidx.version == 0) {
        parser->sidx.earliest_pts =
            gst_byte_reader_get_uint32_be_unchecked (reader);
        parser->sidx.first_offset =
            gst_byte_reader_get_uint32_be_unchecked (reader);
      } else {
        parser->sidx.earliest_pts =
            gst_byte_reader_get_uint64_be_unchecked (reader);
        parser->sidx.first_offset =
            gst_byte_reader_get_uint64_be_unchecked (reader);
      }
      /* skip 2 reserved bytes */
      gst_byte_reader_skip_unchecked (reader, 2);
      parser->sidx.entries_count =
          gst_byte_reader_get_uint16_be_unchecked (reader);

      GST_LOG ("Timescale: %" G_GUINT32_FORMAT, parser->sidx.timescale);
      GST_LOG ("Earliest pts: %" G_GUINT64_FORMAT, parser->sidx.earliest_pts);
      GST_LOG ("First offset: %" G_GUINT64_FORMAT, parser->sidx.first_offset);

      parser->cumulative_pts =
          gst_util_uint64_scale_int_round (parser->sidx.earliest_pts,
          GST_SECOND, parser->sidx.timescale);

      if (parser->sidx.entries_count) {
        parser->sidx.entries =
            g_malloc (sizeof (GstSidxBoxEntry) * parser->sidx.entries_count);
      }
      parser->sidx.entry_index = 0;

      parser->status = GST_ISOFF_SIDX_PARSER_DATA;

    case GST_ISOFF_SIDX_PARSER_DATA:
      while (parser->sidx.entry_index < parser->sidx.entries_count) {
        GstSidxBoxEntry *entry =
            &parser->sidx.entries[parser->sidx.entry_index];

        remaining = gst_byte_reader_get_remaining (reader);
        if (remaining < 12)
          break;

        entry->offset = parser->cumulative_entry_size;
        entry->pts = parser->cumulative_pts;
        gst_isoff_parse_sidx_entry (entry, reader);
        entry->duration = gst_util_uint64_scale_int_round (entry->duration,
            GST_SECOND, parser->sidx.timescale);
        parser->cumulative_entry_size += entry->size;
        parser->cumulative_pts += entry->duration;

        GST_LOG ("Sidx entry %d) offset: %" G_GUINT64_FORMAT ", pts: %"
            GST_TIME_FORMAT ", duration %" GST_TIME_FORMAT " - size %"
            G_GUINT32_FORMAT, parser->sidx.entry_index, entry->offset,
            GST_TIME_ARGS (entry->pts), GST_TIME_ARGS (entry->duration),
            entry->size);

        parser->sidx.entry_index++;
      }

      if (parser->sidx.entry_index == parser->sidx.entries_count)
        parser->status = GST_ISOFF_SIDX_PARSER_FINISHED;
      else
        break;
    case GST_ISOFF_SIDX_PARSER_FINISHED:
      parser->sidx.entry_index = 0;
      res = GST_ISOFF_PARSER_DONE;
      break;
  }

  *consumed = gst_byte_reader_get_pos (reader);

  return res;
}

GstIsoffParserResult
gst_isoff_sidx_parser_add_buffer (GstSidxParser * parser, GstBuffer * buffer,
    guint * consumed)
{
  GstIsoffParserResult res = GST_ISOFF_PARSER_OK;
  GstByteReader reader;
  GstMapInfo info;
  guint32 fourcc;

  INITIALIZE_DEBUG_CATEGORY;
  if (!gst_buffer_map (buffer, &info, GST_MAP_READ)) {
    *consumed = 0;
    return GST_ISOFF_PARSER_ERROR;
  }

  gst_byte_reader_init (&reader, info.data, info.size);

  if (parser->status == GST_ISOFF_SIDX_PARSER_INIT) {
    if (!gst_isoff_parse_box_header (&reader, &fourcc, NULL, NULL,
            &parser->size))
      goto done;

    if (fourcc != GST_ISOFF_FOURCC_SIDX) {
      res = GST_ISOFF_PARSER_UNEXPECTED;
      gst_byte_reader_set_pos (&reader, 0);
      goto done;
    }

    if (parser->size == 0) {
      res = GST_ISOFF_PARSER_ERROR;
      gst_byte_reader_set_pos (&reader, 0);
      goto done;
    }

    /* Try again once we have enough data for the FullBox header */
    if (gst_byte_reader_get_remaining (&reader) < 4) {
      gst_byte_reader_set_pos (&reader, 0);
      goto done;
    }
  }

  res = gst_isoff_sidx_parser_parse (parser, &reader, consumed);

done:
  gst_buffer_unmap (buffer, &info);
  return res;
}
