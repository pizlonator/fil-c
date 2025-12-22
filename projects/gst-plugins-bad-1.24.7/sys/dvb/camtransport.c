/*
 * camtransport.c - GStreamer CAM (EN50221) transport layer
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

#include "camtransport.h"
#include <sys/select.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>

#define GST_CAT_DEFAULT cam_debug_cat
#define READ_TIMEOUT_SEC 2
#define READ_TIMEOUT_USEC 0

#define POLL_INTERVAL 0.300

/* Layer tags */
#define TAG_SB 0x80
#define TAG_RCV 0x81
#define TAG_CREATE_T_C 0x82
#define TAG_C_T_C_REPLY 0x83
#define TAG_DELETE_T_C 0x84
#define TAG_D_T_C_REPLY 0x85
#define TAG_REQUEST_T_C 0x86
#define TAG_NEW_T_C 0x87
#define TAG_T_C_ERROR 0x88
#define TAG_DATA_MORE 0xA1
#define TAG_DATA_LAST 0xA0

/* Session tags */
#define TAG_SESSION_NUMBER 0x90
#define TAG_OPEN_SESSION_REQUEST 0x91
#define TAG_OPEN_SESSION_RESPONSE 0x92
#define TAG_CREATE_SESSION 0x93
#define TAG_CREATE_SESSION_RESPONSE 0x94
#define TAG_CLOSE_SESSION_REQUEST 0x95
#define TAG_CLOSE_SESSION_RESPONSE 0x96


typedef struct
{
  guint tagid;
  const gchar *description;
} CamTagMessage;

static const CamTagMessage debugmessage[] = {
  {TAG_SB, "SB"},
  {TAG_RCV, "RCV"},
  {TAG_CREATE_T_C, "CREATE_T_C"},
  {TAG_C_T_C_REPLY, "CREATE_T_C_REPLY"},
  {TAG_DELETE_T_C, "DELETE_T_C"},
  {TAG_D_T_C_REPLY, "DELETE_T_C_REPLY"},
  {TAG_REQUEST_T_C, "REQUEST_T_C"},
  {TAG_NEW_T_C, "NEW_T_C"},
  {TAG_T_C_ERROR, "T_C_ERROR"},
  {TAG_SESSION_NUMBER, "SESSION_NUMBER"},
  {TAG_OPEN_SESSION_REQUEST, "OPEN_SESSION_REQUEST"},
  {TAG_OPEN_SESSION_RESPONSE, "OPEN_SESSION_RESPONSE"},
  {TAG_CREATE_SESSION, "CREATE_SESSION"},
  {TAG_CREATE_SESSION_RESPONSE, "CREATE_SESSION_RESPONSE"},
  {TAG_CLOSE_SESSION_REQUEST, "CLOSE_SESSION_REQUEST"},
  {TAG_CLOSE_SESSION_RESPONSE, "CLOSE_SESSION_RESPONSE"},
  {TAG_DATA_MORE, "DATA_MORE"},
  {TAG_DATA_LAST, "DATA_LAST"}
};

static inline const gchar *
tag_get_name (guint tagid)
{
  guint i;

  for (i = 0; i < G_N_ELEMENTS (debugmessage); i++)
    if (debugmessage[i].tagid == tagid)
      return debugmessage[i].description;
  return "UNKNOWN";
}

/* utility struct used to store the state of the connections in cam_tl_read_next
 */
typedef struct
{
  GList *active;
  GList *idle;
} CamTLConnectionsStatus;

void cam_gst_util_dump_mem (const guchar * mem, guint size);

static CamTLConnection *
cam_tl_connection_new (CamTL * tl, guint8 id)
{
  CamTLConnection *connection;

  connection = g_new0 (CamTLConnection, 1);
  connection->tl = tl;
  connection->id = id;
  connection->state = CAM_TL_CONNECTION_STATE_CLOSED;
  connection->has_data = FALSE;

  return connection;
}

static void
cam_tl_connection_destroy (CamTLConnection * connection)
{
  if (connection->last_poll)
    g_timer_destroy (connection->last_poll);

  g_free (connection);
}

CamTL *
cam_tl_new (int fd)
{
  CamTL *tl;

  tl = g_new0 (CamTL, 1);
  tl->fd = fd;
  tl->connections = g_hash_table_new_full (g_direct_hash, g_direct_equal,
      NULL, (GDestroyNotify) cam_tl_connection_destroy);

  return tl;
}

