/* GStreamer RTMP Library
 * Copyright (C) 2017 Make.TV, Inc. <info@make.tv>
 *   Contact: Jan Alexander Steffens (heftig) <jsteffens@make.tv>
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
 * Free Software Foundation, Inc., 51 Franklin Street, Suite 500,
 * Boston, MA 02110-1335, USA.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "rtmpchunkstream.h"
#include "rtmputils.h"

GST_DEBUG_CATEGORY_STATIC (gst_rtmp_chunk_stream_debug_category);
#define GST_CAT_DEFAULT gst_rtmp_chunk_stream_debug_category

static void
init_debug (void)
{
  static gsize done = 0;
  if (g_once_init_enter (&done)) {
    GST_DEBUG_CATEGORY_INIT (gst_rtmp_chunk_stream_debug_category,
        "rtmpchunkstream", 0, "debug category for rtmp chunk streams");
    g_once_init_leave (&done, 1);
  }
}

enum
{
  CHUNK_BYTE_TWOBYTE = 0,
  CHUNK_BYTE_THREEBYTE = 1,
  CHUNK_BYTE_MASK = 0x3f,
  CHUNK_STREAM_MIN_TWOBYTE = 0x40,
  CHUNK_STREAM_MIN_THREEBYTE = 0x140,
  CHUNK_STREAM_MAX_THREEBYTE = 0x1003f,
};

typedef enum
{
  CHUNK_TYPE_0 = 0,
  CHUNK_TYPE_1 = 1,
  CHUNK_TYPE_2 = 2,
  CHUNK_TYPE_3 = 3,
} ChunkType;

static const gsize chunk_header_sizes[4] = { 11, 7, 3, 0 };

struct _GstRtmpChunkStream
{
  GstBuffer *buffer;
  GstRtmpMeta *meta;
  GstMapInfo map;               /* Only used for parsing */
  guint32 id;
  guint32 offset;
  guint64 bytes;
};

struct _GstRtmpChunkStreams
{
  GArray *array;
};

static inline gboolean
chunk_stream_is_open (GstRtmpChunkStream * cstream)
{
  return cstream->map.data != NULL;
}

static void
chunk_stream_take_buffer (GstRtmpChunkStream * cstream, GstBuffer * buffer)
{
  GstRtmpMeta *meta = gst_buffer_get_rtmp_meta (buffer);
  g_assert (meta);
  g_assert (cstream->buffer == NULL);
  cstream->buffer = buffer;
  cstream->meta = meta;
}

static void
chunk_stream_clear (GstRtmpChunkStream * cstream)
{
  if (chunk_stream_is_open (cstream)) {
    gst_buffer_unmap (cstream->buffer, &cstream->map);
    cstream->map.data = NULL;
  }

  gst_buffer_replace (&cstream->buffer, NULL);
  cstream->meta = NULL;
  cstream->offset = 0;
}

static guint32
chunk_stream_next_size (GstRtmpChunkStream * cstream, guint32 chunk_size)
{
  guint32 size, offset;

  size = cstream->meta->size;
  offset = cstream->offset;

  g_return_val_if_fail (chunk_size, 0);
  g_return_val_if_fail (offset <= size, 0);
  return MIN (size - offset, chunk_size);
}

static inline gboolean
needs_ext_ts (GstRtmpMeta * meta)
{
  return meta->ts_delta >= 0xffffff;
}


static guint32
dts_to_abs_ts (GstBuffer * buffer)
{
  GstClockTime dts = GST_BUFFER_DTS (buffer);
  guint32 ret = 0;

  if (GST_CLOCK_TIME_IS_VALID (dts)) {
    ret = gst_util_uint64_scale_round (dts, 1, GST_MSECOND);
  }

  GST_TRACE ("Converted DTS %" GST_TIME_FORMAT " into abs TS %"
      G_GUINT32_FORMAT " ms", GST_TIME_ARGS (dts), ret);
  return ret;
}

