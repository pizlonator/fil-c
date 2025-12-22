/*
 * gst-scte-section.c -
 * Copyright (C) 2019 Centricular ltd
 *  Author: Edward Hervey <edward@centricular.com>
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
#define MPEGTIME_TO_GSTTIME(t) ((t) * (guint64)100000 / 9)

/**
 * SECTION:gst-scte-section
 * @title: SCTE variants of MPEG-TS sections
 * @short_description: Sections for the various SCTE specifications
 * @include: gst/mpegts/mpegts.h
 *
 * This contains the %GstMpegtsSection relevent to SCTE specifications.
 */

/* TODO: port to gst_bit_reader / gst_bit_writer */

/* Splice Information Table (SIT) */

static GstMpegtsSCTESpliceEvent *
_gst_mpegts_scte_splice_event_copy (GstMpegtsSCTESpliceEvent * event)
{
  GstMpegtsSCTESpliceEvent *copy =
      g_memdup2 (event, sizeof (GstMpegtsSCTESpliceEvent));

  copy->components = g_ptr_array_ref (event->components);

  return copy;
}

static void
_gst_mpegts_scte_splice_event_free (GstMpegtsSCTESpliceEvent * event)
{
  g_ptr_array_unref (event->components);
  g_free (event);
}

G_DEFINE_BOXED_TYPE (GstMpegtsSCTESpliceEvent, gst_mpegts_scte_splice_event,
    (GBoxedCopyFunc) _gst_mpegts_scte_splice_event_copy,
    (GFreeFunc) _gst_mpegts_scte_splice_event_free);

static GstMpegtsSCTESpliceComponent *
_gst_mpegts_scte_splice_component_copy (GstMpegtsSCTESpliceComponent *
    component)
{
  return g_memdup2 (component, sizeof (GstMpegtsSCTESpliceComponent));
}

static void
_gst_mpegts_scte_splice_component_free (GstMpegtsSCTESpliceComponent *
    component)
{
  g_free (component);
}

G_DEFINE_BOXED_TYPE (GstMpegtsSCTESpliceComponent,
    gst_mpegts_scte_splice_component,
    (GBoxedCopyFunc) _gst_mpegts_scte_splice_component_copy,
    (GFreeFunc) _gst_mpegts_scte_splice_component_free);

static GstMpegtsSCTESpliceComponent *
_parse_splice_component (GstMpegtsSCTESpliceEvent * event, guint8 ** orig_data,
    guint8 * end)
{
  GstMpegtsSCTESpliceComponent *component =
      g_new0 (GstMpegtsSCTESpliceComponent, 1);
  guint8 *data = *orig_data;

  if (data + 1 + 6 > end)
    goto error;

  component->tag = *data;
  data += 1;

  if (event->insert_event && event->splice_immediate_flag == 0) {
    component->splice_time_specified = *data >> 7;
    if (component->splice_time_specified) {
      component->splice_time = ((guint64) (*data & 0x01)) << 32;
      data += 1;
      component->splice_time += GST_READ_UINT32_BE (data);
      data += 4;
      GST_LOG ("component %u splice_time %" G_GUINT64_FORMAT " (%"
          GST_TIME_FORMAT ")", component->tag, component->splice_time,
          GST_TIME_ARGS (MPEGTIME_TO_GSTTIME (component->splice_time)));
    } else {
      data += 1;
    }
  } else if (!event->insert_event) {
    component->utc_splice_time = GST_READ_UINT32_BE (data);
    GST_LOG ("component %u utc_splice_time %u", component->tag,
        component->utc_splice_time);
    data += 4;
  }

  *orig_data = data;

  return component;

error:
  {
    if (event)
      _gst_mpegts_scte_splice_event_free (event);
    return NULL;
  }
}

