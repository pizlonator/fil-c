/*
 * GStreamer
 * Copyright (C) 2022 Matthew Waters <matthew@centricular.com>
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
#  include <config.h>
#endif

#include <gst/base/base.h>

#include "ccutils.h"

#define GST_CAT_DEFAULT ccutils_debug_cat
GST_DEBUG_CATEGORY (GST_CAT_DEFAULT);

typedef struct cdp_fps_entry cdp_fps_entry;

#define VAL_OR_0(v) ((v) ? (*(v)) : 0)

static const struct cdp_fps_entry cdp_fps_table[] = {
  {0x1f, 24000, 1001, 25, 22, 3 /* FIXME: alternating max cea608 count! */ },
  {0x2f, 24, 1, 25, 22, 2},
  {0x3f, 25, 1, 24, 22, 2},
  {0x4f, 30000, 1001, 20, 18, 2},
  {0x5f, 30, 1, 20, 18, 2},
  {0x6f, 50, 1, 12, 11, 1},
  {0x7f, 60000, 1001, 10, 9, 1},
  {0x8f, 60, 1, 10, 9, 1},
};
const struct cdp_fps_entry null_fps_entry = { 0, 0, 0, 0 };

const struct cdp_fps_entry *
cdp_fps_entry_from_fps (guint fps_n, guint fps_d)
{
  int i;
  for (i = 0; i < G_N_ELEMENTS (cdp_fps_table); i++) {
    if (cdp_fps_table[i].fps_n == fps_n && cdp_fps_table[i].fps_d == fps_d)
      return &cdp_fps_table[i];
  }
  return &null_fps_entry;
}

const struct cdp_fps_entry *
cdp_fps_entry_from_id (guint8 id)
{
  int i;
  for (i = 0; i < G_N_ELEMENTS (cdp_fps_table); i++) {
    if (cdp_fps_table[i].fps_idx == id)
      return &cdp_fps_table[i];
  }
  return &null_fps_entry;
}

/* Converts raw CEA708 cc_data and an optional timecode into CDP */
guint
convert_cea708_cc_data_to_cdp (GstObject * dbg_obj, GstCCCDPMode cdp_mode,
    guint16 cdp_hdr_sequence_cntr, const guint8 * cc_data, guint cc_data_len,
    guint8 * cdp, guint cdp_len, const GstVideoTimeCode * tc,
    const cdp_fps_entry * fps_entry)
{
  GstByteWriter bw;
  guint8 flags, checksum;
  guint i, len;

  GST_DEBUG_OBJECT (dbg_obj, "writing out cdp packet from cc_data with "
      "length %u", cc_data_len);

  gst_byte_writer_init_with_data (&bw, cdp, cdp_len, FALSE);
  gst_byte_writer_put_uint16_be_unchecked (&bw, 0x9669);
  /* Write a length of 0 for now */
  gst_byte_writer_put_uint8_unchecked (&bw, 0);

  gst_byte_writer_put_uint8_unchecked (&bw, fps_entry->fps_idx);

  if (cc_data_len / 3 > fps_entry->max_cc_count) {
    GST_WARNING_OBJECT (dbg_obj, "Too many cc_data triplets for framerate: %u. "
        "Truncating to %u", cc_data_len / 3, fps_entry->max_cc_count);
    cc_data_len = 3 * fps_entry->max_cc_count;
  }

  /* caption_service_active */
  flags = 0x02;

  /* ccdata_present */
  if ((cdp_mode & GST_CC_CDP_MODE_CC_DATA))
    flags |= 0x40;

  /* time_code_present */
  if ((cdp_mode & GST_CC_CDP_MODE_TIME_CODE) && tc && tc->config.fps_n > 0)
    flags |= 0x80;

  /* reserved */
  flags |= 0x01;

  gst_byte_writer_put_uint8_unchecked (&bw, flags);

  gst_byte_writer_put_uint16_be_unchecked (&bw, cdp_hdr_sequence_cntr);

  if ((cdp_mode & GST_CC_CDP_MODE_TIME_CODE) && tc && tc->config.fps_n > 0) {
    guint8 u8;

    gst_byte_writer_put_uint8_unchecked (&bw, 0x71);
    /* reserved 11 - 2 bits */
    u8 = 0xc0;
    /* tens of hours - 2 bits */
    u8 |= ((tc->hours / 10) & 0x3) << 4;
    /* units of hours - 4 bits */
    u8 |= (tc->hours % 10) & 0xf;
    gst_byte_writer_put_uint8_unchecked (&bw, u8);

    /* reserved 1 - 1 bit */
    u8 = 0x80;
    /* tens of minutes - 3 bits */
    u8 |= ((tc->minutes / 10) & 0x7) << 4;
    /* units of minutes - 4 bits */
    u8 |= (tc->minutes % 10) & 0xf;
    gst_byte_writer_put_uint8_unchecked (&bw, u8);

    /* field flag - 1 bit */
    u8 = tc->field_count < 2 ? 0x00 : 0x80;
    /* tens of seconds - 3 bits */
    u8 |= ((tc->seconds / 10) & 0x7) << 4;
    /* units of seconds - 4 bits */
    u8 |= (tc->seconds % 10) & 0xf;
    gst_byte_writer_put_uint8_unchecked (&bw, u8);

    /* drop frame flag - 1 bit */
    u8 = (tc->config.flags & GST_VIDEO_TIME_CODE_FLAGS_DROP_FRAME) ? 0x80 :
        0x00;
    /* reserved0 - 1 bit */
    /* tens of frames - 2 bits */
    u8 |= ((tc->frames / 10) & 0x3) << 4;
    /* units of frames 4 bits */
    u8 |= (tc->frames % 10) & 0xf;
    gst_byte_writer_put_uint8_unchecked (&bw, u8);
  }

  if ((cdp_mode & GST_CC_CDP_MODE_CC_DATA)) {
    gst_byte_writer_put_uint8_unchecked (&bw, 0x72);
    gst_byte_writer_put_uint8_unchecked (&bw, 0xe0 | fps_entry->max_cc_count);
    gst_byte_writer_put_data_unchecked (&bw, cc_data, cc_data_len);
    while (fps_entry->max_cc_count > cc_data_len / 3) {
      gst_byte_writer_put_uint8_unchecked (&bw, 0xfa);
      gst_byte_writer_put_uint8_unchecked (&bw, 0x00);
      gst_byte_writer_put_uint8_unchecked (&bw, 0x00);
      cc_data_len += 3;
    }
  }

  gst_byte_writer_put_uint8_unchecked (&bw, 0x74);
  gst_byte_writer_put_uint16_be_unchecked (&bw, cdp_hdr_sequence_cntr);
  /* We calculate the checksum afterwards */
  gst_byte_writer_put_uint8_unchecked (&bw, 0);

  len = gst_byte_writer_get_pos (&bw);
  gst_byte_writer_set_pos (&bw, 2);
  gst_byte_writer_put_uint8_unchecked (&bw, len);

  checksum = 0;
  for (i = 0; i < len; i++) {
    checksum += cdp[i];
  }
  checksum &= 0xff;
  checksum = 256 - checksum;
  cdp[len - 1] = checksum;

  return len;
}