void
cam_tl_destroy (CamTL * tl)
{
  g_hash_table_destroy (tl->connections);

  g_free (tl);
}

/* read data from the module without blocking indefinitely */
static CamReturn
cam_tl_read_timeout (CamTL * tl, struct timeval *timeout)
{
  fd_set read_fd;
  int sret;

  FD_ZERO (&read_fd);
  FD_SET (tl->fd, &read_fd);

  sret = select (tl->fd + 1, &read_fd, NULL, NULL, timeout);
  if (sret == 0) {
    GST_DEBUG ("read timeout");
    return CAM_RETURN_TRANSPORT_TIMEOUT;
  }

  tl->buffer_size = read (tl->fd, &tl->buffer, HOST_BUFFER_SIZE);
  if (tl->buffer_size == -1) {
    GST_ERROR ("error reading tpdu: %s", g_strerror (errno));
    return CAM_RETURN_TRANSPORT_ERROR;
  }

  return CAM_RETURN_OK;
}

/* read data from the module using the default timeout */
static CamReturn
cam_tl_read (CamTL * tl)
{
  struct timeval timeout;

  timeout.tv_sec = READ_TIMEOUT_SEC;
  timeout.tv_usec = READ_TIMEOUT_USEC;

  return cam_tl_read_timeout (tl, &timeout);
}

/* get the number of bytes to allocate for a TPDU with a body of body_length
 * bytes. Also get the offset from the beginning of the buffer that marks the
 * position of the first byte of the TPDU body */
void
cam_tl_calc_buffer_size (CamTL * tl, guint body_length,
    guint * buffer_size, guint * offset)
{
  guint length_field_len;

  /* the size of a TPDU is:
   * 1 byte slot number
   * 1 byte connection id 
   * length_field_len bytes length field 
   * 1 byte connection id
   * body_length bytes body
   */

  /* get the length of the lenght_field block */
  length_field_len = cam_calc_length_field_size (body_length);

  *offset = 3 + length_field_len + 1;
  *buffer_size = *offset + body_length;
}

/* write the header of a TPDU
 * NOTE: this function assumes that the buffer is large enough to contain the
 * complete TPDU (see cam_tl_calc_buffer_size ()) and that enough space has been
 * left from the beginning of the buffer to write the TPDU header.
 */
static CamReturn
cam_tl_connection_write_tpdu (CamTLConnection * connection,
    guint8 tag, guint8 * buffer, guint buffer_size, guint body_length)
{
  int sret;
  CamTL *tl = connection->tl;
  guint8 length_field_len;

  /* slot number */
  buffer[0] = connection->slot;
  /* connection number */
  buffer[1] = connection->id;
  /* tag */
  buffer[2] = tag;
  /* length can take 1 to 4 bytes */
  length_field_len = cam_write_length_field (&buffer[3], body_length);
  buffer[3 + length_field_len] = connection->id;

  GST_DEBUG ("writing TPDU %x (%s) connection %d (size:%d)",
      buffer[2], tag_get_name (buffer[2]), connection->id, buffer_size);

  //cam_gst_util_dump_mem (buffer, buffer_size);

  sret = write (tl->fd, buffer, buffer_size);
  if (sret == -1) {
    GST_ERROR ("error witing TPDU (%d): %s", errno, g_strerror (errno));
    return CAM_RETURN_TRANSPORT_ERROR;
  }

  tl->expected_tpdus += 1;

  GST_DEBUG ("Success writing tpdu 0x%x (%s)", buffer[2],
      tag_get_name (buffer[2]));

  return CAM_RETURN_OK;
}

/* convenience function to write control TPDUs (TPDUs having a single-byte body)
 */
static CamReturn
cam_tl_connection_write_control_tpdu (CamTLConnection * connection, guint8 tag)
{
  guint8 tpdu[5];

  /* TPDU layout (5 bytes):
   *
   * slot number (1 byte)
   * connection id (1 byte)
   * tag (1 byte)
   * length (1 byte)
   * connection id (1 byte)
   */

  return cam_tl_connection_write_tpdu (connection, tag, tpdu, 5, 1);
}

