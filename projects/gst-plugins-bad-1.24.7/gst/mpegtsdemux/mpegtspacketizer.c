/*
 * mpegtspacketizer.c -
 * Copyright (C) 2007, 2008 Alessandro Decina, Zaheer Merali
 *
 * Authors:
 *   Zaheer Merali <zaheerabbas at merali dot org>
 *   Alessandro Decina <alessandro@nnva.org>
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

#include <string.h>
#include <stdlib.h>

/* Skew calculation pameters */
#define MAX_TIME	(2 * GST_SECOND)

/* 90kHz maximum values, coded in 33bits */
#define MAX_33BIT (((guint64)1) << 33)
#define PTS_DTS_MAX_VALUE (MAX_33BIT - 1)
/* maximal PCR time, 27Mhz, coded with additional 9bits */
#define PCR_MAX_VALUE (MAX_33BIT * 300 - 1)
#define PCR_GST_MAX_VALUE (PCR_MAX_VALUE * GST_MSECOND / (PCR_MSECOND))

#include "mpegtspacketizer.h"
#include "gstmpegdesc.h"

GST_DEBUG_CATEGORY_STATIC (mpegts_packetizer_debug);
#define GST_CAT_DEFAULT mpegts_packetizer_debug

static void _init_local (void);
G_DEFINE_TYPE_EXTENDED (MpegTSPacketizer2, mpegts_packetizer, G_TYPE_OBJECT, 0,
    _init_local ());

#define ABSDIFF(a,b) ((a) < (b) ? (b) - (a) : (a) - (b))

#define PACKETIZER_GROUP_LOCK(p) g_mutex_lock(&((p)->group_lock))
#define PACKETIZER_GROUP_UNLOCK(p) g_mutex_unlock(&((p)->group_lock))

static void mpegts_packetizer_dispose (GObject * object);
static void mpegts_packetizer_finalize (GObject * object);
static GstClockTime calculate_skew (MpegTSPacketizer2 * packetizer,
    MpegTSPCR * pcr, guint64 pcrtime, GstClockTime time);
static void _close_current_group (MpegTSPCR * pcrtable);
static void record_pcr (MpegTSPacketizer2 * packetizer, MpegTSPCR * pcrtable,
    guint64 pcr, guint64 offset);

#define CONTINUITY_UNSET 255
#define VERSION_NUMBER_UNSET 255
#define TABLE_ID_UNSET 0xFF
#define PACKET_SYNC_BYTE 0x47

static inline MpegTSPCR *
get_pcr_table (MpegTSPacketizer2 * packetizer, guint16 pid)
{
  MpegTSPCR *res;

  res = packetizer->observations[packetizer->pcrtablelut[pid]];

  if (G_UNLIKELY (res == NULL)) {
    /* If we don't have a PCR table for the requested PID, create one .. */
    res = g_new0 (MpegTSPCR, 1);
    /* Add it to the last table position */
    packetizer->observations[packetizer->lastobsid] = res;
    /* Update the pcrtablelut */
    packetizer->pcrtablelut[pid] = packetizer->lastobsid;
    /* And increment the last know slot */
    packetizer->lastobsid++;

    /* Finally set the default values */
    res->pid = pid;
    res->base_time = GST_CLOCK_TIME_NONE;
    res->base_pcrtime = GST_CLOCK_TIME_NONE;
    res->last_pcrtime = GST_CLOCK_TIME_NONE;
    res->window_pos = 0;
    res->window_filling = TRUE;
    res->window_min = 0;
    res->skew = 0;
    res->prev_send_diff = GST_CLOCK_TIME_NONE;
    res->prev_out_time = GST_CLOCK_TIME_NONE;
    res->pcroffset = 0;

    res->current = g_new0 (PCROffsetCurrent, 1);
  }

  return res;
}

static void
pcr_offset_group_free (PCROffsetGroup * group)
{
  g_free (group->values);
  g_free (group);
}

static void
flush_observations (MpegTSPacketizer2 * packetizer)
{
  gint i;

  for (i = 0; i < packetizer->lastobsid; i++) {
    g_list_free_full (packetizer->observations[i]->groups,
        (GDestroyNotify) pcr_offset_group_free);
    g_free (packetizer->observations[i]->current);
    g_free (packetizer->observations[i]);
    packetizer->observations[i] = NULL;
  }
  memset (packetizer->pcrtablelut, 0xff, 0x2000);
  packetizer->lastobsid = 0;
}

GstClockTime
mpegts_packetizer_get_current_time (MpegTSPacketizer2 * packetizer,
    guint16 pcr_pid)
{
  MpegTSPCR *pcrtable = get_pcr_table (packetizer, pcr_pid);

  if (pcrtable == NULL)
    return GST_CLOCK_TIME_NONE;

  return mpegts_packetizer_pts_to_ts (packetizer, pcrtable->last_pcrtime,
      pcr_pid);
}

static inline MpegTSPacketizerStreamSubtable *
find_subtable (GSList * subtables, guint8 table_id, guint16 subtable_extension)
{
  GSList *tmp;

  /* FIXME: Make this an array ! */
  for (tmp = subtables; tmp; tmp = tmp->next) {
    MpegTSPacketizerStreamSubtable *sub =
        (MpegTSPacketizerStreamSubtable *) tmp->data;
    if (sub->table_id == table_id
        && sub->subtable_extension == subtable_extension)
      return sub;
  }

  return NULL;
}

static gboolean
seen_section_before (MpegTSPacketizerStream * stream, guint8 table_id,
    guint16 subtable_extension, guint8 version_number, guint8 section_number,
    guint8 last_section_number)
{
  MpegTSPacketizerStreamSubtable *subtable;

  /* Check if we've seen this table_id/subtable_extension first */
  subtable = find_subtable (stream->subtables, table_id, subtable_extension);
  if (!subtable) {
    GST_DEBUG ("Haven't seen subtable");
    return FALSE;
  }
  /* If we have, check it has the same version_number */
  if (subtable->version_number != version_number) {
    GST_DEBUG ("Different version number");
    return FALSE;
  }
  /* Did the number of sections change ? */
  if (subtable->last_section_number != last_section_number) {
    GST_DEBUG ("Different last_section_number");
    return FALSE;
  }
  /* Finally return whether we saw that section or not */
  return MPEGTS_BIT_IS_SET (subtable->seen_section, section_number);
}

static MpegTSPacketizerStreamSubtable *
mpegts_packetizer_stream_subtable_new (guint8 table_id,
    guint16 subtable_extension, guint8 last_section_number)
{
  MpegTSPacketizerStreamSubtable *subtable;

  subtable = g_new0 (MpegTSPacketizerStreamSubtable, 1);
  subtable->version_number = VERSION_NUMBER_UNSET;
  subtable->table_id = table_id;
  subtable->subtable_extension = subtable_extension;
  subtable->last_section_number = last_section_number;
  return subtable;
}

static MpegTSPacketizerStream *
mpegts_packetizer_stream_new (guint16 pid)
{
  MpegTSPacketizerStream *stream;

  stream = (MpegTSPacketizerStream *) g_new0 (MpegTSPacketizerStream, 1);
  stream->continuity_counter = CONTINUITY_UNSET;
  stream->subtables = NULL;
  stream->table_id = TABLE_ID_UNSET;
  stream->pid = pid;
  return stream;
}

static void
mpegts_packetizer_clear_section (MpegTSPacketizerStream * stream)
{
  stream->continuity_counter = CONTINUITY_UNSET;
  stream->section_length = 0;
  stream->section_offset = 0;
  stream->table_id = TABLE_ID_UNSET;
  g_free (stream->section_data);
  stream->section_data = NULL;
}

static void
mpegts_packetizer_stream_subtable_free (MpegTSPacketizerStreamSubtable *
    subtable)
{
  g_free (subtable);
}

static void
mpegts_packetizer_stream_free (MpegTSPacketizerStream * stream)
{
  mpegts_packetizer_clear_section (stream);
  g_slist_foreach (stream->subtables,
      (GFunc) mpegts_packetizer_stream_subtable_free, NULL);
  g_slist_free (stream->subtables);
  g_free (stream);
}

static void
mpegts_packetizer_class_init (MpegTSPacketizer2Class * klass)
{
  GObjectClass *gobject_class;

  gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->dispose = mpegts_packetizer_dispose;
  gobject_class->finalize = mpegts_packetizer_finalize;
}

static void
mpegts_packetizer_init (MpegTSPacketizer2 * packetizer)
{
  g_mutex_init (&packetizer->group_lock);

  packetizer->adapter = gst_adapter_new ();
  packetizer->offset = 0;
  packetizer->empty = TRUE;
  packetizer->streams = g_new0 (MpegTSPacketizerStream *, 8192);
  packetizer->packet_size = 0;
  packetizer->calculate_skew = FALSE;
  packetizer->calculate_offset = FALSE;

  packetizer->map_data = NULL;
  packetizer->map_size = 0;
  packetizer->map_offset = 0;
  packetizer->need_sync = FALSE;

  memset (packetizer->pcrtablelut, 0xff, 0x2000);
  memset (packetizer->observations, 0x0, sizeof (packetizer->observations));
  packetizer->lastobsid = 0;

  packetizer->nb_seen_offsets = 0;
  packetizer->refoffset = -1;
  packetizer->last_in_time = GST_CLOCK_TIME_NONE;
  packetizer->pcr_discont_threshold = GST_SECOND;
  packetizer->last_pts = GST_CLOCK_TIME_NONE;
  packetizer->last_dts = GST_CLOCK_TIME_NONE;
  packetizer->extra_shift = 0;
}

static void
mpegts_packetizer_dispose (GObject * object)
{
  MpegTSPacketizer2 *packetizer = GST_MPEGTS_PACKETIZER (object);

  if (!packetizer->disposed) {
    if (packetizer->packet_size)
      packetizer->packet_size = 0;
    if (packetizer->streams) {
      int i;
      for (i = 0; i < 8192; i++) {
        if (packetizer->streams[i])
          mpegts_packetizer_stream_free (packetizer->streams[i]);
      }
      g_free (packetizer->streams);
    }

    gst_adapter_clear (packetizer->adapter);
    g_object_unref (packetizer->adapter);
    g_mutex_clear (&packetizer->group_lock);
    packetizer->disposed = TRUE;
    packetizer->offset = 0;
    packetizer->empty = TRUE;

    flush_observations (packetizer);
  }

  if (G_OBJECT_CLASS (mpegts_packetizer_parent_class)->dispose)
    G_OBJECT_CLASS (mpegts_packetizer_parent_class)->dispose (object);
}

static void
mpegts_packetizer_finalize (GObject * object)
{
  if (G_OBJECT_CLASS (mpegts_packetizer_parent_class)->finalize)
    G_OBJECT_CLASS (mpegts_packetizer_parent_class)->finalize (object);
}

static inline guint64
mpegts_packetizer_compute_pcr (const guint8 * data)
{
  guint32 pcr1;
  guint16 pcr2;
  guint64 pcr, pcr_ext;

  pcr1 = GST_READ_UINT32_BE (data);
  pcr2 = GST_READ_UINT16_BE (data + 4);
  pcr = ((guint64) pcr1) << 1;
  pcr |= (pcr2 & 0x8000) >> 15;
  pcr_ext = (pcr2 & 0x01ff);
  return pcr * 300 + pcr_ext % 300;
}

