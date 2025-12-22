/* GStreamer JPEG parser test utility
 * Copyright (C) 2015 Tim-Philipp Müller <tim@centricular.com>
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
#include "config.h"
#endif

#include <gst/gst.h>
#include <gst/codecparsers/gstjpegparser.h>

#include <stdlib.h>

static GstBuffer *app_segments[16];     /* NULL */

static const gchar *
get_marker_name (guint8 marker)
{
  switch (marker) {
    case GST_JPEG_MARKER_SOF0:
      return "SOF (Baseline)";
    case GST_JPEG_MARKER_SOF1:
      return "SOF (Extended Sequential, Huffman)";
    case GST_JPEG_MARKER_SOF2:
      return "SOF (Extended Progressive, Huffman)";
    case GST_JPEG_MARKER_SOF3:
      return "SOF (Lossless, Huffman)";
    case GST_JPEG_MARKER_SOF5:
      return "SOF (Differential Sequential, Huffman)";
    case GST_JPEG_MARKER_SOF6:
      return "SOF (Differential Progressive, Huffman)";
    case GST_JPEG_MARKER_SOF7:
      return "SOF (Differential Lossless, Huffman)";
    case GST_JPEG_MARKER_SOF9:
      return "SOF (Extended Sequential, Arithmetic)";
    case GST_JPEG_MARKER_SOF10:
      return "SOF (Progressive, Arithmetic)";
    case GST_JPEG_MARKER_SOF11:
      return "SOF (Lossless, Arithmetic)";
    case GST_JPEG_MARKER_SOF13:
      return "SOF (Differential Sequential, Arithmetic)";
    case GST_JPEG_MARKER_SOF14:
      return "SOF (Differential Progressive, Arithmetic)";
    case GST_JPEG_MARKER_SOF15:
      return "SOF (Differential Lossless, Arithmetic)";
    case GST_JPEG_MARKER_DHT:
      return "DHT";
    case GST_JPEG_MARKER_DAC:
      return "DAC";
    case GST_JPEG_MARKER_SOI:
      return "SOI";
    case GST_JPEG_MARKER_EOI:
      return "EOI";
    case GST_JPEG_MARKER_SOS:
      return "SOS";
    case GST_JPEG_MARKER_DQT:
      return "DQT";
    case GST_JPEG_MARKER_DNL:
      return "DNL";
    case GST_JPEG_MARKER_DRI:
      return "DRI";
    case GST_JPEG_MARKER_APP0:
      return "APP0";
    case GST_JPEG_MARKER_APP1:
      return "APP1";
    case GST_JPEG_MARKER_APP2:
      return "APP2";
    case GST_JPEG_MARKER_APP3:
      return "APP3";
    case GST_JPEG_MARKER_APP4:
      return "APP4";
    case GST_JPEG_MARKER_APP5:
      return "APP5";
    case GST_JPEG_MARKER_APP6:
      return "APP6";
    case GST_JPEG_MARKER_APP7:
      return "APP7";
    case GST_JPEG_MARKER_APP8:
      return "APP8";
    case GST_JPEG_MARKER_APP9:
      return "APP9";
    case GST_JPEG_MARKER_APP10:
      return "APP10";
    case GST_JPEG_MARKER_APP11:
      return "APP11";
    case GST_JPEG_MARKER_APP12:
      return "APP12";
    case GST_JPEG_MARKER_APP13:
      return "APP13";
    case GST_JPEG_MARKER_APP14:
      return "APP14";
    case GST_JPEG_MARKER_APP15:
      return "APP15";
    case GST_JPEG_MARKER_COM:
      return "COM";
    default:
      if (marker > GST_JPEG_MARKER_RST_MIN && marker < GST_JPEG_MARKER_RST_MAX)
        return "RST";
      break;
  }
  return "???";
}

