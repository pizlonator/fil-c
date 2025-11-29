/* GDK - The GIMP Drawing Kit
 * Copyright (C) 2009 Carlos Garnacho <carlosg@gnome.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include "gdkdevicetool.h"

G_BEGIN_DECLS

typedef struct _GdkDeviceToolClass GdkDeviceToolClass;

struct _GdkDeviceTool
{
  GObject parent_instance;
  guint64 serial;
  guint64 hw_id;
  GdkDeviceToolType type;
  GdkAxisFlags tool_axes;
};

struct _GdkDeviceToolClass
{
  GObjectClass parent_class;
};

GdkDeviceTool *gdk_device_tool_new    (guint64            serial,
                                       guint64            hw_id,
                                       GdkDeviceToolType  type,
                                       GdkAxisFlags       tool_axes);

G_END_DECLS

