/*
 * gstmpegtssection.c -
 * Copyright (C) 2013 Edward Hervey
 * Copyright (C) 2011, Hewlett-Packard Development Company, L.P.
 * Copyright (C) 2007 Alessandro Decina
 *               2010 Edward Hervey
 *  Author: Youness Alaoui <youness.alaoui@collabora.co.uk>, Collabora Ltd.
 *  Author: Sebastian Dröge <sebastian.droege@collabora.co.uk>, Collabora Ltd.
 *  Author: Edward Hervey <bilboed@bilboed.com>, Collabora Ltd.
 *
 * Authors:
 *   Alessandro Decina <alessandro@nnva.org>
 *   Zaheer Abbas Merali <zaheerabbas at merali dot org>
 *   Edward Hervey <edward@collabora.com>
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

#include "mpegts.h"
#include "gstmpegts-private.h"

/**
 * SECTION:gst-dvb-section
 * @title: DVB variants of MPEG-TS sections
 * @short_description: Sections for the various DVB specifications
 * @include: gst/mpegts/mpegts.h
 *
 * The list of section types defined and used by the DVB specifications can be
 * seen in %GstMpegtsSectionDVBTableID.
 *
 * # Supported DVB MPEG-TS sections
 * These are the sections for which parsing and packetizing code exists.
 *
 * ## Network Information Table (NIT)
 * See:
 * * gst_mpegts_section_get_nit()
 * * %GstMpegtsNIT
 * * %GstMpegtsNITStream
 * * gst_mpegts_nit_new()
 *
 * ## Service Description Table (SDT)
 * See:
 * * gst_mpegts_section_get_sdt()
 * * %GstMpegtsSDT
 * * %GstMpegtsSDTService
 * * gst_mpegts_sdt_new()
 *
 * ## Bouquet Association Table (BAT)
 * See:
 * * gst_mpegts_section_get_bat()
 * * %GstMpegtsBAT
 * * %GstMpegtsBATStream
 *
 * ## Event Information Table (EIT)
 * See:
 * * gst_mpegts_section_get_eit()
 * * %GstMpegtsEIT
 * * %GstMpegtsEITEvent
 *
 * ## Time Date Table (TDT)
 * See:
 * * gst_mpegts_section_get_tdt()
 *
 * ## Time Offset Table (TOT)
 * See:
 * * gst_mpegts_section_get_tot()
 * * %GstMpegtsTOT
 *
 * ## Selection Information Table (SIT)
 * See:
 * * gst_mpegts_section_get_sit()
 * * %GstMpegtsSIT
 * * %GstMpegtsSITService
 *
 * # API
 */


/*
 * TODO
 *
 * * Check minimum size for section parsing in the various
 *   gst_mpegts_section_get_<tabld>() methods
 *
 * * Implement parsing code for
 *   * BAT
 *   * CAT
 *   * TSDT
 */

static inline GstDateTime *
_parse_utc_time (guint8 * data)
{
  guint year, month, day, hour, minute, second;
  guint16 mjd;
  guint8 *utc_ptr;

  mjd = GST_READ_UINT16_BE (data);

  if (mjd == G_MAXUINT16)
    return NULL;

  /* See EN 300 468 Annex C */
  year = (guint32) (((mjd - 15078.2) / 365.25));
  month = (guint8) ((mjd - 14956.1 - (guint) (year * 365.25)) / 30.6001);
  day = mjd - 14956 - (guint) (year * 365.25) - (guint) (month * 30.6001);
  if (month == 14 || month == 15) {
    year++;
    month = month - 1 - 12;
  } else {
    month--;
  }
  year += 1900;

  utc_ptr = data + 2;

  /* First digit of hours cannot exceed 1 (max: 23 hours) */
  hour = ((utc_ptr[0] & 0x30) >> 4) * 10 + (utc_ptr[0] & 0x0F);
  /* First digit of minutes cannot exced 5 (max: 59 mins) */
  minute = ((utc_ptr[1] & 0x70) >> 4) * 10 + (utc_ptr[1] & 0x0F);
  /* first digit of seconds cannot exceed 5 (max: 59 seconds) */
  second = ((utc_ptr[2] & 0x70) >> 4) * 10 + (utc_ptr[2] & 0x0F);

  /* Time is UTC */
  if (hour < 24 && minute < 60 && second < 60) {
    return gst_date_time_new (0.0, year, month, day, hour, minute,
        (gdouble) second);
  } else if (utc_ptr[0] == 0xFF && utc_ptr[1] == 0xFF && utc_ptr[2] == 0xFF) {
    return gst_date_time_new (0.0, year, month, day, -1, -1, -1);
  }

  return NULL;
}

/* Event Information Table */
static GstMpegtsEITEvent *
_gst_mpegts_eit_event_copy (GstMpegtsEITEvent * eit)
{
  GstMpegtsEITEvent *copy;

  copy = g_memdup2 (eit, sizeof (GstMpegtsEITEvent));
  copy->start_time = gst_date_time_ref (eit->start_time);
  copy->descriptors = g_ptr_array_ref (eit->descriptors);

  return copy;
}

static void
_gst_mpegts_eit_event_free (GstMpegtsEITEvent * eit)
{
  if (eit->start_time)
    gst_date_time_unref (eit->start_time);
  if (eit->descriptors)
    g_ptr_array_unref (eit->descriptors);
  g_free (eit);
}

G_DEFINE_BOXED_TYPE (GstMpegtsEITEvent, gst_mpegts_eit_event,
    (GBoxedCopyFunc) _gst_mpegts_eit_event_copy,
    (GFreeFunc) _gst_mpegts_eit_event_free);