static gboolean
dts_diff_to_delta_ts (GstBuffer * old_buffer, GstBuffer * buffer,
    guint32 * out_ts)
{
  GstClockTime dts = GST_BUFFER_DTS (buffer),
      old_dts = GST_BUFFER_DTS (old_buffer);
  guint32 abs_ts, old_abs_ts, delta_32 = 0;

  if (!GST_CLOCK_TIME_IS_VALID (dts) || !GST_CLOCK_TIME_IS_VALID (old_dts)) {
    GST_LOG ("Timestamps not valid; using delta TS 0");
    goto out;
  }

  if (ABS (GST_CLOCK_DIFF (old_dts, dts)) > GST_MSECOND * G_MAXINT32) {
    GST_WARNING ("Timestamp delta too large: %" GST_TIME_FORMAT " -> %"
        GST_TIME_FORMAT, GST_TIME_ARGS (old_dts), GST_TIME_ARGS (dts));
    return FALSE;
  }

  abs_ts = gst_util_uint64_scale_round (dts, 1, GST_MSECOND);
  old_abs_ts = gst_util_uint64_scale_round (old_dts, 1, GST_MSECOND);

  /* underflow wraps around */
  delta_32 = abs_ts - old_abs_ts;

  GST_TRACE ("Converted DTS %" GST_TIME_FORMAT " (%" G_GUINT32_FORMAT
      " ms) -> %" GST_TIME_FORMAT " (%" G_GUINT32_FORMAT " ms) into delta TS %"
      G_GUINT32_FORMAT " ms", GST_TIME_ARGS (old_dts), old_abs_ts,
      GST_TIME_ARGS (dts), abs_ts, delta_32);

out:
  *out_ts = delta_32;
  return TRUE;
}

static ChunkType
select_chunk_type (GstRtmpChunkStream * cstream, GstBuffer * buffer)
{
  GstBuffer *old_buffer = cstream->buffer;
  GstRtmpMeta *meta, *old_meta;

  g_return_val_if_fail (buffer, -1);

  meta = gst_buffer_get_rtmp_meta (buffer);

  g_return_val_if_fail (meta, -1);
  g_return_val_if_fail (gst_rtmp_message_type_is_valid (meta->type), -1);

  meta->size = gst_buffer_get_size (buffer);
  meta->cstream = cstream->id;

  g_return_val_if_fail (meta->size <= GST_RTMP_MAXIMUM_MESSAGE_SIZE, -1);

  if (!old_buffer) {
    GST_TRACE ("Picking header 0: no previous header");
    meta->ts_delta = dts_to_abs_ts (buffer);
    return CHUNK_TYPE_0;
  }

  old_meta = gst_buffer_get_rtmp_meta (old_buffer);
  g_return_val_if_fail (old_meta, -1);

  if (old_meta->mstream != meta->mstream) {
    GST_TRACE ("Picking header 0: stream mismatch; "
        "want %" G_GUINT32_FORMAT " got %" G_GUINT32_FORMAT,
        old_meta->mstream, meta->mstream);
    meta->ts_delta = dts_to_abs_ts (buffer);
    return CHUNK_TYPE_0;
  }

  if (!dts_diff_to_delta_ts (old_buffer, buffer, &meta->ts_delta)) {
    GST_TRACE ("Picking header 0: timestamp delta overflow");
    meta->ts_delta = dts_to_abs_ts (buffer);
    return CHUNK_TYPE_0;
  }

  /* now at least type 1 */

  if (old_meta->type != meta->type) {
    GST_TRACE ("Picking header 1: type mismatch; want %d got %d",
        old_meta->type, meta->type);
    return CHUNK_TYPE_1;
  }

  if (old_meta->size != meta->size) {
    GST_TRACE ("Picking header 1: size mismatch; "
        "want %" G_GUINT32_FORMAT " got %" G_GUINT32_FORMAT,
        old_meta->size, meta->size);
    return CHUNK_TYPE_1;
  }

  /* now at least type 2 */

  if (old_meta->ts_delta != meta->ts_delta) {
    GST_TRACE ("Picking header 2: timestamp delta mismatch; "
        "want %" G_GUINT32_FORMAT " got %" G_GUINT32_FORMAT,
        old_meta->ts_delta, meta->ts_delta);
    return CHUNK_TYPE_2;
  }

  /* now at least type 3 */

  GST_TRACE ("Picking header 3");
  return CHUNK_TYPE_3;
}

