 /*
  * This library is licensed under 2 different licenses and you
  * can choose to use it under the terms of either one of them. The
  * two licenses are the MPL 1.1 and the LGPL.
  *
  * MPL:
  *
  * The contents of this file are subject to the Mozilla Public License
  * Version 1.1 (the "License"); you may not use this file except in
  * compliance with the License. You may obtain a copy of the License at
  * http://www.mozilla.org/MPL/.
  *
  * Software distributed under the License is distributed on an "AS IS"
  * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See the
  * License for the specific language governing rights and limitations
  * under the License.
  *
  * LGPL:
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
  *
  * The Original Code is Fluendo MPEG Demuxer plugin.
  *
  * The Initial Developer of the Original Code is Fluendo, S.L.
  * Portions created by Fluendo, S.L. are Copyright (C) 2005
  * Fluendo, S.L. All Rights Reserved.
  *
  * Contributor(s): Wim Taymans <wim@fluendo.com>
  */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gstmpegdefs.h"
#include "gstpesfilter.h"

GST_DEBUG_CATEGORY (mpegpspesfilter_debug);
#define GST_CAT_DEFAULT (mpegpspesfilter_debug)

static GstFlowReturn gst_pes_filter_data_push (GstPESFilter * filter,
    gboolean first, GstBuffer * buffer);

#define ADAPTER_OFFSET_FLUSH(_bytes_)  if (filter->adapter_offset) *filter->adapter_offset = *filter->adapter_offset + (_bytes_)

/* May pass null for adapter to have the filter create one */
void
gst_pes_filter_init (GstPESFilter * filter, GstAdapter * adapter,
    guint64 * adapter_offset)
{
  g_return_if_fail (filter != NULL);

  if (adapter != NULL)
    g_object_ref (adapter);
  else
    adapter = gst_adapter_new ();

  filter->adapter = adapter;
  filter->adapter_offset = adapter_offset;
  filter->state = STATE_HEADER_PARSE;
  filter->gather_pes = FALSE;
  filter->allow_unbounded = FALSE;
}

void
gst_pes_filter_uninit (GstPESFilter * filter)
{
  g_return_if_fail (filter != NULL);

  if (filter->adapter)
    g_object_unref (filter->adapter);
  filter->adapter = NULL;
  filter->adapter_offset = NULL;
}

void
gst_pes_filter_set_callbacks (GstPESFilter * filter,
    GstPESFilterData data_cb, GstPESFilterResync resync_cb, gpointer user_data)
{
  g_return_if_fail (filter != NULL);

  filter->data_cb = data_cb;
  filter->resync_cb = resync_cb;
  filter->user_data = user_data;
}

static gboolean
gst_pes_filter_is_sync (guint32 sync)
{
  return ((sync & 0xfffffffc) == 0x000001bc) ||
      ((sync & 0xfffffffd) == 0x000001bd) ||
      ((sync & 0xffffffe0) == 0x000001c0) ||
      ((sync & 0xfffffff0) == 0x000001f0) ||
      ((sync & 0xfffffff0) == 0x000001e0);
}

