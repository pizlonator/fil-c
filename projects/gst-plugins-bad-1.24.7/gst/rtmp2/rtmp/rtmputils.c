/* GStreamer RTMP Library
 * Copyright (C) 2013 David Schleef <ds@schleef.org>
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

#include "rtmputils.h"
#include <string.h>

static void read_all_bytes_done (GObject * source, GAsyncResult * result,
    gpointer user_data);
static void write_all_bytes_done (GObject * source, GAsyncResult * result,
    gpointer user_data);
static void write_all_buffer_done (GObject * source, GAsyncResult * result,
    gpointer user_data);

void
gst_rtmp_byte_array_append_bytes (GByteArray * bytearray, GBytes * bytes)
{
  const guint8 *data;
  gsize size;
  guint offset;

  g_return_if_fail (bytearray);

  offset = bytearray->len;
  data = g_bytes_get_data (bytes, &size);

  g_return_if_fail (data);

  g_byte_array_set_size (bytearray, offset + size);
  memcpy (bytearray->data + offset, data, size);
}

void
gst_rtmp_input_stream_read_all_bytes_async (GInputStream * stream, gsize count,
    int io_priority, GCancellable * cancellable, GAsyncReadyCallback callback,
    gpointer user_data)
{
  GTask *task;
  GByteArray *ba;

  g_return_if_fail (G_IS_INPUT_STREAM (stream));

  task = g_task_new (stream, cancellable, callback, user_data);

  ba = g_byte_array_sized_new (count);
  g_byte_array_set_size (ba, count);
  g_task_set_task_data (task, ba, (GDestroyNotify) g_byte_array_unref);

  g_input_stream_read_all_async (stream, ba->data, count, io_priority,
      cancellable, read_all_bytes_done, task);
}

static void
read_all_bytes_done (GObject * source, GAsyncResult * result,
    gpointer user_data)
{
  GInputStream *is = G_INPUT_STREAM (source);
  GTask *task = user_data;
  GByteArray *ba = g_task_get_task_data (task);
  GError *error = NULL;
  gboolean res;
  gsize bytes_read;
  GBytes *bytes;

  res = g_input_stream_read_all_finish (is, result, &bytes_read, &error);
  if (!res) {
    g_task_return_error (task, error);
    g_object_unref (task);
    return;
  }

  g_byte_array_set_size (ba, bytes_read);
  bytes = g_byte_array_free_to_bytes (g_byte_array_ref (ba));

  g_task_return_pointer (task, bytes, (GDestroyNotify) g_bytes_unref);
  g_object_unref (task);
}

GBytes *
gst_rtmp_input_stream_read_all_bytes_finish (GInputStream * stream,
    GAsyncResult * result, GError ** error)
{
  g_return_val_if_fail (g_task_is_valid (result, stream), FALSE);
  return g_task_propagate_pointer (G_TASK (result), error);
}

void
gst_rtmp_output_stream_write_all_bytes_async (GOutputStream * stream,
    GBytes * bytes, int io_priority, GCancellable * cancellable,
    GAsyncReadyCallback callback, gpointer user_data)
{
  GTask *task;
  const void *data;
  gsize size;

  g_return_if_fail (G_IS_OUTPUT_STREAM (stream));
  g_return_if_fail (bytes);

  data = g_bytes_get_data (bytes, &size);
  g_return_if_fail (data);

  task = g_task_new (stream, cancellable, callback, user_data);
  g_task_set_task_data (task, g_bytes_ref (bytes),
      (GDestroyNotify) g_bytes_unref);

  g_output_stream_write_all_async (stream, data, size, io_priority,
      cancellable, write_all_bytes_done, task);
}

static void
write_all_bytes_done (GObject * source, GAsyncResult * result,
    gpointer user_data)
{
  GOutputStream *os = G_OUTPUT_STREAM (source);
  GTask *task = user_data;
  GError *error = NULL;
  gboolean res;

  res = g_output_stream_write_all_finish (os, result, NULL, &error);
  if (!res) {
    g_task_return_error (task, error);
    g_object_unref (task);
    return;
  }

  g_task_return_boolean (task, TRUE);
  g_object_unref (task);
}

gboolean
gst_rtmp_output_stream_write_all_bytes_finish (GOutputStream * stream,
    GAsyncResult * result, GError ** error)
{
  g_return_val_if_fail (g_task_is_valid (result, stream), FALSE);
  return g_task_propagate_boolean (G_TASK (result), error);
}

typedef struct
{
  GstBuffer *buffer;
  GstMapInfo map;
  gboolean mapped;
  gsize bytes_written;
} WriteAllBufferData;

static WriteAllBufferData *
write_all_buffer_data_new (GstBuffer * buffer)
{
  WriteAllBufferData *data = g_new0 (WriteAllBufferData, 1);
  data->buffer = gst_buffer_ref (buffer);
  return data;
}

static void
write_all_buffer_data_free (gpointer ptr)
{
  WriteAllBufferData *data = ptr;
  if (data->mapped) {
    gst_buffer_unmap (data->buffer, &data->map);
  }
  g_clear_pointer (&data->buffer, gst_buffer_unref);
  g_free (data);
}

void
gst_rtmp_output_stream_write_all_buffer_async (GOutputStream * stream,
    GstBuffer * buffer, int io_priority, GCancellable * cancellable,
    GAsyncReadyCallback callback, gpointer user_data)
{
  GTask *task;
  WriteAllBufferData *data;

  g_return_if_fail (G_IS_OUTPUT_STREAM (stream));
  g_return_if_fail (GST_IS_BUFFER (buffer));

  task = g_task_new (stream, cancellable, callback, user_data);

  data = write_all_buffer_data_new (buffer);
  g_task_set_task_data (task, data, write_all_buffer_data_free);

  if (!gst_buffer_map (buffer, &data->map, GST_MAP_READ)) {
    g_task_return_new_error (task, GST_RESOURCE_ERROR, GST_RESOURCE_ERROR_READ,
        "Failed to map buffer for reading");
    g_object_unref (task);
    return;
  }

  data->mapped = TRUE;

  g_output_stream_write_all_async (stream, data->map.data, data->map.size,
      io_priority, cancellable, write_all_buffer_done, task);
}

static void
write_all_buffer_done (GObject * source, GAsyncResult * result,
    gpointer user_data)
{
  GOutputStream *os = G_OUTPUT_STREAM (source);
  GTask *task = user_data;
  WriteAllBufferData *data = g_task_get_task_data (task);
  GError *error = NULL;
  gboolean res;

  res = g_output_stream_write_all_finish (os, result, &data->bytes_written,
      &error);

  gst_buffer_unmap (data->buffer, &data->map);
  data->mapped = FALSE;

  if (!res) {
    g_task_return_error (task, error);
    g_object_unref (task);
    return;
  }

  g_task_return_boolean (task, TRUE);
  g_object_unref (task);
}


gboolean
gst_rtmp_output_stream_write_all_buffer_finish (GOutputStream * stream,
    GAsyncResult * result, gsize * bytes_written, GError ** error)
{
  WriteAllBufferData *data;
  GTask *task;

  g_return_val_if_fail (g_task_is_valid (result, stream), FALSE);
  task = G_TASK (result);

  data = g_task_get_task_data (task);
  if (bytes_written) {
    *bytes_written = data->bytes_written;
  }

  return g_task_propagate_boolean (task, error);
}

static const gchar ascii_table[128] = {
  0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
  0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
  0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
  0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
  ' ', '!', 0x0, '#', '$', '%', '&', '\'',
  '(', ')', '*', '+', ',', '-', '.', '/',
  '0', '1', '2', '3', '4', '5', '6', '7',
  '8', '9', ':', ';', '<', '=', '>', '?',
  '@', 'A', 'B', 'C', 'D', 'E', 'F', 'G',
  'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O',
  'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W',
  'X', 'Y', 'Z', '[', 0x0, ']', '^', '_',
  '`', 'a', 'b', 'c', 'd', 'e', 'f', 'g',
  'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o',
  'p', 'q', 'r', 's', 't', 'u', 'v', 'w',
  'x', 'y', 'z', '{', '|', '}', '~', 0x0,
};

static const gchar ascii_escapes[128] = {
  0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 'a',
  'b', 't', 'n', 'v', 'f', 'r', 0x0, 0x0,
  0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
  0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
  0x0, 0x0, '"', 0x0, 0x0, 0x0, 0x0, 0x0,
  0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
  0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
  0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
  0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
  0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
  0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
  0x0, 0x0, 0x0, 0x0, '\\', 0x0, 0x0, 0x0,
  0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
  0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
  0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
  0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
};

void
gst_rtmp_string_print_escaped (GString * string, const gchar * data,
    gssize size)
{
  gssize i;

  g_return_if_fail (string);

  if (!data) {
    g_string_append (string, "(NULL)");
    return;
  }

  g_string_append_c (string, '"');

  for (i = 0; size < 0 ? data[i] != 0 : i < size; i++) {
    guchar c = data[i];

    if (G_LIKELY (c < G_N_ELEMENTS (ascii_table))) {
      if (ascii_table[c]) {
        g_string_append_c (string, c);
        continue;
      }

      if (ascii_escapes[c]) {
        g_string_append_c (string, '\\');
        g_string_append_c (string, ascii_escapes[c]);
        continue;
      }
    } else {
      gunichar uc = g_utf8_get_char_validated (data + i,
          size < 0 ? -1 : size - i);
      if (uc != (gunichar) (-2) && uc != (gunichar) (-1)) {
        if (g_unichar_isprint (uc)) {
          g_string_append_unichar (string, uc);
        } else if (uc <= G_MAXUINT16) {
          g_string_append_printf (string, "\\u%04X", uc);
        } else {
          g_string_append_printf (string, "\\U%08X", uc);
        }

        i += g_utf8_skip[c] - 1;
        continue;
      }
    }

    g_string_append_printf (string, "\\x%02X", c);
  }

  g_string_append_c (string, '"');

}

gboolean
gst_rtmp_flv_tag_parse_header (GstRtmpFlvTagHeader * header,
    const guint8 * data, gsize size)
{
  g_return_val_if_fail (header, FALSE);
  g_return_val_if_fail (data, FALSE);

  /* Parse FLVTAG header as described in
   * video_file_format_spec_v10.pdf page 5 (page 9 of the PDF) */

  if (size < GST_RTMP_FLV_TAG_HEADER_SIZE) {
    return FALSE;
  }

  /* TagType UI8 */
  header->type = GST_READ_UINT8 (data);

  /* DataSize UI24 */
  header->payload_size = GST_READ_UINT24_BE (data + 1);

  /* 4 bytes for the PreviousTagSize UI32 following every tag */
  header->total_size = GST_RTMP_FLV_TAG_HEADER_SIZE + header->payload_size + 4;

  /* Timestamp UI24 + TimestampExtended UI8 */
  header->timestamp = GST_READ_UINT24_BE (data + 4);
  header->timestamp |= (guint32) GST_READ_UINT8 (data + 7) << 24;

  /* Skip StreamID UI24. It's "always 0" for FLV files and for aggregated RTMP
   * messages we're supposed to use the Stream ID from the AGGREGATE. */

  return TRUE;
}