static GstBuffer *
serialize_next (GstRtmpChunkStream * cstream, guint32 chunk_size,
    ChunkType type)
{
  GstRtmpMeta *meta = cstream->meta;
  guint8 small_stream_id;
  gsize header_size = chunk_header_sizes[type], offset;
  gboolean ext_ts;
  GstBuffer *ret;
  GstMapInfo map;

  GST_TRACE ("Serializing a chunk of type %d, offset %" G_GUINT32_FORMAT,
      type, cstream->offset);

  if (cstream->id < CHUNK_STREAM_MIN_TWOBYTE) {
    small_stream_id = cstream->id;
    header_size += 1;
  } else if (cstream->id < CHUNK_STREAM_MIN_THREEBYTE) {
    small_stream_id = CHUNK_BYTE_TWOBYTE;
    header_size += 2;
  } else {
    small_stream_id = CHUNK_BYTE_THREEBYTE;
    header_size += 3;
  }

  ext_ts = needs_ext_ts (meta);
  if (ext_ts) {
    header_size += 4;
  }

  GST_TRACE ("Allocating buffer, header size %" G_GSIZE_FORMAT, header_size);

  ret = gst_buffer_new_allocate (NULL, header_size, NULL);
  if (!ret) {
    GST_ERROR ("Failed to allocate chunk buffer");
    return NULL;
  }

  if (!gst_buffer_map (ret, &map, GST_MAP_WRITE)) {
    GST_ERROR ("Failed to map %" GST_PTR_FORMAT, ret);
    gst_buffer_unref (ret);
    return NULL;
  }

  /* Chunk Basic Header */
  GST_WRITE_UINT8 (map.data, (type << 6) | small_stream_id);
  offset = 1;

  switch (small_stream_id) {
    case CHUNK_BYTE_TWOBYTE:
      GST_WRITE_UINT8 (map.data + 1, cstream->id - CHUNK_STREAM_MIN_TWOBYTE);
      offset += 1;
      break;

    case CHUNK_BYTE_THREEBYTE:
      GST_WRITE_UINT16_LE (map.data + 1,
          cstream->id - CHUNK_STREAM_MIN_TWOBYTE);
      offset += 2;
      break;
  }

  switch (type) {
    case CHUNK_TYPE_0:
      /* SRSLY:  "Message stream ID is stored in little-endian format." */
      GST_WRITE_UINT32_LE (map.data + offset + 7, meta->mstream);
      /* no break */
    case CHUNK_TYPE_1:
      GST_WRITE_UINT24_BE (map.data + offset + 3, meta->size);
      GST_WRITE_UINT8 (map.data + offset + 6, meta->type);
      /* no break */
    case CHUNK_TYPE_2:
      GST_WRITE_UINT24_BE (map.data + offset,
          ext_ts ? 0xffffff : meta->ts_delta);
      /* no break */
    case CHUNK_TYPE_3:
      offset += chunk_header_sizes[type];

      if (ext_ts) {
        GST_WRITE_UINT32_BE (map.data + offset, meta->ts_delta);
        offset += 4;
      }
  }

  g_assert (offset == header_size);
  GST_MEMDUMP (">>> chunk header", map.data, offset);

  gst_buffer_unmap (ret, &map);

  GST_BUFFER_OFFSET (ret) = GST_BUFFER_OFFSET_IS_VALID (cstream->buffer) ?
      GST_BUFFER_OFFSET (cstream->buffer) + cstream->offset : cstream->bytes;
  GST_BUFFER_OFFSET_END (ret) = GST_BUFFER_OFFSET (ret);

  if (meta->size > 0) {
    guint32 payload_size = chunk_stream_next_size (cstream, chunk_size);

    GST_TRACE ("Appending %" G_GUINT32_FORMAT " bytes of payload",
        payload_size);

    gst_buffer_copy_into (ret, cstream->buffer, GST_BUFFER_COPY_MEMORY,
        cstream->offset, payload_size);

    GST_BUFFER_OFFSET_END (ret) += payload_size;
    cstream->offset += payload_size;
    cstream->bytes += payload_size;
  } else {
    GST_TRACE ("Chunk has no payload");
  }

  gst_rtmp_buffer_dump (ret, ">>> chunk");

  return ret;
}

void
gst_rtmp_chunk_stream_clear (GstRtmpChunkStream * cstream)
{
  g_return_if_fail (cstream);
  GST_LOG ("Clearing chunk stream %" G_GUINT32_FORMAT, cstream->id);
  chunk_stream_clear (cstream);
}

