/* GStreamer Ogg Granulepos Mapping Utility Functions
 * Copyright (C) 2006 Tim-Philipp Müller <tim centricular net>
 * Copyright (C) 2009 David Schleef <ds@schleef.org>
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

#include "gstoggstream.h"
#include "dirac_parse.h"
#include "vorbis_parse.h"

#include <gst/riff/riff-media.h>
#include <gst/pbutils/pbutils.h>

#include <stdlib.h>
#include <string.h>

GST_DEBUG_CATEGORY_EXTERN (gst_ogg_demux_debug);
GST_DEBUG_CATEGORY_EXTERN (gst_ogg_demux_setup_debug);
#define GST_CAT_DEFAULT gst_ogg_demux_debug

typedef struct _GstOggMap GstOggMap;

typedef gboolean (*GstOggMapSetupFunc) (GstOggStream * pad,
    ogg_packet * packet);
typedef gboolean (*GstOggMapSetupFromCapsFunc) (GstOggStream * pad,
    const GstCaps * caps);
typedef GstClockTime (*GstOggMapToTimeFunc) (GstOggStream * pad,
    gint64 granulepos);
typedef gint64 (*GstOggMapToGranuleFunc) (GstOggStream * pad,
    gint64 granulepos);
typedef gint64 (*GstOggMapToGranuleposFunc) (GstOggStream * pad,
    gint64 granule, gint64 keyframe_granule);

/* returns TRUE if the granulepos denotes a key frame */
typedef gboolean (*GstOggMapIsGranuleposKeyFrameFunc) (GstOggStream * pad,
    gint64 granulepos);

/* returns TRUE if the packet is a key frame */
typedef gboolean (*GstOggMapIsPacketKeyFrameFunc) (GstOggStream * pad,
    ogg_packet * packet);

/* returns TRUE if the given packet is a stream header packet */
typedef gboolean (*GstOggMapIsHeaderPacketFunc) (GstOggStream * pad,
    ogg_packet * packet);
typedef gint64 (*GstOggMapPacketDurationFunc) (GstOggStream * pad,
    ogg_packet * packet);
typedef void (*GstOggMapExtractTagsFunc) (GstOggStream * pad,
    ogg_packet * packet);

typedef gint64 (*GstOggMapGranuleposToKeyGranuleFunc) (GstOggStream * pad,
    gint64 granulepos);

typedef GstBuffer *(*GstOggMapGetHeadersFunc) (GstOggStream * pad);
typedef void (*GstOggMapUpdateStatsFunc) (GstOggStream * pad,
    ogg_packet * packet);

#define SKELETON_FISBONE_MIN_SIZE  52
#define SKELETON_FISHEAD_3_3_MIN_SIZE 112
#define SKELETON_FISHEAD_4_0_MIN_SIZE 80

struct _GstOggMap
{
  const gchar *id;
  int id_length;
  int min_packet_size;
  const gchar *media_type;
  GstOggMapSetupFunc setup_func;
  GstOggMapSetupFromCapsFunc setup_from_caps_func;
  GstOggMapToGranuleFunc granulepos_to_granule_func;
  GstOggMapToGranuleposFunc granule_to_granulepos_func;
  GstOggMapIsGranuleposKeyFrameFunc is_granulepos_key_frame_func;
  GstOggMapIsPacketKeyFrameFunc is_packet_key_frame_func;
  GstOggMapIsHeaderPacketFunc is_header_func;
  GstOggMapPacketDurationFunc packet_duration_func;
  GstOggMapGranuleposToKeyGranuleFunc granulepos_to_key_granule_func;
  GstOggMapExtractTagsFunc extract_tags_func;
  GstOggMapGetHeadersFunc get_headers_func;
  GstOggMapUpdateStatsFunc update_stats_func;
};

extern const GstOggMap mappers[];

GstClockTime
gst_ogg_stream_get_packet_start_time (GstOggStream * pad, ogg_packet * packet)
{
  int duration;

  if (packet->granulepos == -1) {
    return GST_CLOCK_TIME_NONE;
  }

  duration = gst_ogg_stream_get_packet_duration (pad, packet);
  if (duration == -1) {
    return GST_CLOCK_TIME_NONE;
  }

  return gst_ogg_stream_granule_to_time (pad,
      gst_ogg_stream_granulepos_to_granule (pad,
          packet->granulepos) - duration);
}

GstClockTime
gst_ogg_stream_get_start_time_for_granulepos (GstOggStream * pad,
    gint64 granulepos)
{
  if (pad->frame_size == 0)
    return GST_CLOCK_TIME_NONE;

  return gst_ogg_stream_granule_to_time (pad,
      gst_ogg_stream_granulepos_to_granule (pad, granulepos));
}

GstClockTime
gst_ogg_stream_get_end_time_for_granulepos (GstOggStream * pad,
    gint64 granulepos)
{
  return gst_ogg_stream_granule_to_time (pad,
      gst_ogg_stream_granulepos_to_granule (pad, granulepos));
}

GstClockTime
gst_ogg_stream_granule_to_time (GstOggStream * pad, gint64 granule)
{
  if (granule == 0 || pad->granulerate_n == 0 || pad->granulerate_d == 0)
    return 0;

  granule += pad->granule_offset;
  if (granule < 0)
    return 0;

  return gst_util_uint64_scale (granule, GST_SECOND * pad->granulerate_d,
      pad->granulerate_n);
}

gint64
gst_ogg_stream_granulepos_to_granule (GstOggStream * pad, gint64 granulepos)
{
  if (granulepos == -1 || granulepos == 0) {
    return granulepos;
  }

  if (mappers[pad->map].granulepos_to_granule_func == NULL) {
    GST_WARNING ("Failed to convert %s granulepos to granule",
        gst_ogg_stream_get_media_type (pad));
    return -1;
  }

  return mappers[pad->map].granulepos_to_granule_func (pad, granulepos);
}

gint64
gst_ogg_stream_granulepos_to_key_granule (GstOggStream * pad, gint64 granulepos)
{
  if (mappers[pad->map].granulepos_to_key_granule_func)
    return mappers[pad->map].granulepos_to_key_granule_func (pad, granulepos);

  if (granulepos == -1 || granulepos == 0 || pad->granuleshift == G_MAXUINT32) {
    return granulepos;
  }

  return granulepos >> pad->granuleshift;
}

gint64
gst_ogg_stream_granule_to_granulepos (GstOggStream * pad, gint64 granule,
    gint64 keyframe_granule)
{
  if (granule == -1 || granule == 0) {
    return granule;
  }

  if (mappers[pad->map].granule_to_granulepos_func == NULL) {
    GST_WARNING ("Failed to convert %s granule to granulepos",
        gst_ogg_stream_get_media_type (pad));
    return -1;
  }

  return mappers[pad->map].granule_to_granulepos_func (pad, granule,
      keyframe_granule);
}

gboolean
gst_ogg_stream_granulepos_is_key_frame (GstOggStream * pad, gint64 granulepos)
{
  if (granulepos == -1) {
    return FALSE;
  }

  if (mappers[pad->map].is_granulepos_key_frame_func == NULL) {
    GST_WARNING ("Failed to determine keyframeness for %s granulepos",
        gst_ogg_stream_get_media_type (pad));
    return FALSE;
  }

  return mappers[pad->map].is_granulepos_key_frame_func (pad, granulepos);
}

gboolean
gst_ogg_stream_packet_is_key_frame (GstOggStream * pad, ogg_packet * packet)
{
  if (mappers[pad->map].is_packet_key_frame_func == NULL) {
    GST_WARNING ("Failed to determine keyframeness of %s packet",
        gst_ogg_stream_get_media_type (pad));
    return FALSE;
  }

  return mappers[pad->map].is_packet_key_frame_func (pad, packet);
}

gboolean
gst_ogg_stream_packet_is_header (GstOggStream * pad, ogg_packet * packet)
{
  if (mappers[pad->map].is_header_func == NULL) {
    GST_WARNING ("Failed to determine headerness of %s packet",
        gst_ogg_stream_get_media_type (pad));
    return FALSE;
  }

  return mappers[pad->map].is_header_func (pad, packet);
}

gint64
gst_ogg_stream_get_packet_duration (GstOggStream * pad, ogg_packet * packet)
{
  if (mappers[pad->map].packet_duration_func == NULL) {
    GST_WARNING ("Failed to determine %s packet duration",
        gst_ogg_stream_get_media_type (pad));
    return -1;
  }

  return mappers[pad->map].packet_duration_func (pad, packet);
}


void
gst_ogg_stream_extract_tags (GstOggStream * pad, ogg_packet * packet)
{
  if (mappers[pad->map].extract_tags_func == NULL) {
    GST_DEBUG ("No tag extraction");
    return;
  }

  mappers[pad->map].extract_tags_func (pad, packet);
}

const char *
gst_ogg_stream_get_media_type (GstOggStream * pad)
{
  const GstCaps *caps = pad->caps;
  const GstStructure *structure;
  if (!caps)
    return NULL;
  structure = gst_caps_get_structure (caps, 0);
  if (!structure)
    return NULL;
  return gst_structure_get_name (structure);
}

GstBuffer *
gst_ogg_stream_get_headers (GstOggStream * pad)
{
  if (!mappers[pad->map].get_headers_func)
    return NULL;

  return mappers[pad->map].get_headers_func (pad);
}

void
gst_ogg_stream_update_stats (GstOggStream * pad, ogg_packet * packet)
{
  if (!mappers[pad->map].get_headers_func)
    return;

  mappers[pad->map].update_stats_func (pad, packet);
}

/* some generic functions */

static gboolean
is_granulepos_keyframe_true (GstOggStream * pad, gint64 granulepos)
{
  return TRUE;
}

static gboolean
is_packet_keyframe_true (GstOggStream * pad, ogg_packet * packet)
{
  return TRUE;
}

static gint64
granulepos_to_granule_default (GstOggStream * pad, gint64 granulepos)
{
  gint64 keyindex, keyoffset;

  if (pad->granuleshift != 0 && pad->granuleshift != G_MAXUINT32) {
    keyindex = granulepos >> pad->granuleshift;
    keyoffset = granulepos - (keyindex << pad->granuleshift);
    return keyindex + keyoffset;
  } else {
    return granulepos;
  }
}


static gint64
granule_to_granulepos_default (GstOggStream * pad, gint64 granule,
    gint64 keyframe_granule)
{
  gint64 keyoffset;

  if (pad->granuleshift != 0 && pad->granuleshift != G_MAXUINT32) {
    /* If we don't know where the previous keyframe is yet, assume it is
       at 0 or 1, depending on bitstream version. If nothing else, this
       avoids getting negative granpos back. */
    if (keyframe_granule < 0)
      keyframe_granule = pad->theora_has_zero_keyoffset ? 0 : 1;
    keyoffset = granule - keyframe_granule;
    return (keyframe_granule << pad->granuleshift) | keyoffset;
  } else {
    return granule;
  }
}

#ifdef unused
static gboolean
is_header_unknown (GstOggStream * pad, ogg_packet * packet)
{
  GST_WARNING ("don't know how to detect header");
  return FALSE;
}
#endif

static gboolean
is_header_true (GstOggStream * pad, ogg_packet * packet)
{
  return TRUE;
}

