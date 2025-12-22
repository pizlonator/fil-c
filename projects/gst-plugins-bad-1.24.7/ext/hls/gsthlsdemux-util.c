/* GStreamer
 * Copyright (C) 2016 Jan Schmidt <jan@centricular.com>
 * Copyright (C) 2016 Tim-Philipp Müller <tim@centricular.com>
 *
 * gsthlsdemux-util.c:
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

#include <gst/gst.h>
#include <gst/tag/tag.h>
#include <string.h>

#include "gsthlsdemux.h"

GST_DEBUG_CATEGORY_EXTERN (gst_hls_demux_debug);
#define GST_CAT_DEFAULT gst_hls_demux_debug

/* Check for sync byte, error_indicator == 0 and packet has payload.
 * Adaptation control field (data[3] & 0x30) may be zero for TS packets with
 * null PIDs. Still, these streams are valid TS streams (for null packets,
 * AFC is supposed to be 0x1, but the spec also says decoders should just
 * discard any packets with AFC = 0x00) */
#define IS_MPEGTS_HEADER(data) (data[0] == 0x47 && \
                                (data[1] & 0x80) == 0x00 && \
                                ((data[3] & 0x30) != 0x00 || \
                                ((data[3] & 0x30) == 0x00 && (data[1] & 0x1f) == 0x1f && (data[2] & 0xff) == 0xff)))

#define PCRTIME_TO_GSTTIME(t) (((t) * (guint64)1000) / 27)
#define MPEGTIME_TO_GSTTIME(t) (((t) * (guint64)100000) / 9)

static gboolean
have_ts_sync (const guint8 * data, guint size, guint packet_size, guint num)
{
  while (num-- > 0) {
    if (size < packet_size)
      return FALSE;
    if (!IS_MPEGTS_HEADER (data))
      return FALSE;
    data += packet_size;
    size -= packet_size;
  }
  return TRUE;
}

static gint
find_offset (GstHLSTSReader * r, const guint8 * data, guint size)
{
  guint sync_points = CLAMP (size / 188, 25, 100);
  guint off;
  const gint packet_size = 188;

  /* FIXME: check 192 as well, and maybe also 204, 208 */
  for (off = 0; off < MIN (size, packet_size); ++off) {
    if (have_ts_sync (data + off, size - off, packet_size, sync_points)) {
      r->packet_size = packet_size;
      return off;
    }
  }
  return -1;
}

static gboolean
handle_pcr (GstHLSTSReader * r, const guint8 * data, guint size)
{
  const guint8 *p = data;
  guint32 hdr = GST_READ_UINT32_BE (p);
  guint af_len, flags;

  guint64 pcr_base, pcr_ext, pcr, ts;

  data = p + 4;
  if ((hdr & 0x00000020) == 0)  /* has_adaptation_field */
    return FALSE;
  af_len = p[4];                /* adaptation_field_len */
  ++data;
  if (af_len < (1 + 6) || af_len > r->packet_size - (4 + 1))
    return FALSE;
  flags = data[0];
  /* Does the packet have a PCR? */
  if ((flags & 0x10) == 0)
    return FALSE;
  ++data;
  --af_len;
  pcr_base = (GST_READ_UINT64_BE (data) >> 16) >> (6 + 9);
  pcr_ext = (GST_READ_UINT64_BE (data) >> 16) & 0x1ff;
  pcr = pcr_base * 300 + pcr_ext;
  ts = PCRTIME_TO_GSTTIME (pcr);
  GST_LOG ("have PCR! %" G_GUINT64_FORMAT "\t%" GST_TIME_FORMAT,
      pcr, GST_TIME_ARGS (ts));
  if (r->first_pcr == GST_CLOCK_TIME_NONE)
    r->first_pcr = ts;
  r->last_pcr = ts;

  return TRUE;
}

