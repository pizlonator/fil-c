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

#ifndef CAM_APPLICATION_INFO_H
#define CAM_APPLICATION_INFO_H

#include "camapplication.h"

#define CAM_APPLICATION_INFO(obj) ((CamApplicationInfo *) obj)

typedef struct _CamApplicationInfo CamApplicationInfo;

struct _CamApplicationInfo
{
  CamALApplication application;
};

CamApplicationInfo *cam_application_info_new (void);
void cam_application_info_destroy (CamApplicationInfo *info);

#endif /* CAM_APPLICATION_INFO_H */