static GstMpegtsEIT *
_gst_mpegts_eit_copy (GstMpegtsEIT * eit)
{
  GstMpegtsEIT *copy;

  copy = g_memdup2 (eit, sizeof (GstMpegtsEIT));
  copy->events = g_ptr_array_ref (eit->events);

  return copy;
}

static void
_gst_mpegts_eit_free (GstMpegtsEIT * eit)
{
  g_ptr_array_unref (eit->events);
  g_free (eit);
}

G_DEFINE_BOXED_TYPE (GstMpegtsEIT, gst_mpegts_eit,
    (GBoxedCopyFunc) _gst_mpegts_eit_copy, (GFreeFunc) _gst_mpegts_eit_free);

static gpointer
_parse_eit (GstMpegtsSection * section)
{
  GstMpegtsEIT *eit = NULL;
  guint allocated_events = 12;
  guint8 *data, *end, *duration_ptr;
  guint16 descriptors_loop_length;

  eit = g_new0 (GstMpegtsEIT, 1);

  data = section->data;
  end = data + section->section_length;

  /* Skip already parsed data */
  data += 8;

  eit->transport_stream_id = GST_READ_UINT16_BE (data);
  data += 2;
  eit->original_network_id = GST_READ_UINT16_BE (data);
  data += 2;
  eit->segment_last_section_number = *data++;
  eit->last_table_id = *data++;

  eit->actual_stream = (section->table_id == 0x4E ||
      (section->table_id >= 0x50 && section->table_id <= 0x5F));
  eit->present_following = (section->table_id == 0x4E
      || section->table_id == 0x4F);

  eit->events =
      g_ptr_array_new_full (allocated_events,
      (GDestroyNotify) _gst_mpegts_eit_event_free);

  while (data < end - 4) {
    GstMpegtsEITEvent *event;

    /* 12 is the minimum entry size + CRC */
    if (end - data < 12 + 4) {
      GST_WARNING ("PID %d invalid EIT entry length %d",
          section->pid, (gint) (end - 4 - data));
      goto error;
    }

    event = g_new0 (GstMpegtsEITEvent, 1);
    g_ptr_array_add (eit->events, event);

    event->event_id = GST_READ_UINT16_BE (data);
    data += 2;

    event->start_time = _parse_utc_time (data);
    duration_ptr = data + 5;
    event->duration = (((duration_ptr[0] & 0xF0) >> 4) * 10 +
        (duration_ptr[0] & 0x0F)) * 60 * 60 +
        (((duration_ptr[1] & 0xF0) >> 4) * 10 +
        (duration_ptr[1] & 0x0F)) * 60 +
        ((duration_ptr[2] & 0xF0) >> 4) * 10 + (duration_ptr[2] & 0x0F);

    data += 8;
    event->running_status = *data >> 5;
    event->free_CA_mode = (*data >> 4) & 0x01;

    descriptors_loop_length = GST_READ_UINT16_BE (data) & 0x0FFF;
    data += 2;

    event->descriptors =
        gst_mpegts_parse_descriptors (data, descriptors_loop_length);
    if (event->descriptors == NULL)
      goto error;
    data += descriptors_loop_length;
  }

  if (data != end - 4) {
    GST_WARNING ("PID %d invalid EIT parsed %d length %d",
        section->pid, (gint) (data - section->data), section->section_length);
    goto error;
  }

  return (gpointer) eit;

error:
  if (eit)
    _gst_mpegts_eit_free (eit);

  return NULL;

}

/**
 * gst_mpegts_section_get_eit:
 * @section: a #GstMpegtsSection of type %GST_MPEGTS_SECTION_EIT
 *
 * Returns the #GstMpegtsEIT contained in the @section.
 *
 * Returns: The #GstMpegtsEIT contained in the section, or %NULL if an error
 * happened.
 */
const GstMpegtsEIT *
gst_mpegts_section_get_eit (GstMpegtsSection * section)
{
  g_return_val_if_fail (section->section_type == GST_MPEGTS_SECTION_EIT, NULL);
  g_return_val_if_fail (section->cached_parsed || section->data, NULL);

  if (!section->cached_parsed)
    section->cached_parsed = __common_section_checks (section, 18, _parse_eit,
        (GDestroyNotify) _gst_mpegts_eit_free);

  return (const GstMpegtsEIT *) section->cached_parsed;
}

/* Bouquet Association Table */
static GstMpegtsBATStream *
_gst_mpegts_bat_stream_copy (GstMpegtsBATStream * bat)
{
  GstMpegtsBATStream *copy;

  copy = g_memdup2 (bat, sizeof (GstMpegtsBATStream));
  copy->descriptors = g_ptr_array_ref (bat->descriptors);

  return copy;
}

static void
_gst_mpegts_bat_stream_free (GstMpegtsBATStream * bat)
{
  if (bat->descriptors)
    g_ptr_array_unref (bat->descriptors);
  g_free (bat);
}

G_DEFINE_BOXED_TYPE (GstMpegtsBATStream, gst_mpegts_bat_stream,
    (GBoxedCopyFunc) _gst_mpegts_bat_stream_copy,
    (GFreeFunc) _gst_mpegts_bat_stream_free);

static GstMpegtsBAT *
_gst_mpegts_bat_copy (GstMpegtsBAT * bat)
{
  GstMpegtsBAT *copy;

  copy = g_memdup2 (bat, sizeof (GstMpegtsBAT));
  copy->descriptors = g_ptr_array_ref (bat->descriptors);
  copy->streams = g_ptr_array_ref (bat->streams);

  return copy;
}

static void
_gst_mpegts_bat_free (GstMpegtsBAT * bat)
{
  if (bat->descriptors)
    g_ptr_array_unref (bat->descriptors);
  if (bat->streams)
    g_ptr_array_unref (bat->streams);
  g_free (bat);
}