static GstMpegtsSCTESpliceEvent *
_parse_splice_event (guint8 ** orig_data, guint8 * end, gboolean insert_event)
{
  GstMpegtsSCTESpliceEvent *event = g_new0 (GstMpegtsSCTESpliceEvent, 1);
  guint8 *data = *orig_data;

  /* Note : +6 is because of the final descriptor_loop_length and CRC */
  if (data + 5 + 6 > end)
    goto error;

  event->components = g_ptr_array_new_with_free_func ((GDestroyNotify)
      _gst_mpegts_scte_splice_component_free);

  event->insert_event = insert_event;
  event->splice_event_id = GST_READ_UINT32_BE (data);
  GST_LOG ("splice_event_id: 0x%08x", event->splice_event_id);
  data += 4;
  event->splice_event_cancel_indicator = *data >> 7;
  GST_LOG ("splice_event_cancel_indicator: %d",
      event->splice_event_cancel_indicator);
  data += 1;

  GST_DEBUG ("data %p", data);

  if (event->splice_event_cancel_indicator == 0) {
    if (data + 5 + 6 > end)
      goto error;
    event->out_of_network_indicator = *data >> 7;
    event->program_splice_flag = (*data >> 6) & 0x01;
    event->duration_flag = (*data >> 5) & 0x01;

    if (insert_event)
      event->splice_immediate_flag = (*data >> 4) & 0x01;

    GST_LOG ("out_of_network_indicator:%d", event->out_of_network_indicator);
    GST_LOG ("program_splice_flag:%d", event->program_splice_flag);
    GST_LOG ("duration_flag:%d", event->duration_flag);

    if (insert_event)
      GST_LOG ("splice_immediate_flag:%d", event->splice_immediate_flag);

    data += 1;

    if (event->program_splice_flag == 0) {
      guint component_count = *data;
      guint i;

      data += 1;

      for (i = 0; i < component_count; i++) {
        GstMpegtsSCTESpliceComponent *component =
            _parse_splice_component (event, &data, end);
        if (component == NULL)
          goto error;
        g_ptr_array_add (event->components, component);
      }
    } else {
      if (insert_event && event->splice_immediate_flag == 0) {
        event->program_splice_time_specified = *data >> 7;
        if (event->program_splice_time_specified) {
          event->program_splice_time = ((guint64) (*data & 0x01)) << 32;
          data += 1;
          event->program_splice_time += GST_READ_UINT32_BE (data);
          data += 4;
          GST_LOG ("program_splice_time %" G_GUINT64_FORMAT " (%"
              GST_TIME_FORMAT ")", event->program_splice_time,
              GST_TIME_ARGS (MPEGTIME_TO_GSTTIME (event->program_splice_time)));
        } else
          data += 1;
      } else if (!insert_event) {
        event->utc_splice_time = GST_READ_UINT32_BE (data);
        GST_LOG ("utc_splice_time %u", event->utc_splice_time);
        data += 4;
      }
    }

    if (event->duration_flag) {
      event->break_duration_auto_return = *data >> 7;
      event->break_duration = ((guint64) (*data & 0x01)) << 32;
      data += 1;
      event->break_duration += GST_READ_UINT32_BE (data);
      data += 4;
      GST_LOG ("break_duration_auto_return:%d",
          event->break_duration_auto_return);
      GST_LOG ("break_duration %" G_GUINT64_FORMAT " (%" GST_TIME_FORMAT ")",
          event->break_duration,
          GST_TIME_ARGS (MPEGTIME_TO_GSTTIME (event->break_duration)));
    }

    event->unique_program_id = GST_READ_UINT16_BE (data);
    GST_LOG ("unique_program_id:%" G_GUINT16_FORMAT, event->unique_program_id);
    data += 2;
    event->avail_num = *data++;
    event->avails_expected = *data++;
    GST_LOG ("avail %d/%d", event->avail_num, event->avails_expected);
  }

  GST_DEBUG ("done");
  *orig_data = data;
  return event;

error:
  {
    if (event)
      _gst_mpegts_scte_splice_event_free (event);
    return NULL;
  }
}

static GstMpegtsSCTESIT *
_gst_mpegts_scte_sit_copy (GstMpegtsSCTESIT * sit)
{
  GstMpegtsSCTESIT *copy = g_memdup2 (sit, sizeof (GstMpegtsSCTESIT));

  copy->splices = g_ptr_array_ref (sit->splices);
  copy->descriptors = g_ptr_array_ref (sit->descriptors);

  return copy;
}

static void
_gst_mpegts_scte_sit_free (GstMpegtsSCTESIT * sit)
{
  g_ptr_array_unref (sit->splices);
  g_ptr_array_unref (sit->descriptors);
  g_free (sit);
}