static gboolean
mpegts_packetizer_parse_adaptation_field_control (MpegTSPacketizer2 *
    packetizer, MpegTSPacketizerPacket * packet)
{
  guint8 length, afcflags;
  guint8 *data;

  length = *packet->data++;

  /* an adaptation field with length 0 is valid and
   * can be used to insert a single stuffing byte */
  if (!length) {
    packet->afc_flags = 0;
    return TRUE;
  }

  if ((packet->scram_afc_cc & 0x30) == 0x20) {
    /* no payload, adaptation field of 183 bytes */
    if (length > 183) {
      GST_WARNING ("PID 0x%04x afc == 0x%02x and length %d > 183",
          packet->pid, packet->scram_afc_cc & 0x30, length);
      return FALSE;
    }
    if (length != 183) {
      GST_WARNING ("PID 0x%04x afc == 0x%02x and length %d != 183",
          packet->pid, packet->scram_afc_cc & 0x30, length);
      GST_MEMDUMP ("Unknown payload", packet->data + length,
          packet->data_end - packet->data - length);
    }
  } else if (length == 183) {
    /* Note: According to the specification, the adaptation field length
     * must be 183 if there is no payload data and < 183 if the packet
     * contains an adaptation field and payload data.
     * Some payloaders always set the flag for payload data, even if the
     * adaptation field length is 183. This just means a zero length
     * payload so we clear the payload flag here and continue.
     */
    GST_DEBUG ("PID 0x%04x afc == 0x%02x and length %d == 183 (ignored)",
        packet->pid, packet->scram_afc_cc & 0x30, length);
    packet->scram_afc_cc &= ~0x10;
  } else if (length > 182) {
    GST_WARNING ("PID 0x%04x afc == 0x%02x and length %d > 182",
        packet->pid, packet->scram_afc_cc & 0x30, length);
    return FALSE;
  }

  if (packet->data + length > packet->data_end) {
    GST_DEBUG
        ("PID 0x%04x afc length %d overflows the buffer current %d max %d",
        packet->pid, length, (gint) (packet->data - packet->data_start),
        (gint) (packet->data_end - packet->data_start));
    return FALSE;
  }

  data = packet->data;
  packet->data += length;

  afcflags = packet->afc_flags = *data++;

  GST_DEBUG ("flags: %s%s%s%s%s%s%s%s%s",
      afcflags & 0x80 ? "discontinuity " : "",
      afcflags & 0x40 ? "random_access " : "",
      afcflags & 0x20 ? "elementary_stream_priority " : "",
      afcflags & 0x10 ? "PCR " : "",
      afcflags & 0x08 ? "OPCR " : "",
      afcflags & 0x04 ? "splicing_point " : "",
      afcflags & 0x02 ? "transport_private_data " : "",
      afcflags & 0x01 ? "extension " : "", afcflags == 0x00 ? "<none>" : "");

  /* PCR */
  if (afcflags & MPEGTS_AFC_PCR_FLAG) {
    MpegTSPCR *pcrtable = NULL;
    packet->pcr = mpegts_packetizer_compute_pcr (data);
    data += 6;
    GST_DEBUG ("pcr 0x%04x %" G_GUINT64_FORMAT " (%" GST_TIME_FORMAT
        ") offset:%" G_GUINT64_FORMAT, packet->pid, packet->pcr,
        GST_TIME_ARGS (PCRTIME_TO_GSTTIME (packet->pcr)), packet->offset);

    PACKETIZER_GROUP_LOCK (packetizer);
    if (packetizer->calculate_skew
        && GST_CLOCK_TIME_IS_VALID (packetizer->last_in_time)) {
      pcrtable = get_pcr_table (packetizer, packet->pid);
      calculate_skew (packetizer, pcrtable, packet->pcr,
          packetizer->last_in_time);
    }
    if (packetizer->calculate_offset) {
      if (!pcrtable)
        pcrtable = get_pcr_table (packetizer, packet->pid);
      record_pcr (packetizer, pcrtable, packet->pcr, packet->offset);
    }
    PACKETIZER_GROUP_UNLOCK (packetizer);
  }
#ifndef GST_DISABLE_GST_DEBUG
  /* OPCR */
  if (afcflags & MPEGTS_AFC_OPCR_FLAG) {
    /* Note: We don't use/need opcr for the time being */
    guint64 opcr = mpegts_packetizer_compute_pcr (data);
    data += 6;
    GST_DEBUG ("opcr %" G_GUINT64_FORMAT " (%" GST_TIME_FORMAT ")",
        opcr, GST_TIME_ARGS (PCRTIME_TO_GSTTIME (opcr)));
  }

  if (afcflags & MPEGTS_AFC_SPLICING_POINT_FLAG) {
    GST_DEBUG ("splice_countdown: %u", *data++);
  }

  if (afcflags & MPEGTS_AFC_TRANSPORT_PRIVATE_DATA_FLAG) {
    guint8 len = *data++;
    GST_MEMDUMP ("private data", data, len);
    data += len;
  }

  if (afcflags & MPEGTS_AFC_EXTENSION_FLAG) {
    guint8 extlen = *data++;
    guint8 flags = *data++;
    GST_DEBUG ("extension size:%d flags : %s%s%s", extlen,
        flags & 0x80 ? "ltw " : "",
        flags & 0x40 ? "piecewise_rate " : "",
        flags & 0x20 ? "seamless_splice " : "");
    if (flags & 0x80) {
      GST_DEBUG ("legal time window: valid_flag:%d offset:%d", *data >> 7,
          GST_READ_UINT16_BE (data) & 0x7fff);
      data += 2;
    }
  }
#endif

  return TRUE;
}

static MpegTSPacketizerPacketReturn
mpegts_packetizer_parse_packet (MpegTSPacketizer2 * packetizer,
    MpegTSPacketizerPacket * packet)
{
  guint8 *data;
  guint8 tmp;

  data = packet->data_start;
  data += 1;
  tmp = *data;

  /* transport_error_indicator 1 */
  if (G_UNLIKELY (tmp & 0x80))
    return PACKET_BAD;

  /* payload_unit_start_indicator 1 */
  packet->payload_unit_start_indicator = tmp & 0x40;

  /* transport_priority 1 */
  /* PID 13 */
  packet->pid = GST_READ_UINT16_BE (data) & 0x1FFF;
  data += 2;

  packet->scram_afc_cc = tmp = *data++;
  /* transport_scrambling_control 2 */
  if (G_UNLIKELY (tmp & 0xc0))
    return PACKET_BAD;

  packet->data = data;

  packet->afc_flags = 0;
  packet->pcr = G_MAXUINT64;

  if (FLAGS_HAS_AFC (tmp)) {
    if (!mpegts_packetizer_parse_adaptation_field_control (packetizer, packet))
      return FALSE;
  }

  if (FLAGS_HAS_PAYLOAD (packet->scram_afc_cc))
    packet->payload = packet->data;
  else
    packet->payload = NULL;

  return PACKET_OK;
}

static GstMpegtsSection *
mpegts_packetizer_parse_section_header (MpegTSPacketizer2 * packetizer,
    MpegTSPacketizerStream * stream)
{
  MpegTSPacketizerStreamSubtable *subtable;
  GstMpegtsSection *res;

  subtable =
      find_subtable (stream->subtables, stream->table_id,
      stream->subtable_extension);
  if (subtable) {
    GST_DEBUG ("Found previous subtable_extension:0x%04x",
        stream->subtable_extension);
    if (G_UNLIKELY (stream->version_number != subtable->version_number)) {
      /* If the version number changed, reset the subtable */
      subtable->version_number = stream->version_number;
      subtable->last_section_number = stream->last_section_number;
      memset (subtable->seen_section, 0, 32);
    }
  } else {
    GST_DEBUG ("Appending new subtable_extension: 0x%04x",
        stream->subtable_extension);
    subtable = mpegts_packetizer_stream_subtable_new (stream->table_id,
        stream->subtable_extension, stream->last_section_number);
    subtable->version_number = stream->version_number;

    stream->subtables = g_slist_prepend (stream->subtables, subtable);
  }

  GST_MEMDUMP ("Full section data", stream->section_data,
      stream->section_length);
  /* TODO ? : Replace this by an efficient version (where we provide all
   * pre-parsed header data) */
  res =
      gst_mpegts_section_new (stream->pid, stream->section_data,
      stream->section_length);
  stream->section_data = NULL;
  mpegts_packetizer_clear_section (stream);

  if (res) {
    /* NOTE : Due to the new mpegts-si system, There is a insanely low probability
     * that we might have gotten a section that was corrupted (i.e. wrong crc)
     * and that we consider it as seen.
     *
     * The reason why we consider this as acceptable is because all the previous
     * checks were already done:
     * * transport layer checks (DVB)
     * * 0x47 validation
     * * continuity counter validation
     * * subtable validation
     * * section_number validation
     * * section_length validation
     *
     * The probability of this happening vs the overhead of doing CRC checks
     * on all sections (including those we would not use) is just not worth it.
     * */
    MPEGTS_BIT_SET (subtable->seen_section, stream->section_number);
    res->offset = stream->offset;
  }

  return res;
}

void
mpegts_packetizer_clear (MpegTSPacketizer2 * packetizer)
{
  guint i;
  MpegTSPCR *pcrtable;

  packetizer->packet_size = 0;

  if (packetizer->streams) {
    int i;
    for (i = 0; i < 8192; i++) {
      if (packetizer->streams[i]) {
        mpegts_packetizer_stream_free (packetizer->streams[i]);
      }
    }
    memset (packetizer->streams, 0, 8192 * sizeof (MpegTSPacketizerStream *));
  }

  gst_adapter_clear (packetizer->adapter);
  packetizer->offset = 0;
  packetizer->empty = TRUE;
  packetizer->need_sync = FALSE;
  packetizer->map_data = NULL;
  packetizer->map_size = 0;
  packetizer->map_offset = 0;
  packetizer->last_in_time = GST_CLOCK_TIME_NONE;
  packetizer->last_pts = GST_CLOCK_TIME_NONE;
  packetizer->last_dts = GST_CLOCK_TIME_NONE;

  pcrtable = packetizer->observations[packetizer->pcrtablelut[0x1fff]];
  if (pcrtable)
    pcrtable->base_time = GST_CLOCK_TIME_NONE;

  /* Close current PCR group */
  PACKETIZER_GROUP_LOCK (packetizer);

  for (i = 0; i < MAX_PCR_OBS_CHANNELS; i++) {
    if (packetizer->observations[i])
      _close_current_group (packetizer->observations[i]);
    else
      break;
  }
  PACKETIZER_GROUP_UNLOCK (packetizer);
}

void
mpegts_packetizer_flush (MpegTSPacketizer2 * packetizer, gboolean hard)
{
  guint i;
  MpegTSPCR *pcrtable;
  GST_DEBUG ("Flushing");

  if (packetizer->streams) {
    for (i = 0; i < 8192; i++) {
      if (packetizer->streams[i]) {
        mpegts_packetizer_clear_section (packetizer->streams[i]);
      }
    }
  }
  gst_adapter_clear (packetizer->adapter);

  packetizer->offset = 0;
  packetizer->empty = TRUE;
  packetizer->need_sync = FALSE;
  packetizer->map_data = NULL;
  packetizer->map_size = 0;
  packetizer->map_offset = 0;
  packetizer->last_in_time = GST_CLOCK_TIME_NONE;
  packetizer->last_pts = GST_CLOCK_TIME_NONE;
  packetizer->last_dts = GST_CLOCK_TIME_NONE;

  pcrtable = packetizer->observations[packetizer->pcrtablelut[0x1fff]];
  if (pcrtable)
    pcrtable->base_time = GST_CLOCK_TIME_NONE;

  /* Close current PCR group */
  PACKETIZER_GROUP_LOCK (packetizer);
  for (i = 0; i < MAX_PCR_OBS_CHANNELS; i++) {
    if (packetizer->observations[i])
      _close_current_group (packetizer->observations[i]);
    else
      break;
  }
  PACKETIZER_GROUP_UNLOCK (packetizer);

  if (hard) {
    /* For pull mode seeks in tsdemux the observation must be preserved */
    flush_observations (packetizer);
  }
}

void
mpegts_packetizer_remove_stream (MpegTSPacketizer2 * packetizer, gint16 pid)
{
  MpegTSPacketizerStream *stream = packetizer->streams[pid];
  if (stream) {
    GST_INFO ("Removing stream for PID 0x%04x", pid);
    mpegts_packetizer_stream_free (stream);
    packetizer->streams[pid] = NULL;
  }
}

MpegTSPacketizer2 *
mpegts_packetizer_new (void)
{
  MpegTSPacketizer2 *packetizer;

  packetizer =
      GST_MPEGTS_PACKETIZER (g_object_new (GST_TYPE_MPEGTS_PACKETIZER, NULL));

  return packetizer;
}

void
mpegts_packetizer_push (MpegTSPacketizer2 * packetizer, GstBuffer * buffer)
{
  GstClockTime ts;
  if (G_UNLIKELY (packetizer->empty)) {
    packetizer->empty = FALSE;
    packetizer->offset = GST_BUFFER_OFFSET (buffer);
  }

  GST_DEBUG ("Pushing %" G_GSIZE_FORMAT " byte from offset %"
      G_GUINT64_FORMAT, gst_buffer_get_size (buffer),
      GST_BUFFER_OFFSET (buffer));
  gst_adapter_push (packetizer->adapter, buffer);
  /* If the buffer has a valid timestamp, store it - preferring DTS,
   * which is where upstream arrival times should be stored */
  ts = GST_BUFFER_DTS_OR_PTS (buffer);
  if (GST_CLOCK_TIME_IS_VALID (ts))
    packetizer->last_in_time = ts;
  packetizer->last_pts = GST_BUFFER_PTS (buffer);
  packetizer->last_dts = GST_BUFFER_DTS (buffer);
}

static void
mpegts_packetizer_flush_bytes (MpegTSPacketizer2 * packetizer, gsize size)
{
  if (size > 0) {
    GST_LOG ("flushing %" G_GSIZE_FORMAT " bytes from adapter", size);
    gst_adapter_flush (packetizer->adapter, size);
  }

  packetizer->map_data = NULL;
  packetizer->map_size = 0;
  packetizer->map_offset = 0;
}