G_DEFINE_BOXED_TYPE (GstMpegtsBAT, gst_mpegts_bat,
    (GBoxedCopyFunc) _gst_mpegts_bat_copy, (GFreeFunc) _gst_mpegts_bat_free);

static gpointer
_parse_bat (GstMpegtsSection * section)
{
  GstMpegtsBAT *bat = NULL;
  guint allocated_streams = 12;
  guint8 *data, *end, *entry_begin;
  guint16 descriptors_loop_length, transport_stream_loop_length;

  GST_DEBUG ("BAT");

  bat = g_new0 (GstMpegtsBAT, 1);

  data = section->data;
  end = data + section->section_length;

  /* Skip already parsed data */
  data += 8;

  descriptors_loop_length = GST_READ_UINT16_BE (data) & 0x0FFF;
  data += 2;

  /* see if the buffer is large enough */
  if (descriptors_loop_length && (data + descriptors_loop_length > end - 4)) {
    GST_WARNING ("PID %d invalid BAT descriptors loop length %d",
        section->pid, descriptors_loop_length);
    goto error;
  }
  bat->descriptors =
      gst_mpegts_parse_descriptors (data, descriptors_loop_length);
  if (bat->descriptors == NULL)
    goto error;
  data += descriptors_loop_length;

  transport_stream_loop_length = GST_READ_UINT16_BE (data) & 0x0FFF;
  data += 2;
  if (G_UNLIKELY (transport_stream_loop_length > (end - 4 - data))) {
    GST_WARNING
        ("PID 0x%04x invalid BAT (transport_stream_loop_length too big)",
        section->pid);
    goto error;
  }

  bat->streams =
      g_ptr_array_new_full (allocated_streams,
      (GDestroyNotify) _gst_mpegts_bat_stream_free);

  /* read up to the CRC */
  while (transport_stream_loop_length - 4 > 0) {
    GstMpegtsBATStream *stream = g_new0 (GstMpegtsBATStream, 1);

    g_ptr_array_add (bat->streams, stream);

    if (transport_stream_loop_length < 6) {
      /* each entry must be at least 6 bytes (+ 4 bytes CRC) */
      GST_WARNING ("PID %d invalid BAT entry size %d",
          section->pid, transport_stream_loop_length);
      goto error;
    }

    entry_begin = data;

    stream->transport_stream_id = GST_READ_UINT16_BE (data);
    data += 2;

    stream->original_network_id = GST_READ_UINT16_BE (data);
    data += 2;

    descriptors_loop_length = GST_READ_UINT16_BE (data) & 0x0FFF;
    data += 2;

    GST_DEBUG ("descriptors_loop_length %d", descriptors_loop_length);

    if (descriptors_loop_length && (data + descriptors_loop_length > end - 4)) {
      GST_WARNING
          ("PID %d invalid BAT entry %d descriptors loop length %d (only have %"
          G_GSIZE_FORMAT ")", section->pid, section->subtable_extension,
          descriptors_loop_length, (gsize) (end - 4 - data));
      goto error;
    }
    stream->descriptors =
        gst_mpegts_parse_descriptors (data, descriptors_loop_length);
    if (stream->descriptors == NULL)
      goto error;

    data += descriptors_loop_length;

    transport_stream_loop_length -= data - entry_begin;
  }

  if (data != end - 4) {
    GST_WARNING ("PID %d invalid BAT parsed %d length %d",
        section->pid, (gint) (data - section->data), section->section_length);
    goto error;
  }

  return (gpointer) bat;

error:
  if (bat)
    _gst_mpegts_bat_free (bat);

  return NULL;
}

/**
 * gst_mpegts_section_get_bat:
 * @section: a #GstMpegtsSection of type %GST_MPEGTS_SECTION_BAT
 *
 * Returns the #GstMpegtsBAT contained in the @section.
 *
 * Returns: The #GstMpegtsBAT contained in the section, or %NULL if an error
 * happened.
 */
const GstMpegtsBAT *
gst_mpegts_section_get_bat (GstMpegtsSection * section)
{
  g_return_val_if_fail (section->section_type == GST_MPEGTS_SECTION_BAT, NULL);
  g_return_val_if_fail (section->cached_parsed || section->data, NULL);

  if (!section->cached_parsed)
    section->cached_parsed =
        __common_section_checks (section, 16, _parse_bat,
        (GDestroyNotify) _gst_mpegts_bat_free);

  return (const GstMpegtsBAT *) section->cached_parsed;
}


/* Network Information Table */

static GstMpegtsNITStream *
_gst_mpegts_nit_stream_copy (GstMpegtsNITStream * nit)
{
  GstMpegtsNITStream *copy;

  copy = g_memdup2 (nit, sizeof (GstMpegtsNITStream));
  copy->descriptors = g_ptr_array_ref (nit->descriptors);

  return copy;
}

static void
_gst_mpegts_nit_stream_free (GstMpegtsNITStream * nit)
{
  if (nit->descriptors)
    g_ptr_array_unref (nit->descriptors);
  g_free (nit);
}

G_DEFINE_BOXED_TYPE (GstMpegtsNITStream, gst_mpegts_nit_stream,
    (GBoxedCopyFunc) _gst_mpegts_nit_stream_copy,
    (GFreeFunc) _gst_mpegts_nit_stream_free);

static GstMpegtsNIT *
_gst_mpegts_nit_copy (GstMpegtsNIT * nit)
{
  GstMpegtsNIT *copy = g_memdup2 (nit, sizeof (GstMpegtsNIT));

  copy->descriptors = g_ptr_array_ref (nit->descriptors);
  copy->streams = g_ptr_array_ref (nit->streams);

  return copy;
}