guint32
gst_rtmp_chunk_stream_parse_id (const guint8 * data, gsize size)
{
  guint32 ret;

  if (size < 1) {
    GST_TRACE ("Not enough bytes to read ID");
    return 0;
  }

  ret = GST_READ_UINT8 (data) & CHUNK_BYTE_MASK;

  switch (ret) {
    case CHUNK_BYTE_TWOBYTE:
      if (size < 2) {
        GST_TRACE ("Not enough bytes to read two-byte ID");
        return 0;
      }

      ret = GST_READ_UINT8 (data + 1) + CHUNK_STREAM_MIN_TWOBYTE;
      break;

    case CHUNK_BYTE_THREEBYTE:
      if (size < 3) {
        GST_TRACE ("Not enough bytes to read three-byte ID");
        return 0;
      }

      ret = GST_READ_UINT16_LE (data + 1) + CHUNK_STREAM_MIN_TWOBYTE;
      break;
  }

  GST_TRACE ("Parsed chunk stream ID %" G_GUINT32_FORMAT, ret);
  return ret;
}

guint32
gst_rtmp_chunk_stream_parse_header (GstRtmpChunkStream * cstream,
    const guint8 * data, gsize size)
{
  GstBuffer *buffer;
  GstRtmpMeta *meta;
  const guint8 *message_header;
  guint32 header_size;
  ChunkType type;
  gboolean has_abs_timestamp = FALSE;

  g_return_val_if_fail (cstream, 0);
  g_return_val_if_fail (cstream->id == gst_rtmp_chunk_stream_parse_id (data,
          size), 0);

  type = GST_READ_UINT8 (data) >> 6;
  GST_TRACE ("Parsing chunk stream %" G_GUINT32_FORMAT " header type %d",
      cstream->id, type);

  switch (GST_READ_UINT8 (data) & CHUNK_BYTE_MASK) {
    case CHUNK_BYTE_TWOBYTE:
      header_size = 2;
      break;
    case CHUNK_BYTE_THREEBYTE:
      header_size = 3;
      break;
    default:
      header_size = 1;
      break;
  }

  message_header = data + header_size;
  header_size += chunk_header_sizes[type];

  if (cstream->buffer) {
    buffer = cstream->buffer;
    meta = cstream->meta;
    g_assert (meta->cstream == cstream->id);
  } else {
    buffer = gst_buffer_new ();
    GST_BUFFER_DTS (buffer) = 0;
    GST_BUFFER_OFFSET (buffer) = cstream->bytes;
    GST_BUFFER_FLAG_SET (buffer, GST_BUFFER_FLAG_DISCONT);

    meta = gst_buffer_add_rtmp_meta (buffer);
    meta->cstream = cstream->id;

    chunk_stream_take_buffer (cstream, buffer);
    GST_DEBUG ("Starting parse with new %" GST_PTR_FORMAT, buffer);
  }

  if (size < header_size) {
    GST_TRACE ("not enough bytes to read header");
    return header_size;
  }

  switch (type) {
    case CHUNK_TYPE_0:
      has_abs_timestamp = TRUE;
      /* SRSLY:  "Message stream ID is stored in little-endian format." */
      meta->mstream = GST_READ_UINT32_LE (message_header + 7);
      /* no break */
    case CHUNK_TYPE_1:
      meta->type = GST_READ_UINT8 (message_header + 6);
      meta->size = GST_READ_UINT24_BE (message_header + 3);
      /* no break */
    case CHUNK_TYPE_2:
      meta->ts_delta = GST_READ_UINT24_BE (message_header);
      /* no break */
    case CHUNK_TYPE_3:
      if (needs_ext_ts (meta)) {
        guint32 timestamp;

        if (size < header_size + 4) {
          GST_TRACE ("not enough bytes to read extended timestamp");
          return header_size + 4;
        }

        GST_TRACE ("Reading extended timestamp");
        timestamp = GST_READ_UINT32_BE (data + header_size);

        if (type == 3 && meta->ts_delta != timestamp) {
          GST_WARNING ("Type 3 extended timestamp does not match expected"
              " timestamp (want %" G_GUINT32_FORMAT " got %" G_GUINT32_FORMAT
              "); assuming it's not present", meta->ts_delta, timestamp);
        } else {
          meta->ts_delta = timestamp;
          header_size += 4;
        }
      }
  }

  GST_MEMDUMP ("<<< chunk header", data, header_size);

  if (!chunk_stream_is_open (cstream)) {
    GstClockTime dts = GST_BUFFER_DTS (buffer);
    guint32 delta_32, abs_32;
    gint64 delta_64;

    if (has_abs_timestamp) {
      abs_32 = meta->ts_delta;
      delta_32 = abs_32 - dts / GST_MSECOND;
    } else {
      delta_32 = meta->ts_delta;
      abs_32 = delta_32 + dts / GST_MSECOND;
    }

    GST_TRACE ("Timestamp delta is %" G_GUINT32_FORMAT " (absolute %"
        G_GUINT32_FORMAT ")", delta_32, abs_32);

    /* emulate signed overflow */
    delta_64 = delta_32;
    if (delta_64 > G_MAXINT32) {
      delta_64 -= G_MAXUINT32;
      delta_64 -= 1;
    }

    delta_64 *= GST_MSECOND;

    if (G_LIKELY (delta_64 >= 0)) {
      /* Normal advancement */
    } else if (G_LIKELY ((guint64) (-delta_64) <= dts)) {
      /* In-bounds regression */
      GST_WARNING ("Timestamp regression: %" GST_STIME_FORMAT,
          GST_STIME_ARGS (delta_64));
    } else {
      /* Out-of-bounds regression */
      GST_WARNING ("Timestamp regression: %" GST_STIME_FORMAT ", offsetting",
          GST_STIME_ARGS (delta_64));
      delta_64 = delta_32 * GST_MSECOND;
    }

    GST_BUFFER_DTS (buffer) += delta_64;

    GST_TRACE ("Adjusted buffer DTS (%" GST_TIME_FORMAT ") by %"
        GST_STIME_FORMAT " to %" GST_TIME_FORMAT, GST_TIME_ARGS (dts),
        GST_STIME_ARGS (delta_64), GST_TIME_ARGS (GST_BUFFER_DTS (buffer)));
  } else {
    GST_TRACE ("Message payload already started; not touching timestamp");
  }

  return header_size;
}