G_DEFINE_BOXED_TYPE (GstMpegtsSCTESIT, gst_mpegts_scte_sit,
    (GBoxedCopyFunc) _gst_mpegts_scte_sit_copy,
    (GFreeFunc) _gst_mpegts_scte_sit_free);


static gpointer
_parse_sit (GstMpegtsSection * section)
{
  GstMpegtsSCTESIT *sit = NULL;
  guint8 *data, *end;
  guint32 tmp;

  GST_DEBUG ("SIT");

  /* Even if the section is not a short one, it still uses CRC */
  if (_calc_crc32 (section->data, section->section_length) != 0) {
    GST_WARNING ("PID:0x%04x table_id:0x%02x, Bad CRC on section", section->pid,
        section->table_id);
    return NULL;
  }

  sit = g_new0 (GstMpegtsSCTESIT, 1);

  sit->fully_parsed = FALSE;

  data = section->data;
  end = data + section->section_length;

  GST_MEMDUMP ("section", data, section->section_length);

  /* Skip already-checked fields */
  data += 3;

  /* Ensure protocol_version is 0 */
  if (*data != 0) {
    GST_WARNING ("Unsupported SCTE SIT protocol version (%d)", *data);
    goto error;
  }
  data += 1;

  /* encryption */
  sit->encrypted_packet = (*data) >> 7;
  sit->encryption_algorithm = (*data) & 0x3f;
  sit->pts_adjustment = ((guint64) (*data & 0x01)) << 32;
  data += 1;

  sit->pts_adjustment += GST_READ_UINT32_BE (data);
  data += 4;

  sit->cw_index = *data;
  data += 1;

  tmp = GST_READ_UINT24_BE (data);
  data += 3;

  sit->tier = (tmp >> 12);
  sit->splice_command_length = tmp & 0xfff;
  /* 0xfff is for backwards compatibility when reading */
  if (sit->splice_command_length == 0xfff)
    sit->splice_command_length = 0;
  GST_LOG ("command length %d", sit->splice_command_length);

  if (sit->encrypted_packet) {
    GST_LOG ("Encrypted SIT, parsed partially");
    goto done;
  }

  if (sit->splice_command_length
      && (data + sit->splice_command_length > end - 5)) {
    GST_WARNING ("PID %d invalid SCTE SIT splice command length %d",
        section->pid, sit->splice_command_length);
    goto error;
  }

  sit->splice_command_type = *data;
  data += 1;

  sit->splices = g_ptr_array_new_with_free_func ((GDestroyNotify)
      _gst_mpegts_scte_splice_event_free);
  switch (sit->splice_command_type) {
    case GST_MTS_SCTE_SPLICE_COMMAND_NULL:
    case GST_MTS_SCTE_SPLICE_COMMAND_BANDWIDTH:
      /* These commands have no payload */
      break;
    case GST_MTS_SCTE_SPLICE_COMMAND_TIME:
    {
      sit->splice_time_specified = (*data >> 7);
      if (sit->splice_time_specified) {
        sit->splice_time = ((guint64) (*data & 0x01)) << 32;
        data += 1;
        sit->splice_time += GST_READ_UINT32_BE (data);
        data += 4;
      } else
        data += 1;
    }
      break;
    case GST_MTS_SCTE_SPLICE_COMMAND_SCHEDULE:
    {
      guint i;
      guint splice_count = *data;
      data += 1;

      for (i = 0; i < splice_count; i++) {
        GstMpegtsSCTESpliceEvent *event =
            _parse_splice_event (&data, end, FALSE);
        if (event == NULL)
          goto error;
        g_ptr_array_add (sit->splices, event);
      }
    }
      break;
    case GST_MTS_SCTE_SPLICE_COMMAND_INSERT:
    {
      GstMpegtsSCTESpliceEvent *event = _parse_splice_event (&data, end, TRUE);
      if (event == NULL)
        goto error;
      g_ptr_array_add (sit->splices, event);
    }
      break;
    case GST_MTS_SCTE_SPLICE_COMMAND_PRIVATE:
    {
      GST_FIXME ("Implement SCTE-35 private commands");
      data += sit->splice_command_length;
    }
      break;
    default:
      GST_WARNING ("Unknown SCTE splice command type (0x%02x) !",
          sit->splice_command_type);
      break;;
  }

  /* descriptors */
  tmp = GST_READ_UINT16_BE (data);
  data += 2;
  GST_MEMDUMP ("desc ?", data, tmp);
  sit->descriptors = gst_mpegts_parse_descriptors (data, tmp);
  if (!sit->descriptors) {
    GST_DEBUG ("no descriptors %d", tmp);
    goto error;
  }
  data += tmp;

  GST_DEBUG ("%p - %p", data, end);
  if (data != end - 4) {
    GST_WARNING ("PID %d invalid SIT parsed %d length %d",
        section->pid, (gint) (data - section->data), section->section_length);
    goto error;
  }

  sit->fully_parsed = TRUE;

done:
  return sit;

error:
  if (sit) {
    _gst_mpegts_scte_sit_free (sit);
    sit = NULL;
  }

  goto done;
}