static void
_gst_mpegts_nit_free (GstMpegtsNIT * nit)
{
  if (nit->descriptors)
    g_ptr_array_unref (nit->descriptors);
  g_ptr_array_unref (nit->streams);
  g_free (nit);
}

G_DEFINE_BOXED_TYPE (GstMpegtsNIT, gst_mpegts_nit,
    (GBoxedCopyFunc) _gst_mpegts_nit_copy, (GFreeFunc) _gst_mpegts_nit_free);


static gpointer
_parse_nit (GstMpegtsSection * section)
{
  GstMpegtsNIT *nit = NULL;
  guint allocated_streams = 12;
  guint8 *data, *end, *entry_begin;
  guint16 descriptors_loop_length, transport_stream_loop_length;

  GST_DEBUG ("NIT");

  nit = g_new0 (GstMpegtsNIT, 1);

  data = section->data;
  end = data + section->section_length;

  /* Set network id, and skip the rest of what is already parsed */
  nit->network_id = section->subtable_extension;
  data += 8;

  nit->actual_network = section->table_id == 0x40;

  descriptors_loop_length = GST_READ_UINT16_BE (data) & 0x0FFF;
  data += 2;

  /* see if the buffer is large enough */
  if (descriptors_loop_length && (data + descriptors_loop_length > end - 4)) {
    GST_WARNING ("PID %d invalid NIT descriptors loop length %d",
        section->pid, descriptors_loop_length);
    goto error;
  }
  nit->descriptors =
      gst_mpegts_parse_descriptors (data, descriptors_loop_length);
  if (nit->descriptors == NULL)
    goto error;
  data += descriptors_loop_length;

  transport_stream_loop_length = GST_READ_UINT16_BE (data) & 0x0FFF;
  data += 2;
  if (G_UNLIKELY (transport_stream_loop_length > (end - 4 - data))) {
    GST_WARNING
        ("PID 0x%04x invalid NIT (transport_stream_loop_length too big)",
        section->pid);
    goto error;
  }

  nit->streams =
      g_ptr_array_new_full (allocated_streams,
      (GDestroyNotify) _gst_mpegts_nit_stream_free);

  /* read up to the CRC */
  while (transport_stream_loop_length - 4 > 0) {
    GstMpegtsNITStream *stream = g_new0 (GstMpegtsNITStream, 1);

    g_ptr_array_add (nit->streams, stream);

    if (transport_stream_loop_length < 6) {
      /* each entry must be at least 6 bytes (+ 4 bytes CRC) */
      GST_WARNING ("PID %d invalid NIT entry size %d",
          section->pid, transport_stream_loop_length);
      goto error;
    }

    entry_begin = data;

    stream->transport_stream_id = GST_READ_UINT16_BE (data);
    data += 2;

    stream->original_network_id = GST_READ_UINT16_BE (data);
    data += 2;

    descriptors_loop_length = GST_READ_UINT16_BE (data) & 0x0FFF;
    data += 2;

    GST_DEBUG ("descriptors_loop_length %d", descriptors_loop_length);

    if (descriptors_loop_length && (data + descriptors_loop_length > end - 4)) {
      GST_WARNING
          ("PID %d invalid NIT entry %d descriptors loop length %d (only have %"
          G_GSIZE_FORMAT ")", section->pid, section->subtable_extension,
          descriptors_loop_length, (gsize) (end - 4 - data));
      goto error;
    }
    stream->descriptors =
        gst_mpegts_parse_descriptors (data, descriptors_loop_length);
    if (stream->descriptors == NULL)
      goto error;

    data += descriptors_loop_length;

    transport_stream_loop_length -= data - entry_begin;
  }

  if (data != end - 4) {
    GST_WARNING ("PID %d invalid NIT parsed %d length %d",
        section->pid, (gint) (data - section->data), section->section_length);
    goto error;
  }

  return (gpointer) nit;

error:
  if (nit)
    _gst_mpegts_nit_free (nit);

  return NULL;
}

/**
 * gst_mpegts_section_get_nit:
 * @section: a #GstMpegtsSection of type %GST_MPEGTS_SECTION_NIT
 *
 * Returns the #GstMpegtsNIT contained in the @section.
 *
 * Returns: The #GstMpegtsNIT contained in the section, or %NULL if an error
 * happened.
 */
const GstMpegtsNIT *
gst_mpegts_section_get_nit (GstMpegtsSection * section)
{
  g_return_val_if_fail (section->section_type == GST_MPEGTS_SECTION_NIT, NULL);
  g_return_val_if_fail (section->cached_parsed || section->data, NULL);

  if (!section->cached_parsed)
    section->cached_parsed =
        __common_section_checks (section, 16, _parse_nit,
        (GDestroyNotify) _gst_mpegts_nit_free);

  return (const GstMpegtsNIT *) section->cached_parsed;
}

/**
 * gst_mpegts_nit_new:
 *
 * Allocates and initializes a #GstMpegtsNIT.
 *
 * Returns: (transfer full): A newly allocated #GstMpegtsNIT
 */
GstMpegtsNIT *
gst_mpegts_nit_new (void)
{
  GstMpegtsNIT *nit;

  nit = g_new0 (GstMpegtsNIT, 1);

  nit->descriptors = g_ptr_array_new_with_free_func ((GDestroyNotify)
      gst_mpegts_descriptor_free);
  nit->streams = g_ptr_array_new_with_free_func ((GDestroyNotify)
      _gst_mpegts_nit_stream_free);

  return nit;
}

/**
 * gst_mpegts_nit_stream_new:
 *
 * Allocates and initializes a #GstMpegtsNITStream
 *
 * Returns: (transfer full): A newly allocated #GstMpegtsNITStream
 */