static gboolean
handle_pmt (GstHLSTSReader * r, const guint8 * data, guint size)
{
  const guint8 *p = data;
  guint32 hdr = GST_READ_UINT32_BE (p);
  guint slen, pcr_pid;

  data = p + 4;
  if ((hdr & 0x00000020) != 0)  /* has_adaptation_field */
    data += 1 + p[4];           /* adaptation_field_len */
  data += 1 + data[0];          /* pointer_field */
  if (data[0] != 0x02)          /* table_id */
    return FALSE;
  //gst_util_dump_mem (data, 8);
  /* we assume the entire PMT fits into a single packet and this is it */
  if (data[6] != 0 || data[6] != data[7])
    return FALSE;
  slen = GST_READ_UINT16_BE (data + 1) & 0x0FFF;
  if (slen > (gsize) (p + r->packet_size - (data + 1 + 2)) || slen < 5 + 2 + 4)
    return FALSE;
  data += 3 + 5;
  slen -= 5;                    /* bytes after section_length field itself */
  slen -= 4;                    /* crc at end */
  pcr_pid = GST_READ_UINT16_BE (data) & 0x1fff;
  if (pcr_pid != 0x1fff) {
    GST_DEBUG ("pcr_pid now: %04x", pcr_pid);
    r->pcr_pid = pcr_pid;
    return TRUE;
  }

  return FALSE;
}

static gboolean
handle_pat (GstHLSTSReader * r, const guint8 * data, guint size)
{
  const guint8 *p = data;
  guint32 hdr = GST_READ_UINT32_BE (p);
  guint slen;

  data = p + 4;
  if ((hdr & 0x00000020) != 0)  /* has_adaptation_field */
    data += 1 + p[4];           /* adaptation_field_len */
  data += 1 + data[0];          /* pointer_field */
  if (data[0] != 0)             /* table_id */
    return FALSE;
  /* we assume the entire PAT fits into a single packet and this is it */
  if (data[6] != 0 || data[6] != data[7])
    return FALSE;
  slen = GST_READ_UINT16_BE (data + 1) & 0x0FFF;
  if (slen > (gsize) (p + r->packet_size - (data + 1 + 2)) || slen < 5 + 4 + 4)
    return FALSE;
  data += 3 + 5;
  slen -= 5;                    /* bytes after section_length field itself */
  slen -= 4;                    /* crc at end */
  while (slen >= 4) {
    guint program_num = GST_READ_UINT16_BE (data);
    guint val = GST_READ_UINT16_BE (data + 2) & 0x1fff;
    if (program_num != 0) {
      GST_DEBUG ("  program %04x: pmt_pid : %04x", program_num, val);
      r->pmt_pid = val;
      return TRUE;
    }
    data += 4;
    slen -= 4;
  }

  return FALSE;
}

void
gst_hlsdemux_tsreader_init (GstHLSTSReader * r)
{
  r->rtype = GST_HLS_TSREADER_NONE;
  r->packet_size = 188;
  r->pmt_pid = r->pcr_pid = -1;
  r->first_pcr = GST_CLOCK_TIME_NONE;
  r->last_pcr = GST_CLOCK_TIME_NONE;
}

void
gst_hlsdemux_tsreader_set_type (GstHLSTSReader * r, GstHLSTSReaderType rtype)
{
  r->rtype = rtype;
  r->have_id3 = FALSE;
}

static gboolean
gst_hlsdemux_tsreader_find_pcrs_mpegts (GstHLSTSReader * r,
    GstBuffer * buffer, GstClockTime * first_pcr, GstClockTime * last_pcr)
{
  GstMapInfo info;
  gint offset;
  const guint8 *p;
  const guint8 *data;
  gsize size;

  if (!gst_buffer_map (buffer, &info, GST_MAP_READ))
    return FALSE;

  data = info.data;
  size = info.size;

  *first_pcr = *last_pcr = GST_CLOCK_TIME_NONE;

  offset = find_offset (r, data, size);
  if (offset < 0) {
    gst_buffer_unmap (buffer, &info);
    return FALSE;
  }

  GST_LOG ("TS packet start offset: %d", offset);

  /* We don't store a partial packet at the end,
   * and just assume that the final PCR is 
   * going to be completely inside the last data
   * segment passed to us */
  data += offset;
  size -= offset;

  for (p = data; size >= r->packet_size;
      p += r->packet_size, size -= r->packet_size) {
    guint32 hdr = GST_READ_UINT32_BE (p);

    /* sync byte (0x47), error indicator (TEI) not set, PID 0, has_payload */
    if ((hdr & 0xFF9FFF10) == 0x47000010) {
      GST_LOG ("Found packet for PID 0000 (PAT)");
      handle_pat (r, p, size);
    }
    /* sync byte (0x47), error indicator (TEI) not set, has_payload, PID = PMT_pid */
    else if ((hdr & 0xFF800010) == 0x47000010
        && ((hdr >> 8) & 0x1fff) == r->pmt_pid) {
      GST_LOG ("Found packet for PID %04x (PMT)", r->pmt_pid);
      handle_pmt (r, p, size);
    }
    /* sync byte (0x47), error indicator (TEI) not set, has_adaptation_field */
    else if ((hdr & 0xFF800020) == 0x47000020
        && ((hdr >> 8) & 0x1fff) == r->pcr_pid) {
      GST_LOG ("Found packet for PID %04x (PCR)", r->pcr_pid);
      handle_pcr (r, p, size);
    }
  }

  gst_buffer_unmap (buffer, &info);

  *first_pcr = r->first_pcr;
  *last_pcr = r->last_pcr;

  /* Return TRUE if this piece was big enough to get a PCR from */
  return (r->first_pcr != GST_CLOCK_TIME_NONE);
}