static gboolean
is_header_count (GstOggStream * pad, ogg_packet * packet)
{
  if (pad->n_header_packets_seen < pad->n_header_packets) {
    return TRUE;
  }
  return FALSE;
}

static gint64
packet_duration_constant (GstOggStream * pad, ogg_packet * packet)
{
  return pad->frame_size;
}

/* helper: extracts tags from vorbis comment ogg packet.
 * Returns result in *tags after free'ing existing *tags (if any) */
static gboolean
tag_list_from_vorbiscomment_packet (ogg_packet * packet,
    const guint8 * id_data, const guint id_data_length, GstTagList ** tags)
{
  gchar *encoder = NULL;
  GstTagList *list;
  gboolean ret = TRUE;

  g_return_val_if_fail (tags != NULL, FALSE);

  list = gst_tag_list_from_vorbiscomment (packet->packet, packet->bytes,
      id_data, id_data_length, &encoder);

  if (!list) {
    GST_WARNING ("failed to decode vorbis comments");
    ret = FALSE;
    goto exit;
  }

  if (encoder) {
    if (encoder[0] && g_utf8_validate (encoder, -1, NULL))
      gst_tag_list_add (list, GST_TAG_MERGE_REPLACE, GST_TAG_ENCODER, encoder,
          NULL);
    g_free (encoder);
  }

exit:
  if (*tags)
    gst_tag_list_unref (*tags);
  *tags = list;

  return ret;
}

/* theora */

static gboolean
setup_theora_mapper (GstOggStream * pad, ogg_packet * packet)
{
  guint8 *data = packet->packet;
  guint w, h, par_d, par_n;
  guint8 vmaj, vmin, vrev;

  vmaj = data[7];
  vmin = data[8];
  vrev = data[9];

  w = GST_READ_UINT24_BE (data + 14) & 0xFFFFFF;
  h = GST_READ_UINT24_BE (data + 17) & 0xFFFFFF;

  pad->granulerate_n = GST_READ_UINT32_BE (data + 22);
  pad->granulerate_d = GST_READ_UINT32_BE (data + 26);
  if (pad->granulerate_n == 0 || pad->granulerate_d == 0) {
    GST_WARNING ("Invalid frame rate %d/%d", pad->granulerate_n,
        pad->granulerate_d);
    pad->granulerate_n = 0;
    pad->granulerate_d = 0;
    return FALSE;
  }

  par_n = GST_READ_UINT24_BE (data + 30);
  par_d = GST_READ_UINT24_BE (data + 33);

  GST_LOG ("fps = %d/%d, PAR = %u/%u, width = %u, height = %u",
      pad->granulerate_n, pad->granulerate_d, par_n, par_d, w, h);

  /* 2 bits + 3 bits = 5 bits KFGSHIFT */
  pad->granuleshift = ((GST_READ_UINT8 (data + 40) & 0x03) << 3) +
      (GST_READ_UINT8 (data + 41) >> 5);
  if (pad->granuleshift >= 63) {
    /* Granuleshift can't be greater than the storage size of a granule */
    GST_WARNING ("Invalid granuleshift (%u >= 63)", pad->granuleshift);
    pad->granulerate_n = 0;
    pad->granulerate_d = 0;
    pad->granuleshift = -1;
    return FALSE;
  }
  GST_LOG ("granshift: %d", pad->granuleshift);

  pad->is_video = TRUE;
  pad->n_header_packets = 3;
  pad->frame_size = 1;

  pad->bitrate = GST_READ_UINT24_BE (data + 37);
  GST_LOG ("bit rate: %d", pad->bitrate);

  /* The interpretation of the granule position has changed with 3.2.1.
     The granule is now made from the number of frames encoded, rather than
     the index of the frame being encoded - so there is a difference of 1. */
  pad->theora_has_zero_keyoffset =
      ((vmaj << 16) | (vmin << 8) | vrev) < 0x030201;

  pad->caps = gst_caps_new_empty_simple ("video/x-theora");

  if (w > 0 && h > 0) {
    gst_caps_set_simple (pad->caps, "width", G_TYPE_INT, w, "height",
        G_TYPE_INT, h, NULL);
  }

  /* PAR of 0:N, N:0 and 0:0 is allowed and maps to 1:1 */
  if (par_n == 0 || par_d == 0)
    par_n = par_d = 1;

  /* only add framerate now so caps look prettier, with width/height first */
  gst_caps_set_simple (pad->caps, "framerate", GST_TYPE_FRACTION,
      pad->granulerate_n, pad->granulerate_d, "pixel-aspect-ratio",
      GST_TYPE_FRACTION, par_n, par_d, NULL);

  return TRUE;
}

static gint64
granulepos_to_granule_theora (GstOggStream * pad, gint64 granulepos)
{
  gint64 keyindex, keyoffset;

  if (pad->granuleshift != 0 && pad->granuleshift != G_MAXUINT32) {
    keyindex = granulepos >> pad->granuleshift;
    keyoffset = granulepos - (keyindex << pad->granuleshift);
    if (pad->theora_has_zero_keyoffset) {
      keyoffset++;
    }
    return keyindex + keyoffset;
  } else {
    return granulepos;
  }
}

static gboolean
is_granulepos_keyframe_theora (GstOggStream * pad, gint64 granulepos)
{
  gint64 frame_mask;

  if (granulepos == (gint64) - 1 || pad->granuleshift == G_MAXUINT32)
    return FALSE;

  frame_mask = (G_GUINT64_CONSTANT (1) << pad->granuleshift) - 1;

  return ((granulepos & frame_mask) == 0);
}

static gboolean
is_packet_keyframe_theora (GstOggStream * pad, ogg_packet * packet)
{
  if (packet->bytes == 0)
    return FALSE;
  return (packet->packet[0] & 0xc0) == 0x00;
}

static gboolean
is_header_theora (GstOggStream * pad, ogg_packet * packet)
{
  return (packet->bytes > 0 && (packet->packet[0] & 0x80) == 0x80);
}

static void
extract_tags_theora (GstOggStream * pad, ogg_packet * packet)
{
  if (packet->bytes > 0 && packet->packet[0] == 0x81) {
    tag_list_from_vorbiscomment_packet (packet,
        (const guint8 *) "\201theora", 7, &pad->taglist);

    if (!pad->taglist)
      pad->taglist = gst_tag_list_new_empty ();

    gst_tag_list_add (pad->taglist, GST_TAG_MERGE_REPLACE,
        GST_TAG_VIDEO_CODEC, "Theora", NULL);

    if (pad->bitrate)
      gst_tag_list_add (pad->taglist, GST_TAG_MERGE_REPLACE,
          GST_TAG_BITRATE, (guint) pad->bitrate, NULL);
  }
}

/* dirac */

static gboolean
setup_dirac_mapper (GstOggStream * pad, ogg_packet * packet)
{
  int ret;
  DiracSequenceHeader header;

  ret = gst_dirac_sequence_header_parse (&header, packet->packet + 13,
      packet->bytes - 13);
  if (ret == 0) {
    GST_DEBUG ("Failed to parse Dirac sequence header");
    return FALSE;
  }

  if (header.interlaced_coding != 0) {
    GST_DEBUG ("non-progressive Dirac coding not implemented");
    return FALSE;
  }

  if (header.frame_rate_numerator == 0 || header.frame_rate_denominator == 0) {
    GST_WARNING ("invalid framerate %d/%d", header.frame_rate_numerator,
        header.frame_rate_denominator);
    return FALSE;
  }

  pad->is_video = TRUE;
  pad->always_flush_page = TRUE;
  pad->granulerate_n = header.frame_rate_numerator * 2;
  pad->granulerate_d = header.frame_rate_denominator;
  pad->granuleshift = 22;
  pad->n_header_packets = 1;
  pad->frame_size = 2;

  pad->caps = gst_caps_new_simple ("video/x-dirac",
      "width", G_TYPE_INT, header.width,
      "height", G_TYPE_INT, header.height,
      "interlace-mode", G_TYPE_STRING,
      (header.interlaced ? "mixed" : "progressive"), "pixel-aspect-ratio",
      GST_TYPE_FRACTION, header.aspect_ratio_numerator,
      header.aspect_ratio_denominator, "framerate", GST_TYPE_FRACTION,
      header.frame_rate_numerator, header.frame_rate_denominator, NULL);

  return TRUE;
}

#define OGG_DIRAC_GRANULE_LOW_MASK ((1<<22) - 1)
static gboolean
is_keyframe_dirac (GstOggStream * pad, gint64 granulepos)
{
  int dist_h;
  int dist_l;
  int dist;

  if (granulepos == -1)
    return -1;

  dist_h = (granulepos >> 22) & 0xff;
  dist_l = granulepos & 0xff;
  dist = (dist_h << 8) | dist_l;

  return (dist == 0);
}

static gint64
granulepos_to_granule_dirac (GstOggStream * pad, gint64 gp)
{
  gint64 pt;
  int delay;
  gint64 dt;

  pt = ((gp >> 22) + (gp & OGG_DIRAC_GRANULE_LOW_MASK)) >> 9;
  delay = (gp >> 9) & 0x1fff;
  dt = pt - delay;

  GST_DEBUG ("pt %" G_GINT64_FORMAT " delay %d", pt, delay);

  return dt + 4;
}

static gint64
granule_to_granulepos_dirac (GstOggStream * pad, gint64 granule,
    gint64 keyframe_granule)
{
  /* This conversion requires knowing more details about the Dirac
   * stream. */
  return -1;
}

static gint64
granulepos_to_key_granule_dirac (GstOggStream * pad, gint64 gp)
{
  gint64 pt;
  int dist_h;
  int dist_l;
  int dist;
  int delay;
  gint64 dt;

  if (gp == -1 || gp == 0)
    return gp;

  pt = ((gp >> 22) + (gp & OGG_DIRAC_GRANULE_LOW_MASK)) >> 9;
  dist_h = (gp >> 22) & 0xff;
  dist_l = gp & 0xff;
  dist = (dist_h << 8) | dist_l;
  delay = (gp >> 9) & 0x1fff;
  dt = pt - delay;

  return dt - 2 * dist + 4;
}

/* VP8 */

static gboolean
setup_vp8_mapper (GstOggStream * pad, ogg_packet * packet)
{
  gint width, height, par_n, par_d, fps_n, fps_d;

  if (packet->bytes < 26) {
    GST_DEBUG ("Failed to parse VP8 BOS page");
    return FALSE;
  }

  width = GST_READ_UINT16_BE (packet->packet + 8);
  height = GST_READ_UINT16_BE (packet->packet + 10);
  par_n = GST_READ_UINT24_BE (packet->packet + 12);
  par_d = GST_READ_UINT24_BE (packet->packet + 15);
  fps_n = GST_READ_UINT32_BE (packet->packet + 18);
  fps_d = GST_READ_UINT32_BE (packet->packet + 22);

  if (fps_n == 0 || fps_d == 0) {
    GST_WARNING ("invalid framerate %d/%d", fps_n, fps_d);
    return FALSE;
  }

  pad->is_video = TRUE;
  pad->is_vp8 = TRUE;
  pad->granulerate_n = fps_n;
  pad->granulerate_d = fps_d;
  pad->n_header_packets = 2;
  pad->frame_size = 1;

  /* PAR of 0:N, N:0 and 0:0 is not explicitely allowed, but the best we can do
   * here is to map to 1:1 so that caps negotiation doesn't break downstream. */
  if (par_n == 0 || par_d == 0)
    par_n = par_d = 1;

  pad->caps = gst_caps_new_simple ("video/x-vp8",
      "width", G_TYPE_INT, width,
      "height", G_TYPE_INT, height,
      "pixel-aspect-ratio", GST_TYPE_FRACTION,
      par_n, par_d, "framerate", GST_TYPE_FRACTION, fps_n, fps_d, NULL);

  return TRUE;
}

