/* GStreamer mplex (mjpegtools) wrapper
 * (c) 2003 Ronald Bultje <rbultje@ronald.bitfreak.net>
 *
 * gstmplexjob.hh: gstreamer/mplex multiplex-job wrapper
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

#include "gstmplexjob.hh"
#include <gst/gst.h>

enum
{
  ARG_0,
  ARG_FORMAT,
  ARG_MUX_BITRATE,
  ARG_VBR,
  ARG_SYSTEM_HEADERS,
  ARG_SPLIT_SEQUENCE,
  ARG_SEGMENT_SIZE,
  ARG_PACKETS_PER_PACK,
  ARG_SECTOR_SIZE,
  ARG_BUFSIZE
      /* FILL ME */
};

#define DEFAULT_FORMAT MPEG_FORMAT_DVD
/*
 * Property enumeration types.
 */

#define GST_TYPE_MPLEX_FORMAT \
  (gst_mplex_format_get_type ())

static GType
gst_mplex_format_get_type (void)
{
  static GType mplex_format_type = 0;

  if (!mplex_format_type) {
    static const GEnumValue mplex_formats[] = {
      {MPEG_FORMAT_MPEG1, "Generic MPEG-1", "mpeg-1"},
      {MPEG_FORMAT_VCD, "Standard VCD", "vcd"},
      {MPEG_FORMAT_VCD_NSR, "User VCD", "vcd-nsr"},
      {MPEG_FORMAT_MPEG2, "Generic MPEG-2", "mpeg-2"},
      {MPEG_FORMAT_SVCD, "Standard SVCD", "svcd"},
      {MPEG_FORMAT_SVCD_NSR, "User SVCD", "svcd-nsr"},
      {MPEG_FORMAT_VCD_STILL, "VCD Stills sequences", "vcd-still"},
      {MPEG_FORMAT_SVCD_STILL, "SVCD Stills sequences", "svcd-still"},
      {MPEG_FORMAT_DVD_NAV, "DVD MPEG-2 for dvdauthor", "dvd-nav"},
      {MPEG_FORMAT_DVD, "DVD MPEG-2", "dvd"},
      {MPEG_FORMAT_ATSC480i, "ATSC 480i", "atsc-480i"},
      {MPEG_FORMAT_ATSC480p, "ATSC 480p", "atsc-480p"},
      {MPEG_FORMAT_ATSC720p, "ATSC 720p", "atsc-720p"},
      {MPEG_FORMAT_ATSC1080i, "ATSC 1080i", "atsc-1080i"},
      {0, NULL, NULL},
    };

    mplex_format_type =
        g_enum_register_static ("GstMplexFormat", mplex_formats);
  }

  return mplex_format_type;
}

/*
 * Class init functions.
 */

GstMplexJob::GstMplexJob (void):
MultiplexJob ()
{
  /* blabla */
  bufsize = 0;
}

/*
 * GObject properties.
 */