static gboolean
mpegts_packetizer_map (MpegTSPacketizer2 * packetizer, gsize size)
{
  gsize available;

  if (packetizer->map_size - packetizer->map_offset >= size)
    return TRUE;

  mpegts_packetizer_flush_bytes (packetizer, packetizer->map_offset);

  available = gst_adapter_available (packetizer->adapter);
  if (available < size)
    return FALSE;

  packetizer->map_data =
      (guint8 *) gst_adapter_map (packetizer->adapter, available);
  if (!packetizer->map_data)
    return FALSE;

  packetizer->map_size = available;
  packetizer->map_offset = 0;

  GST_LOG ("mapped %" G_GSIZE_FORMAT " bytes from adapter", available);

  return TRUE;
}

static gboolean
mpegts_try_discover_packet_size (MpegTSPacketizer2 * packetizer)
{
  guint8 *data;
  gsize size, i, j;

  static const guint psizes[] = {
    MPEGTS_NORMAL_PACKETSIZE,
    MPEGTS_M2TS_PACKETSIZE,
    MPEGTS_DVB_ASI_PACKETSIZE,
    MPEGTS_ATSC_PACKETSIZE
  };

  if (!mpegts_packetizer_map (packetizer, 4 * MPEGTS_MAX_PACKETSIZE))
    return FALSE;

  size = packetizer->map_size - packetizer->map_offset;
  data = packetizer->map_data + packetizer->map_offset;

  for (i = 0; i + 3 * MPEGTS_MAX_PACKETSIZE < size; i++) {
    /* find a sync byte */
    if (data[i] != PACKET_SYNC_BYTE)
      continue;

    /* check for 4 consecutive sync bytes with each possible packet size */
    for (j = 0; j < G_N_ELEMENTS (psizes); j++) {
      guint packet_size = psizes[j];

      if (data[i + packet_size] == PACKET_SYNC_BYTE &&
          data[i + 2 * packet_size] == PACKET_SYNC_BYTE &&
          data[i + 3 * packet_size] == PACKET_SYNC_BYTE) {
        packetizer->packet_size = packet_size;
        goto out;
      }
    }
  }

out:
  packetizer->map_offset += i;

  if (packetizer->packet_size == 0) {
    GST_DEBUG ("Could not determine packet size in %" G_GSIZE_FORMAT
        " bytes buffer, flush %" G_GSIZE_FORMAT " bytes", size, i);
    mpegts_packetizer_flush_bytes (packetizer, packetizer->map_offset);
    return FALSE;
  }

  GST_INFO ("have packetsize detected: %u bytes", packetizer->packet_size);

  if (packetizer->packet_size == MPEGTS_M2TS_PACKETSIZE &&
      packetizer->map_offset >= 4)
    packetizer->map_offset -= 4;

  return TRUE;
}

static gboolean
mpegts_packetizer_sync (MpegTSPacketizer2 * packetizer)
{
  gboolean found = FALSE;
  guint8 *data;
  guint packet_size;
  gsize size, sync_offset, i;

  packet_size = packetizer->packet_size;

  if (!mpegts_packetizer_map (packetizer, 3 * packet_size))
    return FALSE;

  size = packetizer->map_size - packetizer->map_offset;
  data = packetizer->map_data + packetizer->map_offset;

  if (packet_size == MPEGTS_M2TS_PACKETSIZE)
    sync_offset = 4;
  else
    sync_offset = 0;

  for (i = sync_offset; i + 2 * packet_size < size; i++) {
    if (data[i] == PACKET_SYNC_BYTE &&
        data[i + packet_size] == PACKET_SYNC_BYTE &&
        data[i + 2 * packet_size] == PACKET_SYNC_BYTE) {
      found = TRUE;
      break;
    }
  }

  packetizer->map_offset += i - sync_offset;

  if (!found)
    mpegts_packetizer_flush_bytes (packetizer, packetizer->map_offset);

  return found;
}

MpegTSPacketizerPacketReturn
mpegts_packetizer_next_packet (MpegTSPacketizer2 * packetizer,
    MpegTSPacketizerPacket * packet)
{
  guint8 *packet_data;
  guint packet_size;
  gsize sync_offset;

  packet_size = packetizer->packet_size;
  if (G_UNLIKELY (!packet_size)) {
    if (!mpegts_try_discover_packet_size (packetizer))
      return PACKET_NEED_MORE;
    packet_size = packetizer->packet_size;
  }

  /* M2TS packets don't start with the sync byte, all other variants do */
  if (packet_size == MPEGTS_M2TS_PACKETSIZE)
    sync_offset = 4;
  else
    sync_offset = 0;

  while (1) {
    if (packetizer->need_sync) {
      if (!mpegts_packetizer_sync (packetizer))
        return PACKET_NEED_MORE;
      packetizer->need_sync = FALSE;
    }

    if (!mpegts_packetizer_map (packetizer, packet_size))
      return PACKET_NEED_MORE;

    packet_data = &packetizer->map_data[packetizer->map_offset + sync_offset];

    /* Check sync byte */
    if (G_UNLIKELY (*packet_data != PACKET_SYNC_BYTE)) {
      GST_DEBUG ("lost sync");
      packetizer->need_sync = TRUE;
    } else {
      /* ALL mpeg-ts variants contain 188 bytes of data. Those with bigger
       * packet sizes contain either extra data (timesync, FEC, ..) either
       * before or after the data */
      packet->data_start = packet_data;
      packet->data_end = packet->data_start + 188;
      packet->offset = packetizer->offset;
      GST_LOG ("offset %" G_GUINT64_FORMAT, packet->offset);
      packetizer->offset += packet_size;
      GST_MEMDUMP ("data_start", packet->data_start, 16);

      return mpegts_packetizer_parse_packet (packetizer, packet);
    }
  }
}

MpegTSPacketizerPacketReturn
mpegts_packetizer_process_next_packet (MpegTSPacketizer2 * packetizer)
{
  MpegTSPacketizerPacket packet;
  MpegTSPacketizerPacketReturn ret;

  ret = mpegts_packetizer_next_packet (packetizer, &packet);
  if (ret != PACKET_NEED_MORE)
    mpegts_packetizer_clear_packet (packetizer, &packet);

  return ret;
}

void
mpegts_packetizer_clear_packet (MpegTSPacketizer2 * packetizer,
    MpegTSPacketizerPacket * packet)
{
  guint8 packet_size = packetizer->packet_size;

  if (packetizer->map_data) {
    packetizer->map_offset += packet_size;
    if (packetizer->map_size - packetizer->map_offset < packet_size)
      mpegts_packetizer_flush_bytes (packetizer, packetizer->map_offset);
  }
}

gboolean
mpegts_packetizer_has_packets (MpegTSPacketizer2 * packetizer)
{
  if (G_UNLIKELY (!packetizer->packet_size)) {
    if (!mpegts_try_discover_packet_size (packetizer))
      return FALSE;
  }
  return gst_adapter_available (packetizer->adapter) >= packetizer->packet_size;
}

/*
 * Ideally it should just return a section if:
 * * The section is complete
 * * The section is valid (sanity checks for length for example)
 * * The section applies now (current_next_indicator)
 * * The section is an update or was never seen
 *
 * The section should be a new GstMpegtsSection:
 * * properly initialized
 * * With pid, table_id AND section_type set (move logic from mpegtsbase)
 * * With data copied into it (yes, minor overhead)
 *
 * In all other cases it should just return NULL
 *
 * If more than one section is available, the 'remaining' field will
 * be set to the beginning of a valid GList containing other sections.
 * */
GstMpegtsSection *
mpegts_packetizer_push_section (MpegTSPacketizer2 * packetizer,
    MpegTSPacketizerPacket * packet, GList ** remaining)
{
  GstMpegtsSection *section;
  GstMpegtsSection *res = NULL;
  MpegTSPacketizerStream *stream;
  gboolean long_packet;
  guint8 pointer = 0, table_id;
  guint16 subtable_extension;
  gsize to_read;
  guint section_length;
  /* data points to the current read location
   * data_start points to the beginning of the data to accumulate */
  guint8 *data, *data_start;
  guint8 packet_cc;
  GList *others = NULL;
  guint8 version_number, section_number, last_section_number;
  gboolean cc_discont = FALSE;

  data = packet->data;
  packet_cc = FLAGS_CONTINUITY_COUNTER (packet->scram_afc_cc);

  /* Get our filter */
  stream = packetizer->streams[packet->pid];
  if (G_UNLIKELY (stream == NULL)) {
    if (!packet->payload_unit_start_indicator) {
      /* Early exit (we need to start with a section start) */
      GST_DEBUG ("PID 0x%04x  waiting for section start", packet->pid);
      goto out;
    }
    stream = mpegts_packetizer_stream_new (packet->pid);
    packetizer->streams[packet->pid] = stream;
  }

  GST_MEMDUMP ("Full packet data", packet->data,
      packet->data_end - packet->data);

  /* This function is split into several parts:
   *
   * Pre checks (packet-wide). Determines where we go next
   * accumulate_data: store data and check if section is complete
   * section_start: handle beginning of a section, if needed loop back to
   *                accumulate_data
   *
   * The trigger that makes the loop stop and return is if:
   * 1) We do not have enough data for the current packet
   * 2) There is remaining data after a packet which is only made
   *    of stuffing bytes (0xff).
   *
   * Pre-loop checks, related to the whole incoming packet:
   *
   * If there is a CC-discont:
   *  If it is a PUSI, skip the pointer and handle section_start
   *  If not a PUSI, reset and return nothing
   * If there is not a CC-discont:
   *  If it is a PUSI
   *    If pointer, accumulate that data and check for complete section
   *    (loop)
   *  If it is not a PUSI
   *    Accumulate the expected data and check for complete section
   *    (loop)
   *
   **/

  if (packet->payload_unit_start_indicator)
    pointer = *data++;

  if (stream->continuity_counter == CONTINUITY_UNSET ||
      (stream->continuity_counter + 1) % 16 != packet_cc) {
    if (stream->continuity_counter != CONTINUITY_UNSET) {
      GST_WARNING ("PID 0x%04x section discontinuity (%d vs %d)", packet->pid,
          stream->continuity_counter, packet_cc);
      cc_discont = TRUE;
    }
    mpegts_packetizer_clear_section (stream);
    stream->continuity_counter = packet_cc;
    /* If not a PUSI, not much we can do */
    if (!packet->payload_unit_start_indicator) {
      GST_LOG ("PID 0x%04x continuity discont/unset and not PUSI, bailing out",
          packet->pid);
      goto out;
    }
    /* If PUSI, skip pointer data and carry on to section start */
    data += pointer;
    pointer = 0;
    GST_LOG ("discont, but PUSI, skipped %d bytes and doing section start",
        pointer);
    goto section_start;
  }

  if (packet->payload_unit_start_indicator && pointer == 0) {
    /* If the pointer is zero, we're guaranteed to be able to handle it */
    GST_LOG
        ("PID 0x%04x PUSI and pointer == 0, skipping straight to section_start parsing",
        packet->pid);
    mpegts_packetizer_clear_section (stream);
    stream->continuity_counter = packet_cc;
    goto section_start;
  }

  stream->continuity_counter = packet_cc;


  GST_LOG ("Accumulating data from beginning of packet");

  data_start = data;

accumulate_data:
  /* If not the beginning of a new section, accumulate what we have */
  stream->continuity_counter = packet_cc;
  to_read = MIN (stream->section_length - stream->section_offset,
      packet->data_end - data_start);
  memcpy (stream->section_data + stream->section_offset, data_start, to_read);
  stream->section_offset += to_read;
  /* Point data to after the data we accumulated */
  data = data_start + to_read;
  GST_DEBUG ("Appending data (need %d, have %d)", stream->section_length,
      stream->section_offset);

  /* Check if we have enough */
  if (stream->section_offset < stream->section_length) {
    GST_DEBUG ("PID 0x%04x, section not complete (Got %d, need %d)",
        stream->pid, stream->section_offset, stream->section_length);
    goto out;
  }

  /* Small sanity check. We should have collected *exactly* the right amount */
  if (G_UNLIKELY (stream->section_offset != stream->section_length))
    GST_WARNING ("PID 0x%04x Accumulated too much data (%d vs %d) !",
        stream->pid, stream->section_offset, stream->section_length);
  GST_DEBUG ("PID 0x%04x Section complete", stream->pid);

  if ((section = mpegts_packetizer_parse_section_header (packetizer, stream))) {
    if (res)
      others = g_list_append (others, section);
    else
      res = section;
  }

section_start:
  subtable_extension = 0;
  version_number = 0;
  last_section_number = 0;
  section_number = 0;
  table_id = 0;

  /* FIXME : We need at least 3 bytes (or 8 for long packets) with current algorithm :(
   * We might end up losing sections that start across two packets (srsl...) */
  if (data > packet->data_end - 3 || *data == 0xff) {
    /* flush stuffing bytes and leave */
    mpegts_packetizer_clear_section (stream);
    goto out;
  }

  /* We have more data to process ... */
  GST_DEBUG ("PID 0x%04x, More section present in packet (remaining bytes:%"
      G_GSIZE_FORMAT ")", stream->pid, (gsize) (packet->data_end - data));
  GST_MEMDUMP ("section_start", data, packet->data_end - data);
  data_start = data;
  /* Beginning of a new section */
  /*
   * section_syntax_indicator means that the header is of the following format:
   * * table_id (8bit)
   * * section_syntax_indicator (1bit) == 0
   * * reserved/private fields (3bit)
   * * section_length (12bit)
   * * data (of size section_length)
   * * NO CRC !
   */
  long_packet = data[1] & 0x80;

  /* Fast path for short packets */
  if (!long_packet) {
    /* We can create the section now (function will check for size) */
    GST_DEBUG ("Short packet");
    section_length = (GST_READ_UINT16_BE (data + 1) & 0xfff) + 3;
    /* Only do fast-path if we have enough byte */
    if (data + section_length <= packet->data_end) {
      if ((section =
              gst_mpegts_section_new (packet->pid, g_memdup2 (data,
                      section_length), section_length))) {
        GST_DEBUG ("PID 0x%04x Short section complete !", packet->pid);
        section->offset = packet->offset;
        if (res)
          others = g_list_append (others, section);
        else
          res = section;
      }
      /* Advance reader and potentially read another section */
      data += section_length;
      if (data < packet->data_end && *data != 0xff)
        goto section_start;
      /* If not, exit */
      goto out;
    }
    /* We don't have enough bytes to do short section shortcut */
  }

  /* Beginning of a new section, do as much pre-parsing as possible */
  /* table_id                        : 8  bit */
  table_id = *data++;

  /* section_syntax_indicator        : 1  bit
   * other_fields (reserved)         : 3  bit
   * section_length                  : 12 bit */
  section_length = (GST_READ_UINT16_BE (data) & 0x0FFF) + 3;
  data += 2;

  if (long_packet) {
    /* Do we have enough data for a long packet? */
    if (data > packet->data_end - 5)
      goto out;

    /* subtable_extension (always present, we are in a long section) */
    /* subtable extension              : 16 bit */
    subtable_extension = GST_READ_UINT16_BE (data);
    data += 2;

    /* reserved                      : 2  bit
     * version_number                : 5  bit
     * current_next_indicator        : 1  bit */
    /* Bail out now if current_next_indicator == 0 */
    if (G_UNLIKELY (!(*data & 0x01))) {
      GST_DEBUG
          ("PID 0x%04x table_id 0x%02x section does not apply (current_next_indicator == 0)",
          packet->pid, table_id);
      goto out;
    }

    version_number = *data++ >> 1 & 0x1f;
    /* section_number                : 8  bit */
    section_number = *data++;
    /* last_section_number                : 8  bit */
    last_section_number = *data++;
  } else {
    subtable_extension = 0;
    version_number = 0;
    section_number = 0;
    last_section_number = 0;
  }
  GST_DEBUG
      ("PID 0x%04x length:%d table_id:0x%02x subtable_extension:0x%04x version_number:%d section_number:%d(last:%d)",
      packet->pid, section_length, table_id, subtable_extension, version_number,
      section_number, last_section_number);

  to_read = MIN (section_length, packet->data_end - data_start);

  /* Check as early as possible whether we already saw this section
   * i.e. that we saw a subtable with:
   * * same subtable_extension (might be zero)
   * * same version_number
   * * same last_section_number
   * * same section_number was seen
   */
  if (!cc_discont && seen_section_before (stream, table_id, subtable_extension,
          version_number, section_number, last_section_number)) {
    GST_DEBUG
        ("PID 0x%04x Already processed table_id:0x%02x subtable_extension:0x%04x, version_number:%d, section_number:%d",
        packet->pid, table_id, subtable_extension, version_number,
        section_number);
    /* skip data and see if we have more sections after */
    data = data_start + to_read;
    if (data == packet->data_end || *data == 0xff)
      goto out;
    goto section_start;
  }
  if (G_UNLIKELY (section_number > last_section_number)) {
    GST_WARNING
        ("PID 0x%04x corrupted packet (section_number:%d > last_section_number:%d)",
        packet->pid, section_number, last_section_number);
    goto out;
  }


  /* Copy over already parsed values */
  stream->table_id = table_id;
  stream->section_length = section_length;
  stream->version_number = version_number;
  stream->subtable_extension = subtable_extension;
  stream->section_number = section_number;
  stream->last_section_number = last_section_number;
  stream->offset = packet->offset;

  /* Create enough room to store chunks of sections */
  stream->section_data = g_malloc (stream->section_length);
  stream->section_offset = 0;

  /* Finally, accumulate and check if we parsed enough */
  goto accumulate_data;

out:
  packet->data = data;
  *remaining = others;

  GST_DEBUG ("result: %p", res);

  return res;
}