static gboolean
vp8_fill_header (GstOggStream * pad, const GstCaps * caps, guint8 * data)
{
  gint width, height, par_n, par_d, fps_n, fps_d;
  GstStructure *structure = gst_caps_get_structure (caps, 0);

  if (!(gst_structure_get_int (structure, "width", &width) &&
          gst_structure_get_int (structure, "height", &height) &&
          gst_structure_get_fraction (structure, "framerate", &fps_n,
              &fps_d))) {
    GST_DEBUG ("Failed to get width, height or framerate from caps %"
        GST_PTR_FORMAT, caps);
    return FALSE;
  }
  if (!gst_structure_get_fraction (structure, "pixel-aspect-ratio", &par_n,
          &par_d)) {
    par_n = par_d = 1;
  }

  memcpy (data, "OVP80\1\1", 8);
  GST_WRITE_UINT16_BE (data + 8, width);
  GST_WRITE_UINT16_BE (data + 10, height);
  GST_WRITE_UINT24_BE (data + 12, par_n);
  GST_WRITE_UINT24_BE (data + 15, par_d);
  GST_WRITE_UINT32_BE (data + 18, fps_n);
  GST_WRITE_UINT32_BE (data + 22, fps_d);

  return TRUE;
}

static gboolean
setup_vp8_mapper_from_caps (GstOggStream * pad, const GstCaps * caps)
{
  guint8 data[26];
  ogg_packet packet;

  if (!vp8_fill_header (pad, caps, data))
    return FALSE;

  packet.packet = data;
  packet.bytes = 26;
  return setup_vp8_mapper (pad, &packet);
}

static gboolean
is_keyframe_vp8 (GstOggStream * pad, gint64 granulepos)
{
  guint64 gpos = granulepos;

  if (granulepos == -1)
    return FALSE;

  /* Get rid of flags */
  gpos >>= 3;

  return ((gpos & 0x07ffffff) == 0);
}

static gboolean
is_packet_keyframe_vp8 (GstOggStream * pad, ogg_packet * packet)
{
  guint32 hdr;
  gboolean is_kf = FALSE;

  if (packet->bytes < 3) {
    return FALSE;
  }

  hdr = GST_READ_UINT24_LE (packet->packet);

  is_kf = (hdr & 0x1);
  return is_kf;
}

static gint64
granulepos_to_granule_vp8 (GstOggStream * pad, gint64 gpos)
{
  guint64 gp = (guint64) gpos;
  guint32 pt;
  guint32 dist;

  pt = (gp >> 32);
  dist = (gp >> 3) & 0x07ffffff;

  GST_DEBUG ("pt %u, dist %u", pt, dist);

  return pt;
}

static gint64
granule_to_granulepos_vp8 (GstOggStream * pad, gint64 granule,
    gint64 keyframe_granule)
{
  guint inv;
  gint64 granulepos;

  inv = (pad->invisible_count <= 0) ? 0x3 : pad->invisible_count - 1;

  granulepos =
      (granule << 32) | (inv << 30) | ((granule - keyframe_granule) << 3);
  return granulepos;
}

/* Check if this packet contains an invisible frame or not */
static gint64
packet_duration_vp8 (GstOggStream * pad, ogg_packet * packet)
{
  guint32 hdr;

  if (packet->bytes < 3)
    return 0;

  hdr = GST_READ_UINT24_LE (packet->packet);

  return (((hdr >> 4) & 1) != 0) ? 1 : 0;
}

static gint64
granulepos_to_key_granule_vp8 (GstOggStream * pad, gint64 granulepos)
{
  guint64 gp = granulepos;
  guint64 pts = (gp >> 32);
  guint32 dist = (gp >> 3) & 0x07ffffff;

  if (granulepos == -1 || granulepos == 0)
    return granulepos;

  if (dist > pts)
    return 0;

  return pts - dist;
}

static gboolean
is_header_vp8 (GstOggStream * pad, ogg_packet * packet)
{
  if (packet->bytes >= 5 && packet->packet[0] == 0x4F &&
      packet->packet[1] == 0x56 && packet->packet[2] == 0x50 &&
      packet->packet[3] == 0x38 && packet->packet[4] == 0x30)
    return TRUE;
  return FALSE;
}

static void
extract_tags_vp8 (GstOggStream * pad, ogg_packet * packet)
{
  if (packet->bytes >= 7 && memcmp (packet->packet, "OVP80\2 ", 7) == 0) {
    tag_list_from_vorbiscomment_packet (packet,
        (const guint8 *) "OVP80\2 ", 7, &pad->taglist);

    if (!pad->taglist)
      pad->taglist = gst_tag_list_new_empty ();

    gst_tag_list_add (pad->taglist, GST_TAG_MERGE_REPLACE,
        GST_TAG_VIDEO_CODEC, "VP8", NULL);
  }
}

static GstBuffer *
get_headers_vp8 (GstOggStream * pad)
{
  guint8 *data = g_malloc (26);

  if (vp8_fill_header (pad, pad->caps, data)) {
    return gst_buffer_new_wrapped (data, 26);
  }
  g_free (data);
  return NULL;
}

static void
update_stats_vp8 (GstOggStream * pad, ogg_packet * packet)
{
  if (packet_duration_vp8 (pad, packet)) {
    /* set to -1 as when we get thefirst invisible it should be
     * set to 0 */
    pad->invisible_count = -1;
  } else {
    pad->invisible_count++;
  }
}

/* vorbis */

static gboolean
setup_vorbis_mapper (GstOggStream * pad, ogg_packet * packet)
{
  guint8 *data = packet->packet;
  guint chans;

  data += 1 + 6;
  pad->version = GST_READ_UINT32_LE (data);
  data += 4;
  chans = GST_READ_UINT8 (data);
  data += 1;

  pad->granulerate_n = GST_READ_UINT32_LE (data);
  pad->granulerate_d = 1;
  if (pad->granulerate_n == 0) {
    pad->granulerate_n = 0;
    pad->granulerate_d = 0;
    return FALSE;
  }

  pad->granuleshift = 0;
  pad->preroll = 2;
  pad->last_size = 0;
  GST_LOG ("sample rate: %d", pad->granulerate_n);

  data += 4;
  pad->bitrate_upper = GST_READ_UINT32_LE (data);
  data += 4;
  pad->bitrate_nominal = GST_READ_UINT32_LE (data);
  data += 4;
  pad->bitrate_lower = GST_READ_UINT32_LE (data);

  if (pad->bitrate_nominal > 0)
    pad->bitrate = pad->bitrate_nominal;

  if (pad->bitrate_upper > 0 && !pad->bitrate)
    pad->bitrate = pad->bitrate_upper;

  if (pad->bitrate_lower > 0 && !pad->bitrate)
    pad->bitrate = pad->bitrate_lower;

  GST_LOG ("bit rate: %d", pad->bitrate);

  pad->n_header_packets = 3;

  gst_parse_vorbis_header_packet (pad, packet);

  pad->caps = gst_caps_new_simple ("audio/x-vorbis",
      "rate", G_TYPE_INT, pad->granulerate_n, "channels", G_TYPE_INT, chans,
      NULL);

  return TRUE;
}

static gboolean
is_header_vorbis (GstOggStream * pad, ogg_packet * packet)
{
  int res = 0;

  if (packet->bytes == 0 || (packet->packet[0] & 0x01) == 0)
    return FALSE;

  if (packet->packet[0] == 5) {
    res = gst_parse_vorbis_setup_packet (pad, packet);
  }

  return res == 0;
}

static void
extract_tags_vorbis (GstOggStream * pad, ogg_packet * packet)
{
  if (packet->bytes == 0 || (packet->packet[0] & 0x01) == 0)
    return;

  if (((guint8 *) (packet->packet))[0] == 0x03) {
    tag_list_from_vorbiscomment_packet (packet,
        (const guint8 *) "\003vorbis", 7, &pad->taglist);

    if (!pad->taglist)
      pad->taglist = gst_tag_list_new_empty ();

    gst_tag_list_add (pad->taglist, GST_TAG_MERGE_REPLACE,
        GST_TAG_ENCODER_VERSION, pad->version,
        GST_TAG_AUDIO_CODEC, "Vorbis", NULL);

    if (pad->bitrate_nominal > 0)
      gst_tag_list_add (pad->taglist, GST_TAG_MERGE_REPLACE,
          GST_TAG_NOMINAL_BITRATE, (guint) pad->bitrate_nominal, NULL);

    if (pad->bitrate_upper > 0)
      gst_tag_list_add (pad->taglist, GST_TAG_MERGE_REPLACE,
          GST_TAG_MAXIMUM_BITRATE, (guint) pad->bitrate_upper, NULL);

    if (pad->bitrate_lower > 0)
      gst_tag_list_add (pad->taglist, GST_TAG_MERGE_REPLACE,
          GST_TAG_MINIMUM_BITRATE, (guint) pad->bitrate_lower, NULL);

    if (pad->bitrate)
      gst_tag_list_add (pad->taglist, GST_TAG_MERGE_REPLACE,
          GST_TAG_BITRATE, (guint) pad->bitrate, NULL);
  }
}

static gint64
packet_duration_vorbis (GstOggStream * pad, ogg_packet * packet)
{
  int mode;
  int size;
  int duration;

  if (packet->bytes == 0 || packet->packet[0] & 1)
    return 0;

  mode = (packet->packet[0] >> 1) & ((1 << pad->vorbis_log2_num_modes) - 1);
  size = pad->vorbis_mode_sizes[mode] ? pad->long_size : pad->short_size;

  if (pad->last_size == 0) {
    duration = 0;
  } else {
    duration = pad->last_size / 4 + size / 4;
  }
  pad->last_size = size;

  GST_DEBUG ("duration %d", (int) duration);

  return duration;
}

/* speex */


static gboolean
setup_speex_mapper (GstOggStream * pad, ogg_packet * packet)
{
  guint8 *data = packet->packet;
  guint chans;

  data += 8 + 20 + 4 + 4;

  pad->granulerate_n = GST_READ_UINT32_LE (data);
  pad->granulerate_d = 1;
  if (pad->granulerate_n == 0) {
    pad->granulerate_n = 0;
    pad->granulerate_d = 0;
    return FALSE;
  }

  pad->granuleshift = 0;

  data += 4 + 4 + 4;
  chans = GST_READ_UINT32_LE (data);
  data += 4;
  pad->bitrate = GST_READ_UINT32_LE (data);

  GST_LOG ("sample rate: %d, channels: %u", pad->granulerate_n, chans);
  GST_LOG ("bit rate: %d", pad->bitrate);

  pad->n_header_packets = GST_READ_UINT32_LE (packet->packet + 68) + 2;
  pad->frame_size = GST_READ_UINT32_LE (packet->packet + 64) *
      GST_READ_UINT32_LE (packet->packet + 56);

  pad->caps = gst_caps_new_simple ("audio/x-speex", "rate", G_TYPE_INT,
      pad->granulerate_n, "channels", G_TYPE_INT, chans, NULL);

  return TRUE;
}

