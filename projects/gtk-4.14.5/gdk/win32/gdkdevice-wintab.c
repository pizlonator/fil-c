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

#include "gdkwin32.h"
#include "gdkdevice-wintab.h"
#include "gdkdisplay-win32.h"

G_DEFINE_TYPE (GdkDeviceWintab, gdk_device_wintab, GDK_TYPE_DEVICE)

static GdkModifierType
get_current_mask (void)
{
  GdkModifierType mask;
  BYTE kbd[256];

  GetKeyboardState (kbd);
  mask = 0;
  if (kbd[VK_SHIFT] & 0x80)
    mask |= GDK_SHIFT_MASK;
  if (kbd[VK_CAPITAL] & 0x80)
    mask |= GDK_LOCK_MASK;
  if (kbd[VK_CONTROL] & 0x80)
    mask |= GDK_CONTROL_MASK;
  if (kbd[VK_MENU] & 0x80)
    mask |= GDK_ALT_MASK;
  if (kbd[VK_LBUTTON] & 0x80)
    mask |= GDK_BUTTON1_MASK;
  if (kbd[VK_MBUTTON] & 0x80)
    mask |= GDK_BUTTON2_MASK;
  if (kbd[VK_RBUTTON] & 0x80)
    mask |= GDK_BUTTON3_MASK;

  return mask;
}

static void
gdk_device_wintab_set_surface_cursor (GdkDevice *device,
                                     GdkSurface *window,
                                     GdkCursor *cursor)
{
}

void
gdk_device_wintab_query_state (GdkDevice        *device,
                               GdkSurface       *window,
                               GdkSurface      **child_window,
                               double           *win_x,
                               double           *win_y,
                               GdkModifierType  *mask)
{
  GdkDeviceWintab *device_wintab;
  POINT point;
  HWND hwnd, hwndc;
  int scale;

  device_wintab = GDK_DEVICE_WINTAB (device);
  if (window)
    {
      scale = GDK_WIN32_SURFACE (window)->surface_scale;
      hwnd = GDK_SURFACE_HWND (window);
    }
  else
    {
      GdkDisplay *display = gdk_device_get_display (device);

      scale = GDK_WIN32_DISPLAY (display)->surface_scale;
      hwnd = NULL;
    }

  _gdk_win32_get_cursor_pos (&point);

  if (hwnd)
    ScreenToClient (hwnd, &point);

  if (win_x)
    *win_x = point.x / scale;

  if (win_y)
    *win_y = point.y / scale;

  if (hwnd && child_window)
    {
      hwndc = ChildWindowFromPoint (hwnd, point);

      if (hwndc && hwndc != hwnd)
        *child_window = gdk_win32_handle_table_lookup_ (hwndc);
      else
        *child_window = NULL; /* Direct child unknown to gdk */
    }

  if (mask)
    {
      *mask = get_current_mask ();
      *mask &= 0xFF; /* Mask away core pointer buttons */
      *mask |= ((device_wintab->button_state << 8)
                & (GDK_BUTTON1_MASK | GDK_BUTTON2_MASK
                   | GDK_BUTTON3_MASK | GDK_BUTTON4_MASK
                   | GDK_BUTTON5_MASK));

    }
}

static GdkGrabStatus
gdk_device_wintab_grab (GdkDevice    *device,
                        GdkSurface    *window,
                        gboolean      owner_events,
                        GdkEventMask  event_mask,
                        GdkSurface    *confine_to,
                        GdkCursor    *cursor,
                        guint32       time_)
{
  return GDK_GRAB_SUCCESS;
}

static void
gdk_device_wintab_ungrab (GdkDevice *device,
                          guint32    time_)
{
}

static GdkSurface *
gdk_device_wintab_surface_at_position (GdkDevice       *device,
                                       double          *win_x,
                                       double          *win_y,
                                       GdkModifierType *mask)
{
  return NULL;
}

void
_gdk_device_wintab_translate_axes (GdkDeviceWintab *device_wintab,
                                   GdkSurface       *window,
                                   double          *axes,
                                   double          *x,
                                   double          *y)
{
  GdkDevice *device;
  GdkSurface *impl_surface;
  int root_x, root_y;
  double temp_x, temp_y;
  int i;

  device = GDK_DEVICE (device_wintab);
  impl_surface = window;
  temp_x = temp_y = 0;

  gdk_surface_get_origin (impl_surface, &root_x, &root_y);

  for (i = 0; i < gdk_device_get_n_axes (device); i++)
    {
      GdkAxisUse use;

      use = gdk_device_get_axis_use (device, i);

      switch (use)
        {
        case GDK_AXIS_X:
        case GDK_AXIS_Y:
            {
              HMONITOR hmonitor;
              MONITORINFO minfo = {sizeof (MONITORINFO),};

              hmonitor = MonitorFromWindow (GDK_SURFACE_HWND (window),
                                            MONITOR_DEFAULTTONEAREST);
              GetMonitorInfo (hmonitor, &minfo);

              /* XXX: the dimensions from minfo may need to be scaled for HiDPI usage */
              _gdk_device_translate_screen_coord (device, window,
                                                  root_x, root_y,
                                                  minfo.rcWork.right - minfo.rcWork.left,
                                                  minfo.rcWork.bottom - minfo.rcWork.top,
                                                  i,
                                                  device_wintab->last_axis_data[i],
                                                  &axes[use]);
            }
          if (use == GDK_AXIS_X)
            temp_x = axes[use];
          else if (use == GDK_AXIS_Y)
            temp_y = axes[use];

          break;
        default:
          _gdk_device_translate_axis (device, i,
                                      device_wintab->last_axis_data[i],
                                      &axes[use]);
          break;
        }
    }

  if (x)
    *x = temp_x;

  if (y)
    *y = temp_y;
}

static void
gdk_device_wintab_class_init (GdkDeviceWintabClass *klass)
{
  GdkDeviceClass *device_class = GDK_DEVICE_CLASS (klass);

  device_class->set_surface_cursor = gdk_device_wintab_set_surface_cursor;
  device_class->grab = gdk_device_wintab_grab;
  device_class->ungrab = gdk_device_wintab_ungrab;
  device_class->surface_at_position = gdk_device_wintab_surface_at_position;
}

static void
gdk_device_wintab_init (GdkDeviceWintab *device_wintab)
{
}
