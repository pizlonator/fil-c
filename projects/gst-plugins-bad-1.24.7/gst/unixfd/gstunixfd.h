/* GStreamer unix file-descriptor source/sink
 *
 * Copyright (C) 2023 Netflix Inc.
 *  Author: Xavier Claessens <xavier.claessens@collabora.com>
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

#pragma once

#include <gst/gst.h>
#include <gio/gio.h>
#include <gio/gunixfdlist.h>

G_BEGIN_DECLS

typedef enum
{
  COMMAND_TYPE_NEW_BUFFER = 0,
  COMMAND_TYPE_RELEASE_BUFFER = 1,
  COMMAND_TYPE_CAPS = 2,
  COMMAND_TYPE_EOS = 3,
} CommandType;

typedef enum
{
  MEMORY_TYPE_DEFAULT = 0,
  MEMORY_TYPE_DMABUF = 1,
  MEMORY_TYPE_LAST,
} MemoryType;

typedef struct {
  guint64 size;
  guint64 offset;
} MemoryPayload;

typedef struct {
  guint64 id;
  guint64 pts;
  guint64 dts;
  guint64 duration;
  guint64 offset;
  guint64 offset_end;
  guint32 flags;
  guint8 type;
  guint8 n_memory;
  guint16 n_meta;
  MemoryPayload memories[];
} NewBufferPayload;

typedef struct {
  guint64 id;
} ReleaseBufferPayload;

gboolean gst_unix_fd_send_command(GSocket * socket, CommandType type,
    GUnixFDList * fds, const guint8 * payload, gsize payload_size,
    GError ** error);
gboolean gst_unix_fd_receive_command (GSocket *socket,
    GCancellable *cancellable, CommandType *type, GUnixFDList **fds,
    guint8 **payload, gsize *payload_size, GError **error);

gboolean gst_unix_fd_parse_new_buffer (guint8 *payload, gsize payload_size,
    NewBufferPayload **new_buffer, guint32 *consumed);
gboolean gst_unix_fd_parse_release_buffer(guint8 *payload, gsize payload_size,
    ReleaseBufferPayload **release_buffer);
gboolean gst_unix_fd_parse_caps (guint8 *payload, gsize payload_size,
    gchar **caps_str);

GSocket *gst_unix_fd_socket_new(const gchar *socket_path,
    GUnixSocketAddressType socket_type, GSocketAddress **address,
    GError **error);

GST_ELEMENT_REGISTER_DECLARE (unixfdsrc);
GST_ELEMENT_REGISTER_DECLARE (unixfdsink);

G_END_DECLS