/**
 * gst_mpegts_section_get_scte_sit:
 * @section: a #GstMpegtsSection of type %GST_MPEGTS_SECTION_SCTE_SIT
 *
 * Returns the #GstMpegtsSCTESIT contained in the @section.
 *
 * Returns: The #GstMpegtsSCTESIT contained in the section, or %NULL if an error
 * happened.
 */
const GstMpegtsSCTESIT *
gst_mpegts_section_get_scte_sit (GstMpegtsSection * section)
{
  g_return_val_if_fail (section->section_type == GST_MPEGTS_SECTION_SCTE_SIT,
      NULL);
  g_return_val_if_fail (section->cached_parsed || section->data, NULL);

  if (!section->cached_parsed)
    section->cached_parsed =
        __common_section_checks (section, 18, _parse_sit,
        (GDestroyNotify) _gst_mpegts_scte_sit_free);

  return (const GstMpegtsSCTESIT *) section->cached_parsed;
}

/**
 * gst_mpegts_scte_sit_new:
 *
 * Allocates and initializes a #GstMpegtsSCTESIT.
 *
 * Returns: (transfer full): A newly allocated #GstMpegtsSCTESIT
 */
GstMpegtsSCTESIT *
gst_mpegts_scte_sit_new (void)
{
  GstMpegtsSCTESIT *sit;

  sit = g_new0 (GstMpegtsSCTESIT, 1);

  /* Set all default values (which aren't already 0/NULL) */
  sit->tier = 0xfff;
  sit->fully_parsed = TRUE;

  sit->splices = g_ptr_array_new_with_free_func ((GDestroyNotify)
      _gst_mpegts_scte_splice_event_free);
  sit->descriptors = g_ptr_array_new_with_free_func ((GDestroyNotify)
      gst_mpegts_descriptor_free);

  sit->is_running_time = TRUE;

  return sit;
}

/**
 * gst_mpegts_scte_null_new:
 *
 * Allocates and initializes a NULL command #GstMpegtsSCTESIT.
 *
 * Returns: (transfer full): A newly allocated #GstMpegtsSCTESIT
 */
GstMpegtsSCTESIT *
gst_mpegts_scte_null_new (void)
{
  GstMpegtsSCTESIT *sit = gst_mpegts_scte_sit_new ();

  sit->splice_command_type = GST_MTS_SCTE_SPLICE_COMMAND_NULL;

  sit->is_running_time = TRUE;

  return sit;
}

/**
 * gst_mpegts_scte_cancel_new:
 * @event_id: The event ID to cancel.
 *
 * Allocates and initializes a new INSERT command #GstMpegtsSCTESIT
 * setup to cancel the specified @event_id.
 *
 * Returns: (transfer full): A newly allocated #GstMpegtsSCTESIT
 */
GstMpegtsSCTESIT *
gst_mpegts_scte_cancel_new (guint32 event_id)
{
  GstMpegtsSCTESIT *sit = gst_mpegts_scte_sit_new ();
  GstMpegtsSCTESpliceEvent *event = gst_mpegts_scte_splice_event_new ();

  sit->splice_command_type = GST_MTS_SCTE_SPLICE_COMMAND_INSERT;
  event->splice_event_id = event_id;
  event->splice_event_cancel_indicator = TRUE;
  g_ptr_array_add (sit->splices, event);

  sit->is_running_time = TRUE;

  return sit;
}

