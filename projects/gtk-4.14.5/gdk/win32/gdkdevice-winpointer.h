/* GDK - The GIMP Drawing Kit
 * Copyright (C) 2020 the GTK team
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

#include <gdk/gdkdeviceprivate.h>

#include <windows.h>

#include "winpointer.h"

G_BEGIN_DECLS

#define GDK_TYPE_DEVICE_WINPOINTER         (gdk_device_winpointer_get_type ())
#define GDK_DEVICE_WINPOINTER(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), GDK_TYPE_DEVICE_WINPOINTER, GdkDeviceWinpointer))
#define GDK_DEVICE_WINPOINTER_CLASS(c)     (G_TYPE_CHECK_CLASS_CAST ((c), GDK_TYPE_DEVICE_WINPOINTER, GdkDeviceWinpointerClass))
#define GDK_IS_DEVICE_WINPOINTER(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), GDK_TYPE_DEVICE_WINPOINTER))
#define GDK_IS_DEVICE_WINPOINTER_CLASS(c)  (G_TYPE_CHECK_CLASS_TYPE ((c), GDK_TYPE_DEVICE_WINPOINTER))
#define GDK_DEVICE_WINPOINTER_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), GDK_TYPE_DEVICE_WINPOINTER, GdkDeviceWinpointerClass))

typedef struct _GdkDeviceWinpointer GdkDeviceWinpointer;
typedef struct _GdkDeviceWinpointerClass GdkDeviceWinpointerClass;

struct _GdkDeviceWinpointer
{
  GdkDevice parent_instance;

  HANDLE device_handle;
  UINT32 start_cursor_id;
  UINT32 end_cursor_id;

  int origin_x;
  int origin_y;
  double scale_x;
  double scale_y;

  GdkModifierType last_button_mask;

  GdkDeviceTool *tool_pen;
  GdkDeviceTool *tool_eraser;
};

struct _GdkDeviceWinpointerClass
{
  GdkDeviceClass parent_class;
};

GType gdk_device_winpointer_get_type (void) G_GNUC_CONST;

G_END_DECLS

