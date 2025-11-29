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

#include "config.h"

#include <gdk/gdksurface.h>

#include <windowsx.h>
#include <objbase.h>

#include "gdkdisplayprivate.h"
#include "gdkdevice-virtual.h"
#include "gdkdevice-win32.h"
#include "gdkwin32.h"
#include "gdkdisplay-win32.h"

G_DEFINE_TYPE (GdkDeviceVirtual, gdk_device_virtual, GDK_TYPE_DEVICE)

void
_gdk_device_virtual_set_active (GdkDevice *device,
                                GdkDevice *new_active)
{
  GdkDeviceVirtual *virtual = GDK_DEVICE_VIRTUAL (device);
  int n_axes, i;
  GdkAxisUse use;
  double min_value, max_value, resolution;

  if (virtual->active_device == new_active)
    return;

  virtual->active_device = new_active;

  if (gdk_device_get_source (device) != GDK_SOURCE_KEYBOARD)
    {
      _gdk_device_reset_axes (device);
      n_axes = gdk_device_get_n_axes (new_active);
      for (i = 0; i < n_axes; i++)
        {
          _gdk_device_get_axis_info (new_active, i, &use,
                                     &min_value, &max_value, &resolution);
          _gdk_device_add_axis (device, use, min_value, max_value, resolution);
        }
    }

  g_signal_emit_by_name (G_OBJECT (device), "changed");
}

static void
gdk_device_virtual_set_surface_cursor (GdkDevice  *device,
                                       GdkSurface *window,
                                       GdkCursor  *cursor)
{
  GdkDisplay *display = gdk_surface_get_display (window);
  GdkWin32HCursor *win32_hcursor = NULL;

  if (cursor == NULL)
    cursor = gdk_cursor_new_from_name ("default", NULL);

  if (display != NULL)
    win32_hcursor = gdk_win32_display_get_win32hcursor (GDK_WIN32_DISPLAY (display), cursor);

  /* This is correct because the code up the stack already
   * checked that cursor is currently inside this window,
   * and wouldn't have called this function otherwise.
   */
  if (win32_hcursor != NULL)
    SetCursor (gdk_win32_hcursor_get_handle (win32_hcursor));

  g_set_object (&GDK_WIN32_SURFACE (window)->cursor, win32_hcursor);
}

void
gdk_device_virtual_query_state (GdkDevice        *device,
				GdkSurface        *window,
				GdkSurface       **child_window,
				double           *win_x,
				double           *win_y,
				GdkModifierType  *mask)
{
  GdkDeviceVirtual *virtual = GDK_DEVICE_VIRTUAL (device);

  _gdk_device_win32_query_state (virtual->active_device,
                                 window, child_window,
                                 win_x, win_y,
                                 mask);
}

static GdkGrabStatus
gdk_device_virtual_grab (GdkDevice    *device,
			 GdkSurface    *window,
			 gboolean      owner_events,
			 GdkEventMask  event_mask,
			 GdkSurface    *confine_to,
			 GdkCursor    *cursor,
			 guint32       time_)
{
  if (gdk_device_get_source (device) != GDK_SOURCE_KEYBOARD)
    {
      GdkWin32HCursor *win32_hcursor;
      GdkWin32Display *display = GDK_WIN32_DISPLAY (gdk_device_get_display (device));
      win32_hcursor = NULL;

      if (cursor != NULL)
        win32_hcursor = gdk_win32_display_get_win32hcursor (display, cursor);

      g_set_object (&display->grab_cursor, win32_hcursor);

      if (display->grab_cursor != NULL)
        SetCursor (gdk_win32_hcursor_get_handle (display->grab_cursor));
      else
        SetCursor (LoadCursor (NULL, IDC_ARROW));

      SetCapture (GDK_SURFACE_HWND (window));
    }

  return GDK_GRAB_SUCCESS;
}

static void
gdk_device_virtual_ungrab (GdkDevice *device,
                           guint32    time_)
{
  GdkDeviceGrabInfo *info;
  GdkDisplay *display;
  GdkWin32Display *win32_display;

  display = gdk_device_get_display (device);
  win32_display = GDK_WIN32_DISPLAY (display);
  info = _gdk_display_get_last_device_grab (display, device);

  if (info)
    info->serial_end = 0;

  if (gdk_device_get_source (device) != GDK_SOURCE_KEYBOARD)
    {
      g_clear_object (&win32_display->grab_cursor);
      ReleaseCapture ();
    }

  _gdk_display_device_grab_update (display, device, 0);
}

static void
gdk_device_virtual_class_init (GdkDeviceVirtualClass *klass)
{
  GdkDeviceClass *device_class = GDK_DEVICE_CLASS (klass);

  device_class->set_surface_cursor = gdk_device_virtual_set_surface_cursor;
  device_class->grab = gdk_device_virtual_grab;
  device_class->ungrab = gdk_device_virtual_ungrab;
  device_class->surface_at_position = _gdk_device_win32_surface_at_position;
}

static void
gdk_device_virtual_init (GdkDeviceVirtual *device_virtual)
{
}
