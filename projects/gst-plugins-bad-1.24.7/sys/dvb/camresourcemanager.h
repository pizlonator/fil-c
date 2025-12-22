/*
 * camresourcemanager.h - GStreamer CAM (EN50221) Resource Manager
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

#ifndef CAM_RESOURCE_MANAGER_H
#define CAM_RESOURCE_MANAGER_H

#include "camapplication.h"

#define CAM_RESOURCE_MANAGER(obj) ((CamResourceManager *) obj)

typedef struct _CamResourceManager CamResourceManager;

struct _CamResourceManager
{
  CamALApplication application;
};

CamResourceManager *cam_resource_manager_new (void);
void cam_resource_manager_destroy (CamResourceManager *manager);

#endif /* CAM_RESOURCE_MANAGER_H */