GstMpegtsNITStream *
gst_mpegts_nit_stream_new (void)
{
  GstMpegtsNITStream *stream;

  stream = g_new0 (GstMpegtsNITStream, 1);

  stream->descriptors = g_ptr_array_new_with_free_func (
      (GDestroyNotify) gst_mpegts_descriptor_free);

  return stream;
}

static gboolean
_packetize_nit (GstMpegtsSection * section)
{
  gsize length, network_length, loop_length;
  const GstMpegtsNIT *nit;
  GstMpegtsNITStream *stream;
  GstMpegtsDescriptor *descriptor;
  guint i, j;
  guint8 *data, *pos;

  nit = gst_mpegts_section_get_nit (section);

  if (nit == NULL)
    return FALSE;

  /* 8 byte common section fields
     2 byte network_descriptors_length
     2 byte transport_stream_loop_length
     4 byte CRC */
  length = 16;

  /* Find length of network descriptors */
  network_length = 0;
  if (nit->descriptors) {
    for (i = 0; i < nit->descriptors->len; i++) {
      descriptor = g_ptr_array_index (nit->descriptors, i);
      network_length += descriptor->length + 2;
    }
  }

  /* Find length of loop */
  loop_length = 0;
  if (nit->streams) {
    for (i = 0; i < nit->streams->len; i++) {
      stream = g_ptr_array_index (nit->streams, i);
      loop_length += 6;
      if (stream->descriptors) {
        for (j = 0; j < stream->descriptors->len; j++) {
          descriptor = g_ptr_array_index (stream->descriptors, j);
          loop_length += descriptor->length + 2;
        }
      }
    }
  }

  length += network_length + loop_length;

  /* Max length of NIT section is 1024 bytes */
  g_return_val_if_fail (length <= 1024, FALSE);

  _packetize_common_section (section, length);

  data = section->data + 8;
  /* reserved                         - 4  bit
     network_descriptors_length       - 12 bit uimsbf */
  GST_WRITE_UINT16_BE (data, network_length | 0xF000);
  data += 2;

  _packetize_descriptor_array (nit->descriptors, &data);

  /* reserved                         - 4  bit
     transport_stream_loop_length     - 12 bit uimsbf */
  GST_WRITE_UINT16_BE (data, loop_length | 0xF000);
  data += 2;

  if (nit->streams) {
    for (i = 0; i < nit->streams->len; i++) {
      stream = g_ptr_array_index (nit->streams, i);
      /* transport_stream_id          - 16 bit uimsbf */
      GST_WRITE_UINT16_BE (data, stream->transport_stream_id);
      data += 2;

      /* original_network_id          - 16 bit uimsbf */
      GST_WRITE_UINT16_BE (data, stream->original_network_id);
      data += 2;

      /* reserved                     -  4 bit
         transport_descriptors_length - 12 bit uimsbf

         Set length to zero, and update in loop */
      pos = data;
      *data++ = 0xF0;
      *data++ = 0x00;

      _packetize_descriptor_array (stream->descriptors, &data);

      /* Go back and update the descriptor length */
      GST_WRITE_UINT16_BE (pos, (data - pos - 2) | 0xF000);
    }
  }

  return TRUE;
}

/**
 * gst_mpegts_section_from_nit:
 * @nit: (transfer full): a #GstMpegtsNIT to create the #GstMpegtsSection from
 *
 * Ownership of @nit is taken. The data in @nit is managed by the #GstMpegtsSection
 *
 * Returns: (transfer full): the #GstMpegtsSection
 */
GstMpegtsSection *
gst_mpegts_section_from_nit (GstMpegtsNIT * nit)
{
  GstMpegtsSection *section;

  g_return_val_if_fail (nit != NULL, NULL);

  if (nit->actual_network)
    section = _gst_mpegts_section_init (0x10,
        GST_MTS_TABLE_ID_NETWORK_INFORMATION_ACTUAL_NETWORK);
  else
    section = _gst_mpegts_section_init (0x10,
        GST_MTS_TABLE_ID_NETWORK_INFORMATION_OTHER_NETWORK);

  section->subtable_extension = nit->network_id;
  section->cached_parsed = (gpointer) nit;
  section->packetizer = _packetize_nit;
  section->destroy_parsed = (GDestroyNotify) _gst_mpegts_nit_free;

  return section;
}

/* Service Description Table (SDT) */

static GstMpegtsSDTService *
_gst_mpegts_sdt_service_copy (GstMpegtsSDTService * sdt)
{
  GstMpegtsSDTService *copy = g_memdup2 (sdt, sizeof (GstMpegtsSDTService));

  copy->descriptors = g_ptr_array_ref (sdt->descriptors);

  return copy;
}

static void
_gst_mpegts_sdt_service_free (GstMpegtsSDTService * sdt)
{
  if (sdt->descriptors)
    g_ptr_array_unref (sdt->descriptors);
  g_free (sdt);
}

G_DEFINE_BOXED_TYPE (GstMpegtsSDTService, gst_mpegts_sdt_service,
    (GBoxedCopyFunc) _gst_mpegts_sdt_service_copy,
    (GFreeFunc) _gst_mpegts_sdt_service_free);

static GstMpegtsSDT *
_gst_mpegts_sdt_copy (GstMpegtsSDT * sdt)
{
  GstMpegtsSDT *copy = g_memdup2 (sdt, sizeof (GstMpegtsSDT));

  copy->services = g_ptr_array_ref (sdt->services);

  return copy;
}

static void
_gst_mpegts_sdt_free (GstMpegtsSDT * sdt)
{
  g_ptr_array_unref (sdt->services);
  g_free (sdt);
}

G_DEFINE_BOXED_TYPE (GstMpegtsSDT, gst_mpegts_sdt,
    (GBoxedCopyFunc) _gst_mpegts_sdt_copy, (GFreeFunc) _gst_mpegts_sdt_free);


