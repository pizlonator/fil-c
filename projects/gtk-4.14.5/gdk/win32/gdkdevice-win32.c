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
#include <math.h>

#include "gdkdevice-win32.h"
#include "gdkwin32.h"
#include "gdkdisplay-win32.h"
#include "gdkdevice-virtual.h"
#include "gdkdevice-wintab.h"

G_DEFINE_TYPE (GdkDeviceWin32, gdk_device_win32, GDK_TYPE_DEVICE)

static void
gdk_device_win32_set_surface_cursor (GdkDevice *device,
                                    GdkSurface *window,
                                    GdkCursor *cursor)
{
}

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
gdk_device_win32_query_state (GdkDevice        *device,
                              GdkSurface       *window,
                              GdkSurface      **child_window,
                              double           *win_x,
                              double           *win_y,
                              GdkModifierType  *mask)
{
  POINT point;
  HWND hwnd, hwndc;
  int scale;

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
    *mask = get_current_mask ();
}

void
_gdk_device_win32_query_state (GdkDevice        *device,
                               GdkSurface       *window,
                               GdkSurface      **child_window,
                               double           *win_x,
                               double           *win_y,
                               GdkModifierType  *mask)
{
  if (GDK_IS_DEVICE_VIRTUAL (device))
    gdk_device_virtual_query_state (device, window, child_window, win_x, win_y, mask);
  else if (GDK_IS_DEVICE_WINTAB (device))
    gdk_device_wintab_query_state (device, window, child_window, win_x, win_y, mask);
  else
    gdk_device_win32_query_state (device, window, child_window, win_x, win_y, mask);
}

static GdkGrabStatus
gdk_device_win32_grab (GdkDevice    *device,
                       GdkSurface    *window,
                       gboolean      owner_events,
                       GdkEventMask  event_mask,
                       GdkSurface    *confine_to,
                       GdkCursor    *cursor,
                       guint32       time_)
{
  /* No support for grabbing physical devices atm */
  return GDK_GRAB_NOT_VIEWABLE;
}

static void
gdk_device_win32_ungrab (GdkDevice *device,
                         guint32    time_)
{
}

static void
screen_to_client (HWND hwnd, POINT screen_pt, POINT *client_pt)
{
  *client_pt = screen_pt;
  ScreenToClient (hwnd, client_pt);
}

GdkSurface *
_gdk_device_win32_surface_at_position (GdkDevice       *device,
                                       double          *win_x,
                                       double          *win_y,
                                       GdkModifierType *mask)
{
  GdkSurface *window = NULL;
  GdkWin32Surface *impl = NULL;
  POINT screen_pt, client_pt;
  HWND hwnd;
  RECT rect;

  if (!_gdk_win32_get_cursor_pos (&screen_pt))
    return NULL;

  /* Use WindowFromPoint instead of ChildWindowFromPoint(Ex).
  *  Only WindowFromPoint is able to look through transparent
  *  layered windows.
  */
  hwnd = GetAncestor (WindowFromPoint (screen_pt), GA_ROOT);

  /* Verify that we're really inside the client area of the window */
  GetClientRect (hwnd, &rect);
  screen_to_client (hwnd, screen_pt, &client_pt);
  if (!PtInRect (&rect, client_pt))
    hwnd = NULL;

  window = gdk_win32_handle_table_lookup_ (hwnd);

  if (window && (win_x || win_y))
    {
      impl = GDK_WIN32_SURFACE (window);

      if (win_x)
        *win_x = client_pt.x / impl->surface_scale;
      if (win_y)
        *win_y = client_pt.y / impl->surface_scale;
    }

  return window;
}

static void
gdk_device_win32_class_init (GdkDeviceWin32Class *klass)
{
  GdkDeviceClass *device_class = GDK_DEVICE_CLASS (klass);

  device_class->set_surface_cursor = gdk_device_win32_set_surface_cursor;
  device_class->grab = gdk_device_win32_grab;
  device_class->ungrab = gdk_device_win32_ungrab;
  device_class->surface_at_position = _gdk_device_win32_surface_at_position;
}

static void
gdk_device_win32_init (GdkDeviceWin32 *device_win32)
{
  GdkDevice *device;

  device = GDK_DEVICE (device_win32);

  _gdk_device_add_axis (device, GDK_AXIS_X, 0, 0, 1);
  _gdk_device_add_axis (device, GDK_AXIS_Y, 0, 0, 1);
}