/* Converts CDP into raw CEA708 cc_data */
guint
convert_cea708_cdp_to_cc_data (GstObject * dbg_obj,
    const guint8 * cdp, guint cdp_len, guint8 * cc_data,
    GstVideoTimeCode * tc, const cdp_fps_entry ** out_fps_entry)
{
  GstByteReader br;
  guint16 u16;
  guint8 u8;
  guint8 flags;
  guint len = 0;
  const struct cdp_fps_entry *fps_entry;

  *out_fps_entry = &null_fps_entry;
  memset (tc, 0, sizeof (*tc));

  /* Header + footer length */
  if (cdp_len < 11) {
    GST_WARNING_OBJECT (dbg_obj, "cdp packet too short (%u). expected at "
        "least %u", cdp_len, 11);
    return 0;
  }

  gst_byte_reader_init (&br, cdp, cdp_len);
  u16 = gst_byte_reader_get_uint16_be_unchecked (&br);
  if (u16 != 0x9669) {
    GST_WARNING_OBJECT (dbg_obj, "cdp packet does not have initial magic bytes "
        "of 0x9669");
    return 0;
  }

  u8 = gst_byte_reader_get_uint8_unchecked (&br);
  if (u8 != cdp_len) {
    GST_WARNING_OBJECT (dbg_obj, "cdp packet length (%u) does not match passed "
        "in value (%u)", u8, cdp_len);
    return 0;
  }

  u8 = gst_byte_reader_get_uint8_unchecked (&br);
  fps_entry = cdp_fps_entry_from_id (u8);
  if (!fps_entry || fps_entry->fps_n == 0) {
    GST_WARNING_OBJECT (dbg_obj, "cdp packet does not have a valid framerate "
        "id (0x%02x", u8);
    return 0;
  }

  flags = gst_byte_reader_get_uint8_unchecked (&br);
  /* No cc_data? */
  if ((flags & 0x40) == 0) {
    GST_DEBUG_OBJECT (dbg_obj, "cdp packet does have any cc_data");
    return 0;
  }

  /* cdp_hdr_sequence_cntr */
  gst_byte_reader_skip_unchecked (&br, 2);

  /* time_code_present */
  if (flags & 0x80) {
    guint8 hours, minutes, seconds, frames, fields;
    gboolean drop_frame;

    if (gst_byte_reader_get_remaining (&br) < 5) {
      GST_WARNING_OBJECT (dbg_obj, "cdp packet does not have enough data to "
          "contain a timecode (%u). Need at least 5 bytes",
          gst_byte_reader_get_remaining (&br));
      return 0;
    }
    u8 = gst_byte_reader_get_uint8_unchecked (&br);
    if (u8 != 0x71) {
      GST_WARNING_OBJECT (dbg_obj, "cdp packet does not have timecode start "
          "byte of 0x71, found 0x%02x", u8);
      return 0;
    }

    u8 = gst_byte_reader_get_uint8_unchecked (&br);
    if ((u8 & 0xc0) != 0xc0) {
      GST_WARNING_OBJECT (dbg_obj, "reserved bits are not 0xc0, found 0x%02x",
          u8);
      return 0;
    }

    hours = ((u8 >> 4) & 0x3) * 10 + (u8 & 0xf);

    u8 = gst_byte_reader_get_uint8_unchecked (&br);
    if ((u8 & 0x80) != 0x80) {
      GST_WARNING_OBJECT (dbg_obj, "reserved bit is not 0x80, found 0x%02x",
          u8);
      return 0;
    }
    minutes = ((u8 >> 4) & 0x7) * 10 + (u8 & 0xf);

    u8 = gst_byte_reader_get_uint8_unchecked (&br);
    if (u8 & 0x80)
      fields = 2;
    else
      fields = 1;
    seconds = ((u8 >> 4) & 0x7) * 10 + (u8 & 0xf);

    u8 = gst_byte_reader_get_uint8_unchecked (&br);
    if (u8 & 0x40) {
      GST_WARNING_OBJECT (dbg_obj, "reserved bit is not 0x0, found 0x%02x", u8);
      return 0;
    }

    drop_frame = !(!(u8 & 0x80));
    frames = ((u8 >> 4) & 0x3) * 10 + (u8 & 0xf);

    gst_video_time_code_init (tc, fps_entry->fps_n, fps_entry->fps_d, NULL,
        drop_frame ? GST_VIDEO_TIME_CODE_FLAGS_DROP_FRAME :
        GST_VIDEO_TIME_CODE_FLAGS_NONE, hours, minutes, seconds, frames,
        fields);
  }

  /* ccdata_present */
  if (flags & 0x40) {
    guint8 cc_count;

    if (gst_byte_reader_get_remaining (&br) < 2) {
      GST_WARNING_OBJECT (dbg_obj, "not enough data to contain valid cc_data");
      return 0;
    }
    u8 = gst_byte_reader_get_uint8_unchecked (&br);
    if (u8 != 0x72) {
      GST_WARNING_OBJECT (dbg_obj, "missing cc_data start code of 0x72, "
          "found 0x%02x", u8);
      return 0;
    }

    cc_count = gst_byte_reader_get_uint8_unchecked (&br);
    if ((cc_count & 0xe0) != 0xe0) {
      GST_WARNING_OBJECT (dbg_obj, "reserved bits are not 0xe0, found 0x%02x",
          u8);
      return 0;
    }
    cc_count &= 0x1f;

    len = 3 * cc_count;
    if (gst_byte_reader_get_remaining (&br) < len) {
      GST_WARNING_OBJECT (dbg_obj, "not enough bytes (%u) left for the "
          "number of byte triples (%u)", gst_byte_reader_get_remaining (&br),
          cc_count);
      return 0;
    }

    memcpy (cc_data, gst_byte_reader_get_data_unchecked (&br, len), len);
  }

  *out_fps_entry = fps_entry;

  /* skip everything else we don't care about */
  return len;
}