void
GstMplexJob::initProperties (GObjectClass * klass)
{
  /* encoding profile */
  g_object_class_install_property (klass, ARG_FORMAT,
      g_param_spec_enum ("format", "Format", "Encoding profile format",
          GST_TYPE_MPLEX_FORMAT, DEFAULT_FORMAT,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  /* total stream datarate. Normally, this shouldn't be needed, but
   * some DVD/VCD/SVCD players really need strict values to handle
   * the created files correctly. */
  g_object_class_install_property (klass, ARG_MUX_BITRATE,
      g_param_spec_int ("mux-bitrate", "Mux. bitrate",
          "Bitrate of output stream in kbps (0 = autodetect)",
          0, 15 * 1024, 0,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  /* override decode buffer size otherwise determined by format */
  g_object_class_install_property (klass, ARG_BUFSIZE,
      g_param_spec_int ("bufsize", "Decoder buf. size",
          "Target decoders video buffer size (kB) "
          "[default determined by format if not explicitly set]",
          20, 4000, 46,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  /* some boolean stuff for headers */
  g_object_class_install_property (klass, ARG_VBR,
      g_param_spec_boolean ("vbr", "VBR",
          "Whether the input video stream is variable bitrate",
          FALSE,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
  g_object_class_install_property (klass, ARG_SYSTEM_HEADERS,
      g_param_spec_boolean ("system-headers", "System headers",
          "Create system header in every pack for generic formats",
          FALSE,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
#if 0                           /* not supported */
  g_object_class_install_property (klass, ARG_SPLIT_SEQUENCE,
      g_param_spec_boolean ("split-sequence", "Split sequence",
          "Simply split a sequence across files "
          "(rather than building run-out/run-in)",
          FALSE,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  /* size of a segment */
  g_object_class_install_property (klass, ARG_SEGMENT_SIZE,
      g_param_spec_int ("max-segment-size", "Max. segment size",
          "Max. size per segment/file in MB (0 = unlimited)",
          0, 10 * 1024, 0,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
#endif

  /* packets per pack (generic formats) */
  g_object_class_install_property (klass, ARG_PACKETS_PER_PACK,
      g_param_spec_int ("packets-per-pack", "Packets per pack",
          "Number of packets per pack for generic formats",
          1, 100, 1,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  /* size of one sector */
  g_object_class_install_property (klass, ARG_SECTOR_SIZE,
      g_param_spec_int ("sector-size", "Sector size",
          "Specify sector size in bytes for generic formats",
          256, 16384, 2048,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  gst_type_mark_as_plugin_api (GST_TYPE_MPLEX_FORMAT, (GstPluginAPIFlags) 0);
}

/*
 * set/get gobject properties.
 */

void
GstMplexJob::getProperty (guint prop_id, GValue * value)
{
  switch (prop_id) {
    case ARG_FORMAT:
      g_value_set_enum (value, mux_format);
      break;
    case ARG_MUX_BITRATE:
      /* convert from bytes back to bits */
      g_value_set_int (value, (data_rate * 8) / 1000);
      break;
    case ARG_VBR:
      g_value_set_boolean (value, VBR);
      break;
    case ARG_SYSTEM_HEADERS:
      g_value_set_boolean (value, always_system_headers);
      break;
    case ARG_SPLIT_SEQUENCE:
      g_value_set_boolean (value, multifile_segment);
      break;
    case ARG_SEGMENT_SIZE:
      g_value_set_int (value, max_segment_size);
      break;
    case ARG_PACKETS_PER_PACK:
      g_value_set_int (value, packets_per_pack);
      break;
    case ARG_SECTOR_SIZE:
      g_value_set_int (value, sector_size);
      break;
    case ARG_BUFSIZE:
      g_value_set_int (value, bufsize);
      break;
    default:
      break;
  }
}

void
GstMplexJob::setProperty (guint prop_id, const GValue * value)
{
  switch (prop_id) {
    case ARG_FORMAT:
      mux_format = g_value_get_enum (value);
      break;
    case ARG_MUX_BITRATE:
      /* data_rate expects bytes (don't ask me why the property itself is
       * in bits, I'm just staying compatible to mjpegtools options), and
       * rounded up to 50-bytes. */
      data_rate = ((g_value_get_int (value) * 1000 / 8 + 49) / 50) * 50;
      break;
    case ARG_VBR:
      VBR = g_value_get_boolean (value);
      break;
    case ARG_SYSTEM_HEADERS:
      always_system_headers = g_value_get_boolean (value);
      break;
    case ARG_SPLIT_SEQUENCE:
      multifile_segment = g_value_get_boolean (value);
      break;
    case ARG_SEGMENT_SIZE:
      max_segment_size = g_value_get_int (value);
      break;
    case ARG_PACKETS_PER_PACK:
      packets_per_pack = g_value_get_int (value);
      break;
    case ARG_SECTOR_SIZE:
      sector_size = g_value_get_int (value);
      break;
    case ARG_BUFSIZE:
      bufsize = g_value_get_int (value);
      break;
    default:
      break;
  }
}