static gboolean
parse_jpeg_segment (GstJpegSegment * segment)
{
  switch (segment->marker) {
    case GST_JPEG_MARKER_SOF0:
    case GST_JPEG_MARKER_SOF1:
    case GST_JPEG_MARKER_SOF2:
    case GST_JPEG_MARKER_SOF3:
    case GST_JPEG_MARKER_SOF9:
    case GST_JPEG_MARKER_SOF10:
    case GST_JPEG_MARKER_SOF11:{
      GstJpegFrameHdr hdr;
      int i;

      if (!gst_jpeg_segment_parse_frame_header (segment, &hdr)) {
        g_printerr ("Failed to parse frame header!\n");
        return FALSE;
      }

      g_print ("\t\twidth x height   = %u x %u\n", hdr.width, hdr.height);
      g_print ("\t\tsample precision = %u\n", hdr.sample_precision);
      g_print ("\t\tnum components   = %u\n", hdr.num_components);
      for (i = 0; i < hdr.num_components; ++i) {
        g_print ("\t\t%d: id=%d, h=%d, v=%d, qts=%d\n", i,
            hdr.components[i].identifier, hdr.components[i].horizontal_factor,
            hdr.components[i].vertical_factor,
            hdr.components[i].quant_table_selector);
      }
      break;
    }
    case GST_JPEG_MARKER_DHT:{
      GstJpegHuffmanTables ht;

      if (!gst_jpeg_segment_parse_huffman_table (segment, &ht)) {
        g_printerr ("Failed to parse huffman table!\n");
        return FALSE;
      }
      break;
    }
    case GST_JPEG_MARKER_DQT:{
      GstJpegQuantTables qt;

      if (!gst_jpeg_segment_parse_quantization_table (segment, &qt)) {
        g_printerr ("Failed to parse quantization table!\n");
        return FALSE;
      }
      break;
    }
    case GST_JPEG_MARKER_SOS:{
      GstJpegScanHdr hdr;
      int i;

      if (!gst_jpeg_segment_parse_scan_header (segment, &hdr)) {
        g_printerr ("Failed to parse scan header!\n");
        return FALSE;
      }

      g_print ("\t\tnum components   = %u\n", hdr.num_components);
      for (i = 0; i < hdr.num_components; ++i) {
        g_print ("\t\t  %d: cs=%d, dcs=%d, acs=%d\n", i,
            hdr.components[i].component_selector,
            hdr.components[i].dc_selector, hdr.components[i].ac_selector);
      }
    }
    case GST_JPEG_MARKER_COM:
      /* gst_util_dump_mem (segment->data + segment->offset, segment->size); */
      break;
    default:
      if (segment->marker >= GST_JPEG_MARKER_APP_MIN
          && segment->marker <= GST_JPEG_MARKER_APP_MAX) {
        guint n = segment->marker - GST_JPEG_MARKER_APP_MIN;

        if (app_segments[n] == NULL)
          app_segments[n] = gst_buffer_new ();

        gst_buffer_append_memory (app_segments[n],
            gst_memory_new_wrapped (GST_MEMORY_FLAG_READONLY,
                (guint8 *) segment->data + segment->offset,
                segment->size, 0, segment->size, NULL, NULL));
      }
      break;
  }
  return TRUE;
}

static void
parse_jpeg (const guint8 * data, gsize size)
{
  GstJpegSegment segment;
  guint offset = 0;

  while (gst_jpeg_parse (&segment, data, size, offset)) {
    if (segment.offset > offset + 2)
      g_print ("  skipped %u bytes\n", (guint) segment.offset - offset - 2);

    g_print ("%6d bytes at offset %-8u : %s\n", (gint) segment.size,
        segment.offset, get_marker_name (segment.marker));

    if (segment.marker == GST_JPEG_MARKER_EOI)
      break;

    if (offset + segment.size < size &&
        parse_jpeg_segment (&segment) && segment.size >= 0)
      offset = segment.offset + segment.size;
    else
      offset += 2;
  };
}

static void
process_file (const gchar * fn)
{
  GError *err = NULL;
  gchar *data = NULL;
  gsize size = 0;
  guint i;

  g_print ("===============================================================\n");
  g_print (" %s\n", fn);
  g_print ("===============================================================\n");

  if (!g_file_get_contents (fn, &data, &size, &err)) {
    g_error ("%s", err->message);
    g_clear_error (&err);
    return;
  }

  parse_jpeg ((const guint8 *) data, size);

  for (i = 0; i < G_N_ELEMENTS (app_segments); ++i) {
    if (app_segments[i] != NULL) {
      GstMapInfo map = GST_MAP_INFO_INIT;

      /* Could parse/extract tags here */
      gst_buffer_map (app_segments[i], &map, GST_MAP_READ);
      g_print ("\tAPP%-2u : %u bytes\n", i, (guint) map.size);
      gst_util_dump_mem ((guchar *) map.data, MIN (map.size, 16));
      gst_buffer_unmap (app_segments[i], &map);
      gst_buffer_unref (app_segments[i]);
      app_segments[i] = NULL;
    }
  }

  g_free (data);
}

int
main (int argc, gchar ** argv)
{
  gchar **filenames = NULL;
  GOptionEntry options[] = {
    {G_OPTION_REMAINING, 0, 0, G_OPTION_ARG_FILENAME_ARRAY, &filenames, NULL},
    {NULL}
  };
  GOptionContext *ctx;
  GError *err = NULL;
  guint i, num;

  gst_init (&argc, &argv);

  if (argc == 1) {
    g_printerr ("Usage: %s FILE.JPG [FILE2.JPG] [FILE..JPG]\n", argv[0]);
    return -1;
  }

  ctx = g_option_context_new ("JPEG FILES");
  g_option_context_add_main_entries (ctx, options, GETTEXT_PACKAGE);
  g_option_context_add_group (ctx, gst_init_get_option_group ());
  if (!g_option_context_parse (ctx, &argc, &argv, &err)) {
    g_print ("Error initializing: %s\n", GST_STR_NULL (err->message));
    g_option_context_free (ctx);
    g_clear_error (&err);
    exit (1);
  }
  g_option_context_free (ctx);

  if (filenames == NULL || *filenames == NULL) {
    g_printerr ("Please provide one or more filenames.");
    return 1;
  }

  num = g_strv_length (filenames);

  for (i = 0; i < num; ++i) {
    process_file (filenames[i]);
  }

  g_strfreev (filenames);

  return 0;
}
