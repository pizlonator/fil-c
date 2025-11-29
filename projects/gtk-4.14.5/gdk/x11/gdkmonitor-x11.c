/*
 * Copyright © 2016 Red Hat, Inc
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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include <glib.h>
#include <gio/gio.h>

#include "gdkmonitor-x11.h"
#include "gdkx11display.h"
#include "gdkdisplay-x11.h"
#include "gdkscreen-x11.h"
#include "gdkdisplayprivate.h"
#include "gdkprivate-x11.h"


G_DEFINE_TYPE (GdkX11Monitor, gdk_x11_monitor, GDK_TYPE_MONITOR)

static gboolean
gdk_monitor_has_fullscreen_window (GdkMonitor *monitor)
{
  GList *toplevels, *l;
  GdkSurface *surface;
  gboolean has_fullscreen;

  toplevels = gdk_x11_display_get_toplevel_windows (monitor->display);

  has_fullscreen = FALSE;
  for (l = toplevels; l; l = l->next)
    {
      surface = l->data;

      if (!GDK_IS_TOPLEVEL (surface))
        continue;

      if ((gdk_toplevel_get_state (GDK_TOPLEVEL (surface)) & GDK_TOPLEVEL_STATE_FULLSCREEN) == 0)
        continue;

      if (surface->fullscreen_mode == GDK_FULLSCREEN_ON_ALL_MONITORS ||
          gdk_display_get_monitor_at_surface (monitor->display, surface) == monitor)
        {
          has_fullscreen = TRUE;
          break;
        }
    }

  return has_fullscreen;
}

/**
 * gdk_x11_monitor_get_workarea:
 * @monitor: (type GdkX11Monitor): a `GdkMonitor`
 * @workarea: (out): a `GdkRectangle` to be filled with the monitor workarea
 *
 * Retrieves the size and position of the “work area” on a monitor
 * within the display coordinate space.
 *
 * The returned geometry is in ”application pixels”, not in ”device pixels”
 * (see [method@Gdk.Monitor.get_scale_factor]).
 */
void
gdk_x11_monitor_get_workarea (GdkMonitor   *monitor,
                              GdkRectangle *dest)
{
  GdkX11Screen *screen = GDK_X11_DISPLAY (monitor->display)->screen;
  GdkRectangle workarea;

  gdk_monitor_get_geometry (monitor, dest);

  if (_gdk_x11_screen_get_monitor_work_area (screen, monitor, &workarea))
    {
      if (!gdk_monitor_has_fullscreen_window (monitor))
        *dest = workarea;
    }
  else
    {
      /* The EWMH constrains workarea to be a rectangle, so it
       * can't adequately deal with L-shaped monitor arrangements.
       * As a workaround, we ignore the workarea for anything
       * but the primary monitor. Since that is where the 'desktop
       * chrome' usually lives, this works ok in practice.
       */
      if (monitor == gdk_x11_display_get_primary_monitor (monitor->display) &&
          !gdk_monitor_has_fullscreen_window (monitor))
        {
          gdk_x11_screen_get_work_area (screen, &workarea);
          if (gdk_rectangle_intersect (dest, &workarea, &workarea))
            *dest = workarea;
        }
    }
}

static void
gdk_x11_monitor_init (GdkX11Monitor *monitor)
{
}

static void
gdk_x11_monitor_class_init (GdkX11MonitorClass *class)
{
}

/**
 * gdk_x11_monitor_get_output:
 * @monitor: (type GdkX11Monitor): a `GdkMonitor`
 *
 * Returns the XID of the Output corresponding to @monitor.
 *
 * Returns: the XID of @monitor
 */
XID
gdk_x11_monitor_get_output (GdkMonitor *monitor)
{
  g_return_val_if_fail (GDK_IS_X11_MONITOR (monitor), 0);

  return GDK_X11_MONITOR (monitor)->output;
}