static GstFlowReturn
gst_pes_filter_parse (GstPESFilter * filter)
{
  GstFlowReturn ret;
  guint32 start_code;

  gboolean STD_buffer_bound_scale G_GNUC_UNUSED;
  guint16 STD_buffer_size_bound G_GNUC_UNUSED;
  const guint8 *data;
  gint avail, datalen;
  gboolean have_size = FALSE;

  avail = gst_adapter_available (filter->adapter);

  if (avail < 6)
    goto need_more_data;

  data = gst_adapter_map (filter->adapter, 6);

  /* read start code and length */

  /* get start code */
  start_code = GST_READ_UINT32_BE (data);
  if (!gst_pes_filter_is_sync (start_code))
    goto lost_sync;

  filter->start_code = start_code;
  filter->id = data[3];

  /* skip start code */
  data += 4;

  /* start parsing length */
  filter->length = GST_READ_UINT16_BE (data);

  GST_DEBUG ("id 0x%02x length %d, avail %d start code 0x%02x", filter->id,
      filter->length, avail, filter->start_code);

  /* A data length of 0 indicates an unbounded packet in transport
   * streams, but actually a 0 sized packet in program streams or
   * for anything except video packets */

  /* FIXME: Remove this hack that is checking start_code. Instead, we need
   * a callback that a start_code has been collected, giving the caller a chance
   * to set the allow_unbounded flag if they want */
  if (filter->length == 0 &&
      ((filter->start_code & 0xFFFFFFF0) == PACKET_VIDEO_START_CODE ||
          filter->start_code == ID_EXTENDED_STREAM_ID ||
          filter->allow_unbounded)) {
    GST_DEBUG ("id 0x%02x, unbounded length", filter->id);
    filter->unbounded_packet = TRUE;
  } else {
    filter->unbounded_packet = FALSE;

    if (filter->gather_pes && avail < filter->length + 6) {
      GST_DEBUG ("id 0x%02x, bounded length %d, only have %d",
          filter->id, filter->length + 6, avail);
      goto need_more_data;
    }

    /* if we need more data from now on, we lost sync */
    avail = MIN (avail, filter->length + 6);
  }

  if (avail < 6)
    goto need_more_data;

  gst_adapter_unmap (filter->adapter);

  /* read more data, either the whole packet if there is a length
   * or whatever we have available if this in an unbounded packet. */
  data = gst_adapter_map (filter->adapter, avail);

  /* This will make us flag LOST_SYNC if we run out of data from here onward */
  have_size = TRUE;

  /* skip start code and length */
  data += 6;
  datalen = avail - 6;

  GST_DEBUG ("datalen %d", datalen);

  switch (filter->start_code) {
    case ID_PS_PROGRAM_STREAM_MAP:
    case ID_PRIVATE_STREAM_2:
    case ID_ECM_STREAM:
    case ID_EMM_STREAM:
    case ID_PROGRAM_STREAM_DIRECTORY:
    case ID_DSMCC_STREAM:
    case ID_ITU_TREC_H222_TYPE_E_STREAM:
      /* Push directly out */
      goto push_out;
    case ID_PADDING_STREAM:
      GST_DEBUG ("skipping padding stream");
      goto skip;
    default:
      break;
  }

  if (datalen == 0)
    goto need_more_data;
  filter->pts = filter->dts = -1;

  /* stuffing bits, first two bits are '10' for mpeg2 pes so this code is
   * not triggered. */
  while (TRUE) {
    if (*data != 0xff)
      break;

    data++;
    datalen--;

    GST_DEBUG ("got stuffing bit");

    if (datalen < 1)
      goto need_more_data;
  }

  /* STD buffer size, never for mpeg2 */
  if ((*data & 0xc0) == 0x40) {
    GST_DEBUG ("have STD");

    if (datalen < 3)
      goto need_more_data;

    STD_buffer_bound_scale = *data & 0x20;
    STD_buffer_size_bound = ((guint16) (*data++ & 0x1F)) << 8;
    STD_buffer_size_bound |= *data++;

    datalen -= 2;
  }

  /* PTS but no DTS, never for mpeg2 */
  if ((*data & 0xf0) == 0x20) {
    GST_DEBUG ("PTS without DTS");

    if (datalen < 5)
      goto need_more_data;
    READ_TS (data, filter->pts, lost_sync);
    GST_DEBUG ("PTS found %" G_GUINT64_FORMAT, filter->pts);
    datalen -= 5;
  }
  /* PTS and DTS, never for mpeg2 */
  else if ((*data & 0xf0) == 0x30) {
    GST_DEBUG ("PTS and DTS");

    if (datalen < 10)
      goto need_more_data;
    READ_TS (data, filter->pts, lost_sync);
    READ_TS (data, filter->dts, lost_sync);
    GST_DEBUG ("PTS found %" G_GUINT64_FORMAT, filter->pts);
    GST_DEBUG ("DTS found %" G_GUINT64_FORMAT, filter->dts);
    datalen -= 10;
  } else if ((*data & 0xc0) == 0x80) {
    /* mpeg2 case */
    guchar flags;
    guint8 header_data_length = 0;

    GST_DEBUG ("MPEG2 PES packet");

    if (datalen < 3)
      goto need_more_data;
    /* 2: '10'
     * 2: PES_scrambling_control
     * 1: PES_priority
     * 1: data_alignment_indicator
     * 1: copyright
     * 1: original_or_copy
     */
    flags = *data++;

    GST_DEBUG ("flags: 0x%02x", flags);
    if ((flags & 0xc0) != 0x80)
      goto lost_sync;

    /* check PES scrambling control */
    if ((flags & 0x30) != 0)
      GST_DEBUG ("PES scrambling control: %x", (flags >> 4) & 0x3);

    /* 2: PTS_DTS_flags
     * 1: ESCR_flag
     * 1: ES_rate_flag
     * 1: DSM_trick_mode_flag
     * 1: additional_copy_info_flag
     * 1: PES_CRC_flag
     * 1: PES_extension_flag
     */
    flags = *data++;

    /* 8: PES_header_data_length */
    header_data_length = *data++;
    datalen -= 3;

    GST_DEBUG ("header_data_length: %d, flags 0x%02x",
        header_data_length, flags);

    if (header_data_length > datalen)
      goto need_more_data;

    /* only DTS: this is invalid */
    if ((flags & 0xc0) == 0x40)
      goto lost_sync;

    /* check for PTS */
    if ((flags & 0x80)) {
      if (datalen < 5)
        goto need_more_data;

      READ_TS (data, filter->pts, lost_sync);
      GST_DEBUG ("PTS found %" G_GUINT64_FORMAT, filter->pts);
      header_data_length -= 5;
      datalen -= 5;
    }
    /* check for DTS */
    if ((flags & 0x40)) {
      READ_TS (data, filter->dts, lost_sync);
      if (datalen < 5)
        goto need_more_data;
      GST_DEBUG ("DTS found %" G_GUINT64_FORMAT, filter->dts);
      header_data_length -= 5;
      datalen -= 5;
    }
    /* ESCR_flag */
    if ((flags & 0x20)) {
      GST_DEBUG ("%x ESCR found", filter->id);
      if (datalen < 6)
        goto need_more_data;
      data += 6;
      header_data_length -= 6;
      datalen -= 6;
    }
    /* ES_rate_flag */
    if ((flags & 0x10)) {
      guint32 es_rate;

      if (datalen < 3)
        goto need_more_data;

      es_rate = ((guint32) (*data++ & 0x07)) << 14;
      es_rate |= ((guint32) (*data++)) << 7;
      es_rate |= ((guint32) (*data++ & 0xFE)) >> 1;
      GST_DEBUG ("%x ES Rate found %u", filter->id, es_rate);
      header_data_length -= 3;
      datalen -= 3;
    }
    /* DSM_trick_mode_flag */
    if ((flags & 0x08)) {
      guint8 trick_mode_flags;

      if (datalen < 1)
        goto need_more_data;

      /* 3: trick_mode_control */
      trick_mode_flags = *data++;
      GST_DEBUG ("%x DSM trick mode found, flags 0x%02x", filter->id,
          trick_mode_flags);

      /* fast_forward */
      if ((trick_mode_flags & 0xe0) == 0x00) {
      }
      /* slow motion */
      else if ((trick_mode_flags & 0xe0) == 0x20) {
      }
      /* freeze frame */
      else if ((trick_mode_flags & 0xe0) == 0x40) {
      }
      /* fast reverse */
      else if ((trick_mode_flags & 0xe0) == 0x60) {
      }
      /* slow reverse */
      else if ((trick_mode_flags & 0xe0) == 0x80) {
      }
      /* reserved */
      else {
      }

      header_data_length -= 1;
      datalen -= 1;
    }
    /* additional_copy_info_flag  */
    if ((flags & 0x04)) {
      GST_DEBUG ("%x additional copy info, flags 0x%02x", filter->id, *data);
    }
    /* PES_CRC_flag  */
    if ((flags & 0x02)) {
      GST_DEBUG ("%x PES_CRC", filter->id);
    }
    /* PES_extension_flag  */
    if ((flags & 0x01)) {
      flags = *data++;
      header_data_length -= 1;
      datalen -= 1;
      GST_DEBUG ("%x PES_extension, flags 0x%02x", filter->id, flags);
      /* PES_private_data_flag */
      if ((flags & 0x80)) {
        GST_DEBUG ("%x PES_private_data_flag", filter->id);
        data += 16;
        header_data_length -= 16;
        datalen -= 16;
      }
      /* pack_header_field_flag */
      if ((flags & 0x40)) {
        guint8 pack_field_length = *data;
        GST_DEBUG ("%x pack_header_field_flag, pack_field_length %d",
            filter->id, pack_field_length);
        data += pack_field_length + 1;
        header_data_length -= pack_field_length + 1;
        datalen -= pack_field_length + 1;
      }
      /* program_packet_sequence_counter_flag */
      if ((flags & 0x20)) {
        GST_DEBUG ("%x program_packet_sequence_counter_flag", filter->id);
        data += 2;
        header_data_length -= 2;
        datalen -= 2;
      }
      /* P-STD_buffer_flag */
      if ((flags & 0x10)) {
        GST_DEBUG ("%x P-STD_buffer_flag", filter->id);
        data += 2;
        header_data_length -= 2;
        datalen -= 2;
      }
      /* PES_extension_flag_2 */
      if ((flags & 0x01)) {
        guint8 PES_extension_field_length = *data++;
        GST_DEBUG ("%x PES_extension_flag_2, len %d",
            filter->id, PES_extension_field_length & 0x7f);
        if (PES_extension_field_length == 0x81) {
          GST_DEBUG ("%x substream id 0x%02x", filter->id, *data);
        }
        data += PES_extension_field_length & 0x7f;
        header_data_length -= (PES_extension_field_length & 0x7f) + 1;
        datalen -= (PES_extension_field_length & 0x7f) + 1;
      }
    }
    /* calculate the amount of real data in this PES packet */
    data += header_data_length;
    datalen -= header_data_length;
  } else if (*data == 0x0f) {
    /* Not sure what this clause is for */
    data++;
    datalen--;
  } else {
    /* Data byte wasn't recognised as a flags byte */
    GST_DEBUG ("Unrecognised flags byte 0x%02x", *data);
    goto lost_sync;
  }

push_out:
  {
    GstBuffer *out;
#ifndef GST_DISABLE_GST_DEBUG
    guint16 consumed;

    consumed = avail - 6 - datalen;
#endif

    if (filter->unbounded_packet == FALSE) {
      filter->length -= avail - 6;
      GST_DEBUG ("pushing %d, need %d more, consumed %d",
          datalen, filter->length, consumed);
    } else {
      GST_DEBUG ("pushing %d, unbounded packet, consumed %d",
          datalen, consumed);
    }

    if (datalen > 0) {
      out = gst_buffer_new_allocate (NULL, datalen, NULL);
      gst_buffer_fill (out, 0, data, datalen);
      ret = gst_pes_filter_data_push (filter, TRUE, out);
      filter->first = FALSE;
    } else {
      GST_LOG ("first being set to TRUE");
      filter->first = TRUE;
      ret = GST_FLOW_OK;
    }

    if (filter->length > 0 || filter->unbounded_packet)
      filter->state = STATE_DATA_PUSH;
  }

  gst_adapter_unmap (filter->adapter);
  gst_adapter_flush (filter->adapter, avail);
  ADAPTER_OFFSET_FLUSH (avail);

  return ret;

need_more_data:
  {
    if (filter->unbounded_packet == FALSE) {
      if (have_size == TRUE) {
        GST_DEBUG ("bounded need more data %" G_GSIZE_FORMAT " , lost sync",
            gst_adapter_available (filter->adapter));
        ret = GST_FLOW_LOST_SYNC;
      } else {
        GST_DEBUG ("bounded need more data %" G_GSIZE_FORMAT
            ", breaking for more", gst_adapter_available (filter->adapter));
        ret = GST_FLOW_NEED_MORE_DATA;
      }
    } else {
      GST_DEBUG ("unbounded need more data %" G_GSIZE_FORMAT,
          gst_adapter_available (filter->adapter));
      ret = GST_FLOW_NEED_MORE_DATA;
    }
    gst_adapter_unmap (filter->adapter);
    return ret;
  }
skip:
  {
    gst_adapter_unmap (filter->adapter);

    GST_DEBUG ("skipping 0x%02x", filter->id);
    gst_adapter_flush (filter->adapter, avail);
    ADAPTER_OFFSET_FLUSH (avail);

    filter->length -= avail - 6;
    if (filter->length > 0 || filter->unbounded_packet)
      filter->state = STATE_DATA_SKIP;
    return GST_FLOW_OK;
  }
lost_sync:
  {
    gst_adapter_unmap (filter->adapter);
    GST_DEBUG ("lost sync");
    gst_adapter_flush (filter->adapter, 4);
    ADAPTER_OFFSET_FLUSH (4);

    return GST_FLOW_LOST_SYNC;
  }
}