/**
 * gst_mpegts_scte_splice_in_new:
 * @event_id: The event ID.
 * @splice_time: The running time for the splice event
 *
 * Allocates and initializes a new "Splice In" INSERT command
 * #GstMpegtsSCTESIT for the given @event_id and @splice_time.
 *
 * If the @splice_time is #G_MAXUINT64 then the event will be
 * immediate as opposed to for the target @splice_time.
 *
 * Returns: (transfer full): A newly allocated #GstMpegtsSCTESIT
 */
GstMpegtsSCTESIT *
gst_mpegts_scte_splice_in_new (guint32 event_id, GstClockTime splice_time)
{
  GstMpegtsSCTESIT *sit = gst_mpegts_scte_sit_new ();
  GstMpegtsSCTESpliceEvent *event = gst_mpegts_scte_splice_event_new ();

  sit->splice_command_type = GST_MTS_SCTE_SPLICE_COMMAND_INSERT;
  event->splice_event_id = event_id;
  event->insert_event = TRUE;
  if (splice_time == G_MAXUINT64) {
    event->splice_immediate_flag = TRUE;
  } else {
    event->program_splice_time_specified = TRUE;
    event->program_splice_time = splice_time;
  }
  g_ptr_array_add (sit->splices, event);

  sit->is_running_time = TRUE;

  return sit;
}

/**
 * gst_mpegts_scte_splice_out_new:
 * @event_id: The event ID.
 * @splice_time: The running time for the splice event
 * @duration: The optional duration.
 *
 * Allocates and initializes a new "Splice Out" INSERT command
 * #GstMpegtsSCTESIT for the given @event_id, @splice_time and
 * @duration.
 *
 * If the @splice_time is #G_MAXUINT64 then the event will be
 * immediate as opposed to for the target @splice_time.
 *
 * If the @duration is 0 it won't be specified in the event.
 *
 * Returns: (transfer full): A newly allocated #GstMpegtsSCTESIT
 */
GstMpegtsSCTESIT *
gst_mpegts_scte_splice_out_new (guint32 event_id, GstClockTime splice_time,
    GstClockTime duration)
{
  GstMpegtsSCTESIT *sit = gst_mpegts_scte_sit_new ();
  GstMpegtsSCTESpliceEvent *event = gst_mpegts_scte_splice_event_new ();

  sit->splice_command_type = GST_MTS_SCTE_SPLICE_COMMAND_INSERT;
  event->splice_event_id = event_id;
  event->out_of_network_indicator = TRUE;
  event->insert_event = TRUE;
  if (splice_time == G_MAXUINT64) {
    event->splice_immediate_flag = TRUE;
  } else {
    event->program_splice_time_specified = TRUE;
    event->program_splice_time = splice_time;
  }
  if (duration != 0) {
    event->duration_flag = TRUE;
    event->break_duration = duration;
  }
  g_ptr_array_add (sit->splices, event);

  sit->is_running_time = TRUE;

  return sit;
}

/**
 * gst_mpegts_scte_splice_event_new:
 *
 * Allocates and initializes a #GstMpegtsSCTESpliceEvent.
 *
 * Returns: (transfer full): A newly allocated #GstMpegtsSCTESpliceEvent
 */
GstMpegtsSCTESpliceEvent *
gst_mpegts_scte_splice_event_new (void)
{
  GstMpegtsSCTESpliceEvent *event = g_new0 (GstMpegtsSCTESpliceEvent, 1);

  /* Non-0 Default values */
  event->program_splice_flag = TRUE;
  event->components = g_ptr_array_new_with_free_func ((GDestroyNotify)
      _gst_mpegts_scte_splice_component_free);


  return event;
}

/**
 * gst_mpegts_scte_splice_component_new:
 * @tag: the elementary PID stream identifier
 *
 * Allocates and initializes a #GstMpegtsSCTESpliceComponent.
 *
 * Returns: (transfer full): A newly allocated #GstMpegtsSCTESpliceComponent
 * Since: 1.20
 */
GstMpegtsSCTESpliceComponent *
gst_mpegts_scte_splice_component_new (guint8 tag)
{
  GstMpegtsSCTESpliceComponent *component =
      g_new0 (GstMpegtsSCTESpliceComponent, 1);

  component->tag = tag;

  return component;
}