static gboolean
gst_hlsdemux_tsreader_find_pcrs_id3 (GstHLSTSReader * r,
    GstBuffer ** buffer_out, GstClockTime * first_pcr, GstClockTime * last_pcr,
    GstTagList ** tags)
{
  GstMapInfo info;
  guint32 tag_size;
  gsize size;
  GstTagList *taglist;
  GstSample *priv_data = NULL;
  GstBuffer *buffer = *buffer_out;
  GstBuffer *tag_buf;
  guint64 pts;

  *first_pcr = r->first_pcr;
  *last_pcr = r->last_pcr;

  if (r->have_id3)
    return TRUE;

  /* We need at least 10 bytes, starting with "ID3" for the header */
  size = gst_buffer_get_size (buffer);
  if (size < 10)
    return FALSE;

  /* Read the tag size */
  tag_size = gst_tag_get_id3v2_tag_size (buffer);

  /* Check we've collected that much */
  if (size < tag_size)
    return FALSE;

  /* From here, whether the tag is valid or not we'll
   * not try and read again */
  r->have_id3 = TRUE;

  *buffer_out =
      gst_buffer_copy_region (buffer, GST_BUFFER_COPY_ALL, tag_size, -1);

  /* Parse the tag */
  taglist = gst_tag_list_from_id3v2_tag (buffer);
  if (taglist == NULL) {
    gst_buffer_unref (buffer);
    return TRUE;                /* Invalid tag, stop trying */
  }

  *tags = taglist;

  /* Extract the timestamps */
  if (!gst_tag_list_get_sample (taglist, GST_TAG_PRIVATE_DATA, &priv_data))
    goto out;

  if (!g_str_equal ("com.apple.streaming.transportStreamTimestamp",
          gst_structure_get_string (gst_sample_get_info (priv_data), "owner")))
    goto out;

  /* OK, now as per section 3, the tag contains a 33-bit PCR inside a 64-bit
   * BE-word */
  tag_buf = gst_sample_get_buffer (priv_data);
  if (tag_buf == NULL)
    goto out;

  if (!gst_buffer_map (tag_buf, &info, GST_MAP_READ))
    goto out;

  pts = GST_READ_UINT64_BE (info.data);
  *first_pcr = r->first_pcr = MPEGTIME_TO_GSTTIME (pts);

  GST_LOG ("Got AAC TS PTS %" G_GUINT64_FORMAT " (%" G_GUINT64_FORMAT ")",
      pts, r->first_pcr);

  gst_buffer_unmap (tag_buf, &info);

out:
  if (priv_data)
    gst_sample_unref (priv_data);
  gst_buffer_unref (buffer);

  return TRUE;
}

gboolean
gst_hlsdemux_tsreader_find_pcrs (GstHLSTSReader * r,
    GstBuffer ** buffer, GstClockTime * first_pcr, GstClockTime * last_pcr,
    GstTagList ** tags)
{
  *tags = NULL;

  if (r->rtype == GST_HLS_TSREADER_MPEGTS)
    return gst_hlsdemux_tsreader_find_pcrs_mpegts (r, *buffer, first_pcr,
        last_pcr);

  return gst_hlsdemux_tsreader_find_pcrs_id3 (r, buffer, first_pcr, last_pcr,
      tags);
}