guint32
gst_rtmp_chunk_stream_parse_payload (GstRtmpChunkStream * cstream,
    guint32 chunk_size, guint8 ** data)
{
  GstMemory *mem;

  g_return_val_if_fail (cstream, 0);
  g_return_val_if_fail (cstream->buffer, 0);

  if (!chunk_stream_is_open (cstream)) {
    guint32 size = cstream->meta->size;

    GST_TRACE ("Allocating buffer, payload size %" G_GUINT32_FORMAT, size);

    mem = gst_allocator_alloc (NULL, size, 0);
    if (!mem) {
      GST_ERROR ("Failed to allocate buffer for payload size %"
          G_GUINT32_FORMAT, size);
      return 0;
    }

    gst_buffer_append_memory (cstream->buffer, mem);
    gst_buffer_map (cstream->buffer, &cstream->map, GST_MAP_WRITE);
  }

  g_return_val_if_fail (cstream->map.size == cstream->meta->size, 0);

  if (data) {
    *data = cstream->map.data + cstream->offset;
  }

  return chunk_stream_next_size (cstream, chunk_size);
}

guint32
gst_rtmp_chunk_stream_wrote_payload (GstRtmpChunkStream * cstream,
    guint32 chunk_size)
{
  guint32 size;

  g_return_val_if_fail (cstream, FALSE);
  g_return_val_if_fail (chunk_stream_is_open (cstream), FALSE);

  size = chunk_stream_next_size (cstream, chunk_size);
  cstream->offset += size;
  cstream->bytes += size;

  return chunk_stream_next_size (cstream, chunk_size);
}

GstBuffer *
gst_rtmp_chunk_stream_parse_finish (GstRtmpChunkStream * cstream)
{
  GstBuffer *buffer, *empty;

  g_return_val_if_fail (cstream, NULL);
  g_return_val_if_fail (cstream->buffer, NULL);

  buffer = gst_buffer_ref (cstream->buffer);
  GST_BUFFER_OFFSET_END (buffer) = cstream->bytes;

  gst_rtmp_buffer_dump (buffer, "<<< message");

  chunk_stream_clear (cstream);

  empty = gst_buffer_new ();

  if (!gst_buffer_copy_into (empty, buffer, GST_BUFFER_COPY_META, 0, 0)) {
    GST_ERROR ("copy_into failed");
    return NULL;
  }

  GST_BUFFER_DTS (empty) = GST_BUFFER_DTS (buffer);
  GST_BUFFER_OFFSET (empty) = GST_BUFFER_OFFSET_END (buffer);

  chunk_stream_take_buffer (cstream, empty);

  return buffer;
}