static gpointer
_parse_sdt (GstMpegtsSection * section)
{
  GstMpegtsSDT *sdt = NULL;
  guint allocated_services = 8;
  guint8 *data, *end, *entry_begin;
  guint tmp;
  guint sdt_info_length;
  guint descriptors_loop_length;

  GST_DEBUG ("SDT");

  sdt = g_new0 (GstMpegtsSDT, 1);

  data = section->data;
  end = data + section->section_length;

  sdt->transport_stream_id = section->subtable_extension;

  /* Skip common fields */
  data += 8;

  sdt->original_network_id = GST_READ_UINT16_BE (data);
  data += 2;

  /* skip reserved byte */
  data += 1;

  sdt->actual_ts = section->table_id == 0x42;

  sdt_info_length = section->section_length - 11;

  sdt->services = g_ptr_array_new_full (allocated_services,
      (GDestroyNotify) _gst_mpegts_sdt_service_free);

  /* read up to the CRC */
  while (sdt_info_length - 4 > 0) {
    GstMpegtsSDTService *service = g_new0 (GstMpegtsSDTService, 1);
    g_ptr_array_add (sdt->services, service);

    entry_begin = data;

    if (sdt_info_length - 4 < 5) {
      /* each entry must be at least 5 bytes (+4 bytes for the CRC) */
      GST_WARNING ("PID %d invalid SDT entry size %d",
          section->pid, sdt_info_length);
      goto error;
    }

    service->service_id = GST_READ_UINT16_BE (data);
    data += 2;

    service->EIT_schedule_flag = ((*data & 0x02) == 2);
    service->EIT_present_following_flag = (*data & 0x01) == 1;

    data += 1;
    tmp = GST_READ_UINT16_BE (data);

    service->running_status = (*data >> 5) & 0x07;
    service->free_CA_mode = (*data >> 4) & 0x01;

    descriptors_loop_length = tmp & 0x0FFF;
    data += 2;

    if (descriptors_loop_length && (data + descriptors_loop_length > end - 4)) {
      GST_WARNING ("PID %d invalid SDT entry %d descriptors loop length %d",
          section->pid, service->service_id, descriptors_loop_length);
      goto error;
    }
    service->descriptors =
        gst_mpegts_parse_descriptors (data, descriptors_loop_length);
    if (!service->descriptors)
      goto error;
    data += descriptors_loop_length;

    sdt_info_length -= data - entry_begin;
  }

  if (data != end - 4) {
    GST_WARNING ("PID %d invalid SDT parsed %d length %d",
        section->pid, (gint) (data - section->data), section->section_length);
    goto error;
  }

  return sdt;

error:
  if (sdt)
    _gst_mpegts_sdt_free (sdt);

  return NULL;
}

/**
 * gst_mpegts_section_get_sdt:
 * @section: a #GstMpegtsSection of type %GST_MPEGTS_SECTION_SDT
 *
 * Returns the #GstMpegtsSDT contained in the @section.
 *
 * Returns: The #GstMpegtsSDT contained in the section, or %NULL if an error
 * happened.
 */
const GstMpegtsSDT *
gst_mpegts_section_get_sdt (GstMpegtsSection * section)
{
  g_return_val_if_fail (section->section_type == GST_MPEGTS_SECTION_SDT, NULL);
  g_return_val_if_fail (section->cached_parsed || section->data, NULL);

  if (!section->cached_parsed)
    section->cached_parsed =
        __common_section_checks (section, 15, _parse_sdt,
        (GDestroyNotify) _gst_mpegts_sdt_free);

  return (const GstMpegtsSDT *) section->cached_parsed;
}

/**
 * gst_mpegts_sdt_new:
 *
 * Allocates and initializes a #GstMpegtsSDT.
 *
 * Returns: (transfer full): A newly allocated #GstMpegtsSDT
 */
GstMpegtsSDT *
gst_mpegts_sdt_new (void)
{
  GstMpegtsSDT *sdt;

  sdt = g_new0 (GstMpegtsSDT, 1);

  sdt->services = g_ptr_array_new_with_free_func ((GDestroyNotify)
      _gst_mpegts_sdt_service_free);

  return sdt;
}

/**
 * gst_mpegts_sdt_service_new:
 *
 * Allocates and initializes a #GstMpegtsSDTService.
 *
 * Returns: (transfer full): A newly allocated #GstMpegtsSDTService
 */
GstMpegtsSDTService *
gst_mpegts_sdt_service_new (void)
{
  GstMpegtsSDTService *service;

  service = g_new0 (GstMpegtsSDTService, 1);

  service->descriptors = g_ptr_array_new_with_free_func ((GDestroyNotify)
      gst_mpegts_descriptor_free);

  return service;
}

