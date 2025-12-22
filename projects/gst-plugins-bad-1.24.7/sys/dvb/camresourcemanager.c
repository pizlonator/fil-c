/*
 * camresourcemanager.c - GStreamer CAM (EN50221) Resource Manager
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

#include "camresourcemanager.h"

#define GST_CAT_DEFAULT cam_debug_cat
#define TAG_PROFILE_ENQUIRY 0x9F8010
#define TAG_PROFILE_REPLY 0x9F8011
#define TAG_PROFILE_CHANGE 0x9F8012

static CamReturn session_request_impl (CamALApplication * application,
    CamSLSession * session, CamSLResourceStatus * status);
static CamReturn open_impl (CamALApplication * application,
    CamSLSession * session);
static CamReturn close_impl (CamALApplication * application,
    CamSLSession * session);
static CamReturn data_impl (CamALApplication * application,
    CamSLSession * session, guint tag, guint8 * buffer, guint length);

CamResourceManager *
cam_resource_manager_new (void)
{
  CamALApplication *application;
  CamResourceManager *mgr;

  mgr = g_new0 (CamResourceManager, 1);
  application = CAM_AL_APPLICATION (mgr);
  _cam_al_application_init (application);
  application->resource_id = CAM_AL_RESOURCE_MANAGER_ID;
  application->session_request = session_request_impl;
  application->open = open_impl;
  application->close = close_impl;
  application->data = data_impl;

  return mgr;
}

void
cam_resource_manager_destroy (CamResourceManager * mgr)
{
  _cam_al_application_destroy (CAM_AL_APPLICATION (mgr));
  g_free (mgr);
}

static CamReturn
session_request_impl (CamALApplication * application,
    CamSLSession * session, CamSLResourceStatus * status)
{
  *status = CAM_SL_RESOURCE_STATUS_OPEN;

  return CAM_RETURN_OK;
}

static CamReturn
send_simple (CamResourceManager * mgr, CamSLSession * session, guint tag)
{
  guint8 *buffer;
  guint offset;
  guint buffer_size;
  CamReturn ret;

  cam_al_calc_buffer_size (CAM_AL_APPLICATION (mgr)->al, 0, &buffer_size,
      &offset);
  buffer = g_malloc (buffer_size);

  ret = cam_al_application_write (CAM_AL_APPLICATION (mgr), session,
      tag, buffer, buffer_size, 0);

  g_free (buffer);

  return ret;
}

static CamReturn
send_profile_enquiry (CamResourceManager * mgr, CamSLSession * session)
{
  GST_DEBUG ("sending profile enquiry");
  return send_simple (mgr, session, TAG_PROFILE_ENQUIRY);
}

static CamReturn
send_profile_change (CamResourceManager * mgr, CamSLSession * session)
{
  GST_DEBUG ("sending profile change");
  return send_simple (mgr, session, TAG_PROFILE_CHANGE);
}

static CamReturn
send_profile_reply (CamResourceManager * mgr, CamSLSession * session)
{
  CamReturn ret;
  guint8 *buffer;
  guint8 *apdu_body;
  guint buffer_size;
  guint offset;
  GList *resource_ids;
  guint resource_ids_size;
  GList *walk;

  resource_ids = cam_al_get_resource_ids (CAM_AL_APPLICATION (mgr)->al);
  resource_ids_size = g_list_length (resource_ids) * 4;

  cam_al_calc_buffer_size (CAM_AL_APPLICATION (mgr)->al, resource_ids_size,
      &buffer_size, &offset);

  buffer = g_malloc (buffer_size);
  apdu_body = buffer + offset;

  for (walk = resource_ids; walk != NULL; walk = walk->next) {
    GST_WRITE_UINT32_BE (apdu_body, GPOINTER_TO_UINT (walk->data));

    apdu_body += 4;
  }

  g_list_free (resource_ids);

  GST_DEBUG ("sending profile reply");
  ret = cam_al_application_write (CAM_AL_APPLICATION (mgr), session,
      TAG_PROFILE_REPLY, buffer, buffer_size, resource_ids_size);

  g_free (buffer);

  return ret;
}

static CamReturn
open_impl (CamALApplication * application, CamSLSession * session)
{
  CamResourceManager *mgr = CAM_RESOURCE_MANAGER (application);

  return send_profile_enquiry (mgr, session);
}

static CamReturn
close_impl (CamALApplication * application, CamSLSession * session)
{
  return CAM_RETURN_OK;
}

static CamReturn
handle_profile_reply (CamResourceManager * mgr, CamSLSession * session,
    guint8 * buffer, guint length)
{
  /* the APDU contains length / 4 resource_ids */
  /* FIXME: implement this once CamApplicationProxy is implemented */
  GST_DEBUG ("got profile reply");
  return send_profile_change (mgr, session);
}

static CamReturn
data_impl (CamALApplication * application, CamSLSession * session,
    guint tag, guint8 * buffer, guint length)
{
  CamResourceManager *mgr = CAM_RESOURCE_MANAGER (application);

  switch (tag) {
    case TAG_PROFILE_ENQUIRY:
      send_profile_reply (mgr, session);
      break;
    case TAG_PROFILE_REPLY:
      handle_profile_reply (mgr, session, buffer, length);
      break;
    case TAG_PROFILE_CHANGE:
      send_profile_enquiry (mgr, session);
      break;
    default:
      g_return_val_if_reached (CAM_RETURN_APPLICATION_ERROR);
  }

  /* FIXME: Shouldn't this return the retval from the functions above ? */
  return CAM_RETURN_OK;
}