#define CC_DATA_EXTRACT_TOO_MANY_FIELD1 -2
#define CC_DATA_EXTRACT_TOO_MANY_FIELD2 -3

static gint
cc_data_extract_cea608 (const guint8 * cc_data, guint cc_data_len,
    guint8 * cea608_field1, guint * cea608_field1_len,
    guint8 * cea608_field2, guint * cea608_field2_len)
{
  guint i, field_1_len = 0, field_2_len = 0;

  if (cea608_field1_len) {
    field_1_len = *cea608_field1_len;
    *cea608_field1_len = 0;
  }
  if (cea608_field2_len) {
    field_2_len = *cea608_field2_len;
    *cea608_field2_len = 0;
  }

  if (cc_data_len % 3 != 0) {
    GST_WARNING ("Invalid cc_data buffer size %u. Truncating to a multiple "
        "of 3", cc_data_len);
    cc_data_len = cc_data_len - (cc_data_len % 3);
  }

  for (i = 0; i < cc_data_len / 3; i++) {
    guint8 byte0 = cc_data[i * 3 + 0];
    guint8 byte1 = cc_data[i * 3 + 1];
    guint8 byte2 = cc_data[i * 3 + 2];
    gboolean cc_valid = (byte0 & 0x04) == 0x04;
    guint8 cc_type = byte0 & 0x03;

    GST_TRACE ("0x%02x 0x%02x 0x%02x, valid: %u, type: 0b%u%u", byte0, byte1,
        byte2, cc_valid, (cc_type & 0x2) == 0x2, (cc_type & 0x1) == 0x1);

    if (cc_type == 0x00) {
      if (!cc_valid)
        continue;

      if (cea608_field1 && cea608_field1_len) {
        if (*cea608_field1_len + 2 > field_1_len) {
          GST_WARNING ("Too many cea608 input bytes %u for field 1",
              *cea608_field1_len + 2);
          return CC_DATA_EXTRACT_TOO_MANY_FIELD1;
        }

        cea608_field1[(*cea608_field1_len)++] = byte1;
        cea608_field1[(*cea608_field1_len)++] = byte2;
      }
    } else if (cc_type == 0x01) {
      if (!cc_valid)
        continue;

      if (cea608_field2 && cea608_field2_len) {
        if (*cea608_field2_len + 2 > field_2_len) {
          GST_WARNING ("Too many cea608 input bytes %u for field 2",
              *cea608_field2_len + 2);
          return CC_DATA_EXTRACT_TOO_MANY_FIELD2;
        }
        cea608_field2[(*cea608_field2_len)++] = byte1;
        cea608_field2[(*cea608_field2_len)++] = byte2;
      }
    } else {
      /* all cea608 packets must be at the beginning of a cc_data */
      break;
    }
  }

  g_assert_cmpint (i * 3, <=, cc_data_len);

  GST_LOG ("Extracted cea608-1 of length %u and cea608-2 of length %u, "
      "ccp_offset %i", VAL_OR_0 (cea608_field1_len),
      VAL_OR_0 (cea608_field2_len), i * 3);

  return i * 3;
}

