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
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "rtmphandshake.h"
#include "rtmputils.h"

#include <gst/gst.h>
#include <string.h>

GST_DEBUG_CATEGORY_STATIC (gst_rtmp_handshake_debug_category);
#define GST_CAT_DEFAULT gst_rtmp_handshake_debug_category

static void
init_debug (void)
{
  static gsize done = 0;
  if (g_once_init_enter (&done)) {
    GST_DEBUG_CATEGORY_INIT (gst_rtmp_handshake_debug_category, "rtmphandshake",
        0, "debug category for the rtmp connection handshake");
    g_once_init_leave (&done, 1);
  }
}

static void client_handshake1_done (GObject * source, GAsyncResult * result,
    gpointer user_data);
static void client_handshake2_done (GObject * source, GAsyncResult * result,
    gpointer user_data);
static void client_handshake3_done (GObject * source, GAsyncResult * result,
    gpointer user_data);

static inline void
serialize_u8 (GByteArray * array, guint8 value)
{
  g_byte_array_append (array, (guint8 *) & value, sizeof value);
}

static inline void
serialize_u32 (GByteArray * array, guint32 value)
{
  value = GUINT32_TO_BE (value);
  g_byte_array_append (array, (guint8 *) & value, sizeof value);
}

#define SIZE_P0 1
#define SIZE_P1 1536
#define SIZE_P2 SIZE_P1
#define SIZE_P0P1 (SIZE_P0 + SIZE_P1)
#define SIZE_P0P1P2 (SIZE_P0P1 + SIZE_P2)
#define SIZE_RANDOM (SIZE_P1 - 8)

typedef struct
{
  GBytes *random_bytes;
  gboolean strict;
} HandshakeData;

static GBytes *
handshake_random_data (void)
{
  G_STATIC_ASSERT (SIZE_RANDOM % 4 == 0);

  GByteArray *ba = g_byte_array_sized_new (SIZE_RANDOM);
  gint i;

  for (i = 0; i < SIZE_RANDOM; i += 4) {
    serialize_u32 (ba, g_random_int ());
  }

  return g_byte_array_free_to_bytes (ba);
}

static HandshakeData *
handshake_data_new (gboolean strict)
{
  HandshakeData *data = g_new0 (HandshakeData, 1);
  data->random_bytes = handshake_random_data ();
  data->strict = strict;
  return data;
}

static void
handshake_data_free (gpointer ptr)
{
  HandshakeData *data = ptr;
  g_clear_pointer (&data->random_bytes, g_bytes_unref);
  g_free (data);
}

static gboolean
handshake_data_check (HandshakeData * data, const guint8 * p2)
{
  const guint8 *ourrandom = g_bytes_get_data (data->random_bytes, NULL);
  return memcmp (ourrandom, p2 + 8, SIZE_P2 - 8) == 0;
}

static GBytes *
create_c0c1 (GBytes * random_bytes)
{
  GByteArray *ba = g_byte_array_sized_new (SIZE_P0P1);

  /* C0 version */
  serialize_u8 (ba, 3);

  /* C1 time */
  serialize_u32 (ba, g_get_monotonic_time () / 1000);

  /* C1 zero */
  serialize_u32 (ba, 0);

  /* C1 random data */
  gst_rtmp_byte_array_append_bytes (ba, random_bytes);

  GST_DEBUG ("Sending C0+C1");
  GST_MEMDUMP (">>> C0", ba->data, SIZE_P0);
  GST_MEMDUMP (">>> C1", ba->data + SIZE_P0, SIZE_P1);

  return g_byte_array_free_to_bytes (ba);
}

void
gst_rtmp_client_handshake (GIOStream * stream, gboolean strict,
    GCancellable * cancellable, GAsyncReadyCallback callback,
    gpointer user_data)
{
  GTask *task;
  HandshakeData *data;

  g_return_if_fail (G_IS_IO_STREAM (stream));

  init_debug ();
  GST_INFO ("Starting client handshake");

  task = g_task_new (stream, cancellable, callback, user_data);
  data = handshake_data_new (strict);
  g_task_set_task_data (task, data, handshake_data_free);

  {
    GOutputStream *os = g_io_stream_get_output_stream (stream);
    GBytes *bytes = create_c0c1 (data->random_bytes);

    gst_rtmp_output_stream_write_all_bytes_async (os,
        bytes, G_PRIORITY_DEFAULT,
        g_task_get_cancellable (task), client_handshake1_done, task);

    g_bytes_unref (bytes);
  }
}