static void
extract_tags_count (GstOggStream * pad, ogg_packet * packet)
{
  /* packet 2 must be comment packet */
  if (packet->bytes > 0 && pad->n_header_packets_seen == 1) {
    tag_list_from_vorbiscomment_packet (packet, NULL, 0, &pad->taglist);

    if (!pad->taglist)
      pad->taglist = gst_tag_list_new_empty ();

    if (pad->is_video) {
      gst_pb_utils_add_codec_description_to_tag_list (pad->taglist,
          GST_TAG_VIDEO_CODEC, pad->caps);
    } else if (!pad->is_sparse && !pad->is_ogm_text && !pad->is_ogm) {
      gst_pb_utils_add_codec_description_to_tag_list (pad->taglist,
          GST_TAG_AUDIO_CODEC, pad->caps);
    } else {
      GST_FIXME ("not adding codec tag, not sure about codec type");
    }

    if (pad->bitrate)
      gst_tag_list_add (pad->taglist, GST_TAG_MERGE_REPLACE,
          GST_TAG_BITRATE, (guint) pad->bitrate, NULL);
  }
}


/* flac */

static gboolean
setup_fLaC_mapper (GstOggStream * pad, ogg_packet * packet)
{
  pad->granulerate_n = 0;
  pad->granulerate_d = 1;
  pad->granuleshift = 0;

  pad->n_header_packets = 3;

  pad->caps = gst_caps_new_empty_simple ("audio/x-flac");

  return TRUE;
}

static gboolean
is_header_fLaC (GstOggStream * pad, ogg_packet * packet)
{
  if (pad->n_header_packets_seen == 1) {
    if (packet->bytes < 17)
      return FALSE;

    pad->granulerate_n = (packet->packet[14] << 12) |
        (packet->packet[15] << 4) | ((packet->packet[16] >> 4) & 0xf);
  }

  if (pad->n_header_packets_seen < pad->n_header_packets) {
    return TRUE;
  }

  return FALSE;
}

static gboolean
setup_flac_mapper (GstOggStream * pad, ogg_packet * packet)
{
  guint8 *data = packet->packet;
  guint chans;

  /* see http://flac.sourceforge.net/ogg_mapping.html */

  pad->granulerate_n = (GST_READ_UINT32_BE (data + 27) & 0xFFFFF000) >> 12;
  pad->granulerate_d = 1;

  if (pad->granulerate_n == 0) {
    pad->granulerate_n = 0;
    pad->granulerate_d = 0;
    return FALSE;
  }

  pad->granuleshift = 0;
  chans = ((GST_READ_UINT32_BE (data + 27) & 0x00000E00) >> 9) + 1;

  GST_DEBUG ("sample rate: %d, channels: %u", pad->granulerate_n, chans);

  pad->n_header_packets = GST_READ_UINT16_BE (packet->packet + 7);

  pad->caps = gst_caps_new_simple ("audio/x-flac", "rate", G_TYPE_INT,
      pad->granulerate_n, "channels", G_TYPE_INT, chans, NULL);

  return TRUE;
}

static gboolean
is_header_flac (GstOggStream * pad, ogg_packet * packet)
{
  return (packet->bytes > 0 && (packet->packet[0] != 0xff));
}

static gint64
packet_duration_flac (GstOggStream * pad, ogg_packet * packet)
{
  int block_size_index;

  if (packet->bytes < 4)
    return -1;

  block_size_index = packet->packet[2] >> 4;
  if (block_size_index == 1)
    return 192;
  if (block_size_index >= 2 && block_size_index <= 5) {
    return 576 << (block_size_index - 2);
  }
  if (block_size_index >= 8) {
    return G_GUINT64_CONSTANT (256) << (block_size_index - 8);
  }
  if (block_size_index == 6 || block_size_index == 7) {
    guint len, bytes = (block_size_index - 6) + 1;
    guint8 tmp;

    if (packet->bytes < 4 + 1 + bytes)
      return -1;
    tmp = packet->packet[4];
    /* utf-8 prefix */
    len = 0;
    while (tmp & 0x80) {
      len++;
      tmp <<= 1;
    }
    if (len == 2)
      return -1;
    if (len == 0)
      len++;
    if (packet->bytes < 4 + len + bytes)
      return -1;
    if (bytes == 1) {
      return packet->packet[4 + len] + 1;
    } else {
      return GST_READ_UINT16_BE (packet->packet + 4 + len) + 1;
    }
  }
  return -1;
}

static void
extract_tags_flac (GstOggStream * pad, ogg_packet * packet)
{
  if (packet->bytes > 4 && ((packet->packet[0] & 0x7F) == 0x4)) {
    tag_list_from_vorbiscomment_packet (packet,
        packet->packet, 4, &pad->taglist);

    if (!pad->taglist)
      pad->taglist = gst_tag_list_new_empty ();

    gst_tag_list_add (pad->taglist, GST_TAG_MERGE_REPLACE,
        GST_TAG_AUDIO_CODEC, "FLAC", NULL);
  }
}

/* fishead */

static gboolean
setup_fishead_mapper (GstOggStream * pad, ogg_packet * packet)
{
  guint8 *data;
  gint64 prestime_n, prestime_d;
  gint64 basetime_n, basetime_d;

  if (packet->bytes < 44) {
    GST_DEBUG ("Not enough data for fishead header");
    return FALSE;
  }

  data = packet->packet;

  data += 8;                    /* header */

  pad->skeleton_major = GST_READ_UINT16_LE (data);
  data += 2;
  pad->skeleton_minor = GST_READ_UINT16_LE (data);
  data += 2;

  prestime_n = (gint64) GST_READ_UINT64_LE (data);
  data += 8;
  prestime_d = (gint64) GST_READ_UINT64_LE (data);
  data += 8;
  basetime_n = (gint64) GST_READ_UINT64_LE (data);
  data += 8;
  basetime_d = (gint64) GST_READ_UINT64_LE (data);
  data += 8;

  /* FIXME: we don't use basetime anywhere in the demuxer! */
  if (basetime_d != 0)
    pad->basetime = gst_util_uint64_scale (GST_SECOND, basetime_n, basetime_d);
  else
    pad->basetime = -1;

  if (prestime_d != 0)
    pad->prestime = gst_util_uint64_scale (GST_SECOND, prestime_n, prestime_d);
  else
    pad->prestime = -1;

  /* Ogg Skeleton 3.3+ streams provide additional information in the header */
  if (packet->bytes >= SKELETON_FISHEAD_3_3_MIN_SIZE && pad->skeleton_major == 3
      && pad->skeleton_minor > 0) {
    gint64 firstsampletime_n, firstsampletime_d;
    gint64 lastsampletime_n, lastsampletime_d;
    gint64 firstsampletime, lastsampletime;
    guint64 segment_length, content_offset;

    firstsampletime_n = GST_READ_UINT64_LE (data + 64);
    firstsampletime_d = GST_READ_UINT64_LE (data + 72);
    lastsampletime_n = GST_READ_UINT64_LE (data + 80);
    lastsampletime_d = GST_READ_UINT64_LE (data + 88);
    segment_length = GST_READ_UINT64_LE (data + 96);
    content_offset = GST_READ_UINT64_LE (data + 104);

    GST_INFO ("firstsampletime %" G_GUINT64_FORMAT "/%" G_GUINT64_FORMAT,
        firstsampletime_n, firstsampletime_d);
    GST_INFO ("lastsampletime %" G_GUINT64_FORMAT "/%" G_GUINT64_FORMAT,
        lastsampletime_n, lastsampletime_d);
    GST_INFO ("segment length %" G_GUINT64_FORMAT, segment_length);
    GST_INFO ("content offset %" G_GUINT64_FORMAT, content_offset);

    if (firstsampletime_d > 0)
      firstsampletime = gst_util_uint64_scale (GST_SECOND,
          firstsampletime_n, firstsampletime_d);
    else
      firstsampletime = 0;

    if (lastsampletime_d > 0)
      lastsampletime = gst_util_uint64_scale (GST_SECOND,
          lastsampletime_n, lastsampletime_d);
    else
      lastsampletime = 0;

    if (lastsampletime > firstsampletime)
      pad->total_time = lastsampletime - firstsampletime;
    else
      pad->total_time = -1;

    GST_INFO ("skeleton fishead parsed total: %" GST_TIME_FORMAT,
        GST_TIME_ARGS (pad->total_time));
  } else if (packet->bytes >= SKELETON_FISHEAD_4_0_MIN_SIZE
      && pad->skeleton_major == 4) {
    guint64 segment_length, content_offset;

    segment_length = GST_READ_UINT64_LE (data + 64);
    content_offset = GST_READ_UINT64_LE (data + 72);

    GST_INFO ("segment length %" G_GUINT64_FORMAT, segment_length);
    GST_INFO ("content offset %" G_GUINT64_FORMAT, content_offset);
  } else {
    pad->total_time = -1;
  }

  GST_INFO ("skeleton fishead %u.%u parsed (basetime: %" GST_TIME_FORMAT
      ", prestime: %" GST_TIME_FORMAT ")", pad->skeleton_major,
      pad->skeleton_minor, GST_TIME_ARGS (pad->basetime),
      GST_TIME_ARGS (pad->prestime));

  pad->is_skeleton = TRUE;
  pad->is_sparse = TRUE;

  pad->caps = gst_caps_new_empty_simple ("application/x-ogg-skeleton");

  return TRUE;
}

gboolean
gst_ogg_map_parse_fisbone (GstOggStream * pad, const guint8 * data, guint size,
    guint32 * serialno, GstOggSkeleton * type)
{
  GstOggSkeleton stype;
  guint serial_offset;

  if (size != 0 && size < SKELETON_FISBONE_MIN_SIZE) {
    GST_WARNING ("small fisbone packet of size %d, ignoring", size);
    return FALSE;
  }

  if (size == 0) {
    /* Skeleton EOS packet is zero bytes */
    return FALSE;
  } else if (memcmp (data, "fisbone\0", 8) == 0) {
    GST_INFO ("got fisbone packet");
    stype = GST_OGG_SKELETON_FISBONE;
    serial_offset = 12;
  } else if (memcmp (data, "index\0", 6) == 0) {
    GST_INFO ("got index packet");
    stype = GST_OGG_SKELETON_INDEX;
    serial_offset = 6;
  } else if (memcmp (data, "fishead\0", 8) == 0) {
    return FALSE;
  } else {
    GST_WARNING ("unknown skeleton packet \"%10.10s\"", data);
    return FALSE;
  }

  if (serialno)
    *serialno = GST_READ_UINT32_LE (data + serial_offset);

  if (type)
    *type = stype;

  return TRUE;
}