gint
drop_ccp_from_cc_data (guint8 * cc_data, guint cc_data_len)
{
  return cc_data_extract_cea608 (cc_data, cc_data_len, NULL, NULL, NULL, NULL);
}

#define DEFAULT_MAX_BUFFER_TIME (100 * GST_MSECOND)

struct _CCBuffer
{
  GstObject parent;
  GArray *cea608_1;
  GArray *cea608_2;
  GArray *cc_data;
  /* used for tracking which field to write across output buffer boundaries */
  gboolean last_cea608_written_was_field1;

  /* properties */
  GstClockTime max_buffer_time;
  gboolean output_padding;
  gboolean output_ccp_padding;
};

G_DEFINE_TYPE (CCBuffer, cc_buffer, G_TYPE_OBJECT);

CCBuffer *
cc_buffer_new (void)
{
  return g_object_new (cc_buffer_get_type (), NULL);
}

static void
cc_buffer_init (CCBuffer * buf)
{
  buf->cea608_1 = g_array_new (FALSE, FALSE, sizeof (guint8));
  buf->cea608_2 = g_array_new (FALSE, FALSE, sizeof (guint8));
  buf->cc_data = g_array_new (FALSE, FALSE, sizeof (guint8));

  buf->max_buffer_time = DEFAULT_MAX_BUFFER_TIME;
  buf->output_padding = TRUE;
  buf->output_ccp_padding = FALSE;
}

static void
cc_buffer_finalize (GObject * object)
{
  CCBuffer *buf = GST_CC_BUFFER (object);

  g_array_unref (buf->cea608_1);
  buf->cea608_1 = NULL;
  g_array_unref (buf->cea608_2);
  buf->cea608_2 = NULL;
  g_array_unref (buf->cc_data);
  buf->cc_data = NULL;

  G_OBJECT_CLASS (cc_buffer_parent_class)->finalize (object);
}

static void
cc_buffer_class_init (CCBufferClass * buf_class)
{
  GObjectClass *gobject_class = (GObjectClass *) buf_class;

  gobject_class->finalize = cc_buffer_finalize;
}

/* remove padding bytes from a cc_data packet. Returns the length of the new
 * data in @cc_data */
static guint
compact_cc_data (guint8 * cc_data, guint cc_data_len)
{
  gboolean started_ccp = FALSE;
  guint out_len = 0;
  guint i;

  if (cc_data_len % 3 != 0) {
    GST_WARNING ("Invalid cc_data buffer size");
    cc_data_len = cc_data_len - (cc_data_len % 3);
  }

  for (i = 0; i < cc_data_len / 3; i++) {
    gboolean cc_valid = (cc_data[i * 3] & 0x04) == 0x04;
    guint8 cc_type = cc_data[i * 3] & 0x03;

    if (!started_ccp && (cc_type == 0x00 || cc_type == 0x01)) {
      if (cc_valid) {
        /* copy over valid 608 data */
        cc_data[out_len++] = cc_data[i * 3];
        cc_data[out_len++] = cc_data[i * 3 + 1];
        cc_data[out_len++] = cc_data[i * 3 + 2];
      }
      continue;
    }

    if (cc_type & 0x10)
      started_ccp = TRUE;

    if (!cc_valid)
      continue;

    if (cc_type == 0x00 || cc_type == 0x01) {
      GST_WARNING ("Invalid cc_data.  cea608 bytes after cea708");
      return 0;
    }

    cc_data[out_len++] = cc_data[i * 3];
    cc_data[out_len++] = cc_data[i * 3 + 1];
    cc_data[out_len++] = cc_data[i * 3 + 2];
  }

  GST_LOG ("compacted cc_data from %u to %u", cc_data_len, out_len);

  return out_len;
}

static guint
calculate_n_cea608_doubles_from_time_ceil (CCBuffer * buf, GstClockTime ns)
{
  /* cea608 has a maximum bitrate of 60000/1001 * 2 bytes/s */
  guint ret = gst_util_uint64_scale_ceil (ns, 120000, 1001 * GST_SECOND);

  return GST_ROUND_UP_2 (ret);
}

static guint
calculate_n_cea708_doubles_from_time_ceil (CCBuffer * buf, GstClockTime ns)
{
  /* ccp has a maximum bitrate of 9600000/1001 bits/s */
  guint ret = gst_util_uint64_scale_ceil (ns, 9600000 / 8, 1001 * GST_SECOND);

  return GST_ROUND_UP_2 (ret);
}

