/*
 * camsession.h - GStreamer CAM (EN50221) Session Layer
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

#ifndef CAM_SESSION_H
#define CAM_SESSION_H

#include "cam.h"
#include "camtransport.h"

#define CAM_SL(obj) ((CamSL *) obj)
#define CAM_SL_SESSION(obj) ((CamSLSession *) obj)

typedef struct _CamSL CamSL;
typedef struct _CamSLSession CamSLSession;

typedef enum
{
  CAM_SL_SESSION_STATE_IDLE,
  CAM_SL_SESSION_STATE_OPENING,
  CAM_SL_SESSION_STATE_ACTIVE,
  CAM_SL_SESSION_STATE_CLOSING
} CamSLSessionState;

typedef enum
{
  CAM_SL_RESOURCE_STATUS_OPEN = 0x00,
  CAM_SL_RESOURCE_STATUS_NOT_FOUND = 0xF0,
  CAM_SL_RESOURCE_STATUS_UNAVAILABLE = 0xF1,
  CAM_SL_RESOURCE_STATUS_INVALID_VERSION = 0xF2,
  CAM_SL_RESOURCE_STATUS_BUSY = 0xF3
} CamSLResourceStatus;

struct _CamSL
{
  CamTL *tl;

  GHashTable *sessions;
  guint session_ids;

  /* callbacks */
  CamReturn (*open_session_request) (CamSL *sl, CamSLSession *session,
    CamSLResourceStatus *status);
  CamReturn (*session_opened) (CamSL *sl, CamSLSession *session);
  CamReturn (*session_closed) (CamSL *sl, CamSLSession *session);
  CamReturn (*session_data) (CamSL *sl, CamSLSession *session,
    guint8 *data, guint length);

  gpointer user_data;
};

struct _CamSLSession
{
  CamSL *sl;
  CamTLConnection *connection;

  guint resource_id;
  guint16 session_nb;

  CamSLSessionState state;

  gpointer user_data;
};

CamSL *cam_sl_new (CamTL *transport);
void cam_sl_destroy (CamSL *sl);

CamReturn cam_sl_create_session (CamSL *sl, CamTLConnection *connection,
  guint resource_id, CamSLSession **session);
CamReturn cam_sl_session_close (CamSLSession *session);

void cam_sl_calc_buffer_size (CamSL *sl,
  guint body_length, guint *buffer_size, guint *offset);
CamReturn cam_sl_session_write (CamSLSession *session,
  guint8 *buffe, guint buffer_size, guint body_length);

#endif /* CAM_SESSION_H */
