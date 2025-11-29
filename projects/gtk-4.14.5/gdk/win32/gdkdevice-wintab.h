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

#include <windows.h>
#include <wintab.h>

G_BEGIN_DECLS

#define GDK_TYPE_DEVICE_WINTAB         (gdk_device_wintab_get_type ())
#define GDK_DEVICE_WINTAB(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), GDK_TYPE_DEVICE_WINTAB, GdkDeviceWintab))
#define GDK_DEVICE_WINTAB_CLASS(c)     (G_TYPE_CHECK_CLASS_CAST ((c), GDK_TYPE_DEVICE_WINTAB, GdkDeviceWintabClass))
#define GDK_IS_DEVICE_WINTAB(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), GDK_TYPE_DEVICE_WINTAB))
#define GDK_IS_DEVICE_WINTAB_CLASS(c)  (G_TYPE_CHECK_CLASS_TYPE ((c), GDK_TYPE_DEVICE_WINTAB))
#define GDK_DEVICE_WINTAB_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), GDK_TYPE_DEVICE_WINTAB, GdkDeviceWintabClass))

typedef struct _GdkDeviceWintab GdkDeviceWintab;
typedef struct _GdkDeviceWintabClass GdkDeviceWintabClass;

struct _GdkDeviceWintab
{
  GdkDevice parent_instance;

  gboolean sends_core;
  int *last_axis_data;
  int button_state;

  /* WINTAB stuff: */
  HCTX hctx;
  /* Cursor number */
  UINT cursor;
  /* The cursor's CSR_PKTDATA */
  WTPKT pktdata;
  /* Azimuth and altitude axis */
  AXIS orientation_axes[2];
};

struct _GdkDeviceWintabClass
{
  GdkDeviceClass parent_class;
};

GType gdk_device_wintab_get_type (void) G_GNUC_CONST;

void         _gdk_device_wintab_translate_axes (GdkDeviceWintab *device,
                                                GdkSurface       *window,
                                                double          *axes,
                                                double          *x,
                                                double          *y);

void
gdk_device_wintab_query_state (GdkDevice        *device,
                               GdkSurface       *window,
                               GdkSurface      **child_window,
                               double           *win_x,
                               double           *win_y,
                               GdkModifierType  *mask);

G_END_DECLS

