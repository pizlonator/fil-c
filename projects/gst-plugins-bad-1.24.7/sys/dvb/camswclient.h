/*
 * camswclient.h - GStreamer softcam client
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

#ifndef CAM_SW_CLIENT_H
#define CAM_SW_CLIENT_H

#include <glib.h>
#include "camutils.h"

typedef enum
{
  CAM_SW_CLIENT_STATE_CLOSED,
  CAM_SW_CLIENT_STATE_OPEN,
} CamSwClientState;

typedef struct
{
  /* private */
  CamSwClientState state;
  char *sock_path;
  int sock;

} CamSwClient;

CamSwClient *cam_sw_client_new (void);
void cam_sw_client_free (CamSwClient *sw_client);

gboolean cam_sw_client_open (CamSwClient *sw_client, const char *sock_path);
void cam_sw_client_close (CamSwClient *sw_client);

void cam_sw_client_set_pmt (CamSwClient *sw_client, GstMpegtsPMT *pmt);
void cam_sw_client_update_pmt (CamSwClient *sw_client, GstMpegtsPMT *pmt);

#endif /* CAM_SW_CLIENT_H */