GstBuffer *
gst_rtmp_chunk_stream_serialize_start (GstRtmpChunkStream * cstream,
    GstBuffer * buffer, guint32 chunk_size)
{
  ChunkType type;

  g_return_val_if_fail (cstream, NULL);
  g_return_val_if_fail (GST_IS_BUFFER (buffer), NULL);

  type = select_chunk_type (cstream, buffer);
  g_return_val_if_fail (type >= 0, NULL);

  GST_TRACE ("Starting serialization of message %" GST_PTR_FORMAT
      " into stream %" G_GUINT32_FORMAT, buffer, cstream->id);

  gst_rtmp_buffer_dump (buffer, ">>> message");

  chunk_stream_clear (cstream);
  chunk_stream_take_buffer (cstream, gst_buffer_ref (buffer));

  return serialize_next (cstream, chunk_size, type);
}

GstBuffer *
gst_rtmp_chunk_stream_serialize_next (GstRtmpChunkStream * cstream,
    guint32 chunk_size)
{
  g_return_val_if_fail (cstream, NULL);
  g_return_val_if_fail (cstream->buffer, NULL);

  if (chunk_stream_next_size (cstream, chunk_size) == 0) {
    GST_TRACE ("Message serialization finished");
    return NULL;
  }

  GST_TRACE ("Continuing serialization of message %" GST_PTR_FORMAT
      " into stream %" G_GUINT32_FORMAT, cstream->buffer, cstream->id);

  return serialize_next (cstream, chunk_size, CHUNK_TYPE_3);
}

GstBuffer *
gst_rtmp_chunk_stream_serialize_all (GstRtmpChunkStream * cstream,
    GstBuffer * buffer, guint32 chunk_size)
{
  GstBuffer *outbuf, *nextbuf;

  outbuf = gst_rtmp_chunk_stream_serialize_start (cstream, buffer, chunk_size);
  nextbuf = gst_rtmp_chunk_stream_serialize_next (cstream, chunk_size);

  while (nextbuf) {
    outbuf = gst_buffer_append (outbuf, nextbuf);
    nextbuf = gst_rtmp_chunk_stream_serialize_next (cstream, chunk_size);
  }

  return outbuf;
}

GstRtmpChunkStreams *
gst_rtmp_chunk_streams_new (void)
{
  GstRtmpChunkStreams *cstreams;

  init_debug ();

  cstreams = g_new (GstRtmpChunkStreams, 1);
  cstreams->array = g_array_new (FALSE, TRUE, sizeof (GstRtmpChunkStream));
  g_array_set_clear_func (cstreams->array,
      (GDestroyNotify) gst_rtmp_chunk_stream_clear);
  return cstreams;
}

void
gst_rtmp_chunk_streams_free (gpointer ptr)
{
  GstRtmpChunkStreams *cstreams = ptr;
  g_clear_pointer (&cstreams->array, g_array_unref);
  g_free (cstreams);
}

GstRtmpChunkStream *
gst_rtmp_chunk_streams_get (GstRtmpChunkStreams * cstreams, guint32 id)
{
  GArray *array;
  GstRtmpChunkStream *entry;
  guint i;

  g_return_val_if_fail (cstreams, NULL);
  g_return_val_if_fail (id > CHUNK_BYTE_THREEBYTE, NULL);
  g_return_val_if_fail (id <= CHUNK_STREAM_MAX_THREEBYTE, NULL);

  array = cstreams->array;

  for (i = 0; i < array->len; i++) {
    entry = &g_array_index (array, GstRtmpChunkStream, i);
    if (entry->id == id) {
      GST_TRACE ("Obtaining chunk stream %" G_GUINT32_FORMAT, id);
      return entry;
    }
  }

  GST_DEBUG ("Allocating chunk stream %" G_GUINT32_FORMAT, id);

  g_array_set_size (array, i + 1);
  entry = &g_array_index (array, GstRtmpChunkStream, i);
  entry->id = id;
  return entry;
}