static void
_init_local (void)
{
  GST_DEBUG_CATEGORY_INIT (mpegts_packetizer_debug, "mpegtspacketizer", 0,
      "MPEG transport stream parser");
}


static void
mpegts_packetizer_resync (MpegTSPCR * pcr, GstClockTime time,
    GstClockTime gstpcrtime, gboolean reset_skew)
{
  pcr->base_time = time;
  pcr->base_pcrtime = gstpcrtime;
  pcr->prev_out_time = GST_CLOCK_TIME_NONE;
  pcr->prev_send_diff = GST_CLOCK_TIME_NONE;
  if (reset_skew) {
    pcr->window_filling = TRUE;
    pcr->window_pos = 0;
    pcr->window_min = 0;
    pcr->window_size = 0;
    pcr->skew = 0;
  }
}


/* Code mostly copied from -good/gst/rtpmanager/rtpjitterbuffer.c */

/* For the clock skew we use a windowed low point averaging algorithm as can be
 * found in Fober, Orlarey and Letz, 2005, "Real Time Clock Skew Estimation
 * over Network Delays":
 * http://www.grame.fr/Ressources/pub/TR-050601.pdf
 * http://citeseerx.ist.psu.edu/viewdoc/summary?doi=10.1.1.102.1546
 *
 * The idea is that the jitter is composed of:
 *
 *  J = D + n
 *
 *   D   : a constant network delay.
 *   n   : random added noise. The noise is concentrated around 0
 *
 * In the receiver we can track the elapsed time at the sender with:
 *
 *  send_diff(i) = (Tsi - Ts0);
 *
 *   Tsi : The time at the sender at packet i
 *   Ts0 : The time at the sender at the first packet
 *
 * This is the difference between the RTP timestamp in the first received packet
 * and the current packet.
 *
 * At the receiver we have to deal with the jitter introduced by the network.
 *
 *  recv_diff(i) = (Tri - Tr0)
 *
 *   Tri : The time at the receiver at packet i
 *   Tr0 : The time at the receiver at the first packet
 *
 * Both of these values contain a jitter Ji, a jitter for packet i, so we can
 * write:
 *
 *  recv_diff(i) = (Cri + D + ni) - (Cr0 + D + n0))
 *
 *    Cri    : The time of the clock at the receiver for packet i
 *    D + ni : The jitter when receiving packet i
 *
 * We see that the network delay is irrelevant here as we can eliminate D:
 *
 *  recv_diff(i) = (Cri + ni) - (Cr0 + n0))
 *
 * The drift is now expressed as:
 *
 *  Drift(i) = recv_diff(i) - send_diff(i);
 *
 * We now keep the W latest values of Drift and find the minimum (this is the
 * one with the lowest network jitter and thus the one which is least affected
 * by it). We average this lowest value to smooth out the resulting network skew.
 *
 * Both the window and the weighting used for averaging influence the accuracy
 * of the drift estimation. Finding the correct parameters turns out to be a
 * compromise between accuracy and inertia.
 *
 * We use a 2 second window or up to 512 data points, which is statistically big
 * enough to catch spikes (FIXME, detect spikes).
 * We also use a rather large weighting factor (125) to smoothly adapt. During
 * startup, when filling the window, we use a parabolic weighting factor, the
 * more the window is filled, the faster we move to the detected possible skew.
 *
 * Returns: @time adjusted with the clock skew.
 */