static GstFlowReturn
gst_pes_filter_data_push (GstPESFilter * filter, gboolean first,
    GstBuffer * buffer)
{
  GstFlowReturn ret;

  GST_LOG ("pushing, first: %d", first);

  if (filter->data_cb) {
    ret = filter->data_cb (filter, first, buffer, filter->user_data);
  } else {
    gst_buffer_unref (buffer);
    ret = GST_FLOW_OK;
  }
  return ret;
}

GstFlowReturn
gst_pes_filter_push (GstPESFilter * filter, GstBuffer * buffer)
{
  GstFlowReturn ret;

  g_return_val_if_fail (filter != NULL, GST_FLOW_ERROR);
  g_return_val_if_fail (buffer != NULL, GST_FLOW_ERROR);

  switch (filter->state) {
    case STATE_HEADER_PARSE:
      gst_adapter_push (filter->adapter, buffer);
      ret = gst_pes_filter_parse (filter);
      break;
    case STATE_DATA_PUSH:
      ret = gst_pes_filter_data_push (filter, filter->first, buffer);
      filter->first = FALSE;
      break;
    case STATE_DATA_SKIP:
      gst_buffer_unref (buffer);
      ret = GST_FLOW_OK;
      break;
    default:
      goto wrong_state;
  }
  return ret;

  /* ERROR */
wrong_state:
  {
    GST_DEBUG ("wrong internal state %d", filter->state);
    return GST_FLOW_ERROR;
  }
}