static gboolean
_packetize_sdt (GstMpegtsSection * section)
{
  gsize length, service_length;
  const GstMpegtsSDT *sdt;
  GstMpegtsSDTService *service;
  GstMpegtsDescriptor *descriptor;
  guint i, j;
  guint8 *data, *pos;

  sdt = gst_mpegts_section_get_sdt (section);

  if (sdt == NULL)
    return FALSE;

  /* 8 byte common section fields
     2 byte original_network_id
     1 byte reserved
     4 byte CRC */
  length = 15;

  /* Find length of services */
  service_length = 0;
  if (sdt->services) {
    for (i = 0; i < sdt->services->len; i++) {
      service = g_ptr_array_index (sdt->services, i);
      service_length += 5;
      if (service->descriptors) {
        for (j = 0; j < service->descriptors->len; j++) {
          descriptor = g_ptr_array_index (service->descriptors, j);
          service_length += descriptor->length + 2;
        }
      }
    }
  }

  length += service_length;

  /* Max length if SDT section is 1024 bytes */
  g_return_val_if_fail (length <= 1024, FALSE);

  _packetize_common_section (section, length);

  data = section->data + 8;
  /* original_network_id            - 16 bit uimsbf */
  GST_WRITE_UINT16_BE (data, sdt->original_network_id);
  data += 2;
  /* reserved                       -  8 bit */
  *data++ = 0xFF;

  if (sdt->services) {
    for (i = 0; i < sdt->services->len; i++) {
      service = g_ptr_array_index (sdt->services, i);
      /* service_id                 - 16 bit uimsbf */
      GST_WRITE_UINT16_BE (data, service->service_id);
      data += 2;

      /* reserved                   -  6 bit
         EIT_schedule_flag          -  1 bit
         EIT_present_following_flag -  1 bit */
      *data = 0xFC;
      if (service->EIT_schedule_flag)
        *data |= 0x02;
      if (service->EIT_present_following_flag)
        *data |= 0x01;
      data++;

      /* running_status             -  3 bit uimsbf
         free_CA_mode               -  1 bit
         descriptors_loop_length    - 12 bit uimsbf */
      /* Set length to zero for now */
      pos = data;
      *data++ = 0x00;
      *data++ = 0x00;

      _packetize_descriptor_array (service->descriptors, &data);

      /* Go back and update the descriptor length */
      GST_WRITE_UINT16_BE (pos, data - pos - 2);

      *pos |= service->running_status << 5;
      if (service->free_CA_mode)
        *pos |= 0x10;
    }
  }

  return TRUE;
}

/**
 * gst_mpegts_section_from_sdt:
 * @sdt: (transfer full): a #GstMpegtsSDT to create the #GstMpegtsSection from
 *
 * Ownership of @sdt is taken. The data in @sdt is managed by the #GstMpegtsSection
 *
 * Returns: (transfer full): the #GstMpegtsSection
 */
GstMpegtsSection *
gst_mpegts_section_from_sdt (GstMpegtsSDT * sdt)
{
  GstMpegtsSection *section;

  g_return_val_if_fail (sdt != NULL, NULL);

  if (sdt->actual_ts)
    section = _gst_mpegts_section_init (0x11,
        GST_MTS_TABLE_ID_SERVICE_DESCRIPTION_ACTUAL_TS);
  else
    section = _gst_mpegts_section_init (0x11,
        GST_MTS_TABLE_ID_SERVICE_DESCRIPTION_OTHER_TS);

  section->subtable_extension = sdt->transport_stream_id;
  section->cached_parsed = (gpointer) sdt;
  section->packetizer = _packetize_sdt;
  section->destroy_parsed = (GDestroyNotify) _gst_mpegts_sdt_free;

  return section;
}

/* Time and Date Table (TDT) */
static gpointer
_parse_tdt (GstMpegtsSection * section)
{
  return (gpointer) _parse_utc_time (section->data + 3);
}

/**
 * gst_mpegts_section_get_tdt:
 * @section: a #GstMpegtsSection of type %GST_MPEGTS_SECTION_TDT
 *
 * Returns the #GstDateTime of the TDT
 *
 * Returns: The #GstDateTime contained in the section, or %NULL
 * if an error happened. Release with #gst_date_time_unref when done.
 */
GstDateTime *
gst_mpegts_section_get_tdt (GstMpegtsSection * section)
{
  g_return_val_if_fail (section->section_type == GST_MPEGTS_SECTION_TDT, NULL);
  g_return_val_if_fail (section->cached_parsed || section->data, NULL);

  if (!section->cached_parsed)
    section->cached_parsed =
        __common_section_checks (section, 8, _parse_tdt,
        (GDestroyNotify) gst_date_time_unref);

  if (section->cached_parsed)
    return gst_date_time_ref ((GstDateTime *) section->cached_parsed);
  return NULL;
}


/* Time Offset Table (TOT) */
static GstMpegtsTOT *
_gst_mpegts_tot_copy (GstMpegtsTOT * tot)
{
  GstMpegtsTOT *copy = g_memdup2 (tot, sizeof (GstMpegtsTOT));

  if (tot->utc_time)
    copy->utc_time = gst_date_time_ref (tot->utc_time);
  copy->descriptors = g_ptr_array_ref (tot->descriptors);

  return copy;
}

static void
_gst_mpegts_tot_free (GstMpegtsTOT * tot)
{
  if (tot->utc_time)
    gst_date_time_unref (tot->utc_time);
  if (tot->descriptors)
    g_ptr_array_unref (tot->descriptors);
  g_free (tot);
}

G_DEFINE_BOXED_TYPE (GstMpegtsTOT, gst_mpegts_tot,
    (GBoxedCopyFunc) _gst_mpegts_tot_copy, (GFreeFunc) _gst_mpegts_tot_free);

static gpointer
_parse_tot (GstMpegtsSection * section)
{
  guint8 *data;
  GstMpegtsTOT *tot;
  guint16 desc_len;

  GST_DEBUG ("TOT");

  tot = g_new0 (GstMpegtsTOT, 1);

  tot->utc_time = _parse_utc_time (section->data + 3);

  /* Skip 5 bytes from utc_time (+3 of initial offset) */
  data = section->data + 8;

  desc_len = GST_READ_UINT16_BE (data) & 0xFFF;
  data += 2;
  tot->descriptors = gst_mpegts_parse_descriptors (data, desc_len);

  return (gpointer) tot;
}

/**
 * gst_mpegts_section_get_tot:
 * @section: a #GstMpegtsSection of type %GST_MPEGTS_SECTION_TOT
 *
 * Returns the #GstMpegtsTOT contained in the @section.
 *
 * Returns: The #GstMpegtsTOT contained in the section, or %NULL if an error
 * happened.
 */
