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

/**
 * plugin-unixfd:
 *
 * Since: 1.24
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gstunixfd.h"

#include <gio/gunixfdmessage.h>
#include <gio/gunixsocketaddress.h>

typedef struct
{
  guint32 type;
  guint32 payload_size;
} Command;

/* For backward compatibility, do not change the size of Command. It should have
 * same size on 32bits and 64bits arches. */
G_STATIC_ASSERT (sizeof (Command) == 8);
G_STATIC_ASSERT (sizeof (MemoryPayload) == 16);
G_STATIC_ASSERT (sizeof (NewBufferPayload) == 56);
G_STATIC_ASSERT (sizeof (ReleaseBufferPayload) == 8);

gboolean
gst_unix_fd_send_command (GSocket * socket, CommandType type, GUnixFDList * fds,
    const guint8 * payload, gsize payload_size, GError ** error)
{
  Command command = { type, payload_size };
  GOutputVector vect[] = {
    {&command, sizeof (Command)},
    {payload, payload_size},
  };
  GSocketControlMessage *msg = NULL;
  gint num_msg = 0;
  gboolean ret = TRUE;

  if (fds != NULL) {
    msg = g_unix_fd_message_new_with_fd_list (fds);
    num_msg++;
  }

  if (g_socket_send_message (socket, NULL, vect, G_N_ELEMENTS (vect),
          &msg, num_msg, G_SOCKET_MSG_NONE, NULL, error) < 0) {
    ret = FALSE;
  }

  g_clear_object (&msg);
  return ret;
}

gboolean
gst_unix_fd_receive_command (GSocket * socket, GCancellable * cancellable,
    CommandType * type, GUnixFDList ** fds, guint8 ** payload,
    gsize * payload_size, GError ** error)
{
  Command command;
  GInputVector vect = { &command, sizeof (Command) };
  GSocketControlMessage **msg = NULL;
  gint num_msg = 0;
  gint flags = G_SOCKET_MSG_NONE;
  gboolean ret = TRUE;

  if (g_socket_receive_message (socket, NULL, &vect, 1, &msg, &num_msg, &flags,
          cancellable, error) <= 0) {
    return FALSE;
  }

  *type = command.type;
  *payload = NULL;
  *payload_size = 0;

  if (command.payload_size > 0) {
    *payload = g_malloc (command.payload_size);
    *payload_size = command.payload_size;
    if (g_socket_receive (socket, (gchar *) * payload, command.payload_size,
            cancellable, error) < (gssize) command.payload_size) {
      g_clear_pointer (payload, g_free);
      ret = FALSE;
      goto out;
    }
  }

  if (fds != NULL) {
    *fds = NULL;
    for (int i = 0; i < num_msg; i++) {
      if (G_IS_UNIX_FD_MESSAGE (msg[i])) {
        if (*fds != NULL) {
          g_set_error (error, GST_STREAM_ERROR, GST_STREAM_ERROR_FAILED,
              "Received more than one fd message");
          g_clear_pointer (payload, g_free);
          g_clear_object (fds);
          ret = FALSE;
          goto out;
        }
        *fds = g_object_ref (g_unix_fd_message_get_fd_list ((GUnixFDMessage *)
                msg[i]));
      }
    }
  }

out:
  for (int i = 0; i < num_msg; i++)
    g_object_unref (msg[i]);
  g_free (msg);

  return ret;
}

gboolean
gst_unix_fd_parse_new_buffer (guint8 * payload, gsize payload_size,
    NewBufferPayload ** new_buffer, guint32 * consumed)
{
  if (payload == NULL || payload_size < sizeof (NewBufferPayload))
    return FALSE;

  *new_buffer = (NewBufferPayload *) payload;
  gsize struct_size =
      sizeof (NewBufferPayload) +
      sizeof (MemoryPayload) * (*new_buffer)->n_memory;
  if (payload_size < struct_size)
    return FALSE;

  *consumed = struct_size;

  return TRUE;
}

gboolean
gst_unix_fd_parse_release_buffer (guint8 * payload, gsize payload_size,
    ReleaseBufferPayload ** release_buffer)
{
  if (payload == NULL || payload_size < sizeof (ReleaseBufferPayload))
    return FALSE;

  *release_buffer = (ReleaseBufferPayload *) payload;

  return TRUE;
}

gboolean
gst_unix_fd_parse_caps (guint8 * payload, gsize payload_size, gchar ** caps_str)
{
  if (payload == NULL || payload_size < 1 || payload[payload_size - 1] != '\0')
    return FALSE;

  *caps_str = (gchar *) payload;

  return TRUE;
}

GSocket *
gst_unix_fd_socket_new (const gchar * socket_path,
    GUnixSocketAddressType socket_type, GSocketAddress ** address,
    GError ** error)
{
  if (socket_path == NULL) {
    g_set_error_literal (error, GST_STREAM_ERROR, GST_STREAM_ERROR_FAILED,
        "Socket path is NULL");
    return NULL;
  }

  switch (socket_type) {
    case G_UNIX_SOCKET_ADDRESS_PATH:
    case G_UNIX_SOCKET_ADDRESS_ABSTRACT:
    case G_UNIX_SOCKET_ADDRESS_ABSTRACT_PADDED:
      *address =
          g_unix_socket_address_new_with_type (socket_path, -1, socket_type);
      break;
    default:
    {
      gchar *str =
          g_enum_to_string (G_TYPE_UNIX_SOCKET_ADDRESS_TYPE, socket_type);
      g_set_error (error, GST_STREAM_ERROR, GST_STREAM_ERROR_FAILED,
          "Unsupported UNIX socket type %s", str);
      g_free (str);
      return NULL;
    }
  }

  GSocket *socket = g_socket_new (G_SOCKET_FAMILY_UNIX, G_SOCKET_TYPE_STREAM,
      G_SOCKET_PROTOCOL_DEFAULT, error);
  if (socket == NULL)
    g_clear_object (address);
  return socket;
}

static gboolean
plugin_init (GstPlugin * plugin)
{
  gboolean ret = FALSE;

  ret |= GST_ELEMENT_REGISTER (unixfdsrc, plugin);
  ret |= GST_ELEMENT_REGISTER (unixfdsink, plugin);

  return ret;
}

GST_PLUGIN_DEFINE (GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    unixfd,
    "Unix file descriptor sink and source",
    plugin_init, VERSION, GST_LICENSE, GST_PACKAGE_NAME, GST_PACKAGE_ORIGIN)
