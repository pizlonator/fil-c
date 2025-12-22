/*
 * Microsoft Smooth-Streaming fragment parsing library
 *
 * gstmssfragmentparser.h
 *
 * Copyright (C) 2016 Igalia S.L
 * Copyright (C) 2016 Metrological
 *   Author: Philippe Normand <philn@igalia.com>
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

#include "gstmssfragmentparser.h"
#include <gst/base/gstbytereader.h>
#include <string.h>

GST_DEBUG_CATEGORY_EXTERN (mssdemux_debug);
#define GST_CAT_DEFAULT mssdemux_debug

void
gst_mss_fragment_parser_init (GstMssFragmentParser * parser)
{
  parser->status = GST_MSS_FRAGMENT_HEADER_PARSER_INIT;
}

void
gst_mss_fragment_parser_clear (GstMssFragmentParser * parser)
{
  if (parser->moof)
    gst_isoff_moof_box_free (parser->moof);
  parser->moof = NULL;
  parser->current_fourcc = 0;
}

gboolean
gst_mss_fragment_parser_add_buffer (GstMssFragmentParser * parser,
    GstBuffer * buffer)
{
  GstByteReader reader;
  GstMapInfo info;
  guint64 size;
  guint32 fourcc;
  guint header_size;
  gboolean error = FALSE;

  if (!gst_buffer_map (buffer, &info, GST_MAP_READ)) {
    return FALSE;
  }

  gst_byte_reader_init (&reader, info.data, info.size);
  GST_TRACE ("Total buffer size: %u", gst_byte_reader_get_size (&reader));

  do {
    parser->current_fourcc = 0;

    if (!gst_isoff_parse_box_header (&reader, &fourcc, NULL, &header_size,
            &size)) {
      break;
    }

    parser->current_fourcc = fourcc;

    GST_LOG ("box %" GST_FOURCC_FORMAT " size %" G_GUINT64_FORMAT,
        GST_FOURCC_ARGS (fourcc), size);

    parser->current_fourcc = fourcc;

    if (parser->current_fourcc == GST_ISOFF_FOURCC_MOOF) {
      GstByteReader sub_reader;

      g_assert (parser->moof == NULL);
      gst_byte_reader_get_sub_reader (&reader, &sub_reader, size - header_size);
      parser->moof = gst_isoff_moof_box_parse (&sub_reader);
      if (parser->moof == NULL) {
        GST_ERROR ("Failed to parse moof");
        error = TRUE;
      }
    } else if (parser->current_fourcc == GST_ISOFF_FOURCC_MDAT) {
      goto beach;
    } else {
      gst_byte_reader_skip (&reader, size - header_size);
    }
  } while (gst_byte_reader_get_remaining (&reader) > 0);

beach:

  /* Do sanity check */
  if (parser->current_fourcc != GST_ISOFF_FOURCC_MDAT || !parser->moof ||
      parser->moof->traf->len == 0)
    error = TRUE;

  if (!error) {
    GstTrafBox *traf = &g_array_index (parser->moof->traf, GstTrafBox, 0);
    if (!traf->tfxd) {
      GST_ERROR ("no tfxd box");
      error = TRUE;
    } else if (!traf->tfrf) {
      GST_ERROR ("no tfrf box");
      error = TRUE;
    }
  }

  if (!error)
    parser->status = GST_MSS_FRAGMENT_HEADER_PARSER_FINISHED;

  GST_LOG ("Fragment parsing successful: %s", error ? "no" : "yes");
  gst_buffer_unmap (buffer, &info);
  return !error;
}
