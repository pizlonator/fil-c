/*
 * camswclient.c - GStreamer softcam client
 * Copyright (C) 2007 Alessandro Decina
 * 
 * Authors:
 *   Alessandro Decina <alessandro.d@gmail.com>
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

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>

#include <gst/gst.h>

#include "camswclient.h"
#include "cam.h"

#define GST_CAT_DEFAULT cam_debug_cat
#define UNIX_PATH_MAX 108

CamSwClient *
cam_sw_client_new (void)
{
  CamSwClient *client = g_new0 (CamSwClient, 1);

  client->state = CAM_SW_CLIENT_STATE_CLOSED;

  return client;
}

static void
reset_state (CamSwClient * client)
{
  if (client->sock)
    close (client->sock);

  g_free (client->sock_path);
}

void
cam_sw_client_free (CamSwClient * client)
{
  g_return_if_fail (client != NULL);

  if (client->state != CAM_SW_CLIENT_STATE_CLOSED)
    GST_WARNING ("client not in CLOSED state when free'd");

  reset_state (client);
  g_free (client);
}

gboolean
cam_sw_client_open (CamSwClient * client, const char *sock_path)
{
  struct sockaddr_un addr;
  int ret;

  g_return_val_if_fail (client != NULL, FALSE);
  g_return_val_if_fail (client->state == CAM_SW_CLIENT_STATE_CLOSED, FALSE);
  g_return_val_if_fail (sock_path != NULL, FALSE);

  /* sun.path needs to end up NULL-terminated */
  if (strlen (sock_path) >= (sizeof (addr.sun_path) - 1)) {
    GST_ERROR ("sock_path is too long");
    return FALSE;
  }

  addr.sun_family = AF_UNIX;
  memcpy (addr.sun_path, sock_path, strlen (sock_path) + 1);

  GST_INFO ("connecting to softcam socket: %s", sock_path);
  if ((client->sock = socket (PF_UNIX, SOCK_STREAM, 0)) < 0) {
    GST_ERROR ("Failed to create a socket, error: %s", g_strerror (errno));
    return FALSE;
  }
  ret =
      connect (client->sock, (struct sockaddr *) &addr,
      sizeof (struct sockaddr_un));
  if (ret != 0) {
    GST_ERROR ("error opening softcam socket %s, error: %s",
        sock_path, g_strerror (errno));

    return FALSE;
  }

  client->sock_path = g_strdup (sock_path);
  client->state = CAM_SW_CLIENT_STATE_OPEN;

  return TRUE;
}

void
cam_sw_client_close (CamSwClient * client)
{
  g_return_if_fail (client != NULL);
  g_return_if_fail (client->state == CAM_SW_CLIENT_STATE_OPEN);

  reset_state (client);
  client->state = CAM_SW_CLIENT_STATE_CLOSED;
}

static void
send_ca_pmt (CamSwClient * client, GstMpegtsPMT * pmt,
    guint8 list_management, guint8 cmd_id)
{
  guint8 *buffer;
  guint buffer_size;
  guint8 *ca_pmt;
  guint ca_pmt_size;
  guint length_field_len;
  guint header_len;

  ca_pmt = cam_build_ca_pmt (pmt, list_management, cmd_id, &ca_pmt_size);

  length_field_len = cam_calc_length_field_size (ca_pmt_size);
  header_len = 3 + length_field_len;
  buffer_size = header_len + ca_pmt_size;

  buffer = g_malloc0 (buffer_size);
  memcpy (buffer + header_len, ca_pmt, ca_pmt_size);

  /* ca_pmt resource_id */
  buffer[0] = 0x9F;
  buffer[1] = 0x80;
  buffer[2] = 0x32;

  cam_write_length_field (&buffer[3], ca_pmt_size);

  if (write (client->sock, buffer, buffer_size) == -1) {
    GST_WARNING ("write failed when sending PMT with error: %s (%d)",
        g_strerror (errno), errno);
  }

  g_free (ca_pmt);
  g_free (buffer);
}

void
cam_sw_client_set_pmt (CamSwClient * client, GstMpegtsPMT * pmt)
{
  g_return_if_fail (client != NULL);
  g_return_if_fail (pmt != NULL);

  return send_ca_pmt (client, pmt, 0x03 /* only */ ,
      0x01 /* ok_descrambling */ );
}

void
cam_sw_client_update_pmt (CamSwClient * client, GstMpegtsPMT * pmt)
{
  g_return_if_fail (client != NULL);
  g_return_if_fail (pmt != NULL);

  return send_ca_pmt (client, pmt, 0x05 /* update */ ,
      0x01 /* ok_descrambling */ );
}