gboolean
gst_ogg_map_add_fisbone (GstOggStream * pad, GstOggStream * skel_pad,
    const guint8 * data, guint size, GstClockTime * p_start_time)
{
  GstClockTime start_time;
  gint64 start_granule;

  if (pad->have_fisbone) {
    GST_DEBUG ("already have fisbone, ignoring second one");
    return FALSE;
  }

  /* skip "fisbone\0" + headers offset + serialno + num headers */
  data += 8 + 4 + 4 + 4;

  /* We don't overwrite whatever was set before by the format-specific
     setup: skeleton contains wrong information sometimes, and the codec
     headers are authoritative.
     So we only gather information that was not already filled out by
     the mapper setup. This should hopefully allow handling unknown
     streams a bit better, while not trashing correct setup from bad
     skeleton data. */
  if (pad->granulerate_n == 0 || pad->granulerate_d == 0) {
    pad->granulerate_n = GST_READ_UINT64_LE (data);
    pad->granulerate_d = GST_READ_UINT64_LE (data + 8);
  }
  if (pad->granuleshift == G_MAXUINT32) {
    pad->granuleshift = GST_READ_UINT8 (data + 28);
    if (pad->granuleshift >= 63) {
      /* Granuleshift can't be greater than the storage size of a granule */
      GST_WARNING ("Invalid granuleshift (%u >= 63)", pad->granuleshift);
      pad->granulerate_n = 0;
      pad->granulerate_d = 0;
      pad->granuleshift = -1;
      return FALSE;
    }
  }

  pad->have_fisbone = TRUE;

  start_granule = GST_READ_UINT64_LE (data + 16);
  pad->preroll = GST_READ_UINT32_LE (data + 24);

  start_time = granulepos_to_granule_default (pad, start_granule);

  GST_INFO ("skeleton fisbone parsed "
      "(start time: %" GST_TIME_FORMAT
      " granulerate_n: %d granulerate_d: %d "
      " preroll: %" G_GUINT32_FORMAT " granuleshift: %d)",
      GST_TIME_ARGS (start_time),
      pad->granulerate_n, pad->granulerate_d, pad->preroll, pad->granuleshift);

  if (p_start_time)
    *p_start_time = start_time;

  return TRUE;
}

static gboolean
read_vlc (const guint8 ** data, guint * size, guint64 * result)
{
  gint shift = 0;
  guint8 byte;

  *result = 0;

  do {
    if (G_UNLIKELY (*size < 1))
      return FALSE;

    byte = **data;
    *result |= ((byte & 0x7f) << shift);
    shift += 7;

    (*data)++;
    (*size)--;
  } while ((byte & 0x80) != 0x80);

  return TRUE;
}

gboolean
gst_ogg_map_add_index (GstOggStream * pad, GstOggStream * skel_pad,
    const guint8 * data, guint size)
{
  guint64 i, n_keypoints, isize;
  guint64 offset, timestamp;
  guint64 offset_d, timestamp_d;

  if (pad->index) {
    GST_DEBUG ("already have index, ignoring second one");
    return TRUE;
  }

  if ((skel_pad->skeleton_major == 3 && size < 26) ||
      (skel_pad->skeleton_major == 4 && size < 62)) {
    GST_WARNING ("small index packet of size %u, ignoring", size);
    return FALSE;
  }

  /* skip "index\0" + serialno */
  data += 6 + 4;
  size -= 6 + 4;

  n_keypoints = GST_READ_UINT64_LE (data);

  data += 8;
  size -= 8;

  pad->kp_denom = GST_READ_UINT64_LE (data);
  if (pad->kp_denom == 0)
    pad->kp_denom = 1;

  data += 8;
  size -= 8;

  if (skel_pad->skeleton_major == 4) {
    gint64 firstsampletime_n;
    gint64 lastsampletime_n;
    gint64 firstsampletime, lastsampletime;

    firstsampletime_n = GST_READ_UINT64_LE (data + 0);
    lastsampletime_n = GST_READ_UINT64_LE (data + 8);

    GST_INFO ("firstsampletime %" G_GUINT64_FORMAT "/%" G_GUINT64_FORMAT,
        firstsampletime_n, pad->kp_denom);
    GST_INFO ("lastsampletime %" G_GUINT64_FORMAT "/%" G_GUINT64_FORMAT,
        lastsampletime_n, pad->kp_denom);

    firstsampletime = gst_util_uint64_scale (GST_SECOND,
        firstsampletime_n, pad->kp_denom);
    lastsampletime = gst_util_uint64_scale (GST_SECOND,
        lastsampletime_n, pad->kp_denom);

    if (lastsampletime > firstsampletime)
      pad->total_time = lastsampletime - firstsampletime;
    else
      pad->total_time = -1;

    GST_INFO ("skeleton index parsed total: %" GST_TIME_FORMAT,
        GST_TIME_ARGS (pad->total_time));

    data += 16;
    size -= 16;
  }

  GST_INFO ("skeleton index has %" G_GUINT64_FORMAT " keypoints, denom: %"
      G_GINT64_FORMAT, n_keypoints, pad->kp_denom);

  pad->index = g_try_new (GstOggIndex, n_keypoints);
  if (!pad->index)
    return FALSE;

  isize = 0;
  offset = 0;
  timestamp = 0;

  for (i = 0; i < n_keypoints; i++) {
    /* read deltas */
    if (!read_vlc (&data, &size, &offset_d))
      break;
    if (!read_vlc (&data, &size, &timestamp_d))
      break;

    offset += offset_d;
    timestamp += timestamp_d;

    pad->index[i].offset = offset;
    pad->index[i].timestamp = timestamp;
    isize++;

    GST_INFO ("offset %" G_GUINT64_FORMAT " time %" G_GUINT64_FORMAT, offset,
        timestamp);
  }
  if (isize != n_keypoints) {
    GST_WARNING ("truncated index, expected %" G_GUINT64_FORMAT ", found %"
        G_GUINT64_FORMAT, n_keypoints, isize);
  }
  pad->n_index = isize;
  /* try to use the index to estimate the bitrate */
  if (isize > 2) {
    guint64 so, eo, st, et, b, t;

    /* get start and end offset and timestamps */
    so = pad->index[0].offset;
    st = pad->index[0].timestamp;
    eo = pad->index[isize - 1].offset;
    et = pad->index[isize - 1].timestamp;

    b = eo - so;
    t = et - st;

    GST_DEBUG ("bytes/time %" G_GUINT64_FORMAT "/%" G_GUINT64_FORMAT, b, t);

    /* this is the total stream bitrate according to this index */
    pad->idx_bitrate = gst_util_uint64_scale (8 * b, pad->kp_denom, t);

    GST_DEBUG ("bitrate %" G_GUINT64_FORMAT, pad->idx_bitrate);
  }

  return TRUE;
}

static gint
gst_ogg_index_compare (const GstOggIndex * index, const guint64 * ts,
    gpointer user_data)
{
  if (index->timestamp < *ts)
    return -1;
  else if (index->timestamp > *ts)
    return 1;
  else
    return 0;
}

gboolean
gst_ogg_map_search_index (GstOggStream * pad, gboolean before,
    guint64 * timestamp, guint64 * offset)
{
  guint64 n_index;
  guint64 ts;
  GstOggIndex *best;

  g_return_val_if_fail (timestamp != NULL, FALSE);
  g_return_val_if_fail (offset != NULL, FALSE);

  n_index = pad->n_index;
  if (n_index == 0 || pad->index == NULL)
    return FALSE;

  ts = gst_util_uint64_scale (*timestamp, pad->kp_denom, GST_SECOND);
  GST_INFO ("timestamp %" G_GUINT64_FORMAT, ts);

  best =
      gst_util_array_binary_search (pad->index, n_index, sizeof (GstOggIndex),
      (GCompareDataFunc) gst_ogg_index_compare, GST_SEARCH_MODE_BEFORE, &ts,
      NULL);

  if (best == NULL)
    return FALSE;

  GST_INFO ("found at index %u", (guint) (best - pad->index));

  *offset = best->offset;
  *timestamp =
      gst_util_uint64_scale (best->timestamp, GST_SECOND, pad->kp_denom);

  return TRUE;
}

/* Do we need these for something?
 * ogm->hdr.size = GST_READ_UINT32_LE (&data[13]);
 * ogm->hdr.time_unit = GST_READ_UINT64_LE (&data[17]);
 * ogm->hdr.samples_per_unit = GST_READ_UINT64_LE (&data[25]);
 * ogm->hdr.default_len = GST_READ_UINT32_LE (&data[33]);
 * ogm->hdr.buffersize = GST_READ_UINT32_LE (&data[37]);
 * ogm->hdr.bits_per_sample = GST_READ_UINT32_LE (&data[41]);
 */

static gboolean
is_header_ogm (GstOggStream * pad, ogg_packet * packet)
{
  if (packet->bytes >= 1 && (packet->packet[0] & 0x01))
    return TRUE;

  return FALSE;
}

static void
extract_tags_ogm (GstOggStream * pad, ogg_packet * packet)
{
  if (!(packet->packet[0] & 1) && (packet->packet[0] & 3 && pad->is_ogm_text)) {
    tag_list_from_vorbiscomment_packet (packet,
        (const guint8 *) "\003vorbis", 7, &pad->taglist);
  }
}

static gint64
packet_duration_ogm (GstOggStream * pad, ogg_packet * packet)
{
  const guint8 *data;
  gint64 samples;
  int offset;
  int n;

  data = packet->packet;
  offset = 1 + (((data[0] & 0xc0) >> 6) | ((data[0] & 0x02) << 1));

  if (offset > packet->bytes) {
    GST_WARNING ("buffer too small");
    return -1;
  }

  samples = 0;
  for (n = offset - 1; n > 0; n--) {
    samples = (samples << 8) | data[n];
  }

  return samples;
}

static gboolean
setup_ogmaudio_mapper (GstOggStream * pad, ogg_packet * packet)
{
  guint8 *data = packet->packet;
  guint32 fourcc;
  gchar *fstr;

  pad->granulerate_n = GST_READ_UINT64_LE (data + 25);
  pad->granulerate_d = 1;

  GST_LOG ("sample rate: %d", pad->granulerate_n);
  if (pad->granulerate_n == 0) {
    pad->granulerate_n = 0;
    pad->granulerate_d = 0;
    return FALSE;
  }

  fourcc = GST_READ_UINT32_LE (data + 9);
  fstr = g_strdup_printf ("%" GST_FOURCC_FORMAT, GST_FOURCC_ARGS (fourcc));
  GST_DEBUG ("fourcc: %s", fstr);

  /* FIXME: Need to do something with the reorder map */
  pad->caps =
      gst_riff_create_audio_caps (fourcc, NULL, NULL, NULL, NULL, NULL, NULL);

  if (pad->caps) {
    gst_caps_set_simple (pad->caps,
        "rate", G_TYPE_INT, pad->granulerate_n, NULL);
  } else {
    pad->caps = gst_caps_new_simple ("audio/x-ogm-unknown",
        "fourcc", G_TYPE_STRING, fstr,
        "rate", G_TYPE_INT, pad->granulerate_n, NULL);
  }
  g_free (fstr);

  pad->n_header_packets = 1;
  pad->is_ogm = TRUE;

  return TRUE;
}