static GstClockTime
calculate_skew (MpegTSPacketizer2 * packetizer,
    MpegTSPCR * pcr, guint64 pcrtime, GstClockTime time)
{
  guint64 send_diff, recv_diff;
  gint64 delta;
  gint64 old;
  gint pos, i;
  GstClockTime gstpcrtime, out_time;
#ifndef GST_DISABLE_GST_DEBUG
  guint64 slope;
#endif

  gstpcrtime = PCRTIME_TO_GSTTIME (pcrtime) + pcr->pcroffset;

  /* first time, lock on to time and gstpcrtime */
  if (G_UNLIKELY (!GST_CLOCK_TIME_IS_VALID (pcr->base_time))) {
    pcr->base_time = time;
    pcr->prev_out_time = GST_CLOCK_TIME_NONE;
    GST_DEBUG ("Taking new base time %" GST_TIME_FORMAT, GST_TIME_ARGS (time));
  }

  if (G_UNLIKELY (!GST_CLOCK_TIME_IS_VALID (pcr->base_pcrtime))) {
    pcr->base_pcrtime = gstpcrtime;
    pcr->prev_send_diff = -1;
    GST_DEBUG ("Taking new base pcrtime %" GST_TIME_FORMAT,
        GST_TIME_ARGS (gstpcrtime));
  }

  /* Handle PCR wraparound and resets */
  if (GST_CLOCK_TIME_IS_VALID (pcr->last_pcrtime) &&
      gstpcrtime < pcr->last_pcrtime) {
    if (pcr->last_pcrtime - gstpcrtime > PCR_GST_MAX_VALUE / 2) {
      /* PCR wraparound */
      GST_DEBUG ("PCR wrap");
      pcr->pcroffset += PCR_GST_MAX_VALUE;
      gstpcrtime = PCRTIME_TO_GSTTIME (pcrtime) + pcr->pcroffset;
      send_diff = gstpcrtime - pcr->base_pcrtime;
    } else if (GST_CLOCK_TIME_IS_VALID (time)
        && pcr->last_pcrtime - gstpcrtime > 15 * GST_SECOND) {
      /* Time jumped backward by > 15 seconds, and we have a timestamp
       * to use to close the discont. Assume a reset */
      GST_DEBUG ("PCR reset");
      /* Calculate PCR we would have expected for the given input time,
       * essentially applying the reverse correction process
       *
       * We want to find the PCR offset to apply
       *   pcroffset = (corrected) gstpcrtime - (received) gstpcrtime
       *
       * send_diff = (corrected) gstpcrtime - pcr->base_pcrtime
       * recv_diff = time - pcr->base_time
       * out_time = pcr->base_time + send_diff
       *
       * We are assuming that send_diff == recv_diff
       *   (corrected) gstpcrtime - pcr->base_pcrtime = time - pcr->base_time
       * Giving us:
       *   (corrected) gstpcrtime = time - pcr->base_time + pcr->base_pcrtime
       *
       * And therefore:
       *   pcroffset = time - pcr->base_time + pcr->base_pcrtime - (received) gstpcrtime
       **/
      pcr->pcroffset += time - pcr->base_time + pcr->base_pcrtime - gstpcrtime;
      gstpcrtime = PCRTIME_TO_GSTTIME (pcrtime) + pcr->pcroffset;
      send_diff = gstpcrtime - pcr->base_pcrtime;
      GST_DEBUG ("Introduced offset is now %" GST_TIME_FORMAT
          " corrected pcr time %" GST_TIME_FORMAT,
          GST_TIME_ARGS (pcr->pcroffset), GST_TIME_ARGS (gstpcrtime));
    } else {
      /* Small jumps backward, assume some arrival jitter and skip it */
      send_diff = 0;

      /* The following code are the different ways we deal with small-ish
       * jitter, ranging in severity from "can be ignored" to "this needs a full
       * resync" */

      if (time == pcr->base_time) {
        /* If this comes from a non-fully-timestamped source (i.e. adaptive
         * streams), then cope with the fact that some producers generate utter
         * PCR garbage on fragment ends.
         *
         * We detect this comes from a non-fully-timestamped source by the fact
         * that the buffer time never changes */
        GST_DEBUG ("Ignoring PCR resets on non-fully timestamped stream");
      } else if (pcr->last_pcrtime - gstpcrtime < GST_SECOND) {
        GST_WARNING
            ("(small) backward timestamps at server or no buffer timestamps. Ignoring.");
        /* This will trigger the no_skew logic before but leave other state
         * intact */
        time = GST_CLOCK_TIME_NONE;
      } else {
        /* A bigger backward step than packet out-of-order can account for. Reset base PCR time
         * to be resynched the next time we see a PCR */
        GST_WARNING
            ("backward timestamps at server or no buffer timestamps. Resync base PCR");
        pcr->base_pcrtime = GST_CLOCK_TIME_NONE;
      }
    }
  } else
    send_diff = gstpcrtime - pcr->base_pcrtime;

  GST_DEBUG ("gstpcr %" GST_TIME_FORMAT ", buftime %" GST_TIME_FORMAT
      ", base %" GST_TIME_FORMAT ", send_diff %" GST_TIME_FORMAT,
      GST_TIME_ARGS (gstpcrtime), GST_TIME_ARGS (time),
      GST_TIME_ARGS (pcr->base_pcrtime), GST_TIME_ARGS (send_diff));

  /* keep track of the last extended pcrtime */
  pcr->last_pcrtime = gstpcrtime;

  /* we don't have an arrival timestamp so we can't do skew detection. we
   * should still apply a timestamp based on RTP timestamp and base_time */
  if (!GST_CLOCK_TIME_IS_VALID (time)
      || !GST_CLOCK_TIME_IS_VALID (pcr->base_time))
    goto no_skew;

  /* elapsed time at receiver, includes the jitter */
  recv_diff = time - pcr->base_time;

  /* Ignore packets received at 100% the same time (i.e. from the same input buffer) */
  if (G_UNLIKELY (time == pcr->prev_in_time
          && GST_CLOCK_TIME_IS_VALID (pcr->prev_in_time)))
    goto no_skew;

  /* measure the diff */
  delta = ((gint64) recv_diff) - ((gint64) send_diff);

#ifndef GST_DISABLE_GST_DEBUG
  /* measure the slope, this gives a rought estimate between the sender speed
   * and the receiver speed. This should be approximately 8, higher values
   * indicate a burst (especially when the connection starts) */
  slope = recv_diff > 0 ? (send_diff * 8) / recv_diff : 8;
#endif

  GST_DEBUG ("time %" GST_TIME_FORMAT ", base %" GST_TIME_FORMAT
      ", recv_diff %" GST_TIME_FORMAT ", slope %" G_GUINT64_FORMAT,
      GST_TIME_ARGS (time), GST_TIME_ARGS (pcr->base_time),
      GST_TIME_ARGS (recv_diff), slope);

  /* if the difference between the sender timeline and the receiver timeline
   * changed too quickly we have to resync because the server likely restarted
   * its timestamps. */
  if (ABS (delta - pcr->skew) > packetizer->pcr_discont_threshold) {
    GST_WARNING ("delta - skew: %" GST_STIME_FORMAT " too big, reset skew",
        GST_STIME_ARGS (delta - pcr->skew));
    mpegts_packetizer_resync (pcr, time, gstpcrtime, TRUE);
    send_diff = 0;
    delta = 0;
  }

  pos = pcr->window_pos;

  if (G_UNLIKELY (pcr->window_filling)) {
    /* we are filling the window */
    GST_DEBUG ("filling %d, delta %" G_GINT64_FORMAT, pos, delta);
    pcr->window[pos++] = delta;
    /* calc the min delta we observed */
    if (G_UNLIKELY (pos == 1 || delta < pcr->window_min))
      pcr->window_min = delta;

    if (G_UNLIKELY (send_diff >= MAX_TIME || pos >= MAX_WINDOW)) {
      pcr->window_size = pos;

      /* window filled */
      GST_DEBUG ("min %" G_GINT64_FORMAT, pcr->window_min);

      /* the skew is now the min */
      pcr->skew = pcr->window_min;
      pcr->window_filling = FALSE;
    } else {
      gint perc_time, perc_window, perc;

      /* figure out how much we filled the window, this depends on the amount of
       * time we have or the max number of points we keep. */
      perc_time = send_diff * 100 / MAX_TIME;
      perc_window = pos * 100 / MAX_WINDOW;
      perc = MAX (perc_time, perc_window);

      /* make a parabolic function, the closer we get to the MAX, the more value
       * we give to the scaling factor of the new value */
      perc = perc * perc;

      /* quickly go to the min value when we are filling up, slowly when we are
       * just starting because we're not sure it's a good value yet. */
      pcr->skew =
          (perc * pcr->window_min + ((10000 - perc) * pcr->skew)) / 10000;
      pcr->window_size = pos + 1;
    }
  } else {
    /* pick old value and store new value. We keep the previous value in order
     * to quickly check if the min of the window changed */
    old = pcr->window[pos];
    pcr->window[pos++] = delta;

    if (G_UNLIKELY (delta <= pcr->window_min)) {
      /* if the new value we inserted is smaller or equal to the current min,
       * it becomes the new min */
      pcr->window_min = delta;
    } else if (G_UNLIKELY (old == pcr->window_min)) {
      gint64 min = G_MAXINT64;

      /* if we removed the old min, we have to find a new min */
      for (i = 0; i < pcr->window_size; i++) {
        /* we found another value equal to the old min, we can stop searching now */
        if (pcr->window[i] == old) {
          min = old;
          break;
        }
        if (pcr->window[i] < min)
          min = pcr->window[i];
      }
      pcr->window_min = min;
    }
    /* average the min values */
    pcr->skew = (pcr->window_min + (124 * pcr->skew)) / 125;
    GST_DEBUG ("delta %" G_GINT64_FORMAT ", new min: %" G_GINT64_FORMAT,
        delta, pcr->window_min);
  }
  /* wrap around in the window */
  if (G_UNLIKELY (pos >= pcr->window_size))
    pos = 0;

  pcr->window_pos = pos;

no_skew:
  /* the output time is defined as the base timestamp plus the PCR time
   * adjusted for the clock skew .*/
  if (pcr->base_time != -1) {
    out_time = pcr->base_time + send_diff;
    /* skew can be negative and we don't want to make invalid timestamps */
    if (pcr->skew < 0 && out_time < -pcr->skew) {
      out_time = 0;
    } else {
      out_time += pcr->skew;
    }
    /* check if timestamps are not going backwards, we can only check this if we
     * have a previous out time and a previous send_diff */
    if (G_LIKELY (pcr->prev_out_time != -1 && pcr->prev_send_diff != -1)) {
      /* now check for backwards timestamps */
      if (G_UNLIKELY (
              /* if the server timestamps went up and the out_time backwards */
              (send_diff > pcr->prev_send_diff
                  && out_time < pcr->prev_out_time) ||
              /* if the server timestamps went backwards and the out_time forwards */
              (send_diff < pcr->prev_send_diff
                  && out_time > pcr->prev_out_time) ||
              /* if the server timestamps did not change */
              send_diff == pcr->prev_send_diff)) {
        GST_DEBUG ("backwards timestamps, using previous time");
        out_time = GSTTIME_TO_MPEGTIME (out_time);
      }
    }
  } else {
    /* We simply use the pcrtime without applying any skew compensation */
    out_time = time;
  }

  pcr->prev_out_time = out_time;
  pcr->prev_in_time = time;
  pcr->prev_send_diff = send_diff;

  GST_DEBUG ("skew %" G_GINT64_FORMAT ", out %" GST_TIME_FORMAT,
      pcr->skew, GST_TIME_ARGS (out_time));

  return out_time;
}

static void
_reevaluate_group_pcr_offset (MpegTSPCR * pcrtable, PCROffsetGroup * group)
{
  PCROffsetGroup *prev = NULL;
#ifndef GST_DISABLE_GST_DEBUG
  PCROffsetGroup *first = pcrtable->groups->data;
#endif
  PCROffsetCurrent *current = pcrtable->current;
  GList *tmp;

  /* Go over all ESTIMATED groups until the target group */
  for (tmp = pcrtable->groups; tmp; tmp = tmp->next) {
    PCROffsetGroup *cur = (PCROffsetGroup *) tmp->data;

    /* Skip groups that don't need re-evaluation */
    if (!(cur->flags & PCR_GROUP_FLAG_ESTIMATED)) {
      GST_DEBUG ("Skipping group %p pcr_offset (currently %" GST_TIME_FORMAT
          ")", cur, GST_TIME_ARGS (PCRTIME_TO_GSTTIME (cur->pcr_offset)));
      prev = cur;
      continue;
    }

    /* This should not happen ! The first group is *always* correct (zero) */
    if (G_UNLIKELY (prev == NULL)) {
      GST_ERROR ("First PCR Group was not estimated (bug). Setting to zero");
      cur->pcr_offset = 0;
      cur->flags &= ~PCR_GROUP_FLAG_ESTIMATED;
      return;
    }

    /* Finally do the estimation of this group's PCR offset based on the
     * previous group information */

    GST_DEBUG ("Re-evaluating group %p pcr_offset (currently %" GST_TIME_FORMAT
        ")", group, GST_TIME_ARGS (PCRTIME_TO_GSTTIME (cur->pcr_offset)));

    GST_DEBUG ("cur->first_pcr:%" GST_TIME_FORMAT " prev->first_pcr:%"
        GST_TIME_FORMAT, GST_TIME_ARGS (PCRTIME_TO_GSTTIME (cur->first_pcr)),
        GST_TIME_ARGS (PCRTIME_TO_GSTTIME (prev->first_pcr)));

    if (G_UNLIKELY (cur->first_pcr < prev->first_pcr)) {
      guint64 prevbr, lastbr;
      guint64 prevpcr;
      guint64 prevoffset, lastoffset;

      /* Take the previous group pcr_offset and figure out how much to add
       * to it for the current group */

      /* Right now we do a dumb bitrate estimation
       * estimate bitrate (prev - first) : bitrate from the start
       * estimate bitrate (prev) : bitrate of previous group
       * estimate bitrate (last - first) : bitrate from previous group
       *
       * We will use raw (non-corrected/non-absolute) PCR values in a first time
       * to detect wraparound/resets/gaps...
       *
       * We will use the corrected/absolute PCR values to calculate
       * bitrate and estimate the target group pcr_offset.
       * */

      /* If the current window estimator is over the previous group, used those
       * values as the latest (since they are more recent) */
      if (current->group == prev && current->pending[current->last].offset) {
        prevoffset =
            current->pending[current->last].offset + prev->first_offset;
        prevpcr = current->pending[current->last].pcr + prev->first_pcr;
        /* prevbr: bitrate(prev) */
        prevbr =
            gst_util_uint64_scale (PCR_SECOND,
            current->pending[current->last].offset,
            current->pending[current->last].pcr);
        GST_DEBUG ("Previous group bitrate (%" G_GUINT64_FORMAT " / %"
            GST_TIME_FORMAT ") : %" G_GUINT64_FORMAT,
            current->pending[current->last].offset,
            GST_TIME_ARGS (PCRTIME_TO_GSTTIME (current->pending[current->last].
                    pcr)), prevbr);
      } else if (prev->values[prev->last_value].offset) {
        prevoffset = prev->values[prev->last_value].offset + prev->first_offset;
        prevpcr = prev->values[prev->last_value].pcr + prev->first_pcr;
        /* prevbr: bitrate(prev) (FIXME : Cache) */
        prevbr =
            gst_util_uint64_scale (PCR_SECOND,
            prev->values[prev->last_value].offset,
            prev->values[prev->last_value].pcr);
        GST_DEBUG ("Previous group bitrate (%" G_GUINT64_FORMAT " / %"
            GST_TIME_FORMAT ") : %" G_GUINT64_FORMAT,
            prev->values[prev->last_value].offset,
            GST_TIME_ARGS (PCRTIME_TO_GSTTIME (prev->values[prev->last_value].
                    pcr)), prevbr);
      } else {
        GST_DEBUG ("Using overall bitrate");
        prevoffset = prev->values[prev->last_value].offset + prev->first_offset;
        prevpcr = prev->values[prev->last_value].pcr + prev->first_pcr;
        prevbr = gst_util_uint64_scale (PCR_SECOND,
            prev->first_offset, prev->pcr_offset);
      }
      lastoffset = cur->values[cur->last_value].offset + cur->first_offset;

      GST_DEBUG ("Offset first:%" G_GUINT64_FORMAT " prev:%" G_GUINT64_FORMAT
          " cur:%" G_GUINT64_FORMAT, first->first_offset, prevoffset,
          lastoffset);
      GST_DEBUG ("PCR first:%" GST_TIME_FORMAT " prev:%" GST_TIME_FORMAT
          " cur:%" GST_TIME_FORMAT,
          GST_TIME_ARGS (PCRTIME_TO_GSTTIME (first->first_pcr)),
          GST_TIME_ARGS (PCRTIME_TO_GSTTIME (prevpcr)),
          GST_TIME_ARGS (PCRTIME_TO_GSTTIME (cur->values[cur->last_value].pcr +
                  cur->first_pcr)));

      if (prevpcr - cur->first_pcr > (PCR_MAX_VALUE * 9 / 10)) {
        gfloat diffprev;
        guint64 guess_offset;

        /* Let's assume there is a PCR wraparound between the previous and current
         * group.
         * [ prev ]... PCR_MAX | 0 ...[ current ]
         * The estimated pcr_offset would therefore be:
         * current.first + (PCR_MAX_VALUE - prev.first)
         *
         * 1) Check if bitrate(prev) would be consistent with bitrate (cur - prev)
         */
        guess_offset = PCR_MAX_VALUE - prev->first_pcr + cur->first_pcr;
        lastbr = gst_util_uint64_scale (PCR_SECOND, lastoffset - prevoffset,
            guess_offset + cur->values[cur->last_value].pcr - (prevpcr -
                prev->first_pcr));
        GST_DEBUG ("Wraparound prev-cur (guess_offset:%" GST_TIME_FORMAT
            ") bitrate:%" G_GUINT64_FORMAT,
            GST_TIME_ARGS (PCRTIME_TO_GSTTIME (guess_offset)), lastbr);
        diffprev = (float) 100.0 *(ABSDIFF (prevbr, lastbr)) / (float) prevbr;
        GST_DEBUG ("Difference with previous bitrate:%f", diffprev);
        if (diffprev < 10.0) {
          GST_DEBUG ("Difference < 10.0, Setting pcr_offset to %"
              G_GUINT64_FORMAT, guess_offset);
          cur->pcr_offset = guess_offset;
          if (diffprev < 1.0) {
            GST_DEBUG ("Difference < 1.0, Removing ESTIMATED flags");
            cur->flags &= ~PCR_GROUP_FLAG_ESTIMATED;
          }
        }
        /* Indicate the the previous group is before a wrapover */
        prev->flags |= PCR_GROUP_FLAG_WRAPOVER;
      } else {
        guint64 resetprev;
        /* Let's assume there was a PCR reset between the previous and current
         * group
         * [ prev ] ... x | x - reset ... [ current ]
         *
         * The estimated pcr_offset would then be
         * = current.first - (x - reset) + (x - prev.first) + 100ms (for safety)
         * = current.first + reset - prev.first + 100ms (for safety)
         */
        /* In order to calculate the reset, we estimate what the PCR would have
         * been by using prevbr */
        /* FIXME : Which bitrate should we use ???  */
        GST_DEBUG ("Using prevbr:%" G_GUINT64_FORMAT " and taking offsetdiff:%"
            G_GUINT64_FORMAT, prevbr, cur->first_offset - prev->first_offset);
        resetprev =
            gst_util_uint64_scale (PCR_SECOND,
            cur->first_offset - prev->first_offset, prevbr);
        GST_DEBUG ("Estimated full PCR for offset %" G_GUINT64_FORMAT
            ", using prevbr:%"
            GST_TIME_FORMAT, cur->first_offset,
            GST_TIME_ARGS (PCRTIME_TO_GSTTIME (resetprev)));
        cur->pcr_offset = prev->pcr_offset + resetprev + 100 * PCR_MSECOND;
        GST_DEBUG ("Adjusted group PCR_offset to %" GST_TIME_FORMAT,
            GST_TIME_ARGS (PCRTIME_TO_GSTTIME (cur->pcr_offset)));
        /* Indicate the the previous group is before a reset */
        prev->flags |= PCR_GROUP_FLAG_RESET;
      }
    } else {
      /* FIXME : Detect gaps if bitrate difference is really too big ? */
      cur->pcr_offset = prev->pcr_offset + cur->first_pcr - prev->first_pcr;
      GST_DEBUG ("Assuming there is no gap, setting pcr_offset to %"
          GST_TIME_FORMAT,
          GST_TIME_ARGS (PCRTIME_TO_GSTTIME (cur->pcr_offset)));
      /* Remove the reset and wrapover flag (if it was previously there) */
      prev->flags &= ~PCR_GROUP_FLAG_RESET;
      prev->flags &= ~PCR_GROUP_FLAG_WRAPOVER;
    }


    /* Remember prev for the next group evaluation */
    prev = cur;
  }
}