static void
push_internal (CCBuffer * buf, const guint8 * cea608_1,
    guint cea608_1_len, const guint8 * cea608_2, guint cea608_2_len,
    const guint8 * cc_data, guint cc_data_len)
{
  guint max_cea608_bytes;

  GST_DEBUG_OBJECT (buf, "pushing cea608-1: %u cea608-2: %u ccp: %u",
      cea608_1_len, cea608_2_len, cc_data_len);
  max_cea608_bytes =
      calculate_n_cea608_doubles_from_time_ceil (buf, buf->max_buffer_time);

  if (cea608_1_len > 0) {
    if (cea608_1_len + buf->cea608_1->len > max_cea608_bytes) {
      GST_WARNING_OBJECT (buf, "cea608 field 1 overflow, dropping all "
          "previous data, max %u, attempted to hold %u", max_cea608_bytes,
          cea608_1_len + buf->cea608_1->len);
      g_array_set_size (buf->cea608_1, 0);
    }
    g_array_append_vals (buf->cea608_1, cea608_1, cea608_1_len);
  }
  if (cea608_2_len > 0) {
    if (cea608_2_len + buf->cea608_2->len > max_cea608_bytes) {
      GST_WARNING_OBJECT (buf, "cea608 field 2 overflow, dropping all "
          "previous data, max %u, attempted to hold %u", max_cea608_bytes,
          cea608_2_len + buf->cea608_2->len);
      g_array_set_size (buf->cea608_2, 0);
    }
    g_array_append_vals (buf->cea608_2, cea608_2, cea608_2_len);
  }
  if (cc_data_len > 0) {
    guint max_cea708_bytes =
        calculate_n_cea708_doubles_from_time_ceil (buf, buf->max_buffer_time);
    if (cc_data_len + buf->cc_data->len > max_cea708_bytes) {
      GST_WARNING_OBJECT (buf, "ccp data overflow, dropping all "
          "previous data, max %u, attempted to hold %u", max_cea708_bytes,
          cc_data_len + buf->cc_data->len);
      g_array_set_size (buf->cea608_2, 0);
    }
    g_array_append_vals (buf->cc_data, cc_data, cc_data_len);
  }
}

gboolean
cc_buffer_push_separated (CCBuffer * buf, const guint8 * cea608_1,
    guint cea608_1_len, const guint8 * cea608_2, guint cea608_2_len,
    const guint8 * cc_data, guint cc_data_len)
{
  guint8 cea608_1_copy[MAX_CEA608_LEN];
  guint8 cea608_2_copy[MAX_CEA608_LEN];
  guint8 cc_data_copy[MAX_CDP_PACKET_LEN];
  guint i;

  if (cea608_1 && cea608_1_len > 0) {
    guint out_i = 0;
    for (i = 0; i < cea608_1_len / 2; i++) {
      if (cea608_1[i] != 0x80 || cea608_1[i + 1] != 0x80) {
        cea608_1_copy[out_i++] = cea608_1[i];
        cea608_1_copy[out_i++] = cea608_1[i + 1];
      }
    }
    cea608_1_len = out_i;
  } else {
    cea608_1_len = 0;
  }

  if (cea608_2 && cea608_2_len > 0) {
    guint out_i = 0;
    for (i = 0; i < cea608_2_len / 2; i++) {
      if (cea608_2[i] != 0x80 || cea608_2[i + 1] != 0x80) {
        cea608_2_copy[out_i++] = cea608_2[i];
        cea608_2_copy[out_i++] = cea608_2[i + 1];
      }
    }
    cea608_2_len = out_i;
  } else {
    cea608_2_len = 0;
  }

  if (cc_data && cc_data_len > 0) {
    memcpy (cc_data_copy, cc_data, cc_data_len);
    cc_data_len = compact_cc_data (cc_data_copy, cc_data_len);
  } else {
    cc_data_len = 0;
  }

  push_internal (buf, cea608_1_copy, cea608_1_len, cea608_2_copy,
      cea608_2_len, cc_data_copy, cc_data_len);

  return cea608_1_len > 0 || cea608_2_len > 0 || cc_data_len > 0;
}

gboolean
cc_buffer_push_cc_data (CCBuffer * buf, const guint8 * cc_data,
    guint cc_data_len)
{
  guint8 cea608_1[MAX_CEA608_LEN];
  guint8 cea608_2[MAX_CEA608_LEN];
  guint8 cc_data_copy[MAX_CDP_PACKET_LEN];
  guint cea608_1_len = MAX_CEA608_LEN;
  guint cea608_2_len = MAX_CEA608_LEN;
  int ccp_offset;

  memcpy (cc_data_copy, cc_data, cc_data_len);

  cc_data_len = compact_cc_data (cc_data_copy, cc_data_len);

  ccp_offset = cc_data_extract_cea608 (cc_data_copy, cc_data_len, cea608_1,
      &cea608_1_len, cea608_2, &cea608_2_len);

  if (ccp_offset < 0) {
    GST_WARNING_OBJECT (buf, "Failed to extract cea608 from cc_data");
    return FALSE;
  }

  push_internal (buf, cea608_1, cea608_1_len, cea608_2,
      cea608_2_len, &cc_data_copy[ccp_offset], cc_data_len - ccp_offset);

  return cea608_1_len > 0 || cea608_2_len > 0 || cc_data_len - ccp_offset > 0;
}

void
cc_buffer_get_stored_size (CCBuffer * buf, guint * cea608_1_len,
    guint * cea608_2_len, guint * cc_data_len)
{
  if (cea608_1_len)
    *cea608_1_len = buf->cea608_1->len;
  if (cea608_2_len)
    *cea608_2_len = buf->cea608_2->len;
  if (cc_data_len)
    *cc_data_len = buf->cc_data->len;
}

void
cc_buffer_discard (CCBuffer * buf)
{
  g_array_set_size (buf->cea608_1, 0);
  g_array_set_size (buf->cea608_2, 0);
  g_array_set_size (buf->cc_data, 0);
}