static gboolean
setup_ogmvideo_mapper (GstOggStream * pad, ogg_packet * packet)
{
  guint8 *data = packet->packet;
  guint32 fourcc;
  int width, height;
  gint64 time_unit;
  gchar *fstr;

  GST_DEBUG ("time unit %d", GST_READ_UINT32_LE (data + 16));
  GST_DEBUG ("samples per unit %d", GST_READ_UINT32_LE (data + 24));

  pad->is_video = TRUE;
  pad->granulerate_n = 10000000;
  time_unit = GST_READ_UINT64_LE (data + 17);
  if (time_unit > G_MAXINT || time_unit < G_MININT || time_unit == 0) {
    GST_WARNING ("timeunit is out of range");
  }
  pad->granulerate_d = (gint) CLAMP (time_unit, G_MININT, G_MAXINT);

  GST_LOG ("fps = %d/%d = %.3f",
      pad->granulerate_n, pad->granulerate_d,
      (double) pad->granulerate_n / pad->granulerate_d);

  fourcc = GST_READ_UINT32_LE (data + 9);
  width = GST_READ_UINT32_LE (data + 45);
  height = GST_READ_UINT32_LE (data + 49);
  fstr = g_strdup_printf ("%" GST_FOURCC_FORMAT, GST_FOURCC_ARGS (fourcc));
  GST_DEBUG ("fourcc: %s", fstr);

  pad->caps = gst_riff_create_video_caps (fourcc, NULL, NULL, NULL, NULL, NULL);

  if (pad->caps == NULL) {
    pad->caps = gst_caps_new_simple ("video/x-ogm-unknown",
        "fourcc", G_TYPE_STRING, fstr,
        "framerate", GST_TYPE_FRACTION, pad->granulerate_n,
        pad->granulerate_d, NULL);
  } else {
    gst_caps_set_simple (pad->caps,
        "framerate", GST_TYPE_FRACTION, pad->granulerate_n,
        pad->granulerate_d,
        "width", G_TYPE_INT, width, "height", G_TYPE_INT, height, NULL);
  }
  GST_DEBUG ("caps: %" GST_PTR_FORMAT, pad->caps);
  g_free (fstr);

  pad->n_header_packets = 1;
  pad->frame_size = 1;
  pad->is_ogm = TRUE;

  return TRUE;
}

static gboolean
setup_ogmtext_mapper (GstOggStream * pad, ogg_packet * packet)
{
  guint8 *data = packet->packet;
  gint64 time_unit;

  pad->granulerate_n = 10000000;
  time_unit = GST_READ_UINT64_LE (data + 17);
  if (time_unit > G_MAXINT || time_unit < G_MININT) {
    GST_WARNING ("timeunit is out of range");
  }
  pad->granulerate_d = (gint) CLAMP (time_unit, G_MININT, G_MAXINT);

  GST_LOG ("fps = %d/%d = %.3f",
      pad->granulerate_n, pad->granulerate_d,
      (double) pad->granulerate_n / pad->granulerate_d);

  if (pad->granulerate_d <= 0) {
    pad->granulerate_n = 0;
    pad->granulerate_d = 0;
    return FALSE;
  }

  pad->caps = gst_caps_new_simple ("text/x-raw", "format", G_TYPE_STRING,
      "utf8", NULL);

  pad->n_header_packets = 1;
  pad->is_ogm = TRUE;
  pad->is_ogm_text = TRUE;
  pad->is_sparse = TRUE;

  return TRUE;
}

/* PCM */

#define OGGPCM_FMT_S8 0x00000000        /* Signed integer 8 bit */
#define OGGPCM_FMT_U8 0x00000001        /* Unsigned integer 8 bit */
#define OGGPCM_FMT_S16_LE 0x00000002    /* Signed integer 16 bit little endian */
#define OGGPCM_FMT_S16_BE 0x00000003    /* Signed integer 16 bit big endian */
#define OGGPCM_FMT_S24_LE 0x00000004    /* Signed integer 24 bit little endian */
#define OGGPCM_FMT_S24_BE 0x00000005    /* Signed integer 24 bit big endian */
#define OGGPCM_FMT_S32_LE 0x00000006    /* Signed integer 32 bit little endian */
#define OGGPCM_FMT_S32_BE 0x00000007    /* Signed integer 32 bit big endian */

#define OGGPCM_FMT_ULAW 0x00000010      /* G.711 u-law encoding (8 bit) */
#define OGGPCM_FMT_ALAW 0x00000011      /* G.711 A-law encoding (8 bit) */

#define OGGPCM_FMT_FLT32_LE 0x00000020  /* IEEE Float [-1,1] 32 bit little endian */
#define OGGPCM_FMT_FLT32_BE 0x00000021  /* IEEE Float [-1,1] 32 bit big endian */
#define OGGPCM_FMT_FLT64_LE 0x00000022  /* IEEE Float [-1,1] 64 bit little endian */
#define OGGPCM_FMT_FLT64_BE 0x00000023  /* IEEE Float [-1,1] 64 bit big endian */


static gboolean
setup_pcm_mapper (GstOggStream * pad, ogg_packet * packet)
{
  guint8 *data = packet->packet;
  int format;
  int channels;
  GstCaps *caps;

  pad->granulerate_n = GST_READ_UINT32_LE (data + 16);
  pad->granulerate_d = 1;
  GST_LOG ("sample rate: %d", pad->granulerate_n);

  if (pad->granulerate_n == 0) {
    pad->granulerate_n = 0;
    pad->granulerate_d = 0;
    return FALSE;
  }

  format = GST_READ_UINT32_LE (data + 12);
  channels = GST_READ_UINT8 (data + 21);
  switch (format) {
    case OGGPCM_FMT_S8:
      caps = gst_caps_new_simple ("audio/x-raw",
          "format", G_TYPE_STRING, "S8", NULL);
      break;
    case OGGPCM_FMT_U8:
      caps = gst_caps_new_simple ("audio/x-raw",
          "format", G_TYPE_STRING, "U8", NULL);
      break;
    case OGGPCM_FMT_S16_LE:
      caps = gst_caps_new_simple ("audio/x-raw",
          "format", G_TYPE_STRING, "S16LE", NULL);
      break;
    case OGGPCM_FMT_S16_BE:
      caps = gst_caps_new_simple ("audio/x-raw",
          "format", G_TYPE_STRING, "S16BE", NULL);
      break;
    case OGGPCM_FMT_S24_LE:
      caps = gst_caps_new_simple ("audio/x-raw",
          "format", G_TYPE_STRING, "S24LE", NULL);
      break;
    case OGGPCM_FMT_S24_BE:
      caps = gst_caps_new_simple ("audio/x-raw",
          "format", G_TYPE_STRING, "S24BE", NULL);
      break;
    case OGGPCM_FMT_S32_LE:
      caps = gst_caps_new_simple ("audio/x-raw",
          "format", G_TYPE_STRING, "S32LE", NULL);
      break;
    case OGGPCM_FMT_S32_BE:
      caps = gst_caps_new_simple ("audio/x-raw",
          "format", G_TYPE_STRING, "S32BE", NULL);
      break;
    case OGGPCM_FMT_ULAW:
      caps = gst_caps_new_empty_simple ("audio/x-mulaw");
      break;
    case OGGPCM_FMT_ALAW:
      caps = gst_caps_new_empty_simple ("audio/x-alaw");
      break;
    case OGGPCM_FMT_FLT32_LE:
      caps = gst_caps_new_simple ("audio/x-raw",
          "format", G_TYPE_STRING, "F32LE", NULL);
      break;
    case OGGPCM_FMT_FLT32_BE:
      caps = gst_caps_new_simple ("audio/x-raw",
          "format", G_TYPE_STRING, "F32BE", NULL);
      break;
    case OGGPCM_FMT_FLT64_LE:
      caps = gst_caps_new_simple ("audio/x-raw",
          "format", G_TYPE_STRING, "F64LE", NULL);
      break;
    case OGGPCM_FMT_FLT64_BE:
      caps = gst_caps_new_simple ("audio/x-raw",
          "format", G_TYPE_STRING, "F64BE", NULL);
      break;
    default:
      pad->granulerate_n = 0;
      pad->granulerate_d = 0;
      return FALSE;
  }

  pad->n_header_packets = 2 + GST_READ_UINT32_LE (data + 24);

  gst_caps_set_simple (caps,
      "layout", G_TYPE_STRING, "interleaved",
      "rate", G_TYPE_INT, pad->granulerate_n,
      "channels", G_TYPE_INT, channels, NULL);
  pad->caps = caps;

  return TRUE;
}

/* cmml */

static gboolean
setup_cmml_mapper (GstOggStream * pad, ogg_packet * packet)
{
  guint8 *data = packet->packet;

  pad->granulerate_n = GST_READ_UINT64_LE (data + 12);
  pad->granulerate_d = GST_READ_UINT64_LE (data + 20);
  pad->granuleshift = data[28];

  if (pad->granuleshift >= 63) {
    /* Granuleshift can't be greater than the storage size of a granule */
    GST_WARNING ("Invalid granuleshift (%u >= 63)", pad->granuleshift);
    pad->granulerate_n = 0;
    pad->granulerate_d = 0;
    pad->granuleshift = -1;
    return FALSE;
  }
  GST_LOG ("sample rate: %d", pad->granulerate_n);

  if (pad->granulerate_n == 0) {
    pad->granulerate_n = 0;
    pad->granulerate_d = 0;
    pad->granuleshift = -1;
    return FALSE;
  }

  pad->n_header_packets = 3;

  data += 4 + (4 + 4 + 4);
  GST_DEBUG ("blocksize0: %u", 1 << (data[0] >> 4));
  GST_DEBUG ("blocksize1: %u", 1 << (data[0] & 0x0F));

  pad->caps = gst_caps_new_empty_simple ("text/x-cmml");
  pad->always_flush_page = TRUE;
  pad->is_sparse = TRUE;
  pad->is_cmml = TRUE;

  return TRUE;
}

/* celt */

static gboolean
setup_celt_mapper (GstOggStream * pad, ogg_packet * packet)
{
  guint8 *data = packet->packet;

  pad->granulerate_n = GST_READ_UINT32_LE (data + 36);
  pad->granulerate_d = 1;
  pad->granuleshift = 0;
  GST_LOG ("sample rate: %d", pad->granulerate_n);

  pad->frame_size = GST_READ_UINT32_LE (packet->packet + 44);
  pad->n_header_packets = GST_READ_UINT32_LE (packet->packet + 56) + 2;

  if (pad->granulerate_n == 0) {
    pad->granulerate_n = 0;
    pad->granulerate_d = 0;
    pad->granuleshift = -1;
    return FALSE;
  }

  pad->caps = gst_caps_new_simple ("audio/x-celt",
      "rate", G_TYPE_INT, pad->granulerate_n, NULL);

  return TRUE;
}

/* kate */

