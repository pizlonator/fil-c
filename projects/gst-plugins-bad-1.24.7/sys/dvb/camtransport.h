/*
 * camtransport.h - GStreamer CAM (EN50221) transport layer
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

#ifndef CAM_TRANSPORT_H
#define CAM_TRANSPORT_H

#include <gst/gst.h>
#include "cam.h"
#include "camutils.h"

#define HOST_BUFFER_SIZE 1024

#define CAM_TL(obj) ((CamTL *) obj)
#define CAM_TL_CONNECTION(obj) ((CamTLConnection *) obj)

typedef struct _CamTL CamTL;
typedef struct _CamTLPrivate CamTLPrivate;

typedef struct _CamTLConnection CamTLConnection;
typedef struct _CamTLConnectionPrivate CamTLConnectionPrivate;

typedef enum
{
  CAM_TL_CONNECTION_STATE_CLOSED,
  CAM_TL_CONNECTION_STATE_IN_CREATION,
  CAM_TL_CONNECTION_STATE_OPEN,
  CAM_TL_CONNECTION_STATE_IN_DELETION
} CamTLConnectionState;

struct _CamTL
{
  int fd;
  guint connection_ids;

  GHashTable *connections;

  guint expected_tpdus;

  /* buffer containing module data */
  guint8 buffer [HOST_BUFFER_SIZE];
  /* number of bytes written in the buffer */
  guint buffer_size;
  /* index pointing to the first byte of a TPDU's body */
  guint8 *body;
  /* length of the body part */
  guint body_length;

  /* callbacks */
  void (*request_connection) (CamTL *tl, CamTLConnection *connection);
  void (*connection_created) (CamTL *tl, CamTLConnection *connection);
  void (*connection_deleted) (CamTL *tl, CamTLConnection *connection);
  CamReturn (*connection_data) (CamTL *tl, CamTLConnection *connection,
    guint8 *data, guint length);

  /* used by the upper layer to extend this layer */
  gpointer user_data;
};

struct _CamTLConnection
{
  CamTL *tl;

  guint8 slot;
  guint id;
  CamTLConnectionState state;
  /* TRUE if the last status byte was 0x80, FALSE otherwise */
  gboolean has_data;
  /* NCAS 1.0 sometimes reports that it has data even if it doesn't. After
   * MAX_EMPTY_DATA times that we don't get any data we assume that there's
   * actually no data.
   */
  guint empty_data;
  /* timer restarted every time the connection is polled */
  GTimer *last_poll;

  gpointer user_data;
};

CamTL *cam_tl_new (int cam_device_fd);
void cam_tl_destroy (CamTL *tl);

CamReturn cam_tl_create_connection (CamTL *tl,
  guint8 slot, CamTLConnection **connnection);
CamReturn cam_tl_connection_delete (CamTLConnection *connection);

void cam_tl_calc_buffer_size (CamTL *tl,
  guint body_length, guint *buffer_size, guint *offset);

CamReturn cam_tl_connection_write (CamTLConnection *connection,
  guint8 *data, guint buffer_size, guint body_length);

CamReturn cam_tl_connection_poll (CamTLConnection *connection, gboolean force);
CamReturn cam_tl_read_all (CamTL *tl, gboolean poll);

#endif /* CAM_TRANSPORT_H */
