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

#pragma once

#include <windows.h>
#include <glib.h>
#include <gio/gio.h>

#include "gdkmonitorprivate.h"

#include "gdkwin32monitor.h"

struct _GdkWin32Monitor
{
  GdkMonitor parent;

  /* work area */
  GdkRectangle work_rect;

  /* Device instance path (used to match GdkWin32Monitor to monitor device) */
  char *instance_path;

  /* MOnitor handle (used to fullscreen windows on monitors) */
  HMONITOR hmonitor;

  /* TRUE if monitor is made up by us
   * (this happens when system has logical monitors, but no physical ones).
   */
  guint madeup : 1;

  /* TRUE if we should notify GDK about this monitor being added */
  guint add    : 1;

  /* TRUE if we should notify GDK about this monitor being removed */
  guint remove : 1;
};

struct _GdkWin32MonitorClass {
  GdkMonitorClass parent_class;
};

int        _gdk_win32_monitor_compare  (GdkWin32Monitor *a, GdkWin32Monitor *b);