static gboolean
setup_kate_mapper (GstOggStream * pad, ogg_packet * packet)
{
  guint8 *data = packet->packet;
  const char *category;

  if (packet->bytes < 64)
    return FALSE;

  pad->granulerate_n = GST_READ_UINT32_LE (data + 24);
  pad->granulerate_d = GST_READ_UINT32_LE (data + 28);
  pad->granuleshift = GST_READ_UINT8 (data + 15);
  if (pad->granuleshift >= 63) {
    /* Granuleshift can't be greater than the storage size of a granule */
    GST_WARNING ("Invalid granuleshift (%u >= 63)", pad->granuleshift);
    pad->granulerate_n = 0;
    pad->granulerate_d = 0;
    pad->granuleshift = -1;
    return FALSE;
  }
  GST_LOG ("sample rate: %d", pad->granulerate_n);

  if (pad->granulerate_n == 0) {
    pad->granulerate_n = 0;
    pad->granulerate_d = 0;
    pad->granuleshift = -1;
    return FALSE;
  }

  pad->n_header_packets = GST_READ_UINT8 (data + 11);
  GST_LOG ("kate header packets: %d", pad->n_header_packets);

  category = (const char *) data + 48;
  if (strcmp (category, "subtitles") == 0 || strcmp (category, "SUB") == 0 ||
      strcmp (category, "spu-subtitles") == 0 ||
      strcmp (category, "K-SPU") == 0) {
    pad->caps = gst_caps_new_empty_simple ("subtitle/x-kate");
  } else {
    pad->caps = gst_caps_new_empty_simple ("application/x-kate");
  }

  pad->is_sparse = TRUE;
  pad->always_flush_page = TRUE;

  return TRUE;
}

static gint64
packet_duration_kate (GstOggStream * pad, ogg_packet * packet)
{
  gint64 duration;

  if (packet->bytes < 1)
    return 0;

  switch (packet->packet[0]) {
    case 0x00:                 /* text data */
      if (packet->bytes < 1 + 8 * 2) {
        duration = 0;
      } else {
        duration = GST_READ_UINT64_LE (packet->packet + 1 + 8);
        if (duration < 0)
          duration = 0;
      }
      break;
    default:
      duration = GST_CLOCK_TIME_NONE;
      break;
  }

  return duration;
}

static void
extract_tags_kate (GstOggStream * pad, ogg_packet * packet)
{
  GstTagList *list = NULL;

  if (packet->bytes <= 0)
    return;

  switch (packet->packet[0]) {
    case 0x80:{
      const gchar *canonical;
      char language[16];

      if (packet->bytes < 64) {
        GST_WARNING ("Kate ID header packet is less than 64 bytes, ignored");
        break;
      }

      /* the language tag is 16 bytes at offset 32, ensure NUL terminator */
      memcpy (language, packet->packet + 32, 16);
      language[15] = 0;

      /* language is an ISO 639-1 code or RFC 3066 language code, we
       * truncate to ISO 639-1 */
      g_strdelimit (language, NULL, '\0');
      canonical = gst_tag_get_language_code_iso_639_1 (language);
      if (canonical) {
        list = gst_tag_list_new (GST_TAG_LANGUAGE_CODE, canonical, NULL);
      } else {
        GST_WARNING ("Unknown or invalid language code %s, ignored", language);
      }
      break;
    }
    case 0x81:
      tag_list_from_vorbiscomment_packet (packet,
          (const guint8 *) "\201kate\0\0\0\0", 9, &list);

      if (list != NULL) {
        gst_tag_list_add (list, GST_TAG_MERGE_REPLACE,
            GST_TAG_SUBTITLE_CODEC, "Kate", NULL);
      }
      break;
    default:
      break;
  }

  if (list) {
    if (pad->taglist) {
      /* ensure the comment packet cannot override the category/language
         from the identification header */
      gst_tag_list_insert (pad->taglist, list, GST_TAG_MERGE_KEEP_ALL);
      gst_tag_list_unref (list);
    } else
      pad->taglist = list;
  }
}

/* opus */

static gboolean
setup_opus_mapper (GstOggStream * pad, ogg_packet * packet)
{
  GstBuffer *buffer;

  if (packet->bytes < 19)
    return FALSE;

  pad->granulerate_n = 48000;
  pad->granulerate_d = 1;
  pad->granuleshift = 0;
  pad->n_header_packets = 2;
  pad->first_granpos = -1;
  pad->audio_clipping = TRUE;

  /* pre-skip is in samples at 48000 Hz, which matches granule one for one */
  pad->granule_offset = -GST_READ_UINT16_LE (packet->packet + 10);
  GST_INFO ("Opus has a pre-skip of %" G_GINT64_FORMAT " samples",
      -pad->granule_offset);

  buffer = gst_buffer_new_memdup (packet->packet, packet->bytes);
  pad->caps = gst_codec_utils_opus_create_caps_from_header (buffer, NULL);
  gst_buffer_unref (buffer);

  return TRUE;
}

static gboolean
is_header_opus (GstOggStream * pad, ogg_packet * packet)
{
  return packet->bytes >= 8 && !memcmp (packet->packet, "Opus", 4);
}

static gint64
granulepos_to_granule_opus (GstOggStream * pad, gint64 granulepos)
{
  if (granulepos == -1)
    return -1;

  if (pad->first_granpos < 0 || granulepos < pad->first_granpos)
    pad->first_granpos = granulepos;

  return granulepos;
}

static gint64
packet_duration_opus (GstOggStream * pad, ogg_packet * packet)
{
  static const guint64 durations[32] = {
    480, 960, 1920, 2880,       /* Silk NB */
    480, 960, 1920, 2880,       /* Silk MB */
    480, 960, 1920, 2880,       /* Silk WB */
    480, 960,                   /* Hybrid SWB */
    480, 960,                   /* Hybrid FB */
    120, 240, 480, 960,         /* CELT NB */
    120, 240, 480, 960,         /* CELT NB */
    120, 240, 480, 960,         /* CELT NB */
    120, 240, 480, 960,         /* CELT NB */
  };

  gint64 duration;
  gint64 frame_duration;
  gint nframes = 0;
  guint8 toc;

  if (packet->bytes < 1)
    return 0;

  /* headers */
  if (is_header_opus (pad, packet))
    return 0;

  toc = packet->packet[0];

  frame_duration = durations[toc >> 3];
  switch (toc & 3) {
    case 0:
      nframes = 1;
      break;
    case 1:
      nframes = 2;
      break;
    case 2:
      nframes = 2;
      break;
    case 3:
      if (packet->bytes < 2) {
        GST_WARNING ("Code 3 Opus packet has less than 2 bytes");
        return 0;
      }
      nframes = packet->packet[1] & 63;
      break;
  }

  duration = nframes * frame_duration;
  if (duration > 5760) {
    GST_WARNING ("Opus packet duration > 120 ms, invalid");
    return 0;
  }
  GST_LOG ("Opus packet: frame size %.1f ms, %d frames, duration %.1f ms",
      frame_duration / 48.f, nframes, duration / 48.f);
  return duration;
}

static void
extract_tags_opus (GstOggStream * pad, ogg_packet * packet)
{
  if (packet->bytes >= 8 && memcmp (packet->packet, "OpusTags", 8) == 0) {
    tag_list_from_vorbiscomment_packet (packet,
        (const guint8 *) "OpusTags", 8, &pad->taglist);

    if (!pad->taglist)
      pad->taglist = gst_tag_list_new_empty ();

    gst_tag_list_add (pad->taglist, GST_TAG_MERGE_REPLACE,
        GST_TAG_AUDIO_CODEC, "Opus", NULL);
  }
}

/* daala */

static gboolean
setup_daala_mapper (GstOggStream * pad, ogg_packet * packet)
{
  guint8 *data = packet->packet;
  guint w, h, par_d, par_n;
  guint8 vmaj, vmin, vrev;
  guint frame_duration;

  vmaj = data[6];
  vmin = data[7];
  vrev = data[8];

  GST_LOG ("daala %d.%d.%d", vmaj, vmin, vrev);

  w = GST_READ_UINT32_LE (data + 9);
  h = GST_READ_UINT32_LE (data + 13);

  par_n = GST_READ_UINT32_LE (data + 17);
  par_d = GST_READ_UINT32_LE (data + 21);

  pad->granulerate_n = GST_READ_UINT32_LE (data + 25);
  pad->granulerate_d = GST_READ_UINT32_LE (data + 29);
  frame_duration = GST_READ_UINT32_LE (data + 33);

  GST_LOG ("fps = %d/%d, dur %d, PAR = %u/%u, width = %u, height = %u",
      pad->granulerate_n, pad->granulerate_d, frame_duration, par_n, par_d, w,
      h);

  pad->granuleshift = GST_READ_UINT8 (data + 37);
  if (pad->granuleshift >= 63) {
    /* Granuleshift can't be greater than the storage size of a granule */
    GST_WARNING ("Invalid granuleshift (%u >= 63)", pad->granuleshift);
    pad->granulerate_n = 0;
    pad->granulerate_d = 0;
    pad->granuleshift = -1;
    return FALSE;
  }
  GST_LOG ("granshift: %d", pad->granuleshift);

  if (pad->granulerate_n == 0 || pad->granulerate_d == 0) {
    GST_WARNING ("frame rate %d/%d", pad->granulerate_n, pad->granulerate_d);
    pad->granulerate_n = 0;
    pad->granulerate_d = 0;
    pad->granuleshift = -1;
    return FALSE;
  }

  pad->is_video = TRUE;
  pad->n_header_packets = 3;
  pad->frame_size = 1;

  pad->caps = gst_caps_new_empty_simple ("video/x-daala");

  if (w > 0 && h > 0) {
    gst_caps_set_simple (pad->caps, "width", G_TYPE_INT, w, "height",
        G_TYPE_INT, h, NULL);
  }

  /* PAR of 0:N, N:0 and 0:0 is allowed and maps to 1:1 */
  if (par_n == 0 || par_d == 0)
    par_n = par_d = 1;

  /* only add framerate now so caps look prettier, with width/height first */
  gst_caps_set_simple (pad->caps, "framerate", GST_TYPE_FRACTION,
      pad->granulerate_n, pad->granulerate_d, "pixel-aspect-ratio",
      GST_TYPE_FRACTION, par_n, par_d, NULL);

  return TRUE;
}

static gint64
granulepos_to_granule_daala (GstOggStream * pad, gint64 granulepos)
{
  gint64 keyindex, keyoffset;

  if (pad->granuleshift != 0 && pad->granuleshift != G_MAXUINT32) {
    keyindex = granulepos >> pad->granuleshift;
    keyoffset = granulepos - (keyindex << pad->granuleshift);
    return keyindex + keyoffset;
  } else {
    return granulepos;
  }
}

static gboolean
is_granulepos_keyframe_daala (GstOggStream * pad, gint64 granulepos)
{
  gint64 frame_mask;

  if (granulepos == (gint64) - 1 || pad->granuleshift == G_MAXUINT32)
    return FALSE;

  frame_mask = (G_GUINT64_CONSTANT (1) << pad->granuleshift) - 1;

  return ((granulepos & frame_mask) == 0);
}

static gboolean
is_packet_keyframe_daala (GstOggStream * pad, ogg_packet * packet)
{
  if (packet->bytes == 0)
    return FALSE;
  return (packet->packet[0] & 0x40);
}

static gboolean
is_header_daala (GstOggStream * pad, ogg_packet * packet)
{
  return (packet->bytes > 0 && (packet->packet[0] & 0x80) == 0x80);
}