static PCROffsetGroup *
_new_group (guint64 pcr, guint64 offset, guint64 pcr_offset, guint flags)
{
  PCROffsetGroup *group = g_new0 (PCROffsetGroup, 1);

  GST_DEBUG ("Input PCR %" GST_TIME_FORMAT " offset:%" G_GUINT64_FORMAT
      " pcr_offset:%" G_GUINT64_FORMAT " flags:%d",
      GST_TIME_ARGS (PCRTIME_TO_GSTTIME (pcr)), offset, pcr_offset, flags);

  group->flags = flags;
  group->values = g_new0 (PCROffset, DEFAULT_ALLOCATED_OFFSET);
  /* The first pcr/offset diff is always 0/0 */
  group->values[0].pcr = group->values[0].offset = 0;
  group->nb_allocated = DEFAULT_ALLOCATED_OFFSET;

  /* Store the full values */
  group->first_pcr = pcr;
  group->first_offset = offset;
  group->pcr_offset = pcr_offset;

  GST_DEBUG ("Created group starting with pcr:%" GST_TIME_FORMAT " offset:%"
      G_GUINT64_FORMAT " pcr_offset:%" GST_TIME_FORMAT,
      GST_TIME_ARGS (PCRTIME_TO_GSTTIME (group->first_pcr)),
      group->first_offset,
      GST_TIME_ARGS (PCRTIME_TO_GSTTIME (group->pcr_offset)));

  return group;
}

static void
_insert_group_after (MpegTSPCR * pcrtable, PCROffsetGroup * group,
    PCROffsetGroup * prev)
{
  if (prev == NULL) {
    /* First group */
    pcrtable->groups = g_list_prepend (pcrtable->groups, group);
  } else {
    GList *tmp, *toinsert, *prevlist = NULL, *nextlist = NULL;
    /* Insert before next and prev */
    for (tmp = pcrtable->groups; tmp; tmp = tmp->next) {
      if (tmp->data == prev) {
        prevlist = tmp;
        nextlist = tmp->next;
        break;
      }
    }
    if (!prevlist) {
      /* The non NULL prev given isn't in the list */
      GST_WARNING ("Request to insert before a group which isn't in the list");
      pcrtable->groups = g_list_prepend (pcrtable->groups, group);
    } else {
      toinsert = g_list_append (NULL, group);
      toinsert->next = nextlist;
      toinsert->prev = prevlist;
      prevlist->next = toinsert;
      if (nextlist)
        nextlist->prev = toinsert;
    }
  }
}

static void
_use_group (MpegTSPCR * pcrtable, PCROffsetGroup * group)
{
  PCROffsetCurrent *current = pcrtable->current;

  memset (current, 0, sizeof (PCROffsetCurrent));
  current->group = group;
  current->pending[0] = group->values[group->last_value];
  current->last_value = current->pending[0];
  current->write = 1;
  current->prev = group->values[group->last_value];
  current->first_pcr = group->first_pcr;
  current->first_offset = group->first_offset;
}

/* Create a new group with the specified values after prev
 * Set current to that new group */
static void
_set_current_group (MpegTSPCR * pcrtable,
    PCROffsetGroup * prev, guint64 pcr, guint64 offset, gboolean contiguous)
{
  PCROffsetGroup *group;
  guint flags = 0;
  guint64 pcr_offset = 0;

  /* Handle wraparound/gap (only if contiguous with previous group) */
  if (contiguous) {
    guint64 lastpcr = prev->first_pcr + prev->values[prev->last_value].pcr;

    /* Set CLOSED flag on previous group and remember pcr_offset */
    prev->flags |= PCR_GROUP_FLAG_CLOSED;
    pcr_offset = prev->pcr_offset;

    /* Wraparound ? */
    if (lastpcr > pcr) {
      /* In offset-mode, a PCR wraparound is only actually consistent if
       * we have a very high confidence (99% right now, might need to change
       * later) */
      if (lastpcr - pcr > (PCR_MAX_VALUE * 99 / 100)) {
        GST_WARNING ("WRAPAROUND detected. diff %" GST_TIME_FORMAT,
            GST_TIME_ARGS (PCRTIME_TO_GSTTIME (lastpcr - pcr)));
        /* The previous group closed at PCR_MAX_VALUE */
        pcr_offset += PCR_MAX_VALUE - prev->first_pcr + pcr;
      } else {
        GST_WARNING ("RESET detected. diff %" GST_TIME_FORMAT,
            GST_TIME_ARGS (PCRTIME_TO_GSTTIME (lastpcr - pcr)));
        /* The previous group closed at the raw last_pcr diff (+100ms for safety) */
        pcr_offset += prev->values[prev->last_value].pcr + 100 * PCR_MSECOND;
      }
    } else if (lastpcr < pcr - 500 * PCR_MSECOND) {
      GST_WARNING ("GAP detected. diff %" GST_TIME_FORMAT,
          GST_TIME_ARGS (PCRTIME_TO_GSTTIME (pcr - lastpcr)));
      /* The previous group closed at the raw last_pcr diff (+500ms for safety) */
      pcr_offset += prev->values[prev->last_value].pcr + 500 * PCR_MSECOND;
    } else
      /* Normal continuation (contiguous in time) */
      pcr_offset += pcr - prev->first_pcr;

  } else if (prev != NULL)
    /* If we are not contiguous and it's not the first group, the pcr_offset
     * will be estimated */
    flags = PCR_GROUP_FLAG_ESTIMATED;

  group = _new_group (pcr, offset, pcr_offset, flags);
  _use_group (pcrtable, group);
  _insert_group_after (pcrtable, group, prev);
  if (!contiguous)
    _reevaluate_group_pcr_offset (pcrtable, group);
}

static inline void
_append_group_values (PCROffsetGroup * group, PCROffset pcroffset)
{
  /* Only append if new values */
  if (group->values[group->last_value].offset == pcroffset.offset &&
      group->values[group->last_value].pcr == pcroffset.pcr) {
    GST_DEBUG ("Same values, ignoring");
  } else {
    group->last_value++;
    /* Resize values if needed */
    if (G_UNLIKELY (group->nb_allocated == group->last_value)) {
      group->nb_allocated += DEFAULT_ALLOCATED_OFFSET;
      group->values =
          g_realloc (group->values, group->nb_allocated * sizeof (PCROffset));
    }
    group->values[group->last_value] = pcroffset;
  }

  GST_DEBUG ("First PCR:%" GST_TIME_FORMAT " offset:%" G_GUINT64_FORMAT
      " PCR_offset:%" GST_TIME_FORMAT,
      GST_TIME_ARGS (PCRTIME_TO_GSTTIME (group->first_pcr)),
      group->first_offset,
      GST_TIME_ARGS (PCRTIME_TO_GSTTIME (group->pcr_offset)));
  GST_DEBUG ("Last PCR: +%" GST_TIME_FORMAT " offset: +%" G_GUINT64_FORMAT,
      GST_TIME_ARGS (PCRTIME_TO_GSTTIME (pcroffset.pcr)), pcroffset.offset);
}

/* Move last values from current (if any) to the current group
 * and reset current.
 * Note: This does not set the CLOSED flag (since we have no next
 * contiguous group) */
static void
_close_current_group (MpegTSPCR * pcrtable)
{
  PCROffsetCurrent *current = pcrtable->current;
  PCROffsetGroup *group = current->group;

  if (group == NULL)
    return;
  GST_DEBUG ("Closing group and resetting current");

  /* Store last values */
  _append_group_values (group, current->pending[current->last]);
  memset (current, 0, sizeof (PCROffsetCurrent));
  /* And re-evaluate all groups */
}