#if 0
void
cc_buffer_peek (CCBuffer * buf, guint8 ** cea608_1, guint * cea608_1_len,
    guint8 ** cea608_2, guint * cea608_2_len, guint8 ** cc_data,
    guint * cc_data_len)
{
  if (cea608_1_len) {
    if (cea608_1) {
      *cea608_1 = (guint8 *) buf->cea608_1->data;
    }
    *cea608_1_len = buf->cea608_1->len;
  }
  if (cea608_1_len) {
    if (cea608_2) {
      *cea608_2 = (guint8 *) buf->cea608_2->data;
    }
    *cea608_2_len = buf->cea608_2->len;
  }
  if (cc_data_len) {
    if (cc_data) {
      *cc_data = (guint8 *) buf->cc_data->data;
    }
    *cc_data_len = buf->cc_data->len;
  }
}
#endif
static void
cc_buffer_get_out_sizes (CCBuffer * buf, const struct cdp_fps_entry *fps_entry,
    guint * cea608_1_len, guint * field1_padding, guint * cea608_2_len,
    guint * field2_padding, guint * cc_data_len)
{
  gint extra_ccp = 0, extra_cea608_1 = 0, extra_cea608_2 = 0;
  gint write_ccp_size = 0, write_cea608_1_size = 0, write_cea608_2_size = 0;
  gboolean wrote_first = FALSE;

  if (buf->cc_data->len) {
    extra_ccp = buf->cc_data->len - 3 * fps_entry->max_ccp_count;
    extra_ccp = MAX (0, extra_ccp);
    write_ccp_size = buf->cc_data->len - extra_ccp;
  }

  extra_cea608_1 = buf->cea608_1->len;
  extra_cea608_2 = buf->cea608_2->len;
  *field1_padding = 0;
  *field2_padding = 0;

  wrote_first = !buf->last_cea608_written_was_field1;
  /* try to push data into the packets.  Anything 'extra' will be
   * stored for later */
  while (TRUE) {
    gint avail_1, avail_2;

    avail_1 = buf->cea608_1->len - extra_cea608_1 + *field1_padding;
    avail_2 = buf->cea608_2->len - extra_cea608_2 + *field2_padding;
    if (avail_1 + avail_2 >= 2 * fps_entry->max_cea608_count)
      break;

    if (wrote_first) {
      if (extra_cea608_1 > 0) {
        extra_cea608_1 -= 2;
        g_assert_cmpint (extra_cea608_1, >=, 0);
        write_cea608_1_size += 2;
        g_assert_cmpint (write_cea608_1_size, <=, buf->cea608_1->len);
      } else {
        *field1_padding += 2;
      }
    }

    avail_1 = buf->cea608_1->len - extra_cea608_1 + *field1_padding;
    avail_2 = buf->cea608_2->len - extra_cea608_2 + *field2_padding;
    if (avail_1 + avail_2 >= 2 * fps_entry->max_cea608_count)
      break;

    if (extra_cea608_2 > 0) {
      extra_cea608_2 -= 2;
      g_assert_cmpint (extra_cea608_2, >=, 0);
      write_cea608_2_size += 2;
      g_assert_cmpint (write_cea608_2_size, <=, buf->cea608_2->len);
    } else {
      /* we need to insert field 2 padding if we don't have data and are
       * requested to start with field2 */
      *field2_padding += 2;
    }
    wrote_first = TRUE;
  }

  // don't write padding if not requested
  if (!buf->output_padding && write_cea608_1_size == 0
      && write_cea608_2_size == 0) {
    // however if we are producing data for a cdp that only has a single 608 field,
    // in order to keep processing data will still need to alternate fields and
    // produce the relevant padding data
    if (fps_entry->max_cea608_count != 1 || (extra_cea608_1 == 0
            && extra_cea608_2 == 0)) {
      *field1_padding = 0;
      *field2_padding = 0;
    }
  }

  GST_TRACE_OBJECT (buf, "allocated sizes ccp:%u, cea608-1:%u (pad:%u), "
      "cea608-2:%u (pad:%u)", write_ccp_size, write_cea608_1_size,
      *field1_padding, write_cea608_2_size, *field2_padding);

  *cea608_1_len = write_cea608_1_size;
  *cea608_2_len = write_cea608_2_size;
  *cc_data_len = write_ccp_size;
}