static void
extract_tags_daala (GstOggStream * pad, ogg_packet * packet)
{
  if (packet->bytes > 0 && packet->packet[0] == 0x81) {
    tag_list_from_vorbiscomment_packet (packet,
        (const guint8 *) "\201daala", 5, &pad->taglist);

    if (!pad->taglist)
      pad->taglist = gst_tag_list_new_empty ();

    gst_tag_list_add (pad->taglist, GST_TAG_MERGE_REPLACE,
        GST_TAG_VIDEO_CODEC, "Daala", NULL);

    if (pad->bitrate)
      gst_tag_list_add (pad->taglist, GST_TAG_MERGE_REPLACE,
          GST_TAG_BITRATE, (guint) pad->bitrate, NULL);
  }
}

/* *INDENT-OFF* */
/* indent hates our freedoms */
const GstOggMap mappers[] = {
  {
    /* Empty mapper for uninitialized pads/streams */
    NULL, 0, G_MAXINT32,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL
  },
  {
    "\200theora", 7, 42,
    "video/x-theora",
    setup_theora_mapper,
    NULL,
    granulepos_to_granule_theora,
    granule_to_granulepos_default,
    is_granulepos_keyframe_theora,
    is_packet_keyframe_theora,
    is_header_theora,
    packet_duration_constant,
    NULL,
    extract_tags_theora,
    NULL,
    NULL
  },
  {
    "\001vorbis", 7, 22,
    "audio/x-vorbis",
    setup_vorbis_mapper,
    NULL,
    granulepos_to_granule_default,
    granule_to_granulepos_default,
    is_granulepos_keyframe_true,
    is_packet_keyframe_true,
    is_header_vorbis,
    packet_duration_vorbis,
    NULL,
    extract_tags_vorbis,
    NULL,
    NULL
  },
  {
    "Speex", 5, 80,
    "audio/x-speex",
    setup_speex_mapper,
    NULL,
    granulepos_to_granule_default,
    granule_to_granulepos_default,
    is_granulepos_keyframe_true,
    is_packet_keyframe_true,
    is_header_count,
    packet_duration_constant,
    NULL,
    extract_tags_count,
    NULL,
    NULL
  },
  {
    "PCM     ", 8, 0,
    "audio/x-raw",
    setup_pcm_mapper,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    is_header_count,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL
  },
  {
    "CMML\0\0\0\0", 8, 0,
    "text/x-cmml",
    setup_cmml_mapper,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    is_header_count,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL
  },
  {
    "Annodex", 7, 0,
    "application/x-annodex",
    setup_fishead_mapper,
    NULL,
    granulepos_to_granule_default,
    granule_to_granulepos_default,
    NULL,
    NULL,
    is_header_count,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL
  },
  {
    "fishead", 7, 64,
    "application/octet-stream",
    setup_fishead_mapper,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    is_header_true,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL
  },
  {
    "fLaC", 4, 0,
    "audio/x-flac",
    setup_fLaC_mapper,
    NULL,
    granulepos_to_granule_default,
    granule_to_granulepos_default,
    is_granulepos_keyframe_true,
    is_packet_keyframe_true,
    is_header_fLaC,
    packet_duration_flac,
    NULL,
    NULL,
    NULL,
    NULL
  },
  {
    "\177FLAC", 5, 36,
    "audio/x-flac",
    setup_flac_mapper,
    NULL,
    granulepos_to_granule_default,
    granule_to_granulepos_default,
    is_granulepos_keyframe_true,
    is_packet_keyframe_true,
    is_header_flac,
    packet_duration_flac,
    NULL,
    extract_tags_flac,
    NULL,
    NULL
  },
  {
    "AnxData", 7, 0,
    "application/octet-stream",
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL
  },
  {
    "CELT    ", 8, 0,
    "audio/x-celt",
    setup_celt_mapper,
    NULL,
    granulepos_to_granule_default,
    granule_to_granulepos_default,
    NULL,
    NULL,
    is_header_count,
    packet_duration_constant,
    NULL,
    extract_tags_count,
    NULL,
    NULL
  },
  {
    "\200kate\0\0\0", 8, 0,
    "text/x-kate",
    setup_kate_mapper,
    NULL,
    granulepos_to_granule_default,
    granule_to_granulepos_default,
    NULL,
    NULL,
    is_header_count,
    packet_duration_kate,
    NULL,
    extract_tags_kate,
    NULL,
    NULL
  },
  {
    "BBCD\0", 5, 13,
    "video/x-dirac",
    setup_dirac_mapper,
    NULL,
    granulepos_to_granule_dirac,
    granule_to_granulepos_dirac,
    is_keyframe_dirac,
    NULL,
    is_header_count,
    packet_duration_constant,
    granulepos_to_key_granule_dirac,
    NULL,
    NULL,
    NULL
  },
  {
    "OVP80\1\1", 7, 4,
    "video/x-vp8",
    setup_vp8_mapper,
    setup_vp8_mapper_from_caps,
    granulepos_to_granule_vp8,
    granule_to_granulepos_vp8,
    is_keyframe_vp8,
    is_packet_keyframe_vp8,
    is_header_vp8,
    packet_duration_vp8,
    granulepos_to_key_granule_vp8,
    extract_tags_vp8,
    get_headers_vp8,
    update_stats_vp8
  },
  {
    "OpusHead", 8, 0,
    "audio/x-opus",
    setup_opus_mapper,
    NULL,
    granulepos_to_granule_opus,
    granule_to_granulepos_default,
    NULL,
    is_packet_keyframe_true,
    is_header_opus,
    packet_duration_opus,
    NULL,
    extract_tags_opus,
    NULL,
    NULL
  },
  {
    "\001audio\0\0\0", 9, 53,
    "application/x-ogm-audio",
    setup_ogmaudio_mapper,
    NULL,
    granulepos_to_granule_default,
    granule_to_granulepos_default,
    is_granulepos_keyframe_true,
    is_packet_keyframe_true,
    is_header_ogm,
    packet_duration_ogm,
    NULL,
    NULL,
    NULL,
    NULL
  },
  {
    "\001video\0\0\0", 9, 53,
    "application/x-ogm-video",
    setup_ogmvideo_mapper,
    NULL,
    granulepos_to_granule_default,
    granule_to_granulepos_default,
    NULL,
    NULL,
    is_header_ogm,
    packet_duration_constant,
    NULL,
    NULL,
    NULL,
    NULL
  },
  {
    "\001text\0\0\0", 9, 9,
    "application/x-ogm-text",
    setup_ogmtext_mapper,
    NULL,
    granulepos_to_granule_default,
    granule_to_granulepos_default,
    is_granulepos_keyframe_true,
    is_packet_keyframe_true,
    is_header_ogm,
    packet_duration_ogm,
    NULL,
    extract_tags_ogm,
    NULL,
    NULL
  },
  {
    "\200daala", 6, 42,
    "video/x-daala",
    setup_daala_mapper,
    NULL,
    granulepos_to_granule_daala,
    granule_to_granulepos_default,
    is_granulepos_keyframe_daala,
    is_packet_keyframe_daala,
    is_header_daala,
    packet_duration_constant,
    NULL,
    extract_tags_daala,
    NULL,
    NULL
  },
 
};
/* *INDENT-ON* */

gboolean
gst_ogg_stream_setup_map (GstOggStream * pad, ogg_packet * packet)
{
  int i;
  gboolean ret;

  for (i = 0; i < G_N_ELEMENTS (mappers); i++) {
    if (packet->bytes >= mappers[i].min_packet_size &&
        packet->bytes >= mappers[i].id_length &&
        memcmp (packet->packet, mappers[i].id, mappers[i].id_length) == 0) {

      GST_DEBUG ("found mapper for '%s'", mappers[i].id);

      if (mappers[i].setup_func)
        ret = mappers[i].setup_func (pad, packet);
      else
        continue;

      if (ret) {
        GST_DEBUG ("got stream type %" GST_PTR_FORMAT, pad->caps);
        pad->map = i;
        return TRUE;
      } else {
        GST_WARNING ("mapper '%s' did not accept setup header",
            mappers[i].media_type);
      }
    }
  }

  return FALSE;
}

gboolean
gst_ogg_stream_setup_map_from_caps (GstOggStream * pad, const GstCaps * caps)
{
  int i;
  gboolean ret;
  GstStructure *structure;

  g_return_val_if_fail (caps != NULL, FALSE);

  structure = gst_caps_get_structure (caps, 0);

  for (i = 0; i < G_N_ELEMENTS (mappers); i++) {
    if (mappers[i].setup_from_caps_func &&
        gst_structure_has_name (structure, mappers[i].media_type)) {

      GST_DEBUG ("found mapper for '%s'", mappers[i].id);

      if (mappers[i].setup_from_caps_func)
        ret = mappers[i].setup_from_caps_func (pad, caps);
      else
        continue;

      if (ret) {
        GST_DEBUG ("got stream type %" GST_PTR_FORMAT, pad->caps);
        pad->map = i;
        return TRUE;
      } else {
        GST_WARNING ("mapper '%s' did not accept caps %" GST_PTR_FORMAT,
            mappers[i].media_type, caps);
      }
    }
  }

  return FALSE;
}

gboolean
gst_ogg_stream_setup_map_from_caps_headers (GstOggStream * pad,
    const GstCaps * caps)
{
  GstBuffer *buf;
  const GstStructure *structure;
  const GValue *streamheader;
  const GValue *first_element;
  ogg_packet packet;
  GstMapInfo map;
  gboolean ret;

  GST_INFO ("Checking streamheader on caps %" GST_PTR_FORMAT, caps);

  if (caps == NULL)
    return FALSE;

  structure = gst_caps_get_structure (caps, 0);
  streamheader = gst_structure_get_value (structure, "streamheader");

  if (streamheader == NULL) {
    GST_LOG ("no streamheader field in caps %" GST_PTR_FORMAT, caps);
    return FALSE;
  }

  if (!GST_VALUE_HOLDS_ARRAY (streamheader)) {
    GST_ERROR ("streamheader field not an array, caps: %" GST_PTR_FORMAT, caps);
    return FALSE;
  }

  if (gst_value_array_get_size (streamheader) == 0) {
    GST_ERROR ("empty streamheader field in caps %" GST_PTR_FORMAT, caps);
    return FALSE;
  }

  first_element = gst_value_array_get_value (streamheader, 0);

  if (!GST_VALUE_HOLDS_BUFFER (first_element)) {
    GST_ERROR ("first streamheader not a buffer, caps: %" GST_PTR_FORMAT, caps);
    return FALSE;
  }

  buf = gst_value_get_buffer (first_element);
  if (buf == NULL) {
    GST_ERROR ("no first streamheader buffer");
    return FALSE;
  }

  if (!gst_buffer_map (buf, &map, GST_MAP_READ) || map.size == 0) {
    GST_ERROR ("invalid first streamheader buffer");
    return FALSE;
  }

  GST_MEMDUMP ("streamheader", map.data, map.size);

  packet.packet = map.data;
  packet.bytes = map.size;

  GST_INFO ("Found headers on caps, using those to determine type");
  ret = gst_ogg_stream_setup_map (pad, &packet);

  gst_buffer_unmap (buf, &map);

  return ret;
}