GstFlowReturn
gst_pes_filter_process (GstPESFilter * filter)
{
  GstFlowReturn ret;
  gboolean skip = FALSE;

  g_return_val_if_fail (filter != NULL, GST_FLOW_ERROR);

  switch (filter->state) {
    case STATE_HEADER_PARSE:
      ret = gst_pes_filter_parse (filter);
      break;
    case STATE_DATA_SKIP:
      skip = TRUE;
      /* fallthrough */
    case STATE_DATA_PUSH:
      if (filter->length > 0 || filter->unbounded_packet) {
        gint avail;

        avail = gst_adapter_available (filter->adapter);
        if (filter->unbounded_packet == FALSE)
          avail = MIN (avail, filter->length);

        if (skip) {
          gst_adapter_flush (filter->adapter, avail);
          ADAPTER_OFFSET_FLUSH (avail);
          ret = GST_FLOW_OK;
        } else {
          GstBuffer *out;

          out = gst_adapter_take_buffer (filter->adapter, avail);

          ret = gst_pes_filter_data_push (filter, filter->first, out);
          filter->first = FALSE;
        }

        if (filter->unbounded_packet == FALSE) {
          filter->length -= avail;
          if (filter->length == 0)
            filter->state = STATE_HEADER_PARSE;
        }
      } else {
        filter->state = STATE_HEADER_PARSE;
        ret = GST_FLOW_OK;
      }
      break;
    default:
      goto wrong_state;
  }
  return ret;

  /* ERROR */
wrong_state:
  {
    GST_DEBUG ("wrong internal state %d", filter->state);
    return GST_FLOW_ERROR;
  }
}

void
gst_pes_filter_flush (GstPESFilter * filter)
{
  g_return_if_fail (filter != NULL);

  if (filter->adapter) {
    gst_adapter_clear (filter->adapter);
    if (filter->adapter_offset)
      *filter->adapter_offset = G_MAXUINT64;
  }
  filter->state = STATE_HEADER_PARSE;
}

GstFlowReturn
gst_pes_filter_drain (GstPESFilter * filter)
{
  g_return_val_if_fail (filter != NULL, GST_FLOW_ERROR);

  gst_pes_filter_flush (filter);

  return GST_FLOW_OK;
}
