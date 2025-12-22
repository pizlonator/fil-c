/*
 * camapplication.h - GStreamer CAM (EN50221) Application Layer
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

#ifndef CAM_APPLICATION_LAYER_H
#define CAM_APPLICATION_LAYER_H

#include "cam.h"
#include "camsession.h"

#define CAM_AL(obj) ((CamAL *) obj)
#define CAM_AL_APPLICATION(obj) ((CamALApplication *) obj)

#define CAM_AL_RESOURCE_ID_IS_PUBLIC(resource_id) ((resource_id >> 30) != 3)
#define CAM_AL_RESOURCE_ID_CLASS(resource_id) ((resource_id >> 16) & 0x3FFF)
#define CAM_AL_RESOURCE_ID_TYPE(resource_id) ((resource_id >> 6) & 0x03FF)
#define CAM_AL_RESOURCE_ID_VERSION(resource_id) (resource_id & 0x3F)

#define CAM_AL_RESOURCE_MANAGER_ID 0x10041
#define CAM_AL_APPLICATION_INFO_ID 0x20041
#define CAM_AL_CONDITIONAL_ACCESS_ID 0x30041

typedef struct _CamAL CamAL;
typedef struct _CamALApplication CamALApplication;

struct _CamAL
{
  CamSL *sl;

  GHashTable *applications;
};

struct _CamALApplication
{
  CamAL *al;
  guint resource_id;
  GList *sessions;

  /* vtable */
  CamReturn (*session_request) (CamALApplication *application,
    CamSLSession *session, CamSLResourceStatus *status);
  CamReturn (*open) (CamALApplication *application, CamSLSession *session);
  CamReturn (*close) (CamALApplication *application, CamSLSession *session);
  CamReturn (*data) (CamALApplication *application, CamSLSession *session,
    guint tag, guint8 *buffer, guint length);
};

CamAL *cam_al_new (CamSL *sl);
void cam_al_destroy (CamAL *al);

gboolean cam_al_install (CamAL *al, CamALApplication *application);
gboolean cam_al_uninstall (CamAL *al, CamALApplication *application);
CamALApplication *cam_al_get (CamAL *al, guint resource_id);
GList *cam_al_get_resource_ids (CamAL *al);

void cam_al_calc_buffer_size (CamAL *al, guint body_length,
  guint *buffer_size, guint *offset);

void _cam_al_application_init (CamALApplication *application);
void _cam_al_application_destroy (CamALApplication *application);

CamReturn cam_al_application_write (CamALApplication *application,
  CamSLSession *session, guint tag, guint8 *buffer,
  guint buffer_size, guint body_length);
#endif /* CAM_APPLICATION_LAYER_H */
