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

#include <gdk/gdkdeviceprivate.h>

G_BEGIN_DECLS

#define GDK_TYPE_DEVICE_WIN32         (gdk_device_win32_get_type ())
#define GDK_DEVICE_WIN32(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), GDK_TYPE_DEVICE_WIN32, GdkDeviceWin32))
#define GDK_DEVICE_WIN32_CLASS(c)     (G_TYPE_CHECK_CLASS_CAST ((c), GDK_TYPE_DEVICE_WIN32, GdkDeviceWin32Class))
#define GDK_IS_DEVICE_WIN32(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), GDK_TYPE_DEVICE_WIN32))
#define GDK_IS_DEVICE_WIN32_CLASS(c)  (G_TYPE_CHECK_CLASS_TYPE ((c), GDK_TYPE_DEVICE_WIN32))
#define GDK_DEVICE_WIN32_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), GDK_TYPE_DEVICE_WIN32, GdkDeviceWin32Class))

typedef struct _GdkDeviceWin32 GdkDeviceWin32;
typedef struct _GdkDeviceWin32Class GdkDeviceWin32Class;

struct _GdkDeviceWin32
{
  GdkDevice parent_instance;
};

struct _GdkDeviceWin32Class
{
  GdkDeviceClass parent_class;
};

GType gdk_device_win32_get_type (void) G_GNUC_CONST;

GdkSurface *_gdk_device_win32_surface_at_position (GdkDevice       *device,
                                                   double          *win_x,
                                                   double          *win_y,
                                                   GdkModifierType *mask);
void _gdk_device_win32_query_state             (GdkDevice        *device,
                                                GdkSurface        *surface,
                                                GdkSurface       **child_surface,
                                                double           *win_x,
                                                double           *win_y,
                                                GdkModifierType  *mask);


G_END_DECLS