/* read the next TPDU from the CAM */
static CamReturn
cam_tl_read_tpdu_next (CamTL * tl, CamTLConnection ** out_connection)
{
  CamReturn ret;
  CamTLConnection *connection;
  guint8 connection_id;
  guint8 *tpdu;
  guint8 length_field_len;
  guint8 status;

  ret = cam_tl_read (tl);
  if (CAM_FAILED (ret))
    return ret;

  tpdu = tl->buffer;

  /* must hold at least slot, connection_id, 1byte length_field, connection_id
   */
  if (tl->buffer_size < 4) {
    GST_ERROR ("invalid TPDU length %d", tl->buffer_size);
    return CAM_RETURN_TRANSPORT_ERROR;
  }

  /* LPDU slot */
  /* slot = tpdu[0]; */
  /* LPDU connection id */
  connection_id = tpdu[1];

  connection = g_hash_table_lookup (tl->connections,
      GINT_TO_POINTER ((guint) connection_id));
  if (connection == NULL) {
    /* WHAT? */
    GST_ERROR ("CAM sent a TPDU on an unknown connection: %d", connection_id);
    return CAM_RETURN_TRANSPORT_ERROR;
  }

  /* read the length_field () */
  length_field_len = cam_read_length_field (&tpdu[3], &tl->body_length);

  if (tl->body_length + 3 > tl->buffer_size) {
    GST_ERROR ("invalid TPDU length_field (%d) exceeds "
        "the size of the buffer (%d)", tl->body_length, tl->buffer_size);
    return CAM_RETURN_TRANSPORT_ERROR;
  }

  /* skip slot + connection id + tag + lenght_field () + connection id */
  tl->body = tpdu + 4 + length_field_len;
  /* do not count the connection id byte as part of the body */
  tl->body_length -= 1;

  if (tl->buffer[tl->buffer_size - 4] != TAG_SB) {
    GST_ERROR ("no TAG_SB appended to TPDU");
    return CAM_RETURN_TRANSPORT_ERROR;
  }

  status = tl->buffer[tl->buffer_size - 1];
  if (status & 0x80) {
    connection->has_data = TRUE;
  } else {
    connection->has_data = FALSE;
  }

  GST_DEBUG ("received TPDU %x (%s) more data %d", tpdu[2],
      tag_get_name (tpdu[2]), connection->has_data);
  tl->expected_tpdus -= 1;

  *out_connection = connection;

  return CAM_RETURN_OK;
}

/* create a connection with the module */
CamReturn
cam_tl_create_connection (CamTL * tl, guint8 slot,
    CamTLConnection ** connection)
{
  CamReturn ret;
  CamTLConnection *conn = NULL;
  guint count = 10;

  if (tl->connection_ids == 255)
    return CAM_RETURN_TRANSPORT_TOO_MANY_CONNECTIONS;

  conn = cam_tl_connection_new (tl, ++tl->connection_ids);

  /* Some CA devices take a long time to set themselves up,
   * therefore retry every 250ms (for a maximum of 2.5s)
   */
  while (TRUE) {
    /* send a TAG_CREATE_T_C TPDU */
    ret = cam_tl_connection_write_control_tpdu (conn, TAG_CREATE_T_C);
    if (!CAM_FAILED (ret))
      break;
    if (!count)
      goto error;
    GST_DEBUG ("Failed sending initial connection message .. but retrying");
    count--;
    /* Wait 250ms */
    g_usleep (G_USEC_PER_SEC / 4);
  }

  g_hash_table_insert (tl->connections, GINT_TO_POINTER (conn->id), conn);

  *connection = conn;

  return CAM_RETURN_OK;

error:
  if (conn)
    cam_tl_connection_destroy (conn);

  return ret;
}

CamReturn
cam_tl_connection_delete (CamTLConnection * connection)
{
  CamReturn ret;

  ret = cam_tl_connection_write_control_tpdu (connection, TAG_DELETE_T_C);
  if (CAM_FAILED (ret))
    return ret;

  connection->state = CAM_TL_CONNECTION_STATE_IN_DELETION;

  return CAM_RETURN_OK;
}

