/* GStreamer RTMP Library
 * Copyright (C) 2013 David Schleef <ds@schleef.org>
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

#ifndef _GST_RTMP_UTILS_H_
#define _GST_RTMP_UTILS_H_

#include <gst/gst.h>
#include <gio/gio.h>
#include "rtmpmessage.h"

G_BEGIN_DECLS

void gst_rtmp_byte_array_append_bytes (GByteArray * bytearray, GBytes * bytes);

void gst_rtmp_input_stream_read_all_bytes_async (GInputStream * stream,
    gsize count, int io_priority, GCancellable * cancellable,
    GAsyncReadyCallback callback, gpointer user_data);
GBytes * gst_rtmp_input_stream_read_all_bytes_finish (GInputStream * stream,
    GAsyncResult * result, GError ** error);

void gst_rtmp_output_stream_write_all_bytes_async (GOutputStream * stream,
    GBytes * bytes, int io_priority, GCancellable * cancellable,
    GAsyncReadyCallback callback, gpointer user_data);
gboolean gst_rtmp_output_stream_write_all_bytes_finish (GOutputStream * stream,
    GAsyncResult * result, GError ** error);

void gst_rtmp_output_stream_write_all_buffer_async (GOutputStream * stream,
    GstBuffer * buffer, int io_priority, GCancellable * cancellable,
    GAsyncReadyCallback callback, gpointer user_data);
gboolean gst_rtmp_output_stream_write_all_buffer_finish (GOutputStream * stream,
    GAsyncResult * result, gsize * bytes_written, GError ** error);

void gst_rtmp_string_print_escaped (GString * string, const gchar * data,
    gssize size);

#define GST_RTMP_FLV_TAG_HEADER_SIZE 11

typedef struct {
  GstRtmpMessageType type;
  gsize payload_size, total_size;
  guint32 timestamp;
} GstRtmpFlvTagHeader;

gboolean gst_rtmp_flv_tag_parse_header (GstRtmpFlvTagHeader *header,
    const guint8 * data, gsize size);

G_END_DECLS

#endif