static gboolean
_packetize_sit (GstMpegtsSection * section)
{
  gsize length, command_length, descriptor_length;
  const GstMpegtsSCTESIT *sit;
  GstMpegtsDescriptor *descriptor;
  guint32 tmp32;
  guint i;
  guint8 *data;

  sit = gst_mpegts_section_get_scte_sit (section);

  if (sit == NULL)
    return FALSE;

  if (sit->fully_parsed == FALSE) {
    GST_WARNING ("Attempted to packetize an incompletely parsed SIT");
    return FALSE;
  }

  /* Skip cases we don't handle for now */
  if (sit->encrypted_packet) {
    GST_WARNING ("SCTE encrypted packet is not supported");
    return FALSE;
  }

  switch (sit->splice_command_type) {
    case GST_MTS_SCTE_SPLICE_COMMAND_PRIVATE:
      GST_WARNING ("SCTE command not supported");
      return FALSE;
    default:
      break;
  }

  /* Smallest splice section are the NULL and bandwith command:
   * 14 bytes for the header
   * 0 bytes for the command
   * 2 bytes for the empty descriptor loop length
   * 4 bytes for the CRC */
  length = 20;

  command_length = 0;
  /* Add the size of splices */
  for (i = 0; i < sit->splices->len; i++) {
    GstMpegtsSCTESpliceEvent *event = g_ptr_array_index (sit->splices, i);
    /* There is at least 5 bytes */
    command_length += 5;
    if (!event->splice_event_cancel_indicator) {
      /* Add at least 5 bytes for common fields */
      command_length += 5;

      if (event->program_splice_flag) {
        if (event->insert_event) {
          if (!event->splice_immediate_flag) {
            if (event->program_splice_time_specified)
              command_length += 5;
            else
              command_length += 1;
          }
        } else {
          /* Schedule events, 4 bytes for utc_splice_time */
          command_length += 4;
        }
      } else {
        guint j;

        /* component_count */
        command_length += 1;

        for (j = 0; j < event->components->len; j++) {
          GstMpegtsSCTESpliceComponent *component =
              g_ptr_array_index (event->components, j);

          /* component_tag */
          command_length += 1;
          if (event->insert_event) {
            if (!event->splice_immediate_flag) {
              if (component->splice_time_specified)
                command_length += 5;
              else
                command_length += 1;
            }
          } else {
            /* utc_splice_time */
            command_length += 4;
          }
        }
      }

      if (event->duration_flag)
        command_length += 5;
    }
  }

  if (sit->splice_command_type == GST_MTS_SCTE_SPLICE_COMMAND_TIME) {
    if (sit->splice_time_specified)
      command_length += 5;
    else
      command_length += 1;
  }

  length += command_length;

  /* Calculate size of descriptors */

  descriptor_length = 0;
  for (i = 0; i < sit->descriptors->len; i++) {
    descriptor = g_ptr_array_index (sit->descriptors, i);
    descriptor_length += descriptor->length + 2;
  }
  length += descriptor_length;

  /* Max length of SIT section is 4093 bytes */
  g_return_val_if_fail (length <= 4093, FALSE);

  _packetize_common_section (section, length);

  data = section->data + 3;
  /* Protocol version (default 0) */
  *data++ = 0;
  /* 7bits for encryption (not supported : 0) */
  /* 33bits for pts_adjustment */
  *data++ = (sit->pts_adjustment) >> 32 & 0x01;
  GST_WRITE_UINT32_BE (data, sit->pts_adjustment & 0xffffffff);
  data += 4;
  /* cw_index : 8 bit */
  *data++ = sit->cw_index;
  /* tier                  : 12bit
   * splice_command_length : 12bit
   * splice_command_type   : 8 bit */
  tmp32 = (sit->tier & 0xfff) << 20;
  tmp32 |= (command_length & 0xfff) << 8;
  tmp32 |= sit->splice_command_type;
  GST_WRITE_UINT32_BE (data, tmp32);
  data += 4;

  if (sit->splice_command_type == GST_MTS_SCTE_SPLICE_COMMAND_TIME) {
    if (!sit->splice_time_specified) {
      *data++ = 0x7f;
    } else {
      *data++ = 0xf2 | ((sit->splice_time >> 32) & 0x1);
      GST_WRITE_UINT32_BE (data, sit->splice_time & 0xffffffff);
      data += 4;
    }
  }

  /* Write the events */
  for (i = 0; i < sit->splices->len; i++) {
    GstMpegtsSCTESpliceEvent *event = g_ptr_array_index (sit->splices, i);

    /* splice_event_id : 32bit */
    GST_WRITE_UINT32_BE (data, event->splice_event_id);
    data += 4;
    /* splice_event_cancel_indicator : 1bit
     * reserved                      ; 7bit */
    *data++ = event->splice_event_cancel_indicator ? 0xff : 0x7f;

    if (!event->splice_event_cancel_indicator) {
      /* out_of_network_indicator : 1bit
       * program_splice_flag      : 1bit
       * duration_flag            : 1bit
       * splice_immediate_flag    : 1bit
       * reserved                 : 4bit */
      *data++ = (event->out_of_network_indicator << 7) |
          (event->program_splice_flag << 6) |
          (event->duration_flag << 5) |
          (event->insert_event ? (event->splice_immediate_flag << 4) : 0) |
          0x0f;
      if (event->program_splice_flag) {
        if (event->insert_event) {
          if (!event->splice_immediate_flag) {
            /* program_splice_time_specified : 1bit
             * reserved : 6/7 bit */
            if (!event->program_splice_time_specified)
              *data++ = 0x7f;
            else {
              /* time : 33bit */
              *data++ = 0xf2 | ((event->program_splice_time >> 32) & 0x1);
              GST_WRITE_UINT32_BE (data,
                  event->program_splice_time & 0xffffffff);
              data += 4;
            }
          }
        } else {
          GST_WRITE_UINT32_BE (data, event->utc_splice_time);
          data += 4;
        }
      } else {
        guint j;

        *data++ = event->components->len & 0xff;

        for (j = 0; j < event->components->len; j++) {
          GstMpegtsSCTESpliceComponent *component =
              g_ptr_array_index (event->components, j);

          *data++ = component->tag;

          if (event->insert_event) {
            if (!event->splice_immediate_flag) {
              /* program_splice_time_specified : 1bit
               * reserved : 6/7 bit */
              if (!component->splice_time_specified)
                *data++ = 0x7f;
              else {
                /* time : 33bit */
                *data++ = 0xf2 | ((component->splice_time >> 32) & 0x1);
                GST_WRITE_UINT32_BE (data, component->splice_time & 0xffffffff);
                data += 4;
              }
            }
          } else {
            GST_WRITE_UINT32_BE (data, component->utc_splice_time);
            data += 4;
          }
        }
      }

      if (event->duration_flag) {
        *data = event->break_duration_auto_return ? 0xfe : 0x7e;
        *data++ |= (event->break_duration >> 32) & 0x1;
        GST_WRITE_UINT32_BE (data, event->break_duration & 0xffffffff);
        data += 4;
      }
      /* unique_program_id : 16bit */
      GST_WRITE_UINT16_BE (data, event->unique_program_id);
      data += 2;
      /* avail_num : 8bit */
      *data++ = event->avail_num;
      /* avails_expected : 8bit */
      *data++ = event->avails_expected;
    }
  }

  /* Descriptors */
  GST_WRITE_UINT16_BE (data, descriptor_length);
  data += 2;
  _packetize_descriptor_array (sit->descriptors, &data);

  /* CALCULATE AND WRITE THE CRC ! */
  GST_WRITE_UINT32_BE (data, _calc_crc32 (section->data, data - section->data));

  return TRUE;
}

/**
 * gst_mpegts_section_from_scte_sit:
 * @sit: (transfer full): a #GstMpegtsSCTESIT to create the #GstMpegtsSection from
 *
 * Ownership of @sit is taken. The data in @sit is managed by the #GstMpegtsSection
 *
 * Returns: (transfer full): the #GstMpegtsSection
 */
GstMpegtsSection *
gst_mpegts_section_from_scte_sit (GstMpegtsSCTESIT * sit, guint16 pid)
{
  GstMpegtsSection *section;

  g_return_val_if_fail (sit != NULL, NULL);

  section = _gst_mpegts_section_init (pid, GST_MTS_TABLE_ID_SCTE_SPLICE);

  section->short_section = TRUE;
  section->cached_parsed = (gpointer) sit;
  section->packetizer = _packetize_sit;
  section->destroy_parsed = (GDestroyNotify) _gst_mpegts_scte_sit_free;
  section->short_section = TRUE;

  return section;
}