static CamReturn
handle_control_tpdu (CamTL * tl, CamTLConnection * connection)
{
  if (tl->body_length != 0) {
    GST_ERROR ("got control tpdu of invalid length: %d", tl->body_length);
    return CAM_RETURN_TRANSPORT_ERROR;
  }

  switch (tl->buffer[2]) {
      /* create transport connection reply */
    case TAG_C_T_C_REPLY:
      /* a connection might be closed before it's acknowledged */
      if (connection->state != CAM_TL_CONNECTION_STATE_IN_DELETION) {
        GST_DEBUG ("connection created %d", connection->id);
        connection->state = CAM_TL_CONNECTION_STATE_OPEN;

        if (tl->connection_created)
          tl->connection_created (tl, connection);
      }
      break;
      /* delete transport connection reply */
    case TAG_D_T_C_REPLY:
      connection->state = CAM_TL_CONNECTION_STATE_CLOSED;
      GST_DEBUG ("connection closed %d", connection->id);

      if (tl->connection_deleted)
        tl->connection_deleted (tl, connection);

      g_hash_table_remove (tl->connections,
          GINT_TO_POINTER ((guint) connection->id));
      break;
  }

  return CAM_RETURN_OK;
}

static CamReturn
handle_data_tpdu (CamTL * tl, CamTLConnection * connection)
{
  if (tl->body_length == 0) {
    /* FIXME: figure out why this seems to happen from time to time with the
     * predator cam */
    GST_WARNING ("Empty data TPDU received");
    return CAM_RETURN_OK;
  }

  if (tl->connection_data)
    return tl->connection_data (tl, connection, tl->body, tl->body_length);

  return CAM_RETURN_OK;
}

static void
foreach_connection_get (gpointer key, gpointer value, gpointer user_data)
{
  GList **lst = (GList **) user_data;

  *lst = g_list_append (*lst, value);
}

CamReturn
cam_tl_connection_poll (CamTLConnection * connection, gboolean force)
{
  CamReturn ret;

  if (connection->last_poll == NULL) {
    connection->last_poll = g_timer_new ();
  } else if (!force &&
      g_timer_elapsed (connection->last_poll, NULL) < POLL_INTERVAL) {
    return CAM_RETURN_TRANSPORT_POLL;
  }

  GST_DEBUG ("polling connection %d", connection->id);
  ret = cam_tl_connection_write_control_tpdu (connection, TAG_DATA_LAST);
  if (CAM_FAILED (ret))
    return ret;

  g_timer_start (connection->last_poll);

  return CAM_RETURN_OK;
}

/* read all the queued TPDUs */
CamReturn
cam_tl_read_all (CamTL * tl, gboolean poll)
{
  CamReturn ret = CAM_RETURN_OK;
  CamTLConnection *connection;
  GList *connections = NULL;
  GList *walk;
  gboolean done = FALSE;

  while (!done) {
    while (tl->expected_tpdus) {
      /* read the next TPDU from the connection */
      ret = cam_tl_read_tpdu_next (tl, &connection);
      if (CAM_FAILED (ret)) {
        GST_ERROR ("error reading TPDU from module: %d", ret);
        goto out;
      }

      switch (tl->buffer[2]) {
        case TAG_C_T_C_REPLY:
        case TAG_D_T_C_REPLY:
          connection->empty_data = 0;
          ret = handle_control_tpdu (tl, connection);
          break;
        case TAG_DATA_MORE:
        case TAG_DATA_LAST:
          connection->empty_data = 0;
          ret = handle_data_tpdu (tl, connection);
          break;
        case TAG_SB:
          /* this is handled by tpdu_next */
          break;
      }

      if (CAM_FAILED (ret))
        goto out;
    }

    done = TRUE;

    connections = NULL;
    g_hash_table_foreach (tl->connections,
        foreach_connection_get, &connections);

    for (walk = connections; walk; walk = walk->next) {
      CamTLConnection *connection = CAM_TL_CONNECTION (walk->data);

      if (connection->has_data == TRUE && connection->empty_data < 10) {
        ret = cam_tl_connection_write_control_tpdu (connection, TAG_RCV);
        if (CAM_FAILED (ret)) {
          g_list_free (connections);
          goto out;
        }
        /* increment the empty_data counter. If we get data, this will be reset
         * to 0 */
        connection->empty_data++;
        done = FALSE;
      } else if (poll) {
        ret = cam_tl_connection_poll (connection, FALSE);
        if (ret == CAM_RETURN_TRANSPORT_POLL)
          continue;

        if (CAM_FAILED (ret)) {
          g_list_free (connections);
          goto out;
        }

        done = FALSE;
      }
    }

    g_list_free (connections);
  }

out:
  return ret;
}

CamReturn
cam_tl_connection_write (CamTLConnection * connection,
    guint8 * buffer, guint buffer_size, guint body_length)
{
  return cam_tl_connection_write_tpdu (connection,
      TAG_DATA_LAST, buffer, buffer_size, 1 + body_length);
}