static void
client_handshake1_done (GObject * source, GAsyncResult * result,
    gpointer user_data)
{
  GOutputStream *os = G_OUTPUT_STREAM (source);
  GTask *task = user_data;
  GIOStream *stream = g_task_get_source_object (task);
  GInputStream *is = g_io_stream_get_input_stream (stream);
  GError *error = NULL;
  gboolean res;

  res = gst_rtmp_output_stream_write_all_bytes_finish (os, result, &error);
  if (!res) {
    GST_ERROR ("Failed to send C0+C1: %s", error->message);
    g_task_return_error (task, error);
    g_object_unref (task);
    return;
  }

  GST_DEBUG ("Sent C0+C1, waiting for S0+S1+S2");
  gst_rtmp_input_stream_read_all_bytes_async (is, SIZE_P0P1P2,
      G_PRIORITY_DEFAULT, g_task_get_cancellable (task),
      client_handshake2_done, task);
}

static GBytes *
create_c2 (const guint8 * s0s1s2)
{
  G_STATIC_ASSERT (SIZE_P1 == SIZE_P2);

  GByteArray *ba = g_byte_array_sized_new (SIZE_P2);
  gint64 c2time = g_get_monotonic_time ();

  /* Copy S1 to C2 */
  g_byte_array_set_size (ba, SIZE_P2);
  memcpy (ba->data, s0s1s2 + SIZE_P0, SIZE_P1);

  /* C2 time2 */
  GST_WRITE_UINT32_BE (ba->data + 4, c2time / 1000);

  GST_DEBUG ("Sending C2");
  GST_MEMDUMP (">>> C2", ba->data, SIZE_P2);

  return g_byte_array_free_to_bytes (ba);
}

static void
client_handshake2_done (GObject * source, GAsyncResult * result,
    gpointer user_data)
{
  GInputStream *is = G_INPUT_STREAM (source);
  GTask *task = user_data;
  GIOStream *stream = g_task_get_source_object (task);
  HandshakeData *data = g_task_get_task_data (task);
  GError *error = NULL;
  GBytes *res;
  const guint8 *s0s1s2;
  gsize size;

  res = gst_rtmp_input_stream_read_all_bytes_finish (is, result, &error);
  if (!res) {
    GST_ERROR ("Failed to read S0+S1+S2: %s", error->message);
    g_task_return_error (task, error);
    g_object_unref (task);
    return;
  }

  s0s1s2 = g_bytes_get_data (res, &size);
  if (size < SIZE_P0P1P2) {
    GST_ERROR ("Short read (want %d have %" G_GSIZE_FORMAT ")", SIZE_P0P1P2,
        size);
    g_task_return_new_error (task, G_IO_ERROR, G_IO_ERROR_PARTIAL_INPUT,
        "Short read (want %d have %" G_GSIZE_FORMAT ")", SIZE_P0P1P2, size);
    g_object_unref (task);
    goto out;
  }

  GST_DEBUG ("Got S0+S1+S2");
  GST_MEMDUMP ("<<< S0", s0s1s2, SIZE_P0);
  GST_MEMDUMP ("<<< S1", s0s1s2 + SIZE_P0, SIZE_P1);
  GST_MEMDUMP ("<<< S2", s0s1s2 + SIZE_P0P1, SIZE_P2);

  if (handshake_data_check (data, s0s1s2 + SIZE_P0P1)) {
    GST_DEBUG ("S2 random data matches C1");
  } else {
    if (data->strict) {
      GST_ERROR ("Handshake response data did not match");
      g_task_return_new_error (task, G_IO_ERROR, G_IO_ERROR_INVALID_DATA,
          "Handshake response data did not match");
      g_object_unref (task);
      goto out;
    }

    GST_WARNING ("Handshake reponse data did not match; continuing anyway");
  }

  {
    GOutputStream *os = g_io_stream_get_output_stream (stream);
    GBytes *bytes = create_c2 (s0s1s2);

    gst_rtmp_output_stream_write_all_bytes_async (os,
        bytes, G_PRIORITY_DEFAULT,
        g_task_get_cancellable (task), client_handshake3_done, task);

    g_bytes_unref (bytes);
  }

out:
  g_bytes_unref (res);
}

static void
client_handshake3_done (GObject * source, GAsyncResult * result,
    gpointer user_data)
{
  GOutputStream *os = G_OUTPUT_STREAM (source);
  GTask *task = user_data;
  GError *error = NULL;
  gboolean res;

  res = gst_rtmp_output_stream_write_all_bytes_finish (os, result, &error);
  if (!res) {
    GST_ERROR ("Failed to send C2: %s", error->message);
    g_task_return_error (task, error);
    g_object_unref (task);
    return;
  }

  GST_DEBUG ("Sent C2");
  GST_INFO ("Client handshake finished");

  g_task_return_boolean (task, TRUE);
  g_object_unref (task);
}

gboolean
gst_rtmp_client_handshake_finish (GIOStream * stream, GAsyncResult * result,
    GError ** error)
{
  g_return_val_if_fail (g_task_is_valid (result, stream), FALSE);
  return g_task_propagate_boolean (G_TASK (result), error);
}
