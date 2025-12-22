/*
 * camdevice.h - GStreamer hardware CAM interface
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

#ifndef CAM_DEVICE_H
#define CAM_DEVICE_H

#include "camtransport.h"
#include "camsession.h"
#include "camapplication.h"
#include "camresourcemanager.h"
#include "camapplicationinfo.h"
#include "camconditionalaccess.h"

typedef enum
{
  CAM_DEVICE_STATE_CLOSED,
  CAM_DEVICE_STATE_OPEN,
} CamDeviceState;

typedef struct
{
  /* private */
  CamDeviceState state;
  char *filename;
  int fd;

  /* EN50221 layers */
  CamTL *tl;
  CamSL *sl;
  CamAL *al;

  /* apps provided by us */
  CamResourceManager *mgr;
  CamApplicationInfo *info;
  CamConditionalAccess *cas;
} CamDevice;

CamDevice *cam_device_new (void);
void cam_device_free (CamDevice *device);

gboolean cam_device_open (CamDevice *device, const char *filename);
void cam_device_close (CamDevice *device);

gboolean cam_device_ready (CamDevice *device);
void cam_device_poll (CamDevice *device);
void cam_device_set_pmt (CamDevice *device,
  GstMpegtsPMT *pmt, CamConditionalAccessPmtFlag flag);

#endif /* CAM_DEVICE_H */