static void
record_pcr (MpegTSPacketizer2 * packetizer, MpegTSPCR * pcrtable,
    guint64 pcr, guint64 offset)
{
  PCROffsetCurrent *current = pcrtable->current;
  gint64 corpcr, coroffset;

  packetizer->nb_seen_offsets += 1;

  pcrtable->last_pcrtime = PCRTIME_TO_GSTTIME (pcr);
  /* FIXME : Invert logic later (probability is higher that we have a
   * current estimator) */

  /* Check for current */
  if (G_UNLIKELY (current->group == NULL)) {
    PCROffsetGroup *prev = NULL;
    GList *tmp;
    /* No current estimator. This happens for the initial value, or after
     * discont and flushes. Figure out where we need to record this position.
     *
     * Possible choices:
     * 1) No groups at all:
     *    Create a new group with pcr/offset
     *    Initialize current to that group
     * 2) Entirely within an existing group
     *    bail out (FIXME: Make this detection faster)
     * 3) Not in any group
     *    Create a new group with pcr/offset at the right position
     *    Initialize current to that group
     */
    GST_DEBUG ("No current window estimator, Checking for group to use");
    for (tmp = pcrtable->groups; tmp; tmp = tmp->next) {
      PCROffsetGroup *group = (PCROffsetGroup *) tmp->data;

      GST_DEBUG ("First PCR:%" GST_TIME_FORMAT " offset:%" G_GUINT64_FORMAT
          " PCR_offset:%" GST_TIME_FORMAT,
          GST_TIME_ARGS (PCRTIME_TO_GSTTIME (group->first_pcr)),
          group->first_offset,
          GST_TIME_ARGS (PCRTIME_TO_GSTTIME (group->pcr_offset)));
      GST_DEBUG ("Last PCR: +%" GST_TIME_FORMAT " offset: +%" G_GUINT64_FORMAT,
          GST_TIME_ARGS (PCRTIME_TO_GSTTIME (group->values[group->last_value].
                  pcr)), group->values[group->last_value].offset);
      /* Check if before group */
      if (offset < group->first_offset) {
        GST_DEBUG ("offset is before that group");
        break;
      }
      /* Check if within group */
      if (offset <=
          (group->values[group->last_value].offset + group->first_offset)) {
        GST_DEBUG ("Already observed PCR offset %" G_GUINT64_FORMAT, offset);
        return;
      }
      /* Check if just after group (i.e. continuation of it) */
      if (!(group->flags & PCR_GROUP_FLAG_CLOSED) &&
          pcr - group->first_pcr - group->values[group->last_value].pcr <=
          100 * PCR_MSECOND) {
        GST_DEBUG ("Continuation of existing group");
        _use_group (pcrtable, group);
        return;
      }
      /* Else after group */
      prev = group;
    }
    _set_current_group (pcrtable, prev, pcr, offset, FALSE);
    return;
  }

  corpcr = pcr - current->first_pcr;
  coroffset = offset - current->first_offset;

  /* FIXME : Detect if we've gone into the next group !
   * FIXME : Close group when that happens */
  GST_DEBUG ("first:%d, last:%d, write:%d", current->first, current->last,
      current->write);
  GST_DEBUG ("First PCR:%" GST_TIME_FORMAT " offset:%" G_GUINT64_FORMAT,
      GST_TIME_ARGS (PCRTIME_TO_GSTTIME (current->first_pcr)),
      current->first_offset);
  GST_DEBUG ("Last PCR: +%" GST_TIME_FORMAT " offset: +%" G_GUINT64_FORMAT,
      GST_TIME_ARGS (PCRTIME_TO_GSTTIME (current->pending[current->last].pcr)),
      current->pending[current->last].offset);
  GST_DEBUG ("To add (corrected) PCR:%" GST_TIME_FORMAT " offset:%"
      G_GINT64_FORMAT, GST_TIME_ARGS (PCRTIME_TO_GSTTIME (corpcr)), coroffset);

  /* Do we need to close the current group ? */
  /* Check for wrapover/discont */
  if (G_UNLIKELY (corpcr < current->pending[current->last].pcr)) {
    /* FIXME : ignore very small deltas (< 500ms ?) which are most likely
     * stray values */
    GST_DEBUG
        ("PCR smaller than previously observed one, handling discont/wrapover");
    /* Take values from current and put them in the current group (closing it) */
    /* Create new group with new pcr/offset just after the current group
     * and mark it as a wrapover */
    /* Initialize current to that group with new values */
    _append_group_values (current->group, current->pending[current->last]);
    _set_current_group (pcrtable, current->group, pcr, offset, TRUE);
    return;
  }
  /* If PCR diff is greater than 500ms, create new group */
  if (G_UNLIKELY (corpcr - current->pending[current->last].pcr >
          500 * PCR_MSECOND)) {
    GST_DEBUG ("New PCR more than 500ms away, handling discont");
    /* Take values from current and put them in the current group (closing it) */
    /* Create new group with pcr/offset just after the current group
     * and mark it as a discont */
    /* Initialize current to that group with new values */
    _append_group_values (current->group, current->pending[current->last]);
    _set_current_group (pcrtable, current->group, pcr, offset, TRUE);
    return;
  }

  if (G_UNLIKELY (corpcr == current->last_value.pcr)) {
    GST_DEBUG ("Ignoring same PCR (stream is drunk)");
    return;
  }

  /* update current window */
  current->pending[current->write].pcr = corpcr;
  current->pending[current->write].offset = coroffset;
  current->last_value = current->pending[current->write];
  current->last = (current->last + 1) % PCR_BITRATE_NEEDED;
  current->write = (current->write + 1) % PCR_BITRATE_NEEDED;

  GST_DEBUG ("first:%d, last:%d, write:%d", current->first, current->last,
      current->write);
  GST_DEBUG ("First PCR:%" GST_TIME_FORMAT " offset:%" G_GUINT64_FORMAT,
      GST_TIME_ARGS (PCRTIME_TO_GSTTIME (current->first_pcr)),
      current->first_offset);
  GST_DEBUG ("Last PCR: +%" GST_TIME_FORMAT " offset: +%" G_GUINT64_FORMAT,
      GST_TIME_ARGS (PCRTIME_TO_GSTTIME (current->pending[current->last].pcr)),
      current->pending[current->last].offset);

  /* If we haven't stored enough values, bail out */
  if (current->write != current->first) {
    GST_DEBUG
        ("Not enough observations to calculate bitrate (first:%d, last:%d)",
        current->first, current->last);
    return;
  }

  /* If we are at least 1s away from reference value AND we have filled our
   * window, we can start comparing bitrates */
  if (current->pending[current->first].pcr - current->prev.pcr > PCR_SECOND) {
    /* Calculate window bitrate */
    current->cur_bitrate = gst_util_uint64_scale (PCR_SECOND,
        current->pending[current->last].offset -
        current->pending[current->first].offset,
        current->pending[current->last].pcr -
        current->pending[current->first].pcr);
    GST_DEBUG ("Current bitrate is now %" G_GUINT64_FORMAT,
        current->cur_bitrate);

    /* Calculate previous bitrate */
    current->prev_bitrate =
        gst_util_uint64_scale (PCR_SECOND,
        current->pending[current->first].offset - current->prev.offset,
        current->pending[current->first].pcr - current->prev.pcr);
    GST_DEBUG ("Previous group bitrate now %" G_GUINT64_FORMAT,
        current->prev_bitrate);

    /* FIXME : Better bitrate changes ? Currently 10% changes */
    if (ABSDIFF (current->cur_bitrate,
            current->prev_bitrate) * 10 > current->prev_bitrate) {
      GST_DEBUG ("Current bitrate changed by more than 10%% (old:%"
          G_GUINT64_FORMAT " new:%" G_GUINT64_FORMAT ")", current->prev_bitrate,
          current->cur_bitrate);
      /* If we detected a change in bitrate, this means that
       * d(first - prev) is a different bitrate than d(last - first).
       *
       * Two conclusions can be made:
       * 1) d(first - prev) is a complete bitrate "chain" (values between the
       *    reference value and first pending value have consistent bitrate).
       * 2) next values (from second pending value onwards) will no longer have
       *    the same bitrate.
       *
       * The question remains as to how long the new bitrate change is going to
       * last for (it might be short or longer term). For this we need to restart
       * bitrate estimation.
       *
       * * We move over first to the last value of group (a new chain ends and
       *   starts from there)
       * * We remember that last group value as our new window reference
       * * We restart our window filing from the last observed value
       *
       * Once our new window is filled we will end up in two different scenarios:
       * 1) Either the bitrate change was consistent, and therefore the bitrate
       *    will have remained constant over at least 2 window length
       * 2) The bitrate change was very short (1 window duration) and we will
       *    close that chain and restart again.
       * X) And of course if any discont/gaps/wrapover happen in the meantime they
       *    will also close the group.
       */
      _append_group_values (current->group, current->pending[current->first]);
      current->prev = current->pending[current->first];
      current->first = current->last;
      current->write = (current->first + 1) % PCR_BITRATE_NEEDED;
      return;
    }
  }

  /* Update read position */
  current->first = (current->first + 1) % PCR_BITRATE_NEEDED;
}


/* convert specified offset into stream time */
GstClockTime
mpegts_packetizer_offset_to_ts (MpegTSPacketizer2 * packetizer,
    guint64 offset, guint16 pid)
{
  PCROffsetGroup *last;
  MpegTSPCR *pcrtable;
  GList *tmp;
  GstClockTime res;
  guint64 lastpcr, lastoffset;

  GST_DEBUG ("offset %" G_GUINT64_FORMAT, offset);

  if (G_UNLIKELY (!packetizer->calculate_offset))
    return GST_CLOCK_TIME_NONE;

  if (G_UNLIKELY (packetizer->refoffset == -1))
    return GST_CLOCK_TIME_NONE;

  if (G_UNLIKELY (offset < packetizer->refoffset))
    return GST_CLOCK_TIME_NONE;

  PACKETIZER_GROUP_LOCK (packetizer);

  pcrtable = get_pcr_table (packetizer, pid);

  if (g_list_length (pcrtable->groups) < 1) {
    PACKETIZER_GROUP_UNLOCK (packetizer);
    GST_WARNING ("Not enough observations to return a duration estimate");
    return GST_CLOCK_TIME_NONE;
  }

  if (g_list_length (pcrtable->groups) > 1) {
    GST_LOG ("Using last group");

    /* FIXME : Refine this later to use neighbouring groups */
    tmp = g_list_last (pcrtable->groups);
    last = tmp->data;

    if (G_UNLIKELY (last->flags & PCR_GROUP_FLAG_ESTIMATED))
      _reevaluate_group_pcr_offset (pcrtable, last);

    /* lastpcr is the full value in PCR from the first first chunk of data */
    lastpcr = last->values[last->last_value].pcr + last->pcr_offset;
    /* lastoffset is the full offset from the first chunk of data */
    lastoffset =
        last->values[last->last_value].offset + last->first_offset -
        packetizer->refoffset;
  } else {
    PCROffsetCurrent *current = pcrtable->current;

    if (!current->group) {
      PACKETIZER_GROUP_UNLOCK (packetizer);
      GST_LOG ("No PCR yet");
      return GST_CLOCK_TIME_NONE;
    }
    /* If doing progressive read, use current */
    GST_LOG ("Using current group");
    lastpcr = current->group->pcr_offset + current->pending[current->last].pcr;
    lastoffset = current->first_offset + current->pending[current->last].offset;
  }
  GST_DEBUG ("lastpcr:%" GST_TIME_FORMAT " lastoffset:%" G_GUINT64_FORMAT
      " refoffset:%" G_GUINT64_FORMAT,
      GST_TIME_ARGS (PCRTIME_TO_GSTTIME (lastpcr)), lastoffset,
      packetizer->refoffset);

  /* Convert byte difference into time difference (and transformed from 27MHz to 1GHz) */
  res =
      PCRTIME_TO_GSTTIME (gst_util_uint64_scale (offset - packetizer->refoffset,
          lastpcr, lastoffset));

  PACKETIZER_GROUP_UNLOCK (packetizer);

  GST_DEBUG ("Returning timestamp %" GST_TIME_FORMAT " for offset %"
      G_GUINT64_FORMAT, GST_TIME_ARGS (res), offset);

  return res;
}

/* Input  : local PTS (in GHz units)
 * Return : Stream time (in GHz units) */