void
cc_buffer_take_separated (CCBuffer * buf,
    const struct cdp_fps_entry *fps_entry, guint8 * cea608_1,
    guint * cea608_1_len, guint8 * cea608_2, guint * cea608_2_len,
    guint8 * cc_data, guint * cc_data_len)
{
  guint write_cea608_1_size, write_cea608_2_size, write_ccp_size;
  guint field1_padding, field2_padding;

  cc_buffer_get_out_sizes (buf, fps_entry, &write_cea608_1_size,
      &field1_padding, &write_cea608_2_size, &field2_padding, &write_ccp_size);

  if (cea608_1_len) {
    if (*cea608_1_len < write_cea608_1_size + field1_padding) {
      GST_WARNING_OBJECT (buf, "output cea608 field 1 buffer (%u) is too "
          "small to hold output (%u)", *cea608_1_len,
          write_cea608_1_size + field1_padding);
      *cea608_1_len = 0;
    } else if (cea608_1) {
      memcpy (cea608_1, buf->cea608_1->data, write_cea608_1_size);
      memset (&cea608_1[write_cea608_1_size], 0x80, field1_padding);
      *cea608_1_len = write_cea608_1_size + field1_padding;
    } else {
      *cea608_1_len = 0;
    }
  }
  if (cea608_2_len) {
    if (*cea608_2_len < write_cea608_2_size + field2_padding) {
      GST_WARNING_OBJECT (buf, "output cea608 field 2 buffer (%u) is too "
          "small to hold output (%u)", *cea608_2_len, write_cea608_2_size);
      *cea608_2_len = 0;
    } else if (cea608_2) {
      memcpy (cea608_2, buf->cea608_2->data, write_cea608_2_size);
      memset (&cea608_2[write_cea608_2_size], 0x80, field2_padding);
      *cea608_2_len = write_cea608_2_size + field2_padding;
    } else {
      *cea608_2_len = 0;
    }
  }
  if (cc_data_len) {
    if (*cc_data_len < write_ccp_size) {
      GST_WARNING_OBJECT (buf, "output ccp buffer (%u) is too "
          "small to hold output (%u)", *cc_data_len, write_ccp_size);
      *cc_data_len = 0;
    } else if (cc_data) {
      guint ccp_padding = 0;
      memcpy (cc_data, buf->cc_data->data, write_ccp_size);
      if (buf->output_ccp_padding
          && (write_ccp_size < 3 * fps_entry->max_ccp_count)) {
        guint i;

        ccp_padding = 3 * fps_entry->max_ccp_count - write_ccp_size;
        GST_TRACE_OBJECT (buf, "need %u ccp padding bytes (%u - %u)",
            ccp_padding, fps_entry->max_ccp_count, write_ccp_size);
        for (i = 0; i < ccp_padding; i += 3) {
          cc_data[i + write_ccp_size] = 0xfa;
          cc_data[i + 1 + write_ccp_size] = 0x00;
          cc_data[i + 2 + write_ccp_size] = 0x00;
        }
      }
      *cc_data_len = write_ccp_size + ccp_padding;
    } else if (buf->output_padding) {
      guint i;
      guint padding = 3 * fps_entry->max_ccp_count;
      for (i = 0; i < padding; i += 3) {
        cc_data[i + write_ccp_size] = 0xfa;
        cc_data[i + 1 + write_ccp_size] = 0x00;
        cc_data[i + 2 + write_ccp_size] = 0x00;
      }
      GST_TRACE_OBJECT (buf, "outputting only %u padding bytes", padding);
      *cc_data_len = padding;
    } else {
      *cc_data_len = 0;
    }
  }

  g_array_remove_range (buf->cea608_1, 0, write_cea608_1_size);
  g_array_remove_range (buf->cea608_2, 0, write_cea608_2_size);
  g_array_remove_range (buf->cc_data, 0, write_ccp_size);

  GST_LOG_OBJECT (buf, "bytes currently stored, cea608-1:%u, cea608-2:%u "
      "ccp:%u", buf->cea608_1->len, buf->cea608_2->len, buf->cc_data->len);
}

void
cc_buffer_take_cc_data (CCBuffer * buf,
    const struct cdp_fps_entry *fps_entry, gboolean nul_padding,
    guint8 * cc_data, guint * cc_data_len)
{
  guint write_cea608_1_size, write_cea608_2_size, write_ccp_size;
  guint field1_padding, field2_padding;
  gboolean wrote_first;
  guint8 padding_byte = nul_padding ? 0x00 : 0x80;

  cc_buffer_get_out_sizes (buf, fps_entry, &write_cea608_1_size,
      &field1_padding, &write_cea608_2_size, &field2_padding, &write_ccp_size);

  {
    guint cea608_1_i = 0, cea608_2_i = 0;
    guint out_i = 0;
    guint8 *cea608_1 = (guint8 *) buf->cea608_1->data;
    guint8 *cea608_2 = (guint8 *) buf->cea608_2->data;
    guint cea608_output_count =
        write_cea608_1_size + write_cea608_2_size + field1_padding +
        field2_padding;
    guint ccp_padding = 0;

    wrote_first = !buf->last_cea608_written_was_field1;
    while (cea608_1_i + cea608_2_i < cea608_output_count) {
      if (wrote_first) {
        if (cea608_1_i < write_cea608_1_size) {
          cc_data[out_i++] = 0xfc;
          cc_data[out_i++] = cea608_1[cea608_1_i];
          cc_data[out_i++] = cea608_1[cea608_1_i + 1];
          cea608_1_i += 2;
          buf->last_cea608_written_was_field1 = TRUE;
        } else if (cea608_1_i < write_cea608_1_size + field1_padding) {
          GST_TRACE_OBJECT (buf,
              "write field2:%u field2_i:%u, cea608-2 buf len:%u",
              write_cea608_2_size, cea608_2_i, buf->cea608_2->len);
          if (cea608_2_i < write_cea608_2_size
              || buf->cea608_2->len > write_cea608_2_size) {
            /* if we are writing field 2, then we have to write valid field 1 */
            GST_TRACE_OBJECT (buf, "writing valid field1 padding because "
                "we need to write valid field2");
            cc_data[out_i++] = 0xfc;
            cc_data[out_i++] = 0x80;
            cc_data[out_i++] = 0x80;
          } else {
            cc_data[out_i++] = 0xf8;
            cc_data[out_i++] = padding_byte;
            cc_data[out_i++] = padding_byte;
          }
          cea608_1_i += 2;
          buf->last_cea608_written_was_field1 = TRUE;
        }
      }

      if (cea608_2_i < write_cea608_2_size) {
        cc_data[out_i++] = 0xfd;
        cc_data[out_i++] = cea608_2[cea608_2_i];
        cc_data[out_i++] = cea608_2[cea608_2_i + 1];
        cea608_2_i += 2;
        buf->last_cea608_written_was_field1 = FALSE;
      } else if (cea608_2_i < write_cea608_2_size + field2_padding) {
        cc_data[out_i++] = 0xf9;
        cc_data[out_i++] = padding_byte;
        cc_data[out_i++] = padding_byte;
        cea608_2_i += 2;
        buf->last_cea608_written_was_field1 = FALSE;
      }

      wrote_first = TRUE;
    }

    if (write_ccp_size > 0)
      memcpy (&cc_data[out_i], buf->cc_data->data, write_ccp_size);
    if (buf->output_ccp_padding
        && (write_ccp_size < 3 * fps_entry->max_ccp_count)) {
      guint i;

      ccp_padding = 3 * fps_entry->max_ccp_count - write_ccp_size;
      GST_TRACE_OBJECT (buf, "need %u ccp padding bytes (%u - %u)", ccp_padding,
          fps_entry->max_ccp_count, write_ccp_size);
      for (i = 0; i < ccp_padding; i += 3) {
        cc_data[i + out_i + write_ccp_size] = 0xfa;
        cc_data[i + 1 + out_i + write_ccp_size] = 0x00;
        cc_data[i + 2 + out_i + write_ccp_size] = 0x00;
      }
    }
    *cc_data_len = out_i + write_ccp_size + ccp_padding;
    GST_TRACE_OBJECT (buf, "cc_data_len is %u (%u + %u + %u)", *cc_data_len,
        out_i, write_ccp_size, ccp_padding);
  }

  g_array_remove_range (buf->cea608_1, 0, write_cea608_1_size);
  g_array_remove_range (buf->cea608_2, 0, write_cea608_2_size);
  g_array_remove_range (buf->cc_data, 0, write_ccp_size);

  GST_LOG_OBJECT (buf, "bytes currently stored, cea608-1:%u, cea608-2:%u "
      "ccp:%u", buf->cea608_1->len, buf->cea608_2->len, buf->cc_data->len);
}

