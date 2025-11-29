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
#include "gdkprivate-wayland.h"

#include "gdkmonitor-wayland.h"

/**
 * GdkWaylandMonitor:
 *
 * The Wayland implementation of `GdkMonitor`.
 *
 * Beyond the [class@Gdk.Monitor] API, the Wayland implementation
 * offers access to the Wayland `wl_output` object with
 * [method@GdkWayland.WaylandMonitor.get_wl_output].
 */

G_DEFINE_TYPE (GdkWaylandMonitor, gdk_wayland_monitor, GDK_TYPE_MONITOR)

static void
gdk_wayland_monitor_init (GdkWaylandMonitor *monitor)
{
}

static void
gdk_wayland_monitor_finalize (GObject *object)
{
  GdkWaylandMonitor *monitor = (GdkWaylandMonitor *)object;

  g_free (monitor->name);
  g_free (monitor->description);

  g_clear_pointer (&monitor->xdg_output, zxdg_output_v1_destroy);

  if (wl_output_get_version (monitor->output) >= WL_OUTPUT_RELEASE_SINCE_VERSION)
    g_clear_pointer (&monitor->output, wl_output_release);
  else
    g_clear_pointer (&monitor->output, wl_output_destroy);

  G_OBJECT_CLASS (gdk_wayland_monitor_parent_class)->finalize (object);
}

static void
gdk_wayland_monitor_class_init (GdkWaylandMonitorClass *class)
{
  G_OBJECT_CLASS (class)->finalize = gdk_wayland_monitor_finalize;
}

/**
 * gdk_wayland_monitor_get_wl_output: (skip)
 * @monitor: (type GdkWaylandMonitor): a `GdkMonitor`
 *
 * Returns the Wayland `wl_output` of a `GdkMonitor`.
 *
 * Returns: (transfer none): a Wayland `wl_output`
 */
struct wl_output *
gdk_wayland_monitor_get_wl_output (GdkMonitor *monitor)
{
  g_return_val_if_fail (GDK_IS_WAYLAND_MONITOR (monitor), NULL);

  return GDK_WAYLAND_MONITOR (monitor)->output;
}