static GstClockTime
mpegts_packetizer_pts_to_ts_internal (MpegTSPacketizer2 * packetizer,
    GstClockTime pts, guint16 pcr_pid, gboolean check_diff)
{
  GstClockTime res = GST_CLOCK_TIME_NONE;
  MpegTSPCR *pcrtable;

  PACKETIZER_GROUP_LOCK (packetizer);
  pcrtable = get_pcr_table (packetizer, pcr_pid);

  if (!GST_CLOCK_TIME_IS_VALID (pcrtable->base_time) && pcr_pid == 0x1fff &&
      GST_CLOCK_TIME_IS_VALID (packetizer->last_in_time)) {
    pcrtable->base_time = packetizer->last_in_time;
    pcrtable->base_pcrtime = pts;
  }

  /* Use clock skew if present */
  if (packetizer->calculate_skew
      && GST_CLOCK_TIME_IS_VALID (pcrtable->base_time)) {
    GST_DEBUG ("pts %" GST_TIME_FORMAT " base_pcrtime:%" GST_TIME_FORMAT
        " base_time:%" GST_TIME_FORMAT " pcroffset:%" GST_TIME_FORMAT,
        GST_TIME_ARGS (pts),
        GST_TIME_ARGS (pcrtable->base_pcrtime),
        GST_TIME_ARGS (pcrtable->base_time),
        GST_TIME_ARGS (pcrtable->pcroffset));
    res = pts + pcrtable->pcroffset + packetizer->extra_shift;

    /* Don't return anything if we differ too much against last seen PCR */
    if (G_UNLIKELY (check_diff && pcr_pid != 0x1fff &&
            ABSDIFF (res, pcrtable->last_pcrtime) > 15 * GST_SECOND)) {
      res = GST_CLOCK_TIME_NONE;
    } else {
      GstClockTime tmp = pcrtable->base_time + pcrtable->skew;
      if (tmp + res >= pcrtable->base_pcrtime) {
        res += tmp - pcrtable->base_pcrtime;
      } else if (!check_diff || ABSDIFF (tmp + res + PCR_GST_MAX_VALUE,
              pcrtable->base_pcrtime) < PCR_GST_MAX_VALUE / 2) {
        /* Handle wrapover */
        res += tmp + PCR_GST_MAX_VALUE - pcrtable->base_pcrtime;
      } else {
        /* Fallback for values that differ way too much */
        res = GST_CLOCK_TIME_NONE;
      }
    }
  } else if (packetizer->calculate_offset && pcrtable->groups) {
    gint64 refpcr = G_MAXINT64, refpcroffset;
    PCROffsetGroup *group = pcrtable->current->group;

    /* Generic calculation:
     * Stream Time = PTS - first group PCR + group PCR_offset
     *
     * In case of wrapover:
     * Stream Time = PTS + MAX_PCR - first group PCR + group PCR_offset
     * (which we actually do by using first group PCR -= MAX_PCR in order
     *  to end up with the same calculation as for non-wrapover) */

    if (group) {
      /* If we have a current group the value is pretty much guaranteed */
      GST_DEBUG ("Using current First PCR:%" GST_TIME_FORMAT " offset:%"
          G_GUINT64_FORMAT " PCR_offset:%" GST_TIME_FORMAT,
          GST_TIME_ARGS (PCRTIME_TO_GSTTIME (group->first_pcr)),
          group->first_offset,
          GST_TIME_ARGS (PCRTIME_TO_GSTTIME (group->pcr_offset)));
      refpcr = group->first_pcr;
      refpcroffset = group->pcr_offset;
      if (pts < PCRTIME_TO_GSTTIME (refpcr)) {
        /* Only apply wrapover if we're certain it is, and avoid
         * returning bogus values if it's a PTS/DTS which is *just*
         * before the start of the current group
         */
        if (PCRTIME_TO_GSTTIME (refpcr) - pts > GST_SECOND) {
          pts += PCR_GST_MAX_VALUE;
        } else
          refpcr = G_MAXINT64;
      }
    } else {
      GList *tmp;
      /* Otherwise, find a suitable group */

      GST_DEBUG ("Find group for current offset %" G_GUINT64_FORMAT,
          packetizer->offset);

      for (tmp = pcrtable->groups; tmp; tmp = tmp->next) {
        PCROffsetGroup *tgroup = tmp->data;
        GST_DEBUG ("Trying First PCR:%" GST_TIME_FORMAT " offset:%"
            G_GUINT64_FORMAT " PCR_offset:%" GST_TIME_FORMAT,
            GST_TIME_ARGS (PCRTIME_TO_GSTTIME (tgroup->first_pcr)),
            tgroup->first_offset,
            GST_TIME_ARGS (PCRTIME_TO_GSTTIME (tgroup->pcr_offset)));
        /* Gone too far ? */
        if (tgroup->first_offset > packetizer->offset) {
          /* If there isn't a pending reset, use that value */
          if (group) {
            GST_DEBUG ("PTS is %" GST_TIME_FORMAT " into group",
                GST_TIME_ARGS (pts - PCRTIME_TO_GSTTIME (group->first_pcr)));
          }
          break;
        }
        group = tgroup;
        /* In that group ? */
        if (group->first_offset + group->values[group->last_value].offset >
            packetizer->offset) {
          GST_DEBUG ("PTS is %" GST_TIME_FORMAT " into group",
              GST_TIME_ARGS (pts - PCRTIME_TO_GSTTIME (group->first_pcr)));
          break;
        }
      }
      if (group && !(group->flags & PCR_GROUP_FLAG_RESET)) {
        GST_DEBUG ("Using group !");
        refpcr = group->first_pcr;
        refpcroffset = group->pcr_offset;
        if (pts < PCRTIME_TO_GSTTIME (refpcr)) {
          if (PCRTIME_TO_GSTTIME (refpcr) - pts > GST_SECOND)
            pts += PCR_GST_MAX_VALUE;
          else
            refpcr = G_MAXINT64;
        }
      }
    }
    if (refpcr != G_MAXINT64)
      res =
          pts - PCRTIME_TO_GSTTIME (refpcr) + PCRTIME_TO_GSTTIME (refpcroffset);
    else
      GST_WARNING ("No groups, can't calculate timestamp");
  } else
    GST_WARNING ("Not enough information to calculate proper timestamp");

  PACKETIZER_GROUP_UNLOCK (packetizer);

  GST_DEBUG ("Returning timestamp %" GST_TIME_FORMAT " for pts %"
      GST_TIME_FORMAT " pcr_pid:0x%04x", GST_TIME_ARGS (res),
      GST_TIME_ARGS (pts), pcr_pid);
  return res;
}

/* Input  : local PTS (in GHz units)
 * Return : Stream time (in GHz units) */
GstClockTime
mpegts_packetizer_pts_to_ts_unchecked (MpegTSPacketizer2 * packetizer,
    GstClockTime pts, guint16 pcr_pid)
{
  return mpegts_packetizer_pts_to_ts_internal (packetizer, pts, pcr_pid, FALSE);
}

/* Input  : local PTS (in GHz units)
 * Return : Stream time (in GHz units) */
GstClockTime
mpegts_packetizer_pts_to_ts (MpegTSPacketizer2 * packetizer,
    GstClockTime pts, guint16 pcr_pid)
{
  return mpegts_packetizer_pts_to_ts_internal (packetizer, pts, pcr_pid, TRUE);
}

/* Stream time to offset */
guint64
mpegts_packetizer_ts_to_offset (MpegTSPacketizer2 * packetizer,
    GstClockTime ts, guint16 pcr_pid)
{
  MpegTSPCR *pcrtable;
  guint64 res;
  PCROffsetGroup *nextgroup = NULL, *prevgroup = NULL;
  guint64 querypcr, firstpcr, lastpcr, firstoffset, lastoffset;
  PCROffsetCurrent *current;
  GList *tmp;

  if (!packetizer->calculate_offset)
    return -1;

  PACKETIZER_GROUP_LOCK (packetizer);
  pcrtable = get_pcr_table (packetizer, pcr_pid);

  if (pcrtable->groups == NULL) {
    PACKETIZER_GROUP_UNLOCK (packetizer);
    return -1;
  }

  querypcr = GSTTIME_TO_PCRTIME (ts);

  GST_DEBUG ("Searching offset for ts %" GST_TIME_FORMAT, GST_TIME_ARGS (ts));

  /* First check if we're within the current pending group */
  current = pcrtable->current;
  if (current && current->group && (querypcr >= current->group->pcr_offset) &&
      querypcr - current->group->pcr_offset <=
      current->pending[current->last].pcr) {
    GST_DEBUG ("pcr is in current group");
    nextgroup = current->group;
    goto calculate_points;
  }

  /* Find the neighbouring groups */
  for (tmp = pcrtable->groups; tmp; tmp = tmp->next) {
    nextgroup = (PCROffsetGroup *) tmp->data;

    GST_DEBUG ("Trying group PCR %" GST_TIME_FORMAT " (offset %"
        G_GUINT64_FORMAT " pcr_offset %" GST_TIME_FORMAT,
        GST_TIME_ARGS (PCRTIME_TO_GSTTIME (nextgroup->first_pcr)),
        nextgroup->first_offset,
        GST_TIME_ARGS (PCRTIME_TO_GSTTIME (nextgroup->pcr_offset)));

    /* Check if we've gone too far */
    if (nextgroup->pcr_offset > querypcr) {
      GST_DEBUG ("pcr is before that group");
      break;
    }

    if (tmp->next == NULL) {
      GST_DEBUG ("pcr is beyond last group");
      break;
    }

    prevgroup = nextgroup;

    /* Maybe it's in this group */
    if (nextgroup->values[nextgroup->last_value].pcr +
        nextgroup->pcr_offset >= querypcr) {
      GST_DEBUG ("pcr is in that group");
      break;
    }
  }

calculate_points:

  GST_DEBUG ("nextgroup:%p, prevgroup:%p", nextgroup, prevgroup);

  if (nextgroup == prevgroup || prevgroup == NULL) {
    /* We use the current group to calculate position:
     * * if the PCR is within this group
     * * if there is only one group to use for calculation
     */
    GST_DEBUG ("In group or after last one");
    lastoffset = firstoffset = nextgroup->first_offset;
    lastpcr = firstpcr = nextgroup->pcr_offset;
    if (current && nextgroup == current->group) {
      lastoffset += current->pending[current->last].offset;
      lastpcr += current->pending[current->last].pcr;
    } else {
      lastoffset += nextgroup->values[nextgroup->last_value].offset;
      lastpcr += nextgroup->values[nextgroup->last_value].pcr;
    }
  } else {
    GST_DEBUG ("Between group");
    lastoffset = nextgroup->first_offset;
    lastpcr = nextgroup->pcr_offset;
    firstoffset =
        prevgroup->values[prevgroup->last_value].offset +
        prevgroup->first_offset;
    firstpcr =
        prevgroup->values[prevgroup->last_value].pcr + prevgroup->pcr_offset;
  }

  PACKETIZER_GROUP_UNLOCK (packetizer);

  GST_DEBUG ("Using prev PCR %" G_GUINT64_FORMAT " offset %" G_GUINT64_FORMAT,
      firstpcr, firstoffset);
  GST_DEBUG ("Using last PCR %" G_GUINT64_FORMAT " offset %" G_GUINT64_FORMAT,
      lastpcr, lastoffset);

  res = firstoffset;
  if (lastpcr != firstpcr)
    res += gst_util_uint64_scale (querypcr - firstpcr,
        lastoffset - firstoffset, lastpcr - firstpcr);

  GST_DEBUG ("Returning offset %" G_GUINT64_FORMAT " for ts %"
      GST_TIME_FORMAT, res, GST_TIME_ARGS (ts));

  return res;
}

void
mpegts_packetizer_set_reference_offset (MpegTSPacketizer2 * packetizer,
    guint64 refoffset)
{
  GST_DEBUG ("Setting reference offset to %" G_GUINT64_FORMAT, refoffset);

  PACKETIZER_GROUP_LOCK (packetizer);
  packetizer->refoffset = refoffset;
  PACKETIZER_GROUP_UNLOCK (packetizer);
}

void
mpegts_packetizer_set_pcr_discont_threshold (MpegTSPacketizer2 * packetizer,
    GstClockTime threshold)
{
  PACKETIZER_GROUP_LOCK (packetizer);
  packetizer->pcr_discont_threshold = threshold;
  PACKETIZER_GROUP_UNLOCK (packetizer);
}

void
mpegts_packetizer_set_current_pcr_offset (MpegTSPacketizer2 * packetizer,
    GstClockTime offset, guint16 pcr_pid)
{
  guint64 pcr_offset;
  gint64 delta;
  MpegTSPCR *pcrtable;
  PCROffsetGroup *group;
  GList *tmp;
  gboolean apply = FALSE;

  /* fast path */
  PACKETIZER_GROUP_LOCK (packetizer);
  pcrtable = get_pcr_table (packetizer, pcr_pid);

  if (pcrtable == NULL || pcrtable->current->group == NULL) {
    PACKETIZER_GROUP_UNLOCK (packetizer);
    return;
  }

  pcr_offset = GSTTIME_TO_PCRTIME (offset);

  /* Pick delta from *first* group */
  if (pcrtable->groups)
    group = pcrtable->groups->data;
  else
    group = pcrtable->current->group;
  GST_DEBUG ("Current group PCR %" GST_TIME_FORMAT " (offset %"
      G_GUINT64_FORMAT " pcr_offset %" GST_TIME_FORMAT,
      GST_TIME_ARGS (PCRTIME_TO_GSTTIME (group->first_pcr)),
      group->first_offset,
      GST_TIME_ARGS (PCRTIME_TO_GSTTIME (group->pcr_offset)));

  /* Remember the difference between previous initial pcr_offset and
   * new initial pcr_offset */
  delta = pcr_offset - group->pcr_offset;
  if (delta == 0) {
    GST_DEBUG ("No shift to apply");
    PACKETIZER_GROUP_UNLOCK (packetizer);
    return;
  }
  GST_DEBUG ("Shifting groups by %" GST_TIME_FORMAT
      " for new initial pcr_offset %" GST_TIME_FORMAT,
      GST_TIME_ARGS (PCRTIME_TO_GSTTIME (delta)), GST_TIME_ARGS (offset));

  for (tmp = pcrtable->groups; tmp; tmp = tmp->next) {
    PCROffsetGroup *tgroup = (tmp->data);
    if (tgroup == group)
      apply = TRUE;
    if (apply) {
      tgroup->pcr_offset += delta;
      GST_DEBUG ("Update group PCR %" GST_TIME_FORMAT " (offset %"
          G_GUINT64_FORMAT " pcr_offset %" GST_TIME_FORMAT,
          GST_TIME_ARGS (PCRTIME_TO_GSTTIME (tgroup->first_pcr)),
          tgroup->first_offset,
          GST_TIME_ARGS (PCRTIME_TO_GSTTIME (tgroup->pcr_offset)));
    } else
      GST_DEBUG ("Not modifying group PCR %" GST_TIME_FORMAT " (offset %"
          G_GUINT64_FORMAT " pcr_offset %" GST_TIME_FORMAT,
          GST_TIME_ARGS (PCRTIME_TO_GSTTIME (tgroup->first_pcr)),
          tgroup->first_offset,
          GST_TIME_ARGS (PCRTIME_TO_GSTTIME (tgroup->pcr_offset)));
  }
  PACKETIZER_GROUP_UNLOCK (packetizer);
}