const GstMpegtsTOT *
gst_mpegts_section_get_tot (GstMpegtsSection * section)
{
  g_return_val_if_fail (section->section_type == GST_MPEGTS_SECTION_TOT, NULL);
  g_return_val_if_fail (section->cached_parsed || section->data, NULL);

  if (!section->cached_parsed)
    section->cached_parsed =
        __common_section_checks (section, 14, _parse_tot,
        (GDestroyNotify) _gst_mpegts_tot_free);

  return (const GstMpegtsTOT *) section->cached_parsed;
}


/* Selection Information Table (SIT) */

static GstMpegtsSITService *
_gst_mpegts_sit_service_copy (GstMpegtsSITService * sit)
{
  GstMpegtsSITService *copy = g_memdup2 (sit, sizeof (GstMpegtsSITService));

  copy->service_id = sit->service_id;
  copy->running_status = sit->running_status;
  copy->descriptors = g_ptr_array_ref (sit->descriptors);

  return copy;
}

static void
_gst_mpegts_sit_service_free (GstMpegtsSITService * sit)
{
  if (sit->descriptors)
    g_ptr_array_unref (sit->descriptors);
  g_free (sit);
}

G_DEFINE_BOXED_TYPE (GstMpegtsSITService, gst_mpegts_sit_service,
    (GBoxedCopyFunc) _gst_mpegts_sit_service_copy,
    (GFreeFunc) _gst_mpegts_sit_service_free);

static GstMpegtsSIT *
_gst_mpegts_sit_copy (GstMpegtsSIT * sit)
{
  GstMpegtsSIT *copy = g_memdup2 (sit, sizeof (GstMpegtsSIT));

  copy->services = g_ptr_array_ref (sit->services);
  copy->descriptors = g_ptr_array_ref (sit->descriptors);

  return copy;
}

static void
_gst_mpegts_sit_free (GstMpegtsSIT * sit)
{
  g_ptr_array_unref (sit->services);
  g_ptr_array_unref (sit->descriptors);
  g_free (sit);
}

G_DEFINE_BOXED_TYPE (GstMpegtsSIT, gst_mpegts_sit,
    (GBoxedCopyFunc) _gst_mpegts_sit_copy, (GFreeFunc) _gst_mpegts_sit_free);


static gpointer
_parse_sit (GstMpegtsSection * section)
{
  GstMpegtsSIT *sit = NULL;
  guint allocated_services = 8;
  guint8 *data, *end, *entry_begin;
  guint sit_info_length;
  guint descriptors_loop_length;

  GST_DEBUG ("SIT");

  sit = g_new0 (GstMpegtsSIT, 1);

  data = section->data;
  end = data + section->section_length;

  /* Skip common fields */
  data += 8;

  descriptors_loop_length = GST_READ_UINT16_BE (data) & 0x0fff;
  data += 2;
  sit->descriptors =
      gst_mpegts_parse_descriptors (data, descriptors_loop_length);
  if (sit->descriptors == NULL)
    goto error;
  data += descriptors_loop_length;

  sit_info_length = end - data;;
  sit->services = g_ptr_array_new_full (allocated_services,
      (GDestroyNotify) _gst_mpegts_sit_service_free);

  /* read up to the CRC */
  while (sit_info_length - 4 > 0) {
    GstMpegtsSITService *service = g_new0 (GstMpegtsSITService, 1);
    g_ptr_array_add (sit->services, service);

    entry_begin = data;

    if (sit_info_length - 4 < 4) {
      /* each entry must be at least 4 bytes (+4 bytes for the CRC) */
      GST_WARNING ("PID %d invalid SIT entry size %d",
          section->pid, sit_info_length);
      goto error;
    }

    service->service_id = GST_READ_UINT16_BE (data);
    data += 2;

    service->running_status = (*data >> 5) & 0x07;
    descriptors_loop_length = GST_READ_UINT16_BE (data) & 0x0fff;
    data += 2;

    if (descriptors_loop_length && (data + descriptors_loop_length > end - 4)) {
      GST_WARNING ("PID %d invalid SIT entry %d descriptors loop length %d",
          section->pid, service->service_id, descriptors_loop_length);
      goto error;
    }
    service->descriptors =
        gst_mpegts_parse_descriptors (data, descriptors_loop_length);
    if (!service->descriptors)
      goto error;
    data += descriptors_loop_length;

    sit_info_length -= data - entry_begin;
  }

  if (data != end - 4) {
    GST_WARNING ("PID %d invalid SIT parsed %d length %d",
        section->pid, (gint) (data - section->data), section->section_length);
    goto error;
  }

  return sit;

error:
  if (sit)
    _gst_mpegts_sit_free (sit);

  return NULL;
}

/**
 * gst_mpegts_section_get_sit:
 * @section: a #GstMpegtsSection of type %GST_MPEGTS_SECTION_SIT
 *
 * Returns the #GstMpegtsSIT contained in the @section.
 *
 * Returns: The #GstMpegtsSIT contained in the section, or %NULL if an error
 * happened.
 *
 * Since: 1.20
 */
const GstMpegtsSIT *
gst_mpegts_section_get_sit (GstMpegtsSection * section)
{
  g_return_val_if_fail (section->section_type == GST_MPEGTS_SECTION_SIT, NULL);
  g_return_val_if_fail (section->cached_parsed || section->data, NULL);

  if (!section->cached_parsed)
    section->cached_parsed =
        __common_section_checks (section, 18, _parse_sit,
        (GDestroyNotify) _gst_mpegts_sit_free);

  return (const GstMpegtsSIT *) section->cached_parsed;
}