void
cc_buffer_take_cea608_field1 (CCBuffer * buf,
    const struct cdp_fps_entry *fps_entry, guint8 * cea608_1,
    guint * cea608_1_len)
{
  guint write_cea608_1_size, field1_padding;
  guint write_cea608_2_size, field2_padding;
  guint cc_data_len;

  cc_buffer_get_out_sizes (buf, fps_entry, &write_cea608_1_size,
      &field1_padding, &write_cea608_2_size, &field2_padding, &cc_data_len);

  if (*cea608_1_len < write_cea608_1_size + field1_padding) {
    GST_WARNING_OBJECT (buf,
        "Not enough output space to write cea608 field 1 data");
    *cea608_1_len = 0;
    return;
  }

  if (write_cea608_1_size > 0) {
    memcpy (cea608_1, buf->cea608_1->data, write_cea608_1_size);
    g_array_remove_range (buf->cea608_1, 0, write_cea608_1_size);
  }
  *cea608_1_len = write_cea608_1_size;
  if (buf->output_padding && field1_padding > 0) {
    memset (&cea608_1[write_cea608_1_size], 0x80, field1_padding);
    *cea608_1_len += field1_padding;
  }
}

void
cc_buffer_take_cea608_field2 (CCBuffer * buf,
    const struct cdp_fps_entry *fps_entry, guint8 * cea608_2,
    guint * cea608_2_len)
{
  guint write_cea608_1_size, field1_padding;
  guint write_cea608_2_size, field2_padding;
  guint cc_data_len;

  cc_buffer_get_out_sizes (buf, fps_entry, &write_cea608_1_size,
      &field1_padding, &write_cea608_2_size, &field2_padding, &cc_data_len);

  if (*cea608_2_len < write_cea608_2_size + field2_padding) {
    GST_WARNING_OBJECT (buf,
        "Not enough output space to write cea608 field 2 data");
    *cea608_2_len = 0;
    return;
  }

  if (write_cea608_2_size > 0) {
    memcpy (cea608_2, buf->cea608_2->data, write_cea608_2_size);
    g_array_remove_range (buf->cea608_2, 0, write_cea608_2_size);
  }
  *cea608_2_len = write_cea608_2_size;
  if (buf->output_padding && field1_padding > 0) {
    memset (&cea608_2[write_cea608_2_size], 0x80, field2_padding);
    *cea608_2_len += field2_padding;
  }
}

gboolean
cc_buffer_is_empty (CCBuffer * buf)
{
  return buf->cea608_1->len == 0 && buf->cea608_2->len == 0
      && buf->cc_data->len == 0;
}

void
cc_buffer_set_max_buffer_time (CCBuffer * buf, GstClockTime max_time)
{
  buf->max_buffer_time = max_time;
}

void
cc_buffer_set_output_padding (CCBuffer * buf, gboolean output_padding,
    gboolean output_ccp_padding)
{
  buf->output_padding = output_padding;
  buf->output_ccp_padding = output_ccp_padding;
}
