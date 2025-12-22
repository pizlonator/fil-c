/*
 * camapplicationinfo.h - CAM (EN50221) Application Info resource
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

#include <string.h>

#include "camapplicationinfo.h"

#define GST_CAT_DEFAULT cam_debug_cat
#define TAG_APPLICATION_INFO_ENQUIRY 0x9F8020
#define TAG_APPLICATION_INFO_REPLY 0x9F8021
#define TAG_APPLICATION_INFO_ENTER_MENU 0x9F8022

static CamReturn session_request_impl (CamALApplication * application,
    CamSLSession * session, CamSLResourceStatus * status);
static CamReturn open_impl (CamALApplication * application,
    CamSLSession * session);
static CamReturn close_impl (CamALApplication * application,
    CamSLSession * session);
static CamReturn data_impl (CamALApplication * application,
    CamSLSession * session, guint tag, guint8 * buffer, guint length);

CamApplicationInfo *
cam_application_info_new (void)
{
  CamApplicationInfo *info;
  CamALApplication *application;

  info = g_new0 (CamApplicationInfo, 1);
  application = CAM_AL_APPLICATION (info);
  _cam_al_application_init (application);
  application->resource_id = CAM_AL_APPLICATION_INFO_ID;
  application->session_request = session_request_impl;
  application->open = open_impl;
  application->close = close_impl;
  application->data = data_impl;

  return info;
}

void
cam_application_info_destroy (CamApplicationInfo * info)
{
  _cam_al_application_destroy (CAM_AL_APPLICATION (info));
  g_free (info);
}

static CamReturn
send_simple (CamApplicationInfo * info, CamSLSession * session, guint tag)
{
  guint8 *buffer;
  guint offset;
  guint buffer_size;
  CamReturn ret;

  cam_al_calc_buffer_size (CAM_AL_APPLICATION (info)->al, 0, &buffer_size,
      &offset);
  buffer = g_malloc (buffer_size);

  ret = cam_al_application_write (CAM_AL_APPLICATION (info), session,
      tag, buffer, buffer_size, 0);

  g_free (buffer);

  return ret;
}

static CamReturn
send_application_info_enquiry (CamApplicationInfo * info,
    CamSLSession * session)
{
  GST_DEBUG ("sending application info enquiry");
  return send_simple (info, session, TAG_APPLICATION_INFO_ENQUIRY);
}

static CamReturn
session_request_impl (CamALApplication * application,
    CamSLSession * session, CamSLResourceStatus * status)
{
  *status = CAM_SL_RESOURCE_STATUS_OPEN;

  return CAM_RETURN_OK;
}

static CamReturn
open_impl (CamALApplication * application, CamSLSession * session)
{
  CamApplicationInfo *info = CAM_APPLICATION_INFO (application);

  return send_application_info_enquiry (info, session);
}

static CamReturn
close_impl (CamALApplication * application, CamSLSession * session)
{
  return CAM_RETURN_OK;
}

static CamReturn
handle_application_info_reply (CamApplicationInfo * info,
    CamSLSession * session, guint8 * buffer, guint length)
{
#ifndef GST_DISABLE_GST_DEBUG
  guint8 type;
  guint8 menu_length;
  gchar menu[256];

  type = buffer[0];
  menu_length = buffer[5];
  menu_length = MIN (menu_length, 255);
  memcpy (menu, buffer + 6, menu_length);
  menu[menu_length] = 0;

  GST_INFO ("application info reply, type: %d, menu: %s", type, menu);
#endif
  return CAM_RETURN_OK;
}

static CamReturn
data_impl (CamALApplication * application, CamSLSession * session,
    guint tag, guint8 * buffer, guint length)
{
  CamReturn ret;
  CamApplicationInfo *info = CAM_APPLICATION_INFO (application);

  switch (tag) {
    case TAG_APPLICATION_INFO_REPLY:
      ret = handle_application_info_reply (info, session, buffer, length);
      break;
    default:
      g_return_val_if_reached (CAM_RETURN_ERROR);
  }

  return ret;
}
